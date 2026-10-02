#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64wheel.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Bg_Ice_Shelter/z_bg_ice_shelter.h"

#define GUST_JAR_KEY "nei.gust_jar"
#define GUST_JAR_BUTTON_COUNT 8
#define GUST_JAR_HELD_SCALE 1.5f
#define GUST_JAR_GIVE_SCALE 0.5f
#define GUST_JAR_HAND_DROP 2.0f
#define GUST_JAR_NOZZLE_REACH 28.0f
#define GUST_JAR_NOZZLE_HEIGHT 28.0f
#define GUST_JAR_SUCK_LENGTH 260.0f
#define GUST_JAR_SUCK_RADIUS 70.0f
#define GUST_JAR_BLOW_LENGTH 220.0f
#define GUST_JAR_BLOW_RADIUS 90.0f
#define GUST_JAR_PULL_SPEED 8.0f
#define GUST_JAR_ITEM_PULL_SPEED 12.0f
#define GUST_JAR_PUSH_SPEED 14.0f
#define GUST_JAR_DAMAGE 1
#define GUST_JAR_COLLIDER_RADIUS 22
#define GUST_JAR_COLLIDER_HEIGHT 30
#define GUST_JAR_FAIRY_MEMORY 8
// The tornado is modelled 100 units along its own +Y and 47 wide, standing on its base.
#define GUST_JAR_TORNADO_LENGTH 100.0f
#define GUST_JAR_TORNADO_RADIUS 47.0f
#define GUST_JAR_TORNADO_SPIN 0x1800
#define GUST_JAR_RIBBONS 6
#define GUST_JAR_RIBBON_TURNS 0x18000
#define GUST_JAR_RIBBON_STEP 0.06f
#define GUST_JAR_RIBBON_LIFE 8
#define GUST_JAR_SHRUNK_LIMIT 12
#define GUST_JAR_SHRINK_FLOOR 0.15f
// Frame 8 of the two-handed carry is the pose that holds a jar at chest height.
#define GUST_JAR_POSE_FRAME 8

typedef enum {
    GUST_MODE_SUCTION,
    GUST_MODE_WIND,
    GUST_MODE_FIRE,
    GUST_MODE_ICE,
    GUST_MODE_LIGHT,
    GUST_MODE_SHADOW,
    GUST_MODE_SPIRIT,
    GUST_MODE_MAX,
} GustMode;

typedef struct {
    Color_RGBA8 primary;
    Color_RGBA8 secondary;
    u32 damageFlags;
    u16 questItem;
} GustElement;

static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gGustJarNameTex";
static const ALIGN_ASSET(2) char sJarDL[] = "__OTR__objects/object_nei_gust_jar/jar_model_dl";
static const ALIGN_ASSET(2) char sJarBodyDL[] = "__OTR__objects/object_nei_gust_jar/jar_body_dl";
static const ALIGN_ASSET(2) char sJarDecorationDL[] = "__OTR__objects/object_nei_gust_jar/jar_decoration_dl";
// The material paints the funnel white, so the element's colour only sticks if it is set after it.
static const ALIGN_ASSET(2) char sTornadoMaterialDL[] = "__OTR__objects/object_nei_tornado/mat_tornado_f3dlite_tornado";
static const ALIGN_ASSET(2) char sTornadoMeshDL[] = "__OTR__objects/object_nei_tornado/tornado_mesh_tri_0";
static const ALIGN_ASSET(2) char sCarryPoseAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_carryB_free";
static const ALIGN_ASSET(2) char sModeIcons[GUST_MODE_MAX][64] = {
    "__OTR__textures/icon_item_custom/gItemIconGustJarTex",
    "__OTR__textures/icon_item_custom/gItemIconGustJarWindTex",
    "__OTR__textures/icon_item_custom/gItemIconGustJarFireTex",
    "__OTR__textures/icon_item_custom/gItemIconGustJarIceTex",
    "__OTR__textures/icon_item_custom/gItemIconGustJarLightTex",
    "__OTR__textures/icon_item_custom/gItemIconGustJarShadowTex",
    "__OTR__textures/icon_item_custom/gItemIconGustJarSpiritTex",
};

// Suction never damages what it swallows by element, so every blow carries 0 damage: the flags alone
// light torches and fire sun switches, exactly as the matching arrow would. Red ice answers to no damage
// flag at all, so the ice breath melts it by hand.
static const GustElement sElements[GUST_MODE_MAX] = {
    { { 110, 190, 255, 255 }, { 40, 90, 200, 255 }, DMG_SLASH_KOKIRI, 0 },
    { { 180, 255, 180, 255 }, { 60, 170, 90, 255 }, DMG_UNBLOCKABLE, QUEST_MEDALLION_FOREST },
    { { 255, 150, 50, 255 }, { 200, 60, 0, 255 }, DMG_ARROW_FIRE | DMG_MAGIC_FIRE, QUEST_MEDALLION_FIRE },
    { { 150, 225, 255, 255 }, { 60, 140, 220, 255 }, DMG_ARROW_ICE | DMG_MAGIC_ICE, QUEST_MEDALLION_WATER },
    { { 255, 245, 160, 255 },
      { 220, 190, 60, 255 },
      DMG_ARROW_LIGHT | DMG_MAGIC_LIGHT | DMG_MIR_RAY,
      QUEST_MEDALLION_LIGHT },
    { { 190, 130, 230, 255 }, { 100, 40, 150, 255 }, DMG_BOOMERANG, QUEST_MEDALLION_SHADOW },
    { { 255, 200, 110, 255 }, { 210, 120, 30, 255 }, DMG_SLASH_KOKIRI, QUEST_MEDALLION_SPIRIT },
};

static const Z64WheelEntry sWheelEntries[GUST_MODE_MAX] = {
    { sModeIcons[GUST_MODE_SUCTION], "Suction" }, { sModeIcons[GUST_MODE_WIND], "Wind" },
    { sModeIcons[GUST_MODE_FIRE], "Fire" },       { sModeIcons[GUST_MODE_ICE], "Ice" },
    { sModeIcons[GUST_MODE_LIGHT], "Light" },     { sModeIcons[GUST_MODE_SHADOW], "Shadow" },
    { sModeIcons[GUST_MODE_SPIRIT], "Spirit" },
};

// The small things the bare jar can move. Anything heavier waits for the element that matches it.
static const s16 sSuckableActors[] = {
    ACTOR_EN_KUSA,     ACTOR_OBJ_TSUBO,  ACTOR_OBJ_KIBAKO,    ACTOR_OBJ_KIBAKO2, ACTOR_OBJ_COMB,
    ACTOR_EN_KANBAN,   ACTOR_BG_YDAN_SP, ACTOR_BG_ICE_TURARA, ACTOR_EN_FIREFLY,  ACTOR_EN_TITE,
    ACTOR_EN_DEKUNUTS, ACTOR_EN_POH,     ACTOR_EN_OKUTA,      ACTOR_EN_DODOJR,   ACTOR_EN_PEEHAT,
    ACTOR_EN_GOMA,     ACTOR_EN_SB,      ACTOR_EN_BILI,       ACTOR_EN_BW,       ACTOR_EN_EIYER,
    ACTOR_EN_WEIYER,   ACTOR_EN_BB,      ACTOR_EN_NY,         ACTOR_EN_SKB,      ACTOR_EN_CROW,
};

static ColliderCylinderInit sNozzleColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_SLASH_KOKIRI, 0x00, GUST_JAR_DAMAGE },
      { 0, 0, 0 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { GUST_JAR_COLLIDER_RADIUS, GUST_JAR_COLLIDER_HEIGHT, 0, { 0, 0, 0 } },
};

static const u16 sItemButtons[GUST_JAR_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                         BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDrawEnd", "OnSceneInit", "OnInterfaceDrawEnd",
                                              "OnLoadFile" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static ColliderCylinder sNozzleCollider;
static bool sIsColliderReady;
static bool sIsJarOut;
static bool sIsBlowing;
static s16 sUseTimer;
static Actor* sFairiesPaid[GUST_JAR_FAIRY_MEMORY];
static u8 sFairiesPaidCount;
static s32 sRibbonEffects[GUST_JAR_RIBBONS];
static f32 sRibbonPhase[GUST_JAR_RIBBONS];
static bool sAreRibbonsLive;
static u8 sRibbonMode;
static Actor* sShrunkActors[GUST_JAR_SHRUNK_LIMIT];
static Vec3f sShrunkScales[GUST_JAR_SHRUNK_LIMIT];
static u8 sShrunkCount;

s32 Player_UpdateUpperBody(Player* player, PlayState* play);
void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);
// z_bg_ice_shelter internals: the melt that its blue fire starts.
void func_808911BC(BgIceShelter* ice);

static void GustAimAction(Player* player, PlayState* play);
static void HoldJarWithBothHands(Player* player, PlayState* play);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static u8 CurrentMode(void) {
    u32 selection = Z64Wheel_GetSelection();

    return selection < GUST_MODE_MAX ? (u8)selection : GUST_MODE_SUCTION;
}

static bool IsBlowMode(u8 mode) {
    return mode != GUST_MODE_SUCTION && mode != GUST_MODE_SPIRIT;
}

static f32 BreathReach(u8 mode) {
    return IsBlowMode(mode) ? GUST_JAR_BLOW_LENGTH : GUST_JAR_SUCK_LENGTH;
}

static void SyncUnlockedModes(void) {
    for (u8 mode = GUST_MODE_WIND; mode < GUST_MODE_MAX; mode++) {
        Z64Wheel_SetEntryUnlocked(mode, CHECK_QUEST_ITEM(sElements[mode].questItem));
    }
}

static u16 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < GUST_JAR_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, GUST_JAR_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static void GetAim(Player* player, s16* yaw, s16* pitch) {
    if (Z64Aiming_IsAiming()) {
        Z64Aiming_GetDirection(player, yaw, pitch);
        return;
    }
    *yaw = player->actor.shape.rot.y;
    *pitch = 0;
}

// A positive aim pitch looks DOWN, the way every vanilla projectile reads it, so the axis drops as it grows.
static void GetNozzle(Player* player, Vec3f* nozzle, Vec3f* axis, s16 yaw, s16 pitch) {
    f32 flat = Math_CosS(pitch);

    axis->x = Math_SinS(yaw) * flat;
    axis->y = -Math_SinS(pitch);
    axis->z = Math_CosS(yaw) * flat;
    nozzle->x = player->actor.world.pos.x + axis->x * GUST_JAR_NOZZLE_REACH;
    nozzle->y = player->actor.world.pos.y + GUST_JAR_NOZZLE_HEIGHT + axis->y * GUST_JAR_NOZZLE_REACH;
    nozzle->z = player->actor.world.pos.z + axis->z * GUST_JAR_NOZZLE_REACH;
}

static bool IsSuckable(Actor* actor, u8 mode) {
    if (actor->update == NULL || (actor->flags & ACTOR_FLAG_TALK) != 0) {
        return false;
    }
    // The Spirit Medallion's pull takes hold of anything that fights back, not just the small fry.
    if (mode == GUST_MODE_SPIRIT && actor->category == ACTORCAT_ENEMY) {
        return true;
    }
    for (u32 index = 0; index < ARRAY_COUNT(sSuckableActors); index++) {
        if (actor->id == sSuckableActors[index]) {
            return true;
        }
    }
    return false;
}

static bool IsInsideCone(Vec3f* nozzle, Vec3f* axis, Vec3f* point, f32 length, f32 radius, f32* distance) {
    f32 toActorX = point->x - nozzle->x;
    f32 toActorY = point->y - nozzle->y;
    f32 toActorZ = point->z - nozzle->z;
    f32 along = toActorX * axis->x + toActorY * axis->y + toActorZ * axis->z;

    if (along < 0.0f || along > length) {
        return false;
    }
    f32 offX = toActorX - axis->x * along;
    f32 offY = toActorY - axis->y * along;
    f32 offZ = toActorZ - axis->z * along;

    *distance = sqrtf(SQ(toActorX) + SQ(toActorY) + SQ(toActorZ));
    return SQ(offX) + SQ(offY) + SQ(offZ) < SQ(radius);
}

static void ShrinkTowardsNozzle(Actor* actor, f32 factor) {
    for (u8 index = 0; index < sShrunkCount; index++) {
        if (sShrunkActors[index] == actor) {
            actor->scale.x = sShrunkScales[index].x * factor;
            actor->scale.y = sShrunkScales[index].y * factor;
            actor->scale.z = sShrunkScales[index].z * factor;
            return;
        }
    }
    if (sShrunkCount >= GUST_JAR_SHRUNK_LIMIT) {
        return;
    }
    sShrunkActors[sShrunkCount] = actor;
    sShrunkScales[sShrunkCount] = actor->scale;
    sShrunkCount++;
}

// Whatever survived the pull gets its own size back; a dead actor's scale is no longer ours to touch.
static void RestoreShrunkActors(void) {
    for (u8 index = 0; index < sShrunkCount; index++) {
        if (sShrunkActors[index] != NULL && sShrunkActors[index]->update != NULL) {
            sShrunkActors[index]->scale = sShrunkScales[index];
        }
    }
    sShrunkCount = 0;
}

static void StepTowards(Actor* actor, Vec3f* target, f32 speed) {
    f32 deltaX = target->x - actor->world.pos.x;
    f32 deltaY = target->y - actor->world.pos.y;
    f32 deltaZ = target->z - actor->world.pos.z;
    f32 length = sqrtf(SQ(deltaX) + SQ(deltaY) + SQ(deltaZ));

    if (length < 1.0f) {
        return;
    }
    actor->world.pos.x += deltaX * speed / length;
    actor->world.pos.y += deltaY * speed / length;
    actor->world.pos.z += deltaZ * speed / length;
}

static bool WasFairyPaid(Actor* actor) {
    for (u8 index = 0; index < sFairiesPaidCount; index++) {
        if (sFairiesPaid[index] == actor) {
            return true;
        }
    }
    return false;
}

static void PaySpiritFairy(PlayState* play, Actor* actor) {
    if (actor->colChkInfo.health != 0 || WasFairyPaid(actor)) {
        return;
    }
    Vec3f dropPos = { actor->world.pos.x, actor->world.pos.y + 20.0f, actor->world.pos.z };

    Item_DropCollectible(play, &dropPos, ITEM00_FLEXIBLE);
    PlaySfxAt(NA_SE_EV_BUTTERFRY_TO_FAIRY, &dropPos);
    sFairiesPaid[sFairiesPaidCount % GUST_JAR_FAIRY_MEMORY] = actor;
    sFairiesPaidCount = MIN(sFairiesPaidCount + 1, GUST_JAR_FAIRY_MEMORY);
}

static void StopRibbons(PlayState* play) {
    if (!sAreRibbonsLive) {
        return;
    }
    for (u8 index = 0; index < GUST_JAR_RIBBONS; index++) {
        if (sRibbonEffects[index] >= 0) {
            Effect_Delete(play, sRibbonEffects[index]);
            sRibbonEffects[index] = -1;
        }
    }
    sAreRibbonsLive = false;
}

// Blure colours are fixed when the effect is added, so a new element needs new ribbons.
static void StartRibbons(PlayState* play, u8 mode) {
    Color_RGBA8 primary = sElements[mode].primary;
    Color_RGBA8 secondary = sElements[mode].secondary;
    EffectBlureInit1 init;

    StopRibbons(play);
    init.p1StartColor[0] = primary.r;
    init.p1StartColor[1] = primary.g;
    init.p1StartColor[2] = primary.b;
    init.p1StartColor[3] = 255;
    init.p2StartColor[0] = secondary.r;
    init.p2StartColor[1] = secondary.g;
    init.p2StartColor[2] = secondary.b;
    init.p2StartColor[3] = 0;
    init.p1EndColor[0] = primary.r;
    init.p1EndColor[1] = primary.g;
    init.p1EndColor[2] = primary.b;
    init.p1EndColor[3] = 0;
    init.p2EndColor[0] = secondary.r;
    init.p2EndColor[1] = secondary.g;
    init.p2EndColor[2] = secondary.b;
    init.p2EndColor[3] = 0;
    init.elemDuration = GUST_JAR_RIBBON_LIFE;
    init.unkFlag = 0;
    init.calcMode = 2;

    for (u8 index = 0; index < GUST_JAR_RIBBONS; index++) {
        sRibbonEffects[index] = -1;
        Effect_Add(play, &sRibbonEffects[index], EFFECT_BLURE1, 0, 0, &init);
        sRibbonPhase[index] = (f32)index / (f32)GUST_JAR_RIBBONS;
    }
    sAreRibbonsLive = true;
    sRibbonMode = mode;
}

// Each ribbon rides the funnel's surface: it flares out along the axis while it turns around it.
static void FeedRibbons(PlayState* play, Vec3f* nozzle, Vec3f* axis, s16 yaw, u8 mode, bool isBlow) {
    f32 reach = isBlow ? GUST_JAR_BLOW_LENGTH : GUST_JAR_SUCK_LENGTH;
    f32 spread = isBlow ? GUST_JAR_BLOW_RADIUS : GUST_JAR_SUCK_RADIUS;
    f32 width = spread * 0.16f + 3.0f;
    Vec3f right = { Math_CosS(yaw), 0.0f, -Math_SinS(yaw) };
    Vec3f up = { axis->y * right.z - axis->z * right.y, axis->z * right.x - axis->x * right.z,
                 axis->x * right.y - axis->y * right.x };

    if (!sAreRibbonsLive || sRibbonMode != mode) {
        StartRibbons(play, mode);
    }
    for (u8 index = 0; index < GUST_JAR_RIBBONS; index++) {
        EffectBlure* ribbon = Effect_GetByIndex(sRibbonEffects[index]);
        f32 phase = sRibbonPhase[index];
        f32 along = reach * phase;
        f32 flare = spread * phase * phase;
        s16 turn = (s16)((s32)(play->gameplayFrames * GUST_JAR_TORNADO_SPIN) +
                         (s32)(index * (0x10000 / GUST_JAR_RIBBONS)) + (s32)(phase * GUST_JAR_RIBBON_TURNS));
        f32 turnCos = Math_CosS(turn);
        f32 turnSin = Math_SinS(turn);
        Vec3f base = { nozzle->x + axis->x * along + (right.x * turnCos + up.x * turnSin) * flare,
                       nozzle->y + axis->y * along + (right.y * turnCos + up.y * turnSin) * flare,
                       nozzle->z + axis->z * along + (right.z * turnCos + up.z * turnSin) * flare };
        Vec3f tip = { base.x + axis->x * width, base.y + axis->y * width, base.z + axis->z * width };

        if (ribbon != NULL) {
            EffectBlure_AddVertex(ribbon, &tip, &base);
        }
        f32 nextPhase = phase + GUST_JAR_RIBBON_STEP;

        sRibbonPhase[index] = nextPhase >= 1.0f ? nextPhase - 1.0f : nextPhase;
    }
}

static void SpawnStream(PlayState* play, Vec3f* nozzle, Vec3f* axis, u8 mode, bool isBlow) {
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 primary = sElements[mode].primary;
    Color_RGBA8 secondary = sElements[mode].secondary;
    f32 reach = isBlow ? GUST_JAR_BLOW_LENGTH : GUST_JAR_SUCK_LENGTH;
    f32 spread = isBlow ? GUST_JAR_BLOW_RADIUS : GUST_JAR_SUCK_RADIUS;

    for (u8 particle = 0; particle < 3; particle++) {
        f32 along = Rand_ZeroFloat(reach);
        Vec3f pos = { nozzle->x + axis->x * along + Rand_CenteredFloat(spread),
                      nozzle->y + axis->y * along + Rand_CenteredFloat(spread * 0.6f),
                      nozzle->z + axis->z * along + Rand_CenteredFloat(spread) };
        f32 flow = isBlow ? 6.0f : -9.0f;
        Vec3f velocity = { axis->x * flow, axis->y * flow + Rand_CenteredFloat(1.0f), axis->z * flow };

        EffectSsKiraKira_SpawnFocused(play, &pos, &velocity, &accel, &primary, &secondary, 420, 14);
    }
}

static void StrikeWithNozzle(PlayState* play, Vec3f* nozzle, Vec3f* axis, u8 mode, bool isBlow) {
    f32 reach = isBlow ? GUST_JAR_BLOW_LENGTH * 0.5f : 0.0f;

    sNozzleCollider.dim.pos.x = (s16)(nozzle->x + axis->x * reach);
    sNozzleCollider.dim.pos.y = (s16)(nozzle->y + axis->y * reach);
    sNozzleCollider.dim.pos.z = (s16)(nozzle->z + axis->z * reach);
    sNozzleCollider.dim.radius = isBlow ? (s16)GUST_JAR_BLOW_RADIUS : GUST_JAR_COLLIDER_RADIUS;
    sNozzleCollider.dim.height = GUST_JAR_COLLIDER_HEIGHT;
    sNozzleCollider.dim.yShift = -GUST_JAR_COLLIDER_HEIGHT / 2;
    sNozzleCollider.info.toucher.dmgFlags = sElements[mode].damageFlags;
    sNozzleCollider.info.toucher.damage = isBlow ? 0 : GUST_JAR_DAMAGE;
    sNozzleCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sNozzleCollider.base);
    sNozzleCollider.base.atFlags &= ~AT_HIT;
}

static void PullActors(Player* player, PlayState* play, Vec3f* nozzle, Vec3f* axis, u8 mode) {
    static const u8 categories[] = { ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_MISC, ACTORCAT_EXPLOSIVE };

    for (u8 index = 0; index < ARRAY_COUNT(categories); index++) {
        for (Actor* actor = play->actorCtx.actorLists[categories[index]].head; actor != NULL; actor = actor->next) {
            f32 distance = 0.0f;
            bool isCollectible = actor->id == ACTOR_EN_ITEM00;

            if (!IsInsideCone(nozzle, axis, &actor->world.pos, GUST_JAR_SUCK_LENGTH, GUST_JAR_SUCK_RADIUS, &distance) ||
                (!isCollectible && !IsSuckable(actor, mode))) {
                continue;
            }
            if (isCollectible) {
                StepTowards(actor, &player->actor.world.pos, GUST_JAR_ITEM_PULL_SPEED);
                continue;
            }
            StepTowards(actor, nozzle, GUST_JAR_PULL_SPEED);
            ShrinkTowardsNozzle(actor, MAX(distance / GUST_JAR_SUCK_LENGTH, GUST_JAR_SHRINK_FLOOR));
            if (mode == GUST_MODE_SPIRIT) {
                PaySpiritFairy(play, actor);
            }
        }
    }
}

static void PushActors(PlayState* play, Vec3f* nozzle, Vec3f* axis, u8 mode) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; actor != NULL; actor = actor->next) {
        f32 distance = 0.0f;

        if (actor->update == NULL ||
            !IsInsideCone(nozzle, axis, &actor->world.pos, GUST_JAR_BLOW_LENGTH, GUST_JAR_BLOW_RADIUS, &distance)) {
            continue;
        }
        actor->world.pos.x += axis->x * GUST_JAR_PUSH_SPEED;
        actor->world.pos.z += axis->z * GUST_JAR_PUSH_SPEED;
        if (mode == GUST_MODE_WIND) {
            actor->velocity.y = MAX(actor->velocity.y, 4.0f);
        }
    }
}

// Vanilla's own melt: it is the one that sets the block's switch flag and frees whatever it was guarding,
// and for the block that holds a foe frozen it is its parent's timer that lets him thaw.
static void MeltRedIce(BgIceShelter* ice) {
    if (((ice->dyna.actor.params >> 8) & 7) == 4 && ice->dyna.actor.parent != NULL) {
        ice->dyna.actor.parent->freezeTimer = 50;
    }
    func_808911BC(ice);
    Audio_PlayActorSound2(&ice->dyna.actor, NA_SE_EV_ICE_MELT);
}

// Red ice answers only to the blue fire actor, so no damage flag reaches it: the breath melts it itself. Full
// alpha is what marks a block still solid; a mod cannot compare host action funcs, only its own import thunks.
static void MeltRedIceInCone(PlayState* play, Vec3f* nozzle, Vec3f* axis) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor != NULL; actor = actor->next) {
        BgIceShelter* ice = (BgIceShelter*)actor;

        if (actor->id != ACTOR_BG_ICE_SHELTER || ice->alpha != 255) {
            continue;
        }
        // A block stands on its own origin, so the cone is aimed at the middle of the cylinder that guards it.
        Vec3f middle = { actor->world.pos.x, actor->world.pos.y + ice->cylinder1.dim.height * 0.5f,
                         actor->world.pos.z };
        f32 distance = 0.0f;

        if (IsInsideCone(nozzle, axis, &middle, GUST_JAR_BLOW_LENGTH, GUST_JAR_BLOW_RADIUS + ice->cylinder1.dim.radius,
                         &distance)) {
            MeltRedIce(ice);
        }
    }
}

static bool IsButtonHeld(PlayState* play) {
    u16 buttonMask = FindEquippedButtonMask();

    return buttonMask != 0 && (play->state.input[0].cur.button & buttonMask) != 0;
}

static void BreatheThroughJar(Player* player, PlayState* play) {
    u8 mode = CurrentMode();
    s16 yaw;
    s16 pitch;
    Vec3f nozzle;
    Vec3f axis;

    GetAim(player, &yaw, &pitch);
    GetNozzle(player, &nozzle, &axis, yaw, pitch);
    sIsBlowing = IsBlowMode(mode);
    sUseTimer++;
    FeedRibbons(play, &nozzle, &axis, yaw, mode, sIsBlowing);
    SpawnStream(play, &nozzle, &axis, mode, sIsBlowing);
    StrikeWithNozzle(play, &nozzle, &axis, mode, sIsBlowing);
    if (sIsBlowing) {
        PushActors(play, &nozzle, &axis, mode);
        if (mode == GUST_MODE_ICE) {
            MeltRedIceInCone(play, &nozzle, &axis);
        }
    } else {
        PullActors(player, play, &nozzle, &axis, mode);
    }
    if ((sUseTimer % 8) == 1) {
        PlaySfxAt(sIsBlowing ? NA_SE_PL_MAGIC_WIND_NORMAL : NA_SE_EV_WIND_TRAP - SFX_FLAG, &nozzle);
    }
}

static void StopBreathing(PlayState* play) {
    if (sUseTimer == 0) {
        return;
    }
    StopRibbons(play);
    RestoreShrunkActors();
    sNozzleCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    sUseTimer = 0;
    sIsBlowing = false;
}

static void StopAiming(Player* player, PlayState* play) {
    Z64Aiming_Release(player, play);
    if (player->actionFunc == GustAimAction) {
        func_80839FFC(player, play);
    }
}

static void GustAimAction(Player* player, PlayState* play) {
    Z64Aiming_Update(player, play);
    Player_ZeroSpeedXZ(player);
    Player_UpdateUpperBody(player, play);
    HoldJarWithBothHands(player, play);
    // The item's own update stops while this action owns the player, and a stalled item is put away.
    if (SOH_MOD_API_HAS(sApi, KeepCustomItemHeld)) {
        sApi->KeepCustomItemHeld();
    }
    if (IsButtonHeld(play)) {
        BreatheThroughJar(player, play);
    } else {
        StopBreathing(play);
    }
    if (FindEquippedButtonMask() == 0 || (play->state.input[0].press.button & (BTN_CUP | BTN_A | BTN_B))) {
        StopAiming(player, play);
    }
}

static void StartAiming(Player* player, PlayState* play) {
    if (!Z64Aiming_Request(player, play)) {
        return;
    }
    Player_SetupAction(play, player, GustAimAction, 1);
    Player_ZeroSpeedXZ(player);
}

static void TakeOutJar(PlayState* play, Player* player) {
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sNozzleCollider);
        Collider_SetCylinder(play, &sNozzleCollider, &player->actor, &sNozzleColliderInit);
        sIsColliderReady = true;
    }
    // The jar owns C-Up while it is in hand: unclaimed, the press talks to Navi or beeps over the aim.
    if (SOH_MOD_API_HAS(sApi, BlockPlayerInput)) {
        sApi->BlockPlayerInput(GUST_JAR_KEY, BTN_CUP, false);
    }
    sIsJarOut = true;
    sUseTimer = 0;
    sIsBlowing = false;
    sFairiesPaidCount = 0;
}

// Only the arms are borrowed from the carry pose, so Link keeps walking, running and turning as he was.
static void HoldJarWithBothHands(Player* player, PlayState* play) {
    static const u8 armLimbs[] = { PLAYER_LIMB_L_SHOULDER, PLAYER_LIMB_L_FOREARM, PLAYER_LIMB_L_HAND,
                                   PLAYER_LIMB_R_SHOULDER, PLAYER_LIMB_R_FOREARM, PLAYER_LIMB_R_HAND };
    Vec3s poseJoints[PLAYER_LIMB_MAX];

    AnimationContext_SetLoadFrame(play, (LinkAnimationHeader*)sCarryPoseAnim, GUST_JAR_POSE_FRAME, PLAYER_LIMB_MAX,
                                  poseJoints);
    for (u8 index = 0; index < ARRAY_COUNT(armLimbs); index++) {
        player->skelAnime.jointTable[armLimbs[index]] = poseJoints[armLimbs[index]];
    }
}

static s32 UseJar(Player* player, PlayState* play) {
    HoldJarWithBothHands(player, play);
    if (Z64Wheel_IsOpen() || player->actionFunc == GustAimAction) {
        return 0;
    }
    if (play->state.input[0].press.button & BTN_CUP) {
        StartAiming(player, play);
        return 0;
    }
    if (!IsButtonHeld(play)) {
        StopBreathing(play);
        return 0;
    }
    BreatheThroughJar(player, play);
    return 0;
}

static void PutJarAway(Player* player, PlayState* play) {
    StopAiming(player, play);
    StopBreathing(play);
    if (SOH_MOD_API_HAS(sApi, ReleasePlayerInput)) {
        sApi->ReleasePlayerInput(GUST_JAR_KEY);
    }
    sIsJarOut = false;
}

// The jar sits between both hands of the carry pose, laid on its side to face where Link faces, and its band
// takes the colour of the breath: the mouth is the only part of the model that says what the jar is doing.
static void DrawJarInHand(Player* player, PlayState* play) {
    Vec3f* leftHand = &player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    Vec3f* rightHand = &player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    Color_RGBA8 band = sElements[CurrentMode()].primary;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate((leftHand->x + rightHand->x) * 0.5f, (leftHand->y + rightHand->y) * 0.5f - GUST_JAR_HAND_DROP,
                     (leftHand->z + rightHand->z) * 0.5f, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateX(M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Scale(GUST_JAR_HELD_SCALE, GUST_JAR_HELD_SCALE, GUST_JAR_HELD_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sJarBodyDL);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, band.r, band.g, band.b, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sJarDecorationDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The cone stands on its own base along +Y, so it is laid down onto the aim before it is stretched. The quarter
// turn that lays it down grows with the pitch, because a positive pitch aims below the horizon.
static void DrawTornado(Player* player, PlayState* play) {
    u8 mode = CurrentMode();
    Color_RGBA8 color = sElements[mode].primary;
    f32 reach = BreathReach(mode);
    f32 spread = IsBlowMode(mode) ? GUST_JAR_BLOW_RADIUS : GUST_JAR_SUCK_RADIUS;
    f32 grow = MIN(sUseTimer / 8.0f, 1.0f);
    s16 yaw;
    s16 pitch;
    Vec3f nozzle;
    Vec3f axis;

    GetAim(player, &yaw, &pitch);
    GetNozzle(player, &nozzle, &axis, yaw, pitch);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoMaterialDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, color.r, color.g, color.b, (u8)(190.0f * grow));
    Matrix_Translate(nozzle.x, nozzle.y, nozzle.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(yaw), MTXMODE_APPLY);
    Matrix_RotateX(M_PI / 2.0f + BINANG_TO_RAD(pitch), MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD((s16)(play->gameplayFrames * GUST_JAR_TORNADO_SPIN)), MTXMODE_APPLY);
    Matrix_Scale(spread / GUST_JAR_TORNADO_RADIUS * grow, reach / GUST_JAR_TORNADO_LENGTH * grow,
                 spread / GUST_JAR_TORNADO_RADIUS * grow, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoMeshDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGustJar(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (!sIsJarOut || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    if (Z64Aiming_IsAiming()) {
        Z64Aiming_DrawReticle(play, player, BreathReach(CurrentMode()));
    } else if (!(player->stateFlags1 & PLAYER_STATE1_FIRST_PERSON)) {
        DrawJarInHand(player, play);
    }
    if (sUseTimer > 0) {
        DrawTornado(player, play);
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(GUST_JAR_GIVE_SCALE, GUST_JAR_GIVE_SCALE, GUST_JAR_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sJarDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void UpdateUnlocks(void) {
    if (gPlayState != NULL) {
        SyncUnlockedModes();
    }
}

// The scene tears down every actor and effect, so the ribbons and the borrowed scales are gone with it.
static void DropAimOnSceneChange(int16_t sceneNum) {
    Z64Aiming_Drop();
    sIsJarOut = false;
    sUseTimer = 0;
    sAreRibbonsLive = false;
    sShrunkCount = 0;
    sFairiesPaidCount = 0;
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, StorageSet)) {
        return;
    }

    SOHCustomItemDefinition jar = Z64Items_Define(GUST_JAR_KEY, sModeIcons[GUST_MODE_SUCTION], sNameTex);

    Z64Items_SetButtons(&jar, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&jar, 0, 12, 0);
    Z64Items_SetTextbox(&jar, "You got the %rGust Jar%w!&A Minish jar that breathes in&whatever it can swallow.^"
                              "Hold %y\xA1%w to pull in grass, pots&and small foes. Anything that&reaches the mouth "
                              "is cut down.^%y\xA5%w aims it. %y\xA2%w turns the&wheel: every %gmedallion%w adds&a "
                              "breath of its own to blow.");
    Z64Items_SetPauseText(&jar, "%rGust Jar&%wHold %y\xA1%w to suck things in.&%y\xA5%w aims, %y\xA2%w picks the "
                                "breath.");
    Z64Items_SetAction(&jar, TakeOutJar, UseJar);
    Z64Items_SetHeldCallbacks(&jar, NULL, PutJarAway, NULL);
    jar.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    jar.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &jar)) {
        return;
    }
    Z64Wheel_Register(sApi, GUST_JAR_KEY, GUST_JAR_KEY, sWheelEntries, ARRAY_COUNT(sWheelEntries), false);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateUnlocks);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, DropAimOnSceneChange);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawGustJar);
}
