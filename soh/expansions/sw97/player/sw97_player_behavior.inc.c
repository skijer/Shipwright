/**
 * sw97_player_behavior.inc.c - Player behavior hooks for SW97 Medallion Spells
 *
 * Original actors: z64proto/sw97 team (Spaceworld '97 Experience)
 * Adapted for Ship of Harkinian (Shipwright)
 *
 * Provides CVar-gated hooks for:
 * - Magic spell actor spawning (6 spells mapped to spell indices 0-5)
 * - Helper functions for medallion/arrow item identification
 * - Medallion-to-arrow item conversion
 */

// Runtime actor IDs (set by sw97_init.cpp via ActorDB)
extern s16 gSw97ActorId_MagicFire;
extern s16 gSw97ActorId_MagicIce;
extern s16 gSw97ActorId_MagicLight;
extern s16 gSw97ActorId_MagicDark;
extern s16 gSw97ActorId_MagicSoul;
extern s16 gSw97ActorId_MagicWind;
extern s16 gSw97ActorId_ArrowFire;
extern s16 gSw97ActorId_ArrowIce;
extern s16 gSw97ActorId_ArrowLight;
extern s16 gSw97ActorId_ArrowDark;
extern s16 gSw97ActorId_ArrowSoul;
extern s16 gSw97ActorId_ArrowWind;

// SW97 magic spell costs: indices 0-5 match Player_ActionToMagicSpell output
// 0=Wind(12), 1=Soul(24), 2=Dark(12), 3=Ice(24), 4=Light(24), 5=Fire(12)
static u8 sSw97MagicSpellCosts[] = { 12, 24, 12, 24, 24, 12 };

// SW97 magic arrow costs: all 4 except light which is 8
static u8 sSw97MagicArrowCosts[] = { 4, 4, 8, 4, 4, 4 };

/**
 * Spawn the correct SW97 magic spell actor based on spell index.
 * Called from Player_SpawnMagicSpell in z_player.c.
 *
 * Spell index mapping (from Player_ActionToMagicSpell):
 *   0 = IA_MAGIC_SPELL_15 = Forest Medallion → MagicWind
 *   1 = IA_MAGIC_SPELL_16 = Spirit Medallion → MagicSoul
 *   2 = IA_MAGIC_SPELL_17 = Shadow Medallion → MagicDark
 *   3 = IA_FARORES_WIND   = Water Medallion  → MagicIce
 *   4 = IA_NAYRUS_LOVE    = Light Medallion  → MagicLight
 *   5 = IA_DINS_FIRE       = Fire Medallion   → MagicFire
 *
 * Returns the spawned actor, or NULL if SW97 spells are disabled.
 */
static Actor* Sw97_TrySpawnMagicSpell(PlayState* play, Player* player, s32 spell) {
    if (!SW97_MEDALLIONS_ENABLED()) {
        return NULL;
    }

    if (spell < 0 || spell >= 6) {
        return NULL;
    }

    // Shadow medallion heart→magic exchange is handled out-of-band in
    // soh/Enhancements/ShadowMedallionExchange.cpp via an OnPlayerUpdate hook,
    // so the exchange works even when the player has zero magic (otherwise the
    // cast flow short-circuits before reaching this function).

    s16* spellActorIds[] = {
        &gSw97ActorId_MagicWind,  // 0 = Forest
        &gSw97ActorId_MagicSoul,  // 1 = Spirit
        &gSw97ActorId_MagicDark,  // 2 = Shadow
        &gSw97ActorId_MagicIce,   // 3 = Water
        &gSw97ActorId_MagicLight, // 4 = Light
        &gSw97ActorId_MagicFire,  // 5 = Fire
    };

    s16 actorId = *spellActorIds[spell];
    if (actorId < 0) {
        return NULL;
    }

    Actor* spawned = Actor_Spawn(&play->actorCtx, play, actorId, player->actor.world.pos.x, player->actor.world.pos.y,
                                 player->actor.world.pos.z, 0, 0, 0, 0);

    // Tell teammates to spawn the same spell-effect actor on their side.
    // Spells follow the caster (attached_to_owner=1) — their visual stays
    // around the caster's dummy as long as the spell is active. Map the
    // spell index back to the corresponding HARPOON_VFX_KIND_SW97_MAGIC_*.
    if (spawned != NULL) {
        s32 vfxKindByIndex[] = {
            HARPOON_VFX_KIND_SW97_MAGIC_WIND,  // 0
            HARPOON_VFX_KIND_SW97_MAGIC_SOUL,  // 1
            HARPOON_VFX_KIND_SW97_MAGIC_DARK,  // 2
            HARPOON_VFX_KIND_SW97_MAGIC_ICE,   // 3
            HARPOON_VFX_KIND_SW97_MAGIC_LIGHT, // 4
            HARPOON_VFX_KIND_SW97_MAGIC_FIRE,  // 5
        };
        Harpoon_NotifyVfxSpawn(spawned, vfxKindByIndex[spell], /*attachedToOwner=*/1);
    }
    return spawned;
}

/**
 * Check if an item ID is a quest medallion (spell mode).
 */
static s32 Sw97_IsMedallionItem(s32 item) {
    return (item >= ITEM_MEDALLION_FOREST && item <= ITEM_MEDALLION_LIGHT);
}

// (Sw97_IsArrowItem and Sw97_MedallionToArrowItem removed — Skijer's NEI. Both were already dead
// code with zero call sites, and both were built on the premise that the primed element is an item
// id. The element is a flag now: use Sw97_EffectiveElement()/Sw97_ElementIcon() instead.)

/**
 * Returns true while the Shadow Medallion spell (MagicDark) is active.
 * MagicDark drives gSaveContext.nayrusLoveTimer for its lifetime; in SW97 mode
 * the Shadow medallion replaces Nayru's Love (Light medallion is the new NL slot),
 * so a nonzero timer + SW97 enabled uniquely identifies "Shadow stealth is on".
 *
 * Consumed by z_actor.c so enemies/NPCs can't detect Link (same hook point as
 * MmMaskWear_IsStoneMaskActive).
 */
s32 Sw97_ShadowStealthActive(void) {
    if (!SW97_MEDALLIONS_ENABLED())
        return 0;
    return gSaveContext.nayrusLoveTimer > 0;
}

/**
 * Shadow Medallion heart→magic exchange.
 *
 * Hold the C-button that has ITEM_MEDALLION_SHADOW for SHADOW_EXCHANGE_HOLD_FRAMES
 * frames → spend 3 hearts, gain 24 magic. Disarmed until release.
 *
 * Must work even at zero magic (the vanilla cast pipeline short-circuits before
 * reaching Sw97_TrySpawnMagicSpell when magic is insufficient — playing only the
 * "no magic" error sound — so the exchange has to live in a per-frame tick).
 *
 * Called from z_player.c Player_UpdateCommon each frame.
 */
#define SHADOW_EXCHANGE_HOLD_FRAMES 20
#define SHADOW_EXCHANGE_HEART_COST (3 * 0x10) // 3 hearts × 16 HP
#define SHADOW_EXCHANGE_MAGIC_GAIN 24

void Sw97_TickShadowExchange(PlayState* play, Player* player) {
    if (!SW97_MEDALLIONS_ENABLED())
        return;
    if (play == NULL || player == NULL)
        return;

    // Find which C-slot has the Shadow medallion. buttonItems[0]=B, [1..3]=C-LDR.
    u16 medallionMask = 0;
    if (gSaveContext.equips.buttonItems[1] == ITEM_MEDALLION_SHADOW)
        medallionMask |= BTN_CLEFT;
    if (gSaveContext.equips.buttonItems[2] == ITEM_MEDALLION_SHADOW)
        medallionMask |= BTN_CDOWN;
    if (gSaveContext.equips.buttonItems[3] == ITEM_MEDALLION_SHADOW)
        medallionMask |= BTN_CRIGHT;

    static s16 sShadowHoldFrames = 0;
    static u8 sShadowExchanged = 0;

    if (medallionMask == 0) {
        sShadowHoldFrames = 0;
        sShadowExchanged = 0;
        return;
    }

    u16 cur = play->state.input[0].cur.button;
    if (!(cur & medallionMask)) {
        sShadowHoldFrames = 0;
        sShadowExchanged = 0;
        return;
    }

    sShadowHoldFrames++;
    if (sShadowExchanged)
        return;
    if (sShadowHoldFrames < SHADOW_EXCHANGE_HOLD_FRAMES)
        return;
    if (gSaveContext.health <= SHADOW_EXCHANGE_HEART_COST)
        return;

    gSaveContext.health -= SHADOW_EXCHANGE_HEART_COST;
    gSaveContext.magic += SHADOW_EXCHANGE_MAGIC_GAIN;
    if (gSaveContext.magic > gSaveContext.magicCapacity) {
        gSaveContext.magic = gSaveContext.magicCapacity;
    }
    Audio_PlayActorSound2(&player->actor, NA_SE_SY_GET_RUPY);
    sShadowExchanged = 1;
}

/**
 * Shadow-element blindness — per-actor stealth.
 *
 * When Shadow ARROW (ARROW_SW97_0C) hits an enemy, OR when the Gust Jar's
 * Shadow-element BLOW pushes an enemy, the target is "blinded" for ~10
 * seconds: z_actor.c's distance-to-player calculation is spoofed to 32000
 * (same mechanism as Stone Mask + Shadow Medallion stealth), so the enemy
 * stops tracking Link until the timer expires.
 *
 * Storage is a small static table indexed by Actor*. Capacity 32 is plenty
 * for the worst-case crowd you'd reasonably blind in one fight. New tags
 * upsert (longer-of duration); expired slots are reused.
 *
 * `Sw97_TickBlindness` MUST be called once per frame from z_player.c so
 * `framesRemaining` actually counts down.
 */
#define SW97_BLIND_TABLE_SIZE 32
#define SW97_BLIND_DURATION 300 // 10 sec at SW97's 30fps timer convention

typedef struct {
    Actor* actor;
    s16 framesRemaining;
} Sw97BlindEntry;

static Sw97BlindEntry sSw97Blinded[SW97_BLIND_TABLE_SIZE];

void Sw97_TagBlinded(Actor* actor, s16 frames) {
    if (actor == NULL || actor->update == NULL)
        return;
    s32 empty = -1;
    for (s32 i = 0; i < SW97_BLIND_TABLE_SIZE; i++) {
        if (sSw97Blinded[i].actor == actor) {
            if (frames > sSw97Blinded[i].framesRemaining) {
                sSw97Blinded[i].framesRemaining = frames;
            }
            return;
        }
        if (sSw97Blinded[i].actor == NULL && empty < 0)
            empty = i;
    }
    if (empty >= 0) {
        sSw97Blinded[empty].actor = actor;
        sSw97Blinded[empty].framesRemaining = frames;
    }
}

s32 Sw97_IsBlinded(Actor* actor) {
    if (actor == NULL)
        return 0;
    for (s32 i = 0; i < SW97_BLIND_TABLE_SIZE; i++) {
        if (sSw97Blinded[i].actor == actor && sSw97Blinded[i].framesRemaining > 0) {
            return 1;
        }
    }
    return 0;
}

void Sw97_TickBlindness(void) {
    for (s32 i = 0; i < SW97_BLIND_TABLE_SIZE; i++) {
        if (sSw97Blinded[i].actor == NULL)
            continue;
        // Drop dead actors immediately so we don't keep their pointer.
        if (sSw97Blinded[i].actor->update == NULL) {
            sSw97Blinded[i].actor = NULL;
            sSw97Blinded[i].framesRemaining = 0;
            continue;
        }
        if (--sSw97Blinded[i].framesRemaining <= 0) {
            sSw97Blinded[i].actor = NULL;
            sSw97Blinded[i].framesRemaining = 0;
        }
    }
}

/**
 * Cucco Mode — Soul arrow + Cucco → 30-second transformation.
 *
 * Triggered by `ArrowSoul_TryTransform` when the soul arrow hits an `EN_NIW`.
 * Visual: Cucco model swap on the Player draw function. Movement: a Flappy
 * Bird-style flap (A press while airborne = upward burst, reduced gravity for
 * slow fall). Bow / slingshot shots become elemental eggs (free, no magic
 * cost). R = spawn 3 attack-cuccos orbiting Link. B in air = peck dive.
 *
 * State is global so other systems (player draw hook, input intercept, egg
 * spawner) can query without threading through a parameter.
 */
#define CUCCO_MODE_FRAMES 1800   // 30 sec
#define CUCCO_FLAP_VELOCITY 9.0f // upward burst per A press while airborne
#define CUCCO_GRAVITY -1.2f      // Cucco terminal: gentle fall, not Link's -7
#define CUCCO_MAX_VY_DOWN -3.0f  // Cucco terminal velocity cap (float, not plummet)
#define CUCCO_SPEED_MULT 1.15f   // Slightly faster than Link
#define CUCCO_SPEED_MAX 11.0f    // Cap so flap doesn't compound forever

// ─── Kirby-style flight ────────────────────────────────────────────────
// Slow fall is the DEFAULT (CUCCO_MAX_VY_DOWN above). On top of it:
//   A press, airborne → one flap out of a finite budget of 6
//   A held, falling   → directed glide (slower descent + stick-dir push)
// The budget refills only on touching the ground, so height is a resource
// you spend rather than an infinite Flappy-Bird ladder.
#define CUCCO_MAX_FLAPS 3
#define CUCCO_GLIDE_VY -1.0f       // Glide descent — a third of the free fall
#define CUCCO_GLIDE_PUSH 0.7f      // Per-frame horizontal accel while gliding
#define CUCCO_GLIDE_SPEED_MAX 9.0f // Glide horizontal cap (below run speed)

// ─── Leg Spring (R + A on the ground) ──────────────────────────────────
// Banjo-Tooie's Kazooie-solo super jump: compress, then launch far higher
// than a normal flap. Spends nothing — the cost is the wind-up.
#define CUCCO_LEGSPRING_CROUCH 10 // Frames of compression before launch
#define CUCCO_LEGSPRING_VY 26.0f  // Launch speed (vs 9.0 for a flap)

// ─── Aerial spin (B in the air) ────────────────────────────────────────
// Wing Whack's midair form: spins with wings out, damages everything
// around, and is fully intangible for the duration. Replaces the boomerang.
#define CUCCO_SPIN_FRAMES 24
#define CUCCO_SPIN_LIFT 4.0f // Small pop on activation
#define CUCCO_SPIN_VY -1.0f  // Hovers instead of falling while spinning
#define CUCCO_SPIN_RADIUS 45.0f
#define CUCCO_SPIN_HEIGHT 45.0f
#define CUCCO_SPIN_YSHIFT -10
#define CUCCO_SPIN_DAMAGE 8
// CollisionCheck_SetAT resets the collider every registration, so registering
// each frame would land one hit per enemy PER FRAME and melt bosses. Register
// on a cadence instead.
#define CUCCO_SPIN_AT_CADENCE 6
#define CUCCO_SPIN_YAW_STEP 0x3000 // Model spin speed (binang per frame)

// ─── Wing Whack (B on the ground) ──────────────────────────────────────
// Standing: alternating wing slashes forward. Moving: the same wide-wing
// pose plus a body spin, which is how Tooie distinguishes the two.
#define CUCCO_WHACK_FRAMES 18
#define CUCCO_WHACK_RADIUS 60.0f
#define CUCCO_WHACK_DAMAGE 8
#define CUCCO_WHACK_AT_CADENCE 6

// ─── Breegull pound (R while airborne) ─────────────────────────────────
// Two phases: a brief hover telegraph, then a hard slam that ignores the
// slow-fall clamp. Landing does a radial hit.
#define CUCCO_POUND_HOVER 8       // Frames of hang time before the drop
#define CUCCO_POUND_VY -32.0f     // Slam speed (bypasses CUCCO_MAX_VY_DOWN)
#define CUCCO_POUND_RADIUS 130.0f // Landing shockwave radius
#define CUCCO_POUND_DAMAGE 16     // HP drained per enemy inside the radius

// ─── Flock (R while grounded) ──────────────────────────────────────────
// The player-side mirror of the Cucco storm: summoned birds orbit Link and
// dive at whatever enemy is nearest. NOT ACTOR_EN_ATTACK_NIW — that actor
// exists to punish the player (it casts actor.parent to EnNiw* and reads
// cucco->timer9 at z_en_attack_niw.c:366, then damages Link), so handing it
// a Player parent is both wrong and unsafe. These are lightweight state-only
// birds drawn from the same skeleton, matching this file's no-actor-puppet
// pattern.
#define CUCCO_FLOCK_MAX 3
#define CUCCO_FLOCK_LIFE 420       // 7 sec per summon
#define CUCCO_FLOCK_COOLDOWN 150   // 2.5 sec between summons
#define CUCCO_FLOCK_ORBIT_R 70.0f  // Idle orbit radius around Link
#define CUCCO_FLOCK_ORBIT_Y 45.0f  // Idle orbit height above Link's feet
#define CUCCO_FLOCK_SPEED 7.0f     // Dive speed toward a target
#define CUCCO_FLOCK_RANGE 450.0f   // How far it will look for a target
#define CUCCO_FLOCK_HIT_DIST 30.0f // Contact distance for a peck
#define CUCCO_FLOCK_DAMAGE 4       // HP per peck
#define CUCCO_FLOCK_PECK_CD 24     // Frames between pecks on the same bird

// ─── Shield (R held on the ground) ─────────────────────────────────────
// The cucco's own shield: reflects frontal projectiles Deku-style, but —
// unlike a vanilla block — the damage still lands. Three blocked hits call
// in the flock. If Link actually has a shield equipped, none of this runs
// and R falls through to his normal shield.
#define CUCCO_SHIELD_Y 26.0f
#define CUCCO_SHIELD_BLOCKS_TO_FLOCK 3
#define CUCCO_SHIELD_MIN_CHIP 4 // Fallback damage if the attacker has none
// Projectiles read player->currentShield during THEIR update, a frame after
// the bounce is flagged. Without a release grace, letting go of R on the
// bounce frame makes the shot shatter instead of reflecting.
#define CUCCO_SHIELD_GRACE 5

// ─── Egg aim mode (R + B) ──────────────────────────────────────────────
#define CUCCO_EGG_TYPE_COUNT 5
#define CUCCO_EGG_REGULAR 0
#define CUCCO_EGG_FIRE 1
#define CUCCO_EGG_LIGHT 2
#define CUCCO_EGG_ICE 3
#define CUCCO_EGG_BOMB 4
#define CUCCO_EGG_FIRE_CD 8 // Frames between shots
// A bomb egg detonating in the cucco's face is self-inflicted; require some
// travel before it arms.
#define CUCCO_EGG_BOMB_ARM_DIST 60.0f

// Panic window after taking damage — real cuccos throw their wings wide and
// squawk when struck, so the form does too.
#define CUCCO_HURT_FRAMES 40
// How long a single flap keeps the wide-wing pose before easing back.
#define CUCCO_FLAP_POSE_FRAMES 12

// Where the transformation came from. The spell (soul arrow) form is a
// 30-second movement-only buff that pops the moment you reach for an item;
// the CVar form is a persistent playable mode with items intact.
#define CUCCO_SRC_SPELL 0
#define CUCCO_SRC_CVAR 1
#define CUCCO_MODE_CVAR "gEnhancements.SkijerNEI.CuccoMode"
s32 gSw97CuccoModeSource = CUCCO_SRC_SPELL;

s32 gSw97CuccoModeActive = 0;
// Pending → waiting for Link to leave PLAYER_STATE1_IN_ITEM_CS (the
// first-person aim/throw cutscene). Same pattern as magic_soul.inc.c:117
// where the diamond update returns until the player is free. Without this
// the camera stays glued in first-person mode and breaks on entry.
s32 gSw97CuccoModePending = 0;
s32 gSw97CuccoModeTimer = 0;
// Once-shot exit fx flag — guarantees the un-transform flash/sound only
// plays once even though Sw97_TickCuccoMode keeps running on inactive.
static s32 gSw97CuccoExitFx = 0;
// 180° flip animation on egg throw — counts down each frame, used by
// Sw97_DrawCuccoModel to rotate the model. ~12 frames = ~0.4s flip.
s32 gSw97CuccoFlipTimer = 0;
#define CUCCO_FLIP_FRAMES 12

// Cucco draw — direct copy of HGrace's draw-override pattern (no actor
// puppet). Skeleton inited once per cucco-mode session via Sw97_InitCuccoSkel,
// rendered via Sw97_DrawCucco which Link's actor.draw points at.
#include "objects/object_niw/object_niw.h"
static SkelAnime sCuccoSkel;
static Vec3s sCuccoJointTable[16];
static Vec3s sCuccoMorphTable[16];
static u8 sCuccoSkelInited = 0;

static void Sw97_InitCuccoSkel(PlayState* play) {
    if (sCuccoSkelInited)
        return;
    SkelAnime_InitFlex(play, &sCuccoSkel, (FlexSkeletonHeader*)&gCuccoSkel, (AnimationHeader*)&gCuccoAnim,
                       sCuccoJointTable, sCuccoMorphTable, 16);
    sCuccoSkelInited = 1;
}

// ═══════════════════════════════════════════════════════════════════════
// Cucco AI — the procedural limb layer, ported from the real Cucco actor.
//
// object_niw ships exactly ONE animation (gCuccoAnim — a stiff idle loop;
// see object_niw.xml, a single <Animation> entry). Every bit of visible
// Cucco motion is procedural instead: func_80AB5BF8 (z_en_niw.c:262, named
// EnNiw_AnimateWingHead in the MM decomp) writes target angles, Math_ApproachF
// eases toward them, and EnNiw_OverrideLimbDraw (z_en_niw.c:1120) adds the
// result on top of the animation at four limbs.
//
// Cucco mode previously drew the skeleton with a NULL override and only
// varied playSpeed, so none of this ran — the bird was a rigid prop that
// slid around. This block restores it.
//
// A flap is not a keyframe: sCuccoWingPhase flips every sCuccoWingTimer
// frames, so the wing-yaw target alternates between two values (e.g. 25000
// and 8000) and the easing chases it — a square wave, smoothed.
//
// Limb indices are identical in OoT and MM (same skeleton). The names are
// MM's, which labels them; OoT's XML leaves them numeric.
// ═══════════════════════════════════════════════════════════════════════

#define NIW_LIMB_LEFT_WING_ROOT 7
#define NIW_LIMB_RIGHT_WING_ROOT 11
#define NIW_LIMB_UPPER_BODY 13
#define NIW_LIMB_HEAD 15

// MM's ObjectNiwAnim (z_en_niw.h:104) — pose selectors, not animations.
typedef enum {
    /* 0 */ NIW_ANIM_STILL,                   // idle: neck bob, wings down
    /* 1 */ NIW_ANIM_HEAD_PECKING,            // walk: gentle wing roll
    /* 2 */ NIW_ANIM_PECKING_AND_WAVING,      // panic / flap: wings thrown wide
    /* 3 */ NIW_ANIM_PECKING_AND_FORFLAPPING, // glide / fall: low steady flap
    /* 4 */ NIW_ANIM_FREEZE,                  // pound: wings locked (Cucco Storm pose)
    /* 5 */ NIW_ANIM_PECKING_SLOW_FORFLAPPING // run: mid-speed flap
} Sw97CuccoAnim;

// Eased angles, added on top of gCuccoAnim by the override below.
static f32 sCuccoUpperBodyRotY, sCuccoHeadRotY;
static f32 sCuccoLWingRotX, sCuccoLWingRotY, sCuccoLWingRotZ;
static f32 sCuccoRWingRotX, sCuccoRWingRotY, sCuccoRWingRotZ;
// Targets the above chase (EnNiw targetLimbRots[]).
static f32 sCuccoTgtUpperBodyRotY, sCuccoTgtHeadRotY;
static f32 sCuccoTgtLWingRotX, sCuccoTgtLWingRotY, sCuccoTgtLWingRotZ;
static f32 sCuccoTgtRWingRotX, sCuccoTgtRWingRotY, sCuccoTgtRWingRotZ;
// Beat timers / phase toggles (EnNiw unkTimer24C, unkTimer24E, unk292, unkToggle296).
static s16 sCuccoBodyTimer, sCuccoWingTimer, sCuccoBodyPhase, sCuccoWingPhase;
// Set while a Wing Whack is swinging: de-phases the two wings (see the
// PECKING_AND_WAVING case below).
static u8 sCuccoWhackDesync;

static void Sw97_CuccoResetPose(void) {
    sCuccoUpperBodyRotY = sCuccoHeadRotY = 0.0f;
    sCuccoLWingRotX = sCuccoLWingRotY = sCuccoLWingRotZ = 0.0f;
    sCuccoRWingRotX = sCuccoRWingRotY = sCuccoRWingRotZ = 0.0f;
    sCuccoTgtUpperBodyRotY = sCuccoTgtHeadRotY = 0.0f;
    sCuccoTgtLWingRotX = sCuccoTgtLWingRotY = sCuccoTgtLWingRotZ = 0.0f;
    sCuccoTgtRWingRotX = sCuccoTgtRWingRotY = sCuccoTgtRWingRotZ = 0.0f;
    sCuccoBodyTimer = sCuccoWingTimer = sCuccoBodyPhase = sCuccoWingPhase = 0;
    sCuccoWhackDesync = 0;
}

// 1:1 port of func_80AB5BF8 / EnNiw_AnimateWingHead. The `factor` the original
// applies (2.0 for the params==0xD attack cucco, 1.0 otherwise) is folded to
// 1.0 — Link's form is a regular cucco.
static void Sw97_CuccoAnimateWingHead(s16 animIndex) {
    // EnNiw_Update DECRs these before the action func runs; we do it here.
    if (sCuccoBodyTimer > 0)
        sCuccoBodyTimer--;
    if (sCuccoWingTimer > 0)
        sCuccoWingTimer--;

    if (sCuccoBodyTimer == 0) {
        sCuccoTgtUpperBodyRotY = (animIndex == NIW_ANIM_STILL) ? 0.0f : -10000.0f;
        sCuccoBodyPhase++;
        sCuccoBodyTimer = 3;
        if ((sCuccoBodyPhase % 2) == 0) {
            sCuccoTgtUpperBodyRotY = 0.0f;
            if (animIndex == NIW_ANIM_STILL) {
                // Randomised idle pause — why a standing cucco's head twitches
                // at irregular intervals instead of on a metronome.
                sCuccoBodyTimer = (s16)Rand_ZeroFloat(30.0f);
            }
        }
    }

    if (sCuccoWingTimer == 0) {
        sCuccoWingPhase++;
        sCuccoWingPhase &= 1;

        switch (animIndex) {
            case NIW_ANIM_STILL:
                sCuccoTgtLWingRotZ = sCuccoTgtRWingRotZ = 0.0f;
                // Deviation from vanilla: the real actor leaves wing X/Y alone
                // here because it only ever reaches STILL from poses that
                // already zeroed them. Link's form can drop straight from
                // flight to standing, so without this the wings would stay
                // locked out at 25000 on landing.
                sCuccoTgtLWingRotY = sCuccoTgtRWingRotY = 0.0f;
                sCuccoTgtLWingRotX = sCuccoTgtRWingRotX = 0.0f;
                break;

            case NIW_ANIM_HEAD_PECKING:
                sCuccoWingTimer = 3;
                sCuccoTgtLWingRotZ = sCuccoTgtRWingRotZ = 7000.0f;
                if (sCuccoWingPhase == 0) {
                    sCuccoTgtLWingRotZ = sCuccoTgtRWingRotZ = 0.0f;
                }
                break;

            case NIW_ANIM_PECKING_AND_WAVING:
                sCuccoWingTimer = 2;
                sCuccoTgtLWingRotZ = sCuccoTgtRWingRotZ = -10000.0f;
                sCuccoTgtLWingRotY = sCuccoTgtRWingRotY = 25000.0f;
                sCuccoTgtLWingRotX = sCuccoTgtRWingRotX = 6000.0f;
                if (sCuccoWingPhase == 0) {
                    sCuccoTgtLWingRotY = sCuccoTgtRWingRotY = 8000.0f;
                }
                // Wing Whack: drive the two wings HALF A CYCLE APART instead of
                // in phase. Same pose data, but it reads as alternating forward
                // slashes rather than a symmetric panic flap — which is exactly
                // how Tooie distinguishes Wing Whack from Kazooie flailing.
                if (sCuccoWhackDesync) {
                    sCuccoTgtRWingRotY = (sCuccoWingPhase == 0) ? 25000.0f : 8000.0f;
                    sCuccoTgtRWingRotZ = 4000.0f;
                }
                break;

            case NIW_ANIM_PECKING_AND_FORFLAPPING:
                sCuccoWingTimer = 2;
                sCuccoTgtLWingRotY = sCuccoTgtRWingRotY = 10000.0f;
                if (sCuccoWingPhase == 0) {
                    sCuccoTgtLWingRotY = sCuccoTgtRWingRotY = 3000.0f;
                }
                break;

            case NIW_ANIM_FREEZE:
                sCuccoBodyTimer = 5;
                break;

            case NIW_ANIM_PECKING_SLOW_FORFLAPPING:
                sCuccoWingTimer = 5;
                sCuccoTgtLWingRotY = sCuccoTgtRWingRotY = 14000.0f;
                if (sCuccoWingPhase == 0) {
                    sCuccoTgtLWingRotY = sCuccoTgtRWingRotY = 10000.0f;
                }
                break;

            default:
                break;
        }
    }

    // Head/body ease slowly (0.5 / 4000), wings snap harder (0.8 / 7000) —
    // vanilla's rates, and the reason the flap reads as a flap.
    Math_ApproachF(&sCuccoHeadRotY, sCuccoTgtHeadRotY, 0.5f, 4000.0f);
    Math_ApproachF(&sCuccoUpperBodyRotY, sCuccoTgtUpperBodyRotY, 0.5f, 4000.0f);
    Math_ApproachF(&sCuccoLWingRotZ, sCuccoTgtLWingRotZ, 0.8f, 7000.0f);
    Math_ApproachF(&sCuccoLWingRotY, sCuccoTgtLWingRotY, 0.8f, 7000.0f);
    Math_ApproachF(&sCuccoLWingRotX, sCuccoTgtLWingRotX, 0.8f, 7000.0f);
    Math_ApproachF(&sCuccoRWingRotZ, sCuccoTgtRWingRotZ, 0.8f, 7000.0f);
    Math_ApproachF(&sCuccoRWingRotY, sCuccoTgtRWingRotY, 0.8f, 7000.0f);
    Math_ApproachF(&sCuccoRWingRotX, sCuccoTgtRWingRotX, 0.8f, 7000.0f);
}

// Port of EnNiw_OverrideLimbDraw (z_en_niw.c:1120).
static s32 Sw97_CuccoOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg) {
    (void)play;
    (void)dList;
    (void)pos;
    (void)arg;

    if (limbIndex == NIW_LIMB_UPPER_BODY) {
        rot->y += (s16)sCuccoUpperBodyRotY;
    }
    if (limbIndex == NIW_LIMB_HEAD) {
        rot->y += (s16)sCuccoHeadRotY;
    }
    if (limbIndex == NIW_LIMB_RIGHT_WING_ROOT) {
        rot->x += (s16)sCuccoRWingRotX;
        rot->y += (s16)sCuccoRWingRotY;
        rot->z += (s16)sCuccoRWingRotZ;
    }
    if (limbIndex == NIW_LIMB_LEFT_WING_ROOT) {
        rot->x += (s16)sCuccoLWingRotX;
        rot->y += (s16)sCuccoLWingRotY;
        rot->z += (s16)sCuccoLWingRotZ;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════
// Flight / pound / flock state
// ═══════════════════════════════════════════════════════════════════════

static s16 sCuccoFlapsLeft;  // Kirby budget; refills only on the ground
static s16 sCuccoFlapPose;   // Frames left holding the wide-wing flap pose
static s16 sCuccoHurtTimer;  // Panic-squawk window after taking a hit
static s16 sCuccoPrevInvinc; // Edge-detects damage via invincibilityTimer
static s16 sCuccoGliding;    // Set each frame the glide is actually applied
static s16 sCuccoPoundPhase; // 0 none, 1 hover telegraph, 2 slam
static s16 sCuccoPoundTimer;
static s16 sCuccoFlockCd;

// i-frames WE granted ourselves (spin, shield block). Without this the damage
// edge-detect below would read our own intangibility as a fresh hit and fire
// the panic squawk every time the cucco spins.
static s16 sCuccoSelfIFrames;

static s16 sCuccoSpinTimer;      // Aerial spin (B in air); 0 = inactive
static f32 sCuccoSpinYaw;        // Accumulated model spin, radians
static s16 sCuccoWhackTimer;     // Ground Wing Whack; 0 = inactive
static u8 sCuccoWhackMoving;     // Whack started while moving → body-spin variant
static s16 sCuccoLegSpringTimer; // Crouch countdown before the launch

static u8 sCuccoShieldUp;      // 1 while OUR shield quad is registered
static s16 sCuccoShieldBlocks; // Blocked hits banked toward the flock
static s16 sCuccoShieldGrace;  // Keeps currentShield forced briefly after release

static u8 sCuccoAimActive; // In egg-aim (mirilla) mode
static u8 sCuccoEggType;   // CUCCO_EGG_* selector
static s16 sCuccoEggFireCd;

typedef struct {
    u8 active;
    s16 life;
    s16 peckCd;
    f32 orbitAngle;
    Vec3f pos;
    s16 yaw;
} Sw97CuccoBird;

static Sw97CuccoBird sCuccoFlock[CUCCO_FLOCK_MAX];

static void Sw97_CuccoResetCombat(void) {
    sCuccoFlapsLeft = CUCCO_MAX_FLAPS;
    sCuccoFlapPose = 0;
    sCuccoHurtTimer = 0;
    sCuccoPrevInvinc = 0;
    sCuccoGliding = 0;
    sCuccoPoundPhase = 0;
    sCuccoPoundTimer = 0;
    sCuccoFlockCd = 0;
    sCuccoSelfIFrames = 0;
    sCuccoSpinTimer = 0;
    sCuccoSpinYaw = 0.0f;
    sCuccoWhackTimer = 0;
    sCuccoWhackMoving = 0;
    sCuccoLegSpringTimer = 0;
    sCuccoShieldUp = 0;
    sCuccoShieldBlocks = 0;
    sCuccoShieldGrace = 0;
    sCuccoAimActive = 0;
    sCuccoEggType = CUCCO_EGG_REGULAR;
    sCuccoEggFireCd = 0;
    for (s32 i = 0; i < CUCCO_FLOCK_MAX; i++) {
        sCuccoFlock[i].active = 0;
    }
}

// Feather/dust puff. func_8002836C is the same soft-sprite spawner the
// GustJar and tornado VFX use (z_magic_wind.inc.c:685).
static void Sw97_CuccoBurst(PlayState* play, Vec3f* center, s32 count, f32 spread, f32 rise) {
    Color_RGBA8 prim = { 255, 250, 235, 255 };
    Color_RGBA8 env = { 200, 170, 120, 160 };
    Vec3f accel = { 0.0f, -0.12f, 0.0f };

    for (s32 i = 0; i < count; i++) {
        f32 angle = Rand_ZeroFloat(2.0f * M_PI);
        f32 mag = Rand_ZeroFloat(spread);
        Vec3f pos = { center->x + sinf(angle) * mag, center->y + Rand_ZeroFloat(12.0f), center->z + cosf(angle) * mag };
        Vec3f vel = { sinf(angle) * (mag * 0.15f), rise + Rand_ZeroFloat(1.2f), cosf(angle) * (mag * 0.15f) };
        func_8002836C(play, &pos, &vel, &accel, &prim, &env, 190, 24, 18);
    }
}

// Nearest living enemy within `range` of `from`, or NULL.
static Actor* Sw97_CuccoNearestEnemy(PlayState* play, Vec3f* from, f32 range) {
    Actor* best = NULL;
    f32 bestSq = SQ(range);

    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != NULL; a = a->next) {
        if (a->update == NULL || a->colChkInfo.health <= 0)
            continue;
        f32 dx = a->world.pos.x - from->x;
        f32 dy = a->world.pos.y - from->y;
        f32 dz = a->world.pos.z - from->z;
        f32 distSq = SQ(dx) + SQ(dy) + SQ(dz);
        if (distSq < bestSq) {
            bestSq = distSq;
            best = a;
        }
    }
    return best;
}

// Radial hit. Drains colChkInfo.health directly rather than setting
// colChkInfo.damage: damage alone does nothing unless the target's own
// collider reports AC_HIT that frame, which is why the tornado tick
// (z_magic_wind.inc.c:650) writes health too. Same approach here.
static void Sw97_CuccoRadialHit(PlayState* play, Vec3f* center, f32 radius, s32 damage, f32 knockback) {
    f32 radiusSq = SQ(radius);

    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != NULL; a = a->next) {
        if (a->update == NULL || a->colChkInfo.health <= 0)
            continue;
        f32 dx = a->world.pos.x - center->x;
        f32 dy = a->world.pos.y - center->y;
        f32 dz = a->world.pos.z - center->z;
        if ((SQ(dx) + SQ(dy) + SQ(dz)) > radiusSq)
            continue;

        s16 hp = a->colChkInfo.health;
        s16 drain = (hp > damage) ? (s16)damage : hp;
        a->colChkInfo.health -= drain;
        if (a->colChkInfo.health <= 0) {
            a->colChkInfo.health = 0;
            a->colChkInfo.damage = 8; // Let the actor's own death path notice
        }
        if (knockback > 0.0f) {
            a->world.rot.y = Math_FAtan2F(dx, dz) * (0x8000 / M_PI);
            a->speedXZ = knockback;
            a->velocity.y = knockback * 0.6f;
        }
    }
}

static void Sw97_CuccoPoundImpact(PlayState* play, Player* player) {
    Vec3f at = player->actor.world.pos;

    Sw97_CuccoRadialHit(play, &at, CUCCO_POUND_RADIUS, CUCCO_POUND_DAMAGE, 9.0f);
    Sw97_CuccoBurst(play, &at, 18, CUCCO_POUND_RADIUS * 0.45f, 2.4f);
    Rumble_Request(300.0f, 220, 24, 120);
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_M);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_BOMB_EXPLOSION);
}

// ─── Flock ─────────────────────────────────────────────────────────────

static void Sw97_CuccoSummonFlock(PlayState* play, Player* player) {
    if (sCuccoFlockCd > 0)
        return;

    s32 summoned = 0;
    for (s32 i = 0; i < CUCCO_FLOCK_MAX; i++) {
        Sw97CuccoBird* bird = &sCuccoFlock[i];
        if (bird->active)
            continue;

        bird->active = 1;
        bird->life = CUCCO_FLOCK_LIFE;
        bird->peckCd = 0;
        bird->orbitAngle = (2.0f * M_PI / CUCCO_FLOCK_MAX) * i;
        // Spawn on the orbit ring so they fly in rather than pop at Link's feet.
        bird->pos.x = player->actor.world.pos.x + sinf(bird->orbitAngle) * CUCCO_FLOCK_ORBIT_R;
        bird->pos.y = player->actor.world.pos.y + CUCCO_FLOCK_ORBIT_Y;
        bird->pos.z = player->actor.world.pos.z + cosf(bird->orbitAngle) * CUCCO_FLOCK_ORBIT_R;
        bird->yaw = player->actor.shape.rot.y;
        summoned++;
    }

    if (summoned > 0) {
        sCuccoFlockCd = CUCCO_FLOCK_COOLDOWN;
        Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_M);
        Sw97_CuccoBurst(play, &player->actor.world.pos, 10, 40.0f, 1.6f);
        Rumble_Request(120.0f, 100, 14, 60);
    }
}

static void Sw97_TickCuccoFlock(PlayState* play, Player* player) {
    if (sCuccoFlockCd > 0)
        sCuccoFlockCd--;

    for (s32 i = 0; i < CUCCO_FLOCK_MAX; i++) {
        Sw97CuccoBird* bird = &sCuccoFlock[i];
        if (!bird->active)
            continue;

        if (--bird->life <= 0) {
            bird->active = 0;
            Sw97_CuccoBurst(play, &bird->pos, 6, 18.0f, 1.0f);
            continue;
        }
        if (bird->peckCd > 0)
            bird->peckCd--;

        Actor* target = Sw97_CuccoNearestEnemy(play, &bird->pos, CUCCO_FLOCK_RANGE);

        if (target != NULL) {
            // Dive: home straight at the target, peck on contact.
            f32 dx = target->world.pos.x - bird->pos.x;
            f32 dy = (target->world.pos.y + target->shape.yOffset * 0.5f + 10.0f) - bird->pos.y;
            f32 dz = target->world.pos.z - bird->pos.z;
            f32 dist = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));

            if (dist > 0.001f) {
                f32 step = CUCCO_FLOCK_SPEED / dist;
                bird->pos.x += dx * step;
                bird->pos.y += dy * step;
                bird->pos.z += dz * step;
                bird->yaw = (s16)(Math_FAtan2F(dx, dz) * (0x8000 / M_PI));
            }

            if (dist < CUCCO_FLOCK_HIT_DIST && bird->peckCd == 0) {
                s16 hp = target->colChkInfo.health;
                s16 drain = (hp > CUCCO_FLOCK_DAMAGE) ? (s16)CUCCO_FLOCK_DAMAGE : hp;
                target->colChkInfo.health -= drain;
                if (target->colChkInfo.health <= 0) {
                    target->colChkInfo.health = 0;
                    target->colChkInfo.damage = 8;
                }
                bird->peckCd = CUCCO_FLOCK_PECK_CD;
                Audio_PlayActorSound2(target, NA_SE_EV_CHICKEN_CRY_A);
                Sw97_CuccoBurst(play, &bird->pos, 4, 12.0f, 0.8f);
                // Bounce back off the peck so it re-approaches instead of
                // sitting inside the enemy's model.
                bird->pos.x -= dx * (18.0f / dist);
                bird->pos.y -= dy * (18.0f / dist);
                bird->pos.z -= dz * (18.0f / dist);
            }
        } else {
            // No target: orbit Link.
            bird->orbitAngle += 0.09f;
            if (bird->orbitAngle > (2.0f * M_PI))
                bird->orbitAngle -= (2.0f * M_PI);

            Vec3f want = {
                player->actor.world.pos.x + sinf(bird->orbitAngle) * CUCCO_FLOCK_ORBIT_R,
                player->actor.world.pos.y + CUCCO_FLOCK_ORBIT_Y,
                player->actor.world.pos.z + cosf(bird->orbitAngle) * CUCCO_FLOCK_ORBIT_R,
            };
            Math_ApproachF(&bird->pos.x, want.x, 0.3f, 12.0f);
            Math_ApproachF(&bird->pos.y, want.y, 0.3f, 12.0f);
            Math_ApproachF(&bird->pos.z, want.z, 0.3f, 12.0f);
            // Face along the orbit tangent.
            bird->yaw = (s16)((bird->orbitAngle + (M_PI * 0.5f)) * (0x8000 / M_PI));
        }
    }
}

// Drawn from Sw97_DrawCuccoForm. All birds share the player cucco's joint
// table — one SkelAnime_Update a frame, three draws. They flap in sync, which
// at flock scale reads as a swarm rather than a bug.
static void Sw97_DrawCuccoFlock(PlayState* play) {
    if (!sCuccoSkelInited)
        return;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    for (s32 i = 0; i < CUCCO_FLOCK_MAX; i++) {
        Sw97CuccoBird* bird = &sCuccoFlock[i];
        if (!bird->active)
            continue;

        Matrix_Translate(bird->pos.x, bird->pos.y, bird->pos.z, MTXMODE_NEW);
        Matrix_RotateY((f32)bird->yaw * (M_PI / 32768.0f), MTXMODE_APPLY);
        Matrix_Scale(0.011f, 0.011f, 0.011f, MTXMODE_APPLY);
        SkelAnime_DrawFlexOpa(play, sCuccoSkel.skeleton, sCuccoSkel.jointTable, sCuccoSkel.dListCount,
                              Sw97_CuccoOverrideLimbDraw, NULL, NULL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// ═══════════════════════════════════════════════════════════════════════
// Melee — Wing Whack (ground) and the aerial spin
//
// These use a REAL AT collider rather than the direct health drain that
// Sw97_CuccoRadialHit does for the pound. The reason is DMG_BOOMERANG: it is
// a damage-TABLE index that every enemy answers for itself (Skulltula and
// Deku Baba stuns, Gohma's and Barinade's weak points, eye switches). A
// health drain bypasses all of that, which is precisely what something
// replacing the boomerang must not do.
// ═══════════════════════════════════════════════════════════════════════

static ColliderCylinder sCuccoMeleeCol;
static u8 sCuccoMeleeColInited;

static ColliderCylinderInit sCuccoMeleeColInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { DMG_BOOMERANG, 0x00, CUCCO_SPIN_DAMAGE },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    // Integer literals, not the f32 tuning macros: Cylinder16 is s16 and a
    // float in a static initializer is a narrowing conversion. The radius is
    // overwritten per-use by Sw97_CuccoMeleeRegister anyway.
    { 45, 45, CUCCO_SPIN_YSHIFT, { 0, 0, 0 } },
};

// Register the melee hitbox for one frame. Callers MUST rate-limit this:
// CollisionCheck_SetAT resets the collider on every registration, so calling
// it each frame lands one hit per enemy per frame and melts bosses.
static void Sw97_CuccoMeleeRegister(PlayState* play, Player* player, f32 radius, u8 damage) {
    if (!sCuccoMeleeColInited) {
        Collider_InitCylinder(play, &sCuccoMeleeCol);
        Collider_SetCylinder(play, &sCuccoMeleeCol, &player->actor, &sCuccoMeleeColInit);
        sCuccoMeleeColInited = 1;
    }
    sCuccoMeleeCol.dim.radius = (s16)radius;
    sCuccoMeleeCol.info.toucher.damage = damage;
    Collider_UpdateCylinder(&player->actor, &sCuccoMeleeCol);
    // Self-hit is impossible: CollisionCheck_AC skips colAC->actor == colAT->actor
    // unless AT_SELF, and here both are the player.
    CollisionCheck_SetAT(play, &play->colChkCtx, &sCuccoMeleeCol.base);
}

static void Sw97_CuccoMeleeDestroy(PlayState* play) {
    if (sCuccoMeleeColInited) {
        Collider_DestroyCylinder(play, &sCuccoMeleeCol);
        sCuccoMeleeColInited = 0;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Cucco shield
//
// Vanilla will not help us here. Player_UpdateShieldCollider
// (z_player_lib.c:1927) is only ever called from the player's own limb-draw
// callback, and cucco mode replaces that callback wholesale — plus it
// requires PLAYER_STATE1_SHIELDING, which never gets set because R is
// stripped from Link's input. So we position and register the quad ourselves.
//
// Reflection itself is decentralised in OoT: the ENGINE only sets AT_BOUNCED
// on whatever touches an AC_HARD collider (z_collision_check.c:1748), and
// each projectile decides what to do about it. EnNutsball (z_en_nutsball.c:126)
// and EnOkuta (z_en_okuta.c:496) both gate on player->currentShield being a
// Deku shield and then aim themselves along player->shieldMf. So to get real
// reflection we force both of those fields while the cucco shield is up.
// ═══════════════════════════════════════════════════════════════════════

// Quad corners in cucco-local space, roughly a Deku shield squared up.
static Vec3f sCuccoShieldQuadSrc[4] = {
    { -18.0f, -14.0f, 8.0f },
    { 18.0f, -14.0f, 8.0f },
    { -18.0f, 16.0f, 8.0f },
    { 18.0f, 16.0f, 8.0f },
};

static s32 Sw97_CuccoHasRealShield(void) {
    return SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD)) != PLAYER_SHIELD_NONE;
}

s32 Sw97_CuccoShieldIsUp(void) {
    return sCuccoShieldUp;
}

static void Sw97_CuccoUpdateShieldCollider(PlayState* play, Player* player) {
    MtxF mf;
    Vec3f dst[4];
    s16 face = player->actor.shape.rot.y;
    f32 px = player->actor.world.pos.x;
    f32 py = player->actor.world.pos.y + CUCCO_SHIELD_Y;
    f32 pz = player->actor.world.pos.z;

    // SkinMatrix, not the global Matrix_* stack: we run inside Actor_UpdateAll
    // and the global stack belongs to the draw pass.
    SkinMatrix_SetTranslateRotateYXZScale(&mf, 1.0f, 1.0f, 1.0f, 0, face, 0, px, py, pz);
    for (s32 i = 0; i < 4; i++) {
        SkinMatrix_Vec3fMtxFMultXYZ(&mf, &sCuccoShieldQuadSrc[i], &dst[i]);
    }

    // Both reflect sites decode this as `yaw + 0x8000`, so store the facing
    // already rotated a half turn and the shot leaves along the cucco's nose.
    SkinMatrix_SetTranslateRotateYXZScale(&player->shieldMf, 1.0f, 1.0f, 1.0f, 0, face + 0x8000, 0, px, py, pz);

    player->shieldQuad.base.colType = COLTYPE_WOOD; // Deku: wood spark + sfx
    Collider_SetQuadVertices(&player->shieldQuad, &dst[0], &dst[1], &dst[2], &dst[3]);
    CollisionCheck_SetAC(play, &play->colChkCtx, &player->shieldQuad.base);
    CollisionCheck_SetAT(play, &play->colChkCtx, &player->shieldQuad.base);
}

static void Sw97_TickCuccoShield(PlayState* play, Player* player, u8 grounded, u8 rHeld) {
    u8 want = grounded && rHeld && !sCuccoAimActive && !sCuccoPoundPhase && !sCuccoSpinTimer;

    // With a real shield equipped we do nothing at all — R is left in Link's
    // input (see the conditional strip in customequipment.cpp) and his own
    // shield AI takes over. Registering our quad too would double-bounce every
    // projectile, since CollisionCheck_Set* does not de-duplicate.
    if (Sw97_CuccoHasRealShield()) {
        if (sCuccoShieldUp || sCuccoShieldGrace > 0) {
            // Picked a shield up mid-transformation: hand currentShield back
            // before walking away, or it stays stuck on our forced Deku.
            player->currentShield = SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD));
        }
        sCuccoShieldUp = 0;
        sCuccoShieldGrace = 0;
        sCuccoShieldBlocks = 0;
        return;
    }

    if (want) {
        sCuccoShieldUp = 1;
        sCuccoShieldGrace = CUCCO_SHIELD_GRACE;
        Sw97_CuccoUpdateShieldCollider(play, player);
    } else {
        sCuccoShieldUp = 0;
        if (sCuccoShieldGrace > 0)
            sCuccoShieldGrace--;
        if (sCuccoShieldGrace == 0)
            sCuccoShieldBlocks = 0;
    }

    // Written every frame from the true equipment, so restoring is automatic
    // and an abrupt exit can't strand a phantom Deku shield.
    player->currentShield = (sCuccoShieldUp || sCuccoShieldGrace > 0)
                                ? PLAYER_SHIELD_DEKU
                                : SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD));
}

// Called from z_player.c the instant a block is detected, next to
// DivineShield_OnShieldBlock. It has to be there: Player_UpdateShape clears
// AC_BOUNCED before any later dispatch could read it.
//
// Note the damage source. A vanilla block leaves colChkInfo.damage at ZERO —
// CollisionCheck_ApplyDamage only accumulates `if (!(acFlags & AC_HARD))` and
// the shield quad is AC_HARD, so vanilla never computes the damage rather than
// negating it. To make the cucco eat the hit anyway we read it off the
// attacker's own toucher.
void Sw97_CuccoOnShieldBlock(Player* player, PlayState* play) {
    ColliderInfo* hit;
    s32 dmg;

    if (!sCuccoShieldUp) {
        return; // real-shield fallback keeps vanilla's clean block
    }

    hit = player->shieldQuad.info.acHitInfo;
    dmg = (hit != NULL) ? hit->toucher.damage : 0;
    if (dmg <= 0) {
        dmg = CUCCO_SHIELD_MIN_CHIP;
    }

    func_80837B18(play, player, -dmg);
    Player_SetIntangibility(player, 20);
    sCuccoSelfIFrames = 20; // don't let our own i-frames read as a fresh hit

    Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_M);
    Sw97_CuccoBurst(play, &player->actor.world.pos, 8, 22.0f, 1.4f);

    if (++sCuccoShieldBlocks >= CUCCO_SHIELD_BLOCKS_TO_FLOCK) {
        sCuccoShieldBlocks = 0;
        sCuccoFlockCd = 0; // an earned summon ignores the cooldown
        Sw97_CuccoSummonFlock(play, player);
    }
}

// Null-body override — used to walk Link's skeleton without rendering any
// limb geometry. Same idea as GaroForm_OverrideLimbDraw in
// garo_post_limb.cpp:47.
static s32 Sw97_CuccoLinkOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                          void* arg) {
    (void)play;
    (void)limbIndex;
    (void)pos;
    (void)rot;
    (void)arg;
    *dList = NULL;
    return 0;
}

// PostLimbDraw — refreshes the per-frame Link tracking fields that vanilla
// Player_Draw normally populates: bodyPartsPos[], focus.pos (Navi anchor),
// feetPos[] (shadow anchor). Without this, shadow + Navi stay frozen at the
// transformation point. Copied from GaroForm_PostLimbDraw (the essential
// shadow/Navi/bodyParts bits — Garo's sword-trail / held-actor branches are
// not needed for the cucco model swap).
static void Sw97_CuccoLinkPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    (void)dList;
    (void)rot;
    Player* player = (Player*)thisx;
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };

    if (limbIndex > 0 && limbIndex < PLAYER_LIMB_MAX) {
        s8 bodyPart = gPlayerLimbToBodyPart[limbIndex];
        if (bodyPart >= 0) {
            Matrix_MultVec3f(&zeroVec, &player->bodyPartsPos[bodyPart]);
        }
    }
    if (limbIndex == PLAYER_LIMB_HEAD) {
        Vec3f headOffset = { 1100.0f, -700.0f, 0.0f };
        Matrix_MultVec3f(&headOffset, &player->actor.focus.pos);
    }
    if (limbIndex == PLAYER_LIMB_L_FOOT || limbIndex == PLAYER_LIMB_R_FOOT) {
        Actor_SetFeetPos(&player->actor, limbIndex, PLAYER_LIMB_L_FOOT, &zeroVec, PLAYER_LIMB_R_FOOT, &zeroVec);
    }
}

// Cucco visual at thisx->world.pos. Same scaled translate + Y-flip on egg
// throws as before, called from Sw97_DrawCuccoForm after the null-body pass.
static void Sw97_DrawCuccoModel(Actor* thisx, PlayState* play) {
    if (!sCuccoSkelInited)
        return;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, MTXMODE_NEW);
    f32 baseYaw = (f32)thisx->shape.rot.y * (M_PI / 32768.0f);
    f32 flipYaw = 0.0f;
    if (gSw97CuccoFlipTimer > 0) {
        f32 t = (f32)gSw97CuccoFlipTimer / (f32)CUCCO_FLIP_FRAMES;
        flipYaw = (1.0f - t) * M_PI;
    }
    // sCuccoSpinYaw accumulates during the aerial spin and the moving Wing
    // Whack, reusing the same rotation slot as the egg-throw flip.
    Matrix_RotateY(baseYaw + flipYaw + sCuccoSpinYaw, MTXMODE_APPLY);
    f32 s = 0.015f;
    Matrix_Scale(s, s, s, MTXMODE_APPLY);
    // Override supplies the wing/neck/body rotation the single baked animation
    // does not have — see the Cucco AI block above.
    SkelAnime_DrawFlexOpa(play, sCuccoSkel.skeleton, sCuccoSkel.jointTable, sCuccoSkel.dListCount,
                          Sw97_CuccoOverrideLimbDraw, NULL, NULL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Public entry — called from customequipment.cpp's VB_PLAYER_DRAW_BEGIN hook
// when cucco mode is active. Mirrors the GaroForm / MmForm pattern:
//   Pass 1: walk Link's skeleton with nulled DLs so PostLimbDraw refreshes
//           bodyPartsPos / focus.pos / feetPos[] → shadow + Navi follow
//   Pass 2: render the cucco model at Link's world.pos
void Sw97_DrawCuccoForm(PlayState* play, Player* player) {
    if (player->skelAnime.skeleton != NULL && player->skelAnime.jointTable != NULL) {
        SkelAnime_DrawFlexLod(play, player->skelAnime.skeleton, player->skelAnime.jointTable,
                              player->skelAnime.dListCount, Sw97_CuccoLinkOverrideLimbDraw, Sw97_CuccoLinkPostLimbDraw,
                              player, 0);
    }
    Sw97_DrawCuccoModel(&player->actor, play);
    Sw97_DrawCuccoFlock(play);
}

void Sw97_StartCuccoMode(void) {
    if (gSw97CuccoModeActive || gSw97CuccoModePending)
        return;                             // idempotent
    gSw97CuccoModeSource = CUCCO_SRC_SPELL; // soul arrow → the timed, item-less form
    // Don't activate immediately — Link is in first-person aim CS right
    // now. Set pending and let the per-frame tick activate once the
    // first-person camera setting releases (mirror magic_soul.inc.c:117).
    gSw97CuccoModePending = 1;
}

static void Sw97_ActivateCuccoMode(void) {
    gSw97CuccoModePending = 0;
    gSw97CuccoModeActive = 1;
    gSw97CuccoModeTimer = CUCCO_MODE_FRAMES;
    Sw97_CuccoResetPose();
    Sw97_CuccoResetCombat();
}

void Sw97_EndCuccoMode(void) {
    if (!gSw97CuccoModeActive && !gSw97CuccoModePending)
        return;
    gSw97CuccoModeActive = 0;
    gSw97CuccoModePending = 0;
    gSw97CuccoModeTimer = 0;
    gSw97CuccoExitFx = 1;
    // Player flag cleanup + draw restoration happens in the inactive branch
    // of Sw97_TickCuccoMode on the next frame.
}

s32 Sw97_IsCuccoModeActive(void) {
    return gSw97CuccoModeActive;
}

// Scene tracking — reset state on scene change so the next tick doesn't
// reference a stale collision context / pos.
static s32 gSw97CuccoLastScene = -1;

// ───────────────────────────────────────────────────────────────────────
// Cucco eggs — while cucco mode is active, ANY arrow Link fires via the
// vanilla bow/slingshot aim+release CS gets swapped visually to a pocket
// egg + throttled to a slow drift. Elemental params (fire/ice/light/dark/
// soul/wind) are still respected — the SW97 hit hooks fire normally
// because the underlying actor is still EnArrow. Aim behavior is 100%
// vanilla: user pulls the bow, aims first-person, releases → egg flies.
// ───────────────────────────────────────────────────────────────────────

// Cucco egg tuning — vanilla arrows use Actor_SetProjectileSpeed(150), so
// SPEED_MAX / 150 is the scale factor applied to speedXZ AND velocity.y on
// the release frame (preserves the aim's pitch angle proportionally). Extra
// timer beat keeps eggs airborne long enough to reach enemies at range,
// which also fixes the "no damage" report — vanilla timer=12 was too short
// once we slowed the egg down, so it died before hitting anything.
#define CUCCO_EGG_SPEED_MAX 30.0f
#define CUCCO_EGG_TIMER 40       // frames of flight (vanilla arrow = 12)
#define CUCCO_EGG_ARC_BOOST 1.5f // extra +vY on release for a proper egg arc

// Needed to bump EnArrow::timer from the update hook (extend flight time).
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"

// Banjo-Kazooie style 3D egg — vanilla 3D bubble sphere (gEffBubbleDL) with:
//   * ellipsoid scaling (Y taller than X = Z)
//   * per-element primColor tint (fire=red, ice=cyan, light=gold, dark=purple,
//     wind=green, soul=amber, neutral=white)
//   * envColor darker shade for a soft outline highlight
// The bubble DL expects a texture at segment 0x08; we bind gEffBubble1Tex so
// the surface has a subtle patterned shading (like BK's slight egg noise).
#include "objects/gameplay_keep/gameplay_keep.h"
static void Sw97_CuccoEgg_GetColors(s16 arrowParams, Color_RGBA8* prim, Color_RGBA8* env) {
    switch (arrowParams) {
        case ARROW_SW97_FIRE:
            *prim = (Color_RGBA8){ 255, 110, 40, 255 };
            *env = (Color_RGBA8){ 180, 30, 0, 255 };
            break;
        case ARROW_SW97_ICE:
            *prim = (Color_RGBA8){ 100, 210, 255, 255 };
            *env = (Color_RGBA8){ 10, 90, 200, 255 };
            break;
        case ARROW_SW97_LIGHT:
            *prim = (Color_RGBA8){ 255, 240, 130, 255 };
            *env = (Color_RGBA8){ 200, 150, 0, 255 };
            break;
        case ARROW_SW97_0C:
            *prim = (Color_RGBA8){ 150, 70, 210, 255 };
            *env = (Color_RGBA8){ 60, 10, 120, 255 };
            break; // Dark
        case ARROW_SW97_0D:
            *prim = (Color_RGBA8){ 255, 200, 100, 255 };
            *env = (Color_RGBA8){ 180, 130, 0, 255 };
            break; // Soul
        case ARROW_SW97_0E:
            *prim = (Color_RGBA8){ 150, 255, 150, 255 };
            *env = (Color_RGBA8){ 0, 130, 0, 255 };
            break; // Wind
        // Vanilla params, used by the cucco's own egg wheel: fire and light
        // deliberately use the stock arrow types so they still light torches
        // and satisfy the game's own `params == ARROW_FIRE/LIGHT` checks.
        case ARROW_FIRE:
            *prim = (Color_RGBA8){ 255, 110, 40, 255 };
            *env = (Color_RGBA8){ 180, 30, 0, 255 };
            break;
        case ARROW_ICE:
            *prim = (Color_RGBA8){ 100, 210, 255, 255 };
            *env = (Color_RGBA8){ 10, 90, 200, 255 };
            break;
        case ARROW_LIGHT:
            *prim = (Color_RGBA8){ 255, 240, 130, 255 };
            *env = (Color_RGBA8){ 200, 150, 0, 255 };
            break;
        default:
            *prim = (Color_RGBA8){ 255, 255, 255, 255 };
            *env = (Color_RGBA8){ 130, 130, 130, 255 };
            break;
    }
}

void Sw97_DrawCuccoEgg(Actor* thisx, PlayState* play) {
    Color_RGBA8 prim, env;
    Sw97_CuccoEgg_GetColors(thisx->params, &prim, &env);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, MTXMODE_NEW);
    // Ellipse: X = Z base, Y taller for the classic egg silhouette.
    Matrix_Scale(0.02f, 0.028f, 0.02f, MTXMODE_APPLY);
    // Billboard so the egg always presents the same silhouette to the camera,
    // the way Banjo-Tooie's eggs do. Same idiom as z_magic_soul.inc.c:271.
    // This replaces the old shape.rot.y spin, which is meaningless on a
    // rotationally symmetric egg and only made the highlight swim.
    Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, "cucco_egg", __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, prim.r, prim.g, prim.b, prim.a);
    gDPSetEnvColor(POLY_OPA_DISP++, env.r, env.g, env.b, env.a);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gEffBubble1Tex));
    gSPDisplayList(POLY_OPA_DISP++, gEffBubbleDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Called from customequipment.cpp's OnActorInit hook when an EnArrow spawns
// while cucco mode is active. Marks the arrow via home.rot.z (unused by
// EnArrow) so the Update hook can identify + throttle it, and swaps its draw
// to the pocket-egg model.
// home.rot.z is a bitfield, not a plain sentinel: the tag occupies the high
// bits and the low two carry state. It used to be a bare 0x1E66 compared for
// equality, which meant any extra flag (like the bomb bit) would fall into the
// "already fired" branch and skip the release scaling entirely.
#define CUCCO_EGG_MARKER 0x1E60
#define CUCCO_EGG_STATE_MSK 0x3
#define CUCCO_EGG_FIRED_BIT 0x1
#define CUCCO_EGG_BOMB_BIT 0x2
#define CUCCO_EGG_IS_TAGGED(z) (((z) & ~CUCCO_EGG_STATE_MSK) == CUCCO_EGG_MARKER)

void Sw97_TagCuccoEgg(Actor* arrow) {
    arrow->draw = Sw97_DrawCuccoEgg;
    arrow->home.rot.z = CUCCO_EGG_MARKER;
}

// Called from customequipment.cpp's OnActorUpdate hook every frame the
// tagged arrow is alive.
//
// Vanilla release: Actor_SetProjectileSpeed(actor, 150) sets
//   speedXZ    = cos(rot.x) * 150   (horizontal component from aim pitch)
//   velocity.y = -sin(rot.x) * 150  (vertical component from aim pitch)
// So the release is very fast (150 units/frame) and its pitch encodes the
// aim direction. Vanilla timer = 12 frames → 1800-unit range.
//
// For a BK-style thrown egg we want ~⅕ speed BUT proportionally more
// airtime so the range is still usable AND enemies can be hit (short
// timer + slow speed = "no damage" report). On the release frame we:
//   1. Scale BOTH speedXZ and velocity.y by SPEED_MAX/150 (preserves the
//      aim pitch: steep aim still steep, flat still flat).
//   2. Add a small upward boost so eggs always start with a clean arc
//      instead of nose-diving on flat aim.
//   3. Extend arrow->timer well past vanilla so the egg reaches enemies.
// home.rot.z encodes state (MARKER = tagged pre-fire, MARKER+1 = scaled).
#define CUCCO_EGG_VANILLA_RELEASE_SPEED 150.0f
void Sw97_TickCuccoEggClamp(Actor* arrow) {
    if (!CUCCO_EGG_IS_TAGGED(arrow->home.rot.z))
        return;
    EnArrow* enArrow = (EnArrow*)arrow;

    // ─── Bomb egg ───────────────────────────────────────────────────────
    // This hook is the detonation trigger, which costs us no bookkeeping at
    // all: z_actor.c:2815 runs OnActorUpdate immediately after actor->update
    // and does NOT guard on update != NULL, so we still get called on the
    // very frame EnArrow_Fly kills itself on impact.
    if (arrow->home.rot.z & CUCCO_EGG_BOMB_BIT) {
        u8 impacted = (enArrow->hitFlags & 1) || (enArrow->collider.base.atFlags & AT_HIT);
        u8 expired = (arrow->update == NULL) || (enArrow->timer == 0);
        // Arm only after some travel — otherwise a point-blank shot blows up
        // in the cucco's face.
        u8 armed = Math_Vec3f_DistXYZ(&arrow->world.pos, &arrow->home.pos) > CUCCO_EGG_BOMB_ARM_DIST;

        if (impacted && armed) {
            Vec3f at = arrow->world.pos;
            arrow->home.rot.z = 0; // one-shot: the hook can fire again on the kill frame
            BombArrows_SpawnInstantBomb(gPlayState, &at);
            if (arrow->update != NULL) {
                Actor_Kill(arrow);
            }
            return;
        }
        if (expired) {
            // Fuse ran out mid-air, or it hit before arming: fizzle rather
            // than leaving a bomb hanging in the sky.
            arrow->home.rot.z = 0;
            return;
        }
    }

    if (!(arrow->home.rot.z & CUCCO_EGG_FIRED_BIT)) {
        // Pre-fire: wait for release frame (speedXZ jumps above vanilla
        // "held" range — anything > 10 means the projectile-speed set fired).
        if (arrow->speedXZ > 10.0f) {
            f32 scale = CUCCO_EGG_SPEED_MAX / CUCCO_EGG_VANILLA_RELEASE_SPEED;
            arrow->speedXZ *= scale;
            arrow->velocity.y *= scale;
            arrow->velocity.y += CUCCO_EGG_ARC_BOOST; // clean arc
            enArrow->timer = CUCCO_EGG_TIMER;         // extend flight
            arrow->home.rot.z |= CUCCO_EGG_FIRED_BIT;
        }
    } else {
        // Post-fire: cap horizontal speed and keep the timer topped up so
        // the slow egg has time to reach enemies at range. Bumping (not
        // reset) — if a wall clamp already dropped it to 20, we don't want
        // to make the arrow immortal, just make sure it lives long enough
        // to hit its target at BK-style thrown-egg speed.
        if (arrow->speedXZ > CUCCO_EGG_SPEED_MAX) {
            arrow->speedXZ = CUCCO_EGG_SPEED_MAX;
        }
        if (enArrow->timer < CUCCO_EGG_TIMER - 1) {
            enArrow->timer = CUCCO_EGG_TIMER - 1;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Egg aim mode (R + B)
//
// Reuses the first-person helper the custom items already share
// (soh/mods/items/helpers/camera_helper.c) rather than driving the camera
// by hand — item_bombarrows.c does the same. It is already in this
// translation unit: custom_items.c includes it, and z_player.c includes
// custom_items.c at :61, well before sw97_router.c at :121.
//
// Each egg type maps onto a VANILLA arrow param wherever possible, so the
// game's own damage tables and item checks answer for us and no boss needs
// a special case:
//   regular → ARROW_NORMAL   stuns Gohma / Phantom Ganon like an arrow
//   fire    → ARROW_FIRE     lights torches, DMG_ARROW_FIRE
//   light   → ARROW_LIGHT    DMG_ARROW_LIGHT
//   ice     → ARROW_SW97_ICE DMG_ARROW_ICE *and* melts red ice, because
//                            BlueFireArrows.cpp:70-87 already treats the
//                            SW97 ice arrow as blue fire whenever the SW97
//                            medallions are enabled — which cucco mode implies
//   bomb    → ARROW_NORMAL carrying CUCCO_EGG_BOMB_BIT; the real payload is an
//                            EnBom detonated on impact (see the clamp above)
// ═══════════════════════════════════════════════════════════════════════

static const s16 sCuccoEggParams[CUCCO_EGG_TYPE_COUNT] = {
    ARROW_NORMAL,   // CUCCO_EGG_REGULAR
    ARROW_FIRE,     // CUCCO_EGG_FIRE
    ARROW_LIGHT,    // CUCCO_EGG_LIGHT
    ARROW_SW97_ICE, // CUCCO_EGG_ICE
    ARROW_NORMAL,   // CUCCO_EGG_BOMB
};

s32 Sw97_CuccoEggAimActive(void) {
    return sCuccoAimActive;
}

static void Sw97_CuccoEnterEggAim(PlayState* play, Player* player) {
    sCuccoAimActive = 1;
    sCuccoEggFireCd = 0;
    // Aim mode short-circuits the rest of the tick, including the shield
    // update — so drop the shield here rather than leaving currentShield
    // stranded on Deku for as long as the player keeps aiming.
    sCuccoShieldUp = 0;
    sCuccoShieldGrace = 0;
    sCuccoShieldBlocks = 0;
    player->currentShield = SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD));
    FirstPerson_Init(player, play);
    Audio_PlayActorSound2(&player->actor, NA_SE_SY_CAMERA_ZOOM_DOWN);
}

static void Sw97_CuccoExitEggAim(PlayState* play, Player* player) {
    if (!sCuccoAimActive)
        return;
    sCuccoAimActive = 0;
    FirstPerson_Exit(player, play);
    Audio_PlayActorSound2(&player->actor, NA_SE_SY_CAMERA_ZOOM_UP);
}

static void Sw97_CuccoFireEgg(PlayState* play, Player* player) {
    s16 aimYaw = FirstPerson_GetAimYaw(player);
    s16 aimPitch = FirstPerson_GetAimPitch(player);
    Actor* egg;

    egg = Actor_SpawnAsChild(&play->actorCtx, &player->actor, play, ACTOR_EN_ARROW, player->actor.world.pos.x,
                             player->actor.world.pos.y + 40.0f, player->actor.world.pos.z, aimPitch, aimYaw, 0,
                             sCuccoEggParams[sCuccoEggType]);
    if (egg == NULL)
        return;

    egg->world.rot.x = egg->shape.rot.x = aimPitch;
    egg->world.rot.y = egg->shape.rot.y = aimYaw;

    // Vanilla bow detach pattern (item_bombarrows.c:374-379). unk_A73 is the
    // load-bearing part: EnArrow_Shoot kills a parentless arrow without it.
    player->heldActor = egg;
    player->unk_A73 = 4;
    egg->parent = NULL;
    player->actor.child = NULL;
    player->heldActor = NULL;

    // We set the launch ourselves, so mark it fired — the clamp's "wait for
    // the vanilla release signature" branch would never trigger otherwise.
    egg->speedXZ = Math_CosS(aimPitch) * CUCCO_EGG_SPEED_MAX;
    egg->velocity.y = -Math_SinS(aimPitch) * CUCCO_EGG_SPEED_MAX + CUCCO_EGG_ARC_BOOST;
    ((EnArrow*)egg)->timer = CUCCO_EGG_TIMER;
    egg->home.rot.z |= CUCCO_EGG_FIRED_BIT;
    if (sCuccoEggType == CUCCO_EGG_BOMB) {
        egg->home.rot.z |= CUCCO_EGG_BOMB_BIT;
    }

    gSw97CuccoFlipTimer = CUCCO_FLIP_FRAMES; // reuse the existing throw flip
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_A);
}

// Returns 1 while aim mode owns the frame, so the caller can skip the whole
// movement/melee block.
static u8 Sw97_TickCuccoEggAim(PlayState* play, Player* player, u8 rHeld, u8 rPress, u8 lPress, u8 bPress) {
    if (!sCuccoAimActive)
        return 0;

    if (sCuccoEggFireCd > 0)
        sCuccoEggFireCd--;

    // Hold-R-to-aim: releasing R leaves. Keeps R+B reading naturally and
    // avoids needing a second exit binding.
    if (!rHeld) {
        Sw97_CuccoExitEggAim(play, player);
        return 0;
    }

    FirstPerson_Update(player, play);

    if (rPress || lPress) {
        s16 next = (s16)sCuccoEggType + (rPress ? 1 : -1);
        if (next < 0)
            next = CUCCO_EGG_TYPE_COUNT - 1;
        if (next >= CUCCO_EGG_TYPE_COUNT)
            next = 0;
        sCuccoEggType = (u8)next;
        Audio_PlayActorSound2(&player->actor, NA_SE_SY_CURSOR);
    }

    if (bPress && sCuccoEggFireCd == 0) {
        Sw97_CuccoFireEgg(play, player);
        sCuccoEggFireCd = CUCCO_EGG_FIRE_CD;
    }

    return 1;
}

void Sw97_TickCuccoMode(PlayState* play, Player* player) {
    // ─── PENDING: wait for first-person aim CS to end ───────────────────
    // magic_soul.inc.c:117 pattern — defer activation until the player has
    // left PLAYER_STATE1_IN_ITEM_CS. Activating mid-aim leaves the camera
    // setting stuck in first-person mode and the next setting change
    // glitches the angle.
    // ─── CVar-driven permanent form ─────────────────────────────────────
    // Two ways in, and they behave differently on purpose. The CVar form is a
    // persistent playable mode with items intact; the soul-arrow form is a
    // 30-second movement-only buff that pops the moment you reach for an item
    // (see the VB_CHANGE_HELD_ITEM_AND_USE_ITEM hook in customequipment.cpp).
    {
        s32 cvarOn = CVarGetInteger(CUCCO_MODE_CVAR, 0) != 0;
        if (cvarOn && !gSw97CuccoModeActive && !gSw97CuccoModePending) {
            gSw97CuccoModeSource = CUCCO_SRC_CVAR;
            gSw97CuccoModePending = 1;
        } else if (!cvarOn && gSw97CuccoModeActive && gSw97CuccoModeSource == CUCCO_SRC_CVAR) {
            Sw97_EndCuccoMode();
        }
    }

    if (gSw97CuccoModePending && player != NULL && play != NULL) {
        if (!(player->stateFlags1 & PLAYER_STATE1_IN_ITEM_CS)) {
            Sw97_ActivateCuccoMode();
            // Flash + sound on actual entry (mirrors magic_soul's flash
            // before kill on line 123).
            Rumble_Request(200.0f, 150, 20, 80);
            Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_M);
        }
    }

    // ─── INACTIVE: cleanup ──────────────────────────────────────────────
    // The VB_PLAYER_DRAW_BEGIN hook in customequipment.cpp checks
    // Sw97_IsCuccoModeActive() each frame, so just deactivating the flag
    // is enough — no draw swap to undo. We only do the entry-exit flash
    // once via gSw97CuccoExitFx.
    if (!gSw97CuccoModeActive) {
        if (gSw97CuccoExitFx && player != NULL) {
            gSw97CuccoExitFx = 0;
            player->invincibilityTimer = 20;
            sCuccoSkelInited = 0;
            Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_M);
            Rumble_Request(200.0f, 150, 20, 80);
            // Drop the flock and the pose — a stale slam phase would otherwise
            // still be clamping velocity the frame cucco mode ends.
            Sw97_CuccoExitEggAim(play, player);
            Sw97_CuccoResetCombat();
            Sw97_CuccoResetPose();
            Sw97_CuccoMeleeDestroy(play);
            // Never leave a phantom Deku shield behind: the shield tick forces
            // currentShield while it is up, and it is not running any more.
            player->currentShield = SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD));
        }
        gSw97CuccoLastScene = -1;
        return;
    }

    // Only the spell form is on a clock. The CVar form runs until toggled off.
    if (gSw97CuccoModeSource == CUCCO_SRC_SPELL) {
        if (--gSw97CuccoModeTimer <= 0) {
            Sw97_EndCuccoMode();
            return;
        }
    }

    if (player == NULL || play == NULL)
        return;

    // Scene change → skel seg pointers reference the old scene's gfxCtx; re-init.
    // The melee collider is bound to the old scene's collision context too, so
    // it has to go with it.
    if (gSw97CuccoLastScene >= 0 && gSw97CuccoLastScene != play->sceneNum) {
        sCuccoSkelInited = 0;
        Sw97_CuccoMeleeDestroy(play);
        Sw97_CuccoExitEggAim(play, player);
    }
    gSw97CuccoLastScene = play->sceneNum;

    // ─── First-frame setup: init the cucco skel ─────────────────────────
    // We do NOT swap player->actor.draw — the customequipment.cpp
    // VB_PLAYER_DRAW_BEGIN hook detects Sw97_IsCuccoModeActive() and routes
    // through Sw97_DrawCuccoForm, which walks Link's skeleton (null body) to
    // keep shadow + Navi tracking, then draws the cucco model on top.
    if (!sCuccoSkelInited) {
        Sw97_InitCuccoSkel(play);
    }

    // ─── Ivan-style: do NOT disable input/colliders, do NOT zero velocity
    // and do NOT do manual position math. Vanilla Player movement runs
    // normally — sword swings, walking anim, item C-buttons, doors, ladders,
    // collision — and we only TWEAK the physics quantities the engine
    // already produced.

    // ─── Inputs + ground state ──────────────────────────────────────────
    // A and R are both cleared from Link's own input in the
    // customequipment.cpp VB_SM64_PLAYER_PRE_ACTION hook (so his actionFunc
    // never rolls or raises a shield); we read the raw pad here instead.
    u8 grounded = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
    u16 pressed = play->state.input[0].press.button;
    u16 held = play->state.input[0].cur.button;
    u8 aPress = CHECK_BTN_ALL(pressed, BTN_A);
    u8 aHeld = CHECK_BTN_ALL(held, BTN_A);
    u8 rPress = CHECK_BTN_ALL(pressed, BTN_R);
    u8 rHeld = CHECK_BTN_ALL(held, BTN_R);
    u8 bPress = CHECK_BTN_ALL(pressed, BTN_B);
    u8 lPress = CHECK_BTN_ALL(pressed, BTN_L);

    f32 stickMag;
    s16 stickAngle;
    func_80077D10(&stickMag, &stickAngle, &play->state.input[0]);
    s16 camYaw = Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
    // func_80077D10's angle is camera-relative; z_player.c:2228 adds the
    // camera yaw to get a world direction, and so must we.
    s16 stickWorldYaw = camYaw + stickAngle;

    // ─── Damage reaction ────────────────────────────────────────────────
    // A struck cucco throws its wings wide and screams — the single most
    // recognisable thing the bird does. Edge-detect on invincibilityTimer,
    // which Player sets the frame a hit lands.
    // The sCuccoSelfIFrames guard matters: the spin and the shield block both
    // grant intangibility on purpose, and without it our own i-frames would
    // read as a fresh hit and fire the panic squawk every single time.
    if (player->invincibilityTimer > 0 && sCuccoPrevInvinc <= 0 && sCuccoSelfIFrames <= 0) {
        sCuccoHurtTimer = CUCCO_HURT_FRAMES;
        sCuccoPoundPhase = 0; // getting hit cancels a slam in progress
        Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_M);
        Sw97_CuccoBurst(play, &player->actor.world.pos, 12, 26.0f, 1.8f);
    }
    sCuccoPrevInvinc = player->invincibilityTimer;
    if (sCuccoHurtTimer > 0)
        sCuccoHurtTimer--;
    if (sCuccoFlapPose > 0)
        sCuccoFlapPose--;
    if (sCuccoSelfIFrames > 0)
        sCuccoSelfIFrames--;

    // ─── Egg aim mode owns the frame while it is up ─────────────────────
    // Checked before everything else so R and B belong to the mirilla and
    // cannot also trigger the shield, the pound or the spin.
    if (Sw97_TickCuccoEggAim(play, player, rHeld, rPress, lPress, bPress)) {
        Sw97_CuccoAnimateWingHead(NIW_ANIM_HEAD_PECKING);
        sCuccoSkel.playSpeed = 1.0f;
        SkelAnime_Update(&sCuccoSkel);
        Sw97_TickCuccoFlock(play, player);
        if (gSw97CuccoFlipTimer > 0)
            gSw97CuccoFlipTimer--;
        return;
    }
    if (rHeld && bPress) {
        Sw97_CuccoEnterEggAim(play, player);
        return;
    }

    // ─── Shield (R held on the ground) ──────────────────────────────────
    Sw97_TickCuccoShield(play, player, grounded, rHeld);

    // ─── Aerial spin (B in the air) — the boomerang replacement ─────────
    if (sCuccoSpinTimer == 0 && bPress && !grounded && sCuccoPoundPhase == 0) {
        sCuccoSpinTimer = CUCCO_SPIN_FRAMES;
        sCuccoSpinYaw = 0.0f;
        player->actor.velocity.y = CUCCO_SPIN_LIFT;
        // Positive timer = true intangibility: z_player.c:13680 skips
        // CollisionCheck_SetAC entirely, so the cucco passes through enemies
        // instead of merely ignoring their damage.
        Player_SetIntangibility(player, CUCCO_SPIN_FRAMES + 6);
        sCuccoSelfIFrames = CUCCO_SPIN_FRAMES + 6;
        Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_A);
        Sw97_CuccoBurst(play, &player->actor.world.pos, 8, 24.0f, 1.2f);
        bPress = 0; // consumed
    }

    // ─── Wing Whack (B on the ground) ───────────────────────────────────
    if (sCuccoWhackTimer == 0 && bPress && grounded && !sCuccoShieldUp && sCuccoLegSpringTimer == 0) {
        sCuccoWhackTimer = CUCCO_WHACK_FRAMES;
        sCuccoWhackMoving = (fabsf(player->linearVelocity) > 1.5f);
        Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_A);
    }

    // ─── Leg Spring (R + A on the ground) ───────────────────────────────
    // Tested before the plain-A flap below so the combo wins the press.
    if (sCuccoLegSpringTimer == 0 && grounded && rHeld && aPress && !sCuccoAimActive) {
        sCuccoLegSpringTimer = CUCCO_LEGSPRING_CROUCH;
        aPress = 0; // consumed — no takeoff hop this frame
        Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_N);
    }

    // ─── R alone in the air: pound ──────────────────────────────────────
    // Ground R is the shield now; the flock is earned by blocking, not bound.
    if (sCuccoPoundPhase == 0 && rPress && !grounded && sCuccoSpinTimer == 0) {
        sCuccoPoundPhase = 1;
        sCuccoPoundTimer = CUCCO_POUND_HOVER;
        Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_A);
    }

    if (sCuccoPoundPhase == 1) {
        // Hover telegraph — hang still so the slam reads as deliberate.
        player->actor.velocity.y = 0.0f;
        player->linearVelocity = 0.0f;
        player->actor.speedXZ = 0.0f;
        if (--sCuccoPoundTimer <= 0) {
            sCuccoPoundPhase = 2;
        }
    } else if (sCuccoPoundPhase == 2) {
        player->actor.velocity.y = CUCCO_POUND_VY;
        player->linearVelocity = 0.0f;
        player->actor.speedXZ = 0.0f;
        if (grounded) {
            sCuccoPoundPhase = 0;
            Sw97_CuccoPoundImpact(play, player);
        }
    }

    // ─── Aerial spin: hover, damage, spin the model ─────────────────────
    if (sCuccoSpinTimer > 0) {
        if (player->actor.velocity.y < CUCCO_SPIN_VY) {
            player->actor.velocity.y = CUCCO_SPIN_VY;
        }
        // Air control is deliberately NOT zeroed (unlike the pound) — the
        // spin is a travel move as much as an attack.
        if ((sCuccoSpinTimer % CUCCO_SPIN_AT_CADENCE) == 0) {
            Sw97_CuccoMeleeRegister(play, player, CUCCO_SPIN_RADIUS, CUCCO_SPIN_DAMAGE);
        }
        sCuccoSpinYaw += (f32)CUCCO_SPIN_YAW_STEP * (M_PI / 32768.0f);
        sCuccoSpinTimer--;
        if (grounded) {
            sCuccoSpinTimer = 0;
        }
        if (sCuccoSpinTimer == 0) {
            sCuccoSpinYaw = 0.0f;
        }
    }

    // ─── Wing Whack: same collider, tighter radius, on the ground ───────
    if (sCuccoWhackTimer > 0) {
        if ((sCuccoWhackTimer % CUCCO_WHACK_AT_CADENCE) == 0) {
            Sw97_CuccoMeleeRegister(play, player, CUCCO_WHACK_RADIUS, CUCCO_WHACK_DAMAGE);
        }
        if (sCuccoWhackMoving) {
            // The moving variant is the body-spin one; reuse the spin yaw.
            sCuccoSpinYaw += (f32)CUCCO_SPIN_YAW_STEP * (M_PI / 32768.0f);
        }
        sCuccoWhackTimer--;
        if (sCuccoWhackTimer == 0 && !sCuccoSpinTimer) {
            sCuccoSpinYaw = 0.0f;
        }
    }

    // ─── Leg Spring: compress, then launch ──────────────────────────────
    if (sCuccoLegSpringTimer > 0) {
        player->linearVelocity = 0.0f;
        player->actor.speedXZ = 0.0f;
        if (--sCuccoLegSpringTimer == 0) {
            player->actor.velocity.y = CUCCO_LEGSPRING_VY;
            // The launch itself doesn't spend a flap — the wind-up is the cost.
            Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_A);
            Sw97_CuccoBurst(play, &player->actor.world.pos, 10, 20.0f, 2.0f);
        }
    }

    u8 pounding = (sCuccoPoundPhase != 0);
    u8 spinning = (sCuccoSpinTimer > 0);
    u8 springing = (sCuccoLegSpringTimer > 0);
    sCuccoGliding = 0;

    if (!pounding && !spinning && !springing) {
        // Cucco fall: clamp downward velocity to terminal float speed.
        // The engine added vanilla gravity (~-7) into velocity.y this frame;
        // clipping it to -3 makes Link float instead of plummet, without
        // touching `actor.gravity` (which gets stomped each frame by
        // Player_StepHorizontalSpeed @ z_player.c:7870 anyway). Skipped while
        // pounding or spinning — both override the descent on purpose.
        if (player->actor.velocity.y < CUCCO_MAX_VY_DOWN) {
            player->actor.velocity.y = CUCCO_MAX_VY_DOWN;
        }

        // Cucco speed: small horizontal boost over vanilla.
        player->linearVelocity *= CUCCO_SPEED_MULT;
        player->actor.speedXZ *= CUCCO_SPEED_MULT;
        if (player->linearVelocity > CUCCO_SPEED_MAX) {
            player->linearVelocity = CUCCO_SPEED_MAX;
        }
        if (player->actor.speedXZ > CUCCO_SPEED_MAX) {
            player->actor.speedXZ = CUCCO_SPEED_MAX;
        }

        // ─── Kirby flight: a finite flap budget ─────────────────────────
        // Refill happens ONLY on the ground, so altitude is something you
        // spend. Six flaps up, then you are committed to the glide.
        if (grounded) {
            sCuccoFlapsLeft = CUCCO_MAX_FLAPS;
        }

        if (aPress) {
            if (grounded) {
                // Takeoff hop — free. The budget just refilled this frame
                // anyway, so charging for it would be theatre.
                player->actor.velocity.y = CUCCO_FLAP_VELOCITY;
                sCuccoFlapPose = CUCCO_FLAP_POSE_FRAMES;
                Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_A);
            } else if (sCuccoFlapsLeft > 0) {
                sCuccoFlapsLeft--;
                // Assign, don't accumulate — six flaps lift a fixed amount
                // instead of compounding into orbit.
                player->actor.velocity.y = CUCCO_FLAP_VELOCITY;
                sCuccoFlapPose = CUCCO_FLAP_POSE_FRAMES;
                Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_A);
            } else {
                // Budget spent — a thin squawk with no lift, so the player
                // hears the limit instead of guessing at it.
                Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_N);
            }
        }

        // ─── Glide: hold A while descending ─────────────────────────────
        // Distinct from the default slow fall: descent drops to a third and
        // the stick drives real horizontal travel. Gated on velocity.y <= 0
        // so the press frame's own upward burst is never clipped.
        if (!grounded && aHeld && player->actor.velocity.y <= 0.0f) {
            sCuccoGliding = 1;
            if (player->actor.velocity.y < CUCCO_GLIDE_VY) {
                player->actor.velocity.y = CUCCO_GLIDE_VY;
            }
            if (stickMag > 10.0f) {
                player->actor.shape.rot.y = stickWorldYaw;
                player->linearVelocity += CUCCO_GLIDE_PUSH;
                if (player->linearVelocity > CUCCO_GLIDE_SPEED_MAX) {
                    player->linearVelocity = CUCCO_GLIDE_SPEED_MAX;
                }
                player->actor.speedXZ = player->linearVelocity;
            }
        }
    }

    f32 hSpeed = fabsf(player->linearVelocity);

    // Face the camera when idle. OOT keeps shape.rot.y frozen when Link stops
    // moving — the cucco would keep pointing at the last direction he walked.
    if (stickMag < 10.0f && hSpeed < 0.5f && !pounding && !springing && !sCuccoShieldUp) {
        Math_SmoothStepToS(&player->actor.shape.rot.y, camYaw, 4, 0x800, 0x100);
    }

    // ─── Pose: map Link's state onto the Cucco's own six poses ──────────
    // This is the layer that was missing. See the Cucco AI block above for
    // what each pose actually does to the wings.
    //
    // The Wing Whack de-phase flag rides along here: it turns the symmetric
    // wide-wing pose into alternating slashes without needing a seventh pose.
    sCuccoWhackDesync = (sCuccoWhackTimer > 0 && !sCuccoWhackMoving);

    s16 animIndex;
    if (springing) {
        animIndex = NIW_ANIM_FREEZE; // compressed, wings tucked
    } else if (sCuccoShieldUp) {
        animIndex = NIW_ANIM_FREEZE; // braced behind the wings
    } else if (spinning || sCuccoWhackTimer > 0) {
        animIndex = NIW_ANIM_PECKING_AND_WAVING; // wings out, whacking
    } else if (pounding) {
        animIndex = NIW_ANIM_FREEZE; // wings locked for the slam
    } else if (sCuccoHurtTimer > 0 || sCuccoFlapPose > 0) {
        animIndex = NIW_ANIM_PECKING_AND_WAVING; // panic / power flap: wings wide
    } else if (sCuccoGliding) {
        animIndex = NIW_ANIM_PECKING_AND_FORFLAPPING; // controlled: low steady flap
    } else if (!grounded) {
        // Free fall gets the wide flail, which is what vanilla uses for a
        // dropped cucco (z_en_niw.c:656, 717) — it reads as panic, and that
        // is exactly the contrast the glide needs to feel deliberate.
        animIndex = NIW_ANIM_PECKING_AND_WAVING;
    } else if (hSpeed > 8.0f) {
        animIndex = NIW_ANIM_PECKING_SLOW_FORFLAPPING; // running
    } else if (hSpeed > 1.5f) {
        animIndex = NIW_ANIM_HEAD_PECKING; // walking
    } else {
        animIndex = NIW_ANIM_STILL; // idle
    }

    // Idle head sweep — the real cucco alternates its head yaw between
    // ±5000 (D_80AB8604 @ z_en_niw.c:56) while standing around, and holds it
    // straight while it moves.
    // Driven off the frame counter rather than the mode timer, which no longer
    // ticks in the CVar form and would freeze the head mid-sweep.
    if (animIndex == NIW_ANIM_STILL) {
        if ((play->state.frames % 40) == 0) {
            sCuccoTgtHeadRotY = (sCuccoTgtHeadRotY > 0.0f) ? -5000.0f : 5000.0f;
        }
    } else {
        sCuccoTgtHeadRotY = 0.0f;
    }

    Sw97_CuccoAnimateWingHead(animIndex);

    // ─── Baked animation rate ───────────────────────────────────────────
    // gCuccoAnim still plays underneath the procedural layer; its speed sells
    // the leg/body cadence while the override does the wings.
    f32 rate;
    if (pounding || springing) {
        rate = 0.4f;
    } else if (spinning || sCuccoWhackTimer > 0) {
        rate = 4.0f;
    } else if (!grounded) {
        rate = (hSpeed > 1.5f) ? 4.0f : 2.5f;
    } else if (hSpeed > 8.0f) {
        rate = 2.0f;
    } else if (hSpeed > 1.5f) {
        rate = 1.4f;
    } else {
        rate = 0.7f;
    }
    sCuccoSkel.playSpeed = rate;
    SkelAnime_Update(&sCuccoSkel);

    // ─── Summoned flock ─────────────────────────────────────────────────
    Sw97_TickCuccoFlock(play, player);

    // Tick down the 180° flip timer used by Sw97_DrawCuccoModel on egg throws.
    if (gSw97CuccoFlipTimer > 0)
        gSw97CuccoFlipTimer--;
}
