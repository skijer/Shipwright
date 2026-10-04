#ifndef AUDIO_SEQUENCE_DATA_H
#define AUDIO_SEQUENCE_DATA_H

#include <stdint.h>

// Shared host representation for the C engine and C++ resource importer.
// Archive font operands remain bytes; only the in-memory IDs are widened.
typedef struct {
    char* seqData;
    int32_t seqDataSize;
    uint16_t seqNumber;
    uint8_t medium;
    uint8_t cachePolicy;
    int32_t numFonts;
    int32_t fonts[16];
    int32_t resolvedFont; // -1 = use the per-entry font list.
} AudioSequenceData;

// Streamed XML packs encode a CRC as eight byte-valued FontIdx entries.
// Decode explicitly: widening the host array must not change this pack format.
static inline uint64_t AudioSequence_GetFontHash(const AudioSequenceData* sequence) {
    uint64_t hash = 0;
    for (int i = 0; i < 8; ++i) {
        hash |= (uint64_t)(uint8_t)sequence->fonts[i] << (8 * i);
    }
    return hash;
}

static inline int32_t AudioSequence_GetFont(const AudioSequenceData* sequence, int32_t index) {
    if (sequence->numFonts < 0 || sequence->numFonts > 16 || index < 0 || index >= sequence->numFonts) {
        return -1;
    }
    return sequence->resolvedFont >= 0 ? sequence->resolvedFont : sequence->fonts[index];
}

#endif
