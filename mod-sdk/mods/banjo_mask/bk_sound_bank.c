#include "bk_sound_bank.h"

#include <math.h>
#include <stdlib.h>

#include "bk_bytes.h"

// USA 1.0 offsets of the sfx bank's .ctl and .tbl.
#define SFX_CTL_OFFSET 0xD846C0u
#define SFX_CTL_SIZE 0x10DF0u
#define SFX_TBL_OFFSET 0xD954B0u
#define BANK_FILE_REVISION 0x4231
#define WAVE_ADPCM 0
#define WAVE_RAW16 1
#define ADPCM_FRAME_BYTES 9
#define ADPCM_FRAME_SAMPLES 16
#define ADPCM_HALF 8
#define ADPCM_ORDER_MAX 8
#define ADPCM_PREDICTORS_MAX 16
#define ADPCM_COEFFICIENT_ONE 2048
#define VOLUME_MAX 127.0f
#define MIDDLE_C_CENTS 6000

typedef struct AdpcmTable {
    int32_t order;
    // [predictor][output sample][order previous samples + 8 frame inputs], as vadpcm_dec builds it.
    int32_t rows[ADPCM_PREDICTORS_MAX][ADPCM_HALF][ADPCM_ORDER_MAX + ADPCM_HALF];
} AdpcmTable;

static bool IsInside(uint32_t offset, uint32_t size, uint32_t limit) {
    return offset <= limit && size <= limit - offset;
}

bool BkSoundBank_Open(BkSoundBank* bank, const uint8_t* rom, uint32_t romSize) {
    if (!IsInside(SFX_CTL_OFFSET, SFX_CTL_SIZE, romSize) || SFX_TBL_OFFSET >= romSize) {
        return false;
    }
    bank->ctl = rom + SFX_CTL_OFFSET;
    bank->ctlSize = SFX_CTL_SIZE;
    bank->tbl = rom + SFX_TBL_OFFSET;
    bank->tblSize = romSize - SFX_TBL_OFFSET;
    if (BkBytes_U16(bank->ctl) != BANK_FILE_REVISION) {
        return false;
    }

    const uint32_t bankOffset = BkBytes_U32(bank->ctl + 4);

    if (!IsInside(bankOffset, 16, bank->ctlSize)) {
        return false;
    }
    bank->sampleRate = BkBytes_U32(bank->ctl + bankOffset + 4);
    bank->instrumentOffset = BkBytes_U32(bank->ctl + bankOffset + 12);
    if (!IsInside(bank->instrumentOffset, 16, bank->ctlSize)) {
        return false;
    }
    bank->soundCount = BkBytes_U16(bank->ctl + bank->instrumentOffset + 14);
    return IsInside(bank->instrumentOffset + 16, bank->soundCount * 4u, bank->ctlSize);
}

static bool ReadAdpcmTable(const BkSoundBank* bank, uint32_t bookOffset, AdpcmTable* table) {
    if (!IsInside(bookOffset, 8, bank->ctlSize)) {
        return false;
    }

    const int32_t order = BkBytes_S32(bank->ctl + bookOffset);
    const int32_t predictors = BkBytes_S32(bank->ctl + bookOffset + 4);
    const uint8_t* book = bank->ctl + bookOffset + 8;

    if (order < 1 || order > ADPCM_ORDER_MAX || predictors < 1 || predictors > ADPCM_PREDICTORS_MAX ||
        !IsInside(bookOffset + 8, (uint32_t)(order * predictors * ADPCM_HALF * 2), bank->ctlSize)) {
        return false;
    }
    table->order = order;
    for (int32_t predictor = 0; predictor < predictors; predictor++) {
        int32_t (*rows)[ADPCM_ORDER_MAX + ADPCM_HALF] = table->rows[predictor];

        for (int32_t tap = 0; tap < order; tap++) {
            for (int32_t sample = 0; sample < ADPCM_HALF; sample++) {
                rows[sample][tap] = BkBytes_S16(book + ((predictor * order + tap) * ADPCM_HALF + sample) * 2);
            }
        }
        for (int32_t sample = 1; sample < ADPCM_HALF; sample++) {
            rows[sample][order] = rows[sample - 1][order - 1];
        }
        rows[0][order] = ADPCM_COEFFICIENT_ONE;
        for (int32_t input = 1; input < ADPCM_HALF; input++) {
            for (int32_t sample = 0; sample < ADPCM_HALF; sample++) {
                rows[sample][input + order] = (sample < input) ? 0 : rows[sample - input][order];
            }
        }
    }
    return true;
}

static int16_t ClampSample(int32_t value) {
    return (int16_t)(value < INT16_MIN ? INT16_MIN : (value > INT16_MAX ? INT16_MAX : value));
}

// Floor division by 2048, the rounding vadpcm_dec's inner product uses.
static int32_t ScaleDown(int32_t sum) {
    return (sum >= 0) ? sum / ADPCM_COEFFICIENT_ONE : -((-sum + ADPCM_COEFFICIENT_ONE - 1) / ADPCM_COEFFICIENT_ONE);
}

static void DecodeFrame(const uint8_t* frame, const AdpcmTable* table, int16_t* previous, int16_t* out) {
    const int32_t scale = 1 << (frame[0] >> 4);
    const int32_t predictor = frame[0] & 0xF;
    const int32_t order = table->order;
    int32_t inputs[ADPCM_ORDER_MAX + ADPCM_HALF];

    for (int32_t half = 0; half < 2; half++) {
        for (int32_t tap = 0; tap < order; tap++) {
            inputs[tap] = (half == 0) ? previous[ADPCM_FRAME_SAMPLES - order + tap] : out[ADPCM_HALF - order + tap];
        }
        for (int32_t sample = 0; sample < ADPCM_HALF; sample++) {
            const uint8_t packed = frame[1 + half * 4 + sample / 2];
            const int32_t nibble = (sample % 2 == 0) ? (packed >> 4) : (packed & 0xF);

            inputs[order + sample] = ((nibble >= 8) ? nibble - 16 : nibble) * scale;
        }
        for (int32_t sample = 0; sample < ADPCM_HALF; sample++) {
            const int32_t* row = table->rows[predictor][sample];
            int32_t sum = 0;

            for (int32_t at = 0; at < order + ADPCM_HALF; at++) {
                sum += row[at] * inputs[at];
            }
            out[half * ADPCM_HALF + sample] = ClampSample(ScaleDown(sum));
        }
    }
    for (int32_t sample = 0; sample < ADPCM_FRAME_SAMPLES; sample++) {
        previous[sample] = out[sample];
    }
}

static bool DecodeAdpcm(const BkSoundBank* bank, uint32_t base, uint32_t length, uint32_t bookOffset, BkSound* out) {
    static AdpcmTable sTable;
    int16_t previous[ADPCM_FRAME_SAMPLES] = { 0 };
    const uint32_t frames = length / ADPCM_FRAME_BYTES;

    if (!ReadAdpcmTable(bank, bookOffset, &sTable)) {
        return false;
    }
    out->samples = malloc(frames * ADPCM_FRAME_SAMPLES * sizeof(int16_t));
    if (out->samples == NULL) {
        return false;
    }
    for (uint32_t frame = 0; frame < frames; frame++) {
        DecodeFrame(bank->tbl + base + frame * ADPCM_FRAME_BYTES, &sTable, previous,
                    out->samples + frame * ADPCM_FRAME_SAMPLES);
    }
    out->count = frames * ADPCM_FRAME_SAMPLES;
    return true;
}

static bool DecodeRaw16(const BkSoundBank* bank, uint32_t base, uint32_t length, BkSound* out) {
    out->count = length / 2;
    out->samples = malloc(out->count * sizeof(int16_t));
    if (out->samples == NULL) {
        return false;
    }
    for (uint32_t sample = 0; sample < out->count; sample++) {
        out->samples[sample] = BkBytes_S16(bank->tbl + base + sample * 2);
    }
    return true;
}

bool BkSoundBank_Decode(const BkSoundBank* bank, uint16_t soundIndex, BkSound* out) {
    if (soundIndex >= bank->soundCount) {
        return false;
    }

    const uint8_t* ctl = bank->ctl;
    const uint32_t soundOffset = BkBytes_U32(ctl + bank->instrumentOffset + 16 + soundIndex * 4u);

    if (!IsInside(soundOffset, 15, bank->ctlSize)) {
        return false;
    }

    const uint32_t envelopeOffset = BkBytes_U32(ctl + soundOffset);
    const uint32_t keyMapOffset = BkBytes_U32(ctl + soundOffset + 4);
    const uint32_t waveOffset = BkBytes_U32(ctl + soundOffset + 8);

    if (!IsInside(envelopeOffset, 14, bank->ctlSize) || !IsInside(keyMapOffset, 6, bank->ctlSize) ||
        !IsInside(waveOffset, 16, bank->ctlSize)) {
        return false;
    }

    const uint32_t base = BkBytes_U32(ctl + waveOffset);
    const uint32_t length = BkBytes_U32(ctl + waveOffset + 4);
    const uint8_t type = ctl[waveOffset + 8];
    const int32_t keyBase = ctl[keyMapOffset + 4];
    const int32_t detune = (int8_t)ctl[keyMapOffset + 5];
    const float attackVolume = ctl[envelopeOffset + 12] / VOLUME_MAX;

    if (!IsInside(base, length, bank->tblSize)) {
        return false;
    }
    out->pitchRatio = powf(2.0f, (keyBase * 100 + detune - MIDDLE_C_CENTS) / 1200.0f);
    out->gain = (ctl[soundOffset + 13] / VOLUME_MAX) * attackVolume;
    out->pan = ctl[soundOffset + 12];
    if (type == WAVE_ADPCM) {
        return DecodeAdpcm(bank, base, length, BkBytes_U32(ctl + waveOffset + 16), out);
    }
    return type == WAVE_RAW16 && DecodeRaw16(bank, base, length, out);
}
