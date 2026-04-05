#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <stdlib.h>
#include <string.h>

#include "ui_int.h"

static int aml_ui_has_selection(const AmlState *state)
{
    return state->entry_count > 0 &&
           state->selected >= 0 &&
           state->selected < state->entry_count;
}

static int aml_ui_require_editor_mode(AmlState *state)
{
    if (state->editor_mode) {
        return 1;
    }

    aml_ui_show_notice(
        state,
        "Viewer mode",
        "Editing is disabled in the default mode.",
        "",
        ""
    );
    aml_ui_wait_for_ack();
    return 0;
}

static int aml_ui_require_editor_selection(AmlState *state)
{
    return aml_ui_require_editor_mode(state) && aml_ui_has_selection(state);
}

static void aml_ui_draw_detail_line_value(int row, const char *label, const char *value)
{
    aml_ui_draw_detail_line(row, label, value, AML_UI_ATTR_DIALOG_DIM);
}

static void aml_ui_show_details_overlay(const AmlState *state)
{
    char hotkey[2];
    const AmlEntry *entry;

    if (!aml_ui_has_selection(state)) {
        aml_ui_show_message(
            "No current entry",
            "There is nothing to show yet.",
            "Use Ins to add a new record.",
            ""
        );
        aml_ui_wait_for_ack();
        return;
    }

    entry = &state->entries[state->selected];
    hotkey[0] = aml_ui_hotkey_char(state->selected);
    hotkey[1] = '\0';

    aml_ui_render(state, "Details");
    aml_ui_draw_titled_dialog(10, 6, 69, 18, "Entry Details");
    aml_ui_draw_detail_line_value(aml_ui_dialog_row(6, 2), "Name", entry->name);
    aml_ui_draw_detail_line_value(aml_ui_dialog_row(6, 4), "Command", entry->command);
    aml_ui_draw_detail_line_value(aml_ui_dialog_row(6, 6), "Path", entry->path[0] != '\0' ? entry->path : ".");
    aml_ui_draw_detail_line_value(aml_ui_dialog_row(6, 8), "Hotkey", hotkey[0] != ' ' ? hotkey : "-");
    aml_ui_flush();
    aml_ui_wait_for_ack();
}

static int aml_ui_confirm_delete(const AmlState *state)
{
    if (!aml_ui_has_selection(state)) {
        return 0;
    }

    aml_ui_render(state, "Delete");
    aml_ui_draw_titled_dialog(12, 8, 67, 16, "Delete Entry");
    aml_ui_write_centered(aml_ui_dialog_row(8, 2), "Remove the current entry from LAUNCHER.CFG?", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_ellipsis(18, aml_ui_dialog_row(8, 3), state->entries[state->selected].name, 44, AML_UI_ATTR_DIALOG_DIM);
    aml_ui_write_centered(aml_ui_dialog_row(8, 5), "Enter delete  Esc cancel", AML_UI_ATTR_HELP);
    aml_ui_flush();

    for (;;) {
        int key = getch();

        if (key == AML_KEY_ESC) {
            return 0;
        }
        if (key == AML_KEY_ENTER) {
            return 1;
        }
        if (key == AML_KEY_EXTENDED || key == AML_KEY_EXTENDED_2) {
            key = getch();
            if (key == AML_KEY_ESC) {
                return 0;
            }
        }
    }
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
    int field_width = 40;

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
        int *len_ptr = &name_len;
        int *cursor_ptr = &name_cursor;
        char *buf = name;
        int max_len = AML_MAX_NAME;
        int visible_start = aml_ui_visible_start(name, field_width, 0, name_cursor);
        int cursor_col = 24 + name_cursor;
        int cursor_row = 10;

        aml_ui_draw_titled_dialog(8, 5, 71, 18, is_new ? "Insert Entry" : "Edit Entry");
        aml_ui_write_at(12, aml_ui_dialog_row(5, 3), "Name", AML_UI_ATTR_DIALOG_TEXT);
        aml_ui_write_at(12, aml_ui_dialog_row(5, 5), "Command", AML_UI_ATTR_DIALOG_TEXT);
        aml_ui_write_at(12, aml_ui_dialog_row(5, 7), "Path", AML_UI_ATTR_DIALOG_TEXT);
        aml_ui_write_clipped(24, aml_ui_dialog_row(5, 3), name, field_width, 0, name_cursor,
                             field == 0 ? AML_UI_ATTR_SELECTED : AML_UI_ATTR_DIALOG_DIM);
        aml_ui_write_clipped(24, aml_ui_dialog_row(5, 5), command, field_width, 0, command_cursor,
                             field == 1 ? AML_UI_ATTR_SELECTED : AML_UI_ATTR_DIALOG_DIM);
        aml_ui_write_clipped(24, aml_ui_dialog_row(5, 7), path, field_width, 0, path_cursor,
                             field == 2 ? AML_UI_ATTR_SELECTED : AML_UI_ATTR_DIALOG_DIM);
        aml_ui_write_centered(aml_ui_dialog_row(5, 9), "Enter next  F2 save  Tab switch  Esc cancel", AML_UI_ATTR_HELP);
        aml_ui_flush();
        if (field == 1) {
            visible_start = aml_ui_visible_start(command, field_width, 0, command_cursor);
            cursor_col = 24 + (command_cursor - visible_start);
            cursor_row = aml_ui_dialog_row(5, 5);
        } else if (field == 2) {
            visible_start = aml_ui_visible_start(path, field_width, 0, path_cursor);
            cursor_col = 24 + (path_cursor - visible_start);
            cursor_row = aml_ui_dialog_row(5, 7);
        } else {
            cursor_col = 24 + (name_cursor - visible_start);
            cursor_row = aml_ui_dialog_row(5, 3);
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
            }
            continue;
        }
        if (key == AML_KEY_EXTENDED || key == AML_KEY_EXTENDED_2) {
            key = getch();
            if (key == AML_KEY_F2) {
                if (name[0] == '\0' || command[0] == '\0') {
                    aml_ui_show_message(
                        "Name and command required",
                        "Both fields must be filled in.",
                        "",
                        ""
                    );
                    aml_ui_wait_for_ack();
                    continue;
                }
                strcpy(entry->name, name);
                strcpy(entry->command, command);
                strcpy(entry->path, path);
                aml_ui_hide_cursor();
                return 1;
            } else if (key == AML_KEY_UP) {
                field = (field + 2) % 3;
            } else if (key == AML_KEY_DOWN) {
                field = (field + 1) % 3;
            } else {
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

        aml_ui_edit_field(key, buf, max_len, len_ptr, cursor_ptr);
    }
}

static void aml_ui_insert_entry(AmlState *state)
{
    int index;
    int i;
    AmlEntry entry;

    if (!aml_ui_require_editor_mode(state)) {
        return;
    }

    if (state->entry_count >= AML_MAX_PROGRAMS) {
        aml_ui_show_message(
            "List is full",
            "Maximum entry count reached.",
            "Delete something before inserting.",
            ""
        );
        aml_ui_wait_for_ack();
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
    if (!aml_ui_require_editor_mode(state)) {
        return;
    }

    if (!aml_ui_has_selection(state)) {
        aml_ui_insert_entry(state);
        return;
    }

    if (aml_ui_prompt_entry(&state->entries[state->selected], 0)) {
        state->modified = 1;
    }
}

static void aml_ui_delete_entry(AmlState *state)
{
    int i;

    if (!aml_ui_require_editor_selection(state)) {
        return;
    }

    for (i = state->selected; i < state->entry_count - 1; ++i) {
        state->entries[i] = state->entries[i + 1];
    }

    state->entry_count--;
    if (state->entry_count <= 0) {
        state->selected = 0;
    } else if (state->selected >= state->entry_count) {
        state->selected = state->entry_count - 1;
    }
    state->modified = 1;
}

static void aml_ui_delete_entry_with_confirm(AmlState *state)
{
    if (!aml_ui_require_editor_selection(state)) {
        return;
    }

    if (!aml_ui_confirm_delete(state)) {
        return;
    }

    aml_ui_delete_entry(state);
}

static void aml_ui_move_entry_up(AmlState *state)
{
    AmlEntry temp;

    if (!aml_ui_require_editor_selection(state)) {
        return;
    }

    if (state->entry_count <= 1 || state->selected <= 0) {
        return;
    }

    temp = state->entries[state->selected - 1];
    state->entries[state->selected - 1] = state->entries[state->selected];
    state->entries[state->selected] = temp;
    state->selected--;
    state->modified = 1;
}

static void aml_ui_move_entry_down(AmlState *state)
{
    AmlEntry temp;

    if (!aml_ui_require_editor_mode(state)) {
        return;
    }

    if (state->entry_count <= 1 ||
        state->selected < 0 ||
        state->selected >= state->entry_count - 1) {
        return;
    }

    temp = state->entries[state->selected + 1];
    state->entries[state->selected + 1] = state->entries[state->selected];
    state->entries[state->selected] = temp;
    state->selected++;
    state->modified = 1;
}

static void aml_ui_show_help_overlay(const AmlState *state)
{
    int left = 10;
    int top = 6;
    int right = 69;
    int bottom = 19;
    int text_col = left + 3;

    aml_ui_render(state, "Help");
    aml_ui_draw_titled_dialog(left, top, right, bottom, "Launcher Help");

    aml_ui_write_at(text_col, aml_ui_dialog_row(top, 1), "Enter  Launch selected item", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(text_col, aml_ui_dialog_row(top, 2), "/      Search within item name", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(text_col, aml_ui_dialog_row(top, 3), "F2     Save configuration", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(text_col, aml_ui_dialog_row(top, 4), "F3     Show current entry details", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(text_col, aml_ui_dialog_row(top, 5), "F4     Edit current entry", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(text_col, aml_ui_dialog_row(top, 6), "F5/F6  Move current entry up/down", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(text_col, aml_ui_dialog_row(top, 7), "Ins    Insert a new entry", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(text_col, aml_ui_dialog_row(top, 8), "F8     Delete current entry", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(text_col, aml_ui_dialog_row(top, 9), "F9     Debug run menu", AML_UI_ATTR_DIALOG_TEXT);
    aml_ui_write_at(text_col, aml_ui_dialog_row(top, 10), "F10    Exit to DOS", AML_UI_ATTR_DIALOG_TEXT);

    aml_ui_flush();
    aml_ui_wait_for_ack();
}

static int aml_ui_show_debug_run_menu(const AmlState *state)
{
    static const char *items[] = {
        "Via stub, pause on error",
        "Run directly as child",
        "Run via COMMAND.COM"
    };
    static const int actions[] = {
        AML_UI_LAUNCH_DEBUG,
        AML_UI_LAUNCH_CHILD_DIRECT,
        AML_UI_LAUNCH_CHILD_SHELL
    };
    int selected = 0;

    for (;;) {
        int key;
        int i;

        aml_ui_render(state, "Debug Run");
        aml_ui_draw_titled_dialog(10, 6, 69, 18, "Debug Run");
        aml_ui_write_at(14, aml_ui_dialog_row(6, 2), "Command", AML_UI_ATTR_DIALOG_TEXT);
        aml_ui_write_clipped(
            24, aml_ui_dialog_row(6, 2),
            state->entries[state->selected].command,
            39, 0, 0, AML_UI_ATTR_DIALOG_DIM
        );

        for (i = 0; i < 3; ++i) {
            unsigned char attr = (i == selected) ? AML_UI_ATTR_SELECTED : AML_UI_ATTR_DIALOG_TEXT;
            aml_ui_fill_rect(14, aml_ui_dialog_row(6, 4 + i), 64, aml_ui_dialog_row(6, 4 + i), ' ', attr);
            aml_ui_putc(16, aml_ui_dialog_row(6, 4 + i), (i == selected) ? 16 : 250, attr);
            aml_ui_write_at(20, aml_ui_dialog_row(6, 4 + i), items[i], attr);
        }

        aml_ui_write_centered(aml_ui_dialog_row(6, 8), "Enter select  Esc cancel", AML_UI_ATTR_HELP);
        aml_ui_flush();
        key = getch();

        if (key == AML_KEY_ESC) {
            return -1;
        }
        if (key == AML_KEY_ENTER) {
            return actions[selected];
        }
        if (key == AML_KEY_EXTENDED || key == AML_KEY_EXTENDED_2) {
            key = getch();
            if (key == AML_KEY_UP) {
                selected = (selected + 2) % 3;
            } else if (key == AML_KEY_DOWN) {
                selected = (selected + 1) % 3;
            }
        }
    }
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
        int index = aml_ui_find_match(state, line + 7);
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

    if (strcmp(line, "help") == 0) {
        aml_ui_trace_event("auto_help");
        aml_ui_show_help_overlay(state);
        return AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "move_up") == 0) {
        aml_ui_move_entry_up(state);
        aml_ui_trace_event("auto_move_up");
        return AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "move_down") == 0) {
        aml_ui_move_entry_down(state);
        aml_ui_trace_event("auto_move_down");
        return AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "delete") == 0) {
        aml_ui_delete_entry(state);
        aml_ui_trace_event("auto_delete");
        return AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "save") == 0) {
        aml_ui_trace_event("auto_save");
        return AML_UI_SAVE;
    }

    if (strcmp(line, "quit") == 0) {
        aml_ui_trace_event("auto_quit");
        return AML_UI_QUIT;
    }

    aml_ui_trace_event("auto_unknown");
    return AML_UI_QUIT;
}

static int aml_ui_prompt_search(AmlState *state, const char **status)
{
    char query[AML_UI_SEARCH_MAX + 1];
    int len = 0;

    query[0] = '\0';

    for (;;) {
        int key;
        int match;

        aml_ui_render(state, *status);
        aml_ui_fill_rect(3, 23, 76, 23, ' ', AML_UI_ATTR_STATUS);
        aml_ui_write_at(3, 23, "Find:", AML_UI_ATTR_MUTED);
        aml_ui_write_padded(9, 23, query, AML_UI_SEARCH_MAX, AML_UI_ATTR_STATUS);
        aml_ui_flush();

        key = getch();
        if (key == AML_KEY_ESC) {
            *status = "Search cancelled";
            return 0;
        }
        if (key == AML_KEY_ENTER) {
            match = aml_ui_find_match(state, query);
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

        match = aml_ui_find_match(state, query);
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

int aml_ui_run(AmlState *state)
{
    const char *status = "";
    unsigned last_second = 60;

    aml_ui_sync_view_top(state);

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
                aml_ui_sync_view_top(state);
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

        if (key == AML_KEY_SLASH && state->entry_count > 0) {
            aml_ui_prompt_search(state, &status);
            aml_ui_sync_view_top(state);
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
            } else if (key == AML_KEY_F5) {
                aml_ui_move_entry_up(state);
            } else if (key == AML_KEY_F6) {
                aml_ui_move_entry_down(state);
            } else if (key == AML_KEY_INS) {
                aml_ui_insert_entry(state);
            } else if (key == AML_KEY_F8) {
                aml_ui_delete_entry_with_confirm(state);
            } else if (key == AML_KEY_F9) {
                if (state->entry_count > 0) {
                    int action = aml_ui_show_debug_run_menu(state);
                    if (action >= 0) {
                        return action;
                    }
                }
            } else if (key == AML_KEY_F10) {
                return AML_UI_QUIT;
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

            aml_ui_sync_view_top(state);
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
            aml_ui_sync_view_top(state);
            return AML_UI_LAUNCH;
        }

        if (isprint(key)) {
            status = "Unknown key";
        } else {
            status = "";
        }
    }
}
