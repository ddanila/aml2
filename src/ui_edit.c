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

    {
        UiDetailDialogRow rows[4];
        UiDetailDialog dialog;

        rows[0] = ui_detail_dialog_row("Name", entry->name);
        rows[1] = ui_detail_dialog_row("Command", entry->command);
        rows[2] = ui_detail_dialog_row("Path", entry->path[0] != '\0' ? entry->path : ".");
        rows[3] = ui_detail_dialog_row("Hotkey", hotkey[0] != ' ' ? hotkey : "-");

        dialog = ui_detail_dialog_spec(
            ui_dialog_box(10, 6, 69, 18, "Entry Details"),
            rows, 4, 2, 2, UI_ATTR_DIALOG_DIM
        );

        ui_render(state);
        ui_draw_detail_dialog(&dialog);
    }
    ui_flush();
    ui_wait_for_ack();
}

static int confirm_delete(const AmlState *state)
{
    if (!ui_has_selection(state)) {
        return 0;
    }

    {
        UiConfirmDialog dialog;

        dialog = ui_confirm_dialog(
            ui_dialog_box(12, 8, 67, 16, "Delete Entry"),
            "Remove the current entry from LAUNCHER.CFG?",
            state->entries[state->selected].name,
            18, 44,
            "Enter delete  Esc cancel"
        );

        ui_render(state);
        ui_draw_confirm_dialog(&dialog);
    }
    ui_flush();

    for (;;) {
        int key = getch();

        if (key == UI_KEY_ESC) {
            return 0;
        }
        if (key == UI_KEY_ENTER) {
            return 1;
        }
        if (key == UI_KEY_EXTENDED || key == UI_KEY_EXTENDED_2) {
            key = getch();
            if (key == UI_KEY_ESC) {
                return 0;
            }
        }
    }
}

static int edit_field(int key, char *buf, int max_len, int *len, int *cursor)
{
    if (key == UI_KEY_BACKSPACE) {
        if (*cursor > 0 && *len > 0) {
            memmove(&buf[*cursor - 1], &buf[*cursor], (size_t)(*len - *cursor + 1));
            (*len)--;
            (*cursor)--;
        }
        return 1;
    }
    if (key == UI_KEY_DEL) {
        if (*cursor < *len) {
            memmove(&buf[*cursor], &buf[*cursor + 1], (size_t)(*len - *cursor));
            (*len)--;
        }
        return 1;
    }
    if (key == UI_KEY_LEFT) {
        if (*cursor > 0) {
            (*cursor)--;
        }
        return 1;
    }
    if (key == UI_KEY_RIGHT) {
        if (*cursor < *len) {
            (*cursor)++;
        }
        return 1;
    }
    if (key == UI_KEY_HOME) {
        *cursor = 0;
        return 1;
    }
    if (key == UI_KEY_END) {
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

    strncpy(name, is_new ? "" : entry->name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    strncpy(command, is_new ? "" : entry->command, sizeof(command) - 1);
    command[sizeof(command) - 1] = '\0';
    strncpy(path, is_new ? "" : entry->path, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
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
        UiEditField fields[3];
        UiEditDialog dialog;

        fields[0] = ui_edit_field(3, "Name", name, 24, field_width, name_cursor,
                                  field == 0 ? UI_ATTR_SELECTED : UI_ATTR_DIALOG_DIM);
        fields[1] = ui_edit_field(5, "Command", command, 24, field_width, command_cursor,
                                  field == 1 ? UI_ATTR_SELECTED : UI_ATTR_DIALOG_DIM);
        fields[2] = ui_edit_field(7, "Path", path, 24, field_width, path_cursor,
                                  field == 2 ? UI_ATTR_SELECTED : UI_ATTR_DIALOG_DIM);
        dialog = ui_edit_dialog(
            ui_dialog_box(8, 5, 71, 18, is_new ? "Insert Entry" : "Edit Entry"),
            12,
            fields,
            3,
            "Enter next  F2 save  Tab switch  Esc cancel"
        );

        ui_draw_edit_dialog(&dialog);
        ui_flush();
        ui_set_cursor(
            ui_edit_dialog_cursor_col(&fields[field]),
            ui_edit_dialog_cursor_row(&dialog.box, &fields[field])
        );
        ui_show_cursor();

        key = getch();
        if (key == UI_KEY_ESC) {
            ui_hide_cursor();
            return 0;
        }
        if (key == '\t') {
            field = (field + 1) % 3;
            continue;
        }
        if (key == UI_KEY_ENTER) {
            if (field < 2) {
                field++;
            }
            continue;
        }
        if (key == UI_KEY_EXTENDED || key == UI_KEY_EXTENDED_2) {
            key = getch();
            if (key == UI_KEY_F2) {
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
                strncpy(entry->name, name, sizeof(entry->name) - 1);
                entry->name[sizeof(entry->name) - 1] = '\0';
                strncpy(entry->command, command, sizeof(entry->command) - 1);
                entry->command[sizeof(entry->command) - 1] = '\0';
                strncpy(entry->path, path, sizeof(entry->path) - 1);
                entry->path[sizeof(entry->path) - 1] = '\0';
                ui_hide_cursor();
                return 1;
            } else if (key == UI_KEY_UP) {
                field = (field + 2) % 3;
            } else if (key == UI_KEY_DOWN) {
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
    static const UiTextLineDialogLine lines[] = {
        { 1, "Enter  Launch selected item", UI_ATTR_DIALOG_TEXT },
        { 2, "/      Search within item name", UI_ATTR_DIALOG_TEXT },
        { 3, "F2     Save configuration", UI_ATTR_DIALOG_TEXT },
        { 4, "F3     Show current entry details", UI_ATTR_DIALOG_TEXT },
        { 5, "F4     Edit current entry", UI_ATTR_DIALOG_TEXT },
        { 6, "F5/F6  Move current entry up/down", UI_ATTR_DIALOG_TEXT },
        { 7, "Ins    Insert a new entry", UI_ATTR_DIALOG_TEXT },
        { 8, "F8     Delete current entry", UI_ATTR_DIALOG_TEXT },
        { 9, "F9     Debug run menu", UI_ATTR_DIALOG_TEXT },
        { 10, "F10    Exit to DOS", UI_ATTR_DIALOG_TEXT }
    };
    UiTextLineDialog dialog;

    dialog = ui_text_line_dialog(ui_dialog_box(10, 6, 69, 19, "Launcher Help"), 13, lines, 10);

    ui_render(state);
    ui_draw_text_line_dialog(&dialog);

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
        UiMenuDialog dialog;

        ui_render(state);
        dialog = ui_menu_dialog(
            ui_dialog_box(10, 6, 69, 18, "Debug Run"),
            2, 14,
            "Command",
            state->entries[state->selected].command,
            39,
            4, 14, 64, 20,
            items, 3, selected,
            "Enter select  Esc cancel"
        );
        ui_draw_menu_dialog(&dialog);
        ui_flush();
        key = getch();

        if (key == UI_KEY_ESC) {
            return (AmlUiAction)-1;
        }
        if (key == UI_KEY_ENTER) {
            return actions[selected];
        }
        if (key == UI_KEY_EXTENDED || key == UI_KEY_EXTENDED_2) {
            key = getch();
            if (key == UI_KEY_UP) {
                selected = (selected + 2) % 3;
            } else if (key == UI_KEY_DOWN) {
                selected = (selected + 1) % 3;
            }
        }
    }
}
