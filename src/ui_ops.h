#ifndef UI_OPS_H
#define UI_OPS_H

#include "ui.h"

void aml_ui_show_details_overlay(const AmlState *state);
void aml_ui_show_help_overlay(const AmlState *state);
AmlUiAction aml_ui_show_debug_run_menu(const AmlState *state);
void aml_ui_insert_entry(AmlState *state);
void aml_ui_edit_entry(AmlState *state);
void aml_ui_delete_entry(AmlState *state);
void aml_ui_delete_entry_with_confirm(AmlState *state);
void aml_ui_move_entry_up(AmlState *state);
void aml_ui_move_entry_down(AmlState *state);
AmlUiAction aml_ui_apply_automation(AmlState *state);

#endif
