/**
 * ext_buttons.cpp — Extended-button infrastructure accessor API (see ext_buttons.h).
 *
 * Globbed and compiled as its own translation unit by soh/CMakeLists.txt (mods/*.cpp, recursive).
 * Exports C-linkage helpers so the u8 buttonItems marker (ITEM_EXT_BUTTON) and the parallel u16
 * extButtons array stay in lockstep.
 */
#include "ext_buttons.h"

// gSaveContext is declared in variables.h, which z64.h does not pull in. Declaring the one symbol
// this file needs keeps the TU free of that header's global-variable flood (same as nei_save.cpp).
extern "C" SaveContext gSaveContext;

extern "C" {

u16 ExtButton_GetItem(s32 btn) {
    u8 raw = gSaveContext.equips.buttonItems[btn];
    if (raw == ITEM_EXT_BUTTON) {
        return EXT_BUTTON_ITEM(btn);
    }
    return (u16)raw;
}

void ExtButton_SetItem(s32 btn, u16 extId) {
    gSaveContext.equips.buttonItems[btn] = ITEM_EXT_BUTTON;
    EXT_BUTTON_ITEM(btn) = extId;
}

void ExtButton_ClearItem(s32 btn) {
    gSaveContext.equips.buttonItems[btn] = ITEM_NONE;
    EXT_BUTTON_ITEM(btn) = 0;
}

} // extern "C"
