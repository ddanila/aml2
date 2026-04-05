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
BUILD_TARGETS=all EXTRA_CFLAGS="-DAML_TEST_HOOKS=1" "$REPO_ROOT/tools/build.sh"
aml_test_download_base_img

cp "$BASE_IMG" "$BOOT_IMG"
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/amlui.exe" ::AMLUI.EXE
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/tests/launcher.long.cfg" ::LAUNCHER.CFG
mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/tests/AML2.END" ::AML2.AUT
aml_test_write_autoexec "$AUTOEXEC" "AMLUI.EXE /V"
mcopy -o -i "$BOOT_IMG" "$AUTOEXEC" ::AUTOEXEC.BAT

rm -f "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG"
aml_test_start_qemu "$BOOT_IMG" "$QMP_SOCK" "$QEMU_LOG" 20
aml_test_wait_for_qmp "$QMP_SOCK"

SCREEN_EXPECT_TIMEOUT=8 python3 "$REPO_ROOT/tests/screen_expect.py" \
    "$QMP_SOCK" "$SCREEN_LOG" \
    'Arvutimuuseum Launcher' '' \
    'Entry 19' '' \
    'A>' ''

aml_test_stop_qemu

grep -q 'Danila Sukharev' "$SCREEN_LOG"
grep -q 'Entry 19' "$SCREEN_LOG"
grep -q '^A>$' "$SCREEN_LOG"

echo "tui navigation test passed"
