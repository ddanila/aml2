# aml2

`aml2` is a small DOS launcher built in plain C for Open Watcom.

It uses a tiny custom text UI, a supervisor stub (`AMLSTUB.COM`), and a simple `LAUNCHER.CFG` format.

License: MIT. See [LICENSE](LICENSE).

## Screenshot

![aml2 screenshot](assets/aml2-screenshot.png)

To regenerate a release screenshot:

```bash
./tools/capture_release_help_screenshot.sh <tag>
```

## What It Does

- shows entries from `LAUNCHER.CFG`
- launches the selected item through `AMLSTUB.COM`
- supports viewer mode by default
- supports editor mode with `AML2.EXE /E`
- writes `AML2.RUN` as the handoff file between launcher and stub

Current output sizes from `./tools/build.sh` are approximately:

- `aml2.exe`: 23 KB
- `amlstub.com`: 787 bytes

## Quick Start

Build:

```bash
./tools/setup_openwatcom.sh
./tools/build.sh
```

Normal DOS-side entrypoint:

```text
AMLSTUB.COM
```

Do not start with `AML2.EXE` in normal use. The stub is what relaunches the launcher after a game exits.

Expected DOS-side files:

- `AMLSTUB.COM`
- `AML2.EXE`
- `LAUNCHER.CFG`
- the executables or batch files referenced by `LAUNCHER.CFG`

## Command Line

`AML2.EXE` supports:

- `/E` to enable editor mode
- `/?` to print usage and exit

Viewer mode is the default. Mutation actions are only available in editor mode.

## Config Format

`LAUNCHER.CFG` format:

```text
Name|Command|Working Directory
```

Rules:

- lines starting with `#` are comments
- blank lines are ignored
- surrounding whitespace is trimmed
- `name` and `command` are required
- `path` is optional

## Controls

- `Up/Down`: move by one entry
- `PgUp/PgDn`: move by one visible page
- `Home/End`: jump to first or last entry
- `/`: search within entry names
- `Enter`: launch current selection
- `0-9`, `a-z`, `A-Z`: direct launch hotkeys for the first 62 items
- `F1` or `?`: help
- `F3`: details
- `F10`: exit to DOS

Editor mode only:

- `F2`: save `LAUNCHER.CFG`
- `F4`: edit current entry
- `F5` / `F6`: move current entry up or down
- `Ins`: insert a new entry
- `F8`: delete current entry

## Packaging

```bash
./tools/package_dist.sh
```

This creates:

- `out/dist/aml2-local.zip` locally
- `out/dist/aml2-<short-hash>.zip` in CI

The zip contains:

- `README.md`
- `LAUNCHER.CFG`
- `AML2.EXE`
- `AMLSTUB.COM`

## Manual QEMU Run

```bash
./tools/run_qemu_manual.sh
```

To copy extra DOS files onto the floppy image before boot:

```bash
./tools/run_qemu_manual.sh path/to/GAME.EXE path/to/EXTRA.BAT
```

## Tests

Main checks:

```bash
bash tests/test_aml2_e2e.sh
bash tests/test_aml2_tui_navigation.sh
bash tests/test_aml2_search_and_messages.sh
bash tests/test_aml2_hotkeys_and_help.sh
bash tests/test_aml2_edit_ops.sh
bash tests/test_aml2_failure_paths.sh
bash tests/test_kvikdos_smoke.sh
```

QEMU is the authoritative path for the launcher UI and stub loop. `kvikdos` is only used for fast non-TUI smoke checks.

See [docs/stub-design.md](docs/stub-design.md), [docs/e2e-findings.md](docs/e2e-findings.md), [docs/design.md](docs/design.md), and [docs/toolchain.md](docs/toolchain.md) for implementation details.
