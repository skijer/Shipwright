// Puente entre el planificador de bancos de MM (mm_audio_sfx.cpp), que decide qué SFX se queda un canal y
// cuándo, y el sintetizador aislado, que ejecuta la secuencia de SFX de MM. Hace lo que MM hace con
// AUDIOCMD_CHANNEL_SET_IO sobre SEQ_PLAYER_SFX: escribe los puertos que Sequence_0 lee para lanzar la nota.
#include "mm_audio_sfx.h"
#include "mm_sfx_synth.h"

namespace {

// Puertos de E/S de un canal de la secuencia de SFX de MM (sfx.c:645-678).
constexpr int IoEnable = 0;
constexpr int IoDone = 1;
constexpr int IoVolume = 2;
constexpr int IoSfxLow = 4;
constexpr int IoSfxHigh = 5;
// Lo que la secuencia escribe en IoDone cuando el SFX ha terminado.
constexpr uint8_t IoValNone = 0xFF;
// MM's D_801D6608: the ocarina bank keeps port 5 for the note's pitch, so a trigger must not touch it.
constexpr bool BankWritesIoSfxHigh[7] = { true, true, true, true, true, false, true };

// MM: 2^(randFreq/96), aproximado linealmente porque randFreq nunca pasa de 63.
f32 ComputeEntryFreqScale(const MmSfxBankEntry* entry) {
    f32 freqScale = entry->freqScale != nullptr ? *entry->freqScale : 1.0f;
    if (entry->randFreq != 0) {
        freqScale *= 1.0f + (entry->randFreq * (1.0f / 96.0f));
    }
    return freqScale;
}

// La atenuación por distancia no está: el volumen es el que pida quien lanza el SFX, como en la NEI.
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
    // Los bancos grandes reparten el índice en dos bytes (sfx.c:655-670).
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

// Una nota que sigue sonando solo se actualiza: volver a lanzarla la apilaría en cada callback de audio.
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

// Apagar el canal solo impide nuevas notas: la que ya suena acaba con su envolvente, como en MM. Cortarla
// dejaba sin ataque a todos los SFX de un frame.
void MmSfxDispatch_StopEntry(u8 bankId, MmSfxBankEntry* entry, u8 channelIndex) {
    if (MmSfxSynth_IsReady()) {
        MmSfxSynth_WriteChannelIO(channelIndex, IoEnable, 0);
    }
}

} // extern "C"
