#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <i86.h>
#include <string.h>

#include "ui_int.h"

static unsigned short ui_backbuf[UI_ROWS * UI_COLS];

extern void ui_blit_row(const unsigned short *src, unsigned short far *dst);
#pragma aux ui_blit_row parm [si] [es di] modify [cx si di];

enum {
    UI_VSYNC_TIMEOUT = 0x8000
};

static unsigned short cell(unsigned char ch, unsigned char attr)
{
    return (unsigned short)ch | ((unsigned short)attr << 8);
}

static void ui_set_text_mode_80x25(void)
{
    union REGS regs;

    regs.w.ax = 0x0003;
    int86(0x10, &regs, &regs);
}

void ui_hide_cursor(void)
{
    union REGS regs;

    regs.h.ah = 0x01;
    regs.x.cx = 0x2000;
    int86(0x10, &regs, &regs);
}

void ui_show_cursor(void)
{
    union REGS regs;

    regs.h.ah = 0x01;
    regs.x.cx = 0x0607;
    int86(0x10, &regs, &regs);
}

void ui_set_cursor(int col, int row)
{
    union REGS regs;

    if (col < 0) {
        col = 0;
    }
    if (col >= UI_COLS) {
        col = UI_COLS - 1;
    }
    if (row < 0) {
        row = 0;
    }
    if (row >= UI_ROWS) {
        row = UI_ROWS - 1;
    }

    regs.h.ah = 0x02;
    regs.h.bh = 0;
    regs.h.dh = (unsigned char)row;
    regs.h.dl = (unsigned char)col;
    int86(0x10, &regs, &regs);
}

static void wait_vsync(void)
{
    unsigned timeout = UI_VSYNC_TIMEOUT;

    while ((inp(0x3DA) & 0x08) != 0 && timeout-- != 0) {
    }
    timeout = UI_VSYNC_TIMEOUT;
    while ((inp(0x3DA) & 0x08) == 0 && timeout-- != 0) {
    }
}

static void ui_flush_rows_impl(int top, int bottom, int sync)
{
    unsigned short far *video = (unsigned short far *)MK_FP(0xB800, 0);
    int row;

    if (top < 0) {
        top = 0;
    }
    if (bottom >= UI_ROWS) {
        bottom = UI_ROWS - 1;
    }
    if (top > bottom) {
        return;
    }

    if (sync) {
        wait_vsync();
    }
    for (row = top; row <= bottom; ++row) {
        unsigned offset = (unsigned)row * UI_COLS;
        ui_blit_row(ui_backbuf + offset, video + offset);
    }
}

static void ui_flush_rows(int top, int bottom)
{
    ui_flush_rows_impl(top, bottom, 1);
}

static void ui_flush_rows_nosync(int top, int bottom)
{
    ui_flush_rows_impl(top, bottom, 0);
}

void ui_flush(void)
{
    ui_flush_rows(0, UI_ROWS - 1);
}

void ui_fill_rect(int left, int top, int right, int bottom,
                  unsigned char ch, unsigned char attr)
{
    int row;
    int col;
    unsigned short fill = cell(ch, attr);

    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right >= UI_COLS) {
        right = UI_COLS - 1;
    }
    if (bottom >= UI_ROWS) {
        bottom = UI_ROWS - 1;
    }

    for (row = top; row <= bottom; ++row) {
        for (col = left; col <= right; ++col) {
            ui_backbuf[row * UI_COLS + col] = fill;
        }
    }
}

void ui_putc(int col, int row, unsigned char ch, unsigned char attr)
{
    if (col < 0 || col >= UI_COLS || row < 0 || row >= UI_ROWS) {
        return;
    }
    ui_backbuf[row * UI_COLS + col] = cell(ch, attr);
}

void ui_write_at(int col, int row, const char *text, unsigned char attr)
{
    while (*text != '\0' && col < UI_COLS) {
        ui_putc(col, row, (unsigned char)*text, attr);
        col++;
        text++;
    }
}

void ui_write_padded(int col, int row, const char *text, int width, unsigned char attr)
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
        ui_putc(col + i, row, ch, attr);
    }
}

int ui_visible_start(const char *text, int width, int start, int cursor)
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

void ui_write_clipped(int col, int row, const char *text, int width,
                      int start, int cursor, unsigned char attr)
{
    int len = (int)strlen(text);
    int i;
    int visible_start = ui_visible_start(text, width, start, cursor);
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
        ui_putc(col + i, row, ch, attr);
    }

    if (visible_start > 0 && width >= 3) {
        ui_putc(col, row, '.', attr);
        ui_putc(col + 1, row, '.', attr);
        ui_putc(col + 2, row, '.', attr);
    }
    if (visible_end < len && width >= 3) {
        ui_putc(col + width - 3, row, '.', attr);
        ui_putc(col + width - 2, row, '.', attr);
        ui_putc(col + width - 1, row, '.', attr);
    }
}

void ui_write_ellipsis(int col, int row, const char *text, int width, unsigned char attr)
{
    int len = (int)strlen(text);

    if (len <= width) {
        ui_write_padded(col, row, text, width, attr);
        return;
    }

    ui_write_padded(col, row, text, width, attr);
    if (width >= 3) {
        ui_putc(col + width - 3, row, '.', attr);
        ui_putc(col + width - 2, row, '.', attr);
        ui_putc(col + width - 1, row, '.', attr);
    }
}

void ui_write_centered(int row, const char *text, unsigned char attr)
{
    int col = (UI_COLS - (int)strlen(text)) / 2;

    if (col < 1) {
        col = 1;
    }
    ui_write_at(col, row, text, attr);
}

void ui_write_2digit_at(int col, int row, unsigned value, unsigned char attr)
{
    ui_putc(col, row, (unsigned char)('0' + ((value / 10) % 10)), attr);
    ui_putc(col + 1, row, (unsigned char)('0' + (value % 10)), attr);
}

unsigned ui_current_second(void)
{
    struct dostime_t now;

    _dos_gettime(&now);
    return now.second;
}

void ui_draw_frame(void)
{
    int i;

    ui_putc(UI_FRAME_LEFT, UI_FRAME_TOP, 218, UI_ATTR_FRAME);
    ui_putc(UI_FRAME_RIGHT, UI_FRAME_TOP, 191, UI_ATTR_FRAME);
    ui_putc(UI_FRAME_LEFT, UI_FRAME_BOTTOM, 192, UI_ATTR_FRAME);
    ui_putc(UI_FRAME_RIGHT, UI_FRAME_BOTTOM, 217, UI_ATTR_FRAME);

    for (i = UI_FRAME_LEFT + 1; i < UI_FRAME_RIGHT; ++i) {
        ui_putc(i, UI_FRAME_TOP, 196, UI_ATTR_FRAME);
        ui_putc(i, UI_FRAME_BOTTOM, 196, UI_ATTR_FRAME);
    }

    for (i = UI_FRAME_TOP + 1; i < UI_FRAME_BOTTOM; ++i) {
        ui_putc(UI_FRAME_LEFT, i, 179, UI_ATTR_FRAME);
        ui_putc(UI_FRAME_RIGHT, i, 179, UI_ATTR_FRAME);
    }
}

static void apply_shadow(int left, int top, int right, int bottom)
{
    int row;
    int col;

    for (row = top + 1; row <= bottom + 1 && row < UI_ROWS; ++row) {
        for (col = right + 1; col <= right + 2 && col < UI_COLS; ++col) {
            unsigned short saved = ui_backbuf[row * UI_COLS + col];
            ui_backbuf[row * UI_COLS + col] =
                cell((unsigned char)(saved & 0x00FF), UI_ATTR_SHADOW);
        }
    }

    row = bottom + 1;
    if (row < UI_ROWS) {
        for (col = left + 2; col <= right + 2 && col < UI_COLS; ++col) {
            unsigned short saved = ui_backbuf[row * UI_COLS + col];
            ui_backbuf[row * UI_COLS + col] =
                cell((unsigned char)(saved & 0x00FF), UI_ATTR_SHADOW);
        }
    }
}

static void draw_dialog_box(int left, int top, int right, int bottom, const char *title)
{
    int title_len;
    int title_col;
    int i;

    apply_shadow(left, top, right, bottom);
    ui_fill_rect(left, top, right, bottom, ' ', UI_ATTR_DIALOG);
    ui_putc(left, top, 218, UI_ATTR_FRAME);
    ui_putc(right, top, 191, UI_ATTR_FRAME);
    ui_putc(left, bottom, 192, UI_ATTR_FRAME);
    ui_putc(right, bottom, 217, UI_ATTR_FRAME);
    ui_fill_rect(left + 1, top, right - 1, top, 196, UI_ATTR_FRAME);
    ui_fill_rect(left + 1, bottom, right - 1, bottom, 196, UI_ATTR_FRAME);
    ui_fill_rect(left, top + 1, left, bottom - 1, 179, UI_ATTR_FRAME);
    ui_fill_rect(right, top + 1, right, bottom - 1, 179, UI_ATTR_FRAME);

    if (title != NULL && title[0] != '\0') {
        title_len = (int)strlen(title);
        if (title_len > right - left - 4) {
            title_len = right - left - 4;
        }
        title_col = left + ((right - left + 1 - title_len) / 2);
        if (title_col < left + 2) {
            title_col = left + 2;
        }

        ui_putc(title_col - 1, top, ' ', UI_ATTR_DIALOG);
        for (i = 0; i < title_len; ++i) {
            ui_putc(title_col + i, top, (unsigned char)title[i], UI_ATTR_DIALOG_TEXT);
        }
        ui_putc(title_col + title_len, top, ' ', UI_ATTR_DIALOG);
    }
}

static int dialog_row(int top, int inner_row)
{
    return top + 1 + inner_row;
}

UiDialogBox ui_dialog_box(int left, int top, int right, int bottom, const char *title)
{
    UiDialogBox box;

    box.left = left;
    box.top = top;
    box.right = right;
    box.bottom = bottom;
    box.title = title;
    return box;
}

static UiNoticeDialog notice_dialog(UiDialogBox box, const char *line1, const char *line2, const char *line3)
{
    UiNoticeDialog dialog;

    dialog.box = box;
    dialog.line1 = line1;
    dialog.line2 = line2;
    dialog.line3 = line3;
    return dialog;
}

UiConfirmDialog ui_confirm_dialog(UiDialogBox box, const char *question, const char *value,
                                  int value_col, int value_width, const char *footer)
{
    UiConfirmDialog dialog;

    dialog.box = box;
    dialog.question = question;
    dialog.value = value;
    dialog.value_col = value_col;
    dialog.value_width = value_width;
    dialog.footer = footer;
    return dialog;
}

UiDetailDialogRow ui_detail_dialog_row(const char *label, const char *value)
{
    UiDetailDialogRow row;

    row.label = label;
    row.value = value;
    return row;
}

UiDetailDialog ui_detail_dialog_spec(UiDialogBox box, const UiDetailDialogRow *rows,
                                     int row_count, int first_row, int row_step,
                                     unsigned char value_attr)
{
    UiDetailDialog dialog;

    dialog.box = box;
    dialog.rows = rows;
    dialog.row_count = row_count;
    dialog.first_row = first_row;
    dialog.row_step = row_step;
    dialog.value_attr = value_attr;
    return dialog;
}

UiTextLineDialog ui_text_line_dialog(UiDialogBox box, int col,
                                     const UiTextLineDialogLine *lines, int line_count)
{
    UiTextLineDialog dialog;

    dialog.box = box;
    dialog.col = col;
    dialog.lines = lines;
    dialog.line_count = line_count;
    return dialog;
}

UiEditField ui_edit_field(int row, const char *label, const char *text,
                          int text_col, int width, int cursor, unsigned char attr)
{
    UiEditField field;

    field.row = row;
    field.label = label;
    field.text = text;
    field.text_col = text_col;
    field.width = width;
    field.cursor = cursor;
    field.attr = attr;
    return field;
}

UiEditDialog ui_edit_dialog(UiDialogBox box, int label_col,
                            const UiEditField *fields, int field_count, const char *footer)
{
    UiEditDialog dialog;

    dialog.box = box;
    dialog.label_col = label_col;
    dialog.fields = fields;
    dialog.field_count = field_count;
    dialog.footer = footer;
    return dialog;
}

UiMenuDialog ui_menu_dialog(UiDialogBox box, int prompt_row, int prompt_col,
                            const char *prompt_label, const char *prompt_value, int prompt_width,
                            int menu_row, int menu_left, int menu_right, int item_col,
                            const char *const *items, int item_count, int selected,
                            const char *footer)
{
    UiMenuDialog dialog;

    dialog.box = box;
    dialog.prompt_row = prompt_row;
    dialog.prompt_col = prompt_col;
    dialog.prompt_label = prompt_label;
    dialog.prompt_value = prompt_value;
    dialog.prompt_width = prompt_width;
    dialog.menu_row = menu_row;
    dialog.menu_left = menu_left;
    dialog.menu_right = menu_right;
    dialog.item_col = item_col;
    dialog.items = items;
    dialog.item_count = item_count;
    dialog.selected = selected;
    dialog.footer = footer;
    return dialog;
}

static void draw_notice_dialog(const UiNoticeDialog *dialog)
{
    draw_dialog_box(
        dialog->box.left, dialog->box.top, dialog->box.right, dialog->box.bottom, dialog->box.title
    );

    if (dialog->line1 != NULL && dialog->line1[0] != '\0') {
        ui_write_centered(dialog_row(dialog->box.top, 2), dialog->line1, UI_ATTR_DIALOG_TEXT);
    }
    if (dialog->line2 != NULL && dialog->line2[0] != '\0') {
        ui_write_centered(dialog_row(dialog->box.top, 3), dialog->line2, UI_ATTR_DIALOG_DIM);
    }
    if (dialog->line3 != NULL && dialog->line3[0] != '\0') {
        ui_write_centered(dialog_row(dialog->box.top, 5), dialog->line3, UI_ATTR_HELP);
    }
}

void ui_draw_confirm_dialog(const UiConfirmDialog *dialog)
{
    draw_dialog_box(
        dialog->box.left, dialog->box.top, dialog->box.right, dialog->box.bottom, dialog->box.title
    );
    ui_write_centered(dialog_row(dialog->box.top, 2), dialog->question, UI_ATTR_DIALOG_TEXT);
    ui_write_ellipsis(
        dialog->value_col, dialog_row(dialog->box.top, 3),
        dialog->value, dialog->value_width, UI_ATTR_DIALOG_DIM
    );
    ui_write_centered(dialog_row(dialog->box.top, 5), dialog->footer, UI_ATTR_HELP);
}

static void draw_detail_line(int row, const char *label, const char *value, unsigned char attr)
{
    ui_write_at(18, row, label, UI_ATTR_DIALOG_TEXT);
    ui_write_padded(28, row, value, 34, attr);
}

void ui_draw_detail_dialog(const UiDetailDialog *dialog)
{
    int i;

    draw_dialog_box(
        dialog->box.left, dialog->box.top, dialog->box.right, dialog->box.bottom, dialog->box.title
    );
    for (i = 0; i < dialog->row_count; ++i) {
        draw_detail_line(
            dialog_row(dialog->box.top, dialog->first_row + (i * dialog->row_step)),
            dialog->rows[i].label,
            dialog->rows[i].value,
            dialog->value_attr
        );
    }
}

void ui_draw_text_line_dialog(const UiTextLineDialog *dialog)
{
    int i;

    draw_dialog_box(
        dialog->box.left, dialog->box.top, dialog->box.right, dialog->box.bottom, dialog->box.title
    );
    for (i = 0; i < dialog->line_count; ++i) {
        ui_write_at(
            dialog->col,
            dialog_row(dialog->box.top, dialog->lines[i].row),
            dialog->lines[i].text,
            dialog->lines[i].attr
        );
    }
}

void ui_draw_edit_dialog(const UiEditDialog *dialog)
{
    int i;

    draw_dialog_box(
        dialog->box.left, dialog->box.top, dialog->box.right, dialog->box.bottom, dialog->box.title
    );
    for (i = 0; i < dialog->field_count; ++i) {
        ui_write_at(
            dialog->label_col,
            dialog_row(dialog->box.top, dialog->fields[i].row),
            dialog->fields[i].label,
            UI_ATTR_DIALOG_TEXT
        );
        ui_write_clipped(
            dialog->fields[i].text_col,
            dialog_row(dialog->box.top, dialog->fields[i].row),
            dialog->fields[i].text,
            dialog->fields[i].width,
            0,
            dialog->fields[i].cursor,
            dialog->fields[i].attr
        );
    }
    ui_write_centered(dialog_row(dialog->box.top, dialog->fields[dialog->field_count - 1].row + 2),
                      dialog->footer, UI_ATTR_HELP);
}

int ui_edit_dialog_cursor_col(const UiEditField *field)
{
    int visible_start = ui_visible_start(field->text, field->width, 0, field->cursor);

    return field->text_col + (field->cursor - visible_start);
}

int ui_edit_dialog_cursor_row(const UiDialogBox *box, const UiEditField *field)
{
    return dialog_row(box->top, field->row);
}

void ui_draw_menu_dialog(const UiMenuDialog *dialog)
{
    int i;

    draw_dialog_box(
        dialog->box.left, dialog->box.top, dialog->box.right, dialog->box.bottom, dialog->box.title
    );
    ui_write_at(
        dialog->prompt_col,
        dialog_row(dialog->box.top, dialog->prompt_row),
        dialog->prompt_label,
        UI_ATTR_DIALOG_TEXT
    );
    ui_write_clipped(
        dialog->prompt_col + 10,
        dialog_row(dialog->box.top, dialog->prompt_row),
        dialog->prompt_value,
        dialog->prompt_width,
        0,
        0,
        UI_ATTR_DIALOG_DIM
    );

    for (i = 0; i < dialog->item_count; ++i) {
        unsigned char attr = (i == dialog->selected) ? UI_ATTR_SELECTED : UI_ATTR_DIALOG_TEXT;
        int row = dialog_row(dialog->box.top, dialog->menu_row + i);

        ui_fill_rect(dialog->menu_left, row, dialog->menu_right, row, ' ', attr);
        ui_putc(dialog->menu_left + 2, row, (i == dialog->selected) ? 16 : 250, attr);
        ui_write_at(dialog->item_col, row, dialog->items[i], attr);
    }

    ui_write_centered(dialog_row(dialog->box.top, dialog->menu_row + dialog->item_count + 1),
                      dialog->footer, UI_ATTR_HELP);
}

void ui_wait_for_ack(void)
{
#if AML_TEST_HOOKS
    for (;;) {
        char line[AML_MAX_LINE + 1];

        if (ui_read_auto_line(line, sizeof(line))) {
            if (strcmp(line, "ack") == 0) {
                ui_trace_event("auto_ack");
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

void ui_draw_header_on_frame_common(int modified)
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

    ui_fill_rect(1, 0, UI_FRAME_RIGHT - 1, 0, 196, UI_ATTR_FRAME);
    ui_write_at(1, 0, title, UI_ATTR_TITLE);
    ui_write_2digit_at(clock_col, 0, now.hour, UI_ATTR_HELP);
    ui_putc(clock_col + 2, 0, (now.second & 1) ? ':' : ' ', UI_ATTR_HELP);
    ui_write_2digit_at(clock_col + 3, 0, now.minute, UI_ATTR_HELP);
    if (modified) {
        ui_putc(78, 0, '*', UI_ATTR_HELP);
    }
}

void ui_draw_header_on_frame(const AmlState *state)
{
    ui_draw_header_on_frame_common(state->modified);
    if (state->editor_mode) {
        ui_write_at(66, 0, "EDIT", UI_ATTR_HELP);
    }
}

int ui_hotkey_index(int key)
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

char ui_hotkey_char(int index)
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

static int name_contains(const char *name, const char *needle)
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

int ui_find_match(const AmlState *state, const char *needle)
{
    int i;

    if (needle[0] == '\0') {
        return -1;
    }

    for (i = 0; i < state->entry_count; ++i) {
        if (name_contains(state->entries[i].name, needle)) {
            return i;
        }
    }

    return -1;
}

static void clamp_view_top(AmlState *state)
{
    if (state->entry_count <= UI_LIST_VISIBLE) {
        state->view_top = 0;
        return;
    }

    if (state->view_top < 0) {
        state->view_top = 0;
    }
    if (state->view_top > state->entry_count - UI_LIST_VISIBLE) {
        state->view_top = state->entry_count - UI_LIST_VISIBLE;
    }
}

void ui_sync_view_top(AmlState *state)
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

    clamp_view_top(state);
    margin = UI_LIST_VISIBLE / 4;
    top_band = state->view_top + margin;
    bottom_band = state->view_top + UI_LIST_VISIBLE - margin - 1;

    if (state->selected < top_band) {
        state->view_top = state->selected - margin;
    } else if (state->selected > bottom_band) {
        state->view_top = state->selected - (UI_LIST_VISIBLE - margin - 1);
    }

    clamp_view_top(state);
}

static void draw_scrollbar(const AmlState *state)
{
    int row;
    int thumb_row = UI_LIST_ROW + 1;

    ui_putc(UI_SCROLL_COL, UI_LIST_ROW, 30, UI_ATTR_SCROLL);
    ui_putc(UI_SCROLL_COL, UI_LIST_ROW + UI_LIST_ROWS - 1, 31, UI_ATTR_SCROLL);

    for (row = UI_LIST_ROW + 1; row < UI_LIST_ROW + UI_LIST_ROWS - 1; ++row) {
        ui_putc(UI_SCROLL_COL, row, 176, UI_ATTR_SCROLL);
    }

    if (state->entry_count > UI_LIST_VISIBLE) {
        int track = UI_LIST_ROWS - 2;
        thumb_row = UI_LIST_ROW + 1 +
            ((state->selected * (track - 1)) / (state->entry_count - 1));
    }

    ui_putc(UI_SCROLL_COL, thumb_row, 219, UI_ATTR_SCROLL_THUMB);
}

static int scrollbar_thumb_row_for_selected(const AmlState *state, int selected)
{
    if (state->entry_count > UI_LIST_VISIBLE) {
        int track = UI_LIST_ROWS - 2;

        return UI_LIST_ROW + 1 + ((selected * (track - 1)) / (state->entry_count - 1));
    }

    return UI_LIST_ROW + 1;
}

static void draw_entry_row(const AmlState *state, int index, int y)
{
    unsigned char attr = (index == state->selected) ? UI_ATTR_SELECTED : UI_ATTR_TEXT;
    char hotkey = ui_hotkey_char(index);

    ui_fill_rect(UI_LIST_LEFT + 1, y, UI_SCROLL_COL - 1, y + 1, ' ', attr);
    ui_putc(6, y, '[', attr);
    ui_putc(7, y, (unsigned char)hotkey, attr);
    ui_putc(8, y, ']', attr);
    ui_bigtext_write_at(11, y, state->entry_view[index].big_name, attr);
}

static void draw_entries(const AmlState *state)
{
    int row;
    int index;
    int top = state->view_top;

    ui_fill_rect(UI_LIST_LEFT, UI_LIST_ROW, UI_LIST_RIGHT,
                 UI_LIST_ROW + UI_LIST_ROWS - 1, ' ', UI_ATTR_TEXT);

    if (state->entry_count <= 0) {
        ui_write_at(6, 10, "No entries available.", UI_ATTR_DIALOG_TEXT);
        ui_write_at(6, 12, "Check LAUNCHER.CFG or press F10 to exit.", UI_ATTR_DIALOG_DIM);
        draw_scrollbar(state);
        return;
    }

    ui_bigtext_enable_fancy();

    for (row = 0, index = top;
         row < UI_LIST_VISIBLE && index < state->entry_count;
         ++row, ++index) {
        int y = UI_LIST_ROW + (row * UI_LIST_ENTRY_ROWS);
        draw_entry_row(state, index, y);
    }

    draw_scrollbar(state);
}

void ui_draw_list_area(const AmlState *state)
{
    draw_entries(state);
    ui_flush_rows(UI_LIST_ROW, UI_LIST_ROW + UI_LIST_ROWS - 1);
}

void ui_draw_selection_change(const AmlState *state, int old_selected)
{
    int old_row;
    int new_row;
    int old_thumb_row;
    int new_thumb_row;
    int flush_top;
    int flush_bottom;

    if (old_selected < state->view_top ||
        old_selected >= state->view_top + UI_LIST_VISIBLE ||
        state->selected < state->view_top ||
        state->selected >= state->view_top + UI_LIST_VISIBLE) {
        ui_draw_list_area(state);
        return;
    }

    old_row = UI_LIST_ROW + ((old_selected - state->view_top) * UI_LIST_ENTRY_ROWS);
    new_row = UI_LIST_ROW + ((state->selected - state->view_top) * UI_LIST_ENTRY_ROWS);
    draw_entry_row(state, old_selected, old_row);
    if (new_row != old_row) {
        draw_entry_row(state, state->selected, new_row);
    }

    old_thumb_row = scrollbar_thumb_row_for_selected(state, old_selected);
    new_thumb_row = scrollbar_thumb_row_for_selected(state, state->selected);
    ui_putc(UI_SCROLL_COL, old_thumb_row, 176, UI_ATTR_SCROLL);
    ui_putc(UI_SCROLL_COL, new_thumb_row, 219, UI_ATTR_SCROLL_THUMB);

    flush_top = old_row;
    flush_bottom = old_row + 1;
    if (new_row != old_row) {
        if (new_row < flush_top) {
            flush_top = new_row;
        }
        if (new_row + 1 > flush_bottom) {
            flush_bottom = new_row + 1;
        }
    }
    if (old_thumb_row < flush_top) {
        flush_top = old_thumb_row;
    }
    if (old_thumb_row > flush_bottom) {
        flush_bottom = old_thumb_row;
    }
    if (new_thumb_row < flush_top) {
        flush_top = new_thumb_row;
    }
    if (new_thumb_row > flush_bottom) {
        flush_bottom = new_thumb_row;
    }
    ui_flush_rows_nosync(flush_top, flush_bottom);
}

void ui_render(const AmlState *state)
{
    ui_fill_rect(0, 0, UI_COLS - 1, UI_ROWS - 1, ' ', UI_ATTR_BG);
    ui_draw_frame();
    ui_draw_header_on_frame(state);
    draw_entries(state);
}

static void draw_notice_box(const char *title, const char *line1, const char *line2, const char *line3)
{
    UiNoticeDialog dialog = notice_dialog(ui_dialog_box(12, 8, 67, 16, title), line1, line2, line3);

    draw_notice_dialog(&dialog);
}

void ui_show_message(const char *title, const char *line1, const char *line2, const char *line3)
{
    ui_hide_cursor();
    ui_fill_rect(0, 0, UI_COLS - 1, UI_ROWS - 1, ' ', UI_ATTR_BG);
    ui_draw_frame();
    ui_draw_header_on_frame_common(0);
    draw_notice_box(title, line1, line2, line3);
    ui_flush();
}

void ui_show_notice(const AmlState *state, const char *title, const char *line1, const char *line2, const char *line3)
{
    ui_hide_cursor();
    ui_render(state);
    draw_notice_box(title, line1, line2, line3);
    ui_flush();
}

void ui_update_clock(const AmlState *state)
{
    ui_draw_header_on_frame(state);
    ui_flush_rows(0, 0);
}

void ui_init(void)
{
    ui_set_text_mode_80x25();
    ui_hide_cursor();
    ui_bigtext_enable();
    ui_fill_rect(0, 0, UI_COLS - 1, UI_ROWS - 1, ' ', UI_ATTR_BG);
    ui_flush();
}

void ui_shutdown(void)
{
    ui_bigtext_disable();
    ui_set_text_mode_80x25();
    ui_show_cursor();
    ui_set_cursor(0, UI_ROWS - 1);
}

void ui_draw(const AmlState *state)
{
    ui_hide_cursor();
    ui_render(state);
    ui_flush();
}
