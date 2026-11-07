#pragma once
#include "dsp_types.hpp"

namespace splcord {

class LoudnessNormalizer {
public:
    LoudnessNormalizer() noexcept {
        set_target_lufs(-14.0f);
        set_max_boost_db(12.0f);
        set_max_cut_db(18.0f);
        set_speed_ms(400.0f);
        reset();
    }

    void set_target_lufs(float lufs) noexcept { m_target_lufs = lufs; }
    float get_target_lufs() const noexcept { return m_target_lufs; }

    void set_max_boost_db(float db) noexcept { m_max_boost_db = std::max(0.0f, db); }
    float get_max_boost_db() const noexcept { return m_max_boost_db; }

    void set_max_cut_db(float db) noexcept { m_max_cut_db = std::max(0.0f, db); }
    float get_max_cut_db() const noexcept { return m_max_cut_db; }

    void set_speed_ms(float ms, float fs = SAMPLE_RATE_48K) noexcept {
        m_speed_ms = ms;
        m_smooth_coeff = std::exp(-1.0f / (std::max(10.0f, ms) * 0.001f * fs));
    }

    void set_bypass(bool bypass) noexcept { m_bypass = bypass; }
    bool is_bypassed() const noexcept { return m_bypass; }

    void reset() noexcept {
        m_short_term_energy = 0.001f;
        m_current_gain = 1.0f;
        m_applied_gain_db = 0.0f;
    }

    float get_applied_gain_db() const noexcept { return m_applied_gain_db; }

    void process(float *samples, size_t num_samples) noexcept {
        if (m_bypass) {
            m_applied_gain_db = 0.0f;
            return;
        }

        for (size_t i = 0; i < num_samples; ++i) {
            float s = samples[i];
            float inst_energy = s * s;
            m_short_term_energy = m_smooth_coeff * m_short_term_energy + (1.0f - m_smooth_coeff) * inst_energy;

            // Approximate RMS / Loudness in LUFS domain
            float est_rms = std::sqrt(std::max(1e-8f, m_short_term_energy));
            float est_lufs = linear_to_db(est_rms) - 3.0f; // Approx conversion to LUFS

            // Only adjust if speech energy is above noise gate threshold (-50 LUFS)
            float target_gain_db = 0.0f;
            if (est_lufs > -50.0f) {
                float error_db = m_target_lufs - est_lufs;
                target_gain_db = clampf(error_db, -m_max_cut_db, m_max_boost_db);
            }

            float target_gain_lin = db_to_linear(target_gain_db);
            m_current_gain += 0.0005f * (target_gain_lin - m_current_gain);

            samples[i] *= m_current_gain;
        }

        m_applied_gain_db = linear_to_db(m_current_gain);
    }

    void process_stereo(float *samples, size_t num_frames) noexcept {
        if (m_bypass) {
            m_applied_gain_db = 0.0f;
            return;
        }

        for (size_t i = 0; i < num_frames; ++i) {
            float l = samples[2 * i];
            float r = samples[2 * i + 1];
            float inst_energy = 0.5f * (l * l + r * r);
            m_short_term_energy = m_smooth_coeff * m_short_term_energy + (1.0f - m_smooth_coeff) * inst_energy;

            float est_rms = std::sqrt(std::max(1e-8f, m_short_term_energy));
            float est_lufs = linear_to_db(est_rms) - 3.0f;

            float target_gain_db = 0.0f;
            if (est_lufs > -50.0f) {
                float error_db = m_target_lufs - est_lufs;
                target_gain_db = clampf(error_db, -m_max_cut_db, m_max_boost_db);
            }

            float target_gain_lin = db_to_linear(target_gain_db);
            m_current_gain += 0.0005f * (target_gain_lin - m_current_gain);

            samples[2 * i] *= m_current_gain;
            samples[2 * i + 1] *= m_current_gain;
        }

        m_applied_gain_db = linear_to_db(m_current_gain);
    }

private:
    float m_target_lufs = -14.0f;
    float m_max_boost_db = 12.0f;
    float m_max_cut_db = 18.0f;
    float m_speed_ms = 400.0f;
    float m_smooth_coeff = 0.0f;

    float m_short_term_energy = 0.001f;
    float m_current_gain = 1.0f;
    float m_applied_gain_db = 0.0f;
    bool m_bypass = false;
};

} // namespace splcord
