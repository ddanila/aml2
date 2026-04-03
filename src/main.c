#include <conio.h>
#include <stdio.h>

#include "aml.h"
#include "cfg.h"
#include "launch.h"
#include "ui.h"

#if AML_TEST_HOOKS
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
#endif

int main(void)
{
    AmlState state;
    int rc;
    int action;

    state.entry_count = 0;
    state.selected = 0;

#if AML_TEST_HOOKS
    aml_trace_event("launcher");
#endif

    rc = aml_load_config(&state, AML_CONFIG_FILE);

    aml_ui_init();
    if (rc != 0) {
        aml_ui_show_message(
            "Launcher config not available",
            "Expected file: LAUNCHER.CFG",
            "Create a config and start AMLSTUB.COM again.",
            "Press any key to exit."
        );
        getch();
        aml_ui_shutdown();
        return 1;
    }

    if (state.entry_count <= 0) {
        aml_ui_show_message(
            "No launcher entries found",
            "LAUNCHER.CFG loaded, but there are no usable items.",
            "Expected format: name|command|path",
            "Press any key to exit."
        );
        getch();
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
            aml_ui_show_message(
                "Failed to write AML2.RUN",
                "The launcher could not hand off the selected entry.",
                "Check write permissions and available disk space.",
                "Press any key to exit."
            );
            getch();
            aml_ui_shutdown();
            return 1;
        }
#if AML_TEST_HOOKS
        aml_trace_event("run_request");
#endif
    }

    return 0;
}
