#include "mario_page.h"

#include "mario_form.h"

static bool HasLayoutPages(const SOHModApi* api) {
    return SOH_MOD_API_HAS(api, RegisterLayoutPage) && SOH_MOD_API_HAS(api, PlaceLayoutItem);
}

void MarioPage_Register(const SOHModApi* api) {
    SOHLayoutPage page = { sizeof(SOHLayoutPage) };

    if (!HasLayoutPages(api)) {
        return;
    }
    page.key = MARIO_PAGE_KEY;
    page.title = "Mario";
    page.kind = SOH_LAYOUT_ITEMS;
    page.rows = MARIO_PAGE_ROWS;
    page.columns = MARIO_PAGE_COLUMNS;
    page.ownerForm = MARIO_FORM_KEY;
    api->RegisterLayoutPage(&page);
}

bool MarioPage_Place(const SOHModApi* api, const char* itemKey, uint8_t cell) {
    return HasLayoutPages(api) && api->PlaceLayoutItem(itemKey, MARIO_PAGE_KEY, cell);
}
