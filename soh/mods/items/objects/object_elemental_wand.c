/**
 * object_elemental_wand.c — the Elemental Wand in Link's hand (Skijer's NEI).
 *
 * Six rods, six models: the export gives each its own opaque + translucent pair, so the staff in
 * the fist IS the active element. Unity-included by item_elemental_wand.c, which owns Wand_IsDrawn.
 */

#include "z64.h"
#include "../custom_items.h"
#include "../helpers/equip_helper.h"
#include "extended_inventory.h" // Wand_GetMode / WAND_MODE_*
#include "macros.h"
#include "functions.h"

// Dialled in game and baked. The staff measures 350 units tall against the slate's 96, which is the
// whole reason its scale is not the slate's. Order: offset XYZ, rotation XYZ, scale.
static const ItemHandPose sWandPose = {
    0.0f, 7.356f, -3.218f, 91.034f, 180.0f, 111.724f, 0.12f,
};

static const struct {
    const char* opa;
    const char* xlu; // the gem, which is translucent on every rod
} sWandModel[WAND_MODE_COUNT] = {
    { "__OTR__objects/object_nei_wand_sand_rod/gNeiSandRodDL",
      "__OTR__objects/object_nei_wand_sand_rod/gNeiSandRodXluDL" },
    { "__OTR__objects/object_nei_wand_tornado_rod/gNeiTornadoRodDL",
      "__OTR__objects/object_nei_wand_tornado_rod/gNeiTornadoRodXluDL" },
    { "__OTR__objects/object_nei_wand_water_rod/gNeiWaterRodDL",
      "__OTR__objects/object_nei_wand_water_rod/gNeiWaterRodXluDL" },
    { "__OTR__objects/object_nei_wand_meteor_rod/gNeiMeteorRodDL",
      "__OTR__objects/object_nei_wand_meteor_rod/gNeiMeteorRodXluDL" },
    { "__OTR__objects/object_nei_wand_storm_rod/gNeiStormRodDL",
      "__OTR__objects/object_nei_wand_storm_rod/gNeiStormRodXluDL" },
    { "__OTR__objects/object_nei_wand_shadow_scepter/gNeiShadowScepterDL",
      "__OTR__objects/object_nei_wand_shadow_scepter/gNeiShadowScepterXluDL" },
};

void CustomItems_DrawElementalWand(Player* player, PlayState* play) {
    u8 mode = Wand_GetMode();

    // Before the gate below: both of these are world effects that outlive the staff being out. The
    // bolt is already in flight and the wind burns whether or not the wand is in Link's hand.
    WandShadow_Draw(play);
    WandWind_Draw(player, play);

    if (!Wand_IsDrawn() || (mode >= WAND_MODE_COUNT)) {
        return;
    }
    ItemEquip_DrawHeldModel(player, play, sWandModel[mode].opa, sWandModel[mode].xlu, &sWandPose);
}
