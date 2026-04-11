# Input Lag Investigation

## Problem

Keyboard input lag on slow machines (8088/286-class): pressing an arrow key
twice quickly shows a noticeable delay (~500ms+) before the second keypress
is visually reflected. Single keypresses after idle are instant. Page-up/down
(full list redraw) is instant. The lag exists on multiple slow machines.

## Timeline

- **v0.1.8**: Fast on all hardware.
- **v0.1.9**: Fast (only added debug menu and docs).
- **v0.2.0**: First slow version. Introduced bigtext (VGA custom font, 2 rows per entry).
- **v0.2.0 .. v0.3.9**: All slow despite numerous fixes.

## What Was Already Eliminated

| Hypothesis | Test | Result |
|-----------|------|--------|
| VSYNC wait in flush | Removed all VSYNC | Still slow |
| `delay(50)` in input polling | Replaced with busy-wait | Still slow |
| `ui_hide_cursor()` per draw | Removed (was 6 ticks/330ms on slow BIOS) | Helped on second machine, not on first |
| `_dos_gettime()` in render | Removed clock entirely | Still slow |
| 8-dot clock / pixel clock | Disabled | Still slow |
| Custom font upload (INT 10h) | Disabled font loading | Still slow |
| Partial redraw optimization | Restored (only ~300 cells vs 6200) | Still slow |
| Key coalescing | Added | Still slow |
| 12KB static font arrays | Moved to heap (v0.4.0) | Still slow |
| Loop structure | Reverted to v0.1.8 always-redraw style | Still slow |

## Key Observation

v0.1.9 (fast) → v0.2.0 (slow). The diff includes:
1. Many refactoring commits (code split into modules)
2. The bigfont commit (6033187) adding `ui_bigtext.c`

Both the refactoring and bigfont happened between these versions.

## Diagnostic Approach: kvikdos Instrumentation

Use `vendor/kvikdos` (improvements branch, software 8086 CPU backend on macOS)
to run the launcher under instrumented emulation.

### Test Scenario

1. Build AMLUI.EXE with current code
2. Create a LAUNCHER.CFG with 10 entries
3. Run under kvikdos with instruction counting / cycle profiling
4. Automate: open UI, wait for settle, send two DOWN keypresses
5. Measure instruction count between:
   - First keypress injected → first screen update visible in VRAM
   - Second keypress injected → second screen update visible in VRAM
6. Compare with v0.1.8 binary under same conditions

### What kvikdos Can Tell Us

- Exact instruction count per phase (no 55ms tick granularity)
- Which INT calls are made and how many CPU cycles each takes
- Memory access patterns (which addresses are hot)
- Whether the bottleneck is CPU instructions, INT handler overhead,
  or something else entirely

### Setup

```bash
# Build kvikdos (macOS, software CPU)
cd vendor/kvikdos
make kvikdos

# Build aml2
cd ../..
# (need Linux build for the .EXE — use CI artifact or cross-compile)

# Run under kvikdos
./vendor/kvikdos/kvikdos amlui.exe /V
```

### Comparison Builds

Two binaries to compare:
1. v0.1.8 AMLUI.EXE (known fast)
2. Current AMLUI.EXE (known slow)

Run both under identical kvikdos instrumentation. The instruction count
difference between "first keypress response" and "second keypress response"
will pinpoint the exact code path causing the lag.
