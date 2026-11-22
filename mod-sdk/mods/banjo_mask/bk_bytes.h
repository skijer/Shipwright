#ifndef BK_BYTES_H
#define BK_BYTES_H

#include <stdint.h>
#include <string.h>

// Banjo-Kazooie data is big endian.
static inline uint16_t BkBytes_U16(const uint8_t* at) {
    return (uint16_t)((at[0] << 8) | at[1]);
}

static inline int16_t BkBytes_S16(const uint8_t* at) {
    return (int16_t)BkBytes_U16(at);
}

static inline uint32_t BkBytes_U32(const uint8_t* at) {
    return ((uint32_t)at[0] << 24) | ((uint32_t)at[1] << 16) | ((uint32_t)at[2] << 8) | at[3];
}

static inline int32_t BkBytes_S32(const uint8_t* at) {
    return (int32_t)BkBytes_U32(at);
}

static inline float BkBytes_F32(const uint8_t* at) {
    const uint32_t bits = BkBytes_U32(at);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

#endif
