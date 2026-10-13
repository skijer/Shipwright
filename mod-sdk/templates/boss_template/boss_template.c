#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64boss.h"

#define GOLEM_BOSS_KEY "template.rupee_golem"
#define GOLEM_ACTOR_KEY "template.rupee_golem.actor"
#define GOLEM_HEALTH 6
#define GOLEM_SCALE 0.1f
#define GOLEM_MODEL_SCALE 25.0f
#define GOLEM_SPIN_SPEED 0x300
#define GOLEM_HURT_SPIN_SPEED 0x1800
#define GOLEM_HURT_FRAMES 20
#define GOLEM_HEART_OFFSET_Z 120.0f

typedef struct RupeeGolem {
    Actor actor;
    ColliderCylinder collider;
    s16 health;
    s16 hurtTimer;
} RupeeGolem;

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 40, 90, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = { "OnActorInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks, 1 };

static const SOHBossDefinition sGolemBoss = {
    sizeof(SOHBossDefinition),
    GOLEM_BOSS_KEY,
    "Rupee Golem",
    NULL,
    SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_GOHMA),
    SOH_BOSS_SLOT_GOHMA,
    "CanJumpslash or RG_FAIRY_SLINGSHOT",
    NULL,
};

static const SOHModApi* sApi;
static int16_t sGolemActorId = -1;

static void Golem_Defeat(RupeeGolem* this, PlayState* play) {
    Vec3f warp = this->actor.world.pos;
    Vec3f heart = this->actor.world.pos;
    heart.z += GOLEM_HEART_OFFSET_Z;
    Item_DropCollectible(play, &this->actor.world.pos, ITEM00_RUPEE_RED);
    Z64Boss_Finish(sApi, play, &this->actor, warp, heart);
    Actor_Kill(&this->actor);
}

static void Golem_Init(Actor* thisx, PlayState* play) {
    RupeeGolem* this = (RupeeGolem*)thisx;

    Actor_SetScale(&this->actor, GOLEM_SCALE);
    this->actor.gravity = -1.0f;
    this->health = GOLEM_HEALTH;
    this->actor.colChkInfo.health = GOLEM_HEALTH;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
}

static void Golem_Destroy(Actor* thisx, PlayState* play) {
    RupeeGolem* this = (RupeeGolem*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void Golem_Update(Actor* thisx, PlayState* play) {
    RupeeGolem* this = (RupeeGolem*)thisx;

    if (this->hurtTimer > 0) {
        this->hurtTimer--;
    } else if ((this->collider.base.acFlags & AC_HIT) != 0) {
        this->collider.base.acFlags &= ~AC_HIT;
        this->hurtTimer = GOLEM_HURT_FRAMES;
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_GOMA_DAM1);
        if (--this->health <= 0) {
            Golem_Defeat(this, play);
            return;
        }
    }
    this->actor.shape.rot.y += this->hurtTimer > 0 ? GOLEM_HURT_SPIN_SPEED : GOLEM_SPIN_SPEED;
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 40.0f, 40.0f, 5);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void Golem_Draw(Actor* thisx, PlayState* play) {
    Matrix_Scale(GOLEM_MODEL_SCALE, GOLEM_MODEL_SCALE, GOLEM_MODEL_SCALE, MTXMODE_APPLY);
    GetItem_Draw(play, GID_RUPEE_RED);
}

// The vanilla boss still spawns in its own room: it hands its spot over to the golem.
static void OnVanillaBossInit(void* actorPtr) {
    Actor* vanillaBoss = (Actor*)actorPtr;

    if (!Z64Boss_IsFightHere(sApi, GOLEM_BOSS_KEY, gPlayState)) {
        return;
    }
    if (!Flags_GetClear(gPlayState, gPlayState->roomCtx.curRoom.num) && sGolemActorId >= 0) {
        Actor_Spawn(&gPlayState->actorCtx, gPlayState, sGolemActorId, vanillaBoss->world.pos.x,
                    vanillaBoss->world.pos.y, vanillaBoss->world.pos.z, 0, 0, 0, 0);
    }
    Actor_Kill(vanillaBoss);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterBoss)) {
        return;
    }
    SOHActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = GOLEM_ACTOR_KEY;
    definition.description = "Rupee Golem";
    definition.category = ACTORCAT_BOSS;
    definition.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                            ACTOR_FLAG_DRAW_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(RupeeGolem);
    definition.init = Golem_Init;
    definition.destroy = Golem_Destroy;
    definition.update = Golem_Update;
    definition.draw = Golem_Draw;
    definition.naviHint = "Rupee Golem&%cA spinning heap of greed. Hit it six times!%w";
    sGolemActorId = sApi->RegisterActor(&definition);

    sApi->RegisterBoss(&sGolemBoss);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_BOSS_GOMA, OnVanillaBossInit);
}
