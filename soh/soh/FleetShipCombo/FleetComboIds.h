// FleetComboIds.h — CANONICAL cross-game ids for the Fleet Ship Combo (OoT <-> MM).
//
// This file MUST stay byte-identical in both repos:
//   Shipwright:      soh/soh/FleetShipCombo/FleetComboIds.h
//   2ship2harkinian: mm/2s2h/FleetShipCombo/FleetComboIds.h
//
// Two things live here:
//  1. The UNIVERSAL OBTAINED REGISTRY index space (FC_*): one u8 per entry in
//     NeiSaveData.comboObtained[FC_COMBO_OBTAINED_SIZE]. Entries are VALUES, not bits — a flag
//     stores 0/1, a counter stores its raw count (fits u8). This is the "info-only relative":
//     everything obtainable in either game has a slot here so the combo rando can always route
//     an item to the other game even when no native storage exists there (e.g. an MM enemy soul
//     granted while playing OoT). Where native storage DOES exist in a game, FleetSync bridges
//     registry <-> native on departure/arrival (e.g. MM mirrors its randoInf soul bits).
//  2. The bottle-content item-id translation table between the two games' ItemID spaces.
//
// NEVER reorder or renumber existing FC_ entries — they are persisted in save files.

#ifndef FLEET_COMBO_IDS_H
#define FLEET_COMBO_IDS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FC_COMBO_OBTAINED_SIZE 128
#define FC_REGISTRY_VERSION 1

// FCI-space store (SEPARATE from the FC_* registry above): NeiSaveData.comboObtainedFc[] is indexed by
// FcComboItemId (the FleetComboItems.h X-macro item table, ~323 rows today), one u8 COUNT per fcId. It
// lets the combo rando carry ANY cross item between games generically: on obtain, record the fcId; on
// arrival, grant the native deficit. Fixed & generous so future FC rows never force a save migration
// (append-only safe; glue static_asserts FCI_MAX <= this). Do NOT confuse with FC_COMBO_OBTAINED_SIZE.
#define FC_COMBO_OBTAINED_FC_SIZE 512

typedef enum {
    // --- MM enemy/boss SOULS (0..53) -------------------------------------------------------
    // SAME ORDER as MM's contiguous randoInf block RANDO_INF_OBTAINED_SOUL_OF_BOSS_GOHT ..
    // RANDO_INF_OBTAINED_SOUL_OF_ENEMY_WOLFOS (mm/2s2h/Rando/Types.h), so the MM bridge is
    // plain index math: randoInf = RANDO_INF_OBTAINED_SOUL_OF_BOSS_GOHT + (fc - FC_SOUL_FIRST).
    FC_SOUL_BOSS_GOHT = 0,
    FC_SOUL_BOSS_GYORG,
    FC_SOUL_BOSS_MAJORA,
    FC_SOUL_BOSS_ODOLWA,
    FC_SOUL_BOSS_TWINMOLD,
    FC_SOUL_ENEMY_ALIENS,
    FC_SOUL_ENEMY_ARMOS,
    FC_SOUL_ENEMY_BAD_BATS,
    FC_SOUL_ENEMY_BEAMOS,
    FC_SOUL_ENEMY_BOES,
    FC_SOUL_ENEMY_BUBBLES,
    FC_SOUL_ENEMY_CAPTAIN_KEETA,
    FC_SOUL_ENEMY_CHUCHUS,
    FC_SOUL_ENEMY_DEATH_ARMOS,
    FC_SOUL_ENEMY_DEEP_PYTHONS,
    FC_SOUL_ENEMY_DEKU_BABAS,
    FC_SOUL_ENEMY_DEXIHANDS,
    FC_SOUL_ENEMY_DINOLFOS,
    FC_SOUL_ENEMY_DODONGOS,
    FC_SOUL_ENEMY_DRAGONFLIES,
    FC_SOUL_ENEMY_EENOS,
    FC_SOUL_ENEMY_EYEGORES,
    FC_SOUL_ENEMY_FREEZARDS,
    FC_SOUL_ENEMY_GAROS,
    FC_SOUL_ENEMY_GEKKOS,
    FC_SOUL_ENEMY_GIANT_BEES,
    FC_SOUL_ENEMY_GOMESS,
    FC_SOUL_ENEMY_GUAYS,
    FC_SOUL_ENEMY_HIPLOOPS,
    FC_SOUL_ENEMY_IGOS_DU_IKANA,
    FC_SOUL_ENEMY_IRON_KNUCKLES,
    FC_SOUL_ENEMY_KEESE,
    FC_SOUL_ENEMY_LEEVERS,
    FC_SOUL_ENEMY_LIKE_LIKES,
    FC_SOUL_ENEMY_MAD_SCRUBS,
    FC_SOUL_ENEMY_NEJIRONS,
    FC_SOUL_ENEMY_OCTOROKS,
    FC_SOUL_ENEMY_ODOLWA,
    FC_SOUL_ENEMY_PEAHATS,
    FC_SOUL_ENEMY_PIRATES,
    FC_SOUL_ENEMY_POES,
    FC_SOUL_ENEMY_REDEADS,
    FC_SOUL_ENEMY_SHELLBLADES,
    FC_SOUL_ENEMY_SKULLFISH,
    FC_SOUL_ENEMY_SKULLTULAS,
    FC_SOUL_ENEMY_SNAPPERS,
    FC_SOUL_ENEMY_STALCHILDREN,
    FC_SOUL_ENEMY_TAKKURI,
    FC_SOUL_ENEMY_TEKTITES,
    FC_SOUL_ENEMY_WALLMASTERS,
    FC_SOUL_ENEMY_WARTS,
    FC_SOUL_ENEMY_WIZROBES,
    FC_SOUL_ENEMY_WOLFOS, // = 52
    FC_SOUL_FIRST = FC_SOUL_BOSS_GOHT,
    FC_SOUL_LAST = FC_SOUL_ENEMY_WOLFOS,

    // --- Movement / capability abilities (56..63) ------------------------------------------
    FC_ABILITY_SWIM = 56, // bridge: MM RANDO_INF_OBTAINED_SWIM
    FC_ABILITY_CRAWL,     // reserved (no ownable crawl exists yet)
    FC_ABILITY_RESERVED_2,
    FC_ABILITY_RESERVED_3,
    FC_ABILITY_RESERVED_4,
    FC_ABILITY_RESERVED_5,
    FC_ABILITY_RESERVED_6,
    FC_ABILITY_RESERVED_7, // = 63

    // --- Ownership without cross-game native storage (64..) --------------------------------
    // OoT base swords in MM (upgrade bits travel in weaponUpgrades; BASE ownership needs a slot):
    FC_OOT_SWORD_MASTER = 64,
    FC_OOT_SWORD_BIGGORON,
    // OoT child trade chain (info-only in MM):
    FC_OOT_TRADE_WEIRD_EGG,
    FC_OOT_TRADE_CHICKEN,
    FC_OOT_TRADE_ZELDAS_LETTER,
    FC_OOT_TRADE_POCKET_EGG,
    FC_OOT_TRADE_POCKET_CUCCO,
    FC_OOT_TRADE_COJIRO,
    FC_OOT_TRADE_ODD_MUSHROOM,
    FC_OOT_TRADE_ODD_POTION,
    FC_OOT_TRADE_SAW,
    FC_OOT_TRADE_BROKEN_SWORD,
    FC_OOT_TRADE_PRESCRIPTION,
    FC_OOT_TRADE_FROG,
    FC_OOT_TRADE_EYEDROPS,
    FC_OOT_TRADE_CLAIM_CHECK,
    // OoT tunics/boots (info-only in MM):
    FC_OOT_TUNIC_GORON = 80,
    FC_OOT_TUNIC_ZORA,
    FC_OOT_BOOTS_IRON,
    FC_OOT_BOOTS_HOVER,
    // Fishing rod (OoT Fishing Pole <-> MM Fishing Rod — neither has inventory storage):
    FC_FISHING_ROD = 84,
    // MM Tingle Maps (info-only in OoT):
    FC_MM_TINGLE_MAPS = 85,
    // MM world-progress COUNTERS mirrored for the rando (u8 raw counts, not flags):
    FC_MM_SKULLS_SWAMP = 88,
    FC_MM_SKULLS_OCEAN,
    FC_MM_FAIRIES_WOODFALL,
    FC_MM_FAIRIES_SNOWHEAD,
    FC_MM_FAIRIES_GREAT_BAY,
    FC_MM_FAIRIES_STONE_TOWER,
    // OoT Progressive Strength (Goron Bracelet -> Silver -> Gold Gauntlets). Info-only in MM, like
    // the tunics/boots above: the cell records the level so the ext-equipment kaleido and the combo
    // sync can see it. Appended at the first free index, so no existing cell moves. Skijer's NEI
    FC_OOT_STRENGTH = 94,
    // 95..127 free for future assignments (NEVER renumber the above).
    FC_MAX = FC_COMBO_OBTAINED_SIZE
} FleetComboId;

// --- mmQuestItems (OoT-side NeiSaveData field) bit layout -----------------------------------
// Mirror of MM's nei.ootQuestItems pattern: OoT stores MM quest ownership here. Bits chosen to
// match MM's native QuestItem indices where one exists (remains 0-3, songs 6-17) so the sync is
// a masked copy of MM's inventory.questItems.
// --- Combo goal state (Beat Both Bosses) ---------------------------------------------------
// Shared because the goal is genuinely cross-game: neither world may roll credits until BOTH
// bosses are down. Whoever wins first records its bit, saves, and is sent back out to keep
// playing; the second one to fall triggers the real ending. Synced like every other combo field.
#define FC_GOAL_GANON_BEATEN (1 << 0)
#define FC_GOAL_MAJORA_BEATEN (1 << 1)

#define FC_MMQ_REMAINS_ODOLWA (1 << 0)
#define FC_MMQ_REMAINS_GOHT (1 << 1)
#define FC_MMQ_REMAINS_GYORG (1 << 2)
#define FC_MMQ_REMAINS_TWINMOLD (1 << 3)
#define FC_MMQ_SONG_SONATA (1 << 6)
#define FC_MMQ_SONG_GORON_LULLABY (1 << 7)
#define FC_MMQ_SONG_NEW_WAVE (1 << 8)
#define FC_MMQ_SONG_ELEGY (1 << 9)
#define FC_MMQ_SONG_OATH (1 << 10)
#define FC_MMQ_SONG_SARIA (1 << 11) // shared with OoT questItems (kept for display parity)
#define FC_MMQ_SONG_TIME (1 << 12)  // shared
#define FC_MMQ_SONG_HEALING (1 << 13)
#define FC_MMQ_SONG_EPONA (1 << 14) // shared
#define FC_MMQ_SONG_SOARING (1 << 15)
#define FC_MMQ_SONG_STORMS (1 << 16)        // shared
#define FC_MMQ_SONG_SUN (1 << 17)           // shared
#define FC_MMQ_BOMBERS_NOTEBOOK (1 << 18)   // matches MM native QUEST_BOMBERS_NOTEBOOK (0x12)
#define FC_MMQ_SONG_TIME_INVERTED (1 << 19) // playing-variant knowledge flags (info-only)
#define FC_MMQ_SONG_TIME_DOUBLE (1 << 20)
// Mask of the bits that come 1:1 from MM's native inventory.questItems (remains 0-3, songs 6-17,
// Bombers' Notebook 18):
#define FC_MMQ_NATIVE_MASK 0x0007FFCF

// --- shieldOwned (NeiSaveData field, BOTH repos) bit layout ---------------------------------
#define FC_SHIELD_DEKU (1 << 0)       // OoT native only
#define FC_SHIELD_HYLIAN (1 << 1)     // OoT Hylian == MM Hero
#define FC_SHIELD_MIRROR_OOT (1 << 2) // OoT Mirror (NOT the MM one)
#define FC_SHIELD_DIVINE (1 << 3)     // NEI ext shield 1 (both repos)
#define FC_SHIELD_KITE (1 << 4)       // NEI ext shield 2 (both repos)
#define FC_SHIELD_IKANA (1 << 5)      // NEI ext shield 3 == MM Mirror Shield
#define FC_SHIELD_NEW_1 (1 << 6)      // 4 reserved future shields (flags ready, items later)
#define FC_SHIELD_NEW_2 (1 << 7)
#define FC_SHIELD_NEW_3 (1 << 8)
#define FC_SHIELD_NEW_4 (1 << 9)

// --- MM masks: canonical slot order + per-game ids -------------------------------------------
// Canonical order = MM's inventory MASK SLOT order (SLOT_MASK_* 0x18..0x2F) which is ALSO the
// order of OoT's page-3 mask items (ITEM_MM_MASK_POSTMAN..FIERCE_DEITY, contiguous from
// FC_OOT_MM_MASK_ITEM_BASE) and of OoT's NeiSaveData.ownedItems[24..47].
#define FC_MM_MASK_COUNT 24
#define FC_OOT_MM_MASK_ITEM_BASE 0xB8 // OoT ITEM_MM_MASK_POSTMAN; +i follows slot order
#define FC_MM_MASK_SLOT_BASE 0x18     // MM SLOT_MASK_POSTMAN; +i
// MM native mask ITEM id stored at inventory.items[0x18 + i]:
static const uint8_t kFcMmMaskItemBySlot[FC_MM_MASK_COUNT] = {
    0x3E, // POSTMAN
    0x38, // ALL_NIGHT
    0x47, // BLAST
    0x45, // STONE
    0x40, // GREAT_FAIRY
    0x32, // DEKU
    0x3A, // KEATON
    0x46, // BREMEN
    0x39, // BUNNY
    0x42, // DON_GERO
    0x48, // SCENTS
    0x33, // GORON
    0x3C, // ROMANI
    0x3D, // CIRCUS_LEADER
    0x37, // KAFEIS_MASK
    0x3F, // COUPLE
    0x36, // TRUTH
    0x34, // ZORA
    0x43, // KAMARO
    0x41, // GIBDO
    0x3B, // GARO
    0x44, // CAPTAIN
    0x49, // GIANT
    0x35, // FIERCE_DEITY
};

// --- Bottle-content ItemID translation (OoT id <-> MM id) -----------------------------------
// Used to translate NeiSaveData.bottleSlots[8] entries (and bottomlessContent) between the two
// games' item-id spaces. Ids verified against soh/include/z64item.h and mm/include/z64item.h.
typedef struct {
    uint8_t ootId;
    uint8_t mmId;
} FcBottleContentPair;

static const FcBottleContentPair kFcBottleContentMap[] = {
    { 0x14, 0x12 }, // ITEM_BOTTLE (empty)          <-> ITEM_BOTTLE
    { 0x15, 0x13 }, // ITEM_POTION_RED               <-> ITEM_POTION_RED
    { 0x16, 0x14 }, // ITEM_POTION_GREEN             <-> ITEM_POTION_GREEN
    { 0x17, 0x15 }, // ITEM_POTION_BLUE              <-> ITEM_POTION_BLUE
    { 0x18, 0x16 }, // ITEM_FAIRY                    <-> ITEM_FAIRY
    { 0x19, 0x1A }, // ITEM_FISH                     <-> ITEM_FISH
    { 0x1A, 0x18 }, // ITEM_MILK_BOTTLE              <-> ITEM_MILK_BOTTLE
    { 0x1F, 0x19 }, // ITEM_MILK_HALF                <-> ITEM_MILK_HALF
    { 0x1C, 0x1C }, // ITEM_BLUE_FIRE                <-> ITEM_BLUE_FIRE
    { 0x1D, 0x1B }, // ITEM_BUG                      <-> ITEM_BUG
    { 0x20, 0x1D }, // ITEM_POE                      <-> ITEM_POE
    { 0x1E, 0x1E }, // ITEM_BIG_POE                  <-> ITEM_BIG_POE
    { 0xB6, 0x25 }, // ITEM_CHATEAU_ROMANI           <-> ITEM_CHATEAU
    { 0xEC, 0x22 }, // ITEM_GOLD_DUST                <-> ITEM_GOLD_DUST
    { 0xED, 0x20 }, // ITEM_HOT_SPRING_WATER         <-> ITEM_HOT_SPRING_WATER
    { 0xEE, 0x17 }, // ITEM_DEKU_PRINCESS            <-> ITEM_DEKU_PRINCESS
    { 0xEF, 0x24 }, // ITEM_SEAHORSE                 <-> ITEM_SEAHORSE
    { 0xF0, 0x1F }, // ITEM_SPRING_WATER             <-> ITEM_SPRING_WATER
    { 0xF1, 0x21 }, // ITEM_ZORA_EGG                 <-> ITEM_ZORA_EGG
    { 0xF2, 0x26 }, // ITEM_HYLIAN_LOACH             <-> ITEM_HYLIAN_LOACH
    { 0xF3, 0x27 }, // ITEM_OBABA_DRINK              <-> ITEM_OBABA_DRINK
    { 0xDD, 0x23 }, // ITEM_MAGIC_MUSHROOM           <-> ITEM_MUSHROOM
    { 0x1B, 0xFE }, // ITEM_LETTER_RUTO              <-> (no MM relative: sentinel 0xFE, kept OoT-side)
};
#define FC_BOTTLE_CONTENT_MAP_COUNT (sizeof(kFcBottleContentMap) / sizeof(kFcBottleContentMap[0]))
#define FC_BOTTLE_SLOT_EMPTY 0xFF // NeiSaveData.bottleSlots "no bottle in this slot"
#define FC_BOTTLE_UNMAPPED \
    0xFE // translation result for a content with no relative: the
         // applier must replace it with the LOCAL empty-bottle id
         // (never store 0xFE — big ids crash icon/digit lookups)

static inline uint8_t FcBottle_OotToMm(uint8_t ootId) {
    unsigned int i;
    if (ootId == FC_BOTTLE_SLOT_EMPTY) {
        return FC_BOTTLE_SLOT_EMPTY;
    }
    for (i = 0; i < FC_BOTTLE_CONTENT_MAP_COUNT; i++) {
        if (kFcBottleContentMap[i].ootId == ootId) {
            return kFcBottleContentMap[i].mmId;
        }
    }
    return FC_BOTTLE_UNMAPPED; // unknown content: NEVER pass a foreign id through
}

static inline uint8_t FcBottle_MmToOot(uint8_t mmId) {
    unsigned int i;
    if (mmId == FC_BOTTLE_SLOT_EMPTY) {
        return FC_BOTTLE_SLOT_EMPTY;
    }
    for (i = 0; i < FC_BOTTLE_CONTENT_MAP_COUNT; i++) {
        if (kFcBottleContentMap[i].mmId == mmId) {
            return kFcBottleContentMap[i].ootId;
        }
    }
    return FC_BOTTLE_UNMAPPED; // unknown content: NEVER pass a foreign id through
}

// --- General ItemID translation for BUTTON EQUIPS (OoT id <-> MM id) -------------------------
// Used to carry C-button / D-pad equips across; an unmappable id returns 0xFF and the caller
// keeps whatever the destination game had on that button.
static const FcBottleContentPair kFcItemPairMap[] = {
    { 0x00, 0x08 }, // Deku Stick
    { 0x01, 0x09 }, // Deku Nut
    { 0x02, 0x06 }, // Bombs
    { 0x03, 0x01 }, // Bow
    { 0x04, 0x02 }, // Fire Arrow
    { 0x06, 0x0B }, // Slingshot
    { 0x07, 0x05 }, // Fairy Ocarina
    { 0x08, 0x00 }, // Ocarina of Time
    { 0x09, 0x07 }, // Bombchu
    { 0x0A, 0x0F }, // Hookshot
    { 0x0B, 0x11 }, // Longshot (MM keeps the OoT-leftover ITEM_LONGSHOT)
    { 0x0C, 0x03 }, // Ice Arrow
    { 0x0E, 0xEE }, // Boomerang (MM sentinel)
    { 0x0F, 0x0E }, // Lens of Truth
    { 0x10, 0x0A }, // Magic Beans
    { 0x11, 0xED }, // Megaton Hammer (MM sentinel)
    { 0x12, 0x04 }, // Light Arrow
    { 0x38, 0x4A }, // Bow + Fire Arrow
    { 0x39, 0x4B }, // Bow + Ice Arrow
    { 0x3A, 0x4C }, // Bow + Light Arrow
    { 0xF4, 0xF7 }, // Net
    { 0xF5, 0xF8 }, // Bottomless Bottle
    { 0xF6, 0x0C }, // Powder Keg
    { 0xDD, 0xDC }, // Magic Mushroom
    // Elemental Wand — the ONE page-2 custom whose id is the SAME on both sides (0xD0), so it falls
    // outside the +0x18 block below and used to translate to 0xFF (unmappable) in BOTH directions:
    // obtained in either game, it simply never showed up in the other. Skijer's NEI
    { 0xD0, 0xD0 }, // Elemental Wand
};
#define FC_ITEM_PAIR_MAP_COUNT (sizeof(kFcItemPairMap) / sizeof(kFcItemPairMap[0]))
// NEI page-2 custom items sit in a contiguous 26-id block on both sides at a fixed offset:
#define FC_OOT_PAGE2_FIRST 0x9E
#define FC_OOT_PAGE2_LAST 0xB7
#define FC_PAGE2_MM_OFFSET 0x18 // MM id = OoT id + 0x18 (0xB6..0xCF)

static inline uint8_t FcEquip_OotToMm(uint8_t ootId) {
    unsigned int i;
    if (ootId == 0xFF) {
        return 0xFF;
    }
    for (i = 0; i < FC_ITEM_PAIR_MAP_COUNT; i++) {
        if (kFcItemPairMap[i].ootId == ootId) {
            return kFcItemPairMap[i].mmId;
        }
    }
    if (ootId >= FC_OOT_PAGE2_FIRST && ootId <= FC_OOT_PAGE2_LAST) {
        return (uint8_t)(ootId + FC_PAGE2_MM_OFFSET);
    }
    if (ootId >= FC_OOT_MM_MASK_ITEM_BASE && ootId < FC_OOT_MM_MASK_ITEM_BASE + FC_MM_MASK_COUNT) {
        return kFcMmMaskItemBySlot[ootId - FC_OOT_MM_MASK_ITEM_BASE];
    }
    for (i = 0; i < FC_BOTTLE_CONTENT_MAP_COUNT; i++) { // bottled content equipped on a button
        if (kFcBottleContentMap[i].ootId == ootId) {
            return kFcBottleContentMap[i].mmId;
        }
    }
    return 0xFF; // unmappable
}

static inline uint8_t FcEquip_MmToOot(uint8_t mmId) {
    unsigned int i;
    if (mmId == 0xFF) {
        return 0xFF;
    }
    for (i = 0; i < FC_ITEM_PAIR_MAP_COUNT; i++) {
        if (kFcItemPairMap[i].mmId == mmId) {
            return kFcItemPairMap[i].ootId;
        }
    }
    if (mmId >= FC_OOT_PAGE2_FIRST + FC_PAGE2_MM_OFFSET && mmId <= FC_OOT_PAGE2_LAST + FC_PAGE2_MM_OFFSET) {
        return (uint8_t)(mmId - FC_PAGE2_MM_OFFSET);
    }
    for (i = 0; i < FC_MM_MASK_COUNT; i++) {
        if (kFcMmMaskItemBySlot[i] == mmId) {
            return (uint8_t)(FC_OOT_MM_MASK_ITEM_BASE + i);
        }
    }
    for (i = 0; i < FC_BOTTLE_CONTENT_MAP_COUNT; i++) {
        if (kFcBottleContentMap[i].mmId == mmId) {
            return kFcBottleContentMap[i].ootId;
        }
    }
    return 0xFF; // unmappable
}

#ifdef __cplusplus
}
#endif

#endif // FLEET_COMBO_IDS_H
