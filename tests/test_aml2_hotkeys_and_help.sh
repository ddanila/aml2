#!/bin/bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/lib_dos.sh"

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
    cp "$BASE_IMG" "$BOOT_IMG"
    mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/amlui.exe" ::AMLUI.EXE
    mcopy -o -i "$BOOT_IMG" "$cfg" ::LAUNCHER.CFG
    if [[ -n "$auto" ]]; then
        mcopy -o -i "$BOOT_IMG" "$auto" ::AML2.AUT
    fi
    aml_test_write_autoexec "$AUTOEXEC" "$launch_cmd"
    mcopy -o -i "$BOOT_IMG" "$AUTOEXEC" ::AUTOEXEC.BAT

    rm -f "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG"
    aml_test_start_qemu "$BOOT_IMG" "$QMP_SOCK" "$QEMU_LOG" 20
    aml_test_wait_for_qmp "$QMP_SOCK"

    SCREEN_EXPECT_TIMEOUT=8 python3 "$REPO_ROOT/tests/screen_expect.py" \
        "$QMP_SOCK" "$SCREEN_LOG" \
        "$@"

    aml_test_stop_qemu
}

trap cleanup EXIT

echo "Building aml2 for hotkey/help tests ..."
aml_test_build all
aml_test_download_base_img

run_case \
    "extended hotkey range" \
    "$REPO_ROOT/tests/launcher.hotkeys.cfg" \
    "AMLUI.EXE /V" \
    "$REPO_ROOT/tests/AML2.HKA" \
    'Entry 36' '' \
    'A>' ''
grep -q 'Entry 36' "$SCREEN_LOG"

run_case \
    "help dialog" \
    "$REPO_ROOT/tests/launcher.e2e.cfg" \
    "AMLUI.EXE /E" \
    "$REPO_ROOT/tests/AML2.HLP" \
    'EDIT' 'Launcher Help'
grep -q 'F8     Delete current entry' "$SCREEN_LOG"
grep -q 'F10    Exit to DOS' "$SCREEN_LOG"

echo "hotkey/help tests passed"
