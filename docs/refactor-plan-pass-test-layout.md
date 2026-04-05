## Pass Scope

- Reorganize tests into clearer `host`, `dos`, `lib`, and `data` subtrees.
- Move fixtures into explicit config and automation directories.
- Keep segmented runners and `run_all.sh`, but repoint them at the new layout.
- Expand shared DOS helpers so scripts can rely on common extraction and scenario utilities.
- Add a small host-side `launch.c` coverage increment while preserving behavior.

## Constraints

- No product behavior changes.
- DOS/QEMU suites must remain sequential because they share `out/`.
- Standalone test scripts should keep working outside the aggregate runners.

## Verification

- `bash tests/run_host.sh`
- `bash tests/run_dos_ui.sh`
- `bash tests/run_dos_launch.sh`
- `bash tests/run_all.sh`
