#include "MidnaAudioResources.h"

#include <algorithm>
#include <cctype>
#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#ifdef COMBO_BUILD
#include <ship/resource/CrossRMRegistry.h>
#endif
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"

namespace {
std::shared_ptr<Ship::ResourceManager> GetOwnResourceManager() {
#ifdef COMBO_BUILD
    // The shared Context may currently belong to MM, including during startup
    // and cross-game rendering. Optional OoT audio must use only OoT's packs.
    return Ship::CrossRMRegistry::Get("oot");
#else
    auto context = Ship::Context::GetRawInstance();
    return context != nullptr ? context->GetResourceManager() : nullptr;
#endif
}
} // namespace

namespace MidnaAudioResources {
std::vector<std::string> ListClips() {
    const auto manager = GetOwnResourceManager();
    const auto archives = manager != nullptr ? manager->GetArchiveManager() : nullptr;
    if (archives == nullptr) {
        return {};
    }
    const auto files = archives->ListFiles("objects/midna_navi/audio/*");
    std::vector<std::string> result;
    if (files != nullptr) {
        for (const auto& path : *files) {
            if (path.size() < 4) {
                continue;
            }
            std::string extension = path.substr(path.size() - 4);
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension == ".wav") {
                result.push_back(path);
            }
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::string ReadAssignment(const char* event, const char* fallback) {
    const std::string key = std::string(CVAR_ENHANCEMENT("MidnaAudio.")) + event;
    return CVarGetString(key.c_str(), fallback);
}

void WriteAssignment(const char* event, const std::string& path) {
    const std::string key = std::string(CVAR_ENHANCEMENT("MidnaAudio.")) + event;
    CVarSetString(key.c_str(), path.c_str());
}

bool Enabled() {
    return CVarGetInteger(CVAR_ENHANCEMENT("MidnaCompanion"), 0) != 0;
}

bool HasModel() {
    // Audio initializes before OTRExtScanner populates the resource cache.
    const auto manager = GetOwnResourceManager();
    const auto archives = manager != nullptr ? manager->GetArchiveManager() : nullptr;
    return archives != nullptr && archives->HasFile("objects/midna_navi/poc1/MidnaFloatDL");
}

bool ReadClip(const char* path, std::vector<uint8_t>& bytes) {
    // These are raw archive files with extensions. ResourceMgr_FileExists uses
    // the extension-stripped resource cache, which cannot answer this lookup.
    const auto manager = GetOwnResourceManager();
    const auto archives = manager != nullptr ? manager->GetArchiveManager() : nullptr;
    if (archives == nullptr || !archives->HasFile(path)) {
        return false;
    }
    auto file = archives->LoadFile(path);
    if (!file || !file->Buffer || file->Buffer->empty() || file->Buffer->size() > 32000 * 10 * 2 + 65536) {
        return false;
    }
    bytes.assign(file->Buffer->begin(), file->Buffer->end());
    return true;
}

float Gain() {
    const float master = std::clamp(CVarGetInteger(CVAR_SETTING("Volume.Master"), 40) / 100.0f, 0.0f, 1.0f);
    const float sfx = std::clamp(CVarGetInteger(CVAR_SETTING("Volume.SFX"), 100) / 100.0f, 0.0f, 1.0f);
    return master * sfx;
}
} // namespace MidnaAudioResources
