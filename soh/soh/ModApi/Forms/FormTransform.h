#ifndef SOH_MOD_API_FORM_TRANSFORM_H
#define SOH_MOD_API_FORM_TRANSFORM_H

#include <stdbool.h>

#include "soh/ModApi/Forms/FormRegistry.h"

#ifdef __cplusplus
extern "C" {
#endif

bool FormTransform_IsActive(void);

#ifdef __cplusplus
}

void FormTransform_Init();
bool FormTransform_Begin(const SOHFormDefinition* from, const SOHFormDefinition* to);
#endif

#endif
