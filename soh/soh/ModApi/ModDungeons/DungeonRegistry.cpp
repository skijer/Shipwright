#include "soh/ModApi/ModDungeons/DungeonRegistry.h"
#include "soh/ModApi/ModDungeons/DungeonEntrancePairs.h"

#include <algorithm>
#include <list>
#include <optional>
#include <set>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "soh/ActorDB.h"
#include "soh/SaveManager.h"
#include "soh/unbound/SceneDB.h"
#include "soh/unbound/UnboundJson.h"
#include "soh/ModApi/ModDungeons/LogicExpr.h"
#include "soh/ModApi/RandoInfCustom/RandoInfCustom.h"
#include "soh/ModApi/RandoItems/RandoItems.h"
#include "soh/ModApi/RandoOptions/RandoOptions.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/ModApi/ModDungeons/RandoEnumNames.h"
#include "soh/Enhancements/randomizer/3drando/item_pool.hpp"

extern "C" {
#include "z64.h"
#include "variables.h"
extern SaveContext gSaveContext;
}

namespace {

using SOH::Unbound::Json;

constexpr const char* RegistryPath = "unbound/dungeons.json";
// SOH [Unbound] Checks placed in vanilla regions (a new grass patch, pot or sign); see mod-sdk/docs/CHECKS.md.
constexpr const char* ChecksRegistryPath = "unbound/checks.json";
constexpr const char* CheckFlagPrefix = "check.";
constexpr int32_t NoDungeon = -1;
constexpr const char* SaveSection = "unboundDungeons";
constexpr const char* OutsideExit = "@outside";
// No vanilla scene sets this mapIndex (boss scenes subtract SCENE_DEKU_TREE_BOSS), so the dungeon the
// player is in borrows it: doors, chests and the HUD read their key count from here unchanged.
constexpr int32_t KeySlot = SCENE_DODONGOS_CAVERN_BOSS;
constexpr int32_t CheckBase = RC_MAX + 1;
constexpr int32_t EventBase = LOGIC_MAX + 1;
constexpr int32_t SceneLimit = 0xFF;

constexpr uint8_t BossKeyBit = 1 << 0;
constexpr uint8_t CompassBit = 1 << 1;
constexpr uint8_t MapBit = 1 << 2;

// Check types a vanilla region can gain. Each is recognised in game the way its vanilla checks are: by the actor,
// the scene and TWO_ACTOR_PARAMS of the actor's X and Z position (the Shuffle* identities).
struct OverworldCheckType {
    const char* name;
    RandomizerCheckType type;
    ActorID actor;
};

constexpr OverworldCheckType kOverworldCheckTypes[] = {
    { "grass", RCTYPE_GRASS, ACTOR_EN_KUSA },
    { "bush", RCTYPE_BUSH, ACTOR_EN_WOOD02 },
    { "tree", RCTYPE_TREE, ACTOR_EN_WOOD02 },
    { "pot", RCTYPE_POT, ACTOR_OBJ_TSUBO },
    { "crate", RCTYPE_CRATE, ACTOR_OBJ_KIBAKO2 },
    { "small_crate", RCTYPE_SMALL_CRATE, ACTOR_OBJ_KIBAKO },
    { "rock", RCTYPE_ROCK, ACTOR_EN_ISHI },
    { "boulder", RCTYPE_BOULDER, ACTOR_EN_ISHI },
    { "icicle", RCTYPE_ICICLE, ACTOR_BG_ICE_TURARA },
    { "red_ice", RCTYPE_RED_ICE, ACTOR_BG_ICE_SHELTER },
    { "sign", RCTYPE_SIGN, ACTOR_EN_KANBAN },
    { "wonder_item", RCTYPE_WONDER_ITEM, ACTOR_EN_WONDER_ITEM },
    { "freestanding", RCTYPE_FREESTANDING, ACTOR_EN_ITEM00 },
};

// The setting that shuffles each type, as SeedContext reads it; NoAreaVariant marks an on/off setting.
constexpr int32_t NoAreaVariant = -1;

struct TypeShuffleRule {
    RandomizerCheckType type;
    RandomizerSettingKey setting;
    int32_t dungeonsOnly;
    int32_t overworldOnly;
};

constexpr TypeShuffleRule kTypeShuffleRules[] = {
    { RCTYPE_GRASS, RSK_SHUFFLE_GRASS, RO_SHUFFLE_GRASS_DUNGEONS, RO_SHUFFLE_GRASS_OVERWORLD },
    { RCTYPE_POT, RSK_SHUFFLE_POTS, RO_SHUFFLE_POTS_DUNGEONS, RO_SHUFFLE_POTS_OVERWORLD },
    { RCTYPE_CRATE, RSK_SHUFFLE_CRATES, RO_SHUFFLE_CRATES_DUNGEONS, RO_SHUFFLE_CRATES_OVERWORLD },
    { RCTYPE_SMALL_CRATE, RSK_SHUFFLE_CRATES, RO_SHUFFLE_CRATES_DUNGEONS, RO_SHUFFLE_CRATES_OVERWORLD },
    { RCTYPE_BOULDER, RSK_SHUFFLE_BOULDERS, RO_SHUFFLE_BOULDERS_DUNGEONS, RO_SHUFFLE_BOULDERS_OVERWORLD },
    { RCTYPE_SIGN, RSK_SHUFFLE_SIGNS, RO_SHUFFLE_SIGNS_DUNGEONS, RO_SHUFFLE_SIGNS_OVERWORLD },
    { RCTYPE_WONDER_ITEM, RSK_SHUFFLE_WONDER_ITEMS, RO_SHUFFLE_WONDER_ITEMS_DUNGEONS,
      RO_SHUFFLE_WONDER_ITEMS_OVERWORLD },
    { RCTYPE_FREESTANDING, RSK_SHUFFLE_FREESTANDING, RO_SHUFFLE_FREESTANDING_DUNGEONS,
      RO_SHUFFLE_FREESTANDING_OVERWORLD },
    { RCTYPE_ROCK, RSK_SHUFFLE_ROCKS, NoAreaVariant, NoAreaVariant },
    { RCTYPE_TREE, RSK_SHUFFLE_TREES, NoAreaVariant, NoAreaVariant },
    { RCTYPE_BUSH, RSK_SHUFFLE_BUSHES, NoAreaVariant, NoAreaVariant },
    { RCTYPE_ICICLE, RSK_SHUFFLE_ICICLES, NoAreaVariant, NoAreaVariant },
    { RCTYPE_RED_ICE, RSK_SHUFFLE_RED_ICE, NoAreaVariant, NoAreaVariant },
};

struct ModCheck {
    int32_t dungeon;
    std::string vanillaText;
    std::string logicText;
    LogicExpr logic;
    Rando::Location location;
    Rando::ItemLocation itemLocation;
    int16_t scene;
    int16_t flagType;
    int16_t flag;
    int32_t vanillaRegion = RR_NONE;
    uint16_t randoInf = RANDO_INF_CUSTOM_NONE;
    std::string shuffleOption;
};

struct ModEvent {
    int32_t dungeon;
    std::string name;
    std::string logicText;
    LogicExpr logic;
};

struct ModExit {
    int32_t from;
    int32_t to;
    std::string logicText;
    LogicExpr logic;
};

struct ModRegion {
    int32_t dungeon;
    std::string name;
    int16_t scene;
    std::vector<int32_t> events;
    std::vector<int32_t> checks;
    std::vector<int32_t> exits;
};

struct ModDungeon {
    std::string key;
    std::string name;
    std::vector<int16_t> scenes;
    std::string entranceName;
    std::string exitName;
    int32_t overworldRegion = RR_NONE;
    int32_t entryRegion = RR_NONE;
    std::string entranceLogicText;
    LogicExpr entranceLogic;
    uint8_t smallKeyCount = 0;
    RandomizerGet smallKey = RG_NONE;
    RandomizerGet bossKey = RG_NONE;
    RandomizerGet map = RG_NONE;
    RandomizerGet compass = RG_NONE;
    int8_t heldKeys = -1;
    uint8_t receivedKeys = 0;
    uint8_t items = 0;
    uint8_t logicKeys = 0;
    uint8_t logicItems = 0;
};

std::vector<ModDungeon> sDungeons;
std::vector<ModRegion> sRegions;
std::vector<ModCheck> sChecks;
std::vector<ModEvent> sEvents;
std::vector<ModExit> sExits;
std::vector<bool> sEventState;
std::unordered_map<std::string, int32_t> sRegionsByName;
std::unordered_map<std::string, int32_t> sEventsByName;
std::unordered_map<int32_t, int32_t> sItemOwners;

int32_t sSwappedDungeon = -1;
int8_t sSlotKeysBackup = -1;
uint8_t sSlotItemsBackup = 0;

struct SavedEntranceName {
    size_t override;
    std::string field;
    std::string name;
    int32_t layer;
};
std::vector<SavedEntranceName> sPendingEntranceNames;

std::string ScopedName(int32_t dungeon, const std::string& name) {
    return sDungeons[dungeon].key + "/" + name;
}

ModCheck* FindCheck(int32_t check) {
    int32_t index = check - CheckBase;
    return index >= 0 && index < static_cast<int32_t>(sChecks.size()) ? &sChecks[index] : nullptr;
}

int32_t DungeonOfScene(int32_t sceneNum) {
    for (int32_t dungeon = 0; dungeon < static_cast<int32_t>(sDungeons.size()); dungeon++) {
        const std::vector<int16_t>& scenes = sDungeons[dungeon].scenes;
        if (std::find(scenes.begin(), scenes.end(), sceneNum) != scenes.end()) {
            return dungeon;
        }
    }
    return -1;
}

int16_t ResolveScene(const std::string& name, const std::string& owner) {
    int32_t scene = SceneDB::Instance->RetrieveId(name);
    if (scene < 0 || scene > SceneLimit) {
        SPDLOG_ERROR("[DungeonRegistry] '{}': scene '{}' is unknown or above {:#x}", owner, name, SceneLimit);
        return -1;
    }
    return static_cast<int16_t>(scene);
}

std::string StringField(const Json& object, const char* key, const std::string& fallback = std::string()) {
    return object.contains(key) && object[key].is_string() ? object[key].get<std::string>() : fallback;
}

bool BoolField(const Json& object, const char* key) {
    return object.contains(key) && object[key].is_boolean() && object[key].get<bool>();
}

bool Compile(LogicExpr& logic, const std::string& text, int32_t dungeon, const std::string& owner) {
    std::string error;
    if (!logic.Compile(text, LogicExpr::Scope{ dungeon }, error)) {
        SPDLOG_ERROR("[DungeonRegistry] '{}' logic: {}", owner, error);
        return false;
    }
    return true;
}

struct ItemStyle {
    const char* suffix;
    ItemType type;
    int16_t getItemId;
    uint16_t objectId;
    uint16_t gid;
    GetItemCategory category;
    int16_t chestAnimation;
    bool advancement;
};

RandomizerGet RegisterDungeonItem(const ModDungeon& dungeon, const ItemStyle& style) {
    std::string key = dungeon.key + "." + style.suffix;
    int32_t randoGet = RandoItems_RegisterHostItem(key.c_str());
    if (randoGet == 0) {
        return RG_NONE;
    }
    std::string name = dungeon.name + " " + style.suffix;
    std::vector<Rando::Item>& itemTable = Rando::StaticData::GetItemTable();
    if (itemTable.size() <= static_cast<size_t>(randoGet)) {
        itemTable.resize(static_cast<size_t>(randoGet) + 1);
    }
    itemTable[randoGet] =
        Rando::Item(static_cast<RandomizerGet>(randoGet), Text{ name, name, name }, style.type, style.getItemId,
                    style.advancement, LOGIC_NONE, RHT_NONE, static_cast<uint16_t>(randoGet), style.objectId, style.gid,
                    TEXT_RANDOMIZER_CUSTOM_ITEM, 0x80, style.chestAnimation, style.category, MOD_RANDOMIZER,
                    Text{ "the ", "the ", "the " }, "%g");
    Rando::StaticData::itemNameToEnum[name] = static_cast<RandomizerGet>(randoGet);
    return static_cast<RandomizerGet>(randoGet);
}

void RegisterDungeonItems(int32_t dungeonIndex, const Json& def) {
    static const ItemStyle kSmallKey = { "Small Key",          ITEMTYPE_SMALLKEY,       GI_KEY_SMALL, OBJECT_GI_KEY,
                                         GID_KEY_SMALL,        ITEM_CATEGORY_SMALL_KEY, CHEST_ANIM_SHORT, true };
    static const ItemStyle kBossKey = { "Boss Key",          ITEMTYPE_BOSSKEY,       GI_KEY_BOSS, OBJECT_GI_BOSSKEY,
                                        GID_KEY_BOSS,        ITEM_CATEGORY_BOSS_KEY, CHEST_ANIM_LONG, true };
    static const ItemStyle kMap = { "Map",           ITEMTYPE_MAP,         GI_MAP, OBJECT_GI_MAP,
                                    GID_DUNGEON_MAP, ITEM_CATEGORY_LESSER, CHEST_ANIM_LONG, false };
    static const ItemStyle kCompass = { "Compass",   ITEMTYPE_COMPASS,     GI_COMPASS, OBJECT_GI_COMPASS,
                                        GID_COMPASS, ITEM_CATEGORY_LESSER, CHEST_ANIM_LONG, false };
    ModDungeon& dungeon = sDungeons[dungeonIndex];
    dungeon.smallKeyCount = static_cast<uint8_t>(SOH::Unbound::Field(def, "smallKeys", 0));
    if (dungeon.smallKeyCount > 0) {
        dungeon.smallKey = RegisterDungeonItem(dungeon, kSmallKey);
    }
    if (BoolField(def, "bossKey")) {
        dungeon.bossKey = RegisterDungeonItem(dungeon, kBossKey);
    }
    if (BoolField(def, "map")) {
        dungeon.map = RegisterDungeonItem(dungeon, kMap);
    }
    if (BoolField(def, "compass")) {
        dungeon.compass = RegisterDungeonItem(dungeon, kCompass);
    }
    for (RandomizerGet item : { dungeon.smallKey, dungeon.bossKey, dungeon.map, dungeon.compass }) {
        if (item != RG_NONE) {
            sItemOwners[item] = dungeonIndex;
        }
    }
}

// `dungeon` is null for a check in a vanilla region, which has no keys, map or compass of its own.
RandomizerGet ParseVanillaItem(const ModDungeon* dungeon, const std::string& text, const std::string& owner) {
    if (text.empty()) {
        return RG_GREEN_RUPEE;
    }
    if (dungeon != nullptr && text == "@smallkey") {
        return dungeon->smallKey;
    }
    if (dungeon != nullptr && text == "@bosskey") {
        return dungeon->bossKey;
    }
    if (dungeon != nullptr && text == "@map") {
        return dungeon->map;
    }
    if (dungeon != nullptr && text == "@compass") {
        return dungeon->compass;
    }
    if (std::optional<RandomizerGet> vanilla = RandoEnumFromName<RandomizerGet>(text)) {
        return *vanilla;
    }
    int32_t modItem = RandoItems_GetRandoGet(text.c_str());
    if (modItem == 0) {
        SPDLOG_ERROR("[DungeonRegistry] '{}': unknown vanilla item '{}'", owner, text);
        return RG_GREEN_RUPEE;
    }
    return static_cast<RandomizerGet>(modItem);
}

void AddCheck(int32_t regionIndex, const std::string& checkName, const Json& def) {
    ModRegion& region = sRegions[regionIndex];
    const ModDungeon& dungeon = sDungeons[region.dungeon];
    const std::string spoilerName = dungeon.name + " " + checkName;
    if (Rando::StaticData::locationNameToEnum.contains(spoilerName)) {
        SPDLOG_ERROR("[DungeonRegistry] check '{}' is already registered", spoilerName);
        return;
    }
    const std::string type = StringField(def, "type", "chest");
    const bool isChest = type == "chest";
    if (!isChest && type != "collectible") {
        SPDLOG_ERROR("[DungeonRegistry] check '{}' has unknown type '{}'", spoilerName, type);
        return;
    }
    const std::string sceneName = StringField(def, "scene");
    const int16_t scene = sceneName.empty() ? region.scene : ResolveScene(sceneName, spoilerName);
    if (scene < 0) {
        return;
    }
    const uint8_t flag = static_cast<uint8_t>(SOH::Unbound::Field(def, "flag", 0));
    const int32_t params = static_cast<int32_t>(SOH::Unbound::Field(def, "params", 0));
    const RandomizerCheck rc = static_cast<RandomizerCheck>(CheckBase + static_cast<int32_t>(sChecks.size()));

    ModCheck check;
    check.dungeon = region.dungeon;
    check.vanillaText = StringField(def, "vanilla");
    check.logicText = StringField(def, "logic");
    check.scene = scene;
    check.flagType = isChest ? FLAG_SCENE_TREASURE : FLAG_SCENE_COLLECTIBLE;
    check.flag = flag;
    const ActorID actor = isChest ? ACTOR_EN_BOX : ACTOR_EN_ITEM00;
    const uint8_t spoilerScene = static_cast<uint8_t>(scene);
    const Rando::SpoilerCollectionCheck collection = isChest
                                                         ? Rando::SpoilerCollectionCheck::Chest(spoilerScene, flag)
                                                         : Rando::SpoilerCollectionCheck::Collectable(spoilerScene, flag);
    check.location = Rando::Location(rc, RCQUEST_BOTH, RCTYPE_STANDARD, RCAREA_INVALID, actor,
                                     static_cast<SceneID>(scene), params, checkName, spoilerName, RHT_NONE,
                                     ParseVanillaItem(&dungeon, check.vanillaText, dungeon.key), false, collection);
    check.itemLocation = Rando::ItemLocation(rc);
    Rando::StaticData::locationNameToEnum[spoilerName] = rc;
    Rando::StaticData::CheckFromActorMultimap.emplace(std::make_tuple((int16_t)actor, scene, params), rc);
    region.checks.push_back(static_cast<int32_t>(sChecks.size()));
    sChecks.push_back(std::move(check));
}

const OverworldCheckType* FindOverworldCheckType(const std::string& name) {
    for (const OverworldCheckType& candidate : kOverworldCheckTypes) {
        if (name == candidate.name) {
            return &candidate;
        }
    }
    return nullptr;
}

// A vanilla scene's checks name the hint area of everything in it; a scene without any gives none.
RandomizerCheckArea FindSceneArea(int16_t scene) {
    for (Rando::Location& location : Rando::StaticData::GetLocationTable()) {
        if (location.GetScene() == scene && location.GetArea() != RCAREA_INVALID) {
            return location.GetArea();
        }
    }
    return RCAREA_INVALID;
}

ActorID ResolveCheckActor(const Json& def, const OverworldCheckType& checkType, const std::string& owner) {
    const std::string actorName = StringField(def, "actor");
    if (actorName.empty()) {
        return checkType.actor;
    }
    const int32_t actor = ActorDB::Instance->RetrieveId(actorName);
    if (actor < 0) {
        SPDLOG_ERROR("[DungeonRegistry] check '{}': unknown actor '{}'", owner, actorName);
        return checkType.actor;
    }
    return static_cast<ActorID>(actor);
}

bool RegisterOverworldCheck(const std::string& key, const Json& def) {
    const std::string name = StringField(def, "name", key);
    if (Rando::StaticData::locationNameToEnum.contains(name)) {
        SPDLOG_ERROR("[DungeonRegistry] check '{}' is already registered", name);
        return false;
    }
    const OverworldCheckType* checkType = FindOverworldCheckType(StringField(def, "type"));
    std::optional<RandomizerRegion> region = RandoEnumFromName<RandomizerRegion>(StringField(def, "region"));
    const Json& position = SOH::Unbound::SubArray(def, "position");
    const bool hasPosition = position.size() >= 3 && position[0].is_number() && position[2].is_number();
    if (checkType == nullptr || !region.has_value() || !hasPosition) {
        SPDLOG_ERROR("[DungeonRegistry] check '{}' needs a known \"type\", a vanilla \"region\" (RR_*) and a \"position\"",
                     name);
        return false;
    }
    const int16_t scene = ResolveScene(StringField(def, "scene"), name);
    const uint16_t randoInf = RandoInfCustom_Register((CheckFlagPrefix + key).c_str());
    if (scene < 0 || randoInf == RANDO_INF_CUSTOM_NONE) {
        return false;
    }
    const ActorID actor = ResolveCheckActor(def, *checkType, name);
    const int32_t params = TWO_ACTOR_PARAMS(static_cast<int16_t>(position[0].get<double>()),
                                            static_cast<int16_t>(position[2].get<double>()));
    const RandomizerCheck rc = static_cast<RandomizerCheck>(CheckBase + static_cast<int32_t>(sChecks.size()));

    ModCheck check;
    check.dungeon = NoDungeon;
    check.vanillaText = StringField(def, "vanilla");
    check.logicText = StringField(def, "logic");
    check.scene = scene;
    check.flagType = -1;
    check.flag = -1;
    check.vanillaRegion = *region;
    check.randoInf = randoInf;
    check.shuffleOption = StringField(def, "shuffleOption");
    const auto collection = Rando::SpoilerCollectionCheck::RandomizerInf(static_cast<uint16_t>(RAND_INF_MAX + randoInf));
    check.location = Rando::Location(rc, RCQUEST_BOTH, checkType->type, FindSceneArea(scene), actor,
                                     static_cast<SceneID>(scene), params, name, name, RHT_NONE,
                                     ParseVanillaItem(nullptr, check.vanillaText, name), false, collection);
    check.itemLocation = Rando::ItemLocation(rc);
    Compile(check.logic, check.logicText, NoDungeon, name);
    Rando::StaticData::locationNameToEnum[name] = rc;
    Rando::StaticData::CheckFromActorMultimap.emplace(std::make_tuple((int16_t)actor, scene, params), rc);
    sChecks.push_back(std::move(check));
    return true;
}

// Mirrors SeedContext: the type's setting decides, by overworld or dungeon when it has those variants, and a mod
// option the check names must be on as well.
bool IsOverworldCheckShuffled(const ModCheck& check) {
    const RandomizerCheckType type = check.location.GetRCType();
    const TypeShuffleRule* rule = nullptr;
    for (const TypeShuffleRule& candidate : kTypeShuffleRules) {
        if (candidate.type == type) {
            rule = &candidate;
        }
    }
    if (rule == nullptr) {
        return false;
    }
    const uint8_t value = Rando::Context::GetInstance()->GetOption(rule->setting).Get();
    const bool isDungeon = check.location.IsDungeon();
    const bool isOff = value == RO_GENERIC_OFF;
    const bool isWrongArea = (value == rule->dungeonsOnly && !isDungeon) || (value == rule->overworldOnly && isDungeon);
    if (isOff || isWrongArea) {
        return false;
    }
    return check.shuffleOption.empty() || RandoOptions_GetForSeed(check.shuffleOption.c_str()) != 0;
}

int32_t ResolveRegion(int32_t dungeon, const std::string& name) {
    if (name == OutsideExit) {
        return sDungeons[dungeon].overworldRegion;
    }
    auto region = sRegionsByName.find(ScopedName(dungeon, name));
    return region == sRegionsByName.end() ? RR_NONE : RR_MAX + region->second;
}

void DeclareRegions(int32_t dungeonIndex, const Json& regions, int16_t defaultScene) {
    for (const auto& [name, def] : regions.items()) {
        if (!def.is_object()) {
            continue;
        }
        ModRegion region;
        region.dungeon = dungeonIndex;
        region.name = name;
        const std::string sceneName = StringField(def, "scene");
        region.scene = sceneName.empty() ? defaultScene : ResolveScene(sceneName, name);
        sRegionsByName[ScopedName(dungeonIndex, name)] = static_cast<int32_t>(sRegions.size());
        for (const auto& [eventName, condition] : SOH::Unbound::Sub(def, "events").items()) {
            sEventsByName[ScopedName(dungeonIndex, eventName)] = EventBase + static_cast<int32_t>(sEvents.size());
            region.events.push_back(static_cast<int32_t>(sEvents.size()));
            sEvents.push_back({ dungeonIndex, eventName, condition.is_string() ? condition.get<std::string>() : "" });
        }
        sRegions.push_back(std::move(region));
    }
}

void DefineRegions(int32_t dungeonIndex, const Json& regions) {
    for (const auto& [name, def] : regions.items()) {
        auto found = sRegionsByName.find(ScopedName(dungeonIndex, name));
        if (found == sRegionsByName.end()) {
            continue;
        }
        const int32_t regionIndex = found->second;
        for (int32_t event : sRegions[regionIndex].events) {
            Compile(sEvents[event].logic, sEvents[event].logicText, dungeonIndex, sEvents[event].name);
        }
        for (const auto& [checkName, checkDef] : SOH::Unbound::Sub(def, "checks").items()) {
            if (checkDef.is_object()) {
                AddCheck(regionIndex, checkName, checkDef);
            }
        }
        for (int32_t check : sRegions[regionIndex].checks) {
            ModCheck& modCheck = sChecks[check];
            Compile(modCheck.logic, modCheck.logicText, dungeonIndex, modCheck.location.GetName());
        }
        for (const auto& [target, condition] : SOH::Unbound::Sub(def, "exits").items()) {
            int32_t to = ResolveRegion(dungeonIndex, target);
            if (to == RR_NONE) {
                SPDLOG_ERROR("[DungeonRegistry] '{}': exit to unknown region '{}'", name, target);
                continue;
            }
            ModExit exit = { RR_MAX + regionIndex, to, condition.is_string() ? condition.get<std::string>() : "" };
            Compile(exit.logic, exit.logicText, dungeonIndex, name + " -> " + target);
            sRegions[regionIndex].exits.push_back(static_cast<int32_t>(sExits.size()));
            sExits.push_back(std::move(exit));
        }
    }
}

void AddReturnExit(int32_t dungeonIndex) {
    const ModDungeon& dungeon = sDungeons[dungeonIndex];
    ModRegion& entry = sRegions[dungeon.entryRegion - RR_MAX];
    for (int32_t exit : entry.exits) {
        if (sExits[exit].to == dungeon.overworldRegion) {
            return;
        }
    }
    entry.exits.push_back(static_cast<int32_t>(sExits.size()));
    sExits.push_back({ dungeon.entryRegion, dungeon.overworldRegion, "" });
}

bool RegisterDungeon(const std::string& key, const Json& def) {
    ModDungeon dungeon;
    dungeon.key = key;
    dungeon.name = StringField(def, "name", key);
    for (const Json& sceneName : SOH::Unbound::SubArray(def, "scenes")) {
        int16_t scene = sceneName.is_string() ? ResolveScene(sceneName.get<std::string>(), key) : -1;
        if (scene >= 0) {
            dungeon.scenes.push_back(scene);
        }
    }
    if (dungeon.scenes.empty()) {
        SPDLOG_ERROR("[DungeonRegistry] dungeon '{}' names no registered scene", key);
        return false;
    }
    std::optional<RandomizerRegion> overworld = RandoEnumFromName<RandomizerRegion>(StringField(def, "overworldRegion"));
    if (!overworld.has_value()) {
        SPDLOG_ERROR("[DungeonRegistry] dungeon '{}' needs a vanilla \"overworldRegion\" (RR_*)", key);
        return false;
    }
    dungeon.overworldRegion = *overworld;
    dungeon.entranceName = StringField(def, "entrance");
    dungeon.exitName = StringField(def, "exit");
    dungeon.entranceLogicText = StringField(def, "entranceLogic");

    const int32_t dungeonIndex = static_cast<int32_t>(sDungeons.size());
    sDungeons.push_back(std::move(dungeon));
    RegisterDungeonItems(dungeonIndex, def);

    const Json& regions = SOH::Unbound::Sub(def, "regions");
    DeclareRegions(dungeonIndex, regions, sDungeons[dungeonIndex].scenes.front());
    DefineRegions(dungeonIndex, regions);

    ModDungeon& registered = sDungeons[dungeonIndex];
    registered.entryRegion = ResolveRegion(dungeonIndex, StringField(def, "entryRegion"));
    if (registered.entryRegion < RR_MAX) {
        SPDLOG_ERROR("[DungeonRegistry] dungeon '{}' needs an \"entryRegion\" among its regions", key);
        registered.entryRegion = RR_NONE;
        return true;
    }
    Compile(registered.entranceLogic, registered.entranceLogicText, dungeonIndex, key + " entrance");
    AddReturnExit(dungeonIndex);
    return true;
}

void SwapOutKeySlot() {
    if (sSwappedDungeon < 0) {
        return;
    }
    ModDungeon& dungeon = sDungeons[sSwappedDungeon];
    dungeon.heldKeys = gSaveContext.inventory.dungeonKeys[KeySlot];
    dungeon.items = gSaveContext.inventory.dungeonItems[KeySlot];
    gSaveContext.inventory.dungeonKeys[KeySlot] = sSlotKeysBackup;
    gSaveContext.inventory.dungeonItems[KeySlot] = sSlotItemsBackup;
    sSwappedDungeon = -1;
}

void SwapInKeySlot(int32_t dungeonIndex) {
    const ModDungeon& dungeon = sDungeons[dungeonIndex];
    sSlotKeysBackup = gSaveContext.inventory.dungeonKeys[KeySlot];
    sSlotItemsBackup = gSaveContext.inventory.dungeonItems[KeySlot];
    gSaveContext.inventory.dungeonKeys[KeySlot] = dungeon.heldKeys;
    gSaveContext.inventory.dungeonItems[KeySlot] = dungeon.items;
    sSwappedDungeon = dungeonIndex;
}

// Hidden side effect: rewrites gSaveContext's key slot and mapIndex for the whole stay in a mod dungeon.
void UpdateKeySlotOnSceneInit(int16_t sceneNum) {
    int32_t dungeon = DungeonOfScene(sceneNum);
    if (dungeon != sSwappedDungeon) {
        SwapOutKeySlot();
        if (dungeon >= 0) {
            SwapInKeySlot(dungeon);
        }
    }
    if (dungeon >= 0) {
        gSaveContext.mapIndex = KeySlot;
    }
}

uint8_t ItemBit(const ModDungeon& dungeon, int32_t randoGet) {
    if (randoGet == dungeon.bossKey) {
        return BossKeyBit;
    }
    if (randoGet == dungeon.map) {
        return MapBit;
    }
    return randoGet == dungeon.compass ? CompassBit : 0;
}

std::string ItemSaveName(RandomizerGet item) {
    if (const char* modKey = RandoItems_GetKey(item)) {
        return std::string("mod:") + modKey;
    }
    return std::string(EnumToString(item));
}

RandomizerGet ItemFromSaveName(const std::string& name) {
    if (name.rfind("mod:", 0) == 0) {
        return static_cast<RandomizerGet>(RandoItems_GetRandoGet(name.c_str() + 4));
    }
    return RandoEnumFromName<RandomizerGet>(name).value_or(RG_NONE);
}

void SaveEntranceNames() {
    auto entranceCtx = Rando::Context::GetInstance()->GetEntranceShuffler();
    std::vector<SavedEntranceName> names;
    for (size_t index = 0; index < entranceCtx->entranceOverrides.size(); index++) {
        const EntranceOverride& entry = entranceCtx->entranceOverrides[index];
        const std::pair<const char*, int16_t> fields[] = { { "index", entry.index },
                                                           { "destination", entry.destination },
                                                           { "override", entry.override },
                                                           { "overrideDestination", entry.overrideDestination } };
        for (const auto& [field, value] : fields) {
            if (value >= ENTR_MAX) {
                names.push_back({ index, field, EntranceDB_RetrieveName(value), value % 4 });
            }
        }
    }
    SaveManager::Instance->SaveData("entranceNameCount", names.size());
    SaveManager::Instance->SaveArray("entranceNames", names.size(), [&names](size_t index) {
        SaveManager::Instance->SaveStruct("", [&names, index]() {
            SaveManager::Instance->SaveData("override", names[index].override);
            SaveManager::Instance->SaveData("field", names[index].field);
            SaveManager::Instance->SaveData("name", names[index].name);
            SaveManager::Instance->SaveData("layer", names[index].layer);
        });
    });
}

// The rando section stores entrance numbers, which shift with the mod stack; the names saved here win.
void RestoreEntranceNames() {
    auto entranceCtx = Rando::Context::GetInstance()->GetEntranceShuffler();
    for (const SavedEntranceName& saved : sPendingEntranceNames) {
        int32_t group = EntranceDB_RetrieveIndex(saved.name.c_str());
        if (group < 0 || saved.override >= entranceCtx->entranceOverrides.size()) {
            SPDLOG_WARN("[DungeonRegistry] saved entrance '{}' is not registered by any mod", saved.name);
            continue;
        }
        EntranceOverride& entry = entranceCtx->entranceOverrides[saved.override];
        int16_t value = static_cast<int16_t>(group + saved.layer);
        if (saved.field == "index") {
            entry.index = value;
        } else if (saved.field == "destination") {
            entry.destination = value;
        } else if (saved.field == "override") {
            entry.override = value;
        } else if (saved.field == "overrideDestination") {
            entry.overrideDestination = value;
        }
    }
    sPendingEntranceNames.clear();
}

void ResetSaveState(bool) {
    for (ModDungeon& dungeon : sDungeons) {
        dungeon.heldKeys = -1;
        dungeon.receivedKeys = 0;
        dungeon.items = 0;
    }
    sSwappedDungeon = -1;
}

// LoadArray calls back up to the count it is given even past the saved data, so every array saves its length.
size_t LoadCount(const char* name) {
    size_t count = 0;
    SaveManager::Instance->LoadData(name, count, static_cast<size_t>(0));
    return count;
}

void SaveState(const SaveContext& saveContext, int, bool) {
    SaveManager::Instance->SaveData("dungeonCount", sDungeons.size());
    SaveManager::Instance->SaveArray("dungeons", sDungeons.size(), [](size_t index) {
        const ModDungeon& dungeon = sDungeons[index];
        const bool isSwapped = static_cast<int32_t>(index) == sSwappedDungeon;
        SaveManager::Instance->SaveStruct("", [&dungeon, isSwapped]() {
            SaveManager::Instance->SaveData("key", dungeon.key);
            SaveManager::Instance->SaveData(
                "heldKeys", isSwapped ? gSaveContext.inventory.dungeonKeys[KeySlot] : dungeon.heldKeys);
            SaveManager::Instance->SaveData("receivedKeys", dungeon.receivedKeys);
            SaveManager::Instance->SaveData("items",
                                            isSwapped ? gSaveContext.inventory.dungeonItems[KeySlot] : dungeon.items);
        });
    });
    SaveManager::Instance->SaveData("swapped", sSwappedDungeon >= 0);
    SaveManager::Instance->SaveData("slotKeysBackup", sSlotKeysBackup);
    SaveManager::Instance->SaveData("slotItemsBackup", sSlotItemsBackup);
    if (saveContext.ship.quest.id != QUEST_RANDOMIZER) {
        return;
    }
    SaveManager::Instance->SaveData("checkCount", sChecks.size());
    SaveManager::Instance->SaveArray("checks", sChecks.size(), [](size_t index) {
        ModCheck& check = sChecks[index];
        SaveManager::Instance->SaveStruct("", [&check]() {
            SaveManager::Instance->SaveData("name", check.location.GetName());
            SaveManager::Instance->SaveData("item", ItemSaveName(check.itemLocation.GetPlacedRandomizerGet()));
            SaveManager::Instance->SaveData("status", static_cast<int32_t>(check.itemLocation.GetCheckStatus()));
        });
    });
    SaveEntranceNames();
}

void LoadState() {
    SaveManager::Instance->LoadArray("dungeons", LoadCount("dungeonCount"), [](size_t) {
        SaveManager::Instance->LoadStruct("", []() {
            std::string key;
            int8_t heldKeys = -1;
            uint8_t receivedKeys = 0;
            uint8_t items = 0;
            SaveManager::Instance->LoadData("key", key, std::string());
            SaveManager::Instance->LoadData("heldKeys", heldKeys, static_cast<int8_t>(-1));
            SaveManager::Instance->LoadData("receivedKeys", receivedKeys, static_cast<uint8_t>(0));
            SaveManager::Instance->LoadData("items", items, static_cast<uint8_t>(0));
            for (ModDungeon& dungeon : sDungeons) {
                if (dungeon.key == key) {
                    dungeon.heldKeys = heldKeys;
                    dungeon.receivedKeys = receivedKeys;
                    dungeon.items = items;
                }
            }
        });
    });
    bool wasSwapped = false;
    SaveManager::Instance->LoadData("swapped", wasSwapped, false);
    SaveManager::Instance->LoadData("slotKeysBackup", sSlotKeysBackup, static_cast<int8_t>(-1));
    SaveManager::Instance->LoadData("slotItemsBackup", sSlotItemsBackup, static_cast<uint8_t>(0));
    // The base section already wrote the dungeon's counts into the slot; the next scene init swaps it back in.
    if (wasSwapped) {
        gSaveContext.inventory.dungeonKeys[KeySlot] = sSlotKeysBackup;
        gSaveContext.inventory.dungeonItems[KeySlot] = sSlotItemsBackup;
    }
    sSwappedDungeon = -1;
    SaveManager::Instance->LoadArray("checks", LoadCount("checkCount"), [](size_t) {
        SaveManager::Instance->LoadStruct("", []() {
            std::string name;
            std::string item;
            int32_t status = RCSHOW_UNCHECKED;
            SaveManager::Instance->LoadData("name", name, std::string());
            SaveManager::Instance->LoadData("item", item, std::string());
            SaveManager::Instance->LoadData("status", status, static_cast<int32_t>(RCSHOW_UNCHECKED));
            auto rc = Rando::StaticData::locationNameToEnum.find(name);
            ModCheck* check = rc == Rando::StaticData::locationNameToEnum.end() ? nullptr : FindCheck(rc->second);
            if (check == nullptr) {
                SPDLOG_WARN("[DungeonRegistry] the save holds check '{}', which no mod registers", name);
                return;
            }
            check->itemLocation.SetPlacedItem(ItemFromSaveName(item));
            check->itemLocation.SetCheckStatus(static_cast<RandomizerCheckStatus>(status));
        });
    });
    SaveManager::Instance->LoadArray("entranceNames", LoadCount("entranceNameCount"), [](size_t) {
        SaveManager::Instance->LoadStruct("", []() {
            SavedEntranceName saved = {};
            SaveManager::Instance->LoadData("override", saved.override, static_cast<size_t>(0));
            SaveManager::Instance->LoadData("field", saved.field, std::string());
            SaveManager::Instance->LoadData("name", saved.name, std::string());
            SaveManager::Instance->LoadData("layer", saved.layer, 0);
            sPendingEntranceNames.push_back(saved);
        });
    });
    RestoreEntranceNames();
}

} // namespace

int32_t DungeonRegistry_ResolveKeyScene(int32_t sceneNum) {
    return DungeonOfScene(sceneNum) >= 0 ? SCENE_FOREST_TEMPLE : sceneNum;
}

void DungeonRegistry_Init() {
    SaveManager::Instance->AddInitFunction(ResetSaveState);
    SaveManager::Instance->AddSaveFunction(SaveSection, 1, SaveState, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction(SaveSection, 1, LoadState);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(UpdateKeySlotOnSceneInit);
}

void DungeonRegistry_Load() {
    size_t loaded = SOH::Unbound::ForEachRegistryEntry(RegistryPath, "dungeon", RegisterDungeon);
    sEventState.assign(sEvents.size(), false);
    if (loaded > 0) {
        SPDLOG_INFO("[DungeonRegistry] {}: {} dungeon(s), {} region(s), {} check(s)", RegistryPath, loaded,
                    sRegions.size(), sChecks.size());
    }
    size_t overworldChecks = SOH::Unbound::ForEachRegistryEntry(ChecksRegistryPath, "check", RegisterOverworldCheck);
    if (overworldChecks > 0) {
        SPDLOG_INFO("[DungeonRegistry] {}: {} check(s) in vanilla regions", ChecksRegistryPath, overworldChecks);
    }
}

size_t DungeonRegistry_GetRegionCount() {
    return sRegions.size();
}

void DungeonRegistry_AddRegions() {
    for (size_t index = 0; index < sRegions.size(); index++) {
        const ModRegion& region = sRegions[index];
        std::vector<EventAccess> events;
        for (int32_t event : region.events) {
            events.push_back(EventAccess(static_cast<LogicVal>(EventBase + event), sEvents[event].name,
                                         [] { return true; }, sEvents[event].logicText));
        }
        std::vector<LocationAccess> locations;
        for (int32_t check : region.checks) {
            locations.push_back(LocationAccess(static_cast<RandomizerCheck>(CheckBase + check), [] { return true; },
                                               sChecks[check].logicText));
        }
        std::list<Rando::Entrance> exits;
        for (int32_t exit : region.exits) {
            exits.push_back(Rando::Entrance(static_cast<RandomizerRegion>(sExits[exit].to), [] { return true; },
                                            sExits[exit].logicText));
        }
        areaTable[RR_MAX + index] = Region(sDungeons[region.dungeon].name + " " + region.name,
                                           static_cast<SceneID>(region.scene), false, { RA_NONE }, events,
                                           locations, exits);
    }
    for (const ModDungeon& dungeon : sDungeons) {
        if (dungeon.entryRegion == RR_NONE) {
            continue;
        }
        areaTable[dungeon.overworldRegion].exits.push_back(Rando::Entrance(
            static_cast<RandomizerRegion>(dungeon.entryRegion), [] { return true; }, dungeon.entranceLogicText));
    }
    for (size_t index = 0; index < sChecks.size(); index++) {
        const ModCheck& check = sChecks[index];
        if (check.vanillaRegion != RR_NONE) {
            areaTable[check.vanillaRegion].locations.push_back(
                LocationAccess(static_cast<RandomizerCheck>(CheckBase + index), [] { return true; }, check.logicText));
        }
    }
}

void DungeonRegistry_AppendEntrancePairs(std::vector<Rando::EntranceInfoPair>& table) {
    for (const ModDungeon& dungeon : sDungeons) {
        int32_t entrance = EntranceDB_RetrieveIndex(dungeon.entranceName.c_str());
        int32_t exit = EntranceDB_RetrieveIndex(dungeon.exitName.c_str());
        if (dungeon.entryRegion == RR_NONE || entrance < 0 || exit < 0) {
            continue;
        }
        const RandomizerRegion outside = static_cast<RandomizerRegion>(dungeon.overworldRegion);
        const RandomizerRegion entry = static_cast<RandomizerRegion>(dungeon.entryRegion);
        table.push_back({ { Rando::EntranceType::Dungeon, outside, entry, static_cast<int16_t>(entrance) },
                          { Rando::EntranceType::Dungeon, entry, outside, static_cast<int16_t>(exit) } });
    }
}

int32_t DungeonRegistry_FindEvent(int32_t dungeon, const std::string& name) {
    if (dungeon < 0 || dungeon >= static_cast<int32_t>(sDungeons.size())) {
        return -1;
    }
    auto event = sEventsByName.find(ScopedName(dungeon, name));
    return event == sEventsByName.end() ? -1 : event->second;
}

bool DungeonRegistry_IsModEvent(int32_t logicVal) {
    return logicVal >= EventBase && logicVal < EventBase + static_cast<int32_t>(sEvents.size());
}

bool DungeonRegistry_GetEvent(int32_t logicVal) {
    return DungeonRegistry_IsModEvent(logicVal) && sEventState[logicVal - EventBase];
}

void DungeonRegistry_SetEvent(int32_t logicVal, bool state) {
    if (DungeonRegistry_IsModEvent(logicVal)) {
        sEventState[logicVal - EventBase] = state;
    }
}

void DungeonRegistry_ResetLogic(bool resetItems) {
    std::fill(sEventState.begin(), sEventState.end(), false);
    if (!resetItems) {
        return;
    }
    for (ModDungeon& dungeon : sDungeons) {
        dungeon.logicKeys = 0;
        dungeon.logicItems = 0;
    }
}

bool DungeonRegistry_HasSmallKeys(int32_t dungeon, int32_t count) {
    if (dungeon < 0 || dungeon >= static_cast<int32_t>(sDungeons.size())) {
        return false;
    }
    if (logic->HasItem(RG_SKELETON_KEY)) {
        return true;
    }
    const ModDungeon& owner = sDungeons[dungeon];
    const int32_t keys = logic->CalculatingAvailableChecks ? owner.receivedKeys : owner.logicKeys;
    return keys >= count;
}

bool DungeonRegistry_HasBossKey(int32_t dungeon) {
    if (dungeon < 0 || dungeon >= static_cast<int32_t>(sDungeons.size())) {
        return false;
    }
    const RandomizerGet bossKey = sDungeons[dungeon].bossKey;
    return bossKey == RG_NONE || logic->HasItem(bossKey);
}

bool DungeonRegistry_EvaluateCheck(int32_t check) {
    const ModCheck* modCheck = FindCheck(check);
    return modCheck != nullptr && modCheck->logic.Evaluate();
}

bool DungeonRegistry_EvaluateEvent(int32_t logicVal) {
    return DungeonRegistry_IsModEvent(logicVal) && sEvents[logicVal - EventBase].logic.Evaluate();
}

bool DungeonRegistry_EvaluateExit(int32_t fromRegion, int32_t toRegion) {
    for (const ModDungeon& dungeon : sDungeons) {
        if (dungeon.overworldRegion == fromRegion && dungeon.entryRegion == toRegion) {
            return dungeon.entranceLogic.Evaluate();
        }
    }
    for (const ModExit& exit : sExits) {
        if (exit.from == fromRegion && exit.to == toRegion) {
            return exit.logic.Evaluate();
        }
    }
    return false;
}

bool DungeonRegistry_IsDungeonItem(int32_t randoGet) {
    return sItemOwners.contains(randoGet);
}

bool DungeonRegistry_HasItem(int32_t randoGet, bool fromSave) {
    auto owner = sItemOwners.find(randoGet);
    if (owner == sItemOwners.end()) {
        return false;
    }
    const ModDungeon& dungeon = sDungeons[owner->second];
    if (randoGet == dungeon.smallKey) {
        return (fromSave ? dungeon.receivedKeys : dungeon.logicKeys) > 0;
    }
    return ((fromSave ? dungeon.items : dungeon.logicItems) & ItemBit(dungeon, randoGet)) != 0;
}

void DungeonRegistry_ApplyLogicItem(int32_t randoGet, bool state) {
    auto owner = sItemOwners.find(randoGet);
    if (owner == sItemOwners.end()) {
        return;
    }
    ModDungeon& dungeon = sDungeons[owner->second];
    if (randoGet == dungeon.smallKey) {
        dungeon.logicKeys = state ? dungeon.logicKeys + 1 : std::max(0, dungeon.logicKeys - 1);
        return;
    }
    const uint8_t bit = ItemBit(dungeon, randoGet);
    dungeon.logicItems = state ? (dungeon.logicItems | bit) : (dungeon.logicItems & ~bit);
}

bool DungeonRegistry_GrantItem(int32_t randoGet) {
    auto owner = sItemOwners.find(randoGet);
    if (owner == sItemOwners.end()) {
        return false;
    }
    ModDungeon& dungeon = sDungeons[owner->second];
    const bool isInside = owner->second == sSwappedDungeon;
    if (randoGet == dungeon.smallKey) {
        dungeon.receivedKeys++;
        int8_t& held = isInside ? gSaveContext.inventory.dungeonKeys[KeySlot] : dungeon.heldKeys;
        held = held < 0 ? 1 : held + 1;
        return true;
    }
    const uint8_t bit = ItemBit(dungeon, randoGet);
    dungeon.items |= bit;
    if (isInside) {
        gSaveContext.inventory.dungeonItems[KeySlot] |= bit;
    }
    return true;
}

void DungeonRegistry_AddToPool() {
    for (const ModDungeon& dungeon : sDungeons) {
        if (dungeon.smallKey != RG_NONE) {
            itemPool.insert(itemPool.end(), dungeon.smallKeyCount, dungeon.smallKey);
        }
        for (RandomizerGet item : { dungeon.bossKey, dungeon.map, dungeon.compass }) {
            if (item != RG_NONE) {
                itemPool.push_back(item);
            }
        }
    }
}

std::vector<ModDungeonFillInfo> DungeonRegistry_GetFillInfo() {
    std::vector<ModDungeonFillInfo> fill(sDungeons.size());
    for (size_t index = 0; index < sDungeons.size(); index++) {
        fill[index].smallKey = sDungeons[index].smallKey;
        fill[index].bossKey = sDungeons[index].bossKey;
        fill[index].map = sDungeons[index].map;
        fill[index].compass = sDungeons[index].compass;
    }
    for (size_t index = 0; index < sChecks.size(); index++) {
        if (sChecks[index].dungeon == NoDungeon) {
            continue;
        }
        ModDungeonFillInfo& dungeon = fill[sChecks[index].dungeon];
        const RandomizerCheck rc = static_cast<RandomizerCheck>(CheckBase + index);
        const RandomizerGet vanilla = sChecks[index].location.GetVanillaItem();
        dungeon.locations.push_back(rc);
        if (vanilla == dungeon.smallKey && vanilla != RG_NONE) {
            dungeon.vanillaSmallKeyLocations.push_back(rc);
        } else if (vanilla == dungeon.bossKey && vanilla != RG_NONE) {
            dungeon.vanillaBossKeyLocation = rc;
        } else if (vanilla == dungeon.map && vanilla != RG_NONE) {
            dungeon.vanillaMapLocation = rc;
        } else if (vanilla == dungeon.compass && vanilla != RG_NONE) {
            dungeon.vanillaCompassLocation = rc;
        }
    }
    return fill;
}

bool DungeonRegistry_IsModCheck(int32_t check) {
    return FindCheck(check) != nullptr;
}

Rando::Location* DungeonRegistry_GetLocation(int32_t check) {
    ModCheck* modCheck = FindCheck(check);
    return modCheck == nullptr ? nullptr : &modCheck->location;
}

Rando::ItemLocation* DungeonRegistry_GetItemLocation(int32_t check) {
    ModCheck* modCheck = FindCheck(check);
    return modCheck == nullptr ? nullptr : &modCheck->itemLocation;
}

void DungeonRegistry_AddLocations(std::vector<RandomizerCheck>& allLocations) {
    for (size_t index = 0; index < sChecks.size(); index++) {
        ModCheck& check = sChecks[index];
        if (check.dungeon == NoDungeon && !IsOverworldCheckShuffled(check)) {
            // Its actor still finds it in game, so it hands out its vanilla item.
            check.itemLocation.PlaceVanillaItem();
            continue;
        }
        allLocations.push_back(static_cast<RandomizerCheck>(CheckBase + index));
    }
}

int32_t DungeonRegistry_GetCheckRandomizerInf(int32_t check) {
    const ModCheck* modCheck = FindCheck(check);
    if (modCheck == nullptr || modCheck->randoInf == RANDO_INF_CUSTOM_NONE) {
        return RAND_INF_MAX;
    }
    return RAND_INF_MAX + modCheck->randoInf;
}

void DungeonRegistry_ForEachItemLocation(const std::function<void(Rando::ItemLocation&)>& visit) {
    for (ModCheck& check : sChecks) {
        visit(check.itemLocation);
    }
}

RandomizerCheck DungeonRegistry_FindCheckBySceneFlag(int16_t sceneNum, int16_t flagType, int16_t flag) {
    for (size_t index = 0; index < sChecks.size(); index++) {
        const ModCheck& check = sChecks[index];
        if (check.scene == sceneNum && check.flagType == flagType && check.flag == flag) {
            return static_cast<RandomizerCheck>(CheckBase + index);
        }
    }
    return RC_UNKNOWN_CHECK;
}
