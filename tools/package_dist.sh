#!/bin/bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
DIST_DIR="$REPO_ROOT/out/dist"
STAGE_DIR="$DIST_DIR/stage"

if [[ -n "${AML_BUILD_TAG:-}" ]]; then
    BUILD_TAG="$AML_BUILD_TAG"
elif [[ -n "${CI:-}" ]]; then
    BUILD_TAG="$(git -C "$REPO_ROOT" rev-parse --short HEAD)"
else
    BUILD_TAG="local"
fi

cd "$REPO_ROOT"

mkdir -p "$STAGE_DIR"
rm -rf "$STAGE_DIR"/*

cp README.md "$STAGE_DIR/README.md"
cp launcher.cfg "$STAGE_DIR/launcher.example.cfg"
cp aml2.exe "$STAGE_DIR/AML2.EXE"
cp amlstub.com "$STAGE_DIR/AMLSTUB.COM"

mkdir -p "$DIST_DIR"
ZIP_PATH="$DIST_DIR/aml2-${BUILD_TAG}.zip"
rm -f "$ZIP_PATH"

(
    cd "$STAGE_DIR"
    zip -q -r "$ZIP_PATH" .
)

echo "Packaged $ZIP_PATH"
