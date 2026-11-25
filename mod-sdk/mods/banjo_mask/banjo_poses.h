#ifndef BANJO_POSES_H
#define BANJO_POSES_H

#include <stdbool.h>
#include <stdint.h>

#include "banjo_host.h"

typedef struct BanjoPoseFrame {
    uint16_t model;
    uint16_t animation;
    float frame;
    // Banjo's eye selector, 1 (open) to 8 (shut).
    uint8_t eyes;
    // BanjoAppendage flags.
    uint8_t appendages;
} BanjoPoseFrame;

// First and last frame of one of Banjo's clips; false when the clip cannot be loaded.
typedef bool (*BanjoClipRangeFunc)(uint16_t animation, float* outFirst, float* outLast);

// Where Banjo is in which clip this frame, following how the game poses Link.
void BanjoPoses_Follow(Player* player, BanjoClipRangeFunc getClipRange, BanjoPoseFrame* out);
void BanjoPoses_Reset(void);

#endif
