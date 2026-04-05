#!/bin/bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/lib_dos.sh"

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
    cp "$BASE_IMG" "$BOOT_IMG"
    mcopy -o -i "$BOOT_IMG" "$REPO_ROOT/amlui.exe" ::AMLUI.EXE
    mcopy -o -i "$BOOT_IMG" "$cfg" ::LAUNCHER.CFG
    if [[ -n "$auto" ]]; then
        mcopy -o -i "$BOOT_IMG" "$auto" ::AML2.AUT
    fi
    aml_test_write_autoexec "$AUTOEXEC" "AMLUI.EXE /V"
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

echo "Building aml2 for search/message tests ..."
BUILD_TARGETS=all EXTRA_CFLAGS="-DAML_TEST_HOOKS=1" "$REPO_ROOT/tools/build.sh"
aml_test_download_base_img

run_case \
    "search navigation" \
    "$REPO_ROOT/tests/launcher.long.cfg" \
    "$REPO_ROOT/tests/AML2.SEH" \
    "Entry 19" \
    "" \
    "A>" \
    ""
grep -q 'Entry 19' "$SCREEN_LOG"

run_case \
    "empty config message" \
    "$REPO_ROOT/tests/launcher.empty.cfg" \
    "" \
    "Launcher config is empty" \
    "ret" \
    "No entries available." \
    "f10" \
    "A>" \
    ""
grep -q 'Run AMLUI /E to add entries.' "$SCREEN_LOG"

echo "search/message tests passed"
