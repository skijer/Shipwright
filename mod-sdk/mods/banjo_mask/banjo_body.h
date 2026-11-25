#ifndef BANJO_BODY_H
#define BANJO_BODY_H

#include <stdbool.h>

#include "banjo_host.h"
#include "bk_model.h"

void BanjoBody_Register(const SOHModApi* api);
// Loads Banjo's models from the ROM; false until the ROM is there.
bool BanjoBody_Load(void);
void BanjoBody_Reset(PlayState* play, Player* player);
void BanjoBody_Update(PlayState* play, Player* player);
void BanjoBody_Draw(PlayState* play, Player* player);
// One of Banjo's models from the ROM, NULL until it is there.
const BkModel* BanjoBody_GetModel(uint16_t id);
// Banjo's optional parts, as BK's modelappendages.c shows them.
enum BanjoAppendage {
    BANJO_APPENDAGE_KAZOOIE = 1 << 0,
    BANJO_APPENDAGE_KAZOOIE_FEET = 1 << 1,
    BANJO_APPENDAGE_TURBO_TRAINERS = 1 << 2,
    BANJO_APPENDAGE_WADING_BOOTS = 1 << 3,
};

// The parts BK's own move states show while `animation` plays.
uint8_t BanjoBody_GetAppendagesOf(uint16_t animation);
// The geo selectors that dress `model` with `appendages` and the eyes.
void BanjoBody_FillSelectors(int16_t* selectors, uint16_t model, uint8_t appendages, int16_t eyes);
// A small Banjo for get-item models, looping `animation` at the current matrix.
void BanjoBody_DrawFigure(PlayState* play, uint16_t animation, bool isKazooieOut);

#endif
