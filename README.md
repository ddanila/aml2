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

## Launch Model

`aml2` is moving to a two-binary model:

- `AML2.EXE`: launcher UI and config parsing
- `AMLSTUB`: outer supervisor loop

The launcher itself does not directly run the game anymore. Instead it writes a tiny handoff file and exits. The stub then launches the requested command, waits for it to finish, and relaunches the launcher.

This gives us:

- no leftover `GO.BAT`
- no full launcher resident while a game runs
- a cleaner path toward a tiny `.COM` supervisor

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

## E2E Test

The repo has a real-DOS end-to-end test under QEMU:

```bash
bash tests/test_aml2_e2e.sh
```

It boots a minimal DOS floppy, starts `AMLSTUB.EXE`, runs `AML2.EXE`, launches a fake DOS game, returns to the launcher, then exits back to the DOS prompt.

This test does not use DOSBox.

It uses:

- a real DOS floppy image from the `msdos` releases
- `qemu-system-i386`
- QMP text-mode screen capture
- a DOS-side automation file `AML2.AUT`
- a DOS-side trace file `AML2.TRC`

The automation file exists because QEMU key injection was too flaky for a reliable CI-style launcher test. Runtime validation is still done under real DOS.

See [docs/e2e-findings.md](/home/ddanila/fun/aml2/docs/e2e-findings.md) for the bring-up notes and failure modes that were discovered.

## Next Steps

1. Tighten the DOS UI and reduce redraw overhead.
2. Finish config parsing and validation in `cfg.c`.
3. Add and validate the stub loop.
4. Decide how small the supervisor needs to be before rewriting it in asm.
