#!/usr/bin/env python3
"""
Measure key response via VNC framebuffer updates — NO pmemsave.

Uses the VNC protocol's FramebufferUpdate messages to detect screen
changes without pausing the vCPU. This avoids the pmemsave interference
that invalidated previous measurements.

Protocol:
1. Send FramebufferUpdateRequest (incremental=1) to request only changes
2. QEMU responds with FramebufferUpdate when pixels change
3. Measure time from key press to framebuffer update receipt
"""

import os, shutil, socket, struct, subprocess, sys, tempfile, time, select

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOS_IMAGE_URL = "https://github.com/ddanila/msdos/releases/download/0.1/floppy-minimal.img"

XK_Down = 0xFF54
XK_F10 = 0xFFC7


def download_base_image(dest):
    if not os.path.exists(dest):
        import urllib.request
        urllib.request.urlretrieve(DOS_IMAGE_URL, dest)


class VNCClient:
    """VNC client that sends keys and receives framebuffer updates."""

    def __init__(self, host, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        self.width = 0
        self.height = 0
        self.bpp = 0
        self._handshake()
        self.sock.setblocking(False)

    def _handshake(self):
        self.sock.setblocking(True)
        self.sock.recv(12)
        self.sock.sendall(b"RFB 003.008\n")
        n = struct.unpack("B", self.sock.recv(1))[0]
        self.sock.recv(n)
        self.sock.sendall(struct.pack("B", 1))  # None auth
        struct.unpack(">I", self.sock.recv(4))
        self.sock.sendall(struct.pack("B", 1))  # Shared
        header = self.sock.recv(24)
        self.width = struct.unpack(">H", header[0:2])[0]
        self.height = struct.unpack(">H", header[2:4])[0]
        self.bpp = header[4]  # bits per pixel
        name_len = struct.unpack(">I", header[20:24])[0]
        if name_len > 0:
            self.sock.recv(name_len)

        # Set pixel format to 8bpp for simplicity (or keep default)
        # Request initial full framebuffer
        self._request_fb_update(incremental=0)
        # Drain the initial update
        self.sock.setblocking(True)
        self._drain_fb_update(timeout=10)

    def _request_fb_update(self, incremental=1):
        """Send FramebufferUpdateRequest."""
        msg = struct.pack(">BBHHHH", 3, incremental, 0, 0, self.width, self.height)
        self.sock.sendall(msg)

    def _recv_exact(self, n, timeout=5):
        """Receive exactly n bytes with timeout."""
        data = b""
        deadline = time.monotonic() + timeout
        while len(data) < n:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            ready, _, _ = select.select([self.sock], [], [], min(remaining, 0.1))
            if ready:
                try:
                    chunk = self.sock.recv(n - len(data))
                    if not chunk:
                        return None
                    data += chunk
                except BlockingIOError:
                    pass
        return data

    def _drain_fb_update(self, timeout=5):
        """Read and discard one complete FramebufferUpdate message."""
        self.sock.setblocking(False)
        header = self._recv_exact(4, timeout)
        if header is None:
            return False
        msg_type = header[0]
        if msg_type != 0:  # Not a FramebufferUpdate
            return False
        n_rects = struct.unpack(">H", header[2:4])[0]
        for _ in range(n_rects):
            rect_header = self._recv_exact(12, timeout)
            if rect_header is None:
                return False
            w = struct.unpack(">H", rect_header[4:6])[0]
            h = struct.unpack(">H", rect_header[6:8])[0]
            encoding = struct.unpack(">i", rect_header[8:12])[0]
            if encoding == 0:  # Raw
                pixel_bytes = w * h * (self.bpp // 8)
                pixels = self._recv_exact(pixel_bytes, timeout)
                if pixels is None:
                    return False
            elif encoding == 1:  # CopyRect
                self._recv_exact(4, timeout)
            # Other encodings: skip
        return True

    def press_key(self, keysym, hold_ms=30):
        self.sock.setblocking(True)
        self.sock.sendall(struct.pack(">BBHI", 4, 1, 0, keysym))
        time.sleep(hold_ms / 1000.0)
        self.sock.sendall(struct.pack(">BBHI", 4, 0, 0, keysym))
        self.sock.setblocking(False)

    def wait_for_fb_update(self, timeout=10):
        """Request incremental update and wait for response.
        Returns time in ms from request to first update, or -1 on timeout."""
        self._request_fb_update(incremental=1)
        t0 = time.monotonic()
        result = self._drain_fb_update(timeout)
        if result:
            return (time.monotonic() - t0) * 1000
        return -1

    def measure_key_response(self, keysym, label):
        """Send a key and measure time until framebuffer updates."""
        # First, drain any pending updates
        self._request_fb_update(incremental=1)
        self._drain_fb_update(timeout=0.5)

        # Now send the key and request update
        t0 = time.monotonic()
        self.press_key(keysym)

        # Request incremental update and wait
        self._request_fb_update(incremental=1)
        if self._drain_fb_update(timeout=10):
            elapsed = (time.monotonic() - t0) * 1000
            print(f"  {label}: {elapsed:.1f}ms")
            return elapsed
        else:
            print(f"  {label}: TIMEOUT")
            return -1

    def close(self):
        self.sock.close()


def run_test(amlui_path, label):
    work = tempfile.mkdtemp(prefix="aml2_vncfb_")
    base_img = "/tmp/aml2_base_floppy.img"
    boot = os.path.join(work, "boot.img")
    sock = os.path.join(work, "qmp.sock")
    auto = os.path.join(work, "AUTO.BAT")
    cfg = os.path.join(REPO_ROOT, "tests", "perf", "LAUNCHER.CFG")
    vnc_port = 5950

    try:
        download_base_image(base_img)
        shutil.copy(base_img, boot)
        with open(auto, "wb") as f:
            f.write(b"@ECHO OFF\r\nAMLUI.EXE /V\r\n")
        for src, dst in [(amlui_path, "AMLUI.EXE"), (cfg, "LAUNCHER.CFG"), (auto, "AUTOEXEC.BAT")]:
            subprocess.run(["mcopy", "-o", "-i", boot, src, f"::{dst}"], check=True, capture_output=True)

        p = subprocess.Popen(
            ["qemu-system-i386", "-display", "none", "-vnc", f":{vnc_port - 5900}",
             "-drive", f"if=floppy,index=0,format=raw,file={boot}",
             "-boot", "a", "-m", "4"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        time.sleep(5)  # Wait for DOS boot + UI render

        vnc = VNCClient("127.0.0.1", vnc_port)
        time.sleep(2)  # Extra settle time

        print(f"\n[{label}] Measuring key response via VNC framebuffer updates:")
        ms1 = vnc.measure_key_response(XK_Down, "DOWN #1")
        ms2 = vnc.measure_key_response(XK_Down, "DOWN #2")
        ms3 = vnc.measure_key_response(XK_Down, "DOWN #3")

        print(f"\n[{label}] Results: {ms1:.0f}ms, {ms2:.0f}ms, {ms3:.0f}ms")
        if ms1 > 0 and ms2 > 0:
            print(f"[{label}] Ratio #2/#1: {ms2/ms1:.1f}x")

        vnc.press_key(XK_F10)
        time.sleep(0.5)
        vnc.close()
        p.kill()
        p.wait()

    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <AMLUI.EXE> [label]")
        sys.exit(1)
    path = sys.argv[1]
    label = sys.argv[2] if len(sys.argv) > 2 else os.path.basename(path)
    print(f"Testing: {path} ({os.path.getsize(path)} bytes)")
    run_test(path, label)


if __name__ == "__main__":
    main()
