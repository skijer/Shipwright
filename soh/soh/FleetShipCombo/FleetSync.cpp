// FleetSync.cpp (OoT side) — cross-game save cache + shared player-state overlay.
//
// Temp file: <ShipDir>/fleet_temp_flags.json — { "version", "slot", "oot", "mm", "shared" }.
// - WriteDeparture: "oot" = full SaveManager saveBlock (anchor) + regenerate "shared".
// - ApplyArrival: restore "oot" anchor if present (then erase it) + apply "shared" overlay.
// - Save sync: on our OnSaveFile (active game), refresh "shared" + SignalSyncSave; the frozen MM
//   exe applies + saves + acks; on ack we delete the temp file. As the FROZEN side, we do the
//   mirror in ProcessSignals (runs every frame via OnGameFrameUpdate, which still fires while the
//   game update is frozen).
//
// Canonical "shared" schema: OoT item-id space is canonical (bottles/equips translated MM-side via
// FleetComboIds.h). Fields one game can't author natively (e.g. ootMasksOwned here) are ECHOED —
// preserved from the previous shared block instead of regenerated.

#include "FleetSync.h"
#include "FleetShipCombo.h"
#include "FleetOracleClient.h" // pairing MM's derived save files with OoT's (delete/ensure)
#include "FleetComboIds.h"
#include "FleetComboItems.h"     // FCI_NO_ITEM, FCI_MAX
#include "FleetComboItemsGlue.h" // FcCombo_NativeForItem (FcComboItemId -> RG)
#include "FleetComboOptions.h"   // FC_COMBO_OPTION_TABLE (shared NEI options)
#include "soh/SaveManager.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/static_data.h" // Rando::StaticData::RetrieveItem + GetGIEntry_Copy
#include "soh/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h> // CVar: persist last-saved game/slot for boot
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

extern "C" {
#include <z64.h>
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "mods/nei_save.h"
extern SaveContext gSaveContext;
extern PlayState* gPlayState;
// Transformation-mask form bridge (mm_player_form.cpp): current MM form (0 FD..4 Human, >4 custom)
// and a setter that wears/removes the matching transformation mask on next gameplay frame.
int MmForm_GetCurrentForm(void);
int MmForm_GetFleetPublishForm(void); // pending target form if one is queued, else current (anti force-loop)
void MmForm_FleetApplyForm(int mmForm);
unsigned char TransformMasks_IsTransformedAny(void); // gates the form publish — see ExtractShared
void SwitchAge(void); // flips gSaveContext.linkAge + respawns at the current entrance (Enhancements/SwitchAge.cpp)

// Fleet age bridge: the TARGET linkAge the peer (MM's timeGateAdultMode) last asked for, -1 = none
// pending. SwitchAge() reloads the scene and only runs safely in gameplay, so a peer age change is
// QUEUED here (mirroring MmForm's sFleetPendingForm) and applied on the next safe gameplay frame,
// instead of inline in ApplyShared. Critically, ExtractShared publishes THIS target while it is
// pending, so OoT stops re-publishing its stale current age — which is what let the peer's steady
// value keep FORCING the local toggle back ("uso timegate y no me hace niño / siempre lo fuerza").
static s8 sFleetPendingAge = -1;
// Upgrade-column equipment (mods/extended_equipment.h). Declared here rather than including that
// header, which pulls z64item.h/color.h into this TU for four accessors.
unsigned char ExtEquip_CapeOwned(void);
void ExtEquip_GiveCape(void);
unsigned char ExtEquip_PendantOwned(void);
void ExtEquip_GivePendant(void);
// Ownership of a page-2 equipment cell (extEquipOwnedBits). Needed by the fold below.
unsigned char ExtEquip_HasItem(short equipType, unsigned char index);
// The single writer of an equipped ext slot + the RAM re-read after an apply (extended_equipment.h).
void ExtEquip_SetSlot(short equipType, unsigned char index);
void ExtEquip_ResyncFromSave(void);
// Bottle wheel fold (custom_bottles.cpp) — declared HERE, in the extern "C" block: a declaration
// inside this file's anonymous namespace mangles as a local C++ symbol and fails to link.
void Bottle_WheelPersist(unsigned char wheel, unsigned short slotItem);
void Bottle_WheelRecordActive(unsigned char wheel, unsigned short slotItem);
// Elemental Wand / Sheikah Slate grants (mods/extended_inventory.c): place the cell item, light the
// rod/rune bit and pick the active mode -- the same call the native pickup makes.
void Wand_GrantMode(unsigned char mode);
void Slate_GrantRune(unsigned char rune);
void Seasons_GrantSeason(unsigned char season);
}

// Cross-game restart: the raw reset of THIS game (defined in debugconsole.cpp), called by the
// responder pump below WITHOUT signaling so a paired reset never ping-pongs.
extern "C" void FleetCombo_DoLocalReset(void);

namespace {

std::filesystem::path SelfExeDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return {};
    }
    return std::filesystem::path(std::wstring(buf, len)).parent_path();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) {
        return {};
    }
    return std::filesystem::canonical(buf).parent_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

// Ship (host) exe dir IS the shared dir (2ship derives it as parent of its own exe dir).
// All fleet IPC/output files live in a <ShipDir>/fleet/ subfolder to keep the exe dir clean.
std::filesystem::path TempFilePath() {
    std::filesystem::path dir = SelfExeDir();
    if (dir.empty()) {
        return {};
    }
    std::error_code ec;
    std::filesystem::create_directories(dir / "fleet", ec);
    return dir / "fleet" / "temp_flags.json";
}

bool ReadTemp(nlohmann::json& out) {
    std::filesystem::path p = TempFilePath();
    if (p.empty() || !std::filesystem::exists(p)) {
        return false;
    }
    try {
        std::ifstream in(p);
        in >> out;
        return out.is_object();
    } catch (...) {
        SPDLOG_WARN("[FleetSync] temp file unreadable — treating as absent");
        return false;
    }
}

void WriteTemp(const nlohmann::json& j) {
    std::filesystem::path p = TempFilePath();
    if (p.empty()) {
        return;
    }
    try {
        std::filesystem::path tmp = p;
        tmp += ".tmp";
        {
            std::ofstream out(tmp);
            out << std::setw(1) << j << std::endl;
        }
        std::filesystem::rename(tmp, p);
    } catch (...) { SPDLOG_WARN("[FleetSync] temp file write failed"); }
}

void DeleteTemp() {
    std::filesystem::path p = TempFilePath();
    try {
        if (!p.empty() && std::filesystem::exists(p)) {
            std::filesystem::remove(p);
        }
    } catch (...) {}
}

// ---------------------------------------------------------------------------------------------
// Shared-state EXTRACT (live OoT state -> canonical json). `sh` may carry a previous shared block
// so echo-only fields survive.
// ---------------------------------------------------------------------------------------------

// Mirror vanilla + ext shield ownership into nei->shieldOwned (FC_SHIELD_* bits) and return it.
uint16_t ComputeShieldOwned() {
    NeiSaveData* nei = Nei_Save();
    uint16_t sh = nei->shieldOwned;
    uint16_t equip = gSaveContext.inventory.equipment;
    if (equip & (1 << 4))
        sh |= FC_SHIELD_DEKU; // EQUIP_FLAG_SHIELD_DEKU
    if (equip & (1 << 5))
        sh |= FC_SHIELD_HYLIAN; // EQUIP_FLAG_SHIELD_HYLIAN
    if (equip & (1 << 6))
        sh |= FC_SHIELD_MIRROR_OOT; // EQUIP_FLAG_SHIELD_MIRROR
    // NEI ext shields: extEquipOwnedBits, shields = equipType 1 -> bits 19..21
    if (nei->extEquipOwnedBits & (1u << 19))
        sh |= FC_SHIELD_DIVINE;
    if (nei->extEquipOwnedBits & (1u << 20))
        sh |= FC_SHIELD_KITE;
    if (nei->extEquipOwnedBits & (1u << 21))
        sh |= FC_SHIELD_IKANA;
    nei->shieldOwned = sh;
    return sh;
}

// Canonical equipped shield: 0 none, 1 deku, 2 hylian/hero, 3 mirror-OoT, 4 divine, 5 kite,
// 6 ikana/mirror-MM.
int GetEquippedShieldCanonical() {
    NeiSaveData* nei = Nei_Save();
    if (nei->extEquipShield >= 1 && nei->extEquipShield <= 3) {
        return 3 + nei->extEquipShield; // 4 divine, 5 kite, 6 ikana
    }
    int nibble = (gSaveContext.equips.equipment >> 4) & 0xF; // shield nibble
    return nibble;                                           // 0 none, 1 deku, 2 hylian, 3 mirror
}

// Routed through ExtEquip_SetSlot so the outgoing ext shield is cleaned up and the RAM copy every
// predicate/draw reads changes with the save (a raw nei->extEquipShield write left them apart
// until the next scene load). ExtEquip_SetSlot picks the owned vanilla base itself.
void SetEquippedShieldCanonical(int canon) {
    switch (canon) {
        case 1:
        case 2:
        case 3:
            ExtEquip_SetSlot(EQUIP_TYPE_SHIELD, 0);
            gSaveContext.equips.equipment = (gSaveContext.equips.equipment & ~0xF0) | (canon << 4);
            break;
        case 4:
        case 5:
        case 6:
            ExtEquip_SetSlot(EQUIP_TYPE_SHIELD, (unsigned char)(canon - 3));
            break;
        default:
            break; // 0/unknown: leave as-is
    }
}

void PutInvItem(nlohmann::json& inv, const char* key, int slot, bool withAmmo) {
    uint8_t item = gSaveContext.inventory.items[slot];
    inv[key] = (item != 0xFF);
    if (withAmmo) {
        inv[std::string(key) + "Ammo"] = (int)gSaveContext.inventory.ammo[slot];
    }
}

void ApplyInvItem(const nlohmann::json& inv, const char* key, int slot, uint8_t itemId, bool withAmmo) {
    if (!inv.contains(key)) {
        return;
    }
    if (inv[key].get<bool>()) {
        if (gSaveContext.inventory.items[slot] == 0xFF) {
            gSaveContext.inventory.items[slot] = itemId;
        }
    }
    // (ownership is additive: an item you have never disappears because the other game lacks it)
    if (withAmmo) {
        std::string ak = std::string(key) + "Ammo";
        if (inv.contains(ak) && gSaveContext.inventory.items[slot] != 0xFF) {
            gSaveContext.inventory.ammo[slot] = (int8_t)inv[ak].get<int>();
        }
    }
}

// HEALING: MM's ownedItems extract used to publish uninitialized (0x00) slots through
// FcEquip_MmToOot, and 0x00 is MM's Ocarina of Time, so every empty slot arrived here as OoT id
// 0x08 and got stored as a real owned item -- the "half my items turned into Ocarinas of Time"
// bug. Valid entries are ONLY page-2 customs (0x9E..0xB7) in [0..23] and mask ids (0xB8+) in
// [24..47], so anything outside those ranges is provably garbage and is cleared. Runs on every
// extract, so a poisoned save heals itself once and stays healed.
void HealBogusOwnedItems() {
    NeiSaveData* nei = Nei_Save();
    for (int i = 0; i < 48; i++) {
        uint16_t v = nei->ownedItems[i]; // u16 store (see NeiSaveData.ownedItems)
        if (v == 0xFF || v > 0xFF) {
            // Empty, or an EXT id (0x02xx) for an item that lives outside the u8 space. Those are
            // legitimately not page-2 nor mask ids, so without this guard the check below would
            // decide they are bogus and WIPE them. Skijer's NEI
            continue;
        }
        // ITEM_ELEMENTAL_WAND (0xD0) is the one page-2 custom outside the contiguous block; without
        // this it was "bogus" and the wand cell got wiped on every extract ("clearing bogus
        // ownedItems[3] = 0xD0" in the logs) -- owned rods, no wand in the kaleido.
        const bool validPage2 =
            (i < 24) && ((v >= FC_OOT_PAGE2_FIRST && v <= FC_OOT_PAGE2_LAST) || v == ITEM_ELEMENTAL_WAND);
        const bool validMask =
            (i >= 24) && v >= FC_OOT_MM_MASK_ITEM_BASE && v < FC_OOT_MM_MASK_ITEM_BASE + FC_MM_MASK_COUNT;
        if (!validPage2 && !validMask) {
            SPDLOG_WARN("[FleetSync] clearing bogus ownedItems[{}] = 0x{:02X}", i, v);
            nei->ownedItems[i] = 0xFF;
        }
    }
}

// Page-2 equipment -> FC registry. Mirror of the same fix on the MM side. Ownership of these cells
// lives in extEquipOwnedBits, which never travels as a word and was never folded here, so the only
// equipment that ever crossed was what the randomizer hook happened to record plus the three shields
// (which survive by accident, because ComputeShieldOwned reads their bits). Anything granted by the
// save editor, by an item behavior or by ExtEquip_Init's migrations stayed local forever.
//
// Both counters are raised: `obtained` so the peer learns about it, `applied` because the piece is
// already materialized here — otherwise ApplyFcRegistryToNatives would see a deficit against our own
// fold and re-grant it locally every frame.
//
// Trident, Climb Boots and Roc Boots are absent on purpose: no FCI_/RG_/RI_ identity yet, so there is
// no row to fold them into. Magic Cape and Pendant travel as their own booleans. Skijer's NEI
#ifndef EQUIP_TYPE_BOOTS
#define EQUIP_TYPE_BOOTS 3
#endif
static void FoldExtEquipmentIntoRegistry(NeiSaveData* nei) {
    static const struct {
        short equipType;
        unsigned char index;
        int fcId;
    } kExtEquipRows[] = {
        { EQUIP_TYPE_SWORD, 1, FCI_EXT_CANE_OF_BYRNA },
        { EQUIP_TYPE_SWORD, 2, FCI_EXT_FOUR_SWORD },
        { EQUIP_TYPE_SHIELD, 1, FCI_EXT_DIVINE_SHIELD },
        { EQUIP_TYPE_SHIELD, 2, FCI_EXT_SHEIKAH_SHIELD },
        { EQUIP_TYPE_TUNIC, 1, FCI_EXT_CHAMPIONS_TUNIC },
        { EQUIP_TYPE_TUNIC, 2, FCI_EXT_SPIRIT_BREASTPLATE },
        { EQUIP_TYPE_TUNIC, 3, FCI_EXT_WATER_DRAGON_SCALE },
        { EQUIP_TYPE_BOOTS, 1, FCI_EXT_PEGASUS_ANKLET },
        { EQUIP_TYPE_SWORD, 3, FCI_EXT_TRIDENT },
        { EQUIP_TYPE_BOOTS, 2, FCI_EXT_CLIMB_BOOTS },
        { EQUIP_TYPE_BOOTS, 3, FCI_EXT_ROC_BOOTS },
    };
    for (size_t i = 0; i < sizeof(kExtEquipRows) / sizeof(kExtEquipRows[0]); i++) {
        const int fcId = kExtEquipRows[i].fcId;
        if (fcId < 0 || fcId >= FC_COMBO_OBTAINED_FC_SIZE) {
            continue;
        }
        if (!ExtEquip_HasItem(kExtEquipRows[i].equipType, kExtEquipRows[i].index)) {
            continue;
        }
        if (nei->comboObtainedFc[fcId] == 0) {
            nei->comboObtainedFc[fcId] = 1;
        }
        if (nei->comboAppliedFc[fcId] < nei->comboObtainedFc[fcId]) {
            nei->comboAppliedFc[fcId] = nei->comboObtainedFc[fcId];
        }
    }
}

void FoldNativesIntoRegistry() {
    NeiSaveData* nei = Nei_Save();
    HealBogusOwnedItems();
    FoldExtEquipmentIntoRegistry(nei);
    uint16_t equip = gSaveContext.inventory.equipment;
    if (equip & (1 << 1))
        nei->comboObtained[FC_OOT_SWORD_MASTER] = 1; // EQUIP_FLAG_SWORD_MASTER
    if (equip & (1 << 2))
        nei->comboObtained[FC_OOT_SWORD_BIGGORON] = 1; // EQUIP_FLAG_SWORD_BGS
    if (equip & (1 << 9))
        nei->comboObtained[FC_OOT_TUNIC_GORON] = 1;
    if (equip & (1 << 10))
        nei->comboObtained[FC_OOT_TUNIC_ZORA] = 1;
    if (equip & (1 << 13))
        nei->comboObtained[FC_OOT_BOOTS_IRON] = 1;
    if (equip & (1 << 14))
        nei->comboObtained[FC_OOT_BOOTS_HOVER] = 1;
    // Child trade chain: flag at least the currently-held trade item.
    uint8_t trade = gSaveContext.inventory.items[SLOT_TRADE_CHILD];
    if (trade >= 0x21 && trade <= 0x23) {
        nei->comboObtained[FC_OOT_TRADE_WEIRD_EGG + (trade - 0x21)] = 1;
    } else if (trade >= 0x2D && trade <= 0x37) {
        nei->comboObtained[FC_OOT_TRADE_POCKET_EGG + (trade - 0x2D)] = 1;
    } else if (trade >= 0x24 && trade <= 0x2B) {
        // Child-trade MASKS (Keaton..Mask of Truth). They fell in the gap between the two ranges
        // above, and ootMasksOwned itself was echo-only — nothing ever authored it — so a mask
        // earned in OoT never reached MM's OoT-mask wheel. Bit order = MM's sOotMaskIconPaths
        // (Keaton 0 .. Truth 7), which is exactly the item-id order. 2026-08-07, Skijer's NEI
        nei->ootMasksOwned |= (uint16_t)(1u << (trade - 0x24));
    }
    // Triforce pool: keep registry counter as the max of both stores.
    uint8_t nativeTf = gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected;
    if (nativeTf > nei->comboTriforce) {
        nei->comboTriforce = nativeTf;
    }
}

void ApplyRegistryToNatives() {
    NeiSaveData* nei = Nei_Save();
    if (nei->comboObtained[FC_OOT_SWORD_MASTER])
        gSaveContext.inventory.equipment |= (1 << 1);
    if (nei->comboObtained[FC_OOT_SWORD_BIGGORON])
        gSaveContext.inventory.equipment |= (1 << 2);
    if (nei->comboObtained[FC_OOT_TUNIC_GORON])
        gSaveContext.inventory.equipment |= (1 << 9);
    if (nei->comboObtained[FC_OOT_TUNIC_ZORA])
        gSaveContext.inventory.equipment |= (1 << 10);
    if (nei->comboObtained[FC_OOT_BOOTS_IRON])
        gSaveContext.inventory.equipment |= (1 << 13);
    if (nei->comboObtained[FC_OOT_BOOTS_HOVER])
        gSaveContext.inventory.equipment |= (1 << 14);
    uint8_t& nativeTf = gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected;
    if (nei->comboTriforce > nativeTf) {
        nativeTf = (uint8_t)std::min<int>(nei->comboTriforce, 255);
    }
}

// Cell repair (idempotent, both directions). Ownership of these cells is a FLAG/bitmask and the
// cell can be emptied by something other than the player (OoT's HealBogusOwnedItems used to wipe
// the wand cell on every extract; the Sheikah Slate cell was overwritten by the u16->u8 truncation
// loop). Once the bit is set, the incremental "gained" grants in ApplyShared never fire again, so
// the cell has to be re-seeded from the flags -- exactly what the kaleido's Page2Relayout_Heal does
// for the Shovel/Dominion wheel. Runs on every extract and every apply. Slots are
// extended_inventory.h SLOT_* (header not included in this TU).
static void RepairFlagOwnedCells(NeiSaveData* nei) {
    const uint8_t kSlotWand = 27, kSlotSlate = 39, kSlotShovel = 46, kSlotRod = 47;
    const uint16_t kExtSheikahSlate = 0x0220; // EXT_ITEM_SHEIKAH_SLATE (same id in both games)
    const uint16_t kExtRodOfSeasons = 0x0223; // EXT_ITEM_ROD_OF_SEASONS (same id in both games)
    if (nei->wandRodsOwned != 0 && Nei_GetOwnedItem(kSlotWand) != ITEM_ELEMENTAL_WAND) {
        Nei_SetOwnedItem(kSlotWand, ITEM_ELEMENTAL_WAND);
    }
    if (nei->slateRunesOwned != 0 && Nei_GetOwnedItem(kSlotSlate) != kExtSheikahSlate) {
        Nei_SetOwnedItem(kSlotSlate, kExtSheikahSlate);
    }
    if (nei->seasonsOwned != 0 && Nei_GetOwnedItem(kSlotRod) != kExtRodOfSeasons) {
        Nei_SetOwnedItem(kSlotRod, kExtRodOfSeasons);
    }
    if (nei->shovelOwned || nei->dominionOwned) {
        const uint16_t cur = Nei_GetOwnedItem(kSlotShovel);
        if (cur != ITEM_SHOVEL && cur != ITEM_DOMINION_ROD) {
            Nei_SetOwnedItem(kSlotShovel, nei->shovelOwned ? ITEM_SHOVEL : ITEM_DOMINION_ROD);
        }
    }
}

void ExtractShared(nlohmann::json& sh) {
    NeiSaveData* nei = Nei_Save();
    FoldNativesIntoRegistry();

    sh["schema"] = 1;
    sh["vitals"] = { { "health", gSaveContext.health },
                     { "healthCapacity", gSaveContext.healthCapacity },
                     { "doubleDefense", gSaveContext.isDoubleDefenseAcquired },
                     { "defenseHearts", gSaveContext.inventory.defenseHearts },
                     { "magic", gSaveContext.magic },
                     // magicLevel is NOT published: it is the game's own meter-build handshake, not
                     // shared state. See the magic block in ApplyShared.
                     { "isMagic", gSaveContext.isMagicAcquired },
                     { "isDoubleMagic", gSaveContext.isDoubleMagicAcquired },
                     { "rupees", gSaveContext.rupees } };
    sh["upgrades"] = { { "wallet", CUR_UPG_VALUE(UPG_WALLET) },    { "quiver", CUR_UPG_VALUE(UPG_QUIVER) },
                       { "bombBag", CUR_UPG_VALUE(UPG_BOMB_BAG) }, { "sticks", CUR_UPG_VALUE(UPG_STICKS) },
                       { "nuts", CUR_UPG_VALUE(UPG_NUTS) },        { "strength", CUR_UPG_VALUE(UPG_STRENGTH) },
                       { "scale", CUR_UPG_VALUE(UPG_SCALE) },      { "bulletBag", CUR_UPG_VALUE(UPG_BULLET_BAG) } };
    // Grab skill. NOT an upgrade level: with Shuffle Grab on, the first Progressive Strength is
    // RG_POWER_BRACELET and sets this flag while leaving UPG_STRENGTH at 0 (item.cpp:354). MM has no
    // use for the skill but must be able to receive that copy, so the flag has to cross. With Shuffle
    // Grab OFF the flag is already true from logic init, which is exactly what tells MM to send its
    // first Progressive Strength straight to the Goron's Bracelet. Skijer's NEI
    sh["canGrab"] = Flags_GetRandomizerInf(RAND_INF_CAN_GRAB) ? 1 : 0;
    sh["weaponUpgrades"] = nei->weaponUpgrades;
    sh["shieldOwned"] = ComputeShieldOwned();
    sh["equippedShield"] = GetEquippedShieldCanonical();
    // Upgrade-column equipment (Skijer 2026-07-31): the Magic Cape and the Pendant of Memories live
    // outside both extEquipOwnedBits and inventory.equipment, so nothing was carrying them across —
    // obtaining either in one game left the other game's equipment page empty. Ownership only; the
    // capeHidden / pendantEffectOff toggles stay per-game (they are view/moveset preferences, and
    // OR-merging a toggle would make it impossible to turn off). ExtEquip_PendantOwned() is called
    // rather than read raw so the adult-trade-slot grant latches before we publish.
    sh["capeOwned"] = ExtEquip_CapeOwned() != 0;
    sh["pendantOwned"] = ExtEquip_PendantOwned() != 0;
    uint16_t equip = gSaveContext.inventory.equipment;
    sh["swordFlags"] = { { "kokiri", (equip & (1 << 0)) != 0 },
                         { "master", (equip & (1 << 1)) != 0 },
                         { "biggoron", (equip & (1 << 2)) != 0 } };
    sh["equippedSword"] = (int)(gSaveContext.equips.equipment & 0xF); // 0 none,1 kokiri,2 master,3 bgs

    nlohmann::json inv = sh.contains("inv") ? sh["inv"] : nlohmann::json::object();
    PutInvItem(inv, "stick", SLOT_STICK, true);
    PutInvItem(inv, "nut", SLOT_NUT, true);
    PutInvItem(inv, "bomb", SLOT_BOMB, true);
    PutInvItem(inv, "bow", SLOT_BOW, true);
    PutInvItem(inv, "bombchu", SLOT_BOMBCHU, true);
    PutInvItem(inv, "fireArrow", SLOT_ARROW_FIRE, false);
    PutInvItem(inv, "iceArrow", SLOT_ARROW_ICE, false);
    PutInvItem(inv, "lightArrow", SLOT_ARROW_LIGHT, false);
    PutInvItem(inv, "lens", SLOT_LENS, false);
    PutInvItem(inv, "beans", SLOT_BEAN, true);
    PutInvItem(inv, "boomerang", SLOT_BOOMERANG, false);
    PutInvItem(inv, "hammer", SLOT_HAMMER, false);
    PutInvItem(inv, "dins", SLOT_DINS_FIRE, false);
    PutInvItem(inv, "farores", SLOT_FARORES_WIND, false);
    PutInvItem(inv, "nayrus", SLOT_NAYRUS_LOVE, false);
    PutInvItem(inv, "slingshot", SLOT_SLINGSHOT, true);
    inv["ocarinaFairy"] = gSaveContext.inventory.items[SLOT_OCARINA] == ITEM_OCARINA_FAIRY ||
                          gSaveContext.inventory.items[SLOT_OCARINA] == ITEM_OCARINA_TIME;
    inv["ocarinaTime"] = gSaveContext.inventory.items[SLOT_OCARINA] == ITEM_OCARINA_TIME;
    // Hookshot chain: 0 none / 1 hookshot / 2 longshot / 3 ultrashot
    int hookLevel = 0;
    if (gSaveContext.inventory.items[SLOT_HOOKSHOT] == ITEM_HOOKSHOT)
        hookLevel = 1;
    if (gSaveContext.inventory.items[SLOT_HOOKSHOT] == ITEM_LONGSHOT)
        hookLevel = 2;
    if (hookLevel == 2 && nei->ultrashotOwned)
        hookLevel = 3;
    inv["hookshotLevel"] = hookLevel;
    // clawshot: no native OoT ownership store -> echo (MM authors it)
    if (!inv.contains("clawshot"))
        inv["clawshot"] = false;
    inv["pictobox"] = nei->pictoboxOwned != 0;
    inv["powderKeg"] = nei->powerKegOwned != 0;
    inv["powderKegCount"] = nei->powerKegCount;
    inv["net"] = nei->netEquipped != 0;
    inv["bottomlessMode"] = nei->bottomlessBottleMode;
    inv["bottomlessContent"] = nei->bottomlessContent; // OoT id space (canonical)
    inv["bottomlessCount"] = nei->bottomlessCount;
    sh["inv"] = inv;

    // Fold the LIVE bottles into the shared store before publishing (2026-08-07). Persist/Record
    // only ran from the kaleido, so in-game catches/drinks sat in the native slots while FleetSync
    // kept publishing the stale wheel array — the reported "bottles don't share correctly". Mirror
    // of the MM-side fold; same pair the kaleido uses, driven by the two native cells.
    {
        uint16_t nativeA = gSaveContext.inventory.items[SLOT_BOTTLE_1];
        uint16_t nativeB = gSaveContext.inventory.items[SLOT_BOTTLE_2];
        Bottle_WheelPersist(0, nativeA); // BOTTLE_WHEEL_A
        Bottle_WheelRecordActive(0, nativeA);
        Bottle_WheelPersist(1, nativeB); // BOTTLE_WHEEL_B
        Bottle_WheelRecordActive(1, nativeB);
    }
    sh["bottleSlots"] = nei->bottleSlots; // canonical = OoT id space
    // Canonical ownedItems is a u8 id space. EXT items (u16, 0x02xx: Sheikah Slate, Phantom
    // Hourglass, Shadow Crystal, Rod of Seasons) do NOT fit and cross through the fcId registry
    // instead; publishing them raw here made MM truncate them to a byte. Mirror of MM's own guard.
    {
        nlohmann::json owned = nlohmann::json::array();
        for (int i = 0; i < 48; i++) {
            const uint16_t v = nei->ownedItems[i];
            owned.push_back(v > 0xFF ? 0xFF : v);
        }
        sh["ownedItems"] = owned;
    }
    sh["tradeAdultOwned"] = nei->tradeAdultOwned;
    // Elemental Wand rods + Sheikah Slate runes: the cell item travels in ownedItems, but WHICH rods
    // / runes you own lives in these bitmasks, and nothing carried them -- the peer got an empty
    // wand. OR-merged both ways (one-way unlocks); the fcId deficit path still grants the items
    // natively, this just guarantees the bits arrive even if a grant is missed.
    sh["wandRodsOwned"] = (int)nei->wandRodsOwned;
    sh["slateRunesOwned"] = (int)nei->slateRunesOwned;
    sh["seasonsOwned"] = (int)nei->seasonsOwned;
    RepairFlagOwnedCells(nei);
    // Shared NEI options/flags (FleetComboOptions.h). Table-driven so a future
    // option is one row there, not new code here. MAX rows never lose a value;
    // NEWEST rows let either game re-author the player's preference.
#define FCO_EXTRACT(key, field, mode) \
    sh[key] = (mode == FCO_MERGE_MAX) ? (uint8_t)std::max<int>(sh.value(key, 0), nei->field) : (uint8_t)nei->field;
    FC_COMBO_OPTION_TABLE(FCO_EXTRACT)
#undef FCO_EXTRACT
    // Quest bitfields are one-way unlocks authored by BOTH games (cross-placement): merge with the
    // previous shared value instead of overwriting, so bits published by MM that we haven't applied
    // locally yet are never clobbered by our own save.
    // Publish the LIVE save, never "snapshot | save": in a resync `sh` IS the running snapshot, so
    // ORing against it rebroadcast every bit forever, even bits the file never had (the phantom
    // medallions/songs). Anchor's model: publish facts; ApplyShared ORs incoming bits into the save.
    sh["ootQuestItems"] = (uint32_t)gSaveContext.inventory.questItems;
    sh["gsTokens"] = gSaveContext.inventory.gsTokens;
    sh["mmQuestItems"] = nei->mmQuestItems; // live store only — see the ootQuestItems note above
    sh["comboObtained"] = nei->comboObtained;
    // Generic fcId-indexed cross store (counts). comboAppliedFc is LOCAL-only — never serialized.
    sh["comboObtainedFc"] = nei->comboObtainedFc;
    sh["comboTriforce"] = nei->comboTriforce;
    // Goal state (Beat Both Bosses). OR-merged both ways: a boss that fell stays fallen, and
    // neither world may end the run until it sees BOTH bits.
    sh["comboGoalFlags"] = nei->comboGoalFlags;
    // ootMasksOwned: authored now (2026-08-07) — FoldNativesIntoRegistry folds the child-trade mask
    // in the trade slot into the bitmask, so publish the real field instead of the old echo-only 0.
    // Merge-max against anything already in the shared blob so a peer's bits never regress.
    {
        uint32_t prev = sh.contains("ootMasksOwned") ? sh["ootMasksOwned"].get<uint32_t>() : 0u;
        sh["ootMasksOwned"] = prev | (uint32_t)nei->ootMasksOwned;
    }

    // NOTE: cEquips/dEquips are deliberately NOT here. Button equips travel only at a game change —
    // see the ExtractEquips/ApplyEquips block below for why.

    // Publish the pending target form while a peer-requested change is applying (anti force-loop).
    int form = MmForm_GetFleetPublishForm();
    // gFormState is zero-initialised and Fierce Deity IS form 0, so an untransformed Link published
    // "Fierce Deity" and forced MM into it on a fresh seed's first sync. Narrow override: only that
    // ambiguous pair, so a genuinely queued form still publishes and keeps the anti force-loop.
    if (form == 0 && !TransformMasks_IsTransformedAny()) {
        form = 4;
    }
    sh["form"] = (form >= 0 && form <= 4) ? form : 4; // custom forms sync as Human

    // Adult/child age <-> MM's timeGateAdultMode. LINK_AGE_ADULT == 0. Last-writer-wins (not OR-merged).
    // Publish the PENDING target while a peer-requested SwitchAge is queued (not yet applied), so we
    // don't keep advertising our stale current age and forcing the peer's fresh toggle back.
    s8 effAge = (sFleetPendingAge >= 0) ? sFleetPendingAge : gSaveContext.linkAge;
    sh["adult"] = (effAge == LINK_AGE_ADULT);
}

// ---------------------------------------------------------------------------------------------
// Shared-state APPLY (canonical json -> live OoT state)
// ---------------------------------------------------------------------------------------------
void ApplyShared(const nlohmann::json& sh) {
    NeiSaveData* nei = Nei_Save();

    if (sh.contains("vitals")) {
        const auto& v = sh["vitals"];
        gSaveContext.healthCapacity = (int16_t)v.value("healthCapacity", (int)gSaveContext.healthCapacity);
        gSaveContext.health =
            (int16_t)std::min<int>(v.value("health", (int)gSaveContext.health), gSaveContext.healthCapacity);
        // EVERY default here MUST be the current value, never 0: this block also runs for PARTIAL
        // deltas (FleetNet sends one leaf at a time), so a default of 0 would wipe magic and defense
        // hearts every time an unrelated vital -- a single rupee -- changed.
        // Double Defense is a one-way unlock: MAX-merge, so a peer snapshot taken before it received
        // its copy (or a resync from the inactive game) can never strip it again.
        gSaveContext.isDoubleDefenseAcquired =
            (uint8_t)std::max<int>(gSaveContext.isDoubleDefenseAcquired, v.value("doubleDefense", 0));
        gSaveContext.inventory.defenseHearts =
            (int8_t)std::max<int>(gSaveContext.inventory.defenseHearts, v.value("defenseHearts", 0));

        // Magic syncs as the two OWNERSHIP FLAGS only -- magicLevel is deliberately not copied.
        // magicLevel is not "how much magic you have", it is the handshake the game uses to build
        // the meter: z_parameter.c waits for (isMagicAcquired && magicLevel == 0), then sets
        // magicLevel = isDoubleMagicAcquired + 1 and steps magicCapacity up from zero. Copying the
        // peer's magicLevel = 1 skips that init, magicCapacity stays 0, and the bar never appears
        // even though you own the magic. So: take the flags, and whenever they GAIN something, clear
        // magicLevel so this game runs its own init next frame -- exactly what a native grant does.
        const bool hadMagic = gSaveContext.isMagicAcquired != 0;
        const bool hadDouble = gSaveContext.isDoubleMagicAcquired != 0;
        // MAX-merge (never lose): magic is never un-obtained in either game, so a peer snapshot that
        // still says "no magic" (it simply hasn't been granted its copy yet) must not strip the
        // meter this game already has.
        const bool hasMagic = hadMagic || v.value("isMagic", 0) != 0;
        const bool hasDouble = hadDouble || v.value("isDoubleMagic", 0) != 0;
        gSaveContext.isMagicAcquired = hasMagic;
        gSaveContext.isDoubleMagicAcquired = hasDouble;
        if ((hasMagic && !hadMagic) || (hasDouble && !hadDouble)) {
            gSaveContext.magicLevel = 0; // re-run the native meter init (grows magicCapacity)
        }
        gSaveContext.magic = (int8_t)v.value("magic", (int)gSaveContext.magic);
        gSaveContext.rupees = (int16_t)v.value("rupees", (int)gSaveContext.rupees);
    }
    if (sh.contains("upgrades")) {
        const auto& u = sh["upgrades"];
        // These are UNCONDITIONAL writes, so the default matters twice over. This block also runs
        // for PARTIAL deltas, and a default of 0 would reset every upgrade the delta did not happen
        // to mention -- a wallet change alone would wipe the quiver, bomb bag, strength and scale.
        // Take the max as well: none of these ever decrease in either game, so a stale or
        // differently-scaled reading from the peer can never walk an upgrade backwards.
        auto upg = [&u](const char* key, int cur) { return std::max(cur, u.value(key, cur)); };
        Inventory_ChangeUpgrade(UPG_WALLET, upg("wallet", CUR_UPG_VALUE(UPG_WALLET)));
        Inventory_ChangeUpgrade(UPG_QUIVER, upg("quiver", CUR_UPG_VALUE(UPG_QUIVER)));
        Inventory_ChangeUpgrade(UPG_BOMB_BAG, upg("bombBag", CUR_UPG_VALUE(UPG_BOMB_BAG)));
        Inventory_ChangeUpgrade(UPG_STICKS, upg("sticks", CUR_UPG_VALUE(UPG_STICKS)));
        Inventory_ChangeUpgrade(UPG_NUTS, upg("nuts", CUR_UPG_VALUE(UPG_NUTS)));
        Inventory_ChangeUpgrade(UPG_STRENGTH, upg("strength", CUR_UPG_VALUE(UPG_STRENGTH)));
        Inventory_ChangeUpgrade(UPG_SCALE, upg("scale", CUR_UPG_VALUE(UPG_SCALE)));
        Inventory_ChangeUpgrade(UPG_BULLET_BAG, upg("bulletBag", CUR_UPG_VALUE(UPG_BULLET_BAG)));
    }
    // Grab skill — latch on only (never cleared), so a stale peer snapshot cannot revoke it.
    if (sh.contains("canGrab") && sh["canGrab"].get<int>() != 0) {
        Flags_SetRandomizerInf(RAND_INF_CAN_GRAB);
    }
    if (sh.contains("weaponUpgrades")) {
        nei->weaponUpgrades |= (uint8_t)sh["weaponUpgrades"].get<int>(); // additive
    }
    if (sh.contains("shieldOwned")) {
        uint16_t owned = (uint16_t)sh["shieldOwned"].get<int>();
        nei->shieldOwned |= owned;
        if (owned & FC_SHIELD_DEKU)
            gSaveContext.inventory.equipment |= (1 << 4);
        if (owned & FC_SHIELD_HYLIAN)
            gSaveContext.inventory.equipment |= (1 << 5);
        if (owned & FC_SHIELD_MIRROR_OOT)
            gSaveContext.inventory.equipment |= (1 << 6);
        if (owned & FC_SHIELD_DIVINE)
            nei->extEquipOwnedBits |= (1u << 19);
        if (owned & FC_SHIELD_KITE)
            nei->extEquipOwnedBits |= (1u << 20);
        if (owned & FC_SHIELD_IKANA)
            nei->extEquipOwnedBits |= (1u << 21);
    }
    if (sh.contains("equippedShield")) {
        SetEquippedShieldCanonical(sh["equippedShield"].get<int>());
    }
    // Upgrade-column equipment — additive, never cleared (mirror of the extract side above).
    if (sh.value("capeOwned", false)) {
        ExtEquip_GiveCape();
    }
    if (sh.value("pendantOwned", false)) {
        ExtEquip_GivePendant();
    }
    if (sh.contains("swordFlags")) {
        const auto& s = sh["swordFlags"];
        if (s.value("kokiri", false))
            gSaveContext.inventory.equipment |= (1 << 0);
        if (s.value("master", false))
            gSaveContext.inventory.equipment |= (1 << 1);
        if (s.value("biggoron", false))
            gSaveContext.inventory.equipment |= (1 << 2);
    }
    if (sh.contains("equippedSword")) {
        int sw = sh["equippedSword"].get<int>();
        if (sw >= 1 && sw <= 3) { // 4 (Deity) has no OoT equip: keep current
            gSaveContext.equips.equipment = (gSaveContext.equips.equipment & ~0xF) | sw;
        }
    }

    if (sh.contains("inv")) {
        const auto& inv = sh["inv"];
        ApplyInvItem(inv, "stick", SLOT_STICK, ITEM_STICK, true);
        ApplyInvItem(inv, "nut", SLOT_NUT, ITEM_NUT, true);
        ApplyInvItem(inv, "bomb", SLOT_BOMB, ITEM_BOMB, true);
        ApplyInvItem(inv, "bow", SLOT_BOW, ITEM_BOW, true);
        ApplyInvItem(inv, "bombchu", SLOT_BOMBCHU, ITEM_BOMBCHU, true);
        ApplyInvItem(inv, "fireArrow", SLOT_ARROW_FIRE, ITEM_ARROW_FIRE, false);
        ApplyInvItem(inv, "iceArrow", SLOT_ARROW_ICE, ITEM_ARROW_ICE, false);
        ApplyInvItem(inv, "lightArrow", SLOT_ARROW_LIGHT, ITEM_ARROW_LIGHT, false);
        ApplyInvItem(inv, "lens", SLOT_LENS, ITEM_LENS, false);
        ApplyInvItem(inv, "beans", SLOT_BEAN, ITEM_BEAN, true);
        ApplyInvItem(inv, "boomerang", SLOT_BOOMERANG, ITEM_BOOMERANG, false);
        ApplyInvItem(inv, "hammer", SLOT_HAMMER, ITEM_HAMMER, false);
        ApplyInvItem(inv, "dins", SLOT_DINS_FIRE, ITEM_DINS_FIRE, false);
        ApplyInvItem(inv, "farores", SLOT_FARORES_WIND, ITEM_FARORES_WIND, false);
        ApplyInvItem(inv, "nayrus", SLOT_NAYRUS_LOVE, ITEM_NAYRUS_LOVE, false);
        ApplyInvItem(inv, "slingshot", SLOT_SLINGSHOT, ITEM_SLINGSHOT, true);
        if (inv.value("ocarinaTime", false)) {
            gSaveContext.inventory.items[SLOT_OCARINA] = ITEM_OCARINA_TIME;
        } else if (inv.value("ocarinaFairy", false) && gSaveContext.inventory.items[SLOT_OCARINA] == 0xFF) {
            gSaveContext.inventory.items[SLOT_OCARINA] = ITEM_OCARINA_FAIRY;
        }
        int hookLevel = inv.value("hookshotLevel", 0);
        if (hookLevel >= 2) {
            gSaveContext.inventory.items[SLOT_HOOKSHOT] = ITEM_LONGSHOT;
        } else if (hookLevel == 1 && gSaveContext.inventory.items[SLOT_HOOKSHOT] == 0xFF) {
            gSaveContext.inventory.items[SLOT_HOOKSHOT] = ITEM_HOOKSHOT;
        }
        if (hookLevel >= 3) {
            nei->ultrashotOwned = 1;
        }
        if (inv.value("pictobox", false))
            nei->pictoboxOwned = 1;
        if (inv.value("powderKeg", false))
            nei->powerKegOwned = 1;
        if (inv.contains("powderKegCount")) {
            int c = inv["powderKegCount"].get<int>();
            if (c > nei->powerKegCount)
                nei->powerKegCount = (uint8_t)std::min(c, 5);
        }
        if (inv.value("net", false))
            nei->netEquipped = 1;
        if (inv.contains("bottomlessMode"))
            nei->bottomlessBottleMode = (uint8_t)inv["bottomlessMode"].get<int>();
        if (inv.contains("bottomlessContent"))
            nei->bottomlessContent = (uint8_t)inv["bottomlessContent"].get<int>();
        if (inv.contains("bottomlessCount"))
            nei->bottomlessCount = (uint8_t)inv["bottomlessCount"].get<int>();
    }

    if (sh.contains("bottleSlots") && sh["bottleSlots"].is_array()) {
        for (int i = 0; i < 8 && i < (int)sh["bottleSlots"].size(); i++) {
            uint8_t t = (uint8_t)sh["bottleSlots"][i].get<int>(); // already OoT ids
            if (t == FC_BOTTLE_UNMAPPED) {
                t = 0x14; // ITEM_BOTTLE: keep an empty bottle, never store the sentinel
            }
            // Applied VERBATIM, empties included -- see the MM-side note: the old "empty never clears
            // full" guard resurrected consumed contents. Bottles are a VOLATILE leaf now (only the
            // active game publishes them), so there is no stale echo to guard against.
            nei->bottleSlots[i] = t;
        }
    }
    if (sh.contains("ownedItems") && sh["ownedItems"].is_array()) {
        for (int i = 0; i < 48 && i < (int)sh["ownedItems"].size(); i++) {
            const int raw = sh["ownedItems"][i].get<int>();
            // Skip 0x00 as well as 0xFF: an uninitialized slot on either side reads as a raw 0x00,
            // and letting it through writes a spurious item. Above 0xFF never legitimately arrives
            // (EXT ids stay out of this array on both sides) -- refuse it rather than store it.
            if (raw == 0xFF || raw == 0x00 || raw < 0 || raw > 0xFF) {
                continue;
            }
            // Same validity rule as HealBogusOwnedItems: page-2 customs (+ wand) in [0..23], mask ids
            // in [24..47]. Anything else (a peer-side poisoned cell echoing a bottled content, a
            // truncated EXT id) would be stored and then healed away -- or worse, overwrite a real
            // EXT item (the Sheikah Slate 0x0220 was being replaced by 0x20 this way).
            const bool validPage2 =
                (i < 24) && ((raw >= FC_OOT_PAGE2_FIRST && raw <= FC_OOT_PAGE2_LAST) || raw == ITEM_ELEMENTAL_WAND);
            const bool validMask =
                (i >= 24) && raw >= FC_OOT_MM_MASK_ITEM_BASE && raw < FC_OOT_MM_MASK_ITEM_BASE + FC_MM_MASK_COUNT;
            if (validPage2 || validMask) {
                nei->ownedItems[i] = (uint16_t)raw; // additive
            }
        }
    }
    // Shared NEI options/flags (FleetComboOptions.h) — mirror of the extract above.
#define FCO_APPLY(key, field, mode)                        \
    if (sh.contains(key) && sh[key].is_number_integer()) { \
        uint8_t v = (uint8_t)sh[key].get<int>();           \
        if (mode == FCO_MERGE_MAX) {                       \
            if (v > nei->field) {                          \
                nei->field = v;                            \
            }                                              \
        } else {                                           \
            nei->field = v;                                \
        }                                                  \
    }
    FC_COMBO_OPTION_TABLE(FCO_APPLY)
#undef FCO_APPLY

    // Wand rods / slate runes: OR the bits in, and hand the game every rod/rune it did not have yet
    // through its own grant function (which also places the cell item and picks the active mode).
    if (sh.contains("wandRodsOwned") && sh["wandRodsOwned"].is_number_integer()) {
        const uint8_t incoming = (uint8_t)sh["wandRodsOwned"].get<int>();
        const uint8_t gained = (uint8_t)(incoming & ~nei->wandRodsOwned);
        for (uint8_t m = 0; m < 6 && gained != 0; m++) {
            if (gained & (1 << m)) {
                Wand_GrantMode(m);
            }
        }
        nei->wandRodsOwned |= incoming;
    }
    if (sh.contains("slateRunesOwned") && sh["slateRunesOwned"].is_number_integer()) {
        const uint8_t incoming = (uint8_t)sh["slateRunesOwned"].get<int>();
        const uint8_t gained = (uint8_t)(incoming & ~nei->slateRunesOwned);
        for (uint8_t r = 0; r < 4 && gained != 0; r++) {
            if (gained & (1 << r)) {
                Slate_GrantRune(r);
            }
        }
        nei->slateRunesOwned |= incoming;
    }
    if (sh.contains("seasonsOwned") && sh["seasonsOwned"].is_number_integer()) {
        const uint8_t incoming = (uint8_t)sh["seasonsOwned"].get<int>();
        const uint8_t gained = (uint8_t)(incoming & ~nei->seasonsOwned);
        for (uint8_t s = 0; s < 4 && gained != 0; s++) {
            if (gained & (1 << s)) {
                Seasons_GrantSeason(s);
            }
        }
        nei->seasonsOwned |= incoming;
    }

    RepairFlagOwnedCells(nei);
    if (sh.contains("tradeAdultOwned"))
        nei->tradeAdultOwned |= sh["tradeAdultOwned"].get<uint32_t>();
    // ootMasksOwned round-trip (2026-08-07): the field is authored now (child-trade mask fold), so
    // merge it back too — without this, bits earned while playing MM never landed here.
    if (sh.contains("ootMasksOwned"))
        nei->ootMasksOwned |= (uint16_t)sh["ootMasksOwned"].get<uint32_t>();
    if (sh.contains("ootQuestItems"))
        gSaveContext.inventory.questItems |= sh["ootQuestItems"].get<uint32_t>();
    if (sh.contains("gsTokens")) {
        int gs = sh["gsTokens"].get<int>();
        if (gs > gSaveContext.inventory.gsTokens)
            gSaveContext.inventory.gsTokens = (int16_t)gs;
    }
    if (sh.contains("mmQuestItems"))
        nei->mmQuestItems |= sh["mmQuestItems"].get<uint32_t>();
    if (sh.contains("comboObtained") && sh["comboObtained"].is_array()) {
        for (int i = 0; i < FC_COMBO_OBTAINED_SIZE && i < (int)sh["comboObtained"].size(); i++) {
            uint8_t v = (uint8_t)sh["comboObtained"][i].get<int>();
            if (v > nei->comboObtained[i])
                nei->comboObtained[i] = v; // OR/max merge
        }
    }
    // Generic fcId-indexed cross store: MAX-merge the synced counts. comboAppliedFc is untouched, so a
    // count that grows here opens a deficit that ApplyFcRegistryToNatives grants natively next tick.
    if (sh.contains("comboObtainedFc") && sh["comboObtainedFc"].is_array()) {
        int n = std::min<int>(FC_COMBO_OBTAINED_FC_SIZE, (int)sh["comboObtainedFc"].size());
        for (int i = 0; i < n; i++) {
            uint8_t v = (uint8_t)sh["comboObtainedFc"][i].get<int>();
            if (v > nei->comboObtainedFc[i])
                nei->comboObtainedFc[i] = v; // OR/max merge
        }
    }
    if (sh.contains("comboGoalFlags")) {
        nei->comboGoalFlags |= (uint8_t)sh["comboGoalFlags"].get<int>();
    }
    if (sh.contains("comboTriforce")) {
        uint16_t tf = (uint16_t)sh["comboTriforce"].get<int>();
        if (tf > nei->comboTriforce)
            nei->comboTriforce = tf;
    }
    ApplyRegistryToNatives();

    // NOTE: cEquips/dEquips are deliberately NOT applied here — see ApplyEquips below.

    if (sh.contains("form")) {
        MmForm_FleetApplyForm(sh["form"].get<int>());
    }

    // Adult/child age from MM's timeGateAdultMode. QUEUE the target and let the per-frame pump apply it
    // when SwitchAge() can run safely (see FleetSync_ProcessPendingAge). Applying inline here failed when
    // we weren't in gameplay: the flip was silently dropped, we kept publishing our old age, and the
    // peer's toggle got forced back. Queuing + publishing the target (ExtractShared) breaks that loop.
    if (sh.contains("adult")) {
        s8 wantAge = sh["adult"].get<bool>() ? LINK_AGE_ADULT : LINK_AGE_CHILD;
        sFleetPendingAge = (wantAge != gSaveContext.linkAge) ? wantAge : (s8)-1;
    }

    // [FleetSyncAudit] one line per full-state apply: what the peer SAID and what we HAVE now for
    // the fields people report as "not crossing". Compare this line on both sides of a hand-over.
    if (sh.contains("vitals") && sh.contains("upgrades") && sh.contains("inv")) {
        const auto& v = sh["vitals"];
        const auto& u = sh["upgrades"];
        SPDLOG_INFO("[FleetSyncAudit] in: hp={}/{} magic={} isMagic={} dbl={} rupees={} wallet={} str={} wand={:#x} "
                    "slate={:#x} | now: hp={}/{} isMagic={} dbl={} wallet={} wand={:#x} slate={:#x} "
                    "bottles=[{},{},{},{},{},{},{},{}]",
                    v.value("health", -1), v.value("healthCapacity", -1), v.value("magic", -1), v.value("isMagic", -1),
                    v.value("isDoubleMagic", -1), v.value("rupees", -1), u.value("wallet", -1), u.value("strength", -1),
                    sh.value("wandRodsOwned", -1), sh.value("slateRunesOwned", -1), (int)gSaveContext.health,
                    (int)gSaveContext.healthCapacity, (int)gSaveContext.isMagicAcquired,
                    (int)gSaveContext.isDoubleMagicAcquired, (int)CUR_UPG_VALUE(UPG_WALLET), (int)nei->wandRodsOwned,
                    (int)nei->slateRunesOwned, (int)nei->bottleSlots[0], (int)nei->bottleSlots[1],
                    (int)nei->bottleSlots[2], (int)nei->bottleSlots[3], (int)nei->bottleSlots[4],
                    (int)nei->bottleSlots[5], (int)nei->bottleSlots[6], (int)nei->bottleSlots[7]);
    }

    // Everything above wrote Nei_Save()->extEquip* / equipment nibbles directly — the RAM copy the
    // behaviors and draws read must follow now, not at the next scene load.
    ExtEquip_ResyncFromSave();
}

// =================================================================================================
// BUTTON EQUIPS — game-change only, never through FleetNet
// =================================================================================================
// C / D-pad equips used to be ordinary shared state: published by BOTH games ~3x a second and re-sent
// whole on every resync. That made them a one-way ratchet in OoT's favour, and it is exactly what the
// "no matter what I equip in MM, it forces an OoT item onto the button" report was. The loop:
//   - MM publishes 0xFF for anything OoT cannot represent (any ITEM_EXT_BUTTON custom, the pictobox,
//     the Great Fairy's Sword, and even its Ocarina of Time, whose id is 0x00).
//   - OoT reads 0xFF as "keep mine", so it never changes — and keeps publishing its own id.
//   - MM obeys that id and stamps it over the button the player had just set.
// The two snapshots could then never agree either, so the hash verifier kept firing full resyncs,
// which re-stamped the whole set at moments that looked random to the player.
//
// So they are OUT of ExtractShared/ApplyShared entirely: FleetNet neither sends nor applies them, and
// NetResetBaseline strips them from the snapshot so they don't even reach the hash. They travel ONCE,
// in the departure temp file, and the arriving game inherits them under three rules:
//   1. 0xFF from the peer means "this button held something I could not express" -> keep ours.
//   2. A local button holding something the PEER cannot express was put there by the player, in this
//      game, on purpose -> keep it. Only empty or translatable buttons may be replaced.
//   3. Never equip an item this save does not own, and always write cButtonSlots next to buttonItems.
//      An id without its slot is a phantom button the rest of the game cannot resolve.
void ExtractEquips(nlohmann::json& sh) {
    // Publish 0xFF for anything MM has no relative for, so the peer's rule 1 keeps its own button.
    auto pub = [](uint8_t id) -> uint8_t {
        return (id == ITEM_NONE || FcEquip_OotToMm(id) == 0xFF) ? (uint8_t)0xFF : id;
    };
    nlohmann::json c = nlohmann::json::array();
    for (int b = 1; b <= 3; b++) {
        c.push_back(pub(gSaveContext.equips.buttonItems[b]));
    }
    sh["cEquips"] = c;
    nlohmann::json d = nlohmann::json::array();
    for (int b = 4; b <= 7; b++) {
        d.push_back(pub(gSaveContext.equips.buttonItems[b]));
    }
    sh["dEquips"] = d;
}

// Inventory slot holding `item`, or -1 if this save does not have it.
int FindInvSlot(uint8_t item) {
    for (int s = 0; s < (int)ARRAY_COUNT(gSaveContext.inventory.items); s++) {
        if (gSaveContext.inventory.items[s] == item) {
            return s;
        }
    }
    return -1;
}

// btn: 1-3 C, 4-7 D-pad. cButtonSlots is indexed btn-1 in the SOH layout (see z_parameter.c).
void ApplyOneButton(int btn, uint8_t canon) {
    if (canon == 0xFF) {
        return; // rule 1
    }
    const uint8_t cur = gSaveContext.equips.buttonItems[btn];
    if (cur != ITEM_NONE && FcEquip_OotToMm(cur) == 0xFF) {
        return; // rule 2: the player put something MM cannot hold here — don't take it away
    }
    const int slot = FindInvSlot(canon);
    if (slot < 0) {
        return; // rule 3: we don't own it
    }
    gSaveContext.equips.buttonItems[btn] = canon;
    gSaveContext.equips.cButtonSlots[btn - 1] = (uint8_t)slot;
}

void ApplyEquips(const nlohmann::json& sh) {
    if (sh.contains("cEquips") && sh["cEquips"].is_array()) {
        for (int i = 0; i < 3 && i < (int)sh["cEquips"].size(); i++) {
            if (sh["cEquips"][i].is_number_integer()) {
                ApplyOneButton(1 + i, (uint8_t)sh["cEquips"][i].get<int>());
            }
        }
    }
    if (sh.contains("dEquips") && sh["dEquips"].is_array()) {
        for (int i = 0; i < 4 && i < (int)sh["dEquips"].size(); i++) {
            if (sh["dEquips"][i].is_number_integer()) {
                ApplyOneButton(4 + i, (uint8_t)sh["dEquips"][i].get<int>());
            }
        }
    }
}

// Apply a queued cross-game age change once SwitchAge() can run safely (real gameplay, not mid-
// transition). Called every frame from the OnGameFrameUpdate pump. SwitchAge() TOGGLES linkAge, so it
// reaches the target in one call whenever current != target (the only case we queue).
void FleetSync_ProcessPendingAge(void) {
    if (sFleetPendingAge < 0) {
        return;
    }
    if (gSaveContext.linkAge == sFleetPendingAge) {
        sFleetPendingAge = -1; // already there (e.g. applied by a normal transition)
        return;
    }
    if (gPlayState != NULL && gPlayState->transitionTrigger == TRANS_TRIGGER_OFF) {
        SwitchAge();
        sFleetPendingAge = -1;
    }
}

// ---------------------------------------------------------------------------------------------
// Generic fcId-indexed cross-item materialization.
//
// comboObtainedFc[fcId] (synced) counts how many copies of each FC cross item exist in the combo;
// comboAppliedFc[fcId] (local) counts how many this OoT save has already granted natively. When the
// former grows past the latter (e.g. after ApplyShared max-merges an obtain from MM) we grant the
// deficit via the Anchor give idiom (RetrieveItem -> GetGIEntry_Copy -> Randomizer_Item_Give, one
// call per missing copy), then converge applied = obtained.
//
// DOUBLE-COUNT GUARD: Randomizer_Item_Give re-enters the record hook in randomizer.cpp, which would
// bump comboObtainedFc AND comboAppliedFc again (and worse, re-inflate the SYNCED comboObtainedFc and
// send it back to MM). Merely pre-incrementing comboAppliedFc does NOT help, because the hook also
// bumps comboObtainedFc. So we set sApplyingFc for the whole pass; the record hook checks
// FleetSync_IsApplyingFc() and skips recording entirely while we grant. Must run with a live
// gPlayState (Randomizer_Item_Give needs a PlayState) — the caller gates on gPlayState != NULL.
// ---------------------------------------------------------------------------------------------
bool sApplyingFc = false;

void ApplyFcRegistryToNatives() {
    NeiSaveData* nei = Nei_Save();
    sApplyingFc = true; // suppress the record hook's re-entry for the duration of this pass
    for (int fcId = 0; fcId < FCI_MAX; fcId++) {
        int native = FcCombo_NativeForItem(fcId);
        if (native == FCI_NO_ITEM) {
            continue; // no OoT-native relative for this fcId (info-only / MM-only here)
        }
        int deficit = (int)nei->comboObtainedFc[fcId] - (int)nei->comboAppliedFc[fcId];
        if (deficit <= 0) {
            continue;
        }
        GetItemEntry e = Rando::StaticData::RetrieveItem((RandomizerGet)native).GetGIEntry_Copy();
        if (e.modIndex == MOD_RANDOMIZER) { // valid itemTable row (rowless logic-only RGs would assert)
            for (int c = 0; c < deficit; c++) {
                Randomizer_Item_Give(gPlayState, e); // Anchor pattern: one give per missing copy
            }
        }
        // Converge either way so an ungrantable RG doesn't re-run RetrieveItem every frame.
        nei->comboAppliedFc[fcId] = nei->comboObtainedFc[fcId];
    }
    sApplyingFc = false;
}

// ---------------------------------------------------------------------------------------------
// Save sync state
// ---------------------------------------------------------------------------------------------
unsigned long long sLastSeenSyncSeq = 0;
bool sSyncSeqInit = false;
unsigned long long sWaitingAckSeq = 0;
bool sTitleDeleteDone = false;

void RefreshSharedInTemp() {
    nlohmann::json temp;
    ReadTemp(temp);
    nlohmann::json sh = temp.contains("shared") ? temp["shared"] : nlohmann::json::object();
    ExtractShared(sh);
    temp["version"] = 1;
    temp["slot"] = gSaveContext.fileNum;
    temp["shared"] = sh;
    WriteTemp(temp);
}

// =================================================================================================
// FleetNet - continuous Anchor-style state sync over the shared-memory packet rings
// =================================================================================================
// The temp-file handshake above only fires on a SAVE or a game CHANGE. FleetNet keeps the two games
// agreeing the whole time, and it does it WITHOUT a second translator: ExtractShared/ApplyShared
// already map live state <-> the canonical "shared" json, so we just run them continuously and send
// what moved.
//
//   scan  -> ExtractShared into our running snapshot, flatten() it to JSON-pointer leaves
//            ("/vitals/health" -> 16), diff against what we last published, send the changed leaves.
//   apply -> unflatten() the leaves into a PARTIAL shared object and hand it to ApplyShared, which
//            is already partial-safe (every field is contains()/value() guarded with the current
//            value as default), then fold them into our snapshot so we never echo them back.
//   verify-> every few seconds each side sends a hash of its snapshot. Deltas can be lost (ring
//            overrun while a game is mid scene-load), so this is what guarantees convergence:
//            a repeated mismatch triggers a full resend. Without it a single dropped packet would
//            desync the two games permanently.
//
// This block is TEXTUALLY IDENTICAL in Ship and 2ship -- both sides speak the canonical schema, so
// neither needs to know which game it is.

constexpr int kNetScanPeriod = 20;       // frames between delta scans (~3x/sec)
constexpr int kNetVerifyPeriod = 300;    // frames between hash reports (~5s)
constexpr int kNetResyncCooldown = 900;  // min frames between full resends (~15s), anti-loop
constexpr size_t kNetBatchBytes = 3800;  // leave room for the {"op":"delta","d":{}} envelope in 4095
constexpr int kNetMaxDrainPerFrame = 64; // hard cap on packets applied per frame (anti hang: the
                                         // peer can refill the ring while we drain it)

// ---- SWAP TRACE (diagnostic, temporary — see FleetSync.h) ----
// Armed by a swap, counts down per frame. Flushed on every line ON PURPOSE: the whole point is to
// read the log of a process that stopped, and a buffered logger loses exactly the last lines.
int sSwapTrace = 0;
constexpr int kSwapTraceFrames = 180; // ~3s either side of a handover

#define FS_TRACE(...)                                 \
    do {                                              \
        if (sSwapTrace > 0) {                         \
            SPDLOG_WARN("[FleetTrace] " __VA_ARGS__); \
            spdlog::default_logger()->flush();        \
        }                                             \
    } while (0)

nlohmann::json sNetShared = nlohmann::json::object(); // running canonical state (unflattened)

// Repair the null gaps that unflatten() leaves behind, BEFORE ApplyShared ever sees them.
//
// A delta carries only the leaves that changed ("/x/3": 7). unflatten() has to materialise the whole
// container to place index 3, so it emits [null, null, null, 7] — the untouched indices become JSON
// null. ApplyShared then calls .get<>() on one and nlohmann throws type_error.302. That throw is NOT
// local: it is caught out at the delta level, which ABORTS THE ENTIRE REMAINING APPLY and silently
// drops every field after it.
//
// A null here means "this leaf was not in the packet" = UNCHANGED, not "clear it". So fill it from
// the running canonical state. Anything still null afterwards (no canonical value yet) is erased from
// objects; array slots keep their null, since erasing would shift every later index, and callers must
// treat a null array slot as "no value".
//
// Ported from the MM side (2ship FleetSync.cpp), where this landed 2026-07-30. OoT never got it, so
// this game kept aborting applies mid-way and showing intermittent cross-game grants. Skijer's NEI
static void NetFillNullGaps(nlohmann::json& dst, const nlohmann::json& base) {
    if (dst.is_array()) {
        for (size_t i = 0; i < dst.size(); i++) {
            bool haveBase = base.is_array() && (i < base.size());
            if (dst[i].is_null()) {
                if (haveBase) {
                    dst[i] = base[i];
                }
            } else if (haveBase) {
                NetFillNullGaps(dst[i], base[i]);
            }
        }
    } else if (dst.is_object()) {
        for (auto it = dst.begin(); it != dst.end();) {
            bool haveBase = base.is_object() && base.contains(it.key());
            if (it.value().is_null()) {
                if (haveBase && !base[it.key()].is_null()) {
                    it.value() = base[it.key()];
                    ++it;
                } else {
                    it = dst.erase(it); // no canonical value — drop the key so .get<>() is never reached
                }
            } else {
                if (haveBase) {
                    NetFillNullGaps(it.value(), base[it.key()]);
                }
                ++it;
            }
        }
    }
}
nlohmann::json sNetFlat = nlohmann::json::object(); // its flattened form = what the peer has
int sNetScanTick = 0;
int sNetVerifyTick = 0;
int sNetResyncCooldownLeft = 0;
int sNetMismatchStreak = 0;
int sNetFutileResyncs = 0; // consecutive resyncs that did NOT make the hashes agree
bool sNetPrimed = false;   // first scan publishes nothing: it only establishes the baseline

// FNV-1a over the dumped snapshot. nlohmann objects are key-sorted, so the dump -- and therefore
// the hash -- is order-independent on both sides.
uint64_t NetHash(const nlohmann::json& flat) {
    const std::string s = flat.dump();
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

void NetSend(const nlohmann::json& j) {
    FleetShipCombo_PushPacket(j.dump().c_str());
}

// A delta carries two kinds of change, and the split is load-bearing.
//
// Scalars travel as flattened JSON-pointer leaves ("/vitals/rupees": 40), which ApplyShared can
// take partially because every field there is contains()/value() guarded.
//
// ARRAYS travel as WHOLE SUBTREES ("/ownedItems": [ ... ]), never as leaves. Two independent
// reasons, and missing either one corrupts the save:
//   1. unflatten() of a sparse index set fills the gaps with null, and ApplyShared then reads a
//      null as a value (json type_error.302) or writes a bogus item.
//   2. Even when every leaf of the array is queued, the packet batching below would split a large
//      array across two packets, and the second packet unflattens to exactly that sparse, mostly
//      null array. ownedItems alone is 48 entries at ~21 bytes per pointer-keyed leaf, so it does
//      not fit in one packet -- this is what made the bug survive the first fix.
// Sending an array as a single JSON value is also about 5x smaller than one leaf per entry.
struct NetDelta {
    nlohmann::json leaves; // {"/vitals/rupees": 40}
    nlohmann::json arrays; // {"/ownedItems": [ ... ]}
    bool empty() const {
        return leaves.empty() && arrays.empty();
    }
};

// VOLATILE leaves: live meters the PLAYER moves, as opposed to one-way unlocks. Only the ACTIVE
// game may author these. Without that rule the two games fight over rupees: the wallet scales are
// not the same on both sides (OoT can shuffle the child wallet, MM has no "no wallet" state), so
// the frozen game clamps the value to ITS capacity, republishes the clamped number, and the active
// game's real rupees get dragged down -- money visibly draining on its own. Unlocks stay two-way;
// only these follow whoever is actually being played.
bool NetIsVolatileLeaf(const std::string& key) {
    // Live meters AND live contents: anything the PLAYER changes by playing. The inactive game must
    // never author these -- it can only hold a stale copy, and a stale copy echoed back is exactly
    // how a drunk potion came back, ammo walked backwards and a heart container vanished. The active
    // game is the single author; the inactive one takes what it is sent and keeps the peer's value
    // as its own baseline (see NetRescan).
    if (key.rfind("/vitals/health", 0) == 0 || key.rfind("/vitals/magic", 0) == 0 ||
        key.rfind("/vitals/rupees", 0) == 0) {
        return true; // health, healthCapacity, magic, rupees
    }
    if (key.rfind("/bottleSlots", 0) == 0) {
        return true; // the bottle wheel: contents are consumed and caught in the active game only
    }
    // WHAT IS EQUIPPED/WORN is a live choice too, not an unlock: sword, shield, MM form, adult/child.
    // These apply by OVERWRITE, so a parked game's stale copy (any full resync sends the whole
    // snapshot) used to force the active player's equipment right back.
    if (key.rfind("/equippedShield", 0) == 0 || key.rfind("/equippedSword", 0) == 0 || key.rfind("/form", 0) == 0 ||
        key.rfind("/adult", 0) == 0) {
        return true;
    }
    if (key.rfind("/inv/", 0) == 0) {
        const std::string leaf = key.substr(5);
        if (leaf.size() > 4 && leaf.compare(leaf.size() - 4, 4, "Ammo") == 0) {
            return true; // stickAmmo, bombAmmo, bowAmmo, ... slingshotAmmo
        }
        if (leaf == "powderKegCount" || leaf == "bottomlessContent" || leaf == "bottomlessCount") {
            return true;
        }
    }
    return false;
}

// If `key` points inside an array, return that array's root pointer ("/ownedItems"); else "".
std::string NetArrayRootOf(const std::string& key) {
    size_t pos = 0;
    while (true) {
        const size_t next = key.find('/', pos + 1);
        if (next == std::string::npos) {
            return "";
        }
        const std::string path = key.substr(0, next);
        try {
            if (sNetShared.at(nlohmann::json::json_pointer(path)).is_array()) {
                return path;
            }
        } catch (...) {
            return ""; // not a real path in our snapshot
        }
        pos = next;
    }
}

// Send a delta. Scalars are batched to fill packets; every array goes in a packet of its own so it
// can never be split. An array too big even for one packet is dropped with a loud log rather than
// sent half-formed -- a half-formed one is precisely what corrupts the save.
void NetSendDelta(const NetDelta& delta) {
    nlohmann::json batch = nlohmann::json::object();
    auto flush = [&]() {
        if (!batch.empty()) {
            NetSend({ { "op", "delta" }, { "d", batch } });
            batch = nlohmann::json::object();
        }
    };
    for (auto it = delta.leaves.begin(); it != delta.leaves.end(); ++it) {
        batch[it.key()] = it.value();
        if (batch.dump().size() > kNetBatchBytes) {
            // This leaf overflowed the batch: pull it back out, ship the rest, restart with it.
            nlohmann::json held = batch[it.key()];
            batch.erase(it.key());
            flush();
            batch[it.key()] = held;
        }
    }
    flush();

    for (auto it = delta.arrays.begin(); it != delta.arrays.end(); ++it) {
        nlohmann::json pkt = { { "op", "delta" }, { "a", { { it.key(), it.value() } } } };
        const size_t size = pkt.dump().size();
        if (size > kNetBatchBytes) {
            SPDLOG_ERROR("[FleetNet] array {} is {} bytes and does not fit a packet -- not sent", it.key(), size);
            continue;
        }
        NetSend(pkt);
    }
}

// Split a whole flattened snapshot into scalars + whole arrays (used by the full resync).
NetDelta NetSplitAll(const nlohmann::json& flat) {
    NetDelta out;
    out.leaves = nlohmann::json::object();
    out.arrays = nlohmann::json::object();
    for (auto it = flat.begin(); it != flat.end(); ++it) {
        const std::string root = NetArrayRootOf(it.key());
        if (root.empty()) {
            out.leaves[it.key()] = it.value();
        } else if (!out.arrays.contains(root)) {
            out.arrays[root] = sNetShared.at(nlohmann::json::json_pointer(root));
        }
    }
    return out;
}

// Refresh the snapshot from live state. Returns what changed since the last publish.
NetDelta NetRescan() {
    // ExtractShared merges INTO sNetShared, which is what keeps the one-way-unlock bitfields
    // (ootQuestItems / mmQuestItems) from clobbering bits the peer published: same contract the
    // temp-file path relies on, just held in memory instead of re-read from disk 3x a second.
    ExtractShared(sNetShared);
    nlohmann::json flat = sNetShared.flatten();
    const bool active = FleetShipCombo_IsThisGameActive();

    NetDelta out;
    out.leaves = nlohmann::json::object();
    out.arrays = nlohmann::json::object();
    std::vector<std::string> changedArrays;

    for (auto it = flat.begin(); it != flat.end(); ++it) {
        auto prev = sNetFlat.find(it.key());
        if (prev != sNetFlat.end() && *prev == it.value()) {
            continue;
        }
        if (!active && NetIsVolatileLeaf(it.key())) {
            // Not ours to publish right now. Keep the PEER's value as our published baseline so
            // that when we become active again we diff against what they last said, not against
            // our own frozen reading -- otherwise becoming active would replay a stale meter.
            if (prev != sNetFlat.end()) {
                it.value() = *prev;
            }
            continue;
        }
        const std::string root = NetArrayRootOf(it.key());
        if (root.empty()) {
            out.leaves[it.key()] = it.value();
        } else if (std::find(changedArrays.begin(), changedArrays.end(), root) == changedArrays.end()) {
            changedArrays.push_back(root);
        }
    }
    sNetFlat = flat;
    for (const std::string& root : changedArrays) {
        out.arrays[root] = sNetShared.at(nlohmann::json::json_pointer(root));
    }
    return out;
}

void NetHandleDelta(const nlohmann::json& p) {
    const nlohmann::json d = p.value("d", nlohmann::json::object()); // scalar leaves
    const nlohmann::json a = p.value("a", nlohmann::json::object()); // whole arrays
    if (d.empty() && a.empty()) {
        return;
    }
    nlohmann::json partial = nlohmann::json::object();
    if (!d.empty()) {
        nlohmann::json flat = nlohmann::json::object();
        for (auto it = d.begin(); it != d.end(); ++it) {
            flat[it.key()] = it.value();
        }
        partial = flat.unflatten();
        // Repair unflatten()'s null gaps against the canonical state — without this a single null
        // aborts the whole apply via the catch below. See NetFillNullGaps.
        NetFillNullGaps(partial, sNetShared);
    }
    // Arrays are set WHOLE, by pointer -- no unflatten, so null gaps are structurally impossible.
    for (auto it = a.begin(); it != a.end(); ++it) {
        partial[nlohmann::json::json_pointer(it.key())] = it.value();
    }
    try {
        ApplyShared(partial);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[FleetNet] ApplyShared threw on delta: {}", e.what());
        return;
    } catch (...) {
        SPDLOG_ERROR("[FleetNet] ApplyShared threw a non-std exception on delta");
        return;
    }
    // Fold the peer's change into our snapshot BEFORE our next scan, so applying it does not read
    // back as a local change and bounce straight back to them.
    for (auto it = d.begin(); it != d.end(); ++it) {
        sNetFlat[it.key()] = it.value();
    }
    for (auto it = a.begin(); it != a.end(); ++it) {
        // Flatten just this subtree so the snapshot keeps its leaf-wise form.
        nlohmann::json one = nlohmann::json::object();
        one[nlohmann::json::json_pointer(it.key())] = it.value();
        const nlohmann::json oneFlat = one.flatten();
        for (auto lf = oneFlat.begin(); lf != oneFlat.end(); ++lf) {
            sNetFlat[lf.key()] = lf.value();
        }
    }
    sNetShared = sNetFlat.unflatten();
}

void NetSendFullState() {
    NetRescan(); // make sure the snapshot is current before we declare it authoritative
    const NetDelta all = NetSplitAll(sNetFlat);
    SPDLOG_INFO("[FleetNet] full resync: {} scalar leaves + {} arrays", all.leaves.size(), all.arrays.size());
    NetSendDelta(all);
    sNetResyncCooldownLeft = kNetResyncCooldown;
}

// Same call, but it can NEVER throw at its caller. Every warp step calls this (departure publish,
// arrival re-baseline, save handshake), and those callers must complete even if the snapshot is
// unserialisable: a state sync that fails is a desync FleetNet repairs on its next hash round, while
// a warp that fails is a player stuck on a black screen. NetRescan -> ExtractShared and NetSplitAll's
// json_pointer lookups are the throwing parts.
void NetSendFullStateSafe(const char* where) {
    try {
        NetSendFullState();
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[FleetNet] full-state publish threw at {}: {} — skipped (hash validation will retry)", where,
                     e.what());
    } catch (...) { SPDLOG_ERROR("[FleetNet] full-state publish threw a non-std exception at {} — skipped", where); }
}

void NetHandlePacket(const nlohmann::json& p) {
    const std::string op = p.value("op", "");
    if (op == "delta") {
        NetHandleDelta(p);
    } else if (op == "hash") {
        // The peer told us what it thinks the state is. Agreeing is the common case and costs
        // nothing; disagreeing once is usually just a delta still in flight, so we only act on a
        // SECOND consecutive mismatch.
        const uint64_t theirs = std::strtoull(p.value("h", "0").c_str(), nullptr, 10);
        if (theirs == NetHash(sNetFlat)) {
            sNetMismatchStreak = 0;
            sNetFutileResyncs = 0;
        } else if (++sNetMismatchStreak >= 2 && sNetResyncCooldownLeft == 0) {
            // A resync that does NOT restore agreement means the two hashes can never match: the
            // translator is asymmetric somewhere (a field one game extracts and the other cannot
            // reproduce). Resending forever would be a silent 15-second storm, so back off hard and
            // say so once -- the fix belongs in ExtractShared, not here.
            if (++sNetFutileResyncs >= 3) {
                if (sNetFutileResyncs == 3) {
                    SPDLOG_ERROR("[FleetNet] repeated resyncs did not reconcile the snapshots -- the shared "
                                 "schema is asymmetric between the two games. Backing off; deltas keep working.");
                }
                sNetResyncCooldownLeft = kNetResyncCooldown * 20; // ~5 min
                sNetMismatchStreak = 0;
                return;
            }
            SPDLOG_WARN("[FleetNet] state hash mismatch twice in a row -- requesting full resync");
            sNetMismatchStreak = 0;
            NetSend({ { "op", "resync" } });
            sNetResyncCooldownLeft = kNetResyncCooldown; // don't ask again while one is inbound
        }
    } else if (op == "resync") {
        NetSendFullStateSafe("resync request");
    } else if (op == "saveRequest") {
        // The peer is about to hand over (game change) or just saved: publish everything we have so
        // its snapshot is complete before it writes its own file. See FleetNet_RequestPeerSave.
        NetSendFullStateSafe("peer saveRequest");
        NetSend({ { "op", "saveAck" } });
    } else if (op == "saveAck") {
        SPDLOG_INFO("[FleetNet] peer acknowledged the save request");
    }
}

void NetPump() {
    if (FleetShipCombo_GetActiveGame() < 0 || gPlayState == NULL) {
        return; // no combo, or no live save context to read/write
    }
    if (sSwapTrace > 0) {
        SPDLOG_WARN("[FleetTrace] === OoT frame {} (active={}) ===", kSwapTraceFrames - sSwapTrace,
                    (int)FleetShipCombo_IsThisGameActive());
        spdlog::default_logger()->flush();
        sSwapTrace--;
    }
    // NO REAL FILE, NO SYNC. gPlayState alone is not "a game is loaded": OoT's TITLE DEMO runs
    // inside a PlayState too, with a throwaway save (its own items, its own hearts, fileNum 0xFF).
    // Syncing from there publishes that junk to MM as if it were the player's state, and MM applies
    // it — and the longer OoT sits on the title, the more of it goes across. That is exactly what
    // "boot with MM as the last played game" does: OoT never leaves the title/file select while the
    // player is off in MM, quietly broadcasting demo state the whole time. Both halves of the sync
    // are gated the same way, so a game with no file loaded neither publishes nor applies.
    if (gSaveContext.gameMode != GAMEMODE_NORMAL || gSaveContext.fileNum < 0 || gSaveContext.fileNum > 2) {
        return;
    }
    if (sNetResyncCooldownLeft > 0) {
        sNetResyncCooldownLeft--;
    }

    // Drain first: apply what the peer sent before scanning, so their changes land in this frame's
    // snapshot instead of racing our own diff.
    //
    // BOUNDED on purpose. The ring holds kFscRingSlots packets and the peer can refill it while we
    // drain (a resync storm, a peer running many frames per frame of ours), so an unbounded `while`
    // is a loop the game can never leave — a hard freeze with the process still "running". Anything
    // left over is drained next frame; the packets are idempotent and the hash round repairs drops.
    char buf[4200];
    int drained = 0;
    while (drained < kNetMaxDrainPerFrame && FleetShipCombo_PopPacket(buf, (int)sizeof(buf))) {
        drained++;
        try {
            NetHandlePacket(nlohmann::json::parse(buf));
        } catch (const std::exception& e) { SPDLOG_ERROR("[FleetNet] bad packet dropped: {}", e.what()); } catch (...) {
            SPDLOG_ERROR("[FleetNet] packet handler threw a non-std exception — dropped");
        }
    }
    if (drained >= kNetMaxDrainPerFrame) {
        SPDLOG_WARN("[FleetNet] drain cap hit ({} packets this frame) — the rest waits for the next frame", drained);
    }

    // The scan reads the whole live save through ExtractShared, so it throws on exactly the kind of
    // state a fresh check can introduce. It must never take the frame — or the warp logic that runs
    // in the same hook — down with it.
    try {
        if (++sNetScanTick >= kNetScanPeriod) {
            sNetScanTick = 0;
            const NetDelta changed = NetRescan();
            if (!sNetPrimed) {
                // First scan after boot/arrival: the "changes" are just the entire existing state, and
                // the peer already has it from the arrival overlay. Publishing it would be a pointless
                // storm, so we only record the baseline.
                sNetPrimed = true;
            } else if (!changed.empty()) {
                NetSendDelta(changed);
            }
        }

        if (++sNetVerifyTick >= kNetVerifyPeriod) {
            sNetVerifyTick = 0;
            NetSend({ { "op", "hash" }, { "h", std::to_string(NetHash(sNetFlat)) } });
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[FleetNet] scan/verify threw: {} — skipped this round", e.what());
    } catch (...) { SPDLOG_ERROR("[FleetNet] scan/verify threw a non-std exception — skipped this round"); }
}

// Re-seed the snapshot from a known-agreed state (the temp-file overlay at a departure/arrival) and
// re-prime, so the first scan after a game change does not report the whole save as "changed".
void NetResetBaseline(const nlohmann::json& sh) {
    sNetShared = sh.is_object() ? sh : nlohmann::json::object();
    // Button equips are a game-change-only payload (see ExtractEquips). The departure block we are
    // re-baselining from carries them, so strip them here: leaving them in would put them back in the
    // diff and in the verify hash, which is the whole bug this split exists to kill.
    sNetShared.erase("cEquips");
    sNetShared.erase("dEquips");
    sNetFlat = sNetShared.flatten();
    sNetPrimed = false;
    sNetMismatchStreak = 0;
    sNetScanTick = 0;
}

void HandleOwnSave(int32_t fileNum, int32_t sectionID) {
    if (FleetShipCombo_GetActiveGame() < 0 || !FleetShipCombo_IsThisGameActive()) {
        return; // combo off, or we're the frozen responder (avoid signal loops)
    }
    if (sectionID != SECTION_ID_BASE) {
        return; // only full saves
    }
    // Remember WHERE the player last saved (this game = OoT, this slot) so the next combo boot resumes
    // here. Updated ONLY on a real save, unlike isPlayerIn2Ship which tracks every window switch.
    if (fileNum >= 0 && fileNum <= 2) {
        CVarSetInteger("gFleetCombo.LastSavedGame", 0);
        CVarSetInteger("gFleetCombo.LastSavedSlot", fileNum);
        CVarSave();
    }
    RefreshSharedInTemp();
    // Publish everything BEFORE signalling: the responder saves its own slot the moment it sees the
    // signal, so any delta still queued behind it would land in the peer's file one save too late.
    NetSendFullStateSafe("own save");
    FleetShipCombo_SignalSyncSave(fileNum);
    sWaitingAckSeq = FleetShipCombo_GetSyncSaveSeq();
}

int sHoleGrabCooldown = 0; // frames the fleet hole may not GRAB after an arrival (visible, inert)

void ProcessSignals() {
    FS_TRACE("C1. ProcessSignals enter — PollGuestAlive next (this is what closes Ship if MM is gone)");
    // Guest watchdog FIRST, and outside the combo gate below: if 2ship crashed, was closed or hung,
    // this window is showing (or about to show) a frame of a game that no longer exists. There is
    // nothing left to sync — there is a combo to shut down.
    FleetShipCombo_PollGuestAlive();
    FS_TRACE("C2. PollGuestAlive done — guest still considered alive");

    if (FleetShipCombo_GetActiveGame() < 0) {
        return;
    }
    // Cross-game restart: the OTHER game reset -> reset ourselves too. DoLocalReset does NOT re-signal,
    // so this never ping-pongs.
    if (FleetShipCombo_ConsumeRestartRequest()) {
        FleetCombo_DoLocalReset();
        // Both games restarted; the player comes back on OoT's title screen. MM's own title/file
        // select are never shown by the combo, so MM stays frozen off-screen on its logo.
        FleetShipCombo_YieldToOoT();
        return;
    }
    // Materialize any FC cross items obtained in the OTHER game (max-merged into comboObtainedFc by
    // ApplyShared). Randomizer_Item_Give needs a live PlayState, so gate on gPlayState. Self-healing:
    // an unapplied deficit lives in the persisted+synced registry and is granted on a later frame if
    // this one lacks a play context, so no grant is ever lost even if a save happens in between.
    if (gPlayState != NULL) {
        ApplyFcRegistryToNatives();
    }
    if (sHoleGrabCooldown > 0) {
        sHoleGrabCooldown--;
    }
    unsigned long long seq = FleetShipCombo_GetSyncSaveSeq();
    if (!sSyncSeqInit) {
        sSyncSeqInit = true;
        sLastSeenSyncSeq = seq; // don't react to pre-attach signals
    }
    // RESPONDER: the other (active) game saved -> absorb shared + save our slot + ack.
    if (seq != sLastSeenSyncSeq) {
        sLastSeenSyncSeq = seq;
        // Same "no real file, no sync" rule as the pump: absorbing the other game's state into a
        // title demo (and then SAVING that) is how a session that boots straight into MM corrupts
        // the OoT half of the pair. Consume the signal either way so we don't re-handle it later.
        const bool haveRealFile = gPlayState != NULL && gSaveContext.gameMode == GAMEMODE_NORMAL &&
                                  gSaveContext.fileNum >= 0 && gSaveContext.fileNum <= 2;
        if (!FleetShipCombo_IsThisGameActive() && !haveRealFile) {
            SPDLOG_WARN("[FleetSync] save signal ignored: OoT has no file loaded (gameMode={} fileNum={})",
                        (int)gSaveContext.gameMode, (int)gSaveContext.fileNum);
            FleetShipCombo_AckSyncSave(seq); // ack anyway: MM must not wait on a game with no save
        } else if (!FleetShipCombo_IsThisGameActive()) {
            nlohmann::json temp;
            if (ReadTemp(temp) && temp.contains("shared")) {
                // Guard the deserialize: MM's shared block after a check picked up there can carry
                // rando/FC data whose parse throws (nlohmann .at()); an uncaught throw here would
                // std::terminate SoH. Catch + log so the host survives.
                try {
                    ApplyShared(temp["shared"]);
                } catch (const std::exception& e) {
                    SPDLOG_ERROR("[FleetSync] ApplyShared threw (responder): {}", e.what());
                } catch (...) { SPDLOG_ERROR("[FleetSync] ApplyShared threw a non-std exception (responder)"); }
            }
            int slot = FleetShipCombo_GetSyncSaveSlot();
            // The OTHER (active) game = MM just saved: record MM + its slot as the last-saved location
            // so the next combo boot resumes into MM. OoT is the host, so it owns this persistent record
            // for BOTH games.
            if (slot >= 0 && slot <= 2) {
                CVarSetInteger("gFleetCombo.LastSavedGame", 1);
                CVarSetInteger("gFleetCombo.LastSavedSlot", slot);
                CVarSave();
            }
            if (slot >= 0 && slot <= 2 && gSaveContext.fileNum == slot && gPlayState != NULL) {
                // Parked in the waiting room, the file must still say where the player REALLY is:
                // swap the real entrance/scene in around the write, then put the room back (we are
                // still parked). No-op when not parked.
                FleetShipCombo_LimboSaveShadowBegin();
                SaveManager::Instance->SaveFile(slot);
                FleetShipCombo_LimboSaveShadowEnd();
            }
            FleetShipCombo_AckSyncSave(seq);
        }
    }
    // REQUESTER: our save was absorbed by the other exe -> both files combined, delete the temp.
    if (sWaitingAckSeq != 0 && FleetShipCombo_GetSyncSaveAck() >= sWaitingAckSeq) {
        sWaitingAckSeq = 0;
        DeleteTemp();
    }
    // Continuous state sync. Runs in BOTH exes, active or frozen -- the frozen one still gets this
    // hook, which is exactly what makes the responder path above work.
    NetPump();
}

void RegisterFleetSync() {
#ifdef COMBO_BUILD
    // Shared state travels through ComboShip's merged save container (Combo_ReadGameSave /
    // WriteGameSave), and erasing a pair is its SetDeleteForeignSave seam. Installing the file-mirror
    // pumps too would give the same state two owners — and the title-screen MM-save wipe is fatal here.
    return;
#endif
    // Both of these run every frame and touch JSON built from live save state, so both can throw on
    // data a fresh check introduced. Uncaught, that propagates out of the game loop — and it is the
    // same per-frame hook the warp pipeline uses, so one bad frame could swallow a warp step. Swallow
    // + log instead, and let the next frame try again.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveFile>([](int32_t fileNum, int32_t sectionID) {
        try {
            HandleOwnSave(fileNum, sectionID);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[FleetSync] save handshake threw: {}", e.what());
        } catch (...) { SPDLOG_ERROR("[FleetSync] save handshake threw a non-std exception"); }
    });
    // A combo file is ONE save living in two processes, so erasing it has to erase both halves.
    // Leaving MM's half behind is worse than an orphan file: the next combo file created in that
    // slot would find an MM save from the old seed sitting there, and the two games would hand out
    // items from two different fills. Only inside a combo — a plain Ship erasing a plain file must
    // not reach into 2ship's saves.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnDeleteFile>([](int32_t fileNum) {
        if (!CVarGetInteger("isFleetShipCombo.Enabled", 0) || FleetShipCombo_GetActiveGame() < 0) {
            return;
        }
        if (fileNum < 0 || fileNum > 2) {
            return;
        }
        SPDLOG_INFO("[FleetSync] OoT file {} erased -> deleting MM's half of the pair", fileNum);
        FleetOracle_QueueDeleteSave(fileNum);
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>([]() {
        try {
            ProcessSignals();
            FleetSync_ProcessPendingAge(); // apply a queued cross-game SwitchAge when safe
        } catch (const std::exception& e) { SPDLOG_ERROR("[FleetSync] signal pump threw: {}", e.what()); } catch (...) {
            SPDLOG_ERROR("[FleetSync] signal pump threw a non-std exception");
        }
    });
    // A new file starts from a blank snapshot. The snapshot is only "what did I last publish"; if it
    // survives a file load it describes a DIFFERENT save, and the first diff then either republishes
    // the old file's state or suppresses the new file's. (Anchor does the equivalent: joining a room
    // assigns the state outright rather than merging into whatever was there.)
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t fileNum) {
        (void)fileNum;
        NetResetBaseline(nlohmann::json::object());
        SPDLOG_INFO("[FleetNet] file loaded — snapshot reset");
    });
}

// ---------------------------------------------------------------------------------------------
// Door_Ana fleet-hole registry
// ---------------------------------------------------------------------------------------------
void* sFleetHole = nullptr;
bool sHoleFallPending = false;

} // namespace

extern "C" {

// Absolute path of a file inside the shared fleet folder (<ShipDir>/fleet/<name>), created on demand.
// Both exes resolve this to the SAME physical folder — Ship uses its own exe dir, 2ship the parent of
// its own — which is what makes it the place for anything the two games must literally share rather
// than copy, like the combo pictograph. Returns "" if the exe dir can't be resolved; the returned
// pointer stays valid until the next call. Skijer's NEI
const char* FleetSync_SharedFilePath(const char* name) {
    static std::string sPath;
    sPath.clear();
    std::filesystem::path dir = SelfExeDir();
    if (dir.empty() || name == nullptr) {
        return "";
    }
    std::error_code ec;
    std::filesystem::create_directories(dir / "fleet", ec);
    sPath = (dir / "fleet" / name).string();
    return sPath.c_str();
}

// NOTHING in here may throw at the caller. It is called from C (custom_items_common.c) on the frame
// the game flips to MM, ONE LINE before FleetShipCombo_RequestWarp — so an escaping exception both
// crosses a C frame (undefined behaviour) and skips the flip itself, leaving the player at full
// black with the warp never requested and no crash to show for it. Everything it does is
// best-effort bookkeeping: the anchor, the shared extract and the FleetNet publish can all be lost
// and still be repaired later (the peer keeps its previous state and FleetNet reconciles). The warp
// cannot. So: serialise what we can, log what we can't, and always return normally.
void FleetSync_WriteDeparture(int slot) {
    if (FleetShipCombo_GetActiveGame() < 0) {
        return;
    }
    if (slot < 0 || slot > 2) {
        SPDLOG_WARN("[FleetSync] departure with no real file loaded (slot {}) — ignored", slot);
        return; // never anchor/extract an unloaded save (title demo etc.)
    }
    nlohmann::json temp;
    nlohmann::json sh;
    try {
        ReadTemp(temp);
        temp["version"] = 1;
        temp["slot"] = slot;
        temp["oot"] = SaveManager::Instance->SaveToJsonObject(); // full anchor, live state, no disk IO
        sh = temp.contains("shared") ? temp["shared"] : nlohmann::json::object();
        ExtractShared(sh);
        ExtractEquips(sh); // game-change payload: the ONLY moment button equips are published
        temp["shared"] = sh;
        WriteTemp(temp);
    } catch (const std::exception& e) {
        // Drop the anchor rather than write a half-serialised one: MM prefers "no anchor" (it keeps
        // the save it loaded) over a truncated one it would splat over a good save.
        temp.erase("oot");
        SPDLOG_ERROR("[FleetSync] departure serialization threw: {} (slot {}) — departure still committed", e.what(),
                     slot);
    } catch (...) {
        temp.erase("oot");
        SPDLOG_ERROR("[FleetSync] departure serialization threw a non-std exception (slot {})", slot);
    }
    // We are about to freeze: hand the peer everything, so whatever it does while we are asleep is
    // built on our final state rather than on deltas that may still have been in flight.
    try {
        NetResetBaseline(sh.is_object() ? sh : nlohmann::json::object());
        NetSendFullStateSafe("departure");
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[FleetSync] departure re-baseline threw: {} — departure still committed", e.what());
    } catch (...) {
        SPDLOG_ERROR("[FleetSync] departure re-baseline threw a non-std exception — departure still committed");
    }
    SPDLOG_INFO("[FleetSync] OoT departure written (slot {})", slot);
}

void FleetSync_ApplyArrival(int slot) {
    (void)slot;
    if (FleetShipCombo_GetActiveGame() < 0) {
        return;
    }
    nlohmann::json temp;
    if (!ReadTemp(temp)) {
        return;
    }
    bool dirty = false;
    if (temp.contains("oot")) {
        try {
            SaveManager::Instance->LoadFromJsonObject(temp["oot"]); // full state restore (anchor wins over disk)
            SPDLOG_INFO("[FleetSync] OoT anchor restored");
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[FleetSync] OoT anchor restore threw: {} — kept the loaded save", e.what());
        } catch (...) { SPDLOG_ERROR("[FleetSync] OoT anchor restore threw a non-std exception"); }
        temp.erase("oot");
        dirty = true;
    }
    if (temp.contains("shared")) {
        // Guard the shared overlay: MM's shared block after a check picked up there can carry rando/FC
        // data whose parse throws (nlohmann .at()/.get type). An UNCAUGHT throw here crashed OoT on
        // ARRIVAL, and 2ship's host-death watchdog then exit(0)'d — so it LOOKED like 2ship closed after
        // "get a check + return to OoT". Catch + log so the host survives.
        try {
            ApplyShared(temp["shared"]);
            // Button equips: arrival only, and ONE-SHOT. Consuming them here means a stale temp block
            // (one whose departure we already absorbed) can never re-stamp the player's buttons later.
            ApplyEquips(temp["shared"]);
            temp["shared"].erase("cEquips");
            temp["shared"].erase("dEquips");
            dirty = true;
            SPDLOG_INFO("[FleetSync] shared overlay applied (OoT)");
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[FleetSync] ApplyShared threw on arrival: {}", e.what());
        } catch (...) { SPDLOG_ERROR("[FleetSync] ApplyShared threw a non-std exception on arrival"); }
    }
    // Re-baseline on the state we just arrived with, then ASK the peer for its full state (Anchor's
    // request/response shape). The temp file only carries what the peer knew when it wrote the
    // departure; anything it changed afterwards -- or any delta lost while we were frozen -- comes
    // back through this. Its answer is a normal resync, applied by the pump.
    // Guarded for the same reason as the departure: the arrival pipeline continues into the
    // destination overrides right after this returns (and the caller may be a C frame), so none of
    // it may be skipped because a snapshot could not be flattened.
    try {
        NetResetBaseline(temp.contains("shared") ? temp["shared"] : nlohmann::json::object());
        NetSend({ { "op", "saveRequest" } });
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[FleetSync] arrival re-baseline threw: {} — arrival continues", e.what());
    } catch (...) { SPDLOG_ERROR("[FleetSync] arrival re-baseline threw a non-std exception — arrival continues"); }
    if (dirty) {
        WriteTemp(temp);
    }
}

void FleetSync_BeginSwapTrace(const char* why) {
    sSwapTrace = kSwapTraceFrames;
    SPDLOG_WARN("[FleetTrace] ===== OoT swap trace armed ({}) =====", why ? why : "?");
    spdlog::default_logger()->flush();
}

void FleetSync_SwapTrace(const char* step) {
    FS_TRACE("{}", step ? step : "?");
}

void FleetSync_OnTitleScreen(void) {
    if (sTitleDeleteDone || FleetShipCombo_GetActiveGame() < 0) {
        return;
    }
    sTitleDeleteDone = true;
    DeleteTemp();
    SPDLOG_INFO("[FleetSync] title screen -> temp file deleted");
}

void FleetSync_RegisterFleetHole(void* actor) {
    sFleetHole = actor;
}
int FleetSync_IsFleetHole(void* actor) {
    return actor != nullptr && actor == sFleetHole;
}
void FleetSync_OnHoleFall(void) {
    sHoleFallPending = true;
}
int FleetSync_HoleFallPending(void) {
    return sHoleFallPending ? 1 : 0;
}
void FleetSync_ClearHoleFall(void) {
    sHoleFallPending = false;
}
void FleetSync_SetHoleGrabCooldown(int frames) {
    sHoleGrabCooldown = frames;
}
int FleetSync_HoleGrabInert(void) {
    return sHoleGrabCooldown > 0 ? 1 : 0;
}

// Cross-TU guard: the randomizer record hook (randomizer.cpp Randomizer_Item_Give) queries this and
// skips recording while ApplyFcRegistryToNatives is granting the FC deficit, preventing a double-count.
int FleetSync_IsApplyingFc(void) {
    return sApplyingFc ? 1 : 0;
}

} // extern "C"

static RegisterShipInitFunc initFleetSync(RegisterFleetSync, {});

#ifdef COMBO_BUILD
// ComboShip: the file-mirror pumps above stay off, but the two games still reconcile at every
// switch — the peer pulls this snapshot (FleetSharedItems::PullFromPeer) and applies it max-merge.
// Save-only: valid while this game is dormant.
extern "C" FLEET_COMBO_EXPORT const char* SOH_ExtractSharedState(void) {
    static std::string cached;
    nlohmann::json sh;
    ExtractShared(sh);
    cached = sh.dump();
    return cached.c_str();
}

extern "C" void FleetSync_ApplySharedState(const char* json) {
    if (json == nullptr || json[0] == '\0') {
        return;
    }
    try {
        ApplyShared(nlohmann::json::parse(json));
    } catch (const std::exception& e) { SPDLOG_ERROR("[FleetSync] ApplySharedState failed: {}", e.what()); }
}
#endif
