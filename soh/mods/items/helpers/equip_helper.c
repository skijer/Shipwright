/**
 * equip_helper.c - Item input and equip state management
 */

#include "equip_helper.h"
#include "../custom_items.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "libultraship/bridge.h"
#include "transformation_masks/transformation_masks.h"
#include "extended_inventory.h" // Sw97_* — Bomb Arrows rides the bow's element flag (Skijer's NEI)

typedef struct {
    u32 frameCount;
    u8 cachedItems[8];
    u16 cachedButtons[256];
} EquipCache;

static EquipCache sEquipCache = { 0 };

static void EquipCache_Update(PlayState* play) {
    if (sEquipCache.frameCount == play->gameplayFrames)
        return;
    sEquipCache.frameCount = play->gameplayFrames;

    for (int i = 0; i < 256; i++)
        sEquipCache.cachedButtons[i] = 0;

    u8 dpadEnabled = CVarGetInteger("gEnhancements.DpadEquips", 0);
    u8 maxSlot = dpadEnabled ? 8 : 4;

    // Slot 0 = B button (sButtonMasks[0] = BTN_B). Start at 0 so custom
    // items equipped to B (e.g. Roc's Feather, transformation masks) get
    // registered alongside C-buttons / D-pad slots. Previously the loop
    // started at slot 1, so B-equipped custom items never received input.
    for (u8 slot = 0; slot < maxSlot; slot++) {
        u8 itemId = gSaveContext.equips.buttonItems[slot];
        sEquipCache.cachedItems[slot] = itemId;
        if (itemId != ITEM_NONE && itemId < 256) {
            sEquipCache.cachedButtons[itemId] = sButtonMasks[slot];
        }
        // Skijer's NEI — Bomb Arrows has no inventory slot and never reaches a button; it is the
        // 7th value of the bow's element flag. Everything in item_bombarrows.c asks this cache
        // "which button is ITEM_BOM_ARROWS on?", so aliasing it onto the bow's button here is what
        // keeps that whole state machine (baButtonMask, press edges, cleanup) working untouched.
        if (Sw97_IsBowItem(itemId) && (Sw97_EffectiveElement(0) == SW97_ELEM_BOMB)) {
            sEquipCache.cachedButtons[ITEM_BOMB_ARROWS] = sButtonMasks[slot];
        }
    }
}

u16 ItemInput_GetEquippedButton(u8 itemId, PlayState* play) {
    EquipCache_Update(play);
    return sEquipCache.cachedButtons[itemId];
}

// mods/actors/cane_pacci.c - while Ultrahand mode is up the D-pad rotates and moves the held
// object. mods/actors/master_cycle.c - on the bike D-up is the wheelie and D-down cancels it.
// mods/actors/cryonis_rune.c - in the aiming mode D-up/D-down choose how far out the pillar goes.
u8 Pacci_UltrahandModeActive(void);
u8 MasterCycle_IsRiding(void);
u8 Cryonis_ModeActive(void);

// Is this button spoken for THIS FRAME by something that has taken the pad over?
//
// There are four separate places in this fork that decide whether a button press means "use what
// is equipped here", and they do not share a path: Player_GetItemOnButton for engine items,
// ItemInput_Update for custom ones, transformation_masks.c for masks worn while transformed, and
// the in-water Zora clause in custom_items_common.c. The first three all scan the raw pad against
// buttonItems themselves, which is exactly why a guard placed in any ONE of them keeps not being
// enough - Roc's Cape leaked through the second, and the Kafei mask through the third.
//
// So the ANSWER lives here once and the four sites ask the question. Adding a fifth claimant means
// editing this function and nothing else.
u8 ItemInput_ButtonIsClaimed(u16 button) {
    if (!(button & (BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT))) {
        return 0; // only the D-pad is ever claimed; B and the C buttons are never taken this way
    }
    return (Pacci_UltrahandModeActive() || MasterCycle_IsRiding() || Cryonis_ModeActive()) ? 1 : 0;
}

void ItemInput_Update(ItemInputState* out, u8 itemId, Player* player, PlayState* play) {
    out->equippedButton = ItemInput_GetEquippedButton(itemId, play);
    out->wasEquipped = (out->equippedButton != 0);

    // Custom items never go through Player_GetItemOnButton - they find themselves in buttonItems
    // and read the raw pad here - so the guard placed in that engine function did nothing for them.
    // Roc's Cape on a D-pad slot kept firing right through Ultrahand mode because of exactly this
    // second path. An item sitting on the D-pad is simply not usable while the mode owns it.
    if (out->wasEquipped && ItemInput_ButtonIsClaimed(out->equippedButton)) {
        out->isPressed = out->isHeld = out->isReleased = out->otherButtonPressed = out->damageTaken = 0;
        return;
    }

    if (!out->wasEquipped) {
        out->isPressed = out->isHeld = out->isReleased = out->otherButtonPressed = out->damageTaken = 0;
        return;
    }

    u16 press = play->state.input[0].press.button;
    u16 held = play->state.input[0].cur.button;

    out->isPressed = (press & out->equippedButton) != 0;
    out->isHeld = (held & out->equippedButton) != 0;
    out->isReleased = !out->isHeld && !out->isPressed;
    out->otherButtonPressed = ItemInput_CheckOtherButtons(out->equippedButton, &play->state.input[0]);
    out->damageTaken = 0;
}

u8 ItemInput_CheckDamage(Player* player, s8* prevInvincibility) {
    u8 damage = (player->invincibilityTimer > 0 && *prevInvincibility == 0);
    *prevInvincibility = player->invincibilityTimer;
    return damage;
}

u8 ItemInput_CheckOtherButtons(u16 equippedButton, Input* input) {
    static const u16 sActionButtons = BTN_A | BTN_B | BTN_R | BTN_START | BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT | BTN_DUP |
                                      BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT;
    return (input->press.button & (sActionButtons & ~equippedButton)) != 0;
}

u8 ItemInput_IsBlockedEx(Player* player, PlayState* play, u8 skipOptionalBlockers) {
    // Custom items during transformation: allowed items stay on C-buttons
    // (MmForm_SaveAndRestrictEquips unequips blocked items on transform).
    // If an item is still equipped, the slot allowlist permits it.

    if (player->stateFlags1 & ITEM_BLOCK_STATE1)
        return 1;
    if (player->stateFlags1 & PLAYER_STATE1_START_CHANGING_HELD_ITEM)
        return 1;
    if (play->shootingGalleryStatus != 0)
        return 1;

    if (!skipOptionalBlockers) {
        if (player->meleeWeaponState != 0)
            return 1;
        if (player->stateFlags1 & PLAYER_STATE1_SHIELDING)
            return 1;
        if ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) && !(player->actor.bgCheckFlags & 0x0001))
            return 1;
    }

    return 0;
}

u8 ItemInput_IsBlocked(Player* player, PlayState* play) {
    return ItemInput_IsBlockedEx(player, play, 0);
}

void ItemInput_RequestItemChange(Player* player, PlayState* play) {
    if (player->heldItemAction >= 0 && player->heldItemAction != PLAYER_IA_NONE) {
        player->heldItemId = ITEM_NONE;
        player->stateFlags1 |= PLAYER_STATE1_START_CHANGING_HELD_ITEM;
    }
}

u8 ItemInput_CanInterrupt(Player* player) {
    if (player->meleeWeaponState != 0)
        return 0;
    if (player->stateFlags1 & (PLAYER_STATE1_CHARGING_SPIN_ATTACK | PLAYER_STATE1_CARRYING_ACTOR |
                               PLAYER_STATE1_READY_TO_FIRE | PLAYER_STATE1_BOOMERANG_THROWN))
        return 0;
    return 1;
}

u8 Slate_IsDrawn(void);
u8 Seasons_IsDrawn(void);
u8 Wand_IsDrawn(void);
u8 Hourglass_WantsEmptyHand(void);
u8 Pacci_IsHoldingUltrahand(void);

// Read at draw time, where the hand's DL table and type are chosen together; forcing rightHandType
// from the item left them out of step (shield table + closed type = the table's NULL first row).
u8 ItemEquip_HoldsClosedFist(void) {
    return Slate_IsDrawn() || Seasons_IsDrawn() || Wand_IsDrawn();
}

// The two items that borrow the HOOKSHOT model group for its extended-arm pose and must not get
// the hookshot that comes with it. Same rule as above: the table and the type are set together at
// draw time, never poked into the Player from item code.
u8 ItemEquip_HoldsEmptyHand(void) {
    return Hourglass_WantsEmptyHand() || Pacci_IsHoldingUltrahand();
}

// ── Handheld models in the right fist ────────────────────────────────────────────────────────────
extern u8 ResourceMgr_FileExists(const char* resName);
extern Gfx* ResourceMgr_LoadGfxByName(const char* path);

// The right fist's own matrix, taken during the skeleton draw (Player_PostLimbDrawGameplay at
// PLAYER_LIMB_R_HAND). ONE owner for every handheld: a Four Sword clone must get its own hand and
// never Link's, and a transformed body draws through its own callback and never reaches the capture.
static MtxF sHandMtx;
static u8 sHandMtxValid = 0;

void ItemEquip_CaptureHandMatrix(void) {
    Matrix_Get(&sHandMtx);
    sHandMtxValid = 1;
}

void ItemEquip_ReleaseHandMatrix(void) {
    sHandMtxValid = 0;
}

// Six wand rods carry an opaque and a translucent DL each, so a session can walk 12 paths before
// the slate's and the rod's are counted. Sized to hold every handheld, not just the ones in hand.
#define HAND_MODEL_CACHE 24

// Keyed by the literal's ADDRESS — every caller passes a string literal, so identity is enough and
// no strcmp runs per frame. ResourceMgr_LoadGfxByName crashes on a missing path, hence FileExists.
static Gfx* ItemEquip_LoadHandGfx(const char* path) {
    static const char* sPaths[HAND_MODEL_CACHE];
    static Gfx* sDLs[HAND_MODEL_CACHE];
    static u8 sCount = 0;

    for (u8 i = 0; i < sCount; i++) {
        if (sPaths[i] == path) {
            return sDLs[i];
        }
    }
    if (sCount >= HAND_MODEL_CACHE) {
        return NULL;
    }
    sPaths[sCount] = path;
    sDLs[sCount] = ResourceMgr_FileExists(path) ? ResourceMgr_LoadGfxByName(path) : NULL;
    sCount++;
    return sDLs[sCount - 1];
}

u8 ItemEquip_DrawHeldModel(Player* player, PlayState* play, const char* opaPath, const char* xluPath,
                           const ItemHandPose* pose) {
    Gfx* opa = ItemEquip_LoadHandGfx(opaPath);
    Gfx* xlu = (xluPath != NULL) ? ItemEquip_LoadHandGfx(xluPath) : NULL;
    f32 unscale;
    f32 scale;

    if (!sHandMtxValid || (opa == NULL)) {
        return 0;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    // The hand bone itself: position AND full orientation, so the model turns with the wrist.
    Matrix_Put(&sHandMtx);

    // That matrix carries the player's own 0.01 body scale. Dividing it back out puts the offsets
    // and the scale below in world units, which is what they were tuned in.
    unscale = (player->actor.scale.x != 0.0f) ? (1.0f / player->actor.scale.x) : 1.0f;
    Matrix_Scale(unscale, unscale, unscale, MTXMODE_APPLY);

    Matrix_RotateY(DEG_TO_RAD(pose->rotY), MTXMODE_APPLY);
    Matrix_RotateX(DEG_TO_RAD(pose->rotX), MTXMODE_APPLY);
    Matrix_RotateZ(DEG_TO_RAD(pose->rotZ), MTXMODE_APPLY);

    // Offsets AFTER the rotations, so each slides the model along its OWN axis — "up" means up the
    // staff whichever way the hand points. Reordering these is not a refactor.
    Matrix_Translate(pose->offsetX, pose->offsetY, pose->offsetZ, MTXMODE_APPLY);

    scale = pose->scale;
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, opa);

    if (xlu != NULL) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, xlu);
    }

    CLOSE_DISPS(play->state.gfxCtx);
    return 1;
}

void ItemEquip_PlayEquipSFX(PlayState* play, Player* player) {
    Audio_PlaySoundGeneral(NA_SE_PL_CHANGE_ARMS, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

void ItemEquip_PlayUnequipSFX(PlayState* play, Player* player) {
    Audio_PlaySoundGeneral(NA_SE_PL_CHANGE_ARMS, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

u8 ItemEquip_Update(ItemEquipState* state, ItemInputState* input, EquipCallback onEquip, UnequipCallback onUnequip,
                    Player* player, PlayState* play) {
    if (!input->wasEquipped) {
        if (state->isEquipped && onUnequip)
            onUnequip(play, player);
        state->isEquipped = 0;
        return 0;
    }

    if (ItemInput_CheckDamage(player, &state->prevInvincibility)) {
        if (state->isEquipped && onUnequip)
            onUnequip(play, player);
        state->isEquipped = 0;
        return 0;
    }

    if (input->otherButtonPressed) {
        if (state->isEquipped && onUnequip)
            onUnequip(play, player);
        state->isEquipped = 0;
        return 0;
    }

    if (!state->isEquipped && input->isPressed) {
        if (onEquip)
            onEquip(play, player);
        state->isEquipped = 1;
    }

    return state->isEquipped;
}

// Chateau Romani and rando's Magic Infinite both raise this flag, and the vanilla magic path
// honours it — custom items must read the same one or they drain a meter the engine calls bottomless.
static u8 ItemMagic_IsInfinite(void) {
    return Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER) != 0;
}

void ItemMagic_Consume(PlayState* play, s16 amount) {
    if (ItemMagic_IsInfinite())
        return;

    // Magic Cape passive (Skijer 2026-07-15): all custom magic items cost HALF while the cape is
    // owned — and the matching HasEnough check below means they're castable with half the magic.
    // (Commit 10a66533's MAGIC_REQ, applied once here for every ItemMagic_* user.)
    extern u8 ExtEquip_CapeOwned(void);
    if (ExtEquip_CapeOwned())
        amount /= 2;

    if (gSaveContext.magic >= amount)
        gSaveContext.magic -= amount;
}

s32 ItemMagic_HasEnough(PlayState* play, s16 amount) {
    if (gSaveContext.magicCapacity <= 0)
        return 0;
    if (ItemMagic_IsInfinite())
        return 1;

    extern u8 ExtEquip_CapeOwned(void);
    if (ExtEquip_CapeOwned())
        amount /= 2; // Magic Cape: castable with half the base cost

    return (gSaveContext.magic >= amount);
}

u8 ItemSword_HasAnySword(void) {
    if (CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI))
        return 1;
    if (CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER))
        return 1;
    if (CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON))
        return 1;
    return 0;
}

u8 ItemSword_GetCurrentASword(void) {
    u8 aButton = gSaveContext.equips.buttonItems[0];
    if (aButton == ITEM_SWORD_KOKIRI || aButton == ITEM_SWORD_MASTER || aButton == ITEM_SWORD_BGS ||
        aButton == ITEM_SWORD_KNIFE) {
        return aButton;
    }
    return ITEM_NONE;
}

void ItemSword_EquipKokiriToA(void) {
    gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KOKIRI;
}

void ItemSword_RestoreA(u8 prevItem) {
    if (prevItem != ITEM_NONE) {
        gSaveContext.equips.buttonItems[0] = prevItem;
    }
}

u8 ItemHeld_IsActive(Player* player, s32 itemAction) {
    return (player->heldItemAction == itemAction);
}

u16 ItemHeld_GetEquippedButton(u8 itemId, PlayState* play) {
    return ItemInput_GetEquippedButton(itemId, play);
}

u8 ItemHeld_IsButtonHeld(u8 itemId, Player* player, PlayState* play) {
    u16 button = ItemInput_GetEquippedButton(itemId, play);
    if (button == 0)
        return 0;
    return (play->state.input[0].cur.button & button) != 0;
}

u8 ItemHeld_IsButtonPressed(u8 itemId, Player* player, PlayState* play) {
    u16 button = ItemInput_GetEquippedButton(itemId, play);
    if (button == 0)
        return 0;
    return (play->state.input[0].press.button & button) != 0;
}
