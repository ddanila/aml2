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
    aml_test_reset_boot_img "$BOOT_IMG"
    aml_test_copy_to_image "$BOOT_IMG" "$REPO_ROOT/aml.com" ::AML.COM
    if [[ "$include_launcher" == "yes" ]]; then
        aml_test_copy_to_image "$BOOT_IMG" "$REPO_ROOT/amlui.exe" ::AMLUI.EXE
    fi
    aml_test_copy_to_image "$BOOT_IMG" "$REPO_ROOT/fakegame.exe" ::FAKEGAME.EXE
    aml_test_copy_to_image "$BOOT_IMG" "$cfg" ::LAUNCHER.CFG
    aml_test_copy_to_image "$BOOT_IMG" "$auto" ::AML2.AUT
    aml_test_install_autoexec "$BOOT_IMG" "$AUTOEXEC" "$launch_cmd"

    rm -f "$TRACE_LOG" "$TRACE_NORM"
    aml_test_run_screen_case "$BOOT_IMG" "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG" 20 8 \
        "$pattern" '' \
        "$final_pattern" ''

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
aml_test_build test-build
aml_test_download_base_img

run_case "missing launcher" "AML.COM" "$REPO_ROOT/tests/launcher.e2e.cfg" "no" "NO AMLUI.EXE" "$REPO_ROOT/tests/AML2.AUT" "A>"
run_case "bad path" "AML.COM" "$REPO_ROOT/tests/launcher.badpath.cfg" "yes" "Game folder not found" "$REPO_ROOT/tests/AML2.LAUNCH" ""
run_case "direct amlui launch" "AMLUI.EXE /V" "$REPO_ROOT/tests/launcher.e2e.cfg" "yes" "Start with AML.COM to launch games." "$REPO_ROOT/tests/AML2.LAUNCH" ""

echo "failure path checks passed"
