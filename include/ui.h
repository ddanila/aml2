#ifndef UI_H
#define UI_H

#include "aml.h"

enum AmlUiAction {
    AML_UI_QUIT = 0,
    AML_UI_LAUNCH = 1
};

void aml_ui_init(void);
void aml_ui_shutdown(void);
void aml_ui_draw(const AmlState *state, const char *status);
int aml_ui_run(AmlState *state);

#endif
