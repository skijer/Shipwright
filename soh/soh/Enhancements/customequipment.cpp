#include <initializer_list>
#include <libultraship/bridge/resourcebridge.h>
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
#include "objects/object_custom_equip/object_custom_equip.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/vanilla-behavior/PlayerAnimOverride.h"
#include "soh/ShipInit.hpp"
#include "soh/ResourceManagerHelpers.h"
// Skijer's NEI: needed so OPEN_DISPS/CLOSE_DISPS in the draw handler get C linkage (else LNK2001)
#include "soh/frame_interpolation.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "mods/transformation_masks/transformation_masks.h"
// Skijer's NEI: draw-fork subsystems
#include "mods/pak_loader/pak_loader.h" // PakLoader_FrameBegin
#include "expansions/sm64/sm64_mario.h" // Sm64Mario_HasMesh/Draw/ShouldHideLink
#include "mods/items/custom_items.h"    // CustomItems_OverrideDraw
#include "mods/extended_equipment.h"    // ExtEquip_DrawBehavior
extern SaveContext gSaveContext;

// Harpoon Prop Hunt local-prop draw intercept. Forward-declared (matching the
// inline `extern` z_player.c previously used) to avoid pulling the Harpoon C++
// headers into this TU. Returns 1 when it rendered a prop (suppress Link), else 0.
s32 HarpoonPropHunt_TryDrawLocalProp(Actor* thisx, PlayState* play);

// Skijer's NEI: Pikachu status intercept (mm_player_form.cpp / pikachu_form.cpp).
// Forward-declared to match the inline `extern` z_player.c previously used.
u8 MmForm_IsPikachuActive(void);
void PikachuForm_InterceptStatus(PlayState* play, Player* player);

// SW97 Cucco mode draw — Soul-arrow + cucco transformation. When active,
// replaces Link's body with the cucco model while still walking Link's
// skeleton (null limbs) so shadow + Navi keep tracking.
s32 Sw97_IsCuccoModeActive(void);
void Sw97_DrawCuccoForm(PlayState* play, Player* player);

// Cucco-egg projectile hooks: while cucco is active, any bow/slingshot
// arrow Link fires gets tagged so its draw becomes the pocket-egg model
// and its horizontal speed is clamped to CUCCO_EGG_SPEED_MAX.
void Sw97_TagCuccoEgg(Actor* arrow);
void Sw97_TickCuccoEggClamp(Actor* arrow);

// Cucco shield / aim state, needed by the input strip and the VB guards below.
s32 Sw97_CuccoShieldIsUp(void);
s32 Sw97_CuccoEggAimActive(void);
// 0 = soul-arrow form (30s, no items), 1 = CVar form (persistent, items OK).
extern s32 gSw97CuccoModeSource;
void Sw97_EndCuccoMode(void);
}

static const char* ResolveCustomChain(std::initializer_list<const char*> paths) {
    const char* fallback = nullptr;
    for (auto path : paths) {
        if (path == nullptr)
            continue;
        fallback = path;
        if (ResourceGetIsCustomByName(path) || ResourceMgr_FileAltExists(path))
            return path;
    }
    return fallback;
}

static const char* ResolveCustomFPSHand(const char* path) {
    const bool isAdult = path == gCustomAdultFPSHandDL;
    const bool isChild = path == gCustomChildFPSHandDL;

    if (!isAdult && !isChild) {
        return path;
    }

    switch (TUNIC_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC))) {
        case PLAYER_TUNIC_GORON:
            return ResolveCustomChain(
                { isAdult ? gCustomAdultGoronFPSHandDL : gCustomChildGoronFPSHandDL, path, nullptr });
        case PLAYER_TUNIC_ZORA:
            return ResolveCustomChain(
                { isAdult ? gCustomAdultZoraFPSHandDL : gCustomChildZoraFPSHandDL, path, nullptr });
        default:
            return path;
    }
}

static Gfx* LoadGfxByName(const char* path) {
    return path ? ResourceMgr_LoadGfxByName(path) : nullptr;
}

static Gfx* LoadCustomGfx(const char* path) {
    if (!path)
        return nullptr;
    path = ResolveCustomFPSHand(path);
    if (!ResourceMgr_FileAltExists(path) && !ResourceGetIsCustomByName(path))
        return nullptr;
    return ResourceMgr_LoadGfxByName(path);
}

static u8 sLastValidSwordEquip = EQUIP_VALUE_SWORD_NONE;

static u8 GetEquippedSwordValue(PlayState* play) {
    const u8 sword = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    const bool inCutscene = (play->csCtx.state != CS_STATE_IDLE);
    if (!(inCutscene && gSaveContext.linkAge == LINK_AGE_CHILD && sword == EQUIP_VALUE_SWORD_MASTER)) {
        sLastValidSwordEquip = sword;
    }
    return sLastValidSwordEquip;
}

static const char* GetSwordInSheathDL(PlayState* play) {
    switch (GetEquippedSwordValue(play)) {
        case EQUIP_VALUE_SWORD_KOKIRI:
            return gCustomKokiriSwordInSheathDL;
        case EQUIP_VALUE_SWORD_MASTER:
            return gCustomMasterSwordInSheathDL;
        case EQUIP_VALUE_SWORD_BIGGORON:
            if (gSaveContext.bgsFlag)
                return gCustomLongswordInSheathDL;
            if (CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE))
                return gCustomBrokenLongswordInSheathDL;
            return ResolveCustomChain({ gCustomBreakableLongswordInSheathDL, gCustomLongswordInSheathDL, nullptr });
    }
    return nullptr;
}

static const char* GetSheathOnlyDL(PlayState* play) {
    switch (GetEquippedSwordValue(play)) {
        case EQUIP_VALUE_SWORD_KOKIRI:
            return gCustomKokiriSwordSheathDL;
        case EQUIP_VALUE_SWORD_MASTER:
            return gCustomMasterSwordSheathDL;
        case EQUIP_VALUE_SWORD_BIGGORON:
            if (gSaveContext.bgsFlag)
                return gCustomLongswordSheathDL;
            if (CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE))
                return gCustomBrokenLongswordSheathDL;
            return ResolveCustomChain({ gCustomBreakableLongswordSheathDL, gCustomLongswordSheathDL, nullptr });
    }
    return nullptr;
}

static const char* GetShieldOnBackDL(s32 shield) {
    const bool isAdult = gSaveContext.linkAge == LINK_AGE_ADULT;
    switch (shield) {
        case PLAYER_SHIELD_DEKU:
            return gCustomDekuShieldOnBackDL;
        case PLAYER_SHIELD_HYLIAN:
            return isAdult ? gCustomHylianShieldOnBackDL : gCustomHylianShieldOnChildBackDL;
        case PLAYER_SHIELD_MIRROR:
            return gCustomMirrorShieldOnBackDL;
    }
    return nullptr;
}

// Hand/held shield model DL for the given shield value (Deku/Hylian/Mirror).
static const char* GetCustomShieldDL(s32 shield) {
    switch (shield) {
        case PLAYER_SHIELD_DEKU:
            return gCustomDekuShieldDL;
        case PLAYER_SHIELD_HYLIAN:
            return gCustomHylianShieldDL;
        case PLAYER_SHIELD_MIRROR:
            return gCustomMirrorShieldDL;
    }
    return nullptr;
}

// Allocates a small gfx buffer, emits up to two display lists (skipping null
// ones), terminates it, and stores it in *dList. Callers guard with
// "if (a || b)" so at least one is non-null. Mirrors the open-coded
// Graph_Alloc + gSPDisplayList + gSPEndDisplayList sequence used throughout.
static void EmitDLBuffer(PlayState* play, Gfx** dList, Gfx* a, Gfx* b) {
    Gfx* buf = (Gfx*)Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Gfx));
    Gfx* p = buf;
    if (a)
        gSPDisplayList(p++, a);
    if (b)
        gSPDisplayList(p++, b);
    gSPEndDisplayList(p);
    *dList = buf;
}

static const char* GetSwordInSheathDLForPlayer(Player* player, PlayState* play) {
    if (player == GET_PLAYER(play))
        return GetSwordInSheathDL(play);
    switch (player->heldItemId) {
        case ITEM_SWORD_KOKIRI:
            return gCustomKokiriSwordInSheathDL;
        case ITEM_SWORD_MASTER:
            return gCustomMasterSwordInSheathDL;
        case ITEM_SWORD_BGS:
            return gCustomLongswordInSheathDL;
        case ITEM_SWORD_KNIFE:
            return gCustomBrokenLongswordInSheathDL;
    }
    return nullptr;
}

static const char* GetSheathOnlyDLForPlayer(Player* player, PlayState* play) {
    if (player == GET_PLAYER(play))
        return GetSheathOnlyDL(play);
    switch (player->heldItemId) {
        case ITEM_SWORD_KOKIRI:
            return gCustomKokiriSwordSheathDL;
        case ITEM_SWORD_MASTER:
            return gCustomMasterSwordSheathDL;
        case ITEM_SWORD_BGS:
            return gCustomLongswordSheathDL;
        case ITEM_SWORD_KNIFE:
            return gCustomBrokenLongswordSheathDL;
    }
    return nullptr;
}

static u8 PauseGetLimbType(s32 limbIndex) {
    const u8 swordEquip = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    const u8 shieldEquip = CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
    const bool isBGS = (swordEquip == EQUIP_VALUE_SWORD_BIGGORON);
    const bool isSword = (swordEquip != EQUIP_VALUE_SWORD_NONE);
    const bool childHylian = (!LINK_IS_ADULT && shieldEquip == PLAYER_SHIELD_HYLIAN);

    switch (limbIndex) {
        case PLAYER_LIMB_L_HAND:
            if (!isSword)
                return PLAYER_MODELTYPE_LH_OPEN;
            return isBGS ? PLAYER_MODELTYPE_LH_BGS : PLAYER_MODELTYPE_LH_SWORD;
        case PLAYER_LIMB_R_HAND:
            return (isBGS || childHylian) ? PLAYER_MODELTYPE_RH_CLOSED : PLAYER_MODELTYPE_RH_SHIELD;
        case PLAYER_LIMB_SHEATH:
            if (isBGS || childHylian)
                return PLAYER_MODELTYPE_SHEATH_19;
            return PLAYER_MODELTYPE_SHEATH_17;
    }
    return 0;
}

// Counter-scale hand mesh when EquipmentAlwaysVisible + ScaleAdultEquipmentAsChild is active on child Link.
static constexpr float HAND_COUNTER_SCALE_Y_OFFSET = 100.0f;

static void BuildHandItemDL(PlayState* play, Gfx** dList, Gfx* hand, Gfx* item, bool counterScaleHand) {
    if (counterScaleHand) {
        Mtx* scaleMtx = (Mtx*)Graph_Alloc(play->state.gfxCtx, sizeof(Mtx));
        MtxF mf = {};
        mf.xx = mf.yy = mf.zz = 1.25f;
        mf.ww = 1.0f;
        mf.yw = HAND_COUNTER_SCALE_Y_OFFSET;
        Matrix_MtxFToMtx(&mf, scaleMtx);
        Gfx* buf = (Gfx*)Graph_Alloc(play->state.gfxCtx, 5 * sizeof(Gfx));
        Gfx* p = buf;
        gSPMatrix(p++, scaleMtx, G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        gSPDisplayList(p++, hand);
        gSPPopMatrix(p++, G_MTX_MODELVIEW);
        gSPDisplayList(p++, item);
        gSPEndDisplayList(p);
        *dList = buf;
    } else {
        Gfx* buf = (Gfx*)Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Gfx));
        Gfx* p = buf;
        gSPDisplayList(p++, hand);
        gSPDisplayList(p++, item);
        gSPEndDisplayList(p);
        *dList = buf;
    }
}

static bool IsScalingAdultItemAsChild() {
    return CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) &&
           CVarGetInteger(CVAR_ENHANCEMENT("ScaleAdultEquipmentAsChild"), 0) && !LINK_IS_ADULT;
}

const char* bottleContentDLs[] = {
    nullptr,                    // 0: PLAYER_IA_BOTTLE (empty - no custom content needed)
    gCustomBottleFishDL,        // 1: PLAYER_IA_BOTTLE_FISH
    gCustomBottleBlueFireDL,    // 2: PLAYER_IA_BOTTLE_FIRE
    gCustomBottleBugDL,         // 3: PLAYER_IA_BOTTLE_BUG
    gCustomBottlePoeDL,         // 4: PLAYER_IA_BOTTLE_POE
    gCustomBottleBigPoeDL,      // 5: PLAYER_IA_BOTTLE_BIG_POE
    gCustomBottleLetterDL,      // 6: PLAYER_IA_BOTTLE_RUTOS_LETTER
    gCustomBottleRedPotionDL,   // 7: PLAYER_IA_BOTTLE_POTION_RED
    gCustomBottleBluePotionDL,  // 8: PLAYER_IA_BOTTLE_POTION_BLUE
    gCustomBottleGreenPotionDL, // 9: PLAYER_IA_BOTTLE_POTION_GREEN
    gCustomBottleMilkDL,        // 10: PLAYER_IA_BOTTLE_MILK_FULL
    gCustomBottleMilkHalfDL,    // 11: PLAYER_IA_BOTTLE_MILK_HALF
    gCustomBottleFairyDL,       // 12: PLAYER_IA_BOTTLE_FAIRY
};

static void RegisterCustomEquipment() {
    // World (gameplay) character
    COND_VB_SHOULD(VB_PLAYER_OVERRIDE_LIMB_DRAW, CVarGetInteger(CVAR_SETTING("AltAssets"), 1), {
        s32 limbIndex = va_arg(args, s32);
        Gfx** dList = va_arg(args, Gfx**);
        Player* player = (Player*)va_arg(args, void*);
        PlayState* play = va_arg(args, PlayState*);

        // NEI: while transformed (MM mask form) the form draws its own skeleton,
        // so Link's custom-equipment limb override must not run.
        if (TransformMasks_IsTransformedAny()) {
            va_end(args);
            return;
        }

        const bool isAdult = gSaveContext.linkAge == LINK_AGE_ADULT;
        const char* customDL = nullptr;

        switch (limbIndex) {
            case PLAYER_LIMB_L_HAND: {
                const bool isOcarina =
                    player->heldItemAction == PLAYER_IA_OCARINA_FAIRY ||
                    player->heldItemAction == PLAYER_IA_OCARINA_OF_TIME ||
                    player->itemAction == PLAYER_IA_OCARINA_FAIRY || player->itemAction == PLAYER_IA_OCARINA_OF_TIME ||
                    player->modelGroup == PLAYER_MODELGROUP_OCARINA || player->modelGroup == PLAYER_MODELGROUP_OOT;
                if (isOcarina) {
                    Gfx* resolvedHand = LoadGfxByName(isAdult ? gLinkAdultLeftHandNearDL : gLinkChildLeftHandNearDL);
                    if (resolvedHand) {
                        EmitDLBuffer(play, dList, resolvedHand, nullptr);
                    }
                    break;
                }
                switch ((u8)player->leftHandType) {
                    case PLAYER_MODELTYPE_LH_SWORD: {
                        if (player == GET_PLAYER(play)) {
                            if (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KOKIRI)
                                customDL = gCustomKokiriSwordDL;
                            else if (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER)
                                customDL = gCustomMasterSwordDL;
                        } else {
                            if (player->heldItemAction == PLAYER_IA_SWORD_KOKIRI)
                                customDL = gCustomKokiriSwordDL;
                            else if (player->heldItemAction == PLAYER_IA_SWORD_MASTER)
                                customDL = gCustomMasterSwordDL;
                        }
                        break;
                    }
                    case PLAYER_MODELTYPE_LH_BGS: {
                        const bool isBrokenKnife =
                            (player == GET_PLAYER(play))
                                ? CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)
                                : (player->heldItemId == ITEM_SWORD_KNIFE);
                        if (isBrokenKnife) {
                            customDL = gCustomBrokenLongswordDL;
                        } else if (player == GET_PLAYER(play)) {
                            if (gSaveContext.bgsFlag)
                                customDL = gCustomLongswordDL;
                            else
                                customDL =
                                    ResolveCustomChain({ gCustomBreakableLongswordDL, gCustomLongswordDL, nullptr });
                        } else {
                            customDL = gCustomLongswordDL;
                        }
                        break;
                    }
                    case PLAYER_MODELTYPE_LH_HAMMER:
                        customDL = gCustomHammerDL;
                        break;
                    case PLAYER_MODELTYPE_LH_BOOMERANG:
                        if (!(player->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN))
                            customDL = gCustomBoomerangDL;
                        break;
                }
                Gfx* resolvedCustom = LoadCustomGfx(customDL);
                if (resolvedCustom) {
                    Gfx* resolvedHand =
                        LoadGfxByName(isAdult ? gLinkAdultLeftHandClosedNearDL : gLinkChildLeftFistNearDL);
                    if (resolvedHand) {
                        const u8 lht = (u8)player->leftHandType;
                        const bool scaleHand = IsScalingAdultItemAsChild() &&
                                               ((gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI &&
                                                 lht == PLAYER_MODELTYPE_LH_SWORD) ||
                                                lht == PLAYER_MODELTYPE_LH_BGS || lht == PLAYER_MODELTYPE_LH_HAMMER);
                        BuildHandItemDL(play, dList, resolvedHand, resolvedCustom, scaleHand);
                    }
                }
                break;
            }

            case PLAYER_LIMB_R_HAND: {
                if (player->unk_6AD == 2) {
                    const char* fpsHand = nullptr;
                    const char* fpsWeapon = nullptr;

                    switch (player->rightHandType) {
                        case PLAYER_MODELTYPE_RH_BOW_SLINGSHOT:
                        case PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2:
                            fpsHand = isAdult ? gCustomAdultFPSHandDL : gCustomChildFPSHandDL;
                            fpsWeapon =
                                Player_HoldsBow(player)
                                    ? ResolveCustomChain({ gCustomFPSBowDL, gCustomBowDL, nullptr })
                                    : ResolveCustomChain({ gCustomFPSSlingshotDL, gCustomSlingshotDL, nullptr });
                            break;
                        case PLAYER_MODELTYPE_RH_HOOKSHOT:
                            fpsHand = isAdult ? gCustomAdultFPSHandDL : gCustomChildFPSHandDL;
                            fpsWeapon = (player->heldItemAction == PLAYER_IA_HOOKSHOT)
                                            ? ResolveCustomChain({ gCustomFPSHookshotDL, gCustomHookshotDL, nullptr })
                                            : ResolveCustomChain({ gCustomFPSLongshotDL, gCustomLongshotDL, nullptr });
                            break;
                    }

                    Gfx* resolvedFpsWeapon = LoadCustomGfx(fpsWeapon);
                    Gfx* resolvedFpsHand = LoadCustomGfx(fpsHand);
                    if (resolvedFpsWeapon || resolvedFpsHand) {
                        EmitDLBuffer(play, dList, resolvedFpsWeapon, resolvedFpsHand);
                    }
                } else {
                    bool useOpenHand = false;
                    const bool holdsTwoHanded = player->heldItemAction >= PLAYER_IA_SWORD_BIGGORON &&
                                                player->heldItemAction <= PLAYER_IA_HAMMER;
                    const bool isChildHylian = !isAdult && player->currentShield == PLAYER_SHIELD_HYLIAN;
                    const bool isShielding = (player->stateFlags1 & PLAYER_STATE1_SHIELDING) != 0 && !isChildHylian &&
                                             (!holdsTwoHanded || (CVarGetInteger(CVAR_CHEAT("ShieldTwoHanded"), 0) &&
                                                                  player->heldItemAction != PLAYER_IA_DEKU_STICK));
                    const bool isOcarina = player->heldItemAction == PLAYER_IA_OCARINA_FAIRY ||
                                           player->heldItemAction == PLAYER_IA_OCARINA_OF_TIME ||
                                           player->itemAction == PLAYER_IA_OCARINA_FAIRY ||
                                           player->itemAction == PLAYER_IA_OCARINA_OF_TIME ||
                                           player->modelGroup == PLAYER_MODELGROUP_OCARINA ||
                                           player->modelGroup == PLAYER_MODELGROUP_OOT;
                    if (isShielding) {
                        customDL = GetCustomShieldDL(player->currentShield);
                    } else if (isOcarina) {
                        const bool isOoT = player->heldItemAction == PLAYER_IA_OCARINA_OF_TIME ||
                                           player->itemAction == PLAYER_IA_OCARINA_OF_TIME ||
                                           player->modelGroup == PLAYER_MODELGROUP_OOT;
                        customDL = isOoT ? (isAdult ? gCustomOcarinaOfTimeAdultDL : gCustomOcarinaOfTimeDL)
                                         : (isAdult ? gCustomFairyOcarinaAdultDL : gCustomFairyOcarinaDL);
                        useOpenHand = true;
                    } else {
                        switch ((u8)player->rightHandType) {
                            case PLAYER_MODELTYPE_RH_SHIELD:
                                customDL = GetCustomShieldDL(player->currentShield);
                                break;
                            case PLAYER_MODELTYPE_RH_BOW_SLINGSHOT:
                            case PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2:
                                customDL = Player_HoldsBow(player) ? gCustomBowDL : gCustomSlingshotDL;
                                break;
                            case PLAYER_MODELTYPE_RH_HOOKSHOT:
                                customDL = (player->heldItemAction == PLAYER_IA_HOOKSHOT) ? gCustomHookshotDL
                                                                                          : gCustomLongshotDL;
                                break;
                            case PLAYER_MODELTYPE_RH_OCARINA:
                                customDL = isAdult ? gCustomFairyOcarinaAdultDL : gCustomFairyOcarinaDL;
                                useOpenHand = true;
                                break;
                            case PLAYER_MODELTYPE_RH_OOT:
                                customDL = isAdult ? gCustomOcarinaOfTimeAdultDL : gCustomOcarinaOfTimeDL;
                                useOpenHand = true;
                                break;
                        }
                    }
                    Gfx* resolvedCustom = LoadCustomGfx(customDL);
                    if (resolvedCustom) {
                        const char* handPath =
                            useOpenHand ? (isAdult ? gLinkAdultRightHandNearDL : gLinkChildRightHandNearDL)
                                        : (isAdult ? gLinkAdultRightHandClosedNearDL : gLinkChildRightHandClosedNearDL);
                        Gfx* resolvedHand = LoadGfxByName(handPath);
                        if (resolvedHand) {
                            const u8 rht = (u8)player->rightHandType;
                            const bool scaleHand =
                                IsScalingAdultItemAsChild() && !isOcarina &&
                                ((player->currentShield == PLAYER_SHIELD_MIRROR &&
                                  (isShielding || rht == PLAYER_MODELTYPE_RH_SHIELD)) ||
                                 (!isShielding &&
                                  (rht == PLAYER_MODELTYPE_RH_HOOKSHOT ||
                                   (rht == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT && Player_HoldsBow(player)))));
                            BuildHandItemDL(play, dList, resolvedHand, resolvedCustom, scaleHand);
                        }
                    }
                }
                break;
            }

            case PLAYER_LIMB_SHEATH: {
                u8 sheathType = (u8)player->sheathType;
                const bool isOcarinaSheath =
                    player->heldItemAction == PLAYER_IA_OCARINA_FAIRY ||
                    player->heldItemAction == PLAYER_IA_OCARINA_OF_TIME ||
                    player->itemAction == PLAYER_IA_OCARINA_FAIRY || player->itemAction == PLAYER_IA_OCARINA_OF_TIME ||
                    player->modelGroup == PLAYER_MODELGROUP_OCARINA || player->modelGroup == PLAYER_MODELGROUP_OOT;
                if (isOcarinaSheath) {
                    const bool hasShieldEquipped = player->currentShield != PLAYER_SHIELD_NONE;
                    sheathType = hasShieldEquipped ? PLAYER_MODELTYPE_SHEATH_18 : PLAYER_MODELTYPE_SHEATH_16;
                } else if (player->stateFlags1 & PLAYER_STATE1_SHIELDING) {
                    const bool sheathTwoHanded = player->heldItemAction >= PLAYER_IA_SWORD_BIGGORON &&
                                                 player->heldItemAction <= PLAYER_IA_HAMMER;
                    const bool sheathChildHylian = !isAdult && player->currentShield == PLAYER_SHIELD_HYLIAN;
                    const bool sheathCanShield =
                        !sheathChildHylian && (!sheathTwoHanded || (CVarGetInteger(CVAR_CHEAT("ShieldTwoHanded"), 0) &&
                                                                    player->heldItemAction != PLAYER_IA_DEKU_STICK));
                    if (sheathCanShield) {
                        if (sheathType == PLAYER_MODELTYPE_SHEATH_18)
                            sheathType = PLAYER_MODELTYPE_SHEATH_16;
                        else if (sheathType == PLAYER_MODELTYPE_SHEATH_19)
                            sheathType = PLAYER_MODELTYPE_SHEATH_17;
                    }
                }
                const bool hasSword =
                    (sheathType == PLAYER_MODELTYPE_SHEATH_16 || sheathType == PLAYER_MODELTYPE_SHEATH_18);
                const bool hasShield =
                    (sheathType == PLAYER_MODELTYPE_SHEATH_18 || sheathType == PLAYER_MODELTYPE_SHEATH_19);
                const bool emptySheath =
                    (sheathType == PLAYER_MODELTYPE_SHEATH_17 || sheathType == PLAYER_MODELTYPE_SHEATH_19);

                const char* swordPath = hasSword      ? GetSwordInSheathDLForPlayer(player, play)
                                        : emptySheath ? GetSheathOnlyDLForPlayer(player, play)
                                                      : nullptr;
                const char* shieldPath = hasShield ? GetShieldOnBackDL(player->currentShield) : nullptr;

                Gfx* resolvedSword = LoadCustomGfx(swordPath);
                Gfx* resolvedShield = LoadCustomGfx(shieldPath);
                if (resolvedSword || resolvedShield) {
                    EmitDLBuffer(play, dList, resolvedSword, resolvedShield);
                }
                break;
            }

            default:
                break;
        }
    });

    // Pause/equipment screen character
    COND_VB_SHOULD(VB_PLAYER_OVERRIDE_LIMB_DRAW_PAUSE, CVarGetInteger(CVAR_SETTING("AltAssets"), 1), {
        s32 limbIndex = va_arg(args, s32);
        Gfx** dList = va_arg(args, Gfx**);
        Player* player = (Player*)va_arg(args, void*);
        PlayState* play = va_arg(args, PlayState*);

        // NEI: skip custom-equipment limb override while transformed.
        if (TransformMasks_IsTransformedAny()) {
            va_end(args);
            return;
        }

        const bool isAdult = gSaveContext.linkAge == LINK_AGE_ADULT;
        const char* customDL = nullptr;

        switch (limbIndex) {
            case PLAYER_LIMB_L_HAND: {
                switch (PauseGetLimbType(PLAYER_LIMB_L_HAND)) {
                    case PLAYER_MODELTYPE_LH_SWORD: {
                        const u8 swordEquip = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
                        if (swordEquip == EQUIP_VALUE_SWORD_KOKIRI)
                            customDL = gCustomKokiriSwordDL;
                        else
                            customDL = gCustomMasterSwordDL;
                        break;
                    }
                    case PLAYER_MODELTYPE_LH_BGS: {
                        if (gSaveContext.bgsFlag) {
                            customDL = gCustomLongswordDL;
                        } else if (CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
                            customDL = gCustomBrokenLongswordDL;
                        } else {
                            customDL = ResolveCustomChain({ gCustomBreakableLongswordDL, gCustomLongswordDL, nullptr });
                        }
                        break;
                    }
                }
                Gfx* resolvedCustom = LoadCustomGfx(customDL);
                if (resolvedCustom) {
                    Gfx* resolvedHand =
                        LoadGfxByName(isAdult ? gLinkAdultLeftHandClosedNearDL : gLinkChildLeftFistNearDL);
                    if (resolvedHand) {
                        EmitDLBuffer(play, dList, resolvedHand, resolvedCustom);
                    }
                }
                break;
            }

            case PLAYER_LIMB_R_HAND: {
                if (PauseGetLimbType(PLAYER_LIMB_R_HAND) == PLAYER_MODELTYPE_RH_SHIELD) {
                    customDL = GetCustomShieldDL(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD));
                }
                Gfx* resolvedCustom = LoadCustomGfx(customDL);
                if (resolvedCustom) {
                    Gfx* resolvedHand =
                        LoadGfxByName(isAdult ? gLinkAdultRightHandClosedNearDL : gLinkChildRightHandClosedNearDL);
                    if (resolvedHand) {
                        EmitDLBuffer(play, dList, resolvedHand, resolvedCustom);
                    }
                }
                break;
            }

            case PLAYER_LIMB_SHEATH: {
                const u8 sheathType = PauseGetLimbType(PLAYER_LIMB_SHEATH);
                const bool hasSword =
                    (sheathType == PLAYER_MODELTYPE_SHEATH_16 || sheathType == PLAYER_MODELTYPE_SHEATH_18);
                const bool hasShield =
                    (sheathType == PLAYER_MODELTYPE_SHEATH_18 || sheathType == PLAYER_MODELTYPE_SHEATH_19);
                const bool emptySheath =
                    (sheathType == PLAYER_MODELTYPE_SHEATH_17 || sheathType == PLAYER_MODELTYPE_SHEATH_19);

                const char* swordPath = hasSword      ? GetSwordInSheathDL(play)
                                        : emptySheath ? GetSheathOnlyDL(play)
                                                      : nullptr;
                const char* shieldPath = hasShield ? GetShieldOnBackDL(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD)) : nullptr;

                Gfx* resolvedSword = LoadCustomGfx(swordPath);
                Gfx* resolvedShield = LoadCustomGfx(shieldPath);
                if (resolvedSword || resolvedShield) {
                    EmitDLBuffer(play, dList, resolvedSword, resolvedShield);
                }
                break;
            }

            default:
                break;
        }
    });

    COND_VB_SHOULD(VB_DRAW_HOOKSHOT_TIP, CVarGetInteger(CVAR_SETTING("AltAssets"), 1), {
        Player* player = va_arg(args, Player*);
        PlayState* play = va_arg(args, PlayState*);

        // NEI: skip custom hookshot tip DL while transformed.
        if (TransformMasks_IsTransformedAny()) {
            va_end(args);
            return;
        }
        const char* tipPath = (player->heldItemAction == PLAYER_IA_LONGSHOT)
                                  ? ResolveCustomChain({ gCustomLongshotTipDL, gCustomHookshotTipDL, nullptr })
                                  : gCustomHookshotTipDL;
        Gfx* resolvedTip = LoadCustomGfx(tipPath);
        if (resolvedTip) {
            *should = false;
            gSPDisplayList(play->state.gfxCtx->polyOpa.p++, resolvedTip);
        }
    });

    COND_VB_SHOULD(VB_DRAW_HOOKSHOT_CHAIN, CVarGetInteger(CVAR_SETTING("AltAssets"), 1), {
        Player* player = va_arg(args, Player*);
        PlayState* play = va_arg(args, PlayState*);

        // NEI: skip custom hookshot chain DL while transformed.
        if (TransformMasks_IsTransformedAny()) {
            va_end(args);
            return;
        }
        const char* chainPath = (player->heldItemAction == PLAYER_IA_LONGSHOT)
                                    ? ResolveCustomChain({ gCustomLongshotChainDL, gCustomHookshotChainDL, nullptr })
                                    : gCustomHookshotChainDL;
        Gfx* resolvedChain = LoadCustomGfx(chainPath);
        if (resolvedChain) {
            *should = false;
            gSPDisplayList(play->state.gfxCtx->polyOpa.p++, resolvedChain);
        }
    });

    COND_VB_SHOULD(VB_PLAYER_DRAW_BOTTLE, CVarGetInteger(CVAR_SETTING("AltAssets"), 1), {
        Player* player = va_arg(args, Player*);
        PlayState* play = va_arg(args, PlayState*);
        const char* contentDL = nullptr;
        Gfx* resolvedContent = nullptr;
        Gfx* resolvedBottle = LoadCustomGfx(gCustomBottleDL);
        if (resolvedBottle) {
            *should = false;
            gSPDisplayList(play->state.gfxCtx->polyXlu.p++, resolvedBottle);

            if (player->itemAction >= PLAYER_IA_BOTTLE &&
                player->itemAction < PLAYER_IA_BOTTLE + std::size(bottleContentDLs)) {
                contentDL = bottleContentDLs[player->itemAction - PLAYER_IA_BOTTLE];
            }

            if (contentDL) {
                resolvedContent = LoadCustomGfx(contentDL);
            }

            if (resolvedContent) {
                gSPDisplayList(play->state.gfxCtx->polyOpa.p++, resolvedContent);
            }
        }
    });

    COND_VB_SHOULD(VB_PLAYER_UPDATE_BOTTLE_HELD, CVarGetInteger(CVAR_SETTING("AltAssets"), 1), {
        Player* player = va_arg(args, Player*);
        const bool isFullMilk = player->itemAction == PLAYER_IA_BOTTLE_MILK_FULL;
        if (isFullMilk) {
            *should = false;
            player->itemAction = PLAYER_IA_BOTTLE_MILK_HALF;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterCustomEquipment, { CVAR_SETTING("AltAssets") });

// Skijer's NEI: player-draw fork (VB_PLAYER_DRAW_BEGIN). Fires first in Player_Draw;
// *should=false suppresses the vanilla draw. Pak/O2r skeleton swap stays inline in z_player.c.
// Registered unconditionally (each block self-guards), NOT gated on AltAssets.
static void RegisterPlayerDrawForkNEI() {
    // Skijer's NEI: hide Link's held-weapon DL when a custom item draws its own model
    // (Byrna / IK Axe via ExtEquip_ShouldHideSwordDL, or the Fire/Ice/Light rods).
    REGISTER_VB_SHOULD(VB_PLAYER_SHOULD_HIDE_HELD_WEAPON, {
        Player* player = (Player*)va_arg(args, void*);
        if (ExtEquip_ShouldHideSwordDL() || player->itemAction == PLAYER_IA_ROD_FIRE ||
            player->itemAction == PLAYER_IA_ROD_ICE || player->itemAction == PLAYER_IA_ROD_LIGHT) {
            *should = true;
        }
    });

    // Skijer's NEI: held item is two-handed for the FD-skin sword + custom Fire/Ice/Light rods (BGS-style)
    REGISTER_VB_SHOULD(VB_PLAYER_HOLDS_TWO_HANDED_WEAPON, {
        Player* player = (Player*)va_arg(args, void*);
        // FD wields the Deity sword two-handed no matter which sword is equipped, and
        // nothing at all when no sword is in hand — Player_IsFDHoldingSword is that gate
        // (swords only: a Deku Stick / Hammer in FD's hands keeps its own identity).
        // The Cane of Byrna is the Insect Glaive: a pole weapon, so it wields
        // two-handed and carries no shield, exactly like the Biggoron's Sword.
        if (Player_IsFDHoldingSword(player) || ExtEquip_ByrnaIsTwoHanded(player) ||
            player->heldItemAction == PLAYER_IA_ROD_FIRE || player->heldItemAction == PLAYER_IA_ROD_ICE ||
            player->heldItemAction == PLAYER_IA_ROD_LIGHT) {
            *should = true;
        }
    });

    REGISTER_VB_SHOULD(VB_PLAYER_DRAW_BEGIN, {
        PlayState* play = va_arg(args, PlayState*);
        Player* player = va_arg(args, Player*);
        u8 isLocalPlayer = (player == GET_PLAYER(play));

        // PAK Loader: free previous frame's combined DLs (main player only)
        if (isLocalPlayer) {
            PakLoader_FrameBegin();
        }

        // Harpoon Prop Hunt prop-draw intercept (local only); returns 1 when it drew a prop
        if (isLocalPlayer) {
            if (HarpoonPropHunt_TryDrawLocalProp(&player->actor, play)) {
                *should = false;
                va_end(args);
                return;
            }
        }

        // SM64 Mario: draw Mario instead of Link (HasMesh stricter than IsReady)
        if (isLocalPlayer) {
            if (Sm64Mario_HasMesh()) {
                Sm64Mario_Draw(play, player);
                *should = false;
                va_end(args);
                return;
            }
            // CVAR on but Mario not drawable yet (detransform / Lens held): hide Link
            if (Sm64Mario_ShouldHideLink()) {
                *should = false;
                va_end(args);
                return;
            }
        }

        // SW97 Cucco mode: draw cucco model instead of Link, but walk Link's
        // skeleton with null limbs so shadow + Navi follow (same pattern as
        // GaroForm_DrawNullBody / MmForm_Draw).
        if (isLocalPlayer && Sw97_IsCuccoModeActive()) {
            Sw97_DrawCuccoForm(play, player);
            OPEN_DISPS(play->state.gfxCtx);
            if (!(player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
                if (player->unk_862 > 0) {
                    Player_DrawGetItem(play, player);
                }
                CustomItems_OverrideDraw(player, play);
                ExtEquip_DrawBehavior(player, play);
            }
            CLOSE_DISPS(play->state.gfxCtx);
            *should = false;
            va_end(args);
            return;
        }

        // Transformation Masks: draw MM form instead of Link; Dragon Scale swim draws barrier only
        if (isLocalPlayer && TransformMasks_IsZoraSwimEnabled()) {
            TransformMasks_Draw(play, player); // barrier only (INACTIVE + zoraSwimEnabled)
        }

        if (isLocalPlayer && (TransformMasks_IsTransformed() || TransformMasks_IsFDSkinMode())) {
            if (TransformMasks_HasSkeleton()) {
                {
                    TransformMasks_Draw(play, player);

                    // Refresh hookshot anchor (unk_3C8): PostLimbDrawGameplay won't run, else pull never ends
                    if ((player->heldItemAction == PLAYER_IA_HOOKSHOT) ||
                        (player->heldItemAction == PLAYER_IA_LONGSHOT)) {
                        player->unk_3C8.x = player->actor.world.pos.x;
                        player->unk_3C8.y = player->actor.world.pos.y + 40.0f; // approx hand height
                        player->unk_3C8.z = player->actor.world.pos.z;
                    }

                    // Still draw get-item + custom items on MM forms
                    OPEN_DISPS(play->state.gfxCtx);
                    if (!(player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
                        if (player->unk_862 > 0) {
                            Player_DrawGetItem(play, player);
                        }
                        CustomItems_OverrideDraw(player, play);
                        ExtEquip_DrawBehavior(player, play);
                    }
                    CLOSE_DISPS(play->state.gfxCtx);
                    *should = false;
                    va_end(args);
                    return;
                }
                // First-person aim (unk_6AD != 0): fall through; limbs hidden but skeleton still processes
            } else {
                // Skeleton not loaded: flash overlay only, fall through to Link draw
                TransformMasks_Draw(play, player);
            }
        }
    });
}

static RegisterShipInitFunc initFuncPlayerDrawFork(RegisterPlayerDrawForkNEI, {});

// Skijer's NEI: SW97 cucco egg hooks. While cucco mode is active, tag
// EnArrow at Init (swap draw → pocket-egg DL) and clamp its speedXZ each
// Update tick. Vanilla aim + release flow unchanged — only visuals + speed
// are affected once the arrow is airborne.
static void RegisterCuccoArrowEggHooks() {
    COND_ID_HOOK(OnActorInit, ACTOR_EN_ARROW, true, [](void* actorPtr) {
        if (Sw97_IsCuccoModeActive()) {
            Sw97_TagCuccoEgg((Actor*)actorPtr);
        }
    });
    COND_ID_HOOK(OnActorUpdate, ACTOR_EN_ARROW, true, [](void* actorPtr) { Sw97_TickCuccoEggClamp((Actor*)actorPtr); });

    // The cucco shield forces player->currentShield to Deku so vanilla
    // projectiles will reflect off it (EnNutsball / EnOkuta both gate on that
    // field). The side effect to shut down is fire: a burning block would run
    // Inventory_DeleteEquipment and destroy a shield the player does not own.
    REGISTER_VB_SHOULD(VB_BURN_SHIELD, {
        if (Sw97_CuccoShieldIsUp()) {
            *should = false;
        }
    });

    // Cucco eggs are free. Without this, fire and light eggs would bill the
    // player for magic they never spent on a bow.
    REGISTER_VB_SHOULD(VB_EN_ARROW_MAGIC_CONSUMPTION, {
        if (Sw97_IsCuccoModeActive()) {
            *should = false;
        }
    });

    // Soul-arrow cucco is a movement-only form: reaching for any item drops
    // the transformation instead of using it. The CVar form keeps its items.
    REGISTER_VB_SHOULD(VB_CHANGE_HELD_ITEM_AND_USE_ITEM, {
        if (Sw97_IsCuccoModeActive() && gSw97CuccoModeSource == 0) {
            Sw97_EndCuccoMode();
            *should = false;
        }
    });
}
static RegisterShipInitFunc initFuncCuccoArrowEggHooks(RegisterCuccoArrowEggHooks, {});

static void RegisterSagesTunicHooks() {
    REGISTER_VB_SHOULD(VB_RECIEVE_FALL_DAMAGE, {
        if (ExtEquip_HasSagesResistance(SAGES_RESIST_FALL)) {
            ExtEquip_SagesFlash(SAGES_RESIST_FALL);
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_LIKE_LIKE_GRAB_PLAYER, {
        if (ExtEquip_HasSagesResistance(SAGES_RESIST_STUN)) {
            ExtEquip_SagesFlash(SAGES_RESIST_STUN);
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_REDEAD_GIBDO_FREEZE_LINK, {
        if (ExtEquip_HasSagesResistance(SAGES_RESIST_STUN)) {
            ExtEquip_SagesFlash(SAGES_RESIST_STUN);
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_ENEMY_GRAB_PLAYER, {
        if (ExtEquip_HasSagesResistance(SAGES_RESIST_STUN)) {
            ExtEquip_SagesFlash(SAGES_RESIST_STUN);
            *should = false;
        }
    });
}

static RegisterShipInitFunc initFuncSagesTunicHooks(RegisterSagesTunicHooks, {});

extern "C" u8 Champion_AllowsMidairAim(Player* player);

static void RegisterChampionHooks() {
    REGISTER_VB_SHOULD(VB_PLAYER_ALLOW_MIDAIR_AIM, {
        Player* player = va_arg(args, Player*);
        if (Champion_AllowsMidairAim(player)) {
            *should = true;
        }
        // A flying cucco needs to be able to aim its eggs. Without this,
        // Player_ActionHandler_13 refuses midair and the mirilla dies on the
        // frame it opens.
        if (Sw97_CuccoEggAimActive()) {
            *should = true;
        }
    });
}

static RegisterShipInitFunc initFuncChampionHooks(RegisterChampionHooks, {});

// Skijer's NEI: SM64 pre-UpdateCommon pre-pass (z_player pieces 1-2,5-7; 3-4 stay inline)
#define SM64_SWAP_AB(b) (((b) & ~(BTN_A | BTN_B)) | (((b)&BTN_A) ? BTN_B : 0) | (((b)&BTN_B) ? BTN_A : 0))
static void RegisterSm64PreUpdateCommonNEI() {
    // Pieces 1-2: tick transition-suspend (before any IsActive/IsReady check),
    // then the (now no-op) Mario-mask C-Down force/toggle.
    REGISTER_VB_SHOULD(VB_SM64_PLAYER_PRE_ACTION, {
        PlayState* play = va_arg(args, PlayState*);
        Player* player = va_arg(args, Player*);
        (void)va_arg(args, Input*); // &sp44 (unused by pieces 1-2)
        Sm64Mario_TickTransitionSuspend(play, player);
        Sm64MarioMask_ForceAndToggle(play, player);
    });

    // Pieces 5-7: steal damage, read Pikachu status, then swap A<->B on the exact
    // sp44 that is passed straight into Player_UpdateCommon (OOT's contextual A).
    REGISTER_VB_SHOULD(VB_SM64_PLAYER_PRE_UPDATE_COMMON, {
        PlayState* play = va_arg(args, PlayState*);
        Player* player = va_arg(args, Player*);
        Input* in = va_arg(args, Input*);
        Sm64Mario_InterceptDamage(play, player);
        if (MmForm_IsPikachuActive()) {
            PikachuForm_InterceptStatus(play, player);
        }
        // SW97 cucco mode: strip the buttons the cucco moveset owns from
        // Link's input, so his actionFunc doesn't roll, jump-slash or raise a
        // shield a cucco isn't carrying. Sw97_TickCuccoMode reads the raw
        // presses from play->state.input[0] (unaffected by this) instead.
        //
        // A and B are always ours (flap/glide, Wing Whack/spin, egg fire).
        // R is conditional: with a real shield equipped AND on the ground it
        // is left alone so Link's own shield AI takes over — that fallback is
        // the whole reason this isn't a flat strip. Airborne R stays ours so
        // the ground pound survives regardless of equipment.
        //
        // Stripping A/B/R also keeps the first-person aim alive: the vanilla
        // aim state bails on any A/B/R press (z_player.c:14753), and it reads
        // this same stripped copy.
        if (Sw97_IsCuccoModeActive()) {
            u16 strip = BTN_A | BTN_B;
            // Read the EQUIPMENT, never player->currentShield — the cucco
            // shield overwrites that field to Deku while it is up.
            bool hasShield = SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD)) != PLAYER_SHIELD_NONE;
            bool grounded = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
            if (!(hasShield && grounded)) {
                strip |= BTN_R;
            }
            in->cur.button &= ~strip;
            in->press.button &= ~strip;
            in->rel.button &= ~strip;
        }
        if (Sm64Mario_IsReady()) {
            in->cur.button = SM64_SWAP_AB(in->cur.button);
            in->press.button = SM64_SWAP_AB(in->press.button);
            in->rel.button = SM64_SWAP_AB(in->rel.button);
        }
    });
}

static RegisterShipInitFunc initFuncSm64PreUpdateCommon(RegisterSm64PreUpdateCommonNEI, {});
#undef SM64_SWAP_AB

// Skijer's NEI: player anim-override fork (VB_PLAYER_ANIM_OVERRIDE). Each site stores the
// vanilla anim in *animOut; a getter overwrites it only when non-NULL (else vanilla unchanged).
// Registered unconditionally (each block self-guards).
// Left inline in z_player.c (they re-play on top, not single-play): func_808358F0 boomerang
// throw, func_80831F00 melee start.
extern "C" {
LinkAnimationHeader* MmForm_GetJumpSlashAnim(s32 phase);
LinkAnimationHeader* MmForm_GetZoraBoomerangAnim(s32 phase);
}

static void RegisterPlayerAnimOverrideNEI() {
    REGISTER_VB_SHOULD(VB_PLAYER_ANIM_OVERRIDE, {
        s32 siteId = va_arg(args, s32);
        s32 siteArg = va_arg(args, s32);
        LinkAnimationHeader** animOut = va_arg(args, LinkAnimationHeader**);
        Player* player = va_arg(args, Player*);

        switch (siteId) {
            case VB_PLAYER_ANIM_SITE_DODGE_HOP: {
                // Gerudo's sidehops and backflip. siteArg is the direction; the clip
                // is resampled to the vanilla one's length inside the getter, so the
                // hop keeps exactly OOT's timing and travel.
                LinkAnimationHeader* gerudoHop = GerudoMhr_GetHopAnim(siteArg);
                if (gerudoHop != nullptr) {
                    *animOut = gerudoHop;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_SHIELD_RAISE: {
                // Gerudo blade guard: the raise slice (1-20 of the flourish, x2).
                // The Trident does NOT override this: its guard is vanilla's shield
                // stance now, and R+B is a guard dash instead of a crouch stab.
                LinkAnimationHeader* gerudoRaise = GerudoMhr_GetGuardAnim(player, 0);
                if (gerudoRaise != nullptr) {
                    *animOut = gerudoRaise;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_SHIELD_LOOP: {
                LinkAnimationHeader* gerudoLoop = GerudoMhr_GetGuardAnim(player, 1);
                if (gerudoLoop != nullptr) {
                    *animOut = gerudoLoop;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_MELEE_SWING: {
                // Forms fight with their body, so their aerial slashes are kicks and fin swipes,
                // not Link's sword clips.
                if (TransformMasks_IsTransformed() && (siteArg >= PLAYER_MWA_FLIPSLASH_START) &&
                    (siteArg <= PLAYER_MWA_JUMPSLASH_FINISH)) {
                    LinkAnimationHeader* formAnim = MmForm_GetJumpSlashAnim(siteArg);
                    if (formAnim != nullptr) {
                        *animOut = formAnim;
                    }
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_FALL_WAIT: {
                // Gerudo falls with the blades out.
                LinkAnimationHeader* gerudoFall = GerudoMhr_GetFallAnim(player);
                if (gerudoFall != nullptr) {
                    *animOut = gerudoFall;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_ZORA_BOOMERANG_WAIT: {
                // Zora boomerang phase 0, transformed only
                LinkAnimationHeader* formAnim =
                    TransformMasks_IsTransformed() ? MmForm_GetZoraBoomerangAnim(0) : nullptr;
                if (formAnim != nullptr) {
                    *animOut = formAnim;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_ZORA_BOOMERANG_CATCH: {
                // Zora boomerang phase 2, transformed only
                LinkAnimationHeader* zoraCatch =
                    TransformMasks_IsTransformed() ? MmForm_GetZoraBoomerangAnim(2) : nullptr;
                if (zoraCatch != nullptr) {
                    *animOut = zoraCatch;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_ROLL: {
                // Gerudo rolls with a dual-blades tumble. OOT's roll action is
                // untouched — this only changes which clip it plays.
                LinkAnimationHeader* gerudoRoll = GerudoMhr_GetRollAnim();
                if (gerudoRoll != nullptr) {
                    *animOut = gerudoRoll;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_JUMPSLASH_RECOVERY: {
                // Jump-slash recovery: transformed + mwa in [FLIPSLASH_FINISH, JUMPSLASH_FINISH]
                if (TransformMasks_IsTransformed() && (player->meleeWeaponAnimation >= PLAYER_MWA_FLIPSLASH_FINISH) &&
                    (player->meleeWeaponAnimation <= PLAYER_MWA_JUMPSLASH_FINISH)) {
                    LinkAnimationHeader* formAnim = MmForm_GetJumpSlashAnim(player->meleeWeaponAnimation);
                    if (formAnim != nullptr) {
                        *animOut = formAnim;
                    }
                }
                break;
            }
            default:
                break;
        }
    });
}

static RegisterShipInitFunc initFuncPlayerAnimOverride(RegisterPlayerAnimOverrideNEI, {});
