/**
 * item_beetle.c - Beetle from Skyward Sword
 *
 * Controls:
 *   C Button:    Launch beetle in aimed direction
 *   Analog:      Steer beetle flight path
 *   C Button:    Recall beetle early
 *   B Button:    Boost speed temporarily
 *
 * Features:
 *   - Remote-controlled flying beetle with camera follow
 *   - Can grab and carry items back to Link
 *   - Damages enemies on impact
 *   - Limited flight time before returning
 */

#include "z64.h"
#include "item_beetle.h"
#include "../custom_items.h"
#include "../helpers/camera_helper.h"
#include "../helpers/equip_helper.h"
#include "../helpers/combat_helper.h"
#include "../helpers/item_voice.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h" // gGlowCircleDL for our own target indicator
#include "assets/objects/gameplay_keep/gameplay_keep.h"

static u8 sBeetleColInitialized = 0;
static s8 sBeetlePrevInvinc = 0;
static Actor* sBeetleTarget = NULL;    // Z-locked enemy (NULL = no lock). Skijer's NEI
static Actor* sBeetleCandidate = NULL; // nearest lockable actor each frame (drives the "offer" reticle)
static u8 sBeetleKamikaze = 0;         // B-released: home hard at the target to hit it, then return
static u8 sBeetleAutonomous = 0;       // B pressed: camera+control back to Link; beetle flies on its own

static void Beetle_DropGrabbedActor(Player* p);

static ColliderCylinderInit sBeetleColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { BEETLE_DMG_FLAGS, 0x00, 0x01 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_ON },
    { (s16)BEETLE_DAMAGE_RADIUS, (s16)BEETLE_DAMAGE_HEIGHT, 0, { 0, 0, 0 } }
};

static void Beetle_InitCollider(PlayState* play, Player* p) {
    if (sBeetleColInitialized)
        return;
    Collider_InitCylinder(play, &beetleCollider);
    Collider_SetCylinder(play, &beetleCollider, &p->actor, &sBeetleColliderInit);
    sBeetleColInitialized = 1;
}

static void Beetle_UpdateCollider(PlayState* play, Vec3f* pos) {
    beetleCollider.dim.pos.x = (s16)pos->x;
    beetleCollider.dim.pos.y = (s16)pos->y;
    beetleCollider.dim.pos.z = (s16)pos->z;
    beetleCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &beetleCollider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &beetleCollider.base);
}

static void Beetle_PlaySound(Vec3f* pos, u16 sfxId) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void Beetle_PlayLoopSound(Actor* actor, u16 sfxId) {
    Actor_PlaySfx_Flagged(actor, sfxId - SFX_FLAG);
}

u8 Beetle_IsFlying(void) {
    return beetleActive && (beetleState == BEETLE_STATE_FLYING || beetleState == BEETLE_STATE_RETURNING);
}

static void Beetle_DestroySubCam(PlayState* play) {
    if (beetleSubCamId != SUBCAM_FREE) {
        // Force CAM_ID_MAIN out of CAM_MODE_FOLLOWBOOMERANG before reactivating it.
        // While the beetle flew we set PLAYER_STATE1_BOOMERANG_THROWN, which
        // makes z_player.c put CAM_ID_MAIN into FOLLOWBOOMERANG mode pointed at
        // a stale Player.boomerangActor. Reactivating in that mode can deref
        // freed memory (Camera_KeepOn1) and crash — common during Barinade
        // phase 4 where actor churn fills the freed En_Boom slot with valid-
        // looking data, defeating the camera->target->update == NULL guard.
        Camera_ChangeMode(Play_GetCamera(play, CAM_ID_MAIN), CAM_MODE_NORMAL);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
        Play_ClearCamera(play, beetleSubCamId);
        beetleSubCamId = SUBCAM_FREE;
    }
}

static void Beetle_CreateSubCam(PlayState* play) {
    if (beetleSubCamId == SUBCAM_FREE) {
        beetleSubCamId = Play_CreateSubCamera(play);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, beetleSubCamId, CAM_STAT_ACTIVE);
    }
}

static void Beetle_UpdateSubCam(PlayState* play) {
    if (beetleSubCamId == SUBCAM_FREE)
        return;

    f32 sinY = Math_SinS(beetleRot.y);
    f32 cosY = Math_CosS(beetleRot.y);
    f32 sinP = Math_SinS(beetleRot.x);
    f32 cosP = Math_CosS(beetleRot.x);

    Vec3f eye;
    eye.x = beetlePos.x - sinY * cosP * BEETLE_CAM_DISTANCE;
    eye.y = beetlePos.y - BEETLE_CAM_HEIGHT + sinP * BEETLE_CAM_DISTANCE;
    eye.z = beetlePos.z - cosY * cosP * BEETLE_CAM_DISTANCE;

    Vec3f at = beetlePos;

    Play_CameraSetAtEye(play, beetleSubCamId, &at, &eye);
}

static void Beetle_Stop(Player* p, PlayState* play) {
    if (beetleFirstPerson) {
        FirstPerson_Exit(p, play);
        beetleFirstPerson = 0;
    }
    Beetle_DestroySubCam(play);
    p->stateFlags1 &= ~PLAYER_STATE1_BOOMERANG_THROWN;
    p->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    beetleCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    beetleActive = 0;
    beetleState = BEETLE_STATE_IDLE;
    beetleGrabbed = NULL;
    sBeetleTarget = NULL; // drop lock + reticle
    sBeetleKamikaze = 0;
    sBeetleAutonomous = 0;
    p->focusActor = NULL;
    // Stop looping fly sound
    Audio_StopSfxById(BEETLE_SFX_FLY);
    ItemEquip_PlayUnequipSFX(play, p);
}

static void Beetle_Start(Player* p, PlayState* play) {
    if (beetleActive)
        return;
    beetleActive = 1;
    beetleState = BEETLE_STATE_AIMING;
    beetleFirstPerson = 1;
    beetleGrabbed = NULL;
    beetleWingScale = BEETLE_WING_SCALE_MAX;
    beetleWingDir = -1;
    beetleTimer = BEETLE_MAX_TIME;

    LinkAnimation_PlayLoop(play, &p->upperSkelAnime, &gPlayerAnim_link_boom_throw_waitR);

    FirstPerson_Init(p, play);
    ItemEquip_PlayEquipSFX(play, p);
}

static void Beetle_Launch(Player* p, PlayState* play) {
    beetleState = BEETLE_STATE_FLYING;
    beetleTimer = BEETLE_MAX_TIME;
    beetleStartPos = p->actor.world.pos;
    sBeetleTarget = NULL; // fresh flight: no lock yet
    sBeetleKamikaze = 0;
    sBeetleAutonomous = 0;

    s16 launchYaw = FirstPerson_GetAimYaw(p);
    s16 launchPitch = FirstPerson_GetAimPitch(p);

    beetlePos.x = p->actor.world.pos.x + Math_SinS(launchYaw) * BEETLE_LAUNCH_OFFSET_XZ;
    beetlePos.y = p->actor.world.pos.y + BEETLE_LAUNCH_OFFSET_Y;
    beetlePos.z = p->actor.world.pos.z + Math_CosS(launchYaw) * BEETLE_LAUNCH_OFFSET_XZ;

    beetleRot.x = launchPitch;
    beetleRot.y = launchYaw;
    beetleRot.z = 0;

    LinkAnimation_PlayOnce(play, &p->upperSkelAnime, &gPlayerAnim_link_boom_throwR);

    FirstPerson_Exit(p, play);
    beetleFirstPerson = 0;
    Beetle_CreateSubCam(play);

    Beetle_PlaySound(&p->actor.world.pos, BEETLE_SFX_LAUNCH);
    ItemVoice_Play(p, NA_SE_VO_LI_SWORD_N, NA_SE_VO_LI_SWORD_N_KID);
}

static void Beetle_StartReturn(Player* p, PlayState* play) {
    beetleState = BEETLE_STATE_RETURNING;
    sBeetleTarget = NULL; // clear the Z-lock / kamikaze so the next flight starts fresh
    sBeetleKamikaze = 0;
    sBeetleAutonomous = 0;
    p->focusActor = NULL; // drop the lock-on reticle
    Beetle_DestroySubCam(play);
    p->stateFlags1 &= ~PLAYER_STATE1_BOOMERANG_THROWN;
    p->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    Beetle_PlaySound(&beetlePos, BEETLE_SFX_RETURN);
}

static void Beetle_Catch(Player* p, PlayState* play) {
    LinkAnimation_PlayOnce(play, &p->upperSkelAnime, &gPlayerAnim_link_boom_catch);

    Beetle_PlaySound(&p->actor.world.pos, BEETLE_SFX_CATCH);
    ItemVoice_Play(p, NA_SE_VO_LI_SWORD_N, NA_SE_VO_LI_SWORD_N_KID);

    Beetle_DropGrabbedActor(p);

    beetleActive = 0;
    beetleState = BEETLE_STATE_IDLE;
}

static void Beetle_Move(f32 speed) {
    f32 cosP = Math_CosS(beetleRot.x);
    f32 sinP = Math_SinS(beetleRot.x);
    f32 sinY = Math_SinS(beetleRot.y);
    f32 cosY = Math_CosS(beetleRot.y);

    beetlePos.x += sinY * cosP * speed;
    beetlePos.y -= sinP * speed;
    beetlePos.z += cosY * cosP * speed;
}

// Reliable proximity grab for collectibles the AT pass can miss: the silver rupee (En_G_Switch)'s AC
// reacts to specific dmgFlags the beetle's AT doesn't set, so AT-AC never fires — grab it (and rupees)
// by distance instead, then carry it to Link. Skijer's NEI
static void Beetle_ProximityGrab(PlayState* play) {
    static const u8 sCats[] = { ACTORCAT_ITEMACTION, ACTORCAT_PROP };
    s32 c;

    if (beetleGrabbed != NULL) {
        return;
    }
    for (c = 0; c < ARRAY_COUNT(sCats); c++) {
        Actor* actor = play->actorCtx.actorLists[sCats[c]].head;
        while (actor != NULL) {
            u8 grabbable = (actor->id == ACTOR_EN_ITEM00) ||
                           (actor->id == ACTOR_EN_G_SWITCH && (actor->params >> 0xC & 0xF) == ENGSWITCH_SILVER_RUPEE);
            if ((actor->update != NULL) && grabbable &&
                (Math_Vec3f_DistXYZ(&beetlePos, &actor->world.pos) < BEETLE_GRAB_RADIUS * 2.0f)) {
                beetleGrabbed = actor;
                if (actor->id == ACTOR_EN_G_SWITCH) {
                    actor->flags |= ACTOR_FLAG_HOOKSHOT_ATTACHED;
                }
                return;
            }
            actor = actor->next;
        }
    }
}

static u8 Beetle_CheckActorCollision(Player* p, PlayState* play) {
    if (!(beetleCollider.base.atFlags & AT_HIT))
        return 0;

    Actor* hitActor = beetleCollider.base.at;
    u8 shouldReturn = 0;

    if (hitActor != NULL) {
        if (hitActor->id == ACTOR_EN_ITEM00 || hitActor->id == ACTOR_EN_SI ||
            (hitActor->id == ACTOR_EN_G_SWITCH && (hitActor->params >> 0xC & 0xF) == ENGSWITCH_SILVER_RUPEE)) {
            beetleGrabbed = hitActor;
            if (hitActor->id == ACTOR_EN_SI) {
                hitActor->flags |= ACTOR_FLAG_HOOKSHOT_ATTACHED;
            } else if (hitActor->id == ACTOR_EN_G_SWITCH) {
                hitActor->flags |= ACTOR_FLAG_HOOKSHOT_ATTACHED;
            }
        } else {
            Beetle_PlaySound(&beetlePos, BEETLE_SFX_HIT);
            shouldReturn = 1;
        }
    }
    beetleCollider.base.atFlags &= ~AT_HIT;
    return shouldReturn;
}

static u8 Beetle_CheckGeometryCollision(PlayState* play) {
    Vec3f hitPoint;
    CollisionPoly* hitPoly = NULL;
    s32 hitDynaId = 0;

    f32 cosP = Math_CosS(beetleRot.x);
    f32 sinP = Math_SinS(beetleRot.x);
    f32 sinY = Math_SinS(beetleRot.y);
    f32 cosY = Math_CosS(beetleRot.y);

    Vec3f prevPos = beetlePos;
    prevPos.x -= sinY * cosP * BEETLE_SPEED;
    prevPos.y += sinP * BEETLE_SPEED;
    prevPos.z -= cosY * cosP * BEETLE_SPEED;

    if (BgCheck_EntityLineTest1(&play->colCtx, &prevPos, &beetlePos, &hitPoint, &hitPoly, true, true, true, true,
                                &hitDynaId)) {
        beetlePos = hitPoint;
        Beetle_PlaySound(&beetlePos, BEETLE_SFX_HIT);
        return 1;
    }
    return 0;
}

static void Beetle_UpdateGrabbedActor(void) {
    if (beetleGrabbed == NULL)
        return;
    if (beetleGrabbed->update == NULL) {
        beetleGrabbed = NULL;
        return;
    }
    Math_Vec3f_Copy(&beetleGrabbed->world.pos, &beetlePos);
}

static void Beetle_DropGrabbedActor(Player* p) {
    if (beetleGrabbed == NULL)
        return;

    Math_Vec3f_Copy(&beetleGrabbed->world.pos, &p->actor.world.pos);
    if (beetleGrabbed->id == ACTOR_EN_ITEM00) {
        beetleGrabbed->gravity = -0.9f;
        beetleGrabbed->bgCheckFlags &= ~0x03;
    } else if (beetleGrabbed->id == ACTOR_EN_SI) {
        beetleGrabbed->flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
    } else if (beetleGrabbed->id == ACTOR_EN_G_SWITCH) {
        // Silver rupee - drop near Link so it can be collected
        beetleGrabbed->flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
        beetleGrabbed->gravity = -2.0f;
        beetleGrabbed->bgCheckFlags &= ~0x03;
    }
    beetleGrabbed = NULL;
}

static void Beetle_StateAiming(Player* p, PlayState* play, ItemInputState* in) {
    // Animation update is handled by Player_UpperAction_Beetle

    if (beetleFirstPerson) {
        FirstPerson_Update(p, play);
    }

    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_CUP)) {
        if (beetleFirstPerson) {
            FirstPerson_Exit(p, play);
            beetleFirstPerson = 0;
        } else {
            FirstPerson_Init(p, play);
            beetleFirstPerson = 1;
        }
        ItemEquip_PlayEquipSFX(play, p);
        return;
    }

    u8 isZTargeting = Player_IsZTargeting(p);
    if (beetleFirstPerson && isZTargeting) {
        FirstPerson_Exit(p, play);
        beetleFirstPerson = 0;
    } else if (!beetleFirstPerson && !isZTargeting) {
        FirstPerson_Init(p, play);
        beetleFirstPerson = 1;
    }

    if (!in->isHeld && !in->isPressed) {
        Beetle_Launch(p, play);
    }
}

// Only actors the beetle can actually DAMAGE (attention-enabled enemies/bosses) or COLLECT/interact
// with (rupees/items, En_Si, silver-rupee switch) are valid targets — no random props. Skijer's NEI
static u8 Beetle_IsTargetable(Actor* a) {
    if ((a->category == ACTORCAT_ENEMY) || (a->category == ACTORCAT_BOSS)) {
        return (a->flags & ACTOR_FLAG_ATTENTION_ENABLED) ? 1 : 0; // already-targetable enemies
    }
    // Collectibles the beetle grabs (rupees, stray-fairy token, silver rupee).
    if (a->id == ACTOR_EN_ITEM00 || a->id == ACTOR_EN_SI ||
        (a->id == ACTOR_EN_G_SWITCH && (a->params >> 0xC & 0xF) == ENGSWITCH_SILVER_RUPEE)) {
        return 1;
    }
    // Switches a NORMAL beetle hit can trip: crystal / eye switches (Obj_Switch params&7 = 2 EYE /
    // 3 CRYSTAL / 4 CRYSTAL_TARGETABLE — NOT 0/1 floor/weight switches), and the Jabu heavy switch.
    if (a->id == ACTOR_OBJ_SWITCH) {
        u8 t = a->params & 7;
        return (t == 2 || t == 3 || t == 4) ? 1 : 0;
    }
    if (a->id == ACTOR_BG_BDAN_SWITCH) {
        return 1;
    }
    return 0;
}

// Nearest targetable actor within range AND roughly in front of the beetle — the Z-target candidate.
static Actor* Beetle_FindTarget(PlayState* play) {
    static const u8 sCats[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS,   ACTORCAT_ITEMACTION, ACTORCAT_PROP,
                                ACTORCAT_MISC,  ACTORCAT_SWITCH, ACTORCAT_BG };
    Actor* best = NULL;
    f32 bestDist = BEETLE_TARGET_RANGE;
    s32 c;

    for (c = 0; c < ARRAY_COUNT(sCats); c++) {
        Actor* actor = play->actorCtx.actorLists[sCats[c]].head;
        while (actor != NULL) {
            if ((actor->update != NULL) && Beetle_IsTargetable(actor)) {
                // Nearest targetable in range (no front-cone filter — it was excluding everything if
                // the yaw convention didn't line up; nearest-in-range is simpler and reliable). NEI
                f32 dist = Math_Vec3f_DistXYZ(&beetlePos, &actor->world.pos);
                if (dist < bestDist) {
                    bestDist = dist;
                    best = actor;
                }
            }
            actor = actor->next;
        }
    }
    return best;
}

// Turn beetleRot toward the locked target by up to `step` binang/frame (homing). Skijer's NEI
static void Beetle_HomeToTarget(s16 step) {
    Vec3f tgt = sBeetleTarget->world.pos;
    s16 wantYaw = Math_Vec3f_Yaw(&beetlePos, &tgt);
    s16 wantPitch = Math_Vec3f_Pitch(&beetlePos, &tgt);
    Math_SmoothStepToS(&beetleRot.y, wantYaw, 4, step, 0x10);
    Math_SmoothStepToS(&beetleRot.x, wantPitch, 4, step, 0x10);
}

static void Beetle_StateFlying(Player* p, PlayState* play) {
    Input* input = &play->state.input[0];
    u8 aHeld;

    // Drop a lock whose enemy died/despawned.
    if ((sBeetleTarget != NULL) && (sBeetleTarget->update == NULL)) {
        sBeetleTarget = NULL;
        sBeetleKamikaze = 0;
    }

    // ── AUTONOMOUS mode (B was pressed) ───────────────────────────────────────────────────────────
    // Camera + control are back with Link (subcam gone, no player freeze). The beetle flies on its
    // OWN — no stick input. If it has a locked enemy it homes in to hit it, otherwise it heads home.
    // A hit / max distance / timeout returns it to Link. Skijer's NEI
    if (sBeetleAutonomous) {
        u8 hit = Beetle_CheckActorCollision(p, play);
        Beetle_ProximityGrab(play);

        if ((sBeetleTarget != NULL) && (sBeetleTarget->update != NULL)) {
            Beetle_HomeToTarget(BEETLE_KAMIKAZE_STEP);
        } else {
            Beetle_StartReturn(p, play); // nothing to chase → fly back to Link
            return;
        }

        Beetle_Move(BEETLE_SPEED * BEETLE_BOOST_MULT);
        Beetle_UpdateCollider(play, &beetlePos);
        Beetle_UpdateWingAnimation(&beetleWingScale, &beetleWingDir);
        Beetle_UpdateGrabbedActor(); // NOTE: no Beetle_UpdateSubCam — the camera is Link's now

        if (hit || Beetle_CheckGeometryCollision(play)) {
            Beetle_StartReturn(p, play);
            return;
        }
        if ((Math_Vec3f_DistXYZ(&beetlePos, &beetleStartPos) > BEETLE_MAX_DISTANCE) || (DECR(beetleTimer) == 0)) {
            Beetle_StartReturn(p, play);
            return;
        }
        Beetle_PlayLoopSound(&p->actor, BEETLE_SFX_FLY);
        return;
    }

    // ── PILOTED mode (beetle subcam, Link frozen, you steer) ──────────────────────────────────────
    aHeld = CHECK_BTN_ALL(input->cur.button, BTN_A);

    // Animation update is handled by Player_UpperAction_Beetle
    Player_ZeroSpeedXZ(p);
    p->stateFlags1 |= PLAYER_STATE1_BOOMERANG_THROWN;
    p->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    // Point boomerangActor at Link himself so the FOLLOWBOOMERANG camera path
    // (z_player.c:12350) never propagates a stale En_Boom pointer through
    // Camera_SetParam — Link's actor is always valid.
    p->boomerangActor = &p->actor;

    // Z: toggle the enemy lock (locks the nearest attention-enabled enemy, like Link's Z-target).
    if (CHECK_BTN_ALL(input->press.button, BTN_Z)) {
        if (sBeetleTarget != NULL) {
            sBeetleTarget = NULL; // Z again = untarget
            sBeetleKamikaze = 0;
        } else {
            sBeetleTarget = Beetle_FindTarget(play);
        }
    }

    // B: hand the camera + control back to Link and let the beetle fly on its OWN. If a target is
    // locked it kamikaze-homes to hit it (then returns); otherwise it flies straight back to Link.
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        sBeetleAutonomous = 1;
        Beetle_DestroySubCam(play);                        // camera back to Link
        p->stateFlags1 &= ~PLAYER_STATE1_BOOMERANG_THROWN; // control back to Link
        p->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
        p->focusActor = NULL; // drop the lock reticle
        if (sBeetleTarget != NULL) {
            sBeetleKamikaze = 1;
        }
        return; // the autonomous branch takes over next frame
    }

    // Track the nearest lockable actor each frame so the reticle can show as an "offer" even before
    // you press Z (drawn via Beetle_DrawOffer). Skijer's NEI
    sBeetleCandidate = Beetle_FindTarget(play);

    u8 hitActor = Beetle_CheckActorCollision(p, play);
    Beetle_ProximityGrab(play);

    if (sBeetleKamikaze && (sBeetleTarget != NULL)) {
        // Locked-in: home hard at the target, ignore the stick.
        Beetle_HomeToTarget(BEETLE_KAMIKAZE_STEP);
    } else {
        // Normal stick steering; while locked, a gentle homing pull keeps the enemy in your path but
        // you stay in control. Holding A while locked redirects harder toward it (the "Z+A" redirect).
        Projectile_UpdateRotationFromStick(&beetleRot.y, &beetleRot.x, play, BEETLE_TURN_SPEED, BEETLE_PITCH_MAX);
        if (sBeetleTarget != NULL) {
            Beetle_HomeToTarget(aHeld ? BEETLE_TARGET_HOMING_STEP_A : BEETLE_TARGET_HOMING_STEP);
        }
    }

    // While flying + locked, point OoT's lock-on reticle (targetCtx.targetedActor) at our enemy by
    // driving player->focusActor. SoH runs CustomItems_Update AFTER Player_UpdateCommon, so setting it
    // here survives into the next Target_Update (no late hook needed, unlike MM). Skijer's NEI
    if ((sBeetleTarget != NULL) && (sBeetleTarget->update != NULL)) {
        p->focusActor = sBeetleTarget;
        p->zTargetActiveTimer = 15;
    }

    // A = fly faster; kamikaze always boosts to close the gap.
    Beetle_Move((aHeld || sBeetleKamikaze) ? (BEETLE_SPEED * BEETLE_BOOST_MULT) : BEETLE_SPEED);
    Beetle_UpdateCollider(play, &beetlePos);
    Beetle_UpdateWingAnimation(&beetleWingScale, &beetleWingDir);
    Beetle_UpdateSubCam(play);
    Beetle_UpdateGrabbedActor();

    if (hitActor || Beetle_CheckGeometryCollision(play)) {
        Beetle_StartReturn(p, play);
        return;
    }

    f32 distFromStart = Math_Vec3f_DistXYZ(&beetlePos, &beetleStartPos);
    if (distFromStart > BEETLE_MAX_DISTANCE || DECR(beetleTimer) == 0) {
        Beetle_StartReturn(p, play);
        return;
    }

    Beetle_PlayLoopSound(&p->actor, BEETLE_SFX_FLY);
}

// Runs in the DRAW phase (from CustomItems_OverrideDraw, which draws before the HUD targeting draw).
// Points OoT's automatic "offer" arrow (targetCtx.arrowPointedActor) at the nearest candidate while
// flying and NOT locked — so you see which actor Z would grab, without an auto-lock. Suppressed while
// autonomous. Skijer's NEI
void Beetle_DrawOffer(Player* p, PlayState* play) {
    // Draw the game's NATIVE offer arrow over the beetle's candidate while flying and NOT locked. The
    // arrow (gZTargetArrowDL, the spinning yellow arrow) is drawn by Attention_Draw over targetCtx->unk_94
    // — NOT over arrowPointedActor (that field only drives Navi/En_Elf, which is hidden during flight).
    // unk_94 is normally filled by Attention_FindActor searching near LINK, which finds nothing while the
    // beetle is off flying, so it stays NULL → no arrow. We set it here in the DRAW phase (this runs
    // before Attention_Draw, same as MM's arrowHoverActor override). It's the same world-space arrow the
    // game uses, correctly sized. Skijer's NEI
    if (beetleActive && (beetleState == BEETLE_STATE_FLYING) && !sBeetleAutonomous && (sBeetleTarget == NULL) &&
        (sBeetleCandidate != NULL) && (sBeetleCandidate->update != NULL)) {
        play->actorCtx.targetCtx.unk_94 = sBeetleCandidate;
    }
}

// DISABLED. This drew a custom world-space gGlowCircleDL ring, which (a) rendered as an untextured
// dark quad because that DL needs a combiner/texture setup Gfx_SetupDL_25Xlu doesn't provide, and (b)
// ballooned when close to the camera. We now use the game's NATIVE screen-space indicators exactly like
// MM: the lock reticle over player->focusActor (set inline in Beetle_StateFlying) and the offer arrow
// over targetCtx.arrowPointedActor (Beetle_DrawOffer). Kept as a no-op so callers stay valid. NEI
void Beetle_DrawTargetVfx(Player* p, PlayState* play) {
    (void)p;
    (void)play;
}

static void Beetle_StateReturning(Player* p, PlayState* play) {
    Vec3f targetPos = p->actor.world.pos;
    targetPos.y += BEETLE_LAUNCH_OFFSET_Y;

    f32 distToLink = Math_Vec3f_DistXYZ(&beetlePos, &targetPos);

    Beetle_UpdateWingAnimation(&beetleWingScale, &beetleWingDir);
    Beetle_UpdateGrabbedActor();

    if (distToLink > BEETLE_CATCH_DISTANCE) {
        f32 dx = targetPos.x - beetlePos.x;
        f32 dy = targetPos.y - beetlePos.y;
        f32 dz = targetPos.z - beetlePos.z;

        if (distToLink > 0.1f) {
            f32 invNorm = BEETLE_RETURN_SPEED / distToLink;
            beetlePos.x += dx * invNorm;
            beetlePos.y += dy * invNorm;
            beetlePos.z += dz * invNorm;
        }

        beetleRot.y = Math_Vec3f_Yaw(&beetlePos, &targetPos);
        beetleRot.x = Math_Vec3f_Pitch(&beetlePos, &targetPos);
        Beetle_PlayLoopSound(&p->actor, BEETLE_SFX_FLY);
    } else {
        Beetle_Catch(p, play);
    }
}

void Handle_Beetle(Player* p, PlayState* play) {
    if (!sBeetleColInitialized)
        Beetle_InitCollider(play, p);

    ItemInputState in;
    ItemInput_Update(&in, ITEM_BEETLE, p, play);

    if (!in.wasEquipped) {
        if (beetleActive)
            Beetle_Stop(p, play);
        return;
    }

    if (beetleState != BEETLE_STATE_FLYING && beetleState != BEETLE_STATE_RETURNING) {
        if (ItemInput_IsBlocked(p, play)) {
            if (beetleActive)
                Beetle_Stop(p, play);
            return;
        }
    }

    if (ItemInput_CheckDamage(p, &sBeetlePrevInvinc)) {
        if (beetleState == BEETLE_STATE_FLYING) {
            Beetle_StartReturn(p, play);
        } else if (beetleActive) {
            Beetle_Stop(p, play);
        }
        return;
    }

    if (!beetleActive) {
        if (in.isPressed)
            Beetle_Start(p, play);
        return;
    }

    if (beetleState == BEETLE_STATE_AIMING && CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
        Beetle_Stop(p, play);
        return;
    }

    switch (beetleState) {
        case BEETLE_STATE_AIMING:
            Beetle_StateAiming(p, play, &in);
            break;
        case BEETLE_STATE_FLYING:
            Beetle_StateFlying(p, play);
            break;
        case BEETLE_STATE_RETURNING:
            Beetle_StateReturning(p, play);
            break;
        default:
            beetleState = BEETLE_STATE_IDLE;
            beetleActive = 0;
            break;
    }
}

s32 Player_UpperAction_Beetle(Player* this, PlayState* play) {
    // Return busy if beetle is active - this makes the upper body use upperSkelAnime
    if (beetleActive) {
        LinkAnimation_Update(play, &this->upperSkelAnime);
        return 1;
    }
    return 0;
}

void Player_InitBeetleIA(PlayState* play, Player* this) {
    Beetle_InitCollider(play, this);
    beetleActive = 0;
    beetleState = BEETLE_STATE_IDLE;
    beetleFirstPerson = 0;
    beetleGrabbed = NULL;
    beetleWingScale = BEETLE_WING_SCALE_MAX;
    beetleWingDir = -1;
    beetleTimer = 0;
    beetleSubCamId = SUBCAM_FREE;
    this->stateFlags1 |= PLAYER_STATE1_ITEM_IN_HAND;
}
