#include "../../shim/libbk_build.h"
// BK's sound calls from the player code, queued for the host to play: one-shots and the sfx sources basfx.c owns.
#include "../../shim/ultra64.h"
#include <string.h>
#include "../../include/functions.h"
#include "../../include/variables.h"
#include "../../src/libbk_port/libbk.h"

#define SOUND_QUEUE_MAX 32
#define SFX_SOURCE_MAX 35
#define BK_FULL_VOLUME 32767.0f
#define BK_DEFAULT_VOLUME 22000
#define BK_LOUD_VOLUME 0x7FF8

typedef struct SfxSource {
    bool isUsed;
    enum sfx_e sfx;
    f32 pitch;
    s32 volume;
    void (*onDone)(u8);
} SfxSource;

static LibBkSound sSounds[SOUND_QUEUE_MAX];
static s32 sSoundCount;
// BK treats source 0 as "none", so it is never handed out.
static SfxSource sSources[SFX_SOURCE_MAX];

static void QueueSound(enum sfx_e sfx, f32 pitch, s32 volume) {
    if (sSoundCount >= SOUND_QUEUE_MAX || volume <= 0) {
        return;
    }
    sSounds[sSoundCount].sfxId = sfx;
    sSounds[sSoundCount].pitch = pitch;
    sSounds[sSoundCount].volume = volume / BK_FULL_VOLUME;
    sSoundCount++;
}

static SfxSource* GetSource(u8 index) {
    if (index == 0 || index >= SFX_SOURCE_MAX || !sSources[index].isUsed) {
        return NULL;
    }
    return &sSources[index];
}

int32_t LibBk_TakeSounds(LibBkSound* sounds, int32_t capacity) {
    const s32 count = MIN(sSoundCount, capacity);

    memcpy(sounds, sSounds, sizeof(LibBkSound) * count);
    sSoundCount = 0;
    return count;
}

void func_8030E4E4(enum sfx_e uid) {
    QueueSound(uid, 1.0f, BK_DEFAULT_VOLUME);
}

void sfxsource_playHighPriority(enum sfx_e uid) {
    QueueSound(uid, 1.0f, BK_DEFAULT_VOLUME);
}

void func_8030E560(enum sfx_e uid, s32 volume) {
    QueueSound(uid, 1.0f, volume);
}

void func_8030E58C(enum sfx_e uid, f32 pitch) {
    QueueSound(uid, pitch, BK_DEFAULT_VOLUME);
}

void func_8030E5F4(enum sfx_e uid, f32 pitch) {
    QueueSound(uid, pitch, BK_DEFAULT_VOLUME);
}

// Packed as FUNC_8030E624 builds it: 11 bits of pitch * 1023, 10 bits of volume >> 5, 11 bits of sfx.
void func_8030E624(u32 packed) {
    QueueSound(packed & 0x7FF, ((packed >> 21) & 0x7FF) / 1023.0f, (packed >> 6) & 0x7FE0);
}

void gcsfx_playWithPitch(enum sfx_e uid, f32 pitch, s32 volume) {
    QueueSound(uid, pitch, volume);
}

void func_8030E6D4(enum sfx_e uid) {
    QueueSound(uid, 1.0f, BK_LOUD_VOLUME);
}

void func_8030E760(enum sfx_e uid, f32 pitch, s32 volume) {
    QueueSound(uid, pitch, volume);
}

void func_8030EAAC(enum sfx_e uid, f32 pitch, s32 volume, s32 delay) {
    QueueSound(uid, pitch, volume);
}

void func_8030EB00(enum sfx_e uid, f32 minPitch, f32 maxPitch) {
    QueueSound(uid, sfx_randf2(minPitch, maxPitch), BK_DEFAULT_VOLUME);
}

void func_8030EB88(enum sfx_e uid, f32 minPitch, f32 maxPitch) {
    QueueSound(uid, sfx_randf2(minPitch, maxPitch), BK_DEFAULT_VOLUME);
}

void func_8030EBC8(enum sfx_e uid, f32 minPitch, f32 maxPitch, s32 minVolume, s32 maxVolume) {
    QueueSound(uid, sfx_randf2(minPitch, maxPitch), sfx_randi2(minVolume, maxVolume));
}

void func_8030EC20(enum sfx_e uid, f32 minPitch, f32 maxPitch, u32 minVolume, u32 maxVolume) {
    QueueSound(uid, sfx_randf2(minPitch, maxPitch), sfx_randi2(minVolume, maxVolume));
}

// Fades linearly from full at the inner radius to silent at the outer one, as sfx sources do.
void func_8030E9C4(enum sfx_e uid, f32 pitch, u32 volume, f32 position[3], f32 minFadeDistance,
                   f32 maxFadeDistance) {
    f32 playerPosition[3];
    f32 distance;
    f32 fade;

    player_getPosition(playerPosition);
    distance = ml_vec3f_distance(playerPosition, position);
    if (distance >= maxFadeDistance) {
        return;
    }
    fade = (distance <= minFadeDistance) ? 1.0f : (maxFadeDistance - distance) / (maxFadeDistance - minFadeDistance);
    QueueSound(uid, pitch, (s32)(volume * fade));
}

u8 sfxsource_createSfxsourceAndReturnIndex(void) {
    for (u8 index = 1; index < SFX_SOURCE_MAX; index++) {
        if (!sSources[index].isUsed) {
            memset(&sSources[index], 0, sizeof(SfxSource));
            sSources[index].isUsed = true;
            sSources[index].pitch = 1.0f;
            sSources[index].volume = BK_DEFAULT_VOLUME;
            return index;
        }
    }
    return 0;
}

void sfxsource_freeSfxsourceByIndex(u8 index) {
    SfxSource* source = GetSource(index);

    if (source != NULL) {
        source->isUsed = false;
    }
}

void sfxsource_setSfxId(u8 index, enum sfx_e uid) {
    SfxSource* source = GetSource(index);

    if (source != NULL) {
        source->sfx = uid;
    }
}

void sfxsource_setSampleRate(u8 index, s32 volume) {
    SfxSource* source = GetSource(index);

    if (source != NULL) {
        source->volume = volume;
    }
}

s32 sfxSource_getSampleRate(u8 index) {
    SfxSource* source = GetSource(index);

    return (source != NULL) ? source->volume : 0;
}

void sfxsource_playSfxAtVolume(u8 index, f32 pitch) {
    SfxSource* source = GetSource(index);

    if (source != NULL) {
        source->pitch = pitch;
    }
}

void func_8030E04C(u8 index, f32 pitch, f32 minPitch, f32 maxPitch) {
    sfxsource_playSfxAtVolume(index, pitch);
}

void sfxSource_func_8030E2C4(u8 index) {
    SfxSource* source = GetSource(index);

    if (source != NULL) {
        QueueSound(source->sfx, source->pitch, source->volume);
    }
}

void sfxSource_setCallbackByIndex(u8 index, void (*onDone)(u8)) {
    SfxSource* source = GetSource(index);

    if (source != NULL) {
        source->onDone = onDone;
    }
}

void sfxSource_triggerCallbackByIndex(u8 index) {
    SfxSource* source = GetSource(index);

    if (source != NULL && source->onDone != NULL) {
        source->onDone(index);
    }
}

void func_8030DD90(u8 index, s32 priority) {
}

void sfxSource_setunk43_7ByIndex(u8 index, int mode) {
}

// libbk keeps no voices, so a source is never still sounding when the player code asks.
int func_8030E3FC(u8 index) {
    return 0;
}
