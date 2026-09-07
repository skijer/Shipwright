#include "z_arms_hook.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
// Skijer's NEI: the switch hook's live "look at it to pick it" selection is now the shared
// remote selector, so the Phantom Hourglass and friends reuse the exact same feel.
#include "../../../../mods/items/helpers/target_select_helper.h"

extern u8 TransformMasks_IsTransformed(void);

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

void ArmsHook_Init(Actor* thisx, PlayState* play);
void ArmsHook_Destroy(Actor* thisx, PlayState* play);
void ArmsHook_Update(Actor* thisx, PlayState* play);
void ArmsHook_Draw(Actor* thisx, PlayState* play);

void ArmsHook_Wait(ArmsHook* this, PlayState* play);
void ArmsHook_Shoot(ArmsHook* this, PlayState* play);
void ArmsHook_SwitchSwap(ArmsHook* this, PlayState* play); // Skijer's NEI switchhook: OoA swap state

// Skijer's NEI switchhook — Oracle of Ages INSTANT position swap (one hook exists at a time, so module
// statics are safe). StartSwap snapshots both actors' hitboxes and teleports them into each other's
// spot; SwitchSwap then HOLDS them there for a couple frames so the aim action can't clobber the
// teleport, then releases Link; ArmsHook_Update re-anchors the hitboxes onto their real position every
// frame meanwhile. sSwap*Start = each actor's ORIGINAL spot, which is the OTHER actor's destination.
#define ARMSHOOK_SWAP_HOLD_FRAMES 2
static Vec3f sSwapLinkStart;
static Vec3f sSwapTargetStart;
static Actor* sSwapTarget;
static s16 sSwapTimer;

// Skijer's NEI switchhook — hitbox re-anchoring (item_switchhook.c). world.pos moves the model, never
// the colliders, and a one-shot shift by the teleport vector is not enough: after the swap each actor
// is still moved by its OWN update — shoved back out of a wall it materialised inside, dropped onto
// the floor below — and the hitbox has to follow every bit of that. So both actors are re-anchored
// onto their current position each frame until they stop moving. The window is generous because a
// swapped object can fall for a while; re-anchoring is an absolute reposition, so it costs nothing to
// keep running and cannot drift.
#define ARMSHOOK_SWAP_SETTLE_FRAMES 40
extern void SwitchHook_CaptureSwapColliders(PlayState* play, s32 slot, Actor* actor);
extern void SwitchHook_ReanchorSwapColliders(PlayState* play);
extern void SwitchHook_ClearSwapColliders(void);
static s16 sSwapAnchorTimer = 0;

// Skijer's NEI switchhook — while > 0, the PLAYER's scene collision is fully bypassed, exactly like
// the NoClip cheat CVar (z_bgcheck.c consults SwitchHook_PlayerNoClip() next to that CVar), so the
// swap can materialize Link behind walls / below floors.
static s16 sSwitchNoClipTimer = 0;

s32 SwitchHook_PlayerNoClip(void) {
    return sSwitchNoClipTimer > 0;
}

// Skijer's NEI switchhook — an actor is swappable if it's an enemy, a prop (pots, crates, torches,
// grass...), a chest, or an NPC (signs, scarecrows, cuccos...). Bosses, the player, background and
// scene actors are never swapped. This is exactly target_select_helper's common-target notion, so
// the definition lives there now and both games (and every other item that wants a remote pick)
// share one copy.
static s32 ArmsHook_IsSwappable(Actor* actor) {
    return TargetSelect_IsCommonTarget(actor);
}

// Skijer's NEI switchhook — Ultrahand-style CONTINUOUS selection: while the switch hook is in hand,
// the swappable actor closest to Link's LOOK DIRECTION (yaw only — Y is ignored), within longshot
// range, is the live selection. It's tinted blue every frame; firing auto-aims the hook at it, so
// you can swap without precise aiming. The scan itself is TargetSelect_Scan.
static Actor* sSwitchSelection = NULL;

static Actor* ArmsHook_SelectSwapCandidate(PlayState* play) {
    return TargetSelect_Scan(play, TargetSelect_IsCommonTarget);
}

// Nearest swappable actor within `range` of `pos` (scans only the swappable categories). The switch
// hook uses this to find its target by PROXIMITY, so the swap can start before the hook's collider
// reaches the actor and breaks it (pots) / grabs it (chests).
static Actor* ArmsHook_FindSwappable(PlayState* play, Vec3f* pos, f32 range) {
    return TargetSelect_FindNearest(play, NULL, 0, TargetSelect_IsCommonTarget, pos, range);
}

const ActorInit Arms_Hook_InitVars = {
    ACTOR_ARMS_HOOK,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_LINK_BOY,
    sizeof(ArmsHook),
    (ActorFunc)ArmsHook_Init,
    (ActorFunc)ArmsHook_Destroy,
    (ActorFunc)ArmsHook_Update,
    (ActorFunc)ArmsHook_Draw,
    NULL,
};

static ColliderQuadInit sQuadInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000080, 0x00, 0x01 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static Vec3f sUnusedVec1 = { 0.0f, 0.5f, 0.0f };
static Vec3f sUnusedVec2 = { 0.0f, 0.5f, 0.0f };

static Color_RGB8 sUnusedColors[] = {
    { 255, 255, 100 },
    { 255, 255, 50 },
};

static Vec3f D_80865B70 = { 0.0f, 0.0f, 0.0f };
static Vec3f D_80865B7C = { 0.0f, 0.0f, 900.0f };
static Vec3f D_80865B88 = { 0.0f, 500.0f, -3000.0f };
static Vec3f D_80865B94 = { 0.0f, -500.0f, -3000.0f };
static Vec3f D_80865BA0 = { 0.0f, 500.0f, 1200.0f };
static Vec3f D_80865BAC = { 0.0f, -500.0f, 1200.0f };

void ArmsHook_SetupAction(ArmsHook* this, ArmsHookActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void ArmsHook_Init(Actor* thisx, PlayState* play) {
    ArmsHook* this = (ArmsHook*)thisx;

    Collider_InitQuad(play, &this->collider);
    Collider_SetQuad(play, &this->collider, &this->actor, &sQuadInit);
    ArmsHook_SetupAction(this, ArmsHook_Wait);
    this->unk_1E8 = this->actor.world.pos;
}

void ArmsHook_Destroy(Actor* thisx, PlayState* play) {
    ArmsHook* this = (ArmsHook*)thisx;

    if (this->grabbed != NULL) {
        this->grabbed->flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
    }
    // Skijer's NEI switchhook: drop the module-static selection/swap pointers with the hook actor,
    // so a scene change / item swap can't leave them dangling; end any player-noclip window.
    sSwitchSelection = NULL;
    sSwapTarget = NULL;
    sSwitchNoClipTimer = 0;
    sSwapAnchorTimer = 0;
    SwitchHook_ClearSwapColliders();
    Collider_DestroyQuad(play, &this->collider);
}

void ArmsHook_Wait(ArmsHook* this, PlayState* play) {
    extern u8 Nei_ArmsHookVariant(Player * player);

    // Skijer's NEI switchhook — Ultrahand-style live selection: every frame the hook is IN HAND,
    // pick the swappable actor in Link's look direction (Y ignored, longshot range) and tint it
    // blue. Firing then auto-aims at it, so you can swap without precise aiming.
    if (this->actor.parent != NULL) {
        if (Nei_ArmsHookVariant(GET_PLAYER(play)) == 4) { // NEI_HOOK_VARIANT_SWITCHHOOK
            extern u8 SwitchHook_IsAimingManual(void);

            if (SwitchHook_IsAimingManual()) {
                // C-Up manual aim: you fire exactly where you look — no live selection, no auto-aim.
                sSwitchSelection = NULL;
            } else {
                sSwitchSelection = ArmsHook_SelectSwapCandidate(play);
                if (sSwitchSelection != NULL) {
                    // Blue tint (colorFlag 0 = blue, xluFlag 0 = OPA buffer)
                    TargetSelect_Highlight(sSwitchSelection, 4);
                }
            }
        } else {
            sSwitchSelection = NULL;
        }
    }

    if (this->actor.parent == NULL) {
        Player* player = GET_PLAYER(play);
        // Skijer's NEI hookshot overhaul: reach + travel speed scale with the active hookshot
        // variant (reach ~= speed * timer). Vanilla OoT was a flat 20.0f with timer 13 (Hookshot)
        // / 26 (Longshot).
        //   Hookshot (0): 20 * 13     Longshot (1): 20 * 26
        //   Ultrashot (2): 40 * 26 (2x speed)     Clawshot (3): 15 * 35 (0.75x speed)
        //   Switch Hook (4): 20 * 26 (Longshot range; swaps on hit)
        u8 variant = Nei_ArmsHookVariant(player);
        f32 speed = 20.0f;
        s32 timer = 26;

        // Skijer's NEI switchhook — charges: each fired swap costs one of 5; empty/grayed out means
        // the shot simply doesn't come out (error beep, hook stays in hand).
        if (variant == 4) {
            extern s32 SwitchHook_ConsumeCharge(void);
            extern void SwitchHook_OnFired(Player * p);

            if (!SwitchHook_ConsumeCharge()) {
                // Should not be reached (func_808350A4 blocks the launch player-side first), but if
                // it is: restore the FULL held state — parent alone leaves player->heldActor NULL
                // ("hook in flight" forever = softlock).
                Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                this->actor.parent = &player->actor;
                player->heldActor = &this->actor;
                player->actor.child = &this->actor;
                return;
            }
            // Manual aim ends at the launch (drops the aim camera; the shot direction is already
            // the player's aimed world.rot from the vanilla aim flow).
            SwitchHook_OnFired(player);
        }
        switch (variant) {
            case 0: // NEI_HOOK_VARIANT_HOOKSHOT — dist 1
                speed = 20.0f;
                timer = 13;
                break;
            case 1: // NEI_HOOK_VARIANT_LONGSHOT — dist 2
                speed = 20.0f;
                timer = 26;
                break;
            case 2: // NEI_HOOK_VARIANT_ULTRASHOT — dist 4, 2x speed
                speed = 40.0f;
                timer = 26;
                break;
            case 3: // NEI_HOOK_VARIANT_CLAWSHOT — dist 2, 0.75x speed
                speed = 15.0f;
                timer = 35;
                break;
            case 4: // NEI_HOOK_VARIANT_SWITCHHOOK — dist 2 (Longshot range; dist 1 was too short); swaps on hit
                speed = 20.0f;
                timer = 26;
                break;
        }
        // Switch Hook: the swap is a clean position exchange — kill the hook's attack damage so it
        // doesn't BREAK the pot or damage/grab the actor it's supposed to swap with. Detection is by
        // proximity in ArmsHook_Shoot, not by a collider hit. Other variants keep the hookshot dmg.
        if (variant == 4) { // NEI_HOOK_VARIANT_SWITCHHOOK
            this->collider.info.toucher.dmgFlags = 0;
            this->collider.info.toucher.damage = 0;
        } else {
            this->collider.info.toucher.dmgFlags = 0x00000080; // DMG_HOOKSHOT
            this->collider.info.toucher.damage = 1;
        }

        // Switch Hook auto-aim: fly straight at the live selection (Actor_SetProjectileSpeed builds
        // velocity.y from world.rot.x, and speedXZ resolves along world.rot.y each move — so aim
        // BEFORE it). Refresh its tint so it stays blue in flight.
        if ((variant == 4) && (sSwitchSelection != NULL) && (sSwitchSelection->update != NULL)) {
            f32 dx = sSwitchSelection->world.pos.x - this->actor.world.pos.x;
            f32 dy = sSwitchSelection->focus.pos.y - this->actor.world.pos.y;
            f32 dz = sSwitchSelection->world.pos.z - this->actor.world.pos.z;
            f32 distXZ = sqrtf((dx * dx) + (dz * dz));

            this->actor.world.rot.y = Math_Atan2S(dz, dx);
            this->actor.world.rot.x = Math_Atan2S(distXZ, -dy);
            this->actor.shape.rot = this->actor.world.rot;
            TargetSelect_Highlight(sSwitchSelection, 30);
        }

        ArmsHook_SetupAction(this, ArmsHook_Shoot);
        Actor_SetProjectileSpeed(&this->actor, speed);
        this->actor.parent = &player->actor;
        this->timer = timer * CVarGetFloat(CVAR_CHEAT("HookshotReachMultiplier"), 1.0f);

        // Skijer's NEI clawshot rework: the Clawshot Bullet Time hold was removed — the
        // ClawshotBT_NoteShotFired() bookkeeping call is gone with it (the ClawshotBT_* functions
        // stay defined in custom_items_common.c, just no longer fed from here).
    }
}

void ArmsHook_PullPlayer(ArmsHook* this) {
    this->actor.child = this->actor.parent;
    this->actor.parent->parent = &this->actor;
}

s32 ArmsHook_AttachToPlayer(ArmsHook* this, Player* player) {
    player->actor.child = &this->actor;
    player->heldActor = &this->actor;
    if (this->actor.child != NULL) {
        player->actor.parent = NULL;
        this->actor.child = NULL;
        return true;
    }
    return false;
}

void ArmsHook_DetachHookFromActor(ArmsHook* this) {
    if (this->grabbed != NULL) {
        this->grabbed->flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
        this->grabbed = NULL;
    }
}

s32 ArmsHook_CheckForCancel(ArmsHook* this) {
    Player* player = (Player*)this->actor.parent;

    if (Player_HoldsHookshot(player)) {
        if ((player->itemAction != player->heldItemAction) || (player->actor.flags & ACTOR_FLAG_TALK) ||
            ((player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_DAMAGED)))) {
            this->timer = 0;
            ArmsHook_DetachHookFromActor(this);
            Math_Vec3f_Copy(&this->actor.world.pos, &player->unk_3C8);
            return 1;
        }
    }
    return 0;
}

void ArmsHook_AttachHookToActor(ArmsHook* this, Actor* actor) {
    actor->flags |= ACTOR_FLAG_HOOKSHOT_ATTACHED;
    this->grabbed = actor;
    Math_Vec3f_Diff(&actor->world.pos, &this->actor.world.pos, &this->grabbedDistDiff);
}

// Skijer's NEI switchhook — begin the position swap with `target`: record start positions, STUN the
// target if it's an enemy (blue freeze, like the hookshot), SILENCE the flying-hook rattle, play the
// swap sfx, and hand off to the swap-hold state.
static void ArmsHook_StartSwap(ArmsHook* this, PlayState* play, Player* player, Actor* target) {
    sSwapLinkStart = player->actor.world.pos; // Link's original spot = target's destination
    sSwapTargetStart = target->world.pos;     // target's original spot = Link's destination
    sSwapTarget = target;
    sSwapTimer = 0;
    // Full player noclip (the NoClip cheat mechanism) through the swap + a few settle frames.
    sSwitchNoClipTimer = ARMSHOOK_SWAP_HOLD_FRAMES + 6;

    // Snapshot both actors' hitboxes WHILE THEY ARE STILL AT REST — the offsets recorded here are what
    // "the collider sits here relative to its actor" means for the rest of the swap.
    SwitchHook_CaptureSwapColliders(play, 0, &player->actor);
    SwitchHook_CaptureSwapColliders(play, 1, target);

    // INSTANT teleport: move BOTH actors (world.pos, prevPos, home.pos) into each other's spot right
    // away, freezing their physics. world.pos carries the model, the bg checks and the dyna mesh —
    // but NOT the colliders, which is what the re-anchor at the end of this function is for.
    player->actor.world.pos = sSwapTargetStart;
    player->actor.prevPos = sSwapTargetStart;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    // Clear the ground/wall flags: with the GROUND flag still set, the player's scene collision
    // snaps him back up to his OLD floorHeight — a downward swap would move only the object.
    player->actor.bgCheckFlags = 0;
    player->invincibilityTimer = 20;

    target->world.pos = sSwapLinkStart;
    target->prevPos = sSwapLinkStart;
    target->home.pos = sSwapLinkStart;
    target->velocity.x = 0.0f;
    target->velocity.y = 0.0f;
    target->velocity.z = 0.0f;
    target->speedXZ = 0.0f;
    target->bgCheckFlags = 0;

    // Skijer's NEI switchhook — MOVE THE HITBOXES, not just the models (the snapshot above was taken
    // before the teleport). world.pos alone leaves every collider behind, and a one-shot shift by the
    // teleport vector is not enough either: both actors keep being moved AFTER this — shoved back out
    // of a wall they materialised inside, dropped onto the floor below — and the hitbox has to follow
    // all of that. ArmsHook_Update re-anchors them onto their current position every frame until they
    // settle; this first pass just gets them right for the swap frame itself.
    SwitchHook_ReanchorSwapColliders(play);
    sSwapAnchorTimer = ARMSHOOK_SWAP_SETTLE_FRAMES;

    // Recolor the selected actor (highlight — the Ultrahand "recolor your selection" feel). For
    // enemies this doubles as the hookshot stun (blue freeze); props/chests/NPCs just flash to show
    // what got swapped. (colorFlag 0 = blue, xluFlag 0 = OPA)
    Actor_SetColorFilter(target, 0, 255, 0, 80);
    // Kill the flying-hook chain rattle at its SOURCE: it's a flagged sfx re-emitted from the
    // player actor every frame while actor.sfx stays set (Actor_PlaySfx_Flagged2's "dragging" drone) —
    // stopping the playing instance alone lets it re-spawn next frame.
    player->actor.sfx = 0;
    Audio_StopSfxByPos(&player->actor.projectedPos);
    // Same treatment for the actor that just got yanked across the room: a continuous
    // sfx it was holding would carry on from its new spot with nothing to stop it.
    if (target->sfx != 0) {
        target->sfx = 0;
        Audio_StopSfxByPos(&target->projectedPos);
    }
    // Short ONE-SHOT swap cue. A long looping ambience sample (e.g. NA_SE_EV_WARP_HOLE) never stops
    // reliably — it droned on forever after the swap.
    // Fired ONCE, so the continuous flag must come off. Bit 0x800 marks an sfx that the
    // caller re-requests every frame; the sound bank only ages those while they sit in
    // SFX_STATE_QUEUED, and its auto-reclaim path is explicitly gated on !(sfxId & 0xC00).
    // Fire one with the flag on and never ask again and it parks in PLAYING forever — that
    // is the sound that kept ringing after every swap. The sample is picked by sfxId & 0x1FF,
    // so clearing 0x800 changes the lifetime, never which sound you hear.
    Audio_PlaySoundGeneral(NA_SE_IT_HOOKSHOT_STICK_OBJ - SFX_FLAG, &this->actor.projectedPos, 4,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    ArmsHook_SetupAction(this, ArmsHook_SwitchSwap);
}

void ArmsHook_Shoot(ArmsHook* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    // Skijer's NEI hookshot overhaul: which variant is in flight (fixed for the duration of the shot).
    extern u8 Nei_ArmsHookVariant(Player * player);
    u8 hookVariant = Nei_ArmsHookVariant(player);
    u8 clawshot = (hookVariant == 3);   // NEI_HOOK_VARIANT_CLAWSHOT
    u8 switchhook = (hookVariant == 4); // NEI_HOOK_VARIANT_SWITCHHOOK — swaps positions on hit
    Actor* touchedActor;
    Actor* grabbed;
    Vec3f bodyDistDiffVec;
    Vec3f newPos;
    f32 bodyDistDiff;
    f32 phi_f16;
    DynaPolyActor* dynaPolyActor;
    f32 sp94;
    f32 sp90;
    s32 pad;
    CollisionPoly* poly;
    s32 bgId;
    Vec3f sp78;
    Vec3f prevFrameDiff;
    Vec3f sp60;
    f32 sp5C;
    f32 sp58;
    f32 velocity;
    s32 pad1;

    if ((this->actor.parent == NULL) || (!Player_HoldsHookshot(player))) {
        ArmsHook_DetachHookFromActor(this);
        Actor_Kill(&this->actor);
        return;
    }

    Actor_PlaySfx_Flagged2(&player->actor, NA_SE_IT_HOOKSHOT_CHAIN - SFX_FLAG);
    ArmsHook_CheckForCancel(this);

    if ((this->timer != 0) && (this->collider.base.atFlags & AT_HIT) &&
        (this->collider.info.atHitInfo->elemType != ELEMTYPE_UNK4)) {
        touchedActor = this->collider.base.at;

        // Skijer's NEI — Switch Hook: it swaps via proximity in the flight branch (its collider deals
        // no damage), so a collider hit shouldn't normally reach here. Guard anyway — on a swappable
        // hit run the same swap, and never let the switch hook fall through to the vanilla grab/pull.
        if (switchhook) {
            if (ArmsHook_IsSwappable(touchedActor)) {
                ArmsHook_StartSwap(this, play, player, touchedActor);
                return;
            }
        }
        // Skijer's NEI — Clawshot: grab any NON-BOSS ENEMY (ACTORCAT_ENEMY excludes bosses, props,
        // and NPCs by category) and drag it back to Link, bypassing the vanilla HOOKSHOT_PULLS_* /
        // BUMP_HOOKABLE gate. It never pulls Link toward the target — the clawshot only reels
        // enemies in.
        else if (clawshot) {
            if ((touchedActor->update != NULL) && (touchedActor->category == ACTORCAT_ENEMY)) {
                ArmsHook_AttachHookToActor(this, touchedActor);
            }
        } else if ((touchedActor->update != NULL) &&
                   (touchedActor->flags & (ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR | ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER))) {
            if (this->collider.info.atHitInfo->bumperFlags & BUMP_HOOKABLE) {
                ArmsHook_AttachHookToActor(this, touchedActor);
                if (CHECK_FLAG_ALL(touchedActor->flags, ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER)) {
                    ArmsHook_PullPlayer(this);
                }
            }
        }
        this->timer = 0;
        Audio_PlaySoundGeneral(NA_SE_IT_ARROW_STICK_CRE, &this->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    } else if (DECR(this->timer) == 0) {
        grabbed = this->grabbed;
        if (grabbed != NULL) {
            if ((grabbed->update == NULL) || !CHECK_FLAG_ALL(grabbed->flags, ACTOR_FLAG_HOOKSHOT_ATTACHED)) {
                grabbed = NULL;
                this->grabbed = NULL;
            } else if (this->actor.child != NULL) {
                sp94 = Actor_WorldDistXYZToActor(&this->actor, grabbed);
                sp90 = sqrtf(SQ(this->grabbedDistDiff.x) + SQ(this->grabbedDistDiff.y) + SQ(this->grabbedDistDiff.z));
                Math_Vec3f_Diff(&grabbed->world.pos, &this->grabbedDistDiff, &this->actor.world.pos);
                if (50.0f < (sp94 - sp90)) {
                    ArmsHook_DetachHookFromActor(this);
                    grabbed = NULL;
                }
            }
        }

        bodyDistDiff = Math_Vec3f_DistXYZAndStoreDiff(&player->unk_3C8, &this->actor.world.pos, &bodyDistDiffVec);
        if (bodyDistDiff < 30.0f) {
            velocity = 0.0f;
            phi_f16 = 0.0f;
        } else {
            if (this->actor.child != NULL) {
                velocity = 30.0f;
            } else if (grabbed != NULL) {
                velocity = 50.0f;
            } else {
                velocity = 200.0f;
            }
            // Skijer's NEI — Ultrashot: everything twice as fast, including Link's travel.
            if (hookVariant == 2) { // NEI_HOOK_VARIANT_ULTRASHOT
                velocity *= 2.0f;
            }
            phi_f16 = bodyDistDiff - velocity;
            if (bodyDistDiff <= velocity) {
                phi_f16 = 0.0f;
            }
            velocity = phi_f16 / bodyDistDiff;
        }

        newPos.x = bodyDistDiffVec.x * velocity;
        newPos.y = bodyDistDiffVec.y * velocity;
        newPos.z = bodyDistDiffVec.z * velocity;

        if (this->actor.child == NULL) {
            if ((grabbed != NULL) && (grabbed->id == ACTOR_BG_SPOT06_OBJECTS)) {
                Math_Vec3f_Diff(&grabbed->world.pos, &this->grabbedDistDiff, &this->actor.world.pos);
                phi_f16 = 1.0f;
            } else {
                Math_Vec3f_Sum(&player->unk_3C8, &newPos, &this->actor.world.pos);
                if (grabbed != NULL) {
                    Math_Vec3f_Sum(&this->actor.world.pos, &this->grabbedDistDiff, &grabbed->world.pos);
                    // Skijer's NEI — Clawshot: zero the grabbed enemy's own motion so its AI
                    // (gravity, walk, attack pursuit) can't fight the drag we apply via world.pos.
                    if (clawshot) {
                        grabbed->velocity.x = 0.0f;
                        grabbed->velocity.y = 0.0f;
                        grabbed->velocity.z = 0.0f;
                        grabbed->speedXZ = 0.0f;
                        grabbed->gravity = 0.0f;
                    }
                }
            }
        } else {
            Math_Vec3f_Diff(&bodyDistDiffVec, &newPos, &player->actor.velocity);
            player->actor.world.rot.x =
                Math_Atan2S(sqrtf(SQ(bodyDistDiffVec.x) + SQ(bodyDistDiffVec.z)), -bodyDistDiffVec.y);
        }

        if (phi_f16 < 50.0f) {
            ArmsHook_DetachHookFromActor(this);
            if (phi_f16 == 0.0f) {
                ArmsHook_SetupAction(this, ArmsHook_Wait);
                if (ArmsHook_AttachToPlayer(this, player)) {
                    Math_Vec3f_Diff(&this->actor.world.pos, &player->actor.world.pos, &player->actor.velocity);
                    // Skijer's NEI clawshot rework: the Clawshot Bullet Time hold-in-place was
                    // removed (ClawshotBT_TryStartOnArrival is no longer consulted) — always the
                    // vanilla -20 downward kick.
                    player->actor.velocity.y -= 20.0f;
                }
            }
        }
    } else {
        Actor_MoveXZGravity(&this->actor);

        // Skijer's NEI switchhook — once the hook has flown clear of Link, scout ahead for a swappable
        // actor (enemy/prop/chest/NPC) and start the position swap. Detecting by PROXIMITY (not by the
        // collider hit) means the swap fires before the hook reaches the actor, so pots aren't broken
        // and chests aren't grabbed on the way in. The "clear of Link" gate keeps an actor right next
        // to you from being grabbed the instant you fire past it.
        if (switchhook) {
            f32 hdx = this->actor.world.pos.x - player->actor.world.pos.x;
            f32 hdy = this->actor.world.pos.y - player->actor.world.pos.y;
            f32 hdz = this->actor.world.pos.z - player->actor.world.pos.z;
            if (((hdx * hdx) + (hdy * hdy) + (hdz * hdz)) > (50.0f * 50.0f)) {
                Actor* swapTarget = NULL;

                if ((sSwitchSelection != NULL) && (sSwitchSelection->update != NULL)) {
                    // A live selection was made — the hook auto-aims at it, so swap ONLY with it
                    // (an unselected object crossing the path must not intercept the swap).
                    f32 sdx = sSwitchSelection->world.pos.x - this->actor.world.pos.x;
                    f32 sdy = sSwitchSelection->world.pos.y - this->actor.world.pos.y;
                    f32 sdz = sSwitchSelection->world.pos.z - this->actor.world.pos.z;

                    if (((sdx * sdx) + (sdy * sdy) + (sdz * sdz)) < (60.0f * 60.0f)) {
                        swapTarget = sSwitchSelection;
                    }
                } else {
                    swapTarget = ArmsHook_FindSwappable(play, &this->actor.world.pos, 45.0f);
                }
                if (swapTarget != NULL) {
                    ArmsHook_StartSwap(this, play, player, swapTarget);
                    return;
                }
            }
        }

        Math_Vec3f_Diff(&this->actor.world.pos, &this->actor.prevPos, &prevFrameDiff);
        Math_Vec3f_Sum(&this->unk_1E8, &prevFrameDiff, &this->unk_1E8);
        this->actor.shape.rot.x = Math_Atan2S(this->actor.speedXZ, -this->actor.velocity.y);
        sp60.x = this->unk_1F4.x - (this->unk_1E8.x - this->unk_1F4.x);
        sp60.y = this->unk_1F4.y - (this->unk_1E8.y - this->unk_1F4.y);
        sp60.z = this->unk_1F4.z - (this->unk_1E8.z - this->unk_1F4.z);
        u16 buttonsToCheck = BTN_A | BTN_B | BTN_R | BTN_CUP | BTN_CLEFT | BTN_CRIGHT | BTN_CDOWN;
        if (CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) != 0) {
            buttonsToCheck |= BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT;
        }
        // Skijer's NEI switchhook — NOCLIP: the switch hook flies straight THROUGH scene geometry
        // (no wall/floor line test), so it can reach the blue-selected object even behind a wall.
        if (!switchhook &&
            BgCheck_EntityLineTest1(&play->colCtx, &sp60, &this->unk_1E8, &sp78, &poly, true, true, true, true,
                                    &bgId) &&
            !func_8002F9EC(play, &this->actor, poly, bgId, &sp78)) {
            sp5C = COLPOLY_GET_NORMAL(poly->normal.x);
            sp58 = COLPOLY_GET_NORMAL(poly->normal.z);
            Math_Vec3f_Copy(&this->actor.world.pos, &sp78);
            this->actor.world.pos.x += 10.0f * sp5C;
            this->actor.world.pos.z += 10.0f * sp58;
            this->timer = 0;
            // Skijer's NEI — Clawshot never grapples surfaces (it only reels enemies in), so it
            // reflects off every wall/floor instead of pulling Link toward the anchor. (The old
            // Clawshot Bullet Time ClawshotBT_NoteHitSurface bookkeeping is gone with the hold.)
            if (!clawshot && SurfaceType_IsHookshotSurface(&play->colCtx, poly, bgId)) {
                if (bgId != BGCHECK_SCENE) {
                    dynaPolyActor = DynaPoly_GetActor(&play->colCtx, bgId);
                    if (dynaPolyActor != NULL) {
                        ArmsHook_AttachHookToActor(this, &dynaPolyActor->actor);
                    }
                }
                ArmsHook_PullPlayer(this);
                Audio_PlaySoundGeneral(NA_SE_IT_HOOKSHOT_STICK_OBJ, &this->actor.projectedPos, 4,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            } else {
                CollisionCheck_SpawnShieldParticlesMetal(play, &this->actor.world.pos);
                Audio_PlaySoundGeneral(NA_SE_IT_HOOKSHOT_REFLECT, &this->actor.projectedPos, 4,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
        } else if (CHECK_BTN_ANY(play->state.input[0].press.button, (buttonsToCheck))) {
            this->timer = 0;
        }
    }
}

// Skijer's NEI switchhook — end the swap the SAME way the vanilla hookshot recovers from a miss:
// return the hook to Link's hand and reset it to Wait. This releases Link back to holding the hookshot
// (free to move / re-fire) instead of leaving him frozen in the aim pose after the swap.
static void ArmsHook_ReleaseAfterSwap(ArmsHook* this, Player* player) {
    sSwapTarget = NULL;
    // Belt and braces on the chain rattle. It is a CONTINUOUS flagged sfx, and the
    // flagged-audio pass in Actor_UpdateAll runs off the projection loop that walks
    // every actor — so anything that leaves it latched keeps re-emitting it with no
    // owner left to clear it. StartSwap already clears the source on the player;
    // this kills any instance that is still ringing when the swap lets go.
    // The de-flagged id is what actually reaches the sound bank (the drone is queued as
    // NA_SE_IT_HOOKSHOT_CHAIN - SFX_FLAG), and the stop compares sfxId for exact equality,
    // so the flagged constant would silently match nothing.
    Audio_StopSfxById(NA_SE_IT_HOOKSHOT_CHAIN - SFX_FLAG);
    Math_Vec3f_Copy(&this->actor.world.pos, &player->unk_3C8);
    this->unk_1E8 = player->unk_3C8;
    this->timer = 0;
    ArmsHook_SetupAction(this, ArmsHook_Wait);
    ArmsHook_AttachToPlayer(this, player);
}

// Skijer's NEI switchhook — after StartSwap teleports both actors, HOLD them at their swapped
// destinations for a couple frames (a STATIC hold, not a moving ease) so each actor's own update
// resyncs its collider from world.pos — catching up to the teleport — and the aim action can't pull
// Link off the spot. Then release Link back to holding the hookshot at the new position.
void ArmsHook_SwitchSwap(ArmsHook* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* target = sSwapTarget;

    // Target vanished mid-swap (killed/despawned) — just release cleanly.
    if ((target == NULL) || (target->update == NULL)) {
        ArmsHook_ReleaseAfterSwap(this, player);
        return;
    }

    // Keep BOTH pinned at their (already-swapped) destinations. The hitboxes need no attention here:
    // ArmsHook_Update re-anchors them onto whatever position this leaves behind, every frame.
    player->actor.world.pos = sSwapTargetStart; // Link's destination
    player->actor.prevPos = sSwapTargetStart;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    // Keep the ground flags cleared through the hold — the player's scene collision runs BEFORE this
    // actor each frame and would otherwise snap him back up to his OLD floor on downward swaps.
    player->actor.bgCheckFlags = 0;
    player->invincibilityTimer = 10;

    target->world.pos = sSwapLinkStart; // target's destination
    target->prevPos = sSwapLinkStart;
    target->home.pos = sSwapLinkStart;
    target->velocity.x = 0.0f;
    target->velocity.y = 0.0f;
    target->velocity.z = 0.0f;
    target->speedXZ = 0.0f;
    target->bgCheckFlags = 0;

    // Keep the hook on Link so its drawn chain stays short instead of streaking across the room.
    this->actor.world.pos = sSwapTargetStart;
    this->unk_1E8 = sSwapTargetStart;

    if (++sSwapTimer >= ARMSHOOK_SWAP_HOLD_FRAMES) {
        // One-shot: clear the continuous flag (see ArmsHook_StartSwap).
        Audio_PlaySoundGeneral(NA_SE_EV_ROLL_STAND - SFX_FLAG, &player->actor.projectedPos, 4,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        ArmsHook_ReleaseAfterSwap(this, player);
    }
}

void ArmsHook_Update(Actor* thisx, PlayState* play) {
    ArmsHook* this = (ArmsHook*)thisx;

    // Skijer's NEI switchhook — run down the post-swap player-noclip window.
    if (sSwitchNoClipTimer > 0) {
        sSwitchNoClipTimer--;
    }

    this->actionFunc(this, play);

    // Skijer's NEI switchhook — pin both swapped actors' hitboxes onto wherever they ACTUALLY are now.
    // This runs after the action func so it sees the final position for the frame, and it keeps running
    // for a while after the swap releases so wall push-outs and falls carry the colliders with them.
    if (sSwapAnchorTimer > 0) {
        sSwapAnchorTimer--;
        SwitchHook_ReanchorSwapColliders(play);
        if (sSwapAnchorTimer == 0) {
            SwitchHook_ClearSwapColliders();
        }
    }

    this->unk_1F4 = this->unk_1E8;
}

void ArmsHook_Draw(Actor* thisx, PlayState* play) {
    s32 pad;
    ArmsHook* this = (ArmsHook*)thisx;
    Player* player = GET_PLAYER(play);
    Vec3f sp78;
    Vec3f sp6C;
    Vec3f sp60;
    f32 sp5C;
    f32 sp58;

    if ((player->actor.draw != NULL) && (player->rightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT)) {
        // Transformed: OOT's right-hand limb matrix isn't set up (MM form draws instead),
        // so Matrix_MultVec3f would use stale data → chain/tip draw at wrong position.
        // Only draw when the hookshot is actively shooting (matrices set during first-person).
        // In Wait state, the hookshot is invisible (held in hand) anyway in vanilla.
        if (TransformMasks_IsTransformed() && (ArmsHook_Shoot != this->actionFunc)) {
            return;
        }

        OPEN_DISPS(play->state.gfxCtx);

        if ((ArmsHook_Shoot != this->actionFunc) || (this->timer <= 0)) {
            Matrix_MultVec3f(&D_80865B70, &this->unk_1E8);
            Matrix_MultVec3f(&D_80865B88, &sp6C);
            Matrix_MultVec3f(&D_80865B94, &sp60);
            this->hookInfo.active = 0;
        } else {
            Matrix_MultVec3f(&D_80865B7C, &this->unk_1E8);
            Matrix_MultVec3f(&D_80865BA0, &sp6C);
            Matrix_MultVec3f(&D_80865BAC, &sp60);
        }

        func_80090480(play, &this->collider, &this->hookInfo, &sp6C, &sp60);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) &&
            CVarGetInteger(CVAR_ENHANCEMENT("ScaleAdultEquipmentAsChild"), 0) && LINK_IS_CHILD) {
            Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
        }
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        // Twilight Upgrade — Clawshot mode: swap the tip DL to MM's claw
        // (object_link_child_DL_01D960). MM's ArmsHook_Draw also calls this
        // DL during held state, but MM additionally updates the actor's
        // world.pos to track Link's hand — OOT doesn't, so during held state
        // (Wait action) the actor sits at the LAST shoot/anchor position and
        // any tip draw lands there instead of at the body's nose. So we
        // skip the MM tip entirely when not actively shooting; the MM body
        // DL (gLinkHumanRightHandHoldingHookshotDL drawn at the limb in
        // z_player_lib.c) carries the model's silhouette on its own when
        // held. During shoot the actor IS at the flying position, so the
        // MM tip lines up correctly with the chain end.
        {
            extern u8 TwilightUpgrade_IsClawshotActive(void);
            extern void* MmAssets_LoadHookshotTipDL(void);
            extern Gfx* MmDL_Or(Gfx * vanillaDL, Gfx * mmDL);
            u8 isShooting = (this->actionFunc == ArmsHook_Shoot) && (this->timer > 0);
            if (GameInteractor_Should(VB_DRAW_HOOKSHOT_TIP, true, player, play)) {
                Gfx* tipDL = gLinkAdultHookshotTipDL;
                u8 useMmTip = TwilightUpgrade_IsClawshotActive();
                if (useMmTip) {
                    tipDL = MmDL_Or(tipDL, (Gfx*)MmAssets_LoadHookshotTipDL());
                }
                // Skip the tip entirely when clawshot mode is held-not-shooting.
                // Vanilla OOT keeps drawing its own tip here even when not
                // shooting (the body DL is offset enough that they overlap
                // unnoticeably), but the MM tip lands far from the body in
                // that case and is jarring.
                if (!(useMmTip && !isShooting)) {
                    gSPDisplayList(POLY_OPA_DISP++, tipDL);
                }
            }
        }
        Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
        Math_Vec3f_Diff(&player->unk_3C8, &this->actor.world.pos, &sp78);
        sp58 = SQ(sp78.x) + SQ(sp78.z);
        sp5C = sqrtf(sp58);
        Matrix_RotateY(Math_FAtan2F(sp78.x, sp78.z), MTXMODE_APPLY);
        Matrix_RotateX(Math_FAtan2F(-sp78.y, sp5C), MTXMODE_APPLY);
        if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) &&
            CVarGetInteger(CVAR_ENHANCEMENT("ScaleAdultEquipmentAsChild"), 0) && LINK_IS_CHILD) {
            Matrix_Scale(0.012f, 0.012f, sqrtf(SQ(sp78.y) + sp58) * 0.01f, MTXMODE_APPLY);
        } else {
            Matrix_Scale(0.015f, 0.015f, sqrtf(SQ(sp78.y) + sp58) * 0.01f, MTXMODE_APPLY);
        }
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        // Twilight Upgrade — Clawshot mode: swap chain DL to MM's gameplay_keep
        // hookshot chain so the visual matches the body DL swap in z_player_lib.c.
        // Fallback to OOT chain when mm.o2r isn't loaded.
        {
            extern u8 TwilightUpgrade_IsClawshotActive(void);
            extern void* MmAssets_LoadHookshotChainDL(void);
            extern Gfx* MmDL_Or(Gfx * vanillaDL, Gfx * mmDL);
            Gfx* chainDL = gLinkAdultHookshotChainDL;
            if (TwilightUpgrade_IsClawshotActive()) {
                chainDL = MmDL_Or(chainDL, (Gfx*)MmAssets_LoadHookshotChainDL());
            }
            // upstream: alt-asset hookshot models can suppress the vanilla chain draw
            if (GameInteractor_Should(VB_DRAW_HOOKSHOT_CHAIN, true, player, play)) {
                gSPDisplayList(POLY_OPA_DISP++, chainDL);
            }
        }

        CLOSE_DISPS(play->state.gfxCtx);
    }
}
