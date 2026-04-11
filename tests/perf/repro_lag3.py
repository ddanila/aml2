#!/usr/bin/env python3
"""
Reproduce lag v3: each interval gets its own QEMU instance.
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


def read_vram(qmp, tmp):
    qmp.human_cmd(f'pmemsave 0x{VRAM_PHYS:X} {VRAM_SIZE} "{tmp}"')
    with open(tmp, "rb") as f:
        return f.read(VRAM_SIZE)


def find_selected_row(raw):
    for row in range(1, 22):
        if raw[(row * 80 + 6) * 2 + 1] == 0x30:
            return row
    return -1


def single_trial(amlui_path, interval_ms, base_img_path):
    work = tempfile.mkdtemp(prefix="aml2_trial_")
    boot = os.path.join(work, "boot.img")
    sock = os.path.join(work, "qmp.sock")
    tmp = os.path.join(work, "vram.bin")
    auto = os.path.join(work, "AUTOEXEC.BAT")
    cfg = os.path.join(REPO_ROOT, "tests", "perf", "LAUNCHER.CFG")

    try:
        shutil.copy(base_img_path, boot)
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
            p.kill(); p.wait()
            return "NO_UI"

        time.sleep(2.0)

        raw = read_vram(qmp, tmp)
        initial_row = find_selected_row(raw)

        t0 = time.monotonic()
        qmp.send_key("down")
        time.sleep(interval_ms / 1000.0)
        qmp.send_key("down")

        # Wait up to 5 seconds
        for _ in range(500):
            raw = read_vram(qmp, tmp)
            current_row = find_selected_row(raw)
            if current_row >= 0 and current_row - initial_row >= 2:
                elapsed = (time.monotonic() - t0) * 1000
                p.kill(); p.wait()
                return f"{elapsed:.0f}ms (moved {current_row - initial_row} rows)"
            time.sleep(0.01)

        elapsed = (time.monotonic() - t0) * 1000
        raw = read_vram(qmp, tmp)
        current_row = find_selected_row(raw)
        move = current_row - initial_row if current_row >= 0 else -1
        p.kill(); p.wait()
        return f"TIMEOUT {elapsed:.0f}ms (moved {move})"

    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <AMLUI.EXE> [label]")
        sys.exit(1)

    path = sys.argv[1]
    label = sys.argv[2] if len(sys.argv) > 2 else "test"

    # Download base image once
    base = "/tmp/aml2_base_floppy.img"
    download_base_image(base)

    print(f"Testing: {path} ({os.path.getsize(path)} bytes)")
    for interval in [500, 300, 200, 150, 100, 75, 50, 25]:
        result = single_trial(path, interval, base)
        print(f"  [{label}] interval={interval:3d}ms: {result}")


if __name__ == "__main__":
    main()
