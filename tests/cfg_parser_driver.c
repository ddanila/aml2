#include <stdio.h>
#include <string.h>

#include "cfg.h"

static void aml_init_state(AmlState *state)
{
    memset(state, 0, sizeof(*state));
}

static int aml_expect(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int aml_check_load_basic(const char *path)
{
    AmlState state;

    aml_init_state(&state);
    if (aml_load_config(&state, path) != AML_CFG_OK) {
        fprintf(stderr, "FAIL: load basic config\n");
        return 1;
    }

    if (aml_expect(state.entry_count == 3, "basic entry count")) return 1;
    if (aml_expect(strcmp(state.entries[0].name, "Alpha") == 0, "trimmed first name")) return 1;
    if (aml_expect(strcmp(state.entries[0].command, "ALPHA.EXE") == 0, "trimmed first command")) return 1;
    if (aml_expect(strcmp(state.entries[0].path, "C:\\GAMES") == 0, "trimmed first path")) return 1;
    if (aml_expect(strcmp(state.entries[1].name, "Beta") == 0, "second name")) return 1;
    if (aml_expect(strcmp(state.entries[1].path, "") == 0, "empty path preserved")) return 1;
    if (aml_expect(strcmp(state.entries[2].name, "Gamma") == 0, "third valid entry kept")) return 1;
    return 0;
}

static int aml_check_load_limit(const char *path)
{
    AmlState state;

    aml_init_state(&state);
    if (aml_load_config(&state, path) != AML_CFG_OK) {
        fprintf(stderr, "FAIL: load limit config\n");
        return 1;
    }

    if (aml_expect(state.entry_count == AML_MAX_PROGRAMS, "max program cap enforced")) return 1;
    if (aml_expect(strcmp(state.entries[0].name, "Entry 00") == 0, "first capped entry")) return 1;
    if (aml_expect(strcmp(state.entries[AML_MAX_PROGRAMS - 1].name, "Entry 63") == 0, "last capped entry")) return 1;
    return 0;
}

static int aml_check_missing_load(const char *path)
{
    AmlState state;

    aml_init_state(&state);
    if (aml_load_config(&state, path) != AML_CFG_IO_ERROR) {
        fprintf(stderr, "FAIL: missing config should return io error\n");
        return 1;
    }

    if (aml_expect(state.entry_count == 0, "missing config resets entry count")) return 1;
    if (aml_expect(state.selected == 0, "missing config resets selected")) return 1;
    if (aml_expect(state.view_top == 0, "missing config resets view top")) return 1;
    if (aml_expect(state.modified == 0, "missing config resets modified")) return 1;
    return 0;
}

static int aml_check_save_output(const char *out_path)
{
    AmlState state;
    FILE *fp;
    char line[256];

    aml_init_state(&state);
    state.entry_count = 2;
    strcpy(state.entries[0].name, "One");
    strcpy(state.entries[0].command, "ONE.EXE");
    strcpy(state.entries[0].path, "");
    strcpy(state.entries[1].name, "Two");
    strcpy(state.entries[1].command, "TWO.BAT");
    strcpy(state.entries[1].path, "D:\\DOS");

    if (aml_save_config(&state, out_path) != AML_CFG_OK) {
        fprintf(stderr, "FAIL: save config\n");
        return 1;
    }

    fp = fopen(out_path, "r");
    if (fp == NULL) {
        fprintf(stderr, "FAIL: reopen saved config\n");
        return 1;
    }

    if (fgets(line, sizeof(line), fp) == NULL || strcmp(line, "# aml2 configuration\n") != 0) {
        fclose(fp);
        fprintf(stderr, "FAIL: save header line 1\n");
        return 1;
    }
    if (fgets(line, sizeof(line), fp) == NULL || strcmp(line, "# Format: Name|Command|Working Directory\n") != 0) {
        fclose(fp);
        fprintf(stderr, "FAIL: save header line 2\n");
        return 1;
    }
    if (fgets(line, sizeof(line), fp) == NULL || strcmp(line, "\n") != 0) {
        fclose(fp);
        fprintf(stderr, "FAIL: save blank separator\n");
        return 1;
    }
    if (fgets(line, sizeof(line), fp) == NULL || strcmp(line, "One|ONE.EXE|\n") != 0) {
        fclose(fp);
        fprintf(stderr, "FAIL: save entry line 1\n");
        return 1;
    }
    if (fgets(line, sizeof(line), fp) == NULL || strcmp(line, "Two|TWO.BAT|D:\\DOS\n") != 0) {
        fclose(fp);
        fprintf(stderr, "FAIL: save entry line 2\n");
        return 1;
    }

    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: cfg_parser_driver <basic> <limit> <missing> <save-out>\n");
        return 2;
    }

    if (aml_check_load_basic(argv[1])) return 1;
    if (aml_check_load_limit(argv[2])) return 1;
    if (aml_check_missing_load(argv[3])) return 1;
    if (aml_check_save_output(argv[4])) return 1;

    puts("cfg parser checks passed");
    return 0;
}
