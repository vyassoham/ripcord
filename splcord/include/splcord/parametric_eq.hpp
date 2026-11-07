#pragma once
#include "biquad.hpp"
#include <array>

namespace splcord {

struct EQBandConfig {
    FilterType type;
    float frequency; // Hz
    float gain_db;   // dB (-24 to +24)
    float q;         // Quality factor
    bool enabled;
};

class ParametricEQ {
public:
    static constexpr size_t NUM_BANDS = 7;

    ParametricEQ() noexcept : m_master_bypass(false) {
        init_default_bands();
    }

    void init_default_bands() noexcept {
        // Band 0: High-Pass Filter (removes rumble < 80Hz)
        m_bands[0].set_highpass(80.0f, 0.7071f);
        m_config[0] = { FilterType::HighPass, 80.0f, 0.0f, 0.7071f, true };

        // Band 1: Low Shelf (warmth control at 180Hz)
        m_bands[1].set_lowshelf(180.0f, 1.5f, 0.7071f);
        m_config[1] = { FilterType::LowShelf, 180.0f, 1.5f, 0.7071f, true };

        // Band 2: Peaking 1 (mud/boxiness scoop at 400Hz)
        m_bands[2].set_peaking(400.0f, -2.5f, 1.4f);
        m_config[2] = { FilterType::Peaking, 400.0f, -2.5f, 1.4f, true };

        // Band 3: Peaking 2 (vocal body at 1200Hz)
        m_bands[3].set_peaking(1200.0f, 1.0f, 1.2f);
        m_config[3] = { FilterType::Peaking, 1200.0f, 1.0f, 1.2f, true };

        // Band 4: Peaking 3 (speech intelligibility peak at 3200Hz)
        m_bands[4].set_peaking(3200.0f, 4.0f, 1.5f);
        m_config[4] = { FilterType::Peaking, 3200.0f, 4.0f, 1.5f, true };

        // Band 5: High Shelf (broadcast air at 10kHz)
        m_bands[5].set_highshelf(10000.0f, 2.5f, 0.7071f);
        m_config[5] = { FilterType::HighShelf, 10000.0f, 2.5f, 0.7071f, true };

        // Band 6: Low-Pass Filter (high hash cutoff at 18kHz)
        m_bands[6].set_lowpass(18000.0f, 0.7071f);
        m_config[6] = { FilterType::LowPass, 18000.0f, 0.0f, 0.7071f, true };
    }

    void set_band(size_t index, FilterType type, float freq, float gain_db, float q, bool enabled = true) noexcept {
        if (index >= NUM_BANDS) return;
        m_config[index] = { type, freq, gain_db, q, enabled };

        if (!enabled || type == FilterType::Bypass) {
            m_bands[index].set_bypass();
            return;
        }

        switch (type) {
            case FilterType::HighPass:
                m_bands[index].set_highpass(freq, q);
                break;
            case FilterType::LowPass:
                m_bands[index].set_lowpass(freq, q);
                break;
            case FilterType::LowShelf:
                m_bands[index].set_lowshelf(freq, gain_db, q);
                break;
            case FilterType::HighShelf:
                m_bands[index].set_highshelf(freq, gain_db, q);
                break;
            case FilterType::Peaking:
                m_bands[index].set_peaking(freq, gain_db, q);
                break;
            default:
                m_bands[index].set_bypass();
                break;
        }
    }

    const EQBandConfig& get_band_config(size_t index) const noexcept {
        return m_config[index];
    }

    void set_master_bypass(bool bypass) noexcept {
        m_master_bypass = bypass;
    }

    bool is_master_bypassed() const noexcept {
        return m_master_bypass;
    }

    void reset() noexcept {
        for (auto &band : m_bands) {
            band.reset();
        }
    }

    // Process mono buffer in-place
    void process(float *samples, size_t num_samples) noexcept {
        if (m_master_bypass) return;
        for (size_t b = 0; b < NUM_BANDS; ++b) {
            if (!m_config[b].enabled || m_config[b].type == FilterType::Bypass) continue;
            auto &filter = m_bands[b];
            for (size_t i = 0; i < num_samples; ++i) {
                float dummy = samples[i];
                filter.process_stereo_sample(samples[i], dummy);
            }
        }
    }

    // Process stereo interleaved buffer in-place
    void process_stereo(float *samples, size_t num_frames) noexcept {
        if (m_master_bypass) return;
        for (size_t b = 0; b < NUM_BANDS; ++b) {
            if (!m_config[b].enabled || m_config[b].type == FilterType::Bypass) continue;
            auto &filter = m_bands[b];
            for (size_t i = 0; i < num_frames; ++i) {
                filter.process_stereo_sample(samples[2 * i], samples[2 * i + 1]);
            }
        }
    }

    // Get composite frequency response in dB at frequency f
    float get_composite_response_db(float f, float fs = SAMPLE_RATE_48K) const noexcept {
        if (m_master_bypass) return 0.0f;
        float total_mag = 1.0f;
        for (size_t b = 0; b < NUM_BANDS; ++b) {
            if (!m_config[b].enabled || m_config[b].type == FilterType::Bypass) continue;
            total_mag *= m_bands[b].left().get_magnitude_response(f, fs);
        }
        return linear_to_db(total_mag);
    }

private:
    std::array<StereoBiquad, NUM_BANDS> m_bands;
    std::array<EQBandConfig, NUM_BANDS> m_config;
    bool m_master_bypass;
};

} // namespace splcord
