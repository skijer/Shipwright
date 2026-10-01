#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"

#define BOMB_ARROWS_KEY "nei.bomb_arrows"
#define BOMB_ARROWS_TRACKED 8
#define BOMB_ARROWS_SPARK_PERIOD 3
#define BOMB_ARROWS_SPARK_SCALE 40
#define BOMB_ARROWS_SPARK_STEP 2
#define BOMB_ARROWS_BOMB_SCALE 0.01f
#define BOMB_ARROWS_ARROW_DAMAGE 0x00000800
#define BOMB_ARROWS_GIVE_SCALE 0.5f

typedef struct {
    Actor* arrow;
    Vec3f lastPos;
} TrackedArrow;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconBombArrowsTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gBombArrowsNameTex";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_bombarrows/gBombarrowsGiveDL";

static const char* const sRequiredHooks[] = { "OnActorInit", "OnActorUpdate", "OnSceneInit",
                                              "OnPlayerResolveItemActionInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static bool sIsArmed;
static TrackedArrow sTrackedArrows[BOMB_ARROWS_TRACKED];

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static TrackedArrow* FindTracked(Actor* arrow) {
    for (u32 i = 0; i < BOMB_ARROWS_TRACKED; i++) {
        if (sTrackedArrows[i].arrow == arrow) {
            return &sTrackedArrows[i];
        }
    }
    return NULL;
}

static void TrackArrow(Actor* arrow) {
    TrackedArrow* slot = FindTracked(NULL);

    if (slot == NULL) {
        slot = &sTrackedArrows[0];
    }
    slot->arrow = arrow;
    slot->lastPos = arrow->world.pos;
}

// Vanilla builds the explosion sphere from the draw, which no longer runs once the bomb turns into its
// blast on the very first update: its centre is placed here instead.
static void BurstAt(PlayState* play, Vec3f* pos) {
    EnBom* bomb = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, pos->x, pos->y, pos->z, 0, 0, 0, BOMB_BODY);

    if (bomb == NULL) {
        return;
    }
    bomb->timer = 1;
    Actor_SetScale(&bomb->actor, BOMB_ARROWS_BOMB_SCALE);
    bomb->explosionCollider.elements[0].dim.worldSphere.center.x = (s16)pos->x;
    bomb->explosionCollider.elements[0].dim.worldSphere.center.y = (s16)pos->y;
    bomb->explosionCollider.elements[0].dim.worldSphere.center.z = (s16)pos->z;
    // Arrow damage on top of the bomb's own, so the blast reaches whatever answers to either.
    bomb->explosionCollider.elements[0].info.toucher.dmgFlags |= BOMB_ARROWS_ARROW_DAMAGE;
}

static void SparkAlongArrow(Actor* arrow, PlayState* play) {
    Color_RGBA8 prim = { 255, 255, 150, 255 };
    Color_RGBA8 env = { 255, 0, 0, 0 };
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    if ((play->gameplayFrames % BOMB_ARROWS_SPARK_PERIOD) != 0) {
        return;
    }
    EffectSsGSpk_SpawnAccel(play, arrow, &arrow->world.pos, &zero, &zero, &prim, &env, BOMB_ARROWS_SPARK_SCALE,
                            BOMB_ARROWS_SPARK_STEP);
    func_8002F974(arrow, NA_SE_IT_BOMB_IGNIT - SFX_FLAG);
}

static bool HasArrowLanded(EnArrow* arrow) {
    return arrow->hitFlags != 0 || (arrow->collider.base.atFlags & AT_HIT);
}

// The hook runs right after the actor's own update and is not gated on it still being alive, which is what
// catches a seed: it kills itself on impact without ever raising a hit flag.
static void FollowArrow(Actor* actor) {
    PlayState* play = gPlayState;
    TrackedArrow* tracked = FindTracked(actor);

    if (play == NULL || tracked == NULL) {
        return;
    }
    if (actor->update == NULL) {
        BurstAt(play, &tracked->lastPos);
        tracked->arrow = NULL;
        return;
    }
    if (HasArrowLanded((EnArrow*)actor)) {
        BurstAt(play, &actor->world.pos);
        tracked->arrow = NULL;
        Actor_Kill(actor);
        return;
    }
    tracked->lastPos = actor->world.pos;
    SparkAlongArrow(actor, play);
}

static void ArmArrow(Actor* actor) {
    PlayState* play = gPlayState;

    if (!sIsArmed || play == NULL || AMMO(ITEM_BOMB) <= 0) {
        return;
    }
    Inventory_ChangeAmmo(ITEM_BOMB, -1);
    TrackArrow(actor);
}

static void ForgetArrows(int16_t sceneNum) {
    memset(sTrackedArrows, 0, sizeof(sTrackedArrows));
}

// A toggle, not a weapon of its own: the bow stays the bow, and every arrow it looses carries a bomb.
static void ToggleBombArrows(PlayState* play, Player* player) {
    sIsArmed = !sIsArmed;
    PlaySfxAt(sIsArmed ? NA_SE_IT_BOMB_IGNIT : NA_SE_IT_SWORD_PUTAWAY, &player->actor.world.pos);
    Sfx_PlaySfxCentered(sIsArmed ? NA_SE_SY_GET_ITEM : NA_SE_SY_CAMERA_ZOOM_DOWN);
}

static bool CanArmBombArrows(Player* player, PlayState* play) {
    return sIsArmed || AMMO(ITEM_BOMB) > 0;
}

// Every shot spends a bomb, so the bombs left are the shots left.
static int32_t GetBombArrowShots(const char* key) {
    return AMMO(ITEM_BOMB);
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(BOMB_ARROWS_GIVE_SCALE, BOMB_ARROWS_GIVE_SCALE, BOMB_ARROWS_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGiveDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    SOHCustomItemDefinition arrows = Z64Items_Define(BOMB_ARROWS_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&arrows, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&arrows, SOH_ITEM_PAGE_VANILLA, SLOT_ARROW_LIGHT, 0);
    Z64Items_SetTextbox(&arrows, "You got the %rBomb Arrows%w!&Powder packed behind the arrowhead.^Press %y\xA1%w to "
                                 "tip your arrows, and&every shot bursts where it lands.^Each one spends a %rbomb%w "
                                 "as well as&an arrow, so keep your bag full.");
    Z64Items_SetPauseText(&arrows, "%rBomb Arrows&%wPress %y\xA1%w to tip your arrows.&Each shot spends a bomb.");
    Z64Items_SetCanUse(&arrows, CanArmBombArrows);
    Z64Items_SetAmmo(&arrows, GetBombArrowShots);
    Z64Items_SetAction(&arrows, ToggleBombArrows, NULL);
    arrows.flags |= SOH_CUSTOM_ITEM_INSTANT;
    arrows.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &arrows)) {
        return;
    }
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_EN_ARROW, ArmArrow);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_ARROW, FollowArrow);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetArrows);
}
