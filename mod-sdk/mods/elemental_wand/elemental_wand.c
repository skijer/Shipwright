/**
 * elemental_wand — six rods in one staff (Skijer's NEI).
 *
 * One item, one cell, one wheel: Sand, Tornado, Water, Meteor, Storm and the Shadow Scepter share the staff
 * and which one is live is the wheel's selection. Casting is a double sword swing written over the walk, so
 * a rod can go off at a run, and the rod itself fires on a frame of the second slash.
 *
 * Which rods are yours is a randomizer setting (Randomizer > Mods): the six medallions, one item lighting
 * all six, or a rod per pickup.
 */

#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64wheel.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Light/z_en_light.h"
#include "overlays/actors/ovl_En_Siofuki/z_en_siofuki.h"

#define WAND_KEY "nei.elemental_wand"
#define WAND_SHUFFLE_OPTION "nei.wand_shuffle"
#define WAND_RODS_FIELD "rods"
#define WAND_BUTTON_COUNT 8

// Dialled in game and baked by NEI. The staff measures 350 units tall, which is why its scale is not a
// rod's. Order: rotation Y, X, Z, then the offsets along the shaft's own axes, then scale.
#define WAND_HELD_ROT_Y 180.0f
#define WAND_HELD_ROT_X 91.034f
#define WAND_HELD_ROT_Z 111.724f
#define WAND_HELD_OFFSET_Y 7.356f
#define WAND_HELD_OFFSET_Z (-3.218f)
#define WAND_HELD_SCALE 0.12f
#define WAND_GIVE_SCALE 0.3f

// How far into the second slash the rod goes off: the summon belongs to the animation, not to the button.
#define WAND_CAST_FRACTION 0.35f

typedef enum {
    WAND_SAND,
    WAND_TORNADO,
    WAND_WATER,
    WAND_METEOR,
    WAND_STORM,
    WAND_SCEPTER,
    WAND_MODE_MAX,
} WandMode;

// The randomizer treatments, in the order the dropdown lists them.
typedef enum {
    WAND_SHUFFLE_MEDALLIONS,
    WAND_SHUFFLE_SINGLE,
    WAND_SHUFFLE_RODS,
} WandShuffle;

typedef enum {
    WAND_SWING_NONE,
    WAND_SWING_FORE,
    WAND_SWING_BACK,
} WandSwingClip;

typedef struct {
    const char* opaModel;
    const char* xluModel;
    uint16_t questItem;
    s16 magicCost;
} WandRod;

// ---- assets -----------------------------------------------------------------------------------------------

static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gSandRodNameTex";
static const ALIGN_ASSET(2) char sIconSand[] = "__OTR__textures/icon_item_custom/gItemIconSandRodTex";
static const ALIGN_ASSET(2) char sIconTornado[] = "__OTR__textures/icon_item_custom/gItemIconTornadoRodTex";
static const ALIGN_ASSET(2) char sIconWater[] = "__OTR__textures/icon_item_custom/gItemIconWaterRodTex";
static const ALIGN_ASSET(2) char sIconMeteor[] = "__OTR__textures/icon_item_custom/gItemIconMeteorRodTex";
static const ALIGN_ASSET(2) char sIconStorm[] = "__OTR__textures/icon_item_custom/gItemIconStormRodTex";
static const ALIGN_ASSET(2) char sIconScepter[] = "__OTR__textures/icon_item_custom/gItemIconShadowScepterTex";

static const ALIGN_ASSET(2) char sSandOpa[] = "__OTR__objects/object_nei_wand_sand_rod/gNeiSandRodDL";
static const ALIGN_ASSET(2) char sSandXlu[] = "__OTR__objects/object_nei_wand_sand_rod/gNeiSandRodXluDL";
static const ALIGN_ASSET(2) char sTornadoOpa[] = "__OTR__objects/object_nei_wand_tornado_rod/gNeiTornadoRodDL";
static const ALIGN_ASSET(2) char sTornadoXlu[] = "__OTR__objects/object_nei_wand_tornado_rod/gNeiTornadoRodXluDL";
static const ALIGN_ASSET(2) char sWaterOpa[] = "__OTR__objects/object_nei_wand_water_rod/gNeiWaterRodDL";
static const ALIGN_ASSET(2) char sWaterXlu[] = "__OTR__objects/object_nei_wand_water_rod/gNeiWaterRodXluDL";
static const ALIGN_ASSET(2) char sMeteorOpa[] = "__OTR__objects/object_nei_wand_meteor_rod/gNeiMeteorRodDL";
static const ALIGN_ASSET(2) char sMeteorXlu[] = "__OTR__objects/object_nei_wand_meteor_rod/gNeiMeteorRodXluDL";
static const ALIGN_ASSET(2) char sStormOpa[] = "__OTR__objects/object_nei_wand_storm_rod/gNeiStormRodDL";
static const ALIGN_ASSET(2) char sStormXlu[] = "__OTR__objects/object_nei_wand_storm_rod/gNeiStormRodXluDL";
static const ALIGN_ASSET(2) char sScepterOpa[] = "__OTR__objects/object_nei_wand_shadow_scepter/gNeiShadowScepterDL";
static const ALIGN_ASSET(2) char sScepterXlu[] = "__OTR__objects/object_nei_wand_shadow_scepter/gNeiShadowScepterXluDL";

static const ALIGN_ASSET(2) char sTornadoMaterialDL[] = "__OTR__objects/object_nei_tornado/mat_tornado_f3dlite_tornado";
static const ALIGN_ASSET(2) char sTornadoMeshDL[] = "__OTR__objects/object_nei_tornado/tornado_mesh_tri_0";
static const ALIGN_ASSET(2) char sSlabDL[] = "__OTR__objects/object_d_lift/gCollapsingPlatformDL";
static const ALIGN_ASSET(2) char sSwingForeAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_fighter_Lside_kiru";
static const ALIGN_ASSET(2) char sSwingBackAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_fighter_Rside_kiru";
static const ALIGN_ASSET(2) char sSwimAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_swimer_swim";
static const ALIGN_ASSET(2) char sLandingRollAnim[] =
    "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_landing_roll";

// The Boe of Majora's Mask, which the Shadow Scepter throws. It only draws when mm.o2r is mounted; the bolt
// flies and stuns either way, which is what NEI does.
static const ALIGN_ASSET(2) char sBoeMaterialDL[] = "__OTR__objects/object_mkk/gBlackBoeBodyMaterialDL";
static const ALIGN_ASSET(2) char sBoeModelDL[] = "__OTR__objects/object_mkk/gBlackBoeBodyModelDL";
static const ALIGN_ASSET(2) char sBoeEndDL[] = "__OTR__objects/object_mkk/gBlackBoeEndDL";
static const ALIGN_ASSET(2) char sBoeEyesDL[] = "__OTR__objects/object_mkk/gBlackBoeEyesDL";

// Sand and Water undercut the rest because they are movement and get spammed; Storm costs the most because
// it fires world flags. The Tornado is 0 on purpose: it bills itself while the column climbs.
static const WandRod sRods[WAND_MODE_MAX] = {
    { sSandOpa, sSandXlu, QUEST_MEDALLION_SPIRIT, 2 },  { sTornadoOpa, sTornadoXlu, QUEST_MEDALLION_FOREST, 0 },
    { sWaterOpa, sWaterXlu, QUEST_MEDALLION_WATER, 2 }, { sMeteorOpa, sMeteorXlu, QUEST_MEDALLION_FIRE, 3 },
    { sStormOpa, sStormXlu, QUEST_MEDALLION_LIGHT, 6 }, { sScepterOpa, sScepterXlu, QUEST_MEDALLION_SHADOW, 3 },
};

static const Z64WheelEntry sWheelEntries[WAND_MODE_MAX] = {
    { sIconSand, "Sand Rod" },     { sIconTornado, "Tornado Rod" }, { sIconWater, "Water Rod" },
    { sIconMeteor, "Meteor Rod" }, { sIconStorm, "Storm Rod" },     { sIconScepter, "Shadow Scepter" },
};

static const char* const sShuffleValues[] = { "Medallions", "One item", "A rod per pickup" };

static const u16 sItemButtons[WAND_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                     BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDrawEnd",       "OnSceneInit",
                                              "OnLoadFile",     "OnPlayerPostLimbDraw", "OnInterfaceDrawEnd" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

// ---- Sand Rod ---------------------------------------------------------------------------------------------

#define SAND_MAX_SLABS 8
#define SAND_SLAB_SCALE 0.05f
#define SAND_HOLD_INTERVAL 6
// Under 1.0 so consecutive slabs overlap instead of leaving a seam to fall through.
#define SAND_STEP_FRACTION 0.85f
#define SAND_FALLBACK_DIST 40.0f
#define SAND_CRUMBLE_FRAMES 24
#define SAND_DUST_EVERY 4
#define SAND_SFX_TICKS 20
#define SAND_ENV_R 214
#define SAND_ENV_G 178
#define SAND_ENV_B 112
// ObjLift reads a scene switch flag out of (params >> 2) & 0x3F, and with params 0 that is flag 0.
#define SAND_SWITCH_FLAG 0
// The crumble countdown rides on home.rot.x: once the update below is installed no ObjLift code ever runs on
// this actor again except its destroy, which only reads dyna.bgId.
#define SAND_CRUMBLE(actor) ((actor)->home.rot.x)

// ---- Tornado Rod ------------------------------------------------------------------------------------------

// The player update runs at 20 Hz, so a full column is three seconds.
#define WIND_CHARGE_MIN 15
#define WIND_CHARGE_FULL 60
#define WIND_LAUNCH_MIN 8.0f
#define WIND_LAUNCH_MAX 32.0f
// Eight ticks fall inside the climb, so a full gale costs exactly 8 and a partial one costs its share.
#define WIND_DRAIN_INTERVAL 7
#define WIND_FLUTTER_INTERVAL 4
#define WIND_BOOST 1.55f
#define WIND_BOOST_FRAMES 12
#define WIND_PITCH_UP (-0x4000)
#define WIND_HEIGHT 82.0f
#define WIND_RADIUS 30.0f
#define WIND_SPIN 0x0900
#define WIND_ALPHA 70
#define WIND_GROW_MIN 0.35f
#define WIND_FADE_FRAMES 14.0f
#define WIND_FLAME_MAX 8
#define WIND_FLAME_KEEP 4
#define WIND_FLAME_BURST_FRAMES 7
#define WIND_FLAME_RADIUS 17.0f
#define WIND_FLAME_BURST_RADIUS 40.0f
#define WIND_FLAME_HEIGHT 12.0f
#define WIND_FLAME_DRIFT 0x0700
// Index 6 of En_Light's table is the green flame, and it must be POSITIVE: bit 15 is the small candle, whose
// draw hardcodes orange, and an even index also skips its Y flip.
#define WIND_LIGHT_PARAMS 6
#define WIND_LIGHT_SCALE 0.0010f
#define WIND_QUAKE_SPEED 32000
#define WIND_QUAKE_AMPLITUDE 6
#define WIND_QUAKE_FRAMES 11
// A continuous id, only ever played MINUS the flag and re-issued every frame, which is also what stops it.
#define WIND_LOOP_SFX (NA_SE_EV_WIND_TRAP - SFX_FLAG)
#define WIND_RIBBONS 5
#define WIND_RIBBON_SPREAD 2.0f
#define WIND_RIBBON_LIFE 8
#define WIND_RIBBON_STEP 0.06f
#define WIND_RIBBON_TURNS 0x18000

// Link's ~60 units are about 1.6 m and the update runs at 20 Hz, so the gale is roughly twelve metres of
// ground: a long jump rather than a flight.
#define GALE_ANTICIPATE_FRAMES 2
#define GALE_IMPULSE_FRAMES 2
#define GALE_FLIGHT_FRAMES 18
#define GALE_SPEED_XZ 26.0f
#define GALE_RISE 12.0f
#define GALE_GRAVITY (-1.6f)
#define GALE_ROLL_FRAMES 10
#define GALE_LIFTOFF_FRAMES 3
#define GALE_COST 4
#define GALE_FALL_LIMIT 60
#define GALE_TURN_RATE 0x800
#define GALE_QUAKE_SPEED 25000
#define GALE_QUAKE_AMPLITUDE 8
#define GALE_QUAKE_FRAMES 9
#define GALE_VANILLA_GRAVITY (-1.2f)

typedef enum {
    GALE_OFF,
    GALE_ANTICIPATE,
    GALE_IMPULSE,
    GALE_FLIGHT,
    GALE_FALL,
    GALE_ROLL,
} WandGalePhase;

// ---- Water Rod --------------------------------------------------------------------------------------------

#define WATER_SPAWN_DIST 70.0f
#define WATER_RIDE_HEIGHT 240.0f
#define WATER_REST_HEIGHT 10.0f
// shape.rot.x is read as `maxHeight = rot.x * 40.0f` before Init zeroes the rotation.
#define WATER_MAX_HEIGHT_ROT 6
#define WATER_STEP_SCALE 0.8f
#define WATER_STEP_MAX 3.0f
#define WATER_STEP_MIN 0.01f
// The type nibble in params: anything but 0 or 1 makes Init kill the actor.
#define WATER_PARAMS_RAISING 0x0000

// ---- Meteor Rod -------------------------------------------------------------------------------------------

#define METEOR_SPAWN_DIST 30.0f
#define METEOR_SPAWN_HEIGHT 28.0f
#define METEOR_LAUNCH_SPEED 28.0f
#define METEOR_LAUNCH_RISE 6.0f
#define METEOR_FUSE_FRAMES 170
// Nothing detonates it for these first frames: thrown from hand height it is still inside Link's own space.
#define METEOR_ARM_FRAMES 5
// The AT collider only exists while the bomb is ALREADY exploding, so enemy contact is proximity.
#define METEOR_ENEMY_TRIGGER_RADIUS 42.0f
#define METEOR_BOMB_SCALE 0.01f
// Peak and speed both fall as 1/(1 + k*n): slow, where vanilla's restitution dies in two hops.
#define METEOR_HOP_PERIOD 16.0f
#define METEOR_HOP_HEIGHT 46.0f
#define METEOR_HOP_DECAY 0.3f
#define METEOR_STEER_RATE 0x900
#define METEOR_TINT_R 235
#define METEOR_TINT_G 45
#define METEOR_TINT_B 30
#define METEOR_TINT_MIX 255
// Both on home.rot, which En_Bom never touches. Hop time is AIRBORNE until the throw lands.
#define METEOR_HOP_TIME(actor) ((actor)->home.rot.x)
#define METEOR_AGE(actor) ((actor)->home.rot.z)
#define METEOR_AIRBORNE (-1)

// ---- Storm Rod --------------------------------------------------------------------------------------------

// Oceff_Storm is deliberately NOT spawned alongside the weather: its Destroy calls Magic_Reset, which would
// wipe the very meter this rod was just charged against.
#define STORM_OKARINA_PARAMS 1
#define STORM_SPAWN_Y_OFFSET (-30.0f)
#define STORM_RAY_SPEED 18.0f
#define STORM_RAY_LIFE 40
#define STORM_RAY_SPAWN_HEIGHT 30.0f
#define STORM_RAY_RADIUS 22
#define STORM_RAY_HEIGHT 30
#define STORM_RAY_DAMAGE 2
#define STORM_BOLT_SCALE 210
#define STORM_BOLT_LIFE 6
#define STORM_BOLT_BRANCHES 2
#define STORM_BOLT_EVERY 2
// The effect's yaw is a SCREEN-SPACE roll applied after billboarding, not a world direction, so the ray's own
// yaw left the sprite near upright. 0x4000 lays it along the ray instead.
#define STORM_BOLT_ROLL 0x4000
#define STORM_BOLT_ROLL_JITTER 4096.0f

// ---- Shadow Scepter ---------------------------------------------------------------------------------------

#define SHADOW_SEEK_RANGE 460.0f
#define SHADOW_SPAWN_DIST 30.0f
#define SHADOW_SPAWN_HEIGHT 25.0f
#define SHADOW_SPEED 11.0f
#define SHADOW_HIT_RADIUS 22.0f
#define SHADOW_LIFE_FRAMES 90
#define SHADOW_STUN_FRAMES 120
#define SHADOW_SCALE 0.014f
// SoH carries no COLORFILTER_* names: 0x0000 blue, 0x4000 red, 0x8000 grey.
#define SHADOW_FILTER_BLUE 0x0000
#define SHADOW_FILTER_STRENGTH 0xF8

static const u8 sEnemyCategories[2] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };

// ---- state ------------------------------------------------------------------------------------------------

static const SOHModApi* sApi;
static bool sIsWandOut;
static bool sIsPressPending;
static bool sWasWandOut;
static MtxF sHandMatrix;
static bool sHasHandMatrix;
static s16 sLastScene = -1;

static u8 sSwingClip;
static s16 sSwingFrame;
static u8 sSwingMode;
static bool sHasSwingFired;

static Actor* sSandSlabs[SAND_MAX_SLABS];
static u8 sSandNextSlot;
static s16 sSandHoldTimer;
static f32 sSandTopOffset;
static f32 sSandReach;
static bool sIsSandMeasured;

static Actor* sWindFlames[WIND_FLAME_MAX];
static f32 sWindFlamePhase[WIND_FLAME_MAX];
static s32 sWindRibbons[WIND_RIBBONS];
static f32 sWindRibbonPhase[WIND_RIBBONS];
static bool sAreWindRibbonsLive;
static Vec3f sWindOrigin;
static f32 sWindFade;
static f32 sWindGrow;
static s32 sWindAge;
static s16 sWindSpin;
static bool sIsWindUp;
static s16 sWindCharge;
static s16 sWindDrainTimer;
static s16 sWindBoostTimer;
static bool sIsWindCharging;

static u8 sGalePhase;
static s16 sGaleTimer;
static s16 sGaleYaw;

static Actor* sWaterGeyser;

static ActorFunc sMeteorBombUpdate;
static ActorFunc sMeteorBombDraw;

static struct {
    Vec3f pos;
    Vec3f vel;
    s16 life;
    bool isActive;
} sStormRay;
static ColliderCylinder sStormRayCollider;
static bool sIsStormColliderReady;

static struct {
    Vec3f pos;
    Actor* target;
    s16 yaw;
    s16 life;
    bool isActive;
} sShadowBolt;

static Color_RGBA8 sStormBoltPrim = { 255, 255, 255, 255 };
static Color_RGBA8 sStormBoltEnv = { 255, 255, 60, 255 };
static Color_RGBA8 sSandDust = { SAND_ENV_R, SAND_ENV_G, SAND_ENV_B, 255 };
static Color_RGBA8 sGaleDust = { 200, 210, 190, 255 };

static ColliderCylinderInit sStormRayColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_BOOMERANG, 0x00, STORM_RAY_DAMAGE },
      { 0, 0, 0 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { STORM_RAY_RADIUS, STORM_RAY_HEIGHT, 0, { 0, 0, 0 } },
};

int32_t Player_IsZTargeting(Player* player);
void Player_ZeroSpeedXZ(Player* player);
void Player_AnimPlayLoop(PlayState* play, Player* player, LinkAnimationHeader* anim);
LinkAnimationHeader* Player_GetIdleAnim(Player* player);
void func_800F436C(Vec3f* pos, u16 sfxId, f32 arg2);
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);
void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);

static void ChargeWindAction(Player* player, PlayState* play);
static void RideGaleAction(Player* player, PlayState* play);

// ---- shared helpers ---------------------------------------------------------------------------------------

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static u8 CurrentMode(void) {
    uint32_t selection = Z64Wheel_GetSelection();

    return selection < WAND_MODE_MAX ? (u8)selection : WAND_SAND;
}

static u8 ShuffleRule(void) {
    return SOH_MOD_API_HAS(sApi, GetRandoOption) ? sApi->GetRandoOption(WAND_SHUFFLE_OPTION) : WAND_SHUFFLE_MEDALLIONS;
}

static u8 OwnedRodBits(void) {
    u8 rods = 0;

    sApi->StorageGet(WAND_KEY, WAND_RODS_FIELD, &rods, sizeof(rods));
    return rods;
}

// Which rods answer. The three treatments differ only in what lights an entry, never in what the staff is.
static void SyncUnlockedRods(void) {
    u8 rule = ShuffleRule();
    u8 rods = rule == WAND_SHUFFLE_RODS ? OwnedRodBits() : 0;
    bool hasStaff = rule == WAND_SHUFFLE_SINGLE && sApi->IsCustomItemOwned(WAND_KEY);

    for (u8 mode = 0; mode < WAND_MODE_MAX; mode++) {
        bool isOwned;

        switch (rule) {
            case WAND_SHUFFLE_SINGLE:
                isOwned = hasStaff;
                break;
            case WAND_SHUFFLE_RODS:
                isOwned = (rods & (1 << mode)) != 0;
                break;
            default:
                isOwned = CHECK_QUEST_ITEM(sRods[mode].questItem) != 0;
                break;
        }
        Z64Wheel_SetEntryUnlocked(mode, isOwned);
    }
}

// Every pickup of the staff lights the next rod in order, which is what "a rod per pickup" means until the
// randomizer can place a mod's items separately.
static void LightNextRod(const char* key) {
    u8 rods = OwnedRodBits();

    for (u8 mode = 0; mode < WAND_MODE_MAX; mode++) {
        if ((rods & (1 << mode)) == 0) {
            rods |= 1 << mode;
            break;
        }
    }
    sApi->StorageSet(WAND_KEY, WAND_RODS_FIELD, &rods, sizeof(rods));
    SyncUnlockedRods();
}

static u16 FindButtonMask(void) {
    for (u8 button = 0; button < WAND_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);

        if (equipped != NULL && strcmp(equipped, WAND_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool IsButtonHeld(PlayState* play) {
    u16 mask = FindButtonMask();

    return mask != 0 && (play->state.input[0].cur.button & mask) != 0;
}

static bool HasLockOn(Player* player) {
    return Player_IsZTargeting(player) && player->focusActor != NULL && player->focusActor->update != NULL;
}

// Failing to pay costs nothing else: the rod that declined reports it and the swing is never started.
static bool SpendMagic(PlayState* play, s16 cost) {
    if (cost <= 0) {
        return true;
    }
    if (!Magic_RequestChange(play, cost, MAGIC_CONSUME_NOW)) {
        return false;
    }
    // Consuming leaves the meter flashing and refusing every later request until the spell resets it.
    Magic_Reset(play);
    return true;
}

static bool HasMagic(PlayState* play, s16 cost) {
    return cost <= 0 || gSaveContext.magic >= cost;
}

static Actor* FindNearestEnemy(PlayState* play, Vec3f* pos, f32 range) {
    f32 rangeSq = SQ(range);

    for (u8 index = 0; index < ARRAY_COUNT(sEnemyCategories); index++) {
        Actor* actor = play->actorCtx.actorLists[sEnemyCategories[index]].head;

        while (actor != NULL) {
            if (actor->update != NULL) {
                f32 distanceSq =
                    SQ(actor->world.pos.x - pos->x) + SQ(actor->world.pos.y - pos->y) + SQ(actor->world.pos.z - pos->z);

                if (distanceSq < rangeSq) {
                    return actor;
                }
            }
            actor = actor->next;
        }
    }
    return NULL;
}

static void SpawnDust(PlayState* play, Vec3f* center, f32 minRadius, f32 maxRadius, u8 count, Color_RGBA8* color) {
    Vec3f velocity = { 0.0f, 0.6f, 0.0f };
    Vec3f accel = { 0.0f, -0.02f, 0.0f };
    Color_RGBA8 fade = { color->r, color->g, color->b, 0 };

    for (u8 index = 0; index < count; index++) {
        f32 angle = Rand_ZeroOne() * (2.0f * M_PI);
        f32 radius = minRadius + Rand_ZeroOne() * (maxRadius - minRadius);
        Vec3f pos = { center->x + cosf(angle) * radius, center->y, center->z + sinf(angle) * radius };

        func_8002829C(play, &pos, &velocity, &accel, color, &fade, 60, 12);
    }
}

// ---- Sand Rod ---------------------------------------------------------------------------------------------

// The slab is MEASURED off its own registered collision: world units from the actor origin up to the surface
// Link stands on, and out to its nearest edge. Both are constant, so the first slab pays for it.
static void MeasureSlab(PlayState* play, Actor* slab) {
    s32 bgId = ((DynaPolyActor*)slab)->bgId;
    CollisionHeader* header;
    f32 halfX;
    f32 halfZ;

    if (sIsSandMeasured || bgId < 0 || bgId >= play->colCtx.dyna.bgActorMax) {
        return;
    }
    header = play->colCtx.dyna.bgActors[bgId].colHeader;
    if (header == NULL) {
        return;
    }
    halfX = (f32)(header->maxBounds.x - header->minBounds.x) * 0.5f * SAND_SLAB_SCALE;
    halfZ = (f32)(header->maxBounds.z - header->minBounds.z) * 0.5f * SAND_SLAB_SCALE;
    sSandTopOffset = (f32)header->maxBounds.y * SAND_SLAB_SCALE;
    sSandReach = halfX < halfZ ? halfX : halfZ;
    sIsSandMeasured = true;
}

// One step ahead of Link, with its walking surface at his feet. The player's own origin sits on the floor,
// which is why the slab is dropped by its top offset rather than by a number picked to look right.
static void NextSlabSpot(Player* player, Vec3f* out) {
    s16 yaw = player->actor.shape.rot.y;
    f32 distance = sIsSandMeasured ? sSandReach * SAND_STEP_FRACTION : SAND_FALLBACK_DIST;

    out->x = player->actor.world.pos.x + Math_SinS(yaw) * distance;
    out->y = player->actor.world.pos.y - sSandTopOffset;
    out->z = player->actor.world.pos.z + Math_CosS(yaw) * distance;
}

// A slab that has started crumbling does not count: it is on its way out.
static bool IsSpotCovered(Vec3f* spot) {
    for (u8 index = 0; index < SAND_MAX_SLABS; index++) {
        Actor* slab = sSandSlabs[index];

        if (slab == NULL || SAND_CRUMBLE(slab) != 0) {
            continue;
        }
        if (SQ(slab->world.pos.x - spot->x) + SQ(slab->world.pos.z - spot->z) < SQ(sSandReach)) {
            return true;
        }
    }
    return false;
}

static void ForgetSlab(Actor* slab) {
    for (u8 index = 0; index < SAND_MAX_SLABS; index++) {
        if (sSandSlabs[index] == slab) {
            sSandSlabs[index] = NULL;
        }
    }
}

/**
 * The slab's whole life. No shake and no fall: standing on it starts a countdown, and the countdown eats it.
 *
 * The countdown never touches actor->scale, only the draw — the dynapoly is built from the actor's transform,
 * so scaling the actor would shrink the floor out from under Link while he is still standing on it.
 */
static void UpdateSlab(Actor* thisx, PlayState* play) {
    if (SAND_CRUMBLE(thisx) == 0) {
        if (!DynaPolyActor_IsPlayerOnTop((DynaPolyActor*)thisx)) {
            return;
        }
        SAND_CRUMBLE(thisx) = SAND_CRUMBLE_FRAMES;
        // A fixed world position, not the actor's: the slab is killed partway through the sound.
        SoundSource_PlaySfxAtFixedWorldPos(play, &thisx->world.pos, SAND_SFX_TICKS, NA_SE_EV_FALL_DOWN_DIRT);
    }
    SAND_CRUMBLE(thisx)--;
    if ((SAND_CRUMBLE(thisx) % SAND_DUST_EVERY) == 0) {
        SpawnDust(play, &thisx->world.pos, 10.0f, 40.0f, 3, &sSandDust);
    }
    if (SAND_CRUMBLE(thisx) <= 0) {
        ForgetSlab(thisx);
        Actor_Kill(thisx);
    }
}

static void DrawSlab(Actor* thisx, PlayState* play) {
    f32 remaining = SAND_CRUMBLE(thisx) <= 0 ? 1.0f : (f32)SAND_CRUMBLE(thisx) / (f32)SAND_CRUMBLE_FRAMES;

    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, SAND_ENV_R, SAND_ENV_G, SAND_ENV_B, 255);
    CLOSE_DISPS(play->state.gfxCtx);

    // Applied to the matrix Actor_Draw already set up: this shrinks the model and leaves the collision alone.
    Matrix_Scale(remaining, remaining, remaining, MTXMODE_APPLY);
    Gfx_DrawDListOpa(play, (Gfx*)sSlabDL);
}

static void ForgetSandSlabs(void) {
    for (u8 index = 0; index < SAND_MAX_SLABS; index++) {
        sSandSlabs[index] = NULL;
    }
    sSandNextSlot = 0;
    sSandHoldTimer = 0;
}

// Overflow only: the oldest is told to start crumbling and then let go of, so a held button cannot fill the
// actor list. It finishes on its own; the countdown lives on the actor.
static void EvictSlab(u8 slot) {
    Actor* slab = sSandSlabs[slot];

    if (slab != NULL && SAND_CRUMBLE(slab) == 0) {
        SAND_CRUMBLE(slab) = SAND_CRUMBLE_FRAMES;
    }
    sSandSlabs[slot] = NULL;
}

static bool CastSand(Player* player, PlayState* play) {
    Vec3f pos;
    s16 yaw = player->actor.shape.rot.y;
    Actor* slab;
    bool wasFlagSet;

    if (Object_GetIndex(&play->objectCtx, OBJECT_D_LIFT) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_D_LIFT);
        return false;
    }
    // Placed blind, with no raycast on purpose: that is what lets a slab hang in mid-air over a gap.
    NextSlabSpot(player, &pos);

    // ObjLift_Init kills itself when the flag is already set, and nothing runs between these two lines.
    wasFlagSet = Flags_GetSwitch(play, SAND_SWITCH_FLAG) != 0;
    if (wasFlagSet) {
        Flags_UnsetSwitch(play, SAND_SWITCH_FLAG);
    }
    slab = Actor_Spawn(&play->actorCtx, play, ACTOR_OBJ_LIFT, pos.x, pos.y, pos.z, 0, yaw, 0, 0);
    if (wasFlagSet) {
        Flags_SetSwitch(play, SAND_SWITCH_FLAG);
    }
    if (slab == NULL || slab->update == NULL) {
        return false;
    }

    // destroy is left alone: ObjLift_Destroy is what unregisters the dynapoly.
    slab->update = UpdateSlab;
    slab->draw = DrawSlab;
    Actor_SetScale(slab, SAND_SLAB_SCALE);
    slab->room = -1;
    SAND_CRUMBLE(slab) = 0;

    // The collision only exists once Init has registered it, so the first slab is the one that can be
    // measured, and once measured it is moved to where it should have gone.
    MeasureSlab(play, slab);
    if (sIsSandMeasured) {
        NextSlabSpot(player, &pos);
        slab->world.pos = pos;
    }
    EvictSlab(sSandNextSlot);
    sSandSlabs[sSandNextSlot] = slab;
    sSandNextSlot = (u8)((sSandNextSlot + 1) % SAND_MAX_SLABS);

    SpawnDust(play, &pos, 8.0f, 30.0f, 4, &sSandDust);
    SoundSource_PlaySfxAtFixedWorldPos(play, &pos, SAND_SFX_TICKS, NA_SE_EV_LAND_DIRT);
    return true;
}

// What makes the rod a road: it only lays a slab when the step ahead has nothing standable under it, so
// holding the button and walking keeps producing ground, and standing still stops costing magic.
static bool ShouldLaySlab(Player* player, bool isHeld) {
    Vec3f spot;

    if (!isHeld) {
        sSandHoldTimer = 0;
        return false;
    }
    if (--sSandHoldTimer > 0) {
        return false;
    }
    sSandHoldTimer = SAND_HOLD_INTERVAL;
    NextSlabSpot(player, &spot);
    return !IsSpotCovered(&spot);
}

// ---- Tornado Rod: the updraft -------------------------------------------------------------------------------

// Frozen on purpose: EnLight_Update is the only thing that plays its torch sound and the only thing that
// restores the light's radius each frame, so replacing it keeps these motes silent and dark.
static void KeepFlameStill(Actor* thisx, PlayState* play) {
}

static Actor* SpawnFlame(PlayState* play, Vec3f* origin) {
    Actor* flame =
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, origin->x, origin->y, origin->z, 0, 0, 0, WIND_LIGHT_PARAMS);

    if (flame != NULL) {
        Actor_SetScale(flame, WIND_LIGHT_SCALE);
        // Re-seated as a no-glow point of radius 0: EnLight_Init made it a glowing one, which is the wide
        // green disc on the floor. The node still points here, so EnLight_Destroy has something valid.
        Lights_PointNoGlowSetInfo(&((EnLight*)flame)->lightInfo, (s16)origin->x, (s16)origin->y, (s16)origin->z, 0, 0,
                                  0, 0);
        flame->update = KeepFlameStill;
    }
    return flame;
}

static void StopWindRibbons(PlayState* play) {
    if (!sAreWindRibbonsLive) {
        return;
    }
    for (u8 index = 0; index < WIND_RIBBONS; index++) {
        if (sWindRibbons[index] >= 0) {
            Effect_Delete(play, sWindRibbons[index]);
            sWindRibbons[index] = -1;
        }
    }
    sAreWindRibbonsLive = false;
}

static void StartWindRibbons(PlayState* play) {
    EffectBlureInit1 init;

    StopWindRibbons(play);
    init.p1StartColor[0] = 190;
    init.p1StartColor[1] = 255;
    init.p1StartColor[2] = 200;
    init.p1StartColor[3] = 255;
    init.p2StartColor[0] = 120;
    init.p2StartColor[1] = 220;
    init.p2StartColor[2] = 150;
    init.p2StartColor[3] = 0;
    init.p1EndColor[0] = 190;
    init.p1EndColor[1] = 255;
    init.p1EndColor[2] = 200;
    init.p1EndColor[3] = 0;
    init.p2EndColor[0] = 120;
    init.p2EndColor[1] = 220;
    init.p2EndColor[2] = 150;
    init.p2EndColor[3] = 0;
    init.elemDuration = WIND_RIBBON_LIFE;
    init.unkFlag = 0;
    init.calcMode = 2;

    for (u8 index = 0; index < WIND_RIBBONS; index++) {
        sWindRibbons[index] = -1;
        Effect_Add(play, &sWindRibbons[index], EFFECT_BLURE1, 0, 0, &init);
        sWindRibbonPhase[index] = (f32)index / (f32)WIND_RIBBONS;
    }
    sAreWindRibbonsLive = true;
}

// Each streak climbs the column's surface, flaring out as it turns around it.
static void FeedWindRibbons(PlayState* play) {
    f32 reach = WIND_HEIGHT * sWindGrow;
    f32 spread = WIND_RADIUS * sWindGrow * WIND_RIBBON_SPREAD;
    f32 width = spread * 0.16f + 3.0f;

    if (!sAreWindRibbonsLive) {
        StartWindRibbons(play);
    }
    for (u8 index = 0; index < WIND_RIBBONS; index++) {
        EffectBlure* ribbon = Effect_GetByIndex(sWindRibbons[index]);
        f32 phase = sWindRibbonPhase[index];
        f32 along = reach * phase;
        f32 flare = spread * phase * phase;
        s16 turn = (s16)((s32)(sWindSpin) + (s32)(index * (0x10000 / WIND_RIBBONS)) + (s32)(phase * WIND_RIBBON_TURNS));
        Vec3f base = { sWindOrigin.x + Math_CosS(turn) * flare, sWindOrigin.y + along,
                       sWindOrigin.z + Math_SinS(turn) * flare };
        Vec3f tip = { base.x, base.y + width, base.z };
        f32 nextPhase = phase + WIND_RIBBON_STEP;

        if (ribbon != NULL) {
            EffectBlure_AddVertex(ribbon, &tip, &base);
        }
        sWindRibbonPhase[index] = nextPhase >= 1.0f ? nextPhase - 1.0f : nextPhase;
    }
}

static void StopUpdraft(PlayState* play) {
    if (!sIsWindUp) {
        return;
    }
    sIsWindUp = false;
    StopWindRibbons(play);
    // En_Light has no lifetime of its own, so whoever spawns one owns it until they say otherwise.
    for (u8 index = 0; index < WIND_FLAME_MAX; index++) {
        if (sWindFlames[index] != NULL) {
            Actor_Kill(sWindFlames[index]);
            sWindFlames[index] = NULL;
        }
    }
}

// The scene already destroyed every actor and effect: killing a pointer into its arena is how that becomes a
// crash, so the pointers are dropped rather than freed.
static void ForgetUpdraft(void) {
    memset(sWindFlames, 0, sizeof(sWindFlames));
    memset(sWindRibbons, 0, sizeof(sWindRibbons));
    sAreWindRibbonsLive = false;
    sIsWindUp = false;
    sWindFade = 0.0f;
    sWindGrow = WIND_GROW_MIN;
    sWindAge = 0;
}

static void StartUpdraft(PlayState* play, Vec3f* origin) {
    s16 quake;

    if (sIsWindUp) {
        return;
    }
    sWindAge = 0;
    sIsWindUp = true;
    sWindFade = 1.0f;
    sWindGrow = WIND_GROW_MIN;
    sWindOrigin = *origin;

    // Everything erupts at once; the tick culls it back when the burst is over.
    for (u8 index = 0; index < WIND_FLAME_MAX; index++) {
        sWindFlamePhase[index] = (f32)index * (2.0f * M_PI / WIND_FLAME_MAX) + Rand_ZeroFloat(0.6f);
        sWindFlames[index] = SpawnFlame(play, origin);
    }
    quake = Quake_Add(Play_GetCamera(play, 0), 3);
    Quake_SetSpeed(quake, WIND_QUAKE_SPEED);
    Quake_SetQuakeValues(quake, WIND_QUAKE_AMPLITUDE, 0, 0, 0);
    Quake_SetCountdown(quake, WIND_QUAKE_FRAMES);
}

static void TickUpdraft(PlayState* play, bool isHolding, f32 fill) {
    bool isBursting;

    if (!sIsWindUp) {
        return;
    }
    isBursting = sWindAge < WIND_FLAME_BURST_FRAMES;
    sWindAge++;

    // The wind only holds while it is being built; let go and what is left dies away instead of cutting out.
    if (isHolding) {
        sWindFade = 1.0f;
        sWindGrow = WIND_GROW_MIN + (1.0f - WIND_GROW_MIN) * CLAMP(fill, 0.0f, 1.0f);
    } else {
        sWindFade -= 1.0f / WIND_FADE_FRAMES;
        if (sWindFade <= 0.0f) {
            StopUpdraft(play);
            return;
        }
    }
    sWindSpin += WIND_SPIN;
    Audio_PlaySoundGeneral(WIND_LOOP_SFX, &sWindOrigin, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
    FeedWindRibbons(play);

    for (u8 index = 0; index < WIND_FLAME_MAX; index++) {
        f32 angle = sWindFlamePhase[index] + BINANG_TO_RAD(WIND_FLAME_DRIFT) * sWindAge;
        f32 radius = isBursting ? WIND_FLAME_BURST_RADIUS : WIND_FLAME_RADIUS;

        if (sWindFlames[index] == NULL) {
            continue;
        }
        // Once the burst is spent only a few stay, gathered in close around the column.
        if (!isBursting && index >= WIND_FLAME_KEEP) {
            Actor_Kill(sWindFlames[index]);
            sWindFlames[index] = NULL;
            continue;
        }
        sWindFlames[index]->world.pos = sWindOrigin;
        sWindFlames[index]->world.pos.x += Math_SinF(angle) * radius;
        sWindFlames[index]->world.pos.z += Math_CosF(angle) * radius;
        sWindFlames[index]->world.pos.y += WIND_FLAME_HEIGHT;
        Actor_SetScale(sWindFlames[index], WIND_LIGHT_SCALE * sWindFade);
    }
}

// The cone is modelled standing on its base along +Y, which is where the column wants it anyway.
static void DrawUpdraft(PlayState* play) {
    f32 height = WIND_HEIGHT * sWindGrow;
    f32 spread = WIND_RADIUS * sWindGrow;

    if (!sIsWindUp) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoMaterialDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 190, 255, 200, (u8)(WIND_ALPHA * sWindFade));
    Matrix_Translate(sWindOrigin.x, sWindOrigin.y, sWindOrigin.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(sWindSpin), MTXMODE_APPLY);
    Matrix_Scale(spread / 47.0f, height / 100.0f, spread / 47.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoMeshDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- Tornado Rod: riding it ---------------------------------------------------------------------------------

static void EndWindCharge(Player* player, PlayState* play) {
    sIsWindCharging = false;
    sWindCharge = 0;
    if (player->actionFunc == ChargeWindAction) {
        func_80839FFC(player, play);
    }
}

// The charge decides the height; the boost below then keeps the next hop or slash strong.
static void LaunchOnColumn(PlayState* play, Player* player) {
    f32 fill = (f32)sWindCharge / (f32)WIND_CHARGE_FULL;

    player->actor.velocity.y = WIND_LAUNCH_MIN + (WIND_LAUNCH_MAX - WIND_LAUNCH_MIN) * CLAMP(fill, 0.0f, 1.0f);
    player->actor.bgCheckFlags &= ~1;
    player->stateFlags1 |= PLAYER_STATE1_JUMPING;
    player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
    sWindBoostTimer = WIND_BOOST_FRAMES;
    Player_PlaySfx(&player->actor, NA_SE_PL_ROLL);
}

/**
 * Rooted while the column builds. An action func of its own rather than a paused one: anything that takes
 * the player — damage, a cutscene, a textbox — replaces it, and the charge notices next frame that it is no
 * longer the one driving.
 */
static void ChargeWindAction(Player* player, PlayState* play) {
    Player_ZeroSpeedXZ(player);
    Player_AnimPlayLoop(play, player, Player_GetIdleAnim(player));
}

static bool StartWindCharge(Player* player, PlayState* play) {
    if (sIsWindCharging || !(player->actor.bgCheckFlags & 1)) {
        return false;
    }
    sIsWindCharging = true;
    sWindCharge = 0;
    sWindDrainTimer = WIND_DRAIN_INTERVAL;
    StartUpdraft(play, &player->actor.world.pos);
    Player_SetupAction(play, player, ChargeWindAction, 0);
    return true;
}

static void TickWindCharge(PlayState* play, Player* player, bool isHeld) {
    if (sWindBoostTimer > 0) {
        sWindBoostTimer--;
    }
    if (!sIsWindCharging) {
        TickUpdraft(play, false, 0.0f);
        return;
    }
    // Anything that took the stance away drops the charge without launching him.
    if (player->actionFunc != ChargeWindAction || !(player->actor.bgCheckFlags & 1)) {
        EndWindCharge(player, play);
        TickUpdraft(play, false, 0.0f);
        return;
    }

    // Only the CLIMB is billed and only the climb flutters: sitting on a full column is silent and free, so
    // holding it is a question of when to leave, never of how much magic is left.
    if (sWindCharge < WIND_CHARGE_FULL) {
        sWindCharge++;
        if ((sWindCharge % WIND_FLUTTER_INTERVAL) == 0) {
            Player_PlaySfx(&player->actor, NA_SE_EN_KAICHO_FLUTTER);
        }
        if (--sWindDrainTimer <= 0) {
            sWindDrainTimer = WIND_DRAIN_INTERVAL;
            if (!SpendMagic(play, 1)) {
                EndWindCharge(player, play); // ran dry mid-charge: no launch, no refund
                TickUpdraft(play, false, 0.0f);
                return;
            }
        }
    }
    if (isHeld) {
        Z64Items_KeepHeld(sApi);
        TickUpdraft(play, true, (f32)sWindCharge / (f32)WIND_CHARGE_FULL);
        return;
    }
    if (sWindCharge >= WIND_CHARGE_MIN) {
        LaunchOnColumn(play, player);
    }
    EndWindCharge(player, play);
    TickUpdraft(play, false, 0.0f);
}

/**
 * Multiplies whatever vertical launch just happened for a short window after a gale, so a charge that ends in
 * a side hop becomes real distance. The hop flag is cleared for the Rito's reason: a boosted hop that keeps
 * it can no longer grab a ledge on the way up.
 */
static void BoostNextJump(Player* player) {
    if (sWindBoostTimer <= 0 || player->actor.velocity.y <= 0.0f) {
        return;
    }
    player->actor.velocity.y *= WIND_BOOST;
    player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
}

// ---- Tornado Rod: the horizontal launch ---------------------------------------------------------------------

static void EndGale(Player* player, PlayState* play) {
    sGalePhase = GALE_OFF;
    sGaleTimer = 0;
    player->actor.gravity = GALE_VANILLA_GRAVITY;
    if (player->actionFunc == RideGaleAction) {
        func_80839FFC(player, play);
    }
}

// A lock-on gives the BEARING only: the power is fixed, so overshooting is part of the tool.
static bool CastGale(Player* player, PlayState* play) {
    if (sGalePhase != GALE_OFF || !(player->actor.bgCheckFlags & 1) || !SpendMagic(play, GALE_COST)) {
        return false;
    }
    sGaleYaw = HasLockOn(player) ? Math_Vec3f_Yaw(&player->actor.world.pos, &player->focusActor->focus.pos)
                                 : player->actor.shape.rot.y;
    sGalePhase = GALE_ANTICIPATE;
    sGaleTimer = GALE_ANTICIPATE_FRAMES;
    Player_SetupAction(play, player, RideGaleAction, 0);
    return true;
}

static void PlayGaleAnim(PlayState* play, Player* player, LinkAnimationHeader* anim, bool doLoop) {
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim),
                         doLoop ? ANIMMODE_LOOP : ANIMMODE_ONCE, -4.0f);
}

static void KickOffGale(PlayState* play, Player* player) {
    s16 quake = Quake_Add(Play_GetCamera(play, 0), 3);

    player->actor.velocity.y = GALE_RISE;
    player->actor.gravity = 0.0f; // the arc is integrated below, not by the engine
    player->actor.bgCheckFlags &= ~1;
    player->stateFlags1 |= PLAYER_STATE1_JUMPING;
    player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
    PlayGaleAnim(play, player, (LinkAnimationHeader*)sSwimAnim, true);

    Quake_SetSpeed(quake, GALE_QUAKE_SPEED);
    Quake_SetQuakeValues(quake, GALE_QUAKE_AMPLITUDE, 0, 0, 0);
    Quake_SetCountdown(quake, GALE_QUAKE_FRAMES);
    SpawnDust(play, &player->actor.world.pos, 20.0f, 70.0f, 6, &sGaleDust);
    Player_PlaySfx(&player->actor, NA_SE_PL_ROLL);
}

static void FlyGale(Player* player) {
    Math_ScaledStepToS(&player->actor.shape.rot.y, sGaleYaw, GALE_TURN_RATE);
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
    player->linearVelocity = GALE_SPEED_XZ;
    player->actor.velocity.y += GALE_GRAVITY;
    player->actor.gravity = 0.0f;
}

// Anything solid ends the flight. The floor is exempt for the first frames because the launch velocity is
// only applied on the NEXT engine move, so Link is still standing on the ground he just left.
static bool HasGaleHitSomething(Player* player, s16 flown) {
    s16 solid = 8 | 2; // wall and ceiling

    if (flown >= GALE_LIFTOFF_FRAMES) {
        solid |= 1;
    }
    return (player->actor.bgCheckFlags & solid) != 0;
}

// The flight is driven from the mod's tick, which runs after the action func: the body is the last word of
// the frame that way, and the action func itself is only there to keep vanilla out of the wheel.
static void RideGaleAction(Player* player, PlayState* play) {
    LinkAnimation_Update(play, &player->skelAnime);
}

static void TickGale(PlayState* play, Player* player) {
    if (sGalePhase == GALE_OFF) {
        return;
    }
    if (player->actionFunc != RideGaleAction) {
        EndGale(player, play);
        return;
    }
    Z64Items_KeepHeld(sApi);
    sGaleTimer--;

    switch (sGalePhase) {
        case GALE_ANTICIPATE:
            player->linearVelocity = 0.0f;
            if (sGaleTimer <= 0) {
                sGalePhase = GALE_IMPULSE;
                sGaleTimer = GALE_IMPULSE_FRAMES;
            }
            break;

        case GALE_IMPULSE:
            player->linearVelocity = 0.0f;
            if (sGaleTimer <= 0) {
                KickOffGale(play, player);
                sGalePhase = GALE_FLIGHT;
                sGaleTimer = GALE_FLIGHT_FRAMES;
            }
            break;

        case GALE_FLIGHT:
            FlyGale(player);
            if (sGaleTimer > 0 && !HasGaleHitSomething(player, GALE_FLIGHT_FRAMES - sGaleTimer)) {
                break;
            }
            if (player->actor.bgCheckFlags & (8 | 2)) {
                player->linearVelocity = 0.0f; // hitting something kills the run; a timeout keeps it
            }
            sGalePhase = GALE_FALL;
            sGaleTimer = GALE_FALL_LIMIT;
            player->actor.gravity = GALE_VANILLA_GRAVITY;
            break;

        case GALE_FALL:
            if (sGaleTimer > 0 && !(player->actor.bgCheckFlags & 1)) {
                break;
            }
            sGalePhase = GALE_ROLL;
            sGaleTimer = GALE_ROLL_FRAMES;
            player->linearVelocity = 0.0f;
            PlayGaleAnim(play, player, (LinkAnimationHeader*)sLandingRollAnim, false);
            Player_PlaySfx(&player->actor, NA_SE_PL_BOUND_DIRT);
            break;

        default: // GALE_ROLL — hand the body back and let OoT stand him up itself
            if (sGaleTimer <= 0) {
                EndGale(player, play);
            }
            break;
    }
}

// ---- Water Rod --------------------------------------------------------------------------------------------

// The vanilla oscillation, kept because it is what sells the column as water rather than a lift.
static void UpdateGeyser(Actor* thisx, PlayState* play) {
    EnSiofuki* geyser = (EnSiofuki*)thisx;
    f32 bob = Math_SinS((s16)(play->gameplayFrames * 0x800)) * 4.0f;

    Math_SmoothStepToF(&geyser->currentHeight, geyser->targetHeight, WATER_STEP_SCALE, WATER_STEP_MAX, WATER_STEP_MIN);
    thisx->world.pos.y = geyser->initPosY + geyser->currentHeight + bob;
    func_800F436C(&thisx->projectedPos, NA_SE_EV_FOUNTAIN - SFX_FLAG, 1.0f);
}

static void ForgetGeyser(void) {
    sWaterGeyser = NULL;
}

static bool CastWater(Player* player, PlayState* play) {
    Vec3f pos;
    s16 yaw = player->actor.shape.rot.y;
    EnSiofuki* geyser;

    // Already up: the cast is the lift control, so it just flips which way the column is going.
    if (sWaterGeyser != NULL && sWaterGeyser->update == UpdateGeyser) {
        geyser = (EnSiofuki*)sWaterGeyser;
        geyser->targetHeight = geyser->targetHeight > WATER_REST_HEIGHT ? WATER_REST_HEIGHT : WATER_RIDE_HEIGHT;
        return true;
    }
    if (Object_GetIndex(&play->objectCtx, OBJECT_SIOFUKI) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_SIOFUKI);
        return false;
    }
    pos.x = player->actor.world.pos.x + Math_SinS(yaw) * WATER_SPAWN_DIST;
    pos.y = player->actor.world.pos.y;
    pos.z = player->actor.world.pos.z + Math_CosS(yaw) * WATER_SPAWN_DIST;

    sWaterGeyser = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_SIOFUKI, pos.x, pos.y, pos.z, WATER_MAX_HEIGHT_ROT, 0, 0,
                               WATER_PARAMS_RAISING);
    if (sWaterGeyser == NULL) {
        return false;
    }
    // Init kills itself in one Water Temple room, and that path leaves no draw function to put back.
    if (sWaterGeyser->draw == NULL) {
        sWaterGeyser = NULL;
        return false;
    }
    sWaterGeyser->update = UpdateGeyser;
    sWaterGeyser->room = -1;

    geyser = (EnSiofuki*)sWaterGeyser;
    geyser->initPosY = pos.y;
    geyser->currentHeight = 0.0f;
    geyser->targetHeight = WATER_REST_HEIGHT;
    return true;
}

// ---- Meteor Rod -------------------------------------------------------------------------------------------

// Wrapped, not replaced: EnBom_Draw also billboards and runs Collider_UpdateSpheres, which is what gives the
// explosion its hitbox.
static void DrawTintedBomb(Actor* thisx, PlayState* play) {
    if (sMeteorBombDraw == NULL) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, METEOR_TINT_R, METEOR_TINT_G, METEOR_TINT_B, METEOR_TINT_MIX);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, METEOR_TINT_R, METEOR_TINT_G, METEOR_TINT_B, METEOR_TINT_MIX);
    gSPGrayscale(POLY_XLU_DISP++, true);
    CLOSE_DISPS(play->state.gfxCtx);

    sMeteorBombDraw(thisx, play);

    OPEN_DISPS(play->state.gfxCtx);
    gSPGrayscale(POLY_OPA_DISP++, false); // both buffers, or the rest of the frame comes out red too
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Read live rather than captured at launch, so switching target mid-flight redirects the bomb and dropping Z
// lets it run straight on.
static Actor* MeteorTarget(PlayState* play) {
    Player* player = GET_PLAYER(play);

    return HasLockOn(player) ? player->focusActor : NULL;
}

// Measured from the bomb's SHADOW: an enemy's world.pos sits on the floor, so a bomb mid-arc is a whole hop
// height away from a target it is passing straight through.
static bool IsMeteorTouchingEnemy(Actor* thisx, PlayState* play) {
    Vec3f shadow = { thisx->world.pos.x, thisx->floorHeight, thisx->world.pos.z };

    if (thisx->floorHeight <= BGCHECK_Y_MIN) {
        shadow.y = thisx->world.pos.y; // over a pit there is no shadow; it is falling, so use itself
    }
    return FindNearestEnemy(play, &shadow, METEOR_ENEMY_TRIGGER_RADIUS) != NULL;
}

// speedXZ is re-stamped, not just seeded: EnBom_Move brakes while grounded, and a bomb pinned near the floor
// is grounded on almost every frame.
static void HopMeteor(Actor* thisx) {
    f32 phase = (f32)METEOR_HOP_TIME(thisx) / METEOR_HOP_PERIOD;
    s16 hopIndex = (s16)((f32)METEOR_HOP_TIME(thisx) / METEOR_HOP_PERIOD);
    f32 falloff = 1.0f / (1.0f + METEOR_HOP_DECAY * (f32)hopIndex);

    thisx->world.pos.y = thisx->floorHeight + fabsf(sinf(M_PI * phase)) * METEOR_HOP_HEIGHT * falloff;
    thisx->velocity.y = 0.0f; // or the vanilla floor bounce fights the curve
    thisx->speedXZ = METEOR_LAUNCH_SPEED * falloff;
    METEOR_HOP_TIME(thisx)++;
}

static void UpdateMeteor(Actor* thisx, PlayState* play) {
    // Read before the vanilla update: EnBom_Move consumes both bits as it bounces.
    bool hasHitWall = (thisx->bgCheckFlags & 8) != 0;
    bool hasLanded = (thisx->bgCheckFlags & 2) != 0;
    Actor* target;

    if (thisx->params == BOMB_BODY) {
        if (METEOR_AGE(thisx) < METEOR_ARM_FRAMES) {
            METEOR_AGE(thisx)++;
        } else if (hasHitWall || IsMeteorTouchingEnemy(thisx, play)) {
            // Anything solid sets it off. Only the floor may be hit over and over — that is the skip.
            ((EnBom*)thisx)->timer = 0;
        }
        // The throw itself is vanilla gravity out of Link's hand; the skip begins where it lands.
        if (hasLanded && METEOR_HOP_TIME(thisx) == METEOR_AIRBORNE) {
            METEOR_HOP_TIME(thisx) = 0;
        }
        target = MeteorTarget(play);
        if (target != NULL) {
            // world.rot.y is the heading Actor_MoveXZGravity drives speedXZ along, so turning it steers.
            Math_ScaledStepToS(&thisx->world.rot.y, Math_Vec3f_Yaw(&thisx->world.pos, &target->focus.pos),
                               METEOR_STEER_RATE);
        }
    }

    sMeteorBombUpdate(thisx, play);

    if (thisx->params == BOMB_BODY && METEOR_HOP_TIME(thisx) != METEOR_AIRBORNE && thisx->floorHeight > BGCHECK_Y_MIN) {
        HopMeteor(thisx);
    }
}

static bool CastMeteor(Player* player, PlayState* play) {
    Actor* target = MeteorTarget(play);
    s16 yaw = target != NULL ? Math_Vec3f_Yaw(&player->actor.world.pos, &target->focus.pos) : player->actor.shape.rot.y;
    Actor* bomb =
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, player->actor.world.pos.x + Math_SinS(yaw) * METEOR_SPAWN_DIST,
                    player->actor.world.pos.y + METEOR_SPAWN_HEIGHT,
                    player->actor.world.pos.z + Math_CosS(yaw) * METEOR_SPAWN_DIST, 0, yaw, 0, BOMB_BODY);

    if (bomb == NULL) {
        return false;
    }
    if (sMeteorBombUpdate == NULL) {
        sMeteorBombUpdate = bomb->update;
        sMeteorBombDraw = bomb->draw;
    }
    bomb->update = UpdateMeteor;
    bomb->draw = DrawTintedBomb;

    ((EnBom*)bomb)->timer = METEOR_FUSE_FRAMES;
    Actor_SetScale(bomb, METEOR_BOMB_SCALE);
    METEOR_HOP_TIME(bomb) = METEOR_AIRBORNE;
    METEOR_AGE(bomb) = 0;
    bomb->speedXZ = METEOR_LAUNCH_SPEED;
    bomb->velocity.y = METEOR_LAUNCH_RISE;
    bomb->world.rot.y = yaw;

    // NA_SE_IT_BOMB_IGNIT is continuous — vanilla only ever plays it minus the flag, so raw it would loop.
    PlaySfxAt(NA_SE_PL_THROW, &bomb->world.pos);
    return true;
}

// ---- Storm Rod --------------------------------------------------------------------------------------------

static void ForgetStormRay(void) {
    sStormRay.isActive = false;
}

static void TickStormRay(PlayState* play, Player* player) {
    if (!sStormRay.isActive) {
        return;
    }
    if (--sStormRay.life <= 0 || (sStormRayCollider.base.atFlags & AT_HIT)) {
        sStormRayCollider.base.atFlags &= ~AT_HIT;
        sStormRay.isActive = false;
        return;
    }
    sStormRay.pos.x += sStormRay.vel.x;
    sStormRay.pos.y += sStormRay.vel.y;
    sStormRay.pos.z += sStormRay.vel.z;

    // The bolt IS the visual: the effect draws and animates itself, so the rod owns no draw at all.
    if ((sStormRay.life % STORM_BOLT_EVERY) == 0) {
        s16 roll = STORM_BOLT_ROLL + (s16)Rand_CenteredFloat(STORM_BOLT_ROLL_JITTER);

        EffectSsLightning_Spawn(play, &sStormRay.pos, &sStormBoltPrim, &sStormBoltEnv, STORM_BOLT_SCALE, roll,
                                STORM_BOLT_LIFE, STORM_BOLT_BRANCHES);
    }
    if (!sIsStormColliderReady) {
        Collider_InitCylinder(play, &sStormRayCollider);
        Collider_SetCylinder(play, &sStormRayCollider, &player->actor, &sStormRayColliderInit);
        sIsStormColliderReady = true;
    }
    Collider_UpdateCylinder(&player->actor, &sStormRayCollider);
    sStormRayCollider.dim.pos.x = (s16)sStormRay.pos.x;
    sStormRayCollider.dim.pos.y = (s16)sStormRay.pos.y;
    sStormRayCollider.dim.pos.z = (s16)sStormRay.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sStormRayCollider.base);
}

static bool FireStormRay(Player* player, PlayState* play, Actor* target) {
    Vec3f localVel = { 0.0f, 0.0f, STORM_RAY_SPEED };
    s16 yaw;
    s16 pitch;

    if (sStormRay.isActive) {
        return false; // one ray at a time
    }
    sStormRay.pos = player->actor.world.pos;
    sStormRay.pos.y += STORM_RAY_SPAWN_HEIGHT;

    if (target != NULL) {
        // focus.pos, not world.pos: that is the point the game itself considers "where you aimed".
        yaw = Math_Vec3f_Yaw(&sStormRay.pos, &target->focus.pos);
        pitch = Math_Vec3f_Pitch(&sStormRay.pos, &target->focus.pos);
    } else {
        yaw = player->actor.shape.rot.y;
        pitch = 0;
    }
    // Rebuilding this from Math_SinS by hand gets the pitch sign backwards, which is why it goes through the
    // matrix stack the way the elemental rods' own conversion does.
    Matrix_Push();
    Matrix_RotateY(BINANG_TO_RAD(yaw), MTXMODE_NEW);
    Matrix_RotateX(BINANG_TO_RAD(pitch), MTXMODE_APPLY);
    Matrix_MultVec3f(&localVel, &sStormRay.vel);
    Matrix_Pop();

    sStormRay.life = STORM_RAY_LIFE;
    sStormRay.isActive = true;
    PlaySfxAt(NA_SE_IT_MAGIC_ARROW_SHOT, &player->actor.world.pos);
    return true;
}

// En_Okarina_Effect kills itself on Init when a scene weather tag already owns the weather, so a NULL there
// is a legitimate "not now" and the cast reports failure rather than eating the magic.
static bool CallStorm(Player* player, PlayState* play) {
    Actor* storm = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_OKARINA_EFFECT, player->actor.world.pos.x,
                               player->actor.world.pos.y + STORM_SPAWN_Y_OFFSET, player->actor.world.pos.z, 0, 0, 0,
                               STORM_OKARINA_PARAMS);

    return storm != NULL;
}

static bool CastStorm(Player* player, PlayState* play) {
    if (HasLockOn(player)) {
        return FireStormRay(player, play, player->focusActor);
    }
    return CallStorm(player, play);
}

// ---- Shadow Scepter ---------------------------------------------------------------------------------------

// mm.o2r may not be mounted. The bolt still flies and still stuns; it just has nothing to draw.
static bool HasBoeModel(void) {
    static u8 sChecked = 0;
    static bool sIsPresent = false;

    if (!sChecked) {
        sChecked = 1;
        sIsPresent = sApi->HasResource(sBoeModelDL) != 0;
    }
    return sIsPresent;
}

// A frozen enemy is the Deku Nut effect: the freeze stops its update and the colour filter is what makes it
// read as stunned.
static void StunEnemy(Actor* enemy) {
    enemy->freezeTimer = SHADOW_STUN_FRAMES;
    Actor_SetColorFilter(enemy, SHADOW_FILTER_BLUE, SHADOW_FILTER_STRENGTH, 0, SHADOW_STUN_FRAMES);
}

static void ForgetShadowBolt(void) {
    sShadowBolt.isActive = false;
    sShadowBolt.target = NULL;
}

// Nearest by the focus point, which is what the bolt both seeks and hits. Comparing a bolt 25 units above the
// floor with enemy foot origins and a 22 unit radius made level-ground casts miss without exception.
static Actor* FindShadowTarget(PlayState* play, Vec3f* pos, f32 range) {
    Actor* closest = NULL;
    f32 closestSq = SQ(range);

    for (u8 index = 0; index < ARRAY_COUNT(sEnemyCategories); index++) {
        for (Actor* actor = play->actorCtx.actorLists[sEnemyCategories[index]].head; actor != NULL;
             actor = actor->next) {
            f32 distanceSq =
                SQ(actor->focus.pos.x - pos->x) + SQ(actor->focus.pos.y - pos->y) + SQ(actor->focus.pos.z - pos->z);

            if (actor->update != NULL && distanceSq < closestSq) {
                closest = actor;
                closestSq = distanceSq;
            }
        }
    }
    return closest;
}

// A retained target may have been removed since the last frame, so check it against the live lists.
static bool ShadowTargetIsAlive(PlayState* play, Actor* target) {
    for (u8 index = 0; index < ARRAY_COUNT(sEnemyCategories); index++) {
        for (Actor* actor = play->actorCtx.actorLists[sEnemyCategories[index]].head; actor != NULL;
             actor = actor->next) {
            if (actor == target) {
                return actor->update != NULL;
            }
        }
    }
    return false;
}

// Re-acquired rather than trusted: the enemy the bolt left chasing can die before it arrives. Homes in all three
// axes so a focus point above or below the launch height can be hit.
static void TickShadowBolt(PlayState* play) {
    Actor* hit;

    if (!sShadowBolt.isActive) {
        return;
    }
    if (--sShadowBolt.life <= 0) {
        ForgetShadowBolt();
        return;
    }
    if (sShadowBolt.target != NULL && !ShadowTargetIsAlive(play, sShadowBolt.target)) {
        sShadowBolt.target = FindShadowTarget(play, &sShadowBolt.pos, SHADOW_SEEK_RANGE);
    }
    if (sShadowBolt.target != NULL) {
        Vec3f* aim = &sShadowBolt.target->focus.pos;
        f32 dx = aim->x - sShadowBolt.pos.x;
        f32 dy = aim->y - sShadowBolt.pos.y;
        f32 dz = aim->z - sShadowBolt.pos.z;
        f32 distance = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));
        f32 step = distance > SHADOW_SPEED ? SHADOW_SPEED / distance : 1.0f;

        sShadowBolt.yaw = Math_Vec3f_Yaw(&sShadowBolt.pos, aim);
        sShadowBolt.pos.x += dx * step;
        sShadowBolt.pos.y += dy * step;
        sShadowBolt.pos.z += dz * step;
    } else {
        sShadowBolt.pos.x += Math_SinS(sShadowBolt.yaw) * SHADOW_SPEED;
        sShadowBolt.pos.z += Math_CosS(sShadowBolt.yaw) * SHADOW_SPEED;
    }

    hit = FindShadowTarget(play, &sShadowBolt.pos, SHADOW_HIT_RADIUS);
    if (hit != NULL) {
        StunEnemy(hit);
        PlaySfxAt(NA_SE_EN_GANON_DARKWAVE, &sShadowBolt.pos);
        ForgetShadowBolt();
    }
}

// Without mm.o2r the cast still has to be visible: a soft dark billboard with bright eyes.
static Vtx sShadowPuffVtx[] = {
    VTX(0, 0, 0, 0, 0, 18, 10, 26, 245),    VTX(-14, 0, 0, 0, 0, 36, 20, 52, 0),   VTX(-10, 10, 0, 0, 0, 36, 20, 52, 0),
    VTX(0, 14, 0, 0, 0, 36, 20, 52, 0),     VTX(10, 10, 0, 0, 0, 36, 20, 52, 0),  VTX(14, 0, 0, 0, 0, 36, 20, 52, 0),
    VTX(10, -10, 0, 0, 0, 36, 20, 52, 0),   VTX(0, -14, 0, 0, 0, 36, 20, 52, 0),  VTX(-10, -10, 0, 0, 0, 36, 20, 52, 0),
    VTX(-6, 1, 1, 0, 0, 255, 240, 160, 255), VTX(-4, 4, 1, 0, 0, 255, 240, 160, 255),
    VTX(-2, 1, 1, 0, 0, 255, 240, 160, 255), VTX(-4, -2, 1, 0, 0, 255, 240, 160, 255),
    VTX(2, 1, 1, 0, 0, 255, 240, 160, 255), VTX(4, 4, 1, 0, 0, 255, 240, 160, 255),
    VTX(6, 1, 1, 0, 0, 255, 240, 160, 255), VTX(4, -2, 1, 0, 0, 255, 240, 160, 255),
};

static Gfx sShadowPuffDL[] = {
    gsDPPipeSync(),
    gsSPClearGeometryMode(G_LIGHTING | G_CULL_BOTH | G_FOG),
    gsSPSetGeometryMode(G_SHADE | G_SHADING_SMOOTH),
    gsSPTexture(0, 0, 0, G_TX_RENDERTILE, G_OFF),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPVertex(sShadowPuffVtx, ARRAY_COUNT(sShadowPuffVtx), 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(0, 3, 4, 0, 0, 4, 5, 0),
    gsSP2Triangles(0, 5, 6, 0, 0, 6, 7, 0),
    gsSP2Triangles(0, 7, 8, 0, 0, 8, 1, 0),
    gsSP2Triangles(9, 10, 11, 0, 9, 11, 12, 0),
    gsSP2Triangles(13, 14, 15, 0, 13, 15, 16, 0),
    gsSPEndDisplayList(),
};

static void DrawShadowBolt(PlayState* play) {
    bool hasModel;

    if (!sShadowBolt.isActive) {
        return;
    }
    hasModel = HasBoeModel();

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(sShadowBolt.pos.x, sShadowBolt.pos.y, sShadowBolt.pos.z, MTXMODE_NEW);
    // The Boe is a flat billboard in MM too: its own draw replaces the rotation the same way.
    Matrix_ReplaceRotation(&play->billboardMtxF);
    if (hasModel) {
        Mtx* matrix;

        Matrix_Scale(SHADOW_SCALE, SHADOW_SCALE, SHADOW_SCALE, MTXMODE_APPLY);
        matrix = MATRIX_NEWMTX(play->state.gfxCtx);
        // MM's EnMkk sets the body alpha itself and draws the eyes apart; inheriting the alpha of the previous
        // translucent draw can make the whole body disappear.
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 255);
        // The MM material lists reach for segment 8; an empty list keeps them from running whatever the scene
        // left there.
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)gEmptyDL);
        gSPMatrix(POLY_XLU_DISP++, matrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBoeMaterialDL);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBoeModelDL);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBoeEndDL);
        if (sApi->HasResource(sBoeEyesDL)) {
            Gfx_SetupDL_25Opa(play->state.gfxCtx);
            gDPPipeSync(POLY_OPA_DISP++);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 255, 255, 255, 255, 255);
            gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gEmptyDL);
            gSPMatrix(POLY_OPA_DISP++, matrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBoeEyesDL);
        }
    } else {
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, sShadowPuffDL);
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static bool CastShadow(Player* player, PlayState* play) {
    s16 yaw = player->actor.shape.rot.y;

    if (sShadowBolt.isActive) {
        return false; // one bolt at a time
    }
    sShadowBolt.pos.x = player->actor.world.pos.x + Math_SinS(yaw) * SHADOW_SPAWN_DIST;
    sShadowBolt.pos.y = player->actor.world.pos.y + SHADOW_SPAWN_HEIGHT;
    sShadowBolt.pos.z = player->actor.world.pos.z + Math_CosS(yaw) * SHADOW_SPAWN_DIST;

    // No enemy in reach is not a failed cast: the bolt flies off Link's nose and fades on its timer.
    sShadowBolt.target = FindShadowTarget(play, &sShadowBolt.pos, SHADOW_SEEK_RANGE);
    sShadowBolt.yaw = yaw;
    sShadowBolt.life = SHADOW_LIFE_FRAMES;
    sShadowBolt.isActive = true;

    // NOT NA_SE_EN_GANON_DARKWAVE_M: vanilla only ever plays that one minus the flag, so raw it never stops.
    PlaySfxAt(NA_SE_IT_SHIELD_REFLECT_MG, &player->actor.world.pos);
    return true;
}

// ---- casting ----------------------------------------------------------------------------------------------

// Magic is spent only on success, so a rod that declines (no room for the actor, a geyser already up) costs
// nothing and reports the error itself.
static bool CastRod(Player* player, PlayState* play, u8 mode) {
    bool didCast;

    switch (mode) {
        case WAND_SAND:
            didCast = CastSand(player, play);
            break;
        case WAND_TORNADO:
            didCast = StartWindCharge(player, play);
            break;
        case WAND_WATER:
            didCast = CastWater(player, play);
            break;
        case WAND_METEOR:
            didCast = CastMeteor(player, play);
            break;
        case WAND_STORM:
            didCast = CastStorm(player, play);
            break;
        case WAND_SCEPTER:
            didCast = CastShadow(player, play);
            break;
        default:
            return false;
    }
    if (didCast) {
        SpendMagic(play, sRods[mode].magicCost);
    }
    return didCast;
}

static bool StartSwing(Player* player, u8 mode) {
    // The carry animation owns the arms when a rod put something in Link's hands, and the swing would tear
    // the held actor off them.
    if (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
        return false;
    }
    sSwingClip = WAND_SWING_FORE;
    sSwingFrame = 0;
    sSwingMode = mode;
    sHasSwingFired = false;
    return true;
}

/**
 * The swing is written straight onto the joint table after the action func has posed Link, so the legs keep
 * walking or running underneath it and the rod can be cast on the move. The rod goes off on a frame of the
 * second slash: the summon belongs to the animation, not to the button.
 */
static void TickSwing(PlayState* play, Player* player) {
    LinkAnimationHeader* anim;
    Vec3s pose[PLAYER_LIMB_MAX];
    s16 last;

    if (sSwingClip == WAND_SWING_NONE) {
        return;
    }
    anim = (LinkAnimationHeader*)(sSwingClip == WAND_SWING_FORE ? sSwingForeAnim : sSwingBackAnim);
    last = (s16)Animation_GetLastFrame(anim);

    // SetLoadFrame copies synchronously, so the pose is usable in this same pass.
    AnimationContext_SetLoadFrame(play, anim, sSwingFrame, PLAYER_LIMB_MAX, pose);
    for (s32 limb = PLAYER_LIMB_UPPER; limb < PLAYER_LIMB_MAX; limb++) {
        player->skelAnime.jointTable[limb] = pose[limb];
    }

    if (sSwingClip == WAND_SWING_BACK && !sHasSwingFired && sSwingFrame >= (s16)(last * WAND_CAST_FRACTION)) {
        sHasSwingFired = true;
        CastRod(player, play, sSwingMode);
    }
    sSwingFrame++;
    if (sSwingFrame <= last) {
        return;
    }
    sSwingClip = sSwingClip == WAND_SWING_FORE ? WAND_SWING_BACK : WAND_SWING_NONE;
    sSwingFrame = 0;
}

// ---- item callbacks ---------------------------------------------------------------------------------------

static void TakeOutWand(PlayState* play, Player* player) {
    sIsWandOut = true;
    sIsPressPending = false;
    sSwingClip = WAND_SWING_NONE;
}

// The press is only recorded here: resolving it inside the player's own action handler list would let the
// frame's own action func overwrite whatever the rod installed.
static void PressWand(Player* player, PlayState* play) {
    sIsPressPending = true;
}

static int32_t HoldWand(Player* player, PlayState* play) {
    return 0;
}

static void PutWandAway(Player* player, PlayState* play) {
    sIsWandOut = false;
    sSwingClip = WAND_SWING_NONE;
    if (sIsWindCharging) {
        EndWindCharge(player, play);
    }
}

static bool CanUseWand(Player* player, PlayState* play) {
    return !(player->stateFlags1 & PLAYER_STATE1_FIRST_PERSON);
}

// ---- frame ------------------------------------------------------------------------------------------------

// A rod that owns something outside the staff keeps running with it stowed: the bomb is mid-flight, the
// column still stands and the bolt has not reached anything yet.
static void UpdateWand(void) {
    PlayState* play = gPlayState;
    Player* player;
    bool isHeld;
    u8 mode;

    if (play == NULL) {
        return;
    }
    player = GET_PLAYER(play);

    // Everything a rod leaves in the world is an actor the new scene has already thrown away.
    if (sLastScene != play->sceneNum) {
        sLastScene = play->sceneNum;
        ForgetSandSlabs();
        ForgetGeyser();
        ForgetShadowBolt();
        ForgetStormRay();
        ForgetUpdraft();
        sIsWindCharging = false;
        sGalePhase = GALE_OFF;
        sIsStormColliderReady = false;
        sSwingClip = WAND_SWING_NONE;
    }
    SyncUnlockedRods();

    isHeld = IsButtonHeld(play);
    TickShadowBolt(play);
    TickStormRay(play, player);
    TickWindCharge(play, player, isHeld);
    TickGale(play, player);
    BoostNextJump(player);

    if (!sIsWandOut) {
        sSwingClip = WAND_SWING_NONE; // put away mid-swing: the arms go back to the item change
        sWasWandOut = false;
        sIsPressPending = false;
        return;
    }
    // After the action func by construction: the joint table already holds this frame's walk pose.
    TickSwing(play, player);

    mode = CurrentMode();
    // Holding the button lays a sand road. The Tornado's own hold is its charge, ticked above.
    if (mode == WAND_SAND && sSwingClip == WAND_SWING_NONE && ShouldLaySlab(player, isHeld)) {
        if (HasMagic(play, sRods[WAND_SAND].magicCost)) {
            CastRod(player, play, WAND_SAND);
        }
    }

    // The press that drew the staff is not a cast: sWasWandOut is what tells them apart.
    if (sIsPressPending && sWasWandOut && sSwingClip == WAND_SWING_NONE && sGalePhase == GALE_OFF && !sIsWindCharging) {
        bool didFire;

        // Locked on, the Tornado fires Link himself instead of raising a column: no swing, no charge.
        if (mode == WAND_TORNADO && HasLockOn(player)) {
            didFire = CastGale(player, play);
        } else {
            didFire = HasMagic(play, sRods[mode].magicCost) && StartSwing(player, mode);
        }
        if (!didFire) {
            PlaySfxAt(NA_SE_SY_ERROR, &player->actor.world.pos);
        }
    }
    sIsPressPending = false;
    sWasWandOut = true;
}

// Nothing touches the matrix between the hand limb and the end of the post-limb pass while the staff is out,
// so this is the hand bone's own matrix: the rod follows the arm instead of floating beside Link.
static void CaptureHandMatrix(PlayState* play, Player* player, int32_t limbIndex) {
    if (limbIndex != PLAYER_LIMB_R_HAND) {
        return;
    }
    Matrix_Get(&sHandMatrix);
    sHasHandMatrix = true;
}

static void DrawWandInHand(Player* player, PlayState* play) {
    u8 mode = CurrentMode();
    f32 unscale = player->actor.scale.x != 0.0f ? 1.0f / player->actor.scale.x : 1.0f;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Put(&sHandMatrix);
    // That matrix carries Link's own 0.01 body scale: dividing it back out leaves the offsets in world units.
    Matrix_Scale(unscale, unscale, unscale, MTXMODE_APPLY);
    Matrix_RotateY(DEG_TO_RAD(WAND_HELD_ROT_Y), MTXMODE_APPLY);
    Matrix_RotateX(DEG_TO_RAD(WAND_HELD_ROT_X), MTXMODE_APPLY);
    Matrix_RotateZ(DEG_TO_RAD(WAND_HELD_ROT_Z), MTXMODE_APPLY);
    // The offsets come after the rotations, so each slides the staff along its OWN shaft.
    Matrix_Translate(0.0f, WAND_HELD_OFFSET_Y, WAND_HELD_OFFSET_Z, MTXMODE_APPLY);
    Matrix_Scale(WAND_HELD_SCALE, WAND_HELD_SCALE, WAND_HELD_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRods[mode].opaModel);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sRods[mode].xluModel);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawWand(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    // Both of these outlive the staff being out: the column burns and the bolt flies whatever Link is doing.
    DrawUpdraft(play);
    DrawShadowBolt(play);

    if (!sIsWandOut || !sHasHandMatrix || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    DrawWandInHand(player, play);
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(WAND_GIVE_SCALE, WAND_GIVE_SCALE, WAND_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRods[CurrentMode()].opaModel);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void ForgetSceneState(int16_t sceneNum) {
    sLastScene = sceneNum;
    ForgetSandSlabs();
    ForgetGeyser();
    ForgetShadowBolt();
    ForgetStormRay();
    ForgetUpdraft();
    sIsStormColliderReady = false;
    sIsWandOut = false;
    sIsWindCharging = false;
    sGalePhase = GALE_OFF;
}

static void ReloadRods(int32_t fileNum) {
    SyncUnlockedRods();
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOHModRandoOption shuffle = { sizeof(SOHModRandoOption),
                                  WAND_SHUFFLE_OPTION,
                                  "Elemental Wand Rods",
                                  "Which of the six rods the staff can be tuned to: one per medallion, all "
                                  "six from the staff itself, or one more rod for every staff you find.",
                                  sShuffleValues,
                                  ARRAY_COUNT(sShuffleValues),
                                  WAND_SHUFFLE_MEDALLIONS };
    SOHCustomItemDefinition wand = Z64Items_Define(WAND_KEY, sIconSand, sNameTex);

    if (!SOH_MOD_API_HAS(sApi, GetRandoOption)) {
        return;
    }
    Z64Items_SetButtons(&wand, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&wand, 0, 3, 0);
    Z64Items_SetTextbox(&wand, "You got the %rElemental Wand%w!&A staff that answers to six&elements, one at a "
                               "time.^Press %y\xA1%w to swing it and cast&the element it is tuned to.&Hold "
                               "%y\xA2%w to turn the wheel.^Every %gmedallion%w you carry adds&a rod the staff "
                               "can become.");
    Z64Items_SetPauseText(&wand, "%rElemental Wand&%wSix rods in one staff.&%y\xA1%w casts, %y\xA2%w picks the rod.");
    Z64Items_SetAction(&wand, TakeOutWand, HoldWand);
    Z64Items_SetHeldCallbacks(&wand, PressWand, PutWandAway, NULL);
    Z64Items_SetCanUse(&wand, CanUseWand);
    wand.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    wand.getItemEntry.drawFunc = DrawGetItem;
    wand.onReceive = LightNextRod;

    if (!Z64Items_Register(sApi, &wand)) {
        return;
    }
    sApi->RegisterRandoOption(&shuffle);
    Z64Wheel_Register(sApi, WAND_KEY, WAND_KEY, sWheelEntries, ARRAY_COUNT(sWheelEntries), false);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateWand);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    SOH_REGISTER_HOOK(sApi, OnLoadFile, ReloadRods);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, CaptureHandMatrix);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawWand);
}
