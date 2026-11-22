#include "../../sm64_build.h"
#include <stdint.h>

#include "utils.h"

int read_s16_be(unsigned char* buf) {
    return (int16_t)read_u16_be(buf);
}

float read_f32_be(unsigned char* buf) {
    union {
        uint32_t i;
        float f;
    } ret;

    ret.i = read_u32_be(buf);
    return ret.f;
}
