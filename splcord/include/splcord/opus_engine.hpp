#pragma once
#include "dsp_types.hpp"
#include "opus/opus.h"
#include <vector>
#include <chrono>
#include <stdexcept>

namespace splcord {

struct OpusConfig {
    int32_t bitrate_bps = 64000;              // Default Discord 64 kbps
    int complexity = 10;                      // Max quality (0-10)
    int application = OPUS_APPLICATION_AUDIO; // High fidelity CELT/hybrid
    int bandwidth = OPUS_BANDWIDTH_FULLBAND;  // 20kHz audio
    int signal_type = OPUS_SIGNAL_VOICE;      // Voice optimized
    int frame_size_samples = 960;             // 20ms at 48kHz
    int use_fec = 1;                          // In-band FEC enabled
    int packet_loss_perc = 5;                 // 5% simulated loss resistance
    int use_dtx = 0;                          // Continuous stream
    int use_vbr = 1;                          // Variable Bitrate
    int use_cvbr = 1;                         // Constrained VBR
    int channels = 2;                         // Stereo
};

struct OpusEncodingMetrics {
    size_t uncompressed_bytes;
    size_t compressed_bytes;
    float compression_ratio;
    float effective_bitrate_kbps;
    float encode_time_us;
};

class OpusEngine {
public:
    OpusEngine() noexcept : m_encoder(nullptr) {}

    ~OpusEngine() {
        destroy();
    }

    bool init(const OpusConfig &config = OpusConfig()) {
        destroy();
        m_config = config;

        int error = OPUS_OK;
        m_encoder = opus_encoder_create(SAMPLE_RATE_48K, m_config.channels, m_config.application, &error);
        if (error != OPUS_OK || !m_encoder) {
            return false;
        }

        apply_config(m_config);
        m_payload_buffer.resize(4000); // Max Opus payload size
        return true;
    }

    void destroy() noexcept {
        if (m_encoder) {
            opus_encoder_destroy(m_encoder);
            m_encoder = nullptr;
        }
    }

    bool is_initialized() const noexcept {
        return m_encoder != nullptr;
    }

    void apply_config(const OpusConfig &cfg) noexcept {
        m_config = cfg;
        if (!m_encoder) return;

        opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(cfg.bitrate_bps));
        opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(cfg.complexity));
        opus_encoder_ctl(m_encoder, OPUS_SET_BANDWIDTH(cfg.bandwidth));
        opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(cfg.signal_type));
        opus_encoder_ctl(m_encoder, OPUS_SET_INBAND_FEC(cfg.use_fec));
        opus_encoder_ctl(m_encoder, OPUS_SET_PACKET_LOSS_PERC(cfg.packet_loss_perc));
        opus_encoder_ctl(m_encoder, OPUS_SET_DTX(cfg.use_dtx));
        opus_encoder_ctl(m_encoder, OPUS_SET_VBR(cfg.use_vbr));
        opus_encoder_ctl(m_encoder, OPUS_SET_VBR_CONSTRAINT(cfg.use_cvbr));
    }

    const OpusConfig& get_config() const noexcept {
        return m_config;
    }

    // Encode 32-bit float interleaved stereo PCM into Opus packet
    int encode_float(const float *pcm, int frame_size, uint8_t *out_data, int max_data_bytes, OpusEncodingMetrics *out_metrics = nullptr) {
        if (!m_encoder) return -1;

        auto t0 = std::chrono::high_resolution_clock::now();
        int bytes_written = opus_encode_float(m_encoder, pcm, frame_size, out_data, max_data_bytes);
        auto t1 = std::chrono::high_resolution_clock::now();

        if (bytes_written > 0 && out_metrics) {
            float us = std::chrono::duration<float, std::micro>(t1 - t0).count();
            size_t uncompressed = frame_size * m_config.channels * sizeof(float);
            out_metrics->uncompressed_bytes = uncompressed;
            out_metrics->compressed_bytes = bytes_written;
            out_metrics->compression_ratio = static_cast<float>(uncompressed) / bytes_written;
            // Effective bitrate: (bytes * 8) / (frame_size / 48000)
            float frame_duration_s = frame_size / SAMPLE_RATE_48K;
            out_metrics->effective_bitrate_kbps = (bytes_written * 8.0f / frame_duration_s) / 1000.0f;
            out_metrics->encode_time_us = us;
        }

        return bytes_written;
    }

    // Encode 16-bit signed PCM into Opus packet
    int encode_int16(const int16_t *pcm, int frame_size, uint8_t *out_data, int max_data_bytes, OpusEncodingMetrics *out_metrics = nullptr) {
        if (!m_encoder) return -1;

        auto t0 = std::chrono::high_resolution_clock::now();
        int bytes_written = opus_encode(m_encoder, pcm, frame_size, out_data, max_data_bytes);
        auto t1 = std::chrono::high_resolution_clock::now();

        if (bytes_written > 0 && out_metrics) {
            float us = std::chrono::duration<float, std::micro>(t1 - t0).count();
            size_t uncompressed = frame_size * m_config.channels * sizeof(int16_t);
            out_metrics->uncompressed_bytes = uncompressed;
            out_metrics->compressed_bytes = bytes_written;
            out_metrics->compression_ratio = static_cast<float>(uncompressed) / bytes_written;
            float frame_duration_s = frame_size / SAMPLE_RATE_48K;
            out_metrics->effective_bitrate_kbps = (bytes_written * 8.0f / frame_duration_s) / 1000.0f;
            out_metrics->encode_time_us = us;
        }

        return bytes_written;
    }

    std::vector<uint8_t>& get_payload_buffer() noexcept {
        return m_payload_buffer;
    }

private:
    OpusEncoder *m_encoder = nullptr;
    OpusConfig m_config;
    std::vector<uint8_t> m_payload_buffer;
};

} // namespace splcord
