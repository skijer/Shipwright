#include "audio_engine_types.h"
#include "test_require.h"
#include <stdlib.h>
#include <string.h>
#include "sequence.h"

#define ALIGN16(value) (((value) + 15) & ~(uintptr_t)15)
#define ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
AudioContext gAudioContext;
size_t fontMapSize = 1024, sequenceMapSize = 1024;
static char* fontNames[1024];
static char* sequenceNames[1039];
char** fontMap = fontNames;
char** sequenceMap = sequenceNames;
static u8 fontStatus[1024], sequenceStatus[1039];
static SequenceData sequences[1024];
static SoundFont fonts[1024];
static SoundFont* sSoundFontsStorage;
static size_t sSoundFontsCapacity;
static size_t sSequenceMapCapacity;
#define MM_SEQ_CAP_HEADROOM 256
static bool collectionEntries[65536];
bool AudioCollection_HasSequenceNum(u16 id) {
    return collectionEntries[id];
}
void AudioCollection_AddToCollection(char* path, u16 id) {
    collectionEntries[id] = true;
}
static Instrument instruments[1024];
static NoteSubEu gDefaultNoteSub;
static Instrument* instrumentPointers[1024];
static char script[] = { (char)0xFF };
static u8 gDefaultShortNoteVelocityTable[16], gDefaultShortNoteGateTimeTable[16];
static struct { int sceneNum; } * gPlayState;
#define SCENE_JABU_JABU_BOSS 19
void AudioSeq_SkipForwardSequence(SequencePlayer* player) {
}
void GameInteractor_ExecuteOnSeqPlayerInit(s32 player, s32 sequence) {
}
void* AudioHeap_SearchCaches(s32 table, s32 cache, s32 id);
void Audio_NoteInit(Note* note);
void Audio_AdsrInit(AdsrState* adsr, AdsrEnvelope* envelope, s16* volOut);
void Audio_BuildSyntheticWave(Note* note, SequenceLayer* layer, s32 instrument) {
    REQUIRE(false);
}
static u8 sFanfareStartTimer;
static u16 sFanfareSeqId, currentFanfare;
static int fanfareStops;
u16 func_800FA0B4(u8 player) {
    return currentFanfare;
}
s32* AudioLoad_GetFontsForSequence(s32 id, u32* count);
s32* func_800E5E84(s32 id, u32* count) {
    return AudioLoad_GetFontsForSequence(id, count);
}
#define Audio_SeqCmd1(player, fade) (++fanfareStops)
void* AudioHeap_SearchPermanentCache(s32 table, s32 id);
void* AudioHeap_AllocPermanent(s32 table, s32 id, size_t size);
void* AudioHeap_AllocCached(s32 table, ptrdiff_t size, s32 cache, s32 id);
void* AudioLoad_SearchCaches(s32 table, s32 id);
uintptr_t AudioLoad_SyncLoad(u32 table, u32 id, s32* allocated);
typedef void SoundFontData;
typedef struct {
    s32 sampleBankId1, sampleBankId2;
    intptr_t baseAddr1, baseAddr2;
    u32 medium1, medium2;
} RelocInfo;
void AudioLoad_RelocateFontAndPreloadSamples(s32 id, SoundFontData* data, RelocInfo* info, s32 temporary) {
}
void AudioLoad_SyncDma(uintptr_t source, u8* dest, size_t size, s32 medium) {
    memcpy(dest, (void*)source, size);
}
void AudioLoad_SyncDmaUnkMedium(uintptr_t source, u8* dest, size_t size, s32 medium) {
    REQUIRE(false);
}
void AudioLoad_SetSampleFontLoadStatusAndApplyCaches(s32 id, s32 status) {
    REQUIRE(false);
}
void AudioHeap_DiscardSampleBank(s32 id) {
    REQUIRE(false);
}
void AudioHeap_DiscardFont(s32 id) {
}
void AudioHeap_DiscardSequence(s32 id);

u32 AudioLoad_GetRealTableIndex(s32 table, u32 id) {
    return id;
}
SequenceData ResourceMgr_LoadSeqByName(const char* path) {
    return sequences[strtoul(path, NULL, 10)];
}
SequenceData* ResourceMgr_LoadSeqPtrByName(const char* path) {
    return &sequences[strtoul(path, NULL, 10)];
}
u16 AudioEditor_GetReplacementSeq(u16 id) {
    return id;
}
SoundFont* ResourceMgr_LoadAudioSoundFontByName(const char* path) {
    return &fonts[strtoul(path, NULL, 10)];
}
void AudioSeq_SequencePlayerDisable(SequencePlayer* player) {
    player->enabled = false;
}
void AudioSeq_ResetSequencePlayer(SequencePlayer* player) {
    player->enabled = false;
}
u8* AudioLoad_SyncLoadSeq(s32 id);
void AudioLoad_SetFontLoadStatus(s32 id, s32 status);
SoundFontData* AudioLoad_SyncLoadFont(u32 id);

#include "audio_runtime_functions.inc"

int main(void) {
    static char names[1024][12];
    for (int i = 0; i < 1024; ++i) {
        snprintf(names[i], sizeof(names[i]), "%d", i);
        fontNames[i] = sequenceNames[i] = names[i];
        instrumentPointers[i] = &instruments[i];
        fonts[i].instruments = &instrumentPointers[i];
        fonts[i].numInstruments = 1;
        sequences[i] = (SequenceData){
            .seqData = script, .seqDataSize = sizeof(script), .medium = 2, .numFonts = 1, .resolvedFont = i
        };
        sequences[i].fonts[0] = i;
    }
    gAudioContext.fontLoadStatus = fontStatus;
    gAudioContext.seqLoadStatus = sequenceStatus;
    REQUIRE(AudioLoad_InitFontMetadata(1280));
    REQUIRE(gAudioContext.soundFonts[512].instruments == &instrumentPointers[512]);
    REQUIRE(gAudioContext.soundFonts[512].fntIndex == 512);
    REQUIRE(!AudioLoad_InitFontMetadata(10));
    REQUIRE(gAudioContext.soundFonts[512].fntIndex == 512);
    static u8 pool[32768];
    gAudioContext.permanentPool = (AudioAllocPool){ .start = pool, .cur = pool, .size = sizeof(pool) };
    static u8 sequencePool[32768], sequenceTemporaryPool[4096];
    gAudioContext.seqCache.persistent.pool =
        (AudioAllocPool){ .start = sequencePool, .cur = sequencePool, .size = sizeof(sequencePool) };
    gAudioContext.seqCache.temporary.pool = (AudioAllocPool){ .start = sequenceTemporaryPool,
                                                              .cur = sequenceTemporaryPool,
                                                              .size = sizeof(sequenceTemporaryPool) };
    AudioHeap_TemporaryCacheClear(&gAudioContext.seqCache.temporary);
    gAudioContext.audioBufferParameters.numSequencePlayers = 4;
    u32 fontCount = 0;
    const void* fontList = AudioLoad_GetFontsForSequence(512, &fontCount);
    REQUIRE(fontList != NULL);
    REQUIRE(fontCount == 1);
    // Multi-font sequences (including MM remaps) need the same host-width path.
    sequences[512].resolvedFont = -1;
    SequenceChannel channel = { 0 };
    SequencePlayer* player = &gAudioContext.seqPlayers[0];
    player->channels[0] = &channel;

    // A real bank 255 must load normally, not masquerade as the no-font sentinel.
    REQUIRE(!AudioLoad_IsFontLoadComplete(255));
    const int ids[] = { 1, 254, 255, 256, 257, 349, 511, 512, 1023 };
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        int id = ids[i];
        memset(fontStatus, 0, sizeof(fontStatus));
        REQUIRE(AudioLoad_SyncInitSeqPlayerInternal(0, id, 0));
        REQUIRE(player->enabled);
        REQUIRE(player->seqId == id);
        REQUIRE(AudioLoad_IsFontLoadComplete(player->defaultFont));
        REQUIRE(player->defaultFont == id);
        AudioSeq_SequencePlayerSetupChannels(player, 1);
        REQUIRE(Audio_GetInstrumentInner(channel.fontId, 0) == &instruments[id]);
        SequenceLayer layer = { .channel = &channel };
        Note note = { 0 };
        Audio_NoteInitForLayer(&note, &layer);
        REQUIRE(note.playbackState.fontId == id);
        AudioLoad_SetSeqLoadStatus(id, 2);
        REQUIRE(sequenceStatus[id] == 5); // Permanent resources stay permanent.
        REQUIRE(AudioLoad_IsSeqLoadComplete(id));
    }
    REQUIRE(Audio_GetInstrumentInner(-1, 0) == NULL);
    REQUIRE(!AudioLoad_IsFontLoadComplete(1024));
    REQUIRE(!AudioLoad_IsSeqLoadComplete(1039));

    // A high bank must release its notes without releasing a low-byte alias.
    Note notes[2] = { 0 };
    gAudioContext.notes = notes;
    gAudioContext.numNotes = 2;
    notes[0].playbackState.fontId = 512;
    notes[1].playbackState.fontId = 0;
    for (int i = 0; i < 2; ++i) {
        notes[i].playbackState.priority = 2;
        notes[i].playbackState.adsr.action.s.state = ADSR_STATE_DECAY;
    }
    AudioHeap_ReleaseNotesForFont(512);
    REQUIRE(notes[0].playbackState.adsr.action.s.release);
    REQUIRE(!notes[1].playbackState.adsr.action.s.release);

    // Cache lookup must retain the entire host ID, including above signed 16-bit.
    // Start a fresh cache for the isolated cache/selector tests below.
    gAudioContext.permanentPool = (AudioAllocPool){ .start = pool, .cur = pool, .size = sizeof(pool) };
    void* cached = AudioHeap_AllocPermanent(FONT_TABLE, 32768, 16);
    REQUIRE(cached != NULL);
    REQUIRE(AudioHeap_SearchPermanentCache(FONT_TABLE, 32768) == cached);
    REQUIRE(AudioHeap_SearchPermanentCache(FONT_TABLE, 0) == NULL);
    AudioHeap_AllocPermanent(FONT_TABLE, 512, 16);
    AudioHeap_AllocPermanent(FONT_TABLE, 257, 16);
    player->seqId = 512;
    player->defaultFont = 512;
    channel.seqPlayer = player;
    sequences[512].numFonts = 2;
    sequences[512].fonts[0] = 257;
    sequences[512].fonts[1] = 512;
    channel.fontId = 1;
    AudioSeq_SelectChannelFont(&channel, 0);
    REQUIRE(channel.fontId == 512);
    AudioSeq_SelectChannelFont(&channel, 1);
    REQUIRE(channel.fontId == 257);
    AudioSeq_SelectChannelFont(&channel, 2); // Exactly count is out of bounds.
    REQUIRE(channel.fontId == 257);
    AudioSeq_SelectChannelFont(&channel, 255);
    REQUIRE(channel.fontId == 257);
    // Packed stream CRC bytes have the same meaning after widening host IDs.
    SequenceData packed = { .fonts = { 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01 } };
    REQUIRE(AudioSequence_GetFontHash(&packed) == UINT64_C(0x0123456789ABCDEF));
    currentFanfare = 0;
    gAudioContext.seqPlayers[SEQ_PLAYER_FANFARE].defaultFont = 0;
    gAudioContext.seqReplaced[SEQ_PLAYER_FANFARE] = 1;
    gAudioContext.seqToPlay[SEQ_PLAYER_FANFARE] = 512;
    sequences[512].numFonts = 1;
    sequences[512].fonts[0] = 512;
    Audio_PlayFanfare(512);
    REQUIRE(sFanfareStartTimer == 5);
    REQUIRE(fanfareStops == 1);
    REQUIRE(gAudioContext.seqReplaced[SEQ_PLAYER_FANFARE] == 1);
    // Native flagged requests still resolve the base ID exactly once.
    gAudioContext.seqReplaced[SEQ_PLAYER_FANFARE] = 0;
    gAudioContext.seqPlayers[SEQ_PLAYER_FANFARE].defaultFont = 0x22;
    Audio_PlayFanfare(0x922);
    REQUIRE(sFanfareStartTimer == 1);
    REQUIRE(fanfareStops == 1);
    currentFanfare = NA_BGM_DISABLED;
    Audio_PlayFanfare(0x922);
    REQUIRE(sFanfareStartTimer == 1);
    // A large catalog must not write beyond the fixed native cache metadata.
    while (gAudioContext.permanentPool.count < 32) {
        REQUIRE(AudioHeap_AllocPermanent(FONT_TABLE, gAudioContext.permanentPool.count + 600, 16) != NULL);
    }
    REQUIRE(AudioHeap_AllocPermanent(FONT_TABLE, 999, 16) == NULL);
    REQUIRE(gAudioContext.permanentPool.count == 32);
    static u8 fontPool[32768], temporaryPool[4096];
    gAudioContext.fontCache.persistent.pool =
        (AudioAllocPool){ .start = fontPool, .cur = fontPool, .size = sizeof(fontPool) };
    gAudioContext.fontCache.temporary.pool =
        (AudioAllocPool){ .start = temporaryPool, .cur = temporaryPool, .size = sizeof(temporaryPool) };
    AudioHeap_TemporaryCacheClear(&gAudioContext.fontCache.temporary);
    // Sequentially visit >512 unique banks with real bounded cache allocation.
    // No voices are active in this test; voice eviction itself is a separate gate.
    gAudioContext.numNotes = 0;
    for (int id = 0; id < 600; ++id) {
        REQUIRE(AudioLoad_SyncLoadFont(id) != NULL);
        REQUIRE(AudioLoad_IsFontLoadComplete(id));
        REQUIRE(AudioHeap_SearchCaches(FONT_TABLE, CACHE_EITHER, id) != NULL);
        REQUIRE(gAudioContext.fontCache.persistent.numEntries <= 16);
    }
    REQUIRE(AudioLoad_SyncInitSeqPlayerInternal(0, 700, 0));
    // MAIN is enabled but resting: no sounding note may pin its bank. Starting
    // fanfare/SUB banks must not evict MAIN's resource or stop its next update.
    REQUIRE(AudioLoad_SyncLoadFont(701) != NULL);
    REQUIRE(AudioLoad_SyncLoadFont(702) != NULL);
    REQUIRE(AudioLoad_IsFontLoadComplete(player->defaultFont));
    for (int id = 0; id < 600; ++id) {
        REQUIRE(AudioLoad_SyncInitSeqPlayerInternal(0, id, 0));
        REQUIRE(player->seqId == id);
        REQUIRE(player->enabled);
        REQUIRE(player->seqData[0] == 0xFF);
        REQUIRE(AudioLoad_IsSeqLoadComplete(id));
        REQUIRE(gAudioContext.seqCache.persistent.numEntries <= 16);
    }
    sequences[999].numFonts = 0;
    REQUIRE(AudioLoad_SyncInitSeqPlayerInternal(0, 999, 0));
    REQUIRE(player->defaultFont == AUDIO_FONT_NONE);
    REQUIRE(AudioLoad_IsFontLoadComplete(player->defaultFont));
    REQUIRE(Audio_GetInstrumentInner(player->defaultFont, 0) == NULL);
    sequences[998].resolvedFont = 1024;
    REQUIRE(!AudioLoad_SyncInitSeqPlayerInternal(0, 998, 0));
    REQUIRE(!player->enabled);
    // Fanfare cleanup must not pop active MAIN bytecode from a spill cache.
    gAudioContext.seqCache.persistent.numEntries = 0;
    gAudioContext.seqCache.persistent.pool.cur = sequencePool;
    gAudioContext.seqCache.persistent.pool.count = 0;
    REQUIRE(AudioLoad_SyncInitSeqPlayerInternal(0, 900, 0));
    AudioHeap_PopCache(SEQUENCE_TABLE);
    REQUIRE(AudioLoad_IsSeqLoadComplete(900));
    REQUIRE(gAudioContext.seqCache.persistent.numEntries == 1);
    player->enabled = false;
    AudioHeap_PopCache(SEQUENCE_TABLE);
    REQUIRE(gAudioContext.seqCache.persistent.numEntries == 0);
    AudioHeap_TemporaryCacheClear(&gAudioContext.seqCache.temporary);
    REQUIRE(AudioHeap_AllocCached(SEQUENCE_TABLE, 16, CACHE_TEMPORARY, 901) != NULL);
    AudioLoad_SetSeqLoadStatus(901, 2);
    player->enabled = true;
    player->seqId = 901;
    REQUIRE(AudioHeap_AllocCached(SEQUENCE_TABLE, 16, CACHE_TEMPORARY, 902) != NULL);
    AudioLoad_SetSeqLoadStatus(902, 2);
    gAudioContext.seqPlayers[1].enabled = true;
    gAudioContext.seqPlayers[1].seqId = 902;
    REQUIRE(AudioHeap_AllocCached(SEQUENCE_TABLE, 16, CACHE_TEMPORARY, 903) == NULL);
    REQUIRE(AudioLoad_IsSeqLoadComplete(901));
    // MM's 128 registered sequences must fit the boot headroom, not consume
    // sixteen slots each and force a live-reader realloc.
    sSequenceMapCapacity = 1024 + 256 + 15;
    char** registered = calloc(sSequenceMapCapacity, sizeof(char*));
    u8* registeredStatus = calloc(sSequenceMapCapacity, 1);
    sequenceMap = registered;
    gAudioContext.seqLoadStatus = registeredStatus;
    sequenceMapSize = 1024;
    for (int i = 0; i < 128; ++i) {
        s32 id = AudioLoad_FindNextFreeSeqId();
        REQUIRE(id == 1024 + i);
        REQUIRE(AudioLoad_RegisterMmSequence("fixture", id));
        REQUIRE(sequenceMap == registered);
        REQUIRE(gAudioContext.seqLoadStatus == registeredStatus);
    }
    for (int i = 1024; i < 1152; ++i)
        free(sequenceMap[i]);
    free(sequenceMap);
    free(registeredStatus);
    sequenceMap = sequenceNames;
    gAudioContext.seqLoadStatus = sequenceStatus;
    sequenceMapSize = 1024;
    REQUIRE(AudioLoad_IsSeqLoadComplete(902));
    gAudioContext.seqPlayers[1].enabled = false;
    REQUIRE(AudioHeap_AllocCached(SEQUENCE_TABLE, 16, CACHE_TEMPORARY, 903) != NULL);
    REQUIRE(AudioLoad_IsSeqLoadComplete(901));
    REQUIRE(AudioHeap_AllocCached(SEQUENCE_TABLE, sizeof(sequenceTemporaryPool), CACHE_TEMPORARY, 904) == NULL);
    REQUIRE(AudioLoad_IsSeqLoadComplete(901));
    puts("PASS streamed audio runtime: full-width loading, bank lookup, readiness, release and cache identity");
    free(sSoundFontsStorage);
}
