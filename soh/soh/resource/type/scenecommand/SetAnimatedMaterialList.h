#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>
#include <memory>
#include <ship/resource/Resource.h>
#include "SceneCommand.h"
#include "z64scene.h"

namespace SOH {

class SetAnimatedMaterialList : public SceneCommand<AnimatedMaterial> {
  public:
    using SceneCommand::SceneCommand;

    AnimatedMaterial* GetPointer();
    size_t GetPointerSize();

    // entries[i].params points into these deques, which never move elements on push_back.
    struct ScrollStorage {
        AnimatedMatTexScrollParams layers[2];
    };
    struct ColorStorage {
        AnimatedMatColorParams params;
        std::vector<u16> keyFrames;
        std::vector<F3DPrimColor> primColors;
        std::vector<F3DEnvColor> envColors;
    };
    struct CycleStorage {
        AnimatedMatTexCycleParams params;
        std::vector<std::string> texturePaths;
        std::vector<void*> textureList;
        std::vector<u16> frames;
    };

    AnimatedMaterial& AddScroll(u8 segment, u8 pass, u8 type, const ScrollStorage& storage);
    AnimatedMaterial& AddColor(u8 segment, u8 pass, u8 type, ColorStorage&& storage);
    AnimatedMaterial& AddCycle(u8 segment, u8 pass, CycleStorage&& storage);

    std::vector<AnimatedMaterial> entries;
    std::deque<ScrollStorage> scrolls;
    std::deque<ColorStorage> colors;
    std::deque<CycleStorage> cycles;
};

} // namespace SOH
