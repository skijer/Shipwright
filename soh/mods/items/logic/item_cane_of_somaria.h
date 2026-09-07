/**
 * item_cane_of_somaria.h — Dual Cane (Cane of Somaria / Cane of Pacci).
 *
 * ONE item, ONE inventory slot, TWO canes, SIX skills. Skijer's NEI.
 *
 *   A on the kaleido cell -> switch cane (Somaria / Trirod / Pacci / Ultrahand)
 *   C                     -> first press draws the cane; after that it casts
 *   L / R, cane drawn     -> step the active summon (Somaria) or echo (Trirod)
 *   HOLD L, Trirod        -> the echo grid
 *
 *   C, Somaria  -> casts the active skill. Hold length is irrelevant.
 *   C, Pacci    -> Stone / Ultrahand cast on press, same as Somaria.
 *                  FLIP is the one that splits the button:
 *                     TAP  = flip the enemy onto its back
 *                     HOLD = LIFT the target and hold it in the air
 *                     release from a hold = THROW it, aimed at Link's lock-on
 *                                           target if he has one
 *   C, Ultrahand -> opens its own mode instead of casting (B leaves it).
 *
 *   Somaria summons  Statue  Block  Platform
 *   Pacci   skills   Flip    Stone  Ultrahand
 *
 * Cane selection lives on the pause cell so that in-game L and R are free to
 * step the summon without a menu.
 *
 * Both canes share the same display list; only the prim/env tint differs.
 *
 * The six skills are six SEPARATE obtainable items that all land on the same
 * kaleido slot (user-locked): each one lights its own bit in NeiSaveData.
 * caneSkills, and they may be obtained in any order. The slot shows the cane as
 * soon as ANY bit is lit, and the cane type auto-selects to whichever family the
 * player actually owns. Locked spokes are drawn greyed and cannot be selected.
 *
 * Their pickup ids live in the u16 EXT item space (the u8 ItemId space is full —
 * see the ITEM_EXT_BUTTON note in z64item.h). They are never equipped and never
 * stored in an inventory slot, so they only need to be distinct from each other.
 */

#ifndef ITEM_CANE_OF_SOMARIA_H
#define ITEM_CANE_OF_SOMARIA_H

#include "z64.h"
#include "../custom_items.h"

// =============================================================================
// CANE TYPES
// =============================================================================

// FOUR separate things live on this one cell, not two. Trirod does NOT replace the
// Cane of Somaria and Ultrahand does NOT replace the Cane of Pacci — reaching the
// end of a chain ADDS a new entry to the wheel alongside the one you already had.
// Which one is in hand is `NeiSaveData.caneType`, and it is the only thing the icon
// and name lookups consult; that is what keeps all four off the ITEM_* enum.
//
// Unlock: Somaria at Somaria-L1, Trirod at Somaria-L3, Pacci at Pacci-L1,
// Ultrahand at Pacci-L3.
#define CANE_TYPE_SOMARIA 0
#define CANE_TYPE_TRIROD 1
#define CANE_TYPE_PACCI 2
#define CANE_TYPE_ULTRAHAND 3
#define CANE_TYPE_MAX 4

// Which skill bit gates each type's existence in the wheel.
#define CANE_TYPE_GATE_BIT(type)                                \
    ((type) == CANE_TYPE_SOMARIA  ? CANE_SKILL_SOMARIA_STATUE   \
     : (type) == CANE_TYPE_TRIROD ? CANE_SKILL_SOMARIA_PLATFORM \
     : (type) == CANE_TYPE_PACCI  ? CANE_SKILL_PACCI_FLIP       \
                                  : CANE_SKILL_PACCI_ULTRAHAND)

// =============================================================================
// SKILLS (bit indices into NeiSaveData.caneSkills)
// =============================================================================

#define CANE_SKILL_SOMARIA_STATUE 0
#define CANE_SKILL_SOMARIA_BLOCK 1
#define CANE_SKILL_SOMARIA_PLATFORM 2
#define CANE_SKILL_PACCI_FLIP 3
#define CANE_SKILL_PACCI_STONE 4
#define CANE_SKILL_PACCI_ULTRAHAND 5
#define CANE_SKILL_MAX 6
// The Trirod is not a skill bit: Nei_CaneActiveSkill returns this when it is the
// drawn cane, and Cane_FireSkill dispatches it to Trirod_FireSummon. It sits ABOVE
// CANE_SKILL_MAX, which is itself the "fire nothing" sentinel of the throw swing —
// the two were both 6 once, and the Trirod never fired.
#define CANE_SKILL_TRIROD 7

#define CANE_SKILL_BIT(s) (1 << (s))
#define CANE_SOMARIA_MASK 0x07 // bits 0..2
#define CANE_PACCI_MASK 0x38   // bits 3..5

// Skill index (0..5) from (caneType, slot 0..2) and back.
#define CANE_SKILL_OF(type, slot) (((type)*3) + (slot))
#define CANE_SKILL_TYPE(skill) ((skill) / 3)
#define CANE_SKILL_SLOT(skill) ((skill) % 3)

// =============================================================================
// PICKUP IDS (u16 EXT space — see header comment)
// =============================================================================

#define EXT_ITEM_CANE_SOMARIA_STATUE 0x0210
#define EXT_ITEM_CANE_SOMARIA_BLOCK 0x0211
#define EXT_ITEM_CANE_SOMARIA_PLATFORM 0x0212
#define EXT_ITEM_CANE_PACCI_FLIP 0x0213
#define EXT_ITEM_CANE_PACCI_STONE 0x0214
#define EXT_ITEM_CANE_PACCI_ULTRAHAND 0x0215

// =============================================================================
// STATES
// =============================================================================

#define SOMARIA_STATE_INACTIVE 0
#define SOMARIA_STATE_EQUIPPED 1
#define SOMARIA_STATE_CASTING 2
#define SOMARIA_STATE_AIMING 3 // placing a block/platform (preview shown)
#define SOMARIA_STATE_WHEEL 4  // radial wheel open

// =============================================================================
// WHEEL
// =============================================================================

// Spoke order matches the HUD (CaneWheelHud.cpp): sector 0 = up, clockwise.
#define CANE_SPOKE_SWAP 0 // up    — flip Somaria <-> Pacci
#define CANE_SPOKE_A 1    // right — first skill of the active cane
#define CANE_SPOKE_B 2    // down  — second skill
#define CANE_SPOKE_C 3    // left  — third skill
#define CANE_SPOKE_MAX 4

// L is the selector (user-locked). A TAP of L flips between the two canes; HOLDING
// L past this many frames opens the wheel instead, where the stick picks a skill.
// Moving selection off C is what freed C up to mean two different things per cane.
#define CANE_SELECT_HOLD_FRAMES 10
// Kept as the wheel's own name for the same value, for the HUD's benefit.
#define CANE_WHEEL_HOLD_FRAMES CANE_SELECT_HOLD_FRAMES

// Pacci + Flip: releasing C before this many frames is a TAP (flip the enemy),
// holding past it starts the LIFT. Somaria ignores hold entirely — a press casts.
#define CANE_LIFT_HOLD_FRAMES 10
// Stick magnitude (out of ~60) that counts as a tilt toward a spoke.
#define CANE_WHEEL_STICK_THRESHOLD 30.0f

// =============================================================================
// SETTINGS
// =============================================================================

#define SOMARIA_ANIM_DURATION 30
// Per-kind summon budgets live in somaria_cubes.h (CANE_MAX_STATUES / _BLOCKS /
// _PLATFORMS). They are independent: one kind never evicts another.

// Preview placement: distance in front of the CAMERA-relative facing, and the
// vertical span the floor raycast will accept.
#define CANE_PLACE_DIST 90.0f
#define CANE_PLACE_RAY_UP 120.0f
#define CANE_PLACE_RAY_DOWN 400.0f

// Cane tints. Somaria = red, Pacci = yellow — same display list, different colour.
// Documented here but written out component-by-component at every gDPSetPrimColor
// call site: MSVC's preprocessor passes a "255, 60, 60, 255"-style define to a
// function-like macro as ONE argument, which breaks the expansion.
//   Somaria  prim 255,60,60,255   env 140,0,0,255
//   Pacci    prim 255,215,70,255  env 150,105,0,255

// =============================================================================
// STATE ALIASES
// =============================================================================

#define shSomariaActive gCustomItemState.somariaActive
#define somariaState gCustomItemState.somariaActionType
#define shSomariaAnimating gCustomItemState.somariaAnimating
#define shSomariaAnimTimer gCustomItemState.somariaAnimTimer

#define somariaBlocks gCustomItemState.somariaBlocks
#define somariaBlockCount gCustomItemState.somariaBlockCount
#define somariaOldestSlot gCustomItemState.somariaOldestSlot
#define somariaButtonMask gCustomItemState.somariaButtonMask

// Dual-cane additions (CustomItemState).
#define caneHoldTimer gCustomItemState.caneHoldTimer
#define caneWheelSpoke gCustomItemState.caneWheelSpoke
#define canePreviewValid gCustomItemState.canePreviewValid
#define canePreviewPos gCustomItemState.canePreviewPos
#define canePreviewYaw gCustomItemState.canePreviewYaw
#define canePendingSkill gCustomItemState.canePendingSkill
#define caneSelectTimer gCustomItemState.caneSelectTimer

// =============================================================================
// FUNCTIONS
// =============================================================================

void Handle_CaneOfSomaria(Player* player, PlayState* play);
void Player_InitCaneOfSomariaIA(PlayState* play, Player* player);
void CustomItems_DrawCaneOfSomaria(Player* player, PlayState* play);
s32 Player_UpperAction_CaneOfSomaria(Player* player, PlayState* play);

// Grant one skill (CANE_SKILL_*). Lights its bit, and if this is the first skill
// the player owns, also drops the cane into its inventory slot. Returns 1 when
// something new was granted.
u8 Cane_GiveSkill(u8 skill);
// Same, addressed by EXT_ITEM_CANE_* pickup id. Returns 1 when granted.
u8 Cane_GiveByExtItem(u16 extItemId);

// Accessors for the C++ HUD (CaneWheelHud.cpp).
u8 Cane_IsWheelOpen(void);
u8 Cane_GetType(void);
s32 Cane_GetWheelSpoke(void);
u8 Cane_HasSkill(u8 skill);
u8 Cane_GetActiveSkill(void);
u8 Cane_IsAiming(void);
u8 Cane_PreviewValid(void);

#endif // ITEM_CANE_OF_SOMARIA_H
