# First-Pass Refactor Plan

## Goal

Make a small set of readability and maintainability improvements without changing:

- DOS runtime behavior
- `AML2.RUN` handoff format
- `AML.COM` supervisor flow
- fixed-size memory model
- current user-visible UI text and controls

## Constraints

- keep the launcher in plain C
- avoid dynamic allocation in the launcher path
- preserve the existing build and test workflow
- prefer local helper extraction over architectural rewrites

## Planned Changes

1. Refactor duplicated notice and launch-result handling in `src/main.c`.
2. Refactor shared launch validation logic in `src/launch.c`.
3. Remove dead UI code and tidy clearly safe internal helpers in `src/ui.c`.
4. Make `tests/test_kvikdos_smoke.sh` configurable and able to skip cleanly when `kvikdos` is unavailable.

## Explicit Non-Goals

- no changes to `stub/amlstub.asm`
- no changes to the launcher/stub protocol
- no parser format changes
- no large UI redesign or module split in this pass

## Verification

Run these after the refactor, and also after individual batches when relevant:

- `./tools/build.sh`
- `bash tests/test_aml2_failure_paths.sh`
- `bash tests/test_aml2_hotkeys_and_help.sh`
- `bash tests/test_aml2_edit_ops.sh`
- `bash tests/test_kvikdos_smoke.sh`

For `kvikdos`, a clean skip is acceptable when the local dependency is unavailable.
