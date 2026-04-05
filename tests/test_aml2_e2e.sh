#!/bin/bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/lib_dos.sh"

aml_test_init_paths "aml2_e2e"

OUT_DIR="$AML_TEST_OUT_DIR"
BASE_IMG="$AML_TEST_BASE_IMG"
BOOT_IMG="$OUT_DIR/aml2-e2e.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
QMP_SOCK="$OUT_DIR/aml2-e2e-qmp.sock"
SCREEN_LOG="$OUT_DIR/aml2-e2e-screen.log"
QEMU_LOG="$OUT_DIR/aml2-e2e-qemu.log"

PASS=0
FAIL=0

ok() {
    echo "  PASS: $1"
    PASS=$((PASS + 1))
}

fail() {
    echo "  FAIL: $1"
    FAIL=$((FAIL + 1))
}

cleanup() {
    aml_test_cleanup_qemu
    aml_test_cleanup_files "$QMP_SOCK" "$AUTOEXEC"
}

trap cleanup EXIT

echo "=== aml2 DOS e2e ==="

echo "Building launcher, stub, and fake game ..."
BUILD_TARGETS=test-build EXTRA_CFLAGS="-DAML_TEST_HOOKS=1" "$REPO_ROOT/tools/build.sh"

aml_test_download_base_img

echo "Preparing boot floppy ..."
cp "$BASE_IMG" "$BOOT_IMG"
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/amlui.exe" ::AMLUI.EXE
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/aml.com" ::AML.COM
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/fakegame.exe" ::FAKEGAME.EXE
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/tests/launcher.e2e.cfg" ::LAUNCHER.CFG
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/tests/AML2.AUT" ::AML2.AUT
aml_test_write_autoexec "$AUTOEXEC" "AML.COM"
mcopy -o -i "$BOOT_IMG" "$AUTOEXEC" ::AUTOEXEC.BAT

echo "Booting QEMU ..."
rm -f "$SCREEN_LOG"
aml_test_start_qemu "$BOOT_IMG" "$QMP_SOCK" "$QEMU_LOG" 35
aml_test_wait_for_qmp "$QMP_SOCK"

echo "Driving aml2 and fake game ..."
SCREEN_EXPECT_TIMEOUT=12 python3 "$REPO_ROOT/tests/screen_expect.py" \
    "$QMP_SOCK" "$SCREEN_LOG" \
    'Arvutimuuseum Launcher' '' \
    'A>' ''

aml_test_stop_qemu

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

if grep -q "Arvutimuuseum Launcher" "$SCREEN_LOG"; then
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
