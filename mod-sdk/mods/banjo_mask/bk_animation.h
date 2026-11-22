#ifndef BK_ANIMATION_H
#define BK_ANIMATION_H

#include <stdbool.h>
#include <stdint.h>

#define BK_BONE_MAX 128

// Rotation in degrees (x, y, z), scale (x, y, z) and translation (x, y, z) of one bone, as animationfile.c lays them.
typedef struct BkBoneChannels {
    float values[9];
} BkBoneChannels;

typedef struct BkAnimation {
    uint8_t* data;
    uint32_t size;
    int16_t firstFrame;
    int16_t lastFrame;
} BkAnimation;

bool BkAnimation_Load(BkAnimation* animation, uint16_t assetId);
void BkAnimation_Free(BkAnimation* animation);
// Every bone's channels at `frame` (firstFrame..lastFrame); bones the clip does not move keep the rest pose.
void BkAnimation_Sample(const BkAnimation* animation, float frame, BkBoneChannels channels[BK_BONE_MAX]);

#endif
