#include "fast/resource/factory/TextureFactory.h"
#include "fast/resource/type/Texture.h"
#include "ship/utils/binarytools/MemoryStream.h"
#ifndef SPIN_BASELINE_FACTORY
#include "../soh/resource/importer/SpinEffectTextureFactory.h"
#endif
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#define CHECK(c)                                                      \
    do {                                                              \
        if (!(c)) {                                                   \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            std::exit(1);                                             \
        }                                                             \
    } while (0)

static void Append(std::vector<char>& buffer, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        buffer.push_back(static_cast<char>(value >> shift));
}

struct Fixture {
    std::shared_ptr<Ship::File> file;
    std::shared_ptr<Ship::ResourceInitData> init;
    std::vector<uint8_t> pixels;
    uint32_t version = 0;
};

static Fixture Make(const std::string& path, uint32_t width, uint32_t height, uint32_t type = 6) {
    Fixture f;
    f.init = std::make_shared<Ship::ResourceInitData>();
    f.init->Path = path;
    f.init->Format = RESOURCE_FORMAT_BINARY;
    f.file = std::make_shared<Ship::File>();
    f.file->Buffer = std::make_shared<std::vector<char>>();
    auto& b = *f.file->Buffer;
    Append(b, type);
    Append(b, width);
    Append(b, height);
    Append(b, type == 5 ? width * height / 2 : width * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < (type == 5 ? width / 2 : width); ++x) {
            // A distinct row/column signal exposes low-resolution row-stride mistakes.
            uint8_t v = static_cast<uint8_t>((x * 17 + y * 31) & 255);
            b.push_back(static_cast<char>(v));
            f.pixels.push_back(v);
        }
    }
    auto reader = std::make_shared<Ship::BinaryReader>(std::make_shared<Ship::MemoryStream>(f.file->Buffer));
    reader->SetEndianness(Ship::Endianness::Little);
    f.file->Reader = reader;
    return f;
}

static Fixture MakeV1(const std::string& path, uint32_t width, uint32_t height, float hs, float vs,
                      uint32_t flags = TEX_FLAG_LOAD_AS_RAW, uint32_t type = 6) {
    auto f = Make(path, width, height, type);
    f.version = 1;
    auto& b = *f.file->Buffer;
    b.clear();
    f.pixels.clear();
    Append(b, type);
    Append(b, width);
    Append(b, height);
    Append(b, flags);
    Append(b, std::bit_cast<uint32_t>(hs));
    Append(b, std::bit_cast<uint32_t>(vs));
    Append(b, width * height * 4);
    for (uint32_t i = 0; i < width * height * 4; ++i) {
        const auto v = static_cast<uint8_t>((i * 31 + i / width) & 255);
        b.push_back(static_cast<char>(v));
        f.pixels.push_back(v);
    }
    auto reader = std::make_shared<Ship::BinaryReader>(std::make_shared<Ship::MemoryStream>(f.file->Buffer));
    reader->SetEndianness(Ship::Endianness::Little);
    f.file->Reader = reader;
    return f;
}

static std::shared_ptr<Fast::Texture> Load(Fixture& f) {
#ifdef SPIN_BASELINE_FACTORY
    Fast::ResourceFactoryBinaryTextureV0 factory;
    Fast::ResourceFactoryBinaryTextureV1 factoryV1;
#else
    SOH::SpinEffectTextureFactoryV0 factory;
    SOH::SpinEffectTextureFactoryV1 factoryV1;
#endif
    if (f.version == 1)
        return std::dynamic_pointer_cast<Fast::Texture>(factoryV1.ReadResource(f.file, f.init));
    return std::dynamic_pointer_cast<Fast::Texture>(factory.ReadResource(f.file, f.init));
}

static void Promoted(const std::string& path, uint32_t nw, uint32_t nh, uint32_t scale, uint32_t type = 6) {
    auto f = Make(path, nw * scale, nh * scale, type);
    auto t = Load(f);
    CHECK(t != nullptr);
    CHECK(t->Flags == (TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG));
    // Non-unit scales trigger GfxSpTri's HD clamp to the actor's 8x8 scroll
    // tile. Whole-image upload with unit scales retains the native UV domain.
    CHECK(t->HByteScale == 1.0f);
    CHECK(t->VPixelScale == 1.0f);
    CHECK(t->ImageDataSize == nw * nh * scale * scale * 4);
    CHECK(t->Type == static_cast<Fast::TextureType>(type));
    // The renderer must receive every source row, with intensity in all channels,
    // particularly alpha: forcing opaque alpha would expose the polygon as a box.
    for (size_t i = 0; i < nw * nh * scale * scale; ++i) {
        const uint8_t intensity = type == 5 ? ((f.pixels[i / 2] >> (i % 2 ? 0 : 4)) & 15) * 17 : f.pixels[i];
        for (size_t c = 0; c < 4; ++c)
            CHECK(t->ImageData[i * 4 + c] == intensity);
    }
    auto data = t->ImageData;
    f.file.reset();
    f.init.reset();
    CHECK(t->ImageData == data);
    CHECK(t->ImageData[4 * (nw * scale + 9)] == t->ImageData[4 * (nw * scale + 9) + 3]);
    std::printf("PASS promoted %s x%u; row stride, intensity alpha, buffer lifetime\n", path.c_str(), scale);
}

static void Unchanged(const std::string& path, uint32_t w, uint32_t h, uint32_t type = 6) {
    auto f = Make(path, w, h, type);
    auto t = Load(f);
    CHECK(t != nullptr);
    CHECK(t->Flags == 0);
    CHECK(t->HByteScale == 1 && t->VPixelScale == 1);
    CHECK(t->ImageDataSize == f.pixels.size());
    CHECK(t->mImageBuffer == f.file->Buffer);
    CHECK(f.pixels.empty() || std::memcmp(t->ImageData, f.pixels.data(), f.pixels.size()) == 0);
}

static void CheckV1(Fixture f, uint32_t flags, float hs, float vs) {
    auto t = Load(f);
    CHECK(t != nullptr);
    CHECK(t->Flags == flags && t->HByteScale == hs && t->VPixelScale == vs);
    CHECK(t->ImageDataSize == f.pixels.size());
    CHECK(t->mImageBuffer == f.file->Buffer);
    CHECK(f.pixels.empty() || std::memcmp(t->ImageData, f.pixels.data(), f.pixels.size()) == 0);
    auto data = t->ImageData;
    f.file.reset();
    f.init.reset();
    CHECK(t->ImageData == data);
    CHECK(f.pixels.empty() || std::memcmp(t->ImageData, f.pixels.data(), f.pixels.size()) == 0);
}

static void LegacyTests() {
    Promoted("alt/objects/gameplay_keep/gTorchFlameTex", 64, 32, 4);
    Promoted("objects/gameplay_keep/gEffUnknown1Tex", 64, 32, 16);
    Promoted("alt/objects/gameplay_keep/gFlameWall1Tex", 32, 32, 4);
    Promoted("objects/gameplay_keep/gFlameWall2Tex", 32, 32, 4);
    Unchanged("objects/gameplay_keep/gTorchFlameTex", 64, 32);
    Unchanged("alt/objects/gameplay_keep/gFlameWall1Tex", 32, 32);
    Unchanged("objects/gameplay_keep/gDecorativeFlameTex", 256, 128);
    Unchanged("other/gameplay_keep/gTorchFlameTex", 256, 128);
    Unchanged("objects/gameplay_keep/gTorchFlameTex", 128, 128);
    Unchanged("objects/gameplay_keep/gTorchFlameTex", 100, 50);
    Unchanged("objects/gameplay_keep/gTorchFlameTex", 256, 128, 1);
    std::puts("PASS native textures, unrelated paths/formats and unsupported dimensions unchanged");
    constexpr uint32_t imageFlags = TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG;
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 256, 128, 16, 4), imageFlags, 1, 1);
    CheckV1(MakeV1("alt/objects/gameplay_keep/gEffUnknown1Tex", 1024, 512, 64, 16), imageFlags, 1, 1);
    CheckV1(MakeV1("objects/gameplay_keep/gFlameWall1Tex", 128, 128, 16, 4), imageFlags, 1, 1);
    CheckV1(MakeV1("alt/objects/gameplay_keep/gFlameWall2Tex", 128, 128, 16, 4), imageFlags, 1, 1);
    std::puts("PASS V1 raw replacements use whole-image upload; authored RGBA and ownership preserved");
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 64, 32, 4, 1), TEX_FLAG_LOAD_AS_RAW, 4, 1);
    CheckV1(MakeV1("objects/gameplay_keep/gOtherTex", 256, 128, 16, 4), TEX_FLAG_LOAD_AS_RAW, 16, 4);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 256, 128, 16, 4, TEX_FLAG_LOAD_AS_RAW, 1),
            TEX_FLAG_LOAD_AS_RAW, 16, 4);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 128, 128, 8, 4), TEX_FLAG_LOAD_AS_RAW, 8, 4);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 256, 128, 8, 4), TEX_FLAG_LOAD_AS_RAW, 8, 4);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 256, 128, 16, 2), TEX_FLAG_LOAD_AS_RAW, 16, 2);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 256, 128, 1, 1, imageFlags), imageFlags, 1, 1);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 256, 128, 16, 4, 0), 0, 16, 4);
    std::puts("PASS V1 native images, unrelated paths/formats and explicit flags/scales unchanged");
#ifndef SPIN_BASELINE_FACTORY
    auto f = Make("alt/objects/gameplay_keep/gFlameWall1Tex", 128, 128);
    f.file->Buffer->pop_back(); // Header claims a full image but payload is truncated.
    CHECK(Load(f) == nullptr);
    auto v1 = MakeV1("objects/gameplay_keep/gEffUnknown1Tex", 1024, 512, 64, 16);
    v1.file->Buffer->pop_back();
    CHECK(Load(v1) == nullptr);
    std::puts("PASS truncated V0 and V1 candidates rejected");
#endif
}

// Exact independently verified native profiles. No prefix matching is intended.
struct Profile {
    const char* path;
    uint32_t width, height, type;
};
static const Profile profiles[] = {
    { "objects/gameplay_keep/gDecorativeFlameTex", 32, 64, 6 },
    { "objects/gameplay_keep/gDecorativeFlameMaskTex", 32, 128, 5 },
    { "overlays/ovl_Arrow_Fire/s1Tex", 32, 64, 6 },
    { "overlays/ovl_Arrow_Fire/s2Tex", 32, 64, 6 },
    { "overlays/ovl_Arrow_Ice/s1Tex", 32, 64, 6 },
    { "overlays/ovl_Arrow_Ice/s2Tex", 32, 64, 6 },
    { "overlays/ovl_Arrow_Light/s1Tex", 32, 64, 6 },
    { "overlays/ovl_Arrow_Light/s2Tex", 32, 64, 6 },
    { "overlays/ovl_Magic_Fire/sTex", 64, 64, 6 },
    { "overlays/ovl_Magic_Wind/sTex", 64, 64, 6 },
    { "overlays/ovl_Magic_Dark/sDiamondTex", 32, 64, 6 },
    { "custom/medallion_magic/arrows/shadow/s1Tex", 32, 64, 6 },
    { "custom/medallion_magic/arrows/shadow/s2Tex", 32, 64, 6 },
    { "custom/medallion_magic/arrows/water/s1Tex", 32, 64, 6 },
    { "custom/medallion_magic/arrows/water/s2Tex", 32, 64, 6 },
    { "custom/medallion_magic/arrows/forest/s1Tex", 32, 64, 6 },
    { "custom/medallion_magic/arrows/forest/s2Tex", 32, 64, 6 },
    { "custom/medallion_magic/arrows/spirit/s1Tex", 32, 64, 6 },
    { "custom/medallion_magic/arrows/spirit/s2Tex", 32, 64, 6 },
    { "custom/medallion_magic/spells/fire/s1Tex", 64, 64, 5 },
    { "custom/medallion_magic/spells/fire/s2Tex", 64, 64, 5 },
    { "custom/medallion_magic/spells/water/sTex", 64, 64, 6 },
    { "custom/medallion_magic/spells/forest/sTex", 64, 64, 6 },
    { "custom/medallion_magic/spells/forest/dust1Tex", 32, 32, 6 },
    { "custom/medallion_magic/spells/forest/dust2Tex", 32, 32, 6 },
    { "custom/medallion_magic/spells/forest/dust3Tex", 32, 32, 6 },
    { "custom/medallion_magic/spells/forest/dust4Tex", 32, 32, 6 },
    { "custom/medallion_magic/spells/forest/dust5Tex", 32, 32, 6 },
    { "custom/medallion_magic/spells/forest/dust6Tex", 32, 32, 6 },
    { "custom/medallion_magic/spells/forest/dust7Tex", 32, 32, 6 },
    { "custom/medallion_magic/spells/forest/dust8Tex", 32, 32, 6 },
};

static void ProfileTests() {
    constexpr uint32_t imageFlags = TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG;
    for (const auto& p : profiles) {
        Promoted(p.path, p.width, p.height, 2, p.type);
        const float byteScale = p.type == 5 ? 8 : 4;
        for (uint32_t type : { p.type, 1u }) {
            CheckV1(MakeV1("alt/" + std::string(p.path), p.width * 2, p.height * 2, byteScale * 2, 2,
                           TEX_FLAG_LOAD_AS_RAW, type),
                    imageFlags, 1, 1);
            // Explicit IMG and author-chosen scales are never rewritten.
            CheckV1(MakeV1(p.path, p.width * 2, p.height * 2, 1, 1, imageFlags, type), imageFlags, 1, 1);
            CheckV1(MakeV1(p.path, p.width * 2, p.height * 2, 3, 7, imageFlags, type), imageFlags, 3, 7);
            CheckV1(MakeV1(p.path, p.width, p.height, byteScale, 1, TEX_FLAG_LOAD_AS_RAW, type), TEX_FLAG_LOAD_AS_RAW,
                    byteScale, 1);
        }
        Unchanged(p.path, p.width, p.height, p.type);
        Unchanged("alt/" + std::string(p.path), p.width / 2, p.height / 2, p.type);
        Unchanged(p.path, p.width * 2, p.height * 2, p.type == 5 ? 6 : 5);
        Unchanged(p.path, p.width * 2, p.height * 3, p.type);
        Unchanged(std::string(p.path) + "Suffix", p.width * 2, p.height * 2, p.type);
        Unchanged("other/" + std::string(p.path), p.width * 2, p.height * 2, p.type);
        CheckV1(MakeV1(p.path, p.width * 2, p.height * 2, byteScale, 2, TEX_FLAG_LOAD_AS_RAW, p.type),
                TEX_FLAG_LOAD_AS_RAW, byteScale, 2);
        CheckV1(MakeV1(p.path, p.width * 2, p.height * 2, byteScale * 2, 3, TEX_FLAG_LOAD_AS_RAW, p.type),
                TEX_FLAG_LOAD_AS_RAW, byteScale * 2, 3);
        CheckV1(MakeV1(p.path, p.width * 2, p.height * 3, byteScale * 2, 3, TEX_FLAG_LOAD_AS_RAW, p.type),
                TEX_FLAG_LOAD_AS_RAW, byteScale * 2, 3);
        CheckV1(MakeV1(p.path, p.width * 2, p.height * 2, byteScale * 2, 2, TEX_FLAG_LOAD_AS_RAW, 9),
                TEX_FLAG_LOAD_AS_RAW, byteScale * 2, 2);
        CheckV1(MakeV1(p.path, p.width * 2, p.height * 2, byteScale * 2, 2, 0, p.type), 0, byteScale * 2, 2);
    }
    std::puts("PASS all 31 added profiles: native formats, RGBA V1, exact paths, native sizes and malformed scales");
}

static void I4Tests() {
    auto f = Make("custom/medallion_magic/spells/fire/s1Tex", 128, 128, 5);
    // Known nibble ordering, full-range endpoints and intensity alpha.
    (*f.file->Buffer)[16] = static_cast<char>(0xA3);
    (*f.file->Buffer)[17] = static_cast<char>(0x0F);
    auto t = Load(f);
    CHECK(t && t->Flags == (TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG));
    const uint8_t expected[] = { 170, 170, 170, 170, 51, 51, 51, 51, 0, 0, 0, 0, 255, 255, 255, 255 };
    CHECK(t->ImageDataSize == 128 * 128 * 4);
    CHECK(!std::memcmp(t->ImageData, expected, sizeof(expected)));
    CheckV1(MakeV1("objects/gameplay_keep/gDecorativeFlameMaskTex", 256, 1024, 64, 8, TEX_FLAG_LOAD_AS_RAW, 1),
            TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG, 1, 1);
    // I8 byte accounting on an I4 material is not conventional metadata.
    CheckV1(MakeV1("custom/medallion_magic/spells/fire/s1Tex", 256, 256, 16, 4, TEX_FLAG_LOAD_AS_RAW, 5),
            TEX_FLAG_LOAD_AS_RAW, 16, 4);
    std::puts("PASS I4 high/low nibble order, intensity alpha and packed-row metadata");
}

static void RgbaTests() {
    // Exact accepted Henriko Fire Arrow V1 headers; authored RGBA must be intact.
    constexpr uint32_t flags = TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG;
    CheckV1(MakeV1("alt/overlays/ovl_Arrow_Fire/s1Tex", 256, 512, 32, 8, TEX_FLAG_LOAD_AS_RAW, 1), flags, 1, 1);
    CheckV1(MakeV1("alt/overlays/ovl_Arrow_Fire/s2Tex", 512, 1024, 64, 16, TEX_FLAG_LOAD_AS_RAW, 1), flags, 1, 1);
    std::puts("PASS real Henriko RGBA32 metadata profiles retain exact bytes and ownership");
}

static void FractionalTests() {
    constexpr uint32_t flags = TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG;
    CheckV1(MakeV1("alt/overlays/ovl_Magic_Wind/sTex", 1254, 1254, 78.375f, 19.59375f), flags, 1, 1);
    CheckV1(MakeV1("overlays/ovl_Arrow_Light/s1Tex", 50, 100, 6.25f, 1.5625f), flags, 1, 1);
    CheckV1(MakeV1("overlays/ovl_Magic_Wind/sTex", 1254, 1255, 78.375f, 19.609375f), TEX_FLAG_LOAD_AS_RAW, 78.375f,
            19.609375f);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 100, 50, 6.25f, 1.5625f), TEX_FLAG_LOAD_AS_RAW, 6.25f,
            1.5625f);
    Unchanged("overlays/ovl_Magic_Wind/sTex", 1254, 1254); // V0 keeps integer-upscale restriction.
    std::puts("PASS Farore 1254 square and uniform fractional V1 profiles; prior spin rules unchanged");
}

static void MalformedTests() {
#ifndef SPIN_BASELINE_FACTORY
    for (const auto& p : profiles) {
        auto v0 = Make(p.path, p.width * 2, p.height * 2, p.type);
        v0.file->Buffer->pop_back();
        CHECK(Load(v0) == nullptr);
        auto native = Make(p.path, p.width, p.height, p.type);
        native.file->Buffer->pop_back();
        CHECK(Load(native) == nullptr);
        auto explicitImage =
            MakeV1(p.path, p.width * 2, p.height * 2, 1, 1, TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG, p.type);
        explicitImage.file->Buffer->pop_back();
        CHECK(Load(explicitImage) == nullptr);
        const float hs = p.type == 5 ? 16 : 8;
        for (uint32_t type : { p.type, 1u }) {
            auto v1 = MakeV1(p.path, p.width * 2, p.height * 2, hs, 2, TEX_FLAG_LOAD_AS_RAW, type);
            v1.file->Buffer->pop_back();
            CHECK(Load(v1) == nullptr);
        }
    }
    for (uint32_t version : { 0u, 1u }) {
        const size_t headerSize = version == 0 ? 16 : 28;
        for (size_t length = 0; length < headerSize; ++length) {
            auto f = version == 0 ? Make("overlays/ovl_Magic_Fire/sTex", 128, 128)
                                  : MakeV1("overlays/ovl_Magic_Fire/sTex", 128, 128, 8, 2);
            f.file->Buffer->resize(length);
            CHECK(Load(f) == nullptr);
        }
        auto f = version == 0 ? Make("overlays/ovl_Magic_Fire/sTex", 128, 128)
                              : MakeV1("overlays/ovl_Magic_Fire/sTex", 128, 128, 8, 2);
        // Native Texture stores uint16 dimensions; reject a header that would
        // silently truncate 65664 to 128 and masquerade as a supported image.
        (*f.file->Buffer)[6] = 1;
        CHECK(Load(f) == nullptr);
    }
    auto invalidScale = MakeV1("overlays/ovl_Magic_Fire/sTex", 128, 128, std::numeric_limits<float>::quiet_NaN(), 2);
    CHECK(Load(invalidScale) == nullptr);
    const float infinity = std::numeric_limits<float>::infinity();
    CheckV1(MakeV1("overlays/ovl_Magic_Fire/sTex", 128, 128, infinity, 2), TEX_FLAG_LOAD_AS_RAW, infinity, 2);
    Unchanged("custom/medallion_magic/spells/fire/s1Tex", 4098, 4098, 5);
    // Wrong dimensions, path or format must not allocate an expanded image.
    Unchanged("overlays/ovl_Magic_Wind/sTex", 0, 0);
    Unchanged("custom/medallion_magic/spells/forest/dust9Tex", 64, 64);
    std::puts("PASS all new profiles reject truncated payloads, short headers and dimension overflow safely");
#endif
}

static void ActualResource(const char* resourcePath, const char* localPath) {
    std::ifstream input(localPath, std::ios::binary);
    CHECK(input.good());
    auto buffer =
        std::make_shared<std::vector<char>>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    CHECK(buffer->size() >= 64 && (*buffer)[0] == 0); // Local OTR resource, little endian.
    uint32_t version = static_cast<uint8_t>((*buffer)[8]);
    CHECK(version <= 1 && (*buffer)[9] == 0 && (*buffer)[10] == 0 && (*buffer)[11] == 0);
    Fixture f;
    f.version = version;
    f.init = std::make_shared<Ship::ResourceInitData>();
    f.init->Path = resourcePath;
    f.init->Format = RESOURCE_FORMAT_BINARY;
    f.file = std::make_shared<Ship::File>();
    f.file->Buffer = buffer;
    f.file->Reader = std::make_shared<Ship::BinaryReader>(std::make_shared<Ship::MemoryStream>(buffer, 64));
    std::get<std::shared_ptr<Ship::BinaryReader>>(f.file->Reader)->SetEndianness(Ship::Endianness::Little);
    const size_t offset = version == 0 ? 80 : 92;
    CHECK(buffer->size() >= offset);
    const std::vector<char> authored(buffer->begin() + offset, buffer->end());
    auto t = Load(f);
    CHECK(t && t->Flags == (TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG));
    CHECK(t->HByteScale == 1 && t->VPixelScale == 1);
    if (version == 1) {
        CHECK(t->ImageDataSize == authored.size());
        CHECK(!std::memcmp(t->ImageData, authored.data(), authored.size()));
        CHECK(t->mImageBuffer == buffer);
    }
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < t->ImageDataSize; ++i) {
        hash = (hash ^ t->ImageData[i]) * 1099511628211ull;
    }
    std::printf("PASS actual %s: type=%u size=%ux%u flags=%u scales=%g,%g bytes=%u fnv1a=%016llx\n", resourcePath,
                static_cast<unsigned>(t->Type), t->Width, t->Height, t->Flags, t->HByteScale, t->VPixelScale,
                t->ImageDataSize, static_cast<unsigned long long>(hash));
}

int main(int argc, char** argv) {
    if (argc >= 4 && (argc - 1) % 3 == 0 && !std::strcmp(argv[1], "--resource")) {
        for (int i = 1; i < argc; i += 3) {
            CHECK(!std::strcmp(argv[i], "--resource"));
            ActualResource(argv[i + 1], argv[i + 2]);
        }
        return 0;
    }
    const struct {
        const char* name;
        void (*run)();
    } tests[] = {
        { "legacy", LegacyTests }, { "profiles", ProfileTests },      { "i4", I4Tests },
        { "rgba", RgbaTests },     { "fractional", FractionalTests }, { "malformed", MalformedTests },
    };
    bool ran = false;
    for (const auto& test : tests) {
        if (argc == 1 || !std::strcmp(argv[1], test.name)) {
            test.run();
            ran = true;
        }
    }
    CHECK(ran);
}
