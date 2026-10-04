#include "mm_strict_texture_binding.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <ship/resource/ResourceManager.h>

std::shared_ptr<Fast::Texture> MmStrictTextureBindings::Snapshot(const std::shared_ptr<Ship::IResource>& resource,
                                                                 const std::string& canonical) {
    auto source = std::dynamic_pointer_cast<Fast::Texture>(resource);
    if (!source || source->Type < Fast::TextureType::RGBA32bpp ||
        source->Type > Fast::TextureType::GrayscaleAlpha16bpp || !source->Width || !source->Height ||
        !source->ImageData || !source->ImageDataSize || source->ImageDataSize > 256U * 1024U * 1024U ||
        (source->Flags & ~(TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG)) || !std::isfinite(source->HByteScale) ||
        source->HByteScale <= 0 || !std::isfinite(source->VPixelScale) || source->VPixelScale <= 0)
        return nullptr;

    if (source->mImageBuffer) {
        const auto start = reinterpret_cast<uintptr_t>(source->mImageBuffer->data());
        const auto pixels = reinterpret_cast<uintptr_t>(source->ImageData);
        const size_t bytes = source->mImageBuffer->size();
        if (pixels < start || pixels - start > bytes || source->ImageDataSize > bytes - (pixels - start))
            return nullptr;
    }
    static constexpr unsigned bits[] = { 0, 32, 16, 4, 8, 4, 8, 4, 8, 16 };
    const bool palette =
        source->Type == Fast::TextureType::Palette4bpp || source->Type == Fast::TextureType::Palette8bpp;
    // Image textures and non-paletted raw replacements contain decoded RGBA,
    // even when their declared native type is (for example) RGBA16 or IA8.
    const unsigned storageBits =
        ((source->Flags & TEX_FLAG_LOAD_AS_IMG) || ((source->Flags & TEX_FLAG_LOAD_AS_RAW) && !palette))
            ? 32
            : bits[static_cast<unsigned>(source->Type)];
    const size_t required = (size_t(source->Width) * source->Height * storageBits + 7) / 8;
    if (source->ImageDataSize < required)
        return nullptr;

    auto init = source->GetInitData() ? std::make_shared<Ship::ResourceInitData>(*source->GetInitData())
                                      : std::make_shared<Ship::ResourceInitData>();
    init->Path = canonical;
    auto result = std::make_shared<Fast::Texture>(init);
    result->Type = source->Type;
    result->Width = source->Width;
    result->Height = source->Height;
    result->Flags = source->Flags;
    result->HByteScale = source->HByteScale;
    result->VPixelScale = source->VPixelScale;
    result->ImageDataSize = source->ImageDataSize;
    result->mImageBuffer = std::make_shared<std::vector<char>>(source->ImageDataSize);
    std::memcpy(result->mImageBuffer->data(), source->ImageData, source->ImageDataSize);
    result->ImageData = reinterpret_cast<uint8_t*>(result->mImageBuffer->data());
    return result;
}

const char* MmStrictTextureBindings::Bind(Ship::ResourceManager& manager, const std::string& canonical,
                                          const std::shared_ptr<Ship::IResource>& privateResource) {
    auto found = bindings.find(canonical);
    if (found != bindings.end()) {
        EnsurePublished(manager);
        return found->second->path.c_str();
    }
    auto fallback = Snapshot(privateResource, canonical);
    if (!fallback)
        return nullptr;
    auto binding = std::make_unique<Binding>();
    binding->base = fallback;
    binding->alt = fallback;
    // Only Skull Kid's known namespace opts into global texture replacements.
    // Skeletons, vertices and display lists never pass through this resolver.
    if (canonical.rfind("objects/object_stk/", 0) == 0) {
        if (auto replacement = Snapshot(manager.LoadResourceProcess(canonical, true), canonical))
            binding->base = std::move(replacement);
        binding->alt = binding->base;
        if (auto replacement = Snapshot(manager.LoadResourceProcess("alt/" + canonical, true), canonical))
            binding->alt = std::move(replacement);
    }
    static std::atomic<uint64_t> nextAlias{ 0 };
    do {
        binding->path = "__mm_private_texture/" + std::to_string(nextAlias++);
        binding->altPath = "alt/" + binding->path;
    } while (manager.GetArchiveManager()->HasFile(binding->path) ||
             manager.GetArchiveManager()->HasFile(binding->altPath) || manager.GetCachedResource(binding->path, true) ||
             manager.GetCachedResource(binding->altPath, true));
    manager.CacheExternalResource(binding->path, binding->base);
    manager.CacheExternalResource(binding->altPath, binding->alt);
    const char* path = binding->path.c_str();
    bindings.emplace(canonical, std::move(binding));
    return path;
}

void MmStrictTextureBindings::EnsurePublished(Ship::ResourceManager& manager) const {
    for (const auto& item : bindings) {
        auto& binding = *item.second;
        // Broad cache invalidation may mark even external aliases dirty. Keep
        // their immutable pixels, but publish a fresh resource with a clean flag.
        if (binding.base->IsDirty())
            binding.base = Snapshot(binding.base, item.first);
        if (binding.alt->IsDirty())
            binding.alt = Snapshot(binding.alt, item.first);
        if (manager.GetCachedResource(binding.path, true) != binding.base)
            manager.CacheExternalResource(binding.path, binding.base);
        if (manager.GetCachedResource(binding.altPath, true) != binding.alt)
            manager.CacheExternalResource(binding.altPath, binding.alt);
    }
}
