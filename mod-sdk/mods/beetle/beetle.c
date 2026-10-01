#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define BEETLE_KEY "nei.beetle"
#define BEETLE_BUTTON_COUNT 8
#define BEETLE_SPEED 12.0f
#define BEETLE_BOOST_SCALE 1.8f
#define BEETLE_RETURN_SPEED 18.0f
#define BEETLE_MAX_DISTANCE 800.0f
#define BEETLE_CATCH_DISTANCE 50.0f
#define BEETLE_LAUNCH_HEIGHT 40.0f
#define BEETLE_LAUNCH_REACH 30.0f
#define BEETLE_COLLIDER_RADIUS 15
#define BEETLE_COLLIDER_HEIGHT 18
#define BEETLE_GRAB_REACH 40.0f
#define BEETLE_FLIGHT_FRAMES 600
#define BEETLE_TURN_RATE 0x360
#define BEETLE_MAX_PITCH 0x3000
#define BEETLE_TARGET_RANGE 700.0f
#define BEETLE_HOMING_STEP 0x180
#define BEETLE_HOMING_STEP_BOOSTED 0x500
#define BEETLE_HOMING_STEP_CHARGING 0x800
#define BEETLE_CAMERA_DISTANCE 120.0f
#define BEETLE_CAMERA_HEIGHT 30.0f
#define BEETLE_MODEL_SCALE 0.06f
#define BEETLE_HELD_SCALE 0.048f
#define BEETLE_HELD_REACH 8.0f
#define BEETLE_GIVE_SCALE 0.5f
#define BEETLE_WING_BEAT 0.7f
#define BEETLE_WING_MIN 0.3f
#define BEETLE_WING_MAX 1.0f
// The silver rupee is an En_G_Switch subtype, and its type lives in the top nibble of its params.
#define BEETLE_SILVER_RUPEE 1

typedef enum {
    BEETLE_STOWED,
    BEETLE_IN_HAND,
    BEETLE_AIMING,
    BEETLE_PILOTED,
    BEETLE_LOOSE,
    BEETLE_RETURNING,
} BeetlePhase;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconBeetleTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gBeetleNameTex";
static const ALIGN_ASSET(2) char sBodyDL[] = "__OTR__objects/object_nei_beetle/g_beetle_body_dl";
static const ALIGN_ASSET(2) char sWingsDL[] = "__OTR__objects/object_nei_beetle/g_beetle_wings_dl";
static const ALIGN_ASSET(2) char sWholeDL[] = "__OTR__objects/object_nei_beetle/g_beetle_dl";
static const ALIGN_ASSET(2) char sWaitAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_boom_throw_waitR";
static const ALIGN_ASSET(2) char sThrowAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_boom_throwR";
static const ALIGN_ASSET(2) char sCatchAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_boom_catch";

static const u16 sItemButtons[BEETLE_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                       BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static ColliderCylinderInit sColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_BOOMERANG, 0x00, 0x01 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_ON },
    { BEETLE_COLLIDER_RADIUS, BEETLE_COLLIDER_HEIGHT, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDrawEnd", "OnSceneInit",
                                              "OnPlayerFilterInput", "OnPlayerResolveItemActionInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static ColliderCylinder sCollider;
static bool sIsColliderReady;
static bool sIsAimPending;
static bool sWasButtonHeld;
static u8 sPhase;
static u8 sAnimatedPhase;
static s16 sFlightTimer;
static Vec3f sBeetlePos;
static Vec3f sLaunchPos;
static Vec3s sBeetleRot;
static f32 sWingSpread = BEETLE_WING_MAX;
static s8 sWingBeat = -1;
static Actor* sCargo;
static Actor* sTarget;
static Actor* sCandidate;
static bool sIsCharging;
static bool sIsAutonomous;
static s16 sCameraId = SUBCAM_FREE;

void Player_ZeroSpeedXZ(Player* player);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static u16 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < BEETLE_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, BEETLE_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool IsButtonHeld(PlayState* play) {
    return (play->state.input[0].cur.button & FindEquippedButtonMask()) != 0;
}

static bool WasButtonPressed(PlayState* play) {
    return (play->state.input[0].press.button & FindEquippedButtonMask()) != 0;
}

static bool IsAway(void) {
    return sPhase >= BEETLE_PILOTED;
}

static void DestroyFlightCamera(PlayState* play) {
    if (sCameraId == SUBCAM_FREE) {
        return;
    }
    Camera_ChangeMode(Play_GetCamera(play, MAIN_CAM), CAM_MODE_NORMAL);
    Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_ACTIVE);
    Play_ClearCamera(play, sCameraId);
    sCameraId = SUBCAM_FREE;
}

static void CreateFlightCamera(PlayState* play) {
    if (sCameraId != SUBCAM_FREE) {
        return;
    }
    sCameraId = Play_CreateSubCamera(play);
    Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_WAIT);
    Play_ChangeCameraStatus(play, sCameraId, CAM_STAT_ACTIVE);
}

// The eye rides behind and above the beetle along its own heading, so steering turns the view with it.
static void FollowBeetleWithCamera(PlayState* play) {
    f32 pitchScale = Math_CosS(sBeetleRot.x);
    Vec3f eye;

    if (sCameraId == SUBCAM_FREE) {
        return;
    }
    eye.x = sBeetlePos.x - Math_SinS(sBeetleRot.y) * pitchScale * BEETLE_CAMERA_DISTANCE;
    eye.y = sBeetlePos.y - BEETLE_CAMERA_HEIGHT + Math_SinS(sBeetleRot.x) * BEETLE_CAMERA_DISTANCE;
    eye.z = sBeetlePos.z - Math_CosS(sBeetleRot.y) * pitchScale * BEETLE_CAMERA_DISTANCE;
    Play_CameraSetAtEye(play, sCameraId, &sBeetlePos, &eye);
}

static void BeatWings(void) {
    sWingSpread += sWingBeat * BEETLE_WING_BEAT;
    if (sWingSpread >= BEETLE_WING_MAX) {
        sWingSpread = BEETLE_WING_MAX;
        sWingBeat = -1;
    } else if (sWingSpread <= BEETLE_WING_MIN) {
        sWingSpread = BEETLE_WING_MIN;
        sWingBeat = 1;
    }
}

static void CarryCargo(void) {
    if (sCargo == NULL) {
        return;
    }
    if (sCargo->update == NULL) {
        sCargo = NULL;
        return;
    }
    Math_Vec3f_Copy(&sCargo->world.pos, &sBeetlePos);
}

// Whatever it was carrying lands on Link: a collectible that kept riding would never be picked up.
static void DropCargo(Player* player) {
    if (sCargo == NULL) {
        return;
    }
    Math_Vec3f_Copy(&sCargo->world.pos, &player->actor.world.pos);
    sCargo->flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
    sCargo->gravity = -0.9f;
    sCargo->bgCheckFlags &= ~3;
    sCargo = NULL;
}

static void Land(Player* player, PlayState* play) {
    DestroyFlightCamera(play);
    DropCargo(player);
    sCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    sTarget = NULL;
    sCandidate = NULL;
    sIsCharging = false;
    sIsAutonomous = false;
    sPhase = BEETLE_IN_HAND;
}

static void StartReturn(Player* player, PlayState* play) {
    DestroyFlightCamera(play);
    player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    player->focusActor = NULL;
    sTarget = NULL;
    sCandidate = NULL;
    sIsCharging = false;
    sPhase = BEETLE_RETURNING;
    PlaySfxAt(NA_SE_PL_CHANGE_ARMS, &sBeetlePos);
}

static void Launch(Player* player, PlayState* play) {
    s16 yaw;
    s16 pitch;

    Z64Aiming_GetDirection(player, &yaw, &pitch);
    Z64Aiming_Release(player, play);
    sBeetlePos.x = player->actor.world.pos.x + Math_SinS(yaw) * BEETLE_LAUNCH_REACH;
    sBeetlePos.y = player->actor.world.pos.y + BEETLE_LAUNCH_HEIGHT;
    sBeetlePos.z = player->actor.world.pos.z + Math_CosS(yaw) * BEETLE_LAUNCH_REACH;
    sBeetleRot.x = pitch;
    sBeetleRot.y = yaw;
    sBeetleRot.z = 0;
    sLaunchPos = player->actor.world.pos;
    sFlightTimer = BEETLE_FLIGHT_FRAMES;
    sCargo = NULL;
    sTarget = NULL;
    sIsCharging = false;
    sPhase = BEETLE_PILOTED;
    Collider_SetCylinder(play, &sCollider, &player->actor, &sColliderInit);
    CreateFlightCamera(play);
    PlaySfxAt(NA_SE_IT_SWORD_SWING, &player->actor.world.pos);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
}

static void StartAiming(Player* player, PlayState* play) {
    if (!Z64Aiming_Request(player, play)) {
        return;
    }
    sWasButtonHeld = false;
    sPhase = BEETLE_AIMING;
}

static void StopAiming(Player* player, PlayState* play) {
    Z64Aiming_Release(player, play);
    sPhase = BEETLE_IN_HAND;
}

static void Fly(f32 speed) {
    f32 pitchScale = Math_CosS(sBeetleRot.x);

    sBeetlePos.x += Math_SinS(sBeetleRot.y) * pitchScale * speed;
    sBeetlePos.y -= Math_SinS(sBeetleRot.x) * speed;
    sBeetlePos.z += Math_CosS(sBeetleRot.y) * pitchScale * speed;
}

// Steers exactly like the first-person aim the launch came out of (func_8084ABD8): the yaw follows the
// NEGATED stick X and the pitch the stick Y, where a positive pitch points down, the same way
// Math_Vec3f_Pitch reports it and the flight reads it. The processed stick is what carries the dead zone,
// and it tops out at 60.
static void SteerWithStick(PlayState* play) {
    f32 stickX = play->state.input[0].rel.stick_x;
    f32 stickY = play->state.input[0].rel.stick_y;

    if (sqrtf(SQ(stickX) + SQ(stickY)) <= 20.0f) {
        return;
    }
    sBeetleRot.y -= (s16)((stickX / 60.0f) * BEETLE_TURN_RATE);
    sBeetleRot.x = CLAMP((s32)(sBeetleRot.x + (s16)((stickY / 60.0f) * BEETLE_TURN_RATE)), -BEETLE_MAX_PITCH,
                         BEETLE_MAX_PITCH);
}

static bool IsCollectible(Actor* actor) {
    return actor->id == ACTOR_EN_ITEM00 || actor->id == ACTOR_EN_SI ||
           (actor->id == ACTOR_EN_G_SWITCH && ((actor->params >> 0xC) & 0xF) == BEETLE_SILVER_RUPEE);
}

static void Grab(Actor* actor) {
    sCargo = actor;
    // Rupee switches and stray fairies only let go of their spot for something that has hooked them.
    actor->flags |= ACTOR_FLAG_HOOKSHOT_ATTACHED;
}

// Its own AT pass never fires on a silver rupee: that actor answers to damage flags the beetle does not
// carry, so anything it can pick up is taken by distance instead.
static void GrabWhatItPasses(PlayState* play) {
    static const u8 sCollectibleCategories[] = { ACTORCAT_ITEMACTION, ACTORCAT_PROP, ACTORCAT_MISC };

    if (sCargo != NULL) {
        return;
    }
    for (u32 i = 0; i < ARRAY_COUNT(sCollectibleCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[sCollectibleCategories[i]].head; actor != NULL;
             actor = actor->next) {
            if (actor->update != NULL && IsCollectible(actor) &&
                Math_Vec3f_DistXYZ(&sBeetlePos, &actor->world.pos) < BEETLE_GRAB_REACH) {
                Grab(actor);
                return;
            }
        }
    }
}

// A hit on anything else ends the flight: the beetle bounces off and heads home, as a thrown item should.
static bool HasStruckSomething(void) {
    Actor* hit = (sCollider.base.atFlags & AT_HIT) ? sCollider.base.at : NULL;

    sCollider.base.atFlags &= ~AT_HIT;
    if (hit == NULL) {
        return false;
    }
    if (IsCollectible(hit)) {
        Grab(hit);
        return false;
    }
    PlaySfxAt(NA_SE_IT_SHIELD_BOUND, &sBeetlePos);
    return true;
}

static bool HasHitGeometry(PlayState* play, Vec3f* previousPos) {
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;
    Vec3f hitPos;

    if (!BgCheck_EntityLineTest1(&play->colCtx, previousPos, &sBeetlePos, &hitPos, &poly, true, true, true, true,
                                 &bgId)) {
        return false;
    }
    sBeetlePos = hitPos;
    PlaySfxAt(NA_SE_IT_SHIELD_BOUND, &sBeetlePos);
    return true;
}

// Only what the beetle can do something about: an enemy the game already lets Link lock onto, a
// collectible it can carry, or a switch a boomerang-strength hit trips.
static bool IsTargetable(Actor* actor) {
    if (actor->category == ACTORCAT_ENEMY || actor->category == ACTORCAT_BOSS) {
        return (actor->flags & ACTOR_FLAG_ATTENTION_ENABLED) != 0;
    }
    if (IsCollectible(actor)) {
        return true;
    }
    if (actor->id == ACTOR_OBJ_SWITCH) {
        u8 kind = actor->params & 7;

        return kind == 2 || kind == 3 || kind == 4;
    }
    return actor->id == ACTOR_BG_BDAN_SWITCH;
}

static Actor* FindTarget(PlayState* play) {
    static const u8 sTargetCategories[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS,   ACTORCAT_ITEMACTION, ACTORCAT_PROP,
                                            ACTORCAT_MISC,  ACTORCAT_SWITCH, ACTORCAT_BG };
    Actor* nearest = NULL;
    f32 nearestDistance = BEETLE_TARGET_RANGE;

    for (u32 i = 0; i < ARRAY_COUNT(sTargetCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[sTargetCategories[i]].head; actor != NULL;
             actor = actor->next) {
            if (actor->update == NULL || !IsTargetable(actor)) {
                continue;
            }
            f32 distance = Math_Vec3f_DistXYZ(&sBeetlePos, &actor->world.pos);

            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = actor;
            }
        }
    }
    return nearest;
}

static void HomeToTarget(s16 step) {
    Math_SmoothStepToS(&sBeetleRot.y, Math_Vec3f_Yaw(&sBeetlePos, &sTarget->world.pos), 4, step, 0x10);
    Math_SmoothStepToS(&sBeetleRot.x, Math_Vec3f_Pitch(&sBeetlePos, &sTarget->world.pos), 4, step, 0x10);
}

static void DropLostTarget(void) {
    if (sTarget != NULL && sTarget->update == NULL) {
        sTarget = NULL;
        sIsCharging = false;
    }
}

// Z locks and unlocks the nearest thing worth hitting; the lock drives vanilla's own reticle through
// focusActor, which survives because this runs after the player's update.
static void HoldTarget(Player* player, PlayState* play) {
    if (play->state.input[0].press.button & BTN_Z) {
        sTarget = sTarget != NULL ? NULL : FindTarget(play);
        sIsCharging = false;
    }
    if (sTarget == NULL) {
        return;
    }
    player->focusActor = sTarget;
    player->zTargetActiveTimer = 15;
}

static bool HasFlownTooFar(void) {
    return Math_Vec3f_DistXYZ(&sBeetlePos, &sLaunchPos) > BEETLE_MAX_DISTANCE || DECR(sFlightTimer) == 0;
}

static void AdvanceFlight(Player* player, PlayState* play, f32 speed) {
    Vec3f previousPos = sBeetlePos;
    bool hasStruck = HasStruckSomething();

    GrabWhatItPasses(play);
    Fly(speed);
    sCollider.dim.pos.x = (s16)sBeetlePos.x;
    sCollider.dim.pos.y = (s16)(sBeetlePos.y - BEETLE_COLLIDER_HEIGHT / 2);
    sCollider.dim.pos.z = (s16)sBeetlePos.z;
    sCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sCollider.base);
    BeatWings();
    CarryCargo();
    func_8002F974(&player->actor, NA_SE_EN_BIRI_FLY - SFX_FLAG);
    if (hasStruck || HasHitGeometry(play, &previousPos) || HasFlownTooFar()) {
        StartReturn(player, play);
    }
}

// Piloted: Link stands still, the stick and the camera belong to the beetle. A flies faster, Z locks on,
// and B hands the camera back and leaves it to finish the job on its own.
static void UpdatePiloted(Player* player, PlayState* play) {
    bool isBoosting = (play->state.input[0].cur.button & BTN_A) != 0;

    DropLostTarget();
    Player_ZeroSpeedXZ(player);
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    if (WasButtonPressed(play)) {
        StartReturn(player, play);
        return;
    }
    if (play->state.input[0].press.button & BTN_B) {
        DestroyFlightCamera(play);
        player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
        player->focusActor = NULL;
        sCandidate = NULL;
        sIsCharging = sTarget != NULL;
        sIsAutonomous = true;
        sAnimatedPhase = BEETLE_STOWED;
        sPhase = BEETLE_LOOSE;
        return;
    }
    HoldTarget(player, play);
    // Nothing locked: remember what Z would take, so the draw can offer it like the game offers its own.
    sCandidate = sTarget == NULL ? FindTarget(play) : NULL;
    SteerWithStick(play);
    if (sTarget != NULL) {
        HomeToTarget(isBoosting ? BEETLE_HOMING_STEP_BOOSTED : BEETLE_HOMING_STEP);
    }
    AdvanceFlight(player, play, isBoosting ? BEETLE_SPEED * BEETLE_BOOST_SCALE : BEETLE_SPEED);
    FollowBeetleWithCamera(play);
}

// Loose: the camera is Link's again and the beetle finishes alone, charging whatever it had locked.
static void UpdateLoose(Player* player, PlayState* play) {
    DropLostTarget();
    if (!sIsCharging || sTarget == NULL) {
        StartReturn(player, play);
        return;
    }
    HomeToTarget(BEETLE_HOMING_STEP_CHARGING);
    AdvanceFlight(player, play, BEETLE_SPEED * BEETLE_BOOST_SCALE);
}

static void UpdateReturning(Player* player, PlayState* play) {
    Vec3f hand = { player->actor.world.pos.x, player->actor.world.pos.y + BEETLE_LAUNCH_HEIGHT,
                   player->actor.world.pos.z };
    f32 distance = Math_Vec3f_DistXYZ(&sBeetlePos, &hand);

    BeatWings();
    CarryCargo();
    if (distance <= BEETLE_CATCH_DISTANCE) {
        // Only a Link who was waiting for it reaches out: one who let it go is busy with his own hands.
        if (!sIsAutonomous) {
            LinkAnimation_PlayOnce(play, &player->upperSkelAnime, (LinkAnimationHeader*)sCatchAnim);
            sAnimatedPhase = BEETLE_RETURNING;
        }
        PlaySfxAt(NA_SE_PL_CATCH_BOOMERANG, &player->actor.world.pos);
        Land(player, play);
        return;
    }
    f32 step = BEETLE_RETURN_SPEED / distance;

    sBeetlePos.x += (hand.x - sBeetlePos.x) * step;
    sBeetlePos.y += (hand.y - sBeetlePos.y) * step;
    sBeetlePos.z += (hand.z - sBeetlePos.z) * step;
    sBeetleRot.y = Math_Vec3f_Yaw(&sBeetlePos, &hand);
    sBeetleRot.x = Math_Vec3f_Pitch(&sBeetlePos, &hand);
    func_8002F974(&player->actor, NA_SE_EN_BIRI_FLY - SFX_FLAG);
}

static void AnimateArm(Player* player, PlayState* play) {
    if (sAnimatedPhase == sPhase) {
        LinkAnimation_Update(play, &player->upperSkelAnime);
        return;
    }
    sAnimatedPhase = sPhase;
    if (sPhase == BEETLE_AIMING) {
        LinkAnimation_PlayLoop(play, &player->upperSkelAnime, (LinkAnimationHeader*)sWaitAnim);
    } else {
        LinkAnimation_PlayOnce(play, &player->upperSkelAnime, (LinkAnimationHeader*)sThrowAnim);
    }
}

// Aiming is the one phase that has to run inside the player's update: it holds the first-person state
// and the rotations the engine walks back to zero every frame they go unclaimed.
static void UpdateAiming(Player* player, PlayState* play) {
    Z64Aiming_Update(player, play);
    Player_ZeroSpeedXZ(player);
    if (play->state.input[0].press.button & (BTN_B | BTN_A)) {
        StopAiming(player, play);
        return;
    }
    if (IsButtonHeld(play)) {
        sWasButtonHeld = true;
        return;
    }
    // Held to aim, released to fly. The button is only read as a release once it has been seen held, or
    // the press that took the beetle out would launch it during the item-change animation.
    if (sWasButtonHeld || WasButtonPressed(play)) {
        Launch(player, play);
    }
}

// The return value is Link's "upper body is busy" flag: a beetle merely resting in his hand must not block
// his rolls or his other items, and neither must one he has handed over — from the moment it flies on its
// own he is free to fight while it finishes, which is the whole point of letting go of it.
static int32_t UpdateBeetleInHand(Player* player, PlayState* play) {
    if (sPhase == BEETLE_IN_HAND || sPhase == BEETLE_STOWED || sIsAutonomous) {
        return 0;
    }
    if (sPhase == BEETLE_AIMING) {
        UpdateAiming(player, play);
    }
    AnimateArm(player, play);
    return 1;
}

static void TakeOutBeetle(PlayState* play, Player* player) {
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sCollider);
        sIsColliderReady = true;
    }
    sPhase = BEETLE_IN_HAND;
    sAnimatedPhase = BEETLE_STOWED;
    sCargo = NULL;
    sIsAimPending = true;
    PlaySfxAt(NA_SE_PL_CHANGE_ARMS, &player->actor.world.pos);
}

// Pressing the button acts from the end of the player's update, never from the item-button pass: the aim
// claims the camera and the first-person state, and what is set from there is undone the same frame.
static void RequestAim(Player* player, PlayState* play) {
    sIsAimPending = true;
}

static void PutBeetleAway(Player* player, PlayState* play) {
    if (sPhase == BEETLE_AIMING) {
        Z64Aiming_Release(player, play);
    }
    if (IsAway()) {
        Land(player, play);
    }
    DestroyFlightCamera(play);
    sCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    sIsAimPending = false;
    sPhase = BEETLE_STOWED;
    Audio_StopSfxById(NA_SE_EN_BIRI_FLY);
}

static bool IsChangingHeldItem(Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_START_CHANGING_HELD_ITEM) != 0;
}

static void RunPendingAim(Player* player, PlayState* play) {
    if (!sIsAimPending || IsChangingHeldItem(player)) {
        return;
    }
    sIsAimPending = false;
    if (player->heldItemAction == PLAYER_IA_CUSTOM && sPhase == BEETLE_IN_HAND) {
        StartAiming(player, play);
    }
}

static bool IsFlightInterrupted(Player* player) {
    return player->invincibilityTimer < 0 || (player->stateFlags1 & PLAYER_STATE1_IN_WATER);
}

// The flight runs here, after the action func: its position, its camera and the lock-on reticle are the
// last word of the frame. The item's upper action does not necessarily run while it is away, so the
// host's held-item watchdog has to be told the beetle is still out.
static void UpdateBeetleWorld(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (play == NULL || player == NULL) {
        return;
    }
    RunPendingAim(player, play);
    if (!IsAway()) {
        return;
    }
    Z64Items_KeepHeld(sApi);
    if (IsFlightInterrupted(player)) {
        StartReturn(player, play);
        return;
    }
    switch (sPhase) {
        case BEETLE_PILOTED:
            UpdatePiloted(player, play);
            break;
        case BEETLE_LOOSE:
            UpdateLoose(player, play);
            break;
        case BEETLE_RETURNING:
            UpdateReturning(player, play);
            break;
        default:
            Land(player, play);
            break;
    }
}

// While piloting, the stick and the action buttons belong to the beetle. The mod reads the raw input, so
// taking them off Link's copy costs it nothing and keeps him from walking away under the flight camera.
static void HoldPilotInput(Player* player, Input* input) {
    if (sPhase != BEETLE_PILOTED) {
        return;
    }
    input->cur.button &= ~(BTN_A | BTN_B | BTN_R | BTN_Z);
    input->press.button &= ~(BTN_A | BTN_B | BTN_R | BTN_Z);
    input->cur.stick_x = 0;
    input->cur.stick_y = 0;
    input->rel.stick_x = 0;
    input->rel.stick_y = 0;
}

// The camera and the actors it was holding belong to the scene that is ending.
static void ForgetScene(int16_t sceneNum) {
    sCameraId = SUBCAM_FREE;
    sCargo = NULL;
    sTarget = NULL;
    sCandidate = NULL;
    sIsAimPending = false;
    sIsCharging = false;
    sIsAutonomous = false;
    sPhase = BEETLE_STOWED;
}

// The wings are drawn off the body's own matrix, so they beat around the beetle wherever it is.
static void DrawBeetleAt(PlayState* play, Vec3f* pos, Vec3s* rot, f32 scale) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(rot->y), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(rot->x), MTXMODE_APPLY);
    Matrix_RotateY(M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBodyDL);
    Matrix_Scale(1.0f, sWingSpread, 1.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sWingsDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The spinning arrow the game offers over a lockable actor is drawn by Interface_Draw over
// targetCtx.unk_94, and the end of Actor_UpdateAll fills that field from a search around LINK, which finds
// nothing while the beetle is off flying. Writing it here, in the draw phase, is what puts the arrow over
// what the beetle can grab: the same world-space arrow, in the right size and the category's colour.
static void OfferCandidate(PlayState* play) {
    if (sCandidate != NULL && sCandidate->update != NULL) {
        play->actorCtx.targetCtx.unk_94 = sCandidate;
    }
}

static void DrawBeetle(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (sPhase == BEETLE_STOWED || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if (IsAway()) {
        OfferCandidate(play);
        DrawBeetleAt(play, &sBeetlePos, &sBeetleRot, BEETLE_MODEL_SCALE);
        return;
    }
    Vec3s handRot = { 0, player->actor.shape.rot.y, 0 };
    Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];

    hand.x += Math_SinS(handRot.y) * BEETLE_HELD_REACH;
    hand.z += Math_CosS(handRot.y) * BEETLE_HELD_REACH;
    DrawBeetleAt(play, &hand, &handRot, BEETLE_HELD_SCALE);
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(BEETLE_GIVE_SCALE, BEETLE_GIVE_SCALE, BEETLE_GIVE_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sWholeDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, GetEquippedCustomItem)) {
        return;
    }

    SOHCustomItemDefinition beetle = Z64Items_Define(BEETLE_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&beetle, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&beetle, 0, 8, 0);
    Z64Items_SetTextbox(&beetle, "You got the %rBeetle%w!&A clockwork flier from the sky above the clouds, and it "
                                 "answers to your hand. Hold %y\xA1%w to aim and let go to fly it: steer with the "
                                 "stick, hurry it with %y\x9F%w, lock on with %g\xA4%w, or leave it to finish alone "
                                 "with %y\xA0%w. It carries %grupees%w home and knocks the wind out of what it hits.");
    Z64Items_SetPauseText(&beetle, "%rBeetle&%wHold %y\xA1%w to aim, release to fly. Steer with the stick, %y\x9F%w to "
                                   "hurry, %g\xA4%w to lock on.");
    Z64Items_SetAction(&beetle, TakeOutBeetle, UpdateBeetleInHand);
    Z64Items_SetHeldCallbacks(&beetle, RequestAim, PutBeetleAway, NULL);
    beetle.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    beetle.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &beetle)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateBeetleWorld);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, HoldPilotInput);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawBeetle);
}
