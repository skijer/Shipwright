#include "FleetCrossDungeon.h"

#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <fleet/rando/CrossDungeonCatalogue.h>
#include <fleet/rando/CrossDungeonState.h>

#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
#include "soh/cvar_prefixes.h"
#include "z64scene.h"

namespace {

using namespace ComboRando::CrossDungeon;

constexpr int kShip = 0;

HostState sState(kShip);
// Region each native entrance index lands in, and the native shuffle's own overrides.
std::map<int32_t, RandomizerRegion> sRegions;
std::map<int32_t, int32_t> sNativeOverrides;
bool sSearching = false;

std::map<int32_t, int32_t> ActiveNativeOverrides() {
    std::map<int32_t, int32_t> overrides;
    for (const EntranceOverride& entry : Rando::Context::GetInstance()->GetEntranceShuffler()->entranceOverrides) {
        if (entry.type == 0 && entry.index == 0 && entry.override == 0) {
            break;
        }
        overrides[entry.index] = entry.override;
    }
    return overrides;
}

// The immutable table also covers boss doors and grotto indices without re-initializing a shuffled graph.
void IndexRegions() {
    auto edges = GetShuffleableEntrances(Rando::EntranceType::All, false);
    if (edges.empty()) {
        Rando::SetAllEntrancesData();
        edges = GetShuffleableEntrances(Rando::EntranceType::All, false);
    }
    sRegions.clear();
    for (const auto& pair : Rando::GetEntranceShuffleTable()) {
        for (const Rando::EntranceLinkInfo& link : { pair.first, pair.second }) {
            if (link.index >= 0 && link.connectedRegion != RR_NONE) {
                sRegions[link.index] = link.connectedRegion;
            }
        }
    }
    sNativeOverrides = ActiveNativeOverrides();
    for (Rando::Entrance* edge : edges) {
        if (edge->GetIndex() >= 0) {
            sRegions[edge->GetIndex()] = edge->GetOriginalConnectedRegionKey();
        }
    }
}

int32_t ResolvedIndex(const Rando::Entrance& entrance) {
    const int32_t index = entrance.GetIndex();
    const Rando::Entrance* replacement = entrance.GetReplacement();
    return ResolveNativeIndex(index, replacement != nullptr ? replacement->GetIndex() : index, sNativeOverrides);
}

// A route into the other game leaves Ship's graph: the edge leads nowhere for Ship's own logic.
RandomizerRegion RerouteDuringSearch(const Rando::Entrance& entrance, RandomizerRegion native) {
    if (!sSearching) {
        return native;
    }
    const Route* route = sState.Lookup(entrance.GetIndex(), ResolvedIndex(entrance));
    if (route == nullptr) {
        return native;
    }
    if (route->to.game != kShip) {
        return RR_NONE;
    }
    auto region = sRegions.find(route->to.index);
    return region == sRegions.end() ? RR_NONE : region->second;
}

// Cross routing moves dungeon mouths even with native dungeon shuffle off; native age and door rules must
// then apply as if it were on. Returns the setting to restore, if it changed.
std::optional<uint8_t> WidenDungeonRules() {
    auto& option = Rando::Context::GetInstance()->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES);
    if (!sState.HasPool(Pool::Dungeon) || option.Get() != RO_DUNGEON_ENTRANCE_SHUFFLE_OFF) {
        return std::nullopt;
    }
    option.Set(RO_DUNGEON_ENTRANCE_SHUFFLE_ON);
    return RO_DUNGEON_ENTRANCE_SHUFFLE_OFF;
}

void RestoreDungeonRules(std::optional<uint8_t> previous) {
    if (previous) {
        Rando::Context::GetInstance()->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).Set(*previous);
    }
}

bool IsRetailShopEntrance(int32_t entrance) {
    switch (entrance) {
        case ENTR_KOKIRI_SHOP_0:
        case ENTR_GORON_SHOP_0:
        case ENTR_ZORA_SHOP_0:
        case ENTR_BAZAAR_0:
        case ENTR_BAZAAR_1:
        case ENTR_POTION_SHOP_MARKET_0:
        case ENTR_BOMBCHU_SHOP_1:
        case ENTR_POTION_SHOP_GRANNY_0:
        case ENTR_POTION_SHOP_KAKARIKO_FRONT:
        case ENTR_POTION_SHOP_KAKARIKO_BACK:
            return true;
        default:
            return false;
    }
}

// Native ingress identities, not destination scenes: alternate doors stay separate. Open houses, tents, fairy
// caves and the Temple of Time are passages; the Mask Shop stays the portal between games.
InteriorKind InteriorKindForEntrance(int32_t entrance) {
    switch (entrance) {
        case ENTR_KOKIRI_SHOP_0:
        case ENTR_GORON_SHOP_0:
        case ENTR_ZORA_SHOP_0:
            return InteriorKind::ShopPassage;
        case ENTR_LAKESIDE_LABORATORY_0:
        case ENTR_FISHING_POND_0:
        case ENTR_MARKET_GUARD_HOUSE_0:
        case ENTR_BOMBCHU_BOWLING_ALLEY_0:
        case ENTR_TREASURE_BOX_SHOP_0:
        case ENTR_BACK_ALLEY_MAN_IN_GREEN_HOUSE:
        case ENTR_KAKARIKO_CENTER_GUEST_HOUSE_0:
        case ENTR_HOUSE_OF_SKULLTULA_0:
        case ENTR_IMPAS_HOUSE_FRONT:
        case ENTR_IMPAS_HOUSE_BACK:
        case ENTR_GRAVEKEEPERS_HUT_0:
        case ENTR_LON_LON_BUILDINGS_TALONS_HOUSE:
        case ENTR_STABLE_0:
        case ENTR_LON_LON_BUILDINGS_TOWER:
        case ENTR_SHOOTING_GALLERY_0:
        case ENTR_SHOOTING_GALLERY_1:
        case ENTR_WINDMILL_AND_DAMPES_GRAVE_WINDMILL:
            return InteriorKind::Door;
        default:
            return IsRetailShopEntrance(entrance) ? InteriorKind::ShopDoor : InteriorKind::Passage;
    }
}

bool IsInteriorEntrance(const Rando::EntranceLinkInfo& link) {
    return link.type == Rando::EntranceType::Interior || link.type == Rando::EntranceType::SpecialInterior;
}

// After native ER an interior may be reached through any physical source; an open or ambiguous source never
// grants the narrower door or shop kind.
std::map<int32_t, InteriorKind> PhysicalInteriorKinds(const PoolOptions& options,
                                                      const std::map<int32_t, int32_t>& overrides) {
    std::map<int32_t, InteriorKind> kinds;
    const auto record = [&](int32_t source, InteriorKind kind) {
        if (source < 0) {
            return;
        }
        auto [it, inserted] = kinds.emplace(ResolveNativeIndex(source, source, overrides), kind);
        if (!inserted && it->second != kind) {
            it->second = InteriorKind::Passage;
        }
    };
    for (const auto& pair : Rando::GetEntranceShuffleTable()) {
        const auto& source = pair.first;
        const bool ingress = pair.second.index >= 0 && source.index != ENTR_HAPPY_MASK_SHOP_0 && IsInteriorEntrance(source);
        const InteriorKind kind = ingress ? InteriorKindForEntrance(source.index) : InteriorKind::Passage;
        record(source.index,
               options.legacyInteriors && kind == InteriorKind::ShopPassage ? InteriorKind::Passage : kind);
        record(pair.second.index, InteriorKind::Passage);
    }
    return kinds;
}

std::optional<Pool> PoolFor(const Rando::EntranceLinkInfo& entry, bool includeGanon, const PoolOptions& options,
                            const std::map<int32_t, InteriorKind>& physicalKinds) {
    const bool dungeon = entry.type == Rando::EntranceType::Dungeon ||
                         (includeGanon && entry.type == Rando::EntranceType::GanonDungeon);
    if (options.dungeons && dungeon) {
        return Pool::Dungeon;
    }
    if (IsInteriorEntrance(entry)) {
        const auto physical = physicalKinds.find(entry.index);
        InteriorKind kind = physical == physicalKinds.end() ? InteriorKind::Passage : physical->second;
        if (!options.legacyInteriors) {
            if (IsRetailShopEntrance(entry.index)) {
                kind = InteriorKindForEntrance(entry.index);
            } else if (kind == InteriorKind::ShopDoor || kind == InteriorKind::ShopPassage) {
                kind = InteriorKind::Passage;
            }
        }
        return InteriorPool(kind, options);
    }
    if (options.grottos && entry.type == Rando::EntranceType::GrottoGrave) {
        return Pool::Grotto;
    }
    return std::nullopt;
}

std::vector<Dungeon> BaseCatalogue(bool includeGanon, const PoolOptions& options,
                                   const std::map<int32_t, int32_t>& overrides) {
    const bool needsPhysicalKinds = !options.interiors && (options.doorInteriors || (options.legacyInteriors && options.shops));
    const std::map<int32_t, InteriorKind> physicalKinds =
        needsPhysicalKinds ? PhysicalInteriorKinds(options, overrides) : std::map<int32_t, InteriorKind>{};
    std::vector<Dungeon> base;
    for (const auto& pair : Rando::GetEntranceShuffleTable()) {
        const auto& entry = pair.first;
        if (entry.index == ENTR_HAPPY_MASK_SHOP_0 || pair.second.index < 0) {
            continue;
        }
        const std::optional<Pool> pool = PoolFor(entry, includeGanon, options, physicalKinds);
        if (!pool) {
            continue;
        }
        Dungeon portal{ "oot:" + std::to_string(entry.index),
                        { kShip, entry.index },
                        { kShip, pair.second.index },
                        { kShip, pair.second.index } };
        portal.pool = *pool;
        base.push_back(std::move(portal));
    }
    return base;
}

const std::map<int32_t, int32_t>& BlueWarpExits() {
    static const std::map<int32_t, int32_t> warps = {
        { ENTR_KOKIRI_FOREST_DEKU_TREE_BLUE_WARP, ENTR_KOKIRI_FOREST_OUTSIDE_DEKU_TREE },
        { ENTR_DEATH_MOUNTAIN_TRAIL_DODONGO_BLUE_WARP, ENTR_DEATH_MOUNTAIN_TRAIL_OUTSIDE_DODONGOS_CAVERN },
        { ENTR_ZORAS_FOUNTAIN_JABU_JABU_BLUE_WARP, ENTR_ZORAS_FOUNTAIN_OUTSIDE_JABU_JABU },
        { ENTR_SACRED_FOREST_MEADOW_FOREST_TEMPLE_BLUE_WARP, ENTR_SACRED_FOREST_MEADOW_OUTSIDE_TEMPLE },
        { ENTR_DEATH_MOUNTAIN_CRATER_FIRE_TEMPLE_BLUE_WARP, ENTR_DEATH_MOUNTAIN_CRATER_OUTSIDE_TEMPLE },
        { ENTR_LAKE_HYLIA_WATER_TEMPLE_BLUE_WARP, ENTR_LAKE_HYLIA_OUTSIDE_TEMPLE },
        { ENTR_DESERT_COLOSSUS_SPIRIT_TEMPLE_BLUE_WARP, ENTR_DESERT_COLOSSUS_OUTSIDE_TEMPLE },
        { ENTR_GRAVEYARD_SHADOW_TEMPLE_BLUE_WARP, ENTR_GRAVEYARD_OUTSIDE_TEMPLE },
        { ENTR_OUTSIDE_GANONS_CASTLE_1_2, ENTR_CASTLE_GROUNDS_RAINBOW_BRIDGE_EXIT },
    };
    return warps;
}

// Dungeons compose with their blue warps first; every other portal returns where native ER sends its exit.
nlohmann::json Catalogue(bool includeGanon, const std::map<int32_t, int32_t>& overrides, const PoolOptions& options) {
    if (options.bosses) {
        throw std::invalid_argument("cross-game boss entrances are not served by Ship yet");
    }
    std::vector<Dungeon> catalogue = BaseCatalogue(includeGanon, options, overrides);
    std::vector<Dungeon> dungeons;
    for (const Dungeon& entry : catalogue) {
        if (entry.pool == Pool::Dungeon) {
            dungeons.push_back(entry);
        }
    }
    std::vector<Dungeon> composed = ComposeNativeCatalogue(std::move(dungeons), BlueWarpExits(), overrides);
    size_t next = 0;
    for (Dungeon& entry : catalogue) {
        if (entry.pool == Pool::Dungeon) {
            entry = std::move(composed[next++]);
        } else {
            entry.returnTo.index = ResolveNativeIndex(entry.exit.index, entry.exit.index, overrides);
        }
    }
    Validate(catalogue);
    nlohmann::json out = nlohmann::json::array();
    for (const Dungeon& entry : catalogue) {
        out.push_back(DungeonJson(entry));
    }
    return out;
}

bool IncludesGanonsCastle(uint8_t dungeonSetting) {
    return dungeonSetting == RO_DUNGEON_ENTRANCE_SHUFFLE_ON_PLUS_GANON;
}

} // namespace

nlohmann::json FleetCrossDungeon_DumpCatalogue(const nlohmann::json& options) {
    IndexRegions();
    const uint8_t dungeonSetting = Rando::Context::GetInstance()->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).Get();
    return Catalogue(IncludesGanonsCastle(dungeonSetting), sNativeOverrides, ParseOptions(options));
}

// A saved seed's catalogue comes from its recorded native layout, never from the live graph.
nlohmann::json FleetCrossDungeon_DumpCatalogueForSeed(const nlohmann::json& spoiler) {
    const int dungeonSetting =
        spoiler.at("oot").at("settings").value(CVAR_RANDOMIZER_SETTING("ShuffleDungeonsEntrances"), 0);
    if (dungeonSetting < RO_DUNGEON_ENTRANCE_SHUFFLE_OFF || dungeonSetting > RO_DUNGEON_ENTRANCE_SHUFFLE_ON_PLUS_GANON) {
        throw std::invalid_argument("invalid native dungeon shuffle setting");
    }
    std::map<int32_t, int> knownTypes;
    for (const auto& pair : Rando::GetEntranceShuffleTable()) {
        knownTypes[pair.first.index] = static_cast<int>(pair.first.type);
        if (pair.second.index >= 0) {
            knownTypes[pair.second.index] = static_cast<int>(pair.second.type);
        }
    }
    const auto overrides =
        ParseNativeOverrides(spoiler.at("entrances").at("oot"), knownTypes, ENTRANCE_OVERRIDES_MAX_COUNT);
    const PoolOptions options = OptionsFromSeed(spoiler.value("crossDungeonEntrances", nlohmann::json{}));
    return Catalogue(IncludesGanonsCastle(static_cast<uint8_t>(dungeonSetting)), overrides, options);
}

void FleetCrossDungeon_SetSeed(const nlohmann::json& seed) {
    std::set<int32_t> known;
    for (const auto& pair : Rando::GetEntranceShuffleTable()) {
        known.insert(pair.first.index);
        if (pair.second.index >= 0) {
            known.insert(pair.second.index);
        }
    }
    for (const Route& route : Decode(seed)) {
        for (const Endpoint& endpoint : { route.from, route.to }) {
            if (endpoint.game == kShip && !known.contains(endpoint.index)) {
                throw std::invalid_argument("cross-dungeon seed names unknown OoT entrance");
            }
        }
    }
    sState.Install(seed);
    Rando::SetConnectedRegionOverride(RerouteDuringSearch);
}

void FleetCrossDungeon_SetAccess(const nlohmann::json& access) {
    sState.SetAccess(access);
}

// Which routes into the other game Ship's current search can take, evaluated on their native source edge.
nlohmann::json FleetCrossDungeon_GetExits() {
    const std::optional<uint8_t> dungeonRule = WidenDungeonRules();
    const bool previousSearching = sSearching;
    sSearching = false;
    const auto logic = Rando::Context::GetInstance()->GetLogic();
    const RandomizerRegion previousRegion = logic->CurrentRegionKey;
    nlohmann::json exits = nlohmann::json::array();
    for (Rando::Entrance* entrance : GetShuffleableEntrances(Rando::EntranceType::All, false)) {
        const Route* route = sState.Lookup(entrance->GetIndex(), ResolvedIndex(*entrance));
        if (route == nullptr || route->to.game == kShip) {
            continue;
        }
        logic->CurrentRegionKey = entrance->GetParentRegionKey();
        if (entrance->ConditionsMet()) {
            exits.push_back(EndpointJson(route->to));
        }
    }
    logic->CurrentRegionKey = previousRegion;
    sSearching = previousSearching;
    RestoreDungeonRules(dungeonRule);
    return exits;
}

// Without routes this only marks the search, so native baseline probes ignore the playing slot as well.
FleetCrossDungeonSearch::FleetCrossDungeonSearch() : mPreviousSearching(sSearching) {
    if (!sState.routes.empty()) {
        IndexRegions();
        for (int32_t index : sState.arrivals) {
            if (!sRegions.contains(index)) {
                throw std::invalid_argument("unknown OoT cross-dungeon arrival");
            }
        }
        mDungeonRule = WidenDungeonRules();
        Region* root = RegionTable(RR_ROOT);
        if (!sState.overworld) {
            mDetachedRootExits.splice(mDetachedRootExits.end(), root->exits);
        }
        for (int32_t index : sState.arrivals) {
            root->exits.emplace_back(sRegions.at(index), [] { return true; }, "cross-dungeon arrival");
            root->exits.back().SetParentRegion(RR_ROOT);
            mAddedArrivals++;
        }
    }
    sSearching = true;
}

FleetCrossDungeonSearch::~FleetCrossDungeonSearch() {
    sSearching = mPreviousSearching;
    Region* root = RegionTable(RR_ROOT);
    for (; mAddedArrivals > 0; mAddedArrivals--) {
        root->exits.pop_back();
    }
    root->exits.splice(root->exits.end(), mDetachedRootExits);
    RestoreDungeonRules(mDungeonRule);
}
