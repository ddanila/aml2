#!/bin/bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
TAG="${1:-v0.1.1}"
OUT_DIR="$REPO_ROOT/out/release-$TAG-games"
STAGE_DIR="$OUT_DIR/stage"
BASE_IMG="${MSDOS_BASE_IMG:-$REPO_ROOT/out/floppy-minimal.img}"
BOOT_IMG="$OUT_DIR/aml2-$TAG-games-help.img"
AUTOEXEC="$OUT_DIR/AUTOEXEC.BAT"
QMP_SOCK="$OUT_DIR/help-qmp.sock"
SCREEN_LOG="$OUT_DIR/help-screen.log"
QEMU_LOG="$OUT_DIR/help-qemu.log"
CFG_PATH="$OUT_DIR/LAUNCHER.CFG"
PPM_OUT="$OUT_DIR/aml2-$TAG-games-f1-help.ppm"
PNG_OUT="$OUT_DIR/aml2-$TAG-games-f1-help.png"
PNG_2X_OUT="$OUT_DIR/aml2-$TAG-games-f1-help-2x.png"
GAMES_CSV="${AML_MANUAL_GAMES_CSV:-$HOME/fun/games_list/games_1987_1993.csv}"
if [[ ! -f "$GAMES_CSV" ]]; then
    GAMES_CSV="$REPO_ROOT/assets/games_sample.csv"
fi
DOS_RELEASE_TAG="${DOS_RELEASE_TAG:-0.1}"
DOS_IMAGE_URL="${DOS_IMAGE_URL:-https://github.com/ddanila/msdos/releases/download/${DOS_RELEASE_TAG}/floppy-minimal.img}"
QEMU_PID=""

need_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required tool: $1" >&2
        exit 1
    fi
}

cleanup() {
    if [[ -n "$QEMU_PID" ]]; then
        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
    fi
    rm -f "$QMP_SOCK" "$AUTOEXEC"
}

download_base_img() {
    mkdir -p "$(dirname "$BASE_IMG")"
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

generate_manual_config() {
    if [[ ! -f "$GAMES_CSV" ]]; then
        echo "Games CSV not found: $GAMES_CSV" >&2
        exit 1
    fi

    python3 - "$GAMES_CSV" "$CFG_PATH" <<'PY'
import csv
import random
import re
import sys
from pathlib import Path

src = Path(sys.argv[1])
dst = Path(sys.argv[2])

random.seed(1)

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
    f.write("# aml2 release screenshot config\n")
    for name, command, path in sample:
        f.write(f"{name}|{command}|{path}\n")
PY
}

need_tool gh
need_tool unzip
need_tool mcopy
need_tool qemu-system-i386
need_tool python3

trap cleanup EXIT

mkdir -p "$OUT_DIR" "$STAGE_DIR"
rm -rf "$STAGE_DIR"/*
rm -f "$OUT_DIR"/aml2-*.zip

gh release download "$TAG" -R ddanila/aml2 -p 'aml2-*.zip' -D "$OUT_DIR"
unzip -oq "$OUT_DIR"/aml2-*.zip -d "$STAGE_DIR"
download_base_img
generate_manual_config

cp "$BASE_IMG" "$BOOT_IMG"
mcopy -o -i "$BOOT_IMG" "$STAGE_DIR/AMLUI.EXE" ::AMLUI.EXE
mcopy -o -i "$BOOT_IMG" "$STAGE_DIR/AML.COM" ::AML.COM
mcopy -o -i "$BOOT_IMG" "$CFG_PATH" ::LAUNCHER.CFG

printf '@ECHO OFF\r\nAML.COM\r\n' > "$AUTOEXEC"
mcopy -o -i "$BOOT_IMG" "$AUTOEXEC" ::AUTOEXEC.BAT

rm -f "$QMP_SOCK" "$SCREEN_LOG" "$QEMU_LOG" "$PPM_OUT" "$PNG_OUT" "$PNG_2X_OUT"

timeout 20 qemu-system-i386 \
    -display none \
    -monitor none \
    -serial none \
    -drive if=floppy,index=0,format=raw,file="$BOOT_IMG" \
    -boot a \
    -m 4 \
    -qmp unix:"$QMP_SOCK",server,nowait \
    >"$QEMU_LOG" 2>&1 &
QEMU_PID=$!

for _ in $(seq 1 50); do
    [[ -S "$QMP_SOCK" ]] && break
    sleep 0.2
done

SCREEN_EXPECT_TIMEOUT="${SCREEN_EXPECT_TIMEOUT:-10}" python3 "$REPO_ROOT/tests/lib/screen_expect.py" \
    "$QMP_SOCK" "$SCREEN_LOG" \
    'Arvutimuuseum Launcher' 'f1' \
    'Launcher Help' ''

python3 - "$REPO_ROOT/tests/lib/screen_expect.py" "$QMP_SOCK" "$PPM_OUT" "$PNG_OUT" "$PNG_2X_OUT" <<'PY'
import importlib.util
import sys
from PIL import Image

screen_expect_path, sock_path, ppm_path, png_path, png_2x_path = sys.argv[1:6]

spec = importlib.util.spec_from_file_location(
    "screen_expect",
    screen_expect_path,
)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

qmp = mod.QMPConnection(sock_path)
qmp.human_cmd(f"screendump {ppm_path}")
qmp.close()

img = Image.open(ppm_path)
img.save(png_path)
img.resize((img.width * 2, img.height * 2), Image.Resampling.NEAREST).save(
    png_2x_path,
    optimize=True,
)
PY

echo "Generated:"
echo "  $CFG_PATH"
echo "  $PPM_OUT"
echo "  $PNG_OUT"
echo "  $PNG_2X_OUT"
