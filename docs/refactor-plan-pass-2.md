# Second-Pass Refactor Plan

## Goal

Improve the config parser and saver code so it is easier to reason about and better covered by focused tests, without changing:

- the `Name|Command|Working Directory` file format
- comment and blank-line handling
- fixed-size launcher data structures
- existing UI and launcher flow

## Constraints

- keep parser behavior compatible with the current launcher
- avoid heap allocation
- prefer helper extraction and explicit status flow
- add direct tests for parser boundaries instead of relying only on end-to-end coverage

## Planned Changes

1. Refactor `src/cfg.c` into smaller parsing helpers.
2. Make config load/save return codes clearer internally while keeping current callers compatible.
3. Add focused tests for trimming, invalid lines, max-entry handling, and save output.
4. Commit and push the second-pass results after verification.

## Explicit Non-Goals

- no config syntax extensions
- no quoting or escaping support
- no changes to `AML.COM` or launch behavior
- no UI changes in this pass

## Verification

- `./tools/build.sh`
- `bash tests/test_aml2_failure_paths.sh`
- `bash tests/test_aml2_edit_ops.sh`
- new parser-focused test script
