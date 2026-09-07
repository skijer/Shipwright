#include "global.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/gameplay_field_keep/gameplay_field_keep.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
#include "overlays/actors/ovl_Demo_Effect/z_demo_effect.h"

#include <libultraship/bridge/resourcebridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/draw.h"
#include "soh/ResourceManagerHelpers.h"
#include "mods/items/custom_items.h"
#include "mods/items/custom_bottles.h" // Net catch-at-blade (Skijer's NEI)
#include "mods/extended_player.h"
#include "mods/extended_equipment.h"
#include "mods/items/logic/item_mitts.h"
#include "mods/items/logic/weapon_upgrades.h"
#include "mods/transformation_masks/transformation_masks.h"
#include "mods/transformation_masks/gerudo_form.h"
#include "mods/pak_loader/pak_loader.h"
#include "mods/o2r_loader/o2r_loader.h"
#include "mods/transformation_masks/custom_forms.h"
#include "mods/transformation_masks/kafei_form.h"

// Boss Remains (mods/boss_remains) — worn-remains state + limb-space draw hooks. Implemented
// extern "C" in the boss_remains module; declared locally (no header include). Mirrors MM 2ship.
extern s32 BossRemains_IsOdolwaWorn(void);
extern s32 BossRemains_IsGohtWorn(void);
extern void BossRemains_DrawWornMask(PlayState* play, Player* player);
extern void BossRemains_DrawOdolwaSword(PlayState* play, Player* player);
extern void BossRemains_DrawOdolwaShield(PlayState* play, Player* player);

// Kite Shield shield-surfing lower-body pose. Declared locally for the same reason as the
// BossRemains hooks above: it takes a Vec3s*, and extended_equipment.h is reached through
// z64item.h by translation units that have not seen z64math.h. Skijer's NEI
extern void KiteSurf_AdjustLimb(s32 limbIndex, Vec3s* rot);

// The Sheikah Slate is pinned to the right fist and used to rebuild its pose from two bodyPartsPos
// points, which give a direction and so cannot express the wrist twisting around it. Skijer's NEI
extern void ItemEquip_CaptureHandMatrix(void);
extern u8 ItemEquip_HoldsClosedFist(void);
extern u8 ItemEquip_HoldsEmptyHand(void);

#include <stdlib.h>

// SW97: Forward declaration - defined in sw97_player_hooks.c (compiled in z_player.c TU)

typedef struct {
    /* 0x00 */ u8 flag;
    /* 0x02 */ u16 textId;
} TextTriggerEntry; // size = 0x04

typedef struct {
    /* 0x00 */ void* dList;
    /* 0x04 */ Vec3f pos;
} BowStringData; // size = 0x10

FlexSkeletonHeader* gPlayerSkelHeaders[] = { &gLinkAdultSkel, &gLinkChildSkel };

s16 sBootData[PLAYER_BOOTS_MAX][17] = {
    { 200, 1000, 300, 700, 550, 270, 600, 350, 800, 600, -100, 600, 590, 750, 125, 200, 130 },
    { 200, 1000, 300, 700, 550, 270, 1000, 0, 800, 300, -160, 600, 590, 750, 125, 200, 130 },
    { 200, 1000, 300, 700, 550, 270, 600, 600, 800, 550, -100, 600, 540, 270, 25, 0, 130 },
    { 200, 1000, 300, 700, 380, 400, 0, 300, 800, 500, -100, 600, 590, 750, 125, 200, 130 },
    { 80, 800, 150, 700, 480, 270, 600, 50, 800, 550, -40, 400, 540, 270, 25, 0, 80 },
    { 200, 1000, 300, 800, 500, 400, 800, 400, 800, 550, -100, 600, 540, 750, 125, 400, 200 },
};

// Used to map action params to model groups
u8 sActionModelGroups[] = {
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_NONE
    PLAYER_MODELGROUP_SWORD,            // PLAYER_IA_SWORD_CS
    PLAYER_MODELGROUP_10,               // PLAYER_IA_FISHING_POLE
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_IA_SWORD_MASTER
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_IA_SWORD_KOKIRI
    PLAYER_MODELGROUP_BGS,              // PLAYER_IA_SWORD_BIGGORON
    PLAYER_MODELGROUP_10,               // PLAYER_IA_DEKU_STICK
    PLAYER_MODELGROUP_HAMMER,           // PLAYER_IA_HAMMER
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_FIRE
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_ICE
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_LIGHT
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_0C
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_0D
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_0E
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_SLINGSHOT
    PLAYER_MODELGROUP_HOOKSHOT,         // PLAYER_IA_HOOKSHOT
    PLAYER_MODELGROUP_HOOKSHOT,         // PLAYER_IA_LONGSHOT
    PLAYER_MODELGROUP_EXPLOSIVES,       // PLAYER_IA_BOMB
    PLAYER_MODELGROUP_EXPLOSIVES,       // PLAYER_IA_BOMBCHU
    PLAYER_MODELGROUP_BOOMERANG,        // PLAYER_IA_BOOMERANG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MAGIC_SPELL_15
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MAGIC_SPELL_16
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_MAGIC_SPELL_17
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_FARORES_WIND
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_NAYRUS_LOVE
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_DINS_FIRE
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_DEKU_NUT
    PLAYER_MODELGROUP_OCARINA,          // PLAYER_IA_OCARINA_FAIRY
    PLAYER_MODELGROUP_OOT,              // PLAYER_IA_OCARINA_OF_TIME
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_FISH
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_FIRE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_BUG
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_POE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_BIG_POE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_RUTOS_LETTER
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_POTION_RED
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_POTION_BLUE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_POTION_GREEN
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_MILK_FULL
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_MILK_HALF
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_FAIRY
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_ZELDAS_LETTER
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_WEIRD_EGG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_CHICKEN
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MAGIC_BEAN
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_POCKET_EGG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_POCKET_CUCCO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_COJIRO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_ODD_MUSHROOM
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_ODD_POTION
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_POACHERS_SAW
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_BROKEN_GORONS_SWORD
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_PRESCRIPTION
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_FROG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_EYEDROPS
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_CLAIM_CHECK
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_KEATON
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_SKULL
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_SPOOKY
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_BUNNY_HOOD
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_GORON
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_ZORA
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_GERUDO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_TRUTH
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_LENS_OF_TRUTH (0x42 = 66)
    // Custom items (0x43+) are handled by ExtPlayer_GetActionModelGroup() in extended_player.c
};

TextTriggerEntry sTextTriggers[] = {
    { 1, 0x3040 },
    { 2, 0x401D },
    { 0, 0x0000 },
    { 2, 0x401D },
};

// Used to map model groups to model types for [animation, left hand, right hand, sheath, waist]
u8 gPlayerModelTypes[PLAYER_MODELGROUP_MAX][PLAYER_MODELGROUPENTRY_MAX] = {
    /* PLAYER_MODELGROUP_0 */
    { PLAYER_ANIMTYPE_2, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_SHIELD, PLAYER_MODELTYPE_SHEATH_16,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD */
    { PLAYER_ANIMTYPE_1, PLAYER_MODELTYPE_LH_SWORD, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_19,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_SWORD_AND_SHIELD */
    { PLAYER_ANIMTYPE_1, PLAYER_MODELTYPE_LH_SWORD, PLAYER_MODELTYPE_RH_SHIELD, PLAYER_MODELTYPE_SHEATH_17,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_DEFAULT */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_4 */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BGS */
    { PLAYER_ANIMTYPE_3, PLAYER_MODELTYPE_LH_BGS, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_19,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BOW_SLINGSHOT */
    { PLAYER_ANIMTYPE_4, PLAYER_MODELTYPE_LH_CLOSED, PLAYER_MODELTYPE_RH_BOW_SLINGSHOT, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_EXPLOSIVES */
    { PLAYER_ANIMTYPE_5, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BOOMERANG */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_BOOMERANG, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_HOOKSHOT */
    { PLAYER_ANIMTYPE_4, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_HOOKSHOT, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_10 */
    { PLAYER_ANIMTYPE_3, PLAYER_MODELTYPE_LH_CLOSED, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_HAMMER */
    { PLAYER_ANIMTYPE_3, PLAYER_MODELTYPE_LH_HAMMER, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_OCARINA */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OCARINA, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_OOT */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OOT, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BOTTLE */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_BOTTLE, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_SWORD */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_SWORD, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_19,
      PLAYER_MODELTYPE_WAIST },
};

Gfx* sPlayerRightHandShieldDLs[PLAYER_SHIELD_MAX * 4] = {
    // PLAYER_SHIELD_NONE
    gLinkAdultRightHandClosedNearDL,
    gLinkChildRightHandClosedNearDL,
    gLinkAdultRightHandClosedFarDL,
    gLinkChildRightHandClosedFarDL,
    // PLAYER_SHIELD_DEKU
    gLinkAdultRightHandClosedNearDL,
    gLinkChildRightFistAndDekuShieldNearDL,
    gLinkAdultRightHandClosedFarDL,
    gLinkChildRightFistAndDekuShieldFarDL,
    // PLAYER_SHIELD_HYLIAN
    gLinkAdultRightHandHoldingHylianShieldNearDL,
    gLinkChildRightHandClosedNearDL,
    gLinkAdultRightHandHoldingHylianShieldFarDL,
    gLinkChildRightHandClosedFarDL,
    // PLAYER_SHIELD_MIRROR
    gLinkAdultRightHandHoldingMirrorShieldNearDL,
    gLinkChildRightHandClosedNearDL,
    gLinkAdultRightHandHoldingMirrorShieldFarDL,
    gLinkChildRightHandClosedFarDL,
};

Gfx* sSheathWithSwordDLs[(PLAYER_SHIELD_MAX + 2) * 4] = {
    // PLAYER_SHIELD_NONE
    gLinkAdultMasterSwordAndSheathNearDL,
    gLinkChildSwordAndSheathNearDL,
    gLinkAdultMasterSwordAndSheathFarDL,
    gLinkChildSwordAndSheathFarDL,
    // PLAYER_SHIELD_DEKU
    gLinkAdultMasterSwordAndSheathNearDL,
    gLinkChildDekuShieldSwordAndSheathNearDL,
    gLinkAdultMasterSwordAndSheathFarDL,
    gLinkChildDekuShieldSwordAndSheathFarDL,
    // PLAYER_SHIELD_HYLIAN
    gLinkAdultHylianShieldSwordAndSheathNearDL,
    gLinkChildHylianShieldSwordAndSheathNearDL,
    gLinkAdultHylianShieldSwordAndSheathFarDL,
    gLinkChildHylianShieldSwordAndSheathFarDL,
    // PLAYER_SHIELD_MIRROR
    gLinkAdultMirrorShieldSwordAndSheathNearDL,
    gLinkChildSwordAndSheathNearDL,
    gLinkAdultMirrorShieldSwordAndSheathFarDL,
    gLinkChildSwordAndSheathFarDL,
    // PLAYER_SHIELD_NONE (child, no sword)
    NULL,
    NULL,
    NULL,
    NULL,
    // PLAYER_SHIELD_DEKU (child, no sword)
    NULL,
    gLinkChildDekuShieldWithMatrixDL,
    NULL,
    gLinkChildDekuShieldWithMatrixDL,
};

Gfx* sSheathWithoutSwordDLs[(PLAYER_SHIELD_MAX + 2) * 4] = {
    // PLAYER_SHIELD_NONE
    gLinkAdultSheathNearDL,
    gLinkChildSheathNearDL,
    gLinkAdultSheathFarDL,
    gLinkChildSheathFarDL,
    // PLAYER_SHIELD_DEKU
    gLinkAdultSheathNearDL,
    gLinkChildDekuShieldAndSheathNearDL,
    gLinkAdultSheathFarDL,
    gLinkChildDekuShieldAndSheathFarDL,
    // PLAYER_SHIELD_HYLIAN
    gLinkAdultHylianShieldAndSheathNearDL,
    gLinkChildHylianShieldAndSheathNearDL,
    gLinkAdultHylianShieldAndSheathFarDL,
    gLinkChildHylianShieldAndSheathFarDL,
    // PLAYER_SHIELD_MIRROR
    gLinkAdultMirrorShieldAndSheathNearDL,
    gLinkChildSheathNearDL,
    gLinkAdultMirrorShieldAndSheathFarDL,
    gLinkChildSheathFarDL,
    // PLAYER_SHIELD_NONE (child, no sword)
    NULL,
    NULL,
    NULL,
    NULL,
    // PLAYER_SHIELD_DEKU (child, no sword)
    gLinkAdultSheathNearDL,
    gLinkChildDekuShieldWithMatrixDL,
    gLinkAdultSheathNearDL,
    gLinkChildDekuShieldWithMatrixDL,
};

Gfx* gPlayerLeftHandBgsDLs[] = {
    // Biggoron Sword
    gLinkAdultLeftHandHoldingBgsNearDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    gLinkAdultLeftHandHoldingBgsFarDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    // Broken Giant's Knife
    gLinkAdultHandHoldingBrokenGiantsKnifeDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    gLinkAdultHandHoldingBrokenGiantsKnifeFarDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
};

Gfx* gPlayerLeftHandOpenDLs[] = {
    gLinkAdultLeftHandNearDL,
    gLinkChildLeftHandNearDL,
    gLinkAdultLeftHandFarDL,
    gLinkChildLeftHandFarDL,
};

Gfx* gPlayerLeftHandClosedDLs[] = {
    gLinkAdultLeftHandClosedNearDL,
    gLinkChildLeftFistNearDL,
    gLinkAdultLeftHandClosedFarDL,
    gLinkChildLeftFistFarDL,
};

Gfx* sPlayerLeftHandSwordDLs2[] = {
    gLinkAdultLeftHandHoldingMasterSwordNearDL,
    gLinkChildLeftFistAndKokiriSwordNearDL,
    gLinkAdultLeftHandHoldingMasterSwordFarDL,
    gLinkChildLeftFistAndKokiriSwordFarDL,
};

Gfx* sPlayerLeftHandSwordDLs[] = {
    gLinkAdultLeftHandHoldingMasterSwordNearDL,
    gLinkChildLeftFistAndKokiriSwordNearDL,
    gLinkAdultLeftHandHoldingMasterSwordFarDL,
    gLinkChildLeftFistAndKokiriSwordFarDL,
};

Gfx* sPlayerRightHandOpenDLs[] = {
    gLinkAdultRightHandNearDL,
    gLinkChildRightHandNearDL,
    gLinkAdultRightHandFarDL,
    gLinkChildRightHandFarDL,
};

Gfx* sPlayerRightHandClosedDLs[] = {
    gLinkAdultRightHandClosedNearDL,
    gLinkChildRightHandClosedNearDL,
    gLinkAdultRightHandClosedFarDL,
    gLinkChildRightHandClosedFarDL,
};

Gfx* sPlayerRightHandBowSlingshotDLs[] = {
    gLinkAdultRightHandHoldingBowNearDL,
    gLinkChildRightHandHoldingSlingshotNearDL,
    gLinkAdultRightHandHoldingBowFarDL,
    gLinkChildRightHandHoldingSlingshotFarDL,
};

Gfx* sSwordAndSheathDLs[] = {
    gLinkAdultMasterSwordAndSheathNearDL,
    gLinkChildSwordAndSheathNearDL,
    gLinkAdultMasterSwordAndSheathFarDL,
    gLinkChildSwordAndSheathFarDL,
};

Gfx* sSheathDLs[] = {
    gLinkAdultSheathNearDL,
    gLinkChildSheathNearDL,
    gLinkAdultSheathFarDL,
    gLinkChildSheathFarDL,
};

Gfx* sPlayerWaistDLs[] = {
    gLinkAdultWaistNearDL,
    gLinkChildWaistNearDL,
    gLinkAdultWaistFarDL,
    gLinkChildWaistFarDL,
};

Gfx* sPlayerRightHandBowSlingshotDLs2[] = {
    gLinkAdultRightHandHoldingBowNearDL,
    gLinkChildRightHandHoldingSlingshotNearDL,
    gLinkAdultRightHandHoldingBowFarDL,
    gLinkChildRightHandHoldingSlingshotFarDL,
};

Gfx* sPlayerRightHandOcarinaDLs[] = {
    gLinkAdultRightHandHoldingOotNearDL,
    gLinkChildRightHandHoldingFairyOcarinaNearDL,
    gLinkAdultRightHandHoldingOotFarDL,
    gLinkChildRightHandHoldingFairyOcarinaFarDL,
};

Gfx* sPlayerRightHandOotDLs[] = {
    gLinkAdultRightHandHoldingOotNearDL,
    gLinkChildRightHandAndOotNearDL,
    gLinkAdultRightHandHoldingOotFarDL,
    gLinkChildRightHandHoldingOOTFarDL,
};

Gfx* sPlayerRightHandHookshotDLs[] = {
    gLinkAdultRightHandHoldingHookshotNearDL,
    gLinkChildRightHandNearDL,
    gLinkAdultRightHandHoldingHookshotNearDL, // The 'far' display list exists but is not used
    gLinkChildRightHandFarDL,
};

Gfx* sPlayerLeftHandHammerDLs[] = {
    gLinkAdultLeftHandHoldingHammerNearDL,
    gLinkChildLeftHandNearDL,
    gLinkAdultLeftHandHoldingHammerFarDL,
    gLinkChildLeftHandFarDL,
};

Gfx* gPlayerLeftHandBoomerangDLs[] = {
    gLinkAdultLeftHandNearDL,
    gLinkChildLeftFistAndBoomerangNearDL,
    gLinkAdultLeftHandFarDL,
    gLinkChildLeftFistAndBoomerangFarDL,
};

Gfx* sPlayerLeftHandBottleDLs[] = {
    gLinkAdultLeftHandOutNearDL,
    gLinkChildLeftHandUpNearDL,
    gLinkAdultLeftHandOutNearDL,
    gLinkChildLeftHandUpNearDL,
};

Gfx* sFirstPersonLeftForearmDLs[] = {
    gLinkAdultRightArmOutNearDL,
    NULL,
};

Gfx* sFirstPersonLeftHandDLs[] = {
    gLinkAdultRightHandOutNearDL,
    NULL,
};

Gfx* sFirstPersonRightShoulderDLs[] = {
    gLinkAdultRightShoulderNearDL,
    gLinkChildRightShoulderNearDL,
};

Gfx* sFirstPersonForearmDLs[] = {
    gLinkAdultLeftArmOutNearDL,
    NULL,
};

Gfx* sFirstPersonRightHandHoldingWeaponDLs[] = {
    gLinkAdultRightHandHoldingBowFirstPersonDL,
    gLinkChildRightArmStretchedSlingshotDL,
};

// Indexed by model types (left hand, right hand, sheath or waist)
Gfx** sPlayerDListGroups[PLAYER_MODELTYPE_MAX] = {
    gPlayerLeftHandOpenDLs,           // PLAYER_MODELTYPE_LH_OPEN
    gPlayerLeftHandClosedDLs,         // PLAYER_MODELTYPE_LH_CLOSED
    sPlayerLeftHandSwordDLs,          // PLAYER_MODELTYPE_LH_SWORD
    sPlayerLeftHandSwordDLs2,         // PLAYER_MODELTYPE_LH_SWORD_2
    gPlayerLeftHandBgsDLs,            // PLAYER_MODELTYPE_LH_BGS
    sPlayerLeftHandHammerDLs,         // PLAYER_MODELTYPE_LH_HAMMER
    gPlayerLeftHandBoomerangDLs,      // PLAYER_MODELTYPE_LH_BOOMERANG
    sPlayerLeftHandBottleDLs,         // PLAYER_MODELTYPE_LH_BOTTLE
    sPlayerRightHandOpenDLs,          // PLAYER_MODELTYPE_RH_OPEN
    sPlayerRightHandClosedDLs,        // PLAYER_MODELTYPE_RH_CLOSED
    sPlayerRightHandShieldDLs,        // PLAYER_MODELTYPE_RH_SHIELD
    sPlayerRightHandBowSlingshotDLs,  // PLAYER_MODELTYPE_RH_BOW_SLINGSHOT
    sPlayerRightHandBowSlingshotDLs2, // PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2
    sPlayerRightHandOcarinaDLs,       // PLAYER_MODELTYPE_RH_OCARINA
    sPlayerRightHandOotDLs,           // PLAYER_MODELTYPE_RH_OOT
    sPlayerRightHandHookshotDLs,      // PLAYER_MODELTYPE_RH_HOOKSHOT
    sSwordAndSheathDLs,               // PLAYER_MODELTYPE_SHEATH_16
    sSheathDLs,                       // PLAYER_MODELTYPE_SHEATH_17
    sSheathWithSwordDLs,              // PLAYER_MODELTYPE_SHEATH_18
    sSheathWithoutSwordDLs,           // PLAYER_MODELTYPE_SHEATH_19
    sPlayerWaistDLs,                  // PLAYER_MODELTYPE_WAIST
};

Gfx gCullBackDList[] = {
    gsSPSetGeometryMode(G_CULL_BACK),
    gsSPEndDisplayList(),
};

Gfx gCullFrontDList[] = {
    gsSPSetGeometryMode(G_CULL_FRONT),
    gsSPEndDisplayList(),
};

Vec3f* D_80160000;
s32 sDListsLodOffset;
Vec3f sGetItemRefPos;
s32 sLeftHandType;
s32 sRightHandType;

void Player_SetBootData(PlayState* play, Player* this) {
    s32 currentBoots;
    s16* bootRegs;

    REG(27) = 2000;
    REG(48) = 370;

    currentBoots = this->currentBoots;
    if (currentBoots == PLAYER_BOOTS_KOKIRI) {
        if (!LINK_IS_ADULT) {
            currentBoots = PLAYER_BOOTS_KOKIRI_CHILD;
        }
    } else if (currentBoots == PLAYER_BOOTS_IRON) {
        if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) {
            currentBoots = PLAYER_BOOTS_IRON_UNDERWATER;
        }
        REG(27) = 500;
        REG(48) = 100;
    }

    bootRegs = sBootData[currentBoots];
    REG(19) = bootRegs[0];
    REG(30) = bootRegs[1];
    REG(32) = bootRegs[2];
    REG(34) = bootRegs[3];
    REG(35) = bootRegs[4];
    REG(36) = bootRegs[5];
    REG(37) = bootRegs[6];
    REG(38) = bootRegs[7];
    REG(43) = bootRegs[8];
    REG(45) = bootRegs[9];
    REG(68) = bootRegs[10];
    REG(69) = bootRegs[11];
    IREG(66) = bootRegs[12];
    IREG(67) = bootRegs[13];
    IREG(68) = bootRegs[14];
    IREG(69) = bootRegs[15];
    MREG(95) = bootRegs[16];

    if (play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_2) {
        REG(45) = 500;
    }

    // MM transformation boot physics (per-form movement REGs). Skijer's NEI
    if (TransformMasks_IsTransformed()) {
        extern void MmForm_ApplyBootData(void);
        MmForm_ApplyBootData();
    }
}

// Custom method used to determine if we're using a custom model for link
uint8_t Player_IsCustomLinkModel() {
    // Gerudo Form: treat the gerudo-rigged skel as a custom Link model. This
    // skips the vanilla hardcoded WAIST/HEAD/hand overrides in
    // Player_OverrideLimbDrawGameplayDefault, so the gerudo limb's own DL
    // (e.g. gLinkAdultSkel_layer_Opaque for the torso, bone010_* for the
    // head, etc.) stays as the renderer's choice instead of being clobbered
    // by Link's vanilla belt/eyes/closed-hand DLs.
    if (GerudoForm_IsActive()) {
        return 1;
    }
    // Skin forms (Kafei/Keaton/Rito): O2rLoader swapped a whole custom skeleton
    // into skelAnime, so the same reasoning applies — and it must force LOD 0,
    // because XML-authored SkeletonLimbs only carry a level-0 display list.
    if (O2rLoader_HasActiveModel()) {
        return 1;
    }
    return (LINK_IS_ADULT && ResourceGetIsCustomByName(gLinkAdultSkel)) ||
           (LINK_IS_CHILD && ResourceGetIsCustomByName(gLinkChildSkel));
}

s32 Player_InBlockingCsMode(PlayState* play, Player* this) {
    return (this->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE)) || (this->csAction != 0) ||
           (play->transitionTrigger == TRANS_TRIGGER_START) || (this->stateFlags1 & PLAYER_STATE1_LOADING) ||
           (this->stateFlags3 & PLAYER_STATE3_FLYING_WITH_HOOKSHOT) ||
           ((gSaveContext.magicState != MAGIC_STATE_IDLE) && (Player_ActionToMagicSpell(this, this->itemAction) >= 0));
}

s32 Player_InCsMode(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return Player_InBlockingCsMode(play, this) || (this->unk_6AD == 4);
}

/**
 * Checks if Player is currently locked onto a hostile actor.
 * `PLAYER_STATE1_HOSTILE_LOCK_ON` controls Player's "battle" response to hostile actors.
 *
 * Note that within Player, `Player_UpdateHostileLockOn` exists, which updates the flag and also returns the check.
 * Player can use this function instead if the flag should be checked, but not updated.
 */
s32 Player_CheckHostileLockOn(Player* this) {
    return (this->stateFlags1 & PLAYER_STATE1_HOSTILE_LOCK_ON);
}

s32 Player_IsChildWithHylianShield(Player* this) {
    s32 isChildHylian = (gSaveContext.linkAge != 0) && (this->currentShield == PLAYER_SHIELD_HYLIAN);

    return GameInteractor_Should(VB_PLAYER_USE_CHILD_HYLIAN_STANCE, isChildHylian, this);
}

s32 Player_ActionToModelGroup(Player* this, s32 actionParam) {
    s32 modelGroup = ExtPlayer_GetActionModelGroup(actionParam);

    if ((modelGroup == PLAYER_MODELGROUP_SWORD_AND_SHIELD) && Player_IsChildWithHylianShield(this)) {
        // child, using kokiri sword with hylian shield equipped
        return PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD;
    } else {
        return modelGroup;
    }
}

void Player_SetModelsForHoldingShield(Player* this) {
    if ((this->stateFlags1 & PLAYER_STATE1_SHIELDING) &&
        ((this->itemAction < 0) || (this->itemAction == this->heldItemAction))) {
        if ((CVarGetInteger(CVAR_CHEAT("ShieldTwoHanded"), 0) && (this->heldItemAction != PLAYER_IA_DEKU_STICK) ||
             !Player_HoldsTwoHandedWeapon(this)) &&
            !Player_IsChildWithHylianShield(this)) {
            this->rightHandType = PLAYER_MODELTYPE_RH_SHIELD;
            if (LINK_IS_CHILD && (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) &&
                (this->currentShield == PLAYER_SHIELD_MIRROR)) {
                this->rightHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_RH_SHIELD][0];
            } else if (LINK_IS_ADULT && (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) &&
                       (this->currentShield == PLAYER_SHIELD_DEKU)) {
                this->rightHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_RH_SHIELD][1];
            } else {
                this->rightHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_RH_SHIELD][gSaveContext.linkAge];
            }
            if (this->sheathType == PLAYER_MODELTYPE_SHEATH_18) {
                this->sheathType = PLAYER_MODELTYPE_SHEATH_16;
            } else if (this->sheathType == PLAYER_MODELTYPE_SHEATH_19) {
                this->sheathType = PLAYER_MODELTYPE_SHEATH_17;
            }
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][gSaveContext.linkAge];
            if ((CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) && LINK_IS_CHILD &&
                gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI) {
                this->sheathDLists = &sPlayerDListGroups[this->sheathType][0];
            } else if ((CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) && LINK_IS_ADULT &&
                       gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KOKIRI) {
                this->sheathDLists = &sPlayerDListGroups[this->sheathType][1];
            }
            // Gerudo keeps the weapon-drawn column while guarding: her guard clips
            // live there, and column 2 would hand the shield to Link's item set.
            if (!GerudoMhr_ForcesFighter(this)) {
                this->modelAnimType = PLAYER_ANIMTYPE_2;
            }
            this->itemAction = -1;
        }
    }
}

void Player_SetModels(Player* this, s32 modelGroup) {
    // Left hand
    this->leftHandType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_LEFT_HAND];
    this->leftHandDLists = &sPlayerDListGroups[this->leftHandType][gSaveContext.linkAge];

    // Custom rods: Override left hand to use closed fist instead of BGS sword model
    // The rod visual is drawn separately in CustomItems_Draw functions
    if (this->heldItemAction == PLAYER_IA_ROD_FIRE || this->heldItemAction == PLAYER_IA_ROD_ICE ||
        this->heldItemAction == PLAYER_IA_ROD_LIGHT) {
        this->leftHandType = PLAYER_MODELTYPE_LH_CLOSED;
        this->leftHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_LH_CLOSED][gSaveContext.linkAge];
    }

    // Net (Skijer's NEI): uses the Master Sword IA to swing 1:1, but must NOT show the sword model —
    // close the fist and let CustomItems_DrawNet draw the net in the hand. Keyed on heldItemId (the IA
    // is the sword IA, shared with real swords).
    if (this->heldItemId == ITEM_NET) {
        this->leftHandType = PLAYER_MODELTYPE_LH_CLOSED;
        this->leftHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_LH_CLOSED][gSaveContext.linkAge];
    }

    if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) {
        if (LINK_IS_CHILD &&
            (this->leftHandType == PLAYER_MODELTYPE_LH_HAMMER ||
             ((this->leftHandType == PLAYER_MODELTYPE_LH_SWORD || this->leftHandType == PLAYER_MODELTYPE_LH_BGS) &&
              (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI)))) {
            this->leftHandDLists = &sPlayerDListGroups[this->leftHandType][0];
        }

        if (LINK_IS_ADULT && (this->leftHandType == PLAYER_MODELTYPE_LH_BOOMERANG ||
                              (this->leftHandType == PLAYER_MODELTYPE_LH_SWORD &&
                               gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KOKIRI))) {
            this->leftHandDLists = &sPlayerDListGroups[this->leftHandType][1];
        }
    }

    // Right hand
    this->rightHandType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_RIGHT_HAND];
    this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][gSaveContext.linkAge];

    this->rightHandType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_RIGHT_HAND];
    this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][gSaveContext.linkAge];

    if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) {
        if (LINK_IS_CHILD &&
            (this->rightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT ||
             (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD && this->currentShield == PLAYER_SHIELD_MIRROR))) {
            this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][0];
        }
        if (LINK_IS_ADULT &&
            (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD && this->currentShield == PLAYER_SHIELD_DEKU)) {
            this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][1];
        }
    }
    if ((CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) ||
         CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) &&
        this->rightHandType == 11) { // If holding Bow/Slingshot
        this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][Player_HoldsSlingshot(this)];
    }

    // Sheath
    this->sheathType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_SHEATH];
    this->sheathDLists = &sPlayerDListGroups[this->sheathType][gSaveContext.linkAge];

    if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) {
        if (LINK_IS_CHILD && (this->currentShield == PLAYER_SHIELD_HYLIAN &&
                                  ((gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER) ||
                                   (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS)) ||
                              (this->currentShield == PLAYER_SHIELD_MIRROR) &&
                                  (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI))) {
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][0];
        } else if (LINK_IS_CHILD && this->currentShield == PLAYER_SHIELD_MIRROR &&
                   gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KOKIRI &&
                   this->sheathType == PLAYER_MODELTYPE_SHEATH_18) {
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][0];
        } else if (LINK_IS_ADULT && (this->currentShield == PLAYER_SHIELD_DEKU &&
                                     gSaveContext.equips.buttonItems[0] != ITEM_SWORD_MASTER) ||
                   (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER &&
                    this->sheathType == PLAYER_MODELTYPE_SHEATH_18 && this->currentShield == PLAYER_SHIELD_DEKU)) {
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][1];
        } else if (LINK_IS_CHILD && this->sheathType == PLAYER_MODELTYPE_SHEATH_17 &&
                   ((gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER) ||
                    (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS))) {
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][0];
        }
    }

    // Waist
    this->waistDLists = &sPlayerDListGroups[gPlayerModelTypes[modelGroup][4]][gSaveContext.linkAge];

    Player_SetModelsForHoldingShield(this);
    GameInteractor_ExecuteOnPlayerSetModels(this, modelGroup);
}

void Player_SetModelGroup(Player* this, s32 modelGroup) {
    this->modelGroup = modelGroup;

    if (modelGroup == PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD) {
        this->modelAnimType = PLAYER_ANIMTYPE_0;
    } else {
        this->modelAnimType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_ANIM];
    }

    // Vanilla: a drawn weapon with NO shield still uses the free-hand animation
    // set. Gerudo never has a shield in that sense, so this line demoted her to
    // column 0 every time and the entire dual-blades locomotion set — installed
    // into columns 1 and 3 — could never be read. She is the one exception: with
    // blades in hand she IS a fighter, shield or no shield.
    if ((this->modelAnimType < PLAYER_ANIMTYPE_3) && (this->currentShield == PLAYER_SHIELD_NONE) &&
        !GerudoMhr_ForcesFighter(this)) {
        this->modelAnimType = PLAYER_ANIMTYPE_0;
    }

    // ...and PROMOTE, not merely spare. Guarding is a fighter stance even with the
    // blades stowed, and with empty hands the group above resolves to column 0, so
    // without this R would read Link's own free-hand defense clip and the crossed
    // blades installed into columns 1/3 would never show. Only 0 is promoted: 2 is
    // "holding a normal item" and 3+ are the two-handed sets, which are not ours.
    if (GerudoMhr_ForcesFighter(this) && (this->modelAnimType < PLAYER_ANIMTYPE_1)) {
        this->modelAnimType = PLAYER_ANIMTYPE_1;
    }

    Player_SetModels(this, modelGroup);
}

void func_8008EC70(Player* this) {
    this->itemAction = this->heldItemAction;
    Player_SetModelGroup(this, Player_ActionToModelGroup(this, this->heldItemAction));
    this->unk_6AD = 0;
}

void Player_SetEquipmentData(PlayState* play, Player* this) {
    if (this->csAction != 0x56) {
        this->currentShield = SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD));
        this->currentTunic = TUNIC_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC));
        this->currentBoots = BOOTS_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS));
        this->currentSwordItemId = B_BTN_ITEM;
        Player_SetModelGroup(this, Player_ActionToModelGroup(this, this->heldItemAction));
        Player_SetBootData(play, this);
    }
}

void Player_UpdateBottleHeld(PlayState* play, Player* this, s32 item, s32 actionParam) {
    Inventory_UpdateBottleItem(play, item, this->heldItemButton);

    if (item != ITEM_BOTTLE) {
        this->heldItemId = item;
        this->heldItemAction = actionParam;
    }

    if (GameInteractor_Should(VB_PLAYER_UPDATE_BOTTLE_HELD, true, this)) {
        this->itemAction = actionParam;
    }
}

void Player_ReleaseLockOn(Player* this) {
    this->focusActor = NULL;
    this->stateFlags2 &= ~PLAYER_STATE2_LOCK_ON_WITH_SWITCH;
}

/**
 * This function aims to clear Z-Target related state when it isn't in use.
 * It also handles setting a specific free fall related state that is interntwined with Z-Targeting.
 * TODO: Learn more about this and give a name to PLAYER_STATE1_19
 */
void Player_ClearZTargeting(Player* this) {
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        (this->stateFlags1 & (PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER)) ||
        (!(this->stateFlags1 & (PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL)) &&
         ((this->actor.world.pos.y - this->actor.floorHeight) < 100.0f))) {
        this->stateFlags1 &=
            ~(PLAYER_STATE1_Z_TARGETING | PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS | PLAYER_STATE1_PARALLEL |
              PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE);
    } else if (!(this->stateFlags1 &
                 (PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL | PLAYER_STATE1_CLIMBING_LADDER))) {
        this->stateFlags1 |= PLAYER_STATE1_FREEFALL;
    }

    Player_ReleaseLockOn(this);
}

/**
 * Sets the "auto lock-on actor" to lock onto an actor without Player's input.
 * This function will first release any existing lock-on or (try to) release parallel.
 *
 * When using Switch Targeting, it is not possible to carry an auto lock-on actor into a normal
 * lock-on when the auto lock-on is finished.
 * This is because the `PLAYER_STATE2_LOCK_ON_WITH_SWITCH` flag is never set with an auto lock-on.
 * With Hold Targeting it is possible to keep the auto lock-on going by keeping the Z button held down.
 *
 * The auto lock-on is considered "friendly" even if the actor is actually hostile. If the auto lock-on is hostile,
 * Player's battle response will not occur (if he is actionable) and the camera behaves differently.
 * When transitioning from auto lock-on to normal lock-on (with Hold Targeting) there will be a noticeable change
 * when it switches from "friendly" mode to "hostile" mode.
 */
void Player_SetAutoLockOnActor(PlayState* play, Actor* actor) {
    Player* this = GET_PLAYER(play);

    Player_ClearZTargeting(this);
    this->focusActor = actor;
    this->autoLockOnActor = actor;
    this->stateFlags1 |= PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS;
    Camera_SetParam(Play_GetCamera(play, 0), 8, actor);
    Camera_ChangeMode(Play_GetCamera(play, 0), 2);
}

s32 func_8008EF30(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return (this->stateFlags1 & PLAYER_STATE1_ON_HORSE);
}

s32 func_8008EF44(PlayState* play, s32 ammo) {
    play->shootingGalleryStatus = ammo + 1;
    return 1;
}

s32 Player_IsBurningStickInRange(PlayState* play, Vec3f* pos, f32 xzRange, f32 yRange) {
    Player* this = GET_PLAYER(play);
    Vec3f diff;
    s32 pad;

    if ((this->heldItemAction == PLAYER_IA_DEKU_STICK) && (this->unk_860 != 0)) {
        Math_Vec3f_Diff(&this->meleeWeaponInfo[0].tip, pos, &diff);
        return ((SQ(diff.x) + SQ(diff.z)) <= SQ(xzRange)) && (0.0f <= diff.y) && (diff.y <= yRange);
    } else {
        return false;
    }
}

s32 Player_GetStrength(void) {
    s32 strengthUpgrade = CUR_UPG_VALUE(UPG_STRENGTH);

    if (CVarGetInteger(CVAR_ENHANCEMENT("ToggleStrength"), 0) &&
        CVarGetInteger(CVAR_ENHANCEMENT("StrengthDisabled"), 0)) {
        return PLAYER_STR_NONE;
    }

    // MM transformation forms have an intrinsic body strength (independent of save upgrade bits). Skijer's NEI
    if (TransformMasks_IsTransformed()) {
        extern s32 MmForm_GetStrengthOverride(void);
        s32 formStr = MmForm_GetStrengthOverride();
        if (formStr >= 0) {
            return formStr;
        }
    }

    // Giant's Mask grants max lift strength (Gold Gauntlets) without touching the
    // save upgrade bits, so randomizer progressive-strength logic stays intact. Skijer's NEI
    extern s32 MmMaskWear_IsGiantMaskActive(void);
    if (MmMaskWear_IsGiantMaskActive()) {
        return PLAYER_STR_GOLD_G;
    }

    if (CVarGetInteger(CVAR_CHEAT("TimelessEquipment"), 0) || LINK_IS_ADULT) {
        return strengthUpgrade;
    } else if (strengthUpgrade != 0) {
        return PLAYER_STR_BRACELET;
    } else {
        return PLAYER_STR_NONE;
    }
}

u8 Player_GetMask(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return this->currentMask;
}

Player* Player_UnsetMask(PlayState* play) {
    Player* this = GET_PLAYER(play);

    this->currentMask = PLAYER_MASK_NONE;

    return this;
}

// The Rito's shield reflects too. Answering here rather than adding a fourth
// PLAYER_SHIELD_* value is what makes every reflection site inherit it for free —
// Mir_Ray, Twinrova, Anubis and Ganon all go through these two predicates.
// Its model is drawn by the form (MmForm_PostLimbDraw), which is also where
// player->shieldMf gets captured, so rightHandType is never RH_SHIELD for it.
s32 Player_HasMirrorShieldEquipped(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return (this->currentShield == PLAYER_SHIELD_MIRROR) || MmForm_RitoShieldIsDrawn();
}

s32 Player_HasMirrorShieldSetToDraw(PlayState* play) {
    Player* this = GET_PLAYER(play);

    if (MmForm_RitoShieldIsDrawn()) {
        return true;
    }
    return (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD) && (this->currentShield == PLAYER_SHIELD_MIRROR);
}

s32 Player_ActionToMagicSpell(Player* this, s32 actionParam) {
    s32 magicSpell = actionParam - PLAYER_IA_MAGIC_SPELL_15;

    if ((magicSpell >= 0) && (magicSpell < 6)) {
        return magicSpell;
    } else {
        return -1;
    }
}

s32 Player_HoldsHookshot(Player* this) {
    return (this->heldItemAction == PLAYER_IA_HOOKSHOT) || (this->heldItemAction == PLAYER_IA_LONGSHOT);
}

s32 Player_HoldsBow(Player* this) {
    switch (this->heldItemAction) {
        case PLAYER_IA_BOW:
        case PLAYER_IA_BOW_FIRE:
        case PLAYER_IA_BOW_ICE:
        case PLAYER_IA_BOW_LIGHT:
        // SW97 elemental arrows (dark, soul, wind) also use the bow model.
        // Without these cases, Player_SetModels falls back to the slingshot
        // rendering path for these 3 arrow types and the bow disappears.
        case PLAYER_IA_BOW_0C:
        case PLAYER_IA_BOW_0D:
        case PLAYER_IA_BOW_0E:
            return true;
        default:
            return false;
    }
}

s32 Player_HoldsSlingshot(Player* this) {
    return this->heldItemAction == PLAYER_IA_SLINGSHOT;
}

s32 func_8008F128(Player* this) {
    return Player_HoldsHookshot(this) && (this->heldActor == NULL);
}

s32 Player_ActionToMeleeWeapon(s32 actionParam) {
    s32 sword = actionParam - PLAYER_IA_FISHING_POLE;

    if ((sword > 0) && (sword < 6)) {
        return sword;
    }

    // Custom melee weapons (Fire Rod, Ice Rod, Light Rod) - treated as Deku Stick (4)
    if (actionParam == PLAYER_IA_ROD_FIRE || actionParam == PLAYER_IA_ROD_ICE || actionParam == PLAYER_IA_ROD_LIGHT) {
        return 4; // Same as PLAYER_IA_DEKU_STICK
    }

    return 0;
}

/**
 * True while the Fierce Deity skin is active AND an actual sword is in hand.
 *
 * "Sword" means the melee-weapon indices 1..3 (Master / Kokiri / Biggoron) — NOT the
 * Deku Stick (4), the Megaton Hammer (5) or the custom rods, which Player_ActionToMeleeWeapon
 * also reports as melee weapons. This is the single gate behind FD's double-sword identity:
 * any sword equipped → FD swings the two-handed Deity sword; no sword → no sword AI at all.
 * Shared by Player_GetMeleeWeaponHeld, the VB_PLAYER_HOLDS_TWO_HANDED_WEAPON hook in
 * customequipment.cpp, and func_8083BB20 in z_player.c. Skijer's NEI 2026-07-28.
 */
s32 Player_IsFDHoldingSword(Player* this) {
    s32 meleeWeapon;

    if (!TransformMasks_IsFDSkinMode()) {
        return false;
    }

    meleeWeapon = Player_ActionToMeleeWeapon(this->heldItemAction);
    return (meleeWeapon >= 1) && (meleeWeapon <= 3);
}

s32 Player_SuffersHeat(Player* this) {
    s32 exposed = (this->currentTunic != PLAYER_TUNIC_GORON) && (CVarGetInteger(CVAR_CHEAT("SuperTunic"), 0) == 0);

    return GameInteractor_Should(VB_PLAYER_SUFFER_HEAT, exposed, this);
}

s32 Player_GetMeleeWeaponHeld(Player* this) {
    // Gerudo Dual Blades: she IS Link's sword pipeline in other clips, so OOT must see
    // her real sword (1..3, swords only). This one return is what lets B, the jump
    // slash, the charge and the hit-stop all run for her.
    {
        s32 gerudoIdx = GerudoMhr_MeleeWeaponIndex(this);
        if (gerudoIdx != 0) {
            return gerudoIdx;
        }
    }
    // Transformation masks: block sword swings for all forms except Fierce Deity.
    // Non-FD forms use form-specific B-button actions (punch, bubble, etc.).
    if (TransformMasks_IsTransformed() && !TransformMasks_IsFDSkinMode()) {
        return 0;
    }
    // FD skin mode: the Fierce Deity always wields the double-handed Deity sword, no
    // matter WHICH sword the player has equipped — Kokiri, Master or Biggoron all map to
    // the BGS melee index (3), giving BGS damage flags, 5500 reach and the BGS trail
    // without forcing heldItemAction (which causes equip/unequip animation loops).
    // Player_ActionToMeleeWeapon(PLAYER_IA_SWORD_BIGGORON) = 5 - 2 = 3.
    //
    // With NO sword in hand, FD gets no sword AI at all: Player_IsFDHoldingSword is
    // false, we fall through, and Player_ActionToMeleeWeapon(heldItemAction) returns 0.
    // Deliberately scoped to SWORDS only (indices 1..3) — a Deku Stick, the Megaton
    // Hammer or the Fire/Ice/Light rods keep their own identity in FD's hands instead of
    // silently becoming a Biggoron's Sword.
    if (Player_IsFDHoldingSword(this)) {
        return Player_ActionToMeleeWeapon(PLAYER_IA_SWORD_BIGGORON); // 3
    }
    // NEI Razor/Gilded Sword: the upgraded Kokiri Sword wields like the Master Sword
    // (reach 4000 + Master trail/damage flags). Gilded additionally deals Biggoron damage,
    // applied in func_80837948 (z_player.c). Only for the real Kokiri sword in human form.
    if (this->heldItemAction == PLAYER_IA_SWORD_KOKIRI && WeaponUpgrade_KokiriLevel() >= 1) {
        return Player_ActionToMeleeWeapon(PLAYER_IA_SWORD_MASTER); // 1
    }
    return Player_ActionToMeleeWeapon(this->heldItemAction);
}

s32 Player_HoldsTwoHandedWeapon(Player* this) {
    s32 result = (this->heldItemAction >= PLAYER_IA_SWORD_BIGGORON) && (this->heldItemAction <= PLAYER_IA_HAMMER);
    // Skijer's NEI: custom items / forms can be two-handed (FD sword, Fire/Ice/Light rods)
    return GameInteractor_Should(VB_PLAYER_HOLDS_TWO_HANDED_WEAPON, result, this);
}

s32 Player_HoldsBrokenKnife(Player* this) {
    return (this->heldItemAction == PLAYER_IA_SWORD_BIGGORON) && (gSaveContext.swordHealth <= 0.0f);
}

s32 Player_ActionToBottle(Player* this, s32 actionParam) {
    s32 bottle = actionParam - PLAYER_IA_BOTTLE;

    if ((bottle >= 0) && (bottle < 13)) {
        return bottle;
    } else {
        return -1;
    }
}

s32 Player_GetBottleHeld(Player* this) {
    return Player_ActionToBottle(this, this->heldItemAction);
}

s32 Player_ActionToExplosive(Player* this, s32 actionParam) {
    s32 explosive = actionParam - PLAYER_IA_BOMB;

    if ((explosive >= 0) && (explosive < 2)) {
        return explosive;
    } else {
        return -1;
    }
}

s32 Player_GetExplosiveHeld(Player* this) {
    return Player_ActionToExplosive(this, this->heldItemAction);
}

s32 func_8008F2BC(Player* this, s32 actionParam) {
    s32 sword = 0;

    if (actionParam != PLAYER_IA_SWORD_CS) {
        sword = actionParam - PLAYER_IA_SWORD_MASTER;
        if ((sword < 0) || (sword >= 3)) {
            goto return_neg;
        }
    }

    return sword;

return_neg:
    return -1;
}

s32 Player_GetEnvironmentalHazard(PlayState* play) {
    Player* this = GET_PLAYER(play);
    TextTriggerEntry* triggerEntry;
    s32 envHazard;

    if (play->roomCtx.curRoom.behaviorType2 == ROOM_BEHAVIOR_TYPE2_3) { // Room is hot
        envHazard = PLAYER_ENV_HAZARD_HOTROOM - 1;
    } else if ((this->underwaterTimer > 80) &&
               ((this->currentBoots == PLAYER_BOOTS_IRON) || (this->underwaterTimer >= 300))) { // Deep underwater
        envHazard = ((this->currentBoots == PLAYER_BOOTS_IRON) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND))
                        ? (PLAYER_ENV_HAZARD_UNDERWATER_FLOOR - 1)
                        : (PLAYER_ENV_HAZARD_UNDERWATER_FREE - 1);
    } else if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) { // Swimming
        envHazard = PLAYER_ENV_HAZARD_SWIMMING - 1;
    } else {
        return PLAYER_ENV_HAZARD_NONE;
    }

    // Trigger general textboxes under certain conditions, like "It's so hot in here!"
    if (!Player_InCsMode(play)) {
        triggerEntry = &sTextTriggers[envHazard];

        // Zora carries the tunic's effect without wearing it, so "You can't breathe!" must stay
        // silent for the form too.
        if ((triggerEntry->flag != 0) && !(gSaveContext.textTriggerFlags & triggerEntry->flag) &&
            (((envHazard == (PLAYER_ENV_HAZARD_HOTROOM - 1)) &&
              (Player_SuffersHeat(this) && CVarGetInteger(CVAR_ENHANCEMENT("DisableTunicWarningText"), 0) == 0)) ||
             (((envHazard == (PLAYER_ENV_HAZARD_UNDERWATER_FLOOR - 1)) ||
               (envHazard == (PLAYER_ENV_HAZARD_UNDERWATER_FREE - 1))) &&
              (this->currentBoots == PLAYER_BOOTS_IRON) &&
              (this->currentTunic != PLAYER_TUNIC_ZORA && !TransformMasks_HasWaterBreathing() &&
               CVarGetInteger(CVAR_CHEAT("SuperTunic"), 0) == 0 &&
               CVarGetInteger(CVAR_ENHANCEMENT("DisableTunicWarningText"), 0) == 0)))) {
            Message_StartTextbox(play, triggerEntry->textId, NULL);
            gSaveContext.textTriggerFlags |= triggerEntry->flag;
        }
    }

    return envHazard + 1;
}

u8 sEyeMouthIndexes[][2] = {
    { 0, 0 }, { 1, 0 }, { 2, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 }, { 4, 0 }, { 5, 1 },
    { 7, 2 }, { 0, 2 }, { 3, 0 }, { 4, 0 }, { 2, 2 }, { 1, 1 }, { 0, 2 }, { 0, 0 },
};

/**
 * Link's eye and mouth textures are placed at the exact same place in adult and child Link's respective object files.
 * This allows the array to only contain the symbols for one file and have it apply to both. This is a problem for
 * shiftability, and changes will need to be made in the code to account for this in a modding scenario. The symbols
 * from adult Link's object are used here.
 */

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
// TODO: Formatting
void* sEyeTextures[2][8] = {
    { gLinkAdultEyesOpenTex, gLinkAdultEyesHalfTex, gLinkAdultEyesClosedfTex, gLinkAdultEyesRollLeftTex,
      gLinkAdultEyesRollRightTex, gLinkAdultEyesShockTex, gLinkAdultEyesUnk1Tex, gLinkAdultEyesUnk2Tex },
    { gLinkChildEyesOpenTex, gLinkChildEyesHalfTex, gLinkChildEyesClosedfTex, gLinkChildEyesRollLeftTex,
      gLinkChildEyesRollRightTex, gLinkChildEyesShockTex, gLinkChildEyesUnk1Tex, gLinkChildEyesUnk2Tex },
};

#else
void* sEyeTextures[] = {
    gLinkAdultEyesOpenTex,      gLinkAdultEyesHalfTex,  gLinkAdultEyesClosedfTex, gLinkAdultEyesRollLeftTex,
    gLinkAdultEyesRollRightTex, gLinkAdultEyesShockTex, gLinkAdultEyesUnk1Tex,    gLinkAdultEyesUnk2Tex,
};
#endif

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
void* sMouthTextures[2][4] = {
    {
        gLinkAdultMouth1Tex,
        gLinkAdultMouth2Tex,
        gLinkAdultMouth3Tex,
        gLinkAdultMouth4Tex,
    },
    {
        gLinkChildMouth1Tex,
        gLinkChildMouth2Tex,
        gLinkChildMouth3Tex,
        gLinkChildMouth4Tex,
    },
};
#else
void* sMouthTextures[] = {
    gLinkAdultMouth1Tex,
    gLinkAdultMouth2Tex,
    gLinkAdultMouth3Tex,
    gLinkAdultMouth4Tex,
};
#endif

Color_RGB8 sTunicColors[] = {
    { 30, 105, 27 },
    { 100, 20, 0 },
    { 0, 60, 100 },
};

Color_RGB8 sGauntletColors[] = {
    { 255, 255, 255 },
    { 254, 207, 15 },
    // #region SOH [RBA] values matching OOB reads on N64
    { 0, 0, 6 },
    { 2, 89, 24 },
    { 6, 2, 90 },
    { 96, 6, 2 },
};

Gfx* sBootDListGroups[][2] = {
    { gLinkAdultLeftIronBootDL, gLinkAdultRightIronBootDL },   // PLAYER_BOOTS_IRON
    { gLinkAdultLeftHoverBootDL, gLinkAdultRightHoverBootDL }, // PLAYER_BOOTS_HOVER
};

// Skijer's NEI: the tunic env color the player body draws with (captured each frame in
// Player_DrawImpl, re-applied by the held-sword compound DL so the GFS's own env doesn't leak).
static Color_RGB8 sPlayerBodyEnvColor = { 255, 255, 255 };

void Player_DrawImpl(PlayState* play, void** skeleton, Vec3s* jointTable, s32 dListCount, s32 lod, s32 tunic, s32 boots,
                     s32 face, OverrideLimbDrawOpa overrideLimbDraw, PostLimbDrawOpa postLimbDraw, void* data) {
    Color_RGB8* color;
    s32 eyeIndex = (jointTable[22].x & 0xF) - 1;
    s32 mouthIndex = (jointTable[22].x >> 4) - 1;

    OPEN_DISPS(play->state.gfxCtx);

    if (eyeIndex < 0) {
        eyeIndex = sEyeMouthIndexes[face][0];
    }

    if (eyeIndex > 7)
        eyeIndex = 7;

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
    {
        void* pakEye = PakLoader_GetEyeTexture(eyeIndex);
        // A form's face textures ship under the vanilla eye symbol, so the same
        // name-based rule applies. Without this the head renders Link's eyes
        // through the form's own palette — the garbled face bug.
        void* formEye = pakEye ? NULL : CustomForms_ResolveVanillaTexture(sEyeTextures[gSaveContext.linkAge][eyeIndex]);
        if (pakEye) {
            gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)pakEye);
        } else if (formEye) {
            gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)formEye);
        } else {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[gSaveContext.linkAge][eyeIndex]));
        }
    }
#else
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[eyeIndex]));
#endif
    if (mouthIndex < 0) {
        mouthIndex = sEyeMouthIndexes[face][1];
    }

    if (mouthIndex > 3)
        mouthIndex = 3;

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
    {
        void* pakMouth = PakLoader_GetMouthTexture(mouthIndex);
        void* formMouth =
            pakMouth ? NULL : CustomForms_ResolveVanillaTexture(sMouthTextures[gSaveContext.linkAge][mouthIndex]);
        if (pakMouth) {
            gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)pakMouth);
        } else if (formMouth) {
            gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)formMouth);
        } else {
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sMouthTextures[gSaveContext.linkAge][mouthIndex]));
        }
    }
#else
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sMouthTextures[eyeIndex]));
#endif

    Color_RGB8 sTemp;
    color = &sTunicColors[tunic];
    if (tunic == PLAYER_TUNIC_KOKIRI && CVarGetInteger(CVAR_COSMETIC("Link.KokiriTunic.Changed"), 0)) {
        sTemp = CVarGetColor24(CVAR_COSMETIC("Link.KokiriTunic.Value"), sTunicColors[PLAYER_TUNIC_KOKIRI]);
        color = &sTemp;
    } else if (tunic == PLAYER_TUNIC_GORON && CVarGetInteger(CVAR_COSMETIC("Link.GoronTunic.Changed"), 0)) {
        sTemp = CVarGetColor24(CVAR_COSMETIC("Link.GoronTunic.Value"), sTunicColors[PLAYER_TUNIC_GORON]);
        color = &sTemp;
    } else if (tunic == PLAYER_TUNIC_ZORA && CVarGetInteger(CVAR_COSMETIC("Link.ZoraTunic.Changed"), 0)) {
        sTemp = CVarGetColor24(CVAR_COSMETIC("Link.ZoraTunic.Value"), sTunicColors[PLAYER_TUNIC_ZORA]);
        color = &sTemp;
    }

    // Extended RECOLOR tunics (Skijer 2026-07-16): each ext tunic paints Link's tunic 1:1 like the
    // vanilla Goron/Zora recolor (ext tunics equip with Kokiri base, so `tunic`==0 and this wins).
    if (ExtEquip_IsChampionTunic()) {
        // Champion's Tunic — blue #38b6f1
        sTemp.r = 56;
        sTemp.g = 182;
        sTemp.b = 241;
        color = &sTemp;
    } else if (ExtEquip_IsSpiritTunic()) {
        // Spirit Tunic — ORANGE when active (has rupees), BLACK when deactivated (broke).
        if (ExtEquip_SpiritHasMoney()) {
            sTemp.r = 235;
            sTemp.g = 110;
            sTemp.b = 20;
        } else {
            sTemp.r = 20;
            sTemp.g = 20;
            sTemp.b = 20;
        }
        color = &sTemp;
    } else if (ExtEquip_IsSagesTunic()) {
        // Sage's Tunic — white, briefly dyed with a medallion's color while its
        // resistance is absorbing damage (ExtEquip_SagesFlash).
        ExtEquip_GetSagesTunicColor(&sTemp.r, &sTemp.g, &sTemp.b);
        color = &sTemp;
    }

    // Trident (ext sword 3): the opening frames of the max-charge release make Link
    // untouchable, and the tunic goes gold so that is something you can SEE. Last in
    // the chain on purpose — it outranks every tunic, ext ones included, because it
    // is a state and not a garment. Skijer's NEI
    if (Trident_GoldenArmor()) {
        sTemp.r = 255;
        sTemp.g = 205;
        sTemp.b = 40;
        color = &sTemp;
    }

    // Skijer's NEI: remember the body tunic env so the held-sword compound DL (WeaponUpgrade_
    // ApplyHeldSwordDL) can re-apply it after a combined MM sword DL that sets its own env color.
    sPlayerBodyEnvColor = *color;

    if (GameInteractor_Should(VB_APPLY_TUNIC_COLOR, true, data, color)) {
        gDPSetEnvColor(POLY_OPA_DISP++, color->r, color->g, color->b, 0);
    }

    // If we have a custom link model, always use the most detailed LOD
    if (Player_IsCustomLinkModel()) {
        lod = 0;
    }

    sDListsLodOffset = lod * 2;

    // VB_PLAYER_DRAW: subscribers can suppress vanilla Link rendering by
    // returning false (e.g. Harpoon's Prop Hunt hider draws as a prop and
    // wants to hide Link entirely). Default keeps vanilla draw on.
    if (GameInteractor_Should(VB_PLAYER_DRAW, true, play, data)) {
        SkelAnime_DrawFlexLod(play, skeleton, jointTable, dListCount, overrideLimbDraw, postLimbDraw, data, lod);
    }

    if (!GameInteractor_InvisibleLinkActive() &&
        ((CVarGetInteger(CVAR_ENHANCEMENT("FirstPersonGauntlets"), 0) && LINK_IS_ADULT) ||
         (overrideLimbDraw != Player_OverrideLimbDrawGameplayFirstPerson)) &&
        (overrideLimbDraw != Player_OverrideLimbDrawGameplayCrawling) &&
        (gSaveContext.gameMode != GAMEMODE_END_CREDITS)) {
        if (LINK_IS_ADULT) {
            s32 strengthUpgrade = CUR_UPG_VALUE(UPG_STRENGTH);

            // Mogma Mitts: force white gauntlets visible even without strength upgrade
            if (gMogmaMittsForceGauntlets || strengthUpgrade >= 2) {
                gDPPipeSync(POLY_OPA_DISP++);

                // Mogma Mitts always uses white (silver) gauntlets
                if (gMogmaMittsForceGauntlets) {
                    color = &sGauntletColors[0]; // White/silver color
                } else {
                    color = &sGauntletColors[strengthUpgrade - 2];
                }
                if (!gMogmaMittsForceGauntlets && strengthUpgrade == PLAYER_STR_SILVER_G &&
                    CVarGetInteger(CVAR_COSMETIC("Gloves.SilverGauntlets.Changed"), 0)) {
                    sTemp = CVarGetColor24(CVAR_COSMETIC("Gloves.SilverGauntlets.Value"), *color);
                    color = &sTemp;
                } else if (!gMogmaMittsForceGauntlets && strengthUpgrade == PLAYER_STR_GOLD_G &&
                           CVarGetInteger(CVAR_COSMETIC("Gloves.GoldenGauntlets.Changed"), 0)) {
                    sTemp = CVarGetColor24(CVAR_COSMETIC("Gloves.GoldenGauntlets.Value"), *color);
                    color = &sTemp;
                }
                gDPSetEnvColor(POLY_OPA_DISP++, color->r, color->g, color->b, 0);

                gSPDisplayList(POLY_OPA_DISP++, gLinkAdultLeftGauntletPlate1DL);
                gSPDisplayList(POLY_OPA_DISP++, gLinkAdultRightGauntletPlate1DL);
                gSPDisplayList(POLY_OPA_DISP++, (sLeftHandType == PLAYER_MODELTYPE_LH_OPEN)
                                                    ? gLinkAdultLeftGauntletPlate2DL
                                                    : gLinkAdultLeftGauntletPlate3DL);
                gSPDisplayList(POLY_OPA_DISP++, (sRightHandType == PLAYER_MODELTYPE_RH_OPEN)
                                                    ? gLinkAdultRightGauntletPlate2DL
                                                    : gLinkAdultRightGauntletPlate3DL);
            }

            // Skijer 2026-07-15: Pegasus Anklet's model = the vanilla HOVER BOOTS recolored RED (no
            // custom DL anymore). When Pegasus is the current ext boots and no vanilla boot model is
            // shown, draw the hover-boot DLs through the grayscale-recolor path (same technique as
            // the age-restricted icon tint — reliable on any DL regardless of its combiner).
            {
                u8 pegasusRed = (boots == 0) && ExtEquip_IsEnabled() && (ExtEquip_GetCurrent(EQUIP_TYPE_BOOTS) == 1);

                if ((boots != 0) || pegasusRed) {
                    Gfx** bootDLists =
                        pegasusRed ? sBootDListGroups[PLAYER_BOOTS_HOVER - 1] : sBootDListGroups[boots - 1];

                    if (pegasusRed) {
                        gDPSetGrayscaleColor(POLY_OPA_DISP++, 210, 30, 30, 255);
                        gSPGrayscale(POLY_OPA_DISP++, true);
                    }
                    gSPDisplayList(POLY_OPA_DISP++, bootDLists[0]);
                    gSPDisplayList(POLY_OPA_DISP++, bootDLists[1]);
                    if (pegasusRed) {
                        gSPGrayscale(POLY_OPA_DISP++, false);
                    }
                }
            }
        } else {
            // Child Link
            if (gMogmaMittsForceGauntlets) {
                // Mogma Mitts: force white gauntlets visible on child Link too
                // Use adult gauntlet models scaled for child
                gDPPipeSync(POLY_OPA_DISP++);
                color = &sGauntletColors[0]; // White/silver color
                gDPSetEnvColor(POLY_OPA_DISP++, color->r, color->g, color->b, 0);

                gSPDisplayList(POLY_OPA_DISP++, gLinkAdultLeftGauntletPlate1DL);
                gSPDisplayList(POLY_OPA_DISP++, gLinkAdultRightGauntletPlate1DL);
                gSPDisplayList(POLY_OPA_DISP++, (sLeftHandType == PLAYER_MODELTYPE_LH_OPEN)
                                                    ? gLinkAdultLeftGauntletPlate2DL
                                                    : gLinkAdultLeftGauntletPlate3DL);
                gSPDisplayList(POLY_OPA_DISP++, (sRightHandType == PLAYER_MODELTYPE_RH_OPEN)
                                                    ? gLinkAdultRightGauntletPlate2DL
                                                    : gLinkAdultRightGauntletPlate3DL);
            } else if (Player_GetStrength() > PLAYER_STR_NONE) {
                gSPDisplayList(POLY_OPA_DISP++, gLinkChildGoronBraceletDL);
            }
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

Vec3f D_80126038[] = {
    { 1304.0f, 0.0f, 0.0f },
    { 695.0f, 0.0f, 0.0f },
};

f32 D_80126050[] = { 1265.0f, 826.0f };
f32 D_80126058[] = { SQ(13.04f), SQ(6.95f) };
f32 D_80126060[] = { 10.019104f, -19.925102f };
f32 D_80126068[] = { 5.0f, 3.0f };

Vec3f D_80126070 = { 0.0f, -300.0f, 0.0f };

void func_8008F87C(PlayState* play, Player* this, SkelAnime* skelAnime, Vec3f* pos, Vec3s* rot, s32 thighLimbIndex,
                   s32 shinLimbIndex, s32 footLimbIndex) {
    Vec3f spA4;
    // Minish tiny mode: the foot-planting IK raycasts the floor in WORLD space
    // and bends the leg toward it using an unscaled leg-length constant. At ~0.001
    // scale the foot sits far "below" the expected plant point every frame, so the
    // IK computes huge bend angles and folds the legs up into the waist. Skip it
    // while tiny — the legs just play their normal (scaled) animation instead.
    extern s32 MinishTiny_IsActive(void);
    if (MinishTiny_IsActive()) {
        return;
    }
    Vec3f sp98;
    Vec3f footprintPos;
    CollisionPoly* sp88;
    s32 sp84;
    f32 sp80;
    f32 sp7C;
    f32 sp78;
    f32 sp74;
    f32 sp70;
    f32 sp6C;
    f32 sp68;
    f32 sp64;
    f32 sp60;
    f32 sp5C;
    f32 sp58;
    f32 sp54;
    f32 sp50;
    s16 temp1;
    s16 temp2;
    s32 temp3;

    if ((this->actor.scale.y >= 0.0f) && !(this->stateFlags1 & PLAYER_STATE1_DEAD) &&
        (Player_ActionToMagicSpell(this, this->itemAction) < 0)) {
        s32 pad;

        sp7C = D_80126058[gSaveContext.linkAge];
        sp78 = D_80126060[gSaveContext.linkAge];
        sp74 = D_80126068[gSaveContext.linkAge] - this->unk_6C4;

        Matrix_Push();
        Matrix_TranslateRotateZYX(pos, rot);
        Matrix_MultVec3f(&sZeroVec, &spA4);
        Matrix_TranslateRotateZYX(&D_80126038[gSaveContext.linkAge], &skelAnime->jointTable[shinLimbIndex]);
        Matrix_Translate(D_80126050[gSaveContext.linkAge], 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_MultVec3f(&sZeroVec, &sp98);
        Matrix_MultVec3f(&D_80126070, &footprintPos);
        Matrix_Pop();

        footprintPos.y += 15.0f;

        sp80 = BgCheck_EntityRaycastFloor4(&play->colCtx, &sp88, &sp84, &this->actor, &footprintPos) + sp74;

        if (sp98.y < sp80) {
            sp70 = sp98.x - spA4.x;
            sp6C = sp98.y - spA4.y;
            sp68 = sp98.z - spA4.z;

            sp64 = sqrtf(SQ(sp70) + SQ(sp6C) + SQ(sp68));
            sp60 = (SQ(sp64) + sp78) / (2.0f * sp64);

            sp58 = sp7C - SQ(sp60);
            sp58 = (sp7C < SQ(sp60)) ? 0.0f : sqrtf(sp58);

            sp54 = Math_FAtan2F(sp58, sp60);

            sp6C = sp80 - spA4.y;

            sp64 = sqrtf(SQ(sp70) + SQ(sp6C) + SQ(sp68));
            sp60 = (SQ(sp64) + sp78) / (2.0f * sp64);
            sp5C = sp64 - sp60;

            sp58 = sp7C - SQ(sp60);
            sp58 = (sp7C < SQ(sp60)) ? 0.0f : sqrtf(sp58);

            sp50 = Math_FAtan2F(sp58, sp60);

            temp1 = (M_PI - (Math_FAtan2F(sp5C, sp58) + ((M_PI / 2) - sp50))) * (0x8000 / M_PI);
            temp1 = temp1 - skelAnime->jointTable[shinLimbIndex].z;

            if ((s16)(ABS(skelAnime->jointTable[shinLimbIndex].x) + ABS(skelAnime->jointTable[shinLimbIndex].y)) < 0) {
                temp1 += 0x8000;
            }

            temp2 = (sp50 - sp54) * (0x8000 / M_PI);
            rot->z -= temp2;

            skelAnime->jointTable[thighLimbIndex].z = skelAnime->jointTable[thighLimbIndex].z - temp2;
            skelAnime->jointTable[shinLimbIndex].z = skelAnime->jointTable[shinLimbIndex].z + temp1;
            skelAnime->jointTable[footLimbIndex].z = skelAnime->jointTable[footLimbIndex].z + temp2 - temp1;

            temp3 = SurfaceType_GetFloorType(&play->colCtx, sp88, sp84);

            if ((temp3 >= 2) && (temp3 < 4) && !SurfaceType_IsWallDamage(&play->colCtx, sp88, sp84)) {
                footprintPos.y = sp80;
                EffectSsGFire_Spawn(play, &footprintPos);
            }
        }
    }
}

s32 Player_OverrideLimbDrawGameplayCommon(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                          void* thisx) {
    Player* this = (Player*)thisx;

    // Kite Shield: crouch/lean the lower body over the board while shield surfing. Self-guards on
    // the surf being active. Skijer's NEI
    KiteSurf_AdjustLimb(limbIndex, rot);

    if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) &&
        CVarGetInteger(CVAR_ENHANCEMENT("ScaleAdultEquipmentAsChild"), 0) && LINK_IS_CHILD) {
        if (limbIndex == PLAYER_LIMB_L_HAND) {
            if ((gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI &&
                 sLeftHandType == PLAYER_MODELTYPE_LH_SWORD) ||
                (sLeftHandType == PLAYER_MODELTYPE_LH_BGS) || (sLeftHandType == PLAYER_MODELTYPE_LH_HAMMER)) {
                Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
            }
        }
        if (limbIndex == PLAYER_LIMB_R_HAND) {
            if ((this->currentShield == PLAYER_SHIELD_MIRROR && sRightHandType == PLAYER_MODELTYPE_RH_SHIELD) ||
                sRightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT ||
                (sRightHandType == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT && Player_HoldsBow(this))) {
                Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
            }
        }
        if (limbIndex == PLAYER_LIMB_SHEATH) {
            if ((this->currentShield == PLAYER_SHIELD_MIRROR ||
                 (this->currentShield == PLAYER_SHIELD_HYLIAN &&
                  (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER ||
                   gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS))) &&
                ((this->sheathType == PLAYER_MODELTYPE_SHEATH_16) || (this->sheathType == PLAYER_MODELTYPE_SHEATH_17) ||
                 (this->sheathType == PLAYER_MODELTYPE_SHEATH_18) ||
                 (this->sheathType == PLAYER_MODELTYPE_SHEATH_19))) {
                Matrix_Translate(218, -100, 62, MTXMODE_APPLY);
                Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
            }
            if ((this->currentShield == PLAYER_SHIELD_DEKU && gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI &&
                 (this->sheathType == PLAYER_MODELTYPE_SHEATH_16 || this->sheathType == PLAYER_MODELTYPE_SHEATH_17))) {
                Matrix_Translate(218, -100, 62, MTXMODE_APPLY);
                Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
            }
        }
    }

    if (limbIndex == PLAYER_LIMB_ROOT) {
        sLeftHandType = this->leftHandType;
        sRightHandType = this->rightHandType;
        D_80160000 = &this->meleeWeaponInfo[2].base;

        if (!LINK_IS_ADULT) {
            if (!(this->skelAnime.movementFlags & 4) || (this->skelAnime.movementFlags & 1)) {
                pos->x *= 0.64f;
                pos->z *= 0.64f;
            }

            if (!(this->skelAnime.movementFlags & 4) || (this->skelAnime.movementFlags & 2)) {
                pos->y *= 0.64f;
            }
        }

        pos->y -= this->unk_6C4;

        if (this->unk_6C2 != 0) {
            Matrix_Translate(pos->x, ((Math_CosS(this->unk_6C2) - 1.0f) * 200.0f) + pos->y, pos->z, MTXMODE_APPLY);
            Matrix_RotateX(this->unk_6C2 * (M_PI / 0x8000), MTXMODE_APPLY);
            Matrix_RotateZYX(rot->x, rot->y, rot->z, MTXMODE_APPLY);
            pos->x = pos->y = pos->z = 0.0f;
            rot->x = rot->y = rot->z = 0;
        }
    } else {
        if (*dList != NULL) {
            D_80160000++;
        }

        if (limbIndex == PLAYER_LIMB_HEAD) {
            if (CVarGetInteger(CVAR_COSMETIC("Link.HeadScale.Changed"), 0)) {
                f32 scale = CVarGetFloat(CVAR_COSMETIC("Link.HeadScale.Value"), 1.0f);
                if (scale != 1.0f) {
                    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                    if (scale > 1.2f) {
                        Matrix_Translate(-((LINK_IS_ADULT ? 320.0f : 200.0f) * scale), 0.0f, 0.0f, MTXMODE_APPLY);
                    } else if (scale < 1.0f) {
                        Matrix_Translate((LINK_IS_ADULT ? 3600.0f : 2900.0f) * ABS(scale - 1.0f), 0.0f, 0.0f,
                                         MTXMODE_APPLY);
                    }
                }
            }
            rot->x += this->headLimbRot.z;
            rot->y -= this->headLimbRot.y;
            rot->z += this->headLimbRot.x;
        } else if (limbIndex == PLAYER_LIMB_L_HAND) {
            if (CVarGetInteger(CVAR_COSMETIC("Link.SwordScale.Changed"), 0)) {
                f32 scale = CVarGetFloat(CVAR_COSMETIC("Link.SwordScale.Value"), 1.0f);
                if (scale != 1.0f) {
                    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                    Matrix_Translate(-((LINK_IS_ADULT ? 320.0f : 200.0f) * (scale - 1.0f)), 0.0f, 0.0f, MTXMODE_APPLY);
                }
            }
        } else if (limbIndex == PLAYER_LIMB_UPPER) {
            if (this->upperLimbYawSecondary != 0) {
                Matrix_RotateZ(0x44C * (M_PI / 0x8000), MTXMODE_APPLY);
                Matrix_RotateY(this->upperLimbYawSecondary * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            if (this->upperLimbRot.y != 0) {
                Matrix_RotateY(this->upperLimbRot.y * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            if (this->upperLimbRot.x != 0) {
                Matrix_RotateX(this->upperLimbRot.x * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            if (this->upperLimbRot.z != 0) {
                Matrix_RotateZ(this->upperLimbRot.z * (M_PI / 0x8000), MTXMODE_APPLY);
            }
        } else if (limbIndex == PLAYER_LIMB_L_THIGH) {
            func_8008F87C(play, this, &this->skelAnime, pos, rot, PLAYER_LIMB_L_THIGH, PLAYER_LIMB_L_SHIN,
                          PLAYER_LIMB_L_FOOT);
        } else if (limbIndex == PLAYER_LIMB_R_THIGH) {
            func_8008F87C(play, this, &this->skelAnime, pos, rot, PLAYER_LIMB_R_THIGH, PLAYER_LIMB_R_SHIN,
                          PLAYER_LIMB_R_FOOT);
            return false;
        } else {
            return false;
        }
    }

    return false;
}

// Defined in soh/Network/Harpoon/HarpoonSkinSync.cpp. Inline forward decl
// avoids dragging the C++ header (with its <string>/<vector> stuff) into
// every TU that includes z_player_lib.c via the unity build. Returns the
// override-or-patched-vanilla Gfx* for `otrPath` during a Harpoon dummy
// draw, or NULL otherwise — caller must fall through to its normal
// ResourceMgr_LoadGfxByName path on NULL.
extern void* HarpoonSkinSync_ResolvePlayerLimbDL(const char* otrPath);

// Helper for the four hand/sheath/waist branches below: if Harpoon's dummy
// draw is active and we have the path cached, hand back the override /
// patched-vanilla Gfx* directly instead of going through the global
// ArchiveManager (which would return the LOCAL user's modded bytecode and
// paint it onto the remote dummy).
// Defined in mods/transformation_masks/mm_player_form.cpp. Returns the empty-hand Gfx*
// while the Kafei skin is whistling and the engine just asked for an ocarina hand,
// NULL otherwise.
extern void* MmForm_KafeiWhistleHandDL(const char* otrPath);

static Gfx* Player_ResolveLimbDLForDummyOrLocal(void* dlPathOrPtr) {
    Gfx* kafeiDL = (Gfx*)MmForm_KafeiWhistleHandDL((const char*)dlPathOrPtr);
    if (kafeiDL != NULL) {
        return kafeiDL;
    }

    Gfx* harpoonDL = (Gfx*)HarpoonSkinSync_ResolvePlayerLimbDL((const char*)dlPathOrPtr);
    if (harpoonDL != NULL) {
        return harpoonDL;
    }
    // Custom forms: this is where a vanilla resource NAME becomes a pointer, so
    // it is the last moment a form can offer its own version of the hand /
    // sheath / item DL the engine just picked. If the form doesn't ship this
    // one we fall through and vanilla answers, exactly as before.
    Gfx* formDL = (Gfx*)CustomForms_ResolveVanillaResource((const char*)dlPathOrPtr);
    if (formDL != NULL) {
        return formDL;
    }
    return ResourceMgr_LoadGfxByName(dlPathOrPtr);
}

s32 Player_OverrideLimbDrawGameplayDefault(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                           void* thisx) {
    Player* this = (Player*)thisx;

    if (!Player_OverrideLimbDrawGameplayCommon(play, limbIndex, dList, pos, rot, thisx)) {
        // Gerudo Form dual-wield (hand = scimitar DL, sheath hidden). Skijer's NEI
        u8 gerudoHandled = GerudoForm_ResolveLimbDL(limbIndex, dList);

        // PAK Loader: When a custom model or equipment pak is active, try equipment DLs first.
        // If GetEquipDL returns a DL or STUB, use it. If NULL, fall through to vanilla code.
        if (!gerudoHandled) {
            u8 pakHandled = 0;
            if (PakLoader_HasActiveModel() && (limbIndex == PLAYER_LIMB_L_HAND || limbIndex == PLAYER_LIMB_R_HAND ||
                                               limbIndex == PLAYER_LIMB_SHEATH || limbIndex == PLAYER_LIMB_WAIST)) {
                Gfx* pakDL = PakLoader_GetEquipDL(this, limbIndex);
                if (pakDL == PAK_DL_STUB) {
                    *dList = NULL;
                    pakHandled = 1;
                } else if (pakDL != NULL) {
                    *dList = pakDL;
                    pakHandled = 1;
                }
                // pakDL == NULL for hands/sheath → fall through to vanilla for that limb
                // pakDL == NULL for WAIST → skeleton swap already provides the custom DL, don't let vanilla overwrite
                if (pakDL == NULL && limbIndex == PLAYER_LIMB_WAIST && PakLoader_GetSelectedIndex() >= 0) {
                    pakHandled = 1; // Keep skeleton's custom waist DL
                }
            }
            if (!pakHandled && limbIndex == PLAYER_LIMB_L_HAND) {
                Gfx** dLists = this->leftHandDLists;

                if ((sLeftHandType == PLAYER_MODELTYPE_LH_BGS) && (gSaveContext.swordHealth <= 0.0f)) {
                    dLists += 4;
                } else if ((sLeftHandType == PLAYER_MODELTYPE_LH_BOOMERANG) &&
                           (this->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN)) {
                    dLists = &gPlayerLeftHandOpenDLs[gSaveContext.linkAge];
                    sLeftHandType = PLAYER_MODELTYPE_LH_OPEN;
                } else if ((this->leftHandType == PLAYER_MODELTYPE_LH_OPEN) && (this->actor.speedXZ > 2.0f) &&
                           !(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
                    dLists = &gPlayerLeftHandClosedDLs[gSaveContext.linkAge];
                    sLeftHandType = PLAYER_MODELTYPE_LH_CLOSED;
                }

                // Extended equipment: hide sword DL when ext sword draws its own model
                if (ExtEquip_ShouldHideSwordDL() &&
                    (sLeftHandType != PLAYER_MODELTYPE_LH_OPEN && sLeftHandType != PLAYER_MODELTYPE_LH_CLOSED &&
                     sLeftHandType != PLAYER_MODELTYPE_LH_BOOMERANG)) {
                    dLists = &gPlayerLeftHandOpenDLs[gSaveContext.linkAge];
                    sLeftHandType = PLAYER_MODELTYPE_LH_OPEN;
                }

                // Boss Remains: Odolwa hides Link's native sword to a plain closed fist (his own
                // sword is drawn on top in Player_PostLimbDrawGameplay); Goht disables the sword
                // entirely — empty closed fist whenever the hand would hold one. Mirrors MM 2ship.
                if ((BossRemains_IsOdolwaWorn() || BossRemains_IsGohtWorn()) &&
                    (sLeftHandType != PLAYER_MODELTYPE_LH_OPEN && sLeftHandType != PLAYER_MODELTYPE_LH_CLOSED &&
                     sLeftHandType != PLAYER_MODELTYPE_LH_BOOMERANG)) {
                    dLists = &gPlayerLeftHandClosedDLs[gSaveContext.linkAge];
                    sLeftHandType = PLAYER_MODELTYPE_LH_CLOSED;
                }
                *dList = Player_ResolveLimbDLForDummyOrLocal(dLists[sDListsLodOffset]);
            } else if (!pakHandled && limbIndex == PLAYER_LIMB_R_HAND) {
                Gfx** dLists = this->rightHandDLists;

                if (sRightHandType == PLAYER_MODELTYPE_RH_SHIELD) {
                    // Boss Remains: Odolwa's remains also hide the native hand-held shield — his own
                    // shield is drawn in Player_PostLimbDrawGameplay (R_HAND). Mirrors MM 2ship.
                    if ((ExtEquip_GetShieldDLOverride() != NULL) || BossRemains_IsOdolwaWorn()) {
                        // Shield of Ikana: show open hand, custom shield drawn in PostLimbDraw
                        dLists = &sPlayerRightHandOpenDLs[gSaveContext.linkAge];
                        sRightHandType = PLAYER_MODELTYPE_RH_OPEN;
                    } else {
                        dLists += this->currentShield * 4;
                    }
                } else if (ItemEquip_HoldsClosedFist()) {
                    // Slate / Rod of Seasons in hand: gripped like the Hookshot. The shield keeps
                    // precedence above so its row offset is never skipped. Skijer's NEI
                    dLists = &sPlayerRightHandClosedDLs[gSaveContext.linkAge];
                    sRightHandType = PLAYER_MODELTYPE_RH_CLOSED;
                } else if (ItemEquip_HoldsEmptyHand()) {
                    // Recall aim / Ultrahand carry: both take the HOOKSHOT group for its reaching
                    // pose, and this is what keeps the hookshot itself out of the hand. Skijer's NEI
                    dLists = &sPlayerRightHandOpenDLs[gSaveContext.linkAge];
                    sRightHandType = PLAYER_MODELTYPE_RH_OPEN;
                } else if ((this->rightHandType == PLAYER_MODELTYPE_RH_OPEN) && (this->actor.speedXZ > 2.0f) &&
                           !(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
                    dLists = &sPlayerRightHandClosedDLs[gSaveContext.linkAge];
                    sRightHandType = PLAYER_MODELTYPE_RH_CLOSED;
                }

                *dList = Player_ResolveLimbDLForDummyOrLocal(dLists[sDListsLodOffset]);
            } else if (!pakHandled && limbIndex == PLAYER_LIMB_SHEATH) {
                Gfx** dLists = this->sheathDLists;

                if ((this->sheathType == PLAYER_MODELTYPE_SHEATH_18) ||
                    (this->sheathType == PLAYER_MODELTYPE_SHEATH_19)) {
                    if (ExtEquip_GetShieldDLOverride() != NULL) {
                        dLists = &sSheathDLs[gSaveContext.linkAge];
                    } else {
                        dLists += this->currentShield * 4;
                        if (!LINK_IS_ADULT && (this->currentShield < PLAYER_SHIELD_HYLIAN) &&
                            (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI)) {
                            dLists += PLAYER_SHIELD_MAX * 4;
                        }
                    }
                } else if (!CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) ||
                           (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) &&
                            ((gSaveContext.equips.buttonItems[0] != ITEM_SWORD_MASTER &&
                              gSaveContext.equips.buttonItems[0] != ITEM_SWORD_BGS) &&
                             this->currentShield == PLAYER_SHIELD_DEKU))) {
                    if (!LINK_IS_ADULT &&
                        ((this->sheathType == PLAYER_MODELTYPE_SHEATH_16) ||
                         (this->sheathType == PLAYER_MODELTYPE_SHEATH_17)) &&
                        (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI)) {
                        dLists = &sSheathWithSwordDLs[PLAYER_SHIELD_MAX * 4];
                    }
                }

                if (dLists[sDListsLodOffset] != NULL) {
                    *dList = Player_ResolveLimbDLForDummyOrLocal(dLists[sDListsLodOffset]);
                } else {
                    *dList = NULL;
                }

            } else if (!pakHandled && limbIndex == PLAYER_LIMB_WAIST) {

                if (!Player_IsCustomLinkModel()) {
                    *dList = Player_ResolveLimbDLForDummyOrLocal(
                        this->waistDLists[sDListsLodOffset]); // NOTE: This needs to be disabled when using custom
                                                              // characters - they're not going to have LODs anyways...
                }
            }
        } // close pakHandled block

        // Hide Link's held-weapon DL for items that draw their own model (rods/Byrna/IK Axe).
        // After pak_loader so o2r equipment mods are caught too. Skijer's NEI
        if (!gerudoHandled && limbIndex == PLAYER_LIMB_L_HAND && this->actor.scale.y >= 0.0f) {
            // Ext-equipment pieces that draw their OWN weapon (Byrna cane, Trident
            // lance) have to suppress the NEI upgrade blades too — Razor / Gilded /
            // Great Fairy's are drawn by the else-branch below, which never consults
            // VB_PLAYER_SHOULD_HIDE_HELD_WEAPON.
            //
            // ⚠️ Setting hideLH alone was NOT enough and read as "the fix does
            // nothing": the L_HAND limb block ~90 lines up ALREADY forced the hand
            // open for the same reason and left sLeftHandType == LH_OPEN behind. So
            // the `sLeftHandType != LH_OPEN` guard below was false, control fell into
            // the else, and WeaponUpgrade_ApplyHeldSwordDL drew the Gilded Sword on
            // an open hand. The earlier hide SUCCEEDING is what routed us here.
            //
            // Hence the separate flag: when ext equipment owns the weapon the upgrade
            // blade must not be drawn at all, whatever sLeftHandType already says.
            // Skijer's NEI
            u8 extOwnsWeapon = ExtEquip_ShouldHideSwordDL();
            u8 hideLH = extOwnsWeapon || GameInteractor_Should(VB_PLAYER_SHOULD_HIDE_HELD_WEAPON, false, this);
            if (hideLH && sLeftHandType != PLAYER_MODELTYPE_LH_OPEN && sLeftHandType != PLAYER_MODELTYPE_LH_CLOSED &&
                sLeftHandType != PLAYER_MODELTYPE_LH_BOOMERANG) {
                Gfx** openDLs = &gPlayerLeftHandOpenDLs[gSaveContext.linkAge];
                *dList = Player_ResolveLimbDLForDummyOrLocal(openDLs[sDListsLodOffset]);
                sLeftHandType = PLAYER_MODELTYPE_LH_OPEN;
            } else if (!extOwnsWeapon) {
                // NEI progressive sword upgrades: keep an OOT open hand and draw the MM Razor /
                // Gilded / Great Fairy's Sword pieces (loaded from o2r) on top — pak_loader-style
                // (sword then hand), supporting mods. No-op unless the upgraded sword is wielded.
                Gfx** openDLs = &gPlayerLeftHandOpenDLs[gSaveContext.linkAge];
                void* ootHand = Player_ResolveLimbDLForDummyOrLocal(openDLs[sDListsLodOffset]);
                if (WeaponUpgrade_ApplyHeldSwordDL(dList, ootHand, this, sPlayerBodyEnvColor.r, sPlayerBodyEnvColor.g,
                                                   sPlayerBodyEnvColor.b)) {
                    sLeftHandType = PLAYER_MODELTYPE_LH_OPEN;
                } else if (sLeftHandType == PLAYER_MODELTYPE_LH_OPEN) {
                    // No upgraded sword to draw, so ootHand was resolved and then dropped:
                    // *dList kept whatever vanilla picked earlier, which is LINK's hand even
                    // when a custom skin is active. That is why Kafei whistled with an open
                    // Link hand on the left while his right hand was correct - the right
                    // hand's branches all assign, this one only assigned on a hit.
                    // Only for an OPEN hand: any other type means *dList is holding
                    // something and must not be replaced by an empty palm.
                    *dList = ootHand;
                }
            }
        }

        // Twilight clawshot mode: R-hand hookshot DL = OOT closed hand + MM hookshot body. Skijer's NEI
        if (!gerudoHandled && limbIndex == PLAYER_LIMB_R_HAND && this->actor.scale.y >= 0.0f &&
            sRightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT) {
            extern void TwilightUpgrade_ApplyClawshotHandDL(Gfx * *dList, void* ootHand);
            void* ootHand = (sDListsLodOffset == 0) ? gLinkAdultRightHandClosedNearDL : gLinkAdultRightHandClosedFarDL;
            TwilightUpgrade_ApplyClawshotHandDL(dList, ootHand);
        }
    }

    GameInteractor_Should(VB_PLAYER_OVERRIDE_LIMB_DRAW, true, limbIndex, dList, thisx, play);

    // PAK Loader equipment mix must outrank CustomEquipment's VB_PLAYER_OVERRIDE_LIMB_DRAW
    // hook. Upstream #6708 added that hook at the END of this function — after our equipment
    // override at the top — so a per-slot pak selection got silently repainted by any active
    // equipment o2r mod. Re-apply the pak's DL here, but only when it actually provides one for
    // this limb: limbs without a per-slot/equipment-pack override keep the hook's result, so
    // o2r equipment still shows where there's no mix selection (the two coexist, as before).
    if (PakLoader_HasActiveModel() && (limbIndex == PLAYER_LIMB_L_HAND || limbIndex == PLAYER_LIMB_R_HAND ||
                                       limbIndex == PLAYER_LIMB_SHEATH || limbIndex == PLAYER_LIMB_WAIST)) {
        Gfx* pakReDL = PakLoader_GetEquipDL(this, limbIndex);
        if (pakReDL == PAK_DL_STUB) {
            *dList = NULL;
        } else if (pakReDL != NULL) {
            *dList = pakReDL;
        }
    }

    if (GameInteractor_InvisibleLinkActive()) {
        this->actor.shape.shadowDraw = NULL;
        *dList = NULL;
    }

    return false;
}

s32 Player_OverrideLimbDrawGameplayFirstPerson(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                               void* thisx) {
    Player* this = (Player*)thisx;

    if (!Player_OverrideLimbDrawGameplayCommon(play, limbIndex, dList, pos, rot, thisx)) {
        if (TransformMasks_IsTransformed()) {
            // Transformed: hide ALL limbs (including arm). Skeleton is still traversed
            // so body part positions are calculated for hookshot chain, arrow spawn, etc.
            *dList = NULL;
        } else if (this->unk_6AD != 2) {
            *dList = NULL;
        } else if (!Player_HoldsHookshot(this) && !Player_HoldsBow(this) && !Player_HoldsSlingshot(this) &&
                   this->heldItemAction != PLAYER_IA_BOMB_ARROWS) {
            // Custom item in first-person mode - hide vanilla weapon/arm models
            *dList = NULL;
        } else if (limbIndex == PLAYER_LIMB_L_FOREARM) {
            *dList = sFirstPersonLeftForearmDLs[gSaveContext.linkAge];
        } else if (limbIndex == PLAYER_LIMB_L_HAND) {
            s32 handOutDlIndex = gSaveContext.linkAge;
            if ((CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) ||
                 CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) &&
                LINK_IS_ADULT && Player_HoldsSlingshot(this)) {
                handOutDlIndex = 1;
            }
            *dList = sFirstPersonLeftHandDLs[handOutDlIndex];
        } else if (limbIndex == PLAYER_LIMB_R_SHOULDER) {
            *dList = sFirstPersonRightShoulderDLs[gSaveContext.linkAge];
        } else if (limbIndex == PLAYER_LIMB_R_FOREARM) {
            *dList = sFirstPersonForearmDLs[gSaveContext.linkAge];
        } else if (limbIndex == PLAYER_LIMB_R_HAND) {
            s32 firstPersonWeaponIndex = gSaveContext.linkAge;
            if (CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) ||
                CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) {
                if (Player_HoldsBow(this)) {
                    firstPersonWeaponIndex = 0;
                } else if (Player_HoldsSlingshot(this)) {
                    firstPersonWeaponIndex = 1;
                }
            }
            *dList = Player_HoldsHookshot(this) ? gLinkAdultRightHandHoldingHookshotFarDL
                                                : sFirstPersonRightHandHoldingWeaponDLs[firstPersonWeaponIndex];
            // Twilight Upgrade — Clawshot mode: same OOT-hand + MM-body
            // compound DL as the gameplay-view path above. Falls through to
            // vanilla when mm.o2r isn't loaded.
            if (Player_HoldsHookshot(this)) {
                extern u8 TwilightUpgrade_IsClawshotActive(void);
                extern void* MmAssets_LoadHookshotBodyDL(void);
                if (TwilightUpgrade_IsClawshotActive()) {
                    void* mmBody = MmAssets_LoadHookshotBodyDL();
                    if (mmBody != NULL) {
                        static Gfx sClawshotHandBodyFP[3];
                        static void* sLastOotHandFP = NULL;
                        static void* sLastMmBodyFP = NULL;
                        // First-person uses the FAR LOD hand to match the
                        // FAR LOD held-hookshot it would otherwise pick.
                        void* ootHand = gLinkAdultRightHandClosedFarDL;
                        if (sLastOotHandFP != ootHand || sLastMmBodyFP != mmBody) {
                            Gfx* dl = sClawshotHandBodyFP;
                            gSPDisplayList(dl++, ootHand);
                            gSPDisplayList(dl++, mmBody);
                            gSPEndDisplayList(dl);
                            sLastOotHandFP = ootHand;
                            sLastMmBodyFP = mmBody;
                        }
                        *dList = sClawshotHandBodyFP;
                    }
                }
            }
        } else {
            *dList = NULL;
        }
    }

    GameInteractor_Should(VB_PLAYER_OVERRIDE_LIMB_DRAW, true, limbIndex, dList, thisx, play);

    return false;
}

s32 Player_OverrideLimbDrawGameplayCrawling(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                            void* thisx) {
    if (!Player_OverrideLimbDrawGameplayCommon(play, limbIndex, dList, pos, rot, thisx)) {
        *dList = NULL;
    }

    return false;
}

u8 func_80090480(PlayState* play, ColliderQuad* collider, WeaponInfo* weaponInfo, Vec3f* newTip, Vec3f* newBase) {
    if (weaponInfo->active == 0) {
        if (collider != NULL) {
            Collider_ResetQuadAT(play, &collider->base);
        }
        Math_Vec3f_Copy(&weaponInfo->tip, newTip);
        Math_Vec3f_Copy(&weaponInfo->base, newBase);
        weaponInfo->active = 1;
        return 1;
    } else if ((weaponInfo->tip.x == newTip->x) && (weaponInfo->tip.y == newTip->y) &&
               (weaponInfo->tip.z == newTip->z) && (weaponInfo->base.x == newBase->x) &&
               (weaponInfo->base.y == newBase->y) && (weaponInfo->base.z == newBase->z)) {
        if (collider != NULL) {
            Collider_ResetQuadAT(play, &collider->base);
        }
        return 0;
    } else {
        if (collider != NULL) {
            Collider_SetQuadVertices(collider, newBase, newTip, &weaponInfo->base, &weaponInfo->tip);
            CollisionCheck_SetAT(play, &play->colChkCtx, &collider->base);
        }
        Math_Vec3f_Copy(&weaponInfo->base, newBase);
        Math_Vec3f_Copy(&weaponInfo->tip, newTip);
        weaponInfo->active = 1;
        return 1;
    }
}

void Player_UpdateShieldCollider(PlayState* play, Player* this, ColliderQuad* collider, Vec3f* quadSrc) {
    static u8 shieldColTypes[PLAYER_SHIELD_MAX] = {
        COLTYPE_METAL,
        COLTYPE_WOOD,
        COLTYPE_METAL,
        COLTYPE_METAL,
    };

    // Kafei guards passively while standing still (SW97's standalone shield), so the
    // quad has to go live without PLAYER_STATE1_SHIELDING ever being set.
    if ((this->stateFlags1 & PLAYER_STATE1_SHIELDING) || KafeiForm_ShieldIsPassive(this)) {
        Vec3f quadDest[4];

        this->shieldQuad.base.colType = shieldColTypes[this->currentShield];

        // Ext shields borrow a vanilla slot for the model, so the slot's collision is not theirs.
        if (ExtEquip_ShieldIsWooden()) {
            this->shieldQuad.base.colType = COLTYPE_WOOD;
        }

        Matrix_MultVec3f(&quadSrc[0], &quadDest[0]);
        Matrix_MultVec3f(&quadSrc[1], &quadDest[1]);
        Matrix_MultVec3f(&quadSrc[2], &quadDest[2]);
        Matrix_MultVec3f(&quadSrc[3], &quadDest[3]);
        Collider_SetQuadVertices(collider, &quadDest[0], &quadDest[1], &quadDest[2], &quadDest[3]);

        CollisionCheck_SetAC(play, &play->colChkCtx, &collider->base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &collider->base);
    }
}

Vec3f D_80126080 = { 5000.0f, 400.0f, 0.0f };
Vec3f D_8012608C = { 5000.0f, -400.0f, 1000.0f };
Vec3f D_80126098 = { 5000.0f, 1400.0f, -1000.0f };

Vec3f D_801260A4[3] = {
    { 0.0f, 400.0f, 0.0f },
    { 0.0f, 1400.0f, -1000.0f },
    { 0.0f, -400.0f, 1000.0f },
};

// ── Net: catch at the blade instead of dealing damage (Skijer's NEI) ─────────
// The Net uses the Master Sword IA (heldItemId == ITEM_NET), so it swings 1:1 like a sword. But
// instead of the damage quads, we scan for a catchable actor near the blade (meleeWeaponInfo[0]
// tip/base) and scoop it into an empty bottle — one catch per swing.
static u8 Net_ContentForActor(Actor* actor) {
    switch (actor->id) {
        case ACTOR_EN_ELF: // only the small catchable healing fairies (FAIRY_HEAL_TIMED=2, FAIRY_HEAL=6)
            return (actor->params == 2 || actor->params == 6) ? ITEM_FAIRY : ITEM_NONE;
        case ACTOR_EN_FISH:
            return ITEM_FISH;
        case ACTOR_EN_INSECT:
            return ITEM_BUG;
        case ACTOR_EN_ICE_HONO:
            return ITEM_BLUE_FIRE;
        default:
            return ITEM_NONE;
    }
}

static u8 sNetCaughtThisSwing = 0; // one bottle catch per swing; reset between swings

// Min distance from an actor to the net's catch samples (the whole DL, grip -> hoop; see gNetCatchPts).
static f32 Net_DistToCatchVolume(Actor* actor) {
    f32 best = 99999.0f;
    for (s32 i = 0; i < NET_CATCH_PTS; i++) {
        f32 dx = actor->world.pos.x - gNetCatchPts[i].x;
        f32 dy = actor->world.pos.y - gNetCatchPts[i].y;
        f32 dz = actor->world.pos.z - gNetCatchPts[i].z;
        f32 d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d < best) {
            best = d;
        }
    }
    return best;
}

// Butterfly (En_Butte) -> fairy transform forced by the net (see z_en_butte.c). Replaces the vanilla
// held-deku-stick dance; with fairy shuffle on, VB_SPAWN_BUTTERFLY_FAIRY gives the rando check instead.
void EnButte_NetForceTransform(Actor* actor);

static void Net_CaptureAtBlade(PlayState* play, Player* this) {
    if (!gNetCatchPtsValid) {
        return; // net model/DL not drawn yet — no volume to test
    }
    const f32 catchRadius = 30.0f; // fixed (the gNetCatch.Radius dev slider was removed). Skijer's NEI

    Actor* best = NULL;
    u8 bestContent = ITEM_NONE;
    f32 bestDist = catchRadius;
    for (s32 cat = 0; cat < ACTORCAT_MAX; cat++) {
        Actor* actor = play->actorCtx.actorLists[cat].head;
        while (actor != NULL) {
            // Butterflies: crossing the net forces the fairy transform (no deku stick needed) — the
            // rando fairy-shuffle check or the vanilla fairy comes out of the vanilla transform path.
            // Independent of the bottle catch (doesn't consume a bottle or the swing).
            if (actor->id == ACTOR_EN_BUTTE) {
                if (Net_DistToCatchVolume(actor) < catchRadius) {
                    EnButte_NetForceTransform(actor);
                }
            } else {
                u8 content = Net_ContentForActor(actor);
                if (content != ITEM_NONE) {
                    f32 d = Net_DistToCatchVolume(actor);
                    if (d < bestDist) {
                        bestDist = d;
                        best = actor;
                        bestContent = content;
                    }
                }
            }
            actor = actor->next;
        }
    }

    if (sNetCaughtThisSwing || best == NULL) {
        return;
    }

    u8 placed = Bottle_CatchIntoEmpty(bestContent);
    if (!placed) {
        // Fallback: no wheel/bottomless space — fill the first empty VANILLA bottle slot (covers pure
        // vanilla saves where the bottle system isn't driven by NEI bottleSlots), refreshing any
        // C-button that shows it.
        for (s32 bs = SLOT_BOTTLE_1; bs <= SLOT_BOTTLE_4; bs++) {
            if (gSaveContext.inventory.items[bs] == ITEM_BOTTLE) {
                gSaveContext.inventory.items[bs] = bestContent;
                for (s16 j = 1; j < 4; j++) {
                    if (gSaveContext.equips.cButtonSlots[j - 1] == bs) {
                        gSaveContext.equips.buttonItems[j] = bestContent;
                        Interface_LoadItemIcon1(play, j);
                    }
                }
                placed = true;
                break;
            }
        }
    }
    if (placed) {
        Audio_PlayFanfare(NA_BGM_ITEM_GET | 0x900);
        Actor_Kill(best);
        sNetCaughtThisSwing = 1;
    }
}

void func_800906D4(PlayState* play, Player* this, Vec3f* newTipPos) {
    Vec3f newBasePos[3];

    Matrix_MultVec3f(&D_801260A4[0], &newBasePos[0]);
    Matrix_MultVec3f(&D_801260A4[1], &newBasePos[1]);
    Matrix_MultVec3f(&D_801260A4[2], &newBasePos[2]);

    // func_80090480 always runs (it updates meleeWeaponInfo[0].tip used by the Net catch); the sword
    // trail is skipped for the Net (it's a net, not a glowing blade).
    if (func_80090480(play, NULL, &this->meleeWeaponInfo[0], &newTipPos[0], &newBasePos[0]) &&
        !(this->stateFlags1 & PLAYER_STATE1_SHIELDING) && (this->heldItemId != ITEM_NET) &&
        !CVarGetInteger(CVAR_ENHANCEMENT("DisableLinkSwordTrail"), 0)) {
        EffectBlure_AddVertex(Effect_GetByIndex(this->meleeWeaponEffectIndex), &this->meleeWeaponInfo[0].tip,
                              &this->meleeWeaponInfo[0].base);
    }

    if ((this->meleeWeaponState > 0) &&
        ((this->meleeWeaponAnimation < 0x18) || (this->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING))) {
        if (this->heldItemId == ITEM_NET) {
            Net_CaptureAtBlade(play, this); // capture at the blade — NO damage colliders
        } else {
            func_80090480(play, &this->meleeWeaponQuads[0], &this->meleeWeaponInfo[1], &newTipPos[1], &newBasePos[1]);
            func_80090480(play, &this->meleeWeaponQuads[1], &this->meleeWeaponInfo[2], &newTipPos[2], &newBasePos[2]);
        }
    } else if (this->heldItemId == ITEM_NET) {
        sNetCaughtThisSwing = 0; // between swings — allow the next swing to catch again
    }
}

void Player_DrawGetItemIceTrap(PlayState* play, Player* this, Vec3f* refPos, s32 drawIdPlusOne, f32 height) {
    OPEN_DISPS(play->state.gfxCtx);

    if (CVarGetInteger(CVAR_GENERAL("LetItSnow"), 0)) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);

        Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);

        gDPSetGrayscaleColor(POLY_OPA_DISP++, 75, 75, 75, 255);
        gSPGrayscale(POLY_OPA_DISP++, true);

        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gSilverRockDL);

        gSPGrayscale(POLY_OPA_DISP++, false);
    } else {
        if (iceTrapScale < 0.01) {
            iceTrapScale += 0.001f;
        } else if (iceTrapScale < 0.8f) {
            iceTrapScale += 0.2f;
        }

        // Draw the ice only after a bit so it doesn't spoil the fact that it's a trap
        if (iceTrapScale >= 0.01) {
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, (0 - play->gameplayFrames) % 128, 32, 32, 1, 0,
                                          (play->gameplayFrames * -2) % 128, 32, 32, 0, -1, 0, -2));

            Matrix_Translate(0.0f, -40.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(iceTrapScale, iceTrapScale, iceTrapScale, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gDPSetEnvColor(POLY_XLU_DISP++, 0, 50, 100, 255);
            gSPDisplayList(POLY_XLU_DISP++, gEffIceFragment3DL);

            // Reset matrix for the fake item model because we're animating the size of the ice block around it before
            // this.
            Matrix_Translate(refPos->x + (3.3f * Math_SinS(this->actor.shape.rot.y)), refPos->y + height,
                             refPos->z + ((3.3f + (IREG(90) / 10.0f)) * Math_CosS(this->actor.shape.rot.y)),
                             MTXMODE_NEW);
            Matrix_RotateZYX(0, play->gameplayFrames * 1000, 0, MTXMODE_APPLY);
            Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);
        }

        // Draw fake item model.
        if (this->getItemEntry.drawFunc != NULL) {
            this->getItemEntry.drawFunc(play, &this->getItemEntry);
        } else {
            GetItem_Draw(play, drawIdPlusOne - 1);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void Player_DrawGetItemImpl(PlayState* play, Player* this, Vec3f* refPos, s32 drawIdPlusOne) {
    f32 height = (this->exchangeItemId != EXCH_ITEM_NONE) ? 6.0f : 14.0f;

    OPEN_DISPS(play->state.gfxCtx);

    gSegments[6] = VIRTUAL_TO_PHYSICAL(this->giObjectSegment);

    gSPSegment(POLY_OPA_DISP++, 0x06, this->giObjectSegment);
    gSPSegment(POLY_XLU_DISP++, 0x06, this->giObjectSegment);

    Matrix_Translate(refPos->x + (3.3f * Math_SinS(this->actor.shape.rot.y)), refPos->y + height,
                     refPos->z + ((3.3f + (IREG(90) / 10.0f)) * Math_CosS(this->actor.shape.rot.y)), MTXMODE_NEW);
    Matrix_RotateZYX(0, play->gameplayFrames * 1000, 0, MTXMODE_APPLY);
    Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);

    if (this->getItemEntry.modIndex == MOD_RANDOMIZER && this->getItemEntry.getItemId == RG_ICE_TRAP) {
        Player_DrawGetItemIceTrap(play, this, refPos, drawIdPlusOne, height);
    } else if (this->getItemEntry.modIndex == MOD_RANDOMIZER &&
               (this->getItemEntry.getItemId == RG_TRIFORCE_PIECE || this->getItemEntry.getItemId == RG_TRIFORCE)) {
        Randomizer_DrawTriforcePieceGI(play, this->getItemEntry);
    } else if (this->getItemEntry.drawFunc != NULL) {
        this->getItemEntry.drawFunc(play, &this->getItemEntry);
    } else {
        GetItem_Draw(play, drawIdPlusOne - 1);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void Player_DrawGetItem(PlayState* play, Player* this) {
    // if (!this->giObjectLoading || !osRecvMesg(&this->giObjectLoadQueue, NULL, OS_MESG_NOBLOCK)) // OTRTODO: Do
    // something about osRecvMesg here...
    {
        this->giObjectLoading = false;
        Player_DrawGetItemImpl(play, this, &sGetItemRefPos, ABS(this->unk_862));
    }
}

void func_80090A28(Player* this, Vec3f* vecs) {
    D_8012608C.x = D_80126080.x;

    if (this->unk_845 >= 3) {
        this->unk_845 += 1;
        D_8012608C.x *= 1.0f + ((9 - this->unk_845) * 0.1f);
    }

    D_8012608C.x += 1200.0f;
    D_80126098.x = D_8012608C.x;

    Matrix_MultVec3f(&D_80126080, &vecs[0]);
    Matrix_MultVec3f(&D_8012608C, &vecs[1]);
    Matrix_MultVec3f(&D_80126098, &vecs[2]);
}

// Wrapper for FD melee weapon collision quads. Called from MmForm_PostLimbDraw at PLAYER_LIMB_L_HAND.
// FD skin mode uses MmForm_PostLimbDraw instead of Player_PostLimbDrawGameplay, so the melee weapon
// quad code at line 1904-1922 never runs for FD. This function provides the same functionality.
void Player_FDMeleeWeaponPostLimb(PlayState* play, Player* this) {
    Vec3f tipPos[3];

    D_80126080.x = 5500.0f; // FD sword reach (from MM z_player_lib.c)
    // FD always uses BGS trail type (Player_GetMeleeWeaponHeld returns 3 for FD)
    EffectBlure_ChangeType(Effect_GetByIndex(this->meleeWeaponEffectIndex), TRAIL_TYPE_BIGGORON_SWORD);
    func_80090A28(this, tipPos);
    func_800906D4(play, this, tipPos);
}

void Player_DrawHookshotReticle(PlayState* play, Player* this, f32 hookshotRange) {
    static Vec3f D_801260C8 = { -500.0f, -100.0f, 0.0f };
    CollisionPoly* colPoly;
    s32 bgId;
    Vec3f hookshotStart;
    Vec3f hookshotEnd;
    Vec3f firstHit;
    Vec3f sp68;
    f32 sp64;

    D_801260C8.z = 0.0f;
    Matrix_MultVec3f(&D_801260C8, &hookshotStart);
    D_801260C8.z = hookshotRange;
    Matrix_MultVec3f(&D_801260C8, &hookshotEnd);

    if (BgCheck_AnyLineTest3(&play->colCtx, &hookshotStart, &hookshotEnd, &firstHit, &colPoly, 1, 1, 1, 1, &bgId)) {
        OPEN_DISPS(play->state.gfxCtx);

        OVERLAY_DISP = Gfx_SetupDL(OVERLAY_DISP, 0x07);

        SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &firstHit, &sp68, &sp64);

        const f32 sp60 = (sp64 < 200.0f) ? 0.08f : (sp64 / 200.0f) * 0.08f;

        Matrix_Translate(firstHit.x, firstHit.y, firstHit.z, MTXMODE_NEW);
        Matrix_Scale(sp60, sp60, sp60, MTXMODE_APPLY);

        gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        if (GameInteractor_Should(VB_TARGETABLE_HOOKSHOT_RETICLE, true, colPoly, bgId)) {
            gSPSegment(OVERLAY_DISP++, 0x06, play->objectCtx.status[this->actor.objBankIndex].segment);
            // Twilight Upgrade — Clawshot mode: swap reticle DL to MM's
            // gameplay_keep reticle. Falls through to OOT when mm.o2r isn't
            // loaded or the DL doesn't resolve.
            Gfx* reticleDL = gLinkAdultHookshotReticleDL;
            {
                extern u8 TwilightUpgrade_IsClawshotActive(void);
                extern void* MmAssets_LoadHookshotReticleDL(void);
                if (TwilightUpgrade_IsClawshotActive()) {
                    Gfx* mmReticle = (Gfx*)MmAssets_LoadHookshotReticleDL();
                    if (mmReticle != NULL) {
                        reticleDL = mmReticle;
                    }
                }
            }
            gSPDisplayList(OVERLAY_DISP++, reticleDL);
        }

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

Vec3f D_801260D4 = { 1100.0f, -700.0f, 0.0f };

f32 sMeleeWeaponLengths[] = {
    0.0f, 4000.0f, 3000.0f, 5500.0f, 0.0f, 2500.0f,
};

f32 sSwordTypes[] = {
    TRAIL_TYPE_REST,           TRAIL_TYPE_MASTER_SWORD, TRAIL_TYPE_KOKIRI_SWORD,
    TRAIL_TYPE_BIGGORON_SWORD, TRAIL_TYPE_REST,         TRAIL_TYPE_HAMMER,
};

Gfx* sBottleDLists[] = { gLinkAdultBottleDL, gLinkChildBottleDL };

Color_RGB8 sBottleColors[] = {
    { 255, 255, 255 }, { 80, 80, 255 },   { 255, 100, 255 }, { 0, 0, 255 }, { 255, 0, 255 },
    { 255, 0, 255 },   { 200, 200, 100 }, { 255, 0, 0 },     { 0, 0, 255 }, { 0, 255, 0 },
    { 255, 255, 255 }, { 255, 255, 255 }, { 80, 80, 255 },
};

Vec3f sLeftHandArrowVec3 = { 398.0f, 1419.0f, 244.0f };

BowStringData sBowStringData[] = {
    { gLinkAdultBowStringDL, { 0.0f, -360.4f, 0.0f } },        // bow
    { gLinkChildSlingshotStringDL, { 606.0f, 236.0f, 0.0f } }, // slingshot
};

Vec3f sRightHandLimbModelShieldQuadVertices[] = {
    { -4500.0f, -3000.0f, -600.0f },
    { 1500.0f, -3000.0f, -600.0f },
    { -4500.0f, 3000.0f, -600.0f },
    { 1500.0f, 3000.0f, -600.0f },
};

Vec3f D_80126184 = { 100.0f, 1500.0f, 0.0f };
Vec3f D_80126190 = { 100.0f, 1640.0f, 0.0f };

Vec3f sSheathLimbModelShieldQuadVertices[] = {
    { -3000.0f, -3000.0f, -900.0f },
    { 3000.0f, -3000.0f, -900.0f },
    { -3000.0f, 3000.0f, -900.0f },
    { 3000.0f, 3000.0f, -900.0f },
};

Vec3f sSheathLimbModelShieldOnBackPos = { 630.0f, 100.0f, -30.0f };
Vec3s sSheathLimbModelShieldOnBackZyxRot = { 0, 0, 0x7FFF };

Vec3f sLeftRightFootLimbModelFootPos[] = {
    { 200.0f, 300.0f, 0.0f },
    { 200.0f, 200.0f, 0.0f },
};

void Player_PostLimbDrawGameplay(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    Player* this = (Player*)thisx;

    if (*dList != NULL) {
        Matrix_MultVec3f(&sZeroVec, D_80160000);
    }

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        MtxF sp14C;
        Actor* hookedActor;

        Math_Vec3f_Copy(&this->leftHandPos, D_80160000);

        // Boss Remains: draw Odolwa's sword on the hand bone (the native sword was hidden to a
        // closed fist in Player_OverrideLimbDrawGameplayDefault, so *dList != NULL means a hand DL
        // — where a sword would be — is drawing). Self-guards on Odolwa-worn + sword-in-hand; own
        // push/pop + transform. Mirrors the MM 2ship L_HAND post-limb hook.
        if ((*dList != NULL) && (this->actor.scale.y >= 0.0f)) {
            BossRemains_DrawOdolwaSword(play, this);
        }

        if (this->itemAction == PLAYER_IA_DEKU_STICK || this->itemAction == PLAYER_IA_ROD_FIRE ||
            this->itemAction == PLAYER_IA_ROD_ICE || this->itemAction == PLAYER_IA_ROD_LIGHT) {
            Vec3f sp124[3];
            u8 isCustomRod = (this->itemAction == PLAYER_IA_ROD_FIRE || this->itemAction == PLAYER_IA_ROD_ICE ||
                              this->itemAction == PLAYER_IA_ROD_LIGHT);

            OPEN_DISPS(play->state.gfxCtx);

            if (this->actor.scale.y >= 0.0f) {
                D_80126080.x = this->unk_85C * 5000.0f;
                func_80090A28(this, sp124);
                if (this->meleeWeaponState != 0) {
                    EffectBlure_ChangeType(Effect_GetByIndex(this->meleeWeaponEffectIndex), TRAIL_TYPE_STICK);
                    func_800906D4(play, this, sp124);
                } else {
                    Math_Vec3f_Copy(&this->meleeWeaponInfo[0].tip, &sp124[0]);
                }
            }

            Matrix_Translate(-428.26f, 267.2f, -33.82f, MTXMODE_APPLY);
            Matrix_RotateZYX(-0x8000, 0, 0x4000, MTXMODE_APPLY);
            Matrix_Scale(1.0f, this->unk_85C, 1.0f, MTXMODE_APPLY);

            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            if (isCustomRod) {
                // Custom rod - don't draw Deku Stick here
                // Fire Rod is drawn in CustomItems_DrawFireRod following leftHandPos
            } else {
                // Normal Deku Stick
                gSPDisplayList(POLY_OPA_DISP++, gLinkChildLinkDekuStickDL);
            }

            CLOSE_DISPS(play->state.gfxCtx);
        } else if (ExtEquip_ShouldHideSwordDL() && (this->actor.scale.y >= 0.0f)) {
            // Cane of Byrna: draw blue cane using limb matrix (follows hand rotation exactly)
            OPEN_DISPS(play->state.gfxCtx);

            // Melee weapon trail/collision (same as normal sword)
            if (ExtEquip_TridentTrailBegin()) {
                // Trident: the trail and the quads are measured in the LANCE's frame,
                // so they follow the drawn weapon (and its Item Editor placement)
                // instead of the sword that is hidden. The tip is refreshed even
                // between swings so the charge ball can sit on the real lance tip.
                // Skijer's NEI
                Vec3f spE4_trident[3];
                D_80126080.x = ExtEquip_TridentTrailLength();
                if (this->meleeWeaponState != 0) {
                    EffectBlure_ChangeType(Effect_GetByIndex(this->meleeWeaponEffectIndex),
                                           sSwordTypes[Player_GetMeleeWeaponHeld(this)]);
                    func_80090A28(this, spE4_trident);
                    func_800906D4(play, this, spE4_trident);
                } else {
                    // Not func_80090A28 here: it also bumps unk_845 (the combo counter)
                    // as a side effect, which is only right mid-swing.
                    Matrix_MultVec3f(&D_80126080, &this->meleeWeaponInfo[0].tip);
                }
                Matrix_Pop();
            } else if (ExtEquip_ByrnaTrailBegin()) {
                // Same reason as the Trident above: the cane is drawn far from the
                // hidden sword, so the streak and the quads have to be measured in
                // the cane's frame or they trail empty air. Skijer's NEI
                Vec3f spE4_byrnaCane[3];
                D_80126080.x = ExtEquip_ByrnaTrailLength();
                if (this->meleeWeaponState != 0) {
                    EffectBlure_ChangeType(Effect_GetByIndex(this->meleeWeaponEffectIndex),
                                           sSwordTypes[Player_GetMeleeWeaponHeld(this)]);
                    func_80090A28(this, spE4_byrnaCane);
                    func_800906D4(play, this, spE4_byrnaCane);
                } else {
                    // func_80090A28 also bumps unk_845 (the combo counter), which is
                    // only right mid-swing.
                    Matrix_MultVec3f(&D_80126080, &this->meleeWeaponInfo[0].tip);
                }
                Matrix_Pop();
            } else if (this->meleeWeaponState != 0) {
                Vec3f spE4_byrna[3];
                D_80126080.x = sMeleeWeaponLengths[Player_GetMeleeWeaponHeld(this)];

                // Hammer upgrade (Iron Knuckle's Axe): double the hitbox reach
                if (WeaponUpgrade_HasHammerAxe()) {
                    D_80126080.x = 8000.0f; // 2x normal hammer reach (~4000)
                }

                EffectBlure_ChangeType(Effect_GetByIndex(this->meleeWeaponEffectIndex),
                                       sSwordTypes[Player_GetMeleeWeaponHeld(this)]);
                func_80090A28(this, spE4_byrna);
                func_800906D4(play, this, spE4_byrna);
            }

            // Draw Byrna cane model using current limb matrix
            Matrix_Push();
            ExtEquip_ApplySwordDLMatrix();

            Gfx_SetupDL_25Opa(play->state.gfxCtx);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            ExtEquip_DrawSwordDL(play);
            Matrix_Pop();

            CLOSE_DISPS(play->state.gfxCtx);
        } else if ((this->heldItemId == ITEM_NET) && (this->actor.scale.y >= 0.0f)) {
            // Net (Skijer's NEI): wields via the sword IA. Draw the net using THIS limb matrix (the
            // hand BONE) so it follows the hand's full rotation/roll 1:1 like the sword — a
            // forearm->hand reconstruction could not roll. Then run the sword weapon update so the
            // blade-capture works (func_800906D4 catches instead of dealing damage — gated inside).
            CustomItems_DrawNet(this, play); // uses the current (hand-bone) matrix; Push/Pop internally
            if (this->meleeWeaponState != 0) {
                Vec3f spNet[3];
                D_80126080.x = sMeleeWeaponLengths[Player_GetMeleeWeaponHeld(this)];
                func_80090A28(this, spNet);
                func_800906D4(play, this, spNet);
            }
        } else if ((this->actor.scale.y >= 0.0f) && (this->meleeWeaponState != 0)) {
            Vec3f spE4[3];

            if (TransformMasks_IsFDSkinMode()) {
                // Fierce Deity sword reach: 5500 units (from MM z_player_lib.c)
                // Player_GetMeleeWeaponHeld returns BGS index (3) for FD
                D_80126080.x = 5500.0f;
                EffectBlure_ChangeType(Effect_GetByIndex(this->meleeWeaponEffectIndex),
                                       sSwordTypes[Player_GetMeleeWeaponHeld(this)]);
            } else if (Player_HoldsBrokenKnife(this)) {
                D_80126080.x = 1500.0f;
            } else {
                D_80126080.x = sMeleeWeaponLengths[Player_GetMeleeWeaponHeld(this)];
                EffectBlure_ChangeType(Effect_GetByIndex(this->meleeWeaponEffectIndex),
                                       sSwordTypes[Player_GetMeleeWeaponHeld(this)]);
            }

            func_80090A28(this, spE4);
            func_800906D4(play, this, spE4);
        } else if ((*dList != NULL) && (this->leftHandType == PLAYER_MODELTYPE_LH_BOTTLE)) {
            Color_RGB8* bottleColor = &sBottleColors[Player_ActionToBottle(this, this->itemAction)];

            OPEN_DISPS(play->state.gfxCtx);

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            if (GameInteractor_Should(VB_PLAYER_DRAW_BOTTLE, true, this, play)) {
                gDPSetEnvColor(POLY_XLU_DISP++, bottleColor->r, bottleColor->g, bottleColor->b, 0);
                gSPDisplayList(POLY_XLU_DISP++, sBottleDLists[gSaveContext.linkAge]);
            }

            CLOSE_DISPS(play->state.gfxCtx);
        }

        if (this->actor.scale.y >= 0.0f) {
            if (!Player_HoldsHookshot(this) && ((hookedActor = this->heldActor) != NULL)) {
                if (this->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) {
                    Matrix_MultVec3f(&sLeftHandArrowVec3, &hookedActor->world.pos);
                    Matrix_RotateZYX(0x69E8, -0x5708, 0x458E, MTXMODE_APPLY);
                    Matrix_Get(&sp14C);
                    Matrix_MtxFToYXZRotS(&sp14C, &hookedActor->world.rot, 0);
                    hookedActor->shape.rot = hookedActor->world.rot;
                } else if (this->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
                    Vec3s spB8;

                    Matrix_Get(&sp14C);
                    Matrix_MtxFToYXZRotS(&sp14C, &spB8, 0);

                    if (hookedActor->flags & ACTOR_FLAG_CARRY_X_ROT_INFLUENCE) {
                        hookedActor->world.rot.x = hookedActor->shape.rot.x = spB8.x - this->unk_3BC.x;
                    } else {
                        hookedActor->world.rot.y = hookedActor->shape.rot.y = this->actor.shape.rot.y + this->unk_3BC.y;
                    }
                }
            } else {
                Matrix_Get(&this->mf_9E0);
                Matrix_MtxFToYXZRotS(&this->mf_9E0, &this->unk_3BC, 0);
            }
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        Actor* heldActor = this->heldActor;

        ItemEquip_CaptureHandMatrix();

        if (this->rightHandType == PLAYER_MODELTYPE_RH_FF) {
            Matrix_Get(&this->shieldMf);
        } else if ((this->rightHandType == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT) ||
                   (this->rightHandType == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2)) {
            s32 stringModelToUse = gSaveContext.linkAge;
            if (CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) ||
                CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) {
                stringModelToUse = Player_HoldsSlingshot(this);
            }
            BowStringData* stringData = &sBowStringData[stringModelToUse];

            OPEN_DISPS(play->state.gfxCtx);

            Matrix_Push();
            Matrix_Translate(stringData->pos.x, stringData->pos.y, stringData->pos.z, MTXMODE_APPLY);

            if ((this->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) && (this->unk_860 >= 0) && (this->unk_834 <= 10)) {
                Vec3f sp90;
                f32 distXYZ;

                Matrix_MultVec3f(&sZeroVec, &sp90);
                distXYZ = Math_Vec3f_DistXYZ(D_80160000, &sp90);

                this->unk_858 = distXYZ - 3.0f;
                if (distXYZ < 3.0f) {
                    this->unk_858 = 0.0f;
                } else {
                    this->unk_858 *= 1.6f;
                    if (this->unk_858 > 1.0f) {
                        this->unk_858 = 1.0f;
                    }
                }

                this->unk_85C = -0.5f;
            }

            Matrix_Scale(1.0f, this->unk_858, 1.0f, MTXMODE_APPLY);

            if (!LINK_IS_ADULT) {
                Matrix_RotateZ(this->unk_858 * -0.2f, MTXMODE_APPLY);
            }

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, stringData->dList);

            Matrix_Pop();

            CLOSE_DISPS(play->state.gfxCtx);
        } else if ((this->actor.scale.y >= 0.0f) && (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD)) {
            Matrix_Get(&this->shieldMf);
            Player_UpdateShieldCollider(play, this, &this->shieldQuad, sRightHandLimbModelShieldQuadVertices);

            // Gerudo: skip the shield DL — the dual scimitar at R_HAND was
            // already drawn by GerudoForm_GetSwordDL_R via OverrideLimbDraw,
            // and the player sees both swords held up as the "shield" visual
            // (arms-only kf_hanare_loop override). Mechanics still fire:
            // shieldMf is captured above and shieldQuad collider was just
            // activated, so Mirror Shield reflection / deflection / sword
            // sparks all work 1:1 vanilla. Only the model render is suppressed.
            if (!GerudoForm_IsActive()) {
                // Shield of Ikana: draw MM Mirror Shield from mm.o2r
                ExtEquip_DrawShieldDL(play);
                // Boss Remains: draw Odolwa's shield in the raised hand (the native shield was
                // swapped to an open hand in the override above). Self-guards on Odolwa-worn;
                // own push/pop + transform. Mirrors the MM 2ship R_HAND shield hook.
                BossRemains_DrawOdolwaShield(play, this);
            }
        }

        if (this->actor.scale.y >= 0.0f) {
            if (GameInteractor_Should(VB_DRAW_ADDITIONAL_RETICLES,
                                      (this->heldItemAction == PLAYER_IA_HOOKSHOT) ||
                                          (this->heldItemAction == PLAYER_IA_LONGSHOT),
                                      this)) {
                Matrix_MultVec3f(&D_80126184, &this->unk_3C8);

                if (heldActor != NULL) {
                    MtxF sp44;
                    s32 pad;

                    Matrix_MultVec3f(&D_80126190, &heldActor->world.pos);
                    Matrix_RotateZYX(0, -0x4000, -0x4000, MTXMODE_APPLY);
                    Matrix_Get(&sp44);
                    Matrix_MtxFToYXZRotS(&sp44, &heldActor->world.rot, 0);
                    heldActor->shape.rot = heldActor->world.rot;

                    if (func_8002DD78(this) != 0) {
                        // Skijer's NEI hookshot overhaul — Ultrashot: the Longshot reaches TWICE as
                        // far while the unlock is owned, so its reticle raycast must too or it
                        // vanishes over the far half of the range. No other change: same Longshot
                        // in-hand DL and reticle, just double distance.
                        extern u8 Nei_UltrashotOwned(void);
                        f32 reticleRange = (this->heldItemAction == PLAYER_IA_HOOKSHOT) ? 38600.0f : 77600.0f;

                        if ((this->heldItemAction == PLAYER_IA_LONGSHOT) && Nei_UltrashotOwned()) {
                            reticleRange *= 2.0f;
                        }
                        Matrix_Translate(500.0f, 300.0f, 0.0f, MTXMODE_APPLY);
                        Player_DrawHookshotReticle(
                            play, this, reticleRange * CVarGetFloat(CVAR_CHEAT("HookshotReachMultiplier"), 1.0f));
                    }
                }
            }

            if ((this->unk_862 != 0) || ((func_8002DD6C(this) == 0) && (heldActor != NULL))) {
                if (!(this->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) && (this->unk_862 != 0) &&
                    (this->exchangeItemId != EXCH_ITEM_NONE)) {
                    Math_Vec3f_Copy(&sGetItemRefPos, &this->leftHandPos);
                } else {
                    sGetItemRefPos.x = (this->bodyPartsPos[15].x + this->leftHandPos.x) * 0.5f;
                    sGetItemRefPos.y = (this->bodyPartsPos[15].y + this->leftHandPos.y) * 0.5f;
                    sGetItemRefPos.z = (this->bodyPartsPos[15].z + this->leftHandPos.z) * 0.5f;
                }

                if (this->unk_862 == 0) {
                    Math_Vec3f_Copy(&heldActor->world.pos, &sGetItemRefPos);
                }
            }
        }
    } else if (this->actor.scale.y >= 0.0f) {
        if (limbIndex == PLAYER_LIMB_SHEATH) {
            if ((this->rightHandType != PLAYER_MODELTYPE_RH_SHIELD) &&
                (this->rightHandType != PLAYER_MODELTYPE_RH_FF)) {
                if (Player_IsChildWithHylianShield(this)) {
                    Player_UpdateShieldCollider(play, this, &this->shieldQuad, sSheathLimbModelShieldQuadVertices);
                }

                Matrix_TranslateRotateZYX(&sSheathLimbModelShieldOnBackPos, &sSheathLimbModelShieldOnBackZyxRot);
                Matrix_Get(&this->shieldMf);

                // Shield of Ikana: draw MM Mirror Shield on back
                ExtEquip_DrawShieldBackDL(play);
            }

        } else if (limbIndex == PLAYER_LIMB_HEAD) {
            Matrix_MultVec3f(&D_801260D4, &this->actor.focus.pos);

            // Draw worn MM mask on Link's head (matrix is in head limb space)
            TransformMasks_WearDraw(play, this);

            // Boss Remains: draw the worn boss-remains mask on Link's face using the head-limb
            // matrix (current here, same one the mask draw above uses). No-op unless a remains is
            // worn. Mirrors the MM 2ship PLAYER_LIMB_HEAD hook.
            BossRemains_DrawWornMask(play, this);

        } else if (limbIndex == PLAYER_LIMB_ROOT) {
            // Kite Shield: the board under Link's feet while shield surfing. Self-guards on the
            // surf being active, and hides the hand/back shield for as long as it draws.
            ExtEquip_DrawKiteSurfBoard(play);
        } else if (limbIndex == PLAYER_LIMB_UPPER) {
            // Spirit Breastplate: draw Iron Knuckle armor on torso
            ExtEquip_DrawBreastplate(play);
        } else if (limbIndex == PLAYER_LIMB_L_SHOULDER || limbIndex == PLAYER_LIMB_R_SHOULDER) {
            // Magic Cape + Champion's Scarf: capture shoulder world positions
            ExtEquip_CaptureCapeShoulderPos(limbIndex);
        } else if (limbIndex == PLAYER_LIMB_L_FOOT || limbIndex == PLAYER_LIMB_R_FOOT) {
            Vec3f* vec = &sLeftRightFootLimbModelFootPos[(gSaveContext.linkAge)];

            Actor_SetFeetPos(&this->actor, limbIndex, PLAYER_LIMB_L_FOOT, vec, PLAYER_LIMB_R_FOOT, vec);

            // Pegasus Anklet no longer draws a custom per-foot model (torus + wings removed) — its
            // look is now the RED hover boots drawn with the body in Player_DrawImpl (Skijer 2026-07-15).
        }
    }
}

u32 func_80091738(PlayState* play, u8* segment, SkelAnime* skelAnime) {
    s16 linkObjectId = gLinkObjectIds[gSaveContext.linkAge];
    size_t size;
    void* ptr;

    size = gObjectTable[OBJECT_GAMEPLAY_KEEP].vromEnd - gObjectTable[OBJECT_GAMEPLAY_KEEP].vromStart;
    ptr = segment + 0x3800;
    DmaMgr_SendRequest1(ptr, gObjectTable[OBJECT_GAMEPLAY_KEEP].vromStart, size, __FILE__, __LINE__);

    size = gObjectTable[linkObjectId].vromEnd - gObjectTable[linkObjectId].vromStart;
    ptr = segment + 0x8800;
    DmaMgr_SendRequest1(ptr, gObjectTable[linkObjectId].vromStart, size, __FILE__, __LINE__);

    ptr = (void*)ALIGN16((intptr_t)ptr + size);

    gSegments[4] = VIRTUAL_TO_PHYSICAL(segment + 0x3800);
    gSegments[6] = VIRTUAL_TO_PHYSICAL(segment + 0x8800);

    SkelAnime_InitLink(play, skelAnime, gPlayerSkelHeaders[gSaveContext.linkAge], &gPlayerAnim_link_normal_wait, 9, ptr,
                       ptr, PLAYER_LIMB_MAX);

    return size + 0x8800 + 0x90;
}

u8 sPauseModelGroupBySword[] = {
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_SWORD_KOKIRI
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_SWORD_MASTER
    PLAYER_MODELGROUP_BGS,              // PLAYER_SWORD_BIGGORON
};

s32 Player_OverrideLimbDrawPause(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg) {
    u8* playerSwordAndShield = arg;
    // SOH: Ensure positive value from playerSwordAndShield[] to avoid OOB array access.
    //      This can occur in the case where playerSwordAndShield[0] is PLAYER_SWORD_NONE
    u8 modelGroup =
        sPauseModelGroupBySword[playerSwordAndShield[0] > 0 ? playerSwordAndShield[0] - PLAYER_SWORD_KOKIRI : 0];
    s32 type;
    s32 dListOffset = 0;
    Gfx** dLists;
    size_t ptrSize = sizeof(uint32_t);

    if ((modelGroup == PLAYER_MODELGROUP_SWORD_AND_SHIELD) && !LINK_IS_ADULT &&
        (playerSwordAndShield[1] == PLAYER_SHIELD_HYLIAN)) {
        modelGroup = PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD;
    }

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_LEFT_HAND];
        sLeftHandType = type;

        // SOH: Handle unexpected swordless case. Previously OOB array access is avoided, but we want the
        //      hand model-type to be set to open (otherwise it is set to holding sword model-type)
        if (playerSwordAndShield[0] == PLAYER_SWORD_NONE) {
            type = PLAYER_MODELTYPE_LH_OPEN;
        }

        if ((type == PLAYER_MODELTYPE_LH_BGS) && (gSaveContext.swordHealth <= 0.0f)) {
            dListOffset = 4;
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_RIGHT_HAND];
        sRightHandType = type;
        if (type == PLAYER_MODELTYPE_RH_SHIELD) {
            dListOffset = playerSwordAndShield[1] * ptrSize;
        }
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_SHEATH];
        if ((type == PLAYER_MODELTYPE_SHEATH_18) || (type == PLAYER_MODELTYPE_SHEATH_19)) {
            dListOffset = playerSwordAndShield[1] * ptrSize;
        }
    } else if (limbIndex == PLAYER_LIMB_WAIST) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_WAIST];

        if (Player_IsCustomLinkModel()) {
            return 0;
        }
    } else {
        return 0;
    }

    dLists = &sPlayerDListGroups[type][gSaveContext.linkAge];
    *dList = dLists[dListOffset];

    // Run CustomEquipment's hook FIRST (upstream #6708 had it last), so the pak
    // override below outranks any active equipment o2r mod in the kaleido preview
    // too. The pak block's else-branch leaves *dList untouched, so where there's
    // no per-slot override the hook's result (o2r) survives.
    GameInteractor_Should(VB_PLAYER_OVERRIDE_LIMB_DRAW_PAUSE, true, limbIndex, dList, GET_PLAYER(play), play);

    // PakLoader override for the pause-menu equipment subscreen draw. The
    // gameplay path (Player_OverrideLimbDrawGameplayDefault) already consults
    // PakLoader_GetEquipDL — without this mirror here, slot mixes and the main
    // Equipment Pack show up in-world but the kaleido preview keeps rendering
    // vanilla. The preview reflects the sword/shield selected in the equip
    // subscreen rather than the player's battle state, so we patch ONLY the
    // field GetEquipDL actually consults for this limb and restore after.
    if (PakLoader_HasActiveModel() && (limbIndex == PLAYER_LIMB_L_HAND || limbIndex == PLAYER_LIMB_R_HAND ||
                                       limbIndex == PLAYER_LIMB_SHEATH || limbIndex == PLAYER_LIMB_WAIST)) {
        Player* localPlayer = GET_PLAYER(play);
        s32 savedLeft = localPlayer->leftHandType;
        s32 savedRight = localPlayer->rightHandType;
        s32 savedSheath = localPlayer->sheathType;
        s32 savedShield = localPlayer->currentShield;
        if (limbIndex == PLAYER_LIMB_L_HAND) {
            localPlayer->leftHandType = type;
        } else if (limbIndex == PLAYER_LIMB_R_HAND) {
            localPlayer->rightHandType = type;
            localPlayer->currentShield = playerSwordAndShield[1];
        } else if (limbIndex == PLAYER_LIMB_SHEATH) {
            localPlayer->sheathType = type;
            localPlayer->currentShield = playerSwordAndShield[1];
        }

        Gfx* pakDL = PakLoader_GetEquipDL(localPlayer, limbIndex);

        localPlayer->leftHandType = savedLeft;
        localPlayer->rightHandType = savedRight;
        localPlayer->sheathType = savedSheath;
        localPlayer->currentShield = savedShield;

        if (pakDL == PAK_DL_STUB) {
            *dList = NULL;
        } else if (pakDL != NULL) {
            *dList = pakDL;
        }
        // pakDL == NULL → keep the o2r/vanilla *dList from the hook above
    }

    return 0;
}

#include <overlays/actors/ovl_Demo_Effect/z_demo_effect.h>
void DemoEffect_DrawTriforceSpot(Actor* thisx, PlayState* play);

void Pause_DrawTriforceSpot(PlayState* play, s32 showLightColumn) {
    static DemoEffect triforce;
    static s16 rotation = 0;

    triforce.triforceSpot.crystalLightOpacity = 244;
    triforce.triforceSpot.triforceSpotOpacity = 249;
    triforce.triforceSpot.lightColumnOpacity = showLightColumn ? 244 : 0;
    triforce.triforceSpot.rotation = rotation;

    DemoEffect_DrawTriforceSpot(&triforce, play);

    rotation += 0x03E8;
}

void Player_DrawPauseImpl(PlayState* play, void* gameplayKeep, void* linkObject, SkelAnime* skelAnime, Vec3f* pos,
                          Vec3s* rot, f32 scale, s32 sword, s32 tunic, s32 shield, s32 boots, s32 width, s32 height,
                          Vec3f* eye, Vec3f* at, f32 fovy, void* colorFrameBuffer, void* depthFrameBuffer) {
    // Note: the viewport x and y values are overwritten below, before usage
    static Vp viewport = { (PAUSE_EQUIP_PLAYER_WIDTH / 2) << 2, (PAUSE_EQUIP_PLAYER_HEIGHT / 2) << 2, G_MAXZ / 2, 0,
                           (PAUSE_EQUIP_PLAYER_WIDTH / 2) << 2, (PAUSE_EQUIP_PLAYER_HEIGHT / 2) << 2, G_MAXZ / 2, 0 };
    static Lights1 lights1 = gdSPDefLights1(80, 80, 80, 255, 255, 255, 84, 84, -84);
    static Vec3f lightDir = { 89.8f, 0.0f, 89.8f };
    u8 playerSwordAndShield[2];
    Gfx* opaRef;
    Gfx* xluRef;
    u16 perspNorm;
    Mtx* perspMtx = Graph_Alloc(play->state.gfxCtx, sizeof(Mtx));
    Mtx* lookAtMtx = Graph_Alloc(play->state.gfxCtx, sizeof(Mtx));

    u8 mirrorWorldActive = CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0);

    OPEN_DISPS(play->state.gfxCtx);

    opaRef = POLY_OPA_DISP;
    POLY_OPA_DISP++;

    xluRef = POLY_XLU_DISP;
    POLY_XLU_DISP++;

    gSPDisplayList(WORK_DISP++, POLY_OPA_DISP);
    gSPDisplayList(WORK_DISP++, POLY_XLU_DISP);

    if (mirrorWorldActive) {
        gSPSetExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
        gSPSetExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
    }

    gSPSegment(POLY_OPA_DISP++, 0x00, NULL);

    gDPPipeSync(POLY_OPA_DISP++);

    gSPLoadGeometryMode(POLY_OPA_DISP++, 0);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_SHADE, G_CC_SHADE);
    gDPSetOtherMode(POLY_OPA_DISP++,
                    G_AD_DISABLE | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_PERSP | G_CYC_FILL | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_NOOP2);

    // Also matches if some of the previous graphics commands are moved inside this block too. Possible macro?
    if (1) {
        s32 pad[2];

        gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    }

    gDPSetScissor(POLY_OPA_DISP++, G_SC_NON_INTERLACE, 0, 0, width, height);
    gSPClipRatio(POLY_OPA_DISP++, FRUSTRATIO_1);

    gDPSetColorImage(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, depthFrameBuffer);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_FILL);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(POLY_OPA_DISP++, (GPACK_ZDZ(G_MAXFBZ, 0) << 16) | GPACK_ZDZ(G_MAXFBZ, 0));
    gDPFillRectangle(POLY_OPA_DISP++, 0, 0, width - 1, height - 1);

    gDPPipeSync(POLY_OPA_DISP++);

    gDPSetColorImage(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, colorFrameBuffer);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_FILL);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(POLY_OPA_DISP++, (GPACK_RGBA5551(0, 0, 0, 1) << 16) | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillRectangle(POLY_OPA_DISP++, 0, 0, width - 1, height - 1);

    gDPPipeSync(POLY_OPA_DISP++);

    gDPSetDepthImage(POLY_OPA_DISP++, depthFrameBuffer);

    viewport.vp.vscale[0] = viewport.vp.vtrans[0] = width * ((1 << 2) / 2);
    viewport.vp.vscale[1] = viewport.vp.vtrans[1] = height * ((1 << 2) / 2);
    gSPViewport(POLY_OPA_DISP++, &viewport);

    guPerspective(perspMtx, &perspNorm, fovy, (f32)width / (f32)height, 10.0f, 4000.0f, 1.0f);

    gSPPerspNormalize(POLY_OPA_DISP++, perspNorm);
    gSPMatrix(POLY_OPA_DISP++, perspMtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    guLookAt(lookAtMtx, eye->x, eye->y, eye->z, at->x, at->y, at->z, 0.0f, 1.0f, 0.0f);

    gSPMatrix(POLY_OPA_DISP++, lookAtMtx, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);

    playerSwordAndShield[0] = sword;
    playerSwordAndShield[1] = shield;

    Matrix_SetTranslateRotateYXZ(
        pos->x - ((CVarGetInteger(CVAR_ENHANCEMENT("PauseMenuAnimatedLink"), 0) && LINK_AGE_IN_YEARS == YEARS_ADULT)
                      ? 25
                      : 0),
        pos->y - (CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0) ? 16 : 0), pos->z, rot);
    Matrix_Scale(scale * (mirrorWorldActive ? -1 : 1), scale, scale, MTXMODE_APPLY);

    gSPSegment(POLY_OPA_DISP++, 0x04, gameplayKeep);
    gSPSegment(POLY_OPA_DISP++, 0x06, linkObject);

    gSPSetLights1(POLY_OPA_DISP++, lights1);

    func_80093C80(play);

    POLY_OPA_DISP = Gfx_SetFog2(POLY_OPA_DISP++, 0, 0, 0, 0, 997, 1000);

    func_8002EABC(pos, &play->view.eye, &lightDir, play->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x0C, gCullBackDList);

    // PAK Loader: swap pause screen skeleton with custom model
    void* pauseSkelBackup = skelAnime->skeleton;
    s32 pauseDListCountBackup = skelAnime->dListCount;
    if (PakLoader_HasActiveModel()) {
        PakLoader_SwapSkeleton(GET_PLAYER(play));
        // Copy the swapped skeleton to the pause skelAnime
        Player* player = GET_PLAYER(play);
        skelAnime->skeleton = player->skelAnime.skeleton;
        skelAnime->dListCount = player->skelAnime.dListCount;
        PakLoader_RestoreSkeleton(player);
    }

    Player_DrawImpl(play, skelAnime->skeleton, skelAnime->jointTable, skelAnime->dListCount, 0, tunic, boots, 0,
                    Player_OverrideLimbDrawPause, NULL, &playerSwordAndShield);

    // Restore pause skeleton
    skelAnime->skeleton = pauseSkelBackup;
    skelAnime->dListCount = pauseDListCountBackup;

    if (CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
        Matrix_SetTranslateRotateYXZ(pos->x - (LINK_AGE_IN_YEARS == YEARS_ADULT ? 25 : 0),
                                     pos->y + 280 + (LINK_AGE_IN_YEARS == YEARS_ADULT ? 48 : 0), pos->z, rot);
        Matrix_Scale(scale * (mirrorWorldActive ? -1 : 1), scale * 1, scale * 1, MTXMODE_APPLY);

        Pause_DrawTriforceSpot(play, 1);
    }

    if (mirrorWorldActive) {
        gSPClearExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
        gSPClearExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
    }

    gSPEndDisplayList(POLY_OPA_DISP++);
    gSPEndDisplayList(POLY_XLU_DISP++);

    gSPBranchList(opaRef, POLY_OPA_DISP);
    gSPBranchList(xluRef, POLY_XLU_DISP);

    CLOSE_DISPS(play->state.gfxCtx);
}

void Player_DrawPause(PlayState* play, u8* segment, SkelAnime* skelAnime, Vec3f* pos, Vec3s* rot, f32 scale, s32 sword,
                      s32 tunic, s32 shield, s32 boots) {
    Input* p1Input = &play->state.input[0];
    Vec3f eye = { 0.0f, 0.0f, -400.0f };
    Vec3f at = { 0.0f, 0.0f, 0.0f };
    Vec3s* destTable;
    Vec3s* srcTable;
    s32 i;
    bool canswitchrnd = false;

    gSegments[4] = VIRTUAL_TO_PHYSICAL(segment + 0x3800);
    gSegments[6] = VIRTUAL_TO_PHYSICAL(segment + 0x8800);

    uintptr_t* PauseMenuAnimSet[4] = { // IDLE                       // Two Handed                       // No shield //
                                       // Kid Hylian Shield
                                       gPlayerAnim_link_normal_wait, gPlayerAnim_link_fighter_wait_long,
                                       gPlayerAnim_link_normal_wait_free, gPlayerAnim_link_normal_wait_free
    };

    if (CVarGetInteger(CVAR_ENHANCEMENT("PauseMenuAnimatedLink"), 0) ||
        CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
        uintptr_t anim = 0; // Initialise anim

        s16 EquipedStance;
        if (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) >= EQUIP_VALUE_SWORD_BIGGORON) {
            EquipedStance = 1;
        } else if (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SWORD_NONE) {
            EquipedStance = 2;
        } else if (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SWORD_MASTER && LINK_AGE_IN_YEARS == YEARS_CHILD) {
            EquipedStance = 3;
        } else {
            // Link is idle so revert to 0
            EquipedStance = 0;
        }

        if (!CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
            anim = PauseMenuAnimSet[EquipedStance];
        } else {
            anim = gPlayerAnim_link_magic_kaze2;
            sword = 0;
            shield = 0;
        }

        if (skelAnime->animation != anim) {
            LinkAnimation_Change(play, skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), ANIMMODE_LOOP, -6.0f);
        }

        LinkAnimation_Update(play, skelAnime);

        if (!LINK_IS_ADULT) {
            // Link is placed too far up by default when animating
            at.y += 60;
        }
    } else {

        if (!LINK_IS_ADULT) {
            if (shield == PLAYER_SHIELD_DEKU) {
                srcTable = gLinkPauseChildDekuShieldJointTable;
            } else {
                srcTable = gLinkPauseChildJointTable;
            }
        } else {
            if (sword == PLAYER_SWORD_BIGGORON) {
                srcTable = gLinkPauseAdultBgsJointTable;
            } else if (shield != PLAYER_SHIELD_NONE) {
                srcTable = gLinkPauseAdultShieldJointTable;
            } else {
                srcTable = gLinkPauseAdultJointTable;
            }
        }

        srcTable = ResourceMgr_LoadArrayByNameAsVec3s(srcTable);
        Vec3s* ogSrcTable = srcTable;
        destTable = skelAnime->jointTable;
        for (i = 0; i < skelAnime->limbCount; i++) {
            *destTable++ = *srcTable++;
        }
        free(ogSrcTable);
    }

    Player_DrawPauseImpl(play, segment + 0x3800, segment + 0x8800, skelAnime, pos, rot, scale, sword, tunic, shield,
                         boots, PAUSE_EQUIP_PLAYER_WIDTH, PAUSE_EQUIP_PLAYER_HEIGHT, &eye, &at, 60.0f,
                         play->state.gfxCtx->curFrameBuffer,
                         play->state.gfxCtx->curFrameBuffer + (PAUSE_EQUIP_PLAYER_WIDTH * PAUSE_EQUIP_PLAYER_HEIGHT));
}
