#include "ModQuests.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>

#include <fast/interpreter.h>
#include <libultraship/libultraship.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ModApi/Labels/ModLabels.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/SaveManager.h"
#include "soh/cvar_prefixes.h"
#include "soh/z_message_OTR.h"

extern "C" {
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

namespace {

constexpr const char* kSaveSection = "modQuest";
constexpr const char* kGameWorldSetting = "oot";
constexpr size_t kLabelLength = 4;
// The quest menu keeps quests in an s8, QUEST_MOD and up.
constexpr size_t kMaxQuests = INT8_MAX - QUEST_MOD;
#define CVAR_TITLE_WORLD CVAR_SETTING("ModQuests.TitleWorld")
#define CVAR_LAST_WORLD CVAR_GENERAL("ModQuests.LastWorld")

struct ModQuest {
    ModQuestDeclaration declaration;
    std::string owner;
    std::vector<std::shared_ptr<Ship::Archive>> archives;
    std::string logoPath;
    bool isOnTop;
    bool isAwake;
};

// A list, not a vector: gated hooks keep pointers to isAwake.
std::list<ModQuest> sQuests;
// A char array, not a std::string: save sections are written on the save thread.
char sActive[MOD_QUEST_KEY_SIZE] = "";
std::string sWorld;
const ModQuest* sChosen = nullptr;
bool sHasFinishedLoading = false;
// The last switch's unloaded resources: audio and draw calls may still point into them until the next switch.
std::vector<std::shared_ptr<Ship::IResource>> sReleased;

bool HasWindow() {
    return Ship::Context::GetRawInstance()->GetWindow() != nullptr;
}

std::shared_ptr<Ship::ArchiveManager> GetArchiveManager() {
    return Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
}

bool IsValidKey(const std::string& key) {
    if (key.empty() || key.size() >= MOD_QUEST_KEY_SIZE) {
        return false;
    }
    return std::all_of(key.begin(), key.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '_' || character == '-';
    });
}

// The file button draws its label with the file select font's capitals and digits only.
std::string FormatLabel(const std::string& label) {
    std::string text;
    for (unsigned char character : label) {
        if (text.size() == kLabelLength) {
            break;
        }
        if (std::isalnum(character)) {
            text += static_cast<char>(std::toupper(character));
        }
    }
    return text.empty() ? "MOD" : text;
}

ModQuest* FindQuest(const std::string& key) {
    auto quest = std::find_if(sQuests.begin(), sQuests.end(),
                              [&key](const ModQuest& candidate) { return candidate.declaration.key == key; });
    return quest == sQuests.end() ? nullptr : &*quest;
}

const ModQuest* FindQuestOfMod(const std::string& mod) {
    auto quest =
        std::find_if(sQuests.begin(), sQuests.end(), [&mod](const ModQuest& candidate) { return candidate.owner == mod; });
    return quest == sQuests.end() ? nullptr : &*quest;
}

const ModQuest* GetQuestAt(uint32_t index) {
    if (index >= sQuests.size()) {
        return nullptr;
    }
    return &*std::next(sQuests.begin(), index);
}

std::string GetWorldOf(const std::string& key) {
    const ModQuest* quest = FindQuest(key);
    return quest != nullptr && quest->declaration.isRomhack ? key : "";
}

std::shared_ptr<Ship::Archive> FindMountedArchive(const std::string& pathOrFileName) {
    auto mounted = GetArchiveManager()->GetArchives();
    for (const auto& archive : *mounted) {
        if (archive->GetPath() == pathOrFileName ||
            std::filesystem::path(archive->GetPath()).filename().string() == pathOrFileName) {
            return archive;
        }
    }
    return nullptr;
}

const ModQuest* FindQuestOfArchive(const std::shared_ptr<Ship::Archive>& archive) {
    auto quest = std::find_if(sQuests.begin(), sQuests.end(), [&archive](const ModQuest& candidate) {
        return std::find(candidate.archives.begin(), candidate.archives.end(), archive) != candidate.archives.end();
    });
    return quest == sQuests.end() ? nullptr : &*quest;
}

void ReleaseCachedResources(const std::shared_ptr<Ship::Archive>& archive) {
    auto resources = Ship::Context::GetRawInstance()->GetResourceManager();
    auto files = archive->ListFiles();
    if (files == nullptr) {
        return;
    }
    for (const auto& [hash, path] : *files) {
        auto resource = resources->GetCachedResource(path, true);
        if (resource != nullptr) {
            sReleased.push_back(resource);
            resources->UnloadResource(path);
        }
    }
}

// The game reads the topmost copy of a file: sleeping worlds go under every archive, the awake one over them.
void StackArchives() {
    auto mounted = GetArchiveManager()->GetArchives();
    auto ordered = std::make_shared<std::vector<std::shared_ptr<Ship::Archive>>>();
    std::vector<std::shared_ptr<Ship::Archive>> awake;
    for (const ModQuest& quest : sQuests) {
        auto& destination = quest.isOnTop ? awake : *ordered;
        destination.insert(destination.end(), quest.archives.begin(), quest.archives.end());
    }
    for (const auto& archive : *mounted) {
        if (FindQuestOfArchive(archive) == nullptr) {
            ordered->push_back(archive);
        }
    }
    ordered->insert(ordered->end(), awake.begin(), awake.end());
    GetArchiveManager()->SetArchives(ordered);
}

void RefreshGameCaches() {
    OTRMessage_Reload();
    if (!HasWindow() || !sHasFinishedLoading) {
        return;
    }
    gfx_texture_cache_clear();
    AudioHeap_DropPermanentCacheAtReset();
    Audio_QueueSeqCmd(0xF0000000 | (gSfxChannelLayout << 8) | gAudioSpecId);
}

bool ShouldBeAwake(const ModQuest& quest) {
    return quest.declaration.key == (quest.declaration.isRomhack ? sWorld : std::string(sActive));
}

void SetAwake(ModQuest& quest, bool isAwake) {
    if (quest.isAwake == isAwake) {
        return;
    }
    quest.isAwake = isAwake;
    if (isAwake) {
        ModLabels_WakeQuest(quest.declaration.key);
    } else {
        ModLabels_SleepQuest(quest.declaration.key);
    }
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnModQuestAwake>(quest.declaration.key.c_str(), isAwake);
}

// Sleepers first and newest first, so each puts back the labels it found when it woke.
void RefreshAwakeQuests() {
    for (auto quest = sQuests.rbegin(); quest != sQuests.rend(); ++quest) {
        if (!ShouldBeAwake(*quest)) {
            SetAwake(*quest, false);
        }
    }
    for (ModQuest& quest : sQuests) {
        if (ShouldBeAwake(quest)) {
            SetAwake(quest, true);
        }
    }
}

void SetWorld(const std::string& world) {
    std::vector<std::shared_ptr<Ship::Archive>> moved;
    for (ModQuest& quest : sQuests) {
        const bool isOnTop = quest.declaration.key == world;
        if (quest.isOnTop != isOnTop) {
            moved.insert(moved.end(), quest.archives.begin(), quest.archives.end());
            quest.isOnTop = isOnTop;
        }
    }
    sWorld = world;
    if (!moved.empty()) {
        sReleased.clear();
        for (const auto& archive : moved) {
            ReleaseCachedResources(archive);
        }
        StackArchives();
        RefreshGameCaches();
        SPDLOG_INFO("[ModQuests] World: {} ({} archive(s) moved, {} resource(s) to read again)",
                    world.empty() ? "the game's own" : world, moved.size(), sReleased.size());
    }
    RefreshAwakeQuests();
}

void SetActive(const std::string& key) {
    std::snprintf(sActive, sizeof(sActive), "%s", key.c_str());
}

void EnterFileWorld() {
    const std::string world = GetWorldOf(sActive);
    if (gPlayState != nullptr && world != sWorld) {
        SPDLOG_WARN("[ModQuests] A file of '{}' was loaded during play; its world waits for the title screen",
                    world.empty() ? "the game" : world);
        RefreshAwakeQuests();
    } else {
        SetWorld(world);
    }
    CVarSetString(CVAR_LAST_WORLD, world.c_str());
    CVarSave();
}

std::string GetTitleWorld() {
    const std::string setting = CVarGetString(CVAR_TITLE_WORLD, "");
    if (setting == kGameWorldSetting) {
        return "";
    }
    return GetWorldOf(setting.empty() ? CVarGetString(CVAR_LAST_WORLD, "") : setting);
}

void ClearActiveOnNewFile(bool isDebug) {
    sActive[0] = '\0';
}

// Always a key, "" for the game's own quests: a section saved with no data isn't read back.
void SaveActive(const SaveContext& saveContext, int sectionID, bool fullSave) {
    SaveManager::Instance->SaveData("key", std::string(sActive));
}

void LoadActive() {
    std::string key;
    SaveManager::Instance->LoadData("key", key);
    if (IsValidKey(key)) {
        SetActive(key);
    }
}

bool AdoptFile(int fileNum, const std::string& key) {
    const std::filesystem::path path = SaveManager::Instance->GetFileName(fileNum);
    try {
        nlohmann::json save;
        std::ifstream(path) >> save;
        save["sections"][kSaveSection] = { { "version", 1 }, { "data", { { "key", key } } } };
        return SaveManager::WriteFileSafely(path, save.dump(1));
    } catch (const std::exception& exception) {
        SPDLOG_ERROR("[ModQuests] Could not give file {} to '{}': {}", fileNum + 1, key, exception.what());
        return false;
    }
}

void AdoptOriginalFilesInto(const ModQuest& quest) {
    for (int fileNum = 0; fileNum < SaveManager::MaxFiles; fileNum++) {
        SaveFileMetaInfo& meta = SaveManager::Instance->fileMetaInfo[fileNum];
        const bool isUnclaimedOriginal = meta.valid && meta.quest == QUEST_NORMAL && meta.modQuest[0] == '\0';
        if (!isUnclaimedOriginal || !AdoptFile(fileNum, quest.declaration.key)) {
            continue;
        }
        std::snprintf(meta.modQuest, sizeof(meta.modQuest), "%s", quest.declaration.key.c_str());
        SPDLOG_INFO("[ModQuests] File {} predates the quest '{}' and is now one of its files", fileNum + 1,
                    quest.declaration.key);
    }
}

} // namespace

void ModQuests_Init() {
    SaveManager::Instance->AddInitFunction(ClearActiveOnNewFile);
    SaveManager::Instance->AddLoadFunction(kSaveSection, 1, LoadActive);
    SaveManager::Instance->AddSaveFunction(kSaveSection, 1, SaveActive, true, SECTION_PARENT_NONE);
}

bool ModQuests_Declare(const ModQuestDeclaration& declaration, const std::string& owner,
                       const std::string& ownArchivePath) {
    if (!IsValidKey(declaration.key) || declaration.name.empty()) {
        SPDLOG_ERROR("[ModQuests] Mod '{}' declares a quest without a valid key or name", owner);
        return false;
    }
    if (sQuests.size() >= kMaxQuests) {
        SPDLOG_ERROR("[ModQuests] Mod '{}' declares the quest '{}', past the {} the quest menu holds", owner,
                     declaration.key, kMaxQuests);
        return false;
    }
    if (FindQuest(declaration.key) != nullptr || FindQuestOfMod(owner) != nullptr) {
        SPDLOG_ERROR("[ModQuests] Mod '{}' declares the quest '{}', but the key or the mod already has one", owner,
                     declaration.key);
        return false;
    }
    // Its ModInit runs awake, as if its world were on top; FinishLoading puts it to sleep, so its mod puts back
    // what it wrote into the game's tables.
    ModQuest quest = { declaration, owner, {}, "", declaration.isRomhack, true };
    quest.declaration.label = FormatLabel(declaration.label);
    if (!declaration.logo.empty()) {
        quest.logoPath = "__OTR__" + declaration.logo;
    }
    if (declaration.isRomhack) {
        std::vector<std::string> names = { ownArchivePath };
        names.insert(names.end(), declaration.worldArchives.begin(), declaration.worldArchives.end());
        for (const std::string& name : names) {
            auto archive = FindMountedArchive(name);
            if (archive == nullptr) {
                SPDLOG_ERROR("[ModQuests] The quest '{}' needs '{}', which isn't mounted", declaration.key, name);
                return false;
            }
            if (std::find(quest.archives.begin(), quest.archives.end(), archive) == quest.archives.end()) {
                quest.archives.push_back(archive);
            }
        }
    }
    sQuests.push_back(std::move(quest));
    SPDLOG_INFO("[ModQuests] '{}' adds the {} '{}' ({})", owner, declaration.isRomhack ? "romhack" : "mode",
                declaration.name, declaration.key);
    return true;
}

// Mods mount over the game's files; every world but the title screen's goes under them before anything is drawn.
// Headless hosts (Fleet's seed workers) have no title screen and stay in the game's world.
void ModQuests_FinishLoading() {
    SetWorld(HasWindow() ? GetTitleWorld() : "");
    sHasFinishedLoading = true;
}

void ModQuests_AdoptOriginalFiles() {
    for (const ModQuest& quest : sQuests) {
        const std::string adoptedSetting = std::string(CVAR_GENERAL("ModQuests.Adopted.")) + quest.declaration.key;
        if (!quest.declaration.adoptsOriginalFiles || CVarGetInteger(adoptedSetting.c_str(), 0) != 0) {
            continue;
        }
        AdoptOriginalFilesInto(quest);
        CVarSetInteger(adoptedSetting.c_str(), 1);
        CVarSave();
    }
}

void ModQuests_EnterLoadedFile() {
    EnterFileWorld();
}

const bool* ModQuests_GetAwakeFlag(const std::string& mod) {
    const ModQuest* quest = FindQuestOfMod(mod);
    return quest != nullptr ? &quest->isAwake : nullptr;
}

const char* ModQuests_GetQuestOfMod(const std::string& mod) {
    const ModQuest* quest = FindQuestOfMod(mod);
    return quest != nullptr ? quest->declaration.key.c_str() : nullptr;
}


std::vector<ModQuestWorld> ModQuests_ListTitleChoices() {
    std::vector<ModQuestWorld> choices = { { "", "Last Played" }, { kGameWorldSetting, "Ocarina of Time" } };
    for (const ModQuest& quest : sQuests) {
        if (quest.declaration.isRomhack) {
            choices.push_back({ quest.declaration.key, quest.declaration.name });
        }
    }
    return choices.size() > 2 ? choices : std::vector<ModQuestWorld>();
}

std::string ModQuests_GetTitleChoice() {
    return CVarGetString(CVAR_TITLE_WORLD, "");
}

void ModQuests_SetTitleChoice(const std::string& key) {
    CVarSetString(CVAR_TITLE_WORLD, key.c_str());
    CVarSave();
}

extern "C" uint32_t ModQuests_Count(void) {
    return static_cast<uint32_t>(sQuests.size());
}

extern "C" const char* ModQuests_GetName(uint32_t index) {
    const ModQuest* quest = GetQuestAt(index);
    return quest != nullptr ? quest->declaration.name.c_str() : "";
}

extern "C" const char* ModQuests_GetLogo(uint32_t index) {
    const ModQuest* quest = GetQuestAt(index);
    if (quest == nullptr || quest->logoPath.empty() || !ResourceMgr_FileExists(quest->declaration.logo.c_str())) {
        return nullptr;
    }
    return quest->logoPath.c_str();
}

extern "C" bool ModQuests_CanOpenFile(const char* fileQuest) {
    return fileQuest == nullptr || fileQuest[0] == '\0' || FindQuest(fileQuest) != nullptr;
}

extern "C" const char* ModQuests_GetFileLabel(const char* fileQuest) {
    if (fileQuest == nullptr || fileQuest[0] == '\0') {
        return nullptr;
    }
    const ModQuest* quest = FindQuest(fileQuest);
    return quest != nullptr ? quest->declaration.label.c_str() : "MOD";
}

extern "C" void ModQuests_ChooseModQuest(uint32_t index) {
    sChosen = GetQuestAt(index);
}

extern "C" void ModQuests_ChooseGameQuest(void) {
    sChosen = nullptr;
    ModQuests_ShowGameWorld();
}

// Runs in Sram_InitSave after the save is reset, so the new file's world is on top before its first save.
extern "C" void ModQuests_StartNewFile(bool isModQuest) {
    if (isModQuest && sChosen != nullptr) {
        gSaveContext.ship.quest.id = QUEST_NORMAL;
        SetActive(sChosen->declaration.key);
        SPDLOG_INFO("[ModQuests] New file {} is '{}'", gSaveContext.fileNum + 1, sActive);
    }
    EnterFileWorld();
}

extern "C" void ModQuests_LeaveFile(void) {
    sActive[0] = '\0';
    RefreshAwakeQuests();
}

extern "C" void ModQuests_ShowTitleWorld(void) {
    ModQuests_LeaveFile();
    SetWorld(GetTitleWorld());
}

extern "C" void ModQuests_ShowGameWorld(void) {
    SetWorld("");
}

extern "C" const char* ModQuests_GetActive(void) {
    return sActive[0] != '\0' ? sActive : nullptr;
}

extern "C" bool ModQuests_IsPathAsleep(const char* path) {
    if (path == nullptr || sQuests.empty()) {
        return false;
    }
    bool isInSleepingWorld = false;
    auto mounted = GetArchiveManager()->GetArchives();
    for (const auto& archive : *mounted) {
        if (!archive->HasFile(path)) {
            continue;
        }
        const ModQuest* quest = FindQuestOfArchive(archive);
        if (quest == nullptr || quest->isOnTop) {
            return false;
        }
        isInSleepingWorld = true;
    }
    return isInSleepingWorld;
}
