#include <stdlib.h>
#include <string.h>

#include "ui_int.h"
#include "ui_ops.h"
#include "ui_state.h"

AmlUiAction aml_ui_apply_automation(AmlState *state)
{
    char line[AML_MAX_LINE + 1];

    if (!aml_ui_read_auto_line(line, sizeof(line))) {
        return (AmlUiAction)AML_UI_AUTO_NONE;
    }

    if (strncmp(line, "launch ", 7) == 0) {
        int index = atoi(line + 7);
        if (index >= 0 && index < state->entry_count) {
            aml_ui_select_index(state, index);
            aml_ui_trace_event("auto_launch");
            return AML_UI_LAUNCH;
        }
        aml_ui_trace_event("auto_bad_launch");
        return AML_UI_QUIT;
    }

    if (strcmp(line, "up") == 0 && state->entry_count > 0) {
        aml_ui_select_prev_wrap(state);
        aml_ui_trace_event("auto_up");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "down") == 0 && state->entry_count > 0) {
        aml_ui_select_next_wrap(state);
        aml_ui_trace_event("auto_down");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "home") == 0 && state->entry_count > 0) {
        aml_ui_select_first(state);
        aml_ui_trace_event("auto_home");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "end") == 0 && state->entry_count > 0) {
        aml_ui_select_last(state);
        aml_ui_trace_event("auto_end");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "pgup") == 0 && state->entry_count > 0) {
        aml_ui_select_page_up(state);
        aml_ui_trace_event("auto_pgup");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strcmp(line, "pgdn") == 0 && state->entry_count > 0) {
        aml_ui_select_page_down(state);
        aml_ui_trace_event("auto_pgdn");
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strncmp(line, "search ", 7) == 0) {
        int index = aml_ui_find_match(state, line + 7);
        if (index >= 0) {
            aml_ui_select_index(state, index);
            aml_ui_trace_event("auto_search");
        } else {
            aml_ui_trace_event("auto_bad_search");
        }
        return (AmlUiAction)AML_UI_AUTO_REDRAW;
    }

    if (strncmp(line, "hotkey ", 7) == 0) {
        int index = aml_ui_hotkey_index(line[7]);
        if (index >= 0 && index < state->entry_count) {
            aml_ui_select_index(state, index);
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
