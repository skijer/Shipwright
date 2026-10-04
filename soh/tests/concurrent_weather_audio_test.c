#include "test_require.h"
#include <stdint.h>

#include "../src/code/concurrent_weather_audio.h"

int main(void) {
    ConcurrentWeatherAudioState state = { 0 };

    // Rain routing owns only a private fallback loop; ordinary banks and
    // MAIN/FANFARE players are not inputs or outputs of this policy.
    REQUIRE(ConcurrentWeatherAudio_SelectRainAction(0, 1, 0, 0.5f) == CONCURRENT_WEATHER_RAIN_SET_LOOP);
    REQUIRE(ConcurrentWeatherAudio_SelectRainAction(1, 1, 0, 0.25f) == CONCURRENT_WEATHER_RAIN_SET_LOOP);
    REQUIRE(ConcurrentWeatherAudio_SelectRainAction(1, 1, 1, 0.5f) == CONCURRENT_WEATHER_RAIN_STOP_LOOP);
    REQUIRE(ConcurrentWeatherAudio_SelectRainAction(0, 1, 1, 0.5f) == CONCURRENT_WEATHER_RAIN_NO_CHANGE);
    REQUIRE(ConcurrentWeatherAudio_SelectRainAction(1, 0, 0, 0.5f) == CONCURRENT_WEATHER_RAIN_STOP_LOOP);
    REQUIRE(ConcurrentWeatherAudio_SelectRainAction(0, 0, 0, 0.5f) == CONCURRENT_WEATHER_RAIN_NO_CHANGE);
    REQUIRE(ConcurrentWeatherAudio_SelectRainAction(1, 1, 0, 0.0f) == CONCURRENT_WEATHER_RAIN_STOP_LOOP);
    REQUIRE(ConcurrentWeatherAudio_SelectRainAction(0, 1, 0, 0.0f) == CONCURRENT_WEATHER_RAIN_NO_CHANGE);

    REQUIRE(ConcurrentWeatherAudio_ShouldPlayRainSfx(&state, 0x0028, 0x0001));

    ConcurrentWeatherAudio_TrackNatureChannel(&state, 0x0E, 1, 1, 0x0E, 0x0F, 1);
    REQUIRE(!ConcurrentWeatherAudio_ShouldPlayRainSfx(&state, 0x0001, 0x0001));
    REQUIRE(ConcurrentWeatherAudio_ShouldPlayRainSfx(&state, 0x0028, 0x0001));

    ConcurrentWeatherAudio_TrackNatureChannel(&state, 0x0F, 1, 1, 0x0E, 0x0F, 1);
    REQUIRE(!ConcurrentWeatherAudio_ShouldPlayRainSfx(&state, 0x0001, 0x0001));

    ConcurrentWeatherAudio_TrackNatureChannel(&state, 0x0E, 1, 0, 0x0E, 0x0F, 1);
    REQUIRE(ConcurrentWeatherAudio_ShouldPlayRainSfx(&state, 0x0001, 0x0001));

    ConcurrentWeatherAudio_TrackNatureChannel(&state, 0x0E, 4, 1, 0x0E, 0x0F, 1);
    REQUIRE(ConcurrentWeatherAudio_ShouldPlayRainSfx(&state, 0x0001, 0x0001));

    ConcurrentWeatherAudio_TrackNatureChannel(&state, 0x0F, 1, 1, 0x0E, 0x0F, 1);
    REQUIRE(!ConcurrentWeatherAudio_ShouldPlayThunderSfx(&state, 0x0001, 0x0001));
    REQUIRE(ConcurrentWeatherAudio_ShouldPlayThunderSfx(&state, 0x0028, 0x0001));
    ConcurrentWeatherAudio_TrackNatureChannel(&state, 0x0F, 1, 0, 0x0E, 0x0F, 1);
    REQUIRE(ConcurrentWeatherAudio_ShouldPlayThunderSfx(&state, 0x0001, 0x0001));

    REQUIRE(ConcurrentWeatherAudio_ClampPercent(-5) == 0);
    REQUIRE(ConcurrentWeatherAudio_ClampPercent(75) == 75);
    REQUIRE(ConcurrentWeatherAudio_ClampPercent(125) == 100);
    REQUIRE(ConcurrentWeatherAudio_ThunderStyle(-1) == CONCURRENT_WEATHER_THUNDER_LOW);
    REQUIRE(ConcurrentWeatherAudio_ThunderStyle(CONCURRENT_WEATHER_THUNDER_LAYERED) ==
            CONCURRENT_WEATHER_THUNDER_LAYERED);
    REQUIRE(ConcurrentWeatherAudio_ThunderStyle(CONCURRENT_WEATHER_THUNDER_LIGHTNING) ==
            CONCURRENT_WEATHER_THUNDER_LIGHTNING);
    REQUIRE(ConcurrentWeatherAudio_ThunderStyle(999) == CONCURRENT_WEATHER_THUNDER_LOW);
    REQUIRE(ConcurrentWeatherAudio_ThunderFrequencyScale(-5) == 0.0f);
    REQUIRE(ConcurrentWeatherAudio_ThunderFrequencyScale(0) == 0.0f);
    REQUIRE(ConcurrentWeatherAudio_ThunderFrequencyScale(25) == 0.5f);
    REQUIRE(ConcurrentWeatherAudio_ThunderFrequencyScale(50) == 1.0f);
    REQUIRE(ConcurrentWeatherAudio_ThunderFrequencyScale(100) == 2.0f);
    REQUIRE(ConcurrentWeatherAudio_ThunderFrequencyScale(125) == 2.0f);
    return 0;
}
