# Ripcord Security & Privacy Audit Report

## 1. Audit Scope & Objectives

This security audit inspects the repaired Ripcord desktop client ecosystem on Windows x64. The evaluation covers:
1. Credential management, storage security, and secret leakage prevention.
2. Binary exploit mitigations (DEP, ASLR, HighEntropyVA).
3. Transport layer security (TLS, certificate validation, HTTPS enforcement).
4. Media encryption and cryptographic integrity (DAVE E2EE, AEAD XChaCha20-Poly1305).
5. Update mechanisms, supply chain considerations, and network privacy.

---

## 2. Findings & Verification

### 2.1 Credential & Token Hygiene
- **Storage Location**: `%LOCALAPPDATA%\Ripcord\ripcord_accounts.bin`.
- **Finding**: User credentials, Discord authentication tokens, and Slack session cookies are serialized into a binary state file within the user's protected local profile.
- **Log Audit**: Inspected `ripcord.log` generation in `source/hook.c`, `source/dave_*.c`, and `Ripcord.exe`.
  - All token arguments (e.g. `connToken`) are explicitly masked or formatted into network buffers without passing into diagnostic loggers (`DebugMsg`, `ErrorMessage`).
  - Voice session secret keys (`secretKey`) are held in memory buffers and never written to disk or logs.
  - Automated test `tests/test_security_audit.py::test_05_no_token_or_credential_leakage_in_hook` confirms zero secret leakage.

### 2.2 Binary Security Mitigations (PE Characteristics)
Using static PE header analysis, the executables and libraries were checked for modern Windows exploit mitigations:

| Binary Artifact | 64-bit PE | Data Execution Prevention (DEP / NX) | ASLR (DynamicBase) | High Entropy VA (64-bit ASLR) |
| :--- | :--- | :--- | :--- | :--- |
| **`Ripcord.exe` (Baseline)** | Yes (PE32+) | **Enabled** (`0x0100`) | **Enabled** (`0x0040`) | **Enabled** (`0x0020`) |
| **`winmm.dll` (Hook)** | Yes (PE32+) | **Enabled** (`0x0100`) | **Enabled** (`0x0040`) | **Enabled** (`0x0020`) |
| **`patcher.exe` (Hardened)**| Yes (PE32+) | **Enabled** (`0x0100`) | **Enabled** (`0x0040`) | **Enabled** (`0x0020`) |

- **DEP / NX Compatibility**: Prevents arbitrary code execution in data and heap pages.
- **ASLR (Address Space Layout Randomization)**: Randomizes the base address of the executables and DLLs in virtual memory on each execution.
- **High Entropy VA**: Enables the full 64-bit address space (48-bit addressable range) for ASLR, mitigating memory corruption and ROP techniques.

### 2.3 Transport Security & Certificate Validation
- **TLS Configuration**: Handled through Qt5 (`Qt5Network.dll`) and OpenSSL (`libeay32.dll` / `ssleay32.dll`).
- **Certificate Validation**: All REST HTTPS requests (`https://discord.com`, `https://slack.com`) and Gateway WebSockets (`wss://gateway.discord.gg`, `wss://*.discord.media`) utilize standard system certificate validation. No certificate authority bypasses, date-check bypasses, or name-mismatch overrides are present.
- **Update Checker**: Located in `source/update.c`. Uses Windows HTTP Services (`WinHttpOpen`, `WinHttpConnect`, `WinHttpOpenRequest`).
  - Connection flags: `WINHTTP_FLAG_SECURE` is mandated.
  - Port: `INTERNET_DEFAULT_HTTPS_PORT` (Port 443).
  - Flags that disable SSL certificate verification (`SECURITY_FLAG_IGNORE_UNKNOWN_CA`, `SECURITY_FLAG_IGNORE_CERT_DATE_INVALID`, `SECURITY_FLAG_IGNORE_CERT_CN_INVALID`) are completely absent.

### 2.4 Cryptographic Strength & Audio Encryption
- **Transport Encryption**:
  - Deprecated ciphers (`xsalsa20_poly1305`) are eliminated.
  - Mandated cipher: `aead_xchacha20_poly1305_rtpsize` via `libsodium.dll`. Provides 256-bit encryption and 128-bit Poly1305 MAC authentication with 192-bit extended nonces, preventing nonce-reuse collisions.
- **End-to-End Encryption (E2EE)**:
  - Protocol: Discord DAVE (based on IETF MLS RFC 9420).
  - Outbound Opus audio is encrypted using MLS ratcheted keys before transmission.
  - Inbound DAVE frames (`0xFAFA`) are verified and decrypted with participant-specific ratchets.
  - Plaintext fallback is explicitly rejected: packets lacking valid encryption keys are dropped before hitting the network socket.

### 2.5 Supply Chain & Dependency Versions
- **Libsodium**: Embedded DLL v1.0.16 (ISC license).
- **Opus**: Embedded DLL v1.2.1 (BSD license).
- **Qt**: 5.12.1 MinGW/MSVC (LGPLv3/Commercial).
- **MinHook**: 1.3.3 (BSD license).
- **libdave**: 1.2.1 (Discord open source, MLS protocol engine).
- **OpenSSL**: 1.0.2u.
  - *Advisory Note*: OpenSSL 1.0.2 is end-of-life upstream. Because Ripcord links dynamically against `libeay32.dll` and `ssleay32.dll` for QtNetwork TLS, future enterprise hardening could explore drop-in OpenSSL 1.1/3.0 ABI shims.

---

## 3. Compliance Summary

| Security Criterion | Requirement | Result |
| :--- | :--- | :--- |
| **No Credential Exfiltration** | Secrets must never be transmitted to unauthorized third parties or logged | **Compliant** |
| **No Security Disabling** | Do not disable TLS cert checks, authentication, or firewalls | **Compliant** |
| **Safe Binary Modification** | Non-destructive patching, pre-validation, automatic backup | **Compliant** |
| **Native Exploit Defense** | ASLR, DEP/NX enabled across all deliverables | **Compliant** |
| **Cryptographic Standards** | Mandatory AEAD and MLS E2EE, no cleartext media | **Compliant** |
