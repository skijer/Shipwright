#include "z64items.h"
#include "z64wheel.h"

#include <libultraship/log/luslog.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_Obj_Syokudai/z_obj_syokudai.h"
#include "soh/ActorDB.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define SLATE_KEY "nei.sheikah_slate"
#define SLATE_HELD_SCALE 0.1f
#define SLATE_HELD_ROT_Y 180.0f
#define SLATE_HELD_ROT_X 180.0f
#define SLATE_HELD_OFFSET_Y -8.276f
#define SLATE_GIVE_SCALE 0.25f
#define SLATE_TEXT_LENGTH 512
#define SLATE_WHEEL_HOLD_FRAMES 8
#define SLATE_BUTTON_COUNT 8

#define REMOTE_BOMB_INERT 200
#define REMOTE_BOMB_TINT_R 95
#define REMOTE_BOMB_TINT_G 220
#define REMOTE_BOMB_TINT_B 235

#define CRYONIS_DIST_MIN 90.0f
#define CRYONIS_DIST_MAX 420.0f
#define CRYONIS_DIST_START 170.0f
#define CRYONIS_DIST_RATE 7.0f
#define CRYONIS_HEIGHT_MUL 1.8f
#define CRYONIS_WIDTH_MUL 0.6f
#define CRYONIS_SCALE 0.1f
#define CRYONIS_DEPTH 200.0f
#define CRYONIS_BREAK_SLACK 20.0f
#define CRYONIS_GROW_TICKS 11
#define CRYONIS_GROW_START 0.06f
#define CRYONIS_SHARDS 12
#define WALL_FLAG_CLIMBABLE (1 << 3)

typedef enum {
    SLATE_RUNE_NONE,
    SLATE_RUNE_BOMB,
    SLATE_RUNE_STASIS,
    SLATE_RUNE_CRYONIS,
    SLATE_RUNE_MASTER_CYCLE,
    SLATE_RUNE_COUNT,
} SlateRune;

typedef struct {
    const char* name;
    const char* castLine;
    const char* loreLine;
} SlateRuneText;

static const ALIGN_ASSET(2) char sSlateIconTex[] = "__OTR__textures/icon_item_custom/gItemIconSheikahSlateTex";
static const ALIGN_ASSET(2) char sBombIconTex[] = "__OTR__textures/icon_item_custom/gItemIconSheikahSlateBombTex";
static const ALIGN_ASSET(2) char sStasisIconTex[] = "__OTR__textures/icon_item_custom/gItemIconSheikahSlateStasisTex";
static const ALIGN_ASSET(2) char sCryonisIconTex[] = "__OTR__textures/icon_item_custom/gItemIconSheikahSlateCryonisTex";
static const ALIGN_ASSET(2) char sCycleIconTex[] =
    "__OTR__textures/icon_item_custom/gItemIconSheikahSlateMasterCycleTex";

static const ALIGN_ASSET(2) char sBombRuneTex[] = "__OTR__textures/icon_item_custom/gItemIconSlateRuneBombTex";
static const ALIGN_ASSET(2) char sStasisRuneTex[] = "__OTR__textures/icon_item_custom/gItemIconSlateRuneStasisTex";
static const ALIGN_ASSET(2) char sCryonisRuneTex[] = "__OTR__textures/icon_item_custom/gItemIconSlateRuneCryonisTex";
static const ALIGN_ASSET(2) char sCycleRuneTex[] = "__OTR__textures/icon_item_custom/gItemIconSlateRuneMasterCycleTex";

static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gSheikahSlateNameTex";
static const ALIGN_ASSET(2) char sSlateDL[] = "__OTR__objects/object_nei_sheikah_slate/gNeiSheikahSlateDL";
static const ALIGN_ASSET(2) char sIceBlockCol[] = "__OTR__objects/object_ice_objects/object_ice_objects_Col_0003F0";

static const Z64WheelEntry sWheelEntries[SLATE_RUNE_COUNT] = {
    { sSlateIconTex, "No Rune", sSlateIconTex },      { sBombRuneTex, "Remote Bomb", sBombIconTex },
    { sStasisRuneTex, "Stasis", sStasisIconTex },     { sCryonisRuneTex, "Cryonis", sCryonisIconTex },
    { sCycleRuneTex, "Master Cycle", sCycleIconTex },
};

static const SlateRuneText sRuneTexts[SLATE_RUNE_COUNT] = {
    { "", "Press %y\xA1%w to draw the slate. Hold&%y\xA2%w for the rune wheel.", "" },
    { "%bRemote Bomb%w",
      "%y\xA1%w draws the slate, again for a&bomb, again to set it off. Hold %y\xA2%w&for the rune wheel.",
      "A bomb of its own making, that&waits for your word and never&for a fuse." },
    { "%yStasis%w", "%y\xA1%w draws the slate, again to hold&what you aim at still. Hold %y\xA2%w for&the rune wheel.",
      "What it holds leaves time&behind, and keeps every blow&you land until it comes back." },
    { "%bCryonis%w",
      "%y\xA1%w draws the slate, again to aim;&%y\x9F%w raises a pillar of ice. Hold %y\xA2%w&for the rune wheel.",
      "It pulls a pillar of ice out of&any water, tall enough to climb." },
    { "%gMaster Cycle Zero%w", "%y\xA1%w draws the slate, again to call&the cycle. Hold %y\xA2%w for the rune&wheel.",
      "An ancient machine that&answers the slate and outruns&any horse." },
};

static const u16 sItemButtons[SLATE_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                      BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate",
                                              "OnActorDrawEnd",
                                              "OnSceneInit",
                                              "OnPlayerPostLimbDraw",
                                              "OnPlayDrawEnd",
                                              "OnPlayerFilterInput",
                                              "OnActorDraw",
                                              "OnActorResolveGrayscale",
                                              "OnBgCheckResolveWallFlags",
                                              "OnInterfaceDrawEnd",
                                              "OnLoadFile" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHCustomItemRandomizer sSlateLogic = {
    sizeof(SOHCustomItemRandomizer),
    SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT | SOH_CUSTOM_ITEM_RANDO_PROGRESSIVE,
    SOH_CUSTOM_ITEM_TYPE_ITEM,
    0,
    SLATE_RUNE_COUNT - 1,
    SLATE_KEY,
    SLATE_KEY,
    "",
};

static const SOHModApi* sApi;
static bool sIsSlateDrawn;
static MtxF sHandMatrix;
static bool sHasHandMatrix;
static s16 sWheelHoldTimer;
static u8 sShownRune = SLATE_RUNE_COUNT;
static char sPauseText[SLATE_TEXT_LENGTH];
static Actor* sRemoteBomb;
static u32 sCastFrame;

void func_80835688(Player* player, PlayState* play);
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);
void CollisionCheck_ApplyDamage(PlayState* play, CollisionCheckContext* colChkCtx, Collider* collider,
                                ColliderInfo* info);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySfxGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static u8 CurrentRune(void) {
    u32 selection = Z64Wheel_GetSelection();

    return selection < SLATE_RUNE_COUNT ? (u8)selection : SLATE_RUNE_NONE;
}

static bool IsRuneOwned(u8 rune) {
    return rune != SLATE_RUNE_NONE && Z64Wheel_IsEntryUnlocked(rune);
}

static u8 NextAvailableRune(u8 from) {
    for (u8 step = 1; step <= SLATE_RUNE_COUNT; step++) {
        u8 candidate = (from + step) % SLATE_RUNE_COUNT;

        if (candidate == SLATE_RUNE_NONE || IsRuneOwned(candidate)) {
            return candidate;
        }
    }
    return SLATE_RUNE_NONE;
}

static void OpenRuneWheel(void) {
    if (Z64Wheel_Open()) {
        return;
    }
    Z64Wheel_SetSelection(NextAvailableRune(CurrentRune()));
}

static u16 EquippedButtonMask(void) {
    u16 mask = 0;

    for (u8 button = 0; button < SLATE_BUTTON_COUNT; button++) {
        const char* key = sApi->GetEquippedCustomItem(button);

        if (key != NULL && strcmp(key, SLATE_KEY) == 0) {
            mask |= sItemButtons[button];
        }
    }
    return mask;
}

static void DescribeRune(u8 rune) {
    if (sShownRune == rune) {
        return;
    }
    snprintf(sPauseText, sizeof(sPauseText), "%%rSheikah Slate&%%w%s", sRuneTexts[rune].castLine);
    Z64Items_UpdateText(sApi, SLATE_KEY, SOH_ITEM_TEXT_PAUSE, sPauseText);
    sShownRune = rune;
}

static const char* const sPickupKeys[SLATE_RUNE_COUNT] = {
    "", "nei.slate.bomb", "nei.slate.stasis", "nei.slate.cryonis", "nei.slate.master_cycle",
};
static const char* const sVariantNames[SLATE_RUNE_COUNT] = { "", "bomb", "stasis", "cryonis", "master_cycle" };
static const u8 sRuneFlameColors[SLATE_RUNE_COUNT][3] = {
    { 0, 0, 0 }, { 100, 235, 220 }, { 255, 220, 40 }, { 175, 205, 220 }, { 255, 135, 35 },
};

static void ReceiveSlateRune(const char* itemKey, const char* variantKey) {
    for (u8 rune = SLATE_RUNE_BOMB; rune < SLATE_RUNE_COUNT; rune++) {
        if (strcmp(variantKey, sVariantNames[rune]) == 0) {
            Z64Wheel_SetEntryUnlocked(rune, true);
            Z64Wheel_SetSelection(rune);
            DescribeRune(rune);
            return;
        }
    }
}

static void GrantAllRunes(const char* key) {
    for (u8 rune = SLATE_RUNE_BOMB; rune < SLATE_RUNE_COUNT; rune++) {
        Z64Wheel_SetEntryUnlocked(rune, true);
    }
}

static void SyncReceivedRunes(void) {
    for (u8 rune = SLATE_RUNE_BOMB; rune < SLATE_RUNE_COUNT; rune++) {
        if (sApi->IsCustomItemOwned(sPickupKeys[rune])) {
            Z64Wheel_SetEntryUnlocked(rune, true);
        }
    }
}

static bool RegisterRunePickups(void) {
    for (u8 rune = SLATE_RUNE_BOMB; rune < SLATE_RUNE_COUNT; rune++) {
        SOHCustomPickupVariant pickup = Z64Items_DefinePickup(SLATE_KEY, sVariantNames[rune], sPickupKeys[rune]);
        SOHCustomItemRandomizer logic = sSlateLogic;
        char text[SLATE_TEXT_LENGTH];

        snprintf(text, sizeof(text), "You unlocked %s for&the %%rSheikah Slate%%w!^%s^%s", sRuneTexts[rune].name,
                 sRuneTexts[rune].loreLine, sRuneTexts[rune].castLine);
        logic.flags = SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT;
        logic.poolCount = 1;
        logic.logicKey = sPickupKeys[rune];
        logic.hintKey = sPickupKeys[rune];
        pickup.iconPath = sWheelEntries[rune].itemIconPath;
        pickup.getItemText = text;
        pickup.onReceive = ReceiveSlateRune;
        pickup.randomizer = &logic;
        pickup.drawSoulFlame = false;
        pickup.flameRed = sRuneFlameColors[rune][0];
        pickup.flameGreen = sRuneFlameColors[rune][1];
        pickup.flameBlue = sRuneFlameColors[rune][2];
        if (!Z64Items_RegisterPickup(sApi, &pickup)) {
            return false;
        }
    }
    return true;
}

// Never write through the pointer: after a scene change the actor is gone and its slot may be recycled.
static void ForgetRemoteBomb(void) {
    sRemoteBomb = NULL;
}

static bool LiftRemoteBomb(PlayState* play, Player* player) {
    Actor* bomb;

    if (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
        return false;
    }
    bomb = Actor_SpawnAsChild(&play->actorCtx, &player->actor, play, ACTOR_EN_BOM, player->actor.world.pos.x,
                              player->actor.world.pos.y, player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0, 0);
    if (bomb == NULL) {
        return false;
    }
    ((EnBom*)bomb)->timer = REMOTE_BOMB_INERT;
    Actor_SetScale(bomb, 0.01f);

    player->interactRangeActor = bomb;
    player->heldActor = bomb;
    player->getItemId = GI_NONE;
    player->getItemEntry = (GetItemEntry)GET_ITEM_NONE;
    player->unk_3BC.y = bomb->shape.rot.y - player->actor.shape.rot.y;
    player->stateFlags1 |= PLAYER_STATE1_CARRYING_ACTOR;
    func_80835688(player, play);

    sRemoteBomb = bomb;
    return true;
}

static bool DetonateRemoteBomb(void) {
    if (sRemoteBomb == NULL) {
        return false;
    }
    ((EnBom*)sRemoteBomb)->timer = 1;
    ForgetRemoteBomb();
    return true;
}

static bool CastRemoteBomb(PlayState* play, Player* player) {
    if (sRemoteBomb == NULL) {
        return LiftRemoteBomb(play, player);
    }
    return DetonateRemoteBomb();
}

static void TintRemoteBomb(Actor* actor, PlayState* play, Color_RGBA8* grayscale) {
    if (actor != sRemoteBomb) {
        return;
    }
    grayscale->r = REMOTE_BOMB_TINT_R;
    grayscale->g = REMOTE_BOMB_TINT_G;
    grayscale->b = REMOTE_BOMB_TINT_B;
    grayscale->a = 255;
}

static void HoldOntoRemoteBomb(bool* should, va_list args) {
    Input* input = va_arg(args, Input*);

    if (!*should || sRemoteBomb == NULL || input == NULL) {
        return;
    }
    if (input->press.button & EquippedButtonMask()) {
        *should = false;
    }
}

static void RunRemoteBomb(PlayState* play, Player* player) {
    if (sRemoteBomb == NULL) {
        return;
    }
    if (sRemoteBomb->update == NULL) {
        ForgetRemoteBomb();
        return;
    }
    if (sRemoteBomb->params != BOMB_BODY) {
        ForgetRemoteBomb();
        return;
    }
    ((EnBom*)sRemoteBomb)->timer = REMOTE_BOMB_INERT;
    sRemoteBomb->colorFilterParams = 0;
    Z64Items_KeepHeld(sApi);

    if (play->state.frames == sCastFrame) {
        return;
    }
    if (play->state.input[0].press.button & EquippedButtonMask()) {
        DetonateRemoteBomb();
    }
}

typedef struct {
    Actor* pillar;
    s32 bgId;
    u8 growTick;
    f32 waterY;
} CryonisState;

static ColliderCylinderInit sPillarColliderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_ALL,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { DMG_EXPLOSIVE | DMG_HAMMER, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 20, 40, 0, { 0, 0, 0 } },
};

static Vtx sGhostVtx[] = {
    VTX(-1, 0, -1, 0, 0, 0, 0, 0, 255), VTX(1, 0, -1, 0, 0, 0, 0, 0, 255),  VTX(1, 0, 1, 0, 0, 0, 0, 0, 255),
    VTX(-1, 0, 1, 0, 0, 0, 0, 0, 255),  VTX(-1, 1, -1, 0, 0, 0, 0, 0, 255), VTX(1, 1, -1, 0, 0, 0, 0, 0, 255),
    VTX(1, 1, 1, 0, 0, 0, 0, 0, 255),   VTX(-1, 1, 1, 0, 0, 0, 0, 0, 255),
};

static Gfx sGhostDL[] = {
    gsSPVertex(sGhostVtx, 8, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(4, 6, 5, 0, 4, 7, 6, 0),
    gsSP2Triangles(0, 5, 1, 0, 0, 4, 5, 0),
    gsSP2Triangles(1, 6, 2, 0, 1, 5, 6, 0),
    gsSP2Triangles(2, 7, 3, 0, 2, 6, 7, 0),
    gsSP2Triangles(3, 4, 0, 0, 3, 7, 4, 0),
    gsSPEndDisplayList(),
};

static CryonisState sCryonis = { NULL, BGCHECK_SCENE, 0, 0.0f };
static ColliderCylinder sPillarCollider;
static u8 sIsPillarColliderReady;
static bool sIsAiming;
static f32 sAimDistance = CRYONIS_DIST_START;
static Vec3f sAimPos;
static bool sAimIsValid;
static bool sAimBreaks;

static f32 sBlockHalfX;
static f32 sBlockHalfZ;
static f32 sBlockHeight;
static f32 sBlockLocalHeight;
static f32 sBlockMinY;
static f32 sBlockBaseOffset;
static bool sIsBlockMeasured;

static void MeasureIceBlock(CollisionHeader* header) {
    if (sIsBlockMeasured) {
        return;
    }
    if (header == NULL) {
        CollisionHeader_GetVirtual((void*)sIceBlockCol, &header);
        if (header == NULL) {
            return;
        }
    }
    sBlockHalfX = (header->maxBounds.x - header->minBounds.x) * 0.5f * CRYONIS_SCALE * CRYONIS_WIDTH_MUL;
    sBlockHalfZ = (header->maxBounds.z - header->minBounds.z) * 0.5f * CRYONIS_SCALE * CRYONIS_WIDTH_MUL;
    sBlockLocalHeight = (f32)(header->maxBounds.y - header->minBounds.y);
    sBlockHeight = sBlockLocalHeight * CRYONIS_SCALE * CRYONIS_HEIGHT_MUL;
    sBlockMinY = (f32)header->minBounds.y;
    sBlockBaseOffset = -sBlockMinY * CRYONIS_SCALE * CRYONIS_HEIGHT_MUL;
    sIsBlockMeasured = true;
}

static f32 GrowProgress(u8 tick) {
    f32 t = (f32)tick / (f32)CRYONIS_GROW_TICKS;

    return CRYONIS_GROW_START + ((1.0f - CRYONIS_GROW_START) * (1.0f - SQ(1.0f - t)));
}

static void ApplyGrowth(Actor* actor, f32 progress) {
    f32 total = CRYONIS_DEPTH + (sBlockHeight * progress);
    f32 scaleY = (sBlockLocalHeight > 0.0f) ? (total / sBlockLocalHeight) : CRYONIS_SCALE;

    actor->scale.y = scaleY;
    actor->world.pos.y = sCryonis.waterY - CRYONIS_DEPTH - (sBlockMinY * scaleY);
    sPillarCollider.dim.height = (s16)total;
    sPillarCollider.dim.yShift = (s16)(sBlockMinY * scaleY);
}

static void ForgetPillar(void) {
    sCryonis.pillar = NULL;
    sCryonis.bgId = BGCHECK_SCENE;
}

static void PrunePillar(void) {
    if ((sCryonis.pillar != NULL) && (sCryonis.pillar->update == NULL)) {
        ForgetPillar();
    }
}

static void ShatterPillar(PlayState* play) {
    Color_RGBA8 prim = { 250, 250, 250, 255 };
    Color_RGBA8 env = { 180, 200, 235, 255 };
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f pos;

    PrunePillar();
    if (sCryonis.pillar == NULL) {
        return;
    }
    pos = sCryonis.pillar->world.pos;
    PlaySfxAt(NA_SE_EV_ICE_BROKEN, &pos);
    for (s32 i = 0; i < CRYONIS_SHARDS; i++) {
        Vec3f burst = pos;
        Vec3f velocity = { Rand_CenteredFloat(4.0f), 1.0f + (Rand_ZeroOne() * 2.0f), Rand_CenteredFloat(4.0f) };

        burst.x += Rand_CenteredFloat(sBlockHalfX * 2.0f);
        burst.y += Rand_ZeroOne() * sBlockHeight;
        burst.z += Rand_CenteredFloat(sBlockHalfZ * 2.0f);
        func_8002829C(play, &burst, &velocity, &zero, &prim, &env, 250, Rand_S16Offset(40, 15));
    }
    Actor_Kill(sCryonis.pillar);
    ForgetPillar();
}

static void PillarUpdate(Actor* thisx, PlayState* play) {
    DynaPolyActor* dyna = (DynaPolyActor*)thisx;

    if (sCryonis.growTick < CRYONIS_GROW_TICKS) {
        sCryonis.growTick++;
        ApplyGrowth(thisx, GrowProgress(sCryonis.growTick));
    }
    if (dyna->unk_150 != 0.0f) {
        GET_PLAYER(play)->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
        dyna->unk_150 = 0.0f;
    }
    if (sPillarCollider.base.acFlags & AC_HIT) {
        sPillarCollider.base.acFlags &= ~AC_HIT;
        ShatterPillar(play);
        return;
    }
    Collider_UpdateCylinder(thisx, &sPillarCollider);
    CollisionCheck_SetAC(play, &play->colChkCtx, &sPillarCollider.base);
}

static void MakePillarClimbable(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId, uint32_t* flags) {
    // The pointer may already be freed on the first frame of a new scene: compare it, never read through it.
    if ((sCryonis.pillar != NULL) && (bgId == sCryonis.bgId)) {
        *flags |= WALL_FLAG_CLIMBABLE;
    }
}

static s16 AimYaw(PlayState* play, Player* player) {
    Vec3f eye = play->view.eye;
    Vec3f at = play->view.lookAt;

    if ((fabsf(at.x - eye.x) < 0.001f) && (fabsf(at.z - eye.z) < 0.001f)) {
        return player->actor.shape.rot.y;
    }
    return Math_Vec3f_Yaw(&eye, &at);
}

static bool AimIsOnPillar(Vec3f* pos) {
    f32 reach = fmaxf(sBlockHalfX, sBlockHalfZ) + CRYONIS_BREAK_SLACK;
    f32 dx;
    f32 dz;

    if (sCryonis.pillar == NULL) {
        return false;
    }
    dx = pos->x - sCryonis.pillar->world.pos.x;
    dz = pos->z - sCryonis.pillar->world.pos.z;
    return (SQ(dx) + SQ(dz)) < SQ(reach);
}

static bool AimPathIsClear(PlayState* play, Player* player, Vec3f* pos) {
    Vec3f from = player->actor.world.pos;
    Vec3f to = *pos;
    Vec3f hit;
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;

    from.y += 40.0f;
    to.y += 20.0f;
    return !BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true, &bgId);
}

static void UpdateAim(PlayState* play, Player* player) {
    s16 yaw = AimYaw(play, player);
    WaterBox* waterBox = NULL;
    f32 waterY;
    Vec3f pos;

    pos.x = player->actor.world.pos.x + (Math_SinS(yaw) * sAimDistance);
    pos.y = player->actor.world.pos.y;
    pos.z = player->actor.world.pos.z + (Math_CosS(yaw) * sAimDistance);

    if (!WaterBox_GetSurface1(play, &play->colCtx, pos.x, pos.z, &waterY, &waterBox)) {
        sAimPos = pos;
        sAimIsValid = false;
        sAimBreaks = false;
        return;
    }
    pos.y = waterY;
    sAimPos = pos;
    sAimBreaks = AimIsOnPillar(&pos);
    sAimIsValid = sAimBreaks || AimPathIsClear(play, player, &pos);
}

static void LeaveAiming(bool isCancelled) {
    if (!sIsAiming) {
        return;
    }
    sIsAiming = false;
    sAimIsValid = false;
    sAimBreaks = false;
    sApi->ReleasePlayerInput(SLATE_KEY);
    if (isCancelled) {
        PlaySfxAt(NA_SE_SY_CANCEL, &gSfxDefaultPos);
    }
}

static bool RaisePillar(PlayState* play, Vec3f* pos) {
    Actor* actor;

    MeasureIceBlock(NULL);
    if (Object_GetIndex(&play->objectCtx, OBJECT_ICE_OBJECTS) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_ICE_OBJECTS);
    }
    actor =
        Actor_Spawn(&play->actorCtx, play, ACTOR_BG_ICE_OBJECTS, pos->x, pos->y + sBlockBaseOffset, pos->z, 0, 0, 0, 0);
    if ((actor == NULL) || (actor->update == NULL)) {
        return false;
    }
    actor->update = PillarUpdate;
    actor->scale.x *= CRYONIS_WIDTH_MUL;
    actor->scale.z *= CRYONIS_WIDTH_MUL;

    if (!sIsPillarColliderReady) {
        Collider_InitCylinder(play, &sPillarCollider);
        sIsPillarColliderReady = 1;
    }
    Collider_SetCylinder(play, &sPillarCollider, actor, &sPillarColliderInit);
    sPillarCollider.dim.radius = (s16)fmaxf(sBlockHalfX, sBlockHalfZ);

    sCryonis.waterY = pos->y;
    sCryonis.growTick = 0;
    ApplyGrowth(actor, GrowProgress(0));

    sCryonis.pillar = actor;
    sCryonis.bgId = ((DynaPolyActor*)actor)->bgId;
    return true;
}

static bool CastCryonis(PlayState* play, Player* player) {
    if (sIsAiming) {
        LeaveAiming(true);
        return true;
    }
    MeasureIceBlock(NULL);
    sIsAiming = true;
    sAimDistance = CRYONIS_DIST_START;
    UpdateAim(play, player);
    sApi->BlockPlayerInput(SLATE_KEY, BTN_A | BTN_B | BTN_DUP | BTN_DDOWN, false);
    PlaySfxAt(NA_SE_SY_GET_ITEM, &gSfxDefaultPos);
    return true;
}

static bool RunCryonis(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];
    u16 held;

    if (!sIsAiming) {
        return false;
    }
    if (!sIsSlateDrawn || (player->stateFlags1 & PLAYER_STATE1_IN_WATER) || (player->meleeWeaponState != 0)) {
        LeaveAiming(true);
        return false;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        LeaveAiming(true);
        return true;
    }
    held = input->cur.button;
    if (held & BTN_DUP) {
        sAimDistance += CRYONIS_DIST_RATE;
    }
    if (held & BTN_DDOWN) {
        sAimDistance -= CRYONIS_DIST_RATE;
    }
    sAimDistance = CLAMP(sAimDistance, CRYONIS_DIST_MIN, CRYONIS_DIST_MAX);
    UpdateAim(play, player);

    if (!CHECK_BTN_ALL(input->press.button, BTN_A)) {
        return true;
    }
    if (sAimBreaks) {
        ShatterPillar(play);
        LeaveAiming(false);
        return true;
    }
    if (!sAimIsValid) {
        PlaySfxAt(NA_SE_SY_ERROR, &player->actor.world.pos);
        return true;
    }
    ShatterPillar(play);
    if (!RaisePillar(play, &sAimPos)) {
        PlaySfxAt(NA_SE_SY_ERROR, &player->actor.world.pos);
    }
    LeaveAiming(false);
    return true;
}

static void WatchPillarForTrouble(Player* player) {
    bool isCrushed = (player->actor.bgCheckFlags & 0x100) != 0;
    bool hasNoFloor = (player->stateFlags1 & PLAYER_STATE1_FLOOR_DISABLED) != 0;

    if (sCryonis.pillar == NULL || (!isCrushed && !hasNoFloor)) {
        return;
    }
    LUSLOG_WARN("cryonis: %s at y %.1f (water %.1f, floor %.1f); pillar y %.1f scale %.3f bg %d",
                isCrushed ? "crushed" : "no floor", player->actor.world.pos.y, player->actor.yDistToWater,
                player->actor.floorHeight, sCryonis.pillar->world.pos.y, sCryonis.pillar->scale.y, sCryonis.bgId);
}

static void DrawAimGhost(void) {
    PlayState* play = gPlayState;
    f32 pulse;

    if (!sIsAiming || !sIsBlockMeasured || play == NULL) {
        return;
    }
    pulse = 0.94f + (0.06f * Math_SinS((s16)(play->gameplayFrames * 1500)));

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(sAimPos.x, sAimPos.y, sAimPos.z, MTXMODE_NEW);
    Matrix_Scale(sBlockHalfX * pulse, sBlockHeight * pulse, sBlockHalfZ * pulse, MTXMODE_APPLY);
    if (sAimBreaks) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 150, 90, 120);
        gDPSetEnvColor(POLY_XLU_DISP++, 180, 60, 0, 120);
    } else if (sAimIsValid) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 150, 215, 255, 110);
        gDPSetEnvColor(POLY_XLU_DISP++, 20, 90, 180, 110);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 70, 70, 110);
        gDPSetEnvColor(POLY_XLU_DISP++, 150, 0, 0, 110);
    }
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, sGhostDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Never NULL an actor's update: the engine treats it as the dead marker. Culling must be disabled or the
// replacement stops running when the player looks away.

#define STASIS_FRAMES_OBJECT 200
#define STASIS_SFX_RATE_ENEMY 2.0f
#define STASIS_FRAMES_ENEMY ((s16)(STASIS_FRAMES_OBJECT / STASIS_SFX_RATE_ENEMY))
#define STASIS_CHAIN_FRAMES 20
#define STASIS_CHAIN_FRAMES_ENEMY ((s16)(STASIS_CHAIN_FRAMES / STASIS_SFX_RATE_ENEMY))
#define STASIS_FLIGHT_FRAMES 90
#define STASIS_RANGE 520.0f
#define STASIS_CONE 0x1800
#define STASIS_MIN_DIST 30.0f
#define STASIS_LAUNCH_BASE 8.0f
#define STASIS_LAUNCH_PER_DAMAGE 1.8f
#define STASIS_LAUNCH_PER_HIT 3.0f
#define STASIS_LAUNCH_MAX 42.0f
#define STASIS_LAUNCH_VEL_Y 6.0f
#define STASIS_GRAVITY -1.4f
#define STASIS_MIN_VELOCITY_Y -30.0f
#define STASIS_RECOIL_SPEED 7.0f
#define STASIS_RECOIL_HEIGHT 3.5f
#define STASIS_SLIDE_FRICTION 0.93f
#define STASIS_MIN_HEIGHT 25.0f
#define STASIS_CLIMBABLE_MIN_HEIGHT 70.0f
#define STASIS_CLIMB_MIN_HALF_HEIGHT 24.0f
#define STASIS_MAX_OWN_COLLIDERS 4
#define STASIS_SFX_RELEASE_SECONDS 2.5f
#define STASIS_FIRE_DAMAGE 4
#define STASIS_FIRE_EFFECT 1
#define STASIS_FIRE_PAD 12.0f

#define STASIS_TINT_R 255
#define STASIS_TINT_G 235
#define STASIS_TINT_B 90
#define STASIS_TINT_HOT_R 255
#define STASIS_TINT_HOT_G 70
#define STASIS_TINT_HOT_B 40

#define STASIS_BG_GROUND (1 << 0)
#define STASIS_BG_WALL (1 << 3)
#define STASIS_BG_CEILING (1 << 4)

typedef enum {
    STASIS_KIND_NONE,
    STASIS_KIND_ENEMY,
    STASIS_KIND_PROP,
    STASIS_KIND_BLOCK,
    STASIS_KIND_CLIMBABLE,
} StasisKind;

typedef enum {
    STASIS_PHASE_FROZEN,
    STASIS_PHASE_FLYING,
} StasisPhase;

typedef struct {
    Actor* actor;
    u8 kind;
    u8 phase;
    s16 timer;
    s16 chainTimer;
    s16 age;
    u16 accumDamage;
    u32 lastDmgFlags;
    u8 lastDmgEffect;
    u8 lastDmgAmount;
    f32 force;
    Vec3f hitDir;
    u8 hasHitDir;
    s32 bgId;
    u8 bgIsOurs;
    ActorFunc origUpdate;
    u32 origFlags;
    f32 origGravity;
    f32 origMinVelocityY;
    Vec3s origShapeRot;
    Vec3s origWorldRot;
    s16 origRoom;
    u8 origMass;
    ColliderCylinder collider;
    u8 colliderReady;
    ColliderCylinder* ownCollider[STASIS_MAX_OWN_COLLIDERS];
    u8 ownColliderCount;
    u8 extraCollider;
    u8 riderAttached;
    Vec3f riderOffset;
    u8 slideLaunch;
} StasisState;

static StasisState sStasis;
static Actor* sOfferActor;
static ColliderCylinder sFireCollider;
static Actor* sFireOwner;

static void MakePresser(Actor* actor);

static void StasisSfx_Play(f32 rate, f32 volume);
static void StasisSfx_Stop(void);
static void StasisSfx_SeekToTail(f32 seconds);

static Lights1 sStasisLights = gdSPDefLights1(210, 205, 175, 255, 250, 220, 0x28, 0x28, 0x28);

static ColliderCylinderInit sStasisColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x08 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 40, 60, 0, { 0, 0, 0 } },
};

static const s16 sBlockIds[] = {
    ACTOR_OBJ_OSHIHIKI,    ACTOR_OBJ_KIBAKO2,      ACTOR_OBJ_HSBLOCK,        ACTOR_OBJ_TIMEBLOCK,  ACTOR_OBJ_WARP2BLOCK,
    ACTOR_BG_JYA_BLOCK,    ACTOR_BG_GND_ICEBLOCK,  ACTOR_BG_SPOT08_ICEBLOCK, ACTOR_BG_PUSHBOX,     ACTOR_BG_HEAVY_BLOCK,
    ACTOR_OBJ_LIFT,        ACTOR_OBJ_ELEVATOR,     ACTOR_BG_HIDAN_SYOKU,     ACTOR_BG_DDAN_JD,     ACTOR_BG_JYA_1FLIFT,
    ACTOR_BG_JYA_LIFT,     ACTOR_BG_MORI_ELEVATOR, ACTOR_BG_ICE_SHELTER,     ACTOR_BG_ICE_OBJECTS, ACTOR_BG_MORI_BIGST,
    ACTOR_BG_SPOT15_RRBOX,
};

static const s16 sPropIds[] = {
    ACTOR_OBJ_BOMBIWA,       ACTOR_OBJ_HAMISHI,    ACTOR_BG_SPOT16_BOMBSTONE,
    ACTOR_BG_SPOT01_IDOSOKO, ACTOR_BG_HAKA,        ACTOR_BG_MENKURI_EYE,
    ACTOR_OBJ_ICE_POLY,      ACTOR_BG_HIDAN_ROCK,  ACTOR_BG_ICE_TURARA,
    ACTOR_EN_GOROIWA,        ACTOR_BG_JYA_GOROIWA, ACTOR_BG_JYA_HAHENIRON,
    ACTOR_BG_GANON_OTYUKA,   ACTOR_BG_HAKA_TRAP,   ACTOR_EN_TRAP,
    ACTOR_OBJ_TSUBO,         ACTOR_OBJ_KIBAKO,     ACTOR_EN_KUSA,
    ACTOR_OBJ_COMB,          ACTOR_EN_KANBAN,      ACTOR_BG_HAKA_TUBO,
    ACTOR_BG_SPOT18_BASKET,  ACTOR_EN_NIW,         ACTOR_EN_BOM,
    ACTOR_EN_BOMBF,          ACTOR_EN_BOM_CHU,     ACTOR_OBJ_SYOKUDAI,
    ACTOR_BG_PO_SYOKUDAI,    ACTOR_EN_ICE_HONO,
};

static const s16 sEnemyBlacklist[] = {
    ACTOR_EN_IK, ACTOR_EN_TORCH2, ACTOR_EN_ZF, ACTOR_EN_WALLMAS, ACTOR_EN_FLOORMAS, ACTOR_EN_RD,
};

static const s16 sStructureIds[] = {
    ACTOR_BG_MORI_KAITENKABE, ACTOR_BG_MIZU_WATER,   ACTOR_BG_JYA_ZURERUKABE,  ACTOR_BG_JYA_AMISHUTTER,
    ACTOR_BG_HIDAN_HAMSTEP,   ACTOR_BG_MORI_HINERI,  ACTOR_BG_MORI_RAKKATENJO, ACTOR_BG_MORI_IDOMIZU,
    ACTOR_BG_MORI_HASHIRA4,   ACTOR_BG_MORI_HASHIGO, ACTOR_BG_MIZU_MOVEBG,     ACTOR_BG_HAKA_SHIP,
};

static const s16 sKeepsPoseIds[] = {
    ACTOR_OBJ_OSHIHIKI, ACTOR_EN_AM, ACTOR_BG_ICE_OBJECTS, ACTOR_BG_GND_ICEBLOCK, ACTOR_OBJ_ELEVATOR,
};

static u8 IsIdInList(s16 id, const s16* list, s32 count) {
    for (s32 i = 0; i < count; i++) {
        if (list[i] == id) {
            return 1;
        }
    }
    return 0;
}

static u8 IsBodyBurning(Actor* actor) {
    if (actor->id == ACTOR_EN_ICE_HONO) {
        return 1;
    }
    if ((actor->id == ACTOR_OBJ_SYOKUDAI) || (actor->id == ACTOR_BG_PO_SYOKUDAI)) {
        return ((ObjSyokudai*)actor)->litTimer != 0;
    }
    return 0;
}

#define STASIS_MAX_RINGS 4
#define STASIS_RING_SIDES 8
#define STASIS_MAX_STACKS 2
#define STASIS_BOX_VTX (STASIS_RING_SIDES * STASIS_MAX_RINGS)
#define STASIS_BOX_POLY ((STASIS_RING_SIDES * 2 * (STASIS_MAX_RINGS - 1)) + ((STASIS_RING_SIDES - 2) * 2))
#define STASIS_FALLBACK_RADIUS 45
#define STASIS_FALLBACK_HEIGHT 90
#define STASIS_OCT 0.70710678f

typedef struct {
    s16 y;
    s16 radius;
} StasisRing;

typedef struct {
    u8 first;
    u8 count;
    u8 capBottom;
    u8 capTop;
} StasisStack;

typedef struct {
    s16 id;
    s16 paramsMax;
    u8 ringCount;
    StasisRing ring[STASIS_MAX_RINGS];
    u8 stackCount;
    StasisStack stack[STASIS_MAX_STACKS];
} StasisBody;

static const StasisBody sStasisBodies[] = {
    { ACTOR_EN_WOOD02,
      0x04,
      4,
      { { 0, 12 }, { 480, 6 }, { 108, 180 }, { 480, 4 } },
      2,
      { { 0, 2, 1, 1 }, { 2, 2, 0, 0 } } },
    { ACTOR_EN_WOOD02,
      0x09,
      4,
      { { 0, 12 }, { 659, 6 }, { 108, 166 }, { 659, 4 } },
      2,
      { { 0, 2, 1, 1 }, { 2, 2, 0, 0 } } },
    { ACTOR_EN_WOOD02,
      0x0A,
      4,
      { { 0, 64 }, { 593, 6 }, { 193, 160 }, { 593, 4 } },
      2,
      { { 0, 2, 1, 1 }, { 2, 2, 0, 0 } } },
    { ACTOR_OBJ_BOMBIWA, -1, 4, { { -123, 31 }, { 0, 405 }, { 387, 514 }, { 773, 256 } }, 1, { { 0, 4, 1, 1 } } },
    { ACTOR_OBJ_HAMISHI, -1, 4, { { -111, 10 }, { -80, 101 }, { 16, 127 }, { 113, 64 } }, 1, { { 0, 4, 1, 1 } } },
    { ACTOR_EN_ISHI, -1, 4, { { -111, 10 }, { -80, 101 }, { 16, 127 }, { 113, 64 } }, 1, { { 0, 4, 1, 1 } } },
};

static const f32 sRingX[STASIS_RING_SIDES] = {
    1.0f, STASIS_OCT, 0.0f, -STASIS_OCT, -1.0f, -STASIS_OCT, 0.0f, STASIS_OCT
};
static const f32 sRingZ[STASIS_RING_SIDES] = {
    0.0f, STASIS_OCT, 1.0f, STASIS_OCT, 0.0f, -STASIS_OCT, -1.0f, -STASIS_OCT
};

static Vec3i sStasisVtxPool[STASIS_BOX_VTX];
static CollisionPoly sStasisPolyPool[STASIS_BOX_POLY];
static SurfaceType sStasisSurfPool[1];
// The camera reads cameraDataList[camId] with no NULL check: one neutral entry, every poly points at 0.
static CamData sStasisCamData[1];
static CollisionHeader sStasisHeader;

// The engine writes through a DynaPolyActor cast on everything in bgActors[], so a foreign actor must
// never be registered directly: this carrier is registered instead.
static DynaPolyActor sStasisCarrier;

static void CarrierUpdate(Actor* thisx, PlayState* play) {
}

static void SyncCarrier(Actor* target) {
    sStasisCarrier.actor.world.pos = target->world.pos;
    sStasisCarrier.actor.home.pos = target->world.pos;
    sStasisCarrier.actor.prevPos = target->world.pos;
    sStasisCarrier.actor.shape.rot = target->shape.rot;
    sStasisCarrier.actor.world.rot = target->shape.rot;
}

static void MakePoly(CollisionPoly* poly, s32 ia, s32 ib, s32 ic) {
    Vec3f a = { (f32)sStasisVtxPool[ia].x, (f32)sStasisVtxPool[ia].y, (f32)sStasisVtxPool[ia].z };
    Vec3f b = { (f32)sStasisVtxPool[ib].x, (f32)sStasisVtxPool[ib].y, (f32)sStasisVtxPool[ib].z };
    Vec3f c = { (f32)sStasisVtxPool[ic].x, (f32)sStasisVtxPool[ic].y, (f32)sStasisVtxPool[ic].z };
    Vec3f e1 = { b.x - a.x, b.y - a.y, b.z - a.z };
    Vec3f e2 = { c.x - a.x, c.y - a.y, c.z - a.z };
    Vec3f n = { (e1.y * e2.z) - (e1.z * e2.y), (e1.z * e2.x) - (e1.x * e2.z), (e1.x * e2.y) - (e1.y * e2.x) };
    f32 len = sqrtf(SQ(n.x) + SQ(n.y) + SQ(n.z));

    poly->type = 0;
    poly->flags_vIA = (u32)ia;
    poly->flags_vIB = (u32)ib;
    poly->vIC = (u32)ic;
    if (len <= 0.001f) {
        return;
    }
    n.x /= len;
    n.y /= len;
    n.z /= len;
    poly->normal.x = (s16)(n.x * 32767.0f);
    poly->normal.y = (s16)(n.y * 32767.0f);
    poly->normal.z = (s16)(n.z * 32767.0f);
    poly->dist = (s32) - ((n.x * a.x) + (n.y * a.y) + (n.z * a.z));
}

static s32 GetBodyProfile(Actor* actor, StasisRing* rings, StasisStack* stacks, s32* stackCount) {
    PlayState* play = gPlayState;
    f32 sx = fabsf(actor->scale.x);
    f32 sy = fabsf(actor->scale.y);

    if (sx < 0.0001f) {
        sx = 1.0f;
    }
    if (sy < 0.0001f) {
        sy = 1.0f;
    }
    for (s32 i = 0; i < (s32)ARRAY_COUNT(sStasisBodies); i++) {
        const StasisBody* body = &sStasisBodies[i];

        if (body->id != actor->id) {
            continue;
        }
        if ((body->paramsMax >= 0) && ((actor->params & 0xFF) > body->paramsMax)) {
            continue;
        }
        for (s32 j = 0; j < body->ringCount; j++) {
            rings[j].y = (s16)((f32)body->ring[j].y * sy);
            rings[j].radius = (s16)((f32)body->ring[j].radius * sx);
            if (rings[j].radius < 3) {
                rings[j].radius = 3;
            }
        }
        for (s32 j = 0; j < body->stackCount; j++) {
            stacks[j] = body->stack[j];
            for (s32 k = stacks[j].first + 1; k < stacks[j].first + stacks[j].count; k++) {
                if (rings[k].y <= rings[k - 1].y) {
                    rings[k].y = rings[k - 1].y + 1;
                }
            }
        }
        *stackCount = body->stackCount;
        return body->ringCount;
    }

    if (play != NULL) {
        for (s32 bg = 0; bg < play->colCtx.dyna.bgActorMax; bg++) {
            BgActor* bgActor = &play->colCtx.dyna.bgActors[bg];
            CollisionHeader* header = bgActor->colHeader;

            if (!(play->colCtx.dyna.bgActorFlags[bg] & 1) || (bgActor->actor != actor) || (header == NULL)) {
                continue;
            }
            f32 rx = (f32)(header->maxBounds.x - header->minBounds.x) * 0.5f * sx;
            f32 rz = (f32)(header->maxBounds.z - header->minBounds.z) * 0.5f * sx;

            rings[0].y = (s16)((f32)header->minBounds.y * sy);
            rings[1].y = (s16)((f32)header->maxBounds.y * sy);
            rings[0].radius = (s16)((rx > rz) ? rx : rz);
            rings[1].radius = rings[0].radius;
            if (rings[0].radius < 3) {
                rings[0].radius = rings[1].radius = 3;
            }
            if (rings[1].y <= rings[0].y) {
                rings[1].y = (s16)(rings[0].y + (rings[0].radius * 2));
            }
            stacks[0].first = 0;
            stacks[0].count = 2;
            stacks[0].capBottom = 1;
            stacks[0].capTop = 1;
            *stackCount = 1;
            return 2;
        }
    }

    rings[0].y = 0;
    rings[0].radius = (actor->colChkInfo.cylRadius > 0) ? actor->colChkInfo.cylRadius : STASIS_FALLBACK_RADIUS;
    rings[1].y = (actor->colChkInfo.cylHeight > 0) ? actor->colChkInfo.cylHeight : STASIS_FALLBACK_HEIGHT;
    rings[1].radius = rings[0].radius;
    if (rings[1].y <= rings[0].y) {
        rings[1].y = (s16)(rings[0].radius * 2);
    }
    stacks[0].first = 0;
    stacks[0].count = 2;
    stacks[0].capBottom = 1;
    stacks[0].capTop = 1;
    *stackCount = 1;
    return 2;
}

static void MeasureBody(Actor* actor, f32* loY, f32* hiY, f32* maxR, f32* maxRY) {
    StasisRing ring[STASIS_MAX_RINGS];
    StasisStack stack[STASIS_MAX_STACKS];
    s32 stackCount;
    s32 count = GetBodyProfile(actor, ring, stack, &stackCount);

    *loY = *hiY = (f32)ring[0].y;
    *maxR = (f32)ring[0].radius;
    *maxRY = (f32)ring[0].y;
    for (s32 i = 1; i < count; i++) {
        if ((f32)ring[i].y < *loY) {
            *loY = (f32)ring[i].y;
        }
        if ((f32)ring[i].y > *hiY) {
            *hiY = (f32)ring[i].y;
        }
        if ((f32)ring[i].radius > *maxR) {
            *maxR = (f32)ring[i].radius;
            *maxRY = (f32)ring[i].y;
        }
    }
}

static s32 BuildBodyCollision(PlayState* play, Actor* actor, u8 climbable) {
    StasisRing ring[STASIS_MAX_RINGS];
    StasisStack stack[STASIS_MAX_STACKS];
    s32 stackCount;
    s32 count = GetBodyProfile(actor, ring, stack, &stackCount);
    s32 nVtx = 0;
    s32 nPoly = 0;
    s16 minY;
    s16 maxY;
    s16 maxR;

    if ((count < 2) || (stackCount < 1)) {
        return -1;
    }
    minY = maxY = ring[0].y;
    maxR = ring[0].radius;
    for (s32 r = 0; r < count; r++) {
        for (s32 i = 0; i < STASIS_RING_SIDES; i++) {
            sStasisVtxPool[nVtx].x = (s32)(sRingX[i] * (f32)ring[r].radius);
            sStasisVtxPool[nVtx].y = ring[r].y;
            sStasisVtxPool[nVtx].z = (s32)(sRingZ[i] * (f32)ring[r].radius);
            nVtx++;
        }
        if (ring[r].radius > maxR) {
            maxR = ring[r].radius;
        }
        if (ring[r].y < minY) {
            minY = ring[r].y;
        }
        if (ring[r].y > maxY) {
            maxY = ring[r].y;
        }
    }

    for (s32 s = 0; s < stackCount; s++) {
        s32 base = stack[s].first;
        s32 last = base + stack[s].count - 1;

        for (s32 r = base; r < last; r++) {
            s32 lo = r * STASIS_RING_SIDES;
            s32 hi = (r + 1) * STASIS_RING_SIDES;

            for (s32 i = 0; i < STASIS_RING_SIDES; i++) {
                s32 j = (i + 1) % STASIS_RING_SIDES;

                MakePoly(&sStasisPolyPool[nPoly++], lo + i, hi + i, hi + j);
                MakePoly(&sStasisPolyPool[nPoly++], lo + i, hi + j, lo + j);
            }
        }
        for (s32 i = 1; i + 1 < STASIS_RING_SIDES; i++) {
            if (stack[s].capBottom) {
                s32 lo = base * STASIS_RING_SIDES;

                MakePoly(&sStasisPolyPool[nPoly++], lo, lo + i, lo + i + 1);
            }
            if (stack[s].capTop) {
                s32 hi = last * STASIS_RING_SIDES;

                MakePoly(&sStasisPolyPool[nPoly++], hi, hi + i + 1, hi + i);
            }
        }
    }

    // The engine's wall-flag getter reads this whatever the hook answers, so clear it when not climbable.
    memset(&sStasisSurfPool[0], 0, sizeof(sStasisSurfPool[0]));
    sStasisSurfPool[0].wallType = climbable ? 4 : 0;

    sStasisHeader.minBounds.x = -maxR;
    sStasisHeader.minBounds.y = minY;
    sStasisHeader.minBounds.z = -maxR;
    sStasisHeader.maxBounds.x = maxR;
    sStasisHeader.maxBounds.y = maxY;
    sStasisHeader.maxBounds.z = maxR;
    sStasisHeader.numVertices = (u32)nVtx;
    sStasisHeader.vtxList = sStasisVtxPool;
    sStasisHeader.numPolygons = (u32)nPoly;
    sStasisHeader.polyList = sStasisPolyPool;
    sStasisHeader.surfaceTypeList = sStasisSurfPool;
    sStasisCamData[0].cameraSType = 0;
    sStasisCamData[0].numCameras = 0;
    sStasisCamData[0].camPosData = NULL;
    sStasisHeader.cameraDataList = sStasisCamData;
    sStasisHeader.cameraDataListLen = 1;
    sStasisHeader.numWaterBoxes = 0;
    sStasisHeader.waterBoxes = NULL;

    memset(&sStasisCarrier, 0, sizeof(sStasisCarrier));
    sStasisCarrier.actor.update = CarrierUpdate;
    sStasisCarrier.actor.scale.x = sStasisCarrier.actor.scale.y = sStasisCarrier.actor.scale.z = 1.0f;
    SyncCarrier(actor);

    return DynaPoly_SetBgActor(play, &play->colCtx.dyna, &sStasisCarrier.actor, &sStasisHeader);
}

static CollisionHeader* FindBodyBg(PlayState* play, Actor* actor, s32* bgIdOut) {
    if (play == NULL || actor == NULL) {
        return NULL;
    }
    for (s32 i = 0; i < play->colCtx.dyna.bgActorMax; i++) {
        BgActor* bg = &play->colCtx.dyna.bgActors[i];

        if ((bg->actor == actor) && (bg->colHeader != NULL)) {
            if (bgIdOut != NULL) {
                *bgIdOut = i;
            }
            return bg->colHeader;
        }
    }
    return NULL;
}

static s32 ScanOwnColliders(Actor* actor, ColliderCylinder** out, s32 max) {
    ActorDBEntry* entry = ActorDB_Retrieve(actor->id);
    size_t size = entry != NULL ? entry->instanceSize : 0;
    u8* base = (u8*)actor;
    s32 count = 0;

    if (size <= sizeof(Actor)) {
        return 0;
    }
    for (size_t off = sizeof(Actor); (off + sizeof(ColliderCylinder)) <= size; off += 4) {
        ColliderCylinder* cyl = (ColliderCylinder*)(base + off);

        if ((cyl->base.actor != actor) || (cyl->base.shape != COLSHAPE_CYLINDER)) {
            continue;
        }
        if ((cyl->dim.radius <= 0) || (cyl->dim.radius > 4000)) {
            continue;
        }
        if (count < max) {
            out[count++] = cyl;
        }
    }
    return count;
}

static f32 MeasureBodyHeight(PlayState* play, Actor* actor) {
    ColliderCylinder* cyls[STASIS_MAX_OWN_COLLIDERS];
    CollisionHeader* header;
    f32 tallest = 0.0f;
    s32 count;

    for (s32 i = 0; i < (s32)ARRAY_COUNT(sStasisBodies); i++) {
        if (sStasisBodies[i].id != actor->id) {
            continue;
        }
        if ((sStasisBodies[i].paramsMax >= 0) && ((actor->params & 0xFF) > sStasisBodies[i].paramsMax)) {
            continue;
        }
        return (f32)(sStasisBodies[i].ring[sStasisBodies[i].ringCount - 1].y - sStasisBodies[i].ring[0].y) *
               fabsf(actor->scale.y);
    }
    header = FindBodyBg(play, actor, NULL);
    if (header != NULL) {
        return (f32)(header->maxBounds.y - header->minBounds.y) * fabsf(actor->scale.y);
    }
    count = ScanOwnColliders(actor, cyls, STASIS_MAX_OWN_COLLIDERS);
    for (s32 i = 0; i < count; i++) {
        if ((f32)cyls[i]->dim.height > tallest) {
            tallest = (f32)cyls[i]->dim.height;
        }
    }
    return tallest;
}

static u8 IsFreezableEnemy(Actor* actor) {
    if (actor->category != ACTORCAT_ENEMY) {
        return 0;
    }
    if (actor->colChkInfo.mass == MASS_IMMOVABLE) {
        return 0;
    }
    return !IsIdInList(actor->id, sEnemyBlacklist, ARRAY_COUNT(sEnemyBlacklist));
}

static u8 IsFreezableProp(Actor* actor) {
    if (actor->id == ACTOR_EN_ISHI) {
        return (actor->params & 1) == 1;
    }
    if (actor->id == ACTOR_EN_WOOD02) {
        return (actor->params & 0xFF) <= 0x0A;
    }
    return IsIdInList(actor->id, sPropIds, ARRAY_COUNT(sPropIds));
}

static u8 IsNpcSafeToFreeze(PlayState* play, Actor* actor) {
    Player* player = GET_PLAYER(play);

    if ((player != NULL) && (player->talkActor == actor)) {
        return 0;
    }
    if (Player_InCsMode(play) || (play->csCtx.state != CS_STATE_IDLE)) {
        return 0;
    }
    return Message_GetState(&play->msgCtx) == TEXT_STATE_NONE;
}

static u8 ClassifyBody(PlayState* play, Actor* actor, s32* bgIdOut) {
    CollisionHeader* header;
    s32 bgId = -1;

    if (bgIdOut != NULL) {
        *bgIdOut = -1;
    }
    if ((actor == NULL) || (actor->update == NULL) || (actor->id == ACTOR_PLAYER) || actor == sCryonis.pillar) {
        return STASIS_KIND_NONE;
    }
    if (actor->category == ACTORCAT_BOSS) {
        return STASIS_KIND_NONE;
    }
    if (actor->draw == NULL) {
        return STASIS_KIND_NONE;
    }
    if ((actor->parent != NULL) || (actor->child != NULL)) {
        return STASIS_KIND_NONE;
    }
    if (IsIdInList(actor->id, sStructureIds, ARRAY_COUNT(sStructureIds))) {
        return STASIS_KIND_NONE;
    }
    if ((actor->category == ACTORCAT_NPC) && !IsNpcSafeToFreeze(play, actor)) {
        return STASIS_KIND_NONE;
    }
    if (MeasureBodyHeight(play, actor) < STASIS_MIN_HEIGHT) {
        return STASIS_KIND_NONE;
    }
    if (IsFreezableEnemy(actor)) {
        return STASIS_KIND_ENEMY;
    }
    if (IsIdInList(actor->id, sBlockIds, ARRAY_COUNT(sBlockIds))) {
        FindBodyBg(play, actor, bgIdOut);
        return STASIS_KIND_BLOCK;
    }
    if (IsFreezableProp(actor)) {
        return STASIS_KIND_PROP;
    }
    header = FindBodyBg(play, actor, &bgId);
    if (header != NULL) {
        f32 halfHeight = (f32)(header->maxBounds.y - header->minBounds.y) * 0.5f * actor->scale.y;

        if (bgIdOut != NULL) {
            *bgIdOut = bgId;
        }
        return (halfHeight >= STASIS_CLIMB_MIN_HALF_HEIGHT) ? STASIS_KIND_CLIMBABLE : STASIS_KIND_BLOCK;
    }
    return STASIS_KIND_PROP;
}

static const u8 sStasisCats[] = {
    ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_BG, ACTORCAT_NPC, ACTORCAT_EXPLOSIVE, ACTORCAT_MISC,
};

static Actor* ScanForBody(PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* best = NULL;
    s32 bestYawErr = STASIS_CONE;

    for (s32 i = 0; i < (s32)ARRAY_COUNT(sStasisCats); i++) {
        Actor* actor = play->actorCtx.actorLists[sStasisCats[i]].head;

        while (actor != NULL) {
            f32 distXZ = sqrtf(SQ(actor->world.pos.x - player->actor.world.pos.x) +
                               SQ(actor->world.pos.z - player->actor.world.pos.z));

            if ((distXZ > STASIS_MIN_DIST) && (distXZ <= STASIS_RANGE) &&
                (ClassifyBody(play, actor, NULL) != STASIS_KIND_NONE)) {
                s32 yawErr =
                    (s16)(Math_Vec3f_Yaw(&player->actor.world.pos, &actor->world.pos) - player->actor.shape.rot.y);

                if (yawErr < 0) {
                    yawErr = -yawErr;
                }
                if (yawErr < bestYawErr) {
                    bestYawErr = yawErr;
                    best = actor;
                }
            }
            actor = actor->next;
        }
    }
    return best;
}

static void FrozenUpdate(Actor* actor, PlayState* play) {
    if (!sStasis.colliderReady || (sStasis.actor != actor) || (sStasis.phase != STASIS_PHASE_FROZEN)) {
        return;
    }
    actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;
    actor->speedXZ = 0.0f;
}

static f32 ChargeFraction(void) {
    f32 f = sStasis.force / (STASIS_LAUNCH_MAX - STASIS_LAUNCH_BASE);

    return CLAMP(f, 0.0f, 1.0f);
}

static void ChargeColor(f32 t, Color_RGBA8* out) {
    out->r = (u8)(STASIS_TINT_R + ((f32)(STASIS_TINT_HOT_R - STASIS_TINT_R) * t));
    out->g = (u8)(STASIS_TINT_G + ((f32)(STASIS_TINT_HOT_G - STASIS_TINT_G) * t));
    out->b = (u8)(STASIS_TINT_B + ((f32)(STASIS_TINT_HOT_B - STASIS_TINT_B) * t));
}

static void TintFrozenBody(Actor* actor, PlayState* play, Color_RGBA8* grayscale) {
    f32 pulse;

    if (actor == sStasis.actor) {
        pulse = 0.5f + (0.5f * Math_SinS((s16)(sStasis.age * 0x900)));
        ChargeColor(ChargeFraction(), grayscale);
        grayscale->a = (u8)(235.0f + (20.0f * pulse));
        return;
    }
    if (actor == sOfferActor) {
        pulse = 0.5f + (0.5f * Math_SinS((s16)(play->gameplayFrames * 0x1800)));
        ChargeColor(0.0f, grayscale);
        grayscale->a = (u8)(110.0f + (70.0f * pulse));
    }
}

static void BrightenFrozenBody(Actor* actor, PlayState* play, bool* drawVanilla) {
    if ((actor != sStasis.actor) && (actor != sOfferActor)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gSPSetLights1(POLY_OPA_DISP++, sStasisLights);
    gSPSetLights1(POLY_XLU_DISP++, sStasisLights);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Only delete a bgId we registered: deleting an actor's own leaves it without collision for the scene.
static void DropBodyCollision(void) {
    PlayState* play = gPlayState;

    if (sStasis.bgIsOurs && (sStasis.bgId >= 0) && (play != NULL)) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, sStasis.bgId);
    }
    sStasis.bgIsOurs = 0;
    sStasis.bgId = -1;
}

// Scene-change path: the actors are gone, so no pointer may be written through. The collider is kept,
// since re-initialising it every freeze leaks collision list nodes.
static void ForgetBody(void) {
    sStasis.actor = NULL;
    sStasis.kind = STASIS_KIND_NONE;
    sStasis.phase = STASIS_PHASE_FROZEN;
    sStasis.timer = 0;
    sStasis.chainTimer = 0;
    sStasis.age = 0;
    sStasis.accumDamage = 0;
    sStasis.lastDmgFlags = 0;
    sStasis.lastDmgEffect = 0;
    sStasis.lastDmgAmount = 0;
    sStasis.force = 0.0f;
    sStasis.hitDir.x = sStasis.hitDir.y = sStasis.hitDir.z = 0.0f;
    sStasis.hasHitDir = 0;
    sStasis.bgId = -1;
    sStasis.bgIsOurs = 0;
    sStasis.riderAttached = 0;
    sStasis.slideLaunch = 0;
    sStasis.ownColliderCount = 0;
    sStasis.extraCollider = 0;
    sFireOwner = NULL;
}

static void RelocateBody(Actor* actor) {
    actor->home.pos = actor->world.pos;
    for (s32 i = 0; i < sStasis.ownColliderCount; i++) {
        Collider_UpdateCylinder(actor, sStasis.ownCollider[i]);
    }
}

static void RestoreBody(void) {
    Actor* actor = sStasis.actor;

    DropBodyCollision();
    if ((actor == NULL) || (actor->update == NULL)) {
        return;
    }
    if (sStasis.phase == STASIS_PHASE_FLYING) {
        RelocateBody(actor);
    }
    actor->update = sStasis.origUpdate;
    actor->flags = sStasis.origFlags;
    actor->gravity = sStasis.origGravity;
    actor->minVelocityY = sStasis.origMinVelocityY;
    actor->shape.rot = sStasis.origShapeRot;
    actor->world.rot = sStasis.origWorldRot;
    actor->room = (s8)sStasis.origRoom;
    actor->colChkInfo.mass = sStasis.origMass;
    actor->speedXZ = 0.0f;
    actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;
    MakePresser(actor);
}

static void LockDirToAxis(Actor* actor, Vec3f* dir) {
    s16 yaw = (s16)(Math_FAtan2F(dir->x, dir->z) * (0x8000 / M_PI));
    s16 rel = (s16)(((yaw - actor->shape.rot.y) + 0x2000) & (s16)0xC000);
    s16 snapped = (s16)(actor->shape.rot.y + rel);

    dir->x = Math_SinS(snapped);
    dir->y = 0.0f;
    dir->z = Math_CosS(snapped);
}

static u8 KeepsItsPose(Actor* actor) {
    return (sStasis.kind == STASIS_KIND_BLOCK) || IsIdInList(actor->id, sKeepsPoseIds, ARRAY_COUNT(sKeepsPoseIds));
}

static void ResolveLaunchDir(PlayState* play, Vec3f* out) {
    Actor* actor = sStasis.actor;

    if (sStasis.hasHitDir) {
        *out = sStasis.hitDir;
    } else {
        Player* player = GET_PLAYER(play);
        f32 dx = actor->world.pos.x - player->actor.world.pos.x;
        f32 dz = actor->world.pos.z - player->actor.world.pos.z;
        f32 len = sqrtf(SQ(dx) + SQ(dz));

        out->x = (len > 0.001f) ? (dx / len) : 0.0f;
        out->y = 0.0f;
        out->z = (len > 0.001f) ? (dz / len) : 1.0f;
    }
    if (KeepsItsPose(actor)) {
        LockDirToAxis(actor, out);
    }
}

static f32 LaunchSpeed(void) {
    f32 speed = STASIS_LAUNCH_BASE + sStasis.force;

    return (speed > STASIS_LAUNCH_MAX) ? STASIS_LAUNCH_MAX : speed;
}

// Subtracting health directly is not a hit: damageEffect comes from the enemy's own table via
// CollisionCheck_ApplyDamage, so a real hit is staged.
static ColliderInfo sStoredHitInfo;
static Collider sStoredHitCollider;

static void DeliverStoredHit(PlayState* play, Actor* actor) {
    Player* player = GET_PLAYER(play);
    ColliderCylinder* target;
    s32 damage;

    if ((actor == NULL) || (actor->update == NULL) || (sStasis.accumDamage == 0)) {
        return;
    }
    if ((sStasis.lastDmgFlags == 0) || (sStasis.ownColliderCount == 0)) {
        return;
    }
    target = sStasis.ownCollider[0];

    sStoredHitInfo.toucher.dmgFlags = sStasis.lastDmgFlags;
    sStoredHitInfo.toucher.effect = sStasis.lastDmgEffect;
    sStoredHitInfo.toucher.damage = sStasis.lastDmgAmount;
    sStoredHitInfo.toucherFlags = TOUCH_ON;
    sStoredHitCollider.actor = &player->actor;

    // `ac` must be a live actor: many enemies dereference it with no NULL check.
    target->base.acFlags |= AC_HIT;
    target->base.acFlags &= ~AC_BOUNCED;
    target->base.ac = &player->actor;
    target->info.acHit = &sStoredHitCollider;
    target->info.acHitInfo = &sStoredHitInfo;
    target->info.bumperFlags |= BUMP_HIT;
    target->info.bumper.hitPos.x = (s16)actor->world.pos.x;
    target->info.bumper.hitPos.y = (s16)actor->world.pos.y;
    target->info.bumper.hitPos.z = (s16)actor->world.pos.z;

    actor->colChkInfo.damage = 0;
    CollisionCheck_ApplyDamage(play, &play->colChkCtx, &target->base, &target->info);

    damage = (s32)sStasis.accumDamage;
    actor->colChkInfo.damage = (u8)((damage > 255) ? 255 : damage);
}

static void GrabRider(PlayState* play, Actor* actor) {
    Player* player = GET_PLAYER(play);

    sStasis.riderAttached = 0;
    if (sStasis.bgId < 0) {
        return;
    }
    if ((player->actor.floorBgId != sStasis.bgId) && (player->actor.wallBgId != sStasis.bgId) &&
        !(sStasis.bgIsOurs && DynaPolyActor_IsPlayerOnTop(&sStasisCarrier))) {
        if (!(player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER)) {
            return;
        }
        f32 loY;
        f32 hiY;
        f32 maxR;
        f32 maxRY;
        f32 horiz = sqrtf(SQ(player->actor.world.pos.x - actor->world.pos.x) +
                          SQ(player->actor.world.pos.z - actor->world.pos.z));
        f32 py = player->actor.world.pos.y;

        MeasureBody(actor, &loY, &hiY, &maxR, &maxRY);
        if ((horiz > (maxR + 30.0f)) || (py < (actor->world.pos.y + loY - 20.0f)) ||
            (py > (actor->world.pos.y + hiY + 20.0f))) {
            return;
        }
    }
    sStasis.riderOffset.x = player->actor.world.pos.x - actor->world.pos.x;
    sStasis.riderOffset.y = player->actor.world.pos.y - actor->world.pos.y;
    sStasis.riderOffset.z = player->actor.world.pos.z - actor->world.pos.z;
    sStasis.riderAttached = 1;
}

static void CarryRider(PlayState* play, Actor* actor) {
    Player* player = GET_PLAYER(play);

    if (!sStasis.riderAttached) {
        return;
    }
    player->actor.world.pos.x = actor->world.pos.x + sStasis.riderOffset.x;
    player->actor.world.pos.y = actor->world.pos.y + sStasis.riderOffset.y;
    player->actor.world.pos.z = actor->world.pos.z + sStasis.riderOffset.z;
    player->actor.prevPos = player->actor.world.pos;
    player->actor.velocity.y = 0.0f;
    player->actor.speedXZ = 0.0f;
}

static void EndStasis(PlayState* play) {
    Actor* actor = sStasis.actor;

    if (sStasis.timer > 0) {
        StasisSfx_Stop();
    }
    if ((actor == NULL) || (actor->update == NULL)) {
        ForgetBody();
        return;
    }
    if (sStasis.kind == STASIS_KIND_ENEMY) {
        RestoreBody();
        DeliverStoredHit(play, actor);
        ForgetBody();
        return;
    }
    if (sStasis.force <= 0.0f) {
        RestoreBody();
        ForgetBody();
        return;
    }

    GrabRider(play, actor);
    {
        Vec3f dir;
        f32 speed = LaunchSpeed();
        f32 horiz;
        s16 yaw;

        ResolveLaunchDir(play, &dir);
        sStasis.slideLaunch = (sStasis.kind == STASIS_KIND_BLOCK);
        horiz = sqrtf(SQ(dir.x) + SQ(dir.z));
        yaw = (horiz > 0.001f) ? (s16)(Math_FAtan2F(dir.x, dir.z) * (0x8000 / M_PI)) : actor->shape.rot.y;
        actor->world.rot.y = yaw;
        if (!KeepsItsPose(actor)) {
            actor->shape.rot.y = yaw;
        }
        actor->gravity = STASIS_GRAVITY;
        actor->minVelocityY = STASIS_MIN_VELOCITY_Y;
        if (sStasis.slideLaunch) {
            actor->speedXZ = speed;
            actor->velocity.y = 0.0f;
        } else {
            actor->speedXZ = speed * horiz;
            actor->velocity.y = (speed * dir.y) + STASIS_LAUNCH_VEL_Y;
            actor->bgCheckFlags &= ~STASIS_BG_GROUND;
        }
    }
    sStasis.phase = STASIS_PHASE_FLYING;
    sStasis.timer = STASIS_FLIGHT_FRAMES;
    PlaySfxAt(NA_SE_EV_HEAVY_THROW, &actor->world.pos);
}

static void BeginStasis(PlayState* play, Actor* target, u8 kind, s32 bgId) {
    f32 loY;
    f32 hiY;
    f32 maxR;
    f32 maxRY;

    sOfferActor = NULL;
    ForgetBody();

    sStasis.actor = target;
    sStasis.kind = kind;
    sStasis.phase = STASIS_PHASE_FROZEN;
    sStasis.timer = (kind == STASIS_KIND_ENEMY) ? STASIS_FRAMES_ENEMY : STASIS_FRAMES_OBJECT;
    sStasis.chainTimer = (kind == STASIS_KIND_ENEMY) ? STASIS_CHAIN_FRAMES_ENEMY : STASIS_CHAIN_FRAMES;
    sStasis.bgId = (kind == STASIS_KIND_CLIMBABLE) ? bgId : -1;

    if ((kind != STASIS_KIND_ENEMY) && (kind != STASIS_KIND_BLOCK) && (bgId < 0)) {
        s32 newBgId;
        u8 climbable;

        MeasureBody(target, &loY, &hiY, &maxR, &maxRY);
        climbable = (u8)((hiY - loY) >= STASIS_CLIMBABLE_MIN_HEIGHT);
        newBgId = BuildBodyCollision(play, target, climbable);
        if (newBgId >= 0) {
            sStasis.bgId = newBgId;
            sStasis.bgIsOurs = 1;
            if (climbable) {
                sStasis.kind = STASIS_KIND_CLIMBABLE;
            }
        }
    }

    sStasis.origUpdate = target->update;
    sStasis.origFlags = target->flags;
    sStasis.origGravity = target->gravity;
    sStasis.origMinVelocityY = target->minVelocityY;
    sStasis.origShapeRot = target->shape.rot;
    sStasis.origWorldRot = target->world.rot;
    sStasis.origRoom = target->room;
    sStasis.origMass = target->colChkInfo.mass;

    sStasis.ownColliderCount = (u8)ScanOwnColliders(target, sStasis.ownCollider, STASIS_MAX_OWN_COLLIDERS);
    if (!sStasis.colliderReady) {
        Collider_InitCylinder(play, &sStasis.collider);
        sStasis.colliderReady = 1;
    }
    Collider_SetCylinder(play, &sStasis.collider, target, &sStasisColliderInit);

    {
        f32 ownTop;

        MeasureBody(target, &loY, &hiY, &maxR, &maxRY);
        ownTop = loY;
        for (s32 i = 0; i < sStasis.ownColliderCount; i++) {
            f32 top = (f32)(sStasis.ownCollider[i]->dim.yShift + sStasis.ownCollider[i]->dim.height);

            if (top > ownTop) {
                ownTop = top;
            }
        }
        sStasis.extraCollider = (u8)((hiY - ownTop) > 8.0f);
        if (sStasis.extraCollider) {
            sStasis.collider.dim.radius = (s16)maxR;
            sStasis.collider.dim.yShift = (s16)ownTop;
            sStasis.collider.dim.height = (s16)(hiY - ownTop);
        }
        if (sStasis.ownColliderCount > 0) {
            sStasis.collider.base.colType = sStasis.ownCollider[0]->base.colType;
            sStasis.collider.base.acFlags |= (sStasis.ownCollider[0]->base.acFlags & AC_HARD);
        }
    }

    target->update = FrozenUpdate;
    target->velocity.x = target->velocity.y = target->velocity.z = 0.0f;
    target->speedXZ = 0.0f;
    target->gravity = 0.0f;
    target->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    MakePresser(target);
    target->room = -1;
    target->colChkInfo.mass = MASS_IMMOVABLE;

    StasisSfx_Play((kind == STASIS_KIND_ENEMY) ? STASIS_SFX_RATE_ENEMY : 1.0f, 0.85f);
}

#define SWITCHMAGNET_RANGE 700.0f
#define SWITCHMAGNET_LOCK_MISS 110.0f
#define SWITCHMAGNET_ASSIST_MISS 260.0f
#define SWITCHMAGNET_TURN 0x500
#define SWITCHMAGNET_SPEED_STEP 1.0f
#define SWITCHMAGNET_MAX_SPEED 45.0f
#define SWITCHMAGNET_MIN_TIME 6.0f
#define SWITCHMAGNET_SNAP_HEIGHT 100.0f
#define SWITCHMAGNET_SNAP_RADIUS 20.0f
#define SWITCHMAGNET_SNAP_CLEARANCE 20.0f
#define SWITCHMAGNET_LEASES 8
#define SWITCHMAGNET_HOLD_RANGE 40.0f

static const s16 sPresserIds[] = {
    ACTOR_OBJ_OSHIHIKI, ACTOR_OBJ_LIFT,    ACTOR_EN_LIGHTBOX, ACTOR_OBJ_KIBAKO2, ACTOR_BG_HEAVY_BLOCK, ACTOR_BG_PUSHBOX,
    ACTOR_OBJ_HSBLOCK,  ACTOR_OBJ_HAMISHI, ACTOR_OBJ_BOMBIWA, ACTOR_EN_ISHI,     ACTOR_EN_WOOD02,
};

static u8 IsPresser(Actor* actor) {
    if (actor == NULL) {
        return 0;
    }
    if (actor->flags & ACTOR_FLAG_CAN_PRESS_SWITCHES) {
        return 1;
    }
    return IsIdInList(actor->id, sPresserIds, ARRAY_COUNT(sPresserIds));
}

static void MakePresser(Actor* actor) {
    if (IsPresser(actor)) {
        actor->flags |= ACTOR_FLAG_CAN_PRESS_SWITCHES;
    }
}

static u8 IsPressableSwitch(Actor* actor) {
    if ((actor->id != ACTOR_OBJ_SWITCH) || ((actor->params & 7) != 0)) {
        return 0;
    }
    return ((actor->params >> 4) & 7) == 2;
}

static u8 DynaTopY(PlayState* play, Actor* actor, f32* outY) {
    for (s32 bg = 0; bg < play->colCtx.dyna.bgActorMax; bg++) {
        BgActor* bgActor = &play->colCtx.dyna.bgActors[bg];

        if (!(play->colCtx.dyna.bgActorFlags[bg] & 1) || (bgActor->actor != actor) || (bgActor->colHeader == NULL)) {
            continue;
        }
        *outY = actor->world.pos.y + ((f32)bgActor->colHeader->maxBounds.y * actor->scale.y);
        return 1;
    }
    return 0;
}

static f32 BodyBottom(PlayState* play, Actor* actor) {
    for (s32 bg = 0; bg < play->colCtx.dyna.bgActorMax; bg++) {
        BgActor* bgActor = &play->colCtx.dyna.bgActors[bg];

        if (!(play->colCtx.dyna.bgActorFlags[bg] & 1) || (bgActor->actor != actor) || (bgActor->colHeader == NULL)) {
            continue;
        }
        return (f32)bgActor->colHeader->minBounds.y * actor->scale.y;
    }
    return 0.0f;
}

static f32 TimeToFall(Actor* actor, f32 targetY) {
    f32 dy = actor->world.pos.y - targetY;
    f32 disc = SQ(actor->velocity.y) - (2.0f * actor->gravity * dy);
    f32 t;

    if (disc <= 0.0f) {
        return -1.0f;
    }
    t = (-actor->velocity.y - sqrtf(disc)) / actor->gravity;
    return (t > 0.0f) ? t : -1.0f;
}

static void SteerToSwitch(PlayState* play, Actor* actor) {
    Actor* best = NULL;
    f32 bestMiss = SWITCHMAGNET_ASSIST_MISS;
    f32 bestTime = 0.0f;
    f32 bestDist = 0.0f;
    s16 bestYaw = 0;
    f32 needSpeed;

    if (!IsPresser(actor)) {
        return;
    }
    if ((actor->velocity.y >= 0.0f) || (actor->gravity >= 0.0f)) {
        return;
    }
    for (Actor* it = play->actorCtx.actorLists[ACTORCAT_SWITCH].head; it != NULL; it = it->next) {
        f32 dx;
        f32 dz;
        f32 dist;
        f32 t;
        f32 travel;
        f32 miss;

        if (!IsPressableSwitch(it) || (it->world.pos.y >= actor->world.pos.y)) {
            continue;
        }
        dx = it->world.pos.x - actor->world.pos.x;
        dz = it->world.pos.z - actor->world.pos.z;
        dist = sqrtf(SQ(dx) + SQ(dz));
        if (dist > SWITCHMAGNET_RANGE) {
            continue;
        }
        t = TimeToFall(actor, it->world.pos.y);
        if (t < SWITCHMAGNET_MIN_TIME) {
            continue;
        }
        travel = actor->speedXZ * t;
        miss = sqrtf(SQ(actor->world.pos.x + (Math_SinS(actor->world.rot.y) * travel) - it->world.pos.x) +
                     SQ(actor->world.pos.z + (Math_CosS(actor->world.rot.y) * travel) - it->world.pos.z));
        if (miss < bestMiss) {
            bestMiss = miss;
            bestTime = t;
            bestDist = dist;
            bestYaw = (s16)(Math_FAtan2F(dx, dz) * (0x8000 / M_PI));
            best = it;
        }
    }
    if (best == NULL) {
        return;
    }
    needSpeed = bestDist / bestTime;
    if (needSpeed > SWITCHMAGNET_MAX_SPEED) {
        return;
    }
    if (bestMiss <= SWITCHMAGNET_LOCK_MISS) {
        actor->world.rot.y = bestYaw;
        actor->speedXZ = needSpeed;
        return;
    }
    Math_SmoothStepToS(&actor->world.rot.y, bestYaw, 3, SWITCHMAGNET_TURN, 1);
    Math_StepToF(&actor->speedXZ, needSpeed, SWITCHMAGNET_SPEED_STEP);
}

static void SnapOntoSwitch(PlayState* play, Actor* actor, u8 alignToSwitch) {
    if (!IsPresser(actor)) {
        return;
    }
    for (Actor* it = play->actorCtx.actorLists[ACTORCAT_SWITCH].head; it != NULL; it = it->next) {
        f32 dy = actor->world.pos.y - it->world.pos.y;
        f32 plateTop;

        if (!IsPressableSwitch(it) || (dy < 0.0f) || (dy > SWITCHMAGNET_SNAP_HEIGHT)) {
            continue;
        }
        if ((fabsf(actor->world.pos.x - it->world.pos.x) > SWITCHMAGNET_SNAP_RADIUS) ||
            (fabsf(actor->world.pos.z - it->world.pos.z) > SWITCHMAGNET_SNAP_RADIUS)) {
            continue;
        }
        if (actor->floorHeight > (it->world.pos.y + SWITCHMAGNET_SNAP_CLEARANCE)) {
            continue;
        }
        actor->world.pos.x = it->world.pos.x;
        actor->world.pos.z = it->world.pos.z;
        actor->prevPos.x = actor->world.pos.x;
        actor->prevPos.z = actor->world.pos.z;
        if (alignToSwitch) {
            actor->shape.rot.y = it->shape.rot.y;
        }
        actor->world.rot.y = it->shape.rot.y;
        actor->speedXZ = 0.0f;
        actor->velocity.x = 0.0f;
        actor->velocity.z = 0.0f;
        actor->velocity.y = 0.0f;
        if (DynaTopY(play, it, &plateTop)) {
            actor->world.pos.y = plateTop - BodyBottom(play, actor);
            actor->prevPos.y = actor->world.pos.y;
            actor->bgCheckFlags |= STASIS_BG_GROUND;
        }
        return;
    }
}

typedef struct {
    Actor* body;
    Actor* sw;
} SwitchLease;

static SwitchLease sLeases[SWITCHMAGNET_LEASES];

static void HoldSwitch(Actor* body, Actor* sw) {
    s32 free = -1;

    for (s32 i = 0; i < SWITCHMAGNET_LEASES; i++) {
        if ((sLeases[i].body == body) && (sLeases[i].sw == sw)) {
            return;
        }
        if ((free < 0) && (sLeases[i].body == NULL)) {
            free = i;
        }
    }
    if (free >= 0) {
        sLeases[free].body = body;
        sLeases[free].sw = sw;
    }
}

static void PressSwitchUnder(PlayState* play, Actor* actor, f32 halfWidth, f32 bottomY) {
    static const f32 sProbeX[5] = { 0.9f, -0.9f, -0.9f, 0.9f, 0.0f };
    static const f32 sProbeZ[5] = { -0.9f, -0.9f, 0.9f, 0.9f, 0.0f };
    f32 soleY = actor->world.pos.y + bottomY;

    for (s32 i = 0; i < 5; i++) {
        CollisionPoly* poly;
        DynaPolyActor* dyna;
        s32 bgId;
        f32 floorY;
        Vec3f probe;

        probe.x = actor->world.pos.x + (sProbeX[i] * halfWidth);
        probe.y = soleY + 10.0f;
        probe.z = actor->world.pos.z + (sProbeZ[i] * halfWidth);

        floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bgId, actor, &probe);
        if (floorY <= BGCHECK_Y_MIN) {
            continue;
        }
        if ((soleY - floorY) > SWITCHMAGNET_SNAP_CLEARANCE) {
            continue;
        }
        dyna = DynaPoly_GetActor(&play->colCtx, bgId);
        if (dyna == NULL) {
            continue;
        }
        DynaPolyActor_SetActorOnTop(dyna);
        DynaPolyActor_SetSwitchPressed(dyna);
        if (IsPressableSwitch(&dyna->actor)) {
            HoldSwitch(actor, &dyna->actor);
        }
    }
}

static void RunSwitchLeases(PlayState* play) {
    for (s32 i = 0; i < SWITCHMAGNET_LEASES; i++) {
        Actor* body = sLeases[i].body;
        Actor* sw = sLeases[i].sw;
        DynaPolyActor* dyna;
        f32 plateTop;

        if ((body == NULL) || (sw == NULL)) {
            continue;
        }
        if ((body->update == NULL) || (sw->update == NULL)) {
            sLeases[i].body = sLeases[i].sw = NULL;
            continue;
        }
        if ((SQ(body->world.pos.x - sw->world.pos.x) + SQ(body->world.pos.z - sw->world.pos.z)) >
            SQ(SWITCHMAGNET_HOLD_RANGE)) {
            sLeases[i].body = sLeases[i].sw = NULL;
            continue;
        }
        if (DynaTopY(play, sw, &plateTop)) {
            f32 dy = (body->world.pos.y + BodyBottom(play, body)) - plateTop;

            if ((dy > SWITCHMAGNET_HOLD_RANGE) || (dy < -SWITCHMAGNET_HOLD_RANGE)) {
                sLeases[i].body = sLeases[i].sw = NULL;
                continue;
            }
        }
        dyna = DynaPoly_GetActor(&play->colCtx, ((DynaPolyActor*)sw)->bgId);
        if ((dyna == NULL) || (&dyna->actor != sw)) {
            sLeases[i].body = sLeases[i].sw = NULL;
            continue;
        }
        DynaPolyActor_SetActorOnTop(dyna);
        DynaPolyActor_SetSwitchPressed(dyna);
    }
}

static ColliderCylinderInit sFireColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { DMG_FIRE, STASIS_FIRE_EFFECT, STASIS_FIRE_DAMAGE },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 0, 0, 0, { 0, 0, 0 } },
};

static void RunBodyFire(PlayState* play, Actor* actor) {
    f32 loY;
    f32 hiY;
    f32 maxR;
    f32 maxRY;

    if ((actor == NULL) || (actor->update == NULL) || !IsBodyBurning(actor)) {
        sFireOwner = NULL;
        return;
    }
    MeasureBody(actor, &loY, &hiY, &maxR, &maxRY);
    if (sFireOwner != actor) {
        Collider_InitCylinder(play, &sFireCollider);
        Collider_SetCylinder(play, &sFireCollider, actor, &sFireColliderInit);
        sFireOwner = actor;
    }
    sFireCollider.dim.pos.x = (s16)actor->world.pos.x;
    sFireCollider.dim.pos.y = (s16)(actor->world.pos.y + loY - (STASIS_FIRE_PAD * 0.5f));
    sFireCollider.dim.pos.z = (s16)actor->world.pos.z;
    sFireCollider.dim.radius = (s16)(maxR + STASIS_FIRE_PAD);
    sFireCollider.dim.height = (s16)((hiY - loY) + STASIS_FIRE_PAD);
    sFireCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sFireCollider.base);
    sFireCollider.base.atFlags &= ~AT_HIT;
}

static void SubmitBodyColliders(PlayState* play, Actor* actor) {
    for (s32 i = 0; i < sStasis.ownColliderCount; i++) {
        ColliderCylinder* cyl = sStasis.ownCollider[i];

        Collider_UpdateCylinder(actor, cyl);
        if (cyl->base.acFlags & AC_ON) {
            CollisionCheck_SetAC(play, &play->colChkCtx, &cyl->base);
        }
        if (cyl->base.ocFlags1 & OC1_ON) {
            CollisionCheck_SetOC(play, &play->colChkCtx, &cyl->base);
        }
    }
    if (sStasis.colliderReady && sStasis.extraCollider) {
        Collider_UpdateCylinder(actor, &sStasis.collider);
        CollisionCheck_SetAC(play, &play->colChkCtx, &sStasis.collider.base);
    }
}

static ColliderCylinder* TakeHitCollider(void) {
    for (s32 i = 0; i < sStasis.ownColliderCount; i++) {
        if (sStasis.ownCollider[i]->base.acFlags & AC_HIT) {
            return sStasis.ownCollider[i];
        }
    }
    if (sStasis.colliderReady && (sStasis.collider.base.acFlags & AC_HIT)) {
        return &sStasis.collider;
    }
    return NULL;
}

static void CaptureBlow(PlayState* play, Actor* actor) {
    ColliderCylinder* hit = TakeHitCollider();
    Player* player = GET_PLAYER(play);
    Actor* attacker;
    Vec3f dir;
    f32 len;
    f32 charge;

    if (hit == NULL) {
        return;
    }
    attacker = (hit->base.ac != NULL) ? hit->base.ac : &player->actor;

    len = (attacker == &player->actor)
              ? 0.0f
              : sqrtf(SQ(attacker->velocity.x) + SQ(attacker->velocity.y) + SQ(attacker->velocity.z));
    if (len > 1.0f) {
        dir.x = attacker->velocity.x / len;
        dir.y = attacker->velocity.y / len;
        dir.z = attacker->velocity.z / len;
    } else {
        f32 chest = attacker->world.pos.y + 30.0f;

        dir.x = (f32)hit->info.bumper.hitPos.x - attacker->world.pos.x;
        dir.y = (f32)hit->info.bumper.hitPos.y - chest;
        dir.z = (f32)hit->info.bumper.hitPos.z - attacker->world.pos.z;
        len = sqrtf(SQ(dir.x) + SQ(dir.y) + SQ(dir.z));
        if (len <= 0.001f) {
            dir.x = actor->world.pos.x - attacker->world.pos.x;
            dir.y = 0.0f;
            dir.z = actor->world.pos.z - attacker->world.pos.z;
            len = sqrtf(SQ(dir.x) + SQ(dir.z));
        }
        if (len > 0.001f) {
            dir.x /= len;
            dir.y /= len;
            dir.z /= len;
        } else {
            dir.x = dir.y = 0.0f;
            dir.z = 1.0f;
        }
    }

    charge = (actor->colChkInfo.damage > 0) ? ((f32)actor->colChkInfo.damage * STASIS_LAUNCH_PER_DAMAGE)
                                            : STASIS_LAUNCH_PER_HIT;

    for (s32 i = 0; i < sStasis.ownColliderCount; i++) {
        sStasis.ownCollider[i]->base.acFlags &= ~AC_HIT;
    }
    sStasis.collider.base.acFlags &= ~AC_HIT;

    if ((hit->info.acHitInfo != NULL) && (hit->info.acHitInfo->toucher.dmgFlags != 0)) {
        sStasis.lastDmgFlags = hit->info.acHitInfo->toucher.dmgFlags;
        sStasis.lastDmgEffect = hit->info.acHitInfo->toucher.effect;
        sStasis.lastDmgAmount = hit->info.acHitInfo->toucher.damage;
    }
    sStasis.accumDamage += actor->colChkInfo.damage;
    sStasis.force += charge;
    sStasis.hitDir = dir;
    sStasis.hasHitDir = 1;
    actor->colChkInfo.damage = 0;
    actor->colChkInfo.damageEffect = 0;

    if (!(hit->base.acFlags & AC_HARD)) {
        s16 yaw = Math_Vec3f_Yaw(&actor->world.pos, &player->actor.world.pos);

        player->actor.world.rot.y = yaw;
        player->actor.speedXZ = STASIS_RECOIL_SPEED;
        player->actor.velocity.y = STASIS_RECOIL_HEIGHT;
        PlaySfxAt(NA_SE_IT_SHIELD_REFLECT_MG, &actor->world.pos);
    }
}

static void FlyBody(PlayState* play, Actor* actor) {
    f32 loY;
    f32 hiY;
    f32 maxR;
    f32 maxRY;
    f32 probeY;
    f32 speed;
    s32 substeps;
    s32 hide = sStasis.bgIsOurs && (sStasis.bgId >= 0);
    u16 saved = 0;

    if (!sStasis.slideLaunch) {
        SteerToSwitch(play, actor);
    }
    Actor_UpdateVelocityXZGravity(actor);
    MeasureBody(actor, &loY, &hiY, &maxR, &maxRY);

    // maxRY degenerates to the bottom ring on even-width bodies, and BgCheck_EntitySphVsWall then
    // line-tests floors as walls when (checkHeight + dy) < 5.0f, so probe at the middle.
    probeY = (loY + hiY) * 0.5f;
    {
        f32 floor = (hiY - 2.0f) > 25.0f ? 25.0f : (hiY - 2.0f);

        if (probeY < floor) {
            probeY = floor;
        }
    }

    speed = sqrtf(SQ(actor->velocity.x) + SQ(actor->velocity.y) + SQ(actor->velocity.z));
    {
        f32 maxHop = (maxR * 0.5f) < 10.0f ? 10.0f : (maxR * 0.5f);

        substeps = (s32)(speed / maxHop) + 1;
        if (substeps > 8) {
            substeps = 8;
        }
    }

    if (hide) {
        saved = play->colCtx.dyna.bgActorFlags[sStasis.bgId];
        play->colCtx.dyna.bgActorFlags[sStasis.bgId] &= ~1;
    }
    for (s32 i = 0; i < substeps; i++) {
        actor->prevPos = actor->world.pos;
        actor->world.pos.x += actor->velocity.x / (f32)substeps;
        actor->world.pos.y += actor->velocity.y / (f32)substeps;
        actor->world.pos.z += actor->velocity.z / (f32)substeps;
        Actor_UpdateBgCheckInfo(play, actor, probeY, maxR, hiY, 0x87);
        if (actor->bgCheckFlags & (sStasis.slideLaunch ? STASIS_BG_WALL : (STASIS_BG_GROUND | STASIS_BG_WALL))) {
            break;
        }
    }
    if (hide) {
        play->colCtx.dyna.bgActorFlags[sStasis.bgId] = saved;
    }

    if ((actor->bgCheckFlags & STASIS_BG_CEILING) && (actor->velocity.y > 0.0f)) {
        actor->velocity.y = 0.0f;
    }
    if ((loY < 0.0f) && (actor->floorHeight > BGCHECK_Y_MIN) && ((actor->world.pos.y + loY) <= actor->floorHeight)) {
        actor->world.pos.y = actor->floorHeight - loY;
        actor->velocity.y = 0.0f;
        actor->bgCheckFlags |= STASIS_BG_GROUND;
    }

    SnapOntoSwitch(play, actor, !KeepsItsPose(actor));
    PressSwitchUnder(play, actor, maxR, loY);
    RunBodyFire(play, actor);
    if (sStasis.bgIsOurs) {
        SyncCarrier(actor);
        // This move lands after DynaPoly_Setup has run for the frame, so force the re-expansion.
        play->colCtx.dyna.bitFlag |= DYNAPOLY_INVALIDATE_LOOKUP;
    }
    CarryRider(play, actor);

    if (sStasis.colliderReady) {
        Collider_UpdateCylinder(actor, &sStasis.collider);
        CollisionCheck_SetAT(play, &play->colChkCtx, &sStasis.collider.base);
    }
    if (sStasis.timer > 0) {
        sStasis.timer--;
    }
    if (sStasis.slideLaunch) {
        actor->speedXZ *= STASIS_SLIDE_FRICTION;
        if ((actor->bgCheckFlags & STASIS_BG_WALL) || (actor->speedXZ < 1.0f) || (sStasis.timer <= 0) ||
            (sStasis.collider.base.atFlags & AT_HIT)) {
            PlaySfxAt(NA_SE_EV_BLOCK_BOUND, &actor->world.pos);
            RestoreBody();
            ForgetBody();
        }
        return;
    }
    if ((actor->bgCheckFlags & (STASIS_BG_GROUND | STASIS_BG_WALL)) || (sStasis.timer <= 0) ||
        (sStasis.collider.base.atFlags & AT_HIT)) {
        PlaySfxAt(NA_SE_EV_BOMB_DROP_WATER, &actor->world.pos);
        RestoreBody();
        ForgetBody();
    }
}

static void OfferBody(PlayState* play, u8 allowed) {
    sOfferActor = NULL;
    if (!allowed || (sStasis.actor != NULL)) {
        return;
    }
    sOfferActor = ScanForBody(play);
}

static void RunStasis(PlayState* play, Player* player) {
    Actor* actor = sStasis.actor;

    if (actor == NULL) {
        return;
    }
    if (actor->update == NULL) {
        ForgetBody();
        return;
    }
    sStasis.age++;
    if (sStasis.chainTimer > 0) {
        sStasis.chainTimer--;
    }
    if (sStasis.phase == STASIS_PHASE_FLYING) {
        FlyBody(play, actor);
        return;
    }

    CaptureBlow(play, actor);
    if (sStasis.bgIsOurs) {
        SyncCarrier(actor);
    }
    SubmitBodyColliders(play, actor);
    {
        f32 loY;
        f32 hiY;
        f32 maxR;
        f32 maxRY;

        MeasureBody(actor, &loY, &hiY, &maxR, &maxRY);
        PressSwitchUnder(play, actor, maxR, loY);
    }
    RunBodyFire(play, actor);

    if (sStasis.timer > 0) {
        sStasis.timer--;
    }
    if (sStasis.timer <= 0) {
        EndStasis(play);
    }
}

#define STASIS_ARROW_SHAFT_R 4.5f
#define STASIS_ARROW_HEAD_R 13.0f
#define STASIS_ARROW_HEAD_LEN 22.0f
#define STASIS_ARROW_MIN_LEN 34.0f
#define STASIS_ARROW_GROWTH 110.0f
#define STASIS_CHAIN_COUNT 6
#define STASIS_CHAIN_R 255
#define STASIS_CHAIN_G 240
#define STASIS_CHAIN_B 110
#define STASIS_CHAIN_DL_MAX 128

static const ALIGN_ASSET(2) char sChainDL[] = "__OTR__objects/object_link_boy/gLinkAdultHookshotChainDL";

static Vtx sPrismVtx[] = {
    VTX(100, 0, 0, 0, 0, 0, 0, 0, 255),    VTX(71, 0, 71, 0, 0, 0, 0, 0, 255),
    VTX(0, 0, 100, 0, 0, 0, 0, 0, 255),    VTX(-71, 0, 71, 0, 0, 0, 0, 0, 255),
    VTX(-100, 0, 0, 0, 0, 0, 0, 0, 255),   VTX(-71, 0, -71, 0, 0, 0, 0, 0, 255),
    VTX(0, 0, -100, 0, 0, 0, 0, 0, 255),   VTX(71, 0, -71, 0, 0, 0, 0, 0, 255),
    VTX(100, 100, 0, 0, 0, 0, 0, 0, 255),  VTX(71, 100, 71, 0, 0, 0, 0, 0, 255),
    VTX(0, 100, 100, 0, 0, 0, 0, 0, 255),  VTX(-71, 100, 71, 0, 0, 0, 0, 0, 255),
    VTX(-100, 100, 0, 0, 0, 0, 0, 0, 255), VTX(-71, 100, -71, 0, 0, 0, 0, 0, 255),
    VTX(0, 100, -100, 0, 0, 0, 0, 0, 255), VTX(71, 100, -71, 0, 0, 0, 0, 0, 255),
};

static Gfx sPrismDL[] = {
    gsSPVertex(sPrismVtx, 16, 0),
    gsSP2Triangles(0, 8, 9, 0, 0, 9, 1, 0),
    gsSP2Triangles(1, 9, 10, 0, 1, 10, 2, 0),
    gsSP2Triangles(2, 10, 11, 0, 2, 11, 3, 0),
    gsSP2Triangles(3, 11, 12, 0, 3, 12, 4, 0),
    gsSP2Triangles(4, 12, 13, 0, 4, 13, 5, 0),
    gsSP2Triangles(5, 13, 14, 0, 5, 14, 6, 0),
    gsSP2Triangles(6, 14, 15, 0, 6, 15, 7, 0),
    gsSP2Triangles(7, 15, 8, 0, 7, 8, 0, 0),
    gsSPEndDisplayList(),
};

static Vtx sConeVtx[] = {
    VTX(0, 100, 0, 0, 0, 0, 0, 0, 255),   VTX(100, 0, 0, 0, 0, 0, 0, 0, 255),  VTX(71, 0, 71, 0, 0, 0, 0, 0, 255),
    VTX(0, 0, 100, 0, 0, 0, 0, 0, 255),   VTX(-71, 0, 71, 0, 0, 0, 0, 0, 255), VTX(-100, 0, 0, 0, 0, 0, 0, 0, 255),
    VTX(-71, 0, -71, 0, 0, 0, 0, 0, 255), VTX(0, 0, -100, 0, 0, 0, 0, 0, 255), VTX(71, 0, -71, 0, 0, 0, 0, 0, 255),
};

static Gfx sConeDL[] = {
    gsSPVertex(sConeVtx, 9, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(0, 3, 4, 0, 0, 4, 5, 0),
    gsSP2Triangles(0, 5, 6, 0, 0, 6, 7, 0),
    gsSP2Triangles(0, 7, 8, 0, 0, 8, 1, 0),
    gsSPEndDisplayList(),
};

static Gfx sChainCopy[STASIS_CHAIN_DL_MAX];
static u8 sIsChainCopyReady;

static u8 IsGfxTwoWord(u8 op) {
    return (op == 0x20) || (op == 0x24) || (op == 0x25) || (op == 0x27) || (op == 0x31) || (op == 0x32) ||
           (op == 0x33) || (op == 0x35) || (op == 0x36) || (op == 0x42);
}

static Gfx* GetTintableChainDL(void) {
    Gfx* srcDL;
    s32 i = 0;

    if (sIsChainCopyReady) {
        return sChainCopy;
    }
    srcDL = ResourceMgr_FileExists(sChainDL) ? ResourceMgr_LoadGfxByName(sChainDL) : NULL;
    if (srcDL == NULL) {
        return NULL;
    }
    while (i < STASIS_CHAIN_DL_MAX) {
        u8 op = (u8)((srcDL[i].words.w0 >> 24) & 0xFF);

        sChainCopy[i] = srcDL[i];
        if ((op == (u8)G_SETCOMBINE) || (op == (u8)G_SETPRIMCOLOR)) {
            gDPNoOp(&sChainCopy[i]);
        }
        if (op == (u8)G_ENDDL) {
            sIsChainCopyReady = 1;
            return sChainCopy;
        }
        i++;
        if (IsGfxTwoWord(op) && (i < STASIS_CHAIN_DL_MAX)) {
            sChainCopy[i] = srcDL[i];
            i++;
        }
    }
    return NULL;
}

static void DrawChain(PlayState* play, Vec3f* start, Vec3f* end, u8 alpha) {
    Gfx* tintable = GetTintableChainDL();
    f32 dx = end->x - start->x;
    f32 dy = end->y - start->y;
    f32 dz = end->z - start->z;
    f32 distXZ = sqrtf(SQ(dx) + SQ(dz));
    f32 len = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));

    if ((tintable == NULL) || (len < 0.01f)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetCombineLERP(POLY_XLU_DISP++, TEXEL0, 0, PRIMITIVE, ENVIRONMENT, 0, 0, 0, TEXEL0, COMBINED, 0, PRIMITIVE,
                      ENVIRONMENT, 0, 0, 0, COMBINED);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 170, 170, 170, alpha);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, STASIS_CHAIN_R, STASIS_CHAIN_G, STASIS_CHAIN_B, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    Matrix_Translate(start->x, start->y, start->z, MTXMODE_NEW);
    Matrix_RotateY(Math_FAtan2F(dx, dz), MTXMODE_APPLY);
    Matrix_RotateX(Math_FAtan2F(-dy, distXZ), MTXMODE_APPLY);
    Matrix_Scale(0.015f, 0.015f, len * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, tintable);
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawChains(PlayState* play) {
    static const Vec3f sChainAxis[STASIS_CHAIN_COUNT] = {
        { 1.0f, 0.0f, 0.0f },  { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f },  { 0.0f, 0.0f, -1.0f },
    };
    Actor* actor = sStasis.actor;
    f32 span = (f32)((sStasis.kind == STASIS_KIND_ENEMY) ? STASIS_CHAIN_FRAMES_ENEMY : STASIS_CHAIN_FRAMES);
    f32 grow = 1.0f - ((f32)sStasis.chainTimer / span);
    u8 alpha = (u8)(255.0f * ((f32)sStasis.chainTimer / span));
    f32 reach;
    Vec3f center = actor->world.pos;

    if (grow > 1.0f) {
        grow = 1.0f;
    }
    reach = 45.0f + (55.0f * grow);
    center.y += actor->shape.yOffset * actor->scale.y;
    for (s32 i = 0; i < STASIS_CHAIN_COUNT; i++) {
        Vec3f end = { center.x + (sChainAxis[i].x * reach), center.y + (sChainAxis[i].y * reach),
                      center.z + (sChainAxis[i].z * reach) };

        DrawChain(play, &center, &end, alpha);
    }
}

static void DrawSolid(PlayState* play, Gfx** gfxP, Gfx* dl, Vec3f* start, Vec3f* end, f32 radius, Color_RGBA8* color,
                      u8 alpha) {
    Gfx* gfx = *gfxP;
    f32 dx = end->x - start->x;
    f32 dy = end->y - start->y;
    f32 dz = end->z - start->z;
    f32 xzLen = sqrtf(SQ(dx) + SQ(dz));
    f32 len = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));

    if (len < 0.01f) {
        return;
    }
    Matrix_Translate(start->x, start->y, start->z, MTXMODE_NEW);
    Matrix_RotateY(Math_FAtan2F(dx, dz), MTXMODE_APPLY);
    Matrix_RotateX(Math_FAtan2F(xzLen, dy), MTXMODE_APPLY);
    Matrix_Scale(radius / 100.0f, len / 100.0f, radius / 100.0f, MTXMODE_APPLY);
    gDPSetPrimColor(gfx++, 0, 0, color->r, color->g, color->b, alpha);
    gSPMatrix(gfx++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(gfx++, dl);
    *gfxP = gfx;
}

static void DrawLaunchArrow(PlayState* play, Gfx** gfxP) {
    Actor* actor = sStasis.actor;
    f32 charge = ChargeFraction();
    f32 pulse = 0.75f + (0.25f * Math_SinS((s16)(sStasis.age * 0x1000)));
    u8 alpha = (u8)(230.0f * pulse);
    Color_RGBA8 color = { 0, 0, 0, 255 };
    Vec3f origin = actor->world.pos;
    Vec3f neck;
    Vec3f tip;
    Vec3f dir;
    f32 startDist = 20.0f + (actor->colChkInfo.cylRadius * 0.6f);
    f32 length;
    Gfx* gfx = *gfxP;

    ResolveLaunchDir(play, &dir);
    ChargeColor(charge, &color);
    origin.y += actor->shape.yOffset * actor->scale.y;
    length = startDist + STASIS_ARROW_MIN_LEN + (STASIS_ARROW_GROWTH * charge);

    neck.x = origin.x + (dir.x * (length - STASIS_ARROW_HEAD_LEN));
    neck.y = origin.y + (dir.y * (length - STASIS_ARROW_HEAD_LEN));
    neck.z = origin.z + (dir.z * (length - STASIS_ARROW_HEAD_LEN));
    tip.x = origin.x + (dir.x * length);
    tip.y = origin.y + (dir.y * length);
    tip.z = origin.z + (dir.z * length);
    origin.x += dir.x * startDist;
    origin.y += dir.y * startDist;
    origin.z += dir.z * startDist;

    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gSPClearGeometryMode(gfx++, G_LIGHTING | G_CULL_BACK);
    *gfxP = gfx;
    DrawSolid(play, gfxP, sPrismDL, &origin, &neck, STASIS_ARROW_SHAFT_R, &color, alpha);
    DrawSolid(play, gfxP, sConeDL, &neck, &tip, STASIS_ARROW_HEAD_R, &color, alpha);
    gfx = *gfxP;
    gSPSetGeometryMode(gfx++, G_LIGHTING | G_CULL_BACK);
    *gfxP = gfx;
}

static void DrawStasis(void) {
    PlayState* play = gPlayState;
    if (!play)
        return;
    Actor* actor = sStasis.actor;
    Gfx* gfx;

    if ((actor == NULL) || (actor->update == NULL)) {
        return;
    }
    if (sStasis.chainTimer > 0) {
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        CLOSE_DISPS(play->state.gfxCtx);
        DrawChains(play);
    }
    if ((sStasis.phase == STASIS_PHASE_FROZEN) && (sStasis.kind != STASIS_KIND_ENEMY)) {
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gfx = POLY_XLU_DISP;
        DrawLaunchArrow(play, &gfx);
        POLY_XLU_DISP = gfx;
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

static bool CastStasis(PlayState* play, Player* player) {
    Actor* target;
    s32 bgId = -1;
    u8 kind;

    if (sStasis.actor != NULL) {
        if (sStasis.phase != STASIS_PHASE_FLYING) {
            StasisSfx_SeekToTail(STASIS_SFX_RELEASE_SECONDS);
        }
        EndStasis(play);
        return true;
    }
    target = ScanForBody(play);
    if (target == NULL) {
        return false;
    }
    kind = ClassifyBody(play, target, &bgId);
    if (kind == STASIS_KIND_NONE) {
        return false;
    }
    BeginStasis(play, target, kind, bgId);
    return true;
}

static void MakeFrozenBodyClimbable(int32_t bgId, uint32_t* flags) {
    if ((sStasis.actor != NULL) && (sStasis.kind == STASIS_KIND_CLIMBABLE) && (sStasis.bgId == bgId) &&
        (sStasis.bgId >= 0)) {
        *flags |= WALL_FLAG_CLIMBABLE;
    }
}

static void ResolveRuneWallFlags(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId, uint32_t* flags) {
    MakePillarClimbable(colCtx, poly, bgId, flags);
    MakeFrozenBodyClimbable(bgId, flags);
}

static void TakeOutSlate(PlayState* play, Player* player) {
    sIsSlateDrawn = true;
    sCastFrame = play->state.frames;
    PlaySfxAt(NA_SE_PL_CHANGE_ARMS, &player->actor.world.pos);
}

static void StowSlate(Player* player, PlayState* play) {
    sIsSlateDrawn = false;
    sWheelHoldTimer = 0;
    LeaveAiming(true);
}

#include "master_cycle.inc.c"

static void CastRune(Player* player, PlayState* play) {
    u8 rune = CurrentRune();
    bool cast = false;

    sCastFrame = play->state.frames;
    if (!IsRuneOwned(rune)) {
        PlaySfxAt(NA_SE_SY_ERROR, &player->actor.world.pos);
        return;
    }
    switch (rune) {
        case SLATE_RUNE_BOMB:
            cast = CastRemoteBomb(play, player);
            break;
        case SLATE_RUNE_STASIS:
            cast = CastStasis(play, player);
            break;
        case SLATE_RUNE_MASTER_CYCLE:
            cast = MasterCycle_Cast(play, player);
            break;
        case SLATE_RUNE_CRYONIS:
            cast = CastCryonis(play, player);
            break;
        default:
            break;
    }
    if (!cast) {
        PlaySfxAt(NA_SE_SY_ERROR, &player->actor.world.pos);
    }
}

static int32_t HoldSlate(Player* player, PlayState* play) {
    return 0;
}

static void ReadWheelHold(PlayState* play) {
    if (!sIsSlateDrawn || !(play->state.input[0].cur.button & BTN_L)) {
        sWheelHoldTimer = 0;
        return;
    }
    if (sWheelHoldTimer >= SLATE_WHEEL_HOLD_FRAMES) {
        return;
    }
    sWheelHoldTimer++;
    if (sWheelHoldTimer == SLATE_WHEEL_HOLD_FRAMES) {
        OpenRuneWheel();
    }
}

static void RunSlate(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (play == NULL || player == NULL) {
        return;
    }
    MasterCycle_Tick(play, player);
    SyncReceivedRunes();
    if (!sApi->IsCustomItemOwned(SLATE_KEY)) {
        return;
    }
    DescribeRune(CurrentRune());
    RunSwitchLeases(play);
    RunRemoteBomb(play, player);
    RunStasis(play, player);
    PrunePillar();
    WatchPillarForTrouble(player);
    if (Z64Wheel_IsOpen()) {
        return;
    }
    OfferBody(play, sIsSlateDrawn && (CurrentRune() == SLATE_RUNE_STASIS));
    if (RunCryonis(play, player)) {
        return;
    }
    ReadWheelHold(play);
}

static void ForgetRunes(int16_t sceneNum) {
    sMcSceneChanged = 1;
    MasterCycle_Forget();
    ForgetRemoteBomb();
    LeaveAiming(false);
    ForgetPillar();
    ForgetBody();
    sOfferActor = NULL;
}

static void CaptureHandMatrix(PlayState* play, Player* player, int32_t limbIndex) {
    if (limbIndex != PLAYER_LIMB_R_HAND) {
        return;
    }
    Matrix_Get(&sHandMatrix);
    sHasHandMatrix = true;
}

static void DrawSlateInHand(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    f32 unscale = player->actor.scale.x != 0.0f ? 1.0f / player->actor.scale.x : 1.0f;

    DrawAimGhost();

    if (!sIsSlateDrawn || !sHasHandMatrix || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    if (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Put(&sHandMatrix);
    Matrix_Scale(unscale, unscale, unscale, MTXMODE_APPLY);
    Matrix_RotateY(DEG_TO_RAD(SLATE_HELD_ROT_Y), MTXMODE_APPLY);
    Matrix_RotateX(DEG_TO_RAD(SLATE_HELD_ROT_X), MTXMODE_APPLY);
    Matrix_Translate(0.0f, SLATE_HELD_OFFSET_Y, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(SLATE_HELD_SCALE, SLATE_HELD_SCALE, SLATE_HELD_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sSlateDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static const ALIGN_ASSET(2) char sSoulFlameDL[] = "__OTR__objects/object_gi_fire/gGiBlueFireFlameDL";

static void DrawPickupFlameWithModel(PlayState* play, GetItemEntry* entry) {
    const SOHCustomItemDefinition* item = CustomItemRegistry_FindByRuntimeId(entry->tableId);
    const SOHCustomPickupVariant* variant = item ? CustomItemRegistry_GetPickupVariant(item->key) : NULL;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 8,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 16, 32, 1, play->state.frames,
                                           -8 * play->state.frames, 16, 32));
    Matrix_Push();
    Matrix_Scale(2.0f, 2.0f, 2.0f, MTXMODE_APPLY);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, variant ? variant->flameRed : 160, variant ? variant->flameGreen : 230,
                         variant ? variant->flameBlue : 255, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSoulFlameDL);
    gSPGrayscale(POLY_XLU_DISP++, false);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    DrawPickupFlameWithModel(play, getItemEntry);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(SLATE_GIVE_SCALE, SLATE_GIVE_SCALE, SLATE_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sSlateDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// No lock: a race can only start the cue one buffer early or late.

#include "stasis_sfx_pcm.inc.c"

#define STASIS_SFX_OUT_RATE 32000.0f

typedef struct {
    volatile u8 wantPlay;
    volatile u8 wantSeek;
    volatile u8 playing;
    volatile f32 rate;
    volatile f32 volume;
    volatile f32 seekTail;
    f32 pos;
} StasisSfxState;

static StasisSfxState sSfx;

static void StasisSfx_Play(f32 rate, f32 volume) {
    sSfx.rate = rate;
    sSfx.volume = volume;
    sSfx.wantPlay = 1;
}

static void StasisSfx_Stop(void) {
    sSfx.wantPlay = 0;
    sSfx.playing = 0;
}

static void StasisSfx_SeekToTail(f32 seconds) {
    sSfx.seekTail = seconds;
    sSfx.wantSeek = 1;
}

static u8 StasisSfx_IsGameFrozen(void) {
    return (gPlayState == NULL) || (gPlayState->pauseCtx.state != 0) || (gPlayState->pauseCtx.debugState != 0);
}

static void StasisSfx_Mix(s16* samples, u32 frameCount) {
    f32 advance;
    f32 volume;

    if (sSfx.wantPlay) {
        sSfx.wantPlay = 0;
        sSfx.playing = 1;
        sSfx.pos = 0.0f;
    }
    if (sSfx.wantSeek) {
        f32 p = (f32)STASIS_SFX_SAMPLES - (sSfx.seekTail * (f32)STASIS_SFX_RATE);

        sSfx.wantSeek = 0;
        sSfx.playing = 1;
        sSfx.pos = (p > 0.0f) ? p : 0.0f;
    }
    if (!sSfx.playing || (samples == NULL) || StasisSfx_IsGameFrozen()) {
        return;
    }
    advance = (sSfx.rate * (f32)STASIS_SFX_RATE) / STASIS_SFX_OUT_RATE;
    volume = sSfx.volume;

    for (u32 i = 0; i < frameCount; i++) {
        s32 index = (s32)sSfx.pos;
        s32 value;
        s32 left;
        s32 right;

        if (index >= (STASIS_SFX_SAMPLES - 1)) {
            sSfx.playing = 0;
            return;
        }
        {
            f32 frac = sSfx.pos - (f32)index;
            f32 a = (f32)sStasisSfxPcm[index];
            f32 b = (f32)sStasisSfxPcm[index + 1];

            value = (s32)((a + ((b - a) * frac)) * volume);
        }
        left = samples[(i * 2) + 0] + value;
        right = samples[(i * 2) + 1] + value;
        samples[(i * 2) + 0] = (s16)CLAMP(left, -32768, 32767);
        samples[(i * 2) + 1] = (s16)CLAMP(right, -32768, 32767);
        sSfx.pos += advance;
    }
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

static bool IsCryonisPillar(Actor* actor) {
    return actor != NULL && actor == sCryonis.pillar;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (SOH_MOD_API_HAS(sApi, PublishService))
        sApi->PublishService("nei.slate.is_cryonis_pillar", (void*)IsCryonisPillar);
    if (!SOH_MOD_API_HAS(sApi, SetCustomItemText)) {
        return;
    }

    SOHCustomItemDefinition slate = Z64Items_Define(SLATE_KEY, sSlateIconTex, sNameTex);

    Z64Items_SetButtons(&slate, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&slate, 0, 15, 0);
    Z64Items_SetTextbox(&slate, "You got the %rSheikah Slate%w! An ancient tablet of the Sheikah.");
    Z64Items_SetPauseText(&slate, "%rSheikah Slate&%wPress %y\xA1%w to draw the slate, hold %y\xA2%w&"
                                  "for the rune wheel.");
    Z64Items_SetAction(&slate, TakeOutSlate, HoldSlate);
    Z64Items_SetHeldCallbacks(&slate, CastRune, StowSlate, NULL);
    slate.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    slate.appearance.modelPath = sSlateDL;
    slate.appearance.limb = PLAYER_LIMB_R_HAND;
    slate.appearance.rotation[0] = SLATE_HELD_ROT_X;
    slate.appearance.rotation[1] = SLATE_HELD_ROT_Y;
    slate.appearance.offset[1] = SLATE_HELD_OFFSET_Y;
    slate.appearance.scale = SLATE_HELD_SCALE;
    slate.getItemEntry.drawFunc = DrawGetItem;
    slate.onReceive = GrantAllRunes;
    SOHCustomItemRandomizer slateLogic = sSlateLogic;
    slateLogic.poolCount = 0;
    Z64Items_SetLogic(&slate, &slateLogic);

    if (!Z64Items_Register(sApi, &slate)) {
        return;
    }
    Z64Wheel_Register(sApi, SLATE_KEY, SLATE_KEY, sWheelEntries, ARRAY_COUNT(sWheelEntries), true);
    if (!RegisterRunePickups()) {
        return;
    }
    sApi->RegisterVB(VB_THROW_OR_PUT_DOWN_HELD_ITEM, HoldOntoRemoteBomb);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, RunSlate);
    SOH_REGISTER_HOOK(sApi, OnPlayDrawEnd, DrawStasis);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, MasterCycle_FilterInput);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetRunes);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, CaptureHandMatrix);
    SOH_REGISTER_HOOK(sApi, OnBgCheckResolveWallFlags, ResolveRuneWallFlags);
    SOH_REGISTER_HOOK(sApi, OnActorResolveGrayscale, TintFrozenBody);
    SOH_REGISTER_HOOK(sApi, OnActorDraw, BrightenFrozenBody);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawSlateInHand);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorResolveGrayscale, ACTOR_EN_BOM, TintRemoteBomb);
    if (SOH_MOD_API_HAS(sApi, RegisterAudioMix)) {
        sApi->RegisterAudioMix(StasisSfx_Mix);
    }
}
