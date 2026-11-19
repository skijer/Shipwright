#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"
#include "z64strength.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <stdbool.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define KEATON_MASK_KEY "nei.keaton_form_mask"
#define KEATON_FORM_KEY "nei.keaton"
#define KEATON_MODEL_PATH "objects/forms/keaton"
// Measured: this rig's body hangs 1131 below the root where Link's hangs 2336, so the root position must be scaled.
// Child animations carry the root to ~2376 instead of 3377, so the same 1131 needs another scale.
#define KEATON_ROOT_SCALE_ADULT 0.335f
#define KEATON_ROOT_SCALE_CHILD 0.476f
#define KEATON_ROOT_DROP 350.0f
#define KEATON_GUARD_REWARD 15
#define KEATON_MASK_PAGE 1
#define KEATON_MASK_SLOT 6
#define OCARINA_INSTRUMENT_OFF 0
#define OCARINA_INSTRUMENT_FLUTE 6

#define TAIL_SEGMENTS 9
#define TAIL_MATRIX_SEGMENT 0x0B
#define TAIL_CLIP_AMOUNT 0.39f

// Rows of D_80854488 (z_player.c), indexed by sword: the DMG_SLASH_* names in z64collision_check.h are shifted one
// step.
#define SLASH_KOKIRI_SWORD 0x00000200
#define SLASH_MASTER_SWORD 0x00000100

#define FIST_REACH 1200.0f
#define PUNCH_GHOSTS 4
#define LINK_VOICE_ACTIONS 0x20
#define MM_DEKU_VOICE_OFFSET 0x80
// mm_assets service: mods do not link against each other, so its name and signature are copied here.
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"
typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);
#define PUNCH_GHOST_FADE (255 / 3)
#define CHAIN_OPENS 0.55f
#define COMBO_STEPS 3

#define CHARGE_LOOP_START 13.0f
#define CHARGE_LOOP_END 17.0f
#define CHARGE_LEVEL1_FRAMES 20
#define CHARGE_LEVEL2_FRAMES 60
#define CHARGE_MAGIC_LEVEL1 8
#define CHARGE_MAGIC_LEVEL2 16

// Rows of En_Light's D_80A9E840, selected with params & 0xF.
#define FLAME_BLUE 2
#define FLAME_ORANGE 0
#define BALL_SCALE_LEVEL1 (0.0075f * 0.16f)
#define BALL_SCALE_LEVEL2 (0.0075f * 0.33f)
#define BALL_RADIUS_LEVEL1 34
#define BALL_RADIUS_LEVEL2 60

#define SHOT_SPEED 4.5f
#define SHOT_RANGE_LEVEL1 600.0f
#define SHOT_RANGE_LEVEL2 1000.0f
#define PULL_RADIUS_LEVEL1 280.0f
#define PULL_RADIUS_LEVEL2 400.0f
#define PULL_STEP 7.0f

#define LONG_JUMP_SPEED 16.0f
#define LONG_JUMP_LIFT 8.0f
// OoT changes animation as soon as Link leaves the ground, so ours must enter after that change (two-frame wait).
#define LONG_JUMP_ANIM_DELAY 2

#define AIR_KICK_SPEED 14.0f
#define RECOIL_SPEED -18.0f
#define KICK_BLOCK_DAMAGE 8
#define KICK_BLOCK_INVINCIBILITY 20

#define REFLECTOR_HEIGHT 20.0f
#define REFLECT_RADIUS 90.0f
#define BURST_RADIUS 70
#define BURST_HEIGHT 90
#define BURST_DAMAGE 8
#define POP_STEP 0.28f
#define PULSE_MIN 0.95f
#define PULSE_PERIOD 24
#define AIR_DRAG 0.35f
#define PROJECTILE_SPEED 6.0f
#define REFLECTED_MAX 8
#define REFLECTED_LIFE 40

// soh does not name the bgCheckFlags bits; z_player.c reads them raw as 1 and 8.
#define BG_ON_GROUND 1
#define BG_TOUCHING_WALL 8

#define CLIMB_RATE 2.0f
#define CLIMB_DRAIN_INTERVAL 10
#define CLIMB_MAGIC_COST 1

#define ANIM_SS_DIR "__OTR__misc/link_animetion/gPlayerAnim_mhr_ss_"
#define ANIM_FIELD_DIR "__OTR__misc/link_animetion/gPlayerAnim_mhr_field_"
#define ANIM_MM_DIR_RAW "__OTR__misc/link_animetion/gPlayerAnim_"
#define ANIM_MM_DIR ANIM_MM_DIR_RAW "pg_"
#define MM_ANIM_VALUES_PER_FRAME 67
#define MM_ANIM_BASE_TRANSL_X (-57)

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_static/gItemIconMaskKeatonTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_static/gKeatonMaskItemNameENGTex";
static const ALIGN_ASSET(2) char sGetItemMaskDL[] = "__OTR__objects/object_gi_ki_tan_mask/gGiKeatonMaskDL";
static const ALIGN_ASSET(2) char sGetItemMaskEyesDL[] = "__OTR__objects/object_gi_ki_tan_mask/gGiKeatonMaskEyesDL";
static const ALIGN_ASSET(2) char sMaskDL[] = "__OTR__objects/object_link_child/gLinkChildKeatonMaskDL";
static const ALIGN_ASSET(2) char sTailSkelPath[] = "__OTR__objects/forms/keaton/object_link_boy/gLinkAdultTails";

static const char* const sComboPaths[COMBO_STEPS] = {
    ANIM_MM_DIR "punchA_Data",
    ANIM_MM_DIR "punchB_Data",
    ANIM_SS_DIR "dash_attack09",
};
static const f32 sComboHitStart[COMBO_STEPS] = { 4.0f, 6.0f, 14.0f };
static const f32 sComboHitEnd[COMBO_STEPS] = { 9.0f, 13.0f, 26.0f };

static const ALIGN_ASSET(2) char sMaskOnPath[] = ANIM_MM_DIR_RAW "cl_setmask_Data";
static const ALIGN_ASSET(2) char sChargePath[] = ANIM_SS_DIR "attack13";
static const ALIGN_ASSET(2) char sLongJumpPath[] = ANIM_FIELD_DIR "charge_attack01";
static const ALIGN_ASSET(2) char sAirKickPath[] = ANIM_FIELD_DIR "wirebug_dash02";
static const ALIGN_ASSET(2) char sReflectPath[] = "__OTR__misc/link_animetion/gPlayerAnim_mhr_damage_idle03_loop";
static const ALIGN_ASSET(2) char sPunchEffectDL[] = "__OTR__objects/object_link_goron/gLinkGoronGoronPunchEffectDL";

#include "keaton_tail_anim.inc.c"

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate", "OnPlayerPostLimbDraw", "OnPlayerActionHandler",     "OnActorUpdate",          "OnSceneInit",
    "OnActorPlaySfx", "OnPlayerFilterInput",  "OnBgCheckResolveWallFlags", "OnPlayerResolveLimbDraw"
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

typedef enum {
    KEATON_IDLE,
    KEATON_COMBO,
    KEATON_CHARGE,
    KEATON_THROW,
    KEATON_LONG_JUMP,
    KEATON_AIR_KICK,
    KEATON_RECOIL,
} KeatonState;

static const SOHModApi* sApi;

#define MM_ANIM_SLOTS 3
static LinkAnimationHeader sMmAnims[MM_ANIM_SLOTS];
static s16* sMmFrames[MM_ANIM_SLOTS];
static LinkAnimationHeader* sMaskOn;
static LinkAnimationHeader* sCombo[COMBO_STEPS];
static LinkAnimationHeader* sCharge;
static LinkAnimationHeader* sLongJump;
static LinkAnimationHeader* sAirKick;
static bool sAnimsLoaded;

// MM keeps the punch matrix in a ring of 4 and redraws it with falling alpha; the ramp is its own for pg_punchA.
typedef struct {
    MtxF matrix;
    u8 alpha;
} PunchGhost;

static const u8 kPunchAlpha[] = { 100, 200, 255, 255, 255, 200, 100 };
static Gfx* sPunchEffect;
static PunchGhost sPunchGhost[2][PUNCH_GHOSTS];
static s32 sPunchGhostNext[2];
static s32 sPunchFrames;

static u8 sState;
static s32 sStep;
static s16 sChargeTimer;
static u8 sChargeLevel;
static bool sChargeReversing;
static s16 sJumpDelay;
static bool sKickBuffered;
static bool sFistsBlocked;
static s16 sAirKickYaw;
static Vec3f sAirKickStart;
static u32 sAirKickFrames;

static Vec3f sShotPos;
static Vec3f sShotStep;
static f32 sShotTravelled;
static f32 sShotRange;
static f32 sShotPullRadius;
static u8 sShotLevel;
static bool sShotActive;
static Actor* sFlame;
static u8 sFlameCategory;
static ColliderCylinder sShotCylinder;
static bool sShotCylinderReady;

static s32 sFistTrail[2] = { -1, -1 };
static bool sClimbLent;
static bool sReadingNaturalWall;
static s16 sClimbDrain;

typedef enum {
    REFLECTOR_OFF,
    REFLECTOR_POPPING,
    REFLECTOR_HELD,
} ReflectorState;

typedef struct {
    Actor* actor;
    s16 timer;
} ReflectedProjectile;

static LinkAnimationHeader* sReflectAnim;
static u8 sReflectorState;
static f32 sReflectorScale;
static s16 sReflectorPulse;
static ColliderCylinder sGuard;
static ColliderCylinder sBurst;
static bool sGuardReady;
static bool sBurstReady;
static ReflectedProjectile sReflected[REFLECTED_MAX];

static Vec3s sTail[TAIL_SEGMENTS];
static u32 sTailFrame;
static u8 sTailPose;
static SkeletonHeader* sTailSkel;
static u8 sTriedTailSkel;

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);
s32 func_80842DF4(PlayState* play, Player* player);
void func_80837918(Player* player, s32 quadIndex, u32 dmgFlags);
void func_80837C0C(PlayState* play, Player* player, s32 damageResponseType, f32 speed, f32 yVelocity, s16 yRot,
                   s32 invincibilityTimer);
void func_80090A28(Player* player, Vec3f* vectors);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
void Player_FinishAnimMovement(Player* player);
void func_80838940(Player* player, LinkAnimationHeader* anim, f32 lift, PlayState* play, u16 sfxId);
s32 Player_ActionHandler_10(Player* player, PlayState* play);
int Player_IsZTargeting(Player* player);
void Player_RequestRumble(Player* player, s32 sourceStrength, s32 duration, s32 decreaseRate, s32 distSq);
GetItemEntry ItemTable_RetrieveEntry(s16 modIndex, s16 getItemID);
HOST_DATA extern Vec3f D_80126080;
HOST_DATA extern Vec3f D_801260A4[3];

static ColliderCylinderInit sShotCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { SLASH_MASTER_SWORD, 0x00, 0x02 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { BALL_RADIUS_LEVEL1, 46, -24, { 0, 0, 0 } },
};

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySfxGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// An MM player animation is the same raw format as OoT's but its root lives elsewhere: OoT's base translation is forced
// every frame, on a copy because the resource is shared by the whole game.
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
    if (sAnimsLoaded) {
        return;
    }
    sAnimsLoaded = true;
    sCombo[0] = LoadMmAnim(sComboPaths[0], 0);
    sCombo[1] = LoadMmAnim(sComboPaths[1], 1);
    sMaskOn = LoadMmAnim(sMaskOnPath, 2);
    sCombo[2] = ResourceMgr_LoadPlayerAnimAsHeader(sComboPaths[2]);
    sCharge = ResourceMgr_LoadPlayerAnimAsHeader(sChargePath);
    sLongJump = ResourceMgr_LoadPlayerAnimAsHeader(sLongJumpPath);
    sAirKick = ResourceMgr_LoadPlayerAnimAsHeader(sAirKickPath);
    sReflectAnim = ResourceMgr_LoadPlayerAnimAsHeader(sReflectPath);
    sPunchEffect = ResourceMgr_LoadGfxByName(sPunchEffectDL);
}

// Actor_Kill only nulls update(); the actor is freed at the end of the frame, so a stored pointer dangles.
static Actor* ResolveFlame(PlayState* play) {
    if (sFlame == NULL) {
        return NULL;
    }
    for (Actor* it = play->actorCtx.actorLists[sFlameCategory].head; it != NULL; it = it->next) {
        if (it == sFlame) {
            return it->update != NULL ? it : NULL;
        }
    }
    sFlame = NULL;
    return NULL;
}

static void KillFlame(PlayState* play) {
    Actor* flame = ResolveFlame(play);

    if (flame != NULL) {
        Actor_Kill(flame);
    }
    sFlame = NULL;
}

static f32 BallScale(u8 level) {
    return level >= 2 ? BALL_SCALE_LEVEL2 : BALL_SCALE_LEVEL1;
}

// The colour is baked into En_Light's params at spawn, so upgrading to orange replaces the actor.
static void SpawnFlame(PlayState* play, Vec3f* at, u8 level) {
    KillFlame(play);
    Actor* flame = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, at->x, at->y, at->z, 0, 0, 0x4000,
                               level >= 2 ? FLAME_ORANGE : FLAME_BLUE);
    sFlame = flame;
    if (flame != NULL) {
        sFlameCategory = flame->category;
    }
}

static void PlaceFlame(PlayState* play, Vec3f* at, u8 level) {
    Actor* flame = ResolveFlame(play);

    if (flame == NULL) {
        return;
    }
    flame->world.pos = *at;
    Actor_SetScale(flame, BallScale(level));
}

static void PullEnemies(PlayState* play) {
    for (Actor* it = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; it != NULL; it = it->next) {
        if (it->update == NULL) {
            continue;
        }
        Vec3f toBall;
        f32 distance = Math_Vec3f_DistXYZAndStoreDiff(&it->world.pos, &sShotPos, &toBall);
        if (distance > sShotPullRadius || distance < 1.0f) {
            continue;
        }
        f32 step = PULL_STEP / distance;
        it->world.pos.x += toBall.x * step;
        it->world.pos.y += toBall.y * step;
        it->world.pos.z += toBall.z * step;
    }
}

static void SubmitShotCollider(PlayState* play, Player* player) {
    if (!sShotCylinderReady) {
        Collider_InitCylinder(play, &sShotCylinder);
        Collider_SetCylinder(play, &sShotCylinder, &player->actor, &sShotCylinderInit);
        sShotCylinderReady = true;
    }
    sShotCylinder.dim.radius = sShotLevel >= 2 ? BALL_RADIUS_LEVEL2 : BALL_RADIUS_LEVEL1;
    sShotCylinder.base.atFlags |= AT_ON;
    sShotCylinder.dim.pos.x = (s16)sShotPos.x;
    sShotCylinder.dim.pos.y = (s16)sShotPos.y;
    sShotCylinder.dim.pos.z = (s16)sShotPos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sShotCylinder.base);
}

static void AimShot(Player* player) {
    Actor* target = player->focusActor;
    bool lockedOnFoe = target != NULL && target->update != NULL &&
                       (target->category == ACTORCAT_ENEMY || target->category == ACTORCAT_BOSS);

    if (lockedOnFoe) {
        Vec3f toFoe;
        f32 distance = Math_Vec3f_DistXYZAndStoreDiff(&sShotPos, &target->focus.pos, &toFoe);
        if (distance > 1.0f) {
            f32 scale = SHOT_SPEED / distance;
            sShotStep.x = toFoe.x * scale;
            sShotStep.y = toFoe.y * scale;
            sShotStep.z = toFoe.z * scale;
            return;
        }
    }

    s16 yaw = player->actor.shape.rot.y;
    sShotStep.x = Math_SinS(yaw) * SHOT_SPEED;
    sShotStep.y = 0.0f;
    sShotStep.z = Math_CosS(yaw) * SHOT_SPEED;
}

static void LaunchShot(PlayState* play, Player* player, u8 level) {
    sShotPos = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    AimShot(player);
    sShotTravelled = 0.0f;
    sShotRange = level >= 2 ? SHOT_RANGE_LEVEL2 : SHOT_RANGE_LEVEL1;
    sShotPullRadius = level >= 2 ? PULL_RADIUS_LEVEL2 : PULL_RADIUS_LEVEL1;
    sShotLevel = level;
    sShotActive = true;

    Player_PlayVoiceSfx(player, NA_SE_VO_LI_MAGIC_ATTACK);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_MAGIC_ARROW_SHOT);
}

static void UpdateShot(PlayState* play, Player* player) {
    if (!sShotActive) {
        return;
    }

    sShotPos.x += sShotStep.x;
    sShotPos.y += sShotStep.y;
    sShotPos.z += sShotStep.z;
    sShotTravelled += SHOT_SPEED;

    PlaceFlame(play, &sShotPos, sShotLevel);
    PullEnemies(play);
    SubmitShotCollider(play, player);
    PlaySfxAt(NA_SE_EV_FIRE_PILLAR, &sShotPos);

    if (sShotTravelled >= sShotRange || (sShotCylinder.base.atFlags & AT_HIT)) {
        sShotCylinder.base.atFlags &= ~(AT_ON | AT_HIT);
        sShotActive = false;
        KillFlame(play);
    }
}

static void LendClimbEverything(bool shouldLend) {
    sClimbLent = shouldLend;
}

static void AllowKeatonWall(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId, uint32_t* flags) {
    if (!sClimbLent || sReadingNaturalWall || gPlayState == NULL || !sApi->IsFormActive(KEATON_FORM_KEY)) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (colCtx == &gPlayState->colCtx && poly == player->actor.wallPoly && bgId == player->actor.wallBgId) {
        *flags |= 8;
    }
}

// Wall contact only decides whether climbing can start: asking again while climbing drops him, because
// BG_TOUCHING_WALL disappears between handholds.
static void UpdateClimb(PlayState* play, Player* player) {
    bool climbing = (player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) != 0;
    bool holdingA = CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_A);

    if (climbing) {
        if (player->skelAnime.playSpeed > 0.0f) {
            player->skelAnime.playSpeed = CLIMB_RATE;
        } else if (player->skelAnime.playSpeed < 0.0f) {
            player->skelAnime.playSpeed = -CLIMB_RATE;
        }
    }

    bool canHold = climbing || ((player->actor.bgCheckFlags & BG_TOUCHING_WALL) && player->actor.wallPoly != NULL);
    if (!holdingA || !canHold) {
        LendClimbEverything(false);
        sClimbDrain = 0;
        return;
    }

    if (player->actor.wallPoly != NULL) {
        sReadingNaturalWall = true;
        const bool freeSurface =
            (SurfaceType_GetWallFlags(&play->colCtx, player->actor.wallPoly, player->actor.wallBgId) & 8) != 0;
        sReadingNaturalWall = false;
        if (freeSurface) {
            LendClimbEverything(false);
            sClimbDrain = 0;
            return;
        }
    }
    if (!gSaveContext.isMagicAcquired || gSaveContext.magic < CLIMB_MAGIC_COST) {
        LendClimbEverything(false);
        sClimbDrain = 0;
        return;
    }

    sClimbDrain++;
    if (sClimbDrain >= CLIMB_DRAIN_INTERVAL) {
        sClimbDrain = 0;
        if (!Magic_RequestChange(play, CLIMB_MAGIC_COST, MAGIC_CONSUME_NOW)) {
            LendClimbEverything(false);
            return;
        }
        Magic_Reset(play);
    }
    LendClimbEverything(true);
}

// Without G_LIGHTING the shade is the vertex colour, and G_CC_SHADE passes it through untextured.
static Vtx sHexVtx[24] = {
    VTX(22, 0, 0, 0, 0, 74, 148, 240, 255),     VTX(11, 19, 0, 0, 0, 74, 148, 240, 255),
    VTX(-11, 19, 0, 0, 0, 74, 148, 240, 255),   VTX(-22, 0, 0, 0, 0, 74, 148, 240, 255),
    VTX(-11, -19, 0, 0, 0, 74, 148, 240, 255),  VTX(11, -19, 0, 0, 0, 74, 148, 240, 255),

    VTX(20, 0, 0, 0, 0, 168, 218, 251, 255),    VTX(10, 17, 0, 0, 0, 168, 218, 251, 255),
    VTX(-10, 17, 0, 0, 0, 168, 218, 251, 255),  VTX(-20, 0, 0, 0, 0, 168, 218, 251, 255),
    VTX(-10, -17, 0, 0, 0, 168, 218, 251, 255), VTX(10, -17, 0, 0, 0, 168, 218, 251, 255),

    VTX(14, 0, 0, 0, 0, 150, 206, 250, 255),    VTX(7, 12, 0, 0, 0, 150, 206, 250, 255),
    VTX(-7, 12, 0, 0, 0, 150, 206, 250, 255),   VTX(-14, 0, 0, 0, 0, 150, 206, 250, 255),
    VTX(-7, -12, 0, 0, 0, 150, 206, 250, 255),  VTX(7, -12, 0, 0, 0, 150, 206, 250, 255),

    VTX(11, 0, 0, 0, 0, 120, 190, 248, 0),      VTX(6, 10, 0, 0, 0, 120, 190, 248, 0),
    VTX(-6, 10, 0, 0, 0, 120, 190, 248, 0),     VTX(-11, 0, 0, 0, 0, 120, 190, 248, 0),
    VTX(-6, -10, 0, 0, 0, 120, 190, 248, 0),    VTX(6, -10, 0, 0, 0, 120, 190, 248, 0),
};

static Gfx sHexDL[] = {
    gsDPPipeSync(),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPClearGeometryMode(G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    gsSPSetGeometryMode(G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH),
    gsSPVertex(sHexVtx, 24, 0),

    gsSP2Triangles(0, 6, 7, 0, 0, 7, 1, 0),
    gsSP2Triangles(1, 7, 8, 0, 1, 8, 2, 0),
    gsSP2Triangles(2, 8, 9, 0, 2, 9, 3, 0),
    gsSP2Triangles(3, 9, 10, 0, 3, 10, 4, 0),
    gsSP2Triangles(4, 10, 11, 0, 4, 11, 5, 0),
    gsSP2Triangles(5, 11, 6, 0, 5, 6, 0, 0),

    gsSP2Triangles(6, 12, 13, 0, 6, 13, 7, 0),
    gsSP2Triangles(7, 13, 14, 0, 7, 14, 8, 0),
    gsSP2Triangles(8, 14, 15, 0, 8, 15, 9, 0),
    gsSP2Triangles(9, 15, 16, 0, 9, 16, 10, 0),
    gsSP2Triangles(10, 16, 17, 0, 10, 17, 11, 0),
    gsSP2Triangles(11, 17, 12, 0, 11, 12, 6, 0),

    gsSP2Triangles(12, 18, 19, 0, 12, 19, 13, 0),
    gsSP2Triangles(13, 19, 20, 0, 13, 20, 14, 0),
    gsSP2Triangles(14, 20, 21, 0, 14, 21, 15, 0),
    gsSP2Triangles(15, 21, 22, 0, 15, 22, 16, 0),
    gsSP2Triangles(16, 22, 23, 0, 16, 23, 17, 0),
    gsSP2Triangles(17, 23, 18, 0, 17, 18, 12, 0),

    gsSPEndDisplayList(),
};

static ColliderCylinderInit sGuardInit = {
    { COLTYPE_METAL, AT_NONE, AC_ON | AC_HARD | AC_TYPE_ENEMY, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_NONE },
    { BURST_RADIUS, BURST_HEIGHT, -40, { 0, 0, 0 } },
};

static ColliderCylinderInit sBurstInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_MAGIC_LIGHT, 0x00, 0x02 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { BURST_RADIUS, BURST_HEIGHT, -40, { 0, 0, 0 } },
};

static void PunishAttacker(PlayState* play, Player* player, Actor* attacker) {
    if (attacker == NULL || attacker->update == NULL || !sBurstReady) {
        return;
    }
    sBurst.info.toucher.dmgFlags = DMG_DEKU_NUT | DMG_MAGIC_LIGHT;
    sBurst.info.toucher.damage = BURST_DAMAGE;
    sBurst.base.atFlags |= AT_ON;
    sBurst.dim.pos.x = (s16)attacker->focus.pos.x;
    sBurst.dim.pos.y = (s16)attacker->focus.pos.y;
    sBurst.dim.pos.z = (s16)attacker->focus.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sBurst.base);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SHIELD_REFLECT_MG2);
}

// A Beamos fires no projectile: its laser is two stretched quads, so the reflector has to be hit; base.ac says by whom.
static void SubmitGuard(PlayState* play, Player* player) {
    if (!sGuardReady) {
        Collider_InitCylinder(play, &sGuard);
        Collider_SetCylinder(play, &sGuard, &player->actor, &sGuardInit);
        sGuardReady = true;
    }
    if (sGuard.base.acFlags & AC_HIT) {
        sGuard.base.acFlags &= ~AC_HIT;
        PunishAttacker(play, player, sGuard.base.ac);
        sGuard.base.ac = NULL;
    }

    // Link's cylinder sits inside the guard, so an incoming ray hits both; dropping its AC that frame makes the guard
    // block.
    player->cylinder.base.acFlags &= ~(AC_ON | AC_HIT);

    sGuard.base.acFlags |= AC_ON;
    sGuard.dim.pos.x = (s16)player->actor.world.pos.x;
    sGuard.dim.pos.y = (s16)player->actor.world.pos.y;
    sGuard.dim.pos.z = (s16)player->actor.world.pos.z;
    CollisionCheck_SetAC(play, &play->colChkCtx, &sGuard.base);
}

static void SubmitBurst(PlayState* play, Player* player) {
    if (!sBurstReady) {
        Collider_InitCylinder(play, &sBurst);
        Collider_SetCylinder(play, &sBurst, &player->actor, &sBurstInit);
        sBurstReady = true;
    }
    sBurst.info.toucher.dmgFlags = DMG_MAGIC_LIGHT;
    sBurst.info.toucher.damage = BURST_DAMAGE;
    sBurst.base.atFlags |= AT_ON;
    sBurst.dim.pos.x = (s16)player->actor.world.pos.x;
    sBurst.dim.pos.y = (s16)player->actor.world.pos.y;
    sBurst.dim.pos.z = (s16)player->actor.world.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sBurst.base);
}

static void RememberReflected(Actor* actor) {
    for (s32 i = 0; i < REFLECTED_MAX; i++) {
        if (sReflected[i].actor == actor) {
            sReflected[i].timer = REFLECTED_LIFE;
            return;
        }
    }
    for (s32 i = 0; i < REFLECTED_MAX; i++) {
        if (sReflected[i].timer <= 0) {
            sReflected[i].actor = actor;
            sReflected[i].timer = REFLECTED_LIFE;
            return;
        }
    }
}

static void TickReflected(void) {
    for (s32 i = 0; i < REFLECTED_MAX; i++) {
        if (sReflected[i].timer > 0) {
            sReflected[i].timer--;
            if (sReflected[i].timer == 0) {
                sReflected[i].actor = NULL;
            }
        }
    }
}

// A projectile's collider lives in its private struct, but colChkCtx.colAT is the generic way to reach it; this hook
// runs between the projectile registering and the next CollisionCheck_AT.
static void RealignReflectedProjectile(void* actorRef) {
    Actor* actor = (Actor*)actorRef;
    PlayState* play = gPlayState;

    if (actor == NULL || play == NULL) {
        return;
    }
    bool known = false;
    for (s32 i = 0; i < REFLECTED_MAX; i++) {
        if (sReflected[i].timer > 0 && sReflected[i].actor == actor) {
            known = true;
            break;
        }
    }
    if (!known) {
        return;
    }
    for (s32 i = 0; i < play->colChkCtx.colATCount; i++) {
        Collider* collider = play->colChkCtx.colAT[i];

        if (collider != NULL && collider->actor == actor) {
            collider->atFlags &= ~AT_TYPE_ENEMY;
            collider->atFlags |= AT_TYPE_PLAYER;
        }
    }
}

static void ReflectOne(Actor* actor) {
    actor->world.rot.y += 0x8000;
    actor->shape.rot.y = actor->world.rot.y;
    actor->world.rot.x = -actor->world.rot.x;
    actor->velocity.x = -actor->velocity.x;
    actor->velocity.z = -actor->velocity.z;

    if (actor->parent != NULL) {
        actor->world.rot.y = Actor_WorldYawTowardActor(actor, actor->parent);
        actor->shape.rot.y = actor->world.rot.y;
    }
}

// Projectiles span four categories: arrows and magic are ITEMACTION, bombs EXPLOSIVE, nuts PROP, an Octorok rock ENEMY.
// The last two lists hold things that must not be flipped, so those must be flying fast.
typedef struct {
    s32 category;
    bool flyingOnly;
} SweepRule;

static const SweepRule sSweep[] = {
    { ACTORCAT_ITEMACTION, false },
    { ACTORCAT_EXPLOSIVE, false },
    { ACTORCAT_PROP, true },
    { ACTORCAT_ENEMY, true },
};

static void ReflectSweep(PlayState* play, Player* player) {
    for (s32 rule = 0; rule < ARRAY_COUNT(sSweep); rule++) {
        for (Actor* it = play->actorCtx.actorLists[sSweep[rule].category].head; it != NULL; it = it->next) {
            if (it->update == NULL || it == &player->actor) {
                continue;
            }
            if (sSweep[rule].flyingOnly) {
                if ((it->bgCheckFlags & BG_ON_GROUND) || it->speedXZ < PROJECTILE_SPEED) {
                    continue;
                }
            } else if (it->speedXZ <= 0.0f) {
                continue;
            }

            Vec3f toPlayer;
            f32 distance = Math_Vec3f_DistXYZAndStoreDiff(&it->world.pos, &player->actor.world.pos, &toPlayer);
            if (distance > REFLECT_RADIUS || distance < 0.1f) {
                continue;
            }

            // Only what approaches is flipped, which also stops a returned shot from flipping every frame inside the
            // radius.
            f32 approach =
                (it->velocity.x * toPlayer.x) + (it->velocity.y * toPlayer.y) + (it->velocity.z * toPlayer.z);
            if (approach <= 0.0f) {
                continue;
            }
            ReflectOne(it);
            RememberReflected(it);
            Audio_PlayActorSound2(&player->actor, NA_SE_IT_SHIELD_REFLECT_MG);
        }
    }
}

static void OpenReflector(PlayState* play, Player* player) {
    sReflectorState = REFLECTOR_POPPING;
    sReflectorScale = 0.0f;
    sReflectorPulse = 0;

    if (sReflectAnim != NULL) {
        LinkAnimation_Change(play, &player->skelAnime, sReflectAnim, 1.0f, 0.0f, Animation_GetLastFrame(sReflectAnim),
                             ANIMMODE_LOOP, -6.0f);
    }
    if (!(player->actor.bgCheckFlags & BG_ON_GROUND)) {
        player->linearVelocity *= AIR_DRAG;
        player->actor.velocity.y *= AIR_DRAG;
    }
    SubmitBurst(play, player);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SHIELD_REFLECT_MG);
}

static void CloseReflector(Player* player) {
    sReflectorState = REFLECTOR_OFF;
    sReflectorScale = 0.0f;
    if (sBurstReady) {
        sBurst.base.atFlags &= ~(AT_ON | AT_HIT);
    }
    if (sGuardReady) {
        sGuard.base.acFlags &= ~(AC_ON | AC_HIT);
        sGuard.base.ac = NULL;
    }
}

static void ReflectorAction(Player* player, PlayState* play) {
    TickReflected();

    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R) || (player->stateFlags1 & PLAYER_STATE1_DAMAGED)) {
        CloseReflector(player);
        func_80839FFC(player, play);
        return;
    }

    if (sReflectorState == REFLECTOR_POPPING) {
        sReflectorScale += POP_STEP;
        if (sReflectorScale >= 1.0f) {
            sReflectorScale = 1.0f;
            sReflectorState = REFLECTOR_HELD;
        }
    } else {
        sReflectorPulse = (sReflectorPulse + 1) % PULSE_PERIOD;
        sReflectorScale =
            PULSE_MIN + ((1.0f - PULSE_MIN) * (0.5f + (0.5f * Math_CosS(sReflectorPulse * (0x10000 / PULSE_PERIOD)))));
    }

    SubmitGuard(play, player);
    ReflectSweep(play, player);
    LinkAnimation_Update(play, &player->skelAnime);

    // damage_idle03 walks its root and jointTable[0] is the translation the draw uses: pin both to the skeleton base.
    if (player->skelAnime.jointTable != NULL) {
        player->skelAnime.jointTable[0] = player->skelAnime.baseTransl;
        player->skelAnime.prevTransl = player->skelAnime.baseTransl;
    }
    player->linearVelocity = 0.0f;
}

static void DrawReflector(PlayState* play, Player* player) {
    if (sReflectorState == REFLECTOR_OFF) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    // Built from the world position, not the current limb matrix: limb space is in hundreds of units.
    Matrix_Push();
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + REFLECTOR_HEIGHT, player->actor.world.pos.z,
                     MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(sReflectorScale, sReflectorScale, sReflectorScale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, sHexDL);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

static void ClearState(void) {
    sState = KEATON_IDLE;
    sStep = 0;
    sChargeTimer = 0;
    sChargeLevel = 0;
    sChargeReversing = false;
    sJumpDelay = 0;
    sKickBuffered = false;
    sFistsBlocked = false;
}

static void PlayMove(PlayState* play, Player* player, LinkAnimationHeader* anim, f32 start, f32 end) {
    f32 speed = end >= start ? 1.0f : -1.0f;

    LinkAnimation_Change(play, &player->skelAnime, anim, speed, start, end, ANIMMODE_ONCE, -6.0f);
}

static void StartFistTrails(PlayState* play) {
    EffectBlureInit2 blure = {
        0, 8, 0, { 255, 0, 0, 255 }, { 255, 0, 0, 64 }, { 255, 128, 0, 0 }, { 255, 128, 0, 0 }, 4,
        0, 2, 0, { 255, 0, 0, 255 }, { 255, 0, 0, 64 }, TRAIL_TYPE_SWORDS,
    };

    for (s32 hand = 0; hand < 2; hand++) {
        if (sFistTrail[hand] >= 0) {
            continue;
        }
        Effect_Add(play, &sFistTrail[hand], EFFECT_BLURE2, 0, 0, &blure);
    }
}

static void StopFistTrails(PlayState* play) {
    for (s32 hand = 0; hand < 2; hand++) {
        if (sFistTrail[hand] < 0) {
            continue;
        }
        Effect_Delete(play, sFistTrail[hand]);
        sFistTrail[hand] = -1;
    }
}

static void ArmFists(PlayState* play, Player* player) {
    func_80837918(player, 0, SLASH_KOKIRI_SWORD);
    func_80837918(player, 1, SLASH_KOKIRI_SWORD);
    StartFistTrails(play);
}

static void EndMove(PlayState* play, Player* player) {
    player->meleeWeaponState = 0;
    StopFistTrails(play);
    ClearState();
    func_80839FFC(player, play);
}

static void EnterCharge(PlayState* play, Player* player) {
    sState = KEATON_CHARGE;
    sChargeTimer = 0;
    sChargeLevel = 0;
    sChargeReversing = false;
    player->meleeWeaponState = 0;
    PlayMove(play, player, sCharge, 0.0f, CHARGE_LOOP_END);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_CHARGE);
}

// LinkAnimation has ONCE and LOOP but no ping-pong, so each leg is relaunched with the sign flipped.
static void StepChargeLoop(PlayState* play, Player* player) {
    sChargeReversing = !sChargeReversing;
    if (sChargeReversing) {
        PlayMove(play, player, sCharge, CHARGE_LOOP_END, CHARGE_LOOP_START);
    } else {
        PlayMove(play, player, sCharge, CHARGE_LOOP_START, CHARGE_LOOP_END);
    }
}

static void UpdateCombo(PlayState* play, Player* player, Input* input) {
    f32 last = Animation_GetLastFrame(sCombo[sStep]);
    f32 frame = player->skelAnime.curFrame;

    if (sStep + 1 < COMBO_STEPS && frame >= last * CHAIN_OPENS && CHECK_BTN_ALL(input->press.button, BTN_B)) {
        sStep++;
        PlayMove(play, player, sCombo[sStep], 0.0f, Animation_GetLastFrame(sCombo[sStep]));
        return;
    }
    player->meleeWeaponState = (frame >= sComboHitStart[sStep] && frame <= sComboHitEnd[sStep]) ? 1 : 0;

    if (LinkAnimation_Update(play, &player->skelAnime)) {
        player->meleeWeaponState = 0;
        if (CHECK_BTN_ALL(input->cur.button, BTN_B) && sCharge != NULL) {
            EnterCharge(play, player);
            return;
        }
        EndMove(play, player);
    }
}

static void UpdateCharge(PlayState* play, Player* player, Input* input) {
    sChargeTimer++;
    u8 level = sChargeTimer >= CHARGE_LEVEL2_FRAMES ? 2 : (sChargeTimer >= CHARGE_LEVEL1_FRAMES ? 1 : 0);

    if (level != sChargeLevel) {
        sChargeLevel = level;
        SpawnFlame(play, &player->bodyPartsPos[PLAYER_BODYPART_R_HAND], level);
        Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_CHARGE);
    }
    PlaceFlame(play, &player->bodyPartsPos[PLAYER_BODYPART_R_HAND], level);

    if (!CHECK_BTN_ALL(input->cur.button, BTN_B)) {
        s16 cost = level >= 2 ? CHARGE_MAGIC_LEVEL2 : CHARGE_MAGIC_LEVEL1;
        if (level == 0 || !Magic_RequestChange(play, cost, MAGIC_CONSUME_NOW)) {
            KillFlame(play);
            EndMove(play, player);
            return;
        }
        Magic_Reset(play);
        LaunchShot(play, player, level);
        sState = KEATON_THROW;
        PlayMove(play, player, sCharge, CHARGE_LOOP_END, Animation_GetLastFrame(sCharge));
        return;
    }

    if (LinkAnimation_Update(play, &player->skelAnime)) {
        StepChargeLoop(play, player);
    }
}

static void StartRecoil(PlayState* play, Player* player) {
    player->meleeWeaponState = 0;
    player->meleeWeaponQuads[0].base.atFlags &= ~(AT_ON | AT_BOUNCED);
    player->meleeWeaponQuads[1].base.atFlags &= ~(AT_ON | AT_BOUNCED);
    Player_RequestRumble(player, 180, 20, 100, 0);

    if (!(player->actor.bgCheckFlags & BG_ON_GROUND)) {
        ClearState();
        player->actor.colChkInfo.damage = KICK_BLOCK_DAMAGE;
        func_80837C0C(play, player, PLAYER_HIT_RESPONSE_KNOCKBACK_SMALL, 4.0f, 5.0f, player->actor.shape.rot.y + 0x8000,
                      KICK_BLOCK_INVINCIBILITY);
        return;
    }

    sState = KEATON_RECOIL;
    LinkAnimationHeader* rebound = (LinkAnimationHeader*)gPlayerAnim_link_fighter_rebound;
    PlayMove(play, player, rebound, 0.0f, Animation_GetLastFrame(rebound));
    player->linearVelocity = RECOIL_SPEED;
}

static void StartAirKick(PlayState* play, Player* player);

static void TraceAirKick(Player* player) {
    char message[384];
    snprintf(message, sizeof(message),
             "keaton kick: frame=%u delta=(%.2f,%.2f,%.2f) speed=%.2f/%.2f velocity=(%.2f,%.2f,%.2f) "
             "motion=%02X flags=%08X/%08X bg=%04X yaw=%d/%d",
             sAirKickFrames, player->actor.world.pos.x - sAirKickStart.x, player->actor.world.pos.y - sAirKickStart.y,
             player->actor.world.pos.z - sAirKickStart.z, player->linearVelocity, player->actor.speedXZ,
             player->actor.velocity.x, player->actor.velocity.y, player->actor.velocity.z,
             player->skelAnime.movementFlags, player->stateFlags1, player->stateFlags2, player->actor.bgCheckFlags,
             sAirKickYaw, player->actor.world.rot.y);
    sApi->Log(message);
}

static void KeatonAction(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];

    if (sState == KEATON_AIR_KICK) {
        ++sAirKickFrames;
        if (sAirKickFrames == 1 || sAirKickFrames == 4 || sAirKickFrames == 10) {
            TraceAirKick(player);
        }
    }

    if ((sState == KEATON_COMBO || sState == KEATON_AIR_KICK) &&
        (sFistsBlocked || (player->meleeWeaponQuads[0].base.atFlags & AT_BOUNCED) ||
         (player->meleeWeaponQuads[1].base.atFlags & AT_BOUNCED))) {
        sFistsBlocked = false;
        StopFistTrails(play);
        StartRecoil(play, player);
        return;
    }

    if (func_80842DF4(play, player)) {
        if (sState == KEATON_AIR_KICK) {
            sApi->Log("keaton: la patada choca (func_80842DF4)");
        }
        ClearState();
        StopFistTrails(play);
        return;
    }

    if (sState == KEATON_AIR_KICK && player->linearVelocity < 0.0f) {
        EndMove(play, player);
        return;
    }

    switch (sState) {
        case KEATON_LONG_JUMP:
            if ((player->actor.bgCheckFlags & BG_ON_GROUND) && player->actor.velocity.y <= 0.0f) {
                EndMove(play, player);
            } else if ((player->actor.bgCheckFlags & BG_TOUCHING_WALL) && CHECK_BTN_ALL(input->cur.button, BTN_A)) {
                EndMove(play, player);
            } else if (sAirKick != NULL && (sKickBuffered || CHECK_BTN_ALL(input->press.button, BTN_B))) {
                StartAirKick(play, player);
            } else if (LinkAnimation_Update(play, &player->skelAnime)) {
                EndMove(play, player);
            }
            break;

        case KEATON_COMBO:
            player->linearVelocity = 0.0f;
            UpdateCombo(play, player, input);
            break;

        case KEATON_CHARGE:
            player->linearVelocity = 0.0f;
            UpdateCharge(play, player, input);
            break;

        case KEATON_AIR_KICK: {
            // Ground contact only counts when falling: at kick start the player still touches the ground he left.
            const bool landed = (player->actor.bgCheckFlags & BG_ON_GROUND) && player->actor.velocity.y <= 0.0f;

            player->meleeWeaponState = 1;
            // func_80842DF4 leaves the speed negative when a fist hits a wall: the kick ends with vanilla's recoil.
            if (player->linearVelocity < 0.0f) {
                sApi->Log("keaton: la patada rebota en una pared");
                EndMove(play, player);
                break;
            }
            if (landed || LinkAnimation_Update(play, &player->skelAnime)) {
                sApi->Log(landed ? "keaton: la patada termina al aterrizar" : "keaton: la patada termina su clip");
                EndMove(play, player);
            } else {
                player->linearVelocity = AIR_KICK_SPEED;
                player->yaw = sAirKickYaw;
                player->actor.shape.rot.y = sAirKickYaw;
                player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
            }
            break;
        }

        case KEATON_RECOIL:
            if (LinkAnimation_Update(play, &player->skelAnime)) {
                EndMove(play, player);
            }
            break;

        default:
            player->linearVelocity = 0.0f;
            if (LinkAnimation_Update(play, &player->skelAnime)) {
                EndMove(play, player);
            }
            break;
    }
}

static bool IsKeatonBusy(Player* player) {
    return player->actionFunc == KeatonAction || player->actionFunc == ReflectorAction;
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 &
             (PLAYER_STATE1_DEAD | PLAYER_STATE1_LOADING | PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM |
              PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER |
              PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_ITEM_CS |
              PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED | PLAYER_STATE1_CLIMBING_LADDER |
              PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HANGING_OFF_LEDGE));
}

static void StartAirKick(PlayState* play, Player* player) {
    sApi->Log("keaton: patada en el aire");
    sState = KEATON_AIR_KICK;
    sJumpDelay = 0;
    sKickBuffered = false;
    Player_SetupAction(play, player, KeatonAction, 0);
    Player_FinishAnimMovement(player);
    // As vanilla's jump slash after SetupAction (func_8083BA90): without MIDAIR, func_8083AA10 pins the kick to
    // prevPos.
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    ArmFists(play, player);
    sAirKickYaw = player->actor.shape.rot.y;
    if (player->focusActor != NULL && player->focusActor->update != NULL) {
        sAirKickYaw = Math_Vec3f_Yaw(&player->actor.world.pos, &player->focusActor->world.pos);
    }
    sAirKickStart = player->actor.world.pos;
    sAirKickFrames = 0;
    player->yaw = sAirKickYaw;
    player->actor.shape.rot.y = sAirKickYaw;
    player->actor.world.rot.y = sAirKickYaw;
    player->linearVelocity = AIR_KICK_SPEED;
    player->actor.speedXZ = AIR_KICK_SPEED;
    player->meleeWeaponState = 1;
    PlayMove(play, player, sAirKick, 0.0f, Animation_GetLastFrame(sAirKick));
    TraceAirKick(player);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
}

static int32_t OnMeleeAction(PlayState* play, Player* player) {
    if (sCombo[0] == NULL || IsKeatonBusy(player) || !CanAct(player) ||
        !CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) || !(player->actor.bgCheckFlags & BG_ON_GROUND)) {
        return SOH_FORM_ACTION_VANILLA;
    }

    sState = KEATON_COMBO;
    sStep = 0;
    ArmFists(play, player);
    Player_SetupAction(play, player, KeatonAction, 0);
    PlayMove(play, player, sCombo[0], 0.0f, Animation_GetLastFrame(sCombo[0]));
    return SOH_FORM_ACTION_STARTED;
}

static bool CanLongJump(PlayState* play, Player* player) {
    return sLongJump != NULL && !IsKeatonBusy(player) && CanAct(player) &&
           CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) && (player->actor.bgCheckFlags & BG_ON_GROUND) &&
           !(player->actor.bgCheckFlags & BG_TOUCHING_WALL);
}

static int32_t StartLongJump(PlayState* play, Player* player) {
    func_80838940(player, sLongJump, LONG_JUMP_LIFT, play, NA_SE_VO_LI_AUTO_JUMP);
    sState = KEATON_LONG_JUMP;
    sJumpDelay = LONG_JUMP_ANIM_DELAY;
    sKickBuffered = false;
    player->linearVelocity = LONG_JUMP_SPEED;
    player->meleeWeaponState = 0;
    player->actor.velocity.y = LONG_JUMP_LIFT;
    player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    player->stateFlags1 |= PLAYER_STATE1_JUMPING;
    return SOH_FORM_ACTION_STARTED;
}

static int32_t OnRollAction(PlayState* play, Player* player) {
    const s8 stick = player->controlStickDirections[player->controlStickDataIndex];
    if (Player_IsZTargeting(player) && stick > PLAYER_STICK_DIR_FORWARD) {
        if (!IsKeatonBusy(player) && CanAct(player) && (player->actor.bgCheckFlags & BG_ON_GROUND) &&
            Player_ActionHandler_10(player, play)) {
            return SOH_FORM_ACTION_STARTED;
        }
        return SOH_FORM_ACTION_VANILLA;
    }
    return CanLongJump(play, player) ? StartLongJump(play, player) : SOH_FORM_ACTION_VANILLA;
}

static int32_t OnShieldAction(PlayState* play, Player* player) {
    if (sReflectAnim == NULL || IsKeatonBusy(player) || !CanAct(player) ||
        !CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    Player_SetupAction(play, player, ReflectorAction, 0);
    OpenReflector(play, player);
    return SOH_FORM_ACTION_STARTED;
}

static int32_t OnZTargetAction(PlayState* play, Player* player) {
    const s8 stick = player->controlStickDirections[player->controlStickDataIndex];

    if (stick > PLAYER_STICK_DIR_FORWARD || !CanLongJump(play, player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    return StartLongJump(play, player);
}

static const SOHFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_MELEE, OnMeleeAction },
    { SOH_PLAYER_ACTION_ROLL, OnRollAction },
    { SOH_PLAYER_ACTION_SHIELD, OnShieldAction },
    { SOH_PLAYER_ACTION_ZTARGET_A, OnZTargetAction },
};

// In the air no action handler list applies: OoT drives the jump, so the animation and the interrupting kick start
// here, at the end of the frame, where nothing overwrites them.
static void UpdateAirMoves(PlayState* play, Player* player) {
    if (!CanAct(player)) {
        if (sState == KEATON_LONG_JUMP) {
            ClearState();
        }
        return;
    }
    if (sState == KEATON_LONG_JUMP) {
        sKickBuffered |= CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B);
        if (sJumpDelay > 0) {
            sJumpDelay--;
            if (sJumpDelay == 0) {
                Player_SetupAction(play, player, KeatonAction, 0);
                PlayMove(play, player, sLongJump, 0.0f, Animation_GetLastFrame(sLongJump));
                player->linearVelocity = LONG_JUMP_SPEED;
                if (sKickBuffered && sAirKick != NULL && !(player->actor.bgCheckFlags & BG_ON_GROUND)) {
                    StartAirKick(play, player);
                }
            }
            return;
        }
        if ((player->actor.bgCheckFlags & BG_ON_GROUND) && player->actor.velocity.y <= 0.0f) {
            ClearState();
            return;
        }
        if (player->actionFunc == KeatonAction) {
            return;
        }
    }
    if (!sKickBuffered && !CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
        return;
    }
    if (IsKeatonBusy(player) || (player->actor.bgCheckFlags & BG_ON_GROUND) || sAirKick == NULL || !CanAct(player)) {
        char reason[128];

        snprintf(reason, sizeof(reason), "keaton: B en el aire ignorado (busy %d, bg %04X, anim %d, canAct %d)",
                 IsKeatonBusy(player), player->actor.bgCheckFlags, sAirKick != NULL, CanAct(player));
        sApi->Log(reason);
        return;
    }
    StartAirKick(play, player);
}

static void CaptureFistBlock(Player* player, Input* input) {
    sFistsBlocked =
        sApi->IsFormActive(KEATON_FORM_KEY) && (sState == KEATON_COMBO || sState == KEATON_AIR_KICK) &&
        ((player->meleeWeaponQuads[0].base.atFlags | player->meleeWeaponQuads[1].base.atFlags) & AT_BOUNCED);
}

static SkeletonHeader* TailSkeleton(void) {
    if (!sTriedTailSkel) {
        sTriedTailSkel = 1;
        SkeletonHeader* header = ResourceMgr_LoadSkeletonByName(sTailSkelPath, NULL);
        // A missing resource returns a pointer to something else, and walking it crashes.
        if (header != NULL && header->limbCount > 0 && header->limbCount <= TAIL_SEGMENTS && header->segment != NULL) {
            sTailSkel = header;
        }
    }
    return sTailSkel;
}

// Player_DrawImpl reads the face from joint 22: eye in the low nibble, 1-based.
static u8 TailPoseFor(Player* player) {
    s32 eye = (player->skelAnime.jointTable[22].x & 0xF) - 1;

    if (eye < 0) {
        return sTailPose;
    }
    if (eye == 5) {
        return 2;
    }
    if (eye >= 6) {
        return 1;
    }
    return 0;
}

static void UpdateTails(Player* player) {
    SkeletonHeader* skeleton = TailSkeleton();

    if (skeleton == NULL) {
        return;
    }
    u8 pose = TailPoseFor(player);
    if (pose != sTailPose) {
        sTailPose = pose;
        sTailFrame = 0;
    }
    sTailFrame = (sTailFrame + 1) % (u32)kTailClipFrames[sTailPose];
    const s16(*clip)[3] = kTailClip[sTailPose][sTailFrame];

    for (s32 i = 0; i < skeleton->limbCount && i < TAIL_SEGMENTS; i++) {
        s16 x = (s16)(clip[i][0] * TAIL_CLIP_AMOUNT);
        s16 y = (s16)(clip[i][1] * TAIL_CLIP_AMOUNT);
        s16 z = (s16)(clip[i][2] * TAIL_CLIP_AMOUNT);

        sTail[i].x += (x - sTail[i].x) >> 2;
        sTail[i].y += (y - sTail[i].y) >> 2;
        sTail[i].z += (z - sTail[i].z) >> 2;
    }
}

static void DrawTailChain(PlayState* play, StandardLimb** limbs, s32 count, s32 index, Mtx* matrices) {
    while (index != 0xFF && index < count) {
        StandardLimb* limb = limbs[index];

        Matrix_Push();
        Matrix_Translate(limb->jointPos.x, limb->jointPos.y, limb->jointPos.z, MTXMODE_APPLY);
        Matrix_RotateZYX(sTail[index].x, sTail[index].y, sTail[index].z, MTXMODE_APPLY);
        Matrix_ToMtx(&matrices[index], (char*)__FILE__, __LINE__);
        if (limb->dList != NULL) {
            OPEN_DISPS(play->state.gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, limb->dList);
            CLOSE_DISPS(play->state.gfxCtx);
        }
        if (limb->child != 0xFF) {
            DrawTailChain(play, limbs, count, limb->child, matrices);
        }
        Matrix_Pop();
        index = limb->sibling;
    }
}

// The tails are not limbs (21 is the player rig's ceiling) but their own chain; this is the only moment the waist is
// the current matrix.
static void DrawTails(PlayState* play) {
    SkeletonHeader* skeleton = TailSkeleton();

    if (skeleton == NULL) {
        return;
    }
    Mtx* matrices = (Mtx*)Graph_Alloc(play->state.gfxCtx, skeleton->limbCount * sizeof(Mtx));
    if (matrices == NULL) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    // 0x0B: segment 0x0D stays with the player skeleton for its whole draw.
    gSPSegment(POLY_OPA_DISP++, TAIL_MATRIX_SEGMENT, (uintptr_t)matrices);
    CLOSE_DISPS(play->state.gfxCtx);

    DrawTailChain(play, (StandardLimb**)skeleton->segment, skeleton->limbCount, 0, matrices);
}

// z_player_lib's quad placement hangs off a wielded weapon and never runs for Keaton: quads follow the hand matrices.
static void UpdateFistQuads(PlayState* play, Player* player, int32_t limbIndex) {
    s32 quadIndex = limbIndex == PLAYER_LIMB_L_HAND ? 0 : 1;
    bool fistsLive = (sState == KEATON_COMBO || sState == KEATON_AIR_KICK) && player->meleeWeaponState != 0;

    if (!fistsLive) {
        player->meleeWeaponQuads[quadIndex].base.atFlags &= ~AT_ON;
        return;
    }

    Vec3f tips[3];
    Vec3f bases[3];

    D_80126080.x = FIST_REACH;
    func_80090A28(player, tips);
    Matrix_MultVec3f(&D_801260A4[0], &bases[0]);
    Matrix_MultVec3f(&D_801260A4[1], &bases[1]);
    Matrix_MultVec3f(&D_801260A4[2], &bases[2]);

    player->meleeWeaponQuads[quadIndex].info.toucher.dmgFlags = SLASH_KOKIRI_SWORD;
    player->meleeWeaponQuads[quadIndex].info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;
    player->meleeWeaponQuads[quadIndex].base.atFlags |= AT_ON;

    WeaponInfo* fist = &player->meleeWeaponInfo[quadIndex + 1];
    if (func_80090480(play, &player->meleeWeaponQuads[quadIndex], fist, &tips[quadIndex + 1], &bases[quadIndex + 1]) &&
        sFistTrail[quadIndex] >= 0) {
        EffectBlure_AddVertex(Effect_GetByIndex(sFistTrail[quadIndex]), &fist->tip, &fist->base);
    }
    // func_80842DF4 probes walls with meleeWeaponInfo[0], the sword tip, which Keaton never moves; otherwise it brakes
    // every hit to -14.
    if (quadIndex == 0) {
        player->meleeWeaponInfo[0] = *fist;
    }
}

static bool ArePunchEffectsLive(void) {
    return sPunchEffect != NULL && sPunchFrames >= 1 && sPunchFrames <= (s32)ARRAY_COUNT(kPunchAlpha);
}

static void DrawPunchEffect(PlayState* play, int32_t hand) {
    if (!ArePunchEffectsLive()) {
        return;
    }
    const u8 alpha = kPunchAlpha[sPunchFrames - 1];

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, alpha);
    gSPDisplayList(POLY_XLU_DISP++, sPunchEffect);
    CLOSE_DISPS(play->state.gfxCtx);

    PunchGhost* ghost = &sPunchGhost[hand][sPunchGhostNext[hand]];

    Matrix_Get(&ghost->matrix);
    ghost->alpha = alpha;
    sPunchGhostNext[hand] = (sPunchGhostNext[hand] + 1) % PUNCH_GHOSTS;
}

static void DrawPunchGhosts(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    for (s32 hand = 0; hand < 2; hand++) {
        for (s32 i = 0; i < PUNCH_GHOSTS; i++) {
            PunchGhost* ghost = &sPunchGhost[hand][i];

            if (sPunchEffect == NULL || ghost->alpha == 0 || ghost->alpha == 255) {
                continue;
            }
            Matrix_Push();
            Matrix_Put(&ghost->matrix);
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, ghost->alpha);
            gSPDisplayList(POLY_XLU_DISP++, sPunchEffect);
            Matrix_Pop();
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static void UpdatePunchEffect(Player* player) {
    const bool striking = (sState == KEATON_COMBO || sState == KEATON_AIR_KICK) && player->meleeWeaponState != 0;

    sPunchFrames = striking ? sPunchFrames + 1 : 0;

    for (s32 hand = 0; hand < 2; hand++) {
        for (s32 i = 0; i < PUNCH_GHOSTS; i++) {
            u8* alpha = &sPunchGhost[hand][i].alpha;

            *alpha = *alpha <= PUNCH_GHOST_FADE ? 0 : *alpha - PUNCH_GHOST_FADE;
        }
    }
}

static void ClearPunchEffect(void) {
    memset(sPunchGhost, 0, sizeof(sPunchGhost));
    sPunchGhostNext[0] = 0;
    sPunchGhostNext[1] = 0;
    sPunchFrames = 0;
}

// Link's voice is a block of 0x20 actions starting at NA_SE_VO_LI_SWORD_N; MM's Deku block is 0x80 above it.
static void SpeakWithDekuVoice(Actor* actor, int32_t kind, uint16_t* sfxId, bool* handled) {
    static MmSfxPlayFunc sPlayMmSfx;

    if (kind != SOH_ACTOR_SFX_VOICE || !sApi->IsFormActive(KEATON_FORM_KEY)) {
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

static void ResolveKeatonBody(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!sApi->IsFormActive(KEATON_FORM_KEY) || dList == NULL || *dList == NULL) {
        return;
    }
    const char* path = NULL;
    if (limbIndex == PLAYER_LIMB_WAIST) {
        *dList = limbDList;
        return;
    }
    const bool running = player->actor.speedXZ > 2.0f && !(player->stateFlags1 & PLAYER_STATE1_IN_WATER);
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        path = player->leftHandType == PLAYER_MODELTYPE_LH_CLOSED || running
                   ? KEATON_MODEL_PATH "/object_link_boy/gLinkAdultLeftHandClosedNearDL"
                   : KEATON_MODEL_PATH "/object_link_boy/gLinkAdultLeftHandNearDL";
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        if (player->rightHandType == PLAYER_MODELTYPE_RH_OCARINA || player->rightHandType == PLAYER_MODELTYPE_RH_OOT) {
            path = KEATON_MODEL_PATH "/object_link_boy/gKeatonHandAndFluteDL";
        } else {
            path = player->rightHandType == PLAYER_MODELTYPE_RH_CLOSED || running
                       ? KEATON_MODEL_PATH "/object_link_boy/gLinkAdultRightHandClosedNearDL"
                       : KEATON_MODEL_PATH "/object_link_boy/gLinkAdultRightHandNearDL";
        }
    }
    if (path != NULL && sApi->HasResource(path)) {
        *dList = ResourceMgr_LoadGfxByName(path);
    }
}

static void DrawKeatonParts(PlayState* play, Player* player, int32_t limbIndex) {
    if (!sApi->IsFormActive(KEATON_FORM_KEY)) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_WAIST) {
        DrawTails(play);
        DrawReflector(play, player);
        DrawPunchGhosts(play);
    }
    if (limbIndex == PLAYER_LIMB_L_HAND || limbIndex == PLAYER_LIMB_R_HAND) {
        UpdateFistQuads(play, player, limbIndex);
        DrawPunchEffect(play, limbIndex == PLAYER_LIMB_L_HAND ? 0 : 1);
    }
}

static void ResolveKeatonStrength(bool* should, va_list args) {
    if (!sApi->IsFormActive(KEATON_FORM_KEY)) {
        return;
    }
    Z64Strength_ResolveUpgrade(should, args, Z64STRENGTH_OFFSET_DEKU);
}

static void ResolveKeatonGrab(bool* should, va_list args) {
    if (!sApi->IsFormActive(KEATON_FORM_KEY)) {
        return;
    }
    Z64Strength_ResolvePrevent(should, Z64STRENGTH_OFFSET_DEKU);
}

static bool sIsFluteOut;

static bool IsHoldingOcarina(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_OCARINA || player->rightHandType == PLAYER_MODELTYPE_RH_OOT;
}

// Rewritten every frame: the message system sets its own instrument again when it opens, and the last write wins.
static void UpdateFluteVoice(PlayState* play, Player* player) {
    bool isSessionOpen = play->msgCtx.msgMode != MSGMODE_NONE;

    if (IsHoldingOcarina(player)) {
        sIsFluteOut = true;
    } else if (!isSessionOpen) {
        if (sIsFluteOut) {
            AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
        }
        sIsFluteOut = false;
    }
    if (sIsFluteOut && isSessionOpen) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_FLUTE);
    }
}

static void PutFluteAway(void) {
    if (sIsFluteOut) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
        sIsFluteOut = false;
    }
}

static u8 sHumanMeleeDamage[2] = { 1, 1 };

static void EnterKeaton(PlayState* play, Player* player) {
    for (s32 quad = 0; quad < 2; quad++) {
        sHumanMeleeDamage[quad] = player->meleeWeaponQuads[quad].info.toucher.damage;
    }
    ClearPunchEffect();
    memset(sTail, 0, sizeof(sTail));
    sTailFrame = 0;
    sTailPose = 0;
    ClearState();
    LoadAnims();
}

static void ExitKeaton(PlayState* play, Player* player) {
    ClearPunchEffect();
    LendClimbEverything(false);
    KillFlame(play);
    StopFistTrails(play);
    CloseReflector(player);
    sShotActive = false;
    PutFluteAway();
    ClearState();
    // OoT keeps AT_ON enabled and gates sword attacks with meleeWeaponState; leaving it cleared makes every swing miss.
    player->meleeWeaponState = 0;
    player->meleeWeaponInfo[0].active = 0;
    for (s32 quad = 0; quad < 2; quad++) {
        ColliderQuad* collider = &player->meleeWeaponQuads[quad];
        collider->base.atFlags = (collider->base.atFlags & ~(AT_HIT | AT_BOUNCED)) | AT_ON;
        collider->info.toucher.damage = sHumanMeleeDamage[quad];
        player->meleeWeaponInfo[quad + 1].active = 0;
    }
}

// Colliders remember their actor and a new scene brings a new player: rebuild them or they point at the old scene's
// memory.
static void ForgetScene(int16_t sceneNum) {
    LendClimbEverything(false);
    sClimbDrain = 0;
    ClearPunchEffect();
    sGuardReady = false;
    sBurstReady = false;
    sShotCylinderReady = false;
    sShotActive = false;
    sFlame = NULL;
    sReflectorState = REFLECTOR_OFF;
    sFistTrail[0] = -1;
    sFistTrail[1] = -1;
    memset(sReflected, 0, sizeof(sReflected));
    ClearState();
}

static void UpdateKeaton(PlayState* play, Player* player) {
    if (sState != KEATON_IDLE && sState != KEATON_LONG_JUMP && !IsKeatonBusy(player)) {
        StopFistTrails(play);
        ClearState();
    }
    UpdatePunchEffect(player);
    UpdateShot(play, player);
    UpdateTails(player);
    UpdateClimb(play, player);
    UpdateAirMoves(play, player);
    UpdateFluteVoice(play, player);
}

static bool CanWearMask(Player* player, PlayState* play) {
    return (player->actor.bgCheckFlags & BG_ON_GROUND) &&
           !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_CUTSCENE));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(KEATON_FORM_KEY);
}

// A vanilla GetItemEntry has no draw callback: its gid selects the host's draw table, and a NULL callback means no
// model.
static void DrawKeatonGetItem(PlayState* play, GetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGetItemMaskDL);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sGetItemMaskEyesDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

static const SOHCustomItemRandomizer sRandomizerItem = {
    sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, KEATON_MASK_KEY, KEATON_MASK_KEY, "",
};

static bool sGuardBorrowedMask;
static void OfferKeatonToGuard(void) {
    if (gPlayState == NULL) {
        sGuardBorrowedMask = false;
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (sApi->IsFormActive(KEATON_FORM_KEY)) {
        player->currentMask = PLAYER_MASK_KEATON;
        sGuardBorrowedMask = true;
    } else if (sGuardBorrowedMask) {
        if (player->currentMask == PLAYER_MASK_KEATON) {
            player->currentMask = PLAYER_MASK_NONE;
        }
        sGuardBorrowedMask = false;
    }
}
static void KeepBorrowedMask(bool* should, va_list args) {
    if (sApi->IsFormActive(KEATON_FORM_KEY)) {
        *should = false;
    }
}

static void RegisterMask(void) {
    SOHCustomItemDefinition mask = Z64Items_Define(KEATON_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    mask.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, KEATON_MASK_PAGE, KEATON_MASK_SLOT, 0);
    // Only the textId is removed so the custom textbox wins.
    mask.getItemEntry = ItemTable_RetrieveEntry(MOD_NONE, GI_MASK_KEATON);
    mask.getItemEntry.textId = 0;
    mask.getItemEntry.drawFunc = DrawKeatonGetItem;
    Z64Items_SetReplaces(&mask, ITEM_MASK_KEATON);
    Z64Items_SetVanillaMode(&mask, SOH_VANILLA_ITEM_REPLACE, RG_KEATON_MASK);
    Z64Items_SetTextbox(&mask, "You got the %rKeaton Mask%w!&Wear it to show the gate&guard your disguise and&"
                               "keep it.^Wear it with %y\xA1%w and you don't&just look like Keaton: you&"
                               "become him.");
    Z64Items_SetPauseText(&mask, "%rKeaton Mask&%wPress %y\xA1%w to become Keaton. As&"
                                 "Keaton: %y\xA1%w punches, held %y\xA1%w throws&fire, %y\xA0%w leaps and climbs.");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);

    Z64Items_SetLogic(&mask, &sRandomizerItem);
    Z64Items_Register(sApi, &mask);
}

static uint16_t ResolveKeatonEquipment(int32_t equipType, uint16_t value) {
    switch (equipType) {
        case EQUIP_TYPE_SWORD:
            return EQUIP_VALUE_SWORD_NONE;
        case EQUIP_TYPE_SHIELD:
            return EQUIP_VALUE_SHIELD_NONE;
        case EQUIP_TYPE_TUNIC:
            return EQUIP_VALUE_TUNIC_KOKIRI;
        case EQUIP_TYPE_BOOTS:
            return EQUIP_VALUE_BOOTS_KOKIRI;
        default:
            return value;
    }
}

static bool AllowsKeatonItem(uint16_t item, const char* customKey) {
    return item == ITEM_OCARINA_FAIRY || item == ITEM_OCARINA_TIME;
}

static void RegisterForm(void) {
    SOHFormDefinition keaton = { 0 };

    keaton.structSize = sizeof(keaton);
    keaton.key = KEATON_FORM_KEY;
    keaton.label = "Keaton";
    keaton.kind = SOH_FORM_KIND_LINK;
    keaton.item = KEATON_MASK_KEY;
    keaton.modelPath = KEATON_MODEL_PATH;
    keaton.rootScaleAdult = KEATON_ROOT_SCALE_ADULT;
    keaton.rootScaleChild = KEATON_ROOT_SCALE_CHILD;
    keaton.rootDrop = KEATON_ROOT_DROP;
    keaton.resolveEquipment = ResolveKeatonEquipment;
    keaton.allowsButtonItem = AllowsKeatonItem;
    keaton.transformMask = sMaskDL;
    keaton.transformAnim = sMaskOn;
    keaton.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    keaton.actions = sActions;
    keaton.actionCount = ARRAY_COUNT(sActions);
    keaton.onEnter = EnterKeaton;
    keaton.onExit = ExitKeaton;
    keaton.update = UpdateKeaton;

    sApi->RegisterForm(&keaton);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    // Before registering: the form declares its transformation pose, which is already a loaded resource.
    LoadAnims();
    RegisterMask();
    RegisterForm();
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawKeatonParts);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveKeatonBody);
    // This hook's id is the actor's, not the sound type: the type is checked inside.
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_PLAYER, SpeakWithDekuVoice);
    SOH_REGISTER_HOOK(sApi, OnActorUpdate, RealignReflectedProjectile);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, CaptureFistBlock);
    SOH_REGISTER_HOOK(sApi, OnBgCheckResolveWallFlags, AllowKeatonWall);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, ResolveKeatonStrength);
    sApi->RegisterVB(VB_PREVENT_STRENGTH, ResolveKeatonGrab);
    sApi->RegisterVB(VB_DRAW_PLAYER_MASK, KeepBorrowedMask);
    sApi->RegisterVB(VB_PLAYER_UNEQUIP_MASK_WITHOUT_BUTTON, KeepBorrowedMask);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, OfferKeatonToGuard);
}
