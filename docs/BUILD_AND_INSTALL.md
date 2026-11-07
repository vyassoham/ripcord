# Ripcord Windows x64 Build, Installation & Hardening Guide

## Overview

This guide describes how to build, test, install, and verify the repaired Ripcord desktop chat client on Windows x64.

---

## 1. Prerequisites & Toolchain Requirements

To compile the hardened patcher, run the test suites, and build release packages, the following tools are supported on Windows:

- **Operating System**: Windows 10 or Windows 11 (64-bit).
- **C/C++ Compiler**:
  - MinGW-W64 GCC 13.2.0 or newer (`C:\MinGW\bin\gcc.exe`) or Clang 16+
  - Optional for `libdave`: MSVC 2019/2022 x64 Native Tools (`cl.exe`, `vcvarsall.bat x64`)
- **Python**: Python 3.10+ (tested with Python 3.15).
- **Git**: Git for Windows 2.40+.

---

## 2. Directory Structure

```
ripcord-repair/
├── dist/                              # Assembled production release artifacts
│   ├── Ripcord.exe                    # Verified Ripcord 0.4.29 x64 binary
│   ├── winmm.dll                      # Hardened Community Fix hook DLL
│   ├── patcher.exe                    # Hardened safe binary patcher
│   ├── SHA256SUMS.txt                 # Cryptographic checksum manifest
│   ├── Qt5*.dll, opus.dll, etc.       # Runtime dynamic libraries
│   ├── platforms/                     # Qt Windows platform plugin
│   ├── translations/                  # Ripcord localization databases
│   └── twemoji.ripdb                  # Emoji database
├── original_baseline_dist/            # Untouched original distribution backup
├── patcher/                           # Hardened patcher source code & build
│   └── source/                        # C source files (main.c, patches.c, etc.)
├── ripcord-community-fix/             # Hook DLL source code (repair branch)
│   └── source/                        # C source files (hook.c, dave_*.c, etc.)
├── tests/                             # Automated test suite (34 test cases)
│   ├── test_patcher_safety.py
│   ├── test_etf_parser.py
│   ├── test_voice_protocol_lifecycle.py
│   ├── test_audio_transport_rtp.py
│   ├── test_network_socket_scope.py
│   ├── test_security_audit.py
│   └── run_all_tests.py
├── docs/                              # Technical documentation
├── build_release.py                   # Automated build & release packager
└── CHANGELOG.md                       # Release notes and changelog
```

---

## 3. Step-by-Step Build Instructions

### Step 3.1: Build the Hardened Binary Patcher
To compile the standalone `patcher.exe`:
```powershell
$env:Path = "C:\MinGW\bin;" + $env:Path
cd C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\patcher
gcc -O2 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -I"source" source/win32/build.c -o patcher.exe
```

### Step 3.2: Run the Verification Test Suite
Run all 34 automated unit, regression, and security tests:
```powershell
python C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\tests\run_all_tests.py
```

### Step 3.3: Build & Package the Full Release Distribution
Execute the automated release script:
```powershell
python C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\build_release.py
```
This automatically compiles `patcher.exe`, runs the full test suite, populates `dist/`, and generates `dist/SHA256SUMS.txt`.

---

## 4. Installation & Deployment Options

You have two complementary deployment options depending on whether you want dynamic runtime hooking or offline binary modifications:

### Option A: Dynamic Runtime Compatibility (Recommended)
Place `winmm.dll` directly into the Ripcord directory alongside `Ripcord.exe`:
1. Copy `dist\winmm.dll` into your Ripcord application directory (e.g. `D:\Ripcord_Win_0.4.29\`).
2. When `Ripcord.exe` starts, Windows automatically loads `winmm.dll`.
3. `winmm.dll` forwards all 237 multimedia functions to the real `C:\Windows\System32\winmm.dll` and applies runtime hooks:
   - Voice DAVE E2EE and AEAD XChaCha20 encryption.
   - Gateway ETF map and `SMALL_TUPLE_EXT` parsing.
   - Image previews and avatar loading.
   - 74-byte IP discovery.
   - Scoped IPv4 routing for voice servers.
4. The original `Ripcord.exe` remains completely unmodified on disk.

### Option B: Offline Binary Patching
Use the hardened `patcher.exe` to inspect or patch `Ripcord.exe`:

- **Verify Patch Status** (Non-destructive):
  ```powershell
  .\patcher.exe -v "D:\Ripcord_Win_0.4.29\Ripcord.exe"
  ```
- **Simulate Patching** (Dry Run):
  ```powershell
  .\patcher.exe --dry-run "D:\Ripcord_Win_0.4.29\Ripcord.exe"
  ```
- **Apply Patches with Automatic Backup**:
  ```powershell
  .\patcher.exe "D:\Ripcord_Win_0.4.29\Ripcord.exe"
  ```
  *(Creates `Ripcord.exe.bak` automatically before writing changes).*
- **Revert Patches Back to Clean Original**:
  ```powershell
  .\patcher.exe -u "D:\Ripcord_Win_0.4.29\Ripcord.exe"
  ```

---

## 5. Artifact Verification & Checksums

Always verify the SHA-256 checksums of the release artifacts against `dist/SHA256SUMS.txt`:

```powershell
Get-FileHash -Path "C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\dist\Ripcord.exe", `
                   "C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\dist\patcher.exe", `
                   "C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\dist\winmm.dll" -Algorithm SHA256
```

Expected Hashes:
- `Ripcord.exe`: `12D62ABB9AD4DB43C2B9B1398ACAE66857EB6E64205364631A3D3BDA0FF17E2E`
- `patcher.exe`: `0E26DEF129E86DCBAADD65AD05166FA9580BC7B0B47771899F056D10462A280C`
- `winmm.dll`: `CC26D583A44D7C17091DCC3D839347DEA6F3115FC182502EC4A0BE528A494B5D`
