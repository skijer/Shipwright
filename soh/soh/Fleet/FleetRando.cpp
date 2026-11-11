#include "soh/ModApi/RandoItems/RandoItems.h"
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "FleetRando.h"
#include "FleetForeignItems.h"
#include "FleetCrossDungeon.h"
#include "FleetHints.h"
#include "FleetRuntime.h"
#include "soh/ShipInit.hpp"
#include "FleetGame.h"
#include "soh/SohGui/SohMenu.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/FileSelectEnhancements.h"
#include "soh/Notification/Notification.h"

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;
}

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
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
#include "soh/SaveManager.h"
#include "soh/Enhancements/randomizer/savefile.h"

extern "C" {
#include "functions.h"
#include "variables.h"
void Randomizer_InitSaveFile(void);
void Sram_InitNewSave(void);
void FileChoose_Main(GameState* state);
void FileChoose_UpdateRandomizerMenu(GameState* state);
}

#ifdef main
#undef main
#endif

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

// Starting items no starting option can hold (mod items and the like), named as the pool names them.
// The fill and the oracle treat them as owned; Fleet delivers them once the file is played.
const char* const kFleetStartingItemsKey = "fleet.startingItems";
std::vector<std::string> sFleetStartingItems;

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
    settings[kFleetStartingItemsKey] = sFleetStartingItems;
    return { { "settings", std::move(settings) }, { "enabledTricks", EnabledTrickTags() } };
}

// The snapshot is authoritative: what it leaves out falls back to no exclusions and no tricks.
void RestoreSettings(const nlohmann::json& snapshot) {
    CVarSetString(CVAR_RANDOMIZER_SETTING("ExcludedLocations"), "");
    const nlohmann::json settings = snapshot.value("settings", nlohmann::json::object());
    sFleetStartingItems.clear();
    for (const nlohmann::json& name : settings.value(kFleetStartingItemsKey, nlohmann::json::array())) {
        if (name.is_string()) {
            sFleetStartingItems.push_back(name.get<std::string>());
        }
    }
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

enum class EntranceLayout { Roll, FromSeed };

// A saved seed carries its entrance table. Rolling the layout again only has to fail once in a process whose
// random draws drift from the generator's, so a saved seed keeps the native world vanilla until its table is applied.
bool FillConfinedItemsWithLayout(EntranceLayout layout) {
    auto& entranceShuffle = Rando::Context::GetInstance()->GetOption(RSK_SHUFFLE_ENTRANCES);
    const uint8_t rolled = entranceShuffle.Get();
    if (layout == EntranceLayout::FromSeed) {
        entranceShuffle.Set(RO_GENERIC_OFF);
    }
    const bool filled = FillConfinedItems();
    entranceShuffle.Set(rolled);
    return filled;
}

// The native generator's preparation up to its free fill: settings, entrances, then everything confined to Ship.
bool PrepareSeed(EntranceLayout layout) {
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
    if (!FillConfinedItemsWithLayout(layout)) {
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

bool IsAbility(RandomizerGet item) {
    switch (item) {
        case RG_BRONZE_SCALE:
        case RG_POWER_BRACELET:
        case RG_CLIMB:
        case RG_CRAWL:
        case RG_OPEN_CHEST:
        case RG_SPEAK_DEKU:
        case RG_SPEAK_GERUDO:
        case RG_SPEAK_GORON:
        case RG_SPEAK_HYLIAN:
        case RG_SPEAK_KOKIRI:
        case RG_SPEAK_ZORA:
        case RG_OCARINA_A_BUTTON:
        case RG_OCARINA_C_UP_BUTTON:
        case RG_OCARINA_C_DOWN_BUTTON:
        case RG_OCARINA_C_LEFT_BUTTON:
        case RG_OCARINA_C_RIGHT_BUTTON:
        case RG_FISHING_POLE:
            return true;
        default:
            return false;
    }
}

// OoT's own items 2Ship carries as mods, or natively under a fleet.* key: Fleet pairs equal keys.
const char* ReplicaKey(RandomizerGet item) {
    static const std::unordered_map<RandomizerGet, const char*> keys = {
        { RG_BOOMERANG, "oot.boomerang" },
        { RG_DINS_FIRE, "oot.dins_fire" },
        { RG_FARORES_WIND, "oot.farores_wind" },
        { RG_NAYRUS_LOVE, "oot.nayrus_love" },
        { RG_MEGATON_HAMMER, "oot.hammer" },
        { RG_PROGRESSIVE_SLINGSHOT, "oot.slingshot.progressive" },
        { RG_ZELDAS_LULLABY, "oot.song.lullaby" },
        { RG_MINUET_OF_FOREST, "oot.song.minuet" },
        { RG_BOLERO_OF_FIRE, "oot.song.bolero" },
        { RG_SERENADE_OF_WATER, "oot.song.serenade" },
        { RG_REQUIEM_OF_SPIRIT, "oot.song.requiem" },
        { RG_NOCTURNE_OF_SHADOW, "oot.song.nocturne" },
        { RG_PRELUDE_OF_LIGHT, "oot.song.prelude" },
        { RG_MASTER_SWORD, "oot.equip.master_sword" },
        { RG_BIGGORON_SWORD, "oot.equip.biggoron_sword" },
        { RG_DEKU_SHIELD, "oot.equip.deku_shield" },
        { RG_MIRROR_SHIELD, "oot.equip.mirror_shield" },
        { RG_GORON_TUNIC, "oot.equip.goron_tunic" },
        { RG_ZORA_TUNIC, "oot.equip.zora_tunic" },
        { RG_IRON_BOOTS, "oot.equip.iron_boots" },
        { RG_HOVER_BOOTS, "oot.equip.hover_boots" },
        { RG_EMPTY_BOTTLE, "fleet.bottle.empty" },
        { RG_BOTTLE_WITH_MILK, "fleet.bottle.milk" },
        { RG_BOTTLE_WITH_RED_POTION, "fleet.bottle.red_potion" },
    };
    const auto found = keys.find(item);
    return found != keys.end() ? found->second : nullptr;
}

// The pool viewer's section; coarser than the fill categories and shared with 2Ship.
const char* PoolGroup(RandomizerGet item) {
    if (item == RG_ICE_TRAP) {
        return "traps";
    }
    if (IsAbility(item)) {
        return "abilities";
    }
    Rando::Item& staticItem = Rando::StaticData::RetrieveItem(item);
    const ItemType type = staticItem.GetItemType();
    switch (type) {
        case ITEMTYPE_MAP:
        case ITEMTYPE_COMPASS:
        case ITEMTYPE_BOSSKEY:
        case ITEMTYPE_SMALLKEY:
        case ITEMTYPE_FORTRESS_SMALLKEY:
        case ITEMTYPE_SILVER:
        case ITEMTYPE_TOKEN:
            return "keys";
        default:
            break;
    }
    if (staticItem.GetCategory() == ITEM_CATEGORY_HEALTH) {
        return "health";
    }
    if (IsAdvancement(item)) {
        return "progression";
    }
    if (staticItem.GetCategory() == ITEM_CATEGORY_JUNK || type == ITEMTYPE_REFILL || type == ITEMTYPE_DROP) {
        return "consumables";
    }
    return "useful";
}

const std::string& ItemName(RandomizerGet item) {
    return Rando::StaticData::RetrieveItem(item).GetName().GetEnglish();
}

// Every randomizer item: the native table, then each mod's.
std::vector<RandomizerGet> RegisteredItems() {
    std::vector<RandomizerGet> items;
    for (int rg = 0; rg < RG_MAX; rg++) {
        items.push_back(static_cast<RandomizerGet>(rg));
    }
    for (uint32_t index = 0; index < RandoItems_GetCount(); index++) {
        items.push_back(static_cast<RandomizerGet>(RandoItems_GetAt(index)));
    }
    return items;
}

// A mod item's pool name is its key; its definition holds the name the player reads.
std::string DisplayName(RandomizerGet item) {
    const char* key = RandoItems_GetKey(item);
    const auto* definition = key != nullptr ? CustomItemRegistry_Find(key) : nullptr;
    if (definition != nullptr && definition->displayName != nullptr && definition->displayName[0] != '\0') {
        return definition->displayName;
    }
    return ItemName(item);
}

struct StartingOption {
    RandomizerSettingKey key;
    RandomizerGet item;
};

// Ship's starting options whose value is the count of `item` the file starts with (starting_inventory.cpp).
constexpr StartingOption kStartingOptions[] = {
    { RSK_STARTING_KOKIRI_SWORD, RG_KOKIRI_SWORD },
    { RSK_STARTING_MASTER_SWORD, RG_MASTER_SWORD },
    { RSK_STARTING_BIGGORON_SWORD, RG_BIGGORON_SWORD },
    { RSK_STARTING_DEKU_SHIELD, RG_DEKU_SHIELD },
    { RSK_STARTING_HYLIAN_SHIELD, RG_HYLIAN_SHIELD },
    { RSK_STARTING_MIRROR_SHIELD, RG_MIRROR_SHIELD },
    { RSK_STARTING_GORON_TUNIC, RG_GORON_TUNIC },
    { RSK_STARTING_ZORA_TUNIC, RG_ZORA_TUNIC },
    { RSK_STARTING_IRON_BOOTS, RG_IRON_BOOTS },
    { RSK_STARTING_HOVER_BOOTS, RG_HOVER_BOOTS },
    { RSK_STARTING_OCARINA, RG_PROGRESSIVE_OCARINA },
    { RSK_STARTING_SLINGSHOT, RG_PROGRESSIVE_SLINGSHOT },
    { RSK_STARTING_BOW, RG_PROGRESSIVE_BOW },
    { RSK_STARTING_BOMB_BAG, RG_PROGRESSIVE_BOMB_BAG },
    { RSK_STARTING_BOMBCHU_BAG, RG_PROGRESSIVE_BOMBCHU_BAG },
    { RSK_STARTING_HOOKSHOT, RG_PROGRESSIVE_HOOKSHOT },
    { RSK_STARTING_BOOMERANG, RG_BOOMERANG },
    { RSK_STARTING_LENS_OF_TRUTH, RG_LENS_OF_TRUTH },
    { RSK_STARTING_MEGATON_HAMMER, RG_MEGATON_HAMMER },
    { RSK_STARTING_BEANS, RG_MAGIC_BEAN_PACK },
    { RSK_STARTING_FIRE_ARROWS, RG_FIRE_ARROWS },
    { RSK_STARTING_ICE_ARROWS, RG_ICE_ARROWS },
    { RSK_STARTING_LIGHT_ARROWS, RG_LIGHT_ARROWS },
    { RSK_STARTING_DINS_FIRE, RG_DINS_FIRE },
    { RSK_STARTING_FARORES_WIND, RG_FARORES_WIND },
    { RSK_STARTING_NAYRUS_LOVE, RG_NAYRUS_LOVE },
    { RSK_STARTING_STRENGTH, RG_PROGRESSIVE_STRENGTH },
    { RSK_STARTING_SCALE, RG_PROGRESSIVE_SCALE },
    { RSK_STARTING_WALLET, RG_PROGRESSIVE_WALLET },
    { RSK_STARTING_MAGIC_METER, RG_PROGRESSIVE_MAGIC_METER },
    { RSK_STARTING_STONE_OF_AGONY, RG_STONE_OF_AGONY },
    { RSK_STARTING_GERUDO_CARD, RG_GERUDO_MEMBERSHIP_CARD },
    { RSK_STARTING_WEIRD_EGG, RG_WEIRD_EGG },
    { RSK_STARTING_ZELDAS_LETTER, RG_ZELDAS_LETTER },
    { RSK_STARTING_CLAIM_CHECK, RG_CLAIM_CHECK },
    { RSK_STARTING_KEATON_MASK, RG_KEATON_MASK },
    { RSK_STARTING_SKULL_MASK, RG_SKULL_MASK },
    { RSK_STARTING_SPOOKY_MASK, RG_SPOOKY_MASK },
    { RSK_STARTING_BUNNY_HOOD, RG_BUNNY_HOOD },
    { RSK_STARTING_GORON_MASK, RG_GORON_MASK },
    { RSK_STARTING_ZORA_MASK, RG_ZORA_MASK },
    { RSK_STARTING_GERUDO_MASK, RG_GERUDO_MASK },
    { RSK_STARTING_MASK_OF_TRUTH, RG_MASK_OF_TRUTH },
    { RSK_STARTING_ZELDAS_LULLABY, RG_ZELDAS_LULLABY },
    { RSK_STARTING_EPONAS_SONG, RG_EPONAS_SONG },
    { RSK_STARTING_SARIAS_SONG, RG_SARIAS_SONG },
    { RSK_STARTING_SUNS_SONG, RG_SUNS_SONG },
    { RSK_STARTING_SONG_OF_TIME, RG_SONG_OF_TIME },
    { RSK_STARTING_SONG_OF_STORMS, RG_SONG_OF_STORMS },
    { RSK_STARTING_SCARECROWS_SONG, RG_SCARECROWS_SONG },
    { RSK_STARTING_MINUET_OF_FOREST, RG_MINUET_OF_FOREST },
    { RSK_STARTING_BOLERO_OF_FIRE, RG_BOLERO_OF_FIRE },
    { RSK_STARTING_SERENADE_OF_WATER, RG_SERENADE_OF_WATER },
    { RSK_STARTING_REQUIEM_OF_SPIRIT, RG_REQUIEM_OF_SPIRIT },
    { RSK_STARTING_NOCTURNE_OF_SHADOW, RG_NOCTURNE_OF_SHADOW },
    { RSK_STARTING_PRELUDE_OF_LIGHT, RG_PRELUDE_OF_LIGHT },
    { RSK_STARTING_SKULLTULA_TOKEN, RG_GOLD_SKULLTULA_TOKEN },
};

constexpr RandomizerSettingKey kStartingBottles[] = { RSK_STARTING_BOTTLE_1, RSK_STARTING_BOTTLE_2,
                                                      RSK_STARTING_BOTTLE_3, RSK_STARTING_BOTTLE_4 };

const char* StartingCVar(RandomizerSettingKey key) {
    return Rando::Settings::GetInstance()->GetOption(key).GetCVarName().c_str();
}

int StartingMax(RandomizerSettingKey key) {
    return static_cast<int>(Rando::Settings::GetInstance()->GetOption(key).GetOptionCount()) - 1;
}

int StartingValue(RandomizerSettingKey key) {
    Rando::Option& option = Rando::Settings::GetInstance()->GetOption(key);
    return CVarGetInteger(option.GetCVarName().c_str(), option.GetMenuOptionDefault());
}

int StartingBottleCount() {
    int bottles = 0;
    for (RandomizerSettingKey key : kStartingBottles) {
        bottles += StartingValue(key) == RO_STARTING_BOTTLE_EMPTY;
    }
    return bottles;
}

nlohmann::json StartingEntry(RandomizerGet item, int max, int count) {
    return { { "name", ItemName(item) }, { "group", PoolGroup(item) }, { "max", max }, { "count", count } };
}

// The native starting options as one item list, so Fleet offers them beside 2Ship's.
nlohmann::json StartingItems() {
    nlohmann::json items = nlohmann::json::array();
    for (const StartingOption& option : kStartingOptions) {
        items.push_back(StartingEntry(option.item, StartingMax(option.key), StartingValue(option.key)));
    }
    items.push_back(
        StartingEntry(RG_EMPTY_BOTTLE, static_cast<int>(std::size(kStartingBottles)), StartingBottleCount()));
    return items;
}

// Empty bottles fill the bottle slots in order; a slot holding Ruto's Letter or a Big Poe keeps it.
void SetStartingBottles(int count) {
    for (RandomizerSettingKey key : kStartingBottles) {
        const int value = StartingValue(key);
        if (value != RO_STARTING_BOTTLE_OFF && value != RO_STARTING_BOTTLE_EMPTY) {
            continue;
        }
        CVarSetInteger(StartingCVar(key), count > 0 ? RO_STARTING_BOTTLE_EMPTY : RO_STARTING_BOTTLE_OFF);
        count--;
    }
}

void SetStartingItem(const nlohmann::json& input) {
    if (!input.is_object()) {
        return;
    }
    const std::string name = input.value("name", std::string{});
    const int count = std::max(input.value("count", 0), 0);
    if (name == ItemName(RG_EMPTY_BOTTLE)) {
        SetStartingBottles(count);
    }
    for (const StartingOption& option : kStartingOptions) {
        if (name == ItemName(option.item)) {
            CVarSetInteger(StartingCVar(option.key), std::min(count, StartingMax(option.key)));
        }
    }
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

nlohmann::json PoolEntry(RandomizerGet item) {
    return { { "name", ItemName(item) },
             { "advancement", IsAdvancement(item) },
             { "major", Rando::StaticData::RetrieveItem(item).IsMajorItem() },
             { "category", CategoryName(item) },
             { "group", PoolGroup(item) } };
}

// Link's Pocket is not a check the dump offers, so the combined fill must still own whatever it holds.
nlohmann::json ForcedPlacements() {
    auto ctx = Rando::Context::GetInstance();
    nlohmann::json forced = nlohmann::json::object();
    const RandomizerGet reward = ctx->GetItemLocation(RC_LINKS_POCKET)->GetPlacedRandomizerGet();
    Rando::OptionValue& pocket = ctx->GetOption(RSK_LINKS_POCKET);
    if (reward != RG_NONE) {
        forced["Link's Pocket"] = { { "item", ItemName(reward) },
                                    { "itemCategory", CategoryName(reward) },
                                    { "group", PoolGroup(reward) } };
    } else if (pocket.Is(RO_LINKS_POCKET_ADVANCEMENT)) {
        forced["Link's Pocket"]["category"] = "advancement";
    } else if (pocket.Is(RO_LINKS_POCKET_ANYTHING)) {
        forced["Link's Pocket"]["category"] = "any";
    }
    return forced;
}

// The cross-game entrance services; nullopt for any other request.
std::optional<nlohmann::json> HandleCrossDungeonRequest(const std::string& request, const nlohmann::json& input) {
    if (request == "DumpCrossDungeonCatalogue") {
        return FleetCrossDungeon_DumpCatalogue(input);
    }
    if (request == "DumpCrossDungeonCatalogueForSeed") {
        return FleetCrossDungeon_DumpCatalogueForSeed(input);
    }
    if (request == "SetCrossDungeonSeed") {
        FleetCrossDungeon_SetSeed(input);
        return nlohmann::json::object();
    }
    if (request == "SetCrossDungeonAccess") {
        FleetCrossDungeon_SetAccess(input);
        return nlohmann::json::object();
    }
    if (request == "GetCrossBossDefeated") {
        return nlohmann::json{ { "defeated", FleetCrossDungeon_GetBossDefeated() } };
    }
    if (request == "GetCrossDungeonExits") {
        return FleetCrossDungeon_GetExits();
    }
    return std::nullopt;
}

} // namespace

char* FleetRando_DumpStaticData(void) {
    auto ctx = Rando::Context::GetInstance();
    if (!PrepareSeed(EntranceLayout::Roll)) {
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
                              { "category", CategoryName(placed) },
                              { "group", PoolGroup(placed) },
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
    for (const std::string& name : sFleetStartingItems) {
        const auto found = std::find_if(pool.begin(), pool.end(),
                                        [&](const nlohmann::json& entry) { return entry.value("name", "") == name; });
        if (found != pool.end()) {
            pool.erase(found);
        }
    }
    // Shop slots are not in itemPool; the native fill pads them with junk the same way.
    while (pool.size() < checks.size()) {
        pool.push_back(PoolEntry(GetJunkItem()));
    }

    nlohmann::json items = nlohmann::json::array();
    for (const RandomizerGet item : RegisteredItems()) {
        const std::string& name = ItemName(item);
        if (name.empty()) {
            continue;
        }
        items.push_back({ { "name", name },
                          { "displayName", DisplayName(item) },
                          { "advancement", IsAdvancement(item) },
                          { "trap", item == RG_ICE_TRAP },
                          { "trickNames", nlohmann::json::array() } });
        if (const char* replica = ReplicaKey(item); replica != nullptr) {
            items.back()["pairKey"] = replica;
        }
        if (const char* key = RandoItems_GetKey(item); key != nullptr) {
            items.back()["textKey"] = key;
            items.back()["group"] = PoolGroup(item);
        }
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
    FleetRando_SetOwnedItems(nlohmann::json(sFleetStartingItems).dump().c_str());
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
    std::vector<RandomizerCheck> reachable;
    try {
        FleetCrossDungeonSearch crossRoutes;
        reachable = ReachabilitySearch(ctx->allLocations);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Fleet] Ship could not search the cross-game routes: {}", e.what());
    }
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
    if (request == "RuntimeStatus") {
        return CopyString(FleetRuntime_Status().dump());
    }
    if (request == "DumpSettingDefinitions") {
        auto definitions = nlohmann::json::array();
        for (const auto& option : Rando::Settings::GetInstance()->GetAllOptions()) {
            if (option.GetCVarName().empty() || option.GetOptionCount() == 0) {
                continue;
            }
            auto choices = nlohmann::json::array();
            for (size_t i = 0; i < option.GetOptionCount(); i++) {
                choices.push_back({ { "value", i }, { "name", option.GetOptionText(i) } });
            }
            definitions.push_back({ { "key", option.GetCVarName() },
                                    { "name", option.GetName() },
                                    { "description", option.GetDescription() },
                                    { "choices", choices } });
        }
        return CopyString(definitions.dump());
    }
    if (request == "RuntimeCollectCheck") {
        const auto input = nlohmann::json::parse(json != nullptr ? json : "{}", nullptr, false);
        return CopyString(nlohmann::json{
            { "collected", input.is_object() && FleetRuntime_CollectForSmoke(input.value("check", "")) } }
                              .dump());
    }
    const nlohmann::json input = nlohmann::json::parse(json != nullptr ? json : "{}", nullptr, false);
    if (request == "MenuModel") {
        return CopyString(SohGui::mSohMenu ? SohGui::mSohMenu->FleetMenuModel() : "null");
    }
    if (request == "GenerateSeedTest") {
        const auto test = reinterpret_cast<bool (*)()>(FleetGame_GetHostFunction("IsRuntimeTest"));
        if (test == nullptr || !test() || gGameState == nullptr || gGameState->main != FileChoose_Main) {
            throw std::runtime_error("native generation test requires the OoT file selector");
        }
        auto* files = reinterpret_cast<FileChooseContext*>(gGameState);
        files->buttonIndex = 0;
        files->randomizerIndex = RSM_GENERATE_RANDOMIZER;
        files->questType[0] = QUEST_RANDOMIZER;
        files->state.input[0].press.button = BTN_A;
        FileChoose_UpdateRandomizerMenu(gGameState);
        files->state.input[0].press.button = 0;
        if (FleetGame_RandomizerGenerationState() != 1) {
            throw std::runtime_error("A did not start native Fleet generation");
        }
        return CopyString("{}");
    }
    if (request == "CreateNamedSaveTest") {
        const auto test = reinterpret_cast<bool (*)()>(FleetGame_GetHostFunction("IsRuntimeTest"));
        if (test == nullptr || !test() || !FleetGame_HasGeneratedSeed(0) || gGameState == nullptr ||
            gGameState->main != FileChoose_Main) {
            throw std::runtime_error("native name-entry test requires an empty file-select slot");
        }
        auto* files = reinterpret_cast<FileChooseContext*>(gGameState);
        files->buttonIndex = 0;
        files->questType[0] = QUEST_RANDOMIZER;
        const std::array<unsigned char, 8> namePal = { 0x0f, 0x15, 0x0e, 0x0e, 0x1d, 0x3e, 0x3e, 0x3e };
        const std::array<unsigned char, 8> nameNtsc = { 0xb0, 0xb6, 0xaf, 0xaf, 0xbe, 0xdf, 0xdf, 0xdf };
        const auto& name = ResourceMgr_GetGameRegion(0) == GAME_REGION_PAL ? namePal : nameNtsc;
        std::copy(name.begin(), name.end(), SaveManager::Instance->fileMetaInfo[0].playerName);
        gSaveContext.fileNum = 0;
        Sram_InitSave(files);
        return CopyString("{}");
    }
    if (request == "ApplySetting") {
        ShipInit::Init(input.at("key").get<std::string>());
        return CopyString("{}");
    }
    if (request == "MenuDrawWidget") {
        if (SohGui::mSohMenu && input.is_object()) {
            SohGui::mSohMenu->FleetDrawWidget(input.at("section"), input.at("sidebar"), input.at("column"),
                                              input.at("index"));
        }
        return CopyString("{}");
    }
    // Fleet's own confirmations (a seed dropped on the window) in the game the player is looking at.
    if (request == "Notify") {
        Notification::Emit({ .message = input.value("message", std::string{}) });
        Audio_PlaySfxGeneral(NA_SE_SY_CORRECT_CHIME, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return CopyString("{}");
    }
    // Fleet's item test: every randomizer item this game can hand out, by pool name. The session's stand-ins for
    // the other game's items ("fleet:" keys) are its owner's to draw, not this game's.
    if (request == "ListItems") {
        nlohmann::json items = nlohmann::json::array();
        for (const RandomizerGet item : RegisteredItems()) {
            const char* key = RandoItems_GetKey(item);
            if (!ItemName(item).empty() && (key == nullptr || std::strncmp(key, "fleet:", 6) != 0)) {
                items.push_back({ { "name", ItemName(item) }, { "label", DisplayName(item) } });
            }
        }
        return CopyString(items.dump());
    }
    if (request == "MenuDrawPage") {
        if (SohGui::mSohMenu && input.is_object()) {
            SohGui::mSohMenu->FleetDrawPage(input.at("section"), input.at("sidebar"));
        }
        return CopyString("{}");
    }
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
    if (request == "StartingItems") {
        return CopyString(StartingItems().dump());
    }
    if (request == "SetStartingItem") {
        SetStartingItem(input);
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
    // A saved seed's prices win over this preparation's rolls, so wallet logic judges what the player pays.
    if (request == "SetCheckPrices") {
        sPrices.clear();
        for (const auto& [name, price] : input.items()) {
            auto check = Rando::StaticData::locationNameToEnum.find(name);
            if (check != Rando::StaticData::locationNameToEnum.end() && price.is_number_integer()) {
                sPrices[check->second] = price.get<uint16_t>();
            }
        }
        return CopyString("{}");
    }
    if (request == "DumpHintData") {
        // Authored hint text may hold malformed UTF-8; replacing it keeps the dump from throwing across the ABI.
        return CopyString(FleetHints_DumpHintData().dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
    }
    if (request == "GetForcedPlacements") {
        return CopyString(ForcedPlacements().dump());
    }
    // Describes the owned set of the latest GetReachableChecks.
    if (request == "GetPortalOpen") {
        return CopyString(nlohmann::json{ { "open", sPortalOpen } }.dump());
    }
    try {
        if (const std::optional<nlohmann::json> reply = HandleCrossDungeonRequest(request, input)) {
            return CopyString(reply->dump());
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Fleet] Ship rejected '{}': {}", request, e.what());
        return nullptr;
    }
    SPDLOG_WARN("[Fleet] Ship has no randomizer service '{}'", request);
    return nullptr;
}

void FleetRando_CreateCombinedSave(const nlohmann::json& seed, int slot) {
    struct RestorePlayerSettings {
        nlohmann::json snapshot = DumpSettings();
        ~RestorePlayerSettings() {
            RestoreSettings(snapshot);
        }
    } restorePlayerSettings;
    RestoreSettings(seed.at("oot"));
    sSeed = seed.at("masterSeed").get<uint32_t>();
    const auto goal = seed.at("goal");
    sComboOptions.hunt = goal.value("type", "bosses") == "triforceHunt";
    sComboOptions.required = static_cast<uint8_t>(goal.value("requiredPieces", 0));
    sComboOptions.mmStart = seed.value("startingGame", "OOT") == "MM";
    const auto shared = seed.value("sharedItems", nlohmann::json::array());
    sComboOptions.sharedWallets = std::find(shared.begin(), shared.end(), "wallets") != shared.end();
    const int totalPieces = goal.value("totalPieces", 0);
    sComboOptions.pieces = totalPieces - totalPieces / 2;
    Sram_InitNewSave();
    gSaveContext.fileNum = slot;
    if (!PrepareSeed(EntranceLayout::FromSeed)) {
        throw std::runtime_error("could not prepare OoT combined save");
    }
    auto context = Rando::Context::GetInstance();
    if (DumpMasterQuestDungeons() != seed.at("oot").at("masterQuestDungeons")) {
        throw std::runtime_error("OoT dungeon layouts do not match the combined seed");
    }
    nlohmann::json locations = seed.at("oot").at("placements");
    for (auto& [check, placed] : locations.items()) {
        std::string item = placed.get<std::string>();
        for (const char* suffix : { " (OOT)", " (MM)" }) {
            const size_t size = std::strlen(suffix);
            if (item.size() >= size && item.compare(item.size() - size, size, suffix) == 0) {
                item.resize(item.size() - size);
            }
        }
        placed = item;
    }
    FleetForeignItems_Register(seed);
    for (const auto& foreign : seed.at("foreign")) {
        if (foreign.at("checkGame") == "oot") {
            locations[foreign.at("checkName").get<std::string>()] = FleetForeignItems_Key(foreign);
        }
    }
    for (const auto& [check, item] : locations.items()) {
        if (!FleetRando_PlaceItem(check.c_str(), item.get_ref<const std::string&>().c_str())) {
            throw std::runtime_error("unknown OoT placement: " + check + " -> " + item.get<std::string>());
        }
    }
    const auto prices = seed.at("oot").value("prices", nlohmann::json::object());
    for (const auto& [name, price] : prices.items()) {
        context->GetItemLocation(Rando::StaticData::locationNameToEnum.at(name))->SetCustomPrice(price.get<uint16_t>());
    }
    context->GetEntranceShuffler()->ParseJson({ { "entrances", seed.at("entrances").at("oot") } });
    context->SetSeed(sSeed);
    context->SetSeedString(std::to_string(seed.at("seed").get<uint32_t>()));
    context->CreateItemOverrides();
    context->SetSeedGenerated();
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    const uint8_t playerName[8] = { 0x15, 0x2C, 0x31, 0x2E, 0x3E, 0x3E, 0x3E, 0x3E };
    std::memcpy(gSaveContext.playerName, playerName, sizeof(playerName));
    std::memcpy(gSaveContext.newf, "ZELDAZ", sizeof(gSaveContext.newf));
    gSaveContext.entranceIndex = ENTR_LINKS_HOUSE_CHILD_SPAWN;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.dayTime = 0x6AAB;
    Randomizer_InitSaveFile();
    FleetRuntime_Save();
}

void FleetRando_FreeString(char* text) {
    std::free(text);
}

extern "C" bool FleetGame_CreateNamedSave(int slot, const unsigned char* name, int language) {
    const auto begin = reinterpret_cast<bool (*)(int)>(FleetGame_GetHostFunction("BeginCreatedSave"));
    if (begin == nullptr || !begin(slot)) {
        return false;
    }
    std::array<unsigned char, 8> playerName;
    std::copy_n(name, playerName.size(), playerName.begin());
    FleetRuntime_HasSession();
    FleetRando_CreateCombinedSave(FleetRuntime_Seed(), slot);
    std::copy(playerName.begin(), playerName.end(), gSaveContext.playerName);
    gSaveContext.ship.filenameLanguage = language;
    FleetRuntime_Save();
    SaveManager::Instance->ThreadPoolWait();
    const auto finish = reinterpret_cast<void (*)()>(FleetGame_GetHostFunction("FinishCreatedSave"));
    if (finish != nullptr) {
        finish();
    }
    return true;
}
