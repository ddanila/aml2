#include <conio.h>
#include <stdio.h>
#include <string.h>

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

static int aml_arg_is(const char *arg, const char *value)
{
    return strcmp(arg, value) == 0;
}

static void aml_print_usage(void)
{
    printf("AMLUI usage:\r\n");
    printf("  AMLUI /V | /E | /?\r\n");
    printf("\r\n");
    printf("  /V   View mode.\r\n");
    printf("  /E   Enable editor mode.\r\n");
    printf("  /?   Show this help.\r\n");
    printf("\r\n");
    printf("Start AML.COM for the normal launcher loop.\r\n");
}

int main(int argc, char **argv)
{
    int rc;
    int action;
    int launched = AML_EXIT_OK;
    int i;
    int mode_set = 0;

    state.entry_count = 0;
    state.selected = 0;
    state.view_top = 0;
    state.modified = 0;
    state.editor_mode = 0;
    state.supervised = 0;

    for (i = 1; i < argc; ++i) {
        if (aml_arg_is(argv[i], "/v") || aml_arg_is(argv[i], "/V")) {
            state.editor_mode = 0;
            mode_set = 1;
        } else
        if (aml_arg_is(argv[i], "/e") || aml_arg_is(argv[i], "/E")) {
            state.editor_mode = 1;
            mode_set = 1;
        } else
        if (aml_arg_is(argv[i], "/s") || aml_arg_is(argv[i], "/S")) {
            state.supervised = 1;
        } else if (aml_arg_is(argv[i], "/?")) {
            aml_print_usage();
            return AML_EXIT_OK;
        } else {
            aml_print_usage();
            return AML_EXIT_ERROR;
        }
    }

    if (!mode_set) {
        aml_print_usage();
        printf("\r\n");
        printf("Run AML.COM instead.\r\n");
        return AML_EXIT_ERROR;
    }

#if AML_TEST_HOOKS
    aml_trace_event("launcher");
#endif

    rc = aml_load_config(&state, AML_CONFIG_FILE);
    if (rc != 0) {
        state.entry_count = 0;
        state.selected = 0;
        state.view_top = 0;
        state.modified = 0;
    }

    aml_ui_init();

    if (rc != 0) {
        aml_ui_show_message(
            "No launcher.cfg yet",
            state.editor_mode ? "Use Ins to add an entry." : "Run AMLUI /E to create entries.",
            state.editor_mode ? "Use F2 to save the new configuration." : "",
            ""
        );
        aml_ui_wait_for_ack();
    } else if (state.entry_count <= 0) {
        aml_ui_show_message(
            "Launcher config is empty",
            state.editor_mode ? "Use Ins to add an entry." : "Run AMLUI /E to add entries.",
            state.editor_mode ? "Use F2 to save changes." : "",
            ""
        );
        aml_ui_wait_for_ack();
    }

    for (;;) {
        action = aml_ui_run(&state);

        if (action == AML_UI_QUIT) {
            break;
        }

        if (action == AML_UI_SAVE) {
            if (!state.editor_mode) {
                aml_ui_show_notice(
                    &state,
                    "Viewer mode",
                    "Editing is disabled in the default mode.",
                    "Run AMLUI /E to save changes.",
                    ""
                );
                aml_ui_wait_for_ack();
                continue;
            }
            rc = aml_save_config(&state, AML_CONFIG_FILE);
            if (rc != 0) {
                aml_ui_show_notice(
                    &state,
                    "Save failed",
                    "Could not write LAUNCHER.CFG.",
                    "Check disk space and write permissions.",
                    ""
                );
                aml_ui_wait_for_ack();
            } else {
                state.modified = 0;
                aml_ui_show_notice(
                    &state,
                    "Configuration saved",
                    "LAUNCHER.CFG was updated successfully.",
                    "",
                    ""
                );
                aml_ui_wait_for_ack();
            }
            continue;
        }

        if ((action == AML_UI_LAUNCH || action == AML_UI_LAUNCH_DEBUG ||
             action == AML_UI_LAUNCH_CHILD_DIRECT || action == AML_UI_LAUNCH_CHILD_SHELL) &&
            state.selected >= 0 &&
            state.selected < state.entry_count) {
            if ((action == AML_UI_LAUNCH || action == AML_UI_LAUNCH_DEBUG) && !state.supervised) {
                aml_ui_show_notice(
                    &state,
                    "Launch disabled",
                    "AMLUI.EXE was started directly.",
                    "Start with AML.COM to launch games.",
                    ""
                );
                aml_ui_wait_for_ack();
                continue;
            }
            if (action == AML_UI_LAUNCH_CHILD_DIRECT) {
                rc = aml_check_direct_launch_entry(&state.entries[state.selected]);
            } else {
                rc = aml_check_launch_entry(&state.entries[state.selected]);
            }
            if (rc == AML_LAUNCH_STUB_MISSING) {
                aml_ui_show_notice(
                    &state,
                    "Missing AML.COM",
                    "Launcher handoff needs AML.COM in this folder.",
                    "Start AML.COM from the launcher directory.",
                    ""
                );
                aml_ui_wait_for_ack();
                continue;
            }
            if (rc == AML_LAUNCH_BAD_PATH) {
                aml_ui_show_notice(
                    &state,
                    "Game folder not found",
                    "The configured working directory does not exist.",
                    "Edit the entry and fix the path.",
                    ""
                );
                aml_ui_wait_for_ack();
                continue;
            }
            if (rc == AML_LAUNCH_TARGET_MISSING) {
                aml_ui_show_notice(
                    &state,
                    "Game file not found",
                    "The configured command does not exist in that folder.",
                    "Edit the entry and check the program name.",
                    ""
                );
                aml_ui_wait_for_ack();
                continue;
            }
            if (rc == AML_LAUNCH_DIRECT_UNSUPPORTED) {
                aml_ui_show_notice(
                    &state,
                    "Direct run unsupported",
                    "This mode only supports plain .EXE and .COM targets.",
                    "Use the shell option for .BAT files or command lines.",
                    ""
                );
                aml_ui_wait_for_ack();
                continue;
            }

            if (action == AML_UI_LAUNCH_CHILD_DIRECT || action == AML_UI_LAUNCH_CHILD_SHELL) {
                aml_ui_shutdown();
                rc = aml_run_entry_child(
                    &state.entries[state.selected],
                    action == AML_UI_LAUNCH_CHILD_SHELL
                );
                aml_ui_init();
                if (rc == AML_LAUNCH_CHILD_FAILED) {
                    aml_ui_show_notice(
                        &state,
                        "Run failed",
                        "The selected program could not be started.",
                        "Check the program name and DOS environment.",
                        ""
                    );
                    aml_ui_wait_for_ack();
                }
                continue;
            }

            rc = aml_write_run_request(&state.entries[state.selected], AML_RUN_FILE);
            if (rc != 0) {
                aml_ui_show_notice(
                    &state,
                    "Failed to write AML2.RUN",
                    "The launcher could not hand off the selected entry.",
                    "Check write permissions and available disk space.",
                    ""
                );
                aml_ui_wait_for_ack();
                continue;
            }
#if AML_TEST_HOOKS
            aml_trace_event("run_request");
#endif
            launched = (action == AML_UI_LAUNCH_DEBUG) ? AML_EXIT_LAUNCH_DEBUG : AML_EXIT_LAUNCH;
            break;
        }
    }

    aml_ui_shutdown();
    return launched;
}
