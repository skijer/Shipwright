/**
 * item_whip.c - Whip from Spirit Tracks
 *
 * Controls:
 *   C Button:         Lash whip forward (attack/grapple)
 *   Analog (swinging): Control pendulum swing direction
 *   Release C:         Launch from swing with momentum
 *
 * Features:
 *   - Grapples beam/bar shaped surfaces for pendulum swing
 *   - Combat: paralyze enemies, pull shields, disarm
 *   - Can grab certain actors and items
 *   - Momentum-based release for traversal
 */

#include "z64.h"
#include "item_whip.h"
#include "../custom_items.h"
#include "../helpers/equip_helper.h"
#include "../helpers/combat_helper.h"
#include "../helpers/grappling_helper.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include <math.h>
#include "../anim/ballchain/ballchain_anim_data.h"
#include "assets/objects/gameplay_keep/gameplay_keep.h"

// z_player.c internal: OoT's real sword jump-attack setup — plays the jumpslash anim, routes Link into
// the jump-attack action, and launches him with linearVelocity (along shape.rot.y) + velocity.y. Used
// by the whip's B-release so you let go INTO a jump slash. Non-static → linkable. (OoT twin of MM's
// func_808395F0.) Skijer's NEI
extern void func_8083BA90(PlayState* play, Player* this, s32 meleeWeaponAnim, f32 xzVelocity, f32 yVelocity);

// =============================================================================
// Static Data
// =============================================================================
static u8 sWhipColInitialized = 0;
static s32 sWhipAnimState = -1; // Tracks animation state for upper action

// =============================================================================
// Collider Functions
// =============================================================================
static void Whip_InitCollider(PlayState* play, Player* p) {
    if (sWhipColInitialized)
        return;
    Collider_InitCylinder(play, &whipCollider);
    Collider_SetCylinder(play, &whipCollider, &p->actor, &sWhipColInit);
    sWhipColInitialized = 1;
}

static void Whip_UpdateCollider(PlayState* play, Vec3f* pos) {
    whipCollider.dim.pos.x = (s16)pos->x;
    whipCollider.dim.pos.y = (s16)(pos->y - (WHIP_COL_HEIGHT / 2));
    whipCollider.dim.pos.z = (s16)pos->z;
    whipCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &whipCollider.base);
}

// =============================================================================
// Table Lookup Functions
// =============================================================================
static s32 Whip_IsGrappleActor(Actor* actor) {
    s32 i;
    for (i = 0; i < (s32)WHIP_GRAPPLE_COUNT; i++) {
        if (actor->id == sWhipGrappleTable[i].actorId) {
            if (sWhipGrappleTable[i].params == -1 || actor->params == sWhipGrappleTable[i].params) {
                return 1;
            }
        }
    }
    return 0;
}

static s32 Whip_IsParalyzeTarget(Actor* actor) {
    s32 i;
    for (i = 0; i < (s32)WHIP_PARALYZE_COUNT; i++) {
        if (actor->id == sWhipParalyzeTable[i].actorId) {
            if (sWhipParalyzeTable[i].params == -1 || actor->params == sWhipParalyzeTable[i].params) {
                return 1;
            }
        }
    }
    return 0;
}

static s32 Whip_IsDisarmTarget(Actor* actor, WhipDisarmType* outType) {
    s32 i;
    for (i = 0; i < (s32)WHIP_DISARM_COUNT; i++) {
        if (actor->id == sWhipDisarmTable[i].actorId) {
            if (sWhipDisarmTable[i].params == -1 || actor->params == sWhipDisarmTable[i].params) {
                if (outType != NULL)
                    *outType = sWhipDisarmTable[i].type;
                return 1;
            }
        }
    }
    return 0;
}

// =============================================================================
// Enemy Interaction Functions
// =============================================================================
static void Whip_ApplyParalyze(Actor* enemy, Player* p, PlayState* play) {
    enemy->colorFilterTimer = WHIP_STUN_FRAMES;
    enemy->colorFilterParams = 0x0028; // blue tint
    enemy->speedXZ = 0.0f;
    whipPullTarget = enemy;
    whipState = WHIP_STATE_HIT_ENEMY;
    Audio_PlaySoundGeneral(WHIP_SFX_HIT_ENEMY, &enemy->world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void Whip_ApplyDisarm(Actor* enemy, WhipDisarmType type, PlayState* play) {
    enemy->home.rot.z |= WHIP_DISARMED_FLAG;
    Audio_PlaySoundGeneral(WHIP_SFX_DISARM, &enemy->world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void Whip_ApplyBoomerangDamage(Actor* enemy, Player* p, PlayState* play) {
    // AT collider already deals DMG_BOOMERANG via collision system
    // Set up rage mode countdown (activates after stun wears off)
    whipRageTarget = enemy;
    whipRageTimer = WHIP_RAGE_DURATION + WHIP_STUN_FRAMES;
    whipRageOrigSpeed = enemy->speedXZ;
    Audio_PlaySoundGeneral(WHIP_SFX_HIT_ENEMY, &enemy->world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void Whip_UpdateRage(PlayState* play) {
    if (whipRageTarget == NULL || whipRageTarget->update == NULL) {
        whipRageTarget = NULL;
        whipRageTimer = 0;
        return;
    }
    if (whipRageTimer > 0) {
        whipRageTimer--;
        // Rage activates after the initial stun wears off
        if (whipRageTimer <= WHIP_RAGE_DURATION && whipRageTimer > 0) {
            whipRageTarget->colorFilterTimer = 2;
            whipRageTarget->colorFilterParams = 0x4028; // red tint
            if (whipRageTarget->speedXZ > 0.1f) {
                whipRageTarget->speedXZ *= WHIP_RAGE_SPEED_MULT;
            }
        }
        if (whipRageTimer == 0) {
            whipRageTarget = NULL;
        }
    }
}

// =============================================================================
// Grapple Actor Proximity Check
// =============================================================================
static Actor* Whip_FindGrappleActor(PlayState* play, Vec3f* tipPos) {
    Actor* actor;
    Actor* next;
    f32 dist;
    s32 cat;

    for (cat = 0; cat < 2; cat++) {
        s32 category = (cat == 0) ? ACTORCAT_PROP : ACTORCAT_BG;
        for (actor = play->actorCtx.actorLists[category].head; actor != NULL; actor = next) {
            next = actor->next;
            if (actor->update == NULL)
                continue;
            if (!Whip_IsGrappleActor(actor))
                continue;
            dist = Math_Vec3f_DistXYZ(tipPos, &actor->world.pos);
            if (dist < WHIP_GRAPPLE_ACTOR_RADIUS) {
                return actor;
            }
        }
    }
    return NULL;
}

// =============================================================================
// Direct Actor Proximity Check (catches enemies AT collider misses)
// =============================================================================
static Actor* Whip_FindNearbyEnemy(PlayState* play, Vec3f* pos, f32 radius) {
    Actor* actor;
    Actor* next;
    s32 i;
    s32 categories[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };

    for (i = 0; i < 2; i++) {
        for (actor = play->actorCtx.actorLists[categories[i]].head; actor != NULL; actor = next) {
            next = actor->next;
            if (actor->update == NULL)
                continue;
            if (Math_Vec3f_DistXYZ(pos, &actor->world.pos) < radius) {
                return actor;
            }
        }
    }
    return NULL;
}

// =============================================================================
// Swing camera (Wind-Waker-style behind-follow) — same subcam pattern as the beetle: take control from
// CAM_ID_MAIN, sit the eye BEHIND Link along the swing direction (elevated), look at Link, and let the cam
// yaw CHASE the swing yaw slowly so it's "semi-fixed" and only medio-follows as you steer. Skijer's NEI
// =============================================================================
static void Whip_DestroySwingCam(PlayState* play) {
    if (whipSwingSubCamId != SUBCAM_FREE) {
        Camera_ChangeMode(Play_GetCamera(play, CAM_ID_MAIN), CAM_MODE_NORMAL);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
        Play_ClearCamera(play, whipSwingSubCamId);
        whipSwingSubCamId = SUBCAM_FREE;
    }
}

static void Whip_CreateSwingCam(Player* p, PlayState* play) {
    if (whipSwingSubCamId == SUBCAM_FREE) {
        whipSwingSubCamId = Play_CreateSubCamera(play);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, whipSwingSubCamId, CAM_STAT_ACTIVE);
        whipSwingCamYaw = whipSwingYaw; // start already behind the swing dir so it doesn't snap frame 1
    }
}

static void Whip_UpdateSwingCam(Player* p, PlayState* play) {
    Vec3f at, eye;
    f32 sinY, cosY;

    if (whipSwingSubCamId == SUBCAM_FREE) {
        return;
    }

    // Semi-follow: chase the swing plane's yaw a little each frame instead of snapping to it.
    Math_SmoothStepToS(&whipSwingCamYaw, whipSwingYaw, WHIP_CAM_FOLLOW_FRAC, WHIP_CAM_FOLLOW_STEP, 0x10);

    sinY = Math_SinS(whipSwingCamYaw);
    cosY = Math_CosS(whipSwingCamYaw);

    at = p->actor.world.pos;
    at.y += WHIP_CAM_AT_HEIGHT;

    // whipSwingYaw points anchor→Link (the came-from / backward dir). "Behind Link" relative to the
    // grapple is FURTHER along that vector, so eye = Link + dir*DIST looks forward toward Link + the
    // grapple beyond. (Link − dir*DIST put the cam on the anchor side aiming backward = a 180° flip.)
    eye.x = p->actor.world.pos.x + sinY * WHIP_CAM_DISTANCE;
    eye.y = p->actor.world.pos.y + WHIP_CAM_HEIGHT;
    eye.z = p->actor.world.pos.z + cosY * WHIP_CAM_DISTANCE;

    Play_CameraSetAtEye(play, whipSwingSubCamId, &at, &eye);
}

// =============================================================================
// Stop / Start
// =============================================================================
static void Whip_Stop(Player* p, PlayState* play) {
    Whip_DestroySwingCam(play);
    if (whipFirstPerson) {
        FirstPerson_Exit(p, play);
        whipFirstPerson = 0;
    }
    whipCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    whipActive = 0;
    whipState = WHIP_STATE_INACTIVE;
    whipTimer = 0;
    whipPullTarget = NULL;
    whipSwingAngle = 0.0f;
    whipSwingVel = 0.0f;
    whipRopeLength = 0.0f;
    sWhipAnimState = -1;
    p->actor.gravity = -1.0f;
    // Stop looping swing sound
    Audio_StopSfxById(WHIP_SFX_SWING);
    ItemEquip_PlayUnequipSFX(play, p);
}

static void Whip_Start(Player* p, PlayState* play) {
    if (whipActive)
        return;
    whipActive = 1;
    whipState = WHIP_STATE_EQUIP;
    whipTimer = 0;
    whipPullTarget = NULL;
    whipRageTarget = NULL;
    whipRageTimer = 0;

    // When Z-targeting with focus actor, launch immediately toward target
    if (Player_IsZTargeting(p) && p->focusActor != NULL) {
        // Use actor world.pos (body center) instead of focus.pos (head) for better aim
        f32 targetY = p->focusActor->world.pos.y + (p->focusActor->shape.yOffset * p->focusActor->scale.y);
        f32 dx = p->focusActor->world.pos.x - p->actor.world.pos.x;
        f32 dy = targetY - (p->actor.world.pos.y + WHIP_PLAYER_EYE_HEIGHT);
        f32 dz = p->focusActor->world.pos.z - p->actor.world.pos.z;
        f32 hDist = sqrtf(dx * dx + dz * dz);

        whipExtendYaw = Math_Atan2S(dx, dz);
        whipExtendPitch = 0; // Launch flat, per-frame homing adjusts pitch toward target
        whipTipPos = p->bodyPartsPos[PLAYER_BODYPART_R_HAND];
        whipTimer = WHIP_TIMER_MAX;
        whipState = WHIP_STATE_EXTENDING;

        p->actor.shape.rot.y = whipExtendYaw;
        p->actor.world.rot.y = whipExtendYaw;
        p->yaw = whipExtendYaw;

        whipFirstPerson = 0;
        Audio_PlaySoundGeneral(WHIP_SFX_THROW, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    } else {
        // Regular equip with first-person aiming
        if (!Player_IsZTargeting(p)) {
            FirstPerson_Init(p, play);
            whipFirstPerson = 1;
        } else {
            whipFirstPerson = 0;
        }
    }

    ItemEquip_PlayEquipSFX(play, p);
}

// =============================================================================
// State: Equip (coiled rope in hand, waiting for input)
// =============================================================================
static void WhipStateEquip(Player* p, PlayState* play, ItemInputState* in) {
    u8 isZTarget = Player_IsZTargeting(p);

    whipTipPos = p->bodyPartsPos[PLAYER_BODYPART_R_HAND];

    // Toggle first-person based on Z-targeting state
    if (whipFirstPerson && isZTarget) {
        FirstPerson_Exit(p, play);
        whipFirstPerson = 0;
    } else if (!whipFirstPerson && !isZTarget) {
        FirstPerson_Init(p, play);
        whipFirstPerson = 1;
    }

    // Update first-person camera each frame
    if (whipFirstPerson) {
        FirstPerson_Update(p, play);
    }

    if (in->isPressed) {
        // Get aim direction from first-person or Z-target
        if (whipFirstPerson) {
            whipExtendYaw = FirstPerson_GetAimYaw(p);
            whipExtendPitch = FirstPerson_GetAimPitch(p);
            FirstPerson_Exit(p, play);
            whipFirstPerson = 0;
        } else if (isZTarget && p->focusActor != NULL) {
            // Use actor world.pos (body center) instead of focus.pos (head) for better aim
            f32 targetY = p->focusActor->world.pos.y + (p->focusActor->shape.yOffset * p->focusActor->scale.y);
            f32 dx = p->focusActor->world.pos.x - p->actor.world.pos.x;
            f32 dy = targetY - (p->actor.world.pos.y + WHIP_PLAYER_EYE_HEIGHT);
            f32 dz = p->focusActor->world.pos.z - p->actor.world.pos.z;
            f32 hDist = sqrtf(dx * dx + dz * dz);
            whipExtendYaw = Math_Atan2S(dx, dz);
            whipExtendPitch = 0; // Launch flat, per-frame homing adjusts pitch
        } else {
            whipExtendYaw = p->actor.shape.rot.y;
            whipExtendPitch = 0;
        }

        whipTipPos = p->bodyPartsPos[PLAYER_BODYPART_R_HAND];
        whipTimer = WHIP_TIMER_MAX;
        whipState = WHIP_STATE_EXTENDING;

        p->actor.shape.rot.y = whipExtendYaw;
        p->actor.world.rot.y = whipExtendYaw;
        p->yaw = whipExtendYaw;

        Audio_PlaySoundGeneral(WHIP_SFX_THROW, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

// =============================================================================
// State: Extending (tip traveling forward)
// =============================================================================
static void WhipStateExtending(Player* p, PlayState* play) {
    Vec3f prevTip;
    Vec3f hitPos;
    CollisionPoly* hitPoly = NULL;
    s32 bgId = BGCHECK_SCENE;
    GrappleTarget target;
    Actor* grappleActor;
    f32 cosP, sinP, cosY, sinY;

    p->actor.speedXZ = 0.0f;
    p->linearVelocity = 0.0f;
    p->skelAnime.playSpeed = 0.0f;
    p->actor.shape.rot.y = whipExtendYaw;
    p->actor.world.rot.y = whipExtendYaw;
    p->yaw = whipExtendYaw;

    // Save previous tip for line test
    prevTip = whipTipPos;

    // Z-target: move tip directly toward focus actor (like ball and chain)
    if (Player_IsZTargeting(p) && p->focusActor != NULL && p->focusActor->update != NULL) {
        Vec3f target = p->focusActor->focus.pos;
        f32 dx2 = target.x - whipTipPos.x;
        f32 dy2 = target.y - whipTipPos.y;
        f32 dz2 = target.z - whipTipPos.z;
        f32 dist = sqrtf(dx2 * dx2 + dy2 * dy2 + dz2 * dz2);

        if (dist > 1.0f) {
            f32 norm = WHIP_EXTEND_SPEED / dist;
            whipTipPos.x += dx2 * norm;
            whipTipPos.y += dy2 * norm;
            whipTipPos.z += dz2 * norm;
        }
        whipExtendYaw = Math_Atan2S(dx2, dz2);
    } else {
        // Non-Z-target: use pitch/yaw angles (first-person or free aim)
        cosP = Math_CosS(whipExtendPitch);
        sinP = Math_SinS(whipExtendPitch);
        cosY = Math_CosS(whipExtendYaw);
        sinY = Math_SinS(whipExtendYaw);

        whipTipPos.x += sinY * cosP * WHIP_EXTEND_SPEED;
        whipTipPos.y -= sinP * WHIP_EXTEND_SPEED;
        whipTipPos.z += cosY * cosP * WHIP_EXTEND_SPEED;
    }

    // Update collider at new tip position
    Whip_UpdateCollider(play, &whipTipPos);

    // Check 1: Surface collision — only attach if beam/bar shaped (graspable)
    if (BgCheck_EntityLineTest1(&play->colCtx, &prevTip, &whipTipPos, &hitPos, &hitPoly, true, true, true, true,
                                &bgId)) {
        whipTipPos = hitPos;
        Grapple_AnalyzeSurface(play, hitPoly, bgId, &hitPos, &target);

        if (target.isGraspable) {
            // Beam/bar shaped surface — attach and swing
            whipAttachPos = hitPos;
            whipAttachNormal = target.surfaceNormal;
            whipAttachedBgId = bgId;
            whipState = WHIP_STATE_ATTACHED;
            Audio_PlaySoundGeneral(WHIP_SFX_HIT_SURFACE, &hitPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            return;
        }

        // Not graspable (flat wall, floor, etc.) — retract
        whipState = WHIP_STATE_RETRACTING;
        return;
    }

    // Check 2: Graspable actor proximity
    grappleActor = Whip_FindGrappleActor(play, &whipTipPos);
    if (grappleActor != NULL) {
        whipAttachPos = grappleActor->world.pos;
        whipAttachPos.y += WHIP_GRAPPLE_ACTOR_Y_OFFSET;
        whipAttachNormal.x = 0.0f;
        whipAttachNormal.y = 1.0f;
        whipAttachNormal.z = 0.0f;
        whipAttachedBgId = BGCHECK_SCENE;
        whipState = WHIP_STATE_ATTACHED;
        Audio_PlaySoundGeneral(WHIP_SFX_HIT_SURFACE, &grappleActor->world.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }

    // Check 3: Enemy hit via collider AT
    if (whipCollider.base.atFlags & AT_HIT) {
        Actor* hitActor = whipCollider.base.at;
        whipCollider.base.atFlags &= ~AT_HIT;

        if (hitActor != NULL && hitActor->update != NULL) {
            WhipDisarmType disarmType;

            if (Whip_IsParalyzeTarget(hitActor)) {
                Whip_ApplyParalyze(hitActor, p, play);
                return;
            }
            if (Whip_IsDisarmTarget(hitActor, &disarmType)) {
                Whip_ApplyDisarm(hitActor, disarmType, play);
                whipState = WHIP_STATE_RETRACTING;
                return;
            }
            Whip_ApplyBoomerangDamage(hitActor, p, play);
            whipState = WHIP_STATE_RETRACTING;
            return;
        }
    }

    // Check 4: Direct enemy proximity (catches enemies without AC colliders)
    {
        Actor* nearEnemy = Whip_FindNearbyEnemy(play, &whipTipPos, WHIP_ENEMY_DETECT_RADIUS);
        if (nearEnemy != NULL) {
            WhipDisarmType disarmType;

            if (Whip_IsParalyzeTarget(nearEnemy)) {
                Whip_ApplyParalyze(nearEnemy, p, play);
                return;
            }
            if (Whip_IsDisarmTarget(nearEnemy, &disarmType)) {
                Whip_ApplyDisarm(nearEnemy, disarmType, play);
                whipState = WHIP_STATE_RETRACTING;
                return;
            }
            Whip_ApplyBoomerangDamage(nearEnemy, p, play);
            whipState = WHIP_STATE_RETRACTING;
            return;
        }
    }

    // Check 5: Timer expired
    whipTimer--;
    if (whipTimer <= 0) {
        whipState = WHIP_STATE_RETRACTING;
    }
}

// =============================================================================
// State: HitEnemy (paralyze + pull toward Link)
// =============================================================================
static void WhipStateHitEnemy(Player* p, PlayState* play) {
    f32 dx, dy, dz, dist, norm;

    p->actor.speedXZ = 0.0f;
    p->linearVelocity = 0.0f;
    p->skelAnime.playSpeed = 0.0f;

    if (whipPullTarget == NULL || whipPullTarget->update == NULL) {
        whipPullTarget = NULL;
        whipState = WHIP_STATE_RETRACTING;
        return;
    }

    whipTipPos = whipPullTarget->world.pos;

    dx = p->actor.world.pos.x - whipPullTarget->world.pos.x;
    dy = (p->actor.world.pos.y + WHIP_PULL_HEIGHT_OFFSET) - whipPullTarget->world.pos.y;
    dz = p->actor.world.pos.z - whipPullTarget->world.pos.z;
    dist = sqrtf(dx * dx + dy * dy + dz * dz);

    if (dist < WHIP_PULL_ARRIVE_DIST) {
        whipPullTarget = NULL;
        whipState = WHIP_STATE_RETRACTING;
        return;
    }

    if (dist > 0.1f) {
        norm = WHIP_PULL_SPEED / dist;
        whipPullTarget->world.pos.x += dx * norm;
        whipPullTarget->world.pos.y += dy * norm;
        whipPullTarget->world.pos.z += dz * norm;
    }

    whipPullTarget->speedXZ = 0.0f;
    Actor_PlaySfx_Flagged(&p->actor, WHIP_SFX_SWING);
}

// =============================================================================
// State: Attached (setup swing parameters, immediate transition to SWINGING)
// =============================================================================
static void WhipStateAttached(Player* p, PlayState* play) {
    f32 dx, dz, hDist, vDist;

    // Fixed rope length: always 2 adult Links tall
    whipRopeLength = WHIP_FIXED_ROPE_LENGTH;

    dx = p->actor.world.pos.x - whipAttachPos.x;
    dz = p->actor.world.pos.z - whipAttachPos.z;
    whipSwingYaw = Math_Atan2S(dx, dz);

    hDist = sqrtf(dx * dx + dz * dz);
    vDist = whipAttachPos.y - p->actor.world.pos.y;

    if (vDist > 0.1f) {
        whipSwingAngle = atan2f(hDist, vDist);
    } else {
        whipSwingAngle = WHIP_MAX_ANGLE * 0.5f;
    }

    whipSwingVel = 0.0f;
    whipState = WHIP_STATE_SWINGING;

    // Hand the camera to the dedicated Wind-Waker-style swing cam for the whole swing.
    Whip_CreateSwingCam(p, play);
}

// =============================================================================
// State: Swinging (pendulum physics)
// =============================================================================
static void WhipStateSwinging(Player* p, PlayState* play, ItemInputState* in) {
    f32 angAccel, stickInputX, stickInputY;
    f32 sinA, cosA, swingDirX, swingDirZ;
    f32 releaseVel;

    // Disable normal player physics
    p->actor.gravity = 0.0f;
    p->actor.velocity.y = 0.0f;
    p->actor.speedXZ = 0.0f;
    p->linearVelocity = 0.0f;
    p->skelAnime.playSpeed = 0.0f;

    // Apply ball chain spin pose (arms raised, holding whip)
    p->skelAnime.jointTable[PLAYER_LIMB_L_SHOULDER].x = BC_SPIN_L_SHOULDER_X;
    p->skelAnime.jointTable[PLAYER_LIMB_L_SHOULDER].y = BC_SPIN_L_SHOULDER_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_L_SHOULDER].z = BC_SPIN_L_SHOULDER_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_L_FOREARM].x = BC_SPIN_L_FOREARM_X;
    p->skelAnime.jointTable[PLAYER_LIMB_L_FOREARM].y = BC_SPIN_L_FOREARM_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_L_FOREARM].z = BC_SPIN_L_FOREARM_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_L_HAND].x = BC_SPIN_L_HAND_X;
    p->skelAnime.jointTable[PLAYER_LIMB_L_HAND].y = BC_SPIN_L_HAND_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_L_HAND].z = BC_SPIN_L_HAND_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_R_SHOULDER].x = BC_SPIN_R_SHOULDER_X;
    p->skelAnime.jointTable[PLAYER_LIMB_R_SHOULDER].y = BC_SPIN_R_SHOULDER_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_R_SHOULDER].z = BC_SPIN_R_SHOULDER_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_R_FOREARM].x = BC_SPIN_R_FOREARM_X;
    p->skelAnime.jointTable[PLAYER_LIMB_R_FOREARM].y = BC_SPIN_R_FOREARM_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_R_FOREARM].z = BC_SPIN_R_FOREARM_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_R_HAND].x = BC_SPIN_R_HAND_X;
    p->skelAnime.jointTable[PLAYER_LIMB_R_HAND].y = BC_SPIN_R_HAND_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_R_HAND].z = BC_SPIN_R_HAND_Z;

    // Pendulum: angular acceleration from gravity
    angAccel = -WHIP_GRAVITY * sinf(whipSwingAngle);

    // CAMERA/SCREEN-relative steering. Under the swing follow-cam the analog stick is already
    // screen-relative, so use its two axes DIRECTLY and consistently — no swing-plane decomposition
    // (that rotated the mapping with the plane and flipped on the back-swing):
    //   • stick LEFT/RIGHT → rotate the swing plane (steer). Same on-screen meaning at all times.
    //   • stick UP/DOWN    → pump. Up drives the swing forward/away from the camera, down back.
    stickInputX = (f32)play->state.input[0].cur.stick_x / 127.0f;
    stickInputY = (f32)play->state.input[0].cur.stick_y / 127.0f;
    {
        f32 stickMag = sqrtf(stickInputX * stickInputX + stickInputY * stickInputY);
        if (stickMag > 0.1f) {
            angAccel += stickInputY * WHIP_INPUT_FORCE;
            whipSwingYaw += (s16)(stickInputX * WHIP_YAW_TURN_RATE);
        }
    }

    // Integrate
    whipSwingVel = (whipSwingVel + angAccel) * WHIP_DAMPING;
    releaseVel = whipSwingVel; // Save velocity BEFORE angle clamp for release
    whipSwingAngle += whipSwingVel;

    // Clamp angle with soft bounce (preserve energy instead of zeroing)
    if (whipSwingAngle > WHIP_MAX_ANGLE) {
        whipSwingAngle = WHIP_MAX_ANGLE;
        if (whipSwingVel > 0.0f)
            whipSwingVel = -whipSwingVel * WHIP_SWING_BOUNCE;
    }
    if (whipSwingAngle < -WHIP_MAX_ANGLE) {
        whipSwingAngle = -WHIP_MAX_ANGLE;
        if (whipSwingVel < 0.0f)
            whipSwingVel = -whipSwingVel * WHIP_SWING_BOUNCE;
    }

    // Calculate player position from pendulum
    sinA = sinf(whipSwingAngle);
    cosA = cosf(whipSwingAngle);
    swingDirX = Math_SinS(whipSwingYaw);
    swingDirZ = Math_CosS(whipSwingYaw);

    p->actor.world.pos.x = whipAttachPos.x + sinA * swingDirX * whipRopeLength;
    p->actor.world.pos.y = whipAttachPos.y - cosA * whipRopeLength;
    p->actor.world.pos.z = whipAttachPos.z + sinA * swingDirZ * whipRopeLength;

    // Tip at attach point (for rope rendering)
    whipTipPos = whipAttachPos;

    // Face swing direction
    if (whipSwingVel > 0.001f) {
        p->actor.shape.rot.y = whipSwingYaw;
    } else if (whipSwingVel < -0.001f) {
        p->actor.shape.rot.y = whipSwingYaw + 0x8000;
    }

    Actor_PlaySfx_Flagged(&p->actor, WHIP_SFX_SWING);

    // Keep the swing camera behind Link (semi-follows the swing yaw).
    Whip_UpdateSwingCam(p, play);

    // Ground contact check: if Link touches the floor, unequip
    if (p->actor.world.pos.y <= p->actor.floorHeight + WHIP_FLOOR_THRESHOLD) {
        p->actor.world.pos.y = p->actor.floorHeight;
        p->actor.gravity = -1.0f;
        Whip_Stop(p, play);
        return;
    }

    // Release the swing. Any of A / B / C-buttons / the whip button lets go, but A and B differ:
    //   • B  → let go straight INTO a sword jump slash, carrying the full swing momentum.
    //   • A  → let go keeping the FORWARD (horizontal) momentum, but only 1/4 of the vertical.
    //   • whip button / C-buttons → plain release: full momentum coast + fall.
    {
        u16 pressed = play->state.input[0].press.button;
        u8 releaseB = (pressed & BTN_B) != 0;
        u8 releaseA = (pressed & BTN_A) != 0;
        u8 releasePlain = in->isPressed || (pressed & (BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT | BTN_CUP));

        if (releaseA || releaseB || releasePlain) {
            // Pre-clamp velocity for true momentum.
            f32 omega = releaseVel;
            f32 cosTheta = cosf(whipSwingAngle);
            f32 sinTheta = sinf(whipSwingAngle);
            f32 tangentialSpeed = omega * whipRopeLength * WHIP_RELEASE_BOOST;
            f32 hSpeed = cosTheta * tangentialSpeed; // Signed horizontal speed
            f32 vSpeed = sinTheta * tangentialSpeed; // Vertical speed

            // Face the momentum direction (shared by every release path).
            s16 momentumYaw = (hSpeed >= 0.0f) ? whipSwingYaw : (s16)(whipSwingYaw + 0x8000);
            p->actor.shape.rot.y = momentumYaw;
            p->actor.world.rot.y = momentumYaw;
            p->yaw = momentumYaw;

            if (releaseB) {
                // --- B: release straight into a JUMP SLASH, carrying the swing momentum ---
                f32 hMag = fabsf(hSpeed);
                if (hMag > WHIP_MAX_RELEASE_SPEED) {
                    hMag = WHIP_MAX_RELEASE_SPEED;
                }
                Whip_Stop(p, play);                 // fully drop the whip (subcam + camera back to Link, clears state)
                p->actor.shape.rot.y = momentumYaw; // re-affirm facing (func_8083BA90 launches along it)
                p->yaw = momentumYaw;
                // OoT's real sword jump-attack: sets linearVelocity + velocity.y from these + enters the
                // jump-attack action.
                func_8083BA90(play, p, PLAYER_MWA_JUMPSLASH_START, hMag,
                              (vSpeed < WHIP_MIN_LAUNCH_VY) ? WHIP_MIN_LAUNCH_VY : vSpeed);
                whipTimer = WHIP_JUMPSLASH_LOCKOUT; // block a held-button re-equip while the jumpslash runs
                sWhipAnimState = -1;
                return;
            }

            // --- A / plain: set launch velocity, coast a few frames, then fall ---
            p->actor.velocity.x = hSpeed * swingDirX;
            p->actor.velocity.z = hSpeed * swingDirZ;
            p->actor.velocity.y = vSpeed;

            p->actor.speedXZ = fabsf(hSpeed);
            if (p->actor.speedXZ > WHIP_MAX_RELEASE_SPEED) {
                f32 scale = WHIP_MAX_RELEASE_SPEED / p->actor.speedXZ;
                p->actor.velocity.x *= scale;
                p->actor.velocity.z *= scale;
                p->actor.speedXZ = WHIP_MAX_RELEASE_SPEED;
            }
            p->linearVelocity = p->actor.speedXZ;

            if (releaseA) {
                // A: keep the forward momentum, quarter the vertical. No min-upward nudge — a flatter,
                // forward launch (you carry your speed out, not a lob).
                p->actor.velocity.y = vSpeed * WHIP_A_RELEASE_VY_FRAC;
            } else if (p->actor.velocity.y < WHIP_MIN_LAUNCH_VY) {
                // plain: minimum upward nudge so Link doesn't just drop.
                p->actor.velocity.y = WHIP_MIN_LAUNCH_VY;
            }

            // Enter coast state: keep whip active briefly so the engine doesn't reset momentum before
            // position integration picks it up.
            p->actor.gravity = -1.0f;
            whipState = WHIP_STATE_LAUNCHED;
            whipTimer = WHIP_LAUNCH_COAST_FRAMES;
            sWhipAnimState = -1;
            // Give the camera back to Link so the launch arc uses the normal follow cam.
            Whip_DestroySwingCam(play);
            return;
        }
    }
}

// =============================================================================
// State: Launched (coast — preserve momentum for a few frames after release)
// =============================================================================
static void WhipStateLaunched(Player* p, PlayState* play) {
    // Do NOT zero speedXZ or linearVelocity — let momentum carry forward.
    // Gravity is already set to -1.0f, so the player falls naturally.
    // The engine uses speedXZ + world.rot.y for horizontal movement in air.
    whipTimer--;
    if (whipTimer <= 0) {
        Whip_Stop(p, play);
    }
}

// =============================================================================
// State: Retracting (rope returning to Link)
// =============================================================================
static void WhipStateRetracting(Player* p, PlayState* play) {
    Vec3f handPos;
    f32 dx, dy, dz, dist, norm;

    handPos = p->bodyPartsPos[PLAYER_BODYPART_R_HAND];

    dx = handPos.x - whipTipPos.x;
    dy = handPos.y - whipTipPos.y;
    dz = handPos.z - whipTipPos.z;
    dist = sqrtf(dx * dx + dy * dy + dz * dz);

    if (dist < WHIP_ARRIVE_DIST) {
        whipTipPos = handPos;
        whipState = WHIP_STATE_EQUIP;
        Audio_PlaySoundGeneral(WHIP_SFX_RETRACT, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }

    if (dist > 0.1f) {
        norm = WHIP_RETRACT_SPEED / dist;
        whipTipPos.x += dx * norm;
        whipTipPos.y += dy * norm;
        whipTipPos.z += dz * norm;
    }

    Actor_PlaySfx_Flagged(&p->actor, WHIP_SFX_SWING);
}

// =============================================================================
// Public API
// =============================================================================
void Handle_Whip(Player* p, PlayState* play) {
    ItemInputState in;

    if (!sWhipColInitialized) {
        Whip_InitCollider(play, p);
    }

    // Rage mode runs independently of whip state
    Whip_UpdateRage(play);

    ItemInput_Update(&in, ITEM_WHIP, p, play);

    if (!in.wasEquipped || ItemInput_IsBlockedEx(p, play, 1) || ItemInput_CheckDamage(p, &whipPrevInvinc)) {
        if (whipActive)
            Whip_Stop(p, play);
        return;
    }
    if (in.otherButtonPressed && whipState != WHIP_STATE_SWINGING) {
        // A/B/C pressed: normally stop the whip. BUT while SWINGING, do NOT stop-in-place here — fall
        // through to WhipStateSwinging so it can release WITH momentum per button (B = jumpslash,
        // A = forward launch, C/whip = plain launch). Also never interrupt the post-release coast.
        if (whipActive && whipState != WHIP_STATE_LAUNCHED) {
            Whip_Stop(p, play);
        }
        return;
    }

    if (!whipActive) {
        if (whipTimer > 0) {
            // Post-jumpslash lockout: a held item button must not immediately re-equip the whip and
            // cancel the sword jump-attack from a B-release. Skijer's NEI
            whipTimer--;
            return;
        }
        if (in.isPressed || in.isHeld) {
            Whip_Start(p, play);
        }
        return;
    }

    switch (whipState) {
        case WHIP_STATE_EQUIP:
            WhipStateEquip(p, play, &in);
            break;
        case WHIP_STATE_EXTENDING:
            WhipStateExtending(p, play);
            break;
        case WHIP_STATE_HIT_ENEMY:
            WhipStateHitEnemy(p, play);
            break;
        case WHIP_STATE_ATTACHED:
            WhipStateAttached(p, play);
            break;
        case WHIP_STATE_SWINGING:
            WhipStateSwinging(p, play, &in);
            break;
        case WHIP_STATE_RETRACTING:
            WhipStateRetracting(p, play);
            break;
        case WHIP_STATE_LAUNCHED:
            WhipStateLaunched(p, play);
            break;
        default:
            whipState = WHIP_STATE_EQUIP;
            break;
    }
}

void Player_InitWhipIA(PlayState* play, Player* p) {
    Whip_InitCollider(play, p);
    whipActive = 0;
    whipState = WHIP_STATE_INACTIVE;
    whipTimer = 0;
    whipPullTarget = NULL;
    whipRageTarget = NULL;
    whipRageTimer = 0;
    whipSwingAngle = 0.0f;
    whipSwingVel = 0.0f;
    whipRopeLength = 0.0f;
    whipFirstPerson = 0;
    whipSwingSubCamId = SUBCAM_FREE;
    whipSwingCamYaw = 0;
    sWhipAnimState = -1;
}

s32 Player_UpperAction_Whip(Player* p, PlayState* play) {
    // Not active: let lower body control everything
    if (!whipActive) {
        sWhipAnimState = -1;
        return 0;
    }

    // Detect state transitions and play appropriate animation
    if ((s32)whipState != sWhipAnimState) {
        sWhipAnimState = whipState;
        switch (whipState) {
            case WHIP_STATE_EQUIP:
                // Idle holding pose (boomerang wait)
                LinkAnimation_PlayLoop(play, &p->upperSkelAnime, &gPlayerAnim_link_boom_throw_waitR);
                break;
            case WHIP_STATE_EXTENDING:
                // Throw animation (one-handed swing forward)
                LinkAnimation_PlayOnce(play, &p->upperSkelAnime, &gPlayerAnim_link_boom_throwR);
                break;
            case WHIP_STATE_RETRACTING:
                // Keep throw pose while retracting
                break;
            case WHIP_STATE_ATTACHED:
            case WHIP_STATE_SWINGING:
                // Swinging uses joint override from WhipStateSwinging, no anim needed
                break;
            case WHIP_STATE_LAUNCHED:
                // Post-release: let lower body handle falling animation
                return 0;
        }
    }

    // Advance animation and handle transitions when finished
    if (LinkAnimation_Update(play, &p->upperSkelAnime)) {
        switch (whipState) {
            case WHIP_STATE_EXTENDING:
                // Hold at end of throw during extension
                break;
            case WHIP_STATE_RETRACTING:
                // Return to wait pose
                LinkAnimation_PlayLoop(play, &p->upperSkelAnime, &gPlayerAnim_link_boom_throw_waitR);
                break;
            default:
                break;
        }
    }

    return 1;
}
