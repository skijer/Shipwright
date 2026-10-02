#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define SWITCH_HOOK_KEY "nei.switch_hook"
#define SWITCH_HOOK_BUTTON_COUNT 8
#define SWITCH_HOOK_SPEED 20.0f
#define SWITCH_HOOK_FLIGHT_FRAMES 26
#define SWITCH_HOOK_RETURN_SPEED 40.0f
#define SWITCH_HOOK_ARRIVE_DISTANCE 30.0f
#define SWITCH_HOOK_HAND_HEIGHT 45.0f
#define SWITCH_HOOK_SELECT_RANGE 520.0f
#define SWITCH_HOOK_SELECT_CONE 0x1800
#define SWITCH_HOOK_SELECT_MIN_DISTANCE 30.0f
#define SWITCH_HOOK_GRAB_DISTANCE 60.0f
#define SWITCH_HOOK_BLIND_GRAB_DISTANCE 45.0f
#define SWITCH_HOOK_ARMED_DISTANCE 50.0f
#define SWITCH_HOOK_HOLD_FRAMES 2
#define SWITCH_HOOK_SETTLE_FRAMES 40
#define SWITCH_HOOK_TINT_FRAMES 80
#define SWITCH_HOOK_INVINCIBLE_FRAMES 20
#define SWITCH_HOOK_MAX_CHARGES 5
#define SWITCH_HOOK_RECHARGE_FRAMES 400
#define SWITCH_HOOK_DEPLETED_FRAMES 2400
#define SWITCH_HOOK_ANCHOR_SLOTS 2
#define SWITCH_HOOK_ANCHORS_PER_ACTOR 16
#define SWITCH_HOOK_CHAIN_LINK_SPACING 15.0f
#define SWITCH_HOOK_CHAIN_LINK_MAX 40
#define SWITCH_HOOK_CHAIN_SCALE 0.015f
#define SWITCH_HOOK_TIP_SCALE 0.015f
#define SWITCH_HOOK_HELD_SCALE 0.01f
#define SWITCH_HOOK_GIVE_SCALE 0.01f

typedef enum {
    SWITCH_HOOK_STOWED,
    SWITCH_HOOK_IN_HAND,
    SWITCH_HOOK_AIMING,
    SWITCH_HOOK_FLYING,
    SWITCH_HOOK_SWAPPING,
    SWITCH_HOOK_RETURNING,
} SwitchHookPhase;

typedef struct {
    Collider* collider;
    Vec3f offset;
    Vec3f lastSet;
    bool hasLastSet;
} ColliderAnchor;

// A swapped actor keeps its hitboxes where it used to stand: they are world-space geometry that only
// the owner refreshes, and many actors build theirs once, in their init.
typedef struct {
    Actor* owner;
    bool isActive;
    u8 count;
    Vec3f focusOffset;
    Vec3f focusLastSet;
    bool hasFocusLast;
    ColliderAnchor anchors[SWITCH_HOOK_ANCHORS_PER_ACTOR];
} AnchorSlot;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconSwitchHookTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gSwitchHookNameTex";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_switchhook/gSwitchHookGiveDL";
static const ALIGN_ASSET(2) char sTipDL[] = "__OTR__objects/object_link_boy/gLinkAdultHookshotTipDL";
static const ALIGN_ASSET(2) char sChainLinkDL[] = "__OTR__objects/object_link_boy/gLinkAdultHookshotChainDL";
static const ALIGN_ASSET(2) char sAimAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_hook_shot_ready";

static const u16 sItemButtons[SWITCH_HOOK_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                            BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const u8 sSwappableCategories[] = { ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_CHEST, ACTORCAT_NPC };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDrawEnd", "OnSceneInit",
                                              "OnPlayerResolveItemActionInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static u8 sPhase;
static bool sIsAimPending;
static s16 sFlightTimer;
static s16 sHoldTimer;
static s16 sSettleTimer;
static s16 sShotYaw;
static s16 sShotPitch;
static Vec3f sHookPos;
static Actor* sSelection;
static Actor* sSwapTarget;
static u8 sCharges = SWITCH_HOOK_MAX_CHARGES;
static s16 sRechargeTimer;
static s16 sDepletedTimer;
static AnchorSlot sAnchorSlots[SWITCH_HOOK_ANCHOR_SLOTS];

s32 Player_UpdateUpperBody(Player* player, PlayState* play);
s32 Player_UpperAction_ChangeHeldItem(Player* player, PlayState* play);
s32 Player_IsZTargeting(Player* player);
void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static u16 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < SWITCH_HOOK_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, SWITCH_HOOK_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

// Spending the last charge greys the hook out for two minutes, and it comes back full rather than by one.
static void TickCharges(void) {
    if (sDepletedTimer > 0) {
        if (--sDepletedTimer == 0) {
            sCharges = SWITCH_HOOK_MAX_CHARGES;
        }
        return;
    }
    if (sCharges >= SWITCH_HOOK_MAX_CHARGES || ++sRechargeTimer < SWITCH_HOOK_RECHARGE_FRAMES) {
        return;
    }
    sRechargeTimer = 0;
    sCharges++;
}

static bool SpendCharge(void) {
    if (sDepletedTimer > 0 || sCharges == 0) {
        return false;
    }
    sRechargeTimer = 0;
    if (--sCharges == 0) {
        sDepletedTimer = SWITCH_HOOK_DEPLETED_FRAMES;
    }
    return true;
}

static bool IsSwappable(Actor* actor) {
    for (u32 i = 0; i < ARRAY_COUNT(sSwappableCategories); i++) {
        if (actor->category == sSwappableCategories[i]) {
            return actor->update != NULL;
        }
    }
    return false;
}

// Whatever sits closest to the line Link is facing wins, height ignored: the hook aims itself, so the
// player only has to look roughly at what they want to trade places with.
static Actor* ScanForSelection(Player* player, PlayState* play) {
    Actor* best = NULL;
    s16 bestError = SWITCH_HOOK_SELECT_CONE;

    for (u32 i = 0; i < ARRAY_COUNT(sSwappableCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[sSwappableCategories[i]].head; actor != NULL;
             actor = actor->next) {
            f32 dx = actor->world.pos.x - player->actor.world.pos.x;
            f32 dz = actor->world.pos.z - player->actor.world.pos.z;
            f32 distance = sqrtf(SQ(dx) + SQ(dz));

            if (!IsSwappable(actor) || distance < SWITCH_HOOK_SELECT_MIN_DISTANCE ||
                distance > SWITCH_HOOK_SELECT_RANGE) {
                continue;
            }
            s16 error = ABS((s16)(Math_Atan2S(dz, dx) - player->actor.shape.rot.y));

            if (error < bestError) {
                bestError = error;
                best = actor;
            }
        }
    }
    return best;
}

static Actor* FindSwappableNear(PlayState* play, Vec3f* pos, f32 radius) {
    for (u32 i = 0; i < ARRAY_COUNT(sSwappableCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[sSwappableCategories[i]].head; actor != NULL;
             actor = actor->next) {
            if (IsSwappable(actor) && Math_Vec3f_DistXYZ(pos, &actor->world.pos) < radius) {
                return actor;
            }
        }
    }
    return NULL;
}

static bool IsActorAlive(PlayState* play, Actor* actor) {
    if (actor == NULL) {
        return false;
    }
    for (u8 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* other = play->actorCtx.actorLists[category].head; other != NULL; other = other->next) {
            if (other == actor) {
                return true;
            }
        }
    }
    return false;
}

// Rounded, not truncated: the s16 shapes would lose up to a unit per frame and lag behind the model.
static s16 RoundToS16(f32 value) {
    return (s16)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

static void ShiftCollider(Collider* collider, Vec3f* delta) {
    s16 dx = RoundToS16(delta->x);
    s16 dy = RoundToS16(delta->y);
    s16 dz = RoundToS16(delta->z);

    switch (collider->shape) {
        case COLSHAPE_JNTSPH: {
            ColliderJntSph* jntSph = (ColliderJntSph*)collider;

            for (s32 i = 0; i < jntSph->count; i++) {
                Sphere16* sphere = &jntSph->elements[i].dim.worldSphere;

                sphere->center.x += dx;
                sphere->center.y += dy;
                sphere->center.z += dz;
            }
            break;
        }
        case COLSHAPE_CYLINDER: {
            ColliderCylinder* cylinder = (ColliderCylinder*)collider;

            cylinder->dim.pos.x += dx;
            cylinder->dim.pos.y += dy;
            cylinder->dim.pos.z += dz;
            break;
        }
        case COLSHAPE_TRIS: {
            ColliderTris* tris = (ColliderTris*)collider;

            for (s32 i = 0; i < tris->count; i++) {
                TriNorm* tri = &tris->elements[i].dim;

                for (s32 vertex = 0; vertex < 3; vertex++) {
                    tri->vtx[vertex].x += delta->x;
                    tri->vtx[vertex].y += delta->y;
                    tri->vtx[vertex].z += delta->z;
                }
                // The plane is n . p + originDist = 0: without this every test still answers against
                // the triangle's original plane.
                tri->plane.originDist -= tri->plane.normal.x * delta->x + tri->plane.normal.y * delta->y +
                                         tri->plane.normal.z * delta->z;
            }
            break;
        }
        case COLSHAPE_QUAD: {
            ColliderQuad* quad = (ColliderQuad*)collider;

            for (s32 i = 0; i < 4; i++) {
                quad->dim.quad[i].x += delta->x;
                quad->dim.quad[i].y += delta->y;
                quad->dim.quad[i].z += delta->z;
            }
            // The cached edge midpoints are world space too, and they drive the quad-vs-quad tests.
            quad->dim.dcMid.x += dx;
            quad->dim.dcMid.y += dy;
            quad->dim.dcMid.z += dz;
            quad->dim.baMid.x += dx;
            quad->dim.baMid.y += dy;
            quad->dim.baMid.z += dz;
            break;
        }
        default:
            break;
    }
}

static bool GetColliderReference(Collider* collider, Vec3f* out) {
    switch (collider->shape) {
        case COLSHAPE_JNTSPH: {
            ColliderJntSph* jntSph = (ColliderJntSph*)collider;

            if (jntSph->count <= 0 || jntSph->elements == NULL) {
                return false;
            }
            out->x = jntSph->elements[0].dim.worldSphere.center.x;
            out->y = jntSph->elements[0].dim.worldSphere.center.y;
            out->z = jntSph->elements[0].dim.worldSphere.center.z;
            return true;
        }
        case COLSHAPE_CYLINDER: {
            ColliderCylinder* cylinder = (ColliderCylinder*)collider;

            out->x = cylinder->dim.pos.x;
            out->y = cylinder->dim.pos.y;
            out->z = cylinder->dim.pos.z;
            return true;
        }
        case COLSHAPE_TRIS: {
            ColliderTris* tris = (ColliderTris*)collider;

            if (tris->count <= 0 || tris->elements == NULL) {
                return false;
            }
            *out = tris->elements[0].dim.vtx[0];
            return true;
        }
        case COLSHAPE_QUAD:
            *out = ((ColliderQuad*)collider)->dim.quad[0];
            return true;
        default:
            return false;
    }
}

static void CollectColliders(AnchorSlot* slot, Collider** list, s32 count) {
    for (s32 i = 0; i < count && slot->count < SWITCH_HOOK_ANCHORS_PER_ACTOR; i++) {
        Vec3f reference;

        if (list[i] == NULL || list[i]->actor != slot->owner || !GetColliderReference(list[i], &reference)) {
            continue;
        }
        for (u8 known = 0; known < slot->count; known++) {
            if (slot->anchors[known].collider == list[i]) {
                goto next;
            }
        }
        slot->anchors[slot->count].collider = list[i];
        slot->anchors[slot->count].offset.x = reference.x - slot->owner->world.pos.x;
        slot->anchors[slot->count].offset.y = reference.y - slot->owner->world.pos.y;
        slot->anchors[slot->count].offset.z = reference.z - slot->owner->world.pos.z;
        slot->anchors[slot->count].hasLastSet = false;
        slot->count++;
    next:;
    }
}

// Snapshot the actor at rest: the offsets have to describe where its hitboxes sit before the teleport.
static void CaptureColliders(PlayState* play, u8 slotIndex, Actor* actor) {
    AnchorSlot* slot = &sAnchorSlots[slotIndex];

    slot->owner = actor;
    slot->isActive = true;
    slot->count = 0;
    slot->hasFocusLast = false;
    slot->focusOffset.x = actor->focus.pos.x - actor->world.pos.x;
    slot->focusOffset.y = actor->focus.pos.y - actor->world.pos.y;
    slot->focusOffset.z = actor->focus.pos.z - actor->world.pos.z;
    CollectColliders(slot, play->colChkCtx.colAT, play->colChkCtx.colATCount);
    CollectColliders(slot, play->colChkCtx.colAC, play->colChkCtx.colACCount);
    CollectColliders(slot, play->colChkCtx.colOC, play->colChkCtx.colOCCount);
}

static void ReanchorOne(AnchorSlot* slot, ColliderAnchor* anchor) {
    Vec3f reference;

    if (!GetColliderReference(anchor->collider, &reference)) {
        return;
    }
    // The owner rebuilt this collider since the last pass, so its answer wins and the offset follows it.
    if (anchor->hasLastSet && (reference.x != anchor->lastSet.x || reference.y != anchor->lastSet.y ||
                               reference.z != anchor->lastSet.z)) {
        anchor->offset.x = reference.x - slot->owner->world.pos.x;
        anchor->offset.y = reference.y - slot->owner->world.pos.y;
        anchor->offset.z = reference.z - slot->owner->world.pos.z;
        anchor->lastSet = reference;
        return;
    }
    Vec3f delta = { slot->owner->world.pos.x + anchor->offset.x - reference.x,
                    slot->owner->world.pos.y + anchor->offset.y - reference.y,
                    slot->owner->world.pos.z + anchor->offset.z - reference.z };

    ShiftCollider(anchor->collider, &delta);
    // Record where it actually landed: the s16 shapes round, and the ideal would read as "the owner
    // moved it" on the very next frame.
    anchor->hasLastSet = GetColliderReference(anchor->collider, &anchor->lastSet);
}

static void ReanchorSlotFocus(AnchorSlot* slot) {
    Actor* owner = slot->owner;

    if (slot->hasFocusLast && (owner->focus.pos.x != slot->focusLastSet.x || owner->focus.pos.y != slot->focusLastSet.y ||
                               owner->focus.pos.z != slot->focusLastSet.z)) {
        slot->focusOffset.x = owner->focus.pos.x - owner->world.pos.x;
        slot->focusOffset.y = owner->focus.pos.y - owner->world.pos.y;
        slot->focusOffset.z = owner->focus.pos.z - owner->world.pos.z;
        slot->focusLastSet = owner->focus.pos;
        return;
    }
    owner->focus.pos.x = owner->world.pos.x + slot->focusOffset.x;
    owner->focus.pos.y = owner->world.pos.y + slot->focusOffset.y;
    owner->focus.pos.z = owner->world.pos.z + slot->focusOffset.z;
    slot->focusLastSet = owner->focus.pos;
    slot->hasFocusLast = true;
}

// An actor does not stay where it is put: its own update pushes it out of walls and drops it to the
// floor, so the hitboxes are repositioned absolutely, every frame, until both sides settle.
static void ReanchorColliders(PlayState* play) {
    for (u8 index = 0; index < SWITCH_HOOK_ANCHOR_SLOTS; index++) {
        AnchorSlot* slot = &sAnchorSlots[index];

        if (!slot->isActive) {
            continue;
        }
        if (!IsActorAlive(play, slot->owner)) {
            slot->isActive = false;
            slot->owner = NULL;
            slot->count = 0;
            continue;
        }
        for (u8 i = 0; i < slot->count; i++) {
            ReanchorOne(slot, &slot->anchors[i]);
        }
        ReanchorSlotFocus(slot);
    }
}

static void ClearColliderAnchors(void) {
    for (u8 index = 0; index < SWITCH_HOOK_ANCHOR_SLOTS; index++) {
        sAnchorSlots[index].isActive = false;
        sAnchorSlots[index].owner = NULL;
        sAnchorSlots[index].count = 0;
    }
}

static void PlantActorAt(Actor* actor, Vec3f* pos) {
    actor->world.pos = *pos;
    actor->prevPos = *pos;
    actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;
    actor->speedXZ = 0.0f;
    // With the ground flag still set, the scene collision drags the actor back to its old floor height
    // and a downward swap would move only one of the two.
    actor->bgCheckFlags = 0;
}

static void ShotAction(Player* player, PlayState* play);

static void ReturnHookToHand(Player* player, PlayState* play) {
    sPhase = SWITCH_HOOK_IN_HAND;
    sSwapTarget = NULL;
    Audio_StopSfxById(NA_SE_IT_HOOKSHOT_CHAIN - SFX_FLAG);
    if (player->actionFunc == ShotAction) {
        func_80839FFC(player, play);
    }
}

static void StartSwap(Player* player, PlayState* play, Actor* target) {
    Vec3f linkDestination = target->world.pos;
    Vec3f targetDestination = player->actor.world.pos;

    CaptureColliders(play, 0, &player->actor);
    CaptureColliders(play, 1, target);
    PlantActorAt(&player->actor, &linkDestination);
    PlantActorAt(target, &targetDestination);
    target->home.pos = targetDestination;
    player->linearVelocity = 0.0f;
    player->invincibilityTimer = SWITCH_HOOK_INVINCIBLE_FRAMES;
    Actor_SetColorFilter(target, 0, 255, 0, SWITCH_HOOK_TINT_FRAMES);
    // The chain sound is a looping one, and it has to be stopped where it was raised.
    player->actor.sfx = 0;
    Audio_StopSfxByPos(&player->actor.projectedPos);
    if (target->sfx != 0) {
        target->sfx = 0;
        Audio_StopSfxByPos(&target->projectedPos);
    }
    // Without dropping the continuous-sound bit this one-shot would stay playing forever.
    PlaySfxAt(NA_SE_IT_HOOKSHOT_STICK_OBJ - SFX_FLAG, &player->actor.world.pos);
    sSwapTarget = target;
    sHookPos = targetDestination;
    sHoldTimer = SWITCH_HOOK_HOLD_FRAMES;
    sSettleTimer = SWITCH_HOOK_SETTLE_FRAMES;
    sPhase = SWITCH_HOOK_SWAPPING;
}

static void HoldSwap(Player* player, PlayState* play) {
    if (sSwapTarget == NULL || sSwapTarget->update == NULL) {
        ReturnHookToHand(player, play);
        return;
    }
    Vec3f linkPos = player->actor.world.pos;
    Vec3f targetPos = sSwapTarget->world.pos;

    PlantActorAt(&player->actor, &linkPos);
    PlantActorAt(sSwapTarget, &targetPos);
    player->linearVelocity = 0.0f;
    player->invincibilityTimer = SWITCH_HOOK_INVINCIBLE_FRAMES / 2;
    if (--sHoldTimer > 0) {
        return;
    }
    PlaySfxAt(NA_SE_EV_ROLL_STAND - SFX_FLAG, &player->actor.world.pos);
    ReturnHookToHand(player, play);
}

static bool HasHitWall(PlayState* play, Vec3f* from, Vec3f* to) {
    CollisionPoly* poly = NULL;
    s32 bgId;
    Vec3f hit;

    if (!BgCheck_EntityLineTest1(&play->colCtx, from, to, &hit, &poly, true, true, true, true, &bgId)) {
        return false;
    }
    *to = hit;
    return true;
}

static void FlyHook(Player* player, PlayState* play) {
    Vec3f previous = sHookPos;
    f32 reach = Math_CosS(sShotPitch) * SWITCH_HOOK_SPEED;

    sHookPos.x += Math_SinS(sShotYaw) * reach;
    sHookPos.y -= Math_SinS(sShotPitch) * SWITCH_HOOK_SPEED;
    sHookPos.z += Math_CosS(sShotYaw) * reach;
    func_8002F974(&player->actor, NA_SE_IT_HOOKSHOT_CHAIN - SFX_FLAG);

    bool isArmed = Math_Vec3f_DistXYZ(&sHookPos, &player->actor.world.pos) > SWITCH_HOOK_ARMED_DISTANCE;
    Actor* target = NULL;

    if (isArmed && sSelection != NULL && sSelection->update != NULL &&
        Math_Vec3f_DistXYZ(&sHookPos, &sSelection->world.pos) < SWITCH_HOOK_GRAB_DISTANCE) {
        target = sSelection;
    } else if (isArmed && sSelection == NULL) {
        target = FindSwappableNear(play, &sHookPos, SWITCH_HOOK_BLIND_GRAB_DISTANCE);
    }
    if (target != NULL) {
        StartSwap(player, play, target);
        return;
    }
    if (HasHitWall(play, &previous, &sHookPos) || --sFlightTimer <= 0) {
        sPhase = SWITCH_HOOK_RETURNING;
    }
}

static void ReelHookIn(Player* player, PlayState* play) {
    Vec3f hand = { player->actor.world.pos.x, player->actor.world.pos.y + SWITCH_HOOK_HAND_HEIGHT,
                   player->actor.world.pos.z };
    f32 distance = Math_Vec3f_DistXYZ(&sHookPos, &hand);

    if (distance < SWITCH_HOOK_ARRIVE_DISTANCE) {
        PlaySfxAt(NA_SE_IT_HOOKSHOT_RECEIVE, &player->actor.world.pos);
        ReturnHookToHand(player, play);
        return;
    }
    f32 step = SWITCH_HOOK_RETURN_SPEED / distance;

    sHookPos.x += (hand.x - sHookPos.x) * step;
    sHookPos.y += (hand.y - sHookPos.y) * step;
    sHookPos.z += (hand.z - sHookPos.z) * step;
    func_8002F974(&player->actor, NA_SE_IT_HOOKSHOT_CHAIN - SFX_FLAG);
}

static void ShotAction(Player* player, PlayState* play) {
    Player_ZeroSpeedXZ(player);
    player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = sShotYaw;
    Player_UpdateUpperBody(player, play);
    switch (sPhase) {
        case SWITCH_HOOK_FLYING:
            FlyHook(player, play);
            break;
        case SWITCH_HOOK_SWAPPING:
            HoldSwap(player, play);
            break;
        case SWITCH_HOOK_RETURNING:
            ReelHookIn(player, play);
            break;
        default:
            ReturnHookToHand(player, play);
            break;
    }
}

static void FireHook(Player* player, PlayState* play, s16 yaw, s16 pitch) {
    if (!SpendCharge()) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return;
    }
    sShotYaw = yaw;
    sShotPitch = pitch;
    sHookPos.x = player->actor.world.pos.x;
    sHookPos.y = player->actor.world.pos.y + SWITCH_HOOK_HAND_HEIGHT;
    sHookPos.z = player->actor.world.pos.z;
    sFlightTimer = SWITCH_HOOK_FLIGHT_FRAMES;
    sPhase = SWITCH_HOOK_FLYING;
    Player_SetupAction(play, player, ShotAction, 1);
    Player_ZeroSpeedXZ(player);
}

static void StopAiming(Player* player, PlayState* play) {
    Z64Aiming_Release(player, play);
    if (sPhase == SWITCH_HOOK_AIMING) {
        sPhase = SWITCH_HOOK_IN_HAND;
    }
}

static void AimAction(Player* player, PlayState* play);

static void StartAiming(Player* player, PlayState* play) {
    if (!Z64Aiming_Request(player, play)) {
        return;
    }
    sPhase = SWITCH_HOOK_AIMING;
    sSelection = NULL;
    Player_SetupAction(play, player, AimAction, 1);
    Player_ZeroSpeedXZ(player);
    LinkAnimation_PlayLoop(play, &player->skelAnime, (LinkAnimationHeader*)sAimAnim);
}

static bool ShouldLeaveAim(Player* player, PlayState* play) {
    return (play->state.input[0].press.button & (BTN_CUP | BTN_A | BTN_B)) || player->invincibilityTimer < 0 ||
           (player->stateFlags1 & PLAYER_STATE1_IN_WATER);
}

static void AimAction(Player* player, PlayState* play) {
    if (ShouldLeaveAim(player, play)) {
        StopAiming(player, play);
        func_80839FFC(player, play);
        return;
    }
    Z64Aiming_Update(player, play);
    Player_ZeroSpeedXZ(player);
    LinkAnimation_Update(play, &player->skelAnime);
    Player_UpdateUpperBody(player, play);
}

// Aiming by hand trades the auto-aim away: there is no selection, so the hook takes whatever it passes.
static void AimOrFire(Player* player, PlayState* play) {
    if (sPhase == SWITCH_HOOK_AIMING) {
        s16 yaw;
        s16 pitch;

        Z64Aiming_GetDirection(player, &yaw, &pitch);
        StopAiming(player, play);
        FireHook(player, play, yaw, pitch);
        func_80839FFC(player, play);
        return;
    }
    if (sPhase != SWITCH_HOOK_IN_HAND) {
        return;
    }
    if (sSelection != NULL && sSelection->update != NULL) {
        f32 dx = sSelection->focus.pos.x - player->actor.world.pos.x;
        f32 dy = sSelection->focus.pos.y - (player->actor.world.pos.y + SWITCH_HOOK_HAND_HEIGHT);
        f32 dz = sSelection->focus.pos.z - player->actor.world.pos.z;

        FireHook(player, play, Math_Atan2S(dz, dx), Math_Atan2S(sqrtf(SQ(dx) + SQ(dz)), -dy));
        return;
    }
    FireHook(player, play, player->actor.shape.rot.y, 0);
}

static bool CanUseSwitchHook(Player* player, PlayState* play) {
    return !(player->stateFlags1 & PLAYER_STATE1_IN_WATER);
}

static void TakeOutSwitchHook(PlayState* play, Player* player) {
    sPhase = SWITCH_HOOK_IN_HAND;
    sIsAimPending = false;
    sSelection = NULL;
}

static void StowSwitchHook(Player* player, PlayState* play) {
    if (sPhase == SWITCH_HOOK_AIMING) {
        StopAiming(player, play);
    }
    if (player->actionFunc == ShotAction || player->actionFunc == AimAction) {
        func_80839FFC(player, play);
    }
    Audio_StopSfxById(NA_SE_IT_HOOKSHOT_CHAIN - SFX_FLAG);
    sPhase = SWITCH_HOOK_STOWED;
    sSelection = NULL;
    sSwapTarget = NULL;
}

// The hook keeps Link still while it flies, so the upper body is only busy then.
static int32_t HoldSwitchHook(Player* player, PlayState* play) {
    if (sPhase == SWITCH_HOOK_IN_HAND) {
        sHookPos = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    }
    return 0;
}

static bool IsChangingHeldItem(Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_START_CHANGING_HELD_ITEM) ||
           player->upperActionFunc == Player_UpperAction_ChangeHeldItem;
}

// The live selection is tinted blue so the player can see what the shot will trade places with.
static void MarkSelection(Player* player, PlayState* play) {
    if (sPhase != SWITCH_HOOK_IN_HAND) {
        return;
    }
    sSelection = ScanForSelection(player, play);
    if (sSelection != NULL) {
        Actor_SetColorFilter(sSelection, 0, 255, 0, 4);
    }
}

static void UpdateSwitchHook(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    TickCharges();
    if (play == NULL || player == NULL) {
        return;
    }
    if (sIsAimPending && !IsChangingHeldItem(player)) {
        sIsAimPending = false;
    }
    if (sPhase == SWITCH_HOOK_AIMING && player->actionFunc != AimAction) {
        StopAiming(player, play);
    }
    if (sPhase >= SWITCH_HOOK_FLYING && player->actionFunc != ShotAction) {
        ReturnHookToHand(player, play);
    }
    MarkSelection(player, play);
    if (sSettleTimer > 0) {
        ReanchorColliders(play);
        if (--sSettleTimer == 0) {
            ClearColliderAnchors();
        }
    }
}

static void ForgetScene(int16_t sceneNum) {
    ClearColliderAnchors();
    sSettleTimer = 0;
    sSelection = NULL;
    sSwapTarget = NULL;
    sPhase = SWITCH_HOOK_STOWED;
}

static void DrawSegment(PlayState* play, Vec3f* pos, f32 yaw, f32 pitch, f32 scale, Gfx* displayList) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_RotateY(yaw, MTXMODE_APPLY);
    Matrix_RotateX(pitch, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, displayList);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawChain(PlayState* play, Vec3f* from, Vec3f* to) {
    f32 dx = to->x - from->x;
    f32 dy = to->y - from->y;
    f32 dz = to->z - from->z;
    f32 yaw = Math_FAtan2F(dx, dz);
    f32 pitch = Math_FAtan2F(-dy, sqrtf(SQ(dx) + SQ(dz)));
    s32 links = CLAMP((s32)(sqrtf(SQ(dx) + SQ(dy) + SQ(dz)) / SWITCH_HOOK_CHAIN_LINK_SPACING), 1,
                      SWITCH_HOOK_CHAIN_LINK_MAX);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
    for (s32 i = 0; i <= links; i++) {
        f32 t = (f32)i / links;
        Vec3f link = { from->x + dx * t, from->y + dy * t, from->z + dz * t };

        DrawSegment(play, &link, yaw, pitch, SWITCH_HOOK_CHAIN_SCALE, (Gfx*)sChainLinkDL);
    }
    DrawSegment(play, to, yaw, pitch, SWITCH_HOOK_TIP_SCALE, (Gfx*)sTipDL);
}

static void DrawSwitchHook(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];

    if (sPhase == SWITCH_HOOK_STOWED || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    DrawSegment(play, &hand, BINANG_TO_RAD(player->actor.shape.rot.y), 0.0f, SWITCH_HOOK_HELD_SCALE, (Gfx*)sGiveDL);
    if (sPhase == SWITCH_HOOK_FLYING || sPhase == SWITCH_HOOK_RETURNING) {
        DrawChain(play, &hand, &sHookPos);
    }
    if (sPhase == SWITCH_HOOK_AIMING) {
        Z64Aiming_DrawReticle(play, player, SWITCH_HOOK_SPEED * SWITCH_HOOK_FLIGHT_FRAMES);
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(SWITCH_HOOK_GIVE_SCALE, SWITCH_HOOK_GIVE_SCALE, SWITCH_HOOK_GIVE_SCALE, MTXMODE_APPLY);
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

    SOHCustomItemDefinition hook = Z64Items_Define(SWITCH_HOOK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&hook, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&hook, 0, 14, 0);
    Z64Items_SetTextbox(&hook, "You got the %rSwitch Hook%w!&A hook that trades places with whatever&it catches."
                               "^Face a %yfoe%w, a %ypot%w or a %ychest%w and it&glows blue: press %y\xA1%w and you "
                               "stand&where it stood.^Press %y\xA5%w first to aim it by hand.^It holds %g5 shots%w, "
                               "and they come back&on their own.");
    Z64Items_SetPauseText(&hook, "%rSwitch Hook&%wPress %y\xA1%w to trade places with the&blue target. %y\xA5%w aims "
                                 "it by hand.");
    Z64Items_SetCanUse(&hook, CanUseSwitchHook);
    Z64Items_SetAction(&hook, TakeOutSwitchHook, HoldSwitchHook);
    Z64Items_SetHeldCallbacks(&hook, AimOrFire, StowSwitchHook, NULL);
    hook.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    hook.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &hook)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateSwitchHook);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawSwitchHook);
}
