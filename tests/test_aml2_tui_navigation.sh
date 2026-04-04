#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$REPO_ROOT/out"
BASE_IMG="${MSDOS_BASE_IMG:-$OUT_DIR/floppy-minimal.img}"
BOOT_IMG="$OUT_DIR/aml2-tui.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
QMP_SOCK="$OUT_DIR/aml2-tui-qmp.sock"
SCREEN_LOG="$OUT_DIR/aml2-tui-screen.log"
QEMU_LOG="$OUT_DIR/aml2-tui-qemu.log"
DOS_RELEASE_TAG="${DOS_RELEASE_TAG:-0.1}"
DOS_IMAGE_URL="${DOS_IMAGE_URL:-https://github.com/ddanila/msdos/releases/download/${DOS_RELEASE_TAG}/floppy-minimal.img}"
QEMU_PID=""

cleanup() {
    if [[ -n "$QEMU_PID" ]]; then
        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
    fi
    rm -f "$QMP_SOCK" "$AUTOEXEC"
}

download_base_img() {
    mkdir -p "$OUT_DIR"
    if [[ -f "$BASE_IMG" ]]; then
        return
    fi

    python3 - "$DOS_IMAGE_URL" "$BASE_IMG" <<'PY'
import shutil
import sys
import urllib.request

url, dest = sys.argv[1], sys.argv[2]
with urllib.request.urlopen(url) as response, open(dest, "wb") as out:
    shutil.copyfileobj(response, out)
PY
}

trap cleanup EXIT

mkdir -p "$OUT_DIR"

echo "Building aml2 for TUI navigation test ..."
BUILD_TARGETS=all EXTRA_CFLAGS="-DAML_TEST_HOOKS=1" "$REPO_ROOT/tools/build.sh"
download_base_img

cp "$BASE_IMG" "$BOOT_IMG"
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/amlui.exe" ::AMLUI.EXE
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/tests/launcher.long.cfg" ::LAUNCHER.CFG
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/tests/AML2.END" ::AML2.AUT
printf '@ECHO OFF\r\nAMLUI.EXE /V\r\n' > "$AUTOEXEC"
mcopy -o -i "$BOOT_IMG" "$AUTOEXEC" ::AUTOEXEC.BAT

rm -f "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG"
timeout 20 qemu-system-i386 \
    -display none \
    -monitor none \
    -serial none \
    -drive if=floppy,index=0,format=raw,file="$BOOT_IMG" \
    -boot a \
    -m 4 \
    -qmp unix:"$QMP_SOCK",server,nowait \
    >"$QEMU_LOG" 2>&1 &
QEMU_PID=$!

for _ in $(seq 1 50); do
    [[ -S "$QMP_SOCK" ]] && break
    sleep 0.2
done

SCREEN_EXPECT_TIMEOUT=8 python3 "$REPO_ROOT/tests/screen_expect.py" \
    "$QMP_SOCK" "$SCREEN_LOG" \
    'Arvutimuuseum Launcher' '' \
    'Entry 19' '' \
    'A>' ''

kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true
QEMU_PID=""

grep -q 'Danila Sukharev' "$SCREEN_LOG"
grep -q 'Entry 19' "$SCREEN_LOG"
grep -q '^A>$' "$SCREEN_LOG"

echo "tui navigation test passed"
