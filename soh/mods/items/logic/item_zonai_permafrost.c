/**
 * item_zonai_permafrost.c - Zonai Permafrost (time freeze TOGGLE)
 *
 * Controls:
 *   C Button: toggle the time freeze on / off
 *
 * Features:
 *   - Freezes all actors and the day/night cycle the instant you press the button
 *   - No cast animation and no fixed duration: it stays on until you press again
 *     or the magic meter runs dry
 *   - Link moves and fights freely during the effect
 *   - Green Zonai energy runes visual
 *   - Deku Nut style white flash on every toggle, plus an ice-green screen wash
 *     held for as long as the world is stopped
 *
 * The freeze itself is delegated to timestop_helper (TIMECTL_OWNER_PERMAFROST,
 * the HIGHEST priority claim): a hard stop must win over Champion's Tunic bullet
 * time and over the Phantom Hourglass' rewind scrub. The helper re-applies the
 * freeze every frame from CustomItems_Update, so actors that spawn mid-effect are
 * caught too, it keeps frozen enemies hittable, and it restores the day/night
 * clock on release or scene change.
 *
 * Nothing here touches health, rupees or any flag, and it only ever spends its own
 * magic: damage dealt while the world is stopped, purchases made, and scene flags
 * set all persist normally.
 */

#include "z64.h"
#include "item_zonai_permafrost.h"
#include "../custom_items.h"
#include "../helpers/equip_helper.h"
#include "../helpers/fx_helper.h"
#include "../helpers/timestop_helper.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

// ============================================================================
// Audio
//
// Every cue this item plays is emitted from its OWN position vector rather than
// straight from Link's. The ice sounds are sustained samples and the ambient one
// re-triggers while the freeze is held, so they have to be killable in one call
// when the freeze ends — and stopping by Link's own position pointer would take
// his footsteps and everything else he is emitting down with them.
// ============================================================================

static Vec3f sZPermSfxPos;

static void ZPerm_Sfx(Player* p, u16 sfxId) {
    sZPermSfxPos = p->actor.world.pos;
    // Every cue here is fired ONCE, so the continuous flag has to come off. Bit 0x800
    // marks an sfx the caller re-requests every frame; the sound bank only ages those
    // while they sit in SFX_STATE_QUEUED, and its auto-reclaim path is explicitly gated
    // on !(sfxId & 0xC00). Fire one with the flag on and never ask again and it parks in
    // PLAYING forever — which is exactly why the ice sounds outlived the freeze. The
    // sample is chosen by sfxId & 0x1FF, so clearing 0x800 changes the lifetime, never
    // which sound you hear. (NA_SE_EV_ICE_FREEZE is 0x28B2, NA_SE_EV_ICE_MELT 0x28A2 —
    // both carry it.)
    Audio_PlaySoundGeneral(sfxId - SFX_FLAG, &sZPermSfxPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

/** Kill every cue this item still has in flight. */
static void ZPerm_SfxStopAll(void) {
    Audio_StopSfxByPos(&sZPermSfxPos);
}

// ----------------------------------------------------------------------------
// Toggle chime, forwards and "backwards"
//
// Both toggles use the pause screen's ice-arrow cue (the one z_kaleido_item.c
// fires as NA_SE_SY_SET_FIRE_ARROW + 1 when you drop Ice onto the bow), played
// exactly the way the menu plays it: from gSfxDefaultPos, so it is flat 2D and
// unattenuated. That also puts it out of reach of ZPerm_SfxStopAll, which only
// kills what is ringing at our own position vector.
//
// The release cue is the same chime running DOWN. A sample cannot literally be
// played backwards here: the sfx player walks PCM forwards only, and a true
// reversal would mean shipping a second, mirrored sample as a custom asset.
// What it does honour is a live pitch — it keeps the f32* it was handed and
// dereferences it every audio frame (`* entry->freqScale`, sfx bank processing
// in code_800EC960.c) — so pointing it at a value we walk downwards bends the
// cue while it is still ringing. On a short rising chime that glide is what the
// ear reads as the sound running backwards.
//
// The walk is ticked from Handle_ZonaiPermafrost, which runs every frame while
// the item is equipped. Ending the freeze by UNEQUIPPING is the one case that
// stops the tick; the cue then simply rings out at its starting pitch.
// ----------------------------------------------------------------------------

#define ZPERM_REV_FREQ_START 1.45f
#define ZPERM_REV_FREQ_END 0.55f
#define ZPERM_REV_FREQ_STEP 0.11f // start to end in ~8 frames (~0.4 s at 20 fps)

static f32 sZPermRevFreq = ZPERM_REV_FREQ_END;

/** The freeze snapping on: the chime at its normal pitch. */
static void ZPerm_PlayEntryCue(void) {
    Audio_PlaySoundGeneral(NA_SE_SY_SET_ICE_ARROW, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

/** The freeze letting go: same chime, pitch pointer we are about to drag down. */
static void ZPerm_PlayReleaseCue(void) {
    sZPermRevFreq = ZPERM_REV_FREQ_START;
    Audio_PlaySoundGeneral(NA_SE_SY_SET_ICE_ARROW, &gSfxDefaultPos, 4, &sZPermRevFreq, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

/** Bend the release cue down one step. Cheap no-op once it has bottomed out. */
static void ZPerm_TickReleaseCue(void) {
    if (sZPermRevFreq > ZPERM_REV_FREQ_END) {
        sZPermRevFreq -= ZPERM_REV_FREQ_STEP;

        if (sZPermRevFreq < ZPERM_REV_FREQ_END) {
            sZPermRevFreq = ZPERM_REV_FREQ_END;
        }
    }
}

// ============================================================================
// Screen
//
// Two separate channels, deliberately:
//
//   * The toggle FLASH is the game's own Deku Nut white-out. Writing a negative
//     value into the transition-fade flash register snaps the fade to full alpha
//     and z_fbdemo_fade runs it back down on its own (255 -> 0 over ~11 frames).
//     It is ticked by Play_Update, NOT by this item, so it always finishes even
//     if the freeze is dropped, the item is unequipped or Link is pulled into a
//     cutscene on the very next frame. Exactly what the thrown nut writes in
//     z_en_arrow.c, so it is the same flash, not a lookalike.
//
//   * The HELD tint is envCtx.fillScreen, the channel the Champion's Tunic
//     already uses for its bullet-time wash. That one is plain state: it stays
//     up until somebody clears it, so it is re-written every frame the freeze is
//     held and cleared explicitly on release. That also makes it self-healing —
//     anything that stomps it gets it back on the next frame — and a scene load
//     clears it for free, since Environment_Init zeroes fillScreen.
// ============================================================================

// Pale Zonai ice-green. Kept weak on purpose: this sits on screen for as long as
// the meter lasts, not for the handful of frames a hit flash does.
#define ZPERM_TINT_R 120
#define ZPERM_TINT_G 230
#define ZPERM_TINT_B 210
#define ZPERM_TINT_ALPHA 26 // base strength
#define ZPERM_TINT_PULSE 8  // breathes +/- this much, ~2.7 s per cycle at 20 fps

/** Deku Nut white-out. One call and the engine runs the whole fade. */
static void ZPerm_Flash(void) {
    // OoT leaves this register unnamed; MM calls it R_TRANS_FADE_FLASH_ALPHA_STEP.
    // Negative starts the flash — see z_en_arrow.c, ARROW_NUT impact.
    iREG(50) = -1;
}

/** Ice-green wash held while the world is stopped. alpha 0 clears it. */
static void ZPerm_SetScreenTint(PlayState* play, u8 alpha) {
    if (alpha == 0) {
        play->envCtx.fillScreen = false;
        play->envCtx.screenFillColor[3] = 0;
        return;
    }

    play->envCtx.fillScreen = true;
    play->envCtx.screenFillColor[0] = ZPERM_TINT_R;
    play->envCtx.screenFillColor[1] = ZPERM_TINT_G;
    play->envCtx.screenFillColor[2] = ZPERM_TINT_B;
    play->envCtx.screenFillColor[3] = alpha;
}

// ============================================================================
// Visual Effects
// ============================================================================

/**
 * A ring of 8 green rune particles at the given radius. With the cast animation
 * gone this is no longer a slow expanding wind-up; it is stacked into a one-shot
 * flourish (see ZPerm_SpawnBurst) on toggle.
 */
static void ZPerm_SpawnRuneRing(Player* p, PlayState* play, f32 expandRadius) {
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 primColor = { 100, 255, 150, 255 }; // Bright Zonai green
    Color_RGBA8 envColor = { 0, 200, 80, 255 };     // Deep green

    for (u8 i = 0; i < 8; i++) {
        // Integer BAM step on purpose. The angle used to be built as a float
        // (i * 65536/8) and then cast to s16, which is out of s16 range from i=4
        // on — an undefined float-to-int conversion that wraps on x86 but
        // SATURATES to 0x7FFF on arm64, collapsing half the ring onto one point.
        s16 angleS = (s16)(i * (0x10000 / 8));

        Vec3f pos;
        pos.x = p->actor.world.pos.x + Math_SinS(angleS) * expandRadius;
        pos.y = p->actor.world.pos.y + 30.0f + Rand_CenteredFloat(20.0f);
        pos.z = p->actor.world.pos.z + Math_CosS(angleS) * expandRadius;

        Vec3f vel;
        vel.x = Math_SinS(angleS) * 3.0f;
        vel.y = Rand_ZeroFloat(1.5f);
        vel.z = Math_CosS(angleS) * 3.0f;

        EffectSsKiraKira_SpawnFocused(play, &pos, &vel, &accel, &primColor, &envColor, 600, 20);
    }
}

/** Toggle flourish: three concentric rings of runes in a single frame. */
static void ZPerm_SpawnBurst(Player* p, PlayState* play) {
    ZPerm_SpawnRuneRing(p, play, 40.0f);
    ZPerm_SpawnRuneRing(p, play, 110.0f);
    ZPerm_SpawnRuneRing(p, play, 180.0f);
}

/**
 * Green particles hanging motionless in the air while the freeze is held.
 * 3 per frame in a wide box around Link. Held for 15 frames each, so the field
 * is ~45 live particles — dense enough to read as "the air itself is stopped"
 * without starving the shared EffectSs pool that Link's own hits still need.
 */
static void ZPerm_SpawnFrozenParticles(Player* p, PlayState* play) {
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Vec3f vel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 primColor = { 120, 255, 160, 200 };
    Color_RGBA8 envColor = { 0, 180, 60, 150 };

    for (u8 i = 0; i < 3; i++) {
        Vec3f pos;
        pos.x = p->actor.world.pos.x + Rand_CenteredFloat(360.0f);
        pos.y = p->actor.world.pos.y + 20.0f + Rand_ZeroFloat(130.0f);
        pos.z = p->actor.world.pos.z + Rand_CenteredFloat(360.0f);

        EffectSsKiraKira_SpawnFocused(play, &pos, &vel, &accel, &primColor, &envColor, 460, 15);
    }
}

// ============================================================================
// Toggle On / Off
// ============================================================================

static void ZPerm_Stop(Player* p, PlayState* play) {
    if (!zpActive) {
        return;
    }

    // Release the world: actors resume on the next frame and the day/night clock
    // goes back to the speed it had before the freeze.
    TimeCtl_Release(TIMECTL_OWNER_PERMAFROST);

    ZPerm_SpawnBurst(p, play);
    // Same white-out as switching on: the toggle reads as one event in both
    // directions, and it covers the frame where the world snaps back to motion.
    ZPerm_Flash();
    ZPerm_SetScreenTint(play, 0);
    Rumble_Request(200.0f, 100, 15, 40);
    // Kill the activation cue and every ambient tick still ringing BEFORE starting the
    // release one-shot, or the stop would swallow the cue we just started.
    ZPerm_SfxStopAll();
    ZPerm_Sfx(p, NA_SE_EV_ICE_MELT);
    ZPerm_PlayReleaseCue();

    zpActive = 0;
    zpState = ZPERM_STATE_IDLE;
    zpSubPhase = 0;
    zpTimer = 0;
    zpSavedTime = 0;
}

static void ZPerm_Start(Player* p, PlayState* play) {
    if (zpActive) {
        return;
    }

    if (!ItemMagic_HasEnough(play, ZPERM_MAGIC_ACTIVATION)) {
        Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }

    ItemMagic_Consume(play, ZPERM_MAGIC_ACTIVATION);

    // Straight to ACTIVE — there is no cast state any more. The old version locked
    // Link into a three-part Din's Fire animation first, which is exactly what the
    // toggle is meant to remove. Because nothing poses Link now, the freeze also
    // works in mid-air and in water, where the animation used to forbid it.
    zpActive = 1;
    zpState = ZPERM_STATE_ACTIVE;
    zpSubPhase = 0;
    zpTimer = 0;

    TimeCtl_Request(TIMECTL_OWNER_PERMAFROST, 0.0f, 1);

    ZPerm_SpawnBurst(p, play);
    ZPerm_Flash();
    ZPerm_SetScreenTint(play, ZPERM_TINT_ALPHA);
    Rumble_Request(300.0f, 150, 20, 60);
    ZPerm_SfxStopAll(); // clear anything left over from a previous toggle
    ZPerm_Sfx(p, NA_SE_EV_ICE_FREEZE);
    // The kaleido chime instead of a Link grunt: the freeze is a menu-like state
    // change, not an effort, and a voice clip made him sound like he was casting
    // something he is not.
    ZPerm_PlayEntryCue();
}

// ============================================================================
// Upkeep while the freeze is held
// ============================================================================

static void ZPerm_StateActive(Player* p, PlayState* play) {
    u8 runningLow;

    // Defensive: nothing should be posing Link, but make sure he stays free to act.
    p->stateFlags1 &= ~(PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_INPUT_DISABLED);

    // The claim is re-applied to every actor each frame by TimeCtl_Update
    // (CustomItems_Update), which also catches actors that spawn mid-effect.

    // Elapsed counter, visuals only. Clamped so it cannot wrap the s16.
    if (zpTimer < 0x7000) {
        zpTimer++;
    }

    // Drain. Testing BEFORE spending means the freeze switches itself off on the
    // frame the meter can no longer pay, instead of going negative.
    // Ticked off zpTimer, not play->gameplayFrames: the counter resets on every
    // activation, so the first drain always lands a full interval after you switch
    // on rather than on whatever phase the global frame counter happened to be at.
    if ((zpTimer % ZPERM_DRAIN_INTERVAL) == 0) {
        if (!ItemMagic_HasEnough(play, ZPERM_DRAIN_COST)) {
            ZPerm_Stop(p, play);
            return;
        }
        ItemMagic_Consume(play, ZPERM_DRAIN_COST);
    }

    // Ambient frozen particles and the screen wash; both flicker once the meter is
    // nearly out, which is the player's warning that time is about to start moving
    // again. The tint is re-written every frame rather than set once on activation:
    // fillScreen is shared state (the Champion's Tunic and the finishing-blow flash
    // write it too), so owning it per frame is what keeps it from being stolen.
    runningLow = !ItemMagic_HasEnough(play, ZPERM_FLICKER_MAGIC);
    if (!runningLow || ((play->gameplayFrames % 4) >= 2)) {
        ZPerm_SpawnFrozenParticles(p, play);
        ZPerm_SetScreenTint(play, (u8)(ZPERM_TINT_ALPHA + (s32)(Math_SinS((s16)(zpTimer * 1200)) * ZPERM_TINT_PULSE)));
    } else {
        ZPerm_SetScreenTint(play, 0);
    }

    // Ambient SFX
    if ((play->gameplayFrames % 40) == 0) {
        ZPerm_Sfx(p, NA_SE_EV_ICE_MELT);
    }
}

// ============================================================================
// Main Handler
// ============================================================================

void Handle_ZonaiPermafrost(Player* p, PlayState* play) {
    ItemInputState in;

    // Before every early return below: the release chime is still bending down
    // while the freeze itself is already gone.
    ZPerm_TickReleaseCue();

    ItemInput_Update(&in, ITEM_ZONAI_PERMAFROST, p, play);

    // Unequipped: drop the freeze rather than stranding the world stopped.
    if (!in.wasEquipped) {
        if (zpActive) {
            ZPerm_Stop(p, play);
        }
        return;
    }

    if (zpActive) {
        // Toggle OFF. Handled before the upkeep so the press that ends the freeze
        // does not also pay a drain tick on its way out.
        if (in.isPressed) {
            ZPerm_Stop(p, play);
            return;
        }
        // No IsBlocked check here on purpose: once time is stopped the freeze must
        // survive whatever Link gets up to. Ending it is the player's call, or the
        // magic meter's.
        ZPerm_StateActive(p, play);
        return;
    }

    if (ItemInput_IsBlocked(p, play)) {
        return;
    }
    if (in.isPressed) {
        ZPerm_Start(p, play);
    }
}

// ============================================================================
// Stubs
// ============================================================================

void Player_InitZonaiPermafrostIA(PlayState* play, Player* p) {
    ZPerm_SetScreenTint(play, 0);
    zpActive = 0;
    zpState = ZPERM_STATE_IDLE;
    zpSubPhase = 0;
    zpTimer = 0;
    zpSavedTime = 0;
}

s32 Player_UpperAction_ZonaiPermafrost(Player* p, PlayState* play) {
    return 0;
}
