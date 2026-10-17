// Red-maned Lynel miniboss: sword, shield and a triple-shot bow. A headshot stuns it and lets Link ride it.
// Model from a Tripo mesh rigged and animated by blender_labs/lynel/build_lynel_assets.py.

#include "soh/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/effects/ovl_Effect_Ss_Hahen/z_eff_ss_hahen.h"
#include "overlays/effects/ovl_Effect_Ss_HitMark/z_eff_ss_hitmark.h"

#include "object_lynel_assets.h"
#include "object_lynel_assets.inc.c"

#define LYNEL_KEY "lynel.miniboss.red"

// Unbound leaves the update flags of Actor_UpdateBgCheckInfo unnamed.
#define BGCHECK_UPDATE_ALL ((1 << 0) | (1 << 2) | (1 << 3) | (1 << 4))

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000

#define LYNEL_HEALTH 40
#define LYNEL_BOW_PHASE_HEALTH 26
#define LYNEL_RAGE_PHASE_HEALTH 12
#define LYNEL_RIDER_DAMAGE 4
#define LYNEL_RIDER_MAX_HITS 3

#define LYNEL_WAKE_RANGE 450.0f
#define LYNEL_MELEE_RANGE 115.0f
#define LYNEL_COMBO_RANGE 150.0f
#define LYNEL_SHOOT_RANGE 330.0f
#define LYNEL_MOUNT_RANGE 75.0f
#define LYNEL_TURN_RANGE 260.0f
#define LYNEL_LEAP_MIN_RANGE 170.0f
#define LYNEL_LEAP_MAX_RANGE 330.0f
#define LYNEL_RAIN_RANGE 480.0f
#define LYNEL_RIDER_DROP 27.0f
#define LYNEL_MAX_CHARGES 2

// Speeds match the stride of each gait animation so the hooves do not skate.
#define LYNEL_WALK_SPEED 2.6f
#define LYNEL_GALLOP_SPEED 9.0f
#define LYNEL_CHARGE_SPEED 13.0f
#define LYNEL_HIND_PIVOT_DIST 22.0f

#define LYNEL_MAX_PROJECTILES 16
#define LYNEL_ARROW_SPEED 22.0f
#define LYNEL_ARROW_SPREAD 0x600
#define LYNEL_RAIN_ARROWS 5
#define LYNEL_RAIN_HEIGHT 450.0f
#define LYNEL_RAIN_SCATTER 60.0f
#define LYNEL_FIREBALL_SPEED 16.0f
#define LYNEL_FIREBALL_RANGE 520.0f
#define LYNEL_BLAST_RADIUS 70.0f
#define LYNEL_BLAST_FRAMES 14
#define LYNEL_BLAST_MESH_RADIUS 1000.0f

typedef enum {
    LYNEL_PHASE_SWORD,
    LYNEL_PHASE_BOW,
    LYNEL_PHASE_RAGE,
} LynelPhase;

typedef enum {
    LYNEL_DMGEFF_NONE,
    LYNEL_DMGEFF_STUN,
    LYNEL_DMGEFF_FIRE,
    LYNEL_DMGEFF_ICE,
    LYNEL_DMGEFF_LIGHT,
} LynelDamageEffect;

typedef enum {
    LYNEL_PROJECTILE_NONE,
    LYNEL_PROJECTILE_ARROW,
    LYNEL_PROJECTILE_RAIN_ARROW,
    LYNEL_PROJECTILE_FIREBALL,
    LYNEL_PROJECTILE_BLAST,
    LYNEL_PROJECTILE_FLAME,
} LynelProjectileType;

typedef enum {
    LYNEL_SPHERE_HEAD,
    LYNEL_SPHERE_CHEST,
    LYNEL_SPHERE_WAIST,
    LYNEL_SPHERE_BARREL_FRONT,
    LYNEL_SPHERE_BARREL_BACK,
    LYNEL_SPHERE_FL_THIGH,
    LYNEL_SPHERE_FR_THIGH,
    LYNEL_SPHERE_BL_THIGH,
    LYNEL_SPHERE_BR_THIGH,
    LYNEL_SPHERE_MAX,
} LynelBodySphere;

typedef struct {
    Vec3f pos;
    Vec3f velocity;
    f32 travelled;
    s16 pitch;
    s16 life;
    u8 type;
} LynelProjectile;

struct EnLynel;

typedef void (*EnLynelActionFunc)(struct EnLynel*, PlayState*);

typedef struct EnLynel {
    Actor actor;
    Vec3s jointTable[LYNEL_NUM_LIMBS];
    Vec3s morphTable[LYNEL_NUM_LIMBS];
    SkelAnime skelAnime;
    ColliderJntSph bodyCollider;
    ColliderJntSphElement bodyElements[LYNEL_SPHERE_MAX];
    ColliderJntSph shieldCollider;
    ColliderJntSphElement shieldElements[1];
    ColliderQuad swordCollider;
    ColliderCylinder ramCollider;
    ColliderJntSph projectileCollider;
    ColliderJntSphElement projectileElements[LYNEL_MAX_PROJECTILES];
    LynelProjectile projectiles[LYNEL_MAX_PROJECTILES];
    EnLynelActionFunc actionFunc;
    Vec3f seatPos;
    Vec3f mouthPos;
    Vec3f nockPos;
    Vec3f shieldPos;
    s16 timer;
    s16 hurtboxCooldown;
    s16 headYaw;
    s16 turtleTimer;
    s16 riderHits;
    s16 rageFlashTimer;
    u8 phase;
    u8 hasShield;
    u8 isBowInHand;
    u8 isSwordActive;
    u8 isRamActive;
    u8 isFightStarted;
    u8 comboStep;
    u8 chargeCount;
    u8 blockCount;
} EnLynel;

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static EnLynel* sRiddenLynel;
static s16 sRiderSlashTimer;
static u8 sRiderSlashCount;

s32 Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_InitItemAction(PlayState* play, Player* player, s8 itemAction);
s8 Player_ItemToItemAction(s32 item);
void Player_AnimPlayLoop(PlayState* play, Player* player, LinkAnimationHeader* anim);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
void func_80837C0C(PlayState* play, Player* player, s32 damageResponseType, f32 speed, f32 yVelocity, s16 yRot,
                   s32 invincibilityTimer);

static void EnLynel_Decide(EnLynel* this, PlayState* play);
static bool EnLynel_UpdatePhase(EnLynel* this, PlayState* play);
static void EnLynel_SetupStun(EnLynel* this, PlayState* play, s16 duration);
static void EnLynel_RiderAction(Player* player, PlayState* play);

// Centres are in limb model space; radii are already world units (scale 100 = x1).
#define LYNEL_BODY_SPHERE(limb, x, y, z, radius, bumperFlags)                                                          \
    {                                                                                                                  \
        { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, bumperFlags, OCELEM_ON }, \
        {                                                                                                              \
            limb, { { x, y, z }, radius }, 100                                                                         \
        }                                                                                                              \
    }

static ColliderJntSphElementInit sBodyElementsInit[LYNEL_SPHERE_MAX] = {
    LYNEL_BODY_SPHERE(LYNEL_LIMB_HEAD, 0.0f, 0.0f, 0.0f, 14.0f, BUMP_ON | BUMP_HOOKABLE),
    LYNEL_BODY_SPHERE(LYNEL_LIMB_CHEST, 0.0f, 600.0f, 0.0f, 20.0f, BUMP_ON | BUMP_HOOKABLE),
    LYNEL_BODY_SPHERE(LYNEL_LIMB_WAIST, 0.0f, 200.0f, 0.0f, 18.0f, BUMP_ON | BUMP_HOOKABLE),
    LYNEL_BODY_SPHERE(LYNEL_LIMB_ROOT, 0.0f, 0.0f, 1500.0f, 22.0f, BUMP_ON | BUMP_HOOKABLE),
    LYNEL_BODY_SPHERE(LYNEL_LIMB_ROOT, 0.0f, 0.0f, -1500.0f, 22.0f, BUMP_ON | BUMP_HOOKABLE),
    LYNEL_BODY_SPHERE(LYNEL_LIMB_FL_THIGH, 0.0f, -600.0f, 0.0f, 10.0f, BUMP_ON),
    LYNEL_BODY_SPHERE(LYNEL_LIMB_FR_THIGH, 0.0f, -600.0f, 0.0f, 10.0f, BUMP_ON),
    LYNEL_BODY_SPHERE(LYNEL_LIMB_BL_THIGH, 0.0f, -600.0f, 0.0f, 10.0f, BUMP_ON),
    LYNEL_BODY_SPHERE(LYNEL_LIMB_BR_THIGH, 0.0f, -600.0f, 0.0f, 10.0f, BUMP_ON),
};

static ColliderJntSphInit sBodyInit = {
    { COLTYPE_HIT0, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_JNTSPH },
    LYNEL_SPHERE_MAX,
    sBodyElementsInit,
};

static ColliderJntSphElementInit sShieldElementsInit[1] = {
    { { ELEMTYPE_UNK2, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_NONE },
      { LYNEL_LIMB_L_FOREARM, { { 0.0f, 0.0f, 0.0f }, 17.0f }, 100 } },
};

static ColliderJntSphInit sShieldInit = {
    { COLTYPE_METAL, AT_NONE, AC_ON | AC_HARD | AC_TYPE_PLAYER, OC1_NONE, OC2_NONE, COLSHAPE_JNTSPH },
    1,
    sShieldElementsInit,
};

static ColliderQuadInit sSwordInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_QUAD },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x18 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
      BUMP_NONE,
      OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static ColliderCylinderInit sRamInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x10 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_HARD,
      BUMP_NONE,
      OCELEM_NONE },
    { 40, 80, 0, { 0, 0, 0 } },
};

static ColliderJntSphElementInit sProjectileElementInit = {
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x10 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_NONE,
      BUMP_NONE,
      OCELEM_NONE },
    { 0, { { 0.0f, 0.0f, 0.0f }, 8.0f }, 100 },
};

static ColliderJntSphElementInit sProjectileElementsInit[LYNEL_MAX_PROJECTILES];

static ColliderJntSphInit sProjectileInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_JNTSPH },
    LYNEL_MAX_PROJECTILES,
    sProjectileElementsInit,
};

static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, LYNEL_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, LYNEL_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, LYNEL_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(4, LYNEL_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, LYNEL_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, LYNEL_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(4, LYNEL_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, LYNEL_DMGEFF_NONE),
    /* Kokiri sword  */ DMG_ENTRY(1, LYNEL_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, LYNEL_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, LYNEL_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(1, LYNEL_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, LYNEL_DMGEFF_ICE),
    /* Light arrow   */ DMG_ENTRY(6, LYNEL_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, LYNEL_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, LYNEL_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, LYNEL_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(1, LYNEL_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(3, LYNEL_DMGEFF_ICE),
    /* Light magic   */ DMG_ENTRY(6, LYNEL_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, LYNEL_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, LYNEL_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(1, LYNEL_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(4, LYNEL_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(2, LYNEL_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, LYNEL_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, LYNEL_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, LYNEL_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, LYNEL_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, LYNEL_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(6, LYNEL_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, LYNEL_DMGEFF_NONE),
};

static CollisionCheckInfoInit2 sColChkInit = { LYNEL_HEALTH, 40, 100, 0, MASS_IMMOVABLE };

// Upper-body limbs of Link that follow the slash while his legs keep the riding pose.
static u8 sRiderUpperBodyLimbs[PLAYER_LIMB_MAX] = {
    [PLAYER_LIMB_UPPER] = true,  [PLAYER_LIMB_HEAD] = true,       [PLAYER_LIMB_HAT] = true,
    [PLAYER_LIMB_COLLAR] = true, [PLAYER_LIMB_L_SHOULDER] = true, [PLAYER_LIMB_L_FOREARM] = true,
    [PLAYER_LIMB_L_HAND] = true, [PLAYER_LIMB_R_SHOULDER] = true, [PLAYER_LIMB_R_FOREARM] = true,
    [PLAYER_LIMB_R_HAND] = true, [PLAYER_LIMB_SHEATH] = true,     [PLAYER_LIMB_TORSO] = true,
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static void EnLynel_SetupAction(EnLynel* this, EnLynelActionFunc actionFunc) {
    this->actionFunc = actionFunc;
    this->isSwordActive = false;
    this->isRamActive = false;
}

static void EnLynel_PlayAnim(EnLynel* this, AnimationHeader* anim, u8 mode, f32 morph) {
    Animation_Change(&this->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), mode, morph);
}

static Player* EnLynel_GetPlayer(PlayState* play) {
    return GET_PLAYER(play);
}

static bool EnLynel_IsRidden(EnLynel* this, PlayState* play) {
    return sRiddenLynel == this && EnLynel_GetPlayer(play)->actionFunc == EnLynel_RiderAction;
}

static void EnLynel_TurnToward(EnLynel* this, s16 targetYaw, s16 step) {
    Math_SmoothStepToS(&this->actor.world.rot.y, targetYaw, 3, step, 0x40);
    this->actor.shape.rot.y = this->actor.world.rot.y;
}

static void EnLynel_TurnTowardPlayer(EnLynel* this, s16 step) {
    EnLynel_TurnToward(this, this->actor.yawTowardsPlayer, step);
}

static void EnLynel_PlaySfx(EnLynel* this, u16 sfxId) {
    Audio_PlayActorSound2(&this->actor, sfxId);
}

static void EnLynel_Quake(PlayState* play, s16 strength, s16 duration) {
    s16 quake = Quake_Add(GET_ACTIVE_CAM(play), 3);

    Quake_SetSpeed(quake, 25000);
    Quake_SetQuakeValues(quake, strength, 0, 0, 0);
    Quake_SetCountdown(quake, duration);
}

static void EnLynel_StartMusic(EnLynel* this) {
    if (this->isFightStarted) {
        return;
    }
    this->isFightStarted = true;
    func_800F5ACC(NA_BGM_MINI_BOSS);
}

static void EnLynel_SpawnFlameBurst(PlayState* play, Vec3f* pos, s16 count, f32 spread) {
    static Vec3f sRise = { 0.0f, 0.6f, 0.0f };

    for (s16 i = 0; i < count; i++) {
        Vec3f flamePos = *pos;
        Vec3f velocity = { Rand_CenteredFloat(1.0f), Rand_ZeroFloat(1.5f), Rand_CenteredFloat(1.0f) };

        flamePos.x += Rand_CenteredFloat(spread);
        flamePos.z += Rand_CenteredFloat(spread);
        EffectSsKFire_Spawn(play, &flamePos, &velocity, &sRise, 20 + Rand_ZeroFloat(15.0f), 0);
    }
}

static LynelProjectile* EnLynel_FreeProjectile(EnLynel* this) {
    for (s32 i = 0; i < LYNEL_MAX_PROJECTILES; i++) {
        if (this->projectiles[i].type == LYNEL_PROJECTILE_NONE) {
            return &this->projectiles[i];
        }
    }
    return NULL;
}

static void EnLynel_Launch(EnLynel* this, u8 type, Vec3f* from, s16 yaw, s16 pitch, f32 speed, s16 life) {
    LynelProjectile* projectile = EnLynel_FreeProjectile(this);

    if (projectile == NULL) {
        return;
    }
    projectile->type = type;
    projectile->pos = *from;
    projectile->travelled = 0.0f;
    projectile->pitch = pitch;
    projectile->life = life;
    projectile->velocity.x = speed * Math_CosS(pitch) * Math_SinS(yaw);
    projectile->velocity.y = -speed * Math_SinS(pitch);
    projectile->velocity.z = speed * Math_CosS(pitch) * Math_CosS(yaw);
}

static void EnLynel_IgniteGround(EnLynel* this, Vec3f* pos) {
    LynelProjectile* flame = EnLynel_FreeProjectile(this);

    if (flame == NULL) {
        return;
    }
    flame->type = LYNEL_PROJECTILE_FLAME;
    flame->pos = *pos;
    flame->velocity = sZeroVec;
    flame->life = 70;
}

static void EnLynel_ShootArrowFan(EnLynel* this, PlayState* play) {
    Player* player = EnLynel_GetPlayer(play);
    Vec3f target = player->actor.world.pos;

    target.y += 25.0f;
    s16 yaw = Math_Vec3f_Yaw(&this->nockPos, &target);
    s16 pitch = Math_Vec3f_Pitch(&this->nockPos, &target);

    for (s32 i = -1; i <= 1; i++) {
        EnLynel_Launch(this, LYNEL_PROJECTILE_ARROW, &this->nockPos, yaw + i * LYNEL_ARROW_SPREAD, pitch,
                       LYNEL_ARROW_SPEED, 40);
    }
    EnLynel_PlaySfx(this, NA_SE_IT_ARROW_SHOT);
}

static void EnLynel_ShootArrowRain(EnLynel* this) {
    for (s32 i = 0; i < LYNEL_RAIN_ARROWS; i++) {
        s16 pitch = -0x3800 - (s16)Rand_ZeroFloat(0x600);

        EnLynel_Launch(this, LYNEL_PROJECTILE_RAIN_ARROW, &this->nockPos, Rand_Next() & 0xFFFF, pitch, 26.0f + i * 3.0f,
                       90);
    }
    EnLynel_PlaySfx(this, NA_SE_IT_ARROW_SHOT);
}

static void EnLynel_BreatheFireball(EnLynel* this, PlayState* play, s16 yawOffset) {
    Player* player = EnLynel_GetPlayer(play);
    s16 yaw = Math_Vec3f_Yaw(&this->mouthPos, &player->actor.world.pos) + yawOffset;
    s16 pitch = Math_Vec3f_Pitch(&this->mouthPos, &player->actor.world.pos);

    EnLynel_Launch(this, LYNEL_PROJECTILE_FIREBALL, &this->mouthPos, yaw, pitch, LYNEL_FIREBALL_SPEED, 60);
    EnLynel_PlaySfx(this, NA_SE_EN_DODO_K_FIRE);
}

static void EnLynel_RemoveProjectile(LynelProjectile* projectile, ColliderJntSphElement* element) {
    projectile->type = LYNEL_PROJECTILE_NONE;
    element->info.toucherFlags &= ~TOUCH_ON;
}

static void EnLynel_Detonate(PlayState* play, LynelProjectile* fireball) {
    fireball->type = LYNEL_PROJECTILE_BLAST;
    fireball->life = LYNEL_BLAST_FRAMES;
    fireball->velocity = sZeroVec;
    SoundSource_PlaySfxAtFixedWorldPos(play, &fireball->pos, 30, NA_SE_IT_BOMB_EXPLOSION);
    EnLynel_SpawnFlameBurst(play, &fireball->pos, 10, 40.0f);
}

static f32 EnLynel_GetBlastRadius(LynelProjectile* blast) {
    return LYNEL_BLAST_RADIUS * sqrtf(1.0f - (f32)blast->life / LYNEL_BLAST_FRAMES);
}

static void EnLynel_UpdateBlast(PlayState* play, LynelProjectile* blast) {
    f32 radius = EnLynel_GetBlastRadius(blast);

    for (s32 i = 0; i < 3; i++) {
        s16 angle = Rand_Next() & 0xFFFF;
        Vec3f flamePos = blast->pos;

        flamePos.x += radius * Math_SinS(angle);
        flamePos.z += radius * Math_CosS(angle);
        EnLynel_SpawnFlameBurst(play, &flamePos, 1, 10.0f);
    }
}

// Rain arrows leave the screen upwards and come back down on wherever Link stands at that moment.
static void EnLynel_DropRainArrow(PlayState* play, LynelProjectile* arrow) {
    Player* player = EnLynel_GetPlayer(play);

    arrow->pos = player->actor.world.pos;
    arrow->pos.x += Rand_CenteredFloat(2.0f * LYNEL_RAIN_SCATTER);
    arrow->pos.z += Rand_CenteredFloat(2.0f * LYNEL_RAIN_SCATTER);
    arrow->pos.y += LYNEL_RAIN_HEIGHT;
    arrow->velocity.x = 0.0f;
    arrow->velocity.y = -12.0f;
    arrow->velocity.z = 0.0f;
}

static bool EnLynel_IsRainArrowOutOfSight(EnLynel* this, LynelProjectile* arrow) {
    return arrow->velocity.y > 0.0f && arrow->pos.y > this->actor.world.pos.y + LYNEL_RAIN_HEIGHT;
}

static void EnLynel_FlyProjectile(EnLynel* this, PlayState* play, LynelProjectile* projectile,
                                  ColliderJntSphElement* element) {
    Vec3f next;
    Vec3f hitPos;
    CollisionPoly* poly;
    s32 bgId;
    bool isFireball = projectile->type == LYNEL_PROJECTILE_FIREBALL;

    if (projectile->type == LYNEL_PROJECTILE_RAIN_ARROW) {
        if (EnLynel_IsRainArrowOutOfSight(this, projectile)) {
            EnLynel_DropRainArrow(play, projectile);
            return;
        }
        if (projectile->velocity.y < 0.0f) {
            projectile->velocity.y -= 1.0f;
        }
    }
    if (isFireball) {
        EnLynel_SpawnFlameBurst(play, &projectile->pos, 1, 6.0f);
    }
    Math_Vec3f_Sum(&projectile->pos, &projectile->velocity, &next);
    if (BgCheck_EntityLineTest1(&play->colCtx, &projectile->pos, &next, &hitPos, &poly, true, true, true, true,
                                &bgId)) {
        if (isFireball) {
            projectile->pos = hitPos;
            EnLynel_Detonate(play, projectile);
            EnLynel_IgniteGround(this, &hitPos);
        } else {
            EnLynel_RemoveProjectile(projectile, element);
        }
        return;
    }
    projectile->pos = next;
    projectile->travelled += Math3D_Vec3fMagnitude(&projectile->velocity);
    projectile->pitch =
        Math_Atan2S(sqrtf(SQ(projectile->velocity.x) + SQ(projectile->velocity.z)), -projectile->velocity.y);
    if (isFireball && projectile->travelled >= LYNEL_FIREBALL_RANGE) {
        EnLynel_Detonate(play, projectile);
    }
}

static f32 EnLynel_GetProjectileRadius(LynelProjectile* projectile) {
    switch (projectile->type) {
        case LYNEL_PROJECTILE_FLAME:
            return 22.0f;
        case LYNEL_PROJECTILE_BLAST:
            return EnLynel_GetBlastRadius(projectile);
        default:
            return 9.0f;
    }
}

static void EnLynel_UpdateProjectile(EnLynel* this, PlayState* play, s32 index) {
    LynelProjectile* projectile = &this->projectiles[index];
    ColliderJntSphElement* element = &this->projectileElements[index];
    bool hasHit = (element->info.toucherFlags & TOUCH_HIT) != 0;

    switch (projectile->type) {
        case LYNEL_PROJECTILE_NONE:
            return;
        case LYNEL_PROJECTILE_ARROW:
        case LYNEL_PROJECTILE_RAIN_ARROW:
            if (hasHit || --projectile->life <= 0) {
                EnLynel_RemoveProjectile(projectile, element);
                return;
            }
            EnLynel_FlyProjectile(this, play, projectile, element);
            break;
        case LYNEL_PROJECTILE_FIREBALL:
            if (hasHit) {
                EnLynel_Detonate(play, projectile);
            } else if (--projectile->life <= 0) {
                EnLynel_RemoveProjectile(projectile, element);
                return;
            } else {
                EnLynel_FlyProjectile(this, play, projectile, element);
            }
            break;
        case LYNEL_PROJECTILE_BLAST:
            if (--projectile->life <= 0) {
                EnLynel_RemoveProjectile(projectile, element);
                return;
            }
            EnLynel_UpdateBlast(play, projectile);
            break;
        case LYNEL_PROJECTILE_FLAME:
            if (--projectile->life <= 0) {
                EnLynel_RemoveProjectile(projectile, element);
                return;
            }
            if ((projectile->life % 3) == 0) {
                EnLynel_SpawnFlameBurst(play, &projectile->pos, 1, 40.0f);
            }
            break;
        default:
            EnLynel_RemoveProjectile(projectile, element);
            return;
    }
    if (projectile->type == LYNEL_PROJECTILE_NONE) {
        return;
    }

    bool isFire = projectile->type != LYNEL_PROJECTILE_ARROW && projectile->type != LYNEL_PROJECTILE_RAIN_ARROW;

    element->info.toucherFlags |= TOUCH_ON;
    element->info.toucher.effect = isFire ? HIT_SPECIAL_EFFECT_FIRE : HIT_SPECIAL_EFFECT_NONE;
    element->dim.worldSphere.center = projectile->pos;
    element->dim.worldSphere.radius = EnLynel_GetProjectileRadius(projectile);
    if (projectile->type == LYNEL_PROJECTILE_FLAME) {
        element->dim.worldSphere.center.y += 10.0f;
    }
}

static void EnLynel_UpdateProjectiles(EnLynel* this, PlayState* play) {
    bool isAnyActive = false;

    for (s32 i = 0; i < LYNEL_MAX_PROJECTILES; i++) {
        EnLynel_UpdateProjectile(this, play, i);
        isAnyActive |= this->projectiles[i].type != LYNEL_PROJECTILE_NONE;
    }
    if (isAnyActive) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->projectileCollider.base);
    }
}

static bool EnLynel_IsPlayerFacingAndShielding(EnLynel* this, Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_SHIELDING) && Actor_IsFacingPlayer(&this->actor, 0x2000);
}

static bool EnLynel_IsPlayerSwinging(EnLynel* this, Player* player) {
    return player->meleeWeaponState != 0 && this->actor.xzDistToPlayer < LYNEL_COMBO_RANGE &&
           Actor_IsFacingPlayer(&this->actor, 0x2000);
}

static void EnLynel_Idle(EnLynel* this, PlayState* play);
static void EnLynel_Roar(EnLynel* this, PlayState* play);
static void EnLynel_Approach(EnLynel* this, PlayState* play);
static void EnLynel_Slash(EnLynel* this, PlayState* play);
static void EnLynel_Spin(EnLynel* this, PlayState* play);
static void EnLynel_Bash(EnLynel* this, PlayState* play);
static void EnLynel_Guard(EnLynel* this, PlayState* play);
static void EnLynel_BlockHit(EnLynel* this, PlayState* play);
static void EnLynel_Turn(EnLynel* this, PlayState* play);
static void EnLynel_Charge(EnLynel* this, PlayState* play);
static void EnLynel_Wheel(EnLynel* this, PlayState* play);
static void EnLynel_Leap(EnLynel* this, PlayState* play);
static void EnLynel_Backstep(EnLynel* this, PlayState* play);
static void EnLynel_Breath(EnLynel* this, PlayState* play);
static void EnLynel_Swap(EnLynel* this, PlayState* play);
static void EnLynel_Shoot(EnLynel* this, PlayState* play);
static void EnLynel_Rain(EnLynel* this, PlayState* play);
static void EnLynel_Volley(EnLynel* this, PlayState* play);
static void EnLynel_Stunned(EnLynel* this, PlayState* play);
static void EnLynel_Damaged(EnLynel* this, PlayState* play);
static void EnLynel_Ridden(EnLynel* this, PlayState* play);
static void EnLynel_Buck(EnLynel* this, PlayState* play);
static void EnLynel_Die(EnLynel* this, PlayState* play);

static bool EnLynel_HasHyperArmor(EnLynel* this) {
    return this->actionFunc == EnLynel_Slash || this->actionFunc == EnLynel_Spin || this->actionFunc == EnLynel_Bash ||
           this->actionFunc == EnLynel_Charge || this->actionFunc == EnLynel_Breath ||
           this->actionFunc == EnLynel_Roar || this->actionFunc == EnLynel_Buck || this->actionFunc == EnLynel_Shoot ||
           this->actionFunc == EnLynel_Volley || this->actionFunc == EnLynel_Leap ||
           this->actionFunc == EnLynel_Wheel || this->actionFunc == EnLynel_Rain;
}

static bool EnLynel_IsGuarding(EnLynel* this) {
    return this->hasShield && (this->actionFunc == EnLynel_Guard || this->actionFunc == EnLynel_BlockHit);
}

static s16 EnLynel_GetYawToPlayer(EnLynel* this) {
    return this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
}

// Turns swing the body around the hind hooves, as a horse turns on its haunches.
static void EnLynel_PivotToward(EnLynel* this, s16 targetYaw, s16 step) {
    f32 pivotX = this->actor.world.pos.x - LYNEL_HIND_PIVOT_DIST * Math_SinS(this->actor.shape.rot.y);
    f32 pivotZ = this->actor.world.pos.z - LYNEL_HIND_PIVOT_DIST * Math_CosS(this->actor.shape.rot.y);

    Math_ScaledStepToS(&this->actor.world.rot.y, targetYaw, step);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    this->actor.world.pos.x = pivotX + LYNEL_HIND_PIVOT_DIST * Math_SinS(this->actor.shape.rot.y);
    this->actor.world.pos.z = pivotZ + LYNEL_HIND_PIVOT_DIST * Math_CosS(this->actor.shape.rot.y);
}

static void EnLynel_SetupIdle(EnLynel* this) {
    this->actor.speedXZ = 0.0f;
    EnLynel_PlayAnim(this, &gLynelIdleAnim, ANIMMODE_LOOP, -8.0f);
    EnLynel_SetupAction(this, EnLynel_Idle);
}

static void EnLynel_Idle(EnLynel* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.xzDistToPlayer < LYNEL_WAKE_RANGE) {
        EnLynel_SetupAction(this, EnLynel_Roar);
        EnLynel_PlayAnim(this, &gLynelRoarAnim, ANIMMODE_ONCE, -6.0f);
    }
}

static void EnLynel_BreakShield(EnLynel* this, PlayState* play) {
    this->hasShield = false;
    EffectSsHahen_SpawnBurst(play, &this->shieldPos, 6.0f, 0, 25, 12, 16, HAHEN_OBJECT_DEFAULT, 10, NULL);
    EnLynel_PlaySfx(this, NA_SE_EN_IRONNACK_ARMOR_HIT);
}

static void EnLynel_SetupRoar(EnLynel* this) {
    this->actor.speedXZ = 0.0f;
    EnLynel_PlayAnim(this, &gLynelRoarAnim, ANIMMODE_ONCE, -6.0f);
    EnLynel_SetupAction(this, EnLynel_Roar);
}

static void EnLynel_Roar(EnLynel* this, PlayState* play) {
    EnLynel_TurnTowardPlayer(this, 0x400);
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_ROAR_ROAR)) {
        EnLynel_StartMusic(this);
        EnLynel_PlaySfx(this, NA_SE_EN_DODO_K_OTAKEBI);
        EnLynel_Quake(play, 4, 20);
        if (this->actor.xzDistToPlayer < 160.0f) {
            Actor_SetPlayerKnockbackSmallNoDamage(play, &this->actor, 6.0f, this->actor.yawTowardsPlayer, 4.0f);
        }
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_SetupApproach(EnLynel* this) {
    this->timer = 80;
    EnLynel_PlayAnim(this, &gLynelWalkAnim, ANIMMODE_LOOP, -6.0f);
    EnLynel_SetupAction(this, EnLynel_Approach);
}

static void EnLynel_SetupGuard(EnLynel* this, s16 duration) {
    this->actor.speedXZ = 0.0f;
    this->timer = duration;
    EnLynel_PlayAnim(this, &gLynelGuardAnim, ANIMMODE_LOOP, -3.0f);
    EnLynel_SetupAction(this, EnLynel_Guard);
}

static bool EnLynel_IsPlayerAimingAt(EnLynel* this, Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) && Actor_IsFacingPlayer(&this->actor, 0x2000);
}

static bool EnLynel_ShouldRaiseShield(EnLynel* this, Player* player) {
    if (!this->hasShield) {
        return false;
    }
    if (EnLynel_IsPlayerSwinging(this, player)) {
        return Rand_ZeroOne() < 0.1f;
    }
    return EnLynel_IsPlayerAimingAt(this, player) && Rand_ZeroOne() < 0.04f;
}

static void EnLynel_Approach(EnLynel* this, PlayState* play) {
    Player* player = EnLynel_GetPlayer(play);
    bool shouldGallop = this->actor.xzDistToPlayer > 220.0f;
    AnimationHeader* anim = shouldGallop ? &gLynelGallopAnim : &gLynelWalkAnim;
    f32 gaitSpeed = shouldGallop ? LYNEL_GALLOP_SPEED : LYNEL_WALK_SPEED;

    if (this->skelAnime.animation != anim) {
        EnLynel_PlayAnim(this, anim, ANIMMODE_LOOP, -6.0f);
    }
    Math_ApproachF(&this->actor.speedXZ, gaitSpeed, 0.3f, 1.0f);
    this->skelAnime.playSpeed = CLAMP(this->actor.speedXZ / gaitSpeed, 0.5f, 1.2f);
    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, shouldGallop ? LYNEL_ANIM_GALLOP_STEP_A : LYNEL_ANIM_WALK_STEP_A) ||
        Animation_OnFrame(&this->skelAnime, shouldGallop ? LYNEL_ANIM_GALLOP_STEP_B : LYNEL_ANIM_WALK_STEP_B)) {
        EnLynel_PlaySfx(this, shouldGallop ? NA_SE_EV_HORSE_RUN : NA_SE_EN_MORIBLIN_WALK);
    }
    EnLynel_TurnTowardPlayer(this, 0x500);

    if (EnLynel_ShouldRaiseShield(this, player)) {
        EnLynel_SetupGuard(this, 30);
        return;
    }
    if (this->actor.xzDistToPlayer < LYNEL_MELEE_RANGE || DECR(this->timer) == 0) {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_SetupTurn(EnLynel* this) {
    this->actor.speedXZ = 0.0f;
    this->timer = 30;
    EnLynel_PlayAnim(this, &gLynelTurnAnim, ANIMMODE_LOOP, -4.0f);
    EnLynel_SetupAction(this, EnLynel_Turn);
}

static void EnLynel_Turn(EnLynel* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_TURN_STEP_A) ||
        Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_TURN_STEP_B)) {
        EnLynel_PlaySfx(this, NA_SE_EN_MORIBLIN_WALK);
    }
    EnLynel_PivotToward(this, this->actor.yawTowardsPlayer, 0x700);
    if (ABS(EnLynel_GetYawToPlayer(this)) < 0x800 || DECR(this->timer) == 0) {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_SetupSlash(EnLynel* this, u8 step) {
    this->comboStep = step;
    this->actor.speedXZ = 0.0f;
    this->swordCollider.info.toucher.damage = (step == 0) ? 0x18 : 0x14;
    EnLynel_PlayAnim(this, (step == 0) ? &gLynelSlash1Anim : &gLynelSlash2Anim, ANIMMODE_ONCE, -3.0f);
    EnLynel_SetupAction(this, EnLynel_Slash);
}

static void EnLynel_SetupSpin(EnLynel* this) {
    this->actor.speedXZ = 0.0f;
    this->swordCollider.info.toucher.damage = 0x20;
    EnLynel_PlayAnim(this, &gLynelSpinAnim, ANIMMODE_ONCE, -3.0f);
    EnLynel_SetupAction(this, EnLynel_Spin);
}

static void EnLynel_Slash(EnLynel* this, PlayState* play) {
    f32 hitStart = (this->comboStep == 0) ? LYNEL_ANIM_SLASH1_HIT_START : LYNEL_ANIM_SLASH2_HIT_START;
    f32 hitEnd = (this->comboStep == 0) ? LYNEL_ANIM_SLASH1_HIT_END : LYNEL_ANIM_SLASH2_HIT_END;
    f32 frame = this->skelAnime.curFrame;

    if (frame < hitStart) {
        EnLynel_TurnTowardPlayer(this, 0x600);
    }
    if (Animation_OnFrame(&this->skelAnime, hitStart)) {
        this->actor.speedXZ = 7.0f;
        EnLynel_PlaySfx(this, NA_SE_EN_IRONNACK_SWING_AXE);
    }
    Math_StepToF(&this->actor.speedXZ, 0.0f, 0.8f);
    this->isSwordActive = frame >= hitStart && frame <= hitEnd;

    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (this->comboStep == 0 && this->actor.xzDistToPlayer < LYNEL_COMBO_RANGE) {
        EnLynel_SetupSlash(this, 1);
    } else if (this->comboStep == 1 && this->phase != LYNEL_PHASE_SWORD && Rand_ZeroOne() < 0.6f) {
        EnLynel_SetupSpin(this);
    } else {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_Spin(EnLynel* this, PlayState* play) {
    f32 frame = this->skelAnime.curFrame;
    bool isSpinning = frame >= LYNEL_ANIM_SPIN_HIT_START && frame < LYNEL_ANIM_SPIN_HIT_END;

    if (frame < LYNEL_ANIM_SPIN_HIT_START) {
        EnLynel_TurnTowardPlayer(this, 0x600);
    }
    if (isSpinning) {
        this->actor.shape.rot.y += 0x10000 / (LYNEL_ANIM_SPIN_HIT_END - LYNEL_ANIM_SPIN_HIT_START);
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        this->actor.speedXZ = 3.0f;
        if (((s32)frame % 4) == 0) {
            EnLynel_PlaySfx(this, NA_SE_IT_SWORD_SWING_HARD);
        }
    } else {
        this->actor.speedXZ = 0.0f;
    }
    this->isSwordActive = isSpinning;
    if (SkelAnime_Update(&this->skelAnime)) {
        this->actor.world.rot.y = this->actor.shape.rot.y;
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_SetupBash(EnLynel* this) {
    this->actor.speedXZ = 0.0f;
    this->turtleTimer = 0;
    EnLynel_PlayAnim(this, &gLynelBashAnim, ANIMMODE_ONCE, -3.0f);
    EnLynel_SetupAction(this, EnLynel_Bash);
}

static void EnLynel_Bash(EnLynel* this, PlayState* play) {
    f32 frame = this->skelAnime.curFrame;

    if (frame < LYNEL_ANIM_BASH_HIT_START) {
        EnLynel_TurnTowardPlayer(this, 0x800);
    }
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_BASH_HIT_START)) {
        this->actor.speedXZ = 9.0f;
        EnLynel_PlaySfx(this, NA_SE_EN_DODO_K_COLI);
    }
    Math_StepToF(&this->actor.speedXZ, 0.0f, 1.0f);
    this->isRamActive = frame >= LYNEL_ANIM_BASH_HIT_START && frame <= LYNEL_ANIM_BASH_HIT_END;
    if (SkelAnime_Update(&this->skelAnime)) {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_Guard(EnLynel* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    EnLynel_TurnTowardPlayer(this, 0x700);
    if (DECR(this->timer) != 0) {
        return;
    }
    if (this->actor.xzDistToPlayer > LYNEL_COMBO_RANGE) {
        EnLynel_Decide(this, play);
    } else if (this->actor.xzDistToPlayer < LYNEL_MELEE_RANGE && (this->blockCount > 0 || Rand_ZeroOne() < 0.5f)) {
        EnLynel_SetupBash(this);
    } else {
        EnLynel_SetupSlash(this, 0);
    }
}

static void EnLynel_Block(EnLynel* this, PlayState* play) {
    this->blockCount++;
    EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_METAL, &this->shieldPos);
    EnLynel_PlaySfx(this, NA_SE_IT_SHIELD_REFLECT_SW);
    if (this->actor.xzDistToPlayer < LYNEL_COMBO_RANGE) {
        Actor_SetPlayerKnockbackSmallNoDamage(play, &this->actor, 4.0f, this->actor.yawTowardsPlayer, 1.0f);
    }
    EnLynel_PlayAnim(this, &gLynelBlockHitAnim, ANIMMODE_ONCE, -1.0f);
    EnLynel_SetupAction(this, EnLynel_BlockHit);
}

// A second blocked hit in a row is answered with the shield bash, as in BotW.
static void EnLynel_BlockHit(EnLynel* this, PlayState* play) {
    EnLynel_TurnTowardPlayer(this, 0x700);
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (this->blockCount >= 2 && this->actor.xzDistToPlayer < LYNEL_COMBO_RANGE) {
        EnLynel_SetupBash(this);
    } else {
        EnLynel_SetupGuard(this, 15);
    }
}

static void EnLynel_SetupCharge(EnLynel* this) {
    this->timer = 70;
    this->chargeCount++;
    this->ramCollider.info.toucher.damage = 0x18;
    EnLynel_PlayAnim(this, &gLynelChargeAnim, ANIMMODE_LOOP, -6.0f);
    EnLynel_SetupAction(this, EnLynel_Charge);
    EnLynel_PlaySfx(this, NA_SE_EV_HORSE_NEIGH);
}

static void EnLynel_Charge(EnLynel* this, PlayState* play) {
    bool isHoming = this->timer > 45 || this->actor.xzDistToPlayer > 200.0f;
    bool hasPassed = !Actor_IsFacingPlayer(&this->actor, 0x4000) && this->timer < 50;

    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_CHARGE_STEP_A) ||
        Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_CHARGE_STEP_B)) {
        EnLynel_PlaySfx(this, NA_SE_EV_HORSE_RUN);
    }
    Math_ApproachF(&this->actor.speedXZ, LYNEL_CHARGE_SPEED, 0.4f, 2.0f);
    if (isHoming) {
        EnLynel_TurnTowardPlayer(this, 0x300);
    }
    this->isRamActive = true;
    this->isSwordActive = true;

    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        EnLynel_PlaySfx(this, NA_SE_IT_WALL_HIT_HARD);
        EnLynel_Quake(play, 5, 15);
        EnLynel_SetupStun(this, play, 45);
        return;
    }
    if (hasPassed || DECR(this->timer) == 0) {
        EnLynel_PlayAnim(this, &gLynelWheelAnim, ANIMMODE_ONCE, -3.0f);
        EnLynel_SetupAction(this, EnLynel_Wheel);
    }
}

// Sits back on the hinds to brake, then wheels round to face Link and may charge again.
static void EnLynel_Wheel(EnLynel* this, PlayState* play) {
    f32 frame = this->skelAnime.curFrame;

    Math_StepToF(&this->actor.speedXZ, 0.0f, 1.6f);
    if (this->actor.speedXZ > 2.0f && ((s32)frame % 2) == 0) {
        Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, 30.0f, 2, 4.0f, 80, 15, false);
    }
    if (frame >= LYNEL_ANIM_WHEEL_TURN_START && frame <= LYNEL_ANIM_WHEEL_TURN_END) {
        EnLynel_PivotToward(this, this->actor.yawTowardsPlayer, 0xE00);
    }
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_WHEEL_TURN_START)) {
        EnLynel_PlaySfx(this, NA_SE_EV_HORSE_NEIGH);
    }
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_WHEEL_LAND)) {
        EnLynel_PlaySfx(this, NA_SE_EV_HORSE_LAND);
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    bool hasChargesLeft = this->chargeCount < LYNEL_MAX_CHARGES;

    if (hasChargesLeft && this->actor.xzDistToPlayer > 250.0f) {
        EnLynel_SetupCharge(this);
    } else {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_SetupLeap(EnLynel* this) {
    this->actor.speedXZ = 0.0f;
    this->swordCollider.info.toucher.damage = 0x20;
    this->ramCollider.info.toucher.damage = 0x18;
    EnLynel_PlayAnim(this, &gLynelLeapSlamAnim, ANIMMODE_ONCE, -3.0f);
    EnLynel_SetupAction(this, EnLynel_Leap);
}

static void EnLynel_LandLeap(EnLynel* this, PlayState* play) {
    Vec3f impact = this->actor.world.pos;

    impact.x += 60.0f * Math_SinS(this->actor.shape.rot.y);
    impact.z += 60.0f * Math_CosS(this->actor.shape.rot.y);
    this->actor.speedXZ = 0.0f;
    EnLynel_PlaySfx(this, NA_SE_EN_GOLON_LAND_BIG);
    EnLynel_Quake(play, 6, 12);
    Actor_SpawnFloorDustRing(play, &this->actor, &impact, 50.0f, 12, 6.0f, 120, 20, false);
    EffectSsBlast_SpawnWhiteShockwave(play, &impact, &sZeroVec, &sZeroVec);
}

// BotW leaping cleave: vaults at Link, hangs at the apex with the sword raised and splits the ground on landing.
static void EnLynel_Leap(EnLynel* this, PlayState* play) {
    static const f32 sAirFrames = 14.0f;
    f32 frame = this->skelAnime.curFrame;
    bool isAirborne = !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);

    if (frame < LYNEL_ANIM_LEAP_SLAM_TAKEOFF) {
        EnLynel_TurnTowardPlayer(this, 0x800);
    }
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_LEAP_SLAM_TAKEOFF)) {
        this->actor.velocity.y = 8.5f;
        this->actor.speedXZ = CLAMP_MIN(this->actor.xzDistToPlayer - 70.0f, 0.0f) / sAirFrames;
        EnLynel_PlaySfx(this, NA_SE_EN_STAL_JUMP);
    }
    if (frame >= LYNEL_ANIM_LEAP_SLAM_HIT_START - 1 && isAirborne) {
        this->skelAnime.curFrame = LYNEL_ANIM_LEAP_SLAM_HIT_START - 1;
    }
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_LEAP_SLAM_HIT_START)) {
        EnLynel_PlaySfx(this, NA_SE_EN_IRONNACK_SWING_AXE);
    }
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_LEAP_SLAM_LAND)) {
        EnLynel_LandLeap(this, play);
    }
    this->isSwordActive = frame >= LYNEL_ANIM_LEAP_SLAM_HIT_START && frame <= LYNEL_ANIM_LEAP_SLAM_HIT_END;
    this->isRamActive = frame >= LYNEL_ANIM_LEAP_SLAM_LAND && frame <= LYNEL_ANIM_LEAP_SLAM_LAND + 2;
    if (SkelAnime_Update(&this->skelAnime)) {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_SetupBackstep(EnLynel* this) {
    this->actor.speedXZ = 0.0f;
    EnLynel_PlayAnim(this, &gLynelBackstepAnim, ANIMMODE_ONCE, -3.0f);
    EnLynel_SetupAction(this, EnLynel_Backstep);
}

static void EnLynel_Backstep(EnLynel* this, PlayState* play) {
    EnLynel_TurnTowardPlayer(this, 0x600);
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_BACKSTEP_TAKEOFF)) {
        this->actor.speedXZ = -10.0f;
        this->actor.velocity.y = 8.0f;
        EnLynel_PlaySfx(this, NA_SE_EN_STAL_JUMP);
    }
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_BACKSTEP_LAND)) {
        EnLynel_PlaySfx(this, NA_SE_EV_HORSE_LAND);
    }
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && this->skelAnime.curFrame > LYNEL_ANIM_BACKSTEP_LAND) {
        Math_StepToF(&this->actor.speedXZ, 0.0f, 2.0f);
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        this->actor.speedXZ = 0.0f;
        EnLynel_PlayAnim(this, &gLynelBreathAnim, ANIMMODE_ONCE, -4.0f);
        EnLynel_SetupAction(this, EnLynel_Breath);
    }
}

static void EnLynel_Breath(EnLynel* this, PlayState* play) {
    static const s16 sVolleyYaw[] = { -0xA00, 0, 0xA00 };
    s32 frame = (s32)this->skelAnime.curFrame;
    bool isBreathing = frame >= LYNEL_ANIM_BREATH_FIRE_START && frame <= LYNEL_ANIM_BREATH_FIRE_END;

    EnLynel_TurnTowardPlayer(this, isBreathing ? 0x180 : 0x600);
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_BREATH_FIRE_START - 4)) {
        EnLynel_PlaySfx(this, NA_SE_EN_DODO_K_BREATH);
    }
    if (isBreathing && Animation_OnFrame(&this->skelAnime, frame)) {
        s32 elapsed = frame - LYNEL_ANIM_BREATH_FIRE_START;

        if (this->phase == LYNEL_PHASE_RAGE) {
            if ((elapsed % 2) == 0) {
                s16 sweep = -0x1800 + elapsed * 0x3000 / (LYNEL_ANIM_BREATH_FIRE_END - LYNEL_ANIM_BREATH_FIRE_START);
                EnLynel_BreatheFireball(this, play, sweep);
            }
        } else if (elapsed % 4 == 0 && elapsed / 4 < ARRAY_COUNT(sVolleyYaw)) {
            EnLynel_BreatheFireball(this, play, sVolleyYaw[elapsed / 4]);
        }
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_SetupSwap(EnLynel* this) {
    this->actor.speedXZ = 0.0f;
    EnLynel_PlayAnim(this, this->isBowInHand ? &gLynelSwapToSwordAnim : &gLynelSwapToBowAnim, ANIMMODE_ONCE, -4.0f);
    EnLynel_SetupAction(this, EnLynel_Swap);
}

static void EnLynel_Swap(EnLynel* this, PlayState* play) {
    EnLynel_TurnTowardPlayer(this, 0x500);
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_SWAP_TO_BOW_SWAP)) {
        this->isBowInHand = !this->isBowInHand;
        EnLynel_PlaySfx(this, NA_SE_IT_SWORD_PUTAWAY);
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_SetupShoot(EnLynel* this) {
    this->actor.speedXZ = 0.0f;
    EnLynel_PlayAnim(this, &gLynelBowShootAnim, ANIMMODE_ONCE, -4.0f);
    EnLynel_SetupAction(this, EnLynel_Shoot);
}

static void EnLynel_Shoot(EnLynel* this, PlayState* play) {
    if (this->skelAnime.curFrame < LYNEL_ANIM_BOW_SHOOT_RELEASE) {
        EnLynel_TurnTowardPlayer(this, 0x600);
    }
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_BOW_SHOOT_RELEASE)) {
        EnLynel_ShootArrowFan(this, play);
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_SetupRain(EnLynel* this) {
    this->actor.speedXZ = 0.0f;
    EnLynel_PlayAnim(this, &gLynelBowRainAnim, ANIMMODE_ONCE, -4.0f);
    EnLynel_SetupAction(this, EnLynel_Rain);
}

static void EnLynel_Rain(EnLynel* this, PlayState* play) {
    if (this->skelAnime.curFrame < LYNEL_ANIM_BOW_RAIN_RELEASE) {
        EnLynel_TurnTowardPlayer(this, 0x600);
    }
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_BOW_RAIN_RELEASE)) {
        EnLynel_ShootArrowRain(this);
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_SetupVolley(EnLynel* this) {
    this->actor.speedXZ = 0.0f;
    EnLynel_PlayAnim(this, &gLynelBowVolleyAnim, ANIMMODE_ONCE, -4.0f);
    EnLynel_SetupAction(this, EnLynel_Volley);
}

static void EnLynel_Volley(EnLynel* this, PlayState* play) {
    EnLynel_TurnTowardPlayer(this, 0x600);
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_BOW_VOLLEY_TAKEOFF)) {
        this->actor.velocity.y = 15.0f;
        EnLynel_PlaySfx(this, NA_SE_EN_STAL_JUMP);
    }
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_BOW_VOLLEY_RELEASE_A) ||
        Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_BOW_VOLLEY_RELEASE_B)) {
        EnLynel_ShootArrowFan(this, play);
    }
    if (this->skelAnime.curFrame > LYNEL_ANIM_BOW_VOLLEY_RELEASE_B &&
        !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        this->skelAnime.curFrame = LYNEL_ANIM_BOW_VOLLEY_RELEASE_B;
    }
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_BOW_VOLLEY_LAND)) {
        EnLynel_PlaySfx(this, NA_SE_EV_HORSE_LAND);
        EnLynel_Quake(play, 3, 8);
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_SetupStun(EnLynel* this, PlayState* play, s16 duration) {
    this->actor.speedXZ = 0.0f;
    this->timer = (this->phase == LYNEL_PHASE_RAGE) ? duration * 3 / 4 : duration;
    EnLynel_PlayAnim(this, &gLynelStunEnterAnim, ANIMMODE_ONCE, -2.0f);
    EnLynel_PlaySfx(this, NA_SE_EN_DODO_K_DOWN);
    EnLynel_SetupAction(this, EnLynel_Stunned);
}

static bool EnLynel_CanPlayerMount(EnLynel* this, PlayState* play) {
    Player* player = EnLynel_GetPlayer(play);

    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A)) {
        return false;
    }
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || player->heldActor != NULL) {
        return false;
    }
    if (player->stateFlags1 &
        (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DEAD)) {
        return false;
    }
    return Math_Vec3f_DistXZ(&player->actor.world.pos, &this->seatPos) < LYNEL_MOUNT_RANGE;
}

static void EnLynel_DrawSwordForRider(Player* player, PlayState* play) {
    u8 item = gSaveContext.equips.buttonItems[0];

    if (item >= ITEM_SWORD_KOKIRI && item <= ITEM_SWORD_BGS) {
        Player_InitItemAction(play, player, Player_ItemToItemAction(item));
    }
}

static void EnLynel_MountPlayer(EnLynel* this, PlayState* play) {
    Player* player = EnLynel_GetPlayer(play);

    sRiddenLynel = this;
    sRiderSlashTimer = 0;
    this->riderHits = 0;
    this->timer = 110;
    EnLynel_DrawSwordForRider(player, play);
    Player_SetupAction(play, player, EnLynel_RiderAction, 0);
    Player_AnimPlayLoop(play, player, (LinkAnimationHeader*)gPlayerAnim_link_uma_anim_fastrun);
    Audio_PlaySfxGeneral(NA_SE_PL_SIT_ON_HORSE, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    EnLynel_PlaySfx(this, NA_SE_EV_HORSE_NEIGH);
    EnLynel_PlayAnim(this, &gLynelThrashAnim, ANIMMODE_LOOP, -4.0f);
    EnLynel_SetupAction(this, EnLynel_Ridden);
}

static void EnLynel_Stunned(EnLynel* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime) && this->skelAnime.animation == &gLynelStunEnterAnim) {
        EnLynel_PlayAnim(this, &gLynelStunAnim, ANIMMODE_LOOP, 0.0f);
    }
    if (EnLynel_CanPlayerMount(this, play)) {
        EnLynel_MountPlayer(this, play);
        return;
    }
    if (DECR(this->timer) == 0) {
        EnLynel_SetupRoar(this);
    }
}

static void EnLynel_SetupDamaged(EnLynel* this) {
    this->actor.speedXZ = 0.0f;
    EnLynel_PlayAnim(this, &gLynelDamageAnim, ANIMMODE_ONCE, -2.0f);
    EnLynel_SetupAction(this, EnLynel_Damaged);
}

static void EnLynel_Damaged(EnLynel* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_ThrowRider(EnLynel* this, PlayState* play) {
    Player* player = EnLynel_GetPlayer(play);
    bool isRiding = EnLynel_IsRidden(this, play);

    sRiddenLynel = NULL;
    if (!isRiding) {
        return;
    }
    player->actor.colChkInfo.damage = 0;
    func_80837C0C(play, player, PLAYER_HIT_RESPONSE_KNOCKBACK_LARGE, 9.0f, 9.0f, this->actor.shape.rot.y + 0x4000, 20);
}

static void EnLynel_SetupBuck(EnLynel* this) {
    EnLynel_PlayAnim(this, &gLynelBuckAnim, ANIMMODE_ONCE, -3.0f);
    EnLynel_SetupAction(this, EnLynel_Buck);
}

static void EnLynel_Ridden(EnLynel* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->actor.shape.rot.y += (s16)(Math_SinS(this->timer * 0x900) * 0x300);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if ((this->timer % 8) == 0) {
        EnLynel_PlaySfx(this, NA_SE_EV_HORSE_RUN);
    }
    if (!EnLynel_IsRidden(this, play)) {
        sRiddenLynel = NULL;
        EnLynel_Decide(this, play);
        return;
    }
    if (DECR(this->timer) == 0 || this->riderHits >= LYNEL_RIDER_MAX_HITS) {
        EnLynel_PlaySfx(this, NA_SE_EV_HORSE_NEIGH);
        EnLynel_SetupBuck(this);
    }
}

static void EnLynel_Buck(EnLynel* this, PlayState* play) {
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_BUCK_THROW)) {
        EnLynel_ThrowRider(this, play);
        EnLynel_PlaySfx(this, NA_SE_EN_DODO_K_OTAKEBI);
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    EnLynel_PlaySfx(this, NA_SE_EV_HORSE_LAND);
    EnLynel_Quake(play, 3, 8);
    if (!EnLynel_UpdatePhase(this, play)) {
        EnLynel_Decide(this, play);
    }
}

static void EnLynel_SetupDie(EnLynel* this, PlayState* play) {
    EnLynel_ThrowRider(this, play);
    this->actor.speedXZ = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->timer = 20;
    Enemy_StartFinishingBlow(play, &this->actor);
    EnLynel_PlaySfx(this, NA_SE_EN_DODO_K_DEAD);
    EnLynel_PlayAnim(this, &gLynelDeathAnim, ANIMMODE_ONCE, -3.0f);
    EnLynel_SetupAction(this, EnLynel_Die);
    if (this->isFightStarted) {
        func_800F5B58();
    }
}

static void EnLynel_Vanish(EnLynel* this, PlayState* play) {
    static Vec3f sDrift = { 0.0f, 1.5f, 0.0f };
    static Vec3f sNoAccel = { 0.0f, 0.0f, 0.0f };

    for (s32 i = 0; i < 12; i++) {
        Vec3f pos = this->actor.world.pos;

        pos.x += Rand_CenteredFloat(120.0f);
        pos.y += Rand_ZeroFloat(60.0f);
        pos.z += Rand_CenteredFloat(120.0f);
        EffectSsDeadDb_Spawn(play, &pos, &sDrift, &sNoAccel, 150, 10, 255, 255, 255, 255, 200, 40, 40, 1, 9, i == 0);
    }
    Item_DropCollectible(play, &this->actor.world.pos, ITEM00_HEART);
    Item_DropCollectible(play, &this->actor.world.pos, ITEM00_RUPEE_RED);
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xC0);
    Actor_Kill(&this->actor);
}

static void EnLynel_Die(EnLynel* this, PlayState* play) {
    if (Animation_OnFrame(&this->skelAnime, LYNEL_ANIM_DEATH_THUD)) {
        EnLynel_PlaySfx(this, NA_SE_EN_GOLON_LAND_BIG);
        EnLynel_Quake(play, 5, 12);
        Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, 60.0f, 10, 6.0f, 150, 20, false);
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (DECR(this->timer) == 0) {
        EnLynel_Vanish(this, play);
    }
}

static void EnLynel_Decide(EnLynel* this, PlayState* play) {
    Player* player = EnLynel_GetPlayer(play);
    f32 distance = this->actor.xzDistToPlayer;
    bool canShoot = this->phase != LYNEL_PHASE_SWORD;
    f32 roll = Rand_ZeroOne();

    this->chargeCount = 0;
    this->blockCount = 0;
    if (distance < LYNEL_TURN_RANGE && ABS(EnLynel_GetYawToPlayer(this)) > 0x3000) {
        EnLynel_SetupTurn(this);
        return;
    }
    if (distance < LYNEL_MELEE_RANGE) {
        if (this->isBowInHand) {
            EnLynel_SetupSwap(this);
        } else if (this->hasShield && this->turtleTimer > 30) {
            EnLynel_SetupBash(this);
        } else if (this->hasShield && EnLynel_IsPlayerSwinging(this, player) && roll < 0.5f) {
            EnLynel_SetupGuard(this, 30);
        } else {
            EnLynel_SetupSlash(this, 0);
        }
        return;
    }
    if (canShoot && distance > LYNEL_SHOOT_RANGE && roll < 0.6f) {
        if (!this->isBowInHand) {
            EnLynel_SetupSwap(this);
        } else if (distance > LYNEL_RAIN_RANGE && roll < 0.35f) {
            EnLynel_SetupRain(this);
        } else if (roll < 0.2f) {
            EnLynel_SetupVolley(this);
        } else {
            EnLynel_SetupShoot(this);
        }
        return;
    }
    if (this->isBowInHand && (distance < 250.0f || !canShoot)) {
        EnLynel_SetupSwap(this);
        return;
    }
    if (this->isBowInHand) {
        EnLynel_SetupShoot(this);
    } else if (distance > 250.0f && roll < 0.4f) {
        EnLynel_SetupCharge(this);
    } else if (distance >= LYNEL_LEAP_MIN_RANGE && distance <= LYNEL_LEAP_MAX_RANGE && roll < 0.6f) {
        EnLynel_SetupLeap(this);
    } else if (distance < 300.0f && roll < 0.8f && !EnLynel_IsPlayerFacingAndShielding(this, player)) {
        EnLynel_SetupBackstep(this);
    } else {
        EnLynel_SetupApproach(this);
    }
}

static void EnLynel_TakeRiderHit(EnLynel* this, PlayState* play) {
    this->actor.colChkInfo.health = MAX(this->actor.colChkInfo.health - LYNEL_RIDER_DAMAGE, 0);
    this->riderHits++;
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    EffectSsHitMark_SpawnFixedScale(play, 0, &this->seatPos);
    EnLynel_PlaySfx(this, NA_SE_EN_DODO_K_DAMAGE);
    if (this->actor.colChkInfo.health == 0) {
        EnLynel_SetupDie(this, play);
    }
}

static void EnLynel_RiderAction(Player* player, PlayState* play) {
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    player->actor.gravity = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    LinkAnimation_Update(play, &player->skelAnime);

    if (sRiderSlashTimer > 0) {
        LinkAnimation_Update(play, &player->upperSkelAnime);
        AnimationContext_SetCopyTrue(play, player->skelAnime.limbCount, player->skelAnime.jointTable,
                                     player->upperSkelAnime.jointTable, sRiderUpperBodyLimbs);
        if (--sRiderSlashTimer == 6 && sRiddenLynel != NULL && sRiddenLynel->actionFunc == EnLynel_Ridden) {
            EnLynel_TakeRiderHit(sRiddenLynel, play);
        }
        return;
    }
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) && player->heldItemAction != PLAYER_IA_NONE) {
        const char* slash =
            (sRiderSlashCount++ % 2) ? gPlayerAnim_link_fighter_pierce_kiru : gPlayerAnim_link_fighter_normal_kiru;

        LinkAnimation_PlayOnce(play, &player->upperSkelAnime, (LinkAnimationHeader*)slash);
        sRiderSlashTimer = 14;
        Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
        Audio_PlaySfxGeneral(NA_SE_IT_SWORD_SWING_HARD, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

static void EnLynel_SeatRider(EnLynel* this, PlayState* play) {
    Player* player = EnLynel_GetPlayer(play);

    if (!EnLynel_IsRidden(this, play)) {
        return;
    }
    player->actor.world.pos = this->seatPos;
    player->actor.world.pos.y -= LYNEL_RIDER_DROP;
    player->actor.prevPos = player->actor.world.pos;
    player->actor.shape.rot.y = this->actor.shape.rot.y;
    player->actor.world.rot.y = this->actor.shape.rot.y;
    player->yaw = this->actor.shape.rot.y;
}

static bool EnLynel_WasHeadshot(EnLynel* this) {
    ColliderJntSphElement* head = &this->bodyElements[LYNEL_SPHERE_HEAD];

    if (!(head->info.bumperFlags & BUMP_HIT) || head->info.acHitInfo == NULL) {
        return false;
    }
    return (head->info.acHitInfo->toucher.dmgFlags & (DMG_ARROW | DMG_SLINGSHOT | DMG_HOOKSHOT)) != 0;
}

static void EnLynel_EnterPhase(EnLynel* this, PlayState* play, u8 phase) {
    this->phase = phase;
    if (phase == LYNEL_PHASE_BOW && this->hasShield) {
        EnLynel_BreakShield(this, play);
    }
    if (phase == LYNEL_PHASE_RAGE) {
        this->rageFlashTimer = 1;
    }
    EnLynel_SetupRoar(this);
}

static bool EnLynel_UpdatePhase(EnLynel* this, PlayState* play) {
    if (this->phase == LYNEL_PHASE_SWORD && this->actor.colChkInfo.health <= LYNEL_BOW_PHASE_HEALTH) {
        EnLynel_EnterPhase(this, play, LYNEL_PHASE_BOW);
        return true;
    }
    if (this->phase == LYNEL_PHASE_BOW && this->actor.colChkInfo.health <= LYNEL_RAGE_PHASE_HEALTH) {
        EnLynel_EnterPhase(this, play, LYNEL_PHASE_RAGE);
        return true;
    }
    return false;
}

static void EnLynel_ReactToHit(EnLynel* this, PlayState* play, bool isHeadshot, u8 effect) {
    if (effect == LYNEL_DMGEFF_ICE) {
        Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 80);
        EnLynel_PlaySfx(this, NA_SE_EN_GOMA_JR_FREEZE);
        EnLynel_SetupStun(this, play, 80);
    } else if (isHeadshot || effect == LYNEL_DMGEFF_LIGHT) {
        EnLynel_SetupStun(this, play, 70);
    } else if (effect == LYNEL_DMGEFF_STUN && Actor_IsFacingPlayer(&this->actor, 0x3000)) {
        EnLynel_SetupStun(this, play, 35);
    } else if (!EnLynel_HasHyperArmor(this) && this->actionFunc != EnLynel_Stunned) {
        EnLynel_SetupDamaged(this);
    }
}

static bool EnLynel_TryBlock(EnLynel* this, PlayState* play, bool isShieldHit) {
    if (!EnLynel_IsGuarding(this)) {
        return false;
    }
    if (!isShieldHit && !Actor_IsFacingPlayer(&this->actor, 0x3000)) {
        return false;
    }
    EnLynel_Block(this, play);
    return true;
}

static void EnLynel_CheckDamage(EnLynel* this, PlayState* play) {
    bool isShieldHit = (this->shieldCollider.base.acFlags & AC_HIT) != 0;
    bool isBodyHit = (this->bodyCollider.base.acFlags & AC_HIT) != 0;

    this->shieldCollider.base.acFlags &= ~AC_HIT;
    this->bodyCollider.base.acFlags &= ~AC_HIT;
    if (!isShieldHit && !isBodyHit) {
        return;
    }
    if (this->actionFunc == EnLynel_Die || this->actionFunc == EnLynel_Ridden) {
        return;
    }
    if (EnLynel_TryBlock(this, play, isShieldHit) || !isBodyHit) {
        return;
    }

    bool isHeadshot = EnLynel_WasHeadshot(this);
    u8 effect = this->actor.colChkInfo.damageEffect;

    Actor_SetDropFlagJntSph(&this->actor, &this->bodyCollider, true);
    Actor_ApplyDamage(&this->actor);
    this->hurtboxCooldown = 8;
    if (effect != LYNEL_DMGEFF_ICE) {
        Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    }
    if (effect == LYNEL_DMGEFF_FIRE) {
        EnLynel_SpawnFlameBurst(play, &this->actor.focus.pos, 4, 30.0f);
    }
    if (this->actor.colChkInfo.health == 0) {
        EnLynel_SetupDie(this, play);
        return;
    }
    EnLynel_PlaySfx(this, isHeadshot ? NA_SE_EN_DODO_K_DOWN : NA_SE_EN_DODO_K_DAMAGE);
    if (EnLynel_UpdatePhase(this, play)) {
        return;
    }
    EnLynel_ReactToHit(this, play, isHeadshot, effect);
}

static void EnLynel_TrackPlayerHabits(EnLynel* this, PlayState* play) {
    Player* player = EnLynel_GetPlayer(play);

    if (EnLynel_IsPlayerFacingAndShielding(this, player) && this->actor.xzDistToPlayer < LYNEL_COMBO_RANGE) {
        this->turtleTimer++;
    } else {
        this->turtleTimer = 0;
    }
}

static void EnLynel_UpdateRage(EnLynel* this, PlayState* play) {
    if (this->phase != LYNEL_PHASE_RAGE || this->actionFunc == EnLynel_Die) {
        return;
    }
    if ((this->rageFlashTimer % 30) == 0) {
        Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 160, COLORFILTER_OPA, 10);
    }
    if ((this->rageFlashTimer % 4) == 0) {
        EnLynel_SpawnFlameBurst(play, &this->actor.focus.pos, 1, 30.0f);
    }
    this->rageFlashTimer++;
}

static void EnLynel_UpdateHeadYaw(EnLynel* this) {
    bool shouldLook = this->actionFunc == EnLynel_Idle || this->actionFunc == EnLynel_Approach ||
                      this->actionFunc == EnLynel_Guard || this->actionFunc == EnLynel_Shoot ||
                      this->actionFunc == EnLynel_Turn || this->actionFunc == EnLynel_BlockHit;
    s16 target = shouldLook ? CLAMP((s16)(this->actor.yawTowardsPlayer - this->actor.shape.rot.y), -0x2800, 0x2800) : 0;

    Math_SmoothStepToS(&this->headYaw, target, 4, 0x600, 0x80);
}

static void EnLynel_PlaceRamCollider(EnLynel* this) {
    Cylinder16* dim = &this->ramCollider.dim;

    if (this->actionFunc == EnLynel_Bash) {
        dim->radius = 28;
        dim->height = 50;
        dim->pos.x = this->shieldPos.x;
        dim->pos.y = this->shieldPos.y - 25.0f;
        dim->pos.z = this->shieldPos.z;
    } else if (this->actionFunc == EnLynel_Leap) {
        dim->radius = 60;
        dim->height = 30;
        dim->pos.x = this->actor.world.pos.x + 60.0f * Math_SinS(this->actor.shape.rot.y);
        dim->pos.y = this->actor.world.pos.y;
        dim->pos.z = this->actor.world.pos.z + 60.0f * Math_CosS(this->actor.shape.rot.y);
    } else {
        dim->radius = 40;
        dim->height = 90;
        dim->pos.x = this->actor.world.pos.x;
        dim->pos.y = this->actor.world.pos.y;
        dim->pos.z = this->actor.world.pos.z;
    }
}

static void EnLynel_UpdateCollision(EnLynel* this, PlayState* play) {
    if (this->actionFunc == EnLynel_Die) {
        return;
    }
    if (this->isSwordActive && !this->isBowInHand) {
        if (this->swordCollider.base.atFlags & AT_BOUNCED) {
            Actor_SetPlayerKnockbackSmallNoDamage(play, &this->actor, 5.0f, this->actor.yawTowardsPlayer, 2.0f);
        }
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->swordCollider.base);
    }
    if (this->isRamActive) {
        EnLynel_PlaceRamCollider(this);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->ramCollider.base);
    }
    if (this->hasShield) {
        this->shieldElements[0].dim.modelSphere.radius = EnLynel_IsGuarding(this) ? 34 : 17;
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->shieldCollider.base);
    }
    if (DECR(this->hurtboxCooldown) == 0 && this->actionFunc != EnLynel_Ridden) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    if (this->actionFunc != EnLynel_Ridden) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    }
}

static void EnLynel_Init(Actor* thisx, PlayState* play) {
    EnLynel* this = (EnLynel*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 140.0f);
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.targetMode = 5;
    this->actor.gravity = -1.2f;

    sBodyElementsInit[LYNEL_SPHERE_HEAD].dim.modelSphere.center = sLynelHeadCenter;
    sShieldElementsInit[0].dim.modelSphere.center = sLynelShieldCenter;
    for (s32 i = 0; i < LYNEL_MAX_PROJECTILES; i++) {
        sProjectileElementsInit[i] = sProjectileElementInit;
    }

    Collider_InitJntSph(play, &this->bodyCollider);
    Collider_SetJntSph(play, &this->bodyCollider, &this->actor, &sBodyInit, this->bodyElements);
    Collider_InitJntSph(play, &this->shieldCollider);
    Collider_SetJntSph(play, &this->shieldCollider, &this->actor, &sShieldInit, this->shieldElements);
    Collider_InitJntSph(play, &this->projectileCollider);
    Collider_SetJntSph(play, &this->projectileCollider, &this->actor, &sProjectileInit, this->projectileElements);
    Collider_InitQuad(play, &this->swordCollider);
    Collider_SetQuad(play, &this->swordCollider, &this->actor, &sSwordInit);
    Collider_InitCylinder(play, &this->ramCollider);
    Collider_SetCylinder(play, &this->ramCollider, &this->actor, &sRamInit);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, &sDamageTable, &sColChkInit);

    SkelAnime_InitFlex(play, &this->skelAnime, &gLynelSkel, &gLynelIdleAnim, this->jointTable, this->morphTable,
                       LYNEL_NUM_LIMBS);
    this->hasShield = true;
    this->phase = LYNEL_PHASE_SWORD;
    this->seatPos = this->actor.world.pos;
    this->mouthPos = this->actor.world.pos;
    this->nockPos = this->actor.world.pos;
    this->shieldPos = this->actor.world.pos;
    EnLynel_SetupIdle(this);
}

static void EnLynel_Destroy(Actor* thisx, PlayState* play) {
    EnLynel* this = (EnLynel*)thisx;

    if (sRiddenLynel == this) {
        EnLynel_ThrowRider(this, play);
    }
    Collider_DestroyJntSph(play, &this->bodyCollider);
    Collider_DestroyJntSph(play, &this->shieldCollider);
    Collider_DestroyJntSph(play, &this->projectileCollider);
    Collider_DestroyQuad(play, &this->swordCollider);
    Collider_DestroyCylinder(play, &this->ramCollider);
}

static void EnLynel_Update(Actor* thisx, PlayState* play) {
    EnLynel* this = (EnLynel*)thisx;

    EnLynel_CheckDamage(this, play);
    EnLynel_TrackPlayerHabits(this, play);
    this->actionFunc(this, play);
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 40.0f, 45.0f, 80.0f, BGCHECK_UPDATE_ALL);
    EnLynel_SeatRider(this, play);
    EnLynel_UpdateHeadYaw(this);
    EnLynel_UpdateRage(this, play);
    EnLynel_UpdateProjectiles(this, play);
    EnLynel_UpdateCollision(this, play);
}

static void EnLynel_DrawAttachment(PlayState* play, const char* displayList) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)displayList);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnLynel_UpdateSwordQuad(EnLynel* this) {
    Vec3f baseEdge;
    Vec3f tipEdge;
    Vec3f base;
    Vec3f tip;

    Math_Vec3f_Sum(&sLynelSwordBase, &sLynelSwordWidth, &baseEdge);
    Math_Vec3f_Sum(&sLynelSwordTip, &sLynelSwordWidth, &tipEdge);
    Matrix_MultVec3f(&sLynelSwordBase, &base);
    Matrix_MultVec3f(&sLynelSwordTip, &tip);
    Matrix_MultVec3f(&baseEdge, &baseEdge);
    Matrix_MultVec3f(&tipEdge, &tipEdge);
    Collider_SetQuadVertices(&this->swordCollider, &tip, &tipEdge, &base, &baseEdge);
}

static bool EnLynel_IsNockedArrowVisible(EnLynel* this) {
    f32 frame = this->skelAnime.curFrame;

    if (this->actionFunc == EnLynel_Shoot) {
        return frame > 6.0f && frame < LYNEL_ANIM_BOW_SHOOT_RELEASE;
    }
    if (this->actionFunc == EnLynel_Rain) {
        return frame > 6.0f && frame < LYNEL_ANIM_BOW_RAIN_RELEASE;
    }
    if (this->actionFunc == EnLynel_Volley) {
        return frame > 6.0f && frame < LYNEL_ANIM_BOW_VOLLEY_RELEASE_B;
    }
    return false;
}

static s32 EnLynel_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnLynel* this = (EnLynel*)thisx;

    if (limbIndex == LYNEL_LIMB_NECK) {
        rot->y += this->headYaw / 2;
    } else if (limbIndex == LYNEL_LIMB_HEAD) {
        rot->y += this->headYaw / 2;
    }
    return false;
}

static void EnLynel_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnLynel* this = (EnLynel*)thisx;

    Collider_UpdateSpheres(limbIndex, &this->bodyCollider);
    Collider_UpdateSpheres(limbIndex, &this->shieldCollider);

    switch (limbIndex) {
        case LYNEL_LIMB_ROOT:
            Matrix_MultVec3f(&sLynelSeat, &this->seatPos);
            EnLynel_DrawAttachment(play, this->isBowInHand ? gLynelSwordStowedDL : gLynelBowStowedDL);
            break;
        case LYNEL_LIMB_HEAD:
            Matrix_MultVec3f(&sLynelHeadCenter, &this->actor.focus.pos);
            break;
        case LYNEL_LIMB_JAW:
            Matrix_MultVec3f(&sLynelMouth, &this->mouthPos);
            break;
        case LYNEL_LIMB_L_FOREARM:
            Matrix_MultVec3f(&sLynelShieldCenter, &this->shieldPos);
            if (this->hasShield) {
                EnLynel_DrawAttachment(play, gLynelShieldArmDL);
            }
            break;
        case LYNEL_LIMB_L_HAND:
            Matrix_MultVec3f(&sLynelArrowNock, &this->nockPos);
            if (this->isBowInHand) {
                EnLynel_DrawAttachment(play, gLynelBowHandDL);
                if (EnLynel_IsNockedArrowVisible(this)) {
                    EnLynel_DrawAttachment(play, gLynelArrowNockedDL);
                }
            }
            break;
        case LYNEL_LIMB_R_HAND:
            if (!this->isBowInHand) {
                EnLynel_UpdateSwordQuad(this);
                EnLynel_DrawAttachment(play, gLynelSwordHandDL);
            }
            break;
        default:
            break;
    }
}

static void EnLynel_DrawProjectile(PlayState* play, LynelProjectile* projectile) {
    bool isArrow = projectile->type == LYNEL_PROJECTILE_ARROW || projectile->type == LYNEL_PROJECTILE_RAIN_ARROW;

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(projectile->pos.x, projectile->pos.y, projectile->pos.z, MTXMODE_NEW);
    if (isArrow) {
        s16 yaw = Math_Atan2S(projectile->velocity.z, projectile->velocity.x);

        Matrix_RotateZYX(projectile->pitch, yaw, 0, MTXMODE_APPLY);
        Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    } else {
        f32 scale = 0.011f + 0.002f * Math_SinS(projectile->life * 0x2000);

        Matrix_RotateZYX(projectile->life * 0x900, projectile->life * 0x700, 0, MTXMODE_APPLY);
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    }
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)(isArrow ? gLynelArrowDL : gLynelFireballDL));
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnLynel_DrawBlast(PlayState* play, LynelProjectile* blast) {
    f32 scale = EnLynel_GetBlastRadius(blast) / LYNEL_BLAST_MESH_RADIUS;
    u8 alpha = 220 * blast->life / LYNEL_BLAST_FRAMES;

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(blast->pos.x, blast->pos.y, blast->pos.z, MTXMODE_NEW);
    Matrix_RotateZYX(0, blast->life * 0x1000, 0, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, alpha);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gLynelBlastDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnLynel_DrawProjectiles(EnLynel* this, PlayState* play) {
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    for (s32 i = 0; i < LYNEL_MAX_PROJECTILES; i++) {
        LynelProjectile* projectile = &this->projectiles[i];

        switch (projectile->type) {
            case LYNEL_PROJECTILE_ARROW:
            case LYNEL_PROJECTILE_RAIN_ARROW:
            case LYNEL_PROJECTILE_FIREBALL:
                EnLynel_DrawProjectile(play, projectile);
                break;
            case LYNEL_PROJECTILE_BLAST:
                EnLynel_DrawBlast(play, projectile);
                break;
            default:
                break;
        }
    }
}

static void EnLynel_Draw(Actor* thisx, PlayState* play) {
    EnLynel* this = (EnLynel*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          EnLynel_OverrideLimbDraw, EnLynel_PostLimbDraw, this);
    EnLynel_DrawProjectiles(this, play);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterActor)) {
        return;
    }
    SOHActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = LYNEL_KEY;
    definition.description = "Lynel";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                            ACTOR_FLAG_DRAW_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnLynel);
    definition.init = EnLynel_Init;
    definition.destroy = EnLynel_Destroy;
    definition.update = EnLynel_Update;
    definition.draw = EnLynel_Draw;
    definition.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS;
    definition.naviHint = "Lynel&%cA legendary beast! Shoot its %rhead%c to stun it, then press %b\x9F%c by its back "
                          "to ride it!%w";

    if (sApi->RegisterActor(&definition) < 0) {
        sApi->Log("Could not register " LYNEL_KEY);
    }
}
