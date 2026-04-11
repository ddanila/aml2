#!/bin/bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
WATCOM_ROOT="${WATCOM_ROOT:-$REPO_ROOT/vendor/openwatcom-v2/current-build-2026-04-11}"
LOCK_FILE="$REPO_ROOT/.build.lock"

case "$(uname -s):$(uname -m)" in
    Linux:x86_64)
        WATCOM_BIN="$WATCOM_ROOT/binl64"
        ;;
    Linux:i?86)
        WATCOM_BIN="$WATCOM_ROOT/binl"
        ;;
    *)
        echo "Unsupported host: $(uname -s) $(uname -m)" >&2
        echo "Currently supported: Linux x86_64, Linux x86" >&2
        exit 1
        ;;
esac

if [ ! -x "$WATCOM_BIN/wcc" ]; then
    echo "Open Watcom toolchain not found at $WATCOM_BIN" >&2
    echo "Run ./tools/setup_openwatcom.sh first." >&2
    exit 1
fi

export WATCOM="$WATCOM_ROOT"
export EDPATH="$WATCOM_ROOT/eddat"
export INCLUDE="$WATCOM_ROOT/h"
export PATH="$WATCOM_BIN:$PATH"

cd "$REPO_ROOT"

if [[ -n "${AML_BUILD_VERSION:-}" ]]; then
    BUILD_VERSION="$AML_BUILD_VERSION"
else
    BUILD_VERSION="$(git tag --list 'v*' --sort=-version:refname | head -n 1 | sed 's/^v//')"
    if [[ -z "$BUILD_VERSION" ]]; then
        BUILD_VERSION="local"
    fi
fi

exec 9>"$LOCK_FILE"
if ! flock -w 60 9; then
    echo "Timed out waiting for build lock: $LOCK_FILE" >&2
    exit 1
fi

echo "Building aml2 with $WATCOM_BIN"
make clean
export AML_BUILD_VERSION="$BUILD_VERSION"
make ${BUILD_TARGETS:-all} EXTRA_CFLAGS="${EXTRA_CFLAGS:-}"
