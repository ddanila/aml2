## Bucket 1 Refactor Plan

Scope:
- tighten the local and CI test workflow around one shared sequential entrypoint
- add direct host-side coverage for `launch.c`
- narrow the remaining `ui.c` surface by moving edit and overlay helpers into a dedicated internal module

Constraints:
- keep launcher behavior unchanged
- keep DOS/QEMU-backed tests sequential
- avoid protocol or stub changes

Execution:
1. Add a shared `tests/run_all.sh` entrypoint and wire CI to use it.
2. Add host-compatible direct tests for `launch.c` covering validation and child-run directory restore.
3. Split edit and overlay helpers from `src/ui.c` into `src/ui_edit.c`, keeping existing internal interfaces stable.
4. Run the sequential regression suite through the shared runner.
