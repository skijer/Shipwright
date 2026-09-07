/**
 * @file object_mm_rando_items.h
 * @brief MM Boss Remains + Stray Fairy Get-Item asset OTR paths (from mm.o2r)
 *
 * OTR-path-as-pointer #defines for mm.o2r, mirroring object_gi_masks_all.h.
 * Verified against the 2Ship/MM decomp:
 *   - Remains: mm/src/code/z_draw.c GetItem_DrawRemains + object_bsmask.h
 *     (single OPA DL each, scaled 0.02, OBJECT_BSMASK).
 *   - Stray Fairy: mm/2s2h/Rando/DrawItem.cpp DrawStrayFairy — a Flex SkelAnime
 *     living in gameplay_keep (gStrayFairySkel + gStrayFairyFlyingAnim), drawn XLU.
 */

#ifndef MM_SOURCES_OBJECTS_OBJECT_MM_RANDO_ITEMS_H
#define MM_SOURCES_OBJECTS_OBJECT_MM_RANDO_ITEMS_H 1

/* ============================================================================
 * BOSS REMAINS (object_bsmask) — single OPA display lists
 * ============================================================================ */
#define gMmRemainsOdolwaDL "__OTR__objects/object_bsmask/gRemainsOdolwaDL"
#define gMmRemainsGohtDL "__OTR__objects/object_bsmask/gRemainsGohtDL"
#define gMmRemainsGyorgDL "__OTR__objects/object_bsmask/gRemainsGyorgDL"
#define gMmRemainsTwinmoldDL "__OTR__objects/object_bsmask/gRemainsTwinmoldDL"

/* ============================================================================
 * STRAY FAIRY (gameplay_keep) — Flex skeleton + flying animation
 * ============================================================================ */
/* !!! DO NOT USE — see the gameplay_keep warning at the bottom of this file. These resolve to
 * OoT's gameplay_keep (which lacks these symbols) and CRASH. Randomizer_DrawMmStrayFairy now
 * draws OoT's own tinted flame instead. Kept only for reference. */
#define gMmStrayFairySkelPath "__OTR__objects/gameplay_keep/gStrayFairySkel"
#define gMmStrayFairyFlyingAnimPath "__OTR__objects/gameplay_keep/gStrayFairyFlyingAnim"

/* Limb enum values from mm gameplay_keep.h StrayFairyLimb */
#define MM_STRAY_FAIRY_LIMB_RIGHT_FACING_HEAD 1
#define MM_STRAY_FAIRY_LIMB_MAX 10

/* ============================================================================
 * ENEMY / BOSS SOULS (gameplay_keep) — shared "soul flame" orb
 * ----------------------------------------------------------------------------
 * The MM rando (mm/2s2h/Rando/DrawFuncs.cpp DrawEnLight) renders every enemy
 * soul with a billboarded flame aura built from gameplay_keep_DL_01ACF0 (a
 * single XLU DL whose flame texture is embedded; segment 0x08 is only the
 * texcoord-scroll matrix). In MM the flame sits behind the actual enemy's full
 * skeleton — there is NO single "soul" model. For the OoT get-item we use just
 * that shared flame orb as one uncolored-ish model for all 52 souls, tinted by
 * category (boss vs. enemy). See draw.cpp Randomizer_DrawMmSoul.
 * ============================================================================ */
/* !!! DO NOT USE — resolves to OoT's gameplay_keep (no such symbol) and CRASHES. See warning below.
 * Randomizer_DrawMmSoul now draws OoT's own tinted flame (gGiBlueFireFlameDL) instead. */
#define gMmSoulFlameDL "__OTR__objects/gameplay_keep/gameplay_keep_DL_01ACF0"

/* =============================================================================================
 * WARNING — mm.o2r SHADOWING RULE (learned the hard way, 2026-07-19)
 * mm_asset_loader mounts mm.o2r at the LOWEST priority so that paths shared with OoT fall through
 * to OoT instead of assertion-crashing. Consequence for every path in this file:
 *   folder exists ONLY in MM              -> resolves to mm.o2r          [correct]
 *   folder in BOTH + symbol exists in OoT -> resolves to OoT's model     [renders OoT's version]
 *   folder in BOTH + symbol MISSING in OoT-> DOES NOT RESOLVE, and the raw "__OTR__..." string is
 *                                            executed as a display list -> garbage polys + 0xC0000005
 * "objects/gameplay_keep" exists in BOTH, so MM-only gameplay_keep symbols are unusable here.
 * Before adding a path: check whether soh/assets/<folder> exists; if it does, confirm the symbol
 * exists there too, otherwise pick an MM-unique object or use an OoT-native model instead.
 * ============================================================================================= */

/* ============================================================================
 * TRADE / QUEST-CHAIN ITEMS (non-mask) — get-item DLs
 * ----------------------------------------------------------------------------
 * Verified against mm/src/code/z_draw.c sDrawItemTable + the matching
 * object headers in mm/assets/objects/. Each draws with one of the vanilla
 * MM get-item routines:
 *   OPA01     : both DLs opaque       (GetItem_DrawOpa01)
 *   OPA0_XLU1 : DL0 opaque, DL1 xlu   (GetItem_DrawOpa0Xlu1)
 *   MoonsTear : DL0 opaque, DL1 xlu billboarded glow (GetItem_DrawMoonsTear)
 * ============================================================================ */

/* Moon's Tear — object_gi_reserve00 (GetItem_DrawMoonsTear) */
#define gGiMoonsTearItemDL "__OTR__objects/object_gi_reserve00/gGiMoonsTearItemDL"
#define gGiMoonsTearGlowDL "__OTR__objects/object_gi_reserve00/gGiMoonsTearGlowDL"

/* Title Deeds — object_gi_reserve01 (GetItem_DrawOpa01, shared empty base + per-region color) */
#define gGiTitleDeedEmptyDL "__OTR__objects/object_gi_reserve01/gGiTitleDeedEmptyDL"
#define gGiTitleDeedLandColorDL "__OTR__objects/object_gi_reserve01/gGiTitleDeedLandColorDL"
#define gGiTitleDeedSwampColorDL "__OTR__objects/object_gi_reserve01/gGiTitleDeedSwampColorDL"
#define gGiTitleDeedMountainColorDL "__OTR__objects/object_gi_reserve01/gGiTitleDeedMountainColorDL"
#define gGiTitleDeedOceanColorDL "__OTR__objects/object_gi_reserve01/gGiTitleDeedOceanColorDL"

/* Room Key — object_gi_reserve_b_00 (GetItem_DrawOpa0Xlu1) */
#define gGiRoomKeyEmptyDL "__OTR__objects/object_gi_reserve_b_00/gGiRoomKeyEmptyDL"
#define gGiRoomKeyDL "__OTR__objects/object_gi_reserve_b_00/gGiRoomKeyDL"

/* Letter to Mama — object_gi_reserve_b_01 (GetItem_DrawOpa0Xlu1) */
#define gGiLetterToMamaEnvelopeLetterDL "__OTR__objects/object_gi_reserve_b_01/gGiLetterToMamaEnvelopeLetterDL"
#define gGiLetterToMamaInscriptionsDL "__OTR__objects/object_gi_reserve_b_01/gGiLetterToMamaInscriptionsDL"

/* Letter to Kafei — object_gi_reserve_c_00 (GetItem_DrawOpa0Xlu1) */
#define gGiLetterToKafeiEnvelopeLetterDL "__OTR__objects/object_gi_reserve_c_00/gGiLetterToKafeiEnvelopeLetterDL"
#define gGiLetterToKafeiInscriptionsDL "__OTR__objects/object_gi_reserve_c_00/gGiLetterToKafeiInscriptionsDL"

/* Pendant of Memories — object_gi_reserve_c_01 (GetItem_DrawOpa0Xlu1) */
#define gGiPendantOfMemoriesEmptyDL "__OTR__objects/object_gi_reserve_c_01/gGiPendantOfMemoriesEmptyDL"
#define gGiPendantOfMemoriesDL "__OTR__objects/object_gi_reserve_c_01/gGiPendantOfMemoriesDL"

/* Pictograph Box — object_gi_camera (GetItem_DrawOpa0Xlu1) */
#define gGiPictoBoxFrameDL "__OTR__objects/object_gi_camera/gGiPictoBoxFrameDL"
#define gGiPictoBoxBodyAndLensDL "__OTR__objects/object_gi_camera/gGiPictoBoxBodyAndLensDL"

/* Powder Keg — object_gi_bigbomb (GetItem_DrawOpa0Xlu1) */
#define gGiPowderKegBarrelDL "__OTR__objects/object_gi_bigbomb/gGiPowderKegBarrelDL"
#define gGiPowderKegGoronSkullAndFuseDL "__OTR__objects/object_gi_bigbomb/gGiPowderKegGoronSkullAndFuseDL"

/* Bomber's Notebook — object_gi_schedule (GetItem_DrawOpa0Xlu1) */
#define gGiBombersNotebookEmptyDL "__OTR__objects/object_gi_schedule/gGiBombersNotebookEmptyDL"
#define gGiBombersNotebookDL "__OTR__objects/object_gi_schedule/gGiBombersNotebookDL"

/* ============================================================================
 * TINGLE MAPS (object_gi_fieldmap) — dual opaque DLs (GetItem_DrawOpa01)
 * ----------------------------------------------------------------------------
 * All 6 of Tingle's region maps share the same get-item model (GID_TINGLE_MAP).
 * Verified against mm/src/code/z_draw.c sDrawItemTable:
 *   { GetItem_DrawOpa01, { gGiTingleMapDL, gGiTingleMapEmptyDL } }  (both opaque).
 * Drawn via Randomizer_DrawMmTradeQuest in MM_TRADE_DRAW_OPA01 mode.
 * ============================================================================ */
#define gGiTingleMapDL "__OTR__objects/object_gi_fieldmap/gGiTingleMapDL"
#define gGiTingleMapEmptyDL "__OTR__objects/object_gi_fieldmap/gGiTingleMapEmptyDL"

/* ============================================================================
 * OWL STATUE (object_sek) — single opaque DL "sek_open_model"
 * ----------------------------------------------------------------------------
 * The MM owl-statue warp point. In 2Ship's rando (mm/2s2h/Rando/DrawItem.cpp
 * DrawOwlStatue) the opened statue is drawn opaque at scale 0.01 with a
 * -3000 Y translate. See Randomizer_DrawMmOwlStatue in draw.cpp.
 * ============================================================================ */
#define gMmOwlStatueOpenedDL "__OTR__objects/object_sek/gOwlStatueOpenedDL"

/* ============================================================================
 * DUNGEON ITEMS (small key / boss key / dungeon map / compass)
 * ----------------------------------------------------------------------------
 * MM's actual get-item dungeon-item models. Each per-dungeon RG reuses the same
 * model for its type; only the RG name distinguishes the dungeons. Verified
 * against mm/src/code/z_draw.c sDrawItemTable + the object XMLs in
 * mm/assets/xml/{N64_US,GC_US}/objects/:
 *   Small Key   : GetItem_DrawOpa0     { gGiSmallKeyDL }                     (object_gi_key)
 *   Boss Key    : GetItem_DrawOpa0Xlu1 { gGiBossKeyDL, gGiBossKeyGemDL }     (object_gi_bosskey)
 *   Dungeon Map : GetItem_DrawOpa0     { gGiDungeonMapDL }                   (object_gi_map)
 *   Compass     : GetItem_DrawCompass  { gGiCompassDL, gGiCompassGlassDL }   (object_gi_compass)
 * (GetItem_DrawCompass is structurally identical to GetItem_DrawOpa0Xlu1.)
 * ============================================================================ */
/* ============================================================================
 * BOTTLE WITH GOLD DUST (object_gi_bottle_16) — MM's vanilla "bottle with stuff"
 * get-item model (GetItem_DrawOpa0Xlu1). Vanilla MM uses this exact pair for
 * ITEM_GOLD_DUST (z_player GET_ITEM: OBJECT_GI_BOTTLE_16 + GID_SEAHORSE); the
 * decomp DL names say "Seahorse" because the seahorse bottle shares the model.
 * MM-unique folder — no OoT shadowing.
 * ============================================================================ */
#define gMmGoldDustBottleEmptyDL "__OTR__objects/object_gi_bottle_16/gGiSeahorseBottleEmptyDL"
#define gMmGoldDustBottleGlassAndCorkDL "__OTR__objects/object_gi_bottle_16/gGiSeahorseBottleGlassAndCorkDL"

/* ============================================================================
 * CLOCK TOWER FACE (object_obj_tokeidai) — MM time-item get-item model
 * ----------------------------------------------------------------------------
 * The rotating clock-face assembly of the Clock Town clock tower, mirroring
 * mm/2s2h/Rando/DrawFuncs.cpp DrawClock (day: face at 0xC000, night: sun/moon
 * panel at 0x8000), drawn STATIC by Randomizer_DrawMmClock for the 6
 * RG_MM_TIME_* items. MM-unique folder (no objects/object_obj_tokeidai in
 * soh/assets) — no OoT shadowing, plain GSP_MM_DL path resolution is safe.
 * ============================================================================ */
#define gMmClockTowerMinuteRingDL "__OTR__objects/object_obj_tokeidai/gClockTowerMinuteRingDL"
#define gMmClockTowerClockCenterAndHandDL "__OTR__objects/object_obj_tokeidai/gClockTowerClockCenterAndHandDL"
#define gMmClockTowerClockFaceDL "__OTR__objects/object_obj_tokeidai/gClockTowerClockFaceDL"
#define gMmClockTowerSunAndMoonPanelDL "__OTR__objects/object_obj_tokeidai/gClockTowerSunAndMoonPanelDL"

#define gMmDungeonSmallKeyDL "__OTR__objects/object_gi_key/gGiSmallKeyDL"
#define gMmDungeonBossKeyDL "__OTR__objects/object_gi_bosskey/gGiBossKeyDL"
#define gMmDungeonBossKeyGemDL "__OTR__objects/object_gi_bosskey/gGiBossKeyGemDL"
#define gMmDungeonMapDL "__OTR__objects/object_gi_map/gGiDungeonMapDL"
#define gMmDungeonCompassDL "__OTR__objects/object_gi_compass/gGiCompassDL"
#define gMmDungeonCompassGlassDL "__OTR__objects/object_gi_compass/gGiCompassGlassDL"

#endif // MM_SOURCES_OBJECTS_OBJECT_MM_RANDO_ITEMS_H
