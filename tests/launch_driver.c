#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "launch.h"

static int test_spawn_result = 0;
static int test_system_result = 0;
static char test_spawn_command[AML_MAX_COMMAND];
static char test_system_command[AML_MAX_COMMAND];
static char test_spawn_cwd[PATH_MAX];
static char test_system_cwd[PATH_MAX];
static unsigned test_current_drive = 3;
static unsigned test_last_set_drive = 0;

void _dos_getdrive(unsigned *drive)
{
    *drive = test_current_drive;
}

void _dos_setdrive(unsigned drive, unsigned *drives)
{
    test_current_drive = drive;
    test_last_set_drive = drive;
    if (drives != NULL) {
        *drives = 26;
    }
}

int spawnlp(int mode, const char *path, const char *arg0, ...)
{
    (void)mode;
    (void)arg0;
    strncpy(test_spawn_command, path, sizeof(test_spawn_command) - 1);
    test_spawn_command[sizeof(test_spawn_command) - 1] = '\0';
    if (getcwd(test_spawn_cwd, sizeof(test_spawn_cwd)) == NULL) {
        test_spawn_cwd[0] = '\0';
    }
    return test_spawn_result;
}

int system(const char *command)
{
    strncpy(test_system_command, command, sizeof(test_system_command) - 1);
    test_system_command[sizeof(test_system_command) - 1] = '\0';
    if (getcwd(test_system_cwd, sizeof(test_system_cwd)) == NULL) {
        test_system_cwd[0] = '\0';
    }
    return test_system_result;
}

static void aml_reset_exec_mocks(void)
{
    test_spawn_result = 0;
    test_system_result = 0;
    test_spawn_command[0] = '\0';
    test_system_command[0] = '\0';
    test_spawn_cwd[0] = '\0';
    test_system_cwd[0] = '\0';
    test_current_drive = 3;
    test_last_set_drive = 0;
}

static int aml_expect(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int aml_make_dir(const char *path)
{
    if (mkdir(path, 0777) != 0) {
        perror(path);
        return 1;
    }
    return 0;
}

static int aml_write_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");

    if (fp == NULL) {
        perror(path);
        return 1;
    }
    fputs(text, fp);
    fclose(fp);
    return 0;
}

static int aml_setup_check_tree(const char *root)
{
    char path[PATH_MAX];

    if (aml_make_dir(root)) return 1;

    snprintf(path, sizeof(path), "%s/%s", root, AML_STUB_FILE);
    if (aml_write_file(path, "stub\n")) return 1;
    snprintf(path, sizeof(path), "%s/GAME.EXE", root);
    if (aml_write_file(path, "game\n")) return 1;
    snprintf(path, sizeof(path), "%s/START.BAT", root);
    if (aml_write_file(path, "batch\n")) return 1;
    snprintf(path, sizeof(path), "%s/game", root);
    if (aml_make_dir(path)) return 1;
    snprintf(path, sizeof(path), "%s/game/CHILD.EXE", root);
    if (aml_write_file(path, "child\n")) return 1;

    return 0;
}

static void aml_fill_entry(AmlEntry *entry, const char *command, const char *path)
{
    memset(entry, 0, sizeof(*entry));
    strcpy(entry->name, "Test");
    strcpy(entry->command, command);
    strcpy(entry->path, path);
}

static int aml_check_launch_validation(const char *root)
{
    AmlEntry entry;
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }
    if (chdir(root) != 0) {
        perror(root);
        return 1;
    }

    aml_fill_entry(&entry, "GAME.EXE", "");
    if (aml_expect(aml_check_launch_entry(&entry) == AML_LAUNCH_READY, "stub launch ready")) return 1;
    if (aml_expect(aml_check_direct_launch_entry(&entry) == AML_LAUNCH_READY, "direct program ready")) return 1;

    aml_fill_entry(&entry, "MISSING.EXE", "");
    if (aml_expect(aml_check_launch_entry(&entry) == AML_LAUNCH_TARGET_MISSING, "missing target flagged")) return 1;

    aml_fill_entry(&entry, "START.BAT", "");
    if (aml_expect(aml_check_launch_entry(&entry) == AML_LAUNCH_READY, "batch allowed via stub")) return 1;
    if (aml_expect(aml_check_direct_launch_entry(&entry) == AML_LAUNCH_DIRECT_UNSUPPORTED, "batch rejected for direct launch")) return 1;

    aml_fill_entry(&entry, "echo GAME.EXE", "");
    if (aml_expect(aml_check_launch_entry(&entry) == AML_LAUNCH_READY, "complex command allowed via stub")) return 1;
    if (aml_expect(aml_check_direct_launch_entry(&entry) == AML_LAUNCH_DIRECT_UNSUPPORTED, "complex command rejected for direct launch")) return 1;

    aml_fill_entry(&entry, "GAME.EXE", "missing-dir");
    if (aml_expect(aml_check_launch_entry(&entry) == AML_LAUNCH_BAD_PATH, "missing path flagged")) return 1;
    if (aml_expect(aml_check_direct_launch_entry(&entry) == AML_LAUNCH_BAD_PATH, "missing path flagged for direct")) return 1;

    if (remove(AML_STUB_FILE) != 0) {
        perror("remove AML.COM");
        return 1;
    }
    aml_fill_entry(&entry, "GAME.EXE", "");
    if (aml_expect(aml_check_launch_entry(&entry) == AML_LAUNCH_STUB_MISSING, "missing stub flagged")) return 1;

    if (chdir(cwd) != 0) {
        perror(cwd);
        return 1;
    }
    return 0;
}

static int aml_check_child_exec(const char *root)
{
    AmlEntry entry;
    char original_cwd[PATH_MAX];
    char expected_game_dir[PATH_MAX];

    if (getcwd(original_cwd, sizeof(original_cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }

    snprintf(expected_game_dir, sizeof(expected_game_dir), "%s/game", root);

    aml_fill_entry(&entry, "CHILD.EXE", "game");
    aml_reset_exec_mocks();
    if (chdir(root) != 0) {
        perror(root);
        return 1;
    }
    if (aml_expect(aml_run_entry_child(&entry, 0) == AML_LAUNCH_READY, "direct child launch succeeds")) return 1;
    if (aml_expect(strcmp(test_spawn_command, "CHILD.EXE") == 0, "spawnlp used command")) return 1;
    if (aml_expect(strcmp(test_spawn_cwd, expected_game_dir) == 0, "spawnlp ran inside entry directory")) return 1;
    if (getcwd(test_system_cwd, sizeof(test_system_cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }
    if (aml_expect(strcmp(test_system_cwd, root) == 0, "cwd restored after child launch")) return 1;

    aml_reset_exec_mocks();
    test_system_result = 0;
    if (aml_expect(aml_run_entry_child(&entry, 1) == AML_LAUNCH_READY, "shell child launch succeeds")) return 1;
    if (aml_expect(strcmp(test_system_command, "CHILD.EXE") == 0, "system used command")) return 1;
    if (aml_expect(strcmp(test_system_cwd, expected_game_dir) == 0, "system ran inside entry directory")) return 1;
    if (getcwd(test_spawn_cwd, sizeof(test_spawn_cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }
    if (aml_expect(strcmp(test_spawn_cwd, root) == 0, "cwd restored after shell launch")) return 1;

    aml_reset_exec_mocks();
    test_spawn_result = -1;
    if (aml_expect(aml_run_entry_child(&entry, 0) == AML_LAUNCH_CHILD_FAILED, "direct child failure propagated")) return 1;
    if (getcwd(test_spawn_cwd, sizeof(test_spawn_cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }
    if (aml_expect(strcmp(test_spawn_cwd, root) == 0, "cwd restored after failed direct child")) return 1;

    aml_reset_exec_mocks();
    test_system_result = -1;
    if (aml_expect(aml_run_entry_child(&entry, 1) == AML_LAUNCH_CHILD_FAILED, "shell child failure propagated")) return 1;
    if (getcwd(test_system_cwd, sizeof(test_system_cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }
    if (aml_expect(strcmp(test_system_cwd, root) == 0, "cwd restored after failed shell child")) return 1;

    if (chdir(original_cwd) != 0) {
        perror(original_cwd);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: launch_driver <root>\n");
        return 2;
    }

    if (aml_setup_check_tree(argv[1])) return 1;
    if (aml_check_launch_validation(argv[1])) return 1;
    if (aml_check_child_exec(argv[1])) return 1;

    puts("launch checks passed");
    return 0;
}
