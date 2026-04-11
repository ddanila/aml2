#!/bin/bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
TAG="Current-build"
PIN_DATE="2026-04-11"
VENDOR_BASE="$REPO_ROOT/vendor/openwatcom-v2"
TARGET_DIR="$VENDOR_BASE/current-build-$PIN_DATE"
TMP_DIR="$(mktemp -d)"
ARCHIVE="$TMP_DIR/ow-snapshot.tar.xz"

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

if [ -x "$TARGET_DIR/binl64/wcc" ]; then
    echo "Open Watcom already present at $TARGET_DIR"
    exit 0
fi

mkdir -p "$VENDOR_BASE"

echo "Downloading Open Watcom snapshot: tag=$TAG date=$PIN_DATE"
gh release download "$TAG" \
    -R open-watcom/open-watcom-v2 \
    -p 'ow-snapshot.tar.xz' \
    -O "$ARCHIVE"

rm -rf "$TARGET_DIR"
mkdir -p "$TARGET_DIR"

echo "Extracting toolchain to $TARGET_DIR"
tar -xJf "$ARCHIVE" -C "$TARGET_DIR"

if [ ! -x "$TARGET_DIR/binl64/wcc" ]; then
    echo "Open Watcom extraction failed: missing $TARGET_DIR/binl64/wcc" >&2
    exit 1
fi

echo "Open Watcom ready at $TARGET_DIR"
