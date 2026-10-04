#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "soh/resource/type/Skeleton.h"
#include "soh/resource/type/Animation.h"
#include <fast/resource/type/Texture.h>

namespace MmNormalActor {
using Resource = std::shared_ptr<Ship::IResource>;

/* Validate the owned children whose pointers the stock skeleton factory selected.
 * Resolving their paths again after an alt-cache unload can return different
 * resources, even though the original skeleton and its limbs are still valid.
 * Header tags alone cannot distinguish MM Kafei's misleading Standard/LOD graph. */
inline bool ValidateSkeleton(const Resource& resource, unsigned limbs, unsigned matrices,
                             std::vector<Resource>& retained) {
    auto skeleton = std::dynamic_pointer_cast<SOH::Skeleton>(resource);
    if (!skeleton || skeleton->type != SOH::SkeletonType::Flex || skeleton->limbType != SOH::LimbType::Standard ||
        skeleton->limbTableType != SOH::LimbType::Standard || skeleton->limbCount != (int)limbs ||
        skeleton->limbTableCount != (int)limbs || skeleton->dListCount != (int)matrices || limbs == 0 || limbs > 254 ||
        matrices > limbs || skeleton->limbTable.size() != limbs || skeleton->skeletonHeaderSegments.size() != limbs ||
        skeleton->limbResources.size() != limbs)
        return false;
    const auto& header = skeleton->skeletonData.flexSkeletonHeader;
    if (header.sh.limbCount != limbs || header.sh.skeletonType != (uint8_t)SOH::SkeletonType::Flex ||
        header.dListCount != matrices || header.sh.segment != skeleton->skeletonHeaderSegments.data())
        return false;
    std::vector<Resource> children;
    unsigned actualMatrices = 0;
    for (unsigned i = 0; i < limbs; ++i) {
        auto child = std::dynamic_pointer_cast<SOH::SkeletonLimb>(skeleton->limbResources[i]);
        if (!child || child->limbType != SOH::LimbType::Standard || header.sh.segment[i] != child->GetRawPointer())
            return false;
        const auto& limb = child->limbData.standardLimb;
        if ((limb.child != 255 && limb.child >= limbs) || (limb.sibling != 255 && limb.sibling >= limbs))
            return false;
        actualMatrices += limb.dList != nullptr;
        children.push_back(child);
    }
    if (actualMatrices != matrices)
        return false;
    std::vector<bool> visited(limbs, false);
    std::function<bool(unsigned)> walk = [&](unsigned index) {
        if (index == 255)
            return true;
        if (visited[index])
            return false;
        visited[index] = true;
        auto limb = std::static_pointer_cast<SOH::SkeletonLimb>(children[index]);
        return walk(limb->limbData.standardLimb.child) && walk(limb->limbData.standardLimb.sibling);
    };
    if (!walk(0))
        return false;
    for (bool seen : visited)
        if (!seen)
            return false;
    retained.push_back(resource);
    retained.insert(retained.end(), children.begin(), children.end());
    return true;
}

inline bool ValidateAnimation(const Resource& resource, unsigned limbs, unsigned frames) {
    auto animation = std::dynamic_pointer_cast<SOH::Animation>(resource);
    if (!animation || animation->type != SOH::AnimationType::Normal)
        return false;
    const auto& header = animation->animationData.animationHeader;
    if (!frames || header.common.frameCount != (int)frames || animation->rotationIndices.size() != limbs + 1 ||
        animation->rotationValues.empty() || header.staticIndexMax > animation->rotationValues.size() ||
        (void*)header.frameData != animation->rotationValues.data() ||
        (void*)header.jointIndices != animation->rotationIndices.data())
        return false;
    for (const auto& joint : animation->rotationIndices) {
        for (unsigned index : { joint.x, joint.y, joint.z }) {
            size_t end = index + (index < header.staticIndexMax ? 1 : frames);
            if (end > animation->rotationValues.size())
                return false;
        }
    }
    return true;
}

inline bool ValidateTexture(const Resource& resource) {
    auto texture = std::dynamic_pointer_cast<Fast::Texture>(resource);
    if (!(texture && texture->Type >= Fast::TextureType::RGBA32bpp &&
          texture->Type <= Fast::TextureType::GrayscaleAlpha16bpp && texture->Width && texture->Height &&
          texture->ImageData && texture->ImageDataSize && texture->GetPointer() == texture->ImageData))
        return false;
    if (texture->mImageBuffer) {
        uintptr_t begin = reinterpret_cast<uintptr_t>(texture->mImageBuffer->data());
        uintptr_t pixels = reinterpret_cast<uintptr_t>(texture->ImageData);
        size_t size = texture->mImageBuffer->size();
        if (pixels < begin || pixels - begin > size || texture->ImageDataSize > size - (pixels - begin))
            return false;
    }
    if (texture->Flags == 0) {
        static constexpr unsigned bits[] = { 0, 32, 16, 4, 8, 4, 8, 4, 8, 16 };
        size_t required = ((size_t)texture->Width * texture->Height * bits[(unsigned)texture->Type] + 7) / 8;
        if (texture->ImageDataSize < required)
            return false;
    }
    return true;
}
} // namespace MmNormalActor
