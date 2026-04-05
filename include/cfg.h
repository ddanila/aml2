#ifndef CFG_H
#define CFG_H

#include "aml.h"

typedef enum AmlCfgStatus {
    AML_CFG_OK = 0,
    AML_CFG_IO_ERROR = 1
} AmlCfgStatus;

AmlCfgStatus cfg_load(AmlState *state, const char *path);
AmlCfgStatus cfg_save(const AmlState *state, const char *path);

#define aml_load_config cfg_load
#define aml_save_config cfg_save

#endif
