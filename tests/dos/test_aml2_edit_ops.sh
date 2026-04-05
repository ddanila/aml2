#!/bin/bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/../lib/dos.sh"

aml_test_init_paths "aml2_edit_ops"

OUT_DIR="$AML_TEST_OUT_DIR"
BASE_IMG="$AML_TEST_BASE_IMG"
BOOT_IMG="$OUT_DIR/aml2-edit.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
QMP_SOCK="$OUT_DIR/aml2-edit-qmp.sock"
SCREEN_LOG="$OUT_DIR/aml2-edit-screen.log"
QEMU_LOG="$OUT_DIR/aml2-edit-qemu.log"
CFG_LOG="$OUT_DIR/aml2-edit-launcher.cfg"

cleanup() {
    aml_test_cleanup_qemu
    aml_test_cleanup_files "$QMP_SOCK" "$AUTOEXEC"
}

run_case() {
    local name="$1"
    local launch_cmd="$2"
    local auto="$3"

    echo "=== $name ==="
    aml_test_reset_boot_img "$BOOT_IMG"
    aml_test_copy_to_image "$BOOT_IMG" "$REPO_ROOT/amlui.exe" ::AMLUI.EXE
    aml_test_copy_to_image "$BOOT_IMG" "$REPO_ROOT/tests/data/cfg/launcher.edit.cfg" ::LAUNCHER.CFG
    aml_test_copy_to_image "$BOOT_IMG" "$auto" ::AML2.AUT
    aml_test_install_autoexec "$BOOT_IMG" "$AUTOEXEC" "$launch_cmd"

    rm -f "$CFG_LOG"
    aml_test_run_screen_case "$BOOT_IMG" "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG" 20 8 \
        'A>' ''

    aml_test_extract_normalized_text_file "$BOOT_IMG" ::LAUNCHER.CFG "$CFG_LOG"
}

trap cleanup EXIT

echo "Building aml2 for edit-operation tests ..."
aml_test_build all
aml_test_download_base_img

run_case "viewer mode blocks save" "AMLUI.EXE /V" "$REPO_ROOT/tests/data/auto/AML2.VWR"
grep -q '^Alpha|ALPHA.EXE|$' "$CFG_LOG"
grep -q '^Beta|BETA.EXE|$' "$CFG_LOG"
grep -q '^Gamma|GAMMA.EXE|$' "$CFG_LOG"
python3 - "$CFG_LOG" <<'PY'
import sys

lines = [
    line.strip()
    for line in open(sys.argv[1], encoding="ascii")
    if "|" in line and not line.lstrip().startswith("#")
]
expected = [
    "Alpha|ALPHA.EXE|",
    "Beta|BETA.EXE|",
    "Gamma|GAMMA.EXE|",
]
if lines != expected:
    raise SystemExit(f"unexpected viewer-mode result: {lines!r}")
PY

run_case "reorder then save" "AMLUI.EXE /E" "$REPO_ROOT/tests/data/auto/AML2.EDR"
grep -q '^Alpha|ALPHA.EXE|$' "$CFG_LOG"
grep -q '^Gamma|GAMMA.EXE|$' "$CFG_LOG"
grep -q '^Beta|BETA.EXE|$' "$CFG_LOG"
python3 - "$CFG_LOG" <<'PY'
import sys

lines = [
    line.strip()
    for line in open(sys.argv[1], encoding="ascii")
    if "|" in line and not line.lstrip().startswith("#")
]
expected = [
    "Alpha|ALPHA.EXE|",
    "Gamma|GAMMA.EXE|",
    "Beta|BETA.EXE|",
]
if lines != expected:
    raise SystemExit(f"unexpected reorder result: {lines!r}")
PY

run_case "delete then save" "AMLUI.EXE /E" "$REPO_ROOT/tests/data/auto/AML2.EDD"
grep -q '^Alpha|ALPHA.EXE|$' "$CFG_LOG"
grep -q '^Gamma|GAMMA.EXE|$' "$CFG_LOG"
if grep -q '^Beta|BETA.EXE|$' "$CFG_LOG"; then
    echo "deleted entry still present"
    exit 1
fi
python3 - "$CFG_LOG" <<'PY'
import sys

lines = [
    line.strip()
    for line in open(sys.argv[1], encoding="ascii")
    if "|" in line and not line.lstrip().startswith("#")
]
expected = [
    "Alpha|ALPHA.EXE|",
    "Gamma|GAMMA.EXE|",
]
if lines != expected:
    raise SystemExit(f"unexpected delete result: {lines!r}")
PY

echo "edit-operation tests passed"
