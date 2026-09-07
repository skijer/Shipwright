/**
 * item_dekuleaf.c - Deku Leaf from Wind Waker
 *
 * Controls:
 *   C Button (ground): Swing leaf to create wind gust, pushes objects/enemies
 *   C Button (air):    Hold to glide, consumes magic over time
 *
 * Features:
 *   - Wind blow pushes enemies and certain objects
 *   - Gliding reduces fall speed and allows horizontal movement
 *   - Uses skeletal animation (39 frames) for blow attack
 */

#include "z64.h"
#include "item_dekuleaf.h"
#include "../custom_items.h"
#include "../helpers/movement_helper.h"
#include "../helpers/equip_helper.h"
#include "../helpers/fx_helper.h"
#include "transformation_masks/transformation_masks.h"
#include "transformation_masks/assets/mm_asset_loader.h"
#include "sound_translator/mm_sfx_ids.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

// Blow animation now loads from soh.o2r instead of a compiled-in s16 array. Skijer's NEI
#include "../anim/nei_anims.h"

static s8 sDekuLeafPrevInvinc = 0;
static u8 sDekuLeafBlowEffectFired = 0;
static u8 sDekuLeafColInitialized = 0;

// Wind AT collider — DMG_DEKU_NUT so enemies that touch the gust are stunned EXACTLY like a Deku Nut
// (their own damage tables resolve the stun — same flag and all), 0 damage. Skijer's NEI
static ColliderCylinderInit sDekuLeafColInit = { { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE,
                                                   OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
                                                 { ELEMTYPE_UNK0,
                                                   { DMG_DEKU_NUT, 0x00, 0x00 }, // dmgFlags, effect, damage
                                                   { 0x00000000, 0x00, 0x00 },
                                                   TOUCH_ON | TOUCH_SFX_NONE,
                                                   BUMP_NONE,
                                                   OCELEM_NONE },
                                                 { DEKULEAF_COL_RADIUS, DEKULEAF_COL_HEIGHT, 0, { 0, 0, 0 } } };

static void DekuLeaf_InitCollider(PlayState* play, Player* p) {
    if (sDekuLeafColInitialized)
        return;
    Collider_InitCylinder(play, &dlCollider);
    Collider_SetCylinder(play, &dlCollider, &p->actor, &sDekuLeafColInit);
    sDekuLeafColInitialized = 1;
}

// MM Deku SFX play helper — 100% MM verbatim.
// MM calls `Player_PlaySfx` (z_actor.c:2355) for FLOWER_OPEN/CLOSE/STRUGGLE
// and `Audio_PlaySfx_AtPosWithTimer` (audio/code_8019AF00.c:4347) for
// FLOWER_ROLL. Both ultimately invoke `AudioSfx_PlaySfx` with:
//     freqScale = sSfxAdjustedFreq  (= 1.0f in MM; "modified in OoT, but
//                                     remains 1.0f in MM" per the comment
//                                     at code_8019AF00.c:4343)
//     volume    = gSfxDefaultFreqAndVolScale (= 1.0f, sfx.c:78)
//     reverb    = gSfxDefaultReverb          (= 0,    sfx.c:82)
//
// So MM is DRY: no pitch shift, no extra reverb, no volume boost. We route
// through MmSfx_PlayAtPos which calls MmSfx_PlayEx with all three params as
// nullptr (asset_loader.cpp:3871) — the bank engine falls back to its own
// defaults that match MM's. Any remaining "feels off" perception against
// real MM is now a bridge / sample-bank issue, not a call-site issue.
static void DekuLeaf_PlayMmSfx(u16 sfxId, Vec3f* pos) {
    MmSfx_PlayAtPos(sfxId, pos);
}

static void DekuLeaf_Stop(Player* p, PlayState* play) {
    if (!dlActive)
        return;

    u8 wasGliding = dlGliding;

    dlActive = 0;
    dlMode = DEKULEAF_MODE_INACTIVE;
    dlGliding = 0;
    dlBlowing = 0;
    dlAnimTimer = 0;
    dlBlowTimer = 0;
    sDekuLeafBlowEffectFired = 0;
    dlCollider.base.atFlags &= ~(AT_ON | AT_HIT); // drop the wind collider — Skijer's NEI

    // Stop looping sounds — legacy OOT wind + the MM Deku propeller hum.
    Audio_StopSfxById(DEKULEAF_SOUND_WIND);
    Audio_StopSfxById(DEKULEAF_SOUND_BLOW);
    MmSfx_Stop(MM_NA_SE_IT_DEKUNUTS_FLOWER_ROLL);

    // MM verbatim: closing the Deku flower at end-of-flight fires a one-shot
    // "flower close" SFX (mm_player_form.cpp:8684, 2Ship z_player.c:6401).
    // Only fire when leaving a glide — blow-mode stop shouldn't play it.
    if (wasGliding) {
        DekuLeaf_PlayMmSfx(MM_NA_SE_IT_DEKUNUTS_FLOWER_CLOSE, &p->actor.projectedPos);
    }

    p->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
    ItemEquip_PlayUnequipSFX(play, p);
}

static void DekuLeaf_StartGlide(Player* p, PlayState* play) {
    dlActive = 1;
    dlMode = DEKULEAF_MODE_GLIDING;
    dlGliding = 1;
    dlBlowing = 0;
    dlAnimTimer = 0;

    // MM's FLOWER_OPEN fires during the Deku flower LAUNCH sequence (Link
    // pops out of the ground bud). For the Deku Leaf glide context Link
    // isn't launching from a flower — the equip SFX alone covers the entry.
    ItemEquip_PlayEquipSFX(play, p);
}

static void DekuLeaf_StartBlow(Player* p, PlayState* play) {
    // Via the helper so the Magic Cape's half-cost passive applies to the gate too.
    if (!ItemMagic_HasEnough(play, DEKULEAF_BLOW_MAGIC_COST))
        return;

    DekuLeaf_InitCollider(play, p);

    dlActive = 1;
    dlMode = DEKULEAF_MODE_BLOWING;
    dlGliding = 0;
    dlBlowing = 1;
    dlAnimTimer = 0;
    dlBlowTimer = 0;
    sDekuLeafBlowEffectFired = 0;

    // Start the skeletal animation on upperSkelAnime (loaded from soh.o2r), then override playSpeed
    // so the whole 39-frame swing runs 2x fast. Skijer's NEI
    {
        LinkAnimationHeader* anim = NeiAnim_Load(NEI_ANIM_DEKULEAF_BLOW);

        if (anim == NULL) {
            // Resource missing (o2r not regenerated) — don't start a blow we can't animate.
            dlActive = 0;
            dlMode = DEKULEAF_MODE_INACTIVE;
            dlBlowing = 0;
            return;
        }
        LinkAnimation_PlayOnce(play, &p->upperSkelAnime, anim);
        p->upperSkelAnime.playSpeed = DEKULEAF_BLOW_SPEED;
    }

    ItemEquip_PlayEquipSFX(play, p);
}

u8 RocBoots_IsWorn(void); // equip_roc_boots.c (later in this TU)

static void DekuLeaf_UpdateGlide(Player* p, PlayState* play) {
    // Roc's Boots halve every descent — the glide's fixed fall speed included.
    f32 fallVelocity = RocBoots_IsWorn() ? DEKULEAF_FALL_VELOCITY * 0.5f : DEKULEAF_FALL_VELOCITY;

    if (p->skelAnime.animation != &DEKULEAF_ANIM_GLIDE) {
        LinkAnimation_Change(play, &p->skelAnime, &DEKULEAF_ANIM_GLIDE, 1.0f, 0.0f,
                             Animation_GetLastFrame(&DEKULEAF_ANIM_GLIDE), ANIMMODE_LOOP, -4.0f);
    }

    if (p->actor.velocity.y < fallVelocity) {
        p->actor.velocity.y = fallVelocity;
    }

    // Paraglider forward momentum: keep at least a gentle forward drift (Link's yaw already follows
    // the stick in air, so you glide toward wherever you aim) instead of dropping straight down.
    // Eased so it doesn't snap. Skijer's NEI
    if (p->linearVelocity < DEKULEAF_GLIDE_FWD_SPEED) {
        Math_StepToF(&p->linearVelocity, DEKULEAF_GLIDE_FWD_SPEED, 0.5f);
    }

    if (play->gameplayFrames % DEKULEAF_GLIDE_MAGIC_INTERVAL == 0) {
        ItemMagic_Consume(play, DEKULEAF_GLIDE_MAGIC_COST);
    }

    // === MM Deku-flower propeller hum ===
    // Source SFX: mm_player_form.cpp:9100-9129 → MM_NA_SE_IT_DEKUNUTS_FLOWER_ROLL.
    //
    // In MM Deku flight the cadence between pulses tracks the angular speed of
    // the flower's petals (range 2..6 frames). For human-form Deku Leaf there's
    // no petalSpeed equivalent — the leaf either is open and gliding or it
    // isn't, with no acceleration profile. Using fall velocity as a proxy made
    // the cadence dance during the velocity ramp-up at glide start and felt
    // "accelerated" mid-glide. We just hold MM's SLOW end of the range (6
    // frames) for the whole glide — discrete, stable pulses that don't pile
    // up on each other and don't shift tempo unexpectedly.
    {
        static s32 sPropellerTimer = 1;
        sPropellerTimer--;
        if (sPropellerTimer <= 0) {
            DekuLeaf_PlayMmSfx(MM_NA_SE_IT_DEKUNUTS_FLOWER_ROLL, &p->actor.projectedPos);
            sPropellerTimer = 6;
        }
    }

    // === MM Deku flutter struggle ===
    // Source: mm_player_form.cpp:9082, 2Ship z_player.c:19194.
    // In MM the flutter anim fires this on frame 6 of its cycle. We don't have
    // a flutter anim driving us, so we fire it on a fixed ~24-frame cadence
    // (rough match to MM flutter loop length) starting after a small delay so
    // it doesn't double-up with FLOWER_OPEN.
    if ((play->gameplayFrames - dlAnimTimer) % 24 == 18) {
        DekuLeaf_PlayMmSfx(MM_NA_SE_PL_DEKUNUTS_STRUGGLE, &p->actor.projectedPos);
    }
}

static void DekuLeaf_SpawnWindParticles(Player* p, PlayState* play) {
    Vec3f windPos = p->actor.world.pos;
    s16 facingYaw = p->actor.shape.rot.y;

    windPos.y += 30.0f;

    FX_SpawnWindBlow(play, &windPos, facingYaw, DEKULEAF_BLOW_RANGE);
}

// Air-ball burst wrapping a blown enemy — pale wind smoke (gustjar look) as it flies out. Skijer's NEI
static void DekuLeaf_SpawnAirBall(PlayState* play, Vec3f* pos) {
    static Color_RGBA8 prim = { 195, 225, 235, 160 };
    static Color_RGBA8 env = { 150, 200, 220, 100 };
    s32 i;

    for (i = 0; i < 6; i++) {
        s16 ang = (s16)Rand_CenteredFloat(65535.0f);
        f32 r = 8.0f + Rand_ZeroFloat(14.0f);
        Vec3f ppos = { pos->x + Math_SinS(ang) * r, pos->y + 10.0f + Rand_CenteredFloat(16.0f),
                       pos->z + Math_CosS(ang) * r };
        Vec3f vel = { Math_SinS(ang) * 3.0f, Rand_CenteredFloat(1.5f), Math_CosS(ang) * 3.0f };
        Vec3f accel = { 0.0f, 0.3f, 0.0f };
        func_8002836C(play, &ppos, &vel, &accel, &prim, &env, 200, 25, 12);
    }
}

// Ground gust: (1) DMG_DEKU_NUT AT collider out in front of Link → native deku-nut stun on contact;
// (2) strong HORIZONTAL push on enemies in the forward cone — linear speed only, NO height; each
// blown enemy gets an air-ball burst so it flies out wrapped in wind. Skijer's NEI
static void DekuLeaf_BlowEffect(Player* p, PlayState* play) {
    s16 facingYaw = p->actor.shape.rot.y;
    Vec3f windPos = p->actor.world.pos;
    Actor* actor;

    windPos.y += 25.0f;

    // Deku-nut AT collider positioned out in the gust.
    dlCollider.dim.pos.x = (s16)(windPos.x + Math_SinS(facingYaw) * DEKULEAF_COL_FORWARD);
    dlCollider.dim.pos.y = (s16)windPos.y;
    dlCollider.dim.pos.z = (s16)(windPos.z + Math_CosS(facingYaw) * DEKULEAF_COL_FORWARD);
    dlCollider.info.toucher.dmgFlags = DMG_DEKU_NUT;
    dlCollider.info.toucher.damage = 0;
    dlCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &dlCollider.base);
    if (dlCollider.base.atFlags & AT_HIT) {
        dlCollider.base.atFlags &= ~AT_HIT;
    }

    actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].head;
    while (actor != NULL) {
        if (actor->update != NULL) {
            f32 dx = actor->world.pos.x - windPos.x;
            f32 dz = actor->world.pos.z - windPos.z;
            f32 dist = sqrtf(SQ(dx) + SQ(dz));
            f32 dy = fabsf(actor->world.pos.y - windPos.y);

            if (dist < DEKULEAF_BLOW_RANGE && dy < 70.0f && dist > 0.1f) {
                s16 angleToEnemy = Math_Atan2S(dx, dz);
                s16 angleDiff = angleToEnemy - facingYaw;

                if (angleDiff > -0x3800 && angleDiff < 0x3800) { // ~78° forward cone
                    f32 forceMult = 1.0f - (dist / DEKULEAF_BLOW_RANGE);
                    f32 nx = dx / dist;
                    f32 nz = dz / dist;
                    f32 force;

                    if (forceMult < 0.35f) {
                        forceMult = 0.35f;
                    }
                    force = DEKULEAF_BLOW_FORCE * forceMult;

                    // LINEAR speed away from Link — NO vertical component (no lift). Immediate nudge
                    // too, so it visibly slides even if the deku-nut stun freezes it next frame.
                    actor->world.pos.x += nx * force * 0.5f;
                    actor->world.pos.z += nz * force * 0.5f;
                    actor->world.rot.y = angleToEnemy;
                    actor->speedXZ = force;
                    actor->velocity.x = nx * force;
                    actor->velocity.z = nz * force;

                    DekuLeaf_SpawnAirBall(play, &actor->world.pos);
                }
            }
        }
        actor = actor->next;
    }
}

// ============================================================================
// UPPER ACTION - Drives the blow animation via upperSkelAnime
// ============================================================================

s32 Player_UpperAction_DekuLeaf(Player* player, PlayState* play) {
    if (!dlActive)
        return 0;
    if (!dlBlowing)
        return 0;

    // Update the skeletal animation
    if (LinkAnimation_Update(play, &player->upperSkelAnime)) {
        // Animation finished
        DekuLeaf_Stop(player, play);
        return 0;
    }

    // Track frame
    dlAnimTimer++;

    // Stop movement during blow animation
    player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;

    // Fire the gust once, keyed off the ANIMATION frame (not a tick counter) so it stays in sync with
    // the swing no matter what playback speed is used. Skijer's NEI
    if (!sDekuLeafBlowEffectFired && player->upperSkelAnime.curFrame >= DEKULEAF_BLOW_EFFECT_FRAME) {
        sDekuLeafBlowEffectFired = 1;
        ItemMagic_Consume(play, DEKULEAF_BLOW_MAGIC_COST);
        Player_PlaySfx(player, DEKULEAF_SOUND_BLOW);
        dlBlowTimer = DEKULEAF_BLOW_ACTIVE_FRAMES;
    }

    // Active gust window: wind particles + deku-nut collider + horizontal push + air-ball VFX.
    if (dlBlowTimer > 0) {
        dlBlowTimer--;
        if (play->gameplayFrames % DEKULEAF_WIND_SPAWN_RATE == 0) {
            DekuLeaf_SpawnWindParticles(player, play);
        }
        DekuLeaf_BlowEffect(player, play);
    }

    // Return 1 to indicate upper body is busy (use upperSkelAnime)
    return 1;
}

// ============================================================================
// MAIN HANDLER
// ============================================================================

void Handle_DekuLeaf(Player* p, PlayState* play) {
    // Deku form has its own Deku Leaf handling (flower burrow + flight)
    if (TransformMasks_IsTransformed() && MmPlayer_GetForm() == MM_PLAYER_FORM_DEKU)
        return;

    ItemInputState in;
    ItemInput_Update(&in, ITEM_DEKU_LEAF, p, play);

    if (!in.wasEquipped) {
        if (dlActive)
            DekuLeaf_Stop(p, play);
        return;
    }

    if (ItemInput_IsBlocked(p, play)) {
        if (dlActive)
            DekuLeaf_Stop(p, play);
        return;
    }

    if (ItemInput_CheckDamage(p, &sDekuLeafPrevInvinc)) {
        DekuLeaf_Stop(p, play);
        return;
    }

    // If blowing, the upper action handles everything
    if (dlMode == DEKULEAF_MODE_BLOWING) {
        return;
    }

    if (dlMode == DEKULEAF_MODE_GLIDING) {
        if (Movement_IsOnGround(p)) {
            DekuLeaf_Stop(p, play);
            return;
        }

        // Via the helper so the Magic Cape makes gliding FREE and usable at 0 magic: glide cost 1
        // halves to 0, so ItemMagic_HasEnough stays true even with an empty meter (Skijer 2026-07-15).
        if (!in.isHeld || !ItemMagic_HasEnough(play, DEKULEAF_GLIDE_MAGIC_COST) || in.otherButtonPressed) {
            DekuLeaf_Stop(p, play);
            return;
        }

        DekuLeaf_UpdateGlide(p, play);
        return;
    }

    if (!dlActive && in.isPressed) {
        if (!Movement_IsOnGround(p)) {
            // Cape makes gliding free → startable even at 0 magic (ItemMagic_HasEnough, cost 1 -> 0).
            if (ItemMagic_HasEnough(play, DEKULEAF_GLIDE_MAGIC_COST)) {
                DekuLeaf_StartGlide(p, play);
            }
        } else {
            if (p->stateFlags1 & PLAYER_STATE1_IN_WATER)
                return;
            if (p->meleeWeaponState != 0)
                return;
            DekuLeaf_StartBlow(p, play);
        }
        return;
    }

    if (dlMode == DEKULEAF_MODE_GLIDING && !in.isHeld) {
        DekuLeaf_Stop(p, play);
    }
}
