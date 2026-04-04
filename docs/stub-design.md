# Stub Design

## Goal

Use a tiny supervisor process to keep the launcher out of conventional memory while a game is running.

The architecture is:

- `AML.COM`: outer loop and child launcher
- `AMLUI.EXE`: UI and config parser

## Runtime Flow

1. `AML.COM` starts.
2. `AML.COM` runs `AMLUI.EXE` and waits.
3. `AMLUI.EXE` lets the user choose a program.
4. `AMLUI.EXE` writes a launch request to `AML2.RUN` and exits.
5. `AML.COM` reads `AML2.RUN`.
6. `AML.COM` launches the requested command.
7. When the child exits, `AML.COM` starts `AMLUI.EXE` again.
8. If no launch request exists, `AML.COM` exits to DOS.

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

The control signal is not the file alone:

- `AMLUI.EXE` exits with `0` on normal exit
- `AMLUI.EXE` exits with `2` only when a launch request was written
- `AML.COM` reads `AML2.RUN` only after that explicit launch exit code

This avoids replaying a stale handoff file as a fresh launch request.

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

- tested runtime path now uses `AML.COM`
- `AML.COM` is built with Open Watcom `wasm`
- the OMF object is converted to `.COM` with a small repo-local converter
- current tested size is 926 bytes

## Launch Behavior

The supervisor should:

1. launch `AMLUI.EXE`
2. check `AML2.RUN`
3. if missing, exit
4. if present, read command and path
5. switch drive if needed
6. `chdir` if path is present
7. run simple `.EXE` and `.COM` targets directly
8. otherwise fall back to `COMMAND.COM /C <command>`
9. delete `AML2.RUN`
10. repeat

## Current COM Stub Notes

The `.COM` stub must resize its DOS memory block before calling `EXEC`.

Without that step, DOS cannot load `AMLUI.EXE` because the freshly started `.COM` stub owns essentially all available memory.

## Error Handling

For the first version:

- if `AMLUI.EXE` cannot be started, print an error and exit
- if `AML2.RUN` cannot be parsed, print an error, delete it, and exit
- if the target command fails to start, print an error and return to the launcher loop

Keep failure modes visible and simple before attempting size optimization.

Current launch split:

- simple `.EXE` and `.COM` commands without shell metacharacters are started directly
- `.BAT` files and shell-like command lines still go through `COMMAND.COM /C`
- normal launches ignore child errorlevel and return to the launcher immediately
- `F9` debug launches can set a stub debug flag so the stub pauses on child errors
