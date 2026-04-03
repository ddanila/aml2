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
    AML_UI_LIST_ROW = 5,
    AML_UI_LIST_ROWS = 17,
    AML_SEARCH_MAX = 24,
    AML_KEY_ENTER = 13,
    AML_KEY_BACKSPACE = 8,
    AML_KEY_SLASH = '/',
    AML_KEY_ESC = 27,
    AML_KEY_EXTENDED = 0,
    AML_KEY_EXTENDED_2 = 224,
    AML_KEY_HOME = 71,
    AML_KEY_UP = 72,
    AML_KEY_PGUP = 73,
    AML_KEY_END = 79,
    AML_KEY_DOWN = 80,
    AML_KEY_PGDN = 81
};

static void aml_ui_gotoxy(int col, int row)
{
    union REGS inregs;
    union REGS outregs;

    inregs.h.ah = 0x02;
    inregs.h.bh = 0x00;
    inregs.h.dh = (unsigned char)(row - 1);
    inregs.h.dl = (unsigned char)(col - 1);
    int86(0x10, &inregs, &outregs);
}

static void aml_ui_clrscr(void)
{
    union REGS inregs;
    union REGS outregs;

    inregs.h.ah = 0x06;
    inregs.h.al = 0x00;
    inregs.h.bh = 0x07;
    inregs.h.ch = 0x00;
    inregs.h.cl = 0x00;
    inregs.h.dh = AML_UI_ROWS - 1;
    inregs.h.dl = AML_UI_COLS - 1;
    int86(0x10, &inregs, &outregs);
    aml_ui_gotoxy(1, 1);
}

static void aml_ui_clear_line(int row)
{
    int i;

    aml_ui_gotoxy(1, row);
    for (i = 0; i < AML_UI_COLS; ++i) {
        putch(' ');
    }
}

static void aml_ui_write_at(int col, int row, const char *text)
{
    aml_ui_gotoxy(col, row);
    cputs(text);
}

static void aml_ui_write_centered(int row, const char *text)
{
    int col = (AML_UI_COLS - (int)strlen(text)) / 2;

    if (col < 2) {
        col = 2;
    }
    aml_ui_write_at(col, row, text);
}

static void aml_ui_draw_border(void)
{
    int i;

    aml_ui_gotoxy(1, 1);
    putch('+');
    for (i = 2; i < AML_UI_COLS; ++i) {
        putch('-');
    }
    putch('+');

    for (i = 2; i < AML_UI_ROWS; ++i) {
        aml_ui_gotoxy(1, i);
        putch('|');
        aml_ui_gotoxy(AML_UI_COLS, i);
        putch('|');
    }

    aml_ui_gotoxy(1, AML_UI_ROWS);
    putch('+');
    for (i = 2; i < AML_UI_COLS; ++i) {
        putch('-');
    }
    putch('+');
}

static void aml_ui_draw_header(void)
{
    aml_ui_clear_line(2);
    aml_ui_clear_line(3);
    aml_ui_write_at(3, 2, "aml2");
    aml_ui_write_at(10, 2, "Arvutimuuseum Launcher v2");
    aml_ui_write_at(3, 3, "Arrows/Home/End/PgUp/PgDn: Move   Enter: Select   Esc: Quit   0-9,a-z");
}

static void aml_ui_write_padded(const char *text, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
        char ch = text[i];
        if (ch == '\0') {
            break;
        }
        putch(ch);
    }
    for (; i < width; ++i) {
        putch(' ');
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
        return 10 + (key - 'A');
    }
    return -1;
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

static void aml_ui_write_uint(unsigned value)
{
    char digits[6];
    int i = 0;

    do {
        digits[i++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0 && i < (int)sizeof(digits));

    while (i > 0) {
        putch(digits[--i]);
    }
}

static void aml_ui_write_position(const AmlState *state)
{
    if (state->entry_count <= 0) {
        return;
    }

    aml_ui_gotoxy(63, 23);
    cputs("Item ");
    aml_ui_write_uint((unsigned)(state->selected + 1));
    putch('/');
    aml_ui_write_uint((unsigned)state->entry_count);
}

static void aml_ui_draw_entries(const AmlState *state)
{
    int i;
    int row;
    int top = aml_ui_first_visible(state);

    for (row = AML_UI_LIST_ROW; row < AML_UI_LIST_ROW + AML_UI_LIST_ROWS; ++row) {
        aml_ui_clear_line(row);
    }

    if (state->entry_count <= 0) {
        aml_ui_write_at(4, 6, "No entries found in launcher.cfg");
        return;
    }

    for (row = AML_UI_LIST_ROW, i = top;
         row < AML_UI_LIST_ROW + AML_UI_LIST_ROWS && i < state->entry_count;
         ++row, ++i) {
        aml_ui_clear_line(row);
        aml_ui_gotoxy(4, row);
        putch((i == state->selected) ? 16 : ' ');
        putch('[');
        if (i >= 0 && i <= 9) {
            putch('0' + i);
        } else if (i >= 10 && i < 36) {
            putch('a' + (i - 10));
        } else {
            putch(' ');
        }
        putch(']');
        putch(' ');
        aml_ui_write_padded(state->entries[i].name, 52);
    }
}

static void aml_ui_draw_footer(const AmlState *state, const char *status)
{
    aml_ui_clear_line(22);
    aml_ui_clear_line(23);
    aml_ui_clear_line(24);

    if (state->entry_count > 0 &&
        state->selected >= 0 &&
        state->selected < state->entry_count) {
        aml_ui_gotoxy(3, 22);
        cputs("Path: ");
        if (state->entries[state->selected].path[0] != '\0') {
            aml_ui_write_padded(state->entries[state->selected].path, 68);
        } else {
            aml_ui_write_padded(".", 68);
        }
    }

    if (status != NULL && status[0] != '\0') {
        aml_ui_write_at(3, 23, status);
    }
    aml_ui_write_position(state);

    if (state->entry_count > 0 &&
        state->selected >= 0 &&
        state->selected < state->entry_count) {
        aml_ui_gotoxy(3, 24);
        cputs("Command: ");
        aml_ui_write_padded(state->entries[state->selected].command, 68);
    }
}

void aml_ui_show_message(const char *title, const char *line1, const char *line2, const char *line3)
{
    aml_ui_clrscr();
    aml_ui_draw_border();
    aml_ui_draw_header();
    aml_ui_clear_line(9);
    aml_ui_clear_line(11);
    aml_ui_clear_line(13);
    aml_ui_clear_line(15);
    if (title != NULL && title[0] != '\0') {
        aml_ui_write_centered(9, title);
    }
    if (line1 != NULL && line1[0] != '\0') {
        aml_ui_write_centered(11, line1);
    }
    if (line2 != NULL && line2[0] != '\0') {
        aml_ui_write_centered(13, line2);
    }
    if (line3 != NULL && line3[0] != '\0') {
        aml_ui_write_centered(15, line3);
    }
}

static int aml_ui_prompt_search(AmlState *state, const char **status)
{
    char query[AML_SEARCH_MAX + 1];
    int len = 0;

    query[0] = '\0';

    for (;;) {
        int key;
        int match;

        aml_ui_clear_line(23);
        aml_ui_gotoxy(3, 23);
        cputs("Find: ");
        aml_ui_write_padded(query, AML_SEARCH_MAX);

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
        } else if (isprint(key) && len < AML_SEARCH_MAX) {
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
        aml_ui_draw(state, *status);
    }
}

#if AML_TEST_HOOKS
enum {
    AML_UI_AUTO_NONE = -1,
    AML_UI_AUTO_REDRAW = -2
};

static void aml_ui_trim_newline(char *line)
{
    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[len - 1] = '\0';
        len--;
    }
}

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

    if (strcmp(line, "quit") == 0) {
        aml_ui_trace_event("auto_quit");
        return AML_UI_QUIT;
    }

    if (strncmp(line, "search ", 7) == 0) {
        int index = aml_ui_find_prefix(state, line + 7);
        if (index >= 0) {
            state->selected = index;
            aml_ui_trace_event("auto_search");
            return AML_UI_AUTO_REDRAW;
        }
        aml_ui_trace_event("auto_bad_search");
        return AML_UI_AUTO_REDRAW;
    }

    aml_ui_trace_event("auto_unknown");
    return AML_UI_QUIT;
}
#endif

void aml_ui_init(void)
{
    aml_ui_clrscr();
    aml_ui_draw_border();
    aml_ui_draw_header();
}

void aml_ui_shutdown(void)
{
    aml_ui_clrscr();
}

void aml_ui_draw(const AmlState *state, const char *status)
{
    aml_ui_draw_entries(state);
    aml_ui_draw_footer(state, status);
}

int aml_ui_run(AmlState *state)
{
    const char *status = "Select a program";

    for (;;) {
#if AML_TEST_HOOKS
        int auto_action;
#endif
        int key;
        int hotkey_index;

        aml_ui_draw(state, status);
#if AML_TEST_HOOKS
        sleep(1);
        auto_action = aml_ui_apply_automation(state);
        if (auto_action == AML_UI_AUTO_REDRAW) {
            continue;
        }
        if (auto_action >= 0) {
            return auto_action;
        }
#endif
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

            if (state->entry_count <= 0) {
                continue;
            }

            if (key == AML_KEY_HOME) {
                state->selected = 0;
                status = "Select a program";
            } else if (key == AML_KEY_END) {
                state->selected = state->entry_count - 1;
                status = "Select a program";
            } else if (key == AML_KEY_PGUP) {
                state->selected -= AML_UI_LIST_ROWS;
                if (state->selected < 0) {
                    state->selected = 0;
                }
                status = "Select a program";
            } else if (key == AML_KEY_PGDN) {
                state->selected += AML_UI_LIST_ROWS;
                if (state->selected >= state->entry_count) {
                    state->selected = state->entry_count - 1;
                }
                status = "Select a program";
            } else if (key == AML_KEY_UP) {
                if (state->selected > 0) {
                    state->selected--;
                } else {
                    state->selected = state->entry_count - 1;
                }
                status = "Select a program";
            } else if (key == AML_KEY_DOWN) {
                if (state->selected < state->entry_count - 1) {
                    state->selected++;
                } else {
                    state->selected = 0;
                }
                status = "Select a program";
            }
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
            status = "Use arrows, PgUp/PgDn, Home/End, Enter, Esc, or 0-9/a-z";
        }
    }
}
