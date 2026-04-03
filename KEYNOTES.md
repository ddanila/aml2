# KEYNOTES

## Current Direction

`aml2` is intentionally not a Turbo Vision port.

The current design targets:

- plain C
- Open Watcom
- tiny DOS launcher
- custom TUI with only the primitives we actually need

## Why Plain C

The launcher logic is simple and the binary-size target is strict.

Plain C gives us:

- less runtime baggage
- simpler memory layout
- easier control over static buffers
- fewer surprises when pushing toward a `.COM`-style footprint

## Why No `GO.BAT`

The original project used a batch-file handoff to save memory.

For `aml2`, the current preferred model is a stub-managed relaunch loop:

- no generated batch file left on disk
- no extra shell artifact to clean up
- the full launcher does not stay resident while the game runs

## Current Launch Mechanism

The intended flow is:

1. read the selected entry from `launcher.cfg`
2. write `AML2.RUN`
3. exit
4. let the supervisor run the command
5. let the supervisor restart the launcher after the command exits

This keeps `AML2` focused on UI and config management.

## Config Parsing Rules

The launcher now accepts only meaningful entries:

- whitespace around fields is trimmed
- blank lines and comment lines are ignored
- entries without a non-empty name and command are skipped
- path remains optional

The future supervisor can still use `COMMAND.COM /C` internally to preserve free-form command strings.

## Current TUI

The launcher is now past the bare-minimum screen:

- visible scrolling window for long program lists
- stronger selected-row marker
- `Up/Down`, `Home/End`, `PgUp/PgDn` movement
- footer context for item number, command, and working directory

## `.COM` Reality Check

We should not assume `.COM` is guaranteed.

The right sequence is:

1. get the launcher working
2. measure the actual binary size and memory use
3. remove obvious waste
4. decide whether `.COM` remains worth the extra constraints

If a very small `.EXE` is materially better to maintain, that may still be the correct engineering choice.

## QEMU E2E Notes

The current e2e test is against real DOS in QEMU, not DOSBox.

Important findings from bring-up:

- QMP key injection was not reliable enough for deterministic launcher navigation.
- DOS 8.3 filename constraints matter in the test harness.
- The launcher/stub/game loop is now proven with a DOS-side trace file.
- Transient full-screen text UIs are easier to verify with a mix of screen capture and explicit DOS-side trace points than with screen scraping alone.
- The tested supervisor is now a real `AMLSTUB.COM`.
- The current tested supervisor size is 787 bytes.
- `AML2.EXE` is now about 13 KB with the larger TUI slice.
- `kvikdos` is useful for fast non-TUI smoke checks, but QEMU is still the right tool for the full launcher loop.
- The repo now also has explicit DOS failure-path checks for missing launcher and bad working directory handling.
- The repo now also has a real-DOS navigation test for long launcher lists.
