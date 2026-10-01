// The three spiritual stones as C-button items: each gives a passive speed buff while owned, and holding its
// button plants an owl statue that a tap later warps back to. Split out of useful_medallions unchanged.

#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define MOD_NAME "spiritual_stones"
#define STATUE_ACTOR_KEY "spiritual_stones.statue"
#define STORAGE_FIELD "warps"

#define BUTTON_COUNT 8
#define STONE_HOLD_FRAMES 60
#define STATUE_SCALE 0.005f
#define STATUE_GLOW_HEIGHT 62.0f
#define STATUE_GLOW_SCALE 0.006f
#define STONE_WALK_BOOST 1.5f
#define STONE_SWIM_BOOST 2.0f
#define STONE_CLIMB_BOOST 2.0f

typedef enum {
    STONE_KOKIRI,
    STONE_GORON,
    STONE_ZORA,
    STONE_MAX,
} Stone;

typedef struct {
    const char* key;
    const char* icon;
    const char* name;
    uint8_t questItem;
    const char* getItemText;
    const char* pauseText;
} StoneInfo;

typedef struct {
    int32_t entranceIndex;
    int16_t sceneNum;
    int16_t rotY;
    int8_t roomIndex;
    Vec3f pos;
} StoneWarp;

typedef struct {
    StoneWarp warp[STONE_MAX];
} SavedState;

static const ALIGN_ASSET(2) char sStoneIcons[STONE_MAX][64] = {
    "__OTR__textures/icon_item_custom/gItemIconKokiriEmeraldTex",
    "__OTR__textures/icon_item_custom/gItemIconGoronRubyTex",
    "__OTR__textures/icon_item_custom/gItemIconZoraSapphireTex",
};

static const ALIGN_ASSET(2) char sStoneNames[STONE_MAX][72] = {
    "__OTR__textures/item_name_static/gKokiriEmeraldItemNameENGTex",
    "__OTR__textures/item_name_static/gGoronsRubyItemNameENGTex",
    "__OTR__textures/item_name_static/gZorasSapphireItemNameENGTex",
};

static const ALIGN_ASSET(2) char sFlashDL[] = "__OTR__objects/gameplay_keep/gEffFlash1DL";
static const ALIGN_ASSET(2) char sOwlStatueDL[] = "__OTR__objects/object_sek/gOwlStatueOpenedDL";

// MM display lists branch into segment 0x0C for a cull list OoT never binds, so it is pointed at a dead end.
static const Gfx sEmptySegment[4] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

static const StoneInfo sStones[STONE_MAX] = {
    { "nei.spiritual_stone.kokiri", sStoneIcons[STONE_KOKIRI], sStoneNames[STONE_KOKIRI], QUEST_KOKIRI_EMERALD,
      "You got the %gKokiri's Emerald%w!&The forest quickens your step&while you carry it.^%rHold%w %y\xA1%w to "
      "plant an owl statue&here, and press %y\xA1%w to return to it.",
      "%gKokiri's Emerald&%wYou walk faster. Hold %y\xA1%w to plant&a warp, press %y\xA1%w to return." },
    { "nei.spiritual_stone.goron", sStoneIcons[STONE_GORON], sStoneNames[STONE_GORON], QUEST_GORON_RUBY,
      "You got the %rGoron's Ruby%w!&The mountain steadies your climb&while you carry it.^%rHold%w %y\xA1%w to "
      "plant an owl statue&here, and press %y\xA1%w to return to it.",
      "%rGoron's Ruby&%wYou climb faster. Hold %y\xA1%w to plant&a warp, press %y\xA1%w to return." },
    { "nei.spiritual_stone.zora", sStoneIcons[STONE_ZORA], sStoneNames[STONE_ZORA], QUEST_ZORA_SAPPHIRE,
      "You got the %cZora's Sapphire%w!&The lake carries you while you&carry it.^%rHold%w %y\xA1%w to plant an owl "
      "statue&here, and press %y\xA1%w to return to it.",
      "%cZora's Sapphire&%wYou swim faster. Hold %y\xA1%w to plant&a warp, press %y\xA1%w to return." },
};

static const Color_RGB8 sStoneGlow[STONE_MAX] = {
    { 0, 255, 100 },
    { 255, 60, 60 },
    { 60, 160, 255 },
};

static const u16 sItemButtons[BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate",
    "OnSceneInit",
    "OnLoadFile",
    "OnSceneSpawnActors",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static SavedState sSaved;
static int16_t sStatueActorId = -1;
static bool sHasOwlStatue;
static uint32_t sOwnedQuestItems = 0xFFFFFFFF;
static bool sIsVoidDamageWaived;
static uint8_t sStoneHoldFrames[STONE_MAX];
static Actor* sStatues[STONE_MAX];
static int8_t sPendingWarpStone = -1;

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static bool IsPlayerBusy(Player* player, PlayState* play) {
    return player == NULL || play->msgCtx.msgMode != MSGMODE_NONE || play->pauseCtx.state != 0 ||
           (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_IN_ITEM_CS |
                                   PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM)) != 0;
}

static void StoreState(void) {
    sApi->StorageSet(MOD_NAME, STORAGE_FIELD, &sSaved, sizeof(sSaved));
}

static void LoadState(void) {
    memset(&sSaved, 0, sizeof(sSaved));
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        sSaved.warp[stone].entranceIndex = -1;
        sSaved.warp[stone].sceneNum = -1;
    }
    sApi->StorageGet(MOD_NAME, STORAGE_FIELD, &sSaved, sizeof(sSaved));
}

static uint16_t FindEquippedButtonMask(const char* key) {
    for (uint8_t button = 0; button < BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);

        if (equipped != NULL && strcmp(equipped, key) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

// Nothing in the item registry derives ownership from a quest flag, so the mod restates it as the flags move.
static void SyncQuestOwnership(void) {
    uint32_t questItems = gSaveContext.inventory.questItems;

    if (questItems == sOwnedQuestItems) {
        return;
    }
    sOwnedQuestItems = questItems;
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        sApi->SetCustomItemOwned(sStones[stone].key, CHECK_QUEST_ITEM(sStones[stone].questItem) != 0);
    }
}

static bool IsStoneWarpReady(uint8_t stone) {
    return sHasOwlStatue && sSaved.warp[stone].entranceIndex >= 0;
}

// Replanting moves the one waypoint that stone has, so the statue standing on the old spot goes with it.
static void SpawnStatue(PlayState* play, uint8_t stone) {
    const StoneWarp* warp = &sSaved.warp[stone];

    if (sStatueActorId < 0 || sStatues[stone] != NULL) {
        return;
    }
    sStatues[stone] = Actor_Spawn(&play->actorCtx, play, sStatueActorId, warp->pos.x, warp->pos.y, warp->pos.z, 0,
                                  warp->rotY, 0, stone);
}

static void RemoveStatue(uint8_t stone) {
    if (sStatues[stone] == NULL) {
        return;
    }
    if (sStatues[stone]->update != NULL) {
        Actor_Kill(sStatues[stone]);
    }
    sStatues[stone] = NULL;
}

static void PlantStatue(PlayState* play, Player* player, uint8_t stone) {
    StoneWarp* warp = &sSaved.warp[stone];

    warp->entranceIndex = gSaveContext.entranceIndex;
    warp->sceneNum = play->sceneNum;
    warp->roomIndex = play->roomCtx.curRoom.num;
    warp->rotY = player->actor.shape.rot.y;
    warp->pos = player->actor.world.pos;
    StoreState();
    RemoveStatue(stone);
    SpawnStatue(play, stone);
    PlaySfxAt(NA_SE_SY_GET_ITEM, &player->actor.world.pos);
}

// respawnFlag 1 is the "you fell in a hole" landing, the only one that honours a hand-written position,
// so the void damage it carries is waived once, for this transition alone.
static void WaiveVoidDamage(bool* should, va_list args) {
    if (!sIsVoidDamageWaived) {
        return;
    }
    sIsVoidDamageWaived = false;
    *should = false;
}

static void WarpToStatue(PlayState* play, uint8_t stone) {
    const StoneWarp* warp = &sSaved.warp[stone];

    play->nextEntranceIndex = warp->entranceIndex;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_FAST;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = warp->entranceIndex;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = warp->roomIndex;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].pos = warp->pos;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].yaw = warp->rotY;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].playerParams = 0xDFF;
    gSaveContext.respawnFlag = 1;
    sIsVoidDamageWaived = true;
}

static void ResolveWarpPrompt(Player* player, PlayState* play) {
    uint8_t stone;

    if (sPendingWarpStone < 0 || Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE || !Message_ShouldAdvance(play)) {
        return;
    }
    stone = (uint8_t)sPendingWarpStone;
    Message_CloseTextbox(play);
    play->msgCtx.msgMode = MSGMODE_TEXT_DONE;
    if (play->msgCtx.choiceIndex == 0) {
        WarpToStatue(play, stone);
    }
    sPendingWarpStone = -1;
}

static void OpenWarpPrompt(PlayState* play, uint8_t stone) {
    if (!sApi->ShowTextbox(play, "Return to your waypoint?\x1B%g&&Yes&No%w", false)) {
        return;
    }
    sPendingWarpStone = (int8_t)stone;
}

static void TickStoneButtons(Player* player, PlayState* play) {
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        uint16_t button = FindEquippedButtonMask(sStones[stone].key);

        if (button == 0 || !CHECK_QUEST_ITEM(sStones[stone].questItem)) {
            sStoneHoldFrames[stone] = 0;
            continue;
        }
        if (play->state.input[0].cur.button & button) {
            if (sStoneHoldFrames[stone] < STONE_HOLD_FRAMES && ++sStoneHoldFrames[stone] == STONE_HOLD_FRAMES &&
                sHasOwlStatue) {
                PlantStatue(play, player, stone);
            }
            continue;
        }
        if (sStoneHoldFrames[stone] > 0 && sStoneHoldFrames[stone] < STONE_HOLD_FRAMES && sPendingWarpStone < 0 &&
            IsStoneWarpReady(stone)) {
            OpenWarpPrompt(play, stone);
        }
        sStoneHoldFrames[stone] = 0;
    }
}

// The hook runs once the player has already stepped his speed toward the stick, so the buff scales the
// result and speedXZ with it: the mover reads that one, not linearVelocity.
static void Rush(Player* player, float boost) {
    player->linearVelocity *= boost;
    player->actor.speedXZ = player->linearVelocity;
}

static void ApplyStoneBuffs(Player* player) {
    if (CHECK_QUEST_ITEM(QUEST_ZORA_SAPPHIRE) && (player->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
        Rush(player, STONE_SWIM_BOOST);
        return;
    }
    if (CHECK_QUEST_ITEM(QUEST_GORON_RUBY) && (player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER)) {
        player->skelAnime.playSpeed *= STONE_CLIMB_BOOST;
        return;
    }
    if (CHECK_QUEST_ITEM(QUEST_KOKIRI_EMERALD) && (player->actor.bgCheckFlags & 1)) {
        Rush(player, STONE_WALK_BOOST);
    }
}

static void StatueDraw(Actor* thisx, PlayState* play) {
    Color_RGB8 glow = sStoneGlow[thisx->params % STONE_MAX];

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sEmptySegment);
    Gfx_DrawDListOpa(play, (Gfx*)sOwlStatueDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, glow.r, glow.g, glow.b, 200);
    gDPSetEnvColor(POLY_XLU_DISP++, glow.r / 2, glow.g / 2, glow.b / 2, 255);
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y + STATUE_GLOW_HEIGHT, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD((int16_t)(play->gameplayFrames * 0x400)), MTXMODE_APPLY);
    Matrix_Scale(STATUE_GLOW_SCALE, STATUE_GLOW_SCALE, STATUE_GLOW_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sFlashDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void StatueInit(Actor* thisx, PlayState* play) {
    Actor_SetScale(thisx, STATUE_SCALE);
    thisx->gravity = 0.0f;
    thisx->shape.shadowDraw = NULL;
}

static void StatueUpdate(Actor* thisx, PlayState* play) {
}

static void PlantStatuesForScene(void) {
    PlayState* play = gPlayState;

    if (play == NULL) {
        return;
    }
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        if (IsStoneWarpReady(stone) && sSaved.warp[stone].sceneNum == play->sceneNum) {
            SpawnStatue(play, stone);
        }
    }
}

static void ForgetSceneState(int16_t sceneNum) {
    memset(sStoneHoldFrames, 0, sizeof(sStoneHoldFrames));
    memset(sStatues, 0, sizeof(sStatues));
    sPendingWarpStone = -1;
}

static void ReloadState(int32_t fileNum) {
    LoadState();
    sOwnedQuestItems = 0xFFFFFFFF;
}

static void TickMod(void) {
    PlayState* play = gPlayState;
    Player* player = play == NULL ? NULL : GET_PLAYER(play);

    if (player == NULL) {
        return;
    }
    SyncQuestOwnership();
    ResolveWarpPrompt(player, play);
    if (IsPlayerBusy(player, play)) {
        return;
    }
    TickStoneButtons(player, play);
    ApplyStoneBuffs(player);
}

static bool RegisterStatueActor(void) {
    SOHActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = STATUE_ACTOR_KEY;
    definition.description = "Spiritual Stone Owl Statue";
    definition.category = ACTORCAT_PROP;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = 0;
    definition.init = StatueInit;
    definition.update = StatueUpdate;
    definition.draw = StatueDraw;
    sStatueActorId = sApi->RegisterActor(&definition);
    return sStatueActorId >= 0;
}

static bool RegisterStone(Stone stone) {
    SOHCustomItemDefinition jewel = Z64Items_Define(sStones[stone].key, sStones[stone].icon, sStones[stone].name);

    Z64Items_SetButtons(&jewel, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&jewel, SOH_ITEM_PAGE_QUEST_VANILLA, (uint8_t)sStones[stone].questItem, 0);
    Z64Items_SetTextbox(&jewel, sStones[stone].getItemText);
    Z64Items_SetPauseText(&jewel, sStones[stone].pauseText);
    jewel.flags |= SOH_CUSTOM_ITEM_INSTANT;
    return Z64Items_Register(sApi, &jewel);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, HasResource)) {
        return;
    }
    // The owl statue is Majora's Mask art: without mm.o2r the buffs still work and the warps stay off.
    sHasOwlStatue = sApi->HasResource(sOwlStatueDL) != 0 && RegisterStatueActor();
    for (uint8_t stone = 0; stone < STONE_MAX; stone++) {
        if (!RegisterStone((Stone)stone)) {
            return;
        }
    }
    LoadState();
    sApi->RegisterVB(VB_INFLICT_VOID_DAMAGE, WaiveVoidDamage);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickMod);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    SOH_REGISTER_HOOK(sApi, OnLoadFile, ReloadState);
    SOH_REGISTER_HOOK(sApi, OnSceneSpawnActors, PlantStatuesForScene);
}
