#include "bk_animation.h"

#include <stdbool.h>
#include <stdlib.h>

#include "bk_assets.h"
#include "bk_bytes.h"

#define HEADER_SIZE 8
#define ELEMENT_HEADER_SIZE 4
#define KEY_SIZE 4
#define KEY_VALUE_SCALE 64.0f
#define CHANNEL_COUNT 9

static const float kRestChannels[CHANNEL_COUNT] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f };

typedef struct Key {
    bool hasEaseIn;
    bool hasEaseOut;
    int32_t frame;
    float value;
} Key;

static Key ReadKey(const uint8_t* at) {
    const uint16_t word = BkBytes_U16(at);
    Key key = { (word >> 15) & 1, (word >> 14) & 1, word & 0x3FFF, BkBytes_S16(at + 2) / KEY_VALUE_SCALE };

    return key;
}

static float CatmullRom(float x, float k0, float k1, float k2, float k3) {
    const float t = (x < 0.0f) ? 0.0f : ((x > 1.0f) ? 1.0f : x);
    const float a = -0.5f * k0 + 1.5f * k1 - 1.5f * k2 + 0.5f * k3;
    const float b = k0 - 2.5f * k1 + 2.0f * k2 - 0.5f * k3;
    const float c = -0.5f * k0 + 0.5f * k2;

    return ((a * t + b) * t + c) * t + k1;
}

// Mirrors animationfilebin_func_8033AC38: ease flags pull the neighbouring keys into the spline.
static float SampleChannel(const BkAnimation* animation, const uint8_t* keys, int32_t keyCount, int32_t channel,
                           float time) {
    const Key first = ReadKey(keys);
    const Key last = ReadKey(keys + (keyCount - 1) * KEY_SIZE);

    if ((int32_t)time < first.frame) {
        const float next = (first.hasEaseIn && keyCount >= 2) ? ReadKey(keys + KEY_SIZE).value : first.value;
        const float span = (float)(first.frame - animation->firstFrame);

        return CatmullRom(span != 0.0f ? (time - animation->firstFrame) / span : 1.0f, kRestChannels[channel],
                          kRestChannels[channel], first.value, next);
    }
    if ((int32_t)time >= last.frame) {
        const float previous =
            (last.hasEaseOut && keyCount >= 2) ? ReadKey(keys + (keyCount - 2) * KEY_SIZE).value : last.value;

        return CatmullRom(time - last.frame, previous, last.value, last.value, last.value);
    }

    int32_t low = 0;
    int32_t high = keyCount - 1;

    while (high - low > 1) {
        const int32_t middle = (low + high) / 2;

        if (ReadKey(keys + middle * KEY_SIZE).frame <= (int32_t)time) {
            low = middle;
        } else {
            high = middle;
        }
    }

    const Key from = ReadKey(keys + low * KEY_SIZE);
    const Key to = ReadKey(keys + high * KEY_SIZE);
    const float t = (time - from.frame) / (float)(to.frame - from.frame);

    if (!from.hasEaseOut && !to.hasEaseIn) {
        return from.value + (to.value - from.value) * t;
    }

    const float before = (from.hasEaseOut && low > 0) ? ReadKey(keys + (low - 1) * KEY_SIZE).value : from.value;
    const float after = (to.hasEaseIn && high + 1 < keyCount) ? ReadKey(keys + (high + 1) * KEY_SIZE).value : to.value;

    return CatmullRom(t, before, from.value, to.value, after);
}

bool BkAnimation_Load(BkAnimation* animation, uint16_t assetId) {
    animation->data = BkAssets_Load(assetId, &animation->size);
    if (animation->data == NULL || animation->size < HEADER_SIZE) {
        BkAnimation_Free(animation);
        return false;
    }
    animation->firstFrame = BkBytes_S16(animation->data);
    animation->lastFrame = BkBytes_S16(animation->data + 2);
    return true;
}

void BkAnimation_Free(BkAnimation* animation) {
    free(animation->data);
    *animation = (BkAnimation){ 0 };
}

void BkAnimation_Sample(const BkAnimation* animation, float frame, BkBoneChannels channels[BK_BONE_MAX]) {
    const int32_t elementCount = BkBytes_S16(animation->data + 4);
    const uint8_t* end = animation->data + animation->size;
    const uint8_t* element = animation->data + HEADER_SIZE;

    for (int32_t bone = 0; bone < BK_BONE_MAX; bone++) {
        for (int32_t channel = 0; channel < CHANNEL_COUNT; channel++) {
            channels[bone].values[channel] = kRestChannels[channel];
        }
    }
    for (int32_t index = 0; index < elementCount && element + ELEMENT_HEADER_SIZE <= end; index++) {
        const uint16_t head = BkBytes_U16(element);
        const int32_t bone = head >> 4;
        const int32_t channel = head & 0xF;
        const int32_t keyCount = BkBytes_S16(element + 2);
        const uint8_t* keys = element + ELEMENT_HEADER_SIZE;

        if (keys + keyCount * KEY_SIZE > end) {
            return;
        }
        if (bone < BK_BONE_MAX && channel < CHANNEL_COUNT && keyCount > 0) {
            channels[bone].values[channel] = SampleChannel(animation, keys, keyCount, channel, frame);
        }
        element = keys + keyCount * KEY_SIZE;
    }
}
