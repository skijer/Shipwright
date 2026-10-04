#pragma once

#include "z64math.h"
#include <math.h>

// Collision deliberately zeros gameplay velocity while the shot fades. Its
// visible plume must retain the launch heading instead of Unit(0)'s +X fallback.
// yaw/pitch survive the hit; spread is the same native binary-angle constant.
static inline Vec3f RodVisual_Heading(Vec3f velocity, s16 yaw, s16 pitch, s16 spread, unsigned index) {
    if (velocity.x != 0.0f || velocity.y != 0.0f || velocity.z != 0.0f)
        return velocity;
    const s16 headYaw = (s16)(yaw + (index == 1 ? -spread : index == 2 ? spread : 0));
    const float radians = 6.2831853071795864769f / 65536.0f;
    const float y = headYaw * radians, p = pitch * radians;
    return (Vec3f){ sinf(y) * cosf(p), -sinf(p), cosf(y) * cosf(p) };
}
