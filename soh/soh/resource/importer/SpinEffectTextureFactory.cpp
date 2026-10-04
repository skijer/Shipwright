#include "SpinEffectTextureFactory.h"

#include "fast/resource/factory/TextureFactory.h"
#include "fast/resource/type/Texture.h"
#include "spdlog/spdlog.h"
#include <bit>
#include <cmath>
#include <limits>
#include <string_view>
#include <vector>

namespace SOH {
namespace {

struct NativeTextureProfile {
    std::string_view path;
    uint32_t width;
    uint32_t height;
    Fast::TextureType type;
    bool legacySpin = false;
};

const NativeTextureProfile* GetNativeProfile(std::string_view path) {
    if (path.starts_with("alt/")) {
        path.remove_prefix(4);
    }
    using enum Fast::TextureType;
    // Native profiles are verified against asset XML and the consuming effect
    // materials. Private profiles use the same native domains as their hooks.
    // Keep the four existing spin paths' integer-scale/I8 restrictions intact.
    static constexpr NativeTextureProfile profiles[] = {
        { "objects/gameplay_keep/gTorchFlameTex", 64, 32, Grayscale8bpp, true },
        { "objects/gameplay_keep/gEffUnknown1Tex", 64, 32, Grayscale8bpp, true },
        { "objects/gameplay_keep/gFlameWall1Tex", 32, 32, Grayscale8bpp, true },
        { "objects/gameplay_keep/gFlameWall2Tex", 32, 32, Grayscale8bpp, true },
        { "objects/gameplay_keep/gDecorativeFlameTex", 32, 64, Grayscale8bpp },
        { "objects/gameplay_keep/gDecorativeFlameMaskTex", 32, 128, Grayscale4bpp },
        { "overlays/ovl_Arrow_Fire/s1Tex", 32, 64, Grayscale8bpp },
        { "overlays/ovl_Arrow_Fire/s2Tex", 32, 64, Grayscale8bpp },
        { "overlays/ovl_Arrow_Ice/s1Tex", 32, 64, Grayscale8bpp },
        { "overlays/ovl_Arrow_Ice/s2Tex", 32, 64, Grayscale8bpp },
        { "overlays/ovl_Arrow_Light/s1Tex", 32, 64, Grayscale8bpp },
        { "overlays/ovl_Arrow_Light/s2Tex", 32, 64, Grayscale8bpp },
        { "overlays/ovl_Magic_Fire/sTex", 64, 64, Grayscale8bpp },
        { "overlays/ovl_Magic_Wind/sTex", 64, 64, Grayscale8bpp },
        { "overlays/ovl_Magic_Dark/sDiamondTex", 32, 64, Grayscale8bpp },
        { "custom/medallion_magic/arrows/shadow/s1Tex", 32, 64, Grayscale8bpp },
        { "custom/medallion_magic/arrows/shadow/s2Tex", 32, 64, Grayscale8bpp },
        { "custom/medallion_magic/arrows/water/s1Tex", 32, 64, Grayscale8bpp },
        { "custom/medallion_magic/arrows/water/s2Tex", 32, 64, Grayscale8bpp },
        { "custom/medallion_magic/arrows/forest/s1Tex", 32, 64, Grayscale8bpp },
        { "custom/medallion_magic/arrows/forest/s2Tex", 32, 64, Grayscale8bpp },
        { "custom/medallion_magic/arrows/spirit/s1Tex", 32, 64, Grayscale8bpp },
        { "custom/medallion_magic/arrows/spirit/s2Tex", 32, 64, Grayscale8bpp },
        { "custom/medallion_magic/spells/fire/s1Tex", 64, 64, Grayscale4bpp },
        { "custom/medallion_magic/spells/fire/s2Tex", 64, 64, Grayscale4bpp },
        { "custom/medallion_magic/spells/water/sTex", 64, 64, Grayscale8bpp },
        { "custom/medallion_magic/spells/forest/sTex", 64, 64, Grayscale8bpp },
        { "custom/medallion_magic/spells/forest/dust1Tex", 32, 32, Grayscale8bpp },
        { "custom/medallion_magic/spells/forest/dust2Tex", 32, 32, Grayscale8bpp },
        { "custom/medallion_magic/spells/forest/dust3Tex", 32, 32, Grayscale8bpp },
        { "custom/medallion_magic/spells/forest/dust4Tex", 32, 32, Grayscale8bpp },
        { "custom/medallion_magic/spells/forest/dust5Tex", 32, 32, Grayscale8bpp },
        { "custom/medallion_magic/spells/forest/dust6Tex", 32, 32, Grayscale8bpp },
        { "custom/medallion_magic/spells/forest/dust7Tex", 32, 32, Grayscale8bpp },
        { "custom/medallion_magic/spells/forest/dust8Tex", 32, 32, Grayscale8bpp },
    };
    for (const auto& profile : profiles) {
        if (path == profile.path) {
            return &profile;
        }
    }
    return nullptr;
}

bool HasReadableHeader(const Ship::File& file, size_t headerSize) {
    const auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file.Reader);
    if (reader == nullptr || file.Buffer == nullptr) {
        return false;
    }
    const size_t offset = reader->GetBaseAddress();
    if (offset > file.Buffer->size() || headerSize > file.Buffer->size() - offset ||
        offset > static_cast<size_t>(std::numeric_limits<int32_t>::max()) - headerSize) {
        return false;
    }
    // Native Texture stores 16-bit dimensions. Never let a malformed 32-bit
    // header truncate into a seemingly valid small normalization candidate.
    reader->Seek(static_cast<int32_t>(offset + 4), Ship::SeekOffsetType::Start);
    const uint32_t width = reader->ReadUInt32();
    const uint32_t height = reader->ReadUInt32();
    bool validScales = true;
    if (headerSize == 28) {
        reader->ReadUInt32(); // flags
        const float horizontal = std::bit_cast<float>(reader->ReadUInt32());
        const float vertical = std::bit_cast<float>(reader->ReadUInt32());
        // BinaryReader::ReadFloat throws on NaN; reject it before delegation.
        validScales = !std::isnan(horizontal) && !std::isnan(vertical);
    }
    reader->Seek(static_cast<int32_t>(offset), Ship::SeekOffsetType::Start);
    return width <= std::numeric_limits<uint16_t>::max() && height <= std::numeric_limits<uint16_t>::max() &&
           validScales;
}

bool IsUpscale(const NativeTextureProfile& native, const Fast::Texture& texture, bool integerScale) {
    if (texture.Width <= native.width || texture.Height <= native.height || texture.Width > 4096 ||
        texture.Height > 4096 ||
        static_cast<uint64_t>(texture.Width) * native.height != static_cast<uint64_t>(texture.Height) * native.width) {
        return false;
    }
    return !integerScale || (texture.Width % native.width == 0 && texture.Height % native.height == 0);
}

bool HasCompletePayload(const Ship::File& file, const Fast::Texture& texture, size_t bytes) {
    const size_t offset = reinterpret_cast<const char*>(texture.ImageData) - file.Buffer->data();
    return texture.ImageDataSize == bytes && offset <= file.Buffer->size() && bytes <= file.Buffer->size() - offset;
}

void UseWholeImage(Fast::Texture& texture) {
    // These allowlisted effect textures are sampled as whole images.
    // IMG uploads the physical RGBA dimensions directly. Unit scales retain
    // native byte accounting and avoid the HD UV clamp to a smaller
    // scrolling tile. RAW also makes resource previews decode RGBA correctly.
    texture.Flags = TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG;
    texture.HByteScale = 1.0f;
    texture.VPixelScale = 1.0f;
}

} // namespace

std::shared_ptr<Ship::IResource>
SpinEffectTextureFactoryV0::ReadResource(std::shared_ptr<Ship::File> file,
                                         std::shared_ptr<Ship::ResourceInitData> initData) {
    if (file == nullptr || initData == nullptr || !FileHasValidFormatAndReader(file, initData) ||
        !HasReadableHeader(*file, 16)) {
        return nullptr;
    }
    Fast::ResourceFactoryBinaryTextureV0 nativeFactory;
    auto resource = nativeFactory.ReadResource(file, initData);
    if (resource == nullptr) {
        return nullptr;
    }

    auto texture = std::static_pointer_cast<Fast::Texture>(resource);
    // The native parser trusts the declared payload length. Validate its file
    // bounds even when preserving native sizes, explicit IMG or other formats.
    if (!HasCompletePayload(*file, *texture, texture->ImageDataSize)) {
        SPDLOG_WARN("Ignoring truncated texture payload: {}", initData->Path);
        return nullptr;
    }
    const auto* native = GetNativeProfile(initData->Path);
    if (native == nullptr || texture->Type != native->type || !IsUpscale(*native, *texture, true)) {
        return resource;
    }
    const bool fourBit = native->type == Fast::TextureType::Grayscale4bpp;
    const size_t pixelCount = static_cast<size_t>(texture->Width) * texture->Height;
    if (!HasCompletePayload(*file, *texture, fourBit ? pixelCount / 2 : pixelCount)) {
        SPDLOG_WARN("Ignoring incomplete high-resolution effect texture: {}", initData->Path);
        return nullptr;
    }
    auto rgba = std::make_shared<std::vector<char>>(pixelCount * 4);
    for (size_t i = 0; i < pixelCount; ++i) {
        // Both I4 and I8 have intensity alpha. I4 packs the first pixel in
        // the high nibble; expand 0..15 to 0..255 for all four RGBA channels.
        const char intensity = fourBit ? static_cast<char>(((texture->ImageData[i / 2] >> (i % 2 ? 0 : 4)) & 15) * 17)
                                       : static_cast<char>(texture->ImageData[i]);
        for (size_t channel = 0; channel < 4; ++channel) {
            (*rgba)[4 * i + channel] = intensity;
        }
    }

    UseWholeImage(*texture);
    texture->ImageDataSize = static_cast<uint32_t>(rgba->size());
    texture->mImageBuffer = std::move(rgba);
    texture->ImageData = reinterpret_cast<uint8_t*>(texture->mImageBuffer->data());
    return resource;
}

std::shared_ptr<Ship::IResource>
SpinEffectTextureFactoryV1::ReadResource(std::shared_ptr<Ship::File> file,
                                         std::shared_ptr<Ship::ResourceInitData> initData) {
    if (file == nullptr || initData == nullptr || !FileHasValidFormatAndReader(file, initData) ||
        !HasReadableHeader(*file, 28)) {
        return nullptr;
    }
    Fast::ResourceFactoryBinaryTextureV1 nativeFactory;
    auto resource = nativeFactory.ReadResource(file, initData);
    if (resource == nullptr) {
        return nullptr;
    }

    auto texture = std::static_pointer_cast<Fast::Texture>(resource);
    // The native parser trusts the declared payload length. Validate its file
    // bounds even when preserving native sizes, explicit IMG or other formats.
    if (!HasCompletePayload(*file, *texture, texture->ImageDataSize)) {
        SPDLOG_WARN("Ignoring truncated texture payload: {}", initData->Path);
        return nullptr;
    }
    const auto* native = GetNativeProfile(initData->Path);
    // RAW contains physical RGBA even when Type names the logical I4/I8
    // material. Some authored archives instead declare RGBA32. Its row-byte
    // metadata still describes the native material: 8x for I4, 4x for I8.
    if (native == nullptr || !IsUpscale(*native, *texture, native->legacySpin) ||
        (texture->Type != native->type && (native->legacySpin || texture->Type != Fast::TextureType::RGBA32bpp)) ||
        texture->Flags != TEX_FLAG_LOAD_AS_RAW) {
        return resource;
    }
    const float rgbaBytesPerNativeByte = native->type == Fast::TextureType::Grayscale4bpp ? 8.0f : 4.0f;
    if (texture->HByteScale != rgbaBytesPerNativeByte * texture->Width / native->width ||
        texture->VPixelScale != static_cast<float>(texture->Height) / native->height) {
        return resource;
    }
    const size_t bytes = static_cast<size_t>(texture->Width) * texture->Height * 4;
    if (!HasCompletePayload(*file, *texture, bytes)) {
        SPDLOG_WARN("Ignoring incomplete high-resolution effect texture: {}", initData->Path);
        return nullptr;
    }
    // Keep authored RGBA and its owning buffer byte-for-byte intact.
    UseWholeImage(*texture);
    return resource;
}

} // namespace SOH
