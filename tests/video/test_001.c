/*
 * TEST_001: Baseline — plain mode 3 text, no custom font, no 8-dot clock.
 * Writes "HELLO WORLD" using standard VGA text characters.
 * If this fails, the card has a fundamental text-mode problem.
 */

#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <string.h>

static void set_text_mode(void)
{
    union REGS regs;

    regs.w.ax = 0x0003;
    int86(0x10, &regs, &regs);
}

static void hide_cursor(void)
{
    union REGS regs;

    regs.h.ah = 0x01;
    regs.x.cx = 0x2000;
    int86(0x10, &regs, &regs);
}

int main(void)
{
    unsigned short far *video = (unsigned short far *)MK_FP(0xB800, 0);
    const char *msg = "HELLO WORLD";
    int col = 10;
    int row = 12;
    int i;

    set_text_mode();
    hide_cursor();

    for (i = 0; msg[i] != '\0'; ++i) {
        video[row * 80 + col + i] =
            (unsigned short)msg[i] | (0x1F << 8);
    }

    video[24 * 80] = (unsigned short)'T' | (0x0F << 8);
    video[24 * 80 + 1] = (unsigned short)'E' | (0x0F << 8);
    video[24 * 80 + 2] = (unsigned short)'S' | (0x0F << 8);
    video[24 * 80 + 3] = (unsigned short)'T' | (0x0F << 8);
    video[24 * 80 + 4] = (unsigned short)'_' | (0x0F << 8);
    video[24 * 80 + 5] = (unsigned short)'0' | (0x0F << 8);
    video[24 * 80 + 6] = (unsigned short)'0' | (0x0F << 8);
    video[24 * 80 + 7] = (unsigned short)'1' | (0x0F << 8);
    video[24 * 80 + 9] = (unsigned short)'P' | (0x07 << 8);
    video[24 * 80 + 10] = (unsigned short)'l' | (0x07 << 8);
    video[24 * 80 + 11] = (unsigned short)'a' | (0x07 << 8);
    video[24 * 80 + 12] = (unsigned short)'i' | (0x07 << 8);
    video[24 * 80 + 13] = (unsigned short)'n' | (0x07 << 8);
    video[24 * 80 + 15] = (unsigned short)'m' | (0x07 << 8);
    video[24 * 80 + 16] = (unsigned short)'o' | (0x07 << 8);
    video[24 * 80 + 17] = (unsigned short)'d' | (0x07 << 8);
    video[24 * 80 + 18] = (unsigned short)'e' | (0x07 << 8);
    video[24 * 80 + 20] = (unsigned short)'3' | (0x07 << 8);

    getch();
    set_text_mode();
    return 0;
}
