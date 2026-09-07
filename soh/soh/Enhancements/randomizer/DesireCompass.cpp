// =============================================================================
// Quartz of Motion (OoT side) — sensor brain. See DesireCompass.h.
//
// Every tick we walk the live actor lists, resolve each actor to its check with
// OoT's existing actor->check index (Randomizer::GetCheckFromActor), and keep
// the nearest one that is still uncollected and matches the tracked category.
// From that single distance we drive both signals: the "something is here"
// indicator (any match loaded at all) and the proximity blip rate.
// =============================================================================

#include "DesireCompass.h"

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

#include "mods/nei_save.h"

#include <chrono>
#include <cmath>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
extern SaveContext gSaveContext;
}

namespace {

// -----------------------------------------------------------------------------
// Category membership
// -----------------------------------------------------------------------------

bool InRange(RandomizerGet rg, RandomizerGet lo, RandomizerGet hi) {
    return rg >= lo && rg <= hi;
}

bool IsBossSoul(RandomizerGet rg) {
    // OoT boss souls are contiguous, and MM's 5 boss souls lead the MM block.
    return InRange(rg, RG_GOHMA_SOUL, RG_GANON_SOUL) || InRange(rg, RG_MM_SOUL_GOHT, RG_MM_SOUL_TWINMOLD);
}

bool IsOtherSoul(RandomizerGet rg) {
    return InRange(rg, RG_DEATH_MOUNTAIN_CRATER_BEAN_SOUL, RG_ZORAS_RIVER_BEAN_SOUL) ||
           InRange(rg, RG_MM_SOUL_ALIEN, RG_MM_SOUL_WOLFOS);
}

bool IsTriforce(RandomizerGet rg) {
    return rg == RG_TRIFORCE || rg == RG_TRIFORCE_PIECE;
}

bool IsSkill(RandomizerGet rg) {
    return InRange(rg, RG_BRONZE_SCALE, RG_SPEAK_ZORA) || InRange(rg, RG_OCARINA_A_BUTTON, RG_OCARINA_C_RIGHT_BUTTON);
}

bool IsKeyType(const Rando::Item& item) {
    ItemType t = item.GetItemType();
    return t == ITEMTYPE_SMALLKEY || t == ITEMTYPE_BOSSKEY || t == ITEMTYPE_FORTRESS_SMALLKEY;
}

bool ItemMatchesCategory(RandomizerGet rg, DesireCompassCategory cat, s32 subcat) {
    if (rg == RG_NONE || rg == RG_MAX) {
        return false;
    }
    Rando::Item& item = Rando::StaticData::RetrieveItem(rg);

    switch (cat) {
        case DCOMPASS_CAT_BOSS_SOULS:
            return IsBossSoul(rg);
        case DCOMPASS_CAT_OTHER_SOULS:
            return IsOtherSoul(rg);
        case DCOMPASS_CAT_TRIFORCE:
            return IsTriforce(rg);
        case DCOMPASS_CAT_SKILLS:
            return IsSkill(rg);
        case DCOMPASS_CAT_KEYS: {
            ItemType t = item.GetItemType();
            if (subcat == 1) {
                return t == ITEMTYPE_SMALLKEY || t == ITEMTYPE_FORTRESS_SMALLKEY;
            }
            if (subcat == 2) {
                return t == ITEMTYPE_BOSSKEY;
            }
            return IsKeyType(item);
        }
        case DCOMPASS_CAT_JUNK: {
            ItemType t = item.GetItemType();
            return item.GetCategory() == ITEM_CATEGORY_JUNK || t == ITEMTYPE_REFILL || t == ITEMTYPE_DROP;
        }
        case DCOMPASS_CAT_MAJOR:
            return item.IsMajorItem() && !IsBossSoul(rg) && !IsOtherSoul(rg) && !IsTriforce(rg) && !IsSkill(rg) &&
                   !IsKeyType(item);
        case DCOMPASS_CAT_OTHER: {
            if (IsBossSoul(rg) || IsOtherSoul(rg) || IsTriforce(rg) || IsSkill(rg) || IsKeyType(item)) {
                return false;
            }
            ItemType t = item.GetItemType();
            if (item.GetCategory() == ITEM_CATEGORY_JUNK || t == ITEMTYPE_REFILL || t == ITEMTYPE_DROP) {
                return false;
            }
            return !item.IsMajorItem();
        }
        default:
            return false;
    }
}

bool CheckIsOutstanding(RandomizerCheck rc, RandomizerGet* outRg) {
    if (rc == RC_UNKNOWN_CHECK || rc == RC_MAX) {
        return false;
    }
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return false;
    }
    Rando::ItemLocation* itemLoc = ctx->GetItemLocation(rc);
    if (itemLoc == nullptr) {
        return false;
    }
    RandomizerCheckStatus status = itemLoc->GetCheckStatus();
    if (status == RCSHOW_COLLECTED || status == RCSHOW_SAVED) {
        return false;
    }
    if (outRg != nullptr) {
        *outRg = itemLoc->GetPlacedRandomizerGet();
    }
    return true;
}

// -----------------------------------------------------------------------------
// Session state
// -----------------------------------------------------------------------------

// Wall-clock deadline: real time keeps "5 minutes" honest regardless of frame
// rate. 0 = inactive.
s64 sDeadlineMs = 0;
DesireCompassCategory sActiveCat = DCOMPASS_CAT_BOSS_SOULS;
s32 sActiveSubcat = DCOMPASS_SUBCAT_ANY;

bool sRoomHasTarget = false; // something of the category is loaded right now
f32 sProximity = 0.0f;       // 0 = far/none, 1 = on top of it
s64 sNextBlipMs = 0;

// "You walked into a room that has something" flash.
s32 sRoomAlertFrames = 0;
s8 sLastRoomNum = -1;
bool sLastRoomHadTarget = false;

// Queued activation, consumed by the tick once gameplay resumes. -1 = none.
s32 sPendingCat = -1;
s32 sPendingSubcat = DCOMPASS_SUBCAT_ANY;
s32 sAttuneTimer = 0;
constexpr s32 kAttuneFrames = 40;

// Distance at which proximity reads as "right here". Beyond kFarDist it is 0.
constexpr f32 kNearDist = 150.0f;
constexpr f32 kFarDist = 1200.0f;

inline s64 NowMs() {
    using namespace std::chrono;
    return (s64)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

const char* kCategoryNames[DCOMPASS_CAT_MAX] = {
    "Boss Souls", "Keys", "Other Souls", "Major Items", "Skills", "Junk", "Triforce", "Other",
};

void SpawnAttuneSparkles(Player* p) {
    Vec3f accel = { 0.0f, 0.05f, 0.0f };
    Color_RGBA8 primColor = { 180, 120, 255, 255 };
    Color_RGBA8 envColor = { 80, 40, 200, 255 };

    for (u8 i = 0; i < 3; i++) {
        s16 angle = (s16)(Rand_ZeroOne() * 0xFFFF);
        f32 dist = 15.0f + Rand_ZeroOne() * 25.0f;

        Vec3f pos;
        pos.x = p->actor.world.pos.x + Math_SinS(angle) * dist;
        pos.y = p->actor.world.pos.y + 20.0f + Rand_CenteredFloat(40.0f);
        pos.z = p->actor.world.pos.z + Math_CosS(angle) * dist;

        Vec3f vel;
        vel.x = Math_SinS(angle) * 0.3f;
        vel.y = 1.5f + Rand_ZeroOne() * 1.0f;
        vel.z = Math_CosS(angle) * 0.3f;

        EffectSsKiraKira_SpawnFocused(gPlayState, &pos, &vel, &accel, &primColor, &envColor, 500, 18);
    }
}

// Most "scattered" check types encode the carrier's world X and Z straight into
// their actorParams via TWO_ACTOR_PARAMS(x, z) — see randomizerTypes.h:10 and
// the Location::Pot/Grass/Crate/... factories. That lets us know where a check
// is WITHOUT its actor being loaded, which is what makes the sensor generic:
// grass, pots, crates, bushes, rocks, trees, wonder items, icicles, fairies and
// friends all report a position even from another room.
bool RcTypeEncodesPosition(RandomizerCheckType t) {
    switch (t) {
        case RCTYPE_POT:
        case RCTYPE_GRASS:
        case RCTYPE_CRATE:
        case RCTYPE_SMALL_CRATE:
        case RCTYPE_WONDER_ITEM:
        case RCTYPE_BOULDER:
        case RCTYPE_ICICLE:
        case RCTYPE_ROCK:
        case RCTYPE_SIGN:
        case RCTYPE_BUSH:
        case RCTYPE_TREE:
        case RCTYPE_RED_ICE:
        case RCTYPE_STONE_FAIRY:
        case RCTYPE_FOUNTAIN_FAIRY:
        case RCTYPE_BEAN_FAIRY:
        case RCTYPE_SONG_FAIRY:
        case RCTYPE_BUTTERFLY_FAIRY:
        case RCTYPE_BEEHIVE:
        case RCTYPE_FREESTANDING:
            return true;
        default:
            return false;
    }
}

// Decode TWO_ACTOR_PARAMS back into world X/Z (both halves are signed s16).
void DecodeParamsXZ(int32_t params, f32* outX, f32* outZ) {
    *outX = (f32)(s16)((params >> 16) & 0xFFFF);
    *outZ = (f32)(s16)(params & 0xFFFF);
}

// Nearest uncollected check of the category, by any means available:
//   (a) every outstanding check in this SCENE whose type encodes its position,
//       whether or not its actor is loaded, and
//   (b) any loaded actor that resolves through the actor->check index (chests,
//       skulltulas, NPC-held checks, ...).
// Returns false only when the scene genuinely holds nothing of that kind.
bool FindNearestLoaded(DesireCompassCategory cat, s32 subcat, f32* outDist) {
    if (gPlayState == nullptr || OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandomizer == nullptr) {
        return false;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return false;
    }
    Vec3f playerPos = player->actor.world.pos;
    const s32 curScene = (s32)gPlayState->sceneNum;

    f32 bestDistSq = -1.0f;

    // (a) Position-encoded checks anywhere in this scene.
    for (size_t i = 0; i < RC_MAX; i++) {
        RandomizerCheck rc = static_cast<RandomizerCheck>(i);
        RandomizerGet rg = RG_NONE;
        if (!CheckIsOutstanding(rc, &rg) || !ItemMatchesCategory(rg, cat, subcat)) {
            continue;
        }
        Rando::Location* loc = Rando::StaticData::GetLocation(rc);
        if (loc == nullptr || (s32)loc->GetScene() != curScene || !RcTypeEncodesPosition(loc->GetRCType())) {
            continue;
        }
        f32 lx, lz;
        DecodeParamsXZ(loc->GetActorParams(), &lx, &lz);
        f32 dx = lx - playerPos.x;
        f32 dz = lz - playerPos.z;
        f32 distSq = dx * dx + dz * dz;
        if (bestDistSq < 0.0f || distSq < bestDistSq) {
            bestDistSq = distSq;
        }
    }

    // (b) Anything else that happens to be loaded (chests, tokens, NPCs...).
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        Actor* actor = gPlayState->actorCtx.actorLists[category].head;
        while (actor != nullptr) {
            RandomizerCheck rc =
                OTRGlobals::Instance->gRandomizer->GetCheckFromActor(actor->id, gPlayState->sceneNum, actor->params);
            RandomizerGet rg = RG_NONE;
            if (rc != RC_UNKNOWN_CHECK && CheckIsOutstanding(rc, &rg) && ItemMatchesCategory(rg, cat, subcat)) {
                f32 dx = actor->world.pos.x - playerPos.x;
                f32 dz = actor->world.pos.z - playerPos.z;
                f32 distSq = dx * dx + dz * dz;
                if (bestDistSq < 0.0f || distSq < bestDistSq) {
                    bestDistSq = distSq;
                }
            }
            actor = actor->next;
        }
    }
    if (bestDistSq < 0.0f) {
        return false;
    }
    if (outDist != nullptr) {
        *outDist = sqrtf(bestDistSq);
    }
    return true;
}

void ResetSignals() {
    sRoomHasTarget = false;
    sProximity = 0.0f;
    sRoomAlertFrames = 0;
    sLastRoomNum = -1;
    sLastRoomHadTarget = false;
}

} // namespace

// =============================================================================
// Public C API
// =============================================================================

extern "C" const char* Rando_DesireCompass_CategoryName(DesireCompassCategory cat) {
    if (cat < 0 || cat >= DCOMPASS_CAT_MAX) {
        return "?";
    }
    return kCategoryNames[cat];
}

extern "C" s32 Rando_DesireCompass_SubcategoryCount(DesireCompassCategory cat) {
    return (cat == DCOMPASS_CAT_KEYS) ? 2 : 1;
}

extern "C" u8 Rando_DesireCompass_IsAvailable(void) {
    return IS_RANDO ? 1 : 0;
}

extern "C" u8 Rando_DesireCompass_IsOwned(void) {
    NeiSaveData* nei = Nei_Save();
    return (nei != nullptr && nei->quartzOwned) ? 1 : 0;
}

extern "C" s32 Rando_DesireCompass_CountRemaining(DesireCompassCategory cat, s32 subcat) {
    if (!IS_RANDO || cat < 0 || cat >= DCOMPASS_CAT_MAX) {
        return 0;
    }
    if (Rando::Context::GetInstance() == nullptr) {
        return 0;
    }
    s32 count = 0;
    for (size_t i = 0; i < RC_MAX; i++) {
        RandomizerCheck rc = static_cast<RandomizerCheck>(i);
        RandomizerGet rg = RG_NONE;
        if (CheckIsOutstanding(rc, &rg) && ItemMatchesCategory(rg, cat, subcat)) {
            count++;
        }
    }
    return count;
}

extern "C" u8 Rando_DesireCompass_RequestActivation(DesireCompassCategory cat, s32 subcat) {
    if (!IS_RANDO || cat < 0 || cat >= DCOMPASS_CAT_MAX) {
        return 0;
    }
    if (!Rando_DesireCompass_IsOwned()) {
        return 0;
    }
    // Must survive the 3 hearts. Checked BEFORE the menu closes so a refusal can
    // keep the list open instead of silently doing nothing after the fade-out.
    if (gSaveContext.health <= DCOMPASS_HEALTH_COST) {
        return 0;
    }
    sPendingCat = (s32)cat;
    sPendingSubcat = subcat;

    NeiSaveData* nei = Nei_Save();
    if (nei != nullptr) {
        nei->quartzCategory = (uint8_t)cat;
        nei->quartzSubcat = (uint8_t)subcat;
    }
    return 1;
}

extern "C" void Rando_DesireCompass_Cancel(void) {
    sDeadlineMs = 0;
    sPendingCat = -1;
    sAttuneTimer = 0;
    ResetSignals();
}

extern "C" u8 Rando_DesireCompass_IsAttuning(void) {
    return (sAttuneTimer > 0) ? 1 : 0;
}

extern "C" u8 Rando_DesireCompass_IsActive(void) {
    return (sDeadlineMs != 0 && NowMs() < sDeadlineMs) ? 1 : 0;
}

extern "C" s32 Rando_DesireCompass_GetRemainingSeconds(void) {
    if (sDeadlineMs == 0) {
        return 0;
    }
    s64 remainMs = sDeadlineMs - NowMs();
    if (remainMs <= 0) {
        return 0;
    }
    return (s32)((remainMs + 999) / 1000);
}

extern "C" DesireCompassCategory Rando_DesireCompass_GetActiveCategory(void) {
    return sActiveCat;
}

extern "C" u8 Rando_DesireCompass_RoomHasTarget(void) {
    return sRoomHasTarget ? 1 : 0;
}

extern "C" s32 Rando_DesireCompass_RoomAlertFrames(void) {
    return sRoomAlertFrames;
}

extern "C" f32 Rando_DesireCompass_GetProximity(void) {
    return sProximity;
}

// =============================================================================
// Per-frame tick. The Quartz is not an equippable item, so this hook is its
// only heartbeat.
// =============================================================================

namespace {

// Attuning: runs once the pause menu is gone. Locks Link, spins up sparkles and
// rumble, then spends the 3 hearts and starts the sensor.
void AttuneTick() {
    if (gPlayState == nullptr || gPlayState->pauseCtx.state != 0) {
        return; // wait until the menu has actually closed
    }
    Player* p = GET_PLAYER(gPlayState);
    if (p == nullptr) {
        return;
    }

    if (sAttuneTimer == 0) { // first gameplay frame after confirming
        sAttuneTimer = kAttuneFrames;
        Audio_PlaySoundGeneral(NA_SE_SY_TRE_BOX_APPEAR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }

    p->stateFlags1 |= PLAYER_STATE1_IN_ITEM_CS;
    p->linearVelocity = 0.0f;

    if ((sAttuneTimer % 3) == 0) {
        SpawnAttuneSparkles(p);
    }
    if ((sAttuneTimer % 6) == 0) {
        Rumble_Request(50.0f, 80, 8, 4);
    }

    sAttuneTimer--;
    if (sAttuneTimer > 0) {
        return;
    }

    p->stateFlags1 &= ~PLAYER_STATE1_IN_ITEM_CS;
    const DesireCompassCategory cat = (DesireCompassCategory)sPendingCat;
    const s32 subcat = sPendingSubcat;
    sPendingCat = -1;

    // Health could have dropped while attuning; refuse rather than kill.
    if (gSaveContext.health <= DCOMPASS_HEALTH_COST) {
        Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    gSaveContext.health -= DCOMPASS_HEALTH_COST;

    sActiveCat = cat;
    sActiveSubcat = subcat;
    sDeadlineMs = NowMs() + (s64)DCOMPASS_DURATION_SECONDS * 1000;
    sNextBlipMs = 0;
    ResetSignals();

    Audio_PlaySoundGeneral(NA_SE_SY_CORRECT_CHIME, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

void SensorTick() {
    if (sPendingCat >= 0) {
        AttuneTick();
        return;
    }
    if (sDeadlineMs == 0) {
        return;
    }
    if (NowMs() >= sDeadlineMs) { // expired
        sDeadlineMs = 0;
        ResetSignals();
        return;
    }
    if (gPlayState == nullptr || gPlayState->pauseCtx.state != 0) {
        return; // don't scan while paused
    }

    if (sRoomAlertFrames > 0) {
        sRoomAlertFrames--;
    }

    f32 dist = 0.0f;
    sRoomHasTarget = FindNearestLoaded(sActiveCat, sActiveSubcat, &dist);

    // Signal 1: entering a room that holds something announces itself once.
    const s8 room = gPlayState->roomCtx.curRoom.num;
    if (room != sLastRoomNum) {
        sLastRoomNum = room;
        sLastRoomHadTarget = false;
    }
    if (sRoomHasTarget && !sLastRoomHadTarget) {
        sLastRoomHadTarget = true;
        sRoomAlertFrames = 90; // ~1.5 s of on-screen indicator
        Audio_PlaySoundGeneral(NA_SE_SY_ATTENTION_ON, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        Rumble_Request(120.0f, 150, 20, 40);
    }

    if (!sRoomHasTarget) {
        sProximity = 0.0f;
        return;
    }

    // Signal 2: hot/cold. Proximity 0..1, blip rate and pitch follow it.
    f32 t = (kFarDist - dist) / (kFarDist - kNearDist);
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }
    sProximity = t;

    const s64 periodMs = (s64)(900.0f - 780.0f * t); // 900 ms far -> 120 ms close
    const s64 now = NowMs();
    if (now >= sNextBlipMs) {
        // The urgent variant once you are basically on top of it.
        Audio_PlaySoundGeneral(t > 0.85f ? NA_SE_SY_WARNING_COUNT_E : NA_SE_SY_WARNING_COUNT_N, &gSfxDefaultPos, 4,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        Rumble_Request(50.0f, (u8)(60 + 120 * t), 8, 4);
        sNextBlipMs = now + periodMs;
    }
}

void RegisterDesireCompassTick() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([]() { SensorTick(); });
}

static RegisterShipInitFunc dcTickInitFunc(RegisterDesireCompassTick, {});

} // namespace
