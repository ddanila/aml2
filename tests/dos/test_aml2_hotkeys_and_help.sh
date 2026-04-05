#!/bin/bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/../lib/dos.sh"

aml_test_init_paths "aml2_hotkeys_and_help"

OUT_DIR="$AML_TEST_OUT_DIR"
BASE_IMG="$AML_TEST_BASE_IMG"
BOOT_IMG="$OUT_DIR/aml2-help.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
QMP_SOCK="$OUT_DIR/aml2-help-qmp.sock"
SCREEN_LOG="$OUT_DIR/aml2-help-screen.log"
QEMU_LOG="$OUT_DIR/aml2-help-qemu.log"

cleanup() {
    aml_test_cleanup_qemu
    aml_test_cleanup_files "$QMP_SOCK" "$AUTOEXEC"
}

run_case() {
    local name="$1"
    local cfg="$2"
    local launch_cmd="$3"
    local auto="$4"
    shift 4

    echo "=== $name ==="
    aml_test_reset_boot_img "$BOOT_IMG"
    aml_test_copy_to_image "$BOOT_IMG" "$REPO_ROOT/amlui.exe" ::AMLUI.EXE
    aml_test_copy_to_image "$BOOT_IMG" "$cfg" ::LAUNCHER.CFG
    aml_test_copy_optional_to_image "$BOOT_IMG" "$auto" ::AML2.AUT
    aml_test_install_autoexec "$BOOT_IMG" "$AUTOEXEC" "$launch_cmd"

    aml_test_run_screen_case "$BOOT_IMG" "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG" 20 8 \
        "$@"
}

trap cleanup EXIT

echo "Building aml2 for hotkey/help tests ..."
aml_test_build all
aml_test_download_base_img

run_case \
    "extended hotkey range" \
    "$REPO_ROOT/tests/data/cfg/launcher.hotkeys.cfg" \
    "AMLUI.EXE /V" \
    "$REPO_ROOT/tests/data/auto/AML2.HKA" \
    'Entry 36' '' \
    'A>' ''
grep -q 'Entry 36' "$SCREEN_LOG"

run_case \
    "help dialog" \
    "$REPO_ROOT/tests/data/cfg/launcher.e2e.cfg" \
    "AMLUI.EXE /E" \
    "$REPO_ROOT/tests/data/auto/AML2.HLP" \
    'EDIT' 'Launcher Help'
grep -q 'F8     Delete current entry' "$SCREEN_LOG"
grep -q 'F10    Exit to DOS' "$SCREEN_LOG"

echo "hotkey/help tests passed"
