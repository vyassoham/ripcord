"""
Automated Build & Release Script for Ripcord Windows x64 Repair & Hardening
"""

import os
import shutil
import subprocess
import hashlib
import sys
from harden_winmm import apply_hardening

BASE_DIR = r"C:\Users\dell\.gemini\antigravity\scratch\ripcord-repair"
PATCHER_DIR = os.path.join(BASE_DIR, "patcher")
DIST_DIR = os.path.join(BASE_DIR, "dist")
BASELINE_DIR = os.path.join(BASE_DIR, "original_baseline_dist")

def compute_sha256(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest().upper()

def main():
    print("======================================================================")
    print("RIPCORD REPAIR: COMPILING & HARDENING RELEASE ARTIFACTS (WINDOWS X64)")
    print("======================================================================")

    # 1. Compile hardened patcher.exe
    print("\n[Step 1/5] Compiling hardened patcher.exe with MinGW GCC 13.2.0...")
    patcher_src = os.path.join(PATCHER_DIR, "source", "win32", "build.c")
    patcher_out = os.path.join(PATCHER_DIR, "patcher.exe")
    
    env = os.environ.copy()
    env["PATH"] = r"C:\MinGW\bin;" + env.get("PATH", "")

    cmd = [
        r"C:\MinGW\bin\gcc.exe",
        "-O2",
        "-Wall",
        "-Wextra",
        "-Wno-unused-function",
        "-Wno-unused-parameter",
        f"-I{os.path.join(PATCHER_DIR, 'source')}",
        patcher_src,
        "-lbcrypt",
        "-o", patcher_out
    ]
    res = subprocess.run(cmd, env=env, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"ERROR: Patcher compilation failed:\n{res.stderr}")
        return 1
    print(f"SUCCESS: Built {patcher_out}")

    # 2. Assemble distribution directory
    print("\n[Step 2/5] Assembling base release distribution in dist/...")
    if os.path.exists(DIST_DIR):
        shutil.rmtree(DIST_DIR, ignore_errors=True)
    os.makedirs(DIST_DIR, exist_ok=True)

    # Copy all runtime files from baseline dist
    for item in os.listdir(BASELINE_DIR):
        src_path = os.path.join(BASELINE_DIR, item)
        dst_path = os.path.join(DIST_DIR, item)
        if os.path.isdir(src_path):
            shutil.copytree(src_path, dst_path, dirs_exist_ok=True)
        else:
            shutil.copy2(src_path, dst_path)

    # Copy hardened patcher.exe
    shutil.copy2(patcher_out, os.path.join(DIST_DIR, "patcher.exe"))

    # Apply Preferences show patch to dist/Ripcord.exe (0x80C54: 0xF0 -> 0xF8)
    ripcord_dist = os.path.join(DIST_DIR, "Ripcord.exe")
    with open(ripcord_dist, "r+b") as f:
        f.seek(0x80C54)
        b = f.read(1)
        if b == b"\xF0":
            f.seek(0x80C54)
            f.write(b"\xF8")
            print("SUCCESS: Applied Preferences dialog show patch to dist/Ripcord.exe")

    # Copy tools directory into dist/tools
    tools_src = os.path.join(BASE_DIR, "tools")
    tools_dst = os.path.join(DIST_DIR, "tools")
    if os.path.exists(tools_src):
        shutil.copytree(tools_src, tools_dst, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"), dirs_exist_ok=True)
        print(f"SUCCESS: Packaged companion tools in {tools_dst}")

    # 3. Apply verified hardening to dist/winmm.dll
    print("\n[Step 3/5] Applying verified binary hardening to dist/winmm.dll...")
    winmm_dist = os.path.join(DIST_DIR, "winmm.dll")
    winmm_sha = apply_hardening(winmm_dist, winmm_dist)
    print(f"SUCCESS: Hardened winmm.dll in dist (SHA256: {winmm_sha})")

    # 4. Run full verification test suite
    print("\n[Step 4/5] Executing full test suite...")
    test_runner = os.path.join(BASE_DIR, "tests", "run_all_tests.py")
    test_res = subprocess.run([sys.executable, test_runner], capture_output=True, text=True)
    print(test_res.stdout)
    if test_res.returncode != 0:
        print(f"ERROR: Test suite reported failures:\n{test_res.stderr}")
        return 1

    # 5. Generate SHA256SUMS.txt
    print("\n[Step 5/5] Computing SHA-256 checksums for release manifest...")
    sums_file = os.path.join(DIST_DIR, "SHA256SUMS.txt")
    lines = []
    for root, _, files in os.walk(DIST_DIR):
        for f in sorted(files):
            if f == "SHA256SUMS.txt":
                continue
            full_path = os.path.join(root, f)
            rel_path = os.path.relpath(full_path, DIST_DIR)
            sha = compute_sha256(full_path)
            lines.append(f"{sha}  {rel_path}")

    with open(sums_file, "w", encoding="utf-8", newline="\r\n") as f:
        f.write("\n".join(lines) + "\n")

    print(f"Manifest written to: {sums_file}")
    print("\nCore Release Artifacts:")
    for l in lines:
        if any(core in l for core in ("Ripcord.exe", "winmm.dll", "patcher.exe")):
            print(f"  {l}")

    print("\nRelease build process completed successfully.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
