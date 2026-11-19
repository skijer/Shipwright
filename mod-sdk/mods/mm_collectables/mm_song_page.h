#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t size;
    const char* key;
    uint8_t noteCount;
    uint8_t notes[8];
    const OcarinaNote* playback;
} NeiMmSong;

typedef bool (*NeiAddMmSong)(const NeiMmSong* song);
