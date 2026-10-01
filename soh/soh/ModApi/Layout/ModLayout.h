#ifndef SOH_MOD_LAYOUT_H
#define SOH_MOD_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

typedef enum {
    SOH_LAYOUT_ITEMS,
    SOH_LAYOUT_EQUIPMENT,
    SOH_LAYOUT_COLLECTABLES,
    SOH_LAYOUT_COUNT,
} SOHLayoutKind;

typedef struct {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
} SOHLayoutCell;

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* title;
    uint8_t kind;
    uint8_t rows;
    uint8_t columns;
    uint8_t cellCount;
    const SOHLayoutCell* cells;
} SOHLayoutPage;

typedef struct {
    const char* key;
    const char* pageKey;
    uint8_t sourceKind;
    uint8_t source;
    uint8_t cell;
} SOHLayoutVanilla;

bool ModLayout_RegisterPage(const SOHLayoutPage* page);
bool ModLayout_PlaceItem(const char* itemKey, const char* pageKey, uint8_t cell);
bool ModLayout_RegisterVanilla(const SOHLayoutVanilla* entry);
bool ModLayout_CyclePage(struct PlayState* play, uint8_t kind);
bool ModLayout_DrawPage(struct PlayState* play, uint8_t kind);
bool ModLayout_IsActive(uint8_t kind);
bool ModLayout_BeginCustomEquip(struct PlayState* play, const char* key, uint8_t button, int16_t x, int16_t y);
void ModLayout_CompleteCustomEquip(struct PlayState* play);
const char* ModLayout_GetEquipIcon(void);
bool ModLayout_HasMods(void);
void ModLayout_Init(void);

#ifdef __cplusplus
}

void ModLayout_DrawEditor(uint8_t kind);
void ModLayout_DrawWindowButton();
#endif

#endif
