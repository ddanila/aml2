#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KVIKDOS_DIR="/home/ddanila/fun/msdos/kvikdos"
KVIKDOS_BIN="$KVIKDOS_DIR/kvikdos"
OUT_DIR="$REPO_ROOT/out"
TRACE_LOG="$OUT_DIR/kvikdos-trace.log"
STDOUT_LOG="$OUT_DIR/kvikdos-stdout.log"
USAGE_LOG="$OUT_DIR/kvikdos-usage.log"

mkdir -p "$OUT_DIR"

if [[ ! -d "$KVIKDOS_DIR" ]]; then
    echo "kvikdos source tree not found at $KVIKDOS_DIR" >&2
    exit 1
fi

if ! file "$KVIKDOS_BIN" 2>/dev/null | grep -q 'ELF'; then
    make -C "$KVIKDOS_DIR" kvikdos
fi

rm -f "$REPO_ROOT/AML2.AUT" "$REPO_ROOT/AML2.TRC"
cat > "$REPO_ROOT/AML2.AUT" <<'EOF'
launch 0
quit
EOF

BUILD_TARGETS=test-build EXTRA_CFLAGS="-DAML_TEST_HOOKS=1" "$REPO_ROOT/tools/build.sh"
mkdir -p "$OUT_DIR"

(
    cd "$REPO_ROOT"
    timeout 10 "$KVIKDOS_BIN" fakegame.exe > "$STDOUT_LOG" 2>&1
)

tr -d '\r' < "$STDOUT_LOG" > "$TRACE_LOG"

grep -q "FAKE GAME" "$STDOUT_LOG"
grep -q "Press any key to return" "$TRACE_LOG"

(
    cd "$REPO_ROOT"
    timeout 10 "$KVIKDOS_BIN" amlui.exe > "$USAGE_LOG" 2>&1 || true
)

grep -q "AMLUI usage:" "$USAGE_LOG"
grep -q "AMLUI /V | /E | /?" "$USAGE_LOG"
grep -q "Run AML.COM instead." "$USAGE_LOG"

rm -f "$REPO_ROOT/AML2.AUT" "$REPO_ROOT/AML2.TRC"

echo "kvikdos smoke passed"
