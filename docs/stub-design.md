# Stub Design

## Goal

Use a tiny supervisor process to keep the launcher out of conventional memory while a game is running.

The architecture is:

- `AMLSTUB.COM`: outer loop and child launcher
- `AML2.EXE`: UI and config parser

## Runtime Flow

1. `AMLSTUB` starts.
2. `AMLSTUB` runs `AML2.EXE` and waits.
3. `AML2` lets the user choose a program.
4. `AML2` writes a launch request to `AML2.RUN` and exits.
5. `AMLSTUB` reads `AML2.RUN`.
6. `AMLSTUB` launches the requested command.
7. When the child exits, `AMLSTUB` starts `AML2.EXE` again.
8. If no launch request exists, `AMLSTUB` exits to DOS.

## Handoff File

Filename:

- `AML2.RUN`

Format:

```text
command line
working directory
```

Rules:

- line 1 is required and contains the exact command string
- line 2 is optional and contains the working directory
- empty or missing file means "no launch request"
- malformed files are treated as failure and should be deleted

This protocol is intentionally simple so the assembly stub can parse it with minimal code.

## Why Not Direct Exec In `AML2`

Direct exec from the launcher is still workable, but it forces the launcher binary to own process handoff behavior.

The stub model is better because:

- the launcher can stay a normal Open Watcom `.EXE`
- only a tiny supervisor remains between runs
- the launcher can be reloaded after each game exits
- the `.COM` size pressure moves to the stub where it belongs

## Why Not TSR

TSR or interrupt-hook designs are not the right first step.

They add complexity in:

- memory management
- reentry safety
- shell interaction
- cleanup

The stub loop gives the same user-visible effect with much lower risk.

## Supervisor Status

Current state:

- tested runtime path now uses `AMLSTUB.COM`
- `AMLSTUB.COM` is built with Open Watcom `wasm`
- the OMF object is converted to `.COM` with a small repo-local converter
- current tested size is 878 bytes

## Launch Behavior

The supervisor should:

1. launch `AML2.EXE`
2. check `AML2.RUN`
3. if missing, exit
4. if present, read command and path
5. switch drive if needed
6. `chdir` if path is present
7. run `COMMAND.COM /C <command>`
8. delete `AML2.RUN`
9. repeat

## Current COM Stub Notes

The `.COM` stub must resize its DOS memory block before calling `EXEC`.

Without that step, DOS cannot load `AML2.EXE` because the freshly started `.COM` stub owns essentially all available memory.

## Error Handling

For the first version:

- if `AML2.EXE` cannot be started, print an error and exit
- if `AML2.RUN` cannot be parsed, print an error, delete it, and exit
- if the target command fails to start, print an error and return to the launcher loop

Keep failure modes visible and simple before attempting size optimization.
