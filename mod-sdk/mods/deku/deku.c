#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "z64aiming.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define DEKU_MASK_KEY "nei.deku_form_mask"
#define DEKU_FORM_KEY "nei.deku"
#define DEKU_MODEL_PATH "objects/forms/deku"
#define DEKU_MASK_PAGE 1
#define DEKU_MASK_SLOT 5
#define DEKU_LEAF_KEY "nei.deku_leaf"
#define ROCS_FEATHER_KEY "nei.rocs_feather"
#define ROCS_CAPE_KEY "nei.rocs_cape"
#define DEKU_LEAF_BUTTON_COUNT 8
#define DEKU_FLOWER_MAGIC_COST 10

#define BG_ON_GROUND 1
#define MM_ANIM_VALUES_PER_FRAME 67
#define MM_ANIM_BASE_TRANSL_X (-57)
#define DEKU_ROOT_SCALE_ADULT 0.311f
#define DEKU_ROOT_SCALE_CHILD 0.442f
#define DEKU_HEIGHT 36.0f

#define FLOWER_MAX_CHARGE 15
#define FLOWER_GLIDE_CHARGE 10
#define FLOWER_SHORT_DISTANCE 600.0f
#define FLOWER_LONG_DISTANCE 960.0f
#define FLOWER_AIR_MAGIC_INTERVAL 7
#define FLIGHT_SPEED 7.0f
#define FLIGHT_ACCEL 0.35f

#define WATER_HOPS 5
#define WATER_HOP_SPEED 8.0f
#define DEKU_ROCS_JUMP_VELOCITY 11.0f
#define DEKU_ROCS_WATER_VELOCITY 5.5f

#define SPIN_RADIUS 30
#define SPIN_HEIGHT 42
#define SPIN_DAMAGE 1
#define SPIN_ROTATION 20000.0f
#define SPIN_TIMER 196608.0f
#define SPIN_DECEL 800.0f
#define SPIN_MARK_DURATION 62
#define SPIN_MARK_CAPACITY 64

#define BUBBLE_MAGIC_COST 2
#define BUBBLE_MAX_CHARGE 16.0f
#define BUBBLE_LIFE 99
#define BUBBLE_RADIUS_MIN 12
#define BUBBLE_RADIUS_MAX 30
#define BUBBLE_HEIGHT 26

#define MM_SFX_PLAY_SERVICE "mm.sfx.play"
#define MM_SFX_STOP_SERVICE "mm.sfx.stop"
#define LINK_VOICE_ACTIONS 0x20
#define MM_DEKU_VOICE_OFFSET 0x80
#define MM_DEKU_AUTO_JUMP_VOICE (NA_SE_VO_LI_AUTO_JUMP + MM_DEKU_VOICE_OFFSET)
#define MM_DEKU_SPIN 0x09A9
#define MM_DEKU_FLOWER_ENTER 0x08E2
#define MM_DEKU_FLOWER_EXIT 0x08E3
#define MM_DEKU_BUBBLE_BREATH 0x09A1
#define MM_DEKU_BUBBLE_FIRE 0x08E0
#define MM_DEKU_JUMP1 0x09B0
#define MM_DEKU_BUBBLE_MISS_FIRE 0x09BF
#define MM_DEKU_FLOWER_OPEN 0x1850
#define MM_DEKU_FLOWER_ROLL 0x1851
#define MM_DEKU_FLOWER_CLOSE 0x1852
#define MM_DEKU_FLOWER_BUD 0x09A0
#define MM_DEKU_FLOWER_STRUGGLE 0x09A6
#define MM_DEKU_BUBBLE_VANISH 0x1854
#define MM_DEKU_BUBBLE_SHOT_LEVEL 0x185A
#define MM_DEKU_CHANGE_ARMS 0x0835
#define MM_SFX_FLAG 0x0800
#define MM_DEKU_BUBBLE_BREATH_FLAGGED (MM_DEKU_BUBBLE_BREATH - MM_SFX_FLAG)
#define MM_DEKU_BUBBLE_SHOT_LEVEL_FLAGGED (MM_DEKU_BUBBLE_SHOT_LEVEL - MM_SFX_FLAG)
typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);
typedef void (*MmSfxStopFunc)(uint16_t sfxId);
#define MM_OCARINA_NOTE_SERVICE "mm.ocarina.note"
#define MM_OCARINA_INSTRUMENT_DEKU_PIPES 9
typedef bool (*MmOcarinaNoteFunc)(uint8_t instrumentId, uint8_t pitch, f32* bendFreq);
#define OCARINA_NOTE_NONE 0xFF
#define PIPE_PIECE_COUNT 5
// LinkAnimation advances R_UPDATE_RATE * 0.5 clip frames per update, and the rate is always 3.
#define CLIP_FRAMES_PER_UPDATE 1.5f

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__icon_item_static_yar/gItemIconDekuMaskTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__item_name_static/gItemNameDekuMaskENGTex";
static const ALIGN_ASSET(2) char sMaskDL[] = "__OTR__objects/gameplay_keep/gDekuMaskDL";
static const ALIGN_ASSET(2) char sGetItemDL[] = "__OTR__objects/object_gi_nutsmask/gGiDekuMaskDL";
static const ALIGN_ASSET(2) char sFlowerDL[] = "__OTR__objects/gameplay_keep/gGoldDekuFlowerIdleDL";
static const ALIGN_ASSET(2) char sOpenFlowerDL[] = "__OTR__objects/object_link_nuts/gLinkDekuOpenFlowerDL";
static const ALIGN_ASSET(2) char sClosedFlowerDL[] = "__OTR__objects/object_link_nuts/gLinkDekuClosedFlowerDL";
static const ALIGN_ASSET(2) char sLeftFlowerStemDL[] = "__OTR__objects/object_link_nuts/object_link_nuts_DL_008760";
static const ALIGN_ASSET(2) char sRightFlowerStemDL[] = "__OTR__objects/object_link_nuts/object_link_nuts_DL_008660";
static const ALIGN_ASSET(2) char sPipesDL[] = "__OTR__objects/object_link_nuts/object_link_nuts_DL_007390";
static const char* const sPipePieceDLs[PIPE_PIECE_COUNT] = {
    "__OTR__objects/object_link_nuts/object_link_nuts_DL_007A28",
    "__OTR__objects/object_link_nuts/object_link_nuts_DL_0077D0",
    "__OTR__objects/object_link_nuts/object_link_nuts_DL_007548",
    "__OTR__objects/object_link_nuts/object_link_nuts_DL_007900",
    "__OTR__objects/object_link_nuts/object_link_nuts_DL_0076A0",
};
static const ALIGN_ASSET(2) char sBubbleSetupDL[] = "__OTR__objects/gameplay_keep/gameplay_keep_DL_06F380";
static const ALIGN_ASSET(2) char sBubbleStillDL[] = "__OTR__objects/gameplay_keep/gameplay_keep_DL_06F9F0";
static const ALIGN_ASSET(2) char sBubbleMoveDL[] = "__OTR__objects/gameplay_keep/gameplay_keep_DL_06FAE0";
static const u16 sItemButtons[DEKU_LEAF_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                          BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

#define MM_ANIM_PATH(name) "__OTR__misc/link_animetion/gPlayerAnim_" name "_Data"
static const ALIGN_ASSET(2) char sSpinAnimPath[] = MM_ANIM_PATH("pn_attack");
static const ALIGN_ASSET(2) char sBubbleReadyPath[] = MM_ANIM_PATH("pn_tamahakidf");
static const ALIGN_ASSET(2) char sBubbleShootPath[] = MM_ANIM_PATH("pn_tamahaki");
static const ALIGN_ASSET(2) char sGuardAnimPath[] = MM_ANIM_PATH("pn_gurd");
static const ALIGN_ASSET(2) char sFlightLaunchPath[] = MM_ANIM_PATH("pn_kakku");
static const ALIGN_ASSET(2) char sFlightFlutterPath[] = MM_ANIM_PATH("pn_batabata");
static const ALIGN_ASSET(2) char sFlightOpenPath[] = MM_ANIM_PATH("pn_kakkufinish");
static const ALIGN_ASSET(2) char sFlightFinishPath[] = MM_ANIM_PATH("pn_rakkafinish");
static const ALIGN_ASSET(2) char sMaskOnPath[] = MM_ANIM_PATH("cl_setmask");
static const ALIGN_ASSET(2) char sMaskOffPath[] = MM_ANIM_PATH("pn_maskoffstart");
static const ALIGN_ASSET(2) char sJumpPath[] = MM_ANIM_PATH("link_normal_jump");
static const ALIGN_ASSET(2) char sRunJumpPath[] = MM_ANIM_PATH("link_normal_run_jump");
static const ALIGN_ASSET(2) char sIdlePath[] = MM_ANIM_PATH("link_normal_wait_free");
static const ALIGN_ASSET(2) char sPipesRaisePath[] = MM_ANIM_PATH("pn_gakkistart");
static const ALIGN_ASSET(2) char sPipesPlayPath[] = MM_ANIM_PATH("pn_gakkiplay");

static const char* const sDekuLimbDL[PLAYER_LIMB_MAX] = {
    NULL,
    NULL,
    "__OTR__objects/object_link_nuts/gLinkDekuWaistDL",
    NULL,
    "__OTR__objects/object_link_nuts/gLinkDekuRightThighDL",
    "__OTR__objects/object_link_nuts/gLinkDekuRightShinDL",
    "__OTR__objects/object_link_nuts/gLinkDekuRightFootDL",
    "__OTR__objects/object_link_nuts/gLinkDekuLeftThighDL",
    "__OTR__objects/object_link_nuts/gLinkDekuLeftShinDL",
    "__OTR__objects/object_link_nuts/gLinkDekuLeftFootDL",
    NULL,
    "__OTR__objects/object_link_nuts/gLinkDekuHeadDL",
    "__OTR__objects/object_link_nuts/gLinkDekuHatDL",
    "__OTR__objects/object_link_nuts/gLinkDekuCollarDL",
    "__OTR__objects/object_link_nuts/gLinkDekuLeftShoulderDL",
    "__OTR__objects/object_link_nuts/gLinkDekuLeftForearmDL",
    "__OTR__objects/object_link_nuts/gLinkDekuLeftHandDL",
    "__OTR__objects/object_link_nuts/gLinkDekuRightShoulderDL",
    "__OTR__objects/object_link_nuts/gLinkDekuRightForearmDL",
    "__OTR__objects/object_link_nuts/gLinkDekuRightHandDL",
    NULL,
    "__OTR__objects/object_link_nuts/gLinkDekuTorsoDL",
};

// Same limb order as OoT Link. Only the local translations differ.
static const Vec3f sDekuJointPos[PLAYER_LIMB_MAX] = {
    { 0, 0, 0 },       { 0, 0, 0 },       { 0, 0, 0 },       { 945, 0, 0 },
    { -800, 0, -250 }, { 458, 0, 0 },     { 309, 5, 11 },    { -800, 0, 250 },
    { 454, 0, 0 },     { 310, 6, 3 },     { 0, 21, -7 },     { 705, 0, 0 },
    { -500, -1400, 0 },{ 0, 0, 0 },       { 370, 0, 350 },   { 406, 0, 0 },
    { 356, 0, 0 },     { 370, 0, -350 },  { 425, 0, 0 },     { 336, 0, 0 },
    { 600, -250, 0 },  { 0, 0, 0 },
};

static const char* const sRequiredHooks[] = {
    "OnPlayerActionHandler", "OnPlayerResolveLimbDraw", "OnPlayerPostLimbDraw",
    "OnActorDrawEnd",        "OnActorPlaySfx",          "OnSceneInit",
    "OnSaveEditorTabs",      "OnOcarinaNote",           "OnOcarinaPlaybackNote",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

typedef enum {
    DEKU_ACTION_NONE,
    DEKU_ACTION_SPIN,
    DEKU_ACTION_BUBBLE_CHARGE,
    DEKU_ACTION_FLOWER,
    DEKU_ACTION_FLIGHT,
    DEKU_ACTION_WATER_HOP,
    DEKU_ACTION_GUARD,
} DekuAction;

static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static MmSfxStopFunc sStopMmSfx;

static LinkAnimationHeader sMmAnims[15];
static s16* sMmFrames[15];
static LinkAnimationHeader* sPipesRaiseAnim;
static LinkAnimationHeader* sPipesPlayAnim;
static LinkAnimationHeader* sSpinAnim;
static LinkAnimationHeader* sBubbleReadyAnim;
static LinkAnimationHeader* sBubbleShootAnim;
static LinkAnimationHeader* sGuardAnim;
static LinkAnimationHeader* sFlightLaunchAnim;
static LinkAnimationHeader* sFlightFlutterAnim;
static LinkAnimationHeader* sFlightOpenAnim;
static LinkAnimationHeader* sFlightFinishAnim;
static LinkAnimationHeader* sMaskOnAnim;
static LinkAnimationHeader* sMaskOffAnim;
static LinkAnimationHeader* sJumpAnim;
static LinkAnimationHeader* sRunJumpAnim;
static LinkAnimationHeader* sIdleAnim;

static u8 sAction;
static f32 sSpinAngularSpeed;
static f32 sSpinTimer;
static s32 sSpinRotAccum;
typedef struct {
    Vec3f p1;
    Vec3f p2;
    s16 life;
    bool connectPrev;
} DekuSpinMark;
static DekuSpinMark sSpinMarks[SPIN_MARK_CAPACITY];
static s16 sSpinMarkCount;
static bool sSpinMarkContinue;
static s16 sFlowerTimer;
static s16 sFlowerCharge;
static Vec3f sFlowerPos;
static f32 sFlowerDepth;
static f32 sFlowerSinkSpeed;
static s8 sFlowerStage;
static u16 sFlowerButton;
static bool sFlightCanGlide;
static bool sFlightFromAir;
static s8 sFlightMagicTimer;
static f32 sFlightDistance;
static Vec3f sFlightStart;
static bool sFlightOpen;
static s16 sFlowerRollTimer;
static s8 sWaterHops;
static bool sRocsDoubleJumped;
static u32 sRocsGroundJumpFrame;
static s8 sPreviousInvincibility;

static ColliderCylinder sSpinCollider;
static ColliderCylinder sBubbleCollider;
static ColliderCylinder sGuardCollider;
static bool sSpinColliderReady;
static bool sBubbleColliderReady;
static bool sGuardColliderReady;
static s32 sSpinTrail = -1;

static bool sBubbleActive;
static f32 sBubbleCharge;
static s16 sBubbleLife;
static f32 sBubbleScale;
static Vec3f sBubblePos;
static Vec3f sBubbleStep;
static s16 sBubbleRotX;
static s16 sBubbleRotY;
static s16 sBubbleWobbleX;
static s16 sBubbleWobbleY;
static s8 sBubbleState;
static bool sBubbleFirstPerson;
static bool sBubbleArmed;
static s16 sBubbleFullChargeTimer;
static bool sBubbleCanFire;
static bool sBubbleMouthReady;
static Vec3f sBubbleMouthPos;
static Vec3f sBubbleMouthForward;
static f32 sDekuCheekScale = 1.0f;

static void StopSpinTrail(PlayState* play);

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_Action_Idle(Player* player, PlayState* play);
void func_80839FFC(Player* player, PlayState* play);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
int Player_IsZTargeting(Player* player);
s32 func_80836AB8(Player* player, s32 forceFocus);
s32 Player_GetMovementSpeedAndYaw(Player* player, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                  PlayState* play);

#define SPEED_MODE_CURVED 0.018f

static ColliderCylinderInit sSpinColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_SPIN_KOKIRI, 0x00, SPIN_DAMAGE },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { SPIN_RADIUS, SPIN_HEIGHT, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sBubbleColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_SLINGSHOT, 0x00, 1 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { BUBBLE_RADIUS_MIN, BUBBLE_HEIGHT, -13, { 0, 0, 0 } },
};

static ColliderCylinderInit sGuardColliderInit = {
    { COLTYPE_METAL, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0x00000000, 0x00, 0x00 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_NONE,
      BUMP_ON,
      OCELEM_NONE },
    { 28, 38, 0, { 0, 0, 0 } },
};

static bool IsDeku(void) {
    return sApi != NULL && sApi->IsFormActive(DEKU_FORM_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BG_ON_GROUND) != 0;
}

static bool HasBubbleMagic(void) {
    return gSaveContext.isMagicAcquired && gSaveContext.magic >= BUBBLE_MAGIC_COST;
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 &
             (PLAYER_STATE1_DEAD | PLAYER_STATE1_LOADING | PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM |
              PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_CARRYING_ACTOR |
              PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE |
              PLAYER_STATE1_DAMAGED | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_CLIMBING_LEDGE |
              PLAYER_STATE1_HANGING_OFF_LEDGE));
}

static void PlayMmSfx(u16 mmSfx, u16 fallback, Vec3f* pos) {
    if (sPlayMmSfx == NULL && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
    }
    if (sPlayMmSfx == NULL || !sPlayMmSfx(mmSfx, pos)) {
        Audio_PlaySoundGeneral(fallback, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultReverb);
    }
}

static void RefreshMmSfx(u16 mmSfx, Vec3f* pos) {
    if (sPlayMmSfx == NULL && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
    }
    if (sPlayMmSfx != NULL) {
        sPlayMmSfx(mmSfx, pos);
    }
}

static void StopMmSfx(u16 mmSfx) {
    if (sStopMmSfx == NULL && SOH_MOD_API_HAS(sApi, FindService)) {
        sStopMmSfx = (MmSfxStopFunc)sApi->FindService(MM_SFX_STOP_SERVICE);
    }
    if (sStopMmSfx != NULL) {
        sStopMmSfx(mmSfx);
    }
}

static LinkAnimationHeader* LoadMmAnim(const char* path, s32 slot) {
    LinkAnimationHeader* source = ResourceMgr_LoadPlayerAnimAsHeader(path);
    if (source == NULL || source->common.frameCount <= 0) {
        return NULL;
    }
    size_t values = (size_t)source->common.frameCount * MM_ANIM_VALUES_PER_FRAME;
    s16* frames = (s16*)malloc(values * sizeof(s16));
    if (frames == NULL) {
        return NULL;
    }
    memcpy(frames, source->segment, values * sizeof(s16));
    for (s32 frame = 0; frame < source->common.frameCount; frame++) {
        s16* root = &frames[frame * MM_ANIM_VALUES_PER_FRAME];
        root[0] = MM_ANIM_BASE_TRANSL_X;
        root[2] = 0;
    }
    free(sMmFrames[slot]);
    sMmFrames[slot] = frames;
    sMmAnims[slot].common.frameCount = source->common.frameCount;
    sMmAnims[slot].segment = frames;
    return &sMmAnims[slot];
}

static void LoadAnims(void) {
    sSpinAnim = LoadMmAnim(sSpinAnimPath, 0);
    sBubbleReadyAnim = LoadMmAnim(sBubbleReadyPath, 1);
    sBubbleShootAnim = LoadMmAnim(sBubbleShootPath, 2);
    sGuardAnim = LoadMmAnim(sGuardAnimPath, 3);
    sFlightLaunchAnim = LoadMmAnim(sFlightLaunchPath, 4);
    sFlightFlutterAnim = LoadMmAnim(sFlightFlutterPath, 5);
    sFlightOpenAnim = LoadMmAnim(sFlightOpenPath, 11);
    sFlightFinishAnim = LoadMmAnim(sFlightFinishPath, 6);
    sMaskOnAnim = LoadMmAnim(sMaskOnPath, 7);
    sJumpAnim = LoadMmAnim(sJumpPath, 8);
    sRunJumpAnim = LoadMmAnim(sRunJumpPath, 9);
    sIdleAnim = LoadMmAnim(sIdlePath, 10);
    sMaskOffAnim = LoadMmAnim(sMaskOffPath, 12);
    sPipesRaiseAnim = LoadMmAnim(sPipesRaisePath, 13);
    sPipesPlayAnim = LoadMmAnim(sPipesPlayPath, 14);
}

static void ChangeAnim(PlayState* play, Player* player, LinkAnimationHeader* anim, u8 mode, f32 morph) {
    if (anim == NULL) {
        return;
    }
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), mode, morph);
}

static void SetupAirAction(PlayState* play, Player* player, PlayerActionFunc actionFunc) {
    Player_SetupAction(play, player, actionFunc, 0);
    // Without MIDAIR, func_8083AA10 swaps any airborne action for its fall before actionFunc runs.
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
}

static void InitCylinder(PlayState* play, Player* player, ColliderCylinder* collider,
                         ColliderCylinderInit* init, bool* ready) {
    if (*ready) {
        return;
    }
    Collider_InitCylinder(play, collider);
    Collider_SetCylinder(play, collider, &player->actor, init);
    *ready = true;
}

static void PlaceCylinder(ColliderCylinder* collider, Vec3f* pos) {
    collider->dim.pos.x = (s16)pos->x;
    collider->dim.pos.y = (s16)pos->y;
    collider->dim.pos.z = (s16)pos->z;
}

static void StopDekuAction(Player* player, PlayState* play) {
    if (sAction == DEKU_ACTION_NONE) {
        return;
    }
    if (sAction == DEKU_ACTION_SPIN) {
        player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw;
        StopSpinTrail(play);
    }
    player->actor.scale.x = player->actor.scale.y = player->actor.scale.z = 0.01f;
    player->actor.shape.yOffset = 0.0f;
    sAction = DEKU_ACTION_NONE;
    func_80839FFC(player, play);
}

static void SpawnDekuDust(PlayState* play, Player* player, s16 scale) {
    Vec3f pos = player->actor.world.pos;
    Vec3f velocity = { Rand_CenteredFloat(2.5f), Rand_ZeroFloat(2.0f), Rand_CenteredFloat(2.5f) };
    Vec3f accel = { 0.0f, 0.05f, 0.0f };
    Color_RGBA8 prim = { 255, 220, 80, 255 };
    Color_RGBA8 env = { 110, 65, 10, 0 };
    pos.y += 5.0f;
    EffectSsDtBubble_SpawnCustomColor(play, &pos, &velocity, &accel, &prim, &env, scale, 18, 0);
}

// ---- spin ----

static void StartSpinTrail(PlayState* play) {
    EffectBlureInit2 blure = {
        0, 8, 0, { 255, 255, 255, 255 }, { 255, 255, 255, 64 }, { 255, 255, 255, 0 }, { 255, 255, 255, 0 }, 4,
        0, 2, 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
    };

    if (sSpinTrail < 0) {
        Effect_Add(play, &sSpinTrail, EFFECT_BLURE2, 0, 0, &blure);
    }
}

static void StopSpinTrail(PlayState* play) {
    sSpinMarkContinue = false;
    if (sSpinTrail >= 0) {
        EffectBlure_AddSpace(Effect_GetByIndex(sSpinTrail));
        Effect_Delete(play, sSpinTrail);
        sSpinTrail = -1;
    }
}

static void SpawnSpinSparkle(PlayState* play, Player* player) {
    Color_RGBA8 prim = { 255, 200, 200, 0 };
    Color_RGBA8 env = { 255, 255, 0, 0 };
    Vec3f velocity = { 0.0f, 0.3f, 0.0f };
    Vec3f accel = { 0.0f, -0.025f, 0.0f };
    Vec3f pos = player->bodyPartsPos[PLAYER_BODYPART_WAIST];
    f32 sign = Rand_ZeroOne() < 0.5f ? -1.0f : 1.0f;

    velocity.x = (Rand_ZeroFloat(0.5f) + 1.0f) * sign;
    accel.x = 0.0f;
    sign = Rand_ZeroOne() < 0.5f ? -1.0f : 1.0f;
    velocity.z = (Rand_ZeroFloat(0.5f) + 1.0f) * sign;
    accel.z = 0.0f;
    pos.y += Rand_ZeroFloat(15.0f);
    EffectSsKiraKira_SpawnDispersed(play, &pos, &velocity, &accel, &prim, &env,
                                     Rand_ZeroOne() < 0.5f ? 2000 : -150, 32);
}

static void UpdateSpinMarks(void) {
    s16 write = 0;

    for (s16 i = 0; i < sSpinMarkCount; i++) {
        if (--sSpinMarks[i].life > 0) {
            if (write != i) {
                sSpinMarks[write] = sSpinMarks[i];
            }
            write++;
        }
    }
    sSpinMarkCount = write;
    if (sSpinMarkCount == 0) {
        sSpinMarkContinue = false;
    }
}

static void AddSpinMark(Player* player) {
    DekuSpinMark* mark;
    f32 width = 2.0f;
    s16 yaw = player->yaw;

    // MM refuses dynamic collision and ends the current strip at poly gaps.
    if (player->actor.floorPoly == NULL || player->actor.floorBgId != BGCHECK_SCENE) {
        sSpinMarkContinue = false;
        return;
    }
    if (sSpinMarkCount >= SPIN_MARK_CAPACITY) {
        memmove(&sSpinMarks[0], &sSpinMarks[1], sizeof(sSpinMarks[0]) * (SPIN_MARK_CAPACITY - 1));
        sSpinMarkCount--;
    }
    mark = &sSpinMarks[sSpinMarkCount++];
    mark->p1.x = player->actor.world.pos.x + Math_SinS(yaw + 0x4000) * width;
    mark->p1.z = player->actor.world.pos.z + Math_CosS(yaw + 0x4000) * width;
    mark->p2.x = player->actor.world.pos.x + Math_SinS(yaw - 0x4000) * width;
    mark->p2.z = player->actor.world.pos.z + Math_CosS(yaw - 0x4000) * width;
    mark->p1.y = mark->p2.y = player->actor.floorHeight + 2.0f;
    mark->life = SPIN_MARK_DURATION;
    mark->connectPrev = sSpinMarkContinue;
    sSpinMarkContinue = true;
}

static void SpawnSpinFloorEffect(PlayState* play, Player* player) {
    if (!IsGrounded(player)) {
        return;
    }
    // MM's func_8083F8A8 only emits this ring on ordinary ground and sand.
    // Other materials either use their own debris (grass/snow) or no dust;
    // spawning the yellow ring everywhere was the visibly wrong floor effect.
    if (sSpinAngularSpeed > 9500.0f &&
        (player->floorSfxOffset == NA_SE_PL_WALK_GROUND - SFX_FLAG ||
         player->floorSfxOffset == NA_SE_PL_WALK_SAND - SFX_FLAG)) {
        Actor_SpawnFloorDustRing(play, &player->actor, &player->actor.world.pos, 2.0f, 1, 2.5f, 10, 18, true);
    }
    AddSpinMark(player);
    func_8002F974(&player->actor, (u16)(NA_SE_PL_SLIP_LEVEL + player->floorSfxOffset - SFX_FLAG));
}

static void DekuSpinAction(Player* player, PlayState* play) {
    s16 previousYaw = player->yaw;
    f32 targetSpeed = 0.0f;
    s16 targetYaw = player->yaw;

    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    LinkAnimation_Update(play, &player->skelAnime);
    InitCylinder(play, player, &sSpinCollider, &sSpinColliderInit, &sSpinColliderReady);
    PlaceCylinder(&sSpinCollider, &player->actor.world.pos);
    sSpinCollider.base.atFlags |= AT_ON;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sSpinCollider.base);

    Player_GetMovementSpeedAndYaw(player, &targetSpeed, &targetYaw, SPEED_MODE_CURVED, play);
    targetSpeed *= 1.0f - (0.9f * ((11100.0f - sSpinAngularSpeed) / 11100.0f));
    if (ABS((s16)(player->yaw - targetYaw)) > 0x6000) {
        Math_StepToF(&player->linearVelocity, 0.0f, 1.5f);
    } else {
        Math_StepToF(&player->linearVelocity, targetSpeed, 1.5f);
        Math_SmoothStepToS(&player->yaw, targetYaw, 2, 0x320, 0x14);
    }

    sSpinAngularSpeed -= SPIN_DECEL;
    sSpinRotAccum += (s16)sSpinAngularSpeed + (s16)(player->yaw - previousYaw);
    player->actor.shape.rot.y = player->yaw + (s16)sSpinRotAccum;
    player->actor.focus.rot.x = 0;
    player->actor.focus.rot.y = player->yaw;
    player->actor.focus.rot.z = 0;
    SpawnSpinSparkle(play, player);
    SpawnSpinFloorEffect(play, player);
    if (Math_StepToF(&sSpinTimer, 0.0f, fabsf(sSpinAngularSpeed)) || player->invincibilityTimer < 0 ||
        !CanAct(player)) {
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw;
        player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
        StopDekuAction(player, play);
    }
}

static void StartSpin(Player* player, PlayState* play) {
    if (!CanAct(player) || !IsGrounded(player) || sSpinAnim == NULL) {
        return;
    }
    sAction = DEKU_ACTION_SPIN;
    sSpinAngularSpeed = SPIN_ROTATION;
    sSpinTimer = SPIN_TIMER;
    sSpinRotAccum = 0;
    sSpinMarkContinue = false;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    StartSpinTrail(play);
    // Keep ownership of the action after walking off a ledge. Otherwise OoT's
    // airborne dispatcher replaces the spin with its fall/auto-jump action.
    SetupAirAction(play, player, DekuSpinAction);
    ChangeAnim(play, player, sSpinAnim, ANIMMODE_ONCE, -3.0f);
    PlayMmSfx(MM_DEKU_SPIN, NA_SE_IT_SWORD_SWING, &player->actor.projectedPos);
}

static int32_t StartSpinAction(PlayState* play, Player* player) {
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (sAction == DEKU_ACTION_BUBBLE_CHARGE) {
        // A belongs to the bubble pipeline while aiming: TickBubbleCharge uses
        // it to cancel, exactly like NEI, instead of starting a spin underneath it.
        return SOH_FORM_ACTION_BLOCKED;
    }
    if (sAction == DEKU_ACTION_WATER_HOP || !CanAct(player) || !IsGrounded(player) || sSpinAnim == NULL) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    StartSpin(player, play);
    return SOH_FORM_ACTION_STARTED;
}

static int32_t HandleZTargetA(PlayState* play, Player* player) {
    const s8 direction = player->controlStickDirections[player->controlStickDataIndex];

    // Handler 10 also owns vanilla's sidehop and backflip. NEI only redirects the
    // neutral/forward roll path to Deku spin; sideways and backwards stay vanilla.
    if (direction > PLAYER_STICK_DIR_FORWARD) {
        return SOH_FORM_ACTION_VANILLA;
    }
    return StartSpinAction(play, player);
}

// ---- bubble ----

static void PopBubble(PlayState* play) {
    if (!sBubbleActive) {
        return;
    }
    for (s32 i = 0; i < 10; i++) {
        Vec3f velocity = { Rand_CenteredFloat(2.0f), Rand_CenteredFloat(2.0f), Rand_CenteredFloat(2.0f) };
        Vec3f accel = { 0.0f, 0.02f, 0.0f };
        EffectSsBubble_Spawn(play, &sBubblePos, 0.0f, 8.0f, 8.0f, 0.12f);
        (void)velocity;
        (void)accel;
    }
    // 2Ship's EnArrow only stops requesting the flagged flight sound here.
    // DEKUNUTS_FIRE is a one-shot and must be allowed to finish naturally.
    PlayMmSfx(MM_DEKU_BUBBLE_VANISH, NA_SE_EV_WATER_BUBBLE, &sBubblePos);
    sBubbleActive = false;
    sBubbleLife = 0;
}

static void AimBubble(Player* player) {
    Actor* target = player->focusActor;
    Vec3f origin;
    f32 speed;

    if (sBubbleMouthReady) {
        origin = sBubbleMouthPos;
    } else {
        origin.x = player->actor.world.pos.x;
        origin.y = player->actor.world.pos.y + 30.0f;
        origin.z = player->actor.world.pos.z;
    }
    if (!sBubbleFirstPerson && Player_IsZTargeting(player) && target != NULL && target->update != NULL) {
        sBubbleRotY = Math_Vec3f_Yaw(&origin, &target->focus.pos);
        sBubbleRotX = Math_Vec3f_Pitch(&origin, &target->focus.pos);
    } else if (sBubbleFirstPerson) {
        Z64Aiming_GetDirection(player, &sBubbleRotY, &sBubbleRotX);
    } else {
        sBubbleRotY = player->actor.shape.rot.y;
        sBubbleRotX = 0;
    }
    sBubbleScale = MAX(sBubbleCharge, 1.0f);
    speed = CLAMP(16.0f - sBubbleScale, 1.0f, 80.0f);
    sBubbleStep.x = Math_SinS(sBubbleRotY) * Math_CosS(sBubbleRotX) * speed;
    sBubbleStep.y = -Math_SinS(sBubbleRotX) * speed;
    sBubbleStep.z = Math_CosS(sBubbleRotY) * Math_CosS(sBubbleRotX) * speed;
    sBubblePos = origin;
}

static void RefreshBubbleStep(void) {
    f32 speed = CLAMP(16.0f - sBubbleScale, 1.0f, 80.0f);

    sBubbleStep.x = Math_SinS(sBubbleRotY) * Math_CosS(sBubbleRotX) * speed;
    sBubbleStep.y = -Math_SinS(sBubbleRotX) * speed;
    sBubbleStep.z = Math_CosS(sBubbleRotY) * Math_CosS(sBubbleRotX) * speed;
}

static void FireBubble(Player* player, PlayState* play) {
    if (!HasBubbleMagic()) {
        // 2Ship func_808305BC returns zero below 2 MP, so no EnArrow exists;
        // Player_UpperAction_7 plays this dry-fire sound instead.
        PlayMmSfx(MM_DEKU_BUBBLE_MISS_FIRE, NA_SE_IT_SLING_FLICK, &player->actor.projectedPos);
        return;
    }
    gSaveContext.magic -= BUBBLE_MAGIC_COST;
    AimBubble(player);
    // MM's EnArrow clamps a released bubble to 3.5. A scale-1 tap otherwise
    // reaches the shrink endpoint immediately and plays VANISH over FIRE.
    sBubbleScale = MAX(sBubbleScale, 3.5f);
    RefreshBubbleStep();
    sBubbleLife = BUBBLE_LIFE;
    sBubbleWobbleX = sBubbleWobbleY = 0;
    sBubbleState = 0;
    sBubbleActive = true;
    PlayMmSfx(MM_DEKU_BUBBLE_FIRE, NA_SE_IT_SLING_SHOT, &player->actor.projectedPos);
}

static void FireFlightBubble(Player* player, PlayState* play) {
    if (sBubbleActive) {
        return;
    }
    sBubbleCharge = BUBBLE_MAX_CHARGE;
    player->actor.focus.rot.y = player->actor.shape.rot.y;
    player->actor.focus.rot.x = 0;
    AimBubble(player);
    // MM lets Deku drop a projectile during flower flight without leaving the flight action.
    sBubbleStep.x += player->actor.velocity.x * 0.5f;
    sBubbleStep.z += player->actor.velocity.z * 0.5f;
    sBubbleLife = BUBBLE_LIFE;
    sBubbleWobbleX = sBubbleWobbleY = 0;
    sBubbleState = 0;
    sBubbleActive = true;
    PlayMmSfx(MM_DEKU_BUBBLE_FIRE, NA_SE_IT_SLING_SHOT, &player->actor.projectedPos);
}

static void BubbleChargeAction(Player* player, PlayState* play);

static void EndBubbleCharge(Player* player, PlayState* play, bool fire) {
    bool ownsWholeBody = player->actionFunc == BubbleChargeAction;

    if (fire) {
        FireBubble(player, play);
        if (sBubbleShootAnim != NULL) {
            SkelAnime* skeleton = ownsWholeBody ? &player->skelAnime : &player->upperSkelAnime;
            LinkAnimation_Change(play, skeleton, sBubbleShootAnim, 1.0f, 0.0f,
                                 Animation_GetLastFrame(sBubbleShootAnim), ANIMMODE_ONCE, -2.0f);
        }
    }
    if (sBubbleFirstPerson) {
        Z64Aiming_Release(player, play);
    }
    sBubbleFirstPerson = false;
    sBubbleArmed = false;
    sBubbleCharge = 0.0f;
    sBubbleFullChargeTimer = 0;
    sBubbleCanFire = false;
    sDekuCheekScale = 1.0f;
    sAction = DEKU_ACTION_NONE;
    if (ownsWholeBody) {
        func_80839FFC(player, play);
    }
}

static void ShootBubbleAndKeepAiming(Player* player, PlayState* play) {
    bool ownsWholeBody = player->actionFunc == BubbleChargeAction;

    FireBubble(player, play);
    if (sBubbleShootAnim != NULL) {
        SkelAnime* skeleton = ownsWholeBody ? &player->skelAnime : &player->upperSkelAnime;
        LinkAnimation_Change(play, skeleton, sBubbleShootAnim, 1.0f, 0.0f,
                             Animation_GetLastFrame(sBubbleShootAnim), ANIMMODE_ONCE, -2.0f);
    }
    sBubbleArmed = false;
    sBubbleCharge = 0.0f;
    sBubbleFullChargeTimer = 0;
    sBubbleCanFire = false;
    sDekuCheekScale = 1.0f;
}

static void UpdateBubbleAimPose(Player* player) {
    Actor* target = player->focusActor;

    if (!Player_IsZTargeting(player) || target == NULL || target->update == NULL) {
        return;
    }
    player->actor.focus.rot.y = Math_Vec3f_Yaw(&player->actor.focus.pos, &target->focus.pos);
    player->actor.focus.rot.x = Math_Vec3f_Pitch(&player->actor.focus.pos, &target->focus.pos);
    // This is OoT's own head/upper-body tracking splitter. It clamps and smooths
    // the target rotation between headLimbRot and upperLimbRot exactly as Link does.
    func_80836AB8(player, false);
}

static void TickBubbleCharge(Player* player, PlayState* play) {
    bool zTargeting = Player_IsZTargeting(player);
    bool zHeld = CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_Z);

    // ItemCamera_Update in NEI changes modes live: locking on leaves first person,
    // losing lock-on enters it. Only first person owns the whole-body action.
    if (sBubbleFirstPerson && (zTargeting || zHeld)) {
        Z64Aiming_Release(player, play);
        sBubbleFirstPerson = false;
        if (player->actionFunc == BubbleChargeAction) {
            func_80839FFC(player, play);
        }
    } else if (!sBubbleFirstPerson && !zTargeting && !zHeld) {
        if (!Z64Aiming_Request(player, play)) {
            EndBubbleCharge(player, play, false);
            return;
        }
        sBubbleFirstPerson = true;
        Player_SetupAction(play, player, BubbleChargeAction, 0);
        ChangeAnim(play, player, sBubbleReadyAnim, ANIMMODE_LOOP, -3.0f);
    }

    if (sBubbleFirstPerson) {
        Z64Aiming_Update(player, play);
        Player_ZeroSpeedXZ(player);
    } else {
        // Z-target is an upper-body item state in NEI. The current vanilla lower
        // action remains responsible for walk/run/strafe and landing.
        UpdateBubbleAimPose(player);
    }
    if (player->stateFlags1 & (PLAYER_STATE1_DAMAGED | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DEAD)) {
        EndBubbleCharge(player, play, false);
        return;
    }
    if (play->state.input[0].press.button & (BTN_A | BTN_CUP | BTN_CDOWN | BTN_CLEFT | BTN_CRIGHT | BTN_R)) {
        EndBubbleCharge(player, play, false);
        return;
    }
    if (CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_B)) {
        f32 cheekTarget;

        if (!sBubbleArmed) {
            // After an automatic full-charge shot, MM waits for a fresh button
            // press instead of starting another charge from the same hold.
            if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
                return;
            }
            sBubbleArmed = true;
            sBubbleFullChargeTimer = 0;
            sBubbleCanFire = HasBubbleMagic();
            if (sBubbleReadyAnim != NULL) {
                SkelAnime* skeleton = player->actionFunc == BubbleChargeAction ? &player->skelAnime
                                                                               : &player->upperSkelAnime;
                LinkAnimation_Change(play, skeleton, sBubbleReadyAnim, 1.0f, 0.0f,
                                     Animation_GetLastFrame(sBubbleReadyAnim), ANIMMODE_LOOP, -3.0f);
            }
        }
        if (!sBubbleCanFire) {
            sBubbleCharge = 0.0f;
            sDekuCheekScale = 1.0f;
            return;
        }
        f32 chargeStep = Math_SmoothStepToF(&sBubbleCharge, BUBBLE_MAX_CHARGE, 0.07f, 1.8f, 0.0f);
        cheekTarget = 1.0f + (sBubbleCharge / BUBBLE_MAX_CHARGE) * 0.3f;
        Math_SmoothStepToF(&sDekuCheekScale, cheekTarget, 0.3f, 0.05f, 0.01f);
        if (chargeStep > 0.5f) {
            // Verbatim MM dispatch: Actor_PlaySfx_Flagged(..., id - SFX_FLAG).
            // Removing 0x800 is what makes this a per-frame request instead of
            // a persistent one-shot in the MM scheduler.
            RefreshMmSfx(MM_DEKU_BUBBLE_BREATH_FLAGGED, &player->actor.projectedPos);
            sBubbleFullChargeTimer = 0;
        } else {
            // EnArrow waits 21 frames at full size, then releases automatically.
            if (++sBubbleFullChargeTimer > 20) {
                ShootBubbleAndKeepAiming(player, play);
                return;
            }
        }
        if ((play->gameplayFrames & 3) == 0) {
            Vec3f* effectPos = sBubbleMouthReady ? &sBubbleMouthPos : &player->bodyPartsPos[PLAYER_BODYPART_HEAD];
            EffectSsBubble_Spawn(play, effectPos, 2.0f, 3.0f, 3.0f, 0.07f);
        }
        return;
    }
    if (sBubbleArmed) {
        ShootBubbleAndKeepAiming(player, play);
    }
}

static void BubbleChargeAction(Player* player, PlayState* play) {
    LinkAnimation_Update(play, &player->skelAnime);
    TickBubbleCharge(player, play);
}

static int32_t StartBubble(PlayState* play, Player* player) {
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (sAction == DEKU_ACTION_BUBBLE_CHARGE) {
        // The existing aim session re-arms on B; do not recreate its camera or
        // replace the lower-body Z-target action.
        return SOH_FORM_ACTION_STARTED;
    }
    if (sAction == DEKU_ACTION_WATER_HOP || !CanAct(player) || !IsGrounded(player) || sBubbleReadyAnim == NULL) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    sBubbleFirstPerson = !Player_IsZTargeting(player);
    if (sBubbleFirstPerson && !Z64Aiming_Request(player, play)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    sAction = DEKU_ACTION_BUBBLE_CHARGE;
    sBubbleArmed = true;
    sBubbleCharge = 0.0f;
    sBubbleFullChargeTimer = 0;
    sBubbleCanFire = HasBubbleMagic();
    if (sBubbleFirstPerson) {
        Player_SetupAction(play, player, BubbleChargeAction, 0);
        ChangeAnim(play, player, sBubbleReadyAnim, ANIMMODE_LOOP, -3.0f);
    } else {
        // Keep the lower-body action so lock-on walking and strafing continue.
        LinkAnimation_Change(play, &player->upperSkelAnime, sBubbleReadyAnim, 1.0f, 0.0f,
                             Animation_GetLastFrame(sBubbleReadyAnim), ANIMMODE_LOOP, -3.0f);
        UpdateBubbleAimPose(player);
    }
    return SOH_FORM_ACTION_STARTED;
}

static void UpdateBubble(Player* player, PlayState* play) {
    if (!sBubbleActive) {
        return;
    }
    Vec3f previous = sBubblePos;
    if (--sBubbleLife <= 0) {
        PopBubble(play);
        return;
    }

    if (Math_StepToF(&sBubbleScale, 1.0f, 0.4f)) {
        PopBubble(play);
        return;
    }
    sBubbleWobbleX += (s16)(sBubbleScale * (500.0f + Rand_ZeroFloat(1400.0f)));
    sBubbleWobbleY += (s16)(sBubbleScale * (500.0f + Rand_ZeroFloat(1400.0f)));
    sBubbleRotX += (s16)(500.0f * Math_SinS(sBubbleWobbleX));
    sBubbleRotY += (s16)(500.0f * Math_SinS(sBubbleWobbleY));
    f32 speed = CLAMP(16.0f - sBubbleScale, 1.0f, 80.0f);
    sBubbleStep.x = Math_SinS(sBubbleRotY) * Math_CosS(sBubbleRotX) * speed;
    sBubbleStep.y = -Math_SinS(sBubbleRotX) * speed;
    sBubbleStep.z = Math_CosS(sBubbleRotY) * Math_CosS(sBubbleRotX) * speed;
    sBubblePos.x += sBubbleStep.x;
    sBubblePos.y += sBubbleStep.y;
    sBubblePos.z += sBubbleStep.z;
    // 2Ship: Actor_PlaySfx_Flagged(..., id - SFX_FLAG), once per flight frame.
    RefreshMmSfx(MM_DEKU_BUBBLE_SHOT_LEVEL_FLAGGED, &sBubblePos);

    CollisionPoly* poly = NULL;
    s32 bgId;
    Vec3f hit;
    bool hitWall = BgCheck_EntityLineTest1(&play->colCtx, &previous, &sBubblePos, &hit, &poly, true, true, true,
                                           true, &bgId);
    InitCylinder(play, player, &sBubbleCollider, &sBubbleColliderInit, &sBubbleColliderReady);
    sBubbleCollider.dim.radius = MAX(BUBBLE_RADIUS_MIN, (s16)(sBubbleScale * 3.0f));
    sBubbleCollider.dim.height = sBubbleCollider.dim.radius * 2;
    PlaceCylinder(&sBubbleCollider, &sBubblePos);
    sBubbleCollider.base.atFlags |= AT_ON;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sBubbleCollider.base);

    if (hitWall && sBubbleState >= 0) {
        sBubblePos = previous;
        sBubbleRotY += (s16)(0x8000 + (s16)Rand_CenteredFloat(0x1F40));
        sBubbleRotX = -sBubbleRotX;
        sBubbleState = -1;
        return;
    }
    if (hitWall || (sBubbleCollider.base.atFlags & AT_HIT)) {
        sBubbleCollider.base.atFlags &= ~AT_HIT;
        PopBubble(play);
    }
}

// ---- flower and flight ----

static void EndFlight(Player* player, PlayState* play) {
    bool wasOpen = sFlightOpen;

    sFlightOpen = false;
    sFlightFromAir = false;
    sFlowerRollTimer = 0;
    StopMmSfx(MM_DEKU_FLOWER_ROLL);
    player->actor.gravity = -1.0f;
    player->actor.minVelocityY = -20.0f;
    if (wasOpen) {
        PlayMmSfx(MM_DEKU_FLOWER_CLOSE, NA_SE_PL_CHANGE_ARMS, &player->actor.projectedPos);
    }
    StopDekuAction(player, play);
    if (!IsGrounded(player)) {
        ChangeAnim(play, player, sFlightFinishAnim, ANIMMODE_ONCE, -3.0f);
    }
}

static void DekuFlightAction(Player* player, PlayState* play) {
    LinkAnimation_Update(play, &player->skelAnime);
    if (IsGrounded(player)) {
        EndFlight(player, play);
        return;
    }
    if (!sFlightOpen) {
        if (player->actor.velocity.y > 0.0f) {
            // MM's launch rises sharply; the flower opens only on the way down.
            player->actor.gravity = -5.5f;
            player->actor.minVelocityY = -20.0f;
            return;
        }
        if (!sFlightCanGlide) {
            EndFlight(player, play);
            return;
        }
        sFlightOpen = true;
        sFlowerRollTimer = 1;
        player->actor.velocity.y = 6.0f;
        player->actor.gravity = -0.5f;
        player->actor.minVelocityY = -10.0f;
        ChangeAnim(play, player, sFlightOpenAnim != NULL ? sFlightOpenAnim : sFlightFlutterAnim,
                   ANIMMODE_ONCE, -3.0f);
        PlayMmSfx(MM_DEKU_FLOWER_OPEN, NA_SE_PL_CHANGE_ARMS, &player->actor.projectedPos);
        return;
    }
    f32 dx = player->actor.world.pos.x - sFlightStart.x;
    f32 dz = player->actor.world.pos.z - sFlightStart.z;
    f32 remaining = sFlightDistance - sqrtf(SQ(dx) + SQ(dz));
    if ((sFlightFromAir && !(play->state.input[0].cur.button & sFlowerButton)) ||
        CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) || remaining <= 0.0f || !CanAct(player)) {
        EndFlight(player, play);
        return;
    }
    if (sFlightFromAir && ++sFlightMagicTimer >= FLOWER_AIR_MAGIC_INTERVAL) {
        sFlightMagicTimer = 0;
        if (gSaveContext.magic == 0) {
            EndFlight(player, play);
            return;
        }
        gSaveContext.magic--;
    }
    if (remaining <= 300.0f && sFlightFlutterAnim != NULL && player->skelAnime.animation != sFlightFlutterAnim) {
        ChangeAnim(play, player, sFlightFlutterAnim, ANIMMODE_LOOP, -3.0f);
    }
    if (player->skelAnime.animation == sFlightFlutterAnim &&
        LinkAnimation_OnFrame(&player->skelAnime, 6.0f)) {
        PlayMmSfx(MM_DEKU_FLOWER_STRUGGLE, NA_SE_PL_JUMP, &player->actor.projectedPos);
    }
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
        FireFlightBubble(player, play);
    }

    Input* input = &play->state.input[0];
    f32 magnitude = sqrtf(SQ(input->rel.stick_x) + SQ(input->rel.stick_y));
    if (magnitude > 8.0f) {
        Camera* camera = GET_ACTIVE_CAM(play);
        s16 targetYaw = Camera_GetInputDirYaw(camera) + Math_Atan2S(input->rel.stick_x, input->rel.stick_y);
        Math_ScaledStepToS(&player->yaw, targetYaw, 0x500);
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw;
        Math_StepToF(&player->linearVelocity, FLIGHT_SPEED, FLIGHT_ACCEL);
    } else {
        Math_StepToF(&player->linearVelocity, 2.0f, 0.15f);
    }
    if (player->actor.velocity.y < -0.38f) {
        player->actor.velocity.y = -0.38f;
    }
    player->actor.gravity = -0.2f;
    player->actor.minVelocityY = -0.38f;
    player->fallStartHeight = player->actor.world.pos.y;
    // Audio_PlaySfx_AtPosWithTimer(..., 2.0f) in MM resolves to the configured
    // maximum-petal-speed interval: one pulse every two frames.
    if (--sFlowerRollTimer <= 0) {
        PlayMmSfx(MM_DEKU_FLOWER_ROLL, NA_SE_PL_PLANT_MOVE, &player->actor.projectedPos);
        sFlowerRollTimer = 2;
    }
}

static void LaunchFromFlower(Player* player, PlayState* play) {
    bool charged = sFlowerCharge >= FLOWER_GLIDE_CHARGE;
    player->actor.scale.x = player->actor.scale.y = player->actor.scale.z = 0.01f;
    player->actor.shape.yOffset = 0.0f;
    player->actor.velocity.y = charged ? 27.0f : 14.5f;
    player->actor.gravity = -5.5f;
    player->actor.minVelocityY = -20.0f;
    player->linearVelocity = 2.0f;
    sFlightCanGlide = charged;
    sFlightFromAir = false;
    sFlightDistance = charged ? FLOWER_LONG_DISTANCE : FLOWER_SHORT_DISTANCE;
    sFlightStart = player->actor.world.pos;
    sFlightOpen = false;
    sAction = DEKU_ACTION_FLIGHT;
    SetupAirAction(play, player, DekuFlightAction);
    ChangeAnim(play, player, sFlightLaunchAnim, ANIMMODE_ONCE, -2.0f);
}

static void DekuFlowerAction(Player* player, PlayState* play) {
    LinkAnimation_Update(play, &player->skelAnime);
    Player_ZeroSpeedXZ(player);
    sFlowerPos = player->actor.world.pos;
    if (sFlowerStage == 0) {
        sFlowerDepth += sFlowerSinkSpeed;
        if (sFlowerDepth <= -1000.0f) {
            sFlowerDepth = -1000.0f;
            sFlowerSinkSpeed = 0.0f;
            sFlowerStage = 1;
        }
    } else if (sFlowerStage == 1) {
        sFlowerSinkSpeed = MAX(sFlowerSinkSpeed - 22.0f, -170.0f);
        sFlowerDepth += sFlowerSinkSpeed;
        if (sFlowerDepth <= -3900.0f) {
            sFlowerDepth = -3900.0f;
            sFlowerStage = 2;
            player->yaw = Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
            player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw;
            player->actor.scale.x = player->actor.scale.y = player->actor.scale.z = 0.01f;
        } else {
            player->actor.scale.y = 0.01f + Math_SinS((1000.0f + sFlowerDepth) * -30.0f) * 0.004f;
            player->actor.scale.x = player->actor.scale.z = 0.01f + sFlowerSinkSpeed * 0.000015f;
            player->actor.shape.rot.y += (s16)(sFlowerSinkSpeed * 130.0f);
        }
    } else if (sFlowerStage == 2) {
        if (play->state.input[0].cur.button & sFlowerButton) {
            if (sFlowerCharge < FLOWER_MAX_CHARGE && ++sFlowerCharge == FLOWER_GLIDE_CHARGE) {
                SpawnDekuDust(play, player, 140);
            }
            f32 speedTarget = 0.0f;
            s16 yawTarget = player->yaw;
            Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, SPEED_MODE_CURVED, play);
            Math_ScaledStepToS(&player->yaw, yawTarget, 0x258);
        } else {
            sFlowerStage = 3;
            sFlowerSinkSpeed = sFlowerCharge >= FLOWER_GLIDE_CHARGE ? 2700.0f : 1450.0f;
            PlayMmSfx(MM_DEKU_FLOWER_EXIT, NA_SE_PL_JUMP, &player->actor.projectedPos);
        }
    } else {
        sFlowerDepth += sFlowerSinkSpeed;
        if (sFlowerDepth >= 0.0f) {
            LaunchFromFlower(player, play);
            return;
        }
    }

    player->actor.shape.yOffset = sFlowerDepth;
    if (sFlowerDepth < -1500.0f && ++sFlowerTimer == 8) {
        PlayMmSfx(MM_DEKU_FLOWER_BUD, NA_SE_PL_PULL_UP_PLANT, &player->actor.projectedPos);
    }
    if ((sFlowerStage == 1 || sFlowerStage == 3) && (play->gameplayFrames & 1) == 0) {
        SpawnDekuDust(play, player, 90);
    }
}

static void StartFlower(Player* player, PlayState* play, u16 buttonMask) {
    if (!CanAct(player) || !IsGrounded(player) || sFlightLaunchAnim == NULL) {
        return;
    }
    sAction = DEKU_ACTION_FLOWER;
    sFlowerTimer = 0;
    sFlowerCharge = 0;
    sFlowerDepth = 0.0f;
    sFlowerSinkSpeed = -2000.0f;
    sFlowerStage = 0;
    sFlowerButton = buttonMask;
    sFlowerPos = player->actor.world.pos;
    Player_SetupAction(play, player, DekuFlowerAction, 0);
    ChangeAnim(play, player, sSpinAnim, ANIMMODE_ONCE, -3.0f);
    PlayMmSfx(MM_DEKU_FLOWER_ENTER, NA_SE_PL_PULL_UP_PLANT, &player->actor.projectedPos);
}

static void StartAirFlowerGlide(Player* player, PlayState* play, u16 buttonMask) {
    sFlowerButton = buttonMask;
    sFlightCanGlide = true;
    sFlightFromAir = true;
    sFlightMagicTimer = 0;
    sFlightDistance = FLOWER_LONG_DISTANCE;
    sFlightStart = player->actor.world.pos;
    sFlightOpen = true;
    sFlowerRollTimer = 1;
    sAction = DEKU_ACTION_FLIGHT;
    player->actor.gravity = -0.2f;
    player->actor.minVelocityY = -0.38f;
    if (player->actor.velocity.y < -0.38f) {
        player->actor.velocity.y = -0.38f;
    }
    SetupAirAction(play, player, DekuFlightAction);
    ChangeAnim(play, player, sFlightOpenAnim != NULL ? sFlightOpenAnim : sFlightFlutterAnim,
               ANIMMODE_ONCE, -3.0f);
    PlayMmSfx(MM_DEKU_FLOWER_OPEN, NA_SE_PL_CHANGE_ARMS, &player->actor.projectedPos);
}

static u16 FindDekuLeafButtons(void) {
    if (!SOH_MOD_API_HAS(sApi, GetEquippedCustomItem)) {
        return 0;
    }
    for (u8 button = 0; button < DEKU_LEAF_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, DEKU_LEAF_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static void TryStartFlower(Player* player, PlayState* play) {
    u16 buttons = FindDekuLeafButtons();

    if (buttons == 0 || !(play->state.input[0].press.button & buttons) ||
        (sAction != DEKU_ACTION_NONE && sAction != DEKU_ACTION_WATER_HOP) || !CanAct(player) ||
        sFlightLaunchAnim == NULL) {
        return;
    }
    bool grounded = IsGrounded(player);
    u8 magicCost = grounded ? DEKU_FLOWER_MAGIC_COST : 1;
    if (!gSaveContext.isMagicAcquired || gSaveContext.magic < magicCost) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return;
    }
    if (grounded) {
        gSaveContext.magic -= magicCost;
        StartFlower(player, play, buttons);
    } else {
        // Like the regular Deku Leaf glide, airborne use pays while held.
        StartAirFlowerGlide(player, play, buttons);
    }
}

// ---- water hopping ----

static bool FindEquippedRocsButton(u16* buttonMask, bool* cape) {
    if (!SOH_MOD_API_HAS(sApi, GetEquippedCustomItem)) {
        return false;
    }
    for (u8 button = 0; button < DEKU_LEAF_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped == NULL) {
            continue;
        }
        if (strcmp(equipped, ROCS_CAPE_KEY) == 0 || strcmp(equipped, ROCS_FEATHER_KEY) == 0) {
            *buttonMask = sItemButtons[button];
            *cape = strcmp(equipped, ROCS_CAPE_KEY) == 0;
            return true;
        }
    }
    return false;
}

static void TryUseDekuRocs(Player* player, PlayState* play) {
    u16 buttonMask = 0;
    bool cape = false;

    if (!FindEquippedRocsButton(&buttonMask, &cape) ||
        !CHECK_BTN_ALL(play->state.input[0].press.button, buttonMask) || sAction != DEKU_ACTION_NONE ||
        !CanAct(player)) {
        return;
    }
    if (player->stateFlags1 & PLAYER_STATE1_IN_WATER) {
        player->actor.velocity.y = DEKU_ROCS_WATER_VELOCITY;
        PlayMmSfx(MM_DEKU_JUMP1, NA_SE_PL_SKIP, &player->actor.projectedPos);
        return;
    }
    if (IsGrounded(player)) {
        sRocsDoubleJumped = false;
        sRocsGroundJumpFrame = play->state.frames;
        func_80838940(player, sJumpAnim, DEKU_ROCS_JUMP_VELOCITY, play, NA_SE_VO_LI_AUTO_JUMP);
        player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
        return;
    }
    if (cape && !sRocsDoubleJumped && play->state.frames != sRocsGroundJumpFrame) {
        sRocsDoubleJumped = true;
        func_80838940(player, sRunJumpAnim != NULL ? sRunJumpAnim : sJumpAnim, DEKU_ROCS_JUMP_VELOCITY, play,
                      NA_SE_VO_LI_AUTO_JUMP);
        player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
    }
}

// Unlike MM's decay toward half the stick speed, a hop keeps its momentum: the stick only steers or brakes.
static void SteerHop(Player* player, PlayState* play) {
    f32 targetSpeed = 0.0f;
    s16 targetYaw = player->yaw;

    Player_GetMovementSpeedAndYaw(player, &targetSpeed, &targetYaw, SPEED_MODE_CURVED, play);
    if (targetSpeed <= 0.1f) {
        return;
    }
    if (ABS((s16)(player->yaw - targetYaw)) > 0x6000) {
        Math_StepToF(&player->linearVelocity, 0.0f, 0.5f);
        return;
    }
    if (targetSpeed > player->linearVelocity) {
        Math_StepToF(&player->linearVelocity, targetSpeed, 0.2f);
    }
    Math_ScaledStepToS(&player->yaw, targetYaw, 0x190);
}

static void WaterHopAction(Player* player, PlayState* play) {
    LinkAnimation_Update(play, &player->skelAnime);
    if (IsGrounded(player)) {
        sAction = DEKU_ACTION_NONE;
        func_80839FFC(player, play);
        return;
    }
    SteerHop(player, play);
    player->actor.gravity = -1.0f;
    player->actor.minVelocityY = -12.0f;
}

// MM func_8083784C: only water too deep to wade in is skipped over.
static bool IsInDeepWater(Player* player) {
    f32 depth = player->actor.yDistToWater;
    f32 floorDistance = player->actor.world.pos.y - player->actor.floorHeight;

    return depth > 0.0f && (player->ageProperties->unk_2C - depth) < floorDistance;
}

static void ReleaseActionForHop(Player* player, PlayState* play) {
    if (sAction == DEKU_ACTION_BUBBLE_CHARGE) {
        EndBubbleCharge(player, play, false);
    } else if (sAction == DEKU_ACTION_SPIN) {
        player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
        StopSpinTrail(play);
    }
    player->actor.scale.x = player->actor.scale.y = player->actor.scale.z = 0.01f;
    sAction = DEKU_ACTION_NONE;
}

static void StartLastHopSpin(Player* player, PlayState* play) {
    sAction = DEKU_ACTION_SPIN;
    sSpinAngularSpeed = SPIN_ROTATION;
    sSpinTimer = SPIN_TIMER;
    sSpinRotAccum = 0;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    StartSpinTrail(play);
    SetupAirAction(play, player, DekuSpinAction);
    ChangeAnim(play, player, sSpinAnim, ANIMMODE_ONCE, -2.0f);
    PlayMmSfx(MM_DEKU_SPIN, NA_SE_IT_SWORD_SWING, &player->actor.projectedPos);
}

// Mirrors MM func_808373F8's Deku branch: lift to the surface and relaunch, keeping horizontal speed.
static void HopOffWater(Player* player, PlayState* play) {
    f32 used = WATER_HOPS - sWaterHops;
    f32 hopSpeed = MAX(MAX(player->linearVelocity, WATER_HOP_SPEED) * (0.3f + (used * 0.18f)), 4.0f);
    bool useRunJump = ABS((s16)(player->yaw - player->actor.shape.rot.y)) < 0x1000 && player->linearVelocity > 4.0f;

    ReleaseActionForHop(player, play);
    player->actor.world.pos.y += player->actor.yDistToWater;
    player->actor.velocity.y = hopSpeed;
    player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    player->stateFlags1 &= ~PLAYER_STATE1_IN_WATER;
    player->stateFlags2 &= ~PLAYER_STATE2_UNDERWATER;
    player->fallStartHeight = player->actor.world.pos.y;
    Vec3f splashPos = player->actor.world.pos;
    EffectSsGSplash_Spawn(play, &splashPos, NULL, NULL, hopSpeed <= 10.0f ? 0 : 1, (s16)(hopSpeed * 50.0f));
    PlayMmSfx((u16)(MM_DEKU_JUMP1 + used), NA_SE_PL_JUMP, &player->actor.projectedPos);
    // The hops are close enough together that the MM bridge can still own the
    // preceding auto-jump voice. Restart the final one so it is not rejected as
    // a duplicate while preserving MM's hop -> voice -> last-hop spin order.
    if (sWaterHops == 1) {
        StopMmSfx(MM_DEKU_AUTO_JUMP_VOICE);
    }
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_AUTO_JUMP);
    sWaterHops--;
    if (sWaterHops == 0) {
        StartLastHopSpin(player, play);
        return;
    }
    sAction = DEKU_ACTION_WATER_HOP;
    SetupAirAction(play, player, WaterHopAction);
    ChangeAnim(play, player, useRunJump ? sRunJumpAnim : sJumpAnim, ANIMMODE_ONCE, -2.0f);
}

static void VoidOutOfWater(PlayState* play) {
    StopSpinTrail(play);
    sAction = DEKU_ACTION_NONE;
    Play_TriggerVoidOut(play);
    play->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
    Sfx_PlaySfxCentered(NA_SE_OC_ABYSS);
}

// Runs after Player_UpdateCommon: the first contact frame is never deeper than one fall step (<= 20),
// below both swim thresholds (32/56), so the hop always lands before OoT's resolver can keep Deku swimming.
static void UpdateWaterContact(Player* player, PlayState* play) {
    bool swimming = (player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0;

    if (IsGrounded(player) && !swimming) {
        sWaterHops = WATER_HOPS;
        return;
    }
    if (!IsInDeepWater(player) || (!swimming && player->actor.velocity.y >= 0.0f) || gSaveContext.health == 0 ||
        play->transitionTrigger != TRANS_TRIGGER_OFF ||
        (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_LOADING | PLAYER_STATE1_IN_CUTSCENE))) {
        return;
    }
    if (sWaterHops == 0) {
        VoidOutOfWater(play);
        return;
    }
    HopOffWater(player, play);
}

// ---- guard ----

static void DekuGuardAction(Player* player, PlayState* play) {
    LinkAnimation_Update(play, &player->skelAnime);
    Player_ZeroSpeedXZ(player);
    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R) || !CanAct(player)) {
        StopDekuAction(player, play);
        return;
    }
    InitCylinder(play, player, &sGuardCollider, &sGuardColliderInit, &sGuardColliderReady);
    PlaceCylinder(&sGuardCollider, &player->actor.world.pos);
    sGuardCollider.base.acFlags |= AC_ON;
    CollisionCheck_SetAC(play, &play->colChkCtx, &sGuardCollider.base);
    // The wooden shell is a full-body guard. Keep ordinary damage from racing the custom AC pass.
    if (player->invincibilityTimer == 0) {
        player->invincibilityTimer = 1;
    }
}

static int32_t StartGuard(PlayState* play, Player* player) {
    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (sAction != DEKU_ACTION_NONE || !CanAct(player) || !IsGrounded(player)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    sAction = DEKU_ACTION_GUARD;
    Player_SetupAction(play, player, DekuGuardAction, 0);
    ChangeAnim(play, player, sGuardAnim != NULL ? sGuardAnim : sSpinAnim, ANIMMODE_ONCE, -3.0f);
    player->skelAnime.playSpeed = 0.0f;
    PlayMmSfx(MM_DEKU_CHANGE_ARMS, NA_SE_IT_SHIELD_POSTURE, &player->actor.projectedPos);
    return SOH_FORM_ACTION_STARTED;
}

static const SOHFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_ROLL, StartSpinAction },
    { SOH_PLAYER_ACTION_MELEE, StartBubble },
    { SOH_PLAYER_ACTION_ZTARGET_A, HandleZTargetA },
    { SOH_PLAYER_ACTION_SHIELD, StartGuard },
};

// Let OoT select and advance idle normally, but resolve the base wait group to
// MM's pose. Do not force this animation again from UpdateDeku: doing both was
// what made the legs repeatedly snap/flex while standing.
static SOHFormAnimOverride sAnimOverrides[] = {
    { PLAYER_ANIMGROUP_wait, -1, NULL },
};

// ---- model and drawing ----

static void ResolveDekuBody(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!IsDeku() || limbIndex <= PLAYER_LIMB_NONE || limbIndex >= PLAYER_LIMB_MAX) {
        return;
    }
    if (limbIndex != PLAYER_LIMB_ROOT && pos != NULL) {
        *pos = sDekuJointPos[limbIndex];
    }
    if (limbIndex == PLAYER_LIMB_HEAD && sDekuCheekScale > 1.01f) {
        Matrix_Scale(sDekuCheekScale, sDekuCheekScale, sDekuCheekScale, MTXMODE_APPLY);
    }
    const char* path = sDekuLimbDL[limbIndex];
    if (path != NULL) {
        Gfx* deku = ResourceMgr_LoadGfxByName(path);
        if (deku != NULL) {
            *dList = deku;
        }
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        *dList = NULL;
    }
}

static void DrawOpenFlower(PlayState* play, Player* player, int32_t limbIndex) {
    Vec3f mouthOffset = { 1300.0f, -400.0f, 0.0f };
    Vec3f forwardOffset = { 2300.0f, -400.0f, 0.0f };

    if (!IsDeku()) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_HAT && sAction == DEKU_ACTION_SPIN && sSpinTrail >= 0) {
        Vec3f tipLocal = { 3000.0f, 0.0f, 0.0f };
        Vec3f baseLocal = { 2300.0f, 0.0f, 0.0f };
        Vec3f tip;
        Vec3f base;

        // MM z_player_lib.c: the Deku spin ribbon follows these two points on
        // the hat's local X axis, rather than the actor origin or head center.
        Matrix_MultVec3f(&tipLocal, &tip);
        Matrix_MultVec3f(&baseLocal, &base);
        EffectBlure_AddVertex(Effect_GetByIndex(sSpinTrail), &tip, &base);
    }
    if (sAction == DEKU_ACTION_FLIGHT && (limbIndex == PLAYER_LIMB_L_HAND || limbIndex == PLAYER_LIMB_R_HAND)) {
        const char* stem = limbIndex == PLAYER_LIMB_L_HAND ? sLeftFlowerStemDL : sRightFlowerStemDL;
        const char* flower = player->actor.velocity.y < -6.0f ? sClosedFlowerDL : sOpenFlowerDL;
        OPEN_DISPS(play->state.gfxCtx);
        Matrix_Push();
        Matrix_Translate(0.0f, 150.0f, 0.0f, MTXMODE_APPLY);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)stem);
        Matrix_Translate(2150.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD((s16)(play->gameplayFrames * 0x900)), MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)flower);
        Matrix_Pop();
        CLOSE_DISPS(play->state.gfxCtx);
    }
    if (limbIndex != PLAYER_LIMB_HEAD) {
        return;
    }
    // MM attaches the charging bubble to an offset in the head matrix. This
    // matrix already contains Link's upper/head limb tracking, so both points
    // follow the torso instead of being reconstructed from actor yaw.
    Matrix_MultVec3f(&mouthOffset, &sBubbleMouthPos);
    Matrix_MultVec3f(&forwardOffset, &sBubbleMouthForward);
    sBubbleMouthReady = true;
}

static void DrawWorldDL(PlayState* play, Vec3f* pos, f32 scale, f32 spin, const char* path, bool translucent) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_RotateY(spin, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    if (translucent) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)path);
    } else {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)path);
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawBubbleDL(PlayState* play, Vec3f* pos, f32 bubbleScale, s16 yaw, s16 pitch, const char* path) {
    u8 alpha = (u8)CLAMP(255 - (s32)(bubbleScale * 4.0f), 80, 255);
    f32 modelScale = MAX(bubbleScale, 0.25f) * 0.002f;

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(yaw), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(pitch), MTXMODE_APPLY);
    Matrix_Scale(modelScale, modelScale, modelScale, MTXMODE_APPLY);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    // MM's still/moving bubble lists are geometry-only. 06F380 installs the
    // texture tiles and material state they reference.
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBubbleSetupDL);
    // The MM bubble textures carry their silhouette in TEXEL alpha. The generic
    // XLU setup ignored it, which made the quad/sphere look opaque.
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 230, 225, 150, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 150, 150, 100, alpha);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)path);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawChargingBubble(Player* player, PlayState* play) {
    Camera* camera;
    Vec3f direction;
    Vec3f position;
    f32 length;

    if (sBubbleCharge <= 0.01f) {
        return;
    }
    if (!sBubbleFirstPerson) {
        s16 yaw;
        s16 pitch;

        if (sBubbleMouthReady) {
            position = sBubbleMouthPos;
            yaw = Math_Vec3f_Yaw(&sBubbleMouthPos, &sBubbleMouthForward);
            pitch = Math_Vec3f_Pitch(&sBubbleMouthPos, &sBubbleMouthForward);
        } else {
            yaw = player->actor.shape.rot.y;
            pitch = 0;
            position = player->actor.focus.pos;
        }
        DrawBubbleDL(play, &position, sBubbleCharge, yaw, pitch, sBubbleStillDL);
        return;
    }
    camera = GET_ACTIVE_CAM(play);
    direction.x = camera->at.x - camera->eye.x;
    direction.y = camera->at.y - camera->eye.y;
    direction.z = camera->at.z - camera->eye.z;
    length = sqrtf(SQ(direction.x) + SQ(direction.y) + SQ(direction.z));
    if (length < 0.001f) {
        return;
    }
    direction.x /= length;
    direction.y /= length;
    direction.z /= length;
    position.x = camera->eye.x + direction.x * 3.0f;
    position.y = camera->eye.y + direction.y * 3.0f;
    position.z = camera->eye.z + direction.z * 3.0f;
    // NEI's first-person route deliberately uses OoT's self-contained effect
    // sphere; the MM 06F9F0 geometry is reserved for the visible 3D body route.
    {
        f32 chargeRatio = sBubbleCharge / BUBBLE_MAX_CHARGE;
        f32 scale = 0.02f + chargeRatio * 0.23f;
        u8 alpha = (u8)(60.0f + chargeRatio * 160.0f);

        OPEN_DISPS(play->state.gfxCtx);
        Matrix_Push();
        Matrix_Translate(position.x, position.y, position.z, MTXMODE_NEW);
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 230, 225, 150, alpha);
        gDPSetEnvColor(POLY_XLU_DISP++, 150, 150, 100, 0);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)gEffBubble1Tex);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffBubbleDL);
        Matrix_Pop();
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

static void SetSpinMarkVertex(Vtx* vtx, const Vec3f* pos) {
    vtx->v.ob[0] = (s16)pos->x;
    vtx->v.ob[1] = (s16)pos->y;
    vtx->v.ob[2] = (s16)pos->z;
    vtx->v.flag = 0;
    vtx->v.tc[0] = vtx->v.tc[1] = 0;
    vtx->v.cn[0] = vtx->v.cn[1] = vtx->v.cn[2] = vtx->v.cn[3] = 255;
}

static void DrawSpinMarks(PlayState* play) {
    if (sSpinMarkCount < 2) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_NEW);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BACK | G_LIGHTING);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
    for (s16 i = 1; i < sSpinMarkCount; i++) {
        Vtx* quad;
        u8 alpha;

        if (!sSpinMarks[i].connectPrev) {
            continue;
        }
        quad = Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));
        if (quad == NULL) {
            break;
        }
        SetSpinMarkVertex(&quad[0], &sSpinMarks[i - 1].p1);
        SetSpinMarkVertex(&quad[1], &sSpinMarks[i - 1].p2);
        SetSpinMarkVertex(&quad[2], &sSpinMarks[i].p1);
        SetSpinMarkVertex(&quad[3], &sSpinMarks[i].p2);
        alpha = (u8)((150 * sSpinMarks[i].life) / SPIN_MARK_DURATION);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 0, 0, 15, alpha);
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)quad, 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 3, 0, 0, 3, 2, 0);
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawDekuEffects(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    if (!IsDeku()) {
        return;
    }
    DrawSpinMarks(play);
    if (sAction == DEKU_ACTION_BUBBLE_CHARGE) {
        DrawChargingBubble(player, play);
    }
    if (sAction == DEKU_ACTION_FLOWER) {
        Vec3f flower = sFlowerPos;
        DrawWorldDL(play, &flower, 0.01f, 0.0f, sFlowerDL, false);
    }
    if (sBubbleActive) {
        DrawBubbleDL(play, &sBubblePos, sBubbleScale, sBubbleRotY, sBubbleRotX,
                     sBubbleLife == BUBBLE_LIFE ? sBubbleStillDL : sBubbleMoveDL);
    }
    if (sAction == DEKU_ACTION_GUARD) {
        Vec3f shell = player->actor.world.pos;
        shell.y += 4.0f;
        DrawWorldDL(play, &shell, 0.008f, BINANG_TO_RAD(player->actor.shape.rot.y), sFlowerDL, false);
    }
}

static void SpeakWithDekuVoice(Actor* actor, int32_t kind, uint16_t* sfxId, bool* handled) {
    if (kind != SOH_ACTOR_SFX_VOICE || !IsDeku()) {
        return;
    }
    const uint16_t action = (uint16_t)(*sfxId - NA_SE_VO_LI_SWORD_N);
    if (action >= LINK_VOICE_ACTIONS) {
        return;
    }
    if (sPlayMmSfx == NULL && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
    }
    if (sPlayMmSfx != NULL &&
        sPlayMmSfx((uint16_t)(NA_SE_VO_LI_SWORD_N + MM_DEKU_VOICE_OFFSET + action), &actor->projectedPos)) {
        *handled = true;
    }
}

// ---- lifetime and registration ----

static void ResetState(Player* player) {
    sAction = DEKU_ACTION_NONE;
    sFlowerTimer = 0;
    sFlowerCharge = 0;
    sFlowerDepth = 0.0f;
    sFlowerSinkSpeed = 0.0f;
    sFlowerStage = 0;
    sFlowerButton = 0;
    sFlightCanGlide = false;
    sFlightFromAir = false;
    sFlightMagicTimer = 0;
    sFlightDistance = 0.0f;
    sFlightOpen = false;
    sFlowerRollTimer = 0;
    sWaterHops = WATER_HOPS;
    sRocsDoubleJumped = false;
    sRocsGroundJumpFrame = 0;
    sBubbleCharge = 0;
    sBubbleActive = false;
    sBubbleLife = 0;
    sBubbleFirstPerson = false;
    sBubbleArmed = false;
    sBubbleFullChargeTimer = 0;
    sBubbleCanFire = false;
    sBubbleMouthReady = false;
    sDekuCheekScale = 1.0f;
    sSpinTimer = 0.0f;
    sSpinRotAccum = 0;
    sSpinMarkCount = 0;
    sSpinMarkContinue = false;
    sPreviousInvincibility = player != NULL ? player->invincibilityTimer : 0;
    if (player != NULL) {
        player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
        player->actor.scale.x = player->actor.scale.y = player->actor.scale.z = 0.01f;
        player->actor.shape.yOffset = 0.0f;
    }
}

static void EnterDeku(PlayState* play, Player* player) {
    ResetState(player);
}

// ---- pipes ----

typedef enum {
    PIPES_IDLE,
    PIPES_RAISING,
    PIPES_PLAYING,
    PIPES_LOWERING,
} PipesPhase;

typedef struct {
    s16 frame;
    Vec3s scale;
} PipeKey;

// MM's D_801C0340 (the mouthpiece) and D_801C0368 (the pipes): they pop out while pn_gakkistart plays.
static const PipeKey sMouthpieceKeys[] = {
    { 0, { 0, 0, 0 } }, { 5, { 0, 0, 0 } }, { 7, { 100, 100, 100 } }, { 9, { 110, 110, 110 } }, { 11, { 100, 100, 100 } },
};
static const PipeKey sPipeKeys[] = {
    { 0, { 0, 0, 0 } },         { 4, { 0, 0, 0 } },       { 6, { 120, 150, 60 } },  { 8, { 130, 80, 160 } },
    { 9, { 100, 100, 100 } }, { 10, { 90, 100, 90 } }, { 11, { 100, 100, 100 } },
};

static u8 sPipesPhase;
static bool sIsOcarinaOut;
static bool sWasNoteStruck;
static u8 sLastNote = OCARINA_NOTE_NONE;
// Set on the game thread while the pipes are out; the note hook runs on the audio thread and only reads it.
static MmOcarinaNoteFunc sPipesVoice;
static f32 sPipesBend = 1.0f;

// MM's func_80124618: a linear blend between the keys either side of the frame, in hundredths.
static void BlendPipeKeys(const PipeKey* keys, s32 count, f32 frame, Vec3f* scale) {
    s32 next = 1;

    while (next < count - 1 && keys[next].frame < frame) {
        next++;
    }
    const PipeKey* from = &keys[next - 1];
    const PipeKey* to = &keys[next];
    f32 span = (f32)(to->frame - from->frame);
    f32 t = span > 0.0f ? CLAMP((frame - from->frame) / span, 0.0f, 1.0f) : 1.0f;
    scale->x = (from->scale.x + (to->scale.x - from->scale.x) * t) * 0.01f;
    scale->y = (from->scale.y + (to->scale.y - from->scale.y) * t) * 0.01f;
    scale->z = (from->scale.z + (to->scale.z - from->scale.z) * t) * 0.01f;
}

static bool IsHoldingOcarina(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_OCARINA || player->rightHandType == PLAYER_MODELTYPE_RH_OOT;
}

static bool IsPlayingOcarinaClip(Player* player, const char* name) {
    const char* anim = (const char*)player->skelAnime.animation;

    return anim != NULL && ResourceMgr_OTRSigCheck((char*)anim) && strstr(anim, name) != NULL;
}

static MmOcarinaNoteFunc FindPipesVoice(void) {
    if (!SOH_MOD_API_HAS(sApi, FindService)) {
        return NULL;
    }
    return (MmOcarinaNoteFunc)sApi->FindService(MM_OCARINA_NOTE_SERVICE);
}

// The hook runs right after Ocarina triggered its own voice, so stopping that here always lands.
static void VoicePipes(uint8_t note, float bendFreq) {
    MmOcarinaNoteFunc voice = sPipesVoice;

    if (voice == NULL) {
        return;
    }
    sPipesBend = bendFreq;
    if (note != OCARINA_NOTE_NONE) {
        Audio_StopSfxById(NA_SE_OC_OCARINA);
    }
    if (note != sLastNote) {
        voice(MM_OCARINA_INSTRUMENT_DEKU_PIPES, note, &sPipesBend);
    }
}

// Runs inside the ocarina's own update; the pose consumes the edge on the game frame.
static void NoteStruck(uint8_t note, float modulator, int8_t bend) {
    (void)bend;
    if (note != OCARINA_NOTE_NONE && note != sLastNote) {
        sWasNoteStruck = true;
    }
    VoicePipes(note, modulator);
    sLastNote = note;
}

static void PlaybackNoteStruck(uint8_t note, float modulator) {
    NoteStruck(note, modulator, 0);
}

static void SilencePipes(void) {
    MmOcarinaNoteFunc voice = sPipesVoice;

    sPipesVoice = NULL;
    if (voice != NULL) {
        voice(MM_OCARINA_INSTRUMENT_DEKU_PIPES, OCARINA_NOTE_NONE, &sPipesBend);
    }
}

static void ChangePipesClip(PlayState* play, Player* player, LinkAnimationHeader* anim, f32 speed, u8 mode, f32 morph) {
    f32 last = Animation_GetLastFrame(anim);

    LinkAnimation_Change(play, &player->skelAnime, anim, speed, speed >= 0.0f ? 0.0f : last,
                         speed >= 0.0f ? last : 0.0f, mode, morph);
}

// Vanilla's ocarina action waits for the end of its intro and outro clips; the pipes' own clip takes their
// place, so that wait is what raises and lowers them, as MM's Player_Action_63 does.
static bool SwapOcarinaClip(PlayState* play, Player* player) {
    if (IsPlayingOcarinaClip(player, "okarina_start")) {
        ChangePipesClip(play, player, sPipesRaiseAnim, 1.0f, ANIMMODE_ONCE, -6.0f);
        sPipesPhase = PIPES_RAISING;
        return true;
    }
    if (IsPlayingOcarinaClip(player, "okarina_end")) {
        ChangePipesClip(play, player, sPipesRaiseAnim, -1.0f, ANIMMODE_ONCE, -6.0f);
        sPipesPhase = PIPES_LOWERING;
        return true;
    }
    return false;
}

// Each note blows the play clip once, as MM's func_80852290 does. It loops: that same action reopens the
// ocarina whenever a clip reports its end, which would wipe the notes of the song being played.
static void PosePipes(PlayState* play, Player* player) {
    f32 blowEnd = Animation_GetLastFrame(sPipesPlayAnim) - CLIP_FRAMES_PER_UPDATE;

    if (player->skelAnime.animation != sPipesPlayAnim) {
        ChangePipesClip(play, player, sPipesPlayAnim, 1.0f, ANIMMODE_LOOP, -4.0f);
        player->skelAnime.playSpeed = 0.0f;
        sPipesPhase = PIPES_PLAYING;
    }
    bool isBlowDone = player->skelAnime.curFrame >= blowEnd;
    if (sWasNoteStruck) {
        if (isBlowDone) {
            player->skelAnime.curFrame = 0.0f;
        }
        player->skelAnime.playSpeed = 1.0f;
    } else if (isBlowDone) {
        player->skelAnime.playSpeed = 0.0f;
    }
    sWasNoteStruck = false;
}

// The ocarina stays out across the message session, so the latch only lets go once no message is up.
static void UpdatePipes(PlayState* play, Player* player) {
    bool isSessionOpen = play->msgCtx.msgMode != MSGMODE_NONE;

    if (IsHoldingOcarina(player)) {
        sIsOcarinaOut = true;
    } else if (!isSessionOpen) {
        sIsOcarinaOut = false;
    }
    if (!sIsOcarinaOut) {
        sPipesPhase = PIPES_IDLE;
        if (sPipesVoice != NULL) {
            SilencePipes();
        }
        return;
    }
    if (sPipesVoice == NULL) {
        sPipesVoice = FindPipesVoice();
    }
    if (sPipesRaiseAnim == NULL || sPipesPlayAnim == NULL || SwapOcarinaClip(play, player)) {
        return;
    }
    bool isRaising = sPipesPhase == PIPES_RAISING && player->skelAnime.animation == sPipesRaiseAnim;
    if (isRaising || sPipesPhase == PIPES_LOWERING) {
        return;
    }
    // A raise that ended hands over to the play clip even before an actor opens the session it was played for.
    if (isSessionOpen || sPipesPhase == PIPES_RAISING) {
        PosePipes(play, player);
    }
}

static void DrawScaledDL(PlayState* play, const char* dList, Vec3f* scale) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(scale->x, scale->y, scale->z, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dList);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM draws the pipes from the head, grown in by their keys while the raise plays and whole after it.
static void DrawPipes(PlayState* play, Player* player, int32_t limbIndex) {
    if (!IsDeku() || limbIndex != PLAYER_LIMB_HEAD || !sIsOcarinaOut || sPipesPhase == PIPES_IDLE) {
        return;
    }
    Vec3f mouthpiece = { 1.0f, 1.0f, 1.0f };
    Vec3f pipes = { 1.0f, 1.0f, 1.0f };
    if (player->skelAnime.animation == sPipesRaiseAnim) {
        BlendPipeKeys(sMouthpieceKeys, ARRAY_COUNT(sMouthpieceKeys), player->skelAnime.curFrame, &mouthpiece);
        BlendPipeKeys(sPipeKeys, ARRAY_COUNT(sPipeKeys), player->skelAnime.curFrame, &pipes);
    }
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    DrawScaledDL(play, sPipesDL, &mouthpiece);
    for (s32 i = 0; i < PIPE_PIECE_COUNT; i++) {
        DrawScaledDL(play, sPipePieceDLs[i], &pipes);
    }
}

static void ExitDeku(PlayState* play, Player* player) {
    if (Z64Aiming_IsAiming()) {
        Z64Aiming_Release(player, play);
    }
    StopMmSfx(MM_DEKU_BUBBLE_BREATH_FLAGGED);
    StopMmSfx(MM_DEKU_BUBBLE_SHOT_LEVEL_FLAGGED);
    StopMmSfx(MM_DEKU_FLOWER_ROLL);
    StopSpinTrail(play);
    SilencePipes();
    sPipesPhase = PIPES_IDLE;
    sIsOcarinaOut = false;
    ResetState(player);
    if (player->actionFunc == DekuSpinAction || player->actionFunc == BubbleChargeAction ||
        player->actionFunc == DekuFlowerAction || player->actionFunc == DekuFlightAction ||
        player->actionFunc == WaterHopAction || player->actionFunc == DekuGuardAction) {
        func_80839FFC(player, play);
    }
}

static void UpdateDeku(PlayState* play, Player* player) {
    UpdateSpinMarks();
    if (IsGrounded(player)) {
        sRocsDoubleJumped = false;
    }
    UpdateWaterContact(player, play);
    if (player->invincibilityTimer < 0 && sPreviousInvincibility >= 0 && sAction != DEKU_ACTION_NONE) {
        if (sAction == DEKU_ACTION_BUBBLE_CHARGE) {
            EndBubbleCharge(player, play, false);
        } else {
            StopDekuAction(player, play);
        }
    }
    sPreviousInvincibility = player->invincibilityTimer;
    if (sAction == DEKU_ACTION_BUBBLE_CHARGE && player->actionFunc != BubbleChargeAction) {
        TickBubbleCharge(player, play);
    }
    UpdateBubble(player, play);
    TryStartFlower(player, play);
    TryUseDekuRocs(player, play);
    UpdatePipes(play, player);
}

static void ForgetScene(int16_t sceneNum) {
    Z64Aiming_Drop();
    StopMmSfx(MM_DEKU_BUBBLE_BREATH_FLAGGED);
    StopMmSfx(MM_DEKU_BUBBLE_SHOT_LEVEL_FLAGGED);
    StopMmSfx(MM_DEKU_FLOWER_ROLL);
    if (gPlayState != NULL) {
        StopSpinTrail(gPlayState);
    } else {
        sSpinTrail = -1;
    }
    ResetState(NULL);
    sSpinColliderReady = false;
    sBubbleColliderReady = false;
    sGuardColliderReady = false;
}

static bool CanWearMask(Player* player, PlayState* play) {
    return IsGrounded(player) &&
           !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_CUTSCENE));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(DEKU_FORM_KEY);
}

static void RegisterMask(void) {
    SOHCustomItemDefinition mask = Z64Items_Define(DEKU_MASK_KEY, sIconTex, sNameTex);
    Z64Items_SetButtons(&mask, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    mask.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    mask.disabledIconSurfaces |= SOH_ITEM_ICON_SAVE_EDITOR;
    Z64Items_SetPlacement(&mask, DEKU_MASK_PAGE, DEKU_MASK_SLOT, 0);
    Z64Items_SetGetItemModel(&mask, sGetItemDL);
    Z64Items_SetTextbox(&mask, "You got the %gDeku Mask%w!&Wear it with %y\xA1%w to become Deku Link. Press %y\x9F%w "
                               "to spin, use the Deku Leaf for a flower, hold %y\xA0%w to aim a bubble, and use "
                               "%y\xA3%w to guard.");
    Z64Items_SetPauseText(&mask, "%gDeku Mask&%wPress %y\x9F%w: spin  Deku Leaf: flower flight&Hold %y\xA0%w: "
                                 "aim bubble  Hold %y\xA3%w: guard");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    Z64Items_Register(sApi, &mask);
}

static uint16_t ResolveDekuEquipment(int32_t equipType, uint16_t value) {
    if (equipType == EQUIP_TYPE_SWORD || equipType == EQUIP_TYPE_SHIELD) {
        return equipType == EQUIP_TYPE_SWORD ? EQUIP_VALUE_SWORD_NONE : EQUIP_VALUE_SHIELD_NONE;
    }
    if (equipType == EQUIP_TYPE_BOOTS) {
        return EQUIP_VALUE_BOOTS_KOKIRI;
    }
    return value;
}

static bool AllowsDekuItem(uint16_t item, const char* customKey) {
    if (customKey != NULL) {
        return strcmp(customKey, DEKU_LEAF_KEY) == 0 || strcmp(customKey, ROCS_FEATHER_KEY) == 0 ||
               strcmp(customKey, ROCS_CAPE_KEY) == 0;
    }
    return item == ITEM_OCARINA_FAIRY || item == ITEM_OCARINA_TIME || item == ITEM_NUT;
}

static void ResolveDekuStrength(bool* should, va_list args) {
    int32_t* strength = va_arg(args, int32_t*);

    if (!IsDeku()) {
        return;
    }
    *strength = PLAYER_STR_NONE;
    *should = false;
}

// OoT's foot IK measures a human leg; on Deku's short rig it lifts the legs to plant a foot that isn't there.
static void BlockLegPlanting(bool* should, va_list args) {
    if (IsDeku()) {
        *should = false;
    }
}

static void RegisterForm(void) {
    SOHFormDefinition deku = { 0 };

    sAnimOverrides[0].anim = sIdleAnim;

    deku.structSize = sizeof(deku);
    deku.key = DEKU_FORM_KEY;
    deku.label = "Deku";
    deku.kind = SOH_FORM_KIND_LINK;
    deku.item = DEKU_MASK_KEY;
    deku.modelPath = DEKU_MODEL_PATH;
    deku.rootScaleAdult = DEKU_ROOT_SCALE_ADULT;
    deku.rootScaleChild = DEKU_ROOT_SCALE_CHILD;
    deku.height = DEKU_HEIGHT;
    // FormRegistry applies this after OoT has already resolved its stick speed.
    // MM's age-property 0.3 is a skeleton/root-motion value, not this multiplier.
    deku.motionScale = 1.0f;
    deku.resolveEquipment = ResolveDekuEquipment;
    deku.allowsButtonItem = AllowsDekuItem;
    deku.transformAnim = sMaskOnAnim;
    deku.transformOffAnim = sMaskOffAnim;
    deku.transformMask = sMaskDL;
    deku.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    // MM D_801C0E40[PLAYER_FORM_DEKU].
    deku.transformGlowOffset = (Vec3f){ -570.0f, -812.0f, 0.0f };
    deku.actions = sActions;
    deku.actionCount = ARRAY_COUNT(sActions);
    deku.anims = sAnimOverrides;
    deku.animCount = ARRAY_COUNT(sAnimOverrides);
    deku.onEnter = EnterDeku;
    deku.onExit = ExitDeku;
    deku.update = UpdateDeku;
    sApi->RegisterForm(&deku);
}

static void DrawSaveEditorTab(SaveContext* save) {
    (void)save;
    if (!SOH_MOD_API_HAS(sApi, IsCustomItemOwned) || !SOH_MOD_API_HAS(sApi, SetCustomItemOwned) ||
        !ModUI_BeginTab("Deku")) {
        return;
    }
    bool owned = sApi->IsCustomItemOwned(DEKU_MASK_KEY);
    if (ModUI_Checkbox("Deku Mask", &owned)) {
        sApi->SetCustomItemOwned(DEKU_MASK_KEY, owned);
    }
    ModUI_EndTab();
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    LoadAnims();
    RegisterMask();
    RegisterForm();
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveDekuBody);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawOpenFlower);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawPipes);
    SOH_REGISTER_HOOK(sApi, OnOcarinaNote, NoteStruck);
    SOH_REGISTER_HOOK(sApi, OnOcarinaPlaybackNote, PlaybackNoteStruck);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawDekuEffects);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_PLAYER, SpeakWithDekuVoice);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    SOH_REGISTER_HOOK(sApi, OnSaveEditorTabs, DrawSaveEditorTab);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, ResolveDekuStrength);
    sApi->RegisterVB(VB_PLAYER_ADJUST_LEGS_TO_FLOOR, BlockLegPlanting);
}

