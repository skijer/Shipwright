// Majora's Mask's Boe (En_Mkk) as an Unbound custom enemy, drawn from mm.o2r. Params 1 is the white Boe.
// Snowhead's respawning and delayed Boes belong to their scenes: here each one fades in once and stays dead.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define BOE_KEY "mm.boe"
#define BOE_SOUL_ITEM_KEY "soul.mm.boe"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_BOE "__OTR__objects/object_mkk/"

#define BGCHECK_UPDATE_WALL_FLOOR ((1 << 0) | (1 << 2))

#define MM_SE_EN_KUROSUKE_MOVE 0x38BC
#define MM_SE_EN_KUROSUKE_ATTACK 0x38BD

#define BOE_TYPE_BLACK 0
#define BOE_TYPE_WHITE 1
#define BOE_POUNCE_RANGE 120.0f

typedef struct {
    const char* material;
    const char* body;
    const char* end;
    const char* eyes;
} BoeDisplayLists;

struct EnBoe;

typedef void (*EnBoeActionFunc)(struct EnBoe*, PlayState*);

typedef struct EnBoe {
    Actor actor;
    EnBoeActionFunc actionFunc;
    u8 eyeColorIndex;
    u8 pounceCooldown;
    u8 alpha;
    bool isTrailFollowing;
    s16 timer;
    s16 heading;
    Vec3f tailPos[2];
    Vec3f vanishPos;
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[1];
} EnBoe;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sBlackBoeBodyMaterialDL[] = OBJ_BOE "gBlackBoeBodyMaterialDL";
static const ALIGN_ASSET(2) char sBlackBoeBodyModelDL[] = OBJ_BOE "gBlackBoeBodyModelDL";
static const ALIGN_ASSET(2) char sBlackBoeEndDL[] = OBJ_BOE "gBlackBoeEndDL";
static const ALIGN_ASSET(2) char sBlackBoeEyesDL[] = OBJ_BOE "gBlackBoeEyesDL";
static const ALIGN_ASSET(2) char sWhiteBoeBodyMaterialDL[] = OBJ_BOE "gWhiteBoeBodyMaterialDL";
static const ALIGN_ASSET(2) char sWhiteBoeBodyModelDL[] = OBJ_BOE "gWhiteBoeBodyModelDL";
static const ALIGN_ASSET(2) char sWhiteBoeEndDL[] = OBJ_BOE "gWhiteBoeEndDL";
static const ALIGN_ASSET(2) char sWhiteBoeEyesDL[] = OBJ_BOE "gWhiteBoeEyesDL";

static const BoeDisplayLists sBoeDisplayLists[] = {
    { sBlackBoeBodyMaterialDL, sBlackBoeBodyModelDL, sBlackBoeEndDL, sBlackBoeEyesDL },
    { sWhiteBoeBodyMaterialDL, sWhiteBoeBodyModelDL, sWhiteBoeEndDL, sWhiteBoeEyesDL },
};

static const Color_RGBA8 sEyeColors[] = {
    { 255, 255, 255, 255 }, { 128, 128, 128, 255 }, { 0, 0, 0, 255 },
    { 0, 0, 0, 255 },       { 128, 128, 128, 255 }, { 255, 255, 255, 255 },
};
static const Color_RGBA8 sVanishPrimColors[] = { { 50, 50, 50, 255 }, { 255, 255, 255, 255 } };
static const Color_RGBA8 sVanishEnvColors[] = { { 200, 200, 200, 255 }, { 255, 255, 255, 255 } };

static ColliderJntSphElementInit sElementsInit[1] = {
    {
        { ELEMTYPE_UNK0,
          { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x04 },
          { 0xFFCFFFFF, 0x00, 0x00 },
          TOUCH_ON | TOUCH_SFX_HARD,
          BUMP_ON,
          OCELEM_ON },
        { 0, { { 0, 0, 0 }, 15 }, 100 },
    },
};

static ColliderJntSphInit sColliderInit = {
    { COLTYPE_HIT3, AT_NONE | AT_TYPE_ENEMY, AC_NONE | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    1,
    sElementsInit,
};

// One hit of anything pops a Boe, so every column only has to deal damage.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(1, 0x0),
    /* Deku stick    */ DMG_ENTRY(2, 0x0),
    /* Slingshot     */ DMG_ENTRY(1, 0x0),
    /* Explosive     */ DMG_ENTRY(2, 0x0),
    /* Boomerang     */ DMG_ENTRY(1, 0x0),
    /* Normal arrow  */ DMG_ENTRY(2, 0x0),
    /* Hammer swing  */ DMG_ENTRY(2, 0x0),
    /* Hookshot      */ DMG_ENTRY(2, 0x0),
    /* Kokiri sword  */ DMG_ENTRY(1, 0x0),
    /* Master sword  */ DMG_ENTRY(2, 0x0),
    /* Giant's Knife */ DMG_ENTRY(4, 0x0),
    /* Fire arrow    */ DMG_ENTRY(4, 0x0),
    /* Ice arrow     */ DMG_ENTRY(4, 0x0),
    /* Light arrow   */ DMG_ENTRY(4, 0x0),
    /* Unk arrow 1   */ DMG_ENTRY(2, 0x0),
    /* Unk arrow 2   */ DMG_ENTRY(2, 0x0),
    /* Unk arrow 3   */ DMG_ENTRY(2, 0x0),
    /* Fire magic    */ DMG_ENTRY(2, 0x0),
    /* Ice magic     */ DMG_ENTRY(2, 0x0),
    /* Light magic   */ DMG_ENTRY(2, 0x0),
    /* Shield        */ DMG_ENTRY(0, 0x0),
    /* Mirror Ray    */ DMG_ENTRY(0, 0x0),
    /* Kokiri spin   */ DMG_ENTRY(2, 0x0),
    /* Giant spin    */ DMG_ENTRY(8, 0x0),
    /* Master spin   */ DMG_ENTRY(4, 0x0),
    /* Kokiri jump   */ DMG_ENTRY(2, 0x0),
    /* Giant jump    */ DMG_ENTRY(8, 0x0),
    /* Master jump   */ DMG_ENTRY(4, 0x0),
    /* Unknown 1     */ DMG_ENTRY(0, 0x0),
    /* Unblockable   */ DMG_ENTRY(0, 0x0),
    /* Hammer jump   */ DMG_ENTRY(4, 0x0),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

static CollisionCheckInfoInit sColChkInfoInit = { 1, 15, 30, 10 };

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sDustAccel = { 0.0f, 0.45f, 0.0f };
static Color_RGBA8 sDustPrim = { 250, 250, 250, 255 };
static Color_RGBA8 sDustEnv = { 180, 180, 180, 255 };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

static void Boe_FadeIn(EnBoe* this, PlayState* play);
static void Boe_Wait(EnBoe* this, PlayState* play);
static void Boe_Scurry(EnBoe* this, PlayState* play);
static void Boe_Pounce(EnBoe* this, PlayState* play);
static void Boe_Knockback(EnBoe* this, PlayState* play);
static void Boe_Vanish(EnBoe* this, PlayState* play);
static void EnBoe_Draw(Actor* thisx, PlayState* play);
static void Boe_DrawVanishing(Actor* thisx, PlayState* play);

// Falls back to the closest OoT sound when the mm_assets sound service is missing.
static void PlaySfx(Actor* actor, u16 mmSfx, u16 ootSfx) {
    if (!sIsMmSfxSearched && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
        sIsMmSfxSearched = true;
    }
    if (sPlayMmSfx != NULL && sPlayMmSfx(mmSfx, &actor->projectedPos)) {
        return;
    }
    Audio_PlayActorSound2(actor, ootSfx);
}

static void Boe_SetupWait(EnBoe* this) {
    this->timer = (s32)(Rand_ZeroOne() * 20.0f) + 10;
    if (this->pounceCooldown != 0) {
        this->pounceCooldown--;
    }
    this->actionFunc = Boe_Wait;
}

static void Boe_FadeIn(EnBoe* this, PlayState* play) {
    s32 alpha = this->alpha + 5;

    if (alpha < 255) {
        this->alpha = alpha;
        return;
    }
    this->eyeColorIndex = 3;
    this->collider.base.acFlags |= AC_ON;
    this->alpha = 255;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    Boe_SetupWait(this);
}

static void Boe_SetupScurry(EnBoe* this) {
    this->timer = 30;
    this->heading = this->actor.shape.rot.y;
    this->isTrailFollowing = false;
    this->actionFunc = Boe_Scurry;
}

static void Boe_Wait(EnBoe* this, PlayState* play) {
    this->timer--;
    if (this->timer == 0) {
        Boe_SetupScurry(this);
    }
}

static void Boe_SpawnDust(EnBoe* this, PlayState* play) {
    Vec3f pos = { this->actor.world.pos.x, this->actor.world.pos.y + 15.0f, this->actor.world.pos.z };

    func_8002829C(play, &pos, &sZeroVec, &sDustAccel, &sDustPrim, &sDustEnv, 350, 20);
}

static void Boe_SetupPounce(EnBoe* this) {
    this->isTrailFollowing = true;
    this->actor.speedXZ = 3.0f;
    this->actor.velocity.y = 5.0f;
    PlaySfx(&this->actor, MM_SE_EN_KUROSUKE_ATTACK, NA_SE_EN_FFLY_ATTACK);
    this->collider.base.atFlags |= AT_ON;
    Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0x800);
    this->actionFunc = Boe_Pounce;
}

static bool Boe_CanPounce(EnBoe* this) {
    return this->pounceCooldown == 0 && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
           Actor_IsFacingPlayer(&this->actor, 0x1800) && this->actor.xzDistToPlayer < BOE_POUNCE_RANGE &&
           fabsf(this->actor.yDistToPlayer) < 100.0f;
}

// Darts toward Link in short wobbling bursts, then stops to wait.
static void Boe_Scurry(EnBoe* this, PlayState* play) {
    bool isWhiteKickingUpDust = this->actor.params == BOE_TYPE_WHITE &&
                                (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && this->actor.speedXZ > 2.5f;
    bool hasStopped = false;

    this->timer--;
    if (isWhiteKickingUpDust && (play->gameplayFrames % 3) == 0) {
        Boe_SpawnDust(this, play);
    }
    if (this->timer > 0) {
        Math_StepToF(&this->actor.speedXZ, 5.0f, 0.7f);
    } else {
        hasStopped = Math_StepToF(&this->actor.speedXZ, 0.0f, 0.7f);
    }
    Math_ScaledStepToS(&this->heading, this->actor.yawTowardsPlayer, 0x400);
    this->actor.shape.rot.y =
        (s32)(Math_SinF(this->timer * ((2 * M_PI) / 15)) * (614.4f * this->actor.speedXZ)) + this->heading;
    PlaySfx(&this->actor, MM_SE_EN_KUROSUKE_MOVE, NA_SE_EN_FFLY_FLY);
    if (hasStopped) {
        Boe_SetupWait(this);
    } else if (Boe_CanPounce(this)) {
        Boe_SetupPounce(this);
    }
}

static void Boe_Pounce(EnBoe* this, PlayState* play) {
    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~(AT_ON | AT_HIT);
    }
    if (this->actor.velocity.y < 0.0f && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        this->pounceCooldown = 2;
        this->collider.base.atFlags &= ~AT_ON;
        Boe_SetupScurry(this);
    }
}

static void Boe_SetupKnockback(EnBoe* this) {
    Actor* attacker = this->collider.base.ac;

    this->isTrailFollowing = true;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_PO_DEAD);
    this->alpha = 254;
    if (attacker != NULL) {
        this->actor.world.rot.y = Actor_WorldYawTowardActor(attacker, &this->actor);
    }
    this->actor.speedXZ = 7.0f;
    this->actor.shape.rot.y = this->actor.world.rot.y;
    this->actor.velocity.y = 5.0f;
    this->actor.gravity = -1.3f;
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    this->actionFunc = Boe_Knockback;
}

static void Boe_SetupVanish(EnBoe* this) {
    this->vanishPos = this->actor.world.pos;
    this->tailPos[0] = this->actor.world.pos;
    this->tailPos[1] = this->actor.world.pos;
    Actor_SetScale(&this->actor, 0.005f);
    this->actor.draw = Boe_DrawVanishing;
    this->actionFunc = Boe_Vanish;
}

static void Boe_Pop(EnBoe* this, PlayState* play) {
    Color_RGBA8 prim = sVanishPrimColors[this->actor.params];
    Color_RGBA8 env = sVanishEnvColors[this->actor.params];
    Vec3f pos = { this->actor.world.pos.x, this->actor.world.pos.y + 15.0f, this->actor.world.pos.z };

    EffectSsDeadDb_Spawn(play, &pos, &sZeroVec, &sZeroVec, 70, 4, prim.r, prim.g, prim.b, prim.a, env.r, env.g, env.b,
                         1, 12, false);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_EXTINCT);
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x10);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Boe_SetupVanish(this);
}

// Bounces on the floor until it settles, then bursts into soot.
static void Boe_Knockback(EnBoe* this, PlayState* play) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return;
    }
    if (this->actor.velocity.y > -1.0f) {
        Boe_Pop(this, play);
        return;
    }
    this->actor.velocity.y *= -0.8f;
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
}

static void Boe_Vanish(EnBoe* this, PlayState* play) {
    s32 alpha = this->alpha - 20;

    if (alpha <= 0) {
        Actor_Kill(&this->actor);
        return;
    }
    this->alpha = alpha;
    this->actor.world.pos.x += 0.3f + 3.0f * Rand_ZeroOne();
    this->actor.world.pos.y += 0.5f + 3.5f * Rand_ZeroOne();
    this->vanishPos.x -= 0.3f + 3.0f * Rand_ZeroOne();
    this->vanishPos.y += 0.5f + 3.5f * Rand_ZeroOne();
    this->tailPos[0].z += 0.3f + 3.0f * Rand_ZeroOne();
    this->tailPos[0].y += 0.5f + 3.5f * Rand_ZeroOne();
    this->tailPos[1].z -= 0.3f + 3.0f * Rand_ZeroOne();
    this->tailPos[1].y += 0.5f + 3.5f * Rand_ZeroOne();
}

static void Boe_PlaceTailBehind(Vec3f* tail, Vec3f* leader, Vec3f* from, f32 spacing) {
    f32 distance = Math_Vec3f_DistXYZ(leader, from);

    if (distance < 0.001f) {
        *tail = *from;
        return;
    }
    f32 scale = spacing / distance;

    tail->x = (from->x - leader->x) * scale + leader->x;
    tail->y = (from->y - leader->y) * scale + leader->y;
    tail->z = (from->z - leader->z) * scale + leader->z;
}

// The black Boe's two trailing puffs: a wavy tail while it scurries, a straight trail while airborne.
static void Boe_UpdateTail(EnBoe* this) {
    if (this->isTrailFollowing) {
        Vec3f previousFirst = this->tailPos[0];

        Boe_PlaceTailBehind(&this->tailPos[0], &this->actor.world.pos, &this->actor.prevPos, 10.0f);
        Boe_PlaceTailBehind(&this->tailPos[1], &this->tailPos[0], &previousFirst, 12.0f);
        return;
    }
    s16 wave = (s32)(1228.8f * this->actor.speedXZ * Math_SinF(this->timer * (M_PI / 5)));

    this->tailPos[1].y = this->tailPos[0].y;
    this->tailPos[0].y = this->actor.world.pos.y;
    this->tailPos[0].x = this->actor.world.pos.x - 10.0f * Math_SinS(this->actor.shape.rot.y + wave);
    this->tailPos[0].z = this->actor.world.pos.z - 10.0f * Math_CosS(this->actor.shape.rot.y + wave);
    this->tailPos[1].x = this->tailPos[0].x - 12.0f * Math_SinS(this->actor.shape.rot.y - wave);
    this->tailPos[1].z = this->tailPos[0].z - 12.0f * Math_CosS(this->actor.shape.rot.y - wave);
}

static void Boe_CheckDamage(EnBoe* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    Actor_SetDropFlag(&this->actor, &this->collider.elements[0].info, false);
    Enemy_StartFinishingBlow(play, &this->actor);
    Boe_SetupKnockback(this);
}

static void Boe_UpdateEyeBlink(EnBoe* this) {
    if (this->eyeColorIndex > 0) {
        this->eyeColorIndex--;
    } else if (Rand_ZeroOne() < 0.075f && this->actor.params != BOE_TYPE_WHITE) {
        this->eyeColorIndex = ARRAY_COUNT(sEyeColors);
    }
}

static void Boe_LookAtPlayer(EnBoe* this, PlayState* play) {
    if (!Actor_IsFacingPlayer(&this->actor, 0x3000)) {
        return;
    }
    s16 pitch = Actor_WorldPitchTowardPoint(&this->actor, &GET_PLAYER(play)->actor.focus.pos);

    this->actor.shape.rot.x = CLAMP(pitch, -0x1800, 0x1800);
}

static void EnBoe_Init(Actor* thisx, PlayState* play) {
    EnBoe* this = (EnBoe*)thisx;

    this->actor.params = this->actor.params & 1;
    this->actor.gravity = -0.5f;
    this->actor.minVelocityY = -5.0f;
    this->actor.targetArrowOffset = 3000.0f;
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sColliderInit, this->colliderElements);
    this->collider.elements[0].dim.worldSphere.radius = sElementsInit[0].dim.modelSphere.radius;
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    ActorShape_Init(&this->actor.shape, 1000.0f, NULL, 0.0f);
    this->tailPos[0] = this->actor.world.pos;
    this->tailPos[1] = this->actor.world.pos;
    this->isTrailFollowing = true;
    this->alpha = 0;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = Boe_FadeIn;
}

static void EnBoe_Destroy(Actor* thisx, PlayState* play) {
    EnBoe* this = (EnBoe*)thisx;

    Collider_DestroyJntSph(play, &this->collider);
}

static void EnBoe_Update(Actor* thisx, PlayState* play) {
    EnBoe* this = (EnBoe*)thisx;

    if (this->actionFunc == Boe_Vanish) {
        this->actionFunc(this, play);
        return;
    }
    Boe_UpdateEyeBlink(this);
    Boe_CheckDamage(this, play);
    this->actionFunc(this, play);
    if (this->actionFunc == Boe_Vanish) {
        return;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 20.0f, BGCHECK_UPDATE_WALL_FLOOR);
    if (this->actor.params == BOE_TYPE_BLACK) {
        Boe_UpdateTail(this);
    }
    Boe_LookAtPlayer(this, play);
    Actor_SetFocus(&this->actor, 10.0f);
    this->collider.elements[0].dim.worldSphere.center = this->actor.focus.pos;
    if (this->collider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void Boe_DrawPuff(PlayState* play, Vec3f* pos, f32 yOffset, f32 scale, const char* bodyDL) {
    MtxF billboard;

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Get(&billboard);
    billboard.xw = pos->x;
    billboard.yw = pos->y + yOffset;
    billboard.zw = pos->z;
    Matrix_Put(&billboard);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)bodyDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Boe_DrawBody(EnBoe* this, PlayState* play, const BoeDisplayLists* lists) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, this->alpha);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)lists->material);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)lists->body);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Boe_DrawEnd(PlayState* play, const BoeDisplayLists* lists) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)lists->end);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The eyes sit on the opaque pass; D_80116280 + 2 is OoT's empty list for their render-mode segment.
static void Boe_DrawEyes(EnBoe* this, PlayState* play, const BoeDisplayLists* lists) {
    Color_RGBA8 eyeColor = sEyeColors[this->eyeColorIndex % ARRAY_COUNT(sEyeColors)];

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0xFF, eyeColor.r, eyeColor.g, eyeColor.b, eyeColor.a);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)(D_80116280 + 2));
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)lists->eyes);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnBoe_Draw(Actor* thisx, PlayState* play) {
    EnBoe* this = (EnBoe*)thisx;
    const BoeDisplayLists* lists = &sBoeDisplayLists[this->actor.params];

    if (this->actor.projectedPos.z <= 0.0f) {
        return;
    }
    if (this->alpha == 255) {
        Boe_DrawEyes(this, play, lists);
    }
    Boe_DrawBody(this, play, lists);
    if (this->actor.params == BOE_TYPE_BLACK) {
        Boe_DrawPuff(play, &this->tailPos[0], 8.5f, 0.85f, lists->body);
        Boe_DrawPuff(play, &this->tailPos[1], 7.225f, 0.85f, lists->body);
    }
    Boe_DrawEnd(play, lists);
}

static void Boe_DrawVanishing(Actor* thisx, PlayState* play) {
    EnBoe* this = (EnBoe*)thisx;
    const BoeDisplayLists* lists = &sBoeDisplayLists[this->actor.params];

    Boe_DrawBody(this, play, lists);
    Boe_DrawPuff(play, &this->tailPos[0], 5.0f, 1.0f, lists->body);
    Boe_DrawPuff(play, &this->tailPos[1], 5.0f, 1.0f, lists->body);
    Boe_DrawPuff(play, &this->vanishPos, 5.0f, 1.0f, lists->body);
    Boe_DrawEnd(play, lists);
}

// MM's CanKillEnemy(ACTOR_EN_MKK): one hit of nearly anything.
static bool Boe_CanKillInLogic(void* userData) {
    return sApi->LogicHasCapability("CanAttack");
}

#define SOUL_FLAME_DL "__OTR__objects/object_gi_fire/gGiBlueFireFlameDL"
// Per game unit: a Boe is tiny, so its soul shows it larger than the actor's 0.005.
#define SOUL_SCALE 0.015f

// Ship's enemy soul flame, which its own enemies' souls burn in.
static void DrawSoulFlame(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 8,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 16, 32, 1, play->state.frames,
                                             -8 * play->state.frames, 16, 32, 0, 0, 1, -8));
    Matrix_Push();
    Matrix_Translate(0.0f, -70.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(5.0f, 5.0f, 5.0f, MTXMODE_APPLY);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, 200, 60, 60, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)SOUL_FLAME_DL);
    gSPGrayscale(POLY_XLU_DISP++, false);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// The soul shows a Black Boe facing the camera, its eyes open.
static void DrawSoul(PlayState* play, GetItemEntry* getItemEntry) {
    const BoeDisplayLists* lists = &sBoeDisplayLists[BOE_TYPE_BLACK];
    const Color_RGBA8 eyeColor = sEyeColors[0];

    DrawSoulFlame(play);
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(SOUL_SCALE, SOUL_SCALE, SOUL_SCALE, MTXMODE_APPLY);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0xFF, eyeColor.r, eyeColor.g, eyeColor.b, eyeColor.a);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)(D_80116280 + 2));
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)lists->eyes);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)lists->material);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)lists->body);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)lists->end);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterSoul(void) {
    static const SOHCustomItemRandomizer sSoulRandomizer = {
        .structSize = sizeof(SOHCustomItemRandomizer),
        .flags = SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT,
        .type = SOH_CUSTOM_ITEM_TYPE_ITEM,
    };
    SOHCustomItemDefinition soul = Z64Items_Define(BOE_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Boes";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Boes%w!&Boes can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " BOE_SOUL_ITEM_KEY);
    }
}

static void RegisterBoe(void) {
    SOHActorDefinition boe = { 0 };

    boe.structSize = sizeof(boe);
    boe.key = BOE_KEY;
    boe.description = "Boe";
    boe.category = ACTORCAT_ENEMY;
    boe.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE;
    boe.objectId = OBJECT_GAMEPLAY_KEEP;
    boe.instanceSize = sizeof(EnBoe);
    boe.init = EnBoe_Init;
    boe.destroy = EnBoe_Destroy;
    boe.update = EnBoe_Update;
    boe.draw = EnBoe_Draw;
    boe.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    boe.enemyParams = BOE_TYPE_BLACK;
    boe.naviHint = "Boe&%cA ball of soot that pounces from the dark. One hit puffs it away!%w";
    boe.soulKey = BOE_KEY;
    boe.logicKill = Boe_CanKillInLogic;
    if (sApi->RegisterActor(&boe) < 0) {
        sApi->Log("Could not register " BOE_KEY);
    }
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterActor) || !SOH_MOD_API_HAS(sApi, LogicHasCapability)) {
        return;
    }
    RegisterSoul();
    RegisterBoe();
}
