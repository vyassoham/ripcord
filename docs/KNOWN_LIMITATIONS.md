# Ripcord Known Limitations & Upstream Constraints

This document details known limitations, architectural boundaries, and unsupported features resulting from the closed-source nature of Ripcord 0.4.29 and upstream Discord/Slack API evolutions.

---

## 1. Discord Architectural Limitations

### 1.1 Closed-Source Binary Boundaries
- **Proprietary Executable**: The original C++/Qt source code of Ripcord is not public. Any changes requiring deep UI redesign, internal threading rewrites, or database schema migrations beyond what can be safely hooked in memory or patched in PE assembly cannot be implemented without decompilation and complete client reimplementation.
- **Fixed Qt 5.12 Toolchain**: Ripcord 0.4.29 was compiled against Qt 5.12.1 and OpenSSL 1.0.2. While the application runs reliably on Windows 10 and 11, it relies on legacy OpenSSL runtime libraries.

### 1.2 Stage Channels
- **Current Support**: The community patch converts Stage channels (`DisChannelType_GuildVoiceStage`) to standard voice channels (`DisChannelType_GuildVoice`) within `ErfMapFindHook`.
- **Limitation**: Users can join Stage channels as listeners like regular voice channels. However, there is no UI or protocol mechanism to request to speak, become a stage speaker, or moderate stage permissions. Stage channels appear identical to normal voice channels in the UI.

### 1.3 Discord Forum and Media Channels
- **Limitation**: Discord introduced Forum channels (thread-only channels) and Media channels after Ripcord 0.4.29 was released.
- **Behavior**: Forum channels appear as text channels, but thread browsing, tagging, and forum layout cards are not natively supported. Threads are rendered as standalone sub-channels or message streams.

### 1.4 Video Streaming & Screen Share
- **Limitation**: Video receiving and screen sharing sending/receiving are not supported.
- **Technical Context**: While Voice Identify sends `"video": true` to satisfy modern gateway requirements, Ripcord lacks WebRTC H.264/VP8/VP9 video decoders and DirectX/OpenGL video surface widgets. Ripcord remains strictly an audio-only chat client.

### 1.5 Discord Reactions, Stickers & Super Reactions
- **Limitation**: Standard Unicode and custom server emoji reactions are partially displayed. Newer rich media features such as animated stickers (Lottie/APNG) and Super Reactions are rendered as plain text labels or fallback asset links.

---

## 2. Slack Protocol Limitations

### 2.1 Slack Multipart Direct Messages (`#mpdm`)
- **Limitation**: As noted in official Cancel.fm development tickets, channel sidebars may occasionally show cluttered entries named `#mpdm-user1--user2--1` resulting from server-side Slack schema adjustments.
- **Workaround**: Group DMs can be closed or bookmarked manually in the channel sidebar.

### 2.2 Slack Enterprise Grid & SSO
- **Limitation**: Workspaces requiring corporate SAML / Okta Single Sign-On (SSO) with hardware tokens (FIDO2/WebAuthn) cannot be authenticated via Ripcord's built-in login dialog.
- **Workaround**: Users must authenticate via standard browser, extract the `d` session cookie, and import it into Ripcord's manual account configuration.

---

## 3. Network & Transport Constraints

### 3.1 Discord Voice IPv6 Routing
- **Limitation**: Discord voice media infrastructure (`*.discord.media`) frequently experiences severe packet routing and UDP discovery dropouts over native IPv6 on certain residential ISPs.
- **Solution**: The repair selectively enforces IPv4 on connections to `*.discord.media` and `*.discord.gg` via `IsDiscordVoiceHost()`, while preserving native IPv6 for all REST APIs, Slack connections, and CDN traffic.

### 3.2 Offline Binary Patcher vs. Runtime Hook Scope
- **Limitation**: The standalone `patcher.exe` modifies bytes statically on disk. It can toggle flags like deleted message retention and slowmode cooldown bypass. However, it **cannot** implement complex protocols like DAVE E2EE MLS or libsodium AEAD encryption without the companion `winmm.dll` runtime hook.
- **Recommendation**: For full voice and gateway connectivity on modern Discord, `winmm.dll` must be present in the application folder.
