#pragma once
#include <cstdint>

struct AdpcmLoop {
    uint32_t start;
    uint32_t loopEnd;
    uint32_t count;
    uint32_t sampleEnd;
    int16_t predictorState[16];
};
struct AdpcmBook {
    int32_t order;
    int32_t npredictors;
    int16_t* book;
};
// Named fields match the resource boundary; no binary asset is loaded by tests.
struct SoundFontSample {
    uint32_t codec;
    uint32_t medium;
    uint32_t unk_bit26;
    uint32_t isRelocated;
    uint32_t size;
    uint32_t fileSize;
    uint8_t* sampleAddr;
    AdpcmLoop* loop;
    AdpcmBook* book;
};
