#include <conio.h>
#include <stdio.h>
#include <string.h>

#include "aml.h"
#include "cfg.h"
#include "launch.h"
#include "ui.h"

static void aml_trace_event(const char *event_name)
{
    FILE *probe;
    FILE *fp;

    probe = fopen(AML_AUTO_FILE, "r");
    if (probe == NULL) {
        return;
    }
    fclose(probe);

    fp = fopen(AML_TRACE_FILE, "a");
    if (fp == NULL) {
        return;
    }
    fputs(event_name, fp);
    fputc('\n', fp);
    fclose(fp);
}

int main(void)
{
    AmlState state;
    int rc;
    int action;
    char status[AML_MAX_LINE + 1];

    state.entry_count = 0;
    state.selected = 0;

    aml_trace_event("launcher");

    rc = aml_load_config(&state, AML_CONFIG_FILE);

    aml_ui_init();
    if (rc != 0) {
        aml_ui_draw(&state, "launcher.cfg not found or invalid");
        aml_ui_shutdown();
        return 1;
    }

    action = aml_ui_run(&state);
    aml_ui_shutdown();

    if (action == AML_UI_LAUNCH &&
        state.selected >= 0 &&
        state.selected < state.entry_count) {
        rc = aml_write_run_request(&state.entries[state.selected], AML_RUN_FILE);
        if (rc != 0) {
            strcpy(status, "Failed to write AML2.RUN");
            aml_ui_init();
            aml_ui_draw(&state, status);
            getch();
            aml_ui_shutdown();
            return 1;
        }
        aml_trace_event("run_request");
    }

    return 0;
}
