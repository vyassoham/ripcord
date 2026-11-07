# Ripcord v0.4.29 — Next-Generation Feature Audit & Inventory

**Auditor:** Principal C++ Engineer & Desktop Application Reverse Engineer  
**Target:** Ripcord v0.4.29 (Windows x64 PE32+)  
**Baseline Hash:** `12D62ABB9AD4DB43C2B9B1398ACAE66857EB6E64205364631A3D3BDA0FF17E2E`  
**Execution Context:** `ripcord-repair` Production Hardening Environment  

---

## Executive Summary

This document establishes the comprehensive feature inventory and architectural capability audit for Ripcord v0.4.29 on Windows x64. Ripcord is a high-performance native desktop chat client written in C++ and Qt5, designed as a lightweight alternative to resource-heavy Electron clients. 

Each capability was analyzed through binary disassembly, Qt meta-object reflection, SQLite schema extraction, and live network protocol traces against modern Discord API (v9/v10) and Voice Gateway (v8).

### Classification Taxonomy
- **FULLY WORKING:** Operates correctly with modern Discord protocols and UI semantics without defects.
- **PARTIALLY WORKING:** Functional for primary workflows but exhibits edge-case defects, UI regressions, or missing protocol subfields.
- **BROKEN:** Completely fails in the unpatched binary; requires binary patching or proxy hook repair.
- **MISSING:** Feature was never implemented in Ripcord v0.4.29.
- **EXTERNALLY BLOCKED:** Feature cannot operate due to Discord API deprecation, authorization walls, or server-side restrictions.
- **UNSUPPORTED BY CURRENT ARCHITECTURE:** Feature fundamentally incompatible with Ripcord's ultra-lightweight design (e.g. Chromium WebRTC engine vs native Opus socket).

---

## Feature Audit by Domain

| ID | Domain | Feature | Classification | Technical Analysis & Evidence |
|:---|:---|:---|:---|:---|
| **01** | **Server Navigation** | Guild List & Badges | **FULLY WORKING** | Guilds loaded via REST `/users/@me/guilds` and Gateway `GUILD_CREATE`. Unread markers and mention badges correctly maintained in SQLite. |
| **01** | **Server Navigation** | Guild Folder Drag/Reorder | **PARTIALLY WORKING** | DEF-01 hook handles `guild_folders` array in `UpdateUserGuildPositionsHook`. Dragging guild icons in sidebar works locally; folder nesting has visual artifacts. |
| **02** | **Channel Navigation** | Channel List Rendering | **FULLY WORKING** | `DisGuildChannelsTreeModel` renders categories, text channels, and voice channels with mute/collapse states. |
| **02** | **Channel Navigation** | Channel Reordering | **BROKEN -> REPAIRED** | Reordering channels via drag-and-drop previously failed if user lacked `ADMINISTRATOR` due to overly strict `0x40` check. Repaired to check `MANAGE_CHANNELS` (`0x10`). |
| **03** | **Voice Channels** | Voice Connection Handshake | **BROKEN -> REPAIRED** | Baseline failed with 4001/4006 errors. Repaired via Voice Gateway v8 handshake (`max_dave_protocol_version: 1`, `video: true`), 74-byte IP discovery, and AEAD XChaCha20-Poly1305. |
| **03** | **Voice Channels** | Voice Member Drag-and-Drop | **BROKEN -> REPAIRED** | Dragging members across voice channels was disabled because `flags()` lacked `ItemIsDragEnabled` (`0x04`) and `ItemIsDropEnabled` (`0x08`), and `dropMimeData` only handled channel reorders. Repaired via `MOVE_MEMBERS` permission check and REST `PATCH /guilds/{id}/members/{id}` dispatch. |
| **03** | **Voice Channels** | Voice Region Reconnection | **BROKEN -> REPAIRED** | Server voice region swaps (`VOICE_SERVER_UPDATE`) froze client on "Routing...". Repaired by tearing down stale socket/timers, resetting SSRC mappings and DAVE state, and initiating clean Gateway v8 reconnect. |
| **04** | **Text Channels** | Message History & Pagination | **FULLY WORKING** | SQLite-backed cache renders historical messages rapidly with bidirectional pagination (`before`/`after`). |
| **04** | **Text Channels** | File Upload Drag-and-Drop | **BROKEN -> REPAIRED** | Dropping files from Windows Explorer (`CF_HDROP` / `text/uri-list`) into chat input was ignored. Repaired by hooking `dragEnterEvent`/`dropEvent` to trigger `DisFileUploadDialog`. |
| **05** | **Member Lists** | Guild Roster & Role Groups | **FULLY WORKING** | `DisGuildMembersList` categorizes members by highest hoisted role with presence color coding and status icons. |
| **05** | **Member Lists** | Large Guild Chunking | **FULLY WORKING** | Issues `Op 8 Request Guild Members` for large servers (`large_threshold: 250`). |
| **06** | **User Profiles** | Mini-Profile & Badges | **PARTIALLY WORKING** | Shows avatar, username, discriminator, status, mutual servers. Missing modern Discord decoration assets, pronouns, and custom banner images. |
| **07** | **Context Menus** | Message Context Menu | **FULLY WORKING** | Copy ID, Copy Text, Delete, Edit, Pin/Unpin, Reply, Mark Unread, Add Reaction. |
| **07** | **Context Menus** | Voice User Context Menu | **FULLY WORKING** | User Volume slider (0–200%), Server Mute/Deafen (with permission), Kick, Ban. |
| **08** | **Drag-and-Drop** | Channel Tree Sorting | **BROKEN -> REPAIRED** | `application/x-channelidlist` MIME model parsing restored with proper permission filtering. |
| **08** | **Drag-and-Drop** | Voice User Repositioning | **BROKEN -> REPAIRED** | Implemented through MIME type `application/x-discord-voicemember` and REST patcher. |
| **09** | **Notifications** | Windows Native Toast / Tray | **FULLY WORKING** | Uses Windows `Shell_NotifyIconW` with sound notification. Unread counts badge in notification tray icon. |
| **10** | **Search** | Channel & Server Text Search | **PARTIALLY WORKING** | Local SQLite search is instant. Server-side search API (`/guilds/{id}/messages/search`) is partially supported for text queries; advanced filters (`from:`, `has:`, `before:`) not fully parsed. |
| **11** | **Settings** | Audio, Network, Themes | **FULLY WORKING** | Extensive JSON-backed preferences with live theme editor and CSS stylesheet overrides. |
| **12** | **Audio Devices** | Live Device Switching | **BROKEN -> REPAIRED** | Baseline required application restart ("Changes will take effect after restarting..."). Repaired to dynamically re-bind `QAudioInput`/`QAudioOutput` WASAPI streams without terminating voice session. |
| **13** | **Attachments** | Upload File & Image Previews | **FULLY WORKING** | Multipart POST `/channels/{id}/messages` works for attachments up to server limit (25MB/50MB). |
| **14** | **Image & Video Rendering** | Inline Image Previews | **BROKEN -> REPAIRED** | Baseline failed to load Discord images due to `cdn.discordapp.com` URL rewriting bug. Repaired via memory string patch at `0x3E63F8`. |
| **14** | **Image & Video Rendering** | Animated GIF / WebP | **FULLY WORKING** | Uses Qt `QImageReader` and `QMovie` with disk cache for smooth playback. |
| **15** | **Message Interactions** | Reactions Display & Add | **FULLY WORKING** | SQLite table `message_emoji_reaction_count` correctly tracks Unicode and custom guild emoji reactions. |
| **15** | **Message Interactions** | Inline Replies | **PARTIALLY WORKING** | Baseline implemented blockquote replies (`> text`). Repaired to send Discord API `message_reference: {message_id: "..."}` while maintaining backward compatibility. |
| **15** | **Message Interactions** | Message Pinning | **FULLY WORKING** | `DisPinnedMessages` model synchronizes pins via `/channels/{id}/pins`. |
| **16** | **Gateway Connectivity** | Heartbeat & Sequence Track | **FULLY WORKING** | Heartbeats acknowledge latest incoming sequence (`seq_ack`). Handles ETF zlib stream resets. |
| **16** | **Gateway Connectivity** | Session Resume vs Re-Identify| **FULLY WORKING** | Sends `Op 6 Resume` upon reconnection. If Gateway responds with `Op 9 Invalid Session` (`d: false`), waits 1.5s jittered backoff and gracefully transitions to `Op 2 Identify`. |
| **17** | **Voice Connectivity** | Audio Monotonic Timestamps | **BROKEN -> REPAIRED** | Unpatched client experienced initial burst/skips. Assembly cave at `0x266BF` enforces 960 frame stepping and clamps long silence leaps to 3000ms. |
| **17** | **Voice Connectivity** | DAVE E2EE / MLS Protocol | **FULLY WORKING** | Supports DAVE protocol version 1 with MLS Welcome, Group Commit, and Key Ratchet assignment. |
| **18** | **Account Lifecycle** | Multi-Account & Switching | **FULLY WORKING** | Supports simultaneous Discord and Slack account sessions in independent tabs. |
| **18** | **Video & Screen Sharing** | Video Receive / Transmit | **UNSUPPORTED** | WebRTC video pipeline, VP8/H.264 codecs, and RTX packet buffers incompatible with Ripcord's lightweight audio-only architecture. |

---

## Detailed Architectural Gap Analysis

### 1. Voice Member Drag-and-Drop Failure Mechanism
- **Model:** `DisGuildChannelsTreeModel` inherits `QAbstractItemModel`.
- **Root Cause:**
  1. `flags(QModelIndex)` at `0x140110ED0` checked `0x40(%rsi)` (which only represented `MANAGE_CHANNELS` or `ADMINISTRATOR`). If false, it returned `0x21` (`ItemIsEnabled | ItemIsSelectable`) instead of enabling `ItemIsDragEnabled` (`0x04`) or `ItemIsDropEnabled` (`0x08`).
  2. Voice member indices (child rows under a voice channel with `0x8(%r14) == 3`) were explicitly excluded from dragging.
  3. `mimeData()` only serialized `application/x-channelidlist`.
  4. `dropMimeData()` only handled category and channel repositioning via `0x140112640`.
- **Resolution:**
  - Introduce hook on `DisGuildChannelsTreeModel::flags` allowing drag on voice members and drop on voice channels when user possesses `MOVE_MEMBERS` (`0x800000`) or `ADMINISTRATOR` (`0x08`).
  - Register custom MIME data `application/x-discord-voicemember` packing `(guild_id, user_id, source_channel_id)`.
  - In `dropMimeData`, detect `application/x-discord-voicemember` and dispatch asynchronous REST `PATCH /api/v9/guilds/{guild_id}/members/{user_id}` with body `{"channel_id": "{target_channel_id}"}`.

### 2. Voice Server Region Migration & Hang
- **Root Cause:**
  - When Discord changes voice endpoints during active calls (load balancing or server admin region swap), Gateway emits `VOICE_SERVER_UPDATE` with a new `endpoint` and `token`.
  - Unpatched Ripcord initialized a new `DisVLWorker` while the existing UDP socket remained bound to the old IP/port and the old heartbeat timer continued firing.
  - SSRC mappings and DAVE encryption state became desynchronized, leaving the UI stuck on "Routing..." or with one-way silence.
- **Resolution:**
  - Ensure `DisVLWorker` destructor or re-init terminates active UDP socket immediately.
  - Call `DaveReset()` to destroy stale MLS sessions and reset `g_dave` state.
  - Reset voice frame sequence and invoke `ResetVoiceTimestamp()`.
  - Re-run 74-byte UDP IP discovery and AEAD negotiation on the new voice host.

### 3. File Upload Drag-and-Drop
- **Root Cause:**
  - `DisChatInputTextEdit` is a subclass of `QPlainTextEdit`. While it accepted image pasting from clipboard via `pastingWithUploadableMimeData`, its `dragEnterEvent` and `dropEvent` defaulted to standard text insertion, failing when files were dragged from Windows Explorer.
- **Resolution:**
  - Hook widget `dragEnterEvent` to check `event->mimeData()->hasUrls()` and accept proposed action if local files exist (`url.isLocalFile()`).
  - On `dropEvent`, parse file paths from `urls()` and invoke `DisFileUploadDialog` or attach files directly to the active chat buffer.

---

## Conclusion

Ripcord's core C++ and Qt engine remains robust, lean, and fast. The compatibility defects stem from Discord protocol evolution (Voice Gateway v8, AEAD encryption, DAVE MLS, API v9) and incomplete permission/interaction handling in Qt models. With the fixes implemented and verified in this project, Ripcord v0.4.29 achieves exceptional stability and modern Discord compatibility while retaining its signature low memory footprint.
