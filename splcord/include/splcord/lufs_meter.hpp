#pragma once
#include "dsp_types.hpp"
#include "biquad.hpp"
#include <vector>
#include <deque>
#include <numeric>

namespace splcord {

struct LoudnessStats {
    float peak_dbfs;
    float true_peak_dbtp;
    float rms_dbfs;
    float momentary_lufs;
    float short_term_lufs;
    float integrated_lufs;
    float loudness_range_lu;
    float crest_factor_db;
    float snr_db;
    uint64_t total_samples;
    uint64_t clipped_samples;
    float clipping_percentage;
};

class LoudnessMeter {
public:
    LoudnessMeter() noexcept {
        init_k_weighting_filters();
        reset();
    }

    void init_k_weighting_filters() noexcept {
        // ITU-R BS.1770-4 Stage 1: High-shelf pre-filter (head diffraction)
        // b0 = 1.53512485958697, b1 = -2.69169618940638, b2 = 1.19839281085285
        // a1 = -1.69065929318241, a2 = 0.73248077421585
        m_stage1_l.set_highshelf(1681.97f, 3.99984f, 0.7071f);
        m_stage1_r.set_highshelf(1681.97f, 3.99984f, 0.7071f);

        // ITU-R BS.1770-4 Stage 2: High-pass RLB weighting filter
        // b0 = 1.0, b1 = -2.0, b2 = 1.0
        // a1 = -1.99004745483398, a2 = 0.99007225036621
        m_stage2_l.set_highpass(38.135f, 0.5f);
        m_stage2_r.set_highpass(38.135f, 0.5f);
    }

    void reset() noexcept {
        m_stage1_l.reset();
        m_stage1_r.reset();
        m_stage2_l.reset();
        m_stage2_r.reset();

        m_peak_lin = 0.0f;
        m_true_peak_lin = 0.0f;
        m_sum_sq_samples = 0.0;
        m_total_samples = 0;
        m_clipped_samples = 0;

        m_momentary_blocks.clear();
        m_integrated_blocks.clear();

        m_cur_block_energy = 0.0;
        m_cur_block_samples = 0;

        m_noise_floor_lin = 0.0001f;
        m_signal_energy_lin = 0.001f;
    }

    // Process stereo frame (interleaved L, R)
    void process_frame(float l, float r) noexcept {
        ++m_total_samples;

        float abs_l = std::abs(l);
        float abs_r = std::abs(r);
        float frame_peak = std::max(abs_l, abs_r);

        if (frame_peak > m_peak_lin) {
            m_peak_lin = frame_peak;
        }

        // True peak oversampling estimation
        float tp_est = TruePeakLimiter::estimate_true_peak(m_prev_l, m_prev_l2, l, l);
        float tp_est_r = TruePeakLimiter::estimate_true_peak(m_prev_r, m_prev_r2, r, r);
        float frame_tp = std::max(tp_est, tp_est_r);
        if (frame_tp > m_true_peak_lin) {
            m_true_peak_lin = frame_tp;
        }

        m_prev_l2 = m_prev_l; m_prev_l = l;
        m_prev_r2 = m_prev_r; m_prev_r = r;

        // Clip counting (exceeding 0 dBFS / 1.0f)
        if (frame_peak > 1.00001f) {
            ++m_clipped_samples;
        }

        // RMS accumulation
        m_sum_sq_samples += 0.5 * (l * l + r * r);

        // K-weighting filtering for LUFS calculation
        float kl = m_stage2_l.process_sample(m_stage1_l.process_sample(l));
        float kr = m_stage2_r.process_sample(m_stage1_r.process_sample(r));
        float k_energy = 0.5f * (kl * kl + kr * kr);

        m_cur_block_energy += k_energy;
        ++m_cur_block_samples;

        // Block size = 100ms (4800 samples at 48kHz)
        if (m_cur_block_samples >= 4800) {
            float mean_block_e = static_cast<float>(m_cur_block_energy / m_cur_block_samples);
            m_momentary_blocks.push_back(mean_block_e);
            m_integrated_blocks.push_back(mean_block_e);

            // Momentary window is 400ms = 4 blocks of 100ms
            if (m_momentary_blocks.size() > 4) {
                m_momentary_blocks.pop_front();
            }

            // Estimate SNR
            if (mean_block_e > m_signal_energy_lin) {
                m_signal_energy_lin += 0.05f * (mean_block_e - m_signal_energy_lin);
            } else if (mean_block_e < 0.0005f && mean_block_e > 1e-9f) {
                m_noise_floor_lin += 0.05f * (mean_block_e - m_noise_floor_lin);
            }

            m_cur_block_energy = 0.0;
            m_cur_block_samples = 0;
        }
    }

    void process(const float *samples, size_t num_samples) noexcept {
        for (size_t i = 0; i < num_samples; ++i) {
            process_frame(samples[i], samples[i]);
        }
    }

    void process_stereo(const float *samples, size_t num_frames) noexcept {
        for (size_t i = 0; i < num_frames; ++i) {
            process_frame(samples[2 * i], samples[2 * i + 1]);
        }
    }

    LoudnessStats get_stats() const noexcept {
        LoudnessStats stats = {};
        stats.total_samples = m_total_samples;
        stats.clipped_samples = m_clipped_samples;
        stats.clipping_percentage = (m_total_samples > 0) ? (static_cast<float>(m_clipped_samples) * 100.0f / m_total_samples) : 0.0f;

        stats.peak_dbfs = linear_to_db(m_peak_lin);
        stats.true_peak_dbtp = linear_to_db(m_true_peak_lin);

        float rms = (m_total_samples > 0) ? std::sqrt(static_cast<float>(m_sum_sq_samples / m_total_samples)) : 0.0f;
        stats.rms_dbfs = linear_to_db(rms);
        stats.crest_factor_db = stats.peak_dbfs - stats.rms_dbfs;

        // Momentary LUFS (400ms window)
        if (!m_momentary_blocks.empty()) {
            float sum_e = std::accumulate(m_momentary_blocks.begin(), m_momentary_blocks.end(), 0.0f);
            float mean_e = sum_e / m_momentary_blocks.size();
            stats.momentary_lufs = -0.691f + 10.0f * std::log10(std::max(1e-8f, mean_e));
        } else {
            stats.momentary_lufs = -120.0f;
        }

        // Short-term LUFS (approx last 30 blocks = 3s)
        size_t st_count = std::min(m_integrated_blocks.size(), size_t(30));
        if (st_count > 0) {
            float sum_st = 0.0f;
            for (size_t i = m_integrated_blocks.size() - st_count; i < m_integrated_blocks.size(); ++i) {
                sum_st += m_integrated_blocks[i];
            }
            stats.short_term_lufs = -0.691f + 10.0f * std::log10(std::max(1e-8f, sum_st / st_count));
        } else {
            stats.short_term_lufs = -120.0f;
        }

        // Integrated LUFS (with absolute gate -70 LKFS and relative gate -10 LU)
        if (!m_integrated_blocks.empty()) {
            constexpr float abs_threshold_lin = 8.52e-8f; // ~ -70 LKFS
            std::vector<float> abs_gated;
            abs_gated.reserve(m_integrated_blocks.size());
            for (float e : m_integrated_blocks) {
                if (e >= abs_threshold_lin) {
                    abs_gated.push_back(e);
                }
            }

            if (!abs_gated.empty()) {
                float sum_abs = std::accumulate(abs_gated.begin(), abs_gated.end(), 0.0f);
                float mean_abs = sum_abs / abs_gated.size();
                float unweighted_lufs = -0.691f + 10.0f * std::log10(std::max(1e-8f, mean_abs));

                // Relative gate threshold: -10 LU below unweighted mean
                float rel_threshold_lufs = unweighted_lufs - 10.0f;
                float rel_threshold_lin = std::pow(10.0f, (rel_threshold_lufs + 0.691f) * 0.1f);

                std::vector<float> rel_gated;
                rel_gated.reserve(abs_gated.size());
                for (float e : abs_gated) {
                    if (e >= rel_threshold_lin) {
                        rel_gated.push_back(e);
                    }
                }

                if (!rel_gated.empty()) {
                    float sum_rel = std::accumulate(rel_gated.begin(), rel_gated.end(), 0.0f);
                    float mean_rel = sum_rel / rel_gated.size();
                    stats.integrated_lufs = -0.691f + 10.0f * std::log10(std::max(1e-8f, mean_rel));
                } else {
                    stats.integrated_lufs = unweighted_lufs;
                }
            } else {
                stats.integrated_lufs = -120.0f;
            }
        } else {
            stats.integrated_lufs = -120.0f;
        }

        // Loudness Range (LRA): approximate difference between 95th and 10th percentiles
        if (m_integrated_blocks.size() >= 10) {
            std::vector<float> sorted_lufs;
            sorted_lufs.reserve(m_integrated_blocks.size());
            for (float e : m_integrated_blocks) {
                if (e > 1e-8f) sorted_lufs.push_back(-0.691f + 10.0f * std::log10(e));
            }
            if (sorted_lufs.size() >= 10) {
                std::sort(sorted_lufs.begin(), sorted_lufs.end());
                size_t idx10 = sorted_lufs.size() / 10;
                size_t idx95 = (sorted_lufs.size() * 95) / 100;
                stats.loudness_range_lu = std::max(0.0f, sorted_lufs[idx95] - sorted_lufs[idx10]);
            }
        }

        // SNR estimate
        float signal_db = linear_to_db(std::sqrt(m_signal_energy_lin));
        float noise_db = linear_to_db(std::sqrt(m_noise_floor_lin));
        stats.snr_db = std::clamp(signal_db - noise_db, 0.0f, 96.0f);

        return stats;
    }

private:
    Biquad m_stage1_l, m_stage1_r;
    Biquad m_stage2_l, m_stage2_r;

    float m_peak_lin = 0.0f;
    float m_true_peak_lin = 0.0f;
    double m_sum_sq_samples = 0.0;
    uint64_t m_total_samples = 0;
    uint64_t m_clipped_samples = 0;

    float m_prev_l = 0.0f, m_prev_l2 = 0.0f;
    float m_prev_r = 0.0f, m_prev_r2 = 0.0f;

    double m_cur_block_energy = 0.0;
    size_t m_cur_block_samples = 0;
    std::deque<float> m_momentary_blocks;
    std::vector<float> m_integrated_blocks;

    float m_noise_floor_lin = 0.0001f;
    float m_signal_energy_lin = 0.001f;
};

} // namespace splcord
