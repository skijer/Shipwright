// Real actor lifecycle and extracted environment handlers, with only platform,
// hook-registration, and final audio-device boundaries replaced.
#include "test_require.h"
#include <math.h>
#include <string.h>
#include "overlays/actors/ovl_En_Weather_Tag/z_en_weather_tag.h"
#include "soh/Enhancements/audio/GlobalOutdoorRainBridge.h"
#include "soh/Enhancements/audio/WeatherSamplePlayer.h"
#include "code/concurrent_weather_audio.h"
void EnWeatherTag_Init(Actor*, PlayState*);
void EnWeatherTag_Destroy(Actor*, PlayState*);
void EnWeatherTag_Update(Actor*, PlayState*);
void EnWeatherTag_DisabledRainThunder(EnWeatherTag*, PlayState*);
void EnWeatherTag_EnabledRainThunder(EnWeatherTag*, PlayState*);
void EnWeatherTag_DisabledRainLakeHylia(EnWeatherTag*, PlayState*);
void EnWeatherTag_EnabledRainLakeHylia(EnWeatherTag*, PlayState*);
void EnWeatherTag_DisabledCloudyRainThunderKakariko(EnWeatherTag*, PlayState*);
void EnWeatherTag_EnabledCloudyRainThunderKakariko(EnWeatherTag*, PlayState*);
u8 gWeatherMode, D_8011FB34, gSkyboxBlendingEnabled, D_8011FB38;
u16 gTimeSpeed;
SaveContext gSaveContext;
PlayState* gPlayState;
static int sKilled;
static int sThunder = 1;
static float sLoopGain;
GameInfo* gGameInfo;
u32 gBitFlags[32];
// Non-rain/debug paths are link-only platform boundaries in this fixture.
s16 Math_SmoothStepToS(s16* value, s16 target, s16 scale, s16 step, s16 minStep) {
    return 0;
}
DebugDispObject* DebugDisplay_AddObject(f32 x, f32 y, f32 z, s16 rx, s16 ry, s16 rz, f32 sx, f32 sy, f32 sz, u8 red,
                                        u8 green, u8 blue, u8 alpha, s16 type, GraphicsContext* gfx) {
    return NULL;
}
void lusprintf(const char* file, int32_t line, int32_t level, const char* fmt, ...) {
}

f32 Actor_WorldDistXZToActor(Actor* a, Actor* b) {
    return hypotf(a->world.pos.x - b->world.pos.x, a->world.pos.z - b->world.pos.z);
}
s32 Flags_GetEventChkInf(s32 flag) {
    return 0;
}
void Actor_Kill(Actor* actor) {
    actor->update = NULL;
    ++sKilled;
}
int32_t CVarGetInteger(const char* key, int32_t fallback) {
    if (!strcmp(key, "gAudioEditor.GlobalOutdoorRainOvercast"))
        return 0;
    if (!strcmp(key, "gAudioEditor.ProximityWeatherThunder"))
        return sThunder;
    return fallback;
}
Color_RGB8 CVarGetColor24(const char* key, Color_RGB8 fallback) {
    return fallback;
}
s16 Rand_S16Offset(s16 base, s16 range) {
    return base;
}
u8 Audio_IsNatureRainEnabled(void) {
    return 0;
}
void Audio_SetNatureAmbienceChannelIO(u8 channel, u8 port, u8 value) {
}
void WeatherSamplePlayer_SetLoop(const char* path, float gain) {
    sLoopGain = path ? gain : 0;
}
void GlobalOutdoorRain_Log(const char* message) {
}
LightningStrike gLightningStrike;
static int sLightningBolts[3]; // only its count is consumed by the real handler
static s16 sLightningFlashAlpha;
f32 Rand_ZeroOne(void) {
    return 0.5f;
}
u8 Audio_IsNatureLightningEnabled(void) {
    return 0;
}
bool WeatherSamplePlayer_Play(const char* path, float gain) {
    return true;
}
void Environment_AddLightningBolts(PlayState* play, u8 count) {
}
void Environment_DrawLightningFlash(PlayState* play, u8 r, u8 g, u8 b, u8 alpha) {
}
#include "rain_environment_functions.inc"

static void Advance(PlayState* play, int frames) {
    for (int frame = 0; frame < frames; ++frame) {
        play->state.frames = frame;
        GlobalOutdoorRain_Resolve(play);
        func_800766C4(play);
    }
}
static void Overlap(int type, int reverse, EnWeatherTagActionFunc disabled, EnWeatherTagActionFunc enabled) {
    PlayState play = { 0 };
    Player player = { 0 };
    EnWeatherTag a = { 0 }, b = { 0 };
    GlobalOutdoorRain_BeginScene(&play, 0, 0, 0);
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    play.envCtx.indoors = 1;
    a.actor.params = b.actor.params = 0x0A00 | type;
    a.actor.room = 3;
    b.actor.room = 4;
    b.actor.world.pos.x = 900;
    player.actor.world.pos.x = 500;
    disabled(&a, &play);
    disabled(&b, &play);
    Advance(&play, 120);
    REQUIRE(play.envCtx.unk_EE[0] > 0 && play.envCtx.unk_EE[1] > 0);
    REQUIRE(a.actor.room == -1 && b.actor.room == -1);
    player.actor.world.pos.x = 1101;
    if (reverse) {
        b.actionFunc(&b, &play);
        a.actionFunc(&a, &play);
    } else {
        a.actionFunc(&a, &play);
        b.actionFunc(&b, &play);
    }
    REQUIRE(a.actionFunc == disabled && b.actionFunc == enabled);
    Advance(&play, 120);
    REQUIRE(play.envCtx.unk_EE[0] == (type == 5 ? 30 : 25));
    REQUIRE(play.envCtx.unk_EE[1] >= 24 && sLoopGain > 0);
    // The real sky-restoration writer must leave owned rain untouched.
    play.envCtx.indoors = 0;
    play.envCtx.gloomySkyMode = 2;
    play.envCtx.unk_DE = 1;
    gSkyboxBlendingEnabled = 0;
    func_8006FB94(&play.envCtx, 0);
    REQUIRE(play.envCtx.unk_EE[0] > 0);
    EnWeatherTag_Destroy(&b.actor, &play);
    Advance(&play, 240);
    REQUIRE(play.envCtx.unk_EE[0] == 0 && play.envCtx.unk_EE[1] == 0 && sLoopGain == 0);
}
static void Room10AndPool(void) {
    PlayState play = { 0 };
    Player player = { 0 };
    EnWeatherTag a = { 0 }, b = { 0 };
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    GlobalOutdoorRain_BeginScene(&play, 25, 1, 0);
    // Actual archive placements: room 8 and custom room 10.
    a.actor.params = 0x0807;
    a.actor.world.pos.x = 786;
    a.actor.world.pos.z = -2421;
    b.actor.params = 0x2207;
    b.actor.world.pos.x = -340;
    b.actor.world.pos.z = -2402;
    player.actor.world.pos.x = 20;
    player.actor.world.pos.z = -2402;
    EnWeatherTag_DisabledRainThunder(&a, &play);
    EnWeatherTag_DisabledRainThunder(&b, &play);
    player.actor.world.pos.x = -100;
    b.actionFunc(&b, &play);
    a.actionFunc(&a, &play);
    Advance(&play, 120);
    REQUIRE(play.envCtx.unk_EE[0] == 25 && play.envCtx.unk_EE[1] >= 24);
    // Direct 0 -> 9 entry, no rain actor at all in the pool room.
    GlobalOutdoorRain_BeginScene(&play, 25, 1, 0);
    play.roomCtx.curRoom.num = 9;
    Advance(&play, 120);
    REQUIRE(play.envCtx.unk_EE[0] == 25);
    play.envCtx.unk_F2[0] = 40;
    Advance(&play, 240);
    REQUIRE(play.envCtx.unk_EE[1] == 40); // Song of Storms retains independent priority.
    play.envCtx.unk_F2[0] = 0;
    play.csCtx.state = 1;
    GlobalOutdoorRain_SetScriptedRain(&play, 0);
    Advance(&play, 240);
    REQUIRE(play.envCtx.unk_EE[0] == 0 && play.envCtx.unk_EE[1] == 0);
    play.csCtx.state = CS_STATE_IDLE;
    Advance(&play, 120);
    REQUIRE(play.envCtx.unk_EE[0] == 25 && play.envCtx.unk_EE[1] >= 24);
}
static void RoomRevisit(void) {
    PlayState play = { 0 };
    Player player = { 0 };
    EnWeatherTag old = { 0 }, incoming = { 0 };
    GlobalOutdoorRain_BeginScene(&play, 0, 0, 0);
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    old.actor.id = ACTOR_EN_WEATHER_TAG;
    old.actor.params = 0x0807;
    old.actor.room = 8;
    old.actor.update = EnWeatherTag_Update;
    EnWeatherTag_Init(&old.actor, &play);
    old.actionFunc(&old, &play);
    REQUIRE(old.actor.room == -1 && old.sourceRoom == 8);
    play.actorCtx.actorLists[ACTORCAT_PROP].head = &old.actor;
    incoming.actor.id = old.actor.id;
    incoming.actor.params = old.actor.params;
    incoming.actor.room = 8;
    incoming.actor.update = EnWeatherTag_Update;
    sKilled = 0;
    EnWeatherTag_Init(&incoming.actor, &play);
    REQUIRE(sKilled == 1 && incoming.actor.update == NULL);
    EnWeatherTag_Destroy(&incoming.actor, &play);
    Advance(&play, 120);
    REQUIRE(play.envCtx.unk_EE[0] == 25); // destroying rejected duplicate cannot release original
    incoming.actor.room = 7;
    incoming.actor.update = EnWeatherTag_Update;
    sKilled = 0;
    EnWeatherTag_Init(&incoming.actor, &play);
    REQUIRE(sKilled == 0); // same coordinates in another authored room is distinct
}
static void ReleaseDuringFlash(void) {
    for (int disable = 0; disable < 2; ++disable) {
        PlayState play = { 0 };
        int owner;
        GlobalOutdoorRain_BeginScene(&play, 0, 0, 0);
        GlobalOutdoorRain_SetNativeRequest(&play, &owner, 25, 1);
        GlobalOutdoorRain_Resolve(&play);
        gLightningStrike.state = LIGHTNING_STRIKE_START;
        gLightningStrike.flashAlphaTarget = 200;
        sLightningFlashAlpha = 0;
        Environment_UpdateLightningStrike(&play);
        REQUIRE(play.envCtx.adjAmbientColor[0] > 0);
        if (disable)
            sThunder = 0;
        else
            GlobalOutdoorRain_SetNativeRequest(&play, &owner, 0, 0);
        GlobalOutdoorRain_Resolve(&play);
        for (int frame = 0; frame < 100; ++frame)
            Environment_UpdateLightningStrike(&play);
        REQUIRE(gLightningStrike.state == LIGHTNING_STRIKE_WAIT);
        REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_OFF);
        REQUIRE(play.envCtx.adjAmbientColor[0] == 0 && play.envCtx.adjAmbientColor[2] == 0);
        sThunder = 1;
    }
}
static void ReacquireThunderAfterExternalStop(void) {
    PlayState play = { 0 };
    GlobalOutdoorRain_BeginScene(&play, 25, 1, 0);
    GlobalOutdoorRain_Resolve(&play);
    gLightningStrike.state = LIGHTNING_STRIKE_END;
    gLightningStrike.flashAlphaTarget = 0;
    sLightningFlashAlpha = 10;
    // EnOkarinaEffect_Destroy can request LAST even when it never owned rain.
    play.envCtx.lightningMode = LIGHTNING_MODE_LAST;
    Environment_UpdateLightningStrike(&play);
    REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_OFF);
    GlobalOutdoorRain_Resolve(&play);
    REQUIRE(play.envCtx.lightningMode == LIGHTNING_MODE_ON);
}
int main(void) {
    ReacquireThunderAfterExternalStop();
    ReleaseDuringFlash();
    for (int reverse = 0; reverse < 2; ++reverse) {
        Overlap(3, reverse, EnWeatherTag_DisabledRainLakeHylia, EnWeatherTag_EnabledRainLakeHylia);
        Overlap(5, reverse, EnWeatherTag_DisabledCloudyRainThunderKakariko,
                EnWeatherTag_EnabledCloudyRainThunderKakariko);
        Overlap(7, reverse, EnWeatherTag_DisabledRainThunder, EnWeatherTag_EnabledRainThunder);
    }
    Room10AndPool();
    RoomRevisit();
    puts("PASS real weather actors: overlap, sky restoration, room10/pool, scripted weather, revisits, cleanup");
}
