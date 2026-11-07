#pragma once
#include "dsp_types.hpp"
#include <string>

namespace splcord {

struct Preset {
    std::string name;
    std::string description;

    // Gain
    float input_gain_db = 0.0f;
    float output_gain_db = 0.0f;

    // Gate
    bool gate_enabled = true;
    float gate_threshold_db = -48.0f;
    float gate_ratio = 4.0f;
    float gate_attack_ms = 2.0f;
    float gate_hold_ms = 50.0f;
    float gate_release_ms = 100.0f;

    // Parametric EQ
    bool eq_enabled = true;
    float hpf_freq = 80.0f;
    float low_shelf_freq = 180.0f;
    float low_shelf_gain_db = 1.5f;
    float mud_cut_freq = 400.0f;
    float mud_cut_gain_db = -2.5f;
    float body_freq = 1200.0f;
    float body_gain_db = 1.0f;
    float presence_freq = 3200.0f;
    float presence_gain_db = 4.0f;
    float air_freq = 10000.0f;
    float air_gain_db = 2.5f;
    float lpf_freq = 18000.0f;

    // Compressor
    bool compressor_enabled = true;
    float comp_threshold_db = -18.0f;
    float comp_ratio = 4.0f;
    float comp_knee_db = 4.0f;
    float comp_attack_ms = 5.0f;
    float comp_release_ms = 80.0f;
    float comp_makeup_gain_db = 4.0f;

    // Loudness Normalizer
    bool normalizer_enabled = true;
    float norm_target_lufs = -14.0f;
    float norm_max_boost_db = 8.0f;

    // True Peak Limiter
    bool limiter_enabled = true;
    float limiter_ceiling_db = -0.1f;
    float limiter_release_ms = 40.0f;
};

inline Preset get_preset_broadcast() noexcept {
    Preset p;
    p.name = "Clean Broadcast";
    p.description = "Balanced, professional vocal warmth with gentle dynamic leveling.";
    p.input_gain_db = 2.0f;
    p.output_gain_db = 0.0f;
    p.gate_threshold_db = -50.0f;
    p.comp_threshold_db = -16.0f;
    p.comp_ratio = 3.0f;
    p.comp_makeup_gain_db = 3.0f;
    p.norm_target_lufs = -16.0f;
    p.limiter_ceiling_db = -1.0f;
    return p;
}

inline Preset get_preset_intelligibility() noexcept {
    Preset p;
    p.name = "High Intelligibility Radio";
    p.description = "Crisp presence boost around 3.2kHz for maximum clarity in noisy environments.";
    p.input_gain_db = 4.0f;
    p.output_gain_db = 0.0f;
    p.hpf_freq = 100.0f;
    p.mud_cut_gain_db = -4.0f;
    p.presence_freq = 3400.0f;
    p.presence_gain_db = 6.0f;
    p.comp_threshold_db = -20.0f;
    p.comp_ratio = 5.0f;
    p.comp_makeup_gain_db = 5.0f;
    p.norm_target_lufs = -13.0f;
    p.limiter_ceiling_db = -0.3f;
    return p;
}

inline Preset get_preset_maximum_loudness() noexcept {
    Preset p;
    p.name = "Maximum Loudness (SPLCORD Pro)";
    p.description = "Aggressive multi-stage crest-factor minimization reaching -8 to -10 LUFS with zero clipping.";
    p.input_gain_db = 8.0f;
    p.output_gain_db = 0.0f;
    p.gate_threshold_db = -42.0f;
    p.mud_cut_gain_db = -3.5f;
    p.presence_gain_db = 5.0f;
    p.comp_threshold_db = -24.0f;
    p.comp_ratio = 8.0f;
    p.comp_knee_db = 6.0f;
    p.comp_attack_ms = 2.0f;
    p.comp_release_ms = 40.0f;
    p.comp_makeup_gain_db = 8.0f;
    p.norm_target_lufs = -9.0f;
    p.norm_max_boost_db = 12.0f;
    p.limiter_ceiling_db = -0.1f;
    p.limiter_release_ms = 25.0f;
    return p;
}

inline Preset get_preset_gaming() noexcept {
    Preset p;
    p.name = "Competitive Gaming Comms";
    p.description = "Tight noise gate, punchy compression, and low-latency response for Discord calls.";
    p.input_gain_db = 3.0f;
    p.output_gain_db = 0.0f;
    p.gate_threshold_db = -44.0f;
    p.gate_hold_ms = 30.0f;
    p.comp_threshold_db = -18.0f;
    p.comp_ratio = 4.0f;
    p.comp_makeup_gain_db = 4.0f;
    p.norm_target_lufs = -14.0f;
    p.limiter_ceiling_db = -0.2f;
    return p;
}

inline Preset get_preset_bypass() noexcept {
    Preset p;
    p.name = "Bypass / Flat";
    p.description = "Raw microphone passthrough with all processing disabled for baseline A/B comparison.";
    p.input_gain_db = 0.0f;
    p.output_gain_db = 0.0f;
    p.gate_enabled = false;
    p.eq_enabled = false;
    p.compressor_enabled = false;
    p.normalizer_enabled = false;
    p.limiter_enabled = false;
    return p;
}

} // namespace splcord
