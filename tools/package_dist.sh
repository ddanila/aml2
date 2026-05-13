#!/bin/bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
DIST_DIR="$REPO_ROOT/out/dist"
STAGE_DIR="$DIST_DIR/stage"

if [[ -n "${AML_BUILD_TAG:-}" ]]; then
    BUILD_TAG="$AML_BUILD_TAG"
elif tag="$(git -C "$REPO_ROOT" describe --tags --exact-match HEAD 2>/dev/null)"; then
    # DOS-friendly: strip the leading "v" and the dots so the zip filename
    # fits 8.3 (e.g. v0.6.2 -> aml2-062.zip). Keeps the name short enough
    # for FAT12/FAT16 floppies that older targets may unzip onto.
    BUILD_TAG="${tag#v}"
    BUILD_TAG="${BUILD_TAG//./}"
elif [[ -n "${CI:-}" ]]; then
    BUILD_TAG="$(git -C "$REPO_ROOT" rev-parse --short HEAD)"
else
    BUILD_TAG="local"
fi

cd "$REPO_ROOT"

mkdir -p "$STAGE_DIR"
rm -rf "$STAGE_DIR"/*

# README may contain Unicode glyphs (em-dash, +/-, almost-equal).
# Transliterate to CP437 so DOS text editors render it sanely,
# then normalise line endings to CRLF for text files.
iconv -f UTF-8 -t CP437//TRANSLIT README.md | sed 's/$/\r/' > "$STAGE_DIR/README.md"
sed 's/$/\r/' launcher.cfg > "$STAGE_DIR/LAUNCHER.CFG"
cp amlui.exe "$STAGE_DIR/AMLUI.EXE"
cp aml.com "$STAGE_DIR/AML.COM"

mkdir -p "$DIST_DIR"
ZIP_PATH="$DIST_DIR/aml2-${BUILD_TAG}.zip"
rm -f "$ZIP_PATH"

(
    cd "$STAGE_DIR"
    zip -q -r "$ZIP_PATH" .
)

echo "Packaged $ZIP_PATH"
