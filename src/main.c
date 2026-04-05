#include <stdio.h>
#include <string.h>

#include "aml.h"
#include "cfg.h"
#include "launch.h"
#include "ui.h"

#if AML_TEST_HOOKS
static void trace_event(const char *event_name)
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

static int arg_is(const char *arg, const char *value)
{
    return strcmp(arg, value) == 0;
}

static void print_usage(void)
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

static void reset_launcher_state(AmlState *state)
{
    memset(state, 0, sizeof(*state));
}

static void show_notice_and_wait(const AmlState *state,
                                 const char *title,
                                 const char *line1,
                                 const char *line2,
                                 const char *line3)
{
    ui_show_notice(state, title, line1, line2, line3);
    ui_wait_for_ack();
}

static int handle_launch_check(const AmlState *state, AmlLaunchCheck rc)
{
    if (rc == AML_LAUNCH_STUB_MISSING) {
        show_notice_and_wait(
            state,
            "Missing AML.COM",
            "Launcher handoff needs AML.COM in this folder.",
            "Start AML.COM from the launcher directory.",
            ""
        );
        return 1;
    }

    if (rc == AML_LAUNCH_BAD_PATH) {
        show_notice_and_wait(
            state,
            "Game folder not found",
            "The configured working directory does not exist.",
            "Edit the entry and fix the path.",
            ""
        );
        return 1;
    }

    if (rc == AML_LAUNCH_TARGET_MISSING) {
        show_notice_and_wait(
            state,
            "Game file not found",
            "The configured command does not exist in that folder.",
            "Edit the entry and check the program name.",
            ""
        );
        return 1;
    }

    if (rc == AML_LAUNCH_DIRECT_UNSUPPORTED) {
        show_notice_and_wait(
            state,
            "Direct run unsupported",
            "This mode only supports plain .EXE and .COM targets.",
            "Use the shell option for .BAT files or command lines.",
            ""
        );
        return 1;
    }

    return 0;
}

static void show_initial_config_status(const AmlState *state, AmlCfgStatus rc)
{
    if (rc != 0) {
        ui_show_message(
            "No LAUNCHER.CFG yet",
            state->editor_mode ? "Use Ins to add an entry." : "Run AMLUI /E to create entries.",
            state->editor_mode ? "Use F2 to save the new configuration." : "",
            ""
        );
        ui_wait_for_ack();
        return;
    }

    if (state->entry_count <= 0) {
        ui_show_message(
            "Launcher config is empty",
            state->editor_mode ? "Use Ins to add an entry." : "Run AMLUI /E to add entries.",
            state->editor_mode ? "Use F2 to save changes." : "",
            ""
        );
        ui_wait_for_ack();
    }
}

static int parse_args(int argc, char **argv, AmlState *state)
{
    int i;
    int mode_set = 0;

    for (i = 1; i < argc; ++i) {
        if (arg_is(argv[i], "/v") || arg_is(argv[i], "/V")) {
            state->editor_mode = 0;
            mode_set = 1;
        } else
        if (arg_is(argv[i], "/e") || arg_is(argv[i], "/E")) {
            state->editor_mode = 1;
            mode_set = 1;
        } else
        if (arg_is(argv[i], "/s") || arg_is(argv[i], "/S")) {
            state->supervised = 1;
        } else if (arg_is(argv[i], "/?")) {
            print_usage();
            return AML_EXIT_OK;
        } else {
            print_usage();
            return AML_EXIT_ERROR;
        }
    }

    if (!mode_set) {
        print_usage();
        printf("\r\n");
        printf("Run AML.COM instead.\r\n");
        return AML_EXIT_ERROR;
    }

    return -1;
}

static void restore_mode_flags_after_load_error(AmlState *state)
{
    int editor_mode = state->editor_mode;
    int supervised = state->supervised;

    reset_launcher_state(state);
    state->editor_mode = editor_mode;
    state->supervised = supervised;
}

static void load_launcher_config(AmlState *state, AmlCfgStatus *cfg_status)
{
    *cfg_status = cfg_load(state, AML_CONFIG_FILE);
    if (*cfg_status != AML_CFG_OK) {
        restore_mode_flags_after_load_error(state);
    }
}

static int handle_save_action(AmlState *state)
{
    AmlCfgStatus rc;

    if (!state->editor_mode) {
        show_notice_and_wait(
            state,
            "Viewer mode",
            "Editing is disabled in the default mode.",
            "Run AMLUI /E to save changes.",
            ""
        );
        return 1;
    }

    rc = cfg_save(state, AML_CONFIG_FILE);
    if (rc != AML_CFG_OK) {
        show_notice_and_wait(
            state,
            "Save failed",
            "Could not write LAUNCHER.CFG.",
            "Check disk space and write permissions.",
            ""
        );
        return 1;
    }

    state->modified = 0;
    show_notice_and_wait(
        state,
        "Configuration saved",
        "LAUNCHER.CFG was updated successfully.",
        "",
        ""
    );
    return 1;
}

static AmlLaunchCheck check_action_launch_entry(const AmlState *state, AmlUiAction action)
{
    if (action == AML_UI_LAUNCH_CHILD_DIRECT) {
        return launch_check_direct_entry(&state->entries[state->selected]);
    }

    return launch_check_entry(&state->entries[state->selected]);
}

static int is_launch_action(AmlUiAction action)
{
    return action == AML_UI_LAUNCH ||
           action == AML_UI_LAUNCH_DEBUG ||
           action == AML_UI_LAUNCH_CHILD_DIRECT ||
           action == AML_UI_LAUNCH_CHILD_SHELL;
}

static int has_selected_entry(const AmlState *state)
{
    return state->selected >= 0 && state->selected < state->entry_count;
}

static int handle_child_launch_action(AmlState *state, AmlUiAction action)
{
    AmlLaunchCheck rc;

    if (action != AML_UI_LAUNCH_CHILD_DIRECT && action != AML_UI_LAUNCH_CHILD_SHELL) {
        return 0;
    }

    ui_shutdown();
    rc = launch_run_child(
        &state->entries[state->selected],
        action == AML_UI_LAUNCH_CHILD_SHELL
    );
    ui_init();
    if (rc == AML_LAUNCH_CHILD_FAILED) {
        show_notice_and_wait(
            state,
            "Run failed",
            "The selected program could not be started.",
            "Check the program name and DOS environment.",
            ""
        );
    }
    return 1;
}

static int handle_launch_action(AmlState *state, AmlUiAction action, int *launched)
{
    AmlLaunchCheck rc;

    if (!is_launch_action(action) || !has_selected_entry(state)) {
        return 0;
    }

    if ((action == AML_UI_LAUNCH || action == AML_UI_LAUNCH_DEBUG) && !state->supervised) {
        show_notice_and_wait(
            state,
            "Launch disabled",
            "AMLUI.EXE was started directly.",
            "Start with AML.COM to launch games.",
            ""
        );
        return 1;
    }

    rc = check_action_launch_entry(state, action);
    if (handle_launch_check(state, rc)) {
        return 1;
    }

    if (handle_child_launch_action(state, action)) {
        return 1;
    }

    if (launch_write_run_request(&state->entries[state->selected], AML_RUN_FILE) != 0) {
        show_notice_and_wait(
            state,
            "Failed to write AML2.RUN",
            "The launcher could not hand off the selected entry.",
            "Check write permissions and available disk space.",
            ""
        );
        return 1;
    }
#if AML_TEST_HOOKS
    trace_event("run_request");
#endif
    *launched = (action == AML_UI_LAUNCH_DEBUG) ? AML_EXIT_LAUNCH_DEBUG : AML_EXIT_LAUNCH;
    return 2;
}

static int handle_ui_action(AmlState *state, AmlUiAction action, int *launched)
{
    int rc;

    if (action == AML_UI_QUIT) {
        return 1;
    }

    if (action == AML_UI_SAVE && handle_save_action(state)) {
        return 0;
    }

    rc = handle_launch_action(state, action, launched);
    if (rc == 2) {
        return 1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    AmlCfgStatus cfg_status;
    int launched = AML_EXIT_OK;
    int rc;

    reset_launcher_state(&state);
    rc = parse_args(argc, argv, &state);
    if (rc >= 0) {
        return rc;
    }

#if AML_TEST_HOOKS
    trace_event("launcher");
#endif

    load_launcher_config(&state, &cfg_status);

    ui_init();
    show_initial_config_status(&state, cfg_status);

    for (;;) {
        if (handle_ui_action(&state, ui_run(&state), &launched)) {
            break;
        }
    }

    ui_shutdown();
    return launched;
}
