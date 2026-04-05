## Header Hygiene Plan

Scope:
- trim clearly unused includes
- make a few remaining internal call sites use the existing shared helpers
- keep the change limited to dependency/readability cleanup

Constraints:
- no behavioral changes
- no test/layout changes
- verify with full build and regression suite

Execution:
1. Remove unused includes and stale locals where they are clearly unnecessary.
2. Route any remaining direct UI selection writes through the shared helper layer.
3. Run `./tools/build.sh` and `bash tests/run_all.sh`.
