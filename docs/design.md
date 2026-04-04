# aml2 Design Notes

These are early planning notes from the first implementation pass.

They are kept as historical design context:

- useful for the original constraints and intent
- not authoritative for the current shipped feature set
- some milestone-1 cuts listed here were later relaxed

## Goals

- Build a minimal in-house TUI.
- Build with Open Watcom.
- Favor plain C over C++ to keep the runtime and binary size small.
- Aim for a tiny DOS launcher, ideally small enough that a `.COM` target remains realistic.

## Product Contract

The launcher keeps only the essential behavior from the original project:

- Load entries from `launcher.cfg`.
- Parse lines as `name|command|path`.
- Ignore blank lines and `#` comments.
- Display entries in a single list.
- Move selection with arrow keys.
- Launch with `Enter`.
- Support direct hotkeys for quick selection.
- Write a launch request and exit.

## Explicit Non-Goals For Milestone 1

- rich config editor with advanced dialogs or multi-window UI
- Turbo Vision compatibility
- overlapping windows or menus
- about/changelog dialogs
- dynamic plugin architecture

## Technical Strategy

Prefer fixed-size data structures over heap allocation:

- static entry array
- fixed-width buffers for `name`, `command`, and `path`
- bounded parsing with predictable limits

This keeps memory layout simple and makes `.COM` viability easier to judge.

## Proposed Limits

- `AML_MAX_PROGRAMS`: 64
- `AML_MAX_NAME`: 48
- `AML_MAX_COMMAND`: 128
- `AML_MAX_PATH`: 64
- `AML_MAX_LINE`: 255

These are placeholders and should be tuned against real museum launcher data.

## Module Split

### `src/main.c`

- startup
- config load
- event loop
- dispatch selected action

### `src/cfg.c`

- config file parsing
- line trimming
- bounds checks

### `src/ui.c`

- clear screen
- draw frame
- draw list
- key input
- selection updates

### `src/launch.c`

- render `AML2.RUN`
- store command and working directory
- keep the handoff format tiny and deterministic

## Build Strategy

Start with a normal DOS executable under Open Watcom while keeping the code `.COM`-friendly:

- plain C only
- avoid stdio-heavy abstractions where practical
- no dynamic allocation in the main path
- no large generic helpers

After the launcher behavior is stable, test whether the same codebase can be linked into a `.COM` target without unacceptable compromises.

## First Implementation Target

The first usable version only needs:

- config load
- empty-state screen
- list rendering
- keyboard navigation
- handoff-file generation

If size pressure becomes severe, split editor or admin functionality into a separate utility rather than bloating the launcher binary.
