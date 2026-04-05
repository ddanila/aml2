#include <conio.h>
#include <ctype.h>
#include <dos.h>

#include "ui_int.h"
#include "ui_ops.h"
#include "ui_state.h"

static void update_search_status(AmlState *state, const char *query, int len, const char **status)
{
    int match = ui_find_match(state, query);

    if (match >= 0) {
        ui_select_index(state, match);
        *status = "Search matched";
    } else if (len == 0) {
        *status = "Search cleared";
    } else {
        *status = "No matching entry";
    }
}

static int finish_search(AmlState *state, const char *query, int len, const char **status)
{
    int match = ui_find_match(state, query);

    if (match >= 0) {
        ui_select_index(state, match);
        *status = "Search matched";
    } else if (len == 0) {
        *status = "Select a program";
    } else {
        *status = "No matching entry";
    }

    return 0;
}

static int prompt_search(AmlState *state, const char **status)
{
    char query[AML_UI_SEARCH_MAX + 1];
    int len = 0;

    query[0] = '\0';

    for (;;) {
        int key;

        ui_render(state, *status);
        ui_fill_rect(3, 23, 76, 23, ' ', AML_UI_ATTR_STATUS);
        ui_write_at(3, 23, "Find:", AML_UI_ATTR_MUTED);
        ui_write_padded(9, 23, query, AML_UI_SEARCH_MAX, AML_UI_ATTR_STATUS);
        ui_flush();

        key = getch();
        if (key == AML_KEY_ESC) {
            *status = "Search cancelled";
            return 0;
        }
        if (key == AML_KEY_ENTER) {
            return finish_search(state, query, len, status);
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

        update_search_status(state, query, len, status);
    }
}

static void wait_for_input_redraw(AmlState *state, const char *status, unsigned *last_second)
{
    while (!kbhit()) {
        unsigned now_second = ui_current_second();

        if (now_second != *last_second) {
            ui_draw(state, status);
            *last_second = now_second;
        }
        delay(50);
    }
}

static AmlUiAction apply_test_automation(AmlState *state)
{
#if AML_TEST_HOOKS
    sleep(1);
    return ui_apply_automation(state);
#else
    (void)state;
    return (AmlUiAction)AML_UI_AUTO_NONE;
#endif
}

static AmlUiAction handle_extended_key(AmlState *state, int key)
{
    if (key == AML_KEY_F1) {
        ui_show_help_overlay(state);
    } else if (key == AML_KEY_F2) {
        return AML_UI_SAVE;
    } else if (key == AML_KEY_F3) {
        ui_show_details_overlay(state);
    } else if (key == AML_KEY_F4) {
        ui_edit_entry(state);
    } else if (key == AML_KEY_F5) {
        ui_move_entry_up(state);
    } else if (key == AML_KEY_F6) {
        ui_move_entry_down(state);
    } else if (key == AML_KEY_INS) {
        ui_insert_entry(state);
    } else if (key == AML_KEY_F8) {
        ui_delete_entry_with_confirm(state);
    } else if (key == AML_KEY_F9) {
        if (ui_has_entries(state)) {
            return ui_show_debug_run_menu(state);
        }
    } else if (key == AML_KEY_F10) {
        return AML_UI_QUIT;
    } else if (!ui_has_entries(state)) {
        return (AmlUiAction)AML_UI_AUTO_NONE;
    } else if (key == AML_KEY_HOME) {
        ui_select_first(state);
    } else if (key == AML_KEY_END) {
        ui_select_last(state);
    } else if (key == AML_KEY_PGUP) {
        ui_select_page_up(state);
    } else if (key == AML_KEY_PGDN) {
        ui_select_page_down(state);
    } else if (key == AML_KEY_UP) {
        ui_select_prev_wrap(state);
    } else if (key == AML_KEY_DOWN) {
        ui_select_next_wrap(state);
    }

    return (AmlUiAction)AML_UI_AUTO_REDRAW;
}

static AmlUiAction handle_hotkey(AmlState *state, int key)
{
    int hotkey_index = ui_hotkey_index(key);

    if (hotkey_index >= 0 && hotkey_index < state->entry_count) {
        ui_select_index(state, hotkey_index);
        ui_sync_view_top(state);
        return AML_UI_LAUNCH;
    }

    return (AmlUiAction)AML_UI_AUTO_NONE;
}

AmlUiAction ui_run(AmlState *state)
{
    const char *status = "";
    unsigned last_second = 60;

    ui_sync_view_top(state);

    for (;;) {
        AmlUiAction action;
        int key;

        ui_draw(state, status);
        last_second = ui_current_second();

        action = apply_test_automation(state);
        if (action == AML_UI_AUTO_REDRAW) {
            ui_sync_view_top(state);
            continue;
        }
        if (action >= 0) {
            return action;
        }

        wait_for_input_redraw(state, status, &last_second);
        key = getch();

        if (key == AML_KEY_ENTER) {
            if (ui_has_entries(state)) {
                return AML_UI_LAUNCH;
            }
            status = "No launcher entries available";
            continue;
        }
        if (key == AML_KEY_SLASH && ui_has_entries(state)) {
            prompt_search(state, &status);
            ui_sync_view_top(state);
            continue;
        }
        if (key == AML_KEY_EXTENDED || key == AML_KEY_EXTENDED_2) {
            action = handle_extended_key(state, getch());
            if (action >= 0) {
                return action;
            }
            ui_sync_view_top(state);
            status = "";
            continue;
        }
        if (key == AML_KEY_QUESTION) {
            ui_show_help_overlay(state);
            status = "";
            continue;
        }

        action = handle_hotkey(state, key);
        if (action >= 0) {
            return action;
        }
        if (isprint(key)) {
            status = "Unknown key";
        } else {
            status = "";
        }
    }
}
