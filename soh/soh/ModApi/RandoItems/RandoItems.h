#ifndef SOH_MOD_API_RANDO_ITEMS_H
#define SOH_MOD_API_RANDO_ITEMS_H

#include <stdbool.h>
#include <stdint.h>

#include "soh/ModApi/RandoInfCustom/RandoInfCustom.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t RandoItems_GetRandoGet(const char* key);
const char* RandoItems_GetKey(int32_t randoGet);
bool RandoItems_IsModItem(int32_t randoGet);
uint32_t RandoItems_GetCount(void);
int32_t RandoItems_GetAt(uint32_t index);

bool RandoItems_IsOwned(int32_t randoGet, SOHRandoInfBank bank);
void RandoItems_SetOwned(int32_t randoGet, SOHRandoInfBank bank, bool state);

#ifdef __cplusplus
}

int32_t RandoItems_Register(const char* key);
void RandoItems_BuildItemTable();
void RandoItems_AddToPool();
#endif

#endif
