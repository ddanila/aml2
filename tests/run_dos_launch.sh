#!/bin/bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/lib/suite.sh"

aml_suite_shared_build
aml_suite_run_test tests/dos/test_aml2_failure_paths.sh
aml_suite_run_test tests/dos/test_aml2_e2e.sh
