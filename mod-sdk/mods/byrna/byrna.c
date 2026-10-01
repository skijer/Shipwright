/**
 * Cane of Byrna: Monster Hunter's Insect Glaive. B chains three advancing staff sweeps, the last ending in a burst
 * of magic; R+B vaults. In the air B strikes and every strike that lands bounces Link back up, A evades toward the
 * stick and R+B drives the cane down.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

#define BYRNA_KEY "nei.equip.byrna"

// 3 root translation values and 64 limb rotations: the raw layout of a player animation resource.
#define ANIM_VALUES_PER_FRAME 67
#define BGCHECK_ON_GROUND 0x0001

#define COMBO_STEPS 3
#define COMBO_RESET_FRAMES 40
#define B_HOLD_TO_CHARGE 5
#define FINISHER_FRAME 12
#define FINISHER_TICKS 3
#define FINISHER_SPHERES 3
#define RECOVERY_SOURCE_LAST 28
#define RECOVERY_FRAMES 24
#define STEP_DECAY 1.2f
#define STEP_STOP_DISTANCE 60.0f

#define VAULT_UP 18.0f
#define VAULT_FORWARD 8.0f
#define VAULT_FRAMES 14
#define VAULT_PLANT_FRAMES 7
#define AIR_GRAVITY (-1.1f)
#define AIR_SLASH_GRAVITY (-0.5f)
#define AIR_MIN_VY (-26.0f)
#define AIR_DRAG 0.95f
#define BOUNCE_UP 12.0f
#define EVADE_SPEED 20.0f
#define EVADE_UP 6.0f
#define EVADE_FRAMES 8
#define DESCEND_FALL (-30.0f)
#define DESCEND_WINDUP_FRAMES 8
#define JUMP_XZ_SCALE 0.6f
#define JUMP_Y_SCALE 1.25f
#define FHG_LIGHTBALL_BLUE 4

// The cane in the sword hand, dialled in-game in NEI; its long axis is model Y, measured at -416..+223.
#define CANE_POS_X (-887.74f)
#define CANE_POS_Y 988.20f
#define CANE_POS_Z 54.18f
#define CANE_ROT_X 1.0f
#define CANE_ROT_Z 49.7f
#define CANE_SCALE_X 7.76f
#define CANE_SCALE_Y 10.45f
#define CANE_SCALE_Z 7.71f
#define CANE_AXIS_MIN (-416.0f)
#define CANE_AXIS_MAX 223.0f
#define DEG_TO_BINANG 182.04f

#define GLAIVE(name) "__OTR__misc/link_animetion/gMonsterHunterRise_InsectGlaive_" name

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconCaneOfByrnaTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gCaneOfByrnaNameTex";
static const ALIGN_ASSET(2) char sCaneDL[] = "__OTR__objects/object_somaria/g_byrna_cane_dl";
static const ALIGN_ASSET(2) char sCaneGiveDL[] = "__OTR__objects/object_somaria/g_byrna_cane_give_dl";
static const char* const sGripDL[] = { gLinkAdultLeftHandNearDL, gLinkChildLeftHandNearDL };

// Imported by address: compared against the player's actionFunc, which holds the real function.
extern HOST_DATA void Player_Action_808502D0(Player* player, PlayState* play);
extern HOST_DATA void Player_Action_80844AF4(Player* player, PlayState* play);
extern HOST_DATA Vec3f D_80126080;
s32 Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);
s32 func_80842DF4(PlayState* play, Player* player);
void func_80837918(Player* player, s32 quadIndex, u32 dmgFlags);
void func_808377DC(PlayState* play, Player* player);
void func_80832318(Player* player);
void func_80090A28(Player* player, Vec3f* vecs);
void func_800906D4(PlayState* play, Player* player, Vec3f* newTipPos);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
int Player_IsZTargeting(Player* player);
void func_800AA000(f32 distSq, u8 strength, u8 duration, u8 decreaseRate);

typedef enum {
    CLIP_COMBO_1,
    CLIP_COMBO_2,
    CLIP_COMBO_3,
    CLIP_STAB,
    CLIP_RECOVERY,
    CLIP_SPIN,
    CLIP_JUMP,
    CLIP_LAND,
    CLIP_WALK,
    CLIP_RUN,
    CLIP_VAULT,
    CLIP_AIR_TURN,
    CLIP_AIR_FALL,
    CLIP_AIR_SLASH,
    CLIP_AIR_SLASH_2,
    CLIP_EVADE,
    CLIP_EVADE_BACK,
    CLIP_DESCEND,
    CLIP_DESCEND_FALL,
    CLIP_DESCEND_LAND,
    CLIP_MAX,
} ClipId;

// frames is the speed knob: the same motion resampled into fewer frames plays faster. The clips carry a
// constant root offset in X/Z (up to 118 units), so the root is always centred; ground clips keep their own
// root height so the feet stay planted, airborne ones hold the first frame's because physics owns the height.
typedef struct {
    const char* path;
    s16 firstFrame;
    s16 lastFrame;
    s16 frames;
    bool keepsRootHeight;
} ClipSource;

static const ClipSource sClipSources[CLIP_MAX] = {
    { GLAIVE("ForwardRisingDoubleChargedStaffCombo"), -1, -1, 20, true },
    { GLAIVE("ForwardRisingMultiHitAdvancingStaffSweep_Variant08"), -1, -1, 24, true },
    { GLAIVE("ForwardRisingMultiHitAdvancingStaffSweep"), -1, -1, 26, true },
    { GLAIVE("ForwardRisingTripleAerialStaffStrike_Variant05"), -1, -1, 22, true },
    { GLAIVE("StationaryStaffReadyIdle"), 0, RECOVERY_SOURCE_LAST, RECOVERY_FRAMES, true },
    { GLAIVE("StationaryRisingMultiHitChargedStaffCombo"), -1, -1, 26, true },
    { GLAIVE("BackwardRisingMultiHitRetreatingStaffStrike"), -1, -1, 22, false },
    { GLAIVE("StationaryStaffReadyIdle_Variant05"), -1, -1, 12, true },
    // Walk and run share a phase but sample it at 29 and 20 frames; one length for both cuts the cycle short.
    { GLAIVE("ForwardStaffRun"), -1, -1, 29, true },
    { GLAIVE("ForwardStaffRun"), -1, -1, 20, true },
    { GLAIVE("BackwardHighAerialMultiHitSilkbindStaffStrike_Variant15"), -1, -1, VAULT_FRAMES, false },
    { GLAIVE("ForwardTripleAdvancingStaffSweep_Variant14"), -1, -1, 12, false },
    { GLAIVE("StationarySingleStaffTransition"), -1, -1, 20, false },
    { GLAIVE("LeftHighAerialMultiHitSilkbindStaffStrike"), -1, -1, 14, false },
    { GLAIVE("BackwardRisingTripleChargedStaffCombo"), -1, -1, 20, false },
    { GLAIVE("ForwardDoubleAdvancingStaffSweep_Variant13"), -1, -1, EVADE_FRAMES, false },
    { GLAIVE("LeftRisingTripleChargedStaffCombo"), -1, -1, 18, false },
    { GLAIVE("ForwardRisingTripleAerialStaffStrike"), -1, -1, 16, false },
    { GLAIVE("StationaryStaffReadyIdle_Variant05"), -1, -1, 12, false },
    // Frame 0 is still airborne, a metre above the landing pose.
    { GLAIVE("BackwardDoubleChargedStaffCombo"), 1, -1, 22, true },
};

typedef struct {
    LinkAnimationHeader header;
    s16* frames;
    bool isLoaded;
} Clip;

static Clip sClips[CLIP_MAX];

typedef struct {
    s16 start;
    s16 end;
} HitWindow;

// Measured in the Blender lab off the cane's tip and butt speed. A window opens a frame before the first
// strike: that frame only primes the swept quad, as vanilla's own leading frame does.
typedef struct {
    ClipId clip;
    s8 row;
    f32 step;
    HitWindow windows[2];
} Swing;

static const Swing sComboSwings[COMBO_STEPS] = {
    { CLIP_COMBO_1, PLAYER_MWA_FORWARD_SLASH_2H, 8.0f, { { 2, 9 }, { 12, 15 } } },
    { CLIP_COMBO_2, PLAYER_MWA_RIGHT_SLASH_2H, 10.0f, { { 2, 8 }, { 13, 21 } } },
    { CLIP_COMBO_3, PLAYER_MWA_FORWARD_COMBO_2H, 12.0f, { { 9, 13 }, { -1, -1 } } },
};

static const Swing sStabSwing = { CLIP_STAB, PLAYER_MWA_STAB_2H, 14.0f, { { 2, 4 }, { 6, 8 } } };
static const Swing sAirSlashes[2] = {
    { CLIP_AIR_SLASH, PLAYER_MWA_FORWARD_SLASH_2H, 0.0f, { { 0, 13 }, { -1, -1 } } },
    { CLIP_AIR_SLASH_2, PLAYER_MWA_RIGHT_SLASH_2H, 0.0f, { { 1, 9 }, { -1, -1 } } },
};
static const Swing sDescendLand = { CLIP_DESCEND_LAND, PLAYER_MWA_FORWARD_COMBO_2H, 0.0f, { { 0, 2 }, { 13, 19 } } };

typedef enum {
    GLAIVE_SWING,
    GLAIVE_RECOVERY,
    GLAIVE_VAULT,
    GLAIVE_FALL,
    GLAIVE_AIR_SLASH,
    GLAIVE_EVADE,
    GLAIVE_DESCEND,
    GLAIVE_DESCEND_LAND,
} GlaiveState;

// Link's own frame: right +X, up +Y, forward +Z; pitch lays the box toward the floor.
typedef struct {
    f32 up;
    f32 forward;
    f32 halfWidth;
    f32 halfHeight;
    f32 pitch;
} AttackBox;

static const AttackBox sDescendBox = { 10.0f, 30.0f, 58.0f, 44.0f, 1.2f };

typedef struct {
    f32 up;
    f32 forward;
    s16 radius;
    s16 fxScale;
} MagicSphere;

// Tapered and nearly touching, so it reads as one forward thrust rather than three hits on the same enemy.
static const MagicSphere sFinisherShape[FINISHER_SPHERES] = {
    { 34.0f, 68.0f, 18, 500 },
    { 40.0f, 108.0f, 20, 560 },
    { 46.0f, 150.0f, 18, 500 },
};

static ColliderQuadInit sAttackQuadInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_QUAD },
    { ELEMTYPE_UNK2, { DMG_SLASH_MASTER, 0x00, 0x01 }, { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

#define MAGIC_ELEMENT(radius)                                                                                  \
    {                                                                                                          \
        { ELEMTYPE_UNK2, { DMG_SLASH_MASTER, 0x00, 0x01 }, { 0xFFCFFFFF, 0x00, 0x00 },                         \
          TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },                               \
        { 0, { { 0, 0, 0 }, radius }, 100 },                                                                   \
    }
static ColliderJntSphElementInit sFinisherElementsInit[FINISHER_SPHERES] = {
    MAGIC_ELEMENT(18),
    MAGIC_ELEMENT(20),
    MAGIC_ELEMENT(18),
};
#undef MAGIC_ELEMENT

static ColliderJntSphInit sFinisherInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_JNTSPH },
    FINISHER_SPHERES,
    sFinisherElementsInit,
};

static const SOHModApi* sApi;

static struct {
    u8 state;
    s16 timer;
    f32 vy;
    const Swing* swing;
    u8 comboStep;
    s16 comboIdle;
    bool isChainBuffered;
    s16 bHold;
    u8 airSlashIndex;
    bool canAirSlash;
    bool canEvade;
    bool hasSwingLanded;
    bool hasBounced;
    s8 hiddenMeleeState;
    bool isMeleeStateHidden;
} sGlaive;

static ColliderQuad sAttackQuad;
static bool sIsAttackQuadReady;
static ColliderJntSph sFinisher;
static ColliderJntSphElement sFinisherElements[FINISHER_SPHERES];
static bool sIsFinisherReady;
static bool sIsFinisherFired;
static s16 sFinisherTimer;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(BYRNA_KEY);
}

// Player_GetMeleeWeaponHeld also counts the Deku Stick and the hammer, which must keep their own models.
static bool IsSwordAction(s32 itemAction) {
    return itemAction >= PLAYER_IA_SWORD_MASTER && itemAction <= PLAYER_IA_SWORD_BIGGORON;
}

static bool IsWielding(Player* player) {
    return IsWorn() && IsSwordAction(player->heldItemAction);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECK_ON_GROUND) != 0;
}

static bool IsPlayerBusy(Player* player) {
    return (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                                   PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_DAMAGED |
                                   PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE)) != 0;
}

// Nearest-frame resampling on purpose, since interpolating packed angles smears anything crossing the wrap.
static LinkAnimationHeader* LoadClip(ClipId id) {
    Clip* clip = &sClips[id];
    const ClipSource* source = &sClipSources[id];
    LinkAnimationHeader* raw;
    s16* rawFrames;
    s32 rawCount;
    s32 first;
    s32 last;
    s32 rangeCount;
    s32 outCount;

    if (clip->isLoaded) {
        return clip->frames != NULL ? &clip->header : NULL;
    }
    clip->isLoaded = true;
    raw = ResourceMgr_LoadPlayerAnimAsHeader(source->path);
    if (raw == NULL || raw->common.frameCount <= 0) {
        return NULL;
    }
    rawFrames = (s16*)raw->segment;
    rawCount = raw->common.frameCount;
    first = CLAMP(source->firstFrame, 0, rawCount - 1);
    last = source->lastFrame < 0 ? rawCount - 1 : CLAMP(source->lastFrame, first, rawCount - 1);
    rangeCount = last - first + 1;
    outCount = source->frames > 0 ? source->frames : rangeCount;
    clip->frames = (s16*)malloc((size_t)outCount * ANIM_VALUES_PER_FRAME * sizeof(s16));
    if (clip->frames == NULL) {
        return NULL;
    }
    for (s32 frame = 0; frame < outCount; frame++) {
        s32 sourceFrame = first + MIN(frame * rangeCount / outCount, rangeCount - 1);
        s16* out = &clip->frames[frame * ANIM_VALUES_PER_FRAME];

        memcpy(out, &rawFrames[sourceFrame * ANIM_VALUES_PER_FRAME], ANIM_VALUES_PER_FRAME * sizeof(s16));
        out[0] = 0;
        out[2] = 0;
        if (!source->keepsRootHeight) {
            out[1] = rawFrames[first * ANIM_VALUES_PER_FRAME + 1];
        }
    }
    clip->header.common.frameCount = (s16)outCount;
    clip->header.segment = clip->frames;
    return &clip->header;
}

static void PlayClip(PlayState* play, Player* player, ClipId id, bool isLooping, f32 morph) {
    LinkAnimationHeader* anim = LoadClip(id);

    if (anim == NULL) {
        return;
    }
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim),
                         isLooping ? ANIMMODE_LOOP : ANIMMODE_ONCE, morph);
}

static void Sparkle(PlayState* play, Vec3f* pos, s16 scale, s32 life) {
    static Color_RGBA8 prim = { 235, 245, 255, 255 };
    static Color_RGBA8 env = { 120, 170, 220, 0 };
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    EffectSsKiraKira_SpawnDispersed(play, pos, &zero, &zero, &prim, &env, scale, life);
}

// A light-ball burst from the update: nothing has to be object-resident, unlike Morpha's display lists.
static void DrawBlob(PlayState* play, Vec3f* pos, s16 scale) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    EffectSsFhgFlash_SpawnLightBall(play, pos, &zero, &zero, scale, FHG_LIGHTBALL_BLUE);
}

static void PlaceAttackQuad(PlayState* play, Player* player, const AttackBox* box) {
    static const f32 sCornerX[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
    static const f32 sCornerY[4] = { 1.0f, 1.0f, -1.0f, -1.0f };
    Vec3f corners[4];
    f32 sinY = Math_SinS(player->actor.shape.rot.y);
    f32 cosY = Math_CosS(player->actor.shape.rot.y);

    if (!sIsAttackQuadReady) {
        Collider_InitQuad(play, &sAttackQuad);
        Collider_SetQuad(play, &sAttackQuad, &player->actor, &sAttackQuadInit);
        sIsAttackQuadReady = true;
    }
    for (s32 i = 0; i < 4; i++) {
        f32 right = sCornerX[i] * box->halfWidth;
        f32 up = box->up + sCornerY[i] * box->halfHeight * cosf(box->pitch);
        f32 forward = box->forward + sCornerY[i] * box->halfHeight * sinf(box->pitch);

        corners[i].x = player->actor.world.pos.x + right * cosY + forward * sinY;
        corners[i].y = player->actor.world.pos.y + up;
        corners[i].z = player->actor.world.pos.z + forward * cosY - right * sinY;
    }
    Collider_SetQuadVertices(&sAttackQuad, &corners[0], &corners[1], &corners[2], &corners[3]);
    sAttackQuad.base.atFlags &= ~AT_HIT;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sAttackQuad.base);
}

static void PlaceFinisherSpheres(Player* player) {
    f32 sinY = Math_SinS(player->actor.shape.rot.y);
    f32 cosY = Math_CosS(player->actor.shape.rot.y);

    for (s32 i = 0; i < FINISHER_SPHERES; i++) {
        ColliderJntSphElementDim* dim = &sFinisherElements[i].dim;

        dim->worldSphere.center.x = player->actor.world.pos.x + sFinisherShape[i].forward * sinY;
        dim->worldSphere.center.y = player->actor.world.pos.y + sFinisherShape[i].up;
        dim->worldSphere.center.z = player->actor.world.pos.z + sFinisherShape[i].forward * cosY;
        dim->worldSphere.radius = sFinisherShape[i].radius;
        dim->scale = 1.0f;
    }
}

static void FireFinisher(PlayState* play, Player* player) {
    if (!sIsFinisherReady) {
        Collider_InitJntSph(play, &sFinisher);
        Collider_SetJntSph(play, &sFinisher, &player->actor, &sFinisherInit, sFinisherElements);
        sIsFinisherReady = true;
    }
    sIsFinisherFired = true;
    sFinisherTimer = FINISHER_TICKS;
    sFinisher.base.atFlags &= ~AT_HIT;
    Sfx_PlaySfxCentered(NA_SE_IT_MAGIC_ARROW_SHOT);
    func_800AA000(80.0f, 110, 6, 16);
}

// One enemy may overlap more than one sphere: once the collider lands, the glow stays but no second pass hits.
static void TickFinisher(PlayState* play, Player* player) {
    if (sFinisherTimer <= 0) {
        return;
    }
    PlaceFinisherSpheres(player);
    for (s32 i = 0; i < FINISHER_SPHERES; i++) {
        Vec3f* center = &sFinisherElements[i].dim.worldSphere.center;

        DrawBlob(play, center, sFinisherShape[i].fxScale);
        if (sFinisherTimer == FINISHER_TICKS) {
            Sparkle(play, center, 420, 7);
        }
    }
    if (!(sFinisher.base.atFlags & AT_HIT)) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &sFinisher.base);
    }
    sFinisherTimer--;
}

static bool IsInHitWindow(const Swing* swing, f32 frame) {
    s32 current = (s32)(frame + 0.5f);

    for (s32 i = 0; i < ARRAY_COUNT(swing->windows); i++) {
        if (current >= swing->windows[i].start && current <= swing->windows[i].end) {
            return true;
        }
    }
    return false;
}

// Outside a window the swept quad must forget its last edge, or the next window's first quad fans across
// everything the cane passed through in between. func_80832318 is vanilla's own reset for exactly that.
static void UpdateHitWindow(Player* player, const Swing* swing) {
    if (!IsInHitWindow(swing, player->skelAnime.curFrame)) {
        func_80832318(player);
        return;
    }
    player->meleeWeaponState = 1;
    if ((player->meleeWeaponQuads[0].base.atFlags | player->meleeWeaponQuads[1].base.atFlags) & AT_HIT) {
        sGlaive.hasSwingLanded = true;
    }
}

static void FaceTarget(Player* player) {
    if (player->focusActor == NULL) {
        return;
    }
    player->actor.shape.rot.y = Math_Vec3f_Yaw(&player->actor.world.pos, &player->focusActor->world.pos);
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
}

static bool IsCloseToTarget(Player* player) {
    return player->focusActor != NULL &&
           Actor_WorldDistXZToActor(&player->actor, player->focusActor) < STEP_STOP_DISTANCE;
}

static void GlaiveAction(Player* player, PlayState* play);

// AT_HIT outlives the swing that set it until the next window primes the quads.
static void ArmSwing(Player* player, const Swing* swing) {
    sGlaive.swing = swing;
    sGlaive.hasSwingLanded = false;
    sGlaive.hasBounced = false;
    player->meleeWeaponAnimation = swing->row;
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_HIT;
    player->meleeWeaponQuads[1].base.atFlags &= ~AT_HIT;
    func_80832318(player);
    func_80837918(player, 0, DMG_SLASH_MASTER);
    func_80837918(player, 1, DMG_SLASH_MASTER);
}

// Chain rows morph into each other; a swing out of idle keeps the hard start vanilla gives every attack.
static void StartSwing(PlayState* play, Player* player, const Swing* swing, bool isChained) {
    if (LoadClip(swing->clip) == NULL) {
        return;
    }
    if (player->actionFunc != GlaiveAction) {
        Player_SetupAction(play, player, GlaiveAction, 0);
    }
    sGlaive.state = GLAIVE_SWING;
    sGlaive.isChainBuffered = false;
    sIsFinisherFired = false;
    ArmSwing(player, swing);
    FaceTarget(player);
    player->yaw = player->actor.shape.rot.y;
    player->linearVelocity = IsCloseToTarget(player) ? 0.0f : swing->step;
    PlayClip(play, player, swing->clip, false, isChained ? -6.0f : 0.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
}

static void StartComboStep(PlayState* play, Player* player, bool isChained) {
    const Swing* swing = &sComboSwings[sGlaive.comboStep];

    sGlaive.comboStep = (sGlaive.comboStep + 1) % COMBO_STEPS;
    sGlaive.comboIdle = 0;
    StartSwing(play, player, swing, isChained);
}

static void EndGlaiveAction(PlayState* play, Player* player) {
    func_80832318(player);
    sGlaive.swing = NULL;
    sGlaive.state = GLAIVE_SWING;
    func_80839FFC(player, play);
}

static void UpdateSwing(PlayState* play, Player* player, Input* input) {
    const Swing* swing = sGlaive.swing;
    f32 frame = player->skelAnime.curFrame;

    Math_StepToF(&player->linearVelocity, 0.0f, STEP_DECAY);
    UpdateHitWindow(player, swing);
    if (CHECK_BTN_ALL(input->press.button, BTN_B) && frame >= swing->windows[0].start) {
        sGlaive.isChainBuffered = true;
    }
    if (swing->clip == CLIP_COMBO_3 && !sIsFinisherFired && frame >= FINISHER_FRAME) {
        FireFinisher(play, player);
    }
    if (!LinkAnimation_Update(play, &player->skelAnime)) {
        return;
    }
    func_80832318(player);
    if (sGlaive.isChainBuffered && sGlaive.comboStep != 0) {
        StartComboStep(play, player, true);
        return;
    }
    // Vanilla only opens the spin charge the frame after its own short swing; ours end here, B still down.
    if (sGlaive.bHold >= B_HOLD_TO_CHARGE) {
        sGlaive.swing = NULL;
        func_808377DC(play, player);
        return;
    }
    sGlaive.state = GLAIVE_RECOVERY;
    sGlaive.swing = NULL;
    PlayClip(play, player, CLIP_RECOVERY, false, -4.0f);
}

static void UpdateRecovery(PlayState* play, Player* player, Input* input) {
    Math_StepToF(&player->linearVelocity, 0.0f, 5.0f);
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        StartComboStep(play, player, true);
        return;
    }
    if (LinkAnimation_Update(play, &player->skelAnime)) {
        EndGlaiveAction(play, player);
    }
}

static void EnterAirState(PlayState* play, Player* player, u8 state, ClipId clip, bool isLooping) {
    sGlaive.state = state;
    sGlaive.timer = 0;
    PlayClip(play, player, clip, isLooping, -4.0f);
}

static void ExitAir(PlayState* play, Player* player) {
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    EndGlaiveAction(play, player);
}

// The void check measures the fall from fallStartHeight, so an airborne glaive keeps it level with Link.
static void SetVerticalSpeed(Player* player, f32 vy) {
    sGlaive.vy = vy;
    player->actor.gravity = 0.0f;
    player->actor.minVelocityY = DESCEND_FALL;
    player->actor.velocity.y = vy;
    player->fallStartHeight = (s16)player->actor.world.pos.y;
    player->fallDistance = 0;
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
}

static void Fall(Player* player, f32 gravity) {
    SetVerticalSpeed(player, MAX(sGlaive.vy + gravity, AIR_MIN_VY));
}

static void StartFall(PlayState* play, Player* player) {
    func_80832318(player);
    sGlaive.swing = NULL;
    EnterAirState(play, player, GLAIVE_FALL, CLIP_AIR_FALL, true);
}

// Alternating two clips keeps a string of bounces from replaying the same flurry.
static void StartAirSlash(PlayState* play, Player* player) {
    const Swing* swing = &sAirSlashes[sGlaive.airSlashIndex];

    sGlaive.airSlashIndex ^= 1;
    sGlaive.canAirSlash = false;
    FaceTarget(player);
    ArmSwing(player, swing);
    EnterAirState(play, player, GLAIVE_AIR_SLASH, swing->clip, false);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
}

// The dash follows the stick but Link keeps facing ahead; the stick directions are quarter turns from his yaw.
static void StartEvade(PlayState* play, Player* player) {
    s8 direction = player->controlStickDirections[player->controlStickDataIndex];
    bool isBackward = direction == PLAYER_STICK_DIR_BACKWARD;

    sGlaive.canEvade = false;
    player->yaw = player->actor.shape.rot.y + (direction > PLAYER_STICK_DIR_NONE ? direction * 0x4000 : 0);
    player->linearVelocity = EVADE_SPEED;
    SetVerticalSpeed(player, EVADE_UP);
    EnterAirState(play, player, GLAIVE_EVADE, isBackward ? CLIP_EVADE_BACK : CLIP_EVADE, false);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_AUTO_JUMP);
}

static void StartDescend(PlayState* play, Player* player) {
    func_80832318(player);
    sGlaive.swing = NULL;
    player->linearVelocity = 0.0f;
    SetVerticalSpeed(player, DESCEND_FALL);
    EnterAirState(play, player, GLAIVE_DESCEND, CLIP_DESCEND, false);
}

// R+B is tested before plain B because R alone changes nothing in the air.
static void ReadAirButtons(PlayState* play, Player* player, Input* input) {
    bool isBPressed = CHECK_BTN_ALL(input->press.button, BTN_B);

    if (isBPressed && CHECK_BTN_ALL(input->cur.button, BTN_R)) {
        StartDescend(play, player);
    } else if (isBPressed && sGlaive.canAirSlash) {
        StartAirSlash(play, player);
    } else if (CHECK_BTN_ALL(input->press.button, BTN_A) && sGlaive.canEvade) {
        StartEvade(play, player);
    }
}

// The bounce is what keeps a glaive aloft: each strike that connects buys one more slash and one more evade.
static void BounceOnHit(Player* player) {
    if (!sGlaive.hasSwingLanded || sGlaive.hasBounced) {
        return;
    }
    sGlaive.hasBounced = true;
    sGlaive.canAirSlash = true;
    sGlaive.canEvade = true;
    SetVerticalSpeed(player, BOUNCE_UP);
}

static void UpdateAirSlash(PlayState* play, Player* player, Input* input, bool isClipDone) {
    Fall(player, AIR_SLASH_GRAVITY);
    player->linearVelocity *= AIR_DRAG;
    UpdateHitWindow(player, sGlaive.swing);
    BounceOnHit(player);
    if (sGlaive.canAirSlash && CHECK_BTN_ALL(input->press.button, BTN_B)) {
        StartAirSlash(play, player);
    } else if (isClipDone) {
        StartFall(play, player);
    }
}

static void LandDescend(PlayState* play, Player* player) {
    player->linearVelocity = 0.0f;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    ArmSwing(player, &sDescendLand);
    EnterAirState(play, player, GLAIVE_DESCEND_LAND, CLIP_DESCEND_LAND, false);
    Sfx_PlaySfxCentered(NA_SE_IT_HAMMER_HIT);
    func_800AA000(0.0f, 180, 10, 40);
}

static void UpdateAir(PlayState* play, Player* player, Input* input) {
    bool isClipDone;

    sGlaive.timer++;
    player->actor.speedXZ = player->linearVelocity;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    if (IsGrounded(player) && sGlaive.state != GLAIVE_VAULT && sGlaive.state != GLAIVE_DESCEND_LAND) {
        if (sGlaive.state == GLAIVE_DESCEND) {
            LandDescend(play, player);
        } else {
            ExitAir(play, player);
        }
        return;
    }
    isClipDone = LinkAnimation_Update(play, &player->skelAnime);
    switch (sGlaive.state) {
        case GLAIVE_VAULT:
            Fall(player, AIR_GRAVITY);
            if (sGlaive.timer >= VAULT_FRAMES) {
                EnterAirState(play, player, GLAIVE_FALL, CLIP_AIR_TURN, false);
            } else if (sGlaive.timer >= VAULT_PLANT_FRAMES) {
                ReadAirButtons(play, player, input);
            }
            break;
        case GLAIVE_FALL:
            Fall(player, AIR_GRAVITY);
            player->linearVelocity *= AIR_DRAG;
            if (isClipDone && player->skelAnime.animation != LoadClip(CLIP_AIR_FALL)) {
                PlayClip(play, player, CLIP_AIR_FALL, true, -4.0f);
            }
            ReadAirButtons(play, player, input);
            break;
        case GLAIVE_AIR_SLASH:
            UpdateAirSlash(play, player, input, isClipDone);
            break;
        case GLAIVE_EVADE:
            Fall(player, AIR_GRAVITY);
            if (sGlaive.timer >= EVADE_FRAMES) {
                StartFall(play, player);
            }
            break;
        case GLAIVE_DESCEND:
            SetVerticalSpeed(player, DESCEND_FALL);
            PlaceAttackQuad(play, player, &sDescendBox);
            if (sGlaive.timer == DESCEND_WINDUP_FRAMES) {
                PlayClip(play, player, CLIP_DESCEND_FALL, true, -4.0f);
            }
            break;
        case GLAIVE_DESCEND_LAND:
            player->linearVelocity = 0.0f;
            UpdateHitWindow(player, sGlaive.swing);
            if (isClipDone) {
                ExitAir(play, player);
            }
            break;
        default:
            ExitAir(play, player);
            break;
    }
}

static bool IsAirState(u8 state) {
    return state >= GLAIVE_VAULT;
}

// Its own action, so damage, water or a cutscene replace it the way they replace any vanilla one.
static void GlaiveAction(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];

    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    if (IsAirState(sGlaive.state)) {
        UpdateAir(play, player, input);
        return;
    }
    if (func_80842DF4(play, player)) {
        sGlaive.swing = NULL;
        return;
    }
    if (sGlaive.state == GLAIVE_RECOVERY) {
        UpdateRecovery(play, player, input);
    } else if (sGlaive.swing != NULL) {
        UpdateSwing(play, player, input);
    } else {
        EndGlaiveAction(play, player);
    }
}

static bool IsInGlaiveAction(Player* player) {
    return player->actionFunc == GlaiveAction;
}

static void Vault(PlayState* play, Player* player) {
    if (LoadClip(CLIP_VAULT) == NULL) {
        return;
    }
    Player_SetupAction(play, player, GlaiveAction, 0);
    func_80832318(player);
    sGlaive.swing = NULL;
    sGlaive.canAirSlash = true;
    sGlaive.canEvade = true;
    FaceTarget(player);
    player->linearVelocity = VAULT_FORWARD;
    player->yaw = player->actor.shape.rot.y;
    player->actor.bgCheckFlags &= ~BGCHECK_ON_GROUND;
    SetVerticalSpeed(player, VAULT_UP);
    EnterAirState(play, player, GLAIVE_VAULT, CLIP_VAULT, false);
    Sfx_PlaySfxCentered(NA_SE_PL_JUMP);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_AUTO_JUMP);
}

// The thrust only starts the chain: mid-combo, the forward stick that re-aims would otherwise eat a slash.
// With R held, B is the vault and never a swing.
static void TakeMeleeButton(PlayState* play, Player* player, int32_t action, bool* consumed, bool* startedAction) {
    Input* input = &play->state.input[0];
    bool isThrust;

    if (*consumed || !IsWielding(player) || !IsGrounded(player) || IsPlayerBusy(player) ||
        !CHECK_BTN_ALL(input->press.button, BTN_B) || CHECK_BTN_ALL(input->cur.button, BTN_R) ||
        LoadClip(CLIP_COMBO_1) == NULL) {
        return;
    }
    isThrust = sGlaive.comboStep == 0 && Player_IsZTargeting(player) &&
               player->controlStickDirections[player->controlStickDataIndex] == PLAYER_STICK_DIR_FORWARD;
    if (isThrust) {
        StartSwing(play, player, &sStabSwing, false);
    } else {
        StartComboStep(play, player, false);
    }
    *consumed = true;
    *startedAction = IsInGlaiveAction(player);
}

// The glaive has no guard of its own: R belongs to the vault while the cane is equipped.
static void TakeShieldButton(PlayState* play, Player* player, int32_t action, bool* consumed, bool* startedAction) {
    if (*consumed || !IsWorn()) {
        return;
    }
    *consumed = true;
    *startedAction = false;
}

static void ReadVaultButtons(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];

    if (IsWielding(player) && IsGrounded(player) && CHECK_BTN_ALL(input->cur.button, BTN_R) &&
        CHECK_BTN_ALL(input->press.button, BTN_B)) {
        Vault(play, player);
    }
}

// A pole weapon vaults higher and travels less; the reskin is renewed whenever vanilla puts its own clip back.
static void ReskinVanillaSwing(PlayState* play, Player* player) {
    s8 row = player->meleeWeaponAnimation;
    bool isMelee = player->actionFunc == Player_Action_808502D0;
    bool isJumpAir = player->actionFunc == Player_Action_80844AF4;
    ClipId clip = CLIP_MAX;
    LinkAnimationHeader* anim;

    if (isMelee && row >= PLAYER_MWA_SPIN_ATTACK_1H && row <= PLAYER_MWA_BIG_SPIN_2H) {
        clip = CLIP_SPIN;
    } else if (isJumpAir && (row == PLAYER_MWA_JUMPSLASH_START || row == PLAYER_MWA_JUMPSLASH_FINISH)) {
        clip = CLIP_JUMP;
    } else if (isMelee && row == PLAYER_MWA_JUMPSLASH_FINISH) {
        clip = CLIP_LAND;
    }
    if (clip == CLIP_MAX) {
        return;
    }
    anim = LoadClip(clip);
    if (anim == NULL || player->skelAnime.animation == anim) {
        return;
    }
    if (clip == CLIP_JUMP) {
        player->linearVelocity *= JUMP_XZ_SCALE;
        player->actor.velocity.y *= JUMP_Y_SCALE;
    }
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, player->skelAnime.curFrame,
                         Animation_GetLastFrame(anim), ANIMMODE_ONCE, -4.0f);
}

static void TickComboIdle(Player* player) {
    if (IsInGlaiveAction(player) || sGlaive.comboStep == 0) {
        return;
    }
    if (++sGlaive.comboIdle >= COMBO_RESET_FRAMES) {
        sGlaive.comboStep = 0;
        sGlaive.comboIdle = 0;
    }
}

static void TickByrna(void) {
    PlayState* play = gPlayState;
    Player* player;

    if (play == NULL || !IsWorn()) {
        return;
    }
    player = GET_PLAYER(play);
    if (!IsInGlaiveAction(player) && IsAirState(sGlaive.state)) {
        sGlaive.state = GLAIVE_SWING;
    }
    TickFinisher(play, player);
    TickComboIdle(player);
    sGlaive.bHold = CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_B) ? MIN(sGlaive.bHold + 1, B_HOLD_TO_CHARGE) : 0;
    if (IsPlayerBusy(player) || IsAirState(sGlaive.state)) {
        return;
    }
    ReadVaultButtons(play, player);
    ReskinVanillaSwing(play, player);
}

// R never reaches vanilla, and neither do A or B stacked on it; in the air the glaive owns every button.
static void KeepButtonsFromVanilla(Player* player, Input* input) {
    u16 taken = BTN_R;

    if (!IsWorn()) {
        return;
    }
    if (CHECK_BTN_ALL(input->cur.button, BTN_R) || IsAirState(sGlaive.state)) {
        taken |= BTN_A | BTN_B | BTN_L;
    }
    input->cur.button &= ~taken;
    input->press.button &= ~taken;
}

static void RunWithStaff(int32_t group, int32_t animType, LinkAnimationHeader** anim) {
    PlayState* play = gPlayState;
    LinkAnimationHeader* staff;

    if (play == NULL || (group != PLAYER_ANIMGROUP_walk && group != PLAYER_ANIMGROUP_run) ||
        !IsWielding(GET_PLAYER(play))) {
        return;
    }
    staff = LoadClip(group == PLAYER_ANIMGROUP_walk ? CLIP_WALK : CLIP_RUN);
    if (staff != NULL) {
        *anim = staff;
    }
}

// Two-handed like the Biggoron's Sword: the shield goes on the back while the cane is out.
static void WieldWithBothHands(Player* player, int32_t itemAction, int32_t* modelGroup) {
    if (IsWorn() && IsSwordAction(itemAction)) {
        *modelGroup = PLAYER_MODELGROUP_BGS;
    }
}

static void GripCane(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (limbIndex == PLAYER_LIMB_L_HAND && IsWielding(player)) {
        *dList = ResourceMgr_LoadGfxByName(sGripDL[gSaveContext.linkAge]);
    }
}

static void ApplyCaneMatrix(void) {
    Matrix_Translate(CANE_POS_X, CANE_POS_Y, CANE_POS_Z, MTXMODE_APPLY);
    Matrix_RotateZYX((s16)(CANE_ROT_X * DEG_TO_BINANG), 0, (s16)(CANE_ROT_Z * DEG_TO_BINANG), MTXMODE_APPLY);
}

// The sword code lays its blade along +X from the hilt; the cane runs along +Y from the butt, so +X is turned
// onto +Y. Only the axial scale is baked in: the engine adds its tip margin and lateral offsets in sword units.
static void TraceCaneSwing(PlayState* play, Player* player) {
    Vec3f edges[3];

    Matrix_Push();
    ApplyCaneMatrix();
    Matrix_RotateZ(M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Translate(CANE_AXIS_MIN * CANE_SCALE_Y, 0.0f, 0.0f, MTXMODE_APPLY);
    D_80126080.x = (CANE_AXIS_MAX - CANE_AXIS_MIN) * CANE_SCALE_Y;
    EffectBlure_ChangeType(Effect_GetByIndex(player->meleeWeaponEffectIndex), TRAIL_TYPE_BIGGORON_SWORD);
    func_80090A28(player, edges);
    func_800906D4(play, player, edges);
    Matrix_Pop();
}

static void DrawCane(PlayState* play, Player* player, int32_t limbIndex) {
    if (limbIndex != PLAYER_LIMB_L_HAND || player != GET_PLAYER(play) || !IsWielding(player)) {
        return;
    }
    if (sGlaive.isMeleeStateHidden) {
        player->meleeWeaponState = sGlaive.hiddenMeleeState;
        sGlaive.isMeleeStateHidden = false;
        if (player->meleeWeaponState != 0 && player->actor.scale.y >= 0.0f) {
            TraceCaneSwing(play, player);
        }
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    ApplyCaneMatrix();
    Matrix_Scale(CANE_SCALE_X, CANE_SCALE_Y, CANE_SCALE_Z, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sCaneDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// The vanilla hand block would lay the trail along the hidden sword first; it sees no swing until the cane's
// own post-limb puts the state back.
static void HideSwingFromSwordHand(Actor* actor, PlayState* play, bool* drawVanilla) {
    Player* player = (Player*)actor;

    if (actor != &GET_PLAYER(play)->actor || !IsWielding(player) || player->meleeWeaponState == 0) {
        return;
    }
    sGlaive.hiddenMeleeState = player->meleeWeaponState;
    sGlaive.isMeleeStateHidden = true;
    player->meleeWeaponState = 0;
}

static void RestoreHiddenSwing(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (actor != &GET_PLAYER(play)->actor || !sGlaive.isMeleeStateHidden) {
        return;
    }
    player->meleeWeaponState = sGlaive.hiddenMeleeState;
    sGlaive.isMeleeStateHidden = false;
}

static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.05f * 1.15f, 0.05f * 1.15f, 0.05f * 1.15f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sCaneGiveDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The colliders belong to the previous scene's collision context; they are rebuilt on first use.
static void ForgetSceneState(int16_t sceneNum) {
    sGlaive.state = GLAIVE_SWING;
    sGlaive.swing = NULL;
    sGlaive.isMeleeStateHidden = false;
    sIsAttackQuadReady = false;
    sIsFinisherReady = false;
    sFinisherTimer = 0;
}

static void PutCaneAway(const char* key) {
    ForgetSceneState(0);
    sGlaive.comboStep = 0;
}

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate",          "OnPlayerActionHandler", "OnPlayerFilterInput",  "OnPlayerResolveAnim",
    "OnPlayerResolveModelGroup", "OnPlayerResolveLimbDraw", "OnPlayerPostLimbDraw", "OnActorDraw",
    "OnActorDrawEnd",          "OnSceneInit",
};

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOHCustomEquipDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = BYRNA_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rCane of Byrna&%wHold %y\xA3%w and press %y\xA0%w to vault. In the air, %y\xA0%w "
                           "strikes, %y\x9F%w evades and %y\xA3%w+%y\xA0%w drives the cane down.";
    definition.getItemText = "You got the %rCane of Byrna%w!&A glaive that vaults its wielder skyward. Every blow "
                             "that lands in the air lifts you for another.";
    definition.slot = SOH_EQUIP_SLOT_SWORD;
    definition.page = 0;
    definition.row = SOH_EQUIP_SLOT_SWORD;
    definition.column = 1;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_ANY;
    definition.vanillaBase = EQUIP_VALUE_SWORD_KOKIRI;
    definition.toggles = 1;
    definition.onUnequip = PutCaneAway;
    definition.getItemEntry.drawFunc = DrawGetItem;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickByrna);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_MELEE, TakeMeleeButton);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_SHIELD, TakeShieldButton);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, KeepButtonsFromVanilla);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveAnim, RunWithStaff);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveModelGroup, WieldWithBothHands);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, GripCane);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawCane);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_PLAYER, HideSwingFromSwordHand);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, RestoreHiddenSwing);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
}
