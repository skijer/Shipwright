// The elemental shots: the bow and the slingshot each carry a primed element, the shot flies as its vanilla
// arrow type and wears the SW97 arrow actor as its effect. Dark shots drain life, ice shots crack mud walls, and
// light shots wake sun switches, as NEI's SW97 mode does.

#include "z64items.h"

#include <string.h>

#include "sw97_mod.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "overlays/actors/ovl_Bg_Breakwall/z_bg_breakwall.h"
#include "overlays/actors/ovl_Obj_Lightswitch/z_obj_lightswitch.h"

#define ELEMENTS_STORAGE_FIELD "elements"
#define TRACKED_ARROW_COUNT 16
#define LIFESTEAL_SLOTS 16
#define LIFESTEAL_TOTAL_FRAMES 90
#define LIFESTEAL_TICK_FRAMES 20
#define LIFESTEAL_DAMAGE_PER_TICK 4
#define SUN_SWITCH_SLOTS 8
#define SUN_SWITCH_STAYS_LIT_ROOM 25

typedef struct {
    uint8_t bow;
    uint8_t slingshot;
} SavedElements;

typedef struct {
    EnArrow* arrow;
    Sw97Element element;
    bool isSeed;
    bool isShot;
    bool hasBlure;
    Vec3f lastTip;
    Vec3f lastBase;
} TrackedArrow;

typedef struct {
    Actor* actor;
    s16 remainingFrames;
    s16 tickPhase;
} Lifesteal;

typedef struct {
    Actor* actor;
    bool isLitByArrow;
} SunSwitch;

// The address of an exe function is only comparable through its import; &f alone is the DLL's own thunk.
extern HOST_DATA void EnArrow_Fly(EnArrow* arrow, PlayState* play);
u8 Randomizer_GetSettingValue(RandomizerSettingKey randoSettingKey);

static const uint8_t sElementQuest[SW97_ELEM_COUNT] = {
    0,
    QUEST_MEDALLION_FIRE,
    QUEST_MEDALLION_WATER,
    QUEST_MEDALLION_LIGHT,
    QUEST_MEDALLION_SHADOW,
    QUEST_MEDALLION_SPIRIT,
    QUEST_MEDALLION_FOREST,
};

// A vanilla elemental arrow unlocks its element too, so fire arrows alone still give the fire entry.
static const int16_t sElementVanillaArrow[SW97_ELEM_COUNT] = {
    -1, ITEM_ARROW_FIRE, ITEM_ARROW_ICE, ITEM_ARROW_LIGHT, -1, -1, -1,
};

// Vanilla's own damage table for ARROW_FIRE..ARROW_0E, which the elemental seeds borrow.
static const u32 sElementDamage[SW97_ELEM_COUNT] = {
    0, 0x00000800, 0x00001000, 0x00002000, 0x00010000, 0x00004000, 0x00008000,
};

static EffectBlureInit2 sBlureDark = {
    0, 4, 0, { 0, 255, 200, 255 }, { 0, 255, 255, 255 }, { 0, 255, 200, 0 }, { 0, 255, 255, 0 }, 16,
    0, 1, 0, { 80, 0, 80, 255 },   { 30, 0, 30, 0 },     TRAIL_TYPE_REST,
};
static EffectBlureInit2 sBlureSoul = {
    0, 4, 0, { 0, 255, 200, 255 },   { 0, 255, 255, 255 }, { 0, 255, 200, 0 }, { 0, 255, 255, 0 }, 16,
    0, 1, 0, { 255, 255, 170, 255 }, { 200, 200, 0, 0 },   TRAIL_TYPE_REST,
};
static EffectBlureInit2 sBlureWind = {
    0, 4, 0, { 0, 255, 200, 255 },   { 0, 255, 255, 255 }, { 0, 255, 200, 0 }, { 0, 255, 255, 0 }, 16,
    0, 1, 0, { 170, 255, 170, 255 }, { 0, 180, 0, 0 },     TRAIL_TYPE_REST,
};

static ColliderJntSphElementInit sSunSwitchLightElementInit[] = {
    {
        {
            ELEMTYPE_UNK0,
            { 0x00000000, 0x00, 0x00 },
            { 0x00202000, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_ON,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 19 }, 100 },
    },
};
static ColliderJntSphInit sSunSwitchLightInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_JNTSPH,
    },
    1,
    sSunSwitchLightElementInit,
};

static const Vec3f sArrowTip = { 0.0f, 400.0f, 1500.0f };
static const Vec3f sArrowBase = { 0.0f, -400.0f, 1500.0f };

static SavedElements sElements;
static TrackedArrow sTrackedArrows[TRACKED_ARROW_COUNT];
static Lifesteal sLifesteals[LIFESTEAL_SLOTS];
static uint8_t sLifestealCount;
static SunSwitch sSunSwitches[SUN_SWITCH_SLOTS];

static bool IsElementOwned(Sw97Element element) {
    if (element == SW97_ELEM_NONE) {
        return true;
    }
    if (element >= SW97_ELEM_COUNT) {
        return false;
    }
    if (CHECK_QUEST_ITEM(sElementQuest[element])) {
        return true;
    }
    return sElementVanillaArrow[element] >= 0 &&
           INV_CONTENT((uint16_t)sElementVanillaArrow[element]) == (uint8_t)sElementVanillaArrow[element];
}

uint8_t Sw97_ElementCount(void) {
    uint8_t count = 0;

    for (uint8_t element = 0; element < SW97_ELEM_COUNT; element++) {
        count += IsElementOwned((Sw97Element)element) ? 1 : 0;
    }
    return count;
}

Sw97Element Sw97_ElementAt(uint8_t index) {
    uint8_t seen = 0;

    for (uint8_t element = 0; element < SW97_ELEM_COUNT; element++) {
        if (!IsElementOwned((Sw97Element)element)) {
            continue;
        }
        if (seen == index) {
            return (Sw97Element)element;
        }
        seen++;
    }
    return SW97_ELEM_NONE;
}

static void StoreElements(void) {
    gSw97Api->StorageSet(SW97_MOD_NAME, ELEMENTS_STORAGE_FIELD, &sElements, sizeof(sElements));
}

// A medallion can be lost after its element was primed, so a stale flag falls back to the plain shot.
Sw97Element Sw97_GetElement(bool isSling) {
    uint8_t* stored = isSling ? &sElements.slingshot : &sElements.bow;

    if (!IsElementOwned((Sw97Element)*stored)) {
        *stored = SW97_ELEM_NONE;
        StoreElements();
    }
    return (Sw97Element)*stored;
}

void Sw97_SetElement(bool isSling, Sw97Element element) {
    if (!IsElementOwned(element)) {
        return;
    }
    if (isSling) {
        sElements.slingshot = element;
    } else {
        sElements.bow = element;
    }
    StoreElements();
}

static void LoadElements(int32_t fileNum) {
    memset(&sElements, 0, sizeof(sElements));
    gSw97Api->StorageGet(SW97_MOD_NAME, ELEMENTS_STORAGE_FIELD, &sElements, sizeof(sElements));
}

static uint8_t HeldButtonItem(Player* player) {
    if (player->heldItemButton < 0 || player->heldItemButton >= ARRAY_COUNT(gSaveContext.equips.buttonItems)) {
        return ITEM_NONE;
    }
    return gSaveContext.equips.buttonItems[player->heldItemButton];
}

static bool IsPrimedBowShot(Player* player) {
    return HeldButtonItem(player) == ITEM_BOW && Sw97_GetElement(false) != SW97_ELEM_NONE;
}

static void ResolveBowItemAction(int32_t item, int8_t* itemAction) {
    Sw97Element element;

    if (item != ITEM_BOW) {
        return;
    }
    element = Sw97_GetElement(false);
    if (element != SW97_ELEM_NONE) {
        *itemAction = (int8_t)(PLAYER_IA_BOW + element);
    }
}

// SW97 shots cost no magic in NEI: the fire, ice and light ones ride vanilla's paid arrow types here.
static void WaiveElementalShotCost(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);

    if (IsPrimedBowShot(player)) {
        *should = false;
    }
}

static TrackedArrow* FindTrackedArrow(Actor* arrow) {
    for (uint8_t i = 0; i < TRACKED_ARROW_COUNT; i++) {
        if (sTrackedArrows[i].arrow != NULL && &sTrackedArrows[i].arrow->actor == arrow) {
            return &sTrackedArrows[i];
        }
    }
    return NULL;
}

static TrackedArrow* ClaimTrackedArrow(void) {
    for (uint8_t i = 0; i < TRACKED_ARROW_COUNT; i++) {
        if (sTrackedArrows[i].arrow == NULL) {
            return &sTrackedArrows[i];
        }
    }
    return NULL;
}

static EffectBlureInit2* ElementBlure(Sw97Element element) {
    if (element == SW97_ELEM_DARK) {
        return &sBlureDark;
    }
    if (element == SW97_ELEM_SOUL) {
        return &sBlureSoul;
    }
    return element == SW97_ELEM_WIND ? &sBlureWind : NULL;
}

// Which element a freshly spawned arrow belongs to, or none when it is not a SW97 shot at all.
static Sw97Element ElementOfNewArrow(EnArrow* arrow, Player* player) {
    s16 params = arrow->actor.params;

    if (params >= ARROW_FIRE && params <= ARROW_0E && IsPrimedBowShot(player)) {
        return (Sw97Element)(params - ARROW_NORMAL);
    }
    if (params == ARROW_SEED && HeldButtonItem(player) == ITEM_SLINGSHOT) {
        return Sw97_GetElement(true);
    }
    return SW97_ELEM_NONE;
}

static void SpawnElementEffect(PlayState* play, TrackedArrow* tracked) {
    Actor_SpawnAsChild(&play->actorCtx, &tracked->arrow->actor, play, Sw97_GetArrowActorId(tracked->element),
                       tracked->arrow->actor.world.pos.x, tracked->arrow->actor.world.pos.y,
                       tracked->arrow->actor.world.pos.z, 0, 0, 0, 0);
}

// Vanilla's ARROW_0C..0E get no trail at all, so the three SW97-only elements bring their own.
static void TrackNewArrow(Actor* actor) {
    PlayState* play = gPlayState;
    EnArrow* arrow = (EnArrow*)actor;
    Sw97Element element = play == NULL ? SW97_ELEM_NONE : ElementOfNewArrow(arrow, GET_PLAYER(play));
    TrackedArrow* tracked = element == SW97_ELEM_NONE ? NULL : ClaimTrackedArrow();
    EffectBlureInit2* blure;

    if (tracked == NULL) {
        return;
    }
    memset(tracked, 0, sizeof(*tracked));
    tracked->arrow = arrow;
    tracked->element = element;
    tracked->isSeed = arrow->actor.params == ARROW_SEED;
    if (tracked->isSeed) {
        arrow->collider.info.toucher.dmgFlags = sElementDamage[element];
    }
    blure = tracked->isSeed ? NULL : ElementBlure(element);
    if (blure != NULL) {
        Effect_Add(play, &arrow->effectIndex, EFFECT_BLURE2, 0, 0, blure);
        tracked->hasBlure = true;
    }
    SpawnElementEffect(play, tracked);
}

// Vanilla respawns its own effect whenever the child is gone, so the SW97 one takes its place every time.
static void KeepElementEffect(PlayState* play, TrackedArrow* tracked) {
    Actor* child = tracked->arrow->actor.child;

    if (child != NULL && child->id == Sw97_GetArrowActorId(tracked->element)) {
        return;
    }
    if (child != NULL) {
        Actor_Kill(child);
        tracked->arrow->actor.child = NULL;
    }
    SpawnElementEffect(play, tracked);
}

static void AnnounceShot(PlayState* play, TrackedArrow* tracked) {
    Player* player = GET_PLAYER(play);

    if (tracked->isShot || tracked->arrow->actor.parent != NULL) {
        return;
    }
    tracked->isShot = true;
    if (!tracked->isSeed && tracked->element >= SW97_ELEM_DARK) {
        Player_PlaySfx(&player->actor, NA_SE_IT_MAGIC_ARROW_SHOT);
    }
}

// Vanilla zeroes a seed's rotation when it is shot; the SW97 cone orients itself off it, so it follows the arc.
static void AimSeedEffect(TrackedArrow* tracked) {
    Actor* seed = &tracked->arrow->actor;

    if (!tracked->isShot || tracked->arrow->actionFunc != EnArrow_Fly) {
        return;
    }
    seed->shape.rot.x = Math_Atan2S(seed->speedXZ, -seed->velocity.y);
    seed->shape.rot.y = seed->world.rot.y;
    seed->shape.rot.z = 0;
}

static void StartLifesteal(Actor* target) {
    Lifesteal* free = NULL;

    if (target == NULL || target->update == NULL ||
        (target->category != ACTORCAT_ENEMY && target->category != ACTORCAT_BOSS)) {
        return;
    }
    for (uint8_t i = 0; i < LIFESTEAL_SLOTS; i++) {
        if (sLifesteals[i].actor == target) {
            free = &sLifesteals[i];
            break;
        }
        if (free == NULL && sLifesteals[i].actor == NULL) {
            free = &sLifesteals[i];
        }
    }
    if (free == NULL) {
        return;
    }
    if (free->actor == NULL) {
        sLifestealCount++;
    }
    free->actor = target;
    free->remainingFrames = LIFESTEAL_TOTAL_FRAMES;
    free->tickPhase = LIFESTEAL_TICK_FRAMES;
}

static void UpdateTrackedArrow(Actor* actor) {
    PlayState* play = gPlayState;
    TrackedArrow* tracked = FindTrackedArrow(actor);

    if (play == NULL || tracked == NULL) {
        return;
    }
    KeepElementEffect(play, tracked);
    AnnounceShot(play, tracked);
    if (tracked->isSeed) {
        AimSeedEffect(tracked);
    }
    if (tracked->element == SW97_ELEM_DARK && (tracked->arrow->hitFlags & 1)) {
        StartLifesteal(tracked->arrow->collider.base.at);
    }
}

static bool IsSamePoint(Vec3f* a, Vec3f* b) {
    return a->x == b->x && a->y == b->y && a->z == b->z;
}

// Runs right after EnArrow_Draw, while the arrow's own matrix is still current: vanilla's trail samples the
// same two points there.
static void AddTrailVertex(Actor* actor, PlayState* play) {
    TrackedArrow* tracked = FindTrackedArrow(actor);
    Vec3f tip;
    Vec3f base;

    if (tracked == NULL || !tracked->hasBlure || tracked->arrow->actionFunc != EnArrow_Fly) {
        return;
    }
    Matrix_MultVec3f(&sArrowTip, &tip);
    Matrix_MultVec3f(&sArrowBase, &base);
    if (IsSamePoint(&tip, &tracked->lastTip) && IsSamePoint(&base, &tracked->lastBase)) {
        return;
    }
    tracked->lastTip = tip;
    tracked->lastBase = base;
    EffectBlure_AddVertex(Effect_GetByIndex(tracked->arrow->effectIndex), &tip, &base);
}

static void ForgetTrackedArrow(Actor* actor) {
    TrackedArrow* tracked = FindTrackedArrow(actor);

    if (tracked == NULL) {
        return;
    }
    if (tracked->hasBlure && gPlayState != NULL) {
        Effect_Delete(gPlayState, tracked->arrow->effectIndex);
    }
    tracked->arrow = NULL;
}

static void Drain(Lifesteal* lifesteal) {
    Actor* target = lifesteal->actor;
    s16 drain = MIN(target->colChkInfo.health, LIFESTEAL_DAMAGE_PER_TICK);

    target->colChkInfo.health -= drain;
    // Drained to nothing, the actor's own update has to see a lethal hit to run its death.
    if (target->colChkInfo.health <= 0) {
        target->colChkInfo.health = 0;
        target->colChkInfo.damage = 8;
        target->colChkInfo.damageEffect = 0;
        target->colorFilterTimer = 0;
    }
    gSaveContext.health = MIN(gSaveContext.health + drain, gSaveContext.healthCapacity);
    Actor_SetColorFilter(target, 0x8000, 200, 0x2000, LIFESTEAL_TICK_FRAMES);
}

static void EndLifesteal(Lifesteal* lifesteal) {
    lifesteal->actor = NULL;
    sLifestealCount--;
}

// Ticked from the drained actor's own update, so a frozen or culled target stops bleeding too.
static void TickLifesteal(Actor* actor) {
    if (sLifestealCount == 0) {
        return;
    }
    for (uint8_t i = 0; i < LIFESTEAL_SLOTS; i++) {
        Lifesteal* lifesteal = &sLifesteals[i];

        if (lifesteal->actor != actor) {
            continue;
        }
        if (actor->update == NULL) {
            EndLifesteal(lifesteal);
            return;
        }
        if (--lifesteal->tickPhase <= 0) {
            lifesteal->tickPhase = LIFESTEAL_TICK_FRAMES;
            Drain(lifesteal);
        }
        if (--lifesteal->remainingFrames <= 0) {
            EndLifesteal(lifesteal);
        }
        return;
    }
}

static void OpenMudWallToIce(Actor* actor) {
    ((BgBreakwall*)actor)->collider.info.bumper.dmgFlags |= DMG_ARROW_ICE;
}

static SunSwitch* FindSunSwitch(Actor* actor) {
    for (uint8_t i = 0; i < SUN_SWITCH_SLOTS; i++) {
        if (sSunSwitches[i].actor == actor) {
            return &sSunSwitches[i];
        }
    }
    return NULL;
}

static void OpenSunSwitchToLight(Actor* actor) {
    ObjLightswitch* sunSwitch = (ObjLightswitch*)actor;
    SunSwitch* slot = FindSunSwitch(NULL);

    Collider_SetJntSph(gPlayState, &sunSwitch->collider, actor, &sSunSwitchLightInit, sunSwitch->colliderItems);
    Collider_UpdateSpheres(0, &sunSwitch->collider);
    if (slot != NULL) {
        slot->actor = actor;
        slot->isLitByArrow = false;
    }
}

static bool AreVanillaLightArrowsSunlit(void) {
    return CVarGetInteger(SW97_CVAR_SUNLIGHT_ARROWS, 0) || (IS_RANDO && Randomizer_GetSettingValue(RSK_SUNLIGHT_ARROWS));
}

static bool IsElementalLightShot(Actor* shot) {
    TrackedArrow* tracked = FindTrackedArrow(shot);

    return tracked != NULL && tracked->element == SW97_ELEM_LIGHT;
}

// Runs before the switch's own update, so a refused hit is gone by the time it looks: without the Sunlight Arrows
// option only the SW97 light shots wake it, while mirrors and real sunlight still do.
static void NoteSunSwitchHit(void* actor, bool* result) {
    ObjLightswitch* sunSwitch = (ObjLightswitch*)actor;
    SunSwitch* slot = FindSunSwitch((Actor*)actor);
    Actor* hitter = sunSwitch->collider.base.ac;

    if (slot == NULL || !(sunSwitch->collider.base.acFlags & AC_HIT) || hitter == NULL) {
        return;
    }
    if (hitter->id == ACTOR_EN_ARROW && !IsElementalLightShot(hitter) && !AreVanillaLightArrowsSunlit()) {
        sunSwitch->collider.base.acFlags &= ~AC_HIT;
        return;
    }
    slot->isLitByArrow = hitter->id == ACTOR_EN_ARROW;
}

static void KeepArrowLitSwitchOn(bool* should, va_list args) {
    ObjLightswitch* sunSwitch = va_arg(args, ObjLightswitch*);
    SunSwitch* slot = FindSunSwitch(&sunSwitch->actor);

    if (slot != NULL && slot->isLitByArrow) {
        *should = false;
    }
}

// An arrow only lends the sun for the visit: leaving the room puts the switch out again.
static void ForgetSunSwitch(Actor* actor) {
    SunSwitch* slot = FindSunSwitch(actor);
    s32 type = (actor->params >> 4) & 3;

    if (slot == NULL) {
        return;
    }
    if (slot->isLitByArrow && type != OBJLIGHTSWITCH_TYPE_BURN && actor->room != SUN_SWITCH_STAYS_LIT_ROOM) {
        Flags_UnsetSwitch(gPlayState, (actor->params >> 8) & 0x3F);
    }
    slot->actor = NULL;
}

static void ForgetArrowScene(void) {
    memset(sTrackedArrows, 0, sizeof(sTrackedArrows));
    memset(sLifesteals, 0, sizeof(sLifesteals));
    memset(sSunSwitches, 0, sizeof(sSunSwitches));
    sLifestealCount = 0;
}

void Sw97_RegisterArrows(void) {
    gSw97Api->RegisterVB(VB_PLAYER_ARROW_MAGIC_CONSUMPTION, WaiveElementalShotCost);
    gSw97Api->RegisterVB(VB_LIGHTSWITCH_OFF, KeepArrowLitSwitchOn);
    SOH_REGISTER_HOOK(gSw97Api, OnLoadFile, LoadElements);
    SOH_REGISTER_HOOK(gSw97Api, OnPlayDestroy, ForgetArrowScene);
    SOH_REGISTER_HOOK(gSw97Api, OnPlayerResolveItemAction, ResolveBowItemAction);
    SOH_REGISTER_HOOK(gSw97Api, OnActorUpdate, TickLifesteal);
    SOH_REGISTER_HOOK_FOR_ID(gSw97Api, OnActorInit, ACTOR_EN_ARROW, TrackNewArrow);
    SOH_REGISTER_HOOK_FOR_ID(gSw97Api, OnActorUpdate, ACTOR_EN_ARROW, UpdateTrackedArrow);
    SOH_REGISTER_HOOK_FOR_ID(gSw97Api, OnActorDrawEnd, ACTOR_EN_ARROW, AddTrailVertex);
    SOH_REGISTER_HOOK_FOR_ID(gSw97Api, OnActorDestroy, ACTOR_EN_ARROW, ForgetTrackedArrow);
    SOH_REGISTER_HOOK_FOR_ID(gSw97Api, OnActorInit, ACTOR_BG_BREAKWALL, OpenMudWallToIce);
    SOH_REGISTER_HOOK_FOR_ID(gSw97Api, OnActorInit, ACTOR_OBJ_LIGHTSWITCH, OpenSunSwitchToLight);
    SOH_REGISTER_HOOK_FOR_ID(gSw97Api, ShouldActorUpdate, ACTOR_OBJ_LIGHTSWITCH, NoteSunSwitchHit);
    SOH_REGISTER_HOOK_FOR_ID(gSw97Api, OnActorDestroy, ACTOR_OBJ_LIGHTSWITCH, ForgetSunSwitch);
}
