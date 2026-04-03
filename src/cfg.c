#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "cfg.h"

static void aml_copy_field(char *dst, unsigned dst_size, const char *src)
{
    if (dst_size == 0) {
        return;
    }

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void aml_trim_newline(char *line)
{
    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[len - 1] = '\0';
        len--;
    }
}

static char *aml_trim_field(char *text)
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

int aml_load_config(AmlState *state, const char *path)
{
    FILE *fp;
    char line[AML_MAX_LINE + 1];

    state->entry_count = 0;
    state->selected = 0;
    state->modified = 0;

    fp = fopen(path, "r");
    if (fp == NULL) {
        return 1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *name_end;
        char *command_start;
        char *command_end;
        char *path_start;
        char *name;
        char *command;
        char *entry_path;
        AmlEntry *entry;

        aml_trim_newline(line);

        command_start = aml_trim_field(line);

        if (command_start[0] == '\0' || command_start[0] == '#') {
            continue;
        }

        name_end = strchr(command_start, '|');
        if (name_end == NULL) {
            continue;
        }

        *name_end = '\0';
        command_start = name_end + 1;
        command_end = strchr(command_start, '|');
        if (command_end == NULL) {
            continue;
        }

        *command_end = '\0';
        path_start = command_end + 1;
        name = aml_trim_field(line);
        command = aml_trim_field(command_start);
        entry_path = aml_trim_field(path_start);

        if (name[0] == '\0' || command[0] == '\0') {
            continue;
        }

        if (state->entry_count >= AML_MAX_PROGRAMS) {
            break;
        }

        entry = &state->entries[state->entry_count++];
        aml_copy_field(entry->name, sizeof(entry->name), name);
        aml_copy_field(entry->command, sizeof(entry->command), command);
        aml_copy_field(entry->path, sizeof(entry->path), entry_path);
    }

    fclose(fp);
    return 0;
}

int aml_save_config(const AmlState *state, const char *path)
{
    FILE *fp;
    int i;

    fp = fopen(path, "w");
    if (fp == NULL) {
        return 1;
    }

    fprintf(fp, "# aml2 configuration\n");
    fprintf(fp, "# Format: Name|Command|Working Directory\n");
    fprintf(fp, "\n");

    for (i = 0; i < state->entry_count; ++i) {
        fprintf(fp, "%s|%s|%s\n",
            state->entries[i].name,
            state->entries[i].command,
            state->entries[i].path);
    }

    fclose(fp);
    return 0;
}
