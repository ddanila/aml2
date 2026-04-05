# Combined Pass 3 And 4 Refactor Plan

## Goal

Reduce maintenance risk in the launcher UI code by:

- extracting shared state and guard helpers
- splitting the large `src/ui.c` module by responsibility

without changing:

- UI behavior
- keyboard controls
- `AML_TEST_HOOKS` semantics
- DOS rendering and launch flow

## Scope

1. Add small shared helpers for launcher state setup and common UI selection/editor guards.
2. Split UI code into:
   - core rendering and shared helpers
   - UI flow, overlays, editing, and event loop
   - test-hook file automation helpers
3. Keep the public `include/ui.h` API stable.
4. Update the build and run existing regression tests sequentially.

## Constraints

- no user-visible text changes
- no protocol changes
- no config syntax changes
- no test parallelism changes in this pass

## Verification

- `./tools/build.sh`
- `bash tests/test_cfg_parser.sh`
- `bash tests/test_aml2_hotkeys_and_help.sh`
- `bash tests/test_aml2_edit_ops.sh`
- `bash tests/test_aml2_failure_paths.sh`

Run sequentially because the current test scripts share `out/`.
