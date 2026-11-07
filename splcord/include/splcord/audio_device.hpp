#pragma once
#include "dsp_types.hpp"
#include "audio_pipeline.hpp"
#include "miniaudio.h"
#include <vector>
#include <string>
#include <iostream>
#include <atomic>
#include <cstring>

namespace splcord {

struct DeviceInfo {
    int index;
    std::string name;
    bool is_default;
    ma_device_id id;
};

class AudioDeviceManager {
public:
    AudioDeviceManager() noexcept : m_initialized(false), m_running(false) {
        m_pipeline = nullptr;
    }

    ~AudioDeviceManager() {
        stop();
        uninit();
    }

    bool init(AudioPipeline *pipeline) {
        m_pipeline = pipeline;
        ma_result result = ma_context_init(NULL, 0, NULL, &m_context);
        if (result != MA_SUCCESS) {
            std::cerr << "[SPLCORD Audio] Failed to initialize miniaudio context: " << result << std::endl;
            return false;
        }

        refresh_devices();
        m_initialized = true;
        return true;
    }

    void refresh_devices() {
        m_capture_devices.clear();
        m_playback_devices.clear();

        ma_device_info *pPlaybackInfos = nullptr;
        ma_uint32 playbackCount = 0;
        ma_device_info *pCaptureInfos = nullptr;
        ma_uint32 captureCount = 0;

        if (ma_context_get_devices(&m_context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) == MA_SUCCESS) {
            for (ma_uint32 i = 0; i < captureCount; ++i) {
                DeviceInfo info;
                info.index = static_cast<int>(i);
                info.name = pCaptureInfos[i].name;
                info.is_default = (pCaptureInfos[i].isDefault != 0);
                info.id = pCaptureInfos[i].id;
                m_capture_devices.push_back(info);
            }

            for (ma_uint32 i = 0; i < playbackCount; ++i) {
                DeviceInfo info;
                info.index = static_cast<int>(i);
                info.name = pPlaybackInfos[i].name;
                info.is_default = (pPlaybackInfos[i].isDefault != 0);
                info.id = pPlaybackInfos[i].id;
                m_playback_devices.push_back(info);
            }
        }
    }

    static bool is_virtual_cable_name(const std::string &name) {
        std::string lower = name;
        for (char &c : lower) c = (char)std::tolower((unsigned char)c);
        return (lower.find("cable") != std::string::npos ||
                lower.find("virtual") != std::string::npos ||
                lower.find("vb-audio") != std::string::npos ||
                lower.find("audiorelay") != std::string::npos ||
                lower.find("voicemeeter") != std::string::npos);
    }

    const std::vector<DeviceInfo>& get_capture_devices() const noexcept { return m_capture_devices; }
    const std::vector<DeviceInfo>& get_playback_devices() const noexcept { return m_playback_devices; }

    int get_current_capture_index() const noexcept { return m_current_capture_idx; }
    int get_current_playback_index() const noexcept { return m_current_playback_idx; }
    bool is_current_playback_virtual_cable() const noexcept { return m_is_playback_virtual_cable; }

    bool start(int capture_dev_idx = -1, int playback_dev_idx = -1) {
        if (!m_initialized) return false;
        stop();

        // Auto-select Virtual Cable for playback if not explicitly specified
        int actual_playback_idx = playback_dev_idx;
        if (actual_playback_idx == -1) {
            for (size_t i = 0; i < m_playback_devices.size(); ++i) {
                if (is_virtual_cable_name(m_playback_devices[i].name)) {
                    actual_playback_idx = static_cast<int>(i);
                    std::cout << "[SPLCORD Audio] Auto-routing to Virtual Cable for Discord: "
                              << m_playback_devices[i].name << std::endl;
                    break;
                }
            }
        }

        m_current_capture_idx = capture_dev_idx;
        m_current_playback_idx = actual_playback_idx;
        m_is_playback_virtual_cable = (actual_playback_idx >= 0 && actual_playback_idx < static_cast<int>(m_playback_devices.size()))
            ? is_virtual_cable_name(m_playback_devices[actual_playback_idx].name)
            : false;

        ma_device_config config = ma_device_config_init(ma_device_type_duplex);
        config.sampleRate = static_cast<ma_uint32>(SAMPLE_RATE_48K);
        config.capture.format = ma_format_f32;
        config.capture.channels = 2; // Stereo capture
        config.playback.format = ma_format_f32;
        config.playback.channels = 2; // Stereo playback
        config.dataCallback = on_audio_callback;
        config.pUserData = this;
        config.periodSizeInFrames = DEFAULT_FRAME_SIZE / 2; // 10ms periods for minimum latency

        if (capture_dev_idx >= 0 && capture_dev_idx < static_cast<int>(m_capture_devices.size())) {
            config.capture.pDeviceID = &m_capture_devices[capture_dev_idx].id;
        }
        if (actual_playback_idx >= 0 && actual_playback_idx < static_cast<int>(m_playback_devices.size())) {
            config.playback.pDeviceID = &m_playback_devices[actual_playback_idx].id;
        }

        ma_result result = ma_device_init(&m_context, &config, &m_device);
        if (result != MA_SUCCESS) {
            std::cerr << "[SPLCORD Audio] Failed to initialize audio device: " << result << std::endl;
            return false;
        }

        result = ma_device_start(&m_device);
        if (result != MA_SUCCESS) {
            ma_device_uninit(&m_device);
            std::cerr << "[SPLCORD Audio] Failed to start audio device: " << result << std::endl;
            return false;
        }

        m_running = true;
        std::cout << "[SPLCORD Audio] Real-time WASAPI audio engine active at 48kHz stereo!" << std::endl;

        // Initialize WASAPI Loopback to track incoming VC voice from headphones
        ma_device_config loopback_cfg = ma_device_config_init(ma_device_type_loopback);
        loopback_cfg.sampleRate = static_cast<ma_uint32>(SAMPLE_RATE_48K);
        loopback_cfg.capture.format = ma_format_f32;
        loopback_cfg.capture.channels = 2;
        loopback_cfg.dataCallback = on_loopback_callback;
        loopback_cfg.pUserData = this;

        if (ma_device_init(&m_context, &loopback_cfg, &m_loopback_device) == MA_SUCCESS) {
            if (ma_device_start(&m_loopback_device) == MA_SUCCESS) {
                m_loopback_running = true;
                std::cout << "[SPLCORD Radar] Real-time VC Loopback Target Tracker active!" << std::endl;
            } else {
                ma_device_uninit(&m_loopback_device);
            }
        }

        return true;
    }

    void stop() {
        if (m_loopback_running) {
            m_loopback_running = false;
            ma_device_stop(&m_loopback_device);
            ma_device_uninit(&m_loopback_device);
        }
        if (m_running) {
            m_running = false;
            ma_device_stop(&m_device);
            ma_device_uninit(&m_device);
        }
    }

    void uninit() {
        stop();
        if (m_initialized) {
            ma_context_uninit(&m_context);
            m_initialized = false;
        }
    }

    bool is_running() const noexcept { return m_running; }

    // Waveform ring buffer for GUI visualization
    void get_latest_waveform(float *out_samples, size_t num_samples) const noexcept {
        size_t avail = std::min(num_samples, m_waveform_ring.size());
        size_t start = (m_waveform_write_idx + m_waveform_ring.size() - avail) % m_waveform_ring.size();
        for (size_t i = 0; i < avail; ++i) {
            out_samples[i] = m_waveform_ring[(start + i) % m_waveform_ring.size()];
        }
    }

private:
    static void on_audio_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
        auto *manager = static_cast<AudioDeviceManager*>(pDevice->pUserData);
        if (!manager || !manager->m_pipeline) return;

        const float *in_f32 = static_cast<const float*>(pInput);
        float *out_f32 = static_cast<float*>(pOutput);

        if (!in_f32) {
            if (out_f32) std::memset(out_f32, 0, frameCount * 2 * sizeof(float));
            return;
        }

        // Process audio in a local buffer so metering & waveform always run
        std::vector<float> proc_buf(frameCount * 2);
        std::memcpy(proc_buf.data(), in_f32, frameCount * 2 * sizeof(float));

        // Process through SPLCORD Master Pipeline (EQ, Compressor, Normalizer, Spatializer, Limiter, LUFS Meter)
        manager->m_pipeline->process_stereo(proc_buf.data(), frameCount);

        // Record waveform samples for GUI
        for (ma_uint32 i = 0; i < frameCount; ++i) {
            float sample_mono = 0.5f * (proc_buf[2 * i] + proc_buf[2 * i + 1]);
            manager->m_waveform_ring[manager->m_waveform_write_idx] = sample_mono;
            manager->m_waveform_write_idx = (manager->m_waveform_write_idx + 1) % manager->m_waveform_ring.size();
        }

        // Output routing (Anti-Echo / Sidetone Control):
        if (out_f32) {
            auto spat_cfg = manager->m_pipeline->get_spatializer().get_config();

            if (manager->m_is_playback_virtual_cable) {
                // Playback device is a Virtual Audio Cable (feeds Discord):
                // Send the full processed audio stream to Discord!
                std::memcpy(out_f32, proc_buf.data(), frameCount * 2 * sizeof(float));
            } else if (spat_cfg.monitor_enabled) {
                // Physical headphones / speakers and user explicitly enabled "Hear Mic":
                for (ma_uint32 i = 0; i < frameCount * 2; ++i) {
                    out_f32[i] = proc_buf[i] * spat_cfg.monitor_volume;
                }
            } else {
                // Physical headphones / speakers and monitoring is OFF:
                // SILENCE PLAYBACK! Zero echo, zero feedback! User does NOT hear their own voice!
                std::memset(out_f32, 0, frameCount * 2 * sizeof(float));
            }
        }
    }

    static void on_loopback_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
        (void)pOutput;
        auto *manager = static_cast<AudioDeviceManager*>(pDevice->pUserData);
        if (!manager || !manager->m_pipeline || !pInput) return;

        const float *in_f32 = static_cast<const float*>(pInput);
        manager->m_pipeline->tracker().analyze_incoming_stereo(in_f32, frameCount);
    }

    ma_context m_context;
    ma_device m_device;
    ma_device m_loopback_device;
    AudioPipeline *m_pipeline;
    bool m_initialized;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_loopback_running{false};

    int m_current_capture_idx = -1;
    int m_current_playback_idx = -1;
    bool m_is_playback_virtual_cable = false;

    std::vector<DeviceInfo> m_capture_devices;
    std::vector<DeviceInfo> m_playback_devices;

    mutable std::vector<float> m_waveform_ring = std::vector<float>(2048, 0.0f);
    mutable size_t m_waveform_write_idx = 0;
};

} // namespace splcord
