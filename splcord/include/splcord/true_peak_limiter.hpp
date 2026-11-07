#pragma once
#include "dsp_types.hpp"
#include <vector>

namespace splcord {

class TruePeakLimiter {
public:
    static constexpr size_t MAX_LOOKAHEAD_SAMPLES = 256;

    TruePeakLimiter() noexcept {
        set_ceiling_db(-0.1f);
        set_release_ms(40.0f);
        set_lookahead_samples(120); // 2.5ms at 48kHz
        reset();
    }

    void set_ceiling_db(float db) noexcept {
        m_ceiling_db = clampf(db, -12.0f, 0.0f);
        m_ceiling_lin = db_to_linear(m_ceiling_db);
    }
    float get_ceiling_db() const noexcept { return m_ceiling_db; }

    void set_release_ms(float ms, float fs = SAMPLE_RATE_48K) noexcept {
        m_release_ms = std::max(5.0f, ms);
        m_release_coeff = std::exp(-1.0f / (m_release_ms * 0.001f * fs));
    }
    float get_release_ms() const noexcept { return m_release_ms; }

    void set_lookahead_samples(size_t samples) noexcept {
        m_lookahead_samples = std::clamp(samples, size_t(16), MAX_LOOKAHEAD_SAMPLES);
    }
    size_t get_lookahead_samples() const noexcept { return m_lookahead_samples; }

    void set_bypass(bool bypass) noexcept { m_bypass = bypass; }
    bool is_bypassed() const noexcept { return m_bypass; }

    void reset() noexcept {
        m_delay_buf_l.assign(MAX_LOOKAHEAD_SAMPLES, 0.0f);
        m_delay_buf_r.assign(MAX_LOOKAHEAD_SAMPLES, 0.0f);
        m_write_idx = 0;
        m_current_gain = 1.0f;
        m_gain_reduction_db = 0.0f;
        m_clipping_count = 0;
    }

    float get_gain_reduction_db() const noexcept { return m_gain_reduction_db; }
    uint64_t get_clipping_count() const noexcept { return m_clipping_count; }

    // Estimate 4x inter-sample true-peak around sample x0 with adjacent samples
    static inline float estimate_true_peak(float x_prev2, float x_prev1, float x0, float x_next1) noexcept {
        // Hermite / Catmull-Rom sub-sample peak interpolation
        // Checks midpoints at t = 0.25, 0.50, 0.75
        float p0 = std::abs(x0);
        float d1 = 0.5f * (x0 - x_prev2);
        float d2 = 0.5f * (x_next1 - x_prev1);
        
        // Midpoint t = 0.5
        float mid = 0.5f * (x_prev1 + x0) + 0.125f * (d1 - d2);
        float p_mid = std::abs(mid);

        // Quarter points
        float q1 = 0.75f * x_prev1 + 0.25f * x0;
        float q2 = 0.25f * x_prev1 + 0.75f * x0;

        return std::max({p0, p_mid, std::abs(q1), std::abs(q2)});
    }

    void process(float *samples, size_t num_samples) noexcept {
        if (m_bypass) {
            m_gain_reduction_db = 0.0f;
            return;
        }

        float max_gr = 0.0f;
        for (size_t i = 0; i < num_samples; ++i) {
            float in_s = samples[i];

            // Store in circular lookahead buffer
            m_delay_buf_l[m_write_idx] = in_s;
            size_t read_idx = (m_write_idx + MAX_LOOKAHEAD_SAMPLES - m_lookahead_samples) % MAX_LOOKAHEAD_SAMPLES;
            float delayed_s = m_delay_buf_l[read_idx];

            // True-peak estimation on incoming sample
            size_t p1 = (m_write_idx + MAX_LOOKAHEAD_SAMPLES - 1) % MAX_LOOKAHEAD_SAMPLES;
            size_t p2 = (m_write_idx + MAX_LOOKAHEAD_SAMPLES - 2) % MAX_LOOKAHEAD_SAMPLES;
            float peak_est = estimate_true_peak(m_delay_buf_l[p2], m_delay_buf_l[p1], in_s, in_s);

            m_write_idx = (m_write_idx + 1) % MAX_LOOKAHEAD_SAMPLES;

            // Target gain calculation
            float target_gain = 1.0f;
            if (peak_est > m_ceiling_lin) {
                target_gain = m_ceiling_lin / peak_est;
            }

            // Lookahead attack / smooth release
            if (target_gain < m_current_gain) {
                m_current_gain = target_gain; // Instant transient clamp
            } else {
                m_current_gain = m_release_coeff * m_current_gain + (1.0f - m_release_coeff) * 1.0f;
            }

            float out_s = delayed_s * m_current_gain;

            // Failsafe transparent soft-knee saturation at ceiling to prevent any digital overflow
            if (std::abs(out_s) > m_ceiling_lin) {
                ++m_clipping_count;
                float sign = (out_s > 0.0f) ? 1.0f : -1.0f;
                float over = std::abs(out_s) - m_ceiling_lin;
                out_s = sign * (m_ceiling_lin + 0.05f * std::tanh(over / 0.05f));
                out_s = clampf(out_s, -m_ceiling_lin, m_ceiling_lin);
            }

            samples[i] = out_s;
            float gr_db = -linear_to_db(m_current_gain);
            if (gr_db > max_gr) max_gr = gr_db;
        }

        m_gain_reduction_db = max_gr;
    }

    void process_stereo(float *samples, size_t num_frames) noexcept {
        if (m_bypass) {
            m_gain_reduction_db = 0.0f;
            return;
        }

        float max_gr = 0.0f;
        for (size_t i = 0; i < num_frames; ++i) {
            float in_l = samples[2 * i];
            float in_r = samples[2 * i + 1];

            m_delay_buf_l[m_write_idx] = in_l;
            m_delay_buf_r[m_write_idx] = in_r;

            size_t read_idx = (m_write_idx + MAX_LOOKAHEAD_SAMPLES - m_lookahead_samples) % MAX_LOOKAHEAD_SAMPLES;
            float delayed_l = m_delay_buf_l[read_idx];
            float delayed_r = m_delay_buf_r[read_idx];

            size_t p1 = (m_write_idx + MAX_LOOKAHEAD_SAMPLES - 1) % MAX_LOOKAHEAD_SAMPLES;
            size_t p2 = (m_write_idx + MAX_LOOKAHEAD_SAMPLES - 2) % MAX_LOOKAHEAD_SAMPLES;

            float peak_l = estimate_true_peak(m_delay_buf_l[p2], m_delay_buf_l[p1], in_l, in_l);
            float peak_r = estimate_true_peak(m_delay_buf_r[p2], m_delay_buf_r[p1], in_r, in_r);
            float peak_est = std::max(peak_l, peak_r);

            m_write_idx = (m_write_idx + 1) % MAX_LOOKAHEAD_SAMPLES;

            float target_gain = 1.0f;
            if (peak_est > m_ceiling_lin) {
                target_gain = m_ceiling_lin / peak_est;
            }

            if (target_gain < m_current_gain) {
                m_current_gain = target_gain;
            } else {
                m_current_gain = m_release_coeff * m_current_gain + (1.0f - m_release_coeff) * 1.0f;
            }

            float out_l = delayed_l * m_current_gain;
            float out_r = delayed_r * m_current_gain;

            // Failsafe clamp to ceiling
            if (std::abs(out_l) > m_ceiling_lin || std::abs(out_r) > m_ceiling_lin) {
                ++m_clipping_count;
                auto limit_sample = [this](float x) -> float {
                    if (std::abs(x) <= m_ceiling_lin) return x;
                    float s = (x > 0.0f) ? 1.0f : -1.0f;
                    float over = std::abs(x) - m_ceiling_lin;
                    float sat = s * (m_ceiling_lin + 0.05f * std::tanh(over / 0.05f));
                    return clampf(sat, -m_ceiling_lin, m_ceiling_lin);
                };
                out_l = limit_sample(out_l);
                out_r = limit_sample(out_r);
            }

            samples[2 * i] = out_l;
            samples[2 * i + 1] = out_r;

            float gr_db = -linear_to_db(m_current_gain);
            if (gr_db > max_gr) max_gr = gr_db;
        }

        m_gain_reduction_db = max_gr;
    }

private:
    float m_ceiling_db = -0.1f;
    float m_ceiling_lin = 0.98855f;
    float m_release_ms = 40.0f;
    float m_release_coeff = 0.0f;
    size_t m_lookahead_samples = 120;

    std::vector<float> m_delay_buf_l;
    std::vector<float> m_delay_buf_r;
    size_t m_write_idx = 0;

    float m_current_gain = 1.0f;
    float m_gain_reduction_db = 0.0f;
    uint64_t m_clipping_count = 0;
    bool m_bypass = false;
};

} // namespace splcord
