/** Shadow Crystal input bridge for the Wolf Link transformation. */
#include "mods/extended_inventory.h"
#include "mods/ext_buttons/ext_buttons.h"
#include "mods/transformation_masks/transformation_masks.h"
#include "mods/transformation_masks/wolf_link_form.h"

extern void MmForm_HandleMaskUse(PlayState* play, Player* player, s32 item);

void ShadowCrystal_TickInput(PlayState* play, Player* player) {
    static const u16 sButtons[8] = {
        BTN_B, BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT,
    };
    u16 pressed = play->state.input[0].press.button;

    // The extended item id lives outside equips.buttonItems' u8 range, so it
    // cannot reach the vanilla mask scanner.  Resolve the effective u16 ids and
    // forward only the actual button edge to the common transformation state machine.
    for (s32 button = 1; button < 8; ++button) {
        if ((pressed & sButtons[button]) && ExtButton_GetItem(button) == EXT_ITEM_SHADOW_CRYSTAL) {
            MmForm_HandleMaskUse(play, player, EXT_ITEM_SHADOW_CRYSTAL);
            return;
        }
    }
}
