## UI Loop, Header, And Launch Polish Plan

Scope:
- tighten low-risk header/include coupling where module boundaries are already clear
- break `ui_loop.c` into smaller intent-level helpers
- further factor `launch.c` into smaller validation and child-exec helpers

Constraints:
- keep all public headers and behavior stable
- preserve current key handling, statuses, and launch result codes
- verify with the full shared regression suite

Execution:
1. Reduce incidental include dependencies in the UI internals where possible.
2. Extract small loop helpers from `ui_loop.c` for idle redraw, search updates, extended keys, and hotkey launch.
3. Extract child-exec and drive/path helpers in `launch.c` to make the top-level flow more direct.
4. Run `./tools/build.sh` and `bash tests/run_all.sh`.
