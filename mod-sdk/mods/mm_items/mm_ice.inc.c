// Majora's Mask ice magic in Ocarina of Time: anything carrying ice arrow damage that enters water raises MM's
// Bg_Icefloe platform, and an Octorok it hits freezes inside a floating iceberg Link can stand on, as Obj_Iceblock
// encases MM's. Arrows, the Water Medallion's shots and the Ice Rod all carry that damage.

#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_En_Okuta/z_en_okuta.h"

#define ICE_PLATFORM_KEY "mm.icefloe"
#define ICE_BERG_KEY "mm.frozen_octorok"
#define ICE_PLATFORM_MAX 3
// Bg_Icefloe: 20 frames growing by 0.0065, then afloat for its params (the arrow passes 300), 50 frames melting.
#define ICE_PLATFORM_GROW_FRAMES 20
#define ICE_PLATFORM_GROW_STEP 0.0065f
#define ICE_PLATFORM_FLOAT_FRAMES 300
#define ICE_MELT_FRAMES 50
#define ICE_PLATFORM_RISE 10.0f
// Obj_Iceblock: the block reaches scale 0.1 in steps of 0.02 and melts after 450 frames.
#define ICE_BERG_SCALE 0.1f
#define ICE_BERG_GROW_STEP 0.02f
#define ICE_BERG_FLOAT_FRAMES 450
#define ICE_BERG_SINK 20.0f

typedef enum {
    ICE_PHASE_GROW,
    ICE_PHASE_FLOAT,
    ICE_PHASE_MELT,
} IcePhase;

typedef struct {
    DynaPolyActor dyna;
    s16 timer;
    u8 phase;
} MmIce;

static const ALIGN_ASSET(2) char sIcePlatformDL[] = "__OTR__objects/object_icefloe/gIcefloeIcePlatformDL";
static const ALIGN_ASSET(2) char sIcePlatformCol[] = "__OTR__objects/object_icefloe/gIcefloePlatformCol";
static const ALIGN_ASSET(2) char sIceBergDL[] = "__OTR__objects/object_ice_block/gIceBlockIceBergDL";
static const ALIGN_ASSET(2) char sIceBergCol[] = "__OTR__objects/object_ice_block/gIceBlockCol";
// The iceberg calls segment 8 for its sublimating-air scroll, which Obj_Iceblock fills with an AnimatedMat.
static Gfx sEmptyDL[] = { gsSPEndDisplayList() };

static s16 sIcePlatformId = -1;
static s16 sIceBergId = -1;
static MmIce* sIcePlatforms[ICE_PLATFORM_MAX];

static bool IsIceDamage(u32 dmgFlags) {
    return (dmgFlags & DMG_ARROW_ICE) != 0;
}

static void LoadIceMesh(PlayState* play, MmIce* ice, const char* collisionPath) {
    CollisionHeader* header = NULL;

    DynaPolyActor_Init(&ice->dyna, DPM_PLAYER);
    CollisionHeader_GetVirtual((void*)collisionPath, &header);
    ice->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &ice->dyna.actor, header);
}

static void StartIceMelt(MmIce* ice) {
    ice->phase = ICE_PHASE_MELT;
    ice->timer = ICE_MELT_FRAMES;
    Audio_PlayActorSound2(&ice->dyna.actor, NA_SE_EV_ICE_MELT);
}

// Bg_Icefloe_Init keeps three platforms; a fourth sends the oldest melting.
static void TrackIcePlatform(MmIce* ice) {
    if (sIcePlatforms[ICE_PLATFORM_MAX - 1] == NULL) {
        for (u32 i = 0; i < ICE_PLATFORM_MAX; i++) {
            if (sIcePlatforms[i] == NULL) {
                sIcePlatforms[i] = ice;
                return;
            }
        }
    }
    if (sIcePlatforms[0]->dyna.actor.update != NULL && sIcePlatforms[0]->phase != ICE_PHASE_MELT) {
        StartIceMelt(sIcePlatforms[0]);
    }
    for (u32 i = 0; i < ICE_PLATFORM_MAX - 1; i++) {
        sIcePlatforms[i] = sIcePlatforms[i + 1];
    }
    sIcePlatforms[ICE_PLATFORM_MAX - 1] = ice;
}

static void ForgetIcePlatform(MmIce* ice) {
    for (u32 i = 0; i < ICE_PLATFORM_MAX; i++) {
        if (sIcePlatforms[i] == ice) {
            for (u32 j = i; j < ICE_PLATFORM_MAX - 1; j++) {
                sIcePlatforms[j] = sIcePlatforms[j + 1];
            }
            sIcePlatforms[ICE_PLATFORM_MAX - 1] = NULL;
            return;
        }
    }
}

static void IcePlatformInit(Actor* actor, PlayState* play) {
    MmIce* ice = (MmIce*)actor;

    Actor_SetScale(actor, 0.0f);
    LoadIceMesh(play, ice, sIcePlatformCol);
    TrackIcePlatform(ice);
    actor->world.pos.y = actor->home.pos.y + ICE_PLATFORM_RISE;
    ice->phase = ICE_PHASE_GROW;
    ice->timer = ICE_PLATFORM_GROW_FRAMES;
    Audio_PlayActorSound2(actor, NA_SE_EV_ICE_FREEZE);
}

static void IceDestroy(Actor* actor, PlayState* play) {
    MmIce* ice = (MmIce*)actor;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, ice->dyna.bgId);
    ForgetIcePlatform(ice);
}

static void SpawnIceShards(PlayState* play, Actor* actor, f32 height) {
    Vec3f position = { actor->world.pos.x + Rand_CenteredFloat(actor->scale.x * 450.0f),
                       actor->world.pos.y + height, actor->world.pos.z + Rand_CenteredFloat(actor->scale.z * 450.0f) };

    EffectSsIcePiece_SpawnBurst(play, &position, actor->scale.x * 4.0f);
}

static void SpawnIceSmoke(PlayState* play, Actor* actor, s16 timer) {
    Vec3f velocity = { Rand_CenteredFloat(1.5f), 0.8f + (timer - 38) * (1.0f / 12.0f), Rand_CenteredFloat(1.5f) };
    Vec3f position = { actor->world.pos.x + 2.0f * velocity.x, actor->world.pos.y + 3.0f,
                       actor->world.pos.z + 2.0f * velocity.z };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };

    EffectSsIceSmoke_Spawn(play, &position, &velocity, &accel, 200);
}

// Bg_Icefloe_Melt: smoke for the first twelve frames, then the slab thins away.
static bool MeltIce(PlayState* play, MmIce* ice, f32 widthStep, f32 heightStep) {
    Actor* actor = &ice->dyna.actor;

    ice->timer--;
    if (ice->timer >= 38 && (ice->timer % 2) == 0) {
        SpawnIceSmoke(play, actor, ice->timer);
    }
    if (ice->timer < 25) {
        actor->scale.x = MAX(actor->scale.x - widthStep, 0.0f);
        actor->scale.z = MAX(actor->scale.z - widthStep, 0.0f);
    }
    actor->scale.y -= heightStep;
    if (actor->scale.y > 0.0f) {
        return false;
    }
    Actor_Kill(actor);
    return true;
}

static bool FindWaterSurface(PlayState* play, Actor* actor, f32* surface) {
    WaterBox* waterBox;

    return WaterBox_GetSurface1(play, &play->colCtx, actor->world.pos.x, actor->world.pos.z, surface, &waterBox);
}

static void IcePlatformUpdate(Actor* actor, PlayState* play) {
    MmIce* ice = (MmIce*)actor;
    f32 surface;

    switch (ice->phase) {
        case ICE_PHASE_GROW:
            actor->scale.x += ICE_PLATFORM_GROW_STEP;
            actor->scale.y += ICE_PLATFORM_GROW_STEP;
            actor->scale.z += ICE_PLATFORM_GROW_STEP;
            SpawnIceShards(play, actor, 300.0f * actor->scale.y);
            if (--ice->timer == 0) {
                ice->phase = ICE_PHASE_FLOAT;
                ice->timer = ICE_PLATFORM_FLOAT_FRAMES;
            }
            break;
        case ICE_PHASE_FLOAT:
            if (--ice->timer == 0 || !FindWaterSurface(play, actor, &surface)) {
                StartIceMelt(ice);
                break;
            }
            actor->home.pos.y = surface;
            actor->world.pos.y = Math_SinF(ice->timer * (M_PI / 30.0f)) * 3.0f + surface + ICE_PLATFORM_RISE;
            break;
        case ICE_PHASE_MELT:
            MeltIce(play, ice, 0.0052f, 0.0026f);
            break;
        default:
            Actor_Kill(actor);
            break;
    }
}

static void IcePlatformDraw(Actor* actor, PlayState* play) {
    Gfx_DrawDListOpa(play, (Gfx*)sIcePlatformDL);
}

// The parent is the Octorok, kept still inside until the block is gone.
static void IceBergInit(Actor* actor, PlayState* play) {
    MmIce* ice = (MmIce*)actor;

    Actor_SetScale(actor, 0.01f);
    LoadIceMesh(play, ice, sIceBergCol);
    ice->phase = ICE_PHASE_GROW;
    Audio_PlayActorSound2(actor, NA_SE_EV_ICE_FREEZE);
}

static void IceBergUpdate(Actor* actor, PlayState* play) {
    MmIce* ice = (MmIce*)actor;
    f32 surface;

    if (FindWaterSurface(play, actor, &surface)) {
        actor->world.pos.y = surface - ICE_BERG_SINK + Math_SinF(play->gameplayFrames * (M_PI / 30.0f)) * 2.0f;
    }
    switch (ice->phase) {
        case ICE_PHASE_GROW:
            if (Math_StepToF(&actor->scale.x, ICE_BERG_SCALE, ICE_BERG_GROW_STEP)) {
                ice->phase = ICE_PHASE_FLOAT;
                ice->timer = ICE_BERG_FLOAT_FRAMES;
            }
            actor->scale.y = actor->scale.z = actor->scale.x;
            SpawnIceShards(play, actor, 0.0f);
            break;
        case ICE_PHASE_FLOAT:
            if (--ice->timer == 0) {
                StartIceMelt(ice);
            }
            break;
        case ICE_PHASE_MELT:
            MeltIce(play, ice, ICE_BERG_SCALE / ICE_MELT_FRAMES, ICE_BERG_SCALE / ICE_MELT_FRAMES);
            break;
        default:
            Actor_Kill(actor);
            break;
    }
}

static void IceBergDraw(Actor* actor, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sEmptyDL);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sIceBergDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s16 RegisterIceActor(const char* key, const char* description, SOHActorFunc init, SOHActorFunc update,
                            SOHActorFunc draw) {
    SOHActorDefinition actor = { 0 };

    actor.structSize = sizeof(actor);
    actor.key = key;
    actor.description = description;
    actor.category = ACTORCAT_BG;
    actor.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    actor.objectId = OBJECT_GAMEPLAY_KEEP;
    actor.instanceSize = sizeof(MmIce);
    actor.init = init;
    actor.destroy = IceDestroy;
    actor.update = update;
    actor.draw = draw;
    return sApi->RegisterActor(&actor);
}

// En_Arrow has no water handling in OoT, so a shot crossing the surface this frame is caught after its update.
static void FreezeWaterUnderShot(void* actorPtr) {
    Actor* actor = (Actor*)actorPtr;
    EnArrow* arrow = (EnArrow*)actor;
    f32 surface;

    if (!IsIceDamage(arrow->collider.info.toucher.dmgFlags) || actor->world.pos.y >= actor->prevPos.y ||
        !FindWaterSurface(gPlayState, actor, &surface) || actor->prevPos.y < surface ||
        actor->world.pos.y >= surface) {
        return;
    }
    f32 t = (surface - actor->prevPos.y) / (actor->world.pos.y - actor->prevPos.y);
    f32 x = actor->prevPos.x + t * (actor->world.pos.x - actor->prevPos.x);
    f32 z = actor->prevPos.z + t * (actor->world.pos.z - actor->prevPos.z);

    if (Actor_Spawn(&gPlayState->actorCtx, gPlayState, sIcePlatformId, x, surface, z, 0, 0, 0, 0) != NULL) {
        Actor_Kill(actor);
    }
}

static bool IsFrozenInIce(Actor* octorok) {
    return octorok->child != NULL && octorok->child->id == sIceBergId && octorok->child->update != NULL;
}

static void EncaseOctorok(EnOkuta* octorok) {
    Actor* actor = &octorok->actor;

    octorok->collider.base.acFlags &= ~AC_HIT;
    actor->colChkInfo.damage = 0;
    actor->colChkInfo.damageEffect = 0;
    Actor_SpawnAsChild(&gPlayState->actorCtx, actor, gPlayState, sIceBergId, actor->world.pos.x,
                       actor->world.pos.y, actor->world.pos.z, 0, actor->shape.rot.y, 0, 0);
    Audio_PlayActorSound2(actor, NA_SE_PL_FREEZE_S);
}

// Before the Octorok's own update reads the hit, which in OoT always kills it.
static void FreezeOctorok(void* actorPtr, bool* shouldUpdate) {
    EnOkuta* octorok = (EnOkuta*)actorPtr;
    Actor* actor = &octorok->actor;

    if (actor->params != 0) {
        return;
    }
    if (IsFrozenInIce(actor)) {
        *shouldUpdate = false;
        Actor_SetColorFilter(actor, 0, 0xFF, 0, 2);
        return;
    }
    if (!(octorok->collider.base.acFlags & AC_HIT) || octorok->collider.info.acHitInfo == NULL ||
        !IsIceDamage(octorok->collider.info.acHitInfo->toucher.dmgFlags)) {
        return;
    }
    EncaseOctorok(octorok);
    *shouldUpdate = !IsFrozenInIce(actor);
}

static void ForgetIcePlatforms(void) {
    for (u32 i = 0; i < ICE_PLATFORM_MAX; i++) {
        sIcePlatforms[i] = NULL;
    }
}

static void RegisterMmIce(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterActor) || !sApi->HasResource(sIcePlatformDL) ||
        !sApi->HasResource(sIceBergDL)) {
        return;
    }
    sIcePlatformId = RegisterIceActor(ICE_PLATFORM_KEY, "MM ice arrow platform", IcePlatformInit,
                                      IcePlatformUpdate, IcePlatformDraw);
    sIceBergId = RegisterIceActor(ICE_BERG_KEY, "MM frozen Octorok iceberg", IceBergInit, IceBergUpdate,
                                  IceBergDraw);
    if (sIcePlatformId < 0 || sIceBergId < 0) {
        return;
    }
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_ARROW, FreezeWaterUnderShot);
    SOH_REGISTER_HOOK_FOR_ID(sApi, ShouldActorUpdate, ACTOR_EN_OKUTA, FreezeOctorok);
}
