"""
Hardening and Patching Script for Ripcord Community Fix winmm.dll
Applies verified binary fixes:
  1. DEF-08: Restores dual-stack/IPv6 for REST, CDN, and Slack by removing
     unconditional IPv4 socket protocol forcing in QAbstractSocketConnectToHostHook.
  2. DEF-07: Fixes audio timestamp leap on stream start and clamps long silence
     jumps to 3000ms (144,000 samples) via a dedicated assembly cave at 0x266BF.
"""

import os
import struct
import hashlib

def apply_hardening(input_path: str, output_path: str) -> str:
    with open(input_path, "rb") as f:
        data = bytearray(f.read())

    # 1. Verify and patch QAbstractSocketConnectToHostHook (0x80760)
    # Original: c7 44 24 28 00 00 00 00 (mov dword ptr [rsp+28h], 0)
    expected_ipv4 = b"\xc7\x44\x24\x28\x00\x00\x00\x00"
    actual_ipv4 = bytes(data[0x80760:0x80768])
    if actual_ipv4 != expected_ipv4 and actual_ipv4 != b"\x90" * 8:
        raise ValueError(f"Unexpected bytes at 0x80760: {actual_ipv4.hex()}")
    data[0x80760:0x80768] = b"\x90" * 8

    # 2. Verify and patch SendVoiceData timestamp generation (0x811DD)
    expected_ts = b"\x48\x8b\xc8\x48\x2b\x0d\x61\x7c\x31\x00\x48\x83\xf9\x64\x76\x08\x8d\x14\x49\xc1\xe2\x04\xeb\x05\xba\xc0\x03\x00\x00"
    actual_ts = bytes(data[0x811DD:0x811FA])
    
    cave_addr = 0x266BF
    last_send_ms_addr = 0x398E48
    ret_target_addr = 0x811FA

    # Construct cave code
    code = bytearray()
    # 1. mov rcx, [last_send_ms] (7 bytes)
    code += b"\x48\x8b\x0d" + struct.pack("<i", last_send_ms_addr - (cave_addr + len(code) + 7))
    # 2. test rcx, rcx (3 bytes)
    code += b"\x48\x85\xc9"
    # 3. jz is_default (2 bytes placeholder)
    jz_def_idx = len(code)
    code += b"\x74\x00"
    # 4. mov rcx, rax (now) (3 bytes)
    code += b"\x48\x89\xc1"
    # 5. sub rcx, [last_send_ms] (7 bytes)
    code += b"\x48\x2b\x0d" + struct.pack("<i", last_send_ms_addr - (cave_addr + len(code) + 7))
    # 6. cmp rcx, 100 (4 bytes)
    code += b"\x48\x83\xf9\x64"
    # 7. jbe is_default (2 bytes placeholder)
    jbe_def_idx = len(code)
    code += b"\x76\x00"
    # 8. cmp rcx, 3000 (7 bytes)
    code += b"\x48\x81\xf9\xb8\x0b\x00\x00"
    # 9. ja is_clamped (2 bytes placeholder)
    ja_clamp_idx = len(code)
    code += b"\x77\x00"
    # 10. lea edx, [rcx + rcx*2]; shl edx, 4 (6 bytes)
    code += b"\x8d\x14\x49\xc1\xe2\x04"
    # 11. jmp ret_target (2 bytes placeholder)
    jmp_ret1_idx = len(code)
    code += b"\xeb\x00"
    # is_clamped: mov edx, 144000 (3000 * 48) (5 bytes)
    clamp_pos = len(code)
    code[ja_clamp_idx + 1] = clamp_pos - (ja_clamp_idx + 2)
    code += b"\xba\x80\x32\x02\x00"
    jmp_ret2_idx = len(code)
    code += b"\xeb\x00"
    # is_default: mov edx, 960 (5 bytes)
    def_pos = len(code)
    code[jz_def_idx + 1] = def_pos - (jz_def_idx + 2)
    code[jbe_def_idx + 1] = def_pos - (jbe_def_idx + 2)
    code += b"\xba\xc0\x03\x00\x00"
    # ret_target: jmp 0x811FA (5 bytes)
    ret_pos = len(code)
    code[jmp_ret1_idx + 1] = ret_pos - (jmp_ret1_idx + 2)
    code[jmp_ret2_idx + 1] = ret_pos - (jmp_ret2_idx + 2)
    code += b"\xe9" + struct.pack("<i", ret_target_addr - (cave_addr + len(code) + 5))

    if actual_ts != expected_ts:
        expected_hook = b"\xe9" + struct.pack("<i", cave_addr - (0x811DD + 5)) + b"\x90" * 24
        if actual_ts != expected_hook:
            raise ValueError(f"Unexpected bytes at 0x811DD: {actual_ts.hex()}")
    else:
        # Check cave is empty (0xCC)
        assert data[cave_addr:cave_addr+len(code)] == b"\xcc" * len(code), "Cave is not clean 0xCC!"
        data[cave_addr:cave_addr+len(code)] = code

        hook_jmp = b"\xe9" + struct.pack("<i", cave_addr - (0x811DD + 5)) + b"\x90" * 24
        data[0x811DD:0x811FA] = hook_jmp

    with open(output_path, "wb") as f:
        f.write(data)

    sha = hashlib.sha256(data).hexdigest().upper()
    return sha

if __name__ == "__main__":
    base = r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair"
    src = os.path.join(base, "original_baseline_dist", "winmm.dll")
    dst = os.path.join(base, "dist", "winmm.dll")
    sha = apply_hardening(src, dst)
    print(f"Applied verified hardening to {dst} (SHA256: {sha})")
