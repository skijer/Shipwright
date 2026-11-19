#pragma once
#include <libultraship/libultra.h>
#include "z64math.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

struct DeclaredActorType {
    static constexpr const char* kOtrPrefix = "__OTR__";
    static constexpr size_t kOtrPrefixLength = sizeof("__OTR__") - 1;
    static constexpr u8 kSegmentMin = 8;
    static constexpr u8 kSegmentMax = 12;

    std::string name;
    std::string displayName;

    std::string skeleton;
    std::string animation;
    bool holdFrame = false;
    f32 frame = 0.0f;
    f32 speed = 1.0f;
    std::string displayList;
    bool translucent = false;
    f32 scale = 0.01f;
    f32 yOffset = 0.0f;
    f32 shadow = 0.0f;
    f32 cullRadius = 0.0f;
    f32 drawDistance = 0.0f;
    std::vector<std::pair<u8, std::string>> segments;
    std::vector<s32> hideLimbs;

    s16 radius = 0;
    s16 height = 0;
    s16 yShift = 0;

    bool talks = false;
    u16 message = 0;
    f32 talkRange = 0.0f;

    bool looks = false;
    s32 limb = 0;
    f32 pivot = 0.0f;
    f32 lookRange = 200.0f;
    Vec3f turnAxis = { 1.0f, 0.0f, 0.0f };
    Vec3f nodAxis = { 0.0f, 0.0f, 1.0f };

    bool HasCollision() const {
        return radius > 0 && height > 0;
    }
};
