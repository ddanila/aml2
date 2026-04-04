#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$REPO_ROOT/out"
BASE_IMG="${MSDOS_BASE_IMG:-$OUT_DIR/floppy-minimal.img}"
BOOT_IMG="$OUT_DIR/aml2-help.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
QMP_SOCK="$OUT_DIR/aml2-help-qmp.sock"
SCREEN_LOG="$OUT_DIR/aml2-help-screen.log"
QEMU_LOG="$OUT_DIR/aml2-help-qemu.log"
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

run_case() {
    local name="$1"
    local cfg="$2"
    local launch_cmd="$3"
    local auto="$4"
    shift 4

    echo "=== $name ==="
    cp "$BASE_IMG" "$BOOT_IMG"
    mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/amledit.exe" ::AMLEDIT.EXE
    mcopy -o -i "$BOOT_IMG" "$cfg" ::LAUNCHER.CFG
    if [[ -n "$auto" ]]; then
        mcopy -o -i "$BOOT_IMG" "$auto" ::AML2.AUT
    fi
    printf '@ECHO OFF\r\n%s\r\n' "$launch_cmd" > "$AUTOEXEC"
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
        "$@"

    kill "$QEMU_PID" 2>/dev/null || true
    wait "$QEMU_PID" 2>/dev/null || true
    QEMU_PID=""
}

trap cleanup EXIT

mkdir -p "$OUT_DIR"

echo "Building aml2 for hotkey/help tests ..."
BUILD_TARGETS=all EXTRA_CFLAGS="-DAML_TEST_HOOKS=1" "$REPO_ROOT/tools/build.sh"
download_base_img

run_case \
    "extended hotkey range" \
    "$REPO_ROOT/tests/launcher.hotkeys.cfg" \
    "AMLEDIT.EXE" \
    "$REPO_ROOT/tests/AML2.HKA" \
    'Entry 36' '' \
    'A>' ''
grep -q 'Entry 36' "$SCREEN_LOG"

run_case \
    "help dialog" \
    "$REPO_ROOT/tests/launcher.e2e.cfg" \
    "AMLEDIT.EXE /E" \
    "$REPO_ROOT/tests/AML2.HLP" \
    'EDIT' 'Launcher Help'
grep -q 'F8     Delete current entry' "$SCREEN_LOG"
grep -q 'F10    Exit to DOS' "$SCREEN_LOG"

echo "hotkey/help tests passed"
