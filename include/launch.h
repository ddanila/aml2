#ifndef LAUNCH_H
#define LAUNCH_H

#include "aml.h"

typedef enum AmlLaunchCheck {
    AML_LAUNCH_READY = 0,
    AML_LAUNCH_STUB_MISSING = 1,
    AML_LAUNCH_BAD_PATH = 2,
    AML_LAUNCH_TARGET_MISSING = 3,
    AML_LAUNCH_DIRECT_UNSUPPORTED = 4,
    AML_LAUNCH_CHILD_FAILED = 5
} AmlLaunchCheck;

int launch_write_run_request(const AmlEntry *entry, const char *path);
int launch_clear_run_request(const char *path);
AmlLaunchCheck launch_check_entry(const AmlEntry *entry);
AmlLaunchCheck launch_check_direct_entry(const AmlEntry *entry);
AmlLaunchCheck launch_run_child(const AmlEntry *entry, int force_shell);

#define aml_write_run_request launch_write_run_request
#define aml_clear_run_request launch_clear_run_request
#define aml_check_launch_entry launch_check_entry
#define aml_check_direct_launch_entry launch_check_direct_entry
#define aml_run_entry_child launch_run_child

#endif
