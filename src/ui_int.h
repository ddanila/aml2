#ifndef UI_INT_H
#define UI_INT_H

#include "aml.h"

enum {
    AML_UI_ROWS = 25,
    AML_UI_COLS = 80,
    AML_UI_LIST_ROW = 1,
    AML_UI_LIST_ROWS = 23,
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
    AML_UI_ATTR_SHADOW = 0x08,
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
    AML_KEY_F5 = 63,
    AML_KEY_F6 = 64,
    AML_KEY_F8 = 66,
    AML_KEY_F9 = 67,
    AML_KEY_F10 = 68,
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
int ui_dialog_row(int top, int inner_row);
void ui_draw_titled_dialog(int left, int top, int right, int bottom, const char *title);
void ui_draw_header_on_frame_common(int modified);
void ui_draw_header_on_frame(const AmlState *state);
int ui_hotkey_index(int key);
char ui_hotkey_char(int index);
int ui_find_match(const AmlState *state, const char *needle);
void ui_sync_view_top(AmlState *state);
void ui_render(const AmlState *state, const char *status);
void ui_draw_detail_line(int row, const char *label, const char *value, unsigned char attr);
int ui_read_auto_line(char *line, unsigned line_size);
void ui_trace_event(const char *event_name);

#endif
