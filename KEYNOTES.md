# KEYNOTES

Internal notes for ongoing work on `aml2`.

## Direction

- plain C
- Open Watcom
- small DOS launcher
- simple custom TUI, not a framework
- `AMLSTUB.COM` remains the supervisor entrypoint

## Practical Constraints

- keep runtime behavior simple before chasing more size
- avoid dynamic allocation in the main launcher path
- treat `.COM` pressure as a stub concern first, not a launcher concern
- viewer mode is the default; editor mode is explicit with `/E`

## Toolchain

- pinned Open Watcom snapshot: `Current-build` from `2026-04-03`
- vendor root: `vendor/openwatcom-v2/current-build-2026-04-03`
- current tested stub size: `787 bytes`
- current launcher size is roughly `23 KB`

## Testing Notes

- QEMU + real DOS is the authoritative launcher test path
- `kvikdos` is useful only for fast non-TUI smoke checks
- DOS-side automation files are more reliable than QMP key injection
- keep 8.3 naming in mind in the floppy harness

Current real-DOS coverage includes:

- launcher/stub/game end-to-end loop
- long-list navigation
- search and empty-config messaging
- hotkeys and help dialog
- viewer-mode blocking and editor-mode save persistence
- failure cases for missing launcher and invalid working directory

Detailed bring-up findings live in `docs/e2e-findings.md`.

## Release Notes

- release screenshots should come from a release zip, not a local tree
- after tagging, attach a fresh 1x screenshot to the draft release
- keep the release screenshot and README screenshot in sync
- keep release descriptions short and concrete
- screenshot capture helper: `./tools/capture_release_help_screenshot.sh <tag>`

## Future Ideas

- optional per-game statistics, especially launch count and accumulated play time
