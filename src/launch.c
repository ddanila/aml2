#include <direct.h>
#include <dos.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
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

int aml_check_direct_launch_entry(const AmlEntry *entry)
{
    char target[AML_MAX_PATH + AML_MAX_COMMAND + 2];

    if (!aml_directory_exists(entry->path)) {
        return AML_LAUNCH_BAD_PATH;
    }

    if (!aml_is_simple_launch(entry->command) ||
        !aml_has_program_extension(entry->command) ||
        stricmp(strrchr(entry->command, '.'), ".bat") == 0) {
        return AML_LAUNCH_DIRECT_UNSUPPORTED;
    }

    aml_join_path(target, sizeof(target), entry->path, entry->command);
    if (access(target, 0) != 0) {
        return AML_LAUNCH_TARGET_MISSING;
    }

    return AML_LAUNCH_READY;
}

static void aml_restore_directory(unsigned old_drive, const char *old_dir)
{
    unsigned drives;

    _dos_setdrive(old_drive, &drives);
    chdir(old_dir);
}

static int aml_switch_to_entry_directory(const AmlEntry *entry, unsigned *old_drive, char *old_dir, size_t old_dir_size)
{
    unsigned drives;

    _dos_getdrive(old_drive);
    if (getcwd(old_dir, old_dir_size) == NULL) {
        return AML_LAUNCH_BAD_PATH;
    }

    if (entry->path[0] == '\0') {
        return AML_LAUNCH_READY;
    }

    if (entry->path[1] == ':') {
        unsigned target_drive = ((unsigned char)entry->path[0] & 0xDF) - 'A' + 1;
        _dos_setdrive(target_drive, &drives);
    }

    if (chdir(entry->path) != 0) {
        aml_restore_directory(*old_drive, old_dir);
        return AML_LAUNCH_BAD_PATH;
    }

    return AML_LAUNCH_READY;
}

int aml_run_entry_child(const AmlEntry *entry, int force_shell)
{
    unsigned old_drive;
    char old_dir[AML_MAX_PATH];
    int rc;

    rc = aml_switch_to_entry_directory(entry, &old_drive, old_dir, sizeof(old_dir));
    if (rc != AML_LAUNCH_READY) {
        return rc;
    }

    if (force_shell) {
        rc = system(entry->command);
    } else {
        rc = spawnlp(P_WAIT, entry->command, entry->command, NULL);
    }

    aml_restore_directory(old_drive, old_dir);
    if (rc == -1) {
        return AML_LAUNCH_CHILD_FAILED;
    }

    return AML_LAUNCH_READY;
}
