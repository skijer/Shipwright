/**
 * hourglass_sfx.inc.c — the Phantom Hourglass Recall cues (Skijer's NEI). From the item file.
 *
 * Two voices: one for the one-shots (activate / target selected / exit) and one for the rewinding
 * bed, which loops. Built on the same audio-thread seam as stasis_sfx.inc.c because neither engine
 * gives a mod a "play this buffer" API, and identical in both repos.
 *
 * It respects the player's audio settings, which the Stasis cue does not: Master and SFX volume are
 * sampled on the game thread (CVars are not for the audio thread to touch) and handed over as one
 * float. Silence also comes for free from HourglassSfx_Tick not being called — see it.
 *
 * Threading: the game thread only ever raises the `want*` flags and writes parameters; the audio
 * thread owns the cursors. One writer each, two voices, no lock — the worst a race can do is start
 * or stop a cue one buffer early.
 */

#include "hourglass_sfx_pcm.inc.c"

// Both engines render at 32 kHz stereo, and the cues are 16 kHz: one source sample per two output
// frames, interpolated, exactly as the Stasis cue does it.
#define HOURGLASS_SFX_OUT_RATE 32000.0f
// The bed fades rather than cutting when the recall ends, so releasing does not click.
#define HOURGLASS_SFX_FADE_STEP 0.002f
/**
 * MIXER CALLS — not game frames — of silence tolerated before everything is shut up.
 *
 * The game thread refreshes this every gameplay frame. Anything that stops gameplay — the pause
 * menu, a scene load, the window losing focus, the console — stops the refresh too, so the loop
 * goes quiet on its own instead of droning under a paused game, and item code can never leave the
 * bed playing forever.
 *
 * The count must cover the RATIO between the two: gameplay runs at 20 Hz and the audio thread asks
 * for a buffer several times more often than that, so anything near 3 runs out between two frames
 * and chops the sound to pieces. This is roughly half a second of slack.
 */
#define HOURGLASS_SFX_ALIVE_CALLS 30

typedef enum {
    HOURGLASS_CUE_ACTIVATE,
    HOURGLASS_CUE_TARGET,
    HOURGLASS_CUE_EXIT,
} HourglassCue;

typedef struct {
    volatile u8 wantCue; // game thread raises, audio thread consumes
    volatile u8 cue;     // which one-shot
    volatile u8 loopWant;
    volatile f32 volume; // Master x SFX, sampled on the game thread
    volatile s16 alive;  // HourglassSfx_Tick refreshes; the mixer counts it down
    f32 cuePos;          // audio thread only, from here down
    f32 cueEnd;
    u8 cuePlaying;
    f32 loopPos;
    f32 loopGain;
} HourglassSfxState;

static HourglassSfxState sHgSfx = { 0 };

static const s32 sHourglassCueStart[] = {
    HOURGLASS_SFX_ACTIVATE_START,
    HOURGLASS_SFX_TARGET_START,
    HOURGLASS_SFX_EXIT_START,
};

static const s32 sHourglassCueLen[] = {
    HOURGLASS_SFX_ACTIVATE_LEN,
    HOURGLASS_SFX_TARGET_LEN,
    HOURGLASS_SFX_EXIT_LEN,
};

void HourglassSfx_PlayCue(HourglassCue cue) {
    sHgSfx.cue = (u8)cue;
    sHgSfx.wantCue = 1;
}

void HourglassSfx_StartLoop(void) {
    sHgSfx.loopWant = 1;
}

void HourglassSfx_StopLoop(void) {
    sHgSfx.loopWant = 0;
}

/**
 * Per-frame, from the item's own tick. Publishes the player's volume and says "gameplay is still
 * running" — both things the audio thread must not go and read for itself.
 */
void HourglassSfx_Tick(void) {
    f32 master = (f32)CVarGetInteger(CVAR_SETTING("Volume.Master"), 100) / 100.0f;
    f32 sfx = (f32)CVarGetInteger(CVAR_SETTING("Volume.SFX"), 100) / 100.0f;

    sHgSfx.volume = master * sfx;
    sHgSfx.alive = HOURGLASS_SFX_ALIVE_CALLS;
}

/** Linear interpolation between neighbouring source samples; `pos` is fractional. */
static f32 Hourglass_SampleAt(f32 pos, s32 wrapStart, s32 wrapLen) {
    s32 idx = (s32)pos;
    f32 frac = pos - (f32)idx;
    s32 next = idx + 1;
    f32 a;
    f32 b;

    if ((wrapLen > 0) && (next >= (wrapStart + wrapLen))) {
        next = wrapStart; // the crop is a real loop: its tail was faded into its own head
    }
    a = (f32)sHourglassSfxPcm[idx];
    b = (f32)sHourglassSfxPcm[next];
    return a + ((b - a) * frac);
}

static void Hourglass_AddSample(s16* outBuf, u32 i, s32 s) {
    s32 mixL = outBuf[(i * 2) + 0] + s;
    s32 mixR = outBuf[(i * 2) + 1] + s;

    outBuf[(i * 2) + 0] = (s16)((mixL > 32767) ? 32767 : ((mixL < -32768) ? -32768 : mixL));
    outBuf[(i * 2) + 1] = (s16)((mixR > 32767) ? 32767 : ((mixR < -32768) ? -32768 : mixR));
}

/**
 * Audio-thread mixer. `outBuf` is interleaved stereo s16 and `numSamples` counts STEREO FRAMES —
 * the same contract StasisSfx_MixInto is called with right next to this.
 */
void HourglassSfx_MixInto(s16* outBuf, u32 numSamples) {
    f32 advance = (f32)HOURGLASS_SFX_RATE / HOURGLASS_SFX_OUT_RATE;
    f32 vol;
    u32 i;

    if (sHgSfx.alive > 0) {
        sHgSfx.alive--;
    } else {
        sHgSfx.cuePlaying = 0;
        sHgSfx.loopGain = 0.0f;
        sHgSfx.loopPos = 0.0f;
        return;
    }
    if (sHgSfx.wantCue) {
        s32 which = sHgSfx.cue;

        sHgSfx.wantCue = 0;
        sHgSfx.cuePos = (f32)sHourglassCueStart[which];
        sHgSfx.cueEnd = (f32)(sHourglassCueStart[which] + sHourglassCueLen[which] - 1);
        sHgSfx.cuePlaying = 1;
    }
    if (outBuf == NULL) {
        return;
    }

    vol = sHgSfx.volume;
    for (i = 0; i < numSamples; i++) {
        f32 mix = 0.0f;

        if (sHgSfx.cuePlaying) {
            if (sHgSfx.cuePos >= sHgSfx.cueEnd) {
                sHgSfx.cuePlaying = 0;
            } else {
                mix += Hourglass_SampleAt(sHgSfx.cuePos, 0, 0);
                sHgSfx.cuePos += advance;
            }
        }

        if (sHgSfx.loopWant) {
            if (sHgSfx.loopGain < 1.0f) {
                sHgSfx.loopGain += HOURGLASS_SFX_FADE_STEP;
            }
        } else if (sHgSfx.loopGain > 0.0f) {
            sHgSfx.loopGain -= HOURGLASS_SFX_FADE_STEP;
        }
        if (sHgSfx.loopGain > 0.0f) {
            mix += Hourglass_SampleAt(HOURGLASS_SFX_LOOP_START + sHgSfx.loopPos, HOURGLASS_SFX_LOOP_START,
                                      HOURGLASS_SFX_LOOP_LEN) *
                   sHgSfx.loopGain;
            sHgSfx.loopPos += advance;
            if (sHgSfx.loopPos >= (f32)HOURGLASS_SFX_LOOP_LEN) {
                sHgSfx.loopPos -= (f32)HOURGLASS_SFX_LOOP_LEN;
            }
        } else {
            sHgSfx.loopGain = 0.0f;
            sHgSfx.loopPos = 0.0f;
        }

        if (mix != 0.0f) {
            Hourglass_AddSample(outBuf, i, (s32)(mix * vol));
        }
    }
}
