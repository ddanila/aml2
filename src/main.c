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

static AmlState state;

int main(void)
{
    int rc;
    int action;
    int launched = 0;

    state.entry_count = 0;
    state.selected = 0;
    state.modified = 0;

#if AML_TEST_HOOKS
    aml_trace_event("launcher");
#endif

    rc = aml_load_config(&state, AML_CONFIG_FILE);
    if (rc != 0) {
        state.entry_count = 0;
        state.selected = 0;
        state.modified = 0;
    }

    aml_ui_init();

    if (rc != 0) {
        aml_ui_show_message(
            "No launcher.cfg yet",
            "Use Ins to add an entry.",
            "Use F2 to save the new configuration.",
            "Press any key to continue."
        );
        getch();
    } else if (state.entry_count <= 0) {
        aml_ui_show_message(
            "Launcher config is empty",
            "Use Ins to add an entry.",
            "Use F2 to save changes.",
            "Press any key to continue."
        );
        getch();
    }

    for (;;) {
        action = aml_ui_run(&state);

        if (action == AML_UI_QUIT) {
            break;
        }

        if (action == AML_UI_SAVE) {
            rc = aml_save_config(&state, AML_CONFIG_FILE);
            if (rc != 0) {
                aml_ui_show_message(
                    "Save failed",
                    "Could not write LAUNCHER.CFG.",
                    "Check disk space and write permissions.",
                    "Press any key to continue."
                );
                getch();
            } else {
                state.modified = 0;
                aml_ui_show_message(
                    "Configuration saved",
                    "LAUNCHER.CFG was updated successfully.",
                    "",
                    "Press any key to continue."
                );
                getch();
            }
            continue;
        }

        if (action == AML_UI_LAUNCH &&
            state.selected >= 0 &&
            state.selected < state.entry_count) {
            rc = aml_write_run_request(&state.entries[state.selected], AML_RUN_FILE);
            if (rc != 0) {
                aml_ui_show_message(
                    "Failed to write AML2.RUN",
                    "The launcher could not hand off the selected entry.",
                    "Check write permissions and available disk space.",
                    "Press any key to continue."
                );
                getch();
                continue;
            }
#if AML_TEST_HOOKS
            aml_trace_event("run_request");
#endif
            launched = 1;
            break;
        }
    }

    aml_ui_shutdown();
    return launched ? 0 : 0;
}
