#ifndef BANJO_PAGE_H
#define BANJO_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "banjo_host.h"

#define BANJO_PAGE_KEY "banjo_mask.page"
#define BANJO_PAGE_ROWS 4
#define BANJO_PAGE_COLUMNS 6

// Banjo's own pause page: the host opens it first while he is out, and L leads to the game's pages.
void BanjoPage_Register(const SOHModApi* api);
// Assumes the item and the page are registered. Cells run left to right, top to bottom.
bool BanjoPage_Place(const SOHModApi* api, const char* itemKey, uint8_t cell);

#endif
