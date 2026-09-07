/**
 * cryonis_rune.c — Cryonis, the Sheikah Slate's third rune. Skijer's NEI.
 *
 * A pillar of ice rises out of a water surface, twice the height of the Ice Cavern block it is
 * made of, and its faces can be climbed. Only one pillar exists at a time: raising the next one
 * shatters the last, and aiming at the one already standing breaks it instead of placing another.
 *
 * The C press opens an aiming mode that owns the frame the way Ultrahand's does — D-up/D-down
 * choose the distance, A commits, B cancels.
 *
 * Consumed via #include from item_sheikah_slate.c, after master_cycle.c. No header.
 */

#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include <math.h>
#include "objects/object_ice_objects/object_ice_objects.h"
#include "overlays/actors/ovl_Bg_Ice_Objects/z_bg_ice_objects.h"
#include "../items/helpers/equip_helper.h"

// z_scene.c — not declared in functions.h (same as in somaria_cubes.c).
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);
// item_sheikah_slate.c, same TU but defined further down: is the tablet actually in Link's hand?
u8 Slate_IsDrawn(void);

// ── Public API ───────────────────────────────────────────────────────────────
s32 Cryonis_Cast(PlayState* play, Player* player);
void Cryonis_Tick(PlayState* play);
u8 Cryonis_ModeUpdate(PlayState* play, Player* player);
u8 Cryonis_ModeActive(void);
u8 Cryonis_IsClimbableBgId(s32 bgId);
void Cryonis_DrawGhost(PlayState* play);

// ── Tuning ───────────────────────────────────────────────────────────────────
#define CRYONIS_DIST_MIN 90.0f
#define CRYONIS_DIST_MAX 420.0f
#define CRYONIS_DIST_START 170.0f
#define CRYONIS_DIST_RATE 7.0f // units per frame with the pad held
// Slimmer and a touch shorter than a doubled block: a pillar, not a tower of ice.
#define CRYONIS_HEIGHT_MUL 1.8f
#define CRYONIS_WIDTH_MUL 0.6f
// The init chain in BgIceObjects_Init leaves 0.1 on all three axes; every dimension below folds it
// in, along with the two multipliers above.
#define CRYONIS_SCALE 0.1f
#define CRYONIS_BREAK_SLACK 20.0f // how far off the pillar the aim may sit and still mean "break it"
// The rise, in ticks at the 20 Hz the actor system runs at. It starts as a sliver, not at zero: a
// dynapoly box with no height is a set of degenerate polys for as long as it lasts.
#define CRYONIS_GROW_TICKS 11
#define CRYONIS_GROW_START 0.06f

typedef struct {
    Actor* pillar;
    s32 bgId;
    u8 modeActive;
    f32 dist;
    Vec3f ghostPos;
    u8 ghostValid;
    u8 ghostBreaks; // the aim is on the standing pillar: A breaks instead of placing
    u8 growTick;    // 0..CRYONIS_GROW_TICKS, the rise out of the water
    f32 waterY;     // the surface it grew from: the one thing the rise may not move
} CryonisState;

static CryonisState sCryonis = { NULL, BGCHECK_SCENE, 0, CRYONIS_DIST_START, { 0.0f, 0.0f, 0.0f }, 0, 0, 0, 0.0f };

// Half-extents and height of the doubled block, in world units. Measured once from the collision
// header, which resolves by OTR path and so needs no object bank.
static f32 sBlockHalfX = 0.0f;
static f32 sBlockHalfZ = 0.0f;
static f32 sBlockHeight = 0.0f;
static f32 sBlockBaseOffset = 0.0f; // world.pos.y - (bottom of the mesh), full grown
static f32 sBlockMinY = 0.0f;       // the mesh bottom in local space: the rise rescales this
static u8 sBlockMeasured = 0;

static Color_RGBA8 sShardPrim = { 250, 250, 250, 255 };
static Color_RGBA8 sShardEnv = { 180, 200, 235, 255 };

// What breaks the pillar. Only the two things that break ice everywhere else in the game — the rest
// of the damage table is deliberately absent so a stray sword swing on a bridge you are standing on
// cannot drop you in the lake.
static ColliderCylinderInit sPillarColliderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        // ALL, not PLAYER: the hammer is player-aligned damage but a bomb blast is not, and the
        // dmgFlags mask below is what actually keeps everything else out.
        AC_ON | AC_TYPE_ALL,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { DMG_EXPLOSIVE | DMG_HAMMER, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 20, 40, 0, { 0, 0, 0 } },
};

// Actor_Spawn only allocates sizeof(BgIceObjects), which has no room for a collider, so the one
// pillar that may exist keeps its own here.
static ColliderCylinder sPillarCollider;
static u8 sPillarColliderReady = 0;

static void Cryonis_Sfx(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void Cryonis_Measure(void) {
    CollisionHeader* header = NULL;

    if (sBlockMeasured) {
        return;
    }
    CollisionHeader_GetVirtual((void*)object_ice_objects_Col_0003F0, &header);
    if (header == NULL) {
        return;
    }
    sBlockHalfX = (header->maxBounds.x - header->minBounds.x) * 0.5f * CRYONIS_SCALE * CRYONIS_WIDTH_MUL;
    sBlockHalfZ = (header->maxBounds.z - header->minBounds.z) * 0.5f * CRYONIS_SCALE * CRYONIS_WIDTH_MUL;
    sBlockHeight = (header->maxBounds.y - header->minBounds.y) * CRYONIS_SCALE * CRYONIS_HEIGHT_MUL;
    sBlockMinY = (f32)header->minBounds.y;
    sBlockBaseOffset = -sBlockMinY * CRYONIS_SCALE * CRYONIS_HEIGHT_MUL;
    sBlockMeasured = 1;
}

// ── The pillar ───────────────────────────────────────────────────────────────

static void Cryonis_Forget(void) {
    sCryonis.pillar = NULL;
    sCryonis.bgId = BGCHECK_SCENE;
}

// A pillar killed from elsewhere — a scene change, a room unload — leaves a dangling pointer and,
// worse, a bgId the engine will hand to somebody else's collision. Both have to go together.
static void Cryonis_Prune(void) {
    if ((sCryonis.pillar != NULL) && (sCryonis.pillar->update == NULL)) {
        Cryonis_Forget();
    }
}

static void Cryonis_Shatter(PlayState* play) {
    Vec3f pos;
    Vec3f velocity;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    s32 i;

    Cryonis_Prune();
    if (sCryonis.pillar == NULL) {
        return;
    }
    pos = sCryonis.pillar->world.pos;
    Cryonis_Sfx(NA_SE_EV_ICE_BROKEN, &pos);

    for (i = 0; i < 12; i++) {
        Vec3f burst = pos;

        burst.x += Rand_CenteredFloat(sBlockHalfX * 2.0f);
        burst.y += Rand_ZeroOne() * sBlockHeight;
        burst.z += Rand_CenteredFloat(sBlockHalfZ * 2.0f);
        velocity.x = Rand_CenteredFloat(4.0f);
        velocity.y = 1.0f + (Rand_ZeroOne() * 2.0f);
        velocity.z = Rand_CenteredFloat(4.0f);
        func_8002829C(play, &burst, &velocity, &zero, &sShardPrim, &sShardEnv, 250, Rand_S16Offset(40, 15));
    }

    Actor_Kill(sCryonis.pillar);
    Cryonis_Forget();
}

// Fast out of the water, easing into place — the shape of BotW's own rise.
static f32 Cryonis_GrowProgress(u8 tick) {
    f32 t = (f32)tick / (f32)CRYONIS_GROW_TICKS;

    return CRYONIS_GROW_START + ((1.0f - CRYONIS_GROW_START) * (1.0f - SQ(1.0f - t)));
}

/**
 * One frame of the rise: the top stretches up and the base stays welded to the water it came out
 * of. The origin has to travel with the scale, because the mesh's own bottom sits sBlockMinY below
 * it — leaving the origin still is what would sink the pillar as it grew.
 */
static void Cryonis_ApplyGrowth(Actor* actor, f32 progress) {
    f32 scaleY = CRYONIS_SCALE * CRYONIS_HEIGHT_MUL * progress;

    actor->scale.y = scaleY;
    actor->world.pos.y = sCryonis.waterY - (sBlockMinY * scaleY);
    sPillarCollider.dim.height = (s16)(sBlockHeight * progress);
    sPillarCollider.dim.yShift = (s16)(sBlockMinY * scaleY);
}

/**
 * The pillar's whole update. BgIceObjects_Update runs Ice Cavern logic — a table of hardcoded
 * slide targets and a pit check written for that one room — which would drag the pillar back to
 * home.pos or drop it through the floor. What is left is releasing the push the player's dynapoly
 * contact registers — nothing else lowers that flag — the rise, and the break collider.
 */
static void Cryonis_PillarUpdate(Actor* thisx, PlayState* play) {
    BgIceObjects* this = (BgIceObjects*)thisx;

    if (sCryonis.growTick < CRYONIS_GROW_TICKS) {
        sCryonis.growTick++;
        Cryonis_ApplyGrowth(thisx, Cryonis_GrowProgress(sCryonis.growTick));
    }

    if (this->dyna.unk_150 != 0.0f) {
        GET_PLAYER(play)->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
        this->dyna.unk_150 = 0.0f;
    }

    if (sPillarCollider.base.acFlags & AC_HIT) {
        sPillarCollider.base.acFlags &= ~AC_HIT;
        Cryonis_Shatter(play);
        return; // the actor is dead — nothing below may touch it
    }
    Collider_UpdateCylinder(thisx, &sPillarCollider);
    CollisionCheck_SetAC(play, &play->colChkCtx, &sPillarCollider.base);
}

// Raises the pillar with its base on the water surface `pos` sits at. Returns 1 on success.
static u8 Cryonis_Raise(PlayState* play, Vec3f* pos) {
    Actor* actor;

    Cryonis_Measure();
    if (Object_GetIndex(&play->objectCtx, OBJECT_ICE_OBJECTS) < 0) {
        // Actor_Spawn refuses an actor whose object is not in the bank. The load is synchronous,
        // so the request placed here is honoured by the spawn on the next line.
        Object_Spawn(&play->objectCtx, OBJECT_ICE_OBJECTS);
    }

    actor =
        Actor_Spawn(&play->actorCtx, play, ACTOR_BG_ICE_OBJECTS, pos->x, pos->y + sBlockBaseOffset, pos->z, 0, 0, 0, 0);
    if ((actor == NULL) || (actor->update == NULL)) {
        return 0;
    }

    actor->update = Cryonis_PillarUpdate;
    actor->scale.x *= CRYONIS_WIDTH_MUL;
    actor->scale.z *= CRYONIS_WIDTH_MUL;

    if (!sPillarColliderReady) {
        Collider_InitCylinder(play, &sPillarCollider);
        sPillarColliderReady = 1;
    }
    Collider_SetCylinder(play, &sPillarCollider, actor, &sPillarColliderInit);
    sPillarCollider.dim.radius = (s16)fmaxf(sBlockHalfX, sBlockHalfZ);

    sCryonis.waterY = pos->y;
    sCryonis.growTick = 0;
    Cryonis_ApplyGrowth(actor, Cryonis_GrowProgress(0)); // it comes out of the water as a sliver

    sCryonis.pillar = actor;
    sCryonis.bgId = ((DynaPolyActor*)actor)->bgId;
    return 1;
}

/**
 * The pillar's faces are climbable, and nothing else is. Answered by bgId rather than by writing a
 * climbable wall type into the collision header: that header is a shared, cached resource, so
 * editing it would make every Ice Cavern block in the session climbable for good.
 */
u8 Cryonis_IsClimbableBgId(s32 bgId) {
    // Deliberately no prune: bgcheck calls this before the slate's tick has run on the first frame
    // of a new scene, when the pointer may already be freed memory. Comparing it against NULL is
    // safe; reading through it is not.
    return (sCryonis.pillar != NULL) && (bgId == sCryonis.bgId);
}

/**
 * Every frame from the slate's tick, ahead of its early returns. The pillar is a real actor and the
 * scene unload takes it with no word to us, so the scene number is the only reliable notice that
 * the pointer — and the bgId, which the engine hands straight back out — has gone stale.
 */
void Cryonis_Tick(PlayState* play) {
    static s16 sLastScene = -1;

    if (sLastScene != play->sceneNum) {
        sLastScene = play->sceneNum;
        sCryonis.modeActive = 0;
        Cryonis_Forget();
        return;
    }
    Cryonis_Prune();
}

// ── Aiming ───────────────────────────────────────────────────────────────────

// Placement follows the CAMERA, not Link's body, so the pillar lands where the player is looking.
static s16 Cryonis_CameraYaw(PlayState* play, Player* player) {
    Vec3f eye = play->view.eye;
    Vec3f at = play->view.lookAt;

    if ((fabsf(at.x - eye.x) < 0.001f) && (fabsf(at.z - eye.z) < 0.001f)) {
        return player->actor.shape.rot.y;
    }
    return Math_Vec3f_Yaw(&eye, &at);
}

static u8 Cryonis_AimIsOnPillar(Vec3f* pos) {
    f32 dx;
    f32 dz;
    f32 reach;

    if (sCryonis.pillar == NULL) {
        return 0;
    }
    dx = pos->x - sCryonis.pillar->world.pos.x;
    dz = pos->z - sCryonis.pillar->world.pos.z;
    reach = fmaxf(sBlockHalfX, sBlockHalfZ) + CRYONIS_BREAK_SLACK;
    return ((dx * dx) + (dz * dz)) < (reach * reach);
}

// Is the water at `pos` reachable, or is there a wall between Link and it?
static u8 Cryonis_PathIsClear(PlayState* play, Player* player, Vec3f* pos) {
    Vec3f from = player->actor.world.pos;
    Vec3f to = *pos;
    Vec3f hit;
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;

    from.y += 40.0f;
    to.y += 20.0f;
    return !BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true, &bgId);
}

static void Cryonis_UpdateGhost(PlayState* play, Player* player) {
    s16 yaw = Cryonis_CameraYaw(play, player);
    Vec3f pos;
    f32 waterY;
    WaterBox* waterBox = NULL;

    pos.x = player->actor.world.pos.x + (Math_SinS(yaw) * sCryonis.dist);
    pos.y = player->actor.world.pos.y;
    pos.z = player->actor.world.pos.z + (Math_CosS(yaw) * sCryonis.dist);

    if (!WaterBox_GetSurface1(play, &play->colCtx, pos.x, pos.z, &waterY, &waterBox)) {
        sCryonis.ghostPos = pos;
        sCryonis.ghostValid = 0;
        sCryonis.ghostBreaks = 0;
        return;
    }

    pos.y = waterY;
    sCryonis.ghostPos = pos;
    sCryonis.ghostBreaks = Cryonis_AimIsOnPillar(&pos);
    sCryonis.ghostValid = sCryonis.ghostBreaks || Cryonis_PathIsClear(play, player, &pos);
}

// ── The aiming mode ──────────────────────────────────────────────────────────

u8 Cryonis_ModeActive(void) {
    return sCryonis.modeActive;
}

static void Cryonis_ModeExit(u8 cancelled) {
    if (!sCryonis.modeActive) {
        return;
    }
    sCryonis.modeActive = 0;
    sCryonis.ghostValid = 0;
    sCryonis.ghostBreaks = 0;
    if (cancelled) {
        Cryonis_Sfx(NA_SE_SY_CANCEL, &gSfxDefaultPos);
    }
}

/**
 * The rune's cast: it opens the aiming mode rather than placing anything. Returns 1 so the slate
 * tick treats the press as a real cast and skips its error cue.
 */
s32 Cryonis_Cast(PlayState* play, Player* player) {
    Cryonis_Measure();
    sCryonis.modeActive = 1;
    sCryonis.dist = CRYONIS_DIST_START;
    Cryonis_UpdateGhost(play, player);
    Cryonis_Sfx(NA_SE_SY_GET_ITEM, &gSfxDefaultPos);
    return 1;
}

/**
 * One frame of the mode. Returns 1 while it owns the input, so the slate tick stops.
 *
 * Called from Slate_TickInput ABOVE its blocking checks and its stow paths, which is why the ones
 * that matter are repeated here: with the mode below them, the A that commits had already gone
 * through the unequip and the tablet was gone by the time the press arrived (the same ordering the
 * Ultrahand mode documents).
 */
u8 Cryonis_ModeUpdate(PlayState* play, Player* player) {
    Input* input;
    u16 cur;

    if (!sCryonis.modeActive) {
        return 0;
    }
    if (!Slate_IsDrawn() || ItemInput_IsBlocked(player, play) || (player->stateFlags1 & PLAYER_STATE1_IN_WATER) ||
        (player->meleeWeaponState != 0)) {
        Cryonis_ModeExit(1);
        return 0;
    }

    input = &play->state.input[0];
    cur = input->cur.button;

    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        Cryonis_ModeExit(1);
        return 1;
    }

    // Read from cur.button, never press: the player actor consumes the D-pad press bits for its own
    // item handling long before this runs. A held pad is a continuous move here, so the state is
    // what we want anyway.
    if (cur & BTN_DUP) {
        sCryonis.dist += CRYONIS_DIST_RATE;
    }
    if (cur & BTN_DDOWN) {
        sCryonis.dist -= CRYONIS_DIST_RATE;
    }
    sCryonis.dist = CLAMP(sCryonis.dist, CRYONIS_DIST_MIN, CRYONIS_DIST_MAX);

    Cryonis_UpdateGhost(play, player);

    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        if (sCryonis.ghostBreaks) {
            Cryonis_Shatter(play);
            Cryonis_ModeExit(0);
            return 1;
        }
        if (!sCryonis.ghostValid) {
            Cryonis_Sfx(NA_SE_SY_ERROR, &player->actor.world.pos);
            return 1; // stay in the mode: a bad aim is a miss, not a cancel
        }
        Cryonis_Shatter(play); // only one pillar stands at a time
        if (!Cryonis_Raise(play, &sCryonis.ghostPos)) {
            Cryonis_Sfx(NA_SE_SY_ERROR, &player->actor.world.pos);
        }
        Cryonis_ModeExit(0);
    }
    return 1;
}

// ── Ghost ────────────────────────────────────────────────────────────────────

// A self-contained unit cube, so the preview needs nothing out of any object bank.
static Vtx sCryonisGhostVtx[] = {
    VTX(-1, 0, -1, 0, 0, 0, 0, 0, 255), VTX(1, 0, -1, 0, 0, 0, 0, 0, 255),  VTX(1, 0, 1, 0, 0, 0, 0, 0, 255),
    VTX(-1, 0, 1, 0, 0, 0, 0, 0, 255),  VTX(-1, 1, -1, 0, 0, 0, 0, 0, 255), VTX(1, 1, -1, 0, 0, 0, 0, 0, 255),
    VTX(1, 1, 1, 0, 0, 0, 0, 0, 255),   VTX(-1, 1, 1, 0, 0, 0, 0, 0, 255),
};

static Gfx sCryonisGhostDL[] = {
    gsSPVertex(sCryonisGhostVtx, 8, 0),     gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(4, 6, 5, 0, 4, 7, 6, 0), gsSP2Triangles(0, 5, 1, 0, 0, 4, 5, 0),
    gsSP2Triangles(1, 6, 2, 0, 1, 5, 6, 0), gsSP2Triangles(2, 7, 3, 0, 2, 6, 7, 0),
    gsSP2Triangles(3, 4, 0, 0, 3, 7, 4, 0), gsSPEndDisplayList(),
};

void Cryonis_DrawGhost(PlayState* play) {
    f32 pulse;

    if (!sCryonis.modeActive || !sBlockMeasured) {
        return;
    }

    pulse = 0.94f + (0.06f * Math_SinS((s16)(play->gameplayFrames * 1500)));

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    Matrix_Translate(sCryonis.ghostPos.x, sCryonis.ghostPos.y, sCryonis.ghostPos.z, MTXMODE_NEW);
    Matrix_Scale(sBlockHalfX * pulse, sBlockHeight * pulse, sBlockHalfZ * pulse, MTXMODE_APPLY);

    // Components spelled out: MSVC hands a multi-value #define to a function-like macro as ONE
    // argument, so gDPSetPrimColor would not expand.
    if (sCryonis.ghostBreaks) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 150, 90, 120);
        gDPSetEnvColor(POLY_XLU_DISP++, 180, 60, 0, 120);
    } else if (sCryonis.ghostValid) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 150, 215, 255, 110);
        gDPSetEnvColor(POLY_XLU_DISP++, 20, 90, 180, 110);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 70, 70, 110);
        gDPSetEnvColor(POLY_XLU_DISP++, 150, 0, 0, 110);
    }
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, sCryonisGhostDL);

    CLOSE_DISPS(play->state.gfxCtx);
}
