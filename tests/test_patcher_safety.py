"""
Unit & Regression Test Suite: Safe Binary Patcher
Tests verification, backup, idempotent patching, reversion, and corruption resistance.
"""

import os
import shutil
import subprocess
import hashlib
import unittest

PATCHER_EXE = r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\patcher\patcher.exe"
BASELINE_EXE = r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\original_baseline_dist\Ripcord.exe"
TEST_DIR = r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\tests\tmp_patcher"

OFFICIAL_SHA256 = "12d62abb9ad4db43c2b9b1398acae66857eb6e64205364631a3d3bda0ff17e2e"

class TestPatcherSafety(unittest.TestCase):
    def setUp(self):
        os.makedirs(TEST_DIR, exist_ok=True)
        self.target_copy = os.path.join(TEST_DIR, "Ripcord.exe")
        shutil.copyfile(BASELINE_EXE, self.target_copy)

    def tearDown(self):
        if os.path.exists(TEST_DIR):
            shutil.rmtree(TEST_DIR, ignore_errors=True)

    def compute_sha256(self, filepath):
        h = hashlib.sha256()
        with open(filepath, "rb") as f:
            while chunk := f.read(65536):
                h.update(chunk)
        return h.hexdigest().lower()

    def test_01_verify_clean_baseline(self):
        """Clean baseline must report 0 patched, 6 original, 0 mismatched."""
        res = subprocess.run([PATCHER_EXE, "-v", self.target_copy], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertIn("0 Patched, 6 Original, 0 Mismatched", res.stdout)
        self.assertEqual(self.compute_sha256(self.target_copy), OFFICIAL_SHA256)

    def test_02_dry_run_leaves_file_unmodified(self):
        """--dry-run simulates patching without modifying disk bytes."""
        res = subprocess.run([PATCHER_EXE, "--dry-run", self.target_copy], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertIn("(DRY RUN)", res.stdout)
        self.assertEqual(self.compute_sha256(self.target_copy), OFFICIAL_SHA256)
        # Ensure no backup file was created
        self.assertFalse(os.path.exists(self.target_copy + ".bak"))

    def test_03_patch_creates_backup_and_applies_all(self):
        """Applying patches must create .bak and transition all 6 offsets to PATCHED."""
        res = subprocess.run([PATCHER_EXE, self.target_copy], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertTrue("Successfully created" in res.stdout and "backup" in res.stdout)
        self.assertIn("6 patches applied successfully", res.stdout)
        self.assertTrue(os.path.exists(self.target_copy + ".bak"))
        self.assertEqual(self.compute_sha256(self.target_copy + ".bak"), OFFICIAL_SHA256)

        # Verification check
        ver = subprocess.run([PATCHER_EXE, "-v", self.target_copy], capture_output=True, text=True)
        self.assertEqual(ver.returncode, 0)
        self.assertIn("6 Patched, 0 Original, 0 Mismatched", ver.stdout)

    def test_04_patching_is_idempotent(self):
        """Running patcher twice on an already-patched binary must succeed safely with 0 new patches."""
        subprocess.run([PATCHER_EXE, self.target_copy], check=True)
        patched_hash = self.compute_sha256(self.target_copy)

        res2 = subprocess.run([PATCHER_EXE, self.target_copy], capture_output=True, text=True)
        self.assertEqual(res2.returncode, 0)
        self.assertTrue("ALREADY fully patched" in res2.stdout or "0 patches applied successfully" in res2.stdout)
        self.assertEqual(self.compute_sha256(self.target_copy), patched_hash)

    def test_05_revert_restores_exact_original_binary(self):
        """Reverting patches with -u must restore exact original bytes and SHA-256."""
        subprocess.run([PATCHER_EXE, self.target_copy], check=True)
        self.assertNotEqual(self.compute_sha256(self.target_copy), OFFICIAL_SHA256)

        res_rev = subprocess.run([PATCHER_EXE, "-u", self.target_copy], capture_output=True, text=True)
        self.assertEqual(res_rev.returncode, 0)
        self.assertIn("6 patches reverted successfully", res_rev.stdout)
        self.assertEqual(self.compute_sha256(self.target_copy), OFFICIAL_SHA256)

    def test_06_corrupted_byte_aborts_without_modification(self):
        """If target bytes at a patch location do not match, patcher must abort immediately."""
        # Corrupt offset 0x1532BB (cooldown patch target)
        with open(self.target_copy, "r+b") as f:
            f.seek(0x1532BB)
            f.write(b"\x90") # NOP instead of expected 0x74 or 0xEB

        pre_hash = self.compute_sha256(self.target_copy)
        res = subprocess.run([PATCHER_EXE, self.target_copy], capture_output=True, text=True)
        self.assertNotEqual(res.returncode, 0)
        self.assertTrue("ABORT" in res.stderr and ("SHA-256" in res.stderr or "Pre-validation failed" in res.stderr))
        # Ensure file was not altered further
        self.assertEqual(self.compute_sha256(self.target_copy), pre_hash)

    def test_07_invalid_version_fails_integrity_check(self):
        """If binary version string is not 0.4.29, modification must be rejected."""
        with open(self.target_copy, "r+b") as f:
            f.seek(0x3B9970)
            f.write(b"0.9.99")

        res = subprocess.run([PATCHER_EXE, self.target_copy], capture_output=True, text=True)
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("Integrity check 'version_check' failed", res.stderr)

if __name__ == "__main__":
    unittest.main()
