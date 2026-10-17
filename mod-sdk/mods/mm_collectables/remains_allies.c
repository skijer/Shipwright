// The friendly allies the boss remains summon: Odolwa's beetles, sword moths and flight cloud, Goht's thunder
// and bombchu, Gyorg's fish. Every model is Majora's Mask's own, read from mm.o2r; friend or foe is decided by
// the collider's AT_TYPE_PLAYER bit, which hits every enemy and never Link.
#include "remains.h"

#include <math.h>
#include <stdint.h>

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "objects/gameplay_keep/gameplay_keep.h"

// MM sfx ids (2ship mm/include/sfx.h), played through mm_assets' service.
#define MM_SE_EN_MB_MOTH_FLY 0x399B
#define MM_SE_EN_MB_MOTH_DEAD 0x399C
#define MM_SE_EN_COMMON_THUNDER 0x394B
#define MM_SE_EN_BOMCHU_WALK 0x3828
#define MM_SE_EN_BOMCHU_AIM 0x3855
#define MM_SE_EN_BOMCHU_RUN 0x3856
#define MM_SE_EN_PIRANHA_ATTACK 0x39F4

#define ALLY_FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)
#define ALLY_YAW_SCALE 4
#define ALLY_YAW_MAXSTEP 0x1000
#define ALLY_YAW_MINSTEP 0x100
#define ALLY_WALL_HEIGHT 26.0f
#define ALLY_WALL_RADIUS 10.0f
#define BGCHECK_WALL_AND_FLOOR 0x5
#define BGCHECK_WALL_ONLY 0x1

static const SOHModApi* sApi;
static s16 sBugId = -1;
static s16 sChuId = -1;
static s16 sFishId = -1;

// Boss DLs can branch into segment 0x0C (a scene cull list); away from their arena it must point somewhere safe.
static Gfx sSegment0xCNoop[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

static bool HasResources(const char* first, const char* second) {
    return sApi->HasResource(first) && sApi->HasResource(second);
}

// ── Shared motion ───────────────────────────────────────────────────────────

Actor* RemainsAlly_FindNearestEnemy(PlayState* play, Vec3f* from, f32 maxDist) {
    static const u8 categories[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };
    Actor* nearest = NULL;
    f32 nearestDist = maxDist;

    for (u32 i = 0; i < ARRAY_COUNT(categories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[categories[i]].head; actor != NULL; actor = actor->next) {
            if (actor->update == NULL || actor->colChkInfo.health == 0) {
                continue;
            }
            f32 dist = Actor_WorldDistXZToPoint(actor, from);
            if (dist <= nearestDist) {
                nearestDist = dist;
                nearest = actor;
            }
        }
    }
    return nearest;
}

// Actor_MoveXZGravity flies along world.rot.y, so that is the yaw that gets steered.
static void HomeTowardPos(PlayState* play, Actor* actor, Vec3f* targetPos, f32 speed) {
    Math_SmoothStepToS(&actor->world.rot.y, Actor_WorldYawTowardPoint(actor, targetPos), ALLY_YAW_SCALE,
                       ALLY_YAW_MAXSTEP, ALLY_YAW_MINSTEP);
    actor->shape.rot.y = actor->world.rot.y;
    actor->speedXZ = speed;
    Actor_MoveXZGravity(actor);
    Actor_UpdateBgCheckInfo(play, actor, ALLY_WALL_HEIGHT, ALLY_WALL_RADIUS, 0.0f, BGCHECK_WALL_AND_FLOOR);
}

// A spot in a loose ball trailing behind Link, fanned and ring-depthed by a per-ally seed.
static Vec3f BallAnchorBehind(Player* player, u32 seed, f32 distance, f32 spacing) {
    s16 behindYaw = player->actor.shape.rot.y + 0x8000;
    s16 fan = (s16)(((s32)(seed & 3) - 1) * 0x1800);
    f32 ring = distance + (f32)((seed >> 2) & 1) * spacing;
    Vec3f anchor = player->actor.world.pos;

    anchor.x += Math_SinS(behindYaw + fan) * ring;
    anchor.z += Math_CosS(behindYaw + fan) * ring;
    return anchor;
}

// Boids separation, so a school of the same ally stays a spaced ball instead of one dot.
static void SeparateFromKin(Actor* self, PlayState* play, s16 id, f32 radius, bool (*isKin)(Actor*)) {
    for (Actor* other = play->actorCtx.actorLists[ACTORCAT_MISC].head; other != NULL; other = other->next) {
        if (other == self || other->id != id || (isKin != NULL && !isKin(other))) {
            continue;
        }
        f32 dist = Math_Vec3f_DistXZ(&self->world.pos, &other->world.pos);
        if (dist < radius && dist > 0.1f) {
            s16 away = Math_Vec3f_Yaw(&other->world.pos, &self->world.pos);
            f32 push = (radius - dist) * 0.25f;
            self->world.pos.x += Math_SinS(away) * push;
            self->world.pos.z += Math_CosS(away) * push;
        }
    }
}

// ── Odolwa's bug: ground beetle, sword moth, Goht thunder, flight cloud ─────

#define BUG_MODE_BEETLE 0
#define BUG_MODE_PROJECTILE 1
#define BUG_MODE_THUNDER 2
#define BUG_MODE_CLOUD 3

#define BUG_LIMB_MAX 0x15
#define BUG_BEETLE_SCALE 0.025f
#define BUG_SEARCH_RANGE 600.0f
#define BUG_HOP_INTERVAL 15
#define BUG_HOP_STRENGTH 4.5f
#define BUG_CHASE_SPEED 4.2f
#define BUG_IDLE_SPEED 2.5f
#define BUG_HIT_DIST 30.0f
#define BUG_FOLLOW_DIST 55.0f
#define BUG_IDLE_LIFETIME 300
#define BUG_DRAW_SCALE 4.5f
#define BUG_DRAW_Y_LIFT 8.0f
#define BUG_FLAP_RATE 0x1400
#define BUG_PROJECTILE_SPEED 26.0f
#define BUG_PROJECTILE_TTL 45
#define BUG_HOME_RANGE 550.0f
#define BUG_HOME_CONE 0x3000
#define BUG_HOME_STEP 0x600
#define BUG_THUNDER_SPEED 34.0f
#define BUG_THUNDER_TTL 40
#define BUG_THUNDER_DAMAGE 6
#define BUG_THUNDER_SFX_CADENCE 8
#define BUG_CLOUD_RADIUS 22.0f
#define BUG_CLOUD_BELOW 10.0f
#define BUG_CLOUD_ORBIT 0x0300
#define BUG_BALL_DIST 50.0f
#define BUG_BALL_SPACING 26.0f
#define BUG_BALL_SEP 22.0f
#define BUG_LINK_MOVE 2.5f

static const char sBugSkel[] = "__OTR__objects/object_boss01/gOdolwaBugSkel";
static const char sBugCrawlAnim[] = "__OTR__objects/object_boss01/gOdolwaBugCrawlAnim";
static const char sMothSetupDL[] = "__OTR__overlays/ovl_En_Tanron1/ovl_En_Tanron1_DL_001888";
static const char sMothModelDL[] = "__OTR__overlays/ovl_En_Tanron1/ovl_En_Tanron1_DL_001900";
static const char sThunderMaterialDL[] = "__OTR__objects/object_boss_hakugin/gGohtLightningMaterialDL";
static const char sThunderModelDL[] = "__OTR__objects/object_boss_hakugin/gGohtLightningModelDL";

typedef struct {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[BUG_LIMB_MAX];
    Vec3s morphTable[BUG_LIMB_MAX];
    ColliderCylinder collider;
    s16 mode;
    s16 hopTimer; // BUG_MODE_CLOUD: its angle around the ring
    s16 lifeTimer;
    s16 flapTimer;
    u8 isModelReady;
} RemainsAllyBug;

// Radius and height are generous so the hit lands a few frames before the proximity kill fires.
static ColliderCylinderInit sBugColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { DMG_DEKU_STICK, 0x00, 0x04 },
      { 0xF7CFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { 24, 44, 0, { 0, 0, 0 } },
};

// mm.o2r may be mounted after the ally spawns, so a missing model is retried every frame, never latched.
static bool Bug_TryLoadModel(RemainsAllyBug* self, PlayState* play) {
    if (self->isModelReady) {
        return true;
    }
    switch (self->mode) {
        case BUG_MODE_THUNDER:
            self->isModelReady = HasResources(sThunderMaterialDL, sThunderModelDL);
            break;
        case BUG_MODE_PROJECTILE:
        case BUG_MODE_CLOUD:
            self->isModelReady = HasResources(sMothSetupDL, sMothModelDL);
            break;
        case BUG_MODE_BEETLE:
            if (HasResources(sBugSkel, sBugCrawlAnim)) {
                SkelAnime_InitFlex(play, &self->skelAnime, (FlexSkeletonHeader*)sBugSkel,
                                   (AnimationHeader*)sBugCrawlAnim, self->jointTable, self->morphTable, BUG_LIMB_MAX);
                Animation_PlayLoop(&self->skelAnime, (AnimationHeader*)sBugCrawlAnim);
                self->isModelReady = true;
            }
            break;
        default:
            break;
    }
    return self->isModelReady;
}

static void Bug_Init(Actor* thisx, PlayState* play) {
    RemainsAllyBug* self = (RemainsAllyBug*)thisx;

    self->mode = thisx->params;
    if (self->mode == BUG_MODE_BEETLE) {
        Actor_SetScale(thisx, BUG_BEETLE_SCALE);
        ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 12.0f);
        thisx->gravity = -1.0f;
        thisx->minVelocityY = -8.0f;
        self->hopTimer = (s16)Rand_ZeroFloat((f32)BUG_HOP_INTERVAL);
        self->lifeTimer = BUG_IDLE_LIFETIME;
    } else {
        self->flapTimer = (s16)Rand_ZeroFloat(65535.0f);
        thisx->shape.shadowDraw = NULL;
        Actor_SetScale(thisx, 0.01f);
        thisx->gravity = 0.0f;
        thisx->minVelocityY = 0.0f;
        thisx->speedXZ = (self->mode == BUG_MODE_THUNDER) ? BUG_THUNDER_SPEED : BUG_PROJECTILE_SPEED;
        thisx->world.rot.y = thisx->shape.rot.y;
        self->lifeTimer = (self->mode == BUG_MODE_THUNDER) ? BUG_THUNDER_TTL : BUG_PROJECTILE_TTL;
        if (self->mode == BUG_MODE_CLOUD) {
            thisx->speedXZ = 0.0f;
            self->hopTimer = thisx->shape.rot.y;
            self->lifeTimer = 0;
            Actor_SetScale(thisx, 0.014f);
        }
    }
    Bug_TryLoadModel(self, play);
    Collider_InitCylinder(play, &self->collider);
    Collider_SetCylinder(play, &self->collider, thisx, &sBugColliderInit);
    if (self->mode == BUG_MODE_THUNDER) {
        self->collider.info.toucher.dmgFlags = DMG_MAGIC_FIRE;
        self->collider.info.toucher.damage = BUG_THUNDER_DAMAGE;
    }
}

static void Bug_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((RemainsAllyBug*)thisx)->collider);
}

static bool IsBeetle(Actor* actor) {
    return ((RemainsAllyBug*)actor)->mode == BUG_MODE_BEETLE;
}

static void Bug_Die(RemainsAllyBug* self) {
    Remains_PlayMmSfx(MM_SE_EN_MB_MOTH_DEAD, &self->actor.projectedPos);
    Actor_Kill(&self->actor);
}

static void Bug_OrbitUnderLink(RemainsAllyBug* self, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!Remains_IsOdolwaFlying()) {
        Actor_Kill(&self->actor);
        return;
    }
    self->hopTimer += BUG_CLOUD_ORBIT;
    self->actor.world.pos.x = player->actor.world.pos.x + Math_SinS(self->hopTimer) * BUG_CLOUD_RADIUS;
    self->actor.world.pos.z = player->actor.world.pos.z + Math_CosS(self->hopTimer) * BUG_CLOUD_RADIUS;
    self->actor.world.pos.y = player->actor.world.pos.y - BUG_CLOUD_BELOW;
    self->actor.shape.rot.y = self->hopTimer;
}

// The sword moth dies on walls; Goht's thunder flies through them and just times out.
static void Bug_FlyAsProjectile(RemainsAllyBug* self, PlayState* play) {
    Actor* target = RemainsAlly_FindNearestEnemy(play, &self->actor.world.pos, BUG_HOME_RANGE);

    if (target != NULL) {
        s16 toTarget = Actor_WorldYawTowardActor(&self->actor, target);
        if (ABS((s16)(toTarget - self->actor.world.rot.y)) < BUG_HOME_CONE) {
            Math_SmoothStepToS(&self->actor.world.rot.y, toTarget, 3, BUG_HOME_STEP, 0);
            self->actor.shape.rot.y = self->actor.world.rot.y;
        }
    }
    Actor_MoveXZGravity(&self->actor);
    if (self->mode == BUG_MODE_PROJECTILE) {
        Actor_UpdateBgCheckInfo(play, &self->actor, 20.0f, 12.0f, 0.0f, BGCHECK_WALL_ONLY);
    } else if ((self->lifeTimer % BUG_THUNDER_SFX_CADENCE) == 0) {
        Remains_PlayMmSfx(MM_SE_EN_COMMON_THUNDER, &self->actor.projectedPos);
    }
    Collider_UpdateCylinder(&self->actor, &self->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &self->collider.base);
    bool hitWall = self->mode == BUG_MODE_PROJECTILE && (self->actor.bgCheckFlags & BGCHECKFLAG_WALL);
    if ((self->collider.base.atFlags & AT_HIT) || hitWall) {
        self->collider.base.atFlags &= ~AT_HIT;
        Bug_Die(self);
    }
}

// A beetle trails Link in the ball and only breaks off to swarm, kamikaze, while he holds still.
static void Bug_HopAsBeetle(RemainsAllyBug* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f anchor = BallAnchorBehind(player, (u32)((uintptr_t)self >> 5), BUG_BALL_DIST, BUG_BALL_SPACING);
    bool isLinkMoving = player->linearVelocity > BUG_LINK_MOVE;
    Actor* target =
        isLinkMoving ? NULL : RemainsAlly_FindNearestEnemy(play, &player->actor.world.pos, BUG_SEARCH_RANGE);
    bool canHop = self->hopTimer <= 0 && (self->actor.bgCheckFlags & BGCHECKFLAG_GROUND);

    SeparateFromKin(&self->actor, play, sBugId, BUG_BALL_SEP, IsBeetle);
    if (canHop) {
        self->actor.velocity.y = BUG_HOP_STRENGTH;
        self->hopTimer = BUG_HOP_INTERVAL;
    }
    if (target == NULL) {
        f32 distance = Math_Vec3f_DistXZ(&self->actor.world.pos, &anchor);
        f32 speed = (distance > BUG_FOLLOW_DIST) ? BUG_CHASE_SPEED : (distance > 10.0f) ? BUG_IDLE_SPEED : 0.0f;
        HomeTowardPos(play, &self->actor, &anchor, speed);
        return;
    }
    if (canHop) {
        Remains_PlayMmSfx(MM_SE_EN_MB_MOTH_FLY, &self->actor.projectedPos);
    }
    HomeTowardPos(play, &self->actor, &target->world.pos, BUG_CHASE_SPEED);
    Collider_UpdateCylinder(&self->actor, &self->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &self->collider.base);
    bool hasHit = (self->collider.base.atFlags & AT_HIT) != 0;
    self->collider.base.atFlags &= ~AT_HIT;
    if (hasHit || Actor_WorldDistXZToActor(&self->actor, target) < BUG_HIT_DIST) {
        Bug_Die(self);
    }
}

static void Bug_Update(Actor* thisx, PlayState* play) {
    RemainsAllyBug* self = (RemainsAllyBug*)thisx;

    if (Bug_TryLoadModel(self, play) && self->mode == BUG_MODE_BEETLE) {
        SkelAnime_Update(&self->skelAnime);
    }
    if (self->mode != BUG_MODE_BEETLE) {
        self->flapTimer += BUG_FLAP_RATE;
    }
    if (self->mode == BUG_MODE_CLOUD) {
        Bug_OrbitUnderLink(self, play);
        return;
    }
    if (self->hopTimer > 0) {
        self->hopTimer--;
    }
    // Every bug expires, so one chasing a foe it can never reach does not live forever.
    if (self->lifeTimer > 0 && --self->lifeTimer == 0) {
        Bug_Die(self);
        return;
    }
    if (self->mode == BUG_MODE_BEETLE) {
        Bug_HopAsBeetle(self, play);
    } else {
        Bug_FlyAsProjectile(self, play);
    }
}

// Goht's real bolt, drawn like BossHakugin's: each segment twice, the second turned a quarter on Z.
static void Bug_DrawThunder(RemainsAllyBug* self, PlayState* play) {
    s16 yaw = self->actor.shape.rot.y;
    u8 alpha = (play->gameplayFrames & 1) ? 255 : 160;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 255, 255, 0);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sThunderMaterialDL);
    for (s32 segment = 0; segment < 5; segment++) {
        Vec3s rot = { (s16)((segment & 1) ? -0x0500 : 0x0500), (s16)(yaw + ((segment & 1) ? 0x0900 : -0x0900)), 0 };
        f32 back = 70.0f * segment;

        Matrix_SetTranslateRotateYXZ(self->actor.world.pos.x - Math_SinS(yaw) * back, self->actor.world.pos.y,
                                     self->actor.world.pos.z - Math_CosS(yaw) * back, &rot);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, alpha);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sThunderModelDL);
        Matrix_RotateZ((f32)M_PI / 2.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sThunderModelDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Bug_DrawMoth(RemainsAllyBug* self, PlayState* play) {
    f32 flap = 0.75f + 0.30f * Math_SinS(self->flapTimer);

    OPEN_DISPS(play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMothSetupDL);
    Matrix_Translate(self->actor.world.pos.x, self->actor.world.pos.y + BUG_DRAW_Y_LIFT, self->actor.world.pos.z,
                     MTXMODE_NEW);
    Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
    Matrix_Scale(BUG_DRAW_SCALE * flap, BUG_DRAW_SCALE, BUG_DRAW_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMothModelDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Bug_Draw(Actor* thisx, PlayState* play) {
    RemainsAllyBug* self = (RemainsAllyBug*)thisx;

    if (!self->isModelReady) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sSegment0xCNoop);
    CLOSE_DISPS(play->state.gfxCtx);
    switch (self->mode) {
        case BUG_MODE_THUNDER:
            Bug_DrawThunder(self, play);
            break;
        case BUG_MODE_PROJECTILE:
        case BUG_MODE_CLOUD:
            Bug_DrawMoth(self, play);
            break;
        default:
            SkelAnime_DrawFlexOpa(play, self->skelAnime.skeleton, self->skelAnime.jointTable,
                                  self->skelAnime.dListCount, NULL, NULL, NULL);
            break;
    }
}

static Actor* SpawnBug(PlayState* play, Vec3f* pos, s16 rotY, s16 mode) {
    if (sBugId < 0) {
        return NULL;
    }
    return Actor_Spawn(&play->actorCtx, play, sBugId, pos->x, pos->y, pos->z, 0, rotY, 0, mode);
}

Actor* RemainsAllyBug_Spawn(PlayState* play, Vec3f* pos, s16 rotY) {
    return SpawnBug(play, pos, rotY, BUG_MODE_BEETLE);
}

Actor* RemainsAllyBug_SpawnCloud(PlayState* play, Vec3f* pos, s16 rotY) {
    return SpawnBug(play, pos, rotY + (s16)Rand_CenteredFloat(65535.0f), BUG_MODE_CLOUD);
}

Actor* RemainsAllyBug_SpawnProjectile(PlayState* play, Vec3f* pos, s16 rotY) {
    return SpawnBug(play, pos, rotY, BUG_MODE_PROJECTILE);
}

// The charge sets the bolt's lifetime, so how far it reaches, and how hard it hits.
Actor* RemainsAllyBug_SpawnThunderCharged(PlayState* play, Vec3f* pos, s16 rotY, s16 ttl, s16 damage) {
    RemainsAllyBug* bolt = (RemainsAllyBug*)SpawnBug(play, pos, rotY, BUG_MODE_THUNDER);

    if (bolt != NULL) {
        bolt->lifeTimer = ttl;
        bolt->collider.info.toucher.damage = (u8)damage;
    }
    return (Actor*)bolt;
}

// ── Goht's bombchu: Majora's Mask's Real Bombchu, crawling on any surface ───

#define CHU_LIMB_TAIL_END 0x05
#define CHU_LIMB_MAX 0x0A
#define CHU_SCALE 0.010f
#define CHU_SPEED 5.0f
#define CHU_SEEK_RANGE 800.0f
#define CHU_HIT_DIST 18.0f
#define CHU_FUSE 250
#define CHU_RUN_SFX_CADENCE 8

static const char sChuSkel[] = "__OTR__objects/object_rat/gRealBombchuSkel";
static const char sChuRunAnim[] = "__OTR__objects/object_rat/gRealBombchuRunAnim";

typedef struct {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[CHU_LIMB_MAX];
    Vec3s morphTable[CHU_LIMB_MAX];
    Vec3f axisForwards;
    Vec3f axisUp;
    Vec3f axisLeft;
    Actor* target;
    s16 animLoopCounter;
    s16 timer;
    u8 shouldRotateOntoSurfaces;
    u8 hasDetonated;
    u8 isModelReady;
} RemainsAllyChu;

// Goht allows one bombchu at a time; a second R+B detonates this one.
static RemainsAllyChu* sActiveChu;

static bool Chu_TryLoadModel(RemainsAllyChu* self, PlayState* play) {
    if (self->isModelReady || !HasResources(sChuSkel, sChuRunAnim)) {
        return self->isModelReady;
    }
    SkelAnime_InitFlex(play, &self->skelAnime, (FlexSkeletonHeader*)sChuSkel, (AnimationHeader*)sChuRunAnim,
                       self->jointTable, self->morphTable, CHU_LIMB_MAX);
    Animation_PlayLoop(&self->skelAnime, (AnimationHeader*)sChuRunAnim);
    self->isModelReady = true;
    return true;
}

static void Chu_SpawnBlast(RemainsAllyChu* self, PlayState* play) {
    EnBom* bomb = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, self->actor.world.pos.x,
                                      self->actor.world.pos.y, self->actor.world.pos.z, 0, 0, 0, BOMB_BODY);
    if (bomb != NULL) {
        bomb->timer = 0;
    }
}

// The blast hurts Link too, so with no enemy to hit the chu just fizzles.
static void Chu_Detonate(RemainsAllyChu* self, PlayState* play) {
    if (self->hasDetonated) {
        return;
    }
    if (self->target != NULL) {
        Chu_SpawnBlast(self, play);
    } else {
        Remains_PlayMmSfx(MM_SE_EN_BOMCHU_AIM, &self->actor.projectedPos);
    }
    self->actor.speedXZ = 0.0f;
    self->hasDetonated = true;
}

bool RemainsAllyChu_IsAlive(void) {
    return sActiveChu != NULL;
}

void RemainsAllyChu_DetonateActive(PlayState* play) {
    if (sActiveChu == NULL || sActiveChu->hasDetonated) {
        return;
    }
    Chu_SpawnBlast(sActiveChu, play);
    sActiveChu->actor.speedXZ = 0.0f;
    sActiveChu->hasDetonated = true;
}

static void CrossProduct(Vec3f* a, Vec3f* b, Vec3f* dest) {
    dest->x = (a->y * b->z) - (a->z * b->y);
    dest->y = (a->z * b->x) - (a->x * b->z);
    dest->z = (a->x * b->y) - (a->y * b->x);
}

static void NormalizeInPlace(Vec3f* vec, f32 magnitude) {
    vec->x *= 1.0f / magnitude;
    vec->y *= 1.0f / magnitude;
    vec->z *= 1.0f / magnitude;
}

// The crawl engine below is En_Rat's, with the names OoT's own z_en_bom_chu.c uses.
static void Chu_UpdateRotation(RemainsAllyChu* self) {
    MtxF mf;

    mf.xx = self->axisLeft.x;
    mf.yx = self->axisLeft.y;
    mf.zx = self->axisLeft.z;
    mf.xy = self->axisUp.x;
    mf.yy = self->axisUp.y;
    mf.zy = self->axisUp.z;
    mf.xz = self->axisForwards.x;
    mf.yz = self->axisForwards.y;
    mf.zz = self->axisForwards.z;
    Matrix_MtxFToYXZRotS(&mf, &self->actor.world.rot, 0);
    // Negated back in shape.rot.x: the same ledge-stick trick EnBomChu uses.
    self->actor.world.rot.x = -self->actor.world.rot.x;
}

static bool Chu_UpdateFloorPoly(RemainsAllyChu* self, CollisionPoly* floorPoly, PlayState* play) {
    Vec3f normal = { 0.0f, 1.0f, 0.0f };
    Vec3f axis;

    self->actor.floorPoly = floorPoly;
    if (floorPoly != NULL) {
        normal.x = COLPOLY_GET_NORMAL(floorPoly->normal.x);
        normal.y = COLPOLY_GET_NORMAL(floorPoly->normal.y);
        normal.z = COLPOLY_GET_NORMAL(floorPoly->normal.z);
    }
    f32 normDotUp = DOTXYZ(normal, self->axisUp);
    if (fabsf(normDotUp) >= 0.999f) {
        return false;
    }
    f32 angle = Math_FAcosF(normDotUp);
    if (angle < 0.001f) {
        return false;
    }
    CrossProduct(&self->axisUp, &normal, &axis);
    f32 magnitude = Math3D_Vec3fMagnitude(&axis);
    if (magnitude < 0.001f) {
        Chu_Detonate(self, play);
        return false;
    }
    NormalizeInPlace(&axis, magnitude);
    Matrix_RotateAxis(angle, &axis, MTXMODE_NEW);
    Matrix_MultVec3f(&self->axisLeft, &axis);
    self->axisLeft = axis;
    CrossProduct(&self->axisLeft, &normal, &self->axisForwards);
    magnitude = Math3D_Vec3fMagnitude(&self->axisForwards);
    if (magnitude < 0.001f) {
        Chu_Detonate(self, play);
        return false;
    }
    NormalizeInPlace(&self->axisForwards, magnitude);
    self->axisUp = normal;
    return true;
}

static bool Chu_IsOnCollisionPoly(PlayState* play, Vec3f* posA, Vec3f* posB, Vec3f* posResult, CollisionPoly** poly,
                                  s32* bgId) {
    WaterBox* waterBox;
    f32 waterSurface;
    bool isOnWater = WaterBox_GetSurface1(play, &play->colCtx, posB->x, posB->z, &waterSurface, &waterBox) &&
                     waterSurface <= posA->y && posB->y <= waterSurface;

    if (BgCheck_EntityLineTest1(&play->colCtx, posA, posB, posResult, poly, true, true, true, true, bgId) &&
        !(SurfaceType_GetWallFlags(&play->colCtx, *poly, *bgId) & WALL_FLAG_CRAWLSPACE) &&
        (!isOnWater || waterSurface <= posResult->y)) {
        return true;
    }
    if (!isOnWater) {
        return false;
    }
    posResult->x = posB->x;
    posResult->y = waterSurface;
    posResult->z = posB->z;
    *poly = NULL;
    *bgId = BGCHECK_SCENE;
    return true;
}

static bool Chu_IsTouchingSurface(RemainsAllyChu* self, PlayState* play) {
    CollisionPoly* polySide = NULL;
    CollisionPoly* polyUpDown = NULL;
    s32 bgIdSide = BGCHECK_SCENE;
    s32 bgIdUpDown = BGCHECK_SCENE;
    f32 lineLength = 2.0f * self->actor.speedXZ;
    Vec3f posSide;
    Vec3f posUpDown;
    Vec3f posA = { self->actor.world.pos.x + self->axisUp.x * 5.0f, self->actor.world.pos.y + self->axisUp.y * 5.0f,
                   self->actor.world.pos.z + self->axisUp.z * 5.0f };
    Vec3f posB = { self->actor.world.pos.x - self->axisUp.x * 4.0f, self->actor.world.pos.y - self->axisUp.y * 4.0f,
                   self->actor.world.pos.z - self->axisUp.z * 4.0f };

    if (Chu_IsOnCollisionPoly(play, &posA, &posB, &posUpDown, &polyUpDown, &bgIdUpDown)) {
        posB.x = self->axisForwards.x * lineLength + posA.x;
        posB.y = self->axisForwards.y * lineLength + posA.y;
        posB.z = self->axisForwards.z * lineLength + posA.z;
        if (Chu_IsOnCollisionPoly(play, &posA, &posB, &posSide, &polySide, &bgIdSide)) {
            self->shouldRotateOntoSurfaces |= Chu_UpdateFloorPoly(self, polySide, play);
            self->actor.world.pos = posSide;
            self->actor.floorBgId = bgIdSide;
            self->actor.speedXZ = 0.0f;
        } else {
            if (polyUpDown != self->actor.floorPoly) {
                self->shouldRotateOntoSurfaces |= Chu_UpdateFloorPoly(self, polyUpDown, play);
            }
            self->actor.world.pos = posUpDown;
            self->actor.floorBgId = bgIdUpDown;
        }
        return true;
    }

    // Lost the floor: look backwards, left and right for one to wrap onto.
    Vec3f directions[3] = { { -self->axisForwards.x, -self->axisForwards.y, -self->axisForwards.z },
                            self->axisLeft,
                            { -self->axisLeft.x, -self->axisLeft.y, -self->axisLeft.z } };
    self->actor.speedXZ = 0.0f;
    lineLength *= 3.0f;
    posA = posB;
    for (s32 i = 0; i < 3; i++) {
        posB.x = posA.x + directions[i].x * lineLength;
        posB.y = posA.y + directions[i].y * lineLength;
        posB.z = posA.z + directions[i].z * lineLength;
        if (Chu_IsOnCollisionPoly(play, &posA, &posB, &posSide, &polySide, &bgIdSide)) {
            self->shouldRotateOntoSurfaces |= Chu_UpdateFloorPoly(self, polySide, play);
            self->actor.world.pos = posSide;
            self->actor.floorBgId = bgIdSide;
            return true;
        }
    }
    return false;
}

static void RotateAxisAroundY(Vec3f* axis, f32 sin, f32 cos) {
    f32 x = axis->x;

    axis->x = sin * axis->z + cos * x;
    axis->z = cos * axis->z - sin * x;
}

static void Chu_RideDynaPoly(RemainsAllyChu* self, PlayState* play) {
    s16 yaw = self->actor.shape.rot.y;

    func_800433A4(&play->colCtx, self->actor.floorBgId, &self->actor);
    if (yaw == self->actor.shape.rot.y) {
        return;
    }
    yaw = self->actor.shape.rot.y - yaw;
    f32 sin = Math_SinS(yaw);
    f32 cos = Math_CosS(yaw);
    RotateAxisAroundY(&self->axisForwards, sin, cos);
    RotateAxisAroundY(&self->axisUp, sin, cos);
    RotateAxisAroundY(&self->axisLeft, sin, cos);
}

// Turns a little toward the goal each frame, the right way round even upside down on a ceiling.
static void Chu_SteerToward(RemainsAllyChu* self, Actor* goal) {
    Vec3f newAxisForwards;
    s16 angle = Actor_WorldYawTowardActor(&self->actor, goal) - self->actor.shape.rot.y;

    if (self->axisUp.y < -0.25f) {
        angle -= 0x8000;
    }
    angle = CLAMP(angle, -0x800, 0x800);
    Matrix_RotateAxis(BINANG_TO_RAD(angle), &self->axisUp, MTXMODE_NEW);
    Matrix_MultVec3f(&self->axisForwards, &newAxisForwards);
    self->axisForwards = newAxisForwards;
    CrossProduct(&self->axisUp, &self->axisForwards, &self->axisLeft);
    self->shouldRotateOntoSurfaces = true;
}

static void Chu_PlayRunSfx(RemainsAllyChu* self) {
    if (self->isModelReady && Animation_OnFrame(&self->skelAnime, 0.0f)) {
        Remains_PlayMmSfx(MM_SE_EN_BOMCHU_WALK, &self->actor.projectedPos);
        if (self->animLoopCounter != 0) {
            self->animLoopCounter--;
        }
    }
    if (self->animLoopCounter == 0 && Rand_ZeroOne() < 0.05f) {
        Remains_PlayMmSfx(MM_SE_EN_BOMCHU_AIM, &self->actor.projectedPos);
        self->animLoopCounter = 5;
    }
    if (self->target != NULL && (self->timer % CHU_RUN_SFX_CADENCE) == 0) {
        Remains_PlayMmSfx(MM_SE_EN_BOMCHU_RUN, &self->actor.projectedPos);
    }
}

static void Chu_Init(Actor* thisx, PlayState* play) {
    RemainsAllyChu* self = (RemainsAllyChu*)thisx;

    Actor_SetScale(thisx, CHU_SCALE);
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 25.0f);
    Chu_TryLoadModel(self, play);
    self->timer = CHU_FUSE;
    self->animLoopCounter = 5;
    self->axisForwards = (Vec3f){ Math_SinS(thisx->shape.rot.y), 0.0f, Math_CosS(thisx->shape.rot.y) };
    self->axisUp = (Vec3f){ 0.0f, 1.0f, 0.0f };
    self->axisLeft = (Vec3f){ Math_SinS(thisx->shape.rot.y + 0x4000), 0.0f, Math_CosS(thisx->shape.rot.y + 0x4000) };
    Chu_UpdateRotation(self);
    thisx->speedXZ = CHU_SPEED;
    sActiveChu = self;
}

static void Chu_Destroy(Actor* thisx, PlayState* play) {
    if (sActiveChu == (RemainsAllyChu*)thisx) {
        sActiveChu = NULL;
    }
}

// With no foe it runs straight ahead and climbs whatever it meets, like a live Real Bombchu.
static void Chu_Update(Actor* thisx, PlayState* play) {
    RemainsAllyChu* self = (RemainsAllyChu*)thisx;

    if (self->hasDetonated) {
        Actor_Kill(thisx);
        return;
    }
    self->shouldRotateOntoSurfaces = false;
    if (Chu_TryLoadModel(self, play)) {
        SkelAnime_Update(&self->skelAnime);
    }
    self->target = RemainsAlly_FindNearestEnemy(play, &thisx->world.pos, CHU_SEEK_RANGE);
    if (self->timer > 0) {
        self->timer--;
    }
    if (self->timer == 0) {
        Chu_Detonate(self, play);
        return;
    }
    thisx->speedXZ = CHU_SPEED;
    Chu_PlayRunSfx(self);

    if (thisx->floorBgId != BGCHECK_SCENE) {
        Chu_RideDynaPoly(self, play);
    }
    if (self->target != NULL) {
        Chu_SteerToward(self, self->target);
    }
    if (!Chu_IsTouchingSurface(self, play)) {
        Chu_Detonate(self, play);
        return;
    }
    if (self->shouldRotateOntoSurfaces) {
        Chu_UpdateRotation(self);
        thisx->shape.rot.x = -thisx->world.rot.x;
        thisx->shape.rot.y = thisx->world.rot.y;
        thisx->shape.rot.z = thisx->world.rot.z;
    }
    Actor_MoveXYZ(thisx);
    thisx->floorHeight = thisx->world.pos.y;
    if (SurfaceType_IsWallDamage(&play->colCtx, thisx->floorPoly, thisx->floorBgId)) {
        Chu_Detonate(self, play);
        return;
    }
    if (self->target != NULL &&
        Actor_WorldDistXZToActor(thisx, self->target) < CHU_HIT_DIST + (f32)self->target->colChkInfo.cylRadius) {
        Chu_Detonate(self, play);
        return;
    }
    Actor_SetFocus(thisx, thisx->shape.yOffset * 0.015f);
}

static s32 Chu_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    if (limbIndex == CHU_LIMB_TAIL_END) {
        *dList = NULL;
    }
    return false;
}

// The tail end is the bomb, flashing red faster as the fuse runs out (En_Rat's cadence).
static void Chu_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    RemainsAllyChu* self = (RemainsAllyChu*)thisx;

    if (limbIndex != CHU_LIMB_TAIL_END) {
        return;
    }
    s32 period = (self->timer >= 120) ? 30 : (self->timer >= 30) ? 6 : 3;
    f32 red = 1.0f - fabsf(Math_CosF((self->timer % period) * ((f32)M_PI / (f32)period)));

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBombCapDL);
    gDPSetEnvColor(POLY_OPA_DISP++, (s32)(red * 255.0f), 0, 40, 255);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, (s32)(red * 255.0f), 0, 40, 255);
    Matrix_RotateZYX(0x4000, 0, 0, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBombBodyDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Chu_Draw(Actor* thisx, PlayState* play) {
    RemainsAllyChu* self = (RemainsAllyChu*)thisx;

    if (!self->isModelReady) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sSegment0xCNoop);
    CLOSE_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    func_8002EBCC(thisx, play, 0);
    SkelAnime_DrawFlexOpa(play, self->skelAnime.skeleton, self->skelAnime.jointTable, self->skelAnime.dListCount,
                          Chu_OverrideLimbDraw, Chu_PostLimbDraw, thisx);
}

Actor* RemainsAllyChu_Spawn(PlayState* play, Vec3f* pos, s16 rotY) {
    if (sChuId < 0) {
        return NULL;
    }
    return Actor_Spawn(&play->actorCtx, play, sChuId, pos->x, pos->y, pos->z, 0, rotY, 0, 0);
}

// ── Gyorg's fish: the piranha of the Gyorg fight, flopping on land, a school in water ──

#define FISH_LIMB_ROOT 0x01
#define FISH_LIMB_TRUNK_ROOT 0x03
#define FISH_LIMB_TAIL_FIN 0x04
#define FISH_LIMB_MAX 0x0A
#define FISH_SCALE 0.018f
#define FISH_ATTACK_RANGE 220.0f
#define FISH_FOLLOW_DIST 42.0f
#define FISH_SWIM_SPEED 4.0f
#define FISH_LINK_MOVE_SPEED 2.5f
// Has to beat Link's swim speed of about 9, or the school falls behind for good.
#define FISH_CHASE_SPEED 14.0f
#define FISH_LUNGE_SPEED 13.0f
#define FISH_IDLE_ATTACK_RANGE 240.0f
#define FISH_BALL_DIST 55.0f
#define FISH_BALL_SPACING 30.0f
#define FISH_SEPARATION 30.0f
#define FISH_SUBMERGE_MARGIN 25.0f
#define FISH_FLOOR_MARGIN 8.0f
#define FISH_TARGET_Y_OFFSET 10.0f
#define FISH_Y_STEP 6.0f
#define FISH_PITCH_REF 200.0f
#define FISH_WIGGLE_SLOW 0x1F40
#define FISH_WIGGLE_FAST 0x4E20
#define FISH_WIGGLE_AMPL 5000.0f
#define FISH_MAX_OUT_OF_WATER 85
#define FISH_FLOP_BITE_RANGE 22
#define FISH_SWIM_BITE_RANGE 12

static const char sFishSkel[] = "__OTR__objects/object_boss03/gGyorgSmallFishSkel";
static const char sFishSwimAnim[] = "__OTR__objects/object_boss03/gGyorgSmallFishSwimAnim";

typedef struct {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[FISH_LIMB_MAX];
    Vec3s morphTable[FISH_LIMB_MAX];
    ColliderCylinder collider;
    f32 waterSurfaceY;
    s16 timer;
    s16 outOfWaterTimer;
    s32 wiggleAngle;
    s32 wiggleSpeed;
    s16 trunkRotation;
    s16 tailRotation;
    s16 bodyRotation;
    u8 isModelReady;
} RemainsAllyFish;

static ColliderCylinderInit sFishColliderInit = {
    { COLTYPE_HIT3, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK3,
      { DMG_DEKU_STICK, 0x00, 0x02 },
      { 0xF7CFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { FISH_SWIM_BITE_RANGE, 16, -8, { 0, 0, 0 } },
};

static bool Fish_TryLoadModel(RemainsAllyFish* self, PlayState* play) {
    if (self->isModelReady || !HasResources(sFishSkel, sFishSwimAnim)) {
        return self->isModelReady;
    }
    SkelAnime_InitFlex(play, &self->skelAnime, (FlexSkeletonHeader*)sFishSkel, (AnimationHeader*)sFishSwimAnim,
                       self->jointTable, self->morphTable, FISH_LIMB_MAX);
    Animation_MorphToLoop(&self->skelAnime, (AnimationHeader*)sFishSwimAnim, -10.0f);
    self->isModelReady = true;
    return true;
}

static void Fish_Init(Actor* thisx, PlayState* play) {
    RemainsAllyFish* self = (RemainsAllyFish*)thisx;
    WaterBox* waterBox;
    f32 surfaceY = thisx->world.pos.y;

    thisx->gravity = 0.0f;
    thisx->minVelocityY = 0.0f;
    thisx->shape.shadowDraw = ActorShadow_DrawCircle;
    thisx->shape.shadowScale = 20.0f;
    Collider_InitCylinder(play, &self->collider);
    Collider_SetCylinder(play, &self->collider, thisx, &sFishColliderInit);
    Fish_TryLoadModel(self, play);
    Actor_SetScale(thisx, FISH_SCALE);
    WaterBox_GetSurface1(play, &play->colCtx, thisx->world.pos.x, thisx->world.pos.z, &surfaceY, &waterBox);
    self->waterSurfaceY = surfaceY;
    self->wiggleAngle = (s32)Rand_ZeroFloat(50000.0f) + thisx->params * 0x1000;
    self->wiggleSpeed = FISH_WIGGLE_SLOW;
}

static void Fish_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((RemainsAllyFish*)thisx)->collider);
}

static void Fish_Bite(RemainsAllyFish* self, PlayState* play, s16 radius) {
    self->collider.dim.radius = radius;
    Collider_UpdateCylinder(&self->actor, &self->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &self->collider.base);
}

// Stays under the surface and above the floor; returns the climb so the nose can pitch to match.
static f32 Fish_ClampSwimHeight(RemainsAllyFish* self, f32 desiredY) {
    f32 floorY = self->actor.floorHeight;

    desiredY = MIN(desiredY, self->waterSurfaceY - FISH_SUBMERGE_MARGIN);
    if (floorY > BGCHECK_Y_MIN && desiredY < floorY + FISH_FLOOR_MARGIN) {
        desiredY = floorY + FISH_FLOOR_MARGIN;
    }
    f32 yDelta = desiredY - self->actor.world.pos.y;
    Math_ApproachF(&self->actor.world.pos.y, desiredY, 0.5f, FISH_Y_STEP);
    return yDelta;
}

// The school trails Link as a ball and only lunges, all at the same foe, while he holds still.
static void Fish_SwimInWater(RemainsAllyFish* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f anchor = BallAnchorBehind(player, (u32)self->actor.params, FISH_BALL_DIST, FISH_BALL_SPACING);
    bool isLinkMoving = player->linearVelocity > FISH_LINK_MOVE_SPEED;
    Actor* target =
        isLinkMoving ? NULL : RemainsAlly_FindNearestEnemy(play, &player->actor.world.pos, FISH_IDLE_ATTACK_RANGE);
    f32 desiredY;

    self->actor.gravity = 0.0f;
    self->actor.minVelocityY = 0.0f;
    self->actor.velocity.y = 0.0f;
    self->outOfWaterTimer = 0;
    SeparateFromKin(&self->actor, play, sFishId, FISH_SEPARATION, NULL);
    if (target != NULL) {
        HomeTowardPos(play, &self->actor, &target->world.pos, FISH_LUNGE_SPEED);
        desiredY = target->world.pos.y + FISH_TARGET_Y_OFFSET;
        self->wiggleSpeed = FISH_WIGGLE_FAST;
        if (!(self->timer & 0xF) && Rand_ZeroOne() < 0.5f) {
            Remains_PlayMmSfx(MM_SE_EN_PIRANHA_ATTACK, &self->actor.projectedPos);
        }
    } else {
        f32 distance = Math_Vec3f_DistXZ(&self->actor.world.pos, &anchor);
        f32 speed = (distance > FISH_FOLLOW_DIST) ? FISH_CHASE_SPEED : (distance > 10.0f) ? FISH_SWIM_SPEED : 0.0f;
        HomeTowardPos(play, &self->actor, &anchor, speed);
        desiredY = player->actor.world.pos.y + FISH_TARGET_Y_OFFSET;
        self->wiggleSpeed = (speed > FISH_SWIM_SPEED) ? FISH_WIGGLE_FAST : FISH_WIGGLE_SLOW;
    }
    f32 yDelta = Fish_ClampSwimHeight(self, desiredY);
    Math_ApproachS(&self->actor.shape.rot.x, Math_Atan2S(FISH_PITCH_REF, -yDelta), 4, 0x800);
    Fish_Bite(self, play, FISH_SWIM_BITE_RANGE);
}

// Beached, it hops at random, only vaguely toward a foe, and bites only what it lands right next to.
static void Fish_FlopOnLand(RemainsAllyFish* self, PlayState* play) {
    self->outOfWaterTimer++;
    self->actor.gravity = -1.8f;
    self->actor.minVelocityY = -22.0f;
    self->wiggleSpeed = FISH_WIGGLE_FAST;
    if (self->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Actor* target = RemainsAlly_FindNearestEnemy(play, &self->actor.world.pos, FISH_ATTACK_RANGE);
        if (target != NULL) {
            self->actor.world.rot.y =
                Math_Vec3f_Yaw(&self->actor.world.pos, &target->world.pos) + (s16)Rand_CenteredFloat(0x5000);
        } else {
            self->actor.world.rot.y += (s16)Rand_CenteredFloat(0x8000);
        }
        self->actor.velocity.y = Rand_ZeroFloat(4.0f) + 4.0f;
        self->actor.speedXZ = Rand_ZeroFloat(3.0f) + 2.0f;
        if (!(self->timer & 0x7)) {
            Remains_PlayMmSfx(MM_SE_EN_PIRANHA_ATTACK, &self->actor.projectedPos);
        }
    }
    Actor_MoveXZGravity(&self->actor);
    Actor_UpdateBgCheckInfo(play, &self->actor, 20.0f, 10.0f, 0.0f, BGCHECK_WALL_AND_FLOOR);
    self->actor.shape.rot.y = self->actor.world.rot.y;
    Math_ApproachS(&self->actor.shape.rot.x, 0x2000, 4, 0x800);
    Fish_Bite(self, play, FISH_FLOP_BITE_RANGE);
}

static void Fish_Update(Actor* thisx, PlayState* play) {
    RemainsAllyFish* self = (RemainsAllyFish*)thisx;
    WaterBox* waterBox;
    f32 surfaceY = thisx->world.pos.y;

    self->timer++;
    Fish_TryLoadModel(self, play);
    // A fish spends itself on the bite that lands.
    if (self->collider.base.atFlags & AT_HIT) {
        Remains_PlayMmSfx(MM_SE_EN_PIRANHA_ATTACK, &thisx->projectedPos);
        Actor_Kill(thisx);
        return;
    }
    bool hasWater = WaterBox_GetSurface1(play, &play->colCtx, thisx->world.pos.x, thisx->world.pos.z, &surfaceY,
                                         &waterBox);
    if (hasWater) {
        self->waterSurfaceY = surfaceY;
    }
    if (hasWater && thisx->world.pos.y < surfaceY) {
        Fish_SwimInWater(self, play);
    } else {
        Fish_FlopOnLand(self, play);
        if (self->outOfWaterTimer > FISH_MAX_OUT_OF_WATER) {
            Actor_Kill(thisx);
            return;
        }
    }
    if (self->isModelReady) {
        SkelAnime_Update(&self->skelAnime);
    }
    self->wiggleAngle += self->wiggleSpeed;
    self->tailRotation = Math_SinS(self->wiggleAngle) * FISH_WIGGLE_AMPL;
    self->bodyRotation = Math_SinS(self->wiggleAngle + 0x6978) * FISH_WIGGLE_AMPL;
    self->trunkRotation = Math_SinS(self->wiggleAngle) * FISH_WIGGLE_AMPL;
}

// En_Tanron3's procedural sway on the same three limbs.
static s32 Fish_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    RemainsAllyFish* self = (RemainsAllyFish*)thisx;

    if (limbIndex == FISH_LIMB_ROOT) {
        rot->y += self->bodyRotation;
    } else if (limbIndex == FISH_LIMB_TRUNK_ROOT) {
        rot->y += self->trunkRotation;
    } else if (limbIndex == FISH_LIMB_TAIL_FIN) {
        rot->y += self->tailRotation;
    }
    return false;
}

static void Fish_Draw(Actor* thisx, PlayState* play) {
    RemainsAllyFish* self = (RemainsAllyFish*)thisx;

    if (!self->isModelReady) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sSegment0xCNoop);
    CLOSE_DISPS(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, self->skelAnime.skeleton, self->skelAnime.jointTable, self->skelAnime.dListCount,
                          Fish_OverrideLimbDraw, NULL, thisx);
}

Actor* RemainsAllyFish_Spawn(PlayState* play, Vec3f* pos, s16 rotY, s16 index) {
    if (sFishId < 0) {
        return NULL;
    }
    return Actor_Spawn(&play->actorCtx, play, sFishId, pos->x, pos->y, pos->z, 0, rotY, 0, index);
}

// ── Registration ────────────────────────────────────────────────────────────

// ACTORCAT_MISC keeps the allies out of enemy counts, room clears and battle music, and out of their own targeting.
static s16 RegisterAlly(const char* key, const char* description, uint32_t size, SOHActorFunc init,
                        SOHActorFunc destroy, SOHActorFunc update, SOHActorFunc draw) {
    SOHActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = key;
    definition.description = description;
    definition.category = ACTORCAT_MISC;
    definition.actorFlags = ALLY_FLAGS;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = size;
    definition.init = init;
    definition.destroy = destroy;
    definition.update = update;
    definition.draw = draw;
    return sApi->RegisterActor(&definition);
}

bool RemainsAllies_Register(const SOHModApi* api) {
    sApi = api;
    sBugId = RegisterAlly("nei.remains_ally_bug", "Odolwa's remains: beetle, moth and Goht's thunder",
                          sizeof(RemainsAllyBug), Bug_Init, Bug_Destroy, Bug_Update, Bug_Draw);
    sChuId = RegisterAlly("nei.remains_ally_chu", "Goht's remains: friendly Real Bombchu", sizeof(RemainsAllyChu),
                          Chu_Init, Chu_Destroy, Chu_Update, Chu_Draw);
    sFishId = RegisterAlly("nei.remains_ally_fish", "Gyorg's remains: piranha school", sizeof(RemainsAllyFish),
                           Fish_Init, Fish_Destroy, Fish_Update, Fish_Draw);
    return sBugId >= 0 && sChuId >= 0 && sFishId >= 0;
}
