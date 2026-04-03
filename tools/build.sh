#!/bin/bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
WATCOM_ROOT="${WATCOM_ROOT:-$REPO_ROOT/vendor/openwatcom-v2/current-build-2026-04-03}"

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

if [[ -n "${AML_BUILD_TAG:-}" ]]; then
    BUILD_TAG="$AML_BUILD_TAG"
elif [[ -n "${CI:-}" ]]; then
    BUILD_TAG="$(git rev-parse --short HEAD)"
else
    BUILD_TAG="local"
fi

echo "Building aml2 with $WATCOM_BIN"
make clean
mkdir -p build
cat > build/aml_build.h <<EOF
#ifndef AML_BUILD_H
#define AML_BUILD_H

#define AML_BUILD_TAG "$BUILD_TAG"

#endif
EOF
make ${BUILD_TARGETS:-all} EXTRA_CFLAGS="${EXTRA_CFLAGS:-}"
