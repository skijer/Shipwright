#pragma once

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int32_t s32;
typedef float f32;

#define _SHIFTL(value, shift, width) (((u32)(value) & ((1U << (width)) - 1)) << (shift))
