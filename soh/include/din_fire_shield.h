#ifndef DIN_FIRE_SHIELD_H
#define DIN_FIRE_SHIELD_H

#include <stdint.h>

struct PlayState;
struct Player;

#ifdef __cplusplus
extern "C" {
#endif

void DinFireShield_Reset(void);
void DinFireShield_Update(struct PlayState* play, struct Player* player);
// Called with the gameplay right-hand limb matrix current; never from pause.
void DinFireShield_Draw(struct PlayState* play, struct Player* player);
// Return false/NULL to preserve the native item model/icon fallback.
int DinFireShield_DrawItem(struct PlayState* play, int16_t drawId);
void* DinFireShield_ItemIcon(uint16_t itemId);

#ifdef __cplusplus
}
#endif

#endif
