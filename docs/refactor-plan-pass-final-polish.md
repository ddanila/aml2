## Final Code Polish Plan

Scope:
- improve readability in `main.c` and `cfg.c`
- do a last internal consistency cleanup in `launch.c`
- tidy the `Makefile` structure and object grouping

Constraints:
- preserve exit codes, parser behavior, and launch behavior
- keep this pass code-only
- verify with the full build and regression suite

Execution:
1. Extract a few intent-level helpers from `main.c`.
2. Break `cfg.c` load/save flow into smaller helpers.
3. Simplify `launch.c` child-run flow around one directory-context runner helper.
4. Reorganize `Makefile` object groups for readability.
5. Run `./tools/build.sh` and `bash tests/run_all.sh`.
