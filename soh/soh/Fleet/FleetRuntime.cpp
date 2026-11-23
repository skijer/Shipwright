#include "FleetRuntime.h"
#include "FleetRuntimeBridge.h"
#include "FleetGame.h"
#include "FleetForeignItems.h"

#include <filesystem>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <fleet/rando/CrossDungeonRuntime.h>
#include <spdlog/spdlog.h>

#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/ModApi/ProgressiveItems/ProgressiveItems.h"
#include "soh/ModApi/RandoItems/RandoItems.h"
#include "soh/ModApi/RandoInfCustom/RandoInfCustom.h"
#include "soh/SaveManager.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/savefile.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/randomizer_grotto.h"
#include "soh/OTRGlobals.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

void StartingItemGive(GetItemEntry entry, RandomizerCheck check);

extern "C" {
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

namespace {

using namespace ComboRando::CrossDungeon;
constexpr const char* kGame = "oot";
nlohmann::json sSession;
std::set<std::string> sReported;
RuntimeState sRoutes(0);
std::optional<RuntimeState::Transition> sDeparture;
bool sTransitionComplete = false;
bool sLoading = false;
int32_t sCheckpoint = -1;
bool sSaveCommitted = false;

bool IsKnownCheckpoint(int32_t entrance) {
    if (sRoutes.IsRecoveryEntrance(entrance)) {
        return true;
    }
    const auto cross = sSession.at("seed").value("crossDungeonEntrances", nlohmann::json{});
    if (!cross.is_object()) {
        return false;
    }
    const auto dungeons = cross.value("dungeons", nlohmann::json::array());
    for (const auto& entry : dungeons) {
        const auto dungeon = ParseDungeon(entry);
        if (dungeon.returnTo == Endpoint{ 0, entrance }) {
            return true;
        }
    }
    return false;
}

template <class Function> Function Host(const char* name) {
    return reinterpret_cast<Function>(FleetGame_GetHostFunction(name));
}

bool IsPlaying() {
    return gPlayState != nullptr && gGameState == &gPlayState->state && gGameState->running &&
           gSaveContext.gameMode == GAMEMODE_NORMAL && IS_RANDO && gSaveContext.fileNum == FleetRuntime_Slot();
}

void CompleteHeartPieces() {
    constexpr uint32_t pieceShift = QUEST_HEART_PIECE + 4;
    constexpr uint32_t piecesPerContainer = 4;
    gSaveContext.healthAccumulator = MAX_HEALTH;
    while ((gSaveContext.inventory.questItems >> pieceShift) >= piecesPerContainer) {
        gSaveContext.inventory.questItems -= piecesPerContainer << pieceShift;
        gSaveContext.healthCapacity += FULL_HEART_HEALTH;
        gSaveContext.health += FULL_HEART_HEALTH;
    }
}

void ApplyReceiveBookkeeping(const GetItemEntry& entry) {
    if (entry.modIndex != MOD_NONE) {
        return;
    }
    switch (entry.itemId) {
        case ITEM_SHIELD_DEKU:
            Flags_SetRandomizerInf(RAND_INF_HAS_FOUND_DEKU_SHIELD);
            break;
        case ITEM_SHIELD_HYLIAN:
            Flags_SetRandomizerInf(RAND_INF_HAS_FOUND_HYLIAN_SHIELD);
            break;
        case ITEM_TUNIC_GORON:
            Flags_SetRandomizerInf(RAND_INF_HAS_FOUND_GORON_TUNIC);
            break;
        case ITEM_TUNIC_ZORA:
            Flags_SetRandomizerInf(RAND_INF_HAS_FOUND_ZORA_TUNIC);
            break;
        case ITEM_SONG_EPONA:
            Flags_SetEventChkInf(EVENTCHKINF_EPONA_OBTAINED);
            break;
        case ITEM_HEART_PIECE:
        case ITEM_HEART_PIECE_2:
        case ITEM_HEART_CONTAINER:
            CompleteHeartPieces();
            break;
        default:
            break;
    }
}

void GiveNativeItem(RandomizerGet item) {
    const GetItemEntry entry = Rando::StaticData::RetrieveItem(item).GetGIEntry_Copy();
    if (!ProgressiveItems_GiveRando(item)) {
        StartingItemGive(entry, RC_MAX);
    }
    GameInteractor_ExecuteOnItemReceiveHooks(entry);
    ApplyReceiveBookkeeping(entry);
}

std::string DeliveredItem(const nlohmann::json& delivery) {
    return delivery.value("trap", false) ? "Ice Trap" : delivery.at("item").get<std::string>();
}

// A delivery for an item this game does not know (its mod was removed after the seed was made) waits instead of
// stopping the game, and arrives once the mod is back.
bool IsUnknownDelivery(const nlohmann::json& delivery) {
    static std::set<std::string> sReported;
    const std::string item = DeliveredItem(delivery);
    if (Rando::StaticData::itemNameToEnum.contains(item)) {
        return false;
    }
    if (sReported.insert(item).second) {
        SPDLOG_WARN("[Fleet] {} is not an item of this game; its delivery waits", item);
    }
    return true;
}

void Grant(const nlohmann::json& delivery) {
    const std::string item = DeliveredItem(delivery);
    const auto found = Rando::StaticData::itemNameToEnum.find(item);
    if (found == Rando::StaticData::itemNameToEnum.end()) {
        throw std::runtime_error("unknown OoT delivery: " + item);
    }
    const char* key = RandoItems_GetKey(found->second);
    if (key != nullptr) {
        CustomItemRegistry_ReceiveGetItem(key);
        return;
    }
    GiveNativeItem(found->second);
}

uint8_t RandoOption(RandomizerSettingKey key) {
    return OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(key);
}

std::vector<RandomizerGet> SharedTierItems(const std::string& key) {
    std::vector<RandomizerGet> levels;
    RandomizerGet infinite = RG_NONE;
    if (key == "wallets") {
        levels = { RG_CHILD_WALLET, RG_ADULT_WALLET, RG_GIANT_WALLET };
        if (RandoOption(RSK_INCLUDE_TYCOON_WALLET)) {
            levels.push_back(RG_TYCOON_WALLET);
        }
        infinite = RG_WALLET_INF;
    } else if (key == "bows") {
        levels = { RG_FAIRY_BOW, RG_BIG_QUIVER, RG_BIGGEST_QUIVER };
        infinite = RG_QUIVER_INF;
    } else if (key == "bombBags") {
        levels = { RG_BOMB_BAG, RG_BIG_BOMB_BAG, RG_BIGGEST_BOMB_BAG };
        infinite = RG_BOMB_BAG_INF;
    } else if (key == "magic") {
        levels = { RG_MAGIC_SINGLE, RG_MAGIC_DOUBLE };
        infinite = RG_MAGIC_INF;
    } else if (key == "dekuStickBag") {
        levels = { RG_DEKU_STICK_BAG, RG_DEKU_STICK_CAPACITY_20, RG_DEKU_STICK_CAPACITY_30 };
        infinite = RG_STICK_UPGRADE_INF;
    } else if (key == "dekuNutBag") {
        levels = { RG_DEKU_NUT_BAG, RG_DEKU_NUT_CAPACITY_30, RG_DEKU_NUT_CAPACITY_40 };
        infinite = RG_NUT_UPGRADE_INF;
    } else if (key == "bombchuBags") {
        levels = { RG_PROGRESSIVE_BOMBCHU_BAG, RG_PROGRESSIVE_BOMBCHU_BAG, RG_PROGRESSIVE_BOMBCHU_BAG };
        infinite = RG_BOMBCHU_INF;
    } else if (key == "hookshot") {
        levels = { RG_HOOKSHOT, RG_LONGSHOT };
    }
    if (infinite != RG_NONE && RandoOption(RSK_INFINITE_UPGRADES) != RO_INF_UPGRADES_OFF) {
        levels.push_back(infinite);
    }
    return levels;
}

uint16_t TierReceipt(const std::string& key, int level) {
    return RandoInfCustom_Register(("fleet:tier:" + key + ":" + std::to_string(level)).c_str());
}

bool GrantLevelOnce(const std::string& key, int level, const nlohmann::json& item) {
    const uint16_t receipt = TierReceipt(key, level);
    if (receipt == 0 || RandoInfCustom_Get(SOH_RANDO_INF_SAVE, receipt)) {
        return false;
    }
    Grant({ { "item", item } });
    RandoInfCustom_Set(SOH_RANDO_INF_SAVE, receipt, true);
    return true;
}

bool GrantSharedTier(const std::string& key, int current, const nlohmann::json& item) {
    const std::vector<RandomizerGet> levels = SharedTierItems(key);
    if (levels.empty()) {
        return GrantLevelOnce(key, current, item);
    }
    if (current < 0 || static_cast<size_t>(current) >= levels.size()) {
        return false;
    }
    GiveNativeItem(levels[current]);
    return true;
}

int WalletLevel() {
    constexpr int kInfiniteWallet = 5;
    if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MONEY)) {
        return kInfiniteWallet;
    }
    const bool hasWallet = !RandoOption(RSK_SHUFFLE_CHILD_WALLET) || Flags_GetRandomizerInf(RAND_INF_HAS_WALLET);
    return hasWallet ? 1 + CUR_UPG_VALUE(UPG_WALLET) : 0;
}

bool IsChateauLendingMagic() {
    using LendsMagic = bool (*)(void);
    const auto lends = reinterpret_cast<LendsMagic>(ModApi_Get()->FindService("nei.chateau.lends_magic"));
    return lends != nullptr && lends();
}

int MagicLevel() {
    constexpr int kInfiniteMagic = 3;
    const bool isInfinite = Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER) && !IsChateauLendingMagic();
    return isInfinite ? kInfiniteMagic : gSaveContext.isMagicAcquired + gSaveContext.isDoubleMagicAcquired;
}

int BombchuBagLevel() {
    if (RandoOption(RSK_BOMBCHU_BAG) == RO_BOMBCHU_BAG_PROGRESSIVE) {
        return gSaveContext.ship.quest.data.randomizer.bombchuUpgradeLevel;
    }
    return INV_CONTENT(ITEM_BOMBCHU) != ITEM_NONE;
}

int LevelOrInfinite(RandomizerInf infiniteFlag, int infiniteLevel, int level) {
    return Flags_GetRandomizerInf(infiniteFlag) ? infiniteLevel : level;
}

int SwordLevel() {
    const int kokiri = CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI) ? 1 : 0;
    return std::max<int>(kokiri, ProgressiveItems_GetLevel("nei.upgrades.kokiri", SOH_RANDO_INF_SAVE));
}

int StrengthLevel() {
    const int upgrade = CUR_UPG_VALUE(UPG_STRENGTH);
    return upgrade > 0 ? upgrade + 1 : static_cast<int>(Flags_GetRandomizerInf(RAND_INF_CAN_GRAB));
}

int GoldenRedQuarters() {
    using ReadQuarters = int32_t (*)();
    const auto read = reinterpret_cast<ReadQuarters>(ModApi_Get()->FindService("nei.heart.golden_red_quarters"));
    return read != nullptr ? read() : 0;
}

int HeartQuarters() {
    constexpr uint32_t pieceShift = QUEST_HEART_PIECE + 4;
    const int pieces = static_cast<int>(gSaveContext.inventory.questItems >> pieceShift);
    return gSaveContext.healthCapacity / FULL_HEART_HEALTH * 4 + pieces - GoldenRedQuarters();
}

int OcarinaLevel() {
    const u8 ocarina = INV_CONTENT(ITEM_OCARINA_FAIRY);
    if (ocarina == ITEM_NONE) {
        return 0;
    }
    return ocarina == ITEM_OCARINA_TIME ? 2 : 1;
}

nlohmann::json ReadSharedTiers() {
    return { { "bows", LevelOrInfinite(RAND_INF_HAS_INFINITE_QUIVER, 4, CUR_UPG_VALUE(UPG_QUIVER)) },
             { "bombBags", LevelOrInfinite(RAND_INF_HAS_INFINITE_BOMB_BAG, 4, CUR_UPG_VALUE(UPG_BOMB_BAG)) },
             { "bombchuBags", LevelOrInfinite(RAND_INF_HAS_INFINITE_BOMBCHUS, 4, BombchuBagLevel()) },
             { "dekuStickBag", LevelOrInfinite(RAND_INF_HAS_INFINITE_STICK_UPGRADE, 4, CUR_UPG_VALUE(UPG_STICKS)) },
             { "dekuNutBag", LevelOrInfinite(RAND_INF_HAS_INFINITE_NUT_UPGRADE, 4, CUR_UPG_VALUE(UPG_NUTS)) },
             { "magic", MagicLevel() },
             { "wallets", WalletLevel() },
             { "fireArrows", static_cast<int>(INV_CONTENT(ITEM_ARROW_FIRE) == ITEM_ARROW_FIRE) },
             { "iceArrows", static_cast<int>(INV_CONTENT(ITEM_ARROW_ICE) == ITEM_ARROW_ICE) },
             { "lightArrows", static_cast<int>(INV_CONTENT(ITEM_ARROW_LIGHT) == ITEM_ARROW_LIGHT) },
             { "eponasSong", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_EPONA) != 0) },
             { "songOfStorms", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_STORMS) != 0) },
             { "hookshot", static_cast<int>(INV_CONTENT(ITEM_HOOKSHOT) == ITEM_NONE
                                                ? 0
                                                : (INV_CONTENT(ITEM_HOOKSHOT) == ITEM_LONGSHOT ? 2 : 1)) },
             { "lens", static_cast<int>(INV_CONTENT(ITEM_LENS) != ITEM_NONE) },
             { "goronMask", static_cast<int>(Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_GORON)) },
             { "zoraMask", static_cast<int>(Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_ZORA)) },
             { "keatonMask", static_cast<int>(Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_KEATON)) },
             { "bunnyHood", static_cast<int>(Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_BUNNY)) },
             { "maskOfTruth", static_cast<int>(Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_TRUTH)) },
             { "ocarina", OcarinaLevel() },
             { "songOfTime", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_TIME) != 0) },
             { "sunsSong", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_SUN) != 0) },
             { "sariasSong", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_SARIA) != 0) },
             { "swords", SwordLevel() },
             { "strength", StrengthLevel() },
             { "scale", static_cast<int>(Flags_GetRandomizerInf(RAND_INF_CAN_SWIM)) },
             { "heartQuarters", HeartQuarters() } };
}

bool SynchronizeBottles() {
    using ReadBottles = const char* (*)(void);
    using WriteBottles = void (*)(const char*);
    static std::string sReported;
    static std::string sApplied;
    const auto get = Host<const char* (*)(const char*)>("GetBottles");
    const auto report = Host<bool (*)(const char*, const char*)>("ReportBottles");
    const auto read = reinterpret_cast<ReadBottles>(ModApi_Get()->FindService("nei.bottles.read"));
    const auto write = reinterpret_cast<WriteBottles>(ModApi_Get()->FindService("nei.bottles.write"));
    if (get == nullptr || report == nullptr || read == nullptr || write == nullptr) {
        return false;
    }
    bool isRewritten = false;
    const char* remote = get(kGame);
    if (remote != nullptr && remote[0] != '\0' && sApplied != remote) {
        if (std::string(remote) != read()) {
            write(remote);
            isRewritten = true;
        }
        sApplied = remote;
    }
    const std::string local = read();
    if (local != sReported && report(kGame, local.c_str())) {
        sReported = local;
    }
    return isRewritten;
}

struct SharedEquip {
    const char* key;
    u8 slot;
    u8 buttonItem = ITEM_NONE;
};

constexpr SharedEquip kSharedEquips[] = {
    { "fireArrows", SLOT_ARROW_FIRE, ITEM_BOW_ARROW_FIRE },
    { "iceArrows", SLOT_ARROW_ICE, ITEM_BOW_ARROW_ICE },
    { "lightArrows", SLOT_ARROW_LIGHT, ITEM_BOW_ARROW_LIGHT },
    { "bow", SLOT_BOW },
    { "ocarina", SLOT_OCARINA },
    { "bombs", SLOT_BOMB },
    { "bombchus", SLOT_BOMBCHU },
    { "dekuSticks", SLOT_STICK },
    { "dekuNuts", SLOT_NUT },
    { "hookshot", SLOT_HOOKSHOT },
    { "lens", SLOT_LENS },
    { "magicBeans", SLOT_BEAN },
    { "bottle1", SLOT_BOTTLE_1 },
    { "bottle2", SLOT_BOTTLE_2 },
    { "bottle3", SLOT_BOTTLE_3 },
    { "bottle4", SLOT_BOTTLE_4 },
};
constexpr size_t kSharedButtons = 7;
constexpr size_t kSharedCButtons = 3;

struct EquipButton {
    u8* item;
    u8* slot;
};

EquipButton SharedButton(size_t index) {
    return { &gSaveContext.equips.buttonItems[index + 1], &gSaveContext.equips.cButtonSlots[index] };
}

const SharedEquip* FindEquipOnButton(const EquipButton& button) {
    if (*button.item == ITEM_NONE) {
        return nullptr;
    }
    for (const auto& equip : kSharedEquips) {
        const bool isHeld =
            equip.buttonItem != ITEM_NONE ? *button.item == equip.buttonItem : *button.slot == equip.slot;
        if (isHeld) {
            return &equip;
        }
    }
    return nullptr;
}

nlohmann::json ReadSharedEquips() {
    auto equips = nlohmann::json::array();
    for (size_t index = 0; index < kSharedButtons; index++) {
        const SharedEquip* equip = FindEquipOnButton(SharedButton(index));
        equips.push_back(equip != nullptr ? equip->key : "");
    }
    return equips;
}

u8 OwnedButtonItem(const SharedEquip& equip) {
    const u8 owned = gSaveContext.inventory.items[equip.slot];
    if (owned == ITEM_NONE || equip.buttonItem == ITEM_NONE) {
        return owned;
    }
    return gSaveContext.inventory.items[SLOT_BOW] != ITEM_NONE ? equip.buttonItem : ITEM_NONE;
}

bool CanEquipSharedButton(size_t index) {
    return index < kSharedCButtons || CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) != 0;
}

void EquipSharedItem(size_t index, const SharedEquip& equip) {
    const u8 item = OwnedButtonItem(equip);
    const EquipButton target = SharedButton(index);
    if (item == ITEM_NONE || !CanEquipSharedButton(index) || FindEquipOnButton(target) == &equip) {
        return;
    }
    for (size_t other = 0; other < kSharedButtons; other++) {
        const EquipButton holder = SharedButton(other);
        if (FindEquipOnButton(holder) == &equip) {
            *holder.item = *target.item;
            *holder.slot = *target.slot;
        }
    }
    *target.item = item;
    *target.slot = equip.slot;
}

void ReloadSharedButtonIcons() {
    constexpr u8 kFirstIconlessItem = 0xF0;
    for (size_t index = 0; index < kSharedButtons; index++) {
        if (*SharedButton(index).item < kFirstIconlessItem) {
            Interface_LoadItemIcon1(gPlayState, static_cast<u16>(index + 1));
        }
    }
}

void ApplySharedEquips(const nlohmann::json& equips) {
    if (!equips.is_array() || equips.size() != kSharedButtons) {
        return;
    }
    for (size_t index = 0; index < kSharedButtons; index++) {
        const std::string key = equips[index].is_string() ? equips[index].get<std::string>() : std::string{};
        const auto equip = std::find_if(std::begin(kSharedEquips), std::end(kSharedEquips),
                                        [&](const SharedEquip& candidate) { return key == candidate.key; });
        if (equip != std::end(kSharedEquips)) {
            EquipSharedItem(index, *equip);
        }
    }
    ReloadSharedButtonIcons();
}

nlohmann::json ReadLiveState() {
    return { { "rupees", gSaveContext.rupees },
             { "health", gSaveContext.health },
             { "magic", gSaveContext.magic },
             { "chateau", IsChateauLendingMagic() },
             { "equips", ReadSharedEquips() } };
}

void CallChateauService(const char* name) {
    using ChateauService = void (*)(void);
    const auto service = reinterpret_cast<ChateauService>(ModApi_Get()->FindService(name));
    if (service != nullptr) {
        service();
    }
}

void ApplyLiveState(const nlohmann::json& state) {
    if (state.contains("rupees")) {
        gSaveContext.rupees =
            static_cast<s16>(std::clamp(state.at("rupees").get<int>(), 0, static_cast<int>(CUR_CAPACITY(UPG_WALLET))));
    }
    if (state.contains("health") && state.at("health").get<int>() > 0) {
        gSaveContext.health =
            static_cast<s16>(std::min(state.at("health").get<int>(), static_cast<int>(gSaveContext.healthCapacity)));
    }
    if (state.contains("magic") && gSaveContext.isMagicAcquired) {
        gSaveContext.magic =
            static_cast<s8>(std::clamp(state.at("magic").get<int>(), 0, static_cast<int>(gSaveContext.magicCapacity)));
    }
    if (state.contains("chateau") && state.at("chateau").get<bool>() != IsChateauLendingMagic()) {
        CallChateauService(state.at("chateau").get<bool>() ? "nei.chateau.lend" : "nei.chateau.end");
    }
    if (state.contains("equips")) {
        ApplySharedEquips(state.at("equips"));
    }
}

void SynchronizeLiveState() {
    static std::string sReported;
    static std::string sApplied;
    const auto get = Host<const char* (*)(const char*)>("GetLiveState");
    const auto report = Host<bool (*)(const char*, const char*)>("ReportLiveState");
    if (get == nullptr || report == nullptr) {
        return;
    }
    const char* remote = get(kGame);
    if (remote != nullptr && remote[0] != '\0' && sApplied != remote) {
        const auto state = nlohmann::json::parse(remote, nullptr, false);
        if (state.is_object()) {
            ApplyLiveState(state);
        }
        sApplied = remote;
    }
    const std::string local = ReadLiveState().dump();
    if (local != sReported && report(kGame, local.c_str())) {
        sReported = local;
    }
}

bool OwnsItemNamed(const std::string& item) {
    const auto found = Rando::StaticData::itemNameToEnum.find(item);
    return found != Rando::StaticData::itemNameToEnum.end() &&
           OTRGlobals::Instance->gRandoContext->GetLogic()->HasItem(found->second);
}

nlohmann::json ReadTiers(const nlohmann::json& shared) {
    nlohmann::json tiers = ReadSharedTiers();
    for (const auto& entry : shared) {
        const std::string key = entry.at("key");
        if (entry.value("owned", false) && !tiers.contains(key)) {
            tiers[key] = static_cast<int>(OwnsItemNamed(entry.at("item").get<std::string>()));
        }
    }
    return tiers;
}

void MarkHeldLevels(const nlohmann::json& tiers) {
    for (const auto& [key, level] : tiers.items()) {
        for (int held = 0; held < level.get<int>(); held++) {
            const uint16_t receipt = TierReceipt(key, held);
            if (receipt != 0) {
                RandoInfCustom_Set(SOH_RANDO_INF_SAVE, receipt, true);
            }
        }
    }
}

void SynchronizeSharedTiers() {
    static std::string sMarked;
    const auto report = Host<bool (*)(const char*, const char*)>("ReportSharedTiers");
    const auto get = Host<const char* (*)(const char*)>("GetSharedTiers");
    const char* listed = report != nullptr && get != nullptr ? get(kGame) : nullptr;
    if (listed == nullptr) {
        return;
    }
    const nlohmann::json held = ReadTiers(nlohmann::json::parse(listed));
    const std::string native = held.dump();
    if (!report(kGame, native.c_str())) {
        return;
    }
    if (native != sMarked) {
        MarkHeldLevels(held);
        sMarked = native;
    }
    const char* raw = get(kGame);
    if (raw == nullptr) {
        return;
    }
    const auto shared = nlohmann::json::parse(raw);
    bool changed = false;
    for (const auto& entry : shared) {
        const std::string key = entry.at("key");
        const int desired = entry.at("tier");
        if (held.value(key, 0) >= desired) {
            continue;
        }
        for (int attempt = 0; attempt < 16; attempt++) {
            const int previous = ReadTiers(shared).value(key, 0);
            if (previous >= desired || !GrantSharedTier(key, previous, entry.at("item"))) {
                break;
            }
            changed = true;
            if (ReadTiers(shared).value(key, 0) <= previous) {
                break;
            }
        }
    }
    if (changed) {
        FleetRuntime_Save();
    }
}

void ReceiveItems() {
    const auto get = Host<const char* (*)(const char*)>("GetDeliveries");
    const char* raw = get != nullptr ? get(kGame) : nullptr;
    if (raw == nullptr) {
        return;
    }
    const auto deliveries = nlohmann::json::parse(raw);
    bool changed = false;
    for (const auto& delivery : deliveries) {
        const std::string key = "fleet:received:" + delivery.at("id").get<std::string>();
        const uint16_t flag = RandoInfCustom_Register(key.c_str());
        if (flag == 0) {
            throw std::runtime_error("could not register Fleet receipt");
        }
        if (RandoInfCustom_Get(SOH_RANDO_INF_SAVE, flag) || IsUnknownDelivery(delivery)) {
            continue;
        }
        SPDLOG_INFO("[Fleet] Receiving {} from {}", delivery.at("item").get<std::string>(),
                    delivery.at("id").get<std::string>());
        Grant(delivery);
        RandoInfCustom_Set(SOH_RANDO_INF_SAVE, flag, true);
        changed = true;
    }
    if (changed) {
        FleetRuntime_Save();
    }
}

void ReportChecks() {
    const auto report = Host<bool (*)(const char*, const char*)>("ReportObtained");
    if (report == nullptr) {
        return;
    }
    const auto& placements = FleetRuntime_Seed().at(kGame).at("placements");
    bool saved = false;
    for (const auto& [check, item] : placements.items()) {
        if (sReported.contains(check)) {
            continue;
        }
        auto found = Rando::StaticData::locationNameToEnum.find(check);
        const bool obtained = found != Rando::StaticData::locationNameToEnum.end() &&
                              Rando::Context::GetInstance()->GetItemLocation(found->second)->HasObtained();
        if (!obtained) {
            continue;
        }
        if (!saved) {
            FleetRuntime_Save();
            saved = true;
        }
        if (report(kGame, check.c_str())) {
            sReported.insert(check);
        }
    }
}

} // namespace

void FleetRuntime_Initialize() {
    if (!sSession.is_null()) {
        return;
    }
    const auto get = Host<const char* (*)()>("GetSession");
    const char* raw = get != nullptr ? get() : nullptr;
    if (raw == nullptr) {
        return;
    }
    sSession = nlohmann::json::parse(raw);
    SOHCustomItemRandomizer randomizer = {};
    randomizer.structSize = sizeof(randomizer);
    SOHCustomItemDefinition definition = {};
    definition.structSize = sizeof(definition);
    definition.key = "fleet:foreign";
    definition.randomizer = &randomizer;
    definition.presentationFlags = SOH_ITEM_HIDE_FROM_SAVE_EDITOR | SOH_ITEM_HIDE_FROM_KALEIDO;
    definition.getItemText = "You found an item for the other game!";
    definition.displayName = "Cross-game item";
    definition.getItemEntry = Rando::StaticData::RetrieveItem(RG_BLUE_RUPEE).GetGIEntry_Copy();
    definition.getItemEntry.modIndex = MOD_RANDOMIZER;
    if (!CustomItemRegistry_Register(&definition)) {
        throw std::runtime_error("could not register Fleet foreign item");
    }
    FleetForeignItems_Register(FleetRuntime_Seed());
    FleetRuntime_InstallRoutes();
    SaveManager::Instance->AddSaveFunction(
        "fleet", 1, [](const SaveContext&, int, bool) { SaveManager::Instance->SaveData("checkpoint", sCheckpoint); },
        true, -1);
    SaveManager::Instance->AddLoadFunction("fleet", 1, [] {
        SaveManager::Instance->LoadData("checkpoint", sCheckpoint, int32_t(-1));
        if (!IsKnownCheckpoint(sCheckpoint)) {
            sCheckpoint = -1;
        }
    });
    SaveManager::Instance->AddInitFunction([](bool) { sCheckpoint = -1; });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveFile>([](int32_t slot, int32_t section) {
        if (slot == FleetRuntime_Slot() && section == SECTION_ID_BASE) {
            sSaveCommitted = true;
            FleetGame_CommitSave();
        }
    });
}

bool FleetRuntime_HasSession() {
    FleetRuntime_Initialize();
    return sSession.is_object();
}

const nlohmann::json& FleetRuntime_Seed() {
    return sSession.at("seed");
}

int FleetRuntime_Slot() {
    return sSession.is_object() ? sSession.at("slot").get<int>() : -1;
}

static double sSlowestSaveMilliseconds = 0.0;

static void WriteFleetSave();

void FleetRuntime_Save() {
    const auto start = std::chrono::steady_clock::now();
    WriteFleetSave();
    const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
    sSlowestSaveMilliseconds = std::max(sSlowestSaveMilliseconds, elapsed.count());
}

double FleetRuntime_SlowestSaveMilliseconds() {
    return sSlowestSaveMilliseconds;
}

static void WriteFleetSave() {
    if (IsPlaying()) {
        gSaveContext.rupees = static_cast<int16_t>(
            std::clamp<int>(gSaveContext.rupees + gSaveContext.rupeeAccumulator, 0, CUR_CAPACITY(UPG_WALLET)));
        gSaveContext.rupeeAccumulator = 0;
        Play_SaveSceneFlags(gPlayState);
        gSaveContext.savedSceneNum = gPlayState->sceneNum;
    }
    SaveManager::Instance->ThreadPoolWait();
    sSaveCommitted = false;
    FleetAutomaticSave automatic;
    SaveManager::Instance->SaveSection(gSaveContext.fileNum, SECTION_ID_BASE, false);
    if (sSession.is_object() && !sSaveCommitted) {
        throw std::runtime_error("could not commit OoT Fleet save");
    }
}

void FleetRuntime_CatchUpParked() {
    if (!FleetRuntime_HasSession() || !IsPlaying()) {
        return;
    }
    ReceiveItems();
    SynchronizeSharedTiers();
    if (SynchronizeBottles()) {
        FleetRuntime_Save();
    }
}

void FleetRuntime_FrameEnd() {
    if (!FleetRuntime_HasSession() || !IsPlaying() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    ReportChecks();
    if (gPlayState->msgCtx.msgMode == MSGMODE_NONE) {
        ReceiveItems();
        SynchronizeSharedTiers();
        SynchronizeBottles();
        SynchronizeLiveState();
    }
}

void FleetRuntime_InstallRoutes() {
    sRoutes.Install(FleetRuntime_Seed().value("crossDungeonEntrances", nlohmann::json{}));
}

nlohmann::json FleetRuntime_ReadOwnership(const nlohmann::json& items) {
    auto owned = nlohmann::json::object();
    for (const auto& item : items) {
        if (item.is_string()) {
            owned[item.get<std::string>()] = OwnsItemNamed(item.get<std::string>());
        }
    }
    return owned;
}

nlohmann::json FleetRuntime_Status() {
    auto received = nlohmann::json::array();
    if (sSession.is_object() && IsPlaying()) {
        const auto get = Host<const char* (*)(const char*)>("GetDeliveries");
        const char* raw = get != nullptr ? get(kGame) : nullptr;
        if (raw != nullptr) {
            for (const auto& delivery : nlohmann::json::parse(raw)) {
                const std::string id = delivery.at("id");
                const std::string key = "fleet:received:" + id;
                if (RandoInfCustom_Get(SOH_RANDO_INF_SAVE, RandoInfCustom_Find(key.c_str()))) {
                    received.push_back(id);
                }
            }
        }
    }
    return { { "received", received },
             { "sharedTiers", sSession.is_object() && IsPlaying() ? ReadSharedTiers() : nlohmann::json{} },
             { "playing", sSession.is_object() && IsPlaying() },
             { "slot", gSaveContext.fileNum },
             { "scene", sSession.is_object() && IsPlaying() ? static_cast<int>(gPlayState->sceneNum) : -1 } };
}

bool FleetRuntime_CollectForSmoke(const std::string& check) {
    if (!sSession.is_object() || !sSession.value("runtimeTests", false) || !IsPlaying() ||
        !FleetRuntime_Seed().at(kGame).at("placements").contains(check)) {
        return false;
    }
    const auto found = Rando::StaticData::locationNameToEnum.find(check);
    if (found == Rando::StaticData::locationNameToEnum.end()) {
        throw std::runtime_error("unknown OoT smoke check: " + check);
    }
    auto* location = Rando::Context::GetInstance()->GetItemLocation(found->second);
    const std::string expected = FleetForeignItems_KeyForCheck(FleetRuntime_Seed(), check);
    if (location->GetPlacedRandomizerGet() != RandoItems_GetRandoGet(expected.c_str())) {
        throw std::runtime_error("OoT smoke check has item " + std::to_string(location->GetPlacedRandomizerGet()) +
                                 ", expected " + expected + ": " + check);
    }
    location->SetCheckStatus(RCSHOW_COLLECTED);
    ReportChecks();
    return true;
}

int32_t FleetRuntime_Route(int32_t entrance, bool beforeNative) {
    if (!sSession.is_object() || !IsPlaying()) {
        return -1;
    }
    const auto transition = sRoutes.Lookup(beforeNative ? Phase::BeforeNative : Phase::AfterNative, entrance);
    if (!transition) {
        return -1;
    }
    if (transition->to.game == 0) {
        sCheckpoint = sRoutes.RecoveryForArrival(transition->to.index);
        return transition->to.index;
    }
    sDeparture = transition;
    sTransitionComplete = sLoading;
    return entrance;
}

bool FleetRuntime_TakeDeparture(FleetDestination& destination, std::string& payload) {
    if (!sDeparture || !sTransitionComplete) {
        return false;
    }
    const auto transition = *sDeparture;
    sCheckpoint = transition.checkpoint.index;
    sDeparture.reset();
    sTransitionComplete = false;
    gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
    gPlayState->transitionMode = TRANS_MODE_OFF;
    gSaveContext.entranceIndex = FleetRuntime_Materialize(transition.checkpoint.index);
    if (gSaveContext.entranceIndex >= 0 && gSaveContext.entranceIndex < ENTRANCE_TABLE_SIZE) {
        gSaveContext.savedSceneNum = gEntranceTable[gSaveContext.entranceIndex].scene;
    }
    FleetRuntime_Save();
    payload = nlohmann::json{
        { "cross", true }, { "slot", FleetRuntime_Slot() }, { "entrance", transition.to.index }
    }.dump();
    destination = { .game = "mm",
                    .entrance = static_cast<uint32_t>(transition.to.index),
                    .payloadJson = payload.c_str() };
    return true;
}

bool FleetRuntime_QueueArrival(int32_t entrance) {
    const bool queued = sRoutes.QueueArrival(entrance);
    if (queued) {
        sCheckpoint = sRoutes.RecoveryForArrival(entrance);
    }
    return queued;
}

int32_t FleetRuntime_Materialize(int32_t entrance) {
    return Grotto_OverrideSpecialEntrance(static_cast<int16_t>(entrance));
}

int32_t FleetRuntime_ConsumeArrival() {
    const int32_t entrance = sRoutes.ConsumeArrival();
    if (entrance < 0) {
        return -1;
    }
    sCheckpoint = sRoutes.RecoveryForArrival(entrance);
    return FleetRuntime_Materialize(entrance);
}

void FleetRuntime_ObserveScene(int32_t scene) {
    const auto* owner = sRoutes.FindEntrance(sCheckpoint);
    if (owner == nullptr) {
        sCheckpoint = -1;
        return;
    }
    if (owner->pool == Pool::Dungeon) {
        const bool dungeon = (scene >= SCENE_DEKU_TREE && scene <= SCENE_GANONS_TOWER_COLLAPSE_INTERIOR &&
                              scene != SCENE_THIEVES_HIDEOUT) ||
                             scene == SCENE_INSIDE_GANONS_CASTLE_COLLAPSE ||
                             (scene >= SCENE_DEKU_TREE_BOSS && scene <= SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR) ||
                             scene == SCENE_GANON_BOSS;
        if (!dungeon) {
            sCheckpoint = -1;
        }
        return;
    }
    if (owner->pool == Pool::Grotto) {
        if (scene != SCENE_GROTTOS && scene != SCENE_FAIRYS_FOUNTAIN) {
            sCheckpoint = -1;
        }
        return;
    }
    bool matches = false;
    for (int layer = 0; layer < 4 && sCheckpoint + layer < ENTRANCE_TABLE_SIZE; layer++) {
        matches = matches || gEntranceTable[sCheckpoint + layer].scene == scene;
    }
    if (!matches) {
        sCheckpoint = -1;
    }
}

extern "C" int32_t FleetRuntime_RecoveryEntrance(void) {
    return sCheckpoint >= 0 && IsKnownCheckpoint(sCheckpoint) ? FleetRuntime_Materialize(sCheckpoint) : -1;
}

extern "C" int32_t FleetRuntime_ResolveExit(int32_t entrance, int beforeNative) {
    return FleetRuntime_Route(entrance, beforeNative != 0);
}

extern "C" int FleetRuntime_CompleteTransition(void) {
    if (!sDeparture) {
        return 0;
    }
    sTransitionComplete = true;
    return 1;
}

extern "C" void FleetRuntime_CancelTransition(void) {
    sDeparture.reset();
    sTransitionComplete = false;
}

extern "C" void FleetRuntime_SetLoading(int loading) {
    sLoading = loading != 0;
}

extern "C" int FleetRuntime_EntrancesEnabled(void) {
    return sSession.is_object() && sRoutes.Enabled();
}
