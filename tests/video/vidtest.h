/*
 * Shared helpers for video diagnostic test binaries.
 * Kept as a single header to avoid extra build complexity.
 */

#ifndef VIDTEST_H
#define VIDTEST_H

#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <string.h>

#define VIDTEST_GLYPHS 36
#define VIDTEST_TILES (VIDTEST_GLYPHS * 4)
#define VIDTEST_FONT_BYTES 16
#define VIDTEST_VSYNC_TIMEOUT 0x8000u

static unsigned char vidtest_original[256][VIDTEST_FONT_BYTES];
static unsigned char vidtest_patched[256][VIDTEST_FONT_BYTES];
static unsigned char vidtest_codes[VIDTEST_TILES];
static unsigned char vidtest_saved_clocking;
static unsigned char vidtest_saved_misc;

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

static int vidtest_is_reserved(unsigned char code)
{
    return code == 176 || code == 179 || code == 191 ||
           code == 192 || code == 196 || code == 217 ||
           code == 218 || code == 219 || code == 250;
}

static void vidtest_init_codes(void)
{
    unsigned code;
    unsigned idx = 0;

    for (code = 0; code < 27 && idx < VIDTEST_TILES; ++code) {
        if (!vidtest_is_reserved((unsigned char)code))
            vidtest_codes[idx++] = (unsigned char)code;
    }
    for (code = 128; code <= 255 && idx < VIDTEST_TILES; ++code) {
        if (!vidtest_is_reserved((unsigned char)code))
            vidtest_codes[idx++] = (unsigned char)code;
    }
}

static unsigned vidtest_glyph_index(unsigned char ch)
{
    if (ch >= 'A' && ch <= 'Z') return (unsigned)(ch - 'A');
    return 26u + (unsigned)(ch - '0');
}

static void vidtest_capture_rom_font(void)
{
    union REGPACK regs;
    unsigned char far *font_ptr;
    unsigned i;

    memset(&regs, 0, sizeof(regs));
    regs.w.ax = 0x1130;
    regs.h.bh = 0x06;
    intr(0x10, &regs);
    font_ptr = (unsigned char far *)MK_FP(regs.w.es, regs.w.bp);

    for (i = 0; i < sizeof(vidtest_original); ++i)
        ((unsigned char *)vidtest_original)[i] = font_ptr[i];
    memcpy(vidtest_patched, vidtest_original, sizeof(vidtest_patched));
}

static void vidtest_build_glyph(unsigned char letter)
{
    unsigned char scaled[32][2];
    unsigned char quads[4][VIDTEST_FONT_BYTES];
    unsigned char far *src = vidtest_original[letter];
    unsigned row, col, q;
    unsigned base = vidtest_glyph_index(letter) * 4;

    memset(scaled, 0, sizeof(scaled));
    memset(quads, 0, sizeof(quads));

    for (row = 0; row < VIDTEST_FONT_BYTES; ++row) {
        for (col = 0; col < 8; ++col) {
            if ((src[row] & (0x80u >> col)) != 0) {
                unsigned sc = col * 2;
                unsigned sr = row * 2;

                scaled[sr][sc >> 3] |= (unsigned char)(0x80u >> (sc & 7));
                scaled[sr][(sc + 1) >> 3] |= (unsigned char)(0x80u >> ((sc + 1) & 7));
                scaled[sr + 1][sc >> 3] |= (unsigned char)(0x80u >> (sc & 7));
                scaled[sr + 1][(sc + 1) >> 3] |= (unsigned char)(0x80u >> ((sc + 1) & 7));
            }
        }
    }

    for (row = 0; row < 32; ++row) {
        for (col = 0; col < 16; ++col) {
            unsigned char mask = (unsigned char)(0x80u >> (col & 7));

            if ((scaled[row][col >> 3] & mask) == 0) continue;
            q = (row >= 16 ? 2u : 0u) + (col >= 8 ? 1u : 0u);
            quads[q][row & 15] |= (unsigned char)(0x80u >> (col & 7));
        }
    }

    for (q = 0; q < 4; ++q)
        memcpy(vidtest_patched[vidtest_codes[base + q]], quads[q], VIDTEST_FONT_BYTES);
}

static void vidtest_prepare_bigtext(void)
{
    unsigned char ch;

    vidtest_init_codes();
    vidtest_capture_rom_font();

    for (ch = 'A'; ch <= 'Z'; ++ch) vidtest_build_glyph(ch);
    for (ch = '0'; ch <= '9'; ++ch) vidtest_build_glyph(ch);
}

static void vidtest_load_font(void)
{
    union REGPACK regs;

    memset(&regs, 0, sizeof(regs));
    regs.w.ax = 0x1100;
    regs.h.bh = VIDTEST_FONT_BYTES;
    regs.h.bl = 0x00;
    regs.w.cx = 256;
    regs.w.dx = 0;
    regs.w.bp = FP_OFF(vidtest_patched);
    regs.w.es = FP_SEG(vidtest_patched);
    intr(0x10, &regs);
}

static void vidtest_enable_8dot(void)
{
    vidtest_saved_misc = inp(0x3CC);
    outp(0x3C2, (unsigned char)(vidtest_saved_misc & ~0x0C));
    outp(0x3C4, 0x01);
    vidtest_saved_clocking = inp(0x3C5);
    outp(0x3C5, (unsigned char)(vidtest_saved_clocking | 0x01));
}

static void vidtest_disable_8dot(void)
{
    outp(0x3C4, 0x01);
    outp(0x3C5, vidtest_saved_clocking);
    outp(0x3C2, vidtest_saved_misc);
}

static void vidtest_wait_vsync(void)
{
    unsigned timeout = VIDTEST_VSYNC_TIMEOUT;

    while ((inp(0x3DA) & 0x08) != 0 && timeout-- != 0) {}
    timeout = VIDTEST_VSYNC_TIMEOUT;
    while ((inp(0x3DA) & 0x08) == 0 && timeout-- != 0) {}
}

static void vidtest_put_bigchar(unsigned short far *video, int col, int row,
                                 char ch, unsigned char attr)
{
    unsigned base;

    if (ch < 'A' || (ch > 'Z' && (ch < '0' || ch > '9'))) return;

    base = vidtest_glyph_index((unsigned char)ch) * 4;
    video[row * 80 + col] = (unsigned short)vidtest_codes[base + 0] | ((unsigned short)attr << 8);
    video[row * 80 + col + 1] = (unsigned short)vidtest_codes[base + 1] | ((unsigned short)attr << 8);
    video[(row + 1) * 80 + col] = (unsigned short)vidtest_codes[base + 2] | ((unsigned short)attr << 8);
    video[(row + 1) * 80 + col + 1] = (unsigned short)vidtest_codes[base + 3] | ((unsigned short)attr << 8);
}

static void vidtest_draw_hello(int use_vsync)
{
    unsigned short far *video = (unsigned short far *)MK_FP(0xB800, 0);
    const char *text = "HELLO WORLD";
    int col = 4;
    int row = 10;
    int i;

    for (i = 0; text[i] != '\0'; ++i) {
        if (text[i] == ' ') {
            col += 3;
            continue;
        }
        vidtest_put_bigchar(video, col, row, text[i], 0x1F);
        col += 3;
    }

    if (use_vsync) {
        vidtest_wait_vsync();
    }
}

static void vidtest_label(const char *name, const char *desc)
{
    unsigned short far *video = (unsigned short far *)MK_FP(0xB800, 0);
    int i;

    for (i = 0; name[i] != '\0'; ++i)
        video[24 * 80 + i] = (unsigned short)name[i] | (0x0F << 8);
    video[24 * 80 + i] = (unsigned short)' ' | (0x07 << 8);
    ++i;
    for (; *desc != '\0'; ++i, ++desc)
        video[24 * 80 + i] = (unsigned short)*desc | (0x07 << 8);
}

#endif
