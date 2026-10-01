#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "overlays/actors/ovl_Bg_Ice_Shelter/z_bg_ice_shelter.h"
#include "overlays/actors/ovl_Bg_Ice_Turara/z_bg_ice_turara.h"
#include "overlays/actors/ovl_En_Fz/z_en_fz.h"

#define BALL_KEY "nei.ball_and_chain"
#define BALL_BUTTON_COUNT 8
#define BALL_DAMAGE 8
#define BALL_COLLIDER_RADIUS 20
#define BALL_COLLIDER_HEIGHT 20
#define BALL_RADIUS 12.0f
#define BALL_WALL_HEIGHT 20.0f
#define BALL_ICE_REACH 140.0f
#define BALL_POT_REACH 100.0f
#define BALL_IRON_REACH 60.0f
#define BALL_HOLD_WALK_SCALE 0.5f
#define BALL_SPIN_WALK_SCALE 0.35f
#define BALL_GUARD_RADIUS_MIN 45.0f
#define BALL_GUARD_RADIUS_MAX 70.0f
#define BALL_GUARD_HEIGHT 30.0f
#define BALL_GOLDEN_DAMAGE 16
#define BALL_HOLD_DROP 5.0f
#define BALL_CHARGE_MAX 60
// The ball has to have built up a swing before it can be aimed or hurled; a tap only lifts it.
#define BALL_CHARGE_MIN 15
#define BALL_SPIN_RADIUS 20.0f
#define BALL_SPIN_HEIGHT_MIN 50.0f
#define BALL_SPIN_HEIGHT_MAX 55.0f
#define BALL_SPIN_SPEED_MIN 0x1000
#define BALL_SPIN_SPEED_MAX 0x2000
#define BALL_STICK_DEADZONE 5.0f
#define BALL_LEAN_TILT 40.0f
#define BALL_THROW_LEAN 3000
// The swing pulls Link 1 degree off balance at rest and 5 at full charge, in the ball's own direction.
#define BALL_LEAN_MIN 182.0f
#define BALL_LEAN_MAX 910.0f
#define BALL_HOLD_POSE_FRAME 8
#define BALL_SPIN_POSE_FRAME 0
#define BALL_LAUNCH_SPEED_MIN 22.0f
#define BALL_LAUNCH_SPEED_MAX 32.0f
#define BALL_LAUNCH_LIFT 7.0f
#define BALL_GRAVITY -1.5f
#define BALL_TERMINAL_FALL 30.0f
#define BALL_RETRACT_SPEED 34.0f
#define BALL_BOUNCE_KEEP_Y 0.45f
#define BALL_BOUNCE_KEEP_XZ 0.65f
#define BALL_WALL_KEEP 0.55f
#define BALL_MAX_BOUNCES 2
#define BALL_REST_FRAMES 12
#define BALL_CHAIN_LENGTH 380.0f
#define BALL_RETURN_DISTANCE 65.0f
#define BALL_THROW_TIMEOUT 200
#define BALL_HELD_SCALE 0.06f
#define BALL_SWUNG_SCALE 0.1f
// The chain model is one strand 100 units long at scale 1.0, the length the hookshot stretches it by.
#define BALL_CHAIN_STRAND_SPAN 100.0f
#define BALL_CHAIN_STRAND_MAX 8
#define BALL_CHAIN_THICKNESS 0.02f
#define BALL_CHAIN_STRETCH 0.01f
#define BALL_GIVE_SCALE 0.25f

typedef enum {
    BALL_IDLE,
    BALL_HOLDING,
    BALL_SPINNING,
    BALL_AIMING,
    BALL_FLYING,
    BALL_RESTING,
    BALL_RETRACTING,
} BallPhase;

// Goron Bracelet plants you and throws forward, Silver Gauntlets free your feet and your aim, Golden ones
// swing the ball one-handed in a circle that guards Link.
typedef enum {
    BALL_STRENGTH_NONE,
    BALL_STRENGTH_BRACELET,
    BALL_STRENGTH_SILVER,
    BALL_STRENGTH_GOLDEN,
} BallStrength;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconBallAndChainTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gBallAndChainNameTex";
static const ALIGN_ASSET(2) char sBallDL[] = "__OTR__objects/object_nei_ball_and_chain/gBallDL";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_ball_and_chain/g_ball_and_chain_dl";
static const ALIGN_ASSET(2) char sChainLinkDL[] = "__OTR__objects/object_link_boy/gLinkAdultHookshotChainDL";
static const ALIGN_ASSET(2) char sPotShardsDL[] = "__OTR__objects/gameplay_keep/gEffFragments2DL";

// Vanilla carries: the ball rides in front of Link while he holds it, and overhead once he swings it.
static const ALIGN_ASSET(2) char sHoldPoseAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_carryB_free";
static const ALIGN_ASSET(2) char sSpinPoseAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_carryB_wait";

static const u16 sItemButtons[BALL_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                     BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const u8 sArmLimbs[] = { PLAYER_LIMB_L_SHOULDER, PLAYER_LIMB_L_FOREARM, PLAYER_LIMB_L_HAND,
                                PLAYER_LIMB_R_SHOULDER, PLAYER_LIMB_R_FOREARM, PLAYER_LIMB_R_HAND };

static ColliderCylinderInit sBallColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER | AT_TYPE_OTHER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_HAMMER_SWING, 0x00, BALL_DAMAGE }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { BALL_COLLIDER_RADIUS, BALL_COLLIDER_HEIGHT, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = { "OnPlayerUpdate",   "OnActorDraw",
                                              "OnActorDrawEnd",  "OnPlayerFilterInput",
                                              "OnPlayerResolveMotionScale", "OnPlayerResolveItemActionInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static ColliderCylinder sBallCollider;
static bool sIsColliderReady;
static u8 sPhase;
static s16 sCharge;
static s16 sSpinAngle;
static s16 sThrowYaw;
static s16 sThrownFrames;
static s16 sRestTimer;
static u8 sBounces;
static f32 sStickLeanX;
static f32 sStickLeanY;
static Vec3f sBallPos;
static Vec3f sBallVelocity;
static Vec3s sPoseJoints[PLAYER_LIMB_MAX];
static s32 sTrailIndex = -1;

s32 Player_UpdateUpperBody(Player* player, PlayState* play);
s32 Player_GetStrength(void);
s32 Player_IsZTargeting(Player* player);
void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_ZeroSpeedXZ(Player* player);
void func_80839FFC(Player* player, PlayState* play);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
void func_808911BC(BgIceShelter* ice);
void EnFz_SetupMelt(EnFz* freezard);
void BgIceTurara_Break(BgIceTurara* icicle, PlayState* play, f32 scale);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static u16 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < BALL_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, BALL_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool IsButtonHeld(PlayState* play) {
    return (play->state.input[0].cur.button & FindEquippedButtonMask()) != 0;
}

static bool IsBallAway(void) {
    return sPhase >= BALL_FLYING;
}

static s32 Strength(void) {
    return Player_GetStrength();
}

static bool CanLiftBall(Player* player, PlayState* play) {
    return Strength() >= BALL_STRENGTH_BRACELET && !(player->stateFlags1 & PLAYER_STATE1_IN_WATER);
}

static bool CanWalkWithBall(void) {
    return Strength() >= BALL_STRENGTH_SILVER;
}

static bool CanAimBall(void) {
    return Strength() >= BALL_STRENGTH_SILVER;
}

static bool IsOneHanded(void) {
    return Strength() >= BALL_STRENGTH_GOLDEN;
}

static void StartTrail(PlayState* play) {
    EffectBlureInit2 init = {
        0, 8, 0, { 255, 255, 255, 255 }, { 200, 220, 255, 128 }, { 255, 255, 255, 0 }, { 200, 220, 255, 0 }, 4, 0, 2, 0,
        { 235, 235, 245, 200 }, { 180, 190, 210, 96 }, TRAIL_TYPE_SWORDS,
    };

    Effect_Add(play, &sTrailIndex, EFFECT_BLURE2, 0, 0, &init);
}

static void FeedTrail(PlayState* play) {
    Vec3f top = { sBallPos.x, sBallPos.y + 14.0f, sBallPos.z };
    Vec3f bottom = { sBallPos.x, sBallPos.y - 14.0f, sBallPos.z };

    if (sTrailIndex < 0) {
        StartTrail(play);
    }
    EffectBlure_AddVertex((EffectBlure*)Effect_GetByIndex(sTrailIndex), &top, &bottom);
}

static void EndTrail(PlayState* play) {
    if (sTrailIndex >= 0) {
        Effect_Delete(play, sTrailIndex);
        sTrailIndex = -1;
    }
}

// Ground-level strikes count as the hammer's floor slam, which is what floor switches and stakes answer to.
static void StrikeWithBall(Player* player, PlayState* play) {
    bool isAtFootLevel = sBallPos.y - player->actor.world.pos.y < 30.0f;

    sBallCollider.info.toucher.dmgFlags = isAtFootLevel ? DMG_HAMMER_JUMP : DMG_HAMMER_SWING;
    sBallCollider.info.toucher.damage = Strength() >= BALL_STRENGTH_GOLDEN ? BALL_GOLDEN_DAMAGE : BALL_DAMAGE;
    sBallCollider.dim.pos.x = (s16)sBallPos.x;
    sBallCollider.dim.pos.y = (s16)(sBallPos.y - BALL_COLLIDER_HEIGHT / 2);
    sBallCollider.dim.pos.z = (s16)sBallPos.z;
    sBallCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER | AT_TYPE_OTHER;
    if (sBallCollider.base.atFlags & AT_HIT) {
        sBallCollider.base.atFlags &= ~AT_HIT;
        PlaySfxAt(NA_SE_IT_HAMMER_HIT, &sBallPos);
    }
    CollisionCheck_SetAT(play, &play->colChkCtx, &sBallCollider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &sBallCollider.base);
}

static void MeltRedIce(PlayState* play, Actor* actor) {
    BgIceShelter* ice = (BgIceShelter*)actor;
    f32 dx = sBallPos.x - actor->world.pos.x;
    f32 dy = sBallPos.y - actor->world.pos.y;
    f32 dz = sBallPos.z - actor->world.pos.z;
    f32 reach = ice->cylinder1.dim.radius + BALL_COLLIDER_RADIUS;

    if (sqrtf(SQ(dx) + SQ(dz)) < reach && dy > -BALL_COLLIDER_RADIUS &&
        dy < ice->cylinder1.dim.height + BALL_COLLIDER_RADIUS && ice->actionFunc != NULL) {
        func_808911BC(ice);
        Audio_PlayActorSound2(actor, NA_SE_EV_ICE_MELT);
    }
}

static void DropFromPot(PlayState* play, Vec3f* pos, s16 params, s16 yaw, f32 lift) {
    EnItem00* collectible = Item_DropCollectible(play, pos, params);

    if (collectible != NULL) {
        collectible->actor.velocity.y = lift;
        collectible->actor.world.rot.y = yaw;
    }
}

static void ShatterShadowTemplePot(PlayState* play, Actor* pot) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f burstPos = { pot->world.pos.x, pot->world.pos.y + 80.0f, pot->world.pos.z };
    Vec3f dropPos = { pot->world.pos.x, pot->world.pos.y + 200.0f, pot->world.pos.z };

    EffectSsBomb2_SpawnLayered(play, &burstPos, &zero, &zero, 100, 45);
    SoundSource_PlaySfxAtFixedWorldPos(play, &pot->world.pos, 50, NA_SE_EV_BOX_BREAK);
    EffectSsHahen_SpawnBurst(play, &burstPos, 20.0f, 0, 350, 100, 50, OBJECT_HAKA_OBJECTS, 40, (Gfx*)sPotShardsDL);
    if (pot->room == 12) {
        for (s16 i = 0; i < 9; i++) {
            DropFromPot(play, &dropPos, i % 3, pot->shape.rot.y + i * 0x1C71, 15.0f);
        }
        Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
    } else if (Flags_GetCollectible(play, pot->params)) {
        if (!CVarGetInteger("gEnhancements.NoHeartDrops", 0)) {
            DropFromPot(play, &dropPos, ITEM00_HEART, pot->shape.rot.y, 15.0f);
        }
        Sfx_PlaySfxCentered(NA_SE_SY_TRE_BOX_APPEAR);
    } else {
        DropFromPot(play, &dropPos, ((pot->params & 0x3F) << 8) | ITEM00_SMALL_KEY, pot->shape.rot.y, 15.0f);
        Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
    }
    Actor_Kill(pot);
}

// Every reward the Goron City pot can hand out, in one hit, with the heart piece keeping its collectible flag.
static void ShatterGoronCityPot(PlayState* play, Actor* pot) {
    static const s16 sDropYaws[] = { -0x0FA0, 0x0320, 0x0FA0 };
    Vec3f dropPos = { pot->world.pos.x, pot->world.pos.y + 170.0f, pot->world.pos.z };
    s16 heartPieceFlag = pot->params & 0x3F;

    for (s32 i = 0; i < 3; i++) {
        DropFromPot(play, &dropPos, ITEM00_BOMBS_A, sDropYaws[i] + 0x2000, 11.0f);
        DropFromPot(play, &dropPos, ITEM00_RUPEE_GREEN, sDropYaws[i] + 0x4000, 11.0f);
    }
    DropFromPot(play, &dropPos,
                Flags_GetCollectible(play, heartPieceFlag) ? ITEM00_RUPEE_PURPLE : (heartPieceFlag << 8) | ITEM00_HEART_PIECE,
                sDropYaws[1], 11.0f);
    DropFromPot(play, &dropPos, ITEM00_RUPEE_RED, sDropYaws[0] + 0x6000, 11.0f);
    DropFromPot(play, &dropPos, ITEM00_RUPEE_BLUE, sDropYaws[2] + 0x6000, 11.0f);
    Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
    PlaySfxAt(NA_SE_EV_POT_BROKEN, &pot->world.pos);
    if (pot->child != NULL) {
        pot->child->parent = NULL;
        Actor_Kill(pot->child);
    }
    Actor_Kill(pot);
}

static void CrushIronObject(PlayState* play, Actor* iron) {
    Vec3f dropPos = { iron->world.pos.x, iron->world.pos.y + 20.0f, iron->world.pos.z };

    func_80033480(play, &iron->world.pos, 140.0f, 12, 180, 90, 1);
    SoundSource_PlaySfxAtFixedWorldPos(play, &iron->world.pos, 80, NA_SE_EN_IRONNACK_BREAK_PILLAR);
    for (s32 i = 0; i < 3; i++) {
        Item_DropCollectible(play, &dropPos, ITEM00_HEART);
        dropPos.y += 18.0f;
    }
    Actor_Kill(iron);
}

static void BreakIcicle(PlayState* play, Actor* actor) {
    BgIceTurara* icicle = (BgIceTurara*)actor;

    if (actor->params == TURARA_STALAGMITE) {
        icicle->collider.base.acFlags |= AC_HIT;
        return;
    }
    BgIceTurara_Break(icicle, play, 40.0f);
    Actor_Kill(actor);
}

static bool IsBallNear(Actor* actor, f32 reach) {
    return Math_Vec3f_DistXYZ(&sBallPos, &actor->world.pos) < reach;
}

// A fast ball tunnels through thin ice and pots between frames, so those break on proximity, not on contact.
static void BreakWhatTheBallReaches(PlayState* play) {
    Actor* next;

    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor != NULL; actor = next) {
        next = actor->next;
        if (actor->id == ACTOR_BG_ICE_SHELTER) {
            MeltRedIce(play, actor);
        } else if (actor->id == ACTOR_BG_HAKA_TUBO && IsBallNear(actor, BALL_POT_REACH)) {
            ShatterShadowTemplePot(play, actor);
        }
    }
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_PROP].head; actor != NULL; actor = next) {
        next = actor->next;
        if (actor->id == ACTOR_BG_JYA_IRONOBJ && IsBallNear(actor, BALL_IRON_REACH)) {
            CrushIronObject(play, actor);
        } else if (actor->id == ACTOR_BG_SPOT18_BASKET && IsBallNear(actor, BALL_POT_REACH)) {
            ShatterGoronCityPot(play, actor);
        } else if (actor->id == ACTOR_BG_ICE_TURARA && IsBallNear(actor, BALL_ICE_REACH)) {
            BreakIcicle(play, actor);
        }
    }
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; actor != NULL; actor = next) {
        next = actor->next;
        if (actor->id == ACTOR_EN_FZ && ((EnFz*)actor)->state != 3 && IsBallNear(actor, BALL_ICE_REACH)) {
            EnFz_SetupMelt((EnFz*)actor);
        }
    }
}

static void CrushArmoredFoes(void) {
    Actor* hit = sBallCollider.base.at;

    if (!(sBallCollider.base.atFlags & AT_HIT) || hit == NULL || hit->update == NULL) {
        return;
    }
    if ((hit->id == ACTOR_EN_ST || hit->id == ACTOR_EN_FZ) && hit->colChkInfo.health > 0) {
        hit->colChkInfo.health = MAX(hit->colChkInfo.health - BALL_DAMAGE, 0);
    }
}

static void StrikeAndBreak(Player* player, PlayState* play) {
    CrushArmoredFoes();
    StrikeWithBall(player, play);
    BreakWhatTheBallReaches(play);
}

// Golden Gauntlets hold the ball in the throwing hand alone; weaker arms need both.
static void HoldBallInHands(Player* player) {
    Vec3f* left = &player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    Vec3f* right = &player->bodyPartsPos[PLAYER_BODYPART_R_HAND];

    if (IsOneHanded()) {
        sBallPos = *right;
        sBallPos.y += BALL_HOLD_DROP;
        return;
    }
    sBallPos.x = (left->x + right->x) * 0.5f;
    sBallPos.y = (left->y + right->y) * 0.5f + BALL_HOLD_DROP;
    sBallPos.z = (left->z + right->z) * 0.5f;
}

// The engine's own speed resolution, so Link keeps his turning and his animations and only loses pace.
static void WeighDownPlayer(Player* player, int32_t kind, float* scale) {
    if (sPhase == BALL_IDLE || IsBallAway()) {
        return;
    }
    if (!CanWalkWithBall()) {
        *scale = 0.0f;
        return;
    }
    *scale *= sPhase == BALL_SPINNING ? BALL_SPIN_WALK_SCALE : BALL_HOLD_WALK_SCALE;
}

static void FaceThrowDirection(Player* player) {
    player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = sThrowYaw;
}

static void ReadStickLean(Player* player, PlayState* play) {
    s8 stickX = play->state.input[0].cur.stick_x;
    s8 stickY = play->state.input[0].cur.stick_y;

    sStickLeanX = 0.0f;
    sStickLeanY = 0.0f;
    if (!Player_IsZTargeting(player) && sqrtf(SQ(stickX) + SQ(stickY)) > BALL_STICK_DEADZONE) {
        sStickLeanX = stickX / 127.0f;
        sStickLeanY = stickY / 127.0f;
    }
}

static void OrbitBallOverhead(Player* player) {
    f32 chargeRatio = (f32)sCharge / BALL_CHARGE_MAX;
    f32 orbitX = Math_SinS(sSpinAngle) * BALL_SPIN_RADIUS;
    f32 orbitZ = Math_CosS(sSpinAngle) * BALL_SPIN_RADIUS;
    f32 orbitY = BALL_SPIN_HEIGHT_MIN + (BALL_SPIN_HEIGHT_MAX - BALL_SPIN_HEIGHT_MIN) * chargeRatio -
                 Math_CosS(sSpinAngle) * sStickLeanY * BALL_LEAN_TILT - Math_SinS(sSpinAngle) * sStickLeanX * BALL_LEAN_TILT;
    s16 yaw = player->actor.shape.rot.y;

    sSpinAngle += (s16)(BALL_SPIN_SPEED_MIN + (BALL_SPIN_SPEED_MAX - BALL_SPIN_SPEED_MIN) * chargeRatio);
    sBallPos.x = player->actor.world.pos.x + orbitX * Math_CosS(yaw) + orbitZ * Math_SinS(yaw);
    sBallPos.y = player->actor.world.pos.y + orbitY;
    sBallPos.z = player->actor.world.pos.z - orbitX * Math_SinS(yaw) + orbitZ * Math_CosS(yaw);
}

static void BallThrownAction(Player* player, PlayState* play);

static void ReturnBallToHands(Player* player, PlayState* play) {
    sPhase = BALL_HOLDING;
    EndTrail(play);
    Audio_StopSfxById(NA_SE_PL_WALK_GROUND);
    if (player->actionFunc == BallThrownAction) {
        func_80839FFC(player, play);
    }
}

// Aimed throws lead the arc so gravity lands the ball on the mark; a plain throw only goes where Link faces.
static void LaunchTowards(Vec3f* mark, f32 launchSpeed) {
    f32 dx = mark->x - sBallPos.x;
    f32 dy = mark->y - sBallPos.y;
    f32 dz = mark->z - sBallPos.z;
    f32 flightFrames = MAX(sqrtf(SQ(dx) + SQ(dz)) / launchSpeed, 1.0f);

    sBallVelocity.x = dx / flightFrames;
    sBallVelocity.y = dy / flightFrames - 0.5f * BALL_GRAVITY * flightFrames;
    sBallVelocity.z = dz / flightFrames;
    sThrowYaw = Math_Vec3f_Yaw(&sBallPos, mark);
}

static void LaunchAlong(s16 yaw, s16 pitch, f32 launchSpeed) {
    sThrowYaw = yaw;
    sBallVelocity.x = Math_SinS(yaw) * Math_CosS(pitch) * launchSpeed;
    sBallVelocity.y = BALL_LAUNCH_LIFT - Math_SinS(pitch) * launchSpeed;
    sBallVelocity.z = Math_CosS(yaw) * Math_CosS(pitch) * launchSpeed;
}

static void AimLaunch(Player* player, f32 launchSpeed) {
    Actor* target = player->focusActor;

    if (sPhase == BALL_AIMING) {
        s16 yaw;
        s16 pitch;

        Z64Aiming_GetDirection(player, &yaw, &pitch);
        LaunchAlong(yaw, pitch, launchSpeed);
        return;
    }
    if (CanAimBall() && Player_IsZTargeting(player) && target != NULL && target->update != NULL) {
        LaunchTowards(&target->focus.pos, launchSpeed);
        return;
    }
    LaunchAlong(player->actor.shape.rot.y, 0, launchSpeed);
}

static void ThrowBall(Player* player, PlayState* play) {
    f32 launchSpeed = BALL_LAUNCH_SPEED_MIN + (BALL_LAUNCH_SPEED_MAX - BALL_LAUNCH_SPEED_MIN) * sCharge / BALL_CHARGE_MAX;

    sBallPos.x = player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * 20.0f;
    sBallPos.y = player->actor.world.pos.y + 45.0f;
    sBallPos.z = player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * 20.0f;
    AimLaunch(player, launchSpeed);
    sPhase = BALL_FLYING;
    sThrownFrames = 0;
    sBounces = 0;
    Player_SetupAction(play, player, BallThrownAction, 1);
    Player_ZeroSpeedXZ(player);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
    PlaySfxAt(NA_SE_IT_HAMMER_SWING, &player->actor.world.pos);
}

// Golden Gauntlets swing the ball one-handed and wide, so it circles Link at chest height and guards him
// instead of riding over his head.
static void OrbitBallAround(Player* player) {
    f32 chargeRatio = (f32)sCharge / BALL_CHARGE_MAX;
    f32 radius = BALL_GUARD_RADIUS_MIN + (BALL_GUARD_RADIUS_MAX - BALL_GUARD_RADIUS_MIN) * chargeRatio;

    sSpinAngle += (s16)(BALL_SPIN_SPEED_MIN + (BALL_SPIN_SPEED_MAX - BALL_SPIN_SPEED_MIN) * chargeRatio);
    sBallPos.x = player->actor.world.pos.x + Math_SinS(sSpinAngle) * radius;
    sBallPos.y = player->actor.world.pos.y + BALL_GUARD_HEIGHT;
    sBallPos.z = player->actor.world.pos.z + Math_CosS(sSpinAngle) * radius;
}

static void SwingBall(Player* player) {
    if (IsOneHanded()) {
        OrbitBallAround(player);
        return;
    }
    OrbitBallOverhead(player);
}

static void StartAiming(Player* player, PlayState* play);

static bool IsBallSwungUp(void) {
    return sCharge >= BALL_CHARGE_MIN;
}

static void UpdateSpinning(Player* player, PlayState* play) {
    if (play->state.input[0].press.button & (BTN_A | BTN_B)) {
        sPhase = BALL_HOLDING;
        return;
    }
    if (CanAimBall() && IsBallSwungUp() && (play->state.input[0].press.button & BTN_CUP)) {
        StartAiming(player, play);
        return;
    }
    ReadStickLean(player, play);
    sCharge = MIN(sCharge + 1, BALL_CHARGE_MAX);
    SwingBall(player);
    StrikeAndBreak(player, play);
    FeedTrail(play);
    func_8002F974(&player->actor, NA_SE_IT_SWORD_SWING - SFX_FLAG);
    if (IsButtonHeld(play)) {
        return;
    }
    if (!IsBallSwungUp()) {
        sPhase = BALL_HOLDING;
        return;
    }
    ThrowBall(player, play);
}

// The return value is Link's "upper body is busy" flag: only the spin claims it, or the ball merely carried
// in hand would block his rolls and his other item buttons.
static int32_t UpdateBallInHand(Player* player, PlayState* play) {
    if ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) || player->invincibilityTimer < 0) {
        sPhase = BALL_HOLDING;
    }
    if (sPhase == BALL_HOLDING) {
        HoldBallInHands(player);
        StrikeWithBall(player, play);
        EndTrail(play);
        return 0;
    }
    if (sPhase == BALL_SPINNING) {
        UpdateSpinning(player, play);
        return 1;
    }
    return 0;
}

static void BounceOffWall(PlayState* play, Vec3f* previousPos) {
    CollisionPoly* wall = NULL;
    Vec3f resolved = sBallPos;

    if (!BgCheck_EntitySphVsWall1(&play->colCtx, &resolved, &sBallPos, previousPos, BALL_COLLIDER_RADIUS, &wall,
                                  BALL_WALL_HEIGHT)) {
        return;
    }
    sBallPos = resolved;
    if (fabsf(sBallVelocity.x) + fabsf(sBallVelocity.z) > 1.0f) {
        PlaySfxAt(NA_SE_IT_SHIELD_BOUND, &sBallPos);
    }
    if (wall == NULL) {
        sBallVelocity.x = sBallVelocity.z = 0.0f;
        return;
    }
    f32 normalX = COLPOLY_GET_NORMAL(wall->normal.x);
    f32 normalZ = COLPOLY_GET_NORMAL(wall->normal.z);
    f32 intoWall = sBallVelocity.x * normalX + sBallVelocity.z * normalZ;

    sBallVelocity.x = (sBallVelocity.x - 2.0f * intoWall * normalX) * BALL_WALL_KEEP;
    sBallVelocity.z = (sBallVelocity.z - 2.0f * intoWall * normalZ) * BALL_WALL_KEEP;
}

static void KeepChainTaut(Player* player) {
    f32 dx = sBallPos.x - player->actor.world.pos.x;
    f32 dz = sBallPos.z - player->actor.world.pos.z;
    f32 reach = sqrtf(SQ(dx) + SQ(dz));

    if (reach > BALL_CHAIN_LENGTH) {
        sBallPos.x = player->actor.world.pos.x + dx * (BALL_CHAIN_LENGTH / reach);
        sBallPos.z = player->actor.world.pos.z + dz * (BALL_CHAIN_LENGTH / reach);
        sBallVelocity.x = sBallVelocity.z = 0.0f;
    }
}

static void LandOnFloor(Player* player, PlayState* play) {
    CollisionPoly* floor = NULL;
    s32 floorBgId;
    Vec3f probe = { sBallPos.x, sBallPos.y + 20.0f, sBallPos.z };
    f32 floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &floor, &floorBgId, &player->actor, &probe);

    if (floorY <= BGCHECK_Y_MIN || sBallPos.y - BALL_RADIUS > floorY || sBallVelocity.y > 0.0f) {
        if (sBallPos.y < player->actor.world.pos.y - 500.0f) {
            sPhase = BALL_RETRACTING;
        }
        return;
    }
    sBallPos.y = floorY + BALL_RADIUS;
    PlaySfxAt(NA_SE_IT_HAMMER_HIT, &sBallPos);
    if (++sBounces >= BALL_MAX_BOUNCES || fabsf(sBallVelocity.y) < 3.0f) {
        sBallVelocity.x = sBallVelocity.y = sBallVelocity.z = 0.0f;
        sPhase = BALL_RESTING;
        sRestTimer = BALL_REST_FRAMES;
        return;
    }
    sBallVelocity.y = -sBallVelocity.y * BALL_BOUNCE_KEEP_Y;
    sBallVelocity.x *= BALL_BOUNCE_KEEP_XZ;
    sBallVelocity.z *= BALL_BOUNCE_KEEP_XZ;
}

static void FlyBall(Player* player, PlayState* play) {
    Vec3f previousPos = sBallPos;

    sBallVelocity.y = MAX(sBallVelocity.y + BALL_GRAVITY, -BALL_TERMINAL_FALL);
    sBallPos.x += sBallVelocity.x;
    sBallPos.y += sBallVelocity.y;
    sBallPos.z += sBallVelocity.z;
    KeepChainTaut(player);
    BounceOffWall(play, &previousPos);
    LandOnFloor(player, play);
}

static void ReelBallIn(Player* player, PlayState* play) {
    Vec3f hands = { player->actor.world.pos.x, player->actor.world.pos.y + 45.0f, player->actor.world.pos.z };
    f32 distance = Math_Vec3f_DistXYZ(&sBallPos, &player->actor.world.pos);

    if (distance <= BALL_RETURN_DISTANCE) {
        ReturnBallToHands(player, play);
        return;
    }
    f32 step = BALL_RETRACT_SPEED / MAX(Math_Vec3f_DistXYZ(&sBallPos, &hands), 0.1f);

    sBallPos.x += (hands.x - sBallPos.x) * step;
    sBallPos.y += (hands.y - sBallPos.y) * step;
    sBallPos.z += (hands.z - sBallPos.z) * step;
    func_8002F974(&player->actor, NA_SE_PL_WALK_GROUND - SFX_FLAG);
}

static void BallThrownAction(Player* player, PlayState* play) {
    Player_UpdateUpperBody(player, play);
    Player_ZeroSpeedXZ(player);
    FaceThrowDirection(player);
    if (!IsBallAway() || ++sThrownFrames > BALL_THROW_TIMEOUT) {
        ReturnBallToHands(player, play);
        return;
    }
    switch (sPhase) {
        case BALL_FLYING:
            FlyBall(player, play);
            break;
        case BALL_RESTING:
            if (--sRestTimer <= 0) {
                sPhase = BALL_RETRACTING;
            }
            break;
        case BALL_RETRACTING:
            ReelBallIn(player, play);
            break;
        default:
            ReturnBallToHands(player, play);
            return;
    }
    if (sPhase == BALL_HOLDING) {
        return;
    }
    StrikeAndBreak(player, play);
    if (sPhase != BALL_RESTING) {
        FeedTrail(play);
    }
}

static void TakeOutBall(PlayState* play, Player* player) {
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sBallCollider);
        sIsColliderReady = true;
    }
    Collider_SetCylinder(play, &sBallCollider, &player->actor, &sBallColliderInit);
    sPhase = BALL_HOLDING;
    sTrailIndex = -1;
    HoldBallInHands(player);
}

static void StartSpinning(Player* player, PlayState* play) {
    if (sPhase != BALL_HOLDING) {
        return;
    }
    sPhase = BALL_SPINNING;
    sCharge = 0;
    sThrowYaw = player->actor.shape.rot.y;
    PlaySfxAt(NA_SE_IT_HAMMER_SWING, &player->actor.world.pos);
}

static void BallAimAction(Player* player, PlayState* play);

static void StartAiming(Player* player, PlayState* play) {
    if (!Z64Aiming_Request(player, play)) {
        return;
    }
    sPhase = BALL_AIMING;
    Player_SetupAction(play, player, BallAimAction, 1);
    Player_ZeroSpeedXZ(player);
}

static void StopAiming(Player* player, PlayState* play) {
    Z64Aiming_Release(player, play);
    if (sPhase == BALL_AIMING) {
        sPhase = BALL_SPINNING;
    }
}

static void BallAimAction(Player* player, PlayState* play) {
    Z64Aiming_Update(player, play);
    Player_ZeroSpeedXZ(player);
    Player_UpdateUpperBody(player, play);
    if (sPhase != BALL_AIMING) {
        return;
    }
    sCharge = MIN(sCharge + 1, BALL_CHARGE_MAX);
    SwingBall(player);
    StrikeAndBreak(player, play);
    FeedTrail(play);
    if (!IsButtonHeld(play)) {
        ThrowBall(player, play);
        StopAiming(player, play);
        return;
    }
    if (play->state.input[0].press.button & (BTN_CUP | BTN_A | BTN_B)) {
        StopAiming(player, play);
        func_80839FFC(player, play);
    }
}

static void PutBallAway(Player* player, PlayState* play) {
    if (sPhase == BALL_AIMING) {
        StopAiming(player, play);
    }
    if (player->actionFunc == BallThrownAction || player->actionFunc == BallAimAction) {
        func_80839FFC(player, play);
    }
    EndTrail(play);
    sBallCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    sPhase = BALL_IDLE;
    player->upperLimbRot.x = player->upperLimbRot.y = player->upperLimbRot.z = 0;
    Audio_StopSfxById(NA_SE_IT_SWORD_SWING);
    Audio_StopSfxById(NA_SE_PL_WALK_GROUND);
}

// The queued frame lands in sPoseJoints when the animation queue runs, which is after every actor update and
// before any actor is drawn: posing an arm during the update itself is undone by Link's own animation.
static void RequestArmPose(PlayState* play) {
    bool isHeld = sPhase == BALL_HOLDING;

    AnimationContext_SetLoadFrame(play, (LinkAnimationHeader*)(isHeld ? sHoldPoseAnim : sSpinPoseAnim),
                                  isHeld ? BALL_HOLD_POSE_FRAME : BALL_SPIN_POSE_FRAME, PLAYER_LIMB_MAX, sPoseJoints);
}

// Golden Gauntlets swing it with the throwing arm alone, so the left arm keeps whatever Link is animating.
static void ApplyArmPose(Player* player) {
    u32 first = IsOneHanded() ? ARRAY_COUNT(sArmLimbs) / 2 : 0;

    for (u32 i = first; i < ARRAY_COUNT(sArmLimbs); i++) {
        player->skelAnime.jointTable[sArmLimbs[i]] = sPoseJoints[sArmLimbs[i]];
    }
}

// Iron Boots are what vanilla checks before a fan or a current may push Link: the ball weighs at least as much.
static void AnchorAgainstWind(Player* player) {
    player->pushedSpeed = 0.0f;
}

// The overhead spin orbits in Link's own frame, the one-handed guard orbits in world space.
static s16 GetBallBearing(Player* player) {
    return IsOneHanded() ? (s16)(sSpinAngle - player->actor.shape.rot.y) : sSpinAngle;
}

// The ball drags Link a few degrees after it, harder the faster it goes. Weaker arms take the pull in the
// torso; Golden Gauntlets hold their ground and only the swinging shoulder gives.
static void LeanIntoSwing(Player* player) {
    s16 bearing = GetBallBearing(player);
    f32 pull = BALL_LEAN_MIN + (BALL_LEAN_MAX - BALL_LEAN_MIN) * sCharge / BALL_CHARGE_MAX;
    s16 forwards = (s16)(Math_CosS(bearing) * pull);
    s16 sideways = (s16)(-Math_SinS(bearing) * pull);

    if (IsOneHanded()) {
        Vec3s* shoulder = &player->skelAnime.jointTable[PLAYER_LIMB_R_SHOULDER];

        shoulder->y -= forwards;
        shoulder->z += sideways;
        player->upperLimbRot.x = player->upperLimbRot.y = player->upperLimbRot.z = 0;
        return;
    }
    player->upperLimbRot.x = forwards;
    player->upperLimbRot.z = sideways;
    player->upperLimbRot.y = 0;
}

// Runs just before Link is drawn, the only point in the frame where the joint table is his final pose.
static void PoseArmsAroundBall(Actor* actor, PlayState* play, bool* drawVanilla) {
    Player* player = (Player*)actor;

    if (sPhase == BALL_IDLE) {
        return;
    }
    ApplyArmPose(player);
    if (sPhase == BALL_HOLDING) {
        player->upperLimbRot.x = player->upperLimbRot.y = player->upperLimbRot.z = 0;
        return;
    }
    if (IsBallAway()) {
        player->upperLimbRot.x = BALL_THROW_LEAN;
        player->upperLimbRot.y = player->upperLimbRot.z = 0;
        return;
    }
    LeanIntoSwing(player);
}

static void KeepBallStateSane(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (player == NULL || sPhase == BALL_IDLE) {
        return;
    }
    AnchorAgainstWind(player);
    if (sPhase == BALL_AIMING && player->actionFunc != BallAimAction) {
        StopAiming(player, play);
    }
    if (IsBallAway() && player->actionFunc != BallThrownAction) {
        ReturnBallToHands(player, play);
    }
    RequestArmPose(play);
}

// A and B stop the swing, and vanilla would read the same press as "roll" or "draw the sword". The mod reads
// the raw input, so taking them off Link's copy for those frames costs it nothing.
static void KeepSwingButtons(Player* player, Input* input) {
    if (sPhase == BALL_IDLE || sPhase == BALL_HOLDING) {
        return;
    }
    input->cur.button &= ~(BTN_A | BTN_B | BTN_R);
    input->press.button &= ~(BTN_A | BTN_B | BTN_R);
}

// Whole strands laid end to end keep the links their own size and leave no gap between them.
static void DrawChain(PlayState* play, Vec3f* from, Vec3f* to) {
    f32 dx = to->x - from->x;
    f32 dy = to->y - from->y;
    f32 dz = to->z - from->z;
    f32 span = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));
    f32 yaw = Math_FAtan2F(dx, dz);
    f32 pitch = Math_FAtan2F(-dy, sqrtf(SQ(dx) + SQ(dz)));
    s32 strands = CLAMP((s32)(span / BALL_CHAIN_STRAND_SPAN) + 1, 1, BALL_CHAIN_STRAND_MAX);
    f32 strandSpan = span / strands;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 85, 85, 85, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 40, 40, 40, 255);
    for (s32 i = 0; i < strands; i++) {
        f32 t = (f32)i / strands;

        Matrix_Translate(from->x + dx * t, from->y + dy * t, from->z + dz * t, MTXMODE_NEW);
        Matrix_RotateY(yaw, MTXMODE_APPLY);
        Matrix_RotateX(pitch, MTXMODE_APPLY);
        Matrix_Scale(BALL_CHAIN_THICKNESS, BALL_CHAIN_THICKNESS, strandSpan * BALL_CHAIN_STRETCH, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sChainLinkDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawBall(PlayState* play) {
    bool isSwung = sPhase != BALL_HOLDING;
    f32 scale = isSwung ? BALL_SWUNG_SCALE : BALL_HELD_SCALE;
    s16 tumble = (s16)(play->gameplayFrames * 0x400);

    OPEN_DISPS(play->state.gfxCtx);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BACK | G_LIGHTING);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK | G_LIGHTING | G_ZBUFFER);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_SHADE, G_CC_SHADE);
    Matrix_Translate(sBallPos.x, sBallPos.y, sBallPos.z, MTXMODE_NEW);
    if (isSwung) {
        Matrix_RotateY(BINANG_TO_RAD(tumble), MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(tumble / 2), MTXMODE_APPLY);
    }
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBallDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawBallAndChain(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (sPhase == BALL_IDLE || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    DrawChain(play, &player->bodyPartsPos[PLAYER_BODYPART_L_HAND], &player->bodyPartsPos[PLAYER_BODYPART_R_HAND]);
    DrawChain(play, &player->bodyPartsPos[PLAYER_BODYPART_R_HAND], &sBallPos);
    DrawBall(play);
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(BALL_GIVE_SCALE, BALL_GIVE_SCALE, BALL_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGiveDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    SOHCustomItemDefinition ball = Z64Items_Define(BALL_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&ball, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&ball, 0, 9, 0);
    Z64Items_SetTextbox(&ball, "You got the %rBall and Chain%w!&A massive iron ball that only the&strongest arms "
                               "can swing.^Press %y\xA1%w again and hold it to spin&the ball, then let go to "
                               "%rhurl%w it.^Its weight roots you where %ywind%w&would move anyone else, and&it "
                               "crushes %yarmor%w, shatters %cice%w&and smashes heavy pots.^Stronger arms carry it "
                               "farther:&%gSilver Gauntlets%w free your feet&and your aim, and %gGolden%w ones&swing it "
                               "one-handed to %rguard%w you.");
    Z64Items_SetPauseText(&ball, "%rBall and Chain&%wHold %y\xA1%w to spin, release to throw.&%g\xA4%w or %y\xA5%w to "
                                 "aim.");
    Z64Items_SetCanUse(&ball, CanLiftBall);
    Z64Items_SetAction(&ball, TakeOutBall, UpdateBallInHand);
    Z64Items_SetHeldCallbacks(&ball, StartSpinning, PutBallAway, NULL);
    ball.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    ball.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &ball)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, KeepBallStateSane);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, KeepSwingButtons);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, WeighDownPlayer);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_PLAYER, PoseArmsAroundBall);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawBallAndChain);
}
