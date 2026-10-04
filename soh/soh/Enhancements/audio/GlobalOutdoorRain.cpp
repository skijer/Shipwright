#include "GlobalOutdoorRain.h"

GlobalOutdoorRainDecision GlobalOutdoorRain_Select(const GlobalOutdoorRainState& state) {
    if (state.source == GlobalOutdoorRainSource::NativePlaced ||
        state.source == GlobalOutdoorRainSource::AuthoredScene) {
        return GlobalOutdoorRainDecision::NoChange;
    }
    if (!state.enabled || !state.outdoors) {
        return state.source == GlobalOutdoorRainSource::EnhancedOutdoor ? GlobalOutdoorRainDecision::Stop
                                                                        : GlobalOutdoorRainDecision::NoChange;
    }
    if (state.source == GlobalOutdoorRainSource::EnhancedOutdoor) {
        return GlobalOutdoorRainDecision::Maintain;
    }
    // Visual rain density can survive a room transition after its audible
    // owner disappears. With no explicit native owner, reacquire the enhanced
    // loop even when the scene still reports rain particles.
    return GlobalOutdoorRainDecision::Start;
}

int GlobalOutdoorRain_ClampDensity(int density) {
    if (density < 0) {
        return 0;
    }
    return density > 64 ? 64 : density;
}

static float GlobalOutdoorRain_ClampIntensity(float intensity) {
    if (intensity < 0.0f) {
        return 0.0f;
    }
    return intensity > 1.0f ? 1.0f : intensity;
}

void GlobalOutdoorRain_AdvanceCycle(GlobalOutdoorRainCycle& cycle, GlobalOutdoorRainMode mode, bool active,
                                    int dryFrames, int sustainFrames, float fadeStep) {
    fadeStep = GlobalOutdoorRain_ClampIntensity(fadeStep);

    if (!active) {
        if (cycle.phase != GlobalOutdoorRainPhase::Dry && cycle.phase != GlobalOutdoorRainPhase::FadeOut) {
            cycle.phase = GlobalOutdoorRainPhase::FadeOut;
            return;
        }
    } else if (mode == GlobalOutdoorRainMode::Persistent && cycle.phase == GlobalOutdoorRainPhase::Dry) {
        cycle.phase = GlobalOutdoorRainPhase::FadeIn;
        return;
    }

    switch (cycle.phase) {
        case GlobalOutdoorRainPhase::Dry:
            cycle.intensity = 0.0f;
            if (active && mode == GlobalOutdoorRainMode::Intermittent && --cycle.framesRemaining <= 0) {
                cycle.phase = GlobalOutdoorRainPhase::FadeIn;
            }
            break;
        case GlobalOutdoorRainPhase::FadeIn:
            if (!active) {
                cycle.phase = GlobalOutdoorRainPhase::FadeOut;
                break;
            }
            cycle.intensity = GlobalOutdoorRain_ClampIntensity(cycle.intensity + fadeStep);
            if (cycle.intensity >= 1.0f) {
                cycle.phase = GlobalOutdoorRainPhase::Sustain;
                cycle.framesRemaining = sustainFrames;
            }
            break;
        case GlobalOutdoorRainPhase::Sustain:
            cycle.intensity = 1.0f;
            if (!active) {
                cycle.phase = GlobalOutdoorRainPhase::FadeOut;
            } else if (mode == GlobalOutdoorRainMode::Intermittent && --cycle.framesRemaining <= 0) {
                cycle.phase = GlobalOutdoorRainPhase::FadeOut;
            }
            break;
        case GlobalOutdoorRainPhase::FadeOut:
            cycle.intensity = GlobalOutdoorRain_ClampIntensity(cycle.intensity - fadeStep);
            if (cycle.intensity <= 0.0f) {
                cycle.phase = GlobalOutdoorRainPhase::Dry;
                cycle.framesRemaining = dryFrames;
            }
            break;
    }
}

int GlobalOutdoorRain_ScaleDensity(int density, float intensity) {
    return GlobalOutdoorRain_ClampDensity(
        static_cast<int>(density * GlobalOutdoorRain_ClampIntensity(intensity) + 0.5f));
}

float GlobalOutdoorRain_ScaleVolume(float volume, float intensity) {
    return volume * GlobalOutdoorRain_ClampIntensity(intensity);
}

GlobalOutdoorRainOvercastDecision GlobalOutdoorRain_SelectOvercast(const GlobalOutdoorRainOvercastState& state) {
    const bool shouldOwn = state.enabled && state.outdoors && state.compatibleSky && state.rainActive;
    if (shouldOwn && !state.ownsOvercast) {
        return GlobalOutdoorRainOvercastDecision::Enable;
    }
    if (!shouldOwn && state.ownsOvercast) {
        return GlobalOutdoorRainOvercastDecision::Restore;
    }
    return GlobalOutdoorRainOvercastDecision::NoChange;
}

GlobalOutdoorRainLightningDecision GlobalOutdoorRain_SelectLightning(const GlobalOutdoorRainLightningState& state) {
    const bool shouldOwn = state.thunderEnabled && state.enhancedRainActive;
    if (shouldOwn && !state.lightningAlreadyActive) {
        return GlobalOutdoorRainLightningDecision::Enable;
    }
    if (!shouldOwn && state.ownsLightning) {
        return GlobalOutdoorRainLightningDecision::Restore;
    }
    return GlobalOutdoorRainLightningDecision::NoChange;
}

GlobalOutdoorRainColor GlobalOutdoorRain_SelectColor(GlobalOutdoorRainSource source,
                                                     GlobalOutdoorRainColor vanillaColor,
                                                     GlobalOutdoorRainColor configuredColor) {
    return source == GlobalOutdoorRainSource::EnhancedOutdoor ? configuredColor : vanillaColor;
}

#ifndef GLOBAL_OUTDOOR_RAIN_TEST
#include <cstdio>
#include <algorithm>
#include <unordered_map>
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "GlobalOutdoorRainBridge.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "code/concurrent_weather_audio.h"
#include "WeatherSamplePlayer.h"
#include "libultraship/bridge/consolevariablebridge.h"
#include "z64.h"

extern "C" {
#include "functions.h"
#include "sequence.h"
#include "sfx.h"
#include "variables.h"
extern PlayState* gPlayState;
}

static constexpr int kRainDensity = 25;
// This feature advances from OnGameFrameUpdate at OOT's native 20 Hz logic
// rate, independently of the renderer's interpolation rate.
static constexpr int kFramesPerSecond = 20;
static constexpr float kFadeStep = 1.0f / kFramesPerSecond;
static GlobalOutdoorRainSource sRainSource = GlobalOutdoorRainSource::None;
static int sLastMode = -1;
static GlobalOutdoorRainCycle sCycle = { GlobalOutdoorRainPhase::Dry, 0, 0.0f };
static bool sOwnsFallbackLoop = false;
static bool sOwnsOvercast = false;
static bool sOwnsLightning = false;
static void PlayRainLoop(float intensity);
struct RainRequest {
    int density;
    bool thunder;
};
static std::unordered_map<const void*, RainRequest> sNativeRequests;
static PlayState* sOwnerPlay = nullptr;
static RainRequest sSceneRequest = {};
static bool sOwnsDensity = false;
static bool sScriptedRain = false;
static int sScriptedDensity = 0;
static bool sSceneDiagnostics = false;
static int sPreviousRoom = -128;
static int sPreviousOwnerCount = -1;
static int sPreviousSource = -1;
static int sPreviousDrawGate = -1;
static bool sPreviousScript = false;
static bool DiagnosticsEnabled() {
    return sSceneDiagnostics || CVarGetInteger(CVAR_AUDIO("WeatherAudioDiagnostics"), 0);
}
static RainRequest RequestedRain() {
    RainRequest result = sSceneRequest;
    for (const auto& [owner, request] : sNativeRequests) {
        result.density = std::max(result.density, request.density);
        result.thunder |= request.thunder;
    }
    return result;
}
static void BindPlay(PlayState* play) {
    if (play != sOwnerPlay) {
        GlobalOutdoorRain_OnPlayDestroy();
        sOwnerPlay = play;
    }
}

extern "C" int32_t GlobalOutdoorRain_GetRenderColor(uint8_t* red, uint8_t* green, uint8_t* blue) {
    if (sRainSource != GlobalOutdoorRainSource::EnhancedOutdoor || red == nullptr || green == nullptr ||
        blue == nullptr) {
        return false;
    }

    const Color_RGB8 configuredColor = CVarGetColor24(CVAR_AUDIO("GlobalOutdoorRainColor.Value"), { 150, 255, 255 });
    *red = configuredColor.r;
    *green = configuredColor.g;
    *blue = configuredColor.b;
    return true;
}

extern "C" void GlobalOutdoorRain_BeginScene(PlayState* play, int32_t density, int32_t thunder, int32_t diagnostics) {
    // Explicit even when the allocator reuses the outgoing PlayState address.
    GlobalOutdoorRain_OnPlayDestroy();
    sOwnerPlay = play;
    sSceneRequest = { GlobalOutdoorRain_ClampDensity(density), thunder != 0 };
    sSceneDiagnostics = diagnostics != 0;
}

extern "C" void GlobalOutdoorRain_SetNativeRequest(PlayState* play, const void* owner, int32_t density,
                                                   int32_t thunder) {
    if (play == nullptr || owner == nullptr)
        return;
    if (density <= 0 && play != sOwnerPlay)
        return;
    BindPlay(play);
    density = GlobalOutdoorRain_ClampDensity(density);
    if (density == 0)
        sNativeRequests.erase(owner);
    else
        sNativeRequests[owner] = { density, thunder != 0 };
}

extern "C" void GlobalOutdoorRain_SetScriptedRain(PlayState* play, int32_t density) {
    if (play == nullptr)
        return;
    BindPlay(play);
    sScriptedRain = true;
    sScriptedDensity = GlobalOutdoorRain_ClampDensity(density);
    play->envCtx.unk_EE[0] = sScriptedDensity;
}

extern "C" int32_t GlobalOutdoorRain_HasRainIntent() {
    return sOwnerPlay != nullptr && !sScriptedRain &&
           (RequestedRain().density > 0 ||
            (sRainSource == GlobalOutdoorRainSource::EnhancedOutdoor && sCycle.intensity > 0.0f));
}

static void UpdateLightning(PlayState* play, bool thunderEnabled, bool enhancedRainActive) {
    const GlobalOutdoorRainLightningState state = {
        .thunderEnabled = thunderEnabled,
        .enhancedRainActive = enhancedRainActive,
        .lightningAlreadyActive = play->envCtx.lightningMode != LIGHTNING_MODE_OFF,
        .ownsLightning = sOwnsLightning,
    };

    switch (GlobalOutdoorRain_SelectLightning(state)) {
        case GlobalOutdoorRainLightningDecision::Enable:
            play->envCtx.lightningMode = LIGHTNING_MODE_ON;
            sOwnsLightning = true;
            break;
        case GlobalOutdoorRainLightningDecision::Restore:
            if (play->envCtx.lightningMode == LIGHTNING_MODE_ON) {
                // OFF stops the engine's flash state machine. Let an active
                // strike finish or its flash/ambient boost would remain stuck.
                play->envCtx.lightningMode =
                    gLightningStrike.state == LIGHTNING_STRIKE_WAIT ? LIGHTNING_MODE_OFF : LIGHTNING_MODE_LAST;
            }
            sOwnsLightning = false;
            break;
        case GlobalOutdoorRainLightningDecision::NoChange:
            break;
    }
}

static void UpdateOvercast(PlayState* play, bool enabled, bool outdoors) {
    if (sOwnsOvercast && play->envCtx.gloomySkyMode != 1) {
        // A cutscene/sky transition finished our presentation independently.
        sOwnsOvercast = false;
    }
    const bool compatibleSky =
        play->skyboxId == SKYBOX_NORMAL_SKY && !play->envCtx.skyboxDisabled && play->csCtx.state == CS_STATE_IDLE;
    const GlobalOutdoorRainOvercastState state = {
        .enabled = enabled,
        .outdoors = outdoors,
        .compatibleSky = compatibleSky,
        .rainActive =
            sRainSource == GlobalOutdoorRainSource::NativePlaced ||
            sRainSource == GlobalOutdoorRainSource::AuthoredScene ||
            (sRainSource == GlobalOutdoorRainSource::EnhancedOutdoor && sCycle.phase != GlobalOutdoorRainPhase::Dry),
        .ownsOvercast = sOwnsOvercast,
    };

    switch (GlobalOutdoorRain_SelectOvercast(state)) {
        case GlobalOutdoorRainOvercastDecision::Enable:
            if (play->envCtx.gloomySkyMode == 0) {
                play->envCtx.gloomySkyMode = 1;
                sOwnsOvercast = true;
            }
            break;
        case GlobalOutdoorRainOvercastDecision::Restore:
            if (play->envCtx.gloomySkyMode == 1) {
                play->envCtx.gloomySkyMode = 2;
            }
            sOwnsOvercast = false;
            break;
        case GlobalOutdoorRainOvercastDecision::NoChange:
            break;
    }
}

static int RandomDryFrames() {
    return Rand_S16Offset(20 * kFramesPerSecond, 6 * kFramesPerSecond);
}

static int RandomInitialDryFrames() {
    return Rand_S16Offset(2 * kFramesPerSecond, 3 * kFramesPerSecond);
}

static int RandomSustainFrames() {
    return Rand_S16Offset(8 * kFramesPerSecond, 7 * kFramesPerSecond);
}

static void PlayRainLoop(float intensity) {
    const float peakVolume =
        ConcurrentWeatherAudio_ClampPercent(CVarGetInteger(CVAR_AUDIO("ProximityWeatherRainVolume"), 50)) / 100.0f;
    const float rainVolume = GlobalOutdoorRain_ScaleVolume(peakVolume, intensity);
    switch (ConcurrentWeatherAudio_SelectRainAction(sOwnsFallbackLoop, sRainSource != GlobalOutdoorRainSource::None,
                                                    Audio_IsNatureRainEnabled(), rainVolume)) {
        case CONCURRENT_WEATHER_RAIN_SET_LOOP:
            WeatherSamplePlayer_SetLoop("audio/samples/Rainfall_META", rainVolume);
            sOwnsFallbackLoop = true;
            break;
        case CONCURRENT_WEATHER_RAIN_STOP_LOOP:
            WeatherSamplePlayer_SetLoop(nullptr, 0.0f);
            sOwnsFallbackLoop = false;
            break;
        case CONCURRENT_WEATHER_RAIN_NO_CHANGE:
            break;
    }
}

extern "C" void GlobalOutdoorRain_Resolve(PlayState* play) {
    if (play == nullptr)
        return;
    BindPlay(play);
    if (play->csCtx.state == CS_STATE_IDLE)
        sScriptedRain = false;
    const RainRequest request = RequestedRain();
    const int before = play->envCtx.unk_EE[0];
    const bool repair = sOwnsDensity && !sScriptedRain && request.density > 0 && before != request.density;
    if (sScriptedRain) {
        // Cutscene rain/clear commands own the target until their cutscene ends.
        // In particular, MISC 18 needs the particle count to reach zero.
        play->envCtx.unk_EE[0] = sScriptedDensity;
        PlayRainLoop(0.0f);
        sRainSource = GlobalOutdoorRainSource::None;
        sOwnsDensity = false;
        // Keep the ownership token while scripts control rain. Reconcile it
        // when the cutscene ends, even if its final native owner was destroyed.
    } else if (request.density > 0) {
        sRainSource =
            sSceneRequest.density > 0 ? GlobalOutdoorRainSource::AuthoredScene : GlobalOutdoorRainSource::NativePlaced;
        play->envCtx.unk_EE[0] = request.density;
        sOwnsDensity = true;
        if (Audio_IsNatureRainEnabled())
            Audio_SetNatureAmbienceChannelIO(NATURE_CHANNEL_RAIN, CHANNEL_IO_PORT_1, 0);
        PlayRainLoop(1.0f);
        UpdateLightning(play, CVarGetInteger(CVAR_AUDIO("ProximityWeatherThunder"), 1) != 0, request.thunder);
    } else if (sOwnsDensity) {
        play->envCtx.unk_EE[0] = 0;
        sOwnsDensity = false;
        PlayRainLoop(0.0f);
        sRainSource = GlobalOutdoorRainSource::None;
        UpdateLightning(play, false, false);
    }
    if (DiagnosticsEnabled() &&
        (repair || sPreviousRoom != play->roomCtx.curRoom.num ||
         sPreviousOwnerCount != static_cast<int>(sNativeRequests.size()) ||
         sPreviousSource != static_cast<int>(sRainSource) || sPreviousScript != sScriptedRain)) {
        char message[320];
        std::snprintf(message, sizeof(message),
                      "[rain-probe] scene=%d room=%d source=%d owners=%zu sceneDensity=%d target=%d current=%d "
                      "before=%d repaired=%d scripted=%d thunder=%d indoors=%d",
                      play->sceneNum, play->roomCtx.curRoom.num, static_cast<int>(sRainSource), sNativeRequests.size(),
                      sSceneRequest.density, play->envCtx.unk_EE[0], play->envCtx.unk_EE[1], before, repair,
                      sScriptedRain, play->envCtx.lightningMode, play->envCtx.indoors);
        GlobalOutdoorRain_Log(message);
        sPreviousRoom = play->roomCtx.curRoom.num;
        sPreviousOwnerCount = static_cast<int>(sNativeRequests.size());
        sPreviousSource = static_cast<int>(sRainSource);
        sPreviousScript = sScriptedRain;
    }
}

extern "C" void GlobalOutdoorRain_RecordDraw(PlayState* play, int32_t underwater, int32_t suppressed, float cameraY,
                                             float waterY, float viewY) {
    if (play == nullptr || !DiagnosticsEnabled())
        return;
    const int gate = (underwater ? 1 : 0) | (suppressed ? 2 : 0) | (play->envCtx.unk_EE[1] == 0 ? 4 : 0);
    if (gate == sPreviousDrawGate)
        return;
    sPreviousDrawGate = gate;
    char message[256];
    std::snprintf(message, sizeof(message),
                  "[rain-probe] draw scene=%d room=%d target=%d current=%d underwater=%d suppression=%d "
                  "mainEyeY=%.2f waterY=%.2f viewEyeY=%.2f",
                  play->sceneNum, play->roomCtx.curRoom.num, play->envCtx.unk_EE[0], play->envCtx.unk_EE[1], underwater,
                  suppressed, cameraY, waterY, viewY);
    GlobalOutdoorRain_Log(message);
}

void GlobalOutdoorRain_Update(PlayState* play) {
    if (play == nullptr) {
        GlobalOutdoorRain_Reset();
        return;
    }

    GlobalOutdoorRain_Resolve(play);
    if (sScriptedRain)
        return;
    const bool enabled = CVarGetInteger(CVAR_AUDIO("GlobalOutdoorRain"), 0) != 0;
    const bool outdoors = play->envCtx.indoors == 0;
    const bool overcastEnabled = CVarGetInteger(CVAR_AUDIO("GlobalOutdoorRainOvercast"), 1) != 0;
    const bool thunderEnabled = CVarGetInteger(CVAR_AUDIO("ProximityWeatherThunder"), 1) != 0;
    const int modeValue = CVarGetInteger(CVAR_AUDIO("GlobalOutdoorRainMode"), 0);
    const GlobalOutdoorRainMode mode = modeValue == static_cast<int>(GlobalOutdoorRainMode::Intermittent)
                                           ? GlobalOutdoorRainMode::Intermittent
                                           : GlobalOutdoorRainMode::Persistent;
    const GlobalOutdoorRainState state = {
        .enabled = enabled,
        .outdoors = outdoors,
        .source = sRainSource,
        .rainAlreadyActive = play->envCtx.unk_EE[0] != 0,
    };

    const GlobalOutdoorRainDecision decision = GlobalOutdoorRain_Select(state);
    if (modeValue != sLastMode) {
        sLastMode = modeValue;
        sCycle = { GlobalOutdoorRainPhase::Dry,
                   mode == GlobalOutdoorRainMode::Intermittent ? RandomInitialDryFrames() : 0, 0.0f };
        if (sRainSource == GlobalOutdoorRainSource::EnhancedOutdoor && play->envCtx.unk_EE[0] <= kRainDensity) {
            play->envCtx.unk_EE[0] = 0;
        }
    }

    switch (decision) {
        case GlobalOutdoorRainDecision::Start:
            sRainSource = GlobalOutdoorRainSource::EnhancedOutdoor;
            break;
        case GlobalOutdoorRainDecision::Maintain:
            break;
        case GlobalOutdoorRainDecision::Stop:
            break;
        case GlobalOutdoorRainDecision::NoChange:
            break;
    }

    if (sRainSource != GlobalOutdoorRainSource::EnhancedOutdoor) {
        if (!sOwnsDensity)
            UpdateLightning(play, thunderEnabled, false);
        UpdateOvercast(play, overcastEnabled, outdoors);
        return;
    }

    const GlobalOutdoorRainPhase previousPhase = sCycle.phase;
    GlobalOutdoorRain_AdvanceCycle(sCycle, mode, enabled && outdoors, 20 * kFramesPerSecond, 8 * kFramesPerSecond,
                                   kFadeStep);
    if (mode == GlobalOutdoorRainMode::Intermittent && previousPhase != sCycle.phase) {
        if (sCycle.phase == GlobalOutdoorRainPhase::Dry) {
            sCycle.framesRemaining = RandomDryFrames();
        } else if (sCycle.phase == GlobalOutdoorRainPhase::Sustain) {
            sCycle.framesRemaining = RandomSustainFrames();
        }
    }
    play->envCtx.unk_EE[0] = GlobalOutdoorRain_ScaleDensity(kRainDensity, sCycle.intensity);
    // Zero intensity is also an audio transition (intermittent dry/mode reset).
    PlayRainLoop(sCycle.intensity);
    UpdateLightning(play, thunderEnabled, sCycle.phase != GlobalOutdoorRainPhase::Dry);
    UpdateOvercast(play, overcastEnabled, outdoors);
    if ((!enabled || !outdoors) && sCycle.phase == GlobalOutdoorRainPhase::Dry) {
        sRainSource = GlobalOutdoorRainSource::None;
    }
}

void GlobalOutdoorRain_OnPlayDestroy() {
    sNativeRequests.clear();
    sOwnerPlay = nullptr;
    sSceneRequest = {};
    sOwnsDensity = false;
    sScriptedRain = false;
    sSceneDiagnostics = false;
    sPreviousRoom = -128;
    sPreviousOwnerCount = sPreviousSource = sPreviousDrawGate = -1;
    sPreviousScript = false;
    // Enhanced rain is global and may continue across compatible outdoor
    // scenes. Placed proximity rain belongs to the outgoing PlayState and must
    // release its private loop before the destination scene is constructed.
    if (sRainSource == GlobalOutdoorRainSource::NativePlaced || sRainSource == GlobalOutdoorRainSource::AuthoredScene) {
        PlayRainLoop(0.0f);
        sRainSource = GlobalOutdoorRainSource::None;
    }
    // Sky state is PlayState-owned, so the incoming scene must claim its own
    // overcast transition even while the rain loop itself remains continuous.
    sOwnsOvercast = false;
    sOwnsLightning = false;
}

void GlobalOutdoorRain_Reset() {
    GlobalOutdoorRain_OnPlayDestroy();
    sRainSource = GlobalOutdoorRainSource::None;
    sLastMode = -1;
    sCycle = { GlobalOutdoorRainPhase::Dry, 0, 0.0f };
    sOwnsOvercast = false;
    sOwnsLightning = false;
    PlayRainLoop(0.0f);
}

static void RegisterGlobalOutdoorRain() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(
        []() { GlobalOutdoorRain_Update(gPlayState); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>(
        []() { GlobalOutdoorRain_OnPlayDestroy(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) { GlobalOutdoorRain_Reset(); });
}

static RegisterShipInitFunc initFunc(RegisterGlobalOutdoorRain);
#endif
