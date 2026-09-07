/**
 * stasis_sfx.inc.c — the Stasis rune's own sound (Skijer's NEI). Included from stasis_rune.c.
 *
 * A self-contained one-voice PCM player. It exists because neither engine has a "play this buffer"
 * API a mod can reach: soh's MmDirectAudio is the MM-sound emulator (and its PCM entry point is
 * file-static), and 2ship has no equivalent at all. Both DO have the same audio-thread seam though
 * — the spot where Sm64Audio_MixInto and friends are mixed on top of the synth output — so this
 * hangs off that and stays identical in both repos.
 *
 * Threading: StasisSfx_Play/Stop run on the game thread and only ever write `sSfx.wantPlay` and the
 * parameters; the audio thread owns `pos` and reads the rest. One writer each, single voice, no
 * lock — the worst a race can do is start the cue one buffer early or late.
 */

#include "stasis_sfx_pcm.inc.c"

// Both engines render at 32 kHz stereo.
#define STASIS_SFX_OUT_RATE 32000.0f

typedef struct {
    volatile u8 wantPlay; // game thread raises, audio thread consumes
    volatile u8 wantSeek; // ...same for a jump to the tail
    volatile u8 playing;
    volatile f32 rate;     // 1.0 = as recorded; 2.0 = double speed (enemies)
    volatile f32 volume;   // 0..1
    volatile f32 seekTail; // seconds of cue to leave ahead of the cursor
    f32 pos;               // audio-thread only: sample cursor into sStasisSfxPcm
} StasisSfxState;

static StasisSfxState sSfx = { 0 };

extern s32 CVarGetInteger(const char* name, s32 defaultValue);
extern PlayState* gPlayState;

// Settings → Audio. Read per buffer rather than cached, so a slider moved mid-cue is heard at once.
static f32 StasisSfx_SettingsGain(void) {
    f32 master = (f32)CVarGetInteger("gSettings.Volume.Master", 40) / 100.0f;
    f32 sfx = (f32)CVarGetInteger("gSettings.Volume.SFX", 100) / 100.0f;

    return master * sfx;
}

// True whenever Play_Update is frozen — the pause menu, and the rune wheel, which freezes it the
// same way. The cursor is left untouched, so the cue resumes where it stopped.
static u8 StasisSfx_IsGameFrozen(void) {
    return (gPlayState == NULL) || (gPlayState->pauseCtx.state != 0) || (gPlayState->pauseCtx.debugState != 0);
}

// Start the cue. `rate` is a playback multiplier, so the enemy variant is literally 2.0f.
void StasisSfx_Play(f32 rate, f32 volume) {
    sSfx.rate = rate;
    sSfx.volume = volume;
    sSfx.wantPlay = 1;
}

void StasisSfx_Stop(void) {
    sSfx.wantPlay = 0;
    sSfx.playing = 0;
}

/**
 * Jump the cursor so exactly `seconds` of cue remain — the release at the end of the recording.
 *
 * This exists for the early cancel: casting Stasis again on something already held skips the hold
 * and fires the launch there and then, and the sound has to skip WITH it. Cutting the cue off
 * instead would drop the one part of the recording that sells the release.
 */
void StasisSfx_SeekToTail(f32 seconds) {
    sSfx.seekTail = seconds;
    sSfx.wantSeek = 1;
}

/**
 * Audio-thread mixer. `outBuf` is interleaved stereo s16, `numSamples` is the number of STEREO
 * FRAMES — the same contract Sm64Audio_MixInto is called with right next to this.
 */
void StasisSfx_MixInto(s16* outBuf, u32 numSamples) {
    f32 advance;
    f32 vol;
    u32 i;

    if (sSfx.wantPlay) {
        sSfx.wantPlay = 0;
        sSfx.playing = 1;
        sSfx.pos = 0.0f;
    }
    if (sSfx.wantSeek) {
        f32 p = (f32)STASIS_SFX_SAMPLES - (sSfx.seekTail * (f32)STASIS_SFX_RATE);

        sSfx.wantSeek = 0;
        // Seeking a cue that already finished restarts it at the tail, so the release is heard even
        // when the hold ran long enough for the recording to have run out.
        sSfx.playing = 1;
        sSfx.pos = (p > 0.0f) ? p : 0.0f;
    }
    if (!sSfx.playing || (outBuf == NULL) || StasisSfx_IsGameFrozen()) {
        return;
    }

    advance = (sSfx.rate * (f32)STASIS_SFX_RATE) / STASIS_SFX_OUT_RATE;
    vol = sSfx.volume * StasisSfx_SettingsGain();

    for (i = 0; i < numSamples; i++) {
        s32 idx = (s32)sSfx.pos;
        s32 mixL;
        s32 mixR;
        s32 s;

        if (idx >= (STASIS_SFX_SAMPLES - 1)) {
            sSfx.playing = 0;
            return;
        }

        // Linear interpolation between neighbouring samples: at 2x rate we skip every other one,
        // and without it the cue picks up an audible buzz.
        {
            f32 frac = sSfx.pos - (f32)idx;
            f32 a = (f32)sStasisSfxPcm[idx];
            f32 b = (f32)sStasisSfxPcm[idx + 1];

            s = (s32)((a + ((b - a) * frac)) * vol);
        }

        // Mixed on top of whatever the synth already wrote, clamped so a loud scene cannot wrap
        // around into noise.
        mixL = outBuf[(i * 2) + 0] + s;
        mixR = outBuf[(i * 2) + 1] + s;
        outBuf[(i * 2) + 0] = (s16)((mixL > 32767) ? 32767 : ((mixL < -32768) ? -32768 : mixL));
        outBuf[(i * 2) + 1] = (s16)((mixR > 32767) ? 32767 : ((mixR < -32768) ? -32768 : mixR));

        sSfx.pos += advance;
    }
}
