#include "FleetRando.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/3drando/fill.hpp"
#include "soh/Enhancements/randomizer/3drando/item_pool.hpp"
#include "soh/Enhancements/randomizer/3drando/starting_inventory.hpp"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/dungeon.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/rng.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/ModApi/ModDungeons/ModDungeonsHost.h"

namespace {

uint32_t sSeed = 0;
bool sPortalOpen = false;

// What the combined seed decides for both games. pieces is this game's share of the Triforce Hunt, -1 = unset.
struct ComboOptions {
    bool hunt = false;
    uint8_t required = 0;
    int pieces = -1;
    bool mmStart = false;
    bool sharedWallets = false;
};
ComboOptions sComboOptions;
// Confined fill rolls these once per seed; every oracle reset wipes them with the placements.
std::unordered_map<RandomizerCheck, uint16_t> sPrices;

char* CopyString(const std::string& text) {
    char* copy = static_cast<char*>(std::malloc(text.size() + 1));
    std::memcpy(copy, text.c_str(), text.size() + 1);
    return copy;
}

// The rando menu keeps the enabled tricks as one comma-separated list of name tags.
std::vector<std::string> EnabledTrickTags() {
    std::vector<std::string> tags;
    std::stringstream names(CVarGetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"), ""));
    std::string name;
    while (std::getline(names, name, ',')) {
        if (!name.empty()) {
            tags.push_back(name);
        }
    }
    return tags;
}

std::set<RandomizerTrick> ParseEnabledTricks() {
    std::set<RandomizerTrick> tricks;
    for (const std::string& tag : EnabledTrickTags()) {
        if (Rando::StaticData::trickToEnum.contains(tag)) {
            tricks.insert(Rando::StaticData::trickToEnum[tag]);
        }
    }
    return tricks;
}

// Every option lives in its CVar, so the CVars alone reproduce the seed's settings on another machine.
nlohmann::json DumpSettings() {
    nlohmann::json settings = nlohmann::json::object();
    for (const Rando::Option& option : Rando::Settings::GetInstance()->GetAllOptions()) {
        const std::string& cvar = option.GetCVarName();
        if (!cvar.empty()) {
            settings[cvar] = CVarGetInteger(cvar.c_str(), option.GetMenuOptionDefault());
        }
    }
    settings[CVAR_RANDOMIZER_SETTING("ExcludedLocations")] =
        CVarGetString(CVAR_RANDOMIZER_SETTING("ExcludedLocations"), "");
    return { { "settings", std::move(settings) }, { "enabledTricks", EnabledTrickTags() } };
}

// The snapshot is authoritative: what it leaves out falls back to no exclusions and no tricks.
void RestoreSettings(const nlohmann::json& snapshot) {
    CVarSetString(CVAR_RANDOMIZER_SETTING("ExcludedLocations"), "");
    const nlohmann::json settings = snapshot.value("settings", nlohmann::json::object());
    for (const auto& [cvar, value] : settings.items()) {
        if (value.is_string()) {
            CVarSetString(cvar.c_str(), value.get<std::string>().c_str());
        } else if (value.is_number_integer()) {
            CVarSetInteger(cvar.c_str(), value.get<int32_t>());
        }
    }
    std::string tags;
    for (const nlohmann::json& tag : snapshot.value("enabledTricks", nlohmann::json::array())) {
        if (tag.is_string()) {
            tags += tag.get<std::string>() + ",";
        }
    }
    CVarSetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"), tags.c_str());
}

// The combined goal replaces the player's own. A hunt counts pieces from both games, so the local wincon
// shows the combined count while this pool holds only its share; the Mask Shop is the portal, never a hint.
void ApplyComboOptions() {
    auto ctx = Rando::Context::GetInstance();
    ctx->GetOption(RSK_MASK_SHOP_HINT).Set(RO_GENERIC_OFF);
    ctx->GetOption(RSK_WINCON).Set(sComboOptions.hunt ? RO_WINCON_TRIFORCE_PIECES : RO_WINCON_DEFEAT_GANON);
    ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_LOCATION).Set(RO_TRIFORCE_HUNT_LOCATION_ANYWHERE);
    ctx->GetOption(RSK_WINCON_TRIFORCE_COUNT).Set(sComboOptions.required);
    if (sComboOptions.pieces >= 0) {
        const uint8_t pieces = sComboOptions.hunt ? static_cast<uint8_t>(sComboOptions.pieces) : 0;
        ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_TOTAL).Set(pieces);
        for (RandomizerSettingKey key :
             { RSK_RAINBOW_BRIDGE_TRIFORCE_COUNT, RSK_GBK_TRIFORCE_COUNT, RSK_GANONS_SOUL_TRIFORCE_COUNT }) {
            if (ctx->GetOption(key).Get() > pieces) {
                ctx->GetOption(key).Set(pieces);
            }
        }
    }
    // MM has no Child Wallet, so a wallet both games share cannot start from one.
    if (sComboOptions.sharedWallets) {
        ctx->GetOption(RSK_SHUFFLE_CHILD_WALLET).Set(RO_GENERIC_OFF);
    }
    // Starting in MM drops an itemless child at the Mask Shop, who must still be able to leave the forest.
    if (sComboOptions.mmStart) {
        ctx->GetOption(RSK_STARTING_AGE).Set(RO_AGE_CHILD);
        if (ctx->GetOption(RSK_FOREST).Is(RO_CLOSED_FOREST_ON)) {
            ctx->GetOption(RSK_FOREST).Set(RO_CLOSED_FOREST_DEKU_ONLY);
        }
    }
}

// The native generator's preparation up to its free fill: settings, entrances, then everything confined to Ship.
bool PrepareSeed() {
    auto ctx = Rando::Context::GetInstance();
    Rando::Settings::GetInstance()->SetAllToContext();
    ApplyComboOptions();
    Random_Init(sSeed);
    ctx->overrides.clear();
    ctx->ItemReset();
    ctx->HintReset();
    ctx->GetLogic()->Reset();
    Regions::AccessReset();
    ctx->FinalizeSettings(
        Rando::StaticData::ParseExcludedLocations(CVarGetString(CVAR_RANDOMIZER_SETTING("ExcludedLocations"), "")),
        ParseEnabledTricks());
    Random_Init(sSeed);
    BossRegistry_AssignForSeed();
    if (!FillConfinedItems()) {
        return false;
    }
    sPrices.clear();
    for (RandomizerCheck rc : ctx->allLocations) {
        Rando::ItemLocation* location = ctx->GetItemLocation(rc);
        if (location->HasCustomPrice()) {
            sPrices[rc] = location->GetPrice();
        }
    }
    return true;
}

// FinalizeSettings rolls these from the seed, so they belong to the seed rather than to the settings.
nlohmann::json DumpMasterQuestDungeons() {
    nlohmann::json names = nlohmann::json::array();
    for (const Rando::DungeonInfo* dungeon : Rando::Context::GetInstance()->GetDungeons()->GetDungeonList()) {
        if (dungeon->IsMQ()) {
            names.push_back(dungeon->GetName());
        }
    }
    return names;
}

// The shuffled entrances in the spoiler's own format; the table ends at its first all-zero row.
nlohmann::json DumpEntrances() {
    nlohmann::json entrances = nlohmann::json::array();
    for (const EntranceOverride& entrance : Rando::Context::GetInstance()->GetEntranceShuffler()->entranceOverrides) {
        if (entrance.type == 0 && entrance.index == 0 && entrance.override == 0) {
            break;
        }
        entrances.push_back({ { "type", entrance.type },
                              { "index", entrance.index },
                              { "destination", entrance.destination },
                              { "override", entrance.override },
                              { "overrideDestination", entrance.overrideDestination } });
    }
    return entrances;
}

// Hearts never gate glitchless logic, so they fill like junk.
bool IsAdvancement(RandomizerGet item) {
    if (item == RG_PIECE_OF_HEART || item == RG_HEART_CONTAINER || item == RG_TREASURE_GAME_HEART) {
        return false;
    }
    return Rando::StaticData::RetrieveItem(item).IsAdvancement();
}

const char* CategoryName(RandomizerGet item) {
    switch (Rando::StaticData::RetrieveItem(item).GetCategory()) {
        case ITEM_CATEGORY_JUNK:
            return "junk";
        case ITEM_CATEGORY_LESSER:
            return "lesser";
        case ITEM_CATEGORY_HEALTH:
            return "health";
        case ITEM_CATEGORY_BOSS_KEY:
            return "bossKey";
        case ITEM_CATEGORY_SMALL_KEY:
            return "smallKey";
        case ITEM_CATEGORY_SKULLTULA_TOKEN:
            return "token";
        default:
            return "major";
    }
}

const std::string& ItemName(RandomizerGet item) {
    return Rando::StaticData::RetrieveItem(item).GetName().GetEnglish();
}

nlohmann::json PoolEntry(RandomizerGet item) {
    return { { "name", ItemName(item) },
             { "advancement", IsAdvancement(item) },
             { "major", Rando::StaticData::RetrieveItem(item).IsMajorItem() },
             { "category", CategoryName(item) } };
}

// Link's Pocket is not a check the dump offers, so the combined fill must still own whatever it holds.
nlohmann::json ForcedPlacements() {
    auto ctx = Rando::Context::GetInstance();
    nlohmann::json forced = nlohmann::json::object();
    const RandomizerGet reward = ctx->GetItemLocation(RC_LINKS_POCKET)->GetPlacedRandomizerGet();
    Rando::OptionValue& pocket = ctx->GetOption(RSK_LINKS_POCKET);
    if (reward != RG_NONE) {
        forced["Link's Pocket"]["item"] = ItemName(reward);
    } else if (pocket.Is(RO_LINKS_POCKET_ADVANCEMENT)) {
        forced["Link's Pocket"]["category"] = "advancement";
    } else if (pocket.Is(RO_LINKS_POCKET_ANYTHING)) {
        forced["Link's Pocket"]["category"] = "any";
    }
    return forced;
}

} // namespace

char* FleetRando_DumpStaticData(void) {
    auto ctx = Rando::Context::GetInstance();
    if (!PrepareSeed()) {
        SPDLOG_ERROR("[Fleet] Ship could not place its entrances and confined items for seed {}", sSeed);
        return nullptr;
    }

    nlohmann::json checks = nlohmann::json::array();
    nlohmann::json fixed = nlohmann::json::array();
    nlohmann::json prices = nlohmann::json::object();
    for (RandomizerCheck rc : ctx->allLocations) {
        // Link's Pocket and the win condition are not checks the combined fill may use.
        if (rc == RC_LINKS_POCKET || rc == RC_WINCON) {
            continue;
        }
        Rando::Location* location = Rando::StaticData::GetLocation(rc);
        const std::string& name = location->GetName();
        if (name.empty()) {
            continue;
        }
        Rando::ItemLocation* itemLocation = ctx->GetItemLocation(rc);
        RandomizerGet placed = itemLocation->GetPlacedRandomizerGet();
        if (placed == RG_NONE) {
            checks.push_back({ { "name", name } });
        } else {
            fixed.push_back({ { "check", name },
                              { "item", ItemName(placed) },
                              { "advancement", IsAdvancement(placed) },
                              { "major", Rando::StaticData::RetrieveItem(placed).IsMajorItem() },
                              { "hintable", itemLocation->IsHintable() } });
        }
        const RandomizerCheckType type = location->GetRCType();
        if (type == RCTYPE_SHOP || type == RCTYPE_SCRUB || type == RCTYPE_MERCHANT) {
            prices[name] = itemLocation->GetPrice();
        }
    }

    nlohmann::json pool = nlohmann::json::array();
    for (RandomizerGet item : itemPool) {
        if (Rando::StaticData::RetrieveItem(item).GetItemType() == ITEMTYPE_SHOP) {
            continue;
        }
        pool.push_back(PoolEntry(item));
    }
    // Shop slots are not in itemPool; the native fill pads them with junk the same way.
    while (pool.size() < checks.size()) {
        pool.push_back(PoolEntry(GetJunkItem()));
    }

    nlohmann::json items = nlohmann::json::array();
    for (int rg = 0; rg < RG_MAX; rg++) {
        const RandomizerGet item = static_cast<RandomizerGet>(rg);
        const std::string& name = ItemName(item);
        if (name.empty()) {
            continue;
        }
        items.push_back({ { "name", name },
                          { "displayName", name },
                          { "advancement", IsAdvancement(item) },
                          { "trap", item == RG_ICE_TRAP },
                          { "trickNames", nlohmann::json::array() } });
    }

    nlohmann::json accessibility = {
        { "noLogic", ctx->GetOption(RSK_NO_LOGIC).Is(RO_GENERIC_ON) },
        { "allLocationsReachable", static_cast<bool>(ctx->GetOption(RSK_ALL_CHECKS_REACHABLE)) },
        { "lockOverworldDoors", static_cast<bool>(ctx->GetOption(RSK_LOCK_OVERWORLD_DOORS)) },
        { "maskQuestShuffle", static_cast<bool>(ctx->GetOption(RSK_SHUFFLE_MASKS)) },
    };

    const nlohmann::json dump = { { "checks", std::move(checks) }, { "pool", std::move(pool) },
                                  { "fixed", std::move(fixed) },   { "items", std::move(items) },
                                  { "prices", std::move(prices) }, { "accessibility", std::move(accessibility) } };
    return CopyString(dump.dump());
}

void FleetRando_Reset(void) {
    auto ctx = Rando::Context::GetInstance();
    ctx->GetLogic()->Reset();
    Regions::AccessReset();
    ctx->LocationReset();
    ctx->ItemReset();
    for (const auto& [rc, price] : sPrices) {
        ctx->GetItemLocation(rc)->SetCustomPrice(price);
    }
    ApplyStartingInventory();
}

void FleetRando_SetOwnedItems(const char* itemsJson) {
    if (itemsJson == nullptr) {
        return;
    }
    const nlohmann::json items = nlohmann::json::parse(itemsJson, nullptr, false);
    if (!items.is_array()) {
        return;
    }
    for (const nlohmann::json& name : items) {
        auto item = Rando::StaticData::itemNameToEnum.find(name.get<std::string>());
        if (item != Rando::StaticData::itemNameToEnum.end()) {
            Rando::StaticData::RetrieveItem(item->second).ApplyEffect();
        }
    }
}

char* FleetRando_GetReachableChecks(void) {
    auto ctx = Rando::Context::GetInstance();
    const std::vector<RandomizerCheck> reachable = ReachabilitySearch(ctx->allLocations);
    // The Happy Mask Shop holds no checks of its own: Fleet gates Majora's Mask on reaching it.
    Region* portal = RegionTable(RR_MARKET_MASK_SHOP);
    sPortalOpen = portal->Child() || portal->Adult();
    nlohmann::json names = nlohmann::json::array();
    for (RandomizerCheck rc : reachable) {
        const std::string& name = Rando::StaticData::GetLocation(rc)->GetName();
        if (!name.empty()) {
            names.push_back(name);
        }
    }
    return CopyString(names.dump());
}

bool FleetRando_PlaceItem(const char* check, const char* item) {
    if (check == nullptr || item == nullptr) {
        return false;
    }
    auto location = Rando::StaticData::locationNameToEnum.find(check);
    auto placed = Rando::StaticData::itemNameToEnum.find(item);
    if (location == Rando::StaticData::locationNameToEnum.end() || placed == Rando::StaticData::itemNameToEnum.end()) {
        return false;
    }
    Rando::Context::GetInstance()->PlaceItemInLocation(location->second, placed->second, false, false);
    return true;
}

char* FleetRando_Request(const char* op, const char* json) {
    const std::string request = op != nullptr ? op : "";
    const nlohmann::json input = nlohmann::json::parse(json != nullptr ? json : "{}", nullptr, false);
    if (request == "InitRandoHeadless") {
        OTRGlobals::InitRandoHeadless();
        return CopyString("{}");
    }
    if (request == "SetSeed") {
        sSeed = input.value("seed", 0u);
        return CopyString("{}");
    }
    if (request == "SetComboOptions") {
        sComboOptions = { .hunt = input.value("hunt", false),
                       .required = static_cast<uint8_t>(std::clamp(input.value("required", 0), 0, 100)),
                       .pieces = std::clamp(input.value("pieces", -1), -1, 100),
                       .mmStart = input.value("startingGame", std::string("OOT")) == "MM",
                       .sharedWallets = input.value("sharedWallets", false) };
        return CopyString("{}");
    }
    if (request == "DumpSettings") {
        nlohmann::json settings = DumpSettings();
        settings["entrances"] = DumpEntrances();
        settings["masterQuestDungeons"] = DumpMasterQuestDungeons();
        return CopyString(settings.dump());
    }
    if (request == "RestoreSettings") {
        RestoreSettings(input.is_object() ? input : nlohmann::json::object());
        return CopyString("{}");
    }
    if (request == "GetForcedPlacements") {
        return CopyString(ForcedPlacements().dump());
    }
    // Describes the owned set of the latest GetReachableChecks.
    if (request == "GetPortalOpen") {
        return CopyString(nlohmann::json{ { "open", sPortalOpen } }.dump());
    }
    SPDLOG_WARN("[Fleet] Ship has no randomizer service '{}'", request);
    return nullptr;
}

void FleetRando_FreeString(char* text) {
    std::free(text);
}
