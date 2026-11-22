#ifndef BK_INFLATE_H
#define BK_INFLATE_H

#include <stdbool.h>
#include <stdint.h>

// Raw DEFLATE (RFC 1951), the stream inside Banjo-Kazooie's rarezip assets. Fails on corrupt input or overflow.
bool BkInflate_Decode(const uint8_t* input, uint32_t inputSize, uint8_t* output, uint32_t outputSize);

#endif
