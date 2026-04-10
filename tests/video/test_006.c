/*
 * TEST_006: Custom font + 8-dot clock WITHOUT pixel clock compensation.
 * This reproduces the old broken behavior where 8-dot mode shifts the
 * horizontal frequency from 31.47 kHz to 35.40 kHz.
 * If TEST_003 now works but TEST_006 still breaks, the pixel clock
 * fix is confirmed.
 */

#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <string.h>

#include "vidtest.h"

static void enable_8dot_no_compensation(void)
{
    outp(0x3C4, 0x01);
    vidtest_saved_clocking = inp(0x3C5);
    outp(0x3C5, (unsigned char)(vidtest_saved_clocking | 0x01));
}

static void disable_8dot_no_compensation(void)
{
    outp(0x3C4, 0x01);
    outp(0x3C5, vidtest_saved_clocking);
}

int main(void)
{
    set_text_mode();
    hide_cursor();
    vidtest_prepare_bigtext();
    enable_8dot_no_compensation();
    vidtest_load_font();
    vidtest_draw_hello(0);
    vidtest_label("TEST_006", "Font + 8dot, NO clock fix (old behavior)");
    getch();
    disable_8dot_no_compensation();
    set_text_mode();
    return 0;
}
