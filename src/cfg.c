#include <stdio.h>
#include <string.h>

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

int aml_load_config(AmlState *state, const char *path)
{
    FILE *fp;
    char line[AML_MAX_LINE + 1];

    state->entry_count = 0;
    state->selected = 0;

    fp = fopen(path, "r");
    if (fp == NULL) {
        return 1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *name_end;
        char *command_start;
        char *command_end;
        char *path_start;
        AmlEntry *entry;

        aml_trim_newline(line);

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        name_end = strchr(line, '|');
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

        if (state->entry_count >= AML_MAX_PROGRAMS) {
            break;
        }

        entry = &state->entries[state->entry_count++];
        aml_copy_field(entry->name, sizeof(entry->name), line);
        aml_copy_field(entry->command, sizeof(entry->command), command_start);
        aml_copy_field(entry->path, sizeof(entry->path), path_start);
    }

    fclose(fp);
    return 0;
}
