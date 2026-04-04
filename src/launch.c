#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <string.h>

#include "launch.h"

static int aml_is_simple_launch(const char *command)
{
    while (*command != '\0') {
        if (*command == ' ' || *command == '\t' ||
            *command == '"' || *command == '%' ||
            *command == '&' || *command == '|' ||
            *command == '<' || *command == '>') {
            return 0;
        }
        ++command;
    }

    return 1;
}

static int aml_has_program_extension(const char *command)
{
    const char *dot = strrchr(command, '.');

    if (dot == NULL) {
        return 0;
    }

    return stricmp(dot, ".exe") == 0 ||
           stricmp(dot, ".com") == 0 ||
           stricmp(dot, ".bat") == 0;
}

static int aml_directory_exists(const char *path)
{
    char original[AML_MAX_PATH];

    if (path[0] == '\0') {
        return 1;
    }

    if (getcwd(original, sizeof(original)) == NULL) {
        return 0;
    }

    if (chdir(path) != 0) {
        return 0;
    }

    chdir(original);
    return 1;
}

static void aml_join_path(char *out, size_t out_size, const char *dir, const char *name)
{
    size_t len;

    out[0] = '\0';
    if (dir[0] == '\0') {
        strncpy(out, name, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }

    strncpy(out, dir, out_size - 1);
    out[out_size - 1] = '\0';

    len = strlen(out);
    if (len > 0 && out[len - 1] != '\\' && out[len - 1] != '/') {
        if (len + 1 < out_size) {
            out[len] = '\\';
            out[len + 1] = '\0';
            ++len;
        }
    }

    if (len < out_size - 1) {
        strncat(out, name, out_size - len - 1);
    }
}

int aml_write_run_request(const AmlEntry *entry, const char *path)
{
    FILE *fp;

    fp = fopen(path, "w");
    if (fp == NULL) {
        return 1;
    }

    fputs(entry->command, fp);
    fputc('\n', fp);
    fputs(entry->path, fp);
    fputc('\n', fp);

    fclose(fp);
    return 0;
}

int aml_clear_run_request(const char *path)
{
    if (remove(path) != 0) {
        return 1;
    }

    return 0;
}

int aml_check_launch_entry(const AmlEntry *entry)
{
    char target[AML_MAX_PATH + AML_MAX_COMMAND + 2];

    if (access(AML_STUB_FILE, 0) != 0) {
        return AML_LAUNCH_STUB_MISSING;
    }

    if (!aml_directory_exists(entry->path)) {
        return AML_LAUNCH_BAD_PATH;
    }

    if (!aml_is_simple_launch(entry->command) ||
        !aml_has_program_extension(entry->command)) {
        return AML_LAUNCH_READY;
    }

    aml_join_path(target, sizeof(target), entry->path, entry->command);
    if (access(target, 0) != 0) {
        return AML_LAUNCH_TARGET_MISSING;
    }

    return AML_LAUNCH_READY;
}
