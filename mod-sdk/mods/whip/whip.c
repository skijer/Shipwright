#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define WHIP_KEY "nei.whip"
#define WHIP_BUTTON_COUNT 8
#define WHIP_DAMAGE 2
#define WHIP_COLLIDER_RADIUS 12
#define WHIP_COLLIDER_HEIGHT 8
#define WHIP_EXTEND_SPEED 20.0f
#define WHIP_RETRACT_SPEED 25.0f
#define WHIP_LASH_FRAMES 26
#define WHIP_ARRIVE_DISTANCE 30.0f
#define WHIP_TORCH_REACH 40.0f
#define WHIP_TORCH_GRIP_HEIGHT 30.0f
#define WHIP_ENEMY_REACH 25.0f
#define WHIP_STUN_FRAMES 60
#define WHIP_PULL_SPEED 15.0f
#define WHIP_PULL_ARRIVE_DISTANCE 50.0f
#define WHIP_PULL_HEIGHT 30.0f
#define WHIP_RAGE_FRAMES 90
#define WHIP_RAGE_EXTRA_STRIDE 0.5f
// Two adult Links tall: the grab pulls him onto that radius instead of hanging at the distance he happened
// to lash from, which is what makes every swing arc the same. A lower anchor gets the rope that fits under
// it rather than planting him in the floor.
#define WHIP_ROPE_LENGTH 136.0f
#define WHIP_MIN_ROPE_LENGTH 40.0f
#define WHIP_HANG_CLEARANCE 20.0f
#define WHIP_SWING_GRAVITY 0.018f
#define WHIP_SWING_DAMPING 0.998f
#define WHIP_PUMP_FORCE 0.006f
#define WHIP_MAX_SWING_ANGLE 1.2f
#define WHIP_SWING_BOUNCE 0.3f
#define WHIP_STEER_RATE 1536
#define WHIP_RELEASE_BOOST 2.0f
#define WHIP_MAX_RELEASE_SPEED 25.0f
#define WHIP_MIN_LAUNCH_LIFT 1.0f
#define WHIP_FORWARD_RELEASE_LIFT 0.25f
#define WHIP_FLOOR_TOLERANCE 5.0f
// Frame 13 of the swing clip is the vertical pose, the point the swing turns from back to front.
#define WHIP_SWING_MIDDLE_FRAME 13.0f
#define WHIP_CAMERA_DISTANCE 200.0f
#define WHIP_CAMERA_HEIGHT 60.0f
#define WHIP_CAMERA_AT_HEIGHT 20.0f
#define WHIP_CAMERA_FOLLOW_STEP 0x400
#define WHIP_CAMERA_FOLLOW_SCALE 8
#define WHIP_BEAM_MIN_LENGTH 30.0f
#define WHIP_BEAM_MAX_CROSS 200.0f
#define WHIP_BEAM_MIN_THICKNESS 2.0f
#define WHIP_BEAM_MAX_CROSS_SUM 350.0f
#define WHIP_BEAM_ASPECT 1.5f
#define WHIP_BEAM_NEIGHBOR_DISTANCE 50.0f
#define WHIP_BEAM_NORMAL_SIMILARITY 0.85f
// The lash display list is one snake-body piece of this length along +Z, repeated along the cord.
#define WHIP_LASH_SEGMENT_LENGTH 8.0f
#define WHIP_LASH_MAX_SEGMENTS 72
#define WHIP_TIP_SCALE 0.8f
// Pose off the right hand BONE, so the whip turns with the wrist. The angles are the ones NEI dialled for
// its Rod of Seasons, a staff modelled along +Y like this grip; the offsets slide it out of the palm and up
// to the middle of the grip, which is what a fist closes on.
#define WHIP_HAND_ROT_X 109.655f
#define WHIP_HAND_ROT_Y 72.414f
#define WHIP_HAND_OFFSET_Y 5.0f
#define WHIP_HAND_OFFSET_Z -3.0f
// The whip model spans about 120 units, so this puts a third of Link's height of coiled whip in his hand.
#define WHIP_HELD_SCALE 0.2f
#define WHIP_SWING_ROPE_SAG 15.0f
#define WHIP_GIVE_SCALE 0.5f

typedef enum {
    WHIP_IDLE,
    WHIP_COILED,
    WHIP_AIMING,
    WHIP_LASHING,
    WHIP_PULLING,
    WHIP_RETRACTING,
    WHIP_SWINGING,
} WhipPhase;

typedef enum {
    WHIP_RELEASE_PLAIN,
    WHIP_RELEASE_FORWARD,
    WHIP_RELEASE_JUMPSLASH,
} WhipRelease;

typedef struct {
    Vec3f origin;
    Vec3f target;
    f32 scale;
} WhipPart;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconWhipTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gWhipNameTex";
static const ALIGN_ASSET(2) char sItemDL[] = "__OTR__objects/object_nei_whip/whip_item_opaque_dl";
static const ALIGN_ASSET(2) char sGripDL[] = "__OTR__objects/object_nei_whip/whip_grip_opaque_dl";
static const ALIGN_ASSET(2) char sLashDL[] = "__OTR__objects/object_nei_whip/whip_lash_opaque_dl";
static const ALIGN_ASSET(2) char sTipDL[] = "__OTR__objects/object_nei_whip/whip_tip_opaque_dl";
static const ALIGN_ASSET(2) char sWaitAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_boom_throw_waitR";
static const ALIGN_ASSET(2) char sThrowAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_boom_throwR";
static const ALIGN_ASSET(2) char sJumpAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_jump";
static const ALIGN_ASSET(2) char sAimAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_hook_shot_ready";
// Retargeted from Mixamo's rope swing, frames 20-60: the whole clip is one back-to-front swing.
static const ALIGN_ASSET(2) char sSwingAnim[] = "__OTR__objects/object_nei_whip/gPlayerAnim_nei_whip_swing";

static const u16 sItemButtons[WHIP_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                     BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static ColliderCylinderInit sLashColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_BOOMERANG, 0x00, WHIP_DAMAGE }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { WHIP_COLLIDER_RADIUS, WHIP_COLLIDER_HEIGHT, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = { "OnPlayerUpdate",      "OnActorDrawEnd",
                                              "OnSceneInit",         "OnPlayerFilterInput",
                                              "OnPlayerPostLimbDraw", "OnPlayerResolveItemActionInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static ColliderCylinder sLashCollider;
static bool sIsColliderReady;
static bool sIsLashPending;
static u8 sPhase;
static u8 sAnimatedPhase;
static s16 sLashTimer;
static s16 sLashYaw;
static s16 sLashPitch;
static Vec3f sTipPos;
static Vec3f sAnchorPos;
static f32 sSwingAngle;
static f32 sSwingVelocity;
static f32 sRopeLength;
static s16 sSwingYaw;
static s16 sCameraId = SUBCAM_FREE;
static s16 sCameraYaw;
static Actor* sPulledEnemy;
static Actor* sEnragedEnemy;
static s16 sRageTimer;
static MtxF sHandMatrix;
static bool sHasHandMatrix;

s32 Player_UpdateUpperBody(Player* player, PlayState* play);
s32 Player_IsZTargeting(Player* player);
void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_ZeroSpeedXZ(Player* player);
void func_80839FFC(Player* player, PlayState* play);
s32 Player_UpperAction_ChangeHeldItem(Player* player, PlayState* play);
void func_80838940(Player* player, LinkAnimationHeader* anim, f32 lift, PlayState* play, u16 voiceSfx);
void func_8083BA90(PlayState* play, Player* player, s32 meleeWeaponAnim, f32 xzVelocity, f32 yVelocity);
s8 Player_ItemToItemAction(s32 item);
void Player_InitItemActionWithAnim(PlayState* play, Player* player, s8 itemAction);
void Player_AnimPlayLoop(PlayState* play, Player* player, LinkAnimationHeader* anim);
LinkAnimationHeader* Player_GetIdleAnim(Player* player);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static u16 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < WHIP_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, WHIP_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static void ExpandBounds(Vec3f* min, Vec3f* max, Vec3i* vertex) {
    min->x = MIN(min->x, vertex->x);
    min->y = MIN(min->y, vertex->y);
    min->z = MIN(min->z, vertex->z);
    max->x = MAX(max->x, vertex->x);
    max->y = MAX(max->y, vertex->y);
    max->z = MAX(max->z, vertex->z);
}

static void GetPolyCenter(Vec3i* vertices, CollisionPoly* poly, Vec3f* center) {
    Vec3i* a = &vertices[COLPOLY_VTX_INDEX(poly->flags_vIA)];
    Vec3i* b = &vertices[COLPOLY_VTX_INDEX(poly->flags_vIB)];
    Vec3i* c = &vertices[COLPOLY_VTX_INDEX(poly->vIC)];

    center->x = (a->x + b->x + c->x) / 3.0f;
    center->y = (a->y + b->y + c->y) / 3.0f;
    center->z = (a->z + b->z + c->z) / 3.0f;
}

static bool ArePolysFacingAlike(CollisionPoly* poly, CollisionPoly* other) {
    f32 dot = COLPOLY_GET_NORMAL(poly->normal.x) * COLPOLY_GET_NORMAL(other->normal.x) +
              COLPOLY_GET_NORMAL(poly->normal.y) * COLPOLY_GET_NORMAL(other->normal.y) +
              COLPOLY_GET_NORMAL(poly->normal.z) * COLPOLY_GET_NORMAL(other->normal.z);

    return dot > WHIP_BEAM_NORMAL_SIMILARITY;
}

static bool SharesVertex(CollisionPoly* poly, CollisionPoly* other) {
    u32 polyVertices[3] = { COLPOLY_VTX_INDEX(poly->flags_vIA), COLPOLY_VTX_INDEX(poly->flags_vIB), poly->vIC };
    u32 otherVertices[3] = { COLPOLY_VTX_INDEX(other->flags_vIA), COLPOLY_VTX_INDEX(other->flags_vIB), other->vIC };

    for (u32 i = 0; i < 3; i++) {
        for (u32 j = 0; j < 3; j++) {
            if (polyVertices[i] == otherVertices[j]) {
                return true;
            }
        }
    }
    return false;
}

static void SortAscending(f32* dims) {
    for (u32 i = 0; i < 2; i++) {
        for (u32 j = 0; j < 2 - i; j++) {
            if (dims[j] > dims[j + 1]) {
                f32 swap = dims[j];
                dims[j] = dims[j + 1];
                dims[j + 1] = swap;
            }
        }
    }
}

// Scene geometry carries no "beam" flag, so the shape of the same-facing patch around the hit decides it.
// Dyna polys index a shared, relocated vertex list, so their shape can't be read back; only their flag counts.
static bool IsGraspable(PlayState* play, CollisionPoly* poly, s32 bgId) {
    bool isHookshotSurface = SurfaceType_IsHookshotSurface(&play->colCtx, poly, bgId);
    CollisionHeader* header = BgCheck_GetCollisionHeader(&play->colCtx, bgId);

    if (bgId != BGCHECK_SCENE || header == NULL || header->vtxList == NULL) {
        return isHookshotSurface;
    }
    bool isCeiling = COLPOLY_GET_NORMAL(poly->normal.y) < -0.5f;
    f32 neighborDistance = isCeiling ? WHIP_BEAM_NEIGHBOR_DISTANCE * 2.0f : WHIP_BEAM_NEIGHBOR_DISTANCE;
    Vec3f min = { 99999.0f, 99999.0f, 99999.0f };
    Vec3f max = { -99999.0f, -99999.0f, -99999.0f };
    Vec3f hitCenter;

    GetPolyCenter(header->vtxList, poly, &hitCenter);
    for (u32 i = 0; i < header->numPolygons; i++) {
        CollisionPoly* other = &header->polyList[i];
        Vec3f otherCenter;

        if (other != poly && !ArePolysFacingAlike(poly, other)) {
            continue;
        }
        GetPolyCenter(header->vtxList, other, &otherCenter);
        if (other != poly && !SharesVertex(poly, other) &&
            Math_Vec3f_DistXYZ(&hitCenter, &otherCenter) > neighborDistance) {
            continue;
        }
        ExpandBounds(&min, &max, &header->vtxList[COLPOLY_VTX_INDEX(other->flags_vIA)]);
        ExpandBounds(&min, &max, &header->vtxList[COLPOLY_VTX_INDEX(other->flags_vIB)]);
        ExpandBounds(&min, &max, &header->vtxList[COLPOLY_VTX_INDEX(other->vIC)]);
    }
    f32 dims[3] = { max.x - min.x, max.y - min.y, max.z - min.z };

    SortAscending(dims);
    f32 aspect = dims[1] > 0.1f ? dims[2] / dims[1] : 10.0f;

    if (isCeiling) {
        return isHookshotSurface ||
               (dims[2] >= WHIP_BEAM_MIN_LENGTH * 0.5f &&
                (dims[0] <= WHIP_BEAM_MAX_CROSS * 1.5f || aspect >= WHIP_BEAM_ASPECT * 0.75f));
    }
    bool isBar = dims[0] >= WHIP_BEAM_MIN_THICKNESS && dims[0] <= WHIP_BEAM_MAX_CROSS &&
                 dims[1] <= WHIP_BEAM_MAX_CROSS && dims[0] + dims[1] <= WHIP_BEAM_MAX_CROSS_SUM;
    bool isElongated = aspect >= WHIP_BEAM_ASPECT && dims[0] <= WHIP_BEAM_MAX_CROSS;

    return dims[2] >= WHIP_BEAM_MIN_LENGTH && (isBar || isElongated);
}

static void DestroySwingCamera(PlayState* play) {
    if (sCameraId == SUBCAM_FREE) {
        return;
    }
    Camera_ChangeMode(Play_GetCamera(play, MAIN_CAM), CAM_MODE_NORMAL);
    Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_ACTIVE);
    Play_ClearCamera(play, sCameraId);
    sCameraId = SUBCAM_FREE;
}

static void CreateSwingCamera(PlayState* play) {
    if (sCameraId != SUBCAM_FREE) {
        return;
    }
    sCameraId = Play_CreateSubCamera(play);
    Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_WAIT);
    Play_ChangeCameraStatus(play, sCameraId, CAM_STAT_ACTIVE);
    sCameraYaw = sSwingYaw;
}

// The swing yaw points from the anchor to Link, so an eye further along it frames Link with the anchor beyond.
static void FollowSwingWithCamera(Player* player, PlayState* play) {
    Vec3f at = { player->actor.world.pos.x, player->actor.world.pos.y + WHIP_CAMERA_AT_HEIGHT,
                 player->actor.world.pos.z };
    Vec3f eye;

    Math_SmoothStepToS(&sCameraYaw, sSwingYaw, WHIP_CAMERA_FOLLOW_SCALE, WHIP_CAMERA_FOLLOW_STEP, 0x10);
    eye.x = player->actor.world.pos.x + Math_SinS(sCameraYaw) * WHIP_CAMERA_DISTANCE;
    eye.y = player->actor.world.pos.y + WHIP_CAMERA_HEIGHT;
    eye.z = player->actor.world.pos.z + Math_CosS(sCameraYaw) * WHIP_CAMERA_DISTANCE;
    Play_CameraSetAtEye(play, sCameraId, &at, &eye);
}

static void FaceYaw(Player* player, s16 yaw) {
    player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = yaw;
}

static void HoldStill(Player* player) {
    player->linearVelocity = 0.0f;
    player->actor.speedXZ = 0.0f;
}

static void StartLash(Player* player, PlayState* play, s16 yaw, s16 pitch) {
    sLashYaw = yaw;
    sLashPitch = pitch;
    FaceYaw(player, sLashYaw);
    sTipPos = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    sLashTimer = WHIP_LASH_FRAMES;
    sPhase = WHIP_LASHING;
    // Forget whatever the arm was animating, so the throw plays again on every lash.
    sAnimatedPhase = WHIP_IDLE;
    PlaySfxAt(NA_SE_IT_SWORD_SWING, &player->actor.world.pos);
}

static void AdvanceTip(Player* player) {
    Actor* target = player->focusActor;

    if (Player_IsZTargeting(player) && target != NULL && target->update != NULL) {
        f32 distance = Math_Vec3f_DistXYZ(&sTipPos, &target->focus.pos);

        if (distance > 1.0f) {
            f32 step = WHIP_EXTEND_SPEED / distance;
            sTipPos.x += (target->focus.pos.x - sTipPos.x) * step;
            sTipPos.y += (target->focus.pos.y - sTipPos.y) * step;
            sTipPos.z += (target->focus.pos.z - sTipPos.z) * step;
        }
        sLashYaw = Math_Vec3f_Yaw(&player->actor.world.pos, &target->world.pos);
        return;
    }
    sTipPos.x += Math_SinS(sLashYaw) * Math_CosS(sLashPitch) * WHIP_EXTEND_SPEED;
    sTipPos.y -= Math_SinS(sLashPitch) * WHIP_EXTEND_SPEED;
    sTipPos.z += Math_CosS(sLashYaw) * Math_CosS(sLashPitch) * WHIP_EXTEND_SPEED;
}

static void AimAction(Player* player, PlayState* play);

// The body keeps the idle loop and the aim pose rides the upper body, as vanilla aims the bow: the upper
// animation then takes the whole skeleton over while he stands still (Player_UpdateUpperBody).
static void StartAiming(Player* player, PlayState* play) {
    if (!Z64Aiming_Request(player, play)) {
        return;
    }
    sPhase = WHIP_AIMING;
    Player_SetupAction(play, player, AimAction, 1);
    Player_ZeroSpeedXZ(player);
    Player_AnimPlayLoop(play, player, Player_GetIdleAnim(player));
}

static void StopAiming(Player* player, PlayState* play) {
    Z64Aiming_Release(player, play);
    if (sPhase == WHIP_AIMING) {
        sPhase = WHIP_COILED;
    }
}

static bool ShouldLeaveAim(Player* player, PlayState* play) {
    return (play->state.input[0].press.button & (BTN_CUP | BTN_B | BTN_A)) || player->invincibilityTimer < 0 ||
           Player_IsZTargeting(player) || (player->stateFlags1 & PLAYER_STATE1_IN_WATER);
}

// The exit is read before the upper body runs, or vanilla would take the same A or B as "roll" or "draw the
// sword" and drop the whip instead of just lowering it.
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

static void HoldSwingAnimation(Player* player, PlayState* play);

// The angle Link hangs at comes from where he lashed from; the length of the rope does not, so the grab
// swings him onto that radius. Only an anchor with no room under it at all refuses the swing.
static bool StartSwing(Player* player, PlayState* play, Vec3f* anchor) {
    f32 dx = player->actor.world.pos.x - anchor->x;
    f32 dz = player->actor.world.pos.z - anchor->z;
    f32 drop = anchor->y - player->actor.world.pos.y;
    f32 reach = sqrtf(SQ(dx) + SQ(dz));
    f32 room = anchor->y - player->actor.floorHeight - WHIP_HANG_CLEARANCE;

    sRopeLength = MIN(WHIP_ROPE_LENGTH, room);
    if (sRopeLength < WHIP_MIN_ROPE_LENGTH) {
        return false;
    }
    sSwingAngle = drop > 0.1f ? Math_FAtan2F(reach, drop) : WHIP_MAX_SWING_ANGLE * 0.5f;
    sAnchorPos = *anchor;
    sSwingYaw = Math_Atan2S(dx, dz);
    sSwingVelocity = 0.0f;
    sPhase = WHIP_SWINGING;
    HoldSwingAnimation(player, play);
    PlaySfxAt(NA_SE_IT_HOOKSHOT_STICK_OBJ, anchor);
    CreateSwingCamera(play);
    return true;
}

static Actor* FindTorchStandAt(PlayState* play, Vec3f* pos) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_PROP].head; actor != NULL; actor = actor->next) {
        if (actor->update != NULL && actor->id == ACTOR_OBJ_SYOKUDAI &&
            Math_Vec3f_DistXYZ(pos, &actor->world.pos) < WHIP_TORCH_REACH) {
            return actor;
        }
    }
    return NULL;
}

static Actor* FindEnemyAt(PlayState* play, Vec3f* pos) {
    static const u8 sHostileCategories[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };

    for (u32 i = 0; i < ARRAY_COUNT(sHostileCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[sHostileCategories[i]].head; actor != NULL; actor = actor->next) {
            if (actor->update != NULL && Math_Vec3f_DistXYZ(pos, &actor->world.pos) < WHIP_ENEMY_REACH) {
                return actor;
            }
        }
    }
    return NULL;
}

static bool IsLightEnoughToReelIn(Actor* enemy) {
    return enemy->id == ACTOR_EN_FIREFLY || enemy->id == ACTOR_EN_BB;
}

static bool IsArmored(Actor* enemy) {
    return enemy->id == ACTOR_EN_IK || enemy->id == ACTOR_EN_ZF;
}

static void StrikeEnemy(Actor* enemy) {
    PlaySfxAt(NA_SE_IT_SHIELD_BOUND, &enemy->world.pos);
    if (IsLightEnoughToReelIn(enemy)) {
        enemy->colorFilterTimer = WHIP_STUN_FRAMES;
        enemy->colorFilterParams = 0x0028;
        enemy->speedXZ = 0.0f;
        sPulledEnemy = enemy;
        sPhase = WHIP_PULLING;
        return;
    }
    sPhase = WHIP_RETRACTING;
    if (!IsArmored(enemy)) {
        sEnragedEnemy = enemy;
        sRageTimer = WHIP_RAGE_FRAMES + WHIP_STUN_FRAMES;
    }
}

static bool CatchOnGeometry(Player* player, PlayState* play, Vec3f* previousTip) {
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;
    Vec3f hitPos;

    if (!BgCheck_EntityLineTest1(&play->colCtx, previousTip, &sTipPos, &hitPos, &poly, true, true, true, true,
                                 &bgId)) {
        return false;
    }
    sTipPos = hitPos;
    if (!IsGraspable(play, poly, bgId)) {
        sPhase = WHIP_RETRACTING;
        return true;
    }
    if (!StartSwing(player, play, &hitPos)) {
        sPhase = WHIP_RETRACTING;
    }
    return true;
}

static bool CatchOnTorchStand(Player* player, PlayState* play) {
    Actor* torch = FindTorchStandAt(play, &sTipPos);

    if (torch == NULL) {
        return false;
    }
    Vec3f grip = { torch->world.pos.x, torch->world.pos.y + WHIP_TORCH_GRIP_HEIGHT, torch->world.pos.z };

    if (!StartSwing(player, play, &grip)) {
        sPhase = WHIP_RETRACTING;
    }
    return true;
}

static bool CatchEnemy(PlayState* play) {
    Actor* enemy = (sLashCollider.base.atFlags & AT_HIT) ? sLashCollider.base.at : NULL;

    sLashCollider.base.atFlags &= ~AT_HIT;
    if (enemy == NULL || enemy->update == NULL) {
        enemy = FindEnemyAt(play, &sTipPos);
    }
    if (enemy == NULL) {
        return false;
    }
    StrikeEnemy(enemy);
    return true;
}

static void UpdateLashing(Player* player, PlayState* play) {
    Vec3f previousTip = sTipPos;

    HoldStill(player);
    AdvanceTip(player);
    FaceYaw(player, sLashYaw);
    sLashCollider.dim.pos.x = (s16)sTipPos.x;
    sLashCollider.dim.pos.y = (s16)(sTipPos.y - WHIP_COLLIDER_HEIGHT / 2);
    sLashCollider.dim.pos.z = (s16)sTipPos.z;
    sLashCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    if (CatchOnGeometry(player, play, &previousTip) || CatchOnTorchStand(player, play) || CatchEnemy(play)) {
        return;
    }
    CollisionCheck_SetAT(play, &play->colChkCtx, &sLashCollider.base);
    if (--sLashTimer <= 0) {
        sPhase = WHIP_RETRACTING;
    }
}

static void UpdatePulling(Player* player) {
    HoldStill(player);
    if (sPulledEnemy == NULL || sPulledEnemy->update == NULL) {
        sPulledEnemy = NULL;
        sPhase = WHIP_RETRACTING;
        return;
    }
    Vec3f destination = { player->actor.world.pos.x, player->actor.world.pos.y + WHIP_PULL_HEIGHT,
                          player->actor.world.pos.z };
    f32 distance = Math_Vec3f_DistXYZ(&sPulledEnemy->world.pos, &destination);

    sTipPos = sPulledEnemy->world.pos;
    if (distance < WHIP_PULL_ARRIVE_DISTANCE) {
        sPulledEnemy = NULL;
        sPhase = WHIP_RETRACTING;
        return;
    }
    f32 step = WHIP_PULL_SPEED / distance;

    sPulledEnemy->world.pos.x += (destination.x - sPulledEnemy->world.pos.x) * step;
    sPulledEnemy->world.pos.y += (destination.y - sPulledEnemy->world.pos.y) * step;
    sPulledEnemy->world.pos.z += (destination.z - sPulledEnemy->world.pos.z) * step;
    sPulledEnemy->speedXZ = 0.0f;
    func_8002F974(&player->actor, NA_SE_IT_HOOKSHOT_CHAIN - SFX_FLAG);
}

static void UpdateRetracting(Player* player) {
    Vec3f* hand = &player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    f32 distance = Math_Vec3f_DistXYZ(&sTipPos, hand);

    if (distance < WHIP_ARRIVE_DISTANCE) {
        sTipPos = *hand;
        sPhase = WHIP_COILED;
        PlaySfxAt(NA_SE_PL_CATCH_BOOMERANG, &player->actor.world.pos);
        return;
    }
    f32 step = WHIP_RETRACT_SPEED / distance;

    sTipPos.x += (hand->x - sTipPos.x) * step;
    sTipPos.y += (hand->y - sTipPos.y) * step;
    sTipPos.z += (hand->z - sTipPos.z) * step;
    func_8002F974(&player->actor, NA_SE_IT_HOOKSHOT_CHAIN - SFX_FLAG);
}

static void AnimateArm(Player* player, PlayState* play) {
    if (sPhase == WHIP_SWINGING) {
        // The swing clip is already on both skeletons; an arm animation over it would let go of the rope.
        return;
    }
    if (sAnimatedPhase == sPhase) {
        LinkAnimation_Update(play, &player->upperSkelAnime);
        return;
    }
    sAnimatedPhase = sPhase;
    if (sPhase == WHIP_AIMING) {
        // Played once: its last frame is the held aim pose, the way vanilla holds the hookshot up.
        LinkAnimation_PlayOnce(play, &player->upperSkelAnime, (LinkAnimationHeader*)sAimAnim);
    } else if (sPhase == WHIP_LASHING) {
        LinkAnimation_PlayOnce(play, &player->upperSkelAnime, (LinkAnimationHeader*)sThrowAnim);
    } else {
        LinkAnimation_PlayLoop(play, &player->upperSkelAnime, (LinkAnimationHeader*)sWaitAnim);
    }
}

// The return value is Link's "upper body is busy" flag: it is only true while the aim, the lash or the swing
// owns his arms, or a whip merely coiled in hand would block his rolls and his other item buttons.
static int32_t UpdateWhipInHand(Player* player, PlayState* play) {
    if (player->invincibilityTimer < 0 && sPhase != WHIP_SWINGING) {
        sPhase = WHIP_COILED;
    }
    switch (sPhase) {
        case WHIP_COILED:
            sTipPos = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
            return 0;
        case WHIP_AIMING:
            sTipPos = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
            break;
        case WHIP_LASHING:
            UpdateLashing(player, play);
            break;
        case WHIP_PULLING:
            UpdatePulling(player);
            break;
        case WHIP_RETRACTING:
            UpdateRetracting(player);
            break;
        case WHIP_SWINGING:
            // The swing itself runs at the end of the player's update; claiming the upper body here is what
            // keeps vanilla's rolls, attacks and item buttons off it.
            return 1;
        default:
            sPhase = WHIP_COILED;
            return 0;
    }
    AnimateArm(player, play);
    return 1;
}

static bool HasSwordOnB(void) {
    s8 action = Player_ItemToItemAction(gSaveContext.equips.buttonItems[0]);

    return action >= PLAYER_IA_SWORD_MASTER && action <= PLAYER_IA_SWORD_BIGGORON;
}

// The jump slash needs a blade in hand, so the whip trades places with the B sword without the change animation.
static void ReleaseIntoJumpSlash(Player* player, PlayState* play, f32 forwardSpeed, f32 lift) {
    s8 swordAction = Player_ItemToItemAction(gSaveContext.equips.buttonItems[0]);

    player->heldItemId = gSaveContext.equips.buttonItems[0];
    player->nextModelGroup = Player_ActionToModelGroup(player, swordAction);
    Player_InitItemActionWithAnim(play, player, swordAction);
    func_8083BA90(play, player, PLAYER_MWA_JUMPSLASH_START, MIN(fabsf(forwardSpeed), WHIP_MAX_RELEASE_SPEED),
                  MAX(lift, WHIP_MIN_LAUNCH_LIFT));
}

static void ReleaseIntoJump(Player* player, PlayState* play, WhipRelease release, f32 forwardSpeed, f32 lift) {
    func_80838940(player, (LinkAnimationHeader*)sJumpAnim, 1.0f, play, NA_SE_VO_LI_SWORD_N);
    player->linearVelocity = MIN(fabsf(forwardSpeed), WHIP_MAX_RELEASE_SPEED);
    player->actor.speedXZ = player->linearVelocity;
    player->actor.velocity.y =
        release == WHIP_RELEASE_FORWARD ? lift * WHIP_FORWARD_RELEASE_LIFT : MAX(lift, WHIP_MIN_LAUNCH_LIFT);
}

// Tangential speed of the pendulum splits into the launch: its horizontal part carries forward, the rest
// lifts, and Link turns to face the way he is thrown.
static void ReleaseSwing(Player* player, PlayState* play, WhipRelease release, f32 angularVelocity) {
    f32 tangentialSpeed = angularVelocity * sRopeLength * WHIP_RELEASE_BOOST;
    f32 forwardSpeed = Math_CosF(sSwingAngle) * tangentialSpeed;
    f32 lift = Math_SinF(sSwingAngle) * tangentialSpeed;

    FaceYaw(player, forwardSpeed >= 0.0f ? sSwingYaw : (s16)(sSwingYaw + 0x8000));
    player->actor.gravity = -1.0f;
    sPhase = WHIP_COILED;
    DestroySwingCamera(play);
    Audio_StopSfxById(NA_SE_IT_HOOKSHOT_CHAIN);
    if (release == WHIP_RELEASE_JUMPSLASH && HasSwordOnB()) {
        sPhase = WHIP_IDLE;
        ReleaseIntoJumpSlash(player, play, forwardSpeed, lift);
        return;
    }
    ReleaseIntoJump(player, play, release, forwardSpeed, lift);
}

static bool TryReleaseSwing(Player* player, PlayState* play, f32 angularVelocity) {
    u16 pressed = play->state.input[0].press.button;

    if (pressed & BTN_B) {
        ReleaseSwing(player, play, WHIP_RELEASE_JUMPSLASH, angularVelocity);
        return true;
    }
    if (pressed & BTN_A) {
        ReleaseSwing(player, play, WHIP_RELEASE_FORWARD, angularVelocity);
        return true;
    }
    if (pressed & (BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT | BTN_CUP | FindEquippedButtonMask())) {
        ReleaseSwing(player, play, WHIP_RELEASE_PLAIN, angularVelocity);
        return true;
    }
    return false;
}

// Read as the screen reads it. The swing camera sits beyond Link along the anchor-to-Link direction, so
// stick up is away from it, which is the angle going DOWN: pushing up has to SUBTRACT (NEI adds, against
// its own comment, so the stick fought the swing). And Link's offset along the plane is sin(angle), whose
// sign flips under the anchor, so the turn has to follow it or right means right on only half of the arc.
static void PumpAndSteer(PlayState* play, f32* angularAcceleration) {
    f32 stickX = play->state.input[0].cur.stick_x / 127.0f;
    f32 stickY = play->state.input[0].cur.stick_y / 127.0f;
    f32 side = Math_SinF(sSwingAngle) < 0.0f ? -1.0f : 1.0f;

    if (sqrtf(SQ(stickX) + SQ(stickY)) <= 0.1f) {
        return;
    }
    *angularAcceleration -= stickY * WHIP_PUMP_FORCE;
    sSwingYaw += (s16)(stickX * side * WHIP_STEER_RATE);
}

static void ClampSwingAngle(void) {
    if (sSwingAngle > WHIP_MAX_SWING_ANGLE) {
        sSwingAngle = WHIP_MAX_SWING_ANGLE;
        if (sSwingVelocity > 0.0f) {
            sSwingVelocity *= -WHIP_SWING_BOUNCE;
        }
    } else if (sSwingAngle < -WHIP_MAX_SWING_ANGLE) {
        sSwingAngle = -WHIP_MAX_SWING_ANGLE;
        if (sSwingVelocity < 0.0f) {
            sSwingVelocity *= -WHIP_SWING_BOUNCE;
        }
    }
}

// Vanilla keeps its own action func through the swing and only picks an animation when its state changes, so
// the clip has to be put back whenever it does. Both skeletons carry it: the upper body is copied over the
// main one every frame the upper action claims the arms, and copying the same pose changes nothing.
static void HoldSwingAnimation(Player* player, PlayState* play) {
    LinkAnimationHeader* swing = (LinkAnimationHeader*)sSwingAnim;

    if (player->skelAnime.animation != swing) {
        LinkAnimation_Change(play, &player->skelAnime, swing, 0.0f, 0.0f, Animation_GetLastFrame(swing),
                             ANIMMODE_LOOP, -4.0f);
    }
    if (player->upperSkelAnime.animation != swing) {
        LinkAnimation_Change(play, &player->upperSkelAnime, swing, 0.0f, 0.0f, Animation_GetLastFrame(swing),
                             ANIMMODE_LOOP, -4.0f);
    }
}

// The pose follows the pendulum instead of running on its own clock. Link keeps facing the anchor the whole
// swing, so the arc behind him is the clip's back half (frame 0) and the one past the anchor its front half.
static void PoseForSwingAngle(Player* player, PlayState* play) {
    f32 frame = WHIP_SWING_MIDDLE_FRAME * (1.0f - sSwingAngle / WHIP_MAX_SWING_ANGLE);

    player->skelAnime.playSpeed = 0.0f;
    player->upperSkelAnime.playSpeed = 0.0f;
    player->skelAnime.curFrame = frame;
    player->upperSkelAnime.curFrame = frame;
    LinkAnimation_Update(play, &player->skelAnime);
    LinkAnimation_Update(play, &player->upperSkelAnime);
}

// The swing yaw points from the anchor to Link, and the swing camera sits beyond him along it, so facing the
// other way is facing into the screen and towards the anchor. It is held there for the whole swing: turning
// with the travel direction spun Link around every time the pendulum came back.
static void HangFromRope(Player* player) {
    f32 reach = Math_SinF(sSwingAngle) * sRopeLength;

    player->actor.world.pos.x = sAnchorPos.x + reach * Math_SinS(sSwingYaw);
    player->actor.world.pos.y = sAnchorPos.y - Math_CosF(sSwingAngle) * sRopeLength;
    player->actor.world.pos.z = sAnchorPos.z + reach * Math_CosS(sSwingYaw);
    FaceYaw(player, sSwingYaw + 0x8000);
}

// The swing drove Link's own skeleton, and vanilla will not put an animation back on its own.
static void DropFromSwing(Player* player, PlayState* play) {
    player->actor.world.pos.y = player->actor.floorHeight;
    player->actor.gravity = -1.0f;
    sPhase = WHIP_COILED;
    DestroySwingCamera(play);
    Audio_StopSfxById(NA_SE_IT_HOOKSHOT_CHAIN);
    player->skelAnime.playSpeed = 1.0f;
    Player_AnimPlayLoop(play, player, Player_GetIdleAnim(player));
}

static bool IsSwingInterrupted(Player* player) {
    return player->invincibilityTimer < 0 || (player->stateFlags1 & PLAYER_STATE1_IN_WATER);
}

static void UpdateSwinging(Player* player, PlayState* play) {
    f32 angularAcceleration = -WHIP_SWING_GRAVITY * Math_SinF(sSwingAngle);
    f32 previousAngle = sSwingAngle;

    if (IsSwingInterrupted(player)) {
        DropFromSwing(player, play);
        return;
    }
    player->actor.gravity = 0.0f;
    player->actor.velocity.y = 0.0f;
    HoldStill(player);
    PumpAndSteer(play, &angularAcceleration);
    sSwingVelocity = (sSwingVelocity + angularAcceleration) * WHIP_SWING_DAMPING;

    // The launch reads the speed from before the clamp: at the top of the arc the bounce has already turned
    // it around, and releasing there would throw Link the way he came from.
    f32 launchVelocity = sSwingVelocity;

    sSwingAngle += sSwingVelocity;
    ClampSwingAngle();
    if (TryReleaseSwing(player, play, launchVelocity)) {
        return;
    }
    HoldSwingAnimation(player, play);
    PoseForSwingAngle(player, play);
    HangFromRope(player);
    sTipPos = sAnchorPos;
    // One whoosh as he passes under the anchor, where the swing is fastest. NEI asks for the hookshot reel
    // every single frame instead, which drones for the whole swing.
    if ((previousAngle > 0.0f) != (sSwingAngle > 0.0f)) {
        PlaySfxAt(NA_SE_IT_SWORD_SWING, &player->actor.world.pos);
    }
    FollowSwingWithCamera(player, play);
    if (player->actor.world.pos.y <= player->actor.floorHeight + WHIP_FLOOR_TOLERANCE) {
        DropFromSwing(player, play);
    }
}

static void TakeOutWhip(PlayState* play, Player* player) {
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sLashCollider);
        sIsColliderReady = true;
    }
    Collider_SetCylinder(play, &sLashCollider, &player->actor, &sLashColliderInit);
    sPhase = WHIP_COILED;
    sAnimatedPhase = WHIP_IDLE;
    sIsLashPending = true;
    PlaySfxAt(NA_SE_PL_CHANGE_ARMS, &player->actor.world.pos);
}

// Z-targeting already points at the foe, so the button lashes straight away; otherwise it opens the aim first.
static void AimOrLash(Player* player, PlayState* play) {
    Actor* target = player->focusActor;

    if (sPhase == WHIP_AIMING) {
        s16 yaw;
        s16 pitch;

        Z64Aiming_GetDirection(player, &yaw, &pitch);
        StopAiming(player, play);
        StartLash(player, play, yaw, pitch);
        func_80839FFC(player, play);
        return;
    }
    if (sPhase != WHIP_COILED) {
        return;
    }
    if (Player_IsZTargeting(player) && target != NULL) {
        StartLash(player, play, Math_Vec3f_Yaw(&player->actor.world.pos, &target->world.pos), 0);
        return;
    }
    StartAiming(player, play);
}

static void PutWhipAway(Player* player, PlayState* play) {
    if (sPhase == WHIP_AIMING) {
        StopAiming(player, play);
    }
    if (sPhase == WHIP_SWINGING) {
        DropFromSwing(player, play);
    }
    if (player->actionFunc == AimAction) {
        func_80839FFC(player, play);
    }
    DestroySwingCamera(play);
    sLashCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    sPulledEnemy = NULL;
    sIsLashPending = false;
    sPhase = WHIP_IDLE;
    Audio_StopSfxById(NA_SE_IT_HOOKSHOT_CHAIN);
}

static bool IsChangingHeldItem(Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_START_CHANGING_HELD_ITEM) ||
           player->upperActionFunc == Player_UpperAction_ChangeHeldItem;
}

// Every press is answered from the end of the player's update, never inside the item-button pass: the aim
// takes a player action func, and one set from there is overwritten the same frame by whatever action the
// rest of the update picks. Taking the whip out also plays the item-change animation over the arm, so a
// press waits for that to finish.
static void RunPendingWhipAction(Player* player, PlayState* play) {
    if (!sIsLashPending || IsChangingHeldItem(player)) {
        return;
    }
    sIsLashPending = false;
    if (player->heldItemAction != PLAYER_IA_CUSTOM) {
        return;
    }
    if (sPhase == WHIP_COILED || sPhase == WHIP_AIMING) {
        AimOrLash(player, play);
    }
}

static void RequestWhipAction(Player* player, PlayState* play) {
    sIsLashPending = true;
}

static void RecoverFromInterruptedAim(Player* player, PlayState* play) {
    if (sPhase != WHIP_AIMING || player->actionFunc == AimAction) {
        return;
    }
    StopAiming(player, play);
}

// A struck enemy turns on Link once the stun wears off, covering half again the ground it was already covering.
static void DriveEnragedEnemy(void) {
    if (sEnragedEnemy == NULL) {
        return;
    }
    if (sEnragedEnemy->update == NULL || --sRageTimer <= 0) {
        sEnragedEnemy = NULL;
        return;
    }
    if (sRageTimer > WHIP_RAGE_FRAMES) {
        return;
    }
    sEnragedEnemy->colorFilterTimer = 2;
    sEnragedEnemy->colorFilterParams = 0x4028;
    if (sEnragedEnemy->speedXZ > 0.1f) {
        sEnragedEnemy->world.pos.x += Math_SinS(sEnragedEnemy->world.rot.y) * sEnragedEnemy->speedXZ * WHIP_RAGE_EXTRA_STRIDE;
        sEnragedEnemy->world.pos.z += Math_CosS(sEnragedEnemy->world.rot.y) * sEnragedEnemy->speedXZ * WHIP_RAGE_EXTRA_STRIDE;
    }
}

static void UpdateWhipWorld(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (play == NULL || player == NULL) {
        return;
    }
    DriveEnragedEnemy();
    RunPendingWhipAction(player, play);
    RecoverFromInterruptedAim(player, play);
    if (sPhase != WHIP_SWINGING) {
        return;
    }
    // The swing runs here, after the action func, the way NEI drives it: its position, pose and launch are
    // the last word of the frame whatever action vanilla picked. The item's upper action does not
    // necessarily run from there, so the host's held-item watchdog has to be told the whip is still out.
    Z64Items_KeepHeld(sApi);
    UpdateSwinging(player, play);
}

// A and B are the swing's own release, and vanilla would read the same press as "roll" or "draw the sword".
// The mod reads the raw input, so taking them off Link's copy for those frames costs it nothing.
static void KeepSwingButtons(Player* player, Input* input) {
    if (sPhase != WHIP_SWINGING) {
        return;
    }
    input->cur.button &= ~(BTN_A | BTN_B | BTN_R);
    input->press.button &= ~(BTN_A | BTN_B | BTN_R);
}

// Cameras and actors belong to the scene that is ending; holding their ids past it would touch freed state.
static void ForgetScene(int16_t sceneNum) {
    Z64Aiming_Drop();
    sCameraId = SUBCAM_FREE;
    sPulledEnemy = NULL;
    sEnragedEnemy = NULL;
    sIsLashPending = false;
    sHasHandMatrix = false;
    sPhase = WHIP_IDLE;
}

// Nothing touches the matrix between the hand limb and the end of the post-limb pass while the whip is out
// (no held actor, no shield), so this is the hand bone's own matrix: position and full orientation.
static void CaptureHandMatrix(PlayState* play, Player* player, int32_t limbIndex) {
    if (limbIndex != PLAYER_LIMB_R_HAND) {
        return;
    }
    Matrix_Get(&sHandMatrix);
    sHasHandMatrix = true;
}

// Every piece of the whip is modelled along +Z, so yaw then pitch aim it from its origin at its target.
static void DrawPart(PlayState* play, WhipPart* part, Gfx* displayList) {
    f32 dx = part->target.x - part->origin.x;
    f32 dy = part->target.y - part->origin.y;
    f32 dz = part->target.z - part->origin.z;

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(part->origin.x, part->origin.y, part->origin.z, MTXMODE_NEW);
    Matrix_RotateY(Math_FAtan2F(dx, dz), MTXMODE_APPLY);
    Matrix_RotateX(Math_FAtan2F(-dy, sqrtf(SQ(dx) + SQ(dz))), MTXMODE_APPLY);
    Matrix_Scale(part->scale, part->scale, part->scale, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, displayList);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The cord is the snake itself: its body piece stretched span by span down the sagging curve, each piece
// scaled to the gap it covers so they meet, and the head riding at the tip of the last one.
static void DrawLash(PlayState* play, Vec3f* from, Vec3f* to, f32 sag) {
    f32 dx = to->x - from->x;
    f32 dy = to->y - from->y;
    f32 dz = to->z - from->z;
    s32 segments =
        CLAMP((s32)(sqrtf(SQ(dx) + SQ(dy) + SQ(dz)) / WHIP_LASH_SEGMENT_LENGTH), 1, WHIP_LASH_MAX_SEGMENTS);
    WhipPart part = { *from, *from, 1.0f };

    for (s32 i = 1; i <= segments; i++) {
        f32 t = (f32)i / segments;

        part.origin = part.target;
        part.target.x = from->x + dx * t;
        part.target.y = from->y + dy * t - sag * 4.0f * t * (1.0f - t);
        part.target.z = from->z + dz * t;
        part.scale = Math_Vec3f_DistXYZ(&part.origin, &part.target) / WHIP_LASH_SEGMENT_LENGTH;
        DrawPart(play, &part, (Gfx*)sLashDL);
    }
    part.origin = part.target;
    part.target.x += dx;
    part.target.y += dy;
    part.target.z += dz;
    part.scale = WHIP_TIP_SCALE;
    DrawPart(play, &part, (Gfx*)sTipDL);
}

static void DrawWhipInHand(PlayState* play, Player* player) {
    Gfx* displayList = (sPhase == WHIP_COILED || sPhase == WHIP_AIMING) ? (Gfx*)sItemDL : (Gfx*)sGripDL;
    f32 unscale = player->actor.scale.x != 0.0f ? 1.0f / player->actor.scale.x : 1.0f;

    if (!sHasHandMatrix) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Put(&sHandMatrix);
    // That matrix carries Link's own 0.01 body scale: dividing it back out leaves the offsets in world units.
    Matrix_Scale(unscale, unscale, unscale, MTXMODE_APPLY);
    Matrix_RotateY(DEG_TO_RAD(WHIP_HAND_ROT_Y), MTXMODE_APPLY);
    Matrix_RotateX(DEG_TO_RAD(WHIP_HAND_ROT_X), MTXMODE_APPLY);
    // The offsets come after the rotations, so each slides the whip along its OWN axis.
    Matrix_Translate(0.0f, WHIP_HAND_OFFSET_Y, WHIP_HAND_OFFSET_Z, MTXMODE_APPLY);
    Matrix_Scale(WHIP_HELD_SCALE, WHIP_HELD_SCALE, WHIP_HELD_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, displayList);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawWhip(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    // The swing animation hangs Link from his left hand, so that is the hand the rope leaves from.
    s32 bodyPart = sPhase == WHIP_SWINGING ? PLAYER_BODYPART_L_HAND : PLAYER_BODYPART_R_HAND;
    Vec3f hand = player->bodyPartsPos[bodyPart];

    if (sPhase == WHIP_IDLE || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if (sPhase == WHIP_SWINGING) {
        // The grip stays inside his fist while he hangs: only the rope and the biting head show.
        DrawLash(play, &hand, &sAnchorPos, WHIP_SWING_ROPE_SAG);
        return;
    }
    DrawWhipInHand(play, player);
    if (sPhase == WHIP_AIMING) {
        Z64Aiming_DrawReticle(play, player, WHIP_EXTEND_SPEED * WHIP_LASH_FRAMES);
        return;
    }
    if (sPhase != WHIP_COILED) {
        DrawLash(play, &hand, &sTipPos, 0.0f);
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(WHIP_GIVE_SCALE, WHIP_GIVE_SCALE, WHIP_GIVE_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sItemDL);
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

    SOHCustomItemDefinition whip = Z64Items_Define(WHIP_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&whip, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&whip, 0, 2, 0);
    Z64Items_SetTextbox(&whip, "You got the %rWhip%w!&A snake-headed lash from the land of spirit tracks, and its "
                               "fangs still bite. Take aim with %y\xA1%w and press it again to crack it, or let "
                               "%g\xA4%w send it straight at a foe. It bites into %ybeams%w, %ybars%w and %ytorch "
                               "stands%w so you can swing where you cannot jump, and it drags %cKeese%w and "
                               "%cBubbles%w to your feet.");
    Z64Items_SetPauseText(&whip, "%rWhip&%wAim with %y\xA1%w, press again to lash. Tilt the stick to swing, let go "
                                 "with %y\x9F%w or %y\xA0%w.");
    Z64Items_SetAction(&whip, TakeOutWhip, UpdateWhipInHand);
    Z64Items_SetHeldCallbacks(&whip, RequestWhipAction, PutWhipAway, NULL);
    whip.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    whip.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &whip)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateWhipWorld);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, KeepSwingButtons);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, CaptureHandMatrix);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawWhip);
}
