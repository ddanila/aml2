#ifndef CFG_H
#define CFG_H

#include "aml.h"

typedef enum AmlCfgStatus {
    AML_CFG_OK = 0,
    AML_CFG_IO_ERROR = 1
} AmlCfgStatus;

AmlCfgStatus aml_load_config(AmlState *state, const char *path);
AmlCfgStatus aml_save_config(const AmlState *state, const char *path);

#endif
