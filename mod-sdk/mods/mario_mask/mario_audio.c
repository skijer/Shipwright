#include "mario_audio.h"

#include <stdint.h>

#include "libsm64/libsm64.h"
#include "mario_config.h"

#define RING_FRAMES 8192
#define RING_TARGET_FRAMES 4096
#define TICK_MAX_FRAMES 544
#define TICK_CHUNKS 2
#define REFILL_MAX_TICKS 8

_Static_assert((RING_FRAMES & (RING_FRAMES - 1)) == 0, "the ring is indexed with a mask");

static int16_t sRing[RING_FRAMES * 2];
// One writer per cursor: the game thread moves the head, the audio thread the tail. Aligned 32-bit stores are
// atomic on every target, and a sample is written before the head that publishes it.
static volatile uint32_t sHead;
static volatile uint32_t sTail;

static uint32_t GetQueuedFrames(void) {
    return sHead - sTail;
}

static void Mix(int16_t* samples, uint32_t frameCount) {
    const uint32_t queued = GetQueuedFrames();
    const uint32_t mixed = (frameCount < queued) ? frameCount : queued;
    uint32_t tail = sTail;

    for (uint32_t frame = 0; frame < mixed; frame++, tail++) {
        const uint32_t at = (tail & (RING_FRAMES - 1)) * 2;

        samples[frame * 2] = sRing[at];
        samples[frame * 2 + 1] = sRing[at + 1];
    }
    sTail = tail;
}

void MarioAudio_Register(const SOHModApi* api) {
    api->RegisterAudioMixInGroup(Mix, SOH_AUDIO_GROUP_SFX, MARIO_VOLUME_CVAR);
}

void MarioAudio_Refill(void) {
    static int16_t chunk[TICK_MAX_FRAMES * TICK_CHUNKS * 2];

    for (int32_t tick = 0; tick < REFILL_MAX_TICKS && GetQueuedFrames() < RING_TARGET_FRAMES; tick++) {
        const uint32_t framesPerChunk = sm64_audio_tick(GetQueuedFrames(), RING_TARGET_FRAMES, chunk);
        const uint32_t space = RING_FRAMES - GetQueuedFrames();
        uint32_t frames = framesPerChunk * TICK_CHUNKS;
        uint32_t head = sHead;

        if (framesPerChunk == 0) {
            return;
        }
        if (frames > space) {
            frames = space;
        }
        for (uint32_t frame = 0; frame < frames; frame++, head++) {
            const uint32_t at = (head & (RING_FRAMES - 1)) * 2;

            sRing[at] = chunk[frame * 2];
            sRing[at + 1] = chunk[frame * 2 + 1];
        }
        sHead = head;
    }
}
