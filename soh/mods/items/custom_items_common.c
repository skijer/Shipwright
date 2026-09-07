/**
 * custom_items_common.c - Shared state and utilities for custom items
 *
 * Contains:
 *   - Global CustomItemState struct instance
 *   - Common utility functions used by multiple items
 *   - Frame update handlers for active items
 */

#include "custom_items.h"
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include <math.h>
#include "helpers/fx_helper.h"
#include "helpers/camera_helper.h"
#include "logic/item_postman_hat.h"
#include "../extended_inventory.h" // ExtInv_GetItemSlot — custom items must NOT use vanilla SLOT()/INV_CONTENT()
#include "overlays/actors/ovl_En_Boom/z_en_boom.h" // EnBoom struct for Gale Boomerang multi-target override
#include "soh/FleetShipCombo/FleetShipCombo.h"     // cross-game world-connector (loading zone)
#include "soh/FleetShipCombo/FleetSync.h"          // cross-game save cache + fleet-hole registry

extern PlayState* gPlayState;

// Forward declarations for items included after this file in unity build
extern void Handle_Pokeball(Player* p, PlayState* play);

// Global custom items state
CustomItemState gCustomItemState = { .timer1 = 0,
                                     .timer2 = 0,
                                     .globalCooldownTimer = 0,
                                     .rocsFeatherJumpActive = 0,
                                     .rocsJumpCount = 0,
                                     .dekuLeafActive = 0,
                                     .dekuLeafMode = 0,
                                     .dekuLeafGliding = 0,
                                     .dekuLeafBlowing = 0,
                                     .dekuLeafAnimTimer = 0,
                                     .dekuLeafBlowTimer = 0,
                                     .ballAndChainThrown = 0,
                                     .ballAndChainFirstPersonActive = 0,
                                     .spinnerActive = 0,
                                     .spinnerSpinAttackTimer = 0,
                                     .spinnerWallBumpTimer = 0,
                                     .gustJarEquipped = 0,
                                     .gustJarMode = 0,
                                     .gustJarElement = 0,
                                     .gustJarBlowActive = 0,
                                     .gustJarHeatTimer = 0,
                                     .gustJarBlowTimer = 0,
                                     .gustJarCooldownTimer = 0,
                                     .gustJarTimer = 0,
                                     .gustJarFirstPersonActive = 0,
                                     .gustJarAimMode = 0,
                                     .gustJarPrevCameraMode = 0,
                                     .gustJarButtonMask = 0,
                                     .gustJarPrevInvincibility = 0,
                                     .gustJarPotActor = NULL,
                                     .shovelActive = 0,
                                     .shovelAnimating = 0,
                                     .shovelAnimTimer = 0,
                                     .shovelHoleActor = NULL,
                                     .demiseDestructionActive = 0,
                                     .beetleActive = 0,
                                     .beetleState = 0,
                                     .beetleFirstPersonActive = 0,
                                     .beetlePos = { 0, 0, 0 },
                                     .beetleRot = { 0, 0, 0 },
                                     .beetleGrabbed = NULL,
                                     .beetleWingScale = 1.0f,
                                     .beetleWingDir = -1,
                                     .beetleTimer = 0,
                                     .beetleStartPos = { 0, 0, 0 },
                                     .beetleSubCamId = SUBCAM_FREE,
                                     .bombArrowActive = 0,
                                     .bombArrowState = 0,
                                     .bombArrowBombActor = NULL,
                                     .bombArrowArrowActor = NULL,
                                     .bombArrowFirstPersonActive = 0,
                                     .bombArrowButtonMask = 0,
                                     .fireRodActive = 0,
                                     .fireRodState = 0,
                                     .fireRodPrevSword = 0,
                                     .fireRodMatrixValid = 0,
                                     .fireRodProjActive = 0,
                                     .fireRodProjCount = 0,
                                     .fireRodFlameActive = 0,
                                     .fireRodFlameTimer = 0,
                                     .fireRodFirstPerson = 0,
                                     .fireRodButtonMask = 0,
                                     // Ice Rod
                                     .iceRodActive = 0,
                                     .iceRodState = 0,
                                     .iceRodPrevSword = 0,
                                     .iceRodMatrixValid = 0,
                                     .iceRodProjActive = 0,
                                     .iceRodProjCount = 0,
                                     .iceRodWaveActive = 0,
                                     .iceRodWaveTimer = 0,
                                     .iceRodFirstPerson = 0,
                                     .iceRodButtonMask = 0,
                                     // Light Rod
                                     .lightRodActive = 0,
                                     .lightRodState = 0,
                                     .lightRodPrevSword = 0,
                                     .lightRodMatrixValid = 0,
                                     .lightRodProjActive = 0,
                                     .lightRodProjCount = 0,
                                     .lightRodBeamActive = 0,
                                     .lightRodBeamTimer = 0,
                                     .lightRodFirstPerson = 0,
                                     .lightRodButtonMask = 0,
                                     // Dominion Rod
                                     .dominionRodActive = 0,
                                     .dominionRodState = 0,
                                     .dominionRodFirstPersonActive = 0,
                                     .dominionRodOrbPos = { 0, 0, 0 },
                                     .dominionRodOrbRot = { 0, 0, 0 },
                                     .dominionRodControlledActor = NULL,
                                     .dominionRodTimer = 0,
                                     .dominionRodStartPos = { 0, 0, 0 },
                                     .dominionRodLightNode = NULL,
                                     .dominionRodButtonMask = 0,
                                     .dominionRodControlType = 0,
                                     .dominionRodControlVel = { 0, 0, 0 },
                                     .dominionRodDamagePaused = 0,
                                     .dominionRodPrevInvincibility = 0,
                                     // Dominion Rod Actor-Specific
                                     .dominionRodActorHomePos = { 0, 0, 0 },
                                     .dominionRodFlameTimer = 0,
                                     .dominionRodAttackCooldown = 0,
                                     .dominionRodSpikeInvulnerable = 0,
                                     .dominionRodFlameActor = NULL,
                                     // Cane of Somaria
                                     .somariaActive = 0,
                                     .somariaBlocks = { NULL, NULL, NULL },
                                     .somariaBlockCount = 0,
                                     .somariaOldestSlot = 0,
                                     .somariaButtonMask = 0,
                                     .somariaCooldown = 0,
                                     .somariaSelectedType = 0,
                                     .somariaAnimating = 0,
                                     .somariaAnimTimer = 0,
                                     .somariaActionType = 0,
                                     // Hylia's Grace
                                     .hyliasGraceActive = 0,
                                     .hyliasGraceState = 0,
                                     .hyliasGraceSubPhase = 0,
                                     .hyliasGraceTimer = 0,
                                     .hyliasGraceCooldown = 0,
                                     .hyliasGraceFairy = NULL,
                                     // Zonai Permafrost
                                     .zonaiPermafrostActive = 0,
                                     .zonaiPermafrostState = 0,
                                     .zonaiPermafrostSubPhase = 0,
                                     .zonaiPermafrostTimer = 0,
                                     .zonaiPermafrostSavedTimeIncr = 0,
                                     // Time Gate
                                     .timeGateActive = 0,
                                     .timeGateState = 0,
                                     .timeGateSubPhase = 0,
                                     .timeGateTimer = 0,
                                     .timeGatePromptShown = 0,
                                     .timeGateItemVisible = 0,
                                     .timeGatePortalActive = 0,
                                     .timeGatePortalAlpha = 0.0f,
                                     .timeGatePortalScale = 0.0f,
                                     // Mogma Mitts
                                     .mogmaMittsActive = 0,
                                     .mogmaMittsDrainTick = 0,
                                     // Whip
                                     .whipActive = 0,
                                     .whipState = 0,
                                     .whipTipPos = { 0, 0, 0 },
                                     .whipAttachPos = { 0, 0, 0 },
                                     .whipAttachNormal = { 0, 0, 0 },
                                     .whipTimer = 0,
                                     .whipSwingAngle = 0.0f,
                                     .whipSwingVel = 0.0f,
                                     .whipSwingYaw = 0,
                                     .whipRopeLength = 0.0f,
                                     .whipAttachedBgId = 0,
                                     .whipPullTarget = NULL,
                                     .whipRageTarget = NULL,
                                     .whipRageTimer = 0,
                                     .whipRageOrigSpeed = 0.0f,
                                     .whipPrevInvinc = 0,
                                     .whipExtendYaw = 0,
                                     .whipExtendPitch = 0,
                                     .whipFirstPersonActive = 0,
                                     // Switch Hook
                                     .switchHookActive = 0,
                                     .switchHookState = 0,
                                     .switchHookFirstPerson = 0,
                                     .switchHookProjPos = { 0, 0, 0 },
                                     .switchHookProjYaw = 0,
                                     .switchHookProjPitch = 0,
                                     .switchHookTimer = 0,
                                     .switchHookTarget = NULL,
                                     .switchHookLinkStartPos = { 0, 0, 0 },
                                     .switchHookTargetStartPos = { 0, 0, 0 },
                                     .switchHookSwapTimer = 0,
                                     .switchHookButtonMask = 0,
                                     .switchHookVortexTimer = 0,
                                     .sharedProjectilePos = { 0, 0, 0 } };

s32 CustomItems_IsBlocked(Player* p, PlayState* play) {
    if (p == NULL || play == NULL)
        return true;
    if (p->stateFlags1 & CUSTOM_BLOCKING_STATE1_FLAGS)
        return true;
    if (p->csAction != 0)
        return true;
    if (play->transitionTrigger == TRANS_TRIGGER_START)
        return true;
    if (p->stateFlags3 & PLAYER_STATE3_FLYING_WITH_HOOKSHOT)
        return true;
    return false;
}

// Quick check if item is in any C-button slot
static u8 IsItemEquipped(u8 itemId) {
    // i < 8: buttonItems is u8[8]; `i <= 8` read one past it and could report a false positive.
    for (u8 i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
        if (gSaveContext.equips.buttonItems[i] == itemId)
            return 1;
    }
    return 0;
}

// Cleanup items that were unequipped while active - only runs if needed
static void CustomItems_CleanupUnequipped(Player* p, PlayState* play) {
    if (gCustomItemState.spinnerActive && !IsItemEquipped(ITEM_SPINNER))
        Handle_Spinner(p, play);
    if (gCustomItemState.gustJarEquipped && !IsItemEquipped(ITEM_GUST_JAR))
        Handle_GustJar(p, play);
    if (gCustomItemState.ballAndChainThrown && !IsItemEquipped(ITEM_BALL_AND_CHAIN))
        Handle_BallAndChain(p, play);
    if (gCustomItemState.shovelActive && !IsItemEquipped(ITEM_SHOVEL))
        Handle_Shovel(p, play);
    if (gCustomItemState.demiseDestructionActive && !IsItemEquipped(ITEM_DEMISE_DESTRUCTION))
        Handle_DemiseDestruction(p, play);
    if (gCustomItemState.dekuLeafGliding && !IsItemEquipped(ITEM_DEKU_LEAF))
        Handle_DekuLeaf(p, play);
    if (gCustomItemState.beetleActive && !IsItemEquipped(ITEM_BEETLE))
        Handle_Beetle(p, play);
    // Skijer's NEI — Bomb Arrows is the bow's element flag, not an item on a button, so the literal
    // IsItemEquipped(ITEM_BOMB_ARROWS) scan can never hit. Without this it returns false every frame
    // and cancels the aim immediately, which reads as "bomb arrows do nothing".
    if (gCustomItemState.bombArrowActive && !Sw97_BombArrowsOnButton())
        Handle_BombArrows(p, play);
    if (Net_IsActive() && !IsItemEquipped(ITEM_NET))
        Handle_Net(p, play);
    if ((gCustomItemState.fireRodActive || gCustomItemState.fireRodFirstPerson) && !IsItemEquipped(ITEM_ROD_FIRE))
        Handle_FireRod(p, play);
    if ((gCustomItemState.iceRodActive || gCustomItemState.iceRodFirstPerson) && !IsItemEquipped(ITEM_ROD_ICE))
        Handle_IceRod(p, play);
    if ((gCustomItemState.lightRodActive || gCustomItemState.lightRodFirstPerson) && !IsItemEquipped(ITEM_ROD_LIGHT))
        Handle_LightRod(p, play);
    if (gCustomItemState.dominionRodActive && !IsItemEquipped(ITEM_DOMINION_ROD))
        Handle_DominionRod(p, play);
    if (gCustomItemState.somariaActive && !IsItemEquipped(ITEM_CANE_OF_SOMARIA))
        Handle_CaneOfSomaria(p, play);
    if (gCustomItemState.hyliasGraceActive && !IsItemEquipped(ITEM_HYLIAS_GRACE))
        Handle_HyliasGrace(p, play);
    if (gCustomItemState.zonaiPermafrostActive && !IsItemEquipped(ITEM_ZONAI_PERMAFROST))
        Handle_ZonaiPermafrost(p, play);
    if (gCustomItemState.timeGateActive && !IsItemEquipped(ITEM_TIME_GATE))
        Handle_TimeGate(p, play);
    if (gCustomItemState.mogmaMittsActive && !IsItemEquipped(ITEM_MOGMA_MITTS))
        Handle_MogmaMitts(p, play);
    if (gCustomItemState.whipActive && !IsItemEquipped(ITEM_WHIP))
        Handle_Whip(p, play);
    if (gCustomItemState.switchHookActive && !IsItemEquipped(ITEM_SWITCH_HOOK))
        Handle_SwitchHook(p, play);
}

// ============================================================================
// Fleet Ship Combo — cross-game WORLD CONNECTOR (loading zone), OoT side.
// Door: OoT Lost Woods (0x5B) near (772,0,322)  <->  MM Lost Woods Intro (0x65) near (-1092,0,487).
// Runs every frame from CustomItems_Update. Two jobs:
//  (1) ACTIVATION: when OoT just became the active game with a warp addressed to it, drop Link at
//      the target spot (seamless teleport-in-place: keep facing/motion, no fade).
//  (2) TRIGGER: near our door, CANCEL the vanilla Lost Woods exit (its collision-poly trigger is
//      baked in the binary scene and can't be edited/out-sized) and FLIP to MM instead.
// ============================================================================
static u8 sFleetWarpArmed = 0;
static u8 sTotHoleArmed = 0;     // Temple of Time fleet-hole proximity arm (re-arms when Link steps off)
static u8 sFlipPending = 0;      // OoT->MM: a manual fade-out overlay is ramping; flip to MM at full black
static s16 sSendAlpha = 0;       // 0..255 ramp for the sending fade overlay (drawn by the PiP consumer)
static s16 sWarpCooldown = 0;    // suppress the trigger right after any warp (bridges the scene reload)
static s16 sGuestWaitFrames = 0; // frames held at full black waiting for a quiet 2ship to come back
// Destination of the sending fade (set by whichever trigger started it; consumed at full black).
static int sSendScene = 0x65;
static float sSendX = 0.0f, sSendY = 0.0f, sSendZ = 0.0f;
static int sSendRotY = 0;

// One ramp step of the sending fade + the flip at full black. Split out of FleetWarp_Tick because
// that tick only runs from the PLAYER update: anything that stops the player from updating
// (cutscene, textbox, death, a paused/stalled state) also stops the fade — leaving the screen
// permanently black at whatever alpha it reached, with the flip never requested. That is a frozen
// game. FleetWarp_SendFadeWatchdog below drives this same function from the global per-frame hook
// when it sees the ramp stall.
static void FleetWarp_RampSendFade(PlayState* play) {
    if (!sFlipPending) {
        return;
    }
    if (play != NULL && play->transitionMode == TRANS_MODE_OFF) {
        play->transitionTrigger = TRANS_TRIGGER_OFF; // squash before the FSM picks it up
    }
    sSendAlpha += 9; // ~1.5s ramp at the ~20 Hz game-update rate (tune)
    if (sSendAlpha < 255) {
        FleetShipCombo_SetSendFadeAlpha((int)sSendAlpha);
        return;
    }
    sSendAlpha = 255;
    FleetSync_SwapTrace("A1. OoT fade reached full black — starting handover to MM");

    // LAST CHECK BEFORE A ONE-WAY DOOR. The flip makes MM active and freezes OoT; if 2ship is dead
    // or hung at that moment the player is left on a window that will never update again — not a
    // warp bug, but indistinguishable from one, and unrecoverable.
    //
    // But we WAIT before we give up, and that distinction is the whole design. 2ship stops turning
    // frames for entirely normal reasons — a scene load, an oracle turn, a stutter — and the logs
    // show gaps of several seconds during healthy play. Refusing the warp on the first quiet moment
    // would break something that works today. Holding at full black costs the player a slightly
    // longer fade and nothing else, so we hold: if MM comes back (the common case), the warp goes
    // through as usual. Only when it stays silent past the deadline do we call it gone.
    if (!FleetShipCombo_IsGuestResponsive()) {
        // A RESUME hand-off happens seconds after launch, when 2ship may still be reading archives
        // (a first run can take a minute), so it gets a far longer grace than a portal step: the
        // player asked to resume in MM, and giving up on them after ten seconds because the other
        // game is still booting would be the wrong call. A portal step, by contrast, means MM was
        // alive moments ago.
        const s16 waitLimit = (sSendScene == FC_WARP_SCENE_RESUME) ? 3600 : 600; // ~60s vs ~10s
        if (++sGuestWaitFrames < waitLimit) {
            sSendAlpha = 255 - 9; // stay one step short so we re-test instead of flipping
            FleetShipCombo_SetSendFadeAlpha(255);
            return; // sFlipPending stays set: we are still mid-warp, just waiting
        }
        // Really gone. Don't travel: clear the fade, re-arm the trigger, say why, leave the player
        // where they are. The portal works again the moment MM does (and if it is truly dead, the
        // guest watchdog closes the combo on its own).
        sGuestWaitFrames = 0;
        sFlipPending = 0;
        sSendAlpha = 0;
        FleetShipCombo_SetSendFadeAlpha(0); // never leave the host painting black
        FleetShipCombo_ReportGuestUnavailable();
        sWarpCooldown = 120; // don't re-trigger every frame while standing on the portal
        sFleetWarpArmed = 1;
        sTotHoleArmed = 1;
        return;
    }
    sGuestWaitFrames = 0;
    sFlipPending = 0;
    FleetSync_SwapTrace("A2. guest responsive — committing");

    // The flip is UNCONDITIONAL: FleetSync_WriteDeparture is best-effort bookkeeping (it is guarded
    // on its own side and always returns normally), while RequestWarp is the only thing that hands
    // the player to MM. They must never be able to trade places.
    FleetSync_SwapTrace("A3. WriteDeparture enter");
    FleetSync_WriteDeparture(gSaveContext.fileNum); // anchor + shared BEFORE the flip
    FleetSync_SwapTrace("A4. WriteDeparture done — parking in the waiting room");
    // Park first, flip once parked. FleetLimbo_DepartToMm walks Link into the sealed waiting room
    // and FleetWarpBoot_Tick does the RequestWarp the moment he is inside (or after its deadline,
    // which is the old flip-in-place behaviour). OoT keeps RUNNING in there instead of freezing.
    FleetLimbo_DepartToMm(sSendScene, sSendX, sSendY, sSendZ, sSendRotY, gSaveContext.fileNum);
}

// RESUME HAND-OFF: the player loaded a combo file whose last save was made in MM, so OoT is only a
// doorway this time. Start the same sending fade the portal uses — same code path, same departure
// write, same flip — but aimed at FC_WARP_SCENE_RESUME, which tells MM to land in ITS OWN save
// instead of at the Clock Town hole. Called once per file load by FleetWarpBoot.cpp.
// Refused while a warp is already in flight, so it can never stack with a real portal use.
void FleetWarp_StartResumeToMm(void) {
    if (sFlipPending) {
        return;
    }
    sFlipPending = 1;
    sSendAlpha = 0;
    sSendScene = FC_WARP_SCENE_RESUME;
    sSendX = sSendY = sSendZ = 0.0f;
    sSendRotY = 0;
    sFleetWarpArmed = 1; // don't let the Lost Woods door re-trigger on top of this
    sTotHoleArmed = 1;
}

// WATCHDOG (driven by FleetWarpBoot_Tick, which runs in ALL gamestates every frame): if a fade is
// pending but its alpha has not moved for a second, the player update is not running it. Drive it
// from here so the flip still happens. With no PlayState at all there is nothing to fade — flip
// immediately rather than sit on a black screen.
void FleetWarp_SendFadeWatchdog(void) {
    static s16 sLastAlpha = -1;
    static s16 sStalledFrames = 0;

    if (!sFlipPending) {
        sLastAlpha = -1;
        sStalledFrames = 0;
        return;
    }
    if (sSendAlpha != sLastAlpha) {
        sLastAlpha = sSendAlpha;
        sStalledFrames = 0;
        return;
    }
    if (++sStalledFrames < 60) {
        return; // ~1s of no progress before we take over
    }
    sStalledFrames = 0;
    // Don't resume the ramp one step per second — the screen is already dark and whatever the fade
    // was covering is not updating anyway. Jump to full black and let the flip go through.
    sSendAlpha = 255 - 9;
    // While we are the ones driving, a call here stands for a whole second of waiting, not one
    // frame. Without this the guest-wait deadline inside the ramp (counted in frames) would take ten
    // MINUTES to expire in the one case where both the player update AND the guest are stopped.
    if (sGuestWaitFrames > 0) {
        sGuestWaitFrames += 59;
    }
    FleetWarp_RampSendFade(gPlayState); // NULL play is handled: it just skips the trigger squash
}

// Called by the unified arrival pipeline (FleetWarpBoot.cpp) right before it boots the destination
// Play_Init: we just arrived -> don't let the Lost Woods trigger instantly ping-pong back.
void FleetWarp_NotifyArrived(void) {
    sFleetWarpArmed = 1; // Lost Woods door
    sTotHoleArmed = 1;   // ToT hole: Link pops OUT on the spot -> suppress until he steps off
    sWarpCooldown = 40;
}

// ARRIVALS are owned entirely by FleetWarpBoot.cpp (unified pipeline: force-open slot + FleetSync
// overlays + explicit destination overrides + fresh Play_Init). This tick owns only the SENDING
// side: Lost Woods door trigger, fleet-hole fall, and the manual fade-out ramp.
static void FleetWarp_Tick(Player* p, PlayState* play) {
#ifdef COMBO_BUILD
    return; // ComboShip switches games at its scene seams; the manual fade/flip pipeline must stay off
#endif
    if (FleetShipCombo_GetActiveGame() < 0) {
        return; // combo not running -> no-op (standalone OoT unaffected)
    }
    if (sWarpCooldown > 0) {
        sWarpCooldown--;
    }

    // (0) FROZEN GUARD — while we are the INACTIVE game, squash any freshly-set transition trigger
    // (e.g. the hole-fall completion landing AFTER the flip): a frozen game must never start scene
    // transitions on its own. Only the trigger — never a live transition mode.
    // EXCEPTION: our own walk into the waiting room. The hand-over happens the same frame that
    // transition is triggered, so for a frame or two we are inactive with a trigger of our own
    // pending; squashing it would strand the game in its old scene, frozen.
    if (!FleetShipCombo_IsThisGameActive() && play->transitionTrigger != TRANS_TRIGGER_OFF &&
        play->transitionMode == TRANS_MODE_OFF && !FleetLimbo_InFlight()) {
        play->transitionTrigger = TRANS_TRIGGER_OFF;
    }

    // (1) FLEET-HOLE FALL (Temple of Time Door_Ana): falling INTO the hole (z_door_ana.c disabled
    // Link's floor and called FleetSync_OnHoleFall) starts the sending fade. Gated on a real loaded
    // file + normal mode so the title demo can't ghost-flip.
    if (FleetSync_HoleFallPending() && (gSaveContext.fileNum > 2 || gSaveContext.gameMode != GAMEMODE_NORMAL)) {
        FleetSync_ClearHoleFall();
    }
    if (FleetSync_HoleFallPending() && !sFlipPending && sWarpCooldown == 0) {
        FleetSync_ClearHoleFall();
        sFlipPending = 1;
        sSendAlpha = 0;
        FleetSync_BeginSwapTrace("hole fall: OoT -> MM");
        sSendScene = 0x6F; // MM South Clock Town, popping OUT of MM's paired hole
        sSendX = -527.0f;
        sSendY = 100.0f;
        sSendZ = -1173.719f;
        sSendRotY = -16384;
    }

    // (2) SENDING FADE in progress (OoT->MM): no scene transition (that would reload/exit OoT).
    // Each frame: squash a freshly-set exit trigger (mode still OFF -> safe), ramp the black
    // overlay drawn by the host PiP consumer, and FLIP at full black.
    if (sFlipPending) {
        FleetWarp_RampSendFade(play);
        return;
    }

    // (3) LOST WOODS DOOR TRIGGER — when Link reaches the door, start the manual sending fade.
    // Squash the vanilla loading-zone trigger near the door (only while no transition mode runs).
    if (!FleetShipCombo_IsThisGameActive() || play->sceneNum != SCENE_LOST_WOODS) {
        return; // DON'T reset armed here -> survives the scene-reload transition (no re-trigger loop)
    }
    if (gSaveContext.fileNum > 2 || gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return; // no REAL file loaded / title demo -> a ghost flip would stomp the shared state
    }
    Vec3f door = { 772.033f, 0.0f, 322.431f };
    f32 dist = Math_Vec3f_DistXZ(&p->actor.world.pos, &door);
    if (dist < 90.0f) {
        if (play->transitionMode == TRANS_MODE_OFF) {
            play->transitionTrigger = TRANS_TRIGGER_OFF; // squash the vanilla exit -> no reload
        }
        if (!sFleetWarpArmed && sWarpCooldown == 0) {
            sFleetWarpArmed = 1;
            sFlipPending = 1;
            sSendAlpha = 0;
            FleetSync_BeginSwapTrace("Lost Woods door: OoT -> MM");
            sSendScene = 0x65; // MM Lost Woods, landing at the door spot walking out
            sSendX = -1092.578f;
            sSendY = 0.0f;
            sSendZ = 487.082f;
            sSendRotY = 24585;
        }
    } else if (play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF) {
        // Walking around outside the door (stable gameplay) -> re-arm.
        sFleetWarpArmed = 0;
    }
}

void CustomItems_Update(Player* p, PlayState* play) {
    FleetWarp_Tick(p, play); // cross-game loading zone (intercept vanilla exit + flip)

    // Shared world-time arbiter (Champion's Tunic slow-mo, Zonai Permafrost stop, Phantom
    // Hourglass scrub) runs ALWAYS: it is a no-op with no active claim, it re-applies the
    // freeze to actors that spawned mid-effect, and it self-restores on scene change so a
    // held day/night clock can never leak across a load. Skijer's NEI
    TimeCtl_Update(play);

    // Trajectory recorder for the Phantom Hourglass' rewind. Disabled by default
    // (Rewind_SetEnabled), so this is a single compare until that item exists. Must run
    // AFTER TimeCtl_Update so it sees this frame's freeze state. Skijer's NEI
    Rewind_Tick(play);

    // Switch Hook charge regen (Epona-carrot style) runs ALWAYS — even with the hook not in hand
    // or the player blocked — so the 20s/2min timers keep counting. Skijer's NEI
    {
        extern void SwitchHook_ChargeTick(void);
        SwitchHook_ChargeTick();
    }

    // Minish tiny-mode upkeep runs ALWAYS (scene-load auto-reset, per-frame scale
    // guard, shrink/grow animation) — even while blocked or with the cap unequipped
    {
        extern void MinishTiny_Update(Player * p, PlayState * play);
        MinishTiny_Update(p, play);
    }

    // Lantern passive systems run ALWAYS (even when other items block, even when unequipped)
    {
        extern void Lantern_UpdateFlames(PlayState * play);
        extern void Lantern_UpdateBurning(PlayState * play);
        extern void Lantern_UpdateLens(PlayState * play);
        extern void Lantern_UpdatePassive(PlayState * play);
        Lantern_UpdateFlames(play);
        Lantern_UpdateBurning(play);
        Lantern_UpdateLens(play);
        Lantern_UpdatePassive(play); // point light + green-fire regen
    }

    // Bomb-arrows auto-grant. Ownership is a save flag now (bombArrowsOwned) instead of an item in
    // page-2 slot 27 — the slot is the Elemental Wand's. "Bomb Bag" mode latches the flag the moment
    // a bomb bag is owned; the Twilight Upgrade grants it outright. Idempotent.
    {
        extern u8 TwilightUpgrade_HasBombArrows(void);
        u8 bagGrant = (BombArrows_RandoMode() == BOMB_ARROWS_RANDO_BOMB_BAG) && (CUR_UPG_VALUE(UPG_BOMB_BAG) > 0);
        if ((bagGrant || TwilightUpgrade_HasBombArrows()) && !Nei_Save()->bombArrowsOwned) {
            Nei_Save()->bombArrowsOwned = 1; // Skijer's NEI
        }
    }

    // Postman Hat: unlock-on-visit + Mail Dash state machine always run.
    Handle_PostmanHat(p, play);

    // Twilight Upgrade — L-tap toggle for the Clawshot mode. Press L ONLY
    // while the hookshot/longshot is the actually-held item (or actively
    // aiming it). DELIBERATELY narrow: when the player has both hookshot
    // and boomerang equipped on C-buttons, the previous broad fallback
    // (firing on any focusActor != NULL while hookshot was equipped on a
    // C-slot) stole the L press during boomerang aim and prevented the
    // gale multi-target block below from ever receiving it. Now the
    // toggle requires either heldItemAction/Id matching hookshot/longshot,
    // OR ready-to-fire while NOT using the boomerang.
    {
        extern u8 TwilightUpgrade_HasClawshot(void);
        extern u8 TwilightUpgrade_IsClawshotActive(void);
        extern void TwilightUpgrade_SetClawshotActive(u8 active);
        if (TwilightUpgrade_HasClawshot() && CHECK_BTN_ALL(play->state.input[0].press.button, BTN_L) &&
            !(p->stateFlags1 & PLAYER_STATE1_USING_BOOMERANG)) {
            s8 act = p->heldItemAction;
            s16 itemId = p->heldItemId;
            u8 wielding = (act == PLAYER_IA_HOOKSHOT || act == PLAYER_IA_LONGSHOT) ||
                          (itemId == ITEM_HOOKSHOT || itemId == ITEM_LONGSHOT);
            if (wielding) {
                u8 newMode = TwilightUpgrade_IsClawshotActive() ? 0 : 1;
                TwilightUpgrade_SetClawshotActive(newMode);
                // Distinct sound per mode so the player gets audible
                // confirmation of WHICH direction the toggle went.
                Audio_PlaySoundGeneral(newMode ? NA_SE_SY_GET_ITEM : NA_SE_SY_DECIDE, &p->actor.world.pos, 4,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                // Swallow L so the gust-jar / shield / boomerang multi-target
                // block below doesn't also consume the same press.
                play->state.input[0].cur.button &= ~BTN_L;
                play->state.input[0].press.button &= ~BTN_L;
            }
        }
    }

    // Gale Boomerang — multi-target route tracking.
    // Gated on IsGaleBoomerangActive() (the persistent A-toggle in kaleido),
    // not just the upgrade bit. When the player toggles the mode OFF in
    // kaleido, the boomerang behaves vanilla even if they own the upgrade.
    // L tap during aim adds the focusActor to the route (up to 4 points,
    // 500 units max between consecutive points).
    {
        extern u8 TwilightUpgrade_IsGaleBoomerangActive(void);
        u8 galeActive = TwilightUpgrade_IsGaleBoomerangActive();
        u8 isUsingBoomerang = (p->stateFlags1 & PLAYER_STATE1_USING_BOOMERANG) != 0;
        u8 isThrown = (p->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN) != 0;

        if (!galeActive || !isUsingBoomerang) {
            // Reset route when not using boomerang or upgrade isn't owned.
            gCustomItemState.galeBoomerangTargetCount = 0;
            gCustomItemState.galeBoomerangCurrentTargetIdx = 0;
            gCustomItemState.galeBoomerangLockHeld = 0;
            for (u8 i = 0; i < 4; i++) {
                gCustomItemState.galeBoomerangTargets[i] = NULL;
            }
        } else if (!isThrown) {
            // Aim phase — L press adds the focusActor (Z-target) to the route.
            // Debounced so a held L doesn't spam-add.
            u8 lHeld = CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_L) != 0;
            u8 lJustPressed = CHECK_BTN_ALL(play->state.input[0].press.button, BTN_L) != 0;

            // Audible feedback when L is tapped during aim but the player
            // isn't Z-targeting — silent failures were confusing.
            if (lJustPressed && !gCustomItemState.galeBoomerangLockHeld &&
                (p->focusActor == NULL || p->focusActor->update == NULL)) {
                Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                gCustomItemState.galeBoomerangLockHeld = 1; // debounce
            }

            if (lJustPressed && !gCustomItemState.galeBoomerangLockHeld &&
                gCustomItemState.galeBoomerangTargetCount < 4 && p->focusActor != NULL &&
                p->focusActor->update != NULL) {
                gCustomItemState.galeBoomerangLockHeld = 1;

                // Reject if already in the route.
                u8 already = 0;
                for (u8 i = 0; i < gCustomItemState.galeBoomerangTargetCount; i++) {
                    if (gCustomItemState.galeBoomerangTargets[i] == p->focusActor) {
                        already = 1;
                        break;
                    }
                }

                if (!already) {
                    // Distance constraint: new target must be within 500
                    // units of the previous target (or of Link for the
                    // first slot). Z-target lock-on range.
                    Vec3f* prevPos;
                    if (gCustomItemState.galeBoomerangTargetCount == 0) {
                        prevPos = &p->actor.world.pos;
                    } else {
                        prevPos = &gCustomItemState.galeBoomerangTargets[gCustomItemState.galeBoomerangTargetCount - 1]
                                       ->world.pos;
                    }
                    f32 dx = p->focusActor->world.pos.x - prevPos->x;
                    f32 dy = p->focusActor->world.pos.y - prevPos->y;
                    f32 dz = p->focusActor->world.pos.z - prevPos->z;
                    f32 distSq = dx * dx + dy * dy + dz * dz;
                    if (distSq <= (500.0f * 500.0f)) {
                        gCustomItemState.galeBoomerangTargets[gCustomItemState.galeBoomerangTargetCount++] =
                            p->focusActor;
                        Audio_PlaySoundGeneral(NA_SE_SY_LOCK_ON_HUMAN, &p->actor.world.pos, 4,
                                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                               &gSfxDefaultReverb);
                    } else {
                        // Out of chain range — error chirp so the user knows
                        // the press registered but the target was rejected.
                        Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    }
                }
            } else if (!lHeld) {
                gCustomItemState.galeBoomerangLockHeld = 0;
            }
        } else if (isThrown && p->boomerangActor != NULL && p->boomerangActor->update != NULL &&
                   gCustomItemState.galeBoomerangTargetCount > 0) {
            // Flight phase — override En_Boom's moveTo to walk through the route.
            u8 idx = gCustomItemState.galeBoomerangCurrentTargetIdx;
            if (idx < gCustomItemState.galeBoomerangTargetCount) {
                Actor* cur = gCustomItemState.galeBoomerangTargets[idx];
                if (cur == NULL || cur->update == NULL) {
                    // Target died/despawned — skip to next.
                    gCustomItemState.galeBoomerangCurrentTargetIdx++;
                } else {
                    EnBoom* boom = (EnBoom*)p->boomerangActor;
                    boom->moveTo = cur;

                    // Advance when boomerang is within 80 units of the current target.
                    f32 dx = cur->world.pos.x - p->boomerangActor->world.pos.x;
                    f32 dy = cur->world.pos.y - p->boomerangActor->world.pos.y;
                    f32 dz = cur->world.pos.z - p->boomerangActor->world.pos.z;
                    if ((dx * dx + dy * dy + dz * dz) < (80.0f * 80.0f)) {
                        gCustomItemState.galeBoomerangCurrentTargetIdx++;
                    }
                }
            } else {
                // All targets visited — release moveTo so vanilla return logic
                // (returnTimer countdown) brings the boomerang back to Link.
                EnBoom* boom = (EnBoom*)p->boomerangActor;
                boom->moveTo = NULL;
            }
        }
    }

    // Gale Boomerang — B-boost-to-boomerang (TP clawshot-jump style).
    // When the player has the Gale Boomerang mode active AND a boomerang is in
    // flight AND Z-targeting is engaged, pressing B launches Link toward the
    // boomerang's current position with a small upward arc. Lets the player
    // chain mobility off thrown boomerangs.
    {
        extern u8 TwilightUpgrade_IsGaleBoomerangActive(void);
        if (TwilightUpgrade_IsGaleBoomerangActive() && (p->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN) &&
            p->boomerangActor != NULL && p->boomerangActor->update != NULL && Player_IsZTargeting(p) &&
            CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
            // Vector from Link to boomerang
            f32 dx = p->boomerangActor->world.pos.x - p->actor.world.pos.x;
            f32 dy = p->boomerangActor->world.pos.y - p->actor.world.pos.y;
            f32 dz = p->boomerangActor->world.pos.z - p->actor.world.pos.z;
            f32 dist = sqrtf(dx * dx + dy * dy + dz * dz);
            if (dist > 1.0f) {
                f32 speed = 18.0f; // horizontal launch speed
                f32 invNorm = speed / dist;
                p->actor.velocity.x = dx * invNorm;
                p->actor.velocity.z = dz * invNorm;
                p->actor.velocity.y = 8.0f; // upward kick, gravity pulls Link in arc
                // Disable ground flag briefly so velocity actually takes effect.
                p->actor.bgCheckFlags &= ~1;
                Audio_PlaySoundGeneral(NA_SE_PL_SKIP, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
        }
    }

    // Clawshot Bullet Time per-frame update — runs every frame so the slow
    // factor stays applied, exits get detected, joystick aim integrates,
    // and gravity stays suspended while Link is hanging from the anchor.
    // The state machine itself lives in the ClawshotBT_* block below.
    {
        extern void ClawshotBT_Update(Player * player, PlayState * play);
        ClawshotBT_Update(p, play);
    }

    if (gCustomItemState.demiseDestructionActive) {
        Handle_DemiseDestruction(p, play);
        return;
    }

    // Hylia's Grace fairy mode blocks all other custom items
    if (gCustomItemState.hyliasGraceActive) {
        Handle_HyliasGrace(p, play);
        return;
    }

    // Beetle flying blocks all other custom items
    if (gCustomItemState.beetleActive && (gCustomItemState.beetleState == 2 || gCustomItemState.beetleState == 3)) {
        Handle_Beetle(p, play);
        return;
    }

    // Switch Hook blocks all other custom items while active
    if (gCustomItemState.switchHookActive) {
        Handle_SwitchHook(p, play);
        return;
    }

    // Time Gate blocks all other custom items during casting/hovering
    if (gCustomItemState.timeGateActive) {
        Handle_TimeGate(p, play);
        return;
    }

    // Zonai Permafrost is a toggle now: there is no CASTING state, so it is always
    // either idle or ACTIVE and this never swallows the frame. Kept ahead of the
    // IsBlocked check so the freeze can be switched off from states that would
    // otherwise block item input, and the "!= ACTIVE" guard below is what lets the
    // other custom items keep updating while time is stopped.
    if (gCustomItemState.zonaiPermafrostActive) {
        Handle_ZonaiPermafrost(p, play);
        if (gCustomItemState.zonaiPermafrostState != 2 /* ZPERM_STATE_ACTIVE */) {
            return;
        }
    }

    if (CustomItems_IsBlocked(p, play))
        return;

    if (gCustomItemState.globalCooldownTimer > 0)
        gCustomItemState.globalCooldownTimer--;
    if (gCustomItemState.hyliasGraceCooldown > 0)
        gCustomItemState.hyliasGraceCooldown--;

    CustomItems_CleanupUnequipped(p, play);

    // Zora Mask: allow use in water (like custom items, bypasses Player_UseItem water block).
    // Scans all equipped buttons for Zora mask and calls transform handler on press.
    if (p->stateFlags1 & PLAYER_STATE1_IN_WATER) {
        static const u16 sMaskBtns[] = { BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };
        Input* ctrl = &play->state.input[0];
        // The fourth raw-pad scan; same question, same single answer. See equip_helper.c.
        extern u8 ItemInput_ButtonIsClaimed(u16 button);

        for (s32 mi = 0; mi < 7; mi++) {
            if (mi >= 3 && !CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0))
                break;
            if (ItemInput_ButtonIsClaimed(sMaskBtns[mi]))
                continue;
            if (CHECK_BTN_ALL(ctrl->press.button, sMaskBtns[mi])) {
                u8 slot = (mi < 3) ? (mi + 1) : (mi - 3 + 5); // C-buttons: slots 1-3, D-pad: slots 5-8
                u8 maskItem = gSaveContext.equips.buttonItems[slot];
                if (maskItem == ITEM_MM_MASK_ZORA || maskItem == ITEM_MASK_ZORA) {
                    extern void TransformMasks_HandleMaskUse(PlayState*, Player*, s32);
                    TransformMasks_HandleMaskUse(play, p, maskItem);
                    break;
                }
            }
        }
    }

    // Walk every equip slot, including slot 0 = B button. The previous
    // range (1..8) skipped B entirely AND overflowed the 8-element
    // `buttonItems` array at index 8 — so custom items like Roc's
    // Feather equipped to B never had their handler dispatched. Fix:
    // i = 0..7 covers B + 3 C-buttons + 4 D-pad slots cleanly.
    for (u8 i = 0; i < 8; i++) {
        u8 item = gSaveContext.equips.buttonItems[i];
        // Skijer's NEI — Bomb Arrows rides the bow's element flag; the button holds ITEM_BOW, which
        // the custom-item range guard below would reject. This clause must therefore sit ABOVE it.
        if (Sw97_IsBowItem(item) && (Sw97_EffectiveElement(0) == SW97_ELEM_BOMB)) {
            Handle_BombArrows(p, play);
            continue;
        }
        // Skijer's NEI — the Net's id (0xF4) sits ABOVE ITEM_POKEBALL (0xB7), so the
        // custom-item range guard below rejects it and the switch is never reached.
        // Same trap as Bomb Arrows above: this clause has to sit before the guard.
        // Without it Handle_Net never ran and the net could not be put away at all.
        if (item == ITEM_NET) {
            Handle_Net(p, play);
            continue;
        }
        if (item < ITEM_ROCS_FEATHER_SKIJER || item > ITEM_POKEBALL)
            continue;

        switch (item) {
            case ITEM_ROCS_FEATHER_SKIJER:
                Handle_RocsFeather(p, play);
                break;
            case ITEM_ROCS_CAPE:
                Handle_RocsCape(p, play);
                break;
            case ITEM_DEKU_LEAF:
                Handle_DekuLeaf(p, play);
                break;
            case ITEM_SPINNER:
                Handle_Spinner(p, play);
                break;
            case ITEM_GUST_JAR:
                Handle_GustJar(p, play);
                break;
            case ITEM_BALL_AND_CHAIN:
                Handle_BallAndChain(p, play);
                break;
            case ITEM_SHOVEL:
                Handle_Shovel(p, play);
                break;
            case ITEM_DEMISE_DESTRUCTION:
                Handle_DemiseDestruction(p, play);
                break;
            case ITEM_BEETLE:
                Handle_Beetle(p, play);
                break;
            // (ITEM_BOMB_ARROWS case removed — it can no longer sit on a button; see the flag
            // clause above the range guard.)
            case ITEM_ROD_FIRE:
                Handle_FireRod(p, play);
                break;
            case ITEM_ROD_ICE:
                Handle_IceRod(p, play);
                break;
            case ITEM_ROD_LIGHT:
                Handle_LightRod(p, play);
                break;
            case ITEM_DOMINION_ROD:
                Handle_DominionRod(p, play);
                break;
            case ITEM_CANE_OF_SOMARIA:
                Handle_CaneOfSomaria(p, play);
                break;
            case ITEM_MOGMA_MITTS:
                Handle_MogmaMitts(p, play);
                break;
            case ITEM_HYLIAS_GRACE:
                Handle_HyliasGrace(p, play);
                break;
            case ITEM_ZONAI_PERMAFROST:
                // Only call from switch when idle; early-run handles active states
                if (!gCustomItemState.zonaiPermafrostActive)
                    Handle_ZonaiPermafrost(p, play);
                break;
            case ITEM_TIME_GATE:
                Handle_TimeGate(p, play);
                break;
            case ITEM_WHIP:
                Handle_Whip(p, play);
                break;
            case ITEM_SWITCH_HOOK:
                Handle_SwitchHook(p, play);
                break;
            case ITEM_MINISH_CAP:
                Handle_MinishCap(p, play);
                break;
            case ITEM_LANTERN:
                Handle_Lantern(p, play);
                break;
            case ITEM_POKEBALL:
                Handle_Pokeball(p, play);
                break;
            default:
                break;
        }
    }

    // Ultrahand assemblies keep their formation ALWAYS, not just while the mode is open.
    // The merged collision is registered on the ROOT, so the engine re-transforms it by the
    // root's SRT every frame no matter what — but the parts' MODELS are drawn at their own
    // world.pos, and only this drives those. Leaving it inside the mode meant that the moment
    // you pressed B, or simply put the cane away, the root kept moving (it falls, it can be
    // pushed) and dragged the whole welded surface with it while every glued piece's model
    // stayed behind: collision in one place, texture in another. Runs last so it sees wherever
    // the root ended up this frame. Skijer's NEI
    {
        // Declared here rather than by including cane_pacci.h, the way SwitchHook_ChargeTick above
        // does it: this file does not otherwise depend on the actor headers.
        extern void Pacci_FuseFollow(PlayState * play);
        // And the fall, which has to survive the cane being put away - see the note on
        // Pacci_UltrahandDropTick. Before the transform, so the parts follow where the root
        // landed this frame rather than where it was last frame.
        extern void Pacci_UltrahandDropTick(PlayState * play);
        // And the floor switch a placed body is holding down, which has to be re-asserted every
        // frame and has to survive the cane being put away - see Pacci_PlacePressTick.
        extern void Pacci_PlacePressTick(PlayState * play);
        Pacci_UltrahandDropTick(play);
        extern void Pacci_BackRiderTick(PlayState * play);
        Pacci_FuseFollow(play);
        Pacci_PlacePressTick(play);
        extern void Pacci_CutTick(PlayState * play);
        Pacci_BackRiderTick(play);
        extern void Pacci_ThrowTick(PlayState * play);
        Pacci_CutTick(play);
        Pacci_ThrowTick(play);
        // Last, so it tears down a frame that everything above has already had its say in.
        extern void Pacci_UhAbortTick(PlayState * play);
        Pacci_UhAbortTick(play);
    }
    // The same job for anything the switch magnet put on a plate — Stasis, mostly. Outside the
    // cane's block on purpose: a switch a frozen block was left standing on has to stay down while
    // you walk off and use the door, and that has nothing to do with what item is in hand.
    {
        extern void SwitchMagnet_PressTick(PlayState * play);

        SwitchMagnet_PressTick(play);
    }
}

static u8 sCloneDraw;

u8 CustomItems_IsCloneDraw(void) {
    return sCloneDraw;
}

// A Four Sword clone mirrors only what Link HOLDS. World- and screen-space objects (the beetle, a
// thrown ball, offer arrows) are single things and stay on him.
void CustomItems_DrawForClone(Player* clone, PlayState* play) {
    sCloneDraw = 1;
    CustomItems_OverrideDraw(clone, play);
    sCloneDraw = 0;
}

s32 CustomItems_OverrideDraw(Player* p, PlayState* play) {
    u8 heldOnly = sCloneDraw;

    // Point the game's NATIVE offer arrow at the beetle's candidate (screen-space, correctly sized —
    // like MM's arrowHoverActor). The lock reticle is driven separately by player->focusActor, set
    // inline in Beetle_StateFlying. No custom world-space DL. Skijer's NEI
    if (!heldOnly) {
        Beetle_DrawOffer(p, play);
    }
    CustomItems_DrawDekuLeaf(p, play);
    CustomItems_DrawSpinner(p, play);
    CustomItems_DrawFireRod(p, play);  // Call unconditionally like spinner
    CustomItems_DrawIceRod(p, play);   // Ice Rod draw
    CustomItems_DrawLightRod(p, play); // Light Rod draw
    // Net: drawn in Player_PostLimbDrawGameplay at PLAYER_LIMB_L_HAND (using the hand-bone matrix so it
    // rolls 1:1 with the sword), NOT here — a post-draw reconstructed matrix could not follow the roll.

    if (gCustomItemState.gustJarMode > 0 || p->heldItemAction == ITEM_GUST_JAR) {
        CustomItems_DrawGustJar(p, play);
    }
    if (gCustomItemState.ballAndChainThrown && !heldOnly) {
        CustomItems_DrawBallChain(p, play);
    }
    if (gCustomItemState.shovelActive || gCustomItemState.shovelAnimating) {
        CustomItems_DrawShovel(p, play);
    }
    if (gCustomItemState.beetleActive && !heldOnly) {
        CustomItems_DrawBeetle(p, play);
    }
    if (gCustomItemState.dominionRodActive) {
        CustomItems_DrawDominionRod(p, play);
    }
    if (gCustomItemState.somariaActive) {
        CustomItems_DrawCaneOfSomaria(p, play);
    }
    // Slate, Rod of Seasons and Elemental Wand: their equip flags live in their own item TUs (no
    // gCustomItemState entry), so each draw gates itself. Skijer's NEI
    {
        extern void CustomItems_DrawSheikahSlate(Player * player, PlayState * play);
        extern void CustomItems_DrawRodOfSeasons(Player * player, PlayState * play);
        extern void CustomItems_DrawElementalWand(Player * player, PlayState * play);
        extern void ItemEquip_ReleaseHandMatrix(void);
        extern void Stasis_Draw(PlayState * play);
        extern void Hourglass_Draw(PlayState * play);

        CustomItems_DrawSheikahSlate(p, play);
        CustomItems_DrawRodOfSeasons(p, play);
        CustomItems_DrawElementalWand(p, play);
        // After every handheld, never inside one: the hand matrix is shared, and a drawer that
        // declined this frame must not eat it from the one that did not.
        ItemEquip_ReleaseHandMatrix();

        Stasis_Draw(play);    // chains + launch arrow on whatever the Stasis rune is holding
        Hourglass_Draw(play); // the path the recall target is about to retrace
    }
    if (gCustomItemState.mogmaMittsActive) {
        CustomItems_DrawMogmaMitts(p, play);
    }
    if (gCustomItemState.whipActive) {
        CustomItems_DrawWhip(p, play);
    }
    if (gCustomItemState.timeGateActive) {
        CustomItems_DrawTimeGate(p, play);
        CustomItems_DrawTimeGatePortal(p, play);
    }
    if (gCustomItemState.switchHookActive) {
        CustomItems_DrawSwitchHookInHand(p, play);
        CustomItems_DrawSwitchHook(p, play);
    }
    // Lantern draws in hand ONLY during/after swing AND while lantern is still on a C-button.
    if (gCustomItemState.lanternEquipped || gCustomItemState.lanternSwinging) {
        // Check if lantern is still on any C-button
        u8 lanternOnC = 0;
        for (u8 btn = 1; btn < ARRAY_COUNT(gSaveContext.equips.buttonItems); btn++) { // was <= 8, one past the end
            if (gSaveContext.equips.buttonItems[btn] == ITEM_LANTERN) {
                lanternOnC = 1;
                break;
            }
        }
        if (!lanternOnC) {
            // Removed from C-buttons — force hide
            gCustomItemState.lanternEquipped = 0;
            gCustomItemState.lanternSwinging = 0;
        } else {
            s32 heldIA = p->heldItemAction;
            if (heldIA == PLAYER_IA_NONE || heldIA == PLAYER_IA_LANTERN) {
                CustomItems_DrawLantern(p, play);
            } else {
                gCustomItemState.lanternEquipped = 0;
            }
        }
    }

    // Draw reticle for items using first-person aiming mode
    // Color scheme: RED = expel/attack, BLUE = pull/suck, GREEN = control

    // Bomb Arrows - RED (expel)
    if (gCustomItemState.bombArrowActive && gCustomItemState.bombArrowFirstPersonActive) {
        FirstPerson_DrawReticle(p, play, 0.0f, 255, 0, 0);
    }
    // Gust Jar - BLUE when absorbing, element color when blowing, WHITE when idle
    if (gCustomItemState.gustJarFirstPersonActive) {
        if (gCustomItemState.gustJarMode == 2) { // GUST_MODE_ABSORB
            FirstPerson_DrawReticle(p, play, 0.0f, 0, 100, 255);
        } else if (gCustomItemState.gustJarMode == 3) { // GUST_MODE_BLOW
            FirstPerson_DrawReticle(p, play, 0.0f, 255, 100, 0);
        } else {
            FirstPerson_DrawReticle(p, play, 0.0f, 200, 200, 200);
        }
    }
    // Ball and Chain - RED (expel)
    if (gCustomItemState.ballAndChainFirstPersonActive) {
        FirstPerson_DrawReticle(p, play, 0.0f, 255, 0, 0);
    }
    // Beetle - GREEN (control) - only during aiming state
    if (gCustomItemState.beetleFirstPersonActive && gCustomItemState.beetleState == 1) {
        FirstPerson_DrawReticle(p, play, 0.0f, 0, 255, 0);
    }
    // Fire Rod - RED (expel)
    if (gCustomItemState.fireRodFirstPerson) {
        FirstPerson_DrawReticle(p, play, 0.0f, 255, 0, 0);
    }
    // Ice Rod - RED (expel)
    if (gCustomItemState.iceRodFirstPerson) {
        FirstPerson_DrawReticle(p, play, 0.0f, 255, 0, 0);
    }
    // Light Rod - RED (expel)
    if (gCustomItemState.lightRodFirstPerson) {
        FirstPerson_DrawReticle(p, play, 0.0f, 255, 0, 0);
    }
    // Whip - RED (expel)
    if (gCustomItemState.whipFirstPersonActive) {
        FirstPerson_DrawReticle(p, play, 0.0f, 255, 0, 0);
    }
    // Dominion Rod - GREEN (control)
    if (gCustomItemState.dominionRodFirstPersonActive) {
        FirstPerson_DrawReticle(p, play, 0.0f, 0, 255, 0);
    }
    // Switch Hook - BLUE (pull/swap)
    if (gCustomItemState.switchHookFirstPerson) {
        FirstPerson_DrawReticle(p, play, 0.0f, 0, 100, 255);
    }

    return 0;
}

// =============================================================================
// Clawshot Hang (Twilight Upgrade)
// =============================================================================
// When the player lands a clawshot on a hookshot SURFACE (wall/ceiling — NOT
// an enemy or actor), Link is pinned in place at the impact point so he can
// re-aim and fire another clawshot from there. NO slow motion, NO Z-target
// requirement — Z-targeting was suspending Link unintentionally. Instead we
// just keep Link's physics frozen (zero gravity, zero velocity, ground flag
// forced) so the game treats him as standing on solid ground — that is the
// "invisible platform" the player requested, implemented via physics flags
// rather than a separate collision actor.
//
// Wall hit:    Link rotates so his back is to the wall (TP side grapple).
// Ceiling hit: Link drops ~70u so he hangs below the anchor (TP ceiling
//              hang, hands gripping the target above).
//
// Exit:
//  - A press (drop and fall normally — normal physics resume next frame).
//  - Firing another hookshot/clawshot (ArmsHook_Wait → Shoot calls
//    ClawshotBT_NoteShotFired which clears the hang flag; if the new shot
//    lands on another hookshot surface, the hang re-enters on arrival).
//  - Cutscene / damage / loading / etc. (safety unhook).
//
// Enemy pulls (the BUMP_HOOKABLE-bypass branch above) DON'T trigger the
// hang. Only surface hits do. Surface kind is set externally by
// z_arms_hook.c via ClawshotBT_NoteHit*.

// Surface kind detected at hit time — drives where Link hangs and how he
// faces during bullet time. Wall = Link's back glued to the wall (TP side
// grapple); Ceiling = Link dangles ~70u below the anchor (TP ceiling
// hang); else = stick at hook pos with no rotation adjustment.
typedef enum {
    CLAWSHOT_BT_HIT_NONE = 0,
    CLAWSHOT_BT_HIT_WALL,
    CLAWSHOT_BT_HIT_CEILING,
    CLAWSHOT_BT_HIT_OTHER,
} ClawshotBTHitKind;

static u8 sClawshotBTActive = 0;
static u8 sClawshotBTLastHitKind = CLAWSHOT_BT_HIT_NONE;
static Vec3f sClawshotBTLastHitNormal = { 0.0f, 0.0f, 0.0f };
static s16 sClawshotBTLockedYaw = 0;
static Vec3f sClawshotBTAnchorPos = { 0.0f, 0.0f, 0.0f };

// TP ceiling hang: Link's hands grip the anchor, body dangles below. ~70u
// is roughly Link's torso+upper-body height (matches the visual where his
// head sits below the anchor with arms extended up).
#define CLAWSHOT_BT_CEILING_DROP 70.0f

// z_arms_hook.c calls these from the surface- and actor-hit branches so we
// can discriminate when arrival triggers bullet time. Surface variant takes
// the surface normal (XYZ) so we can detect wall (|nY|<0.5) vs ceiling
// (nY<-0.5) and orient Link properly.
void ClawshotBT_NoteHitSurface(f32 nx, f32 ny, f32 nz) {
    sClawshotBTLastHitNormal.x = nx;
    sClawshotBTLastHitNormal.y = ny;
    sClawshotBTLastHitNormal.z = nz;
    if (ny < -0.5f) {
        sClawshotBTLastHitKind = CLAWSHOT_BT_HIT_CEILING;
    } else if (ny > -0.5f && ny < 0.5f) {
        sClawshotBTLastHitKind = CLAWSHOT_BT_HIT_WALL;
    } else {
        // Floor or near-floor — clawshot from above (rare). Treat as a
        // generic anchor; Link stops at hook pos with no special pose.
        sClawshotBTLastHitKind = CLAWSHOT_BT_HIT_OTHER;
    }
}
void ClawshotBT_NoteHitActor(void) {
    sClawshotBTLastHitKind = CLAWSHOT_BT_HIT_NONE;
}
// Called when a new hookshot leaves Link's hand. Cancels any active hang so
// the new shot's vanilla pull isn't fighting against the pin. Gravity will
// self-restore via Player_UpdateCommon next frame.
void ClawshotBT_NoteShotFired(void) {
    sClawshotBTLastHitKind = CLAWSHOT_BT_HIT_NONE;
    sClawshotBTActive = 0;
}

u8 ClawshotBT_IsActive(void) {
    return sClawshotBTActive;
}

// Called by z_arms_hook.c at the arrival moment (phi_f16 == 0.0f) so we can
// suppress the vanilla -20 velocity.y kick AND enter bullet time when the
// upgrade applies. Returns 1 if bullet time started (caller should skip the
// kick), 0 otherwise.
u8 ClawshotBT_TryStartOnArrival(Player* player, PlayState* play) {
    extern u8 TwilightUpgrade_IsClawshotActive(void);
    if (!TwilightUpgrade_IsClawshotActive())
        return 0;
    if (sClawshotBTLastHitKind == CLAWSHOT_BT_HIT_NONE)
        return 0;
    if (sClawshotBTActive)
        return 1; // already active — still suppress the kick

    sClawshotBTActive = 1;
    sClawshotBTAnchorPos = player->actor.world.pos;

    // Surface-kind-specific positioning + facing:
    //
    //   Wall:    Push Link OUT from the wall by an extra 30u along the surface
    //            normal. Vanilla pull stops Link within ~30u of the hook (which
    //            itself is 10u off the wall) — for thin walls or steep angles
    //            Link can overshoot and end up clipped through the wall geometry.
    //            The extra offset keeps his whole body on the safe side.
    //            Rotate his back into the wall (forward = surface normal).
    //
    //   Ceiling: Drop him CLAWSHOT_BT_CEILING_DROP units below the impact so he
    //            hangs from the anchor TP-style. Facing stays as he was.
    //
    //   Other:   Keep current pose.
    switch (sClawshotBTLastHitKind) {
        case CLAWSHOT_BT_HIT_WALL: {
            sClawshotBTAnchorPos.x += 30.0f * sClawshotBTLastHitNormal.x;
            sClawshotBTAnchorPos.z += 30.0f * sClawshotBTLastHitNormal.z;
            sClawshotBTLockedYaw = Math_Atan2S(sClawshotBTLastHitNormal.z, sClawshotBTLastHitNormal.x);
            break;
        }
        case CLAWSHOT_BT_HIT_CEILING: {
            sClawshotBTAnchorPos.y -= CLAWSHOT_BT_CEILING_DROP;
            sClawshotBTLockedYaw = player->actor.shape.rot.y;
            break;
        }
        default:
            sClawshotBTLockedYaw = player->actor.shape.rot.y;
            break;
    }

    // Stop Link cold, zero gravity, force ground flag, and clear all the
    // airborne / hookshot-falling state. Force his action to Player_Action_Idle
    // so the engine treats him as a stationary grounded player — the aim
    // subsystem (bow/hookshot first-person) only engages from idle-ish actions;
    // FreeFall / HookshotFly etc. refuse to enter aim, which is why pressing
    // the hookshot C-button did nothing while hanging.
    extern void Player_Action_Idle(Player * this, PlayState * play);
    extern s32 Player_SetupAction(PlayState * play, Player * this, PlayerActionFunc actionFunc, s32 flags);

    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    player->actor.gravity = 0.0f;
    player->actor.bgCheckFlags |= 1;
    player->stateFlags1 &= ~(PLAYER_STATE1_HOOKSHOT_FALLING | PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);
    player->stateFlags3 &= ~(PLAYER_STATE3_FLYING_WITH_HOOKSHOT | PLAYER_STATE3_MIDAIR);
    player->actor.world.pos = sClawshotBTAnchorPos;
    player->actor.shape.rot.y = sClawshotBTLockedYaw;
    player->yaw = sClawshotBTLockedYaw;
    // The hookshot reel-in writes a pitch into shape.rot.x to align Link with
    // the chain (Math_Atan2S on bodyDistDiffVec.y vs xz dist at z_arms_hook.c
    // ~line 257) — if the anchor is above Link he ends up tilted upward when
    // the pull completes. Zero ONLY shape.rot.x (the user specifically asked
    // for X only; rot.z left alone). Aim look-up/down lives on upperLimbRot,
    // not the body rotation, so zeroing here is safe.
    player->actor.shape.rot.x = 0;

    // No platform actor spawn — Setting Obj_Hsblock's draw=NULL after spawn
    // didn't actually suppress its rendering (the hookshottable-target square
    // was still visible below Link's feet), and most scenes do have a real
    // scene-collision floor somewhere below the wall anchor, which the
    // bgCheck raycast in Player_ProcessSceneCollision finds. That non-NULL
    // floorPoly is enough for our hook there to safely force the ground flag
    // and unblock the first-person aim subsystem. In pure-pit scenes (no
    // floor at all below Link) aim will still glitch, but at least we don't
    // visually spawn the target actor.

    Audio_PlaySoundGeneral(NA_SE_SY_ATTENTION_ON, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    return 1;
}

static void ClawshotBT_End(Player* player) {
    if (!sClawshotBTActive)
        return;
    sClawshotBTActive = 0;
    // Gravity self-restores via Player_UpdateCommon next frame — no need to
    // pick a value here that might fight whatever state Link transitions to.
}

void ClawshotBT_Update(Player* player, PlayState* play) {
    if (!sClawshotBTActive)
        return;

    Input* input = &play->state.input[0];

    // Exit on A press (drop & fall). Other buttons (B / R / C-buttons /
    // Z toggle) stay LIVE so Link can swing the sword, raise the shield,
    // aim items, change C-button equipment, etc. without losing the hang.
    // The hookshot specifically self-exits via ClawshotBT_NoteShotFired
    // when a new shot leaves Link's hand.
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        ClawshotBT_End(player);
        return;
    }

    // Safety: cutscene / damage / loading / etc.
    u32 blockedFlags = PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                       PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_DAMAGED;
    if (player->stateFlags1 & blockedFlags) {
        ClawshotBT_End(player);
        return;
    }

    // Hang pin: lock position + zero physics every frame so Player_UpdateCommon's
    // velocity writes from joystick / gravity / etc. don't drift Link off the
    // anchor. Forcing bgCheckFlags|=1 plus clearing the airborne stateFlags
    // keeps the engine in "Link is grounded" mode so the hookshot aim subsystem
    // engages from the hanging position.
    //
    // We DON'T re-call Player_SetupAction(Idle) here — Idle naturally handles
    // its own transitions (aim, sword, etc.). If the engine kicks Link out of
    // Idle into FreeFall because the raycast finds no floor (which can happen
    // since we don't have a real platform), we only reassert Idle when it's
    // specifically a falling state — anything else (aim/READY_TO_FIRE,
    // first-person, etc.) is exactly what the player wants and we leave alone.
    player->actor.world.pos = sClawshotBTAnchorPos;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    player->actor.gravity = 0.0f;
    player->actor.bgCheckFlags |= 1;
    player->stateFlags1 &= ~(PLAYER_STATE1_HOOKSHOT_FALLING | PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);
    player->stateFlags3 &= ~(PLAYER_STATE3_FLYING_WITH_HOOKSHOT | PLAYER_STATE3_MIDAIR);

    // For a wall hang, keep Link's back glued to the wall WHILE IDLE — but
    // once he enters first-person aim / ready-to-fire, release the yaw lock
    // so the player can rotate freely to look at new targets. Otherwise the
    // pin glues the body to the original facing and the user can't sweep
    // the camera during aim.
    u8 isAiming = (player->stateFlags1 & (PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_READY_TO_FIRE)) != 0;
    if (sClawshotBTLastHitKind == CLAWSHOT_BT_HIT_WALL && !isAiming) {
        player->actor.shape.rot.y = sClawshotBTLockedYaw;
        player->yaw = sClawshotBTLockedYaw;
    }
    // Keep Link upright across the entire hang — shape.rot.x picks up tilt
    // from the reel-in chain alignment and the engine occasionally re-asserts
    // it during transitions. ONLY rot.x is zeroed (per user request — rot.z
    // stays untouched). Aim look-up/down lives on upperLimbRot, not the body
    // rotation, so zeroing here is safe.
    player->actor.shape.rot.x = 0;
}

// ─────────────────────────────────────────────────────────────────────────
// VisualSync field lists (single source of truth shared by Build & Apply)
//
// These X-macro lists drive both CustomItems_BuildVisualSync (sender) and
// CustomItems_ApplyVisualSync (receiver) so the two directions can no longer
// drift apart. They intentionally do NOT redeclare the CustomItemVisualSync
// struct or the CI_FLAG_* bits — those are referenced by name/value from the
// networking layer (Harpoon.cpp / HarpoonDummyPlayer.cpp) and are kept as the
// authoritative hand-written definitions in custom_items.h.
//
// CI_VISUAL_SCALARS / CI_VISUAL_ARRAYS: plain value copies that are symmetric
// in both directions (Build: out->x = s->x; Apply: s->x = sync->x).
//
// CI_VISUAL_FLAGS(F): one entry per activeFlags bit. Each entry carries its
// complete two-way logic so it stays in sync:
//   F(flag, buildCond, buildExtra, applyStmts)
//     buildCond  — expression; when true the flag bit is OR'd into activeFlags
//     buildExtra — extra Build-only statements (e.g. value copies whose Apply
//                  is gated by the flag); empty (0) when none
//     applyStmts — statements run by Apply for this flag; empty (0) when none
// ─────────────────────────────────────────────────────────────────────────

// clang-format off
#define CI_VISUAL_SCALARS(F)        \
    F(dekuLeafGliding)              \
    F(dekuLeafBlowing)              \
    F(dekuLeafAnimTimer)            \
    F(gustJarElement)               \
    F(gustJarBlowActive)            \
    F(gustJarHeatTimer)             \
    F(timer2)                       \
    F(sharedProjectilePos)          \
    F(beetleState)                  \
    F(beetlePos)                    \
    F(beetleRot)                    \
    F(beetleWingScale)              \
    F(fireRodProjActive)            \
    F(fireRodProjCount)             \
    F(fireRodProjType)              \
    F(fireRodProjPos)               \
    F(fireRodProjPos2)              \
    F(fireRodProjPos3)              \
    F(fireRodProjScale)             \
    F(fireRodMatrix)                \
    F(fireRodMatrixValid)           \
    F(iceRodProjActive)             \
    F(iceRodProjCount)              \
    F(iceRodProjPos)                \
    F(iceRodProjPos2)               \
    F(iceRodProjPos3)               \
    F(iceRodProjScale)              \
    F(iceRodMatrix)                 \
    F(iceRodMatrixValid)            \
    F(lightRodProjActive)           \
    F(lightRodProjCount)            \
    F(lightRodProjPos)              \
    F(lightRodProjPos2)             \
    F(lightRodProjPos3)             \
    F(lightRodMatrix)               \
    F(lightRodMatrixValid)          \
    F(dominionRodState)             \
    F(dominionRodOrbPos)            \
    F(whipState)                    \
    F(whipTipPos)                   \
    F(whipAttachPos)                \
    F(whipAttachNormal)             \
    F(timeGateItemVisible)          \
    F(timeGatePortalActive)         \
    F(timeGatePortalAlpha)          \
    F(timeGatePortalScale)          \
    F(switchHookState)              \
    F(switchHookProjPos)            \
    F(rocsJumpCount)                \
    F(rocsMmAnimTimer)              \
    F(bombArrowState)               \
    F(hyliasGraceState)             \
    F(hyliasGraceSubPhase)          \
    F(hyliasGraceTimer)             \
    F(hyliasGraceForcedBySpell)     \
    F(zonaiPermafrostState)         \
    F(zonaiPermafrostSubPhase)      \
    F(zonaiPermafrostTimer)         \
    F(lanternFireType)              \
    F(lanternSwinging)              \
    F(lanternEquipped)              \
    F(lanternSwingFrame)            \
    F(minishCapWarpMode)            \
    F(minishCapShrinking)           \
    F(minishCapGrowing)             \
    F(postmanHatDashing)            \
    F(postmanHatArriving)           \
    F(postmanHatTransitionTimer)

#define CI_VISUAL_ARRAYS(F)         \
    F(fireRodProjTrail)             \
    F(iceRodProjTrail)

#define CI_VISUAL_FLAGS(F)                                                                                   \
    F(CI_FLAG_SPINNER,            s->spinnerActive,                          0, s->spinnerActive = present;)  \
    F(CI_FLAG_GUSTJAR,            s->gustJarMode > 0,    out->gustJarMode = s->gustJarMode;,                  \
                                                         s->gustJarMode = present ? sync->gustJarMode : 0;)  \
    F(CI_FLAG_BALLCHAIN,          s->ballAndChainThrown, out->ballAndChainThrown = s->ballAndChainThrown;,   \
                                                         s->ballAndChainThrown = present;)                   \
    F(CI_FLAG_SHOVEL,             s->shovelAnimating,    out->shovelAnimating = s->shovelAnimating;,          \
                                                         s->shovelAnimating = present ? sync->shovelAnimating : 0; \
                                                         s->shovelActive = present;)                         \
    F(CI_FLAG_BEETLE,             s->beetleActive,                           0, s->beetleActive = present;)  \
    F(CI_FLAG_DOMINION_ROD,       s->dominionRodActive,                      0, s->dominionRodActive = present;) \
    F(CI_FLAG_SOMARIA,            s->somariaActive,                          0, s->somariaActive = present;) \
    F(CI_FLAG_MOGMA_MITTS,        s->mogmaMittsActive,                       0, s->mogmaMittsActive = present;) \
    F(CI_FLAG_WHIP,               s->whipActive,                             0, s->whipActive = present;)    \
    F(CI_FLAG_TIME_GATE,          s->timeGateActive,                         0, s->timeGateActive = present;) \
    F(CI_FLAG_SWITCH_HOOK,        s->switchHookActive,                       0, s->switchHookActive = present;) \
    F(CI_FLAG_DEKU_LEAF,          s->dekuLeafGliding || s->dekuLeafBlowing,  0, 0)                           \
    F(CI_FLAG_FIRE_ROD,           s->fireRodActive,                          0, s->fireRodActive = present;) \
    F(CI_FLAG_ICE_ROD,            s->iceRodActive,                           0, s->iceRodActive = present;)  \
    F(CI_FLAG_LIGHT_ROD,          s->lightRodActive,                         0, s->lightRodActive = present;) \
    F(CI_FLAG_ROCS_FEATHER,       s->rocsFeatherJumpActive,                                                       \
                                  out->rocsFeatherJumpActive = s->rocsFeatherJumpActive;,                        \
                                                         s->rocsFeatherJumpActive = present;)                    \
    F(CI_FLAG_BOMB_ARROW,         s->bombArrowActive,                        0, s->bombArrowActive = present;) \
    F(CI_FLAG_DEMISE_DESTRUCTION, s->demiseDestructionActive,                0, s->demiseDestructionActive = present;) \
    F(CI_FLAG_HYLIAS_GRACE,       s->hyliasGraceActive,                      0, s->hyliasGraceActive = present;) \
    F(CI_FLAG_ZONAI_PERMAFROST,   s->zonaiPermafrostActive,                  0, s->zonaiPermafrostActive = present;) \
    F(CI_FLAG_LANTERN,            s->lanternEquipped || s->lanternSwinging,  0, 0)                           \
    F(CI_FLAG_MINISH_CAP,         s->minishCapShrinking || s->minishCapGrowing || s->minishCapWarpMode ||    \
                                  s->minishTinyActive || s->minishTinyAnim,  0, 0)                           \
    F(CI_FLAG_POSTMAN_HAT,        s->postmanHatDashing || s->postmanHatArriving, 0, 0)
// clang-format on

void CustomItems_BuildVisualSync(CustomItemVisualSync* out) {
    CustomItemState* s = &gCustomItemState;
    memset(out, 0, sizeof(CustomItemVisualSync));

    // Build active flags bitfield + any flag-gated value copies
    u32 flags = 0;
#define CI_BUILD_FLAG(flag, buildCond, buildExtra, applyStmts) \
    if (buildCond)                                             \
        flags |= (flag);                                       \
    buildExtra;
    CI_VISUAL_FLAGS(CI_BUILD_FLAG)
#undef CI_BUILD_FLAG
    out->activeFlags = flags;

    // Plain symmetric value copies
#define CI_BUILD_SCALAR(name) out->name = s->name;
    CI_VISUAL_SCALARS(CI_BUILD_SCALAR)
#undef CI_BUILD_SCALAR

    // Array copies
#define CI_BUILD_ARRAY(name) memcpy(out->name, s->name, sizeof(s->name));
    CI_VISUAL_ARRAYS(CI_BUILD_ARRAY)
#undef CI_BUILD_ARRAY
}

void CustomItems_ApplyVisualSync(const CustomItemVisualSync* sync) {
    CustomItemState* s = &gCustomItemState;

    // Apply active flags from bitfield (+ flag-gated value restores)
#define CI_APPLY_FLAG(flag, buildCond, buildExtra, applyStmts) \
    {                                                          \
        u32 present = (sync->activeFlags & (flag)) ? 1 : 0;    \
        (void)present;                                         \
        applyStmts;                                            \
    }
    CI_VISUAL_FLAGS(CI_APPLY_FLAG)
#undef CI_APPLY_FLAG

    // Plain symmetric value copies
#define CI_APPLY_SCALAR(name) s->name = sync->name;
    CI_VISUAL_SCALARS(CI_APPLY_SCALAR)
#undef CI_APPLY_SCALAR

    // Array copies
#define CI_APPLY_ARRAY(name) memcpy(s->name, sync->name, sizeof(s->name));
    CI_VISUAL_ARRAYS(CI_APPLY_ARRAY)
#undef CI_APPLY_ARRAY

    // Disable first-person reticles (never draw for remote players)
    s->bombArrowFirstPersonActive = 0;
    s->fireRodFirstPerson = 0;
    s->iceRodFirstPerson = 0;
    s->lightRodFirstPerson = 0;
    s->dominionRodFirstPersonActive = 0;
    s->switchHookFirstPerson = 0;
}
