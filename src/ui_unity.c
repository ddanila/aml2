/*
 * Unity build: compile all UI C modules as a single compilation unit.
 *
 * This produces a binary layout where all UI code is in one object file,
 * avoiding a linker layout issue that causes ~1 second keyboard input
 * lag in QEMU and on slow real hardware.
 */

#include "ui_core.c"
#include "ui_bigtext.c"
#include "ui_loop.c"
#include "ui_edit.c"
#include "ui_auto.c"
#include "ui_state.c"
#include "ui_test.c"
