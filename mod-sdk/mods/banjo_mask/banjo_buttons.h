#ifndef BANJO_BUTTONS_H
#define BANJO_BUTTONS_H

#include <stdbool.h>

#include "banjo_host.h"

void BanjoButtons_Register(const SOHModApi* api);
// A takeover form pauses Link's action funcs, which are what use items, so Banjo's own buttons reach them here.
// Returns false once Banjo has been taken off.
bool BanjoButtons_Update(PlayState* play, Player* player);

#endif
