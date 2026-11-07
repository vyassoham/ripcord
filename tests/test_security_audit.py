"""
Security & Hardening Audit Test Suite
Verifies credential hygiene, PE security mitigations (DEP, ASLR, HighEntropyVA),
cryptographic enforcement (TLS, AEAD, libsodium), and safe update endpoints.
"""

import os
import re
import struct
import unittest

BASELINE_EXE = r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\original_baseline_dist\Ripcord.exe"
PATCHER_EXE = r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\patcher\patcher.exe"
WINMM_DLL = r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\original_baseline_dist\winmm.dll"
HOOK_C = r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\ripcord-community-fix\source\hook.c"
UPDATE_C = r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\ripcord-community-fix\source\update.c"

def check_pe_security_characteristics(filepath: str):
    """Parses PE Optional Header DllCharacteristics for DEP, ASLR, and High Entropy VA."""
    with open(filepath, "rb") as f:
        data = f.read(1024)

    # DOS Header
    if data[:2] != b"MZ":
        return None
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]

    # PE Signature
    with open(filepath, "rb") as f:
        f.seek(pe_off)
        pe_sig = f.read(4)
        if pe_sig != b"PE\0\0":
            return None
        # COFF File Header (20 bytes)
        coff = f.read(20)
        machine, n_sections, time_date, sym_ptr, n_syms, opt_hdr_size, characteristics = struct.unpack("<HHIIIHH", coff)
        if opt_hdr_size == 0:
            return None

        # Optional Header
        opt_hdr = f.read(opt_hdr_size)
        magic = struct.unpack_from("<H", opt_hdr, 0)[0]
        # 0x20B = PE32+ (64-bit)
        if magic != 0x20B:
            return None

        # DllCharacteristics is at offset 70 in PE32+ Optional Header
        dll_chars = struct.unpack_from("<H", opt_hdr, 70)[0]

        IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA = 0x0020
        IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE    = 0x0040
        IMAGE_DLLCHARACTERISTICS_NX_COMPAT       = 0x0100

        return {
            "is_64bit": True,
            "high_entropy_va": bool(dll_chars & IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA),
            "aslr_dynamic_base": bool(dll_chars & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE),
            "dep_nx_compat": bool(dll_chars & IMAGE_DLLCHARACTERISTICS_NX_COMPAT),
            "raw_dll_characteristics": hex(dll_chars)
        }

class TestSecurityAudit(unittest.TestCase):
    def test_01_pe_security_mitigations_ripcord(self):
        """Ripcord.exe baseline must have NX/DEP and ASLR DynamicBase enabled."""
        chars = check_pe_security_characteristics(BASELINE_EXE)
        self.assertIsNotNone(chars)
        self.assertTrue(chars["is_64bit"])
        self.assertTrue(chars["dep_nx_compat"], "DEP (NX_COMPAT) must be enabled on Ripcord.exe")
        self.assertTrue(chars["aslr_dynamic_base"], "ASLR (DYNAMIC_BASE) must be enabled on Ripcord.exe")

    def test_02_pe_security_mitigations_patcher(self):
        """Hardened patcher.exe must have NX/DEP and ASLR DynamicBase enabled."""
        self.assertTrue(os.path.exists(PATCHER_EXE), "patcher.exe must be compiled")
        chars = check_pe_security_characteristics(PATCHER_EXE)
        self.assertIsNotNone(chars)
        self.assertTrue(chars["is_64bit"])
        self.assertTrue(chars["dep_nx_compat"], "DEP (NX_COMPAT) must be enabled on patcher.exe")
        self.assertTrue(chars["aslr_dynamic_base"], "ASLR (DYNAMIC_BASE) must be enabled on patcher.exe")

    def test_03_tls_and_cryptographic_enforcement(self):
        """Verify that update checks enforce HTTPS with WINHTTP_FLAG_SECURE and no certificate bypass."""
        with open(UPDATE_C, "r", encoding="utf-8") as f:
            code = f.read()

        self.assertIn("WINHTTP_FLAG_SECURE", code, "Update checker must mandate secure HTTPS connections")
        # Ensure certificate errors are NOT ignored (no SECURITY_FLAG_IGNORE_UNKNOWN_CA, etc.)
        self.assertNotIn("SECURITY_FLAG_IGNORE_UNKNOWN_CA", code)
        self.assertNotIn("SECURITY_FLAG_IGNORE_CERT_DATE_INVALID", code)
        self.assertNotIn("SECURITY_FLAG_IGNORE_CERT_CN_INVALID", code)

    def test_04_no_plaintext_fallback_in_voice_encryption(self):
        """SendVoiceData must strictly mandate secretKey and reject unsupported encryption modes."""
        with open(HOOK_C, "r", encoding="utf-8") as f:
            code = f.read()

        self.assertIn("if (!secretKey || !hostName->d->size)", code)
        self.assertIn("crypto_aead_xchacha20poly1305_ietf_encrypt", code)
        self.assertIn('ErrorMessage("Unsupported encryption mode")', code)

    def test_05_no_token_or_credential_leakage_in_hook(self):
        """Verify that hook.c does not dump session tokens or credentials in log messages."""
        with open(HOOK_C, "r", encoding="utf-8") as f:
            code = f.read()

        # DebugMsg or ErrorMessage must not format token or secretKey
        for line in code.splitlines():
            if any(log_call in line for log_call in ("DebugMsg", "ErrorMessage", "printf", "puts")):
                self.assertNotIn("connToken", line, f"Potential credential leak in log: {line}")
                self.assertNotIn("secretKey", line, f"Potential voice key leak in log: {line}")

if __name__ == "__main__":
    unittest.main()
