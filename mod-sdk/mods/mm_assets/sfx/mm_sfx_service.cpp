// Majora's Mask SFX for other mods. An isolated engine runs MM's SFX sequence with its own fonts, outside OoT's
// pipeline, and mixes into the game's SFX group so the effects slider governs it.
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
constexpr u8 SfxToken = 4;

const SOHModApi* sApi;
// Guards the scheduler and synthesizer: the game touches them to request a sound, the audio thread to mix it.
std::mutex sEngineMutex;
std::atomic<bool> sGamePaused{ false };
bool sQueueReset = false;
// The scheduler compares positions by pointer, so it never gets NULL.
Vec3f sNoPosition = { 0.0f, 0.0f, 0.0f };

// Starts on the first request rather than at load, reading the SFX sequence and two fonts.
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

// Audio thread: never waits. While the game holds the engine, this buffer goes out without MM SFX rather than
// stalling the game's audio.
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

// MM runs the SFX queue once per gameplay frame, not per audio callback; at three buffers a frame, looping
// sounds such as Deku bubbles expired before Deku could renew them.
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

// Mods play at &actor->projectedPos, and OoT only stops its own sounds when an actor goes. Runs before
// Actor_Delete frees the actor, so its position can still be copied.
void DetachActorSfx(void* actor) {
    if (!sQueueReset) {
        return;
    }
    std::lock_guard<std::mutex> lock(sEngineMutex);
    AudioMmSfx_DetachPos(&static_cast<Actor*>(actor)->projectedPos);
}

// MM's GameState_Destroy (AudioMgr_StopAllSfxExceptSystem): a detached loop has no owner left to stop it.
void StopSceneSfx() {
    if (!sQueueReset) {
        return;
    }
    std::lock_guard<std::mutex> lock(sEngineMutex);
    for (u8 bankId = MM_BANK_PLAYER; bankId <= MM_BANK_VOICE; bankId++) {
        if (bankId != MM_BANK_SYSTEM) {
            AudioMmSfx_StopByBank(bankId);
        }
    }
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
    SOH_REGISTER_HOOK(api, OnActorDestroy, DetachActorSfx);
    SOH_REGISTER_HOOK(api, OnPlayDestroy, StopSceneSfx);
}
