#include "banjo_audio.h"

#include <math.h>
#include <string.h>

#include "banjo_config.h"
#include "banjo_rom.h"
#include "bk_sound_bank.h"
#include "libbk/src/libbk_port/libbk.h"

#define SOUND_SLOTS 512
#define SOUNDS_PER_FRAME 16
#define VOICE_MAX 16
#define COMMAND_RING 64
#define BK_SYNTH_RATE 22000.0f
#define HOST_OUTPUT_RATE 32000.0f
#define PAN_RIGHT 127.0f
#define HALF_PI 1.5707963f

_Static_assert((COMMAND_RING & (COMMAND_RING - 1)) == 0, "the ring is indexed with a mask");

typedef struct PlayCommand {
    const BkSound* sound;
    float step;
    float gainLeft;
    float gainRight;
} PlayCommand;

typedef struct Voice {
    const BkSound* sound;
    float position;
    float step;
    float gainLeft;
    float gainRight;
} Voice;

static BkSoundBank sBank;
static bool sIsBankOpen;
// Decoded once on the game thread and never freed, so the audio thread may read them at any time.
static BkSound sSounds[SOUND_SLOTS];
static bool sIsDecoded[SOUND_SLOTS];
static PlayCommand sCommands[COMMAND_RING];
// One writer per cursor: the game thread moves the head, the audio thread the tail.
static volatile uint32_t sCommandHead;
static volatile uint32_t sCommandTail;
static Voice sVoices[VOICE_MAX];

static void StartVoice(const PlayCommand* command) {
    Voice* chosen = &sVoices[0];

    for (int32_t index = 0; index < VOICE_MAX; index++) {
        if (sVoices[index].sound == NULL) {
            chosen = &sVoices[index];
            break;
        }
        if (sVoices[index].position > chosen->position) {
            chosen = &sVoices[index];
        }
    }
    chosen->sound = command->sound;
    chosen->position = 0.0f;
    chosen->step = command->step;
    chosen->gainLeft = command->gainLeft;
    chosen->gainRight = command->gainRight;
}

static int32_t ReadVoice(Voice* voice) {
    const uint32_t at = (uint32_t)voice->position;
    const float blend = voice->position - (float)at;
    const int16_t* samples = voice->sound->samples;
    const int32_t next = (at + 1 < voice->sound->count) ? samples[at + 1] : 0;

    return (int32_t)(samples[at] + (next - samples[at]) * blend);
}

static int16_t Saturate(float value) {
    return (int16_t)fmaxf(-32768.0f, fminf(32767.0f, value));
}

// Audio thread.
static void Mix(int16_t* samples, uint32_t frameCount) {
    uint32_t tail = sCommandTail;

    while (tail != sCommandHead) {
        StartVoice(&sCommands[tail & (COMMAND_RING - 1)]);
        tail++;
    }
    sCommandTail = tail;
    for (uint32_t frame = 0; frame < frameCount; frame++) {
        float left = 0.0f;
        float right = 0.0f;

        for (int32_t index = 0; index < VOICE_MAX; index++) {
            Voice* voice = &sVoices[index];

            if (voice->sound == NULL) {
                continue;
            }

            const float sample = (float)ReadVoice(voice);

            left += sample * voice->gainLeft;
            right += sample * voice->gainRight;
            voice->position += voice->step;
            if (voice->position >= voice->sound->count) {
                voice->sound = NULL;
            }
        }
        samples[frame * 2] = Saturate(left);
        samples[frame * 2 + 1] = Saturate(right);
    }
}

static bool OpenBankOnce(void) {
    const uint8_t* rom = BanjoRom_Get();

    if (!sIsBankOpen && rom != NULL) {
        sIsBankOpen = BkSoundBank_Open(&sBank, rom, BANJO_ROM_SIZE);
    }
    return sIsBankOpen;
}

static const BkSound* GetSound(int32_t sfxId) {
    if (sfxId < 0 || sfxId >= SOUND_SLOTS || sfxId >= sBank.soundCount) {
        return NULL;
    }
    if (!sIsDecoded[sfxId]) {
        sIsDecoded[sfxId] = true;
        if (!BkSoundBank_Decode(&sBank, (uint16_t)sfxId, &sSounds[sfxId])) {
            sSounds[sfxId].count = 0;
        }
    }
    return (sSounds[sfxId].count > 0) ? &sSounds[sfxId] : NULL;
}

static void QueuePlay(const BkSound* sound, const LibBkSound* request) {
    const uint32_t head = sCommandHead;
    const float angle = sound->pan / PAN_RIGHT * HALF_PI;
    PlayCommand* command = &sCommands[head & (COMMAND_RING - 1)];

    if (head - sCommandTail >= COMMAND_RING) {
        return;
    }
    command->sound = sound;
    command->step = BK_SYNTH_RATE * request->pitch * sound->pitchRatio / HOST_OUTPUT_RATE;
    command->gainLeft = cosf(angle) * sound->gain * request->volume;
    command->gainRight = sinf(angle) * sound->gain * request->volume;
    sCommandHead = head + 1;
}

void BanjoAudio_Register(const SOHModApi* api) {
    api->RegisterAudioMixInGroup(Mix, SOH_AUDIO_GROUP_SFX, BANJO_VOLUME_CVAR);
}

void BanjoAudio_PlayQueued(void) {
    LibBkSound requests[SOUNDS_PER_FRAME];
    const int32_t count = LibBk_TakeSounds(requests, SOUNDS_PER_FRAME);

    if (!OpenBankOnce()) {
        return;
    }
    for (int32_t index = 0; index < count; index++) {
        const BkSound* sound = GetSound(requests[index].sfxId);

        if (sound != NULL) {
            QueuePlay(sound, &requests[index]);
        }
    }
}
