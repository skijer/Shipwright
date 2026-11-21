#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include <math.h>
#include <string.h>
#include "soh/ActorDB.h"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h"
#include "objects/object_horse/object_horse.h"
#include "objects/gameplay_keep/gameplay_keep.h"

extern u8 ResourceMgr_FileExists(const char* resName);
extern Gfx* ResourceMgr_LoadGfxByName(const char* path);
void EnHorse_Update(Actor* thisx, PlayState* play);
void EnHorse_Draw(Actor* thisx, PlayState* play);

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
        { ELEMTYPE_UNK2,
          { cfg->dmgFlags, cfg->effect, cfg->damage },
          { 0, 0, 0 },
          TOUCH_ON | TOUCH_SFX_NORMAL,
          BUMP_NONE,
          OCELEM_NONE },
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

inline u8 Combat_CheckHit(ColliderCylinder* col) {
    return (col->base.atFlags & AT_HIT) ? 1 : 0;
}

static s32 MasterCycle_Cast(PlayState* play, Player* player);
static void MasterCycle_Tick(PlayState* play, Player* player);
static s16 MasterCycle_RideYaw(Actor* ride);
static void MasterCycle_Forget(void);
static u8 MasterCycle_IsRiding(void);
static u8 MasterCycle_IsActor(Actor* actor);

#define MC_DL_BODY "__OTR__objects/object_master_cycle/gMasterCycleBodyDL"
#define MC_DL_LIGHTS "__OTR__objects/object_master_cycle/gMasterCycleLigthsDL"
#define MC_DL_WHEEL_F "__OTR__objects/object_master_cycle/gMasterCycleFrontWhellDL"
#define MC_DL_WHEEL_B "__OTR__objects/object_master_cycle/gMasterCycleBackWheelDL"
#define MC_HUB_F_Z 35.0f
#define MC_HUB_B_Z -39.0f
#define MC_HUB_Y 15.0f
#define MC_WHEEL_RADIUS 15.0f
#define MC_WHEELBASE (MC_HUB_F_Z - MC_HUB_B_Z)
#define MC_BODY_HALF_WIDTH 13.0f

#define MC_HULL_RADIUS 15.0f
#define MC_HULL_NOSE_Z 42.0f
#define MC_HULL_TAIL_Z -40.0f
#define MC_BODY_TOP 63.0f

// Player_Action_8084CC98 places Link at riderPos - 27 in Y, so the seat value is the seat height plus 27.
#define MC_SEAT_X 0.0f
#define MC_SEAT_Y 50.0f
#define MC_SEAT_Z -6.0f
#define MC_MODEL_SCALE 1.06f
#define MC_MODEL_Y_OFFSET -8.0f
#define MC_SHADOW_SCALE 0.248f

#define MKW_FPT 3.0f
#define MKW_BASE_SPEED 81.89f
#define MC_MAX_SPEED 36.0f
#define MC_UNIT_PER_MKW (MC_MAX_SPEED / MKW_BASE_SPEED)

#define MKW_ACC_A0 0.65056f
#define MKW_ACC_A1 0.29360f
#define MKW_ACC_A2 0.37680f
#define MKW_ACC_A3 0.04768f
#define MKW_ACC_T0 0.200f
#define MKW_ACC_T1 0.800f
#define MKW_ACC_T2 0.925f
#define MKW_DRIFT_ACC_A0 2.35f
#define MKW_DRIFT_ACC_A1 0.15f
#define MKW_DRIFT_ACC_T0 0.98f

#define MKW_COAST_FWD 0.98f
#define MKW_COAST_REV 0.95f
#define MKW_BRAKE_ACCEL 3.0f
#define MKW_REVERSE_LIMIT 20.0f
#define MKW_BASE_HANDLING 0.9933f
#define MKW_MANUAL_HANDLING 0.0232f
#define MKW_MANUAL_DRIFT 0.0164f
#define MKW_HANDLING_RESP 0.9025f
#define MKW_DRIFT_RESP 0.98f
#define MKW_TURN_SPEED_LO 20.0f
#define MKW_TURN_FRAC_LO 0.40f
#define MKW_TURN_SPEED_HI 70.0f
#define MKW_TURN_FRAC_HI 0.50f
#define MKW_DRIFT_TURN_STICK 0.4f
#define MKW_DRIFT_TURN_DIR 0.6f
#define MKW_WHEELIE_TURN 0.35f
#define MKW_HOP_TURN 1.4f

#define MKW_MT_BASE 2
#define MKW_MT_INSIDE 3
#define MKW_MT_STICK 0.4f
#define MKW_MT_MAX 270
#define MKW_MT_DURATION 37
#define MKW_MT_SPEED_BONUS 0.25f
#define MKW_BOOST_ACCEL 3.0f

#define MC_MT_BONUS 0.08f
#define MC_BOOST_ACCEL 0.9f

#define MKW_WHEELIE_SPEED_BONUS 0.15f
#define MKW_WHEELIE_COOLDOWN 20
#define MKW_WHEELIE_CANCEL_STICK 0.85f
#define MKW_WHEELIE_CANCEL_FRAMES 15
#define MKW_WHEELIE_MIN_RATIO 0.3f
#define MKW_WHEELIE_MAX_FRAMES 180

#define MC_STOPPED 0.35f
#define MC_REVERSE_STICK 0.30f
#define MC_GRAVITY -3.5f
#define MC_MIN_VEL_Y -20.0f
#define MC_STEER_MAX_LOW 0x1C00
#define MC_STEER_MAX_HIGH 0x0500
#define MC_STEER_RATE 0x260
#define MC_STEER_RETURN 0x1A0
#define MC_SLOPE_ACCEL 0.55f
#define MC_LEAN_MAX 0x1400
#define MC_DRIFT_LEAN 0x1900
#define MC_WALL_PROBE_HEIGHT 22.0f
#define MC_WALL_BOUNCE 0.40f
#define MC_WALL_MIN_SFX 12.0f
#define MC_TERRAIN_SLOPE_LIMIT 0.55f

#define MC_SUS_STIFFNESS 0.45f
#define MC_SUS_STIFFNESS_F 0.52f
#define MC_SUS_DAMPING 0.95f
#define MC_SUS_TRAVEL 25.0f

#define MC_MT_FLAME_COUNT 2
#define MC_MT_FLAME_SPREAD 6.0f
#define MC_MT_PIPE_Y (MC_HUB_Y + MC_WHEEL_RADIUS + 2.0f)
#define MC_MT_PIPE_Z (MC_HUB_B_Z - 2.0f)
#define MC_TRAIL_MIN_RATIO 0.35f
#define MC_TRAIL_LIFE 8
#define MC_TRAIL_HALF_Y 5.0f
#define MC_TRAIL_SIDE 6.0f
#define MC_TRAIL_ON_TICKS 7.0f
#define MC_TRAIL_OFF_TICKS 6.0f

#define MC_MT_FLAME_SCALE 0.0008f
#define MC_MT_STAGE1 0.33f
#define MC_MT_STAGE2 0.70f
#define MC_MT_CHARGE_SCALE 0.5f
#define MC_DRIFT_MIN_RATIO 0.30f

#define MC_DRIFT_ANGLE 0x0A00
#define MC_DRIFT_ANGLE_RATE 0x0120
#define MC_DRIFT_ANGLE_DECR 0x01C0
#define MC_RIDER_DRIFT_SHARE 0.35f
#define MC_DRIFT_TURN_SCALE 0.55f
#define MC_HOP_VEL 10.0f

#define MC_WHEELIE_PITCH 0x1C00
#define MC_WHEELIE_PITCH_STEP 0x300

#define MC_HIT_MIN_SPEED 10.0f
#define MC_HIT_DAMAGE_LOW 1
#define MC_HIT_DAMAGE_HIGH 4

#define MC_MATERIALIZE_TICKS 16
#define MC_DISMISS_TICKS 12
#define MC_SUMMON_AHEAD 90.0f
#define MC_MOUNT_RANGE 55.0f
#define MC_MOUNT_MAX_DY 30.0f
#define MC_DEEP_WATER 28.0f

#define MC_ACTION_PARKED ENHORSE_ACT_IDLE
#define MC_ACTION_RIDDEN ENHORSE_ACT_MOUNTED_WALK

typedef enum {
    MC_PHASE_NONE = 0,
    MC_PHASE_MATERIALIZE,
    MC_PHASE_ACTIVE,
    MC_PHASE_DISMISS,
} McPhase;

typedef enum {
    MC_DRIFT_NONE = 0,
    MC_DRIFT_HOP,
    MC_DRIFT_ON,
} McDrift;

typedef struct {
    Actor* actor;
    u8 phase;
    s16 age;
    s16 phaseTimer;

    f32 speed;
    s16 steer;
    s16 lean;
    s16 pitch;
    s16 terrainPitch;
    s16 floorPitch;
    f32 wheelSpin;
    s16 lastYaw;
    Vec3f prevPos;

    u8 drift;
    s8 driftDir;
    s16 mtCharge;
    s16 boostTimer;

    s32 trailIdx[2];
    u8 trailOn[2];
    u8 trailWheelie;
    s16 trailTimer[2];

    u8 wheelie;
    s16 wheelieTimer;
    s16 wheelieCooldown;
    s16 wheelieStickTimer;
    s16 wheeliePitch;
    f32 turnSmooth;

    f32 susY[2];
    f32 susVel[2];
    u8 susReady;
    f32 wheelY[2];

    u8 launched;

    u8 wasRiding;
    u8 airborne;
    s16 hopTimer;

    s16 driftAngle;

    ColliderCylinder hitCol;
    u8 hitColReady;

    s32 mountSide;
    u8 riderTilted;
} MasterCycleState;

static MasterCycleState sMc = { 0 };
static Gfx* sMcDlBody = NULL;
static Gfx* sMcDlLights = NULL;
static Gfx* sMcDlWheelF = NULL;
static Gfx* sMcDlWheelB = NULL;
static u8 sMcDlTried = 0;

static void MasterCycle_BodyPoint(Actor* actor, f32 lx, f32 ly, f32 lz, Vec3f* out);
static void MasterCycle_TrailsInit(PlayState* play);
static void MasterCycle_TrailsFree(PlayState* play);
static void MasterCycle_Update(Actor* thisx, PlayState* play);
static void MasterCycle_Draw(Actor* thisx, PlayState* play);

static void MasterCycle_LoadDLs(void) {
    if (sMcDlTried && sMcDlBody != NULL) {
        return;
    }
    sMcDlTried = 1;
    // ResourceMgr_LoadGfxByName on a missing path crashes, so each load is gated on existence.
    if (ResourceMgr_FileExists(MC_DL_BODY)) {
        sMcDlBody = ResourceMgr_LoadGfxByName(MC_DL_BODY);
    }
    if (ResourceMgr_FileExists(MC_DL_LIGHTS)) {
        sMcDlLights = ResourceMgr_LoadGfxByName(MC_DL_LIGHTS);
    }
    if (ResourceMgr_FileExists(MC_DL_WHEEL_F)) {
        sMcDlWheelF = ResourceMgr_LoadGfxByName(MC_DL_WHEEL_F);
    }
    if (ResourceMgr_FileExists(MC_DL_WHEEL_B)) {
        sMcDlWheelB = ResourceMgr_LoadGfxByName(MC_DL_WHEEL_B);
    }
}

static u8 MasterCycle_IsActor(Actor* actor) {
    return (actor != NULL) && (actor == sMc.actor);
}

static u8 MasterCycle_IsRiding(void) {
    Player* player;

    if ((sMc.actor == NULL) || (gPlayState == NULL)) {
        return 0;
    }
    player = GET_PLAYER(gPlayState);
    return (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) && (player->rideActor == sMc.actor);
}

static void MasterCycle_Forget(void) {
    // Scene unloading can invalidate the resource manager's display-list storage.
    sMcDlBody = sMcDlLights = sMcDlWheelF = sMcDlWheelB = NULL;
    sMcDlTried = 0;
    memset(&sMc, 0, sizeof(sMc));
    sMc.actor = NULL;
    sMc.phase = MC_PHASE_NONE;
    sMc.mountSide = 0;
}

static void MasterCycle_Sfx(u16 id) {
    if (sMc.actor != NULL) {
        Audio_PlaySfxGeneral(id, &sMc.actor->projectedPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultReverb);
    }
}

static void MasterCycle_TrailsInit(PlayState* play) {
    static const u8 sP1Start[] = { 190, 235, 255, 190 };
    static const u8 sP2Start[] = { 120, 200, 255, 120 };
    static const u8 sP1End[] = { 120, 190, 255, 60 };
    static const u8 sP2End[] = { 90, 150, 255, 0 };
    EffectBlureInit1 init;
    s32 i;

    for (i = 0; i < 4; i++) {
        init.p1StartColor[i] = sP1Start[i];
        init.p2StartColor[i] = sP2Start[i];
        init.p1EndColor[i] = sP1End[i];
        init.p2EndColor[i] = sP2End[i];
    }
    init.elemDuration = MC_TRAIL_LIFE;
    init.unkFlag = false;
    init.calcMode = 2;
    init.trailType = TRAIL_TYPE_REST;

    for (i = 0; i < 2; i++) {
        sMc.trailIdx[i] = TOTAL_EFFECT_COUNT;
        Effect_Add(play, &sMc.trailIdx[i], EFFECT_BLURE1, 0, 0, &init);
    }
}

static void MasterCycle_TrailsFree(PlayState* play) {
    s32 i;

    for (i = 0; i < 2; i++) {
        if (sMc.trailIdx[i] != TOTAL_EFFECT_COUNT) {
            Effect_Delete(play, sMc.trailIdx[i]);
            sMc.trailIdx[i] = TOTAL_EFFECT_COUNT;
        }
    }
}

static void MasterCycle_Trails(PlayState* play, Actor* actor) {
    static const f32 sHubZ[2] = { MC_HUB_F_Z, MC_HUB_B_Z };
    f32 ratio = fabsf(sMc.speed) / MC_MAX_SPEED;
    s32 i;

    (void)play;

    for (i = 0; i < 2; i++) {
        EffectBlure* blure;
        Vec3f hub;
        Vec3f top;
        Vec3f bottom;
        f32 lx;
        f32 lz;

        if (sMc.trailIdx[i] == TOTAL_EFFECT_COUNT) {
            continue;
        }
        blure = Effect_GetByIndex(sMc.trailIdx[i]);
        if (blure == NULL) {
            continue;
        }

        if (--sMc.trailTimer[i] <= 0) {
            sMc.trailOn[i] = !sMc.trailOn[i];
            sMc.trailTimer[i] =
                (s16)(2.0f + (Rand_ZeroOne() * (sMc.trailOn[i] ? MC_TRAIL_ON_TICKS : MC_TRAIL_OFF_TICKS)));
        }

        // Changing layout teleports the ribbon's mouth, and a blure would join the two points with a quad.
        if (sMc.trailWheelie != (sMc.wheelie ? 1 : 0)) {
            EffectBlure_AddSpace(blure);
            if (i == 1) {
                sMc.trailWheelie = sMc.wheelie ? 1 : 0;
            }
            continue;
        }

        if (sMc.wheelie) {
            lx = (i == 0) ? MC_TRAIL_SIDE : -MC_TRAIL_SIDE;
            lz = MC_HUB_B_Z;
        } else {
            lx = 0.0f;
            lz = sHubZ[i];
            if (!sMc.trailOn[i]) {
                EffectBlure_AddSpace(blure);
                continue;
            }
        }

        if ((ratio < MC_TRAIL_MIN_RATIO) || (sMc.phase != MC_PHASE_ACTIVE)) {
            EffectBlure_AddSpace(blure);
            continue;
        }

        MasterCycle_BodyPoint(actor, lx, MC_HUB_Y, lz, &hub);
        hub.x += actor->world.pos.x;
        hub.y += actor->world.pos.y + MC_MODEL_Y_OFFSET;
        hub.z += actor->world.pos.z;

        bottom.x = top.x = hub.x;
        bottom.z = top.z = hub.z;
        top.y = hub.y + MC_TRAIL_HALF_Y;
        bottom.y = hub.y - MC_TRAIL_HALF_Y;

        EffectBlure_AddVertex(blure, &top, &bottom);
    }
}

// riderPos is relative: Player_Action_8084CC98 places Link at rideActor->world.pos + riderPos.
static void MasterCycle_BodyPoint(Actor* actor, f32 lx, f32 ly, f32 lz, Vec3f* out) {
    f32 x;
    f32 y;
    f32 z;
    f32 t;
    f32 sn;
    f32 cs;

    y = ly - MC_HUB_Y;
    z = lz - MC_HUB_B_Z;
    sn = Math_SinS(actor->shape.rot.x);
    cs = Math_CosS(actor->shape.rot.x);
    t = (y * cs) - (z * sn);
    z = (y * sn) + (z * cs);
    y = t + MC_HUB_Y;
    z += MC_HUB_B_Z;
    x = lx;

    sn = Math_SinS(actor->shape.rot.z);
    cs = Math_CosS(actor->shape.rot.z);
    t = (x * cs) - (y * sn);
    y = (x * sn) + (y * cs);
    x = t;

    sn = Math_SinS(actor->shape.rot.y);
    cs = Math_CosS(actor->shape.rot.y);
    out->x = (x * cs) + (z * sn);
    out->y = y;
    out->z = (-x * sn) + (z * cs);
}

static void MasterCycle_SeatPos(Actor* actor, Vec3f* out) {
    MasterCycle_BodyPoint(actor, MC_SEAT_X, MC_SEAT_Y, MC_SEAT_Z, out);
}

// Published for companions: places a point given relative to Link (x left, y up, z forward) on the cycle, measured from
// where Link actually sits so it follows the lean and the wheelie exactly as he does.
static bool MasterCycle_PlaceBesideRider(Actor* ride, Vec3f* local, Vec3f* pos, Vec3s* rot) {
    Vec3f seat;
    Vec3f point;

    if ((sMc.actor == NULL) || (ride != sMc.actor) || (gPlayState == NULL) || (local == NULL) || (pos == NULL) ||
        (rot == NULL)) {
        return false;
    }
    MasterCycle_SeatPos(ride, &seat);
    MasterCycle_BodyPoint(ride, MC_SEAT_X + local->x, MC_SEAT_Y + local->y, MC_SEAT_Z + local->z, &point);
    *pos = GET_PLAYER(gPlayState)->actor.world.pos;
    pos->x += point.x - seat.x;
    pos->y += point.y - seat.y;
    pos->z += point.z - seat.z;
    *rot = ride->shape.rot;
    return true;
}

static s16 MasterCycle_RideYaw(Actor* ride) {
    if ((sMc.actor == NULL) || (ride != sMc.actor)) {
        return ride->shape.rot.y;
    }
    return (s16)(ride->world.rot.y + (s16)(sMc.driftAngle * MC_RIDER_DRIFT_SHARE));
}

static Actor* MasterCycle_Spawn(PlayState* play, Player* player, u8 underLink) {
    ActorDBEntry* db = ActorDB_Retrieve(ACTOR_EN_HORSE);
    s32 savedObjectId = 0;
    u8 swappedObject = 0;
    Actor* actor;
    EnHorse* horse;
    f32 s;
    f32 c;
    Vec3f pos;
    s16 yaw = player->actor.shape.rot.y;

    // OBJECT_HORSE is only loaded in Epona's scenes and Actor_Spawn refuses an actor without its object,
    // so the entry is pointed at gameplay_keep for the length of the spawn.
    if ((db != NULL) && (Object_GetIndex(&play->objectCtx, OBJECT_HORSE) < 0)) {
        savedObjectId = db->objectId;
        db->objectId = OBJECT_GAMEPLAY_KEEP;
        swappedObject = 1;
    }

    s = Math_SinS(yaw);
    c = Math_CosS(yaw);
    pos.x = player->actor.world.pos.x + (underLink ? 0.0f : (s * MC_SUMMON_AHEAD));
    pos.y = player->actor.world.pos.y;
    pos.z = player->actor.world.pos.z + (underLink ? 0.0f : (c * MC_SUMMON_AHEAD));

    actor = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_HORSE, pos.x, pos.y, pos.z, 0, yaw, 0, 0);

    if (swappedObject) {
        db->objectId = savedObjectId;
    }
    if (actor == NULL) {
        return NULL;
    }
    // EnHorse_Init may have killed itself, which leaves update NULL.
    if (actor->update == NULL) {
        return NULL;
    }

    horse = (EnHorse*)actor;
    actor->update = MasterCycle_Update;
    actor->draw = MasterCycle_Draw;
    actor->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    actor->room = -1;
    actor->gravity = MC_GRAVITY;
    actor->minVelocityY = MC_MIN_VEL_Y;
    actor->speedXZ = 0.0f;
    actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;
    // The shadow is drawn at actor->scale * shadowScale, and the bike is scale 1 where Epona is 0.01.
    ActorShape_Init(&actor->shape, 0.0f, ActorShadow_DrawHorse, MC_SHADOW_SCALE);
    actor->shape.rot.x = actor->shape.rot.z = 0;
    actor->world.rot = actor->shape.rot;

    horse->action = MC_ACTION_PARKED;
    horse->stateFlags = 0;
    horse->animationIdx = ENHORSE_ANIM_IDLE;
    horse->curFrame = 0.0f;
    horse->playerControlled = false;
    Animation_PlayLoop(&horse->skin.skelAnime, (AnimationHeader*)gEponaIdleAnim);

    memset(&sMc, 0, sizeof(sMc));
    sMc.actor = actor;
    sMc.prevPos = actor->world.pos;
    sMc.lastYaw = actor->shape.rot.y;
    if (underLink) {
        sMc.phase = MC_PHASE_ACTIVE;
        Actor_SetScale(actor, MC_MODEL_SCALE);
    } else {
        sMc.phase = MC_PHASE_MATERIALIZE;
        sMc.phaseTimer = MC_MATERIALIZE_TICKS;
        Actor_SetScale(actor, 0.001f);
    }
    MasterCycle_SeatPos(actor, &horse->riderPos);

    MasterCycle_TrailsInit(play);
    MasterCycle_LoadDLs();
    return actor;
}

static void MasterCycle_ForceDismount(void) {
    EnHorse* horse = (EnHorse*)sMc.actor;

    if (horse != NULL) {
        horse->stateFlags |= ENHORSE_FLAG_6;
    }
}

static void MasterCycle_BeginDismiss(void) {
    if (sMc.actor == NULL) {
        return;
    }
    sMc.phase = MC_PHASE_DISMISS;
    sMc.phaseTimer = MC_DISMISS_TICKS;
    MasterCycle_Sfx(NA_SE_EV_TRE_BOX_APPEAR);
}

static s32 MasterCycle_Cast(PlayState* play, Player* player) {
    if ((sMc.actor != NULL) && (sMc.actor->update == NULL)) {
        MasterCycle_Forget();
    }
    if (sMc.actor != NULL) {
        if (sMc.phase == MC_PHASE_DISMISS) {
            return 0;
        }
        if (MasterCycle_IsRiding()) {
            MasterCycle_ForceDismount();
            sMc.wasRiding = 2;
            return 1;
        }
        MasterCycle_BeginDismiss();
        return 1;
    }
    if (MasterCycle_Spawn(play, player, 0) == NULL) {
        return 0;
    }
    MasterCycle_Sfx(NA_SE_EV_TRE_BOX_APPEAR);
    return 1;
}

static u8 sMcCarry = 0;
static f32 sMcCarrySpeed = 0.0f;
static u8 sMcSceneChanged = 1;

// The cycle has no carrot boosts, but the HUD draws Epona's for any ridden horse.
static void MasterCycle_HideCarrots(bool* should, va_list args) {
    if (MasterCycle_IsRiding()) {
        *should = false;
    }
}

// Riding drops the HUD to A/B/minimap; vanilla only raises it again when a button status flips on the dismount.
static void MasterCycle_RestoreHud(void) {
    if (gSaveContext.hudVisibilityMode != HUD_VISIBILITY_A_B_MINIMAP &&
        gSaveContext.hudVisibilityMode != HUD_VISIBILITY_A_HEARTS_MAGIC_MINIMAP_FORCE) {
        return;
    }
    gSaveContext.hudVisibilityMode = HUD_VISIBILITY_NO_CHANGE;
    Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL);
}

static void MasterCycle_Tick(PlayState* play, Player* player) {
    static s16 sLastScene = -1;

    if (sMcSceneChanged || sLastScene != play->sceneNum) {
        sMcSceneChanged = 0;
        sLastScene = play->sceneNum;
        MasterCycle_Forget();

        if (sMcCarry) {
            Actor* bike;

            sMcCarry = 0;
            bike = MasterCycle_Spawn(play, player, 1);
            if (bike != NULL) {
                Actor_MountHorse(play, player, bike);
                Actor_RequestHorseCameraSetting(play, player);
                sMc.speed = sMcCarrySpeed;
                sMc.lastYaw = bike->shape.rot.y;
            }
        }
        return;
    }
    if (!MasterCycle_IsRiding()) {
        sMcCarry = 0;
    }

    if (sMc.actor == NULL) {
        return;
    }
    if (sMc.actor->update == NULL) {
        MasterCycle_Forget();
        return;
    }

    // AREG(6) makes Horse_SetupInGameplay spawn Epona under Link in the next scene, so it is cleared every frame of the
    // fade.
    if ((play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF)) {
        if (MasterCycle_IsRiding()) {
            AREG(6) = 0;
            if (gSaveContext.respawnFlag != -2) {
                sMcCarry = 1;
                sMcCarrySpeed = sMc.speed * 0.6f;
            }
        }
    }

    if ((sMc.wasRiding == 2) && !MasterCycle_IsRiding() && !(player->stateFlags1 & PLAYER_STATE1_ON_HORSE)) {
        sMc.wasRiding = 0;
        MasterCycle_RestoreHud();
        MasterCycle_BeginDismiss();
    }
}

static f32 MasterCycle_TopSpeed(void) {
    f32 bonus = 0.0f;

    if (sMc.boostTimer > 0) {
        bonus = MC_MT_BONUS;
    } else if (sMc.wheelie) {
        bonus = MKW_WHEELIE_SPEED_BONUS;
    }
    return MC_MAX_SPEED * (1.0f + bonus);
}

static f32 MasterCycle_MkwAccel(f32 ratio, u8 drifting) {
    if (ratio < 0.0f) {
        return 1.0f;
    }
    if (drifting) {
        if (ratio < MKW_DRIFT_ACC_T0) {
            return MKW_DRIFT_ACC_A0 + ((MKW_DRIFT_ACC_A1 - MKW_DRIFT_ACC_A0) * (ratio / MKW_DRIFT_ACC_T0));
        }
        return MKW_DRIFT_ACC_A1;
    }
    if (ratio < MKW_ACC_T0) {
        return MKW_ACC_A0 + ((MKW_ACC_A1 - MKW_ACC_A0) * (ratio / MKW_ACC_T0));
    }
    if (ratio < MKW_ACC_T1) {
        return MKW_ACC_A1 + ((MKW_ACC_A2 - MKW_ACC_A1) * ((ratio - MKW_ACC_T0) / (MKW_ACC_T1 - MKW_ACC_T0)));
    }
    if (ratio < MKW_ACC_T2) {
        return MKW_ACC_A2 + ((MKW_ACC_A3 - MKW_ACC_A2) * ((ratio - MKW_ACC_T1) / (MKW_ACC_T2 - MKW_ACC_T1)));
    }
    return MKW_ACC_A3;
}

static f32 MasterCycle_PerTick(f32 perFrame) {
    return perFrame * perFrame * perFrame;
}

static void MasterCycle_ProbeTerrain(PlayState* play, Actor* actor) {
    Vec3f pf;
    Vec3f pb;
    CollisionPoly* poly;
    s32 bgId;
    f32 yf;
    f32 yb;
    f32 s = Math_SinS(actor->shape.rot.y);
    f32 c = Math_CosS(actor->shape.rot.y);

    pf.x = actor->world.pos.x + (s * MC_HUB_F_Z);
    pf.y = actor->world.pos.y + 40.0f;
    pf.z = actor->world.pos.z + (c * MC_HUB_F_Z);
    pb.x = actor->world.pos.x + (s * MC_HUB_B_Z);
    pb.y = actor->world.pos.y + 40.0f;
    pb.z = actor->world.pos.z + (c * MC_HUB_B_Z);

    yf = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &pf);
    yb = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &pb);
    sMc.wheelY[0] = yf;
    sMc.wheelY[1] = yb;
    if ((yf <= BGCHECK_Y_MIN) || (yb <= BGCHECK_Y_MIN)) {
        Math_SmoothStepToS(&sMc.terrainPitch, 0, 4, 0x200, 0x10);
        return;
    }
    if (sMc.airborne) {
        f32 sp = fabsf(actor->speedXZ);
        s16 want = (sp > 1.0f) ? (s16)(Math_FAtan2F(actor->velocity.y, sp) * (0x8000 / M_PI)) : 0;

        if (want > 0x2000) {
            want = 0x2000;
        }
        if (want < -0x2800) {
            want = -0x2800;
        }
        Math_SmoothStepToS(&sMc.terrainPitch, want, 4, 0x300, 0x10);
        return;
    }
    // shape.rot.x positive tips the nose down, so the terrain pitch is negated when applied.
    {
        f32 dy = yf - yb;

        if (dy > (MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT)) {
            dy = MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT;
        }
        if (dy < -(MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT)) {
            dy = -(MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT);
        }
        (void)dy;
    }
}

static void MasterCycle_ResolveHull(PlayState* play, Actor* actor) {
    static const f32 sHullZ[2] = { MC_HULL_NOSE_Z, MC_HULL_TAIL_Z };
    s32 i;

    for (i = 0; i < 2; i++) {
        Vec3f a;
        Vec3f b;
        Vec3f hit;
        CollisionPoly* poly = NULL;
        s32 bgId = BGCHECK_SCENE;
        f32 sn = Math_SinS(actor->shape.rot.y);
        f32 cs = Math_CosS(actor->shape.rot.y);
        f32 reach = sHullZ[i] + ((sHullZ[i] > 0.0f) ? MC_HULL_RADIUS : -MC_HULL_RADIUS);

        a.x = actor->world.pos.x;
        a.y = actor->world.pos.y + MC_WALL_PROBE_HEIGHT;
        a.z = actor->world.pos.z;
        b.x = a.x + (sn * reach);
        b.y = a.y;
        b.z = a.z + (cs * reach);

        if (!BgCheck_EntityLineTest1(&play->colCtx, &a, &b, &hit, &poly, true, false, false, true, &bgId)) {
            continue;
        }
        if (poly == NULL) {
            continue;
        }
        {
            f32 nx = COLPOLY_GET_NORMAL(poly->normal.x);
            f32 nz = COLPOLY_GET_NORMAL(poly->normal.z);
            f32 over = ((b.x - hit.x) * nx) + ((b.z - hit.z) * nz);

            if (over > 0.0f) {
                actor->world.pos.x += nx * over;
                actor->world.pos.z += nz * over;
                actor->wallYaw = Math_Atan2S(nz, nx);
                actor->bgCheckFlags |= BGCHECKFLAG_WALL;
            }
        }
    }
}

static void MasterCycle_Move(PlayState* play, Actor* actor) {
    f32 dist;
    s32 substeps;
    s32 i;
    u8 hitWall = 0;

    // Ground speed is split into cos(pitch) along the floor and sin(pitch) up, so a ramp launches; otherwise
    // the ground check flattens velocity.y every frame.
    if (!sMc.airborne && (actor->floorPoly != NULL)) {
        f32 nx = COLPOLY_GET_NORMAL(actor->floorPoly->normal.x);
        f32 ny = COLPOLY_GET_NORMAL(actor->floorPoly->normal.y);
        f32 nz = COLPOLY_GET_NORMAL(actor->floorPoly->normal.z);
        f32 sn = Math_SinS(actor->world.rot.y);
        f32 cs = Math_CosS(actor->world.rot.y);

        if (ny > 0.05f) {
            s16 want = Math_Atan2S(1.0f, (-(nx * sn) - (nz * cs)) / ny);

            if (sMc.speed < 0.0f) {
                want = -want;
            }
            Math_SmoothStepToS(&sMc.floorPitch, want, 2, 0x600, 0x40);
        }
    } else {
        Math_SmoothStepToS(&sMc.floorPitch, 0, 3, 0x300, 0x40);
    }

    if (!sMc.airborne && !sMc.launched) {
        sMc.speed -= Math_SinS(sMc.floorPitch) * fabsf(MC_GRAVITY) * MC_SLOPE_ACCEL;
    }

    // func_8002E234 glues an actor to the floor across any gap under 11 units unless it carries this flag.
    actor->bgCheckFlags |= 0x800;

    actor->speedXZ = sMc.speed * Math_CosS(sMc.floorPitch);
    // Skipped while something is deliberately throwing the bike upward, or the launch velocity is overwritten.
    if (!sMc.airborne && !sMc.launched && (actor->velocity.y <= 0.0f)) {
        actor->velocity.y = fabsf(sMc.speed) * Math_SinS(sMc.floorPitch);
    }
    Actor_UpdateVelocityXZGravity(actor);

    dist = sqrtf((actor->velocity.x * actor->velocity.x) + (actor->velocity.y * actor->velocity.y) +
                 (actor->velocity.z * actor->velocity.z));
    substeps = (s32)(dist / 10.0f) + 1;
    if (substeps > 8) {
        substeps = 8;
    }

    for (i = 0; i < substeps; i++) {
        actor->prevPos = actor->world.pos;
        actor->world.pos.x += actor->velocity.x / (f32)substeps;
        actor->world.pos.y += actor->velocity.y / (f32)substeps;
        actor->world.pos.z += actor->velocity.z / (f32)substeps;
        // 0x15 and not Epona's 0x1D: bit 8 makes func_8002E2AC clamp a grounded body's velocity.y
        // to -4 every tick, which would swallow the ramp launch.
        {
            // func_8002E2AC assigns world.pos.y = floorHeight outright; height belongs to MasterCycle_Suspension.
            f32 keepY = actor->world.pos.y;

            Actor_UpdateBgCheckInfo(play, actor, 30.0f, MC_HULL_RADIUS, MC_BODY_TOP, 0x15);
            if (actor->bgCheckFlags & BGCHECKFLAG_GROUND) {
                actor->world.pos.y = keepY;
            }
        }
        MasterCycle_ResolveHull(play, actor);
        if (actor->bgCheckFlags & BGCHECKFLAG_WALL) {
            hitWall = 1;
            break;
        }
    }

    if (hitWall) {
        f32 facing = Math_CosS(actor->wallYaw - actor->world.rot.y);

        if (facing < -0.3f) {
            if (fabsf(sMc.speed) > MC_WALL_MIN_SFX) {
                MasterCycle_Sfx(NA_SE_EV_BOMB_BOUND);
            }
            sMc.speed *= MC_WALL_BOUNCE;
            sMc.wheelie = 0;
            sMc.drift = MC_DRIFT_NONE;
            sMc.mtCharge = 0;
        }
    }

    sMc.airborne = !(actor->bgCheckFlags & BGCHECKFLAG_GROUND);
}

static void MasterCycle_Drive(PlayState* play, Actor* actor, Player* player) {
    Input* input = &play->state.input[0];
    u16 held = input->cur.button;
    u16 pressed = input->press.button;
    f32 stickX = (f32)input->rel.stick_x / 60.0f;
    f32 stickY = (f32)input->rel.stick_y / 60.0f;
    f32 stickMag;
    f32 top = MasterCycle_TopSpeed();
    f32 ratio;
    f32 accel;
    s16 turn = 0;
    s16 wantSteer = 0;

    if (stickX > 1.0f) {
        stickX = 1.0f;
    }
    if (stickX < -1.0f) {
        stickX = -1.0f;
    }
    if (stickY > 1.0f) {
        stickY = 1.0f;
    }
    if (stickY < -1.0f) {
        stickY = -1.0f;
    }
    stickMag = sqrtf((stickX * stickX) + (stickY * stickY));
    stickX = -stickX;

    if (Player_InCsMode(play) || (player->stateFlags1 & (PLAYER_STATE1_TALKING | PLAYER_STATE1_IN_CUTSCENE))) {
        held = 0;
        pressed = 0;
        stickX = 0.0f;
        stickMag = 0.0f;
    }

    ratio = sMc.speed / MC_MAX_SPEED;
    if (ratio > 1.0f + MC_MT_BONUS) {
        ratio = 1.0f + MC_MT_BONUS;
    }
    accel = MasterCycle_MkwAccel(ratio, sMc.drift == MC_DRIFT_ON) * MC_UNIT_PER_MKW * MKW_FPT;

    if (sMc.boostTimer > 0) {
        sMc.boostTimer--;
        sMc.speed += MC_BOOST_ACCEL * MC_UNIT_PER_MKW * MKW_FPT;
        if (sMc.speed > top) {
            sMc.speed = top;
        }
    } else if (held & BTN_A) {
        if (sMc.speed < 0.0f) {
            sMc.speed += MKW_BRAKE_ACCEL * MC_UNIT_PER_MKW * MKW_FPT;
        } else {
            sMc.speed += accel;
        }
    } else if (held & BTN_B) {
        if (sMc.speed > 0.0f) {
            sMc.speed -= MKW_BRAKE_ACCEL * MC_UNIT_PER_MKW * MKW_FPT;
            if (sMc.speed < 0.0f) {
                sMc.speed = 0.0f;
            }
        } else if (stickMag > MC_REVERSE_STICK) {
            if (!sMc.wheelie) {
                sMc.speed -= accel * 0.6f * stickMag;
            }
        } else if (fabsf(sMc.speed) < MC_STOPPED) {
            MasterCycle_ForceDismount();
        } else {
            sMc.speed *= MasterCycle_PerTick(MKW_COAST_REV);
        }
    } else {
        sMc.speed *= MasterCycle_PerTick((sMc.speed >= 0.0f) ? MKW_COAST_FWD : MKW_COAST_REV);
        if (fabsf(sMc.speed) < 0.05f) {
            sMc.speed = 0.0f;
        }
    }

    if ((sMc.drift != MC_DRIFT_ON) && !sMc.wheelie) {
        f32 r = fabsf(ratio);
        f32 scrub = MKW_BASE_HANDLING +
                    ((1.0f - MKW_BASE_HANDLING) * (1.0f - (fabsf(sMc.turnSmooth) * ((r > 1.0f) ? 1.0f : r))));

        sMc.speed *= MasterCycle_PerTick(scrub);
    }

    if (sMc.speed > top) {
        sMc.speed = (sMc.speed - top > 0.5f) ? (sMc.speed - 0.5f) : top;
    }
    if (sMc.speed < -(MKW_REVERSE_LIMIT * MC_UNIT_PER_MKW)) {
        sMc.speed = -(MKW_REVERSE_LIMIT * MC_UNIT_PER_MKW);
    }

    if (sMc.wheelieCooldown > 0) {
        sMc.wheelieCooldown--;
    }
    if (!sMc.wheelie) {
        if ((pressed & BTN_DUP) && (sMc.wheelieCooldown == 0) && (sMc.drift == MC_DRIFT_NONE) && !sMc.airborne &&
            (ratio >= MKW_WHEELIE_MIN_RATIO)) {
            sMc.wheelie = 1;
            sMc.wheelieTimer = 0;
            sMc.wheelieStickTimer = 0;
            sMc.wheelieCooldown = (s16)(MKW_WHEELIE_COOLDOWN / MKW_FPT);
            MasterCycle_Sfx(NA_SE_EV_HORSE_JUMP);
        }
    } else {
        u8 cancel = 0;

        sMc.wheelieTimer++;
        if (fabsf(stickX) > MKW_WHEELIE_CANCEL_STICK) {
            sMc.wheelieStickTimer++;
            if (sMc.wheelieStickTimer > (s16)(MKW_WHEELIE_CANCEL_FRAMES / MKW_FPT)) {
                cancel = 1;
            }
        } else {
            sMc.wheelieStickTimer = 0;
        }
        if ((pressed & BTN_DDOWN) || (ratio < MKW_WHEELIE_MIN_RATIO) || (sMc.drift != MC_DRIFT_NONE) ||
            (sMc.wheelieTimer > (s16)(MKW_WHEELIE_MAX_FRAMES / MKW_FPT))) {
            cancel = 1;
        }
        if (cancel) {
            sMc.wheelie = 0;
            sMc.wheelieCooldown = (s16)(MKW_WHEELIE_COOLDOWN / MKW_FPT);
            MasterCycle_Sfx(NA_SE_EV_HORSE_LAND);
        }
    }
    Math_SmoothStepToS(&sMc.wheeliePitch, sMc.wheelie ? MC_WHEELIE_PITCH : 0, 3, MC_WHEELIE_PITCH_STEP, 0x40);

    if (sMc.hopTimer > 0) {
        sMc.hopTimer--;
    }
    switch (sMc.drift) {
        case MC_DRIFT_NONE:
            if ((pressed & BTN_R) && !sMc.airborne && (fabsf(sMc.speed) > 0.5f)) {
                actor->velocity.y = MC_HOP_VEL;
                sMc.drift = MC_DRIFT_HOP;
                sMc.hopTimer = 6;
                sMc.wheelie = 0;
                MasterCycle_Sfx(NA_SE_EV_HORSE_JUMP);
                actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
                sMc.launched = 1;
            }
            break;
        case MC_DRIFT_HOP:
            if (!(held & BTN_R)) {
                sMc.drift = MC_DRIFT_NONE;
            } else if (!sMc.airborne && (sMc.hopTimer <= 3)) {
                sMc.drift = MC_DRIFT_ON;
                sMc.driftDir = (stickX > 0.05f) ? 1 : ((stickX < -0.05f) ? -1 : ((sMc.steer >= 0) ? 1 : -1));
                sMc.mtCharge = 0;
            }
            break;
        case MC_DRIFT_ON:
            if (!(held & BTN_R) || (ratio < MC_DRIFT_MIN_RATIO * 0.6f)) {
                if (sMc.mtCharge >= MKW_MT_MAX) {
                    sMc.boostTimer = (s16)(MKW_MT_DURATION / MKW_FPT);
                    MasterCycle_Sfx(NA_SE_IT_SWORD_SWING_HARD);
                }
                sMc.drift = MC_DRIFT_NONE;
                sMc.mtCharge = 0;
            } else {
                s32 into = (stickX * (f32)sMc.driftDir) > MKW_MT_STICK;
                s32 before = sMc.mtCharge;

                sMc.mtCharge += (s16)((MKW_MT_BASE + (into ? MKW_MT_INSIDE : 0)) * (s32)MKW_FPT * MC_MT_CHARGE_SCALE);
                if (sMc.mtCharge > MKW_MT_MAX) {
                    sMc.mtCharge = MKW_MT_MAX;
                }
                if (sMc.mtCharge < MKW_MT_MAX) {
                    // A continuous sound is played minus SFX_FLAG every tick; with the flag it never ends.
                    MasterCycle_Sfx(NA_SE_IT_SWORD_CHARGE - SFX_FLAG);
                } else if (before < MKW_MT_MAX) {
                    MasterCycle_Sfx(NA_SE_IT_HOOKSHOT_READY);
                }
            }
            break;
    }

    {
        f32 raw = stickX;
        f32 resp;
        f32 maxSteer;
        f32 r = fabsf(ratio);
        f32 tanDelta;
        f32 yawRad;
        s16 wantAngle;

        if (r > 1.0f) {
            r = 1.0f;
        }
        if (sMc.wheelie) {
            raw *= MKW_WHEELIE_TURN;
        }

        maxSteer = (f32)MC_STEER_MAX_LOW + (((f32)MC_STEER_MAX_HIGH - (f32)MC_STEER_MAX_LOW) * r);

        if (sMc.drift == MC_DRIFT_ON) {
            f32 t = (MKW_DRIFT_TURN_STICK * sMc.turnSmooth) + (MKW_DRIFT_TURN_DIR * (f32)sMc.driftDir);

            if (t > 1.0f) {
                t = 1.0f;
            }
            if (t < -1.0f) {
                t = -1.0f;
            }
            wantAngle = (s16)(t * maxSteer * MC_DRIFT_TURN_SCALE);
        } else if (sMc.drift == MC_DRIFT_HOP) {
            wantAngle = (s16)(((stickX > 0.2f) ? 1.0f : ((stickX < -0.2f) ? -1.0f : 0.0f)) * maxSteer);
        } else {
            wantAngle = (s16)(raw * maxSteer);
        }

        resp = (sMc.drift == MC_DRIFT_ON) ? MKW_DRIFT_RESP : MKW_HANDLING_RESP;
        resp = 1.0f - MasterCycle_PerTick(1.0f - resp);
        sMc.turnSmooth += (raw - sMc.turnSmooth) * resp;
        if (sMc.turnSmooth > 1.0f) {
            sMc.turnSmooth = 1.0f;
        }
        if (sMc.turnSmooth < -1.0f) {
            sMc.turnSmooth = -1.0f;
        }
        Math_SmoothStepToS(&sMc.steer, wantAngle, 2, (wantAngle == 0) ? MC_STEER_RETURN : MC_STEER_RATE, 0x20);

        tanDelta = Math_SinS(sMc.steer) / Math_CosS(sMc.steer);
        yawRad = (sMc.speed / MC_WHEELBASE) * tanDelta;
        if (sMc.airborne) {
            yawRad *= 0.35f;
        }
        turn = (s16)(yawRad * (0x8000 / M_PI));
        wantSteer = sMc.steer;
    }

    {
        f32 rearX = actor->world.pos.x + (Math_SinS(actor->world.rot.y) * MC_HUB_B_Z);
        f32 rearZ = actor->world.pos.z + (Math_CosS(actor->world.rot.y) * MC_HUB_B_Z);

        actor->world.rot.y += turn;

        actor->world.pos.x = rearX - (Math_SinS(actor->world.rot.y) * MC_HUB_B_Z);
        actor->world.pos.z = rearZ - (Math_CosS(actor->world.rot.y) * MC_HUB_B_Z);
    }

    {
        s16 want = (sMc.drift == MC_DRIFT_ON) ? (s16)(-sMc.driftDir * MC_DRIFT_ANGLE) : 0;

        if (sMc.driftAngle < want) {
            sMc.driftAngle += (want > 0) ? MC_DRIFT_ANGLE_RATE : MC_DRIFT_ANGLE_DECR;
            if (sMc.driftAngle > want) {
                sMc.driftAngle = want;
            }
        } else if (sMc.driftAngle > want) {
            sMc.driftAngle -= (want < 0) ? MC_DRIFT_ANGLE_RATE : MC_DRIFT_ANGLE_DECR;
            if (sMc.driftAngle < want) {
                sMc.driftAngle = want;
            }
        }
    }

    {
        s16 turnVel = actor->world.rot.y - sMc.lastYaw;
        f32 sp = fabsf(sMc.speed) / MC_MAX_SPEED;
        s16 want = (s16)(-(f32)MC_LEAN_MAX * sp * ((f32)turnVel / 0x400));

        if (sMc.drift == MC_DRIFT_ON) {
            f32 into = 0.6f + (0.4f * stickX * (f32)sMc.driftDir);

            if (into < 0.2f) {
                into = 0.2f;
            }
            want = (s16)(-(f32)sMc.driftDir * (f32)MC_DRIFT_LEAN * into);
        } else {
            if (want > MC_LEAN_MAX) {
                want = MC_LEAN_MAX;
            }
            if (want < -MC_LEAN_MAX) {
                want = -MC_LEAN_MAX;
            }
        }
        Math_SmoothStepToS(&sMc.lean, want, 3, 0x280, 0x20);
        sMc.lastYaw = actor->world.rot.y;
    }

    sMc.wheelSpin += sMc.speed / MC_WHEEL_RADIUS;
    if (sMc.wheelSpin > (2.0f * M_PI)) {
        sMc.wheelSpin -= 2.0f * M_PI;
    }
    if (sMc.wheelSpin < 0.0f) {
        sMc.wheelSpin += 2.0f * M_PI;
    }
}

// Player_Action_8084CC98 copies the horse's SkelAnime curFrame into Link's riding animation.
static void MasterCycle_UpdateRiderPose(EnHorse* horse) {
    f32 sp = fabsf(sMc.speed);
    s32 want;
    AnimationHeader* anim;
    f32 playSpeed;

    if (sMc.airborne) {
        want = ENHORSE_ANIM_LOW_JUMP;
        anim = (AnimationHeader*)gEponaJumpingAnim;
        playSpeed = 1.0f;
    } else if (sp < MC_STOPPED) {
        want = ENHORSE_ANIM_IDLE;
        anim = (AnimationHeader*)gEponaIdleAnim;
        playSpeed = 1.0f;
    } else if (sp < MC_MAX_SPEED * 0.33f) {
        want = ENHORSE_ANIM_WALK;
        anim = (AnimationHeader*)gEponaWalkingAnim;
        playSpeed = 0.5f + (sp / (MC_MAX_SPEED * 0.33f));
    } else if (sp < MC_MAX_SPEED * 0.66f) {
        want = ENHORSE_ANIM_TROT;
        anim = (AnimationHeader*)gEponaTrottingAnim;
        playSpeed = 0.75f + ((sp - (MC_MAX_SPEED * 0.33f)) / (MC_MAX_SPEED * 0.66f));
    } else {
        want = ENHORSE_ANIM_GALLOP;
        anim = (AnimationHeader*)gEponaGallopingAnim;
        playSpeed = 0.9f + ((sp - (MC_MAX_SPEED * 0.66f)) / (MC_MAX_SPEED * 1.3f));
    }

    if (horse->animationIdx != want) {
        horse->animationIdx = want;
        Animation_PlayLoop(&horse->skin.skelAnime, anim);
    }
    horse->skin.skelAnime.playSpeed = playSpeed;
    SkelAnime_Update(&horse->skin.skelAnime);
    horse->curFrame = horse->skin.skelAnime.curFrame;
}

static void MasterCycle_UpdateHitCollider(PlayState* play, Actor* actor) {
    CombatColliderConfig cfg;
    Vec3f pos;
    f32 sp = fabsf(sMc.speed);
    f32 t;
    u8 dmg;

    if (sp < MC_HIT_MIN_SPEED) {
        return;
    }
    t = (sp - MC_HIT_MIN_SPEED) / (MC_MAX_SPEED * (1.0f + MC_MT_BONUS) - MC_HIT_MIN_SPEED);
    if (t > 1.0f) {
        t = 1.0f;
    }
    dmg = (u8)(MC_HIT_DAMAGE_LOW + (s32)((MC_HIT_DAMAGE_HIGH - MC_HIT_DAMAGE_LOW) * t + 0.5f));

    cfg.dmgFlags = DMG_HAMMER;
    cfg.damage = dmg;
    cfg.effect = 0;
    cfg.radius = MC_WHEELBASE * 0.5f + 8.0f;
    cfg.height = MC_BODY_TOP;
    if (!sMc.hitColReady) {
        Combat_InitCylinder(play, &sMc.hitCol, actor, &cfg);
        sMc.hitColReady = 1;
    }
    pos = actor->world.pos;
    Combat_UpdateCylinder(&sMc.hitCol, &pos, &cfg);
    Combat_RegisterCollider(play, &sMc.hitCol);
    if (Combat_CheckHit(&sMc.hitCol)) {
        sMc.hitCol.base.atFlags &= ~AT_HIT;
        sMc.speed *= 0.85f;
    }
}

static void MasterCycle_Suspension(Actor* actor) {
    f32 stiff[2];
    f32 land;
    f32 mid;
    f32 tilt;
    s32 i;
    s32 grounded = 0;

    stiff[0] = MC_SUS_STIFFNESS_F;
    stiff[1] = MC_SUS_STIFFNESS;

    if (sMc.launched) {
        land = (sMc.wheelY[0] > sMc.wheelY[1]) ? sMc.wheelY[0] : sMc.wheelY[1];
        sMc.airborne = 1;
        actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        sMc.susReady = 0;
        if ((actor->velocity.y <= 0.0f) && (land > BGCHECK_Y_MIN) && (actor->world.pos.y <= land)) {
            actor->world.pos.y = land;
            actor->velocity.y = 0.0f;
            sMc.launched = 0;
            sMc.airborne = 0;
            actor->bgCheckFlags |= BGCHECKFLAG_GROUND;
        }
        return;
    }

    for (i = 0; i < 2; i++) {
        f32 target = sMc.wheelY[i];
        f32 err;

        if (target <= BGCHECK_Y_MIN) {
            sMc.susY[i] = actor->world.pos.y;
            sMc.susVel[i] = 0.0f;
            continue;
        }
        if (!sMc.susReady) {
            sMc.susY[i] = target;
            sMc.susVel[i] = 0.0f;
        }
        err = target - sMc.susY[i];
        if (err < -MC_SUS_TRAVEL) {
            sMc.susY[i] = actor->world.pos.y;
            sMc.susVel[i] = 0.0f;
            continue;
        }
        if (err > MC_SUS_TRAVEL) {
            sMc.susY[i] = target;
            sMc.susVel[i] = 0.0f;
        } else {
            sMc.susVel[i] += (err * stiff[i]) - (sMc.susVel[i] * MC_SUS_DAMPING);
            sMc.susY[i] += sMc.susVel[i];
        }
        grounded++;
    }
    sMc.susReady = 1;

    if (grounded == 0) {
        sMc.airborne = 1;
        actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        return;
    }

    mid = (sMc.susY[0] + sMc.susY[1]) * 0.5f;
    if ((mid - actor->world.pos.y) < -MC_SUS_TRAVEL) {
        sMc.airborne = 1;
        actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        return;
    }
    actor->world.pos.y = mid;
    if (actor->velocity.y < 0.0f) {
        actor->velocity.y = 0.0f;
    }

    tilt = sMc.susY[0] - sMc.susY[1];
    if (tilt > (MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT)) {
        tilt = MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT;
    }
    if (tilt < -(MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT)) {
        tilt = -(MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT);
    }
    sMc.terrainPitch = (s16)(Math_FAtan2F(tilt, MC_WHEELBASE) * (0x8000 / M_PI));

    sMc.airborne = 0;
    actor->bgCheckFlags |= BGCHECKFLAG_GROUND;
}

static void MasterCycle_FilterInput(Player* player, Input* input) {
    if (sMc.actor && player->rideActor == sMc.actor) {
        input->cur.button &= ~BTN_B;
        input->press.button &= ~BTN_B;
    }
}

static void MasterCycle_Update(Actor* thisx, PlayState* play) {
    EnHorse* horse = (EnHorse*)thisx;
    Player* player = GET_PLAYER(play);
    u8 riding;

    if (thisx != sMc.actor) {
        thisx->update = EnHorse_Update;
        thisx->draw = EnHorse_Draw;
        return;
    }
    sMc.age++;

    if (sMc.phase == MC_PHASE_MATERIALIZE) {
        f32 t = 1.0f - ((f32)sMc.phaseTimer / (f32)MC_MATERIALIZE_TICKS);
        f32 sc = MC_MODEL_SCALE * (0.05f + (0.95f * t));

        Actor_SetScale(thisx, sc);
        thisx->speedXZ = 0.0f;
        Actor_MoveXZGravity(thisx);
        Actor_UpdateBgCheckInfo(play, thisx, 30.0f, MC_BODY_HALF_WIDTH, MC_BODY_TOP, 0x1D);
        thisx->prevPos = thisx->world.pos;
        if (--sMc.phaseTimer <= 0) {
            sMc.phase = MC_PHASE_ACTIVE;
            Actor_SetScale(thisx, MC_MODEL_SCALE);
        }
        MasterCycle_SeatPos(thisx, &horse->riderPos);
        return;
    }
    if (sMc.phase == MC_PHASE_DISMISS) {
        f32 t = (f32)sMc.phaseTimer / (f32)MC_DISMISS_TICKS;

        Actor_SetScale(thisx, MC_MODEL_SCALE * (0.05f + (0.95f * t)));
        if (--sMc.phaseTimer <= 0) {
            thisx->bgCheckFlags &= ~0x800;
            MasterCycle_TrailsFree(play);
            Actor_Kill(thisx);
            MasterCycle_Forget();
        }
        return;
    }

    riding = (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) && (player->rideActor == thisx) &&
             (thisx->child == &player->actor);

    if (!riding && (sMc.phase == MC_PHASE_ACTIVE)) {
        f32 dx = player->actor.world.pos.x - thisx->world.pos.x;
        f32 dz = player->actor.world.pos.z - thisx->world.pos.z;
        f32 dy = player->actor.world.pos.y - thisx->world.pos.y;
        f32 dist = sqrtf((dx * dx) + (dz * dz));

        horse->action = MC_ACTION_PARKED;
        if ((dist < MC_MOUNT_RANGE) && (fabsf(dy) < MC_MOUNT_MAX_DY) && (player->rideActor == NULL) &&
            (fabsf(sMc.speed) < MC_STOPPED)) {
            f32 localX = (dx * Math_CosS(thisx->shape.rot.y)) - (dz * Math_SinS(thisx->shape.rot.y));

            sMc.mountSide = (localX >= 0.0f) ? 1 : -1;
            Actor_SetRideActor(play, thisx, sMc.mountSide);
        }
        sMc.speed *= 0.9f;
        if (fabsf(sMc.speed) < 0.05f) {
            sMc.speed = 0.0f;
        }
        sMc.wheelie = 0;
        sMc.drift = MC_DRIFT_NONE;
        sMc.boostTimer = 0;
        Math_SmoothStepToS(&sMc.wheeliePitch, 0, 3, MC_WHEELIE_PITCH_STEP, 0x40);
        Math_SmoothStepToS(&sMc.lean, 0, 3, 0x280, 0x20);
        Math_SmoothStepToS(&sMc.steer, 0, 3, MC_STEER_RETURN, 0x40);
    } else if (riding && (player->av2.actionVar2 != 0)) {
        // Player_Action_8084CC98 leaves av2 at 0 for the whole mount animation, so av2 != 0 means seated.
        horse->action = MC_ACTION_RIDDEN;
        MasterCycle_Drive(play, thisx, player);
    } else if (riding) {
        horse->action = MC_ACTION_RIDDEN;
        sMc.speed = 0.0f;
    } else {
        sMc.speed *= 0.9f;
    }
    if (!riding && (horse->stateFlags & ENHORSE_FLAG_6)) {
        horse->stateFlags &= ~ENHORSE_FLAG_6;
    }
    sMc.wasRiding = riding ? 1 : (sMc.wasRiding == 2 ? 2 : 0);

    MasterCycle_Move(play, thisx);
    MasterCycle_ProbeTerrain(play, thisx);
    MasterCycle_Suspension(thisx);
    MasterCycle_Trails(play, thisx);

    if ((thisx->bgCheckFlags & BGCHECKFLAG_WATER) && (thisx->yDistToWater > MC_DEEP_WATER)) {
        sMc.speed = 0.0f;
        if (riding) {
            MasterCycle_ForceDismount();
        }
    }

    sMc.pitch = (s16)(-sMc.terrainPitch - sMc.wheeliePitch);
    // Nose-up is negative rot.x. In a drift world.rot.y is the travel direction and shape.rot.y the facing.
    thisx->shape.rot.y = (s16)(thisx->world.rot.y + sMc.driftAngle);
    thisx->shape.rot.x = sMc.pitch;
    thisx->shape.rot.z = sMc.lean;
    thisx->world.rot.x = 0;
    thisx->world.rot.z = 0;

    MasterCycle_SeatPos(thisx, &horse->riderPos);
    MasterCycle_UpdateRiderPose(horse);

    // The ride action copies only the yaw, so the lean and pitch are written to his shape.rot here and zeroed on
    // dismount.
    if (riding) {
        player->actor.shape.rot.x = thisx->shape.rot.x;
        player->actor.shape.rot.z = thisx->shape.rot.z;
        sMc.riderTilted = 1;
    } else if (sMc.riderTilted) {
        player->actor.shape.rot.x = 0;
        player->actor.shape.rot.z = 0;
        sMc.riderTilted = 0;
    }

    Collider_UpdateCylinder(thisx, &horse->cyl1);
    Collider_UpdateCylinder(thisx, &horse->cyl2);
    horse->cyl1.dim.pos.x += (s16)(Math_SinS(thisx->shape.rot.y) * 22.0f);
    horse->cyl1.dim.pos.z += (s16)(Math_CosS(thisx->shape.rot.y) * 22.0f);
    horse->cyl2.dim.pos.x += (s16)(Math_SinS(thisx->shape.rot.y) * -24.0f);
    horse->cyl2.dim.pos.z += (s16)(Math_CosS(thisx->shape.rot.y) * -24.0f);
    CollisionCheck_SetOC(play, &play->colChkCtx, &horse->cyl1.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &horse->cyl2.base);
    if (riding) {
        MasterCycle_UpdateHitCollider(play, thisx);
    }

    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 45.0f;
    thisx->speedXZ = fabsf(sMc.speed);
}

static void MasterCycle_DrawChargeFlames(PlayState* play, Actor* actor) {
    static const Color_RGBA8 sPrim[3] = {
        { 255, 200, 60, 255 },
        { 0, 170, 255, 255 },
        { 255, 170, 255, 255 },
    };
    static const Color_RGB8 sEnv[3] = {
        { 255, 80, 0 },
        { 0, 0, 255 },
        { 100, 0, 255 },
    };
    f32 charge;
    s32 stage;
    f32 grow;
    s32 i;

    if ((sMc.drift != MC_DRIFT_ON) || (MKW_MT_MAX <= 0)) {
        return;
    }
    charge = (f32)sMc.mtCharge / (f32)MKW_MT_MAX;
    if (charge <= 0.02f) {
        return;
    }
    stage = (charge < MC_MT_STAGE1) ? 0 : ((charge < MC_MT_STAGE2) ? 1 : 2);
    grow = 0.55f + (0.45f * charge) + ((stage == 2) ? 0.35f : 0.0f);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0, (play->gameplayFrames * -20) & 511, 32,
                                  128, 0, 0, 0, -20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, sPrim[stage].r, sPrim[stage].g, sPrim[stage].b, sPrim[stage].a);
    gDPSetEnvColor(POLY_XLU_DISP++, sEnv[stage].r, sEnv[stage].g, sEnv[stage].b, 0);

    for (i = 0; i < MC_MT_FLAME_COUNT; i++) {
        f32 side = (i == 0) ? MC_MT_FLAME_SPREAD : -MC_MT_FLAME_SPREAD;
        f32 sc = MC_MT_FLAME_SCALE * grow;

        Matrix_Push();
        Matrix_Translate(side, MC_MT_PIPE_Y, MC_MT_PIPE_Z, MTXMODE_APPLY);
        Matrix_RotateX(-M_PI / 2.0f, MTXMODE_APPLY);
        Matrix_RotateY((s16)((Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) - actor->shape.rot.y) + 0x8000) *
                           (M_PI / 32768.0f),
                       MTXMODE_APPLY);
        Matrix_Scale(sc, sc, sc, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
        Matrix_Pop();
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static void MasterCycle_Draw(Actor* thisx, PlayState* play) {
    f32 sc = thisx->scale.x * MC_MODEL_SCALE;
    f32 steer = BINANG_TO_RAD(sMc.steer);

    MasterCycle_LoadDLs();
    if (sMcDlBody == NULL) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y + MC_MODEL_Y_OFFSET, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(thisx->shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(thisx->shape.rot.z), MTXMODE_APPLY);
    Matrix_Translate(0.0f, MC_HUB_Y, MC_HUB_B_Z, MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(thisx->shape.rot.x), MTXMODE_APPLY);
    Matrix_Translate(0.0f, -MC_HUB_Y, -MC_HUB_B_Z, MTXMODE_APPLY);
    Matrix_Scale(sc, sc, sc, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, sMcDlBody);
    if (sMcDlLights != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, sMcDlLights);
    }

    if (sMcDlWheelB != NULL) {
        Matrix_Push();
        Matrix_Translate(0.0f, MC_HUB_Y, MC_HUB_B_Z, MTXMODE_APPLY);
        Matrix_RotateX(sMc.wheelSpin, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, sMcDlWheelB);
        Matrix_Pop();
    }
    MasterCycle_DrawChargeFlames(play, thisx);

    if (sMcDlWheelF != NULL) {
        Matrix_Push();
        Matrix_Translate(0.0f, MC_HUB_Y, MC_HUB_F_Z, MTXMODE_APPLY);
        Matrix_RotateY(steer, MTXMODE_APPLY);
        Matrix_RotateX(sMc.wheelSpin, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, sMcDlWheelF);
        Matrix_Pop();
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
