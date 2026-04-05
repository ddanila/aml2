#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

run_test() {
    local script="$1"

    echo "=== $(basename "$script") ==="
    bash "$REPO_ROOT/$script"
}

run_test tests/test_cfg_parser.sh
run_test tests/test_launch.sh
run_test tests/test_aml2_tui_navigation.sh
run_test tests/test_aml2_search_and_messages.sh
run_test tests/test_aml2_hotkeys_and_help.sh
run_test tests/test_aml2_edit_ops.sh
run_test tests/test_aml2_failure_paths.sh
run_test tests/test_aml2_e2e.sh
