#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <i86.h>
#include <stddef.h>
#include <string.h>
#if AML_TEST_HOOKS
#include <stdlib.h>
#include <stdio.h>
#endif

#include "ui.h"

enum {
    AML_UI_ROWS = 25,
    AML_UI_COLS = 80,
    AML_UI_LIST_ROW = 3,
    AML_UI_LIST_ROWS = 21,
    AML_UI_SEARCH_MAX = 24,
    AML_UI_FRAME_LEFT = 0,
    AML_UI_FRAME_TOP = 0,
    AML_UI_FRAME_RIGHT = 79,
    AML_UI_FRAME_BOTTOM = 24,
    AML_UI_LIST_LEFT = 2,
    AML_UI_LIST_RIGHT = 78,
    AML_UI_SCROLL_COL = 78,
    AML_UI_ATTR_BG = 0x17,
    AML_UI_ATTR_FRAME = 0x1F,
    AML_UI_ATTR_TITLE = 0x1E,
    AML_UI_ATTR_HELP = 0x1B,
    AML_UI_ATTR_TEXT = 0x1F,
    AML_UI_ATTR_MUTED = 0x19,
    AML_UI_ATTR_STATUS = 0x1F,
    AML_UI_ATTR_SELECTED = 0x30,
    AML_UI_ATTR_DIALOG = 0x17,
    AML_UI_ATTR_DIALOG_TEXT = 0x1F,
    AML_UI_ATTR_DIALOG_DIM = 0x1E,
    AML_UI_ATTR_SCROLL = 0x19,
    AML_UI_ATTR_SCROLL_THUMB = 0x3F,
    AML_KEY_ENTER = 13,
    AML_KEY_BACKSPACE = 8,
    AML_KEY_SLASH = '/',
    AML_KEY_QUESTION = '?',
    AML_KEY_ESC = 27,
    AML_KEY_EXTENDED = 0,
    AML_KEY_EXTENDED_2 = 224,
    AML_KEY_INS = 82,
    AML_KEY_F1 = 59,
    AML_KEY_F2 = 60,
    AML_KEY_F3 = 61,
    AML_KEY_F4 = 62,
    AML_KEY_HOME = 71,
    AML_KEY_LEFT = 75,
    AML_KEY_RIGHT = 77,
    AML_KEY_UP = 72,
    AML_KEY_PGUP = 73,
    AML_KEY_END = 79,
    AML_KEY_DOWN = 80,
    AML_KEY_PGDN = 81,
    AML_KEY_DEL = 83
};

enum {
    AML_UI_AUTO_NONE = -1,
    AML_UI_AUTO_REDRAW = -2
};

static unsigned short aml_ui_backbuf[AML_UI_ROWS * AML_UI_COLS];

static unsigned short aml_ui_cell(unsigned char ch, unsigned char attr)
{
    return (unsigned short)ch | ((unsigned short)attr << 8);
}

static void aml_ui_hide_cursor(void)
{
    union REGS regs;

    regs.h.ah = 0x01;
    regs.x.cx = 0x2000;
    int86(0x10, &regs, &regs);
}

static void aml_ui_show_cursor(void)
{
    union REGS regs;

    regs.h.ah = 0x01;
    regs.x.cx = 0x0607;
    int86(0x10, &regs, &regs);
}

static void aml_ui_set_cursor(int col, int row)
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

static void aml_ui_flush(void)
{
    unsigned short far *video = (unsigned short far *)MK_FP(0xB800, 0);
    unsigned i;

    aml_ui_wait_vsync();
    for (i = 0; i < AML_UI_ROWS * AML_UI_COLS; ++i) {
        video[i] = aml_ui_backbuf[i];
    }
}

static void aml_ui_fill_rect(int left, int top, int right, int bottom,
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

static void aml_ui_putc(int col, int row, unsigned char ch, unsigned char attr)
{
    if (col < 0 || col >= AML_UI_COLS || row < 0 || row >= AML_UI_ROWS) {
        return;
    }
    aml_ui_backbuf[row * AML_UI_COLS + col] = aml_ui_cell(ch, attr);
}

static void aml_ui_write_at(int col, int row, const char *text, unsigned char attr)
{
    while (*text != '\0' && col < AML_UI_COLS) {
        aml_ui_putc(col, row, (unsigned char)*text, attr);
        col++;
        text++;
    }
}

static void aml_ui_write_padded(int col, int row, const char *text, int width, unsigned char attr)
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

static void aml_ui_write_centered(int row, const char *text, unsigned char attr)
{
    int col = (AML_UI_COLS - (int)strlen(text)) / 2;

    if (col < 1) {
        col = 1;
    }
    aml_ui_write_at(col, row, text, attr);
}

static void aml_ui_write_uint_at(int col, int row, unsigned value, unsigned char attr)
{
    char digits[6];
    int i = 0;

    do {
        digits[i++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0 && i < (int)sizeof(digits));

    while (i > 0) {
        aml_ui_putc(col++, row, (unsigned char)digits[--i], attr);
    }
}

static void aml_ui_write_2digit_at(int col, int row, unsigned value, unsigned char attr)
{
    aml_ui_putc(col, row, (unsigned char)('0' + ((value / 10) % 10)), attr);
    aml_ui_putc(col + 1, row, (unsigned char)('0' + (value % 10)), attr);
}

static unsigned aml_ui_current_second(void)
{
    struct dostime_t now;

    _dos_gettime(&now);
    return now.second;
}

static void aml_ui_draw_frame(void)
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

static void aml_ui_draw_section_line(int row)
{
    int i;

    aml_ui_putc(AML_UI_FRAME_LEFT, row, 195, AML_UI_ATTR_FRAME);
    aml_ui_putc(AML_UI_FRAME_RIGHT, row, 180, AML_UI_ATTR_FRAME);
    for (i = AML_UI_FRAME_LEFT + 1; i < AML_UI_FRAME_RIGHT; ++i) {
        aml_ui_putc(i, row, 196, AML_UI_ATTR_FRAME);
    }
}

static void aml_ui_draw_header(void)
{
    struct dostime_t now;

    _dos_gettime(&now);

    aml_ui_write_at(2, 1, "Arvutimuuseum Launcher v2 (c) Danila Sukharev", AML_UI_ATTR_TITLE);
    aml_ui_write_2digit_at(72, 1, now.hour, AML_UI_ATTR_HELP);
    aml_ui_putc(74, 1, (now.second & 1) ? ':' : ' ', AML_UI_ATTR_HELP);
    aml_ui_write_2digit_at(75, 1, now.minute, AML_UI_ATTR_HELP);
}

static void aml_ui_draw_modmark(const AmlState *state)
{
    if (state->modified) {
        aml_ui_putc(36, 1, '*', AML_UI_ATTR_HELP);
    }
}

static int aml_ui_hotkey_index(int key)
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

static char aml_ui_hotkey_char(int index)
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

static int aml_ui_name_starts_with(const char *name, const char *prefix)
{
    while (*prefix != '\0') {
        if (tolower((unsigned char)*name) != tolower((unsigned char)*prefix)) {
            return 0;
        }
        name++;
        prefix++;
    }
    return 1;
}

static int aml_ui_find_prefix(const AmlState *state, const char *prefix)
{
    int i;

    if (prefix[0] == '\0') {
        return -1;
    }

    for (i = 0; i < state->entry_count; ++i) {
        if (aml_ui_name_starts_with(state->entries[i].name, prefix)) {
            return i;
        }
    }

    return -1;
}

static int aml_ui_first_visible(const AmlState *state)
{
    int top;

    if (state->entry_count <= AML_UI_LIST_ROWS) {
        return 0;
    }

    top = state->selected - (AML_UI_LIST_ROWS / 2);
    if (top < 0) {
        top = 0;
    }
    if (top > state->entry_count - AML_UI_LIST_ROWS) {
        top = state->entry_count - AML_UI_LIST_ROWS;
    }

    return top;
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
    int top = aml_ui_first_visible(state);

    aml_ui_fill_rect(AML_UI_LIST_LEFT, AML_UI_LIST_ROW, AML_UI_LIST_RIGHT,
                     AML_UI_LIST_ROW + AML_UI_LIST_ROWS - 1, ' ', AML_UI_ATTR_TEXT);

    if (state->entry_count <= 0) {
        aml_ui_write_at(6, 10, "No entries available.", AML_UI_ATTR_DIALOG_TEXT);
        aml_ui_write_at(6, 12, "Check LAUNCHER.CFG or press Esc to exit.", AML_UI_ATTR_DIALOG_DIM);
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
        aml_ui_write_padded(11, y, state->entries[index].name, AML_UI_SCROLL_COL - 11, attr);
    }

    aml_ui_draw_scrollbar(state);
}

static void aml_ui_render(const AmlState *state, const char *status)
{
    (void)status;
    aml_ui_fill_rect(0, 0, AML_UI_COLS - 1, AML_UI_ROWS - 1, ' ', AML_UI_ATTR_BG);
    aml_ui_draw_frame();
    aml_ui_draw_section_line(2);
    aml_ui_draw_header();
    aml_ui_draw_modmark(state);
    aml_ui_draw_entries(state);
}

static void aml_ui_draw_detail_line(int row, const char *label, const char *value, unsigned char attr)
{
    aml_ui_write_at(18, row, label, AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_padded(28, row, value, 34, attr);
}

static void aml_ui_show_details_overlay(const AmlState *state)
{
    char hotkey[2];
    const AmlEntry *entry;

    if (state->entry_count <= 0 ||
        state->selected < 0 ||
        state->selected >= state->entry_count) {
        aml_ui_show_message(
            "No current entry",
            "There is nothing to show yet.",
            "Use Ins to add a new record.",
            "Press any key to return."
        );
        getch();
        return;
    }

    entry = &state->entries[state->selected];
    hotkey[0] = aml_ui_hotkey_char(state->selected);
    hotkey[1] = '\0';

    aml_ui_render(state, "Details");
    aml_ui_fill_rect(10, 6, 69, 18, ' ', AML_UI_ATTR_DIALOG);
    aml_ui_putc(10, 6, 218, AML_UI_ATTR_FRAME);
    aml_ui_putc(69, 6, 191, AML_UI_ATTR_FRAME);
    aml_ui_putc(10, 18, 192, AML_UI_ATTR_FRAME);
    aml_ui_putc(69, 18, 217, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(11, 6, 68, 6, 196, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(11, 18, 68, 18, 196, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(10, 7, 10, 17, 179, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(69, 7, 69, 17, 179, AML_UI_ATTR_FRAME);
    aml_ui_write_centered(8, "Entry Details", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_draw_detail_line(10, "Name", entry->name, AML_UI_ATTR_DIALOG_DIM);
    aml_ui_draw_detail_line(12, "Command", entry->command, AML_UI_ATTR_DIALOG_DIM);
    aml_ui_draw_detail_line(14, "Path", entry->path[0] != '\0' ? entry->path : ".", AML_UI_ATTR_DIALOG_DIM);
    aml_ui_draw_detail_line(16, "Hotkey", hotkey[0] != ' ' ? hotkey : "-", AML_UI_ATTR_DIALOG_DIM);
    aml_ui_write_centered(19, "Press any key to return", AML_UI_ATTR_HELP);
    aml_ui_flush();
    getch();
}

static int aml_ui_edit_field(int key, char *buf, int max_len, int *len, int *cursor)
{
    if (key == AML_KEY_BACKSPACE) {
        if (*cursor > 0 && *len > 0) {
            memmove(&buf[*cursor - 1], &buf[*cursor], (size_t)(*len - *cursor + 1));
            (*len)--;
            (*cursor)--;
        }
        return 1;
    }
    if (key == AML_KEY_DEL) {
        if (*cursor < *len) {
            memmove(&buf[*cursor], &buf[*cursor + 1], (size_t)(*len - *cursor));
            (*len)--;
        }
        return 1;
    }
    if (key == AML_KEY_LEFT) {
        if (*cursor > 0) {
            (*cursor)--;
        }
        return 1;
    }
    if (key == AML_KEY_RIGHT) {
        if (*cursor < *len) {
            (*cursor)++;
        }
        return 1;
    }
    if (key == AML_KEY_HOME) {
        *cursor = 0;
        return 1;
    }
    if (key == AML_KEY_END) {
        *cursor = *len;
        return 1;
    }
    if (isprint(key) && *len < max_len - 1) {
        memmove(&buf[*cursor + 1], &buf[*cursor], (size_t)(*len - *cursor + 1));
        buf[*cursor] = (char)key;
        (*len)++;
        (*cursor)++;
        return 1;
    }
    return 0;
}

static int aml_ui_prompt_entry(AmlEntry *entry, int is_new)
{
    char name[AML_MAX_NAME];
    char command[AML_MAX_COMMAND];
    char path[AML_MAX_PATH];
    int field = 0;
    int name_len;
    int command_len;
    int path_len;
    int name_cursor;
    int command_cursor;
    int path_cursor;

    strcpy(name, is_new ? "" : entry->name);
    strcpy(command, is_new ? "" : entry->command);
    strcpy(path, is_new ? "" : entry->path);
    name_len = strlen(name);
    command_len = strlen(command);
    path_len = strlen(path);
    name_cursor = name_len;
    command_cursor = command_len;
    path_cursor = path_len;

    for (;;) {
        int key;
        int handled = 0;
        int *len_ptr = &name_len;
        int *cursor_ptr = &name_cursor;
        char *buf = name;
        int max_len = AML_MAX_NAME;
        int cursor_col = 24 + name_cursor;
        int cursor_row = 10;

        aml_ui_fill_rect(8, 5, 71, 18, ' ', AML_UI_ATTR_DIALOG);
        aml_ui_putc(8, 5, 218, AML_UI_ATTR_FRAME);
        aml_ui_putc(71, 5, 191, AML_UI_ATTR_FRAME);
        aml_ui_putc(8, 18, 192, AML_UI_ATTR_FRAME);
        aml_ui_putc(71, 18, 217, AML_UI_ATTR_FRAME);
        aml_ui_fill_rect(9, 5, 70, 5, 196, AML_UI_ATTR_FRAME);
        aml_ui_fill_rect(9, 18, 70, 18, 196, AML_UI_ATTR_FRAME);
        aml_ui_fill_rect(8, 6, 8, 17, 179, AML_UI_ATTR_FRAME);
        aml_ui_fill_rect(71, 6, 71, 17, 179, AML_UI_ATTR_FRAME);
        aml_ui_write_centered(7, is_new ? "Insert Entry" : "Edit Entry", AML_UI_ATTR_DIALOG_TEXT);
        aml_ui_write_at(12, 10, "Name", AML_UI_ATTR_DIALOG_TEXT);
        aml_ui_write_at(12, 12, "Command", AML_UI_ATTR_DIALOG_TEXT);
        aml_ui_write_at(12, 14, "Path", AML_UI_ATTR_DIALOG_TEXT);
        aml_ui_write_padded(24, 10, name, 40, field == 0 ? AML_UI_ATTR_SELECTED : AML_UI_ATTR_DIALOG_DIM);
        aml_ui_write_padded(24, 12, command, 40, field == 1 ? AML_UI_ATTR_SELECTED : AML_UI_ATTR_DIALOG_DIM);
        aml_ui_write_padded(24, 14, path, 40, field == 2 ? AML_UI_ATTR_SELECTED : AML_UI_ATTR_DIALOG_DIM);
        aml_ui_write_centered(16, "Enter next/save  Tab switch  Esc cancel", AML_UI_ATTR_HELP);
        aml_ui_flush();
        if (field == 1) {
            cursor_col = 24 + command_cursor;
            cursor_row = 12;
        } else if (field == 2) {
            cursor_col = 24 + path_cursor;
            cursor_row = 14;
        }
        aml_ui_set_cursor(cursor_col, cursor_row);
        aml_ui_show_cursor();

        key = getch();
        if (key == AML_KEY_ESC) {
            aml_ui_hide_cursor();
            return 0;
        }
        if (key == '\t') {
            field = (field + 1) % 3;
            continue;
        }
        if (key == AML_KEY_ENTER) {
            if (field < 2) {
                field++;
                continue;
            }
            if (name[0] == '\0' || command[0] == '\0') {
                aml_ui_show_message(
                    "Name and command required",
                    "Both fields must be filled in.",
                    "",
                    "Press any key to continue."
                );
                getch();
                continue;
            }
            strcpy(entry->name, name);
            strcpy(entry->command, command);
            strcpy(entry->path, path);
            aml_ui_hide_cursor();
            return 1;
        }
        if (key == AML_KEY_EXTENDED || key == AML_KEY_EXTENDED_2) {
            key = getch();
            if (key == AML_KEY_UP) {
                field = (field + 2) % 3;
            } else if (key == AML_KEY_DOWN) {
                field = (field + 1) % 3;
            } else if (field == 0 || field == 1 || field == 2) {
                if (field == 1) {
                    len_ptr = &command_len;
                    cursor_ptr = &command_cursor;
                    buf = command;
                    max_len = AML_MAX_COMMAND;
                } else if (field == 2) {
                    len_ptr = &path_len;
                    cursor_ptr = &path_cursor;
                    buf = path;
                    max_len = AML_MAX_PATH;
                }
                aml_ui_edit_field(key, buf, max_len, len_ptr, cursor_ptr);
            }
            continue;
        }

        if (field == 1) {
            len_ptr = &command_len;
            cursor_ptr = &command_cursor;
            buf = command;
            max_len = AML_MAX_COMMAND;
        } else if (field == 2) {
            len_ptr = &path_len;
            cursor_ptr = &path_cursor;
            buf = path;
            max_len = AML_MAX_PATH;
        }

        handled = aml_ui_edit_field(key, buf, max_len, len_ptr, cursor_ptr);
        if (!handled && key == AML_KEY_QUESTION) {
            buf = buf;
        }
    }
}

static void aml_ui_insert_entry(AmlState *state)
{
    int index;
    int i;
    AmlEntry entry;

    if (state->entry_count >= AML_MAX_PROGRAMS) {
        aml_ui_show_message(
            "List is full",
            "Maximum entry count reached.",
            "Delete something before inserting.",
            "Press any key to continue."
        );
        getch();
        return;
    }

    entry.name[0] = '\0';
    entry.command[0] = '\0';
    entry.path[0] = '\0';
    if (!aml_ui_prompt_entry(&entry, 1)) {
        return;
    }

    index = state->entry_count > 0 ? state->selected + 1 : 0;
    if (index > state->entry_count) {
        index = state->entry_count;
    }
    for (i = state->entry_count; i > index; --i) {
        state->entries[i] = state->entries[i - 1];
    }
    state->entries[index] = entry;
    state->entry_count++;
    state->selected = index;
    state->modified = 1;
}

static void aml_ui_edit_entry(AmlState *state)
{
    if (state->entry_count <= 0 ||
        state->selected < 0 ||
        state->selected >= state->entry_count) {
        aml_ui_insert_entry(state);
        return;
    }

    if (aml_ui_prompt_entry(&state->entries[state->selected], 0)) {
        state->modified = 1;
    }
}

static void aml_ui_show_help_overlay(const AmlState *state)
{
    char hotkey[2];
    const AmlEntry *entry = 0;

    if (state->entry_count > 0 &&
        state->selected >= 0 &&
        state->selected < state->entry_count) {
        entry = &state->entries[state->selected];
    }

    hotkey[0] = aml_ui_hotkey_char(state->selected);
    hotkey[1] = '\0';

    aml_ui_render(state, "Help");
    aml_ui_fill_rect(10, 6, 69, 18, ' ', AML_UI_ATTR_DIALOG);

    aml_ui_putc(10, 6, 218, AML_UI_ATTR_FRAME);
    aml_ui_putc(69, 6, 191, AML_UI_ATTR_FRAME);
    aml_ui_putc(10, 18, 192, AML_UI_ATTR_FRAME);
    aml_ui_putc(69, 18, 217, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(11, 6, 68, 6, 196, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(11, 18, 68, 18, 196, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(10, 7, 10, 17, 179, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(69, 7, 69, 17, 179, AML_UI_ATTR_FRAME);

    aml_ui_write_centered(8, "Launcher Help", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(14, 10, "Enter  Launch selected item", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(14, 11, "/      Prefix search by item name", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(14, 12, "F2     Save configuration", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(14, 13, "F3     Show current entry details", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(14, 14, "F4     Edit current entry", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(14, 15, "Ins    Insert a new entry", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(14, 16, "0-9 a-z A-Z  Direct hotkeys for items 1-62", AML_UI_ATTR_DIALOG_TEXT);

    if (entry != 0) {
        aml_ui_draw_detail_line(17, "Current key", hotkey[0] != ' ' ? hotkey : "-", AML_UI_ATTR_DIALOG_DIM);
    }

    aml_ui_write_centered(19, "Press any key to return", AML_UI_ATTR_HELP);
    aml_ui_flush();
    getch();
}

void aml_ui_show_message(const char *title, const char *line1, const char *line2, const char *line3)
{
    aml_ui_hide_cursor();
    aml_ui_fill_rect(0, 0, AML_UI_COLS - 1, AML_UI_ROWS - 1, ' ', AML_UI_ATTR_BG);
    aml_ui_draw_frame();
    aml_ui_draw_section_line(4);
    aml_ui_draw_header();
    aml_ui_fill_rect(12, 8, 67, 16, ' ', AML_UI_ATTR_DIALOG);

    aml_ui_putc(12, 8, 218, AML_UI_ATTR_FRAME);
    aml_ui_putc(67, 8, 191, AML_UI_ATTR_FRAME);
    aml_ui_putc(12, 16, 192, AML_UI_ATTR_FRAME);
    aml_ui_putc(67, 16, 217, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(13, 8, 66, 8, 196, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(13, 16, 66, 16, 196, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(12, 9, 12, 15, 179, AML_UI_ATTR_FRAME);
    aml_ui_fill_rect(67, 9, 67, 15, 179, AML_UI_ATTR_FRAME);

    if (title != NULL && title[0] != '\0') {
        aml_ui_write_centered(10, title, AML_UI_ATTR_DIALOG_TEXT);
    }
    if (line1 != NULL && line1[0] != '\0') {
        aml_ui_write_centered(12, line1, AML_UI_ATTR_DIALOG_TEXT);
    }
    if (line2 != NULL && line2[0] != '\0') {
        aml_ui_write_centered(13, line2, AML_UI_ATTR_DIALOG_DIM);
    }
    if (line3 != NULL && line3[0] != '\0') {
        aml_ui_write_centered(15, line3, AML_UI_ATTR_HELP);
    }

    aml_ui_flush();
}

static void aml_ui_trim_newline(char *line)
{
    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[len - 1] = '\0';
        len--;
    }
}

#if AML_TEST_HOOKS
static int aml_ui_read_auto_line(char *line, unsigned line_size)
{
    FILE *fp;
    char current[AML_MAX_LINE + 1];
    char next[AML_MAX_LINE + 1];
    int found = 0;
    int have_next = 0;

    fp = fopen(AML_AUTO_FILE, "r");
    if (fp == NULL) {
        return 0;
    }

    line[0] = '\0';
    next[0] = '\0';

    while (fgets(current, sizeof(current), fp) != NULL) {
        aml_ui_trim_newline(current);
        if (!found && current[0] != '\0') {
            strncpy(line, current, line_size - 1);
            line[line_size - 1] = '\0';
            found = 1;
            continue;
        }
        if (found && current[0] != '\0') {
            strncpy(next, current, sizeof(next) - 1);
            next[sizeof(next) - 1] = '\0';
            have_next = 1;
            break;
        }
    }
    fclose(fp);

    if (!found) {
        remove(AML_AUTO_FILE);
        return 0;
    }

    if (!have_next) {
        remove(AML_AUTO_FILE);
    } else {
        fp = fopen(AML_AUTO_FILE, "w");
        if (fp != NULL) {
            fputs(next, fp);
            fputc('\n', fp);
            fclose(fp);
        }
    }

    return 1;
}

static void aml_ui_trace_event(const char *event_name)
{
    FILE *fp;

    fp = fopen(AML_TRACE_FILE, "a");
    if (fp == NULL) {
        return;
    }
    fputs(event_name, fp);
    fputc('\n', fp);
    fclose(fp);
}

static int aml_ui_apply_automation(AmlState *state)
{
    char line[AML_MAX_LINE + 1];

    if (!aml_ui_read_auto_line(line, sizeof(line))) {
        return AML_UI_AUTO_NONE;
    }

    if (strncmp(line, "launch ", 7) == 0) {
        int index = atoi(line + 7);
        if (index >= 0 && index < state->entry_count) {
            state->selected = index;
            aml_ui_trace_event("auto_launch");
            return AML_UI_LAUNCH;
        }
        aml_ui_trace_event("auto_bad_launch");
        return AML_UI_QUIT;
    }

    if (strcmp(line, "up") == 0 && state->entry_count > 0) {
        if (state->selected > 0) {
            state->selected--;
        } else {
            state->selected = state->entry_count - 1;
        }
        aml_ui_trace_event("auto_up");
        return AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "down") == 0 && state->entry_count > 0) {
        if (state->selected < state->entry_count - 1) {
            state->selected++;
        } else {
            state->selected = 0;
        }
        aml_ui_trace_event("auto_down");
        return AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "home") == 0 && state->entry_count > 0) {
        state->selected = 0;
        aml_ui_trace_event("auto_home");
        return AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "end") == 0 && state->entry_count > 0) {
        state->selected = state->entry_count - 1;
        aml_ui_trace_event("auto_end");
        return AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "pgup") == 0 && state->entry_count > 0) {
        state->selected -= AML_UI_LIST_ROWS;
        if (state->selected < 0) {
            state->selected = 0;
        }
        aml_ui_trace_event("auto_pgup");
        return AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "pgdn") == 0 && state->entry_count > 0) {
        state->selected += AML_UI_LIST_ROWS;
        if (state->selected >= state->entry_count) {
            state->selected = state->entry_count - 1;
        }
        aml_ui_trace_event("auto_pgdn");
        return AML_UI_AUTO_REDRAW;
    }

    if (strncmp(line, "search ", 7) == 0) {
        int index = aml_ui_find_prefix(state, line + 7);
        if (index >= 0) {
            state->selected = index;
            aml_ui_trace_event("auto_search");
        } else {
            aml_ui_trace_event("auto_bad_search");
        }
        return AML_UI_AUTO_REDRAW;
    }

    if (strncmp(line, "hotkey ", 7) == 0) {
        int index = aml_ui_hotkey_index(line[7]);
        if (index >= 0 && index < state->entry_count) {
            state->selected = index;
            aml_ui_trace_event("auto_hotkey");
        } else {
            aml_ui_trace_event("auto_bad_hotkey");
        }
        return AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "quit") == 0) {
        aml_ui_trace_event("auto_quit");
        return AML_UI_QUIT;
    }

    aml_ui_trace_event("auto_unknown");
    return AML_UI_QUIT;
}
#endif

static int aml_ui_prompt_search(AmlState *state, const char **status)
{
    char query[AML_UI_SEARCH_MAX + 1];
    int len = 0;

    query[0] = '\0';

    for (;;) {
        int key;
        int match;

        aml_ui_render(state, *status);
        aml_ui_fill_rect(3, 22, 76, 22, ' ', AML_UI_ATTR_STATUS);
        aml_ui_write_at(3, 22, "Find:", AML_UI_ATTR_MUTED);
        aml_ui_write_padded(9, 22, query, AML_UI_SEARCH_MAX, AML_UI_ATTR_STATUS);
        aml_ui_flush();

        key = getch();
        if (key == AML_KEY_ESC) {
            *status = "Search cancelled";
            return 0;
        }
        if (key == AML_KEY_ENTER) {
            match = aml_ui_find_prefix(state, query);
            if (match >= 0) {
                state->selected = match;
                *status = "Search matched";
            } else if (len == 0) {
                *status = "Select a program";
            } else {
                *status = "No matching entry";
            }
            return 0;
        }
        if (key == AML_KEY_BACKSPACE) {
            if (len > 0) {
                query[--len] = '\0';
            }
        } else if (isprint(key) && len < AML_UI_SEARCH_MAX) {
            query[len++] = (char)key;
            query[len] = '\0';
        } else {
            continue;
        }

        match = aml_ui_find_prefix(state, query);
        if (match >= 0) {
            state->selected = match;
            *status = "Search matched";
        } else if (len == 0) {
            *status = "Search cleared";
        } else {
            *status = "No matching entry";
        }
    }
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

int aml_ui_run(AmlState *state)
{
    const char *status = "";
    unsigned last_second = 60;

    for (;;) {
        int key;
        int hotkey_index;

        aml_ui_draw(state, status);
        last_second = aml_ui_current_second();
#if AML_TEST_HOOKS
        sleep(1);
        {
            int auto_action = aml_ui_apply_automation(state);
            if (auto_action == AML_UI_AUTO_REDRAW) {
                continue;
            }
            if (auto_action >= 0) {
                return auto_action;
            }
        }
#endif
        while (!kbhit()) {
            unsigned now_second = aml_ui_current_second();

            if (now_second != last_second) {
                aml_ui_draw(state, status);
                last_second = now_second;
            }
            delay(50);
        }
        key = getch();

        if (key == AML_KEY_ENTER) {
            if (state->entry_count > 0) {
                return AML_UI_LAUNCH;
            }
            status = "No launcher entries available";
            continue;
        }

        if (key == AML_KEY_ESC) {
            return AML_UI_QUIT;
        }

        if (key == AML_KEY_SLASH && state->entry_count > 0) {
            aml_ui_prompt_search(state, &status);
            continue;
        }

        if (key == AML_KEY_EXTENDED || key == AML_KEY_EXTENDED_2) {
            key = getch();

            if (key == AML_KEY_F1) {
                aml_ui_show_help_overlay(state);
            } else if (key == AML_KEY_F2) {
                return AML_UI_SAVE;
            } else if (key == AML_KEY_F3) {
                aml_ui_show_details_overlay(state);
            } else if (key == AML_KEY_F4) {
                aml_ui_edit_entry(state);
            } else if (key == AML_KEY_INS) {
                aml_ui_insert_entry(state);
            } else if (state->entry_count <= 0) {
                continue;
            } else if (key == AML_KEY_HOME) {
                state->selected = 0;
            } else if (key == AML_KEY_END) {
                state->selected = state->entry_count - 1;
            } else if (key == AML_KEY_PGUP) {
                state->selected -= AML_UI_LIST_ROWS;
                if (state->selected < 0) {
                    state->selected = 0;
                }
            } else if (key == AML_KEY_PGDN) {
                state->selected += AML_UI_LIST_ROWS;
                if (state->selected >= state->entry_count) {
                    state->selected = state->entry_count - 1;
                }
            } else if (key == AML_KEY_UP) {
                if (state->selected > 0) {
                    state->selected--;
                } else {
                    state->selected = state->entry_count - 1;
                }
            } else if (key == AML_KEY_DOWN) {
                if (state->selected < state->entry_count - 1) {
                    state->selected++;
                } else {
                    state->selected = 0;
                }
            }

            status = "";
            continue;
        }

        if (key == AML_KEY_QUESTION) {
            aml_ui_show_help_overlay(state);
            status = "";
            continue;
        }

        hotkey_index = aml_ui_hotkey_index(key);
        if (hotkey_index >= 0 && hotkey_index < state->entry_count) {
            state->selected = hotkey_index;
            return AML_UI_LAUNCH;
        }

        if (isprint(key)) {
            status = "Unknown key";
        } else {
            status = "";
        }
    }
}
