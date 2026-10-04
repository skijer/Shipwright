#include "mods/transformation_masks/assets/mm_strict_texture_binding.h"
#include "tests/test_require.h"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <fast/interpreter.h>
#include <fast/resource/factory/TextureFactory.h>
#include <fast/resource/ResourceType.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace Fast {
extern void GfxSetInstance(std::shared_ptr<Interpreter>);
extern bool gfx_set_timg_otr_filepath_handler_custom(F3DGfx**);
} // namespace Fast
using namespace Fast;

static std::shared_ptr<Fast::Texture> MakeTexture(unsigned flags, unsigned char value) {
    auto texture = std::make_shared<Fast::Texture>(std::make_shared<Ship::ResourceInitData>());
    texture->Type = Fast::TextureType::GrayscaleAlpha8bpp;
    texture->Width = 4;
    texture->Height = 8;
    texture->Flags = flags;
    texture->HByteScale = flags ? 8 : 1;
    texture->VPixelScale = flags ? 2 : 1;
    texture->ImageDataSize = flags ? 128 : 32;
    texture->mImageBuffer = std::make_shared<std::vector<char>>(texture->ImageDataSize, value);
    texture->ImageData = reinterpret_cast<uint8_t*>(texture->mImageBuffer->data());
    return texture;
}

int main(int argc, char** argv) {
    REQUIRE(argc == 2);
    auto context = Ship::Context::CreateUninitializedInstance("MM texture test", "mmtex", "/tmp/mm-texture-test.json");
    REQUIRE(context->InitLogging());
    REQUIRE(context->InitConfiguration());
    REQUIRE(context->InitConsoleVariables());
    REQUIRE(context->InitResourceManager({ argv[1] }, {}, 1));
    auto manager = context->GetResourceManager();
    REQUIRE(manager->GetResourceLoader()->RegisterResourceFactory(
        std::make_shared<Fast::ResourceFactoryBinaryTextureV0>(), RESOURCE_FORMAT_BINARY, "Texture",
        static_cast<uint32_t>(Fast::ResourceType::Texture), 0));
    unsigned donorTextures = 0;
    auto archive = manager->GetArchiveManager()->GetArchives()->at(0);
    auto files = archive->ListFiles();
    for (const auto& entry : *files) {
        if (entry.second.rfind("objects/object_stk/", 0) != 0)
            continue;
        // The supplied archive includes this zero-byte, non-texture table marker.
        if (entry.second == "objects/object_stk/gSkullKidSkelLimbs")
            continue;
        auto file = archive->LoadFile(entry.second);
        if (!file || !file->Buffer)
            fprintf(stderr, "Cannot read archive entry: %s\n", entry.second.c_str());
        REQUIRE(file && file->Buffer);
        const auto& bytes = *file->Buffer;
        if (bytes.size() < 64)
            continue;
        if (bytes[4] != 'X' || bytes[5] != 'E' || bytes[6] != 'T' || bytes[7] != 'O')
            continue;
        auto resource = manager->LoadResourceProcess(entry.second, true);
        auto snapshot = MmStrictTextureBindings::Snapshot(resource, entry.second);
        REQUIRE(snapshot && snapshot->GetRawPointer() != resource->GetRawPointer());
        REQUIRE(memcmp(snapshot->GetRawPointer(), resource->GetRawPointer(), snapshot->ImageDataSize) == 0);
        ++donorTextures;
    }
    REQUIRE(donorTextures > 0);
    printf("PASS: %u actual Skull Kid donor texture snapshots through real factory\n", donorTextures);
    auto interpreter = std::make_shared<Interpreter>();
    GfxSetInstance(interpreter);
    MmStrictTextureBindings bindings;
    const std::string canonical = "objects/object_stk/testTex";
    auto donor = MakeTexture(0, 11);
    auto base = MakeTexture(TEX_FLAG_LOAD_AS_RAW, 22);
    auto alt = MakeTexture(TEX_FLAG_LOAD_AS_IMG, 33);
    manager->CacheExternalResource(canonical, base);
    manager->CacheExternalResource("alt/" + canonical, alt);
    const char* alias = bindings.Bind(*manager, canonical, donor);
    REQUIRE(alias);
    const std::string stableAlias = alias;
    F3DGfx command{};
    command.words.w0 = 0x25100003;
    command.words.w1 = reinterpret_cast<uintptr_t>(alias);
    for (bool useAlt : { false, true, false }) {
        manager->SetAltAssetsEnabled(useAlt);
        F3DGfx* cursor = &command;
        REQUIRE(!gfx_set_timg_otr_filepath_handler_custom(&cursor));
        auto& loaded = interpreter->mRdp->texture_to_load;
        REQUIRE(loaded.addr[0] == (useAlt ? 33 : 22));
        REQUIRE(loaded.tex_flags == (useAlt ? TEX_FLAG_LOAD_AS_IMG : TEX_FLAG_LOAD_AS_RAW));
        REQUIRE(loaded.raw_tex_metadata.width == 4 && loaded.raw_tex_metadata.height == 8);
        REQUIRE(loaded.raw_tex_metadata.h_byte_scale == 8 && loaded.raw_tex_metadata.v_pixel_scale == 2);
        REQUIRE(loaded.raw_tex_metadata.type == Fast::TextureType::GrayscaleAlpha8bpp);
        REQUIRE(loaded.raw_tex_metadata.resource->GetInitData()->Path == canonical);
        REQUIRE(command.words.w1 == reinterpret_cast<uintptr_t>(alias));
    }
    base->ImageData[0] = 99;
    alt->Dirty();
    manager->UnloadResource(canonical);
    manager->UnloadResource("alt/" + canonical);
    manager->UnloadResource(stableAlias);
    manager->UnloadResource("alt/" + stableAlias);
    bindings.EnsurePublished(*manager);
    REQUIRE(manager->GetCachedResource(stableAlias, true)->GetRawPointer() != base->ImageData);
    REQUIRE(static_cast<uint8_t*>(manager->GetCachedResource(stableAlias, true)->GetRawPointer())[0] == 22);
    REQUIRE(bindings.Bind(*manager, canonical, nullptr) == alias);
    manager->GetCachedResource(stableAlias, true)->Dirty();
    manager->GetCachedResource("alt/" + stableAlias, true)->Dirty();
    bindings.EnsurePublished(*manager);
    REQUIRE(manager->GetCachedResource(stableAlias, true));
    REQUIRE(manager->GetCachedResource("alt/" + stableAlias, true));
    for (int i = 0; i < 100; ++i)
        REQUIRE(bindings.Bind(*manager, "private/" + std::to_string(i), donor));
    REQUIRE(stableAlias == alias);

    // Names outside Skull Kid's namespace cannot acquire a colliding global texture.
    manager->CacheExternalResource("objects/object_other/testTex", base);
    const char* other = bindings.Bind(*manager, "objects/object_other/testTex", donor);
    REQUIRE(static_cast<uint8_t*>(manager->GetCachedResource(other, true)->GetRawPointer())[0] == 11);
    REQUIRE(!bindings.Bind(*manager, "objects/object_stk/missing", nullptr));
    auto malformed = MakeTexture(0, 0);
    malformed->ImageDataSize = 33;
    REQUIRE(!MmStrictTextureBindings::Snapshot(malformed, canonical));
    malformed = MakeTexture(TEX_FLAG_LOAD_AS_IMG, 0);
    malformed->ImageDataSize = 32;
    REQUIRE(!MmStrictTextureBindings::Snapshot(malformed, canonical));
    malformed = MakeTexture(0, 0);
    malformed->HByteScale = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(!MmStrictTextureBindings::Snapshot(malformed, canonical));
    malformed->HByteScale = 1;
    malformed->Flags = 4;
    REQUIRE(!MmStrictTextureBindings::Snapshot(malformed, canonical));
    malformed->Flags = 0;
    malformed->Type = Fast::TextureType::Error;
    manager->CacheExternalResource("objects/object_stk/bad", malformed);
    const char* fallback = bindings.Bind(*manager, "objects/object_stk/bad", donor);
    REQUIRE(fallback && static_cast<uint8_t*>(manager->GetCachedResource(fallback, true)->GetRawPointer())[0] == 11);
    interpreter.reset();
    puts(
        "PASS: real resource manager and filepath interpreter metadata, Alt toggles, immutable snapshots and eviction");
    fflush(nullptr);
    // The headless Context has no Window; its normal application destructor
    // assumes one. All tested resource/cache lifecycles above are explicit.
    std::_Exit(0);
}
