#!/bin/bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/lib_dos.sh"

aml_test_init_paths "aml2_tui_navigation"

OUT_DIR="$AML_TEST_OUT_DIR"
BASE_IMG="$AML_TEST_BASE_IMG"
BOOT_IMG="$OUT_DIR/aml2-tui.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
QMP_SOCK="$OUT_DIR/aml2-tui-qmp.sock"
SCREEN_LOG="$OUT_DIR/aml2-tui-screen.log"
QEMU_LOG="$OUT_DIR/aml2-tui-qemu.log"

cleanup() {
    aml_test_cleanup_qemu
    aml_test_cleanup_files "$QMP_SOCK" "$AUTOEXEC"
}

trap cleanup EXIT

echo "Building aml2 for TUI navigation test ..."
aml_test_build all
aml_test_download_base_img

aml_test_reset_boot_img "$BOOT_IMG"
aml_test_copy_to_image "$BOOT_IMG" "$REPO_ROOT/amlui.exe" ::AMLUI.EXE
aml_test_copy_to_image "$BOOT_IMG" "$REPO_ROOT/tests/launcher.long.cfg" ::LAUNCHER.CFG
aml_test_copy_to_image "$BOOT_IMG" "$REPO_ROOT/tests/AML2.END" ::AML2.AUT
aml_test_install_autoexec "$BOOT_IMG" "$AUTOEXEC" "AMLUI.EXE /V"

aml_test_run_screen_case "$BOOT_IMG" "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG" 20 8 \
    'Arvutimuuseum Launcher' '' \
    'Entry 19' '' \
    'A>' ''

grep -q 'Danila Sukharev' "$SCREEN_LOG"
grep -q 'Entry 19' "$SCREEN_LOG"
grep -q '^A>$' "$SCREEN_LOG"

echo "tui navigation test passed"
