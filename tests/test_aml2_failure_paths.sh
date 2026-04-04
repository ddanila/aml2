#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$REPO_ROOT/out"
BASE_IMG="${MSDOS_BASE_IMG:-$OUT_DIR/floppy-minimal.img}"
BOOT_IMG="$OUT_DIR/aml2-fail.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
QMP_SOCK="$OUT_DIR/aml2-fail-qmp.sock"
SCREEN_LOG="$OUT_DIR/aml2-fail-screen.log"
QEMU_LOG="$OUT_DIR/aml2-fail-qemu.log"
TRACE_LOG="$OUT_DIR/aml2-fail-trace.log"
TRACE_NORM="$OUT_DIR/aml2-fail-trace.norm.log"
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
    local include_launcher="$3"
    local pattern="$4"
    local auto="$5"
    local final_pattern="$6"

    echo "=== $name ==="
    cp "$BASE_IMG" "$BOOT_IMG"
    mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/aml.com" ::AML.COM
    if [[ "$include_launcher" == "yes" ]]; then
        mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/amlui.exe" ::AMLUI.EXE
    fi
    mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/fakegame.exe" ::FAKEGAME.EXE
    mcopy -o -i "$BOOT_IMG" "$cfg" ::LAUNCHER.CFG
    mcopy -o -i "$BOOT_IMG" "$auto" ::AML2.AUT
    printf '@ECHO OFF\r\nAML.COM\r\n' > "$AUTOEXEC"
    mcopy -o -i "$BOOT_IMG" "$AUTOEXEC" ::AUTOEXEC.BAT

    rm -f "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG" "$TRACE_LOG" "$TRACE_NORM"
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
        "$pattern" '' \
        "$final_pattern" ''

    kill "$QEMU_PID" 2>/dev/null || true
    wait "$QEMU_PID" 2>/dev/null || true
    QEMU_PID=""

    if mdir -i "$BOOT_IMG" ::AML2.TRC >/dev/null 2>&1; then
        mtype -i "$BOOT_IMG" ::AML2.TRC > "$TRACE_LOG"
        tr -d '\r' < "$TRACE_LOG" > "$TRACE_NORM"
    else
        : > "$TRACE_NORM"
    fi

    grep -q "$pattern" "$SCREEN_LOG"
    if [[ -n "$final_pattern" ]]; then
        grep -q "^$final_pattern$" "$SCREEN_LOG"
    fi
}

trap cleanup EXIT

mkdir -p "$OUT_DIR"

echo "Building launcher, stub, and fake game ..."
BUILD_TARGETS=test-build EXTRA_CFLAGS="-DAML_TEST_HOOKS=1" "$REPO_ROOT/tools/build.sh"
mkdir -p "$OUT_DIR"
download_base_img

run_case "missing launcher" "$REPO_ROOT/tests/launcher.e2e.cfg" "no" "NO AMLUI.EXE" "$REPO_ROOT/tests/AML2.AUT" "A>"
run_case "bad path" "$REPO_ROOT/tests/launcher.badpath.cfg" "yes" "Game folder not found" "$REPO_ROOT/tests/AML2.LAUNCH" ""

echo "failure path checks passed"
