#include "draw.h"
#include "soh/OTRGlobals.h"
#include <vector>          // MmDL_WithScopedVerts keeps patched DL copies alive
#include <spdlog/spdlog.h> // SPDLOG_INFO (MmSoul debug instrumentation)
#include "soh/cvar_prefixes.h"
#include "randomizerTypes.h"
#include "soh_assets.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/cosmetics/cosmeticsTypes.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "mods/extended_inventory.h" // Skijer's NEI — Seasons_* (Rod of Seasons flame colour)

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "dungeon.h"
#include "objects/object_gi_key/object_gi_key.h"
#include "objects/object_gi_bosskey/object_gi_bosskey.h"
#include "objects/object_gi_compass/object_gi_compass.h"
#include "objects/object_gi_map/object_gi_map.h"
#include "objects/object_gi_hearts/object_gi_hearts.h"
#include "objects/object_gi_scale/object_gi_scale.h"
#include "objects/object_gi_fire/object_gi_fire.h"
#include "objects/object_toki_objects/object_toki_objects.h"
#include "objects/object_gi_bomb_2/object_gi_bomb_2.h"
#include "objects/object_goma/object_goma.h"
#include "objects/object_kingdodongo/object_kingdodongo.h"
#include "objects/object_bv/object_bv.h"
#include "objects/object_gnd/object_gnd.h"
#include "objects/object_fd/object_fd.h"
#include "objects/object_mamenoki/object_mamenoki.h"
#include "objects/object_mo/object_mo.h"
#include "objects/object_mori_objects/object_mori_objects.h"
#include "objects/object_st/object_st.h" // native token DLs (MM GS tokens draw with OoT's own model)
#include "objects/object_fr/object_fr.h" // native frog skeleton (MM healed frogs)
#include "objects/object_sst/object_sst.h"
#include "overlays/actors/ovl_Boss_Goma/z_boss_goma.h"
#include "objects/object_tw/object_tw.h"
#include "objects/object_ganon2/object_ganon2.h"
#include "objects/object_gi_shield_1/object_gi_shield_1.h"
#include "objects/object_gi_shield_3/object_gi_shield_3.h"
#include "objects/object_gi_hookshot/object_gi_hookshot.h"
#include "objects/object_gi_bottle/object_gi_bottle.h" // Bottomless Bottle GI (glass + stopper)
#include "objects/object_tk/object_tk.h"

#include "mods/mm_sources/objects/object_gi_masks_all.h"
#include "mods/mm_sources/objects/object_mm_rando_items.h"
#include "mods/mm_sources/objects/object_gi_bottle_21.h"
#include "mods/transformation_masks/assets/mm_asset_loader.h"

#include "objects/object_poh/object_poh.h"
#include "mods/equipment/objects/ikaxe_DL/header.h" // gIKAxeInlineDL (Iron Knuckle's Axe GI model)
#include "mods/items/objects/ball_and_chainDL/header.h"
#include "mods/items/objects/ball_and_chainDL/model.inc.c"
#include "mods/items/objects/beetle_giveDL/header.h"
#include "mods/items/objects/beetle_giveDL/model.inc.c"
#include "mods/items/objects/magic_spell_giveDL/header.h"
#include "mods/items/objects/magic_spell_giveDL/model.inc.c"
#include "mods/items/objects/fire_rodDL/header.h"
#include "mods/items/objects/fire_rodDL/model.inc.c"
#include "mods/items/objects/ice_rodDL/header.h"
#include "mods/items/objects/ice_rodDL/model.inc.c"
#include "mods/items/objects/light_rodDL/header.h"
#include "mods/items/objects/light_rodDL/Cylinder_002.c"
#include "mods/items/objects/shovel_giveDL/header.h"
#include "mods/items/objects/shovel_giveDL/model.inc.c"

// Vanilla GI equipment models (for ext equipment recolor draws)
#include "objects/object_gi_sword_1/object_gi_sword_1.h"
#include "objects/object_gi_hammer/object_gi_hammer.h"
#include "objects/object_gi_longsword/object_gi_longsword.h"
#include "objects/object_gi_shield_2/object_gi_shield_2.h"
#include "objects/object_gi_clothes/object_gi_clothes.h"
#include "objects/object_gi_medal/object_gi_medal.h" // Sage's Tunic medallion ring
#include "objects/object_gi_hoverboots/object_gi_hoverboots.h"
#include "objects/object_gi_boots_2/object_gi_boots_2.h" // Climb Boots stand-in (Skijer's NEI)

// Extended equipment models (DLs already compiled in equip_ikaxe.c / equip_breastplate.c)
#include "mods/equipment/objects/ikaxe_DL/header.h"

extern PlayState* gPlayState;
extern SaveContext gSaveContext;
}

const char* SmallBodyCvarValue[10] = {
    CVAR_COSMETIC("Key.ForestSmallBody.Value"), CVAR_COSMETIC("Key.FireSmallBody.Value"),
    CVAR_COSMETIC("Key.WaterSmallBody.Value"),  CVAR_COSMETIC("Key.SpiritSmallBody.Value"),
    CVAR_COSMETIC("Key.ShadowSmallBody.Value"), CVAR_COSMETIC("Key.WellSmallBody.Value"),
    CVAR_COSMETIC("Key.GTGSmallBody.Value"),    CVAR_COSMETIC("Key.FortSmallBody.Value"),
    CVAR_COSMETIC("Key.GanonsSmallBody.Value"), CVAR_COSMETIC("Key.ChestGameSmallBody.Value"),
};

const char* SmallEmblemCvarValue[10] = {
    CVAR_COSMETIC("Key.ForestSmallEmblem.Value"), CVAR_COSMETIC("Key.FireSmallEmblem.Value"),
    CVAR_COSMETIC("Key.WaterSmallEmblem.Value"),  CVAR_COSMETIC("Key.SpiritSmallEmblem.Value"),
    CVAR_COSMETIC("Key.ShadowSmallEmblem.Value"), CVAR_COSMETIC("Key.WellSmallEmblem.Value"),
    CVAR_COSMETIC("Key.GTGSmallEmblem.Value"),    CVAR_COSMETIC("Key.FortSmallEmblem.Value"),
    CVAR_COSMETIC("Key.GanonsSmallEmblem.Value"), CVAR_COSMETIC("Key.ChestGameEmblem.Value"),
};

Color_RGB8 SmallEmblemDefaultValue[10] = {
    { 4, 195, 46 },    // Forest
    { 237, 95, 95 },   // Fire
    { 85, 180, 223 },  // Water
    { 222, 158, 47 },  // Spirit
    { 126, 16, 177 },  // Shadow
    { 227, 110, 255 }, // Well
    { 221, 212, 60 },  // GTG
    { 255, 255, 255 }, // Fortress
    { 80, 80, 80 },    // Ganons
    { 255, 255, 255 }, // Chest Game
};

Color_RGB8 MapOrCompassColor[10] = {
    { 4, 100, 46 },    // Deku Tree
    { 140, 30, 30 },   // Dodongo's Cavern
    { 30, 60, 255 },   // Jabu Jabu's Belly
    { 4, 195, 46 },    // Forest Temple
    { 237, 95, 95 },   // Fire Temple
    { 85, 180, 223 },  // Water Temple
    { 222, 158, 47 },  // Spirit Temple
    { 126, 16, 177 },  // Shadow Temple
    { 227, 110, 255 }, // Bottom of the Well
    { 0, 255, 255 },   // Ice Cavern
};

extern "C" u8 Randomizer_GetSettingValue(RandomizerSettingKey randoSettingKey);

extern "C" void Randomizer_DrawSmallKey(PlayState* play, GetItemEntry* getItemEntry) {
    bool isCustomKeysEnabled = CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("CustomKeyModels"), 1);
    int slot = getItemEntry->drawItemId - RG_FOREST_TEMPLE_SMALL_KEY;

    Gfx* customIconDLs[] = {
        (Gfx*)gSmallKeyIconForestTempleDL,         (Gfx*)gSmallKeyIconFireTempleDL,
        (Gfx*)gSmallKeyIconWaterTempleDL,          (Gfx*)gSmallKeyIconSpiritTempleDL,
        (Gfx*)gSmallKeyIconShadowTempleDL,         (Gfx*)gSmallKeyIconBottomoftheWellDL,
        (Gfx*)gSmallKeyIconGerudoTrainingGroundDL, (Gfx*)gSmallKeyIconGerudoFortressDL,
        (Gfx*)gSmallKeyIconGanonsCastleDL,         (Gfx*)gSmallKeyIconTreasureChestGameDL,
    };

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    Color_RGB8 keyColor = { 255, 255, 255 };
    keyColor = CVarGetColor24(SmallBodyCvarValue[slot], keyColor);

    if (isCustomKeysEnabled) {
        gDPSetEnvColor(POLY_OPA_DISP++, keyColor.r, keyColor.g, keyColor.b, 255);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gSmallKeyCustomDL);

        Gfx_SetupDL_25Xlu(play->state.gfxCtx);

        Color_RGB8 emblemColor = SmallEmblemDefaultValue[slot];
        emblemColor = CVarGetColor24(SmallEmblemCvarValue[slot], emblemColor);

        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gDPSetEnvColor(POLY_XLU_DISP++, emblemColor.r, emblemColor.g, emblemColor.b, 255);

        gSPDisplayList(POLY_XLU_DISP++, customIconDLs[slot]);

    } else {
        gDPSetGrayscaleColor(POLY_OPA_DISP++, keyColor.r, keyColor.g, keyColor.b, 255);
        gSPGrayscale(POLY_OPA_DISP++, true);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiSmallKeyDL);
        gSPGrayscale(POLY_OPA_DISP++, false);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawMap(PlayState* play, GetItemEntry* getItemEntry) {
    auto color = MapOrCompassColor[getItemEntry->drawItemId - RG_DEKU_TREE_MAP];

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, color.r, color.g, color.b, 255);

    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiDungeonMapDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawCompass(PlayState* play, GetItemEntry* getItemEntry) {
    auto color = MapOrCompassColor[getItemEntry->drawItemId - RG_DEKU_TREE_COMPASS];

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, color.r, color.g, color.b, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, color.r / 2, color.g / 2, color.b / 2, 255);

    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiCompassDL);

    POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, 5);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiCompassGlassDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawBossKey(PlayState* play, GetItemEntry* getItemEntry) {
    bool isCustomKeysEnabled = CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("CustomKeyModels"), 1);
    s16 slot = getItemEntry->drawItemId - RG_FOREST_TEMPLE_BOSS_KEY;

    std::string CvarValue[6] = {
        "gCosmetics.Key.ForestBoss", "gCosmetics.Key.FireBoss",   "gCosmetics.Key.WaterBoss",
        "gCosmetics.Key.SpiritBoss", "gCosmetics.Key.ShadowBoss", "gCosmetics.Key.GanonsBoss",
    };

    Gfx* CustomdLists[] = {
        (Gfx*)gBossKeyIconForestTempleDL, (Gfx*)gBossKeyIconFireTempleDL,   (Gfx*)gBossKeyIconWaterTempleDL,
        (Gfx*)gBossKeyIconSpiritTempleDL, (Gfx*)gBossKeyIconShadowTempleDL, (Gfx*)gBossKeyIconGanonsCastleDL,
    };

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    Color_RGB8 keyColor = { 255, 255, 0 };
    // Supposed to use CVAR_COSMETIC but I can't figure out the syntax
    keyColor = CVarGetColor24((CvarValue[slot] + "Body.Value").c_str(), keyColor);

    if (isCustomKeysEnabled) {
        gDPSetEnvColor(POLY_OPA_DISP++, keyColor.r, keyColor.g, keyColor.b, 255);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBossKeyCustomDL);
    } else {
        if (CVarGetInteger((CvarValue[slot] + "Body.Changed").c_str(), false)) {
            gDPSetGrayscaleColor(POLY_OPA_DISP++, keyColor.r, keyColor.g, keyColor.b, 255);
            gSPGrayscale(POLY_OPA_DISP++, true);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiBossKeyDL);
            gSPGrayscale(POLY_OPA_DISP++, false);
        } else {
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiBossKeyDL);
        }
    }

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    Color_RGB8 gemColor = { 255, 0, 0 };
    gemColor = CVarGetColor24((CvarValue[slot] + "Gem.Value").c_str(), gemColor);

    if (isCustomKeysEnabled) {
        gDPSetEnvColor(POLY_XLU_DISP++, gemColor.r, gemColor.g, gemColor.b, 255);
        gSPDisplayList(POLY_XLU_DISP++, CustomdLists[slot]);
    } else {
        if (CVarGetInteger((CvarValue[slot] + "Gem.Changed").c_str(), false)) {
            gDPSetGrayscaleColor(POLY_XLU_DISP++, gemColor.r, gemColor.g, gemColor.b, 255);
            gSPGrayscale(POLY_XLU_DISP++, true);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiBossKeyGemDL);
            gSPGrayscale(POLY_XLU_DISP++, false);
        } else {
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiBossKeyGemDL);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawKeyRing(PlayState* play, GetItemEntry* getItemEntry) {
    bool isCustomKeysEnabled = CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("CustomKeyModels"), 1);
    int slot = getItemEntry->drawItemId - RG_FOREST_TEMPLE_KEY_RING;

    Gfx* CustomIconDLs[] = {
        (Gfx*)gKeyringIconForestTempleDL,         (Gfx*)gKeyringIconFireTempleDL,
        (Gfx*)gKeyringIconWaterTempleDL,          (Gfx*)gKeyringIconSpiritTempleDL,
        (Gfx*)gKeyringIconShadowTempleDL,         (Gfx*)gKeyringIconBottomoftheWellDL,
        (Gfx*)gKeyringIconGerudoTrainingGroundDL, (Gfx*)gKeyringIconGerudoFortressDL,
        (Gfx*)gKeyringIconGanonsCastleDL,         (Gfx*)gKeyringIconTreasureChestGameDL,
    };

    Gfx* CustomKeysDLs[] = {
        (Gfx*)gKeyringKeysForestTempleDL,         (Gfx*)gKeyringKeysFireTempleDL,
        (Gfx*)gKeyringKeysWaterTempleDL,          (Gfx*)gKeyringKeysSpiritTempleDL,
        (Gfx*)gKeyringKeysShadowTempleDL,         (Gfx*)gKeyringKeysBottomoftheWellDL,
        (Gfx*)gKeyringKeysGerudoTrainingGroundDL, (Gfx*)gKeyringKeysGerudoFortressDL,
        (Gfx*)gKeyringKeysGanonsCastleDL,         (Gfx*)gKeyringKeysTreasureChestGameDL,
    };

    Gfx* CustomKeysMQDLs[] = {
        (Gfx*)gKeyringKeysForestTempleMQDL,         (Gfx*)gKeyringKeysFireTempleMQDL,
        (Gfx*)gKeyringKeysWaterTempleMQDL,          (Gfx*)gKeyringKeysSpiritTempleMQDL,
        (Gfx*)gKeyringKeysShadowTempleMQDL,         (Gfx*)gKeyringKeysBottomoftheWellMQDL,
        (Gfx*)gKeyringKeysGerudoTrainingGroundMQDL, (Gfx*)gKeyringKeysGerudoFortressDL,
        (Gfx*)gKeyringKeysGanonsCastleMQDL,         (Gfx*)gKeyringKeysTreasureChestGameDL,
    };

    // RANDOTODO make DungeonInfo static and vanilla accessible to allow all these key model data vars to be stored
    // there. (Rando::DungeonKey)0 means the keyring is not tied to a dungeon and should not be checked for an MQ
    // variant
    Rando::DungeonKey SlotToDungeon[10] = {
        Rando::FOREST_TEMPLE, Rando::FIRE_TEMPLE,        Rando::WATER_TEMPLE,           Rando::SPIRIT_TEMPLE,
        Rando::SHADOW_TEMPLE, Rando::BOTTOM_OF_THE_WELL, Rando::GERUDO_TRAINING_GROUND,
        (Rando::DungeonKey)0, // Gerudo Fortress
        Rando::GANONS_CASTLE,
        (Rando::DungeonKey)0, // Treasure Chest Game
    };

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    Color_RGB8 keyColor = { 255, 255, 255 };
    keyColor = CVarGetColor24(SmallBodyCvarValue[slot], keyColor);

    if (isCustomKeysEnabled) {
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);

        gDPSetEnvColor(POLY_OPA_DISP++, keyColor.r, keyColor.g, keyColor.b, 255);
        if (SlotToDungeon[slot] != 0 && Rando::Context::GetInstance()->GetDungeon(SlotToDungeon[slot])->IsMQ()) {
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)CustomKeysMQDLs[slot]);
        } else {
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)CustomKeysDLs[slot]);
        }

        Color_RGB8 ringColor = { 255, 255, 255 };
        ringColor = CVarGetColor24(CVAR_COSMETIC("Key.KeyringRing.Value"), ringColor);
        gDPSetEnvColor(POLY_OPA_DISP++, ringColor.r, ringColor.g, ringColor.b, 255);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gKeyringRingDL);

        Color_RGB8 emblemColor = SmallEmblemDefaultValue[slot];
        emblemColor = CVarGetColor24(SmallEmblemCvarValue[slot], emblemColor);

        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gDPSetEnvColor(POLY_OPA_DISP++, emblemColor.r, emblemColor.g, emblemColor.b, 255);

        gSPDisplayList(POLY_OPA_DISP++, CustomIconDLs[slot]);
    } else {
        gDPSetGrayscaleColor(POLY_OPA_DISP++, keyColor.r, keyColor.g, keyColor.b, 255);
        gSPGrayscale(POLY_OPA_DISP++, true);
        Matrix_Scale(0.5f, 0.5f, 0.5f, MTXMODE_APPLY);
        Matrix_RotateZ(0.8f, MTXMODE_APPLY);
        Matrix_RotateX(-2.16f, MTXMODE_APPLY);
        Matrix_RotateY(-0.56f, MTXMODE_APPLY);
        Matrix_RotateZ(-0.86f, MTXMODE_APPLY);
        Matrix_Translate(28.29f, 0, 0, MTXMODE_APPLY);
        Matrix_Translate(-(3.12f * 2), -(-0.34f * 2), -(17.53f * 2), MTXMODE_APPLY);
        Matrix_RotateX(-(-0.31f * 2), MTXMODE_APPLY);
        Matrix_RotateY(-(0.19f * 2), MTXMODE_APPLY);
        Matrix_RotateZ(-(0.20f * 2), MTXMODE_APPLY);
        for (int i = 0; i < 5; i++) {
            gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_MODELVIEW | G_MTX_LOAD);
            Matrix_Translate(3.12f, -0.34f, 17.53f, MTXMODE_APPLY);
            Matrix_RotateX(-0.31f, MTXMODE_APPLY);
            Matrix_RotateY(0.19f, MTXMODE_APPLY);
            Matrix_RotateZ(0.20f, MTXMODE_APPLY);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiSmallKeyDL);
        }
        gSPGrayscale(POLY_OPA_DISP++, false);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawDoubleDefense(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gDPSetGrayscaleColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);

    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiHeartBorderDL);

    gSPGrayscale(POLY_XLU_DISP++, false);

    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiHeartContainerDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawMasterSword(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 1 * (play->state.frames * 1),
                                             0 * (play->state.frames * 1), 32, 32, 1, 0 * (play->state.frames * 1),
                                             0 * (play->state.frames * 1), 32, 32, 1, 0, 0, 0));

    Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);
    Matrix_RotateZ(2.1f, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)object_toki_objects_DL_001BD0);

    CLOSE_DISPS(play->state.gfxCtx);
}

Gfx* Randomizer_GetTriforcePieceDL(uint8_t index) {
    switch (index) {
        case 1:
            return (Gfx*)gTriforcePiece1DL;
        case 2:
            return (Gfx*)gTriforcePiece2DL;
        default:
            return (Gfx*)gTriforcePiece0DL;
    }
}

extern "C" void Randomizer_DrawTriforcePiece(PlayState* play, GetItemEntry getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    uint8_t current = gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected;

    Matrix_Scale(0.035f, 0.035f, 0.035f, MTXMODE_APPLY);

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    Gfx* triforcePieceDL = Randomizer_GetTriforcePieceDL(current % 3);

    gSPDisplayList(POLY_XLU_DISP++, triforcePieceDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// Separate draw function for drawing the Triforce piece when in the GI state.
// Needed for delaying showing the triforce piece slightly so the triforce shard doesn't
// suddenly snap to the new piece model or completed triforce because the piece is
// given mid textbox. Also makes it so the overworld models don't turn into the completed
// model when the player has exactly the required amount of pieces.
extern "C" void Randomizer_DrawTriforcePieceGI(PlayState* play, GetItemEntry getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    auto rando = OTRGlobals::Instance->gRandomizer;
    uint8_t current = gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected;
    bool fullTriforce = false;
    if (rando->GetRandoSettingValue(RSK_RAINBOW_BRIDGE) == RO_BRIDGE_TRIFORCE_PIECES) {
        fullTriforce = rando->GetRandoSettingValue(RSK_RAINBOW_BRIDGE_TRIFORCE_COUNT) == current;
    }
    if (rando->GetRandoSettingValue(RSK_WINCON) == RO_WINCON_TRIFORCE_PIECES) {
        fullTriforce = fullTriforce || (rando->GetRandoSettingValue(RSK_WINCON_TRIFORCE_COUNT) == current);
    }
    if (rando->GetRandoSettingValue(RSK_GANONS_BOSS_KEY) == RO_GANON_BOSS_KEY_TRIFORCE_PIECES) {
        fullTriforce = fullTriforce || (rando->GetRandoSettingValue(RSK_GBK_TRIFORCE_COUNT) == current);
    }
    if (rando->GetRandoSettingValue(RSK_GANONS_SOUL) == RO_GANONS_SOUL_TRIFORCE_PIECES) {
        fullTriforce = fullTriforce || (rando->GetRandoSettingValue(RSK_GANONS_SOUL_TRIFORCE_COUNT) == current);
    }

    Matrix_Scale(triforcePieceScale, triforcePieceScale, triforcePieceScale, MTXMODE_APPLY);

    // For creating a delay before showing the model so the model doesn't swap visually when the triforce piece
    // is given when the textbox just appears.
    if (triforcePieceScale < 0.0001f) {
        triforcePieceScale += 0.00003f;
    }

    // Animation. When not the completed triforce, create delay before showing the piece to bypass interpolation.
    // If the completed triforce, make it grow slowly.
    if (!fullTriforce) {
        if (triforcePieceScale > 0.00008f && triforcePieceScale < 0.034f) {
            triforcePieceScale = 0.034f;
        } else if (triforcePieceScale < 0.035f) {
            triforcePieceScale += 0.0005f;
        }
    } else if (triforcePieceScale > 0.00008f && triforcePieceScale < 0.035f) {
        triforcePieceScale += 0.0005f;
    }

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    // Show piece when not currently completing the triforce. Use the scale to create a delay so interpolation doesn't
    // make the triforce twitch when the size is set to a higher value.
    if (!fullTriforce && triforcePieceScale > 0.035f) {
        // Get shard DL. Remove one before division to account for triforce piece given in the textbox
        // to match up the shard from the overworld model.
        Gfx* triforcePieceDL = Randomizer_GetTriforcePieceDL((current - 1) % 3);

        gSPDisplayList(POLY_XLU_DISP++, triforcePieceDL);
    } else if (fullTriforce && triforcePieceScale > 0.00008f) {
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gTriforcePieceCompletedDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawMysteryItem(PlayState* play, GetItemEntry* getItemEntry) {
    Color_RGB8 color = { 0, 60, 100 };
    if (CVarGetInteger(CVAR_COSMETIC("World.MysteryItem.Changed"), 0)) {
        color = CVarGetColor24(CVAR_COSMETIC("World.MysteryItem.Value"), color);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gDPSetGrayscaleColor(POLY_XLU_DISP++, color.r, color.g, color.b, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);

    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gMysteryItemDL);

    gSPGrayscale(POLY_XLU_DISP++, false);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawRocsFeather(PlayState* play, GetItemEntry* getItemEntry) {
    Color_RGB8 color = { 0, 60, 100 };

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiRocsFeatherDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

Gfx* GetEmptyDlist(GraphicsContext* gfxCtx) {
    Gfx* dListHead;
    Gfx* dList;

    dList = dListHead = (Gfx*)Graph_Alloc(gfxCtx, sizeof(Gfx) * 1);

    gSPEndDisplayList(dListHead++);

    return dList;
}

extern "C" s32 OverrideLimbDrawGohma(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 255, 170, 255);

    switch (limbIndex) {
        case BOSSGOMA_LIMB_EYE:
            gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 63);
            break;

        case BOSSGOMA_LIMB_IRIS:
            gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
            break;
    }

    CLOSE_DISPS(play->state.gfxCtx);
    return false;
}

#define LIMB_COUNT_GOHMA 86
extern "C" void DrawGohma(PlayState* play) {
    static bool initialized = false;
    static SkelAnime skelAnime;
    static Vec3s jointTable[LIMB_COUNT_GOHMA];
    static Vec3s otherTable[LIMB_COUNT_GOHMA];
    static u32 lastUpdate = 0;

    if (!initialized) {
        initialized = true;
        SkelAnime_Init(play, &skelAnime, (SkeletonHeader*)&gGohmaSkel, (AnimationHeader*)&gGohmaIdleCrouchedAnim,
                       jointTable, otherTable, LIMB_COUNT_GOHMA);
    }

    if (lastUpdate != play->state.frames) {
        lastUpdate = play->state.frames;
        SkelAnime_Update(&skelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.005f, 0.005f, 0.005f, MTXMODE_APPLY);

    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)GetEmptyDlist(play->state.gfxCtx));
    SkelAnime_DrawSkeletonOpa(play, &skelAnime, OverrideLimbDrawGohma, NULL, NULL);

    CLOSE_DISPS(play->state.gfxCtx);
}

#define LIMB_COUNT_KING_DODONGO 49
extern "C" void DrawKingDodongo(PlayState* play) {
    static bool initialized = false;
    static SkelAnime skelAnime;
    static Vec3s jointTable[LIMB_COUNT_KING_DODONGO];
    static Vec3s otherTable[LIMB_COUNT_KING_DODONGO];
    static u32 lastUpdate = 0;

    if (!initialized) {
        initialized = true;
        SkelAnime_Init(play, &skelAnime, (SkeletonHeader*)&object_kingdodongo_Skel_01B310,
                       (AnimationHeader*)&object_kingdodongo_Anim_00F0D8, jointTable, otherTable,
                       LIMB_COUNT_KING_DODONGO);
    }

    if (lastUpdate != play->state.frames) {
        lastUpdate = play->state.frames;
        SkelAnime_Update(&skelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.003f, 0.003f, 0.003f, MTXMODE_APPLY);

    SkelAnime_DrawSkeletonOpa(play, &skelAnime, NULL, NULL, NULL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" s32 OverrideLimbDrawBarinade(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                        void* thisx) {
    OPEN_DISPS(play->state.gfxCtx);

    s16 unk_1AC = play->gameplayFrames * 0xC31;
    f32 unk_1A0 = 0.0f;
    f32 unk_1A4 = 0.0f;

    if (limbIndex == 20) {
        gDPPipeSync(POLY_OPA_DISP++);
        gSPSegment(POLY_OPA_DISP++, 0x08,
                   (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 8, 16, 1, 0,
                                                 (play->gameplayFrames * -2) % 64, 16, 16, 0, 0, 0, -2));
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 200);
        Matrix_RotateX(-M_PIf / 2.0f, MTXMODE_APPLY);
    } else if ((limbIndex >= 10) && (limbIndex < 20)) {
        rot->x -= 0x4000;
        *dList = NULL;
    } else if (limbIndex == 6) {
        unk_1A4 = (Math_SinS(unk_1AC) * 0.05f) + 1.0f;
        Matrix_Scale(unk_1A4, unk_1A4, unk_1A4, MTXMODE_APPLY);
    } else if (limbIndex == 61) {
        unk_1A0 = (Math_CosS(unk_1AC) * 0.1f) + 1.0f;
        Matrix_Scale(unk_1A0, unk_1A0, unk_1A0, MTXMODE_APPLY);
    } else if (limbIndex == 7) {
        rot->x -= 0xCCC;
    }

    CLOSE_DISPS(play->state.gfxCtx);
    return false;
}

extern "C" void PostLimbDrawBarinade(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    OPEN_DISPS(play->state.gfxCtx);

    if (limbIndex == 25) {
        gSPSegment(POLY_XLU_DISP++, 0x09,
                   (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, (play->gameplayFrames * 10) % 128, 16, 32, 1,
                                                 0, (play->gameplayFrames * 5) % 128, 16, 32, 0, 10, 0, 5));
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBarinadeDL_008D70);
    } else if ((limbIndex >= 10) && (limbIndex < 20)) {
        if (((limbIndex >= 16) || (limbIndex == 10))) {
            gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBarinadeDL_008BB8);
        } else if ((limbIndex >= 11)) {
            gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBarinadeDL_008BB8);
        }
    } else if ((*dList != NULL) && (limbIndex >= 29) && (limbIndex < 56)) {
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, *dList);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

#define LIMB_COUNT_BARINADE 64
extern "C" void DrawBarinade(PlayState* play) {
    static bool initialized = false;
    static SkelAnime skelAnime;
    static Vec3s jointTable[LIMB_COUNT_BARINADE];
    static Vec3s otherTable[LIMB_COUNT_BARINADE];
    static u32 lastUpdate = 0;

    if (!initialized) {
        initialized = true;
        SkelAnime_Init(play, &skelAnime, (SkeletonHeader*)&gBarinadeBodySkel, (AnimationHeader*)&gBarinadeBodyAnim,
                       jointTable, otherTable, LIMB_COUNT_BARINADE);

        // Freeze barniade on the last frame
        f32 lastFrame = Animation_GetLastFrame((AnimationHeader*)&gBarinadeBodyAnim);
        Animation_Change(&skelAnime, (AnimationHeader*)&gBarinadeBodyAnim, 1.0f, lastFrame, lastFrame, ANIMMODE_ONCE,
                         0.0f);
    }

    if (lastUpdate != play->state.frames) {
        lastUpdate = play->state.frames;
        SkelAnime_Update(&skelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(0.0f, -25.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);

    gSPSegment(POLY_OPA_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 8, 16, 1, 0,
                                             (play->gameplayFrames * -10) % 16, 16, 16, 0, 0, 0, -10));
    gSPSegment(POLY_OPA_DISP++, 0x09,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, (play->gameplayFrames * -10) % 32, 16, 0x20, 1,
                                             0, (play->gameplayFrames * -5) % 32, 16, 32, 0, -10, 0, -5));

    SkelAnime_DrawSkeletonOpa(play, &skelAnime, OverrideLimbDrawBarinade, PostLimbDrawBarinade, NULL);

    CLOSE_DISPS(play->state.gfxCtx);
}

#define LIMB_COUNT_PHANTOM_GANON 26
extern "C" void DrawPhantomGanon(PlayState* play) {
    static bool initialized = false;
    static SkelAnime skelAnime;
    static Vec3s jointTable[LIMB_COUNT_PHANTOM_GANON];
    static Vec3s otherTable[LIMB_COUNT_PHANTOM_GANON];
    static u32 lastUpdate = 0;

    if (!initialized) {
        initialized = true;
        SkelAnime_Init(play, &skelAnime, (SkeletonHeader*)&gPhantomGanonSkel,
                       (AnimationHeader*)&gPhantomGanonNeutralAnim, jointTable, otherTable, LIMB_COUNT_PHANTOM_GANON);
    }

    if (lastUpdate != play->state.frames) {
        lastUpdate = play->state.frames;
        SkelAnime_Update(&skelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, 10.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.007f, 0.007f, 0.007f, MTXMODE_APPLY);

    // Eye color
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)GetEmptyDlist(play->state.gfxCtx));
    SkelAnime_DrawSkeletonOpa(play, &skelAnime, NULL, NULL, NULL);

    CLOSE_DISPS(play->state.gfxCtx);
}

#define LIMB_COUNT_VOLVAGIA 7
extern "C" void DrawVolvagia(PlayState* play) {
    static bool initialized = false;
    static SkelAnime skelAnime;
    static Vec3s jointTable[LIMB_COUNT_VOLVAGIA];
    static Vec3s otherTable[LIMB_COUNT_VOLVAGIA];
    static u32 lastUpdate = 0;

    if (!initialized) {
        initialized = true;
        SkelAnime_Init(play, &skelAnime, (SkeletonHeader*)&gVolvagiaHeadSkel,
                       (AnimationHeader*)&gVolvagiaHeadEmergeAnim, jointTable, otherTable, LIMB_COUNT_VOLVAGIA);
    }

    if (lastUpdate != play->state.frames) {
        lastUpdate = play->state.frames;
        SkelAnime_Update(&skelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.007f, 0.007f, 0.007f, MTXMODE_APPLY);

    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)gVolvagiaEyeOpenTex);
    gSPSegment(POLY_OPA_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, play->state.frames * 4, 120, 0x20, 0x20, 1,
                                             play->state.frames * 3, play->state.frames * -2, 0x20, 0x20, 4, 0, 3, -2));

    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);

    SkelAnime_DrawSkeletonOpa(play, &skelAnime, NULL, NULL, NULL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void DrawMorpha(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);

    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, play->state.frames * 3, play->state.frames * 3, 32,
                                             32, 1, play->state.frames * -3, play->state.frames * -3, 32, 32, 3, 3, -3,
                                             -3));

    gSPSegment(POLY_XLU_DISP++, 0x09,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, play->state.frames * 3, 0, 32, 32, 1, 0,
                                             play->state.frames * -5, 32, 32, 3, 0, 0, -5));

    Matrix_RotateX(play->state.frames * 0.1f, MTXMODE_APPLY);
    Matrix_RotateZ(play->state.frames * 0.16f, MTXMODE_APPLY);

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 255, 255);

    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gMorphaCoreMembraneDL);

    gDPPipeSync(POLY_XLU_DISP++);

    gDPSetEnvColor(POLY_XLU_DISP++, 0, 220, 255, 128);
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 255, 255);

    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gMorphaCoreNucleusDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

#define LIMB_COUNT_BONGO_BONGO 27
extern "C" void DrawBongoBongo(PlayState* play) {
    static bool initialized = false;
    static SkelAnime skelAnime;
    static Vec3s jointTable[LIMB_COUNT_BONGO_BONGO];
    static Vec3s otherTable[LIMB_COUNT_BONGO_BONGO];
    static u32 lastUpdate = 0;

    if (!initialized) {
        initialized = true;
        SkelAnime_InitFlex(play, &skelAnime, (FlexSkeletonHeader*)&gBongoLeftHandSkel,
                           (AnimationHeader*)&gBongoLeftHandIdleAnim, jointTable, otherTable, LIMB_COUNT_BONGO_BONGO);
    }

    if (lastUpdate != play->state.frames) {
        lastUpdate = play->state.frames;
        SkelAnime_Update(&skelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, -25.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);

    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)GetEmptyDlist(play->state.gfxCtx));

    gDPSetPrimColor(POLY_OPA_DISP++, 0x80, 0x80, 255, 255, 255, 255);
    SkelAnime_DrawSkeletonOpa(play, &skelAnime, NULL, NULL, NULL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" s32 OverrideLimbDrawKotake(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                      void* thisx) {
    if (limbIndex == 21) { // Head
        *dList = (Gfx*)gTwinrovaKotakeHeadDL;
    }

    return false;
}

extern "C" void PostLimbDrawKotake(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    OPEN_DISPS(play->state.gfxCtx);

    if (limbIndex == 21) { // Head
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gTwinrovaKotakeIceHairDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

#define LIMB_COUNT_KOTAKE 27
extern "C" void DrawKotake(PlayState* play) {
    static bool initialized = false;
    static SkelAnime skelAnime;
    static Vec3s jointTable[LIMB_COUNT_KOTAKE];
    static Vec3s otherTable[LIMB_COUNT_KOTAKE];
    static u32 lastUpdate = 0;

    if (!initialized) {
        initialized = true;
        SkelAnime_InitFlex(play, &skelAnime, (FlexSkeletonHeader*)&gTwinrovaKotakeSkel,
                           (AnimationHeader*)&gTwinrovaKotakeKoumeFlyAnim, jointTable, otherTable, LIMB_COUNT_KOTAKE);
    }

    if (lastUpdate != play->state.frames) {
        lastUpdate = play->state.frames;
        SkelAnime_Update(&skelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(0.0f, -10.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);

    gSPSegment(POLY_OPA_DISP++, 10, (uintptr_t)gTwinrovaKotakeKoumeEyeOpenTex);
    gSPSegment(POLY_XLU_DISP++, 10, (uintptr_t)gTwinrovaKotakeKoumeEyeOpenTex);
    gSPSegment(POLY_XLU_DISP++, 8,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0 & 0x7F, 0 & 0x7F, 0x20, 0x20, 1,
                                             play->state.frames & 0x7F, (play->state.frames * -7) & 0xFF, 0x20, 0x40, 0,
                                             0, 1, -7));

    gSPSegment(POLY_XLU_DISP++, 9,
               (uintptr_t)Gfx_TexScrollEx(play->state.gfxCtx, 0 & 0x7F, play->state.frames & 0xFF, 0x20, 0x40, 0, 1));

    SkelAnime_DrawSkeletonOpa(play, &skelAnime, OverrideLimbDrawKotake, PostLimbDrawKotake, NULL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" s32 OverrideLimbDrawGanon(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    OPEN_DISPS(play->state.gfxCtx);

    if (limbIndex >= 42) { // Tail
        // Brighten up tail
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    }

    CLOSE_DISPS(play->state.gfxCtx);
    return false;
}

#define LIMB_COUNT_GANON 47
extern "C" void DrawGanon(PlayState* play) {
    static bool initialized = false;
    static SkelAnime skelAnime;
    static Vec3s jointTable[LIMB_COUNT_GANON];
    static Vec3s otherTable[LIMB_COUNT_GANON];
    static u32 lastUpdate = 0;

    if (!initialized) {
        initialized = true;
        SkelAnime_InitFlex(play, &skelAnime, (FlexSkeletonHeader*)&gGanonSkel, (AnimationHeader*)&gGanonGuardIdleAnim,
                           jointTable, otherTable, LIMB_COUNT_GANON);
    }

    if (lastUpdate != play->state.frames) {
        lastUpdate = play->state.frames;
        SkelAnime_Update(&skelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, -33.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.005f, 0.005f, 0.005f, MTXMODE_APPLY);

    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gGanonEyeOpenTex);

    SkelAnime_DrawSkeletonOpa(play, &skelAnime, OverrideLimbDrawGanon, NULL, NULL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawBeanSprout(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.3f, 0.3f, 0.3f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gMagicBeanSeedlingDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawBossSoul(PlayState* play, GetItemEntry* getItemEntry) {
    s16 slot;
    if (getItemEntry->getItemId != RG_ICE_TRAP) {
        slot = getItemEntry->getItemId - RG_GOHMA_SOUL;
    } else {
        slot = getItemEntry->drawItemId - RG_GOHMA_SOUL;
    }

    s16 flameColors[9][3] = {
        { 0, 255, 0 },     // Gohma
        { 255, 0, 100 },   // King Dodongo
        { 50, 255, 255 },  // Barinade
        { 4, 195, 46 },    // Phantom Ganon
        { 237, 95, 95 },   // Volvagia
        { 85, 180, 223 },  // Morpha
        { 126, 16, 177 },  // Bongo Bongo
        { 222, 158, 47 },  // Twinrova
        { 150, 150, 150 }, // Ganon/Dorf
    };

    // Draw the blue fire DL but coloured to the boss soul.
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 8,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0 * (play->state.frames * 0),
                                             0 * (play->state.frames * 0), 16, 32, 1, 1 * (play->state.frames * 1),
                                             -1 * (play->state.frames * 8), 16, 32, 0, 0, 1, -8));
    Matrix_Push();
    Matrix_Translate(0.0f, -70.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(5.0f, 5.0f, 5.0f, MTXMODE_APPLY);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, flameColors[slot][0], flameColors[slot][1], flameColors[slot][2], 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiBlueFireFlameDL);
    gSPGrayscale(POLY_XLU_DISP++, false);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);

    // Draw the generic boss soul model
    if (CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("SimplerBossSoulModels"), 0)) {
        OPEN_DISPS(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        if (slot == 8) { // For Ganon only...
            gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, 255);
        } else {
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 255);
        }
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBossSoulSkullDL);
        CLOSE_DISPS(play->state.gfxCtx);
        // Draw the boss' skeleton
    } else {
        switch (slot) {
            case 0: // Gohma
                DrawGohma(play);
                break;
            case 1: // King Dodongo
                DrawKingDodongo(play);
                break;
            case 2: // Barinade
                DrawBarinade(play);
                break;
            case 3: // Phantom Ganon
                DrawPhantomGanon(play);
                break;
            case 4: // Volvagia
                DrawVolvagia(play);
                break;
            case 5: // Morpha
                DrawMorpha(play);
                break;
            case 6: // Bongo Bongo
                DrawBongoBongo(play);
                break;
            case 7: // Twinrova
                DrawKotake(play);
                break;
            case 8: // Ganon
                DrawGanon(play);
                break;
            default:
                break;
        }
    }
}

extern "C" void Randomizer_DrawOcarinaButton(PlayState* play, GetItemEntry* getItemEntry) {
    Color_RGB8 aButtonColor = { 80, 150, 255 };
    if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.Changed"), 0)) {
        aButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.AButton.Value"), aButtonColor);
    } else if (CVarGetInteger(CVAR_COSMETIC("DefaultColorScheme"), COLORSCHEME_N64) == COLORSCHEME_GAMECUBE) {
        aButtonColor = { 80, 255, 150 };
    }

    Color_RGB8 cButtonsColor = { 255, 255, 50 };
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CButtons.Changed"), 0)) {
        cButtonsColor = CVarGetColor24(CVAR_COSMETIC("HUD.CButtons.Value"), cButtonsColor);
    }
    Color_RGB8 cUpButtonColor = cButtonsColor;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.Changed"), 0)) {
        cUpButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CUpButton.Value"), cUpButtonColor);
    }
    Color_RGB8 cDownButtonColor = cButtonsColor;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.Changed"), 0)) {
        cDownButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CDownButton.Value"), cDownButtonColor);
    }
    Color_RGB8 cLeftButtonColor = cButtonsColor;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.Changed"), 0)) {
        cLeftButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CLeftButton.Value"), cLeftButtonColor);
    }
    Color_RGB8 cRightButtonColor = cButtonsColor;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.Changed"), 0)) {
        cRightButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CRightButton.Value"), cRightButtonColor);
    }

    s16 slot = getItemEntry->drawItemId - RG_OCARINA_A_BUTTON;

    Gfx* dLists[] = {
        (Gfx*)gOcarinaAButtonDL,     (Gfx*)gOcarinaCUpButtonDL,    (Gfx*)gOcarinaCDownButtonDL,
        (Gfx*)gOcarinaCLeftButtonDL, (Gfx*)gOcarinaCRightButtonDL,
    };

    Color_RGB8 colors[] = {
        aButtonColor, cUpButtonColor, cDownButtonColor, cLeftButtonColor, cRightButtonColor,
    };

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gDPSetGrayscaleColor(POLY_XLU_DISP++, colors[slot].r, colors[slot].g, colors[slot].b, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);

    gSPDisplayList(POLY_XLU_DISP++, dLists[slot]);

    gSPGrayscale(POLY_XLU_DISP++, false);

    CLOSE_DISPS(play->state.gfxCtx);
}

static Gfx gGiBronzeScaleWaterColorDL[] = {
    gsDPPipeSync(),
    gsDPSetPrimColor(0, 0x60, 255, 255, 255, 255),
    gsDPSetEnvColor(255, 123, 0, 255),
    gsSPEndDisplayList(),
};

static Gfx gGiBronzeScaleColorDL[] = {
    gsDPPipeSync(),
    gsDPSetPrimColor(0, 0x80, 255, 255, 255, 255),
    gsDPSetEnvColor(91, 51, 18, 255),
    gsSPEndDisplayList(),
};

extern "C" void Randomizer_DrawBronzeScale(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 1 * (play->state.frames * 2),
                                             -1 * (play->state.frames * 2), 64, 64, 1, 1 * (play->state.frames * 4),
                                             -1 * (play->state.frames * 4), 32, 32, 2, -2, 4, -4));

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiBronzeScaleColorDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiScaleDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiBronzeScaleWaterColorDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiScaleWaterDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawPowerBracelet(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiGrabDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawLadder(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiClimbDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawKneePads(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiCrawlDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawJabberNut(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_26Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    Color_RGB8 bodyColor;
    if (CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("GenericJabberNutModel"), 0)) {
        bodyColor = CVarGetColor24(CVAR_COSMETIC("Equipment.JabberNut.Value"), Color_RGB8{ 255, 0, 216 });
        gDPSetEnvColor(POLY_OPA_DISP++, bodyColor.r, bodyColor.g, bodyColor.b, 255);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiJabbernutDL);
    } else {
        switch (getItemEntry->drawItemId) {
            case RG_SPEAK_DEKU:
                bodyColor = CVarGetColor24(CVAR_COSMETIC("Equipment.DekuJabberNut.Value"), Color_RGB8{ 255, 160, 32 });
                gDPSetEnvColor(POLY_OPA_DISP++, bodyColor.r, bodyColor.g, bodyColor.b, 255);
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiDekuJabbernutDL);
                break;
            case RG_SPEAK_GERUDO:
                bodyColor = CVarGetColor24(CVAR_COSMETIC("Equipment.GerudoJabberNut.Value"), Color_RGB8{ 128, 64, 0 });
                gDPSetEnvColor(POLY_OPA_DISP++, bodyColor.r, bodyColor.g, bodyColor.b, 255);
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiGerudoJabbernutDL);
                break;
            case RG_SPEAK_GORON:
                bodyColor = CVarGetColor24(CVAR_COSMETIC("Equipment.GoronJabberNut.Value"), Color_RGB8{ 255, 32, 0 });
                gDPSetEnvColor(POLY_OPA_DISP++, bodyColor.r, bodyColor.g, bodyColor.b, 255);
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiGoronJabbernutDL);
                break;
            case RG_SPEAK_HYLIAN:
                bodyColor = CVarGetColor24(CVAR_COSMETIC("Equipment.HylianJabberNut.Value"), Color_RGB8{ 255, 255, 0 });
                gDPSetEnvColor(POLY_OPA_DISP++, bodyColor.r, bodyColor.g, bodyColor.b, 255);
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiHylianJabbernutDL);
                break;
            case RG_SPEAK_KOKIRI:
                bodyColor =
                    CVarGetColor24(CVAR_COSMETIC("Equipment.KokiriJabberNut.Value"), Color_RGB8{ 128, 216, 48 });
                gDPSetEnvColor(POLY_OPA_DISP++, bodyColor.r, bodyColor.g, bodyColor.b, 255);
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiKokiriJabbernutDL);
                break;
            case RG_SPEAK_ZORA:
                bodyColor = CVarGetColor24(CVAR_COSMETIC("Equipment.ZoraJabberNut.Value"), Color_RGB8{ 96, 240, 255 });
                gDPSetEnvColor(POLY_OPA_DISP++, bodyColor.r, bodyColor.g, bodyColor.b, 255);
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiZoraJabbernutDL);
                break;
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawOpenChest(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiOpenChestsDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawFishingPoleGI(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiFishingPoleDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawSkeletonKey(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    Color_RGB8 keyColor = { 255, 255, 170 };
    keyColor = CVarGetColor24(CVAR_COSMETIC("Key.Skeleton.Value"), keyColor);
    gDPSetEnvColor(POLY_OPA_DISP++, keyColor.r, keyColor.g, keyColor.b, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gSkeletonKeyDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawBombchuBag(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_26Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    Color_RGB8 maskColor = { 0, 100, 150 };
    maskColor = CVarGetColor24(CVAR_COSMETIC("Equipment.ChuFace.Value"), maskColor);
    gDPSetEnvColor(POLY_OPA_DISP++, maskColor.r, maskColor.g, maskColor.b, 255);

    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBombchuBagMaskDL);

    Color_RGB8 bodyColor = { 180, 130, 50 };
    bodyColor = CVarGetColor24(CVAR_COSMETIC("Equipment.ChuBody.Value"), bodyColor);
    gDPSetEnvColor(POLY_OPA_DISP++, bodyColor.r, bodyColor.g, bodyColor.b, 255);

    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBombchuBagBodyDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Randomizer_DrawOverworldKey(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gHouseKeyDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// ============================================================================
// Custom 24 Items - 3D Models and Draw Functions
// ============================================================================

// Embedded object models - Green cubes for all items except Ice Rod (blue)
static Vtx object_rando_rocsfeatherVtx[] = {
    VTX(-10, -10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),  VTX(10, -10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),
    VTX(10, 10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),    VTX(-10, 10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),
    VTX(-10, -10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF), VTX(10, -10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),
    VTX(10, 10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),   VTX(-10, 10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),
    VTX(-10, -10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF), VTX(-10, -10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),
    VTX(-10, 10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),   VTX(-10, 10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),
    VTX(10, -10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),  VTX(10, -10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),
    VTX(10, 10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),    VTX(10, 10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),
    VTX(-10, 10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),  VTX(10, 10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),
    VTX(10, 10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),    VTX(-10, 10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),
    VTX(-10, -10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF), VTX(10, -10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),
    VTX(10, -10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),   VTX(-10, -10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),
};

Gfx gRandoRocsfeatherDL[] = {
    gsDPPipeSync(),
    gsDPSetCombineLERP(PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, SHADE, 0),
    gsDPSetPrimColor(0, 0, 0x00, 0xFF, 0x00, 0xFF),
    gsSPClearGeometryMode(G_CULL_BACK | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    gsSPSetGeometryMode(G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH),
    gsSPVertex(object_rando_rocsfeatherVtx, 24, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(4, 6, 5, 0, 4, 7, 6, 0),
    gsSP2Triangles(8, 9, 10, 0, 8, 10, 11, 0),
    gsSP2Triangles(12, 14, 13, 0, 12, 15, 14, 0),
    gsSP2Triangles(16, 17, 18, 0, 16, 18, 19, 0),
    gsSP2Triangles(20, 22, 21, 0, 20, 23, 22, 0),
    gsSPEndDisplayList(),
};

// Copy the same structure for all other items (I'll create a macro to reduce repetition)
#define DEFINE_GREEN_CUBE_ITEM(name)                                                                             \
    static Vtx object_rando_##name##Vtx[] = {                                                                    \
        VTX(-10, -10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),  VTX(10, -10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),        \
        VTX(10, 10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),    VTX(-10, 10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),        \
        VTX(-10, -10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF), VTX(10, -10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),       \
        VTX(10, 10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),   VTX(-10, 10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),       \
        VTX(-10, -10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF), VTX(-10, -10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),       \
        VTX(-10, 10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),   VTX(-10, 10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),       \
        VTX(10, -10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),  VTX(10, -10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),        \
        VTX(10, 10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),    VTX(10, 10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),        \
        VTX(-10, 10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),  VTX(10, 10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),        \
        VTX(10, 10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),    VTX(-10, 10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),        \
        VTX(-10, -10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF), VTX(10, -10, -10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),       \
        VTX(10, -10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),   VTX(-10, -10, 10, 0, 0, 0x00, 0xFF, 0x00, 0xFF),       \
    };                                                                                                           \
    Gfx gRando##name##DL[] = {                                                                                   \
        gsDPPipeSync(),                                                                                          \
        gsDPSetCombineLERP(PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, \
                           SHADE, 0),                                                                            \
        gsDPSetPrimColor(0, 0, 0x00, 0xFF, 0x00, 0xFF),                                                          \
        gsSPClearGeometryMode(G_CULL_BACK | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),          \
        gsSPSetGeometryMode(G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH),                                             \
        gsSPVertex(object_rando_##name##Vtx, 24, 0),                                                             \
        gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),                                                                  \
        gsSP2Triangles(4, 6, 5, 0, 4, 7, 6, 0),                                                                  \
        gsSP2Triangles(8, 9, 10, 0, 8, 10, 11, 0),                                                               \
        gsSP2Triangles(12, 14, 13, 0, 12, 15, 14, 0),                                                            \
        gsSP2Triangles(16, 17, 18, 0, 16, 18, 19, 0),                                                            \
        gsSP2Triangles(20, 22, 21, 0, 20, 23, 22, 0),                                                            \
        gsSPEndDisplayList(),                                                                                    \
    };

Gfx gRandoBallandChainDL[] = {
    gsSPDisplayList(g_ball_and_chain_dl),
    gsSPEndDisplayList(),
};
Gfx gRandoBeetleDL[] = {
    gsSPDisplayList(g_beetle_dl),
    gsSPEndDisplayList(),
};
// Cane of Somaria / Cane of Byrna GetItem models now live in soh.o2r
// (objects/object_somaria/g_somaria_cane_give_dl + g_byrna_cane_give_dl) and are
// drawn directly via (Gfx*)gSomariaCaneGiveDL / (Gfx*)gByrnaCaneGiveDL.
Gfx gRandoHyliagraceDL[] = {
    gsSPDisplayList(gHyliaGraceGiveDL),
    gsSPEndDisplayList(),
};
Gfx gRandoZonaipermafrostDL[] = {
    gsSPDisplayList(gZonaiPermafrostGiveDL),
    gsSPEndDisplayList(),
};
Gfx gRandoDemisedestructionDL[] = {
    gsSPDisplayList(gDemiseDestructionGiveDL),
    gsSPEndDisplayList(),
};
Gfx gRandoFirerodDL[] = {
    gsSPDisplayList(g_fire_rod_give_dl),
    gsSPEndDisplayList(),
};
Gfx gRandoIcerodDL[] = {
    gsSPDisplayList(g_ice_rod_give_dl),
    gsSPEndDisplayList(),
};
Gfx gRandoLightrodDL[] = {
    gsSPDisplayList(g_light_rod_give_dl),
    gsSPEndDisplayList(),
};
DEFINE_GREEN_CUBE_ITEM(Dominionrod)
DEFINE_GREEN_CUBE_ITEM(Magnesis)
DEFINE_GREEN_CUBE_ITEM(Stasis)
DEFINE_GREEN_CUBE_ITEM(Cryonis)

// All draw functions must be in extern "C" to work with OPEN_DISPS/CLOSE_DISPS macros
extern "C" {

// Helper: Generic rotating diamond renderer
static void DrawCustomItemDiamond(PlayState* play, Gfx* displayList, f32 scale) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);

    // Rotating animation
    s16 rotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, displayList);

    CLOSE_DISPS(play->state.gfxCtx);
}

// Helper: Opaque body + translucent overlay, both at the same scale. A Fast64 export splits its
// transparent materials into a second DL (object_nei_ultrahand's aura spheres); drawing that half
// in POLY_OPA would let it write Z and reject the opaque geometry sitting inside it.
static void DrawCustomItemDiamondOpaXlu(PlayState* play, Gfx* opaDL, Gfx* xluDL, f32 scale) {
    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    s16 rotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, opaDL);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, xluDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// Helper: Generic rotating renderer with a grayscale tint (Opaque). Mirrors DrawCustomItemDiamond
// but wraps the DL(s) in a gSPGrayscale tint pass. Pass scale <= 0 to skip Matrix_Scale entirely,
// and dl2 = NULL to draw a single DL.
static void DrawCustomItemDiamondTint(PlayState* play, Gfx* dl1, Gfx* dl2, f32 scale, u8 r, u8 g, u8 b) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if (scale > 0.0f) {
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    }

    s16 rotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, r, g, b, 255);
    gSPDisplayList(POLY_OPA_DISP++, dl1);
    if (dl2 != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, dl2);
    }
    gSPGrayscale(POLY_OPA_DISP++, false);

    CLOSE_DISPS(play->state.gfxCtx);
}

// All 24 draw functions (Skijer's custom items)
void Randomizer_DrawRocsFeatherSkijer(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, (Gfx*)gNeiRocsFeatherDL, 0.5f);
}

void Randomizer_DrawRocsCape(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, (Gfx*)gNeiRocsCapeDL, 0.6f);
}

void Randomizer_DrawWhip(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, (Gfx*)gNeiWhipDL, 0.5f);
}

void Randomizer_DrawSpinner(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, (Gfx*)gNeiSpinnerDL, 0.3f);
}

// Elemental Wand — all six rods are the same physical wand found in six places, so they share one
// draw. Stands in with the Dominion Rod mesh until a dedicated model exists (the icon, name and
// textbox already say which rod it is). Skijer's NEI
void Randomizer_DrawElementalWand(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoDominionrodDL, 2.5f);
}

void Randomizer_DrawBombArrows(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.5f, 0.5f, 0.5f, MTXMODE_APPLY);

    Matrix_RotateZ(M_PI, MTXMODE_APPLY);

    s16 rotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gNeiBombarrowsDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void Randomizer_DrawFireRod(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoFirerodDL, 0.2f);
}

void Randomizer_DrawIceRod(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoIcerodDL, 0.2f);
}

void Randomizer_DrawLightRod(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoLightrodDL, 0.2f);
}

void Randomizer_DrawDekuLeaf(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, (Gfx*)gNeiDekuLeafDL, 0.5f);
}

void Randomizer_DrawSwitchHook(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, (Gfx*)gNeiSwitchHookDL, 0.01f);
}

void Randomizer_DrawMogmaMitts(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, (Gfx*)gNeiMogmaMittsDL, 0.5f);
}

void Randomizer_DrawGustJar(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, (Gfx*)gNeiGustJarDL, 5.0f);
}

void Randomizer_DrawBallAndChain(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoBallandChainDL, 0.25f);
}

static void DrawWeaponFlameOverlay(PlayState* play, u8 r, u8 g, u8 b); // defined with the sword levels below

// Dual Cane — resolution decides which per-skill row presents (item.cpp), so each draw is static:
// Somaria roja, Pacci amarilla (same DL, different color); upgrades add the boss-soul flame.
void Randomizer_DrawCaneOfSomaria(PlayState* play, GetItemEntry* getItemEntry) {
    // Explicit red rather than trusting the mesh's own materials: Somaria red / Pacci yellow /
    // Byrna blue must read as three distinct canes from the SAME DL, so both Somaria draws tint
    // just like the Pacci ones do.
    DrawCustomItemDiamondTint(play, (Gfx*)gSomariaCaneGiveDL, NULL, 0.25f, 235, 55, 45);
}

void Randomizer_DrawCanePacci(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamondTint(play, (Gfx*)gSomariaCaneGiveDL, NULL, 0.25f, 255, 215, 70);
}

void Randomizer_DrawCaneSomariaUpgrade(PlayState* play, GetItemEntry* getItemEntry) {
    DrawWeaponFlameOverlay(play, 255, 60, 60);
    DrawCustomItemDiamondTint(play, (Gfx*)gSomariaCaneGiveDL, NULL, 0.25f, 235, 55, 45);
}

void Randomizer_DrawCanePacciUpgrade(PlayState* play, GetItemEntry* getItemEntry) {
    DrawWeaponFlameOverlay(play, 255, 215, 70);
    DrawCustomItemDiamondTint(play, (Gfx*)gSomariaCaneGiveDL, NULL, 0.25f, 255, 215, 70);
}

// Ultrahand — the one Pacci skill with a model of its own (the glowing green hand), so it drops the
// tinted-cane + flame stand-in the other upgrades use. Mesh is 200 units across => 0.17 puts it at
// the ~34 on-screen size every other custom get-item is calibrated to.
void Randomizer_DrawCanePacciUltrahand(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamondOpaXlu(play, (Gfx*)gNeiUltrahandDL, (Gfx*)gNeiUltrahandXluDL, 0.17f);
}

// (Randomizer_DrawRocsCape ya existía arriba — la fila nueva de RG_ROCS_CAPE lo reutiliza.)

void Randomizer_DrawDominionRod(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoDominionrodDL, 2.5f);
}

void Randomizer_DrawTimeGate(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, (Gfx*)gNeiTimeGateDL, 0.5f);
}

void Randomizer_DrawBeetle(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoBeetleDL, 0.3f);
}

void Randomizer_DrawShovel(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gShovelGiveDL_opaque_dl, 0.2f);
}

void Randomizer_DrawHyliaGrace(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoHyliagraceDL, 1.0f);
}

void Randomizer_DrawZonaiPermafrost(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoZonaipermafrostDL, 1.0f);
}

void Randomizer_DrawDemiseDestruction(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoDemisedestructionDL, 1.0f);
}

void Randomizer_DrawMagnesis(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoMagnesisDL, 2.5f);
}

void Randomizer_DrawStasis(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoStasisDL, 2.5f);
}

void Randomizer_DrawLantern(PlayState* play, GetItemEntry* getItemEntry) {
    // Poe Lantern model from object_poh — modeled at actor scale (~50-70u tall),
    // drop to 1/40× to fit the get-item cylinder.
    DrawCustomItemDiamond(play, (Gfx*)gPoeLanternDL, 0.025f);
}

void Randomizer_DrawCryonis(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, gRandoCryonisDL, 2.5f);
}

void Randomizer_DrawPokeball(PlayState* play, GetItemEntry* getItemEntry) {
    // Vtx range is ±170 units (cull box), so model is ~340 native units across.
    // Get-item cylinder is ~60 units, so 60/340 ≈ 0.18 fits.
    DrawCustomItemDiamond(play, (Gfx*)gNeiPokeballDL, 0.18f);
}

void Randomizer_DrawMinishCap(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, (Gfx*)gNeiMinishCapDL, 0.5f);
}

// Mario Mask — the mask the MM decomp XML literally labels "Mario Mask": mask_03
// on the Happy Mask Salesman's backpack. All ten backpack masks live inline inside
// one display list (gHappyMaskSalesmanBackpackDL), so object_nei_mario_mask/ carries
// that chunk plus its own copy of the CI8 texture, the palette and the 12 verts —
// self-contained like the other custom get-item models, no mm.o2r needed.
// Verts are recentred on the mask (they originally sat ~2300 units off-origin,
// where it hangs on the backpack) and span 1564 units, so 60/1564 ≈ 0.038.
//
// Two fixes over the generic helper: the plate is modelled lying flat (it hangs
// facing up off the backpack), so tilt it upright to face the camera; and it is a
// single-sided plate, so culling is disabled or it vanishes for half of the spin.
void Randomizer_DrawMarioMask(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.038f, 0.038f, 0.038f, MTXMODE_APPLY);
    Matrix_RotateX(-M_PIf / 2.0f, MTXMODE_APPLY);
    Matrix_RotateY(play->gameplayFrames * 0x2 * 0.01f, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BOTH);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gNeiMarioMaskDL);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_CULL_BACK);

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// Extended Equipment Get-Item Draw
// =============================================================================

void Randomizer_DrawExtCaneOfByrna(PlayState* play, GetItemEntry* getItemEntry) {
    // Blue Byrna cane (same mesh as Somaria, blue materials) — from soh.o2r.
    DrawCustomItemDiamond(play, (Gfx*)gByrnaCaneGiveDL, 0.25f);
}

void Randomizer_DrawNet(PlayState* play, GetItemEntry* getItemEntry) {
    // Bottle Randomizer Net (RG_NET) — the soh.o2r held model (object_nei_net). Opa DL is the
    // handle/rim/wrap; Xlu DL is the semitransparent white netting, so it needs the XLU buffer
    // (a plain DrawCustomItemDiamond would drop it). Model spans ~70 units grip->hoop, so a small
    // scale keeps it in get-item presentation range. Same rotation as DrawCustomItemDiamond.
    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Scale(0.55f, 0.55f, 0.55f, MTXMODE_APPLY); // 0.3 presentaba demasiado pequeña
    s16 netRotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(netRotation * 0.01f, MTXMODE_APPLY);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gNeiNetDL);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gNeiNetXluDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// Defined with the per-level sword draws below (upright + 45° hand-local sword presentation).
static void DrawMmWeaponGi(PlayState* play, Gfx* dl1, Gfx* dl2, f32 scale);

void Randomizer_DrawExtFourSword(PlayState* play, GetItemEntry* getItemEntry) {
    // The REAL Four Sword model (soh.o2r object_nei_four_sword, converted out of the old pak).
    // Blade + hilt are separate DLs, both authored in hand-local space like the MM swords, so they
    // take the same upright + 45° presentation. Falls back to the tinted Kokiri sword if the
    // archive is stale.
    Gfx* blade = ResourceMgr_LoadGfxByName(dgNeiFourSwordBladeDL);
    Gfx* hilt = ResourceMgr_LoadGfxByName(dgNeiFourSwordHiltDL);
    if (blade == NULL || ((const char*)blade)[0] == '_') {
        DrawCustomItemDiamondTint(play, (Gfx*)gGiKokiriSwordDL, NULL, 0.55f, 0, 180, 80);
        return;
    }
    DrawMmWeaponGi(play, blade, hilt, 0.04f);
}

// NEI Weapon Upgrades — progressive weapons. The get-item model shows the base weapon
// (level 1); the upgrade variants are conveyed via name + the in-hand model/icon swap.
void Randomizer_DrawProgressiveHammer(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, (Gfx*)gGiHammerDL, 0.5f);
}

void Randomizer_DrawProgressiveKokiriSword(PlayState* play, GetItemEntry* getItemEntry) {
    DrawCustomItemDiamond(play, (Gfx*)gGiKokiriSwordDL, 0.55f);
}

void Randomizer_DrawProgressiveMasterSword(PlayState* play, GetItemEntry* getItemEntry) {
    // Master sword mesh, sacred-blue tint
    DrawCustomItemDiamondTint(play, (Gfx*)gGiKokiriSwordDL, NULL, 0.6f, 120, 180, 255);
}

void Randomizer_DrawProgressiveBGS(PlayState* play, GetItemEntry* getItemEntry) {
    // Biggoron's Sword mesh
    DrawCustomItemDiamond(play, (Gfx*)gGiBiggoronSwordDL, 0.5f);
}

// ─── NEI progressive weapon LEVELS ────────────────────────────────────────────────────────────────
// The progressive GI resolution (item.cpp) lands on these per-level rows, so every give presents
// the mesh/text of the level actually received. MM sword meshes load archive-scoped from mm.o2r
// (the same paths WeaponUpgrade_ApplyHeldSwordDL proved in-hand); if mm.o2r is absent they fall
// back to the tinted base mesh — never invisible.

// MmAssets_LoadResource is ARCHIVE-SCOPED (ResourceIdentifier(path, 0, sMmArchive)), which is what
// makes it safe for the many paths mm.o2r and oot.o2r share, and for a DisplayList its raw pointer
// is Instructions.data() — a valid Gfx*. Do NOT swap it for a by-name loader: those go through
// archive priority and hand back OoT's copy on any colliding path.
//
// `tried` only latches on SUCCESS. It used to latch on the first attempt, so a single call made
// before mm.o2r finished mounting cached NULL forever and the item silently fell back for the rest
// of the run. When mm.o2r is genuinely absent MmAssets_IsAvailable() is false, so the retry costs
// nothing. Skijer's NEI
static Gfx* LoadMmDLOnce(const char* path, Gfx** cache, u8* tried) {
    if (!*tried && MmAssets_IsAvailable()) {
        *cache = (Gfx*)MmAssets_LoadResource(path);
        if (*cache != NULL) {
            *tried = 1;
        }
    }
    return *cache;
}

// Private copy of an MM display list with its vertex loads re-pointed at mm.o2r's OWN vertex array.
//
// Needed when the vertex array's PATH exists in both archives (object_gi_hookshotVtx_000000 is the
// case that forced this): a DL asks for vertices by hash, the handler turns that into a name and
// loads it from the DEFAULT archive, and mm.o2r sits at the LOWEST priority on purpose, so OoT
// always wins. Upstream 2Ship hit the mirror image of this and solved it the same way — its comment
// on RI_HOOKSHOT says gGiHookshotDL "is shadowed by MM's same-path mesh", so it direct-loads off the
// oot.o2r handle.
//
// The hook is gfx_vtx_hash_handler_custom: word1 of a G_VTX_OTR_HASH pair is normally a byte offset
// into the array, but "an offset greater than one million is not a real offset, so it must be a real
// pointer". Writing the resolved pointer there makes the handler use it and never consult the hash.
// Textures are left alone — their names are MM-unique, so they already resolve to MM's. Skijer's NEI
static Gfx* MmDL_WithScopedVerts(const char* dlPath, const char* vtxPath) {
    // Two-word (expanded) commands: the second word is payload, never an opcode.
    auto isTwoWord = [](uint8_t op) {
        return op == 0x20 || op == 0x24 || op == 0x25 || op == 0x27 || op == 0x31 || op == 0x32 || op == 0x33 ||
               op == 0x35 || op == 0x36 || op == 0x42;
    };

    Gfx* src = (Gfx*)MmAssets_LoadResourceStrict(dlPath);
    char* vtx = (char*)MmAssets_LoadResourceStrict(vtxPath);
    if (src == NULL || vtx == NULL) {
        // Says WHICH one failed: a typo in either path is otherwise indistinguishable from
        // "mm.o2r is not mounted", and that ambiguity cost several rounds on the Clawshot.
        SPDLOG_ERROR("[NEI] MmDL_WithScopedVerts FAILED  dl='{}' -> {}   vtx='{}' -> {}", dlPath,
                     src != NULL ? "ok" : "NULL", vtxPath, vtx != NULL ? "ok" : "NULL");
        return NULL;
    }

    size_t count = 0;
    while (count < 4096) {
        uint8_t op = (uint8_t)((src[count].words.w0 >> 24) & 0xFF);
        count++;
        if (op == 0xDF) { // G_ENDDL
            break;
        }
        if (isTwoWord(op)) {
            count++;
        }
    }

    // Kept alive for the process: the returned Gfx* is handed straight to the interpreter.
    static std::vector<std::vector<Gfx>> sPatched;
    sPatched.emplace_back(src, src + count);
    Gfx* dl = sPatched.back().data();

    int patched = 0, vtxOps = 0;
    for (size_t i = 0; i < count; i++) {
        uint8_t op = (uint8_t)((dl[i].words.w0 >> 24) & 0xFF);
        if (op == 0xDF) {
            break;
        }
        if (op == 0x32) { // G_VTX_OTR_HASH
            vtxOps++;
            uintptr_t offset = (uintptr_t)dl[i].words.w1;
            if (offset <= 0xFFFFF) { // still an offset, not an already-resolved pointer
                dl[i].words.w1 = (uintptr_t)(vtx + offset);
                patched++;
            }
            i++; // skip the hash word
        } else if (isTwoWord(op)) {
            i++;
        }
    }
    // patched == 0 would mean this DL does NOT reference its vertices by hash (segment addressing
    // instead), i.e. the whole approach misses and the vertices still come from whatever segment 6
    // points at — which during a get-item is the OoT object the engine loaded.
    SPDLOG_ERROR("[NEI] MmDL_WithScopedVerts '{}': {} instr, {} vtx ops, {} patched", dlPath, count, vtxOps, patched);
    return dl;
}

// MM's OWN get-item sword models (object_gi_sword_2/3/4), drawn with MM's own draw code:
// z_draw.c's table gives Razor and Gilded GetItem_DrawOpa01 (both DLs opaque) and the Great
// Fairy's Sword GetItem_DrawOpa0Xlu1 (blade opaque + hilt emblem translucent). Those routines —
// like SoH's GetItem_DrawOpa0 — apply NO scale and NO rotation: a GI model is already authored in
// the get-item pose, and the item-get animation supplies the matrix. That is why the hand-held
// DLs looked wrong here no matter the angle: they are arm-local meshes, not GI models.
// For meshes authored in HAND-LOCAL space (they are held-weapon models, not GI models): stand them
// up and tilt them into a get-item pose by hand. Only the Four Sword needs this now — the MM sword
// levels moved to their real GI models above. Spin around world-up, then upright, then tilt, then
// shrink; ~1.8 rad is what makes a hand-local blade read like the Master Sword GI (45° left it
// nearly horizontal).
static void DrawMmWeaponGi(PlayState* play, Gfx* dl1, Gfx* dl2, f32 scale) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    s16 rotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
    Matrix_RotateX(-M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_RotateZ(1.8f, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, dl1);
    if (dl2 != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, dl2);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawMmGiModel(PlayState* play, Gfx* dl0, Gfx* dl1, u8 dl1IsXlu) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, dl0);
    if (dl1 != NULL) {
        if (dl1IsXlu) {
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPDisplayList(POLY_XLU_DISP++, dl1);
        } else {
            gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPDisplayList(POLY_OPA_DISP++, dl1);
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// Boss-soul style flame behind an awakened weapon (billboarded blue-fire DL, grayscale-tinted) —
// the visual language for "this is the upgraded form". Push/pop so the weapon draw that follows
// starts from the untouched GI matrix.
static void DrawWeaponFlameOverlay(PlayState* play, u8 r, u8 g, u8 b) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 8,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 16, 32, 1, 1 * (play->state.frames * 1),
                                             -1 * (play->state.frames * 8), 16, 32, 0, 0, 1, -8));
    Matrix_Push();
    Matrix_Translate(0.0f, -70.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(5.0f, 5.0f, 5.0f, MTXMODE_APPLY);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, r, g, b, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiBlueFireFlameDL);
    gSPGrayscale(POLY_XLU_DISP++, false);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

void Randomizer_DrawRazorSword(PlayState* play, GetItemEntry* getItemEntry) {
    static Gfx* sMain = NULL;
    static Gfx* sEmpty = NULL;
    static u8 sMainTried = 0, sEmptyTried = 0;
    Gfx* main = LoadMmDLOnce("objects/object_gi_sword_2/gGiRazorSwordDL", &sMain, &sMainTried);
    Gfx* empty = LoadMmDLOnce("objects/object_gi_sword_2/gGiRazorSwordEmptyDL", &sEmpty, &sEmptyTried);
    if (main == NULL) {
        DrawCustomItemDiamondTint(play, (Gfx*)gGiKokiriSwordDL, NULL, 0.55f, 200, 200, 255);
        return;
    }
    DrawMmGiModel(play, main, empty, false);
}

void Randomizer_DrawGildedSword(PlayState* play, GetItemEntry* getItemEntry) {
    static Gfx* sMain = NULL;
    static Gfx* sEmpty = NULL;
    static u8 sMainTried = 0, sEmptyTried = 0;
    Gfx* main = LoadMmDLOnce("objects/object_gi_sword_3/gGiGildedSwordDL", &sMain, &sMainTried);
    Gfx* empty = LoadMmDLOnce("objects/object_gi_sword_3/gGiGildedSwordEmptyDL", &sEmpty, &sEmptyTried);
    if (main == NULL) {
        DrawCustomItemDiamondTint(play, (Gfx*)gGiKokiriSwordDL, NULL, 0.55f, 255, 210, 60);
        return;
    }
    DrawMmGiModel(play, main, empty, false);
}

void Randomizer_DrawGreatFairySword(PlayState* play, GetItemEntry* getItemEntry) {
    static Gfx* sBlade = NULL;
    static Gfx* sEmblem = NULL;
    static u8 sBladeTried = 0, sEmblemTried = 0;
    Gfx* blade = LoadMmDLOnce("objects/object_gi_sword_4/gGiGreatFairysSwordBladeDL", &sBlade, &sBladeTried);
    Gfx* emblem = LoadMmDLOnce("objects/object_gi_sword_4/gGiGreatFairysSwordHiltEmblemDL", &sEmblem, &sEmblemTried);
    if (blade == NULL) {
        DrawCustomItemDiamondTint(play, (Gfx*)gGiBiggoronSwordDL, NULL, 0.5f, 220, 120, 220);
        return;
    }
    DrawMmGiModel(play, blade, emblem, true); // el emblema del pomo va en XLU (así lo dibuja MM)
}

void Randomizer_DrawTrueMasterSword(PlayState* play, GetItemEntry* getItemEntry) {
    // Sacred-blue boss-soul flame + the real Master Sword mesh (pedestal object).
    DrawWeaponFlameOverlay(play, 120, 180, 255);
    Randomizer_DrawMasterSword(play, getItemEntry);
}

void Randomizer_DrawIronKnuckleAxe(PlayState* play, GetItemEntry* getItemEntry) {
    // The axe has no GI model anywhere: gIKAxeInlineDL is object_ik's ACTOR-scale weapon, measured
    // at 3624 x 726 x 7550 units, lying along its own +Z. The Cane of Byrna give model (the size
    // reference asked for) is 181 x 340 x 334 presented at 0.25 — about 85 units of shaft on
    // screen. Matching that shaft: 85 / 7550 = 0.011 (0.18 was ~16x too big).
    //   • RotateX(-90°) turns the model's long +Z into world up, so the handle stands like the
    //     cane's shaft instead of pointing at the camera.
    //   • Same plain Y spin as the cane — no diagonal tilt, since the cane has none either.
    //   • The mesh sits off-centre on its long axis (Z -5346..2204, centre -1571), so it is
    //     recentred in model space or it would orbit well off the presentation point.
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    s16 rotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
    Matrix_RotateX(-M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Scale(0.011f, 0.011f, 0.011f, MTXMODE_APPLY);
    Matrix_Translate(0.0f, 0.0f, 1571.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gIKAxeInlineDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Resolve-then-draw for custom soh.o2r models (defined with the other NEI gem draws below).
static void DrawCustomItemDiamondByPath(PlayState* play, const char* path, Gfx** cache, u8* tried, f32 scale);

void Randomizer_DrawQuartzOfMotion(PlayState* play, GetItemEntry* getItemEntry) {
    // Skijer's Blender model, exported to soh.o2r by blend_to_xml.py (textured + lit, 147 tris).
    // The mesh is 135 units tall and centered on its bounding box, so 0.25 puts it at ~34 units on
    // screen — the same apparent size as object_nei_divine_shield (68 tall drawn at 0.5). Re-derive
    // this if the model is re-exported: draw scale = 34 / mesh height.
    static Gfx* c = NULL;
    static u8 t = 0;
    DrawCustomItemDiamondByPath(play, "__OTR__objects/object_nei_quartz_of_motion/gNeiQuartzOfMotionDL", &c, &t, 0.25f);
}

void Randomizer_DrawUltrashot(PlayState* play, GetItemEntry* getItemEntry) {
    // Longshot mesh + light-gold flame (its kaleido marker is the Light Medallion).
    DrawWeaponFlameOverlay(play, 255, 240, 130);
    DrawCustomItemDiamond(play, (Gfx*)gGiLongshotDL, 0.5f);
}

void Randomizer_DrawExtDivineShield(PlayState* play, GetItemEntry* getItemEntry) {
    // Custom Divine Shield model from soh.o2r (object_nei_divine_shield). 0.9: at 0.5 it presented
    // noticeably smaller than the vanilla shield GIs.
    DrawCustomItemDiamond(play, (Gfx*)gNeiDivineShieldDL, 0.9f);
}

void Randomizer_DrawExtSheikahShield(PlayState* play, GetItemEntry* getItemEntry) {
    // Custom Kite Shield model from soh.o2r (object_nei_kite_shield). 0.9: same size bump as Divine.
    DrawCustomItemDiamond(play, (Gfx*)gNeiKiteShieldDL, 0.9f);
}

extern void* TransformMasks_LoadMmDL(const char* path);

void Randomizer_DrawExtShieldOfIkana(PlayState* play, GetItemEntry* getItemEntry) {
    // Use the same MM Mirror Shield model that the equipped Shield of Ikana renders with.
    // ExtEquip_LoadMmShieldDLs in extended_equipment.c proves this path resolves cleanly in
    // mm.o2r — using object_gi_shield_3/gGiMirrorShieldDL crashed because its vertex hashes
    // didn't resolve in the OTR pack, and the unresolved bytes were executed as gsSPVertex.
    static Gfx* sCachedMmShieldDL = NULL;
    static u8 sLoadAttempted = 0;
    if (!sLoadAttempted) {
        sLoadAttempted = 1;
        sCachedMmShieldDL = (Gfx*)TransformMasks_LoadMmDL("objects/object_link_child/gLinkHumanMirrorShieldDL");
    }
    if (sCachedMmShieldDL == NULL) {
        return; // mm.o2r not present — silent skip instead of crashing on a NULL DL
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    // gLinkHumanMirrorShieldDL is modelled in arm-local space (oriented for Link's left arm
    // joint). Apply order matters: RotateY first so the spin happens around world-up, THEN
    // tilt +90° X so the shield stands upright facing the camera like the other shield GIs
    // (with -90° it presented face-DOWN).
    s16 rotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
    Matrix_RotateX(M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Scale(0.035f, 0.035f, 0.035f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, sCachedMmShieldDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

void Randomizer_DrawExtMagicCape(PlayState* play, GetItemEntry* getItemEntry) {
    // Own hanging-cloth model (object_nei_magic_cape) — no longer the tinted tunic. Two authored
    // vertex poses alternate every few frames (the same trick the worn cape's gMant1Vtx/gMant2Vtx
    // swap uses) so the cloth waves while it hangs. Both poses are plain soh.o2r XML DLs resolved
    // by path — fully self-contained, no shared segments, nothing to crash.
    const char* dl = ((play->gameplayFrames >> 3) & 1) ? "__OTR__objects/object_nei_magic_cape/gNeiMagicCapeWaveDL"
                                                       : "__OTR__objects/object_nei_magic_cape/gNeiMagicCapeDL";
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    s16 rotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0.0f, 22.0f, 0.0f, MTXMODE_APPLY); // colgada desde arriba del cilindro GI
    Matrix_Scale(0.55f, 0.55f, 0.55f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl);
    CLOSE_DISPS(play->state.gfxCtx);
}

void Randomizer_DrawExtSpiritBreastplate(PlayState* play, GetItemEntry* getItemEntry) {
    // Spirit Tunic (Skijer 2026-07-16): plain vanilla tunic get-item model tinted ORANGE — the armor
    // composite is gone (recolor tunic now), same DrawCustomItemDiamondTint as Cape/Champion.
    DrawCustomItemDiamondTint(play, (Gfx*)gGiTunicCollarDL, (Gfx*)gGiTunicDL, -1.0f, 235, 110, 20);
}

void Randomizer_DrawExtSagesTunic(PlayState* play, GetItemEntry* getItemEntry) {
    // Sage's Tunic: vanilla tunic model tinted WHITE, with the 6 medallions that feed it launching
    // out of it in angled ballistic arcs — the Triforce Thief drop-launch look (angled velocity +
    // gravity + spin while airborne), staggered into a continuous fountain. Each medallion pops in
    // at the tunic and shrinks out at the end of its arc so the loop restart never snaps visibly.
    static Gfx* const sMedallionFaces[6] = {
        (Gfx*)gGiForestMedallionFaceDL, (Gfx*)gGiFireMedallionFaceDL,   (Gfx*)gGiWaterMedallionFaceDL,
        (Gfx*)gGiSpiritMedallionFaceDL, (Gfx*)gGiShadowMedallionFaceDL, (Gfx*)gGiLightMedallionFaceDL,
    };
    const f32 kV0 = 1.5f;       // outward launch speed (model units/frame)
    const f32 kUpBias = 1.5f;   // added to every launch's vertical speed (fountain lift)
    const f32 kGravity = 0.07f; // per-frame² pull on the arcs
    const s32 kCycle = 40;      // frames airborne per medallion
    const s32 kStagger = 7;     // launch offset between medallions
    const f32 kScale = 0.35f;   // per-medallion shrink (medallion mesh spans ±39)
    const f32 kZOffset = 14.0f; // in front of the tunic plane (its z tops out at 9) so depth never eats them
    s16 rotation = play->gameplayFrames * 0x2;

    OPEN_DISPS(play->state.gfxCtx);
    for (s32 i = 0; i < 6; i++) {
        f32 t = (f32)((play->gameplayFrames + i * kStagger) % kCycle);
        f32 theta = (M_PI / 2.0f) + i * (f32)(M_PI / 3.0f); // 6 launch directions in the screen plane
        f32 vx = cosf(theta) * kV0;
        f32 vy = sinf(theta) * kV0 + kUpBias;
        f32 scale = kScale;

        if (t < 4.0f) {
            scale *= t / 4.0f; // pop-in at the tunic
        } else if (t >= kCycle - 9.0f) {
            scale *= (kCycle - 1.0f - t) / 8.0f; // shrink-out at the arc's end
        }
        if (scale <= 0.001f) {
            continue;
        }

        // Exact recipe of GetItem_DrawEggOrMedallion (z_draw.c): the medallion DLs need the
        // SETUPDL_26 state — under 25 they render nothing.
        Gfx_SetupDL_26Opa(play->state.gfxCtx);
        Matrix_Push();
        Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY); // the same spin DrawCustomItemDiamondTint applies
        Matrix_Translate(vx * t, vy * t - 0.5f * kGravity * t * t, kZOffset, MTXMODE_APPLY);
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        Matrix_RotateY(play->gameplayFrames * 0.09f + i, MTXMODE_APPLY); // spin while flying
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_OPA_DISP++, sMedallionFaces[i]);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiMedallionDL);
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);

    // Drawn last: the tint helper mutates the current matrix without restoring it.
    DrawCustomItemDiamondTint(play, (Gfx*)gGiTunicCollarDL, (Gfx*)gGiTunicDL, -1.0f, 240, 244, 250);
}

void Randomizer_DrawExtChampionsTunic(PlayState* play, GetItemEntry* getItemEntry) {
    // Tunic model with BotW champion blue
    DrawCustomItemDiamondTint(play, (Gfx*)gGiTunicCollarDL, (Gfx*)gGiTunicDL, -1.0f, 0, 120, 215);
}

// The hover-boots GI colors its i4 textures through per-section prim/env colors (brown leather:
// cloth prim 80,40,0 / env 40,20,0 ≈ 22% luminance), so a multiplicative grayscale tint can only
// darken — the cloth went near-black. Remap each prim/env color onto the icon's crimson ramp
// instead (gItemIconPegasusBootsTex: body ≈ 85,14,23, highlights ≈ 154,58,65) in a one-time local
// copy of the DL, and draw that untinted at vanilla GI size.
static uint32_t Pegasus_CrimsonRamp(uint32_t rgba) {
    uint8_t r = (rgba >> 24) & 0xFF, g = (rgba >> 16) & 0xFF, b = (rgba >> 8) & 0xFF, a = rgba & 0xFF;
    float lum = 0.299f * r + 0.587f * g + 0.114f * b;
    float nrF = lum * 1.7f;
    uint8_t nr = (uint8_t)(nrF > 255.0f ? 255.0f : nrF);
    uint8_t ng = (uint8_t)(lum * 0.35f);
    uint8_t nb = (uint8_t)(lum * 0.42f);
    return ((uint32_t)nr << 24) | ((uint32_t)ng << 16) | ((uint32_t)nb << 8) | a;
}

static Gfx* Pegasus_GetRecoloredBootsDL() {
    static std::vector<Gfx> sDL;
    if (!sDL.empty()) {
        return sDL.data();
    }
    // Same two-word (expanded) command set as MmDL_WithScopedVerts below.
    auto isTwoWord = [](uint8_t op) {
        return op == 0x20 || op == 0x24 || op == 0x25 || op == 0x27 || op == 0x31 || op == 0x32 || op == 0x33 ||
               op == 0x35 || op == 0x36 || op == 0x42;
    };
    Gfx* src = (Gfx*)ResourceMgr_LoadGfxByName((char*)gGiHoverBootsDL);
    if (src == NULL) {
        return NULL;
    }
    size_t count = 0;
    while (count < 4096) {
        uint8_t op = (uint8_t)((src[count].words.w0 >> 24) & 0xFF);
        count++;
        if (op == 0xDF) { // G_ENDDL
            break;
        }
        if (isTwoWord(op)) {
            count++;
        }
    }
    sDL.assign(src, src + count);
    for (size_t i = 0; i < sDL.size(); i++) {
        uint8_t op = (uint8_t)((sDL[i].words.w0 >> 24) & 0xFF);
        if (op == 0xDF) {
            break;
        }
        if (op == G_SETPRIMCOLOR || op == G_SETENVCOLOR) {
            sDL[i].words.w1 = (uintptr_t)Pegasus_CrimsonRamp((uint32_t)sDL[i].words.w1);
        } else if (isTwoWord(op)) {
            i++; // payload word, never an opcode
        }
    }
    return sDL.data();
}

void Randomizer_DrawExtPegasusAnklet(PlayState* play, GetItemEntry* getItemEntry) {
    Gfx* dl = Pegasus_GetRecoloredBootsDL();

    if (dl == NULL) {
        // Resource not resolvable yet — old grayscale-tinted draw as a stopgap.
        DrawCustomItemDiamondTint(play, (Gfx*)gGiHoverBootsDL, NULL, -1.0f, 150, 40, 50);
        return;
    }

    // Vanilla GetItem_DrawOpa0 recipe (no scale) + the Y-spin the other custom-item draws use.
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    s16 rotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, dl);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The last three page-2 equipment cells. No dedicated mesh exists for any of them, so they follow the
// same convention as every other ext piece: a VANILLA get-item mesh, tinted. Mirrors the MM side.
// Skijer's NEI
void Randomizer_DrawExtTrident(PlayState* play, GetItemEntry* getItemEntry) {
    // Phantom Ganon's lance, straight out of oot.o2r: limb 9 of gPhantomGanonSkel (the elongated
    // one — 50 tris, 14070 units along Z, prongs spreading in Y). Its two limb textures are
    // referenced by hash INSIDE the display list, so they resolve on their own; no segment setup
    // and nothing copied into soh.o2r (the vanilla-asset rule).
    //
    // Authored in limb-local space: shaft along +Z, tip at Z=+8520, and off-center (its bbox
    // centre is Z=+1485). So: spin around world up, tip the shaft upright (+Z -> +Y), scale, then
    // translate the centre back to the origin so it spins about its middle instead of orbiting.
    // 0.00625 puts the 14070-unit lance at ~88 on screen. That is well over the ~34 the other NEI
    // get-items use, but a lance is a thin silhouette: at the shared size it read as a needle, so
    // Skijer asked for 2.5x. Skijer's NEI
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    // MANDATORY: this limb DL branches to segment 8 twice (`gsSPDisplayList(0x08000001)`) — the
    // per-limb hook the boss uses for its glow. Drawing it without pointing segment 8 somewhere
    // valid makes the interpreter jump into whatever that segment last held and execute it as
    // opcodes: the ASCII-opcode burst + 0xC0000005. DrawPhantomGanon does exactly this for the same
    // reason. An empty DL is the right target here — we only want the geometry.
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)GetEmptyDlist(play->state.gfxCtx));
    s16 rotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
    Matrix_RotateX(-M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Scale(0.00625f, 0.00625f, 0.00625f, MTXMODE_APPLY);
    Matrix_Translate(0.0f, 80.0f, -1485.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gPhantomGanonSkelLimbsLimb_00C610DL_009298);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Generic version of the Pegasus copy-and-remap above: one-time local copy of a GI DL with every
// G_SETPRIMCOLOR/G_SETENVCOLOR pushed through `remap`. Per-SECTION recolors (Climb: yellow leather
// vs silver iron) are only possible this way — a grayscale tint is one color for the whole mesh.
static Gfx* BuildRecoloredGiDL(const char* dlName, uint32_t (*remap)(uint32_t), std::vector<Gfx>& out) {
    if (!out.empty()) {
        return out.data();
    }
    auto isTwoWord = [](uint8_t op) {
        return op == 0x20 || op == 0x24 || op == 0x25 || op == 0x27 || op == 0x31 || op == 0x32 || op == 0x33 ||
               op == 0x35 || op == 0x36 || op == 0x42;
    };
    Gfx* src = (Gfx*)ResourceMgr_LoadGfxByName((char*)dlName);
    if (src == NULL) {
        return NULL;
    }
    size_t count = 0;
    while (count < 4096) {
        uint8_t op = (uint8_t)((src[count].words.w0 >> 24) & 0xFF);
        count++;
        if (op == 0xDF) { // G_ENDDL
            break;
        }
        if (isTwoWord(op)) {
            count++;
        }
    }
    out.assign(src, src + count);
    for (size_t i = 0; i < out.size(); i++) {
        uint8_t op = (uint8_t)((out[i].words.w0 >> 24) & 0xFF);
        if (op == 0xDF) {
            break;
        }
        if (op == G_SETPRIMCOLOR || op == G_SETENVCOLOR) {
            out[i].words.w1 = (uintptr_t)remap((uint32_t)out[i].words.w1);
        } else if (isTwoWord(op)) {
            i++; // payload word, never an opcode
        }
    }
    return out.data();
}

// Climb Boots: YELLOW leather + SILVER iron. The GI mesh tells the sections apart by color
// temperature — every leather prim/env is warm brown (r >> b), every iron one is cool gray —
// so classify per color and ramp by luminance.
static uint32_t ClimbBoots_YellowIronRamp(uint32_t rgba) {
    uint8_t r = (rgba >> 24) & 0xFF, g = (rgba >> 16) & 0xFF, b = (rgba >> 8) & 0xFF, a = rgba & 0xFF;
    float lum = 0.299f * r + 0.587f * g + 0.114f * b;
    float nrF, ngF, nbF;

    if (r > b + 30) { // warm brown = leather → yellow
        nrF = lum * 2.2f;
        ngF = lum * 1.75f;
        nbF = lum * 0.35f;
    } else { // cool gray = iron → bright silver
        nrF = lum * 1.2f + 25.0f;
        ngF = lum * 1.25f + 25.0f;
        nbF = lum * 1.35f + 28.0f;
    }
    uint8_t nr = (uint8_t)(nrF > 255.0f ? 255.0f : nrF);
    uint8_t ng = (uint8_t)(ngF > 255.0f ? 255.0f : ngF);
    uint8_t nb = (uint8_t)(nbF > 255.0f ? 255.0f : nbF);
    return ((uint32_t)nr << 24) | ((uint32_t)ng << 16) | ((uint32_t)nb << 8) | a;
}

// Roc's Boots: the whole hover-boots mesh in ONE metallic gold (mids rich gold, highlights
// toward white-gold).
static uint32_t RocBoots_GoldRamp(uint32_t rgba) {
    uint8_t r = (rgba >> 24) & 0xFF, g = (rgba >> 16) & 0xFF, b = (rgba >> 8) & 0xFF, a = rgba & 0xFF;
    float lum = 0.299f * r + 0.587f * g + 0.114f * b;
    float nrF = lum * 1.6f;
    float ngF = lum * 1.22f;
    float nbF = lum * 0.5f;
    uint8_t nr = (uint8_t)(nrF > 255.0f ? 255.0f : nrF);
    uint8_t ng = (uint8_t)(ngF > 255.0f ? 255.0f : ngF);
    uint8_t nb = (uint8_t)(nbF > 255.0f ? 255.0f : nbF);
    return ((uint32_t)nr << 24) | ((uint32_t)ng << 16) | ((uint32_t)nb << 8) | a;
}

void Randomizer_DrawExtClimbBoots(PlayState* play, GetItemEntry* getItemEntry) {
    // Iron Boots GI at vanilla size and composition (GetItem_DrawOpa0Xlu1: main DL Opa + rivets
    // Xlu), palette-remapped per section: yellow leather + silver iron.
    static std::vector<Gfx> sMain;
    static std::vector<Gfx> sRivets;
    Gfx* mainDL = BuildRecoloredGiDL(gGiIronBootsDL, ClimbBoots_YellowIronRamp, sMain);
    Gfx* rivetsDL = BuildRecoloredGiDL(gGiIronBootsRivetsDL, ClimbBoots_YellowIronRamp, sRivets);
    s16 rotation = play->gameplayFrames * 0x2;

    if (mainDL == NULL || rivetsDL == NULL) {
        return; // resource not resolvable yet — try again next frame
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, mainDL);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, rivetsDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

void Randomizer_DrawExtRocBoots(PlayState* play, GetItemEntry* getItemEntry) {
    // Hover Boots GI at vanilla size, whole palette remapped to one metallic gold. Pegasus keeps
    // the red pair.
    static std::vector<Gfx> sDL;
    Gfx* dl = BuildRecoloredGiDL(gGiHoverBootsDL, RocBoots_GoldRamp, sDL);
    s16 rotation = play->gameplayFrames * 0x2;

    if (dl == NULL) {
        return; // resource not resolvable yet — try again next frame
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, dl);
    CLOSE_DISPS(play->state.gfxCtx);
}

void Randomizer_DrawClawshot(PlayState* play, GetItemEntry* getItemEntry) {
    // MM's own get-item hookshot. Upstream 2Ship's asset XML is the authority:
    //   <File Name="object_gi_hookshot" Segment="6">
    //     <DList Name="gGiHookshotDL" Offset="0xA10"/>            <- the model
    //     <DList Name="gGiHookshotEmptyDL" Offset="0xE48"/>       <- 8 bytes, just an ENDDL
    //     <Texture gGiHookshot1Tex i8 32x32/> <Texture gGiHookshot2Tex i4 32x32/>
    // so the get-item is gGiHookshotDL alone (object_link_child holds the HELD model, a different
    // mesh entirely), and MM's draw table confirms it: gItemDrawTable[GID_HOOKSHOT].
    //
    // The one engine-level obstacle: BOTH archives own objects/object_gi_hookshot/, including the
    // single vertex array object_gi_hookshotVtx_000000. The DL asks for vertices by HASH, which
    // resolves hash -> name -> load from the DEFAULT archive, so they came back OoT's however the
    // DL itself was loaded. MmDL_WithScopedVerts loads both strictly from mm.o2r and rewrites each
    // vertex load to point straight at MM's array (gfx_vtx_hash_handler_custom treats word1 as a
    // real pointer once it exceeds 0xFFFFF, and then never consults the hash).
    //
    // Colour check, so this never needs guessing again: MM's DL sets PRIM 0xC3C300 (yellow), OoT's
    // sets 0x0A3CA0 / 0x3278D2 (blue). Yellow on screen = MM's. Skijer's NEI
    static Gfx* sBody = NULL;
    static u8 sTried = 0;
    if (!sTried && MmAssets_IsAvailable()) {
        sBody = MmDL_WithScopedVerts("objects/object_gi_hookshot/gGiHookshotDL",
                                     "objects/object_gi_hookshot/object_gi_hookshotVtx_000000");
        if (sBody != NULL) {
            sTried = 1;
        }
    }
    if (sBody == NULL) {
        return; // no fallback (Skijer's call): OoT's model here hid the real failure for rounds
    }

    // No extra Matrix_Scale: Player_DrawGetItemImpl already built the get-item matrix (translate +
    // spin + Scale 0.2), which is exactly what GetItem_Draw renders a vanilla GI model with, and
    // MM authors its GI models at the same scale as OoT.
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, sBody);
    CLOSE_DISPS(play->state.gfxCtx);
}

void Randomizer_DrawBottomlessBottle(PlayState* play, GetItemEntry* getItemEntry) {
    // Purple boss-soul flame pouring out of the empty bottle (Ultrashot/True MS language).
    DrawWeaponFlameOverlay(play, 190, 60, 230);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    s16 rotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiBottleStopperDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiBottleDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The four 2026-08-06 page-2 additions. Each draws its OWN placeholder asset from soh.o2r
// (object_nei_<item>/gNei<Item>DL — a flat-colour gem, per the "every custom item gets its own XML"
// rule) so a mod can replace the model without touching code. Path-pointer DLs resolve at draw time,
// exactly like the soh_assets.h symbols do. Skijer's NEI
// Custom-asset draws must NEVER hand an unresolved path straight to gSPDisplayList. That path only
// survives if the resource resolves: a missing entry (soh.o2r not regenerated) or one that resolves
// to a non-DisplayList yields a bogus pointer whose bytes get executed as GBI opcodes — the 0xC0000005
// crash. Resolve first, skip the draw if it didn't, and say so once in the log. Skijer's NEI
static void DrawCustomItemDiamondByPath(PlayState* play, const char* path, Gfx** cache, u8* tried, f32 scale) {
    if (!*tried) {
        *tried = 1;
        if (ResourceMgr_FileExists(path)) {
            *cache = ResourceMgr_LoadGfxByName(path);
        }
        if (*cache == NULL) {
            SPDLOG_ERROR("[NEI] custom get-item model missing: {} — regenerate soh.o2r (GenerateSohOtr)", path);
        }
    }
    if (*cache == NULL) {
        return; // nothing drawn beats crashing on a bad pointer
    }
    DrawCustomItemDiamond(play, *cache, scale);
}

void Randomizer_DrawNeiSheikahSlate(PlayState* play, GetItemEntry* getItemEntry) {
    static Gfx* c = NULL;
    static u8 t = 0;
    DrawCustomItemDiamondByPath(play, "__OTR__objects/object_nei_sheikah_slate/gNeiSheikahSlateDL", &c, &t, 0.35f);
}

// Slate runes: the same slate model wrapped in a per-rune boss-soul flame (the cane-upgrade
// language — the flame color IS the rune's identity, matching its badge/glyph icons).
static void DrawSlateRuneCommon(PlayState* play, u8 r, u8 g, u8 b) {
    static Gfx* c = NULL;
    static u8 t = 0;
    DrawWeaponFlameOverlay(play, r, g, b);
    DrawCustomItemDiamondByPath(play, "__OTR__objects/object_nei_sheikah_slate/gNeiSheikahSlateDL", &c, &t, 0.35f);
}

void Randomizer_DrawSlateRuneBomb(PlayState* play, GetItemEntry* getItemEntry) {
    DrawSlateRuneCommon(play, 95, 220, 235); // Remote Bomb — sheikah cyan
}

void Randomizer_DrawSlateRuneMasterCycle(PlayState* play, GetItemEntry* getItemEntry) {
    DrawSlateRuneCommon(play, 100, 230, 190); // Master Cycle Zero — teal
}

void Randomizer_DrawSlateRuneStasis(PlayState* play, GetItemEntry* getItemEntry) {
    DrawSlateRuneCommon(play, 250, 200, 70); // Stasis — gold
}

void Randomizer_DrawSlateRuneCryonis(PlayState* play, GetItemEntry* getItemEntry) {
    DrawSlateRuneCommon(play, 150, 215, 255); // Cryonis — ice blue
}

void Randomizer_DrawSlateRuneSensor(PlayState* play, GetItemEntry* getItemEntry) {
    DrawSlateRuneCommon(play, 200, 130, 255); // Sheikah Sensor — the old Desire Sensor's violet
}

void Randomizer_DrawNeiPhantomHourglass(PlayState* play, GetItemEntry* getItemEntry) {
    static Gfx* c = NULL;
    static u8 t = 0;
    DrawCustomItemDiamondByPath(play, "__OTR__objects/object_nei_phantom_hourglass/gNeiPhantomHourglassDL", &c, &t,
                                0.35f);
}

void Randomizer_DrawNeiShadowCrystal(PlayState* play, GetItemEntry* getItemEntry) {
    static Gfx* c = NULL;
    static u8 t = 0;
    DrawCustomItemDiamondByPath(play, "__OTR__objects/object_nei_shadow_crystal/gNeiShadowCrystalDL", &c, &t, 0.35f);
}

void Randomizer_DrawNeiRodOfSeasons(PlayState* play, GetItemEntry* getItemEntry) {
    static Gfx* c = NULL;
    static u8 t = 0;
    DrawCustomItemDiamondByPath(play, "__OTR__objects/object_nei_rod_of_seasons/gNeiRodOfSeasonsDL", &c, &t, 0.35f);
}

// Seasons: the same rod model wrapped in its season's flame — the flame colour IS the season's
// identity, matching its wheel glyph.
static void DrawSeasonCommon(PlayState* play, u8 season) {
    static Gfx* c = NULL;
    static u8 t = 0;
    u8 r;
    u8 g;
    u8 b;

    Seasons_SeasonColor(season, &r, &g, &b);
    DrawWeaponFlameOverlay(play, r, g, b);
    DrawCustomItemDiamondByPath(play, "__OTR__objects/object_nei_rod_of_seasons/gNeiRodOfSeasonsDL", &c, &t, 0.35f);
}

void Randomizer_DrawSeasonSpring(PlayState* play, GetItemEntry* getItemEntry) {
    DrawSeasonCommon(play, SEASON_SPRING);
}

void Randomizer_DrawSeasonSummer(PlayState* play, GetItemEntry* getItemEntry) {
    DrawSeasonCommon(play, SEASON_SUMMER);
}

void Randomizer_DrawSeasonAutumn(PlayState* play, GetItemEntry* getItemEntry) {
    DrawSeasonCommon(play, SEASON_AUTUMN);
}

void Randomizer_DrawSeasonWinter(PlayState* play, GetItemEntry* getItemEntry) {
    DrawSeasonCommon(play, SEASON_WINTER);
}

void Randomizer_DrawExtPendantOfMemories(PlayState* play, GetItemEntry* getItemEntry) {
    // MM Pendant of Memories GI model (from mm.o2r) — original DL is small, scale up to fill cylinder
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.9f, 0.9f, 0.9f, MTXMODE_APPLY);
    s16 rotation = play->gameplayFrames * 0x2;
    Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)"__OTR__objects/object_gi_reserve_c_01/gGiPendantOfMemoriesDL");
    CLOSE_DISPS(play->state.gfxCtx);
}

void Randomizer_DrawExtWaterDragonScale(PlayState* play, GetItemEntry* getItemEntry) {
    // Scale model (gGiScaleDL) only, no water effect, with blue tint
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 1 * (play->state.frames * 2),
                                             -1 * (play->state.frames * 2), 64, 64, 1, 1 * (play->state.frames * 4),
                                             -1 * (play->state.frames * 4), 32, 32, 2, -2, 4, -4));

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    gSPGrayscale(POLY_XLU_DISP++, true);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, 40, 120, 220, 255);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiScaleDL);
    gSPGrayscale(POLY_XLU_DISP++, false);

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// MM Mask Get-Item Draw (all 24 masks from mm.o2r)
// =============================================================================

typedef enum {
    MM_MASK_DRAW_OPA0_XLU1 = 0, // DL1=Opa, DL2=Xlu (like GetItem_DrawOpa0Xlu1)
    MM_MASK_DRAW_OPA01 = 1,     // Both Opa (like GetItem_DrawOpa01)
} MmMaskDrawMode;

typedef struct {
    const char* dl1;
    const char* dl2;
    MmMaskDrawMode mode;
} MmMaskDrawEntry;

// Table indexed by (itemId - ITEM_MM_MASK_POSTMAN)
// Order MUST match ITEM_MM_MASK_POSTMAN(0xB7) through ITEM_MM_MASK_FIERCE_DEITY(0xCE)
static MmMaskDrawEntry sMmMaskDrawTable[] = {
    /* POSTMAN       */ { gGiPostmanHatCapDL, gGiPostmanHatBunnyLogoDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* ALL_NIGHT     */ { gGiAllNightMaskEyesDL, gGiAllNightMaskFaceDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* BLAST         */ { gGiBlastMaskEmptyDL, gGiBlastMaskDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* STONE         */ { gGiStoneMaskEmptyDL, gGiStoneMaskDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* GREAT_FAIRY   */ { gGiGreatFairyMaskFaceDL, gGiGreatFairyMaskLeavesDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* DEKU          */ { gGiDekuMaskEmptyDL, gGiDekuMaskDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* KEATON        */ { gGiKeatonMaskDL, gGiKeatonMaskEyesDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* BREMEN        */ { gGiBremenMaskEmptyDL, gGiBremenMaskDL, MM_MASK_DRAW_OPA01 },
    /* BUNNY         */ { gGiBunnyHoodDL, gGiBunnyHoodEyesDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* DON_GERO      */ { gGiDonGeroMaskFaceDL, gGiDonGeroMaskBodyDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* SCENTS        */ { gGiMaskOfScentsFaceDL, gGiMaskOfScentsTeethDL, MM_MASK_DRAW_OPA01 },
    /* GORON         */ { gGiGoronMaskEmptyDL, gGiGoronMaskDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* ROMANI        */ { gGiRomaniMaskCapDL, gGiRomaniMaskNoseEyeDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* CIRCUS_LEADER */ { gGiCircusLeaderMaskEyebrowsDL, gGiCircusLeaderMaskFaceDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* KAFEI         */ { gGiKafeiMaskEmptyDL, gGiKafeiMaskDL, MM_MASK_DRAW_OPA01 },
    /* COUPLE        */ { gGiCouplesMaskFullDL, gGiCouplesMaskHalfDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* TRUTH         */ { gGiMaskOfTruthDL, gGiMaskOfTruthAccentsDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* ZORA          */ { gGiZoraMaskEmptyDL, gGiZoraMaskDL, MM_MASK_DRAW_OPA01 },
    /* KAMARO        */ { gGiKamaroMaskDL, gGiKamaroMaskEmptyDL, MM_MASK_DRAW_OPA01 },
    /* GIBDO         */ { gGiGibdoMaskEmptyDL, gGiGibdoMaskDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* GARO          */ { gGiGarosMaskCloakDL, gGiGarosMaskFaceDL, MM_MASK_DRAW_OPA0_XLU1 },
    /* CAPTAIN       */ { gGiCaptainsHatBodyDL, gGiCaptainsHatFaceDL, MM_MASK_DRAW_OPA01 },
    /* GIANT         */ { gGiGiantMaskEmptyDL, gGiGiantMaskDL, MM_MASK_DRAW_OPA01 },
    /* FIERCE_DEITY  */ { gGiFierceDeityMaskFaceDL, gGiFierceDeityMaskHairAndHatDL, MM_MASK_DRAW_OPA01 },
};

// Raw OTR-path-as-pointer draw, the same mechanism the 24 MM masks have always used: the gfx
// interpreter resolves "__OTR__..." string pointers at execution time against the mounted archives.
// VERIFIED (2026-07-20, listing the user's mm.o2r): every path in the mm_sources headers exists in
// the archive — resolution by exact path works, folder overlap with OoT is NOT a problem (only
// exact duplicate paths go through archive priority). Do NOT gate this with ResourceMgr_FileExists:
// that check does not see the mm.o2r mount and blanks every MM item.
// ...EXCEPT that five of the 24 mask objects have EXACT path twins in OoT — object_gi_golonmask
// (Goron), object_gi_zoramask (Zora), object_gi_ki_tan_mask (Keaton), object_gi_rabit_mask (Bunny
// Hood) and object_gi_truth_mask (Mask of Truth) are all OoT child-trade masks too. For those the
// "exact duplicate path" case above DOES trigger: plain resolution hands back OoT's mask, so the
// get-item shows the wrong model with the wrong textures. Resolving archive-scoped against mm.o2r
// (MmAssets_LoadResource, the same mechanism the Shield of Ikana uses) picks MM's every time, and
// is harmless for the other 19. Cached per path literal — the loader itself does not cache, and
// this runs every frame the item is on screen. Skijer's NEI
static Gfx* MmMaskResolveDL(const char* otrPath) {
    static const char* sKeys[64];
    static Gfx* sVals[64];
    static uint8_t sCount = 0;
    for (uint8_t i = 0; i < sCount; i++) {
        if (sKeys[i] == otrPath) {
            return sVals[i];
        }
    }
    // Strict: five of these mask objects share their path with an OoT child-trade mask, and the
    // whole point here is "MM's mask".
    Gfx* dl = (Gfx*)MmAssets_LoadResourceStrict(otrPath);
    // Logged either way, once per path: the Clawshot round showed that "it looks wrong" cannot
    // distinguish "resolved to the other game's copy" from "did not resolve at all", and the log
    // settles it in one line. Skijer's NEI
    SPDLOG_ERROR("[NEI] MM mask DL '{}' -> {}", otrPath, (void*)dl);
    if (sCount < ARRAY_COUNT(sKeys)) {
        sKeys[sCount] = otrPath;
        sVals[sCount] = dl;
        sCount++;
    }
    return dl;
}

#define GSP_MM_DL(disp, path)               \
    do {                                    \
        Gfx* _mmDL = MmMaskResolveDL(path); \
        if (_mmDL != NULL) {                \
            gSPDisplayList((disp), _mmDL);  \
        }                                   \
    } while (0)

void Randomizer_DrawMmMask(PlayState* play, GetItemEntry* getItemEntry) {
    if (!MmAssets_IsAvailable())
        return;

    u16 index = getItemEntry->itemId - ITEM_MM_MASK_POSTMAN;
    if (index >= ARRAY_COUNT(sMmMaskDrawTable))
        return;

    MmMaskDrawEntry* entry = &sMmMaskDrawTable[index];
    // Which mask actually asks to be drawn. The OoT child-trade masks (Goron/Zora/Keaton/Bunny/
    // Truth) share their object with MM's, so this pins down whether the wrong ROW is selected
    // before going back to look at textures. Skijer's NEI
    {
        static u16 sLast = 0xFFFF;
        if (index != sLast) {
            sLast = index;
            SPDLOG_ERROR("[NEI] DrawMmMask itemId=0x{:X} index={} mode={}", getItemEntry->itemId, index,
                         (int)entry->mode);
        }
    }

    OPEN_DISPS(play->state.gfxCtx);

    // Palette mode OFF before every MM mask.
    //
    // This is the one structural difference between the OoT mask that renders correctly and the MM
    // one that renders black, found by diffing their opcode streams:
    //   OoT: ... SetTimg SetTile LoadBlock SetTile SetTileSize  SetTimg TileSync SetTile LoadTLUT ...
    //   MM : ... SetTimg SetTile LoadBlock SetTile SetTileSize  (no TLUT at all)
    // OoT's mask textures are colour-indexed and load a palette; MM's are INTENSITY (I8 / IA8) and
    // load none. MM's DL never clears the LUT mode either, so whatever the previous draw left set
    // carries over — and an intensity texture sampled as palette-indexed comes out black, which is
    // exactly the symptom: right silhouette, no colour. Skijer's NEI
    if (entry->mode == MM_MASK_DRAW_OPA0_XLU1) {
        // DL1: Opaque, DL2: Translucent (like MM GetItem_DrawOpa0Xlu1)
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_NONE);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        GSP_MM_DL(POLY_OPA_DISP++, entry->dl1);

        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        GSP_MM_DL(POLY_XLU_DISP++, entry->dl2);
    } else {
        // Both DLs: Opaque (like MM GetItem_DrawOpa01)
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_NONE);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        GSP_MM_DL(POLY_OPA_DISP++, entry->dl1);
        GSP_MM_DL(POLY_OPA_DISP++, entry->dl2);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// Chateau Romani Bottle Get-Item Draw (from mm.o2r)
// Uses GetItem_DrawOpa0Xlu1 pattern: DL1=Opa (empty bottle), DL2=Xlu (liquid fill)
// =============================================================================

void Randomizer_DrawChateauRomani(PlayState* play, GetItemEntry* getItemEntry) {
    if (!MmAssets_IsAvailable())
        return;

    OPEN_DISPS(play->state.gfxCtx);

    // DL1: Opaque (empty bottle shape)
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_OPA_DISP++, gGiChateauRomaniBottleEmptyDL);

    // DL2: Translucent (liquid fill + label)
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_XLU_DISP++, gGiChateauRomaniBottleDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// Bottle with Magic Mushroom — Get-Item Draw (Mask of Scents reward)
// Reuses OOT's Odd Mushroom DL (loaded via OTR path) on a vanilla bottle base.
// =============================================================================
void Randomizer_DrawBottleWithMagicMushroom(PlayState* play, GetItemEntry* getItemEntry) {
    // MM's REAL Magic Mushroom GI mesh (mm.o2r object_gi_magicmushroom), archive-scoped like the
    // sword levels. Fallback when mm.o2r is absent: OoT's odd mushroom inside the bottle glass.
    static Gfx* sMmMushroom = NULL;
    static u8 sMmMushroomTried = 0;
    Gfx* mmDL = LoadMmDLOnce("objects/object_gi_magicmushroom/gGiMagicMushroomDL", &sMmMushroom, &sMmMushroomTried);

    // OJO: OPEN_DISPS abre una llave léxica y CLOSE_DISPS la cierra — deben aparecer UNA vez y al
    // mismo nivel (un CLOSE+return dentro de un if descuadra todo el fichero).
    OPEN_DISPS(play->state.gfxCtx);

    // Subtle rotation (matches DrawCustomItemDiamond pattern).
    s16 rotation = play->gameplayFrames * 0x2;

    if (mmDL != NULL) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
        Matrix_Scale(0.8f, 0.8f, 0.8f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, mmDL);
    } else {
        Gfx* mushroomDL = (Gfx*)ResourceMgr_LoadGfxByName("__OTR__objects/object_gi_mushroom/gGiOddMushroomDL");
        if (mushroomDL != NULL && ((const char*)mushroomDL)[0] != '_') {
            Gfx_SetupDL_25Opa(play->state.gfxCtx);
            Matrix_Push();
            Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
            Matrix_Scale(0.55f, 0.55f, 0.55f, MTXMODE_APPLY); // shrunk to sit "inside" the bottle glass
            gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, mushroomDL);
            Matrix_Pop();
        }
        // The bottle around it (glass on XLU so the mushroom shows through).
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        Matrix_RotateY(rotation * 0.01f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiBottleDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// MM Boss Remains — Get-Item Draw (from mm.o2r object_bsmask)
// Single OPA display list per remains, scaled 0.02 (mirrors mm GetItem_DrawRemains).
// The RG is carried in getItemEntry->getItemId (GetGIEntry stores randomizerGet there).
// =============================================================================
void Randomizer_DrawMmRemains(PlayState* play, GetItemEntry* getItemEntry) {
    if (!MmAssets_IsAvailable())
        return;

    const char* dl;
    switch ((RandomizerGet)getItemEntry->getItemId) {
        case RG_MM_REMAINS_ODOLWA:
            dl = gMmRemainsOdolwaDL;
            break;
        case RG_MM_REMAINS_GOHT:
            dl = gMmRemainsGohtDL;
            break;
        case RG_MM_REMAINS_GYORG:
            dl = gMmRemainsGyorgDL;
            break;
        case RG_MM_REMAINS_TWINMOLD:
            dl = gMmRemainsTwinmoldDL;
            break;
        default:
            return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_OPA_DISP++, dl);

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// MM Stray Fairy — Get-Item Draw (REAL model from mm.o2r gameplay_keep)
// -----------------------------------------------------------------------------
// The stray fairy is a Flex SkelAnime (gStrayFairySkel, 9 limbs + gStrayFairy-
// FlyingAnim), mirroring mm 2s2h Rando/DrawItem.cpp DrawStrayFairy.
//
// Loading: 2Ship writes skeletons/animations in EXACTLY SoH's binary resource
// format (OSKL/OSLB/OANM v0, identical factories), so SoH parses them natively.
// The load just has to be ARCHIVE-SCOPED to mm.o2r (MmAssets_LoadSkeleton /
// MmAssets_LoadAnimation) so the global name index can't get in the way; the
// fairy's limb/DL paths are MM-unique, so the limb sub-loads and the "__OTR__"
// dList strings inside the limbs resolve into mm.o2r on their own.
//
// Tint: MM colors the fairy per dungeon with AnimatedMat color entries on
// segment 0x08 (body prim/env) and 0x09 (glow prim/env) — the limb DLs branch
// into those segments, so they MUST be set or the interpreter falls over. We
// build the two tiny prim/env color DLs ourselves using the keyframe-0 colors
// extracted from the real gStrayFairy<Dungeon>TexAnim resources in mm.o2r.
// =============================================================================
s32 Randomizer_OverrideLimbDrawStrayFairy(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                          void* thisx, Gfx** gfx) {
    // Hide the redundant right-facing head limb (mm StrayFairyOverrideLimbDraw does the same).
    if (limbIndex == MM_STRAY_FAIRY_LIMB_RIGHT_FACING_HEAD) {
        *dList = NULL;
    }
    return false;
}

// Keyframe-0 colors from the five gStrayFairy<Dungeon>TexAnim AnimatedMat color
// entries in mm.o2r (body = segment 0x08 prim+env, glow = segment 0x09 prim+env).
typedef struct {
    u8 bodyPrim[3];
    u8 bodyEnv[3];
    u8 glowPrim[4]; // includes alpha (0x5A in every dungeon's TexAnim)
    u8 glowEnv[3];
} MmStrayFairyColors;

void Randomizer_DrawMmStrayFairy(PlayState* play, GetItemEntry* getItemEntry) {
    static SkelAnime sFairySkelAnime;
    static Vec3s sFairyJointTable[MM_STRAY_FAIRY_LIMB_MAX];
    static bool sFairyInitialized = false;
    static u32 sFairyLastUpdate = 0;

    if (!MmAssets_IsAvailable()) {
        return;
    }

    if (!sFairyInitialized) {
        FlexSkeletonHeader* skel = (FlexSkeletonHeader*)MmAssets_LoadSkeleton("objects/gameplay_keep/gStrayFairySkel");
        AnimationHeader* anim = (AnimationHeader*)MmAssets_LoadAnimation("objects/gameplay_keep/gStrayFairyFlyingAnim");
        if (skel == NULL || anim == NULL) {
            return; // mm.o2r not ready yet — retry next frame, never latch the failure
        }
        SkelAnime_InitFlex(play, &sFairySkelAnime, skel, anim, sFairyJointTable, sFairyJointTable,
                           MM_STRAY_FAIRY_LIMB_MAX);
        sFairyInitialized = true;
    }

    MmStrayFairyColors colors;
    switch ((RandomizerGet)getItemEntry->getItemId) {
        case RG_MM_STRAY_FAIRY_WOODFALL: // pink
            colors = { { 255, 235, 255 }, { 170, 40, 100 }, { 255, 140, 220, 90 }, { 255, 140, 220 } };
            break;
        case RG_MM_STRAY_FAIRY_SNOWHEAD: // green
            colors = { { 255, 255, 200 }, { 40, 90, 0 }, { 180, 230, 40, 90 }, { 180, 230, 40 } };
            break;
        case RG_MM_STRAY_FAIRY_GREAT_BAY: // violet-blue
            colors = { { 225, 235, 255 }, { 60, 20, 160 }, { 200, 140, 255, 90 }, { 200, 140, 255 } };
            break;
        case RG_MM_STRAY_FAIRY_STONE_TOWER: // yellow
            colors = { { 255, 255, 225 }, { 160, 160, 60 }, { 200, 200, 140, 90 }, { 255, 255, 140 } };
            break;
        default: // RG_MM_STRAY_FAIRY = Clock Town, orange
            colors = { { 255, 255, 200 }, { 150, 50, 0 }, { 255, 180, 30, 90 }, { 255, 180, 30 } };
            break;
    }

    // Advance the shared flying animation once per game frame (2ship does the same;
    // all fairies drawn in one frame share the pose, which is fine for get-items).
    if (sFairyLastUpdate != play->state.frames) {
        sFairyLastUpdate = play->state.frames;
        SkelAnime_Update(&sFairySkelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    // Stand-in for MM's AnimatedMat_Draw: tiny prim/env color DLs on the two
    // segments the fairy limb DLs branch into.
    Gfx* bodyColorDL = (Gfx*)Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Gfx));
    Gfx* glowColorDL = (Gfx*)Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Gfx));
    Gfx* colorGfx = bodyColorDL;
    gDPSetPrimColor(colorGfx++, 0, 0x80, colors.bodyPrim[0], colors.bodyPrim[1], colors.bodyPrim[2], 255);
    gDPSetEnvColor(colorGfx++, colors.bodyEnv[0], colors.bodyEnv[1], colors.bodyEnv[2], 255);
    gSPEndDisplayList(colorGfx++);
    colorGfx = glowColorDL;
    gDPSetPrimColor(colorGfx++, 0, 0x80, colors.glowPrim[0], colors.glowPrim[1], colors.glowPrim[2],
                    colors.glowPrim[3]);
    gDPSetEnvColor(colorGfx++, colors.glowEnv[0], colors.glowEnv[1], colors.glowEnv[2], 255);
    gSPEndDisplayList(colorGfx++);
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)bodyColorDL);
    gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)glowColorDL);

    Matrix_Push();
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);

    POLY_XLU_DISP =
        SkelAnime_DrawFlex(play, sFairySkelAnime.skeleton, sFairySkelAnime.jointTable, sFairySkelAnime.dListCount,
                           Randomizer_OverrideLimbDrawStrayFairy, NULL, NULL, POLY_XLU_DISP);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// MM Enemy / Boss Souls — Get-Item Draw (REAL enemy/boss models from mm.o2r)
// -----------------------------------------------------------------------------
// 1:1 port of 2Ship's Rando/DrawFuncs.cpp soul draws: every soul renders the
// actual enemy/boss skeleton (or DL composite) with the billboarded MM soul
// flame (gameplay_keep_DL_01ACF0, MM-unique path) behind it, exactly like
// DrawEnLight. Loading strategy (verified by walking mm.o2r/oot.o2r bytes):
//  - Class A (MM-unique skeleton AND limb/DL paths, audited): archive-scoped
//    MmAssets_LoadSkeleton/MmAssets_LoadAnimation — same proven mechanism as
//    Randomizer_DrawMmStrayFairy above. Nested "__OTR__" limb-DL refs resolve
//    globally to mm.o2r because those exact paths exist nowhere else.
//  - Class B (exact skeleton path exists in BOTH archives with different
//    bytes: Beamos, DekuBaba, Guay, IronKnuckle, Octorok, Redead, Shellblade,
//    Skulltula, Stalchild, Tektite, Wallmaster, Wolfos): use OoT's OWN
//    skeleton + a matching OoT animation via "__OTR__" path strings (the
//    SkelAnime_Init*/Animation_PlayLoop SoH patches resolve them). MM reuses
//    OoT's models for these, so the visual is faithful; MM anims are only used
//    when byte-identical in both archives (never MM anim on OoT skel).
//  - Standalone MM DLs/textures: MmAssets_LoadResource pointers (never raw MM
//    path strings — those can fail global resolution and would be executed as
//    a display list -> crash; see the Moon's Tear fix above).
// Segment discipline (the #1 crash source): every skeleton's limb DLs were
// byte-scanned for raw seg-branches (G_DL 0x0N000001) and raw seg texture
// loads; each such segment is fed a valid DL / texture below. Segment 0x0D
// (flex limb matrices) is set internally by SkelAnime_DrawFlexOpa.
// =============================================================================

// --- shared helpers ----------------------------------------------------------

// Per-frame empty display list (Graph_Alloc'd like the fairy color DLs) used
// for segments that MM feeds D_801AEFA0 / EnKnight_BuildEmptyDL.
// Empty "branch catcher" DL for segments that limb DLs jump into (seg 0x08-0x0C).
// STATIC (stable, long-lived address) on purpose: a Graph_Alloc'd empty DL lives in the
// per-frame double-buffered gfx pool, so its address changes every frame and can point
// at recycled memory by the time the deferred command stream (and SoH's frame
// interpolation, which re-runs the buffer) actually executes the branch -> the segment
// resolves to garbage -> crash. A file-static const DL never moves or gets recycled, so
// the branch always lands on a valid ENDDL. Four ENDDLs mirror the vanilla
// renderModeSetNoneDL used by Scene_SetRenderModeXlu.
static Gfx* MmSoul_EmptyDL(GraphicsContext* gfxCtx) {
    static Gfx sEmptyDL[] = {
        gsSPEndDisplayList(),
        gsSPEndDisplayList(),
        gsSPEndDisplayList(),
        gsSPEndDisplayList(),
    };
    (void)gfxCtx;
    return sEmptyDL;
}

// prim+env color DL (stand-in for MM's AnimatedMat color entries / En_Ik's
// func_80A761B0 armor-material DLs).
static Gfx* MmSoul_ColorDL(GraphicsContext* gfxCtx, u8 pr, u8 pg, u8 pb, u8 pa, u8 er, u8 eg, u8 eb) {
    Gfx* dl = (Gfx*)Graph_Alloc(gfxCtx, 3 * sizeof(Gfx));
    Gfx* p = dl;
    gDPSetPrimColor(p++, 0, 0, pr, pg, pb, pa);
    gDPSetEnvColor(p++, er, eg, eb, 255);
    gSPEndDisplayList(p++);
    return dl;
}

// DrawEnLight (2ship DrawFuncs.cpp): billboarded MM soul flame behind the
// model. Continues from the CURRENT matrix (already model-scaled), exactly
// like 2ship — the per-soul flameSize values compensate the model scale.
static void MmSoul_DrawFlame(PlayState* play, Color_RGB8 color, Vec3f size) {
    static s8 sFlameCounter = 0;
    static u32 sFlameLastUpdate = 0;
    Gfx* flameDL = (Gfx*)MmAssets_LoadResource("objects/gameplay_keep/gameplay_keep_DL_01ACF0");
    if (flameDL == NULL) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    // seg 0x08: the only segment branch inside gameplay_keep_DL_01ACF0 (byte-scanned).
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x10, 0x20, 1, (sFlameCounter * 2) & 0x3F,
                                             (sFlameCounter * -6) & 0x7F, 0x10, 0x20, 0, 0, 2, -6));
    gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, color.r, color.g, color.b, 0);
    gDPSetEnvColor(POLY_XLU_DISP++, color.r, color.g, color.b, 0);
    Matrix_Scale(size.x, size.y, size.z, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, flameDL);

    CLOSE_DISPS(play->state.gfxCtx);

    if (sFlameLastUpdate != play->state.frames) {
        sFlameLastUpdate = play->state.frames;
        sFlameCounter++;
    }
}

// Original tinted-flame fallback (OoT blue fire) — used while mm.o2r is not
// ready and for any soul whose model resources fail to load.
static void MmSoul_DrawFallbackFlame(PlayState* play, GetItemEntry* getItemEntry) {
    s16 r, g, b;
    switch ((RandomizerGet)getItemEntry->getItemId) {
        case RG_MM_SOUL_GOHT:
        case RG_MM_SOUL_GYORG:
        case RG_MM_SOUL_MAJORA:
        case RG_MM_SOUL_ODOLWA:
        case RG_MM_SOUL_TWINMOLD:
            r = 255;
            g = 210;
            b = 70;
            break;
        default:
            r = 150;
            g = 200;
            b = 255;
            break;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 8,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 16, 32, 1, play->state.frames,
                                             -(play->state.frames * 8), 16, 32, 0, 0, 1, -8));
    Matrix_Push();
    Matrix_Translate(0.0f, -70.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(5.0f, 5.0f, 5.0f, MTXMODE_APPLY);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, r, g, b, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiBlueFireFlameDL);
    gSPGrayscale(POLY_XLU_DISP++, false);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// --- generic skeleton-soul engine -------------------------------------------

typedef enum {
    MMSOUL_SEG_END = 0,
    MMSOUL_SEG_EMPTY,          // empty DL (branch target)
    MMSOUL_SEG_MM_TEX,         // texture from mm.o2r (MmAssets_LoadResource)
    MMSOUL_SEG_OOT_TEX,        // texture from oot.o2r ("__OTR__" path)
    MMSOUL_SEG_COLOR,          // prim/env color DL (AnimatedMat color stand-in)
    MMSOUL_SEG_SCROLL_EYEGORE, // gEyegoreEyeLaserTexAnim: 16x32 two-tex y-scroll
} MmSoulSegKind;

typedef struct {
    u8 seg;
    u8 kind;
    const char* path;
    u8 prim[4];
    u8 env[3];
} MmSoulSegSpec;

typedef enum {
    MMSOUL_SP_NONE = 0,
    MMSOUL_SP_LEEVER, // reverse the spin animation (2ship: playSpeed = -1)
    MMSOUL_SP_KEESE,  // post-limb: red eyes DL on the head limb
    MMSOUL_SP_MAJORA, // extra tentacle-material DL after the skeleton
} MmSoulSpecial;

typedef struct {
    RandomizerGet rg;
    const char* skelPath; // NULL => fully custom draw (dispatched before the engine)
    const char* animPath;
    u8 fromMm; // 1 = Class A (MmAssets loaders), 0 = Class B (OoT "__OTR__" strings)
    u8 flex;
    f32 preTransY; // world-space translate applied BEFORE the scale (2ship order)
    f32 scale;
    f32 postTransY; // model-space translate applied AFTER the scale (2ship order)
    u8 billboard;
    s16 prim[4]; // prim[0] < 0 => unused
    s16 env[4];  // env[0] < 0 => unused
    MmSoulSegSpec segs[3];
    u8 special;
    Color_RGB8 flameColor;
    Vec3f flameSize;
} MmSoulSpec;

typedef struct {
    SkelAnime skelAnime;
    bool initialized;
    u32 lastUpdate;
} MmSoulState;

#define MMSOUL_GRAY \
    { 155, 155, 155 }
#define MMSOUL_NOCOL \
    { -1, 0, 0, 0 }
#define MMSOUL_NOSEG                               \
    {                                              \
        0, MMSOUL_SEG_END, NULL, { 0, 0, 0, 0 }, { \
            0, 0, 0                                \
        }                                          \
    }
#define MMSOUL_SEG3_NONE \
    { MMSOUL_NOSEG, MMSOUL_NOSEG, MMSOUL_NOSEG }

// clang-format off
static MmSoulSpec sMmSoulSpecs[] = {
    // --- bosses ---
    { RG_MM_SOUL_GOHT, "objects/object_boss_hakugin/gGohtSkel", "objects/object_boss_hakugin/gGohtRunAnim", 1, 1,
      -20.0f, 0.005f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL,
      { { 8, MMSOUL_SEG_MM_TEX, "objects/object_boss_hakugin/gGohtMetalPlateWithCirclePatternTex", { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, { 10, 138, 46 }, { 30.0f, 30.0f, 30.0f } },
    { RG_MM_SOUL_GYORG, "objects/object_boss03/gGyorgSkel", "objects/object_boss03/gGyorgGentleSwimmingAnim", 1, 1,
      -20.0f, 0.05f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, { 19, 99, 165 }, { 3.0f, 3.0f, 3.0f } },
    { RG_MM_SOUL_MAJORA, "objects/object_boss07/gMajorasMaskSkel", "objects/object_boss07/gMajorasMaskFloatingAnim", 1, 0,
      0.0f, 0.05f, 0.0f, 1, MMSOUL_NOCOL, MMSOUL_NOCOL,
      { { 8, MMSOUL_SEG_MM_TEX, "objects/object_boss07/gMajorasMaskWithNormalEyesTex", { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_MAJORA, { 232, 128, 21 }, { 3.0f, 3.0f, 3.0f } },
    { RG_MM_SOUL_ODOLWA, "objects/object_boss01/gOdolwaSkel", "objects/object_boss01/gOdolwaReadyAnim", 1, 1,
      -20.0f, 0.005f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, { 145, 20, 133 }, { 25.0f, 25.0f, 25.0f } },
    { RG_MM_SOUL_TWINMOLD, "objects/object_boss02/gTwinmoldHeadSkel", "objects/object_boss02/gTwinmoldHeadFlyAnim", 1, 0,
      0.0f, 0.06f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL,
      { { 8, MMSOUL_SEG_MM_TEX, "objects/object_boss02/gTwinmoldBlueSkinTex", { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, { 168, 180, 20 }, { 3.0f, 3.0f, 3.0f } },
    // --- enemies (skeleton-based) ---
    { RG_MM_SOUL_ALIEN, "objects/object_uch/gAlienSkel", "objects/object_uch/gAlienFloatAnim", 1, 1,
      0.0f, 0.007f, 0.0f, 0, MMSOUL_NOCOL, { 255, 255, 255, 255 },
      { { 8, MMSOUL_SEG_MM_TEX, "objects/object_uch/gAlienEyeTex", { 0 }, { 0 } },
        { 12, MMSOUL_SEG_EMPTY, NULL, { 0 }, { 0 } }, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, { 10, 138, 46 }, { 30.0f, 30.0f, 30.0f } },
    { RG_MM_SOUL_ARMOS, "objects/object_am/object_am_Skel_005948", "objects/object_am/gArmosHopAnim", 1, 0,
      0.0f, 0.01f, -3100.0f, 0, MMSOUL_NOCOL, { 0, 0, 0, 255 }, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_BEAMOS, "__OTR__objects/object_vm/gBeamosSkel", "__OTR__objects/object_vm/gBeamosAnim", 0, 0,
      0.0f, 0.01f, -3200.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_BUBBLE, "objects/object_bb/gBubbleSkel", "objects/object_bb/gBubbleFlyingAnim", 1, 0,
      0.0f, 0.02f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_CAPTAIN_KEETA, "objects/object_bsb/object_bsb_Skel_00C3E0", "objects/object_bsb/object_bsb_Anim_004894", 1, 0,
      0.0f, 0.01f, -3500.0f, 0, MMSOUL_NOCOL, { 0, 0, 0, 255 },
      { { 12, MMSOUL_SEG_EMPTY, NULL, { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, { 255, 192, 0 }, { 5.0f, 10.0f, 5.0f } },
    { RG_MM_SOUL_DEATH_ARMOS, "objects/object_famos/gFamosSkel", "objects/object_famos/gFamosIdleAnim", 1, 0,
      0.0f, 0.008f, -4100.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL,
      // gFamosNormalGlowingEmblemTexAnim stand-in: seg 8 color DL (glowing red emblem)
      { { 8, MMSOUL_SEG_COLOR, NULL, { 255, 0, 70, 255 }, { 255, 0, 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_DEEP_PYTHON, "objects/object_utubo/gDeepPythonSkel", "objects/object_utubo/gDeepPythonUnusedSideSwayAnim", 1, 1,
      0.0f, 0.02f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_DEKU_BABA, "__OTR__objects/object_dekubaba/gDekuBabaSkel", "__OTR__objects/object_dekubaba/gDekuBabaFastChompAnim", 0, 0,
      0.0f, 0.02f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 6.0f, 6.0f, 6.0f } },
    { RG_MM_SOUL_DINOLFOS, "objects/object_dinofos/gDinolfosSkel", "objects/object_dinofos/gDinolfosIdleAnim", 1, 1,
      0.0f, 0.014f, -2200.0f, 0, MMSOUL_NOCOL, { 20, 40, 40, 255 },
      { { 8, MMSOUL_SEG_MM_TEX, "objects/object_dinofos/gDinolfosEyeOpenTex", { 0 }, { 0 } },
        { 12, MMSOUL_SEG_EMPTY, NULL, { 0 }, { 0 } }, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_DODONGO, "objects/object_dodongo/object_dodongo_Skel_008318", "objects/object_dodongo/object_dodongo_Anim_004C20", 1, 0,
      0.0f, 0.015f, -1500.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_DRAGONFLY, "objects/object_grasshopper/gDragonflySkel", "objects/object_grasshopper/gDragonflyFlyAnim", 1, 0,
      0.0f, 0.01f, -700.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_EENO, "objects/object_snowman/gEenoSkel", "objects/object_snowman/gEenoIdleAnim", 1, 1,
      0.0f, 0.01f, -3000.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, { 155, 155, 35 }, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_EYEGORE, "objects/object_eg/gEyegoreSkel", "objects/object_eg/gEyegoreUnusedWalkAnim", 1, 1,
      0.0f, 0.006f, -4000.0f, 0, { 175, 255, 255, 255 }, { 255, 115, 155, 255 },
      // gEyegoreEyeLaserTexAnim stand-in: seg 9 two-tex scroll (eyeball DL branches it)
      { { 9, MMSOUL_SEG_SCROLL_EYEGORE, NULL, { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, { 192, 192, 64 }, { 20.0f, 20.0f, 20.0f } },
    { RG_MM_SOUL_GARO, "objects/object_jso/gGaroSkel", "objects/object_jso/gGaroIdleAnim", 1, 1,
      0.0f, 0.03f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL,
      { { 12, MMSOUL_SEG_EMPTY, NULL, { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, { 150, 255, 150 }, { 8.0f, 8.0f, 8.0f } },
    { RG_MM_SOUL_GEKKO, "objects/object_bigslime/gGekkoSkel", "objects/object_bigslime/gGekkoBoxingStanceAnim", 1, 1,
      0.0f, 0.006f, -4100.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, { 150, 100, 255 }, { 20.0f, 20.0f, 20.0f } },
    { RG_MM_SOUL_GIANT_BEE, "objects/object_bee/gBeeSkel", "objects/object_bee/gBeeFlyingAnim", 1, 0,
      0.0f, 0.01f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_GOMESS, "objects/object_death/gGomessSkel", "objects/object_death/gGomessFloatAnim", 1, 1,
      0.0f, 0.005f, 0.0f, 0, MMSOUL_NOCOL, { 30, 30, 0, 255 },
      // gGomessBodyMatAnim/gGomessCoreMatAnim stand-in: seg 8 color DL
      { { 8, MMSOUL_SEG_COLOR, NULL, { 235, 255, 125, 255 }, { 30, 70, 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, { 155, 0, 0 }, { 15.0f, 15.0f, 15.0f } },
    { RG_MM_SOUL_GUAY, "__OTR__objects/object_crow/gGuaySkel", "__OTR__objects/object_crow/gGuayFlyAnim", 0, 1,
      0.0f, 0.02f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 6.0f, 6.0f, 6.0f } },
    { RG_MM_SOUL_HIPLOOP, "objects/object_pp/gHiploopSkel", "objects/object_pp/gHiploopChargeAnim", 1, 1,
      0.0f, 0.02f, -1400.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_IGOS_DU_IKANA, "objects/object_knight/gIgosSkel", "objects/object_knight/gKnightIdleAnim", 1, 1,
      0.0f, 0.01f, -2000.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL,
      { { 9, MMSOUL_SEG_EMPTY, NULL, { 0 }, { 0 } }, { 10, MMSOUL_SEG_EMPTY, NULL, { 0 }, { 0 } }, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, { 0, 0, 0 }, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_IRON_KNUCKLE, "__OTR__objects/object_ik/gIronKnuckleSkel", "__OTR__objects/object_ik/gIronKnuckleWalkAnim", 0, 1,
      0.0f, 0.01f, -2900.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL,
      // OoT En_Ik armor-material color DLs (params == 0 palette)
      { { 8, MMSOUL_SEG_COLOR, NULL, { 245, 225, 155, 255 }, { 30, 30, 0 } },
        { 9, MMSOUL_SEG_COLOR, NULL, { 255, 40, 0, 255 }, { 40, 0, 0 } },
        { 10, MMSOUL_SEG_COLOR, NULL, { 255, 255, 255, 255 }, { 20, 40, 30 } } },
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 12.0f, 12.0f, 12.0f } },
    { RG_MM_SOUL_KEESE, "objects/object_firefly/gFireKeeseSkel", "objects/object_firefly/gFireKeeseFlyAnim", 1, 0,
      0.0f, 0.01f, -700.0f, 0, MMSOUL_NOCOL, { 0, 0, 0, 0 }, MMSOUL_SEG3_NONE,
      MMSOUL_SP_KEESE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_LEEVER, "objects/object_rb/gLeeverSkel", "objects/object_rb/gLeeverSpinAnim", 1, 0,
      0.0f, 0.05f, -700.0f, 0, { 255, 255, 255, 255 }, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_LEEVER, MMSOUL_GRAY, { 3.0f, 3.0f, 3.0f } },
    { RG_MM_SOUL_MAD_SCRUB, "objects/object_dekunuts/gDekuScrubSkel", "objects/object_dekunuts/gDekuScrubLookAroundAnim", 1, 0,
      0.0f, 0.01f, -2300.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_NEJIRON, "objects/object_gmo/gNejironSkel", "objects/object_gmo/gNejironIdleAnim", 1, 0,
      0.0f, 0.015f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL,
      { { 8, MMSOUL_SEG_MM_TEX, "objects/object_gmo/gNejironEyeOpenTex", { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 13.0f, 13.0f, 13.0f } },
    { RG_MM_SOUL_OCTOROK, "__OTR__objects/object_okuta/gOctorokSkel", "__OTR__objects/object_okuta/gOctorokFloatAnim", 0, 0,
      0.0f, 0.007f, -700.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_PEAHAT, "objects/object_ph/object_ph_Skel_001C80", "objects/object_ph/object_ph_Anim_0009C4", 1, 0,
      0.0f, 0.01f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_PIRATE, "objects/object_kz/gFighterPirateSkel", "objects/object_kz/gFighterPirateFightingIdleAnim", 1, 1,
      0.0f, 0.01f, -2000.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL,
      { { 8, MMSOUL_SEG_MM_TEX, "objects/object_kz/gFighterPirateEyeOpenTex", { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_POE, "objects/object_po/gPoeSkel", "objects/object_po/gPoeFloatAnim", 1, 0,
      0.0f, 0.0075f, -5000.0f, 0, MMSOUL_NOCOL, { 255, 255, 255, 255 },
      { { 8, MMSOUL_SEG_EMPTY, NULL, { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_REDEAD, "__OTR__objects/object_rd/gRedeadSkel", "__OTR__objects/object_rd/gGibdoRedeadIdleAnim", 0, 1,
      0.0f, 0.01f, -2900.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL,
      { { 8, MMSOUL_SEG_EMPTY, NULL, { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_SHELLBLADE, "__OTR__objects/object_sb/object_sb_Skel_002BF0", "__OTR__objects/object_sb/object_sb_Anim_000194", 0, 1,
      0.0f, 0.007f, -3500.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_SKULLFISH, "objects/object_pr/object_pr_Skel_004188", "objects/object_pr/object_pr_Anim_004340", 1, 1,
      0.0f, 0.02f, 0.0f, 0, { 255, 255, 255, 255 }, { 0, 0, 0, 255 },
      { { 12, MMSOUL_SEG_EMPTY, NULL, { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 5.0f, 5.0f, 5.0f } },
    { RG_MM_SOUL_SKULLTULA, "__OTR__objects/object_st/object_st_Skel_005298", "__OTR__objects/object_st/object_st_Anim_000304", 0, 0,
      0.0f, 0.03f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 5.0f, 5.0f, 5.0f } },
    { RG_MM_SOUL_SNAPPER, "objects/object_tl/gSnapperSkel", "objects/object_tl/gSnapperIdleAnim", 1, 1,
      0.0f, 0.01f, -3100.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL,
      { { 8, MMSOUL_SEG_MM_TEX, "objects/object_tl/gSnapperEyeOpenTex", { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_STALCHILD, "__OTR__objects/object_skb/gStalchildSkel", "__OTR__objects/object_skb/gStalchildWalkingAnim", 0, 0,
      0.0f, 0.01f, -3200.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_TAKKURI, "objects/object_thiefbird/gTakkuriSkel", "objects/object_thiefbird/gTakkuriFlyAnim", 1, 1,
      0.0f, 0.01f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_TEKTITE, "__OTR__objects/object_tite/object_tite_Skel_003A20", "__OTR__objects/object_tite/object_tite_Anim_0012E4", 0, 0,
      0.0f, 0.01f, -2900.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL,
      { { 8, MMSOUL_SEG_OOT_TEX, "__OTR__objects/object_tite/object_tite_Tex_001300", { 0 }, { 0 } },
        { 9, MMSOUL_SEG_OOT_TEX, "__OTR__objects/object_tite/object_tite_Tex_001700", { 0 }, { 0 } },
        { 10, MMSOUL_SEG_OOT_TEX, "__OTR__objects/object_tite/object_tite_Tex_001900", { 0 }, { 0 } } },
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_WALLMASTER, "__OTR__objects/object_wallmaster/gWallmasterSkel", "__OTR__objects/object_wallmaster/gWallmasterWaitAnim", 0, 1,
      0.0f, 0.01f, -3500.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_WART, "objects/object_boss04/gWartSkel", "objects/object_boss04/gWartIdleAnim", 1, 1,
      0.0f, 0.02f, 0.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL, MMSOUL_SEG3_NONE,
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
    { RG_MM_SOUL_WIZROBE, "objects/object_wiz/gWizrobeSkel", "objects/object_wiz/gWizrobeIdleAnim", 1, 1,
      -20.0f, 0.006f, 0.0f, 0, MMSOUL_NOCOL, { 255, 255, 255, 255 },
      { { 8, MMSOUL_SEG_MM_TEX, "objects/object_wiz/gWizrobeEyeTex", { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 15.0f, 15.0f, 15.0f } },
    { RG_MM_SOUL_WOLFOS, "__OTR__objects/object_wf/gWolfosNormalSkel", "__OTR__objects/object_wf/gWolfosWaitingAnim", 0, 1,
      0.0f, 0.01f, -3000.0f, 0, MMSOUL_NOCOL, MMSOUL_NOCOL,
      { { 8, MMSOUL_SEG_OOT_TEX, "__OTR__objects/object_wf/gWolfosNormalEyeOpenTex", { 0 }, { 0 } }, MMSOUL_NOSEG, MMSOUL_NOSEG },
      MMSOUL_SP_NONE, MMSOUL_GRAY, { 10.0f, 10.0f, 10.0f } },
};
// clang-format on

static MmSoulState sMmSoulStates[ARRAY_COUNT(sMmSoulSpecs)];

// 2ship DrawEnFirefly_PostLimbDraw (eyes only; the dust sparkles are omitted).
static void MmSoul_KeesePostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* arg) {
    if (limbIndex == 0x1B) { // FIRE_KEESE_LIMB_HEAD
        Gfx* eyesDL = (Gfx*)MmAssets_LoadResource("objects/object_firefly/gKeeseRedEyesDL");
        if (eyesDL != NULL) {
            OPEN_DISPS(play->state.gfxCtx);
            gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPDisplayList(POLY_XLU_DISP++, eyesDL);
            CLOSE_DISPS(play->state.gfxCtx);
        }
    }
}

// Generic skeleton-soul draw. Returns false when resources aren't ready yet
// (caller falls back to the tinted flame for this frame; retried next frame).
static bool MmSoul_DrawSkeletonSoul(PlayState* play, MmSoulSpec* spec, MmSoulState* st) {
    if (!st->initialized) {
        void* skel;
        void* anim;
        if (spec->fromMm) {
            skel = MmAssets_LoadSkeleton(spec->skelPath);
            anim = MmAssets_LoadAnimation(spec->animPath);
            if (skel == NULL || anim == NULL) {
                return false; // mm.o2r not ready — retry next frame, never latch
            }
        } else {
            skel = (void*)spec->skelPath; // "__OTR__" strings; SkelAnime_Init* resolves them
            anim = (void*)spec->animPath;
        }
        if (spec->flex) {
            SkelAnime_InitFlex(play, &st->skelAnime, (FlexSkeletonHeader*)skel, (AnimationHeader*)anim, NULL, NULL, 0);
        } else {
            SkelAnime_Init(play, &st->skelAnime, (SkeletonHeader*)skel, (AnimationHeader*)anim, NULL, NULL, 0);
        }
        if (st->skelAnime.skeleton == NULL || st->skelAnime.jointTable == NULL) {
            return false;
        }
        if (spec->special == MMSOUL_SP_LEEVER) {
            st->skelAnime.playSpeed = -1.0f; // 2ship: reverse so the spin reads slower
        }
        st->initialized = true;
    }

    // Pre-resolve MM textures so a missing resource falls back cleanly.
    void* mmTex[3] = { NULL, NULL, NULL };
    for (s32 i = 0; i < 3; i++) {
        if (spec->segs[i].kind == MMSOUL_SEG_MM_TEX) {
            mmTex[i] = MmAssets_LoadResource(spec->segs[i].path);
            if (mmTex[i] == NULL) {
                return false;
            }
        }
    }

    if (st->lastUpdate != play->state.frames) {
        st->lastUpdate = play->state.frames;
        SkelAnime_Update(&st->skelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    for (s32 i = 0; i < 3; i++) {
        MmSoulSegSpec* ss = &spec->segs[i];
        switch (ss->kind) {
            case MMSOUL_SEG_EMPTY:
                gSPSegment(POLY_OPA_DISP++, ss->seg, (uintptr_t)MmSoul_EmptyDL(play->state.gfxCtx));
                break;
            case MMSOUL_SEG_MM_TEX:
                gSPSegment(POLY_OPA_DISP++, ss->seg, (uintptr_t)mmTex[i]);
                break;
            case MMSOUL_SEG_OOT_TEX:
                gSPSegment(POLY_OPA_DISP++, ss->seg, (uintptr_t)SEGMENTED_TO_VIRTUAL(ss->path));
                break;
            case MMSOUL_SEG_COLOR:
                gSPSegment(POLY_OPA_DISP++, ss->seg,
                           (uintptr_t)MmSoul_ColorDL(play->state.gfxCtx, ss->prim[0], ss->prim[1], ss->prim[2],
                                                     ss->prim[3], ss->env[0], ss->env[1], ss->env[2]));
                break;
            case MMSOUL_SEG_SCROLL_EYEGORE:
                // gEyegoreEyeLaserTexAnim keyframe motion: 16x32 two-layer scroll, y step -7
                gSPSegment(POLY_OPA_DISP++, ss->seg,
                           (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, (play->state.frames * -7) & 0x7F, 0x10,
                                                       0x20, 1, 0, 0, 0x10, 0x20));
                break;
            default:
                break;
        }
    }

    if (spec->preTransY != 0.0f) {
        Matrix_Translate(0.0f, spec->preTransY, 0.0f, MTXMODE_APPLY);
    }
    if (spec->billboard) {
        Matrix_ReplaceRotation(&play->billboardMtxF);
    }
    Matrix_Scale(spec->scale, spec->scale, spec->scale, MTXMODE_APPLY);
    if (spec->postTransY != 0.0f) {
        Matrix_Translate(0.0f, spec->postTransY, 0.0f, MTXMODE_APPLY);
    }

    if (spec->prim[0] >= 0) {
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, spec->prim[0], spec->prim[1], spec->prim[2], spec->prim[3]);
    }
    if (spec->env[0] >= 0) {
        gDPSetEnvColor(POLY_OPA_DISP++, spec->env[0], spec->env[1], spec->env[2], spec->env[3]);
    }

    PostLimbDrawOpa postLimb = (spec->special == MMSOUL_SP_KEESE) ? MmSoul_KeesePostLimbDraw : NULL;

    if (spec->flex) {
        SkelAnime_DrawFlexOpa(play, st->skelAnime.skeleton, st->skelAnime.jointTable, st->skelAnime.dListCount, NULL,
                              postLimb, NULL);
    } else {
        SkelAnime_DrawOpa(play, st->skelAnime.skeleton, st->skelAnime.jointTable, NULL, postLimb, NULL);
    }

    if (spec->special == MMSOUL_SP_MAJORA) {
        Gfx* tentacleDL = (Gfx*)MmAssets_LoadResource("objects/object_boss07/gMajorasMaskTentacleMaterialDL");
        if (tentacleDL != NULL) {
            gSPDisplayList(POLY_OPA_DISP++, tentacleDL);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);

    MmSoul_DrawFlame(play, spec->flameColor, spec->flameSize);
    return true;
}

// --- fully custom soul draws (DL composites, ports of 2ship DrawFuncs.cpp) ---

// 2ship DrawBat: static body + 9-frame wing flip-book (all MM-unique DLs).
static bool MmSoul_DrawBadBat(PlayState* play) {
    static const char* sWingPaths[] = {
        "objects/object_bat/gBadBatWingsFrame0DL", "objects/object_bat/gBadBatWingsFrame1DL",
        "objects/object_bat/gBadBatWingsFrame2DL", "objects/object_bat/gBadBatWingsFrame3DL",
        "objects/object_bat/gBadBatWingsFrame4DL", "objects/object_bat/gBadBatWingsFrame5DL",
        "objects/object_bat/gBadBatWingsFrame6DL", "objects/object_bat/gBadBatWingsFrame7DL",
        "objects/object_bat/gBadBatWingsFrame8DL",
    };
    static u32 sLastUpdate = 0;
    static u32 sWingAnim = 0;

    Gfx* setupDL = (Gfx*)MmAssets_LoadResource("objects/object_bat/gBadBatSetupDL");
    Gfx* bodyDL = (Gfx*)MmAssets_LoadResource("objects/object_bat/gBadBatBodyDL");
    Gfx* wingDL = (Gfx*)MmAssets_LoadResource(sWingPaths[sWingAnim]);
    if (setupDL == NULL || bodyDL == NULL || wingDL == NULL) {
        return false;
    }

    if (sLastUpdate != play->state.frames) {
        sLastUpdate = play->state.frames;
        sWingAnim = (sWingAnim + 1) % 9;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, setupDL);
    gSPDisplayList(POLY_OPA_DISP++, bodyDL);
    gSPDisplayList(POLY_OPA_DISP++, wingDL);

    CLOSE_DISPS(play->state.gfxCtx);

    MmSoul_DrawFlame(play, { 155, 155, 155 }, { 6.0f, 6.0f, 6.0f });
    return true;
}

// 2ship DrawBoe: shadow blob (OPA) + billboarded body + double eyes (XLU).
static bool MmSoul_DrawBoe(PlayState* play) {
    Gfx* endDL = (Gfx*)MmAssets_LoadResource("objects/object_mkk/gBlackBoeEndDL");
    Gfx* bodyMatDL = (Gfx*)MmAssets_LoadResource("objects/object_mkk/gBlackBoeBodyMaterialDL");
    Gfx* bodyModelDL = (Gfx*)MmAssets_LoadResource("objects/object_mkk/gBlackBoeBodyModelDL");
    Gfx* eyesDL = (Gfx*)MmAssets_LoadResource("objects/object_mkk/gBlackBoeEyesDL");
    if (endDL == NULL || bodyMatDL == NULL || bodyModelDL == NULL || eyesDL == NULL) {
        return false;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0.0f, -1200.0f, 0.0f, MTXMODE_APPLY);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0xFF, 0, 0, 0, 255);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)MmSoul_EmptyDL(play->state.gfxCtx));
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, endDL);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    gSPDisplayList(POLY_XLU_DISP++, bodyMatDL);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, bodyModelDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0xFF, 245, 97, 0, 255);
    gSPDisplayList(POLY_XLU_DISP++, eyesDL);
    Matrix_Scale(0.009f, 0.009f, 0.009f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0xFF, 245, 214, 0, 255);
    gSPDisplayList(POLY_XLU_DISP++, eyesDL);

    CLOSE_DISPS(play->state.gfxCtx);

    // 2ship calls DrawEnLight after the 0.009 rescale — hence the huge size.
    MmSoul_DrawFlame(play, { 155, 155, 155 }, { 1000.0f, 1000.0f, 1000.0f });
    return true;
}

// 2ship DrawChuchu: pulsing jelly body + eyes. Byte-scan: gChuchuBodyDL
// branches seg 0x0A (gChuchuSlimeFlowTexAnim two-tex scroll), gChuchuEyesDL
// loads seg 0x09 (eye texture) and branches seg 0x0C (empty).
static bool MmSoul_DrawChuchu(PlayState* play) {
    static s16 sTimer = 25;
    Gfx* bodyDL = (Gfx*)MmAssets_LoadResource("objects/object_slime/gChuchuBodyDL");
    Gfx* eyesDL = (Gfx*)MmAssets_LoadResource("objects/object_slime/gChuchuEyesDL");
    void* eyeTex = MmAssets_LoadResource("objects/object_slime/gChuchuEyeOpenTex");
    if (bodyDL == NULL || eyesDL == NULL || eyeTex == NULL) {
        return false;
    }

    f32 timerFactor = sqrtf((f32)sTimer) * 0.2f;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Scale(0.01f, ((cosf(sTimer * (2.0f * M_PI / 5.0f)) * (0.07f * timerFactor)) + 1.0f) * 0.01f, 0.01f,
                 MTXMODE_APPLY);
    Matrix_Translate(0.0f, -2700.0f, 0.0f, MTXMODE_APPLY);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    // gChuchuSlimeFlowTexAnim: seg 0x0A two-tex scroll (layer1 static 64x64, layer2 y-scroll 32x32)
    gSPSegment(POLY_XLU_DISP++, 0x0A,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x40, 0x40, 1, 0, play->state.frames & 0x7F,
                                           0x20, 0x20));
    gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)MmSoul_EmptyDL(play->state.gfxCtx));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 100, 255, 255, 200, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 180, 0, 255);

    if (sTimer <= 0) {
        sTimer = 25;
    }

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, bodyDL);
    gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)eyeTex);
    gSPDisplayList(POLY_XLU_DISP++, eyesDL);

    CLOSE_DISPS(play->state.gfxCtx);

    MmSoul_DrawFlame(play, { 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
    sTimer--;
    return true;
}

// 2ship DrawFreezard: single XLU DL with seg 0x08 two-tex scroll + custom combine.
static bool MmSoul_DrawFreezard(PlayState* play) {
    Gfx* freezardDL = (Gfx*)MmAssets_LoadResource("objects/object_fz/object_fz_DL_001130");
    if (freezardDL == NULL) {
        return false;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
    Matrix_Translate(0.0f, -4100.0f, 0.0f, MTXMODE_APPLY);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, play->state.frames % 128, 0x20, 0x20, 1, 0,
                                             (play->state.frames * 2) % 128, 0x20, 0x20, 0, 1, 0, 2));
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gDPSetCombineLERP(POLY_XLU_DISP++, TEXEL1, PRIMITIVE, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIMITIVE, TEXEL0,
                      PRIMITIVE, ENVIRONMENT, COMBINED, ENVIRONMENT, COMBINED, 0, ENVIRONMENT, 0);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 155, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 200, 200, 200, 255);
    gSPDisplayList(POLY_XLU_DISP++, freezardDL);

    CLOSE_DISPS(play->state.gfxCtx);

    MmSoul_DrawFlame(play, { 155, 155, 155 }, { 20.0f, 20.0f, 20.0f });
    return true;
}

// 2ship DrawLikeLike: gLikeLikeDL (byte-identical in both archives) with the
// seg 0x0C body-wave matrices and seg 0x08 texture scroll (mirrors OoT En_Rr).
static bool MmSoul_DrawLikeLike(PlayState* play) {
    static u32 sLastUpdate = 0;
    static s16 sTextureScroll = 0;
    static f32 sSegHeightMod[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    static Vec3s sSegRot[5] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };

    if (sLastUpdate != play->state.frames) {
        sLastUpdate = play->state.frames;
        sTextureScroll++;

        f32 phase = play->state.frames * (2500.0f * (2.0f * M_PI / 65536.0f));
        for (s32 j = 0; j < 5; j++) {
            sSegHeightMod[j] = cosf(phase + (j * 0x4000) * (2.0f * M_PI / 65536.0f)) * 0.15f;
        }
        for (s32 j = 1; j < 5; j++) {
            sSegRot[j].x = (s16)(cosf(phase + (j * 0x3000) * (2.0f * M_PI / 65536.0f)) * 2048.0f);
            sSegRot[j].z = (s16)(sinf(phase + (j * 0x1000) * (2.0f * M_PI / 65536.0f)) * 2048.0f);
        }
    }

    Mtx* segMtx = (Mtx*)Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Mtx));

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0.0f, -3000.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Push();

    gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)segMtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x10, 1, 0,
                                             (sTextureScroll * -6) & 0x7F, 0x20, 0x10, 0, 0, 0, -6));

    Matrix_Push();
    Matrix_Scale((1.0f + sSegHeightMod[0]) * 0.8f, 1.0f, (1.0f + sSegHeightMod[0]) * 0.8f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    Matrix_Pop();

    for (s32 i = 1; i < 5; i++) {
        f32 segScale = 0.8f * (sSegHeightMod[i] + 1.0f);

        Matrix_Translate(0.0f, 1000.0f, 0.0f, MTXMODE_APPLY);
        Matrix_RotateZYX(sSegRot[i].x, sSegRot[i].y, sSegRot[i].z, MTXMODE_APPLY);
        Matrix_Push();
        Matrix_Scale(segScale, 1.0f, segScale, MTXMODE_APPLY);
        Matrix_ToMtx(segMtx, (char*)__FILE__, __LINE__);
        Matrix_Pop();
        segMtx++;
    }

    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)"__OTR__objects/object_rr/gLikeLikeDL");

    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Pop();

    MmSoul_DrawFlame(play, { 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
    return true;
}

// 2ship DrawDexihand: base + one arm segment (static DLs) + flex hand skeleton.
static bool MmSoul_DrawDexihand(PlayState* play) {
    static SkelAnime sSkelAnime;
    static bool sInitialized = false;
    static u32 sLastUpdate = 0;

    Gfx* baseDL = (Gfx*)MmAssets_LoadResource("objects/object_wdhand/gDexihandBaseDL");
    Gfx* armDL = (Gfx*)MmAssets_LoadResource("objects/object_wdhand/gDexihandArmSegmentDL");
    if (baseDL == NULL || armDL == NULL) {
        return false;
    }

    if (!sInitialized) {
        FlexSkeletonHeader* skel = (FlexSkeletonHeader*)MmAssets_LoadSkeleton("objects/object_wdhand/gDexihandSkel");
        AnimationHeader* anim = (AnimationHeader*)MmAssets_LoadAnimation("objects/object_wdhand/gDexihandIdleAnim");
        if (skel == NULL || anim == NULL) {
            return false;
        }
        SkelAnime_InitFlex(play, &sSkelAnime, skel, anim, NULL, NULL, 0);
        if (sSkelAnime.skeleton == NULL || sSkelAnime.jointTable == NULL) {
            return false;
        }
        sInitialized = true;
    }

    if (sLastUpdate != play->state.frames) {
        sLastUpdate = play->state.frames;
        SkelAnime_Update(&sSkelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);
    Matrix_Translate(0.0f, -1000.0f, 0.0f, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, baseDL);

    Matrix_Push();
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, armDL);
    Matrix_Pop();

    Matrix_Translate(0.0f, 2500.0f, 0.0f, MTXMODE_APPLY);
    SkelAnime_DrawFlexOpa(play, sSkelAnime.skeleton, sSkelAnime.jointTable, sSkelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(play->state.gfxCtx);

    Matrix_Translate(0.0f, -1000.0f, 0.0f, MTXMODE_APPLY);
    MmSoul_DrawFlame(play, { 155, 155, 70 }, { 6.0f, 6.0f, 6.0f });
    return true;
}

void Randomizer_DrawMmSoul(PlayState* play, GetItemEntry* getItemEntry) {
    RandomizerGet rg = (RandomizerGet)getItemEntry->getItemId;
    bool drawn = false;

    if (MmAssets_IsAvailable()) {
        // Pre-set every segment an MM limb DL might branch/point into (0x08-0x0C) to a valid
        // static empty DL, on BOTH buffers. MM actor DLs branch into these segments expecting
        // the original actor's draw to have populated them; in this GI-draw context an unset
        // segment resolves to a raw low address and the interpreter executes/reads garbage
        // (crash in VTX/SETCIMG handlers). Per-soul spec segs below override as needed.
        {
            OPEN_DISPS(play->state.gfxCtx);
            Gfx* emptyDL = MmSoul_EmptyDL(play->state.gfxCtx);
            for (u32 seg = 0x08; seg <= 0x0C; seg++) {
                gSPSegment(POLY_OPA_DISP++, seg, (uintptr_t)emptyDL);
                gSPSegment(POLY_XLU_DISP++, seg, (uintptr_t)emptyDL);
            }
            CLOSE_DISPS(play->state.gfxCtx);
        }
        Matrix_Push();
        switch (rg) {
            // DL-composite souls (custom 2ship ports)
            case RG_MM_SOUL_BAD_BAT:
                drawn = MmSoul_DrawBadBat(play);
                break;
            case RG_MM_SOUL_BOE:
                drawn = MmSoul_DrawBoe(play);
                break;
            case RG_MM_SOUL_CHUCHU:
                drawn = MmSoul_DrawChuchu(play);
                break;
            case RG_MM_SOUL_FREEZARD:
                drawn = MmSoul_DrawFreezard(play);
                break;
            case RG_MM_SOUL_LIKE_LIKE:
                drawn = MmSoul_DrawLikeLike(play);
                break;
            case RG_MM_SOUL_DEXIHAND:
                drawn = MmSoul_DrawDexihand(play);
                break;
            default:
                for (size_t i = 0; i < ARRAY_COUNT(sMmSoulSpecs); i++) {
                    if (sMmSoulSpecs[i].rg == rg) {
                        drawn = MmSoul_DrawSkeletonSoul(play, &sMmSoulSpecs[i], &sMmSoulStates[i]);
                        break;
                    }
                }
                break;
        }
        Matrix_Pop();
    }

    if (!drawn) {
        // mm.o2r missing/not ready or this soul's resources failed — tinted flame.
        MmSoul_DrawFallbackFlame(play, getItemEntry);
    }
}

// =============================================================================
// MM Trade / Quest-chain items — Get-Item Draw (from mm.o2r)
// -----------------------------------------------------------------------------
// The non-mask trade/quest items (Moon's Tear, 4 Title Deeds, Room Key, the two
// letters, Pendant of Memories, Pictograph Box, Powder Keg, Bomber's Notebook)
// all draw via one of MM's simple get-item routines. Mirrors mm z_draw.c:
//   OPA01     : both DLs opaque              (GetItem_DrawOpa01 — title deeds)
//   OPA0_XLU1 : DL0 opaque, DL1 xlu          (GetItem_DrawOpa0Xlu1 — most)
//   MOONS_TEAR: DL0 opaque, DL1 billboarded  (GetItem_DrawMoonsTear glow)
// The Moon's Tear / potion-style animated-material tint is omitted (OoT has no
// AnimatedMat_Draw); the baked textures render fine, matching the mask port.
// =============================================================================
typedef enum {
    MM_TRADE_DRAW_OPA0_XLU1 = 0,
    MM_TRADE_DRAW_OPA01 = 1,
    MM_TRADE_DRAW_MOONS_TEAR = 2,
} MmTradeDrawMode;

typedef struct {
    RandomizerGet rg;
    const char* dl1;
    const char* dl2;
    MmTradeDrawMode mode;
} MmTradeDrawEntry;

static MmTradeDrawEntry sMmTradeDrawTable[] = {
    { RG_MM_MOONS_TEAR, gGiMoonsTearItemDL, gGiMoonsTearGlowDL, MM_TRADE_DRAW_MOONS_TEAR },
    { RG_MM_DEED_LAND, gGiTitleDeedEmptyDL, gGiTitleDeedLandColorDL, MM_TRADE_DRAW_OPA01 },
    { RG_MM_DEED_SWAMP, gGiTitleDeedEmptyDL, gGiTitleDeedSwampColorDL, MM_TRADE_DRAW_OPA01 },
    { RG_MM_DEED_MOUNTAIN, gGiTitleDeedEmptyDL, gGiTitleDeedMountainColorDL, MM_TRADE_DRAW_OPA01 },
    { RG_MM_DEED_OCEAN, gGiTitleDeedEmptyDL, gGiTitleDeedOceanColorDL, MM_TRADE_DRAW_OPA01 },
    { RG_MM_ROOM_KEY, gGiRoomKeyEmptyDL, gGiRoomKeyDL, MM_TRADE_DRAW_OPA0_XLU1 },
    { RG_MM_LETTER_TO_KAFEI, gGiLetterToKafeiEnvelopeLetterDL, gGiLetterToKafeiInscriptionsDL,
      MM_TRADE_DRAW_OPA0_XLU1 },
    { RG_MM_LETTER_TO_MAMA, gGiLetterToMamaEnvelopeLetterDL, gGiLetterToMamaInscriptionsDL, MM_TRADE_DRAW_OPA0_XLU1 },
    { RG_MM_PENDANT_OF_MEMORIES, gGiPendantOfMemoriesEmptyDL, gGiPendantOfMemoriesDL, MM_TRADE_DRAW_OPA0_XLU1 },
    { RG_MM_PICTOGRAPH_BOX, gGiPictoBoxFrameDL, gGiPictoBoxBodyAndLensDL, MM_TRADE_DRAW_OPA0_XLU1 },
    { RG_MM_POWDER_KEG, gGiPowderKegBarrelDL, gGiPowderKegGoronSkullAndFuseDL, MM_TRADE_DRAW_OPA0_XLU1 },
    { RG_MM_BOMBERS_NOTEBOOK, gGiBombersNotebookEmptyDL, gGiBombersNotebookDL, MM_TRADE_DRAW_OPA0_XLU1 },
    // Tingle's region maps — all 6 share the one field-map model (mm GID_TINGLE_MAP: both DLs opaque).
    { RG_MM_TINGLE_MAP_CLOCK_TOWN, gGiTingleMapDL, gGiTingleMapEmptyDL, MM_TRADE_DRAW_OPA01 },
    { RG_MM_TINGLE_MAP_WOODFALL, gGiTingleMapDL, gGiTingleMapEmptyDL, MM_TRADE_DRAW_OPA01 },
    { RG_MM_TINGLE_MAP_SNOWHEAD, gGiTingleMapDL, gGiTingleMapEmptyDL, MM_TRADE_DRAW_OPA01 },
    { RG_MM_TINGLE_MAP_ROMANI_RANCH, gGiTingleMapDL, gGiTingleMapEmptyDL, MM_TRADE_DRAW_OPA01 },
    { RG_MM_TINGLE_MAP_GREAT_BAY, gGiTingleMapDL, gGiTingleMapEmptyDL, MM_TRADE_DRAW_OPA01 },
    { RG_MM_TINGLE_MAP_STONE_TOWER, gGiTingleMapDL, gGiTingleMapEmptyDL, MM_TRADE_DRAW_OPA01 },
};

void Randomizer_DrawMmTradeQuest(PlayState* play, GetItemEntry* getItemEntry) {
    if (!MmAssets_IsAvailable())
        return;

    MmTradeDrawEntry* entry = NULL;
    for (size_t i = 0; i < ARRAY_COUNT(sMmTradeDrawTable); i++) {
        if (sMmTradeDrawTable[i].rg == (RandomizerGet)getItemEntry->getItemId) {
            entry = &sMmTradeDrawTable[i];
            break;
        }
    }
    if (entry == NULL)
        return;

    OPEN_DISPS(play->state.gfxCtx);

    // Same guard as Randomizer_DrawMmSoul: pre-set segments 0x08-0x0C on both buffers so any
    // raw segment branch inside an MM GI DL lands on a valid empty DL instead of garbage.
    {
        Gfx* emptyDL = MmSoul_EmptyDL(play->state.gfxCtx);
        for (u32 seg = 0x08; seg <= 0x0C; seg++) {
            gSPSegment(POLY_OPA_DISP++, seg, (uintptr_t)emptyDL);
            gSPSegment(POLY_XLU_DISP++, seg, (uintptr_t)emptyDL);
        }
    }

    // DL0: opaque (shared across all three modes)
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if (entry->mode == MM_TRADE_DRAW_MOONS_TEAR) {
        // CRASH FIX (verified by walking the DL bytes in mm.o2r): gGiMoonsTearItemDL contains a
        // raw gsSPDisplayList branch to SEGMENT 0x09 and gGiMoonsTearGlowDL one to SEGMENT 0x0A —
        // MM's AnimatedMat_Draw(gGiMoonsTearTexAnim) populates those segments with per-frame
        // texture-scroll DLs. With the segments unset, the interpreter jumps into garbage and dies
        // in the vtx handler. Feed both segments a valid 32x32 tex-scroll like MM does.
        gSPSegment(POLY_OPA_DISP++, 0x09,
                   (uintptr_t)Gfx_TexScroll(play->state.gfxCtx, 0, play->state.frames & 0x7F, 32, 32));
    }
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_OPA_DISP++, entry->dl1);

    if (entry->mode == MM_TRADE_DRAW_OPA01) {
        // Second DL also opaque (title deeds).
        GSP_MM_DL(POLY_OPA_DISP++, entry->dl2);
    } else {
        // Second DL translucent.
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        if (entry->mode == MM_TRADE_DRAW_MOONS_TEAR) {
            // Glow branches to segment 0x0A (see crash-fix note above).
            gSPSegment(POLY_XLU_DISP++, 0x0A,
                       (uintptr_t)Gfx_TexScroll(play->state.gfxCtx, 0, (play->state.frames * 2) & 0x7F, 32, 32));
            // Moon's Tear glow is a billboarded sprite (mm GetItem_DrawMoonsTear).
            Matrix_ReplaceRotation(&play->billboardMtxF);
        }
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        GSP_MM_DL(POLY_XLU_DISP++, entry->dl2);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// MM Owl-Statue warp points — Get-Item Draw (MM owl model from mm.o2r)
// -----------------------------------------------------------------------------
// The 2Ship rando draws each owl-statue check with the opened owl-statue model
// (object_sek "sek_open_model"), scaled 0.01 with a -3000 Y translate to bring
// the tall statue down to get-item size (mm Rando/DrawItem.cpp DrawOwlStatue).
// Single opaque DL, shared by all 10 owl statues.
// =============================================================================
void Randomizer_DrawMmOwlStatue(PlayState* play, GetItemEntry* getItemEntry) {
    if (!MmAssets_IsAvailable())
        return;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0.0f, -3000.0f, 0.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_OPA_DISP++, gMmOwlStatueOpenedDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// MM Time Items (clock halves) — Get-Item Draw (from mm.o2r object_obj_tokeidai)
// -----------------------------------------------------------------------------
// Static version of mm/2s2h/Rando/DrawFuncs.cpp DrawClock: the Clock Town clock
// tower's rotating face assembly (minute ring + center/hand + clock face +
// sun/moon panel), frozen at the 2Ship rotations — day variants show the clock
// face at 0xC000 (panel at sun), night variants show the sun/moon panel flipped
// 0x8000 (moon side). No animation: yTranslation / xRotation / minute-ring spin
// / face Z-slide all held at 0, so the tower-opening translate/rotate pairs of
// the 2Ship matrix chain cancel out and are omitted. The RG is carried in
// getItemEntry->getItemId (GetGIEntry stores randomizerGet there).
// object_obj_tokeidai is MM-unique (no OoT folder), so plain GSP_MM_DL is safe.
// =============================================================================
void Randomizer_DrawMmClock(PlayState* play, GetItemEntry* getItemEntry) {
    if (!MmAssets_IsAvailable())
        return;

    f32 clockFaceRotation;    // Z rotation of the clock face
    f32 sunMoonPanelRotation; // Y rotation of the sun/moon panel
    switch ((RandomizerGet)getItemEntry->getItemId) {
        case RG_MM_TIME_DAY_1:
        case RG_MM_TIME_DAY_2:
        case RG_MM_TIME_DAY_3:
            // 2Ship: clockFaceRotation = 0xC000, drawn as RotateZS(-rot * 2) => s16 wrap = 180 deg
            clockFaceRotation = M_PIf;
            sunMoonPanelRotation = 0.0f;
            break;
        case RG_MM_TIME_NIGHT_1:
        case RG_MM_TIME_NIGHT_2:
        case RG_MM_TIME_NIGHT_3:
            // 2Ship: sunMoonPanelRotation = 0x8000 => 180 deg (moon side forward)
            clockFaceRotation = 0.0f;
            sunMoonPanelRotation = M_PIf;
            break;
        default:
            return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    // Get-item scale (2Ship draws the world check at 0.015; 0.01 matches the owl-statue get-item)
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);

    // Minute ring (static: no spin)
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_OPA_DISP++, gMmClockTowerMinuteRingDL);

    // Clock center + hand (static: no face Z-slide)
    GSP_MM_DL(POLY_OPA_DISP++, gMmClockTowerClockCenterAndHandDL);

    // Clock face (day = flipped 180 deg, night = neutral)
    Matrix_RotateZ(clockFaceRotation, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_OPA_DISP++, gMmClockTowerClockFaceDL);

    // Sun/moon panel (night = flipped 180 deg to the moon side)
    Matrix_Translate(0.0f, -1112.0f, -19.6f, MTXMODE_APPLY);
    Matrix_RotateY(sunMoonPanelRotation, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_OPA_DISP++, gMmClockTowerSunAndMoonPanelDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// MM Dungeon Items — Get-Item Draw (from mm.o2r)
// -----------------------------------------------------------------------------
// The per-dungeon Small Keys, Boss Keys, Dungeon Maps and Compasses all reuse
// MM's vanilla get-item dungeon-item models (one model per type; the RG name is
// what distinguishes Woodfall vs. Snowhead vs. Great Bay vs. Stone Tower). One
// shared draw func per type, mirroring mm z_draw.c sDrawItemTable:
//   Small Key   : GetItem_DrawOpa0     (single opaque DL)
//   Dungeon Map : GetItem_DrawOpa0     (single opaque DL)
//   Boss Key    : GetItem_DrawOpa0Xlu1 (DL0 opaque body, DL1 xlu gem)
//   Compass     : GetItem_DrawCompass  (DL0 opaque body, DL1 xlu glass — same shape)
// =============================================================================
void Randomizer_DrawMmSmallKey(PlayState* play, GetItemEntry* getItemEntry) {
    if (!MmAssets_IsAvailable())
        return;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_OPA_DISP++, gMmDungeonSmallKeyDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void Randomizer_DrawMmDungeonMap(PlayState* play, GetItemEntry* getItemEntry) {
    if (!MmAssets_IsAvailable())
        return;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_OPA_DISP++, gMmDungeonMapDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void Randomizer_DrawMmBossKey(PlayState* play, GetItemEntry* getItemEntry) {
    if (!MmAssets_IsAvailable())
        return;

    OPEN_DISPS(play->state.gfxCtx);

    // DL0: opaque key body.
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_OPA_DISP++, gMmDungeonBossKeyDL);

    // DL1: translucent gem.
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_XLU_DISP++, gMmDungeonBossKeyGemDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void Randomizer_DrawMmCompass(PlayState* play, GetItemEntry* getItemEntry) {
    if (!MmAssets_IsAvailable())
        return;

    OPEN_DISPS(play->state.gfxCtx);

    // DL0: opaque compass body.
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_OPA_DISP++, gMmDungeonCompassDL);

    // DL1: translucent glass cover.
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_XLU_DISP++, gMmDungeonCompassGlassDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// MM Swamp / Ocean Gold Skulltula Tokens — Get-Item Draw
// -----------------------------------------------------------------------------
// SHADOW VERDICT: MM's token model lives at objects/object_st/gSkulltulaTokenDL
// (+FlameDL) — the EXACT paths OoT's own object_st uses, with the same symbol
// names. Exact-duplicate paths resolve by archive priority to soh's native
// copy, so this is drawn with OoT's OWN token DLs (the models are the same
// token family; no mm.o2r needed and no MmAssets gate). The MM per-region
// identity comes from 2ship's DrawSkulltulaToken flame tint: ocean = blue
// (prim 0,255,255 / env 0,0,255), swamp = pink-green (prim 0,255,170 /
// env 0,255,0), on the vanilla token flame scroll.
// =============================================================================
void Randomizer_DrawMmGsToken(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);

    // Same eye-visibility tilt 2ship applies (tokens parallel to the camera drop their eyes).
    Matrix_RotateZYX(16, 0, 0, MTXMODE_APPLY);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gSkulltulaTokenDL);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    if ((RandomizerGet)getItemEntry->getItemId == RG_MM_GS_TOKEN_OCEAN) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 0, 255, 255, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 255, 255);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 0, 255, 170, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 255, 0, 255);
    }
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, -(play->state.frames * 5), 32, 32, 1, 0, 0, 32,
                                             64, 0, -5, 0, 0));
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gSkulltulaTokenFlameDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// MM Healed Frogs (Don Gero's choir) — Get-Item Draw
// -----------------------------------------------------------------------------
// SHADOW VERDICT: MM's minifrog is objects/object_fr gFrogSkel — OoT ALSO has
// objects/object_fr with its own frog skeleton (the Zora's River choir frogs,
// the same frog family MM reuses), so per the soul Class-B rule we draw OoT's
// OWN skeleton + animation via "__OTR__" path strings (SoH's SkelAnime patches
// resolve them natively; no mm.o2r involved). Mirrors 2ship's DrawMinifrog:
// env tint per frog (same RGB values OoT's En_Fr uses for its five frogs),
// eye limbs (7/8) hidden in the override and re-drawn billboarded in the post
// (EnFr_OverrideLimbDraw/EnFr_PostLimbDraw replica), eye texture on segments
// 0x08/0x09.
// =============================================================================
#define MM_FROG_LIMB_COUNT 24 // OoT object_fr skeleton (z_en_fr.c SkelAnime_InitFlex count)

static s32 Randomizer_OverrideLimbDrawMmFrog(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                             void* thisx, Gfx** gfx) {
    if ((limbIndex == 7) || (limbIndex == 8)) { // eyes — drawn billboarded in the post-limb pass
        *dList = NULL;
    }
    return false;
}

static void Randomizer_PostLimbDrawMmFrog(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx,
                                          Gfx** gfx) {
    if (((limbIndex == 7) || (limbIndex == 8)) && (*dList != NULL)) {
        Matrix_Push();
        Matrix_ReplaceRotation(&play->billboardMtxF);
        gSPMatrix((*gfx)++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList((*gfx)++, *dList);
        Matrix_Pop();
    }
}

void Randomizer_DrawMmFrog(PlayState* play, GetItemEntry* getItemEntry) {
    static SkelAnime sFrogSkelAnime;
    static Vec3s sFrogJointTable[MM_FROG_LIMB_COUNT];
    static Vec3s sFrogMorphTable[MM_FROG_LIMB_COUNT];
    static bool sFrogInitialized = false;
    static u32 sFrogLastUpdate = 0;

    if (!sFrogInitialized) {
        SkelAnime_InitFlex(play, &sFrogSkelAnime, (FlexSkeletonHeader*)object_fr_Skel_00B498,
                           (AnimationHeader*)object_fr_Anim_001534, sFrogJointTable, sFrogMorphTable,
                           MM_FROG_LIMB_COUNT);
        sFrogInitialized = true;
    }

    // MM frog tints (2ship DrawMinifrog — identical to OoT's own sEnFrColor palette).
    Color_RGB8 envColor;
    switch ((RandomizerGet)getItemEntry->getItemId) {
        case RG_MM_FROG_CYAN:
            envColor = { 0, 170, 200 };
            break;
        case RG_MM_FROG_PINK:
            envColor = { 210, 120, 100 };
            break;
        case RG_MM_FROG_WHITE:
            envColor = { 190, 190, 190 };
            break;
        default: // RG_MM_FROG_BLUE
            envColor = { 120, 130, 230 };
            break;
    }

    if (sFrogLastUpdate != play->state.frames) {
        sFrogLastUpdate = play->state.frames;
        SkelAnime_Update(&sFrogSkelAnime);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);

    gDPSetEnvColor(POLY_OPA_DISP++, envColor.r, envColor.g, envColor.b, 255);
    // Eye texture segments the eye limb DLs sample (open iris, both eyes).
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)object_fr_Tex_0059A0);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)object_fr_Tex_0059A0);

    POLY_OPA_DISP =
        SkelAnime_DrawFlex(play, sFrogSkelAnime.skeleton, sFrogSkelAnime.jointTable, sFrogSkelAnime.dListCount,
                           Randomizer_OverrideLimbDrawMmFrog, Randomizer_PostLimbDrawMmFrog, NULL, POLY_OPA_DISP);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// MM Bottle with Gold Dust — Get-Item Draw (from mm.o2r)
// -----------------------------------------------------------------------------
// MM's vanilla get-item for ITEM_GOLD_DUST: OBJECT_GI_BOTTLE_16 drawn with
// GetItem_DrawOpa0Xlu1 (bottle contents Opa + glass/cork Xlu). MM-unique
// folder, so GSP_MM_DL resolves from mm.o2r (MmAssets gate like its peers).
// =============================================================================
void Randomizer_DrawMmGoldDustBottle(PlayState* play, GetItemEntry* getItemEntry) {
    if (!MmAssets_IsAvailable())
        return;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_OPA_DISP++, gMmGoldDustBottleEmptyDL);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    GSP_MM_DL(POLY_XLU_DISP++, gMmGoldDustBottleGlassAndCorkDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

} // extern "C"
