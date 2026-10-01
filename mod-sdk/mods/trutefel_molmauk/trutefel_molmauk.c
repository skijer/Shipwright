// Molmauk (formerly Hammergeist), a brute with an ice hammer and a fire hammer.
// Model @syeo501, code @trueffel; ported from NEI as a custom enemy the enemy randomizer can pick.

#include "soh/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#include "object_hammergeist_assets.h"
#include "object_hammergeist_assets.inc.c"

#define MOLMAUK_KEY "trutefel.enemy.molmauk"

// Unbound leaves the bgCheckFlags bits unnamed.
#define BGCHECK_WATER (1 << 5)
#define BGCHECK_UPDATE_ALL ((1 << 0) | (1 << 2) | (1 << 3) | (1 << 4))

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000

#define MOLMAUK_WAKE_RANGE 800.0f
#define MOLMAUK_FORGET_RANGE 1500.0f
#define MOLMAUK_STAND_RANGE 75.0f
#define MOLMAUK_SLAM_RANGE 120.0f
#define MOLMAUK_EXPLOSION_MIN_RANGE 60.0f
#define MOLMAUK_EXPLOSION_MAX_RANGE 170.0f
#define MOLMAUK_EXPLOSION_MAX_RADIUS 150
#define MOLMAUK_SHOCKWAVE_RANGE 800.0f
#define MOLMAUK_HEAVY_SLAM_COOLDOWN 600
#define MOLMAUK_FLAME_PARTS 10

struct EnMolmauk;

typedef void (*EnMolmaukActionFunc)(struct EnMolmauk*, PlayState*);

typedef struct EnMolmauk {
    Actor actor;
    Vec3s firePos[MOLMAUK_FLAME_PARTS];
    Vec3s jointTable[GHAMMERGEISTSKEL_NUM_LIMBS];
    Vec3s morphTable[GHAMMERGEISTSKEL_NUM_LIMBS];
    Vec3s headRot;
    Vec3s upperBodyRot;
    SkelAnime skelAnime;
    ColliderCylinder collider;
    ColliderCylinder hammerLeftCollider;
    ColliderCylinder hammerRightCollider;
    ColliderJntSph explosionCollider;
    ColliderJntSphElement explosionColliderItems[1];
    s16 faceIndex;
    s16 fireHammerIndex;
    s16 iceHammerIndex;
    s16 hurtboxCooldown;
    s16 explosionTimer;
    s16 infuseTimer;
    s16 slamTimer;
    s16 heavySlamTimer;
    s16 heavySlamCooldown;
    s16 genericAnimationTimer;
    s16 fireTimer;
    s16 alpha;
    u8 explosionRadiusIncrease;
    u8 leftHammerInfused;
    u8 rightHammerInfused;
    u8 playerHit;
    u8 noHitAgain;
    EnMolmaukActionFunc actionFunc;
} EnMolmauk;

typedef enum {
    MOLMAUK_ANIMATION_IDLE,
    MOLMAUK_ANIMATION_WALK,
    MOLMAUK_ANIMATION_DAMAGE,
    MOLMAUK_ANIMATION_DIE,
    MOLMAUK_ANIMATION_EXPLOSION,
    MOLMAUK_ANIMATION_INFUSE,
    MOLMAUK_ANIMATION_SLAM_HEAVY,
    MOLMAUK_ANIMATION_SLAM_L,
    MOLMAUK_ANIMATION_SLAM_R,
    MOLMAUK_ANIMATION_FLEX,
} EnMolmaukAnimation;

typedef enum {
    MOLMAUK_FACE_NORMAL,
    MOLMAUK_FACE_LAUGH,
    MOLMAUK_FACE_MOUTH_OPEN,
} EnMolmaukFace;

typedef enum {
    MOLMAUK_HAMMER_PLAIN,
    MOLMAUK_HAMMER_INFUSED_1,
    MOLMAUK_HAMMER_INFUSED_2,
} EnMolmaukHammerLook;

typedef enum {
    MOLMAUK_DMGEFF_NONE,
    MOLMAUK_DMGEFF_STUN,
    MOLMAUK_DMGEFF_ICE_MAGIC = 6,
    MOLMAUK_DMGEFF_LIGHT_MAGIC = 13,
    MOLMAUK_DMGEFF_FIRE,
} EnMolmaukDamageEffect;

static const SOHModApi* sApi;

static void EnMolmauk_SetupDoNothing(EnMolmauk* this, PlayState* play);
static void EnMolmauk_ApproachPlayer(EnMolmauk* this, PlayState* play);
static void EnMolmauk_SetupDie(EnMolmauk* this, PlayState* play);
static void EnMolmauk_Die(EnMolmauk* this, PlayState* play);
static void EnMolmauk_SetupSlamL(EnMolmauk* this, PlayState* play);
static void EnMolmauk_SetupSlamR(EnMolmauk* this, PlayState* play);
static void EnMolmauk_SetupFlex(EnMolmauk* this, PlayState* play);

static ColliderCylinderInit sBodyCylinderInit = {
    {
        COLTYPE_METAL,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK1,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON | BUMP_HOOKABLE,
        OCELEM_ON,
    },
    { 40, 90, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sHammerCylinderInit = {
    {
        COLTYPE_HIT5,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { DMG_HAMMER, 0x00, 0x10 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 40, 80, 0, { 0, 0, 0 } },
};

static ColliderJntSphElementInit sExplosionElementsInit[1] = {
    {
        {
            ELEMTYPE_UNK0,
            { 0x00000008, 0x00, 0x20 },
            { 0x00000000, 0x00, 0x00 },
            TOUCH_ON | TOUCH_SFX_NONE,
            BUMP_ON,
            OCELEM_NONE,
        },
        { 0, { { 0, 0, 900 }, 0 }, 100 },
    },
};

static ColliderJntSphInit sExplosionInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ALL,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_JNTSPH,
    },
    1,
    sExplosionElementsInit,
};

static AnimationInfo sAnimationInfo[] = {
    { &gHammergeistSkelIdleAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP_INTERP, 3.0f },
    { &gHammergeistSkelWalkAnim, 2.0f, 0.0f, -1.0f, ANIMMODE_LOOP_PARTIAL, 3.0f },
    { &gHammergeistSkelDamageAnim, 3.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelDieAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelExplosionAnim, 2.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelInfuseAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelSlamheavyAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelSlamlAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelSlamrAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelFlexAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
};

static const char* sFaceTextures[] = {
    gHammergeistSkel_normal_ci8,
    gHammergeistSkel_laugh_ci8,
    gHammergeistSkel_mouth_open_ci8,
};

static const char* sFireHammerTextures[] = {
    gHammergeistSkel_metal2_rgba16,
    gHammergeistSkel_hammerfire_1_rgba16,
    gHammergeistSkel_hammerfire_2_rgba16,
};

static const char* sIceHammerTextures[] = {
    gHammergeistSkel_metal2_rgba16,
    gHammergeistSkel_hammerice_1_rgba16,
    gHammergeistSkel_hammerice_2_rgba16,
};

// The exported material DLs branch to segment 0x0C, which the original never sets.
static Gfx sEmptyDL[] = { gsSPEndDisplayList() };

static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, MOLMAUK_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, MOLMAUK_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, MOLMAUK_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(0, MOLMAUK_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(0, MOLMAUK_DMGEFF_STUN),
    /* Normal arrow  */ DMG_ENTRY(2, MOLMAUK_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, MOLMAUK_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, MOLMAUK_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, MOLMAUK_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, MOLMAUK_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, MOLMAUK_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(2, MOLMAUK_DMGEFF_NONE),
    /* Ice arrow     */ DMG_ENTRY(4, MOLMAUK_DMGEFF_NONE),
    /* Light arrow   */ DMG_ENTRY(2, MOLMAUK_DMGEFF_NONE),
    /* Unk arrow 1   */ DMG_ENTRY(2, MOLMAUK_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, MOLMAUK_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, MOLMAUK_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(0, MOLMAUK_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(3, MOLMAUK_DMGEFF_ICE_MAGIC),
    /* Light magic   */ DMG_ENTRY(4, MOLMAUK_DMGEFF_LIGHT_MAGIC),
    /* Shield        */ DMG_ENTRY(0, MOLMAUK_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, MOLMAUK_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(1, MOLMAUK_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(5, MOLMAUK_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(3, MOLMAUK_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, MOLMAUK_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(6, MOLMAUK_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, MOLMAUK_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, MOLMAUK_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, MOLMAUK_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(3, MOLMAUK_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, MOLMAUK_DMGEFF_NONE),
};

static CollisionCheckInfoInit2 sColChkInit = { 16, 35, 55, 0, MASS_HEAVY };

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static void EnMolmauk_SetupAction(EnMolmauk* this, EnMolmaukActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

static void EnMolmauk_ChangeAnimation(EnMolmauk* this, s32 index) {
    Animation_ChangeByInfo(&this->skelAnime, sAnimationInfo, index);
}

// Flickers between two textures every 16 frames so an infused hammer does not look static.
static s16 EnMolmauk_GetHammerLook(s16 look, bool isInfused, PlayState* play) {
    if (!isInfused) {
        return MOLMAUK_HAMMER_PLAIN;
    }
    if (look == MOLMAUK_HAMMER_PLAIN) {
        look = MOLMAUK_HAMMER_INFUSED_1;
    }
    if (play->gameplayFrames % 16 == 0) {
        look = look == MOLMAUK_HAMMER_INFUSED_1 ? MOLMAUK_HAMMER_INFUSED_2 : MOLMAUK_HAMMER_INFUSED_1;
    }
    return look;
}

static void EnMolmauk_SetHammerToucher(ColliderCylinder* hammer, bool isInfused, u8 effect, u32 elementFlag) {
    hammer->info.toucher.effect = isInfused ? effect : 0;
    hammer->info.toucher.dmgFlags = isInfused ? (DMG_HAMMER | elementFlag) : DMG_HAMMER;
    hammer->info.toucher.damage = isInfused ? 0x18 : 0x10;
}

static void EnMolmauk_UpdateHammerColliders(EnMolmauk* this) {
    EnMolmauk_SetHammerToucher(&this->hammerLeftCollider, this->leftHammerInfused, 2, DMG_MAGIC_ICE);
    EnMolmauk_SetHammerToucher(&this->hammerRightCollider, this->rightHammerInfused, 1, DMG_MAGIC_FIRE);

    // Two hearts, four when both elements go off together.
    this->explosionColliderItems[0].info.toucher.damage =
        (this->leftHammerInfused && this->rightHammerInfused) ? 0x40 : 0x20;
}

static void EnMolmauk_BurstIce(EnMolmauk* this, PlayState* play, Vec3f* pos) {
    for (s32 i = 0; i <= 7; i++) {
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, pos, 150, 150, 150, 250, 235, 245, 255, 4.0f);
    }
}

static void EnMolmauk_BurstFire(EnMolmauk* this, PlayState* play) {
    for (s32 i = 0; i <= 7; i++) {
        EffectSsEnFire_SpawnVec3f(play, &this->actor, &this->hammerRightCollider.dim.pos, 400, 0, 0, -1);
    }
}

static void EnMolmauk_DefuseLeftHammer(EnMolmauk* this, PlayState* play) {
    if (!this->leftHammerInfused) {
        return;
    }
    this->leftHammerInfused = false;
    EnMolmauk_BurstIce(this, play, &this->hammerLeftCollider.dim.pos);
}

static void EnMolmauk_DefuseRightHammer(EnMolmauk* this, PlayState* play) {
    if (!this->rightHammerInfused) {
        return;
    }
    this->rightHammerInfused = false;
    EnMolmauk_BurstFire(this, play);
}

static void EnMolmauk_DefuseHammers(EnMolmauk* this, PlayState* play) {
    EnMolmauk_DefuseLeftHammer(this, play);
    EnMolmauk_DefuseRightHammer(this, play);
}

static void EnMolmauk_UpdateBgCheck(EnMolmauk* this, PlayState* play) {
    Actor_UpdateBgCheckInfo(play, &this->actor, this->actor.colChkInfo.cylHeight, this->actor.colChkInfo.cylRadius,
                            this->actor.colChkInfo.cylHeight, BGCHECK_UPDATE_ALL);
}

static void EnMolmauk_FacePlayer(EnMolmauk* this) {
    Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 2000);
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 3000);
}

static void EnMolmauk_TurnToPlayerNow(EnMolmauk* this) {
    this->actor.speedXZ = 0.0f;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.shape.rot.y = this->actor.world.rot.y;
}

static void EnMolmauk_EnsureAnimation(EnMolmauk* this, s32 index) {
    if (this->skelAnime.animation != sAnimationInfo[index].animation) {
        EnMolmauk_ChangeAnimation(this, index);
    }
}

static bool EnMolmauk_IsWalkPause(f32 frame) {
    return (frame >= 10.0f && frame <= 20.0f) || (frame >= 38.0f && frame <= 45.0f);
}

// Two heavy steps per walk cycle: it halts and stomps on each, and only closes in between them.
static void EnMolmauk_WalkToPlayer(EnMolmauk* this) {
    if (this->actor.xzDistToPlayer <= MOLMAUK_STAND_RANGE) {
        EnMolmauk_EnsureAnimation(this, MOLMAUK_ANIMATION_IDLE);
        EnMolmauk_FacePlayer(this);
        this->actor.speedXZ = 0.0f;
        return;
    }
    EnMolmauk_EnsureAnimation(this, MOLMAUK_ANIMATION_WALK);
    f32 frame = this->skelAnime.curFrame;

    if (!EnMolmauk_IsWalkPause(frame)) {
        Math_ApproachF(&this->actor.speedXZ, 5.0f / 1.5f, 0.5f, 1.5f);
        EnMolmauk_FacePlayer(this);
        return;
    }
    this->actor.speedXZ = 0.0f;
    if (frame == 10.0f || frame == 38.0f) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_AMOS_WALK);
    }
}

static void EnMolmauk_Damage(EnMolmauk* this, PlayState* play) {
    if (!SkelAnime_Update(&this->skelAnime) || DECR(this->genericAnimationTimer) != 0) {
        return;
    }
    // One revenge swing at most, so a player trading hits is not locked in a slam loop.
    if (Rand_ZeroOne() < 0.4f && !this->noHitAgain) {
        this->noHitAgain = true;
        if (play->gameplayFrames % 2 == 0) {
            EnMolmauk_SetupSlamL(this, play);
        } else {
            EnMolmauk_SetupSlamR(this, play);
        }
        return;
    }
    this->noHitAgain = false;
    EnMolmauk_SetupDoNothing(this, play);
}

static void EnMolmauk_SetupDamage(EnMolmauk* this, PlayState* play) {
    static f32 sDamagePitch = 0.25f;

    this->genericAnimationTimer = 5;
    this->faceIndex = MOLMAUK_FACE_NORMAL;
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    Actor_ApplyDamage(&this->actor);
    Audio_PlaySoundGeneral(NA_SE_EN_STALKID_DAMAGE, &this->actor.world.pos, 4, &sDamagePitch,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    EnMolmauk_ChangeAnimation(this, MOLMAUK_ANIMATION_DAMAGE);
    EnMolmauk_SetupAction(this, EnMolmauk_Damage);
}

static void EnMolmauk_Stunned(EnMolmauk* this, PlayState* play);

static void EnMolmauk_SetupStunned(EnMolmauk* this, PlayState* play) {
    this->actor.speedXZ = 0.0f;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
    Animation_PlayOnceSetSpeed(&this->skelAnime, &gHammergeistSkelIdleAnim, 0.0f);
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 120, COLORFILTER_OPA, 60);
    EnMolmauk_SetupAction(this, EnMolmauk_Stunned);
}

// Only called in the windows where Molmauk is open to attack; the rest of the time its guard is up.
static void EnMolmauk_CheckDamage(EnMolmauk* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        this->hurtboxCooldown = 10;
        this->actor.speedXZ = 0.0f;

        if (this->actor.colChkInfo.damageEffect == MOLMAUK_DMGEFF_STUN) {
            Actor_ApplyDamage(&this->actor);
            EnMolmauk_SetupStunned(this, play);
        } else {
            EnMolmauk_SetupDamage(this, play);
        }
        if (this->actor.colChkInfo.health == 0) {
            EnMolmauk_SetupDie(this, play);
        }
    }
    if ((this->actor.bgCheckFlags & BGCHECK_WATER) && this->actionFunc != EnMolmauk_Die) {
        EnMolmauk_SetupDie(this, play);
    }
}

static void EnMolmauk_Stunned(EnMolmauk* this, PlayState* play) {
    EnMolmauk_CheckDamage(this, play);
    if (this->actionFunc == EnMolmauk_Stunned && this->actor.colorFilterTimer == 0) {
        EnMolmauk_SetupDoNothing(this, play);
    }
}

static void EnMolmauk_DoNothing(EnMolmauk* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.xzDistToPlayer < MOLMAUK_WAKE_RANGE) {
        EnMolmauk_ChangeAnimation(this, MOLMAUK_ANIMATION_WALK);
        EnMolmauk_SetupAction(this, EnMolmauk_ApproachPlayer);
    }
}

static void EnMolmauk_SetupDoNothing(EnMolmauk* this, PlayState* play) {
    this->actor.speedXZ = 0.0f;
    EnMolmauk_ChangeAnimation(this, MOLMAUK_ANIMATION_IDLE);
    EnMolmauk_SetupAction(this, EnMolmauk_DoNothing);
}

static void EnMolmauk_SetupDie(EnMolmauk* this, PlayState* play) {
    this->faceIndex = MOLMAUK_FACE_NORMAL;
    this->actor.shape.shadowDraw = NULL;
    EnMolmauk_DefuseHammers(this, play);
    this->actor.speedXZ = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 80);
    this->fireTimer = 40;
    Enemy_StartFinishingBlow(play, &this->actor);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_ANUBIS_FIRE);
    EnMolmauk_ChangeAnimation(this, MOLMAUK_ANIMATION_DIE);
    EnMolmauk_SetupAction(this, EnMolmauk_Die);
}

static void EnMolmauk_Die(EnMolmauk* this, PlayState* play) {
    if (this->alpha != 0 && play->gameplayFrames % 2 == 0) {
        this->alpha -= 5;
    }
    if (SkelAnime_Update(&this->skelAnime) && DECR(this->fireTimer) == 0 && this->actor.colorFilterTimer == 0) {
        Actor_Kill(&this->actor);
    }
}

static void EnMolmauk_Flex(EnMolmauk* this, PlayState* play) {
    EnMolmauk_CheckDamage(this, play);
    if (this->actionFunc != EnMolmauk_Flex) {
        return;
    }
    if (SkelAnime_Update(&this->skelAnime) && DECR(this->genericAnimationTimer) == 0) {
        this->faceIndex = MOLMAUK_FACE_NORMAL;
        EnMolmauk_SetupDoNothing(this, play);
    }
}

// Flexing leaves it open to attack.
static void EnMolmauk_SetupFlex(EnMolmauk* this, PlayState* play) {
    static f32 sFlexPitch = 0.7f;

    this->actor.speedXZ = 0.0f;
    this->genericAnimationTimer = 10;
    this->faceIndex = MOLMAUK_FACE_LAUGH;
    Audio_PlaySoundGeneral(NA_SE_EN_FANTOM_VOICE, &this->actor.world.pos, 4, &sFlexPitch, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
    EnMolmauk_ChangeAnimation(this, MOLMAUK_ANIMATION_FLEX);
    EnMolmauk_SetupAction(this, EnMolmauk_Flex);
}

static void EnMolmauk_GrowExplosion(EnMolmauk* this, PlayState* play) {
    ColliderJntSphElementDim* sphere = &this->explosionCollider.elements[0].dim;

    CollisionCheck_SetAT(play, &play->colChkCtx, &this->explosionCollider.base);
    sphere->modelSphere.radius += 15;
    sphere->worldSphere.radius = sphere->modelSphere.radius;
    if (sphere->worldSphere.radius >= MOLMAUK_EXPLOSION_MAX_RADIUS) {
        sphere->modelSphere.radius = 0;
        sphere->worldSphere.radius = 0;
        this->explosionRadiusIncrease = false;
    }
}

static void EnMolmauk_Detonate(EnMolmauk* this, PlayState* play) {
    Vec3f effectPos = this->actor.world.pos;
    Vec3f effectVelocity = { 0.0f, 0.0f, 0.0f };
    Vec3f effectAccel = { 0.0f, 0.0f, 0.0f };

    EnMolmauk_DefuseHammers(this, play);
    EffectSsBomb2_SpawnLayered(play, &effectPos, &effectVelocity, &effectAccel, 100, 30);
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_BOMB_EXPLOSION);
    Camera_AddQuake(&play->mainCamera, 2, 11, 8);
}

static void EnMolmauk_Explosion(EnMolmauk* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime) && DECR(this->genericAnimationTimer) == 0) {
        if (this->playerHit) {
            this->playerHit = false;
            EnMolmauk_SetupFlex(this, play);
        } else {
            this->faceIndex = MOLMAUK_FACE_NORMAL;
            EnMolmauk_SetupDoNothing(this, play);
        }
    }
    if (this->explosionCollider.base.atFlags & AT_HIT) {
        this->explosionCollider.base.atFlags &= ~AT_HIT;
        this->playerHit = true;
        func_8002F71C(play, &this->actor, 10.0f, this->actor.shape.rot.y, 5.0f);
        Player_PlaySfx(&GET_PLAYER(play)->actor, NA_SE_PL_BODY_HIT);
    }
    if (this->explosionRadiusIncrease) {
        EnMolmauk_GrowExplosion(this, play);
    }
    if (this->skelAnime.curFrame == 30.0f) {
        this->explosionRadiusIncrease = true;
    }
    if (this->skelAnime.curFrame == 40.0f) {
        this->explosionRadiusIncrease = true;
        EnMolmauk_Detonate(this, play);
    }
    if (this->skelAnime.curFrame >= 41.0f) {
        EnMolmauk_CheckDamage(this, play);
    }
}

static void EnMolmauk_SetupExplosion(EnMolmauk* this, PlayState* play) {
    this->genericAnimationTimer = 33;
    this->explosionRadiusIncrease = false;
    this->playerHit = false;
    EnMolmauk_ChangeAnimation(this, MOLMAUK_ANIMATION_EXPLOSION);
    EnMolmauk_SetupAction(this, EnMolmauk_Explosion);
    EnMolmauk_TurnToPlayerNow(this);
}

static void EnMolmauk_Infuse(EnMolmauk* this, PlayState* play) {
    f32 frame = this->skelAnime.curFrame;

    if (frame == 9.0f || frame == 31.0f) {
        this->faceIndex = MOLMAUK_FACE_MOUTH_OPEN;
    } else if (frame == 15.0f) {
        this->rightHammerInfused = true;
        EnMolmauk_BurstFire(this, play);
        this->faceIndex = MOLMAUK_FACE_NORMAL;
    } else if (frame == 37.0f) {
        Vec3f icePos = this->hammerLeftCollider.dim.pos;

        // The ice shards read better from above the hammer head.
        icePos.y += 70.0f;
        this->leftHammerInfused = true;
        EnMolmauk_BurstIce(this, play, &icePos);
        this->faceIndex = MOLMAUK_FACE_LAUGH;
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        EnMolmauk_SetupDoNothing(this, play);
    }
}

static void EnMolmauk_SetupInfuse(EnMolmauk* this, PlayState* play) {
    this->actor.speedXZ = 0.0f;
    this->genericAnimationTimer = 10;
    EnMolmauk_ChangeAnimation(this, MOLMAUK_ANIMATION_INFUSE);
    EnMolmauk_SetupAction(this, EnMolmauk_Infuse);
}

static void EnMolmauk_ShakeGround(EnMolmauk* this, PlayState* play) {
    EnMolmauk_DefuseHammers(this, play);
    Audio_PlaySoundGeneral(NA_SE_EV_WALL_BROKEN, &GET_PLAYER(play)->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    for (s32 i = 0; i < 10; i++) {
        Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, i * 100.0f, 4, 4.0f, i * 500, i * 110,
                                 true);
    }
    if (this->actor.xzDistToPlayer < MOLMAUK_SHOCKWAVE_RANGE) {
        func_8002F6D4(play, &this->actor, 20.0f, (s16)(GET_PLAYER(play)->actor.world.rot.y + 0x8000), 10.0f, 0x30);
    }
}

static void EnMolmauk_HeavySlam(EnMolmauk* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        this->heavySlamCooldown = MOLMAUK_HEAVY_SLAM_COOLDOWN;
        if (DECR(this->genericAnimationTimer) == 0) {
            EnMolmauk_SetupFlex(this, play);
            return;
        }
    }
    // A hit just before the hammers land cancels the slam.
    if (this->skelAnime.curFrame >= 38.0f && this->skelAnime.curFrame <= 45.0f) {
        EnMolmauk_CheckDamage(this, play);
        if (this->actionFunc != EnMolmauk_HeavySlam) {
            return;
        }
    }
    if (this->skelAnime.curFrame == 50.0f) {
        EnMolmauk_ShakeGround(this, play);
    }
}

static void EnMolmauk_SetupHeavySlam(EnMolmauk* this, PlayState* play) {
    this->actor.speedXZ = 0.0f;
    this->genericAnimationTimer = 10;
    EnMolmauk_ChangeAnimation(this, MOLMAUK_ANIMATION_SLAM_HEAVY);
    EnMolmauk_SetupAction(this, EnMolmauk_HeavySlam);
}

static void EnMolmauk_SetPlayerAblaze(Player* player) {
    if (player->bodyIsBurning) {
        return;
    }
    for (s32 i = 0; i < PLAYER_BODYPART_MAX; i++) {
        player->bodyFlameTimers[i] = Rand_S16Offset(0, 200);
    }
    player->bodyIsBurning = true;
}

// Shared by both one-handed slams; returns false once the slam has handed over to another action.
static bool EnMolmauk_UpdateSlam(EnMolmauk* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        this->faceIndex = MOLMAUK_FACE_NORMAL;
        EnMolmauk_SetupDoNothing(this, play);
        return false;
    }
    if (this->skelAnime.curFrame >= 20.0f) {
        EnMolmauk_CheckDamage(this, play);
    }
    if (this->skelAnime.curFrame == 15.0f) {
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_HAMMER_HIT);
    }
    return true;
}

// Registering the collider clears AT_HIT, so the hit is read before this runs.
static void EnMolmauk_SwingHammer(EnMolmauk* this, PlayState* play, ColliderCylinder* hammer) {
    if (this->skelAnime.curFrame >= 10.0f && this->skelAnime.curFrame <= 18.0f) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &hammer->base);
    }
}

static bool EnMolmauk_ConsumeHammerHit(ColliderCylinder* hammer) {
    if (!(hammer->base.atFlags & AT_HIT)) {
        return false;
    }
    hammer->base.atFlags &= ~AT_HIT;
    return true;
}

static void EnMolmauk_SlamL(EnMolmauk* this, PlayState* play) {
    if (!EnMolmauk_UpdateSlam(this, play)) {
        return;
    }
    if (this->skelAnime.curFrame == 17.0f) {
        EnMolmauk_DefuseLeftHammer(this, play);
    }
    if (EnMolmauk_ConsumeHammerHit(&this->hammerLeftCollider)) {
        if (this->leftHammerInfused) {
            EnMolmauk_DefuseLeftHammer(this, play);
        } else {
            func_8002F71C(play, &this->actor, 0.0f, this->actor.shape.rot.y, 0.0f);
        }
        Player_PlaySfx(&GET_PLAYER(play)->actor, NA_SE_PL_BODY_HIT);
    }
    EnMolmauk_SwingHammer(this, play, &this->hammerLeftCollider);
}

static void EnMolmauk_SlamR(EnMolmauk* this, PlayState* play) {
    if (!EnMolmauk_UpdateSlam(this, play)) {
        return;
    }
    if (this->skelAnime.curFrame == 17.0f) {
        EnMolmauk_DefuseRightHammer(this, play);
    }
    if (EnMolmauk_ConsumeHammerHit(&this->hammerRightCollider)) {
        if (this->rightHammerInfused) {
            EnMolmauk_DefuseRightHammer(this, play);
            EnMolmauk_SetPlayerAblaze(GET_PLAYER(play));
        }
        func_8002F71C(play, &this->actor, 0.0f, this->actor.shape.rot.y, 0.0f);
        Player_PlaySfx(&GET_PLAYER(play)->actor, NA_SE_PL_BODY_HIT);
    }
    EnMolmauk_SwingHammer(this, play, &this->hammerRightCollider);
}

static void EnMolmauk_SetupSlamL(EnMolmauk* this, PlayState* play) {
    EnMolmauk_TurnToPlayerNow(this);
    EnMolmauk_ChangeAnimation(this, MOLMAUK_ANIMATION_SLAM_L);
    EnMolmauk_SetupAction(this, EnMolmauk_SlamL);
}

static void EnMolmauk_SetupSlamR(EnMolmauk* this, PlayState* play) {
    EnMolmauk_TurnToPlayerNow(this);
    EnMolmauk_ChangeAnimation(this, MOLMAUK_ANIMATION_SLAM_R);
    EnMolmauk_SetupAction(this, EnMolmauk_SlamR);
}

static bool EnMolmauk_RollAttack(s16* timer, s16 resetValue, f32 chance) {
    if (DECR(*timer) != 0) {
        return false;
    }
    *timer = resetValue;
    return Rand_ZeroOne() < chance;
}

static void EnMolmauk_ApproachPlayer(EnMolmauk* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    EnMolmauk_WalkToPlayer(this);

    if (this->actor.xzDistToPlayer > MOLMAUK_FORGET_RANGE) {
        EnMolmauk_SetupDoNothing(this, play);
        return;
    }
    if (this->actor.xzDistToPlayer < MOLMAUK_SLAM_RANGE && EnMolmauk_RollAttack(&this->slamTimer, 30, 0.6f)) {
        if (play->gameplayFrames % 2 == 0) {
            EnMolmauk_SetupSlamL(this, play);
        } else {
            EnMolmauk_SetupSlamR(this, play);
        }
        return;
    }
    if (!this->leftHammerInfused && !this->rightHammerInfused && EnMolmauk_RollAttack(&this->infuseTimer, 40, 0.2f)) {
        EnMolmauk_SetupInfuse(this, play);
        return;
    }
    if (this->actor.xzDistToPlayer < MOLMAUK_EXPLOSION_MAX_RANGE &&
        this->actor.xzDistToPlayer > MOLMAUK_EXPLOSION_MIN_RANGE &&
        EnMolmauk_RollAttack(&this->explosionTimer, 20, 0.3f)) {
        EnMolmauk_SetupExplosion(this, play);
        return;
    }
    if (DECR(this->heavySlamCooldown) == 0 && EnMolmauk_RollAttack(&this->heavySlamTimer, 60, 0.2f)) {
        EnMolmauk_SetupHeavySlam(this, play);
    }
}

static void EnMolmauk_UpdateCollision(EnMolmauk* this, PlayState* play) {
    Collider_UpdateCylinder(&this->actor, &this->collider);
    Collider_UpdateCylinder(&this->actor, &this->hammerLeftCollider);
    Collider_UpdateCylinder(&this->actor, &this->hammerRightCollider);

    if (DECR(this->hurtboxCooldown) == 0 && this->actionFunc != EnMolmauk_Die) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
    EnMolmauk_UpdateHammerColliders(this);
}

static void EnMolmauk_Init(Actor* thisx, PlayState* play) {
    EnMolmauk* this = (EnMolmauk*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 80.0f);
    Actor_SetScale(&this->actor, 0.015f);
    this->actor.gravity = -1.0f;
    this->explosionTimer = 20;
    this->infuseTimer = 20;
    this->slamTimer = 20;
    this->heavySlamTimer = 60;
    this->alpha = 255;
    this->faceIndex = MOLMAUK_FACE_NORMAL;

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sBodyCylinderInit);
    Collider_InitCylinder(play, &this->hammerLeftCollider);
    Collider_SetCylinder(play, &this->hammerLeftCollider, &this->actor, &sHammerCylinderInit);
    Collider_InitCylinder(play, &this->hammerRightCollider);
    Collider_SetCylinder(play, &this->hammerRightCollider, &this->actor, &sHammerCylinderInit);
    Collider_InitJntSph(play, &this->explosionCollider);
    Collider_SetJntSph(play, &this->explosionCollider, &this->actor, &sExplosionInit, &this->explosionColliderItems[0]);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, &sDamageTable, &sColChkInit);

    SkelAnime_InitFlex(play, &this->skelAnime, &gHammergeistSkel, NULL, this->jointTable, this->morphTable,
                       GHAMMERGEISTSKEL_NUM_LIMBS);
    EnMolmauk_SetupDoNothing(this, play);
}

// No SkelAnime_Free: the joint tables live inside the actor, and freeing them would corrupt the arena.
static void EnMolmauk_Destroy(Actor* thisx, PlayState* play) {
    EnMolmauk* this = (EnMolmauk*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
    Collider_DestroyCylinder(play, &this->hammerLeftCollider);
    Collider_DestroyCylinder(play, &this->hammerRightCollider);
    Collider_DestroyJntSph(play, &this->explosionCollider);
}

static void EnMolmauk_Update(Actor* thisx, PlayState* play) {
    EnMolmauk* this = (EnMolmauk*)thisx;

    this->actionFunc(this, play);
    Actor_MoveXZGravity(&this->actor);
    EnMolmauk_UpdateBgCheck(this, play);
    EnMolmauk_UpdateCollision(this, play);
    this->fireHammerIndex = EnMolmauk_GetHammerLook(this->fireHammerIndex, this->rightHammerInfused, play);
    this->iceHammerIndex = EnMolmauk_GetHammerLook(this->iceHammerIndex, this->leftHammerInfused, play);
    func_80038290(play, &this->actor, &this->headRot, &this->upperBodyRot, this->actor.focus.pos);
}

static s32 EnMolmauk_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx,
                                      Gfx** gfx) {
    EnMolmauk* this = (EnMolmauk*)thisx;

    if (limbIndex == GHAMMERGEISTSKEL_HEAD_LIMB && this->actionFunc == EnMolmauk_ApproachPlayer) {
        rot->z += this->headRot.y;
        rot->x += this->headRot.x;
    }
    return false;
}

static void EnMolmauk_PlaceHammer(ColliderCylinder* hammer, Vec3f* auraPos, MtxF* mtx) {
    hammer->dim.pos.x = mtx->xw;
    hammer->dim.pos.y = mtx->yw - 40.0f;
    hammer->dim.pos.z = mtx->zw;
    auraPos->x = mtx->xw;
    auraPos->y = mtx->yw + 30.0f;
    auraPos->z = mtx->zw;
}

// The auras are spawned on every limb, as the original did; fewer made the infused hammers look dim.
static void EnMolmauk_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx, Gfx** gfx) {
    static Vec3f sFireAuraPos;
    static Vec3f sIceAuraPos;
    static Vec3f sAuraVelocity = { 0.0f, 0.0f, 0.0f };
    static Vec3f sAuraAccel = { 0.0f, 0.0f, 0.0f };
    static Color_RGBA8 sFirePrimColor = { 255, 255, 100, 255 };
    static Color_RGBA8 sFireEnvColor = { 255, 50, 0, 0 };
    static Color_RGBA8 sIcePrimColor = { 100, 200, 255, 255 };
    static Color_RGBA8 sIceEnvColor = { 0, 0, 255, 0 };
    EnMolmauk* this = (EnMolmauk*)thisx;
    MtxF mtx;

    Matrix_Get(&mtx);
    switch (limbIndex) {
        case GHAMMERGEISTSKEL_HEAD_LIMB:
            Matrix_MultVec3f(&sZeroVec, &this->actor.focus.pos);
            break;
        case GHAMMERGEISTSKEL_HAMMERL_LIMB:
            EnMolmauk_PlaceHammer(&this->hammerLeftCollider, &sIceAuraPos, &mtx);
            break;
        case GHAMMERGEISTSKEL_HAMMERR_LIMB:
            EnMolmauk_PlaceHammer(&this->hammerRightCollider, &sFireAuraPos, &mtx);
            break;
        default:
            break;
    }
    if (this->rightHammerInfused) {
        func_8002843C(play, &sFireAuraPos, &sAuraVelocity, &sAuraAccel, &sFirePrimColor, &sFireEnvColor, 500, 50, 10);
    }
    if (this->leftHammerInfused) {
        func_8002843C(play, &sIceAuraPos, &sAuraVelocity, &sAuraAccel, &sIcePrimColor, &sIceEnvColor, 500, 50, 10);
    }
}

static s32 EnMolmauk_GetFlamePart(s32 limbIndex) {
    switch (limbIndex) {
        case GHAMMERGEISTSKEL_HEAD_LIMB:
            return 0;
        case GHAMMERGEISTSKEL_HAMMERL_LIMB:
            return 1;
        case GHAMMERGEISTSKEL_HAMMERR_LIMB:
            return 2;
        case GHAMMERGEISTSKEL_BODY_LIMB:
            return 3;
        case GHAMMERGEISTSKEL_HAND_L_LIMB:
            return 4;
        case GHAMMERGEISTSKEL_HAND_R_LIMB:
            return 5;
        case GHAMMERGEISTSKEL_FOOT_L_LIMB:
            return 6;
        case GHAMMERGEISTSKEL_FOOT_R_LIMB:
            return 7;
        case GHAMMERGEISTSKEL_ARM_L_LIMB:
            return 8;
        case GHAMMERGEISTSKEL_ARM_R_LIMB:
            return 9;
        default:
            return -1;
    }
}

// Same flame placement as the ReDead: a point 300 units along each burning limb.
static void EnMolmauk_DeadPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx,
                                       Gfx** gfx) {
    EnMolmauk* this = (EnMolmauk*)thisx;
    Vec3f limbOffset = { 300.0f, 0.0f, 0.0f };
    Vec3f flamePos;
    s32 part = EnMolmauk_GetFlamePart(limbIndex);

    if (this->fireTimer == 0 || part < 0) {
        return;
    }
    Matrix_MultVec3f(&limbOffset, &flamePos);
    this->firePos[part].x = flamePos.x;
    this->firePos[part].y = flamePos.y;
    this->firePos[part].z = flamePos.z;
}

static void EnMolmauk_SetSegments(Gfx** displayList, EnMolmauk* this) {
    gSPSegment((*displayList)++, 0x08, (uintptr_t)sFireHammerTextures[this->fireHammerIndex]);
    gSPSegment((*displayList)++, 0x09, (uintptr_t)sIceHammerTextures[this->iceHammerIndex]);
    gSPSegment((*displayList)++, 0x0A, (uintptr_t)sFaceTextures[this->faceIndex]);
    gSPSegment((*displayList)++, 0x0C, (uintptr_t)sEmptyDL);
}

static void EnMolmauk_SpawnDeathFlames(EnMolmauk* this, PlayState* play) {
    if (this->fireTimer == 0) {
        return;
    }
    this->actor.colorFilterTimer++;
    this->fireTimer--;
    if (this->fireTimer % 4 == 0) {
        EffectSsEnFire_SpawnVec3s(play, &this->actor, &this->firePos[this->fireTimer >> 2], 250, 0, 0,
                                  this->fireTimer >> 2);
    }
}

static void EnMolmauk_Draw(Actor* thisx, PlayState* play) {
    EnMolmauk* this = (EnMolmauk*)thisx;

    Collider_UpdateSpheres(0, &this->explosionCollider);

    OPEN_DISPS(play->state.gfxCtx);

    if (this->alpha == 255) {
        EnMolmauk_SetSegments(&POLY_OPA_DISP, this);
        func_80034BA0(play, &this->skelAnime, EnMolmauk_OverrideLimbDraw, EnMolmauk_PostLimbDraw, thisx, 255);
    } else {
        if (this->alpha != 0) {
            EnMolmauk_SetSegments(&POLY_XLU_DISP, this);
            func_80034CC4(play, &this->skelAnime, NULL, EnMolmauk_DeadPostLimbDraw, thisx, this->alpha);
        }
        EnMolmauk_SpawnDeathFlames(this, play);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterActor)) {
        return;
    }
    SOHActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = MOLMAUK_KEY;
    definition.description = "Molmauk";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                            ACTOR_FLAG_DRAW_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnMolmauk);
    definition.init = EnMolmauk_Init;
    definition.destroy = EnMolmauk_Destroy;
    definition.update = EnMolmauk_Update;
    definition.draw = EnMolmauk_Draw;
    // Sixteen hit points behind a guard that only drops between swings: too slow a fight for a timed room.
    definition.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS;
    definition.naviHint = "Molmauk&%cWatch out for its %whammers%c! Strike when it drops its guard!%w";

    if (sApi->RegisterActor(&definition) < 0) {
        sApi->Log("Could not register " MOLMAUK_KEY);
    }
}
