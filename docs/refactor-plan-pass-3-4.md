# Combined Pass 3 And 4 Refactor Plan

## Goal

Reduce maintenance risk in the launcher UI code by:

- extracting shared state and guard helpers
- splitting the large UI implementation by responsibility

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
- `bash tests/host/test_cfg_parser.sh`
- `bash tests/dos/test_aml2_hotkeys_and_help.sh`
- `bash tests/dos/test_aml2_edit_ops.sh`
- `bash tests/dos/test_aml2_failure_paths.sh`

Run sequentially through the aggregate runners when you want the full suite.
