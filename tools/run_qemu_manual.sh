#!/bin/bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
OUT_DIR="$REPO_ROOT/out"
BASE_IMG="${MSDOS_BASE_IMG:-$OUT_DIR/floppy-minimal.img}"
BOOT_IMG="$OUT_DIR/aml2-manual.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
DOS_RELEASE_TAG="${DOS_RELEASE_TAG:-0.1}"
DOS_IMAGE_URL="${DOS_IMAGE_URL:-https://github.com/ddanila/msdos/releases/download/${DOS_RELEASE_TAG}/floppy-minimal.img}"

need_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required tool: $1" >&2
        exit 1
    fi
}

download_base_img() {
    mkdir -p "$OUT_DIR"
    if [[ -f "$BASE_IMG" ]]; then
        return
    fi

    python3 - "$DOS_IMAGE_URL" "$BASE_IMG" <<'PY'
import shutil
import sys
import urllib.request

url, dest = sys.argv[1], sys.argv[2]
with urllib.request.urlopen(url) as response, open(dest, "wb") as out:
    shutil.copyfileobj(response, out)
PY
}

copy_into_image() {
    local src="$1"
    local dst

    if [[ ! -f "$src" ]]; then
        echo "File not found: $src" >&2
        exit 1
    fi

    dst="$(basename "$src")"
    mcopy -o -i "$BOOT_IMG" "$src" "::${dst}"
}

need_tool python3
need_tool mcopy
need_tool qemu-system-i386

cd "$REPO_ROOT"

./tools/build.sh
download_base_img

cp "$BASE_IMG" "$BOOT_IMG"
copy_into_image "$REPO_ROOT/aml2.exe"
copy_into_image "$REPO_ROOT/amlstub.com"
copy_into_image "$REPO_ROOT/launcher.cfg"

for extra in "$@"; do
    copy_into_image "$extra"
done

printf '@ECHO OFF\r\nAMLSTUB.COM\r\n' > "$AUTOEXEC"
mcopy -o -i "$BOOT_IMG" "$AUTOEXEC" ::AUTOEXEC.BAT

echo "Booting $BOOT_IMG"
echo "Entry point: AMLSTUB.COM"
if [[ $# -gt 0 ]]; then
    echo "Extra files copied: $*"
fi

exec qemu-system-i386 \
    -drive if=floppy,index=0,format=raw,file="$BOOT_IMG" \
    -boot a \
    -m 4
