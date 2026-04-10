/*
 * TEST_003: Custom font + 8-dot clock, NO VSYNC.
 * Enables the sequencer clocking mode bit 0 (8-dot / 9-bit mode).
 * If this breaks but TEST_002 is fine, the 8-dot clock is the culprit.
 */

#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <string.h>

#include "vidtest.h"

int main(void)
{
    set_text_mode();
    hide_cursor();
    vidtest_prepare_bigtext();
    vidtest_enable_8dot();
    vidtest_load_font();
    vidtest_draw_hello(0);
    vidtest_label("TEST_003", "Font + 8dot, no vsync");
    getch();
    vidtest_disable_8dot();
    set_text_mode();
    return 0;
}
