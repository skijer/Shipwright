#include "banjo_page.h"

#include "banjo_form.h"

static bool HasLayoutPages(const SOHModApi* api) {
    return SOH_MOD_API_HAS(api, RegisterLayoutPage) && SOH_MOD_API_HAS(api, PlaceLayoutItem);
}

void BanjoPage_Register(const SOHModApi* api) {
    SOHLayoutPage page = { sizeof(SOHLayoutPage) };

    if (!HasLayoutPages(api)) {
        return;
    }
    page.key = BANJO_PAGE_KEY;
    page.title = "Banjo-Kazooie";
    page.kind = SOH_LAYOUT_ITEMS;
    page.rows = BANJO_PAGE_ROWS;
    page.columns = BANJO_PAGE_COLUMNS;
    page.ownerForm = BANJO_FORM_KEY;
    api->RegisterLayoutPage(&page);
}

bool BanjoPage_Place(const SOHModApi* api, const char* itemKey, uint8_t cell) {
    return HasLayoutPages(api) && api->PlaceLayoutItem(itemKey, BANJO_PAGE_KEY, cell);
}
