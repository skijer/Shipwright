#ifndef MM_SFX_SERVICE_H
#define MM_SFX_SERVICE_H

#include "soh/ModApi/ModApi.h"

// Ids are MM's (2ship's NA_SE_*). MM keeps the position pointer while the sound lives, so it must outlast it,
// like &actor->projectedPos. Returns false without mm.o2r.
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"
#define MM_SFX_PLAY_SCALED_SERVICE "mm.sfx.play_scaled"
#define MM_SFX_STOP_SERVICE "mm.sfx.stop"

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);
// MM's Audio_PlaySfx_AtPosWithFreqAndVolume: frequency and volume are read while the sound lives, so they must
// outlast it the same way the position does.
typedef bool (*MmSfxPlayScaledFunc)(uint16_t sfxId, Vec3f* pos, f32* freqScale, f32* volume);
typedef void (*MmSfxStopFunc)(uint16_t sfxId);

// Instruments are 2ship's OCARINA_INSTRUMENT_*; pitch is OoT/MM's (C4 = 0) and 0xFF releases the note.
// bendFreq is read while the note plays, so it must outlast it.
#define MM_OCARINA_NOTE_SERVICE "mm.ocarina.note"
typedef bool (*MmOcarinaNoteFunc)(uint8_t instrumentId, uint8_t pitch, f32* bendFreq);

void MmSfxService_Init(const SOHModApi* api);

#endif
