// Majora's Mask's Captain Keeta (En_Bsb) as an Unbound custom enemy, drawn from mm.o2r.
// Arena mode, like 2Ship away from Ikana Graveyard: no race path, no Sonata, no week flags; he drops a reward.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/effects/ovl_Effect_Ss_Hahen/z_eff_ss_hahen.h"
#include "overlays/effects/ovl_Effect_Ss_HitMark/z_eff_ss_hitmark.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define KEETA_KEY "mm.captain_keeta"
#define KEETA_SOUL_ITEM_KEY "soul.mm.captain_keeta"
#define GORON_MASK_KEY "nei.goron_form_mask"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_BSB "__OTR__objects/object_bsb/"
#define OBJ_KEEP "__OTR__objects/gameplay_keep/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_FLOOR_WATER 0x1D

#define MM_SE_EN_EYEGOLE_ATTACK 0x387B
#define MM_SE_EN_TEKU_JUMP 0x386C
#define MM_SE_EN_COMMON_FREEZE 0x389E
#define MM_SE_EN_KTIA_WALK 0x3AA2
#define MM_SE_EN_KTIA_PAUSE_K 0x3AA3
#define MM_SE_EN_KITA_LAUGH_K 0x3AA4
#define MM_SE_EN_KITA_DAMAGE 0x3AA5
#define MM_SE_EN_KITA_DEAD 0x3AA6
#define MM_SE_EN_KITA_ATTACK_W 0x3AAA

#define KEETA_HEALTH 24
#define KEETA_SCALE 0.04f
#define KEETA_EFFECT_COUNT 50
#define KEETA_BODYPART_MAX 17
#define KEETA_MUSIC_RANGE 400.0f

typedef enum {
    KEETA_LIMB_NONE,
    KEETA_LIMB_01,
    KEETA_LIMB_02,
    KEETA_LIMB_03,
    KEETA_LIMB_04,
    KEETA_LIMB_05,
    KEETA_LIMB_06,
    KEETA_LIMB_07,
    KEETA_LIMB_08,
    KEETA_LIMB_09,
    KEETA_LIMB_HEAD,
    KEETA_LIMB_0B,
    KEETA_LIMB_0C,
    KEETA_LIMB_0D,
    KEETA_LIMB_0E,
    KEETA_LIMB_0F,
    KEETA_LIMB_10,
    KEETA_LIMB_RIGHT_FOOT,
    KEETA_LIMB_12,
    KEETA_LIMB_13,
    KEETA_LIMB_LEFT_FOOT,
    KEETA_LIMB_MAX
} KeetaLimb;

typedef enum {
    KEETA_ANIM_STAGGER,
    KEETA_ANIM_WALK,
    KEETA_ANIM_SLASH,
    KEETA_ANIM_LAUGH,
    KEETA_ANIM_KNOCKBACK,
    KEETA_ANIM_JUMP,
    KEETA_ANIM_FALL,
    KEETA_ANIM_LAND,
    KEETA_ANIM_RISE,
    KEETA_ANIM_MAX
} KeetaAnimation;

// MM's unk_02B4 states that still exist in arena mode; damage is ignored while jumping or knocked back.
typedef enum {
    KEETA_STATE_STAGGER = 3,
    KEETA_STATE_WALK,
    KEETA_STATE_JUMP,
    KEETA_STATE_SLASH,
    KEETA_STATE_BLOCKED,
    KEETA_STATE_KNOCKBACK,
    KEETA_STATE_FROZEN = 10,
    KEETA_STATE_LAUGH
} KeetaState;

typedef enum {
    KEETA_DMGEFF_NONE,
    KEETA_DMGEFF_FREEZE,
    KEETA_DMGEFF_FIRE,
    KEETA_DMGEFF_ICE,
    KEETA_DMGEFF_LIGHT,
    KEETA_DMGEFF_HIT = 0xD,
    KEETA_DMGEFF_EXPLOSIVE,
    KEETA_DMGEFF_STUN
} KeetaDamageEffect;

typedef enum { KEETA_AURA_NONE, KEETA_AURA_FIRE, KEETA_AURA_LIGHT, KEETA_AURA_FROZEN } KeetaAura;

typedef struct {
    bool isEnabled;
    Vec3f pos;
    Vec3f velocity;
    Vec3f accel;
    Vec3s rot;
    f32 scale;
    s16 life;
} KeetaChip;

struct EnKeeta;

typedef void (*EnKeetaActionFunc)(struct EnKeeta*, PlayState*);

typedef struct EnKeeta {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[KEETA_LIMB_MAX];
    Vec3s morphTable[KEETA_LIMB_MAX];
    EnKeetaActionFunc actionFunc;
    s16 state;
    s16 actionTimer;
    s16 auraTimer;
    s16 aura;
    s16 bodyPartIndex;
    s32 animIndex;
    f32 animEndFrame;
    bool hasShedChips;
    bool isFightStarted;
    Vec3f knockback;
    Vec3f headPos;
    Vec3f leftFootPos;
    Vec3f rightFootPos;
    Vec3f bodyPartsPos[KEETA_BODYPART_MAX];
    KeetaChip chips[KEETA_EFFECT_COUNT];
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[7];
} EnKeeta;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sKeetaSkel[] = OBJ_BSB "object_bsb_Skel_00C3E0";
static const ALIGN_ASSET(2) char sStaggerAnim[] = OBJ_BSB "object_bsb_Anim_000400";
static const ALIGN_ASSET(2) char sWalkAnim[] = OBJ_BSB "object_bsb_Anim_000FF0";
static const ALIGN_ASSET(2) char sSlashAnim[] = OBJ_BSB "object_bsb_Anim_000C50";
static const ALIGN_ASSET(2) char sLaughAnim[] = OBJ_BSB "object_bsb_Anim_006C48";
static const ALIGN_ASSET(2) char sKnockbackAnim[] = OBJ_BSB "object_bsb_Anim_001390";
static const ALIGN_ASSET(2) char sJumpAnim[] = OBJ_BSB "object_bsb_Anim_0043A4";
static const ALIGN_ASSET(2) char sFallAnim[] = OBJ_BSB "object_bsb_Anim_004510";
static const ALIGN_ASSET(2) char sLandAnim[] = OBJ_BSB "object_bsb_Anim_001F90";
static const ALIGN_ASSET(2) char sRiseAnim[] = OBJ_BSB "object_bsb_Anim_00C790";
static const ALIGN_ASSET(2) char sRockChipDL[] = OBJ_KEEP "gameplay_keep_DL_06AB30";

static const char* const sAnimations[KEETA_ANIM_MAX] = {
    sStaggerAnim, sWalkAnim, sSlashAnim, sLaughAnim, sKnockbackAnim, sJumpAnim, sFallAnim, sLandAnim, sRiseAnim,
};

static const u8 sAnimationModes[KEETA_ANIM_MAX] = {
    ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_LOOP,
    ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_ONCE,
};

static ColliderJntSphElementInit sJntSphElementsInit[7] = {
    { { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
      { KEETA_LIMB_HEAD, { { 1000, 400, 0 }, 40 }, 100 } },
    { { ELEMTYPE_UNK0,
        { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_ON },
      { KEETA_LIMB_09, { { 0, 700, 200 }, 35 }, 100 } },
    { { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
      { KEETA_LIMB_06, { { 100, 600, 0 }, 35 }, 100 } },
    { { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_NONE },
      { KEETA_LIMB_03, { { 400, 200, 0 }, 40 }, 100 } },
    { { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
      { KEETA_LIMB_0D, { { 700, -100, 0 }, 35 }, 100 } },
    { { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
      { KEETA_LIMB_10, { { 200, 300, 0 }, 30 }, 100 } },
    { { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
      { KEETA_LIMB_13, { { 200, 300, 0 }, 30 }, 100 } },
};

static ColliderJntSphInit sJntSphInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_PLAYER, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    ARRAY_COUNT(sJntSphElementsInit),
    sJntSphElementsInit,
};

// MM entries multiply the weapon's own damage; OoT's are absolute. Projectiles only make him stagger.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, KEETA_DMGEFF_FREEZE),
    /* Deku stick    */ DMG_ENTRY(2, KEETA_DMGEFF_HIT),
    /* Slingshot     */ DMG_ENTRY(0, KEETA_DMGEFF_STUN),
    /* Explosive     */ DMG_ENTRY(2, KEETA_DMGEFF_EXPLOSIVE),
    /* Boomerang     */ DMG_ENTRY(0, KEETA_DMGEFF_STUN),
    /* Normal arrow  */ DMG_ENTRY(0, KEETA_DMGEFF_STUN),
    /* Hammer swing  */ DMG_ENTRY(2, KEETA_DMGEFF_HIT),
    /* Hookshot      */ DMG_ENTRY(0, KEETA_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, KEETA_DMGEFF_HIT),
    /* Master sword  */ DMG_ENTRY(2, KEETA_DMGEFF_HIT),
    /* Giant's Knife */ DMG_ENTRY(4, KEETA_DMGEFF_HIT),
    /* Fire arrow    */ DMG_ENTRY(0, KEETA_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(0, KEETA_DMGEFF_ICE),
    /* Light arrow   */ DMG_ENTRY(0, KEETA_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(0, KEETA_DMGEFF_STUN),
    /* Unk arrow 2   */ DMG_ENTRY(0, KEETA_DMGEFF_STUN),
    /* Unk arrow 3   */ DMG_ENTRY(0, KEETA_DMGEFF_STUN),
    /* Fire magic    */ DMG_ENTRY(0, KEETA_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(0, KEETA_DMGEFF_ICE),
    /* Light magic   */ DMG_ENTRY(0, KEETA_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, KEETA_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, KEETA_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(2, KEETA_DMGEFF_HIT),
    /* Giant spin    */ DMG_ENTRY(4, KEETA_DMGEFF_HIT),
    /* Master spin   */ DMG_ENTRY(4, KEETA_DMGEFF_HIT),
    /* Kokiri jump   */ DMG_ENTRY(2, KEETA_DMGEFF_HIT),
    /* Giant jump    */ DMG_ENTRY(8, KEETA_DMGEFF_HIT),
    /* Master jump   */ DMG_ENTRY(4, KEETA_DMGEFF_HIT),
    /* Unknown 1     */ DMG_ENTRY(0, KEETA_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, KEETA_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(2, KEETA_DMGEFF_HIT),
    /* Unknown 2     */ DMG_ENTRY(0, KEETA_DMGEFF_NONE),
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

static void Keeta_SetupWalk(EnKeeta* this);
static void Keeta_SetupJumpAttack(EnKeeta* this, PlayState* play);
static void Keeta_Stagger(EnKeeta* this, PlayState* play);

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

static void Keeta_ChangeAnim(EnKeeta* this, s32 animIndex) {
    AnimationHeader* animation = (AnimationHeader*)sAnimations[animIndex];

    this->animIndex = animIndex;
    this->animEndFrame = Animation_GetLastFrame(animation);
    Animation_Change(&this->skelAnime, animation, 1.0f, 0.0f, this->animEndFrame, sAnimationModes[animIndex], -2.0f);
}

static bool Keeta_IsAnimDone(EnKeeta* this) {
    return this->skelAnime.curFrame >= this->animEndFrame;
}

static void Keeta_StartMinibossMusic(EnKeeta* this) {
    if (this->isFightStarted) {
        return;
    }
    this->isFightStarted = true;
    func_800F5ACC(NA_BGM_MINI_BOSS);
}

static void Keeta_StopMinibossMusic(EnKeeta* this) {
    if (!this->isFightStarted) {
        return;
    }
    this->isFightStarted = false;
    func_800F5B58();
}

static void Keeta_SpawnChip(EnKeeta* this, Vec3f* pos, Vec3f* velocity, Vec3f* accel, f32 scale, s16 life) {
    for (s32 i = 0; i < KEETA_EFFECT_COUNT; i++) {
        KeetaChip* chip = &this->chips[i];

        if (chip->isEnabled) {
            continue;
        }
        chip->isEnabled = true;
        chip->pos = *pos;
        chip->velocity = *velocity;
        chip->accel = *accel;
        chip->scale = scale;
        chip->life = life;
        chip->rot.x = (s16)Rand_CenteredFloat(0x7530);
        chip->rot.y = (s16)Rand_CenteredFloat(0x7530);
        chip->rot.z = (s16)Rand_CenteredFloat(0x7530);
        return;
    }
}

// MM sheds a burst of bone chips from both feet only on his first heavy step.
static void Keeta_ShedChips(EnKeeta* this) {
    if (this->hasShedChips) {
        return;
    }
    this->hasShedChips = true;
    for (s32 i = 0; i < 10; i++) {
        Vec3f pos = (i % 2) == 0 ? this->rightFootPos : this->leftFootPos;
        Vec3f accel = { 0.0f, -1.0f, 0.0f };
        Vec3f velocity = { (Rand_ZeroOne() - 0.5f) * 4.0f, Rand_ZeroOne() * 10.0f + 4.0f,
                           (Rand_ZeroOne() - 0.5f) * 4.0f };

        pos.x += Rand_CenteredFloat(5.0f);
        pos.y += Rand_CenteredFloat(5.0f);
        pos.z += Rand_CenteredFloat(5.0f);
        Keeta_SpawnChip(this, &pos, &velocity, &accel, Rand_CenteredFloat(1.0f) * 0.002f + 0.005f,
                        (s32)Rand_ZeroFloat(10.0f) + 30);
    }
}

static void Keeta_Stomp(EnKeeta* this, PlayState* play, Vec3f* footPos) {
    f32 angle = (Rand_ZeroOne() - 0.5f) * 12.56f;
    Vec3f debrisPos = { Math_SinF(angle) * 5.0f + footPos->x, this->actor.floorHeight,
                        Math_CosF(angle) * 5.0f + footPos->z };
    Vec3f debrisVelocity = { 0.0f, 8.0f + (Rand_ZeroOne() - 0.5f) * 15.0f, 0.0f };
    Vec3f debrisAccel = { Rand_CenteredFloat(1.0f) * 0.5f, -1.5f, Rand_CenteredFloat(1.0f) * 0.5f };

    EffectSsHahen_Spawn(play, &debrisPos, &debrisVelocity, &debrisAccel, 0, Rand_ZeroOne() * 5.0f + 10.0f,
                        HAHEN_OBJECT_DEFAULT, 10, NULL);
    func_80033480(play, &debrisPos, 20.0f, 1, 300, 10, true);
    Keeta_ShedChips(this);
}

// Every footfall of his march shakes the ground near Link.
static void Keeta_MarchSteps(EnKeeta* this, PlayState* play) {
    bool isRightStep = Animation_OnFrame(&this->skelAnime, 4.0f) || Animation_OnFrame(&this->skelAnime, 8.0f);
    bool isLeftStep = Animation_OnFrame(&this->skelAnime, 2.0f) || Animation_OnFrame(&this->skelAnime, 6.0f);

    if (isRightStep || isLeftStep) {
        PlaySfx(&this->actor, MM_SE_EN_KTIA_WALK, NA_SE_EN_IRONNACK_WALK);
        Actor_RequestQuake(play, 1, 2);
        Keeta_Stomp(this, play, isRightStep ? &this->rightFootPos : &this->leftFootPos);
    }
    if (this->actor.xzDistToPlayer >= 400.0f || !Animation_OnFrame(&this->skelAnime, 1.0f)) {
        return;
    }
    s32 intensity = CLAMP(160.0f - this->actor.xzDistToPlayer * 0.3f, 0, 70);

    Rumble_Request(0.0f, intensity, 15, 70);
}

static void Keeta_SetupStagger(EnKeeta* this) {
    this->actor.speedXZ = 0.0f;
    Keeta_ChangeAnim(this, KEETA_ANIM_STAGGER);
    PlaySfx(&this->actor, MM_SE_EN_KITA_DAMAGE, NA_SE_EN_STAL_DAMAGE);
    this->actionTimer = 40;
    PlaySfx(&this->actor, MM_SE_EN_COMMON_FREEZE, NA_SE_EN_GOMA_JR_FREEZE);
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 40);
    this->state = KEETA_STATE_STAGGER;
    this->actionFunc = Keeta_Stagger;
}

static void Keeta_ShatterIce(EnKeeta* this, PlayState* play) {
    if (this->aura != KEETA_AURA_FROZEN || this->auraTimer == 0) {
        return;
    }
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < KEETA_BODYPART_MAX; i++) {
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &this->bodyPartsPos[i], 150, 150, 150, 250, 235, 245, 255,
                                       1.5f);
    }
    this->auraTimer = 0;
    this->aura = KEETA_AURA_NONE;
}

static void Keeta_Stagger(EnKeeta* this, PlayState* play) {
    if (Keeta_IsAnimDone(this) && this->actionTimer == 0) {
        Keeta_ShatterIce(this, play);
        Keeta_SetupWalk(this);
    }
}

static void Keeta_Walk(EnKeeta* this, PlayState* play);

static void Keeta_SetupWalk(EnKeeta* this) {
    this->actor.speedXZ = 2.0f;
    Keeta_ChangeAnim(this, KEETA_ANIM_WALK);
    this->actionTimer = Rand_S16Offset(0, 30);
    this->state = KEETA_STATE_WALK;
    this->actionFunc = Keeta_Walk;
}

static void Keeta_Slash(EnKeeta* this, PlayState* play);

static void Keeta_SetupSlash(EnKeeta* this) {
    this->actor.speedXZ = 0.0f;
    PlaySfx(&this->actor, MM_SE_EN_KITA_ATTACK_W, NA_SE_EN_IRONNACK_SWING_AXE);
    Keeta_ChangeAnim(this, KEETA_ANIM_SLASH);
    this->state = KEETA_STATE_SLASH;
    this->actionFunc = Keeta_Slash;
}

// Marches at Link; leaps when Link stands well above or below him, slashes when Link is close and level.
static void Keeta_Walk(EnKeeta* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 heightToPlayer = fabsf(player->actor.world.pos.y - this->actor.world.pos.y);

    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 1, 0xBB8, 0);
    if (this->actor.xzDistToPlayer < KEETA_MUSIC_RANGE) {
        Keeta_StartMinibossMusic(this);
    }
    if (this->actionTimer == 0 && heightToPlayer >= 100.0f && this->actor.xzDistToPlayer < 500.0f) {
        Keeta_SetupJumpAttack(this, play);
        return;
    }
    Keeta_MarchSteps(this, play);
    bool isInReach = this->actionTimer == 0 && Actor_IsFacingPlayer(&this->actor, 0x11C7) && heightToPlayer < 20.0f &&
                     this->actor.xzDistToPlayer < 180.0f;

    if (isInReach) {
        Keeta_SetupSlash(this);
    }
}

static void Keeta_JumpAttack(EnKeeta* this, PlayState* play);

static void Keeta_SetupJumpAttack(EnKeeta* this, PlayState* play) {
    this->actor.speedXZ = 0.0f;
    PlaySfx(&this->actor, MM_SE_EN_KITA_ATTACK_W, NA_SE_EN_IRONNACK_SWING_AXE);
    this->actor.velocity.y = 85.0f;
    this->actor.gravity = -8.0f;
    Keeta_ChangeAnim(this, KEETA_ANIM_JUMP);
    Actor_SpawnFloorDustRing(play, &this->actor, &this->rightFootPos, 5.0f, 10, 8.0f, 1000, 100, true);
    Actor_SpawnFloorDustRing(play, &this->actor, &this->leftFootPos, 5.0f, 10, 8.0f, 1000, 100, true);
    PlaySfx(&this->actor, MM_SE_EN_TEKU_JUMP, NA_SE_EN_STAL_JUMP);
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->state = KEETA_STATE_JUMP;
    this->actionFunc = Keeta_JumpAttack;
}

static void Keeta_Land(EnKeeta* this, PlayState* play) {
    if (this->animIndex == KEETA_ANIM_LAND) {
        Keeta_ChangeAnim(this, KEETA_ANIM_RISE);
        return;
    }
    if (!Keeta_IsAnimDone(this)) {
        return;
    }
    this->actor.flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.gravity = -2.0f;
    Keeta_SetupWalk(this);
    this->actionTimer = 10;
}

// Homes in on Link while rising, then crashes down on him.
static void Keeta_JumpAttack(EnKeeta* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->actor.velocity.y > 0.0f) {
        Math_ApproachF(&this->actor.world.pos.x, player->actor.world.pos.x, 0.5f, 30.0f);
        Math_ApproachF(&this->actor.world.pos.z, player->actor.world.pos.z, 0.5f, 30.0f);
        return;
    }
    this->actor.gravity = -8.0f;
    if (this->animIndex != KEETA_ANIM_FALL) {
        Keeta_ChangeAnim(this, KEETA_ANIM_FALL);
    }
    bool isCrushingPlayer = fabsf(player->actor.world.pos.y - this->actor.world.pos.y) < 40.0f &&
                            this->actor.xzDistToPlayer < 70.0f && player->invincibilityTimer == 0;

    if (isCrushingPlayer) {
        Actor_SetPlayerKnockbackLarge(play, &this->actor, 2.0f, this->actor.yawTowardsPlayer, 5.0f, 16);
    }
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return;
    }
    Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, this->actor.shape.shadowScale, 10, 8.0f, 2000,
                             100, true);
    Actor_RequestQuakeAndRumble(&this->actor, play, 10, 10);
    PlaySfx(&this->actor, MM_SE_EN_EYEGOLE_ATTACK, NA_SE_EN_IRONNACK_HIT_GND);
    Keeta_ChangeAnim(this, KEETA_ANIM_LAND);
    this->actionFunc = Keeta_Land;
}

static void Keeta_Blocked(EnKeeta* this, PlayState* play) {
    if (this->actionTimer == 0) {
        Keeta_SetupWalk(this);
    }
}

// A shield stops the blade: he rewinds the swing and pauses.
static void Keeta_SetupBlocked(EnKeeta* this) {
    this->actor.speedXZ = 0.0f;
    Animation_Change(&this->skelAnime, (AnimationHeader*)sSlashAnim, -1.0f, this->skelAnime.curFrame - 1.0f, 0.0f,
                     ANIMMODE_ONCE, 0.0f);
    this->actionTimer = 10;
    PlaySfx(&this->actor, MM_SE_EN_KTIA_PAUSE_K, NA_SE_EN_STAL_SAKEBI);
    this->state = KEETA_STATE_BLOCKED;
    this->actionFunc = Keeta_Blocked;
}

static bool Keeta_IsSlashShielded(EnKeeta* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3s* hit = &this->collider.elements[1].info.bumper.hitPos;
    Vec3f hitPos = { hit->x, hit->y, hit->z };
    s16 facingOffset = BINANG_ROT180(player->actor.shape.rot.y - this->actor.shape.rot.y);
    bool isShielding = (player->stateFlags1 & PLAYER_STATE1_SHIELDING) &&
                       Math_Vec3f_DistXYZ(&hitPos, &player->actor.world.pos) <= 70.0f && ABS(facingOffset) < 0x2000;

    return (this->collider.base.atFlags & AT_BOUNCED) || isShielding;
}

static void Keeta_Slash(EnKeeta* this, PlayState* play) {
    if (!Keeta_IsSlashShielded(this, play)) {
        if (Keeta_IsAnimDone(this)) {
            Keeta_SetupWalk(this);
        }
        return;
    }
    Vec3s* hit = &this->collider.elements[1].info.bumper.hitPos;
    Vec3f hitPos = { hit->x, hit->y, hit->z };

    this->collider.base.atFlags &= ~(AT_BOUNCED | AT_HIT);
    EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_METAL, &hitPos);
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_BOUND);
    Keeta_SetupBlocked(this);
}

static void Keeta_Knockback(EnKeeta* this, PlayState* play) {
    Keeta_Stomp(this, play, &this->rightFootPos);
    Keeta_Stomp(this, play, &this->leftFootPos);
    if (!Keeta_IsAnimDone(this)) {
        return;
    }
    if (this->actor.colChkInfo.health < 10) {
        Keeta_SetupJumpAttack(this, play);
    } else {
        Keeta_SetupWalk(this);
    }
}

static void Keeta_SetupKnockback(EnKeeta* this, PlayState* play) {
    Keeta_ShatterIce(this, play);
    this->actor.speedXZ = 0.0f;
    this->knockback.x = Math_SinS(this->actor.yawTowardsPlayer) * -30.0f;
    this->knockback.z = Math_CosS(this->actor.yawTowardsPlayer) * -30.0f;
    PlaySfx(&this->actor, MM_SE_EN_KITA_DAMAGE, NA_SE_EN_STAL_DAMAGE);
    Keeta_ChangeAnim(this, KEETA_ANIM_KNOCKBACK);
    this->hasShedChips = true;
    this->state = KEETA_STATE_KNOCKBACK;
    this->actionFunc = Keeta_Knockback;
}

static void Keeta_Frozen(EnKeeta* this, PlayState* play) {
    if (this->actionTimer == 0) {
        Keeta_ShatterIce(this, play);
        Keeta_SetupWalk(this);
    }
}

static void Keeta_SetupFrozen(EnKeeta* this) {
    this->actionTimer = 40;
    this->actor.speedXZ = 0.0f;
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 120, COLORFILTER_OPA, 40);
    PlaySfx(&this->actor, MM_SE_EN_COMMON_FREEZE, NA_SE_EN_GOMA_JR_FREEZE);
    this->state = KEETA_STATE_FROZEN;
    this->actionFunc = Keeta_Frozen;
}

static void Keeta_Laugh(EnKeeta* this, PlayState* play) {
    if (Keeta_IsAnimDone(this)) {
        Keeta_SetupWalk(this);
    }
}

static void Keeta_SetupLaugh(EnKeeta* this) {
    this->actor.speedXZ = 0.0f;
    PlaySfx(&this->actor, MM_SE_EN_KITA_LAUGH_K, NA_SE_EN_STAL_WARAU);
    Keeta_ChangeAnim(this, KEETA_ANIM_LAUGH);
    this->state = KEETA_STATE_LAUGH;
    this->actionFunc = Keeta_Laugh;
}

// The arena ending 2Ship uses: no defeat cutscene, he crumbles and leaves a reward behind.
static void Keeta_Die(EnKeeta* this, PlayState* play) {
    Enemy_StartFinishingBlow(play, &this->actor);
    PlaySfx(&this->actor, MM_SE_EN_KITA_DEAD, NA_SE_EN_STAL_DEAD);
    Keeta_StopMinibossMusic(this);
    for (s32 i = 0; i < KEETA_BODYPART_MAX; i += 2) {
        EffectSsDeadDb_Spawn(play, &this->bodyPartsPos[i], &sZeroVec, &sZeroVec, 150, 0, 255, 255, 255, 255, 0, 0, 255,
                             1, 9, i == 0);
    }
    Item_DropCollectible(play, &this->actor.world.pos, ITEM00_RUPEE_PURPLE);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

static void Keeta_SpillBlood(EnKeeta* this, PlayState* play) {
    for (s32 i = 0; i < ARRAY_COUNT(this->colliderElements); i++) {
        ColliderInfo* info = &this->collider.elements[i].info;

        if (info->bumperFlags & BUMP_HIT) {
            Vec3f hitPos = { info->bumper.hitPos.x, info->bumper.hitPos.y, info->bumper.hitPos.z };

            CollisionCheck_BlueBlood(play, NULL, &hitPos);
        }
    }
}

static void Keeta_TakeDamage(EnKeeta* this, PlayState* play) {
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    Actor_ApplyDamage(&this->actor);
    if (this->actor.colChkInfo.health <= 0) {
        Keeta_Die(this, play);
        return;
    }
    Keeta_SpillBlood(this, play);
    Keeta_SetupKnockback(this, play);
}

static bool Keeta_IsVulnerable(EnKeeta* this) {
    return this->state != KEETA_STATE_JUMP && this->state != KEETA_STATE_KNOCKBACK;
}

static void Keeta_CheckSlashLanded(EnKeeta* this) {
    ColliderInfo* blade = &this->collider.elements[1].info;

    if ((this->collider.base.atFlags & AT_BOUNCED) || !(blade->toucherFlags & TOUCH_HIT)) {
        return;
    }
    blade->toucherFlags &= ~TOUCH_HIT;
    if (this->state != KEETA_STATE_LAUGH && this->state != KEETA_STATE_BLOCKED) {
        Keeta_SetupLaugh(this);
    }
}

static void Keeta_UpdateDamage(EnKeeta* this, PlayState* play) {
    if (!Keeta_IsVulnerable(this)) {
        return;
    }
    Keeta_CheckSlashLanded(this);
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    Keeta_StartMinibossMusic(this);
    switch (this->actor.colChkInfo.damageEffect) {
        case KEETA_DMGEFF_HIT:
        case KEETA_DMGEFF_EXPLOSIVE:
            Keeta_TakeDamage(this, play);
            return;
        case KEETA_DMGEFF_FREEZE:
            Keeta_SetupFrozen(this);
            return;
        case KEETA_DMGEFF_FIRE:
            this->auraTimer = 40;
            this->aura = KEETA_AURA_FIRE;
            break;
        case KEETA_DMGEFF_ICE:
            this->auraTimer = 80;
            this->aura = KEETA_AURA_FROZEN;
            break;
        case KEETA_DMGEFF_LIGHT:
            this->auraTimer = 40;
            this->aura = KEETA_AURA_LIGHT;
            break;
        case KEETA_DMGEFF_STUN:
            break;
        default:
            return;
    }
    if (this->state != KEETA_STATE_STAGGER) {
        Keeta_SetupStagger(this);
    }
}

static void Keeta_UpdateChips(EnKeeta* this) {
    for (s32 i = 0; i < KEETA_EFFECT_COUNT; i++) {
        KeetaChip* chip = &this->chips[i];

        if (!chip->isEnabled) {
            continue;
        }
        chip->rot.x += 0x100;
        chip->rot.z += 0x130;
        Math_Vec3f_Sum(&chip->pos, &chip->velocity, &chip->pos);
        chip->velocity.y += chip->accel.y;
        if (chip->life != 0) {
            chip->life--;
        } else {
            chip->isEnabled = false;
        }
    }
}

// The knockback slides him away from Link while his feet touch the ground.
static void Keeta_ApplyKnockback(EnKeeta* this) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return;
    }
    this->actor.world.pos.x += this->knockback.x;
    this->actor.world.pos.z += this->knockback.z;
    Math_ApproachZeroF(&this->knockback.x, 1.0f, 2.0f);
    Math_ApproachZeroF(&this->knockback.z, 1.0f, 2.0f);
}

static void EnKeeta_Init(Actor* thisx, PlayState* play) {
    EnKeeta* this = (EnKeeta*)thisx;

    Actor_SetScale(&this->actor, KEETA_SCALE);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    this->actor.colChkInfo.health = KEETA_HEALTH;
    this->actor.gravity = -2.0f;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 25.0f);
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sKeetaSkel, (AnimationHeader*)sWalkAnim, this->jointTable,
                   this->morphTable, KEETA_LIMB_MAX);
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sJntSphInit, this->colliderElements);
    Keeta_SetupWalk(this);
}

static void EnKeeta_Destroy(Actor* thisx, PlayState* play) {
    EnKeeta* this = (EnKeeta*)thisx;

    Keeta_StopMinibossMusic(this);
    Collider_DestroyJntSph(play, &this->collider);
}

static void EnKeeta_Update(Actor* thisx, PlayState* play) {
    EnKeeta* this = (EnKeeta*)thisx;

    DECR(this->actionTimer);
    DECR(this->auraTimer);
    if (this->state != KEETA_STATE_FROZEN) {
        SkelAnime_Update(&this->skelAnime);
    }
    this->actionFunc(this, play);
    Actor_MoveXZGravity(&this->actor);
    Keeta_UpdateDamage(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    this->actor.shape.rot.y = this->actor.world.rot.y;
    this->actor.focus.pos.x = this->headPos.x + Math_SinS(this->actor.world.rot.y) * 20.0f;
    this->actor.focus.pos.y = this->headPos.y;
    this->actor.focus.pos.z = this->headPos.z + Math_CosS(this->actor.world.rot.y) * 20.0f;
    this->actor.focus.rot = this->actor.world.rot;
    Keeta_ApplyKnockback(this);
    Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, 60.0f, 40.0f, BGCHECK_UPDATE_WALL_FLOOR_WATER);
    Keeta_UpdateChips(this);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    if (this->state == KEETA_STATE_SLASH || this->state == KEETA_STATE_WALK) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
}

static bool IsKeetaBodyPart(s32 limbIndex) {
    return limbIndex >= KEETA_LIMB_02 && limbIndex <= KEETA_LIMB_LEFT_FOOT && limbIndex != KEETA_LIMB_0B &&
           limbIndex != KEETA_LIMB_0E;
}

static void Keeta_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnKeeta* this = (EnKeeta*)thisx;

    if (limbIndex == KEETA_LIMB_HEAD) {
        Matrix_MultVec3f(&sZeroVec, &this->headPos);
    } else if (limbIndex == KEETA_LIMB_RIGHT_FOOT) {
        Matrix_MultVec3f(&sZeroVec, &this->rightFootPos);
    } else if (limbIndex == KEETA_LIMB_LEFT_FOOT) {
        Matrix_MultVec3f(&sZeroVec, &this->leftFootPos);
    }
    if (IsKeetaBodyPart(limbIndex)) {
        Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[this->bodyPartIndex]);
        this->bodyPartIndex = (this->bodyPartIndex + 1) % KEETA_BODYPART_MAX;
    }
    Collider_UpdateSpheres(limbIndex, &this->collider);
}

static void Keeta_DrawChips(EnKeeta* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    for (s32 i = 0; i < KEETA_EFFECT_COUNT; i++) {
        KeetaChip* chip = &this->chips[i];

        if (!chip->isEnabled) {
            continue;
        }
        Matrix_Translate(chip->pos.x, chip->pos.y, chip->pos.z, MTXMODE_NEW);
        Matrix_RotateX(BINANG_TO_RAD(chip->rot.x), MTXMODE_APPLY);
        Matrix_RotateY(BINANG_TO_RAD(chip->rot.y), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(chip->rot.z), MTXMODE_APPLY);
        Matrix_Scale(chip->scale, chip->scale, chip->scale, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRockChipDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM's fire and light-orb damage effects become OoT's flame billboard, tinted.
static void Keeta_DrawAura(EnKeeta* this, PlayState* play) {
    bool isLight = this->aura == KEETA_AURA_LIGHT;
    u8 alpha = CLAMP_MAX(this->auraTimer * 0.05f, 1.0f) * 255.0f;
    MtxF billboard;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_RotateY(BINANG_TO_RAD(BINANG_ROT180(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)))), MTXMODE_NEW);
    Matrix_Scale(0.012f, 0.012f, 0.012f, MTXMODE_APPLY);
    Matrix_Get(&billboard);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                           (play->gameplayFrames * -20) % 512, 0x20, 0x80));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, isLight ? 255 : 0, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, isLight ? 255 : 10, isLight ? 100 : 0, 255);
    for (s32 i = 0; i < KEETA_BODYPART_MAX; i++) {
        billboard.xw = this->bodyPartsPos[i].x;
        billboard.yw = this->bodyPartsPos[i].y;
        billboard.zw = this->bodyPartsPos[i].z;
        Matrix_Put(&billboard);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM's opaque Scene_SetRenderModeXlu table for segment 0x0C: object_bsb calls entries 0 and 2.
static Gfx sRenderModeOpaDL[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

static void EnKeeta_Draw(Actor* thisx, PlayState* play) {
    EnKeeta* this = (EnKeeta*)thisx;
    bool hasFlameAura = this->auraTimer != 0 && (this->aura == KEETA_AURA_FIRE || this->aura == KEETA_AURA_LIGHT);

    Keeta_DrawChips(this, play);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    CLOSE_DISPS(play->state.gfxCtx);
    SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, NULL, Keeta_PostLimbDraw, this);
    if (hasFlameAura) {
        Keeta_DrawAura(this, play);
    }
}

static bool CanBeGoron(void) {
    int32_t goronMask = sApi->GetRandoItem(GORON_MASK_KEY);

    return goronMask >= 0 && sApi->LogicHasItem(goronMask);
}

// MM's CanKillEnemy(ACTOR_EN_BSB) without the bow, which only matters for the graveyard race.
static bool Keeta_CanKillInLogic(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RG_STICKS) ||
           sApi->LogicHasCapability("HasExplosives") || sApi->LogicCanUseItem(RG_MEGATON_HAMMER) || CanBeGoron();
}

#define SOUL_FLAME_DL "__OTR__objects/object_gi_fire/gGiBlueFireFlameDL"

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

// Segments the enemy's lists read but only its actor fills; four entries cover the indexed render-mode table.
static Gfx sSoulEmptyDL[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

// The soul shows its enemy in an idle loop, as Majora's Mask souls do.
static void DrawSoul(PlayState* play, GetItemEntry* getItemEntry) {
    static SkelAnime sSoulSkelAnime;
    static Vec3s sSoulJointTable[KEETA_LIMB_MAX];
    static Vec3s sSoulMorphTable[KEETA_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_Init(play, &sSoulSkelAnime, (SkeletonHeader*)sKeetaSkel, (AnimationHeader*)sWalkAnim, sSoulJointTable,
                       sSoulMorphTable, KEETA_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    // Animations place the root far from the origin; the soul floats on its root instead.
    Matrix_Translate(-sSoulJointTable[0].x, -sSoulJointTable[0].y, -sSoulJointTable[0].z, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0B, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
    SkelAnime_DrawOpa(play, sSoulSkelAnime.skeleton, sSoulSkelAnime.jointTable, NULL, NULL, NULL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterSoul(void) {
    static const SOHCustomItemRandomizer sSoulRandomizer = {
        .structSize = sizeof(SOHCustomItemRandomizer),
        .flags = SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT,
        .type = SOH_CUSTOM_ITEM_TYPE_ITEM,
    };
    SOHCustomItemDefinition soul = Z64Items_Define(KEETA_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Captain Keeta";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Captain Keeta%w!&Captain Keeta can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " KEETA_SOUL_ITEM_KEY);
    }
}

static void RegisterKeeta(void) {
    SOHActorDefinition keeta = { 0 };

    keeta.structSize = sizeof(keeta);
    keeta.key = KEETA_KEY;
    keeta.description = "Captain Keeta";
    keeta.category = ACTORCAT_ENEMY;
    keeta.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                       ACTOR_FLAG_DRAW_CULLING_DISABLED;
    keeta.objectId = OBJECT_GAMEPLAY_KEEP;
    keeta.instanceSize = sizeof(EnKeeta);
    keeta.init = EnKeeta_Init;
    keeta.destroy = EnKeeta_Destroy;
    keeta.update = EnKeeta_Update;
    keeta.draw = EnKeeta_Draw;
    keeta.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_MINIBOSS | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    keeta.naviHint = "Captain Keeta&%cArrows only make him flinch. Cut him down with your %rsword%c or "
                     "%rexplosives%c!%w";
    keeta.soulKey = KEETA_KEY;
    keeta.logicKill = Keeta_CanKillInLogic;
    if (sApi->RegisterActor(&keeta) < 0) {
        sApi->Log("Could not register " KEETA_KEY);
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
    RegisterKeeta();
}
