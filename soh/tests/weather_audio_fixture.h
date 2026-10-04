#pragma once
#include "test_require.h"
#include <cstring>
#include "weather_audio_stubs/z64audio.h"

// Zero predictors make one ADPCM frame decode to literal 1..7 values.
// Different positions catch accidental restarts, not merely missing voices.
inline SoundFontSample* WeatherAudioFixture(const char* path) {
    static int16_t coefficients[16] = {};
    static AdpcmBook book = { 2, 1, coefficients };
    static uint8_t rainData[9] = { 0, 0x12, 0x34, 0x56, 0x71, 0x23, 0x45, 0x67, 0x12 };
    static uint8_t thunderData[9] = { 0, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22 };
    static uint8_t lightningData[9] = { 0, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33 };
    static AdpcmLoop rainLoop = { 4, 12, UINT32_MAX, 16, {} };
    static SoundFontSample rain = { 0, 0, 0, 0, 9, 9, rainData, &rainLoop, &book };
    static SoundFontSample thunder = { 0, 0, 0, 0, 9, 9, thunderData, nullptr, &book };
    static SoundFontSample lightning = { 0, 0, 0, 0, 9, 9, lightningData, nullptr, &book };
    if (std::strcmp(path, "audio/samples/Rainfall_META") == 0)
        return &rain;
    if (std::strcmp(path, "audio/samples/Low Thunder_META") == 0)
        return &thunder;
    if (std::strcmp(path, "audio/samples/Lightning_META") == 0)
        return &lightning;
    REQUIRE(false);
    return nullptr;
}
