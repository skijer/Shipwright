#include <math.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define SPINNER_KEY "nei.spinner"
#define SPINNER_BUTTON_COUNT 8
#define SPINNER_CHARGE_MAX 60
#define SPINNER_SPEED_MIN 12.0f
#define SPINNER_SPEED_MAX 25.0f
#define SPINNER_SPEED_HOMING 30.0f
#define SPINNER_HOVER_HEIGHT 17.0f
#define SPINNER_RIDE_FRAMES 120
#define SPINNER_ATTACK_FRAMES 20
#define SPINNER_RECOIL_FRAMES 15
#define SPINNER_HOMING_WINDUP_FRAMES 10
#define SPINNER_HOMING_AIM_FRAMES 5
#define SPINNER_HOMING_FLIGHT_FRAMES 30
#define SPINNER_HOMING_ARC 120.0f
#define SPINNER_ATTACK_RADIUS 30
#define SPINNER_COLLIDER_HEIGHT 16
#define SPINNER_ATTACK_DAMAGE 2
#define SPINNER_HOMING_DAMAGE 4
#define SPINNER_ROCK_REACH 40.0f
#define SPINNER_MODEL_SCALE 0.2f
#define SPINNER_GIVE_SCALE 0.3f
#define SPINNER_PLAYER_GRAVITY -1.2f

typedef enum {
    SPINNER_STOWED,
    SPINNER_IN_HAND,
    SPINNER_CHARGING,
    SPINNER_RIDING,
    SPINNER_ATTACKING,
    SPINNER_HOMING_WINDUP,
    SPINNER_HOMING_AIM,
    SPINNER_HOMING_LAUNCH,
    SPINNER_RECOIL,
} SpinnerPhase;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconSpinnerTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gSpinnerNameTex";
static const ALIGN_ASSET(2) char sSpinnerDL[] = "__OTR__objects/object_nei_spinner_tp/spinner_dl";
static const ALIGN_ASSET(2) char sSpinPose[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_fighter_Lpower_kiru_wait";

static const u16 sItemButtons[SPINNER_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                        BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

// Hammer damage lets small, bronze and brown rocks break through their own vanilla logic, drops and all.
static ColliderCylinderInit sAttackColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_HAMMER_SWING | DMG_SLASH_MASTER, 0x00, SPINNER_ATTACK_DAMAGE },
      { 0, 0, 0 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { SPINNER_ATTACK_RADIUS, SPINNER_COLLIDER_HEIGHT, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDrawEnd", "OnPlayerFilterInput",
                                              "OnSceneInit", "OnPlayerResolveItemActionInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static ColliderCylinder sAttackCollider;
static bool sIsColliderReady;
static bool sIsMountPending;
static u8 sPhase;
static s16 sCharge;
static s16 sTimer;
static s16 sAngle;
static f32 sSpeed;
static Actor* sTarget;

void Player_PlayVoiceSfx(Player* player, u16 sfxId);
s32 Player_UpperAction_ChangeHeldItem(Player* player, PlayState* play);
void Player_ZeroSpeedXZ(Player* player);
void Player_AnimPlayLoop(PlayState* play, Player* player, LinkAnimationHeader* anim);
LinkAnimationHeader* Player_GetIdleAnim(Player* player);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static void SpawnFloorShockwave(Actor* actor, PlayState* play) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f pos = { actor->world.pos.x, actor->floorHeight + 2.0f, actor->world.pos.z };

    EffectSsBlast_SpawnWhiteCustomScale(play, &pos, &zero, &zero, 60, 150, 8);
}

static bool IsRiding(void) {
    return sPhase >= SPINNER_CHARGING;
}

static u16 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < SPINNER_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, SPINNER_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool IsButtonHeld(PlayState* play) {
    return (play->state.input[0].cur.button & FindEquippedButtonMask()) != 0;
}

static void SilenceSpinner(void) {
    Audio_StopSfxById(NA_SE_EV_ROCK_SLIDE);
    Audio_StopSfxById(NA_SE_IT_SHIELD_BOUND);
    Audio_StopSfxById(NA_SE_IT_SWORD_SWING);
    Audio_StopSfxById(NA_SE_IT_HAMMER_SWING);
}

// Riding borrows the spin-attack flags, the same ones vanilla uses to keep the camera and turning off Link's back.
static void HoldRidingFlags(Player* player) {
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
}

// The ride drives Link's own skeleton, and his walking action func only picks an animation when its state
// changes: leaving him mid-spin would keep that pose until something else replaced it.
static void RestoreIdleAnimation(Player* player, PlayState* play) {
    player->skelAnime.playSpeed = 1.0f;
    Player_AnimPlayLoop(play, player, Player_GetIdleAnim(player));
}

static void Dismount(Player* player, PlayState* play) {
    RestoreIdleAnimation(player, play);
    sPhase = SPINNER_IN_HAND;
    sTarget = NULL;
    sAttackCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    player->actor.gravity = SPINNER_PLAYER_GRAVITY;
    player->actor.shape.rot.x = 0;
    player->actor.shape.rot.z = 0;
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    SilenceSpinner();
    PlaySfxAt(NA_SE_PL_LAND, &player->actor.world.pos);
}

// Riding hovers off the floor, so the ground check only guards mounting: without this the engine would
// reject the second press and the spin attack would never fire.
static bool CanUseSpinner(Player* player, PlayState* play) {
    if (IsRiding()) {
        return true;
    }
    return (player->actor.bgCheckFlags & 1) && player->meleeWeaponState == 0 &&
           !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON));
}

static void Mount(Player* player, PlayState* play) {
    if (IsRiding() || !CanUseSpinner(player, play)) {
        return;
    }
    Collider_SetCylinder(play, &sAttackCollider, &player->actor, &sAttackColliderInit);
    sAttackCollider.base.atFlags &= ~AT_ON;
    sPhase = SPINNER_CHARGING;
    sCharge = 0;
    player->actor.velocity.y = 8.0f;
    PlaySfxAt(NA_SE_IT_SWORD_SWING, &player->actor.world.pos);
}

static void PlaySpinPose(Player* player, PlayState* play, f32 speed) {
    LinkAnimationHeader* pose = (LinkAnimationHeader*)sSpinPose;

    if (player->skelAnime.animation != pose) {
        LinkAnimation_Change(play, &player->skelAnime, pose, speed, 0.0f, Animation_GetLastFrame(pose), ANIMMODE_LOOP,
                             -4.0f);
    }
    player->skelAnime.playSpeed = speed;
}

static void HoverAboveFloor(Player* player, f32 lift) {
    Math_StepToF(&player->actor.world.pos.y, player->actor.floorHeight + SPINNER_HOVER_HEIGHT + lift, 4.0f);
    player->actor.velocity.y = 0.0f;
    player->actor.gravity = 0.0f;
}

static void MoveAlong(Player* player, s16 angle, f32 speed) {
    player->actor.world.pos.x += Math_SinS(angle) * speed;
    player->actor.world.pos.z += Math_CosS(angle) * speed;
}

static void BounceOffWalls(Player* player) {
    if (player->actor.bgCheckFlags & 8) {
        sAngle += 0x8000;
        PlaySfxAt(NA_SE_IT_SHIELD_BOUND, &player->actor.world.pos);
    }
}

static void SteerWithStick(PlayState* play, f32 deadZone, f32 rate) {
    f32 stickX = play->state.input[0].rel.stick_x;

    if (fabsf(stickX) > deadZone) {
        sAngle += (s16)(stickX * rate);
    }
}

static void LeaveSpinnerAt(Player* player, PlayState* play, Actor* cucco) {
    s16 awayYaw = Math_Vec3f_Yaw(&cucco->world.pos, &player->actor.world.pos);

    Dismount(player, play);
    MoveAlong(player, awayYaw, 50.0f);
    player->actor.velocity.y = 8.0f;
    Audio_PlayActorSound2(&player->actor, NA_SE_PL_DAMAGE);
    Audio_PlayActorSound2(cucco, NA_SE_EV_CHICKEN_CRY_M);
}

static bool IsWithinXZ(Actor* actor, Player* player, f32 radius) {
    f32 dx = actor->world.pos.x - player->actor.world.pos.x;
    f32 dz = actor->world.pos.z - player->actor.world.pos.z;

    return SQ(dx) + SQ(dz) < SQ(radius);
}

static bool KnockedOffByCucco(Player* player, PlayState* play) {
    static const u8 sCuccoCategories[] = { ACTORCAT_PROP, ACTORCAT_ENEMY };

    for (u32 i = 0; i < ARRAY_COUNT(sCuccoCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[sCuccoCategories[i]].head; actor != NULL; actor = actor->next) {
            if (actor->update != NULL && (actor->id == ACTOR_EN_NIW || actor->id == ACTOR_EN_ATTACK_NIW) &&
                IsWithinXZ(actor, player, SPINNER_ATTACK_RADIUS + 30.0f)) {
                LeaveSpinnerAt(player, play, actor);
                return true;
            }
        }
    }
    return false;
}

static void ShatterRollingBoulders(Player* player, PlayState* play) {
    Actor* next;

    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_PROP].head; actor != NULL; actor = next) {
        next = actor->next;
        if (actor->update == NULL || actor->id != ACTOR_EN_GOROIWA ||
            !IsWithinXZ(actor, player, SPINNER_ATTACK_RADIUS + SPINNER_ROCK_REACH)) {
            continue;
        }
        func_80033480(play, &actor->world.pos, 140.0f, 6, 180, 90, 1);
        func_80033480(play, &actor->world.pos, 140.0f, 12, 80, 90, 1);
        SoundSource_PlaySfxAtFixedWorldPos(play, &actor->world.pos, 40, NA_SE_EV_WALL_BROKEN);
        Actor_Kill(actor);
    }
}

static bool StrikeAround(Player* player, PlayState* play, u8 damage) {
    sAttackCollider.dim.pos.x = (s16)player->actor.world.pos.x;
    sAttackCollider.dim.pos.y = (s16)(player->actor.world.pos.y - 25.0f + SPINNER_HOVER_HEIGHT);
    sAttackCollider.dim.pos.z = (s16)player->actor.world.pos.z;
    sAttackCollider.info.toucher.damage = damage;
    sAttackCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    if (sAttackCollider.base.atFlags & AT_HIT) {
        sAttackCollider.base.atFlags &= ~AT_HIT;
        PlaySfxAt(NA_SE_IT_HAMMER_HIT, &player->actor.world.pos);
    }
    CollisionCheck_SetAT(play, &play->colChkCtx, &sAttackCollider.base);
    ShatterRollingBoulders(player, play);
    return KnockedOffByCucco(player, play);
}

static void Launch(Player* player) {
    sSpeed = SPINNER_SPEED_MIN + (SPINNER_SPEED_MAX - SPINNER_SPEED_MIN) * ((f32)sCharge / SPINNER_CHARGE_MAX);
    player->actor.velocity.y = 6.0f;
    if (player->focusActor != NULL) {
        sTarget = player->focusActor;
        sAngle = Math_Vec3f_Yaw(&player->actor.world.pos, &sTarget->world.pos);
        sTimer = 0;
        sPhase = SPINNER_HOMING_WINDUP;
        PlaySfxAt(NA_SE_IT_SWORD_PUTAWAY, &player->actor.world.pos);
        return;
    }
    sAngle = player->actor.shape.rot.y;
    sTimer = SPINNER_RIDE_FRAMES;
    sPhase = SPINNER_RIDING;
    PlaySfxAt(NA_SE_IT_HAMMER_SWING, &player->actor.world.pos);
}

static void UpdateCharging(Player* player, PlayState* play) {
    if (IsButtonHeld(play)) {
        sCharge = MIN(sCharge + 1, SPINNER_CHARGE_MAX);
    }
    HoverAboveFloor(player, 0.0f);
    PlaySpinPose(player, play, 1.0f);
    if ((play->gameplayFrames % 20) == 0) {
        PlaySfxAt(NA_SE_IT_SHIELD_BOUND, &player->actor.world.pos);
    }
    if (!IsButtonHeld(play)) {
        Launch(player);
    }
}

static void StartAttack(Player* player, PlayState* play) {
    sPhase = SPINNER_ATTACKING;
    sTimer = SPINNER_ATTACK_FRAMES;
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_MAGIC_ATTACK);
    PlaySfxAt(NA_SE_IT_HAMMER_SWING, &player->actor.world.pos);
    SpawnFloorShockwave(&player->actor, play);
}

static void UpdateRiding(Player* player, PlayState* play) {
    PlaySpinPose(player, play, 1.0f);
    SteerWithStick(play, 10.0f, 6.0f);
    BounceOffWalls(player);
    MoveAlong(player, sAngle, sSpeed);
    player->yaw = player->actor.world.rot.y = player->actor.shape.rot.y = sAngle;
    HoverAboveFloor(player, 0.0f);
    if ((play->gameplayFrames % 20) == 0) {
        PlaySfxAt(NA_SE_EV_ROCK_SLIDE, &player->actor.world.pos);
    }
    if (--sTimer <= 0) {
        Dismount(player, play);
    }
}

static void UpdateAttacking(Player* player, PlayState* play) {
    if (--sTimer <= 0) {
        sAttackCollider.base.atFlags &= ~AT_ON;
        sTimer = SPINNER_RIDE_FRAMES;
        sPhase = SPINNER_RIDING;
        return;
    }
    PlaySpinPose(player, play, 3.0f);
    SteerWithStick(play, 15.0f, 3.0f);
    BounceOffWalls(player);
    MoveAlong(player, sAngle, sSpeed * 1.3f);
    player->yaw = player->actor.world.rot.y = player->actor.shape.rot.y = sAngle;
    HoverAboveFloor(player, 10.0f);
    if ((play->gameplayFrames % 8) == 0) {
        PlaySfxAt(NA_SE_IT_SWORD_SWING, &player->actor.world.pos);
    }
    StrikeAround(player, play, SPINNER_ATTACK_DAMAGE);
}

static void TrackTarget(Player* player) {
    if (sTarget != NULL && sTarget->update != NULL) {
        sAngle = Math_Vec3f_Yaw(&player->actor.world.pos, &sTarget->world.pos);
    }
}

static void UpdateHomingWindup(Player* player, PlayState* play) {
    HoverAboveFloor(player, 0.0f);
    if ((play->gameplayFrames % 8) == 0) {
        PlaySfxAt(NA_SE_IT_SHIELD_BOUND, &player->actor.world.pos);
    }
    if (++sTimer >= SPINNER_HOMING_WINDUP_FRAMES) {
        sTimer = 0;
        sPhase = SPINNER_HOMING_AIM;
        PlaySfxAt(NA_SE_IT_SWORD_SWING_HARD, &player->actor.world.pos);
    }
}

static void UpdateHomingAim(Player* player, PlayState* play) {
    HoverAboveFloor(player, 0.0f);
    TrackTarget(player);
    if (++sTimer >= SPINNER_HOMING_AIM_FRAMES) {
        sTimer = 0;
        sSpeed = SPINNER_SPEED_HOMING;
        sPhase = SPINNER_HOMING_LAUNCH;
        PlaySfxAt(NA_SE_IT_HAMMER_SWING, &player->actor.world.pos);
    }
}

static bool HasReachedTarget(Player* player) {
    return sTarget != NULL && sTarget->update != NULL &&
           Math_Vec3f_DistXYZ(&player->actor.world.pos, &sTarget->world.pos) < SPINNER_ATTACK_RADIUS;
}

static void StartRecoil(void) {
    sTarget = NULL;
    sTimer = SPINNER_RECOIL_FRAMES;
    sPhase = SPINNER_RECOIL;
}

static void UpdateHomingLaunch(Player* player, PlayState* play) {
    f32 progress = MIN(++sTimer / (f32)SPINNER_HOMING_FLIGHT_FRAMES, 1.0f);

    HoverAboveFloor(player, Math_SinS((s16)(progress * 0x8000)) * SPINNER_HOMING_ARC);
    PlaySpinPose(player, play, 3.0f);
    TrackTarget(player);
    MoveAlong(player, sAngle, sSpeed);
    if (StrikeAround(player, play, SPINNER_HOMING_DAMAGE)) {
        return;
    }
    if (HasReachedTarget(player)) {
        SpawnFloorShockwave(sTarget, play);
        PlaySfxAt(NA_SE_IT_HAMMER_HIT, &sTarget->world.pos);
        StartRecoil();
        return;
    }
    if (sTimer > SPINNER_HOMING_FLIGHT_FRAMES) {
        StartRecoil();
        return;
    }
    if ((play->gameplayFrames % 6) == 0) {
        PlaySfxAt(NA_SE_EV_ROCK_SLIDE, &player->actor.world.pos);
    }
}

static void UpdateRecoil(Player* player, PlayState* play) {
    if (--sTimer <= 0) {
        Dismount(player, play);
        return;
    }
    sAttackCollider.base.atFlags &= ~AT_ON;
    PlaySpinPose(player, play, 0.5f);
    sSpeed *= 0.9f;
    MoveAlong(player, sAngle + 0x8000, sSpeed * 0.3f);
    HoverAboveFloor(player, 0.0f);
}

static bool IsRideInterrupted(Player* player) {
    return player->invincibilityTimer < 0 || (player->stateFlags1 & PLAYER_STATE1_IN_WATER) ||
           player->meleeWeaponState != 0;
}

static bool WasDismountPressed(PlayState* play) {
    return (play->state.input[0].press.button & (BTN_A | BTN_B)) != 0;
}

// The upper action, the same slot vanilla items use: Link keeps his own action func and the spinner only
// pins his speed, height and heading while the ride lasts.
// The return value is Link's "upper body is busy" flag: claiming it while merely holding the spinner would
// block his rolls and his other item buttons for as long as it stayed in hand.
static int32_t UpdateSpinnerInHand(Player* player, PlayState* play) {
    if (!IsRiding()) {
        return 0;
    }
    if (IsRideInterrupted(player) || WasDismountPressed(play)) {
        Dismount(player, play);
        return 1;
    }
    HoldRidingFlags(player);
    Player_ZeroSpeedXZ(player);
    switch (sPhase) {
        case SPINNER_CHARGING:
            UpdateCharging(player, play);
            break;
        case SPINNER_RIDING:
            UpdateRiding(player, play);
            break;
        case SPINNER_ATTACKING:
            UpdateAttacking(player, play);
            break;
        case SPINNER_HOMING_WINDUP:
            UpdateHomingWindup(player, play);
            break;
        case SPINNER_HOMING_AIM:
            UpdateHomingAim(player, play);
            break;
        case SPINNER_HOMING_LAUNCH:
            UpdateHomingLaunch(player, play);
            break;
        case SPINNER_RECOIL:
            UpdateRecoil(player, play);
            break;
        default:
            Dismount(player, play);
            break;
    }
    return 1;
}

// Taking the spinner out runs the item-change animation over the body, so the first ride waits for it.
static void TakeOutSpinner(PlayState* play, Player* player) {
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sAttackCollider);
        sIsColliderReady = true;
    }
    sPhase = SPINNER_IN_HAND;
    sIsMountPending = true;
}

static void RideOrAttack(Player* player, PlayState* play) {
    if (sPhase == SPINNER_IN_HAND) {
        Mount(player, play);
        return;
    }
    if (sPhase == SPINNER_RIDING) {
        StartAttack(player, play);
    }
}

static void PutSpinnerAway(Player* player, PlayState* play) {
    if (IsRiding()) {
        Dismount(player, play);
    }
    sIsMountPending = false;
    sPhase = SPINNER_STOWED;
}

static bool IsChangingHeldItem(Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_START_CHANGING_HELD_ITEM) ||
           player->upperActionFunc == Player_UpperAction_ChangeHeldItem;
}

static void MountWhenReady(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (play == NULL || player == NULL || !sIsMountPending || IsChangingHeldItem(player)) {
        return;
    }
    sIsMountPending = false;
    if (player->heldItemAction == PLAYER_IA_CUSTOM && sPhase == SPINNER_IN_HAND) {
        Mount(player, play);
    }
}

// The ride hovers, and in the air vanilla reads A and B as "put the item away" or "draw the sword" on the
// same frame the mod reads them to get off. They only stay off Link's copy while the ride lasts.
static void KeepRideButtons(Player* player, Input* input) {
    if (!IsRiding()) {
        return;
    }
    input->cur.button &= ~(BTN_A | BTN_B | BTN_R);
    input->press.button &= ~(BTN_A | BTN_B | BTN_R);
}

// The ride belongs to the scene that is ending: its phase must not outlive it holding buttons hostage.
static void ForgetScene(int16_t sceneNum) {
    sIsMountPending = false;
    sPhase = SPINNER_STOWED;
}

static void DrawSpinner(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (!IsRiding() || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD((s16)(play->gameplayFrames * 0x800)), MTXMODE_APPLY);
    Matrix_Scale(SPINNER_MODEL_SCALE, SPINNER_MODEL_SCALE, SPINNER_MODEL_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sSpinnerDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(SPINNER_GIVE_SCALE, SPINNER_GIVE_SCALE, SPINNER_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sSpinnerDL);
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

    SOHCustomItemDefinition spinner = Z64Items_Define(SPINNER_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&spinner, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&spinner, 0, 1, 0);
    Z64Items_SetTextbox(&spinner, "You got the %rSpinner%w!&An ancient spinning top that&rides on its own grinding "
                                  "force.^Hold %y\xA1%w to wind it up, then let go&to ride. Press %y\xA1%w again to "
                                  "%rspin&attack%w.^Let go while %yZ-targeting%w and it&homes in on your foe.");
    Z64Items_SetPauseText(&spinner, "%rSpinner&%wHold %y\xA1%w to charge, release to ride.&Press %y\xA1%w to spin "
                                    "attack.");
    Z64Items_SetCanUse(&spinner, CanUseSpinner);
    Z64Items_SetAction(&spinner, TakeOutSpinner, UpdateSpinnerInHand);
    Z64Items_SetHeldCallbacks(&spinner, RideOrAttack, PutSpinnerAway, NULL);
    spinner.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    spinner.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &spinner)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, MountWhenReady);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, KeepRideButtons);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawSpinner);
}
