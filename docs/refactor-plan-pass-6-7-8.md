# Combined Pass 6, 7, And 8 Refactor Plan

## Goal

Improve maintainability in launcher control flow and launch-path logic by:

- splitting `main.c` action handling into smaller helpers
- clarifying `launch.c` validation and execution paths
- tightening public and internal header boundaries

without changing:

- launcher behavior
- exit codes
- UI text
- launch protocol or DOS runtime flow

## Scope

1. Refactor `main.c` argument parsing and action dispatch into smaller helpers.
2. Refactor `launch.c` around explicit validation and child-execution helpers.
3. Give public status/action enums explicit types in public headers.
4. Reduce internal-header coupling where only `AmlState` or internal helpers are required.
5. Run the sequential regression suite and push the result.

## Constraints

- no user-visible behavior changes
- no config syntax changes
- no `AML2.RUN` protocol changes
- keep fixed-size, plain-C DOS implementation style

## Verification

- `bash tests/host/test_cfg_parser.sh`
- `bash tests/dos/test_aml2_tui_navigation.sh`
- `bash tests/dos/test_aml2_search_and_messages.sh`
- `bash tests/dos/test_aml2_hotkeys_and_help.sh`
- `bash tests/dos/test_aml2_edit_ops.sh`
- `bash tests/dos/test_aml2_failure_paths.sh`
- `bash tests/dos/test_aml2_e2e.sh`
