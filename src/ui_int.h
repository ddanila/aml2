#ifndef UI_INT_H
#define UI_INT_H

#include "aml.h"

enum {
    UI_ROWS = 25,
    UI_COLS = 80,
    UI_LIST_ROW = 2,
    UI_LIST_ROWS = 20,
    UI_LIST_VISIBLE = 10,
    UI_LIST_ENTRY_ROWS = 2,
    UI_SEARCH_MAX = 24,
    UI_FRAME_LEFT = 0,
    UI_FRAME_TOP = 0,
    UI_FRAME_RIGHT = 79,
    UI_FRAME_BOTTOM = 24,
    UI_LIST_LEFT = 2,
    UI_LIST_RIGHT = 78,
    UI_SCROLL_COL = 78,
    UI_ATTR_BG = 0x17,
    UI_ATTR_FRAME = 0x1F,
    UI_ATTR_TITLE = 0x1E,
    UI_ATTR_HELP = 0x1B,
    UI_ATTR_TEXT = 0x1F,
    UI_ATTR_MUTED = 0x19,
    UI_ATTR_STATUS = 0x1F,
    UI_ATTR_SELECTED = 0x30,
    UI_ATTR_DIALOG = 0x17,
    UI_ATTR_DIALOG_TEXT = 0x1F,
    UI_ATTR_DIALOG_DIM = 0x1E,
    UI_ATTR_SHADOW = 0x08,
    UI_ATTR_SCROLL = 0x19,
    UI_ATTR_SCROLL_THUMB = 0x3F,
    UI_KEY_ENTER = 13,
    UI_KEY_BACKSPACE = 8,
    UI_KEY_SLASH = '/',
    UI_KEY_QUESTION = '?',
    UI_KEY_ESC = 27,
    UI_KEY_EXTENDED = 0,
    UI_KEY_EXTENDED_2 = 224,
    UI_KEY_INS = 82,
    UI_KEY_F1 = 59,
    UI_KEY_F2 = 60,
    UI_KEY_F3 = 61,
    UI_KEY_F4 = 62,
    UI_KEY_F5 = 63,
    UI_KEY_F6 = 64,
    UI_KEY_F8 = 66,
    UI_KEY_F9 = 67,
    UI_KEY_F10 = 68,
    UI_KEY_HOME = 71,
    UI_KEY_LEFT = 75,
    UI_KEY_RIGHT = 77,
    UI_KEY_UP = 72,
    UI_KEY_PGUP = 73,
    UI_KEY_END = 79,
    UI_KEY_DOWN = 80,
    UI_KEY_PGDN = 81,
    UI_KEY_DEL = 83
};

enum {
    UI_AUTO_NONE = -1,
    UI_AUTO_REDRAW = -2
};

typedef struct UiDialogBox {
    int left;
    int top;
    int right;
    int bottom;
    const char *title;
} UiDialogBox;

typedef struct UiNoticeDialog {
    UiDialogBox box;
    const char *line1;
    const char *line2;
    const char *line3;
} UiNoticeDialog;

typedef struct UiConfirmDialog {
    UiDialogBox box;
    const char *question;
    const char *value;
    int value_col;
    int value_width;
    const char *footer;
} UiConfirmDialog;

typedef struct UiDetailDialogRow {
    const char *label;
    const char *value;
} UiDetailDialogRow;

typedef struct UiDetailDialog {
    UiDialogBox box;
    const UiDetailDialogRow *rows;
    int row_count;
    int first_row;
    int row_step;
    unsigned char value_attr;
} UiDetailDialog;

typedef struct UiTextLineDialogLine {
    int row;
    const char *text;
    unsigned char attr;
} UiTextLineDialogLine;

typedef struct UiTextLineDialog {
    UiDialogBox box;
    int col;
    const UiTextLineDialogLine *lines;
    int line_count;
} UiTextLineDialog;

typedef struct UiEditField {
    int row;
    const char *label;
    const char *text;
    int text_col;
    int width;
    int cursor;
    unsigned char attr;
} UiEditField;

typedef struct UiEditDialog {
    UiDialogBox box;
    int label_col;
    const UiEditField *fields;
    int field_count;
    const char *footer;
} UiEditDialog;

typedef struct UiMenuDialog {
    UiDialogBox box;
    int prompt_row;
    int prompt_col;
    const char *prompt_label;
    const char *prompt_value;
    int prompt_width;
    int menu_row;
    int menu_left;
    int menu_right;
    int item_col;
    const char *const *items;
    int item_count;
    int selected;
    const char *footer;
} UiMenuDialog;

void ui_hide_cursor(void);
void ui_show_cursor(void);
void ui_set_cursor(int col, int row);
void ui_flush(void);
void ui_fill_rect(int left, int top, int right, int bottom,
                  unsigned char ch, unsigned char attr);
void ui_putc(int col, int row, unsigned char ch, unsigned char attr);
void ui_write_at(int col, int row, const char *text, unsigned char attr);
void ui_write_padded(int col, int row, const char *text, int width, unsigned char attr);
int ui_visible_start(const char *text, int width, int start, int cursor);
void ui_write_clipped(int col, int row, const char *text, int width,
                      int start, int cursor, unsigned char attr);
void ui_write_ellipsis(int col, int row, const char *text, int width, unsigned char attr);
void ui_write_centered(int row, const char *text, unsigned char attr);
void ui_write_2digit_at(int col, int row, unsigned value, unsigned char attr);
unsigned ui_current_second(void);
void ui_draw_frame(void);
UiDialogBox ui_dialog_box(int left, int top, int right, int bottom, const char *title);
UiConfirmDialog ui_confirm_dialog(UiDialogBox box, const char *question, const char *value,
                                  int value_col, int value_width, const char *footer);
UiDetailDialogRow ui_detail_dialog_row(const char *label, const char *value);
UiDetailDialog ui_detail_dialog_spec(UiDialogBox box, const UiDetailDialogRow *rows,
                                     int row_count, int first_row, int row_step,
                                     unsigned char value_attr);
UiTextLineDialog ui_text_line_dialog(UiDialogBox box, int col,
                                     const UiTextLineDialogLine *lines, int line_count);
UiEditField ui_edit_field(int row, const char *label, const char *text,
                          int text_col, int width, int cursor, unsigned char attr);
UiEditDialog ui_edit_dialog(UiDialogBox box, int label_col,
                            const UiEditField *fields, int field_count, const char *footer);
UiMenuDialog ui_menu_dialog(UiDialogBox box, int prompt_row, int prompt_col,
                            const char *prompt_label, const char *prompt_value, int prompt_width,
                            int menu_row, int menu_left, int menu_right, int item_col,
                            const char *const *items, int item_count, int selected,
                            const char *footer);
void ui_draw_confirm_dialog(const UiConfirmDialog *dialog);
void ui_draw_detail_dialog(const UiDetailDialog *dialog);
void ui_draw_text_line_dialog(const UiTextLineDialog *dialog);
void ui_draw_edit_dialog(const UiEditDialog *dialog);
int ui_edit_dialog_cursor_col(const UiEditField *field);
int ui_edit_dialog_cursor_row(const UiDialogBox *box, const UiEditField *field);
void ui_draw_menu_dialog(const UiMenuDialog *dialog);
void ui_draw_header_on_frame_common(int modified);
void ui_draw_header_on_frame(const AmlState *state);
int ui_bigtext_enable(void);
void ui_bigtext_disable(void);
int ui_bigtext_is_enabled(void);
void ui_bigtext_write_at(int col, int row, const char *text, unsigned char attr);
int ui_hotkey_index(int key);
char ui_hotkey_char(int index);
int ui_find_match(const AmlState *state, const char *needle);
void ui_sync_view_top(AmlState *state);
void ui_render(const AmlState *state);
int ui_read_auto_line(char *line, unsigned line_size);
void ui_trace_event(const char *event_name);

#endif
