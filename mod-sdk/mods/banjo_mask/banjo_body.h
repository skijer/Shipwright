#ifndef BANJO_BODY_H
#define BANJO_BODY_H

#include <stdbool.h>

#include "banjo_host.h"

void BanjoBody_Register(const SOHModApi* api);
// Loads Banjo's models from the ROM; false until the ROM is there.
bool BanjoBody_Load(void);
void BanjoBody_Reset(PlayState* play, Player* player);
void BanjoBody_Update(PlayState* play, Player* player);
void BanjoBody_Draw(PlayState* play, Player* player);

#endif
