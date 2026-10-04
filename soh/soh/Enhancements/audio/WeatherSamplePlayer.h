#pragma once

#ifdef __cplusplus
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

inline float WeatherSamplePlayer_ClampGain(float gain) {
    return std::clamp(gain, 0.0f, 1.0f);
}

inline size_t WeatherSamplePlayer_TestMixMonoCountClamps(int16_t* destination, const int16_t* source, size_t frameCount,
                                                         float gain) {
    const float clampedGain = WeatherSamplePlayer_ClampGain(gain);
    size_t clampCount = 0;
    for (size_t i = 0; i < frameCount; ++i) {
        const int32_t sample = static_cast<int32_t>(source[i] * clampedGain);
        for (size_t channel = 0; channel < 2; ++channel) {
            const size_t index = i * 2 + channel;
            const int32_t mixed = static_cast<int32_t>(destination[index]) + sample;
            clampCount += mixed < std::numeric_limits<int16_t>::min() || mixed > std::numeric_limits<int16_t>::max();
            destination[index] =
                static_cast<int16_t>(std::clamp(mixed, static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
                                                static_cast<int32_t>(std::numeric_limits<int16_t>::max())));
        }
    }
    return clampCount;
}

inline void WeatherSamplePlayer_TestMixMono(int16_t* destination, const int16_t* source, size_t frameCount,
                                            float gain) {
    WeatherSamplePlayer_TestMixMonoCountClamps(destination, source, frameCount, gain);
}

inline size_t WeatherSamplePlayer_AdvanceLoopPosition(size_t position, size_t loopStart, size_t loopEnd,
                                                      size_t frames) {
    if (loopEnd <= loopStart) {
        return 0;
    }
    const size_t advanced = position + frames;
    if (advanced < loopEnd) {
        return advanced;
    }
    return loopStart + ((advanced - loopEnd) % (loopEnd - loopStart));
}

extern "C" {
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

void WeatherSamplePlayer_Init(void);
bool WeatherSamplePlayer_Play(const char* resourcePath, float gain);
void WeatherSamplePlayer_SetLoop(const char* resourcePath, float gain);
void WeatherSamplePlayer_Mix(int16_t* interleavedStereo, size_t frameCount);
void WeatherSamplePlayer_Reset(void);
#ifdef __cplusplus
}
#endif
