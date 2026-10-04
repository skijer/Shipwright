#pragma once

#include <math.h>
#include <stddef.h>

// Presentation-only reconstruction. All three heads share a spawn point and
// pitch; their constant flight vectors differ only by +/- the authored yaw.
// Rotate the center history about the current head: this also retains stopped
// history and repeated startup samples without advancing gameplay state.
typedef struct NeiIceTrailPoint {
    float x, y, z;
} NeiIceTrailPoint;

static inline void NeiIceTrail_Reconstruct(const NeiIceTrailPoint* center, size_t count, NeiIceTrailPoint head,
                                           unsigned headIndex, NeiIceTrailPoint* output) {
    if (!center || !output || count == 0)
        return;
    // Exact integer conversion used by IceRod_InitTripleProjectile (30 degrees).
    const float angle = (headIndex == 1   ? -1.0f
                         : headIndex == 2 ? 1.0f
                                          : 0.0f) *
                        (30 * (0x10000 / 360)) * (6.2831853071795864769f / 65536.0f);
    const float s = sinf(angle), c = cosf(angle);
    for (size_t i = 0; i < count; ++i) {
        const float x = center[i].x - center[0].x;
        const float y = center[i].y - center[0].y;
        const float z = center[i].z - center[0].z;
        output[i].x = head.x + c * x + s * z;
        output[i].y = head.y + y;
        output[i].z = head.z - s * x + c * z;
    }
}
