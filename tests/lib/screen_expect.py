#!/usr/bin/env python3
"""Minimal QEMU text-mode screen matcher for DOS E2E tests."""

import json
import os
import socket
import sys
import tempfile
import time


VRAM_PHYS = 0xB8000
VRAM_SIZE = 4000
COLS = 80
ROWS = 25
BIGTEXT_COL = 11
BIGTEXT_ROWS = list(range(2, 22, 2))
POLL_INTERVAL = 0.3
KEY_DELAY = 0.05
POST_MATCH_DELAY = float(os.environ.get("SCREEN_EXPECT_POST_MATCH_DELAY", "0.8"))
TIMEOUT = float(os.environ.get("SCREEN_EXPECT_TIMEOUT", "20"))


class QMPConnection:
    def __init__(self, sock_path: str, retries: int = 10):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(10.0)
        for attempt in range(retries):
            try:
                self.sock.connect(sock_path)
                break
            except (ConnectionRefusedError, FileNotFoundError):
                if attempt == retries - 1:
                    raise
                time.sleep(0.5)
        self._buf = b""
        self._recv_json()
        self._send({"execute": "qmp_capabilities"})
        self._recv_json()

    def _send(self, obj: dict) -> None:
        self.sock.sendall(json.dumps(obj).encode() + b"\n")

    def _recv_json(self) -> dict:
        while True:
            nl = self._buf.find(b"\n")
            if nl >= 0:
                line = self._buf[:nl]
                self._buf = self._buf[nl + 1 :]
                if line.strip():
                    try:
                        return json.loads(line)
                    except json.JSONDecodeError:
                        pass
            else:
                chunk = self.sock.recv(65536)
                if not chunk:
                    raise ConnectionError("QMP connection closed")
                self._buf += chunk

    def _recv_response(self) -> dict:
        while True:
            obj = self._recv_json()
            if "return" in obj or "error" in obj:
                return obj

    def human_cmd(self, cmd_line: str) -> str:
        self._send(
            {
                "execute": "human-monitor-command",
                "arguments": {"command-line": cmd_line},
            }
        )
        resp = self._recv_response()
        return resp.get("return", "")

    def send_key(self, qcode: str) -> None:
        self._send(
            {
                "execute": "send-key",
                "arguments": {"keys": [{"type": "qcode", "data": qcode}]},
            }
        )
        self._recv_response()

    def close(self) -> None:
        self.sock.close()


def build_bigtext_quad_map() -> dict[tuple[int, int, int, int], str]:
    reserved = {176, 179, 191, 192, 196, 217, 218, 219, 250}
    codes: list[int] = []

    for code in range(0x00, 0x1B):
        if code not in reserved:
            codes.append(code)
    for code in range(0x80, 0x100):
        if code not in reserved:
            codes.append(code)

    glyphs = [chr(code) for code in range(ord("A"), ord("Z") + 1)]
    glyphs.extend(chr(code) for code in range(ord("0"), ord("9") + 1))

    quad_map: dict[tuple[int, int, int, int], str] = {}
    for idx, ch in enumerate(glyphs):
        base = idx * 4
        quad_map[(codes[base], codes[base + 1], codes[base + 2], codes[base + 3])] = ch
    return quad_map


BIGTEXT_QUAD_MAP = build_bigtext_quad_map()


def decode_bigtext_row(chars: bytes, top_row: int) -> str:
    if top_row < 0 or top_row + 1 >= ROWS:
        return ""

    top = chars[top_row * COLS : (top_row + 1) * COLS]
    bottom = chars[(top_row + 1) * COLS : (top_row + 2) * COLS]
    col = BIGTEXT_COL
    out: list[str] = []

    while col + 1 < COLS:
        quad = (top[col], top[col + 1], bottom[col], bottom[col + 1])
        if quad in BIGTEXT_QUAD_MAP:
            out.append(BIGTEXT_QUAD_MAP[quad])
            col += 2
            if col < COLS and top[col] == 32 and bottom[col] == 32:
                col += 1
            continue

        if top[col] == 32 and bottom[col] == 32:
            if out and out[-1] != " ":
                out.append(" ")
            col += 1
            continue

        if out:
            break
        col += 1

    return "".join(out).strip()


def decode_bigtext_lines(chars: bytes) -> list[str]:
    lines = []
    for row in BIGTEXT_ROWS:
        decoded = decode_bigtext_row(chars, row)
        if decoded:
            lines.append(decoded)
    return lines


def read_screen_text(qmp: QMPConnection, tmp_path: str) -> str:
    qmp.human_cmd(f'pmemsave 0x{VRAM_PHYS:X} {VRAM_SIZE} "{tmp_path}"')
    try:
        with open(tmp_path, "rb") as f:
            raw = f.read(VRAM_SIZE)
    except FileNotFoundError:
        return ""
    if len(raw) < VRAM_SIZE:
        return ""

    chars = bytes(raw[i] for i in range(0, VRAM_SIZE, 2))
    lines = []
    for row in range(ROWS):
        line = chars[row * COLS : (row + 1) * COLS]
        lines.append(line.decode("cp437", errors="replace").rstrip())
    for decoded in decode_bigtext_lines(chars):
        lines.append(f"[BIG] {decoded}")
    return "\n".join(lines)


def send_keys(qmp: QMPConnection, keys_str: str) -> None:
    if not keys_str:
        return
    for key in keys_str.split("+"):
        key = key.strip()
        if not key:
            continue
        qmp.send_key(key)
        time.sleep(KEY_DELAY)


def main() -> None:
    args = sys.argv[1:]
    if len(args) < 4 or len(args) % 2 != 0:
        sys.exit("usage: screen_expect.py qmp_sock screen_log [pattern response]...")

    qmp_sock = args[0]
    log_path = args[1]
    rules = [(args[i], args[i + 1]) for i in range(2, len(args), 2)]

    qmp = QMPConnection(qmp_sock)
    rule_idx = 0
    deadline = time.monotonic() + TIMEOUT

    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
        tmp_path = tmp.name

    log = open(log_path, "w")
    try:
        while rule_idx < len(rules) and time.monotonic() < deadline:
            screen = read_screen_text(qmp, tmp_path)
            if not screen:
                time.sleep(POLL_INTERVAL)
                continue

            pattern, response = rules[rule_idx]
            if pattern in screen:
                log.write(f"=== Rule {rule_idx}: matched '{pattern}' ===\n")
                log.write(screen + "\n\n")
                time.sleep(POST_MATCH_DELAY)
                send_keys(qmp, response)
                rule_idx += 1
                time.sleep(0.5)
            else:
                time.sleep(POLL_INTERVAL)

        time.sleep(1.0)
        screen = read_screen_text(qmp, tmp_path)
        log.write("=== Final screen ===\n")
        log.write(screen + "\n")

        if rule_idx < len(rules):
            log.write(f"\nTIMEOUT: only {rule_idx}/{len(rules)} rules matched\n")
            sys.exit(1)
    finally:
        log.close()
        os.unlink(tmp_path)
        qmp.close()


if __name__ == "__main__":
    main()
