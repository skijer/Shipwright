#pragma once

#include <stdbool.h>
#include <libultraship/libultra.h>
#include "sequence.h"

// Stand-ins for the engine state used by code_800F9280.c. Queueing, decoding,
// sequence resolution and active-sequence updates are compiled from production.
typedef struct {
    f32 volCur, volTarget, volStep;
    u16 volTimer;
    f32 freqScaleCur, freqScaleTarget, freqScaleStep;
    u16 freqScaleTimer;
} ActiveSequenceChannelData;

typedef struct {
    f32 volCur, volTarget, volStep;
    u16 volTimer;
    u8 volScales[4];
    u8 volFadeTimer, fadeVolUpdate;
    u32 tempoCmd;
    u16 tempoOriginal;
    f32 tempoCur, tempoTarget, tempoStep;
    u16 tempoTimer;
    u32 setupCmd[8];
    u8 setupCmdTimer, setupCmdNum, setupFadeTimer;
    ActiveSequenceChannelData channelData[16];
    u16 freqScaleChannelFlags, volChannelFlags;
    u16 seqId, prevSeqId, channelPortMask;
    u32 startSeqCmd;
    u8 isWaitingForFonts;
} ActiveSequence;

typedef struct {
    u8 seqReplaced[4];
    u16 seqToPlay[4];
    struct {
        u8 updatesPerFrame;
    } audioBufferParameters;
    struct {
        u8 enabled;
        u16 tempo;
    } seqPlayers[4];
} AudioContext;

enum { SEQUENCE_TABLE, FONT_TABLE, SAMPLE_TABLE };

#ifdef __cplusplus
extern "C" {
#endif
extern AudioContext gAudioContext;
extern ActiveSequence gActiveSeqs[4];
extern u8 D_80133408;
extern u8 gSfxChannelLayout;
extern s8 D_80133390[], D_80133398[];

void Audio_QueueCmdS32(u32 cmd, s32 value);
void Audio_QueueCmdF32(u32 cmd, f32 value);
void Audio_QueueCmdS8(u32 cmd, s8 value);
void Audio_QueueCmdU16(u32 cmd, u16 value);
void AudioDebug_ScrPrt(const s8* label, u16 value);
void func_800E5F88(u8 spec);
void func_800F71BC(u8 oldSpec);
s32 func_800E5E20(u32* value);
s32 func_800E5EDC(void);
void func_800F7170(void);
void Audio_QueueSeqCmd(u32 cmd);
void Audio_QueueResolvedSeqCmd(u8 playerIdx, u16 seqId, u8 fadeTimer);
void Audio_QueuePreviewSeqCmd(u16 seqId);
void Audio_ProcessSeqCmd(u32 cmd);
void Audio_ProcessSeqCmds(void);
void Audio_ResetActiveSequences(void);
void Audio_StartSequence(u8 playerIdx, u8 seqId, u8 args, u16 fadeTimer);
void Audio_PrimeMmSideChannel(u8 playerIdx, u16 fullSeqId);
#ifdef __cplusplus
}
#endif
