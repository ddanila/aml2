#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <i86.h>
#include <string.h>

#include "ui_int.h"

enum {
    UI_BIGTEXT_GLYPHS = 36,
    UI_BIGTEXT_TILE_COUNT = UI_BIGTEXT_GLYPHS * 4,
    UI_BIGTEXT_BYTES = 16
};

static unsigned char ui_bigtext_original_font[256][UI_BIGTEXT_BYTES];
static unsigned char ui_bigtext_patched_font[256][UI_BIGTEXT_BYTES];
static unsigned char ui_bigtext_fancy_font[256][UI_BIGTEXT_BYTES];
static unsigned char ui_bigtext_codes[UI_BIGTEXT_TILE_COUNT];
static int ui_bigtext_ready;
static int ui_bigtext_enabled;
static int ui_bigtext_fancy_active;
static int ui_bigtext_8dot_known;
static int ui_bigtext_8dot_supported;
static int ui_bigtext_8dot_active;
static unsigned char ui_bigtext_saved_clocking_mode;
static unsigned char ui_bigtext_saved_misc_output;

/* Debug approach selector for the 8-dot clock dance:
   1 = baseline: MOR + SR1, vretrace + CLI + sync reset (current default)
   2 = SR1 only: do NOT change MOR (diagnostic: did SR1 8-dot bit take effect?)
   3 = MOR only: do NOT change SR1 (diagnostic: did MOR clock change take effect?)
   4 = font swap only: skip both register writes (no clock change at all)
   5 = reversed order: SR1 first, then MOR, otherwise same safety as #1 */
enum {
    UI_BT_APPROACH_DEFAULT = 1,
    UI_BT_APPROACH_MIN = 1,
    UI_BT_APPROACH_MAX = 6
};
static int ui_bigtext_approach = UI_BT_APPROACH_DEFAULT;
static int ui_bigtext_active_approach;

/* Saved CRTC state for approach 6 (CRTC compensation). Indices: 0..5 hold
   CR0..CR5; index 6 holds CR11 so we can restore the protect bit. */
static unsigned char ui_bigtext_saved_crtc[7];

static unsigned char ui_bigtext_crtc_read(unsigned char idx)
{
    outp(0x3D4, idx);
    return inp(0x3D5);
}

static void ui_bigtext_crtc_write(unsigned char idx, unsigned char val)
{
    outp(0x3D4, idx);
    outp(0x3D5, val);
}

static unsigned ui_bigtext_glyph_index(unsigned char ch);

static void ui_bigtext_wait_vretrace_start(void)
{
    /* Wait out any in-progress retrace, then wait for the next one to begin.
       The ~380 us vertical-blank window that follows is long enough to update
       MOR and SR1 with the sequencer in sync reset. */
    while (inp(0x3DA) & 0x08) {}
    while (!(inp(0x3DA) & 0x08)) {}
}

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

    ui_bigtext_init_code_map();
    ui_bigtext_capture_default_font();
    ui_bigtext_build_font();
    ui_bigtext_ready = 1;
}

static void ui_bigtext_apply_clock_change(int approach)
{
    unsigned char new_mor = (unsigned char)(ui_bigtext_saved_misc_output & ~0x0C);
    unsigned char new_sr1 = (unsigned char)(ui_bigtext_saved_clocking_mode | 0x01);

    switch (approach) {
    case 1:  /* baseline: MOR + SR1, vretrace + CLI + sync reset */
        ui_bigtext_wait_vretrace_start();
        _disable();
        outp(0x3C4, 0x00); outp(0x3C5, 0x01);
        outp(0x3C2, new_mor);
        outp(0x3C4, 0x01); outp(0x3C5, new_sr1);
        outp(0x3C4, 0x00); outp(0x3C5, 0x03);
        _enable();
        break;
    case 2:  /* SR1 only — leave MOR at 28.322 MHz */
        ui_bigtext_wait_vretrace_start();
        _disable();
        outp(0x3C4, 0x00); outp(0x3C5, 0x01);
        outp(0x3C4, 0x01); outp(0x3C5, new_sr1);
        outp(0x3C4, 0x00); outp(0x3C5, 0x03);
        _enable();
        break;
    case 3:  /* MOR only — leave SR1 in 9-dot mode */
        ui_bigtext_wait_vretrace_start();
        _disable();
        outp(0x3C4, 0x00); outp(0x3C5, 0x01);
        outp(0x3C2, new_mor);
        outp(0x3C4, 0x00); outp(0x3C5, 0x03);
        _enable();
        break;
    case 4:  /* font swap only — no clock or char-width change */
        break;
    case 5:  /* reversed order: SR1 first, then MOR */
        ui_bigtext_wait_vretrace_start();
        _disable();
        outp(0x3C4, 0x00); outp(0x3C5, 0x01);
        outp(0x3C4, 0x01); outp(0x3C5, new_sr1);
        outp(0x3C2, new_mor);
        outp(0x3C4, 0x00); outp(0x3C5, 0x03);
        _enable();
        break;
    case 6: {
        /* CRTC compensation: leave MOR at 28.322 MHz, enable SR1 8-dot,
           and extend the CRTC horizontal total from 100 to 112 char
           clocks so 8-dot lines run at 28.322 / (112*8) = 31.61 kHz —
           inside both monitors' VGA-text range. CR0..CR5 are protected
           by CR11 bit 7 in mode 3, so unlock first.

           Mode 3 baseline (saved here):
             CR0=0x5F (100 char clocks)  CR1=0x4F  CR2=0x50
             CR3=0x82  CR4=0x55  CR5=0x81

           New layout (112 char clocks, visible 0..79 unchanged):
             CR0=0x6B (=112-5)
             CR4=0x5A (sync starts at char 90)
             CR5=0x85 (sync ends at char 101 mod 32 = 5; bit 7 = 1
                       for End-Blank bit-5 = 1)
             CR3=0x8F (compat bit + End-Blank low 5 = 0x0F; combined
                       with CR5[7] = 6-bit value 0x2F = 47 → blank
                       runs from char 80 (CR2) until count mod 64 = 47,
                       i.e. char 111). */
        ui_bigtext_saved_crtc[0] = ui_bigtext_crtc_read(0x00);
        ui_bigtext_saved_crtc[1] = ui_bigtext_crtc_read(0x01);
        ui_bigtext_saved_crtc[2] = ui_bigtext_crtc_read(0x02);
        ui_bigtext_saved_crtc[3] = ui_bigtext_crtc_read(0x03);
        ui_bigtext_saved_crtc[4] = ui_bigtext_crtc_read(0x04);
        ui_bigtext_saved_crtc[5] = ui_bigtext_crtc_read(0x05);
        ui_bigtext_saved_crtc[6] = ui_bigtext_crtc_read(0x11);

        ui_bigtext_wait_vretrace_start();
        _disable();
        outp(0x3C4, 0x00); outp(0x3C5, 0x01);
        /* Unlock CR0..CR7 (CR11 bit 7 = 0). */
        ui_bigtext_crtc_write(0x11,
            (unsigned char)(ui_bigtext_saved_crtc[6] & 0x7F));
        ui_bigtext_crtc_write(0x00, 0x6B);
        ui_bigtext_crtc_write(0x03, 0x8F);
        ui_bigtext_crtc_write(0x04, 0x5A);
        ui_bigtext_crtc_write(0x05, 0x85);
        /* Re-lock with original protect-bit state. */
        ui_bigtext_crtc_write(0x11, ui_bigtext_saved_crtc[6]);
        /* Set SR1 8-dot. */
        outp(0x3C4, 0x01); outp(0x3C5, new_sr1);
        outp(0x3C4, 0x00); outp(0x3C5, 0x03);
        _enable();
        break;
    }
    default:
        break;
    }
}

static void ui_bigtext_revert_clock_change(int approach)
{
    switch (approach) {
    case 1:
    case 5:
        ui_bigtext_wait_vretrace_start();
        _disable();
        outp(0x3C4, 0x00); outp(0x3C5, 0x01);
        outp(0x3C4, 0x01); outp(0x3C5, ui_bigtext_saved_clocking_mode);
        outp(0x3C2, ui_bigtext_saved_misc_output);
        outp(0x3C4, 0x00); outp(0x3C5, 0x03);
        _enable();
        break;
    case 2:  /* only SR1 was touched */
        ui_bigtext_wait_vretrace_start();
        _disable();
        outp(0x3C4, 0x00); outp(0x3C5, 0x01);
        outp(0x3C4, 0x01); outp(0x3C5, ui_bigtext_saved_clocking_mode);
        outp(0x3C4, 0x00); outp(0x3C5, 0x03);
        _enable();
        break;
    case 3:  /* only MOR was touched */
        ui_bigtext_wait_vretrace_start();
        _disable();
        outp(0x3C4, 0x00); outp(0x3C5, 0x01);
        outp(0x3C2, ui_bigtext_saved_misc_output);
        outp(0x3C4, 0x00); outp(0x3C5, 0x03);
        _enable();
        break;
    case 6:
        ui_bigtext_wait_vretrace_start();
        _disable();
        outp(0x3C4, 0x00); outp(0x3C5, 0x01);
        /* Restore SR1 first so 8-dot is cleared before timing widens
           back; otherwise we'd transiently sit at 8-dot × 100 char =
           35 kHz, which is the original monitor-A failure mode. */
        outp(0x3C4, 0x01); outp(0x3C5, ui_bigtext_saved_clocking_mode);
        ui_bigtext_crtc_write(0x11,
            (unsigned char)(ui_bigtext_saved_crtc[6] & 0x7F));
        ui_bigtext_crtc_write(0x00, ui_bigtext_saved_crtc[0]);
        ui_bigtext_crtc_write(0x01, ui_bigtext_saved_crtc[1]);
        ui_bigtext_crtc_write(0x02, ui_bigtext_saved_crtc[2]);
        ui_bigtext_crtc_write(0x03, ui_bigtext_saved_crtc[3]);
        ui_bigtext_crtc_write(0x04, ui_bigtext_saved_crtc[4]);
        ui_bigtext_crtc_write(0x05, ui_bigtext_saved_crtc[5]);
        ui_bigtext_crtc_write(0x11, ui_bigtext_saved_crtc[6]);
        outp(0x3C4, 0x00); outp(0x3C5, 0x03);
        _enable();
        break;
    case 4:
    default:
        break;
    }
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
            outp(0x3C4, 0x01);
            ui_bigtext_saved_clocking_mode = inp(0x3C5);

            ui_bigtext_active_approach = ui_bigtext_approach;
            ui_bigtext_apply_clock_change(ui_bigtext_active_approach);
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
        ui_bigtext_revert_clock_change(ui_bigtext_active_approach);
        ui_bigtext_8dot_active = 0;
    }
    ui_bigtext_enabled = 0;
    ui_bigtext_fancy_active = 0;
}

int ui_bigtext_is_enabled(void)
{
    return ui_bigtext_enabled;
}

void ui_bigtext_debug_set_approach(int approach)
{
    if (approach < UI_BT_APPROACH_MIN || approach > UI_BT_APPROACH_MAX) {
        return;
    }
    ui_bigtext_approach = approach;
}

void ui_set_bigtext_mode(int mode)
{
    ui_bigtext_debug_set_approach(mode);
}

int ui_bigtext_debug_get_approach(void)
{
    return ui_bigtext_approach;
}

void ui_bigtext_debug_retry(int fancy)
{
    ui_bigtext_disable();
    ui_bigtext_prepare();
    ui_bigtext_activate(fancy ? 1 : 0);
    /* Some approaches (notably svga / SR1 8-dot) leave the CRTC cursor-
       disable bit honoured inconsistently; re-park the cursor off-screen
       so it stays hidden across approach switches. */
    ui_hide_cursor();
}

void ui_bigtext_debug_panic_reset(void)
{
    union REGS regs;

    /* Forget any bigtext state — the BIOS will wipe VRAM fonts and
       reset MOR/SR1 to defaults below. */
    ui_bigtext_enabled = 0;
    ui_bigtext_8dot_active = 0;
    ui_bigtext_fancy_active = 0;
    ui_bigtext_ready = 0;

    /* Drop to the safe "font swap only" approach so the post-reset redraw
       doesn't immediately retrigger whichever approach broke the monitor. */
    ui_bigtext_approach = 4;

    /* INT 10h AX=0003: set 80x25 colour text mode, full reset of CRTC/
       sequencer/MOR to BIOS defaults. Recovers a monitor that lost sync. */
    regs.w.ax = 0x0003;
    int86(0x10, &regs, &regs);
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
