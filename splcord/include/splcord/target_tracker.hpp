#pragma once

#include "dsp_types.hpp"
#include "biquad.hpp"
#include <string>
#include <vector>
#include <cmath>
#include <mutex>
#include <algorithm>
#include <sstream>

namespace splcord {

struct TargetAcousticMetrics {
    std::string target_name = "TargetUser";
    bool is_active = false;             // Target voice activity detected
    float pan_azimuth = 0.0f;           // -1.0 (Left) to +1.0 (Right)
    float stereo_wideness = 1.0f;       // 1.0 = normal, >1.8 = double wide / Haas
    bool above_head_detected = false;   // Pinna HRTF 7.8kHz notch present
    bool in_head_detected = false;      // 2.2kHz crossfeed present
    float reverb_index = 0.0f;          // 0.0 to 1.0 (reverb tail diffusion)
    bool reverb_detected = false;       // reverb_index > 0.35
    float target_rms_dbfs = -60.0f;
    float target_peak_dbfs = -60.0f;

    // Counter status
    bool auto_counter_enabled = true;
    bool pierce_engaged = false;
    std::string detected_trick_summary = "Standby (Listening to VC...)";
    std::string counter_action_summary = "Acoustic Shield Ready";
};

class TargetTracker {
public:
    TargetTracker() {
        init(SAMPLE_RATE_48K);
    }

    void init(float sample_rate = SAMPLE_RATE_48K) {
        m_sample_rate = sample_rate;

        // Bandpass filters for spectral notch & elevation tracking
        // Band A: 5.5 kHz reference
        m_bp_5k5.set_peaking(5500.0f, 0.0f, 2.5f, sample_rate);
        // Band B: 7.8 kHz Concha Notch region
        m_bp_7k8.set_peaking(7800.0f, 0.0f, 3.0f, sample_rate);
        // Band C: 11 kHz Air Boost region
        m_bp_11k.set_peaking(11000.0f, 0.0f, 2.5f, sample_rate);

        // Low-pass for in-head crossfeed coherence (2.2 kHz)
        m_lp_2k2.set_lowpass(2200.0f, 0.707f, sample_rate);

        reset();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_metrics = TargetAcousticMetrics{};
        m_smoothed_pan = 0.0f;
        m_smoothed_width = 1.0f;
        m_smoothed_reverb = 0.0f;
        m_energy_history.assign(32, 0.0f);
        m_history_idx = 0;
    }

    void set_target_name(const std::string &name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_metrics.target_name = name.empty() ? "TargetUser" : name;
    }

    std::string get_target_name() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_metrics.target_name;
    }

    void set_auto_counter(bool enable) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_metrics.auto_counter_enabled = enable;
        if (!enable) m_metrics.pierce_engaged = false;
    }

    void force_pierce(bool enable) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_force_pierce = enable;
        m_metrics.pierce_engaged = enable;
    }

    bool is_pierce_active() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_force_pierce || (m_metrics.auto_counter_enabled && m_metrics.pierce_engaged);
    }

    TargetAcousticMetrics get_metrics() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_metrics;
    }

    // Process incoming loopback/VC audio frames from the other user
    void analyze_incoming_stereo(const float *samples, size_t num_frames) {
        if (!samples || num_frames == 0) return;

        float sum_sq_l = 0.0f;
        float sum_sq_r = 0.0f;
        float peak_val = 0.0f;
        float sum_sq_mid = 0.0f;
        float sum_sq_side = 0.0f;

        float sum_5k5 = 0.0f;
        float sum_7k8 = 0.0f;
        float sum_11k = 0.0f;

        for (size_t i = 0; i < num_frames; ++i) {
            float l = samples[2 * i];
            float r = samples[2 * i + 1];

            sum_sq_l += l * l;
            sum_sq_r += r * r;

            float abs_s = std::max(std::abs(l), std::abs(r));
            if (abs_s > peak_val) peak_val = abs_s;

            // Mid / Side decomposition
            float mid = 0.5f * (l + r);
            float side = 0.5f * (l - r);
            sum_sq_mid += mid * mid;
            sum_sq_side += side * side;

            // Spectral filters on mono sum
            float b5 = m_bp_5k5.process_sample(mid);
            float b7 = m_bp_7k8.process_sample(mid);
            float b11 = m_bp_11k.process_sample(mid);

            sum_5k5 += b5 * b5;
            sum_7k8 += b7 * b7;
            sum_11k += b11 * b11;
        }

        float rms_l = std::sqrt(sum_sq_l / num_frames);
        float rms_r = std::sqrt(sum_sq_r / num_frames);
        float rms_tot = std::sqrt(0.5f * (sum_sq_l + sum_sq_r) / num_frames);

        std::lock_guard<std::mutex> lock(m_mutex);

        // Voice activity gate (-46 dBFS)
        m_metrics.is_active = (rms_tot > 0.005f);
        m_metrics.target_rms_dbfs = linear_to_db(rms_tot);
        m_metrics.target_peak_dbfs = linear_to_db(peak_val);

        if (!m_metrics.is_active) {
            // Decay metrics slowly when target is silent
            m_metrics.reverb_index *= 0.95f;
            m_metrics.reverb_detected = (m_metrics.reverb_index > 0.35f);
            if (!m_force_pierce && !m_metrics.reverb_detected) {
                m_metrics.pierce_engaged = false;
            }
            return;
        }

        // 1. Panning Azimuth (Interaural Level Difference)
        float raw_pan = (rms_r - rms_l) / (rms_r + rms_l + EPSILON);
        m_smoothed_pan += 0.15f * (raw_pan - m_smoothed_pan);
        m_metrics.pan_azimuth = std::clamp(m_smoothed_pan, -1.0f, 1.0f);

        // 2. Stereo Wideness (Side to Mid ratio)
        float rms_mid = std::sqrt(sum_sq_mid / num_frames);
        float rms_side = std::sqrt(sum_sq_side / num_frames);
        float raw_width = (rms_side / (rms_mid + EPSILON)) * 2.0f;
        m_smoothed_width += 0.10f * (raw_width - m_smoothed_width);
        m_metrics.stereo_wideness = std::clamp(m_smoothed_width, 0.0f, 3.5f);

        // 3. Pinna HRTF "Above Head" Elevation Notch Detection
        // If 7.8kHz concha notch has lower energy than 5.5kHz and 11kHz
        float e5 = std::sqrt(sum_5k5 / num_frames);
        float e7 = std::sqrt(sum_7k8 / num_frames);
        float e11 = std::sqrt(sum_11k / num_frames);

        bool notch_found = (e7 < e5 * 0.55f && e11 > e7 * 1.35f);
        m_metrics.above_head_detected = notch_found;

        // 4. Reverb Diffusion & Tail Tracking
        // Reverb creates a lower crest factor and lingering energy floor
        float crest_factor_db = m_metrics.target_peak_dbfs - m_metrics.target_rms_dbfs;
        m_energy_history[m_history_idx] = rms_tot;
        m_history_idx = (m_history_idx + 1) % m_energy_history.size();

        // Calculate variance of energy over history buffer
        float mean_e = 0.0f;
        for (float val : m_energy_history) mean_e += val;
        mean_e /= m_energy_history.size();

        float variance = 0.0f;
        for (float val : m_energy_history) variance += (val - mean_e) * (val - mean_e);
        variance /= m_energy_history.size();

        // Reverb reduces energy variance (smoothes the tail) and compresses crest factor
        float raw_reverb = 0.0f;
        if (crest_factor_db < 12.0f && variance < 0.001f && rms_tot > 0.015f) {
            raw_reverb = std::clamp((12.0f - crest_factor_db) / 6.0f, 0.0f, 1.0f);
        }
        m_smoothed_reverb += 0.12f * (raw_reverb - m_smoothed_reverb);
        m_metrics.reverb_index = m_smoothed_reverb;
        m_metrics.reverb_detected = (m_metrics.reverb_index >= 0.35f);

        // 5. Synthesize Detected Tricks Summary
        std::stringstream ss_trick;
        if (m_metrics.reverb_detected) ss_trick << "[REVERB TAIL DETECTED (" << static_cast<int>(m_metrics.reverb_index * 100) << "%)] ";
        if (m_metrics.above_head_detected) ss_trick << "[ABOVE HEAD PINNA NOTCH] ";
        if (m_metrics.stereo_wideness > 1.8f) ss_trick << "[DOUBLE WIDENESS " << static_cast<int>(m_metrics.stereo_wideness * 100) << "%] ";
        if (std::abs(m_metrics.pan_azimuth) > 0.25f) {
            ss_trick << "[PANNED " << (m_metrics.pan_azimuth < 0 ? "LEFT " : "RIGHT ") 
                     << static_cast<int>(std::abs(m_metrics.pan_azimuth) * 100) << "%]";
        }
        if (ss_trick.str().empty()) ss_trick << "[Standard Dry Vocal / Center]";
        m_metrics.detected_trick_summary = ss_trick.str();

        // 6. Automated Acoustic Counter Decision
        if (m_force_pierce) {
            m_metrics.pierce_engaged = true;
            m_metrics.counter_action_summary = "⚔️ FORCED PIERCE: +4.5dB Formant Knife • Anti-Reverb De-Mask Active";
        } else if (m_metrics.auto_counter_enabled && (m_metrics.reverb_detected || m_metrics.above_head_detected)) {
            m_metrics.pierce_engaged = true;
            std::stringstream ss_counter;
            ss_counter << "⚡ AUTO-PIERCE ENGAGED: Countering " << (m_metrics.reverb_detected ? "Reverb" : "Elevation") 
                       << " (+4.5dB 3.2kHz Knife • -4dB Mud Cut • Maximum Loudness -9.6 LUFS)";
            m_metrics.counter_action_summary = ss_counter.str();
        } else {
            m_metrics.pierce_engaged = false;
            m_metrics.counter_action_summary = "🛡️ Counter Matrix Monitoring...";
        }
    }

private:
    std::mutex m_mutex;
    TargetAcousticMetrics m_metrics;
    float m_sample_rate = SAMPLE_RATE_48K;

    bool m_force_pierce = false;
    float m_smoothed_pan = 0.0f;
    float m_smoothed_width = 1.0f;
    float m_smoothed_reverb = 0.0f;

    std::vector<float> m_energy_history;
    size_t m_history_idx = 0;

    Biquad m_bp_5k5;
    Biquad m_bp_7k8;
    Biquad m_bp_11k;
    Biquad m_lp_2k2;
};

} // namespace splcord
