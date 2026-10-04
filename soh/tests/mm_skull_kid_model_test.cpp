/* Exact production graph functions are inserted by mm_skull_kid_verify.py. */
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/utils/binarytools/MemoryStream.h>
#include <fast/resource/factory/DisplayListFactory.h>
#include <fast/resource/factory/TextureFactory.h>
#include <fast/resource/factory/VertexFactory.h>
#include <fast/resource/type/DisplayList.h>
#include <fast/resource/type/Texture.h>
#include <fast/resource/type/Vertex.h>
#include <fast/resource/ResourceType.h>
#include "soh/OTRGlobals.h"
#include "soh/resource/importer/ArrayFactory.h"
#include "soh/resource/type/Array.h"
#include "soh/resource/type/SohResourceType.h"
#include "mods/transformation_masks/assets/mm_asset_loader.h"
#include "mods/transformation_masks/assets/mm_display_list_patch.h"
#include "mods/transformation_masks/assets/mm_strict_texture_binding.h"
extern "C" {
#include "src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.h"
}
#include "tests/test_require.h"

OTRGlobals* OTRGlobals::Instance = nullptr;
OTRGlobals::OTRGlobals() {
}
OTRGlobals::~OTRGlobals() {
}
static std::shared_ptr<Ship::Archive> sMmArchive;
static std::unordered_map<std::string, std::shared_ptr<Ship::IResource>> sMmResourceCache;
#define MMASSETS_LOG(...) ((void)0)
/* PRODUCTION_GRAPH_FUNCTIONS */

static void CheckSet(const MmSkullKidDisplayLists* set, unsigned value,
                     const std::shared_ptr<Ship::ResourceManager>& manager) {
    REQUIRE(set && set->head && set->eyes && set->mask);
    unsigned roots = 0;
    for (unsigned limb = 0; limb < 22; ++limb) {
        if (limb == 0 || limb == 1 || limb == 17)
            REQUIRE(set->limbs[limb] == nullptr);
        else {
            REQUIRE(set->limbs[limb]);
            ++roots;
        }
    }
    REQUIRE(roots == 19);
    std::vector<Gfx*> lists{ set->head, set->eyes, set->mask };
    for (auto* limb : set->limbs)
        if (limb)
            lists.push_back(limb);
    for (auto* root : lists) {
        REQUIRE(root[0].words.w0 == 0xDE000000);
        auto* nested = reinterpret_cast<Gfx*>(root[0].words.w1);
        REQUIRE(nested && (nested[0].words.w0 >> 24) == 0x25);
        REQUIRE((nested[2].words.w0 >> 24) == 0x01);
        auto* vertex = reinterpret_cast<Vtx*>(nested[2].words.w1);
        REQUIRE(vertex && vertex->v.ob[0] == int(value + 1)); // Offset is 16 bytes, not 16 vertices.
        const char* alias = reinterpret_cast<const char*>(nested[0].words.w1);
        auto texture = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(alias));
        REQUIRE(texture && texture->ImageData[0] == value);
        REQUIRE(texture->Width == 8 && texture->Height == 8);
        REQUIRE(texture->Flags == TEX_FLAG_LOAD_AS_RAW);
        REQUIRE(texture->HByteScale == 8 && texture->VPixelScale == 4);
    }
}

static void CheckAnjuUmbrella(const std::shared_ptr<Ship::ResourceManager>& manager) {
    const char* path = "objects/object_an2/gAnju2UmbrellaDL";
    Gfx* umbrella = MmAssets_LoadDisplayListGraphStrict(path);
    REQUIRE(umbrella);
    auto* graph = MmAssets_GetDisplayListGraph(sMmArchive);
    const auto& list = *graph->displayLists.at(path);
    std::vector<std::pair<std::string, std::vector<uint8_t>>> textures;
    std::vector<std::pair<const Vtx*, Vtx>> vertices;
    for (size_t i = 0; i < list.size(); ++i) {
        const auto& command = list[i];
        auto opcode = command.words.w0 >> 24;
        if (opcode == 0x33) {
            ++i;
        } else if (opcode == 0x01) {
            auto* vertex = reinterpret_cast<const Vtx*>(command.words.w1);
            REQUIRE(vertex);
            vertices.emplace_back(vertex, *vertex);
        } else if (opcode == 0x25) {
            const char* alias = reinterpret_cast<const char*>(command.words.w1);
            auto texture = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(alias));
            REQUIRE(texture && texture->GetInitData()->Parent == sMmArchive);
            textures.emplace_back(
                alias, std::vector<uint8_t>(texture->ImageData, texture->ImageData + texture->ImageDataSize));
        } else {
            REQUIRE(opcode != 0x20 && opcode != 0x31 && opcode != 0x32 && opcode != 0xDA);
        }
    }
    REQUIRE(vertices.size() == 3 && textures.size() == 3);
    for (unsigned scene = 0; scene < 4; ++scene) {
        manager->SetAltAssetsEnabled(scene % 2);
        sMmResourceCache.clear();
        manager->UnloadResources("*");
        for (const auto& item : textures)
            manager->UnloadResource(item.first);
        MmAssets_EnsureStrictTextureBindings();
        REQUIRE(MmAssets_LoadDisplayListGraphStrict(path) == umbrella);
        for (const auto& vertex : vertices)
            REQUIRE(memcmp(vertex.first, &vertex.second, sizeof(Vtx)) == 0);
        for (const auto& item : textures) {
            auto texture = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(item.first));
            REQUIRE(texture && texture->ImageDataSize == item.second.size());
            REQUIRE(memcmp(texture->ImageData, item.second.data(), item.second.size()) == 0);
        }
    }
    puts("PASS Anju umbrella: native graph, vertex offsets and texture ownership across 4 Alt/cache-reentry cycles");
}

static void CheckActualPack(const std::shared_ptr<Ship::ResourceManager>& manager, int argc, char** argv) {
    CheckAnjuUmbrella(manager);
    manager->SetAltAssetsEnabled(false);
    const auto* native = MmAssets_GetSkullKidDisplayLists();
    REQUIRE(native && native->head && native->eyes && native->mask);
    auto archive = manager->GetArchiveManager()->AddArchive(argv[3]);
    REQUIRE(archive);
    REQUIRE(MmAssets_GetSkullKidDisplayLists() == native);
    manager->SetAltAssetsEnabled(true);
    const auto* model = MmAssets_GetSkullKidDisplayLists();
    REQUIRE(model && model != native && model->head && model->eyes && model->mask);
    unsigned rootCount = 3;
    for (unsigned limb = 0; limb < 22; ++limb) {
        if (limb == 0 || limb == 1 || limb == 17)
            REQUIRE(!model->limbs[limb]);
        else {
            REQUIRE(model->limbs[limb]);
            ++rootCount;
        }
    }
    REQUIRE(rootCount == 22);
    auto* graph = MmAssets_GetDisplayListGraph(archive);
    std::vector<std::weak_ptr<Ship::IResource>> owners;
    std::vector<std::pair<uintptr_t, size_t>> vertexRanges;
    for (const auto& item : graph->resources) {
        REQUIRE(item.second->GetInitData()->Parent == archive);
        REQUIRE(item.second->GetInitData()->Path.rfind("objects/object_stk_3ds/v1/", 0) == 0);
        owners.push_back(item.second);
        if (std::dynamic_pointer_cast<SOH::Array>(item.second))
            vertexRanges.emplace_back(reinterpret_cast<uintptr_t>(item.second->GetRawPointer()),
                                      item.second->GetPointerSize());
    }
    REQUIRE(!vertexRanges.empty());
    std::vector<std::string> aliases;
    unsigned vertexCommands = 0, triangles = 0;
    for (const auto& item : graph->displayLists)
        for (size_t index = 0; index < item.second->size(); ++index) {
            const Gfx& command = item.second->at(index);
            const auto opcode = command.words.w0 >> 24;
            if (opcode == 0x33) {
                ++index;
                continue;
            } // Marker payload is a hash, not an opcode.
            REQUIRE(opcode != 0x20 && opcode != 0x31 && opcode != 0x32);
            if (opcode == 0x01) {
                const uintptr_t pointer = command.words.w1;
                const size_t bytes = ((command.words.w0 >> 12) & 0xff) * sizeof(Vtx);
                bool owned = false;
                for (const auto& range : vertexRanges)
                    owned |= pointer >= range.first && pointer - range.first <= range.second &&
                             bytes <= range.second - (pointer - range.first);
                REQUIRE(owned);
                ++vertexCommands;
            } else if (opcode == 0x25) {
                const char* alias = reinterpret_cast<const char*>(command.words.w1);
                auto texture = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(alias));
                REQUIRE(texture && texture->Type == Fast::TextureType::RGBA32bpp &&
                        texture->Flags == TEX_FLAG_LOAD_AS_RAW);
                REQUIRE(texture->GetInitData()->Parent == archive);
                const std::string& path = texture->GetInitData()->Path;
                auto source = std::dynamic_pointer_cast<Fast::Texture>(graph->resources.at(path));
                REQUIRE(source && source->ImageDataSize == texture->ImageDataSize);
                REQUIRE(source->ImageData != texture->ImageData);
                REQUIRE(memcmp(source->ImageData, texture->ImageData, texture->ImageDataSize) == 0);
                REQUIRE(source->Width == texture->Width && source->Height == texture->Height);
                REQUIRE(source->HByteScale == texture->HByteScale && source->VPixelScale == texture->VPixelScale);
                aliases.emplace_back(alias);
            } else if (opcode == 0x05)
                ++triangles;
        }
    REQUIRE(vertexCommands && triangles && !aliases.empty());
    for (int argument = 4; argument < argc; ++argument) {
        auto broken = manager->GetArchiveManager()->AddArchive(argv[argument]);
        REQUIRE(broken && MmAssets_GetSkullKidDisplayLists() == native);
        manager->GetArchiveManager()->RemoveArchive(broken);
        REQUIRE(MmAssets_GetSkullKidDisplayLists() == model);
    }
    for (unsigned scene = 0; scene < 8; ++scene) {
        sMmResourceCache.clear();
        manager->UnloadResources("*");
        for (const auto& alias : aliases) {
            manager->UnloadResource(alias);
            manager->UnloadResource("alt/" + alias);
        }
        manager->SetAltAssetsEnabled(scene % 2 != 0);
        REQUIRE(MmAssets_GetSkullKidDisplayLists() == (scene % 2 ? model : native));
        MmAssets_EnsureStrictTextureBindings();
        for (const auto& owner : owners)
            REQUIRE(!owner.expired());
        for (const auto& alias : aliases)
            REQUIRE(manager->LoadResourceProcess(alias));
    }
    printf("PASS real mm.o2r + 3DS pack: %u roots, %u triangles, %u owned vertex loads, %zu texture commands, "
           "corrupt-pack fallback and 8 Alt/cache-reentry cycles\n",
           rootCount, triangles, vertexCommands, aliases.size());
}

static void CheckAnjuPack(const std::shared_ptr<Ship::ResourceManager>& manager, int argc, char** argv) {
    manager->SetAltAssetsEnabled(false);
    const auto* native = MmAssets_GetAnjuDisplayLists(0);
    REQUIRE(native && !native->custom && native->umbrella);
    auto archive = manager->GetArchiveManager()->AddArchive(argv[3]);
    REQUIRE(archive && MmAssets_GetAnjuDisplayLists(0) == native);
    manager->SetAltAssetsEnabled(true);
    const auto* custom = MmAssets_GetAnjuDisplayLists(0);
    REQUIRE(custom && custom != native && custom->custom && custom->umbrella);
    REQUIRE(!custom->limbs[0] && !custom->limbs[1]);
    for (unsigned i = 2; i <= 20; ++i)
        REQUIRE(custom->limbs[i] && native->limbs[i]);
    REQUIRE(custom->heads[0] == custom->limbs[9]);
    REQUIRE(custom->heads[1] != custom->heads[0] && custom->heads[2] != custom->heads[1]);
    REQUIRE(native->heads[0] == native->heads[1] && native->heads[1] == native->heads[2]);
    auto* graph = MmAssets_GetDisplayListGraph(archive, true);
    REQUIRE(graph->optionalAnju && !graph->optionalSkullKid);
    std::vector<std::weak_ptr<Ship::IResource>> owners;
    std::vector<std::pair<std::string, std::vector<uint8_t>>> pixels;
    std::vector<std::pair<const Vtx*, Vtx>> vertices;
    for (const auto& [path, resource] : graph->resources) {
        REQUIRE(path.rfind("objects/object_anju_hd/v1/", 0) == 0 && resource->GetInitData()->Parent == archive);
        owners.push_back(resource);
    }
    for (const auto& [path, list] : graph->displayLists) {
        for (size_t i = 0; i < list->size(); ++i) {
            const auto& cmd = list->at(i);
            const unsigned op = cmd.words.w0 >> 24;
            if (op == 0x33) {
                ++i;
                continue;
            }
            REQUIRE(op != 0x20 && op != 0x31 && op != 0x32);
            if (op == 0xDA)
                REQUIRE((cmd.words.w1 & 0xFFFFFE) < 19 * 64);
            if (op == 0x01) {
                auto* vertex = reinterpret_cast<const Vtx*>(cmd.words.w1);
                REQUIRE(vertex);
                vertices.emplace_back(vertex, *vertex);
            } else if (op == 0x25) {
                const char* alias = reinterpret_cast<const char*>(cmd.words.w1);
                auto texture = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(alias));
                REQUIRE(texture && texture->GetInitData()->Parent == archive && texture->Flags == TEX_FLAG_LOAD_AS_RAW);
                if (std::none_of(pixels.begin(), pixels.end(), [&](const auto& item) { return item.first == alias; }))
                    pixels.emplace_back(
                        alias, std::vector<uint8_t>(texture->ImageData, texture->ImageData + texture->ImageDataSize));
            }
        }
    }
    REQUIRE(!pixels.empty() && !vertices.empty());
    for (int i = 4; i < argc; ++i) {
        auto broken = manager->GetArchiveManager()->AddArchive(argv[i]);
        REQUIRE(broken && MmAssets_GetAnjuDisplayLists(0) == native);
        manager->GetArchiveManager()->RemoveArchive(broken);
        REQUIRE(MmAssets_GetAnjuDisplayLists(0) == custom);
    }
    for (unsigned scene = 0; scene < 8; ++scene) {
        manager->UnloadResources("*");
        sMmResourceCache.clear();
        for (const auto& [alias, data] : pixels)
            manager->UnloadResource(alias);
        manager->SetAltAssetsEnabled(scene % 2);
        REQUIRE(MmAssets_GetAnjuDisplayLists(0) == (scene % 2 ? custom : native));
        MmAssets_EnsureStrictTextureBindings();
        for (const auto& owner : owners)
            REQUIRE(!owner.expired());
        for (const auto& [pointer, original] : vertices)
            REQUIRE(memcmp(pointer, &original, sizeof(Vtx)) == 0);
        for (const auto& [alias, data] : pixels) {
            auto texture = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(alias));
            REQUIRE(texture && texture->ImageDataSize == data.size());
            REQUIRE(memcmp(texture->ImageData, data.data(), data.size()) == 0);
        }
    }
    printf("PASS Anju HD archive: 19 limbs, 3 blink heads, umbrella, %zu owned vertex loads, complete fallback and 8 "
           "Alt/cache-reentry cycles\n",
           vertices.size());
}

int main(int argc, char** argv) {
    const bool anjuPack = argc >= 4 && strcmp(argv[1], "--anju") == 0;
    const bool actualPack = anjuPack || (argc >= 4 && strcmp(argv[1], "--actual") == 0);
    REQUIRE(actualPack || argc >= 5);
    auto context = Ship::Context::CreateUninitializedInstance("Skull Kid graph test", "stkgraph", "/tmp/stkgraph.json");
    REQUIRE(context->InitLogging());
    REQUIRE(context->InitConfiguration());
    REQUIRE(context->InitConsoleVariables());
    REQUIRE(context->InitResourceManager({ argv[actualPack ? 2 : 1] }, {}, 1));
    OTRGlobals globals;
    OTRGlobals::Instance = &globals;
    globals.context = context;
    auto manager = context->GetResourceManager();
    auto archiveManager = manager->GetArchiveManager();
    auto loader = manager->GetResourceLoader();
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryDisplayListV0>(),
                                            RESOURCE_FORMAT_BINARY, "DisplayList",
                                            static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV1>(),
                                            RESOURCE_FORMAT_BINARY, "Texture",
                                            static_cast<uint32_t>(Fast::ResourceType::Texture), 1));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV0>(),
                                            RESOURCE_FORMAT_BINARY, "Texture",
                                            static_cast<uint32_t>(Fast::ResourceType::Texture), 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryVertexV0>(),
                                            RESOURCE_FORMAT_BINARY, "Vertex",
                                            static_cast<uint32_t>(Fast::ResourceType::Vertex), 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryArrayV0>(),
                                            RESOURCE_FORMAT_BINARY, "Array",
                                            static_cast<uint32_t>(SOH::ResourceType::SOH_Array), 0));
    sMmArchive = archiveManager->GetArchives()->at(0);
    if (actualPack) {
        if (anjuPack)
            CheckAnjuPack(manager, argc, argv);
        else
            CheckActualPack(manager, argc, argv);
        fflush(nullptr);
        std::_Exit(0);
    }

    manager->SetAltAssetsEnabled(true);
    const auto* native = MmAssets_GetSkullKidDisplayLists();
    CheckSet(native, 41, manager);
    auto firstArchive = archiveManager->AddArchive(argv[2]);
    REQUIRE(firstArchive);
    manager->SetAltAssetsEnabled(false);
    REQUIRE(MmAssets_GetSkullKidDisplayLists() == native);
    CheckSet(native, 41, manager);
    manager->SetAltAssetsEnabled(true);
    const auto* first = MmAssets_GetSkullKidDisplayLists();
    REQUIRE(first != native);
    CheckSet(first, 81, manager);
    puts("PASS: complete optional model, all 22 roots, byte offsets and Alt gating");

    auto secondArchive = archiveManager->AddArchive(argv[3]);
    REQUIRE(secondArchive);
    const auto* second = MmAssets_GetSkullKidDisplayLists();
    REQUIRE(second != first && second != native);
    CheckSet(second, 121, manager);
    CheckSet(first, 81, manager); // Identical paths in another archive cannot replace owners or aliases.
    archiveManager->RemoveArchive(secondArchive);
    REQUIRE(MmAssets_GetSkullKidDisplayLists() == first);
    CheckSet(second, 121, manager); // An already-emitted list still owns all references after unmount.
    puts("PASS: identical paths in different archives have separate retained graphs");

    for (int argument = 4; argument < argc - 1; ++argument) {
        auto rejected = archiveManager->AddArchive(argv[argument]);
        REQUIRE(rejected);
        REQUIRE(MmAssets_GetSkullKidDisplayLists() == native);
        CheckSet(native, 41, manager); // Do not borrow missing pieces from firstArchive.
        archiveManager->RemoveArchive(rejected);
        REQUIRE(MmAssets_GetSkullKidDisplayLists() == first);
    }
    puts("PASS: missing roots/resources, foreign references, malformed data and cycles fall back atomically");

    auto poison = archiveManager->AddArchive(argv[argc - 1]);
    REQUIRE(poison);
    // A newly mounted graph must read its own resource header; global .meta cannot redirect it.
    auto fresh = archiveManager->AddArchive(argv[3]);
    REQUIRE(fresh && fresh != secondArchive);
    const auto* freshSet = MmAssets_GetSkullKidDisplayLists();
    CheckSet(freshSet, 121, manager);
    archiveManager->RemoveArchive(fresh);
    archiveManager->RemoveArchive(poison);

    for (unsigned scene = 0; scene < 8; ++scene) {
        auto* nested = reinterpret_cast<Gfx*>(first->head[0].words.w1);
        const std::string alias = reinterpret_cast<const char*>(nested[0].words.w1);
        auto texture = manager->GetCachedResource(alias, true);
        REQUIRE(texture);
        texture->Dirty();
        manager->UnloadResources("*");
        manager->UnloadResource(alias);
        manager->UnloadResource("alt/" + alias);
        sMmResourceCache.clear();
        manager->SetAltAssetsEnabled(scene % 2 != 0);
        MmAssets_EnsureStrictTextureBindings();
        REQUIRE(MmAssets_GetSkullKidDisplayLists() == (scene % 2 ? first : native));
        CheckSet(first, 81, manager);
        CheckSet(native, 41, manager);
        CheckSet(second, 121, manager);
    }
    puts("PASS: global/private cache eviction, alias dirtiness, reentry, live Alt toggles and retained graph owners");
    fflush(nullptr);
    std::_Exit(0); // Headless Context has no Window for application teardown.
}
