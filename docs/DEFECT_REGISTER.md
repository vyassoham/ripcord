# Ripcord Prioritized Defect Register

This register details all investigated, verified, and repaired defects affecting the Ripcord desktop chat client on Windows x64.

---

## Defect Summary Matrix

| ID | Severity | Priority | Component | Status | Summary |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **DEF-001** | Critical | P0 | Gateway ETF Parser | **VERIFIED & FIXED** | Gateway crash/disconnect on Erlang `SMALL_TUPLE_EXT` (tag 104). |
| **DEF-002** | Critical | P0 | Gateway ETF Parser | **VERIFIED & FIXED** | Gateway connection aborted on unknown map keys. |
| **DEF-003** | Blocker | P0 | Voice UDP Discovery | **VERIFIED & FIXED** | Indefinite hang on "Routing..." due to legacy 70-byte packet. |
| **DEF-004** | Blocker | P0 | Voice Crypto Engine | **VERIFIED & FIXED** | Voice disconnect 4016: legacy `xsalsa20_poly1305` deprecated. |
| **DEF-005** | Blocker | P0 | Voice E2EE / DAVE | **VERIFIED & FIXED** | Voice disconnect 4017: mandatory Discord DAVE E2EE missing. |
| **DEF-006** | Major | P1 | Voice Signaling | **VERIFIED & FIXED** | Voice disconnect loop (code 1001) due to missing `seq_ack` in Op 3. |
| **DEF-007** | Major | P1 | Audio Transport | **VERIFIED & FIXED** | Garbled/robotic audio from parsing RTP extensions > 8 bytes. |
| **DEF-008** | High | P1 | Audio Timestamping | **VERIFIED & FIXED** | Timestamp desync & race condition on silence and unmuting. |
| **DEF-009** | Medium | P2 | Network Socket Hook | **VERIFIED & FIXED** | Global socket hook forcing IPv4 on all traffic (Slack, REST, CDN). |
| **DEF-010** | Medium | P2 | UI Server List | **VERIFIED & FIXED** | Broken server ordering/missing servers when user has guild folders. |
| **DEF-011** | Medium | P2 | Asset Rendering | **VERIFIED & FIXED** | Broken image/avatar previews from outdated Discord CDN URLs. |
| **DEF-012** | Major | P1 | Binary Patcher | **VERIFIED & FIXED** | Standalone patcher had no backup, no revert, and no safety validation. |
| **DEF-013** | Low | P3 | Memory Management | **VERIFIED & FIXED** | Redundant buffer allocations in `DisVLWorker` on voice reconnects. |

---

## Detailed Defect Specifications

### DEF-001: Gateway ETF Parser Crash on `SMALL_TUPLE_EXT`
- **Severity**: Critical (P0)
- **Affected Component**: `Ripcord.exe` / `Ded::Step` (RVA `0xB5EF0`)
- **Reproduction**: Connect to Discord Gateway. When any gateway dispatch contains an Erlang small tuple (e.g. tag 104 `0x68`), the client crashes or closes the WebSocket.
- **Expected Behavior**: Decoder parses small tuples into a list representation and steps through children.
- **Actual Behavior**: Unhandled term tag triggers fatal parse failure.
- **Root Cause**: `Ded::Step` lacked handler for ETF tag 104 (`SMALL_TUPLE_EXT`).
- **Proposed Fix**: Hook `Ded::Step` to parse tag 104 with bounds checks, push `DEDFRAME_TUPLE_FLAG | (2 * arity)`, and emit `ListStart` and `ListEnd`.
- **Regression Test**: `tests/test_etf_parser.py::test_01_small_tuple_decoding` (PASS).
- **Implementation Status**: Implemented in `hook.c` and verified.

---

### DEF-002: Gateway Disconnect on Unknown Map Keys
- **Severity**: Critical (P0)
- **Affected Component**: `Ripcord.exe` / `ErfMap::find` (RVA `0xB9690`)
- **Reproduction**: Discord server sends an updated gateway map with unrecognized feature keys.
- **Expected Behavior**: Unknown keys are ignored; known keys are processed.
- **Actual Behavior**: Assert at RVA `0xB9BBF` fails with a non-zero branch, aborting the connection.
- **Root Cause**: Strict schema validation aborts connection when extra fields are encountered.
- **Proposed Fix**: In-memory byte patch at `0xB9BBF` changing `75` (`jnz`) to `EB 80` (`jmp +0x80`).
- **Regression Test**: `tests/test_etf_parser.py::test_05_unknown_map_key_discard_simulation` (PASS).
- **Implementation Status**: Implemented in `LoadHooks()` and verified.

---

### DEF-003: Indefinite Hang on "Routing..." During Voice Connection
- **Severity**: Blocker (P0)
- **Affected Component**: Voice Worker / `write_datagram`, `read_datagram` (RVA `0xDE8D8` - `0xDE936`)
- **Reproduction**: Attempt to join any Discord voice channel. Status stays at "Routing..." indefinitely.
- **Expected Behavior**: IP discovery packet is transmitted; server replies with public IP and port; client transitions to Connected.
- **Actual Behavior**: 70-byte packet is ignored by Discord; client times out.
- **Root Cause**: Discord requires modern 74-byte discovery packet with 4-byte header and big-endian port.
- **Proposed Fix**: Hook `write_datagram` to send 74 bytes; hook `read_datagram` to swap port endianness; patch Ripcord buffer sizes.
- **Regression Test**: `tests/test_audio_transport_rtp.py::test_04_ip_discovery_packet_construction` & `test_05_ip_discovery_reply_parsing` (PASS).
- **Implementation Status**: Implemented in `hook.c` and verified.

---

### DEF-004: Voice Disconnect Code 4016 (Deprecated `xsalsa20_poly1305`)
- **Severity**: Blocker (P0)
- **Affected Component**: `SetVoiceEncStrings` (RVA `0xCAFB0`) / `SendVoiceDatagram` / `ReadVoiceDataPacket`
- **Reproduction**: Connect to voice channel on Voice Gateway v7/v8+.
- **Expected Behavior**: Negotiate modern AEAD encryption mode.
- **Actual Behavior**: Gateway rejects `xsalsa20_poly1305` with code 4016 and closes connection.
- **Root Cause**: Discord deprecated Salsa20 modes in favor of `aead_xchacha20_poly1305_rtpsize`.
- **Proposed Fix**: Hook `SetVoiceEncStrings` to advertise `aead_xchacha20_poly1305_rtpsize`; hook `SendVoiceDatagram` and `ReadVoiceDataPacket` using libsodium AEAD encryption.
- **Regression Test**: `tests/test_voice_protocol_lifecycle.py::test_02_ready_and_session_description_handshake` (PASS).
- **Implementation Status**: Implemented in `hook.c` and verified.

---

### DEF-005: Voice Disconnect Code 4017 (Mandatory Discord DAVE E2EE)
- **Severity**: Blocker (P0)
- **Affected Component**: Voice Gateway WebSocket / `libdave` MLS Pipeline
- **Reproduction**: Connect to any DAVE-enforced voice channel.
- **Expected Behavior**: Complete MLS commit/welcome exchange and establish ratcheted audio encryption.
- **Actual Behavior**: Gateway disconnects with code 4017 ("E2EE protocol required") or voice remains silent.
- **Root Cause**: Discord requires IETF MLS (RFC 9420) DAVE protocol.
- **Proposed Fix**: Integrate `libdave.lib`, hook binary WebSocket signals, handle transitions, and encrypt/decrypt Opus frames through DAVE ratchets.
- **Regression Test**: `tests/test_voice_protocol_lifecycle.py::test_01_identify_advertises_dave_protocol` & `test_05_dave_transition_ready_ack` (PASS).
- **Implementation Status**: Implemented in `dave_*.c` and `hook.c` and verified.

---

### DEF-006: Voice Connect/Disconnect Loop Due to Missing `seq_ack`
- **Severity**: Major (P1)
- **Affected Component**: Voice Heartbeat Lambda (RVA `0xD5820`)
- **Reproduction**: Remain in a voice channel for > 15 seconds.
- **Expected Behavior**: Heartbeat timer maintains active session.
- **Actual Behavior**: WebSocket terminates with code 1001 ("Connection closed") every 15-30 seconds.
- **Root Cause**: Voice Gateway v8 requires `seq_ack` in Op 3 Heartbeats.
- **Proposed Fix**: Track sequence numbers across text and binary frames under synchronization; append `seq_ack` to Op 3 payloads.
- **Regression Test**: `tests/test_voice_protocol_lifecycle.py::test_03_heartbeat_reports_acknowledged_sequence` (PASS).
- **Implementation Status**: Implemented in `hook.c` and verified.

---

### DEF-007: Garbled and Robotic Audio Due to RTP Extension Parsing
- **Severity**: Major (P1)
- **Affected Component**: `ReadVoiceDataPacket` (RVA `0xDD850`)
- **Reproduction**: Listen to voice audio in Discord channels where participants have modern clients.
- **Expected Behavior**: Clear, undistorted Opus audio playback.
- **Actual Behavior**: Voice sounds robotic, static-filled, and garbled.
- **Root Cause**: Ripcord assumed RTP header extensions were at most 8 bytes; bytes exceeding 8 were passed into Opus decoder.
- **Proposed Fix**: Dynamically calculate extension length from the 16-bit extension length field (`extension_dwords * 4`) and skip all extension bytes.
- **Regression Test**: `tests/test_audio_transport_rtp.py::test_06_rtp_header_extension_length_calculation` (PASS).
- **Implementation Status**: Implemented in `hook.c` and verified.

---

### DEF-008: Audio Timestamp Discontinuity and Race Conditions
- **Severity**: High (P1)
- **Affected Component**: `SendVoiceData` (in `source/hook.c`)
- **Reproduction**: Unmute microphone or reconnect to a voice channel after being muted or idle.
- **Expected Behavior**: Smooth monotonic timestamp increments (960 samples per frame).
- **Actual Behavior**: Timestamp jumps by millions on initial packet (uptime offset); non-synchronized static variables race between audio thread and silence timer.
- **Root Cause**: Unsynchronized `static u32 timestamp` and uninitialized `static u64 last_send_ms` leaped by `now * 48`.
- **Proposed Fix**: Protect with `DAVE_LOCK()`, initialize cleanly on session start via `ResetVoiceTimestamp()`, and clamp silence leaps to at most 3000ms.
- **Regression Test**: `tests/test_audio_transport_rtp.py::test_03_timestamp_silence_advance_and_clamping` (PASS).
- **Implementation Status**: Hardened and verified.

---

### DEF-009: Global IPv4 Forcing Breaking Dual-Stack and IPv6 Traffic
- **Severity**: Medium (P2)
- **Affected Component**: `QAbstractSocketConnectToHostHook` (in `source/hook.c`)
- **Reproduction**: Run Ripcord on an IPv6-only network or dual-stack network accessing Slack or Discord REST APIs.
- **Expected Behavior**: Dual-stack / IPv6 sockets resolve and connect naturally.
- **Actual Behavior**: Global hook forced `IPv4Protocol` on all sockets in the application indiscriminately.
- **Root Cause**: Blanket override in `QAbstractSocket::connectToHost`.
- **Proposed Fix**: Added `IsDiscordVoiceHost()` to confine IPv4 forcing strictly to `*.discord.media` and `*.discord.gg` voice servers.
- **Regression Test**: `tests/test_network_socket_scope.py` (5 tests passing).
- **Implementation Status**: Hardened and verified.

---

### DEF-010: Broken Server Sidebar Ordering from Guild Folders
- **Severity**: Medium (P2)
- **Affected Component**: `ErfMap::find` (RVA `0xB9690`) / `UpdateUserGuildPositions` (RVA `0xF9750`)
- **Reproduction**: Account belongs to guilds organized into folders on Discord.
- **Expected Behavior**: Server icons appear in sidebar in user-defined folder order.
- **Actual Behavior**: Servers are unsorted or absent.
- **Root Cause**: Discord replaced flat `guild_positions` with nested `guild_folders`.
- **Proposed Fix**: Traverse `guild_folders`, extract `guild_ids`, and populate SQLite table `user_guild_position`.
- **Implementation Status**: Implemented in `hook.c` and verified.

---

### DEF-011: Broken Image Previews and Avatars
- **Severity**: Medium (P2)
- **Affected Component**: Image URL constructor (RVA `0x3E63F8`)
- **Reproduction**: View chat channels with user avatars or image attachments.
- **Expected Behavior**: Avatars and image attachments render inline.
- **Actual Behavior**: Images fail to load, showing blank squares.
- **Root Cause**: Outdated template URL for Discord CDN assets.
- **Proposed Fix**: Replace string template with `cdn.discordapp.com/\0\0\0`.
- **Implementation Status**: Implemented in `LoadHooks()` and verified.

---

### DEF-012: Binary Patcher Safety, Lack of Revert, and Corruption Risk
- **Severity**: Major (P1)
- **Affected Component**: Standalone CLI utility (`patcher.exe`)
- **Reproduction**: Run upstream community `patcher.exe` on modified or wrong binary.
- **Expected Behavior**: Safe execution with automatic backup, pre-validation, verification, and rollback.
- **Actual Behavior**: Modifies memory-mapped file in place without backup, lacks `--verify`, and lacks `--revert`.
- **Root Cause**: Minimalist tool lacked safety engineering.
- **Proposed Fix**: Re-architected `patcher`: added automatic `.bak` creation, `--verify`, `--revert`, `--dry-run`, PE size/header validation, and full idempotency.
- **Regression Test**: `tests/test_patcher_safety.py` (7 tests passing).
- **Implementation Status**: Compiled, verified, and released.

---

### DEF-013: Memory Leak on Reconnection in `DisVLWorker`
- **Severity**: Low (P3)
- **Affected Component**: `DisVLWorkerConstructorHook`
- **Reproduction**: Repeatedly connect and disconnect from voice channels.
- **Expected Behavior**: Packet buffer allocated once or resized safely without leaks.
- **Actual Behavior**: `malloc(2048)` repeatedly replaced existing buffer pointer without freeing.
- **Proposed Fix**: Guard allocation: reuse buffer if valid and ≥ 2048 bytes; free before reallocating.
- **Implementation Status**: Hardened and verified.
