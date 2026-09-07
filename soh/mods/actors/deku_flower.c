/**
 * deku_flower.c — Majora's Mask's Deku Flower (Obj_Etcetera), ported for the Rod of Seasons: in
 * Summer a bean spot is a gold flower Deku Link burrows into and is shot out of. Skijer's NEI
 *
 * Built as C inside season_scene.cpp's extern "C" block, so no `this`. Assets come from mm.o2r
 * under their MM-unique names. MM read the player's stateFlags3 to react; here the Deku form pushes
 * the launch (DekuFlower_OnLaunch) and answers the two polls declared below.
 */
#include "deku_flower.h"
#include "align_asset_macro.h"
#include "mods/transformation_masks/assets/mm_asset_loader.h"

u8 MmForm_DekuBurrowStarting(void);
u8 MmForm_DekuGoldenFlight(void);

#define DEKU_FLOWER_LIMB_MAX 11
#define DEKU_FLOWER_REST_SCALE 0.01f

typedef struct DekuFlower DekuFlower;
typedef void (*DekuFlowerActionFunc)(DekuFlower*, PlayState*);

struct DekuFlower {
    DynaPolyActor dyna;
    SkelAnime skelAnime;
    ColliderCylinder collider;
    Vec3s jointTable[DEKU_FLOWER_LIMB_MAX];
    Vec3s morphTable[DEKU_FLOWER_LIMB_MAX];
    f32 bounceOscillationScale;
    s16 oscillationTimer;
    u8 playerOnTop;
    Gfx* idleDL;
    DekuFlowerActionFunc actionFunc;
};

s16 gDekuFlowerId = -1;
size_t gDekuFlowerStructSize = sizeof(DekuFlower);

static const ALIGN_ASSET(2) char sDekuFlowerBounceAnim[] = "__OTR__objects/gameplay_keep/gDekuFlowerBounceAnim";
static const ALIGN_ASSET(2) char sDekuFlowerRustleAnim[] = "__OTR__objects/gameplay_keep/gDekuFlowerRustleAnim";
static const ALIGN_ASSET(2) char sPinkDekuFlowerIdleDL[] = "__OTR__objects/gameplay_keep/gPinkDekuFlowerIdleDL";
static const ALIGN_ASSET(2) char sGoldDekuFlowerIdleDL[] = "__OTR__objects/gameplay_keep/gGoldDekuFlowerIdleDL";
static const ALIGN_ASSET(2) char sPinkDekuFlowerCol[] = "__OTR__objects/gameplay_keep/gPinkDekuFlowerCol";
static const ALIGN_ASSET(2) char sGoldDekuFlowerCol[] = "__OTR__objects/gameplay_keep/gGoldDekuFlowerCol";
static const char* const sDekuFlowerSkel[2] = { "objects/gameplay_keep/gPinkDekuFlowerSkel",
                                                "objects/gameplay_keep/gGoldDekuFlowerSkel" };
static const char* const sDekuFlowerIdleDL[2] = { sPinkDekuFlowerIdleDL, sGoldDekuFlowerIdleDL };
static const char* const sDekuFlowerCol[2] = { sPinkDekuFlowerCol, sGoldDekuFlowerCol };

static ColliderCylinderInit sDekuFlowerCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0x00000000, 0x00, 0x00 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_NONE | TOUCH_SFX_NORMAL,
      BUMP_ON,
      OCELEM_NONE },
    { 20, 14, 0, { 0, 0, 0 } },
};

// Scales the X/Z wobble of most interactions, indexed by frame.
static const f32 sDekuFlowerOscillation[18] = {
    -1.0f, -1.0f, -1.0f, -0.7f, 0.0f, 0.7f, 1.0f, 0.7f, 0.0f, -0.7f, -1.0f, -0.7f, 0.0f, 0.7f, 1.0f, 0.7f, 0.0f, -0.7f,
};

static void DekuFlower_Idle(DekuFlower* self, PlayState* play);
static void DekuFlower_Rustle(DekuFlower* self, PlayState* play);
static void DekuFlower_Bounce(DekuFlower* self, PlayState* play);
static void DekuFlower_DrawIdle(Actor* thisx, PlayState* play);
static void DekuFlower_DrawAnimated(Actor* thisx, PlayState* play);

static void DekuFlower_SetRestScale(DekuFlower* self) {
    Actor_SetScale(&self->dyna.actor, DEKU_FLOWER_REST_SCALE);
    self->dyna.actor.scale.y = 2.0f * DEKU_FLOWER_REST_SCALE;
}

static void DekuFlower_SetIdle(DekuFlower* self) {
    self->dyna.actor.draw = DekuFlower_DrawIdle;
    self->actionFunc = DekuFlower_Idle;
    DekuFlower_SetRestScale(self);
}

static void DekuFlower_StartRustle(DekuFlower* self) {
    Animation_Change(&self->skelAnime, (AnimationHeader*)sDekuFlowerRustleAnim, 1.0f, 0.0f,
                     Animation_GetLastFrame((void*)sDekuFlowerRustleAnim), ANIMMODE_ONCE, 0.0f);
    self->dyna.actor.draw = DekuFlower_DrawAnimated;
    self->actionFunc = DekuFlower_Rustle;
    self->oscillationTimer = 10;
}

static void DekuFlower_StartBounce(DekuFlower* self, f32 bounceScale) {
    Animation_Change(&self->skelAnime, (AnimationHeader*)sDekuFlowerBounceAnim, 1.0f, 0.0f,
                     Animation_GetLastFrame((void*)sDekuFlowerBounceAnim), ANIMMODE_ONCE, 0.0f);
    self->dyna.actor.draw = DekuFlower_DrawAnimated;
    self->actionFunc = DekuFlower_Bounce;
    self->oscillationTimer = 30;
    self->bounceOscillationScale = bounceScale;
}

void DekuFlower_Init(Actor* thisx, PlayState* play) {
    DekuFlower* self = (DekuFlower*)thisx;
    s32 type = DEKU_FLOWER_TYPE(thisx->params);

    if (type >= DEKU_FLOWER_TYPE_MAX) {
        type = DEKU_FLOWER_TYPE_PINK;
    }
    u8 gold = (type >= DEKU_FLOWER_TYPE_GOLD);

    // Destroy runs even on an actor killed from Init, so both handles exist before the bail-out.
    DynaPolyActor_Init(&self->dyna, DPM_PLAYER);
    Collider_InitCylinder(play, &self->collider);

    SkeletonHeader* skeleton = (SkeletonHeader*)MmAssets_LoadSkeleton(sDekuFlowerSkel[gold]);

    if (skeleton == NULL) {
        Actor_Kill(thisx);
        return;
    }

    Vec3f probe = thisx->world.pos;
    s32 floorBgId;

    probe.y += 10.0f;
    BgCheck_EntityRaycastFloor5(play, &play->colCtx, &thisx->floorPoly, &floorBgId, thisx, &probe);
    thisx->floorBgId = floorBgId;

    Collider_SetCylinder(play, &self->collider, thisx, &sDekuFlowerCylinderInit);
    if (gold) {
        self->collider.dim.height = 20;
    }
    Collider_UpdateCylinder(thisx, &self->collider);

    CollisionHeader* colHeader = NULL;

    CollisionHeader_GetVirtual((void*)sDekuFlowerCol[gold], &colHeader);
    self->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);

    SkelAnime_Init(play, &self->skelAnime, skeleton, (AnimationHeader*)sDekuFlowerBounceAnim, self->jointTable,
                   self->morphTable, DEKU_FLOWER_LIMB_MAX);
    self->idleDL = (Gfx*)sDekuFlowerIdleDL[gold];
    thisx->focus.pos.y = thisx->home.pos.y + 10.0f;
    thisx->targetMode = 3;
    self->playerOnTop = 0;

    if (type & 1) {
        DekuFlower_StartBounce(self, 0.0f);
        Actor_SetScale(thisx, 0.0f);
    } else {
        DekuFlower_SetIdle(self);
    }
}

void DekuFlower_Destroy(Actor* thisx, PlayState* play) {
    DekuFlower* self = (DekuFlower*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, self->dyna.bgId);
    Collider_DestroyCylinder(play, &self->collider);
}

static void DekuFlower_Oscillate(DekuFlower* self, PlayState* play) {
    if (self->oscillationTimer <= 0) {
        DekuFlower_SetRestScale(self);
        return;
    }
    f32 wobble = sDekuFlowerOscillation[play->gameplayFrames % 18] * (0.0001f * self->oscillationTimer);

    Actor_SetScale(&self->dyna.actor, DEKU_FLOWER_REST_SCALE + wobble);
    self->dyna.actor.scale.y = 2.0f * DEKU_FLOWER_REST_SCALE;
    self->oscillationTimer--;
}

static void DekuFlower_Idle(DekuFlower* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* actor = &self->dyna.actor;
    u8 onTop = DynaPolyActor_IsPlayerOnTop(&self->dyna);

    if (MmForm_DekuGoldenFlight() && (actor->xzDistToPlayer < 30.0f) && (actor->yDistToPlayer > 0.0f)) {
        // Hovering above: the wobble follows how close he is.
        s16 minTimer = 10 - (s32)(actor->yDistToPlayer * 0.05f);

        if (self->oscillationTimer < minTimer) {
            self->oscillationTimer = minTimer;
        }
    } else if (onTop) {
        if (!self->playerOnTop) {
            DekuFlower_StartRustle(self);
        } else if ((player->actor.speedXZ > 0.1f) || MmForm_DekuBurrowStarting()) {
            self->oscillationTimer = 10;
        }
        self->playerOnTop = 1;
    } else {
        if (self->playerOnTop) {
            DekuFlower_StartRustle(self);
        }
        self->playerOnTop = 0;
    }

    if (self->collider.base.acFlags & AC_HIT) {
        self->collider.base.acFlags &= ~AC_HIT;
        DekuFlower_StartRustle(self);
    }
    DekuFlower_Oscillate(self, play);
}

static void DekuFlower_Rustle(DekuFlower* self, PlayState* play) {
    self->playerOnTop = DynaPolyActor_IsPlayerOnTop(&self->dyna);
    if (SkelAnime_Update(&self->skelAnime)) {
        self->dyna.actor.draw = DekuFlower_DrawIdle;
        self->actionFunc = DekuFlower_Idle;
    }
    DekuFlower_Oscillate(self, play);
}

// Stronger than the idle wobble, and on Y too: after a launch, or on a flower that pops in.
static void DekuFlower_Bounce(DekuFlower* self, PlayState* play) {
    self->playerOnTop = DynaPolyActor_IsPlayerOnTop(&self->dyna);
    SkelAnime_Update(&self->skelAnime);

    if (self->oscillationTimer <= 0) {
        DekuFlower_SetIdle(self);
        self->bounceOscillationScale = 0.0f;
        return;
    }
    self->oscillationTimer--;
    self->bounceOscillationScale *= 0.8f;
    self->bounceOscillationScale -= (self->dyna.actor.scale.x - DEKU_FLOWER_REST_SCALE) * 0.4f;

    f32 scale = self->dyna.actor.scale.x + self->bounceOscillationScale;

    Actor_SetScale(&self->dyna.actor, scale);
    self->dyna.actor.scale.y = 2.0f * scale;
}

void DekuFlower_Update(Actor* thisx, PlayState* play) {
    DekuFlower* self = (DekuFlower*)thisx;

    self->actionFunc(self, play);
    CollisionCheck_SetAC(play, &play->colChkCtx, &self->collider.base);
}

static void DekuFlower_DrawIdle(Actor* thisx, PlayState* play) {
    DekuFlower* self = (DekuFlower*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, self->idleDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DekuFlower_DrawAnimated(Actor* thisx, PlayState* play) {
    DekuFlower* self = (DekuFlower*)thisx;

    Gfx_SetupDL_37Opa(play->state.gfxCtx);
    SkelAnime_DrawOpa(play, self->skelAnime.skeleton, self->skelAnime.jointTable, NULL, NULL, thisx);
}

DynaPolyActor* DekuFlower_Underfoot(PlayState* play, Actor* actor) {
    if ((gDekuFlowerId < 0) || (actor->floorBgId == BGCHECK_SCENE)) {
        return NULL;
    }
    DynaPolyActor* dyna = DynaPoly_GetActor(&play->colCtx, actor->floorBgId);

    if ((dyna == NULL) || (dyna->actor.id != gDekuFlowerId)) {
        return NULL;
    }
    return dyna;
}

u8 DekuFlower_IsGold(DynaPolyActor* flower) {
    return DEKU_FLOWER_TYPE(flower->actor.params) >= DEKU_FLOWER_TYPE_GOLD;
}

void DekuFlower_OnLaunch(DynaPolyActor* flower) {
    DekuFlower* self = (DekuFlower*)flower;

    DekuFlower_StartBounce(self, 0.003f);
    DekuFlower_SetRestScale(self);
    self->playerOnTop = 0;
}
