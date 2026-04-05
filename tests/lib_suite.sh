#!/bin/bash

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

aml_suite_run_test() {
    local script="$1"

    echo "=== $(basename "$script") ==="
    bash "$REPO_ROOT/$script"
}

aml_suite_prepare_build_header() {
    mkdir -p "$REPO_ROOT/include"
    printf '#ifndef AML_BUILD_H\n#define AML_BUILD_H\n\n#define AML_BUILD_VERSION "%s"\n\n#endif\n' "${AML_BUILD_VERSION:-local}" > "$REPO_ROOT/include/aml_build.h"
}

aml_suite_shared_build() {
    echo "=== shared test build ==="
    AML_TEST_SHARED_BUILD=0 BUILD_TARGETS=test-build EXTRA_CFLAGS="-DAML_TEST_HOOKS=1" "$REPO_ROOT/tools/build.sh"
    export AML_TEST_SHARED_BUILD=1
}
