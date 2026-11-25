// Majora's Mask's Takkuri (En_Thiefbird) as an Unbound custom enemy, drawn from mm.o2r.
// Its strike knocks rupees, bombs and arrows out of Link and it swoops on them; killing it gives the loot back.
// Bottle and sword theft needs MM's Ikana nest and stolen-item save slots, so it never steals outright.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define TAKKURI_KEY "mm.takkuri"
#define TAKKURI_SOUL_ITEM_KEY "soul.mm.takkuri"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_THIEFBIRD "__OTR__objects/object_thiefbird/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_CEILING_FLOOR ((1 << 0) | (1 << 1) | (1 << 2))

#define MM_SE_EN_THIEFBIRD_VOICE 0x3A97
#define MM_SE_EN_THIEFBIRD_DAMAGE 0x3A98
#define MM_SE_EN_THIEFBIRD_DEAD 0x3A99

#define TAKKURI_HEALTH 24
#define TAKKURI_FEATHER_COUNT 40
#define TAKKURI_LOOT_TYPES 6
#define TAKKURI_DROP_SLOTS 8
#define TAKKURI_BODYPART_MAX 11
#define TAKKURI_BODYPART_BEAK 9

typedef enum {
    TAKKURI_LIMB_NONE,
    TAKKURI_LIMB_BODY,
    TAKKURI_LIMB_LEFT_WING_BASE,
    TAKKURI_LIMB_LEFT_WING_MIDDLE,
    TAKKURI_LIMB_LEFT_WING_TIP,
    TAKKURI_LIMB_RIGHT_WING_BASE,
    TAKKURI_LIMB_RIGHT_WING_MIDDLE,
    TAKKURI_LIMB_RIGHT_WING_TIP,
    TAKKURI_LIMB_NECK,
    TAKKURI_LIMB_HEAD,
    TAKKURI_LIMB_RIGHT_EAR,
    TAKKURI_LIMB_LEFT_EAR,
    TAKKURI_LIMB_LOWER_BEAK,
    TAKKURI_LIMB_EYES,
    TAKKURI_LIMB_LEGS,
    TAKKURI_LIMB_FEET,
    TAKKURI_LIMB_STOLEN_ITEM,
    TAKKURI_LIMB_MAX
} TakkuriLimb;

typedef enum {
    TAKKURI_DMGEFF_NONE,
    TAKKURI_DMGEFF_STUN,
    TAKKURI_DMGEFF_FIRE,
    TAKKURI_DMGEFF_FREEZE,
    TAKKURI_DMGEFF_LIGHT
} TakkuriDamageEffect;

typedef struct {
    Vec3f pos;
    Vec3f velocity;
    f32 scale;
    s16 age;
    s16 yaw;
    s16 roll;
    s16 timer;
} TakkuriFeather;

struct EnTakkuri;

typedef void (*EnTakkuriActionFunc)(struct EnTakkuri*, PlayState*);

typedef struct EnTakkuri {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[TAKKURI_LIMB_MAX];
    Vec3s morphTable[TAKKURI_LIMB_MAX];
    EnTakkuriActionFunc actionFunc;
    s16 timer;
    s16 targetPitch;
    s16 targetYaw;
    s16 neckRoll;
    s16 lootCount[TAKKURI_LOOT_TYPES];
    Gfx* earDL;
    EnItem00* targetLoot;
    Vec3f bodyPartsPos[TAKKURI_BODYPART_MAX];
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[3];
    TakkuriFeather feathers[TAKKURI_FEATHER_COUNT];
} EnTakkuri;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sTakkuriSkel[] = OBJ_THIEFBIRD "gTakkuriSkel";
static const ALIGN_ASSET(2) char sTakkuriFlyAnim[] = OBJ_THIEFBIRD "gTakkuriFlyAnim";
static const ALIGN_ASSET(2) char sTakkuriFlyWithItemAnim[] = OBJ_THIEFBIRD "gTakkuriFlyWithItemAnim";
static const ALIGN_ASSET(2) char sTakkuriAttackAnim[] = OBJ_THIEFBIRD "gTakkuriAttackAnim";
static const ALIGN_ASSET(2) char sTakkuriDeathAnim[] = OBJ_THIEFBIRD "gTakkuriDeathAnim";
static const ALIGN_ASSET(2) char sTakkuriFeatherMaterialDL[] = OBJ_THIEFBIRD "gTakkuriFeatherMaterialDL";
static const ALIGN_ASSET(2) char sTakkuriFeatherModelDL[] = OBJ_THIEFBIRD "gTakkuriFeatherModelDL";

static ColliderJntSphElementInit sElementsInit[3] = {
    {
        { ELEMTYPE_UNK0,
          { 0xF7CFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x00 },
          { 0xF7CFFFFF, 0x00, 0x00 },
          TOUCH_NONE | TOUCH_SFX_NORMAL,
          BUMP_ON,
          OCELEM_ON },
        { TAKKURI_LIMB_BODY, { { 0, 0, 0 }, 24 }, 100 },
    },
    {
        { ELEMTYPE_UNK0,
          { 0xF7CFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x00 },
          { 0xF7CFFFFF, 0x00, 0x00 },
          TOUCH_NONE | TOUCH_SFX_NORMAL,
          BUMP_ON,
          OCELEM_ON },
        { TAKKURI_LIMB_HEAD, { { 900, -600, 0 }, 20 }, 100 },
    },
    {
        { ELEMTYPE_UNK0,
          { 0xF7CFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x04 },
          { 0xF7CFFFFF, 0x00, 0x00 },
          TOUCH_ON | TOUCH_SFX_HARD,
          BUMP_ON,
          OCELEM_ON },
        { TAKKURI_LIMB_LOWER_BEAK, { { 1200, 0, 0 }, 9 }, 100 },
    },
};

static ColliderJntSphInit sColliderInit = {
    { COLTYPE_HIT3, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_JNTSPH },
    3,
    sElementsInit,
};

static CollisionCheckInfoInit sColChkInfoInit = { TAKKURI_HEALTH, 15, 30, 30 };

// MM entries multiply the weapon's own damage; OoT's are absolute: MM's multiplier times OoT's usual weapon damage.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, TAKKURI_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, TAKKURI_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, TAKKURI_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, TAKKURI_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, TAKKURI_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, TAKKURI_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, TAKKURI_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(2, TAKKURI_DMGEFF_NONE),
    /* Kokiri sword  */ DMG_ENTRY(1, TAKKURI_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, TAKKURI_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, TAKKURI_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(4, TAKKURI_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, TAKKURI_DMGEFF_FREEZE),
    /* Light arrow   */ DMG_ENTRY(4, TAKKURI_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, TAKKURI_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, TAKKURI_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, TAKKURI_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(4, TAKKURI_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(4, TAKKURI_DMGEFF_FREEZE),
    /* Light magic   */ DMG_ENTRY(4, TAKKURI_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, TAKKURI_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, TAKKURI_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(2, TAKKURI_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(8, TAKKURI_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(4, TAKKURI_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, TAKKURI_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, TAKKURI_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, TAKKURI_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, TAKKURI_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, TAKKURI_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(4, TAKKURI_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, TAKKURI_DMGEFF_NONE),
};

static const s16 sLootTypes[TAKKURI_LOOT_TYPES] = {
    ITEM00_ARROWS_MEDIUM, ITEM00_BOMBS_A, ITEM00_RUPEE_GREEN, ITEM00_RUPEE_BLUE, ITEM00_RUPEE_RED, ITEM00_RUPEE_PURPLE,
};

static const s8 sLimbToBodyPart[TAKKURI_LIMB_MAX] = {
    [TAKKURI_LIMB_NONE] = -1,
    [TAKKURI_LIMB_BODY] = 0,
    [TAKKURI_LIMB_LEFT_WING_BASE] = -1,
    [TAKKURI_LIMB_LEFT_WING_MIDDLE] = 1,
    [TAKKURI_LIMB_LEFT_WING_TIP] = 3,
    [TAKKURI_LIMB_RIGHT_WING_BASE] = -1,
    [TAKKURI_LIMB_RIGHT_WING_MIDDLE] = 2,
    [TAKKURI_LIMB_RIGHT_WING_TIP] = 5,
    [TAKKURI_LIMB_NECK] = -1,
    [TAKKURI_LIMB_HEAD] = -1,
    [TAKKURI_LIMB_RIGHT_EAR] = 7,
    [TAKKURI_LIMB_LEFT_EAR] = 8,
    [TAKKURI_LIMB_LOWER_BEAK] = 9,
    [TAKKURI_LIMB_EYES] = -1,
    [TAKKURI_LIMB_LEGS] = -1,
    [TAKKURI_LIMB_FEET] = 10,
    [TAKKURI_LIMB_STOLEN_ITEM] = -1,
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sBeakTipOffset = { 1000.0f, 0.0f, 0.0f };
static Vec3f sWingTipOffset = { 2000.0f, 0.0f, 0.0f };
static Vec3f sFeatherGravity = { 0.0f, -0.15f, 0.0f };
static Vec3f sDeathSfxPos;

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

static void Takkuri_FlyIdle(EnTakkuri* this, PlayState* play);
static void Takkuri_Swoop(EnTakkuri* this, PlayState* play);
static void Takkuri_Dying(EnTakkuri* this, PlayState* play);
static void Takkuri_Falling(EnTakkuri* this, PlayState* play);
static void Takkuri_Flinch(EnTakkuri* this, PlayState* play);
static void Takkuri_Retreat(EnTakkuri* this, PlayState* play);
static void Takkuri_GatherLoot(EnTakkuri* this, PlayState* play);

// Falls back to the closest OoT sound when the mm_assets sound service is missing.
static void PlaySfxAt(Actor* actor, Vec3f* pos, u16 mmSfx, u16 ootSfx) {
    if (!sIsMmSfxSearched && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
        sIsMmSfxSearched = true;
    }
    if (sPlayMmSfx != NULL && sPlayMmSfx(mmSfx, pos)) {
        return;
    }
    Audio_PlayActorSound2(actor, ootSfx);
}

static void PlaySfx(Actor* actor, u16 mmSfx, u16 ootSfx) {
    PlaySfxAt(actor, &actor->projectedPos, mmSfx, ootSfx);
}

static void Takkuri_SpawnFeathers(EnTakkuri* this, s32 count) {
    for (s32 i = 0; i < TAKKURI_FEATHER_COUNT && count > 0; i++) {
        TakkuriFeather* feather = &this->feathers[i];

        if (feather->timer != 0) {
            continue;
        }
        feather->timer = (s32)Rand_ZeroFloat(20.0f) + 40;
        feather->pos.x = Rand_CenteredFloat(30.0f) + this->actor.focus.pos.x;
        feather->pos.y = Rand_CenteredFloat(30.0f) + this->actor.focus.pos.y;
        feather->pos.z = Rand_CenteredFloat(30.0f) + this->actor.focus.pos.z;
        feather->velocity.x = Rand_CenteredFloat(5.0f);
        feather->velocity.y = Rand_ZeroOne() + 2.0f;
        feather->velocity.z = Rand_CenteredFloat(5.0f);
        feather->age = Rand_ZeroFloat(1000.0f);
        feather->scale = (Rand_ZeroFloat(20.0f) + 40.0f) * 0.0001f;
        feather->yaw = (s32)Rand_Next() >> 0x10;
        count--;
    }
}

// Feathers drift down swaying, half of them turning each way.
static void Takkuri_UpdateFeathers(EnTakkuri* this) {
    for (s32 i = 0; i < TAKKURI_FEATHER_COUNT; i++) {
        TakkuriFeather* feather = &this->feathers[i];

        if (feather->timer == 0) {
            continue;
        }
        feather->timer--;
        Math_Vec3f_Sum(&feather->pos, &feather->velocity, &feather->pos);
        Math_Vec3f_Sum(&feather->velocity, &sFeatherGravity, &feather->velocity);
        feather->age++;
        Math_StepToF(&feather->velocity.x, 0.0f, 0.05f);
        Math_StepToF(&feather->velocity.z, 0.0f, 0.05f);
        feather->velocity.y = MAX(feather->velocity.y, -0.5f);
        f32 turn = (i % 2) != 0 ? -1.0f : 1.0f;

        feather->roll = Math_SinS(feather->age * 0x7D0) * 0x2000;
        feather->yaw += (s16)(0x666 * fabsf(Math_SinS(feather->age * 0xBB8)) * turn);
    }
}

// Spreads `count` copies of an item over the eight drop slots, five apart, skipping the taken ones.
static void Takkuri_PlaceDrops(s32 drops[], s32 count, s32 item) {
    s32 index = 0;

    for (s32 i = 0; i < count; i++) {
        for (s32 tries = 0; drops[index] != ITEM00_NONE && tries <= TAKKURI_DROP_SLOTS; tries++) {
            index = (index + 5) % TAKKURI_DROP_SLOTS;
        }
        drops[index] = item;
        index = (index + 5) % TAKKURI_DROP_SLOTS;
    }
}

static s32 Takkuri_TakeRupees(s32* budget, s32* slotsLeft, s32 value) {
    s32 count = MIN(*budget / value, *slotsLeft);

    *slotsLeft -= count;
    *budget -= count * value;
    return count;
}

static void Takkuri_ScatterDrops(PlayState* play, s32 drops[], s32 dropCount) {
    Player* player = GET_PLAYER(play);
    s16 angle = 0;

    for (s32 i = 0; i < TAKKURI_DROP_SLOTS; i++) {
        if (drops[i] == ITEM00_NONE) {
            continue;
        }
        Vec3f pos = { Math_SinS(angle) * 40.0f + player->actor.world.pos.x, player->actor.world.pos.y + 20.0f,
                      Math_CosS(angle) * 40.0f + player->actor.world.pos.z };
        EnItem00* drop = Item_DropCollectible(play, &pos, drops[i]);

        if (drop != NULL) {
            drop->actor.velocity.y = Rand_ZeroFloat(3.0f) + 6.0f;
            drop->actor.speedXZ = Rand_ZeroFloat(3.0f) + 3.0f;
            drop->actor.world.rot.y = angle;
        }
        angle += (s16)(0x10000 / dropCount);
    }
}

// MM's strike knocks out a bomb pack, ten arrows and up to three quarters of Link's rupees as coins.
static bool Takkuri_KnockOutLoot(PlayState* play) {
    s32 drops[TAKKURI_DROP_SLOTS];

    for (s32 i = 0; i < TAKKURI_DROP_SLOTS; i++) {
        drops[i] = ITEM00_NONE;
    }
    s32 bombPacks = AMMO(ITEM_BOMB) >= 5 ? 1 : 0;
    s32 arrowPacks = AMMO(ITEM_BOW) >= 10 ? 1 : 0;

    if (bombPacks != 0) {
        drops[1] = ITEM00_BOMBS_A;
    }
    if (arrowPacks != 0) {
        drops[5] = ITEM00_ARROWS_MEDIUM;
    }
    s32 budget = (gSaveContext.rupees / 4) * 3;
    s32 slotsLeft = TAKKURI_DROP_SLOTS - bombPacks - arrowPacks;
    s32 purples = MIN(budget / 50, slotsLeft);

    for (s32 i = purples; i > 0; i--) {
        if (Rand_ZeroOne() < 0.5f) {
            purples--;
        }
    }
    slotsLeft -= purples;
    budget -= purples * 50;
    s32 reds = Takkuri_TakeRupees(&budget, &slotsLeft, 20);
    s32 blues = Takkuri_TakeRupees(&budget, &slotsLeft, 5);
    s32 greens = Takkuri_TakeRupees(&budget, &slotsLeft, 1);

    Takkuri_PlaceDrops(drops, purples, ITEM00_RUPEE_PURPLE);
    Takkuri_PlaceDrops(drops, reds, ITEM00_RUPEE_RED);
    Takkuri_PlaceDrops(drops, blues, ITEM00_RUPEE_BLUE);
    Takkuri_PlaceDrops(drops, greens, ITEM00_RUPEE_GREEN);
    s32 dropCount = bombPacks + arrowPacks + purples + reds + blues + greens;

    if (dropCount == 0) {
        return false;
    }
    Takkuri_ScatterDrops(play, drops, dropCount);
    AMMO(ITEM_BOMB) -= bombPacks * 5;
    AMMO(ITEM_BOW) -= arrowPacks * 10;
    Rupees_ChangeBy(-(purples * 50 + reds * 20 + blues * 5 + greens));
    return true;
}

static s32 Takkuri_GetLootType(EnItem00* item) {
    for (s32 i = 0; i < TAKKURI_LOOT_TYPES; i++) {
        if (item->actor.params == sLootTypes[i]) {
            return i;
        }
    }
    return -1;
}

// OoT's unk_15A is MM's unk152: the despawn timer, positive while the drop can still be picked up.
static bool Takkuri_IsLootable(EnItem00* item, PlayState* play) {
    Player* player = GET_PLAYER(play);

    return item->actor.id == ACTOR_EN_ITEM00 && item->unk_15A > 0 &&
           Actor_WorldDistXZToActor(&player->actor, &item->actor) > 10.0f && Takkuri_GetLootType(item) >= 0;
}

static void Takkuri_FindLoot(EnTakkuri* this, PlayState* play) {
    f32 nearest = 5000.0f;

    this->targetLoot = NULL;
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != NULL; actor = actor->next) {
        EnItem00* item = (EnItem00*)actor;

        if (!Takkuri_IsLootable(item, play)) {
            continue;
        }
        f32 distance = Actor_WorldDistXYZToActor(&this->actor, actor);

        if (distance < nearest) {
            this->targetLoot = item;
            nearest = distance;
        }
    }
}

static void Takkuri_SetupFlyIdle(EnTakkuri* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sTakkuriFlyAnim, -4.0f);
    this->timer = 60;
    this->collider.base.acFlags |= AC_ON;
    this->actionFunc = Takkuri_FlyIdle;
}

static void Takkuri_SetupSwoop(EnTakkuri* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sTakkuriAttackAnim, -10.0f);
    this->timer = 300;
    this->actor.speedXZ = 5.0f;
    this->actionFunc = Takkuri_Swoop;
}

static void Takkuri_WanderYaw(EnTakkuri* this, bool isWingbeat) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->targetYaw = this->actor.wallYaw;
    } else if (Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) > 300.0f) {
        this->targetYaw = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
    }
    bool isTurning = Math_SmoothStepToS(&this->actor.shape.rot.y, this->targetYaw, 5, 0x300, 0x10) != 0;

    if (isTurning || !isWingbeat || Rand_ZeroOne() >= 0.1f) {
        return;
    }
    s16 yawToHome = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos) - this->actor.shape.rot.y;

    this->targetYaw += yawToHome > 0 ? Rand_S16Offset(0x1000, 0x1000) : -Rand_S16Offset(0x1000, 0x1000);
    PlaySfx(&this->actor, MM_SE_EN_THIEFBIRD_VOICE, NA_SE_EN_KAICHO_CRY);
}

// Keeps within 75 units of its roost height, pulling up off the floor and water.
static void Takkuri_WanderPitch(EnTakkuri* this, bool isWingbeat) {
    if (this->actor.yDistToWater > -40.0f || (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        this->targetPitch = -0x1000;
    } else if (this->actor.world.pos.y < this->actor.home.pos.y - 75.0f) {
        this->targetPitch = -Rand_S16Offset(0x800, 0x800);
    } else if (this->actor.world.pos.y > this->actor.home.pos.y + 75.0f) {
        this->targetPitch = Rand_S16Offset(0x800, 0x800);
    }
    bool isTilting = Math_SmoothStepToS(&this->actor.shape.rot.x, this->targetPitch, 10, 0x100, 8) != 0;

    if (isTilting || !isWingbeat || Rand_ZeroOne() >= 0.1f) {
        return;
    }
    if (this->actor.world.pos.y > this->actor.home.pos.y) {
        this->targetPitch -= Rand_S16Offset(0x400, 0x400);
    } else {
        this->targetPitch += Rand_S16Offset(0x400, 0x400);
    }
    this->targetPitch = CLAMP(this->targetPitch, -0x1000, 0x1000);
}

static bool Takkuri_CanPursue(EnTakkuri* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    return !(player->stateFlags1 & PLAYER_STATE1_ON_HORSE) && this->actor.yDistToWater < -40.0f;
}

static void Takkuri_FlyIdle(EnTakkuri* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    bool isWingbeat = Animation_OnFrame(&this->skelAnime, 0.0f);

    this->actor.speedXZ = Rand_ZeroOne() * 1.5f + 3.0f;
    Takkuri_WanderYaw(this, isWingbeat);
    Takkuri_WanderPitch(this, isWingbeat);
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 0 && this->actor.xzDistToPlayer < 300.0f && Takkuri_CanPursue(this, play)) {
        Takkuri_SetupSwoop(this);
    }
}

// Circles in toward Link from his blind side rather than head-on.
static s16 Takkuri_GetSwoopYaw(EnTakkuri* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 offFacing = BINANG_ROT180(this->actor.yawTowardsPlayer - player->actor.shape.rot.y);

    if (offFacing > 0x4000) {
        return this->actor.yawTowardsPlayer + 0x3000;
    }
    if (offFacing < -0x4000) {
        return this->actor.yawTowardsPlayer - 0x3000;
    }
    return this->actor.yawTowardsPlayer;
}

static void Takkuri_SetupRetreat(EnTakkuri* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sTakkuriFlyWithItemAnim, -4.0f);
    Takkuri_SpawnFeathers(this, 15);
    this->targetPitch = -0x1000;
    this->targetYaw = BINANG_ROT180(this->actor.yawTowardsPlayer);
    this->timer = 40;
    this->actionFunc = Takkuri_Retreat;
}

static void Takkuri_SetupGatherLoot(EnTakkuri* this) {
    Animation_Change(&this->skelAnime, (AnimationHeader*)sTakkuriFlyAnim, 3.0f, 0.0f, 0.0f, ANIMMODE_LOOP_INTERP,
                     -4.0f);
    this->targetPitch = 0;
    this->collider.base.acFlags |= AC_ON;
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    this->actor.speedXZ = 4.0f;
    this->actionFunc = Takkuri_GatherLoot;
}

static void Takkuri_ResolveStrike(EnTakkuri* this, PlayState* play) {
    this->collider.base.atFlags &= ~AT_HIT;
    PlaySfx(&this->actor, MM_SE_EN_THIEFBIRD_VOICE, NA_SE_EN_KAICHO_CRY);
    if (this->collider.base.atFlags & AT_BOUNCED) {
        Takkuri_SetupRetreat(this);
    } else if (Takkuri_KnockOutLoot(play)) {
        Takkuri_FindLoot(this, play);
        Takkuri_SetupGatherLoot(this);
    } else {
        Takkuri_SetupFlyIdle(this);
    }
}

static void Takkuri_Swoop(EnTakkuri* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, 1.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_KAICHO_FLUTTER);
    }
    if (this->timer != 0) {
        this->timer--;
    }
    s16 pitch = CLAMP(Actor_WorldPitchTowardActor(&this->actor, &player->actor), -0x2800, 0x2800);

    Math_SmoothStepToS(&this->actor.shape.rot.x, pitch, 4, 0x800, 0x80);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.wallYaw, 6, 0x1000, 0x100);
    } else if (Actor_IsFacingPlayer(&this->actor, 0x3C00) || this->actor.xzDistToPlayer > 120.0f) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, Takkuri_GetSwoopYaw(this, play), 4, 0x1000, 0x100);
    }
    if (this->collider.base.atFlags & AT_HIT) {
        Takkuri_ResolveStrike(this, play);
        return;
    }
    bool shouldGiveUp =
        this->timer == 0 || !Takkuri_CanPursue(this, play) || (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);

    if (shouldGiveUp) {
        Takkuri_SetupFlyIdle(this);
    }
}

static void Takkuri_SetupDying(EnTakkuri* this) {
    this->actor.speedXZ = 0.0f;
    this->actor.velocity.y = 0.0f;
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sTakkuriDeathAnim);
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    this->actor.shape.rot.x = 0;
    this->timer = 40;
    PlaySfx(&this->actor, MM_SE_EN_THIEFBIRD_DEAD, NA_SE_EN_KAICHO_DEAD);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 40);
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    this->targetYaw = 0x1C00;
    this->actionFunc = Takkuri_Dying;
}

static void Takkuri_Dying(EnTakkuri* this, PlayState* play) {
    this->actor.colorFilterTimer = 40;
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer < 20) {
        Math_ScaledStepToS(&this->targetYaw, 0, 0x200);
    }
    this->actor.shape.rot.y += this->targetYaw;
    if (this->timer == 0) {
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        this->actor.gravity = -0.5f;
        this->actionFunc = Takkuri_Falling;
    }
}

static void Takkuri_DropLoot(EnTakkuri* this, PlayState* play) {
    Item_DropCollectible(play, &this->actor.world.pos, ITEM00_RUPEE_ORANGE);
    for (s32 i = 0; i < TAKKURI_LOOT_TYPES; i++) {
        for (s32 j = 0; j < this->lootCount[i]; j++) {
            Item_DropCollectible(play, &this->actor.world.pos, sLootTypes[i]);
        }
    }
}

// Flips over, corkscrews to the ground and bursts into red flames with everything it took.
static void Takkuri_Falling(EnTakkuri* this, PlayState* play) {
    if (Math_ScaledStepToS(&this->actor.shape.rot.x, -0x8000, 0x800)) {
        Math_ScaledStepToS(&this->targetYaw, 0x1C00, 0x200);
        this->actor.shape.rot.y += this->targetYaw;
    }
    bool hasLanded = (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || this->actor.floorHeight == BGCHECK_Y_MIN;

    if (!hasLanded) {
        return;
    }
    for (s32 i = 0; i < TAKKURI_BODYPART_MAX; i++) {
        EffectSsDeadDb_Spawn(play, &this->bodyPartsPos[i], &sZeroVec, &sZeroVec, 140, 0, 255, 255, 255, 255, 255, 0, 0,
                             1, 9, false);
    }
    sDeathSfxPos = this->actor.projectedPos;
    PlaySfxAt(&this->actor, &sDeathSfxPos, NA_SE_EN_EXTINCT, NA_SE_EN_EXTINCT);
    Takkuri_DropLoot(this, play);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

static void Takkuri_DropFlinchRupees(EnTakkuri* this, PlayState* play) {
    static const f32 sOdds[] = { 0.9f, 0.6f, 0.3f };

    for (s32 i = 0; i < ARRAY_COUNT(sOdds); i++) {
        if (Rand_ZeroOne() < sOdds[i]) {
            Item_DropCollectible(play, &this->actor.focus.pos, ITEM00_RUPEE_GREEN);
        }
    }
}

static void Takkuri_SetupFlinch(EnTakkuri* this, PlayState* play) {
    bool isStun = this->actor.colChkInfo.damageEffect == TAKKURI_DMGEFF_STUN;

    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sTakkuriFlyWithItemAnim, -4.0f);
    Takkuri_SpawnFeathers(this, 15);
    this->actor.speedXZ = this->actor.colChkInfo.damageEffect == TAKKURI_DMGEFF_FREEZE ? 0.0f : 4.0f;
    if (isStun) {
        Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 40);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
    } else {
        Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 40);
        PlaySfx(&this->actor, MM_SE_EN_THIEFBIRD_DAMAGE, NA_SE_EN_KAICHO_DAMAGE);
    }
    this->collider.base.acFlags &= ~AC_ON;
    this->targetPitch = -0x1000;
    this->targetYaw = BINANG_ROT180(this->actor.yawTowardsPlayer);
    this->timer = 40;
    Takkuri_DropFlinchRupees(this, play);
    this->actionFunc = Takkuri_Flinch;
}

// Reels away wobbling, then goes after whatever loot lies about.
static void Takkuri_Flinch(EnTakkuri* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 37 && this->actor.speedXZ == 0.0f) {
        this->actor.speedXZ = 4.0f;
    }
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->targetYaw = this->actor.wallYaw;
    } else {
        this->targetYaw = BINANG_ROT180(this->actor.yawTowardsPlayer);
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->targetYaw, 6, 0x1000, 0x100);
    Math_SmoothStepToS(&this->actor.shape.rot.x, this->targetPitch, 5, 0x100, 0x10);
    f32 wobble = this->timer * 153.6f + 0x800;

    this->actor.shape.rot.z = Math_SinS(this->timer * 0x1999) * wobble;
    if (this->timer != 0) {
        return;
    }
    if (this->targetLoot != NULL) {
        Takkuri_SetupGatherLoot(this);
    } else {
        Takkuri_SetupFlyIdle(this);
    }
}

static void Takkuri_Retreat(EnTakkuri* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
    }
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->targetYaw = this->actor.wallYaw;
    } else {
        this->targetYaw = BINANG_ROT180(this->actor.yawTowardsPlayer);
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->targetYaw, 6, 0x1000, 0x100);
    Math_SmoothStepToS(&this->actor.shape.rot.x, this->targetPitch, 5, 0x100, 0x10);
    if (this->timer == 0) {
        Takkuri_SetupFlyIdle(this);
    }
}

static bool Takkuri_IsLootGone(EnTakkuri* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    EnItem00* loot = this->targetLoot;

    return loot->actor.update == NULL || loot->unk_15A == 0 ||
           Actor_WorldDistXZToActor(&player->actor, &loot->actor) <= 10.0f;
}

// Gulps the item down: its despawn timer hits zero and the bird keeps count for its death.
static void Takkuri_SwallowLoot(EnTakkuri* this) {
    s32 type = Takkuri_GetLootType(this->targetLoot);

    this->targetLoot->unk_15A = 0;
    if (type >= 0) {
        this->lootCount[type]++;
    }
    this->targetLoot = NULL;
    this->neckRoll = 0x100;
    this->targetPitch = -0x3800;
}

static void Takkuri_ChaseLoot(EnTakkuri* this) {
    Actor* loot = &this->targetLoot->actor;
    Vec3f* beak = &this->bodyPartsPos[TAKKURI_BODYPART_BEAK];

    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.wallYaw, 3, 0x2000, 0x100);
    } else {
        Math_SmoothStepToS(&this->actor.shape.rot.y, Actor_WorldYawTowardActor(&this->actor, loot), 3, 0x2000, 0x100);
    }
    s16 pitch = CLAMP(Math_Vec3f_Pitch(beak, &loot->world.pos), -0x3000, 0x3000);

    Math_SmoothStepToS(&this->actor.shape.rot.x, pitch, 4, 0x800, 0x80);
    f32 distance = Math_Vec3f_DistXYZ(&loot->world.pos, beak);

    this->actor.speedXZ = MIN(0.02f * distance + 2.0f, 4.0f);
    if (loot->speedXZ <= 0.0f && distance < 40.0f) {
        Takkuri_SwallowLoot(this);
    }
}

static void Takkuri_GatherLoot(EnTakkuri* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->targetLoot != NULL && Takkuri_IsLootGone(this, play)) {
        this->targetLoot = NULL;
    }
    if (this->targetPitch == -0x3800) {
        if (Math_ScaledStepToS(&this->neckRoll, this->targetPitch, 0xE00)) {
            this->targetPitch = 0;
        }
    } else {
        Math_ScaledStepToS(&this->neckRoll, 0, 0x200);
    }
    if (this->targetLoot == NULL) {
        Takkuri_FindLoot(this, play);
    }
    if (this->targetLoot != NULL) {
        Takkuri_ChaseLoot(this);
        return;
    }
    this->actor.speedXZ = 4.0f;
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.wallYaw, 6, 0x1000, 0x100);
    } else {
        Math_SmoothStepToS(&this->actor.shape.rot.y, BINANG_ROT180(this->actor.yawTowardsPlayer), 6, 0x1000, 0x100);
    }
    Math_SmoothStepToS(&this->actor.shape.rot.x, -0x800, 4, 0x800, 0x80);
    if (this->neckRoll == 0) {
        this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        Takkuri_SetupFlyIdle(this);
    }
}

static void Takkuri_ShatterIce(EnTakkuri* this, PlayState* play) {
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < TAKKURI_BODYPART_MAX; i++) {
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &this->bodyPartsPos[i], 150, 150, 150, 250, 235, 245, 255,
                                       0.2f);
    }
}

static void Takkuri_UpdateDamage(EnTakkuri* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    this->collider.base.atFlags &= ~AT_HIT;
    Actor_SetDropFlag(&this->actor, &this->collider.elements[0].info, false);
    this->neckRoll = 0;
    if (this->actor.colChkInfo.damageEffect == TAKKURI_DMGEFF_FREEZE) {
        Takkuri_ShatterIce(this, play);
    }
    if (Actor_ApplyDamage(&this->actor) == 0) {
        this->actor.colChkInfo.health = 0;
        Enemy_StartFinishingBlow(play, &this->actor);
        Takkuri_SpawnFeathers(this, TAKKURI_FEATHER_COUNT);
        Takkuri_SetupDying(this);
    } else {
        Takkuri_SetupFlinch(this, play);
    }
}

// MM's Actor_MoveWithoutGravity: flies along its pitch as well as its yaw.
static void Takkuri_Fly(EnTakkuri* this) {
    f32 speedXZ = Math_CosS(this->actor.world.rot.x) * this->actor.speedXZ;

    this->actor.velocity.x = Math_SinS(this->actor.world.rot.y) * speedXZ;
    this->actor.velocity.y = Math_SinS(this->actor.world.rot.x) * this->actor.speedXZ;
    this->actor.velocity.z = Math_CosS(this->actor.world.rot.y) * speedXZ;
    Actor_UpdatePos(&this->actor);
}

static void Takkuri_PlayWingbeat(EnTakkuri* this) {
    bool isFlying = this->skelAnime.animation == (void*)sTakkuriFlyAnim && Animation_OnFrame(&this->skelAnime, 13.0f);
    bool isFleeing =
        this->skelAnime.animation == (void*)sTakkuriFlyWithItemAnim && Animation_OnFrame(&this->skelAnime, 1.0f);

    if (isFlying || isFleeing) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_KAICHO_FLUTTER);
    }
}

// The limbs place the spheres at draw time; until then (or while culled) they sit on the body so blows still connect.
static void Takkuri_CenterSpheresOnBody(EnTakkuri* this) {
    for (s32 i = 0; i < ARRAY_COUNT(this->colliderElements); i++) {
        ColliderJntSphElementDim* dim = &this->collider.elements[i].dim;

        dim->worldSphere.radius = dim->modelSphere.radius;
        dim->worldSphere.center.x = this->actor.world.pos.x;
        dim->worldSphere.center.y = this->actor.world.pos.y;
        dim->worldSphere.center.z = this->actor.world.pos.z;
    }
}

static void EnTakkuri_Init(Actor* thisx, PlayState* play) {
    EnTakkuri* this = (EnTakkuri*)thisx;

    this->actor.uncullZoneForward = 3000.0f;
    this->actor.uncullZoneScale = 1000.0f;
    this->actor.targetArrowOffset = 500.0f;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sTakkuriSkel, (AnimationHeader*)sTakkuriFlyAnim,
                       this->jointTable, this->morphTable, TAKKURI_LIMB_MAX);
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sColliderInit, this->colliderElements);
    Takkuri_CenterSpheresOnBody(this);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    ActorShape_Init(&this->actor.shape, 1500.0f, ActorShadow_DrawCircle, 35.0f);
    Actor_SetScale(&this->actor, 0.01f);
    Takkuri_SetupFlyIdle(this);
}

static void EnTakkuri_Destroy(Actor* thisx, PlayState* play) {
    EnTakkuri* this = (EnTakkuri*)thisx;

    Collider_DestroyJntSph(play, &this->collider);
}

static void EnTakkuri_Update(Actor* thisx, PlayState* play) {
    EnTakkuri* this = (EnTakkuri*)thisx;

    Takkuri_UpdateDamage(this, play);
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.world.rot.x = -this->actor.shape.rot.x;
    if (this->actor.colChkInfo.health != 0) {
        Takkuri_Fly(this);
    } else {
        Actor_MoveXZGravity(&this->actor);
    }
    Actor_UpdateBgCheckInfo(play, &this->actor, 25.0f, 25.0f, 50.0f, BGCHECK_UPDATE_WALL_CEILING_FLOOR);
    if (this->actionFunc == Takkuri_Swoop) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (!this->actor.isDrawn) {
        Takkuri_CenterSpheresOnBody(this);
    }
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    Takkuri_UpdateFeathers(this);
    Takkuri_PlayWingbeat(this);
}

static s32 Takkuri_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnTakkuri* this = (EnTakkuri*)thisx;

    if (limbIndex == TAKKURI_LIMB_RIGHT_EAR || limbIndex == TAKKURI_LIMB_LEFT_EAR) {
        this->earDL = *dList;
        *dList = NULL;
    } else if (limbIndex == TAKKURI_LIMB_STOLEN_ITEM) {
        *dList = NULL;
    } else if (limbIndex == TAKKURI_LIMB_NECK) {
        rot->z += this->neckRoll;
    }
    return false;
}

// The ears always face the camera.
static void Takkuri_DrawEar(EnTakkuri* this, PlayState* play) {
    if (this->earDL == NULL) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, this->earDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Takkuri_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnTakkuri* this = (EnTakkuri*)thisx;
    s8 bodyPart = sLimbToBodyPart[limbIndex];

    Collider_UpdateSpheres(limbIndex, &this->collider);
    if (limbIndex == TAKKURI_LIMB_RIGHT_EAR || limbIndex == TAKKURI_LIMB_LEFT_EAR) {
        Takkuri_DrawEar(this, play);
    }
    if (bodyPart < 0) {
        return;
    }
    if (bodyPart == TAKKURI_BODYPART_BEAK) {
        Matrix_MultVec3f(&sBeakTipOffset, &this->bodyPartsPos[bodyPart]);
        return;
    }
    Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[bodyPart]);
    if (bodyPart == 3 || bodyPart == 5) {
        Matrix_MultVec3f(&sWingTipOffset, &this->bodyPartsPos[bodyPart + 1]);
    }
}

static void Takkuri_DrawFeathers(EnTakkuri* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sTakkuriFeatherMaterialDL);
    for (s32 i = 0; i < TAKKURI_FEATHER_COUNT; i++) {
        TakkuriFeather* feather = &this->feathers[i];

        if (feather->timer == 0) {
            continue;
        }
        Matrix_Translate(feather->pos.x, feather->pos.y, feather->pos.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_RotateY(BINANG_TO_RAD(feather->yaw), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(feather->roll), MTXMODE_APPLY);
        Matrix_Translate(0.0f, -10.0f, 0.0f, MTXMODE_APPLY);
        Matrix_Scale(feather->scale, feather->scale, 1.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sTakkuriFeatherModelDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnTakkuri_Draw(Actor* thisx, PlayState* play) {
    EnTakkuri* this = (EnTakkuri*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          Takkuri_OverrideLimbDraw, Takkuri_PostLimbDraw, this);
    Takkuri_DrawFeathers(this, play);
    this->actor.focus.pos.x = this->collider.elements[1].dim.worldSphere.center.x;
    this->actor.focus.pos.y = this->collider.elements[1].dim.worldSphere.center.y;
    this->actor.focus.pos.z = this->collider.elements[1].dim.worldSphere.center.z;
}

// MM's CanKillEnemy(ACTOR_EN_THIEFBIRD): a projectile, explosives, a sword or Goron/Zora fists (the hammer here).
static bool Takkuri_CanKillInLogic(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicHasCapability("HasExplosives") ||
           sApi->LogicCanUseItem(RG_FAIRY_BOW) || sApi->LogicCanUseItem(RG_FAIRY_SLINGSHOT) ||
           sApi->LogicCanUseItem(RG_HOOKSHOT) || sApi->LogicCanUseItem(RG_BOOMERANG) ||
           sApi->LogicCanUseItem(RG_MEGATON_HAMMER);
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
    static Vec3s sSoulJointTable[TAKKURI_LIMB_MAX];
    static Vec3s sSoulMorphTable[TAKKURI_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sTakkuriSkel, (AnimationHeader*)sTakkuriFlyAnim,
                           sSoulJointTable, sSoulMorphTable, TAKKURI_LIMB_MAX);
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
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sSoulEmptyDL);
    SkelAnime_DrawFlexOpa(play, sSoulSkelAnime.skeleton, sSoulSkelAnime.jointTable, sSoulSkelAnime.dListCount, NULL,
                          NULL, NULL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterSoul(void) {
    static const SOHCustomItemRandomizer sSoulRandomizer = {
        .structSize = sizeof(SOHCustomItemRandomizer),
        .flags = SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT,
        .type = SOH_CUSTOM_ITEM_TYPE_ITEM,
    };
    SOHCustomItemDefinition soul = Z64Items_Define(TAKKURI_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Takkuri";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Takkuri%w!&Takkuri can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " TAKKURI_SOUL_ITEM_KEY);
    }
}

static void RegisterTakkuri(void) {
    SOHActorDefinition takkuri = { 0 };

    takkuri.structSize = sizeof(takkuri);
    takkuri.key = TAKKURI_KEY;
    takkuri.description = "Takkuri";
    takkuri.category = ACTORCAT_ENEMY;
    takkuri.actorFlags =
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR | ACTOR_FLAG_IGNORE_QUAKE;
    takkuri.objectId = OBJECT_GAMEPLAY_KEEP;
    takkuri.instanceSize = sizeof(EnTakkuri);
    takkuri.init = EnTakkuri_Init;
    takkuri.destroy = EnTakkuri_Destroy;
    takkuri.update = EnTakkuri_Update;
    takkuri.draw = EnTakkuri_Draw;
    takkuri.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NOT_IN_WATER | SOH_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS;
    takkuri.enemySpawnHeight = 120;
    takkuri.naviHint = "Takkuri&%cThis thieving bird knocks the rupees out of your pockets and swoops on them. Beat "
                       "it to win them back!%w";
    takkuri.soulKey = TAKKURI_KEY;
    takkuri.logicKill = Takkuri_CanKillInLogic;
    if (sApi->RegisterActor(&takkuri) < 0) {
        sApi->Log("Could not register " TAKKURI_KEY);
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
    RegisterTakkuri();
}
