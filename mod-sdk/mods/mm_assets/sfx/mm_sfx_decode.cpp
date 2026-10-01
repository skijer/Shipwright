// Decodificación de una muestra VADPCM entera a PCM, que es lo que el sintetizador aislado cachea para cada
// muestra de MM. El algoritmo es el de aADPCMdecImpl (soh/soh/mixer.c), frame a frame.
#include "z64.h"

#include <cstdlib>
#include <cstring>

namespace {

constexpr int32_t FrameBytesAdpcm = 9;
constexpr int32_t FrameBytesSmallAdpcm = 5;
constexpr int32_t SamplesPerFrame = 16;
constexpr int32_t CodecAdpcm = 0;
constexpr int32_t CodecSmallAdpcm = 3;

int16_t Clamp16(int32_t value) {
    return static_cast<int16_t>(value > 32767 ? 32767 : (value < -32768 ? -32768 : value));
}

// Los nibbles se escalan por el shift de la cabecera antes de predecir; el códec pequeño trae 2 bits por
// muestra en vez de 4.
void ReadSubframeInputs(const uint8_t* data, int32_t frameBytes, int32_t half, int32_t shift, int16_t inputs[8]) {
    if (frameBytes == FrameBytesSmallAdpcm) {
        const uint8_t* bytes = &data[half * 2];
        for (int32_t j = 0; j < 2; j++) {
            inputs[j * 4] = static_cast<int16_t>((((int32_t)(bytes[j] >> 6) << 30) >> 30) << shift);
            inputs[j * 4 + 1] = static_cast<int16_t>(((((int32_t)(bytes[j] >> 4) & 0x3) << 30) >> 30) << shift);
            inputs[j * 4 + 2] = static_cast<int16_t>(((((int32_t)(bytes[j] >> 2) & 0x3) << 30) >> 30) << shift);
            inputs[j * 4 + 3] = static_cast<int16_t>(((((int32_t)bytes[j] & 0x3) << 30) >> 30) << shift);
        }
        return;
    }
    const uint8_t* bytes = &data[half * 4];
    for (int32_t j = 0; j < 4; j++) {
        inputs[j * 2] = static_cast<int16_t>((((int32_t)(bytes[j] >> 4) << 28) >> 28) << shift);
        inputs[j * 2 + 1] = static_cast<int16_t>(((((int32_t)bytes[j] & 0xF) << 28) >> 28) << shift);
    }
}

// Cada frame son dos subframes de 8 muestras. La predicción suma sobre las ENTRADAS del subframe, no sobre
// lo ya decodificado, igual que mixer.c: cambiarlo da un sonido parecido y equivocado.
void DecodeFrame(const uint8_t* frame, int32_t frameBytes, const int16_t* book, int32_t order, int16_t history[2],
                 int16_t* out) {
    const int32_t shift = frame[0] >> 4;
    const int32_t predictor = frame[0] & 0xF;
    const int16_t* coefPrev2 = &book[predictor * order * 8];
    const int16_t* coefPrev1 = &book[predictor * order * 8 + 8];

    for (int32_t half = 0; half < 2; half++) {
        int16_t inputs[8];
        ReadSubframeInputs(&frame[1], frameBytes, half, shift, inputs);

        for (int32_t j = 0; j < 8; j++) {
            int32_t acc = coefPrev2[j] * history[0] + coefPrev1[j] * history[1] + ((int32_t)inputs[j] << 11);
            for (int32_t k = 0; k < j; k++) {
                acc += coefPrev1[(j - k) - 1] * inputs[k];
            }
            out[half * 8 + j] = Clamp16(acc >> 11);
        }
        history[0] = out[half * 8 + 6];
        history[1] = out[half * 8 + 7];
    }
}

} // namespace

// Devuelve un buffer de malloc que pasa a ser del llamador, o NULL si la muestra no es ADPCM decodificable.
extern "C" short* MmSfxDecode_Sample(void* soundFontSample, unsigned int* outLen) {
    const SoundFontSample* sample = static_cast<const SoundFontSample*>(soundFontSample);
    if (sample == nullptr || sample->sampleAddr == nullptr || sample->size == 0 || sample->book == nullptr ||
        sample->book->book == nullptr) {
        return nullptr;
    }

    int32_t frameBytes;
    if (sample->codec == CodecAdpcm) {
        frameBytes = FrameBytesAdpcm;
    } else if (sample->codec == CodecSmallAdpcm) {
        frameBytes = FrameBytesSmallAdpcm;
    } else {
        return nullptr;
    }

    uint32_t frameCount = sample->size / frameBytes;
    uint32_t sampleCount = frameCount * SamplesPerFrame;
    // Una muestra en bucle no se reproduce más allá del final del bucle.
    if (sample->loop != nullptr && sample->loop->loopEnd > 0 && sample->loop->loopEnd < sampleCount) {
        sampleCount = sample->loop->loopEnd;
        frameCount = (sampleCount + SamplesPerFrame - 1) / SamplesPerFrame;
    }

    int16_t* pcm = static_cast<int16_t*>(malloc(sampleCount * sizeof(int16_t)));
    if (pcm == nullptr) {
        return nullptr;
    }
    const uint8_t* source = static_cast<const uint8_t*>(sample->sampleAddr);
    int16_t history[2] = { 0, 0 };

    for (uint32_t f = 0; f < frameCount; f++) {
        int16_t frame[SamplesPerFrame];
        DecodeFrame(&source[f * frameBytes], frameBytes, sample->book->book, sample->book->order, history, frame);

        const uint32_t offset = f * SamplesPerFrame;
        const uint32_t count = offset + SamplesPerFrame > sampleCount ? sampleCount - offset : SamplesPerFrame;
        memcpy(&pcm[offset], frame, count * sizeof(int16_t));
    }

    *outLen = sampleCount;
    return pcm;
}
