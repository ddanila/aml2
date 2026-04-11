#!/usr/bin/env python3
"""
Measure keyboard response latency in AMLUI.EXE under QEMU.

Compares response time between first and second DOWN keypresses.
Runs against any AMLUI.EXE binary extracted from a release zip.

Usage:
    python3 tests/perf/measure_qemu.py <path-to-AMLUI.EXE>
"""

import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, "tests", "lib"))

from screen_expect import QMPConnection

VRAM_PHYS = 0xB8000
VRAM_SIZE = 4000
DOS_IMAGE_URL = "https://github.com/ddanila/msdos/releases/download/0.1/floppy-minimal.img"
POLL_MS = 5  # 5ms polling for fine-grained timing


def download_base_image(dest):
    if os.path.exists(dest):
        return
    print(f"Downloading base DOS image...")
    import urllib.request
    urllib.request.urlretrieve(DOS_IMAGE_URL, dest)


def read_vram(qmp, tmp_path):
    """Read raw 4000-byte VGA text buffer from QEMU."""
    qmp.human_cmd(f'pmemsave 0x{VRAM_PHYS:X} {VRAM_SIZE} "{tmp_path}"')
    with open(tmp_path, "rb") as f:
        return f.read(VRAM_SIZE)


def vram_to_text(raw):
    """Extract character bytes from interleaved char+attr pairs."""
    return bytes(raw[i] for i in range(0, len(raw), 2))


def vram_to_attrs(raw):
    """Extract attribute bytes from interleaved char+attr pairs."""
    return bytes(raw[i] for i in range(1, len(raw), 2))


def dump_screen(raw):
    chars = vram_to_text(raw)
    for row in range(25):
        line = chars[row * 80 : (row + 1) * 80]
        print(f"  {row:02d}: [{line.decode('cp437', errors='replace')}]")


def wait_for_screen_text(qmp, tmp_path, text, timeout=30):
    """Wait until text appears anywhere on screen."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        raw = read_vram(qmp, tmp_path)
        screen = vram_to_text(raw).decode("cp437", errors="replace")
        if text in screen:
            return raw
        time.sleep(0.3)
    return None


def measure_key_response(qmp, tmp_path, key, label, watch_row=None, watch_col=6):
    """Send a key and measure wall-clock time until screen changes."""
    baseline = read_vram(qmp, tmp_path)

    t_start = time.monotonic()
    qmp.send_key(key)

    # Poll until VRAM changes
    while True:
        current = read_vram(qmp, tmp_path)
        if watch_row is not None:
            # Watch a specific cell's character byte
            idx = (watch_row * 80 + watch_col) * 2  # char byte (not attr)
            if current[idx] != baseline[idx]:
                t_end = time.monotonic()
                elapsed_ms = (t_end - t_start) * 1000
                print(f"  {label}: {elapsed_ms:.1f} ms (attr at row {watch_row} col {watch_col}: 0x{baseline[idx]:02x} -> 0x{current[idx]:02x})")
                return elapsed_ms, current
        elif current != baseline:
            t_end = time.monotonic()
            elapsed_ms = (t_end - t_start) * 1000
            # Find what changed
            for i in range(len(baseline)):
                if current[i] != baseline[i]:
                    row, col = divmod(i // 2, 80)
                    is_attr = i % 2 == 1
                    print(f"  {label}: {elapsed_ms:.1f} ms (first change at row {row} col {col} {'attr' if is_attr else 'char'}: 0x{baseline[i]:02x} -> 0x{current[i]:02x})")
                    break
            return elapsed_ms, current
        if time.monotonic() - t_start > 30:
            print(f"  {label}: TIMEOUT (no screen change after 30s)")
            return -1, baseline
        time.sleep(POLL_MS / 1000.0)


def run_test(amlui_exe_path):
    work_dir = tempfile.mkdtemp(prefix="aml2_perf_")
    base_img = os.path.join(work_dir, "base.img")
    boot_img = os.path.join(work_dir, "boot.img")
    qmp_sock = os.path.join(work_dir, "qmp.sock")
    tmp_vram = os.path.join(work_dir, "vram.bin")
    autoexec = os.path.join(work_dir, "AUTOEXEC.BAT")
    launcher_cfg = os.path.join(REPO_ROOT, "tests", "perf", "LAUNCHER.CFG")
    qemu_proc = None

    try:
        download_base_image(base_img)
        shutil.copy(base_img, boot_img)

        # Write AUTOEXEC.BAT — run AMLUI.EXE /V directly (no stub needed)
        with open(autoexec, "wb") as f:
            f.write(b"@ECHO OFF\r\nAMLUI.EXE /V\r\n")

        # Copy files to floppy
        subprocess.run(["mcopy", "-o", "-i", boot_img, amlui_exe_path, "::AMLUI.EXE"], check=True)
        subprocess.run(["mcopy", "-o", "-i", boot_img, launcher_cfg, "::LAUNCHER.CFG"], check=True)
        subprocess.run(["mcopy", "-o", "-i", boot_img, autoexec, "::AUTOEXEC.BAT"], check=True)

        # Start QEMU
        print("Starting QEMU...")
        qemu_proc = subprocess.Popen(
            [
                "qemu-system-i386",
                "-display", "none",
                "-monitor", "none",
                "-serial", "none",
                "-drive", f"if=floppy,index=0,format=raw,file={boot_img}",
                "-boot", "a",
                "-m", "4",
                "-qmp", f"unix:{qmp_sock},server,nowait",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        # Wait for QMP socket
        for _ in range(50):
            if os.path.exists(qmp_sock):
                break
            time.sleep(0.2)
        else:
            print("ERROR: QMP socket not found")
            return

        qmp = QMPConnection(qmp_sock)

        # Wait for launcher UI
        print("Waiting for launcher UI...")
        raw = wait_for_screen_text(qmp, tmp_vram, "Launcher", timeout=30)
        if raw is None:
            print("ERROR: Launcher UI did not appear")
            raw = read_vram(qmp, tmp_vram)
            dump_screen(raw)
            return

        # Let UI settle
        time.sleep(1.0)
        print("UI loaded. Screen:")
        raw = read_vram(qmp, tmp_vram)
        dump_screen(raw)

        # --- Measurements ---
        print("\n=== Measuring key response times ===\n")

        # Detect any screen change — with 200ms pause between keys for QMP delivery
        ms1a, _ = measure_key_response(qmp, tmp_vram, "down", "1st DOWN")
        time.sleep(0.2)
        ms2a, _ = measure_key_response(qmp, tmp_vram, "down", "2nd DOWN")
        time.sleep(0.2)
        ms3a, _ = measure_key_response(qmp, tmp_vram, "down", "3rd DOWN")

        print(f"\n=== Results ===")
        print(f"  1st DOWN: {ms1a:.1f} ms")
        print(f"  2nd DOWN: {ms2a:.1f} ms")
        print(f"  3rd DOWN: {ms3a:.1f} ms")
        if ms1a > 0 and ms2a > 0:
            print(f"  Ratio 2nd/1st: {ms2a/ms1a:.2f}x")

        # Exit
        qmp.send_key("f10")

    finally:
        if qemu_proc:
            qemu_proc.kill()
            qemu_proc.wait()
        shutil.rmtree(work_dir, ignore_errors=True)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <path-to-AMLUI.EXE>")
        sys.exit(1)

    amlui_path = sys.argv[1]
    if not os.path.exists(amlui_path):
        print(f"ERROR: {amlui_path} not found")
        sys.exit(1)

    print(f"Testing: {amlui_path}")
    print(f"Size: {os.path.getsize(amlui_path)} bytes\n")
    run_test(amlui_path)


if __name__ == "__main__":
    main()
