#!/bin/bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/lib_dos.sh"

aml_test_init_paths "aml2_failure_paths"

OUT_DIR="$AML_TEST_OUT_DIR"
BASE_IMG="$AML_TEST_BASE_IMG"
BOOT_IMG="$OUT_DIR/aml2-fail.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
QMP_SOCK="$OUT_DIR/aml2-fail-qmp.sock"
SCREEN_LOG="$OUT_DIR/aml2-fail-screen.log"
QEMU_LOG="$OUT_DIR/aml2-fail-qemu.log"
TRACE_LOG="$OUT_DIR/aml2-fail-trace.log"
TRACE_NORM="$OUT_DIR/aml2-fail-trace.norm.log"

cleanup() {
    aml_test_cleanup_qemu
    aml_test_cleanup_files "$QMP_SOCK" "$AUTOEXEC"
}

run_case() {
    local name="$1"
    local launch_cmd="$2"
    local cfg="$3"
    local include_launcher="$4"
    local pattern="$5"
    local auto="$6"
    local final_pattern="$7"

    echo "=== $name ==="
    cp "$BASE_IMG" "$BOOT_IMG"
    mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/aml.com" ::AML.COM
    if [[ "$include_launcher" == "yes" ]]; then
        mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/amlui.exe" ::AMLUI.EXE
    fi
    mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/fakegame.exe" ::FAKEGAME.EXE
    mcopy -o -i "$BOOT_IMG" "$cfg" ::LAUNCHER.CFG
    mcopy -o -i "$BOOT_IMG" "$auto" ::AML2.AUT
    aml_test_write_autoexec "$AUTOEXEC" "$launch_cmd"
    mcopy -o -i "$BOOT_IMG" "$AUTOEXEC" ::AUTOEXEC.BAT

    rm -f "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG" "$TRACE_LOG" "$TRACE_NORM"
    aml_test_start_qemu "$BOOT_IMG" "$QMP_SOCK" "$QEMU_LOG" 20
    aml_test_wait_for_qmp "$QMP_SOCK"

    SCREEN_EXPECT_TIMEOUT=8 python3 "$REPO_ROOT/tests/screen_expect.py" \
        "$QMP_SOCK" "$SCREEN_LOG" \
        "$pattern" '' \
        "$final_pattern" ''

    aml_test_stop_qemu

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

echo "Building launcher, stub, and fake game ..."
BUILD_TARGETS=test-build EXTRA_CFLAGS="-DAML_TEST_HOOKS=1" "$REPO_ROOT/tools/build.sh"
aml_test_download_base_img

run_case "missing launcher" "AML.COM" "$REPO_ROOT/tests/launcher.e2e.cfg" "no" "NO AMLUI.EXE" "$REPO_ROOT/tests/AML2.AUT" "A>"
run_case "bad path" "AML.COM" "$REPO_ROOT/tests/launcher.badpath.cfg" "yes" "Game folder not found" "$REPO_ROOT/tests/AML2.LAUNCH" ""
run_case "direct amlui launch" "AMLUI.EXE /V" "$REPO_ROOT/tests/launcher.e2e.cfg" "yes" "Start with AML.COM to launch games." "$REPO_ROOT/tests/AML2.LAUNCH" ""

echo "failure path checks passed"
