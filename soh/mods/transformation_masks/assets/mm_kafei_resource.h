#pragma once
#include "mm_normal_actor_resource.h"
#include "soh/resource/type/PlayerAnimation.h"
#include <cstring>
namespace MmKafei {
using Bytes = std::vector<char>;
inline uint32_t ReadLE32(const Bytes& b, size_t p) {
    return (uint8_t)b[p] | (uint32_t)(uint8_t)b[p + 1] << 8 | (uint32_t)(uint8_t)b[p + 2] << 16 |
           (uint32_t)(uint8_t)b[p + 3] << 24;
}
inline bool Header(const Bytes& b, uint32_t type) {
    return b.size() >= 64 && b[0] == 0 && ReadLE32(b, 4) == type && ReadLE32(b, 8) == 0;
}
inline const char* ClipPath(unsigned pose) {
    return pose == 0 ? "__OTR__misc/link_animetion/gPlayerAnim_link_normal_wait_free_Data"
                     : "__OTR__misc/link_animetion/gPlayerAnim_al_yareyare_Data";
}
inline bool Wrapper(const Bytes& b, unsigned pose) {
    if (pose > 1)
        return false;
    const char* path = ClipPath(pose);
    size_t length = strlen(path);
    unsigned frames = pose == 0 ? 89 : 48;
    return b.size() == 74 + length && Header(b, 0x4F414E4D) && ReadLE32(b, 64) == 1 &&
           ((uint8_t)b[68] | (unsigned)(uint8_t)b[69] << 8) == frames && ReadLE32(b, 70) == length &&
           memcmp(b.data() + 74, path, length) == 0;
}
inline bool Payload(const Bytes& b, unsigned pose) {
    if (pose > 1)
        return false;
    unsigned count = (pose == 0 ? 89 : 48) * 67;
    return b.size() == 68 + count * 2 && Header(b, 0x4F50414D) && ReadLE32(b, 64) == count;
}
struct LimbSignature {
    const char* name;
    int16_t pos[3];
    uint8_t child, sibling;
    const char* dl;
};
inline constexpr LimbSignature Limbs[] = {
    { "gKafeiRootLimb", { 0, 2376, 0 }, 1, 255, "" },
    { "gKafeiWaistLimb", { -4, -104, 0 }, 2, 9, "gKafeiWaistDL" },
    { "gKafeiLowerRootLimb", { 607, 0, 0 }, 3, 255, "" },
    { "gKafeiRightThighLimb", { -172, 50, -190 }, 4, 6, "gKafeiRightThighDL" },
    { "gKafeiRightShinLimb", { 697, 0, 0 }, 5, 255, "gKafeiRightShinDL" },
    { "gKafeiRightFootLimb", { 825, 5, 11 }, 255, 255, "gKafeiRightFootDL" },
    { "gKafeiLeftThighLimb", { -170, 57, 192 }, 7, 255, "gKafeiLeftThighDL" },
    { "gKafeiLeftShinLimb", { 695, 0, 0 }, 8, 255, "gKafeiLeftShinDL" },
    { "gKafeiLeftFootLimb", { 817, 8, 4 }, 255, 255, "gKafeiLeftFootDL" },
    { "gKafeiUpperRootLimb", { 0, -103, -7 }, 10, 255, "" },
    { "gKafeiHeadLimb", { 996, -201, -1 }, 11, 12, "gKafeiHeadDL" },
    { "gKafeiHatLimb", { -365, -670, 0 }, 255, 255, "gKafeiSunMaskEmptyDL" },
    { "gKafeiCollarLimb", { 0, 0, 0 }, 255, 13, "gKafeiSunMaskEmptyDL" },
    { "gKafeiLeftShoulderLimb", { 696, -175, 466 }, 14, 16, "gKafeiLeftShoulderDL" },
    { "gKafeiLeftForearmLimb", { 581, 0, 0 }, 15, 255, "gKafeiLeftForearmDL" },
    { "gKafeiLeftHandLimb", { 514, 0, 0 }, 255, 255, "gKafeiLeftHandDL" },
    { "gKafeiRightShoulderLimb", { 696, -175, -466 }, 17, 19, "gKafeiRightShoulderDL" },
    { "gKafeiRightForearmLimb", { 577, 0, 0 }, 18, 255, "gKafeiRightForearmDL" },
    { "gKafeiRightHandLimb", { 525, 0, 0 }, 255, 255, "gKafeiRightHandDL" },
    { "gKafeiSheathLimb", { 657, -550, 367 }, 255, 20, "gKafeiSunMaskEmptyDL" },
    { "gKafeiTorsoLimb", { 0, 0, 0 }, 255, 255, "gKafeiTorsoDL" },
};
inline std::string Path(const char* name) {
    return std::string("objects/object_test3/") + name;
}
inline bool SkeletonBytes(const Bytes& b) {
    if (b.size() != 983 || !Header(b, 0x4F534B4C) || b[64] != 1 || b[65] != 1 || ReadLE32(b, 66) != 21 ||
        ReadLE32(b, 70) != 18 || b[74] != 1 || ReadLE32(b, 75) != 21)
        return false;
    size_t at = 79;
    for (const auto& limb : Limbs) {
        std::string path = Path(limb.name);
        if (at + 4 > b.size() || ReadLE32(b, at) != path.size())
            return false;
        at += 4;
        if (path.size() > b.size() - at || memcmp(b.data() + at, path.data(), path.size()))
            return false;
        at += path.size();
    }
    return at == b.size();
}
inline bool LimbBytes(const Bytes& b, unsigned index) {
    if (index >= 21 || !Header(b, 0x4F534C42) || b.size() < 122 || b[64] != 2)
        return false;
    for (size_t i = 65; i < 106; ++i)
        if (b[i] != 0)
            return false;
    const auto& expected = Limbs[index];
    std::string dl = *expected.dl ? Path(expected.dl) : "";
    size_t at = 106;
    for (int i = 0; i < 2; ++i) {
        if (at + 4 > b.size() || ReadLE32(b, at) != dl.size())
            return false;
        at += 4;
        if (dl.size() > b.size() - at || memcmp(b.data() + at, dl.data(), dl.size()))
            return false;
        at += dl.size();
    }
    if (b.size() != at + 8)
        return false;
    for (int i = 0; i < 3; ++i) {
        uint16_t v = (uint8_t)b[at + i * 2] | (uint16_t)(uint8_t)b[at + i * 2 + 1] << 8;
        if ((int16_t)v != expected.pos[i])
            return false;
    }
    return (uint8_t)b[at + 6] == expected.child && (uint8_t)b[at + 7] == expected.sibling;
}
inline bool Limb(const std::shared_ptr<SOH::SkeletonLimb>& resource, unsigned index) {
    if (!resource || index >= 21 || resource->limbType != SOH::LimbType::LOD)
        return false;
    const auto& expected = Limbs[index];
    const auto& limb = resource->limbData.lodLimb;
    if (limb.jointPos.x != expected.pos[0] || limb.jointPos.y != expected.pos[1] ||
        limb.jointPos.z != expected.pos[2] || limb.child != expected.child || limb.sibling != expected.sibling)
        return false;
    std::string dl = *expected.dl ? "__OTR__" + Path(expected.dl) : "";
    return resource->dListPtr == dl && resource->dList2Ptr == dl &&
           limb.dLists[0] == (dl.empty() ? nullptr : (Gfx*)resource->dListPtr.c_str()) &&
           limb.dLists[1] == (dl.empty() ? nullptr : (Gfx*)resource->dList2Ptr.c_str());
}
} // namespace MmKafei
