/**
 * extended_equipment.h - Extended equipment system (cheat)
 *
 * Adds 12 new equipment pieces (3 swords, 3 shields, 3 tunics, 3 boots)
 * accessible via L button on the pause menu equipment page.
 * All extended equipment is "owned" when the cheat CVar is enabled.
 *
 * Page switching: Press L on equipment screen to toggle vanilla/extended.
 */
#ifndef EXTENDED_EQUIPMENT_H
#define EXTENDED_EQUIPMENT_H

#include <libultraship/libultra.h>
#include "z64item.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// CVar keys
// ---------------------------------------------------------------------------
#define CVAR_EXT_EQUIP_ENABLED "gCheats.ExtEquip.Enabled"
// Extended equipment ownership bits in upper 16 of inventory.equipment
// Bit = 16 + equipType*3 + (index-1)
#define EXT_EQUIP_OWNED_SHIFT 16

// ---------------------------------------------------------------------------
// Extended equipment item IDs (for icon/name lookup, NOT stored in inventory)
// ---------------------------------------------------------------------------
#define ITEM_EXT_SWORD_1 0xE0
#define ITEM_EXT_SWORD_2 0xE1
#define ITEM_EXT_SWORD_3 0xE2
#define ITEM_EXT_SHIELD_1 0xE3
#define ITEM_EXT_SHIELD_2 0xE4
#define ITEM_EXT_SHIELD_3 0xE5
#define ITEM_EXT_TUNIC_1 0xE6
#define ITEM_EXT_TUNIC_2 0xE7
#define ITEM_EXT_TUNIC_3 0xE8
#define ITEM_EXT_BOOTS_1 0xE9
#define ITEM_EXT_BOOTS_2 0xEA
#define ITEM_EXT_BOOTS_3 0xEB

// ---------------------------------------------------------------------------
// Extended equipment indices (1-based, 0 = none)
// ---------------------------------------------------------------------------
typedef enum { EXT_EQUIP_NONE = 0, EXT_EQUIP_1 = 1, EXT_EQUIP_2 = 2, EXT_EQUIP_3 = 3, EXT_EQUIP_MAX = 4 } ExtEquipIndex;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
typedef struct {
    int equipPage;       // 0 = vanilla, 1 = extended
    s16 pageSwitchTimer; // Cooldown (15 frames)
    u8 currentExtSword;  // 0=none, 1-3=ext sword
    u8 currentExtShield; // 0=none, 1-3=ext shield
    u8 currentExtTunic;  // 0=none, 1-3=ext tunic
    u8 currentExtBoots;  // 0=none, 1-3=ext boots
} ExtendedEquipmentState;

extern ExtendedEquipmentState gExtEquipState;

// ---------------------------------------------------------------------------
// Page management
// ---------------------------------------------------------------------------

/** Initialize state from CVars */
void ExtEquip_Init(void);

/** Update per frame (cooldown timer) */
void ExtEquip_Update(void);

/** @return Current equipment page (0=vanilla, 1=extended) */
int ExtEquip_GetPage(void);

// ---------------------------------------------------------------------------
// Age requirements (per ext equipment piece)
// ---------------------------------------------------------------------------

/** @return Age requirement value (AGE_REQ_NONE=9, AGE_REQ_ADULT=0, AGE_REQ_CHILD=1) */
u8 ExtEquip_GetAgeReq(s16 equipType, u8 index);

/** @return 1 if Link's current age satisfies the requirement, 0 otherwise */
u8 ExtEquip_CheckAgeReq(s16 equipType, u8 index);

/** Toggle between vanilla and extended page */
void ExtEquip_SwitchPage(void);

/** @return true if page switch cooldown elapsed */
u8 ExtEquip_CanSwitch(void);

/** @return true if the extra equipment cheat is enabled */
u8 ExtEquip_IsEnabled(void);

// ---------------------------------------------------------------------------
// Equip / Unequip
// ---------------------------------------------------------------------------

/**
 * Equip an extended equipment piece.
 * @param equipType EQUIP_TYPE_SWORD/SHIELD/TUNIC/BOOTS
 * @param index     1-3 (ext equipment index)
 */
void ExtEquip_Equip(s16 equipType, u8 index);

/**
 * Unequip extended equipment of a given type (set to 0).
 * Called when vanilla equipment is equipped.
 * @param equipType EQUIP_TYPE_SWORD/SHIELD/TUNIC/BOOTS
 */
void ExtEquip_Unequip(s16 equipType);

/**
 * The single writer of an equipped ext slot: cleans up the outgoing piece synchronously, applies
 * the vanilla base the new value implies (0 = bare sword/shield, Kokiri tunic/boots) and refreshes
 * the player. Equip/Unequip/C-button/kaleido/age swap/FleetSync all end here.
 */
void ExtEquip_SetSlot(s16 equipType, u8 index);
void ExtEquip_RefreshPlayer(void);  // Player_SetEquipmentData on the live player, if any
void ExtEquip_ResyncFromSave(void); // Nei_Save()->extEquip* -> RAM copy (after a FleetSync apply)
void ExtEquip_ValidateForAge(void); // after Inventory_SwapAgeEquipment: drop age-restricted pieces
u8 ExtEquip_TridentAllowsShield(u8 extIndex, u16 vanillaValue); // Divine or a Mirror only
void ExtEquip_SagesFlashReset(void);

/**
 * @param equipType EQUIP_TYPE_SWORD/SHIELD/TUNIC/BOOTS
 * @return Current extended equipment index (0=none, 1-3=equipped)
 */
u8 ExtEquip_GetCurrent(s16 equipType);

// ---------------------------------------------------------------------------
// Ownership
// ---------------------------------------------------------------------------

/** @return true if the player owns this extended equipment piece */
u8 ExtEquip_HasItem(s16 equipType, u8 index);

/** Give the player an extended equipment piece */
void ExtEquip_GiveItem(s16 equipType, u8 index);

/** Remove an extended equipment piece from the player */
void ExtEquip_RemoveItem(s16 equipType, u8 index);

// ---------------------------------------------------------------------------
// Icons / Names
// ---------------------------------------------------------------------------

/**
 * Get icon texture for an extended equipment item.
 * @param equipType EQUIP_TYPE_SWORD/SHIELD/TUNIC/BOOTS
 * @param index     1-3
 * @return Pointer to 32x32 RGBA32 texture data
 */
void* ExtEquip_GetIcon(s16 equipType, u8 index);

/**
 * Get the extended item ID for a given equipment type and index.
 * @param equipType EQUIP_TYPE_SWORD/SHIELD/TUNIC/BOOTS
 * @param index     1-3
 * @return ITEM_EXT_xxx constant
 */
u16 ExtEquip_GetItemId(s16 equipType, u8 index);

/**
 * Toggle an extended equipment item from a C button press.
 * If the item's equipment type is already equipped with this index, unequip it.
 * Otherwise, equip it.
 * @param itemId ITEM_EXT_xxx constant (0xE0-0xEB)
 */
void ExtEquip_ToggleFromCButton(u16 itemId);

/**
 * Get name texture for an extended equipment item.
 * @param itemId ITEM_EXT_xxx constant
 * @param language Language index
 * @return Pointer to name texture, or NULL for placeholder
 */
void* ExtEquip_GetNameTex(u16 itemId, u8 language);

// ---------------------------------------------------------------------------
// Transform integration
// ---------------------------------------------------------------------------

/** Backup current ext equip state and unequip all. Called on transformation. */
void ExtEquip_UnequipForTransform(void);

/** Restore ext equip from backup. Called on detransformation to human. */
void ExtEquip_RestoreFromTransform(void);

/** Discard backup without restoring. Called on reset/reload/death. */
void ExtEquip_ClearTransformBackup(void);

// ---------------------------------------------------------------------------
// Divine Shield helpers (called from z_player_lib.c and z_player.c)
// ---------------------------------------------------------------------------
u8 ExtEquip_ShieldIsWooden(void);
void DivineShield_OnShieldBlock(Player* player, PlayState* play);

// ---------------------------------------------------------------------------
// Shield of Ikana helpers (called from z_player.c at the bounce-detection point)
// ---------------------------------------------------------------------------
void Ikana_OnShieldBlock(Player* player, PlayState* play);

// ---------------------------------------------------------------------------
// Behavior state
// ---------------------------------------------------------------------------

typedef enum {
    PEGASUS_IDLE,
    PEGASUS_WINDUP,
    PEGASUS_RUNNING,
    PEGASUS_BONK,
} PegasusState;

typedef enum {
    DSCALE_INACTIVE,
    DSCALE_SWIMMING,
} DragonScaleState;

typedef struct {
    // Cane of Byrna (Ext Sword 1)
    u8 byrnaSavedSwordEquip;   // Original equips.equipment sword nibble
    u8 byrnaSavedButtonItem;   // Original equips.buttonItems[0]
    f32 byrnaSavedSwordHealth; // Original swordHealth (GK durability)
    u8 byrnaSavedBgsFlag;      // Original bgsFlag (1=BGS, 0=GK)
    u8 byrnaActive;            // Whether Byrna has overridden sword state

    // Pegasus Anklet
    u8 pegasusState;
    s16 pegasusTimer;
    s16 pegasusMagicTick;
    u8 pegasusColInit;
    f32 pegasusWingAngle; // Pendulum angle for wing charm (radians)
    f32 pegasusWingVel;   // Pendulum angular velocity

    // Water Dragon Scale
    u8 dragonScaleState;
    s16 dragonScalePitch; // swim pitch angle
    s16 dragonScaleMagicTick;
    u8 dragonScaleColInit;

    // Iron Knuckle Axe (Ext Sword 3)
    u8 ikAxeSavedSwordEquip;
    u8 ikAxeSavedButtonItem;
    u8 ikAxeActive;
    u8 ikAxeDrawing; // 1 when hammer is out (hide vanilla sword DL), 0 in free mode

    // Four Sword (Ext Sword 2)
    s16 fourSwordBHoldTimer; // frames B has been held while shielding
    u8 fourSwordCharging;    // 1 while charge is armed (B+shield >= threshold)
    // Bit i = clone i SHOULD exist. The actors themselves die with the scene, so this is the intent
    // FourSwordClone_Reconcile rebuilds the summon from.
    u8 fourSwordCloneMask;

    // Four Sword: rising-edge detection for Ivan-style item spawn
    u8 fourSwordPrevA73;       // previous player->unk_A73 (arrow/boomerang fire)
    u8 fourSwordPrevCarrying;  // previous PLAYER_STATE1_CARRYING_ACTOR bit
    u8 fourSwordPrevBoomerang; // previous (player->boomerangActor != NULL)
    s16 fourSwordItemCooldown; // global cooldown prevents actor spam (10 frames)
} ExtEquipBehaviorState;

extern ExtEquipBehaviorState gExtEquipBehavior;

// Champion's Tunic slow factor — 1.0f normal, 0.33f during Flurry Rush / Bullet Time
// Used in z_actor.c Actor_UpdatePos to scale non-player actor movement.
extern f32 gChampionSlowFactor;

// ---------------------------------------------------------------------------
// Behavior
// ---------------------------------------------------------------------------

/**
 * Called per frame from Player_Update when extended equipment is active.
 * Dispatches to individual behavior handlers.
 */
void ExtEquip_UpdateBehavior(void* player, void* play);

/**
 * Called from z_player.c when melee weapon quads register a hit (AT_HIT).
 * Used by Cane of Byrna for MP recovery.
 */
void ExtEquip_OnMeleeHit(void* player, void* play);

/**
 * Ext-equipment parry dispatcher — the SINGLE z_player.c hook for every page-2
 * piece whose guard is an animation rather than a raised shield (shieldQuad never
 * bounces for those, so the *_OnShieldBlock hook never fires for them).
 *
 * Sits in the damage branch chain next to GerudoMhr_TryParry. Returns 1 when a
 * piece consumed the hit, so the vanilla damage branches are skipped. Add new
 * pieces inside the dispatcher, NOT as new branches in z_player.c.
 * Skijer's NEI
 */
u8 ExtEquip_TryParry(void* play, void* player);

/**
 * True when an ext piece owns the B button and OOT must NOT interpret it as a
 * sword swing. Read from Player_GetItemOnButton (index 0), the same place the
 * Deku form and the sword-blocking masks hook — that is BEFORE actionFunc, which
 * is the only point early enough to stop the vanilla swing from starting.
 * Skijer's NEI
 */
u8 ExtEquip_BlocksBButtonSword(void);

/**
 * Called from z_player.c draw section for equipment-specific visuals
 * (barriers, auras, etc.).
 */
void ExtEquip_DrawBehavior(void* player, void* play);

/**
 * Returns 1 if the vanilla sword DL should be hidden (replaced by ext equipment draw).
 * Called from z_player_lib.c in the limb draw callback.
 */
u8 ExtEquip_ShouldHideSwordDL(void);

// ---------------------------------------------------------------------------
// Kite Shield — shield surfing (ext shield 2). State lives in
// mods/equipment/behaviors/equip_kite_shield.c, the engine in mods/equipment/kite_surf.c.
// ---------------------------------------------------------------------------

/** True while the surf owns the player at all, mount and dismount included. */
u8 KiteSurf_IsActive(void);

/** True only while actually riding — the board is out and the hand/back shield must not draw. */
u8 KiteSurf_IsRiding(void);

/** Draws the board under Link's feet. Called from Player_PostLimbDraw on PLAYER_LIMB_ROOT. */
void ExtEquip_DrawKiteSurfBoard(void* play);

// KiteSurf_AdjustLimb (the lower-body crouch/lean) is NOT declared here on purpose: it takes a
// Vec3s*, and this header is pulled in by z64item.h — i.e. by translation units that have not seen
// z64math.h yet. z_player_lib.c declares it locally, the way it already does for the BossRemains
// limb hooks.
// Trident (ext sword 3) sword-trail / melee-quad frame: pushes the matrix into the
// lance's own frame (caller pops) and returns 1 while the lance is out; 0 otherwise.
u8 ExtEquip_TridentTrailBegin(void);
f32 ExtEquip_TridentTrailLength(void);
void ExtEquip_TridentApplyHeldTransform(void);
// Spin-attack charge glow: replaces En_M_Thunder's per-sword translate/scale block
// with the lance's frame so the glow covers the trident. Called with mf_9E0 current.
u8 ExtEquip_TridentThunderTransform(void);

// Trident (ext sword 3) ground chain. Same arrangement as GerudoMhr_NextComboMwa /
// GerudoMhr_OwnsComboRow, and for the same reason: OOT picks the swing row from the
// STICK ANGLE and only reaches a _COMBO row on the third consecutive press, so a
// fixed 1->2->3 sequence cannot be expressed by filling the six rows. Both are
// called from func_80837948, which lives above the unity include of this module —
// hence the declarations here rather than in the .c. Skijer's NEI
s32 Trident_NextComboMwa(Player* player, s32 requested);
u8 Trident_OwnsComboRow(Player* player);
// True for the rows of the ground chain, so func_80837948 starts them with a MORPH
// (Player_AnimChangeOnceMorphAdjusted) instead of cutting straight to frame 0 —
// otherwise each slash begins from a pose the previous one never reached.
u8 Trident_MorphsRow(Player* player, s32 mwa);
// Keeps vanilla's hold-B charge reachable after the trident's long swings — same
// hook and same reason as GerudoMhr_HoldsChargeWindow (Player_UpdateCommon).
u8 Trident_HoldsChargeWindow(Player* player);
// True during the untouchable opening of the max-charge release: z_player_lib.c
// paints the tunic gold so the immunity window is visible.
u8 Trident_GoldenArmor(void);
// Guard clips for the vanilla shield action: 0 raise, 1 hold, 2 lower. NULL = vanilla.
// (Trident_GetGuardAnim is gone: the trident's shield poses are vanilla's now.)

/**
 * Multiplier on how fast the LEG cycle turns over, asked for by func_8084029C after
 * it clamps the phase rate. This is the only lever that works: every locomotion
 * action loads the joint table from unk_868 and never reads skelAnime.playSpeed.
 * 1.0f unless something (the Pegasus dash) is deliberately speeding the legs up.
 */
f32 ExtEquip_LegCycleRateMul(void);
// R+B from the guard: the clip func_808428D8 plays instead of link_normal_defense_kiru.
// Only the fallback for when the guard dash cannot start — R+B is the dash.
LinkAnimationHeader* Trident_GetGuardStabAnim(Player* player);
// (Trident_OnShieldBlock is gone: the trident no longer parries. Its guard is a plain
// vanilla block with whatever shield its age gets — Divine as a child, Mirror as an adult.)
// Draw / sheathe clips for Player_StartChangingHeldItem. NULL = vanilla's.
LinkAnimationHeader* Trident_GetItemChangeAnim(Player* player, s8 newItemAction, s32* itemChangeType);
// Jump slash launch (func_8083BA90, next to GerudoMhr_AdjustJumpSlash): shorter, lower hop.
void Trident_AdjustJumpSlash(Player* player, s32 mwa);
// True while the Phantom Ganon flight owns Link (Player_HandleExitsAndVoids skips the void check).
u8 Trident_IsFlying(void);

/**
 * Returns the MM Mirror Shield OTR path if Shield of Ikana is equipped, NULL otherwise.
 * Called from z_player_lib.c to override shield DL.
 */
const char* ExtEquip_GetShieldDLOverride(void);

/**
 * Draw the ext shield DL in the current matrix context (called from PostLimbDraw).
 * For Shield of Ikana: draws GI Mirror Shield model.
 */
void ExtEquip_DrawShieldDL(void* play);
void ExtEquip_DrawShieldBackDL(void* play);

/**
 * Draw Dragon Scale pendant at waist. Called from PostLimbDraw for PLAYER_LIMB_WAIST.
 */
// ExtEquip_DrawWaistScale removed — Water Dragon Scale item deleted (Zora swim = Zora Tunic effect).
// Retired ext slots (Cape/Pendant moved to the upgrade column, Dragon Scale deleted): true = the
// slot is dead in the grid (ownership bits still meaningful for the new systems).
u8 ExtEquip_SlotRetired(s16 equipType, u8 index);

// Extended recolor tunics (Skijer 2026-07-16) — currently-equipped predicates + Spirit money gate:
u8 ExtEquip_IsChampionTunic(void); // ext tunic 1 (blue) equipped
u8 ExtEquip_IsSpiritTunic(void);   // ext tunic 2 (orange/black) equipped
u8 ExtEquip_IsSagesTunic(void);    // ext tunic 3 (white) equipped
typedef enum {
    SAGES_RESIST_ICE,
    SAGES_RESIST_FIRE,
    SAGES_RESIST_THUNDER,
    SAGES_RESIST_STUN,
    SAGES_RESIST_FALL,
    SAGES_RESIST_WIND,
} SagesResistance;
u8 ExtEquip_HasSagesResistance(SagesResistance resistance);
void ExtEquip_SagesFlash(SagesResistance resistance); // a resistance just absorbed damage
void ExtEquip_SagesFlashTick(void);                   // per-frame decay (Sages_Behavior)
void ExtEquip_GetSagesTunicColor(u8* r, u8* g, u8* b);
u8 ExtEquip_SpiritHasMoney(void); // Spirit equipped AND rupees > 0
void ExtEquip_GiveCape(void);     // grant the Magic Cape (dedicated ownership flag)
void* ExtEquip_GetCapeIcon(void); // upgrade-column icon (decoupled from the ext grid slot)
void* ExtEquip_GetPendantIcon(void);

// Upgrade-column passives (Magic Cape / Pendant of Memories — Skijer 2026-07-15):
// MAGIC_REQ — the Magic Cape's real effect (commit 10a66533): HALVES the magic cost, so a spell is
// castable with only half the base magic. Applied at the shared ItemMagic_* helper (all custom magic
// items), at Magic_RequestChange (vanilla spells + API users) and at the direct-writer sites (Four
// Sword clones, Deku Leaf). Passive: active whenever the cape is OWNED, independent of visibility.
#define MAGIC_REQ(cost) (ExtEquip_CapeOwned() ? ((cost) / 2) : (cost))
u8 ExtEquip_CapeOwned(void);
u8 ExtEquip_CapeVisible(void); // owned && not hidden (draw the cloth)
void ExtEquip_ToggleCapeVisibility(void);
u8 ExtEquip_PendantOwned(void);  // owns the Pendant as EQUIPMENT (permanent once granted)
void ExtEquip_GivePendant(void); // grant it (the adult trade slot does this automatically)
u8 ExtEquip_PendantActive(void); // owned && effect toggle ON
void ExtEquip_TogglePendantEffect(void);

/**
 * Draw the ext sword DL in the current matrix context (called from PostLimbDraw).
 * For Byrna: draws blue Somaria cane.
 */
void ExtEquip_DrawSwordDL(void* play);

// Matrix for the sword DL above: the Cane of Byrna's own placement, or the
// original one for everything else that draws there.
void ExtEquip_ApplySwordDLMatrix(void);

// Byrna swing trail, measured in the cane's frame. Begin pushes a matrix and
// returns 1 when the cane is out; the caller pops it.
u8 ExtEquip_ByrnaTrailBegin(void);
f32 ExtEquip_ByrnaTrailLength(void);

// True while the Cane of Byrna should wield two-handed (no shield), like the BGS.
u8 ExtEquip_ByrnaIsTwoHanded(void* player);

// Insect Glaive ground chain. Same arrangement as Trident_* / GerudoMhr_*: OOT
// picks the swing row from the stick angle, which cannot express a fixed chain.
s32 ByrnaIg_NextComboMwa(Player* player, s32 requested);
u8 ByrnaIg_OwnsComboRow(Player* player);
u8 ByrnaIg_MorphsRow(Player* player, s32 mwa);
u8 ByrnaIg_IsAirborne(void);
u8 ByrnaIg_AirSuperDamage(void);

/**
 * Draw anklet decoration on foot limbs (torus + fairy wings with pendulum).
 * Called from PostLimbDraw for PLAYER_LIMB_L_FOOT and PLAYER_LIMB_R_FOOT.
 * @param play PlayState
 * @param isRightFoot 1 for right foot, 0 for left foot
 */
// ExtEquip_DrawAnklet removed — Pegasus model = red hover boots in Player_DrawImpl.

/**
 * Update pendulum physics for anklet wings. Called from Pegasus_Behavior.
 */
// ExtEquip_UpdateAnkletPhysics removed with the anklet wing model.

/**
 * Capture shoulder world positions for cloth physics (Magic Cape + Champion's Scarf).
 * Called from PostLimbDraw for PLAYER_LIMB_L_SHOULDER and PLAYER_LIMB_R_SHOULDER.
 * @param limbIndex The limb being drawn
 */
void ExtEquip_CaptureCapeShoulderPos(s32 limbIndex);

/**
 * Suppress icon override for ext equipment (used by kaleido equipment screen).
 * When set to 1, ExtInv_GetItemIcon won't replace sword/shield icons.
 */
extern u8 gExtEquipSuppressIconOverride;
// 1 while the equipment page names one of its PAGE-2 GRID cells (disambiguates the one item id
// shared by the Pendant of Memories and the Climb Boots — see extended_equipment.c).
extern u8 gExtEquipGridNameContext;

// ---------------------------------------------------------------------------
// Shield of Ikana: Death Save
// ---------------------------------------------------------------------------

/** Check if Shield of Ikana should revive player instead of dying */
u8 ExtEquip_IkanaDeathSave(void* play);

/** Draw Spirit Breastplate (Iron Knuckle armor) on Link's torso.
 *  Called from PostLimbDraw for PLAYER_LIMB_UPPER. */
void ExtEquip_DrawBreastplate(void* play);

#ifdef __cplusplus
}
#endif

#endif // EXTENDED_EQUIPMENT_H
