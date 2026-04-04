#ifndef AML_H
#define AML_H

#include "aml_build.h"

#define AML_CONFIG_FILE "launcher.cfg"
#define AML_RUN_FILE "AML2.RUN"
#define AML_STUB_FILE "AML.COM"
#define AML_AUTO_FILE "AML2.AUT"
#define AML_TRACE_FILE "AML2.TRC"
#define AML_LAUNCHER_EXE "AMLUI.EXE"
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

typedef struct AmlState {
    AmlEntry entries[AML_MAX_PROGRAMS];
    int entry_count;
    int selected;
    int view_top;
    int modified;
    int editor_mode;
} AmlState;

#endif
