"""
Ripcord / Discord Global Voice Channel Region Rotator — Next-Gen Desktop GUI Panel
Features:
- Live Ripcord Auto-Sync (Token, Server ID, Voice Channel ID, Connected Members)
- Real Permission Enforcement (OWNER, ADMINISTRATOR, MANAGE_CHANNELS)
- Networking Playground & Instant 1-Click "Switch Region Now"
- Region Latency Probe & Protocol Monitor
- Ripcord Settings & Audio Preferences Manager Tab
"""

import os
import sys

# Ensure Python 3.15 DLLs (tcl/tk) are found
dll_dir = os.path.expandvars(r"%LOCALAPPDATA%\Python\pythoncore-3.15-64\DLLs")
if os.path.exists(dll_dir) and hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(dll_dir)
    except Exception:
        pass

import configparser
import json
import logging
import socket
import threading
import time
import tkinter as tk
from tkinter import ttk, messagebox, font
from typing import Dict, List, Optional

from discord_region_rotator import (
    DiscordRegionRotator,
    RipcordLiveSync,
    RotationMode,
    EmptyChannelBehavior,
    RotatorStatus,
    DiscordClientType,
    DISCORD_REGIONS,
    load_persisted_config,
    save_persisted_config,
    CONFIG_PATH,
    RateLimitException,
    PermissionDeniedException,
    AuthenticationException
)

SETTINGS_INI_PATH = os.path.expandvars(r"%LOCALAPPDATA%\Ripcord\settings.ini")


class GlobalRegionRotatorGUI:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Ripcord — Global Voice Channel Region Rotator & Network Suite")
        self.root.geometry("820x760")
        self.root.minsize(760, 680)

        # Apply dark aesthetic theme matching Ripcord
        self.style = ttk.Style()
        self.style.theme_use("clam")
        self._configure_styles()

        # Engine state
        self.rotator: Optional[DiscordRegionRotator] = None
        self.live_state: Dict = {}
        self.auto_sync_enabled = tk.BooleanVar(value=True)
        self.persisted_config = load_persisted_config()

        self._build_ui()
        self._load_persisted_values()

        # Start live Ripcord sync background thread
        self._sync_thread_running = True
        self._sync_thread = threading.Thread(target=self._live_sync_worker, daemon=True)
        self._sync_thread.start()

        # Latency probe background thread
        self._latency_thread = threading.Thread(target=self._latency_probe_worker, daemon=True)
        self._latency_thread.start()

        self._poll_ui_updates()

    def _configure_styles(self):
        bg_dark = "#1E1F22"
        bg_card = "#2B2D31"
        fg_text = "#F2F3F5"
        accent_blue = "#5865F2"
        btn_green = "#23A55A"
        btn_red = "#F23F43"

        self.root.configure(bg=bg_dark)
        self.style.configure(".", background=bg_dark, foreground=fg_text, font=("Segoe UI", 10))
        self.style.configure("TNotebook", background=bg_dark, borderwidth=0)
        self.style.configure("TNotebook.Tab", background="#313338", foreground="#949BA4", padding=[12, 6], font=("Segoe UI", 10, "bold"))
        self.style.map("TNotebook.Tab", background=[("selected", bg_card)], foreground=[("selected", "#FFFFFF")])

        self.style.configure("Card.TFrame", background=bg_card, relief="flat")
        self.style.configure("Header.TLabel", font=("Segoe UI", 13, "bold"), foreground="#FFFFFF", background=bg_card)
        self.style.configure("SubHeader.TLabel", font=("Segoe UI", 10, "bold"), foreground="#949BA4", background=bg_card)
        self.style.configure("Value.TLabel", font=("Segoe UI", 11, "bold"), foreground="#00D26A", background=bg_card)
        self.style.configure("Highlight.TLabel", font=("Segoe UI", 12, "bold"), foreground="#5865F2", background=bg_card)
        self.style.configure("Warn.TLabel", font=("Segoe UI", 11, "bold"), foreground="#FEE75C", background=bg_card)
        self.style.configure("Error.TLabel", font=("Segoe UI", 11, "bold"), foreground="#F23F43", background=bg_card)

        self.style.configure("Start.TButton", font=("Segoe UI", 11, "bold"), background=btn_green, foreground="#FFFFFF")
        self.style.configure("Stop.TButton", font=("Segoe UI", 11, "bold"), background=btn_red, foreground="#FFFFFF")
        self.style.configure("Action.TButton", font=("Segoe UI", 10, "bold"), background=accent_blue, foreground="#FFFFFF")

    def _build_ui(self):
        main_frame = ttk.Frame(self.root, padding=8)
        main_frame.pack(fill=tk.BOTH, expand=True)

        # Tab Notebook
        self.notebook = ttk.Notebook(main_frame)
        self.notebook.pack(fill=tk.BOTH, expand=True)

        self.rotator_tab = ttk.Frame(self.notebook, style="Card.TFrame", padding=10)
        self.network_tab = ttk.Frame(self.notebook, style="Card.TFrame", padding=10)
        self.preferences_tab = ttk.Frame(self.notebook, style="Card.TFrame", padding=10)

        self.notebook.add(self.rotator_tab, text=" 🔄 Global Region Rotator ")
        self.notebook.add(self.network_tab, text=" 🌐 Networking Playground ")
        self.notebook.add(self.preferences_tab, text=" ⚙️ Ripcord Preferences Manager ")

        self._build_rotator_tab()
        self._build_network_tab()
        self._build_preferences_tab()

    # -----------------------------------------------------------------
    # Tab 1: Global Region Rotator
    # -----------------------------------------------------------------
    def _build_rotator_tab(self):
        tab = self.rotator_tab

        # Top Title & Live Sync Header
        title_card = ttk.Frame(tab, style="Card.TFrame")
        title_card.pack(fill=tk.X, pady=(0, 6))

        title_row = ttk.Frame(title_card, style="Card.TFrame")
        title_row.pack(fill=tk.X)

        ttk.Label(title_row, text="GLOBAL VOICE CHANNEL REGION ROTATOR", style="Header.TLabel").pack(side=tk.LEFT)
        sync_cb = ttk.Checkbutton(
            title_row,
            text="⚡ Live Ripcord Auto-Sync",
            variable=self.auto_sync_enabled,
            command=self._on_sync_toggled
        )
        sync_cb.pack(side=tk.RIGHT)

        self.sync_info_label = ttk.Label(
            title_card,
            text="Monitoring Ripcord database...",
            foreground="#949BA4",
            background="#2B2D31"
        )
        self.sync_info_label.pack(anchor=tk.W, pady=(2, 0))

        # Config Card
        config_card = ttk.Frame(tab, style="Card.TFrame", padding=8)
        config_card.pack(fill=tk.X, pady=(0, 6))

        cfg_grid = ttk.Frame(config_card, style="Card.TFrame")
        cfg_grid.pack(fill=tk.X)

        ttk.Label(cfg_grid, text="Discord Token:", background="#2B2D31").grid(row=0, column=0, sticky=tk.W, pady=2)
        token_subframe = ttk.Frame(cfg_grid, style="Card.TFrame")
        token_subframe.grid(row=0, column=1, sticky=tk.W, padx=6, pady=2)
        self.token_entry = ttk.Entry(token_subframe, width=28, show="*")
        self.token_entry.pack(side=tk.LEFT)
        self.btn_toggle_token = ttk.Button(token_subframe, text="👁️", width=3, command=self._toggle_token_visibility)
        self.btn_toggle_token.pack(side=tk.LEFT, padx=(2, 0))
        self.btn_token_help = ttk.Button(token_subframe, text="🔑 Help", width=6, command=self._show_token_help)
        self.btn_token_help.pack(side=tk.LEFT, padx=(2, 0))

        ttk.Label(cfg_grid, text="Server (Guild):", background="#2B2D31").grid(row=0, column=2, sticky=tk.W, padx=(10, 0), pady=2)
        self.guild_entry = ttk.Entry(cfg_grid, width=22)
        self.guild_entry.grid(row=0, column=3, sticky=tk.W, padx=6, pady=2)

        ttk.Label(cfg_grid, text="Voice Channel:", background="#2B2D31").grid(row=1, column=0, sticky=tk.W, pady=4)
        self.channel_entry = ttk.Entry(cfg_grid, width=38)
        self.channel_entry.grid(row=1, column=1, sticky=tk.W, padx=6, pady=4)

        ttk.Label(cfg_grid, text="Rotation Interval (s):", background="#2B2D31").grid(row=1, column=2, sticky=tk.W, padx=(10, 0), pady=4)
        self.interval_spin = ttk.Spinbox(cfg_grid, from_=5, to=3600, width=8)
        self.interval_spin.grid(row=1, column=3, sticky=tk.W, padx=6, pady=4)
        self.interval_spin.set("15")

        # Mode and Options
        opts_frame = ttk.Frame(config_card, style="Card.TFrame")
        opts_frame.pack(fill=tk.X, pady=(4, 0))

        ttk.Label(opts_frame, text="Mode:", background="#2B2D31").pack(side=tk.LEFT, padx=(0, 6))
        self.mode_var = tk.StringVar(value="SEQUENTIAL")
        for m in ("SEQUENTIAL", "RANDOM", "CUSTOM"):
            ttk.Radiobutton(opts_frame, text=m, value=m, variable=self.mode_var).pack(side=tk.LEFT, padx=4)

        ttk.Label(opts_frame, text="Empty Channel:", background="#2B2D31").pack(side=tk.LEFT, padx=(16, 6))
        self.empty_var = tk.StringVar(value="PAUSE")
        ttk.Combobox(
            opts_frame,
            textvariable=self.empty_var,
            values=["PAUSE", "CONTINUE", "RESET_AUTO"],
            state="readonly",
            width=12
        ).pack(side=tk.LEFT)

        self.bypass_perms_var = tk.BooleanVar(value=False)
        self.chk_bypass = ttk.Checkbutton(
            opts_frame,
            text="⚠️ Bypass Permission Check",
            variable=self.bypass_perms_var
        )
        self.chk_bypass.pack(side=tk.RIGHT, padx=4)

        # Status Monitor Card
        status_card = ttk.Frame(tab, style="Card.TFrame", padding=10)
        status_card.pack(fill=tk.X, pady=(0, 6))

        ttk.Label(status_card, text="GLOBAL STATUS MONITOR", style="SubHeader.TLabel").pack(anchor=tk.W, pady=(0, 6))

        grid = ttk.Frame(status_card, style="Card.TFrame")
        grid.pack(fill=tk.X)

        ttk.Label(grid, text="Rotation Status:", background="#2B2D31").grid(row=0, column=0, sticky=tk.W, pady=2)
        self.lbl_status = ttk.Label(grid, text="STOPPED", style="Highlight.TLabel")
        self.lbl_status.grid(row=0, column=1, sticky=tk.W, padx=10, pady=2)

        ttk.Label(grid, text="Connected Members:", background="#2B2D31").grid(row=0, column=2, sticky=tk.W, padx=(20, 0), pady=2)
        self.lbl_members = ttk.Label(grid, text="0 users", style="Value.TLabel")
        self.lbl_members.grid(row=0, column=3, sticky=tk.W, padx=10, pady=2)

        ttk.Label(grid, text="Current RTC Region:", background="#2B2D31").grid(row=1, column=0, sticky=tk.W, pady=2)
        self.lbl_current_region = ttk.Label(grid, text="Automatic (null)", style="Value.TLabel")
        self.lbl_current_region.grid(row=1, column=1, sticky=tk.W, padx=10, pady=2)

        ttk.Label(grid, text="Target RTC Region:", background="#2B2D31").grid(row=1, column=2, sticky=tk.W, padx=(20, 0), pady=2)
        self.lbl_target_region = ttk.Label(grid, text="Pending", style="Highlight.TLabel")
        self.lbl_target_region.grid(row=1, column=3, sticky=tk.W, padx=10, pady=2)

        ttk.Label(grid, text="Next Rotation In:", background="#2B2D31").grid(row=2, column=0, sticky=tk.W, pady=2)
        self.lbl_countdown = ttk.Label(grid, text="--", style="Warn.TLabel")
        self.lbl_countdown.grid(row=2, column=1, sticky=tk.W, padx=10, pady=2)

        ttk.Label(grid, text="Successful Rotations:", background="#2B2D31").grid(row=2, column=2, sticky=tk.W, padx=(20, 0), pady=2)
        self.lbl_rotations_count = ttk.Label(grid, text="0", style="Value.TLabel")
        self.lbl_rotations_count.grid(row=2, column=3, sticky=tk.W, padx=10, pady=2)

        # Real Permission Status Row
        ttk.Label(grid, text="Permission Status:", background="#2B2D31").grid(row=3, column=0, sticky=tk.W, pady=4)
        self.lbl_permission = ttk.Label(grid, text="CHECKING RIPCORD...", style="Warn.TLabel")
        self.lbl_permission.grid(row=3, column=1, columnspan=3, sticky=tk.W, padx=10, pady=4)

        # Permission Explanation Banner
        self.perm_banner = ttk.Label(
            status_card,
            text="",
            background="#2B2D31",
            foreground="#FEE75C",
            wraplength=720,
            justify=tk.LEFT
        )
        self.perm_banner.pack(anchor=tk.W, pady=(2, 0))

        # Control Buttons
        btn_frame = ttk.Frame(tab, style="Card.TFrame")
        btn_frame.pack(fill=tk.X, pady=(4, 6))

        self.btn_start = ttk.Button(btn_frame, text="▶ START GLOBAL ROTATION", style="Start.TButton", command=self.on_start)
        self.btn_start.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 6), ipady=5)

        self.btn_stop = ttk.Button(btn_frame, text="⏹ STOP GLOBAL ROTATION", style="Stop.TButton", command=self.on_stop, state=tk.DISABLED)
        self.btn_stop.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(6, 0), ipady=5)

        # Log & Protocol Output
        log_card = ttk.Frame(tab, style="Card.TFrame", padding=6)
        log_card.pack(fill=tk.BOTH, expand=True)

        ttk.Label(log_card, text="ACTIVITY LOG & PROTOCOL MONITOR", style="SubHeader.TLabel").pack(anchor=tk.W, pady=(0, 4))
        self.log_text = tk.Text(log_card, bg="#1E1F22", fg="#DBDEE1", insertbackground="#FFFFFF", relief="flat", height=8, font=("Consolas", 9))
        self.log_text.pack(fill=tk.BOTH, expand=True)
        self._log("Global Voice Channel Region Rotator ready. Listening to Ripcord live state.")

    # -----------------------------------------------------------------
    # Tab 2: Networking Playground & Instant Switch
    # -----------------------------------------------------------------
    def _build_network_tab(self):
        tab = self.network_tab

        title_card = ttk.Frame(tab, style="Card.TFrame")
        title_card.pack(fill=tk.X, pady=(0, 10))
        ttk.Label(title_card, text="DISCORD VOICE NETWORKING & REGION PLAYGROUND", style="Header.TLabel").pack(anchor=tk.W)
        ttk.Label(
            title_card,
            text="Directly test regional latency, inspect RTC endpoints, and instantly force a voice region switch.",
            foreground="#949BA4",
            background="#2B2D31"
        ).pack(anchor=tk.W)

        # Manual Instant Region Switch Box
        switch_card = ttk.Frame(tab, style="Card.TFrame", padding=12)
        switch_card.pack(fill=tk.X, pady=(0, 10))

        ttk.Label(switch_card, text="MANUAL REGION SWITCH (1-CLICK INSTANT)", style="SubHeader.TLabel").pack(anchor=tk.W, pady=(0, 6))
        switch_row = ttk.Frame(switch_card, style="Card.TFrame")
        switch_row.pack(fill=tk.X)

        ttk.Label(switch_row, text="Target Region:", background="#2B2D31").pack(side=tk.LEFT, padx=(0, 8))
        self.manual_region_var = tk.StringVar(value="india")
        region_choices = [r["id"] if r["id"] is not None else "auto" for r in DISCORD_REGIONS]
        self.region_combo = ttk.Combobox(switch_row, textvariable=self.manual_region_var, values=region_choices, state="readonly", width=18)
        self.region_combo.pack(side=tk.LEFT, padx=(0, 12))

        self.btn_instant_switch = ttk.Button(
            switch_row,
            text="⚡ Switch Region Now",
            style="Action.TButton",
            command=self.on_instant_switch
        )
        self.btn_instant_switch.pack(side=tk.LEFT, ipady=3)

        self.switch_feedback_lbl = ttk.Label(switch_card, text="", background="#2B2D31", foreground="#00D26A")
        self.switch_feedback_lbl.pack(anchor=tk.W, pady=(6, 0))

        # Latency / Ping Monitor
        ping_card = ttk.Frame(tab, style="Card.TFrame", padding=12)
        ping_card.pack(fill=tk.BOTH, expand=True)

        ttk.Label(ping_card, text="LIVE REGIONAL LATENCY & CONNECTIVITY PROBE", style="SubHeader.TLabel").pack(anchor=tk.W, pady=(0, 6))

        self.ping_text = tk.Text(ping_card, bg="#1E1F22", fg="#00D26A", relief="flat", font=("Consolas", 9), height=14)
        self.ping_text.pack(fill=tk.BOTH, expand=True)

    # -----------------------------------------------------------------
    # Tab 3: Ripcord Preferences & Audio Settings Manager
    # -----------------------------------------------------------------
    def _build_preferences_tab(self):
        tab = self.preferences_tab

        title_card = ttk.Frame(tab, style="Card.TFrame")
        title_card.pack(fill=tk.X, pady=(0, 10))
        ttk.Label(title_card, text="RIPCORD PREFERENCES & SETTINGS MANAGER", style="Header.TLabel").pack(anchor=tk.W)
        ttk.Label(
            title_card,
            text="View and modify Ripcord settings.ini directly. Fixes offscreen window coordinates and audio devices.",
            foreground="#949BA4",
            background="#2B2D31"
        ).pack(anchor=tk.W)

        # Settings Form Card
        pref_card = ttk.Frame(tab, style="Card.TFrame", padding=12)
        pref_card.pack(fill=tk.BOTH, expand=True)

        grid = ttk.Frame(pref_card, style="Card.TFrame")
        grid.pack(fill=tk.X)

        ttk.Label(grid, text="Audio Input Device:", background="#2B2D31").grid(row=0, column=0, sticky=tk.W, pady=4)
        self.pref_input_entry = ttk.Entry(grid, width=40)
        self.pref_input_entry.grid(row=0, column=1, sticky=tk.W, padx=8, pady=4)

        ttk.Label(grid, text="Audio Output Device:", background="#2B2D31").grid(row=1, column=0, sticky=tk.W, pady=4)
        self.pref_output_entry = ttk.Entry(grid, width=40)
        self.pref_output_entry.grid(row=1, column=1, sticky=tk.W, padx=8, pady=4)

        ttk.Label(grid, text="Theme Preset:", background="#2B2D31").grid(row=2, column=0, sticky=tk.W, pady=4)
        self.pref_theme_var = tk.StringVar(value="carbon")
        ttk.Combobox(grid, textvariable=self.pref_theme_var, values=["carbon", "classic", "dark", "solarized_dark", "gruvbox"], state="readonly", width=20).grid(row=2, column=1, sticky=tk.W, padx=8, pady=4)

        ttk.Label(grid, text="Push to Talk:", background="#2B2D31").grid(row=3, column=0, sticky=tk.W, pady=4)
        self.pref_ptt_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(grid, text="Enable Push-to-Talk", variable=self.pref_ptt_var).grid(row=3, column=1, sticky=tk.W, padx=8, pady=4)

        ttk.Label(grid, text="Low Bandwidth Mode:", background="#2B2D31").grid(row=4, column=0, sticky=tk.W, pady=4)
        self.pref_lowbw_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(grid, text="Enable Low Bandwidth", variable=self.pref_lowbw_var).grid(row=4, column=1, sticky=tk.W, padx=8, pady=4)

        btn_row = ttk.Frame(pref_card, style="Card.TFrame")
        btn_row.pack(fill=tk.X, pady=(16, 0))

        ttk.Button(btn_row, text="💾 Save Preferences to Ripcord", style="Start.TButton", command=self.on_save_preferences).pack(side=tk.LEFT, padx=(0, 8), ipady=4)
        ttk.Button(btn_row, text="🔄 Reset Window Coordinates (Center on Screen)", style="Action.TButton", command=self.on_reset_geometries).pack(side=tk.LEFT, padx=8, ipady=4)

        self.pref_feedback_lbl = ttk.Label(pref_card, text="", background="#2B2D31", foreground="#00D26A")
        self.pref_feedback_lbl.pack(anchor=tk.W, pady=(8, 0))

        self._load_current_settings_ini()

    # -----------------------------------------------------------------
    # Live State & Sync Background Thread
    # -----------------------------------------------------------------
    def _live_sync_worker(self):
        """Monitors Ripcord's SQLite database every 1 second and syncs state."""
        while self._sync_thread_running:
            try:
                state = RipcordLiveSync.get_live_state()
                self.live_state = state
            except Exception as e:
                logger.debug(f"Sync error: {e}")
            time.sleep(1.0)

    def _on_sync_toggled(self):
        if self.auto_sync_enabled.get():
            self._log("Live Ripcord auto-sync ENABLED.")
        else:
            self._log("Live Ripcord auto-sync PAUSED.")

    def _latency_probe_worker(self):
        """Continuously probes regional Discord gateway / network round-trip time."""
        probes = [
            ("India (Mumbai / Asia)", "162.159.137.232"),
            ("Singapore / APAC", "162.159.134.234"),
            ("Rotterdam / EU Central", "162.159.133.233"),
            ("US East / Global Core", "162.159.135.232"),
        ]
        while self._sync_thread_running:
            lines = []
            lines.append(f"=== DISCORD NETWORK LATENCY PROBE [{time.strftime('%H:%M:%S')}] ===")
            for label, ip in probes:
                t0 = time.perf_counter()
                try:
                    s = socket.create_connection((ip, 443), timeout=1.5)
                    dt = (time.perf_counter() - t0) * 1000
                    s.close()
                    lines.append(f"  ● {label:<26}: {dt:6.1f} ms [CONNECTED]")
                except Exception as e:
                    lines.append(f"  ○ {label:<26}: Timeout / Unreachable")
            
            output = "\n".join(lines)
            try:
                self.root.after(0, self._update_ping_display, output)
            except Exception:
                pass
            time.sleep(5.0)

    def _update_ping_display(self, text: str):
        self.ping_text.delete("1.0", tk.END)
        self.ping_text.insert(tk.END, text)

    # -----------------------------------------------------------------
    # UI Polling & Updates
    # -----------------------------------------------------------------
    def _poll_ui_updates(self):
        """Runs in GUI thread to refresh status labels and sync controls."""
        try:
            # 1. Update from Live Ripcord state
            st = self.live_state
            if st.get("available"):
                if st.get("connected"):
                    # Auto-fill fields if auto-sync is checked
                    if self.auto_sync_enabled.get():
                        gid = str(st["guild_id"])
                        cid = str(st["channel_id"])
                        if self.guild_entry.get() != gid:
                            self.guild_entry.delete(0, tk.END)
                            self.guild_entry.insert(0, gid)
                        if self.channel_entry.get() != cid:
                            self.channel_entry.delete(0, tk.END)
                            self.channel_entry.insert(0, cid)

                    self.sync_info_label.config(
                        text=f"Connected in Ripcord as {st.get('username')} | Server: {st.get('guild_name')} | Channel: {st.get('channel_name')}",
                        foreground="#00D26A"
                    )

                    # Update member count
                    member_count = st.get("connected_members_count", 0)
                    self.lbl_members.config(text=f"{member_count} users active")

                    # Update permissions
                    has_perm = st.get("has_manage_channels", False)
                    perm_status = st.get("perm_status", "UNKNOWN")
                    perm_reason = st.get("perm_reason", "")
                    can_operate = has_perm or self.bypass_perms_var.get()

                    if has_perm:
                        self.lbl_permission.config(text=f"🟢 {perm_status}", foreground="#00D26A")
                        self.perm_banner.config(
                            text=f"✅ {perm_reason} You have permission to rotate voice regions on this server.",
                            foreground="#00D26A"
                        )
                    else:
                        self.lbl_permission.config(text=f"🔴 {perm_status}", foreground="#F23F43")
                        self.perm_banner.config(
                            text=f"⚠️ {perm_reason} You do NOT have MANAGE_CHANNELS permission on this server. Discord will reject voice region changes.",
                            foreground="#F23F43"
                        )

                    if not (self.rotator and self.rotator.is_running()):
                        if can_operate:
                            self.btn_start.config(state=tk.NORMAL)
                            self.btn_instant_switch.config(state=tk.NORMAL)
                        else:
                            self.btn_start.config(state=tk.DISABLED)
                            self.btn_instant_switch.config(state=tk.DISABLED)
                else:
                    self.sync_info_label.config(
                        text="Ripcord is running, but user is not currently connected to any Discord voice channel.",
                        foreground="#949BA4"
                    )
                    self.lbl_members.config(text="0 users")
                    self.lbl_permission.config(text="DISCONNECTED (No Voice Channel)", foreground="#949BA4")
                    self.perm_banner.config(text="Join a voice channel in Ripcord to auto-detect channel configuration.", foreground="#949BA4")
                    if not (self.rotator and self.rotator.is_running()):
                        if self.bypass_perms_var.get():
                            self.btn_start.config(state=tk.NORMAL)
                            self.btn_instant_switch.config(state=tk.NORMAL)
                        else:
                            self.btn_start.config(state=tk.DISABLED)
                            self.btn_instant_switch.config(state=tk.DISABLED)

            # 2. Update from Rotator engine
            if self.rotator:
                self.lbl_status.config(text=self.rotator.status.value)
                cur = self.rotator.current_rtc_region or "Automatic (null)"
                tgt = self.rotator.target_rtc_region or "Pending"
                self.lbl_current_region.config(text=cur)
                self.lbl_target_region.config(text=tgt)
                self.lbl_rotations_count.config(text=str(self.rotator.total_rotations))

                if self.rotator.countdown_remaining > 0:
                    self.lbl_countdown.config(text=f"{self.rotator.countdown_remaining}s")
                else:
                    self.lbl_countdown.config(text="--")

                if self.rotator.last_error:
                    self._log(f"ALERT: {self.rotator.last_error}")
                    self.rotator.last_error = None

        except Exception as e:
            logger.debug(f"UI poll error: {e}")

        self.root.after(500, self._poll_ui_updates)

    # -----------------------------------------------------------------
    # Start / Stop / Action Handlers
    # -----------------------------------------------------------------
    def on_start(self):
        token = self.token_entry.get().strip()
        guild_id_str = self.guild_entry.get().strip()
        channel_id_str = self.channel_entry.get().strip()
        interval_str = self.interval_spin.get().strip()

        if not token or token in ("persisted_test_token", "sample_token_authorized", "MTAyOTM4NDc1NjQ3Mzg5MjAx.sample_token_authorized"):
            messagebox.showerror(
                "Missing Discord Token",
                "Please enter your valid Discord authorization token.\n\n"
                "Click '🔑 Help' next to the token box for instructions on how to get it.\n"
                "The token is saved locally to rotator_config.json so you only enter it once."
            )
            return

        try:
            guild_id = int(guild_id_str)
            channel_id = int(channel_id_str)
            interval = int(interval_str)
        except ValueError:
            messagebox.showerror("Invalid Input", "Server ID, Channel ID, and Interval must be valid integers.")
            return

        # Check permission grant from live Ripcord state
        if self.live_state.get("connected") and not self.bypass_perms_var.get():
            if not self.live_state.get("has_manage_channels", False):
                messagebox.showerror(
                    "Permission Denied (MANAGE_CHANNELS)",
                    "You do not have MANAGE_CHANNELS or ADMINISTRATOR permission on this server.\n\n"
                    "Discord requires Manage Channels permission to alter voice regions.\n"
                    "Please join a voice channel on a server where you are an Admin or Server Owner.\n\n"
                    "(Check 'Bypass Permission Check' only if using elevated external bot/admin tokens.)"
                )
                return

        # Save config
        save_persisted_config({
            "token": token,
            "guild_id": guild_id_str,
            "channel_id": channel_id_str,
            "interval": interval
        })

        mode = RotationMode[self.mode_var.get()]
        empty_behavior = EmptyChannelBehavior[self.empty_var.get()]

        self.rotator = DiscordRegionRotator(
            token=token,
            guild_id=guild_id,
            channel_id=channel_id,
            interval_seconds=interval,
            mode=mode,
            empty_behavior=empty_behavior
        )

        # Seed member list if Ripcord has live members
        if self.live_state.get("connected_members"):
            self.rotator.update_connected_members(self.live_state["connected_members"])

        self.rotator.start()
        self.btn_start.config(state=tk.DISABLED)
        self.btn_stop.config(state=tk.NORMAL)
        self._log(f"Starting rotation on channel {channel_id} (mode={mode.value}, interval={interval}s)...")

    def on_stop(self):
        if self.rotator:
            self._log("STOP command issued: Halting scheduler...")
            self.rotator.stop()
        self.btn_start.config(state=tk.NORMAL)
        self.btn_stop.config(state=tk.DISABLED)
        self._log(f"Stopped. Total successful rotations: {self.rotator.total_rotations if self.rotator else 0}")

    def on_instant_switch(self):
        """Immediately updates the RTC region with 1-click."""
        token = self.token_entry.get().strip()
        channel_id_str = self.channel_entry.get().strip()
        target_choice = self.manual_region_var.get().strip()
        target_val = None if target_choice in ("auto", "null", "Automatic") else target_choice

        if not token or token in ("persisted_test_token", "sample_token_authorized", "MTAyOTM4NDc1NjQ3Mzg5MjAx.sample_token_authorized"):
            messagebox.showerror(
                "Discord Token Required",
                "Please enter your valid Discord authorization token.\n\n"
                "Click '🔑 Help' next to the token box for instructions on how to get it."
            )
            return

        try:
            channel_id = int(channel_id_str)
        except ValueError:
            messagebox.showerror("Channel ID Required", "Enter a valid Voice Channel ID.")
            return

        if self.live_state.get("connected") and not self.bypass_perms_var.get():
            if not self.live_state.get("has_manage_channels", False):
                messagebox.showerror(
                    "Permission Denied (MANAGE_CHANNELS)",
                    "You do not have MANAGE_CHANNELS or ADMINISTRATOR permission on this server.\n\n"
                    "Discord requires Manage Channels permission to alter voice regions.\n"
                    "Please join a voice channel on a server where you are an Admin or Server Owner.\n\n"
                    "(Check 'Bypass Permission Check' only if using elevated external bot/admin tokens.)"
                )
                return

        self.switch_feedback_lbl.config(text=f"Dispatching update to '{target_choice}'...", foreground="#FEE75C")
        self.root.update_idletasks()

        def do_switch():
            rot = DiscordRegionRotator(token=token, guild_id=0, channel_id=channel_id)
            try:
                rot.switch_region_now(target_val)
                self.root.after(0, lambda: self.switch_feedback_lbl.config(
                    text=f"✅ Successfully switched channel {channel_id} to '{target_choice}'!",
                    foreground="#00D26A"
                ))
                self.root.after(0, lambda: self._log(f"Manual Instant Switch: Voice region set to '{target_choice}'."))
            except AuthenticationException:
                msg_auth = "❌ 401 Unauthorized: Invalid Discord Token. Please enter your valid Discord User Token."
                self.root.after(0, lambda m=msg_auth: self.switch_feedback_lbl.config(
                    text=m,
                    foreground="#F23F43"
                ))
                self.root.after(0, lambda m=msg_auth: self._log(f"Manual Switch Failed: {m}"))
            except PermissionDeniedException:
                msg_perm = "❌ Permission Denied (HTTP 403): Missing MANAGE_CHANNELS on this server."
                self.root.after(0, lambda m=msg_perm: self.switch_feedback_lbl.config(
                    text=m,
                    foreground="#F23F43"
                ))
                self.root.after(0, lambda m=msg_perm: self._log(f"Manual Switch Failed: {m}"))
            except Exception as e:
                err_str = str(e)
                if "401" in err_str or "Unauthorized" in err_str:
                    display_err = "❌ 401 Unauthorized: Invalid Discord Token. Please enter your valid Discord User Token."
                elif "403" in err_str or "Forbidden" in err_str:
                    display_err = "❌ Permission Denied (HTTP 403): Missing MANAGE_CHANNELS on this server."
                else:
                    display_err = f"❌ Error: {err_str}"
                self.root.after(0, lambda m=display_err: self.switch_feedback_lbl.config(
                    text=m,
                    foreground="#F23F43"
                ))
                self.root.after(0, lambda m=display_err: self._log(f"Manual Switch Failed: {m}"))

        threading.Thread(target=do_switch, daemon=True).start()

    # -----------------------------------------------------------------
    # Preferences Manager Tab Handlers
    # -----------------------------------------------------------------
    def _load_current_settings_ini(self):
        if not os.path.exists(SETTINGS_INI_PATH):
            return
        try:
            cp = configparser.ConfigParser()
            cp.read(SETTINGS_INI_PATH, encoding="utf-8")
            if "audio" in cp:
                self.pref_input_entry.delete(0, tk.END)
                self.pref_input_entry.insert(0, cp["audio"].get("input_device", ""))
                self.pref_output_entry.delete(0, tk.END)
                self.pref_output_entry.insert(0, cp["audio"].get("output_device", ""))
            if "theme" in cp:
                self.pref_theme_var.set(cp["theme"].get("preset_name", "carbon"))
            if "voice" in cp:
                self.pref_ptt_var.set(cp["voice"].getboolean("use_push_to_talk", False))
            if "network" in cp:
                self.pref_lowbw_var.set(cp["network"].getboolean("low_bandwidth_mode", False))
        except Exception as e:
            logger.warning(f"Failed to read settings.ini: {e}")

    def on_save_preferences(self):
        if not os.path.exists(SETTINGS_INI_PATH):
            messagebox.showerror("Not Found", f"Ripcord settings file not found at:\n{SETTINGS_INI_PATH}")
            return
        try:
            lines = open(SETTINGS_INI_PATH, "r", encoding="utf-8").readlines()
            new_lines = []
            cur_sec = None
            for line in lines:
                sline = line.strip()
                if sline.startswith("[") and sline.endswith("]"):
                    cur_sec = sline[1:-1]
                if cur_sec == "audio" and (line.startswith("input_device=") or line.startswith("output_device=") or line.startswith("outout_device=")):
                    continue
                if cur_sec == "theme" and line.startswith("preset_name="):
                    continue
                if cur_sec == "voice" and line.startswith("use_push_to_talk="):
                    continue
                if cur_sec == "network" and line.startswith("low_bandwidth_mode="):
                    continue
                new_lines.append(line)
                if sline == "[audio]":
                    new_lines.append(f"input_device={self.pref_input_entry.get().strip()}\n")
                    new_lines.append(f"output_device={self.pref_output_entry.get().strip()}\n")
                elif sline == "[theme]":
                    new_lines.append(f"preset_name={self.pref_theme_var.get().strip()}\n")
                elif sline == "[voice]":
                    new_lines.append(f"use_push_to_talk={'true' if self.pref_ptt_var.get() else 'false'}\n")
                elif sline == "[network]":
                    new_lines.append(f"low_bandwidth_mode={'true' if self.pref_lowbw_var.get() else 'false'}\n")

            with open(SETTINGS_INI_PATH, "w", encoding="utf-8") as f:
                f.writelines(new_lines)

            self.pref_feedback_lbl.config(text="✅ Preferences saved to Ripcord settings.ini!", foreground="#00D26A")
            self._log("Updated settings.ini with new audio and theme parameters.")
        except Exception as e:
            messagebox.showerror("Save Error", f"Failed to save settings: {e}")

    def on_reset_geometries(self):
        """Clears off-screen window coordinates so Preferences and chat windows center normally."""
        if not os.path.exists(SETTINGS_INI_PATH):
            return
        try:
            lines = open(SETTINGS_INI_PATH, "r", encoding="utf-8").readlines()
            new_lines = []
            skip = False
            for line in lines:
                if line.strip() == "[geometries]":
                    new_lines.append(line)
                    skip = True
                    continue
                if skip and line.startswith("["):
                    skip = False
                if skip:
                    continue
                new_lines.append(line)

            with open(SETTINGS_INI_PATH, "w", encoding="utf-8") as f:
                f.writelines(new_lines)

            self.pref_feedback_lbl.config(text="✅ Geometries reset! All Ripcord windows will center on your screen.", foreground="#00D26A")
            self._log("Reset [geometries] in settings.ini: Stale offscreen coordinates cleared.")
        except Exception as e:
            messagebox.showerror("Reset Error", f"Failed to reset geometries: {e}")

    # -----------------------------------------------------------------
    # Helpers
    # -----------------------------------------------------------------
    def _toggle_token_visibility(self):
        if self.token_entry.cget("show") == "*":
            self.token_entry.config(show="")
            self.btn_toggle_token.config(text="🙈")
        else:
            self.token_entry.config(show="*")
            self.btn_toggle_token.config(text="👁️")

    def _show_token_help(self):
        help_text = (
            "How to get your Discord User Token:\n\n"
            "1. Open Discord in your Web Browser (Chrome, Edge, or Firefox) or Discord desktop app.\n"
            "2. Press [Ctrl + Shift + I] on your keyboard to open Developer Tools.\n"
            "3. Switch to the 'Network' tab in Developer Tools.\n"
            "4. Click on any server, text channel, or voice channel.\n"
            "5. In the Network log, click any Discord request (such as 'messages', 'typing', or 'science').\n"
            "6. In the right panel, find 'Headers' -> 'Request Headers' -> 'Authorization'.\n"
            "7. Copy that token value and paste it into the Discord Token box here.\n\n"
            "Security Note: The token is saved locally on your computer in %LOCALAPPDATA%\\Ripcord\\rotator_config.json.\n"
            "Never share your token with anyone else!"
        )
        messagebox.showinfo("How to get your Discord Token", help_text)

    def _load_persisted_values(self):
        cfg = self.persisted_config
        token = cfg.get("token", "")
        if token and token not in ("persisted_test_token", "sample_token_authorized", "MTAyOTM4NDc1NjQ3Mzg5MjAx.sample_token_authorized"):
            self.token_entry.delete(0, tk.END)
            self.token_entry.insert(0, token)
        if "guild_id" in cfg and cfg["guild_id"]:
            self.guild_entry.delete(0, tk.END)
            self.guild_entry.insert(0, str(cfg["guild_id"]))
        if "channel_id" in cfg and cfg["channel_id"]:
            self.channel_entry.delete(0, tk.END)
            self.channel_entry.insert(0, str(cfg["channel_id"]))
        if "interval" in cfg and cfg["interval"]:
            self.interval_spin.set(str(cfg["interval"]))

    def _log(self, msg: str):
        ts = time.strftime("[%H:%M:%S]")
        self.log_text.insert(tk.END, f"{ts} {msg}\n")
        self.log_text.see(tk.END)


def main():
    root = tk.Tk()
    app = GlobalRegionRotatorGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
