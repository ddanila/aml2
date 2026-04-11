#!/usr/bin/env python3
"""
Reproduce keyboard lag: send two DOWN keys 200ms apart, check result.

If both keys processed: selection should be at entry 2 (moved 2 positions).
If second key lost/delayed: selection at entry 1 (moved 1 position).
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


def find_selected_entry(raw):
    """Find which entry row has attribute 0x30 (selected) at col 6."""
    for row in range(1, 22):
        idx = (row * 80 + 6) * 2 + 1  # attr byte at col 6
        if raw[idx] == 0x30:
            return row
    return -1


def dump_attrs_col6(raw):
    """Show attribute at col 6 for rows 1-21."""
    for row in range(1, 22):
        idx = (row * 80 + 6) * 2 + 1
        ch_idx = (row * 80 + 6) * 2
        ch = chr(raw[ch_idx]) if raw[ch_idx] >= 32 else '.'
        marker = " <-- SELECTED" if raw[idx] == 0x30 else ""
        print(f"  row {row:2d}: attr=0x{raw[idx]:02x} char='{ch}'{marker}")


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
            if b"Launcher" in text:
                break
            time.sleep(0.3)
        else:
            print(f"  [{label}] ERROR: UI didn't load")
            return

        time.sleep(1.0)  # Let UI settle

        # Check initial state
        raw = read_vram(qmp, tmp)
        sel0 = find_selected_entry(raw)
        print(f"  [{label}] Initial selection at row: {sel0}")

        # Send two DOWN keys 200ms apart — NO pmemsave between them
        qmp.send_key("down")
        time.sleep(0.2)
        qmp.send_key("down")

        # Wait 3 seconds for everything to settle
        time.sleep(3.0)

        # Check final state
        raw = read_vram(qmp, tmp)
        sel_final = find_selected_entry(raw)
        moved = sel_final - sel0 if sel_final >= 0 and sel0 >= 0 else -1

        print(f"  [{label}] Final selection at row: {sel_final}")
        print(f"  [{label}] Entries moved: {moved}")

        if moved == 2:
            print(f"  [{label}] PASS — both keys processed")
        elif moved == 1:
            print(f"  [{label}] FAIL — only first key processed (second lost/delayed)")
        elif moved == 0:
            print(f"  [{label}] FAIL — no movement at all")
        else:
            print(f"  [{label}] UNEXPECTED — moved {moved} positions")
            dump_attrs_col6(raw)

        qmp.send_key("f10")
        time.sleep(0.5)
        p.kill()
        p.wait()

    finally:
        shutil.rmtree(work, ignore_errors=True)

    return moved


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <AMLUI.EXE> [label]")
        sys.exit(1)

    path = sys.argv[1]
    label = sys.argv[2] if len(sys.argv) > 2 else os.path.basename(os.path.dirname(path))

    print(f"Testing: {path} ({os.path.getsize(path)} bytes)")
    run_test(path, label)


if __name__ == "__main__":
    main()
