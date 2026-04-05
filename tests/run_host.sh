#!/bin/bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/lib/suite.sh"

aml_suite_prepare_build_header
aml_suite_run_test tests/host/test_cfg_parser.sh
aml_suite_run_test tests/host/test_launch.sh
