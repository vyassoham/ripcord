#pragma once
#include "dsp_types.hpp"
#include "gain.hpp"
#include "parametric_eq.hpp"
#include "noise_gate.hpp"
#include "compressor.hpp"
#include "loudness_normalizer.hpp"
#include "true_peak_limiter.hpp"
#include "lufs_meter.hpp"
#include "spatializer.hpp"
#include "target_tracker.hpp"
#include "presets.hpp"
#include <chrono>
#include <mutex>

namespace splcord {

class AudioPipeline {
public:
    AudioPipeline() noexcept {
        m_spatializer.init(SAMPLE_RATE_48K);
        m_preset_a = get_preset_broadcast();
        m_preset_b = get_preset_maximum_loudness();
        m_active_is_a = true;
        load_preset(m_preset_a);
    }

    void load_preset(const Preset &p) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_input_gain.set_gain_db(p.input_gain_db);
        m_output_gain.set_gain_db(p.output_gain_db);

        m_gate.set_bypass(!p.gate_enabled);
        m_gate.set_threshold_db(p.gate_threshold_db);
        m_gate.set_ratio(p.gate_ratio);
        m_gate.set_attack_ms(p.gate_attack_ms);
        m_gate.set_hold_ms(p.gate_hold_ms);
        m_gate.set_release_ms(p.gate_release_ms);

        m_eq.set_master_bypass(!p.eq_enabled);
        m_eq.set_band(0, FilterType::HighPass, p.hpf_freq, 0.0f, 0.7071f);
        m_eq.set_band(1, FilterType::LowShelf, p.low_shelf_freq, p.low_shelf_gain_db, 0.7071f);
        m_eq.set_band(2, FilterType::Peaking, p.mud_cut_freq, p.mud_cut_gain_db, 1.4f);
        m_eq.set_band(3, FilterType::Peaking, p.body_freq, p.body_gain_db, 1.2f);
        m_eq.set_band(4, FilterType::Peaking, p.presence_freq, p.presence_gain_db, 1.5f);
        m_eq.set_band(5, FilterType::HighShelf, p.air_freq, p.air_gain_db, 0.7071f);
        m_eq.set_band(6, FilterType::LowPass, p.lpf_freq, 0.0f, 0.7071f);

        m_compressor.set_bypass(!p.compressor_enabled);
        m_compressor.set_threshold_db(p.comp_threshold_db);
        m_compressor.set_ratio(p.comp_ratio);
        m_compressor.set_knee_db(p.comp_knee_db);
        m_compressor.set_attack_ms(p.comp_attack_ms);
        m_compressor.set_release_ms(p.comp_release_ms);
        m_compressor.set_makeup_gain_db(p.comp_makeup_gain_db);

        m_normalizer.set_bypass(!p.normalizer_enabled);
        m_normalizer.set_target_lufs(p.norm_target_lufs);
        m_normalizer.set_max_boost_db(p.norm_max_boost_db);

        m_limiter.set_bypass(!p.limiter_enabled);
        m_limiter.set_ceiling_db(p.limiter_ceiling_db);
        m_limiter.set_release_ms(p.limiter_release_ms);
    }

    void toggle_ab_comparison() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_active_is_a) {
            load_preset(m_preset_b);
            m_active_is_a = false;
        } else {
            load_preset(m_preset_a);
            m_active_is_a = true;
        }
    }

    bool is_preset_a_active() const noexcept { return m_active_is_a; }
    void set_preset_a(const Preset &p) { m_preset_a = p; }
    void set_preset_b(const Preset &p) { m_preset_b = p; }

    // Direct accessors with thread-safety
    GainStage& input_gain() noexcept { return m_input_gain; }
    GainStage& output_gain() noexcept { return m_output_gain; }
    ParametricEQ& eq() noexcept { return m_eq; }
    NoiseGate& gate() noexcept { return m_gate; }
    Compressor& compressor() noexcept { return m_compressor; }
    LoudnessNormalizer& normalizer() noexcept { return m_normalizer; }
    TruePeakLimiter& limiter() noexcept { return m_limiter; }
    LoudnessMeter& meter() noexcept { return m_meter; }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_input_gain.reset();
        m_output_gain.reset();
        m_eq.reset();
        m_gate.reset();
        m_compressor.reset();
        m_normalizer.reset();
        m_limiter.reset();
        m_meter.reset();
    }

    float get_latency_ms() const noexcept {
        // Lookahead delay + per-frame processing duration
        float lookahead_ms = (m_limiter.get_lookahead_samples() / SAMPLE_RATE_48K) * 1000.0f;
        float dsp_frame_ms = (m_last_process_us * 0.001f);
        if (m_last_num_samples > 0) {
            dsp_frame_ms = (dsp_frame_ms / m_last_num_samples) * DEFAULT_FRAME_SIZE;
        }
        return lookahead_ms + dsp_frame_ms;
    }

    // Process mono audio in-place
    void process(float *samples, size_t num_samples) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_last_num_samples = num_samples;
        auto t0 = std::chrono::high_resolution_clock::now();

        // 1. Input Gain Stage
        m_input_gain.process(samples, num_samples);

        // 2. Parametric Equalizer
        m_eq.process(samples, num_samples);

        // 3. Noise Gate / Expander
        m_gate.process(samples, num_samples);

        // 4. Studio Broadcast Compressor
        m_compressor.process(samples, num_samples);

        // 5. Loudness Normalizer / Auto-Leveler
        m_normalizer.process(samples, num_samples);

        // 6. True-Peak Lookahead Limiter
        m_limiter.process(samples, num_samples);

        // 7. Output Gain Stage
        m_output_gain.process(samples, num_samples);

        // 8. Quality & Loudness Metering
        m_meter.process(samples, num_samples);

        auto t1 = std::chrono::high_resolution_clock::now();
        m_last_process_us = std::chrono::duration<float, std::micro>(t1 - t0).count();
    }

    // Process stereo interleaved audio in-place
    void process_stereo(float *samples, size_t num_frames) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_last_num_samples = num_frames;
        auto t0 = std::chrono::high_resolution_clock::now();

        m_input_gain.process_stereo(samples, num_frames);
        m_eq.process_stereo(samples, num_frames);
        m_gate.process_stereo(samples, num_frames);
        m_compressor.process_stereo(samples, num_frames);
        m_normalizer.process_stereo(samples, num_frames);
        m_spatializer.process_stereo(samples, num_frames);
        m_limiter.process_stereo(samples, num_frames);
        m_output_gain.process_stereo(samples, num_frames);
        m_meter.process_stereo(samples, num_frames);

        auto t1 = std::chrono::high_resolution_clock::now();
        m_last_process_us = std::chrono::duration<float, std::micro>(t1 - t0).count();
    }

    Spatializer& get_spatializer() noexcept { return m_spatializer; }
    TargetTracker& tracker() noexcept { return m_tracker; }

    void set_spatial_config(const SpatialConfig &cfg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_spatializer.set_config(cfg);
    }
    SpatialConfig get_spatial_config() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_spatializer.get_config();
    }

private:
    std::mutex m_mutex;
    GainStage m_input_gain;
    ParametricEQ m_eq;
    NoiseGate m_gate;
    Compressor m_compressor;
    LoudnessNormalizer m_normalizer;
    Spatializer m_spatializer;
    TargetTracker m_tracker;
    TruePeakLimiter m_limiter;
    GainStage m_output_gain;
    LoudnessMeter m_meter;

    Preset m_preset_a;
    Preset m_preset_b;
    bool m_active_is_a = true;

    float m_last_process_us = 0.0f;
    size_t m_last_num_samples = DEFAULT_FRAME_SIZE;
};

} // namespace splcord
