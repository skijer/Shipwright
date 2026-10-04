// Exercise the actual engine structures, not a narrowed test copy. The N64 OS
// types below are opaque dependencies of other structures in z64audio.h; this
// test neither instantiates nor uses the audio context/OS-task layouts.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "test_require.h"
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef float f32;
typedef double f64;
typedef void* OSMesg;
typedef struct {
    int unused;
} OSMesgQueue;
typedef struct {
    int unused;
} OSIoMesg;
typedef struct {
    int unused;
} OSTask;
typedef struct {
    int unused;
} OSPiHandle;
typedef struct {
    int unused;
} Acmd;
#include "z64audio.h"

int main(void) {
    // This is a storage regression test, not an end-to-end loader/cache test.
    // Actual loader/cache propagation is covered by audio_stream_runtime_test.
    const int fontIds[] = { 0, 1, 254, 256, 349, 511, 512, 1024 };
    for (size_t i = 0; i < sizeof(fontIds) / sizeof(fontIds[0]); ++i) {
        SequencePlayer player = { 0 };
        SequenceChannel channel = { 0 };
        NotePlaybackState note = { 0 };
        AudioSlowLoad pending = { 0 };
        SampleCacheEntry sampleCache = { 0 };
        // Mirror assignments used by the loader, channel init and Audio_InitNote.
        player.defaultFont = fontIds[i];
        channel.fontId = player.defaultFont;
        note.fontId = channel.fontId;
        pending.seqOrFontId = channel.fontId;
        sampleCache.sampleBankId = channel.fontId;
        REQUIRE(player.defaultFont == fontIds[i]);
        REQUIRE(channel.fontId == fontIds[i]);
        REQUIRE(note.fontId == fontIds[i]);
        REQUIRE(pending.seqOrFontId == fontIds[i]);
        REQUIRE(sampleCache.sampleBankId == fontIds[i]);
    }
}
