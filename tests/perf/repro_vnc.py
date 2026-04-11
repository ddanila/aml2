#!/usr/bin/env python3
"""
Reproduce keyboard lag via VNC key injection.

VNC key events go through QEMU's input pipeline (same as physical keyboard),
unlike QMP send-key which bypasses it. This matches real-hardware behavior.

Usage: python3 repro_vnc.py <path-to-AMLUI.EXE> [interval_ms]
"""

import os, shutil, socket, struct, subprocess, sys, tempfile, time

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, "tests", "lib"))
from screen_expect import QMPConnection

VRAM_PHYS = 0xB8000
VRAM_SIZE = 4000
DOS_IMAGE_URL = "https://github.com/ddanila/msdos/releases/download/0.1/floppy-minimal.img"

# VNC keysyms
XK_Down = 0xFF54
XK_Up = 0xFF52
XK_F10 = 0xFFC7


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


class VNCKeyClient:
    """Minimal VNC client that only sends key events."""

    def __init__(self, host, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        self._handshake()

    def _handshake(self):
        # Server sends version
        ver = self.sock.recv(12)
        # Send same version back
        self.sock.sendall(b"RFB 003.008\n")

        # Security types
        n = struct.unpack("B", self.sock.recv(1))[0]
        types = self.sock.recv(n)
        # Select "None" (type 1)
        self.sock.sendall(struct.pack("B", 1))

        # Security result (4 bytes, 0 = OK)
        result = struct.unpack(">I", self.sock.recv(4))[0]
        if result != 0:
            raise RuntimeError(f"VNC auth failed: {result}")

        # ClientInit: shared flag = 1
        self.sock.sendall(struct.pack("B", 1))

        # ServerInit: read and discard
        header = self.sock.recv(24)  # width(2) + height(2) + pixel_format(16) + name_len(4)
        name_len = struct.unpack(">I", header[20:24])[0]
        if name_len > 0:
            self.sock.recv(name_len)

    def send_key_event(self, down, keysym):
        """Send a VNC KeyEvent message (type 4)."""
        msg = struct.pack(">BBHI", 4, 1 if down else 0, 0, keysym)
        self.sock.sendall(msg)

    def press_key(self, keysym, hold_ms=50):
        """Press and release a key."""
        self.send_key_event(True, keysym)
        time.sleep(hold_ms / 1000.0)
        self.send_key_event(False, keysym)

    def close(self):
        self.sock.close()


def run_test(amlui_path, interval_ms):
    work = tempfile.mkdtemp(prefix="aml2_vnc_")
    base = os.path.join(work, "base.img")
    boot = os.path.join(work, "boot.img")
    qmp_sock = os.path.join(work, "qmp.sock")
    tmp = os.path.join(work, "vram.bin")
    auto = os.path.join(work, "AUTOEXEC.BAT")
    cfg = os.path.join(REPO_ROOT, "tests", "perf", "LAUNCHER.CFG")

    # Find a free VNC display number
    vnc_port = 5950
    vnc_display = vnc_port - 5900

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

        # Start QEMU with VNC and QMP
        p = subprocess.Popen(
            ["qemu-system-i386",
             "-display", "none",
             "-vnc", f":{vnc_display}",
             "-drive", f"if=floppy,index=0,format=raw,file={boot}",
             "-boot", "a", "-m", "4",
             "-qmp", f"unix:{qmp_sock},server,nowait"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        # Wait for QMP
        for _ in range(50):
            if os.path.exists(qmp_sock): break
            time.sleep(0.2)

        qmp = QMPConnection(qmp_sock)

        # Wait for UI to load (use QMP pmemsave)
        print("Waiting for UI...")
        for _ in range(100):
            raw = read_vram(qmp, tmp)
            text = bytes(raw[i] for i in range(0, VRAM_SIZE, 2))
            if b"Launcher" in text: break
            time.sleep(0.3)
        else:
            print("ERROR: UI didn't load")
            p.kill(); p.wait()
            return

        time.sleep(2.0)

        # Connect VNC
        print(f"Connecting to VNC on port {vnc_port}...")
        vnc = VNCKeyClient("127.0.0.1", vnc_port)

        # Get initial state
        raw = read_vram(qmp, tmp)
        initial_row = find_selected_row(raw)
        print(f"Initial selection: row {initial_row}")

        # Send two DOWN keys via VNC with specified interval
        print(f"\nSending 2 DOWN keys via VNC, {interval_ms}ms apart...")
        t0 = time.monotonic()

        vnc.press_key(XK_Down, hold_ms=30)
        time.sleep(interval_ms / 1000.0)
        vnc.press_key(XK_Down, hold_ms=30)

        # Wait up to 5 seconds for both to process
        for _ in range(500):
            raw = read_vram(qmp, tmp)
            current_row = find_selected_row(raw)
            moved = current_row - initial_row if current_row >= 0 else -1
            if moved >= 2:
                elapsed = (time.monotonic() - t0) * 1000
                print(f"Both keys processed: {elapsed:.0f}ms total, moved {moved} rows")
                vnc.close()
                qmp.send_key("f10")
                time.sleep(0.5)
                p.kill(); p.wait()
                return elapsed
            time.sleep(0.01)

        # Timeout
        elapsed = (time.monotonic() - t0) * 1000
        raw = read_vram(qmp, tmp)
        current_row = find_selected_row(raw)
        moved = current_row - initial_row if current_row >= 0 else -1
        print(f"TIMEOUT: {elapsed:.0f}ms, moved {moved} rows")

        vnc.close()
        p.kill(); p.wait()
        return -1

    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <AMLUI.EXE> [interval_ms]")
        sys.exit(1)

    path = sys.argv[1]
    interval = int(sys.argv[2]) if len(sys.argv) > 2 else 100

    print(f"Testing: {path} ({os.path.getsize(path)} bytes)")
    print(f"Key interval: {interval}ms")
    print()

    # Run multiple intervals
    for ivl in [500, 200, 100, 50]:
        result = run_test(path, ivl)
        if result and result > 0:
            status = "OK" if result < ivl + 500 else "SLOW"
        else:
            status = "FAIL"
        print(f"  interval={ivl}ms: {status}\n")


if __name__ == "__main__":
    main()
