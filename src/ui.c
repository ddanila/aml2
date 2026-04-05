#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"
#include "ui_int.h"

static AmlUiAction aml_ui_apply_automation(AmlState *state)
{
    char line[AML_MAX_LINE + 1];

    if (!aml_ui_read_auto_line(line, sizeof(line))) {
        return (AmlUiAction)AML_UI_AUTO_NONE;
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
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "down") == 0 && state->entry_count > 0) {
        if (state->selected < state->entry_count - 1) {
            state->selected++;
        } else {
            state->selected = 0;
        }
        aml_ui_trace_event("auto_down");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "home") == 0 && state->entry_count > 0) {
        state->selected = 0;
        aml_ui_trace_event("auto_home");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "end") == 0 && state->entry_count > 0) {
        state->selected = state->entry_count - 1;
        aml_ui_trace_event("auto_end");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "pgup") == 0 && state->entry_count > 0) {
        state->selected -= AML_UI_LIST_ROWS;
        if (state->selected < 0) {
            state->selected = 0;
        }
        aml_ui_trace_event("auto_pgup");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "pgdn") == 0 && state->entry_count > 0) {
        state->selected += AML_UI_LIST_ROWS;
        if (state->selected >= state->entry_count) {
            state->selected = state->entry_count - 1;
        }
        aml_ui_trace_event("auto_pgdn");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strncmp(line, "search ", 7) == 0) {
        int index = aml_ui_find_match(state, line + 7);
        if (index >= 0) {
            state->selected = index;
            aml_ui_trace_event("auto_search");
        } else {
            aml_ui_trace_event("auto_bad_search");
        }
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strncmp(line, "hotkey ", 7) == 0) {
        int index = aml_ui_hotkey_index(line[7]);
        if (index >= 0 && index < state->entry_count) {
            state->selected = index;
            aml_ui_trace_event("auto_hotkey");
        } else {
            aml_ui_trace_event("auto_bad_hotkey");
        }
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "help") == 0) {
        aml_ui_trace_event("auto_help");
        aml_ui_show_help_overlay(state);
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "move_up") == 0) {
        aml_ui_move_entry_up(state);
        aml_ui_trace_event("auto_move_up");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "move_down") == 0) {
        aml_ui_move_entry_down(state);
        aml_ui_trace_event("auto_move_down");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "delete") == 0) {
        aml_ui_delete_entry(state);
        aml_ui_trace_event("auto_delete");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
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
