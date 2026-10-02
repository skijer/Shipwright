#include "UnboundAudio.h"

#include <z64.h>
#include "sequence.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/audio/AudioCollection.h"

#include <cstdlib>
#include <cstring>

// audio_load.c's table: sequence id -> the archive path the id was built from (vanilla and custom alike).
// The allocation over-reserves 0xF slots because custom-id assignment can push past sequenceMapSize.
extern "C" char** sequenceMap;
extern "C" size_t sequenceMapSize;
extern "C" char** fontMap;
extern "C" size_t fontMapSize;
extern "C" SaveContext gSaveContext;
extern "C" AudioContext gAudioContext;

namespace SOH::Unbound {
uint16_t SequenceIdForPath(const std::string& path) {
    if (path.empty() || sequenceMap == nullptr) {
        return 0;
    }
    // The engine stamps the assigned id on the cached sequence resource (AudioLoad_Init). A custom sequence
    // skipped over a missing soundfont keeps a stale seqNumber, so validate the id against the table it
    // indexes before trusting it (the table is zeroed at allocation, so unassigned slots read as NULL).
    SequenceData* seq = ResourceMgr_LoadSeqPtrByName(path.c_str());
    if (seq == nullptr) {
        return 0;
    }
    size_t id = seq->seqNumber;
    if (id == 0 || id >= sequenceMapSize + 0xF || sequenceMap[id] == nullptr || path != sequenceMap[id]) {
        return 0;
    }
    return static_cast<uint16_t>(id);
}
} // namespace SOH::Unbound

extern "C" uint16_t Unbound_SequenceIdForPath(const char* path) {
    return path == nullptr ? 0 : SOH::Unbound::SequenceIdForPath(path);
}

extern "C" uint16_t Unbound_RegisterSequence(const char* path) {
    if (path == nullptr || sequenceMap == nullptr) {
        return 0;
    }
    SequenceData* sequence = ResourceMgr_LoadSeqPtrByName(path);
    if (sequence == nullptr) {
        return 0;
    }

    uint16_t id = 0;
    while (AudioCollection::Instance->HasSequenceNum(id) || (id < sequenceMapSize && sequenceMap[id] != nullptr)) {
        id++;
    }

    size_t needed = (size_t)id + 1;
    if (needed > sequenceMapSize) {
        char** grown = (char**)realloc(sequenceMap, (needed + 0xF) * sizeof(char*));
        u8* grownStatus = (u8*)realloc(gAudioContext.seqLoadStatus, needed);
        if (grown == nullptr || grownStatus == nullptr) {
            return 0;
        }
        memset(&grown[sequenceMapSize], 0, (needed + 0xF - sequenceMapSize) * sizeof(char*));
        memset(&grownStatus[sequenceMapSize], 5, needed - sequenceMapSize);
        sequenceMap = grown;
        gAudioContext.seqLoadStatus = grownStatus;
        sequenceMapSize = needed;
    }

    AudioCollection::Instance->AddToCollection(const_cast<char*>(path), id);
    sequence->seqNumber = id;
    sequenceMap[id] = strdup(path);
    return id;
}

extern "C" int32_t Unbound_RegisterSoundFont(const char* path) {
    if (path == nullptr || fontMap == nullptr) {
        return -1;
    }
    SoundFont* font = ResourceMgr_LoadAudioSoundFontByName(path);
    if (font == nullptr) {
        return -1;
    }

    size_t index = fontMapSize;
    char** grown = (char**)realloc(fontMap, (index + 1) * sizeof(char*));
    u8* grownStatus = (u8*)realloc(gAudioContext.fontLoadStatus, index + 1);
    if (grown == nullptr || grownStatus == nullptr) {
        return -1;
    }
    grown[index] = strdup(path);
    grownStatus[index] = 0;
    fontMap = grown;
    gAudioContext.fontLoadStatus = grownStatus;
    fontMapSize = index + 1;
    font->fntIndex = (int32_t)index;
    return (int32_t)index;
}

extern "C" void Unbound_BindSceneSong(PlayState* play, uint16_t songSeqId) {
    // The song bound when a scene last ran its sound-settings command; a binary scene binds none.
    static uint16_t sPrevSongSeqId = 0;

    play->sequenceCtx.unboundSongSeqId = songSeqId;
    // Environment_PlaySceneSequence only queues when the vanilla u8 theme changes. Two scenes can share
    // that theme while binding different songs (or one binding none) — when only the effective song
    // changed, drop the "already playing" marker so the new scene's audio is queued (and resolved) fresh.
    if (songSeqId != sPrevSongSeqId && gSaveContext.seqId == play->sequenceCtx.seqId) {
        gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    }
    sPrevSongSeqId = songSeqId;
}
