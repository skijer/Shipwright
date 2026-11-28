#include "EnemySouls.h"

#include <cassert>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

#include "soh/Enhancements/randomizer/EnemyPlacement.h"
#include "soh/Enhancements/randomizer/draw.h"
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/ModApi/ModDungeons/DungeonRegistry.h"
#include "soh/ModApi/RandoItems/RandoItems.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh_assets.h"

namespace {

constexpr std::string_view ResourcePrefix = "__OTR__";
constexpr std::string_view SoulItemPrefix = "soul.";
constexpr Color_RGB8 DefaultFlameColor = { 200, 60, 60 };
constexpr float DefaultModelScale = 0.01f;

struct Soul {
    std::string itemKey;
    std::string name;
    uint8_t category;
    uint8_t origin;
    // Prefixed resource names: SkelAnime keeps the animation's and reads it back every update.
    std::string skeletonResource;
    std::string animationResource;
    EnemySoulModel model;
    Color_RGB8 flameColor;
    std::optional<bool> hasModel;
};

std::deque<Soul> sSouls;
std::map<std::string, size_t> sSoulsBySoulKey;
std::map<std::string, std::vector<std::string>> sNativeSoulSharers;
std::set<int16_t> sBosses;

std::string ToResourceName(const char* path) {
    const std::string_view view = path != nullptr ? path : "";
    return view.starts_with(ResourcePrefix) ? std::string(view) : std::string(ResourcePrefix) + std::string(view);
}

bool HasGenericFields(const SOHActorDefinition* definition) {
    return definition->structSize >= SOH_ACTOR_DEFINITION_GENERIC_SOUL_SIZE;
}

uint8_t ReadCategory(const SOHActorDefinition* definition) {
    const bool declaresCategory = HasGenericFields(definition) &&
                                  definition->enemyCategory > SOH_ENEMY_CATEGORY_REGULAR &&
                                  definition->enemyCategory <= SOH_ENEMY_CATEGORY_BOSS;
    if (declaresCategory) {
        return definition->enemyCategory;
    }
    return (definition->enemyFlags & SOH_ACTOR_ENEMY_MINIBOSS) ? SOH_ENEMY_CATEGORY_MINIBOSS
                                                               : SOH_ENEMY_CATEGORY_REGULAR;
}

uint8_t ResolveOrigin(const SOHActorDefinition* definition, const std::string& soulKey) {
    const uint8_t declared = HasGenericFields(definition) ? definition->enemyOrigin : SOH_ENEMY_ORIGIN_AUTO;
    if (declared != SOH_ENEMY_ORIGIN_AUTO) {
        return declared;
    }
    if (soulKey.starts_with("oot.")) {
        return SOH_ENEMY_ORIGIN_OOT;
    }
    return soulKey.starts_with("mm.") ? SOH_ENEMY_ORIGIN_MM : SOH_ENEMY_ORIGIN_CUSTOM;
}

Color_RGB8 ReadFlameColor(const SOHSoulModel& model) {
    if (model.flameRed == 0 && model.flameGreen == 0 && model.flameBlue == 0) {
        return DefaultFlameColor;
    }
    return { model.flameRed, model.flameGreen, model.flameBlue };
}

void StoreModel(Soul& soul, const SOHSoulModel& model) {
    soul.skeletonResource = ToResourceName(model.skeleton);
    soul.animationResource = ToResourceName(model.idleAnimation);
    soul.model.skeleton = soul.skeletonResource.c_str();
    soul.model.animation = soul.animationResource.c_str();
    soul.model.isFlex = model.isFlex != 0;
    soul.model.scale = model.scale > 0.0f ? model.scale : DefaultModelScale;
    soul.model.yOffset = model.yOffset;
    soul.flameColor = ReadFlameColor(model);
}

// Archives can mount after the mods register, so the model is looked up the first time the soul is drawn.
bool HasModel(Soul& soul) {
    if (!soul.hasModel.has_value()) {
        soul.hasModel = !soul.animationResource.empty() && ResourceMgr_FileExists(soul.skeletonResource.c_str()) &&
                        ResourceMgr_FileExists(soul.animationResource.c_str());
        if (!*soul.hasModel) {
            SPDLOG_WARN("[EnemySouls] '{}' draws the generic soul: its model resources are missing", soul.itemKey);
        }
    }
    return *soul.hasModel;
}

Soul* FindSoulOfItem(const char* itemKey) {
    const std::string_view key = itemKey != nullptr ? itemKey : "";
    if (!key.starts_with(SoulItemPrefix)) {
        return nullptr;
    }
    const auto known = sSoulsBySoulKey.find(std::string(key.substr(SoulItemPrefix.size())));
    return known != sSoulsBySoulKey.end() ? &sSouls[known->second] : nullptr;
}

// The registry hands the soul's runtime id in tableId, which leads back to its item key.
void DrawSoul(PlayState* play, GetItemEntry* entry) {
    const SOHCustomItemDefinition* definition = CustomItemRegistry_FindByRuntimeId(entry->tableId);
    Soul* soul = definition != nullptr ? FindSoulOfItem(definition->key) : nullptr;
    assert(soul != nullptr);
    if (soul == nullptr) {
        return;
    }
    Randomizer_DrawEnemySoulOf(play, HasModel(*soul) ? &soul->model : nullptr, soul->flameColor);
}

void RegisterSoulItem(Soul& soul) {
    const std::string text = "You got the %r" + soul.name + "%w!&They can be defeated now.";
    SOHCustomItemRandomizer randomizer = {};
    randomizer.structSize = sizeof(randomizer);
    randomizer.flags = SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    randomizer.type = SOH_CUSTOM_ITEM_TYPE_ITEM;
    SOHCustomItemDefinition definition = {};
    definition.structSize = sizeof(definition);
    definition.key = soul.itemKey.c_str();
    definition.iconPath = gBossSoulTex;
    definition.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    definition.getItemText = text.c_str();
    definition.getItemModelPath = gBossSoulSkullDL;
    definition.displayName = soul.name.c_str();
    definition.getItemEntry.drawFunc = DrawSoul;
    definition.randomizer = &randomizer;
    if (!CustomItemRegistry_Register(&definition)) {
        SPDLOG_ERROR("[EnemySouls] Could not register '{}'", soul.itemKey);
        return;
    }
    // Each soul keeps its own pose, which the draw keys by the soul's item.
    soul.model.soul = static_cast<RandomizerGet>(RandoItems_GetRandoGet(soul.itemKey.c_str()));
}

size_t AddSoul(const SOHActorDefinition* definition, const SOHCustomEnemy& enemy, uint8_t category) {
    Soul& soul = sSouls.emplace_back();
    soul.itemKey = std::string(SoulItemPrefix) + enemy.soulKey;
    soul.name = std::string("Soul of ") + enemy.name;
    soul.category = category;
    soul.origin = ResolveOrigin(definition, enemy.soulKey);
    soul.flameColor = DefaultFlameColor;
    sSoulsBySoulKey.emplace(enemy.soulKey, sSouls.size() - 1);
    return sSouls.size() - 1;
}

// A later definition may bring the model the first one sharing the soul left out.
void AttachSoulModel(const SOHActorDefinition* definition, size_t index) {
    Soul& soul = sSouls[index];
    const bool declaresModel = HasGenericFields(definition) && definition->soulModel.skeleton != nullptr;
    if (!declaresModel || !soul.skeletonResource.empty() || CustomItemRegistry_Find(soul.itemKey.c_str()) != nullptr) {
        return;
    }
    StoreModel(soul, definition->soulModel);
    RegisterSoulItem(soul);
}

uint8_t RegisterCustomSoul(const SOHActorDefinition* definition, const SOHCustomEnemy& enemy, uint8_t category) {
    const auto known = sSoulsBySoulKey.find(enemy.soulKey);
    const size_t index = known != sSoulsBySoulKey.end() ? known->second : AddSoul(definition, enemy, category);
    AttachSoulModel(definition, index);
    return sSouls[index].category;
}

int32_t RegisterDrop(const SOHCustomEnemy& enemy) {
    const std::string key = std::string("drop.") + enemy.key;
    const std::string name = std::string("Enemy Drop ") + enemy.name;
    return DungeonRegistry_RegisterEnemyDropCheck(key.c_str(), name.c_str(), enemy.actorId);
}

const char* DescribeOrigin(uint8_t origin) {
    switch (origin) {
        case SOH_ENEMY_ORIGIN_OOT:
            return "custom; vanilla in Ocarina of Time";
        case SOH_ENEMY_ORIGIN_MM:
            return "custom; vanilla in Majora's Mask";
        case SOH_ENEMY_ORIGIN_CUSTOM:
            return "custom";
        default:
            SPDLOG_WARN("[EnemySouls] Unknown enemy origin {}", origin);
            return "custom";
    }
}

} // namespace

void EnemySouls_OnEnemyRegistered(const SOHActorDefinition* definition, SOHCustomEnemy& enemy) {
    const uint8_t category = ReadCategory(definition);
    const bool dropOnKill = HasGenericFields(definition) && definition->dropOnKill != 0;
    enemy.soulCategory = SOH_ENEMY_CATEGORY_REGULAR;
    enemy.dropCheck = dropOnKill ? RegisterDrop(enemy) : RC_UNKNOWN_CHECK;
    if (category != SOH_ENEMY_CATEGORY_REGULAR) {
        enemy.flags |= SOH_ACTOR_ENEMY_MINIBOSS;
    }
    if (enemy.soulKey == nullptr) {
        return;
    }
    if (EnemyCatalog_FindNativeSoul(enemy.soulKey) != nullptr) {
        sNativeSoulSharers[enemy.soulKey].push_back(enemy.name);
        return;
    }
    enemy.soulCategory = RegisterCustomSoul(definition, enemy, category);
    if (enemy.soulCategory == SOH_ENEMY_CATEGORY_BOSS) {
        sBosses.insert(enemy.actorId);
    }
}

bool EnemySouls_HasBosses() {
    return !sBosses.empty();
}

std::string EnemySouls_DescribeNativeSoul(const char* soulKey, const std::string& name) {
    const auto sharers = sNativeSoulSharers.find(soulKey);
    if (sharers == sNativeSoulSharers.end()) {
        return name + " (vanilla)";
    }
    std::string names;
    for (const std::string& sharer : sharers->second) {
        names += names.empty() ? sharer : ", " + sharer;
    }
    return name + " (vanilla; shared with custom " + names + ")";
}

std::string EnemySouls_DescribeCustomSoul(const char* soulKey, const std::string& fallbackName) {
    const auto known = sSoulsBySoulKey.find(soulKey);
    if (known == sSoulsBySoulKey.end()) {
        return fallbackName + " (custom)";
    }
    const Soul& soul = sSouls[known->second];
    return soul.name + " (" + DescribeOrigin(soul.origin) + ")";
}

void EnemySouls_WriteSpoiler(nlohmann::ordered_json& spoiler) {
    nlohmann::ordered_json souls = nlohmann::ordered_json::object();
    for (const auto& [soulKey, index] : sSoulsBySoulKey) {
        souls[sSouls[index].itemKey] = EnemySouls_DescribeCustomSoul(soulKey.c_str(), sSouls[index].name);
    }
    for (const EnemySoul& soul : EnemyCatalog_GetNativeSouls()) {
        if (sNativeSoulSharers.contains(soul.key)) {
            souls[std::string(SoulItemPrefix) + soul.key] = EnemySouls_DescribeNativeSoul(soul.key, soul.name);
        }
    }
    spoiler["enemySouls"] = souls;
}
