/**
 * extended_inventory.h - Extended inventory system for custom items
 *
 * Manages a multi-page inventory system (up to 72 total slots).
 * Page 1: Vanilla OOT items (slots 0-23)
 * Page 2: Custom items (slots 24-47)
 * Page 3: MM Masks (slots 48-71) — requires mm.o2r and CVar enabled
 *
 * Page switching: Press L/A button in pause menu to cycle pages.
 */
#ifndef EXTENDED_INVENTORY_H
#define EXTENDED_INVENTORY_H
#include <stdint.h>
#include <stdbool.h>
#include "z64item.h"
#include "nei_save.h" // Skijer's NEI — custom-slot storage (24..71)
#ifdef __cplusplus
extern "C" {
#endif

// Skijer's NEI — layout of the UNIFIED trade wheel (trade_items.c / sTradeAdultItems).
// SLOT_TRADE_ADULT is a grid over every non-mask trade item the player owns, in this bit order
// (the order is also the tradeAdultOwned bit layout in nei_save.h, so entries may only be APPENDED):
//   0..10  OoT adult trade chain (Pocket Egg .. Claim Check)  — genuinely adult-only
//   11..18 MM trade items (Moon's Tear, 4 Title Deeds, Room Key, Letter to Kafei, Special Delivery)
//   19     Pendant of Memories (== ITEM_EXT_BOOTS_2)
//   20..22 OoT child trade chain (Weird Egg, Cucco, Zelda's Letter)
// Only 0..10 keep AGE_REQ_ADULT; everything above is age-free (see ExtInv_GetSlotAgeReq).
#define TRADE_ADULT_OOT_LAST 10

// Skijer's NEI — custom inventory slots 24..71 live in gNeiSave (NOT in the
// vanilla SaveContext, which only has items[0..23]). These dispatch helpers
// read/write the right backing store by slot index. Use them anywhere a slot
// could be >= 24.
// u16, not u8: page-2 slots live in the widened NeiSaveData.ownedItems and can hold an EXT id above
// 0xFF. Page-1 slots (0..23) are still u8 values from the vanilla inventory, just returned widened.
// Skijer's NEI
static inline uint16_t ExtInv_GetSlotItem(int slot) {
    extern SaveContext gSaveContext;
    if (slot >= 0 && slot < 24) {
        uint16_t item = gSaveContext.inventory.items[slot];
        // Pictograph Box shares the Lens of Truth slot: when owned, the slot is selectable AND
        // equippable even without the real Lens (the in-game C-button routes to the pictobox, and the
        // icon swaps via ExtInv_GetItemIcon). Without this, an empty Lens slot can't be equipped at all.
        // Skijer's NEI
        if (slot == SLOT_LENS && item == ITEM_NONE) {
            extern unsigned char Picto_IsOwned(void);
            if (Picto_IsOwned()) {
                return ITEM_LENS;
            }
        }
        // Power Keg rides the Bomb cell and its ownership lives in its own flag, so owning ONLY the
        // keg (no bomb bag yet) left the cell empty — and an empty cell is skipped by the kaleido
        // cursor, which made the keg unreachable. Synthesise ITEM_BOMB, not a keg id: every piece of
        // the keg (icon swap in ExtInv_GetItemIcon, the C-button in z_parameter.c, the mode wheel in
        // z_kaleido_item.c) keys off `item == ITEM_BOMB && PowerKeg_IsOwned()`. Real bombs stay
        // unusable meanwhile — with no bag the ammo is 0. Skijer's NEI
        if (slot == SLOT_BOMB && item == ITEM_NONE) {
            extern unsigned char PowerKeg_IsOwned(void);
            if (PowerKeg_IsOwned()) {
                return ITEM_BOMB;
            }
        }
        return item;
    }
    return Nei_GetOwnedItem((uint8_t)slot);
}
// An EXT id (>0xFF) only fits in a page-2 slot; the vanilla range truncates, hence the explicit cast.
static inline void ExtInv_SetSlotItem(int slot, uint16_t itemId) {
    extern SaveContext gSaveContext;
    if (slot >= 0 && slot < 24) {
        gSaveContext.inventory.items[slot] = (uint8_t)itemId;
    } else {
        Nei_SetOwnedItem((uint8_t)slot, itemId);
    }
}

// Skijer's NEI — item-action func ptr types (shared with extended_player.h via
// the NEI_ITEM_ACTION_FUNC_TYPES guard so neither header redefines them).
#ifndef NEI_ITEM_ACTION_FUNC_TYPES
#define NEI_ITEM_ACTION_FUNC_TYPES
struct Player;
struct PlayState;
typedef int32_t (*ItemActionUpdateFunc)(struct Player* player, struct PlayState* play);
typedef void (*ItemActionInitFunc)(struct PlayState* play, struct Player* player);
#endif

// Skijer's NEI — rando draw-func ptr type. Same signature/type as ItemTableTypes.h's
// CustomDrawFunc (typedef redefinition to the same type is legal in C11/C++), so
// includers of this header don't have to pull in the item-tables header.
#ifndef NEI_CUSTOM_DRAW_FUNC_TYPE
#define NEI_CUSTOM_DRAW_FUNC_TYPE
struct GetItemEntry;
typedef void (*CustomDrawFunc)(struct PlayState*, struct GetItemEntry*);
#endif

// Skijer's NEI — unified custom-item registry row (single source of truth).
// item: ITEM_xxx (or NEI_NO_ITEM for IA-only rows). slot: page-2/3 inventory
// slot, or NEI_NO_SLOT. ageReq: AGE_REQ_*. iconTex: page-2 icon (NULL = dynamic).
// rg: RandomizerGet for this item (NEI_NO_RG if none / non-uniform). drawFunc:
// rando get-item 3D model (NULL = none). name*: textbox message strings (NULL = none).
#define NEI_NO_ITEM ((int16_t)-1)
#define NEI_NO_SLOT ((uint8_t)0xFF)
#define NEI_NO_RG ((int16_t)0) // RG_NONE

typedef struct {
    int16_t item;
    int16_t ia;
    uint8_t modelGroup;
    uint8_t slot;
    uint8_t ageReq;
    void* iconTex;
    ItemActionUpdateFunc updateFn;
    ItemActionInitFunc initFn;
    CustomDrawFunc drawFunc; // Skijer's NEI
    int16_t rg;              // Skijer's NEI
    const char* nameEn;      // Skijer's NEI
    const char* nameFr;      // Skijer's NEI
    const char* nameDe;      // Skijer's NEI
} NeiItem;

const NeiItem* Nei_FindByItem(int32_t item);
const NeiItem* Nei_FindBySlot(uint8_t slot);
const NeiItem* Nei_FindByRg(int16_t rg); // Skijer's NEI

// ── SW97 primed element (Skijer's NEI) ───────────────────────────────────────
// The bow/slingshot element is a FLAG, not the item on the button. Everything downstream reads
// Sw97_EffectiveElement() and nothing else — that is where the CVar gate and the "bombs are
// bow-only" rule live. `isSling` is 0 for the bow, 1 for the slingshot; the two carry independent
// elements on purpose (spirit arrows and wind bullets may be primed at the same time).
uint8_t Sw97_ElementOwned(uint8_t elem);
uint8_t Sw97_ElementCount(uint8_t isSling);
uint8_t Sw97_ElementAt(uint8_t isSling, uint8_t index);
uint8_t Sw97_GetElement(uint8_t isSling);
void Sw97_SetElement(uint8_t isSling, uint8_t elem);
uint8_t Sw97_ElementNeighbor(uint8_t isSling, uint8_t elem, int32_t dir);
uint16_t Sw97_ElementIcon(uint8_t elem);
uint8_t Sw97_EffectiveElement(uint8_t isSling);
uint8_t Sw97_IsBowItem(uint16_t item);
uint8_t Sw97_IsSlingItem(uint16_t item);
uint8_t Sw97_BombArrowsOwned(void);
uint8_t Sw97_BombArrowsOnButton(void);
uint8_t BombArrows_RandoMode(void);
void Sw97_RefreshButtonIcons(struct PlayState* play);
void Sw97_MigrateLayout(struct PlayState* play); // one-shot, gated by NeiSaveData.sw97LayoutVersion

// ── Elemental Wand (Skijer's NEI) ────────────────────────────────────────────
uint8_t Wand_RandoMode(void);
uint8_t Wand_ModeOwned(uint8_t mode);
void Wand_GrantMode(uint8_t mode);
void Wand_SetModeOwned(uint8_t mode, uint8_t owned); // writes whatever the active treatment reads
uint8_t Wand_ModeCount(void);
uint8_t Wand_ModeAt(uint8_t index);
uint8_t Wand_GetMode(void);
void Wand_SetMode(uint8_t mode);
uint8_t Wand_ModeNeighbor(uint8_t mode, int32_t dir);
uint16_t Wand_ModeMedallion(uint8_t mode);
void* Wand_ModeIcon(uint8_t mode);
void* Wand_ModeNameTex(uint8_t mode);

// ── Sheikah Slate runes (Skijer's NEI) — wand idiom over SLOT_SHEIKAH_SLATE ──
uint8_t Slate_RuneOwned(uint8_t rune);
void Slate_GrantRune(uint8_t rune); // also hands over the slot on the first rune
uint8_t Slate_RuneCount(void);      // owned runes
uint8_t Slate_RuneAt(uint8_t index);
uint8_t Slate_GetRune(void); // active rune (self-healing to an owned one)
void Slate_SetRune(uint8_t rune);
uint8_t Slate_RuneNeighbor(uint8_t rune, int32_t dir);
void* Slate_RuneMiniIcon(uint8_t rune); // 24x24 rune glyph (wheel previews / textbox)
void* Slate_RuneIcon(uint8_t rune);     // 32x32 slate-with-rune-badge (cell / HUD)

// ── Rod of Seasons (Skijer's NEI) — slate idiom over SLOT_ROD_OF_SEASONS ──
uint8_t Seasons_SeasonOwned(uint8_t season);
void Seasons_GrantSeason(uint8_t season); // also hands over the rod on the first season
uint8_t Seasons_SeasonCount(void);        // owned seasons
uint8_t Seasons_SeasonAt(uint8_t index);
uint8_t Seasons_GetSeason(void); // active season (self-healing to an owned one)
void Seasons_SetSeason(uint8_t season);
uint8_t Seasons_SeasonNeighbor(uint8_t season, int32_t dir);
void* Seasons_SeasonIcon(uint8_t season); // 32x32 season glyph (wheel / cell / HUD)
// A season's identity colour — the get-item flame, the icon, the rod's own gem. NOT the colour its
// weather draws with: Winter's snow stays the vanilla grey (see item_rod_of_seasons.c).
void Seasons_SeasonColor(uint8_t season, uint8_t* r, uint8_t* g, uint8_t* b);
// Winter freezes every water surface. OR'd into RocBoots_WalksOnWater, which is the one gate for
// that ability, and read again in z_player.c to make the frozen surface behave as ice rather than
// as water — slippery, and with no ripples.
uint8_t Seasons_WalksOnWater(void);
uint8_t Seasons_IsDrawn(void); // the rod is in Link's hand
// Which tunic the season asks for: Winter forgives every hot room, the desert charges for one.
int16_t Seasons_EnvHazard(struct PlayState* play, int16_t hazard);

typedef struct {
    int currentPage;         // 0 = vanilla, 1 = custom items, 2 = MM masks
    int16_t pageSwitchTimer; // Cooldown to prevent rapid switching
} ExtendedInventoryState;

/**
 * @return Pointer to the global extended inventory state
 */
ExtendedInventoryState* ExtInv_GetState(void);

/**
 * Reset inventory state to defaults (page 0, no cooldown)
 */
void ExtInv_Reset(void);

/**
 * Update inventory state each frame (handles page switch cooldown)
 */
void ExtInv_Update(void);

/**
 * Clamp currentPage if it exceeds max pages (e.g., MM masks CVar toggled off)
 */
void ExtInv_ClampPage(void);

/**
 * @return true if page switch cooldown has elapsed
 */
bool ExtInv_CanSwitchPage(void);

/**
 * Cycle to next page (0 → 1 → 2 → 0, or fewer if MM masks disabled)
 */
void ExtInv_SwitchPage(void);

/**
 * @return Current inventory page (0, 1, or 2)
 */
int ExtInv_GetCurrentPage(void);

/**
 * @return Maximum number of pages (2 or 3 depending on MM masks CVar)
 */
int ExtInv_GetMaxPages(void);

/**
 * @return true if custom items (page 2) CVar is enabled
 */
bool ExtInv_IsCustomItemsEnabled(void);

/**
 * @return true if MM masks inventory CVar is enabled
 */
bool ExtInv_IsMmMasksEnabled(void);

/**
 * @return true if "Only Transformation Masks" sub-option is enabled
 */
bool ExtInv_IsOnlyTransformation(void);

/**
 * Convert visual slot (0-23) to actual inventory slot based on current page
 * @param visualSlot - The slot position shown on screen (0-23)
 * @return Actual inventory slot index (0-71)
 */
int ExtInv_GetInventorySlot(int visualSlot);

/**
 * @param slot - Inventory slot to check
 * @return true if slot belongs to current page
 */
bool ExtInv_IsSlotOnCurrentPage(uint8_t slot);

/**
 * @param slot - Inventory slot
 * @return Page number (0, 1, or 2) where this slot belongs
 */
int ExtInv_GetPageForSlot(uint8_t slot);

/**
 * @param itemId - Item ID (ITEM_xxx constant)
 * @return Age requirement (AGE_REQ_ADULT, AGE_REQ_CHILD, or AGE_REQ_NONE)
 */
uint8_t ExtInv_GetItemAgeReq(uint16_t itemId);

/**
 * @param slot - Inventory slot
 * @return Age requirement for items in this slot
 */
uint8_t ExtInv_GetSlotAgeReq(uint8_t slot);

/**
 * @param row - Equipment row (0=swords, 1=shields, 2=tunics, 3=boots)
 * @param col - Equipment column within row
 * @return Age requirement, accounting for transformation restrictions
 */
uint8_t ExtInv_GetEquipAgeReq(uint8_t row, uint8_t col);

/**
 * @param itemId - Custom item ID
 * @param language - Language index for localization
 * @return Pointer to item name texture, or NULL if not found
 */
void* ExtInv_GetCustomItemNameTex(uint16_t itemId, uint8_t language);

/**
 * @param itemId - Item ID
 * @return Pointer to item icon texture
 */
void* ExtInv_GetItemIcon(uint16_t itemId);

/**
 * Returns the SM64 cap icon directly (decoupled from OOT spells).
 * @param cap - 0 = Vanish, 1 = Metal, 2 = Wing
 * @return Pointer to the cap icon texture, or NULL
 */
void* ExtInv_GetCapIcon(uint8_t cap);

/**
 * @param itemId - Item ID
 * @return Inventory slot for this item, or 0xFF if not found
 */
uint8_t ExtInv_GetItemSlot(uint16_t itemId);

/**
 * @param itemId - MM mask item ID (ITEM_MM_MASK_*)
 * @return 1 if the player owns this MM mask (extended inventory page 3)
 */
int32_t ExtInv_HasMmMask(uint16_t itemId);

/**
 * Trade-mask sale helper for En_Mm (Bunny Hood) / En_Heishi2 (Keaton Mask).
 * If the player does NOT own the given MM counterpart mask, unsets the worn OOT
 * trade mask and gives ITEM_SOLD_OUT; otherwise keeps the (permanent) mask.
 * @param play - Current PlayState
 * @param maskItem - MM mask item ID counterpart (ITEM_MM_MASK_BUNNY / ITEM_MM_MASK_KEATON)
 */
void ExtInv_KeepMmMaskOrSell(struct PlayState* play, uint16_t maskItem);
extern const uint8_t gPage2Items[24];
#define AGE_REQ_ADULT LINK_AGE_ADULT
#define AGE_REQ_CHILD LINK_AGE_CHILD
#define AGE_REQ_NONE 9
extern const uint8_t gPage2ItemAgeReqs[24];
// Roc's Feather Skijer and Roc's Cape share slot 24 (progressive upgrade system)
#define SLOT_ROCS 24                // Shared slot for Roc's Feather/Cape progressive
#define SLOT_ROCS_FEATHER_SKIJER 24 // Alias for compatibility
#define SLOT_ROCS_CAPE 24           // Now same slot as Feather (upgrade replaces it)
#define SLOT_WHIP 25
#define SLOT_SPINNER 26
// Slot 27 used to be Bomb Arrows. Bomb Arrows are the 7th value of the bow's element flag now
// (SW97_ELEM_BOMB) and own no cell; the Elemental Wand took the freed cell. SLOT_BOMB_ARROWS is
// KEPT as a reserved marker because call sites still reference the name — it must never be used to
// store an item again, and gPage2Items[3] must never be shifted (each index maps to a
// NeiSaveData::ownedItems byte, so shifting corrupts every existing save).
#define SLOT_BOMB_ARROWS 27 // RESERVED — do not store into
#define SLOT_ELEMENTAL_WAND 27
#define SLOT_FIRE_ROD 28
#define SLOT_DEMISE_DESTRUCTION 29
#define SLOT_DEKU_LEAF 30
#define SLOT_TIME_GATE 31
#define SLOT_BEETLE 32
#define SLOT_SWITCH_HOOK 33
#define SLOT_ICE_ROD 34
#define SLOT_ZONAI_PERMAFROST 35
#define SLOT_MOGMA_MITTS 36
#define SLOT_GUST_JAR 37
#define SLOT_BALL_AND_CHAIN 38
// The four EXT (u16) page-2 item ids added by the 2026-08-06 re-layout. Values must stay
// byte-identical with the MM side (mm/include/z64item.h). First inventory consumers of the u16
// space — the u8 id space is exhausted. Skijer's NEI
#ifndef EXT_ITEM_SHEIKAH_SLATE
#define EXT_ITEM_SHEIKAH_SLATE 0x0220
#define EXT_ITEM_PHANTOM_HOURGLASS 0x0221
#define EXT_ITEM_SHADOW_CRYSTAL 0x0222
#define EXT_ITEM_ROD_OF_SEASONS 0x0223
#endif
// 2026-08-06 re-layout cell owners (same numbers as MM). The old defines below keep their values so
// existing code compiles; the CELL belongs to the new item.
#define SLOT_SHEIKAH_SLATE 39
#define SLOT_PHANTOM_HOURGLASS 41
#define SLOT_SHADOW_CRYSTAL 44
#define SLOT_ROD_OF_SEASONS 47
#define SLOT_DESIRE_SENSOR 39
#define SLOT_LIGHT_ROD 40
#define SLOT_HYLIAS_GRACE 41
#define SLOT_LANTERN 42
#define SLOT_MINISH_CAP 43
#define SLOT_POKEBALL 44
#define SLOT_CANE_OF_SOMARIA 45
#define SLOT_SHOVEL 46
#define SLOT_DOMINION_ROD 47

// Page 3: MM Mask slots (48-71)
#define SLOT_MM_MASK_POSTMAN 48
#define SLOT_MM_MASK_ALL_NIGHT 49
#define SLOT_MM_MASK_BLAST 50
#define SLOT_MM_MASK_STONE 51
#define SLOT_MM_MASK_GREAT_FAIRY 52
#define SLOT_MM_MASK_DEKU 53
#define SLOT_MM_MASK_KEATON 54
#define SLOT_MM_MASK_BREMEN 55
#define SLOT_MM_MASK_BUNNY 56
#define SLOT_MM_MASK_DON_GERO 57
#define SLOT_MM_MASK_SCENTS 58
#define SLOT_MM_MASK_GORON 59
#define SLOT_MM_MASK_ROMANI 60
#define SLOT_MM_MASK_CIRCUS_LEADER 61
#define SLOT_MM_MASK_KAFEI 62
#define SLOT_MM_MASK_COUPLE 63
#define SLOT_MM_MASK_TRUTH 64
#define SLOT_MM_MASK_ZORA 65
#define SLOT_MM_MASK_KAMARO 66
#define SLOT_MM_MASK_GIBDO 67
#define SLOT_MM_MASK_GARO 68
#define SLOT_MM_MASK_CAPTAIN 69
#define SLOT_MM_MASK_GIANT 70
#define SLOT_MM_MASK_FIERCE_DEITY 71

extern const uint8_t gPage3MaskItems[24];
extern const uint8_t gPage3MaskAgeReqs[24];
static inline uint8_t ExtInv_GetPage2AgeReq(uint8_t slot) {
    if (slot >= 24 && slot < 48) {
        return gPage2ItemAgeReqs[slot - 24];
    }
    return 9;
}
static inline bool ExtInv_CheckAgeReqForSlot(uint8_t slot, bool isAdult) {
    uint8_t req = ExtInv_GetSlotAgeReq(slot);
    return (req == 9) || (req == 0 && isAdult) || (req == 1 && !isAdult);
}
static inline bool ExtInv_ShouldRenderGrayscale(uint8_t slot, bool isAdult) {
    return !ExtInv_CheckAgeReqForSlot(slot, isAdult);
}
static inline void ExtInv_InitializePage2Items(void) { // Skijer's NEI
    for (int i = 0; i < 24; i++) {
        if (Nei_GetOwnedItem((uint8_t)(24 + i)) == ITEM_NONE) {
            Nei_SetOwnedItem((uint8_t)(24 + i), gPage2Items[i]);
        }
    }
}
static inline void ExtInv_ClearPage2Items(void) { // Skijer's NEI
    for (int i = 24; i < 48; i++) {
        Nei_SetOwnedItem((uint8_t)i, ITEM_NONE);
    }
}
// itemId is u16 so a page-2 slot can be given an EXT id (>0xFF); the store behind it is u16 too.
// Skijer's NEI
static inline void ExtInv_GiveItem(uint8_t slot, uint16_t itemId) {
    if (slot >= 24 && slot < 48) {
        Nei_SetOwnedItem(slot, itemId);
    }
}
static inline void ExtInv_SetItemById(uint16_t itemId) { // Skijer's NEI
    uint8_t slot = ExtInv_GetItemSlot(itemId);
    if (slot != 0xFF) {
        ExtInv_SetSlotItem(slot, itemId);
    }
}

// Page 3: MM Mask helpers
// "Only Transformation" mode: transformation masks go to rightmost column (positions 5,11,17,23)
// Deku=pos5(slot53), Goron=pos11(slot59), Zora=pos17(slot65), FierceDeity=pos23(slot71)
#define SLOT_MM_ONLY_DEKU 53
#define SLOT_MM_ONLY_GORON 59
#define SLOT_MM_ONLY_ZORA 65
#define SLOT_MM_ONLY_FIERCE 71

static inline void ExtInv_InitializePage3Masks(void) {
    // No-op: masks are given individually (save editor, randomizer, etc.)
    // This function exists for future initialization if needed
}
static inline void ExtInv_ClearPage3Masks(void) { // Skijer's NEI
    for (int i = 48; i < 72; i++) {
        Nei_SetOwnedItem((uint8_t)i, ITEM_NONE);
    }
}
static inline void ExtInv_GiveMask(uint8_t slot, uint8_t itemId) { // Skijer's NEI
    if (slot >= 48 && slot < 72) {
        Nei_SetOwnedItem(slot, itemId);
    }
}

static inline void ExtInv_VerifyConsistency(void) {
    const int expectedSize = 24;
    const int actualSize = sizeof(gPage2Items) / sizeof(gPage2Items[0]);
    if (actualSize != expectedSize) {}
    const int ageReqSize = sizeof(gPage2ItemAgeReqs) / sizeof(gPage2ItemAgeReqs[0]);
    if (ageReqSize != expectedSize) {}
}

// =============================================================================
// Transformation Mask Item Restriction
//
// When a transformation mask is active, items not in the form's allow list
// are restricted (grayed out in KaleidoScope, can't be equipped/used).
// This integrates with CHECK_AGE_REQ_ITEM/SLOT macros so all existing
// usage sites automatically get the restriction without per-site changes.
// =============================================================================

extern u8 TransformMasks_IsEnabled(void);
extern u8 TransformMasks_IsTransformedAny(void);
extern u8 TransformMasks_IsFDSkinMode(void);
extern u8 TransformMasks_IsItemAllowed(s32 item);
extern u8 TransformMasks_IsSlotAllowed(uint8_t slot);

// Returns true if item is restricted by active transformation mask
static inline bool ExtInv_IsTransformRestricted(int itemId) {
    if (itemId == ITEM_NONE)
        return false; // Empty slot = no restriction
    if (!TransformMasks_IsEnabled() || !TransformMasks_IsTransformedAny())
        return false;
    return !TransformMasks_IsItemAllowed(itemId);
}

// Returns true if the item in a given slot is restricted by active transformation mask
// Uses slot-based arrays (72 elements per form) for direct lookup
static inline bool ExtInv_IsSlotTransformRestricted(uint8_t slot) {
    if (slot >= 72)
        return false;
    if (!TransformMasks_IsEnabled() || !TransformMasks_IsTransformedAny())
        return false;
    return !TransformMasks_IsSlotAllowed(slot);
}

#ifdef __cplusplus
}
#endif
#endif
