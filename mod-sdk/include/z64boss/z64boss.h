#ifndef UNBOUND_Z64BOSS_H
#define UNBOUND_Z64BOSS_H

#include <string.h>

#include "soh/ModApi/ModApi.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Door_Warp1/z_door_warp1.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#ifdef __cplusplus
}
#endif

// True while `key` holds the slot of the loaded scene: spawn the fight from OnSceneInit when this holds.
static inline bool Z64Boss_IsFightHere(const SOHModApi* api, const char* key, PlayState* play) {
    if (!SOH_MOD_API_HAS(api, GetSceneBoss) || play == NULL || key == NULL) {
        return false;
    }
    const char* owner = api->GetSceneBoss(play->sceneNum);
    return owner != NULL && strcmp(owner, key) == 0;
}

static inline int32_t Z64Boss_GetSceneSlot(const SOHModApi* api, PlayState* play) {
    for (int32_t slot = 0; slot < SOH_BOSS_SLOT_MAX; slot++) {
        if (api->GetBossSlotScene(slot) == play->sceneNum) {
            return slot;
        }
    }
    return -1;
}

static inline void Z64Boss_StartTransition(PlayState* play, int16_t entrance, uint8_t transitionType) {
    play->nextEntranceIndex = entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = transitionType;
}

// The credits start from the Chamber of Sages cutscene, loaded as child like Ganon's own ending.
static inline void Z64Boss_RollCredits(PlayState* play) {
    gSaveContext.nextCutsceneIndex = 0xFFF2;
    play->linkAgeOnLoad = LINK_AGE_CHILD;
    Z64Boss_StartTransition(play, ENTR_CHAMBER_OF_THE_SAGES_0, TRANS_TYPE_FADE_WHITE);
}

static inline void Z64Boss_SpawnRewards(PlayState* play, Actor* boss, int32_t slot, Vec3f warpPos, Vec3f heartPos) {
    if (GameInteractor_Should(VB_SPAWN_BLUE_WARP, true, boss)) {
        // Barinade's first warp is Ruto, who opens the real one; the blue warp alone skips the sapphire.
        const bool isRutoWarp = slot == SOH_BOSS_SLOT_BARINADE &&
                                !Flags_GetEventChkInf(EVENTCHKINF_USED_JABU_JABUS_BELLY_BLUE_WARP);
        const bool isChildSlot = (SOH_BOSS_SLOTS_CHILD & SOH_BOSS_SLOT_BIT(slot)) != 0;
        Actor_Spawn(&play->actorCtx, play, isRutoWarp ? ACTOR_EN_RU1 : ACTOR_DOOR_WARP1, warpPos.x, warpPos.y,
                    warpPos.z, 0, 0, 0, isRutoWarp ? 0 : (isChildSlot ? WARP_DUNGEON_CHILD : WARP_DUNGEON_ADULT));
    }
    if (GameInteractor_Should(VB_SPAWN_HEART_CONTAINER, true)) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_ITEM_B_HEART, heartPos.x, heartPos.y, heartPos.z, 0, 0, 0, 0);
    }
}

// Ends the fight as the replaced boss would: room clear, defeat hooks, then its blue warp and heart container,
// Ganondorf's tower collapse, or Ganon's credits.
static inline void Z64Boss_Finish(const SOHModApi* api, PlayState* play, Actor* boss, Vec3f warpPos,
                                  Vec3f heartPos) {
    if (!SOH_MOD_API_HAS(api, ReportBossDefeated) || play == NULL || boss == NULL) {
        return;
    }
    const int32_t slot = Z64Boss_GetSceneSlot(api, play);
    Flags_SetClear(play, play->roomCtx.curRoom.num);
    api->ReportBossDefeated(boss);
    switch (slot) {
        case -1:
            break;
        case SOH_BOSS_SLOT_GANONDORF:
            Z64Boss_StartTransition(play, ENTR_GANONS_TOWER_COLLAPSE_EXTERIOR_0, TRANS_TYPE_FADE_WHITE_FAST);
            break;
        case SOH_BOSS_SLOT_GANON:
            Z64Boss_RollCredits(play);
            break;
        default:
            Z64Boss_SpawnRewards(play, boss, slot, warpPos, heartPos);
            break;
    }
}

#endif
