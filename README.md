# Ripcord Hardened (VC Glitch & DAVE MLS Suite)

**Ripcord Hardened** is a production-engineered, Discord-compatible desktop VoIP client for Windows x64 featuring Discord DAVE End-to-End Encryption (MLS), seamless voice region migration, and zero-drop audio transport, integrated with the **SPLCORD Native C++ DSP Audio Engine**.

---

## ⚡ Key Highlights & Production Patches

### 1. Seamless Voice Region Migration (`0x1400D5799`)
* **Problem**: When a Discord channel's RTC region is rotated or moved, Discord migrates the voice server and terminates the old server 4 seconds later with WebSocket close code `1000`. In unpatched Ripcord, receiving code 1000 dropped the entire active call.
* **Binary Shield**: Injected an active-worker verification check directly into `voice_conn_ws_disconnected`:
  ```x86asm
  mov rax, qword ptr [rdi + 0x10]  ; DisVoiceLinePriv*
  cmp rdi, qword ptr [rax + 8]     ; Check if disconnecting worker == active worker
  jne 0x1400d57ef                  ; Drop stale worker close events!
  ```
* **Result**: Voice calls migrate seamlessly between any regions with 0 dropped calls.

### 2. Lock File Failure Bypass (`0x14006D96B`)
* **Problem**: Abnormal terminations or background tasks left stale `%LOCALAPPDATA%\Ripcord\ripcord.lockfile` locks, triggering modal blocking popups (`Unable to remove old lock file and acquire new lock file`).
* **Binary Shield**: Unconditional jump (`jmp 0x14006dbe9`) bypassing `QLockFile` contention dialogs completely.

### 3. Database Existing Table Crash Shield (`0x1400BBA48`)
* **Problem**: Re-initialization collisions in SQLite caused `SQLITE_ERROR: table guild already exists (Code: 1)` fatal errors.
* **Binary Shield**: NOP'd fatal error abort (`75 38` -> `90 90`), allowing safe execution on existing database tables.

### 4. Discord DAVE Protocol v1 (E2EE MLS Audio Hook)
* Integrated via `winmm.dll` proxy hook (`libdave` C API).
* Full support for MLS Key Packages, Epoch transitions, and XChaCha20-Poly1305 encryption.
* Restores dual-stack IPv4/IPv6 socket connectivity.
* Assembly cave at `0x266BF` clamping monotonic RTP timestamp gaps to 3000ms.

### 5. SPLCORD Native C++ Audio DSP Studio (`splcord/`)
* **Real-time 48kHz Stereo Engine**: Direct WASAPI loopback capture and virtual routing.
* **Loudness Normalization**: ITU-R BS.1770-4 LUFS meter (Integrated, Short-term, Momentary).
* **Dynamics Suite**: Fast RMS Compressor + Lookahead True-Peak Limiter (0 dBFS ceiling).
* **Spatial Audio**: Panning Knob with Double Stereo Wideness.
* **Target Radar**: Real-time VC target listener tracker.
* **Auto-Pierce Counter-Measure**: Instant anti-reverb/over-head sound equalization.

---

## 🚀 Quick Start

Run both the SPLCORD DSP Studio and the Patched Ripcord client simultaneously:
```cmd
run_ripcord.bat
```

Or run via PowerShell:
```powershell
Start-Process "dist\Ripcord.exe"
```

---

## 🛠️ Repository Structure

* `dist/` — Production-hardened Windows x64 release binaries (`Ripcord.exe`, `winmm.dll`, `patcher.exe`, `tools/`).
* `splcord/` — C++20 source code and build for the SPLCORD Audio DSP Studio.
* `ripcord-community-fix/` — C source code of `winmm.dll` hook and DAVE MLS protocol implementation.
* `patcher/` — C source code of the standalone binary patcher.
* `tools/` — Global Voice Channel Region Rotator GUI and CLI automation scripts.
* `tests/` — Automated test suite covering dynamic assembly caves and protocol lifecycles.
* `docs/` — Architectural reports, protocol verification manifests, and RFC specifications.

---

## 📜 License & Compliance

Educational and interoperability research suite. Discord is a trademark of Discord Inc.
