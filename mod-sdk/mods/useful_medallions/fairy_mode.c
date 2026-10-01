// The Spirit spell's fairy: Link leaves his body and flies as Ivan (SoH's EnPartner) through walls until the
// Spirit Medallion is pressed again. NEI's Hylia's Grace "Ivan" state, carried here so the mod stands alone.

#include "z64items.h"

#include <string.h>

#include "sw97_mod.h"

#define SPIRIT_KEY "nei.medallion.spirit"
#define TOGGLE_DEBOUNCE_FRAMES 10
#define IVAN_HEIGHT_ABOVE_LINK 5.0f
#define RETURN_INVINCIBILITY 20
#define KEEP_NAVI_OUT 0x100000
#define ITEM_BUTTON_COUNT 8

typedef struct {
    bool isActive;
    s16 timer;
    s8 previousInvincibility;
    Actor* ivan;
    ActorFunc linkDraw;
    s32 savedIvanCoop;
    s32 savedNoClip;
} FairyMode;

extern HOST_DATA s16 gEnPartnerId;

static const u16 sItemButtons[ITEM_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                     BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static FairyMode sFairy;

bool Sw97_IsFairyModeActive(void) {
    return sFairy.isActive;
}

// NEI's possession flag opens the same doors in the engine that Ivan's co-op option does (his hover, his
// boomerang, his damage scale) and grants noclip, so the mod borrows both switches for the flight.
static void BorrowIvanSwitches(void) {
    sFairy.savedIvanCoop = CVarGetInteger(SW97_CVAR_IVAN_COOP, 0);
    sFairy.savedNoClip = CVarGetInteger(SW97_CVAR_NO_CLIP, 0);
    CVarSetInteger(SW97_CVAR_IVAN_COOP, 1);
    CVarSetInteger(SW97_CVAR_NO_CLIP, 1);
}

static void ReturnIvanSwitches(void) {
    CVarSetInteger(SW97_CVAR_IVAN_COOP, sFairy.savedIvanCoop);
    CVarSetInteger(SW97_CVAR_NO_CLIP, sFairy.savedNoClip);
}

void Sw97_StartFairyMode(void) {
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (sFairy.isActive || player == NULL) {
        return;
    }
    memset(&sFairy, 0, sizeof(sFairy));
    sFairy.isActive = true;
    sFairy.previousInvincibility = player->invincibilityTimer;
    BorrowIvanSwitches();
}

static void ResetLighting(PlayState* play) {
    play->envCtx.adjFogNear = 0;
    for (uint8_t i = 0; i < ARRAY_COUNT(play->envCtx.adjFogColor); i++) {
        play->envCtx.adjFogColor[i] = 0;
    }
}

static void KillIvan(void) {
    if (sFairy.ivan != NULL && sFairy.ivan->update != NULL) {
        Actor_Kill(sFairy.ivan);
    }
    sFairy.ivan = NULL;
}

static void RestoreLink(Player* player, PlayState* play) {
    if (sFairy.linkDraw != NULL) {
        player->actor.draw = sFairy.linkDraw;
    }
    player->stateFlags1 &= ~(PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_IN_ITEM_CS);
    player->stateFlags2 &= ~KEEP_NAVI_OUT;
    player->invincibilityTimer = RETURN_INVINCIBILITY;
    ResetLighting(play);
}

static void EndFairyMode(void) {
    KillIvan();
    ReturnIvanSwitches();
    sFairy.isActive = false;
}

// A hit or deep water throws Link back quietly, as HGrace_Stop does.
static void BreakFairyMode(Player* player, PlayState* play) {
    RestoreLink(player, play);
    player->cylinder.base.atFlags |= AT_ON;
    player->cylinder.base.acFlags |= AC_ON;
    player->cylinder.base.ocFlags1 |= OC1_ON;
    func_8005B1A4(Play_GetCamera(play, 0));
    EndFairyMode();
}

static void LeaveFairyMode(Player* player, PlayState* play) {
    RestoreLink(player, play);
    func_800AA000(200.0f, 150, 20, 80);
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_TRIFORCE_FLASH);
    EndFairyMode();
}

static void SpawnIvan(Player* player, PlayState* play) {
    sFairy.ivan = Actor_Spawn(&play->actorCtx, play, gEnPartnerId, player->actor.world.pos.x,
                              player->actor.world.pos.y + Player_GetHeight(player) + IVAN_HEIGHT_ABOVE_LINK,
                              player->actor.world.pos.z, 0, 0, 0, 0);
    sFairy.linkDraw = player->actor.draw;
    player->actor.draw = NULL;
    func_800AA000(200.0f, 150, 20, 80);
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_TRIFORCE_FLASH);
}

// Link stays in the world, invisible and frozen on Ivan, so the camera follows the fairy and a hit still lands.
static void HoldLinkOnIvan(Player* player) {
    player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
    player->linearVelocity = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->actor.velocity.x = player->actor.velocity.y = player->actor.velocity.z = 0.0f;
    player->actor.draw = NULL;
    if (sFairy.ivan == NULL) {
        return;
    }
    player->actor.world.pos = sFairy.ivan->world.pos;
    player->actor.focus.pos = sFairy.ivan->world.pos;
}

static uint16_t FindSpiritButtons(void) {
    uint16_t buttons = 0;

    for (uint8_t button = 0; button < ITEM_BUTTON_COUNT; button++) {
        const char* equipped = gSw97Api->GetEquippedCustomItem(button);

        if (equipped != NULL && strcmp(equipped, SPIRIT_KEY) == 0) {
            buttons |= sItemButtons[button];
        }
    }
    return buttons;
}

static bool WasHit(Player* player) {
    bool wasHit = player->invincibilityTimer > 0 && sFairy.previousInvincibility == 0;

    sFairy.previousInvincibility = player->invincibilityTimer;
    return wasHit;
}

static bool WantsToReturn(PlayState* play) {
    uint16_t buttons = FindSpiritButtons();
    bool isSpiritPressed = buttons != 0 && (play->state.input[0].press.button & buttons);

    return sFairy.timer > TOGGLE_DEBOUNCE_FRAMES && (isSpiritPressed || sFairy.ivan == NULL);
}

static void TickFairyMode(void) {
    PlayState* play = gPlayState;
    Player* player = play == NULL ? NULL : GET_PLAYER(play);

    if (!sFairy.isActive || player == NULL) {
        return;
    }
    if (WasHit(player) || (player->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
        BreakFairyMode(player, play);
        return;
    }
    if (sFairy.ivan == NULL && sFairy.timer == 0) {
        SpawnIvan(player, play);
    }
    HoldLinkOnIvan(player);
    if (sFairy.ivan != NULL && sFairy.ivan->update == NULL) {
        sFairy.ivan = NULL;
    }
    if (WantsToReturn(play)) {
        LeaveFairyMode(player, play);
        return;
    }
    sFairy.timer++;
}

// Leaving the scene ends the flight before the next one loads, or its Play_Init would spawn a co-op Ivan.
static void EndFairyModeWithScene(void) {
    if (!sFairy.isActive) {
        return;
    }
    sFairy.ivan = NULL;
    ReturnIvanSwitches();
    sFairy.isActive = false;
}

void Sw97_RegisterFairyMode(void) {
    SOH_REGISTER_HOOK(gSw97Api, OnPlayerUpdate, TickFairyMode);
    SOH_REGISTER_HOOK(gSw97Api, OnPlayDestroy, EndFairyModeWithScene);
}
