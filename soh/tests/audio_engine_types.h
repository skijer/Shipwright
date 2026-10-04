#pragma once
// Actual engine structures with opaque platform dependencies. No OS task runs
// in these tests; resource I/O and the audio backend are boundary fixtures.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef float f32;
typedef double f64;
typedef void* OSMesg;
typedef struct {
    int unused;
} OSMesgQueue;
typedef struct {
    int unused;
} OSIoMesg;
typedef struct {
    int unused;
} OSTask;
typedef struct {
    int unused;
} OSPiHandle;
typedef struct {
    int unused;
} Acmd;
#include "z64audio.h"
