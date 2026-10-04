#pragma once
#include <libultraship/libultra.h>
#include "../../include/sfx.h"
#include "../../include/vt.h"

/* C allocator boundary types from z64audio.h. SoundBankEntry, ActiveSound
 * and SoundParams retain every field; the synth context contains only the
 * channel IO and replacement flags consumed by code_800F7260.c. */
#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define SQ(x) ((x) * (x))
#define MAX_CHANNELS_PER_BANK 3
#define SFX_BANK_SHIFT(id) (((id) >> 12) & 0xFF)
#define SFX_BANK_MASK(id) ((id)&0xF000)
#define SFX_INDEX(id) ((id)&0x01FF)
#define SFX_BANK(id) SFX_BANK_SHIFT(SFX_BANK_MASK(id))
enum { SEQ_PLAYER_BGM_MAIN, SEQ_PLAYER_FANFARE, SEQ_PLAYER_SFX, SEQ_PLAYER_BGM_SUB };
enum {
    SFX_STATE_EMPTY,
    SFX_STATE_QUEUED,
    SFX_STATE_READY,
    SFX_STATE_PLAYING_REFRESH,
    SFX_STATE_PLAYING_1,
    SFX_STATE_PLAYING_2
};
typedef struct {
    f32 x, y, z;
} Vec3f;
typedef struct {
    f32 *posX, *posY, *posZ;
    u8 token;
    f32 *freqScale, *vol;
    s8* reverbAdd;
    f32 dist;
    u32 priority;
    u8 sfxImportance;
    u16 sfxParams, sfxId;
    u8 state, freshness, prev, next, channelIdx, unk_2F;
} SoundBankEntry;
typedef struct {
    u32 priority;
    u8 entryIndex;
} ActiveSound;
typedef struct {
    u8 importance;
    u16 params;
} SoundParams;
typedef struct {
    s8 soundScriptIO[8];
} SequenceChannel;
typedef struct {
    struct {
        SequenceChannel* channels[16];
    } seqPlayers[4];
    u8 seqReplaced[4];
} WeatherTestAudioContext;
#define IS_SEQUENCE_CHANNEL_VALID(channel) ((channel) != NULL)
extern WeatherTestAudioContext gAudioContext;
extern u8 gChannelsPerBank[4][7], gUsedChannelsPerBank[4][7], gIsLargeSoundBank[7];
extern SoundParams* gSoundParams[7];
extern SoundBankEntry* gSoundBanks[7];
extern ActiveSound gActiveSounds[7][3];
extern u8 gSoundBankMuted[7], gSfxChannelLayout;
extern Vec3f gSfxDefaultPos;
extern f32 gSfxDefaultFreqAndVolScale;
extern s8 gSfxDefaultReverb;
void Audio_SetVolScale(u8 player, u8 scale, u8 volume, u8 fade);
void Audio_QueueCmdS8(u32 command, s8 value);
void Audio_SetSfxProperties(u8 bank, u8 entry, u8 channel);
u32 Audio_NextRandom(void);
void AudioDebug_ScrPrt(const s8* text, u16 number);
void osSyncPrintf(const char* format, ...);
void Audio_ResetSounds(void);
void Audio_PlaySoundGeneral(u16 id, Vec3f* pos, u8 token, f32* frequency, f32* volume, s8* reverb);
void Audio_ProcessSoundRequests(void);
void func_800F8F88(void);
