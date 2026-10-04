// Exercise the production archive adapter. The archive boundary uses exact
// filenames; the legacy ResourceMgr existence boundary strips extensions.
#include "test_require.h"
#include "soh/Enhancements/audio/MidnaAudioResources.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <memory>
#include <string>

#define CVAR_SETTING(name) "gSettings." name
#define CVAR_ENHANCEMENT(name) "gEnhancements." name
static int master = 40, sfx = 50, loads;
static int enabled = -1;
static std::map<std::string, std::string> savedAssignments;
const char* CVarGetString(const char* key, const char* fallback) {
    auto it = savedAssignments.find(key);
    return it == savedAssignments.end() ? fallback : it->second.c_str();
}
void CVarSetString(const char* key, const char* value) {
    savedAssignments[key] = value;
}
int CVarGetInteger(const char* key, int fallback) {
    if (std::strcmp(key, "gEnhancements.MidnaCompanion") == 0) {
        REQUIRE(fallback == 0);
        return enabled < 0 ? fallback : enabled;
    }
    if (std::strcmp(key, "gSettings.Volume.Master") == 0)
        return master;
    if (std::strcmp(key, "gSettings.Volume.SFX") == 0)
        return sfx;
    REQUIRE(false);
    return fallback;
}
uint8_t ResourceMgr_FileExists(const char* key) {
    // OTRAudio_Init runs before OTRExtScanner fills this cache.
    return 0;
}
namespace Ship {
struct File {
    std::shared_ptr<std::vector<char>> Buffer;
};
struct ArchiveManager {
    std::map<std::string, std::shared_ptr<File>> files;
    bool HasFile(const std::string& key) {
        return files.count(key) != 0;
    }
    std::shared_ptr<File> LoadFile(const std::string& key) {
        ++loads;
        return files.at(key);
    }
    std::shared_ptr<std::vector<std::string>> ListFiles(const std::string& pattern) {
        REQUIRE(pattern == "objects/midna_navi/audio/*");
        auto result = std::make_shared<std::vector<std::string>>();
        for (const auto& [path, file] : files) {
            if (path.rfind("objects/midna_navi/audio/", 0) == 0) {
                result->push_back(path);
            }
        }
        return result;
    }
};
struct ResourceManager {
    ArchiveManager archive;
    ArchiveManager* GetArchiveManager() {
        return &archive;
    }
};
static auto manager = std::make_shared<ResourceManager>();
static auto otherManager = std::make_shared<ResourceManager>();
static auto ownManager = manager;
static auto activeManager = manager;
struct CrossRMRegistry {
    static std::shared_ptr<ResourceManager> Get(const std::string& key) {
        REQUIRE(key == "oot");
        return ownManager;
    }
};
struct Context {
    static Context* GetRawInstance() {
        static Context context;
        return &context;
    }
    std::shared_ptr<ResourceManager> GetResourceManager() {
        return activeManager;
    }
};
} // namespace Ship

namespace MidnaAudioResources {
/* PRODUCTION_MIDNA_AUDIO_RESOURCES */
}

int main() {
    const char* path = "objects/midna_navi/audio/dash.wav";
    auto file = std::make_shared<Ship::File>();
    file->Buffer = std::make_shared<std::vector<char>>(std::initializer_list<char>{ 'R', 'I', 'F', 'F', -1 });
    Ship::manager->archive.files[path] = file;
    Ship::manager->archive.files["objects/midna_navi/poc1/MidnaFloatDL"] = file;
    std::vector<uint8_t> bytes;
    REQUIRE(!MidnaAudioResources::Enabled()); // unset option defaults off
    enabled = 1;
    REQUIRE(MidnaAudioResources::Enabled());
    enabled = 0;
    REQUIRE(!MidnaAudioResources::Enabled());
#ifdef COMBO_BUILD
    // Cross-game rendering or MM startup may leave MM active. It must never
    // supply OoT's model or clips, even when the OoT manager is absent.
    Ship::activeManager = Ship::otherManager;
#endif
    REQUIRE(MidnaAudioResources::HasModel());
    Ship::manager->archive.files["objects/midna_navi/audio/extra.WAV"] = file;
    Ship::manager->archive.files["objects/midna_navi/audio/notes.txt"] = file;
    REQUIRE(
        (MidnaAudioResources::ListClips() == std::vector<std::string>{ path, "objects/midna_navi/audio/extra.WAV" }));
    REQUIRE(MidnaAudioResources::ReadAssignment("Emerge", path) == path);
    MidnaAudioResources::WriteAssignment("Emerge", "objects/midna_navi/audio/extra.WAV");
    REQUIRE(MidnaAudioResources::ReadAssignment("Emerge", path) == "objects/midna_navi/audio/extra.WAV");
    REQUIRE(savedAssignments.at("gEnhancements.MidnaAudio.Emerge") == "objects/midna_navi/audio/extra.WAV");
    MidnaAudioResources::WriteAssignment("Emerge", "");
    REQUIRE(MidnaAudioResources::ReadAssignment("Emerge", path).empty());
    REQUIRE(MidnaAudioResources::ReadClip(path, bytes));
    REQUIRE(loads == 1 && bytes.size() == 5 && bytes[4] == 255);
    REQUIRE(!MidnaAudioResources::ReadClip("objects/midna_navi/audio/absent.wav", bytes));
    REQUIRE(loads == 1);
    file->Buffer->clear();
    REQUIRE(!MidnaAudioResources::ReadClip(path, bytes));
    file->Buffer.reset();
    REQUIRE(!MidnaAudioResources::ReadClip(path, bytes));
    Ship::manager->archive.files[path].reset();
    REQUIRE(!MidnaAudioResources::ReadClip(path, bytes));
#ifdef COMBO_BUILD
    Ship::otherManager->archive.files[path] = file;
    Ship::otherManager->archive.files["objects/midna_navi/poc1/MidnaFloatDL"] = file;
    Ship::ownManager.reset();
    REQUIRE(!MidnaAudioResources::HasModel());
    REQUIRE(!MidnaAudioResources::ReadClip(path, bytes));
    REQUIRE(MidnaAudioResources::ListClips().empty());
    Ship::ownManager = Ship::manager;
    Ship::manager->archive.files.clear();
    REQUIRE(!MidnaAudioResources::HasModel());
    REQUIRE(!MidnaAudioResources::ReadClip(path, bytes));
#endif
    REQUIRE(MidnaAudioResources::Gain() == 0.2f);
    master = 0;
    REQUIRE(MidnaAudioResources::Gain() == 0);
    master = 100;
    sfx = 0;
    REQUIRE(MidnaAudioResources::Gain() == 0);
    sfx = 200;
    REQUIRE(MidnaAudioResources::Gain() == 1);
    puts("PASS: default-off option, own-game archive ownership, exact WAV paths and Master/SFX volume");
}
