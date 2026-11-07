#pragma once

#include "dsp_types.hpp"
#include "biquad.hpp"
#include <cmath>
#include <vector>
#include <algorithm>

namespace splcord {

struct SpatialConfig {
    float pan = 0.0f;               // -1.0 (Full Left) to +1.0 (Full Right), 0.0 = Center
    float stereo_width = 1.0f;      // 0.0 (Mono) to 3.0 (3X Super Wide), 1.0 = Default, 2.0 = Double Wide
    bool elevation_enabled = false; // "Above Head" Pinna HRTF
    float elevation_mix = 0.75f;    // 0.0 to 1.0
    bool in_head_enabled = false;   // "In-Head" Binaural Crossfeed
    float in_head_mix = 0.50f;      // 0.0 to 1.0
    bool auto_pan_enabled = false;  // 8D Orbital Auto-Pan
    float auto_pan_speed = 0.20f;   // Hz (rotations per second)
    bool monitor_enabled = false;   // Mic preview / Hear Yourself
    float monitor_volume = 1.0f;    // 0.0 to 1.5
    bool bypass = false;
};

class Spatializer {
public:
    Spatializer() = default;

    void init(float sample_rate = SAMPLE_RATE_48K) {
        m_sample_rate = sample_rate;
        m_haas_delay_samples = static_cast<size_t>(0.008f * sample_rate); // 8ms Haas delay (~384 samples)
        m_haas_buffer.assign(m_haas_delay_samples + 16, 0.0f);
        m_haas_write_idx = 0;

        // Pinna HRTF Elevation: 7.8 kHz Notch + 11 kHz High Shelf Boost
        m_elevation_notch_l.set_notch(7800.0f, 3.5f, sample_rate);
        m_elevation_notch_r.set_notch(7800.0f, 3.5f, sample_rate);

        m_elevation_air_l.set_highshelf(11000.0f, 4.0f, 0.707f, sample_rate);
        m_elevation_air_r.set_highshelf(11000.0f, 4.0f, 0.707f, sample_rate);

        // Binaural Crossfeed: 2.2 kHz Cranial Low-Pass Filter
        m_crossfeed_lpf_l.set_lowpass(2200.0f, 0.707f, sample_rate);
        m_crossfeed_lpf_r.set_lowpass(2200.0f, 0.707f, sample_rate);

        reset();
    }

    void reset() {
        m_current_pan = m_config.pan;
        m_current_width = m_config.stereo_width;
        m_orbit_phase = 0.0f;
        std::fill(m_haas_buffer.begin(), m_haas_buffer.end(), 0.0f);
        m_haas_write_idx = 0;

        m_elevation_notch_l.reset();
        m_elevation_notch_r.reset();
        m_elevation_air_l.reset();
        m_elevation_air_r.reset();
        m_crossfeed_lpf_l.reset();
        m_crossfeed_lpf_r.reset();
    }

    void set_config(const SpatialConfig &cfg) noexcept {
        m_config = cfg;
    }

    const SpatialConfig& get_config() const noexcept {
        return m_config;
    }

    // Process interleaved stereo samples in-place
    void process_stereo(float *samples, size_t num_frames) {
        if (m_config.bypass) return;

        const float slew_alpha = 0.005f; // Slew smoothing prevents audio clicks or mic breakups

        for (size_t i = 0; i < num_frames; ++i) {
            float in_l = samples[2 * i];
            float in_r = samples[2 * i + 1];

            // 1. 8D Orbital Auto-Pan
            float target_pan = m_config.pan;
            if (m_config.auto_pan_enabled) {
                target_pan = std::sin(m_orbit_phase);
                m_orbit_phase += (2.0f * PI_F * m_config.auto_pan_speed) / m_sample_rate;
                if (m_orbit_phase >= 2.0f * PI_F) {
                    m_orbit_phase -= 2.0f * PI_F;
                }
            }

            // Anti-break slew limiter smoothing on pan & width
            m_current_pan += slew_alpha * (target_pan - m_current_pan);
            m_current_width += slew_alpha * (m_config.stereo_width - m_current_width);

            // 2. Haas Delay Decorrelator for Mono -> Stereo Expansion
            float mono_sum = 0.5f * (in_l + in_r);
            m_haas_buffer[m_haas_write_idx] = mono_sum;
            size_t read_idx = (m_haas_write_idx + m_haas_buffer.size() - m_haas_delay_samples) % m_haas_buffer.size();
            float haas_delayed = m_haas_buffer[read_idx];
            m_haas_write_idx = (m_haas_write_idx + 1) % m_haas_buffer.size();

            // Decorrelate if incoming signal is mono
            float decorr_diff = 0.35f * (mono_sum - haas_delayed);
            float l_wide = in_l + decorr_diff;
            float r_wide = in_r - decorr_diff;

            // 3. Mid/Side Stereo Wideness
            float mid = 0.5f * (l_wide + r_wide);
            float side = 0.5f * (l_wide - r_wide) * m_current_width;
            float out_l = mid + side;
            float out_r = mid - side;

            // 4. Constant-Power Sin/Cos Panning
            float clamped_pan = std::clamp(m_current_pan, -1.0f, 1.0f);
            float angle = (clamped_pan + 1.0f) * (PI_F / 4.0f); // 0 to PI/2
            float gain_l = std::cos(angle);
            float gain_r = std::sin(angle);

            // Apply constant power pan
            out_l = out_l * gain_l * 1.4142f;
            out_r = out_r * gain_r * 1.4142f;

            // 5. Pinna HRTF Vertical Elevation ("Above Head")
            if (m_config.elevation_enabled) {
                float elev_l = m_elevation_air_l.process(m_elevation_notch_l.process(out_l));
                float elev_r = m_elevation_air_r.process(m_elevation_notch_r.process(out_r));
                out_l = (1.0f - m_config.elevation_mix) * out_l + m_config.elevation_mix * elev_l;
                out_r = (1.0f - m_config.elevation_mix) * out_r + m_config.elevation_mix * elev_r;
            }

            // 6. Binaural Crossfeed ("In-Head")
            if (m_config.in_head_enabled) {
                float cross_to_r = m_crossfeed_lpf_l.process(out_l) * 0.35f * m_config.in_head_mix;
                float cross_to_l = m_crossfeed_lpf_r.process(out_r) * 0.35f * m_config.in_head_mix;
                out_l = out_l * (1.0f - 0.2f * m_config.in_head_mix) + cross_to_l;
                out_r = out_r * (1.0f - 0.2f * m_config.in_head_mix) + cross_to_r;
            }

            samples[2 * i] = out_l;
            samples[2 * i + 1] = out_r;
        }
    }

private:
    SpatialConfig m_config;
    float m_sample_rate = SAMPLE_RATE_48K;
    float m_current_pan = 0.0f;
    float m_current_width = 1.0f;
    float m_orbit_phase = 0.0f;

    std::vector<float> m_haas_buffer;
    size_t m_haas_delay_samples = 384;
    size_t m_haas_write_idx = 0;

    Biquad m_elevation_notch_l;
    Biquad m_elevation_notch_r;
    Biquad m_elevation_air_l;
    Biquad m_elevation_air_r;

    Biquad m_crossfeed_lpf_l;
    Biquad m_crossfeed_lpf_r;
};

} // namespace splcord
