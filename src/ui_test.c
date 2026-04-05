#include <stdio.h>
#include <string.h>

#include "ui_int.h"

static void trim_newline(char *line)
{
    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[len - 1] = '\0';
        len--;
    }
}

#if AML_TEST_HOOKS
int ui_read_auto_line(char *line, unsigned line_size)
{
    FILE *fp;
    char current[AML_MAX_LINE + 1];
    char rest[2048];
    int found = 0;
    int rest_len = 0;

    fp = fopen(AML_AUTO_FILE, "r");
    if (fp == NULL) {
        return 0;
    }

    line[0] = '\0';
    rest[0] = '\0';

    while (fgets(current, sizeof(current), fp) != NULL) {
        trim_newline(current);
        if (!found && current[0] != '\0') {
            strncpy(line, current, line_size - 1);
            line[line_size - 1] = '\0';
            found = 1;
            continue;
        }
        if (found && current[0] != '\0') {
            size_t current_len = strlen(current);

            if (rest_len + (int)current_len + 1 >= (int)sizeof(rest)) {
                break;
            }
            memcpy(rest + rest_len, current, current_len);
            rest_len += (int)current_len;
            rest[rest_len++] = '\n';
            rest[rest_len] = '\0';
        }
    }
    fclose(fp);

    if (!found) {
        remove(AML_AUTO_FILE);
        return 0;
    }

    if (rest_len == 0) {
        remove(AML_AUTO_FILE);
    } else {
        fp = fopen(AML_AUTO_FILE, "w");
        if (fp != NULL) {
            fputs(rest, fp);
            fclose(fp);
        }
    }

    return 1;
}

void ui_trace_event(const char *event_name)
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
#else
int ui_read_auto_line(char *line, unsigned line_size)
{
    (void)line;
    (void)line_size;
    return 0;
}

void ui_trace_event(const char *event_name)
{
    (void)event_name;
}
#endif
