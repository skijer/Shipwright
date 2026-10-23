// The spiritual stones retain independent passive buffs and C-button waypoint menus.

#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "regs.h"
#include "soh/ModApi/Player/PlayerHookTypes.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define MOD_NAME "spiritual_stones"
#define STATUE_ACTOR_KEY "spiritual_stones.statue"
#define STORAGE_FIELD "warps"

#define BUTTON_COUNT 8
#define STATUE_SCALE 0.005f
#define STATUE_JEWEL_HEIGHT 62.0f
#define STATUE_JEWEL_SCALE 0.10f
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

static const s16 sStoneDrawIds[STONE_MAX] = { GID_KOKIRI_EMERALD, GID_GORON_RUBY, GID_ZORA_SAPPHIRE };
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
      "You got the %gKokiri's Emerald%w!&The forest quickens your step&while you carry it.^"
      "Press %y\xA1%w to choose a new warp&point or return to your saved&owl statue.",
      "%gKokiri's Emerald&%wYou walk faster.&Press %y\xA1%w for the warp point menu." },
    { "nei.spiritual_stone.goron", sStoneIcons[STONE_GORON], sStoneNames[STONE_GORON], QUEST_GORON_RUBY,
      "You got the %rGoron's Ruby%w! The&mountain steadies your climb&while you carry it.^"
      "Press %y\xA1%w to choose a new warp&point or return to your saved&owl statue.",
      "%rGoron's Ruby&%wYou climb faster.&Press %y\xA1%w for the warp point menu." },
    { "nei.spiritual_stone.zora", sStoneIcons[STONE_ZORA], sStoneNames[STONE_ZORA], QUEST_ZORA_SAPPHIRE,
      "You got the %cZora's Sapphire%w!&The lake carries you while you&carry it.^"
      "Press %y\xA1%w to choose a new warp&point or return to your saved&owl statue.",
      "%cZora's Sapphire&%wYou swim faster.&Press %y\xA1%w for the warp point menu." },
};


static const u16 sItemButtons[BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate", "OnPlayerResolveMotionScale", "OnKaleidoInput", "OnPlayDrawBegin", "OnPlayDrawEnd",
    "OnGameFrameUpdate",
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
static Actor* sStatues[STONE_MAX];
static int8_t sPendingWarpStone = -1;
static u8 sPassiveDisabled;
// The vanilla quest renderer reads gItemIcons directly. Substitute only during the draw;
// display-list texture pointers retain their own stable OTR names after restoration.
static void* sQuestIconBackup[STONE_MAX];
static bool sQuestIconsPatched;
static const ALIGN_ASSET(2) char sPassiveOffIcons[STONE_MAX][80]={
    "__OTR__textures/icon_item_custom/gStonePassiveOff0Tex",
    "__OTR__textures/icon_item_custom/gStonePassiveOff1Tex",
    "__OTR__textures/icon_item_custom/gStonePassiveOff2Tex"
};
static void RestoreQuestIcons(void) {
    if(!sQuestIconsPatched)return;
    for(int stone=0;stone<STONE_MAX;++stone)gItemIcons[ITEM_KOKIRI_EMERALD+stone]=sQuestIconBackup[stone];
    sQuestIconsPatched=false;
}
static void DrawStoneState(void) {
    RestoreQuestIcons();
    if(!gPlayState || gPlayState->pauseCtx.state==0 || !KaleidoQuestManager_IsVanillaPageShown())return;
    for(int stone=0;stone<STONE_MAX;++stone) {
        sQuestIconBackup[stone]=gItemIcons[ITEM_KOKIRI_EMERALD+stone];
        if(sPassiveDisabled&(1<<stone))gItemIcons[ITEM_KOKIRI_EMERALD+stone]=(void*)sPassiveOffIcons[stone];
    }
    sQuestIconsPatched=true;
}
static void ToggleStonePassive(PlayState* play,Input* input,bool* handled) {
    if(*handled || !(input->press.button&BTN_A) || play->pauseCtx.pageIndex!=PAUSE_QUEST || !KaleidoQuestManager_IsVanillaPageShown())return;
    int stone=play->pauseCtx.cursorPoint[PAUSE_QUEST]-QUEST_KOKIRI_EMERALD;
    if(stone<0 || stone>=STONE_MAX || !CHECK_QUEST_ITEM(sStones[stone].questItem))return;
    sPassiveDisabled^=1<<stone;
    sApi->StorageSet(MOD_NAME,"passive_disabled",&sPassiveDisabled,sizeof(sPassiveDisabled));
    *handled=true;input->press.button&=~BTN_A;
    Audio_PlaySfxGeneral(NA_SE_SY_DECIDE,&gSfxDefaultPos,4,&gSfxDefaultFreqAndVolScale,&gSfxDefaultFreqAndVolScale,&gSfxDefaultReverb);
}

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySfxGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
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

// The vanilla Quest Status page has no custom-item cells for its three stones.
// Bind its real quest slots explicitly; A remains the independent passive toggle.
static void EquipStoneWarp(PlayState* play,Input* input,bool* handled) {
    if(*handled || play->pauseCtx.pageIndex!=PAUSE_QUEST || play->pauseCtx.cursorSpecialPos!=0 ||
       !KaleidoQuestManager_IsVanillaPageShown()) return;
    int stone=play->pauseCtx.cursorPoint[PAUSE_QUEST]-QUEST_KOKIRI_EMERALD;
    if(stone<0 || stone>=STONE_MAX || !CHECK_QUEST_ITEM(sStones[stone].questItem)) return;
    for(uint8_t button=1;button<BUTTON_COUNT;button++) {
        u16 mask=sItemButtons[button];
        if(!(input->press.button&mask) || (button>=4 && !CVarGetInteger("gEnhancements.DpadEquips",0))) continue;
        SyncQuestOwnership();
        if(sApi->EquipCustomItem(button,sStones[stone].key)) {
            *handled=true;input->press.button&=~mask;
            Audio_PlaySfxGeneral(NA_SE_SY_DECIDE,&gSfxDefaultPos,4,&gSfxDefaultFreqAndVolScale,
                &gSfxDefaultFreqAndVolScale,&gSfxDefaultReverb);
        }
        return;
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

// The native choice renderer has no disabled color. Draw that single label with
// its own native font glyphs, without changing the player's global text colors.
void Message_DrawTextChar(PlayState* play, void* textureImage, Gfx** p);
void Message_SetView(View* view);
#ifdef _WIN32
__declspec(dllimport) extern f32 sFontWidths[144];
#else
extern f32 sFontWidths[144];
#endif
static Font sDisabledFont;
static const char sDisabledLabel[] = "Go to Warp Point";
static bool sDisabledFontLoaded;
static u16 sWarpPromptTextId;

static void DrawDisabledWarpChoice(void) {
    PlayState* play = gPlayState;
    if (!play || sPendingWarpStone < 0 || IsStoneWarpReady((uint8_t)sPendingWarpStone) ||
        play->msgCtx.textId != sWarpPromptTextId || Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE) return;
    if (!sDisabledFontLoaded) {
        for (int i = 0; sDisabledLabel[i]; ++i)
            Font_LoadChar(&sDisabledFont, sDisabledLabel[i] - ' ', i * FONT_CHAR_TEX_SIZE);
        sDisabledFontLoaded = true;
    }
    MessageContext* msg = &play->msgCtx;
    s16 oldX=msg->textPosX, oldY=msg->textPosY;
    s16 oldR=msg->textColorR, oldG=msg->textColorG, oldB=msg->textColorB, oldA=msg->textColorAlpha;
    Message_SetView(&msg->view);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetAlphaCompare(OVERLAY_DISP++, G_AC_NONE);
    gDPSetCombineLERP(OVERLAY_DISP++, 0,0,0,PRIMITIVE, TEXEL0,0,PRIMITIVE,0,
                     0,0,0,PRIMITIVE, TEXEL0,0,PRIMITIVE,0);
    msg->textPosX=R_TEXT_INIT_XPOS+32;
    msg->textPosY=R_TEXT_INIT_YPOS+2*R_TEXT_LINE_SPACING;
    msg->textColorR=msg->textColorG=msg->textColorB=128;
    msg->textColorAlpha=255;
    for (int i=0; sDisabledLabel[i]; ++i) {
        if (sDisabledLabel[i]==' ') msg->textPosX+=CVarGetInteger("gEnhancements.TextSpacing",6);
        else {
            Gfx* gfx=OVERLAY_DISP;
            Message_DrawTextChar(play,&sDisabledFont.charTexBuf[i*FONT_CHAR_TEX_SIZE],&gfx);
            OVERLAY_DISP=gfx;
            msg->textPosX+=(s32)(sFontWidths[sDisabledLabel[i]-' ']*(R_TEXT_CHAR_SCALE/100.0f));
        }
    }
    msg->textPosX=oldX;msg->textPosY=oldY;
    msg->textColorR=oldR;msg->textColorG=oldG;msg->textColorB=oldB;msg->textColorAlpha=oldA;
    CLOSE_DISPS(play->state.gfxCtx);
}

static void ResolveWarpPrompt(Player* player, PlayState* play) {
    if (sPendingWarpStone < 0) return;
    if (play->msgCtx.textId != sWarpPromptTextId || play->msgCtx.msgMode == MSGMODE_NONE) {
        sPendingWarpStone=-1;
        return;
    }
    if (Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE || !Message_ShouldAdvance(play)) return;
    uint8_t stone=(uint8_t)sPendingWarpStone;
    uint8_t choice=play->msgCtx.choiceIndex;
    if (choice==1 && !IsStoneWarpReady(stone)) {
        PlaySfxAt(NA_SE_SY_ERROR,&player->actor.world.pos);
        // Prevent Message_Main from accepting the same disabled selection later this frame.
        play->state.input[0].press.button &= ~(BTN_A|BTN_B|BTN_CUP);
        play->state.input[0].cur.button &= ~BTN_B;
        return;
    }
    Message_CloseTextbox(play);
    sPendingWarpStone=-1;
    if (choice==0 && sHasOwlStatue) PlantStatue(play,player,stone);
    else if (choice==1) WarpToStatue(play,stone);
}

static void OpenWarpPrompt(PlayState* play, uint8_t stone) {
    const char* text=IsStoneWarpReady(stone)
        ? "What do you want to do?\x1C&%gNew Warp Point&Go to Warp Point&Nothing%w"
        : "What do you want to do?\x1C&%gNew Warp Point& &Nothing%w";
    if (!sApi->ShowTextbox(play,text,false)) return;
    sPendingWarpStone=(int8_t)stone;
    sWarpPromptTextId=play->msgCtx.textId;
}

static void TickStoneButtons(Player* player, PlayState* play) {
    if (sPendingWarpStone>=0) return;
    for (uint8_t stone=0;stone<STONE_MAX;stone++) {
        uint16_t button=FindEquippedButtonMask(sStones[stone].key);
        if (button && CHECK_QUEST_ITEM(sStones[stone].questItem) && (play->state.input[0].press.button&button)) {
            OpenWarpPrompt(play,stone);
            return;
        }
    }
}

static void ResolveStoneMotion(Player* player,int32_t kind,float* scale) {
    if(!gPlayState || IsPlayerBusy(player,gPlayState)) return;
    if(kind==SOH_PLAYER_MOTION_WATER_SPEED && (player->stateFlags1&PLAYER_STATE1_IN_WATER) &&
       !(sPassiveDisabled&(1<<STONE_ZORA)) && CHECK_QUEST_ITEM(QUEST_ZORA_SAPPHIRE)) {
        *scale*=STONE_SWIM_BOOST;
    } else if((kind==SOH_PLAYER_MOTION_STICK_SPEED || kind==SOH_PLAYER_MOTION_SPEED_CAP) &&
        !(player->stateFlags1&(PLAYER_STATE1_IN_WATER|PLAYER_STATE1_CLIMBING_LADDER)) &&
        (player->actor.bgCheckFlags&1) && !(sPassiveDisabled&(1<<STONE_KOKIRI)) && CHECK_QUEST_ITEM(QUEST_KOKIRI_EMERALD)) {
        *scale*=STONE_WALK_BOOST;
    }
}
void Player_Action_8084BF1C(Player*,PlayState*);
static void BoostStoneClimb(bool* should,va_list args) {
    Player* player=va_arg(args,Player*);
    if(!*should || !gPlayState || IsPlayerBusy(player,gPlayState) ||
       (sPassiveDisabled&(1<<STONE_GORON)) || !CHECK_QUEST_ITEM(QUEST_GORON_RUBY) ||
       !(player->stateFlags1&PLAYER_STATE1_CLIMBING_LADDER) || player->actionFunc!=Player_Action_8084BF1C) return;
    // Advance the native climb twice before its root movement is applied. Each step retains endpoint checks.
    *should=false;
    Player_Action_8084BF1C(player,gPlayState);
    if(player->actionFunc==Player_Action_8084BF1C && (player->stateFlags1&PLAYER_STATE1_CLIMBING_LADDER))
        Player_Action_8084BF1C(player,gPlayState);
}

static void StatueDraw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sEmptySegment);
    Gfx_DrawDListOpa(play, (Gfx*)sOwlStatueDL);
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y + STATUE_JEWEL_HEIGHT, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD((int16_t)(play->gameplayFrames * 0x100)), MTXMODE_APPLY);
    Matrix_Scale(STATUE_JEWEL_SCALE, STATUE_JEWEL_SCALE, STATUE_JEWEL_SCALE, MTXMODE_APPLY);
    // The Temple of Time's actual gem and gold setting, with their native material setup.
    GetItem_Draw(play,sStoneDrawIds[thisx->params % STONE_MAX]);
    Matrix_Pop();
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
    memset(sStatues, 0, sizeof(sStatues));
    sPendingWarpStone = -1;
}

static void ReloadState(int32_t fileNum) {
    LoadState();
    sPassiveDisabled=0;
    sApi->StorageGet(MOD_NAME,"passive_disabled",&sPassiveDisabled,sizeof(sPassiveDisabled));
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
    ReloadState(0);
    SOH_REGISTER_HOOK(sApi,OnPlayerResolveMotionScale,ResolveStoneMotion);
    SOH_REGISTER_HOOK(sApi,OnKaleidoInput,ToggleStonePassive);
    SOH_REGISTER_HOOK(sApi,OnKaleidoInput,EquipStoneWarp);
    SOH_REGISTER_HOOK(sApi,OnPlayDrawBegin,DrawStoneState);
    SOH_REGISTER_HOOK(sApi,OnPlayDrawEnd,RestoreQuestIcons);
    // This hook runs after the native overlay is built, so the gray label stays above the textbox.
    SOH_REGISTER_HOOK(sApi,OnGameFrameUpdate,DrawDisabledWarpChoice);
    sApi->RegisterVB(VB_INFLICT_VOID_DAMAGE, WaiveVoidDamage);
    sApi->RegisterVB(VB_EXECUTE_PLAYER_ACTION_FUNC,BoostStoneClimb);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickMod);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    SOH_REGISTER_HOOK(sApi, OnLoadFile, ReloadState);
    SOH_REGISTER_HOOK(sApi, OnSceneSpawnActors, PlantStatuesForScene);
}
