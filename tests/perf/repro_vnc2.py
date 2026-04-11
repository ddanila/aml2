#!/usr/bin/env python3
"""
Reproduce lag via VNC: measure INDIVIDUAL key response times.
Sends keys via VNC and measures time until each screen change.
"""

import os, shutil, socket, struct, subprocess, sys, tempfile, time

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, "tests", "lib"))
from screen_expect import QMPConnection

VRAM_PHYS = 0xB8000
VRAM_SIZE = 4000
DOS_IMAGE_URL = "https://github.com/ddanila/msdos/releases/download/0.1/floppy-minimal.img"
XK_Down = 0xFF54
XK_F10 = 0xFFC7


def download_base_image(dest):
    if not os.path.exists(dest):
        import urllib.request
        urllib.request.urlretrieve(DOS_IMAGE_URL, dest)


def read_vram(qmp, tmp):
    qmp.human_cmd(f'pmemsave 0x{VRAM_PHYS:X} {VRAM_SIZE} "{tmp}"')
    with open(tmp, "rb") as f:
        return f.read(VRAM_SIZE)


class VNCKeyClient:
    def __init__(self, host, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        ver = self.sock.recv(12)
        self.sock.sendall(b"RFB 003.008\n")
        n = struct.unpack("B", self.sock.recv(1))[0]
        self.sock.recv(n)
        self.sock.sendall(struct.pack("B", 1))
        result = struct.unpack(">I", self.sock.recv(4))[0]
        if result != 0: raise RuntimeError(f"VNC auth failed: {result}")
        self.sock.sendall(struct.pack("B", 1))
        header = self.sock.recv(24)
        name_len = struct.unpack(">I", header[20:24])[0]
        if name_len > 0: self.sock.recv(name_len)

    def press_key(self, keysym, hold_ms=30):
        self.sock.sendall(struct.pack(">BBHI", 4, 1, 0, keysym))
        time.sleep(hold_ms / 1000.0)
        self.sock.sendall(struct.pack(">BBHI", 4, 0, 0, keysym))

    def close(self):
        self.sock.close()


def run_test(amlui_path):
    work = tempfile.mkdtemp(prefix="aml2_vnc2_")
    base = os.path.join(work, "base.img")
    boot = os.path.join(work, "boot.img")
    qmp_sock = os.path.join(work, "qmp.sock")
    tmp = os.path.join(work, "vram.bin")
    auto = os.path.join(work, "AUTOEXEC.BAT")
    cfg = os.path.join(REPO_ROOT, "tests", "perf", "LAUNCHER.CFG")
    vnc_port = 5950

    try:
        download_base_image(base)
        shutil.copy(base, boot)
        with open(auto, "wb") as f:
            f.write(b"@ECHO OFF\r\nAMLUI.EXE /V\r\n")
        for src, dst in [(amlui_path, "AMLUI.EXE"), (cfg, "LAUNCHER.CFG"), (auto, "AUTOEXEC.BAT")]:
            subprocess.run(["mcopy", "-o", "-i", boot, src, f"::{dst}"], check=True, capture_output=True)

        p = subprocess.Popen(
            ["qemu-system-i386", "-display", "none", "-vnc", f":{vnc_port - 5900}",
             "-drive", f"if=floppy,index=0,format=raw,file={boot}",
             "-boot", "a", "-m", "4",
             "-qmp", f"unix:{qmp_sock},server,nowait"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        for _ in range(50):
            if os.path.exists(qmp_sock): break
            time.sleep(0.2)

        qmp = QMPConnection(qmp_sock)

        # Wait for UI
        for _ in range(100):
            raw = read_vram(qmp, tmp)
            if b"Launcher" in bytes(raw[i] for i in range(0, VRAM_SIZE, 2)): break
            time.sleep(0.3)
        else:
            print("ERROR: UI didn't load"); p.kill(); p.wait(); return

        time.sleep(2.0)
        vnc = VNCKeyClient("127.0.0.1", vnc_port)

        # Measure each key individually
        for trial in range(3):
            # Snapshot BEFORE key
            baseline = read_vram(qmp, tmp)

            # Send DOWN via VNC
            t0 = time.monotonic()
            vnc.press_key(XK_Down, hold_ms=30)

            # Poll until screen changes (using pmemsave)
            for _ in range(1000):
                current = read_vram(qmp, tmp)
                if current != baseline:
                    elapsed = (time.monotonic() - t0) * 1000
                    # Find what changed
                    for i in range(len(baseline)):
                        if current[i] != baseline[i]:
                            row, col = divmod(i // 2, 80)
                            kind = "attr" if i % 2 else "char"
                            print(f"  DOWN #{trial+1}: {elapsed:.1f}ms "
                                  f"(change at row {row} col {col} {kind}: "
                                  f"0x{baseline[i]:02x}->0x{current[i]:02x})")
                            break
                    break
                time.sleep(0.005)
            else:
                print(f"  DOWN #{trial+1}: TIMEOUT (5s)")

        vnc.press_key(XK_F10)
        time.sleep(0.5)
        vnc.close()
        p.kill(); p.wait()

    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <AMLUI.EXE>"); sys.exit(1)
    path = sys.argv[1]
    print(f"Testing: {path} ({os.path.getsize(path)} bytes)\n")
    run_test(path)


if __name__ == "__main__":
    main()
