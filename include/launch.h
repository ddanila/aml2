#ifndef LAUNCH_H
#define LAUNCH_H

#include "aml.h"

enum AmlLaunchCheck {
    AML_LAUNCH_READY = 0,
    AML_LAUNCH_STUB_MISSING = 1,
    AML_LAUNCH_BAD_PATH = 2,
    AML_LAUNCH_TARGET_MISSING = 3
};

int aml_write_run_request(const AmlEntry *entry, const char *path);
int aml_clear_run_request(const char *path);
int aml_check_launch_entry(const AmlEntry *entry);

#endif
