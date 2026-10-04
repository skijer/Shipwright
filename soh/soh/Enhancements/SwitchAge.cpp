#include "soh/Enhancements/SwitchAge.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include <z64.h>
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "mods/extended_equipment.h"
#include "mods/nei_save.h"

extern SaveContext gSaveContext;
extern PlayState* gPlayState;
}

static PlayState* sNonLogicAgeSwitchPlay = nullptr;
static s32 sNonLogicSourceAge;
static s32 sNonLogicEntrance;

/// Switches Link's age using the existing local respawn contract.
static void SwitchAgeInternal(bool updateProgression) {
    if (gPlayState == NULL)
        return;

    Player* player = GET_PLAYER(gPlayState);

    // Hyrule Castle: Very likely to fall through floor, so we force a specific entrance
    if (gPlayState->sceneNum == SCENE_HYRULE_CASTLE || gPlayState->sceneNum == SCENE_OUTSIDE_GANONS_CASTLE) {
        gPlayState->nextEntranceIndex = ENTR_CASTLE_GROUNDS_SOUTH_EXIT;
    } else {
        gSaveContext.respawnFlag = 1;
        gPlayState->nextEntranceIndex = gSaveContext.entranceIndex;

        // Preserve the player's position and orientation
        gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = gPlayState->nextEntranceIndex;
        gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = gPlayState->roomCtx.curRoom.num;
        gSaveContext.respawn[RESPAWN_MODE_DOWN].pos = player->actor.world.pos;
        gSaveContext.respawn[RESPAWN_MODE_DOWN].yaw = player->actor.shape.rot.y;

        if (gPlayState->roomCtx.curRoom.behaviorType2 < 4) {
            gSaveContext.respawn[RESPAWN_MODE_DOWN].playerParams = 0x0DFF;
        } else {
            // Scenes with static backgrounds use a special camera we need to preserve
            Camera* camera = GET_ACTIVE_CAM(gPlayState);
            s16 camId = camera->camDataIdx;
            gSaveContext.respawn[RESPAWN_MODE_DOWN].playerParams = 0x0D00 | camId;
        }
    }

    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_INSTANT;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_FAST;
    gPlayState->linkAgeOnLoad ^= 1;

    if (updateProgression) {
        // Preserve existing Time Gate, menu and ocarina behavior.
        if (gPlayState->linkAgeOnLoad == LINK_AGE_ADULT) {
            Entrance_SetEntranceDiscovered(ENTR_HYRULE_FIELD_10, false);
        } else {
            Entrance_SetEntranceDiscovered(ENTR_LINKS_HOUSE_CHILD_SPAWN, false);
        }
    }

    // If paused, restore things as if unpausing
    if (gPlayState->pauseCtx.state != 0) {
        // Restore A button enabled alpha (disabled if changing on item/equip subscreen, difficult to get re-enable)
        gSaveContext.buttonStatus[4] = 0;
    }

    static HOOK_ID hookId = 0;
    hookId = REGISTER_VB_SHOULD(VB_INFLICT_VOID_DAMAGE, {
        *should = false;
        GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::OnVanillaBehavior>(hookId);
    });
}

void SwitchAge(void) {
    sNonLogicAgeSwitchPlay = nullptr;
    SwitchAgeInternal(true);
}

void SwitchAgeWithoutProgression(void) {
    if (gPlayState == nullptr || !gPlayState->state.running || GET_PLAYER(gPlayState) == nullptr ||
        gPlayState->transitionTrigger == TRANS_TRIGGER_START) {
        return;
    }
    sNonLogicAgeSwitchPlay = gPlayState;
    sNonLogicSourceAge = gSaveContext.linkAge;
    sNonLogicEntrance = gSaveContext.entranceIndex;
    SwitchAgeInternal(false);
    gSaveContext.respawn[RESPAWN_MODE_DOWN].tempSwchFlags = gPlayState->actorCtx.flags.tempSwch;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].tempCollectFlags = gPlayState->actorCtx.flags.tempCollect;
}

int SwitchAge_HandleNonLogicEquipmentSwap(void) {
    bool pending = sNonLogicAgeSwitchPlay != nullptr && sNonLogicAgeSwitchPlay == gPlayState;
    sNonLogicAgeSwitchPlay = nullptr;
    if (!pending || gSaveContext.linkAge != sNonLogicSourceAge ||
        gPlayState->linkAgeOnLoad != (sNonLogicSourceAge ^ 1) || gSaveContext.respawnFlag != 1 ||
        gPlayState->nextEntranceIndex != sNonLogicEntrance) {
        return false;
    }
    Nei_Save()->timePedestalNoMasterSwordRepair = !CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER);

    // The vanilla/rando handoff assumes the story sword was obtained and can
    // equip it (or rewrite permanent swordless flags). Save the real loadout,
    // restore the other age's loadout when initialized, and validate ownership.
    ItemEquips* source = LINK_IS_ADULT ? &gSaveContext.adultEquips : &gSaveContext.childEquips;
    const ItemEquips* target = LINK_IS_ADULT ? &gSaveContext.childEquips : &gSaveContext.adultEquips;
    *source = gSaveContext.equips;
    if (target->equipment != 0) {
        gSaveContext.equips = *target;
    }
    bool becomingAdult = gPlayState->linkAgeOnLoad == LINK_AGE_ADULT;

    for (s16 type = EQUIP_TYPE_SWORD; type <= EQUIP_TYPE_BOOTS; type++) {
        u16 value = (gSaveContext.equips.equipment >> (type * 4)) & 0xF;
        bool owned = value >= 1 && value <= 3 && CHECK_OWNED_EQUIP(type, value - 1);
        bool ageAllowed = true;
        if (type == EQUIP_TYPE_SWORD) {
            ageAllowed = becomingAdult ? value != EQUIP_VALUE_SWORD_KOKIRI : value == EQUIP_VALUE_SWORD_KOKIRI;
        } else if (type == EQUIP_TYPE_SHIELD) {
            ageAllowed = becomingAdult ? value != EQUIP_VALUE_SHIELD_DEKU : value != EQUIP_VALUE_SHIELD_MIRROR;
        } else if (!becomingAdult) {
            ageAllowed = value == 1;
        }
        if (!owned || !ageAllowed) {
            value = 0;
            if (type == EQUIP_TYPE_SWORD) {
                u16 preferred = becomingAdult ? EQUIP_VALUE_SWORD_MASTER : EQUIP_VALUE_SWORD_KOKIRI;
                if (CHECK_OWNED_EQUIP(type, preferred - 1)) {
                    value = preferred;
                } else if (becomingAdult && CHECK_OWNED_EQUIP(type, EQUIP_INV_SWORD_BIGGORON)) {
                    value = EQUIP_VALUE_SWORD_BIGGORON;
                }
            } else if (type == EQUIP_TYPE_TUNIC || type == EQUIP_TYPE_BOOTS) {
                value = 1; // The base tunic/boots are necessary player model defaults, not grants.
            }
            gSaveContext.equips.equipment =
                (gSaveContext.equips.equipment & ~(0xF << (type * 4))) | (value << (type * 4));
        }
    }
    static const u8 swordItems[] = { ITEM_NONE, ITEM_SWORD_KOKIRI, ITEM_SWORD_MASTER, ITEM_SWORD_BGS };
    u16 sword = gSaveContext.equips.equipment & 0xF;
    gSaveContext.equips.buttonItems[0] = swordItems[sword];
    if (sword == EQUIP_VALUE_SWORD_BIGGORON && CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
        gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KNIFE;
    }
    // Refresh saved bottle/trade slots and remove stale unowned vanilla items;
    // custom inventory slots retain their own established validation path.
    for (size_t button = 1; button < ARRAY_COUNT(gSaveContext.equips.buttonItems); button++) {
        u8 slot = gSaveContext.equips.cButtonSlots[button - 1];
        if (slot < ARRAY_COUNT(gSaveContext.inventory.items)) {
            gSaveContext.equips.buttonItems[button] = gSaveContext.inventory.items[slot];
        }
    }
    ExtEquip_ValidateForAgeWithoutProgression(gPlayState->linkAgeOnLoad);
    return true;
}
