#pragma once
#include "dsp_types.hpp"

namespace splcord {

class Compressor {
public:
    Compressor() noexcept {
        set_threshold_db(-18.0f);
        set_ratio(4.0f);
        set_knee_db(4.0f);
        set_attack_ms(5.0f);
        set_release_ms(80.0f);
        set_makeup_gain_db(3.0f);
        reset();
    }

    void set_threshold_db(float db) noexcept { m_threshold_db = db; }
    float get_threshold_db() const noexcept { return m_threshold_db; }

    void set_ratio(float ratio) noexcept { m_ratio = std::max(1.0f, ratio); }
    float get_ratio() const noexcept { return m_ratio; }

    void set_knee_db(float db) noexcept { m_knee_db = std::max(0.0f, db); }
    float get_knee_db() const noexcept { return m_knee_db; }

    void set_attack_ms(float ms, float fs = SAMPLE_RATE_48K) noexcept {
        m_attack_ms = ms;
        m_attack_coeff = std::exp(-1.0f / (std::max(0.05f, ms) * 0.001f * fs));
    }
    float get_attack_ms() const noexcept { return m_attack_ms; }

    void set_release_ms(float ms, float fs = SAMPLE_RATE_48K) noexcept {
        m_release_ms = ms;
        m_release_coeff = std::exp(-1.0f / (std::max(1.0f, ms) * 0.001f * fs));
    }
    float get_release_ms() const noexcept { return m_release_ms; }

    void set_makeup_gain_db(float db) noexcept {
        m_makeup_gain_db = db;
        m_makeup_gain_lin = db_to_linear(db);
    }
    float get_makeup_gain_db() const noexcept { return m_makeup_gain_db; }

    void set_bypass(bool bypass) noexcept { m_bypass = bypass; }
    bool is_bypassed() const noexcept { return m_bypass; }

    void reset() noexcept {
        m_envelope_db = -120.0f;
        m_gain_reduction_db = 0.0f;
    }

    float get_gain_reduction_db() const noexcept { return m_gain_reduction_db; }

    // Static characteristic transfer function in dB
    float compute_gain_reduction(float input_db) const noexcept {
        float T = m_threshold_db;
        float W = m_knee_db;
        float R = m_ratio;

        if (W > 0.0f && input_db > (T - W * 0.5f) && input_db < (T + W * 0.5f)) {
            // Quadratic soft-knee region
            float delta = input_db - T + W * 0.5f;
            float knee_excess = delta * delta / (2.0f * W);
            return (1.0f / R - 1.0f) * knee_excess;
        } else if (input_db >= (T + W * 0.5f)) {
            // Hard compression region
            float excess = input_db - T;
            return (excess / R) - excess; // negative dB gain reduction
        }
        return 0.0f;
    }

    void process(float *samples, size_t num_samples) noexcept {
        if (m_bypass) {
            m_gain_reduction_db = 0.0f;
            return;
        }

        float max_gr = 0.0f;
        for (size_t i = 0; i < num_samples; ++i) {
            float in_abs = std::abs(samples[i]);
            float in_db = linear_to_db(in_abs);

            // Envelope follower in dB domain
            if (in_db > m_envelope_db) {
                m_envelope_db = m_attack_coeff * m_envelope_db + (1.0f - m_attack_coeff) * in_db;
            } else {
                m_envelope_db = m_release_coeff * m_envelope_db + (1.0f - m_release_coeff) * in_db;
            }

            float gr_db = compute_gain_reduction(m_envelope_db);
            float total_gain_lin = db_to_linear(gr_db) * m_makeup_gain_lin;

            samples[i] *= total_gain_lin;
            if (-gr_db > max_gr) max_gr = -gr_db;
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
            float in_abs = std::max(std::abs(samples[2 * i]), std::abs(samples[2 * i + 1]));
            float in_db = linear_to_db(in_abs);

            if (in_db > m_envelope_db) {
                m_envelope_db = m_attack_coeff * m_envelope_db + (1.0f - m_attack_coeff) * in_db;
            } else {
                m_envelope_db = m_release_coeff * m_envelope_db + (1.0f - m_release_coeff) * in_db;
            }

            float gr_db = compute_gain_reduction(m_envelope_db);
            float total_gain_lin = db_to_linear(gr_db) * m_makeup_gain_lin;

            samples[2 * i] *= total_gain_lin;
            samples[2 * i + 1] *= total_gain_lin;
            if (-gr_db > max_gr) max_gr = -gr_db;
        }

        m_gain_reduction_db = max_gr;
    }

private:
    float m_threshold_db = -18.0f;
    float m_ratio = 4.0f;
    float m_knee_db = 4.0f;
    float m_attack_ms = 5.0f;
    float m_release_ms = 80.0f;
    float m_makeup_gain_db = 3.0f;
    float m_makeup_gain_lin = 1.4125f;

    float m_attack_coeff = 0.0f;
    float m_release_coeff = 0.0f;

    float m_envelope_db = -120.0f;
    float m_gain_reduction_db = 0.0f;
    bool m_bypass = false;
};

} // namespace splcord
