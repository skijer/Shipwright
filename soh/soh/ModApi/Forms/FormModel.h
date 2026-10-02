#ifndef SOH_MOD_API_FORM_MODEL_H
#define SOH_MOD_API_FORM_MODEL_H

#include <stdbool.h>

#include "soh/ModApi/Forms/FormRegistry.h"

#ifdef __cplusplus
extern "C" {
#endif

bool FormModel_IsActive(void);

#ifdef __cplusplus
}

void FormModel_Init();
void FormModel_SetActive(const SOHFormDefinition* form);
#endif

#endif
