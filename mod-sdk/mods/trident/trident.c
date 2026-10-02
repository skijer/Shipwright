/**
 * Trident: Phantom Ganon's lance, played as Monster Hunter's Gunlance. B chains three heavy strikes, holding B
 * charges three levels up to Ganondorf's big magic, R+B runs with the lance out front, and holding R+A takes
 * flight the way Phantom Ganon does.
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
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

#define TRIDENT_KEY "nei.equip.trident"
#define PROJECTILE_KEY "nei.equip.trident_projectile"
#define MAGIC_CAPE_KEY "nei.equip.magic_cape"

// 3 root translation values and 64 limb rotations: the raw layout of a player animation resource.
#define ANIM_VALUES_PER_FRAME 67
#define BGCHECK_ON_GROUND 0x0001
#define SPEED_MODE_LINEAR 0.0f

#define COMBO_STEPS 3
#define COMBO_RESET_FRAMES 40
#define B_HOLD_TO_CHARGE 5
#define STAB_LUNGE_SPEED 15.0f

// Vanilla fills at 0.02 a frame; the spec wants five seconds. 0.85 is vanilla's own orange threshold.
#define CHARGE_RATE 0.0085f
#define CHARGE_LEVEL_2 0.85f
#define CHARGE_LEVEL_3 0.995f
#define CHARGE_LEVEL_3_HOLD 20
#define BALL_MAGIC_COST 24
#define MAX_IMMUNE_LAST 24
#define MAX_FAST_FROM 25
#define MAX_FAST_SPEED 3.0f
#define MAX_BURST_FRAME 64
#define MAX_BALL_DAMAGE 12
#define MAX_RECOIL (-10.0f)
#define GOLD_FRAMES 2
#define DOME_SCALE_1 0.025f
#define DOME_SCALE_2 0.060f
#define DOME_FRAMES 14
#define DOME_UP 18.0f
#define JUMP_XZ_SCALE 0.45f
#define JUMP_Y_SCALE 0.7f
#define JUMP_FINISH_LUNGE 3.0f

#define DASH_DAMAGE 1
#define DASH_SPEED_SCALE 1.2f
#define DASH_BASE_SPEED 9.0f

#define FLY_ENTER_HOLD 6
#define FLY_SPEED 5.5f
#define FLY_CLIMB 4.0f
#define FLY_MAGIC_COST 4
#define FLY_MAGIC_TICK 20
#define FLY_TAKEOFF_VY 3.0f
#define LAUNCH_SPEED 22.0f
#define LAUNCH_MAX_FRAMES 40
#define LAUNCH_ARRIVE_DIST 45.0f
#define LAUNCH_LOOP_A 10
#define LAUNCH_LOOP_B 16
#define ARC_FALL 1.4f
#define ARC_MIN_VY 5.0f
#define ARC_MIN_FLIGHT 4.0f
#define POUND_FALL (-22.0f)
#define POUND_RADIUS 110.0f
#define POUND_DAMAGE 4
#define POUND_FX_SCALE 400
#define POUND_FX_STEP 90
#define SHOOT_FRAME 8
#define MELEE_DAMAGE 2
#define LAND_GRAVITY (-1.2f)

#define PROJECTILE_GRACE 5
#define BALL_LIFETIME 60
#define BALL_SPEED 22.0f
#define BALL_HOMING 0.25f
#define BALL_SEEK_RANGE 2500.0f
#define BALL_BURST_FRAMES 6
#define BALL_BURST_PEAK 2.6f
#define BALL_BURST_RISE 0.3f
#define BALL_CIRCLE_SCALE 0.16f
#define BALL_DRAW_SCALE 14.0f
#define HUNTER_COUNT 4
#define HUNTER_DAMAGE 8
#define HUNTER_LIFETIME 80
#define HUNTER_SPEED 22.0f
#define HUNTER_FAN 10.0f
#define HUNTER_HOMING 0.35f
#define LIGHT_DAMAGE 4
#define LIGHT_LIFETIME 70
#define LIGHT_SPEED 18.0f
#define LIGHT_HOMING 0.20f
#define LIGHT_SCALE 6.0f
#define TRAIL_LENGTH 15
#define TRAIL_DRAWN 12
#define STREAK_SCALE 0.01f
#define RAYS_MAX 6
#define FHG_LIGHTBALL_BLUE 4
#define FHG_LIGHTBALL_PURPLE 5
#define FHG_SHOCK_ANY_ACTOR 3
#define CHEST_UP 40.0f
#define CHEST_FORWARD 22.0f

// The lance rides the limb transform the Byrna cane was tuned against in NEI, then its own placement on top.
#define LANCE_HELD_SCALE 0.1f
#define LANCE_ROT_X (-53.5f)
#define LANCE_ROT_Y (-7.1f)
#define LANCE_ROT_Z (-50.5f)
#define LANCE_OFF_X 3000.0f
#define LANCE_OFF_Y (-1148.5f)
#define LANCE_OFF_Z (-2049.5f)
#define TRAIL_TIP 8000.0f
#define TRAIL_BASE 2000.0f
#define TRAIL_WIDTH 2.0f
#define GET_ITEM_SCALE 0.012f
#define DEG_TO_BINANG 182.04f

#define GUNLANCE(name) "__OTR__misc/link_animetion/gMonsterHunterRise_Gunlance_" name

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconTridentTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gTridentNameTex";
static const ALIGN_ASSET(2) char sLanceDL[] = "__OTR__objects/object_gnd/gPhantomGanonSkelLimbsLimb_00C610DL_009298";
static const ALIGN_ASSET(2) char sEnergyBallDL[] = "__OTR__objects/object_fhg/gPhantomEnergyBallDL";
static const ALIGN_ASSET(2) char sBigMagicMaterialDL[] = "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightBallMaterialDL";
static const ALIGN_ASSET(2) char sBigMagicBallDL[] = "__OTR__overlays/ovl_Boss_Ganon/gGanondorfSquareDL";
static const ALIGN_ASSET(2) char sBigMagicFlecksDL[] = "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightFlecksDL";
static const ALIGN_ASSET(2) char sBigMagicCircleDL[] = "__OTR__overlays/ovl_Boss_Ganon/gGanondorfBigMagicBGCircleDL";
static const ALIGN_ASSET(2) char sBigMagicDotDL[] = "__OTR__overlays/ovl_Boss_Ganon/gGanondorfDotDL";
static const ALIGN_ASSET(2) char sBigMagicRayDL[] = "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightRayTriDL";
static const ALIGN_ASSET(2) char sDomeTex[] = "__OTR__overlays/ovl_Magic_Fire/sTex";
static const ALIGN_ASSET(2) char sDomeMaterialDL[] = "__OTR__overlays/ovl_Magic_Fire/sMaterialDL";
static const ALIGN_ASSET(2) char sDomeModelDL[] = "__OTR__overlays/ovl_Magic_Fire/sModelDL";

// Newest first: the draw walks the history back from the head.
static const char* const sStreakDLs[TRAIL_DRAWN] = {
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak12DL", "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak11DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak10DL", "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak9DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak8DL",  "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak7DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak6DL",  "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak5DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak4DL",  "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak3DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak2DL",  "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak1DL",
};

// The lance branches to segment 8 twice for Phantom Ganon's per-limb glow; unset, it jumps into garbage.
static Gfx sEmptyDL[] = { gsSPEndDisplayList() };

// Imported by address: compared against the player's actionFunc, which holds the real function.
extern HOST_DATA void Player_Action_808502D0(Player* player, PlayState* play);
extern HOST_DATA void Player_Action_80844AF4(Player* player, PlayState* play);
extern HOST_DATA Vec3f D_80126080;
s32 Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);
s32 func_80842DF4(PlayState* play, Player* player);
void func_80837918(Player* player, s32 quadIndex, u32 dmgFlags);
void func_808377DC(PlayState* play, Player* player);
void func_80090A28(Player* player, Vec3f* vecs);
void func_800906D4(PlayState* play, Player* player, Vec3f* newTipPos);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
void Player_AnimPlayOnce(PlayState* play, Player* player, LinkAnimationHeader* anim);
void Player_RequestRumble(Player* player, s32 sourceStrength, s32 duration, s32 decreaseRate, s32 distSq);
void Player_RequestQuake(PlayState* play, s32 speed, s32 y, s32 countdown);
void Player_SetIntangibility(Player* player, s32 timer);
void Player_UseItem(PlayState* play, Player* player, s32 item);
void Player_SetBootData(PlayState* play, Player* player);
void Player_SetModelsForHoldingShield(Player* player);
void Player_SetModelGroup(Player* player, s32 modelGroup);
s32 Player_ActionToModelGroup(Player* player, s32 itemAction);
s32 Player_GetMovementSpeedAndYaw(Player* player, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                  PlayState* play);
int Player_IsZTargeting(Player* player);
void func_8002F974(Actor* actor, u16 sfxId);

typedef enum {
    CLIP_SLASH_1,
    CLIP_SLASH_2,
    CLIP_SLASH_3,
    CLIP_THRUST,
    CLIP_JUMP_START,
    CLIP_JUMP_FINISH,
    CLIP_CHARGE_1,
    CLIP_CHARGE_2,
    CLIP_CHARGE_3,
    CLIP_CHARGE_RELEASE,
    CLIP_CHARGE_MAX,
    CLIP_CHARGE_HURT,
    CLIP_WALK,
    CLIP_RUN,
    CLIP_DASH_LEGS,
    CLIP_DASH_START,
    CLIP_DASH_POSE,
    CLIP_FLY_START,
    CLIP_FLY_IDLE,
    CLIP_FLY_SHOOT_PRE,
    CLIP_FLY_SHOOT,
    CLIP_FLY_LAUNCH,
    CLIP_FLY_POUND,
    CLIP_FLY_LAND,
    CLIP_POUND_FINISH,
    CLIP_MAX,
} ClipId;

// frames: the resampled length, which is the speed knob. HALF plays twice as fast, NATIVE as authored.
#define HALF 0
#define NATIVE (-1)

typedef struct {
    const char* path;
    s16 frames;
} ClipSource;

static const ClipSource sClipSources[CLIP_MAX] = {
    { GUNLANCE("StationarySingleGunlanceThrust"), 14 },
    { GUNLANCE("ForwardDoubleWeaponTransition"), 24 },
    { GUNLANCE("ForwardRisingMultiHitAerialThrust"), 20 },
    { GUNLANCE("ForwardMultiHitLungingThrust"), 26 },
    { GUNLANCE("ForwardRisingTripleChargedShellingMotion"), 38 },
    { GUNLANCE("ForwardRisingMultiHitAerialThrust"), 24 },
    { GUNLANCE("StationaryGuardIdle_Variant03"), NATIVE },
    { GUNLANCE("StationaryGuardIdle_Variant04"), NATIVE },
    { GUNLANCE("StationaryGuardIdle_Variant05"), NATIVE },
    { GUNLANCE("BackwardMultiHitAerialThrust"), 19 },
    { GUNLANCE("BackwardHighAerialMultiHitSilkbindGunlanceStrike"), NATIVE },
    { GUNLANCE("BackwardRisingAerialMove_Variant06"), HALF },
    // Walk and run share a phase but sample it at 29 and 20 frames; one length for both cuts the cycle short.
    { GUNLANCE("ForwardWeaponRun"), 29 },
    { GUNLANCE("ForwardWeaponRun"), 20 },
    { GUNLANCE("ForwardWeaponRun"), NATIVE },
    { GUNLANCE("BackwardDoubleWeaponTransition_Variant07"), HALF },
    { GUNLANCE("StationaryGuardIdle_Variant14"), NATIVE },
    { GUNLANCE("ForwardDoubleChargedShellingMotion"), HALF },
    { GUNLANCE("StationaryGuardIdle_Variant10"), HALF },
    { GUNLANCE("ForwardMultiHitWeaponTransition"), HALF },
    { GUNLANCE("StationaryTripleWeaponTransition_Variant11"), HALF },
    { GUNLANCE("ForwardSingleChargedShellingMotion"), HALF },
    { GUNLANCE("ForwardRisingTripleChargedShellingMotion"), HALF },
    { GUNLANCE("StationaryRisingAerialMove_Variant25"), HALF },
    { GUNLANCE("ForwardRisingMultiHitAerialThrust"), HALF },
};

typedef struct {
    LinkAnimationHeader header;
    s16* frames;
    bool isLoaded;
} Clip;

static Clip sClips[CLIP_MAX];

// Link's own frame: right +X, up +Y, forward +Z; pitch lays the box toward the floor.
typedef struct {
    f32 right;
    f32 up;
    f32 forward;
    f32 halfWidth;
    f32 halfHeight;
    f32 pitch;
} AttackBox;

static const AttackBox sFrontBox = { 0.0f, 52.0f, 42.0f, 30.0f, 26.0f, 0.0f };
static const AttackBox sLeftBox = { -42.0f, 40.0f, 18.0f, 34.0f, 30.0f, 0.0f };
static const AttackBox sSweepBox = { 0.0f, 38.0f, 52.0f, 56.0f, 46.0f, 0.75f };
static const AttackBox sThrustBox = { 0.0f, 38.0f, 58.0f, 20.0f, 22.0f, 0.0f };
static const AttackBox sGuardBox = { 30.0f, 40.0f, 10.0f, 22.0f, 30.0f, 0.0f };

// Windows in the installed clip's frames, scaled from the source frames NEI's user tuned.
typedef struct {
    ClipId clip;
    s8 row;
    s16 hitStart;
    s16 hitEnd;
    const AttackBox* box;
    bool isGuarded;
} Swing;

static const Swing sComboSwings[COMBO_STEPS] = {
    { CLIP_SLASH_1, PLAYER_MWA_FORWARD_SLASH_1H, 3, 14, &sFrontBox, true },
    { CLIP_SLASH_2, PLAYER_MWA_RIGHT_SLASH_1H, 1, 4, &sLeftBox, true },
    { CLIP_SLASH_3, PLAYER_MWA_FORWARD_COMBO_1H, 2, 14, &sSweepBox, true },
};

static const Swing sThrustSwing = { CLIP_THRUST, PLAYER_MWA_STAB_1H, 9, 26, &sThrustBox, false };
#define THRUST_LUNGE_UNTIL 8

static ColliderQuadInit sAttackQuadInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_QUAD },
    { ELEMTYPE_UNK2, { DMG_SLASH_MASTER, 0x00, MELEE_DAMAGE }, { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

// Vanilla's own shield quad, so the flank bounces exactly what a raised shield bounces.
static ColliderQuadInit sGuardQuadInit = {
    { COLTYPE_METAL, AT_NONE, AC_ON | AC_HARD | AC_TYPE_ENEMY, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_QUAD },
    { ELEMTYPE_UNK2, { 0x00000000, 0x00, 0x00 }, { 0xDFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

typedef enum {
    PROJECTILE_BALL,
    PROJECTILE_HUNTER,
    PROJECTILE_LIGHT,
} ProjectileKind;

// A real weapon bit so boss bumpers accept the hit; the hunters and the max ball add a fixed damage on top.
static ColliderCylinderInit sBallColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_SLASH_MASTER, 0x00, 0x01 }, { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 30, 44, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sHunterColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_SLASH_MASTER, 0x00, HUNTER_DAMAGE }, { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 14, 22, 0, { 0, 0, 0 } },
};

// A light arrow to every damage table, which is what makes it the counter against Ganon-class foes.
static ColliderCylinderInit sLightColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_ARROW_LIGHT, 0x01, LIGHT_DAMAGE }, { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 22, 30, 0, { 0, 0, 0 } },
};

typedef struct {
    Actor actor;
    ColliderCylinder collider;
    Actor* target;
    Vec3f velocity;
    Vec3f trailPos[TRAIL_LENGTH];
    Vec3f trailRot[TRAIL_LENGTH];
    s16 trailIndex;
    s16 lifetime;
    u8 fixedDamage;
    u8 burst;
    bool isColliderReady;
} Projectile;

typedef enum {
    LANCE_SWING,
    LANCE_DASH_START,
    LANCE_DASH_RUN,
    LANCE_FLY_START,
    LANCE_FLY_IDLE,
    LANCE_FLY_SHOOT_PRE,
    LANCE_FLY_SHOOT,
    LANCE_FLY_LAUNCH,
    LANCE_FLY_POUND,
} LanceState;

static const SOHModApi* sApi;
static s16 sProjectileActorId = -1;
static s16 sProjectileGrace;
static Projectile* sLiveProjectiles[16];
static u8 sUpperBodyMap[PLAYER_LIMB_MAX];
static Vec3s sPoseBuffer[PLAYER_LIMB_BUF_COUNT];
static ColliderQuad sAttackQuad;
static ColliderQuad sGuardQuad;
static bool sAreQuadsReady;

static struct {
    u8 state;
    s16 timer;
    const Swing* swing;
    bool isChainBuffered;
    u8 comboStep;
    s16 comboIdle;
    s16 bHold;
    s16 flyHold;
    s16 flyMagicTick;
    f32 prevFrame;
    Actor* launchTarget;
    s16 launchTimer;
    f32 launchPhase;
    s8 launchPing;
    bool hasShot;
    bool isPoundFalling;
    Vec3f lanceTip;
    bool isLanceTipValid;
    s8 hiddenMeleeState;
    bool isMeleeStateHidden;
    bool areBootsHeavy;
} sLance;

static struct {
    f32 prevCharge;
    s16 fullHold;
    s8 level;
    s8 releaseLevel;
    bool wasCharging;
    bool isShieldRaised;
    bool isBallPaid;
    bool isBallArmed;
    f32 prevReleaseFrame;
    s16 goldTimer;
    f32 domeScale;
    s16 domeTimer;
    bool isMagicActive;
    Vec3f magicAnchor;
    f32 magicCircle;
    f32 magicBall;
    f32 magicAlpha;
    s16 magicRays;
    s16 magicTimer;
} sCharge;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(TRIDENT_KEY);
}

// Player_GetMeleeWeaponHeld also counts the Deku Stick and the hammer, which keep their own models.
static bool IsSwordAction(s32 itemAction) {
    return itemAction >= PLAYER_IA_SWORD_MASTER && itemAction <= PLAYER_IA_SWORD_BIGGORON;
}

static bool IsWielding(Player* player) {
    return IsWorn() && IsSwordAction(player->heldItemAction);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECK_ON_GROUND) != 0;
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                                    PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM |
                                    PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_CLIMBING_LADDER |
                                    PLAYER_STATE1_IN_WATER | PLAYER_STATE1_DAMAGED)) &&
           !(player->stateFlags2 & PLAYER_STATE2_DIVING);
}

static bool IsFlightState(u8 state) {
    return state >= LANCE_FLY_START;
}

static bool IsDashState(u8 state) {
    return state == LANCE_DASH_START || state == LANCE_DASH_RUN;
}

// A copy with the root pinned to frame 0, so no clip slides Link on top of the movement OoT owns.
// Nearest-frame resampling on purpose, since interpolating packed angles smears anything crossing the wrap.
static LinkAnimationHeader* LoadClip(ClipId id) {
    Clip* clip = &sClips[id];
    const ClipSource* source = &sClipSources[id];
    LinkAnimationHeader* raw;
    s16* rawFrames;
    s32 rawCount;
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
    if (source->frames == NATIVE) {
        outCount = rawCount;
    } else if (source->frames == HALF) {
        outCount = MAX((rawCount + 1) / 2, 2);
    } else {
        outCount = source->frames;
    }
    clip->frames = (s16*)malloc((size_t)outCount * ANIM_VALUES_PER_FRAME * sizeof(s16));
    if (clip->frames == NULL) {
        return NULL;
    }
    for (s32 frame = 0; frame < outCount; frame++) {
        s32 sourceFrame = MIN(frame * rawCount / outCount, rawCount - 1);
        s16* out = &clip->frames[frame * ANIM_VALUES_PER_FRAME];

        memcpy(out, &rawFrames[sourceFrame * ANIM_VALUES_PER_FRAME], ANIM_VALUES_PER_FRAME * sizeof(s16));
        out[0] = rawFrames[0];
        out[1] = rawFrames[1];
        out[2] = rawFrames[2];
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
    sLance.prevFrame = -1.0f;
}

static bool IsPlayingClip(Player* player, ClipId id) {
    return sClips[id].frames != NULL && player->skelAnime.animation == &sClips[id].header;
}

// A point test drops the mark whenever a resampled clip steps by more than one frame.
static bool HasCrossed(f32 prev, f32 current, f32 mark) {
    return current >= mark && prev < mark;
}

// The pose goes in behind whatever the actions loaded this frame, torso only.
static void HoldUpperPose(PlayState* play, Player* player, ClipId id, u32 frame) {
    LinkAnimationHeader* pose = LoadClip(id);

    if (pose == NULL) {
        return;
    }
    AnimationContext_SetLoadFrame(play, pose, frame % pose->common.frameCount, player->skelAnime.limbCount,
                                  sPoseBuffer);
    AnimationContext_SetCopyTrue(play, player->skelAnime.limbCount, player->skelAnime.jointTable, sPoseBuffer,
                                 sUpperBodyMap);
}

static void InitQuads(PlayState* play, Player* player) {
    if (sAreQuadsReady) {
        return;
    }
    Collider_InitQuad(play, &sAttackQuad);
    Collider_SetQuad(play, &sAttackQuad, &player->actor, &sAttackQuadInit);
    Collider_InitQuad(play, &sGuardQuad);
    Collider_SetQuad(play, &sGuardQuad, &player->actor, &sGuardQuadInit);
    sAreQuadsReady = true;
}

static void PlaceQuad(ColliderQuad* quad, Player* player, const AttackBox* box) {
    static const f32 sCornerX[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
    static const f32 sCornerY[4] = { 1.0f, 1.0f, -1.0f, -1.0f };
    Vec3f corners[4];
    f32 sinY = Math_SinS(player->actor.shape.rot.y);
    f32 cosY = Math_CosS(player->actor.shape.rot.y);

    for (s32 i = 0; i < 4; i++) {
        f32 right = box->right + sCornerX[i] * box->halfWidth;
        f32 up = box->up + sCornerY[i] * box->halfHeight * cosf(box->pitch);
        f32 forward = box->forward + sCornerY[i] * box->halfHeight * sinf(box->pitch);

        corners[i].x = player->actor.world.pos.x + right * cosY + forward * sinY;
        corners[i].y = player->actor.world.pos.y + up;
        corners[i].z = player->actor.world.pos.z + forward * cosY - right * sinY;
    }
    Collider_SetQuadVertices(quad, &corners[0], &corners[1], &corners[2], &corners[3]);
}

// The shaft's own quads are a thin line; each step also gets a box where the strike points, and the chain
// keeps a shield on Link's right the whole time.
static void TickSwingVolumes(PlayState* play, Player* player, const Swing* swing, bool isHitting) {
    InitQuads(play, player);
    if (isHitting) {
        PlaceQuad(&sAttackQuad, player, swing->box);
        sAttackQuad.base.atFlags &= ~AT_HIT;
        CollisionCheck_SetAT(play, &play->colChkCtx, &sAttackQuad.base);
    }
    if (swing->isGuarded) {
        PlaceQuad(&sGuardQuad, player, &sGuardBox);
        CollisionCheck_SetAC(play, &play->colChkCtx, &sGuardQuad.base);
    }
}

static void ArmBlade(Player* player, s8 row, u8 damage) {
    func_80837918(player, 0, DMG_SLASH_MASTER);
    func_80837918(player, 1, DMG_SLASH_MASTER);
    player->meleeWeaponQuads[0].info.toucher.damage = damage;
    player->meleeWeaponQuads[1].info.toucher.damage = damage;
    player->meleeWeaponAnimation = row;
}

static void FaceTarget(Player* player) {
    if (player->focusActor == NULL || player->focusActor->update == NULL) {
        return;
    }
    player->actor.shape.rot.y = Math_Vec3f_Yaw(&player->actor.world.pos, &player->focusActor->world.pos);
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
}

static void LanceAction(Player* player, PlayState* play);

static bool IsInLanceAction(Player* player) {
    return player->actionFunc == LanceAction;
}

// Chain rows morph into each other; a strike out of idle keeps the hard start vanilla gives every attack.
static void StartSwing(PlayState* play, Player* player, const Swing* swing, bool isChained) {
    if (LoadClip(swing->clip) == NULL) {
        return;
    }
    if (!IsInLanceAction(player)) {
        Player_SetupAction(play, player, LanceAction, 0);
    }
    sLance.state = LANCE_SWING;
    sLance.swing = swing;
    sLance.isChainBuffered = false;
    player->meleeWeaponState = 0;
    ArmBlade(player, swing->row, MELEE_DAMAGE);
    FaceTarget(player);
    PlayClip(play, player, swing->clip, false, isChained ? -6.0f : 0.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
}

static void StartComboStep(PlayState* play, Player* player, bool isChained) {
    const Swing* swing = &sComboSwings[sLance.comboStep];

    sLance.comboStep = (sLance.comboStep + 1) % COMBO_STEPS;
    sLance.comboIdle = 0;
    StartSwing(play, player, swing, isChained);
}

static void EndLanceAction(PlayState* play, Player* player) {
    player->meleeWeaponState = 0;
    sLance.swing = NULL;
    sLance.state = LANCE_SWING;
    func_80839FFC(player, play);
}

// Vanilla only opens the spin charge the frame after its own short swing; ours end here, B still down.
static void FinishSwing(PlayState* play, Player* player) {
    player->meleeWeaponState = 0;
    if (sLance.isChainBuffered && sLance.comboStep != 0) {
        StartComboStep(play, player, true);
        return;
    }
    if (sLance.bHold >= B_HOLD_TO_CHARGE) {
        sLance.swing = NULL;
        func_808377DC(play, player);
        return;
    }
    EndLanceAction(play, player);
}

static void UpdateSwing(PlayState* play, Player* player, Input* input) {
    const Swing* swing = sLance.swing;
    f32 frame = player->skelAnime.curFrame;
    bool isHitting = frame >= swing->hitStart && frame <= swing->hitEnd;

    if (swing == &sThrustSwing && frame < THRUST_LUNGE_UNTIL) {
        player->linearVelocity = MAX(player->linearVelocity, STAB_LUNGE_SPEED);
    } else {
        Math_StepToF(&player->linearVelocity, 0.0f, 5.0f);
    }
    player->meleeWeaponState = isHitting ? 1 : 0;
    TickSwingVolumes(play, player, swing, isHitting);
    if (CHECK_BTN_ALL(input->press.button, BTN_B) && frame >= swing->hitStart) {
        sLance.isChainBuffered = true;
    }
    if (LinkAnimation_Update(play, &player->skelAnime)) {
        FinishSwing(play, player);
    }
}

static void ResetFlight(Player* player) {
    player->meleeWeaponState = 0;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->actor.gravity = LAND_GRAVITY;
    sLance.state = LANCE_SWING;
    sLance.launchTarget = NULL;
    sLance.flyHold = 0;
}

static void LandFromFlight(PlayState* play, Player* player) {
    LinkAnimationHeader* land = LoadClip(CLIP_FLY_LAND);

    ResetFlight(player);
    func_80839FFC(player, play);
    if (land != NULL) {
        Player_AnimPlayOnce(play, player, land);
    }
    player->linearVelocity = 0.0f;
    Player_RequestRumble(player, 120, 10, 100, 0);
}

// The ring goes on the floor under Link, large: EffectSsBlast is a flat textured quad and barely reads small.
static void PoundTheGround(PlayState* play, Player* player) {
    Vec3f pos = player->actor.world.pos;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    LinkAnimationHeader* finish = LoadClip(CLIP_POUND_FINISH);

    ResetFlight(player);
    player->linearVelocity = 0.0f;
    pos.y = player->actor.floorHeight + 2.0f;
    EffectSsBlast_SpawnWhiteCustomScale(play, &pos, &zero, &zero, POUND_FX_SCALE, POUND_FX_STEP, 12);
    Player_RequestQuake(play, 32967, 8, 24);
    Player_RequestRumble(player, 255, 30, 200, 0);
    Sfx_PlaySfxCentered(NA_SE_IT_BOMB_EXPLOSION);
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; actor != NULL; actor = actor->next) {
        if (Math_Vec3f_DistXYZ(&pos, &actor->world.pos) <= POUND_RADIUS) {
            actor->colChkInfo.damage = POUND_DAMAGE;
            Actor_ApplyDamage(actor);
            Actor_SetColorFilter(actor, 0x4000, 0xC8, 0x0000, 12);
        }
    }
    func_80839FFC(player, play);
    if (finish != NULL) {
        Player_AnimPlayOnce(play, player, finish);
    }
}

static f32 GetClimbSpeed(Input* input) {
    if (CHECK_BTN_ALL(input->cur.button, BTN_R)) {
        return FLY_CLIMB;
    }
    return CHECK_BTN_ALL(input->cur.button, BTN_L) ? -FLY_CLIMB : 0.0f;
}

// Dust trailing opposite the motion: rising leaves it below, dropping above. Only there to say the air moves.
static void KickUpWind(PlayState* play, Player* player, f32 vy) {
    static Color_RGBA8 prim = { 235, 240, 255, 140 };
    static Color_RGBA8 env = { 130, 160, 200, 0 };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Vec3f pos = player->actor.world.pos;
    Vec3f vel = { Rand_CenteredFloat(1.5f), vy > 0.0f ? -2.0f : 2.0f, Rand_CenteredFloat(1.5f) };

    if (vy == 0.0f || (play->gameplayFrames & 1) != 0) {
        return;
    }
    pos.x += Rand_CenteredFloat(18.0f);
    pos.z += Rand_CenteredFloat(18.0f);
    pos.y += vy > 0.0f ? 2.0f : 46.0f;
    EffectSsDust_Spawn(play, 0, &pos, &vel, &accel, &prim, &env, 60, 12, 8, 0);
}

// Stick to yaw and speed, camera-relative and capped at a walk; standing still with a lock-on faces it.
static void SteerFlight(PlayState* play, Player* player) {
    f32 speed = 0.0f;
    s16 yaw = player->actor.shape.rot.y;

    Player_GetMovementSpeedAndYaw(player, &speed, &yaw, SPEED_MODE_LINEAR, play);
    if (speed > 0.5f) {
        player->linearVelocity = MIN(speed, FLY_SPEED);
        player->yaw = yaw;
        Math_ScaledStepToS(&player->actor.shape.rot.y, yaw, 2500);
        return;
    }
    player->linearVelocity = 0.0f;
    if (player->focusActor != NULL && player->focusActor->update != NULL) {
        Math_ScaledStepToS(&player->actor.shape.rot.y, Actor_WorldYawTowardActor(&player->actor, player->focusActor),
                           2500);
    }
    player->yaw = player->actor.shape.rot.y;
}

static f32 HoverWithStick(PlayState* play, Player* player, Input* input) {
    f32 vy = GetClimbSpeed(input);

    SteerFlight(play, player);
    KickUpWind(play, player, vy);
    return vy;
}

static Actor* FindLaunchTarget(PlayState* play, Player* player) {
    Actor* target = player->focusActor;

    if (target != NULL && target->update != NULL) {
        return target;
    }
    target = Actor_FindNearby(play, &player->actor, -1, ACTORCAT_BOSS, 900.0f);
    if (target != NULL && target->update != NULL) {
        return target;
    }
    target = Actor_FindNearby(play, &player->actor, -1, ACTORCAT_ENEMY, 900.0f);
    return (target != NULL && target->update != NULL) ? target : NULL;
}

// Solved once at entry, vy0 = dy/T + g*T/2, so the parabola lands on the target instead of beelining.
static void AimLaunchArc(Player* player, Actor* target) {
    f32 dx = target->world.pos.x - player->actor.world.pos.x;
    f32 dz = target->world.pos.z - player->actor.world.pos.z;
    f32 dy = (target->world.pos.y + target->focus.pos.y) * 0.5f - player->actor.world.pos.y;
    f32 flight = MAX(sqrtf(SQ(dx) + SQ(dz)) / LAUNCH_SPEED, ARC_MIN_FLIGHT);
    s16 yaw = Actor_WorldYawTowardActor(&player->actor, target);

    player->actor.shape.rot.y = yaw;
    player->yaw = yaw;
    sLance.launchPhase = MAX(dy / flight + ARC_FALL * flight * 0.5f, ARC_MIN_VY);
}

static void EnterFlightState(PlayState* play, Player* player, u8 state, ClipId clip, bool isLooping) {
    sLance.state = state;
    sLance.timer = 0;
    PlayClip(play, player, clip, isLooping, -4.0f);
}

static void StartLaunch(PlayState* play, Player* player) {
    sLance.launchTarget = FindLaunchTarget(play, player);
    sLance.launchTimer = 0;
    sLance.launchPhase = LAUNCH_LOOP_A;
    sLance.launchPing = 1;
    player->actor.shape.rot.x = 0;
    player->actor.shape.rot.z = 0;
    if (sLance.launchTarget != NULL) {
        AimLaunchArc(player, sLance.launchTarget);
    }
    EnterFlightState(play, player, LANCE_FLY_LAUNCH, CLIP_FLY_LAUNCH, false);
}

// R+B is tested before B: R is also the climb, so the slam must win or tapping B while rising would shoot.
static f32 UpdateHover(PlayState* play, Player* player, Input* input) {
    bool isBPressed = CHECK_BTN_ALL(input->press.button, BTN_B);

    if (isBPressed && CHECK_BTN_ALL(input->cur.button, BTN_R)) {
        sLance.isPoundFalling = false;
        player->linearVelocity = 0.0f;
        EnterFlightState(play, player, LANCE_FLY_POUND, CLIP_FLY_POUND, false);
        Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
        Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
        return 0.0f;
    }
    if (isBPressed) {
        EnterFlightState(play, player, LANCE_FLY_SHOOT_PRE, CLIP_FLY_SHOOT_PRE, false);
        return 0.0f;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        StartLaunch(play, player);
        return 0.0f;
    }
    return HoverWithStick(play, player, input);
}

static void SpawnProjectile(PlayState* play, Vec3f* pos, ProjectileKind kind, Vec3f* velocity, Actor* target);

static f32 UpdateShoot(PlayState* play, Player* player, Input* input, bool isClipDone) {
    Vec3f velocity = { Math_SinS(player->actor.shape.rot.y) * LIGHT_SPEED, 0.0f,
                       Math_CosS(player->actor.shape.rot.y) * LIGHT_SPEED };
    f32 vy = HoverWithStick(play, player, input);

    if (!sLance.hasShot && HasCrossed(sLance.prevFrame, player->skelAnime.curFrame, SHOOT_FRAME)) {
        sLance.hasShot = true;
        SpawnProjectile(play, &sLance.lanceTip, PROJECTILE_LIGHT, &velocity, NULL);
        Sfx_PlaySfxCentered(NA_SE_EN_FANTOM_MASIC1);
    }
    if (isClipDone) {
        EnterFlightState(play, player, LANCE_FLY_IDLE, CLIP_FLY_IDLE, true);
    }
    return vy;
}

// Without a target the launch lasts as long as A is held, the clip ping-ponging across 10..16; curFrame is
// written after the update, so it is what the next frame reads.
static f32 UpdateStraightLaunch(PlayState* play, Player* player, Input* input, bool isClipDone) {
    player->yaw = player->actor.shape.rot.y;
    player->linearVelocity = LAUNCH_SPEED;
    if (CHECK_BTN_ALL(input->cur.button, BTN_A)) {
        sLance.launchPhase += sLance.launchPing;
        if (sLance.launchPhase >= LAUNCH_LOOP_B) {
            sLance.launchPhase = LAUNCH_LOOP_B;
            sLance.launchPing = -1;
        } else if (sLance.launchPhase <= LAUNCH_LOOP_A) {
            sLance.launchPhase = LAUNCH_LOOP_A;
            sLance.launchPing = 1;
        }
        player->skelAnime.curFrame = sLance.launchPhase;
    } else if (isClipDone) {
        player->meleeWeaponState = 0;
        player->linearVelocity = 0.0f;
        EnterFlightState(play, player, LANCE_FLY_IDLE, CLIP_FLY_IDLE, true);
    }
    return 0.0f;
}

// With a target the arc is committed: letting go halfway would drop Link out of the sky.
static f32 UpdateArcLaunch(PlayState* play, Player* player, Actor* target) {
    Vec3f to = { target->world.pos.x - player->actor.world.pos.x,
                 (target->world.pos.y + target->focus.pos.y) * 0.5f - player->actor.world.pos.y,
                 target->world.pos.z - player->actor.world.pos.z };
    f32 distance = sqrtf(SQ(to.x) + SQ(to.y) + SQ(to.z));
    bool isBladeHit = (player->meleeWeaponQuads[0].base.atFlags & AT_HIT) ||
                      (player->meleeWeaponQuads[1].base.atFlags & AT_HIT);

    if (distance > 1.0f) {
        player->yaw = Math_Atan2S(to.z, to.x);
        player->actor.shape.rot.y = player->yaw;
        player->linearVelocity = LAUNCH_SPEED;
    }
    if (distance > LAUNCH_ARRIVE_DIST && sLance.launchTimer < LAUNCH_MAX_FRAMES && !isBladeHit) {
        return sLance.launchPhase - sLance.launchTimer * ARC_FALL;
    }
    if (!isBladeHit && distance <= LAUNCH_ARRIVE_DIST) {
        target->colChkInfo.damage = MELEE_DAMAGE;
        Actor_ApplyDamage(target);
        Actor_SetColorFilter(target, 0x4000, 0xC8, 0x0000, 8);
        Sfx_PlaySfxCentered(NA_SE_IT_SWORD_STRIKE_HARD);
    }
    player->meleeWeaponState = 0;
    player->linearVelocity = 0.0f;
    sLance.launchTarget = NULL;
    EnterFlightState(play, player, LANCE_FLY_IDLE, CLIP_FLY_IDLE, true);
    return 0.0f;
}

static f32 UpdateLaunch(PlayState* play, Player* player, Input* input, bool isClipDone) {
    Actor* target = sLance.launchTarget;

    if (++sLance.launchTimer == 1) {
        player->meleeWeaponState = 1;
        ArmBlade(player, PLAYER_MWA_STAB_1H, MELEE_DAMAGE);
        Sfx_PlaySfxCentered(NA_SE_EN_FANTOM_MASIC2);
    }
    if (target == NULL || target->update == NULL) {
        sLance.launchTarget = NULL;
        return UpdateStraightLaunch(play, player, input, isClipDone);
    }
    return UpdateArcLaunch(play, player, target);
}

static f32 UpdatePound(Player* player, bool isClipDone) {
    player->linearVelocity = 0.0f;
    if (sLance.isPoundFalling) {
        return POUND_FALL;
    }
    if (isClipDone) {
        sLance.isPoundFalling = true;
        player->meleeWeaponState = 1;
        ArmBlade(player, PLAYER_MWA_STAB_1H, MELEE_DAMAGE);
    }
    return 0.0f;
}

static bool IsCapeOwned(void) {
    return CustomEquipRegistry_IsOwned(MAGIC_CAPE_KEY);
}

// Four magic a second, free with the Magic Cape; running dry brings Link down.
static bool PayForFlight(PlayState* play) {
    if (IsCapeOwned() || ++sLance.flyMagicTick < FLY_MAGIC_TICK) {
        return true;
    }
    sLance.flyMagicTick = 0;
    return Magic_RequestChange(play, FLY_MAGIC_COST, MAGIC_CONSUME_NOW);
}

// The arc ends where its own case says; the slam's touchdown is the move itself.
static void TouchDown(PlayState* play, Player* player, f32 vy) {
    if (sLance.state == LANCE_FLY_START || vy > 0.0f || !IsGrounded(player)) {
        return;
    }
    if (sLance.state == LANCE_FLY_LAUNCH && sLance.launchTarget != NULL) {
        return;
    }
    if (sLance.state == LANCE_FLY_POUND) {
        PoundTheGround(play, player);
    } else {
        LandFromFlight(play, player);
    }
}

// The flight hum is continuous: asked for every frame, it stops on its own the frame this stops running.
static void UpdateFlight(PlayState* play, Player* player, Input* input) {
    bool isClipDone;
    f32 vy = 0.0f;

    if (!PayForFlight(play)) {
        ResetFlight(player);
        func_80839FFC(player, play);
        return;
    }
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    player->actor.gravity = 0.0f;
    player->actor.minVelocityY = POUND_FALL;
    player->fallStartHeight = (s16)player->actor.world.pos.y;
    player->fallDistance = 0;
    func_8002F974(&player->actor, NA_SE_EN_FANTOM_FLOAT - SFX_FLAG);
    sLance.timer++;
    isClipDone = LinkAnimation_Update(play, &player->skelAnime);
    switch (sLance.state) {
        case LANCE_FLY_START:
            vy = FLY_TAKEOFF_VY;
            player->linearVelocity = 0.0f;
            if ((play->gameplayFrames & 1) == 0) {
                Vec3f zero = { 0.0f, 0.0f, 0.0f };

                EffectSsFhgFlash_SpawnLightBall(play, &player->actor.world.pos, &zero, &zero, 110,
                                                FHG_LIGHTBALL_BLUE);
            }
            if (isClipDone) {
                EnterFlightState(play, player, LANCE_FLY_IDLE, CLIP_FLY_IDLE, true);
            }
            break;
        case LANCE_FLY_IDLE:
            vy = UpdateHover(play, player, input);
            break;
        case LANCE_FLY_SHOOT_PRE:
            vy = HoverWithStick(play, player, input);
            if (isClipDone) {
                sLance.hasShot = false;
                EnterFlightState(play, player, LANCE_FLY_SHOOT, CLIP_FLY_SHOOT, false);
            }
            break;
        case LANCE_FLY_SHOOT:
            vy = UpdateShoot(play, player, input, isClipDone);
            break;
        case LANCE_FLY_LAUNCH:
            vy = UpdateLaunch(play, player, input, isClipDone);
            break;
        case LANCE_FLY_POUND:
            vy = UpdatePound(player, isClipDone);
            break;
        default:
            ResetFlight(player);
            func_80839FFC(player, play);
            return;
    }
    sLance.prevFrame = player->skelAnime.curFrame;
    player->actor.velocity.y = vy;
    player->actor.speedXZ = player->linearVelocity;
    TouchDown(play, player, vy);
}

static void TakeOff(PlayState* play, Player* player) {
    if (LoadClip(CLIP_FLY_START) == NULL) {
        return;
    }
    Player_SetupAction(play, player, LanceAction, 0);
    sLance.swing = NULL;
    sLance.flyMagicTick = 0;
    sLance.launchTarget = NULL;
    player->meleeWeaponState = 0;
    player->linearVelocity = 0.0f;
    player->actor.velocity.y = 6.0f;
    player->actor.bgCheckFlags &= ~BGCHECK_ON_GROUND;
    EnterFlightState(play, player, LANCE_FLY_START, CLIP_FLY_START, false);
}

// The run cycle on the legs at its native length, the guard stance over the torso; stick X steers.
static void UpdateDash(PlayState* play, Player* player, Input* input) {
    f32 speed = 0.0f;
    s16 yaw = player->actor.shape.rot.y;

    if (!CHECK_BTN_ALL(input->cur.button, BTN_R)) {
        EndLanceAction(play, player);
        return;
    }
    if (sLance.state == LANCE_DASH_START) {
        player->linearVelocity = 0.0f;
        if (LinkAnimation_Update(play, &player->skelAnime)) {
            sLance.state = LANCE_DASH_RUN;
            sLance.timer = 0;
            player->meleeWeaponState = 1;
            ArmBlade(player, PLAYER_MWA_STAB_1H, DASH_DAMAGE);
            PlayClip(play, player, CLIP_DASH_LEGS, true, -6.0f);
            Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
        }
        return;
    }
    if (ABS(input->rel.stick_x) > 10) {
        player->actor.shape.rot.y -= (s16)(input->rel.stick_x * 5.0f);
    }
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
    Player_GetMovementSpeedAndYaw(player, &speed, &yaw, SPEED_MODE_LINEAR, play);
    player->linearVelocity = MAX(speed, DASH_BASE_SPEED) * DASH_SPEED_SCALE;
    player->actor.speedXZ = player->linearVelocity;
    LinkAnimation_Update(play, &player->skelAnime);
    HoldUpperPose(play, player, CLIP_DASH_POSE, (u32)play->gameplayFrames);
    func_8002F974(&player->actor, NA_SE_PL_WALK_GROUND - SFX_FLAG);
}

static void StartDash(PlayState* play, Player* player) {
    if (LoadClip(CLIP_DASH_START) == NULL) {
        return;
    }
    Player_SetupAction(play, player, LanceAction, 0);
    sLance.swing = NULL;
    sLance.state = LANCE_DASH_START;
    player->linearVelocity = 0.0f;
    player->yaw = player->actor.shape.rot.y;
    PlayClip(play, player, CLIP_DASH_START, false, -6.0f);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
}

// Its own action, so damage, water or a cutscene replace it the way they replace any vanilla one.
static void LanceAction(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];

    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    if (IsFlightState(sLance.state)) {
        UpdateFlight(play, player, input);
        return;
    }
    if (IsDashState(sLance.state)) {
        UpdateDash(play, player, input);
        return;
    }
    if (func_80842DF4(play, player)) {
        sLance.swing = NULL;
        return;
    }
    if (sLance.swing == NULL) {
        EndLanceAction(play, player);
        return;
    }
    UpdateSwing(play, player, input);
}

// Magic is folded into a consume already running: the spin's own cost has magicState busy by now, and a
// second request would be refused.
static void PayForBall(PlayState* play) {
    s16 cost = BALL_MAGIC_COST;

    if (!gSaveContext.isMagicAcquired) {
        return;
    }
    if (gSaveContext.magicState != MAGIC_STATE_IDLE) {
        gSaveContext.magicTarget = MAX(gSaveContext.magicTarget - cost, 0);
        return;
    }
    cost = MIN(cost, gSaveContext.magic);
    if (cost > 0) {
        Magic_RequestChange(play, cost, MAGIC_CONSUME_NOW);
    }
}

static bool IsRealBoss(Actor* actor) {
    return actor->id != ACTOR_BOSS_VA || actor->params == -1;
}

static bool IsTargetUsable(Actor* target) {
    return target != NULL && target->update != NULL && target->colChkInfo.health > 0;
}

// Anything on screen is fair game at any range; anything off it has to be close.
static bool IsReachable(Actor* actor, Vec3f* origin, f32 range) {
    return actor->isDrawn || Math_Vec3f_DistXYZ(origin, &actor->world.pos) <= range;
}

static bool IsTaken(Actor* actor, Actor** taken, s32 takenCount) {
    for (s32 i = 0; i < takenCount; i++) {
        if (taken[i] == actor) {
            return true;
        }
    }
    return false;
}

// Bosses before enemies, and only the body of a split boss: Barinade's stumps and tentacles are bosses too.
static Actor* FindNearestTarget(PlayState* play, Vec3f* origin, Actor** taken, s32 takenCount, bool isBossOnly) {
    static const u8 sCategories[] = { ACTORCAT_BOSS, ACTORCAT_ENEMY };
    s32 categoryCount = isBossOnly ? 1 : 2;

    for (s32 c = 0; c < categoryCount; c++) {
        Actor* best = NULL;
        f32 bestDistance = 1.0e9f;

        for (Actor* actor = play->actorCtx.actorLists[sCategories[c]].head; actor != NULL; actor = actor->next) {
            f32 distance = Math_Vec3f_DistXYZ(origin, &actor->world.pos);

            if (!IsTargetUsable(actor) || !IsRealBoss(actor) || !IsReachable(actor, origin, BALL_SEEK_RANGE) ||
                IsTaken(actor, taken, takenCount) || distance >= bestDistance) {
                continue;
            }
            best = actor;
            bestDistance = distance;
        }
        if (best != NULL) {
            return best;
        }
    }
    return NULL;
}

static Actor* AcquireTarget(PlayState* play, Vec3f* origin) {
    Player* player = GET_PLAYER(play);

    if (IsTargetUsable(player->focusActor) && IsRealBoss(player->focusActor)) {
        return player->focusActor;
    }
    return FindNearestTarget(play, origin, NULL, 0, false);
}

static void SpawnProjectile(PlayState* play, Vec3f* pos, ProjectileKind kind, Vec3f* velocity, Actor* target) {
    Projectile* projectile;

    if (sProjectileActorId < 0) {
        return;
    }
    projectile = (Projectile*)Actor_Spawn(&play->actorCtx, play, sProjectileActorId, pos->x, pos->y, pos->z, 0, 0, 0,
                                          kind);
    if (projectile == NULL) {
        return;
    }
    projectile->velocity = *velocity;
    projectile->target = target != NULL ? target : AcquireTarget(play, pos);
}

// Four seekers, each after a different nearest enemy; with fewer enemies than seekers the spares double up.
static void SpawnHunters(PlayState* play, Vec3f* origin) {
    Actor* taken[HUNTER_COUNT];
    s32 takenCount = 0;

    for (s32 i = 0; i < HUNTER_COUNT; i++) {
        f32 angle = i * (2.0f * M_PI / HUNTER_COUNT);
        Vec3f velocity = { cosf(angle) * HUNTER_FAN, 4.0f, sinf(angle) * HUNTER_FAN };
        Actor* target = FindNearestTarget(play, origin, taken, takenCount, false);

        if (target == NULL && takenCount > 0) {
            takenCount = 0;
            target = FindNearestTarget(play, origin, taken, takenCount, false);
        }
        SpawnProjectile(play, origin, PROJECTILE_HUNTER, &velocity, target);
        if (target != NULL) {
            taken[takenCount++] = target;
        }
    }
}

static void BurstBall(PlayState* play, Projectile* ball) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    sProjectileGrace = PROJECTILE_GRACE;
    EffectSsFhgFlash_SpawnShock(play, &ball->actor, &ball->actor.world.pos, 200, FHG_SHOCK_ANY_ACTOR);
    for (s32 i = 0; i < 8; i++) {
        Vec3f velocity = { Rand_CenteredFloat(12.0f), Rand_ZeroFloat(8.0f) + 2.0f, Rand_CenteredFloat(12.0f) };

        EffectSsFhgFlash_SpawnLightBall(play, &ball->actor.world.pos, &velocity, &zero,
                                        (s16)(Rand_ZeroOne() * 80.0f) + 150, FHG_LIGHTBALL_PURPLE);
    }
    SpawnHunters(play, &ball->actor.world.pos);
    ball->burst = BALL_BURST_FRAMES;
}

// The ball ends a real boss outright: health is zeroed rather than damage piled on, since bosses cap what a
// hit takes, and the hit still lands so the boss plays its own death.
static void ResolveProjectileHit(PlayState* play, Projectile* projectile) {
    Actor* hit = projectile->collider.base.at;

    projectile->collider.base.atFlags &= ~AT_HIT;
    if (projectile->actor.params != PROJECTILE_BALL) {
        Actor_Kill(&projectile->actor);
        return;
    }
    if (projectile->fixedDamage != 0 && hit != NULL && hit->category == ACTORCAT_BOSS && IsRealBoss(hit)) {
        hit->colChkInfo.health = 0;
    }
    BurstBall(play, projectile);
}

static void SteerProjectile(Projectile* projectile, f32 speed, f32 homing) {
    Actor* target = projectile->target;
    Vec3f to;
    f32 length;

    if (!IsTargetUsable(target)) {
        return;
    }
    to.x = target->world.pos.x - projectile->actor.world.pos.x;
    to.y = (target->world.pos.y + target->focus.pos.y) * 0.5f - projectile->actor.world.pos.y;
    to.z = target->world.pos.z - projectile->actor.world.pos.z;
    length = sqrtf(SQ(to.x) + SQ(to.y) + SQ(to.z));
    if (length < 1.0f) {
        return;
    }
    projectile->velocity.x += (to.x / length * speed - projectile->velocity.x) * homing;
    projectile->velocity.y += (to.y / length * speed - projectile->velocity.y) * homing;
    projectile->velocity.z += (to.z / length * speed - projectile->velocity.z) * homing;
}

// Sampled after the move: position plus the heading it travels on, which orients each streak segment.
static void RecordTrail(Projectile* projectile) {
    f32 horizontal = sqrtf(SQ(projectile->velocity.x) + SQ(projectile->velocity.z));

    projectile->trailIndex = (projectile->trailIndex + 1) % TRAIL_LENGTH;
    projectile->trailPos[projectile->trailIndex] = projectile->actor.world.pos;
    projectile->trailRot[projectile->trailIndex].y = atan2f(projectile->velocity.x, projectile->velocity.z);
    projectile->trailRot[projectile->trailIndex].x = atan2f(projectile->velocity.y, horizontal);
}

static void TrackProjectile(Projectile* projectile, bool isLive) {
    Projectile* wanted = isLive ? NULL : projectile;
    Projectile* stored = isLive ? projectile : NULL;

    for (s32 i = 0; i < ARRAY_COUNT(sLiveProjectiles); i++) {
        if (sLiveProjectiles[i] == wanted) {
            sLiveProjectiles[i] = stored;
            return;
        }
    }
}

// A damage table would weigh a seeker or the max ball like any sword slash; theirs is exact.
static void KeepFixedDamage(Actor* victim, ColliderInfo* attack, float* damage) {
    for (s32 i = 0; i < ARRAY_COUNT(sLiveProjectiles); i++) {
        Projectile* projectile = sLiveProjectiles[i];

        if (projectile == NULL || attack != &projectile->collider.info) {
            continue;
        }
        if (projectile->actor.params == PROJECTILE_HUNTER || projectile->fixedDamage != 0) {
            *damage = attack->toucher.damage;
        }
        return;
    }
}

static void ProjectileInit(Actor* actor, PlayState* play) {
    static ColliderCylinderInit* const sInits[] = { &sBallColliderInit, &sHunterColliderInit, &sLightColliderInit };
    static const s16 sLifetimes[] = { BALL_LIFETIME, HUNTER_LIFETIME, LIGHT_LIFETIME };
    Projectile* projectile = (Projectile*)actor;

    if (actor->params < PROJECTILE_BALL || actor->params > PROJECTILE_LIGHT) {
        Actor_Kill(actor);
        return;
    }
    Collider_InitCylinder(play, &projectile->collider);
    Collider_SetCylinder(play, &projectile->collider, actor, sInits[actor->params]);
    projectile->isColliderReady = true;
    TrackProjectile(projectile, true);
    projectile->lifetime = sLifetimes[actor->params];
    for (s32 i = 0; i < TRAIL_LENGTH; i++) {
        projectile->trailPos[i] = actor->world.pos;
    }
    Actor_SetScale(actor, actor->params == PROJECTILE_LIGHT ? LIGHT_SCALE : STREAK_SCALE);
}

static void ProjectileDestroy(Actor* actor, PlayState* play) {
    Projectile* projectile = (Projectile*)actor;

    TrackProjectile(projectile, false);
    if (projectile->isColliderReady) {
        Collider_DestroyCylinder(play, &projectile->collider);
    }
}

static void ProjectileUpdate(Actor* actor, PlayState* play) {
    static const f32 sSpeeds[] = { BALL_SPEED, HUNTER_SPEED, LIGHT_SPEED };
    static const f32 sHomings[] = { BALL_HOMING, HUNTER_HOMING, LIGHT_HOMING };
    Projectile* projectile = (Projectile*)actor;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    if (projectile->burst > 0) {
        if (--projectile->burst == 0) {
            Actor_Kill(actor);
        }
        return;
    }
    if (projectile->fixedDamage != 0) {
        projectile->collider.info.toucher.damage = projectile->fixedDamage;
    }
    if (!IsTargetUsable(projectile->target)) {
        projectile->target = AcquireTarget(play, &actor->world.pos);
    }
    SteerProjectile(projectile, sSpeeds[actor->params], sHomings[actor->params]);
    actor->world.pos.x += projectile->velocity.x;
    actor->world.pos.y += projectile->velocity.y;
    actor->world.pos.z += projectile->velocity.z;
    actor->shape.rot.z += actor->params == PROJECTILE_BALL ? 0x0C00 : 0x1000;
    if (actor->params == PROJECTILE_HUNTER) {
        RecordTrail(projectile);
    } else if ((projectile->lifetime & 3) == 0) {
        EffectSsFhgFlash_SpawnLightBall(play, &actor->world.pos, &zero, &zero, 120,
                                        actor->params == PROJECTILE_BALL ? FHG_LIGHTBALL_PURPLE : FHG_LIGHTBALL_BLUE);
    }
    Collider_UpdateCylinder(actor, &projectile->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &projectile->collider.base);
    if (projectile->collider.base.atFlags & AT_HIT) {
        ResolveProjectileHit(play, projectile);
        return;
    }
    if (projectile->lifetime-- > 0) {
        return;
    }
    // A ball that expires still bursts, so a miss reads as a miss rather than a blink.
    if (actor->params == PROJECTILE_BALL) {
        BurstBall(play, projectile);
    } else {
        Actor_Kill(actor);
    }
}

// Ganondorf's big magic: flecks, background circle, dot, the light ball and a fan of rays. The ball Link charges
// and the projectile it becomes both draw through here, so the release reads as that ball leaving.
static void DrawBigMagic(PlayState* play, Vec3f* pos, f32 circleScale, f32 ballScale, f32 alpha, s32 rays,
                         f32 spin) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u32 frame = play->gameplayFrames;
    u8 opacity = (u8)CLAMP(alpha, 0.0f, 255.0f);

    if (circleScale <= 0.001f) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Xlu(gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 170, opacity);
    gDPSetEnvColor(POLY_XLU_DISP++, 200, 255, 0, 128);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, frame * -2, 0, 0x40, 0x40, 1, 0, frame * 0xA, 0x40, 0x40, -2,
                                             0, 0, 0xA));
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(circleScale, circleScale, circleScale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicFlecksDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 0, 100, opacity);
    gSPSegment(POLY_XLU_DISP++, 0x09,
               (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 0x20, 0x20, 1, 0, frame * -4, 0x20, 0x20, 0, 0, 0, -4));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicCircleDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 150, 170, 0, opacity);
    gSPSegment(POLY_XLU_DISP++, 0x0A,
               (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 0x20, 0x20, 1, frame * 2, frame * -0x14, 0x40, 0x40, 0,
                                             0, 2, -0x14));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicDotDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 100, 0);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicMaterialDL);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(ballScale, ballScale, ballScale, MTXMODE_APPLY);
    Matrix_RotateZ(spin, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicBallDL);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_RotateY(frame * 10.0f / 1000.0f, MTXMODE_APPLY);
    gDPSetEnvColor(POLY_XLU_DISP++, 200, 255, 0, 0);
    for (s32 i = 0; i < MIN(rays, RAYS_MAX); i++) {
        f32 angle = i * (M_PI * 2.0f / RAYS_MAX);

        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 200);
        Matrix_Push();
        Matrix_RotateY(angle, MTXMODE_APPLY);
        Matrix_RotateX((i & 1) ? 0.6f : -0.6f, MTXMODE_APPLY);
        Matrix_RotateZ(angle * 0.5f, MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, ballScale * 1.6f, MTXMODE_APPLY);
        Matrix_Scale(ballScale * 0.115f, ballScale * 0.115f, ballScale * 0.032f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicRayDL);
        Matrix_Pop();
    }
    CLOSE_DISPS(gfxCtx);
}

// The seekers carry the lit streak of the balls Ganondorf's light arrow turns back: twelve tapering quads along
// the last twelve samples, their matrices in segment 0x0D where the streak lists index them.
static void DrawStreak(Projectile* projectile, PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Mtx* matrices = Graph_Alloc(gfxCtx, TRAIL_DRAWN * sizeof(Mtx));
    u8 alpha = projectile->lifetime >= 8 ? 255 : (u8)(projectile->lifetime * 255 / 8);

    if (matrices == NULL) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Xlu(gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 255, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 150, 255, 0, 128);
    gSPSegment(POLY_XLU_DISP++, 0x0D, (uintptr_t)matrices);
    for (s32 i = 0; i < TRAIL_DRAWN; i++) {
        s32 sample = (projectile->trailIndex - i + TRAIL_LENGTH) % TRAIL_LENGTH;

        Matrix_Translate(projectile->trailPos[sample].x, projectile->trailPos[sample].y,
                         projectile->trailPos[sample].z, MTXMODE_NEW);
        Matrix_RotateY(projectile->trailRot[sample].y, MTXMODE_APPLY);
        Matrix_RotateX(-projectile->trailRot[sample].x, MTXMODE_APPLY);
        Matrix_Scale(STREAK_SCALE, STREAK_SCALE, STREAK_SCALE, MTXMODE_APPLY);
        Matrix_RotateY(M_PI / 2.0f, MTXMODE_APPLY);
        MATRIX_TOMTX(&matrices[i]);
        gSPMatrix(POLY_XLU_DISP++, &matrices[i], G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sStreakDLs[i]);
    }
    Matrix_Translate(projectile->actor.world.pos.x, projectile->actor.world.pos.y, projectile->actor.world.pos.z,
                     MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(6.0f, 6.0f, 6.0f, MTXMODE_APPLY);
    Matrix_RotateZ(projectile->actor.shape.rot.z / (f32)0x8000 * M_PI, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicMaterialDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicBallDL);
    CLOSE_DISPS(gfxCtx);
}

// Phantom Ganon's own energy ball, drawn the way EnFhgFire_Draw does it.
static void DrawLightBall(Projectile* projectile, PlayState* play) {
    Actor* actor = &projectile->actor;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(actor->world.pos.x, actor->world.pos.y, actor->world.pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(actor->scale.x, actor->scale.y, actor->scale.z, MTXMODE_APPLY);
    Matrix_RotateZ(actor->shape.rot.z / (f32)0x8000 * M_PI, MTXMODE_APPLY);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 165, 255, 75, 0);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sEnergyBallDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The burst swells to BALL_BURST_PEAK over its first BALL_BURST_RISE, then collapses while it fades.
static void DrawBall(Projectile* projectile, PlayState* play) {
    f32 size = 1.0f;
    f32 alpha = 255.0f;

    if (projectile->burst > 0) {
        f32 t = 1.0f - (f32)projectile->burst / BALL_BURST_FRAMES;

        size = t < BALL_BURST_RISE ? 1.0f + (BALL_BURST_PEAK - 1.0f) * t / BALL_BURST_RISE
                                   : BALL_BURST_PEAK * (1.0f - (t - BALL_BURST_RISE) / (1.0f - BALL_BURST_RISE));
        size = MAX(size, 0.0f);
        alpha = 255.0f * (1.0f - t);
    }
    DrawBigMagic(play, &projectile->actor.world.pos, BALL_CIRCLE_SCALE * size, BALL_DRAW_SCALE * size, alpha,
                 RAYS_MAX, projectile->actor.shape.rot.z / (f32)0x8000 * M_PI);
}

static void ProjectileDraw(Actor* actor, PlayState* play) {
    Projectile* projectile = (Projectile*)actor;

    switch (actor->params) {
        case PROJECTILE_BALL:
            DrawBall(projectile, play);
            break;
        case PROJECTILE_HUNTER:
            DrawStreak(projectile, play);
            break;
        case PROJECTILE_LIGHT:
            DrawLightBall(projectile, play);
            break;
        default:
            break;
    }
}

// Against a boss in range the ball goes after it for a fixed super hit; anything else, it breaks on the very
// next frame and the burst is the payload.
static void ReleaseMaxBall(PlayState* play, Player* player) {
    Vec3f pos = sCharge.isMagicActive ? sCharge.magicAnchor : sLance.lanceTip;
    Vec3f velocity = { Math_SinS(player->actor.shape.rot.y) * BALL_SPEED, 0.0f,
                       Math_CosS(player->actor.shape.rot.y) * BALL_SPEED };
    Actor* boss = FindNearestTarget(play, &pos, NULL, 0, true);
    Projectile* ball;

    if (sProjectileActorId < 0) {
        return;
    }
    ball = (Projectile*)Actor_Spawn(&play->actorCtx, play, sProjectileActorId, pos.x, pos.y, pos.z, 0, 0, 0,
                                    PROJECTILE_BALL);
    if (ball != NULL) {
        ball->velocity = velocity;
        ball->fixedDamage = MAX_BALL_DAMAGE;
        ball->target = boss;
        if (boss == NULL) {
            ball->lifetime = 1;
        }
    }
    Sfx_PlaySfxCentered(NA_SE_EN_GANON_THROW_MASIC);
    Player_RequestRumble(player, 255, 25, 150, 0);
    sCharge.isMagicActive = false;
    sCharge.magicCircle = 0.0f;
    sCharge.magicBall = 0.0f;
    sCharge.magicAlpha = 0.0f;
    sCharge.magicRays = 0;
}

// The first frames are golden and untouchable: intangible, not invulnerable, since a knockback would tear the
// release in half. From frame 25 the clip runs three times as fast, and the ball leaves at frame 64.
static void TickMaxRelease(PlayState* play, Player* player) {
    f32 frame = player->skelAnime.curFrame;
    f32 prev = sCharge.prevReleaseFrame;
    f32 last = Animation_GetLastFrame(&sClips[CLIP_CHARGE_MAX].header);
    f32 burstMark = MIN((f32)MAX_BURST_FRAME, last - 1.0f);

    sCharge.prevReleaseFrame = frame;
    if (frame <= MAX_IMMUNE_LAST) {
        sCharge.goldTimer = GOLD_FRAMES;
        Player_SetIntangibility(player, 20);
    }
    if (frame >= MAX_FAST_FROM) {
        player->skelAnime.playSpeed = MAX_FAST_SPEED;
    }
    if (!sCharge.isBallPaid) {
        PayForBall(play);
        sCharge.isBallPaid = true;
        sCharge.isBallArmed = true;
        player->linearVelocity = MAX_RECOIL;
        player->yaw = player->actor.shape.rot.y;
    }
    if (sCharge.isBallArmed && HasCrossed(prev, frame, burstMark)) {
        sCharge.isBallArmed = false;
        ReleaseMaxBall(play, player);
    }
}

static void StartChargeRelease(PlayState* play, Player* player, ClipId clip) {
    LinkAnimationHeader* anim = LoadClip(clip);

    if (anim == NULL) {
        return;
    }
    sCharge.isBallPaid = false;
    sCharge.isBallArmed = false;
    sCharge.prevReleaseFrame = -1.0f;
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), ANIMMODE_ONCE,
                         0.0f);
    if (clip == CLIP_CHARGE_RELEASE) {
        sCharge.domeScale = sCharge.releaseLevel >= 2 ? DOME_SCALE_2 : DOME_SCALE_1;
        sCharge.domeTimer = DOME_FRAMES;
        Sfx_PlaySfxCentered(NA_SE_IT_BOMB_EXPLOSION);
        Player_RequestRumble(player, sCharge.releaseLevel >= 2 ? 220 : 140, 15, 120, 0);
    }
}

// A heavy weapon hops short and low, and the landing lunges a step with a quake.
static void ReskinJumpSlash(PlayState* play, Player* player, bool isLanding) {
    LinkAnimationHeader* anim = LoadClip(isLanding ? CLIP_JUMP_FINISH : CLIP_JUMP_START);

    if (anim == NULL || player->skelAnime.animation == anim) {
        return;
    }
    if (isLanding) {
        Player_RequestRumble(player, 255, 20, 150, 0);
        Player_RequestQuake(play, 27767, 5, 12);
        player->linearVelocity = JUMP_FINISH_LUNGE;
        player->yaw = player->actor.shape.rot.y;
    } else {
        player->linearVelocity *= JUMP_XZ_SCALE;
        player->actor.velocity.y *= JUMP_Y_SCALE;
    }
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, player->skelAnime.curFrame,
                         Animation_GetLastFrame(anim), ANIMMODE_ONCE, -4.0f);
}

// Vanilla's spin release and jump slash, re-skinned; a clip vanilla puts back is re-skinned again.
static void ReskinVanillaStrikes(PlayState* play, Player* player) {
    s8 row = player->meleeWeaponAnimation;
    bool isMelee = player->actionFunc == Player_Action_808502D0;
    bool isSpin = row >= PLAYER_MWA_SPIN_ATTACK_1H && row <= PLAYER_MWA_BIG_SPIN_2H;

    if (isMelee && isSpin) {
        // The level is spent here, so a quick spin with no charge behind it never inherits the last one.
        if (!IsPlayingClip(player, CLIP_CHARGE_MAX) && !IsPlayingClip(player, CLIP_CHARGE_RELEASE)) {
            StartChargeRelease(play, player, sCharge.releaseLevel >= 3 ? CLIP_CHARGE_MAX : CLIP_CHARGE_RELEASE);
            sCharge.releaseLevel = 0;
        }
        if (IsPlayingClip(player, CLIP_CHARGE_MAX)) {
            TickMaxRelease(play, player);
        }
        return;
    }
    if (player->actionFunc == Player_Action_80844AF4 &&
        (row == PLAYER_MWA_JUMPSLASH_START || row == PLAYER_MWA_JUMPSLASH_FINISH)) {
        ReskinJumpSlash(play, player, false);
    } else if (isMelee && row == PLAYER_MWA_JUMPSLASH_FINISH) {
        ReskinJumpSlash(play, player, true);
    }
}

// The charge is a guard stance, so a shield comes up with it, the hands the way vanilla's walking shield sets them.
static void RaiseShieldWhileCharging(Player* player) {
    if (player->currentShield == PLAYER_SHIELD_NONE || Player_HoldsTwoHandedWeapon(player)) {
        return;
    }
    player->stateFlags1 |= PLAYER_STATE1_SHIELDING;
    Player_SetModelsForHoldingShield(player);
    sCharge.isShieldRaised = true;
}

// Vanilla fills the bar inside the action; it is pulled back here to the slower rate. Level 3 is the bar full
// and then held there another second.
static void TickCharging(PlayState* play, Player* player) {
    if (sCharge.prevCharge >= 0.0f && player->unk_858 > sCharge.prevCharge + CHARGE_RATE) {
        player->unk_858 = sCharge.prevCharge + CHARGE_RATE;
    }
    sCharge.prevCharge = player->unk_858;
    sCharge.fullHold = player->unk_858 >= CHARGE_LEVEL_3 ? MIN(sCharge.fullHold + 1, CHARGE_LEVEL_3_HOLD) : 0;
    if (sCharge.fullHold >= CHARGE_LEVEL_3_HOLD) {
        sCharge.level = 3;
    } else {
        sCharge.level = player->unk_858 >= CHARGE_LEVEL_2 ? 2 : 1;
    }
    sCharge.releaseLevel = sCharge.level;
    HoldUpperPose(play, player, CLIP_CHARGE_1 + sCharge.level - 1, (u32)play->gameplayFrames);
    RaiseShieldWhileCharging(player);
    sCharge.wasCharging = true;
}

// Hit while charging: vanilla already put its own reaction on, so the stagger goes on after it, once.
static void EndCharging(PlayState* play, Player* player) {
    LinkAnimationHeader* stagger;

    if (sCharge.wasCharging && (player->stateFlags1 & PLAYER_STATE1_DAMAGED)) {
        stagger = LoadClip(CLIP_CHARGE_HURT);
        if (stagger != NULL) {
            Player_AnimPlayOnce(play, player, stagger);
        }
    }
    sCharge.wasCharging = false;
    sCharge.prevCharge = -1.0f;
    sCharge.level = 0;
    sCharge.fullHold = 0;
    if (sCharge.isShieldRaised) {
        sCharge.isShieldRaised = false;
        Player_SetModelGroup(player, Player_ActionToModelGroup(player, player->heldItemAction));
    }
}

// The ball sits on the lance tip through the charge, drifts into Link's chest for the full release, and fades
// wherever it is otherwise.
static void TickChargeBall(Player* player, bool isCharging) {
    Vec3f want;
    f32 fill = MIN(player->unk_858 / CHARGE_LEVEL_2, 1.0f);

    sCharge.magicTimer++;
    if (isCharging) {
        want = sLance.lanceTip;
        if (!sCharge.isMagicActive) {
            sCharge.magicAnchor = want;
            sCharge.isMagicActive = true;
        }
        Math_ApproachF(&sCharge.magicAnchor.x, want.x, 0.5f, 30.0f);
        Math_ApproachF(&sCharge.magicAnchor.y, want.y, 0.5f, 30.0f);
        Math_ApproachF(&sCharge.magicAnchor.z, want.z, 0.5f, 30.0f);
        Math_ApproachF(&sCharge.magicCircle, BALL_CIRCLE_SCALE * fill, 0.3f, 0.02f);
        Math_ApproachF(&sCharge.magicBall, BALL_DRAW_SCALE * fill, 0.3f, 2.0f);
        Math_ApproachF(&sCharge.magicAlpha, 255.0f, 1.0f, 30.0f);
        if (fill >= 1.0f && sCharge.magicRays < RAYS_MAX && (sCharge.magicTimer & 3) == 0) {
            sCharge.magicRays++;
        } else if (fill < 1.0f && sCharge.magicRays > 0) {
            sCharge.magicRays--;
        }
        return;
    }
    if (sCharge.isMagicActive && IsPlayingClip(player, CLIP_CHARGE_MAX)) {
        want = player->actor.world.pos;
        want.x += Math_SinS(player->actor.shape.rot.y) * CHEST_FORWARD;
        want.z += Math_CosS(player->actor.shape.rot.y) * CHEST_FORWARD;
        want.y += CHEST_UP;
        Math_ApproachF(&sCharge.magicAnchor.x, want.x, 0.35f, 40.0f);
        Math_ApproachF(&sCharge.magicAnchor.y, want.y, 0.35f, 40.0f);
        Math_ApproachF(&sCharge.magicAnchor.z, want.z, 0.35f, 40.0f);
        sCharge.magicRays = MIN(sCharge.magicRays + 1, RAYS_MAX);
        return;
    }
    Math_ApproachZeroF(&sCharge.magicCircle, 1.0f, 0.02f);
    Math_ApproachZeroF(&sCharge.magicBall, 1.0f, 2.0f);
    Math_ApproachZeroF(&sCharge.magicAlpha, 1.0f, 30.0f);
    sCharge.magicRays = MAX(sCharge.magicRays - 1, 0);
    if (sCharge.magicCircle <= 0.0f && sCharge.magicBall <= 0.0f) {
        sCharge.isMagicActive = false;
    }
}

static void TickCharge(PlayState* play, Player* player) {
    bool isCharging = (player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) && IsWielding(player);

    if (isCharging) {
        TickCharging(play, player);
    } else if (sCharge.wasCharging || sCharge.isShieldRaised) {
        EndCharging(play, player);
    }
    TickChargeBall(player, isCharging);
    if (sCharge.goldTimer > 0) {
        sCharge.goldTimer--;
    }
    if (sCharge.domeTimer > 0) {
        sCharge.domeTimer--;
    }
}

// The iron boots' physics while the lance is out, only the REGs: currentBoots is swapped for the call alone.
static void WeighDown(PlayState* play, Player* player, bool isDrawn) {
    u8 boots = player->currentBoots;

    if (isDrawn) {
        player->currentBoots = PLAYER_BOOTS_IRON;
        Player_SetBootData(play, player);
        player->currentBoots = boots;
        sLance.areBootsHeavy = true;
        return;
    }
    if (sLance.areBootsHeavy) {
        sLance.areBootsHeavy = false;
        Player_SetBootData(play, player);
    }
}

// Entering water puts the lance away through vanilla's own item change.
static void SheatheInWater(PlayState* play, Player* player) {
    if ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) && IsWielding(player) &&
        !(player->stateFlags1 & PLAYER_STATE1_START_CHANGING_HELD_ITEM) &&
        player->itemAction == player->heldItemAction) {
        Player_UseItem(play, player, ITEM_NONE);
    }
}

static bool CanTakeOff(Player* player) {
    return IsGrounded(player) && (IsCapeOwned() || gSaveContext.magic > 0);
}

// R+B is tested before the R+A hold, so a deliberate dash is never swallowed by the take-off counter.
static void ReadGuardButtons(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];
    bool isRHeld = CHECK_BTN_ALL(input->cur.button, BTN_R);

    if (isRHeld && CHECK_BTN_ALL(input->press.button, BTN_B) && IsGrounded(player)) {
        sLance.flyHold = 0;
        StartDash(play, player);
        return;
    }
    if (!isRHeld || !CHECK_BTN_ALL(input->cur.button, BTN_A)) {
        sLance.flyHold = 0;
        return;
    }
    if (++sLance.flyHold >= FLY_ENTER_HOLD) {
        sLance.flyHold = 0;
        if (CanTakeOff(player)) {
            TakeOff(play, player);
        }
    }
}

static void TickComboIdle(Player* player) {
    if (IsInLanceAction(player) || sLance.comboStep == 0) {
        return;
    }
    if (++sLance.comboIdle >= COMBO_RESET_FRAMES) {
        sLance.comboStep = 0;
        sLance.comboIdle = 0;
    }
}

static void TickBHold(PlayState* play) {
    if (CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_B)) {
        sLance.bHold = MIN(sLance.bHold + 1, B_HOLD_TO_CHARGE);
    } else {
        sLance.bHold = 0;
    }
}

static void TickTrident(void) {
    PlayState* play = gPlayState;
    Player* player;
    bool isDrawn;

    if (play == NULL || !IsWorn()) {
        return;
    }
    player = GET_PLAYER(play);
    if (sProjectileGrace > 0) {
        sProjectileGrace--;
    }
    if (!IsInLanceAction(player) && sLance.state != LANCE_SWING) {
        ResetFlight(player);
    }
    isDrawn = CanAct(player) && IsWielding(player);
    WeighDown(play, player, isDrawn);
    SheatheInWater(play, player);
    TickBHold(play);
    TickComboIdle(player);
    TickCharge(play, player);
    if (IsInLanceAction(player)) {
        return;
    }
    ReskinVanillaStrikes(play, player);
    if (isDrawn) {
        ReadGuardButtons(play, player);
    } else {
        sLance.flyHold = 0;
    }
}

// With R+B from the guard the thrust is not a swing either.
static void TakeMeleeButton(PlayState* play, Player* player, int32_t action, bool* consumed, bool* startedAction) {
    Input* input = &play->state.input[0];
    bool isThrust;

    if (*consumed || !IsWielding(player) || !IsGrounded(player) || !CanAct(player) ||
        !CHECK_BTN_ALL(input->press.button, BTN_B) || CHECK_BTN_ALL(input->cur.button, BTN_R) ||
        LoadClip(CLIP_SLASH_1) == NULL) {
        return;
    }
    isThrust = sLance.comboStep == 0 && Player_IsZTargeting(player) &&
               player->controlStickDirections[player->controlStickDataIndex] == PLAYER_STICK_DIR_FORWARD;
    if (isThrust) {
        StartSwing(play, player, &sThrustSwing, false);
    } else {
        StartComboStep(play, player, false);
    }
    *consumed = true;
    *startedAction = IsInLanceAction(player);
}

// R stays vanilla's shield; what stacks on it belongs to the dash and the take-off, and in flight every button
// belongs to the lance.
static void KeepButtonsFromVanilla(Player* player, Input* input) {
    u16 taken = 0;

    if (!IsWorn()) {
        return;
    }
    if (IsFlightState(sLance.state) && IsInLanceAction(player)) {
        taken = BTN_A | BTN_B | BTN_R | BTN_L;
    } else if (IsSwordAction(player->heldItemAction) && CHECK_BTN_ALL(input->cur.button, BTN_R)) {
        taken = BTN_A | BTN_B;
    }
    input->cur.button &= ~taken;
    input->press.button &= ~taken;
}

static void RunWithLance(int32_t group, int32_t animType, LinkAnimationHeader** anim) {
    PlayState* play = gPlayState;
    LinkAnimationHeader* run;

    if (play == NULL || (group != PLAYER_ANIMGROUP_walk && group != PLAYER_ANIMGROUP_run) ||
        !IsWielding(GET_PLAYER(play))) {
        return;
    }
    run = LoadClip(group == PLAYER_ANIMGROUP_walk ? CLIP_WALK : CLIP_RUN);
    if (run != NULL) {
        *anim = run;
    }
}

static void GripLance(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (limbIndex == PLAYER_LIMB_L_HAND && IsWielding(player)) {
        *dList = ResourceMgr_LoadGfxByName(gLinkAdultLeftHandNearDL);
    }
}

// Rotations before the offset, so the offset runs along the lance's own axes whichever way the hand points.
static void ApplyLanceMatrix(void) {
    Matrix_Translate(2028.26f, 267.2f, -33.82f, MTXMODE_APPLY);
    Matrix_RotateZYX(-0x8000, 0, 0x4000, MTXMODE_APPLY);
    Matrix_Scale(5.0f, 5.0f, 5.0f, MTXMODE_APPLY);
    Matrix_RotateZYX((s16)(LANCE_ROT_X * DEG_TO_BINANG), (s16)(LANCE_ROT_Y * DEG_TO_BINANG),
                     (s16)(LANCE_ROT_Z * DEG_TO_BINANG), MTXMODE_APPLY);
    Matrix_Scale(LANCE_HELD_SCALE, LANCE_HELD_SCALE, LANCE_HELD_SCALE, MTXMODE_APPLY);
    Matrix_Translate(LANCE_OFF_X, LANCE_OFF_Y, LANCE_OFF_Z, MTXMODE_APPLY);
}

// The sword code lays its blade along +X; the lance's shaft is +Z, and the lance frame is half a limb unit wide.
static void ApplyLanceTrailMatrix(void) {
    ApplyLanceMatrix();
    Matrix_RotateY(-M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Translate(TRAIL_BASE, 0.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(1.0f, TRAIL_WIDTH, TRAIL_WIDTH, MTXMODE_APPLY);
}

static void TraceLanceSwing(PlayState* play, Player* player) {
    static Vec3f sTipLocal = { TRAIL_TIP - TRAIL_BASE, 0.0f, 0.0f };
    Vec3f edges[3];

    Matrix_Push();
    ApplyLanceTrailMatrix();
    Matrix_MultVec3f(&sTipLocal, &sLance.lanceTip);
    sLance.isLanceTipValid = true;
    if (sLance.isMeleeStateHidden) {
        player->meleeWeaponState = sLance.hiddenMeleeState;
        sLance.isMeleeStateHidden = false;
    }
    if (player->meleeWeaponState != 0 && player->actor.scale.y >= 0.0f) {
        D_80126080.x = TRAIL_TIP - TRAIL_BASE;
        EffectBlure_ChangeType(Effect_GetByIndex(player->meleeWeaponEffectIndex), TRAIL_TYPE_MASTER_SWORD);
        func_80090A28(player, edges);
        func_800906D4(play, player, edges);
    }
    Matrix_Pop();
}

static void DrawLance(PlayState* play, Player* player, int32_t limbIndex) {
    if (limbIndex != PLAYER_LIMB_L_HAND || player != GET_PLAYER(play) || !IsWielding(player)) {
        return;
    }
    TraceLanceSwing(play, player);
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    ApplyLanceMatrix();
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEmptyDL);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sLanceDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// The vanilla hand block would lay the trail along the hidden sword first; it sees no swing until the lance's own
// post-limb puts the state back.
static void HideSwingFromSwordHand(Actor* actor, PlayState* play, bool* drawVanilla) {
    Player* player = (Player*)actor;

    if (actor != &GET_PLAYER(play)->actor || !IsWielding(player) || player->meleeWeaponState == 0) {
        return;
    }
    sLance.hiddenMeleeState = player->meleeWeaponState;
    sLance.isMeleeStateHidden = true;
    player->meleeWeaponState = 0;
}

// Din's Fire's own sphere shrunk to Link, without MagicFire_Draw's writes into its shared vertex buffer.
static void DrawDome(PlayState* play, Player* player) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u32 frame = play->gameplayFrames;
    f32 age = 1.0f - (f32)sCharge.domeTimer / DOME_FRAMES;
    f32 scale = sCharge.domeScale * (age < 0.35f ? age / 0.35f : 1.0f);
    u8 alpha = (u8)(255.0f * (age < 0.5f ? 1.0f : 1.0f - (age - 0.5f) * 2.0f));

    if (scale <= 0.0001f) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Xlu(gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 210, 255, 130, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 120, 255, 0, alpha);
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + DOME_UP, player->actor.world.pos.z,
                     MTXMODE_NEW);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
    gDPLoadTextureBlock(POLY_XLU_DISP++, sDomeTex, G_IM_FMT_I, G_IM_SIZ_8b, 64, 64, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, 6, 6, 15, G_TX_NOLOD);
    gDPSetTile(POLY_XLU_DISP++, G_IM_FMT_I, G_IM_SIZ_8b, 8, 0, 1, 0, G_TX_NOMIRROR | G_TX_WRAP, 6, 14,
               G_TX_NOMIRROR | G_TX_WRAP, 6, 14);
    gDPSetTileSize(POLY_XLU_DISP++, 1, 0, 0, 252, 252);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sDomeMaterialDL);
    gSPDisplayList(POLY_XLU_DISP++,
                   Gfx_TwoTexScrollEx(gfxCtx, 0, (frame * 2) % 512, 511 - ((frame * 5) % 512), 64, 64, 1,
                                      (frame * 2) % 256, 255 - ((frame * 20) % 256), 32, 32, 2, -5, 2, -20));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sDomeModelDL);
    CLOSE_DISPS(gfxCtx);
}

static void DrawChargeEffects(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (actor != &GET_PLAYER(play)->actor) {
        return;
    }
    if (sLance.isMeleeStateHidden) {
        player->meleeWeaponState = sLance.hiddenMeleeState;
        sLance.isMeleeStateHidden = false;
    }
    if (!IsWorn()) {
        return;
    }
    if (sCharge.domeTimer > 0) {
        DrawDome(play, player);
    }
    if (sCharge.isMagicActive) {
        DrawBigMagic(play, &sCharge.magicAnchor, sCharge.magicCircle, sCharge.magicBall, sCharge.magicAlpha,
                     sCharge.magicRays, play->gameplayFrames * 10.0f / 1000.0f);
    }
}

static void GildTunic(bool* should, va_list args) {
    Color_RGB8* color;

    va_arg(args, void*);
    color = va_arg(args, Color_RGB8*);
    if (IsWorn() && sCharge.goldTimer > 0) {
        *color = (Color_RGB8){ 255, 205, 40 };
    }
}

// A boss that only takes sword-class blows still has to take the ball as the heavy one it is.
static void StrikeHeavily(PlayState* play, int32_t dmgFlags, uint8_t* damage) {
    if (IsWorn() && sProjectileGrace > 0 && *damage < MAX_BALL_DAMAGE) {
        *damage = MAX_BALL_DAMAGE;
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateZ(M_PI / 4.0f, MTXMODE_APPLY);
    Matrix_Scale(GET_ITEM_SCALE, GET_ITEM_SCALE, GET_ITEM_SCALE, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEmptyDL);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sLanceDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The lance is wielded with the Mirror Shield or bare-handed; the shield it replaces is not given back.
static void TakeUpMirrorShield(const char* key) {
    if (CustomEquipRegistry_GetWorn(SOH_EQUIP_SLOT_SHIELD) != NULL) {
        CustomEquipRegistry_TakeOff(SOH_EQUIP_SLOT_SHIELD);
    }
    if (CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_MIRROR)) {
        Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_MIRROR);
    } else {
        Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);
    }
}

// Spawned actors are gone by the next scene; the pointers are dropped, never followed.
static void ForgetSceneState(int16_t sceneNum) {
    sLance.state = LANCE_SWING;
    sLance.swing = NULL;
    sLance.launchTarget = NULL;
    sLance.isLanceTipValid = false;
    sLance.isMeleeStateHidden = false;
    sAreQuadsReady = false;
    sProjectileGrace = 0;
    memset(sLiveProjectiles, 0, sizeof(sLiveProjectiles));
    memset(&sCharge, 0, sizeof(sCharge));
    sCharge.prevCharge = -1.0f;
}

static void PutLanceAway(const char* key) {
    PlayState* play = gPlayState;

    if (play != NULL) {
        Player* player = GET_PLAYER(play);

        if (IsInLanceAction(player)) {
            ResetFlight(player);
            func_80839FFC(player, play);
        }
        if (sLance.areBootsHeavy) {
            Player_SetBootData(play, player);
        }
    }
    ForgetSceneState(0);
    sLance.comboStep = 0;
    sLance.areBootsHeavy = false;
}

static bool RegisterProjectileActor(void) {
    SOHActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = PROJECTILE_KEY;
    definition.description = "Trident light and big magic";
    definition.category = ACTORCAT_MISC;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(Projectile);
    definition.init = ProjectileInit;
    definition.destroy = ProjectileDestroy;
    definition.update = ProjectileUpdate;
    definition.draw = ProjectileDraw;
    sProjectileActorId = sApi->RegisterActor(&definition);
    return sProjectileActorId >= 0;
}

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate",       "OnPlayerActionHandler", "OnPlayerFilterInput", "OnPlayerResolveAnim",
    "OnPlayerResolveLimbDraw", "OnPlayerPostLimbDraw", "OnActorDraw",      "OnActorDrawEnd",
    "OnResolveSwordDamage", "OnCollisionResolveDamage", "OnSceneInit",
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

    if (!SOH_MOD_API_HAS(sApi, RegisterActor) || !RegisterProjectileActor()) {
        return;
    }
    for (s32 limb = PLAYER_LIMB_UPPER; limb < PLAYER_LIMB_MAX; limb++) {
        sUpperBodyMap[limb] = true;
    }
    sCharge.prevCharge = -1.0f;

    definition.structSize = sizeof(definition);
    definition.key = TRIDENT_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rTrident&%wHold %y\xA0%w to charge. %y\xA3%w+%y\xA0%w dashes; hold %y\xA3%w+%y\x9F%w "
                           "to fly.";
    definition.getItemText = "You got the %rTrident%w!&Phantom Ganon's lance. Charge it to call down his master's "
                             "magic, or hold it high and take to the air as he did.";
    definition.slot = SOH_EQUIP_SLOT_SWORD;
    definition.page = 0;
    definition.row = SOH_EQUIP_SLOT_SWORD;
    definition.column = 3;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_ADULT;
    definition.vanillaBase = EQUIP_VALUE_SWORD_MASTER;
    definition.toggles = 1;
    definition.onEquip = TakeUpMirrorShield;
    definition.onUnequip = PutLanceAway;
    definition.getItemEntry.drawFunc = DrawGetItem;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickTrident);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_MELEE, TakeMeleeButton);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, KeepButtonsFromVanilla);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveAnim, RunWithLance);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, GripLance);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawLance);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_PLAYER, HideSwingFromSwordHand);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawChargeEffects);
    SOH_REGISTER_HOOK(sApi, OnResolveSwordDamage, StrikeHeavily);
    SOH_REGISTER_HOOK(sApi, OnCollisionResolveDamage, KeepFixedDamage);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    sApi->RegisterVB(VB_APPLY_TUNIC_COLOR, GildTunic);
}
