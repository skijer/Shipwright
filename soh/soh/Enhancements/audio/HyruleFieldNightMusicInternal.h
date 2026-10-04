#pragma once

#include <cstdint>

#include "HyruleFieldNightMusic.h"

using HyruleFieldNightMusicSequenceValidator = bool (*)(uint16_t);
using HyruleFieldNightMusicSequenceResolver = uint16_t (*)(uint16_t);

uint16_t HyruleFieldNightMusic_StartSequence(uint16_t selected, uint16_t fallback,
                                             HyruleFieldNightMusicSequenceValidator isValid,
                                             HyruleFieldNightMusicSequenceResolver getReplacement);
uint16_t HyruleFieldNightMusic_RestoreSequence();
void HyruleFieldNightMusic_ClearSequence();
bool HyruleFieldNightMusic_ShouldRestoreDaySequence(const HyruleFieldNightMusicState& state);
bool HyruleFieldNightMusic_IsFieldLifecycleSequence(uint16_t sequence, uint16_t fieldLogic, uint16_t natureAmbience,
                                                    uint16_t disabled);
uint8_t HyruleFieldNightMusic_GetPlaybackPlayer();
bool HyruleFieldNightMusic_IsNightSequencePlaying(bool ownsNightBgm, uint16_t mainSequence,
                                                  uint16_t nightPlaybackSequence);

struct HyruleFieldNightMusicDiagnosticSnapshot {
    HyruleFieldNightMusicState state;
    HyruleFieldNightMusicDecision decision;
    uint16_t mainSequence;
    uint16_t subSequence;
    uint16_t fanfareSequence;
    uint16_t nightPlaybackSequence;
};

bool HyruleFieldNightMusic_ShouldLogDiagnostic(const HyruleFieldNightMusicDiagnosticSnapshot* previous,
                                               const HyruleFieldNightMusicDiagnosticSnapshot& current);
