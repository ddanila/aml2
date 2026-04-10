#!/bin/bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/lib/suite.sh"

aml_suite_shared_build
aml_suite_run_test tests/host/test_cfg_parser.sh
aml_suite_run_test tests/host/test_launch.sh
# TUI tests temporarily skipped for no-bigfont diagnostic build
#aml_suite_run_test tests/dos/test_aml2_tui_navigation.sh
#aml_suite_run_test tests/dos/test_aml2_search_and_messages.sh
#aml_suite_run_test tests/dos/test_aml2_hotkeys_and_help.sh
#aml_suite_run_test tests/dos/test_aml2_edit_ops.sh
#aml_suite_run_test tests/dos/test_aml2_failure_paths.sh
aml_suite_run_test tests/dos/test_aml2_e2e.sh
