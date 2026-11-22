#ifndef MARIO_BUTTONS_H
#define MARIO_BUTTONS_H

#include <stdbool.h>

#include "mario_host.h"

void MarioButtons_Register(const SOHModApi* api);
// A takeover form pauses Link's action funcs, which are what use items, so Mario's own buttons reach them here.
// Returns false once Mario has been taken off.
bool MarioButtons_Update(PlayState* play);

#endif
