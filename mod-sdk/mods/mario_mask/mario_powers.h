#ifndef MARIO_POWERS_H
#define MARIO_POWERS_H

#include <stdbool.h>

#include "mario_host.h"

// The three caps, usable from Mario's buttons, and the punch and crouch abilities, each an item of its own.
void MarioPowers_Register(const SOHModApi* api);
// Whatever the seed does not shuffle comes with the mask.
void MarioPowers_GrantWithMask(void);
bool MarioPowers_CanPunch(void);
bool MarioPowers_CanCrouch(void);
// Puts the cap `itemKey` names on Mario for its magic; false when it names no cap or the magic is short.
bool MarioPowers_UseCap(PlayState* play, const char* itemKey);

#endif
