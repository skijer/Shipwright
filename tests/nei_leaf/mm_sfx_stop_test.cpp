// Exercise the real MM bank engine, including its process-start storage.
#include "mods/sound_translator/mm_audio_sfx.h"
#include "mods/sound_translator/mm_sfx_ids.h"
#include <cassert>
#include <cstdio>

int main(int argc, char**) {
    if (argc > 1) {
        AudioMmSfx_Reset();
    }
    std::puts(argc > 1 ? "Stopping Leaf wind after engine initialization" :
                        "Stopping Leaf wind before any MM sound has played");
    std::fflush(stdout);
    AudioMmSfx_StopById(MM_NA_SE_IT_DEKUNUTS_FLOWER_ROLL);
    std::puts("PASS: stop returned");

    AudioMmSfx_Reset();
    AudioMmSfx_ProcessRequests(); // Finish the previous stop-guard cycle.
    constexpr u16 wind = MM_NA_SE_IT_DEKUNUTS_FLOWER_ROLL;
    constexpr u16 other = MM_NA_SE_IT_DEKUNUTS_FLOWER_CLOSE;
    auto queue = [](u16 id) {
        AudioMmSfx_PlaySfx(id, &gMmSfxDefaultPos, 4, &gMmSfxDefaultFreqAndVolScale,
                          &gMmSfxDefaultFreqAndVolScale, &gMmSfxDefaultReverb);
    };
    queue(wind);
    queue(other);
    AudioMmSfx_ProcessRequests();
    assert(AudioMmSfx_IsPlaying(wind));
    assert(AudioMmSfx_IsPlaying(other));
    AudioMmSfx_StopById(wind);
    assert(!AudioMmSfx_IsPlaying(wind));
    assert(AudioMmSfx_IsPlaying(other));

    // A same-cycle stale request must remain suppressed; a later use must work.
    queue(wind);
    AudioMmSfx_ProcessRequests();
    assert(!AudioMmSfx_IsPlaying(wind));
    queue(wind);
    AudioMmSfx_ProcessRequests();
    assert(AudioMmSfx_IsPlaying(wind));

    // Stop must still purge pending requests before they become bank entries.
    AudioMmSfx_StopById(wind);
    AudioMmSfx_ProcessRequests();
    queue(wind);
    AudioMmSfx_StopById(wind);
    AudioMmSfx_ProcessRequests();
    assert(!AudioMmSfx_IsPlaying(wind));
    assert(AudioMmSfx_IsPlaying(other));
    std::puts("PASS: initialized stop, unrelated sound, stale request, later replay, queued stop");
}
