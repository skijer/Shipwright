// The actual cache lookup/load decisions and game helpers are compiled below.
// Archive I/O/import and thread scheduling are replaced by deterministic fixtures.
#include <fast/resource/ResourceType.h>
#include <fast/resource/type/DisplayList.h>
#include <fast/resource/type/Texture.h>
#include <cstdio>
#include <cstring>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <variant>

#define SPDLOG_ERROR(...) ((void)0)
#define SPDLOG_TRACE(...) ((void)0)

namespace BS {
using priority_t = int;
namespace pr {
constexpr int highest = 0;
}
struct FixtureThreadPool {
    template <class F> auto submit_task(F callback, priority_t) {
        return std::async(std::launch::deferred, std::move(callback)).share();
    }
};
} // namespace BS

namespace Ship {
struct ResourceIdentifier {
    std::string Path;
    uintptr_t Owner = 0;
    std::shared_ptr<Archive> Parent;
    auto operator<=>(const ResourceIdentifier&) const = default;
};
struct FixtureArchive {
    std::set<std::string> files;
    bool HasFile(const std::string& path) {
        return files.contains(path);
    }
};
struct FixtureLoader {
    std::map<std::string, std::shared_ptr<IResource>> resources;
    std::shared_ptr<IResource> LoadResource(const std::string& path, std::shared_ptr<File>,
                                            std::shared_ptr<ResourceInitData>) {
        const auto found = resources.find(path);
        return found == resources.end() ? nullptr : found->second;
    }
};
class ResourceManager {
  public:
    enum class ResourceLoadError { None, NotCached, NotFound };
    using CacheLine = std::variant<ResourceLoadError, std::shared_ptr<IResource>>;
    std::map<ResourceIdentifier, CacheLine> mResourceCache;
    bool mAltAssetsEnabled = false;
    uintptr_t mDefaultCacheOwner = 0;
    std::shared_ptr<Archive> mDefaultCacheArchive;
    std::mutex mMutex;
    auto IsAltAssetsEnabled() const {
        return mAltAssetsEnabled;
    }
    std::shared_ptr<FixtureArchive> mArchiveManager = std::make_shared<FixtureArchive>();
    std::shared_ptr<FixtureLoader> loader = std::make_shared<FixtureLoader>();
    std::shared_ptr<BS::FixtureThreadPool> mThreadPool = std::make_shared<BS::FixtureThreadPool>();
    auto GetResourceLoader() {
        return loader;
    }
    bool OtrSignatureCheck(const char* path) {
        return path != nullptr && std::strncmp(path, "__OTR__", 7) == 0;
    }
    std::shared_ptr<File> LoadFileProcess(const std::string& path) {
        return mArchiveManager->HasFile(path) ? std::make_shared<File>() : nullptr;
    }
    std::shared_ptr<IResource> LoadResourceProcess(const ResourceIdentifier&, bool = false,
                                                   std::shared_ptr<ResourceInitData> = nullptr);
    std::shared_ptr<IResource> LoadResourceProcess(const std::string&, bool = false,
                                                   std::shared_ptr<ResourceInitData> = nullptr);
    std::shared_future<std::shared_ptr<IResource>> LoadResourceAsync(const ResourceIdentifier&, bool, BS::priority_t,
                                                                     std::shared_ptr<ResourceInitData> = nullptr);
    std::shared_future<std::shared_ptr<IResource>> LoadResourceAsync(const std::string&, bool, BS::priority_t,
                                                                     std::shared_ptr<ResourceInitData> = nullptr);
    std::shared_ptr<IResource> LoadResource(const ResourceIdentifier&, bool = false,
                                            std::shared_ptr<ResourceInitData> = nullptr);
    std::shared_ptr<IResource> LoadResource(const std::string&, bool = false,
                                            std::shared_ptr<ResourceInitData> = nullptr);
    CacheLine CheckCache(const ResourceIdentifier&, bool);
    CacheLine CheckCache(const std::string&, bool);
    std::shared_ptr<IResource> GetCachedResource(const ResourceIdentifier&, bool = false);
    std::shared_ptr<IResource> GetCachedResource(const std::string&, bool = false);
    std::shared_ptr<IResource> GetCachedResource(CacheLine);
};
#include "alt_segment_cache.inc"

struct Context {
    std::shared_ptr<ResourceManager> manager = std::make_shared<ResourceManager>();
    static Context* GetRawInstance() {
        static Context context;
        return &context;
    }
    std::shared_ptr<ResourceManager> GetResourceManager() {
        return manager;
    }
};
} // namespace Ship

static std::set<std::string> ExtensionCache;
static bool ResourceMgr_IsGameMasterQuest() {
    return false;
}
static int ResourceMgr_OTRSigCheck(char* path) {
    return Ship::Context::GetRawInstance()->manager->OtrSignatureCheck(path);
}
#include "alt_segment_helpers.inc"

static unsigned failures = 0;
#define REQUIRE(condition)                                                    \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

template <class T> static std::shared_ptr<T> Resource(const std::string& path, Fast::ResourceType type) {
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Path = path;
    init->Type = static_cast<uint32_t>(type);
    return std::make_shared<T>(init);
}

static uintptr_t Bind(const std::string& path, int segment = 8) {
    Gfx command{};
    gSPSegment(&command, segment, reinterpret_cast<uintptr_t>(path.c_str()));
    REQUIRE(command.words.w0 == (0xDB060000u | (segment * 4)));
    return command.words.w1;
}

int main(int argc, char** argv) {
    auto rm = Ship::Context::GetRawInstance()->manager;
    const std::string name = "objects/object_link_child/gLinkChildEyesOpenTex";
    const std::string path = "__OTR__" + name;
    const std::string alt = "alt/" + name;
    auto native = Resource<Fast::Texture>(name, Fast::ResourceType::Texture);
    auto material = Resource<Fast::DisplayList>(alt, Fast::ResourceType::DisplayList);
    material->Instructions = { gsSPEndDisplayList() };
    rm->loader->resources[name] = native;
    rm->loader->resources[alt] = material;
    rm->mArchiveManager->files = { name, alt }; // Supplied Din packs use a direct binary DL at the texture path.
    ExtensionCache.insert(alt);
    REQUIRE(rm->LoadResource(name) == native); // warm vanilla texture, cold Alt DL
    REQUIRE(rm->GetCachedResource(alt, true) == nullptr);
    REQUIRE(Bind(path) == reinterpret_cast<uintptr_t>(path.c_str()));
    rm->mAltAssetsEnabled = true;
    REQUIRE(Bind(path) == reinterpret_cast<uintptr_t>(material->Instructions.data()));
    REQUIRE(rm->GetCachedResource(name, true) == native); // Neither cache entry may be evicted.
    REQUIRE(rm->GetCachedResource(alt, true) == material);
    rm->mAltAssetsEnabled = false;
    REQUIRE(Bind(path) == reinterpret_cast<uintptr_t>(path.c_str()));
    rm->mAltAssetsEnabled = true;
    REQUIRE(Bind(path, 9) == reinterpret_cast<uintptr_t>(material->Instructions.data()));
    REQUIRE(ResourceMgr_LoadIfDListByName(name.c_str()) == reinterpret_cast<char*>(material->Instructions.data()));

    const auto checkOther = [&](const char* suffix, const std::shared_ptr<Ship::IResource>& replacement,
                                bool metadataOnly, bool enabled, Gfx* expected = nullptr) {
        const std::string base = name + suffix;
        const std::string tagged = "__OTR__" + base;
        const std::string selected = "alt/" + base;
        rm->mAltAssetsEnabled = false;
        auto original = Resource<Fast::Texture>(base, Fast::ResourceType::Texture);
        rm->loader->resources[base] = original;
        rm->mArchiveManager->files.insert(base);
        REQUIRE(rm->LoadResource(base) == original);
        if (replacement) {
            rm->loader->resources[selected] = replacement;
            rm->mArchiveManager->files.insert(selected + (metadataOnly ? ".meta" : ""));
            ExtensionCache.insert(selected);
        }
        rm->mAltAssetsEnabled = enabled;
        REQUIRE(Bind(tagged) ==
                (expected ? reinterpret_cast<uintptr_t>(expected) : reinterpret_cast<uintptr_t>(tagged.c_str())));
        REQUIRE(rm->GetCachedResource(base, true) == original);
    };
    // This pinned archive loader requires bytes at the base path before deserialization.
    // A metadata-only name is indexed by ExtensionCache but cannot supply a display list.
    checkOther("MetadataOnly", material, true, true);
    checkOther("Missing", nullptr, false, true);
    checkOther("Disabled", material, false, false);
    checkOther("HD", Resource<Fast::Texture>("alt/hd", Fast::ResourceType::Texture), false, true);
    checkOther("Empty", Resource<Fast::DisplayList>("alt/empty", Fast::ResourceType::DisplayList), false, true);
    // A misleading metadata tag must not permit a Texture-to-DisplayList cast.
    checkOther("WrongType", Resource<Fast::Texture>("alt/wrong", Fast::ResourceType::DisplayList), false, true);
    REQUIRE(ResourceMgr_LoadIfDListByName(nullptr) == nullptr);
    REQUIRE(ResourceMgr_LoadIfDListByName("__OTR__objects/not_present") == nullptr);
    if (failures)
        return 1;
    std::printf("PASS %s Alt segment binding: cold direct DL over warm texture, on/off, retained caches, HD texture, "
                "metadata-only/missing/empty/wrong type/null fallback\n",
                argc > 1 ? argv[1] : "game");
}
