/**
 * cane_pacci.c — Pacci side of the Dual Cane. See cane_pacci.h. Skijer's NEI
 *
 * Flip and Stone work on ANY ACTORCAT_ENEMY actor rather than a curated list, so
 * the mechanic behaves the same on enemies nobody thought to special-case. They
 * do it by taking the actor over: its `update` (and, for Stone, its `draw`) is
 * swapped for ours, which is what "loses all its AI" means literally. Everything
 * we overwrite is saved in the pool entry and put back on release, so an enemy
 * that survives a Flip resumes its own behaviour exactly where it left off.
 */

#include "cane_pacci.h"
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "objects/gameplay_field_keep/gameplay_field_keep.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_dy_obj/object_dy_obj.h"
#include "../items/helpers/target_select_helper.h"
#include "../items/helpers/rewind_helper.h" // the Phantom Hourglass drives an actor the same way we do
#include <math.h>

extern PlayState* gPlayState;

// z_scene.c — request an object into the scene's bank at runtime. Not declared in
// functions.h, so it is forward-declared here.
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);

// ============================================================================
// POOL
// ============================================================================

typedef enum {
    PACCI_FX_NONE = 0,
    PACCI_FX_FLIP,
    PACCI_FX_STONE,
} PacciFxMode;

typedef enum {
    PACCI_FLIP_PHASE_AIRBORNE = 0,
    PACCI_FLIP_PHASE_DOWNED,
    PACCI_FLIP_PHASE_RIGHTING,
} PacciFlipPhase;

typedef enum {
    PACCI_STONE_PHASE_IDLE = 0,
    PACCI_STONE_PHASE_HELD,
    PACCI_STONE_PHASE_THROWN,
} PacciStonePhase;

typedef struct {
    Actor* actor;
    u8 mode; // PacciFxMode
    u8 phase;
    s16 timer;
    // Saved actor state, restored verbatim on release.
    ActorFunc origUpdate;
    ActorFunc origDraw;
    u32 origFlags;
    f32 origGravity;
    f32 origMinVelocityY;
    f32 origSpeed;
    // FULL rotation cache. The ObjTsubo tumble in the free fall writes shape.rot.x
    // and .y as well as .z, so restoring only .z would hand the enemy back to its
    // own AI sitting on a garbage orientation — its animations then play tilted or
    // inside-out for the rest of its life. Both rotations are snapshotted before
    // the flip and forced back verbatim on recovery.
    Vec3s origShapeRot;
    Vec3s origWorldRot;
    s16 origRoom;
    u8 origMass;
    f32 origYOffset; // shape.yOffset is moved during the flip, so it has to come back
    // ObjTsubo_Thrown's tumble, per-entry (the pot keeps these in file statics, which
    // would make every flipped enemy spin in lockstep).
    s16 tumbleX;
    s16 tumbleY;
    s16 tumbleTargetX;
    s16 tumbleTargetY;
    // AT collider used while a flipped enemy is falling, so it smashes pots and
    // cuts grass on the way down.
    ColliderCylinder collider;
    u8 colliderReady;
} PacciFx;

static PacciFx sPacciPool[PACCI_MAX_AFFECTED] = { { 0 } };

// Per-skill aim colours (user-locked): Flip RED, Stone YELLOW, Ultrahand BLUE.
// The tint says which of Pacci's three things the button is about to do, which
// matters once the cane grows toward a Sheikah-Stone-style multi-tool.
//
// LIMITATION: OoT's colour filter is not a colour — it is three hardcoded modes.
// z64actor.h documents colorFilterParams as: bit 0x8000 = white, bit 0x4000 = red,
// neither = blue. There is no yellow, and no way to ask for one without replacing
// the engine's filter draw. WHITE is used for Stone as the nearest reading: it is
// the pale "about to be petrified" wash, and it is unmistakably distinct from the
// other two at a glance. (MM has the same three modes plus GRAY.)
#define PACCI_TINT_FLIP(actor, dur) Actor_SetColorFilter((actor), 0x4000, 255, 0, (dur))
#define PACCI_TINT_STONE(actor, dur) Actor_SetColorFilter((actor), 0x8000, 255, 0, (dur))
#define PACCI_TINT_ULTRAHAND(actor, dur) Actor_SetColorFilter((actor), 0, 255, 0, (dur))
// There is deliberately NO Zonai tint macro. Every attempt to make one went through
// Actor_SetColorFilter, which offers exactly three modes - white, red, blue - and picking
// white produced the pale, petrified-looking wash the tint was supposed to avoid. Green on
// the object itself comes from a real point light instead (Pacci_UhLightAt), which tints
// the model's own texture rather than painting over it.
// Flash for a non-lethal hit landed on a helpless enemy.
#define PACCI_TINT_HURT(actor, dur) Actor_SetColorFilter((actor), 0x8000, 255, 0, (dur))

// Flip sound cues. SoH quotes the Tektite directly, since that is the actor whose
// behaviour this move is a port of.
#define PACCI_SFX_FLIP NA_SE_EN_TEKU_REVERSE
#define PACCI_SFX_FLIP_LAND NA_SE_EN_DODO_M_GND

// While flipped the enemy carries THIS collider instead of its own, and it does
// double duty:
//   AT (player-type) — the thrown-pot behaviour: it smashes pots and cuts grass on
//                      the way down, with the vanilla break particles.
//   AC (player-type) — what makes a flipped enemy hittable at all. The collider is
//                      owned by the target actor, so a hit here lands the damage in
//                      the ENEMY's colChkInfo; Pacci_FlipUpdate then hands the actor
//                      straight back to its own update, which processes that damage
//                      through its normal path (death, drops, animation, all of it).
static ColliderCylinderInit sPacciFallColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x08 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 30, 46, -12, { 0, 0, 0 } },
};

void Pacci_CleanupPool(void); // used by Pacci_TakeEntry below, defined further down

static PacciFx* Pacci_FindEntry(Actor* actor) {
    for (u8 i = 0; i < PACCI_MAX_AFFECTED; i++) {
        if (sPacciPool[i].actor == actor) {
            return &sPacciPool[i];
        }
    }
    return NULL;
}

static PacciFx* Pacci_TakeEntry(void) {
    // Reap first. A flipped enemy that gets killed mid-flip — the intended outcome
    // of the move — leaves an entry whose actor is gone, and since the effect now
    // outlives the cane there is no longer a per-frame cleanup guaranteed to be
    // running. Without this the pool would silently fill up with corpses.
    Pacci_CleanupPool();

    for (u8 i = 0; i < PACCI_MAX_AFFECTED; i++) {
        if (sPacciPool[i].actor == NULL) {
            return &sPacciPool[i];
        }
    }
    return NULL;
}

// Put the actor back exactly as we found it and free the slot.
static void Pacci_Restore(PacciFx* fx) {
    Actor* actor = fx->actor;

    if (actor != NULL && actor->update != NULL) {
        actor->update = fx->origUpdate;
        if (fx->origDraw != NULL) {
            actor->draw = fx->origDraw;
        }
        actor->flags = fx->origFlags;
        actor->gravity = fx->origGravity;
        actor->minVelocityY = fx->origMinVelocityY;
        actor->shape.rot = fx->origShapeRot;
        actor->world.rot = fx->origWorldRot;
        actor->room = (s8)fx->origRoom;
        actor->colChkInfo.mass = fx->origMass;
        actor->shape.yOffset = fx->origYOffset;
        actor->speedXZ = 0.0f;
        actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;
    }

    fx->actor = NULL;
    fx->mode = PACCI_FX_NONE;
    fx->phase = 0;
    fx->timer = 0;
}

void Pacci_CleanupPool(void) {
    // THE ASSEMBLY IS NOT TOUCHED HERE, and it must never be again.
    //
    // This used to open with an unconditional Pacci_FuseForget(). Pacci_CleanupPool runs EVERY
    // FRAME from the cane's handler, so sFuse was wiped one frame after every weld - while the
    // parts were still alive and still carrying Pacci_FusePartUpdate as their update.
    //
    // That leaves a zombie. The wrapper runs, walks a part list that no longer contains the
    // actor, matches nothing, and returns having done neither of its two jobs: it never calls
    // the actor's own update, so the actor stops submitting its collider and it disappears from
    // the world entirely; and it never calls Pacci_FusePlacePart, so the piece stops following
    // the root and hangs wherever it was. Both halves of "los attached no caen y pierden su
    // collider", from one line.
    //
    // The comment that was here justified the choice with "by the time this runs the actors are
    // gone" - true of scene teardown, which is what Pacci_FuseForget exists for, and not true of
    // a per-frame housekeeping pass. Pacci_FuseFollow already drops dead parts and releases a
    // structure whose root died, which is all the upkeep an assembly actually needs.
    for (u8 i = 0; i < PACCI_MAX_AFFECTED; i++) {
        if (sPacciPool[i].actor != NULL && sPacciPool[i].actor->update == NULL) {
            sPacciPool[i].actor = NULL;
            sPacciPool[i].mode = PACCI_FX_NONE;
        }
    }
}

// ============================================================================
// TARGETING
// ============================================================================

u8 Pacci_IsValidEnemy(Actor* actor) {
    if (actor == NULL || actor->update == NULL) {
        return 0;
    }
    if (actor->category != ACTORCAT_ENEMY) {
        return 0; // user-locked: enemies only
    }
    if (actor->id == ACTOR_PLAYER) {
        return 0;
    }
    // Bosses are excluded (user-locked). ACTORCAT_BOSS covers the real bosses;
    // minibosses live in ACTORCAT_ENEMY, so exclude the heavy ones too — an
    // IMMOVABLE mass is exactly what marks "this thing does not get knocked
    // around" and needs no per-actor id list to stay correct.
    if (actor->category == ACTORCAT_BOSS) {
        return 0;
    }
    if (actor->colChkInfo.mass == MASS_IMMOVABLE) {
        return 0;
    }
    switch (actor->id) {
        case ACTOR_EN_IK:     // Iron Knuckle
        case ACTOR_EN_ZF:     // Lizalfos / Dinolfos
        case ACTOR_EN_TORCH2: // Dark Link
            return 0;
        default:
            break;
    }
    if (Pacci_FindEntry(actor) != NULL) {
        return 0; // already flipped or petrified
    }
    return 1;
}

static s32 Pacci_EnemyFilter(Actor* actor) {
    return Pacci_IsValidEnemy(actor);
}

static const u8 sPacciEnemyCats[1] = { ACTORCAT_ENEMY };

static Actor* Pacci_ScanEnemy(PlayState* play) {
    return TargetSelect_ScanCats(play, sPacciEnemyCats, 1, Pacci_EnemyFilter, TARGETSEL_DEFAULT_RANGE,
                                 TARGETSEL_DEFAULT_CONE);
}

// Defined down in the LIFT section, but Flip shares it — both moves use one notion
// of "something I can grab", and Flip is written above that section.
static Actor* Pacci_ScanLiftable(PlayState* play);
// Defined down in the ULTRAHAND section, but the highlight above it shares the same
// resolver — that is the whole point, so the two can never mark and grab different
// things.
static Actor* Pacci_ResolveUltrahandTarget(PlayState* play, Player* player);
static void Pacci_BurstSpawn(PlayState* play, Player* player, Vec3f* pos, u8 heavy);

// Live aim feedback for Flip and Stone: every frame the cane is in hand with one
// of them selected, the enemy that would be hit is tinted in that skill's colour.
// Same idea as the Switch Hook's continuous selection (z_arms_hook.c calls
// TargetSelect_Highlight with a short duration and re-applies it each frame).
void Pacci_HighlightEnemyTarget(PlayState* play, u8 stone) {
    // Flip acts on anything liftable, so the highlight has to scan the SAME set.
    // It used to scan enemies only, which is why standing in front of a pot showed
    // no suggestion at all even after Flip itself had been widened to props.
    // Stone is still enemies-only — petrifying a crate means nothing.
    Actor* target = stone ? Pacci_ScanEnemy(play) : Pacci_ScanLiftable(play);

    if (target == NULL) {
        return;
    }
    // Short duration, because it is re-applied every frame; the moment the player
    // looks away it lapses on its own.
    if (stone) {
        PACCI_TINT_STONE(target, 4);
    } else {
        PACCI_TINT_FLIP(target, 4);
    }
}

// ============================================================================
// FLIP
// ============================================================================
//
// Ported from two vanilla behaviours, without touching either actor:
//
//   EnTite_SetupFlipOnBack / EnTite_FlipOnBack / EnTite_FlipUpright  — the flip.
//     A hammered Tektite launches straight up (velocity.y 11, gravity -1), rolls
//     shape.rot.z toward 0x7FFF while airborne, and ramps shape.yOffset up to 2800
//     so the actor's pivot slides from its feet to its back. On landing it puffs a
//     floor dust ring, then lies there for the on-back timer before righting itself
//     with a second hop (velocity.y 13) and rot.z easing back to 0. Every constant
//     below is that actor's.
//
//   ObjTsubo_Thrown — the free fall. A thrown pot keeps a LIVE AT+OC collider the
//     whole way down, which is what lets it smash things (and be smashed), and it
//     tumbles by stepping shape.rot.x/y toward random targets. The flipped enemy
//     falls the same way, so anything it lands on breaks exactly as if a pot had
//     been thrown at it.
//
// The enemy is paralysed throughout for the simple reason that its own `update` is
// not running at all — this function replaced it.

// EnTite_SetupFlipOnBack
#define PACCI_FLIP_LAUNCH_VEL_Y 11.0f
#define PACCI_FLIP_LAUNCH_GRAVITY -1.0f
#define PACCI_FLIP_ROT_STEP 4000 // rot.z -> 0x7FFF while flipping over
#define PACCI_FLIP_YOFFSET_STEP 400.0f
#define PACCI_FLIP_YOFFSET_MAX 2800.0f
// EnTite_SetupFlipUpright
#define PACCI_FLIP_RIGHT_VEL_Y 13.0f
#define PACCI_FLIP_RIGHT_ROT_STEP 0xFA0
// ObjTsubo_SetupThrown
#define PACCI_FLIP_THROWN_MASS 240
#define PACCI_FLIP_TUMBLE_STEP 0x64

// ObjTsubo_SetupThrown's tumble targets, rolled per flip so two enemies never spin
// identically. Kept on the pool entry rather than in file statics — the pot uses
// four file-scope s16s, which would make every flipped enemy share one spin.
static void Pacci_FlipRollTumble(PacciFx* fx) {
    fx->tumbleTargetX = (s16)((Rand_ZeroOne() - 0.7f) * 2800.0f);
    fx->tumbleTargetY = (s16)((Rand_ZeroOne() - 0.5f) * 2000.0f);
    fx->tumbleX = 0;
    fx->tumbleY = 0;
}

// One frame of ObjTsubo_Thrown's motion: gravity, tumble, bg check, and the live
// AT/OC that does the smashing.
static void Pacci_FlipFallStep(PacciFx* fx, Actor* thisx, PlayState* play) {
    thisx->velocity.y += thisx->gravity;
    if (thisx->velocity.y < thisx->minVelocityY) {
        thisx->velocity.y = thisx->minVelocityY;
    }
    Actor_UpdatePos(thisx);

    Math_StepToS(&fx->tumbleX, fx->tumbleTargetX, PACCI_FLIP_TUMBLE_STEP);
    Math_StepToS(&fx->tumbleY, fx->tumbleTargetY, PACCI_FLIP_TUMBLE_STEP);
    thisx->shape.rot.x += fx->tumbleX;
    thisx->shape.rot.y += fx->tumbleY;

    Actor_UpdateBgCheckInfo(play, thisx, 5.0f, 15.0f, 0.0f, 0x85);
}

// Submit the flipped enemy's stand-in colliders for this frame.
//
// AC and OC run for the WHOLE effect: the enemy has to stay hittable while it is
// down, which is the entire reason the move exists. AT only runs while it is in the
// air — a downed enemy resting on a pot should not keep smashing it every frame.
static void Pacci_FlipSubmitColliders(PacciFx* fx, Actor* thisx, PlayState* play, u8 airborne) {
    if (!fx->colliderReady) {
        return;
    }
    Collider_UpdateCylinder(thisx, &fx->collider);
    if (airborne) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &fx->collider.base);
    }
    CollisionCheck_SetAC(play, &play->colChkCtx, &fx->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &fx->collider.base);
}

static void Pacci_FlipUpdate(Actor* thisx, PlayState* play) {
    PacciFx* fx = Pacci_FindEntry(thisx);

    if (fx == NULL) {
        return; // pool entry vanished — leave the actor frozen rather than crash
    }

    // Hit while it was down. Handing the actor straight back to its own update here
    // was wrong: it woke up on the very frame it was struck, so the flip bought no
    // free hits at all. Instead the damage is applied to its health WITHOUT waking
    // it, and it stays helpless — which is the whole point of knocking it over.
    //
    // Only the killing blow gives the actor back, and it goes back with colChkInfo
    // untouched, so its own update runs its own death: its animation, its drops,
    // its effects. Nothing here needs to know how any particular enemy dies.
    if (fx->colliderReady && (fx->collider.base.acFlags & AC_HIT)) {
        fx->collider.base.acFlags &= ~AC_HIT;

        if (thisx->colChkInfo.damage > 0) {
            if (thisx->colChkInfo.health > thisx->colChkInfo.damage) {
                thisx->colChkInfo.health -= thisx->colChkInfo.damage;
                // Flash white on the hit so a free hit still reads as a hit.
                PACCI_TINT_HURT(thisx, 12);
                Audio_PlayActorSound2(thisx, PACCI_SFX_FLIP_LAND);
                thisx->colChkInfo.damage = 0;
            } else {
                thisx->colChkInfo.health = 0;
                Pacci_Restore(fx); // lethal — let it die its own way
                return;
            }
        }
    }

    switch (fx->phase) {
        case PACCI_FLIP_PHASE_AIRBORNE:
            // Roll onto its back on the way up (EnTite_FlipOnBack).
            Math_SmoothStepToS(&thisx->shape.rot.z, 0x7FFF, 1, PACCI_FLIP_ROT_STEP, 0);
            Pacci_FlipFallStep(fx, thisx, play);
            Pacci_FlipSubmitColliders(fx, thisx, play, true);

            if (thisx->bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH)) {
                if (thisx->bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
                    Actor_SpawnFloorDustRing(play, thisx, &thisx->world.pos, 20.0f, 11, 4.0f, 0, 0, false);
                    Audio_PlayActorSound2(thisx, PACCI_SFX_FLIP_LAND);
                }
                // A flipped PROP lands like one Link had picked up and dropped: it
                // takes the impact and breaks. Only enemies stay down and helpless —
                // lying there paralysed is what flipping an enemy is FOR.
                if (thisx->category != ACTORCAT_ENEMY) {
                    Player* impactPlayer = GET_PLAYER(play);
                    Vec3f impact = thisx->world.pos;
                    u8 heavy = (thisx->id == ACTOR_EN_ISHI);

                    Pacci_Restore(fx); // hand it back before the burst resolves
                    if (impactPlayer != NULL) {
                        Pacci_BurstSpawn(play, impactPlayer, &impact, heavy);
                    }
                    return;
                }
                fx->phase = PACCI_FLIP_PHASE_DOWNED;
                fx->timer = PACCI_FLIP_ON_BACK_TIMER;
                thisx->speedXZ = 0.0f;
            } else {
                // Slide the pivot from its feet to its back so it visibly lies on
                // its shell rather than hovering upside down over its own origin.
                if (thisx->shape.yOffset < PACCI_FLIP_YOFFSET_MAX) {
                    thisx->shape.yOffset += PACCI_FLIP_YOFFSET_STEP;
                }
            }
            break;

        case PACCI_FLIP_PHASE_DOWNED:
            // Paralysed on its back. Still held at 0x7FFF so a slope cannot roll it.
            Math_SmoothStepToS(&thisx->shape.rot.z, 0x7FFF, 1, PACCI_FLIP_ROT_STEP, 0);
            Math_StepToF(&thisx->speedXZ, 0.0f, 1.0f);
            Actor_MoveXZGravity(thisx);
            Actor_UpdateBgCheckInfo(play, thisx, 5.0f, 15.0f, 0.0f, 0x85);
            Pacci_FlipSubmitColliders(fx, thisx, play, false);
            if (fx->timer > 0) {
                fx->timer--;
            } else {
                // EnTite_SetupFlipUpright: a hop, and rot.z eases back to normal.
                fx->phase = PACCI_FLIP_PHASE_RIGHTING;
                thisx->velocity.y = PACCI_FLIP_RIGHT_VEL_Y;
                Audio_PlayActorSound2(thisx, PACCI_SFX_FLIP);
            }
            break;

        case PACCI_FLIP_PHASE_RIGHTING:
            Math_SmoothStepToS(&thisx->shape.rot.z, fx->origShapeRot.z, 1, PACCI_FLIP_RIGHT_ROT_STEP, 0);
            Actor_MoveXZGravity(thisx);
            Actor_UpdateBgCheckInfo(play, thisx, 5.0f, 15.0f, 0.0f, 0x85);
            Pacci_FlipSubmitColliders(fx, thisx, play, false);
            if (thisx->bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
                Audio_PlayActorSound2(thisx, PACCI_SFX_FLIP_LAND);
                thisx->shape.yOffset = fx->origYOffset;
                thisx->world.pos.y = thisx->floorHeight;
                Pacci_Restore(fx); // back on its feet; its own AI takes over again
                return;
            }
            break;
    }

    thisx->focus.pos = thisx->world.pos;
}

u8 Pacci_CastFlip(PlayState* play, Player* player) {
    // Flip used to scan ACTORCAT_ENEMY only, which is why it silently refused pots,
    // crates and boulders — the prop list existed but only the lift consulted it.
    // Both moves now share one notion of "something I can grab".
    Actor* target = Pacci_ScanLiftable(play);
    PacciFx* fx;

    if (target == NULL) {
        return 0;
    }
    fx = Pacci_TakeEntry();
    if (fx == NULL) {
        return 0;
    }

    fx->actor = target;
    fx->mode = PACCI_FX_FLIP;
    fx->phase = PACCI_FLIP_PHASE_AIRBORNE;
    fx->timer = 0;
    fx->origUpdate = target->update;
    fx->origDraw = NULL; // Flip keeps the enemy's own look
    fx->origFlags = target->flags;
    fx->origGravity = target->gravity;
    fx->origMinVelocityY = target->minVelocityY;
    fx->origSpeed = target->speedXZ;
    fx->origShapeRot = target->shape.rot;
    fx->origWorldRot = target->world.rot;
    fx->origRoom = target->room;
    fx->origMass = target->colChkInfo.mass;
    fx->origYOffset = target->shape.yOffset;
    Pacci_FlipRollTumble(fx);

    if (!fx->colliderReady) {
        Collider_InitCylinder(play, &fx->collider);
        fx->colliderReady = 1;
    }
    Collider_SetCylinder(play, &fx->collider, target, &sPacciFallColliderInit);

    // EnTite_SetupFlipOnBack: straight up. Plus a thrown pot's mass, so on the way
    // down it shoulders things aside instead of being shoved by them.
    target->update = Pacci_FlipUpdate;
    target->gravity = PACCI_FLIP_LAUNCH_GRAVITY;
    target->minVelocityY = PACCI_FLIP_MIN_VEL_Y;
    target->velocity.y = PACCI_FLIP_LAUNCH_VEL_Y;
    target->speedXZ = 0.0f;
    target->colChkInfo.mass = PACCI_FLIP_THROWN_MASS;
    target->bgCheckFlags &= ~(BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH);
    // Culling would stop OUR update too, freezing the enemy mid-flip until the
    // player walked back into range. The flags are part of origFlags, so they go
    // back to normal on recovery.
    target->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;

    Audio_PlayActorSound2(target, PACCI_SFX_FLIP);
    PacciFlipVfx_Start(play, player, target); // stub — see pacci_flip_vfx.h
    return 1;
}

// ============================================================================
// LIFT / THROW  (Pacci: hold C)
// ============================================================================
//
// Holding the button telekinetically picks the target up and holds it in the air
// in front of Link; releasing throws it. If Link is Z-targeting something when he
// lets go, the throw is aimed at that target, so a lifted enemy or pot becomes a
// projectile you can hurl into something else.
//
// WHAT CAN BE LIFTED, and why it is deliberately narrow
//   - Props: an EXPLICIT list (sPacciLiftableProps). What each tool can move is the
//     only thing separating this from Magnesis and from Ultrahand; leaving it open
//     collapses all three into one ability.
//   - Enemies: only on their LAST point of health. Anything more generous turns the
//     lift into a delete button, and it is meant to be a finisher.
//   - And only within PACCI_LIFT_RADIUS. This is the deliberate difference from the
//     Switch Hook, which does not care how far its target is: the lift is short
//     ranged on purpose, so each tool owns a distinct band of reach.

// Enemies have to be worn down before Pacci can manhandle them. 1 HP proved far too
// strict — sharing this gate with Flip meant almost nothing in a room qualified and
// the move looked broken. Three is low enough to still be a finisher.
#define PACCI_LIFT_MAX_HP 3
#define PACCI_LIFT_RADIUS 220.0f
#define PACCI_LIFT_DIST 70.0f   // held this far in front of Link
#define PACCI_LIFT_HEIGHT 45.0f // and this far above him
#define PACCI_LIFT_FOLLOW 0.55f // weight of the OLD position; snappier than Ultrahand
#define PACCI_LIFT_THROW_SPEED 12.0f
// Nearly flat. The object is ALREADY held PACCI_LIFT_HEIGHT above Link, so adding a
// real upward kick on release lobbed it way over whatever you were aiming at — the
// arc has to start from that raised position, not from Link's feet.
#define PACCI_LIFT_THROW_VEL_Y 0.5f
#define PACCI_LIFT_GRAVITY -1.4f
#define PACCI_LIFT_FLIGHT_FRAMES 90 // give up and drop it after this long in flight

typedef struct {
    Actor* held;
    u8 thrown;
    s16 flightTimer;
    f32 origGravity;
    f32 origMinVelocityY;
    s16 origRoom;
    ActorFunc origUpdate;
    u32 origFlags;
    u8 frozeEnemy;
    PacciFx* fx; // pool entry that owns the collider doing the smashing
} PacciLift;

static PacciLift sLift = { 0 };

// The cane owns movement and collision while an enemy is held or thrown. Keeping
// a live no-op update also prevents the actor list from treating it as destroyed.
static void Pacci_LiftFrozenEnemyUpdate(Actor* actor, PlayState* play) {
}

u8 Pacci_IsLifting(void) {
    return (sLift.held != NULL) && !sLift.thrown;
}

// Enemies Pacci must never touch. Bosses are excluded by category; these are the
// ACTORCAT_ENEMY entries that would break if knocked over — arena minibosses with
// their own scripted state, and anything that is not really a body.
// Same bit the switch hook uses to let a custom actor opt in regardless of its category
// (item_switchhook.h). Declared with a guard rather than by including that header: this file is
// pulled into the cane's translation unit and must not start dragging item headers in with it.
#ifndef PACCI_FLAG_LIFTABLE
#define PACCI_FLAG_LIFTABLE (1 << 28)
#endif

static const s16 sPacciBlacklist[] = {
    ACTOR_EN_IK,       // Iron Knuckle
    ACTOR_EN_TORCH2,   // Dark Link
    ACTOR_EN_ZF,       // Lizalfos / Dinolfos
    ACTOR_EN_WALLMAS,  // Wallmaster
    ACTOR_EN_FLOORMAS, // Floormaster
    ACTOR_EN_RD,       // Redead / Gibdo
    ACTOR_EN_FZ,       // Freezard
    ACTOR_EN_VM,       // Beamos
    ACTOR_EN_RR,       // Like Like
    // Invisible song spots. They have no model and no texture - their whole job is to sit
    // on a patch of ground and notice an ocarina - so grabbing one hauls nothing visible
    // around and, worse, carries the trigger away from the place it is supposed to be.
    ACTOR_EN_OKARINA_TAG,
    ACTOR_EN_OKARINA_EFFECT,
};

// -- per-actor traits ----------------------------------------------------------
// Some actors are not generic objects, and pretending otherwise is what makes a physics toy feel
// broken in a dungeon. A lift dragged out of its shaft, a room quadrant carried in front of you,
// an ice block lifted over the wall it was supposed to slide against - each one is a puzzle
// solved by ignoring it rather than by using the tool.
//
// So: a table, and a deny/constrain list rather than anything derived. "Too big to pick up",
// "belongs on a rail", "is on fire" are not properties the actor struct exposes and never will
// be. Adding a case is adding a row.
typedef enum {
    PACCI_UH_TRAIT_EXCLUDE = 1 << 0,  // not liftable at all
    PACCI_UH_TRAIT_AXIS_Y = 1 << 1,   // up and down only; XZ pinned to the grab
    PACCI_UH_TRAIT_PLANE_XZ = 1 << 2, // along the ground only; Y pinned to the grab
    PACCI_UH_TRAIT_PATH = 1 << 3,     // confined to the scene path its params name
    PACCI_UH_TRAIT_NO_TURN = 1 << 4,  // orientation frozen at the grab pose
    PACCI_UH_TRAIT_BURNS = 1 << 5,    // sets fire to what it touches while carried
    // Grabbable even though neither normal route can see it. The raycast only finds dynapoly and
    // the actor scan only walks the categories TargetSelect_IsCommonTarget covers, so anything
    // outside both - En_Ice_Hono is ACTORCAT_ITEMACTION and has no collision header - is
    // unreachable no matter what the filter says. This opts a row in explicitly.
    PACCI_UH_TRAIT_REACHABLE = 1 << 6,
    // Does not turn to follow Link, but DOES still answer L + D-pad. Distinct from NO_TURN, which
    // freezes the pose outright: a lift is a floor and has no business being rotated at all, while
    // a block is something you line up deliberately and nothing else should be nudging.
    PACCI_UH_TRAIT_NO_FACE = 1 << 7,
    // Moving it far enough by hand sets the switch flag its params name. For the Dodongo's Cavern
    // machinery, whose whole job is to be somewhere and whose "somewhere" is normally decided by a
    // flag you set elsewhere in the room.
    PACCI_UH_TRAIT_SETS_FLAG = 1 << 8,
    // ...and moving it back the other way clears it again. Only for the ones where both states are
    // things the room can be in: a staircase can be up or down, a mouth open or shut. Deliberately
    // NOT on the shortcut platform, where the flag is progress and undoing it by carrying the
    // thing downstairs would be a trap rather than a mechanic.
    PACCI_UH_TRAIT_CLEARS_FLAG = 1 << 9,
    // Held, but never moved. The body stays exactly where it is and the D-pad drives its FLAG
    // instead of its position - for machinery whose two states are the only thing about it that
    // was ever meant to change.
    PACCI_UH_TRAIT_LOCKED = 1 << 10,
    // SETS_FLAG only: the flag index is only real when params fit entirely inside the field.
    // Vanilla's own rule for the platforms, and the difference between "this one has a switch" and
    // "this one's params mean something else and we would be flipping a stranger's switch".
    PACCI_UH_TRAIT_FLAG_STRICT = 1 << 11,
    // Grabbing it does not take IT - it spawns something and you carry that instead. For the
    // things whose whole job is to stay where they are: a bomb flower keeps its flower, a flame
    // keeps burning in its bowl, and what comes with you is a copy that dies when you let go.
    PACCI_UH_TRAIT_PROXY = 1 << 12,
    // The proxy is a live bomb: its fuse is held off while carried, and the attach button
    // detonates it instead of welding it.
    PACCI_UH_TRAIT_EXPLODES = 1 << 13,
    // The constraint writes home.pos instead of world.pos.
    //
    // For the actors that rebuild their position from home every frame. Bg_Hidan_Fslift steps its
    // y toward home.pos.y, or home.pos.y + 790 when someone is standing on it, and it does that
    // from scratch each frame - so writing world.pos is writing into a value that is about to be
    // recomputed, and moving one meant moving where it thinks it lives.
    PACCI_UH_TRAIT_DRIVE_HOME = 1 << 14,
    // Aiming at it HITS it, with whatever kind of blow that particular actor is waiting for. No
    // carry: the cane lands the hit its damage table already accepts and gets out of the way, so
    // the actor plays its own reaction, sets its own flag and dies its own death.
    //
    // Which blow is per row, because the answer is different every time: the Jabu tentacle only
    // accepts a boomerang, the bombchu rock only an explosion. One mechanism, one field.
    PACCI_UH_TRAIT_STRIKES = 1 << 15,
    // Held in place, D-pad drives its HEIGHT rather than its position - for the one actor whose
    // size is the thing worth changing about it.
    PACCI_UH_TRAIT_HEIGHT = 1 << 16,
    // Aiming at it THROWS it, by handing it to its own throw. Nothing is carried and nothing is
    // written except the one field its state machine is already watching.
    PACCI_UH_TRAIT_THROWS = 1 << 17,
    // Not carried - HAULED. Link braces and pulls it along the ground with the vanilla pulling
    // animation, for the things that are too big to float in front of you and would look absurd
    // doing it.
    PACCI_UH_TRAIT_PULLABLE = 1 << 18,
    // The D-pad swings ONE hinge of this body open and shut.
    //
    // Split out of LOCKED, which used to imply it. That was written for the Dodongo's jaw and then
    // quietly tilted every other locked body 26 degrees on its X axis - the chain platform, the
    // grate, the bombchu rock and the coffin lid all leaned over while you held them.
    PACCI_UH_TRAIT_JAW = 1 << 19,
    // Same gesture, but handed to an animation the actor already owns instead of posed by hand.
    PACCI_UH_TRAIT_HINGE = 1 << 20,
} PacciUhTrait;

// Bg_Mizu_Movebg is seven machines wearing one actor id. MOVEBG_TYPE is the top nibble of params
// and it decides which; these four split the table row by row so each variant gets the truth.
static u8 Pacci_UhMovebgType(Actor* actor) {
    return (u8)(((u16)actor->params >> 0xC) & 0xF);
}

static u8 Pacci_UhCondMovebgWaterSlaved(Actor* actor) {
    return (Pacci_UhMovebgType(actor) <= 2) ? 1 : 0;
}

static u8 Pacci_UhCondMovebgDragonRoom(Actor* actor) {
    return (Pacci_UhMovebgType(actor) == 3) ? 1 : 0;
}

static u8 Pacci_UhCondMovebgSwitched(Actor* actor) {
    u8 type = Pacci_UhMovebgType(actor);

    return ((type >= 4) && (type <= 6)) ? 1 : 0;
}

static u8 Pacci_UhCondMovebgHookshot(Actor* actor) {
    return (Pacci_UhMovebgType(actor) == 7) ? 1 : 0;
}

// Only the deck. The two chain segments are spawned as its children and driven by it - grabbing one
// of those would be grabbing a limb.
//
// The deck is params -1 (DT_DRAWBRIDGE), which lives in an enum inside z_bg_spot00_hanebasi.c and
// not in its header, so the value is spelled out rather than named. The chains are 0 and 1.
static u8 Pacci_UhCondDrawbridge(Actor* actor) {
    return (actor->params == -1) ? 1 : 0;
}

typedef struct {
    s16 actorId;
    // u32, not u16, and that is load-bearing. PacciUhTrait runs past bit 15 - HEIGHT is 1 << 16 and
    // HINGE is 1 << 20 - so a u16 field silently truncated the top five traits to nothing, in the
    // TABLE itself. Every row that used one was a row with no traits at all, and the five newest
    // behaviours were dead on arrival with nothing to see in the code that declared them.
    u32 traits;
    // A bit field inside params, spelled out per row because every actor packs it somewhere
    // different. PATH reads a scene path index out of it; SETS_FLAG reads a switch flag index.
    u8 pathShift;
    u8 pathMask;
    // Optional. Some rows are about a STATE, not an actor: a torch only burns while it is lit, and
    // only the persistent blue flame is worth carrying. NULL means the row applies unconditionally.
    u8 (*cond)(Actor* actor);
    // SETS_FLAG only: which way the body has to travel before the flag is set. +1 up, -1 down,
    // 0 either. It matters because these mean opposite things - a platform is unlocked by being
    // raised and a staircase by being pushed down - and setting a flag the wrong way round would
    // solve the room by accident.
    s8 flagDir;

    // PROXY only: what to spawn in the body's place, and with what params.
    s16 proxyId;
    s16 proxyParams;
    // STRIKES only: the blow to land, and how hard. dmgFlags has to be something the target's own
    // bumper accepts or nothing happens at all - which is the point, since it means the actor's
    // rules decide, not ours.
    u32 hitFlags;
    s16 hitDamage;
} PacciUhTraitRow;

// A torch's flame is a separate collider from its stand, and it is only submitted while the torch
// is actually alight. Asking whether it is in this frame's AC list is a real read of the actor's
// state; reaching into ObjSyokudai's private struct for litTimer would be a guess about a layout
// this file has no business knowing.
static u8 Pacci_UhTorchLit(PlayState* play, Actor* actor) {
    s32 i;

    for (i = 0; i < play->colChkCtx.colACCount; i++) {
        Collider* col = play->colChkCtx.colAC[i];

        // The flame is the one with no OC: the stand collides with you, the fire does not.
        if ((col != NULL) && (col->actor == actor) && (col->ocFlags1 == OC1_NONE)) {
            return 1;
        }
    }
    return 0;
}

static u8 Pacci_UhCondTorchLit(Actor* actor) {
    return (gPlayState != NULL) ? Pacci_UhTorchLit(gPlayState, actor) : 0;
}

// Only the PERSISTENT blue flame - the one placed in the scene, the one a bottle can scoop up.
// params 0xFFFF is that variant; the short-lived ones a Freezard breathes out are not something
// you should be able to pocket and walk off with.
// Vanilla treats params as a switch index only below 0x40 - see BgDdanJd_Idle, z_bg_ddan_jd.c:89.
static u8 Pacci_UhCondSwitchParams(Actor* actor) {
    return (actor->params < 0x40) ? 1 : 0;
}

static u8 Pacci_UhCondPersistent(Actor* actor) {
    return ((u16)actor->params == 0xFFFF) ? 1 : 0;
}

// Child Ruto, SITTING - which is the state that means "waiting for someone to pick me up".
// gRutoChildSittingAnim is what plays immediately before Actor_OfferCarry in both of the places
// that offer the carry (z_en_ru1.c:1691-1698 and :1857-1866).
//
// Two wrong answers came before this one and both are worth recording. BGCHECKFLAG_GROUND was
// backwards: it is checked by func_80AEEAC8, which is the path for her LANDING after a drop, and
// not by func_80AEF1F0, which is the path for her sitting down after the conversation - so it
// allowed standing and refused sitting, exactly inverted. Then parent == NULL alone was too loose:
// it means "nobody is carrying her", which is true standing, sitting, floating and treading water.
//
// So the animation is read directly, through EnRu1 itself.
//
// The first version of this computed the address from the /* 0x014C */ in z_en_ru1.h. That is an
// N64 offset and this is a 64-bit build, where Actor's own pointers are twice as wide - so it was
// reading well short of skelAnime, never matched, and quietly made her ungrabbable in every state.
// It failed safe, which is the only good thing about it. The header is the answer; it knows.
static u8 Pacci_UhCondRutoSitting(Actor* actor) {
    if ((actor == NULL) || (actor->parent != NULL)) {
        return 0; // already in somebody's arms - Link's or ours
    }
    return (((EnRu1*)actor)->skelAnime.animation == (void*)gRutoChildSittingAnim) ? 1 : 0;
}

// Friendly names are the ones the wiki shows the player (prelude.roborich.com/wiki/oot/actors),
// so a comment here and a bug report say the same words.
static const PacciUhTraitRow sPacciUhTraits[] = {
    // -- architecture, not objects --------------------------------------------------------------
    { ACTOR_BG_MORI_KAITENKABE, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL }, // Rotatable Walls: a whole
                                                                      // room quadrant, with Link
                                                                      // standing on it
    // Golden Gauntlets Pillar. It cannot be CARRIED - its own Draw rewrites world.pos and home.pos
    // from a matrix every frame, so any position written to it survives until the next draw call -
    // but it can be thrown, because throwing is a state it already has and already knows how to do
    // properly, cutscene camera and quake included.
    { ACTOR_BG_HEAVY_BLOCK, PACCI_UH_TRAIT_THROWS, 0, 0, NULL },
    // The ferry, pushed and pulled along the canal by hand. Nothing in BgHakaShip_Move recomputes
    // x or z - only y, which it rebuilds from home plus a sine to make it bob - so the horizontal
    // plane is genuinely free and the vertical one is genuinely not. PLANE_XZ says exactly that.
    //
    // Dragging it does not skip the wreck either: the trigger is a distance from home
    // (home.pos.x - world.pos.x > 7600, z_bg_haka_ship.c:130) checked every frame, so shoving it
    // to the far end runs the crash the same way sailing there does.
    { ACTOR_BG_HAKA_SHIP, PACCI_UH_TRAIT_PLANE_XZ | PACCI_UH_TRAIT_NO_TURN, 0, 0, NULL },
    { ACTOR_BG_MIZU_WATER, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },     // Water Plane: not a body,
                                                                     // it is the water level
    { ACTOR_BG_JYA_ZURERUKABE, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL }, // Sliding Climbable Wall
    { ACTOR_BG_JYA_AMISHUTTER, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL }, // Sliding Metal Grate
    { ACTOR_BG_HIDAN_HAMSTEP, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },  // Stone Steps and Platforms
    // Square Collapsing Platform. Its update ends in Math_Vec3f_Copy(world.pos, home.pos), so it
    // teleports back the frame you let go: moving it was never anything but a visual lie.
    { ACTOR_OBJ_LIFT, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    // Eye Statue. Its JntSph collider centre is baked at Init and never refreshed, so moving it
    // leaves the hitbox behind and the puzzle breaks without anything looking wrong.
    { ACTOR_BG_MENKURI_EYE, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    // The rest of the Forest Temple's room hardware. Pacci_UhTooBig already catches these by size,
    // and they are named anyway: a row says "this was decided", a threshold only says "this was
    // measured", and the twisted corridors in particular are the ones that were being carried off.
    { ACTOR_BG_MORI_HINERI, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },     // Twisted Corridor
    { ACTOR_BG_MORI_BIGST, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },      // Big Stone Block room
    { ACTOR_BG_MORI_RAKKATENJO, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL }, // Falling Ceiling
    { ACTOR_BG_MORI_IDOMIZU, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },    // Well water room
    { ACTOR_BG_MORI_HASHIRA4, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },   // Four Pillars
    { ACTOR_BG_MORI_HASHIGO, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },    // Ladder

    // -- lifts: their own vertical axis, nothing else --------------------------------------------
    // These two keep a switch flag for "I am up", so raising one by hand is the same statement as
    // whatever normally raises it. (Obj_Elevator and Bg_Hidan_Syoku are left plain: their params
    // are scales and heights, there is no flag in them to set.)
    { ACTOR_BG_MORI_ELEVATOR, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_SETS_FLAG, 0, 0x3F, NULL,
      1 },
    // Chain Platform. It was an AXIS_Y lift and it is a TOGGLE now, because that is what it
    // actually is: BgJyaLift_Move only ever steps between two hardcoded heights, 1613 and 973, and
    // then nulls its own actionFunc. There is no continuum to slide it along - the two heights are
    // the whole vocabulary - so driving the flag says the same thing more directly.
    { ACTOR_BG_JYA_LIFT, PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_SETS_FLAG, 0, 0x3F, NULL, 1 },
    { ACTOR_OBJ_ELEVATOR, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN, 0, 0, NULL },
    // -- Dodongo's Cavern: moved by hand, and the room notices ------------------------------------
    // These three are all the same idea. Each one's position is normally decided by a switch flag
    // set somewhere else in the room, and each one already knows how to be in both places. So
    // moving one there YOURSELF sets that flag: you did the thing the switch was going to do.
    //
    // Rising Stone Platform. It cycles bottom to middle on its own; the flag upgrades it to the
    // shortcut that goes all the way to MOVE_HEIGHT_TOP, 700 units (z_bg_ddan_jd.c:44-45, 89-94).
    // Carry it up far enough and that route is open from then on.
    //
    // EVERY platform gets the axis; only the ones that HAVE a flag can set one. Unlocking the tall
    // route on the others is not something that can be built: BgDdanJd_Idle only ever consults a
    // switch when params < 0x40 (z_bg_ddan_jd.c:89), and a platform outside that range never asks
    // anything, so there is no flag to set and nothing that would read it if there were. Its two
    // heights are all it has.
    //
    // FLAG_STRICT is that rule expressed as data, and it gates the FLAG rather than the row, which
    // is why the condition that used to be here is gone - it took AXIS_Y down with it.
    { ACTOR_BG_DDAN_JD,
      PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_SETS_FLAG | PACCI_UH_TRAIT_FLAG_STRICT, 0, 0x3F,
      NULL, 1 },
    // The staircase. Its own code sets this flag when it finishes descending
    // (z_bg_ddan_kd.c:142-144), so pushing it down by hand is doing the same thing by hand.
    // ...and the flag traits are gone from it. BgDdanKd only reads that flag in its Init; at runtime
    // it is written by BgDdanKd_LowerStairs and read by nobody, so setting it by hand moved a
    // number and nothing else. Pushing the staircase down still works, because CheckForExplosions
    // writes no position at all.
    { ACTOR_BG_DDAN_KD, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN, 0, 0, NULL },
    // The skull mouth: LOCKED. It is the wall of the room with a face on it, and dragging it around
    // was never the point - the only thing about it that is meant to change is whether the mouth is
    // open. So it does not move at all, and D-up / D-down open and shut it directly. The flag is
    // params & 0x3F (z_bg_dodoago.c:131, 169).
    // -- state, not position ---------------------------------------------------------------------
    // Held in place with D-up / D-down driving the switch flag, the same gesture as the skull. Each
    // of these is a thing with two states and no meaningful third place to be.
    //
    // All three keep their flag index readable at runtime, which is the whole reason they qualify:
    // params & 0x3F survives, so the flag can still be found while the actor is alive. Several
    // other candidates do `params &= 0xFF` in their Init and throw the index away - see the note on
    // sPacciUhTraits below.
    // These three are ONE WAY, and the CLEARS_FLAG they used to carry was a lie in all three.
    // BgJyaLift_SetFinalPosY and func_80899A08 both null their own actionFunc when they arrive, and
    // BgHakaHuta_Open never looks back - so clearing the flag afterwards moved nothing and only
    // switched off something the rest of the room might be reading.
    { ACTOR_BG_JYA_KANAAMI, PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_SETS_FLAG, 0, 0x3F, NULL,
      1 }, // Sliding Metal Grate: world.rot.x 0 up, 0x4000 fallen
    // Bombchu rock. It was a flag toggle and the flag did nothing: BgJyaBombchuiwa_Init is the only
    // place that reads it (z_bg_jya_bombchuiwa.c:82), so flipping it mid-room changed nothing until
    // you left and came back. What the rock is actually listening for is an explosion - bumper
    // 0x00000008, DMG_EXPLOSIVE - and one of those brings down the whole thing: rubble, sound, and
    // the light ray it was hiding.
    { ACTOR_BG_JYA_BOMBCHUIWA, PACCI_UH_TRAIT_STRIKES, 0, 0, NULL, 0, 0, 0, DMG_EXPLOSIVE, PACCI_UH_CUT_DAMAGE },
    // Bg_Haka_Huta, the coffin lid. Its Init does `params &= 0xFF` and then uses what is left AS the
    // flag index, so unlike the others the surviving byte IS the flag - it is readable at runtime by
    // accident of that ordering rather than by design, and it works.
    { ACTOR_BG_HAKA_HUTA, PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_SETS_FLAG, 0, 0xFF, NULL, 1 },

    // JAW is what actually opens it. The flag is Init-only here too, so the mouth is posed directly
    // and the flag comes along for the room's sake - which is also why this one keeps CLEARS_FLAG
    // when its neighbours lost it: the jaw is OUR pose, so shutting it again is ours to do.
    { ACTOR_BG_DODOAGO,
      PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_JAW | PACCI_UH_TRAIT_SETS_FLAG | PACCI_UH_TRAIT_CLEARS_FLAG, 0, 0x3F, NULL,
      1 },
    // Stone Elevator (Fire Temple) re-derives its Y from a cosine of its own timer every frame, so
    // this row only works because the carry freezes the actor's update. If that ever changes, this
    // one has to become an EXCLUDE rather than quietly fighting the player.
    // The three Fire Temple movers all rebuild their position from home.pos every frame, so all
    // three are driven the same way - by moving where they think they live. That is not a
    // workaround, it is the only handle they have.
    //
    // Stone Elevator: world.pos.y = cos(timer * pi/140) * 540 + home.pos.y. The cosine is its whole
    // life and it is not going to stop; moving home slides the entire cycle up or down, so it keeps
    // its rhythm at a new height instead of being yanked out of it.
    { ACTOR_BG_HIDAN_SYOKU, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_DRIVE_HOME, 0, 0, NULL },
    // Stone Blocks. Type 0 is a full push/pull block with its own accumulated distance; the rise
    // and fall states both step world.pos.y toward home.pos.y plus a constant (+1820 for type 0,
    // +480 otherwise), so home is the handle for those too.
    { ACTOR_BG_HIDAN_ROCK, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_DRIVE_HOME, 0, 0, NULL },
    // Sinking platform / fire-jet shuttle. Both variants read home: the sinker steps between
    // home.pos.y and home.pos.y - 100, and the shuttle builds its x/z from home.pos plus a 200-unit
    // sine. Raising home raises the floor of both.
    { ACTOR_BG_HIDAN_SIMA, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_DRIVE_HOME, 0, 0, NULL },
    // Hookshot elevator. Its y is rewritten from home.pos.y every frame - toward home when nobody
    // is aboard, toward home + 790 when somebody is - so it is home that has to move. Writing
    // world.pos here would be shouting into a value the actor overwrites before anyone sees it.
    { ACTOR_BG_HIDAN_FSLIFT, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_DRIVE_HOME, 0, 0, NULL },

    // -- blocks that SLIDE -----------------------------------------------------------------------
    // Pinning Y is not a restriction here, it is the whole point: the puzzle is which way the block
    // goes on the floor, and lifting one over the obstacle it is supposed to catch on deletes the
    // room. Obj_Oshihiki is deliberately NOT in this list - a pushable block stays fully liftable.
    { ACTOR_BG_ICE_OBJECTS, PACCI_UH_TRAIT_PLANE_XZ | PACCI_UH_TRAIT_NO_TURN, 0, 0, NULL },
    { ACTOR_BG_GND_ICEBLOCK, PACCI_UH_TRAIT_PLANE_XZ | PACCI_UH_TRAIT_NO_TURN, 0, 0, NULL },

    // -- confined to a scene path ----------------------------------------------------------------
    // Each of these packs the path index somewhere different, which is exactly why the shift and
    // mask are per row. En_Goroiwa is deliberately absent: a rolling boulder is more fun loose, and
    // it picks its route back up from wherever you set it down.
    // Water Temple platforms: SEVEN different machines behind one actor id, and one row was never
    // going to describe them. MOVEBG_TYPE is params >> 12, and BgMizuMovebg_UpdateMain switches on
    // it every frame (z_bg_mizu_movebg.c:248-300).
    //
    // Types 0/1/2 are welded to the water: world.pos.y = waterBoxes[2].ySurface + 15, rewritten
    // every frame from the room's water level. There is no height to give them.
    { ACTOR_BG_MIZU_MOVEBG, PACCI_UH_TRAIT_EXCLUDE, 0, 0, Pacci_UhCondMovebgWaterSlaved },
    // Type 3 steps toward this->homeY - the actor's OWN field, not actor.home.pos - so neither the
    // position nor home is a handle. Left alone rather than pretending.
    { ACTOR_BG_MIZU_MOVEBG, PACCI_UH_TRAIT_EXCLUDE, 0, 0, Pacci_UhCondMovebgDragonRoom },
    // Types 4/5/6 are a real switch toggle: home Y, or home Y + 115.2 when params & 0x3F is set,
    // stepped 1.0 a frame and re-read every frame. That one goes both ways.
    { ACTOR_BG_MIZU_MOVEBG, PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_SETS_FLAG | PACCI_UH_TRAIT_CLEARS_FLAG, 0, 0x3F,
      Pacci_UhCondMovebgSwitched, 1 },
    // Type 7 is the hookshot platform, and it really does ride a scene path.
    { ACTOR_BG_MIZU_MOVEBG, PACCI_UH_TRAIT_PATH | PACCI_UH_TRAIT_NO_TURN, 8, 0xF, Pacci_UhCondMovebgHookshot },
    { ACTOR_OBJ_BEAN, PACCI_UH_TRAIT_PATH | PACCI_UH_TRAIT_NO_TURN, 8, 0x1F, NULL },

    // -- fire: carrying one is carrying an open flame --------------------------------------------
    // The Flame Circle is the one fire you carry FOR REAL. Moving it is how it solves things - it
    // is a barrier, and taking the barrier somewhere else is the puzzle - so no proxy here.
    { ACTOR_BG_HIDAN_CURTAIN, PACCI_UH_TRAIT_BURNS, 0, 0, NULL }, // Flame Circle
    // The walls stay where they are and what comes away is a PIECE OF FIRE - an En_Light, the
    // game's own loose flame. Carrying a copy of the wall itself would have been a second wall
    // following you around, which is not what taking fire off a fire looks like. Params 0 is the
    // plain orange flame at full radius (z_en_light.c:38-46).
    { ACTOR_BG_HIDAN_FIREWALL, PACCI_UH_TRAIT_BURNS | PACCI_UH_TRAIT_PROXY, 0, 0, NULL, 0, ACTOR_EN_LIGHT, 0 },
    { ACTOR_BG_HIDAN_FWBIG, PACCI_UH_TRAIT_BURNS | PACCI_UH_TRAIT_PROXY, 0, 0, NULL, 0, ACTOR_EN_LIGHT, 0 },
    // En_Light is literally "Flame" (z_en_light.c:4) - the loose fires and torch flames. Same deal.
    { ACTOR_EN_LIGHT, PACCI_UH_TRAIT_BURNS | PACCI_UH_TRAIT_PROXY | PACCI_UH_TRAIT_REACHABLE, 0, 0, NULL, 0,
      ACTOR_EN_LIGHT, 0 },
    // A bomb flower keeps its flower. What you lift off it is a BOMB, and one with no fuse running
    // while you hold it - a timer would make every carry a countdown and every plan a sprint.
    // The attach button lights it instead of welding it.
    { ACTOR_EN_BOMBF, PACCI_UH_TRAIT_PROXY | PACCI_UH_TRAIT_EXPLODES, 0, 0, NULL, 0, ACTOR_EN_BOM, 0 },
    { ACTOR_OBJ_SYOKUDAI, PACCI_UH_TRAIT_BURNS, 0, 0, Pacci_UhCondTorchLit },   // Torch
    { ACTOR_BG_PO_SYOKUDAI, PACCI_UH_TRAIT_BURNS, 0, 0, Pacci_UhCondTorchLit }, // Golden Torch

    // -- lined up by hand, never by walking -------------------------------------------------------
    // The carry normally keeps the face you grabbed pointed at Link, which is right for a pot and
    // wrong for anything that has to end up SQUARE with something. A block you had aligned with a
    // slot came off true the moment you took a step, and the only way to fix it was to stop
    // walking and re-aim. These keep whatever angle you set and ignore where you are standing.
    { ACTOR_OBJ_OSHIHIKI, PACCI_UH_TRAIT_NO_FACE, 0, 0, NULL }, // Pushable Block
    { ACTOR_EN_AM, PACCI_UH_TRAIT_NO_FACE, 0, 0, NULL },        // Armos Statue

    // -- blue fire -------------------------------------------------------------------------------
    // Red ice does not check damage flags. BgIceShelter_Update asks whether the thing that hit it
    // IS an En_Ice_Hono (z_bg_ice_shelter.c:343), by actor id and nothing else - so no collider we
    // could build would ever melt it, and carrying the flame itself is not a workaround, it is the
    // only door. The game already knows how to do this; it only needed the flame to be portable.
    // Blue Fire: a copy comes with you and the bowl keeps burning. The copy is the persistent
    // variant so it does not time out in your hands, and it goes on shedding ordinary params-0
    // flames as it travels, which is what red ice actually answers to.
    { ACTOR_EN_ICE_HONO, PACCI_UH_TRAIT_REACHABLE | PACCI_UH_TRAIT_PROXY, 0, 0, Pacci_UhCondPersistent, 0,
      ACTOR_EN_ICE_HONO, (s16)0xFFFF },

    // -- carried, not talked to ------------------------------------------------------------------
    // Ruto is already something the game lets you pick up and put down; Ultrahand just does it from
    // across the room. NO_FACE because she should keep the way she is facing rather than swivel to
    // look at Link as he walks - she is a person being carried, not a crate presenting a side.
    { ACTOR_EN_RU1, PACCI_UH_TRAIT_REACHABLE | PACCI_UH_TRAIT_NO_FACE, 0, 0, Pacci_UhCondRutoSitting },

    // -- cut, not carried --------------------------------------------------------------------------
    // The Jabu-Jabu tentacle you sever with the boomerang. Its bumper is 0x00000010, which is
    // DMG_BOOMERANG and nothing else (z_en_ba.c:48), it has health 4, and on the killing blow it
    // sets its own switch flag and dies (z_en_ba.c:446-447). All of that is already written; the
    // cane only has to land the hit.
    //
    // NOT En_Bx. That one is the ELECTRIFIED tentacle - same room, same silhouette, and it has no
    // death at all: no health, no damage table, and its only Actor_Kill is the flag check in Init.
    // Its flag index is discarded there too (params &= 0xFF with nothing keeping the top byte), so
    // it cannot even be marked as dealt with. En_Bx is a hazard, En_Ba is the thing you cut.
    { ACTOR_EN_BA, PACCI_UH_TRAIT_STRIKES, 0, 0, NULL, 0, 0, 0, DMG_BOOMERANG, PACCI_UH_CUT_DAMAGE },

    // -- size, not position ------------------------------------------------------------------------
    // The Water Spout. Its world.pos.y is rebuilt every frame from initPosY + currentHeight, so
    // moving it does nothing at all; and its scale.y is dead weight - the initchain sets it and
    // Draw does Matrix_Scale(1,1,1). targetHeight is the only number that means "how tall".
    { ACTOR_EN_SIOFUKI, PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_HEIGHT, 0, 0, NULL },

    // -- its own hinge, its own animation ----------------------------------------------------------
    // The Market drawbridge. Nothing here is reimplemented: BgSpot00Hanebasi_DrawbridgeRiseAndFall
    // already walks shape.rot.x toward destAngle at 80 a frame, drags both chain segments along at
    // 0.4x that rate, and plays NA_SE_EV_BRIDGE_OPEN / _CLOSE with their stop variants. The cane
    // writes destAngle and points the actor at that function; the bridge does the rest.
    { ACTOR_BG_SPOT00_HANEBASI, PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_HINGE, 0, 0, Pacci_UhCondDrawbridge },

    // -- hauled, not carried -----------------------------------------------------------------------
    // Big enough that floating them at arm's length would look ridiculous. Bg_Haka_Zou is the
    // cleanest of the three - while it is waiting it writes nothing at all, so it goes exactly
    // where it is put. Bg_Po_Event already owns a push/pull implementation of its own and this
    // simply drives the same distance by hand. Bg_Hidan_Dalm runs Actor_MoveXZGravity every frame,
    // so its own physics pulls back against you; that is the actor's character, not a bug to fix.
    { ACTOR_BG_HAKA_ZOU, PACCI_UH_TRAIT_PULLABLE, 0, 0, NULL },   // Giant bird statue
    { ACTOR_BG_PO_EVENT, PACCI_UH_TRAIT_PULLABLE, 0, 0, NULL },   // Poe sisters' block
    { ACTOR_BG_HIDAN_DALM, PACCI_UH_TRAIT_PULLABLE, 0, 0, NULL }, // Fire Temple face block
};

static const PacciUhTraitRow* Pacci_UhTraitRow(Actor* actor) {
    if (actor != NULL) {
        for (u32 i = 0; i < ARRAY_COUNT(sPacciUhTraits); i++) {
            if (sPacciUhTraits[i].actorId != actor->id) {
                continue;
            }
            // A row whose condition is false does not apply AT ALL - it is not a row with its
            // traits stripped. An unlit torch is an ordinary torch, and that is the whole answer.
            //
            // But it does not end the search either: one actor id can carry several rows, one per
            // variant, and the condition is what tells them apart. Bg_Mizu_Movebg is seven
            // different machines behind one id - two of them are welded to the water level, one
            // rebuilds itself from a private field, three are switch toggles and one follows a
            // path - and returning on the first miss meant only ever seeing the first of them.
            if ((sPacciUhTraits[i].cond != NULL) && !sPacciUhTraits[i].cond(actor)) {
                continue;
            }
            return &sPacciUhTraits[i];
        }
    }
    return NULL;
}

static u32 Pacci_UhTraits(Actor* actor) {
    const PacciUhTraitRow* row = Pacci_UhTraitRow(actor);

    return (row != NULL) ? row->traits : 0;
}

// Too big to be an object. THIS is the general answer, and the enumerated list below it is only
// for the exceptions it cannot see.
//
// I said in the plan that "too big to pick up" was not a property the actor struct exposes. That
// was wrong: a dynapoly actor's CollisionHeader carries its own model-space bounds, so the size of
// the thing is right there. A crate is about 30 units of half-extent, a gravestone 40, a pushblock
// 60, a lift platform under 200. A Forest Temple room quadrant is thousands. One threshold
// separates them, and it keeps working for every room-scale actor nobody has run into yet - which
// a list never does.
//
// Only the horizontal extents are tested. Height alone does not make something a room: a totem or
// a pillar is tall and still an object, and refusing those would cost more than it saves.
static u8 Pacci_UhTooBig(Actor* actor) {
    PlayState* play = gPlayState;
    s32 i;

    if ((play == NULL) || (actor == NULL)) {
        return 0;
    }
    // Reading the bgActors table directly rather than through Pacci_FuseGetBox: that lives in the
    // FUSION section far below this one, along with the type it returns.
    for (i = 0; i < BG_ACTOR_MAX; i++) {
        BgActor* bg = &play->colCtx.dyna.bgActors[i];
        CollisionHeader* hdr;
        f32 halfX;
        f32 halfZ;

        if ((bg->actor != actor) || (bg->colHeader == NULL)) {
            continue;
        }
        hdr = bg->colHeader;
        halfX = ((f32)hdr->maxBounds.x - (f32)hdr->minBounds.x) * 0.5f * actor->scale.x;
        halfZ = ((f32)hdr->maxBounds.z - (f32)hdr->minBounds.z) * 0.5f * actor->scale.z;
        return ((halfX > PACCI_UH_MAX_HALF) || (halfZ > PACCI_UH_MAX_HALF)) ? 1 : 0;
    }
    return 0; // no dynapoly: whatever it is, it is not a room
}

static u8 Pacci_UhIsStructure(Actor* actor) {
    return (Pacci_UhTraits(actor) & PACCI_UH_TRAIT_EXCLUDE) ? 1 : 0;
}

// Per-actor behaviour. Everything defaults to THROW; the table only lists actors
// that should do something else, which is what keeps it short and what makes it
// the place to extend when a new special case turns up.
typedef enum {
    PACCI_BEHAV_THROW = 0,  // flies, damages what it hits, breaks on impact
    PACCI_BEHAV_SNAP_FLOOR, // released onto the floor, and onto a switch if one is
                            // underneath — this is what makes a pushable block a
                            // puzzle solver instead of a thing you shove around
    PACCI_BEHAV_PLACE,      // stays exactly where released, no gravity
    PACCI_BEHAV_CARRY_ONLY, // movable, but its own logic keeps running (live bombs)
} PacciBehaviour;

typedef struct {
    s16 actorId;
    u8 behaviour;
} PacciBehaviourRow;

static const PacciBehaviourRow sPacciBehaviours[] = {
    { ACTOR_OBJ_OSHIHIKI, PACCI_BEHAV_SNAP_FLOOR }, // pushable block — reposition, do not hurl
    { ACTOR_OBJ_LIFT, PACCI_BEHAV_SNAP_FLOOR },     // platform slab
    { ACTOR_EN_BOM, PACCI_BEHAV_CARRY_ONLY },       // lit bomb: the fuse keeps burning
    // Uprooting a bomb flower takes the PLANT, so you can replant it where the
    // puzzle actually needs a bomb rather than carrying a lit one there.
    { ACTOR_EN_BOMBF, PACCI_BEHAV_PLACE },
};

u8 Pacci_BehaviourFor(Actor* actor) {
    if (actor != NULL) {
        for (u32 i = 0; i < ARRAY_COUNT(sPacciBehaviours); i++) {
            if (actor->id == sPacciBehaviours[i].actorId) {
                return sPacciBehaviours[i].behaviour;
            }
        }
    }
    return PACCI_BEHAV_THROW;
}

// BLACKLIST, not whitelist (user-locked): anything Pacci can plausibly grab is fair
// game, and only the listed exceptions are refused. The earlier whitelist plus an
// HP gate is what made Flip look broken — almost nothing in a room qualified.
// isDyna: this actor was handed to us by DynaPoly_GetActor, so it OWNS a registered collision
// surface. That is a stronger qualification than any category could be, and it is why the
// category gate below is skipped for it - see the note on the gate.
u8 Pacci_IsLiftableEx(Actor* actor, u8 isDyna) {
    if ((actor == NULL) || (actor->update == NULL)) {
        return 0;
    }
    // Two hands on one body: the Hourglass writes its transform every frame from the recorded
    // path while we would write ours from the carry, and whichever ran second would win.
    if (Rewind_IsScrubbing(actor)) {
        return 0;
    }
    if ((actor->id == ACTOR_PLAYER) || (actor->category == ACTORCAT_BOSS)) {
        return 0;
    }
    // A REACHABLE row is an explicit YES from the table, and it outranks every generic rule below
    // - which is the point of it, and why it is tested first rather than last.
    //
    // Each of the rows that uses it fails a different one of those rules, and would keep failing
    // it wherever this check sat: En_Ice_Hono is ACTORCAT_ITEMACTION with no collision of its own,
    // and En_Ru1 is an NPC, which the next rule refuses outright. A table entry is somebody having
    // thought about that specific actor; the generic rules are guesses for everything nobody has.
    if (Pacci_UhTraits(actor) & PACCI_UH_TRAIT_REACHABLE) {
        return 1;
    }

    // NPCs are the only category ruled out by being what they are. Everything else that is not
    // Link and not a boss is a physical object as far as this is concerned.
    //
    // This started as an allow-list of four categories copied from SwitchHook_CanSwap, and that
    // was wrong for Ultrahand specifically. The switch hook swaps places with a target, so props
    // and enemies really are the whole story. Ultrahand picks up THINGS, and things are scattered
    // across nearly every category depending on the scene - bombs in EXPLOSIVE, pushblocks and
    // elevators in SWITCH and BG, crates in MISC, doors in DOOR. A four-category list quietly
    // stopped detecting most of them.
    //
    // What actually keeps the junk out is not the category, it is the draw check further down:
    // dialogue triggers, song spots and spawn markers are invisible, and that is the property
    // worth testing. NPCs are excluded here instead because they are visible and physical and
    // still should not be dragged around by the neck.
    if (!(actor->flags & PACCI_FLAG_LIFTABLE) && (actor->category == ACTORCAT_NPC)) {
        return 0;
    }
    // MASS_IMMOVABLE only disqualifies ENEMIES, where it marks the heavy minibosses
    // that should not be knocked around. It must NOT be applied to props: pots,
    // crates and pushable blocks all set MASS_IMMOVABLE deliberately (Obj_Oshihiki
    // does it right in its Init), so testing it across every category silently
    // excluded the exact objects the lift exists for.
    if ((actor->category == ACTORCAT_ENEMY) && (actor->colChkInfo.mass == MASS_IMMOVABLE)) {
        return 0;
    }
    // The blacklist yields to a trait row, for the same reason the size gate does: a row is
    // somebody having thought about that specific actor, and the list is a guess about everything
    // nobody has.
    //
    // En_Am is why. It sits in the blacklist as "an enemy too heavy for the lift to be throwing
    // around", and it is also one of the five things Pacci_PlaceIsWeight pulls toward a floor
    // switch - the code was asking for an Armos and refusing one in the same breath. Iron Knuckle,
    // Dark Link, Wallmaster and the rest have no row and stay refused.
    if (Pacci_UhTraits(actor) == 0) {
        for (u32 i = 0; i < ARRAY_COUNT(sPacciBlacklist); i++) {
            if (actor->id == sPacciBlacklist[i]) {
                return 0;
            }
        }
    }
    if (Pacci_UhIsStructure(actor)) {
        return 0;
    }
    // The size gate does NOT apply to actors the table already has an opinion about: a lift is
    // large on purpose and somebody wrote down that it may be moved, along its own axis, anyway.
    if ((Pacci_UhTraits(actor) == 0) && Pacci_UhTooBig(actor)) {
        return 0;
    }
    // No draw function means nothing is rendered: talk triggers, spawn points, cutscene and
    // region markers. Their entire job is to sit invisibly and offer a conversation, so grabbing
    // one moves something the player cannot see and looks like a bug. This is the check that
    // does the real work of keeping them out.
    //
    // Except for dynapoly, where it means the opposite. A bg actor with no draw is collision-only
    // scenery - an invisible wall, a floor you stand on, a platform whose model belongs to the
    // room mesh. Those are as physical as anything gets; they are simply drawn by something else.
    if (!isDyna && (actor->draw == NULL)) {
        return 0;
    }
    if (Pacci_FindEntry(actor) != NULL) {
        return 0; // already flipped, petrified or held
    }
    // Glued parts stay VISIBLE to the targeting on purpose. Refusing them here used to
    // make a finished structure unclickable anywhere except its root, which is not how
    // you look at a thing you just built. The grab redirects to the root instead — see
    // Pacci_FuseRootOf in Pacci_CastUltrahand — so aiming at any piece takes the whole
    // assembly, models and all.
    return 1;
}

u8 Pacci_IsLiftable(Actor* actor) {
    return Pacci_IsLiftableEx(actor, 0);
}

static s32 Pacci_LiftFilter(Actor* actor) {
    return Pacci_IsLiftable(actor);
}

// Categories the lift scans, and the short leash it scans them with.
static const u8 sPacciLiftCats[3] = { ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_BG };

static Actor* Pacci_ScanLiftable(PlayState* play) {
    return TargetSelect_ScanCats(play, sPacciLiftCats, 3, Pacci_LiftFilter, PACCI_LIFT_RADIUS, TARGETSEL_DEFAULT_CONE);
}

// ---------------------------------------------------------------------------
// IMPACT BURST
// ---------------------------------------------------------------------------
// When a thrown object lands, the damage is dealt by a REAL collider instead of by
// writing health directly. Writing colChkInfo.health by hand skipped every reaction
// an actor has to being hurt: no flinch, no death effect, no drop.
//
// The burst is owned by the PLAYER, and that is the whole trick. An actor AT can
// never hit that same actor AC, so a collider owned by the thrown object could
// damage what it landed on but never itself. Owned by Link, it damages both.
// Impact damage flavours. Rocks only break to a heavy blow, so the burst switches
// between them; everything else takes an arrow-grade hit.
#define PACCI_DMG_LIGHT DMG_ARROW
#define PACCI_DMG_HEAVY DMG_HAMMER
#define PACCI_BURST_DAMAGE 8
#define PACCI_BURST_FRAMES 3
#define PACCI_BURST_RADIUS 34
#define PACCI_BURST_HEIGHT 40

static ColliderCylinder sPacciBurst;
static u8 sPacciBurstReady = 0;
static s16 sPacciBurstTimer = 0;

static ColliderCylinderInit sPacciBurstInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, PACCI_BURST_DAMAGE },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { PACCI_BURST_RADIUS, PACCI_BURST_HEIGHT, -10, { 0, 0, 0 } },
};

// `heavy` picks the damage flavour: rocks take a HAMMER hit (the only thing En_Ishi
// breaks to), everything else takes an ARROW hit.
static void Pacci_BurstSpawn(PlayState* play, Player* player, Vec3f* pos, u8 heavy) {
    if (!sPacciBurstReady) {
        Collider_InitCylinder(play, &sPacciBurst);
        sPacciBurstReady = 1;
    }
    Collider_SetCylinder(play, &sPacciBurst, &player->actor, &sPacciBurstInit);
    sPacciBurst.info.toucher.dmgFlags = heavy ? PACCI_DMG_HEAVY : PACCI_DMG_LIGHT;
    sPacciBurst.dim.pos.x = (s16)pos->x;
    sPacciBurst.dim.pos.y = (s16)pos->y;
    sPacciBurst.dim.pos.z = (s16)pos->z;
    sPacciBurstTimer = PACCI_BURST_FRAMES;
}

static void Pacci_BurstUpdate(PlayState* play) {
    if ((sPacciBurstTimer <= 0) || !sPacciBurstReady) {
        return;
    }
    sPacciBurstTimer--;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sPacciBurst.base);
}

void Pacci_HighlightLiftTarget(PlayState* play) {
    Actor* target;

    if (Pacci_IsLifting()) {
        return;
    }
    target = Pacci_ScanLiftable(play);
    if (target != NULL) {
        PACCI_TINT_FLIP(target, 4);
    }
}

// Let go of whatever is held and put its physics back the way we found them.
static void Pacci_LiftLetGo(void) {
    Actor* actor = sLift.held;

    if ((actor != NULL) && (actor->update != NULL)) {
        if (sLift.frozeEnemy) {
            actor->update = sLift.origUpdate;
            actor->flags = sLift.origFlags;
            SwitchMagnet_MakePresser(actor); // after the restore, or the flag goes back off
        }
        actor->gravity = sLift.origGravity;
        actor->minVelocityY = sLift.origMinVelocityY;
        actor->room = (s8)sLift.origRoom;
        actor->colorFilterParams = 0;
    }
    if (sLift.fx != NULL) {
        sLift.fx->actor = NULL;
        sLift.fx->mode = PACCI_FX_NONE;
        sLift.fx = NULL;
    }
    sLift.held = NULL;
    sLift.thrown = 0;
    sLift.flightTimer = 0;
    sLift.origUpdate = NULL;
    sLift.origFlags = 0;
    sLift.frozeEnemy = 0;
}

void Pacci_LiftCancel(void) {
    PacciFlipVfx_Release();
    Pacci_LiftLetGo();
}

// Grab whatever Link is aiming at. Returns 1 if something was picked up.
u8 Pacci_LiftTryGrab(PlayState* play, Player* player) {
    Actor* target;
    PacciFx* fx;

    if (sLift.held != NULL) {
        return 0; // already holding
    }
    target = Pacci_ScanLiftable(play);
    if (target == NULL) {
        return 0;
    }

    // A pool entry is taken purely for its collider: while the object is in flight
    // it needs a live AT so it damages whatever it slams into.
    fx = Pacci_TakeEntry();
    if (fx == NULL) {
        return 0;
    }
    if (!fx->colliderReady) {
        Collider_InitCylinder(play, &fx->collider);
        fx->colliderReady = 1;
    }
    Collider_SetCylinder(play, &fx->collider, target, &sPacciFallColliderInit);
    fx->actor = target;
    fx->mode = PACCI_FX_NONE; // not a flip/stone — the lift drives it directly

    sLift.held = target;
    sLift.fx = fx;
    sLift.thrown = 0;
    sLift.flightTimer = 0;
    sLift.origGravity = target->gravity;
    sLift.origMinVelocityY = target->minVelocityY;
    sLift.origRoom = target->room;
    sLift.origUpdate = NULL;
    sLift.origFlags = 0;
    sLift.frozeEnemy = 0;
    if (target->category == ACTORCAT_ENEMY) {
        sLift.origUpdate = target->update;
        sLift.origFlags = target->flags;
        sLift.frozeEnemy = 1;
        target->update = Pacci_LiftFrozenEnemyUpdate;
        target->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    }
    target->room = -1; // a held object should survive a room change
    // Something heavy enough to be worth throwing is heavy enough to stand on a floor switch. The
    // engine gates that on a flag almost nothing carries, so arm it here and let it keep it.
    SwitchMagnet_MakePresser(target);

    Audio_PlayActorSound2(target, PACCI_SFX_FLIP);
    PacciFlipVfx_StartLift(play, player, target);
    return 1;
}

// Throw it. Aimed at Link's lock-on target when he has one, otherwise straight
// ahead — which is what makes "lift this and hurl it into that" work.
void Pacci_LiftThrow(PlayState* play, Player* player) {
    Actor* actor = sLift.held;
    s16 yaw;

    if ((actor == NULL) || sLift.thrown) {
        return;
    }
    if (actor->update == NULL) {
        Pacci_LiftLetGo();
        return;
    }

    if ((player->focusActor != NULL) && (player->focusActor != actor) && (player->focusActor->update != NULL)) {
        yaw = Math_Vec3f_Yaw(&actor->world.pos, &player->focusActor->world.pos);
    } else {
        yaw = player->actor.shape.rot.y;
    }

    actor->world.rot.y = yaw;
    actor->shape.rot.y = yaw;
    actor->speedXZ = PACCI_LIFT_THROW_SPEED;
    actor->velocity.y = PACCI_LIFT_THROW_VEL_Y;
    actor->gravity = PACCI_LIFT_GRAVITY;
    actor->colorFilterParams = 0;

    sLift.thrown = 1;
    sLift.flightTimer = PACCI_LIFT_FLIGHT_FRAMES;
    PacciFlipVfx_Release();
    Audio_PlayActorSound2(actor, PACCI_SFX_FLIP);
}

// Runs every frame the cane is equipped: holds the object aloft, then flies it.
void Pacci_LiftUpdate(PlayState* play, Player* player) {
    Actor* actor = sLift.held;

    // The impact burst outlives the throw by a few frames, so it ticks before the
    // "nothing held" early-out below.
    Pacci_BurstUpdate(play);

    if (actor == NULL) {
        return;
    }
    if (actor->update == NULL) { // it died in our hands
        Pacci_LiftLetGo();
        return;
    }

    if (!sLift.thrown) {
        // Float it in front of and above Link, following his facing.
        f32 targetX = player->actor.world.pos.x + (Math_SinS(player->actor.shape.rot.y) * PACCI_LIFT_DIST);
        f32 targetZ = player->actor.world.pos.z + (Math_CosS(player->actor.shape.rot.y) * PACCI_LIFT_DIST);
        f32 targetY = player->actor.world.pos.y + PACCI_LIFT_HEIGHT;
        f32 oldW = PACCI_LIFT_FOLLOW;
        f32 newW = 1.0f - oldW;

        actor->world.pos.x = (actor->world.pos.x * oldW) + (targetX * newW);
        actor->world.pos.y = (actor->world.pos.y * oldW) + (targetY * newW);
        actor->world.pos.z = (actor->world.pos.z * oldW) + (targetZ * newW);
        actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;
        actor->speedXZ = 0.0f;
        actor->gravity = 0.0f;
        // Slow spin so a held object reads as "under your control", not stuck.
        actor->shape.rot.y += 0x400;
        PACCI_TINT_FLIP(actor, 8);
        return;
    }

    // In flight: fly it, and keep the AT live so it damages what it reaches.
    //
    // On the way down a heavy body also leans toward any floor switch it could press, so a throw
    // aimed roughly at one lands on it. Shared with Stasis; see switch_magnet.c.
    SwitchMagnet_Steer(play, actor);
    Actor_MoveXZGravity(actor);
    Actor_UpdateBgCheckInfo(play, actor, 5.0f, 15.0f, 0.0f, 0x85);
    // ...and once it is directly over one, it drops onto it square.
    SwitchMagnet_SnapOnto(play, actor, 1);

    if (sLift.fx != NULL && sLift.fx->colliderReady) {
        Collider_UpdateCylinder(actor, &sLift.fx->collider);
        CollisionCheck_SetAT(play, &play->colChkCtx, &sLift.fx->collider.base);
    }

    if (sLift.flightTimer > 0) {
        sLift.flightTimer--;
    }

    // Landed, hit a wall, connected with something, or ran out of flight time.
    if ((actor->bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_WALL)) || (sLift.flightTimer <= 0) ||
        ((sLift.fx != NULL) && (sLift.fx->collider.base.atFlags & AT_HIT))) {
        // Real damage, dealt by a real collider, to the thrown object AND to
        // whatever it slammed into. Let go FIRST, so the object is back under its
        // own update with its own AC live by the time the burst resolves.
        Vec3f impact = actor->world.pos;
        u8 heavy = (actor->id == ACTOR_EN_ISHI); // rocks only break to a hammer

        Audio_PlayActorSound2(actor, PACCI_SFX_FLIP_LAND);
        Pacci_LiftLetGo();
        Pacci_BurstSpawn(play, player, &impact, heavy);
    }
}

// ============================================================================
// STONE
// ============================================================================

// En_Ishi's small-rock shatter, reproduced 1:1 (same effect, gravity, life,
// object and display list) so a petrified enemy breaks like a real rock.
static void Pacci_StoneShatter(Actor* actor, PlayState* play) {
    Vec3f pos;
    Vec3f velocity;
    static const s16 sDebrisScales[] = { 12, 10, 10, 8, 8, 6 };

    for (u8 i = 0; i < ARRAY_COUNT(sDebrisScales); i++) {
        pos.x = ((Rand_ZeroOne() - 0.5f) * 8.0f) + actor->world.pos.x;
        pos.y = (Rand_ZeroOne() * 5.0f) + actor->world.pos.y + 5.0f;
        pos.z = ((Rand_ZeroOne() - 0.5f) * 8.0f) + actor->world.pos.z;

        Math_Vec3f_Copy(&velocity, &actor->velocity);
        if (actor->bgCheckFlags & BGCHECKFLAG_GROUND) {
            velocity.x *= 0.6f;
            velocity.y *= -0.3f;
            velocity.z *= 0.6f;
        } else if (actor->bgCheckFlags & BGCHECKFLAG_WALL) {
            velocity.x *= -0.5f;
            velocity.y *= 0.5f;
            velocity.z *= -0.5f;
        }
        velocity.x += (Rand_ZeroOne() - 0.5f) * 11.0f;
        velocity.y += (Rand_ZeroOne() * 7.0f) + 6.0f;
        velocity.z += (Rand_ZeroOne() - 0.5f) * 11.0f;

        EffectSsKakera_Spawn(play, &pos, &velocity, &pos, -420, ((s32)Rand_Next() > 0) ? 65 : 33, 30, 5, 0,
                             // -1 is KAKERA_COLOR_NONE (z_eff_ss_kakera.h). Spelled as the literal so
                             // this file does not have to pull an overlay header into the z_player TU;
                             // En_Ishi in MM passes the same -1 inline.
                             sDebrisScales[i], 3, 10, 40, -1, OBJECT_GAMEPLAY_FIELD_KEEP, gFieldKakeraDL);
    }

    Math_Vec3f_Copy(&pos, &actor->world.pos);
    func_80033480(play, &pos, 60.0f, 3, 0x50, 0x3C, 1);

    Audio_PlaySoundGeneral(NA_SE_EV_ROCK_BROKEN, &actor->projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    Item_DropCollectibleRandom(play, NULL, &actor->world.pos, 0x30);
}

static void Pacci_StoneUpdate(Actor* thisx, PlayState* play) {
    PacciFx* fx = Pacci_FindEntry(thisx);
    Player* player = GET_PLAYER(play);

    if (fx == NULL || player == NULL) {
        return;
    }

    // Keep it drained of colour — the filter is a countdown, so it has to be re-armed.
    // NOTE: OoT's colour filter has no GRAY flag (only white 0x8000, red 0x4000, and
    // blue when neither is set), so the stone wash here is WHITE at full intensity.
    // MM uses its native COLORFILTER_COLORFLAG_GRAY for the same effect.
    Actor_SetColorFilter(thisx, 0x8000, 255, 0, 20);

    switch (fx->phase) {
        case PACCI_STONE_PHASE_IDLE:
            if (Actor_HasParent(thisx, play)) {
                fx->phase = PACCI_STONE_PHASE_HELD;
                thisx->room = -1;
                break;
            }
            if (thisx->bgCheckFlags & BGCHECKFLAG_GROUND) {
                Math_StepToF(&thisx->speedXZ, 0.0f, 1.0f);
                Actor_OfferCarry(thisx, play);
            } else {
                Math_StepToF(&thisx->speedXZ, 0.0f, 0.2f);
            }
            Actor_MoveXZGravity(thisx);
            Actor_UpdateBgCheckInfo(play, thisx, 30.0f, 20.0f, 0.0f, 0x1D);
            break;

        case PACCI_STONE_PHASE_HELD:
            if (Actor_HasNoParent(thisx, play)) {
                fx->phase = PACCI_STONE_PHASE_THROWN;
                thisx->velocity.y = PACCI_STONE_THROW_VEL_Y;
                thisx->speedXZ = PACCI_STONE_THROW_SPEED;
                thisx->world.rot.y = player->actor.shape.rot.y;
            }
            break;

        case PACCI_STONE_PHASE_THROWN:
            Actor_MoveXZGravity(thisx);
            Actor_UpdateBgCheckInfo(play, thisx, 30.0f, 20.0f, 0.0f, 0x1D);
            // A thrown rock shatters on the first solid thing it meets.
            if ((thisx->bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_WALL)) &&
                ((thisx->speedXZ > PACCI_STONE_BREAK_SPEED) || (thisx->velocity.y < -PACCI_STONE_BREAK_SPEED))) {
                Pacci_StoneShatter(thisx, play);
                fx->actor = NULL;
                fx->mode = PACCI_FX_NONE;
                Actor_Kill(thisx);
                return;
            }
            break;
    }

    thisx->focus.pos = thisx->world.pos;
}

// A petrified enemy keeps its own skeleton draw (frozen in its last pose); the
// grey comes from the colour filter, which the engine applies around that draw.
u8 Pacci_CastStone(PlayState* play, Player* player) {
    Actor* target = Pacci_ScanEnemy(play);
    PacciFx* fx;

    if (target == NULL) {
        return 0;
    }
    fx = Pacci_TakeEntry();
    if (fx == NULL) {
        return 0;
    }

    // The shatter debris lives in gameplay_field_keep; ask for it now so it is
    // resident by the time the rock actually breaks.
    if (Object_GetIndex(&play->objectCtx, OBJECT_GAMEPLAY_FIELD_KEEP) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_GAMEPLAY_FIELD_KEEP);
    }

    fx->actor = target;
    fx->mode = PACCI_FX_STONE;
    fx->phase = PACCI_STONE_PHASE_IDLE;
    fx->timer = 0;
    fx->origUpdate = target->update;
    fx->origDraw = target->draw;
    fx->origFlags = target->flags;
    fx->origGravity = target->gravity;
    fx->origMinVelocityY = target->minVelocityY;
    fx->origSpeed = target->speedXZ;
    fx->origShapeRot = target->shape.rot;
    fx->origWorldRot = target->world.rot;
    fx->origRoom = target->room;
    fx->origMass = target->colChkInfo.mass;

    target->update = Pacci_StoneUpdate;
    target->gravity = PACCI_STONE_GRAVITY;
    target->minVelocityY = PACCI_STONE_MIN_VEL_Y;
    target->speedXZ = 0.0f;
    target->colChkInfo.mass = MASS_HEAVY;
    // A rock presses switches and can no longer be locked on to as an enemy.
    target->flags |= ACTOR_FLAG_CAN_PRESS_SWITCHES;
    target->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    // Same reason as Flip: our update is the only thing moving it now.
    target->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;

    Actor_SetColorFilter(target, 0x8000, 255, 0, 20); // white wash — OoT has no gray flag
    Audio_PlayActorSound2(target, NA_SE_EV_STONE_STATUE_OPEN);
    return 1;
}

// Ultrahand mode state. Declared up here because Pacci_UpdateUltrahand — further
// down — reads the height offset the mode drives, while the mode's own section
// sits below it.
typedef struct {
    u8 active;     // in the mode at all
    f32 heightOff; // vertical offset from the aim line, driven by R + D-up/down
    f32 sideOff;   // sideways offset, perpendicular to the aim, from R + D-left/right
    u8 prevDpad;   // our own edge detection; see the note in the cane's L/R cycler
    // Detach is a fast left-right shake of the stick, standing in for TotK's right-stick
    // wiggle. Counting DIRECTION FLIPS rather than raw deflection is what separates a shake
    // from simply running sideways, which the stick is also still doing.
    s8 wiggleDir;
    u8 wiggleFlips;
    s16 wiggleTimer;
    s16 summonHold; // frames the cane's C button has been held, for the recall
} PacciUhMode;

static PacciUhMode sUhMode = { 0, 0.0f, 0 };

// ============================================================================
// ULTRAHAND
// ============================================================================

typedef struct {
    Actor* held;
    u8 dropping;
    s16 dropTimer;
    f32 distance;
    f32 origGravity;
    f32 origMinVelocityY;
    s16 origRoom;
    // The chosen pose stays fixed relative to the radial line from the object to
    // Link. Turning/moving Link therefore carries the object around him and keeps
    // the same face presented to the player; only the explicit rotation controls
    // alter that pose.
    //
    // baseRot is the orientation YOU chose (what it had on grab, plus every D-pad
    // nudge since). faceOffsetYaw records which face was presented to Link. Each
    // frame combines that radial facing with the manual delta, keeping turning and
    // adjustment independent.
    Vec3s baseRot;
    Vec3s grabRot; // orientation at the moment of the grab, for the Z reset
    // world.rot AS IT WAS, kept so it can be handed back intact.
    //
    // For a lot of actors world.rot is not a pose at all, it is a HEADING. Bg_Haka_Ship sets
    // world.rot.y = shape.rot.y - 0x4000 in its Init and then sails along it; Bg_Hidan_Rock and
    // Bg_Hidan_Sima build their x/z out of Math_SinS(world.rot.y); Bg_Jya_Kanaami animates its
    // whole fall in world.rot.x. The carry writes shape.rot into world.rot every frame, which for
    // those is not a small liberty - the ferry left the dock 90 degrees off course and never came
    // back, and the grate's fall froze mid-air. So it is saved here and restored on release.
    Vec3s grabWorldRot;
    s16 faceOffsetYaw; // grabbed facing minus object->player yaw
    // The yaw the CARRY POINT orbits at, which is NOT Link's aim: it chases it at a capped
    // angular rate. Building the hold position straight from focus.rot.y meant a fast turn
    // moved the anchor to the far side instantly, and the positional lerp then walked the
    // object there along the CHORD — straight through Link. Stepping the angle instead makes
    // it sweep the arc at constant radius, so it goes around him however hard you spin.
    s16 carryYaw;
    // How fast the body was actually travelling while carried, smoothed. Handed over as
    // velocity when you let go, so a release inherits the motion you gave it.
    Vec3f carryVel;
    // Constraints for the body in hand, resolved once at the grab. railPos is the pose the
    // constraint is measured against - the XZ a lift may not leave, the Y an ice block may not
    // leave - and it is the position AT THE GRAB, not the one the scene spawned it at, so a
    // block you already slid keeps the plane it is on.
    u32 traits; // u32 for the same reason the row's is - see PacciUhTraitRow
    s8 flagDir; // SETS_FLAG: which way the body has to be moved. See PacciUhTraitRow.
    Vec3f railPos;
    s32 pathId;
    s32 pathCount;
    // Where the CONTROLS want the body, before any placement magnet moves it. The offer has to be
    // measured against this and not against where the body actually ended up, or a block that has
    // already slid onto a switch keeps answering "I am right on top of it" and there is no way to
    // pull it back off.
    Vec3f anchorPos;
    // 0 = the controls own the body, 1 = the offered spot does. Ramps rather than snapping, so
    // the slide is something you watch happen and can change your mind about halfway through.
    f32 placeBlend;
    s16 vfxAge; // drives the short acquisition/reach animation
    // Held objects are frozen: their own update is what submits their OC collider,
    // and with it live the object shoves Link around while he is carrying it.
    ActorFunc origUpdate;
} PacciUltrahand;

static PacciUltrahand sUltrahand = { 0 };
// The stand-in a PROXY grab is carrying, if any. Declared up here with the carry rather than
// with the proxy code far below, because Pacci_UltrahandLetGo has to know about it and runs
// long before that block.
static Actor* sUhProxy = NULL;
static Actor* sUhHighlightTarget = NULL;

// z_player_lib.c — re-latch Link's model group. The group is sampled when the item
// ACTION changes and cached in nextModelGroup, so changing what
// ExtPlayer_GetActionModelGroup returns mid-hold has no effect on its own: nothing
// asks again. Grabbing and releasing have to ask for it explicitly.
s32 Player_ActionToModelGroup(Player* this, s32 actionParam);
void Player_SetModels(Player* this, s32 modelGroup);

// -- the tint ------------------------------------------------------------------
// Zonai green, on the actor's own model. NOT Actor_SetColorFilter - that offers white, red
// and blue and nothing else, which is why every attempt to get green out of it produced
// either a petrified-looking wash or the wrong colour entirely.
//
// LUS exposes grayscale as RSP STATE (G_SETGRAYSCALE), separate from the combiner: the
// texture is desaturated and multiplied by a colour. Because it is state and not a combiner
// setting, the actor's own material setup cannot clobber it halfway through its display
// list the way a prim or env colour would - which is the whole reason this works on an
// arbitrary actor whose draw we do not control. It is the same mechanism the ports already
// use to recolour rupees (z_en_item00.c) and to grey out kaleido cells.
//
// Applied by swapping actor->draw for a wrapper, the same trick the carry already plays on
// actor->update. The table is rebuilt from scratch every frame from whatever should be lit
// right now, so anything that stops being a target has its own draw back on the next one.
#define PACCI_UH_TINT_SLOTS 8
// Bright, slightly yellow-green: grayscale x colour keeps the model's own light and shade,
// so the multiplier has to be bright or the object just goes dark green. MIX is the blend
// against the untinted texture - 255 is fully recoloured.
#define PACCI_UH_TINT_R 110
#define PACCI_UH_TINT_G 255
#define PACCI_UH_TINT_B 165
#define PACCI_UH_TINT_MIX 255

// The tether, layered so a flat untextured tube reads as light: a wide soft halo, a mid
// body, and a thin near-white core on top. Same idea as the reference - the bright streaks
// are almost white and it is the haze around them that carries the colour.
#define PACCI_UH_FLOW_HALO_R 30
#define PACCI_UH_FLOW_HALO_G 235
#define PACCI_UH_FLOW_HALO_B 165
#define PACCI_UH_FLOW_MID_R 120
#define PACCI_UH_FLOW_MID_G 255
#define PACCI_UH_FLOW_MID_B 200
#define PACCI_UH_FLOW_CORE_R 225
#define PACCI_UH_FLOW_CORE_G 255
#define PACCI_UH_FLOW_CORE_B 240

static struct {
    Actor* actor;
    ActorFunc origDraw;
} sUhTint[PACCI_UH_TINT_SLOTS];
static u8 sUhTintCount = 0;

static ActorFunc Pacci_UhTintFind(Actor* actor) {
    for (u8 i = 0; i < sUhTintCount; i++) {
        if (sUhTint[i].actor == actor) {
            return sUhTint[i].origDraw;
        }
    }
    return NULL;
}

static void Pacci_UhTintedDraw(Actor* thisx, PlayState* play) {
    ActorFunc orig = Pacci_UhTintFind(thisx);

    if (orig == NULL) {
        return; // dropped from the table between the swap and the draw pass
    }

    OPEN_DISPS(play->state.gfxCtx);
    // Both buffers. Which one an actor draws into is its own business and plenty use both,
    // so setting it on one and not the other tints half a model.
    gDPSetGrayscaleColor(POLY_OPA_DISP++, PACCI_UH_TINT_R, PACCI_UH_TINT_G, PACCI_UH_TINT_B, PACCI_UH_TINT_MIX);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, PACCI_UH_TINT_R, PACCI_UH_TINT_G, PACCI_UH_TINT_B, PACCI_UH_TINT_MIX);
    gSPGrayscale(POLY_XLU_DISP++, true);
    CLOSE_DISPS(play->state.gfxCtx);

    orig(thisx, play);

    // Turn it off again, or every actor drawn after this one in the same pass comes out green.
    OPEN_DISPS(play->state.gfxCtx);
    gSPGrayscale(POLY_OPA_DISP++, false);
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Hand every tinted actor its own draw back.
static void Pacci_UhTintClear(void) {
    for (u8 i = 0; i < sUhTintCount; i++) {
        Actor* actor = sUhTint[i].actor;

        // update == NULL means it died while tinted, and its draw pointer died with it.
        if ((actor != NULL) && (actor->update != NULL) && (actor->draw == Pacci_UhTintedDraw)) {
            actor->draw = sUhTint[i].origDraw;
        }
        sUhTint[i].actor = NULL;
        sUhTint[i].origDraw = NULL;
    }
    sUhTintCount = 0;
}

// Light this actor for THIS frame. Safe to call more than once on the same actor.
static void Pacci_UhTintAdd(Actor* actor) {
    if ((actor == NULL) || (actor->update == NULL) || (actor->draw == NULL) || (actor->draw == Pacci_UhTintedDraw) ||
        (sUhTintCount >= PACCI_UH_TINT_SLOTS)) {
        return;
    }
    sUhTint[sUhTintCount].actor = actor;
    sUhTint[sUhTintCount].origDraw = actor->draw;
    sUhTintCount++;
    actor->draw = Pacci_UhTintedDraw;
}

// Ultrahand borrows PLAYER_MODELGROUP_HOOKSHOT for its extended-arm pose, and that group also
// carries the hookshot itself, drawn in a hand that is supposed to be empty. Hiding it is NOT done
// from here: the right hand's DL table and its type are chosen together in the draw, and writing
// the type alone leaves them out of step — see ItemEquip_HoldsEmptyHand.
static void Pacci_RefreshPlayerPose(Player* player) {
    if (player != NULL) {
        Player_SetModels(player, Player_ActionToModelGroup(player, player->itemAction));
    }
}

// -- collider anchoring --------------------------------------------------------
// Colliders hold WORLD-SPACE geometry that only the owning actor refreshes, and plenty of actors
// build theirs ONCE in Init and never touch it again - Obj_Bombiwa is the clearest case. Write
// world.pos on one of those and the model moves while the hitbox stays behind.
//
// A one-shot shift by the frame's delta is NOT enough, and item_switchhook.c already learned why:
// it is incremental, so anything that makes it miss a frame - the collider not being in the
// frame's lists yet, an owner that half-maintains its own collider, s16 rounding - is an error
// that never comes back. So this is the switch hook's answer instead: each collider's reference
// point is captured as an OFFSET from its owner's resting position, and then forced onto
// `owner world.pos + offset` every frame. Absolute, therefore idempotent, therefore self-healing.
//
// And it runs from inside the OWNER'S OWN update, straight after that update returns. That timing
// is the other half of the fix. CollisionCheck_ClearContext runs at the very END of
// Actor_UpdateAll (z_actor.c), so a collider is only in the frame's lists from the moment its
// actor submits it onward - and props update AFTER the player. Collecting from the cane's code,
// which runs inside the player's update, could never see a prop's collider at all.
// Root + every part, with a slot to spare. Written out rather than derived from
// PACCI_FUSE_MAX_PARTS because that define belongs to the FUSION section hundreds of lines
// below this one, and a macro is not visible before the line that defines it. The static
// assert keeps the two honest if either ever moves.
#define PACCI_ANCHOR_OWNERS 8
#define PACCI_ANCHOR_COLS 10 // colliders tracked per actor; extras are simply left alone

typedef struct {
    Collider* col;
    Vec3f offset;  // reference point, relative to the owner's RESTING position
    Vec3f lastSet; // where we last left that reference point
    u8 hasLastSet;
} PacciAnchor;

typedef struct {
    Actor* owner;
    // The pose the offsets describe. Recorded at the grab, NOT when a collider is first found:
    // collection can take a frame or two to succeed, and by then the actor has already been
    // moved. The collider has not - that is the whole bug - so measuring against where the actor
    // WAS is what makes a late capture come out right.
    Vec3f basePos;
    u8 active;
    u8 count;
    PacciAnchor cols[PACCI_ANCHOR_COLS];
} PacciAnchorSlot;

static PacciAnchorSlot sUhAnchor[PACCI_ANCHOR_OWNERS];

extern void SwitchHook_ShiftCollider(Collider* col, Vec3f* delta);
extern s32 SwitchHook_GetColliderRefPos(Collider* col, Vec3f* out);

static PacciAnchorSlot* Pacci_AnchorFind(Actor* actor) {
    for (u8 i = 0; i < PACCI_ANCHOR_OWNERS; i++) {
        if (sUhAnchor[i].active && (sUhAnchor[i].owner == actor)) {
            return &sUhAnchor[i];
        }
    }
    return NULL;
}

// Start tracking. Call at the moment the actor is taken, while it is still at rest.
static void Pacci_AnchorTake(Actor* actor) {
    PacciAnchorSlot* slot;

    if (actor == NULL) {
        return;
    }
    slot = Pacci_AnchorFind(actor);
    if (slot == NULL) {
        for (u8 i = 0; i < PACCI_ANCHOR_OWNERS; i++) {
            if (!sUhAnchor[i].active) {
                slot = &sUhAnchor[i];
                break;
            }
        }
    }
    if (slot == NULL) {
        return;
    }
    slot->owner = actor;
    slot->active = 1;
    slot->count = 0;
    slot->basePos = actor->world.pos;
}

static void Pacci_AnchorRelease(Actor* actor) {
    PacciAnchorSlot* slot = Pacci_AnchorFind(actor);

    if (slot != NULL) {
        slot->active = 0;
        slot->owner = NULL;
        slot->count = 0;
    }
}

static void Pacci_AnchorClear(void) {
    for (u8 i = 0; i < PACCI_ANCHOR_OWNERS; i++) {
        sUhAnchor[i].active = 0;
        sUhAnchor[i].owner = NULL;
        sUhAnchor[i].count = 0;
    }
}

static void Pacci_AnchorCollect(PacciAnchorSlot* slot, Collider** list, s32 count) {
    for (s32 i = 0; (i < count) && (slot->count < PACCI_ANCHOR_COLS); i++) {
        Collider* col = list[i];
        Vec3f refPos;
        u8 seen = 0;

        if ((col == NULL) || (col->actor != slot->owner)) {
            continue;
        }
        // One collider can be registered as AT and AC and OC in the same frame - track it once.
        for (u8 j = 0; j < slot->count; j++) {
            if (slot->cols[j].col == col) {
                seen = 1;
                break;
            }
        }
        if (seen || !SwitchHook_GetColliderRefPos(col, &refPos)) {
            continue;
        }
        slot->cols[slot->count].col = col;
        slot->cols[slot->count].offset.x = refPos.x - slot->basePos.x;
        slot->cols[slot->count].offset.y = refPos.y - slot->basePos.y;
        slot->cols[slot->count].offset.z = refPos.z - slot->basePos.z;
        slot->cols[slot->count].hasLastSet = 0;
        slot->count++;
    }
}

// Collect anything newly submitted, then force every tracked collider onto the owner.
// Call from inside the owner's own update, right after its own update has run.
static void Pacci_AnchorSync(PlayState* play, Actor* actor) {
    PacciAnchorSlot* slot;

    if ((play == NULL) || (actor == NULL) || (actor->update == NULL)) {
        return;
    }
    slot = Pacci_AnchorFind(actor);
    if (slot == NULL) {
        return;
    }
    Pacci_AnchorCollect(slot, play->colChkCtx.colAT, play->colChkCtx.colATCount);
    Pacci_AnchorCollect(slot, play->colChkCtx.colAC, play->colChkCtx.colACCount);
    Pacci_AnchorCollect(slot, play->colChkCtx.colOC, play->colChkCtx.colOCCount);

    for (u8 i = 0; i < slot->count; i++) {
        PacciAnchor* anchor = &slot->cols[i];
        Vec3f refPos;
        Vec3f delta;

        if (!SwitchHook_GetColliderRefPos(anchor->col, &refPos)) {
            continue;
        }
        // The owner rebuilt this collider itself since our last pass - its answer wins. Re-derive
        // the offset from it so we stay in step instead of fighting an actor that is already
        // doing the right thing. This is also what heals a first capture taken from a stale pose.
        if (anchor->hasLastSet &&
            ((refPos.x != anchor->lastSet.x) || (refPos.y != anchor->lastSet.y) || (refPos.z != anchor->lastSet.z))) {
            anchor->offset.x = refPos.x - actor->world.pos.x;
            anchor->offset.y = refPos.y - actor->world.pos.y;
            anchor->offset.z = refPos.z - actor->world.pos.z;
            anchor->lastSet = refPos;
            continue;
        }
        delta.x = (actor->world.pos.x + anchor->offset.x) - refPos.x;
        delta.y = (actor->world.pos.y + anchor->offset.y) - refPos.y;
        delta.z = (actor->world.pos.z + anchor->offset.z) - refPos.z;
        SwitchHook_ShiftCollider(anchor->col, &delta);
        // Record where it ACTUALLY ended up, not where we aimed: the s16 shapes round, and
        // comparing against the un-rounded ideal would read as "the owner moved it" every frame.
        if (SwitchHook_GetColliderRefPos(anchor->col, &anchor->lastSet)) {
            anchor->hasLastSet = 1;
        }
    }
}

// An actor we drive must never believe it is off screen.
//
// ACTOR_FLAG_UPDATE_CULLING_DISABLED and ACTOR_FLAG_INSIDE_CULLING_VOLUME are NOT the same
// question, and z_actor.c treats them separately: the culling check sets or clears
// INSIDE_CULLING_VOLUME every frame on its own, and the update then runs if EITHER flag is set
// (z_actor.c:2798). So the flag we add to keep a piece being driven off screen has a side
// effect - the actor now RUNS while the engine is telling it, through the other flag, that it
// is not visible.
//
// Plenty of actors read that as "despawn me". En_Wood02 is the one that showed it up:
// z_en_wood02.c:334-346, a tree spawned as part of a group calls Actor_Kill on itself the
// moment INSIDE_CULLING_VOLUME goes away. Frozen, it never got that far; driven by us, it does,
// and then it dies mid-assembly. That is the whole bug - the piece stops following because it is
// dead, and its collider vanishes from the world because a dead actor submits nothing.
//
// Forcing the flag on, from inside the actor's own update and BEFORE its logic runs, closes it
// for every actor with that pattern rather than just for trees.
static void Pacci_UhKeepOnScreen(Actor* actor) {
    if (actor != NULL) {
        actor->flags |= ACTOR_FLAG_INSIDE_CULLING_VOLUME;
    }
}

// The held object runs its OWN update, and is then put straight back where the carry left it.
//
// It used to be an empty function. That froze the actor, which was tidy, but it also meant the
// actor never submitted a collider - and a collider that is never submitted is one
// SwitchHook_ShiftActorColliders can never find, because that walks the frame's collision lists.
// So the hitbox of anything carried stayed at the spot it was grabbed from, and stayed there
// after it was dropped. Letting the update run puts the collider back in the lists where the
// shift can reach it, and costs nothing else: the transform is overwritten immediately.
static void Pacci_UltrahandHeldUpdate(Actor* thisx, PlayState* play) {
    Vec3f pos = thisx->world.pos;
    Vec3s rot = thisx->shape.rot;
    Vec3s worldRot = thisx->world.rot;

    Pacci_UhKeepOnScreen(thisx);
    if (sUltrahand.origUpdate != NULL) {
        sUltrahand.origUpdate(thisx, play);
    }
    // Its own update may have killed it, and a dead actor must not be written to.
    if (thisx->update == NULL) {
        return;
    }
    // Whatever it did to its own position this frame is not motion the carry agreed to.
    thisx->world.pos = pos;
    thisx->shape.rot = rot;
    thisx->world.rot = worldRot; // what it had, not what shape.rot says - see grabWorldRot
    // ONLY while carried. This wrapper stays installed through the whole fall - it is
    // Pacci_UltrahandLetGo that removes it, and that only runs on landing - so zeroing the
    // velocity unconditionally reset the fall to a standstill every single frame. The body
    // descended by exactly one frame of gravity, over and over, never accumulating any: a
    // constant crawl instead of an acceleration, which is the "cae muy lento".
    if (!sUltrahand.dropping) {
        thisx->velocity.x = 0.0f;
        thisx->velocity.y = 0.0f;
        thisx->velocity.z = 0.0f;
    }
    // Right here, and nowhere else: its collider was submitted a few lines ago by the update
    // above, so this is the one moment in the frame it can be found.
    Pacci_AnchorSync(play, thisx);
}

// Live with the rest of the VFX and the fire, far below; the release path has to be able to put
// both out and it runs long before either is defined.
static void Pacci_UhLightOff(PlayState* play);
static void Pacci_UhFireTick(PlayState* play, Actor* actor);
static void Pacci_UhFireOff(void);
static void Pacci_UhIceTick(PlayState* play, Actor* actor);
static void Pacci_UhPlaceMagnet(Actor* actor);
static void Pacci_UhFlagTick(PlayState* play, Actor* actor);
static void Pacci_UhLockedInput(PlayState* play, u8 edge);
static void Pacci_UhLockedPose(PlayState* play, Actor* actor);
static void Pacci_UhHeightInput(Actor* actor, u8 edge);
static u8 Pacci_UhAiming(Player* player, Actor* actor);
static void Pacci_UhHingeInput(Actor* actor, u8 edge);
// z_bg_spot00_hanebasi.c, not static and not in any header. The drawbridge's own raise/lower.
void BgSpot00Hanebasi_DrawbridgeRiseAndFall(BgSpot00Hanebasi* this, PlayState* play);
// Not in functions.h - it is Player's own. equip_champion.c reaches for it the same way.
extern int Player_IsZTargeting(Player* this);
static void Pacci_UhBombTick(Actor* actor);
static u8 Pacci_UhBombDetonate(PlayState* play);
// The passenger lives with the placement code, well below the grab that starts it.
// Pacci_BackRiderDrop needs no forward declaration - it is public and cane_pacci.h is already in.
static void Pacci_BackRiderTake(PlayState* play, Player* player, Actor* rider);

u8 Pacci_IsHoldingUltrahand(void) {
    return (sUltrahand.held != NULL) && !sUltrahand.dropping;
}

/** What Ultrahand has in hand, for the items that must not touch the same body. */
Actor* Pacci_GetUltrahandHeld(void) {
    return sUltrahand.held;
}

static void Pacci_UltrahandLetGo(void) {
    Actor* actor = sUltrahand.held;

    if (actor != NULL && actor->update != NULL) {
        if (sUltrahand.origUpdate != NULL) {
            actor->update = sUltrahand.origUpdate;
        }
        actor->gravity = sUltrahand.origGravity;
        actor->minVelocityY = sUltrahand.origMinVelocityY;
        actor->velocity.y = 0.0f;
        actor->room = (s8)sUltrahand.origRoom;
        actor->colorFilterParams = 0;
        actor->world.rot = sUltrahand.grabWorldRot; // its heading back, see grabWorldRot
    }
    // The assembly is NOT un-fused here, and that was a mistake worth writing down. Releasing
    // the parts on landing gave each one its update and gravity back - and then they hung in the
    // air anyway, because the actors this system exists for (boulders, gravestones, blocks) have
    // no gravity of their own: nothing in their update ever moves them. What the release
    // actually did was take away the two things that WERE working, the root driving their
    // position and the anchor holding their colliders on them.
    //
    // So a set-down structure stays a structure: still driven, still anchored, still one thing
    // you can pick back up. What falls is the assembly, as a body - see Pacci_UhGroundUnder.
    Pacci_AnchorRelease(actor);
    // A stand-in only exists for as long as it is being carried. Killed AFTER its update and flags
    // are back, so it dies as itself rather than as something we are halfway through rewiring.
    if ((actor != NULL) && (actor == sUhProxy)) {
        if (actor->update != NULL) {
            Actor_Kill(actor);
        }
        sUhProxy = NULL;
    }
    sUltrahand.held = NULL;
    sUltrahand.dropping = 0;
    sUltrahand.dropTimer = 0;
    sUltrahand.origUpdate = NULL;
    sUltrahand.vfxAge = 0;
    sUhHighlightTarget = NULL;
    Pacci_UhFireOff();
    Pacci_UhLightOff(gPlayState); // the draw hook stops running the moment nothing is held
    Pacci_UhTintClear();
    Pacci_RefreshPlayerPose(GET_PLAYER(gPlayState)); // hookshot hold -> empty-handed
}

// Drive a fall in progress no matter what the player is holding, or whether he is holding
// anything at all. Called every frame from CustomItems_Update, which is the only place in this
// mod that keeps running after the cane leaves Link's hand.
//
// Without it a release was only animated while the cane was still out and the mode still up,
// and every other way of letting go - B out of the mode, drawing the sword, reaching for
// another item - dropped the object where it floated. A structure left hanging in the air with
// live collision is the "rompe colliders" case: its surfaces stay registered at a height
// nothing can reach, and Link walks into them.
void Pacci_UltrahandDropTick(PlayState* play) {
    if ((play == NULL) || (sUltrahand.held == NULL) || !sUltrahand.dropping) {
        return;
    }
    Pacci_UpdateUltrahand(play, GET_PLAYER(play));
}

// Is Ultrahand the cane's selected skill right now? Set by the cane every frame it is in hand.
//
// A static rather than a call into the cane's own state, because this file is #included INTO
// item_cane_of_somaria.c above the point where the skill accessors are defined - asking from
// here would be asking a question the translation unit cannot answer yet.
//
// It is what separates ARMED from IN THE MODE. Armed is the resting state: the arm is out, the
// aim marks what it would take, and a weld is offered when one is on. The mode is only what C
// opens on top of that to get the D-pad controls.
static u8 sUhArmed = 0;

void Pacci_SetUltrahandArmed(u8 armed) {
    // The cane calls this once per frame, before anything has decided what to light, so this is
    // also where the tint table is wiped outside the mode. It has to be a single point: the
    // held object, the aim candidate and both ends of a weld offer are registered from three
    // different places later in the frame, and whichever of them clears would wipe the others.
    Pacci_UhTintClear();
    sUhArmed = armed;
}

u8 Pacci_UltrahandArmed(void) {
    return sUhArmed;
}

void Pacci_HighlightUltrahandTarget(PlayState* play) {
    sUhHighlightTarget = NULL;
    // sUltrahand.held, not Pacci_IsHoldingUltrahand(): that one goes false the moment a drop
    // starts, and marking a new candidate while the last one is still falling advertises a
    // grab that Pacci_CastUltrahand now correctly refuses.
    if (sUltrahand.held != NULL) {
        return;
    }
    // Nothing is held, so the ONLY thing that may be lit right now is a candidate. Clearing
    // here rather than at the top of the function is deliberate: this runs from the cane's own
    // per-frame handler as well as from the mode, and clearing before the early return above
    // would wipe the held object's tint on every frame a grab happened outside the mode.
    Pacci_UhTintClear();

    Actor* target = Pacci_ResolveUltrahandTarget(play, GET_PLAYER(play));

    if (target != NULL) {
        // What A would take, marked the same way it will be marked once taken.
        target->colorFilterParams = 0;
        Pacci_UhTintAdd(target);
        sUhHighlightTarget = target;
    }
}

// Line-test along Link's aim and return the DynaPoly actor owning whatever surface
// it lands on, or NULL for plain scenery. Same idea as the remote's projectile,
// minus the projectile: it line-tested during flight and read the bgId out of the
// hit, which is what let it latch onto bg actors.
// One cast, at the aim plus a pitch offset. Split out so the caller can sweep.
static Actor* Pacci_UltrahandRaycastAt(PlayState* play, Player* player, s16 pitchBias) {
    Vec3f from = player->actor.world.pos;
    Vec3f to;
    Vec3f hit;
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;
    s16 pitch = player->actor.focus.rot.x + pitchBias;
    f32 distXZ = Math_CosS(pitch) * PACCI_UH_DIST_MAX;
    f32 distY = Math_SinS(pitch) * PACCI_UH_DIST_MAX;

    from.y += 40.0f; // from the chest, not the feet
    to.x = from.x + (Math_SinS(player->actor.focus.rot.y) * distXZ);
    to.z = from.z + (Math_CosS(player->actor.focus.rot.y) * distXZ);
    to.y = from.y - distY;

    // ProjectileLineTest, not EntityLineTest1: the entity variant resolves against
    // scene geometry and does not report the bg actor behind a dynapoly surface, so
    // bgId came back as BGCHECK_SCENE and DynaPoly_GetActor always returned NULL —
    // which is why scenery never got grabbed. This is the one the remote used, and
    // its signature is identical.
    // Cast repeatedly instead of once. The FIRST surface in front of Link is very often the
    // thing he is already holding — it hangs on the aim line by construction — or a piece
    // welded to it. One cast therefore answered "the held object", the preview rejected it as
    // target == held, and no weld was ever offered. That is the "I am clearly lined up and it
    // will not stick" case. Each time the ray lands on our own assembly it restarts just past
    // that surface and keeps going; anything else ends the search, so this cannot reach
    // through a wall.
    for (s32 attempt = 0; attempt < 4; attempt++) {
        DynaPolyActor* dyna;
        Actor* found;
        u8 ours;
        f32 dx;
        f32 dy;
        f32 dz;
        f32 len;

        if (!BgCheck_ProjectileLineTest(&play->colCtx, &from, &to, &hit, &poly, true, true, true, true, &bgId)) {
            break;
        }
        dyna = DynaPoly_GetActor(&play->colCtx, bgId);
        found = ((dyna != NULL) && (dyna->actor.update != NULL)) ? &dyna->actor : NULL;

        if (found == NULL) {
            break; // plain scenery: whatever is behind it is behind a wall
        }
        // "Is this OUR assembly?" - and the second half of that question only has meaning when
        // something is actually held.
        //
        // It used to read `(found != held) && (Pacci_FuseRootOf(found) != held)`, which looks
        // right and is wrong in the single most common case there is: with nothing in hand, held
        // is NULL, and Pacci_FuseRootOf of an actor that belongs to no assembly is ALSO NULL. So
        // NULL != NULL came out false, the guard rejected the hit, and the ray stepped past it.
        // Every dynapoly surface in the world was skipped, every time, whenever Link was
        // empty-handed - which is exactly when you are trying to grab one.
        //
        // Loose props were never affected because they are found by the actor scan instead, and
        // that is why "some things work and gravestones never do" kept coming back.
        ours =
            (found == sUltrahand.held) || ((sUltrahand.held != NULL) && (Pacci_FuseRootOf(found) == sUltrahand.held));
        if (!ours) {
            if (Pacci_IsLiftableEx(found, 1)) { // dynapoly by construction
                return found;
            }
            // "Not liftable" is not the same as "nothing here". Returning NULL let a single
            // unliftable dynapoly surface in front hide every grabbable one behind it - and in
            // a graveyard, or any room built out of bg actors, that is most of what the aim
            // line passes through on the way to the thing you are pointing at. Step past it,
            // exactly as we step past our own assembly.
        }

        // It was our own piece. Resume from just past where the ray hit it.
        dx = to.x - from.x;
        dy = to.y - from.y;
        dz = to.z - from.z;
        len = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
        if (len < 1.0f) {
            break;
        }
        from.x = hit.x + ((dx / len) * 3.0f);
        from.y = hit.y + ((dy / len) * 3.0f);
        from.z = hit.z + ((dz / len) * 3.0f);
    }
    return NULL;
}

// Cast along the aim, then a little below it, then further below, then a little above.
//
// The straight aim alone misses things low in front of Link, and it is not the ray's fault: the
// third-person camera will not pitch down far enough to point at something near his feet, so the
// player IS looking at the object and the aim line still passes over its head. That is the "a
// veces no detecta bien en Y debajo de Link".
//
// A sweep rather than simply aiming lower, because biasing the single ray downward would trade
// the bug for its mirror image at eye level. Order matters: the unbiased cast is tried first and
// wins outright, so nothing that already worked starts resolving to something else. The upward
// entry is last and small - it is only there for a body on a ledge just above the aim.
static Actor* Pacci_UltrahandRaycastDyna(PlayState* play, Player* player) {
    static const s16 sPitchSweep[] = { 0, 0x0A00, 0x1600, -0x0800 };

    for (u8 i = 0; i < ARRAY_COUNT(sPitchSweep); i++) {
        Actor* found = Pacci_UltrahandRaycastAt(play, player, sPitchSweep[i]);

        if (found != NULL) {
            return found;
        }
    }
    return NULL;
}

// The ONE place that decides what Ultrahand would take. Both the grab and the
// highlight go through it, so what you see marked is always exactly what A gets.
// They used to resolve separately — the grab raycast for dynapoly first and then
// fell back to the actor scan, while the highlight only ever ran the actor scan —
// which is why scenery could be grabbed without ever being marked, and why marked
// objects sometimes were not the one taken.
// The hole that let dialogue triggers, song spots and spawners be picked up. The raycast
// half of the resolve has always run Pacci_IsLiftable on what it finds, but the fallback
// actor scan went straight to TargetSelect_IsCommonTarget, which only asks "is this a
// targetable category" - it knows nothing about draw functions or blacklists. So anything
// invisible in PROP or NPC sailed through the one path that never checked.
static s32 Pacci_UhTargetFilter(Actor* actor) {
    return TargetSelect_IsCommonTarget(actor) && Pacci_IsLiftable(actor);
}

// Third route: a proximity sweep over the actors that neither of the other two can see.
//
// It is bounded by construction - it only ever looks at actors with a REACHABLE row, so it cannot
// start turning up junk the way a blanket "walk every category" scan would. In front of Link and
// within arm's reach, rather than along the aim line, because these are small things sitting on
// the floor and the camera cannot point at those anyway (the same reason the raycast sweeps pitch).
static Actor* Pacci_UhScanReachable(PlayState* play, Player* player) {
    Actor* best = NULL;
    f32 bestD = PACCI_UH_REACH_RANGE * PACCI_UH_REACH_RANGE;
    s32 cat;

    for (cat = 0; cat < ACTORCAT_MAX; cat++) {
        Actor* it;

        for (it = play->actorCtx.actorLists[cat].head; it != NULL; it = it->next) {
            f32 dx;
            f32 dy;
            f32 dz;
            f32 d;
            s16 yawOff;

            if ((it->update == NULL) || !(Pacci_UhTraits(it) & PACCI_UH_TRAIT_REACHABLE)) {
                continue;
            }
            dx = it->world.pos.x - player->actor.world.pos.x;
            dy = it->world.pos.y - (player->actor.world.pos.y + 20.0f);
            dz = it->world.pos.z - player->actor.world.pos.z;
            d = (dx * dx) + (dy * dy) + (dz * dz);
            if (d >= bestD) {
                continue;
            }
            // In front of him, not behind: s16 subtraction already wraps to the shortest signed
            // difference, so this is the angle between the aim and the actor with no normalising.
            yawOff = Math_Vec3f_Yaw(&player->actor.world.pos, &it->world.pos) - player->actor.focus.rot.y;
            if ((yawOff > PACCI_UH_REACH_CONE) || (yawOff < -PACCI_UH_REACH_CONE)) {
                continue;
            }
            bestD = d;
            best = it;
        }
    }
    return best;
}

static Actor* Pacci_ResolveUltrahandTarget(PlayState* play, Player* player) {
    Actor* target = Pacci_UltrahandRaycastDyna(play, player);

    if (target == NULL) {
        target = TargetSelect_Scan(play, Pacci_UhTargetFilter);
    }
    // Last, so nothing that already resolved starts resolving to a flame at your feet instead.
    if (target == NULL) {
        target = Pacci_UhScanReachable(play, player);
    }
    return target;
}

// Hand the body back to physics and let it fall. Split out because there are three ways to
// let go - the cast, A inside the mode, and leaving the mode - and only the first of them
// used to run any physics at all. The other two called Pacci_UltrahandLetGo directly, which
// restores the actor's own update on the spot: a crate released mid-air simply stopped there
// if its own update had no gravity of its own.
//
// NOT used by Pacci_DropUltrahand. That one runs on UNEQUIP, and the fall is driven from
// Pacci_UpdateUltrahand, which the cane stops calling the moment it leaves Link's hand -
// so a drop begun there would freeze on its first frame. Unequipping stays an instant let go.
static void Pacci_UltrahandBeginDrop(void) {
    Actor* actor = sUltrahand.held;
    f32 speed;

    if ((actor == NULL) || sUltrahand.dropping) {
        return;
    }
    // A body on a track is PLACED, not dropped. So is one that was never allowed to move at all.
    //
    // A lift you raised is meant to stay raised - that is the whole reason for raising it - and a
    // platform confined to a path has no meaningful "down" to fall toward anyway: the constraint
    // would just slide it back along its own rail while it fell. Letting go in place is what the
    // player asked for by moving it there.
    //
    // LOCKED is here for a blunter reason. That body was pinned to the spot for the entire hold and
    // the D-pad was driving its FLAG, so dropping it was the one thing the whole trait exists to
    // prevent - and the skull's jaw went down to the floor like a boulder the moment you let go.
    //
    // What this cannot promise is that a lift STAYS. Several drive their own height from their own
    // logic the moment they have their update back - Bg_Mori_Elevator steps toward a fixed 73.0f,
    // Bg_Hidan_Syoku rebuilds its Y from a cosine of its own timer - and that is theirs to do. The
    // difference is that they now return to their own position on their own terms instead of being
    // thrown at the floor first.
    //
    // PLANE_XZ belongs here too: its constraint pins Y for the whole fall, so "dropping" one was a
    // body hanging exactly where it already was until the timeout ran out. A ferry and a sliding
    // ice block are both floors - neither was ever above anything to land on.
    if (sUltrahand.traits &
        (PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_PLANE_XZ | PACCI_UH_TRAIT_PATH | PACCI_UH_TRAIT_LOCKED)) {
        Pacci_UltrahandLetGo();
        Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    sUltrahand.dropping = 1;
    sUltrahand.dropTimer = PACCI_UH_DROP_TIMEOUT;
    actor->colorFilterParams = 0;

    // Letting go while aiming is a THROW, not a drop. Aimed at the lock-on target if there is one,
    // straight ahead if the lock is on nothing in particular - and it overrides the carry's own
    // inertia entirely, because a throw is a decision and inertia is an accident.
    {
        Player* player = (gPlayState != NULL) ? GET_PLAYER(gPlayState) : NULL;

        if ((player != NULL) && Pacci_UhAiming(player, actor)) {
            Actor* mark = player->focusActor;
            s16 yaw = (mark != NULL) ? Math_Vec3f_Yaw(&actor->world.pos, &mark->world.pos) : player->actor.shape.rot.y;

            sUltrahand.carryVel.x = Math_SinS(yaw) * PACCI_UH_THROW_SPEED;
            sUltrahand.carryVel.z = Math_CosS(yaw) * PACCI_UH_THROW_SPEED;
            sUltrahand.carryVel.y = PACCI_UH_THROW_LIFT;
            LinkAnimation_PlayOnce(gPlayState, &player->upperSkelAnime, &gPlayerAnim_link_boom_throwR);
            Audio_PlayActorSound2(actor, NA_SE_IT_BOOMERANG_THROW);
        }
    }
    actor->gravity = PACCI_UH_DROP_GRAVITY;
    actor->minVelocityY = PACCI_UH_DROP_MIN_VEL_Y;
    actor->velocity = sUltrahand.carryVel;

    speed = sqrtf((actor->velocity.x * actor->velocity.x) + (actor->velocity.y * actor->velocity.y) +
                  (actor->velocity.z * actor->velocity.z));
    if (speed > PACCI_UH_THROW_MAX) {
        f32 scale = PACCI_UH_THROW_MAX / speed;

        actor->velocity.x *= scale;
        actor->velocity.y *= scale;
        actor->velocity.z *= scale;
    }
    Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// While Z is held the object rides in Link's HAND instead of orbiting him, because that is what
// winding up to throw looks like: the thing you are about to throw is in the hand that throws it.
//
// Only for plain objects - a pot, a rock, a crate, a bomb. Anything with a row in the trait table
// is a lift or a wall or a flame and has somewhere it is supposed to be; none of them wants to be
// cocked over Link's shoulder.
static u8 Pacci_UhThrowable(Actor* actor) {
    if (actor == NULL) {
        return 0;
    }
    return ((sUltrahand.traits == 0) || (sUltrahand.traits & PACCI_UH_TRAIT_EXPLODES)) ? 1 : 0;
}

static u8 Pacci_UhAiming(Player* player, Actor* actor) {
    return (Player_IsZTargeting(player) && Pacci_UhThrowable(actor)) ? 1 : 0;
}

// -- hauling --------------------------------------------------------------------
// Some things are too big to hold out in front of you. Link braces against them and drags them
// along the floor instead, with the animation the game already has for exactly this
// (gPlayerAnim_link_normal_pull_start / _pulling / _pull_end).
//
// The whole body is animated, not just the upper half the cane normally claims - a pull is a
// stance, and half a stance is a man doing a mime. The precedent for taking p->skelAnime is
// item_hylias_grace.c and item_dekuleaf.c, which do the same for their own set pieces.
//
// Link is pinned while he hauls. Letting him walk would fight the animation and stretch the rope
// nobody drew, so speed is zeroed every frame and the D-pad does the moving - which is the same
// place every other Ultrahand distance control lives.
static Actor* sPullActor = NULL;
static u8 sPullStarted = 0;

u8 Pacci_PullActive(void) {
    return (sPullActor != NULL) ? 1 : 0;
}

void Pacci_PullStop(PlayState* play) {
    Player* player = (play != NULL) ? GET_PLAYER(play) : NULL;

    if ((sPullActor != NULL) && (player != NULL) && sPullStarted) {
        LinkAnimation_Change(play, &player->skelAnime, &gPlayerAnim_link_normal_pull_end, 1.0f, 0.0f,
                             Animation_GetLastFrame(&gPlayerAnim_link_normal_pull_end), ANIMMODE_ONCE, -6.0f);
    }
    sPullActor = NULL;
    sPullStarted = 0;
}

static void Pacci_PullStart(PlayState* play, Player* player, Actor* target) {
    sPullActor = target;
    sPullStarted = 0;
    LinkAnimation_Change(play, &player->skelAnime, &gPlayerAnim_link_normal_pull_start, 1.0f, 0.0f,
                         Animation_GetLastFrame(&gPlayerAnim_link_normal_pull_start), ANIMMODE_ONCE, -6.0f);
    // The grunt Link makes taking hold of something heavy. A positional sound, from the body -
    // a menu blip out of nowhere would give away that nothing is really being lifted.
    Audio_PlayActorSound2(target, NA_SE_PL_PULL_UP_BIGROCK);
}

// One frame of hauling. This takes the pad's STATE rather than the edges every other Ultrahand
// control uses, because a pull is a sustained effort and not a press. Holding D-down drags it
// toward you and D-up pushes it away, along the line between you and it - the only direction a
// braced pull can go.
static void Pacci_PullTick(PlayState* play, Player* player, u8 dpad) {
    f32 dx;
    f32 dz;
    f32 len;
    f32 step = 0.0f;

    if ((sPullActor == NULL) || (player == NULL)) {
        return;
    }
    if (sPullActor->update == NULL) {
        Pacci_PullStop(play);
        return;
    }
    // The start animation runs once and then the loop takes over. Checked by asking the animation
    // rather than by counting frames, so a different playback speed cannot desynchronise it.
    if (!sPullStarted) {
        if (LinkAnimation_Update(play, &player->skelAnime)) {
            sPullStarted = 1;
            LinkAnimation_Change(play, &player->skelAnime, &gPlayerAnim_link_normal_pulling, 1.0f, 0.0f,
                                 Animation_GetLastFrame(&gPlayerAnim_link_normal_pulling), ANIMMODE_LOOP, -4.0f);
        }
    } else {
        LinkAnimation_Update(play, &player->skelAnime);
    }
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;

    if (dpad & 2) {
        step = -PACCI_UH_PULL_RATE; // toward Link
    } else if (dpad & 1) {
        step = PACCI_UH_PULL_RATE; // away
    } else {
        return;
    }
    dx = sPullActor->world.pos.x - player->actor.world.pos.x;
    dz = sPullActor->world.pos.z - player->actor.world.pos.z;
    len = sqrtf((dx * dx) + (dz * dz));
    if (len < 1.0f) {
        return;
    }
    sPullActor->world.pos.x += (dx / len) * step;
    sPullActor->world.pos.z += (dz / len) * step;
    // Bg_Po_Event and Bg_Hidan_Dalm both rebuild their position from home, so home comes along.
    sPullActor->home.pos.x = sPullActor->world.pos.x;
    sPullActor->home.pos.z = sPullActor->world.pos.z;
    sPullActor->prevPos = sPullActor->world.pos;
    Pacci_AnchorSync(play, sPullActor);
}

// -- handing something to its own throw -----------------------------------------
// Bg_Heavy_Block already knows how to be thrown. BgHeavyBlock_Wait watches Actor_HasParent and
// BgHeavyBlock_LiftedUp watches Actor_HasNoParent, and between them they run the lift cutscene,
// the quake, the NA_SE_EV_HEAVY_THROW and the flight (z_bg_heavy_block.c:322-390). All of that is
// gated on ONE field: actor->parent.
//
// So the cane sets parent for a frame and clears it. Two writes, and the block does the rest -
// no actionFunc poked, no velocity guessed, no cutscene reimplemented. It is the same sequence
// Link's own hands produce, which is why it looks right.
static Actor* sUhThrowActor = NULL;
static s16 sUhThrowTimer = 0;

void Pacci_ThrowTick(PlayState* play) {
    if ((play == NULL) || (sUhThrowActor == NULL)) {
        return;
    }
    if (sUhThrowActor->update == NULL) {
        sUhThrowActor = NULL;
        sUhThrowTimer = 0;
        return;
    }
    if (sUhThrowTimer > 0) {
        sUhThrowTimer--;
        return; // still "lifted"; its Wait state is running the pickup
    }
    // The throw itself, and this part is NOT the actor's. BgHeavyBlock_Fly integrates speedXZ along
    // world.rot.y and never sets either - in vanilla they arrive from Link, on frame 6 of his throw
    // animation (Player_Action_80846358, z_player.c:12253-12256). Without them the block just
    // dropped where it stood. These are his three numbers, unchanged.
    {
        Player* player = GET_PLAYER(play);

        sUhThrowActor->world.rot.y = player->actor.shape.rot.y;
        sUhThrowActor->speedXZ = 10.0f;
        sUhThrowActor->velocity.y = 20.0f;
    }
    // Actor_HasNoParent goes true and the block flies.
    sUhThrowActor->parent = NULL;
    sUhThrowActor = NULL;
}

// Is a THROWS body mid-hand-off? cane_ship.cpp asks, so Link is not frozen into the carry cutscene
// for a lift he never actually performed.
u8 Pacci_IsThrowing(void) {
    return (sUhThrowActor != NULL) ? 1 : 0;
}

static void Pacci_ThrowArm(Player* player, Actor* target) {
    // Held long enough for the target's own update to see the parent and move on to its carried
    // state. One frame would be a coin flip on update order; a handful is not.
    target->parent = &player->actor;
    sUhThrowActor = target;
    sUhThrowTimer = PACCI_UH_THROW_HOLD;
}

// -- cutting --------------------------------------------------------------------
// One frame of boomerang damage, delivered where the actor is standing.
//
// Dealt as a real hit rather than by writing colChkInfo.health or calling Actor_Kill, because the
// actor's own damage path is where everything worth having lives: the recoil, the sound, the
// flag it sets on the killing blow, and the fact that four hits are four hits. Reaching past all
// of that to zero the health would be reimplementing En_Ba badly.
//
// It stays armed for a few frames because an AT only meets an AC when both are in the same frame's
// lists, and the target submits its own on its own schedule.
static ColliderCylinder sUhCutCol;
static Actor* sUhCutTarget = NULL;
static s16 sUhCutTimer = 0;
static u32 sUhCutFlags = 0;
static s16 sUhCutDamage = 0;

void Pacci_CutTick(PlayState* play) {
    CombatColliderConfig cfg;
    Vec3f pos;

    if ((play == NULL) || (sUhCutTarget == NULL) || (sUhCutTimer <= 0)) {
        return;
    }
    if (sUhCutTarget->update == NULL) {
        sUhCutTarget = NULL; // it died - which is the point
        sUhCutTimer = 0;
        return;
    }
    sUhCutTimer--;
    cfg.dmgFlags = sUhCutFlags;
    cfg.damage = sUhCutDamage;
    cfg.effect = 0;
    cfg.radius = PACCI_UH_CUT_RADIUS;
    cfg.height = PACCI_UH_CUT_HEIGHT;
    pos = sUhCutTarget->world.pos;
    Combat_UpdateCylinder(&sUhCutCol, &pos, &cfg);
    Combat_RegisterCollider(play, &sUhCutCol);
    if (Combat_CheckHit(&sUhCutCol)) {
        sUhCutCol.base.atFlags &= ~AT_HIT;
    }
}

static void Pacci_CutArm(PlayState* play, Player* player, Actor* target, const PacciUhTraitRow* row) {
    CombatColliderConfig cfg;

    sUhCutFlags = row->hitFlags;
    sUhCutDamage = (row->hitDamage != 0) ? row->hitDamage : PACCI_UH_CUT_DAMAGE;
    cfg.dmgFlags = sUhCutFlags;
    cfg.damage = sUhCutDamage;
    cfg.effect = 0;
    cfg.radius = PACCI_UH_CUT_RADIUS;
    cfg.height = PACCI_UH_CUT_HEIGHT;
    // Owned by LINK, not by the target: an actor's own collider never damages itself, and the game
    // has to attribute the cut to the player for the tentacle to react to it as a boomerang would.
    Combat_InitCylinder(play, &sUhCutCol, &player->actor, &cfg);
    sUhCutTarget = target;
    sUhCutTimer = PACCI_UH_CUT_FRAMES;
    Audio_PlayActorSound2(target, NA_SE_IT_BOOMERANG_THROW);
}

// -- proxies and fuses ---------------------------------------------------------
// Some things are worth carrying and worth leaving alone at the same time. A bomb flower that
// walks away is a bomb flower nobody else can use; a fire wall carried off is a room that stopped
// being dangerous. So the grab spawns a stand-in and you carry that, and it dies when you let go.
//

// The fuse, through the real struct.
//
// This used to compute the address by hand from the /* 0x01F8 */ in z_en_bom.h, and that was
// simply wrong: those comments are N64 offsets, and on 64-bit every pointer inside Actor and inside
// ColliderCylinder is twice as wide, so EnBom::timer is nowhere near 0x1F8. The guard that was
// supposed to make a hand-computed offset safe did exactly its job - it saw garbage and refused to
// write - which is why the fuse kept running and nothing appeared to be happening at all.
static s16* Pacci_UhBombTimer(Actor* actor) {
    if ((actor == NULL) || (actor->id != ACTOR_EN_BOM)) {
        return NULL;
    }
    return &((EnBom*)actor)->timer;
}

// While it is in your hands the fuse does not run. Re-asserted every frame rather than set once,
// because En_Bom counts down inside its own update and that update is still running.
static void Pacci_UhBombTick(Actor* actor) {
    s16* timer;

    if (!(sUltrahand.traits & PACCI_UH_TRAIT_EXPLODES)) {
        return;
    }
    timer = Pacci_UhBombTimer(actor);
    if (timer != NULL) {
        *timer = 60;
    }
}

// Light it. The bomb explodes on its own terms - its own update owns the flash, the sound, the
// damage and the debris, and none of that is worth reimplementing badly.
static u8 Pacci_UhBombDetonate(PlayState* play) {
    Actor* actor = sUltrahand.held;
    s16* timer;

    if (!(sUltrahand.traits & PACCI_UH_TRAIT_EXPLODES) || (actor == NULL)) {
        return 0;
    }
    timer = Pacci_UhBombTimer(actor);
    if (timer == NULL) {
        return 0;
    }
    *timer = 1;
    sUhProxy = NULL; // it is going to kill itself; do not kill it out from under the explosion
    Pacci_UltrahandLetGo();
    return 1;
}

// Everything a grab sets up, with no opinion about how the target was chosen. The summon
// needs exactly this and has no aim to resolve - it just built the thing itself.
static void Pacci_UltrahandTake(Player* player, Actor* target) {
    f32 dx;

    // ONE assembly exists at a time - sFuse is a single global - so picking up something that is
    // not the current structure's root has to let that structure go first.
    //
    // This is the bug behind "solo cae el objeto agarrado, sus attached no". Nothing used to
    // clear sFuse on a new grab, so its parts stayed in the list with offsets measured against
    // the OLD root while sFuse.root got overwritten by the next weld. From then on Pacci_FuseFollow
    // drove somebody else's pieces from the new root: they no longer moved with the thing that was
    // actually falling, and their anchored colliders stayed pinned to a structure that no longer
    // existed. Whether they looked stuck in the air or teleported across the room depended only on
    // where the two roots happened to be.
    // Through the accessors, not sFuse directly: that lives in the FUSION section hundreds of
    // lines below and is not visible here.
    if ((Pacci_FuseCount() > 0) && (Pacci_FuseRootOf(target) != target)) {
        Pacci_FuseRelease();
    }

    dx = target->world.pos.x - player->actor.world.pos.x;
    f32 dy = target->world.pos.y - player->actor.world.pos.y;
    f32 dz = target->world.pos.z - player->actor.world.pos.z;

    sUltrahand.held = target;
    sUltrahand.dropping = 0;
    sUltrahand.dropTimer = 0;
    sUltrahand.origGravity = target->gravity;
    sUltrahand.origMinVelocityY = target->minVelocityY;
    sUltrahand.origRoom = target->room;
    sUltrahand.baseRot = target->shape.rot;
    sUltrahand.grabRot = target->shape.rot;
    sUltrahand.grabWorldRot = target->world.rot;
    sUltrahand.carryYaw = player->actor.focus.rot.y; // start on the aim, no opening swing
    sUltrahand.carryVel.x = 0.0f;
    sUltrahand.carryVel.y = 0.0f;
    sUltrahand.carryVel.z = 0.0f;
    sUltrahand.traits = Pacci_UhTraits(target);
    {
        const PacciUhTraitRow* row = Pacci_UhTraitRow(target);

        sUltrahand.flagDir = (row != NULL) ? row->flagDir : 0;
    }
    sUltrahand.railPos = target->world.pos;
    sUltrahand.pathId = 0;
    sUltrahand.pathCount = 0;
    if (sUltrahand.traits & PACCI_UH_TRAIT_PATH) {
        const PacciUhTraitRow* row = Pacci_UhTraitRow(target);
        PlayState* play = gPlayState;

        sUltrahand.pathId = (target->params >> row->pathShift) & row->pathMask;
        // A path that is missing or degenerate silently drops the constraint rather than pinning
        // the actor to garbage: setupPathList is scene data and not every scene has one.
        if ((play != NULL) && (play->setupPathList != NULL)) {
            sUltrahand.pathCount = play->setupPathList[sUltrahand.pathId].count;
        }
        if (sUltrahand.pathCount < 2) {
            sUltrahand.traits &= ~PACCI_UH_TRAIT_PATH;
        }
    }
    sUltrahand.faceOffsetYaw = target->shape.rot.y - Math_Vec3f_Yaw(&target->world.pos, &player->actor.world.pos);
    sUltrahand.vfxAge = 0;
    sUhHighlightTarget = NULL;
    Pacci_AnchorTake(target); // while it is still at rest, before the carry moves it
    sUltrahand.origUpdate = target->update;
    target->update = Pacci_UltrahandHeldUpdate;
    sUltrahand.distance = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
    sUltrahand.distance = CLAMP(sUltrahand.distance, PACCI_UH_DIST_MIN, PACCI_UH_DIST_MAX);
    target->room = -1; // held objects should survive a room change

    // The carry runs from LINK's update, and the engine syncs an actor's prevPos to world.pos right
    // before that actor's own update — so a carried body always reads as still to the recorder's
    // automatic admission test. Without this, nothing moved by Ultrahand could ever be recalled.
    Rewind_Track(target);
    Audio_PlayActorSound2(target, NA_SE_SY_GET_ITEM);
    Pacci_RefreshPlayerPose(player); // empty-handed -> hookshot hold
}

u8 Pacci_CastUltrahand(PlayState* play, Player* player) {
    // Already holding something -> the cast is the release.
    if (Pacci_IsHoldingUltrahand()) {
        Pacci_UltrahandBeginDrop();
        return 1;
    }

    // Ultrahand again is what takes her off. Checked before anything else, so the press is spent
    // on setting her down rather than on grabbing whatever happens to be behind her.
    if (Pacci_BackRiderActive()) {
        Pacci_BackRiderDrop();
        Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return 1;
    }

    // A drop in progress still owns sUltrahand.held: the body is falling, but it is ours until
    // it lands. Grabbing something else here overwrote that pointer and left the falling object
    // orphaned with our no-op update on it - frozen in mid-air, permanently. It also kept
    // blocking the aim raycast, which skips whatever sUltrahand.held is, so the next dynapoly
    // behind it stopped being detectable. Wait for the landing instead.
    if (sUltrahand.held != NULL) {
        return 0;
    }

    // DynaPoly FIRST, then loose actors. This is the one thing the remote's version
    // could do that ours could not: it grabs scenery. It finds it by line-testing
    // the world and asking DynaPoly_GetActor which bg actor owns the surface it hit
    // — the shared actor selector can never see those, because it walks the actor
    // categories and TargetSelect_IsCommonTarget covers ENEMY/PROP/CHEST/NPC only.
    // Platforms, moving blocks and structural pieces all live in ACTORCAT_BG with
    // their collision registered as dynapoly, so they were simply invisible to us.
    Actor* target = Pacci_ResolveUltrahandTarget(play, player);

    if (target == NULL) {
        return 0;
    }
    // Pointed at a piece of something already built? Take the structure, not the piece.
    // The parts are frozen and driven from the root, so holding a part directly would
    // move nothing at all — the visible failure was "I grabbed it and it did not come".
    if (Pacci_FuseRootOf(target) != NULL) {
        target = Pacci_FuseRootOf(target);
    }

    // Sitting Ruto rides instead of being carried out in front. Not through Pacci_UltrahandTake at
    // all: the carry holds one thing at a time and she is not taking that slot from a crate.
    if ((target->id == ACTOR_EN_RU1) && Pacci_UhCondRutoSitting(target)) {
        Pacci_BackRiderTake(play, player, target);
        return 1;
    }

    {
        const PacciUhTraitRow* row = Pacci_UhTraitRow(target);

        if ((row != NULL) && (row->traits & PACCI_UH_TRAIT_PULLABLE)) {
            Pacci_PullStart(play, player, target);
            return 1; // braced against it, not holding it
        }
        if ((row != NULL) && (row->traits & PACCI_UH_TRAIT_THROWS)) {
            Pacci_ThrowArm(player, target);
            return 1; // nothing is carried; the throw IS the action
        }
        if ((row != NULL) && (row->traits & PACCI_UH_TRAIT_STRIKES)) {
            Pacci_CutArm(play, player, target, row);
            return 1; // nothing is carried; the blow IS the action
        }
        if ((row != NULL) && (row->traits & PACCI_UH_TRAIT_PROXY)) {
            Actor* copy = Actor_Spawn(&play->actorCtx, play, row->proxyId, target->world.pos.x, target->world.pos.y,
                                      target->world.pos.z, 0, target->shape.rot.y, 0, row->proxyParams);

            if (copy == NULL) {
                return 0; // the actor pool said no; better to do nothing than half of it
            }
            sUhProxy = copy;
            Pacci_UltrahandTake(player, copy);
            // The traits belong to the ROW, not to the stand-in: a spawned En_Bom has no row of
            // its own and would otherwise be an ordinary object with no fuse control at all.
            sUltrahand.traits = row->traits;
            return 1;
        }
    }

    Pacci_UltrahandTake(player, target);
    return 1;
}

// Defined down in the FUSION section, because they need the oriented box and the part list.
// Declared here because the carry and the fall need them, and both live above that section.
static u8 Pacci_UhGroundUnder(PlayState* play, Actor* actor, f32* outY);
static u8 Pacci_UhAssemblyGround(PlayState* play, Actor* root, f32* outY);

// Put a constrained body back on its own axis, plane or path. Runs AFTER the carry has placed it
// wherever the controls asked for, so nothing above has to know these actors exist.
static void Pacci_UhConstrain(PlayState* play, Actor* actor) {
    u32 traits = sUltrahand.traits;

    if (traits & PACCI_UH_TRAIT_AXIS_Y) {
        actor->world.pos.x = sUltrahand.railPos.x;
        actor->world.pos.z = sUltrahand.railPos.z;
        sUltrahand.carryVel.x = 0.0f;
        sUltrahand.carryVel.z = 0.0f;
        if (traits & PACCI_UH_TRAIT_DRIVE_HOME) {
            // The height you dragged it to becomes the height it wants to be at. Its own update
            // takes it from there, at its own speed, which is why this looks like helping it rather
            // than dragging it: the lift still rides its rail, it has just been told a new floor.
            actor->home.pos.y = actor->world.pos.y;
        }
    }
    if (traits & PACCI_UH_TRAIT_PLANE_XZ) {
        actor->world.pos.y = sUltrahand.railPos.y;
        sUltrahand.carryVel.y = 0.0f;
    }
    if ((traits & PACCI_UH_TRAIT_PATH) && (sUltrahand.pathCount >= 2)) {
        // Nearest point on the path POLYLINE, segment by segment - not the nearest waypoint. A
        // Water Temple path is a handful of points tens of units apart; snapping to waypoints
        // would make the platform teleport between them instead of sliding along.
        Path* path = &play->setupPathList[sUltrahand.pathId];
        Vec3s* pts = (Vec3s*)SEGMENTED_TO_VIRTUAL(path->points);
        Vec3f want = actor->world.pos;
        Vec3f best = want;
        f32 bestD = 3.0e38f;
        s32 i;

        for (i = 0; i < (sUltrahand.pathCount - 1); i++) {
            Vec3f a;
            Vec3f b;
            Vec3f on;
            f32 abx;
            f32 aby;
            f32 abz;
            f32 len2;
            f32 t;
            f32 dx;
            f32 dy;
            f32 dz;
            f32 d;

            a.x = pts[i].x;
            a.y = pts[i].y;
            a.z = pts[i].z;
            b.x = pts[i + 1].x;
            b.y = pts[i + 1].y;
            b.z = pts[i + 1].z;
            abx = b.x - a.x;
            aby = b.y - a.y;
            abz = b.z - a.z;
            len2 = (abx * abx) + (aby * aby) + (abz * abz);
            if (len2 < 1.0f) {
                continue;
            }
            t = (((want.x - a.x) * abx) + ((want.y - a.y) * aby) + ((want.z - a.z) * abz)) / len2;
            t = CLAMP(t, 0.0f, 1.0f); // clamped, so the ends of the path are the ends of the travel
            on.x = a.x + (abx * t);
            on.y = a.y + (aby * t);
            on.z = a.z + (abz * t);
            dx = want.x - on.x;
            dy = want.y - on.y;
            dz = want.z - on.z;
            d = (dx * dx) + (dy * dy) + (dz * dz);
            if (d < bestD) {
                bestD = d;
                best = on;
            }
        }
        actor->world.pos = best;
    }
}

// Last frame this ran, so it cannot run twice in one. There are now three callers - the mode,
// the cane's own handler, and the global drop tick - and gravity applied twice in a frame is a
// fall at double speed that punches through the floor before the landing probe sees it.
static u32 sUhLastTick = 0xFFFFFFFF;

void Pacci_UpdateUltrahand(PlayState* play, Player* player) {
    Actor* actor = sUltrahand.held;
    Input* input = &play->state.input[0];

    if (actor == NULL) {
        return;
    }
    if (sUhLastTick == play->gameplayFrames) {
        return;
    }
    sUhLastTick = play->gameplayFrames;
    if (actor->update == NULL) { // it died while we held it
        sUltrahand.held = NULL;
        sUltrahand.dropping = 0;
        return;
    }

    // Re-claimed every frame the body is ours. Held perfectly still it would otherwise age out of
    // the recorder as "nothing ever happened here" and lose the carry that came before.
    Rewind_Track(actor);

    if (!sUltrahand.dropping) {
        s16 playerFacingYaw;
        s16 targetFacingYaw;
        Vec3f prevWorld = actor->world.pos; // sampled before the carry moves it, for inertia
        // NO D-pad handling here. This function is the carry physics only; the mode
        // (Pacci_UltrahandModeUpdate) owns every button and calls this at the end of
        // its frame. The port's original D-pad block used to live here — reading the
        // pad WITHOUT the L modifier — so it ran alongside the mode's and overrode
        // it: L + D-left/right looked like push/pull instead of the X rotation the
        // mode had just applied, because this ran second and won.
        //
        // Hold it where Link is looking (pitch included), easing in so it glides.
        f32 distXZ = Math_CosS(player->actor.focus.rot.x) * sUltrahand.distance;
        f32 distY = Math_SinS(player->actor.focus.rot.x) * sUltrahand.distance;
        // sideOff runs perpendicular to the aim (yaw + 90 degrees), so R + D-left/right slides the
        // object across your view instead of along it — the plane D-up/down does not cover.
        // Chase Link's aim at a capped rate. s16 subtraction already wraps to the shortest
        // signed difference, so this needs no angle normalisation of its own.
        s16 yawDelta = player->actor.focus.rot.y - sUltrahand.carryYaw;
        f32 sideX;
        f32 sideZ;
        f32 targetX;
        f32 targetZ;

        if (yawDelta > PACCI_UH_TURN_RATE) {
            yawDelta = PACCI_UH_TURN_RATE;
        } else if (yawDelta < -PACCI_UH_TURN_RATE) {
            yawDelta = -PACCI_UH_TURN_RATE;
        }
        sUltrahand.carryYaw += yawDelta;

        // Everything positional hangs off carryYaw, never off the raw aim. That is the whole
        // arc: the anchor can only ever be one capped step further round the circle.
        sideX = Math_SinS(sUltrahand.carryYaw + 0x4000) * sUhMode.sideOff;
        sideZ = Math_CosS(sUltrahand.carryYaw + 0x4000) * sUhMode.sideOff;
        targetX = (Math_SinS(sUltrahand.carryYaw) * distXZ) + player->actor.world.pos.x + sideX;
        targetZ = (Math_CosS(sUltrahand.carryYaw) * distXZ) + player->actor.world.pos.z + sideZ;
        // The mode's L + D-up/down offset rides on top of the aim line.
        f32 targetY = -distY + player->actor.world.pos.y + sUhMode.heightOff;

        f32 oldW = PACCI_UH_FOLLOW_WEIGHT;
        f32 newW = 1.0f - oldW;

        // Winding up: the carry point becomes Link's own throwing hand.
        if (Pacci_UhAiming(player, actor)) {
            Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];

            targetX = hand.x + (Math_SinS(player->actor.shape.rot.y) * PACCI_UH_THROW_REACH);
            targetY = hand.y + PACCI_UH_THROW_RISE;
            targetZ = hand.z + (Math_CosS(player->actor.shape.rot.y) * PACCI_UH_THROW_REACH);
        }

        // The anchor is THIS - the raw target the controls asked for - and it has to be taken
        // before the easing below, not after.
        //
        // Taking it after was the bug behind "once it offers, you cannot move it away". The carry
        // eases from the CURRENT position toward the target at 20% a frame, and the magnet keeps
        // putting the current position back on the switch, so the eased result never travelled far
        // from the switch either. The anchor read as "still right on top of it" no matter where the
        // player pointed, the offer never expired, and the block was stuck to the plate. The
        // feedback loop ran through the carry's own smoothing, not through the measurement.
        sUltrahand.anchorPos.x = targetX;
        sUltrahand.anchorPos.y = targetY;
        sUltrahand.anchorPos.z = targetZ;

        actor->world.pos.x = (actor->world.pos.x * oldW) + (targetX * newW) - actor->colChkInfo.displacement.x;
        actor->world.pos.y = (actor->world.pos.y * oldW) + (targetY * newW) - actor->colChkInfo.displacement.y;
        actor->world.pos.z = (actor->world.pos.z * oldW) + (targetZ * newW) - actor->colChkInfo.displacement.z;
        actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;

        Actor_UpdateBgCheckInfo(play, actor, 0.0f, 0.0f, 0.0f, 4);
        // CLAMP to the floor, do not nudge upward. The old "+= 1.0f while grounded" was a
        // ratchet: on a slope the object stays flagged as grounded every single frame, so it
        // gained a unit per frame and shot into the sky. Snapping to floorHeight only when it
        // is actually below it does the job the nudge was meant to do — keep it out of the
        // ground — with no way to accumulate.
        if ((actor->bgCheckFlags & BGCHECKFLAG_GROUND) && (actor->world.pos.y < actor->floorHeight)) {
            actor->world.pos.y = actor->floorHeight;
        }
        // OC displacement is what Link's own body pushed into it. It was already subtracted
        // above; leaving it set lets it build up frame after frame while he stands against the
        // thing he is carrying, which is the other half of the launch.
        actor->colChkInfo.displacement.x = 0.0f;
        actor->colChkInfo.displacement.y = 0.0f;
        actor->colChkInfo.displacement.z = 0.0f;
        // Release inertia, measured from how far the body ACTUALLY moved and smoothed. The
        // carry eases toward its anchor, so the raw delta on the single frame you let go is
        // either near zero (you had already stopped) or a spike (you had just whipped round);
        // neither is the throw the player meant.
        sUltrahand.carryVel.x = (sUltrahand.carryVel.x * 0.65f) + ((actor->world.pos.x - prevWorld.x) * 0.35f);
        sUltrahand.carryVel.y = (sUltrahand.carryVel.y * 0.65f) + ((actor->world.pos.y - prevWorld.y) * 0.35f);
        sUltrahand.carryVel.z = (sUltrahand.carryVel.z * 0.65f) + ((actor->world.pos.z - prevWorld.z) * 0.35f);
        // Constrained actors: everything above ran normally - the anchor still orbits Link, every
        // control still works - and then the position is simply put back onto whatever the actor
        // is allowed to move along. Doing it here rather than special-casing the maths above
        // keeps ONE carry, and means the controls drive a lift or an ice block exactly the way
        // they drive anything else.
        if (sUltrahand.traits & PACCI_UH_TRAIT_LOCKED) {
            // Held, not moved. Put back exactly where it was grabbed, every frame - the controls
            // are driving its flag instead (see Pacci_UhLockedInput).
            actor->world.pos = sUltrahand.railPos;
            sUltrahand.carryVel.x = 0.0f;
            sUltrahand.carryVel.y = 0.0f;
            sUltrahand.carryVel.z = 0.0f;
        }
        Pacci_UhConstrain(play, actor);
        // LOCKED gets out first, and touches as little as possible on the way.
        //
        // NO_TURN used to do this job and it was the wrong tool: it writes the whole of shape.rot
        // every frame, which is fine for a lift that has no opinion about its own facing and wrong
        // for anything with a state machine - it was overwriting the actor's own animation. A
        // locked body keeps its rotation; the only axis touched is the one its flag owns, and only
        // when that flag says so.
        if (sUltrahand.traits & PACCI_UH_TRAIT_LOCKED) {
            Pacci_UhLockedPose(play, actor);
            actor->world.rot = actor->shape.rot;
            actor->colorFilterParams = 0;
            Pacci_UhTintAdd(actor);
            if (sUltrahand.vfxAge < 0x7FFF) {
                sUltrahand.vfxAge++;
            }
            return;
        }

        if (sUltrahand.traits & PACCI_UH_TRAIT_NO_TURN) {
            // A lift is a floor and an ice block is a wall you push. Presenting a chosen face to
            // the player is meaningless for either, and turning one drags whatever is standing on
            // it around with it.
            actor->shape.rot = sUltrahand.grabRot;
            actor->world.rot = actor->shape.rot;
            Pacci_UhFlagTick(play, actor); // this branch returns early; it still has to count
            Pacci_UhFireTick(play, actor); // ...and it still has to burn
            Pacci_UhIceTick(play, actor);
            actor->colorFilterParams = 0;
            Pacci_UhTintAdd(actor);
            if (sUltrahand.vfxAge < 0x7FFF) {
                sUltrahand.vfxAge++;
            }
            return;
        }

        if (sUltrahand.traits & PACCI_UH_TRAIT_NO_FACE) {
            // Exactly the angle you set with L + D-pad, and nothing else touching it. baseRot IS
            // that angle - the manual controls are the only thing that writes it - so there is
            // nothing to combine here and nothing to smooth toward.
            actor->shape.rot = sUltrahand.baseRot;
        } else {
            // The selected face keeps looking toward Link as the held anchor orbits him.
            // Manual rotation is the delta from the grab pose, so movement never eats a
            // D-pad rotation and a D-pad rotation never changes the movement offsets.
            playerFacingYaw = Math_Vec3f_Yaw(&actor->world.pos, &player->actor.world.pos);
            targetFacingYaw =
                playerFacingYaw + sUltrahand.faceOffsetYaw + (sUltrahand.baseRot.y - sUltrahand.grabRot.y);
            actor->shape.rot.x = sUltrahand.baseRot.x;
            Math_SmoothStepToS(&actor->shape.rot.y, targetFacingYaw, 4, 0x1000, 0x20);
            actor->shape.rot.z = sUltrahand.baseRot.z;
        }
        actor->world.rot = actor->shape.rot;

        Pacci_UhPlaceMagnet(actor);
        Pacci_UhBombTick(actor);
        Pacci_UhFlagTick(play, actor);
        Pacci_UhFireTick(play, actor);
        Pacci_UhIceTick(play, actor);

        // THE TINT. Re-registered every frame for as long as the object is held; the table is
        // wiped at the top of the mode's frame, so this is what keeps it lit.
        actor->colorFilterParams = 0; // nothing from the engine filter fights the grayscale
        Pacci_UhTintAdd(actor);
        if (sUltrahand.vfxAge < 0x7FFF) {
            sUltrahand.vfxAge++;
        }
        return;
    }

    // Released: real gravity, and a landing decided by the body's own footprint.
    actor->gravity = PACCI_UH_DROP_GRAVITY;
    actor->velocity.y += actor->gravity;
    if (actor->velocity.y < PACCI_UH_DROP_MIN_VEL_Y) {
        actor->velocity.y = PACCI_UH_DROP_MIN_VEL_Y;
    }
    // Integrated by hand rather than through the engine's move-with-gravity helper. That one
    // drives XZ from speedXZ along world.rot.y, and world.rot here is the ORIENTATION YOU CHOSE for the
    // object - so a released crate flew wherever its front face happened to be pointing
    // instead of where you had been swinging it.
    actor->world.pos.y += actor->velocity.y;
    actor->world.pos.x += actor->velocity.x;
    actor->world.pos.z += actor->velocity.z;
    // The constraint outlives the release. Inherited sideways momentum would walk a lift out of
    // its shaft on the way down, which is the one thing the lock exists to prevent.
    Pacci_UhConstrain(play, actor);
    actor->velocity.x *= PACCI_UH_DROP_DRAG;
    actor->velocity.z *= PACCI_UH_DROP_DRAG;
    Actor_UpdateBgCheckInfo(play, actor, 0.0f, 0.0f, 0.0f, 4);
    // The parts have to be where the root says they are BEFORE the footprint is measured,
    // or the probe reads last frame's shape.
    Pacci_FuseFollow(play);

    {
        f32 restY;
        u8 landed = 0;

        if (Pacci_UhAssemblyGround(play, actor, &restY)) {
            if (actor->world.pos.y <= restY) {
                actor->world.pos.y = restY;
                landed = 1;
            }
        } else if (actor->bgCheckFlags & BGCHECKFLAG_GROUND) {
            // No usable box: the single-point check is all there is. Better than nothing,
            // and it is the old behaviour, so nothing that used to land stops landing.
            landed = 1;
        }
        if (sUltrahand.dropTimer > 0) {
            sUltrahand.dropTimer--;
        }
        if (landed) {
            actor->velocity.x = 0.0f;
            actor->velocity.y = 0.0f;
            actor->velocity.z = 0.0f;
            Pacci_UltrahandLetGo();
        } else if ((sUltrahand.dropTimer <= 0) ||
                   ((player->actor.world.pos.y - actor->world.pos.y) >= PACCI_UH_ABANDON_DROP)) {
            Pacci_UltrahandLetGo();
        }
    }
}

// ============================================================================
// FUSION
// ============================================================================
//
// Gluing makes the HELD object the ROOT of an assembly. Every piece stuck to it is
// stored as a position and rotation IN THE ROOT'S LOCAL FRAME, and from then on the
// root is the only thing anybody drives: each frame every part is rebuilt as
// root_pos + rotate(offset, root_rot). Move or spin the root and the structure moves
// as one solid. Grab the root again later and the whole thing comes with it.
//
// WHERE pieces may join is not free-form. Each actor gets an oriented box, and the
// box offers 27 weld points: its 8 corners, the 12 edge midpoints, the 6 face
// centres and the centre itself. A weld happens at the CLOSEST pair of points
// between the two boxes, which is what makes a cube land flush on another cube's
// corner or edge instead of floating at whatever sub-unit offset it happened to be
// at. The box comes from real collision wherever there is any: a dynapoly actor's
// CollisionHeader bounds, otherwise the actor's collision cylinder.
//
// An actor with NO collision of its own has no surface to weld along, so it is
// restricted to CORNERS, and the thing it pins to has to be genuine dynapoly.
//
// The collision is NOT merged, and that is deliberate. There used to be a runtime
// CollisionHeader builder here: it unregistered each part's bg actor and folded its polys
// into one surface owned by the root. It existed for one reason - parts were frozen, so
// their own colliders never repositioned - and once a part runs its own update again that
// reason is gone.
//
// Keeping it was actively harmful. Unregistering a part's dynapoly takes away the thing that
// IDENTIFIES it: DynaPoly_GetActor on any surface of the assembly answered "the root", so
// anything that finds an actor through its collision stopped finding the part. Bonk a tree
// glued to another tree and the game saw the other tree.
//
// So an assembly is N actors, each entirely itself - own update, own collision, own colliders -
// whose TRANSFORMS are driven together. A dynapoly actor re-transforms its own header from its
// actor SRT every frame, so writing world.pos and shape.rot moves its collision with it. The
// cost is that two pieces can leave a seam between them; the benefit is that a piece stuck to
// something is still the piece.
#define PACCI_FUSE_MAX_PARTS 6
// The anchor table has to have room for the root and every part it can hold. Declared up with
// the anchors, checked down here where the part count actually lives.
typedef char PacciAnchorFitsAssembly[(PACCI_ANCHOR_OWNERS >= (PACCI_FUSE_MAX_PARTS + 1)) ? 1 : -1];
#define PACCI_FUSE_PTS 27            // 8 corners + 12 edge mids + 6 face centres + centre
#define PACCI_FUSE_WELD_RANGE 130.0f // the best point pair has to be at least this close
// 80 was too strict once face centres were dropped: with only corners and edge midpoints
// left, two boxes can be visibly touching while their nearest PAIR of those points is
// still most of a box apart.
#define PACCI_FUSE_NOCOL_HALF 12.0f // nominal box for an actor with no collision at all
// The other way to earn a weld offer: the two BODIES are this close, measured surface to
// surface, whatever their nearest pair of weld points happens to be doing. See the two gates
// in Pacci_FuseSolve.
#define PACCI_FUSE_NEAR_GAP 55.0f
#define PACCI_FUSE_DETACH_HOLD 12 // frames of held R before a weld comes apart
// The shake-to-detach knobs live HERE, not with the rest of the Ultrahand-mode tuning: the
// detector is part of the fusion code and sits hundreds of lines above that block.
#define PACCI_UH_WIGGLE_WINDOW 14 // frames a direction flip stays "recent"
#define PACCI_UH_WIGGLE_FLIPS 3   // reversals inside that window before it counts as a shake

// Zonai green. Nintendo never published a hex for the Ultrahand glue, so this is
// eyeballed off the in-game glow: a pale chartreuse core inside a deeper green halo.
#define PACCI_ZONAI_CORE_R 210
#define PACCI_ZONAI_CORE_G 255
#define PACCI_ZONAI_CORE_B 140
#define PACCI_ZONAI_GLOW_R 80
#define PACCI_ZONAI_GLOW_G 200
#define PACCI_ZONAI_GLOW_B 40

typedef struct {
    Actor* actor;
    Vec3f offset;    // position in the root's local frame
    Vec3s rot;       // rotation relative to the root
    Vec3f weldLocal; // where the two met, root-local, so the bead can be redrawn
    // The part's own update, which our wrapper calls before re-imposing the transform.
    ActorFunc origUpdate;
    u32 origFlags;
    f32 origGravity;
    s16 origRoom;
} PacciFusePart;

typedef struct {
    Actor* root;
    PacciFusePart parts[PACCI_FUSE_MAX_PARTS];
    u8 count;
} PacciAssembly;

static PacciAssembly sFuse = { NULL, { { 0 } }, 0 };

// What the next A press would do, recomputed every frame while you hold something.
static struct {
    u8 valid;
    Actor* target;
    Vec3f weld;    // world point the two meet at — the Zonai bead sits here
    Vec3f snapPos; // where the held actor's origin lands if you commit
    // The two points the solve actually paired, each still on its own object. Both
    // are drawn, because a single bead at the meeting point cannot tell you WHICH
    // corner of the thing in your hands is about to land on WHICH corner of the
    // target — and that pairing is the whole decision you are making.
    Vec3f heldPt;
    Vec3f targetPt;
} sFusePv = { 0, NULL, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

static s16 sFuseDetachHold = 0;

// An oriented box standing in for the actor's shape. Only yaw: pitch and roll would
// need the full matrix and no weld point is worth that.
typedef struct {
    Vec3f center; // world
    Vec3f half;   // half-extents, already scaled
    s16 yaw;
    u8 solid; // has real collision of some kind
    u8 dyna;  // specifically a dynapoly bg actor
} PacciFuseBox;

// Put one part back where the assembly says it belongs: the offset is stored in the root's
// LOCAL frame, so it is rotated by the root's yaw and added to the root's position.
static void Pacci_FusePlacePart(PlayState* play, PacciFusePart* slot, Actor* root) {
    Actor* part = slot->actor;
    f32 sin = Math_SinS(root->shape.rot.y);
    f32 cos = Math_CosS(root->shape.rot.y);

    part->world.pos.x = root->world.pos.x + ((slot->offset.x * cos) + (slot->offset.z * sin));
    part->world.pos.y = root->world.pos.y + slot->offset.y;
    part->world.pos.z = root->world.pos.z + ((slot->offset.z * cos) - (slot->offset.x * sin));
    part->shape.rot.x = root->shape.rot.x + slot->rot.x;
    part->shape.rot.y = root->shape.rot.y + slot->rot.y;
    part->shape.rot.z = root->shape.rot.z + slot->rot.z;
    part->world.rot = part->shape.rot;
    // Whatever its own logic built up this frame is not motion the assembly agreed to.
    part->velocity.x = 0.0f;
    part->velocity.y = 0.0f;
    part->velocity.z = 0.0f;
}

// A glued part KEEPS ITS OWN AI. This used to be an empty function - the part was frozen
// outright, the same update-replacement the held object gets - and that was simpler, but it
// also meant a glued Deku Baba stopped biting, a glued torch stopped burning and every
// animation on every piece stopped dead. Gluing an ACTOR to something is not worth doing if
// what you get back is a statue.
//
// So its update runs, and then the assembly's transform is re-imposed IN THE SAME CALL.
// Correcting it afterwards from Pacci_FuseFollow is not enough on its own: actors update in
// category order, so a piece whose category runs after that correction would already have
// walked away from the structure by the time anything drew it.
static void Pacci_FusePartUpdate(Actor* thisx, PlayState* play) {
    for (u8 i = 0; i < sFuse.count; i++) {
        PacciFusePart* slot = &sFuse.parts[i];

        if (slot->actor != thisx) {
            continue;
        }
        Pacci_UhKeepOnScreen(thisx);
        if (slot->origUpdate != NULL) {
            slot->origUpdate(thisx, play);
        }
        // Its own update is allowed to kill it (a glued pot smashed by something, an enemy
        // that died mid-structure), and a dead actor must not be written to.
        if ((thisx->update != NULL) && (sFuse.root != NULL) && (sFuse.root->update != NULL)) {
            Pacci_FusePlacePart(play, slot, sFuse.root);
        }
        Pacci_AnchorSync(play, thisx); // same timing argument as the held object
        return;
    }
}

u8 Pacci_FuseCount(void) {
    return sFuse.count;
}

// Grabbing a piece that is already glued would tear the formation apart, so the
// targeting refuses it. The ROOT stays grabbable on purpose — that is how you pick
// the finished structure back up.
u8 Pacci_FuseIsPart(Actor* actor) {
    for (u8 i = 0; i < sFuse.count; i++) {
        if (sFuse.parts[i].actor == actor) {
            return 1;
        }
    }
    return 0;
}

// Which assembly, if any, this actor belongs to. Aiming at a glued piece has to act
// on the ROOT, because the root is the only thing the transform driver moves — taking
// hold of a part directly would drag it out of formation while the rest stayed put.
Actor* Pacci_FuseRootOf(Actor* actor) {
    if ((actor != NULL) && (sFuse.count > 0)) {
        if (actor == sFuse.root) {
            return sFuse.root;
        }
        if (Pacci_FuseIsPart(actor)) {
            return sFuse.root;
        }
    }
    return NULL;
}

// Drop the bookkeeping WITHOUT touching the actors. For scene teardown, where the
// pointers are already dead and writing through them would be a use-after-free.
void Pacci_FuseForget(void) {
    for (u8 i = 0; i < PACCI_FUSE_MAX_PARTS; i++) {
        sFuse.parts[i].actor = NULL;
    }
    sFuse.root = NULL;
    sFuse.count = 0;
    sFusePv.valid = 0;
    sFusePv.target = NULL;
}

static void Pacci_FuseGiveBack(PacciFusePart* part) {
    Actor* actor = part->actor;

    Pacci_AnchorRelease(actor);

    if ((actor != NULL) && (actor->update != NULL)) {
        if (part->origUpdate != NULL) {
            actor->update = part->origUpdate;
        }
        actor->flags = part->origFlags;
        actor->gravity = part->origGravity;
        actor->velocity.y = 0.0f;
        actor->room = (s8)part->origRoom;
        actor->colorFilterParams = 0;
    }
    part->actor = NULL;
}

// Hand every part back to itself, in place. Un-fuses a live structure.
void Pacci_FuseRelease(void) {
    for (u8 i = 0; i < sFuse.count; i++) {
        Pacci_FuseGiveBack(&sFuse.parts[i]);
    }
    sFuse.root = NULL;
    sFuse.count = 0;
    sFusePv.valid = 0;
    sFusePv.target = NULL;
}

// Pull ONE piece off and leave the rest of the structure standing.
u8 Pacci_FuseDetachPart(Actor* actor) {
    for (u8 i = 0; i < sFuse.count; i++) {
        if (sFuse.parts[i].actor != actor) {
            continue;
        }
        Pacci_FuseGiveBack(&sFuse.parts[i]);
        // Close the gap so the array stays dense; order carries no meaning here.
        for (u8 j = i; j < (u8)(sFuse.count - 1); j++) {
            sFuse.parts[j] = sFuse.parts[j + 1];
        }
        sFuse.count--;
        if (sFuse.count == 0) {
            sFuse.root = NULL;
        }
        return 1;
    }
    return 0;
}

// Fit an oriented box to whatever collision the actor actually has. Dynapoly gives a
// true fit from its CollisionHeader bounds; everything else falls back to the
// collision cylinder, which is the only size field every actor carries.
static u8 Pacci_FuseGetBox(PlayState* play, Actor* actor, PacciFuseBox* box) {
    CollisionHeader* hdr = NULL;

    if ((actor == NULL) || (actor->update == NULL)) {
        return 0;
    }
    box->yaw = actor->shape.rot.y;
    box->solid = 0;
    box->dyna = 0;

    for (s32 i = 0; i < BG_ACTOR_MAX; i++) {
        BgActor* bg = &play->colCtx.dyna.bgActors[i];

        if ((bg->actor == actor) && (bg->colHeader != NULL)) {
            hdr = bg->colHeader;
            break;
        }
    }

    if (hdr != NULL) {
        f32 lx = ((f32)hdr->maxBounds.x + (f32)hdr->minBounds.x) * 0.5f * actor->scale.x;
        f32 ly = ((f32)hdr->maxBounds.y + (f32)hdr->minBounds.y) * 0.5f * actor->scale.y;
        f32 lz = ((f32)hdr->maxBounds.z + (f32)hdr->minBounds.z) * 0.5f * actor->scale.z;
        f32 sin = Math_SinS(box->yaw);
        f32 cos = Math_CosS(box->yaw);

        box->half.x = ((f32)hdr->maxBounds.x - (f32)hdr->minBounds.x) * 0.5f * actor->scale.x;
        box->half.y = ((f32)hdr->maxBounds.y - (f32)hdr->minBounds.y) * 0.5f * actor->scale.y;
        box->half.z = ((f32)hdr->maxBounds.z - (f32)hdr->minBounds.z) * 0.5f * actor->scale.z;
        // The bounds are model-space, so their centre has to be carried out to world
        // through the actor's yaw before it means anything.
        box->center.x = actor->world.pos.x + ((lx * cos) + (lz * sin));
        box->center.y = actor->world.pos.y + ly;
        box->center.z = actor->world.pos.z + ((lz * cos) - (lx * sin));
        box->solid = 1;
        box->dyna = 1;
        return 1;
    }

    if (actor->colChkInfo.cylRadius > 0) {
        f32 r = (f32)actor->colChkInfo.cylRadius;
        f32 h = (actor->colChkInfo.cylHeight > 0) ? (f32)actor->colChkInfo.cylHeight : (r * 2.0f);

        box->half.x = r;
        box->half.z = r;
        box->half.y = h * 0.5f;
        box->center.x = actor->world.pos.x;
        box->center.y = actor->world.pos.y + (f32)actor->colChkInfo.cylYShift + (h * 0.5f);
        box->center.z = actor->world.pos.z;
        box->solid = 1;
        return 1;
    }

    // No collision at all. It still gets a nominal box so it has corners to be
    // pinned by, but solid stays 0 and that is what triggers the corners-only rule.
    box->half.x = PACCI_FUSE_NOCOL_HALF;
    box->half.y = PACCI_FUSE_NOCOL_HALF;
    box->half.z = PACCI_FUSE_NOCOL_HALF;
    box->center = actor->world.pos;
    box->center.y += PACCI_FUSE_NOCOL_HALF;
    return 1;
}

// Emit the box's weld points in world space. Sweeping i/j/k over {-1,0,1} produces
// exactly the set we want: all three non-zero is a corner, one zero an edge midpoint,
// two zeros a face centre, all zero the centre.
static u8 Pacci_FuseBoxPoints(PacciFuseBox* box, u8 cornersOnly, Vec3f* out) {
    f32 sin = Math_SinS(box->yaw);
    f32 cos = Math_CosS(box->yaw);
    u8 n = 0;

    for (s32 i = -1; i <= 1; i++) {
        for (s32 j = -1; j <= 1; j++) {
            for (s32 k = -1; k <= 1; k++) {
                f32 lx;
                f32 ly;
                f32 lz;

                // Face centres and the box centre are dropped. They were winning the
                // closest-pair search whenever two objects overlapped, which welded
                // pieces INTO each other instead of against each other. Corners and edge
                // midpoints are the only places two solids can meet and still be
                // touching, which is what the bead is supposed to mark.
                s32 zeros = ((i == 0) ? 1 : 0) + ((j == 0) ? 1 : 0) + ((k == 0) ? 1 : 0);

                if (zeros > (cornersOnly ? 0 : 1)) {
                    continue;
                }
                lx = (f32)i * box->half.x;
                ly = (f32)j * box->half.y;
                lz = (f32)k * box->half.z;

                out[n].x = box->center.x + ((lx * cos) + (lz * sin));
                out[n].y = box->center.y + ly;
                out[n].z = box->center.z + ((lz * cos) - (lx * sin));
                n++;
            }
        }
    }
    return n;
}

// Find the closest weld-point pair between held and target. `weld` comes back as the
// world point they meet at, `snapPos` as where the held actor's origin has to move
// for the two points to coincide.
// Distance between the two BODIES, not between their nearest weld points. Standard box-to-box
// separation: an axis where they overlap contributes nothing, and what is left is how far apart
// they actually are. Yaw is ignored here on purpose - this is a proximity gate, not the solve,
// and an oriented test would only ever move the threshold by a few units.
static f32 Pacci_FuseBoxGap(PacciFuseBox* a, PacciFuseBox* b) {
    f32 gx = fabsf(b->center.x - a->center.x) - (a->half.x + b->half.x);
    f32 gy = fabsf(b->center.y - a->center.y) - (a->half.y + b->half.y);
    f32 gz = fabsf(b->center.z - a->center.z) - (a->half.z + b->half.z);

    if (gx < 0.0f) {
        gx = 0.0f;
    }
    if (gy < 0.0f) {
        gy = 0.0f;
    }
    if (gz < 0.0f) {
        gz = 0.0f;
    }
    return sqrtf((gx * gx) + (gy * gy) + (gz * gz));
}

static u8 Pacci_FuseSolve(PlayState* play, Actor* held, Actor* target, Vec3f* weld, Vec3f* snapPos, Vec3f* outHeldPt,
                          Vec3f* outTargetPt) {
    PacciFuseBox hb;
    PacciFuseBox tb;
    Vec3f hp[PACCI_FUSE_PTS];
    Vec3f tp[PACCI_FUSE_PTS];
    u8 hn;
    u8 tn;
    u8 cornersOnly;
    // No cap on the search itself any more. The closest pair is always found; whether that pair
    // is CLOSE ENOUGH is a separate question, answered below by two gates instead of one.
    f32 best = 3.0e38f;
    s32 bh = -1;
    s32 bt = -1;

    if (!Pacci_FuseGetBox(play, held, &hb) || !Pacci_FuseGetBox(play, target, &tb)) {
        return 0;
    }

    // Something with no collision has no face or edge to lie along, so it may only be
    // pinned corner-to-corner, and only onto real dynapoly.
    cornersOnly = (!hb.solid || !tb.solid);
    if (cornersOnly && !hb.dyna && !tb.dyna) {
        return 0;
    }

    hn = Pacci_FuseBoxPoints(&hb, cornersOnly, hp);
    tn = Pacci_FuseBoxPoints(&tb, cornersOnly, tp);

    for (u8 i = 0; i < hn; i++) {
        for (u8 j = 0; j < tn; j++) {
            f32 dx = tp[j].x - hp[i].x;
            f32 dy = tp[j].y - hp[i].y;
            f32 dz = tp[j].z - hp[i].z;
            f32 d = (dx * dx) + (dy * dy) + (dz * dz);

            if (d < best) {
                best = d;
                bh = i;
                bt = j;
            }
        }
    }
    if (bh < 0) {
        return 0;
    }
    // TWO gates, and either one is enough.
    //
    // The weld-point test alone is what made the offer feel dead: only corners and edge
    // midpoints are candidates, so two large boxes can be visibly touching - resting against
    // each other, even overlapping - while their nearest CORNER pair is still most of a box
    // apart and nothing is offered. Asking how far apart the bodies are instead answers the
    // question the player is actually asking, which is "are these two things next to each
    // other". The point test still earns its keep for small objects held out at arm's length,
    // where the bodies are far apart but you are lining a corner up precisely.
    if ((best > (PACCI_FUSE_WELD_RANGE * PACCI_FUSE_WELD_RANGE)) &&
        (Pacci_FuseBoxGap(&hb, &tb) > PACCI_FUSE_NEAR_GAP)) {
        return 0;
    }

    *weld = tp[bt];
    snapPos->x = held->world.pos.x + (tp[bt].x - hp[bh].x);
    snapPos->y = held->world.pos.y + (tp[bt].y - hp[bh].y);
    snapPos->z = held->world.pos.z + (tp[bt].z - hp[bh].z);

    // Hand back the pair itself, not just where it ends up, so the preview can mark
    // both objects.
    if (outHeldPt != NULL) {
        *outHeldPt = hp[bh];
    }
    if (outTargetPt != NULL) {
        *outTargetPt = tp[bt];
    }
    return 1;
}

// ── preview ──────────────────────────────────────────────────────────────────

u8 Pacci_FusePreviewValid(void) {
    return sFusePv.valid;
}

// Every frame you are holding something: work out whether a weld is on offer and
// where. Must run after the carry has moved the held object, or the bead lags a
// frame behind the thing it is supposed to be touching.
void Pacci_FuseUpdatePreview(PlayState* play, Player* player) {
    Actor* held = sUltrahand.held;
    Actor* target;

    sFusePv.valid = 0;
    sFusePv.target = NULL;

    if ((held == NULL) || (held->update == NULL)) {
        return;
    }
    if (sFuse.count >= PACCI_FUSE_MAX_PARTS) {
        return;
    }
    // One assembly at a time: if a structure already exists and this is not its root,
    // welding to it would leave the old one with nothing driving it.
    if ((sFuse.count > 0) && (sFuse.root != held)) {
        return;
    }

    target = Pacci_ResolveUltrahandTarget(play, player);
    if ((target == NULL) || (target == held) || (target->update == NULL) || Pacci_FuseIsPart(target)) {
        return;
    }
    if (!Pacci_FuseSolve(play, held, target, &sFusePv.weld, &sFusePv.snapPos, &sFusePv.heldPt, &sFusePv.targetPt)) {
        return;
    }

    sFusePv.valid = 1;
    sFusePv.target = target;
    // BOTH ends light up, so the offer reads as a pair about to join rather than as
    // one more thing you could grab.
    // BOTH ends light up, so the offer reads as a pair about to join rather than as one more
    // thing you could grab.
    Pacci_UhTintAdd(target);
    Pacci_UhTintAdd(held);
}

// Commit the previewed weld. Returns 0 if there was nothing on offer, which is what
// lets A fall through to "drop".
// Register an ALREADY-POSITIONED part on the root at a local offset it is handed, instead of
// one measured from where the solve put the two objects. That is what rebuilding a stored
// structure needs: it knows the shape and has to reproduce it exactly, and there is no aim,
// no preview and no weld point involved. The caller merges the collision once, at the end.
static u8 Pacci_FuseAdopt(Actor* root, Actor* part, Vec3f* localOff, Vec3s* relRot) {
    PacciFusePart* slot;

    if ((root == NULL) || (part == NULL) || (part->update == NULL) || (sFuse.count >= PACCI_FUSE_MAX_PARTS)) {
        return 0;
    }
    slot = &sFuse.parts[sFuse.count];
    sFuse.root = root;
    slot->actor = part;
    slot->offset = *localOff;
    slot->rot = *relRot;
    slot->weldLocal = *localOff; // no solve ran, so the bead marks the piece itself
    slot->origUpdate = part->update;
    slot->origFlags = part->flags;
    slot->origGravity = part->gravity;
    slot->origRoom = part->room;
    sFuse.count++;

    Pacci_AnchorTake(part);
    part->update = Pacci_FusePartUpdate;
    part->gravity = 0.0f;
    part->velocity.x = 0.0f;
    part->velocity.y = 0.0f;
    part->velocity.z = 0.0f;
    part->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    return 1;
}

u8 Pacci_FuseTryAttach(PlayState* play) {
    Actor* root = sUltrahand.held;
    Actor* part = sFusePv.target;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 sin;
    f32 cos;
    PacciFusePart* slot;

    if (!sFusePv.valid || (root == NULL) || (part == NULL) || (part->update == NULL)) {
        return 0;
    }
    if (sFuse.count >= PACCI_FUSE_MAX_PARTS) {
        return 0;
    }
    // Same rule from the other side: welding onto something that is not the current structure's
    // root starts a NEW structure. Appending to the old part list would leave pieces whose
    // offsets describe a root that is no longer driving them.
    if ((sFuse.count > 0) && (sFuse.root != root)) {
        Pacci_FuseRelease();
    }

    // Snap the held piece onto the weld point BEFORE the offset is measured, so what
    // gets recorded is the aligned pose and not where your aim happened to be.
    root->world.pos = sFusePv.snapPos;
    root->prevPos = root->world.pos;

    dx = part->world.pos.x - root->world.pos.x;
    dy = part->world.pos.y - root->world.pos.y;
    dz = part->world.pos.z - root->world.pos.z;

    // World delta -> the root's local frame, so the part keeps its relative place when
    // the root turns. Negative angle because this is the inverse rotation.
    sin = Math_SinS(-root->shape.rot.y);
    cos = Math_CosS(-root->shape.rot.y);

    slot = &sFuse.parts[sFuse.count];
    sFuse.root = root;
    slot->actor = part;
    slot->offset.x = (dx * cos) + (dz * sin);
    slot->offset.y = dy;
    slot->offset.z = (dz * cos) - (dx * sin);
    slot->rot.x = part->shape.rot.x - root->shape.rot.x;
    slot->rot.y = part->shape.rot.y - root->shape.rot.y;
    slot->rot.z = part->shape.rot.z - root->shape.rot.z;
    // Square. The weld already snaps the two bodies together at a corner or an edge, and then let
    // them sit at whatever tilt the carry happened to have - so a structure came out looking like
    // it had been dropped rather than built. Only the tilt is taken from the target; the yaw stays
    // whatever you chose, because turning a piece before sticking it on is a real decision and the
    // pitch and roll of a carried object almost never are.
    root->shape.rot.x = part->shape.rot.x;
    root->shape.rot.z = part->shape.rot.z;
    root->world.rot = root->shape.rot;
    sUltrahand.baseRot = root->shape.rot;
    slot->rot.x = 0;
    slot->rot.z = 0;
    {
        f32 wx = sFusePv.weld.x - root->world.pos.x;
        f32 wy = sFusePv.weld.y - root->world.pos.y;
        f32 wz = sFusePv.weld.z - root->world.pos.z;

        slot->weldLocal.x = (wx * cos) + (wz * sin);
        slot->weldLocal.y = wy;
        slot->weldLocal.z = (wz * cos) - (wx * sin);
    }
    slot->origUpdate = part->update;
    slot->origFlags = part->flags;
    slot->origGravity = part->gravity;
    slot->origRoom = part->room;
    sFuse.count++;

    Pacci_AnchorTake(part);
    part->update = Pacci_FusePartUpdate;
    part->gravity = 0.0f;
    part->velocity.x = 0.0f;
    part->velocity.y = 0.0f;
    part->velocity.z = 0.0f;
    // Culling would stop the part's transform being driven while the root is still on
    // screen, and the piece would be left behind mid-air.
    part->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;

    sFusePv.valid = 0;
    sFusePv.target = NULL;

    Audio_PlaySoundGeneral(NA_SE_SY_GET_ITEM, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    return 1;
}

// Detach by shaking the stick left and right, the closest a controller with one stick gets to
// TotK's right-stick wiggle. Aiming at one piece takes that piece off; aiming at nothing in
// particular dissolves the whole structure.
//
// FLIPS are what is counted, not deflection: the stick is still steering Link, so any threshold
// on "how far left" would fire every time he ran sideways. Three reversals inside the window is
// something you can only do on purpose.
void Pacci_FuseWiggleDetach(PlayState* play, Player* player, Input* input) {
    s8 x = input->cur.stick_x;
    s8 dir = (x > 40) ? 1 : ((x < -40) ? -1 : 0);

    if (sUhMode.wiggleTimer > 0) {
        sUhMode.wiggleTimer--;
    } else {
        sUhMode.wiggleFlips = 0;
        sUhMode.wiggleDir = 0;
    }

    if (dir != 0) {
        if ((sUhMode.wiggleDir != 0) && (dir != sUhMode.wiggleDir)) {
            sUhMode.wiggleFlips++;
        }
        sUhMode.wiggleDir = dir;
        sUhMode.wiggleTimer = PACCI_UH_WIGGLE_WINDOW;
    }

    if (sUhMode.wiggleFlips < PACCI_UH_WIGGLE_FLIPS) {
        return;
    }
    sUhMode.wiggleFlips = 0;
    sUhMode.wiggleDir = 0;
    sUhMode.wiggleTimer = 0;

    if (sFuse.count == 0) {
        return;
    }
    if (!Pacci_FuseDetachPart(Pacci_ResolveUltrahandTarget(play, player))) {
        Pacci_FuseRelease();
    }
    Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// Hold R to come apart. Aiming at one piece takes that piece off; aiming at nothing
// in particular dissolves the whole structure.
void Pacci_FuseHoldDetach(PlayState* play, Player* player, u8 rHeld) {
    if (!rHeld) {
        sFuseDetachHold = 0;
        return;
    }
    sFuseDetachHold++;
    if (sFuseDetachHold != PACCI_FUSE_DETACH_HOLD) {
        return; // fires once per hold, not every frame it stays down
    }
    if (sFuse.count == 0) {
        return;
    }
    if (!Pacci_FuseDetachPart(Pacci_ResolveUltrahandTarget(play, player))) {
        Pacci_FuseRelease();
    }
    Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// Rebuild every part from the root. Must run AFTER the root has been positioned.
void Pacci_FuseFollow(PlayState* play) {
    Actor* root = sFuse.root;
    u8 lit;

    if ((root == NULL) || (sFuse.count == 0)) {
        return;
    }
    if (root->update == NULL) { // root died — the structure is not a structure any more
        // The parts outlived it and nothing drives them any more, so hand them back to
        // themselves rather than leaving them frozen for the rest of the scene.
        Pacci_FuseRelease();
        return;
    }

    // Only while the assembly is in hand. This also runs from CustomItems_Update long after a
    // structure was set down, and a permanently glowing pile of crates is not the effect.
    lit = Pacci_IsHoldingUltrahand() && (root == sUltrahand.held);
    for (u8 i = 0; i < sFuse.count; i++) {
        Actor* part = sFuse.parts[i].actor;

        if ((part == NULL) || (part->update == NULL)) {
            // It died anyway - broken, burned, despawned by something we do not control. Drop it
            // from the assembly rather than carrying a hole around: a dead slot keeps its anchor
            // alive and makes Pacci_FuseCount lie about how much room is left.
            if (part != NULL) {
                Pacci_AnchorRelease(part);
                sFuse.parts[i].actor = NULL;
            }
            continue;
        }
        // Same helper the part's own update calls, so there is exactly one definition of where
        // a piece belongs. This pass still matters: it catches the pieces whose category
        // updates BEFORE the root moved, which would otherwise sit one frame behind.
        Pacci_FusePlacePart(play, &sFuse.parts[i], root);
        if (lit) {
            Pacci_UhTintAdd(part);
        }
    }
}

// ── the bead ─────────────────────────────────────────────────────────────────

// An octahedron, not a cube: at bead size the silhouette is all you read, and eight
// faces round off where six would show corners.
static Vtx sFuseBeadVtx[] = {
    VTX(0, 1, 0, 0, 0, 0, 0, 0, 255),  VTX(0, -1, 0, 0, 0, 0, 0, 0, 255), VTX(1, 0, 0, 0, 0, 0, 0, 0, 255),
    VTX(-1, 0, 0, 0, 0, 0, 0, 0, 255), VTX(0, 0, 1, 0, 0, 0, 0, 0, 255),  VTX(0, 0, -1, 0, 0, 0, 0, 0, 255),
};

static Gfx sFuseBeadDL[] = {
    gsSPVertex(sFuseBeadVtx, 6, 0),         gsSP2Triangles(0, 4, 2, 0, 0, 2, 5, 0),
    gsSP2Triangles(0, 5, 3, 0, 0, 3, 4, 0), gsSP2Triangles(1, 2, 4, 0, 1, 5, 2, 0),
    gsSP2Triangles(1, 3, 5, 0, 1, 4, 3, 0), gsSPEndDisplayList(),
};

// -- placing a weight on a floor switch ----------------------------------------
// The same offer/commit gesture as a weld, aimed at the one job every heavy object in this game
// exists for. Line a block up on a switch by hand and you are fighting the carry's own easing for
// the last few units; the switch is a fixed point, so it can simply be snapped to.
//
// It is a PSEUDO attach: nothing is fused, nothing is driven afterwards. A commits the placement
// and lets go, and from there the block is an ordinary block sitting on an ordinary switch.
//
// WHICH SWITCHES ACTUALLY LATCH is vanilla's business, not ours, and it is worth knowing before
// this looks broken: ObjSwitch_FloorUp (z_obj_switch.c:390-415) only consults
// DynaPolyActor_IsSwitchPressed for subtypes FLOOR_2 and FLOOR_3, the hold-down kind. Subtypes
// FLOOR_0 and FLOOR_1 ask DynaPolyActor_IsPlayerOnTop - a block will never press those, and
// neither will anything else that is not Link - and FLOOR_RUSTY wants an AC hit from a hammer.
// The offer is made on every floor switch anyway, because "put this exactly there" is useful even
// where the switch will not answer.
// How close the ANCHOR - where the controls are asking for, not where the body slid to - has to be
// for the offer. Generous, because the offer flickering on and off leaves the body hovering
// halfway: the magnet ramps both ways, so an offer that keeps expiring never finishes arriving.
#define PACCI_PLACE_RANGE 140.0f

static struct {
    u8 valid;
    Actor* sw;
    Vec3f pos; // where the held actor's ORIGIN goes
} sPlacePv = { 0, NULL, { 0.0f, 0.0f, 0.0f } };

u8 Pacci_PlaceOfferValid(void) {
    return sPlacePv.valid;
}

// The things a floor switch pulls at. A NAMED list, not ACTOR_FLAG_CAN_PRESS_SWITCHES, and the
// investigation behind that is worth writing down because the flag reads like the right answer.
//
// In the whole of vanilla OoT exactly two actors carry that flag: En_Am and Obj_Kibako. (En_Ru1
// has it as well, which is the Jabu-Jabu switch puzzle, and En_Partner is this fork's own.) That
// is not the same as "only those can press a switch" - Obj_Oshihiki presses one perfectly well by
// calling SetSwitchPressed itself (z_obj_oshihiki.c:510-513) - it only means the flag answers a
// narrower question than it appears to.
//
// Either way it is the wrong question here. What belongs in this list is what should be PULLED
// toward a switch, and nothing in the engine has an opinion about that.
//
// A block, an armos, a crate of either size, and a Somaria statue. Everything else keeps the free
// three-dimensional carry and is never dragged anywhere - the whole reason the general "set it
// down" magnet was taken out.
static u8 Pacci_PlaceIsWeight(Actor* actor) {
    if (actor == NULL) {
        return 0;
    }
    // A Somaria statue is one of ours and has no id of its own to test - the pool knows.
    // somaria_cubes.h is already in this translation unit; it is included above cane_pacci.c.
    if (SomariaCube_IsSomariaCube(actor)) {
        return 1;
    }
    return (actor->id == ACTOR_OBJ_OSHIHIKI) || (actor->id == ACTOR_EN_AM) || (actor->id == ACTOR_OBJ_KIBAKO) ||
           (actor->id == ACTOR_OBJ_KIBAKO2);
}

static u8 Pacci_PlaceIsFloorSwitch(Actor* actor) {
    s32 type;

    if ((actor == NULL) || (actor->update == NULL) || (actor->id != ACTOR_OBJ_SWITCH)) {
        return 0;
    }
    // Literals rather than ObjSwitchType: that enum lives in the overlay's own header, and this
    // file is compiled into the player's translation unit, which has no business including it.
    // z_obj_switch.c:12-17 - 0 is FLOOR, 1 is FLOOR_RUSTY, and the type is params & 7 (:14).
    type = actor->params & 7;
    return (type == 0) || (type == 1);
}

// The world Y of the TOP of a dynapoly actor's own collision bounds, or 0 with a 0 return if it
// has none. Used to sit a body exactly on a floor switch's plate.
static u8 Pacci_UhDynaTopY(PlayState* play, Actor* actor, f32* outY) {
    s32 i;

    if ((play == NULL) || (actor == NULL)) {
        return 0;
    }
    for (i = 0; i < BG_ACTOR_MAX; i++) {
        BgActor* bg = &play->colCtx.dyna.bgActors[i];

        if ((bg->actor != actor) || (bg->colHeader == NULL)) {
            continue;
        }
        *outY = actor->world.pos.y + ((f32)bg->colHeader->maxBounds.y * actor->scale.y);
        return 1;
    }
    return 0;
}

// A FLOOR SWITCH IS THE ONLY THING THAT PULLS. There used to be a general "set it down flush on
// whatever is under you" offer here as well, and once the magnet arrived it turned every object
// into something that wanted to fall to the floor: the offer fires whenever a body is within a
// hundred-odd units above its resting place, which is most of the time you are carrying anything,
// so the magnet was quietly dragging everything down. It took away exactly the thing Ultrahand is
// for, which is holding something wherever you want it.
//
// A switch is a small, deliberate target you had to line up on anyway. Everything else is left
// alone, and freedom is the default.

void Pacci_PlaceUpdatePreview(PlayState* play) {
    Actor* held = sUltrahand.held;
    PacciFuseBox box;
    Actor* it;
    f32 best = PACCI_PLACE_RANGE * PACCI_PLACE_RANGE;

    sPlacePv.valid = 0;
    sPlacePv.sw = NULL;
    if ((play == NULL) || (held == NULL) || sUltrahand.dropping) {
        return;
    }
    // The weld offer wins if there is one: gluing is the deliberate act, placing is the
    // convenience, and one A press cannot mean both.
    if (sFusePv.valid) {
        return;
    }
    if (!Pacci_FuseGetBox(play, held, &box)) {
        return;
    }
    if (!Pacci_PlaceIsWeight(held)) {
        return;
    }

    // Walking ACTORCAT_SWITCH directly rather than through the shared target selector: this is
    // not a thing you aim at, it is a thing you are near, and the selector answers the wrong
    // question. Distance is measured XZ only, from the body's CENTRE - a block held above a
    // switch is lined up with it however high you are holding it.
    for (it = play->actorCtx.actorLists[ACTORCAT_SWITCH].head; it != NULL; it = it->next) {
        f32 dx;
        f32 dz;
        f32 d;

        if (!Pacci_PlaceIsFloorSwitch(it)) {
            continue;
        }
        // From the ANCHOR, not the body - see Pacci_UhPlaceMagnet. Once the block has slid onto
        // the plate, the body is at zero distance forever and only the anchor can say you have
        // moved on.
        dx = it->world.pos.x - sUltrahand.anchorPos.x;
        dz = it->world.pos.z - sUltrahand.anchorPos.z;
        d = (dx * dx) + (dz * dz);
        if (d < best) {
            best = d;
            sPlacePv.sw = it;
        }
    }
    if (sPlacePv.sw == NULL) {
        return;
    }

    // Centred on the switch, with its bottom face on the PLATE'S OWN TOP.
    //
    // This is why a placed block pressed nothing. A floor switch latches when the engine reports
    // something standing on its dynapoly (DynaPolyActor_IsSwitchPressed), and that means genuinely
    // resting on the plate's collision surface. An Obj_Switch's origin is not the top of its plate,
    // so a body placed relative to the origin sat slightly inside it or slightly above, and either
    // way nothing was standing on anything.
    //
    // Read from the plate's own collision bounds rather than probed with a ray. The ray would have
    // been the obvious reuse and it is wrong here: Pacci_UhGroundUnder starts its cast one unit
    // BELOW the body it is testing, so once the block is already resting on the plate the ray
    // starts inside the plate, misses its top face and reports the floor underneath - and the
    // block would sink off the switch it had just landed on.
    sPlacePv.pos.x = sPlacePv.sw->world.pos.x;
    sPlacePv.pos.z = sPlacePv.sw->world.pos.z;
    {
        f32 plateTop;

        if (Pacci_UhDynaTopY(play, sPlacePv.sw, &plateTop)) {
            // Bottom face on the plate's top face, minus a hair. The body's ORIGIN then sits that
            // far above it, since the origin is not the bottom of the box.
            //
            // The hair matters. Resting EXACTLY on a surface is ambiguous to the engine: standing
            // is decided by the body's Y against the floor height under it, and a body placed at
            // precisely that height lands on whichever side of the comparison the float rounding
            // puts it. A unit of overlap is invisible and makes the answer always the same one.
            sPlacePv.pos.y = plateTop - PACCI_PLACE_SINK + (held->world.pos.y - (box.center.y - box.half.y));
        } else {
            sPlacePv.pos.y = sPlacePv.sw->world.pos.y + (held->world.pos.y - (box.center.y - box.half.y));
        }
    }
    sPlacePv.valid = 1;
}

static void Pacci_FuseDrawBead(PlayState* play, Vec3f* pos, f32 scale, u8 pulsing) {
    // Two passes: a soft halo with a brighter core inside it. One flat blob does not
    // read as glowing, it reads as a green rock.
    f32 pulse = pulsing ? (0.85f + (0.15f * Math_SinS((s16)(play->gameplayFrames * 2200)))) : 1.0f;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);

    // Halo. Components spelled out: MSVC hands a multi-value #define to a
    // function-like macro as ONE argument, so a packed colour would not expand.
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_Scale(scale * 1.9f * pulse, scale * 1.9f * pulse, scale * 1.9f * pulse, MTXMODE_APPLY);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, PACCI_ZONAI_GLOW_R, PACCI_ZONAI_GLOW_G, PACCI_ZONAI_GLOW_B, 90);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, sFuseBeadDL);

    // Core.
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_Scale(scale * pulse, scale * pulse, scale * pulse, MTXMODE_APPLY);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, PACCI_ZONAI_CORE_R, PACCI_ZONAI_CORE_G, PACCI_ZONAI_CORE_B, 235);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, sFuseBeadDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// Zonai marks on a whole object: a small bead on each of its eight box corners.
//
// This exists because the engine's colour filter CANNOT do green — OoT offers white (0x8000),
// red (0x4000) and blue (0), MM adds grey, and that is the whole palette. A Zonai read therefore
// has to be geometry rather than a tint, so the corners of the box the weld solver is already
// using get lit up instead.
static void Pacci_FuseDrawBoxMarks(PlayState* play, Actor* actor) {
    PacciFuseBox box;
    Vec3f pts[PACCI_FUSE_PTS];
    u8 n;

    if ((actor == NULL) || (actor->update == NULL)) {
        return;
    }
    if (!Pacci_FuseGetBox(play, actor, &box)) {
        return;
    }
    n = Pacci_FuseBoxPoints(&box, 1, pts); // corners only: 8 marks, not the full 20
    for (u8 i = 0; i < n; i++) {
        Pacci_FuseDrawBead(play, &pts[i], 3.0f, 0);
    }
}

// One bead per existing joint, plus a pulsing one on the weld being offered.
void Pacci_FuseDrawPreview(PlayState* play) {
    Actor* root = sFuse.root;

    if ((root != NULL) && (root->update != NULL)) {
        f32 sin = Math_SinS(root->shape.rot.y);
        f32 cos = Math_CosS(root->shape.rot.y);

        for (u8 i = 0; i < sFuse.count; i++) {
            Vec3f* w = &sFuse.parts[i].weldLocal;
            Vec3f pos;

            if ((sFuse.parts[i].actor == NULL) || (sFuse.parts[i].actor->update == NULL)) {
                continue;
            }
            pos.x = root->world.pos.x + ((w->x * cos) + (w->z * sin));
            pos.y = root->world.pos.y + w->y;
            pos.z = root->world.pos.z + ((w->z * cos) - (w->x * sin));
            Pacci_FuseDrawBead(play, &pos, 6.0f, 0);
        }
    }

    if (sPlacePv.valid) {
        Vec3f mark = sPlacePv.pos;

        // One bead where it will land and the body outlined, the same vocabulary the weld offer
        // uses, so "A will do something here" reads the same way in all three cases.
        Pacci_FuseDrawBoxMarks(play, sUltrahand.held);
        if (sPlacePv.sw != NULL) {
            mark.y = sPlacePv.sw->world.pos.y;
        }
        Pacci_FuseDrawBead(play, &mark, 8.0f, 1);
    }

    if (sFusePv.valid) {
        // One bead on EACH object, so the pairing is legible before you commit: the
        // corner of the thing in your hands, the corner of the thing it will stick
        // to, and a dotted run between them for the move about to happen.
        // Outline BOTH bodies in Zonai corners first, under the joint beads: the engine has no
        // green colour filter, so "these two are what is about to join" has to be drawn.
        Pacci_FuseDrawBoxMarks(play, sUltrahand.held);
        Pacci_FuseDrawBoxMarks(play, sFusePv.target);
        Pacci_FuseDrawBead(play, &sFusePv.heldPt, 6.0f, 1);
        Pacci_FuseDrawBead(play, &sFusePv.targetPt, 7.0f, 1);

        {
            Vec3f step;
            f32 t;

            for (t = 0.2f; t < 0.99f; t += 0.2f) {
                step.x = sFusePv.heldPt.x + ((sFusePv.targetPt.x - sFusePv.heldPt.x) * t);
                step.y = sFusePv.heldPt.y + ((sFusePv.targetPt.y - sFusePv.heldPt.y) * t);
                step.z = sFusePv.heldPt.z + ((sFusePv.targetPt.z - sFusePv.heldPt.z) * t);
                Pacci_FuseDrawBead(play, &step, 2.5f, 0);
            }
        }
    }
}

// ============================================================================
// ULTRAHAND VFX + WORLD-SPACE CONTROL GIZMO
// ============================================================================

#define PACCI_UH_VFX_POINTS 13
#define PACCI_UH_GIZMO_SEGMENTS 20
// Thickness of the solid gizmo parts, in world units.
#define PACCI_UH_GIZMO_SHAFT_R 3.6f
#define PACCI_UH_GIZMO_HEAD_R 10.5f
#define PACCI_UH_GIZMO_RING_R 3.0f
// The energy wave: how many pulses ride the tether at once, how many frames one takes
// to travel it end to end, how much of the tether a single pulse covers (0..1), and the
// tube's resting / peak radius.
// Colours lifted from the Blender materials rather than picked by eye.
//   "Selected object outline"  (0.005, 0.92, 0.20)
//   "Contact rings"            (0.01,  0.95, 0.28)
//   the held tint's ramp, dark end (0.015, 0.22, 0.09)
// The point light the held body casts. The radius is generous on purpose: the reference's
// bounce light reaches the floor and the walls, and a tight radius just makes a green dot.
#define PACCI_UH_LIGHT_R 55
#define PACCI_UH_LIGHT_G 255
#define PACCI_UH_LIGHT_B 160
#define PACCI_UH_LIGHT_RADIUS 340

// -- solid gizmo geometry ----------------------------------------------------
// The gizmo is real geometry, not beam sprites. Maya and Blender draw their move
// and rotate handles as opaque shafts, cones and bands, and that reading is the
// whole point of a gizmo: a solid arrow says "this axis moves", a spiral beam
// says "magic is happening". Both primitives are unit-sized at 100 model units
// and run along LOCAL +Y, so a caller scales by (radius/100, length/100,
// radius/100) and shares the beams' own orientation maths.

static Vtx sPacciUhPrismVtx[] = {
    VTX(100, 0, 0, 0, 0, 0, 0, 0, 255),    VTX(71, 0, 71, 0, 0, 0, 0, 0, 255),
    VTX(0, 0, 100, 0, 0, 0, 0, 0, 255),    VTX(-71, 0, 71, 0, 0, 0, 0, 0, 255),
    VTX(-100, 0, 0, 0, 0, 0, 0, 0, 255),   VTX(-71, 0, -71, 0, 0, 0, 0, 0, 255),
    VTX(0, 0, -100, 0, 0, 0, 0, 0, 255),   VTX(71, 0, -71, 0, 0, 0, 0, 0, 255),
    VTX(100, 100, 0, 0, 0, 0, 0, 0, 255),  VTX(71, 100, 71, 0, 0, 0, 0, 0, 255),
    VTX(0, 100, 100, 0, 0, 0, 0, 0, 255),  VTX(-71, 100, 71, 0, 0, 0, 0, 0, 255),
    VTX(-100, 100, 0, 0, 0, 0, 0, 0, 255), VTX(-71, 100, -71, 0, 0, 0, 0, 0, 255),
    VTX(0, 100, -100, 0, 0, 0, 0, 0, 255), VTX(71, 100, -71, 0, 0, 0, 0, 0, 255),
};

static Gfx sPacciUhPrismDL[] = {
    gsSPVertex(sPacciUhPrismVtx, 16, 0),       gsSP2Triangles(0, 1, 9, 0, 0, 9, 8, 0),
    gsSP2Triangles(1, 2, 10, 0, 1, 10, 9, 0),  gsSP2Triangles(2, 3, 11, 0, 2, 11, 10, 0),
    gsSP2Triangles(3, 4, 12, 0, 3, 12, 11, 0), gsSP2Triangles(4, 5, 13, 0, 4, 13, 12, 0),
    gsSP2Triangles(5, 6, 14, 0, 5, 14, 13, 0), gsSP2Triangles(6, 7, 15, 0, 6, 15, 14, 0),
    gsSP2Triangles(7, 0, 8, 0, 7, 8, 15, 0),   gsSPEndDisplayList(),
};

static Vtx sPacciUhConeVtx[] = {
    VTX(0, 100, 0, 0, 0, 0, 0, 0, 255),   VTX(100, 0, 0, 0, 0, 0, 0, 0, 255),  VTX(71, 0, 71, 0, 0, 0, 0, 0, 255),
    VTX(0, 0, 100, 0, 0, 0, 0, 0, 255),   VTX(-71, 0, 71, 0, 0, 0, 0, 0, 255), VTX(-100, 0, 0, 0, 0, 0, 0, 0, 255),
    VTX(-71, 0, -71, 0, 0, 0, 0, 0, 255), VTX(0, 0, -100, 0, 0, 0, 0, 0, 255), VTX(71, 0, -71, 0, 0, 0, 0, 0, 255),
};

static Gfx sPacciUhConeDL[] = {
    gsSPVertex(sPacciUhConeVtx, 9, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(0, 3, 4, 0, 0, 4, 5, 0),
    gsSP2Triangles(0, 5, 6, 0, 0, 6, 7, 0),
    gsSP2Triangles(0, 7, 8, 0, 0, 8, 1, 0),
    gsSP2Triangles(1, 2, 3, 0, 1, 3, 4, 0),
    gsSP2Triangles(1, 4, 5, 0, 1, 5, 6, 0),
    gsSP2Triangles(1, 6, 7, 0, 1, 7, 8, 0),
    gsSPEndDisplayList(),
};
static void Pacci_UhGetHandPos(Player* player, Vec3f* pos) {
    Vec3f forearm = player->bodyPartsPos[PLAYER_BODYPART_R_FOREARM];
    Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    f32 dx = hand.x - forearm.x;
    f32 dy = hand.y - forearm.y;
    f32 dz = hand.z - forearm.z;
    f32 length = sqrtf((dx * dx) + (dy * dy) + (dz * dz));

    *pos = hand;
    if (length > 0.001f) {
        pos->x += (dx / length) * 10.0f;
        pos->y += (dy / length) * 10.0f;
        pos->z += (dz / length) * 10.0f;
    }
}

static void Pacci_UhGetStreamPoint(Vec3f* point, Vec3f* start, Vec3f* end, f32 t, s16 phase, u32 frame) {
    f32 dx = end->x - start->x;
    f32 dz = end->z - start->z;
    f32 xzLength = sqrtf((dx * dx) + (dz * dz));
    f32 envelope = Math_SinS((s16)(t * 0x7FFF));
    f32 sideX = 1.0f;
    f32 sideZ = 0.0f;
    s16 broadWave = (s16)((t * 0x6000) + (frame * 0x0180) + phase);
    s16 fineWave = (s16)((t * 0xE000) - (frame * 0x00C0) + (phase >> 1));
    f32 flutter = ((Math_SinS(broadWave) * 0.65f) + (Math_SinS(fineWave) * 0.35f)) * envelope;

    if (xzLength > 0.001f) {
        sideX = dz / xzLength;
        sideZ = -dx / xzLength;
    }
    point->x = start->x + ((end->x - start->x) * t) + (sideX * flutter * 3.8f);
    point->y = start->y + ((end->y - start->y) * t) + (envelope * 10.0f) + (Math_CosS(broadWave) * envelope * 1.8f);
    point->z = start->z + ((end->z - start->z) * t) + (sideZ * flutter * 3.8f);
}

// Flat, untextured, double-sided state for the solid primitives. Hoisted out of the
// per-part draw because a rotation band is twenty-odd segments: re-sending the
// combiner and the geometry mode for each one is the difference between three
// display-list commands per segment and ten.
//
// The combiner reads PRIMITIVE in every slot on purpose. Nothing binds a texture for
// these, so leaving TEXEL0 in the equation samples whatever the previous pass happened
// to leave loaded, which is how a solid arrow ends up striped with the tether's noise.
static void Pacci_UhSolidBegin(Gfx** gfxP) {
    Gfx* gfx = *gfxP;

    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gSPClearGeometryMode(gfx++, G_LIGHTING | G_CULL_BACK);
    *gfxP = gfx;
}

static void Pacci_UhSolidEnd(Gfx** gfxP) {
    Gfx* gfx = *gfxP;

    gSPSetGeometryMode(gfx++, G_LIGHTING | G_CULL_BACK);
    *gfxP = gfx;
}

// Draw one solid primitive spanning start -> end. Must sit between a Begin/End pair.
static void Pacci_UhDrawSolid(PlayState* play, Gfx** gfxP, Gfx* dl, Vec3f* start, Vec3f* end, f32 radius, u8 r, u8 g,
                              u8 b, u8 alpha) {
    f32 dx = end->x - start->x;
    f32 dy = end->y - start->y;
    f32 dz = end->z - start->z;
    f32 xzLength = sqrtf((dx * dx) + (dz * dz));
    f32 length = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
    Gfx* gfx = *gfxP;

    if (length < 0.1f) {
        return;
    }
    Matrix_Translate(start->x, start->y, start->z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(Math_Vec3f_Yaw(start, end)), MTXMODE_APPLY);
    Matrix_RotateX(atan2f(xzLength, dy), MTXMODE_APPLY);
    Matrix_Scale(radius / 100.0f, length / 100.0f, radius / 100.0f, MTXMODE_APPLY);
    gDPSetPrimColor(gfx++, 0, 0, r, g, b, alpha);
    gSPMatrix(gfx++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(gfx++, dl);
    *gfxP = gfx;
}

// -- the light -----------------------------------------------------------------
// "hasta luz": the composition lights the scene from the object, not just draws on top of
// it, and that is most of why it reads as energy. A real point light in the scene's light
// context does the same thing here - Link, the floor and everything the structure passes
// get the green spill for free, with no extra geometry.
static LightInfo sUhLightInfo;
static LightNode* sUhLightNode = NULL;
// Which scene the node was inserted in. A scene change reinitialises the whole light
// context, so the node we are holding is freed underneath us: removing it then would be a
// write into a reused list. Comparing the scene lets us drop the pointer instead.
static s16 sUhLightScene = -1;

static void Pacci_UhLightOff(PlayState* play) {
    if (sUhLightNode != NULL) {
        if ((play != NULL) && (play->sceneNum == sUhLightScene)) {
            LightContext_RemoveLight(play, &play->lightCtx, sUhLightNode);
        }
        sUhLightNode = NULL;
        sUhLightScene = -1;
    }
}

static void Pacci_UhLightAt(PlayState* play, Vec3f* pos, f32 strength) {
    // Breathes with the same period as the contact rings, so light and geometry pulse
    // together instead of drifting in and out of phase with each other.
    f32 pulse = strength * (0.86f + (Math_SinS((s16)(play->gameplayFrames * 0x0500)) * 0.14f));

    Lights_PointNoGlowSetInfo(&sUhLightInfo, (s16)pos->x, (s16)pos->y, (s16)pos->z, (u8)(PACCI_UH_LIGHT_R * pulse),
                              (u8)(PACCI_UH_LIGHT_G * pulse), (u8)(PACCI_UH_LIGHT_B * pulse), PACCI_UH_LIGHT_RADIUS);
    if (sUhLightNode == NULL) {
        sUhLightNode = LightContext_InsertLight(play, &play->lightCtx, &sUhLightInfo);
        sUhLightScene = play->sceneNum;
    }
}

static void Pacci_UhDrawFlowPass(PlayState* play, Gfx** gfxP, Vec3f* hand, Vec3f* target, f32 reach, s16 phase,
                                 f32 width, u8 r, u8 g, u8 b, u8 alpha) {
    Vec3f points[PACCI_UH_VFX_POINTS];
    u8 i;

    for (i = 0; i < PACCI_UH_VFX_POINTS; i++) {
        f32 t = ((f32)i / (PACCI_UH_VFX_POINTS - 1)) * reach;

        Pacci_UhGetStreamPoint(&points[i], hand, target, t, phase, play->gameplayFrames);
    }
    // One Begin/End for the whole strand: twelve segments at three commands each instead of
    // twelve full state changes.
    Pacci_UhSolidBegin(gfxP);
    for (i = 0; i < PACCI_UH_VFX_POINTS - 1; i++) {
        // Fat in the middle, tapering to nothing at both ends, so the strand is born at the
        // hand and dies into the object rather than being cut off flat.
        f32 edge = Math_SinS((s16)((i * 0x7FFF) / (PACCI_UH_VFX_POINTS - 2)));

        Pacci_UhDrawSolid(play, gfxP, sPacciUhPrismDL, &points[i], &points[i + 1], width * (0.55f + (edge * 0.45f)), r,
                          g, b, alpha);
    }
    Pacci_UhSolidEnd(gfxP);
}

// A Maya / Blender translate handle: solid shaft, solid cone head, flat colour. The
// beam-segment version read as a spell rather than as a control, which is the one
// thing a gizmo must not do.
static void Pacci_UhDrawArrow(PlayState* play, Gfx** gfxP, Vec3f* center, Vec3f* direction, f32 startDist, f32 length,
                              u8 r, u8 g, u8 b) {
    f32 headLength = CLAMP_MIN((length - startDist) * 0.34f, 16.0f);
    f32 shaftEnd = length - headLength;
    Vec3f start;
    Vec3f neck;
    Vec3f tip;

    if (shaftEnd <= (startDist + 1.0f)) {
        shaftEnd = startDist + 1.0f;
    }
    start.x = center->x + (direction->x * startDist);
    start.y = center->y + (direction->y * startDist);
    start.z = center->z + (direction->z * startDist);
    neck.x = center->x + (direction->x * shaftEnd);
    neck.y = center->y + (direction->y * shaftEnd);
    neck.z = center->z + (direction->z * shaftEnd);
    tip.x = center->x + (direction->x * length);
    tip.y = center->y + (direction->y * length);
    tip.z = center->z + (direction->z * length);

    Pacci_UhSolidBegin(gfxP);
    Pacci_UhDrawSolid(play, gfxP, sPacciUhPrismDL, &start, &neck, PACCI_UH_GIZMO_SHAFT_R, r, g, b, 255);
    Pacci_UhDrawSolid(play, gfxP, sPacciUhConeDL, &neck, &tip, PACCI_UH_GIZMO_HEAD_R, r, g, b, 255);
    Pacci_UhSolidEnd(gfxP);
}

// The rotate handle: a solid band around the axis, built from the same prism the
// arrows use. One Begin/End wraps the whole ring, so a twenty-segment band costs
// twenty matrices instead of twenty full state changes.
static void Pacci_UhDrawRotationRing(PlayState* play, Gfx** gfxP, Vec3f* center, f32 radius, s16 yaw, u8 vertical, u8 r,
                                     u8 g, u8 b) {
    Vec3f previous;
    f32 sinYaw = Math_SinS(yaw);
    f32 cosYaw = Math_CosS(yaw);
    u8 i;

    Pacci_UhSolidBegin(gfxP);
    for (i = 0; i <= PACCI_UH_GIZMO_SEGMENTS; i++) {
        s16 angle = (s16)((i * 0x10000) / PACCI_UH_GIZMO_SEGMENTS);
        f32 sin = Math_SinS(angle);
        f32 cos = Math_CosS(angle);
        Vec3f point;

        if (vertical) {
            point.x = center->x + (sinYaw * cos * radius);
            point.y = center->y + (sin * radius);
            point.z = center->z + (cosYaw * cos * radius);
        } else {
            point.x = center->x + (sin * radius);
            point.y = center->y;
            point.z = center->z + (cos * radius);
        }
        if (i != 0) {
            Pacci_UhDrawSolid(play, gfxP, sPacciUhPrismDL, &previous, &point, PACCI_UH_GIZMO_RING_R, r, g, b, 245);
        }
        previous = point;
    }
    Pacci_UhSolidEnd(gfxP);
}

void Pacci_UltrahandDrawVfx(PlayState* play, Player* player) {
    Actor* held;
    PacciFuseBox box;
    Vec3f hand;
    Vec3f forward;
    Vec3f side;
    Vec3f up = { 0.0f, 1.0f, 0.0f };
    Vec3f down = { 0.0f, -1.0f, 0.0f };
    u16 buttons;
    u8 skip = 0;

    // Armed is enough. Requiring the MODE meant the tether, the light and the weld bead only
    // existed once C had been pressed, so simply having Ultrahand out showed nothing at all -
    // and the offer the player was being asked to act on was invisible until after they had
    // committed to a mode.
    if (!sUhMode.active && !sUhArmed) {
        Pacci_UhLightOff(play);
        return;
    }
    held = sUltrahand.held;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, play->gameplayFrames * 2, 0x20, 0x40, 1,
                                  play->gameplayFrames, play->gameplayFrames * -5, 0x10, 0x10, 2, 0, 1, -8));

    // ONE CLOSE_DISPS per OPEN_DISPS, and both at the same brace level. OPEN_DISPS opens a
    // block; every CLOSE_DISPS closes one. Two of these used to sit inside early returns, so
    // the first bail-out closed the function itself and everything after it was parsed at file
    // scope — which is where the "syntax error: '&'" on the next call came from. Hence the
    // guards set a flag instead of returning.
    if ((held == NULL) || sUltrahand.dropping) {
        // Nothing in hand: the colour filter IS the selection feedback. No geometry is drawn
        // over a candidate at all - that is what the wireframe box was, and a box is the one
        // shape almost nothing in this game actually is.
        Pacci_UhLightOff(play);
        skip = 1;
    } else if (!Pacci_FuseGetBox(play, held, &box)) {
        Pacci_UhLightOff(play);
        skip = 1;
    } else {
        // Held: the object becomes a light source. Ramped in over the same frames the tether
        // takes to reach it, so the room does not snap green on the grab frame.
        Pacci_UhLightAt(play, &box.center, CLAMP_MAX((f32)sUltrahand.vfxAge / 12.0f, 1.0f));
    }

    if (!skip) {

        // The same four layers as the Blender study: a narrow green core, two faint
        // irregular wisps and a translucent shell/ripple treatment on the held body.
        Pacci_UhGetHandPos(player, &hand);
        {
            f32 reach = CLAMP_MAX((f32)sUltrahand.vfxAge / 9.0f, 1.0f);

            // Untextured, like the gizmo arrows: solid octagonal tube, flat colour, no sprite.
            // The Great Fairy beam sprite it used before carried a visible spiral pattern
            // that read as "a vanilla effect stuck on" rather than as Zonai energy.
            //
            // Three strands, drawn widest-first so the thin bright one lands on top: a soft
            // halo, a mid body, and a near-white core. That layering is what makes a flat
            // untextured tube glow - a single pass of one colour is just a green stick. The
            // two outer strands run on different phases of the same wobble, so they cross the
            // core instead of sheathing it.
            Pacci_UhDrawFlowPass(play, &POLY_XLU_DISP, &hand, &box.center, reach, 0x2AAA, 5.0f, PACCI_UH_FLOW_HALO_R,
                                 PACCI_UH_FLOW_HALO_G, PACCI_UH_FLOW_HALO_B, 55);
            Pacci_UhDrawFlowPass(play, &POLY_XLU_DISP, &hand, &box.center, reach, 0x6AAA, 2.4f, PACCI_UH_FLOW_MID_R,
                                 PACCI_UH_FLOW_MID_G, PACCI_UH_FLOW_MID_B, 150);
            Pacci_UhDrawFlowPass(play, &POLY_XLU_DISP, &hand, &box.center, reach, 0, 1.1f, PACCI_UH_FLOW_CORE_R,
                                 PACCI_UH_FLOW_CORE_G, PACCI_UH_FLOW_CORE_B, 255);
        }

        // The handles are only drawn while the control they describe is being used. They used
        // to draw unconditionally, and four solid arrows the size of the object, permanently
        // wrapped around it, read as a box stuck to its front rather than as a gizmo. Maya
        // and Blender do the same: the handle appears when you reach for it.
        // The gizmo stays MODE-only: it describes D-pad controls that only the mode binds, and
        // drawing handles for bindings that are not live would be a lie.
        buttons = sUhMode.active ? play->state.input[0].cur.button : 0;
        forward.x = Math_SinS(player->actor.focus.rot.y);
        forward.y = 0.0f;
        forward.z = Math_CosS(player->actor.focus.rot.y);
        side.x = Math_SinS(player->actor.focus.rot.y + 0x4000);
        side.y = 0.0f;
        side.z = Math_CosS(player->actor.focus.rot.y + 0x4000);

        if (buttons & BTN_L) {
            f32 yawRadius = fmaxf(box.half.x, box.half.z) + 30.0f;
            f32 pitchRadius = fmaxf(box.half.y, box.half.z) + 30.0f;
            // Blue follows left/right (yaw), red follows up/down (pitch).
            Pacci_UhDrawRotationRing(play, &POLY_XLU_DISP, &box.center, yawRadius, player->actor.focus.rot.y, 0, 40,
                                     135, 255);
            Pacci_UhDrawRotationRing(play, &POLY_XLU_DISP, &box.center, pitchRadius, player->actor.focus.rot.y, 1, 255,
                                     75, 55);
        } else if (buttons & BTN_R) {
            f32 maxHalf = fmaxf(fmaxf(box.half.x, box.half.y), box.half.z);
            f32 start = maxHalf * 0.35f;
            f32 length = maxHalf + 72.0f;
            // R owns the screen plane: red vertical, blue horizontal.
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &up, start, length, 255, 70, 50);
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &down, start, length, 255, 70, 50);
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &side, start, length, 45, 135, 255);
            side.x = -side.x;
            side.z = -side.z;
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &side, start, length, 45, 135, 255);
        } else if (sUhMode.active && (buttons & (BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT))) {
            f32 maxHalf = fmaxf(fmaxf(box.half.x, box.half.y), box.half.z);
            f32 start = maxHalf * 0.35f;
            f32 length = maxHalf + 72.0f;
            // Ground plane: red forward/back, blue right/left.
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &forward, start, length, 255, 70, 50);
            forward.x = -forward.x;
            forward.z = -forward.z;
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &forward, start, length, 255, 70, 50);
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &side, start, length, 45, 135, 255);
            side.x = -side.x;
            side.z = -side.z;
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &side, start, length, 45, 135, 255);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// -- carrying an open flame ----------------------------------------------------
// DMG_FIRE is DMG_ARROW_FIRE | DMG_MAGIC_FIRE = 0x00020800 (z64collision_check.h:407), and that
// number is not a coincidence anywhere it turns up:
//
//   Bg_Ydan_Sp   (spider web)     bumper 0x00020800  - exactly this
//   Obj_Syokudai (torch flame)    bumper 0x00020820  - a superset of it
//
// So ONE AT collider carrying DMG_FIRE burns enemies, lights torches and burns webs, with no
// special case written for any of the three. The fire was already in the game; it just needed
// something to be attached to.
//
// A collider of our own even for actors that already have one, and each of the three reasons is
// a different actor: Obj_Syokudai's flame is AC, not AT - it is a target to be lit, and touching
// it damages nothing. Bg_Hidan_Firewall computes its collider position FROM THE PLAYER rather
// than from itself, so moving the actor does not move its fire. Only Bg_Hidan_Curtain brings an
// AT that follows it. One uniform collider is simpler than three exceptions.
#define PACCI_UH_FIRE_DAMAGE 4  // two hearts, where the enemy's table defers to the toucher
#define PACCI_UH_FIRE_EFFECT 1  // the fire slot in the vanilla damage-effect tables
#define PACCI_UH_FIRE_PAD 12.0f // reach past the body's own surface, so it lights what it nears
// Floor on the burning volume. A fire actor with no collision geometry of its own reads as a
// 12-unit cube through Pacci_FuseGetBox, and a flame you have to touch with the exact centre of
// is not a flame.
#define PACCI_UH_FIRE_MIN_R 45.0f
#define PACCI_UH_FIRE_MIN_H 90.0f

static ColliderCylinder sUhFireCol;
static Actor* sUhFireOwner = NULL;

static void Pacci_UhFireOff(void) {
    // Nothing to destroy: Collider_InitCylinder does not allocate, and re-initialising over the
    // same struct for a new owner is what item_spinner.c does too. Dropping the owner is enough,
    // and it is what stops the collider being submitted.
    sUhFireOwner = NULL;
}

// Blue fire in the hand keeps dropping the flame that MELTS.
//
// En_Ice_Hono's params pick a variant, and red ice only answers to a flame that is an En_Ice_Hono
// by actor id. The one worth carrying is the persistent scene-placed 0xFFFF; the one red ice reacts
// to is the ordinary 0. Rather than rewrite the carried actor's params mid-life - its update
// branches on them in several places and it was not built for that - the carried flame simply
// sheds ordinary ones as it goes, which is also what it looks like a torch should do.
//
// They cost nothing to leave behind: EnIceHono_SmallFlameMove sets timer = 44 and kills itself
// when it runs out (z_en_ice_hono.c), so at this period two or three exist at a time and none
// outlives being carried past.
static void Pacci_UhIceTick(PlayState* play, Actor* actor) {
    if ((actor == NULL) || (actor->id != ACTOR_EN_ICE_HONO)) {
        return;
    }
    if ((sUltrahand.vfxAge % PACCI_UH_ICE_PERIOD) != 0) {
        return;
    }
    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ICE_HONO, actor->world.pos.x, actor->world.pos.y, actor->world.pos.z, 0,
                actor->shape.rot.y, 0, 0);
}

// -- riding on Link's back -----------------------------------------------------
// Ultrahand on sitting Ruto does not carry her out in front like a crate - it puts her on Link's
// back, where the shield rides, and leaves her there.
//
// The reason is the one thing carrying her costs you in vanilla: both hands. She blocks ladders,
// vines and climbing generally, so half of Jabu-Jabu is walking her somewhere, putting her down,
// climbing, and coming back. On the back your hands are free and the escort stops being a leash.
//
// She comes off for the two things that need that space: using the cane again, and raising the
// shield. Shielding especially - she is sitting where the shield goes, and a shield that comes up
// through a passenger is worse than no feature at all.
static Actor* sBackRider = NULL;
static ActorFunc sBackRiderUpdate = NULL;
static u32 sBackRiderFlags = 0;
static s16 sBackRiderRoom = 0;

u8 Pacci_BackRiderActive(void) {
    return (sBackRider != NULL) ? 1 : 0;
}

// Hand her back to herself, wherever she currently is. She lands and sits back down on her own -
// her update never stopped running, so nothing has to be restarted.
void Pacci_BackRiderDrop(void) {
    Actor* rider = sBackRider;

    if ((rider != NULL) && (rider->update != NULL)) {
        if (sBackRiderUpdate != NULL) {
            rider->update = sBackRiderUpdate;
        }
        rider->flags = sBackRiderFlags;
        rider->room = (s8)sBackRiderRoom;
        rider->velocity.x = 0.0f;
        rider->velocity.y = 0.0f;
        rider->velocity.z = 0.0f;
    }
    sBackRider = NULL;
    sBackRiderUpdate = NULL;
}

// Put her where Link's back is, facing the way he faces.
static void Pacci_BackRiderPlace(Actor* rider, Player* player) {
    f32 sin = Math_SinS(player->actor.shape.rot.y);
    f32 cos = Math_CosS(player->actor.shape.rot.y);
    Vec3f torso = player->bodyPartsPos[PLAYER_BODYPART_TORSO];

    // Behind the torso along his facing, which is where the shield sits. Taken from the TORSO body
    // part rather than from world.pos so she leans and turns with him instead of hovering at a
    // fixed spot over his feet.
    rider->world.pos.x = torso.x - (sin * PACCI_BACKRIDE_BEHIND);
    rider->world.pos.y = torso.y + PACCI_BACKRIDE_RISE;
    rider->world.pos.z = torso.z - (cos * PACCI_BACKRIDE_BEHIND);
    rider->prevPos = rider->world.pos;
    rider->shape.rot.y = player->actor.shape.rot.y;
    rider->shape.rot.x = 0;
    rider->shape.rot.z = 0;
    rider->world.rot = rider->shape.rot;
    rider->velocity.x = 0.0f;
    rider->velocity.y = 0.0f;
    rider->velocity.z = 0.0f;
}

// Her own update still runs - she blinks, she talks, she offers the carry - and then she is put
// back on the shield. Same shape as the held-object wrapper and for the same reasons.
static void Pacci_BackRiderUpdate(Actor* thisx, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Pacci_UhKeepOnScreen(thisx);
    if (sBackRiderUpdate != NULL) {
        sBackRiderUpdate(thisx, play);
    }
    if ((thisx->update == NULL) || (player == NULL)) {
        return;
    }
    Pacci_BackRiderPlace(thisx, player);
    Pacci_AnchorSync(play, thisx);
}

static void Pacci_BackRiderTake(PlayState* play, Player* player, Actor* rider) {
    Pacci_BackRiderDrop(); // one passenger

    sBackRider = rider;
    sBackRiderUpdate = rider->update;
    sBackRiderFlags = rider->flags;
    sBackRiderRoom = rider->room;
    rider->update = Pacci_BackRiderUpdate;
    rider->room = -1; // she rides through doors with him
    rider->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    Pacci_AnchorTake(rider);
    Pacci_BackRiderPlace(rider, player);
    // Actor_PlaySfx is the Majora's Mask name; OOT/SoH calls the same function
    // (Actor*, u16) Player_PlaySfx — see z_actor.c:2310.
    Player_PlaySfx(rider, NA_SE_SY_GET_ITEM);
}

// Every frame, from CustomItems_Update: she has to come off when the shield goes up whether or not
// the cane is still in hand, and she has to be forgotten if she dies or the scene takes her.
void Pacci_BackRiderTick(PlayState* play) {
    Player* player;

    if ((play == NULL) || (sBackRider == NULL)) {
        return;
    }
    if (sBackRider->update == NULL) {
        sBackRider = NULL;
        sBackRiderUpdate = NULL;
        return;
    }
    player = GET_PLAYER(play);
    if ((player != NULL) && (player->stateFlags1 & PLAYER_STATE1_SHIELDING)) {
        Pacci_BackRiderDrop();
    }
}

// -- holding a floor switch down -----------------------------------------------
// The placement was landing correctly and pressing nothing, and the reason is two facts that only
// matter together.
//
// There are TWO ways an actor presses a floor switch in vanilla, and a body Ultrahand sets down
// goes through neither.
//
// The generic one: the engine marks a switch pressed for an actor carrying
// ACTOR_FLAG_CAN_PRESS_SWITCHES (code_800430A0.c:59-70, func_80043334 ->
// DynaPolyActor_SetSwitchPressed), and that runs from Actor_UpdateBgCheckInfo. Across all of
// vanilla exactly two actors use it - En_Am and Obj_Kibako - which is far fewer than it looks like
// it should be.
//
// The other one: the actor does it itself. Obj_Oshihiki has no flag and does not need it - it has
// a whole state for resting on another dynapoly, and calls SetActorOnTop and SetSwitchPressed from
// there by hand (z_obj_oshihiki.c:510-513). A pushable block absolutely can press a floor switch;
// it just does not take the route the flag describes.
//
// What both routes have in common is that they are reached through the actor's OWN movement logic.
// A block put on a plate by the cane never pushed, never fell, and never entered the state that
// does the pressing - it simply appeared there, correctly positioned, and told nobody.
//
// So the press is done here, explicitly, for the pair Ultrahand put together. It is the same two
// calls the engine would have made. The pairing outlives the release on purpose - a block set down
// on a switch is supposed to hold it, and the whole feature is worth nothing otherwise - and it
// ends the moment the body is no longer over the plate.
static Actor* sPressBody = NULL;
static Actor* sPressSwitch = NULL;

static void Pacci_PlacePressForget(void) {
    sPressBody = NULL;
    sPressSwitch = NULL;
}

// Remember that this body is sitting on this switch. Called once the magnet has finished arriving.
static void Pacci_PlacePressSet(Actor* body, Actor* sw) {
    sPressBody = body;
    sPressSwitch = sw;
}

// Every frame, from CustomItems_Update - which keeps running after the cane is put away, and has
// to, since the whole point is a switch that stays down while you walk off and use the door.
//
// Re-asserted rather than latched because the engine wipes interactFlags every frame
// (DynaPolyActor_UnsetAllInteractFlags): a switch is pressed only for as long as something keeps
// saying so, which is exactly the behaviour wanted here.
void Pacci_PlacePressTick(PlayState* play) {
    DynaPolyActor* dyna;
    f32 dx;
    f32 dz;
    f32 plateTop;

    if ((play == NULL) || (sPressBody == NULL) || (sPressSwitch == NULL)) {
        return;
    }
    if ((sPressBody->update == NULL) || (sPressSwitch->update == NULL)) {
        Pacci_PlacePressForget();
        return;
    }
    // Still over the plate? Measured in XZ against the switch, and in Y against the plate's own
    // top: lifting the body off has to release the switch, and so does sliding it away.
    dx = sPressBody->world.pos.x - sPressSwitch->world.pos.x;
    dz = sPressBody->world.pos.z - sPressSwitch->world.pos.z;
    if (((dx * dx) + (dz * dz)) > (PACCI_PRESS_HOLD * PACCI_PRESS_HOLD)) {
        Pacci_PlacePressForget();
        return;
    }
    if (Pacci_UhDynaTopY(play, sPressSwitch, &plateTop)) {
        f32 dy = sPressBody->world.pos.y - plateTop;

        if ((dy > PACCI_PRESS_HOLD) || (dy < -PACCI_PRESS_HOLD)) {
            Pacci_PlacePressForget();
            return;
        }
    }
    // The switch has to BE dynapoly for this to mean anything; it always is, but a cast to
    // DynaPolyActor on the strength of an actor id is worth checking rather than assuming.
    dyna = DynaPoly_GetActor(&play->colCtx, ((DynaPolyActor*)sPressSwitch)->bgId);
    if ((dyna == NULL) || (&dyna->actor != sPressSwitch)) {
        Pacci_PlacePressForget();
        return;
    }
    DynaPolyActor_SetActorOnTop(dyna);
    DynaPolyActor_SetSwitchPressed(dyna);
}

// A LOCKED body's flag is not just bookkeeping - something has to visibly happen. For the skull it
// is the jaw, and the jaw is just a rotation: BgDodoago_Init sits it at 0x1333 when its flag is
// already set (z_bg_dodoago.c:131-133), and its collision rides the actor's own SRT, so turning it
// turns the mouth you can walk into as well as the one you can see.
//
// Driven here rather than by prodding BgDodoago's actionFunc, which would mean writing a function
// pointer into another actor's private struct - and unlike reading a field, getting that wrong is
// not something a guard can catch.
static void Pacci_UhLockedPose(PlayState* play, Actor* actor) {
    const PacciUhTraitRow* row;
    s16 want;
    s32 open;

    if (!(sUltrahand.traits & PACCI_UH_TRAIT_JAW)) {
        return;
    }
    row = Pacci_UhTraitRow(actor);
    if (row == NULL) {
        return;
    }
    open = Flags_GetSwitch(play, (actor->params >> row->pathShift) & row->pathMask);
    want = open ? PACCI_UH_LOCKED_OPEN_X : 0;
    Math_SmoothStepToS(&actor->shape.rot.x, want, 4, 0x0400, 0x20);

    // Open is THREE things to this actor, not one. BgDodoago_Init sets the flag, the jaw angle and
    // both eye brightnesses together (z_bg_dodoago.c:131-134), and BgDodoago_WaitExplosives reads
    // those eyes back to decide what a bomb does next. Setting only the flag and the angle would
    // leave a skull that looks open and still believes its eyes are dark.
    if (actor->id == ACTOR_BG_DODOAGO) {
        play->roomCtx.unk_74[0] = play->roomCtx.unk_74[1] = open ? 255 : 0;
    }
}

// D-pad on a HINGE body runs the actor's OWN open/close animation.
//
// The difference from JAW is the whole reason both exist. The Dodongo has no animation to call, so
// its jaw is posed by hand; the drawbridge has one, and calling it gets the chains, the timing and
// the two bridge sounds for free. Writing shape.rot.x here instead would have meant reproducing all
// three badly, and its own BgSpot00Hanebasi_DrawbridgeWait would have fought the result.
static void Pacci_UhHingeInput(Actor* actor, u8 edge) {
    BgSpot00Hanebasi* bridge;
    BgSpot00Hanebasi* chain;
    s16 want;

    if ((actor == NULL) || !(sUltrahand.traits & PACCI_UH_TRAIT_HINGE) || (actor->id != ACTOR_BG_SPOT00_HANEBASI) ||
        (actor->child == NULL)) {
        return;
    }
    if (edge & 1) {
        want = -0x4000; // raised
    } else if (edge & 2) {
        want = 0; // lowered
    } else {
        return;
    }
    bridge = (BgSpot00Hanebasi*)actor;
    chain = (BgSpot00Hanebasi*)actor->child;
    if (bridge->destAngle == want) {
        return;
    }
    bridge->destAngle = want;
    chain->destAngle = (want != 0) ? -0xFE0 : 0; // the chain swings a fraction of the deck's arc
    bridge->actionFunc = BgSpot00Hanebasi_DrawbridgeRiseAndFall;
}

// D-pad on a HEIGHT body changes how BIG it is, because that is the only thing about it worth
// changing. Same up/down gesture that raises and lowers anything else - it just grows instead.
static void Pacci_UhHeightInput(Actor* actor, u8 edge) {
    EnSiofuki* spout;

    if ((actor == NULL) || !(sUltrahand.traits & PACCI_UH_TRAIT_HEIGHT) || (actor->id != ACTOR_EN_SIOFUKI)) {
        return;
    }
    spout = (EnSiofuki*)actor;
    if (edge & 1) {
        spout->targetHeight += PACCI_UH_HEIGHT_STEP;
    } else if (edge & 2) {
        spout->targetHeight -= PACCI_UH_HEIGHT_STEP;
    } else {
        return;
    }
    // Clamped to something the spout can actually be. It smooth-steps currentHeight toward this,
    // so the growing and shrinking is its own animation and not a jump.
    spout->targetHeight = CLAMP(spout->targetHeight, 0.0f, PACCI_UH_HEIGHT_MAX);
    Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// D-pad on a LOCKED body drives its flag directly, because there is nothing else about it to
// drive. Up is "on", down is "off" - the same directions that would have raised and lowered
// anything else, so the gesture reads the same even though nothing moves.
static void Pacci_UhLockedInput(PlayState* play, u8 edge) {
    Actor* actor = sUltrahand.held;
    const PacciUhTraitRow* row;
    s32 flag;
    u8 want;

    if ((actor == NULL) || !(sUltrahand.traits & PACCI_UH_TRAIT_LOCKED) ||
        !(sUltrahand.traits & PACCI_UH_TRAIT_SETS_FLAG) || (sUltrahand.traits & PACCI_UH_TRAIT_HINGE)) {
        return; // a hinge answers to its own animation, not to a flag
    }
    if (edge & 1) {
        want = 1;
    } else if (edge & 2) {
        want = 0;
    } else {
        return;
    }
    row = Pacci_UhTraitRow(actor);
    if (row == NULL) {
        return;
    }
    flag = (actor->params >> row->pathShift) & row->pathMask;
    if ((Flags_GetSwitch(play, flag) != 0) == (want != 0)) {
        return;
    }
    if (want) {
        Flags_SetSwitch(play, flag);
    } else {
        Flags_UnsetSwitch(play, flag);
    }
    Audio_PlaySoundGeneral(want ? NA_SE_SY_GET_ITEM : NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// Moved far enough by hand? Then the room learns about it - and, for the ones that can go back,
// moving it the other way tells the room that too.
//
// Measured from railPos, which is where the body was when it was GRABBED, not from its home in the
// scene. A platform you had already raised halfway does not count as raised again for having been
// picked up; the travel has to be travel you just did. That also makes the two directions
// symmetric without any extra state: carry it up past the threshold and it is up, keep going the
// other way past the threshold and it is down.
//
// Debounced on the FLAG rather than on a "done" bit. Asking whether the switch already reads the
// way we are about to write it is both the cheaper test and the honest one - it is the actual
// state, so it stays right across a save, a room reload, or the room's own switch being hit while
// the thing is in your hands.
static void Pacci_UhFlagTick(PlayState* play, Actor* actor) {
    const PacciUhTraitRow* row;
    f32 dy;
    f32 dx;
    f32 dz;
    s32 flag;
    u8 want;

    if (!(sUltrahand.traits & PACCI_UH_TRAIT_SETS_FLAG)) {
        return;
    }
    dy = actor->world.pos.y - sUltrahand.railPos.y;
    if (sUltrahand.flagDir != 0) {
        f32 travelled = (sUltrahand.flagDir > 0) ? dy : -dy;

        if (travelled >= PACCI_UH_FLAG_TRAVEL) {
            want = 1;
        } else if ((sUltrahand.traits & PACCI_UH_TRAIT_CLEARS_FLAG) && (travelled <= -PACCI_UH_FLAG_TRAVEL)) {
            want = 0;
        } else {
            return; // still inside the deadband, in a direction that means nothing
        }
    } else {
        dx = actor->world.pos.x - sUltrahand.railPos.x;
        dz = actor->world.pos.z - sUltrahand.railPos.z;
        if (((dx * dx) + (dy * dy) + (dz * dz)) < (PACCI_UH_FLAG_TRAVEL * PACCI_UH_FLAG_TRAVEL)) {
            return;
        }
        want = 1;
    }
    row = Pacci_UhTraitRow(actor);
    if (row == NULL) {
        return;
    }
    flag = (actor->params >> row->pathShift) & row->pathMask;
    // Vanilla's own rule, as data: if params carry bits outside the flag field then those bits mean
    // something else and this actor has no switch of its own. Setting flag would be flipping a
    // stranger's.
    if ((sUltrahand.traits & PACCI_UH_TRAIT_FLAG_STRICT) && (actor->params != (s16)(flag << row->pathShift))) {
        return;
    }
    if ((Flags_GetSwitch(play, flag) != 0) == (want != 0)) {
        return; // the room already agrees
    }
    if (want) {
        Flags_SetSwitch(play, flag);
    } else {
        Flags_UnsetSwitch(play, flag);
    }
    Audio_PlaySoundGeneral(want ? NA_SE_SY_GET_ITEM : NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// The placement magnet: while a spot is on offer, the body SLIDES there on its own.
//
// This replaces snapping it into place on the button. Lining a block up by hand means fighting the
// carry's easing for the last few units, and a snap-on-press hides the alignment until after you
// have committed to it - you press A hoping. Sliding shows you the answer while you can still walk
// away from it, and then the press is just a release, which is what a press should be.
//
// The blend is what makes it reversible. It ramps toward the offer and back toward the controls,
// and because the offer is measured from anchorPos - where the controls asked for, not where the
// body slid to - moving the anchor away kills the offer and the body comes back. Measuring from
// the body would have been a trap: once it reached the switch it would report itself as on target
// forever and never let go.
static void Pacci_UhPlaceMagnet(Actor* actor) {
    Vec3f want;
    f32 blend;

    if (sPlacePv.valid) {
        Math_StepToF(&sUltrahand.placeBlend, 1.0f, PACCI_PLACE_PULL);
    } else {
        Math_StepToF(&sUltrahand.placeBlend, 0.0f, PACCI_PLACE_PULL);
    }
    blend = sUltrahand.placeBlend;
    if (blend <= 0.0f) {
        return;
    }
    // sPlacePv.pos is stale by one frame - the preview runs after the carry - and that is fine at
    // this speed. It is a fixed point in the world, not something that moves.
    want = sPlacePv.pos;
    actor->world.pos.x += (want.x - actor->world.pos.x) * blend;
    actor->world.pos.y += (want.y - actor->world.pos.y) * blend;
    actor->world.pos.z += (want.z - actor->world.pos.z) * blend;

    // The slide is NOT a throw. carryVel is sampled from how far the body moved each frame and is
    // handed to the release as inertia - so a block that had just slid onto a plate was launched
    // off it the instant you let go, which is the "lo hacen saltar y lo mueven fuera del switch".
    // The magnet's motion is placement, not something the player wound up.
    sUltrahand.carryVel.x = 0.0f;
    sUltrahand.carryVel.y = 0.0f;
    sUltrahand.carryVel.z = 0.0f;

    // Arrived, and it is a switch: from here that switch is held down, and stays held after the
    // release. Registered at the top of the ramp rather than on a button because there is no
    // button any more - the placement finished while you were watching it.
    if ((blend >= PACCI_PLACE_ARRIVED) && (sPlacePv.sw != NULL)) {
        Pacci_PlacePressSet(actor, sPlacePv.sw);
    }

    // Squares up on the way in, so it arrives level instead of arriving and then jerking straight.
    // A switch hands over its whole orientation; plain ground only takes the tilt out.
    if (blend > 0.5f) {
        s16 wantX = (sPlacePv.sw != NULL) ? sPlacePv.sw->shape.rot.x : 0;
        s16 wantY = (sPlacePv.sw != NULL) ? sPlacePv.sw->shape.rot.y : actor->shape.rot.y;
        s16 wantZ = (sPlacePv.sw != NULL) ? sPlacePv.sw->shape.rot.z : 0;

        Math_SmoothStepToS(&actor->shape.rot.x, wantX, 3, 0x1000, 0x40);
        Math_SmoothStepToS(&actor->shape.rot.y, wantY, 3, 0x1000, 0x40);
        Math_SmoothStepToS(&actor->shape.rot.z, wantZ, 3, 0x1000, 0x40);
        actor->world.rot = actor->shape.rot;
        sUltrahand.baseRot = actor->shape.rot;
    }
}

// One frame of fire, if the body in hand is something that burns. Called from the carry.
static void Pacci_UhFireTick(PlayState* play, Actor* actor) {
    PacciFuseBox box;
    CombatColliderConfig cfg;
    Vec3f pos;

    if (!(sUltrahand.traits & PACCI_UH_TRAIT_BURNS) || (actor == NULL) || (actor->update == NULL)) {
        Pacci_UhFireOff();
        return;
    }
    if (!Pacci_FuseGetBox(play, actor, &box)) {
        return;
    }
    cfg.dmgFlags = DMG_FIRE;
    cfg.damage = PACCI_UH_FIRE_DAMAGE;
    cfg.effect = PACCI_UH_FIRE_EFFECT;
    // The WHOLE body burns, not a token cylinder at its middle. Two things were making it small:
    // the radius took the larger horizontal half-extent, which under-covers a wide flat curtain
    // seen corner-on, so it is the diagonal now; and Pacci_FuseGetBox falls back to a 12-unit cube
    // for an actor with neither dynapoly nor a collision cylinder, which is most of the fire
    // actors - Bg_Hidan_Curtain has an AT and no AC geometry for the box to read. Hence the floor.
    cfg.radius = sqrtf((box.half.x * box.half.x) + (box.half.z * box.half.z)) + PACCI_UH_FIRE_PAD;
    cfg.height = (box.half.y * 2.0f) + PACCI_UH_FIRE_PAD;
    if (cfg.radius < PACCI_UH_FIRE_MIN_R) {
        cfg.radius = PACCI_UH_FIRE_MIN_R;
    }
    if (cfg.height < PACCI_UH_FIRE_MIN_H) {
        cfg.height = PACCI_UH_FIRE_MIN_H;
    }

    // Re-armed whenever the body changes, because Collider_SetCylinder bakes the OWNER in and the
    // game needs to attribute the burn to the flame rather than to whatever we held last.
    if (sUhFireOwner != actor) {
        Combat_InitCylinder(play, &sUhFireCol, actor, &cfg);
        sUhFireOwner = actor;
    }
    // A cylinder is positioned by its BASE, not its centre.
    pos.x = box.center.x;
    pos.y = box.center.y - box.half.y - (PACCI_UH_FIRE_PAD * 0.5f);
    pos.z = box.center.z;
    Combat_UpdateCylinder(&sUhFireCol, &pos, &cfg);
    Combat_RegisterCollider(play, &sUhFireCol);
    // AT_HIT has to be cleared by hand or the collider counts as spent and stops registering
    // hits - item_spinner.c:50-55 does the same, and for the same reason.
    if (Combat_CheckHit(&sUhFireCol)) {
        sUhFireCol.base.atFlags &= ~AT_HIT;
    }
}

// Does this collision surface belong to the thing we are dropping? The root and every piece
// glued to it are all live dynapoly while they fall, and a downward probe from any of them runs
// straight into the others.
static u8 Pacci_UhIsOwnBody(PlayState* play, s32 bgId) {
    DynaPolyActor* dyna = DynaPoly_GetActor(&play->colCtx, bgId);
    Actor* hitActor;

    if (dyna == NULL) {
        return 0; // plain scene collision: this is exactly what we are looking for
    }
    hitActor = &dyna->actor;
    if ((hitActor == sUltrahand.held) || (hitActor == sFuse.root)) {
        return 1;
    }
    for (u8 i = 0; i < sFuse.count; i++) {
        if (sFuse.parts[i].actor == hitActor) {
            return 1;
        }
    }
    return 0;
}

// Where this body's FOOTPRINT lands, as opposed to where its origin does.
// Actor_UpdateBgCheckInfo raycasts one point at the actor's origin, so a structure whose
// origin sits in its middle sinks half its own height into the floor before anything
// reports contact - and a wide platform dropped across a pit fell straight through, because
// its centre had nothing under it while its corners had plenty of floor. This is the
// "usando sus geometrias" half of the drop.
//
// BgCheck_ProjectileLineTest rather than one of the BgCheck floor helpers: their names and
// signatures diverge between the two games, this one does not, and it sees dynapoly as well
// as scene collision - which is what lets you set one built structure down on another.
//
// The probe starts just BELOW the box instead of above it. Our own merged surface is still
// registered and still moving while we fall, and a ray starting inside it would hit the very
// body it is testing; every one of our polys is inside the box by definition, so starting
// under it cannot self-intersect.
static u8 Pacci_UhGroundUnder(PlayState* play, Actor* actor, f32* outY) {
    static const f32 sProbeX[5] = { -1.0f, 1.0f, -1.0f, 1.0f, 0.0f };
    static const f32 sProbeZ[5] = { -1.0f, -1.0f, 1.0f, 1.0f, 0.0f };
    PacciFuseBox box;
    Vec3f from;
    Vec3f to;
    Vec3f hit;
    CollisionPoly* poly;
    s32 bgId;
    f32 best = 0.0f;
    f32 bottom;
    f32 sin;
    f32 cos;
    u8 found = 0;
    u8 i;

    if (!Pacci_FuseGetBox(play, actor, &box)) {
        return 0;
    }
    bottom = box.center.y - box.half.y;
    sin = Math_SinS(box.yaw);
    cos = Math_CosS(box.yaw);

    // Four bottom corners and the centre. Five probes catch an edge hanging over a drop
    // without turning every falling frame into a raycast storm.
    for (i = 0; i < 5; i++) {
        f32 lx = sProbeX[i] * box.half.x;
        f32 lz = sProbeZ[i] * box.half.z;

        from.x = box.center.x + ((lx * cos) + (lz * sin));
        from.z = box.center.z + ((lz * cos) - (lx * sin));
        from.y = bottom - 1.0f;
        to.x = from.x;
        to.z = from.z;
        to.y = bottom - PACCI_UH_GROUND_PROBE;

        // Cast repeatedly, stepping past anything belonging to our OWN assembly. THIS is what
        // made a built structure stop dead in mid-air the instant it was released: starting the
        // ray just under the root's box put it straight through any piece glued BELOW the root,
        // so the very first hit was our own body a couple of units down. That reads as "the
        // floor is right here", the landing test passes on the first frame of the fall, and the
        // whole thing is set down where it was floating.
        //
        // Sibling pieces have to be skipped as well, not just the root: a part probing downward
        // finds whatever else is bolted under it long before it finds the ground.
        for (u8 attempt = 0; attempt < 5; attempt++) {
            if (!BgCheck_ProjectileLineTest(&play->colCtx, &from, &to, &hit, &poly, true, true, true, true, &bgId)) {
                break;
            }
            if (Pacci_UhIsOwnBody(play, bgId)) {
                // Restart just below the surface we hit and keep going down.
                from.y = hit.y - 1.0f;
                if (from.y <= to.y) {
                    break;
                }
                continue;
            }
            // HIGHEST hit wins: that is the surface the body comes to rest on first, and it is
            // why a structure straddling a step ends up on the step and not through it.
            if (!found || (hit.y > best)) {
                best = hit.y;
                found = 1;
            }
            break;
        }
    }
    if (!found) {
        return 0;
    }
    // Floor-under-the-box -> where the ORIGIN goes, since the origin is not the box bottom.
    *outY = best + (actor->world.pos.y - bottom);
    return 1;
}

// Where the ROOT has to stop so that NOTHING in the assembly is underground.
//
// The probe above only knows about one body. Run it on the root alone and a structure lands on
// whatever is under the root - so a piece glued below it goes through the floor, and a piece
// glued out to the side hangs over a pit unsupported. Every piece is asked instead, and each
// answer is converted into the root Y it implies; the HIGHEST wins, because the first piece to
// touch down is what stops the whole thing.
static u8 Pacci_UhAssemblyGround(PlayState* play, Actor* root, f32* outY) {
    f32 best = 0.0f;
    u8 found = 0;
    f32 y;

    if (Pacci_UhGroundUnder(play, root, &y)) {
        best = y;
        found = 1;
    }
    if (sFuse.root == root) {
        for (u8 i = 0; i < sFuse.count; i++) {
            Actor* part = sFuse.parts[i].actor;

            if ((part == NULL) || (part->update == NULL)) {
                continue;
            }
            if (!Pacci_UhGroundUnder(play, part, &y)) {
                continue;
            }
            // y is where THAT PIECE'S origin would rest. The root sits a fixed distance from it,
            // so the same landing expressed in the root's terms is y plus that distance.
            y += root->world.pos.y - part->world.pos.y;
            if (!found || (y > best)) {
                best = y;
                found = 1;
            }
        }
    }
    if (!found) {
        return 0;
    }
    *outY = best;
    return 1;
}

// -- stored geometry ---------------------------------------------------------
// One slot, holding a RECIPE rather than geometry: actor id, params, and each piece's place
// in the root's local frame. Copying the merged CollisionHeader instead would be wrong twice
// over - the pools are a single static instance that the next weld overwrites, and we own
// none of the display lists it describes, so what came back would be an invisible shape that
// stopped existing as soon as you glued anything else together.
typedef struct {
    s16 actorId;
    s16 params;
    Vec3f offset; // root-local; index 0 is the root itself and is all zero
    Vec3s rot;    // relative to the root
} PacciBlueprintPiece;

static struct {
    u8 used;
    u8 count;
    // Scene it was taken from. An actor cannot be spawned without its object loaded, and the
    // object bank is per scene: rebuilding a graveyard's gravestones inside a dungeon spawns
    // actors whose object is not resident, which is a crash and not a missing model.
    s16 scene;
    PacciBlueprintPiece piece[PACCI_FUSE_MAX_PARTS + 1];
} sBlueprint = { 0 };

u8 Pacci_BlueprintStored(void) {
    return sBlueprint.used;
}

static void Pacci_BlueprintDeny(void) {
    Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

u8 Pacci_BlueprintSave(PlayState* play) {
    Actor* doomed[PACCI_FUSE_MAX_PARTS + 1];
    Actor* root = sUltrahand.held;
    PacciFuseBox box;
    u8 n = 0;
    u8 i;

    if ((root == NULL) || sUltrahand.dropping) {
        Pacci_BlueprintDeny();
        return 0;
    }
    // Dynapoly only, and every piece of it. Anything without a collision surface of its own
    // would come back from the recipe as a plain actor you fall through, which is worse than
    // refusing to store it. Each piece is asked directly now - nothing unregisters their bg
    // actors any more, so their own boxes tell the truth.
    if (!Pacci_FuseGetBox(play, root, &box) || !box.dyna) {
        Pacci_BlueprintDeny();
        return 0;
    }
    if ((sFuse.count > 0) && (sFuse.root != root)) {
        Pacci_BlueprintDeny();
        return 0;
    }
    for (i = 0; i < sFuse.count; i++) {
        Actor* part = sFuse.parts[i].actor;

        if ((part == NULL) || (part->update == NULL) || !Pacci_FuseGetBox(play, part, &box) || !box.dyna) {
            Pacci_BlueprintDeny();
            return 0;
        }
    }
    // The root's box was overwritten by the loop above; nothing below reads it.

    // Validated in full before anything is written or killed: a half-stored structure with
    // half its pieces deleted is not something the player can undo.
    sBlueprint.piece[0].actorId = root->id;
    sBlueprint.piece[0].params = root->params;
    sBlueprint.piece[0].offset.x = 0.0f;
    sBlueprint.piece[0].offset.y = 0.0f;
    sBlueprint.piece[0].offset.z = 0.0f;
    sBlueprint.piece[0].rot.x = 0;
    sBlueprint.piece[0].rot.y = 0;
    sBlueprint.piece[0].rot.z = 0;
    doomed[0] = root;
    n = 1;

    for (i = 0; i < sFuse.count; i++) {
        sBlueprint.piece[n].actorId = sFuse.parts[i].actor->id;
        sBlueprint.piece[n].params = sFuse.parts[i].actor->params;
        sBlueprint.piece[n].offset = sFuse.parts[i].offset;
        sBlueprint.piece[n].rot = sFuse.parts[i].rot;
        doomed[n] = sFuse.parts[i].actor;
        n++;
    }

    // Give every piece its collision and its update back BEFORE killing any of it. Killing an
    // actor whose update we had replaced runs its Destroy against a body we are still holding
    // half-rewired, and a dynapoly piece would take its merged surface to the grave with it.
    Pacci_FuseRelease();
    Pacci_UltrahandLetGo();
    for (i = 0; i < n; i++) {
        if ((doomed[i] != NULL) && (doomed[i]->update != NULL)) {
            Actor_Kill(doomed[i]);
        }
    }

    sBlueprint.used = 1;
    sBlueprint.count = n;
    sBlueprint.scene = play->sceneNum;
    Audio_PlaySoundGeneral(NA_SE_SY_GET_ITEM, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    return 1;
}

u8 Pacci_BlueprintSummon(PlayState* play, Player* player) {
    Actor* spawned[PACCI_FUSE_MAX_PARTS + 1];
    Vec3f base;
    s16 yaw = player->actor.focus.rot.y;
    f32 sin = Math_SinS(yaw);
    f32 cos = Math_CosS(yaw);
    u8 built = 0;
    u8 i;

    if (!sBlueprint.used || Pacci_IsHoldingUltrahand() || (sFuse.count > 0)) {
        return 0;
    }
    if (sBlueprint.scene != play->sceneNum) {
        Pacci_BlueprintDeny(); // see the note on sBlueprint.scene
        return 0;
    }
    // Magic LAST among the checks and before the first spawn: charging for a summon that
    // then fails to build is the one outcome with no way back.
    if (!Magic_RequestChange(play, PACCI_UH_SUMMON_COST, MAGIC_CONSUME_NOW)) {
        Pacci_BlueprintDeny();
        return 0;
    }

    base.x = player->actor.world.pos.x + (sin * PACCI_UH_DIST_MIN);
    base.y = player->actor.world.pos.y + PACCI_UH_SUMMON_RISE;
    base.z = player->actor.world.pos.z + (cos * PACCI_UH_DIST_MIN);

    for (i = 0; i < sBlueprint.count; i++) {
        Vec3f off = sBlueprint.piece[i].offset;

        spawned[i] =
            Actor_Spawn(&play->actorCtx, play, sBlueprint.piece[i].actorId, base.x + ((off.x * cos) + (off.z * sin)),
                        base.y + off.y, base.z + ((off.z * cos) - (off.x * sin)), sBlueprint.piece[i].rot.x,
                        sBlueprint.piece[i].rot.y + yaw, sBlueprint.piece[i].rot.z, sBlueprint.piece[i].params);
        if (spawned[i] == NULL) {
            break;
        }
        built++;
    }
    // The actor pool can refuse. Roll the whole thing back rather than leave a half-built
    // structure standing in front of the player with the magic already spent.
    if (built != sBlueprint.count) {
        for (i = 0; i < built; i++) {
            Actor_Kill(spawned[i]);
        }
        Pacci_BlueprintDeny();
        return 0;
    }

    Pacci_UltrahandTake(player, spawned[0]);
    for (i = 1; i < sBlueprint.count; i++) {
        Pacci_FuseAdopt(spawned[0], spawned[i], &sBlueprint.piece[i].offset, &sBlueprint.piece[i].rot);
    }
    // Single use, exactly as asked: the slot empties on recall.
    sBlueprint.used = 0;
    sBlueprint.count = 0;
    return 1;
}

// ============================================================================
// ULTRAHAND MODE
// ============================================================================
//
// C enters the mode and it OWNS the input from then on, which is the whole reason
// it can afford this many controls: nothing here has to share a button with
// rolling, shielding or item swapping.
//
//   A          grab what you are pointing at; it stays in the air
//              (a second A will glue — not built yet)
//   B          drop it and leave the mode
//   D-pad      ground plane: forward/back and left/right
//   L + D-pad  rotate yaw/pitch
//   R + D-pad  vertical plane: up/down and left/right
//
// Roughly TotK's scheme: grab on A, cancel on B, a held modifier to switch the
// D-pad from rotating to moving.
#define PACCI_UH_ROT_SNAP 0x2000 // 45 degrees per press, TotK-style snapping
// Movement is CONTINUOUS while the pad is held, not one step per press: a single
// nudge of 20 was swallowed whole by the carry's easing, which is why raising and
// lowering looked like it did nothing. Rotation stays per-press — snapping to 45
// degrees is the whole point there.
#define PACCI_UH_MOVE_RATE 6.0f // height units per FRAME while held
#define PACCI_UH_DIST_RATE 8.0f // push/pull units per FRAME while held
#define PACCI_UH_SIDE_MIN -120.0f
#define PACCI_UH_SIDE_MAX 120.0f
#define PACCI_UH_HEIGHT_MIN -80.0f
#define PACCI_UH_HEIGHT_MAX 160.0f

u8 Pacci_UltrahandModeActive(void) {
    return sUhMode.active;
}

void Pacci_UltrahandModeEnter(PlayState* play, Player* player) {
    sUhMode.active = 1;
    sUhMode.heightOff = 0.0f;
    sUhMode.sideOff = 0.0f;
    sUhMode.prevDpad = 0;
    sUhMode.summonHold = 0;
    sUhHighlightTarget = NULL;
    Audio_PlaySoundGeneral(NA_SE_SY_GET_ITEM, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

void Pacci_UltrahandModeExit(PlayState* play) {
    if (sUltrahand.held != NULL) {
        // Falls on the way out too. Pacci_UpdateUltrahand keeps being called by the cane's own
        // per-frame handler, so the drop finishes after the mode is gone.
        Pacci_UltrahandBeginDrop();
    }
    Pacci_UhTintClear();
    sUhMode.active = 0;
    sUhMode.heightOff = 0.0f;
    sUhMode.sideOff = 0.0f;
    sUhMode.prevDpad = 0;
    sUhMode.summonHold = 0;
    sUhHighlightTarget = NULL;
    Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// One frame of the mode. Returns 1 while it owns the input, so the caller stops.
u8 Pacci_UltrahandModeUpdate(PlayState* play, Player* player) {
    Input* input;
    u16 cur;
    u8 dpad;
    u8 edge;
    u8 withL;
    u8 withR;

    if (!sUhMode.active) {
        return 0;
    }

    input = &play->state.input[0];
    cur = input->cur.button;

    // Wipe the tint table FIRST, before anything in this frame decides what to light. The draw
    // pass runs after the whole update, so re-registering below still lands in time; clearing
    // at the end instead would leave every actor untinted for the frame it mattered.
    Pacci_UhTintClear();

    // Shake the stick left-right to take the structure apart. R used to hold this job and
    // cannot any more — it is the raise/slide modifier now — and a shake is what TotK asks
    // for anyway, just on a stick the N64 does not have.
    // Hauling owns the frame while it lasts: Link is animating a stance and the pad is the only
    // thing that moves anything. B lets go, as it does everywhere else.
    if (Pacci_PullActive()) {
        u8 pullPad = 0;

        if (cur & BTN_DUP) {
            pullPad |= 1;
        }
        if (cur & BTN_DDOWN) {
            pullPad |= 2;
        }
        if (CHECK_BTN_ALL(input->press.button, BTN_B) || CHECK_BTN_ALL(input->press.button, BTN_A)) {
            Pacci_PullStop(play);
            Pacci_UltrahandModeExit(play);
            return 1;
        }
        Pacci_PullTick(play, player, pullPad);
        return 1;
    }

    Pacci_FuseWiggleDetach(play, player, input);

    // B always leaves, held object or not.
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        Pacci_UltrahandModeExit(play);
        return 1;
    }

    // L + R + A stores the structure. Tested BEFORE the plain A branch below, which would
    // otherwise weld or drop on the same press and there would be nothing left to store.
    if (CHECK_BTN_ALL(input->press.button, BTN_A) && (cur & BTN_L) && (cur & BTN_R)) {
        Pacci_BlueprintSave(play);
        return 1;
    }

    // Keep the cane's C button down to build the stored structure again. The press that
    // opened the mode is the same one that starts this count, so it reads as one gesture:
    // tap C for the mode, keep holding it to get your building back. Any C is accepted
    // because the mode does not know which of the four the cane is on, and it can only have
    // been opened by that one.
    if (cur & (BTN_CUP | BTN_CDOWN | BTN_CLEFT | BTN_CRIGHT)) {
        if (sUhMode.summonHold < PACCI_UH_SUMMON_HOLD) {
            sUhMode.summonHold++;
            if (sUhMode.summonHold == PACCI_UH_SUMMON_HOLD) {
                Pacci_BlueprintSummon(play, player);
            }
        }
    } else {
        sUhMode.summonHold = 0;
    }

    // A does three things, in order of what is possible right now: commit the weld
    // the preview is showing, else drop what you are holding, else grab. Welding has
    // to come first — if A always dropped, you could never stick a second piece on
    // without letting go of the first.
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        if (Pacci_IsHoldingUltrahand()) {
            // Weld, else drop. There is no third case any more: a placement no longer happens
            // ON the press, it has already happened by the time you press - the body slid there
            // while you watched. So A over a switch is the same plain release B is, and it lands
            // where it is already sitting.
            // A live bomb has no business being welded to anything. On this one the attach
            // button lights it.
            if (Pacci_UhBombDetonate(play)) {
                Pacci_UltrahandModeExit(play);
            } else if (!Pacci_FuseTryAttach(play)) {
                // Nothing on offer, so this press is a release - and a release ENDS THE MODE.
                // The mode is a control layer over the thing in your hands; with nothing in
                // them there is nothing for it to control, and staying in it just left the
                // D-pad captured for no reason. Exiting begins the fall on the way out.
                Pacci_UltrahandModeExit(play);
            }
        } else {
            Pacci_CastUltrahand(play, player);
        }
        return 1;
    }

    // D-pad edges, detected here rather than read from press.button: the player
    // actor consumes those bits for its own item handling before this runs.
    dpad = 0;
    if (cur & BTN_DUP) {
        dpad |= 1;
    }
    if (cur & BTN_DDOWN) {
        dpad |= 2;
    }
    if (cur & BTN_DLEFT) {
        dpad |= 4;
    }
    if (cur & BTN_DRIGHT) {
        dpad |= 8;
    }
    edge = dpad & ~sUhMode.prevDpad;
    sUhMode.prevDpad = dpad;

    // A locked body answers the pad instead of moving, and answers only edges - a held D-up is one
    // instruction, not sixty.
    Pacci_UhLockedInput(play, edge);
    Pacci_UhHeightInput(sUltrahand.held, edge);
    Pacci_UhHingeInput(sUltrahand.held, edge);

    // Runs on any pad STATE, not just an edge: continuous moves need every frame.
    if (Pacci_IsHoldingUltrahand() && (dpad != 0)) {
        withL = (cur & BTN_L) ? 1 : 0;
        withR = (cur & BTN_R) ? 1 : 0;

        // Every function gets exactly ONE binding, and a modifier owns a whole PLANE rather
        // than one axis of it — that is what makes the layout guessable:
        //
        //   D-pad alone            push / pull            (continuous)
        //   L + D-left / D-right   rotate Y  (yaw)        (snapped, per press)
        //   L + D-up   / D-down    rotate X  (pitch)      (snapped, per press)
        //   R + D-up   / D-down    raise / lower          (continuous)
        //   R + D-left / D-right   slide left / right     (continuous)
        if (withL) {
            if (edge & 8) {
                sUltrahand.baseRot.y += PACCI_UH_ROT_SNAP;
            }
            if (edge & 4) {
                sUltrahand.baseRot.y -= PACCI_UH_ROT_SNAP;
            }
            if (edge & 1) {
                sUltrahand.baseRot.x += PACCI_UH_ROT_SNAP;
            }
            if (edge & 2) {
                sUltrahand.baseRot.x -= PACCI_UH_ROT_SNAP;
            }
        } else if (withR) {
            if (dpad & 1) {
                sUhMode.heightOff += PACCI_UH_MOVE_RATE;
            }
            if (dpad & 2) {
                sUhMode.heightOff -= PACCI_UH_MOVE_RATE;
            }
            sUhMode.heightOff = CLAMP(sUhMode.heightOff, PACCI_UH_HEIGHT_MIN, PACCI_UH_HEIGHT_MAX);

            if (dpad & 8) {
                sUhMode.sideOff += PACCI_UH_MOVE_RATE;
            }
            if (dpad & 4) {
                sUhMode.sideOff -= PACCI_UH_MOVE_RATE;
            }
            sUhMode.sideOff = CLAMP(sUhMode.sideOff, PACCI_UH_SIDE_MIN, PACCI_UH_SIDE_MAX);
        } else {
            if (dpad & 1) {
                sUltrahand.distance += PACCI_UH_DIST_RATE;
            }
            if (dpad & 2) {
                sUltrahand.distance -= PACCI_UH_DIST_RATE;
            }
            sUltrahand.distance = CLAMP(sUltrahand.distance, PACCI_UH_DIST_MIN, PACCI_UH_DIST_MAX);

            // Left/right slides here too. The bare D-pad owns the whole GROUND plane, not just
            // the axis running away from Link: up/down is nearer and further, left/right is
            // across. R owns the vertical plane, and the two planes share the left-right axis,
            // so that binding appears in both on purpose rather than by accident.
            if (dpad & 8) {
                sUhMode.sideOff += PACCI_UH_MOVE_RATE;
            }
            if (dpad & 4) {
                sUhMode.sideOff -= PACCI_UH_MOVE_RATE;
            }
            sUhMode.sideOff = CLAMP(sUhMode.sideOff, PACCI_UH_SIDE_MIN, PACCI_UH_SIDE_MAX);
        }
        // Only the snapped rotations click; a continuous move would machine-gun it.
        if (withL && (edge & 15)) {
            Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    }

    // Z puts the orientation back to however the object was sitting when you grabbed it —
    // TotK's ZL. Untangling a piece you have over-rotated is otherwise seven more presses.
    if (Pacci_IsHoldingUltrahand() && CHECK_BTN_ALL(input->press.button, BTN_Z)) {
        sUltrahand.baseRot = sUltrahand.grabRot;
        Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }

    // Nothing grabbed yet? Show what A would take.
    if (!Pacci_IsHoldingUltrahand()) {
        Pacci_HighlightUltrahandTarget(play);
    }

    Pacci_UpdateUltrahand(play, player);
    // Strict order: the carry moves the root, the formation follows it, and only then
    // is it meaningful to ask where a new weld would land.
    Pacci_FuseFollow(play);
    Pacci_FuseUpdatePreview(play, player);
    Pacci_PlaceUpdatePreview(play); // after the weld preview, which outranks it
    return 1;
}

// ============================================================================
// TEARDOWN
// ============================================================================

// Putting the cane away drops whatever Ultrahand is carrying — you cannot hold an
// object with a cane you are no longer holding — but it deliberately does NOT undo
// a Flip or a Stone.
//
// That is the entire point of the move: you flip an enemy, switch to your sword,
// and hit it while it is down. Restoring flipped enemies on unequip made the skill
// useless, since the target righted itself the instant you reached for a weapon.
// The effect now runs to completion on its own — the flipped enemy's `update` IS
// our function, so it keeps ticking whether or not the cane is in hand.
//
// The cost stays where the design puts it: the cast locks Link in place for the
// animation with no invincibility, so using Flip is what leaves him open.
void Pacci_DropUltrahand(void) {
    if (sUltrahand.held != NULL) {
        // A fall, not an instant hand-back. This used to have to be instant: the fall was driven
        // from Pacci_UpdateUltrahand, which the cane stops calling the moment it is unequipped,
        // so a drop begun here would have frozen on its first frame. Pacci_UltrahandDropTick
        // runs from CustomItems_Update instead, which does not care what Link is holding.
        Pacci_UltrahandBeginDrop();
    }
}

// Hard teardown: undo EVERYTHING, flips included. Not called on unequip — this is
// for tearing the whole subsystem down (a new file, a full state reset).
void Pacci_ReleaseAll(PlayState* play) {
    Pacci_UhTintClear(); // borrowed draw pointers must not outlive the subsystem
    Pacci_UhFireOff();
    Pacci_PlacePressForget();
    Pacci_BackRiderDrop();
    sUhCutTarget = NULL;
    sUhCutTimer = 0;
    sUhThrowActor = NULL;
    sUhThrowTimer = 0;
    Pacci_PullStop(play);
    Pacci_AnchorClear();
    Pacci_UhLightOff(play);
    for (u8 i = 0; i < PACCI_MAX_AFFECTED; i++) {
        if (sPacciPool[i].actor != NULL) {
            Pacci_Restore(&sPacciPool[i]);
        }
    }
    Pacci_DropUltrahand();
}

// Three things end a hold whether the player meant it or not: leaving the scene, getting hit, and
// falling out of the world. Checked here rather than inside the mode's own update because that one
// only runs while the cane is the item in hand, and none of these three waits for that.
//
// The two halves are NOT the same teardown, and the difference matters. A hit or a void-out leaves
// the body where it is and lets it fall: that is a drop, and the player should see it happen. A
// scene change is a teardown - every actor the cane is holding, tinting, anchoring or driving is
// about to be freed, and the pool is full of borrowed update and draw pointers into overlays that
// are going away. Pacci_ReleaseAll hands all of it back first. It was written for exactly this and
// had never been called from anywhere.
void Pacci_UhAbortTick(PlayState* play) {
    Player* player;
    u8 busy;

    if (play == NULL) {
        return;
    }
    busy = (Pacci_UltrahandModeActive() || Pacci_IsHoldingUltrahand()) ? 1 : 0;
    if (!busy) {
        return;
    }
    if ((play->transitionTrigger != TRANS_TRIGGER_OFF) || (gSaveContext.respawnFlag != 0)) {
        Pacci_ReleaseAll(play);
        sUhMode.active = 0;
        sUhMode.prevDpad = 0;
        sUhMode.summonHold = 0;
        return;
    }
    player = GET_PLAYER(play);
    if ((player != NULL) && (player->stateFlags1 & (PLAYER_STATE1_DAMAGED | PLAYER_STATE1_DEAD))) {
        Pacci_UltrahandModeExit(play); // drops what is held on the way out
    }
}
