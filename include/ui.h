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

void ui_init(void);
void ui_shutdown(void);
void ui_draw(const AmlState *state, const char *status);
void ui_show_message(const char *title, const char *line1, const char *line2, const char *line3);
void ui_show_notice(const AmlState *state, const char *title, const char *line1, const char *line2, const char *line3);
void ui_wait_for_ack(void);
AmlUiAction ui_run(AmlState *state);

#define aml_ui_init ui_init
#define aml_ui_shutdown ui_shutdown
#define aml_ui_draw ui_draw
#define aml_ui_show_message ui_show_message
#define aml_ui_show_notice ui_show_notice
#define aml_ui_wait_for_ack ui_wait_for_ack
#define aml_ui_run ui_run

#endif
