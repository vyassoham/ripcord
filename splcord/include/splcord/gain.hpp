#pragma once
#include "dsp_types.hpp"

namespace splcord {

class GainStage {
public:
    GainStage() noexcept : m_gain_db(0.0f), m_target_lin(1.0f), m_current_lin(1.0f), m_bypass(false) {}

    void set_gain_db(float db) noexcept {
        m_gain_db = db;
        m_target_lin = db_to_linear(db);
    }

    float get_gain_db() const noexcept {
        return m_gain_db;
    }

    void set_gain_linear(float lin) noexcept {
        m_target_lin = std::max(0.0f, lin);
        m_gain_db = linear_to_db(m_target_lin);
    }

    float get_gain_linear() const noexcept {
        return m_target_lin;
    }

    void set_bypass(bool bypass) noexcept {
        m_bypass = bypass;
    }

    bool is_bypassed() const noexcept {
        return m_bypass;
    }

    // Process mono buffer in-place with slew smoothing
    void process(float *samples, size_t num_samples) noexcept {
        if (m_bypass) return;

        // Smooth parameter transitions over 128 samples or per sample
        constexpr float slew_factor = 0.005f;
        for (size_t i = 0; i < num_samples; ++i) {
            m_current_lin += slew_factor * (m_target_lin - m_current_lin);
            samples[i] *= m_current_lin;
        }
    }

    // Process stereo interleaved buffer in-place
    void process_stereo(float *samples, size_t num_frames) noexcept {
        if (m_bypass) return;

        constexpr float slew_factor = 0.005f;
        for (size_t i = 0; i < num_frames; ++i) {
            m_current_lin += slew_factor * (m_target_lin - m_current_lin);
            samples[2 * i] *= m_current_lin;
            samples[2 * i + 1] *= m_current_lin;
        }
    }

    void reset() noexcept {
        m_current_lin = m_target_lin;
    }

private:
    float m_gain_db;
    float m_target_lin;
    float m_current_lin;
    bool m_bypass;
};

} // namespace splcord
