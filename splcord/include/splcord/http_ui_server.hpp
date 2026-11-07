#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include "audio_pipeline.hpp"
#include "audio_device.hpp"
#include "benchmark.hpp"

namespace splcord {

static const char G_UI_HTML[] = 
#include "ui_html.inl"
;

class HTTPServer {
public:
    static void init(AudioPipeline *pipeline, AudioDeviceManager *dev_mgr) {
        s_pipeline = pipeline;
        s_dev_mgr = dev_mgr;
        CreateThread(NULL, 0, server_thread_proc, NULL, 0, NULL);
    }

    static void launch_browser_app() {
        Sleep(250);
        wchar_t browserPath[MAX_PATH] = L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
        if (GetFileAttributesW(browserPath) == INVALID_FILE_ATTRIBUTES) {
            wcscpy(browserPath, L"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe");
        }
        if (GetFileAttributesW(browserPath) == INVALID_FILE_ATTRIBUTES) {
            wcscpy(browserPath, L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe");
        }
        if (GetFileAttributesW(browserPath) == INVALID_FILE_ATTRIBUTES) {
            wcscpy(browserPath, L"C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe");
        }

        if (GetFileAttributesW(browserPath) != INVALID_FILE_ATTRIBUTES) {
            wchar_t cmdLine[2048];
            wsprintfW(cmdLine, L"\"%s\" --app=http://127.0.0.1:48200/ --window-size=1280,850", browserPath);
            STARTUPINFOW si = {};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi = {};
            if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                return;
            }
        }
        ShellExecuteW(NULL, L"open", L"http://127.0.0.1:48200/", NULL, NULL, SW_SHOWNORMAL);
    }

private:
    static inline AudioPipeline *s_pipeline = nullptr;
    static inline AudioDeviceManager *s_dev_mgr = nullptr;

    static std::string build_telemetry_json() {
        if (!s_pipeline) return "{}";

        auto stats = s_pipeline->meter().get_stats();
        float gr = s_pipeline->compressor().get_gain_reduction_db();
        float lat = s_pipeline->get_latency_ms();

        std::vector<float> wf(128, 0.0f);
        if (s_dev_mgr) {
            s_dev_mgr->get_latest_waveform(wf.data(), wf.size());
        }

        std::ostringstream ss;
        auto spat = s_pipeline ? s_pipeline->get_spatial_config() : SpatialConfig{};
        auto target = s_pipeline ? s_pipeline->tracker().get_metrics() : TargetAcousticMetrics{};
        ss << "{"
           << "\"momentary_lufs\":" << stats.momentary_lufs << ","
           << "\"short_term_lufs\":" << stats.short_term_lufs << ","
           << "\"integrated_lufs\":" << stats.integrated_lufs << ","
           << "\"peak_dbfs\":" << stats.peak_dbfs << ","
           << "\"true_peak_dbtp\":" << stats.true_peak_dbtp << ","
           << "\"comp_gr\":" << gr << ","
           << "\"clipping_perc\":" << stats.clipping_percentage << ","
           << "\"snr_db\":" << stats.snr_db << ","
           << "\"latency_ms\":" << lat << ","
           << "\"pan\":" << spat.pan << ","
           << "\"stereo_width\":" << spat.stereo_width << ","
           << "\"elevation_enabled\":" << (spat.elevation_enabled ? "true" : "false") << ","
           << "\"in_head_enabled\":" << (spat.in_head_enabled ? "true" : "false") << ","
           << "\"auto_pan_enabled\":" << (spat.auto_pan_enabled ? "true" : "false") << ","
           << "\"monitor_enabled\":" << (spat.monitor_enabled ? "true" : "false") << ","
           << "\"target_name\":\"" << target.target_name << "\","
           << "\"target_active\":" << (target.is_active ? "true" : "false") << ","
           << "\"target_pan\":" << target.pan_azimuth << ","
           << "\"target_width\":" << target.stereo_wideness << ","
           << "\"target_above_head\":" << (target.above_head_detected ? "true" : "false") << ","
           << "\"target_reverb_index\":" << target.reverb_index << ","
           << "\"target_reverb_detected\":" << (target.reverb_detected ? "true" : "false") << ","
           << "\"target_rms_dbfs\":" << target.target_rms_dbfs << ","
           << "\"target_trick\":\"" << target.detected_trick_summary << "\","
           << "\"counter_action\":\"" << target.counter_action_summary << "\","
           << "\"pierce_engaged\":" << (target.pierce_engaged ? "true" : "false") << ","
           << "\"auto_counter\":" << (target.auto_counter_enabled ? "true" : "false") << ","
           << "\"waveform\":[";
        for (size_t i = 0; i < wf.size(); ++i) {
            ss << wf[i] << (i + 1 < wf.size() ? "," : "");
        }
        ss << "]}";

        return ss.str();
    }

    static std::string build_devices_json() {
        if (!s_dev_mgr) return "{\"capture\":[],\"playback\":[]}";
        s_dev_mgr->refresh_devices();
        const auto &caps = s_dev_mgr->get_capture_devices();
        const auto &plays = s_dev_mgr->get_playback_devices();

        std::stringstream ss;
        ss << "{\"capture\":[";
        for (size_t i = 0; i < caps.size(); ++i) {
            ss << "{\"index\":" << caps[i].index << ",\"name\":\"" << caps[i].name << "\",\"default\":" << (caps[i].is_default ? "true" : "false") << "}"
               << (i + 1 < caps.size() ? "," : "");
        }
        ss << "],\"playback\":[";
        for (size_t i = 0; i < plays.size(); ++i) {
            ss << "{\"index\":" << plays[i].index << ",\"name\":\"" << plays[i].name << "\",\"default\":" << (plays[i].is_default ? "true" : "false") << "}"
               << (i + 1 < plays.size() ? "," : "");
        }
        ss << "]}";
        return ss.str();
    }

    static void handle_set_param(const std::string &query) {
        if (!s_pipeline) return;
        size_t eq = query.find('=');
        if (eq == std::string::npos) return;
        std::string key = query.substr(0, eq);
        float val = static_cast<float>(std::atof(query.substr(eq + 1).c_str()));

        if (key == "input_gain") s_pipeline->input_gain().set_gain_db(val);
        else if (key == "target_lufs") s_pipeline->normalizer().set_target_lufs(val);
        else if (key == "max_boost") s_pipeline->normalizer().set_max_boost_db(val);
        else if (key == "gate_thresh") s_pipeline->gate().set_threshold_db(val);
        else if (key == "gate_ratio") s_pipeline->gate().set_ratio(val);
        else if (key == "hpf_freq") s_pipeline->eq().set_band(0, FilterType::HighPass, val, 0.0f, 0.7071f);
        else if (key == "mud_gain") s_pipeline->eq().set_band(2, FilterType::Peaking, 400.0f, val, 1.4f);
        else if (key == "presence_gain") s_pipeline->eq().set_band(4, FilterType::Peaking, 3200.0f, val, 1.5f);
        else if (key == "comp_thresh") s_pipeline->compressor().set_threshold_db(val);
        else if (key == "comp_ratio") s_pipeline->compressor().set_ratio(val);
        else if (key == "comp_makeup") s_pipeline->compressor().set_makeup_gain_db(val);
        else if (key == "lim_ceiling") s_pipeline->limiter().set_ceiling_db(val);
        else if (key == "lim_rel") s_pipeline->limiter().set_release_ms(val);
        else if (key == "pan") {
            auto cfg = s_pipeline->get_spatial_config();
            cfg.pan = val;
            s_pipeline->set_spatial_config(cfg);
        }
        else if (key == "stereo_width") {
            auto cfg = s_pipeline->get_spatial_config();
            cfg.stereo_width = val;
            s_pipeline->set_spatial_config(cfg);
        }
        else if (key == "elevation") {
            auto cfg = s_pipeline->get_spatial_config();
            cfg.elevation_enabled = (val > 0.5f);
            s_pipeline->set_spatial_config(cfg);
        }
        else if (key == "in_head") {
            auto cfg = s_pipeline->get_spatial_config();
            cfg.in_head_enabled = (val > 0.5f);
            s_pipeline->set_spatial_config(cfg);
        }
        else if (key == "autopan") {
            auto cfg = s_pipeline->get_spatial_config();
            cfg.auto_pan_enabled = (val > 0.5f);
            s_pipeline->set_spatial_config(cfg);
        }
        else if (key == "autopan_speed") {
            auto cfg = s_pipeline->get_spatial_config();
            cfg.auto_pan_speed = val;
            s_pipeline->set_spatial_config(cfg);
        }
        else if (key == "monitor") {
            auto cfg = s_pipeline->get_spatial_config();
            cfg.monitor_enabled = (val > 0.5f);
            s_pipeline->set_spatial_config(cfg);
        }
        else if (key == "target_name") {
            s_pipeline->tracker().set_target_name(query.substr(eq + 1));
        }
        else if (key == "auto_counter") {
            s_pipeline->tracker().set_auto_counter(val > 0.5f);
        }
        else if (key == "force_pierce") {
            s_pipeline->tracker().force_pierce(val > 0.5f);
        }
    }

    static DWORD WINAPI server_thread_proc(LPVOID) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) {
            WSACleanup();
            return 1;
        }

        int opt = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(48200);

        if (bind(s, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(s);
            WSACleanup();
            return 0; // Already running
        }

        if (listen(s, 16) == SOCKET_ERROR) {
            closesocket(s);
            WSACleanup();
            return 1;
        }

        while (true) {
            SOCKET client = accept(s, NULL, NULL);
            if (client == INVALID_SOCKET) break;

            char reqBuf[2048] = {0};
            int recvd = recv(client, reqBuf, sizeof(reqBuf) - 1, 0);
            if (recvd > 0) {
                std::string req(reqBuf);
                std::string method, path;
                std::istringstream iss(req);
                iss >> method >> path;

                std::string respBody;
                std::string contentType = "text/plain";

                if (path == "/" || path == "/index.html") {
                    respBody = G_UI_HTML;
                    contentType = "text/html; charset=utf-8";
                } else if (path == "/telemetry") {
                    respBody = build_telemetry_json();
                    contentType = "application/json";
                } else if (path.find("/set?") == 0) {
                    handle_set_param(path.substr(5));
                    respBody = "{\"ok\":true}";
                    contentType = "application/json";
                } else if (path == "/devices") {
                    respBody = build_devices_json();
                    contentType = "application/json";
                } else if (path.find("/select_device?") == 0) {
                    int cap = -1, play = -1;
                    std::string q = path.substr(15);
                    size_t c_pos = q.find("cap=");
                    if (c_pos != std::string::npos) cap = std::atoi(q.c_str() + c_pos + 4);
                    size_t p_pos = q.find("play=");
                    if (p_pos != std::string::npos) play = std::atoi(q.c_str() + p_pos + 5);
                    if (s_dev_mgr) s_dev_mgr->start(cap, play);
                    respBody = "{\"ok\":true}";
                    contentType = "application/json";
                } else if (path.find("/preset?name=") == 0) {
                    std::string name = path.substr(13);
                    if (s_pipeline) {
                        if (name == "broadcast") s_pipeline->load_preset(get_preset_broadcast());
                        else if (name == "intelligibility") s_pipeline->load_preset(get_preset_intelligibility());
                        else if (name == "max_loudness") s_pipeline->load_preset(get_preset_maximum_loudness());
                        else if (name == "gaming") s_pipeline->load_preset(get_preset_gaming());
                        else if (name == "bypass") s_pipeline->load_preset(get_preset_bypass());
                    }
                    respBody = "{\"ok\":true}";
                    contentType = "application/json";
                } else if (path == "/toggle_ab") {
                    if (s_pipeline) s_pipeline->toggle_ab_comparison();
                    respBody = "{\"ok\":true}";
                    contentType = "application/json";
                } else if (path == "/run_benchmark") {
                    auto results = BenchmarkSuite::run_full_benchmark();
                    std::string md = BenchmarkSuite::generate_report_markdown(results);
                    // JSON escape
                    std::string escaped_md;
                    for (char c : md) {
                        if (c == '"') escaped_md += "\\\"";
                        else if (c == '\\') escaped_md += "\\\\";
                        else if (c == '\n') escaped_md += "\\n";
                        else if (c == '\r') continue;
                        else escaped_md += c;
                    }
                    respBody = "{\"report\":\"" + escaped_md + "\"}";
                    contentType = "application/json";
                } else {
                    respBody = "404 Not Found";
                }

                char header[512];
                snprintf(header, sizeof(header),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %zu\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Connection: close\r\n\r\n",
                    contentType.c_str(), respBody.size()
                );

                send(client, header, static_cast<int>(strlen(header)), 0);
                send(client, respBody.c_str(), static_cast<int>(respBody.size()), 0);
            }
            closesocket(client);
        }

        closesocket(s);
        WSACleanup();
        return 0;
    }
};

} // namespace splcord
