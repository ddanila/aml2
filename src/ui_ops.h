#ifndef UI_OPS_H
#define UI_OPS_H

#include "ui.h"

void ui_show_details_overlay(const AmlState *state);
void ui_show_help_overlay(const AmlState *state);
AmlUiAction ui_show_debug_run_menu(const AmlState *state);
void ui_insert_entry(AmlState *state);
void ui_edit_entry(AmlState *state);
void ui_delete_entry(AmlState *state);
void ui_delete_entry_with_confirm(AmlState *state);
void ui_move_entry_up(AmlState *state);
void ui_move_entry_down(AmlState *state);
AmlUiAction ui_apply_automation(AmlState *state);

#endif
