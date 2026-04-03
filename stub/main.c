#include <direct.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aml.h"

#if AML_TEST_HOOKS
static void aml_trace_event(const char *event_name)
{
    FILE *fp;

    fp = fopen(AML_TRACE_FILE, "a");
    if (fp == NULL) {
        return;
    }
    fputs(event_name, fp);
    fputc('\n', fp);
    fclose(fp);
}
#endif

static void aml_trim_newline(char *line)
{
    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[len - 1] = '\0';
        len--;
    }
}

static int aml_select_drive(const char *path)
{
    if (path[0] != '\0' && path[1] == ':') {
        char drive = path[0];

        if (drive >= 'a' && drive <= 'z') {
            drive = (char)(drive - 'a' + 'A');
        }
        if (drive >= 'A' && drive <= 'Z') {
            return _chdrive(drive - 'A' + 1);
        }
    }

    return 0;
}

static int aml_read_run_request(char *command, char *path)
{
    FILE *fp;

    fp = fopen(AML_RUN_FILE, "r");
    if (fp == NULL) {
        return 1;
    }

    command[0] = '\0';
    path[0] = '\0';

    if (fgets(command, AML_MAX_COMMAND, fp) == NULL) {
        fclose(fp);
        return 1;
    }
    aml_trim_newline(command);

    if (fgets(path, AML_MAX_PATH, fp) != NULL) {
        aml_trim_newline(path);
    }

    fclose(fp);

    if (command[0] == '\0') {
        return 1;
    }

    return 0;
}

static int aml_run_command(const char *command, const char *path)
{
    const char *shell;

    if (path[0] != '\0') {
        if (aml_select_drive(path) != 0) {
            return 1;
        }
        if (chdir(path) != 0) {
            return 1;
        }
    }

    shell = getenv("COMSPEC");
    if (shell == NULL || shell[0] == '\0') {
        shell = "COMMAND.COM";
    }

    return spawnlp(P_WAIT, shell, shell, "/C", command, NULL);
}

int main(void)
{
    for (;;) {
        char command[AML_MAX_COMMAND];
        char path[AML_MAX_PATH];
        int rc;

        remove(AML_RUN_FILE);

        rc = spawnlp(P_WAIT, AML_LAUNCHER_EXE, AML_LAUNCHER_EXE, NULL);
        if (rc == -1) {
            puts("AMLSTUB: failed to start AML2.EXE");
            return 1;
        }

        if (aml_read_run_request(command, path) != 0) {
            return 0;
        }

#if AML_TEST_HOOKS
        aml_trace_event("stub_read");
#endif
        remove(AML_RUN_FILE);

#if AML_TEST_HOOKS
        aml_trace_event("stub_launch");
#endif
        rc = aml_run_command(command, path);
        if (rc != 0) {
            puts("AMLSTUB: failed to launch selected command");
            puts(command);
            return 1;
        }
#if AML_TEST_HOOKS
        aml_trace_event("stub_return");
#endif
    }
}
