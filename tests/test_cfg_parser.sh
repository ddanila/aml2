#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$REPO_ROOT/out"
TEST_INCLUDE_DIR="$OUT_DIR/test-include"
BASIC_CFG="$OUT_DIR/cfg-basic.cfg"
LIMIT_CFG="$OUT_DIR/cfg-limit.cfg"
MISSING_CFG="$OUT_DIR/cfg-missing.cfg"
SAVED_CFG="$OUT_DIR/cfg-saved.cfg"
DRIVER_BIN="$OUT_DIR/cfg_parser_driver"

mkdir -p "$OUT_DIR"
mkdir -p "$TEST_INCLUDE_DIR"

cat > "$TEST_INCLUDE_DIR/aml_build.h" <<'EOF'
#ifndef AML_BUILD_H
#define AML_BUILD_H

#define AML_BUILD_VERSION "test"

#endif
EOF

cat > "$BASIC_CFG" <<'EOF'
# comment

  Alpha  |  ALPHA.EXE  |  C:\GAMES  
Broken only
|MISSINGNAME.EXE|
Missing command||
Beta|BETA.EXE|
Gamma|GAMMA.EXE|  D:\ARCADE  
EOF

: > "$LIMIT_CFG"
for i in $(seq 0 69); do
    printf 'Entry %02d|E%02d.EXE|\n' "$i" "$i" >> "$LIMIT_CFG"
done

rm -f "$MISSING_CFG" "$SAVED_CFG" "$DRIVER_BIN"

cc -I"$TEST_INCLUDE_DIR" \
   -I"$REPO_ROOT/include" \
   "$REPO_ROOT/tests/cfg_parser_driver.c" \
   "$REPO_ROOT/src/cfg.c" \
   -o "$DRIVER_BIN"

"$DRIVER_BIN" "$BASIC_CFG" "$LIMIT_CFG" "$MISSING_CFG" "$SAVED_CFG"
