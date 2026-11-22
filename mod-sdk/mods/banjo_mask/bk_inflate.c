#include "bk_inflate.h"

#include <string.h>

#define MAX_BITS 15
#define LITERAL_CODES 288
#define DISTANCE_CODES 30
#define CODE_LENGTH_CODES 19
#define END_OF_BLOCK 256

typedef struct Huffman {
    uint16_t counts[MAX_BITS + 1];
    uint16_t symbols[LITERAL_CODES];
} Huffman;

typedef struct Stream {
    const uint8_t* input;
    uint32_t inputSize;
    uint32_t inputAt;
    uint32_t bitBuffer;
    int32_t bitCount;
    uint8_t* output;
    uint32_t outputSize;
    uint32_t outputAt;
    bool hasFailed;
} Stream;

static const uint16_t kLengthBase[29] = { 3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                          31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t kLengthExtra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t kDistanceBase[30] = { 1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                            193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const uint8_t kDistanceExtra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
static const uint8_t kCodeLengthOrder[CODE_LENGTH_CODES] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

static uint32_t ReadBits(Stream* stream, int32_t count) {
    while (stream->bitCount < count) {
        if (stream->inputAt >= stream->inputSize) {
            stream->hasFailed = true;
            return 0;
        }
        stream->bitBuffer |= (uint32_t)stream->input[stream->inputAt++] << stream->bitCount;
        stream->bitCount += 8;
    }

    const uint32_t value = stream->bitBuffer & ((1u << count) - 1);

    stream->bitBuffer >>= count;
    stream->bitCount -= count;
    return value;
}

static void BuildHuffman(Huffman* huffman, const uint8_t* lengths, int32_t count) {
    uint16_t offsets[MAX_BITS + 1];

    memset(huffman->counts, 0, sizeof(huffman->counts));
    for (int32_t symbol = 0; symbol < count; symbol++) {
        huffman->counts[lengths[symbol]]++;
    }
    huffman->counts[0] = 0;
    offsets[1] = 0;
    for (int32_t bits = 1; bits < MAX_BITS; bits++) {
        offsets[bits + 1] = offsets[bits] + huffman->counts[bits];
    }
    for (int32_t symbol = 0; symbol < count; symbol++) {
        if (lengths[symbol] != 0) {
            huffman->symbols[offsets[lengths[symbol]]++] = (uint16_t)symbol;
        }
    }
}

// Canonical codes are read one bit at a time, most significant first.
static int32_t DecodeSymbol(Stream* stream, const Huffman* huffman) {
    int32_t code = 0;
    int32_t first = 0;
    int32_t index = 0;

    for (int32_t bits = 1; bits <= MAX_BITS; bits++) {
        code |= (int32_t)ReadBits(stream, 1);
        const int32_t count = huffman->counts[bits];

        if (code - count < first) {
            return huffman->symbols[index + (code - first)];
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    stream->hasFailed = true;
    return END_OF_BLOCK;
}

static void PutByte(Stream* stream, uint8_t value) {
    if (stream->outputAt >= stream->outputSize) {
        stream->hasFailed = true;
        return;
    }
    stream->output[stream->outputAt++] = value;
}

static void InflateCodes(Stream* stream, const Huffman* literals, const Huffman* distances) {
    while (!stream->hasFailed) {
        const int32_t symbol = DecodeSymbol(stream, literals);

        if (symbol < END_OF_BLOCK) {
            PutByte(stream, (uint8_t)symbol);
            continue;
        }
        if (symbol == END_OF_BLOCK || symbol - 257 >= 29) {
            return;
        }

        const int32_t lengthIndex = symbol - 257;
        const uint32_t length = kLengthBase[lengthIndex] + ReadBits(stream, kLengthExtra[lengthIndex]);
        const int32_t distanceIndex = DecodeSymbol(stream, distances);

        if (distanceIndex >= DISTANCE_CODES) {
            stream->hasFailed = true;
            return;
        }

        const uint32_t distance = kDistanceBase[distanceIndex] + ReadBits(stream, kDistanceExtra[distanceIndex]);

        if (distance > stream->outputAt) {
            stream->hasFailed = true;
            return;
        }
        for (uint32_t copied = 0; copied < length; copied++) {
            PutByte(stream, stream->output[stream->outputAt - distance]);
        }
    }
}

static void InflateStored(Stream* stream) {
    stream->bitBuffer = 0;
    stream->bitCount = 0;
    if (stream->inputAt + 4 > stream->inputSize) {
        stream->hasFailed = true;
        return;
    }

    const uint32_t length = stream->input[stream->inputAt] | (stream->input[stream->inputAt + 1] << 8);

    stream->inputAt += 4;
    if (stream->inputAt + length > stream->inputSize) {
        stream->hasFailed = true;
        return;
    }
    for (uint32_t index = 0; index < length; index++) {
        PutByte(stream, stream->input[stream->inputAt++]);
    }
}

static void InflateFixed(Stream* stream) {
    uint8_t lengths[LITERAL_CODES];
    Huffman literals;
    Huffman distances;

    memset(lengths, 8, 144);
    memset(lengths + 144, 9, 112);
    memset(lengths + 256, 7, 24);
    memset(lengths + 280, 8, 8);
    BuildHuffman(&literals, lengths, LITERAL_CODES);
    memset(lengths, 5, DISTANCE_CODES);
    BuildHuffman(&distances, lengths, DISTANCE_CODES);
    InflateCodes(stream, &literals, &distances);
}

static void InflateDynamic(Stream* stream) {
    uint8_t lengths[LITERAL_CODES + DISTANCE_CODES] = { 0 };
    Huffman codeLengths;
    Huffman literals;
    Huffman distances;
    const int32_t literalCount = (int32_t)ReadBits(stream, 5) + 257;
    const int32_t distanceCount = (int32_t)ReadBits(stream, 5) + 1;
    const int32_t codeLengthCount = (int32_t)ReadBits(stream, 4) + 4;

    for (int32_t index = 0; index < codeLengthCount; index++) {
        lengths[kCodeLengthOrder[index]] = (uint8_t)ReadBits(stream, 3);
    }
    BuildHuffman(&codeLengths, lengths, CODE_LENGTH_CODES);
    memset(lengths, 0, CODE_LENGTH_CODES);
    for (int32_t index = 0; index < literalCount + distanceCount && !stream->hasFailed;) {
        const int32_t symbol = DecodeSymbol(stream, &codeLengths);
        int32_t repeat = 0;
        uint8_t value = 0;

        if (symbol < 16) {
            lengths[index++] = (uint8_t)symbol;
            continue;
        }
        if (symbol == 16) {
            if (index == 0) {
                stream->hasFailed = true;
                return;
            }
            value = lengths[index - 1];
            repeat = 3 + (int32_t)ReadBits(stream, 2);
        } else if (symbol == 17) {
            repeat = 3 + (int32_t)ReadBits(stream, 3);
        } else {
            repeat = 11 + (int32_t)ReadBits(stream, 7);
        }
        if (index + repeat > literalCount + distanceCount) {
            stream->hasFailed = true;
            return;
        }
        while (repeat-- > 0) {
            lengths[index++] = value;
        }
    }
    BuildHuffman(&literals, lengths, literalCount);
    BuildHuffman(&distances, lengths + literalCount, distanceCount);
    InflateCodes(stream, &literals, &distances);
}

bool BkInflate_Decode(const uint8_t* input, uint32_t inputSize, uint8_t* output, uint32_t outputSize) {
    Stream stream = { input, inputSize, 0, 0, 0, output, outputSize, 0, false };
    uint32_t isLastBlock = 0;

    while (!isLastBlock && !stream.hasFailed) {
        isLastBlock = ReadBits(&stream, 1);
        switch (ReadBits(&stream, 2)) {
            case 0:
                InflateStored(&stream);
                break;
            case 1:
                InflateFixed(&stream);
                break;
            case 2:
                InflateDynamic(&stream);
                break;
            default:
                stream.hasFailed = true;
                break;
        }
    }
    return !stream.hasFailed && stream.outputAt == outputSize;
}
