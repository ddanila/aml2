/*
 * TEST_005: Custom font + VSYNC, NO 8-dot clock.
 * Tests if VSYNC-synced blitting works without the clocking mode change.
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
    vidtest_draw_hello(1);
    vidtest_label("TEST_005", "Font + vsync, no 8dot");
    getch();
    set_text_mode();
    return 0;
}
