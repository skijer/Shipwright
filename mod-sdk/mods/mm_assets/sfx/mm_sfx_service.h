#ifndef MM_SFX_SERVICE_H
#define MM_SFX_SERVICE_H

#include "soh/ModApi/ModApi.h"

// Lo que mm_assets ofrece a los demás mods con PublishService. El id es el de MM (NA_SE_* de 2ship), no el de
// OoT. El planificador de MM guarda el puntero de posición mientras el sonido vive: tiene que ser uno que dure,
// como &actor->projectedPos. Devuelve false si el mm.o2r no está.
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"
#define MM_SFX_PLAY_SCALED_SERVICE "mm.sfx.play_scaled"
#define MM_SFX_STOP_SERVICE "mm.sfx.stop"

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);
// MM's Audio_PlaySfx_AtPosWithFreqAndVolume: frequency and volume are read while the sound lives, so they must
// outlast it the same way the position does.
typedef bool (*MmSfxPlayScaledFunc)(uint16_t sfxId, Vec3f* pos, f32* freqScale, f32* volume);
typedef void (*MmSfxStopFunc)(uint16_t sfxId);

// Una nota de ocarina con la voz de un instrumento de MM (OCARINA_INSTRUMENT_* de 2ship: 5 Ikana, 7 tambores
// Goron, 8 guitarra Zora, 9 gaitas Deku). pitch es el de OoT/MM (C4 = 0); 0xFF la suelta. bendFreq se lee
// mientras la nota suena, así que tiene que durar.
#define MM_OCARINA_NOTE_SERVICE "mm.ocarina.note"
typedef bool (*MmOcarinaNoteFunc)(uint8_t instrumentId, uint8_t pitch, f32* bendFreq);

void MmSfxService_Init(const SOHModApi* api);

#endif
