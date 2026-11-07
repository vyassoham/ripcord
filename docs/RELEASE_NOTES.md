# Ripcord v0.4.29-hardened.2 — Production Release Notes

**Release Date:** October 30, 2026  
**Target Platform:** Windows 10 / 11 (x64)  
**Distribution:** Portable ZIP / Directory Deployment (`dist/`)  
**Baseline Executable:** `Ripcord.exe` (v0.4.29 x64, 5,509,808 bytes)  

---

## Highlights of this Release

Ripcord v0.4.29-hardened.2 is a major engineering and compatibility milestone for the lightweight native desktop chat client. It restores complete Discord audio voice connectivity, fixes critical user interaction defects, and brings modern protocol hardening while preserving Ripcord's signature ultra-low memory footprint (~35 MB RAM vs Discord's 450+ MB).

---

## What's New & Repaired

### 1. Interactive Voice Channel Drag-and-Drop
- **Moderator Drag-and-Drop:** You can now drag connected users between voice channels directly in the server channel tree.
- **Permission-Aware:** Properly checks for Discord's `MOVE_MEMBERS` (`0x800000`) or `ADMINISTRATOR` (`0x08`) permissions, without erroneously requiring `MANAGE_CHANNELS`.
- **REST Dispatch:** Dispatches standard `PATCH /api/v9/guilds/{guild_id}/members/{user_id}` with `{"channel_id": "{destination}"}` and handles full-channel / permission errors gracefully.

### 2. Seamless Voice Region Reconnection & Endpoint Migration
- **Zero-Hang Server Migrations:** When Discord migrates voice servers or an administrator switches voice regions, Ripcord re-establishes the connection cleanly within 200ms without getting stuck on "Routing..." or dead audio.
- **Clean Socket & DAVE Teardown:** Safely terminates obsolete UDP transports and resets DAVE MLS state before initiating Voice Gateway v8 handshakes to the new media server.

### 3. File Upload Drag-and-Drop
- **Explorer File Drop:** Drag and drop images, logs, or documents directly from Windows Explorer into the chat input area to trigger Discord's file upload dialog.

### 4. Audio Quality & Monotonic Timestamps (DEF-07)
- **Monotonic Frame Stepping:** Assembly cave at offset `0x266BF` enforces deterministic 960-sample steps per 20ms frame at 48 kHz stereo.
- **Silence Clamping:** Completely eliminates audio bursts and stutter when starting to talk after silence, clamping pause advances to 3000ms.

### 5. Network Dual-Stack Socket Restoration (DEF-08)
- **IPv6 & Dual-Stack Enabled:** Restores full IPv6 and dual-stack connectivity for Discord REST API, image CDNs (`cdn.discordapp.com`), and Slack workspaces.
- **Selective Routing:** Restricts IPv4 forcing strictly to Discord voice media hosts (`*.discord.media`).

### 6. Modern Voice Protocol Core (DEF-01 to DEF-06)
- **Voice Gateway v8 & Heartbeats (DEF-02):** Acknowledges latest sequence IDs (`seq_ack`).
- **74-Byte UDP Discovery (DEF-03):** Resolves IP discovery compatibility and big-endian port ordering.
- **AEAD XChaCha20-Poly1305 (DEF-04):** Modern authenticated voice transport encryption via `libsodium.dll`.
- **DAVE E2EE / MLS Protocol (DEF-05):** Full support for Discord's End-to-End Encryption protocol version 1.
- **RTP Header Extensions (DEF-06):** Correctly handles variable-length RTP extension headers.

### 7. Hardened Patcher & Verifiable Rollback
- **Cryptographic Patcher (`patcher.exe`):** Built with MinGW GCC 13.2.0 and Windows CNG BCrypt SHA-256 verification.
- **Automated Backup & Rollback:** Automatically generates a byte-verified `.bak` before applying patches and supports bit-perfect rollback (`--rollback`).

---

## Release Artifacts & SHA-256 Checksums

| Artifact | Size (bytes) | SHA-256 Checksum | Description |
|:---|:---:|:---:|:---|
| **`Ripcord.exe`** | 5,509,808 | `12D62ABB9AD4DB43C2B9B1398ACAE66857EB6E64205364631A3D3BDA0FF17E2E` | Baseline x64 Application |
| **`winmm.dll`** | 3,928,064 | `5FEF0D5AA5A3D990711E14D00DA3568F882DEAE8AB1CD179D7A08CE05A6E9BD9` | Hardened Proxy Sidecar |
| **`patcher.exe`** | 108,189 | `B4B5352BE7EAE79874CF39745043B733A6A67A14CF98296F221A1ECDBBBDE511` | Cryptographic Binary Patcher |

*Full release manifest available in `dist/SHA256SUMS.txt`.*

---

## Installation & Quick Start

1. **Extract Distribution:**
   Copy the contents of `dist/` into your chosen Ripcord folder (e.g. `D:\Ripcord_Win_0.4.29`).
2. **Apply Binary Patches (Optional, for deleted message display & cooldown):**
   ```cmd
   cd D:\Ripcord_Win_0.4.29
   patcher.exe
   ```
3. **Launch Ripcord:**
   Run `Ripcord.exe`. The `winmm.dll` proxy will load automatically at startup.
4. **Rollback (if ever needed):**
   ```cmd
   patcher.exe --rollback
   ```
   Or simply delete `winmm.dll` to restore default behavior.

---

## Automated Test Results
- **Master Regression Suite:** 53 / 53 tests passed (100%).
- **Verification Harness:** Real x64 machine code execution, live socket loopback, ETF parser fuzzing, and process smoke test verified.
