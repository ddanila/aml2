#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$REPO_ROOT/out"
BASE_IMG="${MSDOS_BASE_IMG:-$OUT_DIR/floppy-minimal.img}"
BOOT_IMG="$OUT_DIR/aml2-e2e.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
QMP_SOCK="$OUT_DIR/aml2-e2e-qmp.sock"
SCREEN_LOG="$OUT_DIR/aml2-e2e-screen.log"
QEMU_LOG="$OUT_DIR/aml2-e2e-qemu.log"
DOS_RELEASE_TAG="${DOS_RELEASE_TAG:-0.1}"
DOS_IMAGE_URL="${DOS_IMAGE_URL:-https://github.com/ddanila/msdos/releases/download/${DOS_RELEASE_TAG}/floppy-minimal.img}"

PASS=0
FAIL=0
QEMU_PID=""

ok() {
    echo "  PASS: $1"
    PASS=$((PASS + 1))
}

fail() {
    echo "  FAIL: $1"
    FAIL=$((FAIL + 1))
}

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

    echo "Downloading minimal DOS floppy from $DOS_IMAGE_URL ..."
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

echo "=== aml2 DOS e2e ==="

echo "Building launcher, stub, and fake game ..."
BUILD_TARGETS=test-build EXTRA_CFLAGS="-DAML_TEST_HOOKS=1" "$REPO_ROOT/tools/build.sh"

download_base_img

echo "Preparing boot floppy ..."
cp "$BASE_IMG" "$BOOT_IMG"
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/aml2.exe" ::AML2.EXE
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/amlstub.com" ::AMLSTUB.COM
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/fakegame.exe" ::FAKEGAME.EXE
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/tests/launcher.e2e.cfg" ::LAUNCHER.CFG
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/tests/AML2.AUT" ::AML2.AUT
printf '@ECHO OFF\r\nAMLSTUB.COM\r\n' > "$AUTOEXEC"
mcopy -o -i "$BOOT_IMG" "$AUTOEXEC" ::AUTOEXEC.BAT

echo "Booting QEMU ..."
rm -f "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG"
timeout 35 qemu-system-i386 \
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

if [[ ! -S "$QMP_SOCK" ]]; then
    echo "ERROR: QMP socket did not appear"
    exit 1
fi

echo "Driving aml2 and fake game ..."
SCREEN_EXPECT_TIMEOUT=12 python3 "$REPO_ROOT/tests/screen_expect.py" \
    "$QMP_SOCK" "$SCREEN_LOG" \
    'Arvutimuuseum Launcher v2' '' \
    'A>' ''

kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true
QEMU_PID=""

TRACE_LOG="$OUT_DIR/aml2-trace.log"
mtype -i "$BOOT_IMG" ::AML2.TRC > "$TRACE_LOG"
TRACE_NORM="$OUT_DIR/aml2-trace.norm.log"
tr -d '\r' < "$TRACE_LOG" > "$TRACE_NORM"

echo ""
echo "--- aml2 checks ---"

if [[ -s "$SCREEN_LOG" ]]; then
    ok "Screen log created"
else
    fail "Screen log missing or empty"
fi

if grep -q "Arvutimuuseum Launcher v2" "$SCREEN_LOG"; then
    ok "Launcher screen rendered"
else
    fail "Launcher screen not found"
fi

if grep -q '^launcher$' "$TRACE_NORM"; then
    ok "Launcher trace written"
else
    fail "Launcher trace missing"
fi

if grep -q '^auto_launch$' "$TRACE_NORM"; then
    ok "Automation selected the launcher entry"
else
    fail "Automation did not trigger launch selection"
fi

if grep -q '^run_request$' "$TRACE_NORM"; then
    ok "Launcher wrote AML2.RUN"
else
    fail "Launcher did not write AML2.RUN"
fi

if grep -q '^fakegame$' "$TRACE_NORM"; then
    ok "Stub launched the selected command"
else
    fail "Stub did not launch the selected command"
fi

if [[ "$(grep -c '^launcher$' "$TRACE_NORM")" -ge 2 ]] && grep -q '^auto_quit$' "$TRACE_NORM"; then
    ok "Launcher returned after game exit"
else
    fail "Launcher did not return after game exit"
fi

if grep -q "=== Final screen ===" "$SCREEN_LOG"; then
    ok "Final screen captured"
else
    fail "Final screen not captured"
fi

if grep -q '^A>$' "$SCREEN_LOG"; then
    ok "Returned to DOS prompt after automated quit"
else
    fail "DOS prompt not observed"
fi

if [[ $FAIL -gt 0 ]]; then
    echo ""
    echo "--- screen log ---"
    cat "$SCREEN_LOG" 2>/dev/null || true
    echo "--- end screen log ---"
    echo "--- trace log ---"
    cat "$TRACE_LOG" 2>/dev/null || true
    echo "--- end trace log ---"
    echo "--- qemu log ---"
    cat "$QEMU_LOG" 2>/dev/null || true
    echo "--- end qemu log ---"
fi

echo ""
echo "Results: $PASS passed, $FAIL failed"
[[ $FAIL -eq 0 ]]
