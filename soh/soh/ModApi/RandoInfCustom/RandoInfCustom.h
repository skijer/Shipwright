#ifndef SOH_MOD_API_RANDO_INF_CUSTOM_H
#define SOH_MOD_API_RANDO_INF_CUSTOM_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SOH_RANDO_INF_SAVE,
    SOH_RANDO_INF_LOGIC,
    SOH_RANDO_INF_BANK_MAX,
} SOHRandoInfBank;

#ifdef __cplusplus
extern "C" {
#endif

#define RANDO_INF_CUSTOM_NONE 0

uint16_t RandoInfCustom_Register(const char* key);
uint16_t RandoInfCustom_Find(const char* key);
const char* RandoInfCustom_GetKey(uint16_t flag);
uint16_t RandoInfCustom_GetCount(void);

bool RandoInfCustom_Get(SOHRandoInfBank bank, uint16_t flag);
void RandoInfCustom_Set(SOHRandoInfBank bank, uint16_t flag, bool state);
void RandoInfCustom_ClearBank(SOHRandoInfBank bank);

bool RandoInfCustom_GetByKey(const char* key);
void RandoInfCustom_SetByKey(const char* key, bool state);

#ifdef __cplusplus
}

void RandoInfCustom_Init();
#endif

#endif
