#include "concurrent_weather_audio.h"

ConcurrentWeatherRainAction ConcurrentWeatherAudio_SelectRainAction(uint8_t ownsFallbackLoop, uint8_t weatherOwnsRain,
                                                                    uint8_t natureRainEnabled, float gain) {
    // Use an already-active native rain channel without replacing MAIN BGM
    // or allocating voices in ordinary SFX banks. Otherwise use a private loop.
    if (weatherOwnsRain && !natureRainEnabled && gain > 0.0f) {
        return CONCURRENT_WEATHER_RAIN_SET_LOOP;
    }
    return ownsFallbackLoop ? CONCURRENT_WEATHER_RAIN_STOP_LOOP : CONCURRENT_WEATHER_RAIN_NO_CHANGE;
}
