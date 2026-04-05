## Pass Scope

- Remove the temporary `aml_*` compatibility aliases from public headers.
- Keep the module-prefixed exported names as the only supported API surface.
- Verify the repository no longer depends on the removed aliases.

## Constraints

- No product behavior changes.
- Do not rename shared types or `AML_*` constants in this pass.

## Verification

- `./tools/build.sh`
- `bash tests/run_all.sh`
