#ifndef AML_H
#define AML_H

#include <ctype.h>
#include <stddef.h>

#include "aml_build.h"

#define AML_CONFIG_FILE "LAUNCHER.CFG"
#define AML_RUN_FILE "AML2.RUN"
#define AML_STUB_FILE "AML.COM"
#define AML_AUTO_FILE "AML2.AUT"
#define AML_TRACE_FILE "AML2.TRC"
#define AML_LAUNCHER_EXE "AMLUI.EXE"
#define AML_EXIT_OK 0
#define AML_EXIT_ERROR 1
#define AML_EXIT_LAUNCH 2
#define AML_EXIT_LAUNCH_DEBUG 3
#define AML_MAX_PROGRAMS 64
#define AML_MAX_NAME 48
#define AML_MAX_COMMAND 128
#define AML_MAX_PATH 64
#define AML_MAX_LINE 255

#ifndef AML_TEST_HOOKS
#define AML_TEST_HOOKS 0
#endif

typedef struct AmlEntry {
    char name[AML_MAX_NAME];
    char command[AML_MAX_COMMAND];
    char path[AML_MAX_PATH];
} AmlEntry;

typedef struct AmlEntryView {
    char big_name[25];
} AmlEntryView;

typedef struct AmlState {
    AmlEntry entries[AML_MAX_PROGRAMS];
    AmlEntryView entry_view[AML_MAX_PROGRAMS];
    int entry_count;
    int selected;
    int view_top;
    int modified;
    int editor_mode;
    int supervised;
} AmlState;

static inline void aml_build_big_name(char *dst, size_t dst_size, const char *src_name)
{
    size_t src_i;
    size_t dst_i = 0;

    if (dst_size == 0) {
        return;
    }

    for (src_i = 0; dst_i < dst_size - 1 && src_name[src_i] != '\0'; ++src_i) {
        unsigned char ch = (unsigned char)src_name[src_i];
        char out = ' ';

        if (isalpha(ch)) {
            out = (char)toupper(ch);
        } else if (isdigit(ch)) {
            out = (char)ch;
        } else if (ch == ' ') {
            out = ' ';
        }

        if (out != ' ' || (dst_i > 0 && dst[dst_i - 1] != ' ')) {
            dst[dst_i++] = out;
        }
    }

    while (dst_i > 0 && dst[dst_i - 1] == ' ') {
        --dst_i;
    }
    dst[dst_i] = '\0';
}

static inline void aml_refresh_entry_view(AmlState *state, int index)
{
    if (index < 0 || index >= state->entry_count) {
        return;
    }

    aml_build_big_name(
        state->entry_view[index].big_name,
        sizeof(state->entry_view[index].big_name),
        state->entries[index].name
    );
}

static inline void aml_refresh_all_entry_views(AmlState *state)
{
    int i;

    for (i = 0; i < state->entry_count; ++i) {
        aml_refresh_entry_view(state, i);
    }
}

#endif
