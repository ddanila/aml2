#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <i86.h>
#include <string.h>

#include <stdlib.h>

#include "ui_int.h"

enum {
    UI_BIGTEXT_GLYPHS = 36,
    UI_BIGTEXT_TILE_COUNT = UI_BIGTEXT_GLYPHS * 4,
    UI_BIGTEXT_BYTES = 16,
    UI_BIGTEXT_FONT_SIZE = 256 * UI_BIGTEXT_BYTES
};

static unsigned char (*ui_bigtext_original_font)[UI_BIGTEXT_BYTES];
static unsigned char (*ui_bigtext_patched_font)[UI_BIGTEXT_BYTES];
static unsigned char (*ui_bigtext_fancy_font)[UI_BIGTEXT_BYTES];
static unsigned char ui_bigtext_codes[UI_BIGTEXT_TILE_COUNT];
static int ui_bigtext_ready;
static int ui_bigtext_enabled;
static int ui_bigtext_fancy_active;
static int ui_bigtext_8dot_known;
static int ui_bigtext_8dot_supported;
static int ui_bigtext_8dot_active;
static unsigned char ui_bigtext_saved_clocking_mode;
static unsigned char ui_bigtext_saved_misc_output;

static unsigned ui_bigtext_glyph_index(unsigned char ch);

static int ui_bigtext_should_use_8dot_clock(void)
{
    union REGS regs;

    if (ui_bigtext_8dot_known) {
        return ui_bigtext_8dot_supported;
    }

    regs.w.ax = 0x1A00;
    int86(0x10, &regs, &regs);

    ui_bigtext_8dot_supported = 1;
    if (regs.h.al == 0x1A && regs.h.bl == 0x07) {
        ui_bigtext_8dot_supported = 0;
    }

    ui_bigtext_8dot_known = 1;
    return ui_bigtext_8dot_supported;
}

static int ui_bigtext_is_reserved_code(unsigned char code)
{
    return code == 176 ||
           code == 179 ||
           code == 191 ||
           code == 192 ||
           code == 196 ||
           code == 217 ||
           code == 218 ||
           code == 219 ||
           code == 250;
}

static void ui_bigtext_init_code_map(void)
{
    unsigned code;
    unsigned idx = 0;

    for (code = 0; code < 27 && idx < UI_BIGTEXT_TILE_COUNT; ++code) {
        if (ui_bigtext_is_reserved_code((unsigned char)code)) {
            continue;
        }
        ui_bigtext_codes[idx++] = (unsigned char)code;
    }

    for (code = 128; code <= 255 && idx < UI_BIGTEXT_TILE_COUNT; ++code) {
        if (ui_bigtext_is_reserved_code((unsigned char)code)) {
            continue;
        }
        ui_bigtext_codes[idx++] = (unsigned char)code;
    }
}

static void ui_bigtext_get_rom_font_8x16(unsigned char far **font_ptr)
{
    union REGPACK regs;

    memset(&regs, 0, sizeof(regs));
    regs.w.ax = 0x1130;
    regs.h.bh = 0x06;
    intr(0x10, &regs);
    *font_ptr = (unsigned char far *)MK_FP(regs.w.es, regs.w.bp);
}

static void ui_bigtext_load_font(unsigned char font[256][UI_BIGTEXT_BYTES])
{
    union REGPACK regs;

    memset(&regs, 0, sizeof(regs));
    regs.w.ax = 0x1100;
    regs.h.bh = UI_BIGTEXT_BYTES;
    regs.h.bl = 0x00;
    regs.w.cx = 256;
    regs.w.dx = 0;
    regs.w.bp = FP_OFF(font);
    regs.w.es = FP_SEG(font);
    intr(0x10, &regs);
}

static void ui_bigtext_capture_default_font(void)
{
    unsigned char far *font_ptr;
    unsigned i;

    ui_bigtext_get_rom_font_8x16(&font_ptr);
    for (i = 0; i < sizeof(ui_bigtext_original_font); ++i) {
        ((unsigned char *)ui_bigtext_original_font)[i] = font_ptr[i];
    }
    memcpy(ui_bigtext_patched_font, ui_bigtext_original_font, sizeof(ui_bigtext_patched_font));
    memcpy(ui_bigtext_fancy_font, ui_bigtext_original_font, sizeof(ui_bigtext_fancy_font));
}

static void ui_bigtext_build_letter_into(unsigned char letter,
                                          unsigned char font[256][UI_BIGTEXT_BYTES],
                                          int fancy)
{
    unsigned char scaled_rows[32][2];
    unsigned char quads[4][UI_BIGTEXT_BYTES];
    unsigned char far *src = ui_bigtext_original_font[letter];
    unsigned row;
    unsigned col;
    unsigned q;
    unsigned base = ui_bigtext_glyph_index(letter) * 4;

    memset(scaled_rows, 0, sizeof(scaled_rows));
    memset(quads, 0, sizeof(quads));

    for (row = 0; row < UI_BIGTEXT_BYTES; ++row) {
        for (col = 0; col < 8; ++col) {
            if ((src[row] & (0x80u >> col)) != 0) {
                unsigned scaled_col = col * 2;
                unsigned scaled_row = row * 2;

                scaled_rows[scaled_row][scaled_col >> 3] |=
                    (unsigned char)(0x80u >> (scaled_col & 7));
                scaled_rows[scaled_row][(scaled_col + 1) >> 3] |=
                    (unsigned char)(0x80u >> ((scaled_col + 1) & 7));
                scaled_rows[scaled_row + 1][scaled_col >> 3] |=
                    (unsigned char)(0x80u >> (scaled_col & 7));
                scaled_rows[scaled_row + 1][(scaled_col + 1) >> 3] |=
                    (unsigned char)(0x80u >> ((scaled_col + 1) & 7));
            }
        }
    }

    if (fancy) {
        for (row = 16; row < 32; ++row) {
            unsigned char s0 = (unsigned char)((scaled_rows[row][0] << 1) |
                                               (scaled_rows[row][1] >> 7));
            unsigned char s1 = (unsigned char)(scaled_rows[row][1] << 1);
            scaled_rows[row][0] |= s0;
            scaled_rows[row][1] |= s1;
        }
    }

    for (row = 0; row < 32; ++row) {
        for (col = 0; col < 16; ++col) {
            unsigned char mask = (unsigned char)(0x80u >> (col & 7));

            if ((scaled_rows[row][col >> 3] & mask) == 0) {
                continue;
            }

            q = (row >= 16 ? 2u : 0u) + (col >= 8 ? 1u : 0u);
            quads[q][row & 15] |= (unsigned char)(0x80u >> (col & 7));
        }
    }

    for (q = 0; q < 4; ++q) {
        memcpy(font[ui_bigtext_codes[base + q]], quads[q], UI_BIGTEXT_BYTES);
    }
}

static unsigned ui_bigtext_glyph_index(unsigned char ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return (unsigned)(ch - 'A');
    }

    return 26u + (unsigned)(ch - '0');
}

static int ui_bigtext_is_supported_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}

static void ui_bigtext_build_font(void)
{
    unsigned char ch;

    for (ch = 'A'; ch <= 'Z'; ++ch) {
        ui_bigtext_build_letter_into(ch, ui_bigtext_patched_font, 0);
        ui_bigtext_build_letter_into(ch, ui_bigtext_fancy_font, 1);
    }
    for (ch = '0'; ch <= '9'; ++ch) {
        ui_bigtext_build_letter_into(ch, ui_bigtext_patched_font, 0);
        ui_bigtext_build_letter_into(ch, ui_bigtext_fancy_font, 1);
    }
}

static void ui_bigtext_prepare(void)
{
    if (ui_bigtext_ready) {
        return;
    }

    ui_bigtext_original_font = malloc(UI_BIGTEXT_FONT_SIZE);
    ui_bigtext_patched_font = malloc(UI_BIGTEXT_FONT_SIZE);
    ui_bigtext_fancy_font = malloc(UI_BIGTEXT_FONT_SIZE);

    ui_bigtext_init_code_map();
    ui_bigtext_capture_default_font();
    ui_bigtext_build_font();
    ui_bigtext_ready = 1;
}

static void ui_bigtext_activate(int fancy)
{
    unsigned char (*font)[UI_BIGTEXT_BYTES] = fancy
        ? ui_bigtext_fancy_font
        : ui_bigtext_patched_font;

    if (!ui_bigtext_enabled) {
        ui_bigtext_8dot_active = 0;
        if (ui_bigtext_should_use_8dot_clock()) {
            ui_bigtext_saved_misc_output = inp(0x3CC);
            outp(0x3C2, (unsigned char)(ui_bigtext_saved_misc_output & ~0x0C));
            outp(0x3C4, 0x01);
            ui_bigtext_saved_clocking_mode = inp(0x3C5);
            outp(0x3C5, (unsigned char)(ui_bigtext_saved_clocking_mode | 0x01));
            ui_bigtext_8dot_active = 1;
        }
        ui_bigtext_load_font(font);
        ui_bigtext_fancy_active = fancy;
        ui_bigtext_enabled = 1;
    } else if (ui_bigtext_fancy_active != fancy) {
        ui_bigtext_load_font(font);
        ui_bigtext_fancy_active = fancy;
    }
}

int ui_bigtext_enable(void)
{
    ui_bigtext_prepare();
    ui_bigtext_activate(0);
    return 1;
}

int ui_bigtext_enable_fancy(void)
{
    ui_bigtext_prepare();
    ui_bigtext_activate(1);
    return 1;
}

void ui_bigtext_disable(void)
{
    if (!ui_bigtext_enabled) {
        return;
    }

    ui_bigtext_load_font(ui_bigtext_original_font);
    if (ui_bigtext_8dot_active) {
        outp(0x3C4, 0x01);
        outp(0x3C5, ui_bigtext_saved_clocking_mode);
        outp(0x3C2, ui_bigtext_saved_misc_output);
        ui_bigtext_8dot_active = 0;
    }
    ui_bigtext_enabled = 0;
    ui_bigtext_fancy_active = 0;
}

int ui_bigtext_is_enabled(void)
{
    return ui_bigtext_enabled;
}

static void ui_bigtext_put_char(int col, int row, char ch, unsigned char attr)
{
    unsigned base;

    ch = (char)toupper((unsigned char)ch);
    if (!ui_bigtext_is_supported_char(ch)) {
        if (ch == ' ') {
            return;
        }
        ch = '?';
    }

    if (ch == '?') {
        ui_putc(col, row, '?', attr);
        ui_putc(col + 1, row, '?', attr);
        ui_putc(col, row + 1, '?', attr);
        ui_putc(col + 1, row + 1, '?', attr);
        return;
    }

    base = ui_bigtext_glyph_index((unsigned char)ch) * 4;
    ui_putc(col, row, ui_bigtext_codes[base + 0], attr);
    ui_putc(col + 1, row, ui_bigtext_codes[base + 1], attr);
    ui_putc(col, row + 1, ui_bigtext_codes[base + 2], attr);
    ui_putc(col + 1, row + 1, ui_bigtext_codes[base + 3], attr);
}

void ui_bigtext_write_at(int col, int row, const char *text, unsigned char attr)
{
    if (!ui_bigtext_enabled) {
        return;
    }

    while (*text != '\0') {
        ui_bigtext_put_char(col, row, *text, attr);
        col += 3;
        ++text;
    }
}
