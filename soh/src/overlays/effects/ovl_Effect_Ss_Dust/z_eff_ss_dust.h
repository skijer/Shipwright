#ifndef Z_EFF_SS_DUST_H
#define Z_EFF_SS_DUST_H

#include <libultraship/libultra.h>
#include "global.h"

// Private SW97 tornado appearance tag. Low bits 0x1/0x2 select native draw
// modes and 0x4 selects randomized colors; 0x100 changes neither behavior.
#define EFFECT_SS_DUST_DRAW_SW97_FOREST 0x100

typedef struct {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ Vec3f velocity;
    /* 0x18 */ Vec3f accel;
    /* 0x24 */ Color_RGBA8 primColor;
    /* 0x28 */ Color_RGBA8 envColor;
    /* 0x2C */ s16 scale;
    /* 0x2E */ s16 scaleStep;
    /* 0x30 */ s16 life;
    /* 0x32 */ u16 drawFlags;
    /* 0x34 */ u8 updateMode;
} EffectSsDustInitParams; // size = 0x38

#endif
