// Audio commands and the updater remain production code. Only unrelated
// audio-core state, flag lookup and channel splitting are fixture boundaries.
#include "night_bgm_bridge.h"
static u8 sSeqModeInput, sAudioCutsceneFlag, sPrevSeqMode;
static u16 sPrevMainBgmSeqId = NA_BGM_DISABLED;
static int sNumFramesMoving, sNumFramesStill;
static u8 sAudioEnemyVol = 100;
static int splitVolume;
static u8 Audio_GetSeqFlags(u8 id) {
    return id == 0x18 ? 1 : 0;
}
static void Audio_SplitBgmChannels(int volume) {
    splitVolume = volume;
}
extern "C" void Audio_SetVolScale(u8, u8, u8, u8);
#define Audio_StartSeq(player, fade, id) Audio_QueueSeqCmd(((player) << 24) | ((fade) << 16) | (id))
#define Audio_SeqCmd1(player, fade) Audio_QueueSeqCmd(0x100000FF | ((player) << 24) | ((fade) << 16))
#define Audio_SeqCmd7(player, port, value) Audio_QueueSeqCmd(0x70000000 | ((player) << 24) | ((port) << 16) | (value))
#include "night_sequence_mode.inc"

static void TestCombat(PlayState& play) {
    // Dusk can replace FIELD_LOGIC while its internal enemy mode is already
    // set. That untagged mode is not an active SUB battle overlay.
    StartNight(play);
    sPrevSeqMode = SEQ_MODE_ENEMY;
    Audio_SetSequenceMode(SEQ_MODE_ENEMY);
    Audio_ProcessSeqCmds();
    REQUIRE(gActiveSeqs[SEQ_PLAYER_BGM_SUB].seqId == 0x081A);
    StartNight(play);
    sPrevSeqMode = SEQ_MODE_DEFAULT;
    Audio_SetSequenceMode(SEQ_MODE_ENEMY);
    Audio_ProcessSeqCmds();
    REQUIRE(gActiveSeqs[SEQ_PLAYER_BGM_SUB].seqId == 0x081A);
    REQUIRE(gActiveSeqs[0].volScales[3] == 27);
    REQUIRE(splitVolume == 100);
    commands.clear();
    Audio_SetSequenceMode(SEQ_MODE_ENEMY);
    Audio_ProcessSeqCmds();
    REQUIRE(commands.empty()); // no repeated battle starts
    Audio_SetSequenceMode(SEQ_MODE_DEFAULT);
    Audio_ProcessSeqCmds();
    REQUIRE(gActiveSeqs[SEQ_PLAYER_BGM_SUB].seqId == 0xFFFF);
    REQUIRE(gActiveSeqs[0].volScales[3] == 127);
    REQUIRE(splitVolume == 0);
    REQUIRE(gActiveSeqs[0].seqId == 0x5D);

    // Full-width identity: neither vanilla 0x5D nor custom 0x25D may
    // inherit 0x15D's registration, despite identical player-facing IDs.
    for (u16 other : { 0x005D, 0x025D }) {
        StartNight(play);
        Audio_QueueResolvedSeqCmd(0, other, 0);
        Audio_ProcessSeqCmds();
        sPrevSeqMode = SEQ_MODE_DEFAULT;
        Audio_SetSequenceMode(SEQ_MODE_ENEMY);
        Audio_ProcessSeqCmds();
        REQUIRE(gActiveSeqs[SEQ_PLAYER_BGM_SUB].seqId == 0xFFFF);
    }

    // Every ownership release must prevent a later replay of the same
    // resource from silently regaining combat eligibility.
    for (int release = 0; release < 5; ++release) {
        StartNight(play);
        if (release == 0)
            gSaveContext.nightFlag = 0;
        if (release == 1)
            enabled = false;
        if (release == 2)
            play.sceneNum = 0x20;
        if (release < 3)
            Frame();
        if (release == 3)
            HyruleFieldNightMusic_Reset();
        if (release == 4)
            Audio_ResetActiveSequences();
        Audio_QueueResolvedSeqCmd(0, 0x15D, 0);
        Audio_ProcessSeqCmds();
        sPrevSeqMode = SEQ_MODE_DEFAULT;
        Audio_SetSequenceMode(SEQ_MODE_ENEMY);
        Audio_ProcessSeqCmds();
        REQUIRE(gActiveSeqs[SEQ_PLAYER_BGM_SUB].seqId == 0xFFFF);
    }

    StartNight(play);
    sPrevSeqMode = SEQ_MODE_DEFAULT;
    sAudioCutsceneFlag = 1;
    Audio_SetSequenceMode(SEQ_MODE_ENEMY);
    Audio_ProcessSeqCmds();
    REQUIRE(gActiveSeqs[SEQ_PLAYER_BGM_SUB].seqId == 0xFFFF);
    sAudioCutsceneFlag = 0;

    // Vanilla eligibility still works without night ownership.
    HyruleFieldNightMusic_Reset();
    Audio_QueueResolvedSeqCmd(0, 0x18, 0);
    Audio_ProcessSeqCmds();
    sPrevSeqMode = SEQ_MODE_DEFAULT;
    Audio_SetSequenceMode(SEQ_MODE_ENEMY);
    Audio_ProcessSeqCmds();
    REQUIRE(gActiveSeqs[SEQ_PLAYER_BGM_SUB].seqId == 0x081A);
}
