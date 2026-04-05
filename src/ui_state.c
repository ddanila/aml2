#include "ui_int.h"
#include "ui_state.h"

int ui_has_entries(const AmlState *state)
{
    return state->entry_count > 0;
}

int ui_has_selection(const AmlState *state)
{
    return ui_has_entries(state) &&
           state->selected >= 0 &&
           state->selected < state->entry_count;
}

void ui_select_index(AmlState *state, int index)
{
    if (!ui_has_entries(state)) {
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

void ui_select_first(AmlState *state)
{
    ui_select_index(state, 0);
}

void ui_select_last(AmlState *state)
{
    ui_select_index(state, state->entry_count - 1);
}

void ui_select_prev_wrap(AmlState *state)
{
    if (!ui_has_entries(state)) {
        return;
    }
    if (state->selected > 0) {
        state->selected--;
    } else {
        state->selected = state->entry_count - 1;
    }
}

void ui_select_next_wrap(AmlState *state)
{
    if (!ui_has_entries(state)) {
        return;
    }
    if (state->selected < state->entry_count - 1) {
        state->selected++;
    } else {
        state->selected = 0;
    }
}

void ui_select_page_up(AmlState *state)
{
    ui_select_index(state, state->selected - UI_LIST_VISIBLE);
}

void ui_select_page_down(AmlState *state)
{
    ui_select_index(state, state->selected + UI_LIST_VISIBLE);
}
