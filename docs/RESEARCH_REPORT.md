# Ripcord Deep Technical Research & Reverse Engineering Report

## Executive Summary

Ripcord is a lightweight desktop chat client for Discord and Slack developed by Cancel (`cancel.fm`). The client was engineered as a high-performance native desktop application utilizing C++ and Qt 5 (specifically Qt 5.12). The latest official release is **v0.4.29**, released on June 4, 2021. The source code is proprietary and closed-source; official development has ceased.

Because Discord continuously evolves its HTTP REST API, Gateway WebSocket protocols, and Voice UDP/WebRTC transport architectures, unpatched Ripcord 0.4.29 suffers from fatal connection, parsing, and audio failures. 

This research report documents the technical investigation, reverse-engineered binary structures, network protocol traces, and root causes of defects impacting Ripcord on Windows x64.

---

## 1. Project Identification & Ecosystem Mapping

| Component | Upstream Source / Reference | Nature | Status & Role |
| :--- | :--- | :--- | :--- |
| **Official Ripcord Client** | `https://cancel.fm/ripcord/` | Closed-source C++/Qt5 PE x64 binary | Version 0.4.29 (build date 2021-06-05). Target of repair. |
| **Official Bug Tracker** | `https://dev.cancel.fm/` | Fossil issue tracker | Historical tickets (registration disabled). |
| **Community Compatibility Fix** | `https://github.com/geniiii/ripcord-community-fix` | Open-source C/C++ proxy DLL (`winmm.dll`) | Runtime hook using MinHook; patches in-memory code, implements DAVE E2EE via `libdave`. |
| **Community Binary Patcher** | `https://github.com/geniiii/patcher` | Open-source C CLI utility | Offline PE binary patcher for specific flags (e.g. deleted messages, slowmode bypass). |
| **Community Issue Tracker** | `https://github.com/geniiii/ripcord-community-fix/issues` | GitHub Issues | Active issue tracker documenting real-world breakages. |

---

## 2. Gateway Handshake & Protocol Incompatibilities

### 2.1 Discord Erlang External Term Format (ETF) Parser Failures
* **Affected Component**: `Ripcord.exe` internal ETF decoder (`Ded::Step` at RVA `0xB5EF0` and `ErfMap::find` at RVA `0xB9690`).
* **Protocol Context**: Ripcord connects to Discord Gateway using ETF encoding:
  `wss://gateway.discord.gg/?encoding=etf&v=6&compress=zlib-stream`
* **Defect 1: Unhandled Map Keys Abort**:
  In earlier Discord versions, payload maps contained fixed key schemas. When Discord introduced new features (such as application directory metadata, guild features, and experiments), new string keys appeared in Gateway maps. Ripcord's `ErfMap` parser was compiled with an assert/abort check at `0xB9BBF`: if an unknown map key was encountered, the parser aborted the connection, dropping the gateway.
  * *Confirmed Fix*: Byte patch at RVA `0xB9BBF` changing `75` (`jnz`) to `EB 80` (`jmp +0x80`), discarding unknown keys and allowing parsing to continue.
* **Defect 2: Missing `SMALL_TUPLE_EXT` (Tag 104 / `0x68`) Support**:
  In Erlang ETF, tuples with arity ≤ 255 are encoded with tag `104` (`SMALL_TUPLE_EXT`), consisting of `[104, arity, element_1, ...]`. Ripcord's `Ded::Step` handled lists (`108`), maps (`116`), atoms (`100`/`115`), and integers, but omitted tag 104. When Discord returned small tuples in gateway events, Ripcord failed with an unrecognized term type error and terminated the session.
  * *Confirmed Fix*: Hook `Ded::Step` (RVA `0xB5EF0`) to intercept tag 104, emit a `ListStart` event with the tuple's arity, set a tuple flag (`DEDFRAME_TUPLE_FLAG = 0x8000000`), step through its elements, and emit `ListEnd`.

### 2.2 Server Sidebar Ordering and Guild Folders
* **Affected Component**: `UpdateUserGuildPositions` (RVA `0xF9750`).
* **Symptom**: Server icons in the sidebar appeared in arbitrary order or disappeared entirely.
* **Cause**: Discord migrated user settings from a flat `guild_positions` array to nested `guild_folders`, where each folder contains a `guild_ids` array. Ripcord failed to unpack nested folders, leaving SQLite table `user_guild_position` empty.
* **Fix**: Hook `ErfMap::find` to detect `guild_positions` requests, recursively traverse `guild_folders`, flatten all contained `guild_ids`, and execute parameterized SQL transactions (`replace into user_guild_position values (?, ?, ?)`) on Ripcord's internal database.

### 2.3 Image and CDN Preview Failures
* **Affected Component**: Asset URL generator at RVA `0x3E63F8`.
* **Symptom**: User avatars, server icons, and attachment thumbnails failed to load.
* **Cause**: Ripcord hardcoded Discord's legacy CDN URL format. Discord updated CDN path requirements and query parameters.
* **Fix**: In-memory string patch replacing the base template at `0x3E63F8` with `cdn.discordapp.com/\0\0\0`.

---

## 3. Voice Signaling & Transport Layer Incompatibilities

### 3.1 UDP IP Discovery Packet Protocol Mismatch ("Routing..." Hang)
* **Affected Component**: `QUdpSocket` datagram I/O and Ripcord voice worker packet parser (`0xDE8D8` - `0xDE936`).
* **Symptom**: Client hangs indefinitely displaying "Routing..." upon entering any voice channel.
* **Root Cause**:
  1. *Request Format*: Legacy Discord IP Discovery used a 70-byte packet. Discord Voice servers migrated to a 74-byte packet structure:
     `[0x00, 0x01 (Request), 0x00, 0x46 (Length 70), SSRC (4 bytes big-endian), 66 bytes of zero padding]`.
     Ripcord emitted a 70-byte packet lacking the 4-byte header.
  2. *Response Format*: Discord Voice servers reply with a 74-byte packet:
     `[0x00, 0x02 (Response), 0x00, 0x46 (Length 70), IP String (null-terminated), Port (bytes 72-73, Big-Endian)]`.
     Ripcord expected little-endian port order and smaller packet size checks.
* **Fix**:
  - Hook `QUdpSocket::writeDatagram` to intercept 70-byte packets, prepend `0x00, 0x01, 0x00, 0x46`, shift SSRC, and send 74 bytes.
  - Hook `QUdpSocket::readDatagram` to intercept 74-byte discovery replies and swap endianness of bytes 72-73.
  - Patch packet size constraints in `Ripcord.exe` at RVAs `0xDE8D8`, `0xDE8EA`, `0xDE90C`, `0xDE936`.

### 3.2 Voice Transport Encryption: Deprecation of `xsalsa20_poly1305`
* **Affected Component**: Voice encryption mode negotiation (`SetVoiceEncStrings` at RVA `0xCAFB0`) and RTP crypto routines.
* **Symptom**: Disconnection with error code 4016 when establishing voice session.
* **Root Cause**: Discord completely deprecated legacy `xsalsa20_poly1305` and `xsalsa20_poly1305_suffix` encryption modes. Discord Voice Gateway v7/v8+ strictly requires Authenticated Encryption with Associated Data (AEAD):
  - `aead_xchacha20_poly1305_rtpsize` (Mandatory baseline)
  - `aead_aes256_gcm_rtpsize`
* **Fix**:
  - Hook `SetVoiceEncStrings` to substitute `aead_xchacha20_poly1305_rtpsize`.
  - Hook outbound voice datagram transmission (`SendVoiceDatagram` at `0xE2750`) to encrypt RTP payload using libsodium's `crypto_aead_xchacha20poly1305_ietf_encrypt`, passing the 12-byte RTP header as Associated Data (AAD), appending a 4-byte 32-bit incremental nonce suffix.
  - Hook inbound voice datagram reception (`ReadVoiceDataPacket` at `0xDD850`) to decrypt using `crypto_aead_xchacha20poly1305_ietf_decrypt`.

### 3.3 Discord DAVE (End-to-End Encryption - MLS) Enforcement
* **Affected Component**: Voice Gateway WebSocket and Opus audio pipeline.
* **Symptom**: Disconnection with code 4017 ("E2EE/DAVE protocol required") or complete audio silence.
* **Root Cause**: In 2024, Discord deployed DAVE (Discord Audio/Video End-to-End Encryption), based on the IETF Messaging Layer Security (MLS) protocol (RFC 9420).
  - Voice WebSocket connections negotiate DAVE via `max_dave_protocol_version: 1` in Identify (Op 0).
  - Binary WebSocket messages transmit MLS Key Packages, External Senders, Proposals, Commits, and Welcomes (Opcodes 25–30).
  - Outbound audio frames must be encrypted through an MLS key ratchet before the transport AEAD layer. Inbound frames ending with DAVE magic bytes (`0xFAFA`) must be decrypted through per-SSRC key ratchets.
* **Fix**:
  - Integrated Discord's official `libdave` C++ MLS library.
  - Subscribed to binary WebSocket messages via Qt5 dynamic slot connection.
  - Implemented state machine transitions for `VOICE_OP_DAVE_PREPARE_TRANSITION` (Op 21), `VOICE_OP_DAVE_EXECUTE_TRANSITION` (Op 22), and `VOICE_OP_DAVE_TRANSITION_READY` (Op 23).
  - Chained DAVE frame encryptor and per-SSRC decryptors directly into Ripcord's audio processing pipeline.

### 3.4 Voice Gateway v8+ Heartbeat Sequence Tracking (`seq_ack`)
* **Affected Component**: Voice heartbeat timer lambda (RVA `0xD5820`).
* **Symptom**: Voice session drops exactly every 10–30 seconds with WebSocket code 1001 ("Connection closed").
* **Root Cause**: Starting with Voice Gateway v8, Discord requires clients to acknowledge received gateway sequence numbers in Heartbeats (Op 3). The payload must contain `{ "op": 3, "d": { "t": <timestamp_ms>, "seq_ack": <last_seq> } }`.
  Binary WebSocket messages also advance sequence numbers (encoded in the first 2 bytes). Unpatched Ripcord sent legacy heartbeats without `seq_ack`, causing Discord to drop the socket for unresponsive acknowledgment.
* **Fix**: Intercept voice gateway text and binary frames, track `last_seq` under synchronization, and construct Op 3 payloads containing valid `seq_ack`.

### 3.5 RTP Header Extension Parsing & Audio Distortion ("Garbled / Robotic Voice")
* **Affected Component**: Inbound RTP packet decoding (`ReadVoiceDataPacket` at RVA `0xDD850`).
* **Symptom**: Inbound voice sounds like metallic static or high-pitched garble.
* **Root Cause**: Unpatched Ripcord hardcoded an assumption that RTP header extensions (RFC 5285 / 8285) were at most 8 bytes long. When Discord expanded extension sizes (e.g. 12, 16, or 24 bytes for voice levels and client IDs), Ripcord skipped only 8 bytes and passed the remaining extension header bytes directly into the Opus decoder, corrupting the decoded PCM audio.
* **Fix**: Dynamically inspect the RTP extension header, read the 16-bit length field (`extension_dwords * 4`), compute total extension offset, and skip the exact extension length prior to Opus decoding.

---

## 4. Reverse Engineering Methodology & Evidence

| Artifact / Binary | SHA-256 Checksum | Architecture | Toolchain / Origin |
| :--- | :--- | :--- | :--- |
| **`Ripcord.exe` (Official Baseline)** | `12D62ABB9AD4DB43C2B9B1398ACAE66857EB6E64205364631A3D3BDA0FF17E2E` | PE32+ (x86_64) | MSVC 2017/2019, Qt 5.12.1 |
| **`winmm.dll` (Community Fix)** | `CC26D583A44D7C17091DCC3D839347DEA6F3115FC182502EC4A0BE528A494B5D` | PE32+ (x86_64) DLL | MSVC x64, MinHook, libdave |
| **`patcher.exe` (Hardened Release)** | `0E26DEF129E86DCBAADD65AD05166FA9580BC7B0B47771899F056D10462A280C` | PE32+ (x86_64) | MinGW-W64 GCC 13.2.0 |

All investigations were performed using non-destructive static analysis, PE header inspection, memory pattern analysis, and runtime log correlation.
