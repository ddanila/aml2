#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$REPO_ROOT/out/launch"
TEST_INCLUDE_DIR="$OUT_DIR/test-include"
DRIVER_BIN="$OUT_DIR/launch_driver"
TEST_ROOT="$OUT_DIR/root"

mkdir -p "$OUT_DIR" "$TEST_INCLUDE_DIR"
rm -rf "$TEST_ROOT" "$DRIVER_BIN"

cat > "$TEST_INCLUDE_DIR/aml_build.h" <<'EOF'
#ifndef AML_BUILD_H
#define AML_BUILD_H

#define AML_BUILD_VERSION "test"

#endif
EOF

cat > "$TEST_INCLUDE_DIR/direct.h" <<'EOF'
#ifndef AML_TEST_DIRECT_H
#define AML_TEST_DIRECT_H

#include <unistd.h>

#endif
EOF

cat > "$TEST_INCLUDE_DIR/dos.h" <<'EOF'
#ifndef AML_TEST_DOS_H
#define AML_TEST_DOS_H

void _dos_getdrive(unsigned *drive);
void _dos_setdrive(unsigned drive, unsigned *drives);

#endif
EOF

cat > "$TEST_INCLUDE_DIR/io.h" <<'EOF'
#ifndef AML_TEST_IO_H
#define AML_TEST_IO_H

#include <unistd.h>

#endif
EOF

cat > "$TEST_INCLUDE_DIR/process.h" <<'EOF'
#ifndef AML_TEST_PROCESS_H
#define AML_TEST_PROCESS_H

#define P_WAIT 0

int spawnlp(int mode, const char *path, const char *arg0, ...);

#endif
EOF

cc -include strings.h \
   -Dstricmp=strcasecmp \
   -I"$TEST_INCLUDE_DIR" \
   -I"$REPO_ROOT/include" \
   "$REPO_ROOT/tests/launch_driver.c" \
   "$REPO_ROOT/src/launch.c" \
   -o "$DRIVER_BIN"

"$DRIVER_BIN" "$TEST_ROOT"
