/*
 * TEST_004: Custom font + 8-dot clock + VSYNC sync.
 * This is the full current launcher approach.
 * If this works, the card is compatible with the production code.
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
    vidtest_draw_hello(1);
    vidtest_label("TEST_004", "Font + 8dot + vsync");
    getch();
    vidtest_disable_8dot();
    set_text_mode();
    return 0;
}
