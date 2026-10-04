#include "test_require.h"
#include <cstring>
#include "weather_audio_stubs/weather_runtime.h"
#include "weather_audio_fixture.h"
#include "weather_sfx_engine_fixture.h"
#include "code/concurrent_weather_audio.h"
#include "soh/Enhancements/audio/GlobalOutdoorRain.h"
#include "soh/Enhancements/audio/GlobalOutdoorRainBridge.h"
#include "soh/Enhancements/audio/WeatherSamplePlayer.h"
#include "soh/ShipInit.hpp"

static int sEnabled = 1;
static int sMode = 0;
static int sRainVolume = 100;
static int sOvercast = 1;
static int sThunder = 1;
static int sProbeLogs = 0;
static ConcurrentWeatherAudioState sNatureWeather = {};
static int sNatureRainWrites = 0;
GameInteractor* GameInteractor::Instance = nullptr;
extern "C" {
PlayState* gPlayState = nullptr;
LightningStrike gLightningStrike = {};
void GlobalOutdoorRain_Log(const char*) {
    ++sProbeLogs;
}
SoundFontSample* ResourceMgr_LoadAudioSample(const char* path) {
    return WeatherAudioFixture(path);
}
int32_t CVarGetInteger(const char* name, int32_t fallback) {
    if (std::strcmp(name, "gAudioEditor.GlobalOutdoorRain") == 0)
        return sEnabled;
    if (std::strcmp(name, "gAudioEditor.GlobalOutdoorRainMode") == 0)
        return sMode;
    if (std::strcmp(name, "gAudioEditor.ProximityWeatherRainVolume") == 0)
        return sRainVolume;
    if (std::strcmp(name, "gAudioEditor.GlobalOutdoorRainOvercast") == 0)
        return sOvercast;
    if (std::strcmp(name, "gAudioEditor.ProximityWeatherThunder") == 0)
        return sThunder;
    return fallback;
}
Color_RGB8 CVarGetColor24(const char*, Color_RGB8 fallback) {
    return fallback;
}
int16_t Rand_S16Offset(int16_t base, int16_t) {
    return base;
}
// The sequence-player boundary is fixed to native ambience (0x0001).
// Run the real policy used by code_800EC960.c, not a weather eligibility mock.
uint8_t Audio_IsNatureRainEnabled(void) {
    return !ConcurrentWeatherAudio_ShouldPlayRainSfx(&sNatureWeather, 0x0001, 0x0001);
}
void Audio_SetNatureAmbienceChannelIO(uint8_t channel, uint8_t port, uint8_t value) {
    ++sNatureRainWrites;
    ConcurrentWeatherAudio_TrackNatureChannel(&sNatureWeather, channel, port, value, 0x0E, 0x0F, 1);
}
}

static void SetNatureRain(bool enabled) {
    ConcurrentWeatherAudio_TrackNatureChannel(&sNatureWeather, 0x0E, 1, enabled, 0x0E, 0x0F, 1);
}

static void RequireMixFrame(int16_t expected) {
    WeatherSfxEngine_RequirePlaying();
    int16_t buffer[2] = { 100, -100 };
    WeatherSamplePlayer_Mix(buffer, 1);
    REQUIRE(buffer[0] - 100 == buffer[1] + 100);
    REQUIRE(buffer[0] == expected);
    WeatherSfxEngine_Refresh();
}

static void TestSimultaneousEngineAndWeatherVoices() {
    // Catch weather suppressing real requests or consuming/muting ordinary
    // engine voices. Submit AFTER rain and both thunder layers are active.
    for (bool nativeRain : { false, true }) {
        WeatherSamplePlayer_Init();
        GlobalOutdoorRain_Reset();
        WeatherSfxEngine_Reset();
        SetNatureRain(nativeRain);
        PlayState play = {};
        for (int frame = 0; frame < 62; ++frame)
            GlobalOutdoorRain_Update(&play);
        const bool thunderEligible = ConcurrentWeatherAudio_ShouldPlayThunderSfx(&sNatureWeather, 0x0001, 0x0001);
        REQUIRE(thunderEligible); // rain channel alone must not suppress thunder
        const bool thunderStarted = WeatherSamplePlayer_Play("audio/samples/Low Thunder_META", 1.0f);
        const bool lightningStarted = WeatherSamplePlayer_Play("audio/samples/Lightning_META", 1.0f);
        REQUIRE(thunderStarted);
        REQUIRE(lightningStarted);
        WeatherSfxEngine_Start();
        RequireMixFrame(nativeRain ? 105 : 106);
        SetNatureRain(!nativeRain);
        GlobalOutdoorRain_Update(&play);
        RequireMixFrame(nativeRain ? 106 : 105);
        GlobalOutdoorRain_Reset();
        RequireMixFrame(105);
    }
    SetNatureRain(false);
}

static void TestDenseFlameHubDoesNotStealNaviOrThunder() {
    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    WeatherSfxEngine_Reset();
    SetNatureRain(false);
    PlayState play = {};
    for (int frame = 0; frame < 62; ++frame)
        GlobalOutdoorRain_Update(&play);
    REQUIRE(WeatherSamplePlayer_Play("audio/samples/Low Thunder_META", 1.0f));
    REQUIRE(WeatherSamplePlayer_Play("audio/samples/Lightning_META", 1.0f));
    WeatherSfxEngine_StartDenseFlameHub();
    RequireMixFrame(106);
}

static void TestNaviEmergenceSurvivesEnvironmentBankSaturation() {
    WeatherSfxEngine_Reset();
    WeatherSfxEngine_StartSaturatedEnvironment();
    REQUIRE(WeatherSfxEngine_IsNaviPlaying());
}

static void TestPlacedWeatherRainUsesPrivateLoop() {
    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    WeatherSfxEngine_Reset();
    WeatherSfxEngine_StartDenseFlameHub();
    SetNatureRain(false);
    PlayState play = {};

    GlobalOutdoorRain_SetNativeRequest(&play, &play, 25, 0);
    GlobalOutdoorRain_Resolve(&play);
    RequireMixFrame(101);

    GlobalOutdoorRain_SetNativeRequest(&play, &play, 0, 0);
    GlobalOutdoorRain_Resolve(&play);
    RequireMixFrame(100);

    // Placed weather relinquishes the unscaled native rain channel and uses
    // the same live slider-controlled private loop as enhanced outdoor rain.
    SetNatureRain(true);
    sNatureRainWrites = 0;
    GlobalOutdoorRain_SetNativeRequest(&play, &play, 25, 0);
    GlobalOutdoorRain_Resolve(&play);
    REQUIRE(sNatureRainWrites == 1);
    RequireMixFrame(101);
    sRainVolume = 0;
    GlobalOutdoorRain_SetNativeRequest(&play, &play, 25, 0);
    GlobalOutdoorRain_Resolve(&play);
    RequireMixFrame(100);
    sRainVolume = 100;
    GlobalOutdoorRain_SetNativeRequest(&play, &play, 0, 0);
    GlobalOutdoorRain_Resolve(&play);
    SetNatureRain(false);
}

static void TestRemainingNativeOwnerKeepsRain() {
    for (bool releaseFirst : { false, true }) {
        WeatherSamplePlayer_Init();
        GlobalOutdoorRain_Reset();
        sEnabled = 0;
        int a, b;
        PlayState play = {};
        GlobalOutdoorRain_SetNativeRequest(&play, &a, 30, true);
        GlobalOutdoorRain_SetNativeRequest(&play, &b, 25, false);
        GlobalOutdoorRain_Resolve(&play);
        REQUIRE(play.envCtx.unk_EE[0] == 30);
        REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_ON);
        if (releaseFirst)
            GlobalOutdoorRain_SetNativeRequest(&play, &a, 0, false);
        GlobalOutdoorRain_SetNativeRequest(&play, &b, 25, false);
        if (!releaseFirst)
            GlobalOutdoorRain_SetNativeRequest(&play, &a, 0, false);
        // Another environment writer cleared the target; the surviving owner
        // must repair it before particle integration, independent of order.
        play.envCtx.unk_EE[0] = 0;
        GlobalOutdoorRain_Resolve(&play);
        REQUIRE(play.envCtx.unk_EE[0] == 25);
        REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_OFF);
        REQUIRE(GlobalOutdoorRain_HasRainIntent());
        for (int frame = 0; frame < 120; ++frame)
            GlobalOutdoorRain_Update(&play);
        REQUIRE(play.envCtx.unk_EE[0] == 25);
        GlobalOutdoorRain_SetNativeRequest(&play, &b, 0, false);
        GlobalOutdoorRain_Resolve(&play);
        REQUIRE(play.envCtx.unk_EE[0] == 0);
        REQUIRE(!GlobalOutdoorRain_HasRainIntent());
    }
    GlobalOutdoorRain_Reset();
    sEnabled = 1;
}

static void TestAuthoredSceneRain() {
    for (int enabled : { 0, 1 })
        for (int mode : { 0, 1 }) {
            GlobalOutdoorRain_Reset();
            sEnabled = enabled;
            sMode = mode;
            PlayState play = {};
            play.sceneNum = 0x5B;
            GlobalOutdoorRain_BeginScene(&play, 25, true, false);
            // No native actors: direct entry, room changes, and a whole intermittent
            // cycle must all leave authored rain active.
            for (int room : { 0, 3, 4, 7, 9, 10, 8, 10 }) {
                play.roomCtx.curRoom.num = room;
                for (int frame = 0; frame < 1500; ++frame)
                    GlobalOutdoorRain_Update(&play);
                REQUIRE(play.envCtx.unk_EE[0] == 25);
            }
            sThunder = 0;
            GlobalOutdoorRain_Update(&play);
            REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_OFF);
            sThunder = 1;
            GlobalOutdoorRain_Update(&play);
            REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_ON);
            REQUIRE(GlobalOutdoorRain_HasRainIntent());
            play.csCtx.state = 1;
            GlobalOutdoorRain_SetScriptedRain(&play, 0);
            for (int frame = 0; frame < 200; ++frame)
                GlobalOutdoorRain_Update(&play);
            REQUIRE(play.envCtx.unk_EE[0] == 0);
            REQUIRE(!GlobalOutdoorRain_HasRainIntent());
            GlobalOutdoorRain_SetScriptedRain(&play, 20);
            GlobalOutdoorRain_Update(&play);
            REQUIRE(play.envCtx.unk_EE[0] == 20);
            play.csCtx.state = CS_STATE_IDLE;
            GlobalOutdoorRain_Update(&play);
            REQUIRE(play.envCtx.unk_EE[0] == 25);
            // Same address after reset/age change must discard every old owner.
            int oldActor;
            GlobalOutdoorRain_SetNativeRequest(&play, &oldActor, 30, true);
            GlobalOutdoorRain_BeginScene(&play, 25, true, true);
            GlobalOutdoorRain_Update(&play);
            REQUIRE(play.envCtx.unk_EE[0] == 25);
            sProbeLogs = 0;
            GlobalOutdoorRain_RecordDraw(&play, 1, 0, -100, 0, -100);
            GlobalOutdoorRain_RecordDraw(&play, 1, 0, -101, 0, -101);
            REQUIRE(sProbeLogs == 1); // no per-frame spam as the camera moves
            GlobalOutdoorRain_RecordDraw(&play, 0, 0, 10, 0, 10);
            REQUIRE(sProbeLogs == 2);
            REQUIRE(play.envCtx.unk_EE[0] == 25);
            sEnabled = 0;
            GlobalOutdoorRain_OnPlayDestroy();
            play = {};
            GlobalOutdoorRain_BeginScene(&play, 0, false, false);
            GlobalOutdoorRain_Update(&play);
            REQUIRE(play.envCtx.unk_EE[0] == 0);
            REQUIRE(!GlobalOutdoorRain_HasRainIntent());
        }
    GlobalOutdoorRain_Reset();
    sEnabled = 1;
    sMode = 0;
}

static void TestScriptedClearReleasesOwnedLightning() {
    GlobalOutdoorRain_Reset();
    sEnabled = 0;
    PlayState play = {};
    int owner;
    GlobalOutdoorRain_SetNativeRequest(&play, &owner, 25, true);
    GlobalOutdoorRain_Resolve(&play);
    REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_ON);
    play.csCtx.state = 1;
    GlobalOutdoorRain_SetScriptedRain(&play, 0);
    GlobalOutdoorRain_Resolve(&play);
    GlobalOutdoorRain_SetNativeRequest(&play, &owner, 0, false);
    play.csCtx.state = CS_STATE_IDLE;
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 0);
    REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_OFF);
    GlobalOutdoorRain_Reset();
    sEnabled = 1;
}

static void TestSkyOwnershipAfterScriptedRestore() {
    GlobalOutdoorRain_Reset();
    PlayState play = {};
    play.skyboxId = SKYBOX_NORMAL_SKY;
    GlobalOutdoorRain_BeginScene(&play, 25, true, false);
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.gloomySkyMode == 1);
    play.csCtx.state = 1;
    GlobalOutdoorRain_SetScriptedRain(&play, 0);
    GlobalOutdoorRain_Update(&play);
    // The cutscene's own sky restoration has completed.
    play.envCtx.gloomySkyMode = 0;
    play.csCtx.state = CS_STATE_IDLE;
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 25);
    REQUIRE(play.envCtx.gloomySkyMode == 1);
    GlobalOutdoorRain_Reset();
}

int main() {
    TestSkyOwnershipAfterScriptedRestore();
    TestScriptedClearReleasesOwnedLightning();
    TestRemainingNativeOwnerKeepsRain();
    TestAuthoredSceneRain();
    // Live color changes are read during rendering and must never stack update hooks.
    REQUIRE(RegisterShipInitFunc::updatePathCount == 0);
    TestSimultaneousEngineAndWeatherVoices();
    TestDenseFlameHubDoesNotStealNaviOrThunder();
    TestNaviEmergenceSurvivesEnvironmentBankSaturation();
    TestPlacedWeatherRainUsesPrivateLoop();

    // Overcast follows the enhanced rain lifecycle, stays off fixed skies,
    // and is reclaimed by the incoming PlayState without restarting audio.
    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    sEnabled = 1;
    sMode = 0;
    sOvercast = 1;
    PlayState overcastPlay = {};
    overcastPlay.skyboxId = SKYBOX_NORMAL_SKY;
    GlobalOutdoorRain_Update(&overcastPlay);
    REQUIRE(overcastPlay.envCtx.gloomySkyMode == 1);
    GlobalOutdoorRain_OnPlayDestroy();
    PlayState incomingOvercastPlay = {};
    incomingOvercastPlay.skyboxId = SKYBOX_NORMAL_SKY;
    GlobalOutdoorRain_Update(&incomingOvercastPlay);
    REQUIRE(incomingOvercastPlay.envCtx.gloomySkyMode == 1);
    sOvercast = 0;
    GlobalOutdoorRain_Update(&incomingOvercastPlay);
    REQUIRE(incomingOvercastPlay.envCtx.gloomySkyMode == 2);
    GlobalOutdoorRain_Reset();
    sOvercast = 1;

    // A proximity weather actor belongs to its outgoing PlayState. Its private
    // loop must not leak into a dry destination scene when that state is destroyed.
    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    WeatherSfxEngine_Reset();
    WeatherSfxEngine_Start();
    SetNatureRain(false);
    PlayState nativePlay = {};
    GlobalOutdoorRain_SetNativeRequest(&nativePlay, &nativePlay, 25, false);
    GlobalOutdoorRain_Resolve(&nativePlay);
    RequireMixFrame(101);
    GlobalOutdoorRain_OnPlayDestroy();
    RequireMixFrame(100);

    // Enabling intermittent weather should produce visible rain within five
    // seconds instead of spending the first visit in the normal long clear phase.
    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    sEnabled = 1;
    sMode = 0;
    PlayState promptIntermittent = {};
    GlobalOutdoorRain_Update(&promptIntermittent);
    sMode = 1;
    GlobalOutdoorRain_Update(&promptIntermittent);
    for (int update = 0; update < 100 && promptIntermittent.envCtx.unk_EE[0] == 0; ++update)
        GlobalOutdoorRain_Update(&promptIntermittent);
    REQUIRE(promptIntermittent.envCtx.unk_EE[0] > 0);
    GlobalOutdoorRain_Reset();
    sMode = 0;

    // The same opt-in overcast presentation applies to placed rain actors on
    // compatible outdoor skies; it releases when the actor relinquishes rain.
    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    sEnabled = 0;
    PlayState actorStormPlay = {};
    actorStormPlay.skyboxId = SKYBOX_NORMAL_SKY;
    GlobalOutdoorRain_SetNativeRequest(&actorStormPlay, &actorStormPlay, 25, true);
    GlobalOutdoorRain_Update(&actorStormPlay);
    REQUIRE(actorStormPlay.envCtx.gloomySkyMode == 1);
    GlobalOutdoorRain_SetNativeRequest(&actorStormPlay, &actorStormPlay, 0, false);
    GlobalOutdoorRain_Update(&actorStormPlay);
    REQUIRE(actorStormPlay.envCtx.gloomySkyMode == 2);
    GlobalOutdoorRain_Reset();

    // A normal scene handoff must not restart the authored rain loop. The next
    // outdoor PlayState adopts the existing global voice and cycle.
    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    sEnabled = 1;
    sMode = 0;
    sRainVolume = 100;
    SetNatureRain(false);
    PlayState outgoing = {};
    for (int frame = 0; frame < 62; ++frame)
        GlobalOutdoorRain_Update(&outgoing);
    int16_t beforeHandoff[6] = {};
    WeatherSamplePlayer_Mix(beforeHandoff, 3);
    REQUIRE(beforeHandoff[0] == 1 && beforeHandoff[2] == 2 && beforeHandoff[4] == 3);
    GlobalOutdoorRain_OnPlayDestroy();
    PlayState incoming = {};
    GlobalOutdoorRain_Update(&incoming);
    int16_t afterHandoff[2] = {};
    WeatherSamplePlayer_Mix(afterHandoff, 1);
    REQUIRE(afterHandoff[0] == 4);

    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    WeatherSfxEngine_Reset();
    WeatherSfxEngine_Start();
    PlayState play = {};
    for (int frame = 0; frame < 62; ++frame)
        GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 25);
    REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_ON);
    RequireMixFrame(101);

    // Persistent -> intermittent resets to dry immediately, including audio.
    // The old updater skipped zero intensity and left the persistent loop alive.
    sMode = 1;
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 0);
    REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_OFF);
    RequireMixFrame(100);

    // Exercise a full intermittent fade-out: dry must leave no residual loop.
    for (int frame = 0; frame < 1625; ++frame)
        GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 0);
    RequireMixFrame(100);

    sMode = 0;
    for (int frame = 0; frame < 62; ++frame)
        GlobalOutdoorRain_Update(&play);
    RequireMixFrame(101);
    const bool thunderStarted = WeatherSamplePlayer_Play("audio/samples/Low Thunder_META", 1.0f);
    const bool lightningStarted = WeatherSamplePlayer_Play("audio/samples/Lightning_META", 1.0f);
    REQUIRE(thunderStarted);
    REQUIRE(lightningStarted);
    SetNatureRain(true);
    GlobalOutdoorRain_Update(&play);
    RequireMixFrame(105); // native nature channel takes priority; shots survive
    SetNatureRain(false);
    GlobalOutdoorRain_Update(&play);
    RequireMixFrame(106);

    GlobalOutdoorRain_SetNativeRequest(&play, &play, 40, true);
    play.envCtx.lightningMode = LIGHTNING_MODE_ON;
    play.envCtx.unk_EE[0] = 40;
    play.envCtx.indoors = 1;
    sEnabled = 0;
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 40);
    REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_ON);
    RequireMixFrame(107);
    uint8_t red = 0, green = 0, blue = 0;
    const bool nativeHasEnhancedColor = GlobalOutdoorRain_GetRenderColor(&red, &green, &blue);
    REQUIRE(!nativeHasEnhancedColor);
    play.envCtx.indoors = 0;
    sEnabled = 1;
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 40);
    RequireMixFrame(108);
    GlobalOutdoorRain_SetNativeRequest(&play, &play, 0, 1);
    play.envCtx.unk_EE[0] = 0;
    GlobalOutdoorRain_Update(&play);
    const bool enhancedHasColor = GlobalOutdoorRain_GetRenderColor(&red, &green, &blue);
    REQUIRE(enhancedHasColor);
    RequireMixFrame(106);
    GlobalOutdoorRain_Reset();
    RequireMixFrame(105); // teardown stops only the loop, not either one-shot

    // A placed-weather notification adopts the shared rain loop without
    // restarting it, and reset releases that explicitly acquired ownership.
    WeatherSamplePlayer_Reset();
    WeatherSamplePlayer_SetLoop("audio/samples/Rainfall_META", 1.0f);
    sEnabled = 0;
    GlobalOutdoorRain_Update(&play);
    RequireMixFrame(101);
    GlobalOutdoorRain_SetNativeRequest(&play, &play, 25, 0);
    GlobalOutdoorRain_Update(&play);
    RequireMixFrame(102);
    GlobalOutdoorRain_Reset();
    GlobalOutdoorRain_Reset();
    RequireMixFrame(100);
    WeatherSamplePlayer_Reset();

    // Volume zero must release owned fallback without changing visual density.
    play = {};
    sEnabled = 1;
    for (int frame = 0; frame < 62; ++frame)
        GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_ON);

    // Rain-only placed weather suppresses lightning owned by enhanced rain.
    GlobalOutdoorRain_SetNativeRequest(&play, &play, 25, 0);
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_OFF);
    GlobalOutdoorRain_SetNativeRequest(&play, &play, 0, 0);
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_ON);

    // A placed thunderstorm takes ownership and preserves its native mode.
    GlobalOutdoorRain_SetNativeRequest(&play, &play, 25, 1);
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_ON);
    GlobalOutdoorRain_SetNativeRequest(&play, &play, 0, 1);

    sRainVolume = 0;
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 25);
    RequireMixFrame(100);
    sRainVolume = 100;
    GlobalOutdoorRain_Update(&play);
    RequireMixFrame(101);
    sEnabled = 0;
    for (int frame = 0; frame < 62; ++frame)
        GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 0);
    RequireMixFrame(100);
}
