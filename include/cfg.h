#ifndef CFG_H
#define CFG_H

#include "aml.h"

enum AmlCfgStatus {
    AML_CFG_OK = 0,
    AML_CFG_IO_ERROR = 1
};

int aml_load_config(AmlState *state, const char *path);
int aml_save_config(const AmlState *state, const char *path);

#endif
