#include <conio.h>
#include <ctype.h>
#include <dos.h>

#include "ui.h"
#include "ui_int.h"
#include "ui_ops.h"
#include "ui_state.h"

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
                aml_ui_select_index(state, match);
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
            aml_ui_select_index(state, match);
            *status = "Search matched";
        } else if (len == 0) {
            *status = "Search cleared";
        } else {
            *status = "No matching entry";
        }
    }
}

AmlUiAction aml_ui_run(AmlState *state)
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
            AmlUiAction auto_action = aml_ui_apply_automation(state);
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
            if (aml_ui_has_entries(state)) {
                return AML_UI_LAUNCH;
            }
            status = "No launcher entries available";
            continue;
        }

        if (key == AML_KEY_SLASH && aml_ui_has_entries(state)) {
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
                if (aml_ui_has_entries(state)) {
                    int action = aml_ui_show_debug_run_menu(state);
                    if (action >= 0) {
                        return action;
                    }
                }
            } else if (key == AML_KEY_F10) {
                return AML_UI_QUIT;
            } else if (!aml_ui_has_entries(state)) {
                continue;
            } else if (key == AML_KEY_HOME) {
                aml_ui_select_first(state);
            } else if (key == AML_KEY_END) {
                aml_ui_select_last(state);
            } else if (key == AML_KEY_PGUP) {
                aml_ui_select_page_up(state);
            } else if (key == AML_KEY_PGDN) {
                aml_ui_select_page_down(state);
            } else if (key == AML_KEY_UP) {
                aml_ui_select_prev_wrap(state);
            } else if (key == AML_KEY_DOWN) {
                aml_ui_select_next_wrap(state);
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
            aml_ui_select_index(state, hotkey_index);
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
