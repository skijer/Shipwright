/**
 * picto_message.cpp - Pictograph Box "Keep this picture?" textbox (Skijer's NEI).
 *
 * MM shows a 2-choice message right after the shutter (z_parameter.c: Message_StartTextbox 0xF8):
 * Yes / No. SOH has no MM message infra, so we register an OnOpenText hook for a custom textId
 * and build the prompt with the existing NEI custom-message system (CustomMessageManager) — the same
 * pattern as clm_behavior.cpp (Circus Leader's Mask). picto_box.c opens it via
 * Message_StartTextbox(PICTO_KEEP_TEXTID) and reads msgCtx.choiceIndex (0 = keep, !=0 = discard),
 * exactly like MM's PICTO_BOX_STATE_PHOTO handler.
 *
 * 1:1 with 2Ship (Skijer 2026-08-07). The Pictograph Box is an MM item, so MM's version wins on every
 * divergence. What that means here, mirroring 2s2h/Enhancements/Equipment/BetterPictoMessage.cpp:
 *   • Same CVar, same default: gEnhancements.Equipment.BetterPictoMessage, ON. When on, the prompt
 *     names the photographed subject ("Keep this picture of a Pirate?"); when off, the plain MM text.
 *   • Same subject table and same precedence (later checks override earlier; Lulu needs all 3 parts).
 *   • Same choice labels: MM's Yes / No — NOT the old "Keep it / Throw it away".
 *   • No "you already have a picture, replace it?" wording: MM overwrites the photo silently, so that
 *     SOH-only invention (and its unused PICTO_REPLACE_TEXTID prompt) is gone.
 *
 * New .cpp -> add it in the VS Solution Explorer (the CMake mods glob picks up *.cpp).
 */

#include <spdlog/spdlog.h>
#include <libultraship/bridge.h> // CVarGetInteger (gEnhancements.Equipment.BetterPictoMessage)
#include <soh/OTRGlobals.h>
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
#include "mods/items/logic/snap.h" // Snap_CheckFlag + PICTO_VALID_* (the photographed subject)
}

#define PICTO_KEEP_TEXTID 0x6F08 // must match soh/mods/items/logic/snap.h

// Same CVar name and same default (ON) as 2Ship's BetterPictoMessage enhancement, so the option reads
// and behaves identically in both games.
#define BETTER_PICTO_MESSAGE_CVAR "gEnhancements.Equipment.BetterPictoMessage"

// Build MM's "Keep this picture?" 2-choice prompt. TWO_WAY_CHOICE() = \x1B; the two options follow it
// and AutoFormat lays them onto the choice lines. choiceIndex 0 = "Yes" (keep), 1 = "No" (discard) —
// MM's own labels and ordering (z_parameter.c reads choiceIndex the same way).
static void Picto_BuildKeepMessage(uint16_t* textId, bool* loadFromMessageTable) {
    // Name the photographed subject: read the validation flags set at the shutter
    // (Snap_RecordPictographedActors) and pick the target. Later checks override earlier; Lulu needs
    // all three body parts. Table copied verbatim from 2Ship's BetterPictoMessage.
    std::string target;
    if (CVarGetInteger(BETTER_PICTO_MESSAGE_CVAR, 1)) {
        if (Snap_CheckFlag(PICTO_VALID_IN_SWAMP))
            target = "the Swamp";
        if (Snap_CheckFlag(PICTO_VALID_MONKEY))
            target = "a Monkey";
        if (Snap_CheckFlag(PICTO_VALID_BIG_OCTO))
            target = "a Big Octo";
        if (Snap_CheckFlag(PICTO_VALID_LULU_HEAD) && Snap_CheckFlag(PICTO_VALID_LULU_RIGHT_ARM) &&
            Snap_CheckFlag(PICTO_VALID_LULU_LEFT_ARM))
            target = "Lulu";
        if (Snap_CheckFlag(PICTO_VALID_SCARECROW))
            target = "a Scarecrow";
        if (Snap_CheckFlag(PICTO_VALID_TINGLE))
            target = "Tingle";
        if (Snap_CheckFlag(PICTO_VALID_PIRATE_GOOD))
            target = "a Pirate";
        if (Snap_CheckFlag(PICTO_VALID_DEKU_KING))
            target = "the Deku King";
    }

    // With no subject (or the enhancement off) this is MM's plain 0xF8 line — MM never warns about
    // overwriting the stored photo, so neither do we. %r/%w color the prompt, %g the choices.
    std::string question =
        target.empty() ? std::string("Keep this %rpicture%w?") : ("Keep this %rpicture of " + target + "%w?");

    CustomMessage msg = CustomMessage(question + CustomMessage::TWO_WAY_CHOICE() + "%gYes&No");
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

// Gag message when an MM trade-quest item is "used" (trade_items.c present flow) — the classic line.
// Plain single-box message; & = newline.
static void Picto_BuildTradeUseMessage(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg = CustomMessage("Oak's words echoed... There's a time and place for everything, but not now.");
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

static void Picto_RegisterMessageHooks() {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(PICTO_KEEP_TEXTID,
                                                                                Picto_BuildKeepMessage);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(MM_TRADE_USE_TEXTID,
                                                                                Picto_BuildTradeUseMessage);
}

static RegisterShipInitFunc sPictoMessageInit(Picto_RegisterMessageHooks);
