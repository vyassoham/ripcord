#pragma once
#include "dsp_types.hpp"

namespace splcord {

class NoiseGate {
public:
    NoiseGate() noexcept {
        set_threshold_db(-48.0f);
        set_ratio(4.0f);
        set_attack_ms(2.0f);
        set_hold_ms(50.0f);
        set_release_ms(100.0f);
        set_hysteresis_db(3.0f);
        reset();
    }

    void set_threshold_db(float db) noexcept {
        m_threshold_db = db;
        m_open_thresh_lin = db_to_linear(db);
        m_close_thresh_lin = db_to_linear(db - m_hysteresis_db);
    }

    float get_threshold_db() const noexcept { return m_threshold_db; }

    void set_ratio(float ratio) noexcept { m_ratio = std::max(1.0f, ratio); }
    float get_ratio() const noexcept { return m_ratio; }

    void set_attack_ms(float ms, float fs = SAMPLE_RATE_48K) noexcept {
        m_attack_ms = ms;
        m_attack_coeff = std::exp(-1.0f / (std::max(0.1f, ms) * 0.001f * fs));
    }
    float get_attack_ms() const noexcept { return m_attack_ms; }

    void set_hold_ms(float ms, float fs = SAMPLE_RATE_48K) noexcept {
        m_hold_ms = ms;
        m_hold_samples = static_cast<uint32_t>(ms * 0.001f * fs);
    }
    float get_hold_ms() const noexcept { return m_hold_ms; }

    void set_release_ms(float ms, float fs = SAMPLE_RATE_48K) noexcept {
        m_release_ms = ms;
        m_release_coeff = std::exp(-1.0f / (std::max(1.0f, ms) * 0.001f * fs));
    }
    float get_release_ms() const noexcept { return m_release_ms; }

    void set_hysteresis_db(float db) noexcept {
        m_hysteresis_db = std::max(0.0f, db);
        m_close_thresh_lin = db_to_linear(m_threshold_db - m_hysteresis_db);
    }

    void set_bypass(bool bypass) noexcept { m_bypass = bypass; }
    bool is_bypassed() const noexcept { return m_bypass; }

    void reset() noexcept {
        m_envelope = 0.0f;
        m_gain = 1.0f;
        m_hold_counter = 0;
        m_is_open = false;
        m_gain_reduction_db = 0.0f;
    }

    float get_gain_reduction_db() const noexcept { return m_gain_reduction_db; }
    bool is_open() const noexcept { return m_is_open; }

    void process(float *samples, size_t num_samples) noexcept {
        if (m_bypass) {
            m_gain_reduction_db = 0.0f;
            return;
        }

        float min_gain = 1.0f;
        for (size_t i = 0; i < num_samples; ++i) {
            float in_abs = std::abs(samples[i]);
            
            // Envelope detection (fast peak detector)
            if (in_abs > m_envelope) {
                m_envelope = in_abs;
            } else {
                m_envelope += 0.002f * (in_abs - m_envelope);
            }

            // Hysteresis gate logic
            if (m_envelope >= m_open_thresh_lin) {
                m_is_open = true;
                m_hold_counter = m_hold_samples;
            } else if (m_envelope < m_close_thresh_lin) {
                if (m_hold_counter > 0) {
                    --m_hold_counter;
                } else {
                    m_is_open = false;
                }
            }

            // Target gain calculation
            float target_gain = 1.0f;
            if (!m_is_open) {
                float env_db = linear_to_db(m_envelope);
                float diff_db = env_db - m_threshold_db; // negative
                float atten_db = diff_db * (m_ratio - 1.0f);
                target_gain = db_to_linear(std::max(-60.0f, atten_db));
            }

            // Ballistics smoothing
            if (target_gain > m_gain) {
                m_gain = m_attack_coeff * m_gain + (1.0f - m_attack_coeff) * target_gain;
            } else {
                m_gain = m_release_coeff * m_gain + (1.0f - m_release_coeff) * target_gain;
            }

            samples[i] *= m_gain;
            if (m_gain < min_gain) min_gain = m_gain;
        }

        m_gain_reduction_db = linear_to_db(min_gain);
    }

    void process_stereo(float *samples, size_t num_frames) noexcept {
        if (m_bypass) {
            m_gain_reduction_db = 0.0f;
            return;
        }

        float min_gain = 1.0f;
        for (size_t i = 0; i < num_frames; ++i) {
            float in_abs = std::max(std::abs(samples[2 * i]), std::abs(samples[2 * i + 1]));

            if (in_abs > m_envelope) {
                m_envelope = in_abs;
            } else {
                m_envelope += 0.002f * (in_abs - m_envelope);
            }

            if (m_envelope >= m_open_thresh_lin) {
                m_is_open = true;
                m_hold_counter = m_hold_samples;
            } else if (m_envelope < m_close_thresh_lin) {
                if (m_hold_counter > 0) {
                    --m_hold_counter;
                } else {
                    m_is_open = false;
                }
            }

            float target_gain = 1.0f;
            if (!m_is_open) {
                float env_db = linear_to_db(m_envelope);
                float diff_db = env_db - m_threshold_db;
                float atten_db = diff_db * (m_ratio - 1.0f);
                target_gain = db_to_linear(std::max(-60.0f, atten_db));
            }

            if (target_gain > m_gain) {
                m_gain = m_attack_coeff * m_gain + (1.0f - m_attack_coeff) * target_gain;
            } else {
                m_gain = m_release_coeff * m_gain + (1.0f - m_release_coeff) * target_gain;
            }

            samples[2 * i] *= m_gain;
            samples[2 * i + 1] *= m_gain;
            if (m_gain < min_gain) min_gain = m_gain;
        }

        m_gain_reduction_db = linear_to_db(min_gain);
    }

private:
    float m_threshold_db = -48.0f;
    float m_open_thresh_lin = 0.0f;
    float m_close_thresh_lin = 0.0f;
    float m_ratio = 4.0f;
    float m_attack_ms = 2.0f;
    float m_hold_ms = 50.0f;
    float m_release_ms = 100.0f;
    float m_hysteresis_db = 3.0f;
    float m_attack_coeff = 0.0f;
    float m_release_coeff = 0.0f;
    uint32_t m_hold_samples = 0;
    uint32_t m_hold_counter = 0;

    float m_envelope = 0.0f;
    float m_gain = 1.0f;
    bool m_is_open = false;
    bool m_bypass = false;
    float m_gain_reduction_db = 0.0f;
};

} // namespace splcord
