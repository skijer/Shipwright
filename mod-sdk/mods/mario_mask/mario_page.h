#ifndef MARIO_PAGE_H
#define MARIO_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "mario_host.h"

#define MARIO_PAGE_KEY "mario_mask.page"
#define MARIO_PAGE_ROWS 4
#define MARIO_PAGE_COLUMNS 6

// Mario's own pause page: the host opens it first while he is out, and L leads to the game's pages.
void MarioPage_Register(const SOHModApi* api);
// Assumes the item and the page are registered. Cells run left to right, top to bottom.
bool MarioPage_Place(const SOHModApi* api, const char* itemKey, uint8_t cell);

#endif
