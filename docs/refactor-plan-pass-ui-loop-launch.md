## UI Loop And Launch Refactor Plan

Scope:
- split the remaining `ui.c` loop and automation code into narrower modules
- tighten UI internal header boundaries so render internals and action internals are not mixed together
- simplify `launch.c` around shared validation and child-run context handling

Constraints:
- preserve all UI behavior, key handling, and test-hook behavior
- preserve all launch status codes and existing caller-visible behavior
- keep the current full regression suite as the verification baseline

Execution:
1. Move UI automation handling into its own compilation unit.
2. Move the UI run loop and search prompt into a dedicated loop unit.
3. Move edit/overlay declarations out of `ui_int.h` into a narrower internal header.
4. Refactor `launch.c` around shared entry validation and directory context helpers.
5. Run the full shared regression suite.
