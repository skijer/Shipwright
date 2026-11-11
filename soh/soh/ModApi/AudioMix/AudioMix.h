#ifndef SOH_MOD_API_AUDIO_MIX_H
#define SOH_MOD_API_AUDIO_MIX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*SOHAudioMixFunc)(int16_t* interleavedStereoSamples, uint32_t frameCount);

typedef enum {
    SOH_AUDIO_GROUP_SFX,
    SOH_AUDIO_GROUP_MUSIC,
    SOH_AUDIO_GROUP_SUB_MUSIC,
    SOH_AUDIO_GROUP_FANFARE,
    SOH_AUDIO_GROUP_MAX,
} SOHAudioGroup;

bool AudioMix_Register(SOHAudioMixFunc callback);
bool AudioMix_RegisterInGroup(SOHAudioMixFunc callback, uint8_t group, const char* volumeCvar);
void AudioMix_Run(int16_t* samples, uint32_t frameCount);
void AudioMix_Init(void);

#ifdef __cplusplus
}
#endif

#endif
