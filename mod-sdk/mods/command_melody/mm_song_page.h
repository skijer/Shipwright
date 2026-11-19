#pragma once
#include <stdbool.h>
#include <stdint.h>

// Publish/FindService("nei.mm_collectables.add_song") returns NeiAddMmSong. A song added there gets the MM
// quest page's staff preview and A-to-play; its owner keeps the item, recognition and effect.
typedef struct {
    uint32_t size;
    const char* key;
    uint8_t noteCount;
    uint8_t notes[8];
    const OcarinaNote* playback; // Optional; ends with a zero-length note. NULL plays the notes evenly.
} NeiMmSong;

typedef bool (*NeiAddMmSong)(const NeiMmSong* song);
