#include <stdio.h>

#include "launch.h"

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
