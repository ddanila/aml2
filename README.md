# aml2

`aml2` is a fresh rewrite of the Arvutimuuseum launcher for DOS.

The target is a very small binary built with Open Watcom, using plain C and a tiny custom text UI.

License: MIT. See [LICENSE](LICENSE).

## Screenshot

![aml2 screenshot](assets/aml2-screenshot.png)

The screenshot can be regenerated from a release zip with:

```bash
./tools/capture_release_help_screenshot.sh v0.1.3
```

There is also a manual GitHub Actions workflow, `Capture Release Screenshot`, for generating the same artifacts in CI.

Release checklist:

1. Push the release tag.
2. Wait for the draft release zip to be published by CI.
3. Run `./tools/capture_release_help_screenshot.sh <tag>` or the manual screenshot workflow.
4. Upload the 1x PNG to the draft release.
5. Embed that PNG in the draft release description.

## Scope

The first milestone is intentionally narrow:

- read `LAUNCHER.CFG`
- show a simple launcher list
- support direct hotkeys, lightweight editing, and save
- write a tiny launch request and exit
- let a tiny outer stub launch the selected program and bring the launcher back later

The current version still does not try to be a windowing system or generic UI framework.

Current TUI features:

- direct text-mode VRAM renderer with a full-screen backbuffer
- scrolling list window for configs larger than one screen
- stronger selected-row marker, boxed sections, and a scrollbar-style gutter
- direct hotkeys `0-9`, `a-z`, and `A-Z`
- `Up/Down`, `Home/End`, and `PgUp/PgDn` navigation
- `/` substring search against entry names
- `F3` details dialog for the current entry
- `F4` edit current entry and `Ins` insert a new one
- `F5` / `F6` move the current entry up or down
- `F8` delete the current entry after confirmation
- `F2` save the current in-memory config back to `LAUNCHER.CFG`
- `?` / `F1` help dialog
- clearer full-screen message panels for missing or unusable config
- same list used for both launching and editing

Current config parsing rules:

- lines starting with `#` are comments
- blank lines are ignored
- fields are `name|command|path`
- surrounding whitespace is trimmed
- entries with an empty name or command are ignored

## Launch Model

`aml2` uses a two-binary model:

- `AML2.EXE`: launcher UI and config parsing
- `AMLSTUB.COM`: outer supervisor loop

The launcher itself does not directly run the game anymore. Instead it writes a tiny handoff file and exits. The stub then launches the requested command, waits for it to finish, and relaunches the launcher.

This gives us:

- no leftover `GO.BAT`
- no full launcher resident while a game runs
- a cleaner path toward a tiny `.COM` supervisor

Current tested supervisor build is an Open Watcom `wasm`-built `AMLSTUB.COM`.

See [docs/stub-design.md](docs/stub-design.md) for the protocol details.

## Repository Layout

- `docs/`
  - design notes and constraints
- `include/`
  - shared headers
- `src/`
  - launcher implementation

## Build Direction

The intended toolchain is Open Watcom targeting 16-bit DOS.

The repo uses a vendored Open Watcom snapshot from the `open-watcom-v2` GitHub releases instead of relying on a host install.

Typical flow:

```bash
./tools/setup_openwatcom.sh
./tools/build.sh
./tools/package_dist.sh
```

The repo starts with a small GNU `make` file and a plain-C module split:

- `main.c`
- `cfg.c`
- `ui.c`
- `launch.c`

Current release-sized outputs from `./tools/build.sh` are approximately:

- `aml2.exe`: 22 KB
- `amlstub.com`: 787 bytes

Packaging output from `./tools/package_dist.sh`:

- `out/dist/aml2-local.zip` for local builds
- `out/dist/aml2-<short-hash>.zip` in CI builds

The zip contains:

- `README.md`
- `LAUNCHER.CFG`
- `AML2.EXE`
- `AMLSTUB.COM`

## Usage

In normal use, start `AMLSTUB.COM`, not `AML2.EXE`.

- `AMLSTUB.COM` is the user-facing entrypoint
- it starts `AML2.EXE`
- `AML2.EXE` shows the menu and writes `AML2.RUN`
- `AMLSTUB.COM` launches the selected game and returns to the launcher after the game exits

Run `AML2.EXE` directly only if you want to inspect the launcher UI by itself. In that mode it can still write `AML2.RUN`, but there is no supervisor process around to consume it and bring the launcher back.

Expected DOS-side layout:

- `AMLSTUB.COM`
- `AML2.EXE`
- `LAUNCHER.CFG`
- the game executables or batch commands referenced by `LAUNCHER.CFG`

Manual real-DOS run in QEMU:

```bash
./tools/run_qemu_manual.sh
```

To copy extra DOS files such as games or helper batch files onto the boot floppy before starting QEMU:

```bash
./tools/run_qemu_manual.sh path/to/GAME.EXE path/to/EXTRA.BAT
```

Main controls in the launcher:

- `Up/Down`: move by one entry
- `PgUp/PgDn`: move by one visible page
- `Home/End`: jump to first or last entry
- `/`: search within entry names
- `F1` or `?`: open the help dialog
- `F2`: save all changes to `LAUNCHER.CFG`
- `F3`: show details for the current entry
- `F4`: edit the current entry
- `F5`: move the current entry up
- `F6`: move the current entry down
- `Ins`: insert a new entry after the current one
- `F8`: delete the current entry after confirmation
- `F10`: exit to DOS
- `0-9`, `a-z`, `A-Z`: launch the corresponding hotkey entry directly for the first 62 items
- `Enter`: launch current selection

## E2E Test

The repo has a real-DOS end-to-end test under QEMU:

```bash
bash tests/test_aml2_e2e.sh
```

It boots a minimal DOS floppy, starts `AMLSTUB.COM`, runs `AML2.EXE`, launches a fake DOS game, returns to the launcher, then exits back to the DOS prompt.

This test does not use DOSBox.

It uses:

- a real DOS floppy image from the `msdos` releases
- `qemu-system-i386`
- QMP text-mode screen capture
- a DOS-side automation file `AML2.AUT`
- a DOS-side trace file `AML2.TRC`

The automation file exists because QEMU key injection was too flaky for a reliable CI-style launcher test. Runtime validation is still done under real DOS.

For faster non-TUI smoke checks, `kvikdos` is also a useful option, but QEMU remains the authoritative path for the launcher loop because the launcher is a full-screen TUI.

Fast smoke test:

```bash
bash tests/test_kvikdos_smoke.sh
```

This one runs `fakegame.exe` under `kvikdos` with a timeout and checks that the DOS payload starts, prints, and exits quickly.

Additional DOS failure coverage:

```bash
bash tests/test_aml2_failure_paths.sh
```

This verifies that the supervisor exits cleanly for a missing launcher and for an invalid working directory.

Long-list TUI navigation coverage:

```bash
bash tests/test_aml2_tui_navigation.sh
```

This boots real DOS in QEMU, runs `AML2.EXE` directly, jumps to the end of a 20-entry config through DOS-side automation, and verifies that the scrolling UI shows the final item and position indicator correctly before exiting to `A>`.

Search and message-screen coverage:

```bash
bash tests/test_aml2_search_and_messages.sh
```

This verifies in-name search on a long list and the full-screen empty-config message path under real DOS in QEMU.

Extended hotkey and help coverage:

```bash
bash tests/test_aml2_hotkeys_and_help.sh
```

This verifies the uppercase hotkey range for entries past `z` and the in-app help dialog under real DOS in QEMU.

Edit-operation coverage:

```bash
bash tests/test_aml2_edit_ops.sh
```

This verifies reorder, delete, and save persistence under real DOS in QEMU by reading back the updated `LAUNCHER.CFG` from the floppy image.

See [docs/e2e-findings.md](docs/e2e-findings.md) for the bring-up notes and failure modes that were discovered.

## Next Steps

1. Decide whether very large configs need another navigation aid beyond scroll and search.
2. Revisit `AML2.EXE` size now that the renderer and editor slice are in place.
3. Capture and attach a fresh screenshot for each release tag so the draft page stays in sync with the actual build.
