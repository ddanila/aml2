#ifndef LAUNCH_H
#define LAUNCH_H

#include "aml.h"

int aml_write_run_request(const AmlEntry *entry, const char *path);
int aml_clear_run_request(const char *path);

#endif
