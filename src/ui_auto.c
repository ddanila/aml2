#include <stdlib.h>
#include <string.h>

#include "ui_int.h"
#include "ui_ops.h"
#include "ui_state.h"

AmlUiAction ui_apply_automation(AmlState *state)
{
    char line[AML_MAX_LINE + 1];

    if (!ui_read_auto_line(line, sizeof(line))) {
        return (AmlUiAction)UI_AUTO_NONE;
    }

    if (strncmp(line, "launch ", 7) == 0) {
        int index = atoi(line + 7);
        if (index >= 0 && index < state->entry_count) {
            ui_select_index(state, index);
            ui_trace_event("auto_launch");
            return AML_UI_LAUNCH;
        }
        ui_trace_event("auto_bad_launch");
        return AML_UI_QUIT;
    }

    if (strcmp(line, "up") == 0 && state->entry_count > 0) {
        ui_select_prev_wrap(state);
        ui_trace_event("auto_up");
        return (AmlUiAction)UI_AUTO_REDRAW;
    }

    if (strcmp(line, "down") == 0 && state->entry_count > 0) {
        ui_select_next_wrap(state);
        ui_trace_event("auto_down");
        return (AmlUiAction)UI_AUTO_REDRAW;
    }

    if (strcmp(line, "home") == 0 && state->entry_count > 0) {
        ui_select_first(state);
        ui_trace_event("auto_home");
        return (AmlUiAction)UI_AUTO_REDRAW;
    }

    if (strcmp(line, "end") == 0 && state->entry_count > 0) {
        ui_select_last(state);
        ui_trace_event("auto_end");
        return (AmlUiAction)UI_AUTO_REDRAW;
    }

    if (strcmp(line, "pgup") == 0 && state->entry_count > 0) {
        ui_select_page_up(state);
        ui_trace_event("auto_pgup");
        return (AmlUiAction)UI_AUTO_REDRAW;
    }

    if (strcmp(line, "pgdn") == 0 && state->entry_count > 0) {
        ui_select_page_down(state);
        ui_trace_event("auto_pgdn");
        return (AmlUiAction)UI_AUTO_REDRAW;
    }

    if (strncmp(line, "search ", 7) == 0) {
        int index = ui_find_match(state, line + 7);
        if (index >= 0) {
            ui_select_index(state, index);
            ui_trace_event("auto_search");
        } else {
            ui_trace_event("auto_bad_search");
        }
        return (AmlUiAction)UI_AUTO_REDRAW;
    }

    if (strcmp(line, "help") == 0) {
        ui_trace_event("auto_help");
        ui_show_help_overlay(state);
        return (AmlUiAction)UI_AUTO_REDRAW;
    }

    if (strcmp(line, "move_up") == 0) {
        ui_move_entry_up(state);
        ui_trace_event("auto_move_up");
        return (AmlUiAction)UI_AUTO_REDRAW;
    }

    if (strcmp(line, "move_down") == 0) {
        ui_move_entry_down(state);
        ui_trace_event("auto_move_down");
        return (AmlUiAction)UI_AUTO_REDRAW;
    }

    if (strcmp(line, "delete") == 0) {
        ui_delete_entry(state);
        ui_trace_event("auto_delete");
        return (AmlUiAction)UI_AUTO_REDRAW;
    }

    if (strcmp(line, "save") == 0) {
        ui_trace_event("auto_save");
        return AML_UI_SAVE;
    }

    if (strcmp(line, "quit") == 0) {
        ui_trace_event("auto_quit");
        return AML_UI_QUIT;
    }

    ui_trace_event("auto_unknown");
    return AML_UI_QUIT;
}
