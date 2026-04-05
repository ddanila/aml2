#!/bin/bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/lib_suite.sh"

aml_suite_prepare_build_header
aml_suite_run_test tests/test_cfg_parser.sh
aml_suite_run_test tests/test_launch.sh
