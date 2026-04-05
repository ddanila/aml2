#include <direct.h>
#include <dos.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "launch.h"

typedef enum AmlLaunchTargetKind {
    AML_TARGET_COMPLEX = 0,
    AML_TARGET_PROGRAM = 1,
    AML_TARGET_BATCH = 2
} AmlLaunchTargetKind;

typedef struct AmlLaunchContext {
    unsigned drive;
    char dir[AML_MAX_PATH];
} AmlLaunchContext;

static int is_simple_launch(const char *command)
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

static AmlLaunchTargetKind classify_target(const char *command)
{
    const char *dot = strrchr(command, '.');

    if (!is_simple_launch(command) || dot == NULL) {
        return AML_TARGET_COMPLEX;
    }

    if (stricmp(dot, ".exe") == 0 || stricmp(dot, ".com") == 0) {
        return AML_TARGET_PROGRAM;
    }
    if (stricmp(dot, ".bat") == 0) {
        return AML_TARGET_BATCH;
    }

    return AML_TARGET_COMPLEX;
}

static int directory_exists(const char *path)
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

static void join_path(char *out, size_t out_size, const char *dir, const char *name)
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

static AmlLaunchCheck check_entry_directory(const AmlEntry *entry)
{
    if (!directory_exists(entry->path)) {
        return AML_LAUNCH_BAD_PATH;
    }

    return AML_LAUNCH_READY;
}

static AmlLaunchCheck check_existing_target(const AmlEntry *entry)
{
    char target[AML_MAX_PATH + AML_MAX_COMMAND + 2];

    join_path(target, sizeof(target), entry->path, entry->command);
    if (access(target, 0) != 0) {
        return AML_LAUNCH_TARGET_MISSING;
    }

    return AML_LAUNCH_READY;
}

static AmlLaunchCheck check_stub_present(void)
{
    if (access(AML_STUB_FILE, 0) != 0) {
        return AML_LAUNCH_STUB_MISSING;
    }

    return AML_LAUNCH_READY;
}

static AmlLaunchCheck check_entry_target(const AmlEntry *entry, AmlLaunchTargetKind kind)
{
    if (kind == AML_TARGET_COMPLEX) {
        return AML_LAUNCH_READY;
    }

    return check_existing_target(entry);
}

static AmlLaunchCheck validate_entry(const AmlEntry *entry, int require_stub, int require_program)
{
    AmlLaunchTargetKind kind;
    AmlLaunchCheck rc;

    if (require_stub) {
        rc = check_stub_present();
        if (rc != AML_LAUNCH_READY) {
            return rc;
        }
    }

    rc = check_entry_directory(entry);
    if (rc != AML_LAUNCH_READY) {
        return rc;
    }

    kind = classify_target(entry->command);
    if (require_program && kind != AML_TARGET_PROGRAM) {
        return AML_LAUNCH_DIRECT_UNSUPPORTED;
    }

    return check_entry_target(entry, kind);
}

static void restore_directory(const AmlLaunchContext *ctx)
{
    unsigned drives;

    _dos_setdrive(ctx->drive, &drives);
    chdir(ctx->dir);
}

static AmlLaunchCheck capture_directory_context(AmlLaunchContext *ctx)
{
    _dos_getdrive(&ctx->drive);
    if (getcwd(ctx->dir, sizeof(ctx->dir)) == NULL) {
        return AML_LAUNCH_BAD_PATH;
    }

    return AML_LAUNCH_READY;
}

static void switch_to_entry_drive(const char *path)
{
    unsigned drives;

    if (path[0] != '\0' && path[1] == ':') {
        unsigned target_drive = ((unsigned char)path[0] & 0xDF) - 'A' + 1;
        _dos_setdrive(target_drive, &drives);
    }
}

static AmlLaunchCheck change_to_entry_directory(const AmlEntry *entry)
{
    if (entry->path[0] == '\0') {
        return AML_LAUNCH_READY;
    }

    switch_to_entry_drive(entry->path);
    if (chdir(entry->path) != 0) {
        return AML_LAUNCH_BAD_PATH;
    }

    return AML_LAUNCH_READY;
}

static AmlLaunchCheck switch_to_entry_directory(const AmlEntry *entry, AmlLaunchContext *ctx)
{
    AmlLaunchCheck rc;

    rc = capture_directory_context(ctx);
    if (rc != AML_LAUNCH_READY) {
        return rc;
    }

    rc = change_to_entry_directory(entry);
    if (rc != AML_LAUNCH_READY) {
        restore_directory(ctx);
        return rc;
    }

    return AML_LAUNCH_READY;
}

static AmlLaunchCheck execute_child_direct(const AmlEntry *entry)
{
    if (spawnlp(P_WAIT, entry->command, entry->command, NULL) == -1) {
        return AML_LAUNCH_CHILD_FAILED;
    }

    return AML_LAUNCH_READY;
}

static AmlLaunchCheck execute_child_shell(const AmlEntry *entry)
{
    if (system(entry->command) == -1) {
        return AML_LAUNCH_CHILD_FAILED;
    }

    return AML_LAUNCH_READY;
}

static AmlLaunchCheck execute_child_command(const AmlEntry *entry, int force_shell)
{
    if (force_shell) {
        return execute_child_shell(entry);
    }

    return execute_child_direct(entry);
}

static AmlLaunchCheck run_child_in_entry_directory(const AmlEntry *entry, int force_shell)
{
    AmlLaunchContext ctx;
    AmlLaunchCheck rc;

    rc = switch_to_entry_directory(entry, &ctx);
    if (rc != AML_LAUNCH_READY) {
        return rc;
    }

    rc = execute_child_command(entry, force_shell);
    restore_directory(&ctx);
    return rc;
}

int launch_write_run_request(const AmlEntry *entry, const char *path)
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

int launch_clear_run_request(const char *path)
{
    if (remove(path) != 0) {
        return 1;
    }

    return 0;
}

AmlLaunchCheck launch_check_entry(const AmlEntry *entry)
{
    return validate_entry(entry, 1, 0);
}

AmlLaunchCheck launch_check_direct_entry(const AmlEntry *entry)
{
    return validate_entry(entry, 0, 1);
}

AmlLaunchCheck launch_run_child(const AmlEntry *entry, int force_shell)
{
    return run_child_in_entry_directory(entry, force_shell);
}
