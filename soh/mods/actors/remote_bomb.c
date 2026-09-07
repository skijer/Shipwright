/**
 * remote_bomb.c — Sheikah Slate rune: Remote Bomb (Skijer's NEI)
 *
 * OoT's own En_Bom with the fuse taken away and a cyan tint on top. One press of the slate's button
 * pulls a bomb into Link's hands, the next sets it off — still held, or thrown with A/B.
 *
 * No header on purpose: both repos glob mods/*.h with CONFIGURE_DEPENDS, and a new one there forces
 * a full CMake regeneration. Unity-included into item_sheikah_slate.c, like the other runes.
 */

#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"

extern PlayState* gPlayState;
extern void func_80835688(Player* this, PlayState* play); // start holding the carried actor

s32 RemoteBomb_Cast(PlayState* play, Player* player);
void RemoteBomb_Tick(PlayState* play);
u8 RemoteBomb_IsHeld(void);

// Above BOTH of En_Bom's fuse thresholds: under 63 it sparks and hisses, under 100 it flashes red.
#define REMOTE_BOMB_INERT 200

// The cyan of the rune's own pickup model, so the bomb and the icon that granted it match.
#define REMOTE_BOMB_TINT_R 95
#define REMOTE_BOMB_TINT_G 220
#define REMOTE_BOMB_TINT_B 235
#define REMOTE_BOMB_TINT_MIX 255

static Actor* sRemoteBomb = NULL;
static ActorFunc sRemoteBombDraw = NULL;

// Through the real struct, never a hand-computed offset: the ones written in z_en_bom.h are N64
// offsets, and on 64-bit every pointer ahead of this field is twice as wide.
static s16* RemoteBomb_Fuse(Actor* actor) {
    if ((actor == NULL) || (actor->id != ACTOR_EN_BOM)) {
        return NULL;
    }
    return &((EnBom*)actor)->timer;
}

// The Ultrahand idiom: the actor's own display list run between a grayscale recolour and its reset.
// The engine's colour filter has only white, red and blue, so it cannot do cyan.
static void RemoteBomb_TintDraw(Actor* thisx, PlayState* play) {
    if ((thisx != sRemoteBomb) || (sRemoteBombDraw == NULL)) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, REMOTE_BOMB_TINT_R, REMOTE_BOMB_TINT_G, REMOTE_BOMB_TINT_B,
                         REMOTE_BOMB_TINT_MIX);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, REMOTE_BOMB_TINT_R, REMOTE_BOMB_TINT_G, REMOTE_BOMB_TINT_B,
                         REMOTE_BOMB_TINT_MIX);
    gSPGrayscale(POLY_XLU_DISP++, true);
    CLOSE_DISPS(play->state.gfxCtx);

    sRemoteBombDraw(thisx, play);

    OPEN_DISPS(play->state.gfxCtx);
    // Both buffers, or everything drawn after the bomb this frame comes out cyan too.
    gSPGrayscale(POLY_OPA_DISP++, false);
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RemoteBomb_TintAttach(Actor* actor) {
    if ((actor->draw == NULL) || (actor->draw == RemoteBomb_TintDraw)) {
        return;
    }
    sRemoteBombDraw = actor->draw;
    actor->draw = RemoteBomb_TintDraw;
}

// Stop owning the bomb without writing through the pointer: after a scene change the actor is gone,
// and a dead one may have had its slot recycled.
static void RemoteBomb_Drop(void) {
    sRemoteBomb = NULL;
    sRemoteBombDraw = NULL;
}

// Give the bomb its own draw back, then stop owning it. Only for one still alive this frame.
static void RemoteBomb_Release(void) {
    if ((sRemoteBomb != NULL) && (sRemoteBomb->draw == RemoteBomb_TintDraw)) {
        sRemoteBomb->draw = sRemoteBombDraw;
    }
    RemoteBomb_Drop();
}

// Player_InitExplosiveIA's own sequence, minus the ammo cost — the rune makes its own bombs.
static s32 RemoteBomb_Lift(PlayState* play, Player* player) {
    Actor* bomb;

    if (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
        return 0;
    }

    bomb = Actor_SpawnAsChild(&play->actorCtx, &player->actor, play, ACTOR_EN_BOM, player->actor.world.pos.x,
                              player->actor.world.pos.y, player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0, 0);
    if (bomb == NULL) {
        return 0;
    }

    ((EnBom*)bomb)->timer = REMOTE_BOMB_INERT;
    // EnBom_Init leaves the bomb at scale 0, and the frame that grows it is timer == 67 — which a
    // bomb with no fuse never reaches. Without this it is invisible.
    Actor_SetScale(bomb, 0.01f);

    player->interactRangeActor = bomb;
    player->heldActor = bomb;
    player->getItemId = GI_NONE;
    player->getItemEntry = (GetItemEntry)GET_ITEM_NONE;
    player->unk_3BC.y = bomb->shape.rot.y - player->actor.shape.rot.y;
    player->stateFlags1 |= PLAYER_STATE1_CARRYING_ACTOR;
    // Hoist it overhead like a bomb from the bag. A real bomb gets this pose from the item-action
    // table, which the slate has no row in, so the rune asks for it the way a lifted pot does.
    func_80835688(player, play);

    sRemoteBomb = bomb;
    RemoteBomb_TintAttach(bomb);
    return 1;
}

// En_Bom owns the flash, the sound, the damage, the quake and the debris.
static s32 RemoteBomb_Detonate(void) {
    s16* fuse = RemoteBomb_Fuse(sRemoteBomb);

    if (fuse == NULL) {
        return 0;
    }
    *fuse = 1;
    RemoteBomb_Release();
    return 1;
}

// Read by the VB hook that stops the slate's own button from throwing it (remote_bomb_ship.cpp).
u8 RemoteBomb_IsHeld(void) {
    if ((sRemoteBomb == NULL) || (gPlayState == NULL)) {
        return 0;
    }
    return (GET_PLAYER(gPlayState)->heldActor == sRemoteBomb) ? 1 : 0;
}

s32 RemoteBomb_Cast(PlayState* play, Player* player) {
    if (sRemoteBomb == NULL) {
        return RemoteBomb_Lift(play, player);
    }
    return RemoteBomb_Detonate();
}

/**
 * Per-frame ownership, from Slate_TickInput. Runs with the slate stowed and the rune wheel open
 * too: a live bomb has to stay inert whatever the player is doing.
 */
void RemoteBomb_Tick(PlayState* play) {
    static s16 sLastScene = -1;

    if (sLastScene != play->sceneNum) {
        sLastScene = play->sceneNum;
        RemoteBomb_Drop();
        return;
    }
    if (sRemoteBomb == NULL) {
        return;
    }
    if (sRemoteBomb->update == NULL) {
        RemoteBomb_Drop();
        return;
    }
    // Already exploding — set off by us, by a sword blow or by an enemy. Either way it stops being
    // ours, and the blast must not be tinted.
    if (sRemoteBomb->params != BOMB_BODY) {
        RemoteBomb_Release();
        return;
    }

    // Re-asserted every frame, not set once: En_Bom counts the fuse down inside its own update.
    *RemoteBomb_Fuse(sRemoteBomb) = REMOTE_BOMB_INERT;
    sRemoteBomb->colorFilterParams = 0;
    RemoteBomb_TintAttach(sRemoteBomb);
}
