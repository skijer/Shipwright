#include "item.h"
#include "item_location.h"

#include "SeedContext.h"
#include "logic.h"
#include "3drando/item_pool.hpp"
#include "z64item.h"
#include "variables.h"
#include "functions.h"
#include "../../OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizer.h"

// Extended Inventory for Custom Items (Page 2)
extern "C" {
#include "mods/items/custom_items.h"
#include "mods/items/logic/weapon_upgrades.h" // NEI chains: which upgrade level is owned
#include "mods/nei_save.h"                    // ultrashotOwned (hookshot chain level 3)
#include "mods/extended_inventory.h"          // ExtInv_GetSlotItem (Roc's chain reads the REAL slot)
u8 Cane_HasSkill(u8 skill);                   // item_cane_of_somaria.h (cane chain resolution)
}

namespace Rando {
Item::Item()
    : randomizerGet(RG_NONE), type(ITEMTYPE_ITEM), getItemId(GI_NONE), advancement(false), hintKey(RHT_NONE),
      progressive(false), price(0) {
}
Item::Item(const RandomizerGet randomizerGet_, Text name_, const ItemType type_, const int16_t getItemId_,
           const bool advancement_, LogicVal logicVal_, const RandomizerHintTextKey hintKey_, const uint16_t itemId_,
           const uint16_t objectId_, const uint16_t gid_, const uint16_t textId_, const uint16_t field_,
           const int16_t chestAnimation_, const GetItemCategory category_, const uint16_t modIndex_, Text article_,
           const std::string color_, const bool progressive_, const uint16_t price_)
    : randomizerGet(randomizerGet_), name(std::move(name_)), type(type_), getItemId(getItemId_),
      advancement(advancement_), logicVal(logicVal_), hintKey(hintKey_), category(category_),
      article(std::move(article_)), color(std::move(color_)), progressive(progressive_), price(price_) {

    if (modIndex_ == MOD_RANDOMIZER || getItemId > 0x7D) {
        giEntry = std::make_shared<GetItemEntry>(GetItemEntry{
            itemId_, field_, static_cast<int16_t>((chestAnimation_ != CHEST_ANIM_SHORT ? 1 : -1) * (gid_ + 1)), textId_,
            objectId_, modIndex_, TABLE_RANDOMIZER, static_cast<int16_t>(randomizerGet_), gid_, true, ITEM_FROM_NPC,
            category_, static_cast<uint16_t>(randomizerGet_), MOD_RANDOMIZER, NULL });
    } else {
        giEntry = std::make_shared<GetItemEntry>(GetItemEntry{
            itemId_, field_, static_cast<int16_t>((chestAnimation_ != CHEST_ANIM_SHORT ? 1 : -1) * (gid_ + 1)), textId_,
            objectId_, modIndex_, TABLE_VANILLA, getItemId_, gid_, true, ITEM_FROM_NPC, category_, itemId_, modIndex_,
            NULL });
    }
}

Item::Item(const RandomizerGet randomizerGet_, Text name_, const ItemType type_, const int16_t getItemId_,
           const bool advancement_, LogicVal logicVal_, const RandomizerHintTextKey hintKey_,
           const GetItemCategory category_, Text article_, const std::string color_, const bool progressive_,
           const uint16_t price_)
    : randomizerGet(randomizerGet_), name(std::move(name_)), type(type_), getItemId(getItemId_),
      advancement(advancement_), logicVal(logicVal_), hintKey(hintKey_), category(category_),
      article(std::move(article_)), color(std::move(color_)), progressive(progressive_), price(price_) {
}

void Item::ApplyEffect() const {
    auto ctx = Rando::Context::GetInstance();
    auto logic = ctx->GetLogic();
    if (!logic->CalculatingAvailableChecks) {
        logic->ApplyItemEffect(StaticData::RetrieveItem(randomizerGet), true);
    }
    logic->Set(logicVal, true);
}

void Item::UndoEffect() const {
    auto ctx = Rando::Context::GetInstance();
    auto logic = ctx->GetLogic();
    if (!logic->CalculatingAvailableChecks) {
        logic->ApplyItemEffect(StaticData::RetrieveItem(randomizerGet), false);
    }
    logic->Set(logicVal, false);
}

const Text& Item::GetName() const {
    return name;
}

const Text& Item::GetArticle() const {
    return article;
}

const std::string& Item::GetColor() const {
    return color;
}

bool Item::IsAdvancement() const {
    // With the shop shield/tunic gate on, a found Deku/Hylian Shield unlocks its shop copy, so it must
    // be treated as progression. Tunics already are.
    if (!advancement && (randomizerGet == RG_DEKU_SHIELD || randomizerGet == RG_HYLIAN_SHIELD) &&
        Context::GetInstance()->GetOption(RSK_SHOP_SHIELDS_AND_TUNICS_ONLY_REFILL).Is(RO_GENERIC_ON)) {
        return true;
    }
    return advancement;
}

int Item::GetItemID() const {
    return getItemId;
}

ItemType Item::GetItemType() const {
    return type;
}

LogicVal Item::GetLogicVal() const {
    return logicVal;
}

RandomizerGet Item::GetRandomizerGet() const {
    return randomizerGet;
}

uint16_t Item::GetPrice() const {
    return price;
}

// Rows whose GI entry depends on SAVE STATE: the switch in GetGIEntry is what turns one pool item
// into the level actually being received, so these must never be served from the cached giEntry.
//
// This used to read `giEntry->itemId != RG_PROGRESSIVE_BOMBCHU_BAG` — an ItemID compared against a
// RandomizerGet — so every row built with the full constructor (i.e. every row that HAS a cached
// entry) skipped resolution entirely. The chains that still worked were the ones built with the
// short constructor (no giEntry: hookshot, strength, scale, agony...); the ones that carry their own
// item/object/icon (Cane of Somaria, Progressive Roc) always presented as level 1 no matter how many
// copies you received. Skijer's NEI
static bool ItemResolvesFromState(RandomizerGet rg) {
    switch (rg) {
        case RG_PROGRESSIVE_STICK_UPGRADE:
        case RG_PROGRESSIVE_NUT_UPGRADE:
        case RG_PROGRESSIVE_BOMB_BAG:
        case RG_PROGRESSIVE_BOW:
        case RG_PROGRESSIVE_SLINGSHOT:
        case RG_PROGRESSIVE_OCARINA:
        case RG_PROGRESSIVE_HOOKSHOT:
        case RG_PROGRESSIVE_ROCS:
        case RG_PROGRESSIVE_STRENGTH:
        case RG_PROGRESSIVE_WALLET:
        case RG_PROGRESSIVE_SCALE:
        case RG_PROGRESSIVE_MAGIC_METER:
        case RG_PROGRESSIVE_GORONSWORD:
        case RG_PROGRESSIVE_KOKIRI_SWORD:
        case RG_PROGRESSIVE_MASTER_SWORD:
        case RG_PROGRESSIVE_BGS:
        case RG_PROGRESSIVE_HAMMER:
        case RG_PROGRESSIVE_BOMBCHU_BAG:
        case RG_STONE_OF_AGONY:
        case RG_CANE_OF_SOMARIA:
            return true;
        default:
            return false;
    }
}

std::shared_ptr<GetItemEntry> Item::GetGIEntry() const { // NOLINT(*-no-recursion)
    if (giEntry != nullptr && !ItemResolvesFromState(randomizerGet)) {
        return giEntry;
    }
    std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
    auto logic = ctx->GetLogic();
    RandomizerGet actual = RG_NONE;
    const bool tycoonWallet = OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(RSK_INCLUDE_TYCOON_WALLET);
    const u8 infiniteUpgrades = OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(RSK_INFINITE_UPGRADES);
    switch (randomizerGet) {
        case RG_PROGRESSIVE_STICK_UPGRADE:
            switch (logic->CurrentUpgrade(UPG_STICKS)) {
                case 0:
                    if (ctx->GetOption(RSK_SHUFFLE_DEKU_STICK_BAG)) {
                        actual = RG_DEKU_STICK_BAG;
                        break;
                    }
                    [[fallthrough]];
                case 1:
                    if (infiniteUpgrades == RO_INF_UPGRADES_CONDENSED_PROGRESSIVE) {
                        actual = RG_STICK_UPGRADE_INF;
                    } else {
                        actual = RG_DEKU_STICK_CAPACITY_20;
                    }
                    break;
                case 2:
                    actual = RG_DEKU_STICK_CAPACITY_30;
                    break;
                case 3:
                case 4:
                    if (infiniteUpgrades == RO_INF_UPGRADES_PROGRESSIVE) {
                        actual = RG_STICK_UPGRADE_INF;
                    } else {
                        actual = RG_DEKU_STICK_CAPACITY_30;
                    }
                    break;
                default:
                    break;
            }
            break;
        case RG_PROGRESSIVE_NUT_UPGRADE:
            switch (logic->CurrentUpgrade(UPG_NUTS)) {
                case 0:
                    if (ctx->GetOption(RSK_SHUFFLE_DEKU_NUT_BAG)) {
                        actual = RG_DEKU_NUT_BAG;
                        break;
                    }
                    [[fallthrough]];
                case 1:
                    if (infiniteUpgrades == RO_INF_UPGRADES_CONDENSED_PROGRESSIVE) {
                        actual = RG_NUT_UPGRADE_INF;
                    } else {
                        actual = RG_DEKU_NUT_CAPACITY_30;
                    }
                    break;
                case 2:
                    actual = RG_DEKU_NUT_CAPACITY_40;
                    break;
                case 3:
                case 4:
                    if (infiniteUpgrades == RO_INF_UPGRADES_PROGRESSIVE) {
                        actual = RG_NUT_UPGRADE_INF;
                    } else {
                        actual = RG_DEKU_NUT_CAPACITY_40;
                    }
                    break;
                default:
                    break;
            }
            break;
        case RG_PROGRESSIVE_BOMB_BAG:
            switch (logic->CurrentUpgrade(UPG_BOMB_BAG)) {
                case 0:
                    actual = RG_BOMB_BAG;
                    break;
                case 1:
                    if (infiniteUpgrades == RO_INF_UPGRADES_CONDENSED_PROGRESSIVE) {
                        actual = RG_BOMB_BAG_INF;
                    } else {
                        actual = RG_BIG_BOMB_BAG;
                    }
                    break;
                case 2:
                    actual = RG_BIGGEST_BOMB_BAG;
                    break;
                case 3:
                case 4:
                    if (infiniteUpgrades == RO_INF_UPGRADES_PROGRESSIVE) {
                        actual = RG_BOMB_BAG_INF;
                    } else {
                        actual = RG_BIGGEST_BOMB_BAG;
                    }
                    break;
                default:
                    break;
            }
            break;
        case RG_PROGRESSIVE_BOW:
            switch (logic->CurrentUpgrade(UPG_QUIVER)) {
                case 0:
                    actual = RG_FAIRY_BOW;
                    break;
                case 1:
                    if (infiniteUpgrades == RO_INF_UPGRADES_CONDENSED_PROGRESSIVE) {
                        actual = RG_QUIVER_INF;
                    } else {
                        actual = RG_BIG_QUIVER;
                    }
                    break;
                case 2:
                    actual = RG_BIGGEST_QUIVER;
                    break;
                case 3:
                case 4:
                    if (infiniteUpgrades == RO_INF_UPGRADES_PROGRESSIVE) {
                        actual = RG_QUIVER_INF;
                    } else {
                        actual = RG_BIGGEST_QUIVER;
                    }
                    break;
                default:
                    break;
            }
            break;
        case RG_PROGRESSIVE_SLINGSHOT:
            switch (logic->CurrentUpgrade(UPG_BULLET_BAG)) {
                case 0:
                    actual = RG_FAIRY_SLINGSHOT;
                    break;
                case 1:
                    if (infiniteUpgrades == RO_INF_UPGRADES_CONDENSED_PROGRESSIVE) {
                        actual = RG_BULLET_BAG_INF;
                    } else {
                        actual = RG_BIG_BULLET_BAG;
                    }
                    break;
                case 2:
                    actual = RG_BIGGEST_BULLET_BAG;
                    break;
                case 3:
                case 4:
                    if (infiniteUpgrades == RO_INF_UPGRADES_PROGRESSIVE) {
                        actual = RG_BULLET_BAG_INF;
                    } else {
                        actual = RG_BIGGEST_BULLET_BAG;
                    }
                    break;
                default:
                    break;
            }
            break;
        case RG_PROGRESSIVE_OCARINA:
            switch (logic->CurrentInventory(ITEM_OCARINA_FAIRY)) {
                case ITEM_NONE:
                    actual = RG_FAIRY_OCARINA;
                    break;
                case ITEM_OCARINA_FAIRY:
                case ITEM_OCARINA_TIME:
                    actual = RG_OCARINA_OF_TIME;
                    break;
                default:
                    break;
            }
            break;
        case RG_PROGRESSIVE_HOOKSHOT:
            switch (logic->CurrentInventory(ITEM_HOOKSHOT)) {
                case ITEM_NONE:
                    actual = RG_HOOKSHOT;
                    break;
                case ITEM_HOOKSHOT:
                    actual = RG_LONGSHOT;
                    break;
                case ITEM_LONGSHOT:
                    // NEI chain level 3: Longshot in hand -> the next copy is the Ultrashot.
                    actual = RG_ULTRASHOT;
                    break;
                default:
                    break;
            }
            break;
        case RG_PROGRESSIVE_ROCS:
            // Read the REAL NEI slot: logic->CurrentInventory only sees the vanilla inventory
            // array, so a custom id (0x9E) always came back ITEM_NONE and the SECOND copy still
            // presented as the feather ("me da el item pero el textbox no se muestra bien") even
            // though the give case stepped to the cape correctly.
            switch (ExtInv_GetSlotItem(SLOT_ROCS)) {
                case ITEM_NONE:
                    // First copy is Skijer's feather — resolve to the progressive entry itself.
                    // RG_ROCS_FEATHER is the separate vanilla rando feather (Nayru's Love slot).
                    actual = RG_PROGRESSIVE_ROCS;
                    break;
                case ITEM_ROCS_FEATHER_SKIJER:
                case ITEM_ROCS_CAPE:
                    actual = RG_ROCS_CAPE;
                    break;
                default:
                    break;
            }
            break;
        case RG_PROGRESSIVE_STRENGTH:
            if (!logic->CheckRandoInf(RAND_INF_CAN_GRAB)) {
                actual = RG_POWER_BRACELET;
                break;
            }
            switch (logic->CurrentUpgrade(UPG_STRENGTH)) {
                case 0:
                    actual = RG_GORONS_BRACELET;
                    break;
                case 1:
                    actual = RG_SILVER_GAUNTLETS;
                    break;
                case 2:
                case 3:
                    actual = RG_GOLDEN_GAUNTLETS;
                    break;
                default:
                    break;
            }
            break;
        case RG_PROGRESSIVE_WALLET:
            if (!logic->CheckRandoInf(RAND_INF_HAS_WALLET)) {
                actual = RG_CHILD_WALLET;
                break;
            }
            switch (logic->CurrentUpgrade(UPG_WALLET)) {
                case 0:
                    actual = RG_ADULT_WALLET;
                    break;
                case 1:
                    actual = RG_GIANT_WALLET;
                    break;
                case 2:
                    if (tycoonWallet) {
                        actual = RG_TYCOON_WALLET;
                        break;
                    }
                    // fallthrough
                case 3:
                case 4:
                    if (infiniteUpgrades != RO_INF_UPGRADES_OFF) {
                        actual = RG_WALLET_INF;
                    } else {
                        actual = tycoonWallet ? RG_TYCOON_WALLET : RG_GIANT_WALLET;
                    }
                    break;
                default:
                    break;
            }
            break;
        case RG_PROGRESSIVE_SCALE:
            if (!logic->CheckRandoInf(RAND_INF_CAN_SWIM)) {
                actual = RG_BRONZE_SCALE;
                break;
            }
            switch (logic->CurrentUpgrade(UPG_SCALE)) {
                case 0:
                    actual = RG_SILVER_SCALE;
                    break;
                case 1:
                case 2:
                    actual = RG_GOLDEN_SCALE;
                    break;
                default:
                    break;
            }
            break;
        case RG_PROGRESSIVE_MAGIC_METER:
            switch (logic->GetSaveContext()->magicLevel) {
                case 0:
                    actual = RG_MAGIC_SINGLE;
                    break;
                case 1:
                    actual = RG_MAGIC_DOUBLE;
                    break;
                case 2:
                case 3:
                    if (infiniteUpgrades != RO_INF_UPGRADES_OFF) {
                        actual = RG_MAGIC_INF;
                    } else {
                        actual = RG_MAGIC_DOUBLE;
                    }
                    break;
                default:
                    break;
            }
            break;
        case RG_PROGRESSIVE_GORONSWORD: // todo progressive?
            actual = RG_BIGGORON_SWORD;
            break;
        // NEI weapon chains — these used to fall to `default` (actual = RG_NONE), so every copy
        // presented as the generic "Progressive X" entry and never showed the level being received.
        // Level 1 resolves to the vanilla weapon's own row (full vanilla presentation); the upper
        // levels resolve to their per-level rows. Equip ownership comes from the logic save context
        // (bits 0-2 of inventory.equipment = Kokiri/Master/Biggoron — EQUIP_TYPE_SWORD is nibble 0);
        // upgrade bits live in the NEI save (process-global). Skijer's NEI
        case RG_PROGRESSIVE_KOKIRI_SWORD:
            if (!(logic->GetSaveContext()->inventory.equipment & (1 << EQUIP_INV_SWORD_KOKIRI))) {
                actual = RG_KOKIRI_SWORD;
            } else if (!WeaponUpgrade_HasRazor()) {
                actual = RG_RAZOR_SWORD;
            } else {
                actual = RG_GILDED_SWORD;
            }
            break;
        case RG_PROGRESSIVE_MASTER_SWORD:
            if (!(logic->GetSaveContext()->inventory.equipment & (1 << EQUIP_INV_SWORD_MASTER))) {
                actual = RG_MASTER_SWORD;
            } else {
                actual = RG_TRUE_MASTER_SWORD;
            }
            break;
        case RG_PROGRESSIVE_BGS:
            if (!(logic->GetSaveContext()->inventory.equipment & (1 << EQUIP_INV_SWORD_BIGGORON))) {
                actual = RG_BIGGORON_SWORD;
            } else {
                actual = RG_GREAT_FAIRY_SWORD;
            }
            break;
        case RG_PROGRESSIVE_HAMMER:
            if (logic->CurrentInventory(ITEM_HAMMER) == ITEM_NONE) {
                actual = RG_MEGATON_HAMMER;
            } else {
                actual = RG_IRON_KNUCKLE_AXE;
            }
            break;
        case RG_STONE_OF_AGONY:
            // NEI 2-level chain: the vanilla stone, then the Quartz of Motion. The stone copy keeps
            // resolving to this row's own entry (vanilla presentation); the second copy presents as
            // the Quartz with its own textbox/icon/model.
            if (logic->GetSaveContext()->inventory.questItems & gBitFlags[QUEST_STONE_OF_AGONY]) {
                actual = RG_QUARTZ_OF_MOTION;
            }
            break;
        case RG_CANE_OF_SOMARIA: {
            // Dual Cane: the give order is fixed (kCaneOrder in randomizer.cpp — Statue, Flip,
            // Block, Stone, Platform, Ultrahand), so the number of owned skills says exactly which
            // per-skill identity THIS copy presents as. Reads the real NEI state (Cane_HasSkill),
            // never logic->CurrentInventory (it cannot see NEI slots).
            static const RandomizerGet kCaneLevels[6] = {
                RG_CANE_OF_SOMARIA,  RG_CANE_PACCI_FLIP,       RG_CANE_SOMARIA_BLOCK,
                RG_CANE_PACCI_STONE, RG_CANE_SOMARIA_PLATFORM, RG_CANE_PACCI_ULTRAHAND,
            };
            int owned = 0;
            for (u8 s = 0; s < 6; s++) {
                owned += Cane_HasSkill(s) ? 1 : 0;
            }
            if (owned > 0 && owned <= 5) {
                actual = kCaneLevels[owned];
            }
            break;
        }
        case RG_PROGRESSIVE_BOMBCHU_BAG:
            if (OTRGlobals::Instance->gRandoContext->GetOption(RSK_BOMBCHU_BAG).Is(RO_BOMBCHU_BAG_SINGLE)) {
                if (logic->CurrentInventory(ITEM_BOMBCHU) != ITEM_NONE) {
                    if (infiniteUpgrades != RO_INF_UPGRADES_OFF) {
                        actual = RG_BOMBCHU_INF;
                    } else {
                        actual = RG_BOMBCHU_10;
                    }
                }
            } else if (OTRGlobals::Instance->gRandoContext->GetOption(RSK_BOMBCHU_BAG).Is(RO_BOMBCHU_BAG_PROGRESSIVE)) {
                if (logic->CurrentInventory(ITEM_BOMBCHU) != ITEM_NONE) {
                    if (infiniteUpgrades == RO_INF_UPGRADES_CONDENSED_PROGRESSIVE) {
                        actual = RG_BOMBCHU_INF;
                    } else if (infiniteUpgrades == RO_INF_UPGRADES_PROGRESSIVE) {
                        if (logic->GetSaveContext()->ship.quest.data.randomizer.bombchuUpgradeLevel >= 3) {
                            actual = RG_BOMBCHU_INF;
                        }
                    }
                }
            }
            break;
        default:
            actual = RG_NONE;
            break;
    }
    // `actual == randomizerGet` is a row resolving to ITSELF (level 1 of a chain, e.g. Progressive
    // Roc with an empty slot). Now that the cache no longer short-circuits these rows, recursing
    // into RetrieveItem(actual) would call straight back into this function forever — stack
    // overflow. Serve the row's own entry instead.
    if (giEntry != nullptr && (actual == RG_NONE || actual == randomizerGet)) {
        return giEntry;
    }
    return StaticData::RetrieveItem(actual).GetGIEntry();
}

GetItemEntry Item::GetGIEntry_Copy() const {
    return *GetGIEntry();
}

void Item::SetPrice(const uint16_t price_) {
    price = price_;
}

void Item::SetAsPlaythrough() {
    playthrough = true;
}

void Item::SetCustomDrawFunc(const CustomDrawFunc drawFunc) const {
    giEntry->drawFunc = drawFunc;
}

bool Item::IsPlaythrough() const {
    return playthrough;
}

bool Item::IsBottleItem() const {
    return getItemId == 0x0F ||                      // Empty Bottle
           getItemId == 0X14 ||                      // Bottle with Milk
           (getItemId >= 0x8C && getItemId <= 0x94); // Rest of bottled contents
}

bool Item::IsMajorItem() const {
    const auto ctx = Context::GetInstance();
    if (type == ITEMTYPE_TOKEN) {
        return ctx->GetOption(RSK_RAINBOW_BRIDGE).Is(RO_BRIDGE_TOKENS) ||
               ctx->GBKCondition() == RO_CHECK_TRIGGER_TOKENS;
    }

    if (type == ITEMTYPE_DROP || type == ITEMTYPE_EVENT || type == ITEMTYPE_SHOP || type == ITEMTYPE_MAP ||
        type == ITEMTYPE_COMPASS) {
        return false;
    }

    if (type == ITEMTYPE_DUNGEONREWARD &&
        ctx->GetOption(RSK_SHUFFLE_DUNGEON_REWARDS).Is(RO_DUNGEON_REWARDS_END_OF_DUNGEON)) {
        return false;
    }

    if ((randomizerGet == RG_BOMBCHU_5 || randomizerGet == RG_BOMBCHU_10 || randomizerGet == RG_BOMBCHU_20) &&
        ctx->GetOption(RSK_BOMBCHU_BAG).Is(RO_BOMBCHU_BAG_NONE)) {
        return false;
    }

    if (randomizerGet == RG_HEART_CONTAINER || randomizerGet == RG_PIECE_OF_HEART ||
        randomizerGet == RG_TREASURE_GAME_HEART) {
        return false;
    }

    if (type == ITEMTYPE_SMALLKEY && (ctx->GetOption(RSK_KEYSANITY).Is(RO_DUNGEON_ITEM_LOC_VANILLA) ||
                                      ctx->GetOption(RSK_KEYSANITY).Is(RO_DUNGEON_ITEM_LOC_OWN_DUNGEON))) {
        return false;
    }

    if (type == ITEMTYPE_FORTRESS_SMALLKEY && ctx->GetOption(RSK_GERUDO_KEYS).Is(RO_GERUDO_KEYS_VANILLA)) {
        return false;
    }

    if (type == ITEMTYPE_BOSSKEY && getItemId != 0xAD &&
        (ctx->GetOption(RSK_BOSS_KEYSANITY).Is(RO_DUNGEON_ITEM_LOC_VANILLA) ||
         ctx->GetOption(RSK_BOSS_KEYSANITY).Is(RO_DUNGEON_ITEM_LOC_OWN_DUNGEON))) {
        return false;
    }
    // Ganons Castle Boss Key
    if (getItemId == 0xAD && (ctx->GetOption(RSK_GANONS_BOSS_KEY).Is(RO_GANON_BOSS_KEY_VANILLA) ||
                              ctx->GetOption(RSK_GANONS_BOSS_KEY).Is(RO_GANON_BOSS_KEY_OWN_DUNGEON))) {
        return false;
    }

    if (randomizerGet == RG_GREG_RUPEE) {
        return ctx->GetOption(RSK_RAINBOW_BRIDGE).Is(RO_BRIDGE_GREG);
    }

    return IsAdvancement();
}

bool Item::IsShieldOrTunic() const {
    switch (randomizerGet) {
        case RG_DEKU_SHIELD:
        case RG_HYLIAN_SHIELD:
        case RG_MIRROR_SHIELD:
        case RG_GORON_TUNIC:
        case RG_ZORA_TUNIC:
        case RG_BUY_DEKU_SHIELD:
        case RG_BUY_HYLIAN_SHIELD:
        case RG_BUY_GORON_TUNIC:
        case RG_BUY_ZORA_TUNIC:
            return true;
        default:
            return false;
    }
}

RandomizerHintTextKey Item::GetHintKey() const {
    return hintKey;
}

const HintText& Item::GetHint() const {
    return StaticData::hintTextTable[hintKey];
}

GetItemCategory Item::GetCategory() {
    return category;
}

Item Item::CustomIcon(const char* customIcon_, CustomIconSize iconSize_) {
    customIcon = customIcon_;
    iconSize = iconSize_;
    return *this;
}

const char* Item::GetCustomIcon() {
    return customIcon;
}

CustomIconSize Item::GetCustomIconSize() {
    return iconSize;
}

bool Item::HasCustomIcon() {
    return customIcon != nullptr;
}

bool Item::operator==(const Item& right) const {
    return type == right.GetItemType() && getItemId == right.GetItemID();
}

bool Item::operator!=(const Item& right) const {
    return !operator==(right);
}
} // namespace Rando
