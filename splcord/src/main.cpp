#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <winsock2.h>
#include <windows.h>
#include <cstring>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "splcord/dsp_types.hpp"
#include "splcord/audio_pipeline.hpp"
#include "splcord/audio_device.hpp"
#include "splcord/opus_engine.hpp"
#include "splcord/benchmark.hpp"
#include "splcord/native_gui.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main(int argc, char *argv[]) {
    std::cout << "===============================================================\n";
    std::cout << "  SPLCORD // Advanced Voice DSP Engine & Opus Codec Suite\n";
    std::cout << "  ITU-R BS.1770-4 LUFS • True-Peak Lookahead • 48kHz Direct\n";
    std::cout << "===============================================================\n\n";

    // 1. Check CLI options
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--benchmark" || arg == "-b") {
            std::cout << "[*] Executing Objective Scientific Benchmark Suite...\n\n";
            auto results = splcord::BenchmarkSuite::run_full_benchmark();
            std::string report = splcord::BenchmarkSuite::generate_report_markdown(results);
            std::cout << report << "\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: splcord.exe [options]\n";
            std::cout << "  --benchmark, -b    Run automated DSP & Opus loudness benchmarks\n";
            std::cout << "  --no-gui           Run headless without launching Edge App window\n";
            std::cout << "  --port <num>       Set custom telemetry web server port\n";
            std::cout << "  --help, -h         Show this help screen\n";
            return 0;
        }
    }

    // 2. Initialize Master DSP Pipeline
    splcord::AudioPipeline pipeline;
    pipeline.load_preset(splcord::get_preset_maximum_loudness());
    std::cout << "[+] DSP Pipeline initialized with 'Maximum Loudness (SPLCORD Pro)' preset.\n";

    // 3. Initialize Opus Engine
    splcord::OpusEngine opus;
    splcord::OpusConfig opus_cfg;
    opus_cfg.bitrate_bps = 64000;
    opus_cfg.application = OPUS_APPLICATION_AUDIO;
    opus_cfg.complexity = 10;
    if (opus.init(opus_cfg)) {
        std::cout << "[+] libopus RFC 6716 encoder initialized (64 kbps, 48kHz, Fullband, Audio Mode).\n";
    } else {
        std::cerr << "[!] Warning: libopus initialization returned error. Continuing DSP audio engine.\n";
    }

    // 4. Initialize Hardware Audio Devices (WASAPI)
    splcord::AudioDeviceManager device_mgr;
    if (device_mgr.init(&pipeline)) {
        std::cout << "[+] Audio Device Manager initialized. Found "
                  << device_mgr.get_capture_devices().size() << " capture devices and "
                  << device_mgr.get_playback_devices().size() << " playback devices.\n";
        
        // Start streaming with default capture and playback devices
        if (device_mgr.start(-1, -1)) {
            std::cout << "[+] Real-time WASAPI duplex stream active!\n";
        } else {
            std::cout << "[*] Audio device capture in standby mode.\n";
        }
    }

    bool no_gui = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--no-gui") no_gui = true;
    }

    if (!no_gui) {
        std::cout << "[+] Launching Native C++ Studio GUI & Target VC Radar (Zero Web Overhead)...\n";
        splcord::NativeGUI &gui = splcord::NativeGUI::instance();
        if (gui.init(GetModuleHandleW(NULL), &pipeline, &device_mgr)) {
            gui.run_message_loop();
            gui.shutdown();
            device_mgr.stop();
            return 0;
        } else {
            std::cerr << "[!] Native GUI initialization failed, falling back to console mode.\n";
        }
    }

    std::cout << "\n[!] SPLCORD Audio Engine is running actively in headless console mode. Press Ctrl+C to exit.\n";

    // Main keep-alive loop with periodic console status
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        auto stats = pipeline.meter().get_stats();
        float gr = pipeline.compressor().get_gain_reduction_db();
        float lat = pipeline.get_latency_ms();

        // Print compact live status
        std::cout << "\r[Status] Integrated: " << stats.integrated_lufs << " LUFS | "
                  << "True-Peak: " << stats.true_peak_dbtp << " dBTP | "
                  << "Comp GR: " << gr << " dB | "
                  << "Clip%: " << stats.clipping_percentage << "% | "
                  << "Lat: " << lat << " ms   " << std::flush;
    }

    return 0;
}
