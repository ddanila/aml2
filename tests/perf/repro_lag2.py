#!/usr/bin/env python3
"""
Reproduce lag v2: send two DOWN keys with varying intervals,
measure total time until BOTH are reflected on screen.
"""

import os, shutil, subprocess, sys, tempfile, time

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, "tests", "lib"))
from screen_expect import QMPConnection

VRAM_PHYS = 0xB8000
VRAM_SIZE = 4000
DOS_IMAGE_URL = "https://github.com/ddanila/msdos/releases/download/0.1/floppy-minimal.img"


def download_base_image(dest):
    if os.path.exists(dest):
        return
    import urllib.request
    urllib.request.urlretrieve(DOS_IMAGE_URL, dest)


def read_vram(qmp, tmp_path):
    qmp.human_cmd(f'pmemsave 0x{VRAM_PHYS:X} {VRAM_SIZE} "{tmp_path}"')
    with open(tmp_path, "rb") as f:
        return f.read(VRAM_SIZE)


def find_selected_row(raw):
    """Find which row has attribute 0x30 at col 6."""
    for row in range(1, 22):
        if raw[(row * 80 + 6) * 2 + 1] == 0x30:
            return row
    return -1


def run_trial(qmp, tmp, label, key_interval_ms):
    """Send two DOWN keys with given interval, measure time until both processed."""
    raw = read_vram(qmp, tmp)
    initial_row = find_selected_row(raw)

    # v0.1.8: 1 row/entry, bigtext: 2 rows/entry
    # After 2 DOWNs: should move by 2 entries
    expected_move = 2 if initial_row >= 2 else 2  # rows to move

    t0 = time.monotonic()

    # Send first DOWN
    qmp.send_key("down")

    # Wait interval, then send second DOWN
    time.sleep(key_interval_ms / 1000.0)
    qmp.send_key("down")

    # Poll until selection has moved the expected amount
    # For bigtext: 2 entries = 4 rows. For plain: 2 entries = 2 rows
    target_min_move = 2  # At least 2 rows means both keys processed
    deadline = time.monotonic() + 10.0

    while time.monotonic() < deadline:
        raw = read_vram(qmp, tmp)
        current_row = find_selected_row(raw)
        if current_row >= 0 and current_row - initial_row >= target_min_move:
            elapsed = (time.monotonic() - t0) * 1000
            move = current_row - initial_row
            print(f"  {label}: {elapsed:.0f}ms total, moved {move} rows "
                  f"(interval={key_interval_ms}ms)")
            # Send UP keys to reset position
            for _ in range(move):
                qmp.send_key("up")
                time.sleep(0.05)
            time.sleep(1.0)
            return elapsed
        time.sleep(0.005)

    elapsed = (time.monotonic() - t0) * 1000
    raw = read_vram(qmp, tmp)
    current_row = find_selected_row(raw)
    move = current_row - initial_row if current_row >= 0 else -1
    print(f"  {label}: TIMEOUT after {elapsed:.0f}ms, moved {move} rows")
    return -1


def run_test(amlui_path, label):
    work = tempfile.mkdtemp(prefix="aml2_repro_")
    base = os.path.join(work, "base.img")
    boot = os.path.join(work, "boot.img")
    sock = os.path.join(work, "qmp.sock")
    tmp = os.path.join(work, "vram.bin")
    auto = os.path.join(work, "AUTOEXEC.BAT")
    cfg = os.path.join(REPO_ROOT, "tests", "perf", "LAUNCHER.CFG")

    try:
        download_base_image(base)
        shutil.copy(base, boot)
        with open(auto, "wb") as f:
            f.write(b"@ECHO OFF\r\nAMLUI.EXE /V\r\n")
        subprocess.run(["mcopy", "-o", "-i", boot, amlui_path, "::AMLUI.EXE"],
                       check=True, capture_output=True)
        subprocess.run(["mcopy", "-o", "-i", boot, cfg, "::LAUNCHER.CFG"],
                       check=True, capture_output=True)
        subprocess.run(["mcopy", "-o", "-i", boot, auto, "::AUTOEXEC.BAT"],
                       check=True, capture_output=True)

        p = subprocess.Popen(
            ["qemu-system-i386", "-display", "none", "-monitor", "none",
             "-serial", "none",
             "-drive", f"if=floppy,index=0,format=raw,file={boot}",
             "-boot", "a", "-m", "4",
             "-qmp", f"unix:{sock},server,nowait"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        for _ in range(50):
            if os.path.exists(sock): break
            time.sleep(0.2)

        qmp = QMPConnection(sock)

        # Wait for UI
        for _ in range(100):
            raw = read_vram(qmp, tmp)
            text = bytes(raw[i] for i in range(0, VRAM_SIZE, 2))
            if b"Launcher" in text: break
            time.sleep(0.3)
        else:
            print(f"  [{label}] UI didn't load")
            p.kill(); p.wait()
            return

        time.sleep(2.0)

        print(f"\n  [{label}] Testing with different key intervals:")
        for interval in [200, 150, 100, 50]:
            run_trial(qmp, tmp, label, interval)

        qmp.send_key("f10")
        time.sleep(0.5)
        p.kill(); p.wait()

    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <AMLUI.EXE> [label]")
        sys.exit(1)
    path = sys.argv[1]
    label = sys.argv[2] if len(sys.argv) > 2 else "test"
    print(f"Testing: {path} ({os.path.getsize(path)} bytes)")
    run_test(path, label)


if __name__ == "__main__":
    main()
