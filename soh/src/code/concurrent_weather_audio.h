#ifndef CONCURRENT_WEATHER_AUDIO_H
#define CONCURRENT_WEATHER_AUDIO_H

#include <stdint.h>

typedef struct {
    uint8_t natureRainEnabled;
    uint8_t natureLightningEnabled;
} ConcurrentWeatherAudioState;

typedef enum {
    CONCURRENT_WEATHER_THUNDER_LOW = 0,
    CONCURRENT_WEATHER_THUNDER_LAYERED = 1,
    CONCURRENT_WEATHER_THUNDER_LIGHTNING = 2,
} ConcurrentWeatherThunderStyle;

typedef enum {
    CONCURRENT_WEATHER_RAIN_NO_CHANGE,
    CONCURRENT_WEATHER_RAIN_SET_LOOP,
    CONCURRENT_WEATHER_RAIN_STOP_LOOP,
} ConcurrentWeatherRainAction;

#ifdef __cplusplus
extern "C" {
#endif
ConcurrentWeatherRainAction ConcurrentWeatherAudio_SelectRainAction(uint8_t ownsFallbackLoop,
                                                                    uint8_t weatherOwnsRain,
                                                                    uint8_t natureRainEnabled, float gain);
#ifdef __cplusplus
}
#endif

static inline int32_t ConcurrentWeatherAudio_ClampPercent(int32_t percent) {
    return percent < 0 ? 0 : percent > 100 ? 100 : percent;
}

static inline ConcurrentWeatherThunderStyle ConcurrentWeatherAudio_ThunderStyle(int32_t style) {
    return (style == CONCURRENT_WEATHER_THUNDER_LAYERED || style == CONCURRENT_WEATHER_THUNDER_LIGHTNING)
               ? (ConcurrentWeatherThunderStyle)style
               : CONCURRENT_WEATHER_THUNDER_LOW;
}

static inline float ConcurrentWeatherAudio_ThunderFrequencyScale(int32_t percent) {
    return ConcurrentWeatherAudio_ClampPercent(percent) / 50.0f;
}

static inline void ConcurrentWeatherAudio_TrackNatureChannel(ConcurrentWeatherAudioState* state,
                                                              uint8_t channelRange, uint8_t port, uint8_t value,
                                                              uint8_t rainChannel, uint8_t lightningChannel,
                                                              uint8_t enablePort) {
    uint8_t firstChannel = channelRange >> 4;
    uint8_t lastChannel = channelRange & 0xF;

    if (firstChannel == 0) {
        firstChannel = lastChannel;
    }

    if ((port == enablePort) && (rainChannel >= firstChannel) && (rainChannel <= lastChannel)) {
        state->natureRainEnabled = value != 0;
    }

    if ((port == enablePort) && (lightningChannel >= firstChannel) && (lightningChannel <= lastChannel)) {
        state->natureLightningEnabled = value != 0;
    }
}

static inline int ConcurrentWeatherAudio_ShouldPlayRainSfx(const ConcurrentWeatherAudioState* state,
                                                            uint16_t currentMainBgm,
                                                            uint16_t natureAmbienceSequence) {
    return !state->natureRainEnabled || (currentMainBgm != natureAmbienceSequence);
}

static inline int ConcurrentWeatherAudio_ShouldPlayThunderSfx(const ConcurrentWeatherAudioState* state,
                                                               uint16_t currentMainBgm,
                                                               uint16_t natureAmbienceSequence) {
    return !state->natureLightningEnabled || (currentMainBgm != natureAmbienceSequence);
}

#endif
