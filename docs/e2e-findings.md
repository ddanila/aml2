# E2E Findings

## Result

`tests/test_aml2_e2e.sh` now passes against real DOS under QEMU.

Validated sequence:

1. `AML.COM` starts from `AUTOEXEC.BAT`
2. `AMLEDIT.EXE` renders the launcher
3. launcher writes `AML2.RUN`
4. stub reads the run request
5. stub launches the fake DOS game
6. fake game returns
7. launcher starts again
8. launcher exits
9. DOS prompt returns

## Why QEMU And Not DOSBox

The goal is a real-DOS execution path, so the test reuses the same style as the `beta_kappa` harness:

- boot a DOS floppy image
- use `qemu-system-i386`
- inspect text-mode video memory through QMP

## Important Findings

### 1. QMP key injection was not reliable enough

Trying to drive the launcher with `send-key` events was flaky. The launcher screen rendered reliably, but the synthetic keypresses did not consistently advance selection or launch the program.

Resolution:

- keep QEMU for real-DOS execution
- move automation into a DOS-side script file instead of relying on injected keys

For quick non-TUI checks, `kvikdos` is still useful, but not as a replacement for the QEMU launcher test.
The repo now also carries a small `kvikdos` smoke test for `fakegame.exe`, but that path only checks fast process start and exit, not the full launcher loop.

### 2. DOS 8.3 naming matters

`AML2.AUTO` was a bad test filename because it did not survive onto the floppy image as-is.

Resolution:

- use `AML2.AUT`

### 3. The first automation parser implementation was wrong

The initial parser returned the last non-empty line from the automation file instead of the first one.

Symptom:

- launcher immediately quit instead of launching the fake game

Resolution:

- keep the first non-empty line as the active command
- rewrite the remainder back to disk

### 4. Multi-step automation was truncating too early

After the first parser fix, longer automation scripts still broke because only one remaining line was preserved after each read.

Symptom:

- tests with more than two scripted actions stalled before save, delete, or quit

Resolution:

- rewrite the full remaining tail of the automation file, not just the next line

### 5. Screen assertions alone were too brittle

The fake game and second launcher run can be transient under automation.

Resolution:

- keep screen assertions for major visible states
- add a DOS-side trace file `AML2.TRC` for deterministic sequencing checks

### 6. The COM supervisor needed explicit memory shrink

The first `AML.COM` attempt failed to start `AMLEDIT.EXE`.

Cause:

- DOS gave the `.COM` stub essentially all available memory

Resolution:

- move the stub to a small internal stack
- resize its memory block before `EXEC`

## Current Test Mechanism

Files used by the test:

- `AML2.AUT`
  - DOS-side automation commands for the launcher
- `AML2.TRC`
  - DOS-side trace of launcher, stub, and fake-game milestones
- `AML2.RUN`
  - normal launcher-to-stub handoff file

Additional failure-path coverage now exists in `tests/test_aml2_failure_paths.sh`:

- missing `AMLEDIT.EXE`
- invalid launch working directory

## Tradeoff

The test hooks are now gated behind `AML_TEST_HOOKS` and only enabled in the test build.

This is acceptable for now because:

- the runtime path stays simple
- the real-DOS e2e test is stable
- the normal build no longer carries the automation path
