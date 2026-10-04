#include "weather_audio_stubs/global.h"
#include "weather_sfx_engine_fixture.h"
#include "test_require.h"
#include <string.h>

/* Default layout fixture from code_800EC960.c: only row 0 is supported by
 * sequence 0. Actual request filtering, bank allocation, priorities and
 * channel assignment run in code_800F7260.c with audio_sound_params.c. */
u8 gChannelsPerBank[4][7] = { { 3, 2, 3, 3, 2, 1, 2 } };
u8 gUsedChannelsPerBank[4][7] = { { 3, 2, 3, 2, 2, 1, 1 } };
u8 gIsLargeSoundBank[7] = { 0, 0, 0, 1, 0, 0, 0 };
WeatherTestAudioContext gAudioContext;
static SequenceChannel sChannels[16];
static u16 sStartedChannels;
static Vec3f sFlamePos = { 10.0f, 0.0f, 0.0f };
static Vec3f sDenseFlamePos[10];
static Vec3f sEnemyPos = { 20.0f, 0.0f, 0.0f };
static Vec3f sAmbiencePos[3] = {
    { 10.0f, 0.0f, 0.0f },
    { 20.0f, 0.0f, 0.0f },
    { 30.0f, 0.0f, 0.0f },
};
static const u16 sEffects[] = {
    // En_Elf's actual emergence/movement sound, not a Navi voice line.
    NA_SE_EV_FAIRY_DASH,
    NA_SE_EV_TORCH - SFX_FLAG,
    NA_SE_EN_STALKID_ATTACK,
    NA_SE_SY_GET_RUPY,
};

/* External randomizer is disabled. Synth command transport is captured;
 * actual synthesis/positional attenuation is outside this eligibility test. */
u16 AudioEditor_GetReplacementSeq(u16 id) {
    return id;
}
void Audio_QueueCmdS8(u32 command, s8 value) {
    if ((command >> 24) == 6 && ((command >> 16) & 0xFF) == SEQ_PLAYER_SFX && (command & 0xFF) == 0) {
        u16 bit = (u16)(1u << ((command >> 8) & 0xFF));
        if (value == 1)
            sStartedChannels |= bit;
        else
            sStartedChannels &= (u16)~bit;
    }
}
void Audio_SetVolScale(u8 player, u8 scale, u8 volume, u8 fade) {
    (void)player;
    (void)scale;
    (void)volume;
    (void)fade;
}
void Audio_SetSfxProperties(u8 bank, u8 entry, u8 channel) {
    (void)bank;
    (void)entry;
    (void)channel;
}
u32 Audio_NextRandom(void) {
    return 0;
}
void AudioDebug_ScrPrt(const s8* text, u16 number) {
    (void)text;
    (void)number;
}
void osSyncPrintf(const char* format, ...) {
    (void)format;
}

static void Request(u16 id, Vec3f* pos) {
    Audio_PlaySoundGeneral(id, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

void WeatherSfxEngine_Reset(void) {
    memset(&gAudioContext, 0, sizeof(gAudioContext));
    memset(sChannels, 0, sizeof(sChannels));
    sStartedChannels = 0;
    gSfxChannelLayout = 0;
    for (size_t i = 0; i < ARRAY_COUNT(sChannels); ++i) {
        gAudioContext.seqPlayers[SEQ_PLAYER_SFX].channels[i] = &sChannels[i];
    }
    Audio_ResetSounds();
}

void WeatherSfxEngine_Start(void) {
    Request(sEffects[0], &gSfxDefaultPos);
    Request(sEffects[1], &sFlamePos); // En_Light / Obj_Syokudai's continuous torch ID
    Request(sEffects[2], &sEnemyPos);
    Request(sEffects[3], &gSfxDefaultPos);
    Audio_ProcessSoundRequests();
    func_800F8F88();
    WeatherSfxEngine_RequirePlaying();
}

void WeatherSfxEngine_StartDenseFlameHub(void) {
    /* Model the reported central Hyrule Field load: ten independent En_Light
     * actors resubmit the same positional loop while Navi emerges. */
    for (size_t i = 0; i < ARRAY_COUNT(sDenseFlamePos); ++i) {
        sDenseFlamePos[i] = (Vec3f){ 10.0f + (float)i * 10.0f, 0.0f, 0.0f };
        Request(sEffects[1], &sDenseFlamePos[i]);
    }
    Request(sEffects[0], &gSfxDefaultPos);
    Request(sEffects[2], &sEnemyPos);
    Request(sEffects[3], &gSfxDefaultPos);
    Audio_ProcessSoundRequests();
    func_800F8F88();
    WeatherSfxEngine_RequirePlaying();
}

static int WeatherSfxEngine_IsPlaying(u16 sfxId) {
    const u8 bank = SFX_BANK(sfxId);
    for (u8 slot = 0; slot < gChannelsPerBank[0][bank]; ++slot) {
        u8 index = gActiveSounds[bank][slot].entryIndex;
        if (index == 0xFF)
            continue;
        SoundBankEntry* entry = &gSoundBanks[bank][index];
        if (entry->sfxId == sfxId && (entry->state == SFX_STATE_PLAYING_1 || entry->state == SFX_STATE_PLAYING_2)) {
            return 1;
        }
    }
    return 0;
}

int WeatherSfxEngine_IsNaviPlaying(void) {
    return WeatherSfxEngine_IsPlaying(NA_SE_EV_FAIRY_DASH);
}

void WeatherSfxEngine_StartSaturatedEnvironment(void) {
    /* Gerudo Valley and the drawbridge can already occupy all three
     * environment-bank voices before Navi's emergence one-shot arrives. */
    Request(NA_SE_EV_RIVER_STREAM, &sAmbiencePos[0]);
    Request(NA_SE_EV_WATER_WALL_BIG, &sAmbiencePos[1]);
    Request(NA_SE_EV_ROCK_SLIDE, &sAmbiencePos[2]);
    Request(NA_SE_EV_FAIRY_DASH, &gSfxDefaultPos);
    Audio_ProcessSoundRequests();
    func_800F8F88();
}

void WeatherSfxEngine_RequirePlaying(void) {
    u16 assigned = 0;
    for (size_t effect = 0; effect < ARRAY_COUNT(sEffects); ++effect) {
        const u8 bank = SFX_BANK(sEffects[effect]);
        int found = 0;
        REQUIRE(!gSoundBankMuted[bank]);
        for (u8 slot = 0; slot < gChannelsPerBank[0][bank]; ++slot) {
            u8 index = gActiveSounds[bank][slot].entryIndex;
            if (index == 0xFF)
                continue;
            SoundBankEntry* entry = &gSoundBanks[bank][index];
            if (entry->sfxId != sEffects[effect])
                continue;
            REQUIRE(entry->state == SFX_STATE_PLAYING_1 || entry->state == SFX_STATE_PLAYING_2);
            REQUIRE(entry->channelIdx < 16);
            u16 bit = (u16)(1u << entry->channelIdx);
            REQUIRE((sStartedChannels & bit) != 0);
            REQUIRE((assigned & bit) == 0);
            assigned |= bit;
            found = 1;
        }
        if (!found)
            fprintf(stderr, "Missing engine voice for SFX 0x%04x\n", sEffects[effect]);
        REQUIRE(found);
    }
}

void WeatherSfxEngine_Refresh(void) {
    // Actor loops resubmit each frame; one-shots must retain their voices.
    Request(sEffects[1], &sFlamePos);
    Audio_ProcessSoundRequests();
    func_800F8F88();
    WeatherSfxEngine_RequirePlaying();
}
