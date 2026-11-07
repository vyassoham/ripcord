#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <cmath>
#include <sstream>
#include <algorithm>

#include "audio_pipeline.hpp"
#include "audio_device.hpp"

namespace splcord {

using namespace Gdiplus;

class NativeGUI {
public:
    static NativeGUI& instance() {
        static NativeGUI s_inst;
        return s_inst;
    }

    bool init(HINSTANCE hInstance, AudioPipeline *pipeline, AudioDeviceManager *dev_mgr) {
        m_hInst = hInstance;
        m_pipeline = pipeline;
        m_dev_mgr = dev_mgr;

        // Initialize GDI+
        GdiplusStartupInput gdiplusStartupInput;
        if (GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL) != Ok) {
            return false;
        }

        // Register Window Class
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = s_WndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = L"SPLCORD_NATIVE_GUI_CLASS";

        RegisterClassExW(&wc);

        // Window Size
        const int winW = 860;
        const int winH = 640;

        RECT rc = { 0, 0, winW, winH };
        AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int posX = (screenW - (rc.right - rc.left)) / 2;
        int posY = (screenH - (rc.bottom - rc.top)) / 2;

        m_hwnd = CreateWindowExW(
            WS_EX_APPWINDOW,
            wc.lpszClassName,
            L"SPLCORD // C++ STUDIO VOICE & TARGET VC RADAR",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
            posX, posY, rc.right - rc.left, rc.bottom - rc.top,
            NULL, NULL, hInstance, this
        );

        if (!m_hwnd) return false;

        // Create Edit control for Target Name
        m_hwndEditTarget = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"EnemyVC",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            445, 92, 230, 24,
            m_hwnd, (HMENU)1001, hInstance, NULL
        );

        HFONT hFont = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SendMessageW(m_hwndEditTarget, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Start 30 FPS Render Timer (33 ms)
        SetTimer(m_hwnd, 1, 33, NULL);

        return true;
    }

    void run_message_loop() {
        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void shutdown() {
        if (m_hwnd) {
            KillTimer(m_hwnd, 1);
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
        if (m_gdiplusToken) {
            GdiplusShutdown(m_gdiplusToken);
            m_gdiplusToken = 0;
        }
    }

private:
    NativeGUI() = default;

    HINSTANCE m_hInst = nullptr;
    HWND m_hwnd = nullptr;
    HWND m_hwndEditTarget = nullptr;
    ULONG_PTR m_gdiplusToken = 0;

    AudioPipeline *m_pipeline = nullptr;
    AudioDeviceManager *m_dev_mgr = nullptr;

    // Mouse Interaction State
    bool m_dragging_pan = false;
    bool m_dragging_width = false;
    POINT m_last_mouse = { 0, 0 };

    // Layout Rectangles
    RECT m_rc_knob = { 45, 95, 135, 185 }; // Center (90, 140), r=45
    RECT m_rc_snap_l100 = { 150, 105, 195, 129 };
    RECT m_rc_snap_l50  = { 200, 105, 245, 129 };
    RECT m_rc_snap_ctr  = { 250, 105, 295, 129 };
    RECT m_rc_snap_r50  = { 300, 105, 345, 129 };
    RECT m_rc_snap_r100 = { 350, 105, 395, 129 };

    RECT m_rc_btn_orbit = { 150, 138, 268, 166 };
    RECT m_rc_btn_hearmic = { 277, 138, 395, 166 };

    RECT m_rc_slider_width = { 30, 222, 395, 234 };

    RECT m_rc_btn_elevation = { 30, 252, 205, 280 };
    RECT m_rc_btn_inhead    = { 220, 252, 395, 280 };

    // Presets
    RECT m_rc_p_broad = { 30, 360, 115, 388 };
    RECT m_rc_p_radio = { 125, 360, 210, 388 };
    RECT m_rc_p_max   = { 220, 360, 305, 388 };
    RECT m_rc_p_game  = { 315, 360, 395, 388 };
    RECT m_rc_p_ab    = { 30, 396, 210, 424 };
    RECT m_rc_p_raw   = { 220, 396, 395, 424 };

    // Target VC Radar Controls
    RECT m_rc_btn_lock_target = { 685, 92, 755, 116 };
    RECT m_rc_btn_auto_pierce = { 445, 266, 615, 296 };
    RECT m_rc_btn_force_pierce = { 625, 266, 795, 296 };

    static LRESULT CALLBACK s_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_NCCREATE) {
            auto *cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        }
        auto *gui = reinterpret_cast<NativeGUI*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (gui) {
            return gui->handle_message(hwnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT handle_message(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_TIMER:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            render_double_buffered(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            m_last_mouse = pt;

            if (PtInRect(&m_rc_knob, pt)) {
                m_dragging_pan = true;
                SetCapture(hwnd);
                return 0;
            }
            if (PtInRect(&m_rc_slider_width, pt)) {
                m_dragging_width = true;
                SetCapture(hwnd);
                float w = std::clamp((float)(pt.x - m_rc_slider_width.left) / (m_rc_slider_width.right - m_rc_slider_width.left) * 3.0f, 0.0f, 3.0f);
                if (m_pipeline) {
                    auto cfg = m_pipeline->get_spatial_config();
                    cfg.stereo_width = w;
                    m_pipeline->set_spatial_config(cfg);
                }
                return 0;
            }

            // Snap Chips
            if (PtInRect(&m_rc_snap_l100, pt)) set_pan(-1.0f);
            else if (PtInRect(&m_rc_snap_l50, pt)) set_pan(-0.5f);
            else if (PtInRect(&m_rc_snap_ctr, pt)) set_pan(0.0f);
            else if (PtInRect(&m_rc_snap_r50, pt)) set_pan(0.5f);
            else if (PtInRect(&m_rc_snap_r100, pt)) set_pan(1.0f);

            // Toggles
            else if (PtInRect(&m_rc_btn_orbit, pt)) {
                if (m_pipeline) {
                    auto cfg = m_pipeline->get_spatial_config();
                    cfg.auto_pan_enabled = !cfg.auto_pan_enabled;
                    m_pipeline->set_spatial_config(cfg);
                }
            }
            else if (PtInRect(&m_rc_btn_hearmic, pt)) {
                if (m_pipeline) {
                    auto cfg = m_pipeline->get_spatial_config();
                    cfg.monitor_enabled = !cfg.monitor_enabled;
                    m_pipeline->set_spatial_config(cfg);
                }
            }
            else if (PtInRect(&m_rc_btn_elevation, pt)) {
                if (m_pipeline) {
                    auto cfg = m_pipeline->get_spatial_config();
                    cfg.elevation_enabled = !cfg.elevation_enabled;
                    m_pipeline->set_spatial_config(cfg);
                }
            }
            else if (PtInRect(&m_rc_btn_inhead, pt)) {
                if (m_pipeline) {
                    auto cfg = m_pipeline->get_spatial_config();
                    cfg.in_head_enabled = !cfg.in_head_enabled;
                    m_pipeline->set_spatial_config(cfg);
                }
            }

            // Presets
            else if (PtInRect(&m_rc_p_broad, pt)) { if (m_pipeline) m_pipeline->load_preset(get_preset_broadcast()); }
            else if (PtInRect(&m_rc_p_radio, pt)) { if (m_pipeline) m_pipeline->load_preset(get_preset_intelligibility()); }
            else if (PtInRect(&m_rc_p_max, pt))   { if (m_pipeline) m_pipeline->load_preset(get_preset_maximum_loudness()); }
            else if (PtInRect(&m_rc_p_game, pt))  { if (m_pipeline) m_pipeline->load_preset(get_preset_gaming()); }
            else if (PtInRect(&m_rc_p_ab, pt))    { if (m_pipeline) m_pipeline->toggle_ab_comparison(); }
            else if (PtInRect(&m_rc_p_raw, pt))   { if (m_pipeline) m_pipeline->load_preset(get_preset_bypass()); }

            // Target VC Buttons
            else if (PtInRect(&m_rc_btn_lock_target, pt)) {
                wchar_t buf[128] = { 0 };
                GetWindowTextW(m_hwndEditTarget, buf, 127);
                char narrow[128] = { 0 };
                WideCharToMultiByte(CP_UTF8, 0, buf, -1, narrow, 127, NULL, NULL);
                if (m_pipeline) m_pipeline->tracker().set_target_name(narrow);
            }
            else if (PtInRect(&m_rc_btn_auto_pierce, pt)) {
                if (m_pipeline) {
                    auto m = m_pipeline->tracker().get_metrics();
                    m_pipeline->tracker().set_auto_counter(!m.auto_counter_enabled);
                }
            }
            else if (PtInRect(&m_rc_btn_force_pierce, pt)) {
                if (m_pipeline) {
                    auto m = m_pipeline->tracker().get_metrics();
                    m_pipeline->tracker().force_pierce(!m.pierce_engaged);
                }
            }

            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (m_dragging_pan) {
                int dy = m_last_mouse.y - GET_Y_LPARAM(lParam);
                m_last_mouse.x = GET_X_LPARAM(lParam);
                m_last_mouse.y = GET_Y_LPARAM(lParam);
                if (m_pipeline) {
                    auto cfg = m_pipeline->get_spatial_config();
                    cfg.pan = std::clamp(cfg.pan + dy * 0.015f, -1.0f, 1.0f);
                    m_pipeline->set_spatial_config(cfg);
                }
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (m_dragging_width) {
                int px = GET_X_LPARAM(lParam);
                float w = std::clamp((float)(px - m_rc_slider_width.left) / (m_rc_slider_width.right - m_rc_slider_width.left) * 3.0f, 0.0f, 3.0f);
                if (m_pipeline) {
                    auto cfg = m_pipeline->get_spatial_config();
                    cfg.stereo_width = w;
                    m_pipeline->set_spatial_config(cfg);
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (m_dragging_pan || m_dragging_width) {
                m_dragging_pan = false;
                m_dragging_width = false;
                ReleaseCapture();
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (m_pipeline) {
                auto cfg = m_pipeline->get_spatial_config();
                cfg.pan = std::clamp(cfg.pan + (delta > 0 ? 0.05f : -0.05f), -1.0f, 1.0f);
                m_pipeline->set_spatial_config(cfg);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (PtInRect(&m_rc_knob, pt)) {
                set_pan(0.0f);
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_SPACE) {
                if (m_pipeline) m_pipeline->toggle_ab_comparison();
            } else if (wParam == 'P' || wParam == 'p') {
                if (m_pipeline) {
                    auto m = m_pipeline->tracker().get_metrics();
                    m_pipeline->tracker().force_pierce(!m.pierce_engaged);
                }
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void set_pan(float pan) {
        if (m_pipeline) {
            auto cfg = m_pipeline->get_spatial_config();
            cfg.pan = std::clamp(pan, -1.0f, 1.0f);
            m_pipeline->set_spatial_config(cfg);
        }
    }

    void render_double_buffered(HDC hdc) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        Graphics g(memDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

        // Background
        SolidBrush bgBrush(Color(255, 10, 13, 20)); // Obsidian dark
        g.FillRectangle(&bgBrush, 0, 0, w, h);

        // Fetch Live States
        SpatialConfig spat = m_pipeline ? m_pipeline->get_spatial_config() : SpatialConfig{};
        LoudnessStats stats = m_pipeline ? m_pipeline->meter().get_stats() : LoudnessStats{};
        TargetAcousticMetrics target = m_pipeline ? m_pipeline->tracker().get_metrics() : TargetAcousticMetrics{};
        float gr = m_pipeline ? m_pipeline->compressor().get_gain_reduction_db() : 0.0f;
        float lat = m_pipeline ? m_pipeline->get_latency_ms() : 2.5f;

        // --- 1. HEADER ---
        Font fontTitle(L"Segoe UI", 12, FontStyleBold);
        Font fontSubtitle(L"Segoe UI", 8, FontStyleRegular);
        Font fontBold(L"Segoe UI", 9, FontStyleBold);
        Font fontRegular(L"Segoe UI", 9, FontStyleRegular);
        Font fontSmall(L"Segoe UI", 8, FontStyleBold);

        SolidBrush whiteBrush(Color(255, 248, 250, 252));
        SolidBrush cyanBrush(Color(255, 0, 229, 255));
        SolidBrush emeraldBrush(Color(255, 16, 185, 129));
        SolidBrush redBrush(Color(255, 239, 68, 68));
        SolidBrush goldBrush(Color(255, 245, 158, 11));
        SolidBrush grayBrush(Color(255, 148, 163, 184));

        // Header LED dot
        g.FillEllipse(&emeraldBrush, 16, 18, 10, 10);
        g.DrawString(L"SPLCORD // NATIVE C++ VOICE ENGINE & VC RADAR", -1, &fontTitle, PointF(34.0f, 13.0f), &whiteBrush);
        g.DrawString(L"WASAPI Duplex Direct • Strict 0 dBFS Peak Ceiling • Loopback Target Radar", -1, &fontSubtitle, PointF(34.0f, 32.0f), &grayBrush);

        // Header Badges
        draw_badge(g, L"48kHz STEREO", 520, 16, Color(255, 16, 185, 129));
        wchar_t latBuf[32];
        swprintf_s(latBuf, L"LATENCY: %.2f ms", lat);
        draw_badge(g, latBuf, 625, 16, Color(255, 0, 229, 255));
        draw_badge(g, L"CLIP: 0.000%", 750, 16, Color(255, 16, 185, 129));

        // Separator Line
        Pen sepPen(Color(40, 255, 255, 255), 1.0f);
        g.DrawLine(&sepPen, 0, 52, w, 52);

        // --- 2. LEFT CARD: 3D SPATIAL STEREO & ROTARY PANNING ---
        draw_card_frame(g, 16, 60, 395, 240, L"🎧 3D SPATIAL STEREO & PANNING KNOB", Color(255, 0, 229, 255));

        // Rotary Knob
        draw_rotary_knob(g, 90, 140, 42, spat.pan);

        // Digital Pan Readout
        wchar_t panReadout[32];
        if (std::abs(spat.pan) < 0.03f) swprintf_s(panReadout, L"• CENTER (0%)");
        else if (spat.pan < 0) swprintf_s(panReadout, L"◀ %d%% LEFT", (int)(-spat.pan * 100));
        else swprintf_s(panReadout, L"%d%% RIGHT ▶", (int)(spat.pan * 100));
        g.DrawString(panReadout, -1, &fontBold, PointF(50.0f, 188.0f), &cyanBrush);

        // Snap Chips
        draw_button(g, m_rc_snap_l100, L"100%L", false);
        draw_button(g, m_rc_snap_l50,  L"50%L", false);
        draw_button(g, m_rc_snap_ctr,  L"• CTR", std::abs(spat.pan) < 0.03f);
        draw_button(g, m_rc_snap_r50,  L"50%R", false);
        draw_button(g, m_rc_snap_r100, L"100%R", false);

        // Orbit & Monitor Buttons
        draw_button(g, m_rc_btn_orbit, spat.auto_pan_enabled ? L"8D Orbit [ON]" : L"8D Orbit [OFF]", spat.auto_pan_enabled, Color(255, 0, 229, 255));
        draw_button(g, m_rc_btn_hearmic, spat.monitor_enabled ? L"Hear Mic [ON]" : L"Hear Mic [OFF]", spat.monitor_enabled, Color(255, 245, 158, 11));

        // Double Wideness Slider
        wchar_t widthTxt[48];
        swprintf_s(widthTxt, L"Stereo Field // Double Haas: %d%% (%s)", (int)(spat.stereo_width * 100),
            spat.stereo_width >= 2.0f ? L"2X Wide" : (spat.stereo_width <= 0.2f ? L"Mono" : L"Standard"));
        g.DrawString(widthTxt, -1, &fontSmall, PointF(30.0f, 206.0f), &whiteBrush);
        draw_slider(g, m_rc_slider_width, spat.stereo_width / 3.0f, Color(255, 0, 229, 255));

        // Toggles
        draw_button(g, m_rc_btn_elevation, spat.elevation_enabled ? L"▲ Above Head [ON]" : L"▲ Above Head [OFF]", spat.elevation_enabled, Color(255, 239, 68, 68));
        draw_button(g, m_rc_btn_inhead, spat.in_head_enabled ? L"• In-Head Cross [ON]" : L"• In-Head Cross [OFF]", spat.in_head_enabled, Color(255, 16, 185, 129));

        // --- 3. LEFT LOWER CARD: PRESETS & BROADCAST MASTERING ---
        draw_card_frame(g, 16, 310, 395, 195, L"💾 STUDIO PRESETS & LOUDNESS MASTER", Color(255, 16, 185, 129));
        draw_button(g, m_rc_p_broad, L"Broadcast", true);
        draw_button(g, m_rc_p_radio, L"High Radio", false);
        draw_button(g, m_rc_p_max,   L"Max Loud", false);
        draw_button(g, m_rc_p_game,  L"Gaming", false);
        draw_button(g, m_rc_p_ab,    L"⇄ A / B COMPARE", false, Color(255, 168, 85, 247));
        draw_button(g, m_rc_p_raw,   L"BYPASS (RAW)", false);

        // DSP Stats Summary
        wchar_t dspSummary[128];
        swprintf_s(dspSummary, L"Master Limiter: -0.10 dBTP Brickwall • Compressor: 4:1 Log-Knee\nNormalizer: -9.6 LUFS Dense • Formant Filter: 3.2kHz Knife");
        g.DrawString(dspSummary, -1, &fontSubtitle, PointF(30.0f, 435.0f), &grayBrush);

        // --- 4. RIGHT TOP CARD: TARGET VC RADAR & AUTO-COUNTER ---
        draw_card_frame(g, 425, 60, 415, 240, L"🎯 TARGET VC RADAR & AUTO-PIERCE COUNTER", Color(255, 239, 68, 68));
        g.DrawString(L"Target VC User / Handle:", -1, &fontSmall, PointF(445.0f, 75.0f), &grayBrush);
        draw_button(g, m_rc_btn_lock_target, L"LOCK", false, Color(255, 239, 68, 68));

        // Target Status Box
        RectF rcTgtBox(445.0f, 122.0f, 375.0f, 134.0f);
        SolidBrush tgtBg(Color(120, 0, 0, 0));
        g.FillRectangle(&tgtBg, rcTgtBox);
        Pen tgtPen(Color(50, 239, 68, 68), 1.0f);
        g.DrawRectangle(&tgtPen, rcTgtBox);

        // Status lines
        wchar_t tAzimuth[48];
        if (std::abs(target.pan_azimuth) < 0.15f) swprintf_s(tAzimuth, L"Target Azimuth: CENTER (0%%)");
        else if (target.pan_azimuth < 0) swprintf_s(tAzimuth, L"Target Azimuth: %d%% LEFT ◀", (int)(-target.pan_azimuth * 100));
        else swprintf_s(tAzimuth, L"Target Azimuth: %d%% RIGHT ▶", (int)(target.pan_azimuth * 100));
        g.DrawString(tAzimuth, -1, &fontSmall, PointF(455.0f, 128.0f), &cyanBrush);

        wchar_t tWidthStr[48];
        swprintf_s(tWidthStr, L"Stereo Field: %s (%d%%)", target.stereo_wideness > 1.8f ? L"DOUBLE WIDENESS" : L"Normal", (int)(target.stereo_wideness * 100));
        g.DrawString(tWidthStr, -1, &fontSmall, PointF(455.0f, 146.0f), &goldBrush);

        wchar_t tElevStr[48];
        swprintf_s(tElevStr, L"Above Head: %s", target.above_head_detected ? L"DETECTED! (7.8kHz Notch Active)" : L"NONE");
        SolidBrush elevBrush(target.above_head_detected ? Color(255, 239, 68, 68) : Color(255, 148, 163, 184));
        g.DrawString(tElevStr, -1, &fontSmall, PointF(455.0f, 164.0f), &elevBrush);

        // Reverb bar
        wchar_t tRevStr[48];
        swprintf_s(tRevStr, L"Reverb Diffusion: %d%% %s", (int)(target.reverb_index * 100), target.reverb_detected ? L"(REVERB ALERT!)" : L"(DRY)");
        SolidBrush revBrush(target.reverb_detected ? Color(255, 239, 68, 68) : Color(255, 148, 163, 184));
        g.DrawString(tRevStr, -1, &fontSmall, PointF(455.0f, 182.0f), &revBrush);

        RECT rcRevBar = { 455, 200, 805, 208 };
        draw_bar(g, rcRevBar, target.reverb_index, Color(255, 239, 68, 68));

        // Trick summary
        wchar_t wTrick[128] = { 0 };
        MultiByteToWideChar(CP_UTF8, 0, target.detected_trick_summary.c_str(), -1, wTrick, 127);
        g.DrawString(wTrick, -1, &fontSubtitle, PointF(455.0f, 215.0f), &whiteBrush);

        wchar_t wCounter[128] = { 0 };
        MultiByteToWideChar(CP_UTF8, 0, target.counter_action_summary.c_str(), -1, wCounter, 127);
        SolidBrush emeraldLight(Color(255, 52, 211, 153));
        g.DrawString(wCounter, -1, &fontSubtitle, PointF(455.0f, 235.0f), &emeraldLight);

        // Auto-Pierce and Force Pierce buttons
        draw_button(g, m_rc_btn_auto_pierce, target.auto_counter_enabled ? L"⚡ AUTO-PIERCE [ON]" : L"⚡ AUTO-PIERCE [OFF]", target.auto_counter_enabled, Color(255, 239, 68, 68));
        draw_button(g, m_rc_btn_force_pierce, target.pierce_engaged ? L"⚔️ PIERCE [ACTIVE]" : L"⚔️ FORCE PIERCE", target.pierce_engaged, Color(255, 239, 68, 68));

        // --- 5. RIGHT LOWER CARD: OBJECTIVE ITU-R BS.1770-4 METERS ---
        draw_card_frame(g, 425, 310, 415, 195, L"📊 OBJECTIVE ITU-R BS.1770-4 METERS", Color(255, 0, 229, 255));

        auto to_pct = [](float db, float min_db = -60.0f, float max_db = 0.0f) {
            return std::clamp((db - min_db) / (max_db - min_db), 0.0f, 1.0f);
        };

        draw_meter_row(g, 445, 340, L"Momentary:", stats.momentary_lufs, to_pct(stats.momentary_lufs), L"LUFS", Color(255, 16, 185, 129));
        draw_meter_row(g, 445, 365, L"Short-Term:", stats.short_term_lufs, to_pct(stats.short_term_lufs), L"LUFS", Color(255, 16, 185, 129));
        draw_meter_row(g, 445, 390, L"Integrated:", stats.integrated_lufs, to_pct(stats.integrated_lufs), L"LUFS", Color(255, 0, 229, 255));
        draw_meter_row(g, 445, 415, L"True-Peak:", stats.true_peak_dbtp, to_pct(stats.true_peak_dbtp), L"dBTP", Color(255, 245, 158, 11));
        draw_meter_row(g, 445, 440, L"Comp Redux:", gr, to_pct(-gr, -20.0f, 0.0f), L"dB", Color(255, 239, 68, 68));

        // --- 6. BOTTOM OSCILLOSCOPE WAVEFORM ---
        RectF rcWfBox(16.0f, 515.0f, (float)(w - 32), 85.0f);
        SolidBrush wfBg(Color(255, 6, 8, 12));
        g.FillRectangle(&wfBg, rcWfBox);
        Pen wfBoxPen(Color(35, 255, 255, 255), 1.0f);
        g.DrawRectangle(&wfBoxPen, rcWfBox);

        // Center line
        Pen centerPen(Color(25, 255, 255, 255), 1.0f);
        float cy = rcWfBox.Y + rcWfBox.Height * 0.5f;
        g.DrawLine(&centerPen, rcWfBox.X, cy, rcWfBox.X + rcWfBox.Width, cy);

        // Draw waveform points
        std::vector<float> wf(128, 0.0f);
        if (m_dev_mgr) {
            m_dev_mgr->get_latest_waveform(wf.data(), wf.size());
        }

        std::vector<PointF> pts(wf.size());
        float stepX = rcWfBox.Width / (wf.size() - 1);
        for (size_t i = 0; i < wf.size(); ++i) {
            pts[i].X = rcWfBox.X + i * stepX;
            pts[i].Y = cy + wf[i] * (rcWfBox.Height * 0.45f);
        }

        Pen wfPen(Color(255, 0, 229, 255), 1.5f);
        g.DrawLines(&wfPen, pts.data(), static_cast<INT>(pts.size()));

        g.DrawString(L"REAL-TIME 48KHZ STEREO OSCILLOSCOPE", -1, &fontSmall, PointF(24.0f, 520.0f), &grayBrush);

        // Footer Hotkeys
        g.DrawString(L"ENGINE: SPLCORD Native C++ • Pure Windows WASAPI • Space = A/B Toggle • P = Force Pierce • M = Mute", -1, &fontSubtitle, PointF(16.0f, 608.0f), &grayBrush);

        // Blit backbuffer
        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
    }

    void draw_card_frame(Graphics &g, int x, int y, int w, int h, const wchar_t *title, Color color) {
        RectF r((float)x, (float)y, (float)w, (float)h);
        SolidBrush brush(Color(220, 15, 20, 30));
        g.FillRectangle(&brush, r);
        Pen borderPen(Color(45, 255, 255, 255), 1.0f);
        g.DrawRectangle(&borderPen, r);

        Font font(L"Segoe UI", 9, FontStyleBold);
        SolidBrush textBrush(color);
        g.DrawString(title, -1, &font, PointF((float)(x + 12), (float)(y + 10)), &textBrush);
    }

    void draw_badge(Graphics &g, const wchar_t *text, int x, int y, Color color) {
        Font font(L"Segoe UI", 8, FontStyleBold);
        RectF r((float)x, (float)y, 95.0f, 20.0f);
        SolidBrush bg(Color(35, color.GetR(), color.GetG(), color.GetB()));
        g.FillRectangle(&bg, r);
        Pen borderPen(Color(120, color.GetR(), color.GetG(), color.GetB()), 1.0f);
        g.DrawRectangle(&borderPen, r);

        SolidBrush textBrush(color);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(text, -1, &font, r, &sf, &textBrush);
    }

    void draw_button(Graphics &g, RECT r, const wchar_t *text, bool active, Color activeColor = Color(255, 0, 229, 255)) {
        RectF rf((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top));
        if (active) {
            SolidBrush bg(activeColor);
            g.FillRectangle(&bg, rf);
            SolidBrush txt(Color(255, 8, 10, 15));
            Font font(L"Segoe UI", 8, FontStyleBold);
            StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
            g.DrawString(text, -1, &font, rf, &sf, &txt);
        } else {
            SolidBrush bg(Color(200, 25, 30, 44));
            g.FillRectangle(&bg, rf);
            Pen pen(Color(50, 255, 255, 255), 1.0f);
            g.DrawRectangle(&pen, rf);
            SolidBrush txt(Color(255, 240, 240, 240));
            Font font(L"Segoe UI", 8, FontStyleBold);
            StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
            g.DrawString(text, -1, &font, rf, &sf, &txt);
        }
    }

    void draw_slider(Graphics &g, RECT r, float normVal, Color color) {
        float x = (float)r.left;
        float y = (float)r.top;
        float w = (float)(r.right - r.left);
        float h = (float)(r.bottom - r.top);

        SolidBrush trackBg(Color(255, 20, 25, 35));
        g.FillRectangle(&trackBg, x, y, w, h);

        float fillW = w * std::clamp(normVal, 0.0f, 1.0f);
        SolidBrush fillBg(color);
        g.FillRectangle(&fillBg, x, y, fillW, h);

        // Thumb
        SolidBrush thumb(Color(255, 255, 255, 255));
        g.FillEllipse(&thumb, x + fillW - 6.0f, y - 3.0f, 12.0f, 12.0f);
    }

    void draw_bar(Graphics &g, RECT r, float normVal, Color color) {
        float x = (float)r.left;
        float y = (float)r.top;
        float w = (float)(r.right - r.left);
        float h = (float)(r.bottom - r.top);

        SolidBrush trackBg(Color(255, 15, 20, 30));
        g.FillRectangle(&trackBg, x, y, w, h);

        float fillW = w * std::clamp(normVal, 0.0f, 1.0f);
        SolidBrush fillBg(color);
        g.FillRectangle(&fillBg, x, y, fillW, h);
    }

    void draw_rotary_knob(Graphics &g, int cx, int cy, int r, float pan) {
        // Outer glow
        Pen glowPen(Color(40, 0, 229, 255), 2.0f);
        g.DrawEllipse(&glowPen, cx - r - 2, cy - r - 2, (r + 2) * 2, (r + 2) * 2);

        // Body
        SolidBrush body(Color(255, 18, 24, 36));
        g.FillEllipse(&body, cx - r, cy - r, r * 2, r * 2);

        Pen rimPen(Color(120, 0, 229, 255), 1.5f);
        g.DrawEllipse(&rimPen, cx - r, cy - r, r * 2, r * 2);

        // Needle angle (-135 to +135 deg)
        float deg = pan * 135.0f;
        float rad = (deg - 90.0f) * (PI_F / 180.0f);

        float nx = cx + std::cos(rad) * (r * 0.75f);
        float ny = cy + std::sin(rad) * (r * 0.75f);

        Pen needlePen(Color(255, 0, 229, 255), 3.0f);
        g.DrawLine(&needlePen, (float)cx, (float)cy, nx, ny);

        SolidBrush centerDot(Color(255, 255, 255, 255));
        g.FillEllipse(&centerDot, cx - 4, cy - 4, 8, 8);
    }

    void draw_meter_row(Graphics &g, int x, int y, const wchar_t *label, float val, float norm, const wchar_t *unit, Color color) {
        Font font(L"Segoe UI", 8, FontStyleBold);
        SolidBrush gray(Color(255, 148, 163, 184));
        g.DrawString(label, -1, &font, PointF((float)x, (float)y), &gray);

        RECT rBar = { x + 85, y + 2, x + 310, y + 10 };
        draw_bar(g, rBar, norm, color);

        wchar_t valBuf[32];
        swprintf_s(valBuf, L"%.1f %s", val, unit);
        SolidBrush txt(Color(255, 240, 240, 240));
        g.DrawString(valBuf, -1, &font, PointF((float)(x + 320), (float)y), &txt);
    }
};

} // namespace splcord
