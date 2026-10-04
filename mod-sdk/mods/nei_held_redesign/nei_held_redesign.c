// marsh6487's redesigned held models for Skijer's NEI items, as a separate mod. Models and hand fitting from
// marsh6487's feat/nei-gi-upgrade-recovered-20260927 (bea6f438..c77c1858): soh/assets/custom/objects/nei_held_redesign,
// NeiHeldPresentation.cpp, and the wrist fitting of ItemEquip_ApplyLeftHandPose in equip_helper.c.
//
// Skijer's items are not touched. For the frames a rod or the jar is in hand this mod
//   1. keeps the left-hand matrix of the player draw (OnPlayerPostLimbDraw), as the original did,
//   2. makes the item's own held draw stand down: it draws only while `heldItemAction` is the custom-item action, so
//      the action is set to none for the duration of the player's draw and put back after the world has drawn,
//   3. draws the redesigned model on the captured hand matrix (OnActorDrawEnd).
// With the models absent, the option off or another item in hand, nothing here does anything.
//
// Done: Fire, Ice and Light Rod, Gust Jar. The other held components (Whip's segments, Switch Hook, Beetle, Ball and
// Chain, Shovel, Spinner, Deku Leaf, Cane of Somaria, Mitts) are drawn by their items from internal state (swing,
// charge, extension) that another mod cannot read; the models are already in this package for when they can.
// Not ported: the energy orb at the rod tip and the charge focus effects.

#include "soh/ModApi/ModApi.h"

#include <string.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define HELD_CVAR "gMods.NeiHeldRedesign.Enabled"
#define HELD_ROOT "__OTR__objects/nei_held_redesign/"

static const ALIGN_ASSET(2) char sFireRod[] = HELD_ROOT "fire_rod/gi_dl";
static const ALIGN_ASSET(2) char sIceRod[] = HELD_ROOT "ice_rod/gi_dl";
static const ALIGN_ASSET(2) char sLightRod[] = HELD_ROOT "light_rod/gi_dl";
static const ALIGN_ASSET(2) char sJarBody[] = HELD_ROOT "gust_jar/gi_dl";
static const ALIGN_ASSET(2) char sJarBand[] = HELD_ROOT "gust_jar_band/gi_dl";

typedef enum {
    HELD_NONE,
    HELD_ROD,
    HELD_JAR,
} HeldKind;

typedef struct {
    const char* key;
    HeldKind kind;
    const char* model;
} HeldItem;

static const HeldItem sItems[] = {
    { "nei.rod_fire", HELD_ROD, sFireRod },
    { "nei.rod_ice", HELD_ROD, sIceRod },
    { "nei.rod_light", HELD_ROD, sLightRod },
    { "nei.gust_jar", HELD_JAR, sJarBody },
};

static const char* const sRequiredHooks[] = { "OnPlayerPostLimbDraw", "OnActorDraw", "OnActorDrawEnd",
                                              "OnPlayDrawEnd" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static MtxF sLeftHand;
static bool sLeftHandValid;
static const HeldItem* sActive;
static s8 sSavedAction;
static bool sHidden;

static const HeldItem* FindHeld(Player* player) {
    const char* held;

    if (!CVarGetInteger(HELD_CVAR, 1) || player->heldItemAction != PLAYER_IA_CUSTOM ||
        (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) || (player->stateFlags1 & PLAYER_STATE1_FIRST_PERSON)) {
        return NULL;
    }
    held = CustomItemRegistry_GetHeldKey();
    if (held == NULL) {
        return NULL;
    }
    for (u32 i = 0; i < ARRAY_COUNT(sItems); i++) {
        if (strcmp(held, sItems[i].key) == 0) {
            if (!sApi->HasResource(sItems[i].model) || (sItems[i].kind == HELD_JAR && !sApi->HasResource(sJarBand))) {
                return NULL;
            }
            return &sItems[i];
        }
    }
    return NULL;
}

// Before the player is drawn: pick the item and have its own held draw stand down.
static void BeginPlayerDraw(Actor* actor, PlayState* play, bool* drawVanilla) {
    Player* player = (Player*)actor;

    sLeftHandValid = false;
    sActive = FindHeld(player);
    if (sActive != NULL && !sHidden) {
        sSavedAction = player->heldItemAction;
        player->heldItemAction = PLAYER_IA_NONE;
        sHidden = true;
    }
}

static void CaptureHand(PlayState* play, Player* player, int32_t limbIndex) {
    if (limbIndex == PLAYER_LIMB_L_HAND && player == GET_PLAYER(play) && sActive != NULL) {
        Matrix_Get(&sLeftHand);
        sLeftHandValid = true;
    }
}

static void DrawModel(PlayState* play, const char* model) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)model);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The shaft is exported 32 degrees off the model's +Y: its author axis is turned onto the hand's weapon axis, at the
// palm sockets measured on the adult sword hilt and the child Kokiri Sword grip (the wrist is below the fist).
static void DrawRod(PlayState* play, Player* player, const HeldItem* item) {
    f32 gripY = LINK_IS_CHILD ? 216.22f : 328.0f;
    f32 gripZ = LINK_IS_CHILD ? 4.5f : -77.0f;
    f32 unscale = player->actor.scale.x != 0.0f ? 1.0f / player->actor.scale.x : 1.0f;

    if (!sLeftHandValid) {
        return;
    }
    Matrix_Push();
    Matrix_Put(&sLeftHand);
    Matrix_Scale(unscale, unscale, unscale, MTXMODE_APPLY); // the limb matrix carries Link's 0.01 body scale
    Matrix_Translate(0.0f, gripY * player->actor.scale.x, gripZ * player->actor.scale.x, MTXMODE_APPLY);
    Matrix_RotateZ(DEG_TO_RAD(-122.0f), MTXMODE_APPLY);
    Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);
    DrawModel(play, item->model);
    Matrix_Pop();
}

// Held in both hands, at their midpoint, along the way Link faces.
static void DrawJar(PlayState* play, Player* player, const HeldItem* item) {
    Vec3f left = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    Vec3f right = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];

    Matrix_Push();
    Matrix_Translate((left.x + right.x) * 0.5f, (left.y + right.y) * 0.5f, (left.z + right.z) * 0.5f, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateX(M_PI * 0.5f, MTXMODE_APPLY);
    Matrix_Scale(0.22f, 0.22f, 0.22f, MTXMODE_APPLY);
    DrawModel(play, item->model);
    // The band keeps the primitive colour it is given: the cool neutral of an idle jar.
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 0, 120, 200, 255);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawModel(play, sJarBand);
    Matrix_Pop();
}

static void DrawHeld(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (sActive == NULL || player != GET_PLAYER(play)) {
        return;
    }
    if (sActive->kind == HELD_ROD) {
        DrawRod(play, player, sActive);
    } else if (sActive->kind == HELD_JAR) {
        DrawJar(play, player, sActive);
    }
}

// After the whole world: give the item its action back.
static void EndWorldDraw(void) {
    PlayState* play = gPlayState;

    if (sHidden && play != NULL) {
        GET_PLAYER(play)->heldItemAction = sSavedAction;
    }
    sHidden = false;
    sActive = NULL;
    sLeftHandValid = false;
}

static void RegisterToggle(void) {
    SOHModMenuWidget widget = { sizeof(SOHModMenuWidget) };

    widget.section = "Enhancements";
    widget.sidebar = "Items";
    widget.type = SOH_MOD_MENU_CHECKBOX;
    widget.label = "Redesigned Held Rods and Gust Jar";
    widget.cvar = HELD_CVAR;
    widget.tooltip = "marsh6487's redesigned models for the held Fire, Ice and Light Rods and the Gust Jar.";
    widget.defaultInt = 1;
    sApi->RegisterMenuWidget(&widget);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_PLAYER, BeginPlayerDraw);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, CaptureHand);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawHeld);
    SOH_REGISTER_HOOK(sApi, OnPlayDrawEnd, EndWorldDraw);
    if (SOH_MOD_API_HAS(sApi, RegisterMenuWidget)) {
        RegisterToggle();
    }
}
