#include "HyruleFieldNightMusicInternal.h"

HyruleFieldNightMusicDecision HyruleFieldNightMusic_Select(const HyruleFieldNightMusicState& state) {
    if (!state.inHyruleField) {
        return state.ownsNightBgm ? HyruleFieldNightMusicDecision::StopNightRestoreDay
                                  : HyruleFieldNightMusicDecision::NoChange;
    }
    if (state.explicitAudioOverride || state.fanfarePlaying) {
        return HyruleFieldNightMusicDecision::NoChange;
    }
    if (!state.enabled || !state.isNight) {
        return state.ownsNightBgm ? HyruleFieldNightMusicDecision::StopNightRestoreDay
                                  : HyruleFieldNightMusicDecision::NoChange;
    }
    if (!state.ownsNightBgm) {
        return HyruleFieldNightMusicDecision::StartNight;
    }
    return state.nightBgmPlaying ? HyruleFieldNightMusicDecision::NoChange
                                 : HyruleFieldNightMusicDecision::RestoreNight;
}

uint16_t HyruleFieldNightMusic_ResolveSequence(uint16_t selected, uint16_t replacement) {
    (void)selected;
    return replacement;
}

uint16_t HyruleFieldNightMusic_ValidateSequence(uint16_t selected, bool isValid, uint16_t fallback) {
    return isValid ? selected : fallback;
}

static constexpr uint16_t kDisabledSequence = 0xFFFF;
static uint16_t sNightPlaybackSeq = kDisabledSequence;

uint8_t HyruleFieldNightMusic_GetPlaybackPlayer() {
    return 0;
}

bool HyruleFieldNightMusic_IsNightSequencePlaying(bool ownsNightBgm, uint16_t mainSequence,
                                                  uint16_t nightPlaybackSequence) {
    return ownsNightBgm && mainSequence != kDisabledSequence && nightPlaybackSequence != kDisabledSequence &&
           (mainSequence & 0xFF) == (nightPlaybackSequence & 0xFF);
}

uint16_t HyruleFieldNightMusic_StartSequence(uint16_t selected, uint16_t fallback,
                                             HyruleFieldNightMusicSequenceValidator isValid,
                                             HyruleFieldNightMusicSequenceResolver getReplacement) {
    const uint16_t valid = HyruleFieldNightMusic_ValidateSequence(selected, isValid(selected), fallback);
    const uint16_t replacement = HyruleFieldNightMusic_ResolveSequence(valid, getReplacement(valid));
    sNightPlaybackSeq = HyruleFieldNightMusic_ValidateSequence(replacement, isValid(replacement), valid);
    return sNightPlaybackSeq;
}

uint16_t HyruleFieldNightMusic_RestoreSequence() {
    return sNightPlaybackSeq;
}

void HyruleFieldNightMusic_ClearSequence() {
    sNightPlaybackSeq = kDisabledSequence;
}

bool HyruleFieldNightMusic_ShouldRestoreDaySequence(const HyruleFieldNightMusicState& state) {
    return state.inHyruleField && state.ownsNightBgm && (!state.enabled || !state.isNight) &&
           !state.explicitAudioOverride && !state.fanfarePlaying;
}

bool HyruleFieldNightMusic_IsFieldLifecycleSequence(uint16_t sequence, uint16_t fieldLogic, uint16_t natureAmbience,
                                                    uint16_t disabled) {
    return sequence == disabled || (sequence & 0xFF) == (fieldLogic & 0xFF) ||
           (sequence & 0xFF) == (natureAmbience & 0xFF);
}

bool HyruleFieldNightMusic_ShouldLogDiagnostic(const HyruleFieldNightMusicDiagnosticSnapshot* previous,
                                               const HyruleFieldNightMusicDiagnosticSnapshot& current) {
    if (previous == nullptr) {
        return current.state.inHyruleField || current.state.ownsNightBgm;
    }
    if (!previous->state.inHyruleField && !previous->state.ownsNightBgm && !current.state.inHyruleField &&
        !current.state.ownsNightBgm) {
        return false;
    }
    return previous->state.enabled != current.state.enabled ||
           previous->state.inHyruleField != current.state.inHyruleField ||
           previous->state.isNight != current.state.isNight ||
           previous->state.ownsNightBgm != current.state.ownsNightBgm ||
           previous->state.nightBgmPlaying != current.state.nightBgmPlaying ||
           previous->state.fanfarePlaying != current.state.fanfarePlaying ||
           previous->state.explicitAudioOverride != current.state.explicitAudioOverride ||
           previous->decision != current.decision || previous->mainSequence != current.mainSequence ||
           previous->subSequence != current.subSequence || previous->fanfareSequence != current.fanfareSequence ||
           previous->nightPlaybackSequence != current.nightPlaybackSequence;
}

#ifndef HYRULE_FIELD_NIGHT_MUSIC_TEST
#include <spdlog/spdlog.h>

#include "AudioCollection.h"
#include "night_bgm_bridge.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"

extern "C" {
#include "functions.h"
#include "sequence.h"
#include "variables.h"
#include "z64.h"
extern PlayState* gPlayState;
}

static_assert(SEQ_PLAYER_BGM_MAIN == 0,
              "HyruleFieldNightMusic_GetPlaybackPlayer must identify the MAIN sequence player");

static constexpr uint16_t kDefaultNightSequence = NA_BGM_KAKARIKO_ADULT;
static bool sOwnsNightBgm = false;
static bool sHasDiagnosticSnapshot = false;
static HyruleFieldNightMusicDiagnosticSnapshot sDiagnosticSnapshot = {};

static const char* DecisionName(HyruleFieldNightMusicDecision decision) {
    switch (decision) {
        case HyruleFieldNightMusicDecision::StartNight:
            return "start-night";
        case HyruleFieldNightMusicDecision::RestoreNight:
            return "restore-night";
        case HyruleFieldNightMusicDecision::StopNightRestoreDay:
            return "stop-night-restore-day";
        case HyruleFieldNightMusicDecision::NoChange:
        default:
            return "no-change";
    }
}

static bool IsSequenceValid(uint16_t sequence) {
    return AudioCollection::Instance->HasSequenceNum(sequence);
}

static uint16_t GetReplacementSequence(uint16_t sequence) {
    return AudioCollection::Instance->GetReplacementSequence(sequence);
}

static bool HasExplicitAudioOverride(PlayState* play) {
    if (play->csCtx.state != CS_STATE_IDLE || Player_InCsMode(play)) {
        return true;
    }

    const uint16_t mainSeq = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);
    const uint16_t subSeq = func_800FA0B4(SEQ_PLAYER_BGM_SUB);
    const bool mainIsFieldLifecycle = HyruleFieldNightMusic_IsFieldLifecycleSequence(
        mainSeq, NA_BGM_FIELD_LOGIC, NA_BGM_NATURE_AMBIENCE, NA_BGM_DISABLED);
    const bool mainIsNightTrack =
        HyruleFieldNightMusic_IsNightSequencePlaying(sOwnsNightBgm, mainSeq, sNightPlaybackSeq);
    return (!mainIsFieldLifecycle && !mainIsNightTrack) || subSeq != NA_BGM_DISABLED;
}

void HyruleFieldNightMusic_Update(PlayState* play) {
    if (play == nullptr) {
        HyruleFieldNightMusic_Reset();
        return;
    }

    const uint16_t mainSeq = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);
    const uint16_t subSeq = func_800FA0B4(SEQ_PLAYER_BGM_SUB);
    const uint16_t fanfareSeq = func_800FA0B4(SEQ_PLAYER_FANFARE);
    const HyruleFieldNightMusicState state = {
        .enabled = CVarGetInteger(CVAR_AUDIO("HyruleFieldNightMusic"), 0) != 0,
        .inHyruleField = play->sceneNum == SCENE_HYRULE_FIELD,
        .isNight = gSaveContext.nightFlag != 0,
        .ownsNightBgm = sOwnsNightBgm,
        .nightBgmPlaying = HyruleFieldNightMusic_IsNightSequencePlaying(sOwnsNightBgm, mainSeq, sNightPlaybackSeq),
        .fanfarePlaying = fanfareSeq != NA_BGM_DISABLED,
        .explicitAudioOverride = HasExplicitAudioOverride(play),
    };
    const HyruleFieldNightMusicDecision decision = HyruleFieldNightMusic_Select(state);
    const HyruleFieldNightMusicDiagnosticSnapshot diagnostic = {
        .state = state,
        .decision = decision,
        .mainSequence = mainSeq,
        .subSequence = subSeq,
        .fanfareSequence = fanfareSeq,
        .nightPlaybackSequence = sNightPlaybackSeq,
    };
    if (HyruleFieldNightMusic_ShouldLogDiagnostic(sHasDiagnosticSnapshot ? &sDiagnosticSnapshot : nullptr,
                                                  diagnostic)) {
        SPDLOG_INFO("[HyruleNight] scene=0x{:02X} enabled={} inField={} night={} owns={} playing={} override={} "
                    "main=0x{:04X} sub=0x{:04X} fanfare=0x{:04X} cached=0x{:04X} decision={}",
                    play->sceneNum, state.enabled, state.inHyruleField, state.isNight, state.ownsNightBgm,
                    state.nightBgmPlaying, state.explicitAudioOverride, mainSeq, subSeq, fanfareSeq, sNightPlaybackSeq,
                    DecisionName(decision));
    }
    sDiagnosticSnapshot = diagnostic;
    sHasDiagnosticSnapshot = true;

    switch (decision) {
        case HyruleFieldNightMusicDecision::StartNight: {
            const uint16_t selected =
                static_cast<uint16_t>(CVarGetInteger(CVAR_AUDIO("HyruleFieldNightSequence"), kDefaultNightSequence));
            const uint16_t playbackSequence = HyruleFieldNightMusic_StartSequence(
                selected, kDefaultNightSequence, IsSequenceValid, GetReplacementSequence);
            SPDLOG_INFO("[HyruleNight] queue-start selected=0x{:04X} resolved=0x{:04X}", selected, playbackSequence);
            Audio_QueueResolvedSeqCmd(HyruleFieldNightMusic_GetPlaybackPlayer(), playbackSequence, 0x1E);
            Audio_RegisterNightBgm(playbackSequence);
            sOwnsNightBgm = true;
            break;
        }
        case HyruleFieldNightMusicDecision::RestoreNight:
            Audio_QueueResolvedSeqCmd(HyruleFieldNightMusic_GetPlaybackPlayer(),
                                      HyruleFieldNightMusic_RestoreSequence(), 0x1E);
            break;
        case HyruleFieldNightMusicDecision::StopNightRestoreDay: {
            if (HyruleFieldNightMusic_ShouldRestoreDaySequence(state)) {
                const uint16_t replacement = AudioCollection::Instance->GetReplacementSequence(NA_BGM_FIELD_LOGIC);
                const uint16_t daySequence = HyruleFieldNightMusic_ValidateSequence(
                    replacement, AudioCollection::Instance->HasSequenceNum(replacement), NA_BGM_FIELD_LOGIC);
                Audio_QueueResolvedSeqCmd(SEQ_PLAYER_BGM_MAIN, daySequence, 0x1E);
            }
            sOwnsNightBgm = false;
            Audio_RegisterNightBgm(NA_BGM_DISABLED);
            HyruleFieldNightMusic_ClearSequence();
            break;
        }
        case HyruleFieldNightMusicDecision::NoChange:
            break;
    }
}

void HyruleFieldNightMusic_Reset() {
    Audio_RegisterNightBgm(NA_BGM_DISABLED);
    sOwnsNightBgm = false;
    HyruleFieldNightMusic_ClearSequence();
    sHasDiagnosticSnapshot = false;
}

static void RegisterHyruleFieldNightMusic() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(
        []() { HyruleFieldNightMusic_Update(gPlayState); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() { HyruleFieldNightMusic_Reset(); });
}

// The updater reads both CVars each frame; re-registering on edits duplicates
// callbacks and can enqueue several starts before the audio thread processes one.
static RegisterShipInitFunc initFunc(RegisterHyruleFieldNightMusic);
#endif
