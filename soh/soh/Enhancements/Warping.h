#pragma once

#include <cstdint>
#include <string>

extern "C" {
#include "z64math.h"
}

typedef struct WarpPoint {
    int32_t entranceId = 0;
    std::string entranceName;
    int16_t roomNum = 0;
    Vec3f pos = { 0.0f, 0.0f, 0.0f };
    int16_t rotY = 0;
    bool bootToPoint = false;
    int32_t linkAge = 0;
    int32_t dayTime = 0x8000;
} WarpPoint;

void Warping_WarpToEntrance(int32_t entranceId, int32_t linkAge, int32_t dayTime);

int32_t Warping_ResolveEntrance(const WarpPoint& point);

bool Warping_WarpToPoint(const WarpPoint& point);
