/**
 * Elemental rods: the Fire, Ice and Light rods from A Link Between Worlds.
 *
 * One core, three configs. Each rod is a held item that swings with vanilla sword mechanics (its
 * `meleeWeapon` index is what makes the engine's attack handlers accept it) and turns every swing into
 * magic: slash throws three bolts, stab throws one far, the jump slash lays a line of element on the
 * ground and the spin attack opens an expanding cylinder. Holding the item button charges that spin.
 * C-Up aims in first person. Casting without magic backfires on Link.
 */

#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Bg_Ice_Shelter/z_bg_ice_shelter.h"

#define FIRE_ROD_KEY "nei.rod_fire"
#define ICE_ROD_KEY "nei.rod_ice"
#define LIGHT_ROD_KEY "nei.rod_light"

#define ROD_MAX_SETS 5
#define ROD_SET_BOLTS 3
#define ROD_TRAIL_POINTS 6
#define ROD_LINE_COUNT 6
#define ROD_LINE_SPACING 40.0f
#define ROD_LINE_FRAMES 30
#define ROD_BUTTON_COUNT 8
#define ROD_SPIN_HEIGHT 80
#define ROD_SPIN_START_RADIUS 50.0f
#define ROD_SPIN_GROW_SMALL 15.0f
#define ROD_SPIN_GROW_BIG 30.0f
#define ROD_BOLT_TARGET_SCALE 2.0f
#define ROD_BOLT_MIN_FRAMES 10
#define ROD_BOLT_SPARK_SCALE 0.4f
#define ROD_BOLT_COLLIDER_SCALE 0.6f
#define ROD_FREEZE_FRAMES 60
#define ROD_STUN_FRAMES 60
#define ROD_BACKFIRE_INVINCIBILITY 20
#define ROD_AIM_RANGE 0.0f
#define ROD_HELD_SCALE 0.05f
#define ROD_HELD_SIDE_OFFSET 0.5f
#define ROD_GIVE_SCALE 0.2f
#define ROD_RED_ICE_MARGIN 15.0f
#define ROD_ICE_SHARD_SCALE 0.01f
#define ROD_FLAME_BOLT_SCALE 0.0015f
#define ROD_FLAME_BOLT_MIN 0.001f
#define ROD_ORB_BOLT_SCALE 5.5f
#define ROD_TRAIL_FRAMES 8

/** Link's hit responses, by the value func_80837C0C takes: frozen solid and electrocuted. */
#define ROD_HIT_RESPONSE_FROZEN 3
#define ROD_HIT_RESPONSE_SHOCKED 4

typedef struct {
    Color_RGBA8 prim;
    Color_RGBA8 env;
} RodColors;

typedef struct {
    Vec3f pos[ROD_SET_BOLTS];
    Vec3f vel[ROD_SET_BOLTS];
    Vec3f trail[ROD_TRAIL_POINTS];
    ColliderCylinder colliders[ROD_SET_BOLTS];
    s16 timer;
    s16 rotZ;
    f32 scale;
    f32 targetScale;
    u8 count;
    u8 active;
    u8 collidersReady;
} RodBoltSet;

typedef struct {
    RodBoltSet sets[ROD_MAX_SETS];
    ColliderCylinder line[ROD_LINE_COUNT];
    Vec3f linePos[ROD_LINE_COUNT];
    ColliderCylinder spin;
    s16 lineTimer;
    s16 chargeTimer;
    s16 holdFrames;
    f32 chargeLevel;
    f32 spinRadius;
    f32 spinMaxRadius;
    s32 blureIdx;
    u8 lineReady;
    u8 lineActive;
    u8 spinReady;
    u8 spinActive;
    u8 spinIsBig;
    u8 charging;
    u8 chargeReady;
    u8 buttonHeld;
    u8 lastSwing;
    u8 jumpSpawned;
    u8 aiming;
} RodState;

typedef struct RodConfig RodConfig;

struct RodConfig {
    const char* key;
    const char* iconPath;
    const char* namePath;
    const char* opaqueDL;
    const char* xluDL;
    const RodColors* colors;
    f32 boltSpeed;
    s16 boltLifetime;
    s16 boltMaxFrames;
    f32 slashRange;
    s16 slashSpread;
    s16 boltSpin;
    f32 boltGrowth;
    s16 magicBolt;
    s16 magicLine;
    s16 magicSpinSmall;
    s16 magicSpinBig;
    u8 backfireBolt;
    u8 backfireLine;
    u8 backfireSpin;
    f32 chargeRate;
    f32 chargeMin;
    f32 chargeBig;
    s16 chargeHoldFrames;
    f32 spinSmallRadius;
    f32 spinBigRadius;
    u16 sfxSwing;
    u16 sfxCharge;
    u16 sfxLoop;
    u16 sfxIgnite;
    u16 sfxHit;
    u16 sfxCast;
    u16 sfxLine;
    /** 0 = the ignite sound is a looped sfx kept alive every frame; otherwise its period in frames. */
    u8 ignitePeriod;
    f32 lineBaseScale;
    f32 lineScaleGrow;
    f32 lineRadiusScale;
    f32 lineRadiusBase;
    f32 lineHeightScale;
    f32 lineHeightBase;
    ColliderCylinderInit* boltCollider;
    ColliderCylinderInit* spinCollider;
    ColliderCylinderInit* lineCollider;
    void (*onBoltTrail)(PlayState* play, Vec3f* pos, f32 scale);
    void (*onHit)(PlayState* play, Player* player, Actor* hitActor, Vec3f* pos);
    void (*onLineSpawn)(PlayState* play, Player* player, Vec3f* pos, f32 scale);
    void (*onBoltsAlive)(PlayState* play, const RodConfig* cfg);
    f32 boltDrawScale;
    s16 boltScrollStep;
    void (*onBoltsDrawBegin)(PlayState* play, const RodConfig* cfg);
    void (*onBoltDraw)(PlayState* play, const RodConfig* cfg, Vec3f* pos, f32 scale, s32 index);
    void (*onBackfire)(PlayState* play, Player* player);
    const RodColors* (*chargeColors)(const RodState* state, const RodConfig* cfg);
    RodState* state;
};

static const ALIGN_ASSET(2) char sFireIconTex[] = "__OTR__textures/icon_item_custom/gItemIconFireRodTex";
static const ALIGN_ASSET(2) char sFireNameTex[] = "__OTR__textures/item_name_custom/gFireRodNameTex";
static const ALIGN_ASSET(2) char sIceIconTex[] = "__OTR__textures/icon_item_custom/gItemIconIceRodTex";
static const ALIGN_ASSET(2) char sIceNameTex[] = "__OTR__textures/item_name_custom/gIceRodNameTex";
static const ALIGN_ASSET(2) char sLightIconTex[] = "__OTR__textures/icon_item_custom/gItemIconLightRodTex";
static const ALIGN_ASSET(2) char sLightNameTex[] = "__OTR__textures/item_name_custom/gLightRodNameTex";

static const ALIGN_ASSET(2) char sFireRodDL[] = "__OTR__objects/object_nei_fire_rod/Cylinder_001_opaque_dl";
static const ALIGN_ASSET(2) char sIceRodDL[] = "__OTR__objects/object_nei_ice_rod/ice_rod_opaque_dl";
static const ALIGN_ASSET(2) char sIceRodXluDL[] = "__OTR__objects/object_nei_ice_rod/ice_rod_transparent_dl";
static const ALIGN_ASSET(2) char sLightRodDL[] = "__OTR__objects/object_nei_light_rod/Cylinder_002_opaque_dl";
static const ALIGN_ASSET(2) char sLightRodXluDL[] = "__OTR__objects/object_nei_light_rod/Cylinder_002_transparent_dl";
static const ALIGN_ASSET(2) char sBoltDL[] = "__OTR__objects/gameplay_keep/gEffFire1DL";
static const ALIGN_ASSET(2) char sOrbDL[] = "__OTR__objects/object_fhg/gPhantomEnergyBallDL";
static const ALIGN_ASSET(2) char sRingDL[] = "__OTR__objects/gameplay_keep/gEffFireCircleDL";

static const u16 sItemButtons[ROD_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                    BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayDrawEnd", "OnActorDrawEnd", "OnSceneInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static const RodConfig* sRodInHand;

void func_80837948(PlayState* play, Player* player, s32 meleeWeaponAnim);
void func_80837C0C(PlayState* play, Player* player, s32 hitResponse, f32 damageSpeed, f32 damageRot, s16 damageRotType,
                   s32 invincibility);
void func_8002F698(PlayState* play, Actor* actor, f32 speed, s16 rot, f32 yVelocity, u32 type, u32 damage);
void func_8083821C(Player* player);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
s32 Player_IsZTargeting(Player* player);
s32 Player_UpperAction_Sword(Player* player, PlayState* play);

// ---- element effects ------------------------------------------------------------------------------------

static const RodColors sFireColors = { { 255, 255, 0, 255 }, { 255, 80, 0, 255 } };
static const RodColors sIceColors = { { 200, 255, 255, 255 }, { 0, 100, 255, 255 } };
static const RodColors sLightColors = { { 255, 255, 200, 255 }, { 255, 255, 0, 255 } };

/** What NEI paints at full charge: the pale rod body washes out, so the aura goes pure gold instead. */
static const RodColors sLightMaxChargeColors = { { 255, 255, 0, 255 }, { 255, 255, 100, 255 } };

static void FreezeActor(PlayState* play, Actor* hitActor) {
    hitActor->freezeTimer = ROD_FREEZE_FRAMES;
    Actor_SetColorFilter(hitActor, 0x4000, 255, 0x2000, ROD_FREEZE_FRAMES);
    EffectSsEnIce_SpawnFlyingVec3f(play, hitActor, &hitActor->world.pos, 150, 150, 150, 250, 235, 245, 255, 1.0f);
    Audio_PlayActorSound2(hitActor, NA_SE_PL_FREEZE_S);
}

static u8 IsUndeadActor(Actor* actor) {
    switch (actor->id) {
        case ACTOR_EN_RD:
        case ACTOR_EN_POH:
        case ACTOR_EN_PO_SISTERS:
        case ACTOR_EN_PO_RELAY:
        case ACTOR_EN_PO_FIELD:
        case ACTOR_EN_PO_DESERT:
        case ACTOR_EN_SKB:
        case ACTOR_EN_WALLMAS:
        case ACTOR_EN_FLOORMAS:
        case ACTOR_EN_DH:
        case ACTOR_EN_DHA:
            return true;
        default:
            return false;
    }
}

static void SpawnSparkles(PlayState* play, Vec3f* pos, const RodColors* colors, f32 spread, s32 count, s16 scale,
                          s32 life) {
    Color_RGBA8 prim = colors->prim;
    Color_RGBA8 env = colors->env;
    Vec3f vel = { 0.0f, 0.0f, 0.0f };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };

    for (s32 i = 0; i < count; i++) {
        Vec3f sparkPos = { pos->x + Rand_CenteredFloat(spread), pos->y + Rand_CenteredFloat(spread),
                           pos->z + Rand_CenteredFloat(spread) };

        EffectSsKiraKira_SpawnDispersed(play, &sparkPos, &vel, &accel, &prim, &env, scale, life);
    }
}

static void TrailFire(PlayState* play, Vec3f* pos, f32 scale) {
    SpawnSparkles(play, pos, &sFireColors, scale * 20.0f, 10, 1000, 10);
}

static void TrailIce(PlayState* play, Vec3f* pos, f32 scale) {
    Color_RGBA8 prim = sIceColors.prim;
    Color_RGBA8 env = sIceColors.env;
    Vec3f accel = { 0.0f, -0.5f, 0.0f };

    for (s32 i = 0; i < 6; i++) {
        Vec3f icePos = { pos->x + Rand_CenteredFloat(scale * 15.0f), pos->y + Rand_CenteredFloat(scale * 15.0f),
                         pos->z + Rand_CenteredFloat(scale * 15.0f) };
        Vec3f vel = { Rand_CenteredFloat(3.0f), Rand_ZeroOne() * 2.0f, Rand_CenteredFloat(3.0f) };

        EffectSsEnIce_Spawn(play, &icePos, scale * 0.3f, &vel, &accel, &prim, &env, 15);
    }
}

static void TrailLight(PlayState* play, Vec3f* pos, f32 scale) {
    s32 count = CLAMP((s32)(scale * 3.0f), 2, 8);

    SpawnSparkles(play, pos, &sLightColors, scale * 10.0f, count, 1200, 12);
}

static void HitFire(PlayState* play, Player* player, Actor* hitActor, Vec3f* pos) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    EffectSsBomb2_SpawnLayered(play, pos, &zero, &zero, 10, 5);
}

static void HitIce(PlayState* play, Player* player, Actor* hitActor, Vec3f* pos) {
    EffectSsIcePiece_SpawnBurst(play, pos, 1.0f);
    if (hitActor != NULL) {
        FreezeActor(play, hitActor);
    }
}

static void HitLight(PlayState* play, Player* player, Actor* hitActor, Vec3f* pos) {
    SpawnSparkles(play, pos, &sLightColors, 30.0f, 20, 1200, 12);
    if (hitActor == NULL) {
        return;
    }
    if (IsUndeadActor(hitActor)) {
        // The white filter is the Sun's Song paralysis, which is what light magic does to the undead.
        Actor_SetColorFilter(hitActor, -0x8000, 0xC8, 0, ROD_STUN_FRAMES);
        SpawnSparkles(play, &hitActor->world.pos, &sLightColors, 50.0f, 25, 1200, 12);
    } else {
        Actor_SetColorFilter(hitActor, 0, 0xFF, 0, ROD_STUN_FRAMES);
    }
    hitActor->freezeTimer = ROD_STUN_FRAMES;
    Audio_PlayActorSound2(hitActor, NA_SE_EN_LIGHT_ARROW_HIT);
}

static void LineFire(PlayState* play, Player* player, Vec3f* pos, f32 scale) {
    EffectSsEnFire_SpawnVec3f(play, &player->actor, pos, (s16)scale, 0, 0, -1);
}

// The shard burst takes an actor-sized scale, not the thousands EffectSsEnFire wants, so the shared line
// scale is brought down to it. Without this the trail draws a hundred times too big.
static void LineIce(PlayState* play, Player* player, Vec3f* pos, f32 scale) {
    EffectSsIcePiece_SpawnBurst(play, pos, scale * ROD_ICE_SHARD_SCALE);
}

static void LineLight(PlayState* play, Player* player, Vec3f* pos, f32 scale) {
    Color_RGBA8 prim = sLightColors.prim;
    Color_RGBA8 env = sLightColors.env;
    Vec3f accel = { 0.0f, -0.05f, 0.0f };
    s32 count = CLAMP((s32)(scale * 0.05f), 3, 15);

    for (s32 i = 0; i < count; i++) {
        Vec3f sparkPos = { pos->x + Rand_CenteredFloat(20.0f), pos->y + Rand_CenteredFloat(15.0f),
                           pos->z + Rand_CenteredFloat(20.0f) };
        Vec3f vel = { Rand_CenteredFloat(2.0f), 1.0f, Rand_CenteredFloat(2.0f) };

        EffectSsKiraKira_SpawnDispersed(play, &sparkPos, &vel, &accel, &prim, &env, 1500, 15);
    }
}

static void BackfireFire(PlayState* play, Player* player) {
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_FALL_L);
    func_8083821C(player);
    func_8002F698(play, &player->actor, 4.0f, player->actor.shape.rot.y + 0x8000, 6.0f, 2, 0);
}

static void BackfireIce(PlayState* play, Player* player) {
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_FREEZE);
    func_80837C0C(play, player, ROD_HIT_RESPONSE_FROZEN, 0.0f, 0.0f, 0, ROD_BACKFIRE_INVINCIBILITY);
}

static void BackfireLight(PlayState* play, Player* player) {
    Vec3f shockPos = player->actor.world.pos;

    Player_PlayVoiceSfx(player, NA_SE_VO_LI_DAMAGE_S);
    func_80837C0C(play, player, ROD_HIT_RESPONSE_SHOCKED, 0.0f, 0.0f, 0, ROD_BACKFIRE_INVINCIBILITY);
    shockPos.y += 50.0f;
    SpawnSparkles(play, &shockPos, &sLightColors, 40.0f, 15, 1200, 12);
}

static const RodColors* ChargeColorsLight(const RodState* state, const RodConfig* cfg) {
    return state->chargeLevel >= cfg->chargeBig ? &sLightMaxChargeColors : cfg->colors;
}

// ---- colliders ------------------------------------------------------------------------------------------

static ColliderCylinderInit sFireBoltCollider = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_ARROW_FIRE, 0x01, 4 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 10, 20, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sFireSpinCollider = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_MAGIC_FIRE | DMG_SLASH, 0x01, 8 },
      { 0, 0, 0 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { 50, ROD_SPIN_HEIGHT, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sFireLineCollider = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_ARROW_FIRE, 0x01, 4 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 5, 10, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sIceBoltCollider = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_ARROW_ICE, 0x01, 4 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 10, 20, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sIceSpinCollider = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_MAGIC_ICE | DMG_SLASH, 0x01, 8 },
      { 0, 0, 0 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { 50, ROD_SPIN_HEIGHT, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sIceLineCollider = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_ARROW_ICE, 0x01, 4 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 5, 10, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sLightBoltCollider = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_ARROW_LIGHT, 0x01, 4 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 10, 20, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sLightSpinCollider = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_MAGIC_LIGHT | DMG_SLASH, 0x01, 8 },
      { 0, 0, 0 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { 50, ROD_SPIN_HEIGHT, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sLightLineCollider = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_ARROW_LIGHT, 0x01, 4 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 5, 10, 0, { 0, 0, 0 } },
};

// ---- rod table ------------------------------------------------------------------------------------------

static void MeltRedIce(PlayState* play, const RodConfig* cfg);
static void BeginFlameBolts(PlayState* play, const RodConfig* cfg);
static void DrawFlameBolt(PlayState* play, const RodConfig* cfg, Vec3f* pos, f32 scale, s32 index);
static void BeginOrbBolts(PlayState* play, const RodConfig* cfg);
static void DrawOrbBolt(PlayState* play, const RodConfig* cfg, Vec3f* pos, f32 scale, s32 index);

static RodState sFireState;
static RodState sIceState;
static RodState sLightState;

static const RodConfig sFireRod = {
    .key = FIRE_ROD_KEY,
    .iconPath = sFireIconTex,
    .namePath = sFireNameTex,
    .opaqueDL = sFireRodDL,
    .xluDL = NULL,
    .colors = &sFireColors,
    .boltSpeed = 15.0f,
    .boltLifetime = 30,
    .boltMaxFrames = 30,
    .slashRange = 200.0f,
    .slashSpread = 30,
    .boltSpin = 5000,
    .boltGrowth = 0.4f,
    .magicBolt = 3,
    .magicLine = 6,
    .magicSpinSmall = 6,
    .magicSpinBig = 12,
    .backfireBolt = 10,
    .backfireLine = 20,
    .backfireSpin = 50,
    .chargeRate = 0.02f,
    .chargeMin = 0.1f,
    .chargeBig = 0.85f,
    .chargeHoldFrames = 10,
    .spinSmallRadius = 100.0f,
    .spinBigRadius = 500.0f,
    .sfxSwing = NA_SE_IT_SWORD_SWING,
    .sfxCharge = NA_SE_PL_SWORD_CHARGE,
    .sfxLoop = NA_SE_EN_ANUBIS_FIRE,
    .sfxIgnite = NA_SE_IT_BOMB_IGNIT,
    .sfxHit = NA_SE_EN_ANUBIS_FIREBOMB,
    .sfxCast = NA_SE_PL_MAGIC_FIRE,
    .sfxLine = NA_SE_PL_MAGIC_FIRE,
    .ignitePeriod = 0,
    .lineBaseScale = 50.0f,
    .lineScaleGrow = 30.0f,
    .lineRadiusScale = 0.12f,
    .lineRadiusBase = 3.0f,
    .lineHeightScale = 0.2f,
    .lineHeightBase = 5.0f,
    .boltCollider = &sFireBoltCollider,
    .spinCollider = &sFireSpinCollider,
    .lineCollider = &sFireLineCollider,
    .onBoltTrail = TrailFire,
    .onHit = HitFire,
    .onLineSpawn = LineFire,
    .onBoltsAlive = NULL,
    .boltDrawScale = 0.0015f,
    .boltScrollStep = 20,
    .onBoltsDrawBegin = BeginFlameBolts,
    .onBoltDraw = DrawFlameBolt,
    .onBackfire = BackfireFire,
    .chargeColors = NULL,
    .state = &sFireState,
};

static const RodConfig sIceRod = {
    .key = ICE_ROD_KEY,
    .iconPath = sIceIconTex,
    .namePath = sIceNameTex,
    .opaqueDL = sIceRodDL,
    .xluDL = sIceRodXluDL,
    .colors = &sIceColors,
    .boltSpeed = 15.0f,
    .boltLifetime = 30,
    .boltMaxFrames = 30,
    .slashRange = 200.0f,
    .slashSpread = 30,
    .boltSpin = 5000,
    .boltGrowth = 0.4f,
    .magicBolt = 3,
    .magicLine = 6,
    .magicSpinSmall = 6,
    .magicSpinBig = 12,
    .backfireBolt = 10,
    .backfireLine = 20,
    .backfireSpin = 50,
    .chargeRate = 0.02f,
    .chargeMin = 0.1f,
    .chargeBig = 0.85f,
    .chargeHoldFrames = 10,
    .spinSmallRadius = 100.0f,
    .spinBigRadius = 500.0f,
    .sfxSwing = NA_SE_IT_SWORD_SWING,
    .sfxCharge = NA_SE_PL_SWORD_CHARGE,
    .sfxLoop = NA_SE_EV_ICE_FREEZE,
    .sfxIgnite = NA_SE_EV_ICE_MELT,
    .sfxHit = NA_SE_EV_ICE_BROKEN,
    .sfxCast = NA_SE_PL_FREEZE_S,
    .sfxLine = NA_SE_EV_ICE_FREEZE,
    .ignitePeriod = 8,
    .lineBaseScale = 0.15f,
    .lineScaleGrow = 0.05f,
    .lineRadiusScale = 80.0f,
    .lineRadiusBase = 20.0f,
    .lineHeightScale = 100.0f,
    .lineHeightBase = 40.0f,
    .boltCollider = &sIceBoltCollider,
    .spinCollider = &sIceSpinCollider,
    .lineCollider = &sIceLineCollider,
    .onBoltTrail = TrailIce,
    .onHit = HitIce,
    .onLineSpawn = LineIce,
    .onBoltsAlive = MeltRedIce,
    .boltDrawScale = 0.002f,
    .boltScrollStep = 10,
    .onBoltsDrawBegin = BeginFlameBolts,
    .onBoltDraw = DrawFlameBolt,
    .onBackfire = BackfireIce,
    .chargeColors = NULL,
    .state = &sIceState,
};

static const RodConfig sLightRod = {
    .key = LIGHT_ROD_KEY,
    .iconPath = sLightIconTex,
    .namePath = sLightNameTex,
    .opaqueDL = sLightRodDL,
    .xluDL = sLightRodXluDL,
    .colors = &sLightColors,
    .boltSpeed = 18.0f,
    .boltLifetime = 25,
    .boltMaxFrames = 25,
    .slashRange = 200.0f,
    .slashSpread = 30,
    .boltSpin = 6000,
    .boltGrowth = 0.5f,
    .magicBolt = 3,
    .magicLine = 6,
    .magicSpinSmall = 6,
    .magicSpinBig = 12,
    .backfireBolt = 10,
    .backfireLine = 20,
    .backfireSpin = 50,
    .chargeRate = 0.02f,
    .chargeMin = 0.1f,
    .chargeBig = 0.85f,
    .chargeHoldFrames = 10,
    .spinSmallRadius = 100.0f,
    .spinBigRadius = 500.0f,
    .sfxSwing = NA_SE_IT_SWORD_SWING,
    .sfxCharge = NA_SE_PL_SWORD_CHARGE,
    .sfxLoop = NA_SE_EN_FANTOM_SPARK,
    .sfxIgnite = NA_SE_EV_TRIFORCE_FLASH,
    .sfxHit = NA_SE_IT_SWORD_REFLECT_MG,
    .sfxCast = NA_SE_IT_LASH,
    .sfxLine = NA_SE_EN_FANTOM_SPARK,
    .ignitePeriod = 8,
    .lineBaseScale = 50.0f,
    .lineScaleGrow = 30.0f,
    .lineRadiusScale = 0.12f,
    .lineRadiusBase = 3.0f,
    .lineHeightScale = 0.2f,
    .lineHeightBase = 5.0f,
    .boltCollider = &sLightBoltCollider,
    .spinCollider = &sLightSpinCollider,
    .lineCollider = &sLightLineCollider,
    .onBoltTrail = TrailLight,
    .onHit = HitLight,
    .onLineSpawn = LineLight,
    .onBoltsAlive = NULL,
    .onBoltsDrawBegin = BeginOrbBolts,
    .onBoltDraw = DrawOrbBolt,
    .onBackfire = BackfireLight,
    .chargeColors = ChargeColorsLight,
    .state = &sLightState,
};

static const RodConfig* const sRods[] = { &sFireRod, &sIceRod, &sLightRod };

// ---- input and magic ------------------------------------------------------------------------------------

static u16 FindButtonMask(const RodConfig* cfg) {
    for (u8 button = 0; button < ROD_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);

        if (equipped != NULL && strcmp(equipped, cfg->key) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool IsButtonHeld(const RodConfig* cfg, PlayState* play) {
    u16 mask = FindButtonMask(cfg);

    return mask != 0 && (play->state.input[0].cur.button & mask) != 0;
}

static bool WasButtonPressed(const RodConfig* cfg, PlayState* play) {
    u16 mask = FindButtonMask(cfg);

    return mask != 0 && (play->state.input[0].press.button & mask) != 0;
}

/**
 * Pays for a cast. Failing to pay is the whole point of the rods: it rolls the element back onto Link.
 * Consuming leaves the meter flashing and refusing every later request until the spell resets it.
 */
static bool SpendMagic(const RodConfig* cfg, Player* player, PlayState* play, s16 cost, u8 backfireChance) {
    if (Magic_RequestChange(play, cost, MAGIC_CONSUME_NOW)) {
        Magic_Reset(play);
        return true;
    }
    if ((u8)(Rand_ZeroOne() * 100.0f) < backfireChance) {
        Audio_PlayActorSound2(&player->actor, NA_SE_PL_BODY_HIT);
        cfg->onBackfire(play, player);
    } else {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
    }
    return false;
}

static void PlayIgniteSfx(const RodConfig* cfg, Player* player, PlayState* play, u8 period) {
    if (cfg->ignitePeriod == 0) {
        Audio_PlayActorSound2(&player->actor, cfg->sfxIgnite - SFX_FLAG);
        return;
    }
    if ((play->gameplayFrames % period) == 0) {
        Audio_PlayActorSound2(&player->actor, cfg->sfxIgnite);
    }
}

static void SilenceRod(const RodConfig* cfg) {
    Audio_StopSfxById(cfg->sfxLoop);
    Audio_StopSfxById(cfg->sfxCharge);
    Audio_StopSfxById(cfg->sfxCast);
    Audio_StopSfxById(cfg->sfxLine);
}

// ---- bolts ----------------------------------------------------------------------------------------------

static void AimVelocity(const RodConfig* cfg, Vec3f* outVel, s16 yaw, s16 pitch) {
    Vec3f forward = { 0.0f, 0.0f, cfg->boltSpeed };

    Matrix_Push();
    Matrix_RotateY(BINANG_TO_RAD(yaw), MTXMODE_NEW);
    Matrix_RotateX(BINANG_TO_RAD(pitch), MTXMODE_APPLY);
    Matrix_MultVec3f(&forward, outVel);
    Matrix_Pop();
}

/**
 * Without this teardown the engine keeps the dead bolts' collider records, the global AT pool fills up and
 * hit registration silently dies for every custom item and the bow alike.
 */
static void DropSetColliders(RodBoltSet* set, PlayState* play) {
    if (!set->collidersReady) {
        return;
    }
    for (s32 i = 0; i < ROD_SET_BOLTS; i++) {
        Collider_DestroyCylinder(play, &set->colliders[i]);
    }
    set->collidersReady = false;
}

static RodBoltSet* TakeFreeSet(const RodConfig* cfg, PlayState* play) {
    RodState* state = cfg->state;
    RodBoltSet* oldest = &state->sets[0];

    for (s32 i = 0; i < ROD_MAX_SETS; i++) {
        if (!state->sets[i].active) {
            return &state->sets[i];
        }
        if (state->sets[i].timer < oldest->timer) {
            oldest = &state->sets[i];
        }
    }
    DropSetColliders(oldest, play);
    return oldest;
}

static void ReadySetColliders(const RodConfig* cfg, RodBoltSet* set, Player* player, PlayState* play) {
    if (set->collidersReady) {
        return;
    }
    for (s32 i = 0; i < ROD_SET_BOLTS; i++) {
        Collider_InitCylinder(play, &set->colliders[i]);
        Collider_SetCylinder(play, &set->colliders[i], &player->actor, cfg->boltCollider);
    }
    set->collidersReady = true;
}

static void LaunchBolts(const RodConfig* cfg, Player* player, PlayState* play, Vec3f* startPos, s16 yaw, s16 pitch,
                        u8 count, f32 range) {
    RodBoltSet* set = TakeFreeSet(cfg, play);
    s16 spread = (s16)(cfg->slashSpread * (0x10000 / 360));

    ReadySetColliders(cfg, set, player, play);
    set->active = true;
    set->count = count;
    set->scale = 0.0f;
    set->targetScale = ROD_BOLT_TARGET_SCALE;
    set->rotZ = 0;
    set->timer = (s16)(range / cfg->boltSpeed);
    if (set->timer < ROD_BOLT_MIN_FRAMES) {
        set->timer = ROD_BOLT_MIN_FRAMES;
    }
    if (count == 1 && set->timer > cfg->boltMaxFrames) {
        set->timer = cfg->boltMaxFrames;
    }

    for (s32 i = 0; i < ROD_TRAIL_POINTS; i++) {
        set->trail[i] = *startPos;
    }
    for (s32 i = 0; i < count; i++) {
        s16 boltYaw = yaw + (i == 0 ? 0 : (i == 1 ? -spread : spread));

        set->pos[i] = *startPos;
        AimVelocity(cfg, &set->vel[i], boltYaw, pitch);
    }
    Audio_PlayActorSound2(&player->actor, cfg->sfxSwing);
}

static void SizeBoltCollider(const RodConfig* cfg, ColliderCylinder* collider, Vec3f* pos, f32 scale, PlayState* play) {
    collider->dim.radius = (s16)(scale * 1.5f + 2.0f);
    collider->dim.height = (s16)(scale * 2.0f + 3.0f);
    collider->dim.pos.x = (s16)pos->x;
    collider->dim.pos.y = (s16)pos->y;
    collider->dim.pos.z = (s16)pos->z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &collider->base);
}

static Actor* TakeHitActor(ColliderCylinder* collider) {
    Actor* hit = collider->base.at;

    if (hit == NULL || hit->update == NULL) {
        return NULL;
    }
    return (hit->category == ACTORCAT_ENEMY || hit->category == ACTORCAT_BOSS) ? hit : NULL;
}

static bool ResolveHit(const RodConfig* cfg, ColliderCylinder* collider, Vec3f* pos, Player* player, PlayState* play) {
    if (!(collider->base.atFlags & AT_HIT)) {
        return false;
    }
    Audio_PlayActorSound2(&player->actor, cfg->sfxHit);
    cfg->onHit(play, player, TakeHitActor(collider), pos);
    collider->base.atFlags &= ~AT_HIT;
    return true;
}

static void UpdateBoltSet(const RodConfig* cfg, RodBoltSet* set, Player* player, PlayState* play) {
    bool anyHit = false;

    set->rotZ += cfg->boltSpin;
    if (set->timer > 0) {
        set->timer--;
    }
    if (set->timer == 0) {
        set->targetScale = 0.0f;
    }
    Math_ApproachF(&set->scale, set->targetScale, 0.2f, cfg->boltGrowth);

    if (set->timer == 0 && set->scale < 0.1f) {
        set->active = false;
        DropSetColliders(set, play);
        return;
    }

    for (s32 i = 0; i < set->count; i++) {
        set->pos[i].x += set->vel[i].x;
        set->pos[i].y += set->vel[i].y;
        set->pos[i].z += set->vel[i].z;
    }
    for (s32 i = ROD_TRAIL_POINTS - 2; i >= 0; i--) {
        set->trail[i + 1] = set->trail[i];
    }
    set->trail[0] = set->pos[0];

    if (set->scale >= ROD_BOLT_SPARK_SCALE) {
        for (s32 i = 0; i < set->count; i++) {
            cfg->onBoltTrail(play, &set->pos[i], set->scale);
        }
        Audio_PlayActorSound2(&player->actor, cfg->sfxLoop - SFX_FLAG);
    }
    if (set->scale >= ROD_BOLT_COLLIDER_SCALE) {
        for (s32 i = 0; i < set->count; i++) {
            SizeBoltCollider(cfg, &set->colliders[i], &set->pos[i], set->scale, play);
        }
    }
    for (s32 i = 0; i < set->count; i++) {
        anyHit |= ResolveHit(cfg, &set->colliders[i], &set->pos[i], player, play);
    }
    if (!anyHit) {
        return;
    }
    for (s32 i = 0; i < ROD_SET_BOLTS; i++) {
        set->vel[i].x = set->vel[i].y = set->vel[i].z = 0.0f;
    }
    set->timer = 0;
    set->targetScale = 0.0f;
}

static void UpdateBolts(const RodConfig* cfg, Player* player, PlayState* play) {
    RodState* state = cfg->state;
    bool anyAlive = false;

    for (s32 i = 0; i < ROD_MAX_SETS; i++) {
        if (!state->sets[i].active) {
            continue;
        }
        UpdateBoltSet(cfg, &state->sets[i], player, play);
        anyAlive |= state->sets[i].active;
    }
    if (!anyAlive) {
        Audio_StopSfxById(cfg->sfxLoop);
        return;
    }
    if (cfg->onBoltsAlive != NULL) {
        cfg->onBoltsAlive(play, cfg);
    }
}

/**
 * Red ice answers only to the blue-fire actor, never to a damage flag, so a bolt that reaches a block
 * spawns one instead of trying to break it. Bg_Ice_Shelter's own cylinder gives the block's reach.
 */
static void MeltRedIce(PlayState* play, const RodConfig* cfg) {
    RodState* state = cfg->state;

    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor != NULL; actor = actor->next) {
        BgIceShelter* ice = (BgIceShelter*)actor;
        RodBoltSet* spentSet = NULL;
        f32 reach;
        f32 height;
        bool touched = false;

        if (actor->id != ACTOR_BG_ICE_SHELTER) {
            continue;
        }
        reach = (f32)ice->cylinder1.dim.radius + ROD_RED_ICE_MARGIN;
        height = (f32)ice->cylinder1.dim.height;

        for (s32 i = 0; i < ROD_MAX_SETS && !touched; i++) {
            RodBoltSet* set = &state->sets[i];

            if (!set->active) {
                continue;
            }
            for (s32 bolt = 0; bolt < set->count && !touched; bolt++) {
                f32 dx = set->pos[bolt].x - actor->world.pos.x;
                f32 dy = set->pos[bolt].y - actor->world.pos.y;
                f32 dz = set->pos[bolt].z - actor->world.pos.z;

                touched =
                    sqrtf(SQ(dx) + SQ(dz)) < reach && dy > -ROD_RED_ICE_MARGIN && dy < height + ROD_RED_ICE_MARGIN;
                if (touched) {
                    spentSet = set;
                }
            }
        }
        if (touched) {
            Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ICE_HONO, actor->world.pos.x, actor->world.pos.y,
                        actor->world.pos.z, 0, 0, 0, 0);
            spentSet->active = false;
            DropSetColliders(spentSet, play);
        }
    }
}

// ---- ground line (jump slash) ---------------------------------------------------------------------------

static f32 LineScaleAt(const RodConfig* cfg, s32 index) {
    return cfg->lineBaseScale + index * cfg->lineScaleGrow;
}

static void StartLine(const RodConfig* cfg, Player* player, PlayState* play) {
    RodState* state = cfg->state;
    Vec3f groundPos = player->actor.world.pos;
    s16 facing = player->actor.shape.rot.y;

    if (!state->lineReady) {
        for (s32 i = 0; i < ROD_LINE_COUNT; i++) {
            Collider_InitCylinder(play, &state->line[i]);
            Collider_SetCylinder(play, &state->line[i], &player->actor, cfg->lineCollider);
        }
        state->lineReady = true;
    }
    groundPos.y = player->actor.floorHeight + 5.0f;
    state->lineActive = true;
    state->lineTimer = ROD_LINE_FRAMES;

    for (s32 i = 0; i < ROD_LINE_COUNT; i++) {
        f32 distance = (i + 1) * ROD_LINE_SPACING;

        state->linePos[i].x = groundPos.x + distance * Math_SinS(facing);
        state->linePos[i].y = groundPos.y + 10.0f;
        state->linePos[i].z = groundPos.z + distance * Math_CosS(facing);
        cfg->onLineSpawn(play, player, &state->linePos[i], LineScaleAt(cfg, i));
    }
    Audio_PlayActorSound2(&player->actor, cfg->sfxLine);
    Audio_PlayActorSound2(&player->actor, cfg->sfxLoop);
}

static void UpdateLine(const RodConfig* cfg, Player* player, PlayState* play) {
    RodState* state = cfg->state;

    if (!state->lineActive) {
        return;
    }
    if (state->lineTimer <= 0) {
        state->lineActive = false;
        Audio_StopSfxById(cfg->sfxLoop);
        Audio_StopSfxById(cfg->sfxLine);
        return;
    }
    state->lineTimer--;

    for (s32 i = 0; i < ROD_LINE_COUNT; i++) {
        f32 scale = LineScaleAt(cfg, i);

        state->line[i].dim.radius = (s16)(scale * cfg->lineRadiusScale + cfg->lineRadiusBase);
        state->line[i].dim.height = (s16)(scale * cfg->lineHeightScale + cfg->lineHeightBase);
        state->line[i].dim.pos.x = (s16)state->linePos[i].x;
        state->line[i].dim.pos.y = (s16)state->linePos[i].y;
        state->line[i].dim.pos.z = (s16)state->linePos[i].z;
        CollisionCheck_SetAT(play, &play->colChkCtx, &state->line[i].base);
        ResolveHit(cfg, &state->line[i], &state->linePos[i], player, play);
    }
}

// ---- spin cylinder --------------------------------------------------------------------------------------

static void StartSpin(const RodConfig* cfg, Player* player, PlayState* play, u8 isBigSpin) {
    RodState* state = cfg->state;
    s16 cost = isBigSpin ? cfg->magicSpinBig : cfg->magicSpinSmall;

    if (!SpendMagic(cfg, player, play, cost, cfg->backfireSpin)) {
        return;
    }
    if (!state->spinReady) {
        Collider_InitCylinder(play, &state->spin);
        Collider_SetCylinder(play, &state->spin, &player->actor, cfg->spinCollider);
        state->spinReady = true;
    }
    state->spinActive = true;
    state->spinIsBig = isBigSpin;
    state->spinRadius = ROD_SPIN_START_RADIUS;
    state->spinMaxRadius = isBigSpin ? cfg->spinBigRadius : cfg->spinSmallRadius;
    Audio_PlayActorSound2(&player->actor, cfg->sfxCast);
}

static void UpdateSpin(const RodConfig* cfg, Player* player, PlayState* play) {
    RodState* state = cfg->state;

    if (!state->spinActive) {
        return;
    }
    state->spinRadius += state->spinIsBig ? ROD_SPIN_GROW_BIG : ROD_SPIN_GROW_SMALL;
    if (state->spinRadius > state->spinMaxRadius) {
        state->spinRadius = state->spinMaxRadius;
    }
    state->spin.dim.radius = (s16)state->spinRadius;
    state->spin.dim.height = ROD_SPIN_HEIGHT;
    state->spin.dim.pos.x = (s16)player->actor.world.pos.x;
    state->spin.dim.pos.y = (s16)player->actor.world.pos.y;
    state->spin.dim.pos.z = (s16)player->actor.world.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &state->spin.base);
    ResolveHit(cfg, &state->spin, &player->actor.world.pos, player, play);
    PlayIgniteSfx(cfg, player, play, 6);
}

static void StopSpin(const RodConfig* cfg) {
    cfg->state->spinActive = false;
    cfg->state->spinRadius = 0.0f;
    Audio_StopSfxById(cfg->sfxCast);
}

// ---- swing ----------------------------------------------------------------------------------------------

static bool IsSpinAnim(u8 weaponAnim) {
    return weaponAnim == PLAYER_MWA_SPIN_ATTACK_1H || weaponAnim == PLAYER_MWA_SPIN_ATTACK_2H ||
           weaponAnim == PLAYER_MWA_BIG_SPIN_1H || weaponAnim == PLAYER_MWA_BIG_SPIN_2H;
}

static void SpawnSwingSparks(const RodConfig* cfg, Player* player, PlayState* play) {
    Vec3f* tip = &player->meleeWeaponInfo[0].tip;
    Vec3f* base = &player->meleeWeaponInfo[0].base;
    Color_RGBA8 prim = cfg->colors->prim;
    Color_RGBA8 env = { cfg->colors->env.r, cfg->colors->env.g, cfg->colors->env.b, 0 };
    Vec3f vel = { 0.0f, 0.5f, 0.0f };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };

    if ((play->gameplayFrames % 2) == 0) {
        EffectSsGSpk_SpawnAccel(play, &player->actor, tip, &vel, &accel, &prim, &env, 100, 10);
    }
    if (cfg->state->blureIdx >= 0) {
        EffectBlure* blure = Effect_GetByIndex(cfg->state->blureIdx);

        if (blure != NULL) {
            EffectBlure_AddVertex(blure, tip, base);
        }
    }
    PlayIgniteSfx(cfg, player, play, 8);
}

static void ThrowSlash(const RodConfig* cfg, Player* player, PlayState* play) {
    Vec3f* tip = &player->meleeWeaponInfo[0].tip;
    s16 yaw = player->actor.shape.rot.y;
    s16 pitch = 0;

    if (!SpendMagic(cfg, player, play, cfg->magicBolt, cfg->backfireBolt)) {
        return;
    }
    if (Player_IsZTargeting(player) && player->focusActor != NULL && player->focusActor->update != NULL) {
        yaw = Math_Vec3f_Yaw(tip, &player->focusActor->focus.pos);
        pitch = Math_Vec3f_Pitch(tip, &player->focusActor->focus.pos);
    }
    LaunchBolts(cfg, player, play, tip, yaw, pitch, ROD_SET_BOLTS, cfg->slashRange);
}

static void ThrowStab(const RodConfig* cfg, Player* player, PlayState* play) {
    Vec3f* tip = &player->meleeWeaponInfo[0].tip;
    Vec3f* base = &player->meleeWeaponInfo[0].base;
    s16 yaw;
    s16 pitch;

    if (!SpendMagic(cfg, player, play, cfg->magicBolt, cfg->backfireBolt)) {
        return;
    }
    if (Player_IsZTargeting(player) && player->focusActor != NULL && player->focusActor->update != NULL) {
        yaw = Math_Vec3f_Yaw(tip, &player->focusActor->focus.pos);
        pitch = Math_Vec3f_Pitch(tip, &player->focusActor->focus.pos);
    } else {
        yaw = Math_Vec3f_Yaw(base, tip);
        pitch = Math_Vec3f_Pitch(base, tip);
    }
    LaunchBolts(cfg, player, play, tip, yaw, pitch, 1, cfg->boltLifetime * cfg->boltSpeed);
}

static void ThrowLine(const RodConfig* cfg, Player* player, PlayState* play) {
    if (!SpendMagic(cfg, player, play, cfg->magicLine, cfg->backfireLine)) {
        return;
    }
    StartLine(cfg, player, play);
}

static void ProcessSwing(const RodConfig* cfg, Player* player, PlayState* play) {
    RodState* state = cfg->state;
    u8 weaponAnim = player->meleeWeaponAnimation;

    SpawnSwingSparks(cfg, player, play);

    if (IsSpinAnim(weaponAnim)) {
        if (!state->spinActive) {
            StartSpin(cfg, player, play, weaponAnim == PLAYER_MWA_BIG_SPIN_1H || weaponAnim == PLAYER_MWA_BIG_SPIN_2H);
        }
        UpdateSpin(cfg, player, play);
    } else if (state->spinActive) {
        StopSpin(cfg);
    }

    if (weaponAnim == state->lastSwing) {
        return;
    }
    state->lastSwing = weaponAnim;

    switch (weaponAnim) {
        case PLAYER_MWA_FORWARD_SLASH_1H:
        case PLAYER_MWA_FORWARD_SLASH_2H:
        case PLAYER_MWA_FORWARD_COMBO_1H:
        case PLAYER_MWA_FORWARD_COMBO_2H:
        case PLAYER_MWA_RIGHT_SLASH_1H:
        case PLAYER_MWA_RIGHT_SLASH_2H:
        case PLAYER_MWA_RIGHT_COMBO_1H:
        case PLAYER_MWA_RIGHT_COMBO_2H:
        case PLAYER_MWA_LEFT_SLASH_1H:
        case PLAYER_MWA_LEFT_SLASH_2H:
        case PLAYER_MWA_LEFT_COMBO_1H:
        case PLAYER_MWA_LEFT_COMBO_2H:
            ThrowSlash(cfg, player, play);
            break;
        case PLAYER_MWA_STAB_1H:
        case PLAYER_MWA_STAB_2H:
        case PLAYER_MWA_STAB_COMBO_1H:
        case PLAYER_MWA_STAB_COMBO_2H:
            ThrowStab(cfg, player, play);
            break;
        case PLAYER_MWA_FLIPSLASH_START:
        case PLAYER_MWA_JUMPSLASH_START:
            state->jumpSpawned = false;
            break;
        case PLAYER_MWA_FLIPSLASH_FINISH:
        case PLAYER_MWA_JUMPSLASH_FINISH:
            if (!state->jumpSpawned) {
                ThrowLine(cfg, player, play);
                state->jumpSpawned = true;
            }
            break;
        default:
            break;
    }
}

// ---- charge ---------------------------------------------------------------------------------------------

static bool CanCharge(Player* player) {
    if (player->meleeWeaponState > 0) {
        return false;
    }
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED |
                               PLAYER_STATE1_LOADING | PLAYER_STATE1_HOOKSHOT_FALLING)) {
        return false;
    }
    if (player->stateFlags2 & PLAYER_STATE2_HOPPING) {
        return false;
    }
    return (player->actor.bgCheckFlags & 1) != 0;
}

static void ClearCharge(const RodConfig* cfg) {
    RodState* state = cfg->state;

    state->charging = false;
    state->chargeReady = false;
    state->chargeLevel = 0.0f;
    state->chargeTimer = 0;
    state->buttonHeld = false;
    state->holdFrames = 0;
    Audio_StopSfxById(cfg->sfxCharge);
}

static void StartCharge(const RodConfig* cfg, Player* player) {
    RodState* state = cfg->state;

    state->charging = true;
    state->chargeReady = false;
    state->chargeLevel = 0.0f;
    state->chargeTimer = 0;
    Audio_PlayActorSound2(&player->actor, cfg->sfxCharge);
}

static void UpdateCharge(const RodConfig* cfg, Player* player, PlayState* play) {
    RodState* state = cfg->state;

    state->chargeTimer++;
    if (state->chargeLevel < 1.0f) {
        state->chargeLevel += cfg->chargeRate;
        if (state->chargeLevel > 1.0f) {
            state->chargeLevel = 1.0f;
        }
    }
    if (!state->chargeReady && state->chargeLevel >= cfg->chargeMin) {
        state->chargeReady = true;
        Audio_PlayActorSound2(&player->actor, cfg->sfxCharge);
    }
    if (state->chargeLevel >= cfg->chargeBig && state->chargeTimer == (s16)(cfg->chargeBig / cfg->chargeRate)) {
        Audio_PlayActorSound2(&player->actor, cfg->sfxCharge);
    }
    if ((play->gameplayFrames % 3) == 0) {
        Color_RGBA8 prim = cfg->colors->prim;
        Color_RGBA8 env = { cfg->colors->env.r, cfg->colors->env.g, cfg->colors->env.b, 0 };
        Vec3f vel = { 0.0f, 0.5f, 0.0f };
        Vec3f accel = { 0.0f, 0.0f, 0.0f };

        EffectSsGSpk_SpawnAccel(play, &player->actor, &player->leftHandPos, &vel, &accel, &prim, &env, 100, 10);
    }
    PlayIgniteSfx(cfg, player, play, 12);
}

static void ReleaseCharge(const RodConfig* cfg, Player* player, PlayState* play) {
    RodState* state = cfg->state;
    s32 spinAnim;

    if (state->chargeLevel >= cfg->chargeBig) {
        spinAnim = PLAYER_MWA_BIG_SPIN_1H;
    } else if (state->chargeLevel >= cfg->chargeMin) {
        spinAnim = PLAYER_MWA_SPIN_ATTACK_1H;
    } else {
        ClearCharge(cfg);
        return;
    }
    ClearCharge(cfg);
    func_80837948(play, player, spinAnim);
    Audio_PlayActorSound2(&player->actor, cfg->sfxSwing);
}

// ---- first person ---------------------------------------------------------------------------------------

static void EnterAim(const RodConfig* cfg, Player* player, PlayState* play) {
    cfg->state->aiming = true;
    Z64Aiming_Request(player, play);
}

static void ExitAim(const RodConfig* cfg, Player* player, PlayState* play) {
    cfg->state->aiming = false;
    Z64Aiming_Release(player, play);
}

static void FireAimedBolt(const RodConfig* cfg, Player* player, PlayState* play) {
    s16 yaw;
    s16 pitch;
    Vec3f startPos;

    if (!SpendMagic(cfg, player, play, cfg->magicBolt, cfg->backfireBolt)) {
        return;
    }
    Z64Aiming_GetDirection(player, &yaw, &pitch);
    startPos.x = player->actor.world.pos.x + 30.0f * Math_SinS(yaw);
    startPos.y = player->actor.world.pos.y + 40.0f;
    startPos.z = player->actor.world.pos.z + 30.0f * Math_CosS(yaw);
    LaunchBolts(cfg, player, play, &startPos, yaw, pitch, 1, cfg->boltLifetime * cfg->boltSpeed);
}

static void UpdateAim(const RodConfig* cfg, Player* player, PlayState* play) {
    u16 exitButtons = BTN_A | BTN_B | BTN_CLEFT | BTN_CRIGHT | BTN_CDOWN;

    Z64Aiming_Update(player, play);
    if (WasButtonPressed(cfg, play)) {
        FireAimedBolt(cfg, player, play);
    }
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_CUP)) {
        ExitAim(cfg, player, play);
        return;
    }
    exitButtons &= ~FindButtonMask(cfg);
    if (CHECK_BTN_ANY(play->state.input[0].press.button, exitButtons) ||
        (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED))) {
        ExitAim(cfg, player, play);
    }
}

// ---- held item cycle ------------------------------------------------------------------------------------

static void ResetRod(const RodConfig* cfg, PlayState* play) {
    RodState* state = cfg->state;

    ClearCharge(cfg);
    state->lastSwing = 0;
    state->jumpSpawned = false;
    state->lineActive = false;
    state->lineTimer = 0;
    for (s32 i = 0; i < ROD_MAX_SETS; i++) {
        DropSetColliders(&state->sets[i], play);
        state->sets[i].active = false;
    }
    if (state->spinActive) {
        StopSpin(cfg);
    }
    SilenceRod(cfg);
}

static void StartTrail(const RodConfig* cfg, PlayState* play) {
    EffectBlureInit1 blureInit = { 0 };
    s32 index = -1;

    for (s32 i = 0; i < 4; i++) {
        blureInit.p1StartColor[i] = ((const u8*)&cfg->colors->prim)[i];
        blureInit.p2StartColor[i] = ((const u8*)&cfg->colors->env)[i];
        blureInit.p1EndColor[i] = ((const u8*)&cfg->colors->prim)[i];
        blureInit.p2EndColor[i] = ((const u8*)&cfg->colors->env)[i];
    }
    blureInit.p1EndColor[3] = 0;
    blureInit.p2EndColor[3] = 0;
    blureInit.elemDuration = ROD_TRAIL_FRAMES;
    blureInit.calcMode = 2;

    Effect_Add(play, &index, EFFECT_BLURE1, 0, 0, &blureInit);
    cfg->state->blureIdx = index;
}

static void EndTrail(const RodConfig* cfg, PlayState* play) {
    if (cfg->state->blureIdx < 0) {
        return;
    }
    Effect_Delete(play, cfg->state->blureIdx);
    cfg->state->blureIdx = -1;
}

static void TakeOutFireRod(PlayState* play, Player* player);
static void TakeOutIceRod(PlayState* play, Player* player);
static void TakeOutLightRod(PlayState* play, Player* player);

static void TakeOutRod(const RodConfig* cfg, PlayState* play) {
    if (sRodInHand != NULL && sRodInHand != cfg) {
        ResetRod(sRodInHand, play);
        EndTrail(sRodInHand, play);
    }
    sRodInHand = cfg;
    ResetRod(cfg, play);
    StartTrail(cfg, play);
}

static void StowRod(const RodConfig* cfg, Player* player, PlayState* play) {
    if (cfg->state->aiming) {
        ExitAim(cfg, player, play);
    }
    ClearCharge(cfg);
    SilenceRod(cfg);
    EndTrail(cfg, play);
    if (cfg->state->spinActive) {
        StopSpin(cfg);
    }
    if (sRodInHand == cfg) {
        sRodInHand = NULL;
    }
}

/** Shielding and swapping items are all the sword's upper action does; the swing itself is an action handler. */
static int32_t HoldRod(Player* player, PlayState* play) {
    return Player_UpperAction_Sword(player, play);
}

static void RunRodInHand(const RodConfig* cfg, Player* player, PlayState* play) {
    RodState* state = cfg->state;
    u32 stolenByEngine = PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                         PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM |
                         PLAYER_STATE1_DAMAGED | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
                         PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_HOOKSHOT_FALLING;

    // The swing runs from an action func, which does not run the upper action: without this the host's
    // watchdog reads four idle frames as an item that left the hand and stows the rod mid-attack.
    Z64Items_KeepHeld(sApi);

    if (state->aiming) {
        UpdateAim(cfg, player, play);
        return;
    }
    if (player->stateFlags1 & stolenByEngine) {
        ClearCharge(cfg);
        SilenceRod(cfg);
        return;
    }
    if (player->meleeWeaponState == 0 && !state->charging &&
        CHECK_BTN_ALL(play->state.input[0].press.button, BTN_CUP)) {
        EnterAim(cfg, player, play);
        return;
    }

    if (state->charging) {
        if (IsButtonHeld(cfg, play)) {
            UpdateCharge(cfg, player, play);
        } else {
            ReleaseCharge(cfg, player, play);
        }
    } else if (IsButtonHeld(cfg, play) && CanCharge(player)) {
        state->holdFrames++;
        if (state->holdFrames >= cfg->chargeHoldFrames) {
            StartCharge(cfg, player);
        }
    } else {
        state->holdFrames = 0;
    }

    if (player->meleeWeaponState > 0) {
        ProcessSwing(cfg, player, play);
        if (state->charging) {
            ClearCharge(cfg);
        }
        return;
    }
    if (!state->charging) {
        state->lastSwing = 0;
    }
    if (state->spinActive) {
        StopSpin(cfg);
    }
}

static void TickRods(void) {
    PlayState* play = gPlayState;
    Player* player;

    if (play == NULL) {
        return;
    }
    player = GET_PLAYER(play);

    for (s32 i = 0; i < ARRAY_COUNT(sRods); i++) {
        const RodConfig* cfg = sRods[i];

        UpdateBolts(cfg, player, play);
        UpdateLine(cfg, player, play);
        if (cfg == sRodInHand && player->heldItemAction == PLAYER_IA_CUSTOM) {
            RunRodInHand(cfg, player, play);
        }
    }
}

static void DropRodsOnSceneInit(int16_t sceneNum) {
    for (s32 i = 0; i < ARRAY_COUNT(sRods); i++) {
        RodState* state = sRods[i]->state;

        // The player and every collider are rebuilt across a scene, so nothing here may outlive it.
        state->blureIdx = -1;
        state->lineReady = false;
        state->spinReady = false;
        state->lineActive = false;
        state->spinActive = false;
        state->aiming = false;
        for (s32 set = 0; set < ROD_MAX_SETS; set++) {
            state->sets[set].active = false;
            state->sets[set].collidersReady = false;
        }
        ClearCharge(sRods[i]);
    }
    sRodInHand = NULL;
    Z64Aiming_Drop();
}

// ---- drawing --------------------------------------------------------------------------------------------

/**
 * The rod follows the forearm-to-hand direction the way NEI dialled it in; the offsets and the 0.05 scale
 * are that calibration, so they only make sense against this construction.
 */
static void DrawRodInHand(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    const RodConfig* cfg = sRodInHand;
    Vec3f forearm;
    Vec3f hand;
    f32 dx;
    f32 dy;
    f32 dz;

    if (cfg == NULL || player->heldItemAction != PLAYER_IA_CUSTOM ||
        (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    forearm = player->bodyPartsPos[PLAYER_BODYPART_L_FOREARM];
    hand = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    dx = hand.x - forearm.x;
    dy = hand.y - forearm.y;
    dz = hand.z - forearm.z;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(hand.x, hand.y, hand.z, MTXMODE_NEW);
    Matrix_RotateY(atan2f(dx, dz), MTXMODE_APPLY);
    Matrix_RotateX(-atan2f(dy, sqrtf(SQ(dx) + SQ(dz))), MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD(0x4000), MTXMODE_APPLY);
    Matrix_Translate(-ROD_HELD_SIDE_OFFSET, 0.0f, ROD_HELD_SIDE_OFFSET, MTXMODE_APPLY);
    Matrix_Scale(ROD_HELD_SCALE, ROD_HELD_SCALE, ROD_HELD_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)cfg->opaqueDL);

    if (cfg->xluDL != NULL) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)cfg->xluDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawRodModel(PlayState* play, const RodConfig* cfg) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(ROD_GIVE_SCALE, ROD_GIVE_SCALE, ROD_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)cfg->opaqueDL);
    if (cfg->xluDL != NULL) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)cfg->xluDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawFireRodGive(PlayState* play, GetItemEntry* entry) {
    DrawRodModel(play, &sFireRod);
}

static void DrawIceRodGive(PlayState* play, GetItemEntry* entry) {
    DrawRodModel(play, &sIceRod);
}

static void DrawLightRodGive(PlayState* play, GetItemEntry* entry) {
    DrawRodModel(play, &sLightRod);
}

// OPEN_DISPS declares the pointer POLY_XLU_DISP writes through, so every function that emits commands opens
// its own scope: borrowing the caller's leaves __gfxCtx as an unresolved external at link time.
static void BeginFlameBolts(PlayState* play, const RodConfig* cfg) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                           (play->gameplayFrames * -cfg->boltScrollStep) & 0x1FF, 0x20, 0x80));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, cfg->colors->prim.r, cfg->colors->prim.g, cfg->colors->prim.b,
                    cfg->colors->prim.a);
    gDPSetEnvColor(POLY_XLU_DISP++, cfg->colors->env.r, cfg->colors->env.g, cfg->colors->env.b, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawFlameBolt(PlayState* play, const RodConfig* cfg, Vec3f* pos, f32 scale, s32 index) {
    f32 drawScale = scale * cfg->boltDrawScale;

    if (drawScale < ROD_FLAME_BOLT_MIN) {
        drawScale = ROD_FLAME_BOLT_MIN;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000), MTXMODE_APPLY);
    Matrix_Scale(drawScale, drawScale, drawScale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBoltDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Phantom Ganon's energy ball: a fixed-size orb that spins on its own axis, so it ignores the bolt's growth.
static void BeginOrbBolts(PlayState* play, const RodConfig* cfg) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 200);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 50, 0);
    gDPPipeSync(POLY_XLU_DISP++);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawOrbBolt(PlayState* play, const RodConfig* cfg, Vec3f* pos, f32 scale, s32 index) {
    s16 spin = (play->gameplayFrames * 0x1000) + index * 0x5555;
    f32 drawScale = ROD_ORB_BOLT_SCALE * (scale / ROD_BOLT_TARGET_SCALE);

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(drawScale, drawScale, drawScale, MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(spin), MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sOrbDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawBolts(const RodConfig* cfg, PlayState* play) {
    RodState* state = cfg->state;
    bool anyAlive = false;

    for (s32 i = 0; i < ROD_MAX_SETS && !anyAlive; i++) {
        anyAlive = state->sets[i].active;
    }
    if (!anyAlive) {
        return;
    }
    cfg->onBoltsDrawBegin(play, cfg);

    for (s32 i = 0; i < ROD_MAX_SETS; i++) {
        RodBoltSet* set = &state->sets[i];

        if (!set->active) {
            continue;
        }
        for (s32 bolt = 0; bolt < set->count; bolt++) {
            cfg->onBoltDraw(play, cfg, &set->pos[bolt], set->scale, bolt);
        }
        for (s32 point = 1; point < 4; point++) {
            cfg->onBoltDraw(play, cfg, &set->trail[point], set->scale * (1.0f - point * 0.25f), point);
        }
    }
}

static void DrawRing(PlayState* play, Player* player, const RodColors* colors, f32 scaleXZ, f32 scaleY, u8 alpha,
                     bool faceLink) {
    u32 scroll = play->gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, scroll & 0x7F, 0, 0x20, 0x40, 1, 0,
                                           (scroll * -15) & 0xFF, 0x20, 0x40));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, colors->prim.r, colors->prim.g, colors->prim.b, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, colors->env.r, colors->env.g, colors->env.b, 0);
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + 5.0f, player->actor.world.pos.z,
                     MTXMODE_NEW);
    if (faceLink) {
        Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    }
    Matrix_Scale(scaleXZ, scaleY, scaleXZ, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sRingDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawChargeAura(const RodConfig* cfg, Player* player, PlayState* play) {
    RodState* state = cfg->state;
    const RodColors* colors = cfg->chargeColors != NULL ? cfg->chargeColors(state, cfg) : cfg->colors;
    f32 pulse = 1.0f + 0.1f * Math_SinS((s16)(play->gameplayFrames * 0x800));
    f32 scaleXZ = (0.02f + state->chargeLevel * 0.04f) * pulse;
    f32 scaleY = 0.025f + state->chargeLevel * 0.015f;
    u8 alpha = state->chargeLevel < cfg->chargeBig ? (u8)(100 + state->chargeLevel / cfg->chargeBig * 100.0f) : 220;

    DrawRing(play, player, colors, scaleXZ, scaleY, alpha, true);
}

static void DrawSpinCylinder(const RodConfig* cfg, Player* player, PlayState* play) {
    RodState* state = cfg->state;
    f32 pulse = 1.0f + 0.05f * Math_SinS((s16)(play->gameplayFrames * 0x1000));

    DrawRing(play, player, cfg->colors, (state->spinRadius / 1000.0f) * pulse, 0.08f, state->spinIsBig ? 220 : 200,
             false);
}

static void DrawRodEffects(void) {
    PlayState* play = gPlayState;
    Player* player;

    if (play == NULL) {
        return;
    }
    player = GET_PLAYER(play);

    for (s32 i = 0; i < ARRAY_COUNT(sRods); i++) {
        const RodConfig* cfg = sRods[i];

        DrawBolts(cfg, play);
        if (cfg != sRodInHand) {
            continue;
        }
        if (cfg->state->charging) {
            DrawChargeAura(cfg, player, play);
        }
        if (cfg->state->spinActive) {
            DrawSpinCylinder(cfg, player, play);
        }
        if (cfg->state->aiming) {
            Z64Aiming_DrawReticle(play, player, ROD_AIM_RANGE);
        }
    }
}

// ---- registration ---------------------------------------------------------------------------------------

static void TakeOutFireRod(PlayState* play, Player* player) {
    TakeOutRod(&sFireRod, play);
}

static void TakeOutIceRod(PlayState* play, Player* player) {
    TakeOutRod(&sIceRod, play);
}

static void TakeOutLightRod(PlayState* play, Player* player) {
    TakeOutRod(&sLightRod, play);
}

static void StowFireRod(Player* player, PlayState* play) {
    StowRod(&sFireRod, player, play);
}

static void StowIceRod(Player* player, PlayState* play) {
    StowRod(&sIceRod, player, play);
}

static void StowLightRod(Player* player, PlayState* play) {
    StowRod(&sLightRod, player, play);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

static bool RegisterRod(const RodConfig* cfg, u8 slot, SOHCustomItemInitFunc takeOut, SOHCustomItemPlayerFunc stow,
                        CustomDrawFunc drawGive, const char* getItemText, const char* pauseText) {
    SOHCustomItemDefinition rod = Z64Items_Define(cfg->key, cfg->iconPath, cfg->namePath);

    Z64Items_SetButtons(&rod, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&rod, 0, slot, 0);
    Z64Items_SetAction(&rod, takeOut, HoldRod);
    Z64Items_SetHeldCallbacks(&rod, NULL, stow, NULL);
    Z64Items_SetTextbox(&rod, getItemText);
    Z64Items_SetPauseText(&rod, pauseText);
    // The kokiri index, not the deku stick's: a stick measures zero in sMeleeWeaponLengths, so its quad would
    // never touch anything. Group 10 is the stick's own: a closed fist, with the weapon drawn separately.
    Z64Items_SetMeleeWeapon(&rod, 2);
    rod.modelGroup = PLAYER_MODELGROUP_10;
    rod.getItemEntry.drawFunc = drawGive;
    return Z64Items_Register(sApi, &rod);
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, KeepCustomItemHeld)) {
        return;
    }

    if (!RegisterRod(&sFireRod, 4, TakeOutFireRod, StowFireRod, DrawFireRodGive,
                     "You got the %rFire Rod%w!&A magic weapon that channels fire.^%ySwing%w for three "
                     "fireballs,&%ystab%w for one long shot,&%yjump%w to lay a flame trail.^%rHold%w %y\xA1%w to "
                     "charge a fire wave,&%c\xA5%w to aim in first person.^Without magic the fire burns %rYOU%w.",
                     "%rFire Rod&%wSwing to cast, hold %y\xA1%w to charge,&%c\xA5%w to aim.")) {
        return;
    }
    if (!RegisterRod(&sIceRod, 10, TakeOutIceRod, StowIceRod, DrawIceRodGive,
                     "You got the %cIce Rod%w!&A magic weapon that channels ice.^%ySwing%w for three "
                     "iceballs,&%ystab%w for one long shot,&%yjump%w to lay an ice trail.^%rHold%w %y\xA1%w to "
                     "charge an ice wave,&%c\xA5%w to aim in first person.^It freezes what it hits — and red ice "
                     "with it.",
                     "%cIce Rod&%wSwing to cast, hold %y\xA1%w to charge,&%c\xA5%w to aim.")) {
        return;
    }
    if (!RegisterRod(&sLightRod, 16,TakeOutLightRod, StowLightRod, DrawLightRodGive,
                     "You got the %yLight Rod%w!&A magic weapon that channels light.^%ySwing%w for three "
                     "light balls,&%ystab%w for one long shot,&%yjump%w to lay a trail of light.^%rHold%w %y\xA1%w "
                     "to charge a light wave,&%c\xA5%w to aim in first person.^The undead freeze where they stand.",
                     "%yLight Rod&%wSwing to cast, hold %y\xA1%w to charge,&%c\xA5%w to aim.")) {
        return;
    }

    for (s32 i = 0; i < ARRAY_COUNT(sRods); i++) {
        sRods[i]->state->blureIdx = -1;
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickRods);
    SOH_REGISTER_HOOK(sApi, OnPlayDrawEnd, DrawRodEffects);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, DropRodsOnSceneInit);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawRodInHand);
}
