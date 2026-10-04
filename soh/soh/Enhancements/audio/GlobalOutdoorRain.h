#pragma once

enum class GlobalOutdoorRainDecision {
    NoChange,
    Start,
    Maintain,
    Stop,
};

enum class GlobalOutdoorRainSource {
    None,
    NativePlaced,
    EnhancedOutdoor,
    AuthoredScene,
};

enum class GlobalOutdoorRainMode {
    Persistent = 0,
    Intermittent = 1,
};

enum class GlobalOutdoorRainPhase {
    Dry,
    FadeIn,
    Sustain,
    FadeOut,
};

enum class GlobalOutdoorRainOvercastDecision {
    NoChange,
    Enable,
    Restore,
};

enum class GlobalOutdoorRainLightningDecision {
    NoChange,
    Enable,
    Restore,
};

struct GlobalOutdoorRainLightningState {
    bool thunderEnabled;
    bool enhancedRainActive;
    bool lightningAlreadyActive;
    bool ownsLightning;
};

struct GlobalOutdoorRainOvercastState {
    bool enabled;
    bool outdoors;
    bool compatibleSky;
    bool rainActive;
    bool ownsOvercast;
};

struct GlobalOutdoorRainCycle {
    GlobalOutdoorRainPhase phase;
    int framesRemaining;
    float intensity;
};

struct GlobalOutdoorRainState {
    bool enabled;
    bool outdoors;
    GlobalOutdoorRainSource source;
    bool rainAlreadyActive;
};

struct GlobalOutdoorRainColor {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
};

GlobalOutdoorRainDecision GlobalOutdoorRain_Select(const GlobalOutdoorRainState& state);
int GlobalOutdoorRain_ClampDensity(int density);
void GlobalOutdoorRain_AdvanceCycle(GlobalOutdoorRainCycle& cycle, GlobalOutdoorRainMode mode, bool active,
                                    int dryFrames, int sustainFrames, float fadeStep);
int GlobalOutdoorRain_ScaleDensity(int density, float intensity);
float GlobalOutdoorRain_ScaleVolume(float volume, float intensity);
GlobalOutdoorRainOvercastDecision GlobalOutdoorRain_SelectOvercast(const GlobalOutdoorRainOvercastState& state);
GlobalOutdoorRainLightningDecision GlobalOutdoorRain_SelectLightning(const GlobalOutdoorRainLightningState& state);
GlobalOutdoorRainColor GlobalOutdoorRain_SelectColor(GlobalOutdoorRainSource source,
                                                     GlobalOutdoorRainColor vanillaColor,
                                                     GlobalOutdoorRainColor configuredColor);

#ifndef GLOBAL_OUTDOOR_RAIN_TEST
struct PlayState;
void GlobalOutdoorRain_Update(PlayState* play);
void GlobalOutdoorRain_OnPlayDestroy();
void GlobalOutdoorRain_Reset();
#endif
