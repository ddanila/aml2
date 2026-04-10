/*
 * TEST_002: Custom font loaded, NO 8-dot clock, NO VSYNC.
 * Uploads bigtext glyphs via INT 10h AX=1100h but does not
 * touch the sequencer clocking mode register.
 * If this works but TEST_003 doesn't, the 8-dot clock is the problem.
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
    vidtest_load_font();
    vidtest_draw_hello(0);
    vidtest_label("TEST_002", "Font only, no 8dot, no vsync");
    getch();
    set_text_mode();
    return 0;
}
