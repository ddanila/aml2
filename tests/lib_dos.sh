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

aml_test_reset_boot_img() {
    local boot_img="$1"

    aml_test_ensure_out_dir
    cp "$AML_TEST_BASE_IMG" "$boot_img"
}

aml_test_copy_to_image() {
    local boot_img="$1"
    local host_path="$2"
    local image_path="$3"

    mcopy -o -i "$boot_img" "$host_path" "$image_path"
}

aml_test_copy_optional_to_image() {
    local boot_img="$1"
    local host_path="$2"
    local image_path="$3"

    if [[ -n "$host_path" ]]; then
        aml_test_copy_to_image "$boot_img" "$host_path" "$image_path"
    fi
}

aml_test_install_autoexec() {
    local boot_img="$1"
    local autoexec_path="$2"
    local command_text="$3"

    aml_test_write_autoexec "$autoexec_path" "$command_text"
    aml_test_copy_to_image "$boot_img" "$autoexec_path" ::AUTOEXEC.BAT
}

aml_test_run_screen_case() {
    local boot_img="$1"
    local qmp_sock="$2"
    local screen_log="$3"
    local qemu_log="$4"
    local qemu_timeout="$5"
    local expect_timeout="$6"
    shift 6

    rm -f "$qmp_sock" "$screen_log" "$qemu_log"
    aml_test_start_qemu "$boot_img" "$qmp_sock" "$qemu_log" "$qemu_timeout"
    aml_test_wait_for_qmp "$qmp_sock"

    SCREEN_EXPECT_TIMEOUT="$expect_timeout" python3 "$REPO_ROOT/tests/screen_expect.py" \
        "$qmp_sock" "$screen_log" \
        "$@"

    aml_test_stop_qemu
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

aml_test_build() {
    local targets="${1:-all}"
    local cflags="${2:--DAML_TEST_HOOKS=1}"

    if [[ "${AML_TEST_SHARED_BUILD:-0}" == "1" ]]; then
        return
    fi

    BUILD_TARGETS="$targets" EXTRA_CFLAGS="$cflags" "$REPO_ROOT/tools/build.sh"
}
