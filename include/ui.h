#ifndef UI_H
#define UI_H

#include "aml.h"

typedef enum AmlUiAction {
    AML_UI_QUIT = 0,
    AML_UI_LAUNCH = 1,
    AML_UI_SAVE = 2,
    AML_UI_LAUNCH_DEBUG = 3,
    AML_UI_LAUNCH_CHILD_DIRECT = 4,
    AML_UI_LAUNCH_CHILD_SHELL = 5
} AmlUiAction;

void aml_ui_init(void);
void aml_ui_shutdown(void);
void aml_ui_draw(const AmlState *state, const char *status);
void aml_ui_show_message(const char *title, const char *line1, const char *line2, const char *line3);
void aml_ui_show_notice(const AmlState *state, const char *title, const char *line1, const char *line2, const char *line3);
void aml_ui_wait_for_ack(void);
AmlUiAction aml_ui_run(AmlState *state);

#endif
