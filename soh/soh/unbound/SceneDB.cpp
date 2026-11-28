#include "SceneDB.h"
#include "global.h" // gSaveContext

#include <libultraship/libultraship.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <spdlog/spdlog.h>

#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/SaveManager.h"
#include "soh/unbound/UnboundJson.h"
#include "soh/unbound/UnboundSchema.h"
#include "soh/util.h"

extern "C" {
EntranceInfo* gEntranceTable = nullptr;
}

SceneDB* SceneDB::Instance = new SceneDB();

namespace {
// Scene ids and entrance indices travel in s16 engine fields (EntranceInfo.scene, exit lists).
constexpr int64_t kMaxSceneId = INT16_MAX;

struct VanillaScene {
    const char* fileName;
    const char* enumName;
    uint8_t drawConfig;
};

#define none ""
#define DEFINE_SCENE(name, title, enumValue, config, unk_10, unk_12) { #name, #enumValue, config },
const VanillaScene sVanillaScenes[] = {
#include "tables/scene_table.h"
};
#undef DEFINE_SCENE
#undef none

constexpr int16_t sVanillaHorseScenes[] = { SCENE_HYRULE_FIELD, SCENE_LAKE_HYLIA, SCENE_GERUDO_VALLEY,
                                            SCENE_GERUDOS_FORTRESS, SCENE_LON_LON_RANCH };

struct VanillaEntrance {
    const char* name;
    int16_t scene;
    int8_t spawn;
    bool continueBgm;
    bool displayTitleCard;
    uint8_t endTransType;
    uint8_t startTransType;
};

#define DEFINE_ENTRANCE(enumValue, sceneId, spawn, continueBgm, displayTitleCard, endTransType, startTransType) \
    { #enumValue, sceneId, spawn, continueBgm, displayTitleCard, endTransType, startTransType },
const VanillaEntrance sVanillaEntrances[] = {
#include "tables/entrance_table.h"
};
#undef DEFINE_ENTRANCE

const char* const sReturnEntranceNames[] = {
    "ENTR_RETURN_YOUSEI_IZUMI_YOKO", "ENTR_RETURN_SYATEKIJYOU", "ENTR_RETURN_2", "ENTR_RETURN_SHOP1", "ENTR_RETURN_4",
    "ENTR_RETURN_DAIYOUSEI_IZUMI",   "ENTR_RETURN_GROTTO",
};
static_assert(std::size(sReturnEntranceNames) == ENTR_RETURN_GROTTO - ENTR_RETURN_YOUSEI_IZUMI_YOKO + 1);

uint16_t PackEntranceField(bool continueBgm, bool displayTitleCard, uint8_t endTransType, uint8_t startTransType) {
    return (continueBgm ? ENTRANCE_INFO_CONTINUE_BGM_FLAG : 0) |
           (displayTitleCard ? ENTRANCE_INFO_DISPLAY_TITLE_CARD_FLAG : 0) |
           ((endTransType << ENTRANCE_INFO_END_TRANS_TYPE_SHIFT) & ENTRANCE_INFO_END_TRANS_TYPE_MASK) |
           ((startTransType << ENTRANCE_INFO_START_TRANS_TYPE_SHIFT) & ENTRANCE_INFO_START_TRANS_TYPE_MASK);
}

constexpr int32_t kEntranceLayerCount = 4;
// z_player.c reads any exit value from ENTR_RETURN_YOUSEI_IZUMI_YOKO (0x7FF9) up as a dynamic return entrance before
// it indexes the table, so a group reaching into that range would hand out entrances no exit could name.
constexpr int64_t kMaxEntranceIndex =
    (ENTR_RETURN_YOUSEI_IZUMI_YOKO - kEntranceLayerCount) / kEntranceLayerCount * kEntranceLayerCount;

std::unordered_map<int32_t, SavedSceneFlags> sCustomSceneFlags;

// Clear flags are staged like ActorContext.flags: SceneFlagsExt_LoadClear copies the persisted bits into the live map
// on scene init and SceneFlagsExt_SaveClear commits them back, so a game over discards unsaved flags.
using ExtBitset = std::vector<uint32_t>;
std::unordered_map<int32_t, ExtBitset> sExtClearFlags;
std::unordered_map<int32_t, ExtBitset> sExtLiveClearFlags;
std::unordered_map<int32_t, ExtBitset> sExtTempClearFlags;

std::unordered_map<int32_t, ExtBitset>& ExtFlagMap(SceneFlagsExtKind kind) {
    return kind == SCENE_FLAGS_EXT_TEMP_CLEAR ? sExtTempClearFlags : sExtLiveClearFlags;
}

bool ExtBitTest(const ExtBitset& bits, int32_t bit) {
    size_t word = (size_t)bit / 32;
    return word < bits.size() && (bits[word] & (1u << (bit % 32)));
}

void ExtBitWrite(ExtBitset& bits, int32_t bit, bool value) {
    size_t word = (size_t)bit / 32;
    if (word >= bits.size()) {
        if (!value) {
            return;
        }
        bits.resize(word + 1, 0);
    }
    if (value) {
        bits[word] |= (1u << (bit % 32));
    } else {
        bits[word] &= ~(1u << (bit % 32));
    }
}

} // namespace

SceneDB::SceneDB() {
    SeedVanillaScenes();
    SeedVanillaEntrances();
}

void SceneDB::SeedVanillaScenes() {
    db.resize(SCENE_ID_MAX);
    for (int32_t id = 0; id < SCENE_ID_MAX; id++) {
        Entry& entry = db[id];
        entry.id = id;
        entry.valid = true;
        entry.isCustom = false;
        entry.name = sVanillaScenes[id].enumName;
        entry.displayName = entry.name;
        entry.sceneFileName = sVanillaScenes[id].fileName;
        entry.drawConfig = sVanillaScenes[id].drawConfig;
        entry.horse.allowed = std::find(std::begin(sVanillaHorseScenes), std::end(sVanillaHorseScenes), id) !=
                              std::end(sVanillaHorseScenes);
        nameTable[entry.name] = id;
    }
}

// The constructor runs before SohUtils' name table is guaranteed to exist, so names are filled in from
// LoadCustomScenes.
void SceneDB::SeedVanillaDisplayNames() {
    for (int32_t id = 0; id < SCENE_ID_MAX; id++) {
        db[id].displayName = SohUtils::GetSceneName(id);
    }
}

void SceneDB::SeedVanillaEntrances() {
    entranceTable.reserve(ENTR_MAX + 256);
    for (int32_t index = 0; index < ENTR_MAX; index++) {
        const VanillaEntrance& v = sVanillaEntrances[index];
        EntranceInfo info;
        info.scene = v.scene;
        info.spawn = v.spawn;
        info.field = PackEntranceField(v.continueBgm, v.displayTitleCard, v.endTransType, v.startTransType);
        entranceTable.push_back(info);
        entranceNameTable[v.name] = index;
    }
    for (int32_t index = ENTR_RETURN_YOUSEI_IZUMI_YOKO; index <= ENTR_RETURN_GROTTO; index++) {
        entranceNameTable[sReturnEntranceNames[index - ENTR_RETURN_YOUSEI_IZUMI_YOKO]] = index;
    }
    nextEntranceIndex = (ENTR_MAX + kEntranceLayerCount - 1) / kEntranceLayerCount * kEntranceLayerCount;
    RefreshEntranceTablePointer();
}

void SceneDB::RefreshEntranceTablePointer() {
    gEntranceTable = entranceTable.data();
}

SceneDB::Entry& SceneDB::AddCustomScene(const CustomSceneInit& init) {
    static Entry invalid;

    if (init.name.empty() || nameTable.contains(init.name)) {
        SPDLOG_ERROR("[Unbound] scene '{}' is unnamed or already registered", init.name);
        return invalid;
    }

    int32_t id = nextSceneId;
    if (id > kMaxSceneId) {
        SPDLOG_ERROR("[Unbound] scene '{}' has no free scene id left; {} is the last one", init.name, kMaxSceneId);
        return invalid;
    }
    if (init.drawConfig >= SDC_MAX) {
        SPDLOG_ERROR("[Unbound] scene '{}' requests draw config {} (max {})", init.name, init.drawConfig, SDC_MAX - 1);
        return invalid;
    }

    if (id >= (int32_t)db.size()) {
        db.resize(id + 1);
    }
    Entry& entry = db[id];
    entry.id = id;
    entry.valid = true;
    entry.isCustom = true;
    entry.name = init.name;
    entry.displayName = init.displayName.empty() ? init.name : init.displayName;
    entry.scenePath = init.scenePath;
    entry.titleCardTexture = init.titleCardTexture;
    entry.drawConfig = init.drawConfig;
    entry.horse = init.horse;
    nameTable[entry.name] = id;
    nextSceneId = std::max(nextSceneId, id + 1);
    return entry;
}

void SceneDB::AddEntranceLayerGroup(int32_t index, const EntranceInfo& info) {
    if ((int32_t)entranceTable.size() < index + kEntranceLayerCount) {
        EntranceInfo unused = { SCENE_ID_MAX, 0, 0 };
        entranceTable.resize(index + kEntranceLayerCount, unused);
    }
    for (int32_t layer = 0; layer < kEntranceLayerCount; layer++) {
        entranceTable[index + layer] = info;
    }
    RefreshEntranceTablePointer();
}

int32_t SceneDB::AddCustomEntrance(const CustomEntranceInit& init) {
    if (init.name.empty() || entranceNameTable.contains(init.name)) {
        SPDLOG_ERROR("[Unbound] entrance '{}' is unnamed or already registered", init.name);
        return -1;
    }
    if (init.sceneId < 0 || init.sceneId >= (int32_t)db.size() || !db[init.sceneId].valid) {
        SPDLOG_ERROR("[Unbound] entrance '{}' targets unknown scene id {}", init.name, init.sceneId);
        return -1;
    }

    int32_t index = nextEntranceIndex;
    if (index > kMaxEntranceIndex) {
        SPDLOG_ERROR("[Unbound] entrance '{}' has no free entrance-table group left; {:#x} is the last one", init.name,
                     kMaxEntranceIndex);
        return -1;
    }

    EntranceInfo info;
    info.scene = (s16)init.sceneId;
    info.spawn = init.spawn;
    info.field = PackEntranceField(init.continueBgm, init.displayTitleCard, init.endTransType, init.startTransType);
    AddEntranceLayerGroup(index, info);

    entranceNameTable[init.name] = index;
    customEntrances.push_back({ init.name, index, init.sceneId });
    nextEntranceIndex = std::max(nextEntranceIndex, index + kEntranceLayerCount);
    return index;
}

SceneDB::Entry& SceneDB::RetrieveEntry(int32_t id) {
    static Entry invalid;
    if (id < 0 || id >= (int32_t)db.size() || !db[id].valid) {
        return invalid;
    }
    return db[id];
}

const SceneDB::Entry& SceneDB::RetrieveEntry(int32_t id) const {
    return const_cast<SceneDB*>(this)->RetrieveEntry(id);
}

int32_t SceneDB::RetrieveId(const std::string& name) const {
    auto it = nameTable.find(name);
    return it == nameTable.end() ? -1 : it->second;
}

size_t SceneDB::GetEntryCount() const {
    return db.size();
}

const std::vector<SceneDB::Entry>& SceneDB::Entries() const {
    return db;
}

int32_t SceneDB::RetrieveEntranceIndex(const std::string& name) const {
    auto it = entranceNameTable.find(name);
    return it == entranceNameTable.end() ? -1 : it->second;
}

const std::string& SceneDB::RetrieveEntranceName(int32_t index) const {
    static const std::string none;
    int32_t group = index - index % kEntranceLayerCount;
    for (const EntranceEntry& entrance : customEntrances) {
        if (entrance.index == group) {
            return entrance.name;
        }
    }
    return none;
}

size_t SceneDB::GetEntranceCount() const {
    return entranceTable.size();
}

const std::vector<SceneDB::EntranceEntry>& SceneDB::CustomEntrances() const {
    return customEntrances;
}

std::string SceneDB::GetScenePath(int32_t id) const {
    return GetScenePath(id, ResourceMgr_IsGameMasterQuest());
}

std::string SceneDB::GetScenePath(int32_t id, bool masterQuest) const {
    const Entry& entry = RetrieveEntry(id);
    if (!entry.valid) {
        return "";
    }
    if (entry.isCustom) {
        return entry.scenePath;
    }

    bool hasMqVariant = (id >= SCENE_DEKU_TREE && id <= SCENE_ICE_CAVERN) || id == SCENE_GERUDO_TRAINING_GROUND ||
                        id == SCENE_INSIDE_GANONS_CASTLE;
    bool useMq = hasMqVariant && masterQuest;
    std::string legacyFolder =
        std::string(hasMqVariant ? (useMq ? "mq" : "nonmq") : "shared") + "/" + entry.sceneFileName;
    if (unboundBase && !legacySceneOverrides.contains(legacyFolder)) {
        std::string dir = entry.sceneFileName;
        const std::string suffix = "_scene";
        if (dir.ends_with(suffix)) {
            dir.resize(dir.size() - suffix.size());
        }
        return "scenes/" + dir + (useMq ? "_mq" : "") + "/scene.json";
    }
    return "scenes/" + legacyFolder + "/" + entry.sceneFileName;
}

bool SceneDB::HasUnboundBase() const {
    return unboundBase;
}

namespace {

namespace K = SOH::Unbound::Schema;
using SOH::Unbound::Field;
using SOH::Unbound::Json;

void WarnNumberIgnored(const Json& def, const char* key, const std::string& owner) {
    if (def.contains(key)) {
        SPDLOG_WARN("[Unbound] '{}': \"{}\" is deprecated and ignored; the game assigns the number and "
                    "everything addresses this by name (SPEC.md §7)",
                    owner, key);
    }
}
constexpr const char* kFeatureScenes = "scenes";

bool ManifestVersionIsReadable(const Json& doc) {
    int64_t version = Field(doc, K::kFormatVersion, K::kCurrentFormatVersion);
    int64_t required = Field(SOH::Unbound::Sub(doc, K::kRequires), K::kFormatVersion, version);
    if (version != K::kCurrentFormatVersion || required != K::kCurrentFormatVersion) {
        SPDLOG_ERROR("[Unbound] {}: format version {} (requires {}); this build reads only {}; layer ignored",
                     K::kManifestPath, version, required, K::kCurrentFormatVersion);
        return false;
    }
    return true;
}

bool ManifestProvidesScenes(const Json& doc) {
    for (const auto& feature : SOH::Unbound::SubArray(doc, K::kFeatures)) {
        if (feature.is_string() && feature.get<std::string>() == kFeatureScenes) {
            return true;
        }
    }
    return false;
}

bool DetectUnboundBase() {
    auto archiveManager = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    bool base = false;
    for (const auto& file : archiveManager->LoadFileFromAllLayers(K::kManifestPath)) {
        Json doc;
        try {
            doc = Json::parse(file->Buffer->begin(), file->Buffer->end(), nullptr, true, true);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Unbound] {}: invalid JSON: {}", K::kManifestPath, e.what());
            continue;
        }
        if (!doc.is_object()) {
            SPDLOG_ERROR("[Unbound] {}: manifest is not a JSON object", K::kManifestPath);
            continue;
        }
        if (ManifestVersionIsReadable(doc) && ManifestProvidesScenes(doc)) {
            base = true;
        }
    }
    return base;
}

bool IsRoomSuffix(const std::string& suffix) {
    const std::string marker = "_room_";
    return suffix.size() > marker.size() && suffix.starts_with(marker) &&
           std::all_of(suffix.begin() + marker.size(), suffix.end(),
                       [](unsigned char c) { return std::isdigit(c) != 0; });
}

// "scenes/shared/spot04_scene/spot04_room_0" -> "shared/spot04_scene". Display lists and textures keep their
// vanilla names, so scene.json picks a mod's copies up anyway: only scene and room headers count.
std::string LegacySceneFolderOfHeader(const std::string& path) {
    const std::string prefix = "scenes/";
    size_t versionEnd = path.find('/', prefix.size());
    size_t folderEnd = versionEnd == std::string::npos ? std::string::npos : path.find('/', versionEnd + 1);
    if (!path.starts_with(prefix) || folderEnd == std::string::npos) {
        return "";
    }
    std::string folder = path.substr(versionEnd + 1, folderEnd - versionEnd - 1);
    std::string leaf = path.substr(folderEnd + 1);
    const std::string sceneSuffix = "_scene";
    if (!folder.ends_with(sceneSuffix) || !leaf.starts_with(folder.substr(0, folder.size() - sceneSuffix.size()))) {
        return "";
    }
    std::string headerSuffix = leaf.substr(folder.size() - sceneSuffix.size());
    if (headerSuffix != sceneSuffix && !IsRoomSuffix(headerSuffix)) {
        return "";
    }
    return path.substr(prefix.size(), folderEnd - prefix.size());
}

SceneDB::Horse ReadHorse(const std::string& id, const Json& def) {
    SceneDB::Horse horse;
    if (!def.contains(K::kHorse)) {
        return horse;
    }
    const Json& value = def[K::kHorse];
    if (!value.is_object()) {
        horse.allowed = SOH::Unbound::ToInt(value) != 0;
        return horse;
    }
    horse.allowed = true;
    horse.angle = (int16_t)Field(value, K::kAngle);
    if (value.contains(K::kPos)) {
        const Json& pos = value[K::kPos];
        if (!pos.is_array() || pos.size() < 3) {
            SPDLOG_ERROR("[Unbound] scene '{}': \"{}\".\"{}\" is not an [x, y, z] array; ignoring it", id, K::kHorse,
                         K::kPos);
        } else {
            horse.pos = SOH::Unbound::ReadVec3f(pos);
            horse.hasSpawn = true;
        }
    }
    return horse;
}

} // namespace

void SceneDB::LoadCustomScenes() {
    SeedVanillaDisplayNames();
    unboundBase = DetectUnboundBase();
    if (unboundBase) {
        SPDLOG_INFO("[Unbound] Unbound-format archive mounted; vanilla scenes load from scene.json");
        FindLegacySceneOverrides();
    }

    size_t loaded = SOH::Unbound::ForEachRegistryEntry(
        K::kRegistryPath, "scene",
        [this](const std::string& id, const nlohmann::json& def) { return RegisterScene(id, def); });
    if (loaded == 0 && customEntrances.empty()) {
        return;
    }
    SPDLOG_INFO("[Unbound] {}: registered {} custom scene(s), {} custom entrance(s)", K::kRegistryPath, loaded,
                customEntrances.size());
}

// The base was converted before any mod was mounted, so its scene.json would hide a legacy mod's scene and room
// headers: such a scene loads from the legacy files instead. Assumes the mods are already mounted.
void SceneDB::FindLegacySceneOverrides() {
    legacySceneOverrides.clear();
    auto archiveManager = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    auto archives = archiveManager->GetArchives();
    auto base = std::find_if(archives->begin(), archives->end(),
                             [](const auto& archive) { return archive->HasFile(K::kManifestPath); });
    if (base == archives->end()) {
        return;
    }
    for (auto archive = std::next(base); archive != archives->end(); ++archive) {
        for (const auto& [hash, path] : *(*archive)->ListFiles("scenes/*")) {
            std::string folder = LegacySceneFolderOfHeader(path);
            if (folder.empty() || legacySceneOverrides.contains(folder)) {
                continue;
            }
            std::string sceneHeader = "scenes/" + folder + "/" + folder.substr(folder.find('/') + 1);
            // A standalone base consumed the ROM's legacy headers: rooms the mod leaves out would be missing.
            bool hasVanillaHeaders = std::any_of(archives->begin(), base,
                                                 [&sceneHeader](const auto& rom) { return rom->HasFile(sceneHeader); });
            if (!hasVanillaHeaders) {
                SPDLOG_WARN("[Unbound] {} replaces {}, but no ROM archive holds its legacy scene; scene.json is kept",
                            (*archive)->GetPath(), path);
                continue;
            }
            SPDLOG_INFO("[Unbound] {} replaces scenes/{}; that scene loads from its legacy files",
                        (*archive)->GetPath(), folder);
            legacySceneOverrides.insert(folder);
        }
    }
}

bool SceneDB::RegisterScene(const std::string& id, const nlohmann::json& def) {
    CustomSceneInit scene;
    scene.name = id;
    scene.displayName = def.contains(K::kName) && def[K::kName].is_string() ? def[K::kName].get<std::string>() : id;
    scene.scenePath = SOH::Unbound::PathField(def, K::kScene);
    scene.titleCardTexture = SOH::Unbound::PathField(def, K::kTitleCardTexture);
    int64_t drawConfig = Field(def, K::kDrawConfig);
    if (scene.scenePath.empty()) {
        SPDLOG_ERROR("[Unbound] {}: scene '{}' has no \"{}\" path", K::kRegistryPath, id, K::kScene);
        return false;
    }
    WarnNumberIgnored(def, K::kSceneId, id);
    if (drawConfig < 0 || drawConfig >= SDC_MAX) {
        SPDLOG_ERROR("[Unbound] scene '{}' requests draw config {} (max {})", id, drawConfig, SDC_MAX - 1);
        return false;
    }
    scene.drawConfig = (uint8_t)drawConfig;
    scene.horse = ReadHorse(id, def);

    Entry& entry = AddCustomScene(scene);
    if (!entry.valid) {
        return false;
    }
    const Json& entrances = SOH::Unbound::Sub(def, K::kEntrances);
    for (const auto& key : SOH::Unbound::ListKeys(entrances)) {
        if (entrances[key].is_object()) {
            RegisterEntrance(entry, key, entrances[key]);
        }
    }
    SPDLOG_INFO("[Unbound] scene '{}' -> id {:#x}", entry.name, entry.id);
    return true;
}

void SceneDB::RegisterEntrance(const Entry& scene, const std::string& key, const nlohmann::json& def) {
    if (def.contains(K::kLayers)) {
        SPDLOG_WARN("[Unbound] {}/{}: \"{}\" is reserved and not read yet; all four layers are identical", scene.name,
                    key, K::kLayers);
    }
    CustomEntranceInit entrance;
    entrance.name = scene.name + "/" + key;
    WarnNumberIgnored(def, K::kIndex, entrance.name);
    entrance.sceneId = scene.id;
    entrance.spawn = (int8_t)Field(def, K::kSpawn);
    entrance.continueBgm = Field(def, K::kContinueBgm) != 0;
    entrance.displayTitleCard = Field(def, K::kShowTitleCard) != 0;
    entrance.endTransType = (uint8_t)Field(def, K::kEndTransition, 2);
    entrance.startTransType = (uint8_t)Field(def, K::kStartTransition, 2);
    AddCustomEntrance(entrance);
}

namespace {

// Reads the snapshot SaveSection took rather than gSaveContext: the write runs on a worker thread, and a name that
// disagreed with the id the "base" section wrote from the same snapshot would win on load.
void SaveHorseScene(const SaveContext* saveContext) {
    const SceneDB::Entry& entry = SceneDB::Instance->RetrieveEntry(saveContext->horseData.scene);
    SaveManager::Instance->SaveData("horseScene", entry.valid && entry.isCustom ? entry.name : std::string());
}

// Save sections live in a std::map keyed by name, so "base" is always read before "unbound".
void LoadHorseScene() {
    std::string name;
    SaveManager::Instance->LoadData("horseScene", name);
    if (name.empty()) {
        return;
    }
    int32_t id = SceneDB::Instance->RetrieveId(name);
    if (id < 0) {
        SPDLOG_WARN("[Unbound] the horse is parked in scene '{}', which is not registered", name);
        return;
    }
    gSaveContext.horseData.scene = (s16)id;
}

constexpr const char* kSavedEntranceName = "name";
constexpr const char* kSavedEntranceLayer = "layer";

// Reads the snapshot SaveSection took rather than gSaveContext (the write runs on a worker thread). Writes the key
// even for a vanilla entrance, with an empty name: SaveManager keeps one json block for the whole session, so a
// skipped key would keep a stale custom entrance name that wins on load.
void SaveEntranceName(const char* key, int32_t entranceIndex) {
    const std::string& name = SceneDB::Instance->RetrieveEntranceName(entranceIndex);
    SaveManager::Instance->SaveStruct(key, [&]() {
        SaveManager::Instance->SaveData(kSavedEntranceName, name);
        SaveManager::Instance->SaveData(kSavedEntranceLayer, name.empty() ? 0 : entranceIndex % kEntranceLayerCount);
    });
}

struct SavedEntrance {
    bool present = false;
    std::string name;
    int32_t index = -1;
};

SavedEntrance LoadEntranceName(const char* key) {
    SavedEntrance saved;
    int32_t layer = 0;
    SaveManager::Instance->LoadStruct(key, [&]() {
        SaveManager::Instance->LoadData(kSavedEntranceName, saved.name);
        SaveManager::Instance->LoadData(kSavedEntranceLayer, layer);
    });
    if (saved.name.empty()) {
        return saved;
    }
    saved.present = true;
    if (layer < 0 || layer >= kEntranceLayerCount) {
        SPDLOG_WARN("[Unbound] the save's entrance '{}' has layer {}; using 0", saved.name, layer);
        layer = 0;
    }
    int32_t group = SceneDB::Instance->RetrieveEntranceIndex(saved.name);
    if (group >= 0) {
        saved.index = group + layer;
    }
    return saved;
}

// Matches Sram_OpenSave's default branch, which special-cases SCENE_LINKS_HOUSE: naming it for an adult would send
// them to the child spawn.
int32_t DefaultSpawnEntrance() {
    return LINK_AGE_IN_YEARS == YEARS_CHILD ? ENTR_LINKS_HOUSE_CHILD_SPAWN : ENTR_TEMPLE_OF_TIME_WARP_PAD;
}

int32_t DefaultSpawnScene() {
    return LINK_AGE_IN_YEARS == YEARS_CHILD ? SCENE_LINKS_HOUSE : SCENE_TEMPLE_OF_TIME;
}

// Runs before Sram_OpenSave picks the spawn, so the number it sees is already the resolved one.
void LoadEntranceIndex() {
    SavedEntrance saved = LoadEntranceName("entrance");
    if (!saved.present) {
        return;
    }
    if (saved.index < 0) {
        SPDLOG_WARN("[Unbound] the save is at entrance '{}', which is not registered; spawning at the default",
                    saved.name);
        gSaveContext.entranceIndex = DefaultSpawnEntrance();
        return;
    }
    gSaveContext.entranceIndex = saved.index;
}

void LoadFWEntrance(const char* key, FaroresWindData& fw) {
    SavedEntrance saved = LoadEntranceName(key);
    if (!saved.present) {
        return;
    }
    if (saved.index < 0) {
        SPDLOG_WARN("[Unbound] the Farore's Wind warp is at entrance '{}', which is not registered; clearing it",
                    saved.name);
        fw.set = 0;
        return;
    }
    fw.entranceIndex = saved.index;
}

void SaveSavedSceneName(const SaveContext* saveContext) {
    const SceneDB::Entry& entry = SceneDB::Instance->RetrieveEntry(saveContext->savedSceneNum);
    SaveManager::Instance->SaveData("savedScene", entry.valid && entry.isCustom ? entry.name : std::string());
}

void LoadSavedSceneName() {
    std::string name;
    SaveManager::Instance->LoadData("savedScene", name);
    if (name.empty()) {
        return;
    }
    int32_t id = SceneDB::Instance->RetrieveId(name);
    if (id < 0) {
        SPDLOG_WARN("[Unbound] the save is in scene '{}', which is not registered", name);
        gSaveContext.savedSceneNum = (s16)DefaultSpawnScene();
        return;
    }
    gSaveContext.savedSceneNum = (s16)id;
}

void SaveUnboundSection(const SaveContext& saveContext, int sectionID, bool fullSave) {
    SaveHorseScene(&saveContext);
    SaveSavedSceneName(&saveContext);
    SaveEntranceName("entrance", saveContext.entranceIndex);
    SaveEntranceName("fwEntrance", saveContext.fw.entranceIndex);
    SaveEntranceName("backupFwEntrance", saveContext.ship.backupFW.entranceIndex);
    SaveManager::Instance->SaveStruct("sceneFlags", []() {
        for (const auto& [id, flags] : sCustomSceneFlags) {
            const SceneDB::Entry& entry = SceneDB::Instance->RetrieveEntry(id);
            if (!entry.valid) {
                continue;
            }
            SaveManager::Instance->SaveStruct(entry.name, [&flags]() {
                SaveManager::Instance->SaveData("chest", flags.chest);
                SaveManager::Instance->SaveData("swch", flags.swch);
                SaveManager::Instance->SaveData("clear", flags.clear);
                SaveManager::Instance->SaveData("collect", flags.collect);
                SaveManager::Instance->SaveData("unk", flags.unk);
                SaveManager::Instance->SaveData("rooms", flags.rooms);
                SaveManager::Instance->SaveData("floors", flags.floors);
            });
        }
    });
    SaveManager::Instance->SaveStruct("roomClearExt", []() {
        for (const auto& [id, bits] : sExtClearFlags) {
            const SceneDB::Entry& entry = SceneDB::Instance->RetrieveEntry(id);
            if (!entry.valid || bits.empty()) {
                continue;
            }
            SaveManager::Instance->SaveStruct(entry.name, [&bits]() {
                SaveManager::Instance->SaveData("words", (uint32_t)bits.size());
                SaveManager::Instance->SaveArray("bits", bits.size(),
                                                 [&bits](size_t i) { SaveManager::Instance->SaveData("", bits[i]); });
            });
        }
    });
}

void LoadExtClearFlags() {
    SaveManager::Instance->LoadStruct("roomClearExt", []() {
        for (const auto& entry : SceneDB::Instance->Entries()) {
            if (!entry.valid) {
                continue;
            }
            ExtBitset bits;
            SaveManager::Instance->LoadStruct(entry.name, [&bits]() {
                uint32_t words = 0;
                SaveManager::Instance->LoadData("words", words);
                bits.assign(words, 0);
                SaveManager::Instance->LoadArray("bits", words,
                                                 [&bits](size_t i) { SaveManager::Instance->LoadData("", bits[i]); });
            });
            if (!bits.empty()) {
                sExtClearFlags[entry.id] = std::move(bits);
            }
        }
    });
}

void LoadUnboundSection() {
    SaveManager::Instance->LoadStruct("sceneFlags", []() {
        for (const auto& entry : SceneDB::Instance->Entries()) {
            if (!entry.valid || !entry.isCustom) {
                continue;
            }
            SavedSceneFlags& flags = sCustomSceneFlags[entry.id];
            SaveManager::Instance->LoadStruct(entry.name, [&flags]() {
                SaveManager::Instance->LoadData("chest", flags.chest);
                SaveManager::Instance->LoadData("swch", flags.swch);
                SaveManager::Instance->LoadData("clear", flags.clear);
                SaveManager::Instance->LoadData("collect", flags.collect);
                SaveManager::Instance->LoadData("unk", flags.unk);
                SaveManager::Instance->LoadData("rooms", flags.rooms);
                SaveManager::Instance->LoadData("floors", flags.floors);
            });
        }
    });
    LoadExtClearFlags();
    LoadHorseScene();
    LoadSavedSceneName();
    LoadEntranceIndex();
    LoadFWEntrance("fwEntrance", gSaveContext.fw);
    LoadFWEntrance("backupFwEntrance", gSaveContext.ship.backupFW);
}

void InitUnboundSection(bool isDebug) {
    sCustomSceneFlags.clear();
    sExtClearFlags.clear();
    sExtLiveClearFlags.clear();
    sExtTempClearFlags.clear();
}

} // namespace

void SceneDB_RegisterSaveFunctions(SaveManager& saveManager) {
    // SaveManager::Instance is not set yet when this runs from the SaveManager constructor; use the reference.
    saveManager.AddLoadFunction("unbound", 1, LoadUnboundSection);
    saveManager.AddSaveFunction("unbound", 1, SaveUnboundSection, true, SECTION_PARENT_NONE);
    saveManager.AddInitFunction(InitUnboundSection);
}

extern "C" int32_t SceneDB_IsValid(int32_t id) {
    return SceneDB::Instance->RetrieveEntry(id).valid;
}

extern "C" int32_t SceneDB_IsCustom(int32_t id) {
    const SceneDB::Entry& entry = SceneDB::Instance->RetrieveEntry(id);
    return entry.valid && entry.isCustom;
}

extern "C" int32_t SceneDB_HorseAllowed(int32_t id) {
    const SceneDB::Entry& entry = SceneDB::Instance->RetrieveEntry(id);
    return entry.valid && entry.horse.allowed;
}

extern "C" int32_t SceneDB_GetHorseSpawn(int32_t id, Vec3f* pos, int16_t* angle) {
    const SceneDB::Entry& entry = SceneDB::Instance->RetrieveEntry(id);
    if (!entry.valid || !entry.horse.allowed || !entry.horse.hasSpawn) {
        return false;
    }
    *pos = entry.horse.pos;
    *angle = entry.horse.angle;
    return true;
}

extern "C" int32_t SceneDB_GetEntryCount(void) {
    return (int32_t)SceneDB::Instance->GetEntryCount();
}

extern "C" uint8_t SceneDB_GetDrawConfig(int32_t id) {
    return SceneDB::Instance->RetrieveEntry(id).drawConfig;
}

extern "C" const char* SceneDB_GetDisplayName(int32_t id) {
    return SceneDB::Instance->RetrieveEntry(id).displayName.c_str();
}

extern "C" const char* SceneDB_GetTitleCardTexture(int32_t id) {
    const SceneDB::Entry& entry = SceneDB::Instance->RetrieveEntry(id);
    if (!entry.valid || entry.titleCardTexture.empty()) {
        return nullptr;
    }
    return entry.titleCardTexture.c_str();
}

extern "C" int32_t EntranceDB_GetEntryCount(void) {
    return (int32_t)SceneDB::Instance->GetEntranceCount();
}

extern "C" int32_t EntranceDB_RetrieveIndex(const char* name) {
    return SceneDB::Instance->RetrieveEntranceIndex(name);
}

extern "C" const char* EntranceDB_RetrieveName(int32_t index) {
    return SceneDB::Instance->RetrieveEntranceName(index).c_str();
}

extern "C" SavedSceneFlags* SceneFlags_Get(int32_t sceneNum) {
    if (sceneNum >= 0 && sceneNum < SCENE_ID_MAX) {
        return &gSaveContext.sceneFlags[sceneNum];
    }
    if (SceneDB::Instance->RetrieveEntry(sceneNum).valid) {
        return &sCustomSceneFlags[sceneNum];
    }
    static SavedSceneFlags scratch;
    SPDLOG_ERROR("[Unbound] SceneFlags_Get: scene id {} is not registered", sceneNum);
    scratch = SavedSceneFlags{};
    return &scratch;
}

extern "C" int32_t SceneFlagsExt_Get(int32_t sceneNum, SceneFlagsExtKind kind, int32_t bit) {
    auto& map = ExtFlagMap(kind);
    auto it = map.find(sceneNum);
    return it != map.end() && ExtBitTest(it->second, bit);
}

extern "C" void SceneFlagsExt_Set(int32_t sceneNum, SceneFlagsExtKind kind, int32_t bit) {
    ExtBitWrite(ExtFlagMap(kind)[sceneNum], bit, true);
}

extern "C" void SceneFlagsExt_Unset(int32_t sceneNum, SceneFlagsExtKind kind, int32_t bit) {
    ExtBitWrite(ExtFlagMap(kind)[sceneNum], bit, false);
}

extern "C" void SceneFlagsExt_LoadClear(int32_t sceneNum) {
    sExtLiveClearFlags.clear();
    sExtTempClearFlags.clear();
    auto it = sExtClearFlags.find(sceneNum);
    if (it != sExtClearFlags.end()) {
        sExtLiveClearFlags[sceneNum] = it->second;
    }
}

extern "C" void SceneFlagsExt_SaveClear(int32_t sceneNum) {
    auto it = sExtLiveClearFlags.find(sceneNum);
    if (it != sExtLiveClearFlags.end()) {
        sExtClearFlags[sceneNum] = it->second;
    } else {
        sExtClearFlags.erase(sceneNum);
    }
}
