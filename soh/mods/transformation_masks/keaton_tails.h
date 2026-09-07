// The Keaton form's three tails. Not skeleton limbs (21 is a hard ceiling), so they
// are a separate 9-segment flex skeleton, frames translation-only from the waist,
// drawn after it with matrices in segment 0x0B — vanilla's Bunny Hood ear pattern.
#ifndef KEATON_TAILS_H
#define KEATON_TAILS_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

void KeatonTails_Reset(void);
void KeatonTails_Update(Player* player);
void KeatonTails_Draw(PlayState* play, Player* player);

#ifdef __cplusplus
}
#endif

#endif
