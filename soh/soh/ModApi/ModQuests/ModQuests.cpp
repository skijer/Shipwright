#include "ModQuests.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include <libultraship/libultraship.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ModApi/Labels/ModLabels.h"
#include "soh/ModApi/ModApi.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/SaveManager.h"
#include "soh/z_message_OTR.h"

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"

void gfx_texture_cache_clear();
void AudioHeap_DropPermanentCacheAtReset(void);
}

namespace {

constexpr size_t kLabelLength = 4;
constexpr const char* kSection = "modQuest";
#define CVAR_MOD_QUEST_WORLD CVAR_GENERAL("ModQuests.World")
#define CVAR_MOD_QUEST_ADOPTED(key) (std::string(CVAR_GENERAL("ModQuests.Adopted.")) + (key)).c_str()

struct ModQuest {
    std::string key;
    std::string name;
    std::string label;
    uint32_t flags;
    std::string owner;
    std::string archive; // file name in the mods folder, empty for the game's world
    std::string logo;    // "__OTR__" + path, empty for none
    bool inWorld;        // its archive covers the game's files; the owner's hooks run under this flag
};

// A list: hooks keep pointers to inWorld.
std::list<ModQuest> sQuests;
// The quest of the file being played. A char array, because save sections are written on the save thread.
char sActive[SOH_MOD_QUEST_KEY_SIZE] = "";
std::string sWorld;
const ModQuest* sChosen;
// The resources the last switch took out of the cache: the audio thread, the texture cache and draw calls may still
// point into them until the next switch.
std::vector<std::shared_ptr<Ship::IResource>> sReleased;

bool IsKey(const char* key) {
    if (key == nullptr || key[0] == '\0' || std::strlen(key) >= SOH_MOD_QUEST_KEY_SIZE) {
        return false;
    }
    return std::all_of(key, key + std::strlen(key), [](unsigned char character) {
        return std::isalnum(character) || character == '_' || character == '-';
    });
}

// The file's button draws its label with the file select font's capitals and digits only.
std::string LabelFor(const char* label) {
    std::string text;
    for (const char* character = label != nullptr ? label : ""; *character != '\0' && text.size() < kLabelLength;
         character++) {
        if (std::isalnum(static_cast<unsigned char>(*character))) {
            text += static_cast<char>(std::toupper(static_cast<unsigned char>(*character)));
        }
    }
    return text.empty() ? "MOD" : text;
}

ModQuest* Find(const std::string& key) {
    auto quest =
        std::find_if(sQuests.begin(), sQuests.end(), [&key](const ModQuest& candidate) { return candidate.key == key; });
    return quest == sQuests.end() ? nullptr : &*quest;
}

const ModQuest* At(uint32_t index) {
    if (index >= sQuests.size()) {
        return nullptr;
    }
    return &*std::next(sQuests.begin(), index);
}

std::shared_ptr<Ship::ArchiveManager> Archives() {
    return Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
}

std::shared_ptr<Ship::Archive> FindArchive(const std::string& fileName) {
    auto mounted = Archives()->GetArchives(); // a copy: the loop must not outlive it
    for (const auto& archive : *mounted) {
        if (std::filesystem::path(archive->GetPath()).filename().string() == fileName) {
            return archive;
        }
    }
    return nullptr;
}

// Takes every file the archive holds out of the resource cache, so the next load reads the archive now on top.
void ReleaseResources(const std::shared_ptr<Ship::Archive>& archive) {
    auto resources = Ship::Context::GetRawInstance()->GetResourceManager();
    auto files = archive->ListFiles(); // a copy: the loop must not outlive it
    for (const auto& [hash, path] : *files) {
        auto resource = resources->GetCachedResource(path, true);
        if (resource != nullptr) {
            sReleased.push_back(resource);
            resources->UnloadResource(path);
        }
    }
}

// Puts the archive of `world`'s quest on top of the game's files and every other quest's archive under them (the
// game loads the topmost copy of a file). Then everything read from the old order is read again: cached resources,
// textures, the message tables, and the audio caches at an audio reset.
void SetWorld(const std::string& world) {
    if (world == sWorld) {
        return;
    }
    auto mounted = Archives()->GetArchives();
    auto ordered = std::make_shared<std::vector<std::shared_ptr<Ship::Archive>>>();
    std::vector<std::shared_ptr<Ship::Archive>> under;
    std::shared_ptr<Ship::Archive> top;
    std::vector<std::shared_ptr<Ship::Archive>> moved;
    for (ModQuest& quest : sQuests) {
        auto archive = quest.archive.empty() ? nullptr : FindArchive(quest.archive);
        if (archive == nullptr) {
            continue;
        }
        (quest.key == world ? top : archive) = archive;
        if (quest.key != world) {
            under.push_back(archive);
        }
        if (quest.inWorld != (quest.key == world)) {
            moved.push_back(archive);
        }
    }
    for (const auto& archive : *mounted) {
        if (archive != top && std::find(under.begin(), under.end(), archive) == under.end()) {
            ordered->push_back(archive);
        }
    }
    ordered->insert(ordered->begin(), under.begin(), under.end());
    if (top != nullptr) {
        ordered->push_back(top);
    }

    sReleased.clear();
    for (const auto& archive : moved) {
        ReleaseResources(archive);
    }
    Archives()->SetArchives(ordered);
    if (Find(sWorld) != nullptr) {
        ModLabels_LeaveWorld(sWorld);
    }
    for (ModQuest& quest : sQuests) {
        quest.inWorld = quest.key == world;
    }
    sWorld = world;
    if (!world.empty()) {
        ModLabels_EnterWorld(world);
    }
    gfx_texture_cache_clear();
    OTRMessage_Reload();
    AudioHeap_DropPermanentCacheAtReset();
    Audio_QueueSeqCmd(0xF0000000 | (gSfxChannelLayout << 8) | gAudioSpecId);
    CVarSetString(CVAR_MOD_QUEST_WORLD, world.c_str());
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    SPDLOG_INFO("[ModQuests] World: {} ({} archive(s) moved, {} resource(s) to read again)",
                world.empty() ? "the game's own" : world, moved.size(), sReleased.size());
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnQuestWorldChange>(ModQuests_GetWorld());
}

void SetActive(const std::string& key) {
    std::snprintf(sActive, sizeof(sActive), "%s", key.c_str());
}

void InitFile(bool isDebug) {
    sActive[0] = '\0';
}

void SaveSection(const SaveContext& saveContext, int sectionID, bool fullSave) {
    // Always a key ("" for the game's own quests): a section saved with no data can't be read back
    SaveManager::Instance->SaveData("key", std::string(sActive));
}

void LoadSection() {
    std::string key;
    SaveManager::Instance->LoadData("key", key);
    if (IsKey(key.c_str())) {
        SetActive(key);
    }
}

std::string WorldOf(const char* key) {
    const ModQuest* quest = key != nullptr ? Find(key) : nullptr;
    return quest != nullptr && !quest->archive.empty() ? quest->key : "";
}

} // namespace

void ModQuests_Init() {
    SaveManager::Instance->AddInitFunction(InitFile);
    SaveManager::Instance->AddLoadFunction(kSection, 1, LoadSection);
    SaveManager::Instance->AddSaveFunction(kSection, 1, SaveSection, true, SECTION_PARENT_NONE);
}

extern "C" bool ModQuests_Register(const SOHModQuest* quest) {
    const std::string owner = ModApi_GetCurrentMod();
    if (quest == nullptr || quest->structSize < SOH_MOD_QUEST_MIN_SIZE || !IsKey(quest->key) ||
        quest->name == nullptr || quest->name[0] == '\0') {
        SPDLOG_ERROR("[ModQuests] Mod '{}' registered a quest without a valid key or name", owner);
        return false;
    }
    if (Find(quest->key) != nullptr) {
        SPDLOG_ERROR("[ModQuests] Mod '{}' registered the quest '{}', which another mod already has", owner,
                     quest->key);
        return false;
    }
    const size_t size = std::min<size_t>(quest->structSize, sizeof(SOHModQuest));
    SOHModQuest definition = {};
    std::memcpy(&definition, quest, size);
    if (definition.archive != nullptr && FindArchive(definition.archive) == nullptr) {
        SPDLOG_ERROR("[ModQuests] Mod '{}': the quest '{}' needs '{}', which isn't loaded", owner, quest->key,
                     definition.archive);
        return false;
    }
    // Until a file of the quest is played, its archive is in the world as it was mounted: on top.
    sQuests.push_back({ definition.key, definition.name, LabelFor(definition.label), definition.flags, owner,
                        definition.archive != nullptr ? definition.archive : "",
                        definition.logo != nullptr ? std::string("__OTR__") + definition.logo : "",
                        definition.archive != nullptr });
    if (definition.archive != nullptr) {
        sWorld = definition.key;
    }
    SPDLOG_INFO("[ModQuests] '{}' added the quest '{}' ({}){}", owner, quest->name, quest->key,
                definition.archive != nullptr ? std::string(", world ") + definition.archive : "");
    return true;
}

// Mods mount their archives on top, so after loading them the world is the last quest archive's. The world of the
// last quest played goes back on top before anything is drawn.
void ModQuests_RestoreWorld() {
    // Without a quest that has a world, the archives stay as mounted
    if (std::none_of(sQuests.begin(), sQuests.end(), [](const ModQuest& quest) { return !quest.archive.empty(); })) {
        return;
    }
    const std::string saved = CVarGetString(CVAR_MOD_QUEST_WORLD, "");
    const std::string current = sWorld;
    sWorld = "\n"; // differs from every key: the order of the mounted archives is set whatever it was
    for (ModQuest& quest : sQuests) {
        quest.inWorld = quest.key == current;
    }
    SetWorld(WorldOf(saved.c_str()));
}

void ModQuests_AdoptFiles() {
    for (const ModQuest& quest : sQuests) {
        if ((quest.flags & SOH_MOD_QUEST_ADOPTS_ORIGINAL_FILES) == 0 ||
            CVarGetInteger(CVAR_MOD_QUEST_ADOPTED(quest.key), 0) != 0) {
            continue;
        }
        for (int fileNum = 0; fileNum < SaveManager::MaxFiles; fileNum++) {
            SaveFileMetaInfo& meta = SaveManager::Instance->fileMetaInfo[fileNum];
            if (!meta.valid || meta.quest != QUEST_NORMAL || meta.modQuest[0] != '\0') {
                continue;
            }
            const std::filesystem::path path = Ship::Context::GetPathRelativeToAppDirectory("Save") + "/file" +
                                               std::to_string(fileNum + 1) + ".sav";
            try {
                nlohmann::json save;
                std::ifstream(path) >> save;
                save["sections"][kSection] = { { "version", 1 }, { "data", { { "key", quest.key } } } };
                if (!SaveManager::WriteFileSafely(path, save.dump(1))) {
                    continue;
                }
            } catch (const std::exception& e) {
                SPDLOG_ERROR("[ModQuests] Could not give file {} to '{}': {}", fileNum + 1, quest.key, e.what());
                continue;
            }
            std::snprintf(meta.modQuest, sizeof(meta.modQuest), "%s", quest.key.c_str());
            SPDLOG_INFO("[ModQuests] File {} was made before '{}' was a quest; it is now one of its files", fileNum + 1,
                        quest.key);
        }
        CVarSetInteger(CVAR_MOD_QUEST_ADOPTED(quest.key), 1);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
}

void ModQuests_OnFileLoaded() {
    SetWorld(WorldOf(ModQuests_GetActive()));
}

const bool* ModQuests_GetWorldFlag(const std::string& mod) {
    for (const ModQuest& quest : sQuests) {
        if (quest.owner == mod && !quest.archive.empty()) {
            return &quest.inWorld;
        }
    }
    return nullptr;
}

extern "C" const char* ModQuests_GetActive(void) {
    return sActive[0] != '\0' ? sActive : nullptr;
}

extern "C" const char* ModQuests_GetWorld(void) {
    return sWorld.empty() ? nullptr : sWorld.c_str();
}

extern "C" uint32_t ModQuests_Count(void) {
    return static_cast<uint32_t>(sQuests.size());
}

extern "C" const char* ModQuests_GetName(uint32_t index) {
    const ModQuest* quest = At(index);
    return quest != nullptr ? quest->name.c_str() : "";
}

extern "C" const char* ModQuests_GetLogo(uint32_t index) {
    const ModQuest* quest = At(index);
    if (quest == nullptr || quest->logo.empty() ||
        !ResourceMgr_FileExists(quest->logo.c_str() + std::strlen("__OTR__"))) {
        return nullptr;
    }
    return quest->logo.c_str();
}

extern "C" bool ModQuests_IsFileAllowed(const char* modQuest) {
    return modQuest == nullptr || modQuest[0] == '\0' || Find(modQuest) != nullptr;
}

extern "C" const char* ModQuests_GetFileLabel(const char* modQuest) {
    if (modQuest == nullptr || modQuest[0] == '\0') {
        return nullptr;
    }
    const ModQuest* quest = Find(modQuest);
    return quest != nullptr ? quest->label.c_str() : "MOD";
}

extern "C" void ModQuests_Choose(uint32_t index) {
    sChosen = At(index);
}

extern "C" void ModQuests_UseGameWorld(void) {
    sChosen = nullptr;
    if (gPlayState == nullptr) {
        SetWorld("");
    }
}

// A new mod quest file plays as Original Quest and remembers its quest (Sram_InitSave, after the save is reset). The
// new file's world goes on top now, so only that world's mods set up the file before its first save.
extern "C" void ModQuests_ApplyToNewFile(bool modQuest) {
    if (modQuest && sChosen != nullptr) {
        gSaveContext.ship.quest.id = QUEST_NORMAL;
        SetActive(sChosen->key);
        SPDLOG_INFO("[ModQuests] New file {} is '{}'", gSaveContext.fileNum + 1, sActive);
    }
    SetWorld(WorldOf(ModQuests_GetActive()));
}
