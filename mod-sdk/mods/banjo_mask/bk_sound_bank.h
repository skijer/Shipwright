#ifndef BK_SOUND_BANK_H
#define BK_SOUND_BANK_H

#include <stdbool.h>
#include <stdint.h>

// Banjo-Kazooie's sound effect bank: an N64 ALBankFile with one instrument whose sounds are the sfx ids.
typedef struct BkSoundBank {
    const uint8_t* ctl;
    uint32_t ctlSize;
    const uint8_t* tbl;
    uint32_t tblSize;
    uint32_t sampleRate;
    uint32_t instrumentOffset;
    uint16_t soundCount;
} BkSoundBank;

typedef struct BkSound {
    // Mono, at the bank's sample rate; the caller frees it.
    int16_t* samples;
    uint32_t count;
    // From the key map: BK's sound player multiplies the requested pitch by it.
    float pitchRatio;
    // Sample volume and envelope attack level, 0..1.
    float gain;
    // 0 is left, 64 centre, 127 right.
    uint8_t pan;
} BkSound;

bool BkSoundBank_Open(BkSoundBank* bank, const uint8_t* rom, uint32_t romSize);
bool BkSoundBank_Decode(const BkSoundBank* bank, uint16_t soundIndex, BkSound* out);

#endif
