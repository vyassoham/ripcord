# RIPCORD PRODUCTION ROLLBACK & RESTORATION MANUAL

**Target**: Ripcord v0.4.29 Windows x64  
**Workspace**: `C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair`  
**Official Baseline SHA-256**: `12D62ABB9AD4DB43C2B9B1398ACAE66857EB6E64205364631A3D3BDA0FF17E2E`  
**Patched Binary SHA-256**: `D37B9FC412B684EC0741F53354BF6CC839BD9F4BBECE8754B7E46E96012255B4`  

---

## 1. Rollback Overview

The Ripcord repair and production hardening framework is architected for complete, non-destructive reversibility. Modifications consist of:
1. **Dynamic Sidecar Hooking (`winmm.dll`)**: In-memory API hooks applied at process launch.
2. **Static Byte Patches (`patcher.exe`)**: Reversible byte modifications in `Ripcord.exe`.

Both components can be rolled back to restore the unpatched official application baseline at any time.

---

## 2. Procedure A: Reverting Static Binary Patches

The hardened `patcher.exe` utility includes built-in cryptographic rollback functionality (`-u` / `--revert`).

### Step-by-Step Reversion:
1. Open PowerShell and navigate to the Ripcord installation directory (e.g. `D:\Ripcord_Win_0.4.29` or `dist/`):
   ```powershell
   cd D:\Ripcord_Win_0.4.29
   ```
2. Execute the patcher in revert mode:
   ```powershell
   .\patcher.exe -u Ripcord.exe
   ```
3. The patcher will revert all 5 patch offsets:
   - `0x3E0FF1`: `0x20` $\rightarrow$ `0x00`
   - `0x179B4B`: `0x08` $\rightarrow$ `0x07`
   - `0x179B79`: `EB 4C` $\rightarrow$ `E9 D6`
   - `0x0EA8AC`: `0xEB` $\rightarrow$ `0x74`
   - `0x1532BB`: `0xEB` $\rightarrow$ `0x74`
4. Patcher verifies the post-revert hash:
   ```
   Post-revert SHA-256: 12D62ABB9AD4DB43C2B9B1398ACAE66857EB6E64205364631A3D3BDA0FF17E2E
   SUCCESS: Bit-perfect restoration to official Ripcord 0.4.29 baseline verified!
   ```

### Manual Verification of Restored Binary:
```powershell
Get-FileHash -Algorithm SHA256 .\Ripcord.exe
```
Confirm the output exactly equals `12D62ABB9AD4DB43C2B9B1398ACAE66857EB6E64205364631A3D3BDA0FF17E2E`.

---

## 3. Procedure B: Removing Dynamic Sidecar Hooking

Dynamic hooking is governed entirely by `winmm.dll` located in the application directory.

### To Disable Dynamic Hooking:
1. Ensure `Ripcord.exe` is not running:
   ```powershell
   Stop-Process -Name "Ripcord" -Force -ErrorAction SilentlyContinue
   ```
2. Delete or rename `winmm.dll`:
   ```powershell
   Remove-Item .\winmm.dll
   ```
3. On the next launch, Windows will load the genuine system multimedia driver from `C:\Windows\System32\winmm.dll`. MinHook and proxy hooks will not be initialized.

---

## 4. Procedure C: Emergency Restoration from Backup File (`.bak`)

Before any byte modifications are written to disk, `patcher.exe` creates an automatic backup file named `Ripcord.exe.bak` and verifies its SHA-256 hash.

If an unexpected crash or file corruption occurs:
1. Terminate any running instances of Ripcord:
   ```powershell
   Stop-Process -Name "Ripcord" -Force -ErrorAction SilentlyContinue
   ```
2. Restore the backup:
   ```powershell
   Copy-Item .\Ripcord.exe.bak -Destination .\Ripcord.exe -Force
   ```
3. Verify integrity:
   ```powershell
   .\patcher.exe -v .\Ripcord.exe
   ```
   Expected status: `EXACT OFFICIAL UNPATCHED BASELINE` (0 Patched, 5 Original).

---

## 5. Rollback Verification Checklist

- [ ] `Ripcord.exe` file size is exactly `5,509,808` bytes.
- [ ] `Ripcord.exe` SHA-256 matches `12D62ABB9AD4DB43C2B9B1398ACAE66857EB6E64205364631A3D3BDA0FF17E2E`.
- [ ] `winmm.dll` removed from application directory.
- [ ] Application launches without proxy console or hook initialization.
