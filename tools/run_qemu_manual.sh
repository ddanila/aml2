#!/bin/bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
OUT_DIR="$REPO_ROOT/out"
BASE_IMG="${MSDOS_BASE_IMG:-$OUT_DIR/floppy-minimal.img}"
BOOT_IMG="$OUT_DIR/aml2-manual.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
MANUAL_CFG="$OUT_DIR/launcher.manual.cfg"
LAUNCHER_CFG_SOURCE=""
GAMES_CSV="${AML_MANUAL_GAMES_CSV:-$HOME/fun/games_list/games_1987_1993.csv}"
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

generate_manual_config() {
    if [[ ! -f "$GAMES_CSV" ]]; then
        echo "Games CSV not found: $GAMES_CSV" >&2
        exit 1
    fi

    python3 - "$GAMES_CSV" "$MANUAL_CFG" <<'PY'
import csv
import random
import re
import sys
from pathlib import Path

src = Path(sys.argv[1])
dst = Path(sys.argv[2])

random.seed()

def clean_field(text: str, limit: int) -> str:
    text = text.replace("|", "/").replace("\r", " ").replace("\n", " ")
    text = re.sub(r"\s+", " ", text).strip()
    text = text.encode("cp437", "replace").decode("cp437")
    return text[:limit].rstrip() or "Unknown"

rows = []
with src.open(newline="", encoding="utf-8") as f:
    for row in csv.DictReader(f):
        name = clean_field(row.get("name", ""), 48)
        year = clean_field(row.get("year", ""), 4)
        if year and year != "Unknown":
            label = clean_field(f"{name} ({year})", 48)
        else:
            label = name
        cmd_name = clean_field(name, 110)
        rows.append((label, f"ECHO {cmd_name}", ""))

sample = random.sample(rows, min(50, len(rows)))

with dst.open("w", encoding="ascii", newline="") as f:
    f.write("# aml2 manual launcher config\n")
    f.write("# Generated from games_1987_1993.csv\n\n")
    for name, command, path in sample:
        f.write(f"{name}|{command}|{path}\n")
PY
}

copy_launcher_config() {
    local src="$1"

    if [[ ! -f "$src" ]]; then
        echo "Launcher config not found: $src" >&2
        exit 1
    fi

    mcopy -o -i "$BOOT_IMG" "$src" ::LAUNCHER.CFG
}

need_tool python3
need_tool mcopy
need_tool qemu-system-i386

cd "$REPO_ROOT"

if [[ $# -gt 0 && "${1##*.}" == "cfg" ]]; then
    LAUNCHER_CFG_SOURCE="$1"
    shift
fi

./tools/build.sh
download_base_img
if [[ -n "$LAUNCHER_CFG_SOURCE" ]]; then
    :
else
    generate_manual_config
    LAUNCHER_CFG_SOURCE="$MANUAL_CFG"
fi

cp "$BASE_IMG" "$BOOT_IMG"
copy_into_image "$REPO_ROOT/amlui.exe"
copy_into_image "$REPO_ROOT/aml.com"
copy_launcher_config "$LAUNCHER_CFG_SOURCE"

for extra in "$@"; do
    copy_into_image "$extra"
done

printf '@ECHO OFF\r\nAML.COM\r\n' > "$AUTOEXEC"
mcopy -o -i "$BOOT_IMG" "$AUTOEXEC" ::AUTOEXEC.BAT

echo "Booting $BOOT_IMG"
echo "Entry point: AML.COM"
echo "Launcher config: $LAUNCHER_CFG_SOURCE"
if [[ $# -gt 0 ]]; then
    echo "Extra files copied: $*"
fi

exec qemu-system-i386 \
    -drive if=floppy,index=0,format=raw,file="$BOOT_IMG" \
    -boot a \
    -m 4
