/**
 * equip_champion.c - Champion's Tunic (Extended Tunic Slot 1)
 *
 * Features:
 *  1. Flurry Rush: Z-targeting + sidehop/backflip on the frame an incoming
 *     attack sweeps past Link → he blinks to the far side of the locked-on
 *     enemy, facing it, with iframes and the world in slow motion.
 *  2. Bullet Time: aim any aimable item while airborne → the world slows and
 *     Link hangs in the air. Aiming itself is the game's own first-person aim;
 *     this module does not touch it.
 *
 * Slow-motion goes through timestop_helper (TIMECTL_OWNER_CHAMPION), which owns
 * gChampionSlowFactor for everyone. Champion holds the LOWEST priority claim: a
 * hard time stop always wins over bullet time.
 *
 * Screen tint via play->envCtx.fillScreen + screenFillColor[].
 * Champion_Cleanup() takes PlayState* so it can clear the tint on unequip.
 *
 * Included by ext_equip_behavior.c (unity build).
 */

#include "../../items/helpers/timestop_helper.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define CHAMPION_FLURRY_DURATION 100 // real frames the committed rush lasts

// BOTW splits the move in two: the perfect dodge only slows time, and the rush itself
// starts when you swing. This is how long that offer stays open before time resumes.
#define CHAMPION_FLURRY_OFFER_FRAMES 45

// BOTW cancels the rush if Link stops connecting. Every landed hit refreshes this.
#define CHAMPION_FLURRY_CONNECT_FRAMES 45

// Hits before the rush ends. BOTW: 7 with a one-handed weapon, 4 with a two-hander.
#define CHAMPION_FLURRY_HIT_MAX 7
#define CHAMPION_FLURRY_HIT_MAX_TWOHAND 4

// World speed during both modes. This is SLOW MOTION, not a stop: the world
// still visibly moves, you just get time to read it. See the note in z_actor.c —
// the engine expresses a partial slowdown by letting actors tick 1 frame in N
// (N = 1/factor), and it must NOT also scale their motion on the frames they do
// run, or the two multiply and 0.33 turns into a dead stop.
#define CHAMPION_SLOW_FACTOR 0.33f

#define CHAMPION_SCREEN_FLASH 5     // initial bright-tint burst frames
#define CHAMPION_BULLET_FLOAT 1.15f // velocity.y counterforce each frame (net fall ~= -0.05/frame)
#define CHAMPION_TINT_ALPHA 30      // subtle blue tint (BOTW has no heavy overlay)

// Flurry Rush trigger + blink.
#define CHAMPION_DODGE_RANGE 140.0f     // how close an incoming attack must sweep
#define CHAMPION_DODGE_COOLDOWN 20      // frames before the same hop may re-trigger
#define CHAMPION_TELEPORT_DIST 65.0f    // where Link lands relative to the enemy
#define CHAMPION_MAX_TELEPORT 600.0f    // never blink across the room to a far target
#define CHAMPION_ATTACK_SNAPSHOT_MAX 24 // incoming attacks tracked per frame

#ifndef BGCHECKFLAG_GROUND
#define BGCHECKFLAG_GROUND 0x0001
#endif

// Forward declarations (defined later in z_player.c unity build).
// No Player_SetIntangibility here on purpose: BOTW leaves Link vulnerable for the whole
// flurry, and a hit cancels it. See the damage check in Champion_Behavior.
extern int Player_IsZTargeting(Player* this);

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------
typedef enum {
    CHAMPION_IDLE,
    CHAMPION_FLURRY_OFFER, // dodge landed: world slowed, waiting for Link to swing
    CHAMPION_FLURRY_RUSH,  // committed: blinked in, the victim cannot go invulnerable
    CHAMPION_BULLET_TIME,
} ChampionState;

// ---------------------------------------------------------------------------
// Module-level statics
// ---------------------------------------------------------------------------
static ChampionState sChampionState = CHAMPION_IDLE;
static s16 sChampionTimer = 0;
static u8 sChampionHitCount = 0;
static s16 sScreenFlashTimer = 0;
// The enemy this flurry is aimed at. Its invulnerability is stripped every frame of the
// window so consecutive swings all land; nothing else in the room is affected.
static Actor* sChampionFlurryTarget = NULL;
static s16 sChampionConnectTimer = 0; // frames left to land the next hit before it fizzles
static s8 sChampionPrevInvinc = 0;    // damage edge detection — a hit cancels the rush
// Blocks a re-trigger while still inside the hop that just produced a flurry. Without it
// a single sidehop next to a sustained hitbox would re-arm the moment the last one ended.
static s16 sChampionDodgeCooldown = 0;

// ---------------------------------------------------------------------------
// Incoming-attack snapshot
//
// Flurry Rush has to know that a damage collider is sweeping past Link RIGHT NOW.
// The obvious way — walk play->colChkCtx.colAT from the behavior — does not work:
// CollisionCheck_ClearContext runs BEFORE Actor_UpdateAll every frame, so by the
// time Link updates the AT list only holds whatever the first couple of actor
// categories have re-registered. The list is complete exactly once per frame, at
// CollisionCheck_AT, which runs before the wipe. So we snapshot POSITIONS there
// (never pointers, so nothing can go stale) and the behavior reads the snapshot.
// ---------------------------------------------------------------------------
static Vec3f sChampionAttackPos[CHAMPION_ATTACK_SNAPSHOT_MAX];
static s32 sChampionAttackCount = 0;

/** Best-effort world position of a collider, whatever its shape. */
static s32 Champion_ColliderPos(Collider* col, Vec3f* out) {
    switch (col->shape) {
        case COLSHAPE_QUAD: {
            // Sword swings and most weapon arcs are quads. Their centre is a far
            // better "where is the blade" answer than the wielder's own position.
            ColliderQuad* quad = (ColliderQuad*)col;

            out->x = (quad->dim.quad[0].x + quad->dim.quad[1].x + quad->dim.quad[2].x + quad->dim.quad[3].x) * 0.25f;
            out->y = (quad->dim.quad[0].y + quad->dim.quad[1].y + quad->dim.quad[2].y + quad->dim.quad[3].y) * 0.25f;
            out->z = (quad->dim.quad[0].z + quad->dim.quad[1].z + quad->dim.quad[2].z + quad->dim.quad[3].z) * 0.25f;
            return 1;
        }
        case COLSHAPE_CYLINDER: {
            ColliderCylinder* cyl = (ColliderCylinder*)col;

            out->x = (f32)cyl->dim.pos.x;
            out->y = (f32)cyl->dim.pos.y;
            out->z = (f32)cyl->dim.pos.z;
            return 1;
        }
        default:
            // JNTSPH / TRIS: their element geometry is per-element, so fall back to
            // the owning actor. Good enough — those are mostly bodies and projectiles,
            // where the actor IS roughly where the danger is.
            if (col->actor != NULL) {
                *out = col->actor->world.pos;
                return 1;
            }
            return 0;
    }
}

/**
 * Called from CollisionCheck_AT, the one point in the frame where the AT list is
 * complete. Records where every hostile attack collider is, so Flurry Rush can
 * ask "is something swinging at me" later in the same frame.
 */
void Champion_NoteIncomingAttacks(PlayState* play) {
    Player* player;
    s32 i;

    sChampionAttackCount = 0;
    if (play == NULL) {
        return;
    }
    player = GET_PLAYER(play);
    if (player == NULL) {
        return;
    }

    for (i = 0; (i < play->colChkCtx.colATCount) && (sChampionAttackCount < CHAMPION_ATTACK_SNAPSHOT_MAX); i++) {
        Collider* col = play->colChkCtx.colAT[i];

        if ((col == NULL) || !(col->atFlags & AT_ON)) {
            continue;
        }
        // Link's own sword, and anything he spawned, are not incoming attacks.
        if (col->actor == &player->actor) {
            continue;
        }
        if ((col->actor != NULL) && (col->actor->parent == &player->actor)) {
            continue;
        }
        if (Champion_ColliderPos(col, &sChampionAttackPos[sChampionAttackCount])) {
            sChampionAttackCount++;
        }
    }
}

/** Is one of this frame's hostile attacks sweeping within dodge range of Link? */
static u8 Champion_IncomingAttackNearby(Player* player) {
    s32 i;

    for (i = 0; i < sChampionAttackCount; i++) {
        if (Math_Vec3f_DistXYZ(&sChampionAttackPos[i], &player->actor.world.pos) <= CHAMPION_DODGE_RANGE) {
            return 1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Returns 1 if Link is holding any first-person aimable item:
 * bow variants, slingshot, hookshot/longshot, boomerang.
 */
static u8 Champion_IsAimableAction(PlayerItemAction ia) {
    if (ia >= PLAYER_IA_BOW && ia <= PLAYER_IA_LONGSHOT)
        return 1; // bow..sling..hookshot..longshot
    if (ia == PLAYER_IA_BOOMERANG)
        return 1;
    return 0;
}

static u8 Champion_IsAimableItem(Player* player) {
    // heldItemAction can briefly lag itemAction while the airborne upper-body
    // transition hands control to the first-person action. Accept either side
    // of that transition so the permission cannot disappear for one frame.
    return Champion_IsAimableAction(player->heldItemAction) || Champion_IsAimableAction(player->itemAction);
}

/** Is Link actually aiming the thing, as opposed to merely holding it? */
static u8 Champion_IsAiming(Player* player) {
    return (player->stateFlags1 & (PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_READY_TO_FIRE)) != 0;
}

// ---------------------------------------------------------------------------
// Mid-air aim permission — read by z_player.c
//
// Vanilla flatly refuses to let Link raise an aimable item off the ground:
// Player_ActionHandler_13 (the C-button item-use handler) gates on
// bgCheckFlags & BGCHECKFLAG_GROUND, and on top of that the airborne action
// function never runs an action-handler list at all. Both have to give way for
// Bullet Time to be reachable, because entering it REQUIRES the aim state that
// vanilla is refusing — without this the trigger is circular and never fires.
//
// This is the single switch both z_player relaxations consult, so the exception
// is exactly "wearing the Champion's Tunic, holding something aimable" and
// nothing wider.
// ---------------------------------------------------------------------------
u8 Champion_AllowsMidairAim(Player* player) {
    if ((player == NULL) || !ExtEquip_IsChampionTunic()) {
        return 0;
    }
    return (sChampionState == CHAMPION_BULLET_TIME) || Champion_IsAimableItem(player);
}

/** The locked-on actor, but only when it is something worth flurrying around. */
static Actor* Champion_LockedEnemy(Player* player) {
    Actor* target = player->focusActor;

    if ((target == NULL) || (target->update == NULL)) {
        return NULL;
    }
    if ((target->category != ACTORCAT_ENEMY) && (target->category != ACTORCAT_BOSS)) {
        return NULL;
    }
    return target;
}

/**
 * Set or clear the screen tint.
 * fillScreen must be toggled alongside screenFillColor for the engine to
 * render the overlay. fillScreen persists until explicitly cleared.
 *
 * golden=1 → warm gold (unused: both modes are blue now, so the tunic reads as one
 *            coherent effect instead of two unrelated ones)
 * golden=0 → cool blue (Bullet Time AND Flurry Rush)
 * alpha=0  → clear tint
 */
static void Champion_SetScreenTint(PlayState* play, u8 golden, u8 alpha) {
    if (alpha == 0) {
        play->envCtx.fillScreen = false;
        play->envCtx.screenFillColor[3] = 0;
        return;
    }
    play->envCtx.fillScreen = true;
    if (golden) {
        play->envCtx.screenFillColor[0] = 220;
        play->envCtx.screenFillColor[1] = 180;
        play->envCtx.screenFillColor[2] = 40;
    } else {
        play->envCtx.screenFillColor[0] = 6;
        play->envCtx.screenFillColor[1] = 24;
        play->envCtx.screenFillColor[2] = 66;
    }
    play->envCtx.screenFillColor[3] = alpha;
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

/**
 * The BOTW blink: close the distance to the enemy Link is locked onto in a single frame
 * and turn him to face it, so the dodge ends with him already in striking range.
 *
 * prevPos is written alongside world.pos and bgCheckFlags cleared so the engine
 * does not treat the jump as a collision sweep and drag him back — the same trick
 * the Switch Hook's swap uses. Y comes from the ENEMY, not from Link, so blinking
 * past a flying or elevated target does not leave him standing in the air.
 */
static void Champion_BlinkToTarget(Player* player, Actor* target) {
    f32 dx = player->actor.world.pos.x - target->world.pos.x;
    f32 dz = player->actor.world.pos.z - target->world.pos.z;
    f32 distXZ = sqrtf((dx * dx) + (dz * dz));
    Vec3f dest;

    if (distXZ > CHAMPION_MAX_TELEPORT) {
        return; // too far to be a dodge — leave him where he is, just slow the world
    }
    if (distXZ < 1.0f) {
        // Standing on top of it: fall back to pushing him out along its facing.
        dx = Math_SinS(target->shape.rot.y);
        dz = Math_CosS(target->shape.rot.y);
        distXZ = 1.0f;
    }

    // Close the gap: drop Link at striking distance on the side he already was, rather
    // than mirroring him past the enemy. (dx,dz) points FROM the enemy TO Link, so adding
    // it keeps him on his own side; subtracting would put him behind.
    dest.x = target->world.pos.x + (dx / distXZ) * CHAMPION_TELEPORT_DIST;
    dest.z = target->world.pos.z + (dz / distXZ) * CHAMPION_TELEPORT_DIST;
    dest.y = target->world.pos.y;

    player->actor.world.pos = dest;
    player->actor.prevPos = dest;
    player->actor.bgCheckFlags = 0;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->linearVelocity = 0.0f;
    player->actor.speedXZ = 0.0f;

    // Face the target from the new spot.
    player->actor.shape.rot.y = Math_Vec3f_Yaw(&player->actor.world.pos, &target->world.pos);
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
}

/**
 * The perfect dodge itself. Time slows and the offer opens — but Link does not move and
 * the enemy is not touched yet. In BOTW the dodge only buys you the slow-motion; the rush
 * is a separate commitment you make by swinging, and you can decline it and just reposition.
 *
 * Note Link gets NO intangibility here. BOTW leaves him vulnerable throughout: another
 * enemy landing a hit cancels the whole thing, which is what stops the move from being
 * free value in a crowd.
 */
static void Champion_EnterFlurryOffer(Player* player, PlayState* play, Actor* target) {
    sChampionState = CHAMPION_FLURRY_OFFER;
    sChampionTimer = CHAMPION_FLURRY_OFFER_FRAMES;
    sChampionHitCount = 0;
    sChampionFlurryTarget = target;

    TimeCtl_Request(TIMECTL_OWNER_CHAMPION, CHAMPION_SLOW_FACTOR, 0);

    sScreenFlashTimer = CHAMPION_SCREEN_FLASH;
    Champion_SetScreenTint(play, 0, 200);

    Audio_PlaySoundGeneral(NA_SE_SY_ATTENTION_ON, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

/** Link swung during the offer: close the distance and open the victim up. */
static void Champion_CommitFlurry(Player* player, PlayState* play) {
    sChampionState = CHAMPION_FLURRY_RUSH;
    sChampionTimer = CHAMPION_FLURRY_DURATION;
    sChampionConnectTimer = CHAMPION_FLURRY_CONNECT_FRAMES;
    sChampionHitCount = 0;

    if (sChampionFlurryTarget != NULL) {
        Champion_BlinkToTarget(player, sChampionFlurryTarget);
    }

    Champion_SetScreenTint(play, 0, 120);
    Audio_PlaySoundGeneral(NA_SE_SY_ATTENTION_ON, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

/** 7 swings with a one-handed weapon, 4 with a two-hander, as in BOTW. */
static u8 Champion_FlurryHitLimit(Player* player) {
    return Player_HoldsTwoHandedWeapon(player) ? CHAMPION_FLURRY_HIT_MAX_TWOHAND : CHAMPION_FLURRY_HIT_MAX;
}

static void Champion_ExitFlurry(PlayState* play) {
    sChampionState = CHAMPION_IDLE;
    sChampionTimer = 0;
    sChampionHitCount = 0;
    sChampionConnectTimer = 0;
    sChampionFlurryTarget = NULL;
    sChampionDodgeCooldown = CHAMPION_DODGE_COOLDOWN;
    TimeCtl_Release(TIMECTL_OWNER_CHAMPION);
    Champion_SetScreenTint(play, 0, 0);
}

static void Champion_EnterBulletTime(Player* player, PlayState* play) {
    sChampionState = CHAMPION_BULLET_TIME;
    TimeCtl_Request(TIMECTL_OWNER_CHAMPION, CHAMPION_SLOW_FACTOR, 0);
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    Champion_SetScreenTint(play, 0, CHAMPION_TINT_ALPHA);
}

static void Champion_ExitBulletTime(Player* player, PlayState* play) {
    (void)player;
    sChampionState = CHAMPION_IDLE;
    TimeCtl_Release(TIMECTL_OWNER_CHAMPION);
    Champion_SetScreenTint(play, 0, 0);
}

// ---------------------------------------------------------------------------
// Melee hit callback — called from ExtEquip_OnMeleeHitDispatch
// ---------------------------------------------------------------------------
static void Champion_OnMeleeHit(Player* player, PlayState* play) {
    if (sChampionState != CHAMPION_FLURRY_RUSH) {
        return;
    }
    sChampionConnectTimer = CHAMPION_FLURRY_CONNECT_FRAMES; // connecting keeps it alive
    sChampionHitCount++;
    if (sChampionHitCount >= Champion_FlurryHitLimit(player)) {
        Champion_ExitFlurry(play);
    }
}

// ---------------------------------------------------------------------------
// Per-frame behavior
// ---------------------------------------------------------------------------
static void Champion_Behavior(Player* player, PlayState* play) {
    // Skijer 2026-07-16: the BOTW Link skin force is REMOVED — Champion's Tunic is now a plain recolor
    // tunic (blue, painted in Player_DrawImpl). Only the flurry-rush + bullet-time mechanics remain.

    // ---- Guard: clean exit during cutscenes / death / loading --------------
    u32 blockedFlags = PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                       PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM;
    if (player->stateFlags1 & blockedFlags) {
        if ((sChampionState == CHAMPION_FLURRY_RUSH) || (sChampionState == CHAMPION_FLURRY_OFFER)) {
            Champion_ExitFlurry(play);
        } else if (sChampionState == CHAMPION_BULLET_TIME) {
            Champion_ExitBulletTime(player, play);
        }
        return;
    }

    // ---- Screen flash fade -------------------------------------------------
    if (sScreenFlashTimer > 0) {
        sScreenFlashTimer--;
        if (sScreenFlashTimer == 0 &&
            ((sChampionState == CHAMPION_FLURRY_RUSH) || (sChampionState == CHAMPION_FLURRY_OFFER))) {
            Champion_SetScreenTint(play, 0, 50); // settle to a dim persistent blue
        }
    }

    // ---- Per-frame reads ---------------------------------------------------
    u8 curHopping = (player->stateFlags2 & PLAYER_STATE2_HOPPING) != 0;
    u8 onGround = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
    // Same damage edge the custom items use (see ItemInput_CheckDamage). Read once per
    // frame so both flurry states see the identical value.
    u8 tookDamage = (player->invincibilityTimer > 0) && (sChampionPrevInvinc == 0);

    sChampionPrevInvinc = player->invincibilityTimer;

    if (sChampionDodgeCooldown > 0) {
        sChampionDodgeCooldown--;
    }

    // ---- State machine -----------------------------------------------------
    switch (sChampionState) {

        case CHAMPION_IDLE: {
            // Bullet Time: aim an aimable item while airborne. No Z-targeting
            // required — being in the air with the thing raised IS the gesture.
            if (!onGround && Champion_IsAimableItem(player) && Champion_IsAiming(player)) {
                Champion_EnterBulletTime(player, play);
                break;
            }
            // Flurry Rush: the first frame of a sidehop/backflip, while locked on,
            // with a damage collider sweeping past. That is the BOTW perfect dodge.
            // Any frame of the hop counts, not just its first. Requiring the rising edge
            // meant the incoming attack had to be inside CHAMPION_DODGE_RANGE on exactly
            // the frame the hop started — a one-frame coincidence that mostly did not
            // happen, since the blade is usually still travelling toward Link then. Being
            // airborne in a sidehop or backflip IS the dodge; the distance test to the
            // sweeping damage collider is what decides whether it was a good one.
            if (curHopping && (sChampionDodgeCooldown == 0) && Player_IsZTargeting(player) &&
                Champion_IncomingAttackNearby(player)) {
                Champion_EnterFlurryOffer(player, play, Champion_LockedEnemy(player));
            }
            break;
        }

        case CHAMPION_FLURRY_OFFER: {
            // Time is slowed and the rush is on offer. Swing to take it, or let it lapse
            // and just use the slow motion to reposition — both are valid in BOTW.
            if (tookDamage) {
                Champion_ExitFlurry(play);
                break;
            }
            if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) && (sChampionFlurryTarget != NULL) &&
                (sChampionFlurryTarget->update != NULL)) {
                Champion_CommitFlurry(player, play);
                break;
            }
            if (--sChampionTimer <= 0) {
                Champion_ExitFlurry(play);
            }
            break;
        }

        case CHAMPION_FLURRY_RUSH: {
            // Link is NOT invincible here — a hit from anything cancels the rush. That is
            // what keeps the move honest when more than one enemy is on you.
            if (tookDamage) {
                Champion_ExitFlurry(play);
                break;
            }

            // Hold the victim open. Enemies go invulnerable between hits, which would eat
            // every swing after the first; clearing it each frame is what turns the window
            // into a real combo. Only this one enemy — the rest of the room is untouched.
            if ((sChampionFlurryTarget != NULL) && (sChampionFlurryTarget->update != NULL)) {
                TimeCtl_ClearIframes(sChampionFlurryTarget);
            } else {
                sChampionFlurryTarget = NULL; // it died or despawned mid-flurry
                Champion_ExitFlurry(play);
                break;
            }

            // Stop connecting and it fizzles out, rather than handing you a free slow-mo
            // window to stroll around in. Every landed hit refreshes this.
            if (--sChampionConnectTimer <= 0) {
                Champion_ExitFlurry(play);
                break;
            }

            if (--sChampionTimer <= 0) {
                Champion_ExitFlurry(play);
            }
            break;
        }

        case CHAMPION_BULLET_TIME: {
            // Exit on landing or the moment he stops aiming / puts the item away.
            if (onGround || !Champion_IsAimableItem(player) || !Champion_IsAiming(player)) {
                Champion_ExitBulletTime(player, play);
                break;
            }

            // Suspend the fall. That is the ONLY thing this state does to Link —
            // aiming is the game's own first-person aim, untouched. The old build
            // drove yaw/pitch off the analog stick and wrote shape.rot/focus.rot
            // every frame, which fought the real aim camera and felt wrong.
            player->actor.velocity.y = CHAMPION_BULLET_FLOAT;

            Champion_SetScreenTint(play, 0, CHAMPION_TINT_ALPHA);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Cleanup — called from ExtEquip_DispatchBehavior with PlayState* when the
// tunic slot is no longer 1. Takes PlayState* unlike other cleanups so that
// the screen tint (fillScreen) can be properly cleared immediately.
// ---------------------------------------------------------------------------
static void Champion_Cleanup(PlayState* play) {
    // (BOTW skin force removed 2026-07-16 — nothing to clear model-side.)
    TimeCtl_Release(TIMECTL_OWNER_CHAMPION);

    if (play != NULL) {
        Champion_SetScreenTint(play, 0, 0);
    }

    sChampionState = CHAMPION_IDLE;
    sChampionTimer = 0;
    sChampionHitCount = 0;
    sChampionConnectTimer = 0;
    sChampionPrevInvinc = 0;
    sChampionDodgeCooldown = 0;
    sChampionFlurryTarget = NULL;
    sScreenFlashTimer = 0;
}
