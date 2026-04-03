# aml2

`aml2` is a fresh rewrite of the Arvutimuuseum launcher for DOS.

The target is a very small binary built with Open Watcom, using plain C and a tiny custom text UI instead of Turbo Vision.

## Scope

The first milestone is intentionally narrow:

- read `launcher.cfg`
- show a simple launcher list
- support arrow keys and direct hotkeys
- write a tiny launch request and exit
- let a tiny outer stub launch the selected program and bring the launcher back later

The first version does not include an in-app config editor, windowing system, or generic UI framework.

Current TUI features:

- scrolling list window for configs larger than one screen
- stronger selected-row marker and richer footer context
- direct hotkeys `0-9` and `a-z`
- `Up/Down`, `Home/End`, and `PgUp/PgDn` navigation
- footer with item position, command preview, and working directory preview

Current config parsing rules:

- lines starting with `#` are comments
- blank lines are ignored
- fields are `name|command|path`
- surrounding whitespace is trimmed
- entries with an empty name or command are ignored

## Launch Model

`aml2` is moving to a two-binary model:

- `AML2.EXE`: launcher UI and config parsing
- `AMLSTUB.COM`: outer supervisor loop

The launcher itself does not directly run the game anymore. Instead it writes a tiny handoff file and exits. The stub then launches the requested command, waits for it to finish, and relaunches the launcher.

This gives us:

- no leftover `GO.BAT`
- no full launcher resident while a game runs
- a cleaner path toward a tiny `.COM` supervisor

Current tested supervisor build is an Open Watcom `wasm`-built `AMLSTUB.COM`.

See [docs/stub-design.md](/home/ddanila/fun/aml2/docs/stub-design.md) for the protocol details.

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
```

The repo starts with a small GNU `make` file and a plain-C module split:

- `main.c`
- `cfg.c`
- `ui.c`
- `launch.c`

Current release-sized outputs from `./tools/build.sh` are approximately:

- `aml2.exe`: 13 KB
- `amlstub.com`: 787 bytes

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

Main controls in the launcher:

- `Up/Down`: move by one entry
- `PgUp/PgDn`: move by one visible page
- `Home/End`: jump to first or last entry
- `0-9`, `a-z`: launch the corresponding hotkey entry directly
- `Enter`: launch current selection
- `Esc`: exit to DOS

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

This boots real DOS in QEMU, runs `AML2.EXE` directly, jumps to the end of a 20-entry config through DOS-side automation, and verifies that the scrolling UI shows the final item and position footer correctly before exiting to `A>`.

See [docs/e2e-findings.md](/home/ddanila/fun/aml2/docs/e2e-findings.md) for the bring-up notes and failure modes that were discovered.

## Next Steps

1. Add list-search or first-letter jump so large menus stay fast to navigate.
2. Handle more than 36 direct-launch hotkeys cleanly in the UI.
3. Improve error and empty-state screens so launcher problems are clearer on DOS hardware.
4. Decide whether a second screen for per-entry details is worth the size cost.
