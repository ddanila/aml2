#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <i86.h>
#include <string.h>

#include "ui_int.h"

static unsigned short aml_ui_backbuf[AML_UI_ROWS * AML_UI_COLS];

static unsigned short aml_ui_cell(unsigned char ch, unsigned char attr)
{
    return (unsigned short)ch | ((unsigned short)attr << 8);
}

void aml_ui_hide_cursor(void)
{
    union REGS regs;

    regs.h.ah = 0x01;
    regs.x.cx = 0x2000;
    int86(0x10, &regs, &regs);
}

void aml_ui_show_cursor(void)
{
    union REGS regs;

    regs.h.ah = 0x01;
    regs.x.cx = 0x0607;
    int86(0x10, &regs, &regs);
}

void aml_ui_set_cursor(int col, int row)
{
    union REGS regs;

    if (col < 0) {
        col = 0;
    }
    if (col >= AML_UI_COLS) {
        col = AML_UI_COLS - 1;
    }
    if (row < 0) {
        row = 0;
    }
    if (row >= AML_UI_ROWS) {
        row = AML_UI_ROWS - 1;
    }

    regs.h.ah = 0x02;
    regs.h.bh = 0;
    regs.h.dh = (unsigned char)row;
    regs.h.dl = (unsigned char)col;
    int86(0x10, &regs, &regs);
}

static void aml_ui_wait_vsync(void)
{
    while (inp(0x3DA) & 0x08) {
    }
    while ((inp(0x3DA) & 0x08) == 0) {
    }
}

void aml_ui_flush(void)
{
    unsigned short far *video = (unsigned short far *)MK_FP(0xB800, 0);
    unsigned i;

    aml_ui_wait_vsync();
    for (i = 0; i < AML_UI_ROWS * AML_UI_COLS; ++i) {
        video[i] = aml_ui_backbuf[i];
    }
}

void aml_ui_fill_rect(int left, int top, int right, int bottom,
                      unsigned char ch, unsigned char attr)
{
    int row;
    int col;
    unsigned short cell = aml_ui_cell(ch, attr);

    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right >= AML_UI_COLS) {
        right = AML_UI_COLS - 1;
    }
    if (bottom >= AML_UI_ROWS) {
        bottom = AML_UI_ROWS - 1;
    }

    for (row = top; row <= bottom; ++row) {
        for (col = left; col <= right; ++col) {
            aml_ui_backbuf[row * AML_UI_COLS + col] = cell;
        }
    }
}

void aml_ui_putc(int col, int row, unsigned char ch, unsigned char attr)
{
    if (col < 0 || col >= AML_UI_COLS || row < 0 || row >= AML_UI_ROWS) {
        return;
    }
    aml_ui_backbuf[row * AML_UI_COLS + col] = aml_ui_cell(ch, attr);
}

void aml_ui_write_at(int col, int row, const char *text, unsigned char attr)
{
    while (*text != '\0' && col < AML_UI_COLS) {
        aml_ui_putc(col, row, (unsigned char)*text, attr);
        col++;
        text++;
    }
}

void aml_ui_write_padded(int col, int row, const char *text, int width, unsigned char attr)
{
    int i;
    int done = 0;

    for (i = 0; i < width; ++i) {
        unsigned char ch = ' ';

        if (!done && text[i] != '\0') {
            ch = (unsigned char)text[i];
        } else {
            done = 1;
        }
        aml_ui_putc(col + i, row, ch, attr);
    }
}

int aml_ui_visible_start(const char *text, int width, int start, int cursor)
{
    int len = (int)strlen(text);
    int visible_start = start;
    int visible_end;

    if (visible_start < 0) {
        visible_start = 0;
    }
    if (cursor < visible_start) {
        visible_start = cursor;
    }
    if (cursor > visible_start + width - 1) {
        visible_start = cursor - width + 1;
    }
    if (visible_start < 0) {
        visible_start = 0;
    }

    visible_end = visible_start + width;
    if (visible_end > len) {
        visible_end = len;
    }
    if (visible_end - visible_start < width && visible_start > 0) {
        visible_start = visible_end - width;
        if (visible_start < 0) {
            visible_start = 0;
        }
    }

    return visible_start;
}

void aml_ui_write_clipped(int col, int row, const char *text, int width,
                          int start, int cursor, unsigned char attr)
{
    int len = (int)strlen(text);
    int i;
    int visible_start = aml_ui_visible_start(text, width, start, cursor);
    int visible_end;

    if (width <= 0) {
        return;
    }

    visible_end = visible_start + width;
    if (visible_end > len) {
        visible_end = len;
    }

    for (i = 0; i < width; ++i) {
        int src = visible_start + i;
        unsigned char ch = ' ';

        if (src < len) {
            ch = (unsigned char)text[src];
        }
        aml_ui_putc(col + i, row, ch, attr);
    }

    if (visible_start > 0 && width >= 3) {
        aml_ui_putc(col, row, '.', attr);
        aml_ui_putc(col + 1, row, '.', attr);
        aml_ui_putc(col + 2, row, '.', attr);
    }
    if (visible_end < len && width >= 3) {
        aml_ui_putc(col + width - 3, row, '.', attr);
        aml_ui_putc(col + width - 2, row, '.', attr);
        aml_ui_putc(col + width - 1, row, '.', attr);
    }
}

void aml_ui_write_ellipsis(int col, int row, const char *text, int width, unsigned char attr)
{
    int len = (int)strlen(text);

    if (len <= width) {
        aml_ui_write_padded(col, row, text, width, attr);
        return;
    }

    aml_ui_write_padded(col, row, text, width, attr);
    if (width >= 3) {
        aml_ui_putc(col + width - 3, row, '.', attr);
        aml_ui_putc(col + width - 2, row, '.', attr);
        aml_ui_putc(col + width - 1, row, '.', attr);
    }
}

void aml_ui_write_centered(int row, const char *text, unsigned char attr)
{
    int col = (AML_UI_COLS - (int)strlen(text)) / 2;

    if (col < 1) {
        col = 1;
    }
    aml_ui_write_at(col, row, text, attr);
}

void aml_ui_write_2digit_at(int col, int row, unsigned value, unsigned char attr)
{
    aml_ui_putc(col, row, (unsigned char)('0' + ((value / 10) % 10)), attr);
    aml_ui_putc(col + 1, row, (unsigned char)('0' + (value % 10)), attr);
}

unsigned aml_ui_current_second(void)
{
    struct dostime_t now;

    _dos_gettime(&now);
    return now.second;
}

void aml_ui_draw_frame(void)
{
    int i;

    aml_ui_putc(AML_UI_FRAME_LEFT, AML_UI_FRAME_TOP, 218, AML_UI_ATTR_FRAME);
    aml_ui_putc(AML_UI_FRAME_RIGHT, AML_UI_FRAME_TOP, 191, AML_UI_ATTR_FRAME);
    aml_ui_putc(AML_UI_FRAME_LEFT, AML_UI_FRAME_BOTTOM, 192, AML_UI_ATTR_FRAME);
    aml_ui_putc(AML_UI_FRAME_RIGHT, AML_UI_FRAME_BOTTOM, 217, AML_UI_ATTR_FRAME);

    for (i = AML_UI_FRAME_LEFT + 1; i < AML_UI_FRAME_RIGHT; ++i) {
        aml_ui_putc(i, AML_UI_FRAME_TOP, 196, AML_UI_ATTR_FRAME);
        aml_ui_putc(i, AML_UI_FRAME_BOTTOM, 196, AML_UI_ATTR_FRAME);
    }

    for (i = AML_UI_FRAME_TOP + 1; i < AML_UI_FRAME_BOTTOM; ++i) {
        aml_ui_putc(AML_UI_FRAME_LEFT, i, 179, AML_UI_ATTR_FRAME);
        aml_ui_putc(AML_UI_FRAME_RIGHT, i, 179, AML_UI_ATTR_FRAME);
    }
}

static void aml_ui_apply_shadow(int left, int top, int right, int bottom)
{
    int row;
    int col;

    for (row = top + 1; row <= bottom + 1 && row < AML_UI_ROWS; ++row) {
        for (col = right + 1; col <= right + 2 && col < AML_UI_COLS; ++col) {
            unsigned short cell = aml_ui_backbuf[row * AML_UI_COLS + col];
            aml_ui_backbuf[row * AML_UI_COLS + col] =
                aml_ui_cell((unsigned char)(cell & 0x00FF), AML_UI_ATTR_SHADOW);
        }
    }

    row = bottom + 1;
    if (row < AML_UI_ROWS) {
        for (col = left + 2; col <= right + 2 && col < AML_UI_COLS; ++col) {
            unsigned short cell = aml_ui_backbuf[row * AML_UI_COLS + col];
            aml_ui_backbuf[row * AML_UI_COLS + col] =
                aml_ui_cell((unsigned char)(cell & 0x00FF), AML_UI_ATTR_SHADOW);
        }
    }
}

static void aml_ui_draw_dialog_box(int left, int top, int right, int bottom, const char *title)
{
    int title_len;
    int title_col;
    int i;

    aml_ui_apply_shadow(left, top, right, bottom);
    aml_ui_fill_rect(left, top, right, bottom, ' ', AML_UI_ATTR_DIALOG);
    aml_ui_putc(left, top, 218, AML_UI_ATTR_FRAME);
    aml_ui_putc(right, top, 191, AML_UI_ATTR_FRAME);
    aml_ui_putc(left, bottom, 192, AML_UI_ATTR_FRAME);
    aml_ui_putc(right, bottom, 217, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(left + 1, top, right - 1, top, 196, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(left + 1, bottom, right - 1, bottom, 196, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(left, top + 1, left, bottom - 1, 179, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(right, top + 1, right, bottom - 1, 179, AML_UI_ATTR_FRAME);

    if (title != NULL && title[0] != '\0') {
        title_len = (int)strlen(title);
        if (title_len > right - left - 4) {
            title_len = right - left - 4;
        }
        title_col = left + ((right - left + 1 - title_len) / 2);
        if (title_col < left + 2) {
            title_col = left + 2;
        }

        aml_ui_putc(title_col - 1, top, ' ', AML_UI_ATTR_DIALOG);
        for (i = 0; i < title_len; ++i) {
            aml_ui_putc(title_col + i, top, (unsigned char)title[i], AML_UI_ATTR_DIALOG_TEXT);
        }
        aml_ui_putc(title_col + title_len, top, ' ', AML_UI_ATTR_DIALOG);
    }
}

int aml_ui_dialog_row(int top, int inner_row)
{
    return top + 1 + inner_row;
}

void aml_ui_draw_titled_dialog(int left, int top, int right, int bottom, const char *title)
{
    aml_ui_draw_dialog_box(left, top, right, bottom, title);
}

void aml_ui_wait_for_ack(void)
{
#if AML_TEST_HOOKS
    for (;;) {
        char line[AML_MAX_LINE + 1];

        if (aml_ui_read_auto_line(line, sizeof(line))) {
            if (strcmp(line, "ack") == 0) {
                aml_ui_trace_event("auto_ack");
                return;
            }
        }

        if (kbhit()) {
            getch();
            return;
        }
        delay(50);
    }
#else
    getch();
#endif
}

void aml_ui_draw_header_on_frame_common(int modified)
{
    struct dostime_t now;
    char title[80];
    int clock_col = 72;

    _dos_gettime(&now);

    strcpy(title, " Arvutimuuseum Launcher (c) 2026 Danila Sukharev, v");
    strncat(title, AML_BUILD_VERSION, sizeof(title) - strlen(title) - 1);
    strncat(title, " ", sizeof(title) - strlen(title) - 1);
    if ((int)strlen(title) > clock_col - 1) {
        title[clock_col - 1] = '\0';
    }

    aml_ui_fill_rect(1, 0, AML_UI_FRAME_RIGHT - 1, 0, 196, AML_UI_ATTR_FRAME);
    aml_ui_write_at(1, 0, title, AML_UI_ATTR_TITLE);
    aml_ui_write_2digit_at(clock_col, 0, now.hour, AML_UI_ATTR_HELP);
    aml_ui_putc(clock_col + 2, 0, (now.second & 1) ? ':' : ' ', AML_UI_ATTR_HELP);
    aml_ui_write_2digit_at(clock_col + 3, 0, now.minute, AML_UI_ATTR_HELP);
    if (modified) {
        aml_ui_putc(78, 0, '*', AML_UI_ATTR_HELP);
    }
}

void aml_ui_draw_header_on_frame(const AmlState *state)
{
    aml_ui_draw_header_on_frame_common(state->modified);
    if (state->editor_mode) {
        aml_ui_write_at(66, 0, "EDIT", AML_UI_ATTR_HELP);
    }
}

int aml_ui_hotkey_index(int key)
{
    if (key >= '0' && key <= '9') {
        return key - '0';
    }
    if (key >= 'a' && key <= 'z') {
        return 10 + (key - 'a');
    }
    if (key >= 'A' && key <= 'Z') {
        return 36 + (key - 'A');
    }
    return -1;
}

char aml_ui_hotkey_char(int index)
{
    if (index >= 0 && index <= 9) {
        return (char)('0' + index);
    }
    if (index >= 10 && index < 36) {
        return (char)('a' + (index - 10));
    }
    if (index >= 36 && index < 62) {
        return (char)('A' + (index - 36));
    }
    return ' ';
}

static int aml_ui_name_contains(const char *name, const char *needle)
{
    const char *scan;
    const char *match_name;
    const char *match_needle;

    if (needle[0] == '\0') {
        return 1;
    }

    for (scan = name; *scan != '\0'; ++scan) {
        match_name = scan;
        match_needle = needle;
        while (*match_name != '\0' &&
               *match_needle != '\0' &&
               tolower((unsigned char)*match_name) ==
               tolower((unsigned char)*match_needle)) {
            match_name++;
            match_needle++;
        }
        if (*match_needle == '\0') {
            return 1;
        }
    }

    return 0;
}

int aml_ui_find_match(const AmlState *state, const char *needle)
{
    int i;

    if (needle[0] == '\0') {
        return -1;
    }

    for (i = 0; i < state->entry_count; ++i) {
        if (aml_ui_name_contains(state->entries[i].name, needle)) {
            return i;
        }
    }

    return -1;
}

static void aml_ui_clamp_view_top(AmlState *state)
{
    if (state->entry_count <= AML_UI_LIST_ROWS) {
        state->view_top = 0;
        return;
    }

    if (state->view_top < 0) {
        state->view_top = 0;
    }
    if (state->view_top > state->entry_count - AML_UI_LIST_ROWS) {
        state->view_top = state->entry_count - AML_UI_LIST_ROWS;
    }
}

void aml_ui_sync_view_top(AmlState *state)
{
    int margin;
    int top_band;
    int bottom_band;

    if (state->entry_count <= 0) {
        state->view_top = 0;
        return;
    }

    if (state->selected < 0) {
        state->selected = 0;
    }
    if (state->selected >= state->entry_count) {
        state->selected = state->entry_count - 1;
    }

    aml_ui_clamp_view_top(state);
    margin = AML_UI_LIST_ROWS / 4;
    top_band = state->view_top + margin;
    bottom_band = state->view_top + AML_UI_LIST_ROWS - margin - 1;

    if (state->selected < top_band) {
        state->view_top = state->selected - margin;
    } else if (state->selected > bottom_band) {
        state->view_top = state->selected - (AML_UI_LIST_ROWS - margin - 1);
    }

    aml_ui_clamp_view_top(state);
}

static void aml_ui_draw_scrollbar(const AmlState *state)
{
    int row;
    int thumb_row = AML_UI_LIST_ROW + 1;

    aml_ui_putc(AML_UI_SCROLL_COL, AML_UI_LIST_ROW, 30, AML_UI_ATTR_SCROLL);
    aml_ui_putc(AML_UI_SCROLL_COL, AML_UI_LIST_ROW + AML_UI_LIST_ROWS - 1, 31, AML_UI_ATTR_SCROLL);

    for (row = AML_UI_LIST_ROW + 1; row < AML_UI_LIST_ROW + AML_UI_LIST_ROWS - 1; ++row) {
        aml_ui_putc(AML_UI_SCROLL_COL, row, 176, AML_UI_ATTR_SCROLL);
    }

    if (state->entry_count > AML_UI_LIST_ROWS) {
        int track = AML_UI_LIST_ROWS - 2;
        thumb_row = AML_UI_LIST_ROW + 1 +
            ((state->selected * (track - 1)) / (state->entry_count - 1));
    }

    aml_ui_putc(AML_UI_SCROLL_COL, thumb_row, 219, AML_UI_ATTR_SCROLL_THUMB);
}

static void aml_ui_draw_entries(const AmlState *state)
{
    int row;
    int index;
    int top = state->view_top;

    aml_ui_fill_rect(AML_UI_LIST_LEFT, AML_UI_LIST_ROW, AML_UI_LIST_RIGHT,
                     AML_UI_LIST_ROW + AML_UI_LIST_ROWS - 1, ' ', AML_UI_ATTR_TEXT);

    if (state->entry_count <= 0) {
        aml_ui_write_at(6, 10, "No entries available.", AML_UI_ATTR_DIALOG_TEXT);
        aml_ui_write_at(6, 12, "Check LAUNCHER.CFG or press F10 to exit.", AML_UI_ATTR_DIALOG_DIM);
        aml_ui_draw_scrollbar(state);
        return;
    }

    for (row = 0, index = top;
         row < AML_UI_LIST_ROWS && index < state->entry_count;
         ++row, ++index) {
        int y = AML_UI_LIST_ROW + row;
        unsigned char attr = (index == state->selected) ? AML_UI_ATTR_SELECTED : AML_UI_ATTR_TEXT;
        char hotkey = aml_ui_hotkey_char(index);

        aml_ui_fill_rect(AML_UI_LIST_LEFT + 1, y, AML_UI_SCROLL_COL - 1, y, ' ', attr);
        aml_ui_putc(4, y, (index == state->selected) ? 16 : 250, attr);
        aml_ui_putc(6, y, '[', attr);
        aml_ui_putc(7, y, (unsigned char)hotkey, attr);
        aml_ui_putc(8, y, ']', attr);
        aml_ui_write_ellipsis(11, y, state->entries[index].name, AML_UI_SCROLL_COL - 11, attr);
    }

    aml_ui_draw_scrollbar(state);
}

void aml_ui_render(const AmlState *state, const char *status)
{
    (void)status;
    aml_ui_fill_rect(0, 0, AML_UI_COLS - 1, AML_UI_ROWS - 1, ' ', AML_UI_ATTR_BG);
    aml_ui_draw_frame();
    aml_ui_draw_header_on_frame(state);
    aml_ui_draw_entries(state);
}

void aml_ui_draw_detail_line(int row, const char *label, const char *value, unsigned char attr)
{
    aml_ui_write_at(18, row, label, AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_padded(28, row, value, 34, attr);
}

static void aml_ui_draw_notice_box(const char *title, const char *line1, const char *line2, const char *line3)
{
    aml_ui_draw_titled_dialog(12, 8, 67, 16, title);

    if (line1 != NULL && line1[0] != '\0') {
        aml_ui_write_centered(aml_ui_dialog_row(8, 2), line1, AML_UI_ATTR_DIALOG_TEXT);
    }
    if (line2 != NULL && line2[0] != '\0') {
        aml_ui_write_centered(aml_ui_dialog_row(8, 3), line2, AML_UI_ATTR_DIALOG_DIM);
    }
    if (line3 != NULL && line3[0] != '\0') {
        aml_ui_write_centered(aml_ui_dialog_row(8, 5), line3, AML_UI_ATTR_HELP);
    }
}

void aml_ui_show_message(const char *title, const char *line1, const char *line2, const char *line3)
{
    aml_ui_hide_cursor();
    aml_ui_fill_rect(0, 0, AML_UI_COLS - 1, AML_UI_ROWS - 1, ' ', AML_UI_ATTR_BG);
    aml_ui_draw_frame();
    aml_ui_draw_header_on_frame_common(0);
    aml_ui_draw_notice_box(title, line1, line2, line3);
    aml_ui_flush();
}

void aml_ui_show_notice(const AmlState *state, const char *title, const char *line1, const char *line2, const char *line3)
{
    aml_ui_hide_cursor();
    aml_ui_render(state, "");
    aml_ui_draw_notice_box(title, line1, line2, line3);
    aml_ui_flush();
}

void aml_ui_init(void)
{
    aml_ui_hide_cursor();
    aml_ui_fill_rect(0, 0, AML_UI_COLS - 1, AML_UI_ROWS - 1, ' ', AML_UI_ATTR_BG);
    aml_ui_flush();
}

void aml_ui_shutdown(void)
{
    aml_ui_set_cursor(0, AML_UI_ROWS - 1);
    aml_ui_show_cursor();
    aml_ui_fill_rect(0, 0, AML_UI_COLS - 1, AML_UI_ROWS - 1, ' ', 0x07);
    aml_ui_flush();
}

void aml_ui_draw(const AmlState *state, const char *status)
{
    aml_ui_hide_cursor();
    aml_ui_render(state, status);
    aml_ui_flush();
}
