#include <conio.h>
#include <ctype.h>
#include <string.h>

#include "ui_int.h"
#include "ui_ops.h"
#include "ui_state.h"

static int require_editor_mode(AmlState *state)
{
    if (state->editor_mode) {
        return 1;
    }

    ui_show_notice(
        state,
        "Viewer mode",
        "Editing is disabled in the default mode.",
        "",
        ""
    );
    ui_wait_for_ack();
    return 0;
}

static int require_editor_selection(AmlState *state)
{
    return require_editor_mode(state) && ui_has_selection(state);
}

static void draw_detail_line_value(int row, const char *label, const char *value)
{
    ui_draw_detail_line(row, label, value, UI_ATTR_DIALOG_DIM);
}

void ui_show_details_overlay(const AmlState *state)
{
    char hotkey[2];
    const AmlEntry *entry;

    if (!ui_has_selection(state)) {
        ui_show_message(
            "No current entry",
            "There is nothing to show yet.",
            "Use Ins to add a new record.",
            ""
        );
        ui_wait_for_ack();
        return;
    }

    entry = &state->entries[state->selected];
    hotkey[0] = ui_hotkey_char(state->selected);
    hotkey[1] = '\0';

    ui_render(state, "Details");
    ui_draw_titled_dialog(10, 6, 69, 18, "Entry Details");
    draw_detail_line_value(ui_dialog_row(6, 2), "Name", entry->name);
    draw_detail_line_value(ui_dialog_row(6, 4), "Command", entry->command);
    draw_detail_line_value(ui_dialog_row(6, 6), "Path", entry->path[0] != '\0' ? entry->path : ".");
    draw_detail_line_value(ui_dialog_row(6, 8), "Hotkey", hotkey[0] != ' ' ? hotkey : "-");
    ui_flush();
    ui_wait_for_ack();
}

static int confirm_delete(const AmlState *state)
{
    if (!ui_has_selection(state)) {
        return 0;
    }

    ui_render(state, "Delete");
    ui_draw_titled_dialog(12, 8, 67, 16, "Delete Entry");
    ui_write_centered(ui_dialog_row(8, 2), "Remove the current entry from LAUNCHER.CFG?", UI_ATTR_DIALOG_TEXT);
    ui_write_ellipsis(18, ui_dialog_row(8, 3), state->entries[state->selected].name, 44, UI_ATTR_DIALOG_DIM);
    ui_write_centered(ui_dialog_row(8, 5), "Enter delete  Esc cancel", UI_ATTR_HELP);
    ui_flush();

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

static int edit_field(int key, char *buf, int max_len, int *len, int *cursor)
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

static int prompt_entry(AmlEntry *entry, int is_new)
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
        int visible_start = ui_visible_start(name, field_width, 0, name_cursor);
        int cursor_col = 24 + name_cursor;
        int cursor_row = 10;

        ui_draw_titled_dialog(8, 5, 71, 18, is_new ? "Insert Entry" : "Edit Entry");
        ui_write_at(12, ui_dialog_row(5, 3), "Name", UI_ATTR_DIALOG_TEXT);
        ui_write_at(12, ui_dialog_row(5, 5), "Command", UI_ATTR_DIALOG_TEXT);
        ui_write_at(12, ui_dialog_row(5, 7), "Path", UI_ATTR_DIALOG_TEXT);
        ui_write_clipped(24, ui_dialog_row(5, 3), name, field_width, 0, name_cursor,
                             field == 0 ? UI_ATTR_SELECTED : UI_ATTR_DIALOG_DIM);
        ui_write_clipped(24, ui_dialog_row(5, 5), command, field_width, 0, command_cursor,
                             field == 1 ? UI_ATTR_SELECTED : UI_ATTR_DIALOG_DIM);
        ui_write_clipped(24, ui_dialog_row(5, 7), path, field_width, 0, path_cursor,
                             field == 2 ? UI_ATTR_SELECTED : UI_ATTR_DIALOG_DIM);
        ui_write_centered(ui_dialog_row(5, 9), "Enter next  F2 save  Tab switch  Esc cancel", UI_ATTR_HELP);
        ui_flush();
        if (field == 1) {
            visible_start = ui_visible_start(command, field_width, 0, command_cursor);
            cursor_col = 24 + (command_cursor - visible_start);
            cursor_row = ui_dialog_row(5, 5);
        } else if (field == 2) {
            visible_start = ui_visible_start(path, field_width, 0, path_cursor);
            cursor_col = 24 + (path_cursor - visible_start);
            cursor_row = ui_dialog_row(5, 7);
        } else {
            cursor_col = 24 + (name_cursor - visible_start);
            cursor_row = ui_dialog_row(5, 3);
        }
        ui_set_cursor(cursor_col, cursor_row);
        ui_show_cursor();

        key = getch();
        if (key == AML_KEY_ESC) {
            ui_hide_cursor();
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
                    ui_show_message(
                        "Name and command required",
                        "Both fields must be filled in.",
                        "",
                        ""
                    );
                    ui_wait_for_ack();
                    continue;
                }
                strcpy(entry->name, name);
                strcpy(entry->command, command);
                strcpy(entry->path, path);
                ui_hide_cursor();
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
                edit_field(key, buf, max_len, len_ptr, cursor_ptr);
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

        edit_field(key, buf, max_len, len_ptr, cursor_ptr);
    }
}

void ui_insert_entry(AmlState *state)
{
    int index;
    int i;
    AmlEntry entry;

    if (!require_editor_mode(state)) {
        return;
    }

    if (state->entry_count >= AML_MAX_PROGRAMS) {
        ui_show_message(
            "List is full",
            "Maximum entry count reached.",
            "Delete something before inserting.",
            ""
        );
        ui_wait_for_ack();
        return;
    }

    entry.name[0] = '\0';
    entry.command[0] = '\0';
    entry.path[0] = '\0';
    if (!prompt_entry(&entry, 1)) {
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

void ui_edit_entry(AmlState *state)
{
    if (!require_editor_mode(state)) {
        return;
    }

    if (!ui_has_selection(state)) {
        ui_insert_entry(state);
        return;
    }

    if (prompt_entry(&state->entries[state->selected], 0)) {
        state->modified = 1;
    }
}

void ui_delete_entry(AmlState *state)
{
    int i;

    if (!require_editor_selection(state)) {
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

void ui_delete_entry_with_confirm(AmlState *state)
{
    if (!require_editor_selection(state)) {
        return;
    }

    if (!confirm_delete(state)) {
        return;
    }

    ui_delete_entry(state);
}

void ui_move_entry_up(AmlState *state)
{
    AmlEntry temp;

    if (!require_editor_selection(state)) {
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

void ui_move_entry_down(AmlState *state)
{
    AmlEntry temp;

    if (!require_editor_mode(state)) {
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

void ui_show_help_overlay(const AmlState *state)
{
    int left = 10;
    int top = 6;
    int right = 69;
    int bottom = 19;
    int text_col = left + 3;

    ui_render(state, "Help");
    ui_draw_titled_dialog(left, top, right, bottom, "Launcher Help");

    ui_write_at(text_col, ui_dialog_row(top, 1), "Enter  Launch selected item", UI_ATTR_DIALOG_TEXT);
    ui_write_at(text_col, ui_dialog_row(top, 2), "/      Search within item name", UI_ATTR_DIALOG_TEXT);
    ui_write_at(text_col, ui_dialog_row(top, 3), "F2     Save configuration", UI_ATTR_DIALOG_TEXT);
    ui_write_at(text_col, ui_dialog_row(top, 4), "F3     Show current entry details", UI_ATTR_DIALOG_TEXT);
    ui_write_at(text_col, ui_dialog_row(top, 5), "F4     Edit current entry", UI_ATTR_DIALOG_TEXT);
    ui_write_at(text_col, ui_dialog_row(top, 6), "F5/F6  Move current entry up/down", UI_ATTR_DIALOG_TEXT);
    ui_write_at(text_col, ui_dialog_row(top, 7), "Ins    Insert a new entry", UI_ATTR_DIALOG_TEXT);
    ui_write_at(text_col, ui_dialog_row(top, 8), "F8     Delete current entry", UI_ATTR_DIALOG_TEXT);
    ui_write_at(text_col, ui_dialog_row(top, 9), "F9     Debug run menu", UI_ATTR_DIALOG_TEXT);
    ui_write_at(text_col, ui_dialog_row(top, 10), "F10    Exit to DOS", UI_ATTR_DIALOG_TEXT);

    ui_flush();
    ui_wait_for_ack();
}

AmlUiAction ui_show_debug_run_menu(const AmlState *state)
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

        ui_render(state, "Debug Run");
        ui_draw_titled_dialog(10, 6, 69, 18, "Debug Run");
        ui_write_at(14, ui_dialog_row(6, 2), "Command", UI_ATTR_DIALOG_TEXT);
        ui_write_clipped(
            24, ui_dialog_row(6, 2),
            state->entries[state->selected].command,
            39, 0, 0, UI_ATTR_DIALOG_DIM
        );

        for (i = 0; i < 3; ++i) {
            unsigned char attr = (i == selected) ? UI_ATTR_SELECTED : UI_ATTR_DIALOG_TEXT;
            ui_fill_rect(14, ui_dialog_row(6, 4 + i), 64, ui_dialog_row(6, 4 + i), ' ', attr);
            ui_putc(16, ui_dialog_row(6, 4 + i), (i == selected) ? 16 : 250, attr);
            ui_write_at(20, ui_dialog_row(6, 4 + i), items[i], attr);
        }

        ui_write_centered(ui_dialog_row(6, 8), "Enter select  Esc cancel", UI_ATTR_HELP);
        ui_flush();
        key = getch();

        if (key == AML_KEY_ESC) {
            return (AmlUiAction)-1;
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
