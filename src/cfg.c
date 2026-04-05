#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "cfg.h"

static void reset_config_state(AmlState *state)
{
    state->entry_count = 0;
    state->selected = 0;
    state->view_top = 0;
    state->modified = 0;
}

static void copy_field(char *dst, unsigned dst_size, const char *src)
{
    if (dst_size == 0) {
        return;
    }

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void trim_newline(char *line)
{
    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[len - 1] = '\0';
        len--;
    }
}

static char *trim_field(char *text)
{
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';

    return text;
}

static int parse_config_line(AmlState *state, char *line)
{
    char *name_end;
    char *command_start;
    char *command_end;
    char *path_start;
    char *name;
    char *command;
    char *entry_path;
    AmlEntry *entry;

    if (state->entry_count >= AML_MAX_PROGRAMS) {
        return 1;
    }

    trim_newline(line);

    command_start = trim_field(line);
    if (command_start[0] == '\0' || command_start[0] == '#') {
        return 0;
    }

    name_end = strchr(command_start, '|');
    if (name_end == NULL) {
        return 0;
    }

    *name_end = '\0';
    command_start = name_end + 1;
    command_end = strchr(command_start, '|');
    if (command_end == NULL) {
        return 0;
    }

    *command_end = '\0';
    path_start = command_end + 1;
    name = trim_field(line);
    command = trim_field(command_start);
    entry_path = trim_field(path_start);

    if (name[0] == '\0' || command[0] == '\0') {
        return 0;
    }

    entry = &state->entries[state->entry_count++];
    copy_field(entry->name, sizeof(entry->name), name);
    copy_field(entry->command, sizeof(entry->command), command);
    copy_field(entry->path, sizeof(entry->path), entry_path);
    return 0;
}

static AmlCfgStatus open_config_for_read(const char *path, FILE **fp)
{
    *fp = fopen(path, "r");
    if (*fp == NULL) {
        return AML_CFG_IO_ERROR;
    }

    return AML_CFG_OK;
}

static void load_config_lines(AmlState *state, FILE *fp)
{
    char line[AML_MAX_LINE + 1];

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (parse_config_line(state, line)) {
            break;
        }
    }
}

AmlCfgStatus cfg_load(AmlState *state, const char *path)
{
    FILE *fp;

    reset_config_state(state);

    if (open_config_for_read(path, &fp) != AML_CFG_OK) {
        return AML_CFG_IO_ERROR;
    }

    load_config_lines(state, fp);
    fclose(fp);
    return AML_CFG_OK;
}

static AmlCfgStatus open_config_for_write(const char *path, FILE **fp)
{
    *fp = fopen(path, "w");
    if (*fp == NULL) {
        return AML_CFG_IO_ERROR;
    }

    return AML_CFG_OK;
}

static void write_config_header(FILE *fp)
{
    fprintf(fp, "# aml2 configuration\n");
    fprintf(fp, "# Format: Name|Command|Working Directory\n");
    fprintf(fp, "\n");
}

static void write_config_entries(const AmlState *state, FILE *fp)
{
    int i;

    for (i = 0; i < state->entry_count; ++i) {
        fprintf(fp, "%s|%s|%s\n",
            state->entries[i].name,
            state->entries[i].command,
            state->entries[i].path);
    }
}

AmlCfgStatus cfg_save(const AmlState *state, const char *path)
{
    FILE *fp;

    if (open_config_for_write(path, &fp) != AML_CFG_OK) {
        return AML_CFG_IO_ERROR;
    }

    write_config_header(fp);
    write_config_entries(state, fp);
    fclose(fp);
    return AML_CFG_OK;
}
