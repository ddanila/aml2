#ifndef CFG_H
#define CFG_H

#include "aml.h"

int aml_load_config(AmlState *state, const char *path);
int aml_save_config(const AmlState *state, const char *path);

#endif
