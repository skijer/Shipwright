/**
 * Ultrahand: grab what Link aims at and carry it, turn it, weld pieces into one solid, store a structure and
 * rebuild it with magic. Assembled from pieces: it does nothing until the configured number is found.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_ru1/object_ru1.h"
#include "overlays/actors/ovl_Bg_Spot00_Hanebasi/z_bg_spot00_hanebasi.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Ru1/z_en_ru1.h"
#include "overlays/actors/ovl_En_Siofuki/z_en_siofuki.h"

// This fork's bgCheckFlags carry no names; these are OoT's bits.
#define BGCHECKFLAG_GROUND (1 << 0)
#define BGCHECKFLAG_GROUND_TOUCH (1 << 1)
#define BGCHECKFLAG_GROUND_LEAVE (1 << 2)
#define BGCHECKFLAG_WALL (1 << 3)
#define BGCHECKFLAG_CEILING (1 << 4)
#define BGCHECKFLAG_WATER (1 << 5)
#define BGCHECKFLAG_WATER_TOUCH (1 << 6)
#define BGCHECKFLAG_GROUND_STRICT (1 << 7)

#define TARGETSEL_LIST_HEAD(list) ((list).head)

typedef s32 (*TargetSelectFilter)(struct Actor* actor);

typedef struct {
    u32 dmgFlags;
    u8 damage;
    u8 effect;
    f32 radius;
    f32 height;
} CombatColliderConfig;

static void Combat_InitCylinder(PlayState* play, ColliderCylinder* col, Actor* owner, CombatColliderConfig* cfg) {
    ColliderCylinderInit init = {
        { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
        { ELEMTYPE_UNK2, { cfg->dmgFlags, cfg->effect, cfg->damage }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL,
          BUMP_NONE, OCELEM_NONE },
        { (s16)cfg->radius, (s16)cfg->height, 0, { 0, 0, 0 } },
    };

    Collider_InitCylinder(play, col);
    Collider_SetCylinder(play, col, owner, &init);
}

static void Combat_UpdateCylinder(ColliderCylinder* col, Vec3f* pos, CombatColliderConfig* cfg) {
    col->dim.pos.x = (s16)pos->x;
    col->dim.pos.y = (s16)pos->y;
    col->dim.pos.z = (s16)pos->z;
    col->dim.radius = (s16)cfg->radius;
    col->dim.height = (s16)cfg->height;
    col->info.toucher.dmgFlags = cfg->dmgFlags;
    col->info.toucher.damage = cfg->damage;
    col->info.toucher.effect = cfg->effect;
    col->base.atFlags |= AT_ON | AT_TYPE_PLAYER;
}

static void Combat_RegisterCollider(PlayState* play, ColliderCylinder* col) {
    CollisionCheck_SetAT(play, &play->colChkCtx, &col->base);
}

static u8 Combat_CheckHit(ColliderCylinder* col) {
    return (col->base.atFlags & AT_HIT) ? 1 : 0;
}

static Actor* Pacci_ResolveUltrahandTarget(PlayState* play, Player* player);
void Pacci_UpdateUltrahand(PlayState* play, Player* player);
Actor* Pacci_FuseRootOf(Actor* actor);
u8 Pacci_FuseCount(void);
void Pacci_FuseRelease(void);
void Pacci_FuseFollow(PlayState* play);
u8 Pacci_BackRiderActive(void);
void Pacci_BackRiderDrop(void);

#define PACCI_UH_ICE_PERIOD 16

static const SOHModApi* sApi;

// Another mod's actors, known only by the string keys it registered them under.
static bool IsSomariaWeight(Actor* actor) {
    static const char* const sWeightKeys[] = { "nei.somaria.statue", "nei.somaria.crate" };
    static s16 sWeightIds[] = { -1, -1 };

    for (u32 i = 0; i < ARRAY_COUNT(sWeightKeys); i++) {
        if (sWeightIds[i] < 0) {
            sWeightIds[i] = sApi->GetActorId(sWeightKeys[i]);
        }
        if (sWeightIds[i] >= 0 && actor->id == sWeightIds[i]) {
            return true;
        }
    }
    return false;
}

// Reach sweep for things at Link's feet the aim line can't hit; short on purpose, not a second grab range.
#define PACCI_UH_REACH_RANGE 180.0f
#define PACCI_UH_REACH_CONE 0x2000
#define PACCI_UH_DIST_MIN 150.0f
#define PACCI_UH_DIST_MAX 500.0f
#define PACCI_UH_DIST_STEP 25.0f
#define PACCI_UH_ROT_STEP 1000
#define PACCI_UH_FOLLOW_WEIGHT 0.80f
#define PACCI_UH_TURN_RATE 0x0400
// En_Ishi's own fall numbers, the closest vanilla analogue to a heavy released prop.
#define PACCI_UH_DROP_GRAVITY -3.2f
#define PACCI_UH_DROP_MIN_VEL_Y -36.0f
#define PACCI_UH_DROP_DRAG 0.94f
// Caps inertia inherited from the carry so a fast aim whip can't catapult the release.
#define PACCI_UH_THROW_MAX 16.0f
#define PACCI_UH_GROUND_PROBE 600.0f
// Runaway guard only; PACCI_UH_ABANDON_DROP is what actually gives up on a fall.
#define PACCI_UH_DROP_TIMEOUT 400
#define PACCI_UH_ABANDON_DROP 250.0f

#define PACCI_UH_SUMMON_COST 48
#define PACCI_UH_SUMMON_HOLD 20
#define PACCI_UH_SUMMON_RISE 40.0f
#define PACCI_PLACE_PULL 0.12f
// Resting exactly ON a surface is a coin flip to the engine's standing test; sink past it a little.
#define PACCI_PLACE_SINK 1.5f
#define PACCI_PLACE_ARRIVED 0.9f
#define PACCI_PRESS_HOLD 40.0f
#define PACCI_BACKRIDE_BEHIND 9.0f
#define PACCI_BACKRIDE_RISE 6.0f
#define PACCI_UH_FLAG_TRAVEL 70.0f
// Bg_Dodoago's own open pose; the only LOCKED actor there is.
#define PACCI_UH_LOCKED_OPEN_X 0x1333
// Damage 2 is one boomerang hit; En_Ba has 4 HP, so a cut takes 4 goes like the real thing.
#define PACCI_UH_CUT_DAMAGE 2
#define PACCI_UH_CUT_RADIUS 30.0f
#define PACCI_UH_CUT_HEIGHT 80.0f
#define PACCI_UH_CUT_FRAMES 6
#define PACCI_UH_HEIGHT_STEP 40.0f
#define PACCI_UH_HEIGHT_MAX 600.0f
#define PACCI_UH_THROW_HOLD 4
#define PACCI_UH_PULL_RATE 1.6f
#define PACCI_UH_THROW_REACH 18.0f
#define PACCI_UH_THROW_RISE 6.0f
#define PACCI_UH_THROW_SPEED 14.0f
#define PACCI_UH_THROW_LIFT 3.0f
// Past this half-extent a dynapoly body with no trait row is room geometry, not an object.
#define PACCI_UH_MAX_HALF 300.0f

// colorFilterParams has only three modes: 0x8000 white, 0x4000 red, anything else blue.
#define PACCI_TINT_FLIP(actor, dur) Actor_SetColorFilter((actor), 0x4000, 255, 0, (dur))
#define PACCI_TINT_STONE(actor, dur) Actor_SetColorFilter((actor), 0x8000, 255, 0, (dur))
#define PACCI_TINT_ULTRAHAND(actor, dur) Actor_SetColorFilter((actor), 0, 255, 0, (dur))
#define PACCI_TINT_HURT(actor, dur) Actor_SetColorFilter((actor), 0x8000, 255, 0, (dur))

#ifndef PACCI_FLAG_LIFTABLE
#define PACCI_FLAG_LIFTABLE (1 << 28)
#endif

static const s16 sPacciBlacklist[] = {
    ACTOR_EN_IK,
    ACTOR_EN_TORCH2,
    ACTOR_EN_ZF,
    ACTOR_EN_WALLMAS,
    ACTOR_EN_FLOORMAS,
    ACTOR_EN_RD,
    ACTOR_EN_FZ,
    ACTOR_EN_VM,
    ACTOR_EN_RR,
    // Invisible, model-less song-trigger spots; grabbing one would carry the trigger away.
    ACTOR_EN_OKARINA_TAG,
    ACTOR_EN_OKARINA_EFFECT,
};

#define TARGETSEL_DEFAULT_RANGE 520.0f
#define TARGETSEL_DEFAULT_CONE 0x1800
#define TARGETSEL_MIN_DIST 30.0f

/** The categories the Switch Hook scans; the default set when `cats` is NULL. */
extern const u8 gTargetSelectDefaultCats[4];
#define TARGETSEL_DEFAULT_CAT_COUNT 4

const u8 gTargetSelectDefaultCats[4] = {
    ACTORCAT_ENEMY,
    ACTORCAT_PROP,
    ACTORCAT_CHEST,
    ACTORCAT_NPC,
};

s32 TargetSelect_IsCommonTarget(Actor* actor) {
    return (actor != NULL) && (actor->update != NULL) &&
           ((actor->category == ACTORCAT_ENEMY) || (actor->category == ACTORCAT_PROP) ||
            (actor->category == ACTORCAT_CHEST) || (actor->category == ACTORCAT_NPC));
}

Actor* TargetSelect_ScanCatsFromYaw(PlayState* play, const u8* cats, s32 numCats, TargetSelectFilter filter, f32 range,
                                    f32 minDist, s16 cone, s16 yaw) {
    Player* player;
    Actor* best = NULL;
    s32 bestYawErr;
    s32 i;

    if (play == NULL) {
        return NULL;
    }
    if (cats == NULL) {
        cats = gTargetSelectDefaultCats;
        numCats = TARGETSEL_DEFAULT_CAT_COUNT;
    }
    player = GET_PLAYER(play);
    if (player == NULL) {
        return NULL;
    }
    bestYawErr = cone;

    for (i = 0; i < numCats; i++) {
        Actor* actor = TARGETSEL_LIST_HEAD(play->actorCtx.actorLists[cats[i]]);

        while (actor != NULL) {
            if ((actor->update != NULL) && ((filter == NULL) || filter(actor))) {
                f32 dx = actor->world.pos.x - player->actor.world.pos.x;
                f32 dz = actor->world.pos.z - player->actor.world.pos.z;
                f32 distXZ = sqrtf((dx * dx) + (dz * dz));

                if ((distXZ > minDist) && (distXZ <= range)) {
                    // OoT's Math_Atan2S takes (x, y): swapping the arguments turns the cone 90 degrees.
                    s32 yawErr = (s16)(Math_Atan2S(dz, dx) - yaw);

                    if (yawErr < 0) {
                        yawErr = -yawErr;
                    }
                    if (yawErr < bestYawErr) {
                        bestYawErr = yawErr;
                        best = actor;
                    }
                }
            }
            actor = actor->next;
        }
    }
    return best;
}

Actor* TargetSelect_ScanCats(PlayState* play, const u8* cats, s32 numCats, TargetSelectFilter filter, f32 range,
                             s16 cone) {
    Player* player = (play != NULL) ? GET_PLAYER(play) : NULL;

    if (player == NULL) {
        return NULL;
    }
    return TargetSelect_ScanCatsFromYaw(play, cats, numCats, filter, range, TARGETSEL_MIN_DIST, cone,
                                        player->actor.shape.rot.y);
}

Actor* TargetSelect_Scan(PlayState* play, TargetSelectFilter filter) {
    return TargetSelect_ScanCats(play, NULL, 0, (filter != NULL) ? filter : TargetSelect_IsCommonTarget,
                                 TARGETSEL_DEFAULT_RANGE, TARGETSEL_DEFAULT_CONE);
}

void SwitchHook_ShiftCollider(Collider* col, Vec3f* delta) {
    // Round rather than truncate: the s16 shapes would otherwise lag the model by a unit/frame.
    s16 dxs = (s16)((delta->x >= 0.0f) ? (delta->x + 0.5f) : (delta->x - 0.5f));
    s16 dys = (s16)((delta->y >= 0.0f) ? (delta->y + 0.5f) : (delta->y - 0.5f));
    s16 dzs = (s16)((delta->z >= 0.0f) ? (delta->z + 0.5f) : (delta->z - 0.5f));
    s32 i;
    s32 j;

    switch (col->shape) {
        case COLSHAPE_JNTSPH: {
            ColliderJntSph* jntSph = (ColliderJntSph*)col;

            for (i = 0; i < jntSph->count; i++) {
                Sphere16* sphere = &jntSph->elements[i].dim.worldSphere;

                sphere->center.x += dxs;
                sphere->center.y += dys;
                sphere->center.z += dzs;
            }
            break;
        }

        case COLSHAPE_CYLINDER: {
            ColliderCylinder* cyl = (ColliderCylinder*)col;

            cyl->dim.pos.x += dxs;
            cyl->dim.pos.y += dys;
            cyl->dim.pos.z += dzs;
            break;
        }

        case COLSHAPE_TRIS: {
            ColliderTris* tris = (ColliderTris*)col;

            for (i = 0; i < tris->count; i++) {
                TriNorm* tri = &tris->elements[i].dim;

                for (j = 0; j < 3; j++) {
                    tri->vtx[j].x += delta->x;
                    tri->vtx[j].y += delta->y;
                    tri->vtx[j].z += delta->z;
                }
                // Plane is `n . p + originDist = 0`; translating the tri moves it by -(n . delta).
                tri->plane.originDist -= (tri->plane.normal.x * delta->x) + (tri->plane.normal.y * delta->y) +
                                         (tri->plane.normal.z * delta->z);
            }
            break;
        }

        case COLSHAPE_QUAD: {
            ColliderQuad* quad = (ColliderQuad*)col;

            for (i = 0; i < 4; i++) {
                quad->dim.quad[i].x += delta->x;
                quad->dim.quad[i].y += delta->y;
                quad->dim.quad[i].z += delta->z;
            }
            // Cached edge midpoints are world-space too; they drive the quad-vs-quad tests.
            quad->dim.dcMid.x += dxs;
            quad->dim.dcMid.y += dys;
            quad->dim.dcMid.z += dzs;
            quad->dim.baMid.x += dxs;
            quad->dim.baMid.y += dys;
            quad->dim.baMid.z += dzs;
            break;
        }
    }
}

// One point per collider: the rest of the shape moves rigidly with it.
s32 SwitchHook_GetColliderRefPos(Collider* col, Vec3f* out) {
    switch (col->shape) {
        case COLSHAPE_JNTSPH: {
            ColliderJntSph* jntSph = (ColliderJntSph*)col;

            if ((jntSph->count <= 0) || (jntSph->elements == NULL)) {
                return 0;
            }
            out->x = jntSph->elements[0].dim.worldSphere.center.x;
            out->y = jntSph->elements[0].dim.worldSphere.center.y;
            out->z = jntSph->elements[0].dim.worldSphere.center.z;
            return 1;
        }

        case COLSHAPE_CYLINDER: {
            ColliderCylinder* cyl = (ColliderCylinder*)col;

            out->x = cyl->dim.pos.x;
            out->y = cyl->dim.pos.y;
            out->z = cyl->dim.pos.z;
            return 1;
        }

        case COLSHAPE_TRIS: {
            ColliderTris* tris = (ColliderTris*)col;

            if ((tris->count <= 0) || (tris->elements == NULL)) {
                return 0;
            }
            *out = tris->elements[0].dim.vtx[0];
            return 1;
        }

        case COLSHAPE_QUAD: {
            ColliderQuad* quad = (ColliderQuad*)col;

            *out = quad->dim.quad[0];
            return 1;
        }
    }
    return 0;
}


// A table, not a rule: "on a rail" or "on fire" is nothing the actor struct exposes.
typedef enum {
    PACCI_UH_TRAIT_EXCLUDE = 1 << 0,
    PACCI_UH_TRAIT_AXIS_Y = 1 << 1,
    PACCI_UH_TRAIT_PLANE_XZ = 1 << 2,
    PACCI_UH_TRAIT_PATH = 1 << 3,     // confined to the scene path its params name
    PACCI_UH_TRAIT_NO_TURN = 1 << 4,
    PACCI_UH_TRAIT_BURNS = 1 << 5,    // sets fire to what it touches while carried
    // Opts in actors neither the raycast nor the category scan can see (En_Ice_Hono).
    PACCI_UH_TRAIT_REACHABLE = 1 << 6,
    PACCI_UH_TRAIT_NO_FACE = 1 << 7,    // still answers L + D-pad, but doesn't turn to face Link
    PACCI_UH_TRAIT_SETS_FLAG = 1 << 8,  // moving it far enough sets the switch flag params name
    PACCI_UH_TRAIT_CLEARS_FLAG = 1 << 9, // moving it back the other way clears that flag
    PACCI_UH_TRAIT_LOCKED = 1 << 10,    // never moved; D-pad drives its FLAG instead of position
    // SETS_FLAG only: the flag index is real only when params fit entirely inside the field.
    PACCI_UH_TRAIT_FLAG_STRICT = 1 << 11,
    // A proxy is carried instead, for actors that must stay put (bomb flower, torch).
    PACCI_UH_TRAIT_PROXY = 1 << 12,
    PACCI_UH_TRAIT_EXPLODES = 1 << 13, // proxy is a live bomb; attach button detonates it
    // home.pos is written too, for actors that rebuild position from it (Bg_Hidan_Fslift).
    PACCI_UH_TRAIT_DRIVE_HOME = 1 << 14,
    // Hit with the row's blow instead of carried; the actor plays its own reaction.
    PACCI_UH_TRAIT_STRIKES = 1 << 15,
    PACCI_UH_TRAIT_HEIGHT = 1 << 16,   // held in place; D-pad drives HEIGHT, not position
    PACCI_UH_TRAIT_THROWS = 1 << 17,   // aiming at it hands it to its own throw
    PACCI_UH_TRAIT_PULLABLE = 1 << 18, // hauled along the ground with the vanilla pull animation
    // Split out of LOCKED: LOCKED used to imply this and tilted every locked body 26 degrees on X.
    PACCI_UH_TRAIT_JAW = 1 << 19,      // D-pad swings one hinge of this body open/shut, posed by hand
    PACCI_UH_TRAIT_HINGE = 1 << 20,    // same gesture, handed to an animation the actor already owns
} PacciUhTrait;

// MOVEBG_TYPE is the top nibble of params; these four split Bg_Mizu_Movebg's seven machines by it.
static u8 Pacci_UhMovebgType(Actor* actor) {
    return (u8)(((u16)actor->params >> 0xC) & 0xF);
}

static u8 Pacci_UhCondMovebgWaterSlaved(Actor* actor) {
    return (Pacci_UhMovebgType(actor) <= 2) ? 1 : 0;
}

static u8 Pacci_UhCondMovebgDragonRoom(Actor* actor) {
    return (Pacci_UhMovebgType(actor) == 3) ? 1 : 0;
}

static u8 Pacci_UhCondMovebgSwitched(Actor* actor) {
    u8 type = Pacci_UhMovebgType(actor);

    return ((type >= 4) && (type <= 6)) ? 1 : 0;
}

static u8 Pacci_UhCondMovebgHookshot(Actor* actor) {
    return (Pacci_UhMovebgType(actor) == 7) ? 1 : 0;
}

// Only the deck, params -1 (unexported DT_DRAWBRIDGE); the chains are 0 and 1 and follow it.
static u8 Pacci_UhCondDrawbridge(Actor* actor) {
    return (actor->params == -1) ? 1 : 0;
}

typedef struct {
    s16 actorId;
    // u32: the traits run past bit 15.
    u32 traits;
    u8 pathShift;
    u8 pathMask;
    // NULL means the row applies unconditionally; otherwise it gates on actor state, not identity.
    u8 (*cond)(Actor* actor);
    // SETS_FLAG only: which way the body must travel before the flag sets. +1 up, -1 down, 0 either.
    s8 flagDir;

    s16 proxyId;
    s16 proxyParams;
    // STRIKES only: flags the target's own bumper accepts, so its rules decide the outcome.
    u32 hitFlags;
    s16 hitDamage;
} PacciUhTraitRow;

// The flame collider is submitted only while lit; read off the AC list, not ObjSyokudai's layout.
static u8 Pacci_UhTorchLit(PlayState* play, Actor* actor) {
    s32 i;

    for (i = 0; i < play->colChkCtx.colACCount; i++) {
        Collider* col = play->colChkCtx.colAC[i];

        // The flame is the one with no OC: the stand collides with you, the fire does not.
        if ((col != NULL) && (col->actor == actor) && (col->ocFlags1 == OC1_NONE)) {
            return 1;
        }
    }
    return 0;
}

static u8 Pacci_UhCondTorchLit(Actor* actor) {
    return (gPlayState != NULL) ? Pacci_UhTorchLit(gPlayState, actor) : 0;
}

// params 0xFFFF is the persistent variant; vanilla treats params as a switch index only below 0x40.
static u8 Pacci_UhCondSwitchParams(Actor* actor) {
    return (actor->params < 0x40) ? 1 : 0;
}

static u8 Pacci_UhCondPersistent(Actor* actor) {
    return ((u16)actor->params == 0xFFFF) ? 1 : 0;
}

// EnRu1 plays gRutoChildSittingAnim right before each Actor_OfferCarry: the one reliable tell.
static u8 Pacci_UhCondRutoSitting(Actor* actor) {
    if ((actor == NULL) || (actor->parent != NULL)) {
        return 0;
    }
    return (((EnRu1*)actor)->skelAnime.animation == (void*)gRutoChildSittingAnim) ? 1 : 0;
}

static const PacciUhTraitRow sPacciUhTraits[] = {
    { ACTOR_BG_MORI_KAITENKABE, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    // Can't be CARRIED: its Draw rewrites world/home.pos from a matrix every frame. Can be thrown.
    { ACTOR_BG_HEAVY_BLOCK, PACCI_UH_TRAIT_THROWS, 0, 0, NULL },
    // Only y is rebuilt from home (bob sine); x/z stay free, so PLANE_XZ is correct here.
    { ACTOR_BG_HAKA_SHIP, PACCI_UH_TRAIT_PLANE_XZ | PACCI_UH_TRAIT_NO_TURN, 0, 0, NULL },
    { ACTOR_BG_MIZU_WATER, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    { ACTOR_BG_JYA_ZURERUKABE, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    { ACTOR_BG_JYA_AMISHUTTER, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    { ACTOR_BG_HIDAN_HAMSTEP, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    // Update ends in Math_Vec3f_Copy(world.pos, home.pos): teleports back the frame you let go.
    { ACTOR_OBJ_LIFT, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    // JntSph collider centre is baked at Init and never refreshed; moving it leaves the hitbox behind.
    { ACTOR_BG_MENKURI_EYE, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    { ACTOR_BG_MORI_HINERI, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    { ACTOR_BG_MORI_BIGST, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    { ACTOR_BG_MORI_RAKKATENJO, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    { ACTOR_BG_MORI_IDOMIZU, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    { ACTOR_BG_MORI_HASHIRA4, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },
    { ACTOR_BG_MORI_HASHIGO, PACCI_UH_TRAIT_EXCLUDE, 0, 0, NULL },

    // These two track a switch flag for "I am up"; Obj_Elevator/Bg_Hidan_Syoku have no such flag.
    { ACTOR_BG_MORI_ELEVATOR, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_SETS_FLAG, 0, 0x3F, NULL,
      1 },
    // BgJyaLift_Move only steps between two hardcoded heights then nulls its actionFunc: a toggle.
    { ACTOR_BG_JYA_LIFT, PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_SETS_FLAG, 0, 0x3F, NULL, 1 },
    { ACTOR_OBJ_ELEVATOR, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN, 0, 0, NULL },
    // Moving one sets the flag the room's own switch would; BgDdanJd_Idle reads it only below 0x40.
    { ACTOR_BG_DDAN_JD,
      PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_SETS_FLAG | PACCI_UH_TRAIT_FLAG_STRICT, 0, 0x3F,
      NULL, 1 },
    { ACTOR_BG_DDAN_KD, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN, 0, 0, NULL },
    // Held in place, D-up/D-down drive the switch flag (params & 0x3F) directly.
    { ACTOR_BG_JYA_KANAAMI, PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_SETS_FLAG, 0, 0x3F, NULL, 1 },
    // Flag does nothing at runtime (Init-only read); what it answers to is an explosion.
    { ACTOR_BG_JYA_BOMBCHUIWA, PACCI_UH_TRAIT_STRIKES, 0, 0, NULL, 0, 0, 0, DMG_EXPLOSIVE, PACCI_UH_CUT_DAMAGE },
    // Init does `params &= 0xFF`, so the surviving byte IS the flag index here, unlike its neighbours.
    { ACTOR_BG_HAKA_HUTA, PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_SETS_FLAG, 0, 0xFF, NULL, 1 },
    { ACTOR_BG_DODOAGO,
      PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_JAW | PACCI_UH_TRAIT_SETS_FLAG | PACCI_UH_TRAIT_CLEARS_FLAG, 0, 0x3F, NULL,
      1 },
    // Fire Temple movers rebuild position from home.pos every frame; home is the only handle they have.
    { ACTOR_BG_HIDAN_SYOKU, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_DRIVE_HOME, 0, 0, NULL },
    { ACTOR_BG_HIDAN_ROCK, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_DRIVE_HOME, 0, 0, NULL },
    { ACTOR_BG_HIDAN_SIMA, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_DRIVE_HOME, 0, 0, NULL },
    { ACTOR_BG_HIDAN_FSLIFT, PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_DRIVE_HOME, 0, 0, NULL },

    // Pinning Y is the puzzle itself: lifting a slide block over its obstacle deletes the room.
    { ACTOR_BG_ICE_OBJECTS, PACCI_UH_TRAIT_PLANE_XZ | PACCI_UH_TRAIT_NO_TURN, 0, 0, NULL },
    { ACTOR_BG_GND_ICEBLOCK, PACCI_UH_TRAIT_PLANE_XZ | PACCI_UH_TRAIT_NO_TURN, 0, 0, NULL },

    // Types 0-3 follow the water level or a private field: nothing to hand over.
    { ACTOR_BG_MIZU_MOVEBG, PACCI_UH_TRAIT_EXCLUDE, 0, 0, Pacci_UhCondMovebgWaterSlaved },
    { ACTOR_BG_MIZU_MOVEBG, PACCI_UH_TRAIT_EXCLUDE, 0, 0, Pacci_UhCondMovebgDragonRoom },
    // Types 4-6 are a real two-way switch toggle.
    { ACTOR_BG_MIZU_MOVEBG, PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_SETS_FLAG | PACCI_UH_TRAIT_CLEARS_FLAG, 0, 0x3F,
      Pacci_UhCondMovebgSwitched, 1 },
    { ACTOR_BG_MIZU_MOVEBG, PACCI_UH_TRAIT_PATH | PACCI_UH_TRAIT_NO_TURN, 8, 0xF, Pacci_UhCondMovebgHookshot },
    { ACTOR_OBJ_BEAN, PACCI_UH_TRAIT_PATH | PACCI_UH_TRAIT_NO_TURN, 8, 0x1F, NULL },

    // Flame Circle is carried for real, as a barrier; no proxy.
    { ACTOR_BG_HIDAN_CURTAIN, PACCI_UH_TRAIT_BURNS, 0, 0, NULL },
    // What comes away is a loose En_Light flame, not a copy of the wall.
    { ACTOR_BG_HIDAN_FIREWALL, PACCI_UH_TRAIT_BURNS | PACCI_UH_TRAIT_PROXY, 0, 0, NULL, 0, ACTOR_EN_LIGHT, 0 },
    { ACTOR_BG_HIDAN_FWBIG, PACCI_UH_TRAIT_BURNS | PACCI_UH_TRAIT_PROXY, 0, 0, NULL, 0, ACTOR_EN_LIGHT, 0 },
    { ACTOR_EN_LIGHT, PACCI_UH_TRAIT_BURNS | PACCI_UH_TRAIT_PROXY | PACCI_UH_TRAIT_REACHABLE, 0, 0, NULL, 0,
      ACTOR_EN_LIGHT, 0 },
    // Lifting a bomb flower takes a fuseless proxy bomb; the attach button lights it.
    { ACTOR_EN_BOMBF, PACCI_UH_TRAIT_PROXY | PACCI_UH_TRAIT_EXPLODES, 0, 0, NULL, 0, ACTOR_EN_BOM, 0 },
    { ACTOR_OBJ_SYOKUDAI, PACCI_UH_TRAIT_BURNS, 0, 0, Pacci_UhCondTorchLit },
    { ACTOR_BG_PO_SYOKUDAI, PACCI_UH_TRAIT_BURNS, 0, 0, Pacci_UhCondTorchLit },

    // Keep whatever angle is set, ignoring Link's position, for bodies that must end up square.
    { ACTOR_OBJ_OSHIHIKI, PACCI_UH_TRAIT_NO_FACE, 0, 0, NULL },
    { ACTOR_EN_AM, PACCI_UH_TRAIT_NO_FACE, 0, 0, NULL },

    // Red ice only checks the hit actor's id against En_Ice_Hono; carrying the flame is the only door.
    { ACTOR_EN_ICE_HONO, PACCI_UH_TRAIT_REACHABLE | PACCI_UH_TRAIT_PROXY, 0, 0, Pacci_UhCondPersistent, 0,
      ACTOR_EN_ICE_HONO, (s16)0xFFFF },

    // NO_FACE: she keeps her own facing while carried, like a person, not a crate.
    { ACTOR_EN_RU1, PACCI_UH_TRAIT_REACHABLE | PACCI_UH_TRAIT_NO_FACE, 0, 0, Pacci_UhCondRutoSitting },

    // DMG_BOOMERANG only, health 4; not En_Bx, the electrified tentacle with no death at all.
    { ACTOR_EN_BA, PACCI_UH_TRAIT_STRIKES, 0, 0, NULL, 0, 0, 0, DMG_BOOMERANG, PACCI_UH_CUT_DAMAGE },

    // world.pos.y rebuilds every frame from currentHeight; targetHeight is the only real handle.
    { ACTOR_EN_SIOFUKI, PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_HEIGHT, 0, 0, NULL },

    // Writes destAngle and points the actor at its own rise/fall function; the bridge does the rest.
    { ACTOR_BG_SPOT00_HANEBASI, PACCI_UH_TRAIT_LOCKED | PACCI_UH_TRAIT_HINGE, 0, 0, Pacci_UhCondDrawbridge },

    { ACTOR_BG_HAKA_ZOU, PACCI_UH_TRAIT_PULLABLE, 0, 0, NULL },
    { ACTOR_BG_PO_EVENT, PACCI_UH_TRAIT_PULLABLE, 0, 0, NULL },
    { ACTOR_BG_HIDAN_DALM, PACCI_UH_TRAIT_PULLABLE, 0, 0, NULL },
};

static const PacciUhTraitRow* Pacci_UhTraitRow(Actor* actor) {
    if (actor != NULL) {
        for (u32 i = 0; i < ARRAY_COUNT(sPacciUhTraits); i++) {
            if (sPacciUhTraits[i].actorId != actor->id) {
                continue;
            }
            // One id can have several rows, told apart by cond, so a false one keeps searching.
            if ((sPacciUhTraits[i].cond != NULL) && !sPacciUhTraits[i].cond(actor)) {
                continue;
            }
            return &sPacciUhTraits[i];
        }
    }
    return NULL;
}

static u32 Pacci_UhTraits(Actor* actor) {
    const PacciUhTraitRow* row = Pacci_UhTraitRow(actor);

    return (row != NULL) ? row->traits : 0;
}

// Horizontal extent only, from the CollisionHeader bounds: a tall totem is still an object.
static u8 Pacci_UhTooBig(Actor* actor) {
    PlayState* play = gPlayState;
    s32 i;

    if ((play == NULL) || (actor == NULL)) {
        return 0;
    }
    for (i = 0; i < play->colCtx.dyna.bgActorMax; i++) {
        BgActor* bg = &play->colCtx.dyna.bgActors[i];
        CollisionHeader* hdr;
        f32 halfX;
        f32 halfZ;

        if ((bg->actor != actor) || (bg->colHeader == NULL)) {
            continue;
        }
        hdr = bg->colHeader;
        halfX = ((f32)hdr->maxBounds.x - (f32)hdr->minBounds.x) * 0.5f * actor->scale.x;
        halfZ = ((f32)hdr->maxBounds.z - (f32)hdr->minBounds.z) * 0.5f * actor->scale.z;
        return ((halfX > PACCI_UH_MAX_HALF) || (halfZ > PACCI_UH_MAX_HALF)) ? 1 : 0;
    }
    return 0;
}

static u8 Pacci_UhIsStructure(Actor* actor) {
    return (Pacci_UhTraits(actor) & PACCI_UH_TRAIT_EXCLUDE) ? 1 : 0;
}

// Everything defaults to THROW; the table only lists actors that need something else.
typedef enum {
    PACCI_BEHAV_THROW = 0,  // flies, damages what it hits, breaks on impact
    PACCI_BEHAV_SNAP_FLOOR, // released onto the floor, and onto a switch if one is underneath
    PACCI_BEHAV_PLACE,      // stays exactly where released, no gravity
    PACCI_BEHAV_CARRY_ONLY, // movable, but its own logic keeps running (live bombs)
} PacciBehaviour;

typedef struct {
    s16 actorId;
    u8 behaviour;
} PacciBehaviourRow;

static const PacciBehaviourRow sPacciBehaviours[] = {
    { ACTOR_OBJ_OSHIHIKI, PACCI_BEHAV_SNAP_FLOOR },
    { ACTOR_OBJ_LIFT, PACCI_BEHAV_SNAP_FLOOR },
    { ACTOR_EN_BOM, PACCI_BEHAV_CARRY_ONLY },
    { ACTOR_EN_BOMBF, PACCI_BEHAV_PLACE }, // takes the plant, so it can be replanted elsewhere
};

u8 Pacci_BehaviourFor(Actor* actor) {
    if (actor != NULL) {
        for (u32 i = 0; i < ARRAY_COUNT(sPacciBehaviours); i++) {
            if (actor->id == sPacciBehaviours[i].actorId) {
                return sPacciBehaviours[i].behaviour;
            }
        }
    }
    return PACCI_BEHAV_THROW;
}

// isDyna: the actor came from DynaPoly_GetActor, so it owns collision and the draw gate is skipped.
u8 Pacci_IsLiftableEx(Actor* actor, u8 isDyna) {
    if ((actor == NULL) || (actor->update == NULL)) {
        return 0;
    }
    if ((actor->id == ACTOR_PLAYER) || (actor->category == ACTORCAT_BOSS)) {
        return 0;
    }
    // A REACHABLE row outranks every generic rule below (En_Ru1 is an NPC, En_Ice_Hono has no collision).
    if (Pacci_UhTraits(actor) & PACCI_UH_TRAIT_REACHABLE) {
        return 1;
    }

    // NPCs are refused by category; everything else that is invisible is filtered by the draw check.
    if (!(actor->flags & PACCI_FLAG_LIFTABLE) && (actor->category == ACTORCAT_NPC)) {
        return 0;
    }
    // Only on enemies: props such as Obj_Oshihiki set MASS_IMMOVABLE themselves.
    if ((actor->category == ACTORCAT_ENEMY) && (actor->colChkInfo.mass == MASS_IMMOVABLE)) {
        return 0;
    }
    // A trait row outranks the blacklist: En_Am is on both, and must stay a switch weight.
    if (Pacci_UhTraits(actor) == 0) {
        for (u32 i = 0; i < ARRAY_COUNT(sPacciBlacklist); i++) {
            if (actor->id == sPacciBlacklist[i]) {
                return 0;
            }
        }
    }
    if (Pacci_UhIsStructure(actor)) {
        return 0;
    }
    // A row's opinion overrides the size gate (e.g. a lift is deliberately large but movable).
    if ((Pacci_UhTraits(actor) == 0) && Pacci_UhTooBig(actor)) {
        return 0;
    }
    // No draw means an invisible trigger, except on dynapoly, where the room mesh draws it.
    if (!isDyna && (actor->draw == NULL)) {
        return 0;
    }
    // Glued parts stay targetable; the grab redirects to the root (Pacci_FuseRootOf).
    return 1;
}

u8 Pacci_IsLiftable(Actor* actor) {
    return Pacci_IsLiftableEx(actor, 0);
}

typedef struct {
    u8 active;
    f32 heightOff; // vertical offset from the aim line, driven by R + D-up/down
    f32 sideOff;   // sideways offset, perpendicular to the aim, from R + D-left/right
    u8 prevDpad;
    // Counts direction flips of the stick, not raw deflection, to tell a shake from just walking.
    s8 wiggleDir;
    u8 wiggleFlips;
    s16 wiggleTimer;
    s16 summonHold;
} PacciUhMode;

static PacciUhMode sUhMode = { 0, 0.0f, 0 };

typedef struct {
    Actor* held;
    u8 dropping;
    s16 dropTimer;
    f32 distance;
    f32 origGravity;
    f32 origMinVelocityY;
    s16 origRoom;
    // Pose kept relative to the object-to-Link line, so the same face stays toward him.
    Vec3s baseRot;
    Vec3s grabRot; // orientation at the grab, for the Z reset
    // For some actors world.rot is a heading (Bg_Haka_Ship, Bg_Hidan_Rock): saved and given back on release.
    Vec3s grabWorldRot;
    s16 faceOffsetYaw; // grabbed facing minus object->player yaw
    // Chases the aim at a capped rate: it sweeps an arc instead of cutting a chord through Link.
    s16 carryYaw;
    Vec3f carryVel; // smoothed carry velocity, handed over as release velocity
    // railPos is the grab pose, not the spawn pose, so a slid block keeps its plane.
    u32 traits;
    s8 flagDir;
    Vec3f railPos;
    s32 pathId;
    s32 pathCount;
    // Where the controls want it, before the magnet: a slid block can still be pulled back off.
    Vec3f anchorPos;
    f32 placeBlend; // 0 = controls own the body, 1 = the offered spot does; ramps, doesn't snap
    s16 vfxAge;
    // Held objects are frozen; their own update still submits the OC collider that shoves Link.
    ActorFunc origUpdate;
} PacciUltrahand;

static PacciUltrahand sUltrahand = { 0 };
static Actor* sUhProxy = NULL;
static Actor* sUhHighlightTarget = NULL;

// nextModelGroup is cached on item-action change, so grabbing/releasing must re-latch explicitly.
s32 Player_ActionToModelGroup(Player* this, s32 actionParam);
void Player_SetModels(Player* this, s32 modelGroup);

// Grayscale is RSP state, so an actor's own materials cannot undo it mid-list; applied by swapping draw.
#define PACCI_UH_TINT_SLOTS 8
#define PACCI_UH_TINT_R 110
#define PACCI_UH_TINT_G 255
#define PACCI_UH_TINT_B 165
#define PACCI_UH_TINT_MIX 255

#define PACCI_UH_FLOW_HALO_R 30
#define PACCI_UH_FLOW_HALO_G 235
#define PACCI_UH_FLOW_HALO_B 165
#define PACCI_UH_FLOW_MID_R 120
#define PACCI_UH_FLOW_MID_G 255
#define PACCI_UH_FLOW_MID_B 200
#define PACCI_UH_FLOW_CORE_R 225
#define PACCI_UH_FLOW_CORE_G 255
#define PACCI_UH_FLOW_CORE_B 240

static struct {
    Actor* actor;
    ActorFunc origDraw;
} sUhTint[PACCI_UH_TINT_SLOTS];
static u8 sUhTintCount = 0;

static ActorFunc Pacci_UhTintFind(Actor* actor) {
    for (u8 i = 0; i < sUhTintCount; i++) {
        if (sUhTint[i].actor == actor) {
            return sUhTint[i].origDraw;
        }
    }
    return NULL;
}

static void Pacci_UhTintedDraw(Actor* thisx, PlayState* play) {
    ActorFunc orig = Pacci_UhTintFind(thisx);

    if (orig == NULL) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    // Both buffers: which one an actor draws into is its own business, and some use both.
    gDPSetGrayscaleColor(POLY_OPA_DISP++, PACCI_UH_TINT_R, PACCI_UH_TINT_G, PACCI_UH_TINT_B, PACCI_UH_TINT_MIX);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, PACCI_UH_TINT_R, PACCI_UH_TINT_G, PACCI_UH_TINT_B, PACCI_UH_TINT_MIX);
    gSPGrayscale(POLY_XLU_DISP++, true);
    CLOSE_DISPS(play->state.gfxCtx);

    orig(thisx, play);

    // Turn it off again, or every actor drawn after this one in the same pass comes out green.
    OPEN_DISPS(play->state.gfxCtx);
    gSPGrayscale(POLY_OPA_DISP++, false);
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Pacci_UhTintClear(void) {
    for (u8 i = 0; i < sUhTintCount; i++) {
        Actor* actor = sUhTint[i].actor;

        if ((actor != NULL) && (actor->update != NULL) && (actor->draw == Pacci_UhTintedDraw)) {
            actor->draw = sUhTint[i].origDraw;
        }
        sUhTint[i].actor = NULL;
        sUhTint[i].origDraw = NULL;
    }
    sUhTintCount = 0;
}

static void Pacci_UhTintAdd(Actor* actor) {
    if ((actor == NULL) || (actor->update == NULL) || (actor->draw == NULL) || (actor->draw == Pacci_UhTintedDraw) ||
        (sUhTintCount >= PACCI_UH_TINT_SLOTS)) {
        return;
    }
    sUhTint[sUhTintCount].actor = actor;
    sUhTint[sUhTintCount].origDraw = actor->draw;
    sUhTintCount++;
    actor->draw = Pacci_UhTintedDraw;
}

static void Pacci_RefreshPlayerPose(Player* player) {
    if (player != NULL) {
        Player_SetModels(player, Player_ActionToModelGroup(player, player->itemAction));
    }
}

// Colliders are pinned to owner pos + offset from the owner's own update, when they are in the lists.
#define PACCI_ANCHOR_OWNERS 8
#define PACCI_ANCHOR_COLS 10

typedef struct {
    Collider* col;
    Vec3f offset;
    Vec3f lastSet;
    u8 hasLastSet;
} PacciAnchor;

typedef struct {
    Actor* owner;
    // Taken at the grab: collection can lag a frame behind the actor having moved.
    Vec3f basePos;
    u8 active;
    u8 count;
    PacciAnchor cols[PACCI_ANCHOR_COLS];
} PacciAnchorSlot;

static PacciAnchorSlot sUhAnchor[PACCI_ANCHOR_OWNERS];

static PacciAnchorSlot* Pacci_AnchorFind(Actor* actor) {
    for (u8 i = 0; i < PACCI_ANCHOR_OWNERS; i++) {
        if (sUhAnchor[i].active && (sUhAnchor[i].owner == actor)) {
            return &sUhAnchor[i];
        }
    }
    return NULL;
}

static void Pacci_AnchorTake(Actor* actor) {
    PacciAnchorSlot* slot;

    if (actor == NULL) {
        return;
    }
    slot = Pacci_AnchorFind(actor);
    if (slot == NULL) {
        for (u8 i = 0; i < PACCI_ANCHOR_OWNERS; i++) {
            if (!sUhAnchor[i].active) {
                slot = &sUhAnchor[i];
                break;
            }
        }
    }
    if (slot == NULL) {
        return;
    }
    slot->owner = actor;
    slot->active = 1;
    slot->count = 0;
    slot->basePos = actor->world.pos;
}

static void Pacci_AnchorRelease(Actor* actor) {
    PacciAnchorSlot* slot = Pacci_AnchorFind(actor);

    if (slot != NULL) {
        slot->active = 0;
        slot->owner = NULL;
        slot->count = 0;
    }
}

static void Pacci_AnchorClear(void) {
    for (u8 i = 0; i < PACCI_ANCHOR_OWNERS; i++) {
        sUhAnchor[i].active = 0;
        sUhAnchor[i].owner = NULL;
        sUhAnchor[i].count = 0;
    }
}

static void Pacci_AnchorCollect(PacciAnchorSlot* slot, Collider** list, s32 count) {
    for (s32 i = 0; (i < count) && (slot->count < PACCI_ANCHOR_COLS); i++) {
        Collider* col = list[i];
        Vec3f refPos;
        u8 seen = 0;

        if ((col == NULL) || (col->actor != slot->owner)) {
            continue;
        }
        // One collider can be registered as AT, AC and OC in the same frame; track it once.
        for (u8 j = 0; j < slot->count; j++) {
            if (slot->cols[j].col == col) {
                seen = 1;
                break;
            }
        }
        if (seen || !SwitchHook_GetColliderRefPos(col, &refPos)) {
            continue;
        }
        slot->cols[slot->count].col = col;
        slot->cols[slot->count].offset.x = refPos.x - slot->basePos.x;
        slot->cols[slot->count].offset.y = refPos.y - slot->basePos.y;
        slot->cols[slot->count].offset.z = refPos.z - slot->basePos.z;
        slot->cols[slot->count].hasLastSet = 0;
        slot->count++;
    }
}

// Must run from inside the owner's update, right after its own update.
static void Pacci_AnchorSync(PlayState* play, Actor* actor) {
    PacciAnchorSlot* slot;

    if ((play == NULL) || (actor == NULL) || (actor->update == NULL)) {
        return;
    }
    slot = Pacci_AnchorFind(actor);
    if (slot == NULL) {
        return;
    }
    Pacci_AnchorCollect(slot, play->colChkCtx.colAT, play->colChkCtx.colATCount);
    Pacci_AnchorCollect(slot, play->colChkCtx.colAC, play->colChkCtx.colACCount);
    Pacci_AnchorCollect(slot, play->colChkCtx.colOC, play->colChkCtx.colOCCount);

    for (u8 i = 0; i < slot->count; i++) {
        PacciAnchor* anchor = &slot->cols[i];
        Vec3f refPos;
        Vec3f delta;

        if (!SwitchHook_GetColliderRefPos(anchor->col, &refPos)) {
            continue;
        }
        // The owner moved it itself: adopt its answer instead of fighting it.
        if (anchor->hasLastSet &&
            ((refPos.x != anchor->lastSet.x) || (refPos.y != anchor->lastSet.y) || (refPos.z != anchor->lastSet.z))) {
            anchor->offset.x = refPos.x - actor->world.pos.x;
            anchor->offset.y = refPos.y - actor->world.pos.y;
            anchor->offset.z = refPos.z - actor->world.pos.z;
            anchor->lastSet = refPos;
            continue;
        }
        delta.x = (actor->world.pos.x + anchor->offset.x) - refPos.x;
        delta.y = (actor->world.pos.y + anchor->offset.y) - refPos.y;
        delta.z = (actor->world.pos.z + anchor->offset.z) - refPos.z;
        SwitchHook_ShiftCollider(anchor->col, &delta);
        // Where it really ended up: s16 rounding must not read as the owner moving it.
        if (SwitchHook_GetColliderRefPos(anchor->col, &anchor->lastSet)) {
            anchor->hasLastSet = 1;
        }
    }
}

// Some actors (En_Wood02) despawn when INSIDE_CULLING_VOLUME clears; a driven one must keep it.
static void Pacci_UhKeepOnScreen(Actor* actor) {
    if (actor != NULL) {
        actor->flags |= ACTOR_FLAG_INSIDE_CULLING_VOLUME;
    }
}

static void Pacci_UltrahandHeldUpdate(Actor* thisx, PlayState* play) {
    Vec3f pos = thisx->world.pos;
    Vec3s rot = thisx->shape.rot;
    Vec3s worldRot = thisx->world.rot;

    Pacci_UhKeepOnScreen(thisx);
    if (sUltrahand.origUpdate != NULL) {
        sUltrahand.origUpdate(thisx, play);
    }
    if (thisx->update == NULL) {
        return;
    }
    thisx->world.pos = pos;
    thisx->shape.rot = rot;
    thisx->world.rot = worldRot;
    // The wrapper stays through the fall: velocity is zeroed only while carried.
    if (!sUltrahand.dropping) {
        thisx->velocity.x = 0.0f;
        thisx->velocity.y = 0.0f;
        thisx->velocity.z = 0.0f;
    }
    Pacci_AnchorSync(play, thisx);
}

static void Pacci_UhLightOff(PlayState* play);
static void Pacci_UhFireTick(PlayState* play, Actor* actor);
static void Pacci_UhFireOff(void);
static void Pacci_UhIceTick(PlayState* play, Actor* actor);
static void Pacci_UhPlaceMagnet(Actor* actor);
static void Pacci_UhFlagTick(PlayState* play, Actor* actor);
static void Pacci_UhLockedInput(PlayState* play, u8 edge);
static void Pacci_UhLockedPose(PlayState* play, Actor* actor);
static void Pacci_UhHeightInput(Actor* actor, u8 edge);
static u8 Pacci_UhAiming(Player* player, Actor* actor);
static void Pacci_UhHingeInput(Actor* actor, u8 edge);
// z_bg_spot00_hanebasi.c, not static and not in any header.
void BgSpot00Hanebasi_DrawbridgeRiseAndFall(BgSpot00Hanebasi* this, PlayState* play);
// Not in functions.h - it is Player's own.
extern int Player_IsZTargeting(Player* this);
static void Pacci_UhBombTick(Actor* actor);
static u8 Pacci_UhBombDetonate(PlayState* play);
static void Pacci_BackRiderTake(PlayState* play, Player* player, Actor* rider);

u8 Pacci_IsHoldingUltrahand(void) {
    return (sUltrahand.held != NULL) && !sUltrahand.dropping;
}

Actor* Pacci_GetUltrahandHeld(void) {
    return sUltrahand.held;
}

static void Pacci_UltrahandLetGo(void) {
    Actor* actor = sUltrahand.held;

    if (actor != NULL && actor->update != NULL) {
        if (sUltrahand.origUpdate != NULL) {
            actor->update = sUltrahand.origUpdate;
        }
        actor->gravity = sUltrahand.origGravity;
        actor->minVelocityY = sUltrahand.origMinVelocityY;
        actor->velocity.y = 0.0f;
        actor->room = (s8)sUltrahand.origRoom;
        actor->colorFilterParams = 0;
        actor->world.rot = sUltrahand.grabWorldRot;
    }
    // Stays fused: most of these actors have no gravity and would hang where released.
    Pacci_AnchorRelease(actor);
    // Killed after its update/flags are restored, so it dies as itself.
    if ((actor != NULL) && (actor == sUhProxy)) {
        if (actor->update != NULL) {
            Actor_Kill(actor);
        }
        sUhProxy = NULL;
    }
    sUltrahand.held = NULL;
    sUltrahand.dropping = 0;
    sUltrahand.dropTimer = 0;
    sUltrahand.origUpdate = NULL;
    sUltrahand.vfxAge = 0;
    sUhHighlightTarget = NULL;
    Pacci_UhFireOff();
    Pacci_UhLightOff(gPlayState);
    Pacci_UhTintClear();
    Pacci_RefreshPlayerPose(GET_PLAYER(gPlayState));
}

// Every way of letting go ends in a fall, whatever Link is holding by then.
void Pacci_UltrahandDropTick(PlayState* play) {
    if ((play == NULL) || (sUltrahand.held == NULL) || !sUltrahand.dropping) {
        return;
    }
    Pacci_UpdateUltrahand(play, GET_PLAYER(play));
}

// Armed: hand out, target marked, weld offered. The mode only adds the D-pad controls.
static u8 sUhArmed = 0;

void Pacci_SetUltrahandArmed(u8 armed) {
    // The one wipe: held body, candidate and weld ends are tinted from three places later.
    Pacci_UhTintClear();
    sUhArmed = armed;
}

u8 Pacci_UltrahandArmed(void) {
    return sUhArmed;
}

void Pacci_HighlightUltrahandTarget(PlayState* play) {
    sUhHighlightTarget = NULL;
    // held, not IsHolding: a falling body still blocks a new candidate.
    if (sUltrahand.held != NULL) {
        return;
    }
    // Cleared after the early return, or the held body's tint would be wiped outside the mode.
    Pacci_UhTintClear();

    Actor* target = Pacci_ResolveUltrahandTarget(play, GET_PLAYER(play));

    if (target != NULL) {
        target->colorFilterParams = 0;
        Pacci_UhTintAdd(target);
        sUhHighlightTarget = target;
    }
}

// The DynaPoly actor owning the surface on Link's aim, or NULL for scenery.
static Actor* Pacci_UltrahandRaycastAt(PlayState* play, Player* player, s16 pitchBias) {
    Vec3f from = player->actor.world.pos;
    Vec3f to;
    Vec3f hit;
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;
    s16 pitch = player->actor.focus.rot.x + pitchBias;
    f32 distXZ = Math_CosS(pitch) * PACCI_UH_DIST_MAX;
    f32 distY = Math_SinS(pitch) * PACCI_UH_DIST_MAX;

    from.y += 40.0f; // from the chest, not the feet
    to.x = from.x + (Math_SinS(player->actor.focus.rot.y) * distXZ);
    to.z = from.z + (Math_CosS(player->actor.focus.rot.y) * distXZ);
    to.y = from.y - distY;

    // ProjectileLineTest reports the bg actor behind a surface; EntityLineTest1 does not.
    for (s32 attempt = 0; attempt < 4; attempt++) {
        DynaPolyActor* dyna;
        Actor* found;
        u8 ours;
        f32 dx;
        f32 dy;
        f32 dz;
        f32 len;

        if (!BgCheck_ProjectileLineTest(&play->colCtx, &from, &to, &hit, &poly, true, true, true, true, &bgId)) {
            break;
        }
        dyna = DynaPoly_GetActor(&play->colCtx, bgId);
        found = ((dyna != NULL) && (dyna->actor.update != NULL)) ? &dyna->actor : NULL;

        if (found == NULL) {
            break; // plain scenery: whatever is behind it is behind a wall
        }
        // held == NULL first, or NULL != NULL rejects every surface while empty-handed.
        ours =
            (found == sUltrahand.held) || ((sUltrahand.held != NULL) && (Pacci_FuseRootOf(found) == sUltrahand.held));
        if (!ours) {
            if (Pacci_IsLiftableEx(found, 1)) {
                return found;
            }
            // Step past an unliftable surface instead of stopping, or it hides everything behind it.
        }

        dx = to.x - from.x;
        dy = to.y - from.y;
        dz = to.z - from.z;
        len = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
        if (len < 1.0f) {
            break;
        }
        from.x = hit.x + ((dx / len) * 3.0f);
        from.y = hit.y + ((dy / len) * 3.0f);
        from.z = hit.z + ((dz / len) * 3.0f);
    }
    return NULL;
}

// Sweeps the pitch downward: the camera cannot aim at things by Link's feet.
static Actor* Pacci_UltrahandRaycastDyna(PlayState* play, Player* player) {
    static const s16 sPitchSweep[] = { 0, 0x0A00, 0x1600, -0x0800 };

    for (u8 i = 0; i < ARRAY_COUNT(sPitchSweep); i++) {
        Actor* found = Pacci_UltrahandRaycastAt(play, player, sPitchSweep[i]);

        if (found != NULL) {
            return found;
        }
    }
    return NULL;
}

// Grab and highlight both ask here, so what is marked is what A takes.
static s32 Pacci_UhTargetFilter(Actor* actor) {
    return TargetSelect_IsCommonTarget(actor) && Pacci_IsLiftable(actor);
}

// REACHABLE rows only, within arm's reach: they sit where the camera cannot aim.
static Actor* Pacci_UhScanReachable(PlayState* play, Player* player) {
    Actor* best = NULL;
    f32 bestD = PACCI_UH_REACH_RANGE * PACCI_UH_REACH_RANGE;
    s32 cat;

    for (cat = 0; cat < ACTORCAT_MAX; cat++) {
        Actor* it;

        for (it = play->actorCtx.actorLists[cat].head; it != NULL; it = it->next) {
            f32 dx;
            f32 dy;
            f32 dz;
            f32 d;
            s16 yawOff;

            if ((it->update == NULL) || !(Pacci_UhTraits(it) & PACCI_UH_TRAIT_REACHABLE)) {
                continue;
            }
            dx = it->world.pos.x - player->actor.world.pos.x;
            dy = it->world.pos.y - (player->actor.world.pos.y + 20.0f);
            dz = it->world.pos.z - player->actor.world.pos.z;
            d = (dx * dx) + (dy * dy) + (dz * dz);
            if (d >= bestD) {
                continue;
            }
            // s16 subtraction already wraps to the shortest signed difference.
            yawOff = Math_Vec3f_Yaw(&player->actor.world.pos, &it->world.pos) - player->actor.focus.rot.y;
            if ((yawOff > PACCI_UH_REACH_CONE) || (yawOff < -PACCI_UH_REACH_CONE)) {
                continue;
            }
            bestD = d;
            best = it;
        }
    }
    return best;
}

static Actor* Pacci_ResolveUltrahandTarget(PlayState* play, Player* player) {
    Actor* target = Pacci_UltrahandRaycastDyna(play, player);

    if (target == NULL) {
        target = TargetSelect_Scan(play, Pacci_UhTargetFilter);
    }
    // Last, so nothing that already resolved starts resolving to a flame at your feet instead.
    if (target == NULL) {
        target = Pacci_UhScanReachable(play, player);
    }
    return target;
}

static void Pacci_UltrahandBeginDrop(void) {
    Actor* actor = sUltrahand.held;
    f32 speed;

    if ((actor == NULL) || sUltrahand.dropping) {
        return;
    }
    // Constrained and LOCKED bodies stay put on release: a fall would break the axis they are pinned to.
    if (sUltrahand.traits &
        (PACCI_UH_TRAIT_AXIS_Y | PACCI_UH_TRAIT_PLANE_XZ | PACCI_UH_TRAIT_PATH | PACCI_UH_TRAIT_LOCKED)) {
        Pacci_UltrahandLetGo();
        Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    sUltrahand.dropping = 1;
    sUltrahand.dropTimer = PACCI_UH_DROP_TIMEOUT;
    actor->colorFilterParams = 0;

    // Letting go while aiming is a throw, not a drop, and overrides carry inertia entirely.
    {
        Player* player = (gPlayState != NULL) ? GET_PLAYER(gPlayState) : NULL;

        if ((player != NULL) && Pacci_UhAiming(player, actor)) {
            Actor* mark = player->focusActor;
            s16 yaw = (mark != NULL) ? Math_Vec3f_Yaw(&actor->world.pos, &mark->world.pos) : player->actor.shape.rot.y;

            sUltrahand.carryVel.x = Math_SinS(yaw) * PACCI_UH_THROW_SPEED;
            sUltrahand.carryVel.z = Math_CosS(yaw) * PACCI_UH_THROW_SPEED;
            sUltrahand.carryVel.y = PACCI_UH_THROW_LIFT;
            LinkAnimation_PlayOnce(gPlayState, &player->upperSkelAnime, (void*)gPlayerAnim_link_boom_throwR);
            Audio_PlayActorSound2(actor, NA_SE_IT_BOOMERANG_THROW);
        }
    }
    actor->gravity = PACCI_UH_DROP_GRAVITY;
    actor->minVelocityY = PACCI_UH_DROP_MIN_VEL_Y;
    actor->velocity = sUltrahand.carryVel;

    speed = sqrtf((actor->velocity.x * actor->velocity.x) + (actor->velocity.y * actor->velocity.y) +
                  (actor->velocity.z * actor->velocity.z));
    if (speed > PACCI_UH_THROW_MAX) {
        f32 scale = PACCI_UH_THROW_MAX / speed;

        actor->velocity.x *= scale;
        actor->velocity.y *= scale;
        actor->velocity.z *= scale;
    }
    Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// With Z held a plain object rides in the hand to be thrown; trait rows have a place to be.
static u8 Pacci_UhThrowable(Actor* actor) {
    if (actor == NULL) {
        return 0;
    }
    return ((sUltrahand.traits == 0) || (sUltrahand.traits & PACCI_UH_TRAIT_EXPLODES)) ? 1 : 0;
}

static u8 Pacci_UhAiming(Player* player, Actor* actor) {
    return (Player_IsZTargeting(player) && Pacci_UhThrowable(actor)) ? 1 : 0;
}

// Too big to carry: Link braces with the vanilla pull animation and the D-pad drags it.
static Actor* sPullActor = NULL;
static u8 sPullStarted = 0;

u8 Pacci_PullActive(void) {
    return (sPullActor != NULL) ? 1 : 0;
}

void Pacci_PullStop(PlayState* play) {
    Player* player = (play != NULL) ? GET_PLAYER(play) : NULL;

    if ((sPullActor != NULL) && (player != NULL) && sPullStarted) {
        LinkAnimation_Change(play, &player->skelAnime, (void*)gPlayerAnim_link_normal_pull_end, 1.0f, 0.0f,
                             Animation_GetLastFrame((void*)gPlayerAnim_link_normal_pull_end), ANIMMODE_ONCE, -6.0f);
    }
    sPullActor = NULL;
    sPullStarted = 0;
}

static void Pacci_PullStart(PlayState* play, Player* player, Actor* target) {
    sPullActor = target;
    sPullStarted = 0;
    LinkAnimation_Change(play, &player->skelAnime, (void*)gPlayerAnim_link_normal_pull_start, 1.0f, 0.0f,
                         Animation_GetLastFrame((void*)gPlayerAnim_link_normal_pull_start), ANIMMODE_ONCE, -6.0f);
    Audio_PlayActorSound2(target, NA_SE_PL_PULL_UP_BIGROCK);
}

// Pad state, not edges, since a pull is a sustained effort. D-down toward Link, D-up away.
static void Pacci_PullTick(PlayState* play, Player* player, u8 dpad) {
    f32 dx;
    f32 dz;
    f32 len;
    f32 step = 0.0f;

    if ((sPullActor == NULL) || (player == NULL)) {
        return;
    }
    if (sPullActor->update == NULL) {
        Pacci_PullStop(play);
        return;
    }
    // Asks the animation, not a frame count, so playback speed cannot desync the loop.
    if (!sPullStarted) {
        if (LinkAnimation_Update(play, &player->skelAnime)) {
            sPullStarted = 1;
            LinkAnimation_Change(play, &player->skelAnime, (void*)gPlayerAnim_link_normal_pulling, 1.0f, 0.0f,
                                 Animation_GetLastFrame((void*)gPlayerAnim_link_normal_pulling), ANIMMODE_LOOP, -4.0f);
        }
    } else {
        LinkAnimation_Update(play, &player->skelAnime);
    }
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;

    if (dpad & 2) {
        step = -PACCI_UH_PULL_RATE;
    } else if (dpad & 1) {
        step = PACCI_UH_PULL_RATE;
    } else {
        return;
    }
    dx = sPullActor->world.pos.x - player->actor.world.pos.x;
    dz = sPullActor->world.pos.z - player->actor.world.pos.z;
    len = sqrtf((dx * dx) + (dz * dz));
    if (len < 1.0f) {
        return;
    }
    sPullActor->world.pos.x += (dx / len) * step;
    sPullActor->world.pos.z += (dz / len) * step;
    // Bg_Po_Event and Bg_Hidan_Dalm both rebuild their position from home, so home comes along.
    sPullActor->home.pos.x = sPullActor->world.pos.x;
    sPullActor->home.pos.z = sPullActor->world.pos.z;
    sPullActor->prevPos = sPullActor->world.pos;
    Pacci_AnchorSync(play, sPullActor);
}

// Bg_Heavy_Block's own throw is gated on actor->parent: setting and clearing it runs vanilla's.
static Actor* sUhThrowActor = NULL;
static s16 sUhThrowTimer = 0;

void Pacci_ThrowTick(PlayState* play) {
    if ((play == NULL) || (sUhThrowActor == NULL)) {
        return;
    }
    if (sUhThrowActor->update == NULL) {
        sUhThrowActor = NULL;
        sUhThrowTimer = 0;
        return;
    }
    if (sUhThrowTimer > 0) {
        sUhThrowTimer--;
        return; // still "lifted"; its Wait state is running the pickup
    }
    // BgHeavyBlock_Fly never sets its own speed; vanilla's throw (z_player.c) does, with these.
    {
        Player* player = GET_PLAYER(play);

        sUhThrowActor->world.rot.y = player->actor.shape.rot.y;
        sUhThrowActor->speedXZ = 10.0f;
        sUhThrowActor->velocity.y = 20.0f;
    }
    sUhThrowActor->parent = NULL;
    sUhThrowActor = NULL;
}

u8 Pacci_IsThrowing(void) {
    return (sUhThrowActor != NULL) ? 1 : 0;
}

static void Pacci_ThrowArm(Player* player, Actor* target) {
    // A few frames, so its own update sees the parent whatever the update order.
    target->parent = &player->actor;
    sUhThrowActor = target;
    sUhThrowTimer = PACCI_UH_THROW_HOLD;
}

// A real hit, so the target's own damage path runs; kept a few frames since AT meets AC in one frame.
static ColliderCylinder sUhCutCol;
static Actor* sUhCutTarget = NULL;
static s16 sUhCutTimer = 0;
static u32 sUhCutFlags = 0;
static s16 sUhCutDamage = 0;

void Pacci_CutTick(PlayState* play) {
    CombatColliderConfig cfg;
    Vec3f pos;

    if ((play == NULL) || (sUhCutTarget == NULL) || (sUhCutTimer <= 0)) {
        return;
    }
    if (sUhCutTarget->update == NULL) {
        sUhCutTarget = NULL;
        sUhCutTimer = 0;
        return;
    }
    sUhCutTimer--;
    cfg.dmgFlags = sUhCutFlags;
    cfg.damage = sUhCutDamage;
    cfg.effect = 0;
    cfg.radius = PACCI_UH_CUT_RADIUS;
    cfg.height = PACCI_UH_CUT_HEIGHT;
    pos = sUhCutTarget->world.pos;
    Combat_UpdateCylinder(&sUhCutCol, &pos, &cfg);
    Combat_RegisterCollider(play, &sUhCutCol);
    if (Combat_CheckHit(&sUhCutCol)) {
        sUhCutCol.base.atFlags &= ~AT_HIT;
    }
}

static void Pacci_CutArm(PlayState* play, Player* player, Actor* target, const PacciUhTraitRow* row) {
    CombatColliderConfig cfg;

    sUhCutFlags = row->hitFlags;
    sUhCutDamage = (row->hitDamage != 0) ? row->hitDamage : PACCI_UH_CUT_DAMAGE;
    cfg.dmgFlags = sUhCutFlags;
    cfg.damage = sUhCutDamage;
    cfg.effect = 0;
    cfg.radius = PACCI_UH_CUT_RADIUS;
    cfg.height = PACCI_UH_CUT_HEIGHT;
    // Owned by Link, so the target reacts to a player's boomerang hit.
    Combat_InitCylinder(play, &sUhCutCol, &player->actor, &cfg);
    sUhCutTarget = target;
    sUhCutTimer = PACCI_UH_CUT_FRAMES;
    Audio_PlayActorSound2(target, NA_SE_IT_BOOMERANG_THROW);
}

// Through the struct: z_en_bom.h's offset comments are N64 ones, wrong with 64-bit pointers.
static s16* Pacci_UhBombTimer(Actor* actor) {
    if ((actor == NULL) || (actor->id != ACTOR_EN_BOM)) {
        return NULL;
    }
    return &((EnBom*)actor)->timer;
}

// Re-asserted every frame, since En_Bom's own update keeps counting the fuse down.
static void Pacci_UhBombTick(Actor* actor) {
    s16* timer;

    if (!(sUltrahand.traits & PACCI_UH_TRAIT_EXPLODES)) {
        return;
    }
    timer = Pacci_UhBombTimer(actor);
    if (timer != NULL) {
        *timer = 60;
    }
}

static u8 Pacci_UhBombDetonate(PlayState* play) {
    Actor* actor = sUltrahand.held;
    s16* timer;

    if (!(sUltrahand.traits & PACCI_UH_TRAIT_EXPLODES) || (actor == NULL)) {
        return 0;
    }
    timer = Pacci_UhBombTimer(actor);
    if (timer == NULL) {
        return 0;
    }
    *timer = 1;
    sUhProxy = NULL; // it is going to kill itself; do not kill it out from under the explosion
    Pacci_UltrahandLetGo();
    return 1;
}

static void Pacci_UltrahandTake(Player* player, Actor* target) {
    f32 dx;

    // One assembly at a time: grabbing outside it releases the old one first.
    if ((Pacci_FuseCount() > 0) && (Pacci_FuseRootOf(target) != target)) {
        Pacci_FuseRelease();
    }

    dx = target->world.pos.x - player->actor.world.pos.x;
    f32 dy = target->world.pos.y - player->actor.world.pos.y;
    f32 dz = target->world.pos.z - player->actor.world.pos.z;

    sUltrahand.held = target;
    sUltrahand.dropping = 0;
    sUltrahand.dropTimer = 0;
    sUltrahand.origGravity = target->gravity;
    sUltrahand.origMinVelocityY = target->minVelocityY;
    sUltrahand.origRoom = target->room;
    sUltrahand.baseRot = target->shape.rot;
    sUltrahand.grabRot = target->shape.rot;
    sUltrahand.grabWorldRot = target->world.rot;
    sUltrahand.carryYaw = player->actor.focus.rot.y;
    sUltrahand.carryVel.x = 0.0f;
    sUltrahand.carryVel.y = 0.0f;
    sUltrahand.carryVel.z = 0.0f;
    sUltrahand.traits = Pacci_UhTraits(target);
    {
        const PacciUhTraitRow* row = Pacci_UhTraitRow(target);

        sUltrahand.flagDir = (row != NULL) ? row->flagDir : 0;
    }
    sUltrahand.railPos = target->world.pos;
    sUltrahand.pathId = 0;
    sUltrahand.pathCount = 0;
    if (sUltrahand.traits & PACCI_UH_TRAIT_PATH) {
        const PacciUhTraitRow* row = Pacci_UhTraitRow(target);
        PlayState* play = gPlayState;

        sUltrahand.pathId = (target->params >> row->pathShift) & row->pathMask;
        // Not every scene has a path list: without one the constraint is dropped.
        if ((play != NULL) && (play->setupPathList != NULL)) {
            sUltrahand.pathCount = play->setupPathList[sUltrahand.pathId].count;
        }
        if (sUltrahand.pathCount < 2) {
            sUltrahand.traits &= ~PACCI_UH_TRAIT_PATH;
        }
    }
    sUltrahand.faceOffsetYaw = target->shape.rot.y - Math_Vec3f_Yaw(&target->world.pos, &player->actor.world.pos);
    sUltrahand.vfxAge = 0;
    sUhHighlightTarget = NULL;
    Pacci_AnchorTake(target); // while it is still at rest, before the carry moves it
    sUltrahand.origUpdate = target->update;
    target->update = Pacci_UltrahandHeldUpdate;
    sUltrahand.distance = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
    sUltrahand.distance = CLAMP(sUltrahand.distance, PACCI_UH_DIST_MIN, PACCI_UH_DIST_MAX);
    target->room = -1; // held objects should survive a room change

    Audio_PlayActorSound2(target, NA_SE_SY_GET_ITEM);
    Pacci_RefreshPlayerPose(player);
}

u8 Pacci_CastUltrahand(PlayState* play, Player* player) {
    if (Pacci_IsHoldingUltrahand()) {
        Pacci_UltrahandBeginDrop();
        return 1;
    }

    if (Pacci_BackRiderActive()) {
        Pacci_BackRiderDrop();
        Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return 1;
    }

    // A falling body still owns the slot: a new grab would freeze it mid-air.
    if (sUltrahand.held != NULL) {
        return 0;
    }

    // Dynapoly first: the category scan cannot see ACTORCAT_BG scenery.
    Actor* target = Pacci_ResolveUltrahandTarget(play, player);

    if (target == NULL) {
        return 0;
    }
    // Parts are frozen and driven from the root, so holding a part directly would move nothing.
    if (Pacci_FuseRootOf(target) != NULL) {
        target = Pacci_FuseRootOf(target);
    }

    // Sitting Ruto rides instead, since the carry only holds one thing at a time.
    if ((target->id == ACTOR_EN_RU1) && Pacci_UhCondRutoSitting(target)) {
        Pacci_BackRiderTake(play, player, target);
        return 1;
    }

    {
        const PacciUhTraitRow* row = Pacci_UhTraitRow(target);

        if ((row != NULL) && (row->traits & PACCI_UH_TRAIT_PULLABLE)) {
            Pacci_PullStart(play, player, target);
            return 1;
        }
        if ((row != NULL) && (row->traits & PACCI_UH_TRAIT_THROWS)) {
            Pacci_ThrowArm(player, target);
            return 1;
        }
        if ((row != NULL) && (row->traits & PACCI_UH_TRAIT_STRIKES)) {
            Pacci_CutArm(play, player, target, row);
            return 1;
        }
        if ((row != NULL) && (row->traits & PACCI_UH_TRAIT_PROXY)) {
            Actor* copy = Actor_Spawn(&play->actorCtx, play, row->proxyId, target->world.pos.x, target->world.pos.y,
                                      target->world.pos.z, 0, target->shape.rot.y, 0, row->proxyParams);

            if (copy == NULL) {
                return 0;
            }
            sUhProxy = copy;
            Pacci_UltrahandTake(player, copy);
            // Traits belong to the row, not the stand-in: a spawned En_Bom has no row of its own.
            sUltrahand.traits = row->traits;
            return 1;
        }
    }

    Pacci_UltrahandTake(player, target);
    return 1;
}

static u8 Pacci_UhGroundUnder(PlayState* play, Actor* actor, f32* outY);
static u8 Pacci_UhAssemblyGround(PlayState* play, Actor* root, f32* outY);

// Puts a constrained body back on its own axis/plane/path, after the carry places it freely.
static void Pacci_UhConstrain(PlayState* play, Actor* actor) {
    u32 traits = sUltrahand.traits;

    if (traits & PACCI_UH_TRAIT_AXIS_Y) {
        actor->world.pos.x = sUltrahand.railPos.x;
        actor->world.pos.z = sUltrahand.railPos.z;
        sUltrahand.carryVel.x = 0.0f;
        sUltrahand.carryVel.z = 0.0f;
        if (traits & PACCI_UH_TRAIT_DRIVE_HOME) {
            // The dragged-to height becomes the new floor; the actor's own update rides to it.
            actor->home.pos.y = actor->world.pos.y;
        }
    }
    if (traits & PACCI_UH_TRAIT_PLANE_XZ) {
        actor->world.pos.y = sUltrahand.railPos.y;
        sUltrahand.carryVel.y = 0.0f;
    }
    if ((traits & PACCI_UH_TRAIT_PATH) && (sUltrahand.pathCount >= 2)) {
        // Nearest point on the polyline, not the nearest waypoint, so it slides instead of jumping.
        Path* path = &play->setupPathList[sUltrahand.pathId];
        Vec3s* pts = (Vec3s*)SEGMENTED_TO_VIRTUAL(path->points);
        Vec3f want = actor->world.pos;
        Vec3f best = want;
        f32 bestD = 3.0e38f;
        s32 i;

        for (i = 0; i < (sUltrahand.pathCount - 1); i++) {
            Vec3f a;
            Vec3f b;
            Vec3f on;
            f32 abx;
            f32 aby;
            f32 abz;
            f32 len2;
            f32 t;
            f32 dx;
            f32 dy;
            f32 dz;
            f32 d;

            a.x = pts[i].x;
            a.y = pts[i].y;
            a.z = pts[i].z;
            b.x = pts[i + 1].x;
            b.y = pts[i + 1].y;
            b.z = pts[i + 1].z;
            abx = b.x - a.x;
            aby = b.y - a.y;
            abz = b.z - a.z;
            len2 = (abx * abx) + (aby * aby) + (abz * abz);
            if (len2 < 1.0f) {
                continue;
            }
            t = (((want.x - a.x) * abx) + ((want.y - a.y) * aby) + ((want.z - a.z) * abz)) / len2;
            t = CLAMP(t, 0.0f, 1.0f);
            on.x = a.x + (abx * t);
            on.y = a.y + (aby * t);
            on.z = a.z + (abz * t);
            dx = want.x - on.x;
            dy = want.y - on.y;
            dz = want.z - on.z;
            d = (dx * dx) + (dy * dy) + (dz * dz);
            if (d < bestD) {
                bestD = d;
                best = on;
            }
        }
        actor->world.pos = best;
    }
}

// Once per frame: double gravity punches through the floor before the probe sees it.
static u32 sUhLastTick = 0xFFFFFFFF;

void Pacci_UpdateUltrahand(PlayState* play, Player* player) {
    Actor* actor = sUltrahand.held;
    Input* input = &play->state.input[0];

    if (actor == NULL) {
        return;
    }
    if (sUhLastTick == play->gameplayFrames) {
        return;
    }
    sUhLastTick = play->gameplayFrames;
    if (actor->update == NULL) {
        sUltrahand.held = NULL;
        sUltrahand.dropping = 0;
        return;
    }

    if (!sUltrahand.dropping) {
        s16 playerFacingYaw;
        s16 targetFacingYaw;
        Vec3f prevWorld = actor->world.pos;
        // No D-pad here: the mode owns the buttons, or L + D-pad would fight its rotation.
        f32 distXZ = Math_CosS(player->actor.focus.rot.x) * sUltrahand.distance;
        f32 distY = Math_SinS(player->actor.focus.rot.x) * sUltrahand.distance;
        // sideOff runs perpendicular to the aim, so R + D-left/right slides across the view.
        s16 yawDelta = player->actor.focus.rot.y - sUltrahand.carryYaw;
        f32 sideX;
        f32 sideZ;
        f32 targetX;
        f32 targetZ;

        if (yawDelta > PACCI_UH_TURN_RATE) {
            yawDelta = PACCI_UH_TURN_RATE;
        } else if (yawDelta < -PACCI_UH_TURN_RATE) {
            yawDelta = -PACCI_UH_TURN_RATE;
        }
        sUltrahand.carryYaw += yawDelta;

        // From carryYaw, not the raw aim, so the anchor moves one capped step at a time.
        sideX = Math_SinS(sUltrahand.carryYaw + 0x4000) * sUhMode.sideOff;
        sideZ = Math_CosS(sUltrahand.carryYaw + 0x4000) * sUhMode.sideOff;
        targetX = (Math_SinS(sUltrahand.carryYaw) * distXZ) + player->actor.world.pos.x + sideX;
        targetZ = (Math_CosS(sUltrahand.carryYaw) * distXZ) + player->actor.world.pos.z + sideZ;
        f32 targetY = -distY + player->actor.world.pos.y + sUhMode.heightOff;

        f32 oldW = PACCI_UH_FOLLOW_WEIGHT;
        f32 newW = 1.0f - oldW;

        if (Pacci_UhAiming(player, actor)) {
            Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];

            targetX = hand.x + (Math_SinS(player->actor.shape.rot.y) * PACCI_UH_THROW_REACH);
            targetY = hand.y + PACCI_UH_THROW_RISE;
            targetZ = hand.z + (Math_CosS(player->actor.shape.rot.y) * PACCI_UH_THROW_REACH);
        }

        // Taken before easing, or the magnet-pulled position never reads as having moved.
        sUltrahand.anchorPos.x = targetX;
        sUltrahand.anchorPos.y = targetY;
        sUltrahand.anchorPos.z = targetZ;

        actor->world.pos.x = (actor->world.pos.x * oldW) + (targetX * newW) - actor->colChkInfo.displacement.x;
        actor->world.pos.y = (actor->world.pos.y * oldW) + (targetY * newW) - actor->colChkInfo.displacement.y;
        actor->world.pos.z = (actor->world.pos.z * oldW) + (targetZ * newW) - actor->colChkInfo.displacement.z;
        actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;

        Actor_UpdateBgCheckInfo(play, actor, 0.0f, 0.0f, 0.0f, 4);
        // Clamp, never nudge up: on a slope that ratchets a unit per frame into the sky.
        if ((actor->bgCheckFlags & BGCHECKFLAG_GROUND) && (actor->world.pos.y < actor->floorHeight)) {
            actor->world.pos.y = actor->floorHeight;
        }
        // Zeroed each frame so it can't accumulate while Link stands against what he's carrying.
        actor->colChkInfo.displacement.x = 0.0f;
        actor->colChkInfo.displacement.y = 0.0f;
        actor->colChkInfo.displacement.z = 0.0f;
        // Smoothed: the raw last-frame delta is either zero or a spike.
        sUltrahand.carryVel.x = (sUltrahand.carryVel.x * 0.65f) + ((actor->world.pos.x - prevWorld.x) * 0.35f);
        sUltrahand.carryVel.y = (sUltrahand.carryVel.y * 0.65f) + ((actor->world.pos.y - prevWorld.y) * 0.35f);
        sUltrahand.carryVel.z = (sUltrahand.carryVel.z * 0.65f) + ((actor->world.pos.z - prevWorld.z) * 0.35f);
        // Constrained bodies run the same carry and are then put back on their axis.
        if (sUltrahand.traits & PACCI_UH_TRAIT_LOCKED) {
            // Pinned where it was grabbed: the controls drive its flag instead.
            actor->world.pos = sUltrahand.railPos;
            sUltrahand.carryVel.x = 0.0f;
            sUltrahand.carryVel.y = 0.0f;
            sUltrahand.carryVel.z = 0.0f;
        }
        Pacci_UhConstrain(play, actor);
        // A LOCKED body keeps its own rotation: only the axis its flag owns is touched.
        if (sUltrahand.traits & PACCI_UH_TRAIT_LOCKED) {
            Pacci_UhLockedPose(play, actor);
            actor->world.rot = actor->shape.rot;
            actor->colorFilterParams = 0;
            Pacci_UhTintAdd(actor);
            if (sUltrahand.vfxAge < 0x7FFF) {
                sUltrahand.vfxAge++;
            }
            return;
        }

        if (sUltrahand.traits & PACCI_UH_TRAIT_NO_TURN) {
            // Lifts and pushed blocks keep their grab facing: turning one drags what stands on it.
            actor->shape.rot = sUltrahand.grabRot;
            actor->world.rot = actor->shape.rot;
            Pacci_UhFlagTick(play, actor);
            Pacci_UhFireTick(play, actor);
            Pacci_UhIceTick(play, actor);
            actor->colorFilterParams = 0;
            Pacci_UhTintAdd(actor);
            if (sUltrahand.vfxAge < 0x7FFF) {
                sUltrahand.vfxAge++;
            }
            return;
        }

        if (sUltrahand.traits & PACCI_UH_TRAIT_NO_FACE) {
            actor->shape.rot = sUltrahand.baseRot;
        } else {
            // Manual rotation is a delta from the grab pose, independent of the orbit.
            playerFacingYaw = Math_Vec3f_Yaw(&actor->world.pos, &player->actor.world.pos);
            targetFacingYaw =
                playerFacingYaw + sUltrahand.faceOffsetYaw + (sUltrahand.baseRot.y - sUltrahand.grabRot.y);
            actor->shape.rot.x = sUltrahand.baseRot.x;
            Math_SmoothStepToS(&actor->shape.rot.y, targetFacingYaw, 4, 0x1000, 0x20);
            actor->shape.rot.z = sUltrahand.baseRot.z;
        }
        actor->world.rot = actor->shape.rot;

        Pacci_UhPlaceMagnet(actor);
        Pacci_UhBombTick(actor);
        Pacci_UhFlagTick(play, actor);
        Pacci_UhFireTick(play, actor);
        Pacci_UhIceTick(play, actor);

        // Re-registered every frame; the table is wiped at the top of the mode's frame.
        actor->colorFilterParams = 0;
        Pacci_UhTintAdd(actor);
        if (sUltrahand.vfxAge < 0x7FFF) {
            sUltrahand.vfxAge++;
        }
        return;
    }

    actor->gravity = PACCI_UH_DROP_GRAVITY;
    actor->velocity.y += actor->gravity;
    if (actor->velocity.y < PACCI_UH_DROP_MIN_VEL_Y) {
        actor->velocity.y = PACCI_UH_DROP_MIN_VEL_Y;
    }
    // By hand: the engine helper drives XZ from world.rot.y, the pose, not the swing.
    actor->world.pos.y += actor->velocity.y;
    actor->world.pos.x += actor->velocity.x;
    actor->world.pos.z += actor->velocity.z;
    // Constraint outlives the release, or inherited sideways momentum walks a lift out of its shaft.
    Pacci_UhConstrain(play, actor);
    actor->velocity.x *= PACCI_UH_DROP_DRAG;
    actor->velocity.z *= PACCI_UH_DROP_DRAG;
    Actor_UpdateBgCheckInfo(play, actor, 0.0f, 0.0f, 0.0f, 4);
    // Parts must match the root before the footprint is measured, or the probe reads a stale shape.
    Pacci_FuseFollow(play);

    {
        f32 restY;
        u8 landed = 0;

        if (Pacci_UhAssemblyGround(play, actor, &restY)) {
            if (actor->world.pos.y <= restY) {
                actor->world.pos.y = restY;
                landed = 1;
            }
        } else if (actor->bgCheckFlags & BGCHECKFLAG_GROUND) {
            landed = 1;
        }
        if (sUltrahand.dropTimer > 0) {
            sUltrahand.dropTimer--;
        }
        if (landed) {
            actor->velocity.x = 0.0f;
            actor->velocity.y = 0.0f;
            actor->velocity.z = 0.0f;
            Pacci_UltrahandLetGo();
        } else if ((sUltrahand.dropTimer <= 0) ||
                   ((player->actor.world.pos.y - actor->world.pos.y) >= PACCI_UH_ABANDON_DROP)) {
            Pacci_UltrahandLetGo();
        }
    }
}

// Parts live in the root's frame; collision is not merged, so each surface still names its part.
#define PACCI_FUSE_MAX_PARTS 6
typedef char PacciAnchorFitsAssembly[(PACCI_ANCHOR_OWNERS >= (PACCI_FUSE_MAX_PARTS + 1)) ? 1 : -1];
#define PACCI_FUSE_PTS 27            // 8 corners + 12 edge mids + 6 face centres + centre
#define PACCI_FUSE_WELD_RANGE 130.0f
#define PACCI_FUSE_NOCOL_HALF 12.0f
#define PACCI_FUSE_NEAR_GAP 55.0f // alternate weld gate: body-to-body surface gap
#define PACCI_FUSE_DETACH_HOLD 12 // frames of held R before a weld comes apart
#define PACCI_UH_WIGGLE_WINDOW 14 // frames a direction flip stays "recent"
#define PACCI_UH_WIGGLE_FLIPS 3   // reversals inside that window before it counts as a shake

// Zonai green, eyeballed off the in-game glow (no published hex exists).
#define PACCI_ZONAI_CORE_R 210
#define PACCI_ZONAI_CORE_G 255
#define PACCI_ZONAI_CORE_B 140
#define PACCI_ZONAI_GLOW_R 80
#define PACCI_ZONAI_GLOW_G 200
#define PACCI_ZONAI_GLOW_B 40

typedef struct {
    Actor* actor;
    Vec3f offset;
    Vec3s rot;
    Vec3f weldLocal;
    ActorFunc origUpdate;
    u32 origFlags;
    f32 origGravity;
    s16 origRoom;
} PacciFusePart;

typedef struct {
    Actor* root;
    PacciFusePart parts[PACCI_FUSE_MAX_PARTS];
    u8 count;
} PacciAssembly;

static PacciAssembly sFuse = { NULL, { { 0 } }, 0 };

// What the next A press would do, recomputed every frame while you hold something.
static struct {
    u8 valid;
    Actor* target;
    Vec3f weld;
    Vec3f snapPos;
    // Both paired points, so which corner meets which is visible.
    Vec3f heldPt;
    Vec3f targetPt;
} sFusePv = { 0, NULL, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

static s16 sFuseDetachHold = 0;

// Yaw only: pitch and roll would need the full matrix, and no weld point is worth that.
typedef struct {
    Vec3f center;
    Vec3f half;
    s16 yaw;
    u8 solid;
    u8 dyna;
} PacciFuseBox;

// Offset is stored in the root's local frame, so it's rotated by root yaw and added to root pos.
static void Pacci_FusePlacePart(PlayState* play, PacciFusePart* slot, Actor* root) {
    Actor* part = slot->actor;
    f32 sin = Math_SinS(root->shape.rot.y);
    f32 cos = Math_CosS(root->shape.rot.y);

    part->world.pos.x = root->world.pos.x + ((slot->offset.x * cos) + (slot->offset.z * sin));
    part->world.pos.y = root->world.pos.y + slot->offset.y;
    part->world.pos.z = root->world.pos.z + ((slot->offset.z * cos) - (slot->offset.x * sin));
    part->shape.rot.x = root->shape.rot.x + slot->rot.x;
    part->shape.rot.y = root->shape.rot.y + slot->rot.y;
    part->shape.rot.z = root->shape.rot.z + slot->rot.z;
    part->world.rot = part->shape.rot;
    part->velocity.x = 0.0f;
    part->velocity.y = 0.0f;
    part->velocity.z = 0.0f;
}

// The part runs its own update, then the assembly transform is reimposed in the same call.
static void Pacci_FusePartUpdate(Actor* thisx, PlayState* play) {
    for (u8 i = 0; i < sFuse.count; i++) {
        PacciFusePart* slot = &sFuse.parts[i];

        if (slot->actor != thisx) {
            continue;
        }
        Pacci_UhKeepOnScreen(thisx);
        if (slot->origUpdate != NULL) {
            slot->origUpdate(thisx, play);
        }
        if ((thisx->update != NULL) && (sFuse.root != NULL) && (sFuse.root->update != NULL)) {
            Pacci_FusePlacePart(play, slot, sFuse.root);
        }
        Pacci_AnchorSync(play, thisx);
        return;
    }
}

u8 Pacci_FuseCount(void) {
    return sFuse.count;
}

u8 Pacci_FuseIsPart(Actor* actor) {
    for (u8 i = 0; i < sFuse.count; i++) {
        if (sFuse.parts[i].actor == actor) {
            return 1;
        }
    }
    return 0;
}

// Aiming at a glued piece acts on the root, the only thing the transform driver moves.
Actor* Pacci_FuseRootOf(Actor* actor) {
    if ((actor != NULL) && (sFuse.count > 0)) {
        if (actor == sFuse.root) {
            return sFuse.root;
        }
        if (Pacci_FuseIsPart(actor)) {
            return sFuse.root;
        }
    }
    return NULL;
}

// For scene teardown: drops bookkeeping without touching actors, whose pointers are already dead.
void Pacci_FuseForget(void) {
    for (u8 i = 0; i < PACCI_FUSE_MAX_PARTS; i++) {
        sFuse.parts[i].actor = NULL;
    }
    sFuse.root = NULL;
    sFuse.count = 0;
    sFusePv.valid = 0;
    sFusePv.target = NULL;
}

static void Pacci_FuseGiveBack(PacciFusePart* part) {
    Actor* actor = part->actor;

    Pacci_AnchorRelease(actor);

    if ((actor != NULL) && (actor->update != NULL)) {
        if (part->origUpdate != NULL) {
            actor->update = part->origUpdate;
        }
        actor->flags = part->origFlags;
        actor->gravity = part->origGravity;
        actor->velocity.y = 0.0f;
        actor->room = (s8)part->origRoom;
        actor->colorFilterParams = 0;
    }
    part->actor = NULL;
}

void Pacci_FuseRelease(void) {
    for (u8 i = 0; i < sFuse.count; i++) {
        Pacci_FuseGiveBack(&sFuse.parts[i]);
    }
    sFuse.root = NULL;
    sFuse.count = 0;
    sFusePv.valid = 0;
    sFusePv.target = NULL;
}

u8 Pacci_FuseDetachPart(Actor* actor) {
    for (u8 i = 0; i < sFuse.count; i++) {
        if (sFuse.parts[i].actor != actor) {
            continue;
        }
        Pacci_FuseGiveBack(&sFuse.parts[i]);
        for (u8 j = i; j < (u8)(sFuse.count - 1); j++) {
            sFuse.parts[j] = sFuse.parts[j + 1];
        }
        sFuse.count--;
        if (sFuse.count == 0) {
            sFuse.root = NULL;
        }
        return 1;
    }
    return 0;
}

// Dynapoly fits from CollisionHeader bounds; everything else falls back to the collision
// cylinder, the only size field every actor carries.
static u8 Pacci_FuseGetBox(PlayState* play, Actor* actor, PacciFuseBox* box) {
    CollisionHeader* hdr = NULL;

    if ((actor == NULL) || (actor->update == NULL)) {
        return 0;
    }
    box->yaw = actor->shape.rot.y;
    box->solid = 0;
    box->dyna = 0;

    for (s32 i = 0; i < play->colCtx.dyna.bgActorMax; i++) {
        BgActor* bg = &play->colCtx.dyna.bgActors[i];

        if ((bg->actor == actor) && (bg->colHeader != NULL)) {
            hdr = bg->colHeader;
            break;
        }
    }

    if (hdr != NULL) {
        f32 lx = ((f32)hdr->maxBounds.x + (f32)hdr->minBounds.x) * 0.5f * actor->scale.x;
        f32 ly = ((f32)hdr->maxBounds.y + (f32)hdr->minBounds.y) * 0.5f * actor->scale.y;
        f32 lz = ((f32)hdr->maxBounds.z + (f32)hdr->minBounds.z) * 0.5f * actor->scale.z;
        f32 sin = Math_SinS(box->yaw);
        f32 cos = Math_CosS(box->yaw);

        box->half.x = ((f32)hdr->maxBounds.x - (f32)hdr->minBounds.x) * 0.5f * actor->scale.x;
        box->half.y = ((f32)hdr->maxBounds.y - (f32)hdr->minBounds.y) * 0.5f * actor->scale.y;
        box->half.z = ((f32)hdr->maxBounds.z - (f32)hdr->minBounds.z) * 0.5f * actor->scale.z;
        // The bounds are model-space, so their centre has to be carried out to world
        // through the actor's yaw before it means anything.
        box->center.x = actor->world.pos.x + ((lx * cos) + (lz * sin));
        box->center.y = actor->world.pos.y + ly;
        box->center.z = actor->world.pos.z + ((lz * cos) - (lx * sin));
        box->solid = 1;
        box->dyna = 1;
        return 1;
    }

    if (actor->colChkInfo.cylRadius > 0) {
        f32 r = (f32)actor->colChkInfo.cylRadius;
        f32 h = (actor->colChkInfo.cylHeight > 0) ? (f32)actor->colChkInfo.cylHeight : (r * 2.0f);

        box->half.x = r;
        box->half.z = r;
        box->half.y = h * 0.5f;
        box->center.x = actor->world.pos.x;
        box->center.y = actor->world.pos.y + (f32)actor->colChkInfo.cylYShift + (h * 0.5f);
        box->center.z = actor->world.pos.z;
        box->solid = 1;
        return 1;
    }

    // No collision at all. It still gets a nominal box so it has corners to be
    // pinned by, but solid stays 0 and that is what triggers the corners-only rule.
    box->half.x = PACCI_FUSE_NOCOL_HALF;
    box->half.y = PACCI_FUSE_NOCOL_HALF;
    box->half.z = PACCI_FUSE_NOCOL_HALF;
    box->center = actor->world.pos;
    box->center.y += PACCI_FUSE_NOCOL_HALF;
    return 1;
}

// i/j/k over {-1,0,1}: three non-zero is a corner, one zero an edge midpoint.
static u8 Pacci_FuseBoxPoints(PacciFuseBox* box, u8 cornersOnly, Vec3f* out) {
    f32 sin = Math_SinS(box->yaw);
    f32 cos = Math_CosS(box->yaw);
    u8 n = 0;

    for (s32 i = -1; i <= 1; i++) {
        for (s32 j = -1; j <= 1; j++) {
            for (s32 k = -1; k <= 1; k++) {
                f32 lx;
                f32 ly;
                f32 lz;

                // Face and box centres win on overlap and weld pieces into each other, so they are skipped.
                s32 zeros = ((i == 0) ? 1 : 0) + ((j == 0) ? 1 : 0) + ((k == 0) ? 1 : 0);

                if (zeros > (cornersOnly ? 0 : 1)) {
                    continue;
                }
                lx = (f32)i * box->half.x;
                ly = (f32)j * box->half.y;
                lz = (f32)k * box->half.z;

                out[n].x = box->center.x + ((lx * cos) + (lz * sin));
                out[n].y = box->center.y + ly;
                out[n].z = box->center.z + ((lz * cos) - (lx * sin));
                n++;
            }
        }
    }
    return n;
}

// Separation between the two bodies, yaw ignored: a proximity gate, not the solve.
static f32 Pacci_FuseBoxGap(PacciFuseBox* a, PacciFuseBox* b) {
    f32 gx = fabsf(b->center.x - a->center.x) - (a->half.x + b->half.x);
    f32 gy = fabsf(b->center.y - a->center.y) - (a->half.y + b->half.y);
    f32 gz = fabsf(b->center.z - a->center.z) - (a->half.z + b->half.z);

    if (gx < 0.0f) {
        gx = 0.0f;
    }
    if (gy < 0.0f) {
        gy = 0.0f;
    }
    if (gz < 0.0f) {
        gz = 0.0f;
    }
    return sqrtf((gx * gx) + (gy * gy) + (gz * gz));
}

static u8 Pacci_FuseSolve(PlayState* play, Actor* held, Actor* target, Vec3f* weld, Vec3f* snapPos, Vec3f* outHeldPt,
                          Vec3f* outTargetPt) {
    PacciFuseBox hb;
    PacciFuseBox tb;
    Vec3f hp[PACCI_FUSE_PTS];
    Vec3f tp[PACCI_FUSE_PTS];
    u8 hn;
    u8 tn;
    u8 cornersOnly;
    f32 best = 3.0e38f;
    s32 bh = -1;
    s32 bt = -1;

    if (!Pacci_FuseGetBox(play, held, &hb) || !Pacci_FuseGetBox(play, target, &tb)) {
        return 0;
    }

    // Something with no collision has no face or edge to lie along, so it may only be
    // pinned corner-to-corner, and only onto real dynapoly.
    cornersOnly = (!hb.solid || !tb.solid);
    if (cornersOnly && !hb.dyna && !tb.dyna) {
        return 0;
    }

    hn = Pacci_FuseBoxPoints(&hb, cornersOnly, hp);
    tn = Pacci_FuseBoxPoints(&tb, cornersOnly, tp);

    for (u8 i = 0; i < hn; i++) {
        for (u8 j = 0; j < tn; j++) {
            f32 dx = tp[j].x - hp[i].x;
            f32 dy = tp[j].y - hp[i].y;
            f32 dz = tp[j].z - hp[i].z;
            f32 d = (dx * dx) + (dy * dy) + (dz * dz);

            if (d < best) {
                best = d;
                bh = i;
                bt = j;
            }
        }
    }
    if (bh < 0) {
        return 0;
    }
    // Either gate offers the weld: corner pairs alone miss two large boxes resting against each other.
    if ((best > (PACCI_FUSE_WELD_RANGE * PACCI_FUSE_WELD_RANGE)) &&
        (Pacci_FuseBoxGap(&hb, &tb) > PACCI_FUSE_NEAR_GAP)) {
        return 0;
    }

    *weld = tp[bt];
    snapPos->x = held->world.pos.x + (tp[bt].x - hp[bh].x);
    snapPos->y = held->world.pos.y + (tp[bt].y - hp[bh].y);
    snapPos->z = held->world.pos.z + (tp[bt].z - hp[bh].z);

    if (outHeldPt != NULL) {
        *outHeldPt = hp[bh];
    }
    if (outTargetPt != NULL) {
        *outTargetPt = tp[bt];
    }
    return 1;
}


u8 Pacci_FusePreviewValid(void) {
    return sFusePv.valid;
}

// Must run after the carry moved the held body, or the bead lags a frame.
void Pacci_FuseUpdatePreview(PlayState* play, Player* player) {
    Actor* held = sUltrahand.held;
    Actor* target;

    sFusePv.valid = 0;
    sFusePv.target = NULL;

    if ((held == NULL) || (held->update == NULL)) {
        return;
    }
    if (sFuse.count >= PACCI_FUSE_MAX_PARTS) {
        return;
    }
    if ((sFuse.count > 0) && (sFuse.root != held)) {
        return;
    }

    target = Pacci_ResolveUltrahandTarget(play, player);
    if ((target == NULL) || (target == held) || (target->update == NULL) || Pacci_FuseIsPart(target)) {
        return;
    }
    if (!Pacci_FuseSolve(play, held, target, &sFusePv.weld, &sFusePv.snapPos, &sFusePv.heldPt, &sFusePv.targetPt)) {
        return;
    }

    sFusePv.valid = 1;
    sFusePv.target = target;
    // Both ends light up, so the offer reads as a pair about to join.
    Pacci_UhTintAdd(target);
    Pacci_UhTintAdd(held);
}

// Registers an already-placed part at a given offset: a stored structure is rebuilt exactly.
static u8 Pacci_FuseAdopt(Actor* root, Actor* part, Vec3f* localOff, Vec3s* relRot) {
    PacciFusePart* slot;

    if ((root == NULL) || (part == NULL) || (part->update == NULL) || (sFuse.count >= PACCI_FUSE_MAX_PARTS)) {
        return 0;
    }
    slot = &sFuse.parts[sFuse.count];
    sFuse.root = root;
    slot->actor = part;
    slot->offset = *localOff;
    slot->rot = *relRot;
    slot->weldLocal = *localOff;
    slot->origUpdate = part->update;
    slot->origFlags = part->flags;
    slot->origGravity = part->gravity;
    slot->origRoom = part->room;
    sFuse.count++;

    Pacci_AnchorTake(part);
    part->update = Pacci_FusePartUpdate;
    part->gravity = 0.0f;
    part->velocity.x = 0.0f;
    part->velocity.y = 0.0f;
    part->velocity.z = 0.0f;
    part->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    return 1;
}

u8 Pacci_FuseTryAttach(PlayState* play) {
    Actor* root = sUltrahand.held;
    Actor* part = sFusePv.target;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 sin;
    f32 cos;
    PacciFusePart* slot;

    if (!sFusePv.valid || (root == NULL) || (part == NULL) || (part->update == NULL)) {
        return 0;
    }
    if (sFuse.count >= PACCI_FUSE_MAX_PARTS) {
        return 0;
    }
    // Welding onto another root starts a new structure: old offsets would describe the wrong root.
    if ((sFuse.count > 0) && (sFuse.root != root)) {
        Pacci_FuseRelease();
    }

    // Snap the held piece onto the weld point BEFORE the offset is measured, so what
    // gets recorded is the aligned pose and not where your aim happened to be.
    root->world.pos = sFusePv.snapPos;
    root->prevPos = root->world.pos;

    dx = part->world.pos.x - root->world.pos.x;
    dy = part->world.pos.y - root->world.pos.y;
    dz = part->world.pos.z - root->world.pos.z;

    // World delta -> the root's local frame, so the part keeps its relative place when
    // the root turns. Negative angle because this is the inverse rotation.
    sin = Math_SinS(-root->shape.rot.y);
    cos = Math_CosS(-root->shape.rot.y);

    slot = &sFuse.parts[sFuse.count];
    sFuse.root = root;
    slot->actor = part;
    slot->offset.x = (dx * cos) + (dz * sin);
    slot->offset.y = dy;
    slot->offset.z = (dz * cos) - (dx * sin);
    slot->rot.x = part->shape.rot.x - root->shape.rot.x;
    slot->rot.y = part->shape.rot.y - root->shape.rot.y;
    slot->rot.z = part->shape.rot.z - root->shape.rot.z;
    // Tilt comes from the target so a structure looks built; the chosen yaw is kept.
    root->shape.rot.x = part->shape.rot.x;
    root->shape.rot.z = part->shape.rot.z;
    root->world.rot = root->shape.rot;
    sUltrahand.baseRot = root->shape.rot;
    slot->rot.x = 0;
    slot->rot.z = 0;
    {
        f32 wx = sFusePv.weld.x - root->world.pos.x;
        f32 wy = sFusePv.weld.y - root->world.pos.y;
        f32 wz = sFusePv.weld.z - root->world.pos.z;

        slot->weldLocal.x = (wx * cos) + (wz * sin);
        slot->weldLocal.y = wy;
        slot->weldLocal.z = (wz * cos) - (wx * sin);
    }
    slot->origUpdate = part->update;
    slot->origFlags = part->flags;
    slot->origGravity = part->gravity;
    slot->origRoom = part->room;
    sFuse.count++;

    Pacci_AnchorTake(part);
    part->update = Pacci_FusePartUpdate;
    part->gravity = 0.0f;
    part->velocity.x = 0.0f;
    part->velocity.y = 0.0f;
    part->velocity.z = 0.0f;
    // Culling would stop the part's transform being driven while the root is still on
    // screen, and the piece would be left behind mid-air.
    part->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;

    sFusePv.valid = 0;
    sFusePv.target = NULL;

    Audio_PlaySoundGeneral(NA_SE_SY_GET_ITEM, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    return 1;
}

// Stick shaken left-right detaches; reversals are counted, not deflection, so running sideways
// never triggers it.
void Pacci_FuseWiggleDetach(PlayState* play, Player* player, Input* input) {
    s8 x = input->cur.stick_x;
    s8 dir = (x > 40) ? 1 : ((x < -40) ? -1 : 0);

    if (sUhMode.wiggleTimer > 0) {
        sUhMode.wiggleTimer--;
    } else {
        sUhMode.wiggleFlips = 0;
        sUhMode.wiggleDir = 0;
    }

    if (dir != 0) {
        if ((sUhMode.wiggleDir != 0) && (dir != sUhMode.wiggleDir)) {
            sUhMode.wiggleFlips++;
        }
        sUhMode.wiggleDir = dir;
        sUhMode.wiggleTimer = PACCI_UH_WIGGLE_WINDOW;
    }

    if (sUhMode.wiggleFlips < PACCI_UH_WIGGLE_FLIPS) {
        return;
    }
    sUhMode.wiggleFlips = 0;
    sUhMode.wiggleDir = 0;
    sUhMode.wiggleTimer = 0;

    if (sFuse.count == 0) {
        return;
    }
    if (!Pacci_FuseDetachPart(Pacci_ResolveUltrahandTarget(play, player))) {
        Pacci_FuseRelease();
    }
    Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

void Pacci_FuseHoldDetach(PlayState* play, Player* player, u8 rHeld) {
    if (!rHeld) {
        sFuseDetachHold = 0;
        return;
    }
    sFuseDetachHold++;
    if (sFuseDetachHold != PACCI_FUSE_DETACH_HOLD) {
        return; // fires once per hold, not every frame it stays down
    }
    if (sFuse.count == 0) {
        return;
    }
    if (!Pacci_FuseDetachPart(Pacci_ResolveUltrahandTarget(play, player))) {
        Pacci_FuseRelease();
    }
    Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// Rebuild every part from the root. Must run AFTER the root has been positioned.
void Pacci_FuseFollow(PlayState* play) {
    Actor* root = sFuse.root;
    u8 lit;

    if ((root == NULL) || (sFuse.count == 0)) {
        return;
    }
    if (root->update == NULL) {
        // The parts outlived it and nothing drives them any more, so hand them back to
        // themselves rather than leaving them frozen for the rest of the scene.
        Pacci_FuseRelease();
        return;
    }

    // Only in hand: this also runs every frame after the structure is set down.
    lit = Pacci_IsHoldingUltrahand() && (root == sUltrahand.held);
    for (u8 i = 0; i < sFuse.count; i++) {
        Actor* part = sFuse.parts[i].actor;

        if ((part == NULL) || (part->update == NULL)) {
            // A dead part leaves the assembly, or its anchor stays alive and the count lies.
            if (part != NULL) {
                Pacci_AnchorRelease(part);
                sFuse.parts[i].actor = NULL;
            }
            continue;
        }
        // Catches parts whose category updated before the root moved.
        Pacci_FusePlacePart(play, &sFuse.parts[i], root);
        if (lit) {
            Pacci_UhTintAdd(part);
        }
    }
}


// An octahedron, not a cube: at bead size the silhouette is all you read, and eight
// faces round off where six would show corners.
static Vtx sFuseBeadVtx[] = {
    VTX(0, 1, 0, 0, 0, 0, 0, 0, 255),  VTX(0, -1, 0, 0, 0, 0, 0, 0, 255), VTX(1, 0, 0, 0, 0, 0, 0, 0, 255),
    VTX(-1, 0, 0, 0, 0, 0, 0, 0, 255), VTX(0, 0, 1, 0, 0, 0, 0, 0, 255),  VTX(0, 0, -1, 0, 0, 0, 0, 0, 255),
};

static Gfx sFuseBeadDL[] = {
    gsSPVertex(sFuseBeadVtx, 6, 0),         gsSP2Triangles(0, 4, 2, 0, 0, 2, 5, 0),
    gsSP2Triangles(0, 5, 3, 0, 0, 3, 4, 0), gsSP2Triangles(1, 2, 4, 0, 1, 5, 2, 0),
    gsSP2Triangles(1, 3, 5, 0, 1, 4, 3, 0), gsSPEndDisplayList(),
};

// Floor switches: a held weight slides onto the plate. Only FLOOR_2/3 latch on a dynapoly press
// (z_obj_switch.c:390-415). Generous range: an offer that keeps expiring never arrives.
#define PACCI_PLACE_RANGE 140.0f

static struct {
    u8 valid;
    Actor* sw;
    Vec3f pos; // where the held actor's ORIGIN goes
} sPlacePv = { 0, NULL, { 0.0f, 0.0f, 0.0f } };

u8 Pacci_PlaceOfferValid(void) {
    return sPlacePv.valid;
}

// What a floor switch pulls: a named list, since ACTOR_FLAG_CAN_PRESS_SWITCHES is on only two actors.
static u8 Pacci_PlaceIsWeight(Actor* actor) {
    if (actor == NULL) {
        return 0;
    }
    if (IsSomariaWeight(actor)) {
        return 1;
    }
    return (actor->id == ACTOR_OBJ_OSHIHIKI) || (actor->id == ACTOR_EN_AM) || (actor->id == ACTOR_OBJ_KIBAKO) ||
           (actor->id == ACTOR_OBJ_KIBAKO2);
}

static u8 Pacci_PlaceIsFloorSwitch(Actor* actor) {
    s32 type;

    if ((actor == NULL) || (actor->update == NULL) || (actor->id != ACTOR_OBJ_SWITCH)) {
        return 0;
    }
    // z_obj_switch.c:12-17: 0 FLOOR, 1 FLOOR_RUSTY, type = params & 7.
    type = actor->params & 7;
    return (type == 0) || (type == 1);
}

// The world Y of the TOP of a dynapoly actor's own collision bounds, or 0 with a 0 return if it
// has none. Used to sit a body exactly on a floor switch's plate.
static u8 Pacci_UhDynaTopY(PlayState* play, Actor* actor, f32* outY) {
    s32 i;

    if ((play == NULL) || (actor == NULL)) {
        return 0;
    }
    for (i = 0; i < play->colCtx.dyna.bgActorMax; i++) {
        BgActor* bg = &play->colCtx.dyna.bgActors[i];

        if ((bg->actor != actor) || (bg->colHeader == NULL)) {
            continue;
        }
        *outY = actor->world.pos.y + ((f32)bg->colHeader->maxBounds.y * actor->scale.y);
        return 1;
    }
    return 0;
}


void Pacci_PlaceUpdatePreview(PlayState* play) {
    Actor* held = sUltrahand.held;
    PacciFuseBox box;
    Actor* it;
    f32 best = PACCI_PLACE_RANGE * PACCI_PLACE_RANGE;

    sPlacePv.valid = 0;
    sPlacePv.sw = NULL;
    if ((play == NULL) || (held == NULL) || sUltrahand.dropping) {
        return;
    }
    // The weld offer wins if there is one: gluing is the deliberate act, placing is the
    // convenience, and one A press cannot mean both.
    if (sFusePv.valid) {
        return;
    }
    if (!Pacci_FuseGetBox(play, held, &box)) {
        return;
    }
    if (!Pacci_PlaceIsWeight(held)) {
        return;
    }

    // XZ from the body's centre: a block held high over a switch is still lined up with it.
    for (it = play->actorCtx.actorLists[ACTORCAT_SWITCH].head; it != NULL; it = it->next) {
        f32 dx;
        f32 dz;
        f32 d;

        if (!Pacci_PlaceIsFloorSwitch(it)) {
            continue;
        }
        // From the anchor, not the body: once on the plate the body is always at zero distance.
        dx = it->world.pos.x - sUltrahand.anchorPos.x;
        dz = it->world.pos.z - sUltrahand.anchorPos.z;
        d = (dx * dx) + (dz * dz);
        if (d < best) {
            best = d;
            sPlacePv.sw = it;
        }
    }
    if (sPlacePv.sw == NULL) {
        return;
    }

    // Bottom face on the plate's collision top; the origin is not the plate top, and a ray from inside
    // the plate would report the floor below.
    sPlacePv.pos.x = sPlacePv.sw->world.pos.x;
    sPlacePv.pos.z = sPlacePv.sw->world.pos.z;
    {
        f32 plateTop;

        if (Pacci_UhDynaTopY(play, sPlacePv.sw, &plateTop)) {
            // Sunk a hair: resting exactly on a surface is decided by float rounding.
            sPlacePv.pos.y = plateTop - PACCI_PLACE_SINK + (held->world.pos.y - (box.center.y - box.half.y));
        } else {
            sPlacePv.pos.y = sPlacePv.sw->world.pos.y + (held->world.pos.y - (box.center.y - box.half.y));
        }
    }
    sPlacePv.valid = 1;
}

static void Pacci_FuseDrawBead(PlayState* play, Vec3f* pos, f32 scale, u8 pulsing) {
    // Two passes: a soft halo with a brighter core inside it. One flat blob does not
    // read as glowing, it reads as a green rock.
    f32 pulse = pulsing ? (0.85f + (0.15f * Math_SinS((s16)(play->gameplayFrames * 2200)))) : 1.0f;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);

    // Halo. Components spelled out: MSVC hands a multi-value #define to a
    // function-like macro as ONE argument, so a packed colour would not expand.
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_Scale(scale * 1.9f * pulse, scale * 1.9f * pulse, scale * 1.9f * pulse, MTXMODE_APPLY);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, PACCI_ZONAI_GLOW_R, PACCI_ZONAI_GLOW_G, PACCI_ZONAI_GLOW_B, 90);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, sFuseBeadDL);

    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_Scale(scale * pulse, scale * pulse, scale * pulse, MTXMODE_APPLY);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, PACCI_ZONAI_CORE_R, PACCI_ZONAI_CORE_G, PACCI_ZONAI_CORE_B, 235);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, sFuseBeadDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// The colour filter has no green, so the Zonai mark is beads on the box corners.
static void Pacci_FuseDrawBoxMarks(PlayState* play, Actor* actor) {
    PacciFuseBox box;
    Vec3f pts[PACCI_FUSE_PTS];
    u8 n;

    if ((actor == NULL) || (actor->update == NULL)) {
        return;
    }
    if (!Pacci_FuseGetBox(play, actor, &box)) {
        return;
    }
    n = Pacci_FuseBoxPoints(&box, 1, pts); // corners only: 8 marks, not the full 20
    for (u8 i = 0; i < n; i++) {
        Pacci_FuseDrawBead(play, &pts[i], 3.0f, 0);
    }
}

// One bead per existing joint, plus a pulsing one on the weld being offered.
void Pacci_FuseDrawPreview(PlayState* play) {
    Actor* root = sFuse.root;

    if ((root != NULL) && (root->update != NULL)) {
        f32 sin = Math_SinS(root->shape.rot.y);
        f32 cos = Math_CosS(root->shape.rot.y);

        for (u8 i = 0; i < sFuse.count; i++) {
            Vec3f* w = &sFuse.parts[i].weldLocal;
            Vec3f pos;

            if ((sFuse.parts[i].actor == NULL) || (sFuse.parts[i].actor->update == NULL)) {
                continue;
            }
            pos.x = root->world.pos.x + ((w->x * cos) + (w->z * sin));
            pos.y = root->world.pos.y + w->y;
            pos.z = root->world.pos.z + ((w->z * cos) - (w->x * sin));
            Pacci_FuseDrawBead(play, &pos, 6.0f, 0);
        }
    }

    if (sPlacePv.valid) {
        Vec3f mark = sPlacePv.pos;

        // One bead where it will land and the body outlined, the same vocabulary the weld offer
        // uses, so "A will do something here" reads the same way in all three cases.
        Pacci_FuseDrawBoxMarks(play, sUltrahand.held);
        if (sPlacePv.sw != NULL) {
            mark.y = sPlacePv.sw->world.pos.y;
        }
        Pacci_FuseDrawBead(play, &mark, 8.0f, 1);
    }

    if (sFusePv.valid) {
        // Both bodies outlined, then one bead per object and a dotted run between them.
        Pacci_FuseDrawBoxMarks(play, sUltrahand.held);
        Pacci_FuseDrawBoxMarks(play, sFusePv.target);
        Pacci_FuseDrawBead(play, &sFusePv.heldPt, 6.0f, 1);
        Pacci_FuseDrawBead(play, &sFusePv.targetPt, 7.0f, 1);

        {
            Vec3f step;
            f32 t;

            for (t = 0.2f; t < 0.99f; t += 0.2f) {
                step.x = sFusePv.heldPt.x + ((sFusePv.targetPt.x - sFusePv.heldPt.x) * t);
                step.y = sFusePv.heldPt.y + ((sFusePv.targetPt.y - sFusePv.heldPt.y) * t);
                step.z = sFusePv.heldPt.z + ((sFusePv.targetPt.z - sFusePv.heldPt.z) * t);
                Pacci_FuseDrawBead(play, &step, 2.5f, 0);
            }
        }
    }
}


#define PACCI_UH_VFX_POINTS 13
#define PACCI_UH_GIZMO_SEGMENTS 20
#define PACCI_UH_GIZMO_SHAFT_R 3.6f
#define PACCI_UH_GIZMO_HEAD_R 10.5f
#define PACCI_UH_GIZMO_RING_R 3.0f
// Tether pulses, the Blender material colours, and a point light wide enough to reach floor and walls.
#define PACCI_UH_LIGHT_R 55
#define PACCI_UH_LIGHT_G 255
#define PACCI_UH_LIGHT_B 160
#define PACCI_UH_LIGHT_RADIUS 340

// Solid primitives, 100 units along local +Y: scale by (r/100, len/100, r/100).

static Vtx sPacciUhPrismVtx[] = {
    VTX(100, 0, 0, 0, 0, 0, 0, 0, 255),    VTX(71, 0, 71, 0, 0, 0, 0, 0, 255),
    VTX(0, 0, 100, 0, 0, 0, 0, 0, 255),    VTX(-71, 0, 71, 0, 0, 0, 0, 0, 255),
    VTX(-100, 0, 0, 0, 0, 0, 0, 0, 255),   VTX(-71, 0, -71, 0, 0, 0, 0, 0, 255),
    VTX(0, 0, -100, 0, 0, 0, 0, 0, 255),   VTX(71, 0, -71, 0, 0, 0, 0, 0, 255),
    VTX(100, 100, 0, 0, 0, 0, 0, 0, 255),  VTX(71, 100, 71, 0, 0, 0, 0, 0, 255),
    VTX(0, 100, 100, 0, 0, 0, 0, 0, 255),  VTX(-71, 100, 71, 0, 0, 0, 0, 0, 255),
    VTX(-100, 100, 0, 0, 0, 0, 0, 0, 255), VTX(-71, 100, -71, 0, 0, 0, 0, 0, 255),
    VTX(0, 100, -100, 0, 0, 0, 0, 0, 255), VTX(71, 100, -71, 0, 0, 0, 0, 0, 255),
};

static Gfx sPacciUhPrismDL[] = {
    gsSPVertex(sPacciUhPrismVtx, 16, 0),       gsSP2Triangles(0, 1, 9, 0, 0, 9, 8, 0),
    gsSP2Triangles(1, 2, 10, 0, 1, 10, 9, 0),  gsSP2Triangles(2, 3, 11, 0, 2, 11, 10, 0),
    gsSP2Triangles(3, 4, 12, 0, 3, 12, 11, 0), gsSP2Triangles(4, 5, 13, 0, 4, 13, 12, 0),
    gsSP2Triangles(5, 6, 14, 0, 5, 14, 13, 0), gsSP2Triangles(6, 7, 15, 0, 6, 15, 14, 0),
    gsSP2Triangles(7, 0, 8, 0, 7, 8, 15, 0),   gsSPEndDisplayList(),
};

static Vtx sPacciUhConeVtx[] = {
    VTX(0, 100, 0, 0, 0, 0, 0, 0, 255),   VTX(100, 0, 0, 0, 0, 0, 0, 0, 255),  VTX(71, 0, 71, 0, 0, 0, 0, 0, 255),
    VTX(0, 0, 100, 0, 0, 0, 0, 0, 255),   VTX(-71, 0, 71, 0, 0, 0, 0, 0, 255), VTX(-100, 0, 0, 0, 0, 0, 0, 0, 255),
    VTX(-71, 0, -71, 0, 0, 0, 0, 0, 255), VTX(0, 0, -100, 0, 0, 0, 0, 0, 255), VTX(71, 0, -71, 0, 0, 0, 0, 0, 255),
};

static Gfx sPacciUhConeDL[] = {
    gsSPVertex(sPacciUhConeVtx, 9, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(0, 3, 4, 0, 0, 4, 5, 0),
    gsSP2Triangles(0, 5, 6, 0, 0, 6, 7, 0),
    gsSP2Triangles(0, 7, 8, 0, 0, 8, 1, 0),
    gsSP2Triangles(1, 2, 3, 0, 1, 3, 4, 0),
    gsSP2Triangles(1, 4, 5, 0, 1, 5, 6, 0),
    gsSP2Triangles(1, 6, 7, 0, 1, 7, 8, 0),
    gsSPEndDisplayList(),
};
static void Pacci_UhGetHandPos(Player* player, Vec3f* pos) {
    Vec3f forearm = player->bodyPartsPos[PLAYER_BODYPART_R_FOREARM];
    Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    f32 dx = hand.x - forearm.x;
    f32 dy = hand.y - forearm.y;
    f32 dz = hand.z - forearm.z;
    f32 length = sqrtf((dx * dx) + (dy * dy) + (dz * dz));

    *pos = hand;
    if (length > 0.001f) {
        pos->x += (dx / length) * 10.0f;
        pos->y += (dy / length) * 10.0f;
        pos->z += (dz / length) * 10.0f;
    }
}

static void Pacci_UhGetStreamPoint(Vec3f* point, Vec3f* start, Vec3f* end, f32 t, s16 phase, u32 frame) {
    f32 dx = end->x - start->x;
    f32 dz = end->z - start->z;
    f32 xzLength = sqrtf((dx * dx) + (dz * dz));
    f32 envelope = Math_SinS((s16)(t * 0x7FFF));
    f32 sideX = 1.0f;
    f32 sideZ = 0.0f;
    s16 broadWave = (s16)((t * 0x6000) + (frame * 0x0180) + phase);
    s16 fineWave = (s16)((t * 0xE000) - (frame * 0x00C0) + (phase >> 1));
    f32 flutter = ((Math_SinS(broadWave) * 0.65f) + (Math_SinS(fineWave) * 0.35f)) * envelope;

    if (xzLength > 0.001f) {
        sideX = dz / xzLength;
        sideZ = -dx / xzLength;
    }
    point->x = start->x + ((end->x - start->x) * t) + (sideX * flutter * 3.8f);
    point->y = start->y + ((end->y - start->y) * t) + (envelope * 10.0f) + (Math_CosS(broadWave) * envelope * 1.8f);
    point->z = start->z + ((end->z - start->z) * t) + (sideZ * flutter * 3.8f);
}

// PRIMITIVE in every combiner slot: nothing binds a texture here, TEXEL0 would read a stale one.
static void Pacci_UhSolidBegin(Gfx** gfxP) {
    Gfx* gfx = *gfxP;

    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gSPClearGeometryMode(gfx++, G_LIGHTING | G_CULL_BACK);
    *gfxP = gfx;
}

static void Pacci_UhSolidEnd(Gfx** gfxP) {
    Gfx* gfx = *gfxP;

    gSPSetGeometryMode(gfx++, G_LIGHTING | G_CULL_BACK);
    *gfxP = gfx;
}

// Draw one solid primitive spanning start -> end. Must sit between a Begin/End pair.
static void Pacci_UhDrawSolid(PlayState* play, Gfx** gfxP, Gfx* dl, Vec3f* start, Vec3f* end, f32 radius, u8 r, u8 g,
                              u8 b, u8 alpha) {
    f32 dx = end->x - start->x;
    f32 dy = end->y - start->y;
    f32 dz = end->z - start->z;
    f32 xzLength = sqrtf((dx * dx) + (dz * dz));
    f32 length = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
    Gfx* gfx = *gfxP;

    if (length < 0.1f) {
        return;
    }
    Matrix_Translate(start->x, start->y, start->z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(Math_Vec3f_Yaw(start, end)), MTXMODE_APPLY);
    Matrix_RotateX(atan2f(xzLength, dy), MTXMODE_APPLY);
    Matrix_Scale(radius / 100.0f, length / 100.0f, radius / 100.0f, MTXMODE_APPLY);
    gDPSetPrimColor(gfx++, 0, 0, r, g, b, alpha);
    gSPMatrix(gfx++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(gfx++, dl);
    *gfxP = gfx;
}

// A real point light, so Link and the floor catch the green spill.
static LightInfo sUhLightInfo;
static LightNode* sUhLightNode = NULL;
// A scene change frees the light list; the scene tells a stale node apart.
static s16 sUhLightScene = -1;

static void Pacci_UhLightOff(PlayState* play) {
    if (sUhLightNode != NULL) {
        if ((play != NULL) && (play->sceneNum == sUhLightScene)) {
            LightContext_RemoveLight(play, &play->lightCtx, sUhLightNode);
        }
        sUhLightNode = NULL;
        sUhLightScene = -1;
    }
}

static void Pacci_UhLightAt(PlayState* play, Vec3f* pos, f32 strength) {
    // Breathes with the same period as the contact rings, so light and geometry pulse
    // together instead of drifting in and out of phase with each other.
    f32 pulse = strength * (0.86f + (Math_SinS((s16)(play->gameplayFrames * 0x0500)) * 0.14f));

    Lights_PointNoGlowSetInfo(&sUhLightInfo, (s16)pos->x, (s16)pos->y, (s16)pos->z, (u8)(PACCI_UH_LIGHT_R * pulse),
                              (u8)(PACCI_UH_LIGHT_G * pulse), (u8)(PACCI_UH_LIGHT_B * pulse), PACCI_UH_LIGHT_RADIUS);
    if (sUhLightNode == NULL) {
        sUhLightNode = LightContext_InsertLight(play, &play->lightCtx, &sUhLightInfo);
        sUhLightScene = play->sceneNum;
    }
}

static void Pacci_UhDrawFlowPass(PlayState* play, Gfx** gfxP, Vec3f* hand, Vec3f* target, f32 reach, s16 phase,
                                 f32 width, u8 r, u8 g, u8 b, u8 alpha) {
    Vec3f points[PACCI_UH_VFX_POINTS];
    u8 i;

    for (i = 0; i < PACCI_UH_VFX_POINTS; i++) {
        f32 t = ((f32)i / (PACCI_UH_VFX_POINTS - 1)) * reach;

        Pacci_UhGetStreamPoint(&points[i], hand, target, t, phase, play->gameplayFrames);
    }
    // One Begin/End for the whole strand: twelve segments at three commands each instead of
    // twelve full state changes.
    Pacci_UhSolidBegin(gfxP);
    for (i = 0; i < PACCI_UH_VFX_POINTS - 1; i++) {
        // Fat in the middle, tapering to nothing at both ends, so the strand is born at the
        // hand and dies into the object rather than being cut off flat.
        f32 edge = Math_SinS((s16)((i * 0x7FFF) / (PACCI_UH_VFX_POINTS - 2)));

        Pacci_UhDrawSolid(play, gfxP, sPacciUhPrismDL, &points[i], &points[i + 1], width * (0.55f + (edge * 0.45f)), r,
                          g, b, alpha);
    }
    Pacci_UhSolidEnd(gfxP);
}

static void Pacci_UhDrawArrow(PlayState* play, Gfx** gfxP, Vec3f* center, Vec3f* direction, f32 startDist, f32 length,
                              u8 r, u8 g, u8 b) {
    f32 headLength = CLAMP_MIN((length - startDist) * 0.34f, 16.0f);
    f32 shaftEnd = length - headLength;
    Vec3f start;
    Vec3f neck;
    Vec3f tip;

    if (shaftEnd <= (startDist + 1.0f)) {
        shaftEnd = startDist + 1.0f;
    }
    start.x = center->x + (direction->x * startDist);
    start.y = center->y + (direction->y * startDist);
    start.z = center->z + (direction->z * startDist);
    neck.x = center->x + (direction->x * shaftEnd);
    neck.y = center->y + (direction->y * shaftEnd);
    neck.z = center->z + (direction->z * shaftEnd);
    tip.x = center->x + (direction->x * length);
    tip.y = center->y + (direction->y * length);
    tip.z = center->z + (direction->z * length);

    Pacci_UhSolidBegin(gfxP);
    Pacci_UhDrawSolid(play, gfxP, sPacciUhPrismDL, &start, &neck, PACCI_UH_GIZMO_SHAFT_R, r, g, b, 255);
    Pacci_UhDrawSolid(play, gfxP, sPacciUhConeDL, &neck, &tip, PACCI_UH_GIZMO_HEAD_R, r, g, b, 255);
    Pacci_UhSolidEnd(gfxP);
}

// One Begin/End per ring: twenty segments cost twenty matrices, not twenty state changes.
static void Pacci_UhDrawRotationRing(PlayState* play, Gfx** gfxP, Vec3f* center, f32 radius, s16 yaw, u8 vertical, u8 r,
                                     u8 g, u8 b) {
    Vec3f previous;
    f32 sinYaw = Math_SinS(yaw);
    f32 cosYaw = Math_CosS(yaw);
    u8 i;

    Pacci_UhSolidBegin(gfxP);
    for (i = 0; i <= PACCI_UH_GIZMO_SEGMENTS; i++) {
        s16 angle = (s16)((i * 0x10000) / PACCI_UH_GIZMO_SEGMENTS);
        f32 sin = Math_SinS(angle);
        f32 cos = Math_CosS(angle);
        Vec3f point;

        if (vertical) {
            point.x = center->x + (sinYaw * cos * radius);
            point.y = center->y + (sin * radius);
            point.z = center->z + (cosYaw * cos * radius);
        } else {
            point.x = center->x + (sin * radius);
            point.y = center->y;
            point.z = center->z + (cos * radius);
        }
        if (i != 0) {
            Pacci_UhDrawSolid(play, gfxP, sPacciUhPrismDL, &previous, &point, PACCI_UH_GIZMO_RING_R, r, g, b, 245);
        }
        previous = point;
    }
    Pacci_UhSolidEnd(gfxP);
}

void Pacci_UltrahandDrawVfx(PlayState* play, Player* player) {
    Actor* held;
    PacciFuseBox box;
    Vec3f hand;
    Vec3f forward;
    Vec3f side;
    Vec3f up = { 0.0f, 1.0f, 0.0f };
    Vec3f down = { 0.0f, -1.0f, 0.0f };
    u16 buttons;
    u8 skip = 0;

    // Armed is enough: the tether and the weld offer show before the mode is opened.
    if (!sUhMode.active && !sUhArmed) {
        Pacci_UhLightOff(play);
        return;
    }
    held = sUltrahand.held;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, play->gameplayFrames * 2, 0x20, 0x40, 1,
                                             play->gameplayFrames, play->gameplayFrames * -5, 0x10, 0x10, 2, 0, 1,
                                             -8));

    // One CLOSE_DISPS per OPEN_DISPS at the same brace level, so guards set a flag instead of returning.
    if ((held == NULL) || sUltrahand.dropping) {
        // Nothing in hand: the tint is the whole selection feedback.
        Pacci_UhLightOff(play);
        skip = 1;
    } else if (!Pacci_FuseGetBox(play, held, &box)) {
        Pacci_UhLightOff(play);
        skip = 1;
    } else {
        // Held: the object becomes a light source. Ramped in over the same frames the tether
        // takes to reach it, so the room does not snap green on the grab frame.
        Pacci_UhLightAt(play, &box.center, CLAMP_MAX((f32)sUltrahand.vfxAge / 12.0f, 1.0f));
    }

    if (!skip) {

        Pacci_UhGetHandPos(player, &hand);
        {
            f32 reach = CLAMP_MAX((f32)sUltrahand.vfxAge / 9.0f, 1.0f);

            // Halo, mid and core strands, widest first: layering is what makes a flat tube glow.
            Pacci_UhDrawFlowPass(play, &POLY_XLU_DISP, &hand, &box.center, reach, 0x2AAA, 5.0f, PACCI_UH_FLOW_HALO_R,
                                 PACCI_UH_FLOW_HALO_G, PACCI_UH_FLOW_HALO_B, 55);
            Pacci_UhDrawFlowPass(play, &POLY_XLU_DISP, &hand, &box.center, reach, 0x6AAA, 2.4f, PACCI_UH_FLOW_MID_R,
                                 PACCI_UH_FLOW_MID_G, PACCI_UH_FLOW_MID_B, 150);
            Pacci_UhDrawFlowPass(play, &POLY_XLU_DISP, &hand, &box.center, reach, 0, 1.1f, PACCI_UH_FLOW_CORE_R,
                                 PACCI_UH_FLOW_CORE_G, PACCI_UH_FLOW_CORE_B, 255);
        }

        // Handles only while their control is in use, and only in the mode that binds them.
        buttons = sUhMode.active ? play->state.input[0].cur.button : 0;
        forward.x = Math_SinS(player->actor.focus.rot.y);
        forward.y = 0.0f;
        forward.z = Math_CosS(player->actor.focus.rot.y);
        side.x = Math_SinS(player->actor.focus.rot.y + 0x4000);
        side.y = 0.0f;
        side.z = Math_CosS(player->actor.focus.rot.y + 0x4000);

        if (buttons & BTN_L) {
            f32 yawRadius = fmaxf(box.half.x, box.half.z) + 30.0f;
            f32 pitchRadius = fmaxf(box.half.y, box.half.z) + 30.0f;
            // Blue follows left/right (yaw), red follows up/down (pitch).
            Pacci_UhDrawRotationRing(play, &POLY_XLU_DISP, &box.center, yawRadius, player->actor.focus.rot.y, 0, 40,
                                     135, 255);
            Pacci_UhDrawRotationRing(play, &POLY_XLU_DISP, &box.center, pitchRadius, player->actor.focus.rot.y, 1, 255,
                                     75, 55);
        } else if (buttons & BTN_R) {
            f32 maxHalf = fmaxf(fmaxf(box.half.x, box.half.y), box.half.z);
            f32 start = maxHalf * 0.35f;
            f32 length = maxHalf + 72.0f;
            // R owns the screen plane: red vertical, blue horizontal.
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &up, start, length, 255, 70, 50);
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &down, start, length, 255, 70, 50);
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &side, start, length, 45, 135, 255);
            side.x = -side.x;
            side.z = -side.z;
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &side, start, length, 45, 135, 255);
        } else if (sUhMode.active && (buttons & (BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT))) {
            f32 maxHalf = fmaxf(fmaxf(box.half.x, box.half.y), box.half.z);
            f32 start = maxHalf * 0.35f;
            f32 length = maxHalf + 72.0f;
            // Ground plane: red forward/back, blue right/left.
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &forward, start, length, 255, 70, 50);
            forward.x = -forward.x;
            forward.z = -forward.z;
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &forward, start, length, 255, 70, 50);
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &side, start, length, 45, 135, 255);
            side.x = -side.x;
            side.z = -side.z;
            Pacci_UhDrawArrow(play, &POLY_XLU_DISP, &box.center, &side, start, length, 45, 135, 255);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// DMG_FIRE (0x00020800) burns webs, lights torches and hurts enemies with no special case; our own
// collider because those actors' fire colliders are AC-only or placed from the player.
#define PACCI_UH_FIRE_DAMAGE 4  // two hearts, where the enemy's table defers to the toucher
#define PACCI_UH_FIRE_EFFECT 1  // the fire slot in the vanilla damage-effect tables
#define PACCI_UH_FIRE_PAD 12.0f // reach past the body's own surface, so it lights what it nears
// A fire actor without collision reads as a 12-unit cube; this is the floor on its volume.
#define PACCI_UH_FIRE_MIN_R 45.0f
#define PACCI_UH_FIRE_MIN_H 90.0f

static ColliderCylinder sUhFireCol;
static Actor* sUhFireOwner = NULL;

static void Pacci_UhFireOff(void) {
    // Collider_InitCylinder allocates nothing: dropping the owner stops the submission.
    sUhFireOwner = NULL;
}

// Red ice melts only for an En_Ice_Hono with params 0; the carried 0xFFFF flame sheds small ones,
// which kill themselves after 44 frames.
static void Pacci_UhIceTick(PlayState* play, Actor* actor) {
    if ((actor == NULL) || (actor->id != ACTOR_EN_ICE_HONO)) {
        return;
    }
    if ((sUltrahand.vfxAge % PACCI_UH_ICE_PERIOD) != 0) {
        return;
    }
    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ICE_HONO, actor->world.pos.x, actor->world.pos.y, actor->world.pos.z, 0,
                actor->shape.rot.y, 0, 0);
}

// Sitting Ruto rides on Link's back so his hands stay free to climb; the shield or the item
// coming out again takes her off.
static Actor* sBackRider = NULL;
static ActorFunc sBackRiderUpdate = NULL;
static u32 sBackRiderFlags = 0;
static s16 sBackRiderRoom = 0;

u8 Pacci_BackRiderActive(void) {
    return (sBackRider != NULL) ? 1 : 0;
}

// Hand her back to herself, wherever she currently is. She lands and sits back down on her own -
// her update never stopped running, so nothing has to be restarted.
void Pacci_BackRiderDrop(void) {
    Actor* rider = sBackRider;

    if ((rider != NULL) && (rider->update != NULL)) {
        if (sBackRiderUpdate != NULL) {
            rider->update = sBackRiderUpdate;
        }
        rider->flags = sBackRiderFlags;
        rider->room = (s8)sBackRiderRoom;
        rider->velocity.x = 0.0f;
        rider->velocity.y = 0.0f;
        rider->velocity.z = 0.0f;
    }
    sBackRider = NULL;
    sBackRiderUpdate = NULL;
}

static void Pacci_BackRiderPlace(Actor* rider, Player* player) {
    f32 sin = Math_SinS(player->actor.shape.rot.y);
    f32 cos = Math_CosS(player->actor.shape.rot.y);
    Vec3f torso = player->bodyPartsPos[PLAYER_BODYPART_TORSO];

    // Placed from the torso body part, so she leans and turns with him.
    rider->world.pos.x = torso.x - (sin * PACCI_BACKRIDE_BEHIND);
    rider->world.pos.y = torso.y + PACCI_BACKRIDE_RISE;
    rider->world.pos.z = torso.z - (cos * PACCI_BACKRIDE_BEHIND);
    rider->prevPos = rider->world.pos;
    rider->shape.rot.y = player->actor.shape.rot.y;
    rider->shape.rot.x = 0;
    rider->shape.rot.z = 0;
    rider->world.rot = rider->shape.rot;
    rider->velocity.x = 0.0f;
    rider->velocity.y = 0.0f;
    rider->velocity.z = 0.0f;
}

// Her own update still runs - she blinks, she talks, she offers the carry - and then she is put
// back on the shield. Same shape as the held-object wrapper and for the same reasons.
static void Pacci_BackRiderUpdate(Actor* thisx, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Pacci_UhKeepOnScreen(thisx);
    if (sBackRiderUpdate != NULL) {
        sBackRiderUpdate(thisx, play);
    }
    if ((thisx->update == NULL) || (player == NULL)) {
        return;
    }
    Pacci_BackRiderPlace(thisx, player);
    Pacci_AnchorSync(play, thisx);
}

static void Pacci_BackRiderTake(PlayState* play, Player* player, Actor* rider) {
    Pacci_BackRiderDrop();

    sBackRider = rider;
    sBackRiderUpdate = rider->update;
    sBackRiderFlags = rider->flags;
    sBackRiderRoom = rider->room;
    rider->update = Pacci_BackRiderUpdate;
    rider->room = -1; // she rides through doors with him
    rider->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    Pacci_AnchorTake(rider);
    Pacci_BackRiderPlace(rider, player);
    Player_PlaySfx(rider, NA_SE_SY_GET_ITEM);
}

// Every frame, item in hand or not: the shield drops her, and death or a scene change forgets her.
void Pacci_BackRiderTick(PlayState* play) {
    Player* player;

    if ((play == NULL) || (sBackRider == NULL)) {
        return;
    }
    if (sBackRider->update == NULL) {
        sBackRider = NULL;
        sBackRiderUpdate = NULL;
        return;
    }
    player = GET_PLAYER(play);
    if ((player != NULL) && (player->stateFlags1 & PLAYER_STATE1_SHIELDING)) {
        Pacci_BackRiderDrop();
    }
}

// A body set on a plate by the hand never ran the code that presses it, so the press is made here
// and outlives the release until the body leaves the plate.
static Actor* sPressBody = NULL;
static Actor* sPressSwitch = NULL;

static void Pacci_PlacePressForget(void) {
    sPressBody = NULL;
    sPressSwitch = NULL;
}

// Remember that this body is sitting on this switch. Called once the magnet has finished arriving.
static void Pacci_PlacePressSet(Actor* body, Actor* sw) {
    sPressBody = body;
    sPressSwitch = sw;
}

// Re-asserted every frame: the engine clears interactFlags each frame.
void Pacci_PlacePressTick(PlayState* play) {
    DynaPolyActor* dyna;
    f32 dx;
    f32 dz;
    f32 plateTop;

    if ((play == NULL) || (sPressBody == NULL) || (sPressSwitch == NULL)) {
        return;
    }
    if ((sPressBody->update == NULL) || (sPressSwitch->update == NULL)) {
        Pacci_PlacePressForget();
        return;
    }
    // Still over the plate? Measured in XZ against the switch, and in Y against the plate's own
    // top: lifting the body off has to release the switch, and so does sliding it away.
    dx = sPressBody->world.pos.x - sPressSwitch->world.pos.x;
    dz = sPressBody->world.pos.z - sPressSwitch->world.pos.z;
    if (((dx * dx) + (dz * dz)) > (PACCI_PRESS_HOLD * PACCI_PRESS_HOLD)) {
        Pacci_PlacePressForget();
        return;
    }
    if (Pacci_UhDynaTopY(play, sPressSwitch, &plateTop)) {
        f32 dy = sPressBody->world.pos.y - plateTop;

        if ((dy > PACCI_PRESS_HOLD) || (dy < -PACCI_PRESS_HOLD)) {
            Pacci_PlacePressForget();
            return;
        }
    }
    // The switch has to BE dynapoly for this to mean anything; it always is, but a cast to
    // DynaPolyActor on the strength of an actor id is worth checking rather than assuming.
    dyna = DynaPoly_GetActor(&play->colCtx, ((DynaPolyActor*)sPressSwitch)->bgId);
    if ((dyna == NULL) || (&dyna->actor != sPressSwitch)) {
        Pacci_PlacePressForget();
        return;
    }
    DynaPolyActor_SetActorOnTop(dyna);
    DynaPolyActor_SetSwitchPressed(dyna);
}

// The jaw is a rotation (BgDodoago_Init, z_bg_dodoago.c:131-133) and its collision rides the SRT.
static void Pacci_UhLockedPose(PlayState* play, Actor* actor) {
    const PacciUhTraitRow* row;
    s16 want;
    s32 open;

    if (!(sUltrahand.traits & PACCI_UH_TRAIT_JAW)) {
        return;
    }
    row = Pacci_UhTraitRow(actor);
    if (row == NULL) {
        return;
    }
    open = Flags_GetSwitch(play, (actor->params >> row->pathShift) & row->pathMask);
    want = open ? PACCI_UH_LOCKED_OPEN_X : 0;
    Math_SmoothStepToS(&actor->shape.rot.x, want, 4, 0x0400, 0x20);

    // Open is the flag, the jaw and both eyes together (z_bg_dodoago.c:131-134).
    if (actor->id == ACTOR_BG_DODOAGO) {
        play->roomCtx.unk_74[0] = play->roomCtx.unk_74[1] = open ? 255 : 0;
    }
}

// The drawbridge's own animation brings its chains, timing and sounds; posing it would fight it.
static void Pacci_UhHingeInput(Actor* actor, u8 edge) {
    BgSpot00Hanebasi* bridge;
    BgSpot00Hanebasi* chain;
    s16 want;

    if ((actor == NULL) || !(sUltrahand.traits & PACCI_UH_TRAIT_HINGE) || (actor->id != ACTOR_BG_SPOT00_HANEBASI) ||
        (actor->child == NULL)) {
        return;
    }
    if (edge & 1) {
        want = -0x4000;
    } else if (edge & 2) {
        want = 0;
    } else {
        return;
    }
    bridge = (BgSpot00Hanebasi*)actor;
    chain = (BgSpot00Hanebasi*)actor->child;
    if (bridge->destAngle == want) {
        return;
    }
    bridge->destAngle = want;
    chain->destAngle = (want != 0) ? -0xFE0 : 0; // the chain swings a fraction of the deck's arc
    bridge->actionFunc = BgSpot00Hanebasi_DrawbridgeRiseAndFall;
}

// D-pad on a HEIGHT body changes how BIG it is, because that is the only thing about it worth
// changing. Same up/down gesture that raises and lowers anything else - it just grows instead.
static void Pacci_UhHeightInput(Actor* actor, u8 edge) {
    EnSiofuki* spout;

    if ((actor == NULL) || !(sUltrahand.traits & PACCI_UH_TRAIT_HEIGHT) || (actor->id != ACTOR_EN_SIOFUKI)) {
        return;
    }
    spout = (EnSiofuki*)actor;
    if (edge & 1) {
        spout->targetHeight += PACCI_UH_HEIGHT_STEP;
    } else if (edge & 2) {
        spout->targetHeight -= PACCI_UH_HEIGHT_STEP;
    } else {
        return;
    }
    // Clamped to something the spout can actually be. It smooth-steps currentHeight toward this,
    // so the growing and shrinking is its own animation and not a jump.
    spout->targetHeight = CLAMP(spout->targetHeight, 0.0f, PACCI_UH_HEIGHT_MAX);
    Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// A LOCKED body's flag follows the D-pad: up is on, down is off.
static void Pacci_UhLockedInput(PlayState* play, u8 edge) {
    Actor* actor = sUltrahand.held;
    const PacciUhTraitRow* row;
    s32 flag;
    u8 want;

    if ((actor == NULL) || !(sUltrahand.traits & PACCI_UH_TRAIT_LOCKED) ||
        !(sUltrahand.traits & PACCI_UH_TRAIT_SETS_FLAG) || (sUltrahand.traits & PACCI_UH_TRAIT_HINGE)) {
        return; // a hinge answers to its own animation, not to a flag
    }
    if (edge & 1) {
        want = 1;
    } else if (edge & 2) {
        want = 0;
    } else {
        return;
    }
    row = Pacci_UhTraitRow(actor);
    if (row == NULL) {
        return;
    }
    flag = (actor->params >> row->pathShift) & row->pathMask;
    if ((Flags_GetSwitch(play, flag) != 0) == (want != 0)) {
        return;
    }
    if (want) {
        Flags_SetSwitch(play, flag);
    } else {
        Flags_UnsetSwitch(play, flag);
    }
    Audio_PlaySoundGeneral(want ? NA_SE_SY_GET_ITEM : NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// Travel is measured from the grab, so only this carry counts; debounced on the flag itself, which
// stays right across saves and reloads.
static void Pacci_UhFlagTick(PlayState* play, Actor* actor) {
    const PacciUhTraitRow* row;
    f32 dy;
    f32 dx;
    f32 dz;
    s32 flag;
    u8 want;

    if (!(sUltrahand.traits & PACCI_UH_TRAIT_SETS_FLAG)) {
        return;
    }
    dy = actor->world.pos.y - sUltrahand.railPos.y;
    if (sUltrahand.flagDir != 0) {
        f32 travelled = (sUltrahand.flagDir > 0) ? dy : -dy;

        if (travelled >= PACCI_UH_FLAG_TRAVEL) {
            want = 1;
        } else if ((sUltrahand.traits & PACCI_UH_TRAIT_CLEARS_FLAG) && (travelled <= -PACCI_UH_FLAG_TRAVEL)) {
            want = 0;
        } else {
            return;
        }
    } else {
        dx = actor->world.pos.x - sUltrahand.railPos.x;
        dz = actor->world.pos.z - sUltrahand.railPos.z;
        if (((dx * dx) + (dy * dy) + (dz * dz)) < (PACCI_UH_FLAG_TRAVEL * PACCI_UH_FLAG_TRAVEL)) {
            return;
        }
        want = 1;
    }
    row = Pacci_UhTraitRow(actor);
    if (row == NULL) {
        return;
    }
    flag = (actor->params >> row->pathShift) & row->pathMask;
    // Params with bits outside the flag field mean the actor has no switch of its own.
    if ((sUltrahand.traits & PACCI_UH_TRAIT_FLAG_STRICT) && (actor->params != (s16)(flag << row->pathShift))) {
        return;
    }
    if ((Flags_GetSwitch(play, flag) != 0) == (want != 0)) {
        return;
    }
    if (want) {
        Flags_SetSwitch(play, flag);
    } else {
        Flags_UnsetSwitch(play, flag);
    }
    Audio_PlaySoundGeneral(want ? NA_SE_SY_GET_ITEM : NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// The body slides to the offered spot; measured from the anchor so walking away takes it back.
static void Pacci_UhPlaceMagnet(Actor* actor) {
    Vec3f want;
    f32 blend;

    if (sPlacePv.valid) {
        Math_StepToF(&sUltrahand.placeBlend, 1.0f, PACCI_PLACE_PULL);
    } else {
        Math_StepToF(&sUltrahand.placeBlend, 0.0f, PACCI_PLACE_PULL);
    }
    blend = sUltrahand.placeBlend;
    if (blend <= 0.0f) {
        return;
    }
    // sPlacePv.pos is stale by one frame - the preview runs after the carry - and that is fine at
    // this speed. It is a fixed point in the world, not something that moves.
    want = sPlacePv.pos;
    actor->world.pos.x += (want.x - actor->world.pos.x) * blend;
    actor->world.pos.y += (want.y - actor->world.pos.y) * blend;
    actor->world.pos.z += (want.z - actor->world.pos.z) * blend;

    // The slide is placement, not a throw: no inertia is handed to the release.
    sUltrahand.carryVel.x = 0.0f;
    sUltrahand.carryVel.y = 0.0f;
    sUltrahand.carryVel.z = 0.0f;

    // Arrived on a switch: from here it is held down, and stays held after the release.
    if ((blend >= PACCI_PLACE_ARRIVED) && (sPlacePv.sw != NULL)) {
        Pacci_PlacePressSet(actor, sPlacePv.sw);
    }

    // Squares up on the way in, so it arrives level instead of arriving and then jerking straight.
    // A switch hands over its whole orientation; plain ground only takes the tilt out.
    if (blend > 0.5f) {
        s16 wantX = (sPlacePv.sw != NULL) ? sPlacePv.sw->shape.rot.x : 0;
        s16 wantY = (sPlacePv.sw != NULL) ? sPlacePv.sw->shape.rot.y : actor->shape.rot.y;
        s16 wantZ = (sPlacePv.sw != NULL) ? sPlacePv.sw->shape.rot.z : 0;

        Math_SmoothStepToS(&actor->shape.rot.x, wantX, 3, 0x1000, 0x40);
        Math_SmoothStepToS(&actor->shape.rot.y, wantY, 3, 0x1000, 0x40);
        Math_SmoothStepToS(&actor->shape.rot.z, wantZ, 3, 0x1000, 0x40);
        actor->world.rot = actor->shape.rot;
        sUltrahand.baseRot = actor->shape.rot;
    }
}

static void Pacci_UhFireTick(PlayState* play, Actor* actor) {
    PacciFuseBox box;
    CombatColliderConfig cfg;
    Vec3f pos;

    if (!(sUltrahand.traits & PACCI_UH_TRAIT_BURNS) || (actor == NULL) || (actor->update == NULL)) {
        Pacci_UhFireOff();
        return;
    }
    if (!Pacci_FuseGetBox(play, actor, &box)) {
        return;
    }
    cfg.dmgFlags = DMG_FIRE;
    cfg.damage = PACCI_UH_FIRE_DAMAGE;
    cfg.effect = PACCI_UH_FIRE_EFFECT;
    // Radius from the diagonal, so a wide curtain seen corner-on burns whole.
    cfg.radius = sqrtf((box.half.x * box.half.x) + (box.half.z * box.half.z)) + PACCI_UH_FIRE_PAD;
    cfg.height = (box.half.y * 2.0f) + PACCI_UH_FIRE_PAD;
    if (cfg.radius < PACCI_UH_FIRE_MIN_R) {
        cfg.radius = PACCI_UH_FIRE_MIN_R;
    }
    if (cfg.height < PACCI_UH_FIRE_MIN_H) {
        cfg.height = PACCI_UH_FIRE_MIN_H;
    }

    // Re-armed whenever the body changes, because Collider_SetCylinder bakes the OWNER in and the
    // game needs to attribute the burn to the flame rather than to whatever we held last.
    if (sUhFireOwner != actor) {
        Combat_InitCylinder(play, &sUhFireCol, actor, &cfg);
        sUhFireOwner = actor;
    }
    // A cylinder is positioned by its BASE, not its centre.
    pos.x = box.center.x;
    pos.y = box.center.y - box.half.y - (PACCI_UH_FIRE_PAD * 0.5f);
    pos.z = box.center.z;
    Combat_UpdateCylinder(&sUhFireCol, &pos, &cfg);
    Combat_RegisterCollider(play, &sUhFireCol);
    // AT_HIT has to be cleared by hand or the collider counts as spent and stops registering
    // hits - item_spinner.c:50-55 does the same, and for the same reason.
    if (Combat_CheckHit(&sUhFireCol)) {
        sUhFireCol.base.atFlags &= ~AT_HIT;
    }
}

// Our own root and parts are live dynapoly while falling; a probe from one would hit the others.
static u8 Pacci_UhIsOwnBody(PlayState* play, s32 bgId) {
    DynaPolyActor* dyna = DynaPoly_GetActor(&play->colCtx, bgId);
    Actor* hitActor;

    if (dyna == NULL) {
        return 0;
    }
    hitActor = &dyna->actor;
    if ((hitActor == sUltrahand.held) || (hitActor == sFuse.root)) {
        return 1;
    }
    for (u8 i = 0; i < sFuse.count; i++) {
        if (sFuse.parts[i].actor == hitActor) {
            return 1;
        }
    }
    return 0;
}

// Where the footprint lands: the engine's bg check reads one point at the origin. The probe starts
// below the box, since our own falling surface would stop a ray from inside it.
static u8 Pacci_UhGroundUnder(PlayState* play, Actor* actor, f32* outY) {
    static const f32 sProbeX[5] = { -1.0f, 1.0f, -1.0f, 1.0f, 0.0f };
    static const f32 sProbeZ[5] = { -1.0f, -1.0f, 1.0f, 1.0f, 0.0f };
    PacciFuseBox box;
    Vec3f from;
    Vec3f to;
    Vec3f hit;
    CollisionPoly* poly;
    s32 bgId;
    f32 best = 0.0f;
    f32 bottom;
    f32 sin;
    f32 cos;
    u8 found = 0;
    u8 i;

    if (!Pacci_FuseGetBox(play, actor, &box)) {
        return 0;
    }
    bottom = box.center.y - box.half.y;
    sin = Math_SinS(box.yaw);
    cos = Math_CosS(box.yaw);

    // Four bottom corners and the centre. Five probes catch an edge hanging over a drop
    // without turning every falling frame into a raycast storm.
    for (i = 0; i < 5; i++) {
        f32 lx = sProbeX[i] * box.half.x;
        f32 lz = sProbeZ[i] * box.half.z;

        from.x = box.center.x + ((lx * cos) + (lz * sin));
        from.z = box.center.z + ((lz * cos) - (lx * sin));
        from.y = bottom - 1.0f;
        to.x = from.x;
        to.z = from.z;
        to.y = bottom - PACCI_UH_GROUND_PROBE;

        // Steps past our own pieces: the first hit under the root is often a part glued below it.
        for (u8 attempt = 0; attempt < 5; attempt++) {
            if (!BgCheck_ProjectileLineTest(&play->colCtx, &from, &to, &hit, &poly, true, true, true, true, &bgId)) {
                break;
            }
            if (Pacci_UhIsOwnBody(play, bgId)) {
                from.y = hit.y - 1.0f;
                if (from.y <= to.y) {
                    break;
                }
                continue;
            }
            // HIGHEST hit wins: that is the surface the body comes to rest on first, and it is
            // why a structure straddling a step ends up on the step and not through it.
            if (!found || (hit.y > best)) {
                best = hit.y;
                found = 1;
            }
            break;
        }
    }
    if (!found) {
        return 0;
    }
    // Floor-under-the-box -> where the ORIGIN goes, since the origin is not the box bottom.
    *outY = best + (actor->world.pos.y - bottom);
    return 1;
}

// Every part is asked and the highest implied root Y wins: the first piece to touch down stops it.
static u8 Pacci_UhAssemblyGround(PlayState* play, Actor* root, f32* outY) {
    f32 best = 0.0f;
    u8 found = 0;
    f32 y;

    if (Pacci_UhGroundUnder(play, root, &y)) {
        best = y;
        found = 1;
    }
    if (sFuse.root == root) {
        for (u8 i = 0; i < sFuse.count; i++) {
            Actor* part = sFuse.parts[i].actor;

            if ((part == NULL) || (part->update == NULL)) {
                continue;
            }
            if (!Pacci_UhGroundUnder(play, part, &y)) {
                continue;
            }
            // y is where THAT PIECE'S origin would rest. The root sits a fixed distance from it,
            // so the same landing expressed in the root's terms is y plus that distance.
            y += root->world.pos.y - part->world.pos.y;
            if (!found || (y > best)) {
                best = y;
                found = 1;
            }
        }
    }
    if (!found) {
        return 0;
    }
    *outY = best;
    return 1;
}

// A recipe, not geometry: the merged header is overwritten by the next weld.
typedef struct {
    s16 actorId;
    s16 params;
    Vec3f offset;
    Vec3s rot;
} PacciBlueprintPiece;

static struct {
    u8 used;
    u8 count;
    // Actors need their object loaded, and the object bank is per scene.
    s16 scene;
    PacciBlueprintPiece piece[PACCI_FUSE_MAX_PARTS + 1];
} sBlueprint = { 0 };

u8 Pacci_BlueprintStored(void) {
    return sBlueprint.used;
}

static void Pacci_BlueprintDeny(void) {
    Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

u8 Pacci_BlueprintSave(PlayState* play) {
    Actor* doomed[PACCI_FUSE_MAX_PARTS + 1];
    Actor* root = sUltrahand.held;
    PacciFuseBox box;
    u8 n = 0;
    u8 i;

    if ((root == NULL) || sUltrahand.dropping) {
        Pacci_BlueprintDeny();
        return 0;
    }
    // Dynapoly only: a piece without collision would come back as something you fall through.
    if (!Pacci_FuseGetBox(play, root, &box) || !box.dyna) {
        Pacci_BlueprintDeny();
        return 0;
    }
    if ((sFuse.count > 0) && (sFuse.root != root)) {
        Pacci_BlueprintDeny();
        return 0;
    }
    for (i = 0; i < sFuse.count; i++) {
        Actor* part = sFuse.parts[i].actor;

        if ((part == NULL) || (part->update == NULL) || !Pacci_FuseGetBox(play, part, &box) || !box.dyna) {
            Pacci_BlueprintDeny();
            return 0;
        }
    }

    // Validated in full before anything is written or killed: a half-stored structure with
    // half its pieces deleted is not something the player can undo.
    sBlueprint.piece[0].actorId = root->id;
    sBlueprint.piece[0].params = root->params;
    sBlueprint.piece[0].offset.x = 0.0f;
    sBlueprint.piece[0].offset.y = 0.0f;
    sBlueprint.piece[0].offset.z = 0.0f;
    sBlueprint.piece[0].rot.x = 0;
    sBlueprint.piece[0].rot.y = 0;
    sBlueprint.piece[0].rot.z = 0;
    doomed[0] = root;
    n = 1;

    for (i = 0; i < sFuse.count; i++) {
        sBlueprint.piece[n].actorId = sFuse.parts[i].actor->id;
        sBlueprint.piece[n].params = sFuse.parts[i].actor->params;
        sBlueprint.piece[n].offset = sFuse.parts[i].offset;
        sBlueprint.piece[n].rot = sFuse.parts[i].rot;
        doomed[n] = sFuse.parts[i].actor;
        n++;
    }

    // Updates and collision go back before any kill, or Destroy runs on a half-rewired body.
    Pacci_FuseRelease();
    Pacci_UltrahandLetGo();
    for (i = 0; i < n; i++) {
        if ((doomed[i] != NULL) && (doomed[i]->update != NULL)) {
            Actor_Kill(doomed[i]);
        }
    }

    sBlueprint.used = 1;
    sBlueprint.count = n;
    sBlueprint.scene = play->sceneNum;
    Audio_PlaySoundGeneral(NA_SE_SY_GET_ITEM, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    return 1;
}

u8 Pacci_BlueprintSummon(PlayState* play, Player* player) {
    Actor* spawned[PACCI_FUSE_MAX_PARTS + 1];
    Vec3f base;
    s16 yaw = player->actor.focus.rot.y;
    f32 sin = Math_SinS(yaw);
    f32 cos = Math_CosS(yaw);
    u8 built = 0;
    u8 i;

    if (!sBlueprint.used || Pacci_IsHoldingUltrahand() || (sFuse.count > 0)) {
        return 0;
    }
    if (sBlueprint.scene != play->sceneNum) {
        Pacci_BlueprintDeny();
        return 0;
    }
    // Magic LAST among the checks and before the first spawn: charging for a summon that
    // then fails to build is the one outcome with no way back.
    if (!Magic_RequestChange(play, PACCI_UH_SUMMON_COST, MAGIC_CONSUME_NOW)) {
        Pacci_BlueprintDeny();
        return 0;
    }
    // The spend is already done; without this the meter keeps flashing and blocks every other spell.
    Magic_Reset(play);

    base.x = player->actor.world.pos.x + (sin * PACCI_UH_DIST_MIN);
    base.y = player->actor.world.pos.y + PACCI_UH_SUMMON_RISE;
    base.z = player->actor.world.pos.z + (cos * PACCI_UH_DIST_MIN);

    for (i = 0; i < sBlueprint.count; i++) {
        Vec3f off = sBlueprint.piece[i].offset;

        spawned[i] =
            Actor_Spawn(&play->actorCtx, play, sBlueprint.piece[i].actorId, base.x + ((off.x * cos) + (off.z * sin)),
                        base.y + off.y, base.z + ((off.z * cos) - (off.x * sin)), sBlueprint.piece[i].rot.x,
                        sBlueprint.piece[i].rot.y + yaw, sBlueprint.piece[i].rot.z, sBlueprint.piece[i].params);
        if (spawned[i] == NULL) {
            break;
        }
        built++;
    }
    // The actor pool can refuse. Roll the whole thing back rather than leave a half-built
    // structure standing in front of the player with the magic already spent.
    if (built != sBlueprint.count) {
        for (i = 0; i < built; i++) {
            Actor_Kill(spawned[i]);
        }
        Pacci_BlueprintDeny();
        return 0;
    }

    Pacci_UltrahandTake(player, spawned[0]);
    for (i = 1; i < sBlueprint.count; i++) {
        Pacci_FuseAdopt(spawned[0], spawned[i], &sBlueprint.piece[i].offset, &sBlueprint.piece[i].rot);
    }
    sBlueprint.used = 0;
    sBlueprint.count = 0;
    return 1;
}

#define PACCI_UH_ROT_SNAP 0x2000 // 45 degrees per press, TotK-style snapping
// Movement is continuous while held; rotation snaps once per press.
#define PACCI_UH_MOVE_RATE 6.0f
#define PACCI_UH_DIST_RATE 8.0f
#define PACCI_UH_SIDE_MIN -120.0f
#define PACCI_UH_SIDE_MAX 120.0f
#define PACCI_UH_HEIGHT_MIN -80.0f
#define PACCI_UH_HEIGHT_MAX 160.0f

u8 Pacci_UltrahandModeActive(void) {
    return sUhMode.active;
}

void Pacci_UltrahandModeEnter(PlayState* play, Player* player) {
    sUhMode.active = 1;
    sUhMode.heightOff = 0.0f;
    sUhMode.sideOff = 0.0f;
    sUhMode.prevDpad = 0;
    sUhMode.summonHold = 0;
    sUhHighlightTarget = NULL;
    Audio_PlaySoundGeneral(NA_SE_SY_GET_ITEM, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

void Pacci_UltrahandModeExit(PlayState* play) {
    if (sUltrahand.held != NULL) {
        // The per-frame update keeps running, so the drop finishes after the mode is gone.
        Pacci_UltrahandBeginDrop();
    }
    Pacci_UhTintClear();
    sUhMode.active = 0;
    sUhMode.heightOff = 0.0f;
    sUhMode.sideOff = 0.0f;
    sUhMode.prevDpad = 0;
    sUhMode.summonHold = 0;
    sUhHighlightTarget = NULL;
    Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

u8 Pacci_UltrahandModeUpdate(PlayState* play, Player* player) {
    Input* input;
    u16 cur;
    u8 dpad;
    u8 edge;
    u8 withL;
    u8 withR;

    if (!sUhMode.active) {
        return 0;
    }

    input = &play->state.input[0];
    cur = input->cur.button;

    // The tint table is wiped first; the draw runs after the whole update.
    Pacci_UhTintClear();

    // Hauling owns the frame; B or A lets go.
    if (Pacci_PullActive()) {
        u8 pullPad = 0;

        if (cur & BTN_DUP) {
            pullPad |= 1;
        }
        if (cur & BTN_DDOWN) {
            pullPad |= 2;
        }
        if (CHECK_BTN_ALL(input->press.button, BTN_B) || CHECK_BTN_ALL(input->press.button, BTN_A)) {
            Pacci_PullStop(play);
            Pacci_UltrahandModeExit(play);
            return 1;
        }
        Pacci_PullTick(play, player, pullPad);
        return 1;
    }

    Pacci_FuseWiggleDetach(play, player, input);

    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        Pacci_UltrahandModeExit(play);
        return 1;
    }

    // L + R + A stores the structure. Tested BEFORE the plain A branch below, which would
    // otherwise weld or drop on the same press and there would be nothing left to store.
    if (CHECK_BTN_ALL(input->press.button, BTN_A) && (cur & BTN_L) && (cur & BTN_R)) {
        Pacci_BlueprintSave(play);
        return 1;
    }

    // Holding C rebuilds the stored structure: tap opens the mode, keep holding to summon.
    if (cur & (BTN_CUP | BTN_CDOWN | BTN_CLEFT | BTN_CRIGHT)) {
        if (sUhMode.summonHold < PACCI_UH_SUMMON_HOLD) {
            sUhMode.summonHold++;
            if (sUhMode.summonHold == PACCI_UH_SUMMON_HOLD) {
                Pacci_BlueprintSummon(play, player);
            }
        }
    } else {
        sUhMode.summonHold = 0;
    }

    // A welds what is offered, else drops, else grabs: welding first lets pieces be added without letting go.
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        if (Pacci_IsHoldingUltrahand()) {
            // A live bomb is lit by the attach button instead of welded.
            if (Pacci_UhBombDetonate(play)) {
                Pacci_UltrahandModeExit(play);
            } else if (!Pacci_FuseTryAttach(play)) {
                // A release ends the mode: with nothing in hand there is nothing for it to control.
                Pacci_UltrahandModeExit(play);
            }
        } else {
            Pacci_CastUltrahand(play, player);
        }
        return 1;
    }

    // D-pad edges, detected here rather than read from press.button: the player
    // actor consumes those bits for its own item handling before this runs.
    dpad = 0;
    if (cur & BTN_DUP) {
        dpad |= 1;
    }
    if (cur & BTN_DDOWN) {
        dpad |= 2;
    }
    if (cur & BTN_DLEFT) {
        dpad |= 4;
    }
    if (cur & BTN_DRIGHT) {
        dpad |= 8;
    }
    edge = dpad & ~sUhMode.prevDpad;
    sUhMode.prevDpad = dpad;

    // A locked body answers the pad instead of moving, and answers only edges - a held D-up is one
    // instruction, not sixty.
    Pacci_UhLockedInput(play, edge);
    Pacci_UhHeightInput(sUltrahand.held, edge);
    Pacci_UhHingeInput(sUltrahand.held, edge);

    // Runs on any pad STATE, not just an edge: continuous moves need every frame.
    if (Pacci_IsHoldingUltrahand() && (dpad != 0)) {
        withL = (cur & BTN_L) ? 1 : 0;
        withR = (cur & BTN_R) ? 1 : 0;

        // Pad alone pushes and slides, L rotates in 45-degree snaps, R raises and slides.
        if (withL) {
            if (edge & 8) {
                sUltrahand.baseRot.y += PACCI_UH_ROT_SNAP;
            }
            if (edge & 4) {
                sUltrahand.baseRot.y -= PACCI_UH_ROT_SNAP;
            }
            if (edge & 1) {
                sUltrahand.baseRot.x += PACCI_UH_ROT_SNAP;
            }
            if (edge & 2) {
                sUltrahand.baseRot.x -= PACCI_UH_ROT_SNAP;
            }
        } else if (withR) {
            if (dpad & 1) {
                sUhMode.heightOff += PACCI_UH_MOVE_RATE;
            }
            if (dpad & 2) {
                sUhMode.heightOff -= PACCI_UH_MOVE_RATE;
            }
            sUhMode.heightOff = CLAMP(sUhMode.heightOff, PACCI_UH_HEIGHT_MIN, PACCI_UH_HEIGHT_MAX);

            if (dpad & 8) {
                sUhMode.sideOff += PACCI_UH_MOVE_RATE;
            }
            if (dpad & 4) {
                sUhMode.sideOff -= PACCI_UH_MOVE_RATE;
            }
            sUhMode.sideOff = CLAMP(sUhMode.sideOff, PACCI_UH_SIDE_MIN, PACCI_UH_SIDE_MAX);
        } else {
            if (dpad & 1) {
                sUltrahand.distance += PACCI_UH_DIST_RATE;
            }
            if (dpad & 2) {
                sUltrahand.distance -= PACCI_UH_DIST_RATE;
            }
            sUltrahand.distance = CLAMP(sUltrahand.distance, PACCI_UH_DIST_MIN, PACCI_UH_DIST_MAX);

            if (dpad & 8) {
                sUhMode.sideOff += PACCI_UH_MOVE_RATE;
            }
            if (dpad & 4) {
                sUhMode.sideOff -= PACCI_UH_MOVE_RATE;
            }
            sUhMode.sideOff = CLAMP(sUhMode.sideOff, PACCI_UH_SIDE_MIN, PACCI_UH_SIDE_MAX);
        }
        // Only the snapped rotations click; a continuous move would machine-gun it.
        if (withL && (edge & 15)) {
            Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    }

    // Z puts the orientation back to however the object was sitting when you grabbed it —
    // TotK's ZL. Untangling a piece you have over-rotated is otherwise seven more presses.
    if (Pacci_IsHoldingUltrahand() && CHECK_BTN_ALL(input->press.button, BTN_Z)) {
        sUltrahand.baseRot = sUltrahand.grabRot;
        Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }

    if (!Pacci_IsHoldingUltrahand()) {
        Pacci_HighlightUltrahandTarget(play);
    }

    Pacci_UpdateUltrahand(play, player);
    // Strict order: the carry moves the root, the formation follows it, and only then
    // is it meaningful to ask where a new weld would land.
    Pacci_FuseFollow(play);
    Pacci_FuseUpdatePreview(play, player);
    Pacci_PlaceUpdatePreview(play); // after the weld preview, which outranks it
    return 1;
}


// Putting the hand away drops what it carries; the fall keeps running from the per-frame ticks.
void Pacci_DropUltrahand(void) {
    if (sUltrahand.held != NULL) {
        Pacci_UltrahandBeginDrop();
    }
}

// Full reset for a new file or scene teardown; unequip only lets go.
void Pacci_ReleaseAll(PlayState* play) {
    Pacci_UhTintClear(); // borrowed draw pointers must not outlive the subsystem
    Pacci_UhFireOff();
    Pacci_PlacePressForget();
    Pacci_BackRiderDrop();
    sUhCutTarget = NULL;
    sUhCutTimer = 0;
    sUhThrowActor = NULL;
    sUhThrowTimer = 0;
    Pacci_PullStop(play);
    Pacci_AnchorClear();
    Pacci_UhLightOff(play);
    Pacci_DropUltrahand();
}

// A hit or a void-out drops the body; a scene change hands every borrowed update and draw back first.
void Pacci_UhAbortTick(PlayState* play) {
    Player* player;
    u8 busy;

    if (play == NULL) {
        return;
    }
    busy = (Pacci_UltrahandModeActive() || Pacci_IsHoldingUltrahand()) ? 1 : 0;
    if (!busy) {
        return;
    }
    if ((play->transitionTrigger != TRANS_TRIGGER_OFF) || (gSaveContext.respawnFlag != 0)) {
        Pacci_ReleaseAll(play);
        sUhMode.active = 0;
        sUhMode.prevDpad = 0;
        sUhMode.summonHold = 0;
        return;
    }
    player = GET_PLAYER(play);
    if ((player != NULL) && (player->stateFlags1 & (PLAYER_STATE1_DAMAGED | PLAYER_STATE1_DEAD))) {
        Pacci_UltrahandModeExit(play); // drops what is held on the way out
    }
}


#define UH_KEY "nei.ultrahand"
#define UH_MOD "ultrahand"
#define UH_PIECES_FIELD "pieces"
#define UH_PIECES_OPTION "ultrahand_pieces"
#define UH_BUTTON_COUNT 8
#define UH_MAX_PIECES 10
#define UH_DEFAULT_PIECES_INDEX 2
#define UH_REACH_PITCH 0x1800
#define UH_GIVE_SCALE 0.17f
#define UH_TEXT_SIZE 192
// While the mode is up it owns these: A grabs, B leaves, R and the pad move what is held.
#define UH_MODE_BUTTONS (BTN_A | BTN_B | BTN_R | BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT)

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconUltrahandTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gUltrahandNameTex";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_ultrahand/gUltrahandGiveDL";
static const ALIGN_ASSET(2) char sGiveXluDL[] = "__OTR__objects/object_nei_ultrahand/gUltrahandGiveXluDL";

static const u16 sItemButtons[UH_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                   BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };
static const char* const sPieceCounts[UH_MAX_PIECES] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };

static const SOHModRandoOption sPiecesOption = {
    sizeof(SOHModRandoOption),
    UH_PIECES_OPTION,
    "Ultrahand Pieces",
    "How many Ultrahand pieces are shuffled. It works only once every one of them is found.",
    sPieceCounts,
    UH_MAX_PIECES,
    UH_DEFAULT_PIECES_INDEX,
    SOH_MOD_RANDO_WIDGET_SLIDER,
};

// One copy in the pool plus the option's index: the slider's "1" is index 0.
static const SOHCustomItemRandomizer sUltrahandLogic = {
    sizeof(SOHCustomItemRandomizer), SOH_CUSTOM_ITEM_RANDO_PROGRESSIVE, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, "", "", "",
    UH_PIECES_OPTION,
};

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDrawEnd", "OnLoadFile" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static u8 sPieces;
static bool sIsInHand;
static bool sIsWaitingForRelease;
static bool sIsReachPoseHeld;
static bool sIsModeInputBlocked;
static char sGetItemText[UH_TEXT_SIZE];
static char sPauseText[UH_TEXT_SIZE];

static u16 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < UH_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, UH_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static u8 RequiredPieces(void) {
    return (SOH_MOD_API_HAS(sApi, GetRandoOption) ? sApi->GetRandoOption(UH_PIECES_OPTION) : UH_DEFAULT_PIECES_INDEX) +
           1;
}

static bool IsAssembled(void) {
    return sPieces >= RequiredPieces();
}

static void RefreshTexts(void) {
    u8 required = RequiredPieces();

    if (IsAssembled()) {
        snprintf(sGetItemText, UH_TEXT_SIZE,
                 "The %%rUltrahand%%w is whole again!^Press %%y\xA1%%w to reach out: %%y\x9F%%w grabs,&the "
                 "%%y\xA0%%w button lets go.");
        snprintf(sPauseText, UH_TEXT_SIZE,
                 "%%rUltrahand&%%w%%y\xA1%%w to reach, %%y\x9F%%w grab, %%y\xA0%%w leave.&D-pad moves, L turns, R "
                 "raises.");
    } else {
        snprintf(sGetItemText, UH_TEXT_SIZE, "You got a piece of the %%rUltrahand%%w!&%u of %u found.", sPieces,
                 required);
        snprintf(sPauseText, UH_TEXT_SIZE, "%%rUltrahand&%%w%u of %u pieces found. It stirs only&once it is whole.",
                 sPieces, required);
    }
    Z64Items_UpdateText(sApi, UH_KEY, SOH_ITEM_TEXT_GET_ITEM, sGetItemText);
    Z64Items_UpdateText(sApi, UH_KEY, SOH_ITEM_TEXT_PAUSE, sPauseText);
}

static void ReceivePiece(const char* key) {
    sPieces = MIN(sPieces + 1, UH_MAX_PIECES);
    sApi->StorageSet(UH_MOD, UH_PIECES_FIELD, &sPieces, sizeof(sPieces));
    RefreshTexts();
}

static void LoadPieces(int32_t fileNum) {
    sPieces = 0;
    sApi->StorageGet(UH_MOD, UH_PIECES_FIELD, &sPieces, sizeof(sPieces));
    sIsInHand = false;
    RefreshTexts();
}

static bool CanUseUltrahand(Player* player, PlayState* play) {
    return IsAssembled();
}

static void TakeOutUltrahand(PlayState* play, Player* player) {
    sIsInHand = true;
    sIsReachPoseHeld = false;
    // The press that brought the hand out is not the one that opens the mode.
    sIsWaitingForRelease = true;
}

// The press is read raw each frame in UpdateUltrahandWorld; here it only has to be consumed.
static void PressUltrahand(Player* player, PlayState* play) {
}

static void PutUltrahandAway(Player* player, PlayState* play) {
    if (Pacci_UltrahandModeActive()) {
        Pacci_UltrahandModeExit(play);
    }
    Pacci_SetUltrahandArmed(0);
    Pacci_DropUltrahand();
    sIsInHand = false;
    sIsReachPoseHeld = false;
}

// The arm stays out toward what the hand would take, its pitch following the aim; the legs are left alone.
static int32_t HoldReachPose(Player* player, PlayState* play) {
    if (!sIsInHand) {
        sIsReachPoseHeld = false;
        return 0;
    }
    if (!sIsReachPoseHeld) {
        LinkAnimation_PlayOnceSetSpeed(play, &player->upperSkelAnime, (void*)gPlayerAnim_link_boom_throw_waitR, 0.0f);
        sIsReachPoseHeld = true;
    }
    player->upperSkelAnime.curFrame = 0.0f;
    LinkAnimation_Update(play, &player->upperSkelAnime);
    player->upperLimbRot.x = CLAMP(player->actor.focus.rot.x, -UH_REACH_PITCH, UH_REACH_PITCH);
    player->upperLimbRot.y = 0;
    player->upperLimbRot.z = 0;
    return 1;
}

static void SyncModeInput(void) {
    bool isModeActive = Pacci_UltrahandModeActive();

    if (isModeActive == sIsModeInputBlocked) {
        return;
    }
    sIsModeInputBlocked = isModeActive;
    if (isModeActive) {
        sApi->BlockPlayerInput(UH_KEY, UH_MODE_BUTTONS, false);
    } else {
        sApi->ReleasePlayerInput(UH_KEY);
    }
}

static bool WasButtonPressed(PlayState* play) {
    u16 button = FindEquippedButtonMask();
    bool isHeld = (play->state.input[0].cur.button & button) != 0;

    if (sIsWaitingForRelease) {
        sIsWaitingForRelease = isHeld;
        return false;
    }
    return (play->state.input[0].press.button & button) != 0;
}

static bool IsLinkBusy(Player* player) {
    return (player->stateFlags1 &
            (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING | PLAYER_STATE1_IN_ITEM_CS |
             PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_DAMAGED |
             PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_CLIMBING_LADDER |
             PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_HOOKSHOT_FALLING)) != 0;
}

static void UpdateHandInUse(PlayState* play, Player* player) {
    if (Pacci_UltrahandModeUpdate(play, player) || IsLinkBusy(player)) {
        return;
    }
    Pacci_SetUltrahandArmed(1);
    Pacci_UpdateUltrahand(play, player);
    Pacci_HighlightUltrahandTarget(play);
    if (Pacci_IsHoldingUltrahand()) {
        Pacci_FuseUpdatePreview(play, player);
        Pacci_PlaceUpdatePreview(play);
    }
    if (WasButtonPressed(play)) {
        Pacci_UltrahandModeEnter(play, player);
    }
}

// The hand first, then the ticks that outlive it: a fall, a pressed switch, a rider or a throw keeps going
// with the hand put away.
static void UpdateUltrahandWorld(void) {
    PlayState* play = gPlayState;
    Player* player = play == NULL ? NULL : GET_PLAYER(play);

    if (player == NULL) {
        return;
    }
    if (sIsInHand) {
        UpdateHandInUse(play, player);
    }
    Pacci_UltrahandDropTick(play);
    Pacci_FuseFollow(play);
    Pacci_PlacePressTick(play);
    Pacci_BackRiderTick(play);
    Pacci_CutTick(play);
    Pacci_ThrowTick(play);
    Pacci_UhAbortTick(play);
    SyncModeInput();
}

static void DrawUltrahandEffects(Actor* actor, PlayState* play) {
    if (sIsInHand) {
        Pacci_UltrahandDrawVfx(play, (Player*)actor);
    }
    Pacci_FuseDrawPreview(play);
}

// A heavy block is thrown by its own state machine, which freezes Link as if he were holding it overhead.
static void KeepLinkFreeDuringThrow(bool* should, va_list args) {
    if (Pacci_IsThrowing()) {
        *should = false;
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    Matrix_Scale(UH_GIVE_SCALE, UH_GIVE_SCALE, UH_GIVE_SCALE, MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGiveDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sGiveXluDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, GetActorId) || !SOH_MOD_API_HAS(sApi, SetCustomItemText)) {
        return;
    }
    if (SOH_MOD_API_HAS(sApi, RegisterRandoOption)) {
        sApi->RegisterRandoOption(&sPiecesOption);
    }

    SOHCustomItemDefinition hand = Z64Items_Define(UH_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&hand, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    // Shares the Sheikah Slate's cell, after it.
    Z64Items_SetPlacement(&hand, 0, 15, -1);
    Z64Items_SetCanUse(&hand, CanUseUltrahand);
    Z64Items_SetAction(&hand, TakeOutUltrahand, HoldReachPose);
    Z64Items_SetHeldCallbacks(&hand, PressUltrahand, PutUltrahandAway, NULL);
    Z64Items_SetLogic(&hand, &sUltrahandLogic);
    hand.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    hand.onReceive = ReceivePiece;
    hand.getItemEntry.drawFunc = DrawGetItem;
    RefreshTexts();
    Z64Items_SetTextbox(&hand, sGetItemText);
    Z64Items_SetPauseText(&hand, sPauseText);

    if (!Z64Items_Register(sApi, &hand)) {
        return;
    }
    sApi->RegisterVB(VB_FREEZE_LINK_FOR_BLOCK_THROW, KeepLinkFreeDuringThrow);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateUltrahandWorld);
    SOH_REGISTER_HOOK(sApi, OnLoadFile, LoadPieces);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawUltrahandEffects);
}
