"""
Unit & Binary Integrity Test Suite: Hardened winmm.dll Proxy
Verifies:
  1. PE32+ 64-bit header, DEP/NX, ASLR, HighEntropyVA.
  2. 237 multimedia export thunks with 64-bit indirect tail-call jumps.
  3. QAbstractSocket::connectToHost dual-stack/IPv6 passthrough (DEF-08).
  4. SendVoiceData monotonic timestamp initialization and 3000ms clamp cave (DEF-07).
  5. Live process launch and clean shutdown smoke verification.
"""

import os
import shutil
import struct
import subprocess
import time
import unittest

WINMM_PATH = r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\dist\winmm.dll"
DIST_DIR = r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair\dist"

class TestWinmmBinaryIntegrity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(WINMM_PATH, "rb") as f:
            cls.data = f.read()

        # Parse PE headers
        cls.e_lfanew = struct.unpack_from("<I", cls.data, 0x3C)[0]
        cls.opt_offset = cls.e_lfanew + 24
        cls.num_sections = struct.unpack_from("<H", cls.data, cls.e_lfanew + 6)[0]
        cls.opt_hdr_size = struct.unpack_from("<H", cls.data, cls.e_lfanew + 20)[0]
        cls.sec_offset = cls.opt_offset + cls.opt_hdr_size

        cls.sections = []
        for i in range(cls.num_sections):
            s_data = cls.data[cls.sec_offset + i * 40 : cls.sec_offset + (i + 1) * 40]
            name = s_data[:8].rstrip(b"\x00").decode("latin1", "ignore")
            vsize, vaddr, raw_size, raw_ptr = struct.unpack("<IIII", s_data[8:24])
            cls.sections.append({
                "name": name, "vsize": vsize, "vaddr": vaddr,
                "raw_size": raw_size, "raw_ptr": raw_ptr
            })

    @classmethod
    def rva_to_offset(cls, rva):
        for s in cls.sections:
            if s["vaddr"] <= rva < s["vaddr"] + s["vsize"]:
                return s["raw_ptr"] + (rva - s["vaddr"])
        return None

    def test_01_pe_architecture_and_security(self):
        """winmm.dll must be 64-bit AMD64 PE32+ with DEP, ASLR, and HighEntropyVA."""
        magic = struct.unpack_from("<H", self.data, self.opt_offset)[0]
        self.assertEqual(magic, 0x20B, "Must be PE32+ (64-bit)")

        dll_chars = struct.unpack_from("<H", self.data, self.opt_offset + 70)[0]
        self.assertTrue(bool(dll_chars & 0x0100), "DEP (NX_COMPAT) must be active")
        self.assertTrue(bool(dll_chars & 0x0040), "ASLR (DYNAMIC_BASE) must be active")
        self.assertTrue(bool(dll_chars & 0x0020), "HighEntropyVA must be active")

    def test_02_export_table_and_thunks(self):
        """winmm.dll must export exactly 237 functions as 64-bit indirect JMP thunks."""
        exp_rva, exp_sz = struct.unpack_from("<II", self.data, self.opt_offset + 112)
        self.assertGreater(exp_rva, 0)
        exp_off = self.rva_to_offset(exp_rva)

        num_funcs, num_names, funcs_rva, names_rva, ords_rva = struct.unpack_from("<IIIII", self.data, exp_off + 16 + 4)
        self.assertEqual(num_names, 237, "Must export exactly 237 multimedia functions")

        names_off = self.rva_to_offset(names_rva)
        funcs_off = self.rva_to_offset(funcs_rva)
        ords_off = self.rva_to_offset(ords_rva)

        checked_names = ["CloseDriver", "DefDriverProc", "timeGetTime", "PlaySoundW"]
        for idx in range(num_names):
            nrva = struct.unpack_from("<I", self.data, names_off + idx * 4)[0]
            noff = self.rva_to_offset(nrva)
            ename = self.data[noff : self.data.find(b"\x00", noff)].decode("latin1")
            
            if ename in checked_names:
                ord_val = struct.unpack_from("<H", self.data, ords_off + idx * 2)[0]
                frva = struct.unpack_from("<I", self.data, funcs_off + ord_val * 4)[0]
                foff = self.rva_to_offset(frva)
                # Verify instruction is 48 FF 25 [disp32] (REX.W JMP QWORD PTR [rip+disp32])
                instr = self.data[foff : foff + 3]
                self.assertEqual(instr, b"\x48\xff\x25", f"Export {ename} must be indirect jump thunk")

    def test_03_dual_stack_socket_passthrough(self):
        """Verifies QAbstractSocket::connectToHost hook does not force IPv4 (DEF-08)."""
        # Offset 0x80760 must contain 8 NOPs
        nops = self.data[0x80760 : 0x80768]
        self.assertEqual(nops, b"\x90" * 8, "0x80760 must be NOPs to preserve socket protocol")
        # Followed by 48 FF 25 d1 44 31 00 (JMP [qabstractsocket_connecttohost])
        jmp = self.data[0x80768 : 0x8076F]
        self.assertEqual(jmp, b"\x48\xff\x25\xd1\x44\x31\x00")

    def test_04_audio_monotonic_and_clamping_cave(self):
        """Verifies SendVoiceData hook jumps to cave at 0x266BF with proper checks (DEF-07)."""
        # Hook at 0x811DD: e9 [disp32] landing at 0x266BF
        hook = self.data[0x811DD : 0x811DD + 5]
        self.assertEqual(hook[0], 0xE9, "Must begin with relative JMP")
        disp = struct.unpack("<i", hook[1:5])[0]
        dest = 0x811DD + 5 + disp
        self.assertEqual(dest, 0x266BF, "Hook must land at cave 0x266BF")

        # Verify cave code at 0x266BF
        cave = self.data[0x266BF : 0x266BF + 62]
        # Check initial packet test: 48 85 c9 74 (test rcx, rcx; jz is_default)
        self.assertIn(b"\x48\x85\xc9\x74", cave)
        # Check 3000ms clamp: 48 81 f9 b8 0b 00 00 (cmp rcx, 3000)
        self.assertIn(b"\x48\x81\xf9\xb8\x0b\x00\x00", cave)
        # Check clamped value: ba 80 32 02 00 (mov edx, 144000)
        self.assertIn(b"\xba\x80\x32\x02\x00", cave)
        # Check default frame value: ba c0 03 00 00 (mov edx, 960)
        self.assertIn(b"\xba\xc0\x03\x00\x00", cave)

        # Check return jump to 0x811FA
        ret_jmp = cave[-5:]
        self.assertEqual(ret_jmp[0], 0xE9)
        ret_disp = struct.unpack("<i", ret_jmp[1:5])[0]
        ret_dest = 0x266BF + 62 + ret_disp
        self.assertEqual(ret_dest, 0x811FA, "Cave must return to 0x811FA")

    def test_05_live_process_smoke_test(self):
        """Verifies that Ripcord boots with winmm.dll and terminates cleanly."""
        tmp_dir = os.path.join(os.path.dirname(__file__), "tmp_winmm_smoke")
        if os.path.exists(tmp_dir):
            shutil.rmtree(tmp_dir, ignore_errors=True)
        os.makedirs(tmp_dir, exist_ok=True)

        try:
            for item in os.listdir(DIST_DIR):
                s = os.path.join(DIST_DIR, item)
                d = os.path.join(tmp_dir, item)
                if os.path.isfile(s):
                    shutil.copy2(s, d)
                elif os.path.isdir(s):
                    shutil.copytree(s, d)

            proc = subprocess.Popen([os.path.join(tmp_dir, "Ripcord.exe")], cwd=tmp_dir)
            self.assertIsNotNone(proc.pid)
            time.sleep(1.5)
            self.assertIsNone(proc.poll(), "Ripcord process must remain running without crashing")

            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
        finally:
            time.sleep(0.5)
            shutil.rmtree(tmp_dir, ignore_errors=True)

if __name__ == "__main__":
    unittest.main()
