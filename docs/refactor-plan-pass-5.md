# Pass 5 Test Harness Refactor Plan

## Goal

Reduce coupling between DOS/QEMU test scripts by:

- moving repeated harness logic into shared shell helpers
- giving each test its own output directory under `out/`

without changing:

- test intent
- launcher behavior
- DOS image contents used by each test

## Scope

1. Add a shared shell helper for DOS/QEMU test setup and teardown.
2. Move each DOS/QEMU test to `out/<test-name>/`.
3. Keep the shared downloaded base floppy image at `out/floppy-minimal.img`.
4. Update parser test outputs to use its own subdirectory too.
5. Run the sequential regression suite and push the result.

## Constraints

- no test behavior changes beyond path/layout cleanup
- no parallel test execution changes in this pass
- preserve current screen assertions and fixture usage

## Verification

- `bash tests/test_cfg_parser.sh`
- `bash tests/test_aml2_tui_navigation.sh`
- `bash tests/test_aml2_search_and_messages.sh`
- `bash tests/test_aml2_hotkeys_and_help.sh`
- `bash tests/test_aml2_edit_ops.sh`
- `bash tests/test_aml2_failure_paths.sh`
- `bash tests/test_aml2_e2e.sh`
