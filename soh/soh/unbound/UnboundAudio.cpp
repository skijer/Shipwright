#include "UnboundAudio.h"

#include <z64.h>
#include "sequence.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/audio/AudioCollection.h"

#include <cstdlib>
#include <cstring>

#ifdef _MSC_VER
#define strdup _strdup
#endif

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
    // A custom sequence skipped over a missing soundfont keeps a stale seqNumber, so validate the id against the table.
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

    const size_t capacity = sequenceMapSize + 0xF;
    uint16_t id = 0;
    while (AudioCollection::Instance->HasSequenceNum(id) || (id < capacity && sequenceMap[id] != nullptr)) {
        id++;
    }

    const size_t needed = (size_t)id + 1;
    if (needed > sequenceMapSize) {
        const size_t grownCapacity = needed + 0xF;
        char** grown = (char**)realloc(sequenceMap, grownCapacity * sizeof(char*));
        if (grown == nullptr) {
            return 0;
        }
        sequenceMap = grown;
        u8* grownStatus = (u8*)realloc(gAudioContext.seqLoadStatus, grownCapacity);
        if (grownStatus == nullptr) {
            return 0;
        }
        gAudioContext.seqLoadStatus = grownStatus;
        memset(&grown[capacity], 0, (grownCapacity - capacity) * sizeof(char*));
        memset(&grownStatus[capacity], 5, grownCapacity - capacity);
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
    static uint16_t sPrevSongSeqId = 0;

    play->sequenceCtx.unboundSongSeqId = songSeqId;
    // Environment_PlaySceneSequence only queues when the vanilla u8 theme changes; when only the bound song changed,
    // drop the "already playing" marker so the new scene's audio is queued.
    if (songSeqId != sPrevSongSeqId && gSaveContext.seqId == play->sequenceCtx.seqId) {
        gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    }
    sPrevSongSeqId = songSeqId;
}
