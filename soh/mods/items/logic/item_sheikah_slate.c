/**
 * item_sheikah_slate.c — Sheikah Slate (Skijer's NEI)
 *
 * Five runes share ONE page-2 cell (SLOT_SHEIKAH_SLATE) behind ONE ext item id
 * (EXT_ITEM_SHEIKAH_SLATE). Which rune is live is NeiSaveData.slateMode, cycled by the kaleido
 * wheel; ownership is the NeiSaveData.slateRunesOwned bitmask, one sibling pickup per rune
 * (RG_SLATE_RUNE_* — the wand idiom: gettable in any order, no levels). This file is where the
 * active rune turns into behavior.
 *
 * CONTROLS (BotW)
 * --------------
 *   C (the button holding the slate) : FIRST press draws the slate into Link's hand (equip, the
 *                                      Cane of Somaria idiom); every press after that casts the
 *                                      ACTIVE rune, with the arm extended like a hookshot shot
 *   HOLD L                           : the world pauses and the rune row opens — stick left/right
 *                                      picks, releasing L confirms, B cancels
 *
 * Cryonis is the one rune whose cast does not resolve on the press: it opens an aiming mode that
 * owns the pad until A places the pillar or B backs out (see cryonis_rune.c).
 *
 * The slate has no PlayerItemAction: it lives in the u16 EXT item space, so it rides a C button
 * through the ext-button marker (ITEM_EXT_BUTTON + the parallel u16 store) rather than through
 * equips.buttonItems, and the press is read here in a per-frame tick — the Spiritual Stones idiom —
 * instead of through the player's held-item dispatch.
 *
 * STATUS
 * ------
 * All four runes have real behavior. The shared plumbing — cell ownership, one sibling pickup per
 * rune, the badge on the cell/HUD icon, cycling in the kaleido AND in the hold-L wheel — is live
 * for all of them.
 */

#include "global.h"
#include "mods/extended_inventory.h"         // Slate_GetRune / SLATE_RUNE_*
#include "mods/items/helpers/equip_helper.h" // equip SFX + the shared blocking checks
// box_menu.c is unity-included just before this file in custom_items.c, so its BoxMenu_*
// declarations are already in scope — it has no header (see the note at its top).

// The runes with real behaviour. Included here (not globbed) so they share this translation unit —
// and so they need no header, which would drag 2ship into a CMake regeneration.
#include "../../actors/remote_bomb.c"
#include "../../actors/stasis_rune.c"
#include "../../actors/master_cycle.c"
#include "../../actors/cryonis_rune.c"
#include "../../actors/sensor_rune.c"

extern s32 func_8083485C(Player* this, PlayState* play); // generic "held item" upper action
// Ext-button store: which u16 item a button really holds when it shows ITEM_EXT_BUTTON.
extern u16 ExtButton_GetItem(s32 btn);

/**
 * Per-rune cast. `rune` is a SLATE_RUNE_*; returns 1 if the rune actually fired (so the caller can
 * play cast/error feedback).
 */
s32 Slate_CastRune(Player* player, PlayState* play, u8 rune) {
    switch (rune) {
        case SLATE_RUNE_BOMB:
            return RemoteBomb_Cast(play, player);
        case SLATE_RUNE_STASIS:
            return Stasis_Cast(play, player);
        case SLATE_RUNE_CRYONIS:
            return Cryonis_Cast(play, player);
        case SLATE_RUNE_MASTER_CYCLE:
            return MasterCycle_Cast(play, player);
        case SLATE_RUNE_SENSOR:
            return Sensor_Cast(play, player);
        default:
            break;
    }
    return 0;
}

/**
 * The one climbable-surface question the engine asks (func_80041DB8 in z_bgcheck.c). Two runes make
 * a body climbable and neither may touch the collision headers they borrow, which are shared and
 * cached — both answer by bgId instead, and this is where the two answers meet.
 */
u8 Slate_IsClimbableBgId(s32 bgId) {
    return Stasis_IsClimbableBgId(bgId) || Cryonis_IsClimbableBgId(bgId);
}

/**
 * Per-rune upper action, wand-shaped. Not reachable today (no item action — see header note);
 * wired so a future C-equip only needs a registry row.
 */
s32 Player_UpperAction_SheikahSlate(Player* player, PlayState* play) {
    switch (Slate_GetRune()) {
        case SLATE_RUNE_BOMB:
        case SLATE_RUNE_STASIS:
        case SLATE_RUNE_CRYONIS:
        case SLATE_RUNE_MASTER_CYCLE:
        case SLATE_RUNE_SENSOR:
            // Per-rune held/aim behavior goes here once the casts above exist.
            break;
        default:
            break;
    }

    return func_8083485C(player, play);
}

/**
 * Runs once when the slate becomes the held item. Per-rune setup (aim reticles, target selectors,
 * helper actors) belongs here, keyed the same way as the dispatch above.
 */
void Player_InitSheikahSlateIA(PlayState* play, Player* player) {
    // No per-rune init needed while the behaviors are stubs. Kept as the named entry point so
    // adding a rune is a local change instead of a dispatch change.
    (void)play;
    (void)player;
}

// ============================================================================
// INPUT TICK — C casts, hold L opens the rune row
// ============================================================================

// How long L must be held before the row opens. Short enough to feel instant, long enough that a
// tap still reaches the vanilla Z-target.
#define SLATE_WHEEL_HOLD_FRAMES 8

static s16 sSlateHoldTimer = 0;
static u8 sSlateDrawn = 0;      // the tablet is out, in Link's hand
static s16 sSlateCastTimer = 0; // frames left of the cast pose
static s8 sSlatePrevInvinc = 0;

// How long the arm stays extended on a cast. The hookshot's own shot pose is short and snappy;
// this only has to cover the moment the rune fires.
#define SLATE_CAST_POSE_FRAMES 18

// Is the tablet currently in Link's hand? Read by the in-hand draw (object_sheikah_slate.c).
u8 Slate_IsDrawn(void) {
    return sSlateDrawn;
}

// Put it away. Idempotent, so every blocking path can call it unconditionally.
static void Slate_Stow(PlayState* play, Player* player) {
    if (!sSlateDrawn) {
        return;
    }
    sSlateDrawn = 0;
    sSlateCastTimer = 0;
    ItemEquip_PlayUnequipSFX(play, player);
}

// The cast pose: the hookshot's aim/shot animation on the UPPER body only, so Link keeps walking
// and the tablet — which is drawn off the forearm→hand vector — swings out with the arm.
static void Slate_CastPose(PlayState* play, Player* player) {
    LinkAnimation_PlayOnce(play, &player->upperSkelAnime, &gPlayerAnim_link_hook_shot_ready);
    sSlateCastTimer = SLATE_CAST_POSE_FRAMES;
}

// Every C button currently holding the slate. C items live in the flat button array at 1..3.
// Not static: the Remote Bomb's throw-suppression hook needs it from its own translation unit.
u16 Slate_EquippedButtonMask(void) {
    u16 mask = 0;

    if (ExtButton_GetItem(1) == EXT_ITEM_SHEIKAH_SLATE) {
        mask |= BTN_CLEFT;
    }
    if (ExtButton_GetItem(2) == EXT_ITEM_SHEIKAH_SLATE) {
        mask |= BTN_CDOWN;
    }
    if (ExtButton_GetItem(3) == EXT_ITEM_SHEIKAH_SLATE) {
        mask |= BTN_CRIGHT;
    }
    return mask;
}

static void Slate_OnWheelConfirm(s32 index) {
    Slate_SetRune(Slate_RuneAt((u8)index));
}

// Owned runes only, in the order the kaleido wheel cycles them, so every entry is selectable.
static s32 Slate_BuildWheel(BoxMenuEntry* out) {
    s32 count = Slate_RuneCount();

    for (s32 i = 0; i < count; i++) {
        out[i].iconPath = (const char*)Slate_RuneMiniIcon(Slate_RuneAt((u8)i));
        out[i].iconSize = 32;
        out[i].enabled = 1;
    }
    return count;
}

// A SLATE_RUNE_* value stops matching its row position as soon as one rune is missing.
static s32 Slate_ActiveWheelIndex(void) {
    u8 active = Slate_GetRune();
    s32 count = Slate_RuneCount();

    for (s32 i = 0; i < count; i++) {
        if (Slate_RuneAt((u8)i) == active) {
            return i;
        }
    }
    return 0;
}

/**
 * Per-frame slate input, called from Player_UpdateCommon. Owns nothing else: if the slate is not
 * owned, or is on no button, this is a no-op.
 */
void Slate_TickInput(PlayState* play, Player* player) {
    BoxMenuEntry entries[SLATE_RUNE_COUNT];
    u16 btnMask;
    u16 held;

    // Stasis drives whatever it has frozen every frame, and must keep doing so even with the slate
    // stowed or the menu open — it owns another actor's update until it lets go.
    Stasis_Update(play, player);
    // The bike keeps its own scene-change and transition guards; runs every frame for the same
    // reason Stasis does — it owns another actor.
    MasterCycle_Tick(play, player);
    // Same reason again: the remote bomb's fuse has to be held down every frame it exists.
    RemoteBomb_Tick(play);
    // Cryonis owns a real actor too — this is where it learns the scene took it away.
    Cryonis_Tick(play);

    // Paint what a cast would grab, but only while the tablet is actually out on the Stasis rune —
    // otherwise every actor Link walks past would shimmer. Called every frame either way so the
    // highlight is taken off cleanly the moment any of that stops being true.
    Stasis_UpdateOffer(play, sSlateDrawn && (Slate_GetRune() == SLATE_RUNE_STASIS) && !BoxMenu_IsOpen());

    if (BoxMenu_IsOpen()) {
        return; // the menu owns the frame (and the game is paused anyway)
    }
    if (Nei_Save()->slateRunesOwned == 0) {
        sSlateHoldTimer = 0;
        sSlateDrawn = 0;
        return; // no runes -> no slate powers at all
    }

    btnMask = Slate_EquippedButtonMask();
    held = play->state.input[0].cur.button;

    // Cryonis's aiming mode sits ABOVE the wheel and the blocking checks below, both of which can
    // stow the tablet: with it further down, the A that commits a pillar had already been through
    // the unequip and there was nothing left in Link's hand to cast with.
    if (Cryonis_ModeUpdate(play, player)) {
        return;
    }
    // Same reason: the Sensor's prompt and hint own the frame until the player closes them.
    if (Sensor_Tick(play, player)) {
        return;
    }

    // ---- HOLD L: open the rune row ---------------------------------------
    // Only while the slate is IN HAND. Merely having it on a C button must leave L alone — it is
    // still Z-target for every other item, and stealing it there would break normal play.
    //
    // Read from cur.button, never press: L is Z-target and the player actor consumes its press bit
    // long before item code runs (the same trap the Dual Cane's L/R cycler documents).
    if (sSlateDrawn && (held & BTN_L)) {
        if (sSlateHoldTimer < (SLATE_WHEEL_HOLD_FRAMES + 1)) {
            sSlateHoldTimer++;
        }
        if (sSlateHoldTimer == SLATE_WHEEL_HOLD_FRAMES) {
            s32 count = Slate_BuildWheel(entries);

            BoxMenu_Open(play, entries, count, Slate_ActiveWheelIndex(), BTN_L, Slate_OnWheelConfirm);
        }
        return; // L is ours while it is down
    }
    sSlateHoldTimer = 0;

    // ---- C: first press draws the slate, the rest cast --------------------
    if (btnMask == 0) {
        Slate_Stow(play, player); // taken off the button while it was out
        return;
    }

    // Anything that would look wrong with a tablet in hand puts it away.
    if (ItemInput_IsBlocked(player, play) || (player->stateFlags1 & PLAYER_STATE1_IN_WATER) ||
        (player->meleeWeaponState != 0)) {
        Slate_Stow(play, player);
        return;
    }
    if (ItemInput_CheckDamage(player, &sSlatePrevInvinc)) {
        Slate_Stow(play, player);
        return;
    }

    // Mid-cast: hold the pose and let the animation run out.
    if (sSlateCastTimer > 0) {
        sSlateCastTimer--;
        LinkAnimation_Update(play, &player->upperSkelAnime);
        return;
    }

    if (play->state.input[0].press.button & btnMask) {
        if (!sSlateDrawn) {
            // Equip only — the press that draws the slate never also casts, same as the cane.
            sSlateDrawn = 1;
            ItemEquip_PlayEquipSFX(play, player);
            return;
        }
        if (!Slate_CastRune(player, play, Slate_GetRune())) {
            Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            return;
        }
        // The carry animation owns the upper body when the rune put something in Link's hands, and
        // the cast pose would tear the held actor off them.
        if (!(player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR)) {
            Slate_CastPose(play, player);
        }
    }
}

// The in-hand model. Included here (not globbed) so it shares this translation unit and can read
// the equip state above — the same arrangement the cane uses for its own object file.
#include "../objects/object_sheikah_slate.c"
