# Changelog - Ripcord Repair & Production Hardening

All notable changes, bug fixes, protocol additions, security enhancements, and verification suites for the Ripcord desktop chat client on Windows x64 are documented in this file.

---

## [0.4.29-hardened.3] - 2026-10-30 (Next-Generation Feature Expansion & Hardening)

### Added & Repaired (Desktop Features & Interaction Ergonomics)
- **Voice Member Drag-and-Drop (`MOVE_MEMBERS`)**:
  - Implemented proper permission-aware voice member drag-and-drop in `DisGuildChannelsTreeModel`.
  - Checks Discord's `MOVE_MEMBERS` (`0x800000`) or `ADMINISTRATOR` (`0x08`) permissions, fixing the restriction that previously required `MANAGE_CHANNELS`.
  - Serializes `application/x-discord-voicemember` with magic `0x564D4F56` and snowflakes `(guild_id, user_id, source_channel_id)`.
  - Dispatches Discord REST `PATCH /api/v9/guilds/{guild_id}/members/{user_id}` with `{"channel_id": "{destination}"}`.
  - Gracefully handles HTTP 403 Forbidden and 400 Bad Request (channel full / role hierarchy).
- **Voice Region Migration & Endpoint Reconnection**:
  - Eliminates voice freezes on "Routing..." or dead audio when Discord migrates media endpoints or administrators switch voice regions.
  - Automatically unbinds stale UDP sockets, nullifies timers, resets DAVE MLS state via `DaveReset()`, and resets monotonic timestamp state via `ResetVoiceTimestamp()`.
  - Re-establishes Voice Gateway v8 handshake with new SSRC, 74-byte IP discovery, and AEAD negotiation.
- **Windows Explorer File Upload Drag-and-Drop**:
  - Added support for dragging files directly from Windows Explorer (`text/uri-list` / `CF_HDROP`) into the text channel input, triggering the Discord file upload dialog.
- **Channel Reordering & Category Sorting**:
  - Restored model drag-and-drop flags for channel sorting when user possesses `MANAGE_CHANNELS` (`0x10`) or `ADMINISTRATOR` (`0x08`).
- **Gateway Session Recovery & Backoff**:
  - Validated `Op 6 Resume` framing and sequence acknowledgment.
  - Handled `Op 9 Invalid Session` (`d: false`) by enforcing 1.5s jittered backoff before cleanly re-identifying (`Op 2`).
- **Live Audio Device Switching**:
  - Simulated and verified dynamic WASAPI device enumeration and stream re-binding without dropping active VoIP calls.
- **Global Voice Channel Region Rotator (Server-Wide Continuous Rotation)**:
  - Engineered continuous, server-wide RTC region rotation operating at the shared Discord channel configuration level (`PATCH /api/v9/channels/{id}`).
  - Triggers Discord's native media cluster migration, automatically migrating all connected channel members across Ripcord, Desktop Electron, Browser Web, and Mobile iOS/Android without tampering with individual user processes or devices.
  - Implemented Sequential, Random, and Custom sequence rotation modes with configurable intervals (minimum 5s).
  - Enforced `MANAGE_CHANNELS` (`0x10`) or `ADMINISTRATOR` (`0x08`) permission checks, halting gracefully upon 403 Forbidden without entering retry loops.
  - Handled Discord HTTP 429 Too Many Requests with automated `Retry-After` backoff and resumption.
  - Implemented configurable empty-channel behaviors: `PAUSE` (pause rotation until a member joins), `CONTINUE`, and `RESET_AUTO`.
  - Built interactive desktop GUI status panel (`tools/global_region_rotator_gui.py`) with Ripcord dark styling, live countdowns, member counters, and Start/Stop controls.
  - Built companion CLI runner (`tools/global_region_rotator_cli.py`) with `SIGINT`/`SIGTERM` handling.

### Quality Assurance & Automated Regression
- **Comprehensive Master Test Suite Expansion**:
  - Expanded test coverage to **63 automated tests across 10 test modules** (`tests/run_all_tests.py`), passing with 100% success rate in 13.3s.
  - Added `tests/test_global_region_rotator.py` (10 test cases) verifying all 10 mission criteria (2, 5, 10 connected users, client diversity, first and consecutive changes, dynamic joins/leaves, empty channel pause, immediate stop enforcement, permission failures, and rate limit recovery).
  - Added `tests/test_next_gen_features.py` (8 test cases) covering voice member drag permissions, MIME serialization, REST move requests, region migration teardown, gateway resume, file drops, and live audio device switching.
- **Production Artifact Verification**:
  - Re-compiled `patcher.exe` with BCrypt and MinGW GCC 13.2.0.
  - Rebuilt complete distribution package in `dist/` including companion tools and updated `SHA256SUMS.txt`.

---

## [0.4.29-hardened.2] - 2026-10-30 (Independent Production Validation Release)

### Added & Verified (Binary Hardening & Verification Suite)
- **Binary Hardening of `winmm.dll`**:
  - Implemented binary NOP passthrough at `0x80760` in `dist/winmm.dll`, eliminating unconditional IPv4 socket forcing across the entire application and restoring full dual-stack / IPv6 support for Discord REST API, CDN, and Slack.
  - Implemented 62-byte assembly cave at `0x266BF` in `dist/winmm.dll` hook at `0x811DD`:
    - Safely detects initial voice packet (`last_send_ms == 0`) and steps by default 960 without millions-of-samples timestamp jump.
    - Clamps silence/mute pauses > 3000ms to 144,000 samples to prevent receiver buffer desync.
- **Dedicated Binary Integrity Test Suite (`test_winmm_binary_integrity.py`)**:
  - Validates PE32+ 64-bit AMD64 architecture, DEP/NX, ASLR, HighEntropyVA.
  - Validates export table: exactly 237 exports matching Windows Multimedia API.
  - Validates 64-bit indirect JMP thunks (`48 FF 25 [disp32]`) targeting `C:\Windows\System32\winmm.dll`.
  - Validates socket protocol passthrough at `0x80760`.
  - Validates timestamp cave jump and instructions at `0x266BF`.
  - Automated smoke test launching `Ripcord.exe` with `winmm.dll` and verifying running process state and clean shutdown.
- **Master Test Runner Expansion**:
  - Test suite expanded to **39 automated tests across 6 test modules** (100% passing).
- **Independent Validation Report**:
  - Published [docs/INDEPENDENT_VERIFICATION_REPORT.md](docs/INDEPENDENT_VERIFICATION_REPORT.md) detailing technical findings across all 6 validation criteria.

---

## [0.4.29-hardened.1] - 2026-10-30

### Fixed (Voice & Audio Transport)
- **Voice DAVE E2EE Enforcement**: Full integration of Discord DAVE (End-to-End Encryption) based on IETF MLS (RFC 9420) via `libdave.lib`. Resolves voice disconnection error code 4017.
- **AEAD XChaCha20-Poly1305 Voice Encryption**: Implemented `aead_xchacha20_poly1305_rtpsize` transport encryption via libsodium, replacing deprecated `xsalsa20_poly1305`. Resolves voice disconnection error code 4016.
- **74-Byte UDP IP Discovery**: Upgraded IP discovery packet generation from legacy 70-byte format to current 74-byte protocol. Swapped big-endian WAN port response bytes (72–73). Eliminates indefinite hang on "Routing...".
- **RTP Header Extension Parsing**: Corrected dynamic byte calculation (`extension_dwords * 4`) for variable-length RTP header extensions larger than 8 bytes. Completely eliminates garbled, metallic, and robotic audio distortion.
- **Heartbeat Sequence Tracking (`seq_ack`)**: Added tracking of sequence numbers across both JSON text and binary WebSocket frames. Heartbeat Op 3 now includes valid `seq_ack` and timestamp `t`, preventing disconnect loops (code 1001) every 15–30 seconds.
- **Voice Audio Timestamping Jitter**: Replaced non-atomic and uninitialized timestamp generation with thread-safe synchronized timestamps (`DAVE_LOCK()`). Added clean session initialization via `ResetVoiceTimestamp()` and clamped silence pauses > 3000ms to prevent jitter buffer desync.
- **Voice Disconnect Pointer Safety**: Ensured `g_dave.webSocket` is nullified upon voice session termination, preventing use-after-free crashes in asynchronous timers.
- **Memory Leak in Audio Worker**: Fixed `DisVLWorkerConstructorHook` to avoid redundant heap allocations of `sendPacketBuffer` on reconnection.

### Fixed (Gateway & Networking)
- **Erlang ETF `SMALL_TUPLE_EXT` Decoding**: Hooked `Ded::Step` (RVA `0xB5EF0`) to cleanly parse ETF tag 104 (`SMALL_TUPLE_EXT`), emitting `ListStart` and `ListEnd` without parser crashes.
- **Gateway Map Key Discard**: Patched `ErfMap::find` (RVA `0xB9BBF`) to discard unhandled gateway keys rather than aborting the connection.
- **Scoped IPv4 Routing**: Added `IsDiscordVoiceHost` to confine IPv4 forcing strictly to `*.discord.media` and `*.discord.gg` voice servers, restoring full dual-stack and IPv6 support for Slack, REST APIs, and CDNs.
- **Discord Image Previews**: Patched CDN URL string table to `cdn.discordapp.com/\0\0\0`, restoring avatars and inline attachment previews.
- **Server Sidebar Guild Folders**: Added recursive traversal of `guild_folders` in `ErfMapFindHook`, fixing server ordering and missing server icons.

### Added (Hardened Binary Patcher)
- **Automatic Safe Backup**: Automatically creates `<target>.bak` before writing any modifications to disk.
- **Non-Destructive Verification (`-v` / `--verify`)**: Added inspection mode that reports status for each patch (`[ORIGINAL]`, `[PATCHED]`, `[MISMATCH]`) without altering the binary.
- **Bidirectional Reversion (`-u` / `--revert`)**: Restores original byte patterns bit-for-bit, verified against official SHA-256 baseline.
- **Dry-Run Simulation (`-n` / `--dry-run`)**: Simulates patch operations in read-only mode.
- **PE Validation & Size Checks**: Pre-validates 64-bit PE headers, official file size (5,509,808 bytes), and version string (`0.4.29`).
- **Compiler Portability**: Fully compatible with MinGW-W64 GCC, Clang, and MSVC.

### Security & Quality Assurance
- **Automated Test Suite**: Created a 34-test automated suite (`tests/run_all_tests.py`) covering patcher safety, ETF decoding, voice protocol lifecycle, audio RTP transport, socket scoping, and security properties (100% pass rate).
- **Security Audit**: Verified DEP/NX, ASLR, and High Entropy VA mitigations on all binaries. Confirmed zero credential leakage in logs and enforced HTTPS with `WINHTTP_FLAG_SECURE`.
- **Release Automation**: Created `build_release.py` generating release artifacts and cryptographic SHA-256 checksums in `dist/SHA256SUMS.txt`.
