#ifndef SOH_MOD_API_RANDO_OPTIONS_H
#define SOH_MOD_API_RANDO_OPTIONS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOH_MOD_RANDO_WIDGET_COMBOBOX = 0,
    SOH_MOD_RANDO_WIDGET_SLIDER = 1,
} SOHModRandoWidget;

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* label;
    const char* tooltip;
    const char* const* values;
    uint32_t valueCount;
    uint8_t defaultValue;
    uint8_t widget;
} SOHModRandoOption;

#define SOH_MOD_RANDO_OPTION_MIN_SIZE \
    (offsetof(SOHModRandoOption, defaultValue) + sizeof(((SOHModRandoOption*)0)->defaultValue))

bool RandoOptions_Register(const SOHModRandoOption* option);
uint8_t RandoOptions_Get(const char* key);
uint8_t RandoOptions_GetForSeed(const char* key);

#ifdef __cplusplus
}

void RandoOptions_SaveAll();
void RandoOptions_LoadAll();
#endif

#endif
