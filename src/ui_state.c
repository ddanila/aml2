#include "ui_int.h"
#include "ui_state.h"

int aml_ui_has_entries(const AmlState *state)
{
    return state->entry_count > 0;
}

int aml_ui_has_selection(const AmlState *state)
{
    return aml_ui_has_entries(state) &&
           state->selected >= 0 &&
           state->selected < state->entry_count;
}

void aml_ui_select_index(AmlState *state, int index)
{
    if (!aml_ui_has_entries(state)) {
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index >= state->entry_count) {
        index = state->entry_count - 1;
    }
    state->selected = index;
}

void aml_ui_select_first(AmlState *state)
{
    aml_ui_select_index(state, 0);
}

void aml_ui_select_last(AmlState *state)
{
    aml_ui_select_index(state, state->entry_count - 1);
}

void aml_ui_select_prev_wrap(AmlState *state)
{
    if (!aml_ui_has_entries(state)) {
        return;
    }
    if (state->selected > 0) {
        state->selected--;
    } else {
        state->selected = state->entry_count - 1;
    }
}

void aml_ui_select_next_wrap(AmlState *state)
{
    if (!aml_ui_has_entries(state)) {
        return;
    }
    if (state->selected < state->entry_count - 1) {
        state->selected++;
    } else {
        state->selected = 0;
    }
}

void aml_ui_select_page_up(AmlState *state)
{
    aml_ui_select_index(state, state->selected - AML_UI_LIST_ROWS);
}

void aml_ui_select_page_down(AmlState *state)
{
    aml_ui_select_index(state, state->selected + AML_UI_LIST_ROWS);
}
