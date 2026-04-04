#ifndef LAUNCH_H
#define LAUNCH_H

#include "aml.h"

enum AmlLaunchCheck {
    AML_LAUNCH_READY = 0,
    AML_LAUNCH_STUB_MISSING = 1,
    AML_LAUNCH_BAD_PATH = 2,
    AML_LAUNCH_TARGET_MISSING = 3,
    AML_LAUNCH_DIRECT_UNSUPPORTED = 4,
    AML_LAUNCH_CHILD_FAILED = 5
};

int aml_write_run_request(const AmlEntry *entry, const char *path);
int aml_clear_run_request(const char *path);
int aml_check_launch_entry(const AmlEntry *entry);
int aml_check_direct_launch_entry(const AmlEntry *entry);
int aml_run_entry_child(const AmlEntry *entry, int force_shell);

#endif
