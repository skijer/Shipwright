/**
 * object_rod_of_seasons.c — the Rod of Seasons in Link's hand (Skijer's NEI).
 *
 * Model and pose only: the equip state (sRodDrawn) belongs to item_rod_of_seasons.c, which
 * unity-includes this file, and the fist itself comes from ItemEquip_HoldsClosedFist.
 */

#include "z64.h"
#include "../custom_items.h"
#include "../helpers/equip_helper.h"
#include "macros.h"
#include "functions.h"

// Dialled in game and baked. The staff measures 96 units tall, so the scale is the share of Link's
// own height it takes up. Order: offset XYZ, rotation XYZ, scale.
static const ItemHandPose sRodPose = {
    0.0f, 5.977f, -3.218f, 109.655f, 72.414f, 0.0f, 0.4f,
};

void CustomItems_DrawRodOfSeasons(Player* player, PlayState* play) {
    if (!Seasons_IsDrawn()) {
        return;
    }
    ItemEquip_DrawHeldModel(player, play, "__OTR__objects/object_nei_rod_of_seasons/gNeiRodOfSeasonsDL", NULL,
                            &sRodPose);
}
