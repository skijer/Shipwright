/* Assembled with the actual production loader functions by mm_ordinary_verify.py. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/utils/binarytools/MemoryStream.h>
#include <ship/utils/StrHash64.h>
#include <fast/resource/factory/TextureFactory.h>
#include <fast/resource/ResourceType.h>
#include "soh/OTRGlobals.h"
#include "soh/resource/importer/SkeletonFactory.h"
#include "soh/resource/importer/SkeletonLimbFactory.h"
#include "soh/resource/importer/AnimationFactory.h"
#include "mods/transformation_masks/assets/mm_normal_actor_resource.h"
#include "mods/transformation_masks/assets/mm_strict_texture_binding.h"
#include "mods/transformation_masks/assets/mm_kafei_resource.h"
#include "soh/resource/importer/PlayerAnimationFactory.h"
#include "mods/transformation_masks/assets/mm_asset_loader.h"
extern "C" {
#include "src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.h"
}
#include "tests/test_require.h"

// Application startup is the boundary; all resource/archive factories and types are real.
OTRGlobals* OTRGlobals::Instance = nullptr;
OTRGlobals::OTRGlobals() {
}
OTRGlobals::~OTRGlobals() {
}
static std::shared_ptr<Ship::Archive> sMmArchive;
static std::unordered_map<std::string, std::shared_ptr<Ship::IResource>> sMmResourceCache;
#define MMASSETS_LOG(...) ((void)0)
/* PRODUCTION_RESOURCE_FUNCTIONS */

static void TestAnjuNativeHierarchy(const std::shared_ptr<Ship::ResourceManager>& manager) {
    manager->SetAltAssetsEnabled(false);
    const auto* presentation = StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_ANJU, 0);
    auto seed =
        std::dynamic_pointer_cast<SOH::Skeleton>(MmAssets_LoadResourceObjectFromMmArchive(presentation->skeletonPath));
    REQUIRE(seed && seed->limbTable.size() == 20);
    auto paths = seed->limbTable;
    std::vector<Vec3s> offsets;
    for (const auto& path : paths) {
        auto child =
            std::dynamic_pointer_cast<SOH::SkeletonLimb>(MmAssets_LoadResourceObjectFromMmArchive(path.c_str()));
        REQUIRE(child);
        offsets.push_back(child->limbData.standardLimb.jointPos);
    }
    for (unsigned cycle = 0; cycle < 4; ++cycle) {
        sMmResourceCache.clear();
        manager->UnloadResources("*");
        manager->SetAltAssetsEnabled(true);
        for (unsigned i = 0; i < paths.size(); ++i) {
            auto child = std::dynamic_pointer_cast<SOH::SkeletonLimb>(
                manager->GetResourceLoader()->LoadResource("alt/" + paths[i], sMmArchive->LoadFile(paths[i])));
            REQUIRE(child);
            child->limbData.standardLimb.jointPos.y += 476;
            manager->CacheExternalResource("alt/" + paths[i], child);
        }
        MmNormalActorResources resources{};
        REQUIRE(MmAssets_LoadNormalActor(STATIC_STORY_ACTOR_ANJU, 0, &resources));
        for (unsigned i = 0; i < paths.size(); ++i) {
            auto* child = static_cast<StandardLimb*>(resources.skeleton->sh.segment[i]);
            REQUIRE(child && std::memcmp(&child->jointPos, &offsets[i], sizeof(Vec3s)) == 0);
        }
        sMmResourceCache.clear();
        manager->UnloadResources("*");
        for (unsigned i = 0; i < paths.size(); ++i) {
            auto* child = static_cast<StandardLimb*>(resources.skeleton->sh.segment[i]);
            REQUIRE(child && std::memcmp(&child->jointPos, &offsets[i], sizeof(Vec3s)) == 0);
        }
        MmAssets_ReleaseNormalActor(resources.owner);
    }
    manager->SetAltAssetsEnabled(false);
    puts("PASS Anju native hierarchy: global alt offsets ignored, all 20 children retained through 4 cache cycles");
}

static void TestAnjuNativeMetadata(const std::shared_ptr<Ship::ResourceManager>& manager) {
    const char* pack = std::getenv("MM_ANJU_META_TEST_PACK");
    REQUIRE(pack);
    MmNormalActorResources native{};
    REQUIRE(MmAssets_LoadNormalActor(STATIC_STORY_ACTOR_ANJU, 0, &native));
    std::vector<Vec3s> offsets;
    for (unsigned i = 0; i < 20; ++i)
        offsets.push_back(static_cast<StandardLimb*>(native.skeleton->sh.segment[i])->jointPos);
    auto poison = manager->GetArchiveManager()->AddArchive(pack);
    REQUIRE(poison);
    // Ordinary O2R indexing strips .meta names. Explicitly expose them here to
    // exercise ResourceLoader's metadata capability, not a normal-pack failure.
    auto files = poison->ListFiles();
    const auto originalFiles = *files;
    for (const auto& [hash, path] : originalFiles) {
        const auto meta = path + ".meta";
        if (poison->LoadFile(meta))
            (*files)[CRC64(meta.c_str())] = meta;
    }
    auto archives =
        std::make_shared<std::vector<std::shared_ptr<Ship::Archive>>>(*manager->GetArchiveManager()->GetArchives());
    manager->GetArchiveManager()->SetArchives(archives);
    for (bool alt : { false, true }) {
        manager->SetAltAssetsEnabled(alt);
        manager->UnloadResources("*");
        sMmResourceCache.clear();
        const char* animationPath = "objects/object_an2/gAnju2UmbrellaCryAnim";
        REQUIRE(manager->LoadFileProcess(std::string(animationPath) + ".meta"));
        auto redirected =
            std::dynamic_pointer_cast<SOH::Animation>(MmAssets_LoadResourceObjectFromMmArchive(animationPath));
        REQUIRE(redirected && redirected->animationData.animationHeader.common.frameCount == 1);
        MmNormalActorResources resources{};
        REQUIRE(MmAssets_LoadNormalActor(STATIC_STORY_ACTOR_ANJU, 0, &resources));
        REQUIRE(resources.animation->common.frameCount == 43);
        for (unsigned i = 0; i < 20; ++i) {
            auto* limb = static_cast<StandardLimb*>(resources.skeleton->sh.segment[i]);
            REQUIRE(std::memcmp(&limb->jointPos, &offsets[i], sizeof(Vec3s)) == 0);
        }
        auto retained = static_cast<std::vector<MmNormalActor::Resource>*>(resources.owner);
        for (unsigned i = 1; i < retained->size(); ++i)
            REQUIRE(retained->at(i)->GetInitData()->Parent == sMmArchive);
        MmAssets_ReleaseNormalActor(resources.owner);
    }
    MmAssets_ReleaseNormalActor(native.owner);
    manager->GetArchiveManager()->RemoveArchive(poison);
    manager->UnloadResources("*");
    sMmResourceCache.clear();
    manager->SetAltAssetsEnabled(false);
    puts("PASS Anju native metadata: global redirects cannot replace hierarchy, animation or face textures");
}

static void TestSkullKidSceneReentry(const std::shared_ptr<Ship::ResourceManager>& manager) {
    for (bool alt : { false, true })
        for (unsigned pose = 0; pose < 2; ++pose) {
            manager->SetAltAssetsEnabled(alt);
            const auto* presentation = StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_SKULL_KID, pose);
            auto seed = std::dynamic_pointer_cast<SOH::Skeleton>(
                MmAssets_LoadResourceObjectFromMmArchive(presentation->skeletonPath));
            REQUIRE(seed && seed->limbTable.size() == 21);
            const auto paths = seed->limbTable;
            sMmResourceCache.clear();
            manager->UnloadResources("*");
            seed.reset();
            // Real archive limbs, selected as alternate resources without requiring
            // a particular third-party Skull Kid pack. Only the cache keys differ.
            auto loadAlternateLimbs = [&]() {
                for (const auto& path : paths) {
                    auto file = sMmArchive->LoadFile(path);
                    REQUIRE(file);
                    auto limb = manager->GetResourceLoader()->LoadResource("alt/" + path, file);
                    REQUIRE(limb);
                    manager->CacheExternalResource("alt/" + path, limb);
                }
            };
            if (alt)
                loadAlternateLimbs();
            auto skeleton = std::dynamic_pointer_cast<SOH::Skeleton>(
                MmAssets_LoadResourceObjectFromMmArchive(presentation->skeletonPath));
            auto animation = std::dynamic_pointer_cast<SOH::Animation>(
                MmAssets_LoadResourceObjectFromMmArchive(presentation->animationPath));
            REQUIRE(skeleton && animation);
            REQUIRE(MmNormalActor::ValidateAnimation(animation, 21,
                                                     animation->animationData.animationHeader.common.frameCount));
            std::vector<std::weak_ptr<Ship::IResource>> limbs;
            std::vector<std::string> displayLists;
            for (size_t i = 0; i < paths.size(); ++i) {
                auto limb = std::dynamic_pointer_cast<SOH::SkeletonLimb>(manager->LoadResourceProcess(paths[i]));
                REQUIRE(limb && limb->limbType == SOH::LimbType::Standard);
                REQUIRE(limb->GetRawPointer() == skeleton->skeletonHeaderSegments[i]);
                REQUIRE(limb->GetInitData()->Path.starts_with("alt/") == alt);
                limbs.push_back(limb);
                const auto* dl = reinterpret_cast<const char*>(limb->limbData.standardLimb.dList);
                displayLists.emplace_back(dl ? dl : "");
            }
            for (unsigned scene = 0; scene < 4; ++scene) {
                manager->UnloadResources("alt/*");
                manager->UnloadResources("*"); // Also exercise eviction of vanilla limbs.
                // Synthetic alternate keys are not in an archive's directory index.
                for (const auto& path : paths)
                    manager->UnloadResource("alt/" + path);
                for (size_t i = 0; i < limbs.size(); ++i) {
                    auto limb = std::dynamic_pointer_cast<SOH::SkeletonLimb>(limbs[i].lock());
                    REQUIRE(limb);
                    REQUIRE(limb->GetRawPointer() == skeleton->skeletonHeaderSegments[i]);
                    const auto& data = limb->limbData.standardLimb;
                    REQUIRE(data.child == 255 || data.child < limbs.size());
                    REQUIRE(data.sibling == 255 || data.sibling < limbs.size());
                    const auto* dl = reinterpret_cast<const char*>(data.dList);
                    REQUIRE(displayLists[i] == (dl ? dl : ""));
                }
                if (alt)
                    loadAlternateLimbs();
                else
                    for (const auto& path : paths)
                        REQUIRE(manager->LoadResourceProcess(path));
                REQUIRE(MmAssets_LoadResourceObjectFromMmArchive(presentation->skeletonPath) == skeleton);
                REQUIRE(MmAssets_LoadResourceObjectFromMmArchive(presentation->animationPath) == animation);
            }
            sMmResourceCache.clear();
            manager->UnloadResources("*");
            for (const auto& path : paths)
                manager->UnloadResource("alt/" + path);
            skeleton.reset();
            for (const auto& limb : limbs)
                REQUIRE(limb.expired());
        }
    puts("PASS Skull Kid: both poses, Alt off/on, 16 cache-unload/reentry cycles and final limb release");
}

static void TestSkullKidTextureReentry(const std::shared_ptr<Ship::ResourceManager>& manager) {
    const std::string canonical = "objects/object_stk/gSkullKidEyeTex";
    auto source = MmAssets_LoadResourceObjectFromMmArchive(canonical.c_str());
    REQUIRE(MmNormalActor::ValidateTexture(source));
    MmStrictTextureBindings bindings;
    const char* alias = bindings.Bind(*manager, canonical, source);
    REQUIRE(alias);
    const std::string stableAlias = alias;
    auto texture = std::dynamic_pointer_cast<Fast::Texture>(manager->GetCachedResource(stableAlias, true));
    REQUIRE(texture);
    const std::vector<uint8_t> pixels(texture->ImageData, texture->ImageData + texture->ImageDataSize);
    texture.reset();
    source.reset();
    sMmResourceCache.clear();
    for (bool alt : { false, true })
        for (unsigned scene = 0; scene < 4; ++scene) {
            manager->SetAltAssetsEnabled(alt);
            manager->UnloadResources("*");
            manager->UnloadResource(stableAlias);
            manager->UnloadResource("alt/" + stableAlias);
            bindings.EnsurePublished(*manager); // Called before each Skull Kid draw.
            auto reloaded = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(stableAlias));
            REQUIRE(reloaded && reloaded->ImageDataSize == pixels.size());
            REQUIRE(std::memcmp(reloaded->ImageData, pixels.data(), pixels.size()) == 0);
            REQUIRE(reloaded->GetInitData()->Path == canonical);
            REQUIRE(stableAlias == alias);
        }
    manager->UnloadResource(stableAlias);
    manager->UnloadResource("alt/" + stableAlias);
    puts("PASS Skull Kid: texture aliases and retained pixels survive repeated eviction with Alt off/on");
}

/* GameState_Destroy unloads alternate assets while the MM archive cache survives. The
 * cached skeleton must own its original limbs, even when the same paths are
 * subsequently loaded into new resources by the next scene. */
static void TestSceneReentry(const std::shared_ptr<Ship::ResourceManager>& manager) {
    manager->SetAltAssetsEnabled(true);
    for (auto actor : { STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, STATIC_STORY_ACTOR_LULU }) {
        sMmResourceCache.clear();
        manager->UnloadResources("*");
        const auto* presentation = StaticStoryMm_GetPresentation(actor, 0);
        auto skeleton = std::dynamic_pointer_cast<SOH::Skeleton>(
            MmAssets_LoadResourceObjectFromMmArchive(presentation->skeletonPath));
        REQUIRE(skeleton);
        std::vector<std::weak_ptr<Ship::IResource>> limbs;
        std::vector<std::string> displayLists;
        bool hasAlternateLimb = false;
        for (size_t i = 0; i < skeleton->limbTable.size(); ++i) {
            auto limb =
                std::dynamic_pointer_cast<SOH::SkeletonLimb>(manager->LoadResourceProcess(skeleton->limbTable[i]));
            REQUIRE(limb && limb->limbType == SOH::LimbType::Standard);
            REQUIRE(limb->GetRawPointer() == skeleton->skeletonHeaderSegments[i]);
            hasAlternateLimb |= limb->GetInitData()->Path.starts_with("alt/");
            limbs.push_back(limb);
            const auto* dl = reinterpret_cast<const char*>(limb->limbData.standardLimb.dList);
            displayLists.emplace_back(dl ? dl : "");
        }
        // An unmodded archive alone does not exercise the HD scene-unload bug.
        REQUIRE(hasAlternateLimb);
        for (unsigned scene = 0; scene < 4; ++scene) {
            if (actor == STATIC_STORY_ACTOR_LULU) {
                MmNormalActorResources resources{};
                const bool loaded = MmAssets_LoadNormalActor(actor, 0, &resources);
                REQUIRE(loaded && resources.owner && resources.skeleton);
                REQUIRE(resources.skeleton == skeleton->GetRawPointer());
                MmAssets_ReleaseNormalActor(resources.owner);
            } else {
                auto cached = MmAssets_LoadResourceObjectFromMmArchive(presentation->skeletonPath);
                REQUIRE(cached == skeleton);
            }
            manager->UnloadResources("alt/*");
            for (size_t i = 0; i < limbs.size(); ++i) {
                auto limb = std::dynamic_pointer_cast<SOH::SkeletonLimb>(limbs[i].lock());
                REQUIRE(limb); // Previously freed here: the next draw dereferenced this address.
                REQUIRE(limb->GetRawPointer() == skeleton->skeletonHeaderSegments[i]);
                const auto* dl = reinterpret_cast<const char*>(limb->limbData.standardLimb.dList);
                REQUIRE(displayLists[i] == (dl ? dl : ""));
                // Populate the new scene's cache with fresh objects at the same paths.
                auto reloaded = manager->LoadResourceProcess(skeleton->limbTable[i]);
                REQUIRE(reloaded);
            }
        }
        sMmResourceCache.clear();
        manager->UnloadResources("*");
        skeleton.reset();
        for (const auto& limb : limbs)
            REQUIRE(limb.expired());
        std::printf("PASS actor %d: four HD scene unload/reentry cycles, final limb release\n", actor);
    }
}

static void TestXmlSkeletonRetention(const std::shared_ptr<Ship::ResourceManager>& manager) {
    // Exercise the other skeleton factory with a real limb from the archive.
    auto source = std::dynamic_pointer_cast<SOH::Skeleton>(
        MmAssets_LoadResourceObjectFromMmArchive("objects/object_zov/gLuluSkel"));
    REQUIRE(source && !source->limbTable.empty());
    const std::string limbPath = source->limbTable.front();
    const std::string xml = "<Skeleton Version=\"0\" Type=\"Flex\" LimbType=\"Standard\" "
                            "LimbCount=\"1\" DisplayListCount=\"0\"><SkeletonLimb Path=\"" +
                            limbPath + "\"/></Skeleton>";
    auto file = std::make_shared<Ship::File>();
    file->Buffer = std::make_shared<std::vector<char>>(xml.begin(), xml.end());
    file->IsLoaded = true;
    auto skeleton = std::dynamic_pointer_cast<SOH::Skeleton>(
        manager->GetResourceLoader()->LoadResource("tests/retained_xml_skeleton", file));
    REQUIRE(skeleton && skeleton->skeletonHeaderSegments.size() == 1);
    std::weak_ptr<Ship::IResource> limb = manager->LoadResourceProcess(limbPath);
    REQUIRE(!limb.expired() && limb.lock()->GetRawPointer() == skeleton->skeletonHeaderSegments[0]);
    source.reset();
    sMmResourceCache.clear();
    manager->UnloadResources("*");
    REQUIRE(!limb.expired());
    REQUIRE(limb.lock()->GetRawPointer() == skeleton->skeletonHeaderSegments[0]);
    skeleton.reset();
    REQUIRE(limb.expired());
    puts("PASS XML skeleton: limb retained across cache unload, released with skeleton");
}

int main(int argc, char** argv) {
    REQUIRE(argc >= 2);
    auto context =
        Ship::Context::CreateUninitializedInstance("MM ordinary test", "mmtest", "/tmp/mm-ordinary-test.json");
    REQUIRE(context->InitLogging());
    REQUIRE(context->InitConfiguration());
    REQUIRE(context->InitConsoleVariables());
    REQUIRE(context->InitResourceManager(std::vector<std::string>(argv + 1, argv + argc), {}, 1));
    OTRGlobals globals;
    globals.context = context;
    OTRGlobals::Instance = &globals;
    auto manager = context->GetResourceManager();
    auto loader = manager->GetResourceLoader();
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonV0>(),
                                            RESOURCE_FORMAT_BINARY, "Skeleton", 0x4F534B4C, 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonLimbV0>(),
                                            RESOURCE_FORMAT_BINARY, "SkeletonLimb", 0x4F534C42, 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLSkeletonV0>(), RESOURCE_FORMAT_XML,
                                            "Skeleton", 0x4F534B4C, 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLSkeletonLimbV0>(),
                                            RESOURCE_FORMAT_XML, "SkeletonLimb", 0x4F534C42, 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAnimationV0>(),
                                            RESOURCE_FORMAT_BINARY, "Animation", 0x4F414E4D, 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV0>(),
                                            RESOURCE_FORMAT_BINARY, "Texture",
                                            static_cast<uint32_t>(Fast::ResourceType::Texture), 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryPlayerAnimationV0>(),
                                            RESOURCE_FORMAT_BINARY, "PlayerAnimation", 0x4F50414D, 0));
    sMmArchive = manager->GetArchiveManager()->GetArchives()->at(0);
    TestSkullKidSceneReentry(manager);
    TestSkullKidTextureReentry(manager);
    if (argc > 2)
        TestSceneReentry(manager);
    TestXmlSkeletonRetention(manager);
    TestAnjuNativeHierarchy(manager);
    TestAnjuNativeMetadata(manager);
    auto resolve = [manager](const std::string& path) { return manager->LoadResourceProcess(path); };
    unsigned loads = 0;
    for (bool alt : { false, true }) {
        manager->SetAltAssetsEnabled(alt);
        sMmResourceCache.clear();
        for (auto actor : { STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, STATIC_STORY_ACTOR_KEATON, STATIC_STORY_ACTOR_LULU,
                            STATIC_STORY_ACTOR_ANJU }) {
            unsigned count = actor == STATIC_STORY_ACTOR_ANJU ? 2 : actor == STATIC_STORY_ACTOR_LULU ? 4 : 3;
            for (unsigned pose = 0; pose < count; ++pose) {
                MmNormalActorResources resources{};
                bool loaded = MmAssets_LoadNormalActor(actor, pose, &resources);
                REQUIRE(loaded);
                auto presentation = StaticStoryMm_GetPresentation(actor, pose);
                REQUIRE(resources.owner && resources.skeleton && resources.animation);
                REQUIRE(resources.skeleton->sh.limbCount == presentation->limbCount);
                REQUIRE(resources.animation->common.frameCount == presentation->frameCount);
                auto retained = static_cast<std::vector<MmNormalActor::Resource>*>(resources.owner);
                REQUIRE(retained->size() ==
                        2U + presentation->limbCount + presentation->eyeCount + presentation->mouthCount);
                std::weak_ptr<Ship::IResource> child = retained->at(1);
                auto rawChild = resources.skeleton->sh.segment[0];
                sMmResourceCache.clear();
                manager->UnloadResource(presentation->skeletonPath);
                for (const auto& path : std::static_pointer_cast<SOH::Skeleton>(retained->at(0))->limbTable) {
                    manager->UnloadResource(path);
                    manager->UnloadResource("alt/" + path);
                }
                REQUIRE(!child.expired());
                REQUIRE(child.lock()->GetRawPointer() == rawChild);
                REQUIRE(resources.animation->frameData[0] ==
                        (s16)std::static_pointer_cast<SOH::Animation>(retained->at(1 + presentation->limbCount))
                            ->rotationValues[0]);
                MmAssets_ReleaseNormalActor(resources.owner);
                REQUIRE(child.expired());
                ++loads;
            }
        }
    }
    MmNormalActorResources disabled{};
    REQUIRE(!MmAssets_LoadNormalActor(STATIC_STORY_ACTOR_CHILD_KAFEI, 0, &disabled));
    REQUIRE(!disabled.owner && !disabled.skeleton);
    auto skeleton = MmAssets_LoadResourceObjectFromMmArchive("objects/object_zov/gLuluSkel");
    auto animation = MmAssets_LoadResourceObjectFromMmArchive("objects/object_zov/gLuluLookDownAnim");
    auto typed = std::dynamic_pointer_cast<SOH::Skeleton>(skeleton);
    auto anim = std::dynamic_pointer_cast<SOH::Animation>(animation);
    REQUIRE(typed && anim);
    std::vector<MmNormalActor::Resource> retained;
    REQUIRE(MmNormalActor::ValidateSkeleton(skeleton, 22, 21, retained));
    retained.clear();
    auto original = typed->skeletonHeaderSegments[0];
    typed->skeletonHeaderSegments[0] = typed->skeletonHeaderSegments[1];
    REQUIRE(!MmNormalActor::ValidateSkeleton(skeleton, 22, 21, retained));
    typed->skeletonHeaderSegments[0] = original;
    auto child = std::dynamic_pointer_cast<SOH::SkeletonLimb>(resolve(typed->limbTable[0]));
    child->limbType = SOH::LimbType::LOD;
    REQUIRE(!MmNormalActor::ValidateSkeleton(skeleton, 22, 21, retained));
    child->limbType = SOH::LimbType::Standard;
    auto oldSibling = child->limbData.standardLimb.sibling;
    child->limbData.standardLimb.sibling = 0;
    REQUIRE(!MmNormalActor::ValidateSkeleton(skeleton, 22, 21, retained));
    child->limbData.standardLimb.sibling = oldSibling;
    REQUIRE(!MmNormalActor::ValidateSkeleton(animation, 22, 21, retained));
    REQUIRE(!MmNormalActor::ValidateAnimation(skeleton, 22, 30));
    REQUIRE(!MmNormalActor::ValidateAnimation(animation, 22, 31));
    auto index = anim->rotationIndices[0].x;
    anim->rotationIndices[0].x = 65535;
    REQUIRE(!MmNormalActor::ValidateAnimation(animation, 22, 30));
    anim->rotationIndices[0].x = index;
    auto data = anim->animationData.animationHeader.frameData;
    anim->animationData.animationHeader.frameData = nullptr;
    REQUIRE(!MmNormalActor::ValidateAnimation(animation, 22, 30));
    anim->animationData.animationHeader.frameData = data;
    anim->type = SOH::AnimationType::Link;
    REQUIRE(!MmNormalActor::ValidateAnimation(animation, 22, 30));
    anim->type = SOH::AnimationType::Normal;
    REQUIRE(!MmNormalActor::ValidateTexture(animation));
    auto kafei = MmAssets_LoadResourceObjectFromMmArchive("objects/object_test3/gKafeiSkel");
    REQUIRE(kafei);
    REQUIRE(!MmNormalActor::ValidateSkeleton(kafei, 21, 18, retained));
    // MM Link wrappers are deliberately unsupported by the global SoH factory.
    // Type rejection is also exercised above on a real SOH::Animation instance.
    REQUIRE(!MmNormalActor::ValidateAnimation(kafei, 21, 89));
    auto texture = MmAssets_LoadResourceObjectFromMmArchive("objects/object_zov/gLuluEyeOpenTex");
    REQUIRE(MmNormalActor::ValidateTexture(texture));
    auto tex = std::dynamic_pointer_cast<Fast::Texture>(texture);
    auto width = tex->Width;
    tex->Width = 0;
    REQUIRE(!MmNormalActor::ValidateTexture(texture));
    tex->Width = width;
    auto dataSize = tex->ImageDataSize;
    tex->ImageDataSize = 0xFFFFFFFF;
    REQUIRE(!MmNormalActor::ValidateTexture(texture));
    tex->ImageDataSize = dataSize;
    REQUIRE(!MmAssets_LoadNormalActor(STATIC_STORY_ACTOR_LULU, 4, &disabled));
    std::printf("PASS actual archive loader: %u presentations (11 x Alt off/on), native faces; "
                "type/bounds/identity/retention negatives\n",
                loads);

    for (bool alt : { false, true })
        for (unsigned pose = 0; pose < 2; ++pose) {
            manager->SetAltAssetsEnabled(alt);
            const auto* p = StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_CHILD_KAFEI, pose);
            auto wrapper = *sMmArchive->LoadFile(p->animationPath)->Buffer;
            auto payload = *sMmArchive->LoadFile(MmKafei::ClipPath(pose) + 7)->Buffer;
            auto header = *sMmArchive->LoadFile(p->skeletonPath)->Buffer;
            REQUIRE(MmKafei::Wrapper(wrapper, pose) && MmKafei::Payload(payload, pose) &&
                    MmKafei::SkeletonBytes(header));
            for (size_t at : { 0U, 4U, 8U, 64U, 68U, 69U, 70U, 74U }) {
                auto bad = wrapper;
                bad[at] ^= 1;
                REQUIRE(!MmKafei::Wrapper(bad, pose));
            }
            for (size_t at : { 0U, 4U, 8U, 64U, 65U, 67U }) {
                auto bad = payload;
                bad[at] ^= 1;
                REQUIRE(!MmKafei::Payload(bad, pose));
            }
            for (size_t size : { 0U, 63U, 64U, 68U, 73U }) {
                auto bad = wrapper;
                bad.resize(size);
                REQUIRE(!MmKafei::Wrapper(bad, pose));
                bad = payload;
                bad.resize(size);
                REQUIRE(!MmKafei::Payload(bad, pose));
            }
            auto bad = wrapper;
            bad.push_back(0);
            REQUIRE(!MmKafei::Wrapper(bad, pose));
            bad = payload;
            bad.pop_back();
            REQUIRE(!MmKafei::Payload(bad, pose));
            bad = header;
            bad[83] ^= 1;
            REQUIRE(!MmKafei::SkeletonBytes(bad));
            for (unsigned i = 0; i < 21; ++i) {
                auto bytes = *sMmArchive->LoadFile(MmKafei::Path(MmKafei::Limbs[i].name))->Buffer;
                REQUIRE(MmKafei::LimbBytes(bytes, i));
                bytes[64] = 1;
                REQUIRE(!MmKafei::LimbBytes(bytes, i));
                bytes[64] = 2;
                bytes.back() ^= 1;
                REQUIRE(!MmKafei::LimbBytes(bytes, i));
            }
            // Poison every globally selected child. The private header must ignore them.
            for (const auto& row : MmKafei::Limbs)
                manager->CacheExternalResource(MmKafei::Path(row.name), texture);
            MmNormalActorResources first{}, second{};
            REQUIRE(MmAssets_LoadKafei(pose, &first) && MmAssets_LoadKafei(pose, &second));
            REQUIRE(first.skeleton != second.skeleton && first.playerFrames != second.playerFrames);
            REQUIRE(!first.animation && first.skeleton->sh.limbCount == 21 && first.skeleton->dListCount == 18);
            auto owner = static_cast<std::vector<MmNormalActor::Resource>*>(first.owner);
            REQUIRE(owner->size() == 35);
            for (unsigned i = 0; i < 21; ++i)
                REQUIRE(MmKafei::Limb(std::dynamic_pointer_cast<SOH::SkeletonLimb>(owner->at(i + 1)), i));
            for (unsigned i = 0; i < 8; ++i)
                REQUIRE(first.eyes[i]);
            for (unsigned i = 0; i < 4; ++i)
                REQUIRE(first.mouths[i]);
            struct {
                uint32_t before;
                int16_t joints[66];
                uint32_t after;
            } sample;
            sample.before = 0xabcddcba;
            sample.after = 0x12345678;
            uint16_t appearance = 0xffff;
            float cursor = 0;
            for (unsigned frame = 0; frame < p->frameCount; ++frame) {
                cursor = (float)frame;
                REQUIRE(StaticStoryMm_SampleKafei(first.playerFrames, p->frameCount, &cursor, 0, sample.joints,
                                                  &appearance));
                REQUIRE(memcmp(sample.joints, first.playerFrames + frame * 67, 132) == 0 && appearance == 0);
                REQUIRE(sample.before == 0xabcddcba && sample.after == 0x12345678);
            }
            cursor = p->frameCount - 1;
            REQUIRE(
                StaticStoryMm_SampleKafei(first.playerFrames, p->frameCount, &cursor, 1, sample.joints, &appearance) &&
                cursor == 0);
            REQUIRE(StaticStoryMm_SampleKafei(first.playerFrames, p->frameCount, &cursor, p->frameCount * 3 + 0.5f,
                                              sample.joints, &appearance) &&
                    cursor == 0.5f);
            REQUIRE(
                StaticStoryMm_SampleKafei(first.playerFrames, p->frameCount, &cursor, -1, sample.joints, &appearance) &&
                cursor == p->frameCount - 0.5f);
            cursor = 0;
            REQUIRE(StaticStoryMm_SampleKafei(first.playerFrames, p->frameCount, &cursor, -1e-30f, sample.joints,
                                              &appearance) &&
                    cursor < p->frameCount);
            float saved = cursor;
            REQUIRE(!StaticStoryMm_SampleKafei(first.playerFrames, p->frameCount, &cursor, INFINITY, sample.joints,
                                               &appearance) &&
                    cursor == saved);
            for (unsigned eye = 0; eye < 16; ++eye)
                for (unsigned mouth = 0; mouth < 16; ++mouth) {
                    auto face = StaticStoryMm_KafeiFace(0xff00 | (mouth << 4) | eye);
                    REQUIRE(face.eye == (eye >= 1 && eye <= 8 ? eye - 1 : 0));
                    REQUIRE(face.mouth == (mouth >= 1 && mouth <= 4 ? mouth - 1 : 0));
                }
            MmAssets_ReleaseNormalActor(first.owner);
            sMmResourceCache.clear();
            cursor = 0;
            REQUIRE(
                StaticStoryMm_SampleKafei(second.playerFrames, p->frameCount, &cursor, 1, sample.joints, &appearance));
            MmAssets_ReleaseNormalActor(second.owner);
        }
    REQUIRE(!MmAssets_LoadKafei(2, &disabled));
    puts("PASS Kafei archive: finite private 21 LOD/18 matrix skeleton, two bounded clips, 12 faces, all-frame "
         "canaries and lifecycle");
    // Headless Context destructor requires a Window. Explicit resource lifetime checks
    // precede this exit; application shutdown is intentionally outside this fixture.
    sMmResourceCache.clear();
    retained.clear();
    sMmArchive.reset();
    std::fflush(nullptr);
    std::_Exit(0);
}
