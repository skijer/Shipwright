#pragma once
#include "z64.h"

#ifdef __cplusplus
#include <cstdint>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class SceneDB {
  public:
    static SceneDB* Instance;

    // Clear of SCENE_ID_MAX (0x6E), which the vanilla entrance table uses as an "unused / no scene" sentinel.
    static constexpr int32_t CUSTOM_SCENE_ID_BASE = 0x80;

    struct Horse {
        bool allowed = false;
        bool hasSpawn = false;
        Vec3f pos = { 0.0f, 0.0f, 0.0f };
        int16_t angle = 0;
    };

    struct Entry {
        int32_t id = -1;
        bool valid = false;
        bool isCustom = false;
        std::string name;
        std::string displayName;
        std::string sceneFileName;
        std::string scenePath;
        std::string titleCardTexture;
        uint8_t drawConfig = 0;
        Horse horse;
    };

    struct EntranceEntry {
        std::string name;
        int32_t index;
        int32_t sceneId;
    };

    struct CustomSceneInit {
        std::string name;
        std::string displayName;
        std::string scenePath;
        std::string titleCardTexture;
        uint8_t drawConfig = 0;
        Horse horse;
    };

    struct CustomEntranceInit {
        std::string name;
        int32_t sceneId = -1;
        int8_t spawn = 0;
        bool continueBgm = false;
        bool displayTitleCard = false;
        uint8_t endTransType = 2;
        uint8_t startTransType = 2;
    };

    SceneDB();

    Entry& AddCustomScene(const CustomSceneInit& init);
    int32_t AddCustomEntrance(const CustomEntranceInit& init);

    Entry& RetrieveEntry(int32_t id);
    const Entry& RetrieveEntry(int32_t id) const;
    int32_t RetrieveId(const std::string& name) const;
    size_t GetEntryCount() const;
    const std::vector<Entry>& Entries() const;

    int32_t RetrieveEntranceIndex(const std::string& name) const;
    const std::string& RetrieveEntranceName(int32_t index) const;
    size_t GetEntranceCount() const;
    const std::vector<EntranceEntry>& CustomEntrances() const;

    std::string GetScenePath(int32_t id) const;
    std::string GetScenePath(int32_t id, bool masterQuest) const;

    void LoadCustomScenes();
    bool HasUnboundBase() const;

  private:
    void SeedVanillaScenes();
    void SeedVanillaDisplayNames();
    void SeedVanillaEntrances();
    void RefreshEntranceTablePointer();
    void AddEntranceLayerGroup(int32_t index, const EntranceInfo& info);
    bool RegisterScene(const std::string& id, const nlohmann::json& def);
    void RegisterEntrance(const Entry& scene, const std::string& key, const nlohmann::json& def);

    std::vector<Entry> db;
    std::unordered_map<std::string, int32_t> nameTable;
    int32_t nextSceneId = CUSTOM_SCENE_ID_BASE;
    bool unboundBase = false;

    std::vector<EntranceInfo> entranceTable;
    std::unordered_map<std::string, int32_t> entranceNameTable;
    std::vector<EntranceEntry> customEntrances;
    int32_t nextEntranceIndex = 0;
};

class SaveManager;
void SceneDB_RegisterSaveFunctions(SaveManager& saveManager);

extern "C" {
#endif

int32_t SceneDB_IsValid(int32_t id);
int32_t SceneDB_IsCustom(int32_t id);
int32_t SceneDB_GetEntryCount(void);
uint8_t SceneDB_GetDrawConfig(int32_t id);
const char* SceneDB_GetDisplayName(int32_t id);
const char* SceneDB_GetTitleCardTexture(int32_t id);

int32_t SceneDB_HorseAllowed(int32_t id);
int32_t SceneDB_GetHorseSpawn(int32_t id, Vec3f* pos, int16_t* angle);
int32_t EntranceDB_GetEntryCount(void);
int32_t EntranceDB_RetrieveIndex(const char* name);
const char* EntranceDB_RetrieveName(int32_t index);

SavedSceneFlags* SceneFlags_Get(int32_t sceneNum);

// Room-keyed flags for rooms >= 32, which do not fit the u32 masks in SavedSceneFlags/ActorContext.
typedef enum SceneFlagsExtKind {
    SCENE_FLAGS_EXT_CLEAR,
    SCENE_FLAGS_EXT_TEMP_CLEAR,
} SceneFlagsExtKind;
int32_t SceneFlagsExt_Get(int32_t sceneNum, SceneFlagsExtKind kind, int32_t bit);
void SceneFlagsExt_Set(int32_t sceneNum, SceneFlagsExtKind kind, int32_t bit);
void SceneFlagsExt_Unset(int32_t sceneNum, SceneFlagsExtKind kind, int32_t bit);
void SceneFlagsExt_LoadClear(int32_t sceneNum);
void SceneFlagsExt_SaveClear(int32_t sceneNum);

#ifdef __cplusplus
}
#endif
