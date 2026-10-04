#pragma once

#include <cstdint>

enum class HyruleFieldNightMusicDecision {
    NoChange,
    StartNight,
    RestoreNight,
    StopNightRestoreDay,
};

struct HyruleFieldNightMusicState {
    bool enabled;
    bool inHyruleField;
    bool isNight;
    bool ownsNightBgm;
    bool nightBgmPlaying;
    bool fanfarePlaying;
    bool explicitAudioOverride;
};

HyruleFieldNightMusicDecision HyruleFieldNightMusic_Select(const HyruleFieldNightMusicState& state);
uint16_t HyruleFieldNightMusic_ValidateSequence(uint16_t selected, bool isValid, uint16_t fallback);
uint16_t HyruleFieldNightMusic_ResolveSequence(uint16_t selected, uint16_t replacement);

#ifndef HYRULE_FIELD_NIGHT_MUSIC_TEST
struct PlayState;
void HyruleFieldNightMusic_Update(PlayState* play);
void HyruleFieldNightMusic_Reset();
#endif
