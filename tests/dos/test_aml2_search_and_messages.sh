#!/bin/bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/../lib/dos.sh"

aml_test_init_paths "aml2_search_and_messages"

OUT_DIR="$AML_TEST_OUT_DIR"
BASE_IMG="$AML_TEST_BASE_IMG"
BOOT_IMG="$OUT_DIR/aml2-search.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
QMP_SOCK="$OUT_DIR/aml2-search-qmp.sock"
SCREEN_LOG="$OUT_DIR/aml2-search-screen.log"
QEMU_LOG="$OUT_DIR/aml2-search-qemu.log"

cleanup() {
    aml_test_cleanup_qemu
    aml_test_cleanup_files "$QMP_SOCK" "$AUTOEXEC"
}

run_case() {
    local name="$1"
    local cfg="$2"
    local auto="$3"
    shift 3

    echo "=== $name ==="
    aml_test_reset_boot_img "$BOOT_IMG"
    aml_test_copy_to_image "$BOOT_IMG" "$REPO_ROOT/amlui.exe" ::AMLUI.EXE
    aml_test_copy_to_image "$BOOT_IMG" "$cfg" ::LAUNCHER.CFG
    aml_test_copy_optional_to_image "$BOOT_IMG" "$auto" ::AML2.AUT
    aml_test_install_autoexec "$BOOT_IMG" "$AUTOEXEC" "AMLUI.EXE /V"

    aml_test_run_screen_case "$BOOT_IMG" "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG" 20 8 \
        "$@"
}

trap cleanup EXIT

echo "Building aml2 for search/message tests ..."
aml_test_build all
aml_test_download_base_img

run_case \
    "search navigation" \
    "$REPO_ROOT/tests/data/cfg/launcher.long.cfg" \
    "$REPO_ROOT/tests/data/auto/AML2.SEH" \
    "ENTRY 19" \
    "" \
    "A>" \
    ""
grep -q '\[BIG\] ENTRY 19' "$SCREEN_LOG"

run_case \
    "empty config message" \
    "$REPO_ROOT/tests/data/cfg/launcher.empty.cfg" \
    "" \
    "Launcher config is empty" \
    "ret" \
    "No entries available." \
    "f10" \
    "A>" \
    ""
grep -q 'Run AMLUI /E to add entries.' "$SCREEN_LOG"

echo "search/message tests passed"
