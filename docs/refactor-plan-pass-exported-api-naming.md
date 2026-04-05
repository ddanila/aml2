## Pass Scope

- Rename exported functions to module-prefixed names:
  - `cfg_*`
  - `ui_*`
  - `launch_*`
- Keep temporary compatibility aliases for the old `aml_*` names in public headers.
- Update all in-repo definitions and call sites to the new canonical names.

## Constraints

- No product behavior changes.
- Keep shared types and `AML_*` constants unchanged in this pass.
- Preserve downstream compatibility for one transition pass via header aliases.

## Verification

- `./tools/build.sh`
- `bash tests/run_all.sh`
