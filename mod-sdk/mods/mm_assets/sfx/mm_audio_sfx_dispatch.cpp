// Bridges MM's bank scheduler to the isolated synthesizer the way MM's AUDIOCMD_CHANNEL_SET_IO does: it writes
// the ports Sequence_0 reads to start a note.
#include "mm_audio_sfx.h"
#include "mm_sfx_synth.h"

namespace {

// I/O ports of an MM SFX sequence channel (sfx.c:645-678).
constexpr int IoEnable = 0;
constexpr int IoDone = 1;
constexpr int IoVolume = 2;
constexpr int IoSfxLow = 4;
constexpr int IoSfxHigh = 5;
// Written to IoDone by the sequence once the SFX ends.
constexpr uint8_t IoValNone = 0xFF;
// MM's D_801D6608: the ocarina bank keeps port 5 for the note's pitch, so a trigger must not touch it.
constexpr bool BankWritesIoSfxHigh[7] = { true, true, true, true, true, false, true };

// MM: 2^(randFreq/96), approximated linearly because randFreq never exceeds 63.
f32 ComputeEntryFreqScale(const MmSfxBankEntry* entry) {
    f32 freqScale = entry->freqScale != nullptr ? *entry->freqScale : 1.0f;
    if (entry->randFreq != 0) {
        freqScale *= 1.0f + (entry->randFreq * (1.0f / 96.0f));
    }
    return freqScale;
}

// No distance attenuation: the volume is whatever the caller asks for.
void DriveChannel(u8 bankId, const MmSfxBankEntry* entry, u8 channelIndex, bool firstTrigger) {
    const f32 volume = entry->volume != nullptr ? *entry->volume : 1.0f;
    MmSfxSynth_SetChannelState(channelIndex, volume, ComputeEntryFreqScale(entry), 0x40, 0);
    if (!firstTrigger) {
        return;
    }
    const u16 sfxId = entry->sfxId;
    MmSfxSynth_WriteChannelIO(channelIndex, IoEnable, 1);
    MmSfxSynth_WriteChannelIO(channelIndex, IoVolume, 0x7F);
    MmSfxSynth_WriteChannelIO(channelIndex, IoSfxLow, static_cast<int8_t>(sfxId & 0xFF));
    if (bankId >= 7 || !BankWritesIoSfxHigh[bankId]) {
        return;
    }
    // Large banks split the index across two bytes (sfx.c:655-670).
    const u8 high = gMmIsLargeSfxBank[bankId] ? static_cast<u8>(((sfxId & 0x300) >> 7) + ((sfxId & 0xFF) >> 7)) : 0;
    MmSfxSynth_WriteChannelIO(channelIndex, IoSfxHigh, static_cast<int8_t>(high));
}

} // namespace

extern "C" {

void MmSfxDispatch_PlayEntry(u8 bankId, MmSfxBankEntry* entry, u8 channelIndex) {
    if (entry == nullptr || entry->posX == nullptr || !MmSfxSynth_IsReady()) {
        return;
    }
    DriveChannel(bankId, entry, channelIndex, true);
}

// A note still playing is only updated; retriggering it would stack it on every audio callback.
void MmSfxDispatch_RefreshEntry(u8 bankId, MmSfxBankEntry* entry, u8 channelIndex) {
    if (entry == nullptr || entry->posX == nullptr || !MmSfxSynth_IsReady()) {
        return;
    }
    DriveChannel(bankId, entry, channelIndex, false);
}

s32 MmSfxDispatch_IsEntryActive(u8 bankId, MmSfxBankEntry* entry) {
    if (entry == nullptr || !MmSfxSynth_IsReady()) {
        return 0;
    }
    return static_cast<u8>(MmSfxSynth_ReadChannelIO(entry->channelIndex, IoDone)) == IoValNone ? 0 : 1;
}

// Disabling the channel only blocks new notes; the playing one ends on its envelope, as in MM.
void MmSfxDispatch_StopEntry(u8 bankId, MmSfxBankEntry* entry, u8 channelIndex) {
    if (MmSfxSynth_IsReady()) {
        MmSfxSynth_WriteChannelIO(channelIndex, IoEnable, 0);
    }
}

} // extern "C"
