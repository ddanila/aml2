## Pass Scope

- Drop the `aml_` prefix from file-local `static` helpers.
- Rename shared internal UI helpers from `aml_ui_*` to `ui_*`.
- Keep public headers and exported API unchanged.

## Constraints

- No product behavior changes.
- No public API rename in this pass.
- Keep the rename mechanical and module-scoped.

## Verification

- `./tools/build.sh`
- `bash tests/run_all.sh`
