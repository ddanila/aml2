## UI State Helper Refactor Plan

Scope:
- tighten the remaining UI internal boundaries
- extract shared selection/state helpers so loop and automation code stop open-coding the same movement rules

Constraints:
- preserve all current key behavior, wrapping rules, and status text
- keep the public UI API unchanged
- verify with the full shared regression suite

Execution:
1. Add a small internal UI state helper module for selection and paging behavior.
2. Update `ui_loop.c`, `ui_auto.c`, and `ui_edit.c` to use those helpers.
3. Keep render internals, action internals, and state internals separated by header.
4. Run `./tools/build.sh` and `bash tests/run_all.sh`.
