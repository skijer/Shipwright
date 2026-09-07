// Skijer's NEI
#ifndef NEI_SAVE_H
#define NEI_SAVE_H

#include <stdint.h>
#include "soh/FleetShipCombo/FleetComboIds.h" // FC_COMBO_OBTAINED_FC_SIZE (fcId-indexed store size)

#ifdef __cplusplus
extern "C" {
#endif

// ── SW97 primed element (Skijer's NEI) ───────────────────────────────────────────────────────────
// The bow's / slingshot's elemental shot used to BE the item id sitting on the C-button (six
// ITEM_SW97_ARROW_* ids). That burned six inventory ids to express three bits, and in 2ship the
// twin bullet ids collided with ITEM_MAP_POINT_*. The element is now a flag (Gust Jar pattern):
// the button always holds the plain weapon and the medallion is composited over the icon.
//
// The 1..6 ordering is load-bearing: it keeps every existing offset expression a one-liner
// (`ARROW_SW97_FIRE + (e - SW97_ELEM_FIRE)`, `ARROW_SEED_FIRE + (e - SW97_ELEM_FIRE)`), and it is
// bit-identical to 2ship's legacy `slingshotWheel` index so that save migrates by plain copy.
#define SW97_ELEM_NONE 0  // plain bow / plain slingshot
#define SW97_ELEM_FIRE 1  // Fire Medallion
#define SW97_ELEM_ICE 2   // Water Medallion
#define SW97_ELEM_LIGHT 3 // Light Medallion
#define SW97_ELEM_DARK 4  // Shadow Medallion
#define SW97_ELEM_SOUL 5  // Spirit Medallion
#define SW97_ELEM_WIND 6  // Forest Medallion
#define SW97_ELEM_BOMB 7  // Bomb Arrows — BOW ONLY (never rides the slingshot flag)
#define SW97_ELEM_COUNT 8

// Elemental Wand modes (Skijer's NEI) — six rods in ONE page-2 cell, same wheel idiom. Index order
// IS the wheel order; the medallion column is what the wheel draws behind the rod icon.
//   0 Spirit -> Sand Rod      1 Forest -> Tornado Rod   2 Water  -> Water Rod
//   3 Fire   -> Meteor Rod    4 Light  -> Storm Rod     5 Shadow -> Shadow Scepter
#define WAND_MODE_SAND 0
#define WAND_MODE_TORNADO 1
#define WAND_MODE_WATER 2
#define WAND_MODE_METEOR 3
#define WAND_MODE_STORM 4
#define WAND_MODE_SCEPTER 5
#define WAND_MODE_COUNT 6

// Randomizer treatment of the wand (all three share the SAME slot flag).
#define WAND_RANDO_MEDALLIONS 0 // 1 pool item; mode N usable iff you own medallion N
#define WAND_RANDO_SINGLE 1     // 1 pool item; obtaining it lights all six modes
#define WAND_RANDO_ELEMENTAL 2  // 6 pool items; each lights its own mode

// Sheikah Slate runes (Skijer's NEI) — five runes in ONE page-2 cell, wand idiom: sibling
// obtainable items over one slot (each with its own textbox), gettable in any order, no levels.
// Index order IS the wheel order.
#define SLATE_RUNE_BOMB 0 // Remote Bomb
#define SLATE_RUNE_STASIS 1
#define SLATE_RUNE_CRYONIS 2
#define SLATE_RUNE_MASTER_CYCLE 3 // Master Cycle Zero
#define SLATE_RUNE_SENSOR 4       // Sheikah Sensor — the old Desire Sensor, rehoused as a rune
#define SLATE_RUNE_COUNT 5
// Future runes with art already staged in icon_item_custom: Magnesis, Camera
// (gItemIconSlateRuneMagnesisTex / gItemIconSlateRuneCameraTex).

// Rod of Seasons (Skijer's NEI) — four seasons in ONE page-2 cell, slate idiom: sibling obtainable
// items over one slot (each with its own textbox), gettable in any order, no levels.
// Index order IS the wheel order, and it is the natural year order so the wheel reads as a calendar.
#define SEASON_SPRING 0
#define SEASON_SUMMER 1
#define SEASON_AUTUMN 2
#define SEASON_WINTER 3
#define SEASON_COUNT 4 // real seasons: the seasonsOwned bit width, the palette, the coin art
// The blank coin: the rod stops driving the weather and the scene keeps its own. Never granted and
// never stored as owned — it is free, so it sits OUTSIDE the bitmask and past SEASON_COUNT, which
// is what keeps "do I own the rod at all" answerable by counting bits.
#define SEASON_OFF 4
#define SEASON_SLOTS 5 // wheel entries: the four seasons plus the blank coin

// Randomizer treatment of Bomb Arrows.
#define BOMB_ARROWS_RANDO_OFF 0      // never granted on their own (Twilight Upgrade still works)
#define BOMB_ARROWS_RANDO_BOMB_BAG 1 // auto-granted the moment any bomb bag is owned
#define BOMB_ARROWS_RANDO_SHUFFLED 2 // a real randomizer item

// Skijer's NEI: per-save state, serialized via the "nei" SaveManager section
// (NOT in the vanilla SaveContext, which is kept 100% upstream).
typedef struct NeiSaveData {
    // Custom inventory slots 24..71 (page-2 items + MM masks).
    // u16, NOT u8: mirrors the widening done on the MM side. The vanilla u8 item-id space is
    // exhausted there (5 free ids in all of 0x9C-0xFF), so page-2 items can now carry an EXT id
    // above 0xFF. Both games must agree on the width or FleetSync's ownedItems array desyncs.
    // ITEM_NONE stays 0xFF — the empty marker did NOT become 0xFFFF. Skijer's NEI
    uint16_t ownedItems[48];
    // 2026-08-06 page-2 re-layout (mirror of the MM fields; the MM kaleido wheel reads them there).
    // In soh they are set/serialized so the state survives and syncs later; soh's own shovel wheel
    // and broken-items gating are pending. Skijer's NEI
    uint8_t shovelOwned;
    uint8_t dominionOwned;
    uint8_t pokeballOwned;
    uint32_t extEquipOwnedBits; // ext-equipment ownership (was inventory.equipment high bits)
    uint8_t lanternFireType;
    uint8_t lanternCapturedTypes;
    uint8_t twilightUpgrade;
    uint8_t ultrashotOwned; // Skijer's NEI hookshot overhaul: when owned, the Longshot becomes the
                            // Ultrashot (4x hookshot reach, 2x speed; Longshot icon + Light-medallion
                            // corner marker + "Ultrashot" name)
    uint8_t clawshotModeActive;
    uint8_t galeBoomerangModeActive;
    uint8_t weaponUpgrades;
    uint8_t extEquipSword;
    uint8_t extEquipShield;
    uint8_t extEquipTunic;
    uint8_t extEquipBoots;
    // Bottle randomizer (Skijer's NEI). The bottle inventory is 8 slots shown in the save editor as a
    // 4x2 grid: "Bottle A" = slots 0-3, "Bottle B" = slots 4-7. Each holds an OoT ITEM_ content id,
    // ITEM_BOTTLE (empty bottle), or 0xFF (empty slot). This is the OoT-side "which content is in
    // which bottle" state; the kaleido Wheel A/B each cycle their 4 slots. (Cross-game sharing reads/
    // writes these on game switch — layered on later via FscShared.) Wheels A/B map to the vanilla
    // SLOT_BOTTLE_1/2; Net + Bottomless take SLOT_BOTTLE_3/4.
    uint8_t bottleSlots[8];       // 0xFF = empty; ITEM_BOTTLE = empty bottle; else a content id
    uint8_t bottomlessBottleMode; // Bottomless Bottle OWNED (SLOT_BOTTLE_4 item). Skijer's NEI
    uint8_t netEquipped;          // Net OWNED (SLOT_BOTTLE_3 item; behavior deferred)
    // Bottomless Bottle "ammo": SLOT_BOTTLE_4 holds a real bottle content, but instead of emptying in
    // one use it has a per-content use-counter. Each empty (drink/sell) decrements bottomlessCount;
    // while >0 the content auto-refills, at 0 it becomes an empty Bottomless Bottle. (Net has none.)
    uint8_t bottomlessContent; // content id in the Bottomless Bottle, or ITEM_BOTTLE/0xFF when empty
    uint8_t bottomlessCount;   // remaining uses of bottomlessContent (the counter shown on the icon)
    uint8_t powerKegOwned;     // Power Keg owned (granted via menu); shares the Bomb slot via a
                               // kaleido wheel, USE gated by form + strength (see power_keg.c)
    uint8_t powerKegCount;     // Power Keg "ammo": how many kegs the player carries (its own
                               // counter; each use consumes 1). Skijer's NEI
    uint8_t powerKegMode;      // keg mode selected on the Bomb slot (kaleido wheel toggle) — persists
                               // so the slot doesn't revert to bombs on reload. Skijer's NEI
    // MM adult trade-quest items (Skijer's NEI). Bitmask over a NEI trade index: 0-10 = the OoT items
    // (ITEM_POCKET_EGG..ITEM_CLAIM_CHECK), 11 = Moon's Tear, 12-15 = the four Title Deeds, 16 = Room Key,
    // 17 = Letter to Kafei, 18 = Special Delivery to Mama, 19 = Pendant of Memories. The 2D-grid wheel on
    // SLOT_TRADE_ADULT shows every owned entry. The Pendant's bit is set alongside its combat ownership
    // (extEquipOwnedBits, Ext Boots 2) — both flags on grant. See trade_items.c.
    uint32_t tradeAdultOwned;
    // Pictograph Box (Skijer's NEI). Stored in Majora's Mask's EXACT save layout so a 2Ship bridge
    // can consume it: pictoFlags0/1 are the 64 PICTO_VALID_* bits (set by Snap_SetFlag when a mapped
    // OoT actor is validly photographed), pictoPhotoI5 is the last photo compressed to I5 (160x112).
    // OoT itself gives no reward for these — they exist only to be read by MM/2Ship. See snap.h.
    uint8_t pictoboxOwned;       // Pictobox item owned (granted via CVar/menu)
    uint8_t pictoHasPhoto;       // a photo has been kept (gates the "Replace?" warn before capture)
    uint32_t pictoFlags0;        // MM pictoFlags0: PICTO_VALID_* bits 0x00..0x1F
    uint32_t pictoFlags1;        // MM pictoFlags1: PICTO_VALID_* bits 0x20..0x3F
    uint8_t pictoPhotoI5[11200]; // MM PICTO_PHOTO_COMPRESSED_SIZE = (160*112)*5/8 (I5, last photo)
    // --- Fleet Ship Combo (cross-game) fields — bit/index layouts in FleetShipCombo/FleetComboIds.h ---
    uint16_t shieldOwned;       // unified 10-shield ownership bitmask (FC_SHIELD_*): 3 OoT vanilla +
                                // 3 NEI ext (Divine/Kite/Ikana) + 4 reserved. Mirrored from the
                                // vanilla EQUIP_FLAG_SHIELD_* + extEquipOwnedBits on load.
    uint32_t mmQuestItems;      // MM quest ownership OoT-side (FC_MMQ_*: remains, MM songs,
                                // Bombers' Notebook) — mirror of MM's nei.ootQuestItems pattern
    uint8_t comboObtained[128]; // universal cross-game obtained registry (FC_* index; u8 VALUES:
                                // flags store 0/1, counters store raw counts). Info-only relatives
                                // for the combo rando (souls, abilities, trade chain, ...)
    uint16_t comboTriforce;     // shared Triforce-piece count (syncs vs triforcePiecesCollected/MM)
    uint8_t comboGoalFlags;     // FC_GOAL_*: which bosses are already down, across BOTH games.
                                // Beat Both Bosses ends only when both bits are set; the first
                                // win records its bit, saves, and returns you to play on.
    // Upgrade-column passives (Skijer 2026-07-15): Magic Cape + Pendant of Memories moved out of the
    // ext-equipment grid into the equipment page's upgrade column. Toggled with A on their cells
    // (transparent = off, solid = on — the spiritual-stones visual). Ownership stays in
    // extEquipOwnedBits (TUNIC 1 / BOOTS 2).
    uint8_t capeHidden;       // 1 = don't DRAW the Magic Cape on Link (its magic refund is
                              // ALWAYS active once owned; this only hides the cloth)
    uint8_t pendantEffectOff; // 1 = Pendant of Memories moveset disabled (effect toggle)
    // Magic Cape ownership moved OUT of the ext-equipment TUNIC-1 bit (Skijer 2026-07-16): the ext
    // tunic slot 1 is now a real recolor tunic (Champion's Tunic), so the Cape can't squat on that
    // bit anymore. Migrated from the old bit in ExtEquip_Init.
    uint8_t capeOwned;             // 1 = owns the Magic Cape (upgrade-column passive)
    uint8_t extTunicLayoutVersion; // 1 = Champion/Spirit/Sage's
    // --- Generic fcId-indexed cross-item sync (FleetComboItems.h FcComboItemId space) ------------
    // SEPARATE id space from comboObtained[128] (that is FC_* registry indices). This store is indexed
    // by FcComboItemId (the X-macro item table); each entry is a u8 COUNT. On obtaining ANY FC cross
    // item in OoT the count is bumped (synced to MM via a MAX-merge); on arrival OoT grants the native
    // deficit for fcIds obtained in the other game. comboAppliedFc is LOCAL bookkeeping (NOT synced):
    // how many copies of each fcId have already been materialized into OoT's native inventory.
    uint8_t comboObtainedFc[FC_COMBO_OBTAINED_FC_SIZE]; // fcId-indexed cross store (counts); synced
    uint8_t comboAppliedFc[FC_COMBO_OBTAINED_FC_SIZE];  // local: copies already materialized here (NOT synced)
    // Dual Cane (Somaria / Pacci) — Skijer's NEI. Six SEPARATE obtainable items share one
    // kaleido slot; each lights its own bit here and they may be obtained in any order.
    // Appended at the END so older blobs stay readable.
    uint8_t caneSkills;      // bitmask, CANE_SKILL_BIT(CANE_SKILL_*) — 0 = cane not owned at all
    uint8_t caneType;        // active cane: CANE_TYPE_SOMARIA / CANE_TYPE_PACCI
    uint8_t caneSkillSel[2]; // per-cane selected skill SLOT (0..2); index by caneType
    // Quartz of Motion (level 2 of the progressive Stone of Agony). The tracking
    // category is chosen by pressing A on the Stone of Agony quest slot in the
    // kaleido; activating spends one heart container and runs the sensor for 5
    // minutes. Only the SELECTION persists — the countdown is session state.
    // Appended at the END so older blobs stay readable.
    uint8_t quartzOwned;    // 1 = Quartz of Motion obtained (2nd Stone of Agony copy)
    uint8_t quartzCategory; // DesireCompassCategory last selected in the kaleido
    uint8_t quartzSubcat;   // subcategory within that category (0 = any)
    // Pendant of Memories ownership moved OUT of the ext-equipment BOOTS-2 bit (Skijer 2026-07-29,
    // same move the Magic Cape made off TUNIC-1): the ext boots slots 2/3 are real boots now
    // (Climb Boots / Roc Boots), so the Pendant can't squat on that bit. Migrated from the old bit
    // in ExtEquip_Init, gated by extBootsLayoutVersion. Appended at the END.
    uint8_t pendantOwned;          // 1 = owns the Pendant of Memories (left-column passive)
    uint8_t extBootsLayoutVersion; // 1 = Pegasus / Climb / Roc
    // SW97 elemental shot + Bomb Arrows + Elemental Wand (Skijer's NEI). The bow and the slingshot
    // carry INDEPENDENT elements on purpose — you may prime spirit arrows and wind bullets at once.
    // Appended at the END so older blobs stay readable.
    uint8_t sw97BowElement;    // SW97_ELEM_*
    uint8_t sw97SlingElement;  // SW97_ELEM_* — never SW97_ELEM_BOMB (bombs are bow-only)
    uint8_t bombArrowsOwned;   // replaces the old page-2 SLOT_BOMB_ARROWS cell
    uint8_t wandMode;          // WAND_MODE_* — the rod the page-2 cell is showing
    uint8_t wandRodsOwned;     // WAND_MODE_* bitmask (six bits)
    uint8_t sw97LayoutVersion; // 1 = migrated off the per-element item ids
    // OoT child-trade MASKS (Keaton .. Mask of Truth) as a bitmask, bit N = item id 0x24 + N — the
    // same order as MM's sOotMaskIconPaths, since MM has no item ids for them and shows them on one
    // kaleido cell driven by this mask (nei->ootMasksOwned there, wheel position in ootMaskCursor).
    // OoT authors it from SLOT_TRADE_CHILD in FleetSync's FoldNativesIntoRegistry (2026-08-07,
    // before that it was echo-only on this side); both games MAX-merge it through the shared store.
    // Appended at the END so older blobs stay readable. Skijer's NEI
    uint16_t ootMasksOwned;
    // Sheikah Slate — Skijer's NEI. Four runes (Remote Bomb / Stasis / Cryonis / Master Cycle) share
    // the one SLOT_SHEIKAH_SLATE cell; each pickup grants a RANDOM unowned rune (like the wands,
    // no levels). Appended at the END so older blobs stay readable.
    uint8_t slateMode;       // SLATE_RUNE_* — the rune the cell is showing / the button casts
    uint8_t slateRunesOwned; // SLATE_RUNE_* bitmask (four bits) — 0 = slate not owned at all
    // Trirod echoes (Somaria L3 = Echoes of Wisdom's Tri Rod) — Skijer's NEI. The learned mask is
    // bit-per-row over expansions/trirod/trirod_echoes.inc.c, split across two u32 because the
    // save layer has no u64 path; trirodSel is the row the C button summons. That table is
    // APPEND-ONLY for exactly this reason. Appended at the END so older blobs stay readable.
    uint32_t trirodEchoesLo; // learned echoes, rows 0..31
    uint32_t trirodEchoesHi; // learned echoes, rows 32..63
    uint8_t trirodSel;       // selected row (normalised to a learned one on use)
    // Rito Mask — Skijer's NEI. The mask shares the Farore's Wind CELL, so the cell can
    // only ever show one of the two and cannot answer "does this save own the other?".
    // That is what this byte remembers. Appended at the END so older blobs stay readable.
    //   bit0 RITO_FLAG_MASK_OWNED    — the Rito Mask has been granted to this file
    //   bit1 RITO_FLAG_FARORES_OWNED — the spell was in the cell when the mask took it
    //                                  over, so cycling back may hand it out again
    uint8_t ritoMaskFlags;
    // Trirod v2 (2026-08-11): the echo table was REORDERED (rows deleted/merged), so
    // bits from a v1 save mean different echoes — anything below TRIROD_LAYOUT_VERSION
    // gets its mask cleared on load (extBootsLayoutVersion pattern). trirodFullList
    // picks between the COMPRESSED list (one echo per distinct effect) and the FULL
    // one (flavour duplicates). Appended at the END so older blobs stay readable.
    uint8_t trirodLayoutVersion;
    uint8_t trirodFullList; // 0 = compressed (default), 1 = full
    // Rod of Seasons — Skijer's NEI. Four seasons share the one SLOT_ROD_OF_SEASONS cell; each
    // pickup grants one season (slate idiom, no levels). The active season drives the weather
    // everywhere, so it is save state and not per-scene. Appended at the END so older blobs stay
    // readable.
    uint8_t season;       // SEASON_* — the season the cell shows / the world is currently in
    uint8_t seasonsOwned; // SEASON_* bitmask (four bits) — 0 = rod not owned at all
} NeiSaveData;

#define RITO_FLAG_MASK_OWNED (1 << 0)
#define RITO_FLAG_FARORES_OWNED (1 << 1)

// Single accessor — returns the live per-save state (never NULL).
// ── Dual Cane (Somaria / Pacci) — Skijer's NEI ────────────────────────────────────────────────────
// Thin wrappers over NeiSaveData.caneSkills/caneType/caneSkillSel so the C item code and the C++ HUD
// share one source of truth. `skill` is a CANE_SKILL_* index (0..5), `type` a CANE_TYPE_*.
uint8_t Nei_CaneHasSkill(uint8_t skill); // does the player own that skill?
void Nei_CaneGrantSkill(uint8_t skill);  // light its bit (idempotent)
uint8_t Nei_CaneSkillMask(void);         // the whole 6-bit mask (0 = cane not owned)
uint8_t Nei_CaneOwned(void);             // any skill owned -> the cane exists
uint8_t Nei_CaneTypeOwned(uint8_t type); // is that wheel entry unlocked (4 entries)
uint8_t Nei_CaneTypeCount(void);         // how many of the four are owned
uint8_t Nei_CaneNextType(int8_t dir);    // next owned entry, wrapping
uint8_t Nei_CaneGetType(void);           // active cane (auto-corrected to one the player owns)
void Nei_CaneSetType(uint8_t type);
uint8_t Nei_CaneGetSkillSlot(uint8_t type); // selected slot 0..2 for that cane (auto-corrected)
void Nei_CaneSetSkillSlot(uint8_t type, uint8_t slot);
uint8_t Nei_CaneActiveSkill(void); // CANE_SKILL_* the button would cast right now

// ── Trirod echoes (Somaria L3) — Skijer's NEI ────────────────────────────────────────────────────
// `idx` is a row index into gTrirodEchoes (expansions/trirod/trirod_echoes.inc.c).
uint8_t Nei_TrirodEchoLearned(uint8_t idx);
void Nei_TrirodLearnEcho(uint8_t idx);
uint8_t Nei_TrirodLearnedCount(void);
uint8_t Nei_TrirodGetSel(void);
void Nei_TrirodSetSel(uint8_t idx);
void Nei_TrirodGiveAll(void); // save editor
void Nei_TrirodClear(void);   // save editor
uint8_t Nei_TrirodFullList(void);
void Nei_TrirodSetFullList(uint8_t on);
void Nei_TrirodNotify(const char* msg); // Notification::Emit bridge for the C item code

NeiSaveData* Nei_Save(void);

// Hookshot chain level 3 (tiny accessor for TUs that don't want the whole struct; defined in
// nei_save.cpp and already used by z_player_lib.c via a local extern).
uint8_t Nei_UltrashotOwned(void);

// Custom inventory slot helpers (slot 24..71 -> ownedItems[slot-24]).
uint16_t Nei_GetOwnedItem(uint8_t slot);
void Nei_SetOwnedItem(uint8_t slot, uint16_t v);

#ifdef __cplusplus
}
#endif

#endif // NEI_SAVE_H
