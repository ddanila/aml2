#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$REPO_ROOT/out"
BASE_IMG="${MSDOS_BASE_IMG:-$OUT_DIR/floppy-minimal.img}"
BOOT_IMG="$OUT_DIR/aml2-edit.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
QMP_SOCK="$OUT_DIR/aml2-edit-qmp.sock"
SCREEN_LOG="$OUT_DIR/aml2-edit-screen.log"
QEMU_LOG="$OUT_DIR/aml2-edit-qemu.log"
CFG_LOG="$OUT_DIR/aml2-edit-launcher.cfg"
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
    local auto="$2"

    echo "=== $name ==="
    cp "$BASE_IMG" "$BOOT_IMG"
    mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/aml2.exe" ::AML2.EXE
    mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/tests/launcher.edit.cfg" ::LAUNCHER.CFG
    mcopy -o -i "$BOOT_IMG" "$auto" ::AML2.AUT
    printf '@ECHO OFF\r\nAML2.EXE\r\n' > "$AUTOEXEC"
    mcopy -o -i "$BOOT_IMG" "$AUTOEXEC" ::AUTOEXEC.BAT

    rm -f "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG" "$CFG_LOG"
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
        'A>' ''

    kill "$QEMU_PID" 2>/dev/null || true
    wait "$QEMU_PID" 2>/dev/null || true
    QEMU_PID=""

    mtype -i "$BOOT_IMG" ::LAUNCHER.CFG > "$CFG_LOG"
    tr -d '\r' < "$CFG_LOG" > "$CFG_LOG.tmp"
    mv "$CFG_LOG.tmp" "$CFG_LOG"
}

trap cleanup EXIT

mkdir -p "$OUT_DIR"

echo "Building aml2 for edit-operation tests ..."
BUILD_TARGETS=all EXTRA_CFLAGS="-DAML_TEST_HOOKS=1" "$REPO_ROOT/tools/build.sh"
download_base_img

run_case "reorder then save" "$REPO_ROOT/tests/AML2.EDR"
grep -q '^Alpha|ALPHA.EXE|$' "$CFG_LOG"
grep -q '^Gamma|GAMMA.EXE|$' "$CFG_LOG"
grep -q '^Beta|BETA.EXE|$' "$CFG_LOG"
python3 - "$CFG_LOG" <<'PY'
import sys

lines = [
    line.strip()
    for line in open(sys.argv[1], encoding="ascii")
    if "|" in line and not line.lstrip().startswith("#")
]
expected = [
    "Alpha|ALPHA.EXE|",
    "Gamma|GAMMA.EXE|",
    "Beta|BETA.EXE|",
]
if lines != expected:
    raise SystemExit(f"unexpected reorder result: {lines!r}")
PY

run_case "delete then save" "$REPO_ROOT/tests/AML2.EDD"
grep -q '^Alpha|ALPHA.EXE|$' "$CFG_LOG"
grep -q '^Gamma|GAMMA.EXE|$' "$CFG_LOG"
if grep -q '^Beta|BETA.EXE|$' "$CFG_LOG"; then
    echo "deleted entry still present"
    exit 1
fi
python3 - "$CFG_LOG" <<'PY'
import sys

lines = [
    line.strip()
    for line in open(sys.argv[1], encoding="ascii")
    if "|" in line and not line.lstrip().startswith("#")
]
expected = [
    "Alpha|ALPHA.EXE|",
    "Gamma|GAMMA.EXE|",
]
if lines != expected:
    raise SystemExit(f"unexpected delete result: {lines!r}")
PY

echo "edit-operation tests passed"
