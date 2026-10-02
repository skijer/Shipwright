#include "AudioMix.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

namespace {

constexpr uint32_t MaxCallbacks = 8;

struct Registration {
    SOHAudioMixFunc callback;
    std::string volumeCvar;
    const char* groupCvar;
    std::atomic<float> gain;
};

const char* GroupCvar(uint8_t group) {
    switch (group) {
        case SOH_AUDIO_GROUP_MUSIC:
            return CVAR_SETTING("Volume.MainMusic");
        case SOH_AUDIO_GROUP_SUB_MUSIC:
            return CVAR_SETTING("Volume.SubMusic");
        case SOH_AUDIO_GROUP_FANFARE:
            return CVAR_SETTING("Volume.Fanfare");
        default:
            return CVAR_SETTING("Volume.SFX");
    }
}

Registration sRegistrations[MaxCallbacks];
std::atomic<uint32_t> sCount = 0;
std::vector<int16_t> sScratch;

float ReadVolume(const char* cvar) {
    return static_cast<float>(CVarGetInteger(cvar, 100)) / 100.0f;
}

void RefreshGains() {
    const float master = ReadVolume(CVAR_SETTING("Volume.Master"));
    const uint32_t count = sCount.load(std::memory_order_acquire);

    for (uint32_t index = 0; index < count; ++index) {
        Registration& registration = sRegistrations[index];
        float gain = master * ReadVolume(registration.groupCvar);
        if (!registration.volumeCvar.empty()) {
            gain *= ReadVolume(registration.volumeCvar.c_str());
        }
        registration.gain.store(gain, std::memory_order_relaxed);
    }
}

int16_t AddSaturating(int16_t mixed, int32_t addition) {
    const int32_t sum = mixed + addition;
    if (sum > INT16_MAX) {
        return INT16_MAX;
    }
    if (sum < INT16_MIN) {
        return INT16_MIN;
    }
    return static_cast<int16_t>(sum);
}

} // namespace

bool AudioMix_RegisterInGroup(SOHAudioMixFunc callback, uint8_t group, const char* volumeCvar) {
    uint32_t count = sCount.load(std::memory_order_relaxed);

    if (callback == nullptr || count >= MaxCallbacks || group >= SOH_AUDIO_GROUP_MAX) {
        return false;
    }
    sRegistrations[count].callback = callback;
    sRegistrations[count].groupCvar = GroupCvar(group);
    sRegistrations[count].volumeCvar = volumeCvar != nullptr ? volumeCvar : "";
    sRegistrations[count].gain.store(1.0f, std::memory_order_relaxed);
    sCount.store(count + 1, std::memory_order_release);
    RefreshGains();
    return true;
}

bool AudioMix_Register(SOHAudioMixFunc callback) {
    return AudioMix_RegisterInGroup(callback, SOH_AUDIO_GROUP_SFX, nullptr);
}

void AudioMix_Run(int16_t* samples, uint32_t frameCount) {
    const uint32_t count = sCount.load(std::memory_order_acquire);
    if (count == 0 || frameCount == 0) {
        return;
    }

    const size_t needed = static_cast<size_t>(frameCount) * 2;
    if (sScratch.size() < needed) {
        sScratch.resize(needed);
    }

    for (uint32_t index = 0; index < count; ++index) {
        const float gain = sRegistrations[index].gain.load(std::memory_order_relaxed);
        if (gain <= 0.0f) {
            continue;
        }

        std::fill_n(sScratch.begin(), needed, static_cast<int16_t>(0));
        sRegistrations[index].callback(sScratch.data(), frameCount);

        for (size_t sample = 0; sample < needed; ++sample) {
            if (sScratch[sample] != 0) {
                samples[sample] = AddSaturating(samples[sample], static_cast<int32_t>(sScratch[sample] * gain));
            }
        }
    }
}

void AudioMix_Init(void) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(RefreshGains);
}
