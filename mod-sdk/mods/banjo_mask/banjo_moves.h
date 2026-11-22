#ifndef BANJO_MOVES_H
#define BANJO_MOVES_H

#include "banjo_host.h"

void BanjoMoves_Register(const SOHModApi* api);
// Without move shuffle the mask brings every move Bottles teaches.
void BanjoMoves_GrantWithMask(void);
// Teaches BK's player the Spiral Mountain moves plus every move item the save owns.
void BanjoMoves_ApplyLearned(void);

#endif
