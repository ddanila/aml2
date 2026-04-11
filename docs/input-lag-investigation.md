# Input Lag Investigation

## Problem

Keyboard input lag: pressing an arrow key twice quickly shows a ~660ms
delay before the second keypress is visually reflected. Reproducible in
QEMU and on real hardware. The 1st keypress is always instant (~35ms).

## Root Cause

**Open Watcom linker bug** in the snapshot from 2026-04-03. When the
project is compiled as multiple object files, the linker produces a
binary layout that delays keyboard interrupt delivery by ~660ms. The
daily Watcom build from 2026-04-11 fixes this.

### Evidence

VNC framebuffer update measurement (no vCPU interference):

| Watcom version | Binary size | DOWN #1 | DOWN #2 | Ratio |
|---------------|------------|---------|---------|-------|
| Apr 3, 2026   | 36764 bytes | 69ms   | **664ms** | **9.6x** |
| Apr 11, 2026  | 35790 bytes | 119ms  | **35ms**  | **0.3x** |

Same source code, same compiler flags, same object files.

### Fix

Update the pinned Watcom snapshot from `2026-04-03` to `2026-04-11`.

## Timeline

- **v0.1.8** (Apr 4): Fast on all hardware.
- **v0.1.9** (Apr 4): Fast. Last version before refactoring.
- **482255e** (Apr 5): First slow version. Extracted `ui_edit.c` as a
  separate object file. Identical code, different linker layout.
- **v0.2.0** (Apr 5): Slow. Added bigtext (initially suspected cause).
- **v0.2.0 .. v0.4.2**: All slow despite removing every suspected cause.

## Binary Search Results

| Commit | Size | 2nd DOWN | Status |
|--------|------|----------|--------|
| v0.1.9 (da8fcc2) | 30KB | 34ms | FAST |
| 599bd2b | 30KB | 56ms | FAST |
| a9dc2cd | 30KB | 32ms | FAST |
| **482255e** (ui_edit.c split) | **32KB** | **662ms** | **SLOW** |
| 4c74d3d | 32KB | 1038ms | SLOW |
| v0.2.0 | 35KB | 1043ms | SLOW |

## What Was Eliminated

| Hypothesis | Test | Result |
|-----------|------|--------|
| VSYNC wait in flush | Removed all VSYNC | Still slow |
| `delay(50)` in input polling | Replaced with busy-wait | Still slow |
| `ui_hide_cursor()` per draw | Removed | Still slow |
| `_dos_gettime()` in render | Removed clock entirely | Still slow |
| 8-dot clock / pixel clock | Disabled | Still slow |
| Custom font upload | Disabled font loading | Still slow |
| Partial redraw | Restored | Still slow |
| Key coalescing | Added | Still slow |
| 12KB static font arrays | Shrunk to [1] | Still slow |
| Loop structure | Reverted to v0.1.8 style | Still slow |
| `kbhit()` replacement | Direct BIOS buffer check | Still slow |
| HLT instruction | Added to wait loop | Still slow |
| `rep movsw` blit | Replaced with for-loop | Still slow |
| Unity build | Single compilation unit | Still slow |
| Binary size | Padded v0.1.9 to 35KB | STILL FAST |

## Measurement Infrastructure

### VNC Framebuffer Updates (`tests/perf/repro_vnc_fb.py`)

The most accurate measurement. Uses VNC protocol `FramebufferUpdate`
messages to detect screen changes without pausing the vCPU. QEMU sends
these asynchronously on its display refresh.

Previous approaches (`pmemsave`, QMP `send-key`) were unreliable:
- `pmemsave` pauses the vCPU, causing ~1s measurement artifacts
- QMP `send-key` bypasses the keyboard controller, doesn't reproduce the issue
- VNC key events go through the full 8042 → IRQ 1 → INT 9 path

### kvikdos Instrumentation

`vendor/kvikdos` (improvements branch) provides instruction-level
measurement via its software 8086 CPU backend. Custom port I/O handling
was added for VGA registers used by the launcher.

## Other Fixes Made During Investigation

- **env_seg=0 for child launches**: fixed Turbo Pascal error 201 on DOS 6.22
- **Pixel clock compensation**: fixed monitor sync loss on 8-dot mode
- **ui_hide_cursor removal**: saves ~330ms on slow VGA BIOSes
- **Attribute-only selection swap**: reduces per-keypress work
- **Tick counter clock**: avoids slow `_dos_gettime()` on slow machines
- **clamp_view_top**: prevents unnecessary full redraws
