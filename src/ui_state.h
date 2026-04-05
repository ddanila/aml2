#ifndef UI_STATE_H
#define UI_STATE_H

#include "aml.h"

int aml_ui_has_entries(const AmlState *state);
int aml_ui_has_selection(const AmlState *state);
void aml_ui_select_index(AmlState *state, int index);
void aml_ui_select_first(AmlState *state);
void aml_ui_select_last(AmlState *state);
void aml_ui_select_prev_wrap(AmlState *state);
void aml_ui_select_next_wrap(AmlState *state);
void aml_ui_select_page_up(AmlState *state);
void aml_ui_select_page_down(AmlState *state);

#endif
