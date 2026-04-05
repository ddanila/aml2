#!/bin/bash

set -euo pipefail

AML_TEST_QEMU_PID=""

aml_test_init_paths() {
    local test_name="$1"

    REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    AML_TEST_OUT_ROOT="$REPO_ROOT/out"
    AML_TEST_OUT_DIR="$AML_TEST_OUT_ROOT/$test_name"
    AML_TEST_BASE_IMG="${MSDOS_BASE_IMG:-$AML_TEST_OUT_ROOT/floppy-minimal.img}"
    AML_TEST_DOS_RELEASE_TAG="${DOS_RELEASE_TAG:-0.1}"
    AML_TEST_DOS_IMAGE_URL="${DOS_IMAGE_URL:-https://github.com/ddanila/msdos/releases/download/${AML_TEST_DOS_RELEASE_TAG}/floppy-minimal.img}"

    mkdir -p "$AML_TEST_OUT_ROOT" "$AML_TEST_OUT_DIR"
}

aml_test_ensure_out_dir() {
    mkdir -p "$AML_TEST_OUT_ROOT" "$AML_TEST_OUT_DIR"
}

aml_test_cleanup_qemu() {
    if [[ -n "${AML_TEST_QEMU_PID:-}" ]]; then
        kill "$AML_TEST_QEMU_PID" 2>/dev/null || true
        wait "$AML_TEST_QEMU_PID" 2>/dev/null || true
        AML_TEST_QEMU_PID=""
    fi
}

aml_test_cleanup_files() {
    rm -f "$@"
}

aml_test_download_base_img() {
    aml_test_ensure_out_dir
    if [[ -f "$AML_TEST_BASE_IMG" ]]; then
        return
    fi

    python3 - "$AML_TEST_DOS_IMAGE_URL" "$AML_TEST_BASE_IMG" <<'PY'
import shutil
import sys
import urllib.request

url, dest = sys.argv[1], sys.argv[2]
with urllib.request.urlopen(url) as response, open(dest, "wb") as out:
    shutil.copyfileobj(response, out)
PY
}

aml_test_write_autoexec() {
    local autoexec_path="$1"
    local command_text="$2"

    aml_test_ensure_out_dir
    printf '@ECHO OFF\r\n%s\r\n' "$command_text" > "$autoexec_path"
}

aml_test_start_qemu() {
    local boot_img="$1"
    local qmp_sock="$2"
    local qemu_log="$3"
    local timeout_secs="${4:-20}"

    aml_test_ensure_out_dir
    rm -f "$qmp_sock" "$qemu_log"
    timeout "$timeout_secs" qemu-system-i386 \
        -display none \
        -monitor none \
        -serial none \
        -drive if=floppy,index=0,format=raw,file="$boot_img" \
        -boot a \
        -m 4 \
        -qmp unix:"$qmp_sock",server,nowait \
        >"$qemu_log" 2>&1 &
    AML_TEST_QEMU_PID=$!
}

aml_test_wait_for_qmp() {
    local qmp_sock="$1"

    for _ in $(seq 1 50); do
        [[ -S "$qmp_sock" ]] && return 0
        sleep 0.2
    done

    echo "ERROR: QMP socket did not appear: $qmp_sock" >&2
    return 1
}

aml_test_stop_qemu() {
    aml_test_cleanup_qemu
}
