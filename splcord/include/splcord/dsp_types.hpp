#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace splcord {

constexpr float SAMPLE_RATE_48K = 48000.0f;
constexpr size_t DEFAULT_FRAME_SIZE = 960; // 20ms at 48kHz
constexpr float PI_F = 3.14159265358979323846f;
constexpr float EPSILON = 1e-9f;

inline float linear_to_db(float lin) noexcept {
    if (lin <= EPSILON) return -120.0f;
    return 20.0f * std::log10(lin);
}

inline float db_to_linear(float db) noexcept {
    return std::pow(10.0f, db * 0.05f);
}

inline float clampf(float val, float min_val, float max_val) noexcept {
    return std::clamp(val, min_val, max_val);
}

// Convert 16-bit signed PCM to 32-bit float [-1.0, 1.0]
inline void pcm16_to_float(const int16_t *src, float *dst, size_t num_samples) noexcept {
    for (size_t i = 0; i < num_samples; ++i) {
        dst[i] = src[i] * (1.0f / 32768.0f);
    }
}

// Convert 32-bit float [-1.0, 1.0] to 16-bit signed PCM with saturation
inline void float_to_pcm16(const float *src, int16_t *dst, size_t num_samples) noexcept {
    for (size_t i = 0; i < num_samples; ++i) {
        float s = clampf(src[i], -1.0f, 1.0f);
        dst[i] = static_cast<int16_t>(s * 32767.0f);
    }
}

} // namespace splcord
