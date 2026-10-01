// Los SFX de Majora's Mask para los demás mods. El motor aislado de la NEI ejecuta la secuencia de SFX de MM
// con sus propios fonts, fuera del pipeline de OoT, y se mezcla en el grupo SFX del juego para que el slider
// de efectos mande sobre él. Aquí se publica como servicio y se conecta al audio.
#include "mm_sfx_service.h"
#include "mm_audio_sfx.h"
#include "mm_sfx_synth.h"

#include <atomic>
#include <mutex>

extern "C" {
#include "variables.h"
}

namespace {

constexpr const char* SfxSequence = "audio/sequences/Sequence_0";
// El token con el que MM agrupa las peticiones de un mismo emisor.
constexpr u8 SfxToken = 4;

const SOHModApi* sApi;
// Guarda el planificador y el sintetizador, que el juego toca al pedir un sonido y el audio al mezclarlo.
std::mutex sEngineMutex;
std::atomic<bool> sGamePaused{ false };
bool sQueueReset = false;
// El planificador compara posiciones por puntero, así que nunca puede recibir NULL.
Vec3f sNoPosition = { 0.0f, 0.0f, 0.0f };

// Arranca la primera vez que alguien pide un sonido, no al cargar: lee la secuencia de SFX y dos fonts.
// Assumes sEngineMutex is held.
bool IsEngineReady() {
    if (MmSfxSynth_IsReady()) {
        return true;
    }
    return sApi->HasResource(SfxSequence) && MmSfxSynth_Init() != 0;
}

// Assumes sEngineMutex is held.
bool IsQueueReady() {
    if (!IsEngineReady()) {
        return false;
    }
    if (!sQueueReset) {
        AudioMmSfx_Reset();
        sQueueReset = true;
    }
    return true;
}

bool PlayMmSfxScaled(uint16_t sfxId, Vec3f* pos, f32* freqScale, f32* volume) {
    std::lock_guard<std::mutex> lock(sEngineMutex);
    if (!IsQueueReady()) {
        return false;
    }
    AudioMmSfx_PlaySfx(sfxId, pos != nullptr ? pos : &sNoPosition, SfxToken, freqScale, volume, nullptr);
    return true;
}

// MM's AudioOcarina_SetInstrument and AudioOcarina_PlayControllerInput: Sequence_0's ocarina channel reads the
// instrument from port 7 (an index into its own table) and the pitch from port 5 when NA_SE_OC_OCARINA starts.
constexpr u16 OcarinaSfxId = 0x5800;
constexpr int OcarinaChannel = 13;
constexpr int IoOcarinaInstrumentSet = 1;
constexpr int IoOcarinaPitch = 5;
constexpr int IoOcarinaInstrument = 7;
constexpr uint8_t OcarinaPitchNone = 0xFF;
f32 sOcarinaVolume = 87.0f / 127.0f;
uint8_t sOcarinaInstrument = 0;

bool PlayMmOcarinaNote(uint8_t instrumentId, uint8_t pitch, f32* bendFreq) {
    std::lock_guard<std::mutex> lock(sEngineMutex);
    if (!IsQueueReady()) {
        return false;
    }
    if (pitch == OcarinaPitchNone) {
        AudioMmSfx_StopById(OcarinaSfxId);
        return true;
    }
    if (instrumentId != sOcarinaInstrument) {
        MmSfxSynth_WriteChannelIO(OcarinaChannel, IoOcarinaInstrumentSet, static_cast<int8_t>(instrumentId));
        sOcarinaInstrument = instrumentId;
    }
    MmSfxSynth_WriteChannelIO(OcarinaChannel, IoOcarinaInstrument, static_cast<int8_t>(instrumentId - 1));
    MmSfxSynth_WriteChannelIO(OcarinaChannel, IoOcarinaPitch, static_cast<int8_t>(pitch));
    AudioMmSfx_PlaySfx(OcarinaSfxId, &sNoPosition, SfxToken, bendFreq, &sOcarinaVolume, nullptr);
    return true;
}

bool PlayMmSfx(uint16_t sfxId, Vec3f* pos) {
    return PlayMmSfxScaled(sfxId, pos, nullptr, nullptr);
}

void StopMmSfx(uint16_t sfxId) {
    std::lock_guard<std::mutex> lock(sEngineMutex);
    if (MmSfxSynth_IsReady()) {
        AudioMmSfx_StopById(sfxId);
    }
}

// Hilo de audio: nunca espera. Si el juego tiene el motor (encolando, o cargándolo la primera vez), ese
// buffer sale sin SFX de MM antes que atascar el audio del juego.
void MixMmSfx(int16_t* samples, uint32_t frameCount) {
    if (sGamePaused.load(std::memory_order_relaxed)) {
        return;
    }
    std::unique_lock<std::mutex> lock(sEngineMutex, std::try_to_lock);
    if (!lock.owns_lock() || !MmSfxSynth_IsReady()) {
        return;
    }
    MmSfxSynth_RenderInto(samples, frameCount);
}

// MM procesa la cola y los bancos de SFX una vez por frame de gameplay, no una
// vez por callback de audio. SoH suele producir tres buffers de audio por cada
// frame lógico; hacerlo en MixMmSfx consumía la renovación de los sonidos
// flagged (bubble breath/flight) tres veces más rápido de lo que Deku podía
// renovarla, reiniciando o cortando el sample entre frames.
//
// Las órdenes al sequence player cruzan al hilo de audio mediante su propia
// cola; el mutex sólo protege el estado compartido del scheduler aislado.
void UpdateMmSfx() {
    sGamePaused.store(gPlayState != nullptr && gPlayState->pauseCtx.state != 0, std::memory_order_relaxed);
    if (!sQueueReset) {
        return;
    }
    std::lock_guard<std::mutex> lock(sEngineMutex);
    if (!MmSfxSynth_IsReady()) {
        return;
    }
    AudioMmSfx_ProcessRequests();
    AudioMmSfx_ProcessActiveSfx();
}

} // namespace

void MmSfxService_Init(const SOHModApi* api) {
    sApi = api;
    api->PublishService(MM_SFX_PLAY_SERVICE, reinterpret_cast<void*>(&PlayMmSfx));
    api->PublishService(MM_SFX_PLAY_SCALED_SERVICE, reinterpret_cast<void*>(&PlayMmSfxScaled));
    api->PublishService(MM_SFX_STOP_SERVICE, reinterpret_cast<void*>(&StopMmSfx));
    api->PublishService(MM_OCARINA_NOTE_SERVICE, reinterpret_cast<void*>(&PlayMmOcarinaNote));
    api->RegisterAudioMixInGroup(MixMmSfx, SOH_AUDIO_GROUP_SFX, nullptr);
    SOH_REGISTER_HOOK(api, OnGameFrameUpdate, UpdateMmSfx);
}
