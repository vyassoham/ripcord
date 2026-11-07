#pragma once
#include "dsp_types.hpp"

namespace splcord {

enum class FilterType {
    Bypass,
    HighPass,
    LowPass,
    LowShelf,
    HighShelf,
    Peaking,
    BandPass,
    Notch
};

class Biquad {
public:
    Biquad() noexcept {
        reset();
        set_bypass();
    }

    void reset() noexcept {
        m_s1 = 0.0f;
        m_s2 = 0.0f;
    }

    void set_bypass() noexcept {
        m_type = FilterType::Bypass;
        m_b0 = 1.0f; m_b1 = 0.0f; m_b2 = 0.0f;
        m_a1 = 0.0f; m_a2 = 0.0f;
    }

    void set_peaking(float f0, float gain_db, float Q, float fs = SAMPLE_RATE_48K) noexcept {
        m_type = FilterType::Peaking;
        m_f0 = f0; m_gain_db = gain_db; m_q = Q;
        float A = std::pow(10.0f, gain_db * 0.025f); // 10^(gain_db / 40)
        float w0 = 2.0f * PI_F * f0 / fs;
        float alpha = std::sin(w0) / (2.0f * Q);
        float cos_w0 = std::cos(w0);

        float a0 = 1.0f + alpha / A;
        m_b0 = (1.0f + alpha * A) / a0;
        m_b1 = (-2.0f * cos_w0) / a0;
        m_b2 = (1.0f - alpha * A) / a0;
        m_a1 = (-2.0f * cos_w0) / a0;
        m_a2 = (1.0f - alpha / A) / a0;
    }

    void set_highpass(float f0, float Q = 0.7071f, float fs = SAMPLE_RATE_48K) noexcept {
        m_type = FilterType::HighPass;
        m_f0 = f0; m_q = Q;
        float w0 = 2.0f * PI_F * f0 / fs;
        float alpha = std::sin(w0) / (2.0f * Q);
        float cos_w0 = std::cos(w0);

        float a0 = 1.0f + alpha;
        m_b0 = ((1.0f + cos_w0) * 0.5f) / a0;
        m_b1 = (-(1.0f + cos_w0)) / a0;
        m_b2 = ((1.0f + cos_w0) * 0.5f) / a0;
        m_a1 = (-2.0f * cos_w0) / a0;
        m_a2 = (1.0f - alpha) / a0;
    }

    void set_lowpass(float f0, float Q = 0.7071f, float fs = SAMPLE_RATE_48K) noexcept {
        m_type = FilterType::LowPass;
        m_f0 = f0; m_q = Q;
        float w0 = 2.0f * PI_F * f0 / fs;
        float alpha = std::sin(w0) / (2.0f * Q);
        float cos_w0 = std::cos(w0);

        float a0 = 1.0f + alpha;
        m_b0 = ((1.0f - cos_w0) * 0.5f) / a0;
        m_b1 = (1.0f - cos_w0) / a0;
        m_b2 = ((1.0f - cos_w0) * 0.5f) / a0;
        m_a1 = (-2.0f * cos_w0) / a0;
        m_a2 = (1.0f - alpha) / a0;
    }

    void set_lowshelf(float f0, float gain_db, float Q = 0.7071f, float fs = SAMPLE_RATE_48K) noexcept {
        m_type = FilterType::LowShelf;
        m_f0 = f0; m_gain_db = gain_db; m_q = Q;
        float A = std::pow(10.0f, gain_db * 0.025f);
        float w0 = 2.0f * PI_F * f0 / fs;
        float cos_w0 = std::cos(w0);
        float sin_w0 = std::sin(w0);
        float alpha = sin_w0 / (2.0f * Q);
        float two_sqrt_A_alpha = 2.0f * std::sqrt(A) * alpha;

        float a0 = (A + 1.0f) + (A - 1.0f) * cos_w0 + two_sqrt_A_alpha;
        m_b0 = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + two_sqrt_A_alpha)) / a0;
        m_b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0)) / a0;
        m_b2 = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - two_sqrt_A_alpha)) / a0;
        m_a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0)) / a0;
        m_a2 = ((A + 1.0f) + (A - 1.0f) * cos_w0 - two_sqrt_A_alpha) / a0;
    }

    void set_highshelf(float f0, float gain_db, float Q = 0.7071f, float fs = SAMPLE_RATE_48K) noexcept {
        m_type = FilterType::HighShelf;
        m_f0 = f0; m_gain_db = gain_db; m_q = Q;
        float A = std::pow(10.0f, gain_db * 0.025f);
        float w0 = 2.0f * PI_F * f0 / fs;
        float cos_w0 = std::cos(w0);
        float sin_w0 = std::sin(w0);
        float alpha = sin_w0 / (2.0f * Q);
        float two_sqrt_A_alpha = 2.0f * std::sqrt(A) * alpha;

        float a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + two_sqrt_A_alpha;
        m_b0 = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + two_sqrt_A_alpha)) / a0;
        m_b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0)) / a0;
        m_b2 = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - two_sqrt_A_alpha)) / a0;
        m_a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0)) / a0;
        m_a2 = ((A + 1.0f) - (A - 1.0f) * cos_w0 - two_sqrt_A_alpha) / a0;
    }

    void set_notch(float f0, float Q, float fs = SAMPLE_RATE_48K) noexcept {
        m_type = FilterType::Notch;
        m_f0 = f0; m_q = Q;
        float w0 = 2.0f * PI_F * f0 / fs;
        float alpha = std::sin(w0) / (2.0f * Q);
        float cos_w0 = std::cos(w0);

        float a0 = 1.0f + alpha;
        m_b0 = 1.0f / a0;
        m_b1 = (-2.0f * cos_w0) / a0;
        m_b2 = 1.0f / a0;
        m_a1 = (-2.0f * cos_w0) / a0;
        m_a2 = (1.0f - alpha) / a0;
    }

    // Transposed Direct Form II processing: single sample
    inline float process_sample(float in) noexcept {
        if (m_type == FilterType::Bypass) return in;
        float out = m_b0 * in + m_s1;
        m_s1 = m_b1 * in - m_a1 * out + m_s2;
        m_s2 = m_b2 * in - m_a2 * out;
        return out;
    }

    inline float process(float in) noexcept {
        return process_sample(in);
    }

    FilterType get_type() const noexcept { return m_type; }
    float get_frequency() const noexcept { return m_f0; }
    float get_gain_db() const noexcept { return m_gain_db; }
    float get_q() const noexcept { return m_q; }

    // Magnitude response at frequency f (Hz)
    float get_magnitude_response(float f, float fs = SAMPLE_RATE_48K) const noexcept {
        if (m_type == FilterType::Bypass) return 1.0f;
        float w = 2.0f * PI_F * f / fs;
        float cos_w = std::cos(w);
        float cos_2w = std::cos(2.0f * w);
        float sin_w = std::sin(w);
        float sin_2w = std::sin(2.0f * w);

        float num_re = m_b0 + m_b1 * cos_w + m_b2 * cos_2w;
        float num_im = -(m_b1 * sin_w + m_b2 * sin_2w);
        float den_re = 1.0f + m_a1 * cos_w + m_a2 * cos_2w;
        float den_im = -(m_a1 * sin_w + m_a2 * sin_2w);

        float num_mag_sq = num_re * num_re + num_im * num_im;
        float den_mag_sq = den_re * den_re + den_im * den_im;
        if (den_mag_sq <= EPSILON) return 1.0f;
        return std::sqrt(num_mag_sq / den_mag_sq);
    }

private:
    FilterType m_type = FilterType::Bypass;
    float m_f0 = 1000.0f;
    float m_gain_db = 0.0f;
    float m_q = 0.7071f;

    // Filter coefficients
    float m_b0 = 1.0f, m_b1 = 0.0f, m_b2 = 0.0f;
    float m_a1 = 0.0f, m_a2 = 0.0f;

    // State variables (Transposed Direct Form II)
    float m_s1 = 0.0f, m_s2 = 0.0f;
};

// Dual-channel (Stereo) Biquad filter
class StereoBiquad {
public:
    void reset() noexcept {
        m_left.reset();
        m_right.reset();
    }

    void set_peaking(float f0, float gain_db, float Q, float fs = SAMPLE_RATE_48K) noexcept {
        m_left.set_peaking(f0, gain_db, Q, fs);
        m_right.set_peaking(f0, gain_db, Q, fs);
    }

    void set_highpass(float f0, float Q = 0.7071f, float fs = SAMPLE_RATE_48K) noexcept {
        m_left.set_highpass(f0, Q, fs);
        m_right.set_highpass(f0, Q, fs);
    }

    void set_lowpass(float f0, float Q = 0.7071f, float fs = SAMPLE_RATE_48K) noexcept {
        m_left.set_lowpass(f0, Q, fs);
        m_right.set_lowpass(f0, Q, fs);
    }

    void set_lowshelf(float f0, float gain_db, float Q = 0.7071f, float fs = SAMPLE_RATE_48K) noexcept {
        m_left.set_lowshelf(f0, gain_db, Q, fs);
        m_right.set_lowshelf(f0, gain_db, Q, fs);
    }

    void set_highshelf(float f0, float gain_db, float Q = 0.7071f, float fs = SAMPLE_RATE_48K) noexcept {
        m_left.set_highshelf(f0, gain_db, Q, fs);
        m_right.set_highshelf(f0, gain_db, Q, fs);
    }

    void set_bypass() noexcept {
        m_left.set_bypass();
        m_right.set_bypass();
    }

    inline void process_stereo_sample(float &left, float &right) noexcept {
        left = m_left.process_sample(left);
        right = m_right.process_sample(right);
    }

    const Biquad& left() const noexcept { return m_left; }

private:
    Biquad m_left;
    Biquad m_right;
};

} // namespace splcord
