#include <conio.h>

#include "ui.h"
#include "ui_int.h"

#define ATTR_BG 0x1F
#define ATTR_BIG 0x1E
#define ATTR_DIM 0x17

int main(void)
{
    ui_init();
    ui_bigtext_enable();
    ui_fill_rect(0, 0, 79, 24, ' ', ATTR_BG);
    ui_write_at(2, 1, "VGA text mode big-letter experiment", ATTR_DIM);
    ui_write_at(2, 2, "ROM 8x16 A-Z glyphs scaled 2x and split across 4 custom chars.", ATTR_DIM);
    ui_write_at(2, 4, "HELLO VGA", ATTR_BIG);
    ui_bigtext_write_at(2, 6, "HELLO VGA", ATTR_BIG);
    ui_write_at(2, 10, "AML DEMO", ATTR_BIG);
    ui_bigtext_write_at(2, 12, "AML DEMO", ATTR_BIG);
    ui_write_at(2, 18, "Only extended characters were reprogrammed; normal ASCII stays intact.", ATTR_DIM);
    ui_write_at(2, 20, "Press any key to restore the ROM font and return to DOS.", ATTR_DIM);
    ui_flush();

    getch();
    ui_shutdown();
    return 0;
}
