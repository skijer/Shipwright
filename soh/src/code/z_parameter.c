#include "global.h"
#include "vt.h"
#include "textures/parameter_static/parameter_static.h"
#include "textures/do_action_static/do_action_static.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "soh_assets.h"

#include "soh/Enhancements/gameplaystats.h"
#include "soh/Enhancements/custom-message/CustomMessageInterfaceAddon.h"
#include "soh/Enhancements/cosmetics/cosmeticsTypes.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/ShipUtils.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizer_grotto.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/gameplaystats.h"
#include "soh/ObjectExtension/ActorMaximumHealth.h"

#include "message_data_static.h"
extern MessageTableEntry* sNesMessageEntryTablePtr;
extern MessageTableEntry* sGerMessageEntryTablePtr;
extern MessageTableEntry* sFraMessageEntryTablePtr;
extern MessageTableEntry* sJpnMessageEntryTablePtr;

#define DO_ACTION_TEX_WIDTH() 48
#define DO_ACTION_TEX_HEIGHT() 16
#define DO_ACTION_TEX_SIZE() ((DO_ACTION_TEX_WIDTH() * DO_ACTION_TEX_HEIGHT()) / 2)

// The button statuses include the A button when most things are only the equip item buttons
// So, when indexing into it with a item button index, we need to adjust
#define BUTTON_STATUS_INDEX(button) ((button) >= 4) ? ((button) + 1) : (button)

#define LOCAL_MP_PLAYER_COUNT_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount")
#define LOCAL_MP_DISABLED_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.Disable")
#define LOCAL_MP_SPLITSCREEN_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreen")
#define LOCAL_MP_SPLITSCREEN_VERTICAL_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreenVertical")
#define LOCAL_MP_ORIGINAL_ITEMS_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.OriginalItemSystem")
#define LOCAL_MP_RADIAL_QUICK_ITEM_MENU_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.RadialQuickItemMenu")
#define LOCAL_MP_INDEPENDENT_SLOT_USAGE_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.IndependentSlotUsage")
#define LOCAL_MP_SECOND_ITEM_SLOT_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.SecondItemSlot")
#define LOCAL_MP_SECOND_ITEM_SLOT_SIDE_BY_SIDE_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.SecondItemSlotSideBySide")

s16 Top_HUD_Margin = 0;
s16 Left_HUD_Margin = 0;
s16 Right_HUD_Margin = 0;
s16 Bottom_HUD_Margin = 0;

typedef struct {
    /* 0x00 */ u8 scene;
    /* 0x01 */ u8 flags1;
    /* 0x02 */ u8 flags2;
    /* 0x03 */ u8 flags3;
} RestrictionFlags;

static RestrictionFlags sRestrictionFlags[] = {
    { SCENE_HYRULE_FIELD, 0x00, 0x00, 0x10 },
    { SCENE_KAKARIKO_VILLAGE, 0x00, 0x00, 0x10 },
    { SCENE_GRAVEYARD, 0x00, 0x00, 0x10 },
    { SCENE_ZORAS_RIVER, 0x00, 0x00, 0x10 },
    { SCENE_KOKIRI_FOREST, 0x00, 0x00, 0x10 },
    { SCENE_SACRED_FOREST_MEADOW, 0x00, 0x00, 0x10 },
    { SCENE_LAKE_HYLIA, 0x00, 0x00, 0x10 },
    { SCENE_ZORAS_DOMAIN, 0x00, 0x00, 0x10 },
    { SCENE_ZORAS_FOUNTAIN, 0x00, 0x00, 0x10 },
    { SCENE_GERUDO_VALLEY, 0x00, 0x00, 0x10 },
    { SCENE_LOST_WOODS, 0x00, 0x00, 0x10 },
    { SCENE_DESERT_COLOSSUS, 0x00, 0x00, 0x10 },
    { SCENE_GERUDOS_FORTRESS, 0x00, 0x00, 0x10 },
    { SCENE_HAUNTED_WASTELAND, 0x00, 0x00, 0x10 },
    { SCENE_HYRULE_CASTLE, 0x00, 0x00, 0x10 },
    { SCENE_OUTSIDE_GANONS_CASTLE, 0x00, 0x00, 0x10 },
    { SCENE_DEATH_MOUNTAIN_TRAIL, 0x00, 0x00, 0x10 },
    { SCENE_DEATH_MOUNTAIN_CRATER, 0x00, 0x00, 0x10 },
    { SCENE_GORON_CITY, 0x00, 0x00, 0x10 },
    { SCENE_LON_LON_RANCH, 0x00, 0x00, 0x10 },
    { SCENE_TEMPLE_OF_TIME, 0x00, 0x10, 0x15 },
    { SCENE_CHAMBER_OF_THE_SAGES, 0xA2, 0xAA, 0xAA },
    { SCENE_SHOOTING_GALLERY, 0x11, 0x55, 0x55 },
    { SCENE_CASTLE_COURTYARD_GUARDS_DAY, 0x11, 0x55, 0x55 },
    { SCENE_CASTLE_COURTYARD_GUARDS_NIGHT, 0x11, 0x55, 0x55 },
    { SCENE_REDEAD_GRAVE, 0x00, 0x00, 0xD0 },
    { SCENE_GRAVE_WITH_FAIRYS_FOUNTAIN, 0x00, 0x00, 0xD0 },
    { SCENE_ROYAL_FAMILYS_TOMB, 0x00, 0x00, 0xD0 },
    { SCENE_GREAT_FAIRYS_FOUNTAIN_MAGIC, 0x00, 0x00, 0x10 },
    { SCENE_FAIRYS_FOUNTAIN, 0x00, 0x00, 0xD0 },
    { SCENE_GREAT_FAIRYS_FOUNTAIN_SPELLS, 0x00, 0x00, 0x10 },
    { SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR, 0x00, 0x05, 0x50 },
    { SCENE_CASTLE_COURTYARD_ZELDA, 0x00, 0x05, 0x54 },
    { SCENE_FISHING_POND, 0x11, 0x55, 0x55 },
    { SCENE_BOMBCHU_BOWLING_ALLEY, 0x11, 0x55, 0x55 },
    { SCENE_LON_LON_BUILDINGS, 0x00, 0x10, 0x15 },
    { SCENE_MARKET_GUARD_HOUSE, 0x00, 0x10, 0x14 },
    { SCENE_POTION_SHOP_GRANNY, 0x10, 0x15, 0x55 },
    { SCENE_TREASURE_BOX_SHOP, 0x10, 0x15, 0x55 },
    { SCENE_HOUSE_OF_SKULLTULA, 0x00, 0x10, 0x15 },
    { SCENE_MARKET_ENTRANCE_DAY, 0x00, 0x10, 0x15 },
    { SCENE_MARKET_ENTRANCE_NIGHT, 0x00, 0x10, 0x15 },
    { SCENE_MARKET_ENTRANCE_RUINS, 0x00, 0x10, 0xD5 },
    { SCENE_MARKET_DAY, 0x00, 0x10, 0x15 },
    { SCENE_MARKET_NIGHT, 0x00, 0x10, 0x15 },
    { SCENE_MARKET_RUINS, 0x00, 0x10, 0xD5 },
    { SCENE_BACK_ALLEY_DAY, 0x00, 0x10, 0x15 },
    { SCENE_BACK_ALLEY_NIGHT, 0x00, 0x10, 0x15 },
    { SCENE_TEMPLE_OF_TIME_EXTERIOR_DAY, 0x00, 0x10, 0x15 },
    { SCENE_TEMPLE_OF_TIME_EXTERIOR_NIGHT, 0x00, 0x10, 0x15 },
    { SCENE_TEMPLE_OF_TIME_EXTERIOR_RUINS, 0x00, 0x10, 0xD5 },
    { SCENE_LINKS_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_KAKARIKO_CENTER_GUEST_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_BACK_ALLEY_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_KNOW_IT_ALL_BROS_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_TWINS_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_MIDOS_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_SARIAS_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_STABLE, 0x10, 0x10, 0x15 },
    { SCENE_GRAVEKEEPERS_HUT, 0x10, 0x10, 0x15 },
    { SCENE_DOG_LADY_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_IMPAS_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_LAKESIDE_LABORATORY, 0x00, 0x10, 0x15 },
    { SCENE_CARPENTERS_TENT, 0x10, 0x10, 0x15 },
    { SCENE_BAZAAR, 0x10, 0x10, 0x15 },
    { SCENE_KOKIRI_SHOP, 0x10, 0x10, 0x15 },
    { SCENE_GORON_SHOP, 0x10, 0x10, 0x15 },
    { SCENE_ZORA_SHOP, 0x10, 0x10, 0x15 },
    { SCENE_POTION_SHOP_KAKARIKO, 0x10, 0x10, 0x15 },
    { SCENE_POTION_SHOP_MARKET, 0x10, 0x10, 0x15 },
    { SCENE_BOMBCHU_SHOP, 0x10, 0x10, 0x15 },
    { SCENE_HAPPY_MASK_SHOP, 0x10, 0x10, 0x15 },
    { SCENE_GERUDO_TRAINING_GROUND, 0x00, 0x03, 0x10 },
    { SCENE_DEKU_TREE, 0x00, 0x00, 0x00 },
    { SCENE_DEKU_TREE_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_DODONGOS_CAVERN, 0x00, 0x00, 0x00 },
    { SCENE_DODONGOS_CAVERN_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_JABU_JABU, 0x00, 0x00, 0x00 },
    { SCENE_JABU_JABU_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_FOREST_TEMPLE, 0x00, 0x00, 0x00 },
    { SCENE_FOREST_TEMPLE_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_BOTTOM_OF_THE_WELL, 0x00, 0x00, 0x00 },
    { SCENE_SHADOW_TEMPLE, 0x00, 0x00, 0x00 },
    { SCENE_SHADOW_TEMPLE_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_FIRE_TEMPLE, 0x00, 0x00, 0x00 },
    { SCENE_FIRE_TEMPLE_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_WATER_TEMPLE, 0x00, 0x00, 0x00 },
    { SCENE_WATER_TEMPLE_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_SPIRIT_TEMPLE, 0x00, 0x00, 0x00 },
    { SCENE_SPIRIT_TEMPLE_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_GANONS_TOWER, 0x00, 0x00, 0x00 },
    { SCENE_GANONDORF_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_ICE_CAVERN, 0x00, 0x00, 0xC0 },
    { SCENE_WINDMILL_AND_DAMPES_GRAVE, 0x00, 0x03, 0x14 },
    { SCENE_INSIDE_GANONS_CASTLE, 0x00, 0x03, 0x10 },
    { SCENE_GANON_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_GANONS_TOWER_COLLAPSE_INTERIOR, 0x00, 0x05, 0x50 },
    { SCENE_INSIDE_GANONS_CASTLE_COLLAPSE, 0x00, 0x05, 0x50 },
    { SCENE_THIEVES_HIDEOUT, 0x00, 0x00, 0x10 },
    { SCENE_GROTTOS, 0x00, 0x00, 0xD0 },
    { 0xFF, 0x00, 0x00, 0x00 },
};

static s16 sHBAScoreTier = 0;
static u16 sHBAScoreDigits[] = { 0, 0, 0, 0 };

static u16 sCUpInvisible = 0;
static u16 sCUpTimer = 0;
static u8 sP4QuickItem = ITEM_NONE;
static s16 sP4QuickItemSlot = SLOT_NONE;
static u8 sQuickItemSlot2[4] = { ITEM_NONE, ITEM_NONE, ITEM_NONE, ITEM_NONE };
static s16 sQuickItemSlot2InventorySlot[4] = { SLOT_NONE, SLOT_NONE, SLOT_NONE, SLOT_NONE };
static u8 sQuickItemSelectedSlot[4] = { 0, 0, 0, 0 };
static u8 sQuickItemUseSlot[4] = { 0, 0, 0, 0 };

typedef struct {
    u8 active;
    u8 slotIndex;
    u8 itemCount;
    u8 selectedIndex;
    u8 items[24];
    s16 inventorySlots[24];
} LocalMPRadialQuickMenuState;

static LocalMPRadialQuickMenuState sRadialQuickMenuStates[4];

s16 gSpoilingItems[] = { ITEM_ODD_MUSHROOM, ITEM_FROG, ITEM_EYEDROPS };
s16 gSpoilingItemReverts[] = { ITEM_COJIRO, ITEM_PRESCRIPTION, ITEM_PRESCRIPTION };

static Color_RGB8 sMagicBorder = { 255, 255, 255 };
static Color_RGB8 sMagicBorder_ori = { 255, 255, 255 };

static s16 sP2MagicCapacity = 0;
static s16 sP2MagicTarget = 0;
static s16 sP2MagicFillTarget = 0;
static s16 sP2MagicState = MAGIC_STATE_IDLE;
static s16 sP2PrevMagicState = MAGIC_STATE_IDLE;
static s16 sP2MagicBorderRatio = 2;
static s16 sP2MagicBorderStep = 1;
static s16 sP2LensMagicConsumptionTimer = 0;
static Color_RGB8 sMagicBorder2 = { 255, 255, 255 };

static s8 sP3MagicLevel = 0;
static s8 sP3Magic = MAGIC_NORMAL_METER;
static u8 sP3IsMagicAcquired = false;
static u8 sP3IsDoubleMagicAcquired = false;
static s16 sP3MagicCapacity = 0;
static s16 sP3MagicTarget = 0;
static s16 sP3MagicFillTarget = 0;
static s16 sP3MagicState = MAGIC_STATE_IDLE;
static s16 sP3PrevMagicState = MAGIC_STATE_IDLE;
static s16 sP3MagicBorderRatio = 2;
static s16 sP3MagicBorderStep = 1;
static s16 sP3LensMagicConsumptionTimer = 0;
static Color_RGB8 sMagicBorder3 = { 255, 255, 255 };

static s8 sP4MagicLevel = 0;
static s8 sP4Magic = MAGIC_NORMAL_METER;
static u8 sP4IsMagicAcquired = false;
static u8 sP4IsDoubleMagicAcquired = false;
static s16 sP4MagicCapacity = 0;
static s16 sP4MagicTarget = 0;
static s16 sP4MagicFillTarget = 0;
static s16 sP4MagicState = MAGIC_STATE_IDLE;
static s16 sP4PrevMagicState = MAGIC_STATE_IDLE;
static s16 sP4MagicBorderRatio = 2;
static s16 sP4MagicBorderStep = 1;
static s16 sP4LensMagicConsumptionTimer = 0;
static Color_RGB8 sMagicBorder4 = { 255, 255, 255 };

static Player* sItemGivePlayer = NULL;

static s16 sExtraItemBases[] = {
    ITEM_STICK, ITEM_STICK, ITEM_NUT,   ITEM_NUT,     ITEM_BOMB,    ITEM_BOMB,  ITEM_BOMB,  ITEM_BOMB, ITEM_BOW,
    ITEM_BOW,   ITEM_BOW,   ITEM_SEEDS, ITEM_BOMBCHU, ITEM_BOMBCHU, ITEM_STICK, ITEM_STICK, ITEM_NUT,  ITEM_NUT,
};

static s16 sEnvHazard = PLAYER_ENV_HAZARD_NONE;
static s16 sEnvHazardActive = false;

static Gfx sSetupDL_80125A60[] = {
    gsDPPipeSync(),
    gsSPClearGeometryMode(G_ZBUFFER | G_SHADE | G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN |
                          G_TEXTURE_GEN_LINEAR | G_SHADING_SMOOTH | G_LOD),
    gsDPSetOtherMode(G_AD_DISABLE | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                         G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_1PRIMITIVE,
                     G_AC_NONE | G_ZS_PIXEL | G_RM_CLD_SURF | G_RM_CLD_SURF2),
    gsDPSetCombineMode(G_CC_PRIMITIVE, G_CC_PRIMITIVE),
    gsSPEndDisplayList(),
};

static const char* actionsTbl[] = {
    gAttackDoActionENGTex, gCheckDoActionENGTex, gEnterDoActionENGTex,      gReturnDoActionENGTex,
    gOpenDoActionENGTex,   gJumpDoActionENGTex,  gDecideDoActionENGTex,     gDiveDoActionENGTex,
    gFasterDoActionENGTex, gThrowDoActionENGTex, gUnusedNaviDoActionENGTex, gClimbDoActionENGTex,
    gDropDoActionENGTex,   gDownDoActionENGTex,  gSaveDoActionENGTex,       gSpeakDoActionENGTex,
    gNextDoActionENGTex,   gGrabDoActionENGTex,  gStopDoActionENGTex,       gPutAwayDoActionENGTex,
    gReelDoActionENGTex,   gNum1DoActionENGTex,  gNum2DoActionENGTex,       gNum3DoActionENGTex,
    gNum4DoActionENGTex,   gNum5DoActionENGTex,  gNum6DoActionENGTex,       gNum7DoActionENGTex,
    gNum8DoActionENGTex,
};

void Magic_ResetExtraData(void);
s16 Magic_GetStateForPlayer(Player* player);
void Magic_FillForPlayer(PlayState* play, Player* player);
s32 Magic_RequestChangeForPlayer(PlayState* play, Player* player, s16 amount, s16 type);

// original name: "alpha_change"
void Interface_ChangeAlpha(u16 alphaType) {
    if (alphaType != gSaveContext.unk_13EA) {
        osSyncPrintf("ＡＬＰＨＡーＴＹＰＥ＝%d  LAST_TIME_TYPE=%d\n", alphaType, gSaveContext.unk_13EE);
        gSaveContext.unk_13EA = gSaveContext.unk_13E8 = alphaType;
        gSaveContext.unk_13EC = 1;
    }
}

void func_80082644(PlayState* play, s16 alpha) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    if (gSaveContext.buttonStatus[0] == BTN_DISABLED) {
        if (interfaceCtx->bAlpha != 70) {
            interfaceCtx->bAlpha = 70;
        }
    } else {
        if (interfaceCtx->bAlpha != 255) {
            interfaceCtx->bAlpha = alpha;
        }
    }

    if (gSaveContext.buttonStatus[1] == BTN_DISABLED) {
        if (interfaceCtx->cLeftAlpha != 70) {
            interfaceCtx->cLeftAlpha = 70;
        }
    } else {
        if (interfaceCtx->cLeftAlpha != 255) {
            interfaceCtx->cLeftAlpha = alpha;
        }
    }

    if (gSaveContext.buttonStatus[2] == BTN_DISABLED) {
        if (interfaceCtx->cDownAlpha != 70) {
            interfaceCtx->cDownAlpha = 70;
        }
    } else {
        if (interfaceCtx->cDownAlpha != 255) {
            interfaceCtx->cDownAlpha = alpha;
        }
    }

    if (gSaveContext.buttonStatus[3] == BTN_DISABLED) {
        if (interfaceCtx->cRightAlpha != 70) {
            interfaceCtx->cRightAlpha = 70;
        }
    } else {
        if (interfaceCtx->cRightAlpha != 255) {
            interfaceCtx->cRightAlpha = alpha;
        }
    }

    if (gSaveContext.buttonStatus[4] == BTN_DISABLED) {
        if (interfaceCtx->aAlpha != 70) {
            interfaceCtx->aAlpha = 70;
        }
    } else {
        if (interfaceCtx->aAlpha != 255) {
            interfaceCtx->aAlpha = alpha;
        }
    }

    if (gSaveContext.buttonStatus[5] == BTN_DISABLED) {
        if (interfaceCtx->dpadUpAlpha != 70) {
            interfaceCtx->dpadUpAlpha = 70;
        }
    } else {
        if (interfaceCtx->dpadUpAlpha != 255) {
            interfaceCtx->dpadUpAlpha = alpha;
        }
    }

    if (gSaveContext.buttonStatus[6] == BTN_DISABLED) {
        if (interfaceCtx->dpadDownAlpha != 70) {
            interfaceCtx->dpadDownAlpha = 70;
        }
    } else {
        if (interfaceCtx->dpadDownAlpha != 255) {
            interfaceCtx->dpadDownAlpha = alpha;
        }
    }

    if (gSaveContext.buttonStatus[7] == BTN_DISABLED) {
        if (interfaceCtx->dpadLeftAlpha != 70) {
            interfaceCtx->dpadLeftAlpha = 70;
        }
    } else {
        if (interfaceCtx->dpadLeftAlpha != 255) {
            interfaceCtx->dpadLeftAlpha = alpha;
        }
    }

    if (gSaveContext.buttonStatus[8] == BTN_DISABLED) {
        if (interfaceCtx->dpadRightAlpha != 70) {
            interfaceCtx->dpadRightAlpha = 70;
        }
    } else {
        if (interfaceCtx->dpadRightAlpha != 255) {
            interfaceCtx->dpadRightAlpha = alpha;
        }
    }
}

void func_8008277C(PlayState* play, s16 maxAlpha, s16 alpha) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    if (gSaveContext.forceRisingButtonAlphas != 0) {
        func_80082644(play, alpha);
        return;
    }

    if ((interfaceCtx->bAlpha != 0) && (interfaceCtx->bAlpha > maxAlpha)) {
        interfaceCtx->bAlpha = maxAlpha;
    }

    if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
        interfaceCtx->aAlpha = maxAlpha;
    }

    if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
        interfaceCtx->cLeftAlpha = maxAlpha;
    }

    if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
        interfaceCtx->cDownAlpha = maxAlpha;
    }

    if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
        interfaceCtx->cRightAlpha = maxAlpha;
    }

    if ((interfaceCtx->dpadUpAlpha != 0) && (interfaceCtx->dpadUpAlpha > maxAlpha)) {
        interfaceCtx->dpadUpAlpha = maxAlpha;
    }

    if ((interfaceCtx->dpadDownAlpha != 0) && (interfaceCtx->dpadDownAlpha > maxAlpha)) {
        interfaceCtx->dpadDownAlpha = maxAlpha;
    }

    if ((interfaceCtx->dpadLeftAlpha != 0) && (interfaceCtx->dpadLeftAlpha > maxAlpha)) {
        interfaceCtx->dpadLeftAlpha = maxAlpha;
    }

    if ((interfaceCtx->dpadRightAlpha != 0) && (interfaceCtx->dpadRightAlpha > maxAlpha)) {
        interfaceCtx->dpadRightAlpha = maxAlpha;
    }
}

void func_80082850(PlayState* play, s16 maxAlpha) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 alpha = 255 - maxAlpha;

    switch (gSaveContext.unk_13E8) {
        case 1:
        case 2:
        case 8:
            osSyncPrintf("a_alpha=%d, c_alpha=%d   →   ", interfaceCtx->aAlpha, interfaceCtx->cLeftAlpha);

            if (gSaveContext.unk_13E8 == 8) {
                if (interfaceCtx->bAlpha != 255) {
                    interfaceCtx->bAlpha = alpha;
                }
            } else {
                if ((interfaceCtx->bAlpha != 0) && (interfaceCtx->bAlpha > maxAlpha)) {
                    interfaceCtx->bAlpha = maxAlpha;
                }
            }

            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
                interfaceCtx->cLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
                interfaceCtx->cDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
                interfaceCtx->cRightAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadUpAlpha != 0) && (interfaceCtx->dpadUpAlpha > maxAlpha)) {
                interfaceCtx->dpadUpAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadDownAlpha != 0) && (interfaceCtx->dpadDownAlpha > maxAlpha)) {
                interfaceCtx->dpadDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadLeftAlpha != 0) && (interfaceCtx->dpadLeftAlpha > maxAlpha)) {
                interfaceCtx->dpadLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadRightAlpha != 0) && (interfaceCtx->dpadRightAlpha > maxAlpha)) {
                interfaceCtx->dpadRightAlpha = maxAlpha;
            }

            if ((interfaceCtx->healthAlpha != 0) && (interfaceCtx->healthAlpha > maxAlpha)) {
                interfaceCtx->healthAlpha = maxAlpha;
            }

            if ((interfaceCtx->magicAlpha != 0) && (interfaceCtx->magicAlpha > maxAlpha)) {
                interfaceCtx->magicAlpha = maxAlpha;
            }

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            osSyncPrintf("a_alpha=%d, c_alpha=%d\n", interfaceCtx->aAlpha, interfaceCtx->cLeftAlpha);

            break;
        case 3:
            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            func_8008277C(play, maxAlpha, alpha);

            if ((interfaceCtx->magicAlpha != 0) && (interfaceCtx->magicAlpha > maxAlpha)) {
                interfaceCtx->magicAlpha = maxAlpha;
            }

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            break;
        case 4:
            if ((interfaceCtx->bAlpha != 0) && (interfaceCtx->bAlpha > maxAlpha)) {
                interfaceCtx->bAlpha = maxAlpha;
            }

            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
                interfaceCtx->cLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
                interfaceCtx->cDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
                interfaceCtx->cRightAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadUpAlpha != 0) && (interfaceCtx->dpadUpAlpha > maxAlpha)) {
                interfaceCtx->dpadUpAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadDownAlpha != 0) && (interfaceCtx->dpadDownAlpha > maxAlpha)) {
                interfaceCtx->dpadDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadLeftAlpha != 0) && (interfaceCtx->dpadLeftAlpha > maxAlpha)) {
                interfaceCtx->dpadLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadRightAlpha != 0) && (interfaceCtx->dpadRightAlpha > maxAlpha)) {
                interfaceCtx->dpadRightAlpha = maxAlpha;
            }

            if ((interfaceCtx->healthAlpha != 0) && (interfaceCtx->healthAlpha > maxAlpha)) {
                interfaceCtx->healthAlpha = maxAlpha;
            }

            if ((interfaceCtx->magicAlpha != 0) && (interfaceCtx->magicAlpha > maxAlpha)) {
                interfaceCtx->magicAlpha = maxAlpha;
            }

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if (interfaceCtx->aAlpha != 255) {
                interfaceCtx->aAlpha = alpha;
            }

            break;
        case 5:
            func_8008277C(play, maxAlpha, alpha);

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if (interfaceCtx->aAlpha != 255) {
                interfaceCtx->aAlpha = alpha;
            }

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            if (interfaceCtx->magicAlpha != 255) {
                interfaceCtx->magicAlpha = alpha;
            }

            break;
        case 6:
            func_8008277C(play, maxAlpha, alpha);

            if (interfaceCtx->aAlpha != 255) {
                interfaceCtx->aAlpha = alpha;
            }

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            if (interfaceCtx->magicAlpha != 255) {
                interfaceCtx->magicAlpha = alpha;
            }

            switch (play->sceneNum) {
                case SCENE_HYRULE_FIELD:
                case SCENE_KAKARIKO_VILLAGE:
                case SCENE_GRAVEYARD:
                case SCENE_ZORAS_RIVER:
                case SCENE_KOKIRI_FOREST:
                case SCENE_SACRED_FOREST_MEADOW:
                case SCENE_LAKE_HYLIA:
                case SCENE_ZORAS_DOMAIN:
                case SCENE_ZORAS_FOUNTAIN:
                case SCENE_GERUDO_VALLEY:
                case SCENE_LOST_WOODS:
                case SCENE_DESERT_COLOSSUS:
                case SCENE_GERUDOS_FORTRESS:
                case SCENE_HAUNTED_WASTELAND:
                case SCENE_HYRULE_CASTLE:
                case SCENE_DEATH_MOUNTAIN_TRAIL:
                case SCENE_DEATH_MOUNTAIN_CRATER:
                case SCENE_GORON_CITY:
                case SCENE_LON_LON_RANCH:
                case SCENE_OUTSIDE_GANONS_CASTLE:
                    if (interfaceCtx->minimapAlpha < 170) {
                        interfaceCtx->minimapAlpha = alpha;
                    } else {
                        interfaceCtx->minimapAlpha = 170;
                    }
                    break;
                default:
                    if (interfaceCtx->minimapAlpha != 255) {
                        interfaceCtx->minimapAlpha = alpha;
                    }
                    break;
            }
            break;
        case 7:
            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            func_80082644(play, alpha);

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            if (interfaceCtx->magicAlpha != 255) {
                interfaceCtx->magicAlpha = alpha;
            }

            break;
        case 9:
            if ((interfaceCtx->bAlpha != 0) && (interfaceCtx->bAlpha > maxAlpha)) {
                interfaceCtx->bAlpha = maxAlpha;
            }

            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
                interfaceCtx->cLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
                interfaceCtx->cDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
                interfaceCtx->cRightAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadUpAlpha != 0) && (interfaceCtx->dpadUpAlpha > maxAlpha)) {
                interfaceCtx->dpadUpAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadDownAlpha != 0) && (interfaceCtx->dpadDownAlpha > maxAlpha)) {
                interfaceCtx->dpadDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadLeftAlpha != 0) && (interfaceCtx->dpadLeftAlpha > maxAlpha)) {
                interfaceCtx->dpadLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadRightAlpha != 0) && (interfaceCtx->dpadRightAlpha > maxAlpha)) {
                interfaceCtx->dpadRightAlpha = maxAlpha;
            }

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            if (interfaceCtx->magicAlpha != 255) {
                interfaceCtx->magicAlpha = alpha;
            }

            break;
        case 10:
            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
                interfaceCtx->cLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
                interfaceCtx->cDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
                interfaceCtx->cRightAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadUpAlpha != 0) && (interfaceCtx->dpadUpAlpha > maxAlpha)) {
                interfaceCtx->dpadUpAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadDownAlpha != 0) && (interfaceCtx->dpadDownAlpha > maxAlpha)) {
                interfaceCtx->dpadDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadLeftAlpha != 0) && (interfaceCtx->dpadLeftAlpha > maxAlpha)) {
                interfaceCtx->dpadLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadRightAlpha != 0) && (interfaceCtx->dpadRightAlpha > maxAlpha)) {
                interfaceCtx->dpadRightAlpha = maxAlpha;
            }

            if ((interfaceCtx->healthAlpha != 0) && (interfaceCtx->healthAlpha > maxAlpha)) {
                interfaceCtx->healthAlpha = maxAlpha;
            }

            if ((interfaceCtx->magicAlpha != 0) && (interfaceCtx->magicAlpha > maxAlpha)) {
                interfaceCtx->magicAlpha = maxAlpha;
            }

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if (interfaceCtx->bAlpha != 255) {
                interfaceCtx->bAlpha = alpha;
            }

            break;
        case 11:
            if ((interfaceCtx->bAlpha != 0) && (interfaceCtx->bAlpha > maxAlpha)) {
                interfaceCtx->bAlpha = maxAlpha;
            }

            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
                interfaceCtx->cLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
                interfaceCtx->cDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
                interfaceCtx->cRightAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadUpAlpha != 0) && (interfaceCtx->dpadUpAlpha > maxAlpha)) {
                interfaceCtx->dpadUpAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadDownAlpha != 0) && (interfaceCtx->dpadDownAlpha > maxAlpha)) {
                interfaceCtx->dpadDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadLeftAlpha != 0) && (interfaceCtx->dpadLeftAlpha > maxAlpha)) {
                interfaceCtx->dpadLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadRightAlpha != 0) && (interfaceCtx->dpadRightAlpha > maxAlpha)) {
                interfaceCtx->dpadRightAlpha = maxAlpha;
            }

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if ((interfaceCtx->magicAlpha != 0) && (interfaceCtx->magicAlpha > maxAlpha)) {
                interfaceCtx->magicAlpha = maxAlpha;
            }

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            break;
        case 12:
            if (interfaceCtx->aAlpha != 255) {
                interfaceCtx->aAlpha = alpha;
            }

            if (interfaceCtx->bAlpha != 255) {
                interfaceCtx->bAlpha = alpha;
            }

            if (interfaceCtx->minimapAlpha != 255) {
                interfaceCtx->minimapAlpha = alpha;
            }

            if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
                interfaceCtx->cLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
                interfaceCtx->cDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
                interfaceCtx->cRightAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadUpAlpha != 0) && (interfaceCtx->dpadUpAlpha > maxAlpha)) {
                interfaceCtx->dpadUpAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadDownAlpha != 0) && (interfaceCtx->dpadDownAlpha > maxAlpha)) {
                interfaceCtx->dpadDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadLeftAlpha != 0) && (interfaceCtx->dpadLeftAlpha > maxAlpha)) {
                interfaceCtx->dpadLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->dpadRightAlpha != 0) && (interfaceCtx->dpadRightAlpha > maxAlpha)) {
                interfaceCtx->dpadRightAlpha = maxAlpha;
            }

            if ((interfaceCtx->magicAlpha != 0) && (interfaceCtx->magicAlpha > maxAlpha)) {
                interfaceCtx->magicAlpha = maxAlpha;
            }

            if ((interfaceCtx->healthAlpha != 0) && (interfaceCtx->healthAlpha > maxAlpha)) {
                interfaceCtx->healthAlpha = maxAlpha;
            }

            break;
        case 13:
            func_8008277C(play, maxAlpha, alpha);

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            if (interfaceCtx->magicAlpha != 255) {
                interfaceCtx->magicAlpha = alpha;
            }

            break;
    }

    if ((play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_1) && (interfaceCtx->minimapAlpha >= 255)) {
        interfaceCtx->minimapAlpha = 255;
    }
}

// buttonStatus[0] is used to represent if the B button is disabled, but also tracks
// the last active B button item during mini-games/epona (temp B)
// Since ITEM_NONE is the same as BTN_DISABLED (255), we need a different value to help us track
// that the player was swordless before like ITEM_NONE_FE (254)
#define SWORDLESS_STATUS ITEM_NONE_FE

// Restores swordless state when using the custom value for temp B and then clears temp B
void Interface_RandoRestoreSwordless(void) {
    if (IS_RANDO && gSaveContext.buttonStatus[0] == SWORDLESS_STATUS) {
        gSaveContext.equips.buttonItems[0] = ITEM_NONE;
        gSaveContext.buttonStatus[0] = BTN_ENABLED;
    }
}

void func_80083108(PlayState* play) {
    MessageContext* msgCtx = &play->msgCtx;
    Player* player = GET_PLAYER(play);
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 i;
    s16 sp28 = 0;

    // Check for the player being swordless in rando (no item on B and swordless flag set)
    // Child is always assumed due to not finding kokiri sword yet. Adult is only checked with MS shuffle on.
    u8 randoIsSwordless = IS_RANDO && (LINK_IS_CHILD || Randomizer_GetSettingValue(RSK_SHUFFLE_MASTER_SWORD)) &&
                          gSaveContext.equips.buttonItems[0] == ITEM_NONE && Flags_GetInfTable(INFTABLE_SWORDLESS);
    u8 randoWasSwordlessBefore = IS_RANDO && gSaveContext.buttonStatus[0] == SWORDLESS_STATUS;
    u8 randoCanTrackSwordless = randoIsSwordless && !randoWasSwordlessBefore;

    if ((gSaveContext.cutsceneIndex < 0xFFF0) ||
        ((play->sceneNum == SCENE_LON_LON_RANCH) && (gSaveContext.cutsceneIndex == 0xFFF0))) {
        gSaveContext.forceRisingButtonAlphas = 0;

        if ((player->stateFlags1 & PLAYER_STATE1_ON_HORSE) || (play->shootingGalleryStatus > 1) ||
            ((play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY) && Flags_GetSwitch(play, 0x38))) {
            if (gSaveContext.equips.buttonItems[0] != ITEM_NONE || randoCanTrackSwordless) {
                gSaveContext.forceRisingButtonAlphas = 1;

                if (gSaveContext.buttonStatus[0] == BTN_DISABLED) {
                    gSaveContext.buttonStatus[0] = gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] =
                        gSaveContext.buttonStatus[3] = BTN_ENABLED;
                    gSaveContext.buttonStatus[5] = gSaveContext.buttonStatus[6] = gSaveContext.buttonStatus[7] =
                        gSaveContext.buttonStatus[8] = BTN_ENABLED;
                }

                if ((gSaveContext.equips.buttonItems[0] != ITEM_SLINGSHOT) &&
                    (gSaveContext.equips.buttonItems[0] != ITEM_BOW) &&
                    (gSaveContext.equips.buttonItems[0] != ITEM_BOMBCHU) &&
                    (gSaveContext.equips.buttonItems[0] != ITEM_NONE || randoCanTrackSwordless)) {
                    gSaveContext.buttonStatus[0] = gSaveContext.equips.buttonItems[0];

                    // Track swordless status for restoration later
                    if (randoCanTrackSwordless) {
                        gSaveContext.buttonStatus[0] = SWORDLESS_STATUS;
                    }

                    if ((play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY) && Flags_GetSwitch(play, 0x38)) {
                        gSaveContext.equips.buttonItems[0] = ITEM_BOMBCHU;
                        Interface_LoadItemIcon1(play, 0);
                    } else {
                        gSaveContext.equips.buttonItems[0] = ITEM_BOW;
                        if (play->shootingGalleryStatus > 1) {
                            if (LINK_AGE_IN_YEARS == YEARS_CHILD) {
                                gSaveContext.equips.buttonItems[0] = ITEM_SLINGSHOT;
                            }

                            Interface_LoadItemIcon1(play, 0);
                        } else {
                            if (gSaveContext.inventory.items[SLOT_BOW] == ITEM_NONE) {
                                gSaveContext.equips.buttonItems[0] = ITEM_NONE;
                            } else {
                                Interface_LoadItemIcon1(play, 0);
                            }
                        }
                    }

                    gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] = gSaveContext.buttonStatus[3] =
                        BTN_DISABLED;
                    gSaveContext.buttonStatus[5] = gSaveContext.buttonStatus[6] = gSaveContext.buttonStatus[7] =
                        gSaveContext.buttonStatus[8] = BTN_DISABLED;
                    Interface_ChangeAlpha(6);
                }

                if (play->transitionMode != TRANS_MODE_OFF) {
                    Interface_ChangeAlpha(1);
                } else if (gSaveContext.minigameState == 1) {
                    Interface_ChangeAlpha(8);
                } else if (play->shootingGalleryStatus > 1) {
                    Interface_ChangeAlpha(8);
                } else if ((play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY) && Flags_GetSwitch(play, 0x38)) {
                    Interface_ChangeAlpha(8);
                } else if (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) {
                    Interface_ChangeAlpha(12);
                }
            } else {
                if (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) {
                    Interface_ChangeAlpha(12);
                }
            }
            // Don't hide the HUD in the Chamber of Sages when in Boss Rush.
        } else if (play->sceneNum == SCENE_CHAMBER_OF_THE_SAGES && !IS_BOSS_RUSH) {
            Interface_ChangeAlpha(1);
        } else if (play->sceneNum == SCENE_FISHING_POND) {
            gSaveContext.forceRisingButtonAlphas = 2;
            if (play->interfaceCtx.unk_260 != 0) {
                if (gSaveContext.equips.buttonItems[0] != ITEM_FISHING_POLE) {
                    gSaveContext.buttonStatus[0] = gSaveContext.equips.buttonItems[0];

                    // Track swordless status for restoration later
                    if (randoCanTrackSwordless) {
                        gSaveContext.buttonStatus[0] = SWORDLESS_STATUS;
                    }

                    gSaveContext.equips.buttonItems[0] = ITEM_FISHING_POLE;
                    gSaveContext.unk_13EA = 0;
                    Interface_LoadItemIcon1(play, 0);
                    Interface_ChangeAlpha(12);
                }

                if (gSaveContext.unk_13EA != 12) {
                    Interface_ChangeAlpha(12);
                }
            } else if (gSaveContext.equips.buttonItems[0] == ITEM_FISHING_POLE) {
                gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];
                gSaveContext.unk_13EA = 0;

                Interface_RandoRestoreSwordless();

                if (gSaveContext.equips.buttonItems[0] != ITEM_NONE) {
                    Interface_LoadItemIcon1(play, 0);
                }

                gSaveContext.buttonStatus[0] = gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] =
                    gSaveContext.buttonStatus[3] = BTN_DISABLED;
                gSaveContext.buttonStatus[5] = gSaveContext.buttonStatus[6] = gSaveContext.buttonStatus[7] =
                    gSaveContext.buttonStatus[8] = BTN_DISABLED;
                Interface_ChangeAlpha(50);
            } else {
                if (gSaveContext.buttonStatus[0] == BTN_ENABLED) {
                    gSaveContext.unk_13EA = 0;
                }

                gSaveContext.buttonStatus[0] = gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] =
                    gSaveContext.buttonStatus[3] = BTN_DISABLED;
                gSaveContext.buttonStatus[5] = gSaveContext.buttonStatus[6] = gSaveContext.buttonStatus[7] =
                    gSaveContext.buttonStatus[8] = BTN_DISABLED;
                Interface_ChangeAlpha(50);
            }
        } else if (msgCtx->msgMode == MSGMODE_NONE) {
            if (GameInteractor_PacifistModeActive()) {
                gSaveContext.buttonStatus[0] = gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] =
                    gSaveContext.buttonStatus[3] = gSaveContext.buttonStatus[5] = gSaveContext.buttonStatus[6] =
                        gSaveContext.buttonStatus[7] = gSaveContext.buttonStatus[8] = BTN_DISABLED;
            } else if ((Player_GetEnvironmentalHazard(play) >= 2) && (Player_GetEnvironmentalHazard(play) < 5)) {
                if (gSaveContext.buttonStatus[0] != BTN_DISABLED) {
                    sp28 = 1;
                }

                gSaveContext.buttonStatus[0] = BTN_DISABLED;

                for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                    if ((gSaveContext.equips.buttonItems[i] >= ITEM_SHIELD_DEKU) &&
                        (gSaveContext.equips.buttonItems[i] <= ITEM_BOOTS_HOVER)) {
                        // Equipment on c-buttons is always enabled
                        if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                            sp28 = 1;
                        }

                        gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                    } else if (Player_GetEnvironmentalHazard(play) == 2) {
                        if ((gSaveContext.equips.buttonItems[i] != ITEM_HOOKSHOT) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_LONGSHOT)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                        } else {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    } else {
                        if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                            sp28 = 1;
                        }

                        gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                    }
                }

                if (sp28) {
                    gSaveContext.unk_13EA = 0;
                }

                Interface_ChangeAlpha(50);
            } else if ((player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) ||
                       (player->stateFlags2 & PLAYER_STATE2_CRAWLING)) {
                if (gSaveContext.buttonStatus[0] != BTN_DISABLED) {
                    gSaveContext.buttonStatus[0] = BTN_DISABLED;
                    gSaveContext.buttonStatus[1] = BTN_DISABLED;
                    gSaveContext.buttonStatus[2] = BTN_DISABLED;
                    gSaveContext.buttonStatus[3] = BTN_DISABLED;
                    gSaveContext.buttonStatus[5] = BTN_DISABLED;
                    gSaveContext.buttonStatus[6] = BTN_DISABLED;
                    gSaveContext.buttonStatus[7] = BTN_DISABLED;
                    gSaveContext.buttonStatus[8] = BTN_DISABLED;
                    gSaveContext.unk_13EA = 0;
                    Interface_ChangeAlpha(50);
                }
            } else if ((gSaveContext.eventInf[0] & 0xF) == 1) {
                if (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) {
                    if ((gSaveContext.equips.buttonItems[0] != ITEM_NONE) &&
                        (gSaveContext.equips.buttonItems[0] != ITEM_BOW)) {
                        if (gSaveContext.inventory.items[SLOT_BOW] == ITEM_NONE) {
                            gSaveContext.equips.buttonItems[0] = ITEM_NONE;
                        } else {
                            gSaveContext.equips.buttonItems[0] = ITEM_BOW;
                            sp28 = 1;
                        }
                    }
                } else {
                    sp28 = 1;

                    if ((gSaveContext.equips.buttonItems[0] == ITEM_NONE) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_BOW)) {

                        if ((gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI) &&
                            (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_MASTER) &&
                            (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_BGS) &&
                            (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KNIFE)) {
                            gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];

                            Interface_RandoRestoreSwordless();
                        } else {
                            gSaveContext.buttonStatus[0] = gSaveContext.equips.buttonItems[0];
                        }
                    }
                }

                if (sp28) {
                    Interface_LoadItemIcon1(play, 0);
                    sp28 = 0;
                }

                for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                    if ((gSaveContext.equips.buttonItems[i] != ITEM_OCARINA_FAIRY) &&
                        (gSaveContext.equips.buttonItems[i] != ITEM_OCARINA_TIME)) {
                        if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                            sp28 = 1;
                        }

                        gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                    } else {
                        if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                            sp28 = 1;
                        }

                        gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                    }
                }

                if (sp28) {
                    gSaveContext.unk_13EA = 0;
                }

                Interface_ChangeAlpha(50);
            } else {
                if (interfaceCtx->restrictions.bButton == 0) {
                    if ((gSaveContext.equips.buttonItems[0] == ITEM_SLINGSHOT) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_BOW) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_BOMBCHU) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_NONE)) {
                        if ((gSaveContext.equips.buttonItems[0] != ITEM_NONE) || (gSaveContext.infTable[29] == 0) ||
                            randoWasSwordlessBefore) {
                            gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];

                            Interface_RandoRestoreSwordless();

                            sp28 = 1;

                            if (gSaveContext.equips.buttonItems[0] != ITEM_NONE) {
                                Interface_LoadItemIcon1(play, 0);
                            }
                        }
                    } else if ((gSaveContext.buttonStatus[0] & 0xFF) == BTN_DISABLED) {
                        sp28 = 1;

                        if (((gSaveContext.buttonStatus[0] & 0xFF) == BTN_DISABLED) ||
                            ((gSaveContext.buttonStatus[0] & 0xFF) == BTN_ENABLED)) {
                            gSaveContext.buttonStatus[0] = BTN_ENABLED;
                        } else {
                            gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0] & 0xFF;
                        }
                    }
                } else if (interfaceCtx->restrictions.bButton == 1) {
                    if ((gSaveContext.equips.buttonItems[0] == ITEM_SLINGSHOT) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_BOW) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_BOMBCHU) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_NONE)) {
                        if ((gSaveContext.equips.buttonItems[0] != ITEM_NONE) || (gSaveContext.infTable[29] == 0) ||
                            randoWasSwordlessBefore) {
                            gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];

                            Interface_RandoRestoreSwordless();

                            sp28 = 1;

                            if (gSaveContext.equips.buttonItems[0] != ITEM_NONE) {
                                Interface_LoadItemIcon1(play, 0);
                            }
                        }
                    } else {
                        if (gSaveContext.buttonStatus[0] == BTN_ENABLED) {
                            sp28 = 1;
                        }

                        gSaveContext.buttonStatus[0] = BTN_DISABLED;
                    }
                }

                for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                    if ((gSaveContext.equips.buttonItems[i] >= ITEM_SHIELD_DEKU) &&
                        (gSaveContext.equips.buttonItems[i] <= ITEM_BOOTS_HOVER)) {
                        // Equipment on c-buttons is always enabled
                        if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                            sp28 = 1;
                        }

                        gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                    }
                }

                if (interfaceCtx->restrictions.bottles != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                            (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                        }
                    }
                } else if (interfaceCtx->restrictions.bottles == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                            (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }

                if (interfaceCtx->restrictions.tradeItems != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((CVarGetInteger(CVAR_ENHANCEMENT("MMBunnyHood"), BUNNY_HOOD_VANILLA) !=
                             BUNNY_HOOD_VANILLA) &&
                            (gSaveContext.equips.buttonItems[i] >= ITEM_MASK_KEATON) &&
                            (gSaveContext.equips.buttonItems[i] <= ITEM_MASK_TRUTH)) {
                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        } else if ((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                                   (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                        }
                    }
                } else if (interfaceCtx->restrictions.tradeItems == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                            (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }

                if (interfaceCtx->restrictions.hookshot != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] == ITEM_HOOKSHOT) ||
                            (gSaveContext.equips.buttonItems[i] == ITEM_LONGSHOT)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                        }
                    }
                } else if (interfaceCtx->restrictions.hookshot == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] == ITEM_HOOKSHOT) ||
                            (gSaveContext.equips.buttonItems[i] == ITEM_LONGSHOT)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }

                if (interfaceCtx->restrictions.ocarina != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] == ITEM_OCARINA_FAIRY) ||
                            (gSaveContext.equips.buttonItems[i] == ITEM_OCARINA_TIME)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                        }
                    }
                } else if (interfaceCtx->restrictions.ocarina == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] == ITEM_OCARINA_FAIRY) ||
                            (gSaveContext.equips.buttonItems[i] == ITEM_OCARINA_TIME)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }

                if (interfaceCtx->restrictions.farores != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if (gSaveContext.equips.buttonItems[i] == ITEM_FARORES_WIND) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                            osSyncPrintf("***(i=%d)***  ", i);
                        }
                    }
                } else if (interfaceCtx->restrictions.farores == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if (gSaveContext.equips.buttonItems[i] == ITEM_FARORES_WIND) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }

                if (interfaceCtx->restrictions.dinsNayrus != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] == ITEM_DINS_FIRE) ||
                            (gSaveContext.equips.buttonItems[i] == ITEM_NAYRUS_LOVE)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                        }
                    }
                } else if (interfaceCtx->restrictions.dinsNayrus == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] == ITEM_DINS_FIRE) ||
                            (gSaveContext.equips.buttonItems[i] == ITEM_NAYRUS_LOVE)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }

                if (interfaceCtx->restrictions.all != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] != ITEM_OCARINA_FAIRY) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_OCARINA_TIME) &&
                            !((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                              (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) &&
                            !((gSaveContext.equips.buttonItems[i] >= ITEM_SHIELD_DEKU) && // Never disable equipment
                              (gSaveContext.equips.buttonItems[i] <=
                               ITEM_BOOTS_HOVER)) && // (tunics/boots) on C-buttons
                            !((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                              (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK))) {
                            if ((play->sceneNum != SCENE_TREASURE_BOX_SHOP) ||
                                (gSaveContext.equips.buttonItems[i] != ITEM_LENS)) {
                                if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                    sp28 = 1;
                                }

                                gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                            } else {
                                if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                    sp28 = 1;
                                }

                                gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                            }
                        }
                    }
                } else if (interfaceCtx->restrictions.all == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] != ITEM_DINS_FIRE) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_HOOKSHOT) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_LONGSHOT) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_FARORES_WIND) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_NAYRUS_LOVE) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_OCARINA_FAIRY) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_OCARINA_TIME) &&
                            !((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                              (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) &&
                            !((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                              (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK))) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }
            }
        }
    }

    if (sp28) {
        gSaveContext.unk_13EA = 0;
        if ((play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->transitionMode == TRANS_MODE_OFF)) {
            Interface_ChangeAlpha(50);
            osSyncPrintf("????????  alpha_change( 50 );  ?????\n");
        } else {
            osSyncPrintf("game_play->fade_direction || game_play->fbdemo_wipe_modem");
        }
    }
}

void Interface_SetSceneRestrictions(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 i;
    u8 currentScene;

    // clang-format off
    interfaceCtx->restrictions.hGauge = interfaceCtx->restrictions.bButton =
    interfaceCtx->restrictions.aButton = interfaceCtx->restrictions.bottles =
    interfaceCtx->restrictions.tradeItems = interfaceCtx->restrictions.hookshot =
    interfaceCtx->restrictions.ocarina = interfaceCtx->restrictions.warpSongs =
    interfaceCtx->restrictions.sunsSong = interfaceCtx->restrictions.farores =
    interfaceCtx->restrictions.dinsNayrus = interfaceCtx->restrictions.all = 0;
    // clang-format on

    i = 0;

    // "Data settings related to button display scene_data_ID=%d\n"
    osSyncPrintf("ボタン表示関係データ設定 scene_data_ID=%d\n", play->sceneNum);

    do {
        currentScene = (u8)play->sceneNum;
        if (sRestrictionFlags[i].scene == currentScene) {
            interfaceCtx->restrictions.hGauge = (sRestrictionFlags[i].flags1 & 0xC0) >> 6;
            interfaceCtx->restrictions.bButton = (sRestrictionFlags[i].flags1 & 0x30) >> 4;
            interfaceCtx->restrictions.aButton = (sRestrictionFlags[i].flags1 & 0x0C) >> 2;
            interfaceCtx->restrictions.bottles = (sRestrictionFlags[i].flags1 & 0x03) >> 0;
            interfaceCtx->restrictions.tradeItems = (sRestrictionFlags[i].flags2 & 0xC0) >> 6;
            interfaceCtx->restrictions.hookshot = (sRestrictionFlags[i].flags2 & 0x30) >> 4;
            interfaceCtx->restrictions.ocarina = (sRestrictionFlags[i].flags2 & 0x0C) >> 2;
            interfaceCtx->restrictions.warpSongs = (sRestrictionFlags[i].flags2 & 0x03) >> 0;
            interfaceCtx->restrictions.sunsSong = (sRestrictionFlags[i].flags3 & 0xC0) >> 6;
            interfaceCtx->restrictions.farores = (sRestrictionFlags[i].flags3 & 0x30) >> 4;
            interfaceCtx->restrictions.dinsNayrus = (sRestrictionFlags[i].flags3 & 0x0C) >> 2;
            interfaceCtx->restrictions.all = (sRestrictionFlags[i].flags3 & 0x03) >> 0;

            osSyncPrintf(VT_FGCOL(YELLOW));
            osSyncPrintf("parameter->button_status = %x,%x,%x\n", sRestrictionFlags[i].flags1,
                         sRestrictionFlags[i].flags2, sRestrictionFlags[i].flags3);
            osSyncPrintf("h_gage=%d, b_button=%d, a_button=%d, c_bottle=%d\n", interfaceCtx->restrictions.hGauge,
                         interfaceCtx->restrictions.bButton, interfaceCtx->restrictions.aButton,
                         interfaceCtx->restrictions.bottles);
            osSyncPrintf("c_warasibe=%d, c_hook=%d, c_ocarina=%d, c_warp=%d\n", interfaceCtx->restrictions.tradeItems,
                         interfaceCtx->restrictions.hookshot, interfaceCtx->restrictions.ocarina,
                         interfaceCtx->restrictions.warpSongs);
            osSyncPrintf("c_sunmoon=%d, m_wind=%d, m_magic=%d, another=%d\n", interfaceCtx->restrictions.sunsSong,
                         interfaceCtx->restrictions.farores, interfaceCtx->restrictions.dinsNayrus,
                         interfaceCtx->restrictions.all);
            osSyncPrintf(VT_RST);
            if (CVarGetInteger(CVAR_ENHANCEMENT("BetterFarore"), 0)) {
                if (currentScene == SCENE_GERUDO_TRAINING_GROUND || currentScene == SCENE_INSIDE_GANONS_CASTLE) {
                    interfaceCtx->restrictions.farores = 0;
                }
            }
            return;
        }
        i++;
    } while (sRestrictionFlags[i].scene != 0xFF);
}

Gfx* Gfx_TextureIA8(Gfx* displayListHead, void* texture, s16 textureWidth, s16 textureHeight, s16 rectLeft, s16 rectTop,
                    s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy) {
    gDPLoadTextureBlock(displayListHead++, texture, G_IM_FMT_IA, G_IM_SIZ_8b, textureWidth, textureHeight, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);

    gSPWideTextureRectangle(displayListHead++, rectLeft << 2, rectTop << 2, (rectLeft + rectWidth) << 2,
                            (rectTop + rectHeight) << 2, G_TX_RENDERTILE, 0, 0, dsdx, dtdy);

    return displayListHead;
}

Gfx* Gfx_TextureI8(Gfx* displayListHead, void* texture, s16 textureWidth, s16 textureHeight, s16 rectLeft, s16 rectTop,
                   s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy) {
    gDPLoadTextureBlock(displayListHead++, texture, G_IM_FMT_I, G_IM_SIZ_8b, textureWidth, textureHeight, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);

    gSPWideTextureRectangle(displayListHead++, rectLeft << 2, rectTop << 2, (rectLeft + rectWidth) << 2,
                            (rectTop + rectHeight) << 2, G_TX_RENDERTILE, 0, 0, dsdx, dtdy);

    return displayListHead;
}

void Rando_Inventory_SwapAgeEquipment(void) {
    s16 i;
    u16 shieldEquipValue;

    if (LINK_AGE_IN_YEARS == YEARS_CHILD) {
        for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            if (i != 0) {
                gSaveContext.childEquips.buttonItems[i] = gSaveContext.equips.buttonItems[i];
            } else {
                gSaveContext.childEquips.buttonItems[i] = ITEM_SWORD_KOKIRI;
            }

            if (i != 0) {
                gSaveContext.childEquips.cButtonSlots[i - 1] = gSaveContext.equips.cButtonSlots[i - 1];
            }
        }

        gSaveContext.childEquips.equipment = gSaveContext.equips.equipment;

        // When becoming adult, remove swordless flag since we'll get master sword
        // This gets set back appropriately later in the case of master sword shuffle
        Flags_UnsetInfTable(INFTABLE_SWORDLESS);

        // This section sets up the equipment on the first time going adult.
        // On master sword shuffle the check for the B button is insufficient, and so checking the equipment is
        // completely zero-ed is needed (Could just always use `gSaveContext.adultEquips.equipment == 0` for rando?)
        if (gSaveContext.adultEquips.buttonItems[0] == ITEM_NONE &&
            ((IS_RANDO && !Randomizer_GetSettingValue(RSK_SHUFFLE_MASTER_SWORD)) ||
             (gSaveContext.adultEquips.equipment == 0))) {
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_MASTER;

            if (gSaveContext.inventory.items[SLOT_NUT] != ITEM_NONE) {
                gSaveContext.equips.buttonItems[1] = ITEM_NUT;
                gSaveContext.equips.cButtonSlots[0] = SLOT_NUT;
            } else {
                gSaveContext.equips.buttonItems[1] = gSaveContext.equips.cButtonSlots[0] = ITEM_NONE;
            }

            gSaveContext.equips.buttonItems[2] = ITEM_BOMB;
            gSaveContext.equips.buttonItems[3] = gSaveContext.inventory.items[SLOT_OCARINA];
            gSaveContext.equips.cButtonSlots[1] = SLOT_BOMB;
            gSaveContext.equips.cButtonSlots[2] = SLOT_OCARINA;
            gSaveContext.equips.equipment = (EQUIP_VALUE_SWORD_MASTER << (EQUIP_TYPE_SWORD * 4)) |
                                            (EQUIP_VALUE_SHIELD_HYLIAN << (EQUIP_TYPE_SHIELD * 4)) |
                                            (EQUIP_VALUE_TUNIC_KOKIRI << (EQUIP_TYPE_TUNIC * 4)) |
                                            (EQUIP_VALUE_BOOTS_KOKIRI << (EQUIP_TYPE_BOOTS * 4));

            // In Master Sword Shuffle we want to override the equip of the master sword from the vanilla code
            // First check we have the Master sword in our inventory, and if not, then unequip
            if (IS_RANDO && Randomizer_GetSettingValue(RSK_SHUFFLE_MASTER_SWORD) &&
                !CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER)) {
                gSaveContext.equips.equipment &= (u16) ~(0xF << (EQUIP_TYPE_SWORD * 4));
                gSaveContext.equips.buttonItems[0] = ITEM_NONE;
                Flags_SetInfTable(INFTABLE_SWORDLESS);
            }
            // Set the dpad to nothing
            gSaveContext.equips.buttonItems[4] = ITEM_NONE;
            gSaveContext.equips.buttonItems[5] = ITEM_NONE;
            gSaveContext.equips.buttonItems[6] = ITEM_NONE;
            gSaveContext.equips.buttonItems[7] = ITEM_NONE;
            gSaveContext.equips.cButtonSlots[3] = SLOT_NONE;
            gSaveContext.equips.cButtonSlots[4] = SLOT_NONE;
            gSaveContext.equips.cButtonSlots[5] = SLOT_NONE;
            gSaveContext.equips.cButtonSlots[6] = SLOT_NONE;
        } else {
            for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                gSaveContext.equips.buttonItems[i] = gSaveContext.adultEquips.buttonItems[i];

                if (i != 0) {
                    gSaveContext.equips.cButtonSlots[i - 1] = gSaveContext.adultEquips.cButtonSlots[i - 1];
                }

                if (((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                     (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) ||
                    ((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                     (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK))) {
                    if (GameInteractor_Should(VB_SET_BUTTON_ITEM_FROM_C_BUTTON_SLOT, true, i)) {
                        gSaveContext.equips.buttonItems[i] =
                            gSaveContext.inventory.items[gSaveContext.equips.cButtonSlots[i - 1]];
                    }
                }
            }

            // In Master Sword Shuffle we want to set the swordless flag if no item is on the B button
            if (IS_RANDO && Randomizer_GetSettingValue(RSK_SHUFFLE_MASTER_SWORD) &&
                gSaveContext.equips.buttonItems[0] == ITEM_NONE) {
                Flags_SetInfTable(INFTABLE_SWORDLESS);
            }

            gSaveContext.equips.equipment = gSaveContext.adultEquips.equipment;
        }
    } else {

        for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            gSaveContext.adultEquips.buttonItems[i] = gSaveContext.equips.buttonItems[i];

            if (i != 0) {
                gSaveContext.adultEquips.cButtonSlots[i - 1] = gSaveContext.equips.cButtonSlots[i - 1];
            }
        }

        gSaveContext.adultEquips.equipment = gSaveContext.equips.equipment;

        if (gSaveContext.childEquips.buttonItems[0] != ITEM_NONE) {
            for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                gSaveContext.equips.buttonItems[i] = gSaveContext.childEquips.buttonItems[i];

                if (i != 0) {
                    gSaveContext.equips.cButtonSlots[i - 1] = gSaveContext.childEquips.cButtonSlots[i - 1];
                }

                if (((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                     (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) ||
                    ((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                     (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK))) {
                    gSaveContext.equips.buttonItems[i] =
                        gSaveContext.inventory.items[gSaveContext.equips.cButtonSlots[i - 1]];
                }
            }

            gSaveContext.equips.equipment = gSaveContext.childEquips.equipment;
            gSaveContext.equips.equipment &= (u16) ~(0xF << (EQUIP_TYPE_SWORD * 4));
            gSaveContext.equips.equipment |= EQUIP_VALUE_SWORD_KOKIRI << (EQUIP_TYPE_SWORD * 4);
        }
        // In Rando we need an extra case to handle starting as adult. We can use the fact that the childEquips will be
        // uninitialised (i.e. 0) at this point
        else if (gSaveContext.childEquips.equipment == 0) {

            // zero out items
            for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                gSaveContext.equips.buttonItems[i] = ITEM_NONE;
                if (i != 0) {
                    gSaveContext.equips.cButtonSlots[i - 1] = ITEM_NONE;
                }
            }
            gSaveContext.equips.equipment = (EQUIP_VALUE_SWORD_KOKIRI << (EQUIP_TYPE_SWORD * 4)) |
                                            (EQUIP_VALUE_SHIELD_DEKU << (EQUIP_TYPE_SHIELD * 4)) |
                                            (EQUIP_VALUE_TUNIC_KOKIRI << (EQUIP_TYPE_TUNIC * 4)) |
                                            (EQUIP_VALUE_BOOTS_KOKIRI << (EQUIP_TYPE_BOOTS * 4));
        }

        // When becoming child in rando, set swordless flag and clear B button if player doesn't have kokiri sword
        // Otherwise, equip sword and unset flag
        if (!CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI)) {
            gSaveContext.equips.equipment &= (u16) ~(0xF << (EQUIP_TYPE_SWORD * 4));
            gSaveContext.equips.buttonItems[0] = ITEM_NONE;
            Flags_SetInfTable(INFTABLE_SWORDLESS);
        } else {
            gSaveContext.equips.equipment &= (u16) ~(0xF << (EQUIP_TYPE_SWORD * 4));
            gSaveContext.equips.equipment |= (EQUIP_VALUE_SWORD_KOKIRI << (EQUIP_TYPE_SWORD * 4));
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KOKIRI;
            Flags_UnsetInfTable(INFTABLE_SWORDLESS);
        }
    }

    shieldEquipValue = gEquipMasks[EQUIP_TYPE_SHIELD] & gSaveContext.equips.equipment;
    if (shieldEquipValue) {
        shieldEquipValue >>= gEquipShifts[EQUIP_TYPE_SHIELD];
        if (!CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SHIELD, shieldEquipValue - 1)) {
            gSaveContext.equips.equipment &= gEquipNegMasks[EQUIP_TYPE_SHIELD];
        }
    }
}

void Inventory_SwapAgeEquipment(void) {
    s16 i;
    u16 shieldEquipValue;

    if (IS_RANDO) {
        Rando_Inventory_SwapAgeEquipment();
        return;
    }

    if (LINK_AGE_IN_YEARS == YEARS_CHILD) {
        for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            if (i != 0) {
                gSaveContext.childEquips.buttonItems[i] = gSaveContext.equips.buttonItems[i];
            } else {
                gSaveContext.childEquips.buttonItems[i] = ITEM_SWORD_KOKIRI;
            }

            if (i != 0) {
                gSaveContext.childEquips.cButtonSlots[i - 1] = gSaveContext.equips.cButtonSlots[i - 1];
            }
        }

        gSaveContext.childEquips.equipment = gSaveContext.equips.equipment;

        if (gSaveContext.adultEquips.buttonItems[0] == ITEM_NONE) {
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_MASTER;

            if (gSaveContext.inventory.items[SLOT_NUT] != ITEM_NONE) {
                gSaveContext.equips.buttonItems[1] = ITEM_NUT;
                gSaveContext.equips.cButtonSlots[0] = SLOT_NUT;
            } else {
                gSaveContext.equips.buttonItems[1] = gSaveContext.equips.cButtonSlots[0] = ITEM_NONE;
            }

            gSaveContext.equips.buttonItems[2] = ITEM_BOMB;
            gSaveContext.equips.buttonItems[3] = gSaveContext.inventory.items[SLOT_OCARINA];
            gSaveContext.equips.cButtonSlots[1] = SLOT_BOMB;
            gSaveContext.equips.cButtonSlots[2] = SLOT_OCARINA;
            gSaveContext.equips.equipment = (EQUIP_VALUE_SWORD_MASTER << (EQUIP_TYPE_SWORD * 4)) |
                                            (EQUIP_VALUE_SHIELD_HYLIAN << (EQUIP_TYPE_SHIELD * 4)) |
                                            (EQUIP_VALUE_TUNIC_KOKIRI << (EQUIP_TYPE_TUNIC * 4)) |
                                            (EQUIP_VALUE_BOOTS_KOKIRI << (EQUIP_TYPE_BOOTS * 4));
            // Set the dpad to nothing
            gSaveContext.equips.buttonItems[4] = ITEM_NONE;
            gSaveContext.equips.buttonItems[5] = ITEM_NONE;
            gSaveContext.equips.buttonItems[6] = ITEM_NONE;
            gSaveContext.equips.buttonItems[7] = ITEM_NONE;
            gSaveContext.equips.cButtonSlots[3] = SLOT_NONE;
            gSaveContext.equips.cButtonSlots[4] = SLOT_NONE;
            gSaveContext.equips.cButtonSlots[5] = SLOT_NONE;
            gSaveContext.equips.cButtonSlots[6] = SLOT_NONE;
        } else {
            for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                gSaveContext.equips.buttonItems[i] = gSaveContext.adultEquips.buttonItems[i];

                if (i != 0) {
                    gSaveContext.equips.cButtonSlots[i - 1] = gSaveContext.adultEquips.cButtonSlots[i - 1];
                }

                if (((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                     (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) ||
                    ((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                     (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK))) {
                    osSyncPrintf("Register_Item_Pt(%d)=%d\n", i, gSaveContext.equips.cButtonSlots[i - 1]);
                    if (GameInteractor_Should(VB_SET_BUTTON_ITEM_FROM_C_BUTTON_SLOT, true, i)) {
                        gSaveContext.equips.buttonItems[i] =
                            gSaveContext.inventory.items[gSaveContext.equips.cButtonSlots[i - 1]];
                    }
                }
            }

            gSaveContext.equips.equipment = gSaveContext.adultEquips.equipment;
        }
    } else {
        for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            gSaveContext.adultEquips.buttonItems[i] = gSaveContext.equips.buttonItems[i];

            if (i != 0) {
                gSaveContext.adultEquips.cButtonSlots[i - 1] = gSaveContext.equips.cButtonSlots[i - 1];
            }
        }

        gSaveContext.adultEquips.equipment = gSaveContext.equips.equipment;

        if (gSaveContext.childEquips.buttonItems[0] != ITEM_NONE) {
            for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                gSaveContext.equips.buttonItems[i] = gSaveContext.childEquips.buttonItems[i];

                if (i != 0) {
                    gSaveContext.equips.cButtonSlots[i - 1] = gSaveContext.childEquips.cButtonSlots[i - 1];
                }

                if (((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                     (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) ||
                    ((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                     (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK))) {
                    osSyncPrintf("Register_Item_Pt(%d)=%d\n", i, gSaveContext.equips.cButtonSlots[i - 1]);
                    gSaveContext.equips.buttonItems[i] =
                        gSaveContext.inventory.items[gSaveContext.equips.cButtonSlots[i - 1]];
                }
            }

            gSaveContext.equips.equipment = gSaveContext.childEquips.equipment;
            gSaveContext.equips.equipment &= (u16) ~(0xF << (EQUIP_TYPE_SWORD * 4));
            gSaveContext.equips.equipment |= EQUIP_VALUE_SWORD_KOKIRI << (EQUIP_TYPE_SWORD * 4);
        }
    }

    shieldEquipValue = gEquipMasks[EQUIP_TYPE_SHIELD] & gSaveContext.equips.equipment;
    if (shieldEquipValue) {
        shieldEquipValue >>= gEquipShifts[EQUIP_TYPE_SHIELD];
        if (!CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SHIELD, shieldEquipValue - 1)) {
            gSaveContext.equips.equipment &= gEquipNegMasks[EQUIP_TYPE_SHIELD];
        }
    }
}

void Interface_InitHorsebackArchery(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    gSaveContext.minigameState = 1;
    interfaceCtx->unk_23C = interfaceCtx->unk_240 = interfaceCtx->unk_242 = 0;
    gSaveContext.minigameScore = sHBAScoreTier = 0;
    interfaceCtx->hbaAmmo = 20;
}

void func_800849EC(PlayState* play) {
    gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
    gSaveContext.inventory.equipment ^= OWNED_EQUIP_FLAG_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);

    if (CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
        gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KNIFE;
    } else {
        gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;
    }

    Interface_LoadItemIcon1(play, 0);
}

static s16 Interface_GetLocalMultiplayerPlayerCount(void) {
    s16 playerCount;

    if (CVarGetInteger(LOCAL_MP_DISABLED_CVAR, 0)) {
        return 1;
    }

    playerCount = CVarGetInteger(LOCAL_MP_PLAYER_COUNT_CVAR, 2);
    if (playerCount < 1) {
        playerCount = 1;
    } else if (playerCount > 4) {
        playerCount = 4;
    }

    return playerCount;
}

static s32 Interface_IsSplitScreenEnabled(void) {
    return !CVarGetInteger(LOCAL_MP_DISABLED_CVAR, 0) && CVarGetInteger(LOCAL_MP_SPLITSCREEN_CVAR, 1);
}

static void Interface_GetSplitViewportTopLeft(s16 playerCount, s16 playerIndex, s16* leftX, s16* topY) {
    s16 halfWidth = SCREEN_WIDTH / 2;
    s16 halfHeight = SCREEN_HEIGHT / 2;

    *leftX = 0;
    *topY = 0;

    if (playerCount <= 1) {
        return;
    }

    if (playerCount == 2) {
        if (CVarGetInteger(LOCAL_MP_SPLITSCREEN_VERTICAL_CVAR, 1)) {
            *leftX = (playerIndex == 0) ? 0 : halfWidth;
        } else {
            *topY = (playerIndex == 0) ? 0 : halfHeight;
        }
        return;
    }

    if (playerCount == 3) {
        if (playerIndex == 0) {
            return;
        }

        *topY = halfHeight;
        *leftX = (playerIndex == 1) ? 0 : halfWidth;
        return;
    }

    *topY = (playerIndex < 2) ? 0 : halfHeight;
    *leftX = ((playerIndex % 2) == 0) ? 0 : halfWidth;
}

static void Interface_GetSplitViewportBounds(s16 playerCount, s16 playerIndex, Viewport* viewport) {
    s16 halfWidth = SCREEN_WIDTH / 2;
    s16 halfHeight = SCREEN_HEIGHT / 2;

    viewport->topY = 0;
    viewport->bottomY = SCREEN_HEIGHT;
    viewport->leftX = 0;
    viewport->rightX = SCREEN_WIDTH;

    if (playerCount <= 1) {
        return;
    }

    if (playerCount == 2) {
        if (CVarGetInteger(LOCAL_MP_SPLITSCREEN_VERTICAL_CVAR, 1)) {
            viewport->leftX = (playerIndex == 0) ? 0 : halfWidth;
            viewport->rightX = (playerIndex == 0) ? halfWidth : SCREEN_WIDTH;
        } else {
            viewport->topY = (playerIndex == 0) ? 0 : halfHeight;
            viewport->bottomY = (playerIndex == 0) ? halfHeight : SCREEN_HEIGHT;
        }
        return;
    }

    if (playerCount == 3) {
        if (playerIndex == 0) {
            viewport->topY = 0;
            viewport->bottomY = halfHeight;
            viewport->leftX = 0;
            viewport->rightX = SCREEN_WIDTH;
        } else if (playerIndex == 1) {
            viewport->topY = halfHeight;
            viewport->bottomY = SCREEN_HEIGHT;
            viewport->leftX = 0;
            viewport->rightX = halfWidth;
        } else {
            viewport->topY = halfHeight;
            viewport->bottomY = SCREEN_HEIGHT;
            viewport->leftX = halfWidth;
            viewport->rightX = SCREEN_WIDTH;
        }
        return;
    }

    viewport->topY = (playerIndex < 2) ? 0 : halfHeight;
    viewport->bottomY = (playerIndex < 2) ? halfHeight : SCREEN_HEIGHT;
    viewport->leftX = ((playerIndex % 2) == 0) ? 0 : halfWidth;
    viewport->rightX = ((playerIndex % 2) == 0) ? halfWidth : SCREEN_WIDTH;
}

static s16 Interface_NormalizeQuickItemSlotIndex(s16 slotIndex);

static s32 Interface_IsOriginalMultiplayerItemModeEnabled(void) {
    return CVarGetInteger(LOCAL_MP_ORIGINAL_ITEMS_CVAR, 1) &&
           (Interface_GetLocalMultiplayerPlayerCount() > 1);
}

static s32 Interface_IsSecondQuickItemSlotEnabled(void) {
    return Interface_IsOriginalMultiplayerItemModeEnabled() &&
           CVarGetInteger(LOCAL_MP_SECOND_ITEM_SLOT_CVAR, 1);
}

static s32 Interface_IsSecondQuickItemSlotSideBySideEnabled(void) {
    return Interface_IsSecondQuickItemSlotEnabled() &&
           CVarGetInteger(LOCAL_MP_SECOND_ITEM_SLOT_SIDE_BY_SIDE_CVAR, 0);
}

static s32 Interface_IsRadialQuickItemMenuEnabled(void) {
    return Interface_IsOriginalMultiplayerItemModeEnabled() &&
           CVarGetInteger(LOCAL_MP_RADIAL_QUICK_ITEM_MENU_CVAR, 0);
}

static s32 Interface_CanCycleToItem(u8 item) {
    if (LINK_AGE_IN_YEARS == YEARS_CHILD) {
        switch (item) {
            case ITEM_BOW:
            case ITEM_ARROW_FIRE:
            case ITEM_ARROW_ICE:
            case ITEM_ARROW_LIGHT:
            case ITEM_HOOKSHOT:
            case ITEM_LONGSHOT:
            case ITEM_HAMMER:
            case ITEM_BOW_ARROW_FIRE:
            case ITEM_BOW_ARROW_ICE:
            case ITEM_BOW_ARROW_LIGHT:
                return false;

            default:
                return true;
        }
    }

    switch (item) {
        case ITEM_STICK:
        case ITEM_SLINGSHOT:
        case ITEM_BOOMERANG:
        case ITEM_BEAN:
            return false;

        default:
            return true;
    }
}

static s32 Interface_IsMultiplayerQuickMenuItemBlocked(u8 item) {
    switch (item) {
        case ITEM_OCARINA_FAIRY:
        case ITEM_OCARINA_TIME:
        case ITEM_BEAN:
        case ITEM_LENS:
        case ITEM_DINS_FIRE:
        case ITEM_NAYRUS_LOVE:
        case ITEM_FARORES_WIND:
        case ITEM_WEIRD_EGG:
        case ITEM_CHICKEN:
        case ITEM_LETTER_ZELDA:
        case ITEM_POCKET_EGG:
        case ITEM_POCKET_CUCCO:
        case ITEM_COJIRO:
        case ITEM_ODD_MUSHROOM:
        case ITEM_ODD_POTION:
        case ITEM_SAW:
        case ITEM_SWORD_BROKEN:
        case ITEM_PRESCRIPTION:
        case ITEM_FROG:
        case ITEM_EYEDROPS:
        case ITEM_CLAIM_CHECK:
            return true;

        default:
            return false;
    }
}

static Player* Interface_FindPlayerByPort(PlayState* play, u8 controllerPort) {
    Player* player;

    if ((controllerPort < 1) || (controllerPort > 4)) {
        return NULL;
    }

    player = GET_PLAYER(play);

    while ((player != NULL) && (player->actor.id == ACTOR_PLAYER)) {
        if (!player->isSecondPlayer) {
            if (controllerPort == 1) {
                return player;
            }
        } else if (player->controllerPort == controllerPort) {
            return player;
        }

        player = (Player*)player->actor.next;
    }

    return NULL;
}

static s32 Interface_CanUseRadialQuickMenuForPort(PlayState* play, u8 controllerPort) {
    Player* player = Interface_FindPlayerByPort(play, controllerPort);

    if (controllerPort == 1) {
        return player != NULL;
    }

    if ((player == NULL) || !player->isSecondPlayer) {
        return false;
    }

    return player->inputEnabled;
}

static void Interface_ClearRadialQuickMenuState(LocalMPRadialQuickMenuState* state) {
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

static void Interface_ClearAllRadialQuickMenuStates(void) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sRadialQuickMenuStates); i++) {
        Interface_ClearRadialQuickMenuState(&sRadialQuickMenuStates[i]);
    }
}

static void Interface_BuildRadialQuickMenuItemsForPort(PlayState* play, u8 controllerPort, u8 slotIndex,
                                                        LocalMPRadialQuickMenuState* state) {
    s32 i;
    u8 currentItem;
    s16 currentInventorySlot;
    u8 seenItems[ITEM_NONE_FE];

    (void)play;

    if (state == NULL) {
        return;
    }

    memset(seenItems, 0, sizeof(seenItems));

    state->itemCount = 0;
    state->selectedIndex = 0;
    state->slotIndex = Interface_NormalizeQuickItemSlotIndex(slotIndex);

    for (i = 0; i < 24; i++) {
        u8 item = gSaveContext.inventory.items[i];

        if ((item == ITEM_NONE) || (item == ITEM_SOLD_OUT) || (item >= ITEM_NONE_FE)) {
            continue;
        }

        if (!Interface_CanCycleToItem(item)) {
            continue;
        }

        if ((controllerPort > 1) && Interface_IsMultiplayerQuickMenuItemBlocked(item)) {
            continue;
        }

        if (seenItems[item]) {
            continue;
        }

        if (state->itemCount >= ARRAY_COUNT(state->items)) {
            break;
        }

        seenItems[item] = true;
        state->items[state->itemCount] = item;
        state->inventorySlots[state->itemCount] = i;
        state->itemCount++;
    }

    if (state->itemCount == 0) {
        return;
    }

    currentItem = Interface_GetQuickItemForPortAndSlot(controllerPort, state->slotIndex);
    currentInventorySlot = Interface_GetQuickItemInventorySlotForPortAndSlot(controllerPort, state->slotIndex);

    for (i = 0; i < state->itemCount; i++) {
        if ((state->inventorySlots[i] == currentInventorySlot) && (state->items[i] == currentItem)) {
            state->selectedIndex = i;
            return;
        }
    }

    for (i = 0; i < state->itemCount; i++) {
        if (state->items[i] == currentItem) {
            state->selectedIndex = i;
            return;
        }
    }
}

static void Interface_UpdateRadialQuickMenuSelectionForPort(PlayState* play, u8 controllerPort,
                                                            LocalMPRadialQuickMenuState* state) {
    Input* input;
    s16 stickX;
    s16 stickY;
    s32 stickMagnitudeSq;
    f32 angle;
    u32 scaledIndex;

    if ((state == NULL) || (state->itemCount == 0)) {
        return;
    }

    if ((controllerPort < 1) || (controllerPort > ARRAY_COUNT(play->state.input))) {
        return;
    }

    input = &play->state.input[controllerPort - 1];
    stickX = input->cur.stick_x;
    stickY = input->cur.stick_y;
    stickMagnitudeSq = SQ(stickX) + SQ(stickY);

    if (stickMagnitudeSq < SQ(30)) {
        return;
    }

    // Use atan2(x, y) so 0 radians points straight up, matching the wheel layout.
    angle = Math_FAtan2F(stickX, stickY);

    if (angle < 0.0f) {
        angle += (2.0f * M_PI);
    }

    scaledIndex = (u32)(((angle / (2.0f * M_PI)) * state->itemCount) + 0.5f);

    if (scaledIndex >= state->itemCount) {
        scaledIndex = 0;
    }

    if (state->selectedIndex != scaledIndex) {
        state->selectedIndex = scaledIndex;
        Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

static void Interface_UpdateRadialQuickMenu(PlayState* play) {
    s32 playerCount;
    s32 port;

    if (!Interface_IsRadialQuickItemMenuEnabled()) {
        Interface_ClearAllRadialQuickMenuStates();
        return;
    }

    playerCount = Interface_GetLocalMultiplayerPlayerCount();
    if (playerCount <= 1) {
        Interface_ClearAllRadialQuickMenuStates();
        return;
    }

    if (Player_InCsMode(play) || (play->pauseCtx.state != 0) || (play->transitionTrigger != TRANS_TRIGGER_OFF)) {
        Interface_ClearAllRadialQuickMenuStates();
        return;
    }

    for (port = 1; port <= ARRAY_COUNT(sRadialQuickMenuStates); port++) {
        LocalMPRadialQuickMenuState* state = &sRadialQuickMenuStates[port - 1];
        Input* input;
        s32 holdActive;
        s32 holdReleased;
        u8 slotIndex;

        if ((port > playerCount) || !Interface_CanUseRadialQuickMenuForPort(play, port)) {
            Interface_ClearRadialQuickMenuState(state);
            continue;
        }

        input = &play->state.input[port - 1];
        holdActive = CHECK_BTN_ANY(input->cur.button, BTN_DLEFT | BTN_DRIGHT);
        holdReleased = CHECK_BTN_ANY(input->rel.button, BTN_DLEFT | BTN_DRIGHT);
        slotIndex = Interface_GetQuickItemSelectedSlotForPort(port);

        if (holdActive) {
            if (!state->active || (state->slotIndex != Interface_NormalizeQuickItemSlotIndex(slotIndex))) {
                Interface_BuildRadialQuickMenuItemsForPort(play, port, slotIndex, state);
                state->active = true;
            }

            Interface_UpdateRadialQuickMenuSelectionForPort(play, port, state);
        } else if (state->active) {
            if ((state->itemCount > 0) && (state->selectedIndex < state->itemCount) &&
                (holdReleased || !holdActive)) {
                u8 selectedItem = state->items[state->selectedIndex];
                s16 selectedInventorySlot = state->inventorySlots[state->selectedIndex];
                u8 currentItem = Interface_GetQuickItemForPortAndSlot(port, state->slotIndex);
                s16 currentInventorySlot = Interface_GetQuickItemInventorySlotForPortAndSlot(port, state->slotIndex);

                if ((selectedItem != currentItem) || (selectedInventorySlot != currentInventorySlot)) {
                    Interface_SetQuickItemForPortAndSlot(play, port, state->slotIndex, selectedItem,
                                                         selectedInventorySlot);
                    Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                }
            }

            Interface_ClearRadialQuickMenuState(state);
        }
    }
}

void Interface_LoadItemIconForItem(PlayState* play, u16 button, u8 item) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    if (item >= ITEM_NONE_FE) {
        return;
    }

    osCreateMesgQueue(&interfaceCtx->loadQueue, &interfaceCtx->loadMsg, OS_MESG_BLOCK);
    DmaMgr_SendRequest2(&interfaceCtx->dmaRequest_160, interfaceCtx->iconItemSegment + button * 0x1000,
                        (uintptr_t)_icon_item_staticSegmentRomStart + (item * 0x1000), 0x1000, 0,
                        &interfaceCtx->loadQueue, OS_MESG_PTR(NULL), __FILE__, __LINE__);
    osRecvMesg(&interfaceCtx->loadQueue, NULL, OS_MESG_BLOCK);
}

static s16 Interface_GetQuickItemButtonIndexImpl(u8 controllerPort) {
    switch (controllerPort) {
        case 4:
            return 4;

        case 3:
            return 2;

        case 2:
            return 3;

        case 1:
        default:
            return 1;
    }
}

s16 Interface_GetQuickItemButtonIndex(u8 controllerPort) {
    return Interface_GetQuickItemButtonIndexImpl(controllerPort);
}

static s16 Interface_NormalizeQuickItemSlotIndex(s16 slotIndex) {
    if (!Interface_IsSecondQuickItemSlotEnabled()) {
        return 0;
    }

    return (slotIndex != 0) ? 1 : 0;
}

static s16 Interface_GetQuickItemPortArrayIndex(u8 controllerPort) {
    if ((controllerPort < 1) || (controllerPort > 4)) {
        return -1;
    }

    return controllerPort - 1;
}

static s16 Interface_GetQuickItemControllerPortByButtonIndex(s16 buttonIndex) {
    switch (buttonIndex) {
        case 1:
            return 1;

        case 3:
            return 2;

        case 2:
            return 3;

        case 4:
            return 4;

        default:
            return 0;
    }
}

s16 Interface_GetQuickItemInventorySlotForPortAndSlot(u8 controllerPort, s16 slotIndex) {
    s16 portIndex = Interface_GetQuickItemPortArrayIndex(controllerPort);

    if (portIndex < 0) {
        return SLOT_NONE;
    }

    if (Interface_NormalizeQuickItemSlotIndex(slotIndex) == 1) {
        return sQuickItemSlot2InventorySlot[portIndex];
    }

    switch (controllerPort) {
        case 4:
            if (Interface_IsOriginalMultiplayerItemModeEnabled()) {
                return sP4QuickItemSlot;
            }
            return gSaveContext.equips.cButtonSlots[3];

        case 3:
            return gSaveContext.equips.cButtonSlots[1];

        case 2:
            return gSaveContext.equips.cButtonSlots[2];

        case 1:
        default:
            return gSaveContext.equips.cButtonSlots[0];
    }
}

s16 Interface_GetQuickItemInventorySlotForPort(u8 controllerPort) {
    return Interface_GetQuickItemInventorySlotForPortAndSlot(controllerPort, 0);
}

u8 Interface_GetQuickItemForPortAndSlot(u8 controllerPort, s16 slotIndex) {
    s16 buttonIndex = Interface_GetQuickItemButtonIndexImpl(controllerPort);
    s16 inventorySlot;
    s16 portIndex = Interface_GetQuickItemPortArrayIndex(controllerPort);

    if (portIndex < 0) {
        return ITEM_NONE;
    }

    slotIndex = Interface_NormalizeQuickItemSlotIndex(slotIndex);

    if (slotIndex == 1) {
        inventorySlot = sQuickItemSlot2InventorySlot[portIndex];

        if ((inventorySlot >= 0) && (inventorySlot < 24)) {
            return gSaveContext.inventory.items[inventorySlot];
        }

        return sQuickItemSlot2[portIndex];
    }

    if ((controllerPort == 4) && Interface_IsOriginalMultiplayerItemModeEnabled()) {
        if ((sP4QuickItemSlot >= 0) && (sP4QuickItemSlot < 24)) {
            return gSaveContext.inventory.items[sP4QuickItemSlot];
        }

        return sP4QuickItem;
    }

    if ((buttonIndex >= 1) && (buttonIndex < ARRAY_COUNT(gSaveContext.equips.buttonItems))) {
        return gSaveContext.equips.buttonItems[buttonIndex];
    }

    return ITEM_NONE;
}

u8 Interface_GetQuickItemForPort(u8 controllerPort) {
    return Interface_GetQuickItemForPortAndSlot(controllerPort, 0);
}

void Interface_SetQuickItemSelectedSlotForPort(u8 controllerPort, s16 slotIndex) {
    s16 portIndex = Interface_GetQuickItemPortArrayIndex(controllerPort);

    if (portIndex < 0) {
        return;
    }

    sQuickItemSelectedSlot[portIndex] = Interface_NormalizeQuickItemSlotIndex(slotIndex);
}

s16 Interface_GetQuickItemSelectedSlotForPort(u8 controllerPort) {
    s16 portIndex = Interface_GetQuickItemPortArrayIndex(controllerPort);

    if (portIndex < 0) {
        return 0;
    }

    sQuickItemSelectedSlot[portIndex] = Interface_NormalizeQuickItemSlotIndex(sQuickItemSelectedSlot[portIndex]);
    return sQuickItemSelectedSlot[portIndex];
}

void Interface_SetQuickItemUseSlotForPort(u8 controllerPort, s16 slotIndex) {
    s16 portIndex = Interface_GetQuickItemPortArrayIndex(controllerPort);

    if (portIndex < 0) {
        return;
    }

    sQuickItemUseSlot[portIndex] = Interface_NormalizeQuickItemSlotIndex(slotIndex);
}

s16 Interface_GetQuickItemUseSlotForPort(u8 controllerPort) {
    s16 portIndex = Interface_GetQuickItemPortArrayIndex(controllerPort);

    if (portIndex < 0) {
        return 0;
    }

    sQuickItemUseSlot[portIndex] = Interface_NormalizeQuickItemSlotIndex(sQuickItemUseSlot[portIndex]);
    return sQuickItemUseSlot[portIndex];
}

void Interface_ResetExtraItemData(void) {
    s32 i;

    sP4QuickItem = ITEM_NONE;
    sP4QuickItemSlot = SLOT_NONE;

    for (i = 0; i < ARRAY_COUNT(sQuickItemSlot2); i++) {
        sQuickItemSlot2[i] = ITEM_NONE;
        sQuickItemSlot2InventorySlot[i] = SLOT_NONE;
        sQuickItemSelectedSlot[i] = 0;
        sQuickItemUseSlot[i] = 0;
    }

    Interface_ClearAllRadialQuickMenuStates();

    Magic_ResetExtraData();
}

static void Interface_SetQuickItemForPortAndSlotInternal(PlayState* play, u8 controllerPort, s16 slotIndex, u8 item,
                                                         s16 inventorySlot) {
    s16 buttonIndex = Interface_GetQuickItemButtonIndexImpl(controllerPort);
    s16 portIndex = Interface_GetQuickItemPortArrayIndex(controllerPort);

    if (portIndex < 0) {
        return;
    }

    slotIndex = Interface_NormalizeQuickItemSlotIndex(slotIndex);

    if (slotIndex == 1) {
        sQuickItemSlot2[portIndex] = item;
        if ((inventorySlot >= 0) && (inventorySlot < 24)) {
            sQuickItemSlot2InventorySlot[portIndex] = inventorySlot;
        } else if (item == ITEM_NONE) {
            sQuickItemSlot2InventorySlot[portIndex] = SLOT_NONE;
        }

        return;
    }

    if ((controllerPort == 4) && Interface_IsOriginalMultiplayerItemModeEnabled()) {
        sP4QuickItem = item;
        if ((inventorySlot >= 0) && (inventorySlot < 24)) {
            sP4QuickItemSlot = inventorySlot;
        } else if (item == ITEM_NONE) {
            sP4QuickItemSlot = SLOT_NONE;
        }

        if (item < ITEM_NONE_FE) {
            Interface_LoadItemIconForItem(play, 4, item);
        }
        return;
    }

    if ((buttonIndex >= 1) && (buttonIndex < ARRAY_COUNT(gSaveContext.equips.buttonItems))) {
        gSaveContext.equips.buttonItems[buttonIndex] = item;
        if ((inventorySlot >= 0) && (inventorySlot < 24) && (buttonIndex - 1 < ARRAY_COUNT(gSaveContext.equips.cButtonSlots))) {
            gSaveContext.equips.cButtonSlots[buttonIndex - 1] = inventorySlot;
        } else if ((item == ITEM_NONE) && (buttonIndex - 1 < ARRAY_COUNT(gSaveContext.equips.cButtonSlots))) {
            gSaveContext.equips.cButtonSlots[buttonIndex - 1] = SLOT_NONE;
        }
        Interface_LoadItemIcon1(play, buttonIndex);
    }
}

void Interface_SetQuickItemForPortAndSlot(PlayState* play, u8 controllerPort, s16 slotIndex, u8 item,
                                          s16 inventorySlot) {
    if ((controllerPort < 1) || (controllerPort > 4)) {
        return;
    }

    Interface_SetQuickItemForPortAndSlotInternal(play, controllerPort, slotIndex, item, inventorySlot);
}

void Interface_SetQuickItemForPort(PlayState* play, u8 controllerPort, u8 item, s16 inventorySlot) {
    Interface_SetQuickItemForPortAndSlot(play, controllerPort, 0, item, inventorySlot);
}

void Interface_LoadItemIcon1(PlayState* play, u16 button) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    osCreateMesgQueue(&interfaceCtx->loadQueue, &interfaceCtx->loadMsg, OS_MESG_BLOCK);
    DmaMgr_SendRequest2(&interfaceCtx->dmaRequest_160, interfaceCtx->iconItemSegment + button * 0x1000,
                        (uintptr_t)_icon_item_staticSegmentRomStart +
                            (gSaveContext.equips.buttonItems[button] * 0x1000),
                        0x1000, 0, &interfaceCtx->loadQueue, OS_MESG_PTR(NULL), __FILE__, __LINE__);
    osRecvMesg(&interfaceCtx->loadQueue, NULL, OS_MESG_BLOCK);
}

void Interface_LoadItemIcon2(PlayState* play, u16 button) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    osCreateMesgQueue(&interfaceCtx->loadQueue, &interfaceCtx->loadMsg, OS_MESG_BLOCK);
    DmaMgr_SendRequest2(&interfaceCtx->dmaRequest_180, interfaceCtx->iconItemSegment + button * 0x1000,
                        (uintptr_t)_icon_item_staticSegmentRomStart +
                            (gSaveContext.equips.buttonItems[button] * 0x1000),
                        0x1000, 0, &interfaceCtx->loadQueue, OS_MESG_PTR(NULL), __FILE__, __LINE__);
    osRecvMesg(&interfaceCtx->loadQueue, NULL, OS_MESG_BLOCK);
}

void func_80084BF4(PlayState* play, u16 flag) {
    if (flag) {
        if ((gSaveContext.equips.buttonItems[0] == ITEM_SLINGSHOT) ||
            (gSaveContext.equips.buttonItems[0] == ITEM_BOW) || (gSaveContext.equips.buttonItems[0] == ITEM_BOMBCHU) ||
            (gSaveContext.equips.buttonItems[0] == ITEM_FISHING_POLE) ||
            (gSaveContext.buttonStatus[0] == BTN_DISABLED)) {
            if ((gSaveContext.equips.buttonItems[0] == ITEM_SLINGSHOT) ||
                (gSaveContext.equips.buttonItems[0] == ITEM_BOW) ||
                (gSaveContext.equips.buttonItems[0] == ITEM_BOMBCHU) ||
                (gSaveContext.equips.buttonItems[0] == ITEM_FISHING_POLE)) {
                gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];
                Interface_RandoRestoreSwordless();
                Interface_LoadItemIcon1(play, 0);
            }
        } else if (gSaveContext.equips.buttonItems[0] == ITEM_NONE) {
            if ((gSaveContext.equips.buttonItems[0] != ITEM_NONE) || (gSaveContext.infTable[29] == 0)) {
                gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];
                Interface_RandoRestoreSwordless();
                Interface_LoadItemIcon1(play, 0);
            }
        }

        gSaveContext.buttonStatus[0] = gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] =
            gSaveContext.buttonStatus[3] = BTN_ENABLED;
        gSaveContext.buttonStatus[5] = gSaveContext.buttonStatus[6] = gSaveContext.buttonStatus[7] =
            gSaveContext.buttonStatus[8] = BTN_ENABLED;
        Interface_ChangeAlpha(7);
    } else {
        gSaveContext.buttonStatus[0] = gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] =
            gSaveContext.buttonStatus[3] = BTN_ENABLED;
        gSaveContext.buttonStatus[5] = gSaveContext.buttonStatus[6] = gSaveContext.buttonStatus[7] =
            gSaveContext.buttonStatus[8] = BTN_ENABLED;
        func_80083108(play);
    }
}

// Gameplay stat tracking: Update time the item was acquired
// (special cases for some duplicate items)
void GameplayStats_SetTimestamp(PlayState* play, u8 item) {

    // If we already have a timestamp for this item, do nothing
    if (gSaveContext.ship.stats.itemTimestamp[item] != 0) {
        return;
    }
    // Use ITEM_KEY_BOSS only for Ganon's boss key - not any other boss keys
    if (play != NULL) {
        if (item == ITEM_KEY_BOSS && play->sceneNum != SCENE_INSIDE_GANONS_CASTLE &&
            play->sceneNum != SCENE_GANONS_TOWER) {
            return;
        }
    }

    u32 time = GAMEPLAYSTAT_TOTAL_TIME;

    // Have items in Link's pocket shown as being obtained at 0.1 seconds
    if (time == 0) {
        time = 1;
    }

    // Count any bottled item as a bottle
    if (item >= ITEM_BOTTLE && item <= ITEM_POE) {
        if (gSaveContext.ship.stats.itemTimestamp[ITEM_BOTTLE] == 0) {
            gSaveContext.ship.stats.itemTimestamp[ITEM_BOTTLE] = time;
        }
        return;
    }
    // Count any bombchu pack as bombchus
    if (item == ITEM_BOMBCHU || (item >= ITEM_BOMBCHUS_5 && item <= ITEM_BOMBCHUS_20)) {
        if (gSaveContext.ship.stats.itemTimestamp[ITEM_BOMBCHU] == 0) {
            gSaveContext.ship.stats.itemTimestamp[ITEM_BOMBCHU] = time;
        }
        return;
    }

    gSaveContext.ship.stats.itemTimestamp[item] = time;
    GameInteractor_ExecuteOnTimestamp(item);
}

u8 Return_Item_Entry(GetItemEntry itemEntry, u8 returnItem) {
    GameInteractor_ExecuteOnItemReceiveHooks(itemEntry);
    return returnItem;
}

// Processes Item_Give returns
u8 Return_Item(u8 itemID, ModIndex modId, ItemID returnItem) {
    // ITEM_SOLD_OUT doesn't have an ItemTable entry, so pass custom entry instead
    if (itemID == ITEM_SOLD_OUT) {
        GetItemEntry gie = { ITEM_SOLD_OUT, 0, 0, 0, 0, 0, 0, 0, 0, false, ITEM_FROM_NPC, ITEM_CATEGORY_LESSER, NULL };
        return Return_Item_Entry(gie, returnItem);
    }

    GetItemID getItemID = RetrieveGetItemIDFromItemID(itemID);
    if (getItemID != GI_MAX) {
        // Vanilla ItemID with an associated GetItemID
        return Return_Item_Entry(ItemTable_RetrieveEntry(modId, getItemID), returnItem);
    }

    RandomizerGet randomizerGet = RetrieveRandomizerGetFromItemID(itemID);
    if (randomizerGet != RG_MAX) {
        // Vanilla ItemID with an associated RandomizerGet (These are items in extendedVanillaGetItemTable)
        return Return_Item_Entry(ItemTable_RetrieveEntry(MOD_RANDOMIZER, randomizerGet), returnItem);
    }

    // All randomizer items should go through Randomizer_Item_Give, so this should never be reached
    // but leaving this here just in case, as it was in the original behavior
    assert(false);
    return Return_Item_Entry(ItemTable_RetrieveEntry(MOD_RANDOMIZER, itemID), returnItem);
}

void Item_Give_SetPlayer(Player* player) {
    sItemGivePlayer = player;
}

void Item_Give_ClearPlayer(void) {
    sItemGivePlayer = NULL;
}

static Player* Item_Give_GetPlayer(PlayState* play) {
    if (sItemGivePlayer != NULL) {
        return sItemGivePlayer;
    }

    if (play != NULL) {
        return GET_PLAYER(play);
    }

    return NULL;
}

/**
 * @brief Adds the given item to Link's inventory.
 *
 * NOTE: This function has been edited to be safe to use with a NULL play.
 * If you need to add to this function, be sure you check if the play is not
 * NULL before doing any operations requiring it.
 *
 * @param play
 * @param item
 * @return u8
 */
u8 Item_Give(PlayState* play, u8 item) {
    // prevents getting sticks without the bag in case something got missed
    if (IS_RANDO && (item == ITEM_STICK || item == ITEM_STICKS_5 || item == ITEM_STICKS_10) &&
        Randomizer_GetSettingValue(RSK_SHUFFLE_DEKU_STICK_BAG) && CUR_UPG_VALUE(UPG_STICKS) == 0) {
        return item;
    }

    // prevents getting nuts without the bag in case something got missed
    if (IS_RANDO && (item == ITEM_NUT || item == ITEM_NUTS_5 || item == ITEM_NUTS_10) &&
        Randomizer_GetSettingValue(RSK_SHUFFLE_DEKU_NUT_BAG) && CUR_UPG_VALUE(UPG_NUTS) == 0) {
        return item;
    }

    lusprintf(__FILE__, __LINE__, 2, "Item Give - item: %#x", item);
    static s16 sAmmoRefillCounts[] = { 5, 10, 20, 30, 5, 10, 30, 0, 5, 20, 1, 5, 20, 50, 200, 10 };
    s16 i;
    s16 slot;
    s16 temp;

    GetItemID returnItem = ITEM_NONE;

    // Gameplay stats: Update the time the item was obtained
    GameplayStats_SetTimestamp(play, item);

    slot = SLOT(item);
    if (item >= ITEM_STICKS_5) {
        slot = SLOT(sExtraItemBases[item - ITEM_STICKS_5]);
    }

    osSyncPrintf(VT_FGCOL(YELLOW));
    osSyncPrintf("item_get_setting=%d  pt=%d  z=%x\n", item, slot, gSaveContext.inventory.items[slot]);
    osSyncPrintf(VT_RST);

    if ((item >= ITEM_MEDALLION_FOREST) && (item <= ITEM_MEDALLION_LIGHT)) {
        gSaveContext.inventory.questItems |= gBitFlags[item - ITEM_MEDALLION_FOREST + QUEST_MEDALLION_FOREST];

        osSyncPrintf(VT_FGCOL(YELLOW));
        osSyncPrintf("封印 = %x\n", gSaveContext.inventory.questItems); // "Seals = %x"
        osSyncPrintf(VT_RST);

        if (item == ITEM_MEDALLION_WATER) {
            func_8006D0AC(play);
        }

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_SONG_MINUET) && (item <= ITEM_SONG_STORMS)) {
        gSaveContext.inventory.questItems |= gBitFlags[item - ITEM_SONG_MINUET + QUEST_SONG_MINUET];

        osSyncPrintf(VT_FGCOL(YELLOW));
        osSyncPrintf("楽譜 = %x\n", gSaveContext.inventory.questItems); // "Musical scores = %x"
        // "Musical scores = %x (%x) (%x)"
        osSyncPrintf("楽譜 = %x (%x) (%x)\n", gSaveContext.inventory.questItems,
                     gBitFlags[item - ITEM_SONG_MINUET + QUEST_SONG_MINUET], gBitFlags[item - ITEM_SONG_MINUET]);
        osSyncPrintf(VT_RST);

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_KOKIRI_EMERALD) && (item <= ITEM_ZORA_SAPPHIRE)) {
        gSaveContext.inventory.questItems |= gBitFlags[item - ITEM_KOKIRI_EMERALD + QUEST_KOKIRI_EMERALD];

        osSyncPrintf(VT_FGCOL(YELLOW));
        osSyncPrintf("精霊石 = %x\n", gSaveContext.inventory.questItems); // "Spiritual Stones = %x"
        osSyncPrintf(VT_RST);

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item == ITEM_STONE_OF_AGONY) || (item == ITEM_GERUDO_CARD)) {
        gSaveContext.inventory.questItems |= gBitFlags[item - ITEM_STONE_OF_AGONY + QUEST_STONE_OF_AGONY];

        osSyncPrintf(VT_FGCOL(YELLOW));
        osSyncPrintf("アイテム = %x\n", gSaveContext.inventory.questItems); // "Items = %x"
        osSyncPrintf(VT_RST);

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_SKULL_TOKEN) {
        gSaveContext.inventory.questItems |= gBitFlags[item - ITEM_SKULL_TOKEN + QUEST_SKULL_TOKEN];
        gSaveContext.inventory.gsTokens++;

        osSyncPrintf(VT_FGCOL(YELLOW));
        // "N Coins = %x(%d)"
        osSyncPrintf("Ｎコイン = %x(%d)\n", gSaveContext.inventory.questItems, gSaveContext.inventory.gsTokens);
        osSyncPrintf(VT_RST);

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_BGS)) {
        gSaveContext.inventory.equipment |=
            OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, item - ITEM_SWORD_KOKIRI + EQUIP_INV_SWORD_KOKIRI);

        // Both Giant's Knife and Biggoron Sword have the same Item ID, so this part handles both of them
        if (item == ITEM_SWORD_BGS) {
            gSaveContext.swordHealth = 8;

            if (ALL_EQUIP_VALUE(EQUIP_TYPE_SWORD) ==
                ((1 << EQUIP_INV_SWORD_KOKIRI) | (1 << EQUIP_INV_SWORD_MASTER) | (1 << EQUIP_INV_SWORD_BIGGORON) |
                 (1 << EQUIP_INV_SWORD_BROKENGIANTKNIFE))) {

                gSaveContext.inventory.equipment ^=
                    OWNED_EQUIP_FLAG_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);

                if (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KNIFE) {
                    gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;
                    if (play != NULL) {
                        Interface_LoadItemIcon1(play, 0);
                    }
                }
            }

        } else if (item == ITEM_SWORD_MASTER) {
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_MASTER;
            gSaveContext.equips.equipment &= (u16) ~(0xF << (EQUIP_TYPE_SWORD * 4));
            gSaveContext.equips.equipment |= EQUIP_VALUE_SWORD_MASTER << (EQUIP_TYPE_SWORD * 4);
            if (play != NULL) {
                Interface_LoadItemIcon1(play, 0);
            }
        }

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_SHIELD_DEKU) && (item <= ITEM_SHIELD_MIRROR)) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, item - ITEM_SHIELD_DEKU);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_TUNIC_KOKIRI) && (item <= ITEM_TUNIC_ZORA)) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_TUNIC, item - ITEM_TUNIC_KOKIRI);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_BOOTS_KOKIRI) && (item <= ITEM_BOOTS_HOVER)) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_BOOTS, item - ITEM_BOOTS_KOKIRI);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item == ITEM_KEY_BOSS) || (item == ITEM_COMPASS) || (item == ITEM_DUNGEON_MAP)) {
        gSaveContext.inventory.dungeonItems[gSaveContext.mapIndex] |= gBitFlags[item - ITEM_KEY_BOSS];
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_KEY_SMALL) {
        if (gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex] < 0) {
            gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex] = 1;
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        } else {
            gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex]++;
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        }
    } else if ((item == ITEM_QUIVER_30) || (item == ITEM_BOW)) {
        if (CUR_UPG_VALUE(UPG_QUIVER) == 0) {
            Inventory_ChangeUpgrade(UPG_QUIVER, 1);
            INV_CONTENT(ITEM_BOW) = ITEM_BOW;
            AMMO(ITEM_BOW) = CAPACITY(UPG_QUIVER, 1);
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        } else {
            AMMO(ITEM_BOW)++;
            if (AMMO(ITEM_BOW) > CUR_CAPACITY(UPG_QUIVER)) {
                AMMO(ITEM_BOW) = CUR_CAPACITY(UPG_QUIVER);
            }
        }
    } else if (item == ITEM_QUIVER_40) {
        Inventory_ChangeUpgrade(UPG_QUIVER, 2);
        AMMO(ITEM_BOW) = CAPACITY(UPG_QUIVER, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_QUIVER_50) {
        Inventory_ChangeUpgrade(UPG_QUIVER, 3);
        AMMO(ITEM_BOW) = CAPACITY(UPG_QUIVER, 3);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BULLET_BAG_40) {
        Inventory_ChangeUpgrade(UPG_BULLET_BAG, 2);
        AMMO(ITEM_SLINGSHOT) = CAPACITY(UPG_BULLET_BAG, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BULLET_BAG_50) {
        Inventory_ChangeUpgrade(UPG_BULLET_BAG, 3);
        AMMO(ITEM_SLINGSHOT) = CAPACITY(UPG_BULLET_BAG, 3);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BOMB_BAG_20) {
        if (CUR_UPG_VALUE(UPG_BOMB_BAG) == 0) {
            Inventory_ChangeUpgrade(UPG_BOMB_BAG, 1);
            INV_CONTENT(ITEM_BOMB) = ITEM_BOMB;
            AMMO(ITEM_BOMB) = CAPACITY(UPG_BOMB_BAG, 1);
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        } else {
            AMMO(ITEM_BOMB)++;
            if (AMMO(ITEM_BOMB) > CUR_CAPACITY(UPG_BOMB_BAG)) {
                AMMO(ITEM_BOMB) = CUR_CAPACITY(UPG_BOMB_BAG);
            }
        }
    } else if (item == ITEM_BOMB_BAG_30) {
        Inventory_ChangeUpgrade(UPG_BOMB_BAG, 2);
        AMMO(ITEM_BOMB) = CAPACITY(UPG_BOMB_BAG, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BOMB_BAG_40) {
        Inventory_ChangeUpgrade(UPG_BOMB_BAG, 3);
        AMMO(ITEM_BOMB) = CAPACITY(UPG_BOMB_BAG, 3);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BRACELET) {
        Inventory_ChangeUpgrade(UPG_STRENGTH, 1);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_GAUNTLETS_SILVER) {
        Inventory_ChangeUpgrade(UPG_STRENGTH, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_GAUNTLETS_GOLD) {
        Inventory_ChangeUpgrade(UPG_STRENGTH, 3);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_SCALE_SILVER) {
        Inventory_ChangeUpgrade(UPG_SCALE, 1);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_SCALE_GOLDEN) {
        Inventory_ChangeUpgrade(UPG_SCALE, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_WALLET_ADULT) {
        Inventory_ChangeUpgrade(UPG_WALLET, 1);
        if (IS_RANDO && Randomizer_GetSettingValue(RSK_FULL_WALLETS)) {
            Rupees_ChangeBy(200);
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_WALLET_GIANT) {
        Inventory_ChangeUpgrade(UPG_WALLET, 2);
        if (IS_RANDO && Randomizer_GetSettingValue(RSK_FULL_WALLETS)) {
            Rupees_ChangeBy(500);
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_STICK_UPGRADE_20) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(ITEM_STICK) = ITEM_STICK;
        }
        Inventory_ChangeUpgrade(UPG_STICKS, 2);
        AMMO(ITEM_STICK) = CAPACITY(UPG_STICKS, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_STICK_UPGRADE_30) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(ITEM_STICK) = ITEM_STICK;
        }
        Inventory_ChangeUpgrade(UPG_STICKS, 3);
        AMMO(ITEM_STICK) = CAPACITY(UPG_STICKS, 3);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_NUT_UPGRADE_30) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(ITEM_NUT) = ITEM_NUT;
        }
        Inventory_ChangeUpgrade(UPG_NUTS, 2);
        AMMO(ITEM_NUT) = CAPACITY(UPG_NUTS, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_NUT_UPGRADE_40) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(ITEM_NUT) = ITEM_NUT;
        }
        Inventory_ChangeUpgrade(UPG_NUTS, 3);
        AMMO(ITEM_NUT) = CAPACITY(UPG_NUTS, 3);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_LONGSHOT) {
        INV_CONTENT(item) = item;
        // always update "equips" as this is what is currently on the c-buttons
        for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            if (gSaveContext.equips.buttonItems[i] == ITEM_HOOKSHOT) {
                gSaveContext.equips.buttonItems[i] = ITEM_LONGSHOT;
                if (play != NULL) {
                    Interface_LoadItemIcon1(play, i);
                }
            }
        }
        // update the adult/child equips when rando'd (accounting for equip swapped hookshot as child)
        if (IS_RANDO && LINK_IS_CHILD) {
            for (i = 1; i < ARRAY_COUNT(gSaveContext.adultEquips.buttonItems); i++) {
                if (gSaveContext.adultEquips.buttonItems[i] == ITEM_HOOKSHOT) {
                    gSaveContext.adultEquips.buttonItems[i] = ITEM_LONGSHOT;
                    if (play != NULL) {
                        Interface_LoadItemIcon1(play, i);
                    }
                }
            }
        }
        if (IS_RANDO && LINK_IS_ADULT) {
            for (i = 1; i < ARRAY_COUNT(gSaveContext.childEquips.buttonItems); i++) {
                if (gSaveContext.childEquips.buttonItems[i] == ITEM_HOOKSHOT) {
                    gSaveContext.childEquips.buttonItems[i] = ITEM_LONGSHOT;
                    if (play != NULL) {
                        Interface_LoadItemIcon1(play, i);
                    }
                }
            }
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_STICK) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            Inventory_ChangeUpgrade(UPG_STICKS, 1);
            AMMO(ITEM_STICK) = 1;
        } else {
            AMMO(ITEM_STICK)++;
            if (AMMO(ITEM_STICK) > CUR_CAPACITY(UPG_STICKS)) {
                AMMO(ITEM_STICK) = CUR_CAPACITY(UPG_STICKS);
            }
        }
    } else if ((item == ITEM_STICKS_5) || (item == ITEM_STICKS_10)) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            Inventory_ChangeUpgrade(UPG_STICKS, 1);
            AMMO(ITEM_STICK) = sAmmoRefillCounts[item - ITEM_STICKS_5];
        } else {
            AMMO(ITEM_STICK) += sAmmoRefillCounts[item - ITEM_STICKS_5];
            if (AMMO(ITEM_STICK) > CUR_CAPACITY(UPG_STICKS)) {
                AMMO(ITEM_STICK) = CUR_CAPACITY(UPG_STICKS);
            }
        }
        // [SOH] This results in the same behavior as the original code, but also allows us to get an accurate
        // ReceivedItemEntry hook
        INV_CONTENT(ITEM_STICK) = ITEM_STICK;
        return Return_Item(item, MOD_NONE, returnItem);
    } else if (item == ITEM_NUT) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            Inventory_ChangeUpgrade(UPG_NUTS, 1);
            AMMO(ITEM_NUT) = ITEM_NUT;
        } else {
            AMMO(ITEM_NUT)++;
            if (AMMO(ITEM_NUT) > CUR_CAPACITY(UPG_NUTS)) {
                AMMO(ITEM_NUT) = CUR_CAPACITY(UPG_NUTS);
            }
        }
    } else if ((item == ITEM_NUTS_5) || (item == ITEM_NUTS_10)) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            Inventory_ChangeUpgrade(UPG_NUTS, 1);
            AMMO(ITEM_NUT) += sAmmoRefillCounts[item - ITEM_NUTS_5];
            // "Deku Nuts %d(%d)=%d BS_count=%d"
            osSyncPrintf("デクの実 %d(%d)=%d  BS_count=%d\n", item, ITEM_NUTS_5, item - ITEM_NUTS_5,
                         sAmmoRefillCounts[item - ITEM_NUTS_5]);
        } else {
            AMMO(ITEM_NUT) += sAmmoRefillCounts[item - ITEM_NUTS_5];
            if (AMMO(ITEM_NUT) > CUR_CAPACITY(UPG_NUTS)) {
                AMMO(ITEM_NUT) = CUR_CAPACITY(UPG_NUTS);
            }
        }
        // [SOH] This results in the same behavior as the original code, but also allows us to get an accurate
        // ReceivedItemEntry hook
        INV_CONTENT(ITEM_NUT) = ITEM_NUT;
        return Return_Item(item, MOD_NONE, returnItem);
    } else if (item == ITEM_BOMB) {
        // "Bomb  Bomb  Bomb  Bomb Bomb   Bomb Bomb"
        osSyncPrintf(" 爆弾  爆弾  爆弾  爆弾 爆弾   爆弾 爆弾 \n");
        if ((AMMO(ITEM_BOMB) += 1) > CUR_CAPACITY(UPG_BOMB_BAG)) {
            AMMO(ITEM_BOMB) = CUR_CAPACITY(UPG_BOMB_BAG);
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_BOMBS_5) && (item <= ITEM_BOMBS_30)) {
        if ((AMMO(ITEM_BOMB) += sAmmoRefillCounts[item - ITEM_BOMBS_5]) > CUR_CAPACITY(UPG_BOMB_BAG)) {
            AMMO(ITEM_BOMB) = CUR_CAPACITY(UPG_BOMB_BAG);
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BOMBCHU) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
            AMMO(ITEM_BOMBCHU) = 10;
        } else {
            AMMO(ITEM_BOMBCHU) += 10;
            if (GameInteractor_Should(VB_CHECK_BOMBCHU_CAPACITY, true)) {
                if (AMMO(ITEM_BOMBCHU) > 50) {
                    AMMO(ITEM_BOMBCHU) = 50;
                }
            }
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item == ITEM_BOMBCHUS_5) || (item == ITEM_BOMBCHUS_20)) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
            AMMO(ITEM_BOMBCHU) += sAmmoRefillCounts[item - ITEM_BOMBCHUS_5 + 8];
        } else {
            AMMO(ITEM_BOMBCHU) += sAmmoRefillCounts[item - ITEM_BOMBCHUS_5 + 8];
            if (GameInteractor_Should(VB_CHECK_BOMBCHU_CAPACITY, true)) {
                if (AMMO(ITEM_BOMBCHU) > 50) {
                    AMMO(ITEM_BOMBCHU) = 50;
                }
            }
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_ARROWS_SMALL) && (item <= ITEM_ARROWS_LARGE)) {
        AMMO(ITEM_BOW) += sAmmoRefillCounts[item - ITEM_ARROWS_SMALL + 4];

        if ((AMMO(ITEM_BOW) >= CUR_CAPACITY(UPG_QUIVER)) || (AMMO(ITEM_BOW) < 0)) {
            AMMO(ITEM_BOW) = CUR_CAPACITY(UPG_QUIVER);
        }

        osSyncPrintf("%d本  Item_MaxGet=%d\n", AMMO(ITEM_BOW), CUR_CAPACITY(UPG_QUIVER));

        return Return_Item(item, MOD_NONE, ITEM_BOW);
    } else if (item == ITEM_SLINGSHOT) {
        Inventory_ChangeUpgrade(UPG_BULLET_BAG, 1);
        INV_CONTENT(ITEM_SLINGSHOT) = ITEM_SLINGSHOT;
        AMMO(ITEM_SLINGSHOT) = 30;
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_SEEDS) {
        AMMO(ITEM_SLINGSHOT) += 5;

        if (AMMO(ITEM_SLINGSHOT) >= CUR_CAPACITY(UPG_BULLET_BAG)) {
            AMMO(ITEM_SLINGSHOT) = CUR_CAPACITY(UPG_BULLET_BAG);
        }

        if (!Flags_GetItemGetInf(ITEMGETINF_13)) {
            Flags_SetItemGetInf(ITEMGETINF_13);
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        }

        return Return_Item(item, MOD_NONE, ITEM_SEEDS);
    } else if (item == ITEM_SEEDS_30) {
        AMMO(ITEM_SLINGSHOT) += 30;

        if (AMMO(ITEM_SLINGSHOT) >= CUR_CAPACITY(UPG_BULLET_BAG)) {
            AMMO(ITEM_SLINGSHOT) = CUR_CAPACITY(UPG_BULLET_BAG);
        }

        if (!Flags_GetItemGetInf(ITEMGETINF_13)) {
            Flags_SetItemGetInf(ITEMGETINF_13);
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        }

        return Return_Item(item, MOD_NONE, ITEM_SEEDS);
    } else if (item == ITEM_OCARINA_FAIRY) {
        INV_CONTENT(ITEM_OCARINA_FAIRY) = ITEM_OCARINA_FAIRY;
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_OCARINA_TIME) {
        INV_CONTENT(ITEM_OCARINA_TIME) = ITEM_OCARINA_TIME;
        // always update "equips" as this is what is currently on the c-buttons
        for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            if (gSaveContext.equips.buttonItems[i] == ITEM_OCARINA_FAIRY) {
                gSaveContext.equips.buttonItems[i] = ITEM_OCARINA_TIME;
                Interface_LoadItemIcon1(play, i);
            }
        }

        // update the adult/child equips when rando'd
        if (IS_RANDO && LINK_IS_CHILD) {
            for (i = 1; i < ARRAY_COUNT(gSaveContext.adultEquips.buttonItems); i++) {
                if (gSaveContext.adultEquips.buttonItems[i] == ITEM_OCARINA_FAIRY) {
                    gSaveContext.adultEquips.buttonItems[i] = ITEM_OCARINA_TIME;
                    if (play != NULL) {
                        Interface_LoadItemIcon1(play, i);
                    }
                }
            }
        }
        if (IS_RANDO && LINK_IS_ADULT) {
            for (i = 1; i < ARRAY_COUNT(gSaveContext.childEquips.buttonItems); i++) {
                if (gSaveContext.childEquips.buttonItems[i] == ITEM_OCARINA_FAIRY) {
                    gSaveContext.childEquips.buttonItems[i] = ITEM_OCARINA_TIME;
                    if (play != NULL) {
                        Interface_LoadItemIcon1(play, i);
                    }
                }
            }
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BEAN) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(item) = item;
            AMMO(ITEM_BEAN) = 1;
            BEANS_BOUGHT = 1;
        } else {
            AMMO(ITEM_BEAN)++;
            BEANS_BOUGHT++;
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item == ITEM_HEART_PIECE_2) || (item == ITEM_HEART_PIECE)) {
        gSaveContext.inventory.questItems += 1 << (QUEST_HEART_PIECE + 4);
        gSaveContext.ship.stats.heartPieces++;
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_HEART_CONTAINER) {
        if (GameInteractor_Should(VB_HEARTS_INCREASE_WITH_CONTAINERS, true)) {
            gSaveContext.healthCapacity += FULL_HEART_HEALTH;
            gSaveContext.health += FULL_HEART_HEALTH;
        }
        gSaveContext.ship.stats.heartContainers++;
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_HEART) {
        osSyncPrintf("回復ハート回復ハート回復ハート\n"); // "Recovery Heart"
        if (play != NULL) {
            Health_ChangeBy(play, FULL_HEART_HEALTH, Item_Give_GetPlayer(play));
        }
        return Return_Item(item, MOD_NONE, item);
    } else if (item == ITEM_MAGIC_SMALL) {
        Player* itemPlayer = Item_Give_GetPlayer(play);
        if (Magic_GetStateForPlayer(itemPlayer) != MAGIC_STATE_ADD) {
            if (play != NULL) {
                Magic_FillForPlayer(play, itemPlayer);
            }
        }

        if (play != NULL) {
            Magic_RequestChangeForPlayer(play, itemPlayer, 12, MAGIC_ADD);
        }

        if (!Flags_GetInfTable(INFTABLE_198)) {
            Flags_SetInfTable(INFTABLE_198);
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        }

        return Return_Item(item, MOD_NONE, item);
    } else if (item == ITEM_MAGIC_LARGE) {
        Player* itemPlayer = Item_Give_GetPlayer(play);
        if (Magic_GetStateForPlayer(itemPlayer) != MAGIC_STATE_ADD) {
            if (play != NULL) {
                Magic_FillForPlayer(play, itemPlayer);
            }
        }
        if (play != NULL) {
            Magic_RequestChangeForPlayer(play, itemPlayer, 24, MAGIC_ADD);
        }

        if (!Flags_GetInfTable(INFTABLE_198)) {
            Flags_SetInfTable(INFTABLE_198);
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        }

        return Return_Item(item, MOD_NONE, item);
    } else if ((item >= ITEM_RUPEE_GREEN) && (item <= ITEM_INVALID_8)) {
        Rupees_ChangeBy(sAmmoRefillCounts[item - ITEM_RUPEE_GREEN + 10]);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BOTTLE) {
        temp = SLOT(item);

        for (i = 0; i < 4; i++) {
            if (gSaveContext.inventory.items[temp + i] == ITEM_NONE) {
                gSaveContext.inventory.items[temp + i] = item;
                return Return_Item(item, MOD_NONE, ITEM_NONE);
            }
        }
    } else if (((item >= ITEM_POTION_RED) && (item <= ITEM_POE)) || (item == ITEM_MILK)) {
        temp = SLOT(item);

        if ((item != ITEM_MILK_BOTTLE) && (item != ITEM_LETTER_RUTO)) {
            if (item == ITEM_MILK) {
                item = ITEM_MILK_BOTTLE;
                temp = SLOT(item);
            }

            for (i = 0; i < 4; i++) {
                if (gSaveContext.inventory.items[temp + i] == ITEM_BOTTLE) {
                    // "Item_Pt(1)=%d Item_Pt(2)=%d Item_Pt(3)=%d   Empty Bottle=%d   Content=%d"
                    osSyncPrintf("Item_Pt(1)=%d Item_Pt(2)=%d Item_Pt(3)=%d   空瓶=%d   中味=%d\n",
                                 gSaveContext.equips.cButtonSlots[0], gSaveContext.equips.cButtonSlots[1],
                                 gSaveContext.equips.cButtonSlots[2], temp + i, item);

                    for (int buttonIndex = 1; buttonIndex < ARRAY_COUNT(gSaveContext.equips.buttonItems);
                         buttonIndex++) {
                        if ((temp + i) == gSaveContext.equips.cButtonSlots[buttonIndex - 1]) {
                            gSaveContext.equips.buttonItems[buttonIndex] = item;
                            if (play != NULL) {
                                Interface_LoadItemIcon2(play, buttonIndex);
                            }
                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(buttonIndex)] = BTN_ENABLED;
                            break;
                        }
                    }

                    gSaveContext.inventory.items[temp + i] = item;
                    break;
                }
            }
        } else {
            if (item == ITEM_LETTER_RUTO) {
                Flags_SetRandomizerInf(RAND_INF_OBTAINED_RUTOS_LETTER);
            }
            for (i = 0; i < 4; i++) {
                if (gSaveContext.inventory.items[temp + i] == ITEM_NONE) {
                    gSaveContext.inventory.items[temp + i] = item;
                    break;
                }
            }
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_WEIRD_EGG) && (item <= ITEM_CLAIM_CHECK)) {
        if (GameInteractor_Should(VB_POACHERS_SAW_SET_DEKU_NUT_UPGRADE_FLAG, item == ITEM_SAW)) {
            Flags_SetItemGetInf(ITEMGETINF_OBTAINED_NUT_UPGRADE_FROM_STAGE);
        }

        if (IS_RANDO) {
            if (item >= ITEM_POCKET_EGG) {
                Flags_SetRandomizerInf(item - ITEM_POCKET_EGG + RAND_INF_ADULT_TRADES_HAS_POCKET_EGG);
            } else if (item == ITEM_LETTER_ZELDA) {
                // don't care about zelda's letter if it's already been shown to the guard
                if (!Flags_GetInfTable(INFTABLE_SHOWED_ZELDAS_LETTER_TO_GATE_GUARD)) {
                    Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_LETTER_ZELDA);
                }
            } else {
                Flags_SetRandomizerInf(item - ITEM_WEIRD_EGG + RAND_INF_CHILD_TRADES_HAS_WEIRD_EGG);
                if (item == ITEM_WEIRD_EGG) {
                    Flags_SetRandomizerInf(RAND_INF_WEIRD_EGG);
                }
            }
        }

        temp = INV_CONTENT(item);
        INV_CONTENT(item) = item;

        if (temp != ITEM_NONE) {
            for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                if (temp == gSaveContext.equips.buttonItems[i]) {
                    if (item != ITEM_SOLD_OUT) {
                        gSaveContext.equips.buttonItems[i] = item;
                        if (play != NULL) {
                            Interface_LoadItemIcon1(play, i);
                        }
                    } else {
                        gSaveContext.equips.buttonItems[i] = ITEM_NONE;
                    }
                    return Return_Item(item, MOD_NONE, ITEM_NONE);
                }
            }
        }

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_NAYRUS_LOVE && Randomizer_GetSettingValue(RSK_ROCS_FEATHER)) {
        Flags_SetRandomizerInf(RAND_INF_OBTAINED_NAYRUS_LOVE);
        if (INV_CONTENT(ITEM_NAYRUS_LOVE) == ITEM_NONE) {
            INV_CONTENT(ITEM_NAYRUS_LOVE) = ITEM_NAYRUS_LOVE;
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    }
    returnItem = gSaveContext.inventory.items[slot];
    osSyncPrintf("Item_Register(%d)=%d  %d\n", slot, item, returnItem);
    INV_CONTENT(item) = item;
    return Return_Item(item, MOD_NONE, returnItem);
}

u8 Item_CheckObtainability(u8 item) {
    s16 i;
    s16 slot = SLOT(item);
    s32 temp;

    if (item >= ITEM_STICKS_5) {
        slot = SLOT(sExtraItemBases[item - ITEM_STICKS_5]);
    }

    osSyncPrintf(VT_FGCOL(GREEN));
    osSyncPrintf("item_get_non_setting=%d  pt=%d  z=%x\n", item, slot, gSaveContext.inventory.items[slot]);
    osSyncPrintf(VT_RST);

    if (IS_RANDO) {
        if (item == ITEM_SINGLE_MAGIC || item == ITEM_DOUBLE_MAGIC || item == ITEM_DOUBLE_DEFENSE) {
            return ITEM_NONE;
        }
    }

    if ((item >= ITEM_SONG_MINUET) && (item <= ITEM_SONG_STORMS)) {
        return ITEM_NONE;
    } else if ((item >= ITEM_MEDALLION_FOREST) && (item <= ITEM_MEDALLION_LIGHT)) {
        return ITEM_NONE;
    } else if ((item >= ITEM_KOKIRI_EMERALD) && (item <= ITEM_SKULL_TOKEN)) {
        return ITEM_NONE;
    } else if ((item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_BGS)) {
        if (item == ITEM_SWORD_BGS) {
            return ITEM_NONE;
        } else if (CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, item - ITEM_SWORD_KOKIRI + EQUIP_INV_SWORD_KOKIRI)) {
            return IS_RANDO ? ITEM_NONE : item;
        } else {
            return ITEM_NONE;
        }
    } else if ((item >= ITEM_SHIELD_DEKU) && (item <= ITEM_SHIELD_MIRROR)) {
        if (CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, item - ITEM_SHIELD_DEKU + EQUIP_INV_SHIELD_DEKU)) {
            return IS_RANDO ? ITEM_NONE : item;
        } else {
            return ITEM_NONE;
        }
    } else if ((item >= ITEM_TUNIC_KOKIRI) && (item <= ITEM_TUNIC_ZORA)) {
        if (CHECK_OWNED_EQUIP(EQUIP_TYPE_TUNIC, item - ITEM_TUNIC_KOKIRI + EQUIP_INV_TUNIC_KOKIRI)) {
            return IS_RANDO ? ITEM_NONE : item;
        } else {
            return ITEM_NONE;
        }
    } else if ((item >= ITEM_BOOTS_KOKIRI) && (item <= ITEM_BOOTS_HOVER)) {
        if (CHECK_OWNED_EQUIP(EQUIP_TYPE_BOOTS, item - ITEM_BOOTS_KOKIRI + EQUIP_INV_BOOTS_KOKIRI)) {
            return IS_RANDO ? ITEM_NONE : item;
        } else {
            return ITEM_NONE;
        }
    } else if ((item == ITEM_KEY_BOSS) || (item == ITEM_COMPASS) || (item == ITEM_DUNGEON_MAP)) {
        return ITEM_NONE;
    } else if (item == ITEM_KEY_SMALL) {
        return ITEM_NONE;
    } else if ((item >= ITEM_SLINGSHOT) && (item <= ITEM_BOMBCHU)) {
        return ITEM_NONE;
    } else if ((item == ITEM_BOMBCHUS_5) || (item == ITEM_BOMBCHUS_20)) {
        return ITEM_NONE;
    } else if ((item == ITEM_QUIVER_30) || (item == ITEM_BOW)) {
        if (CUR_UPG_VALUE(UPG_QUIVER) == 0) {
            return ITEM_NONE;
        } else {
            return 0;
        }
    } else if ((item == ITEM_QUIVER_40) || (item == ITEM_QUIVER_50)) {
        return ITEM_NONE;
    } else if ((item == ITEM_BULLET_BAG_40) || (item == ITEM_BULLET_BAG_50)) {
        return ITEM_NONE;
    } else if ((item == ITEM_BOMB_BAG_20) || (item == ITEM_BOMB)) {
        if (CUR_UPG_VALUE(UPG_BOMB_BAG) == 0) {
            return ITEM_NONE;
        } else {
            return 0;
        }
    } else if ((item >= ITEM_STICK_UPGRADE_20) && (item <= ITEM_NUT_UPGRADE_40)) {
        return ITEM_NONE;
    } else if ((item >= ITEM_BOMB_BAG_30) && (item <= ITEM_WALLET_GIANT)) {
        return ITEM_NONE;
    } else if (item == ITEM_LONGSHOT) {
        return ITEM_NONE;
    } else if ((item == ITEM_SEEDS) || (item == ITEM_SEEDS_30)) {
        if (!Flags_GetItemGetInf(ITEMGETINF_13)) {
            return ITEM_NONE;
        } else {
            return ITEM_SEEDS;
        }
    } else if (item == ITEM_BEAN) {
        return ITEM_NONE;
    } else if ((item == ITEM_HEART_PIECE_2) || (item == ITEM_HEART_PIECE)) {
        return ITEM_NONE;
    } else if (item == ITEM_HEART_CONTAINER) {
        return ITEM_NONE;
    } else if (item == ITEM_HEART) {
        return ITEM_HEART;
    } else if ((item == ITEM_MAGIC_SMALL) || (item == ITEM_MAGIC_LARGE)) {
        // "Magic Pot Get_Inf_Table( 25, 0x0100)=%d"
        osSyncPrintf("魔法の壷 Get_Inf_Table( 25, 0x0100)=%d\n", Flags_GetInfTable(INFTABLE_198));
        if (!Flags_GetInfTable(INFTABLE_198)) {
            return ITEM_NONE;
        } else {
            return item;
        }
    } else if ((item >= ITEM_RUPEE_GREEN) && (item <= ITEM_INVALID_8)) {
        return ITEM_NONE;
    } else if (item == ITEM_BOTTLE) {
        return ITEM_NONE;
    } else if (((item >= ITEM_POTION_RED) && (item <= ITEM_POE)) || (item == ITEM_MILK)) {
        temp = SLOT(item);

        if ((item != ITEM_MILK_BOTTLE) && (item != ITEM_LETTER_RUTO)) {
            if (item == ITEM_MILK) {
                item = ITEM_MILK_BOTTLE;
                temp = SLOT(item);
            }

            for (i = 0; i < 4; i++) {
                if (gSaveContext.inventory.items[temp + i] == ITEM_BOTTLE) {
                    return ITEM_NONE;
                }
            }
        } else {
            for (i = 0; i < 4; i++) {
                if (gSaveContext.inventory.items[temp + i] == ITEM_NONE) {
                    return ITEM_NONE;
                }
            }
        }
    } else if ((item >= ITEM_WEIRD_EGG) && (item <= ITEM_CLAIM_CHECK)) {
        return ITEM_NONE;
    }

    return gSaveContext.inventory.items[slot];
}

void Inventory_DeleteItem(u16 item, u16 invSlot) {
    s16 i;

    if (item == ITEM_BEAN) {
        BEANS_BOUGHT = 0;
    }

    gSaveContext.inventory.items[invSlot] = ITEM_NONE;

    osSyncPrintf("\nItem_Register(%d)\n", invSlot, gSaveContext.inventory.items[invSlot]);

    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
        if (gSaveContext.equips.buttonItems[i] == item) {
            gSaveContext.equips.buttonItems[i] = ITEM_NONE;
            gSaveContext.equips.cButtonSlots[i - 1] = SLOT_NONE;
        }
    }
}

s32 Inventory_ReplaceItem(PlayState* play, u16 oldItem, u16 newItem) {
    s16 i;

    for (i = 0; i < ARRAY_COUNT(gSaveContext.inventory.items); i++) {
        if (gSaveContext.inventory.items[i] == oldItem) {
            gSaveContext.inventory.items[i] = newItem;
            osSyncPrintf("アイテム消去(%d)\n", i); // "Item Purge (%d)"
            for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                if (gSaveContext.equips.buttonItems[i] == oldItem) {
                    gSaveContext.equips.buttonItems[i] = newItem;
                    Interface_LoadItemIcon1(play, i);
                    break;
                }
            }
            return 1;
        }
    }

    return 0;
}

s32 Inventory_HasEmptyBottle(void) {
    u8* items = gSaveContext.inventory.items;

    if (items[SLOT_BOTTLE_1] == ITEM_BOTTLE) {
        return 1;
    } else if (items[SLOT_BOTTLE_2] == ITEM_BOTTLE) {
        return 1;
    } else if (items[SLOT_BOTTLE_3] == ITEM_BOTTLE) {
        return 1;
    } else if (items[SLOT_BOTTLE_4] == ITEM_BOTTLE) {
        return 1;
    } else {
        return 0;
    }
}

bool Inventory_HasEmptyBottleSlot(void) {
    u8* items = gSaveContext.inventory.items;

    return (items[SLOT_BOTTLE_1] == ITEM_NONE || items[SLOT_BOTTLE_2] == ITEM_NONE ||
            items[SLOT_BOTTLE_3] == ITEM_NONE || items[SLOT_BOTTLE_4] == ITEM_NONE);
}

s32 Inventory_HasSpecificBottle(u8 bottleItem) {
    u8* items = gSaveContext.inventory.items;

    if (items[SLOT_BOTTLE_1] == bottleItem) {
        return 1;
    } else if (items[SLOT_BOTTLE_2] == bottleItem) {
        return 1;
    } else if (items[SLOT_BOTTLE_3] == bottleItem) {
        return 1;
    } else if (items[SLOT_BOTTLE_4] == bottleItem) {
        return 1;
    } else {
        return 0;
    }
}

void Inventory_UpdateBottleItem(PlayState* play, u8 item, u8 button) {
    if (Interface_IsOriginalMultiplayerItemModeEnabled()) {
        s16 quickControllerPort = Interface_GetQuickItemControllerPortByButtonIndex(button);

        if (quickControllerPort != 0) {
            s16 quickSlot = Interface_GetQuickItemUseSlotForPort(quickControllerPort);
            s16 inventorySlot = Interface_GetQuickItemInventorySlotForPortAndSlot(quickControllerPort, quickSlot);

            if ((inventorySlot >= 0) && (inventorySlot < 24)) {
                // Special case to only empty half of a Lon Lon Milk Bottle.
                if ((gSaveContext.inventory.items[inventorySlot] == ITEM_MILK_BOTTLE) && (item == ITEM_BOTTLE)) {
                    item = ITEM_MILK_HALF;
                }

                if (GameInteractor_Should(VB_UPDATE_BOTTLE_ITEM, true, button, item)) {
                    gSaveContext.inventory.items[inventorySlot] = item;
                }
            }

            Interface_SetQuickItemForPortAndSlot(play, quickControllerPort, quickSlot, item, inventorySlot);

            if (item < ITEM_NONE_FE) {
                Interface_LoadItemIconForItem(play, button, item);
            }

            play->pauseCtx.cursorItem[PAUSE_ITEM] = item;
            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(button)] = BTN_ENABLED;
            return;
        }
    }

    osSyncPrintf("item_no=%x,  c_no=%x,  Pt=%x  Item_Register=%x\n", item, button,
                 gSaveContext.equips.cButtonSlots[button - 1],
                 gSaveContext.inventory.items[gSaveContext.equips.cButtonSlots[button - 1]]);

    // Special case to only empty half of a Lon Lon Milk Bottle
    if ((gSaveContext.inventory.items[gSaveContext.equips.cButtonSlots[button - 1]] == ITEM_MILK_BOTTLE) &&
        (item == ITEM_BOTTLE)) {
        item = ITEM_MILK_HALF;
    }

    if (GameInteractor_Should(VB_UPDATE_BOTTLE_ITEM, true, button, item)) {
        gSaveContext.inventory.items[gSaveContext.equips.cButtonSlots[button - 1]] = item;
    }

    gSaveContext.equips.buttonItems[button] = item;

    Interface_LoadItemIcon1(play, button);

    play->pauseCtx.cursorItem[PAUSE_ITEM] = item;
    gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(button)] = BTN_ENABLED;
}

s32 Inventory_ConsumeFairy(PlayState* play) {
    s32 bottleSlot = SLOT(ITEM_FAIRY);
    s16 i;
    s16 j;
    s16 quickControllerPort;
    s16 quickSlot;

    for (i = 0; i < 4; i++) {
        if (gSaveContext.inventory.items[bottleSlot + i] == ITEM_FAIRY) {
            if (Interface_IsOriginalMultiplayerItemModeEnabled()) {
                for (quickControllerPort = 1; quickControllerPort <= 4; quickControllerPort++) {
                    for (quickSlot = 0; quickSlot < 2; quickSlot++) {
                        if (Interface_GetQuickItemForPortAndSlot(quickControllerPort, quickSlot) == ITEM_FAIRY) {
                            s16 quickInventorySlot =
                                Interface_GetQuickItemInventorySlotForPortAndSlot(quickControllerPort, quickSlot);

                            Interface_SetQuickItemForPortAndSlot(play, quickControllerPort, quickSlot, ITEM_BOTTLE,
                                                                 quickInventorySlot);

                            if ((quickInventorySlot >= 0) && (quickInventorySlot < 24)) {
                                bottleSlot = quickInventorySlot;
                                i = 0;
                            }

                            goto Inventory_ConsumeFairy_ApplyBottle;
                        }
                    }
                }
            }

            for (j = 1; j < ARRAY_COUNT(gSaveContext.equips.buttonItems); j++) {
                u8 equippedItem = gSaveContext.equips.buttonItems[j];

                if (Interface_IsOriginalMultiplayerItemModeEnabled() && (j == 4)) {
                    equippedItem = sP4QuickItem;
                }

                if (equippedItem == ITEM_FAIRY) {
                    i = 0;

                    if (Interface_IsOriginalMultiplayerItemModeEnabled() && (j == 4)) {
                        sP4QuickItem = ITEM_BOTTLE;
                        Interface_LoadItemIconForItem(play, 4, ITEM_BOTTLE);
                        if ((sP4QuickItemSlot >= 0) && (sP4QuickItemSlot < 24)) {
                            bottleSlot = sP4QuickItemSlot;
                        }
                    } else {
                        gSaveContext.equips.buttonItems[j] = ITEM_BOTTLE;
                        Interface_LoadItemIcon1(play, j);
                        bottleSlot = gSaveContext.equips.cButtonSlots[j - 1];
                    }
                    break;
                }
            }

Inventory_ConsumeFairy_ApplyBottle:
            osSyncPrintf("妖精使用＝%d\n", bottleSlot); // "Fairy Usage＝%d"
            gSaveContext.inventory.items[bottleSlot + i] = ITEM_BOTTLE;
            return 1;
        }
    }

    return 0;
}

bool Inventory_HatchPocketCucco(PlayState* play) {
    if (!IS_RANDO) {
        return Inventory_ReplaceItem(play, ITEM_POCKET_EGG, ITEM_POCKET_CUCCO);
    }

    if (!Flags_GetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_POCKET_EGG)) {
        return 0;
    }

    Flags_UnsetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_POCKET_EGG);
    Flags_SetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_POCKET_CUCCO);
    Inventory_ReplaceItem(play, ITEM_POCKET_EGG, ITEM_POCKET_CUCCO);
    return 1;
}

void func_80086D5C(s32* buf, u16 size) {
    for (u16 i = 0; i < size; i++) {
        buf[i] = 0;
    }
}

void Interface_LoadActionLabel(InterfaceContext* interfaceCtx, u16 action, s16 loadOffset) {
    static void* sDoActionTextures[] = { gAttackDoActionENGTex, gCheckDoActionENGTex };
    if (action >= DO_ACTION_MAX) {
        action = DO_ACTION_NONE;
    }

    char* doAction = actionsTbl[action];

    static char newName[4][512];
    if (gSaveContext.language != LANGUAGE_ENG) {
        size_t length = strlen(doAction);
        strcpy(newName[loadOffset], doAction);
        if (gSaveContext.language == LANGUAGE_FRA) {
            newName[loadOffset][length - 6] = 'F';
            newName[loadOffset][length - 5] = 'R';
            newName[loadOffset][length - 4] = 'A';
        } else if (gSaveContext.language == LANGUAGE_GER) {
            newName[loadOffset][length - 6] = 'G';
            newName[loadOffset][length - 5] = 'E';
            newName[loadOffset][length - 4] = 'R';
        } else if (gSaveContext.language == LANGUAGE_JPN) {
            newName[loadOffset][length - 6] = 'J';
            newName[loadOffset][length - 5] = 'P';
            newName[loadOffset][length - 4] = 'N';
        }
        doAction = newName[loadOffset];
    }

    char* segment = interfaceCtx->doActionSegment[loadOffset];
    interfaceCtx->doActionSegment[loadOffset] = action != DO_ACTION_NONE ? doAction : gEmptyTexture;
    gSegments[7] = interfaceCtx->doActionSegment[loadOffset];
}

void Interface_SetDoAction(PlayState* play, u16 action) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    PauseContext* pauseCtx = &play->pauseCtx;

    if (interfaceCtx->unk_1F0 != action) {
        GameInteractor_ExecuteOnSetDoAction(action);
        interfaceCtx->unk_1F0 = action;
        interfaceCtx->unk_1EC = 1;
        interfaceCtx->unk_1F4 = 0.0f;
        Interface_LoadActionLabel(interfaceCtx, action, 1);
        if (pauseCtx->state != 0) {
            interfaceCtx->unk_1EC = 3;
        }
    }
}

void Interface_SetNaviCall(PlayState* play, u16 naviCallState) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    if (CVarGetInteger(CVAR_ENHANCEMENT("DisableNaviMessages"), 0)) {
        interfaceCtx->naviCalling = 0;
        return;
    }

    if (((naviCallState == 0x1D) || (naviCallState == 0x1E)) && !interfaceCtx->naviCalling &&
        (play->csCtx.state == CS_STATE_IDLE)) {
        if (!CVarGetInteger(CVAR_AUDIO("DisableNaviCallAudio"), 0)) {
            // clang-format off
            if (naviCallState == 0x1E) { Audio_PlaySoundGeneral(NA_SE_VO_NAVY_CALL, &gSfxDefaultPos, 4,
                                                                &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb); }
            // clang-format on

            if (naviCallState == 0x1D) {
                func_800F4524(&gSfxDefaultPos, NA_SE_VO_NA_HELLO_2, 32);
            }
        }

        interfaceCtx->naviCalling = 1;
        sCUpInvisible = 0;
        sCUpTimer = 10;
    } else if ((naviCallState == 0x1F) && interfaceCtx->naviCalling) {
        interfaceCtx->naviCalling = 0;
    }
}

void Interface_LoadActionLabelB(PlayState* play, u16 action) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    char* doAction = actionsTbl[action];
    static char newName[512];

    if (gSaveContext.language != LANGUAGE_ENG) {
        size_t length = strlen(doAction);
        strcpy(newName, doAction);
        if (gSaveContext.language == LANGUAGE_FRA) {
            newName[length - 6] = 'F';
            newName[length - 5] = 'R';
            newName[length - 4] = 'A';
        } else if (gSaveContext.language == LANGUAGE_GER) {
            newName[length - 6] = 'G';
            newName[length - 5] = 'E';
            newName[length - 4] = 'R';
        } else if (gSaveContext.language == LANGUAGE_JPN) {
            newName[length - 6] = 'J';
            newName[length - 5] = 'P';
            newName[length - 4] = 'N';
        }
        doAction = newName;
    }

    interfaceCtx->unk_1FC = action;

    char* segment = interfaceCtx->doActionSegment[1];
    interfaceCtx->doActionSegment[1] = action != DO_ACTION_NONE ? doAction : gEmptyTexture;
    osRecvMesg(&interfaceCtx->loadQueue, NULL, OS_MESG_BLOCK);

    interfaceCtx->unk_1FA = 1;
}

static void Health_GetTargetValuesForPlayer(Player* player, s16** health, s16** healthCapacity) {
    *health = &gSaveContext.health;
    *healthCapacity = &gSaveContext.healthCapacity;

    if ((player == NULL) || !player->isSecondPlayer) {
        return;
    }

    if (player->controllerPort == 4) {
        *health = &gSaveContext.health4;
        *healthCapacity = &gSaveContext.healthCapacity4;
    } else if (player->controllerPort == 3) {
        *health = &gSaveContext.health3;
        *healthCapacity = &gSaveContext.healthCapacity3;
    } else {
        *health = &gSaveContext.health2;
        *healthCapacity = &gSaveContext.healthCapacity2;
    }

    if (**healthCapacity < STARTING_HEALTH) {
        **healthCapacity = gSaveContext.healthCapacity;
        if (**healthCapacity < STARTING_HEALTH) {
            **healthCapacity = STARTING_HEALTH;
        }

        if (**health <= 0) {
            **health = **healthCapacity;
        }
    }
}

s32 Health_ChangeBy(PlayState* play, s16 healthChange, Player* player) {
    u16 heartCount;
    u16 healthLevel;
    s16* health;
    s16* healthCapacity;

    Health_GetTargetValuesForPlayer(player, &health, &healthCapacity);

    // "＊＊＊＊＊ Fluctuation=%d (now=%d, max=%d) ＊＊＊"
    osSyncPrintf("＊＊＊＊＊  増減=%d (now=%d, max=%d)  ＊＊＊", healthChange, *health, *healthCapacity);

    if (healthChange < 0) {
        gSaveContext.ship.stats.count[COUNT_DAMAGE_TAKEN] += -healthChange;
    }

    // If one-hit ko mode is on, any damage kills you and you cannot gain health.
    if (GameInteractor_OneHitKOActive()) {
        if (healthChange < 0) {
            *health = 0;
        }

        return 0;
    }

    // clang-format off
    if (healthChange > 0) { Audio_PlaySoundGeneral(NA_SE_SY_HP_RECOVER, &gSfxDefaultPos, 4,
                                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    } else if ((gSaveContext.isDoubleDefenseAcquired != 0) && (healthChange < 0)) {
        healthChange >>= 1;
        osSyncPrintf("ハート減少半分！！＝%d\n", healthChange); // "Heart decrease halved!!＝%d"
    }
    // clang-format on

    int32_t giDefenseModifier = GameInteractor_DefenseModifier();
    if (giDefenseModifier != 0 && healthChange < 0) {
        if (giDefenseModifier > 0) {
            healthChange /= giDefenseModifier;
        } else {
            healthChange *= abs(giDefenseModifier);
        }
    }

    *health += healthChange;

    if (*health > *healthCapacity) {
        *health = *healthCapacity;
    }

    heartCount = *health % FULL_HEART_HEALTH;

    healthLevel = heartCount;
    if (heartCount != 0) {
        if (heartCount > 10) {
            healthLevel = 3;
        } else if (heartCount > 5) {
            healthLevel = 2;
        } else {
            healthLevel = 1;
        }
    }

    // "Life=%d ＊＊＊  %d ＊＊＊＊＊＊"
    osSyncPrintf("  ライフ=%d  ＊＊＊  %d  ＊＊＊＊＊＊\n", *health, healthLevel);

    GameInteractor_ExecuteOnPlayerHealthChange(healthChange);

    if (*health <= 0) {
        *health = 0;
        return 0;
    } else {
        return 1;
    }
}

void Rupees_ChangeBy(s16 rupeeChange) {
    if (gPlayState == NULL) {
        gSaveContext.rupees += rupeeChange;
    } else {
        gSaveContext.rupeeAccumulator += rupeeChange;
    }

    if (rupeeChange > 0) {
        gSaveContext.ship.stats.count[COUNT_RUPEES_COLLECTED] += rupeeChange;
    }
    if (rupeeChange < 0) {
        gSaveContext.ship.stats.count[COUNT_RUPEES_SPENT] += -rupeeChange;
    }
}

void GameplayStats_UpdateAmmoUsed(s16 item, s16 ammoUsed) {

    switch (item) {
        case ITEM_STICK:
            gSaveContext.ship.stats.count[COUNT_AMMO_USED_STICK] += ammoUsed;
            break;
        case ITEM_NUT:
            gSaveContext.ship.stats.count[COUNT_AMMO_USED_NUT] += ammoUsed;
            break;
        case ITEM_BOMB:
            gSaveContext.ship.stats.count[COUNT_AMMO_USED_BOMB] += ammoUsed;
            break;
        case ITEM_BOW:
            gSaveContext.ship.stats.count[COUNT_AMMO_USED_ARROW] += ammoUsed;
            break;
        case ITEM_SLINGSHOT:
            gSaveContext.ship.stats.count[COUNT_AMMO_USED_SEED] += ammoUsed;
            break;
        case ITEM_BOMBCHU:
            gSaveContext.ship.stats.count[COUNT_AMMO_USED_BOMBCHU] += ammoUsed;
            break;
        case ITEM_BEAN:
            gSaveContext.ship.stats.count[COUNT_AMMO_USED_BEAN] += ammoUsed;
            break;
        default:
            break;
    }
    return;
}

void Inventory_ChangeAmmo(s16 item, s16 ammoChange) {
    // "Item = (%d)    Amount = (%d + %d)"
    osSyncPrintf("アイテム = (%d)    数 = (%d + %d)  ", item, AMMO(item), ammoChange);

    if (item == ITEM_STICK) {
        AMMO(ITEM_STICK) += ammoChange;

        if (AMMO(ITEM_STICK) >= CUR_CAPACITY(UPG_STICKS)) {
            AMMO(ITEM_STICK) = CUR_CAPACITY(UPG_STICKS);
        } else if (AMMO(ITEM_STICK) < 0) {
            AMMO(ITEM_STICK) = 0;
        }
    } else if (item == ITEM_NUT) {
        AMMO(ITEM_NUT) += ammoChange;

        if (AMMO(ITEM_NUT) >= CUR_CAPACITY(UPG_NUTS)) {
            AMMO(ITEM_NUT) = CUR_CAPACITY(UPG_NUTS);
        } else if (AMMO(ITEM_NUT) < 0) {
            AMMO(ITEM_NUT) = 0;
        }
    } else if (item == ITEM_BOMBCHU) {
        AMMO(ITEM_BOMBCHU) += ammoChange;

        if (GameInteractor_Should(VB_CHECK_BOMBCHU_CAPACITY, true)) {
            if (AMMO(ITEM_BOMBCHU) > 50) {
                AMMO(ITEM_BOMBCHU) = 50;
            }
        } else if (AMMO(ITEM_BOMBCHU) < 0) {
            AMMO(ITEM_BOMBCHU) = 0;
        }
    } else if (item == ITEM_BOW) {
        AMMO(ITEM_BOW) += ammoChange;

        if (AMMO(ITEM_BOW) >= CUR_CAPACITY(UPG_QUIVER)) {
            AMMO(ITEM_BOW) = CUR_CAPACITY(UPG_QUIVER);
        } else if (AMMO(ITEM_BOW) < 0) {
            AMMO(ITEM_BOW) = 0;
        }
    } else if ((item == ITEM_SLINGSHOT) || (item == ITEM_SEEDS)) {
        AMMO(ITEM_SLINGSHOT) += ammoChange;

        if (AMMO(ITEM_SLINGSHOT) >= CUR_CAPACITY(UPG_BULLET_BAG)) {
            AMMO(ITEM_SLINGSHOT) = CUR_CAPACITY(UPG_BULLET_BAG);
        } else if (AMMO(ITEM_SLINGSHOT) < 0) {
            AMMO(ITEM_SLINGSHOT) = 0;
        }
    } else if (item == ITEM_BOMB) {
        AMMO(ITEM_BOMB) += ammoChange;

        if (AMMO(ITEM_BOMB) >= CUR_CAPACITY(UPG_BOMB_BAG)) {
            AMMO(ITEM_BOMB) = CUR_CAPACITY(UPG_BOMB_BAG);
        } else if (AMMO(ITEM_BOMB) < 0) {
            AMMO(ITEM_BOMB) = 0;
        }
    } else if (item == ITEM_BEAN) {
        AMMO(ITEM_BEAN) += ammoChange;
    }

    osSyncPrintf("合計 = (%d)\n", AMMO(item)); // "Total = (%d)"

    if (ammoChange < 0) {
        GameplayStats_UpdateAmmoUsed(item, -ammoChange);
    }
}

static u8 Magic_GetControllerPortForPlayer(Player* player) {
    if ((player != NULL) && player->isSecondPlayer) {
        if ((player->controllerPort >= 2) && (player->controllerPort <= 4)) {
            return player->controllerPort;
        }
        return 2;
    }

    return 1;
}

static s32 Magic_IsSecondaryPort(u8 controllerPort) {
    return (controllerPort >= 2) && (controllerPort <= 4);
}

static s8* Magic_GetLevelPtrForPort(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sP4MagicLevel;
    }
    if (controllerPort == 3) {
        return &sP3MagicLevel;
    }
    if (controllerPort == 2) {
        return &gSaveContext.magicLevel2;
    }
    return &gSaveContext.magicLevel;
}

static s8* Magic_GetAmountPtrForPort(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sP4Magic;
    }
    if (controllerPort == 3) {
        return &sP3Magic;
    }
    if (controllerPort == 2) {
        return &gSaveContext.magic2;
    }
    return &gSaveContext.magic;
}

static u8* Magic_GetAcquiredPtrForPort(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sP4IsMagicAcquired;
    }
    if (controllerPort == 3) {
        return &sP3IsMagicAcquired;
    }
    if (controllerPort == 2) {
        return &gSaveContext.isMagicAcquired2;
    }
    return &gSaveContext.isMagicAcquired;
}

static u8* Magic_GetDoubleAcquiredPtrForPort(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sP4IsDoubleMagicAcquired;
    }
    if (controllerPort == 3) {
        return &sP3IsDoubleMagicAcquired;
    }
    if (controllerPort == 2) {
        return &gSaveContext.isDoubleMagicAcquired2;
    }
    return &gSaveContext.isDoubleMagicAcquired;
}

static s16* Magic_GetSecondaryCapacityPtr(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sP4MagicCapacity;
    }
    if (controllerPort == 3) {
        return &sP3MagicCapacity;
    }
    return &sP2MagicCapacity;
}

static s16* Magic_GetSecondaryTargetPtr(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sP4MagicTarget;
    }
    if (controllerPort == 3) {
        return &sP3MagicTarget;
    }
    return &sP2MagicTarget;
}

static s16* Magic_GetSecondaryFillTargetPtr(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sP4MagicFillTarget;
    }
    if (controllerPort == 3) {
        return &sP3MagicFillTarget;
    }
    return &sP2MagicFillTarget;
}

static s16* Magic_GetSecondaryStatePtr(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sP4MagicState;
    }
    if (controllerPort == 3) {
        return &sP3MagicState;
    }
    return &sP2MagicState;
}

static s16* Magic_GetSecondaryPrevStatePtr(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sP4PrevMagicState;
    }
    if (controllerPort == 3) {
        return &sP3PrevMagicState;
    }
    return &sP2PrevMagicState;
}

static s16* Magic_GetSecondaryBorderRatioPtr(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sP4MagicBorderRatio;
    }
    if (controllerPort == 3) {
        return &sP3MagicBorderRatio;
    }
    return &sP2MagicBorderRatio;
}

static s16* Magic_GetSecondaryBorderStepPtr(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sP4MagicBorderStep;
    }
    if (controllerPort == 3) {
        return &sP3MagicBorderStep;
    }
    return &sP2MagicBorderStep;
}

static s16* Magic_GetSecondaryLensTimerPtr(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sP4LensMagicConsumptionTimer;
    }
    if (controllerPort == 3) {
        return &sP3LensMagicConsumptionTimer;
    }
    return &sP2LensMagicConsumptionTimer;
}

static Color_RGB8* Magic_GetSecondaryBorderPtr(u8 controllerPort) {
    if (controllerPort == 4) {
        return &sMagicBorder4;
    }
    if (controllerPort == 3) {
        return &sMagicBorder3;
    }
    return &sMagicBorder2;
}

s16 Magic_GetLevelForPort(u8 controllerPort) {
    return *Magic_GetLevelPtrForPort(controllerPort);
}

s16 Magic_GetAmountForPort(u8 controllerPort) {
    return *Magic_GetAmountPtrForPort(controllerPort);
}

void Magic_InitForPort(u8 controllerPort) {
    s8* magicLevel;
    s8* magic;
    u8* isMagicAcquired;
    u8* isDoubleMagicAcquired;

    if (!Magic_IsSecondaryPort(controllerPort)) {
        return;
    }

    magicLevel = Magic_GetLevelPtrForPort(controllerPort);
    magic = Magic_GetAmountPtrForPort(controllerPort);
    isMagicAcquired = Magic_GetAcquiredPtrForPort(controllerPort);
    isDoubleMagicAcquired = Magic_GetDoubleAcquiredPtrForPort(controllerPort);

    if (gSaveContext.isMagicAcquired && !*isMagicAcquired) {
        *isMagicAcquired = gSaveContext.isMagicAcquired;
        *isDoubleMagicAcquired = gSaveContext.isDoubleMagicAcquired;
        *magicLevel = gSaveContext.magicLevel;
        *magic = gSaveContext.magic;
    }

    if (*isMagicAcquired && (*magicLevel == 0)) {
        *magicLevel = *isDoubleMagicAcquired + 1;
    }
}

void Magic_ResetExtraData(void) {
    sP2MagicCapacity = 0;
    sP2MagicTarget = 0;
    sP2MagicFillTarget = 0;
    sP2MagicState = MAGIC_STATE_IDLE;
    sP2PrevMagicState = MAGIC_STATE_IDLE;
    sP2MagicBorderRatio = 2;
    sP2MagicBorderStep = 1;
    sP2LensMagicConsumptionTimer = 0;
    sMagicBorder2 = (Color_RGB8){ 255, 255, 255 };

    sP3MagicLevel = 0;
    sP3Magic = MAGIC_NORMAL_METER;
    sP3IsMagicAcquired = false;
    sP3IsDoubleMagicAcquired = false;
    sP3MagicCapacity = 0;
    sP3MagicTarget = 0;
    sP3MagicFillTarget = 0;
    sP3MagicState = MAGIC_STATE_IDLE;
    sP3PrevMagicState = MAGIC_STATE_IDLE;
    sP3MagicBorderRatio = 2;
    sP3MagicBorderStep = 1;
    sP3LensMagicConsumptionTimer = 0;
    sMagicBorder3 = (Color_RGB8){ 255, 255, 255 };

    sP4MagicLevel = 0;
    sP4Magic = MAGIC_NORMAL_METER;
    sP4IsMagicAcquired = false;
    sP4IsDoubleMagicAcquired = false;
    sP4MagicCapacity = 0;
    sP4MagicTarget = 0;
    sP4MagicFillTarget = 0;
    sP4MagicState = MAGIC_STATE_IDLE;
    sP4PrevMagicState = MAGIC_STATE_IDLE;
    sP4MagicBorderRatio = 2;
    sP4MagicBorderStep = 1;
    sP4LensMagicConsumptionTimer = 0;
    sMagicBorder4 = (Color_RGB8){ 255, 255, 255 };
}

s16 Magic_GetStateForPlayer(Player* player) {
    u8 controllerPort = Magic_GetControllerPortForPlayer(player);

    if (Magic_IsSecondaryPort(controllerPort)) {
        return *Magic_GetSecondaryStatePtr(controllerPort);
    }
    return gSaveContext.magicState;
}

void Magic_SetStateForPlayer(Player* player, s16 state) {
    u8 controllerPort = Magic_GetControllerPortForPlayer(player);

    if (Magic_IsSecondaryPort(controllerPort)) {
        *Magic_GetSecondaryStatePtr(controllerPort) = state;
        return;
    }

    gSaveContext.magicState = state;
}

void Magic_FillForPlayer(PlayState* play, Player* player) {
    u8 controllerPort = Magic_GetControllerPortForPlayer(player);

    if (Magic_IsSecondaryPort(controllerPort)) {
        if (*Magic_GetAcquiredPtrForPort(controllerPort)) {
            *Magic_GetSecondaryPrevStatePtr(controllerPort) = *Magic_GetSecondaryStatePtr(controllerPort);
            *Magic_GetSecondaryFillTargetPtr(controllerPort) =
                (*Magic_GetDoubleAcquiredPtrForPort(controllerPort) + 1) * MAGIC_NORMAL_METER;
            *Magic_GetSecondaryStatePtr(controllerPort) = MAGIC_STATE_FILL;
        }
        return;
    }

    Magic_Fill(play);
}

void Magic_ResetForPlayer(PlayState* play, Player* player) {
    u8 controllerPort = Magic_GetControllerPortForPlayer(player);
    s16* magicState;

    if (Magic_IsSecondaryPort(controllerPort)) {
        magicState = Magic_GetSecondaryStatePtr(controllerPort);

        if ((*magicState != MAGIC_STATE_STEP_CAPACITY) && (*magicState != MAGIC_STATE_FILL)) {
            if (*magicState == MAGIC_STATE_ADD) {
                *Magic_GetSecondaryPrevStatePtr(controllerPort) = *magicState;
            }
            *magicState = MAGIC_STATE_RESET;
        }
        return;
    }

    Magic_Reset(play);
}

static s32 Magic_RequestChangeSecondary(PlayState* play, u8 controllerPort, s16 amount, s16 type) {
    s8* magic = Magic_GetAmountPtrForPort(controllerPort);
    u8* isMagicAcquired = Magic_GetAcquiredPtrForPort(controllerPort);
    s16* magicCapacity = Magic_GetSecondaryCapacityPtr(controllerPort);
    s16* magicTarget = Magic_GetSecondaryTargetPtr(controllerPort);
    s16* magicState = Magic_GetSecondaryStatePtr(controllerPort);
    s16* prevMagicState = Magic_GetSecondaryPrevStatePtr(controllerPort);
    s16* lensMagicConsumptionTimer = Magic_GetSecondaryLensTimerPtr(controllerPort);

    if (!*isMagicAcquired) {
        return false;
    }

    if ((type != MAGIC_ADD) && ((*magic - amount) < 0)) {
        if (*magicCapacity != 0) {
            Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
        return false;
    }

    switch (type) {
        case MAGIC_CONSUME_NOW:
        case MAGIC_CONSUME_NOW_ALT:
            if ((*magicState == MAGIC_STATE_IDLE) || (*magicState == MAGIC_STATE_CONSUME_LENS)) {
                if (*magicState == MAGIC_STATE_CONSUME_LENS) {
                    play->actorCtx.lensActive = false;
                }
                *magicTarget = *magic - amount;
                *magicState = MAGIC_STATE_CONSUME_SETUP;
                return true;
            }

            Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            return false;

        case MAGIC_CONSUME_WAIT_NO_PREVIEW:
            if ((*magicState == MAGIC_STATE_IDLE) || (*magicState == MAGIC_STATE_CONSUME_LENS)) {
                if (*magicState == MAGIC_STATE_CONSUME_LENS) {
                    play->actorCtx.lensActive = false;
                }
                *magicTarget = *magic - amount;
                *magicState = MAGIC_STATE_METER_FLASH_3;
                return true;
            }

            Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            return false;

        case MAGIC_CONSUME_LENS:
            if (*magicState == MAGIC_STATE_IDLE) {
                if (*magic != 0) {
                    *lensMagicConsumptionTimer = 80;
                    *magicState = MAGIC_STATE_CONSUME_LENS;
                    return true;
                }
                return false;
            }

            return *magicState == MAGIC_STATE_CONSUME_LENS;

        case MAGIC_CONSUME_WAIT_PREVIEW:
            if ((*magicState == MAGIC_STATE_IDLE) || (*magicState == MAGIC_STATE_CONSUME_LENS)) {
                if (*magicState == MAGIC_STATE_CONSUME_LENS) {
                    play->actorCtx.lensActive = false;
                }
                *magicTarget = *magic - amount;
                *magicState = MAGIC_STATE_METER_FLASH_2;
                return true;
            }

            Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            return false;

        case MAGIC_ADD:
            if (*magic <= *magicCapacity) {
                *magicTarget = *magic + amount;

                if (*magicTarget >= *magicCapacity) {
                    *magicTarget = *magicCapacity;
                }

                *magicState = MAGIC_STATE_ADD;
                return true;
            }
            break;
    }

    return false;
}

s32 Magic_RequestChangeForPlayer(PlayState* play, Player* player, s16 amount, s16 type) {
    u8 controllerPort = Magic_GetControllerPortForPlayer(player);

    if (Magic_IsSecondaryPort(controllerPort)) {
        return Magic_RequestChangeSecondary(play, controllerPort, amount, type);
    }

    return Magic_RequestChange(play, amount, type);
}

void Magic_Fill(PlayState* play) {
    if (gSaveContext.isMagicAcquired) {
        gSaveContext.prevMagicState = gSaveContext.magicState;
        gSaveContext.magicFillTarget = (gSaveContext.isDoubleMagicAcquired + 1) * MAGIC_NORMAL_METER;
        gSaveContext.magicState = MAGIC_STATE_FILL;
    }
}

void Magic_Reset(PlayState* play) {
    if ((gSaveContext.magicState != MAGIC_STATE_STEP_CAPACITY) && (gSaveContext.magicState != MAGIC_STATE_FILL)) {
        if (gSaveContext.magicState == MAGIC_STATE_ADD) {
            gSaveContext.prevMagicState = gSaveContext.magicState;
        }
        gSaveContext.magicState = MAGIC_STATE_RESET;
    }
}

s32 Magic_RequestChange(PlayState* play, s16 amount, s16 type) {
    if (!gSaveContext.isMagicAcquired) {
        return false;
    }

    if ((type != 5) && (gSaveContext.magic - amount) < 0) {
        if (gSaveContext.magicCapacity != 0) {
            Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
        return false;
    }

    switch (type) {
        case MAGIC_CONSUME_NOW:
        case MAGIC_CONSUME_NOW_ALT:
            if ((gSaveContext.magicState == MAGIC_STATE_IDLE) ||
                (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS)) {
                if (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS) {
                    play->actorCtx.lensActive = false;
                }
                gSaveContext.magicTarget = gSaveContext.magic - amount;
                gSaveContext.magicState = MAGIC_STATE_CONSUME_SETUP;
                return 1;
            } else {
                Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                return false;
            }
        case MAGIC_CONSUME_WAIT_NO_PREVIEW:
            if ((gSaveContext.magicState == MAGIC_STATE_IDLE) ||
                (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS)) {
                if (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS) {
                    play->actorCtx.lensActive = false;
                }
                gSaveContext.magicTarget = gSaveContext.magic - amount;
                gSaveContext.magicState = MAGIC_STATE_METER_FLASH_3;
                return true;
            } else {
                Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                return false;
            }
        case MAGIC_CONSUME_LENS:
            if (gSaveContext.magicState == MAGIC_STATE_IDLE) {
                if (gSaveContext.magic != 0) {
                    play->interfaceCtx.unk_230 = 80;
                    gSaveContext.magicState = MAGIC_STATE_CONSUME_LENS;
                    return true;
                } else {
                    return false;
                }
            } else {
                if (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS) {
                    return true;
                } else {
                    return false;
                }
            }
        case MAGIC_CONSUME_WAIT_PREVIEW:
            if ((gSaveContext.magicState == MAGIC_STATE_IDLE) ||
                (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS)) {
                if (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS) {
                    play->actorCtx.lensActive = false;
                }
                gSaveContext.magicTarget = gSaveContext.magic - amount;
                gSaveContext.magicState = MAGIC_STATE_METER_FLASH_2;
                return true;
            } else {
                Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                return false;
            }
        case MAGIC_ADD:
            if (gSaveContext.magicCapacity >= gSaveContext.magic) {
                gSaveContext.magicTarget = gSaveContext.magic + amount;

                if (gSaveContext.magicTarget >= gSaveContext.magicCapacity) {
                    gSaveContext.magicTarget = gSaveContext.magicCapacity;
                }

                gSaveContext.magicState = MAGIC_STATE_ADD;
                return true;
            }
            break;
    }

    return 0;
}

void Interface_UpdateMagicBar(PlayState* play) {
    static s16 sMagicBorderColors[][3] = {
        { 255, 255, 255 },
        { 150, 150, 150 },
        { 255, 255, 150 },
        { 255, 255, 50 },
    };
    Color_RGB8 MagicBorder_0 = { 255, 255, 255 };
    Color_RGB8 MagicBorder_1 = { 150, 150, 150 };
    Color_RGB8 MagicBorder_2 = { 255, 255, 150 };
    Color_RGB8 MagicBorder_3 = { 255, 255, 50 };

    if (CVarGetInteger(CVAR_COSMETIC("Consumable.MagicBorderActive.Changed"),
                       0)) { // This will make custom color based on users selected colors.
        sMagicBorderColors[0][0] = CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorderActive.Value"), MagicBorder_0).r;
        sMagicBorderColors[0][1] = CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorderActive.Value"), MagicBorder_0).g;
        sMagicBorderColors[0][2] = CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorderActive.Value"), MagicBorder_0).b;

        sMagicBorderColors[1][0] =
            CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorderActive.Value"), MagicBorder_1).r / 2;
        sMagicBorderColors[1][1] =
            CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorderActive.Value"), MagicBorder_1).g / 2;
        sMagicBorderColors[1][2] =
            CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorderActive.Value"), MagicBorder_1).b / 2;

        sMagicBorderColors[2][0] =
            CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorderActive.Value"), MagicBorder_2).r / 2.5;
        sMagicBorderColors[2][1] =
            CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorderActive.Value"), MagicBorder_2).g / 2.5;
        sMagicBorderColors[2][2] =
            CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorderActive.Value"), MagicBorder_2).b / 2.5;

        sMagicBorderColors[3][0] =
            CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorderActive.Value"), MagicBorder_3).r / 3;
        sMagicBorderColors[3][1] =
            CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorderActive.Value"), MagicBorder_3).g / 3;
        sMagicBorderColors[3][2] =
            CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorderActive.Value"), MagicBorder_3).b / 3;
    }

    static s16 sMagicBorderIndexes[] = { 0, 1, 1, 0 };
    static s16 sMagicBorderRatio = 2;
    static s16 sMagicBorderStep = 1;
    MessageContext* msgCtx = &play->msgCtx;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 borderChangeR;
    s16 borderChangeG;
    s16 borderChangeB;
    s16 temp;

    switch (gSaveContext.magicState) {
        case MAGIC_STATE_STEP_CAPACITY:
            temp = gSaveContext.magicLevel * MAGIC_NORMAL_METER;
            if (gSaveContext.magicCapacity != temp) {
                if (gSaveContext.magicCapacity < temp) {
                    gSaveContext.magicCapacity += 8;
                    if (gSaveContext.magicCapacity > temp) {
                        gSaveContext.magicCapacity = temp;
                    }
                } else {
                    gSaveContext.magicCapacity -= 8;
                    if (gSaveContext.magicCapacity <= temp) {
                        gSaveContext.magicCapacity = temp;
                    }
                }
            } else {
                gSaveContext.magicState = MAGIC_STATE_FILL;
            }
            break;

        case MAGIC_STATE_FILL:
            gSaveContext.magic += 4;

            if (gSaveContext.gameMode == GAMEMODE_NORMAL && gSaveContext.sceneSetupIndex < 4) {
                Audio_PlaySoundGeneral(NA_SE_SY_GAUGE_UP - SFX_FLAG, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }

            // "Storage  MAGIC_NOW=%d (%d)"
            osSyncPrintf("蓄電  MAGIC_NOW=%d (%d)\n", gSaveContext.magic, gSaveContext.magicFillTarget);
            if (gSaveContext.magic >= gSaveContext.magicFillTarget) {
                gSaveContext.magic = gSaveContext.magicFillTarget;
                gSaveContext.magicState = gSaveContext.prevMagicState;
                gSaveContext.prevMagicState = 0;
            }
            break;

        case MAGIC_STATE_CONSUME_SETUP:
            sMagicBorderRatio = 2;
            gSaveContext.magicState = MAGIC_STATE_CONSUME;
            break;

        case MAGIC_STATE_CONSUME:
            if (!Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER)) {
                gSaveContext.magic -= 2;
                if (gSaveContext.magic <= 0) {
                    gSaveContext.magic = 0;
                    gSaveContext.magicState = MAGIC_STATE_METER_FLASH_1;
                    if (CVarGetInteger(CVAR_COSMETIC("Consumable.MagicBorder.Changed"), 0)) {
                        sMagicBorder = CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorder.Value"), sMagicBorder_ori);
                    } else {
                        sMagicBorder = sMagicBorder_ori;
                    }
                } else if (gSaveContext.magic == gSaveContext.magicTarget) {
                    gSaveContext.magicState = MAGIC_STATE_METER_FLASH_1;
                    if (CVarGetInteger(CVAR_COSMETIC("Consumable.MagicBorder.Changed"), 0)) {
                        sMagicBorder = CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorder.Value"), sMagicBorder_ori);
                    } else {
                        sMagicBorder = sMagicBorder_ori;
                    }
                }
            }
        case MAGIC_STATE_METER_FLASH_1:
        case MAGIC_STATE_METER_FLASH_2:
        case MAGIC_STATE_METER_FLASH_3:
            temp = sMagicBorderIndexes[sMagicBorderStep];
            borderChangeR = ABS(sMagicBorder.r - sMagicBorderColors[temp][0]) / sMagicBorderRatio;
            borderChangeG = ABS(sMagicBorder.g - sMagicBorderColors[temp][1]) / sMagicBorderRatio;
            borderChangeB = ABS(sMagicBorder.b - sMagicBorderColors[temp][2]) / sMagicBorderRatio;

            if (sMagicBorder.r >= sMagicBorderColors[temp][0]) {
                sMagicBorder.r -= borderChangeR;
            } else {
                sMagicBorder.r += borderChangeR;
            }

            if (sMagicBorder.g >= sMagicBorderColors[temp][1]) {
                sMagicBorder.g -= borderChangeG;
            } else {
                sMagicBorder.g += borderChangeG;
            }

            if (sMagicBorder.b >= sMagicBorderColors[temp][2]) {
                sMagicBorder.b -= borderChangeB;
            } else {
                sMagicBorder.b += borderChangeB;
            }

            sMagicBorderRatio--;
            if (sMagicBorderRatio == 0) {
                sMagicBorder.r = sMagicBorderColors[temp][0];
                sMagicBorder.g = sMagicBorderColors[temp][1];
                sMagicBorder.b = sMagicBorderColors[temp][2];
                sMagicBorderRatio = YREG(40 + sMagicBorderStep);
                sMagicBorderStep++;
                if (sMagicBorderStep >= 4) {
                    sMagicBorderStep = 0;
                }
            }
            break;

        case MAGIC_STATE_RESET:
            if (CVarGetInteger(CVAR_COSMETIC("Consumable.MagicBorder.Changed"), 0)) {
                sMagicBorder = CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorder.Value"), sMagicBorder_ori);
            } else {
                sMagicBorder = sMagicBorder_ori;
            }
            gSaveContext.magicState = MAGIC_STATE_IDLE;
            break;

        case MAGIC_STATE_CONSUME_LENS:
            if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) && (msgCtx->msgMode == MSGMODE_NONE) &&
                (play->gameOverCtx.state == GAMEOVER_INACTIVE) && (play->transitionTrigger == TRANS_TRIGGER_OFF) &&
                (play->transitionMode == TRANS_MODE_OFF) && !Play_InCsMode(play)) {
                bool hasLens = false;
                for (int buttonIndex = 1; buttonIndex < ((CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) != 0)
                                                             ? ARRAY_COUNT(gSaveContext.equips.buttonItems)
                                                             : 4);
                     buttonIndex++) {
                    if (gSaveContext.equips.buttonItems[buttonIndex] == ITEM_LENS) {
                        hasLens = true;
                        break;
                    }
                }
                if ((gSaveContext.magic == 0) ||
                    ((Player_GetEnvironmentalHazard(play) >= 2) && (Player_GetEnvironmentalHazard(play) < 5)) ||
                    !hasLens || !play->actorCtx.lensActive) {
                    play->actorCtx.lensActive = false;
                    Audio_PlaySoundGeneral(NA_SE_SY_GLASSMODE_OFF, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    gSaveContext.magicState = MAGIC_STATE_IDLE;
                    if (CVarGetInteger(CVAR_COSMETIC("Consumable.MagicBorder.Changed"), 0)) {
                        sMagicBorder = CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorder.Value"), sMagicBorder_ori);
                    } else {
                        sMagicBorder = sMagicBorder_ori;
                    }
                    break;
                }

                interfaceCtx->unk_230--;
                if (interfaceCtx->unk_230 == 0) {
                    gSaveContext.magic--;
                    interfaceCtx->unk_230 = 80;
                }
            }

            temp = sMagicBorderIndexes[sMagicBorderStep];
            borderChangeR = ABS(sMagicBorder.r - sMagicBorderColors[temp][0]) / sMagicBorderRatio;
            borderChangeG = ABS(sMagicBorder.g - sMagicBorderColors[temp][1]) / sMagicBorderRatio;
            borderChangeB = ABS(sMagicBorder.b - sMagicBorderColors[temp][2]) / sMagicBorderRatio;

            if (sMagicBorder.r >= sMagicBorderColors[temp][0]) {
                sMagicBorder.r -= borderChangeR;
            } else {
                sMagicBorder.r += borderChangeR;
            }

            if (sMagicBorder.g >= sMagicBorderColors[temp][1]) {
                sMagicBorder.g -= borderChangeG;
            } else {
                sMagicBorder.g += borderChangeG;
            }

            if (sMagicBorder.b >= sMagicBorderColors[temp][2]) {
                sMagicBorder.b -= borderChangeB;
            } else {
                sMagicBorder.b += borderChangeB;
            }

            sMagicBorderRatio--;
            if (sMagicBorderRatio == 0) {
                sMagicBorder.r = sMagicBorderColors[temp][0];
                sMagicBorder.g = sMagicBorderColors[temp][1];
                sMagicBorder.b = sMagicBorderColors[temp][2];
                sMagicBorderRatio = YREG(40 + sMagicBorderStep);
                sMagicBorderStep++;
                if (sMagicBorderStep >= 4) {
                    sMagicBorderStep = 0;
                }
            }
            break;

        case MAGIC_STATE_ADD:
            gSaveContext.magic += 4;
            Audio_PlaySoundGeneral(NA_SE_SY_GAUGE_UP - SFX_FLAG, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            if (gSaveContext.magic >= gSaveContext.magicTarget) {
                gSaveContext.magic = gSaveContext.magicTarget;
                gSaveContext.magicState = gSaveContext.prevMagicState;
                gSaveContext.prevMagicState = 0;
            }
            break;

        default:
            gSaveContext.magicState = MAGIC_STATE_IDLE;
            if (CVarGetInteger(CVAR_COSMETIC("Consumable.MagicBorder.Changed"), 0)) {
                sMagicBorder = CVarGetColor24(CVAR_COSMETIC("Consumable.MagicBorder.Value"), sMagicBorder_ori);
            } else {
                sMagicBorder = sMagicBorder_ori;
            }
            break;
    }
}

static void Magic_UpdateSecondary(PlayState* play, u8 controllerPort) {
    static s16 sMagicBorderColors[][3] = {
        { 255, 255, 255 },
        { 150, 150, 150 },
        { 255, 255, 150 },
        { 255, 255, 50 },
    };
    static s16 sMagicBorderIndices[] = { 0, 1, 1, 0 };
    MessageContext* msgCtx = &play->msgCtx;
    s8* magicLevel = Magic_GetLevelPtrForPort(controllerPort);
    s8* magic = Magic_GetAmountPtrForPort(controllerPort);
    s16* magicCapacity = Magic_GetSecondaryCapacityPtr(controllerPort);
    s16* magicTarget = Magic_GetSecondaryTargetPtr(controllerPort);
    s16* magicFillTarget = Magic_GetSecondaryFillTargetPtr(controllerPort);
    s16* magicState = Magic_GetSecondaryStatePtr(controllerPort);
    s16* prevMagicState = Magic_GetSecondaryPrevStatePtr(controllerPort);
    s16* magicBorderRatio = Magic_GetSecondaryBorderRatioPtr(controllerPort);
    s16* magicBorderStep = Magic_GetSecondaryBorderStepPtr(controllerPort);
    s16* lensMagicConsumptionTimer = Magic_GetSecondaryLensTimerPtr(controllerPort);
    Color_RGB8* magicBorder = Magic_GetSecondaryBorderPtr(controllerPort);
    s16 borderChangeR;
    s16 borderChangeG;
    s16 borderChangeB;
    s16 temp;

    switch (*magicState) {
        case MAGIC_STATE_STEP_CAPACITY:
            temp = *magicLevel * MAGIC_NORMAL_METER;
            if (temp != *magicCapacity) {
                if (*magicCapacity < temp) {
                    *magicCapacity += 8;
                    if (*magicCapacity > temp) {
                        *magicCapacity = temp;
                    }
                } else {
                    *magicCapacity -= 8;
                    if (*magicCapacity <= temp) {
                        *magicCapacity = temp;
                    }
                }
            } else {
                *magicState = MAGIC_STATE_FILL;
            }
            break;

        case MAGIC_STATE_FILL:
            *magic += 4;

            if (gSaveContext.gameMode == GAMEMODE_NORMAL && gSaveContext.sceneSetupIndex < 4) {
                Audio_PlaySoundGeneral(NA_SE_SY_GAUGE_UP - SFX_FLAG, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }

            if (*magic >= *magicFillTarget) {
                *magic = *magicFillTarget;
                *magicState = *prevMagicState;
                *prevMagicState = MAGIC_STATE_IDLE;
            }
            break;

        case MAGIC_STATE_CONSUME_SETUP:
            *magicBorderRatio = 2;
            *magicState = MAGIC_STATE_CONSUME;
            break;

        case MAGIC_STATE_CONSUME:
            if (!Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER)) {
                *magic -= 2;

                if (*magic <= 0) {
                    *magic = 0;
                    *magicState = MAGIC_STATE_METER_FLASH_1;
                    *magicBorder = (Color_RGB8){ 255, 255, 255 };
                } else if (*magic == *magicTarget) {
                    *magicState = MAGIC_STATE_METER_FLASH_1;
                    *magicBorder = (Color_RGB8){ 255, 255, 255 };
                }
            }
            FALLTHROUGH;

        case MAGIC_STATE_METER_FLASH_1:
        case MAGIC_STATE_METER_FLASH_2:
        case MAGIC_STATE_METER_FLASH_3:
            temp = sMagicBorderIndices[*magicBorderStep];
            borderChangeR = ABS(magicBorder->r - sMagicBorderColors[temp][0]) / *magicBorderRatio;
            borderChangeG = ABS(magicBorder->g - sMagicBorderColors[temp][1]) / *magicBorderRatio;
            borderChangeB = ABS(magicBorder->b - sMagicBorderColors[temp][2]) / *magicBorderRatio;

            if (magicBorder->r >= sMagicBorderColors[temp][0]) {
                magicBorder->r -= borderChangeR;
            } else {
                magicBorder->r += borderChangeR;
            }

            if (magicBorder->g >= sMagicBorderColors[temp][1]) {
                magicBorder->g -= borderChangeG;
            } else {
                magicBorder->g += borderChangeG;
            }

            if (magicBorder->b >= sMagicBorderColors[temp][2]) {
                magicBorder->b -= borderChangeB;
            } else {
                magicBorder->b += borderChangeB;
            }

            *magicBorderRatio -= 1;
            if (*magicBorderRatio == 0) {
                magicBorder->r = sMagicBorderColors[temp][0];
                magicBorder->g = sMagicBorderColors[temp][1];
                magicBorder->b = sMagicBorderColors[temp][2];
                *magicBorderRatio = YREG(40 + *magicBorderStep);
                *magicBorderStep += 1;
                if (*magicBorderStep >= 4) {
                    *magicBorderStep = 0;
                }
            }
            break;

        case MAGIC_STATE_RESET:
            *magicBorder = (Color_RGB8){ 255, 255, 255 };
            *magicState = MAGIC_STATE_IDLE;
            break;

        case MAGIC_STATE_CONSUME_LENS:
            if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) && (msgCtx->msgMode == MSGMODE_NONE) &&
                (play->gameOverCtx.state == GAMEOVER_INACTIVE) && (play->transitionTrigger == TRANS_TRIGGER_OFF) &&
                (play->transitionMode == TRANS_MODE_OFF) && !Play_InCsMode(play)) {
                bool hasLens = false;

                for (s32 buttonIndex = 1; buttonIndex < ((CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) != 0)
                                                             ? ARRAY_COUNT(gSaveContext.equips.buttonItems)
                                                             : 4);
                     buttonIndex++) {
                    if (gSaveContext.equips.buttonItems[buttonIndex] == ITEM_LENS) {
                        hasLens = true;
                        break;
                    }
                }

                if ((Interface_GetQuickItemForPortAndSlot(controllerPort, 0) == ITEM_LENS) ||
                    (Interface_GetQuickItemForPortAndSlot(controllerPort, 1) == ITEM_LENS)) {
                    hasLens = true;
                }

                if ((*magic == 0) ||
                    ((Player_GetEnvironmentalHazard(play) >= PLAYER_ENV_HAZARD_UNDERWATER_FLOOR) &&
                     (Player_GetEnvironmentalHazard(play) <= PLAYER_ENV_HAZARD_UNDERWATER_FREE)) ||
                    !hasLens || !play->actorCtx.lensActive) {
                    play->actorCtx.lensActive = false;
                    Audio_PlaySoundGeneral(NA_SE_SY_GLASSMODE_OFF, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    *magicState = MAGIC_STATE_IDLE;
                    *magicBorder = (Color_RGB8){ 255, 255, 255 };
                    break;
                }

                *lensMagicConsumptionTimer -= 1;
                if (*lensMagicConsumptionTimer == 0) {
                    *magic -= 1;
                    *lensMagicConsumptionTimer = 80;
                }
            }

            temp = sMagicBorderIndices[*magicBorderStep];
            borderChangeR = ABS(magicBorder->r - sMagicBorderColors[temp][0]) / *magicBorderRatio;
            borderChangeG = ABS(magicBorder->g - sMagicBorderColors[temp][1]) / *magicBorderRatio;
            borderChangeB = ABS(magicBorder->b - sMagicBorderColors[temp][2]) / *magicBorderRatio;

            if (magicBorder->r >= sMagicBorderColors[temp][0]) {
                magicBorder->r -= borderChangeR;
            } else {
                magicBorder->r += borderChangeR;
            }

            if (magicBorder->g >= sMagicBorderColors[temp][1]) {
                magicBorder->g -= borderChangeG;
            } else {
                magicBorder->g += borderChangeG;
            }

            if (magicBorder->b >= sMagicBorderColors[temp][2]) {
                magicBorder->b -= borderChangeB;
            } else {
                magicBorder->b += borderChangeB;
            }

            *magicBorderRatio -= 1;
            if (*magicBorderRatio == 0) {
                magicBorder->r = sMagicBorderColors[temp][0];
                magicBorder->g = sMagicBorderColors[temp][1];
                magicBorder->b = sMagicBorderColors[temp][2];
                *magicBorderRatio = YREG(40 + *magicBorderStep);
                *magicBorderStep += 1;
                if (*magicBorderStep >= 4) {
                    *magicBorderStep = 0;
                }
            }
            break;

        case MAGIC_STATE_ADD:
            *magic += 4;
            Audio_PlaySoundGeneral(NA_SE_SY_GAUGE_UP - SFX_FLAG, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            if (*magic >= *magicTarget) {
                *magic = *magicTarget;
                *magicState = *prevMagicState;
                *prevMagicState = MAGIC_STATE_IDLE;
            }
            break;

        default:
            *magicState = MAGIC_STATE_IDLE;
            break;
    }
}

void Interface_DrawLineupTick(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_39Overlay(play->state.gfxCtx);

    gDPSetEnvColor(OVERLAY_DISP++, 255, 255, 255, 255);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);

    s16 width = 32;
    s16 height = 32;
    s16 x = -8 + (SCREEN_WIDTH / 2);
    s16 y = -6;

    OVERLAY_DISP =
        Gfx_TextureIA8(OVERLAY_DISP, gEmptyCDownArrowTex, width, height, x, y, width, height, 2 << 10, 2 << 10);

    gDPPipeSync(OVERLAY_DISP++);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void Interface_DrawMagicMeterForPortAt(PlayState* play, u8 controllerPort, s16 magicMeterX, s16 magicMeterY);
static s16 Interface_GetHeartRowCountForPort(u8 controllerPort);

void Interface_DrawMagicBar(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 magicDrop = R_MAGIC_BAR_LARGE_Y - R_MAGIC_BAR_SMALL_Y + 2;
    s16 magicBarY;
    Color_RGB8 magicbar_yellow = { 250, 250, 0 }; // Magic bar being used
    Color_RGB8 magicbar_green = { R_MAGIC_FILL_COLOR(0), R_MAGIC_FILL_COLOR(1),
                                  R_MAGIC_FILL_COLOR(2) }; // Magic bar fill
    Color_RGB8 magicbar_blue = { 0, 0, 200 };              // Infinite magic bar

    if (CVarGetInteger(CVAR_COSMETIC("Consumable.MagicActive.Changed"), 0)) {
        magicbar_yellow = CVarGetColor24(CVAR_COSMETIC("Consumable.MagicActive.Value"), magicbar_yellow);
    }
    if (CVarGetInteger(CVAR_COSMETIC("Consumable.Magic.Changed"), 0)) {
        magicbar_green = CVarGetColor24(CVAR_COSMETIC("Consumable.Magic.Value"), magicbar_green);
    }
    if (CVarGetInteger("gCosmetics.Consumable_MagicInfinite.Changed", 0)) {
        magicbar_blue = CVarGetColor24("gCosmetics.Consumable_MagicInfinite.Value", magicbar_blue);
    }

    OPEN_DISPS(play->state.gfxCtx);

    if (gSaveContext.magicLevel != 0) {
        s16 X_Margins;
        s16 Y_Margins;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.UseMargins"), 0) != 0) {
            X_Margins = Left_HUD_Margin;
            Y_Margins = (Top_HUD_Margin * -1);
        } else {
            X_Margins = 0;
            Y_Margins = 0;
        }
        const s16 magicBarY_original_l = R_MAGIC_BAR_LARGE_Y + Y_Margins;
        const s16 magicBarY_original_s = R_MAGIC_BAR_SMALL_Y + Y_Margins;
        const s16 PosX_Start_original = OTRGetRectDimensionFromLeftEdge(R_MAGIC_BAR_X + X_Margins);
        const s16 PosX_MidEnd_original = OTRGetRectDimensionFromLeftEdge(R_MAGIC_BAR_X + X_Margins + 8);
        const s16 rMagicBarX_original = OTRGetRectDimensionFromLeftEdge(R_MAGIC_BAR_X + X_Margins);
        const s16 rMagicFillX_original = OTRGetRectDimensionFromLeftEdge(R_MAGIC_FILL_X + X_Margins);
        s16 PosX_Start;
        s16 magicBarY;
        s16 rMagicBarX;
        s16 PosX_MidEnd;
        s16 rMagicFillX;
        s32 lineLength = CVarGetInteger(CVAR_COSMETIC("HUD.Hearts.LineLength"), 10);
        if (CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosType"), 0) != ORIGINAL_LOCATION) {
            magicBarY = CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosY"), 0) + Y_Margins;
            if (CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosType"), 0) == ANCHOR_LEFT) {
                if (CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.UseMargins"), 0) != 0) {
                    X_Margins = Left_HUD_Margin;
                };
                PosX_Start =
                    OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + X_Margins);
                rMagicBarX =
                    OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + X_Margins);
                PosX_MidEnd =
                    OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + X_Margins + 8);
                rMagicFillX =
                    OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + X_Margins + 8);
            } else if (CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosType"), 0) == ANCHOR_RIGHT) {
                if (CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.UseMargins"), 0) != 0) {
                    X_Margins = Right_HUD_Margin;
                };
                PosX_Start =
                    OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + X_Margins);
                rMagicBarX =
                    OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + X_Margins);
                PosX_MidEnd =
                    OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + X_Margins + 8);
                rMagicFillX =
                    OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + X_Margins + 8);
            } else if (CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosType"), 0) == ANCHOR_NONE) {
                PosX_Start = CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + X_Margins;
                rMagicBarX = CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + X_Margins;
                PosX_MidEnd = CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + X_Margins + 8;
                rMagicFillX = CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + X_Margins + 8;
            } else if (CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosType"), 0) == HIDDEN) {
                PosX_Start = -9999;
                rMagicBarX = -9999;
                PosX_MidEnd = -9999;
                rMagicFillX = -9999;
            } else if (CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosType"), 0) == ANCHOR_TO_LIFE_METER) {
                magicBarY =
                    R_MAGIC_BAR_SMALL_Y - 2 +
                    magicDrop * (lineLength == 0 ? 0 : (gSaveContext.healthCapacity - 1) / (0x10 * lineLength)) +
                    CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosY"), 0) + getHealthMeterYOffset();
                s16 xPushover =
                    CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + getHealthMeterXOffset() + R_MAGIC_BAR_X - 1;
                PosX_Start = xPushover;
                rMagicBarX = xPushover;
                PosX_MidEnd = xPushover + 8;
                rMagicFillX = CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosX"), 0) + getHealthMeterXOffset() +
                              R_MAGIC_FILL_X - 1;
            }
        } else {
            if ((gSaveContext.healthCapacity - 1) / FULL_HEART_HEALTH >= lineLength && lineLength != 0) {
                magicBarY =
                    magicBarY_original_l +
                    magicDrop * (lineLength == 0 ? 0 : ((gSaveContext.healthCapacity - 1) / (0x10 * lineLength) - 1));
            } else {
                magicBarY = magicBarY_original_s;
            }
            PosX_Start = PosX_Start_original;
            rMagicBarX = rMagicBarX_original;
            PosX_MidEnd = PosX_MidEnd_original;
            rMagicFillX = rMagicFillX_original;
        }

        if (Interface_IsOriginalMultiplayerItemModeEnabled() && !Interface_IsSplitScreenEnabled() &&
            (CVarGetInteger(CVAR_COSMETIC("HUD.MagicBar.PosType"), 0) == ORIGINAL_LOCATION)) {
            f32 heartsScale = 0.68f;
            f32 topHeartsAnchorY;
            f32 heartsBottomY;
            s16 heartCount = gSaveContext.healthCapacity / FULL_HEART_HEALTH;
            s16 heartRows;

            if (heartCount <= 0) {
                heartCount = STARTING_HEALTH / FULL_HEART_HEALTH;
            }
            heartRows = (heartCount + 9) / 10;

            if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) != ORIGINAL_LOCATION) {
                heartsScale = CVarGetFloat(CVAR_COSMETIC("HUD.HeartsCount.Scale"), 0.7f);
            }

            topHeartsAnchorY = getHealthMeterYOffset() + 21.0f - (8.0f * heartsScale);
            heartsBottomY = topHeartsAnchorY + ((heartRows - 1) * 10.0f) + (16.0f * heartsScale);
            magicBarY = (s16)(heartsBottomY + 0.0f);
        }

        Gfx_SetupDL_39Overlay(play->state.gfxCtx);

        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, sMagicBorder.r, sMagicBorder.g, sMagicBorder.b, interfaceCtx->magicAlpha);
        gDPSetEnvColor(OVERLAY_DISP++, 100, 50, 50, 255);

        OVERLAY_DISP =
            Gfx_TextureIA8(OVERLAY_DISP, gMagicMeterEndTex, 8, 16, PosX_Start, magicBarY, 8, 16, 1 << 10, 1 << 10);

        OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gMagicMeterMidTex, 24, 16, PosX_MidEnd, magicBarY,
                                      gSaveContext.magicCapacity, 16, 1 << 10, 1 << 10);

        gDPLoadTextureBlock(OVERLAY_DISP++, gMagicMeterEndTex, G_IM_FMT_IA, G_IM_SIZ_8b, 8, 16, 0,
                            G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 3, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

        gSPWideTextureRectangle(OVERLAY_DISP++, ((rMagicBarX + gSaveContext.magicCapacity) + 8) << 2, magicBarY << 2,
                                ((rMagicBarX + gSaveContext.magicCapacity) + 16) << 2, (magicBarY + 16) << 2,
                                G_TX_RENDERTILE, 256, 0, 1 << 10, 1 << 10);

        gDPPipeSync(OVERLAY_DISP++);
        gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE, PRIMITIVE,
                          ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE);
        gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);

        if (gSaveContext.magicState == MAGIC_STATE_METER_FLASH_2) {
            // Yellow part of the bar indicating the amount of magic to be subtracted
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbar_yellow.r, magicbar_yellow.g, magicbar_yellow.b,
                            interfaceCtx->magicAlpha);

            gDPLoadMultiBlock_4b(OVERLAY_DISP++, gMagicMeterFillTex, 0, G_TX_RENDERTILE, G_IM_FMT_I, 16, 16, 0,
                                 G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                 G_TX_NOLOD, G_TX_NOLOD);

            gSPWideTextureRectangle(OVERLAY_DISP++, rMagicFillX << 2, (magicBarY + 3) << 2,
                                    (rMagicFillX + gSaveContext.magic) << 2, (magicBarY + 10) << 2, G_TX_RENDERTILE, 0,
                                    0, 1 << 10, 1 << 10);

            // Fill the rest of the bar with the normal magic color
            gDPPipeSync(OVERLAY_DISP++);
            if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER)) {
                // Blue magic
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbar_blue.r, magicbar_blue.g, magicbar_blue.b,
                                interfaceCtx->magicAlpha);
            } else {
                // Green magic (default)
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbar_green.r, magicbar_green.g, magicbar_green.b,
                                interfaceCtx->magicAlpha);
            }

            gSPWideTextureRectangle(OVERLAY_DISP++, rMagicFillX << 2, (magicBarY + 3) << 2,
                                    (rMagicFillX + gSaveContext.magicTarget) << 2, (magicBarY + 10) << 2,
                                    G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        } else {
            // Fill the whole bar with the normal magic color
            if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER)) {
                // Blue magic
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbar_blue.r, magicbar_blue.g, magicbar_blue.b,
                                interfaceCtx->magicAlpha);
            } else {
                // Green magic (default)
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbar_green.r, magicbar_green.g, magicbar_green.b,
                                interfaceCtx->magicAlpha);
            }

            gDPLoadMultiBlock_4b(OVERLAY_DISP++, gMagicMeterFillTex, 0, G_TX_RENDERTILE, G_IM_FMT_I, 16, 16, 0,
                                 G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                 G_TX_NOLOD, G_TX_NOLOD);

            gSPWideTextureRectangle(OVERLAY_DISP++, rMagicFillX << 2, (magicBarY + 3) << 2,
                                    (rMagicFillX + gSaveContext.magic) << 2, (magicBarY + 10) << 2, G_TX_RENDERTILE, 0,
                                    0, 1 << 10, 1 << 10);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);

    if (!Interface_IsOriginalMultiplayerItemModeEnabled()) {
        s16 playerCount = Interface_GetLocalMultiplayerPlayerCount();

        if (playerCount > 1) {
            s16 i;
            f32 heartsScale = 0.68f;
            f32 topHeartsAnchorY;
            f32 topHeartsCenterY;
            f32 bottomHeartsAnchorY;
            f32 splitViewportScaleX;
            s32 splitScreenEnabled = Interface_IsSplitScreenEnabled();
            const s16 leftMagicAnchorX = OTRGetRectDimensionFromLeftEdge(22.0f);
            const s16 iconSize = 24;
            const s16 magicHeight = 16;
            const s16 sharedMagicOffset = 2;

            if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) != ORIGINAL_LOCATION) {
                heartsScale = CVarGetFloat(CVAR_COSMETIC("HUD.HeartsCount.Scale"), 0.7f);
            }

            splitViewportScaleX =
                (OTRGetDimensionFromRightEdge(SCREEN_WIDTH) - OTRGetDimensionFromLeftEdge(0.0f)) / SCREEN_WIDTH;

            topHeartsAnchorY = getHealthMeterYOffset() + 21.0f - (8.0f * heartsScale);
            topHeartsCenterY = topHeartsAnchorY + (8.0f * heartsScale);
            bottomHeartsAnchorY = SCREEN_HEIGHT - topHeartsCenterY;

            for (i = 1; i < playerCount; i++) {
                s16 magicCapacity;
                s16 magicWidth;
                s16 heartsTopY;
                s16 heartsBottomY;
                s16 heartRows;
                s16 magicX;
                s16 magicY;
                u8 controllerPort = i + 1;

                if (Magic_GetLevelForPort(controllerPort) == 0) {
                    continue;
                }

                heartRows = Interface_GetHeartRowCountForPort(controllerPort);
                magicCapacity = Magic_GetLevelForPort(controllerPort) * MAGIC_NORMAL_METER;
                magicWidth = 16 + magicCapacity;

                if (splitScreenEnabled) {
                    s16 viewportLeftX;
                    s16 viewportTopY;
                    s16 viewportLeftHudSpace;

                    Interface_GetSplitViewportTopLeft(playerCount, i, &viewportLeftX, &viewportTopY);
                    viewportLeftHudSpace =
                        (s16)(OTRGetDimensionFromLeftEdge((f32)viewportLeftX) + (viewportLeftX * splitViewportScaleX));

                    heartsTopY = (s16)(viewportTopY + topHeartsAnchorY);
                    heartsBottomY = (s16)(heartsTopY + ((heartRows - 1) * 10.0f) + (16.0f * heartsScale));
                    magicX = viewportLeftHudSpace + leftMagicAnchorX;
                    magicY = (s16)(heartsBottomY + 1.0f);
                } else {
                    s16 playerSideAnchorX = (i % 2) ? (SCREEN_WIDTH - iconSize - leftMagicAnchorX) : leftMagicAnchorX;

                    heartsTopY = (s16)((i < 2) ? topHeartsAnchorY : bottomHeartsAnchorY);
                    heartsBottomY = (s16)(heartsTopY + ((heartRows - 1) * 10.0f) + (16.0f * heartsScale));

                    if (i % 2) {
                        magicX = playerSideAnchorX + iconSize - magicWidth;
                    } else {
                        magicX = playerSideAnchorX;
                    }

                    if (i < 2) {
                        magicY = (s16)(heartsBottomY + sharedMagicOffset);
                    } else {
                        magicY = (s16)(heartsTopY - magicHeight + sharedMagicOffset);
                    }
                }

                Interface_DrawMagicMeterForPortAt(play, controllerPort, magicX, magicY);
            }
        }
    }
}

static s16 Magic_GetMeterCapacityForPort(u8 controllerPort) {
    if (Magic_IsSecondaryPort(controllerPort)) {
        return *Magic_GetSecondaryCapacityPtr(controllerPort);
    }

    return gSaveContext.magicCapacity;
}

static s16 Magic_GetMeterTargetForPort(u8 controllerPort) {
    if (Magic_IsSecondaryPort(controllerPort)) {
        return *Magic_GetSecondaryTargetPtr(controllerPort);
    }

    return gSaveContext.magicTarget;
}

static s16 Magic_GetMeterStateForPort(u8 controllerPort) {
    if (Magic_IsSecondaryPort(controllerPort)) {
        return *Magic_GetSecondaryStatePtr(controllerPort);
    }

    return gSaveContext.magicState;
}

static Color_RGB8 Magic_GetBorderColorForPort(u8 controllerPort) {
    if (Magic_IsSecondaryPort(controllerPort)) {
        return *Magic_GetSecondaryBorderPtr(controllerPort);
    }

    return sMagicBorder;
}

static void Interface_DrawMagicMeterForPortAt(PlayState* play, u8 controllerPort, s16 magicMeterX, s16 magicMeterY) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 magicCapacity;
    s16 currentMagic;
    s16 magicTarget;
    s16 magicState;
    s16 magicFillX;
    Color_RGB8 magicBorder;
    Color_RGB8 magicbarYellow = { 250, 250, 0 };
    Color_RGB8 magicbarGreen = { R_MAGIC_FILL_COLOR(0), R_MAGIC_FILL_COLOR(1), R_MAGIC_FILL_COLOR(2) };
    Color_RGB8 magicbarBlue = { 0, 0, 200 };

    if (Magic_GetLevelForPort(controllerPort) == 0) {
        return;
    }

    magicCapacity = Magic_GetMeterCapacityForPort(controllerPort);
    if (magicCapacity <= 0) {
        magicCapacity = Magic_GetLevelForPort(controllerPort) * MAGIC_NORMAL_METER;
    }

    currentMagic = Magic_GetAmountForPort(controllerPort);
    magicTarget = Magic_GetMeterTargetForPort(controllerPort);
    magicState = Magic_GetMeterStateForPort(controllerPort);
    magicFillX = magicMeterX + 8;
    magicBorder = Magic_GetBorderColorForPort(controllerPort);

    if (CVarGetInteger(CVAR_COSMETIC("Consumable.MagicActive.Changed"), 0)) {
        magicbarYellow = CVarGetColor24(CVAR_COSMETIC("Consumable.MagicActive.Value"), magicbarYellow);
    }

    if (CVarGetInteger(CVAR_COSMETIC("Consumable.Magic.Changed"), 0)) {
        magicbarGreen = CVarGetColor24(CVAR_COSMETIC("Consumable.Magic.Value"), magicbarGreen);
    }

    if (CVarGetInteger("gCosmetics.Consumable_MagicInfinite.Changed", 0)) {
        magicbarBlue = CVarGetColor24("gCosmetics.Consumable_MagicInfinite.Value", magicbarBlue);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_39Overlay(play->state.gfxCtx);

    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicBorder.r, magicBorder.g, magicBorder.b, interfaceCtx->magicAlpha);
    gDPSetEnvColor(OVERLAY_DISP++, 100, 50, 50, 255);

    OVERLAY_DISP =
        Gfx_TextureIA8(OVERLAY_DISP, gMagicMeterEndTex, 8, 16, magicMeterX, magicMeterY, 8, 16, 1 << 10, 1 << 10);

    OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gMagicMeterMidTex, 24, 16, magicMeterX + 8, magicMeterY,
                                  magicCapacity, 16, 1 << 10, 1 << 10);

    gDPLoadTextureBlock(OVERLAY_DISP++, gMagicMeterEndTex, G_IM_FMT_IA, G_IM_SIZ_8b, 8, 16, 0,
                        G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 3, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    gSPWideTextureRectangle(OVERLAY_DISP++, (magicMeterX + 8 + magicCapacity) << 2, magicMeterY << 2,
                            (magicMeterX + 8 + magicCapacity + 8) << 2, (magicMeterY + 16) << 2, G_TX_RENDERTILE,
                            256, 0, 1 << 10, 1 << 10);

    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE);
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);

    if (magicState == MAGIC_STATE_METER_FLASH_2) {
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbarYellow.r, magicbarYellow.g, magicbarYellow.b,
                        interfaceCtx->magicAlpha);

        gDPLoadMultiBlock_4b(OVERLAY_DISP++, gMagicMeterFillTex, 0x0000, G_TX_RENDERTILE, G_IM_FMT_I, 16, 16, 0,
                             G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                             G_TX_NOLOD, G_TX_NOLOD);

        gSPWideTextureRectangle(OVERLAY_DISP++, magicFillX << 2, (magicMeterY + 3) << 2,
                                (magicFillX + currentMagic) << 2, (magicMeterY + 10) << 2, G_TX_RENDERTILE, 0, 0,
                                1 << 10, 1 << 10);

        gDPPipeSync(OVERLAY_DISP++);
        if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER)) {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbarBlue.r, magicbarBlue.g, magicbarBlue.b,
                            interfaceCtx->magicAlpha);
        } else {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbarGreen.r, magicbarGreen.g, magicbarGreen.b,
                            interfaceCtx->magicAlpha);
        }

        gSPWideTextureRectangle(OVERLAY_DISP++, magicFillX << 2, (magicMeterY + 3) << 2,
                                (magicFillX + magicTarget) << 2, (magicMeterY + 10) << 2, G_TX_RENDERTILE, 0, 0,
                                1 << 10, 1 << 10);
    } else {
        if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER)) {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbarBlue.r, magicbarBlue.g, magicbarBlue.b,
                            interfaceCtx->magicAlpha);
        } else {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbarGreen.r, magicbarGreen.g, magicbarGreen.b,
                            interfaceCtx->magicAlpha);
        }

        gDPLoadMultiBlock_4b(OVERLAY_DISP++, gMagicMeterFillTex, 0x0000, G_TX_RENDERTILE, G_IM_FMT_I, 16, 16, 0,
                             G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                             G_TX_NOLOD, G_TX_NOLOD);

        gSPWideTextureRectangle(OVERLAY_DISP++, magicFillX << 2, (magicMeterY + 3) << 2,
                                (magicFillX + currentMagic) << 2, (magicMeterY + 10) << 2, G_TX_RENDERTILE, 0, 0,
                                1 << 10, 1 << 10);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static Vtx sEnemyHealthVtx[16];
static Mtx sEnemyHealthMtx[2];

// Draws an enemy health bar using the magic bar textures and positions it in a similar way to Z-Targeting
void Interface_DrawEnemyHealthBar(TargetContext* targetCtx, PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Player* player = GET_PLAYER(play);
    Actor* actor = targetCtx->targetedActor;

    Vec3f projTargetCenter;
    f32 projTargetCappedInvW;

    Color_RGBA8 healthbar_red = { 255, 0, 0, 255 };
    Color_RGBA8 healthbar_border = { 255, 255, 255, 255 };
    s16 healthbar_fillWidth = 64;
    s16 healthbar_actorOffset = 40;
    s32 healthbar_offsetX = CVarGetInteger(CVAR_COSMETIC("HUD.EnemyHealthBar.PosX"), 0);
    s32 healthbar_offsetY = CVarGetInteger(CVAR_COSMETIC("HUD.EnemyHealthBar.PosY"), 0);
    s8 anchorType = CVarGetInteger(CVAR_COSMETIC("HUD.EnemyHealthBar.PosType"), ENEMYHEALTH_ANCHOR_ACTOR);

    if (CVarGetInteger(CVAR_COSMETIC("HUD.EnemyHealthBar.Changed"), 0)) {
        healthbar_red = CVarGetColor(CVAR_COSMETIC("HUD.EnemyHealthBar.Value"), healthbar_red);
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.EnemyHealthBorder.Changed"), 0)) {
        healthbar_border = CVarGetColor(CVAR_COSMETIC("HUD.EnemyHealthBorder.Value"), healthbar_border);
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.EnemyHealthBar.Width.Changed"), 0)) {
        healthbar_fillWidth = CVarGetInteger(CVAR_COSMETIC("HUD.EnemyHealthBar.Width.Value"), healthbar_fillWidth);
    }

    OPEN_DISPS(play->state.gfxCtx);

    if (targetCtx->unk_48 != 0 && actor != NULL && actor->category == ACTORCAT_ENEMY) {
        s16 texHeight = 16;
        s16 endTexWidth = 8;
        f32 scaleY = -0.75f;
        f32 scaledHeight = -texHeight * scaleY;
        f32 halfBarWidth = endTexWidth + ((f32)healthbar_fillWidth / 2);
        s16 healthBarFill = ((f32)actor->colChkInfo.health / GetActorMaximumHealth(actor)) * healthbar_fillWidth;

        if (anchorType == ENEMYHEALTH_ANCHOR_ACTOR) {
            // Get actor projected position
            func_8002BE04(play, &targetCtx->targetCenterPos, &projTargetCenter, &projTargetCappedInvW);

            projTargetCenter.x = (SCREEN_WIDTH / 2) * (projTargetCenter.x * projTargetCappedInvW);
            projTargetCenter.x = projTargetCenter.x * (CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0) ? -1 : 1);
            projTargetCenter.x =
                CLAMP(projTargetCenter.x, (-SCREEN_WIDTH / 2) + halfBarWidth, (SCREEN_WIDTH / 2) - halfBarWidth);

            projTargetCenter.y = (SCREEN_HEIGHT / 2) * (projTargetCenter.y * projTargetCappedInvW);
            projTargetCenter.y = projTargetCenter.y - healthbar_offsetY + healthbar_actorOffset;
            projTargetCenter.y = CLAMP(projTargetCenter.y, (-SCREEN_HEIGHT / 2) + (scaledHeight / 2),
                                       (SCREEN_HEIGHT / 2) - (scaledHeight / 2));
        } else if (anchorType == ENEMYHEALTH_ANCHOR_TOP) {
            projTargetCenter.x = healthbar_offsetX;
            projTargetCenter.y = (SCREEN_HEIGHT / 2) - (scaledHeight / 2) - healthbar_offsetY;
        } else if (anchorType == ENEMYHEALTH_ANCHOR_BOTTOM) {
            projTargetCenter.x = healthbar_offsetX;
            projTargetCenter.y = (-SCREEN_HEIGHT / 2) + (scaledHeight / 2) - healthbar_offsetY;
        }

        // Health bar border end left
        Ship_CreateQuadVertexGroup(&sEnemyHealthVtx[0], -floorf(halfBarWidth), -texHeight / 2, endTexWidth, texHeight,
                                   0);
        // Health bar border middle
        Ship_CreateQuadVertexGroup(&sEnemyHealthVtx[4], -floorf(halfBarWidth) + endTexWidth, -texHeight / 2,
                                   healthbar_fillWidth, texHeight, 0);
        // Health bar border end right
        Ship_CreateQuadVertexGroup(&sEnemyHealthVtx[8], ceilf(halfBarWidth) - endTexWidth, -texHeight / 2, endTexWidth,
                                   texHeight, 1);
        // Health bar fill
        Ship_CreateQuadVertexGroup(&sEnemyHealthVtx[12], -floorf(halfBarWidth) + endTexWidth, (-texHeight / 2) + 3,
                                   healthBarFill, 7, 0);

        if (((!(player->stateFlags1 & PLAYER_STATE1_TALKING)) || (actor != player->focusActor)) &&
            targetCtx->unk_44 < 500.0f) {
            f32 slideInOffsetY = 0;

            // Slide in the health bar from edge of the screen (mimic the Z-Target triangles fly in)
            if (anchorType == ENEMYHEALTH_ANCHOR_ACTOR && targetCtx->unk_44 > 120.0f) {
                slideInOffsetY = (targetCtx->unk_44 - 120.0f) / 2;
                // Slide in from the top if the bar is placed on the top half of the screen
                if (healthbar_offsetY - healthbar_actorOffset <= 0) {
                    slideInOffsetY *= -1;
                }
            }

            // Setup DL for overlay disp
            Gfx_SetupDL_39Overlay(play->state.gfxCtx);

            Matrix_Translate(projTargetCenter.x, projTargetCenter.y - slideInOffsetY, 0, MTXMODE_NEW);
            Matrix_Scale(1.0f, scaleY, 1.0f, MTXMODE_APPLY);
            Matrix_ToMtx(&sEnemyHealthMtx[0], __FILE__, __LINE__);
            gSPMatrix(OVERLAY_DISP++, &sEnemyHealthMtx[0], G_MTX_MODELVIEW | G_MTX_LOAD);

            // Health bar border
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, healthbar_border.r, healthbar_border.g, healthbar_border.b,
                            healthbar_border.a);
            gDPSetEnvColor(OVERLAY_DISP++, 100, 50, 50, 255);

            gSPVertex(OVERLAY_DISP++, sEnemyHealthVtx, 16, 0);

            gDPLoadTextureBlock(OVERLAY_DISP++, gMagicMeterEndTex, G_IM_FMT_IA, G_IM_SIZ_8b, endTexWidth, texHeight, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);

            gSP1Quadrangle(OVERLAY_DISP++, 0, 2, 3, 1, 0);

            gDPLoadTextureBlock(OVERLAY_DISP++, gMagicMeterMidTex, G_IM_FMT_IA, G_IM_SIZ_8b, 24, texHeight, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);

            gSP1Quadrangle(OVERLAY_DISP++, 4, 6, 7, 5, 0);

            gDPLoadTextureBlock(OVERLAY_DISP++, gMagicMeterEndTex, G_IM_FMT_IA, G_IM_SIZ_8b, endTexWidth, texHeight, 0,
                                G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 3, G_TX_NOMASK, G_TX_NOLOD,
                                G_TX_NOLOD);

            gSP1Quadrangle(OVERLAY_DISP++, 8, 10, 11, 9, 0);

            // Health bar fill
            Matrix_Push();
            Matrix_Translate(-0.375f, -0.5f, 0, MTXMODE_APPLY);
            Matrix_ToMtx(&sEnemyHealthMtx[1], __FILE__, __LINE__);
            gSPMatrix(OVERLAY_DISP++, &sEnemyHealthMtx[1], G_MTX_MODELVIEW | G_MTX_LOAD);

            gDPPipeSync(OVERLAY_DISP++);
            gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE,
                              PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE);
            gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);

            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, healthbar_red.r, healthbar_red.g, healthbar_red.b, healthbar_red.a);

            gDPLoadMultiBlock_4b(OVERLAY_DISP++, gMagicMeterFillTex, 0, G_TX_RENDERTILE, G_IM_FMT_I, 16, texHeight, 0,
                                 G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                 G_TX_NOLOD, G_TX_NOLOD);

            gSPVertex(OVERLAY_DISP++, &sEnemyHealthVtx[12], 4, 0);

            gSP1Quadrangle(OVERLAY_DISP++, 0, 2, 3, 1, 0);

            Matrix_Pop();
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void Interface_SetSubTimer(s16 seconds) {
    gSaveContext.timerX[TIMER_ID_SUB] = 140;
    gSaveContext.timerY[TIMER_ID_SUB] = 80;
    sEnvHazardActive = false;
    gSaveContext.subTimerSeconds = seconds;

    if (seconds) {
        gSaveContext.subTimerState = SUBTIMER_STATE_DOWN_INIT;
    } else {
        gSaveContext.subTimerState = SUBTIMER_STATE_UP_INIT;
    }
}

void Interface_SetSubTimerToFinalSecond(PlayState* play) {
    if (gSaveContext.subTimerState != SUBTIMER_STATE_OFF) {
        if (gSaveContext.eventInf[1] & 1) {
            gSaveContext.subTimerSeconds = 239;
        } else {
            gSaveContext.subTimerSeconds = 1;
        }
    }
}

void Interface_SetTimer(s16 seconds) {
    gSaveContext.timerX[TIMER_ID_MAIN] = 140;
    gSaveContext.timerY[TIMER_ID_MAIN] = 80;
    sEnvHazardActive = false;
    gSaveContext.timerSeconds = seconds;

    if (seconds) {
        gSaveContext.timerState = TIMER_STATE_DOWN_INIT;
    } else {
        gSaveContext.timerState = TIMER_STATE_UP_INIT;
    }
}

void Interface_DrawActionLabel(GraphicsContext* gfxCtx, void* texture) {
    OPEN_DISPS(gfxCtx);

    gDPLoadTextureBlock_4b(OVERLAY_DISP++, texture, G_IM_FMT_IA, DO_ACTION_TEX_WIDTH(), DO_ACTION_TEX_HEIGHT(), 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                           G_TX_NOLOD);

    gSP1Quadrangle(OVERLAY_DISP++, 0, 2, 3, 1, 0);

    CLOSE_DISPS(gfxCtx);
}

void Interface_DrawItemButtons(PlayState* play) {
    static void* cUpLabelTextures[] = { gNaviCUpENGTex, gNaviCUpENGTex, gNaviCUpENGTex, gNaviCUpJPTex };
    static s16 startButtonLeftPos[] = { 132, 130, 130, 132 };
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Player* player = GET_PLAYER(play);
    PauseContext* pauseCtx = &play->pauseCtx;
    s16 temp; // Used as both an alpha value and a button index
    s16 dxdy;
    s16 width;
    s16 height;
    s32 originalQuickItemModeEnabled = Interface_IsOriginalMultiplayerItemModeEnabled();

    Color_RGB8 bButtonColor = { 0, 150, 0 };
    if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.Changed"), 0)) {
        bButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.BButton.Value"), bButtonColor);
    } else if (CVarGetInteger(CVAR_COSMETIC("DefaultColorScheme"), COLORSCHEME_N64) == COLORSCHEME_GAMECUBE) {
        bButtonColor = (Color_RGB8){ 255, 30, 30 };
    }

    Color_RGB8 cButtonsColor = { 255, 160, 0 };
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CButtons.Changed"), 0)) {
        cButtonsColor = CVarGetColor24(CVAR_COSMETIC("HUD.CButtons.Value"), cButtonsColor);
    }
    Color_RGB8 cUpButtonColor = cButtonsColor;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.Changed"), 0)) {
        cUpButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CUpButton.Value"), cUpButtonColor);
    }
    Color_RGB8 cDownButtonColor = cButtonsColor;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.Changed"), 0)) {
        cDownButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CDownButton.Value"), cDownButtonColor);
    }
    Color_RGB8 cLeftButtonColor = cButtonsColor;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.Changed"), 0)) {
        cLeftButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CLeftButton.Value"), cLeftButtonColor);
    }
    Color_RGB8 cRightButtonColor = cButtonsColor;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.Changed"), 0)) {
        cRightButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CRightButton.Value"), cRightButtonColor);
    }

    Color_RGB8 startButtonColor = { 200, 0, 0 };
    if (CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.Changed"), 0)) {
        startButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.StartButton.Value"), startButtonColor);
    } else if (CVarGetInteger(CVAR_COSMETIC("DefaultColorScheme"), COLORSCHEME_N64) == COLORSCHEME_GAMECUBE) {
        startButtonColor = (Color_RGB8){ 120, 120, 120 };
    }

    // B Button
    s16 X_Margins_BtnB;
    s16 Y_Margins_BtnB;
    s16 BBtn_Size = 32;
    int BBtnScaled = BBtn_Size * 0.95f;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) != ORIGINAL_LOCATION) {
        BBtnScaled = BBtn_Size * CVarGetFloat(CVAR_COSMETIC("HUD.BButton.Scale"), 0.95f);
    }
    int BBtn_factor = (1 << 10) * BBtn_Size / BBtnScaled;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_BtnB = Right_HUD_Margin;
        };
        Y_Margins_BtnB = (Top_HUD_Margin * -1);
    } else {
        X_Margins_BtnB = 0;
        Y_Margins_BtnB = 0;
    }
    s16 PosX_BtnB_ori = OTRGetRectDimensionFromRightEdge(R_ITEM_BTN_X(0) + X_Margins_BtnB);
    s16 PosY_BtnB_ori = R_ITEM_BTN_Y(0) + Y_Margins_BtnB;
    s16 PosX_BtnB;
    s16 PosY_BtnB;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) != ORIGINAL_LOCATION) {
        PosY_BtnB = CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosY"), 0) + Y_Margins_BtnB;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
                X_Margins_BtnB = Left_HUD_Margin;
            };
            PosX_BtnB =
                OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosX"), 0) + X_Margins_BtnB);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
                X_Margins_BtnB = Right_HUD_Margin;
            };
            PosX_BtnB =
                OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosX"), 0) + X_Margins_BtnB);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ANCHOR_NONE) {
            PosX_BtnB = CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosX"), 0);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == HIDDEN) {
            PosX_BtnB = -9999;
        }
    } else {
        PosY_BtnB = PosY_BtnB_ori;
        PosX_BtnB = PosX_BtnB_ori;
    }

    if (originalQuickItemModeEnabled) {
        PosX_BtnB = -9999;
        PosY_BtnB = -9999;
    }

    // Start Button
    s16 X_Margins_StartBtn;
    s16 Y_Margins_StartBtn;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_StartBtn = Right_HUD_Margin;
        };
        Y_Margins_StartBtn = Top_HUD_Margin * -1;
    } else {
        X_Margins_StartBtn = 0;
        Y_Margins_StartBtn = 0;
    }
    s16 StartBtn_Icon_H = 32;
    s16 StartBtn_Icon_W = 32;
    float Start_BTN_Scale = 0.75f;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.PosType"), 0) != ORIGINAL_LOCATION) {
        Start_BTN_Scale = CVarGetFloat(CVAR_COSMETIC("HUD.StartButton.Scale"), 0.75f);
    }
    int StartBTN_H_Scaled = StartBtn_Icon_H * Start_BTN_Scale;
    int StartBTN_W_Scaled = StartBtn_Icon_W * Start_BTN_Scale;
    int StartBTN_W_factor = (1 << 10) * StartBtn_Icon_W / StartBTN_W_Scaled;
    int StartBTN_H_factor = (1 << 10) * StartBtn_Icon_H / StartBTN_H_Scaled;
    const s16 PosX_StartBtn_ori =
        OTRGetRectDimensionFromRightEdge(startButtonLeftPos[gSaveContext.language] + X_Margins_StartBtn);
    const s16 PosY_StartBtn_ori = 16 + Y_Margins_StartBtn;
    s16 StartBTN_Label_W = DO_ACTION_TEX_WIDTH();
    s16 StartBTN_Label_H = DO_ACTION_TEX_HEIGHT();
    s16 PosX_StartBtn;
    s16 PosY_StartBtn;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.PosType"), 0) != ORIGINAL_LOCATION) {
        PosY_StartBtn =
            CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.PosY"), 0) - (Start_BTN_Scale * 13) + Y_Margins_StartBtn;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.UseMargins"), 0) != 0) {
                X_Margins_StartBtn = Left_HUD_Margin;
            };
            PosX_StartBtn = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.PosX"), 0) -
                                                        (Start_BTN_Scale * 13) + X_Margins_StartBtn);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.UseMargins"), 0) != 0) {
                X_Margins_StartBtn = Right_HUD_Margin;
            };
            PosX_StartBtn = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.PosX"), 0) -
                                                         (Start_BTN_Scale * 13) + X_Margins_StartBtn);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.PosType"), 0) == ANCHOR_NONE) {
            PosX_StartBtn = CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.PosX"), 0);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.StartButton.PosType"), 0) == HIDDEN) {
            PosX_StartBtn = -9999;
        }
    } else {
        StartBTN_H_Scaled = StartBtn_Icon_H * 0.75f;
        StartBTN_W_Scaled = StartBtn_Icon_W * 0.75f;
        StartBTN_W_factor = (1 << 10) * StartBtn_Icon_W / StartBTN_W_Scaled;
        StartBTN_H_factor = (1 << 10) * StartBtn_Icon_H / StartBTN_H_Scaled;
        PosY_StartBtn = PosY_StartBtn_ori;
        PosX_StartBtn = PosX_StartBtn_ori;
    }
    // C Buttons position
    s16 X_Margins_CL;
    s16 X_Margins_CR;
    s16 X_Margins_CU;
    s16 X_Margins_CD;
    s16 Y_Margins_CL;
    s16 Y_Margins_CR;
    s16 Y_Margins_CU;
    s16 Y_Margins_CD;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CL = Right_HUD_Margin;
        };
        Y_Margins_CL = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CL = 0;
        Y_Margins_CL = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CR = Right_HUD_Margin;
        };
        Y_Margins_CR = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CR = 0;
        Y_Margins_CR = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CU = Right_HUD_Margin;
        };
        Y_Margins_CU = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CU = 0;
        Y_Margins_CU = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CD = Right_HUD_Margin;
        };
        Y_Margins_CD = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CD = 0;
        Y_Margins_CD = 0;
    }
    const s16 C_Left_BTN_Pos_ori[] = { C_LEFT_BUTTON_X + X_Margins_CL, C_LEFT_BUTTON_Y + Y_Margins_CL }; //(X,Y)
    const s16 C_Right_BTN_Pos_ori[] = { C_RIGHT_BUTTON_X + X_Margins_CR, C_RIGHT_BUTTON_Y + Y_Margins_CR };
    const s16 C_Up_BTN_Pos_ori[] = { C_UP_BUTTON_X + X_Margins_CU, C_UP_BUTTON_Y + Y_Margins_CU };
    const s16 C_Down_BTN_Pos_ori[] = { C_DOWN_BUTTON_X + X_Margins_CD, C_DOWN_BUTTON_Y + Y_Margins_CD };
    s16 LabelX_Navi = 7 + !!CVarGetInteger(CVAR_ENHANCEMENT("NaviTextFix"), 0);
    s16 LabelY_Navi = 4;
    s16 C_Left_BTN_Pos[2]; //(X,Y)
    s16 C_Right_BTN_Pos[2];
    s16 C_Up_BTN_Pos[2];
    s16 C_Down_BTN_Pos[2];
    // C button Left
    s16 C_Left_BTN_Size = 32;
    float CLeftScale = CVarGetFloat(CVAR_COSMETIC("HUD.CLeftButton.Scale"), 0.87f);
    int CLeftScaled = C_Left_BTN_Size * 0.87f;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) != ORIGINAL_LOCATION) {
        CLeftScaled = C_Left_BTN_Size * CLeftScale;
    }
    int CLeft_factor = (1 << 10) * C_Left_BTN_Size / CLeftScaled;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) != ORIGINAL_LOCATION) {
        C_Left_BTN_Pos[1] = CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosY"), 0) + Y_Margins_CL;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
                X_Margins_CL = Left_HUD_Margin;
            };
            C_Left_BTN_Pos[0] =
                OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0) + X_Margins_CL);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
                X_Margins_CL = Right_HUD_Margin;
            };
            C_Left_BTN_Pos[0] =
                OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0) + X_Margins_CL);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_NONE) {
            C_Left_BTN_Pos[0] = CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == HIDDEN) {
            C_Left_BTN_Pos[0] = -9999;
        }
    } else {
        C_Left_BTN_Pos[1] = C_Left_BTN_Pos_ori[1];
        C_Left_BTN_Pos[0] = OTRGetRectDimensionFromRightEdge(C_Left_BTN_Pos_ori[0]);
    }
    // C button Right
    s16 C_Right_BTN_Size = 32;
    float CRightScale = CVarGetFloat(CVAR_COSMETIC("HUD.CRightButton.Scale"), 0.87f);
    int CRightScaled = C_Right_BTN_Size * 0.87f;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) != ORIGINAL_LOCATION) {
        CRightScaled = C_Right_BTN_Size * CRightScale;
    }
    int CRight_factor = (1 << 10) * C_Right_BTN_Size / CRightScaled;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) != ORIGINAL_LOCATION) {
        C_Right_BTN_Pos[1] = CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosY"), 0) + Y_Margins_CR;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
                X_Margins_CR = Left_HUD_Margin;
            };
            C_Right_BTN_Pos[0] =
                OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0) + X_Margins_CR);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
                X_Margins_CR = Right_HUD_Margin;
            };
            C_Right_BTN_Pos[0] =
                OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0) + X_Margins_CR);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_NONE) {
            C_Right_BTN_Pos[0] = CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == HIDDEN) {
            C_Right_BTN_Pos[0] = -9999;
        }
    } else {
        C_Right_BTN_Pos[1] = C_Right_BTN_Pos_ori[1];
        C_Right_BTN_Pos[0] = OTRGetRectDimensionFromRightEdge(C_Right_BTN_Pos_ori[0]);
    }
    // C Button Up
    s16 C_Up_BTN_Size = 32;
    int CUpScaled = C_Up_BTN_Size * 0.5f;
    float CUpScale = CVarGetFloat(CVAR_COSMETIC("HUD.CUpButton.Scale"), 0.5f);
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.PosType"), 0) != ORIGINAL_LOCATION) {
        CUpScaled = C_Up_BTN_Size * CUpScale;
    }
    int CUp_factor = (1 << 10) * C_Up_BTN_Size / CUpScaled;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.PosType"), 0) != ORIGINAL_LOCATION) {
        C_Up_BTN_Pos[1] = CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.PosY"), 0) + Y_Margins_CU;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.UseMargins"), 0) != 0) {
                X_Margins_CU = Left_HUD_Margin;
            };
            C_Up_BTN_Pos[0] =
                OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.PosX"), 0) + X_Margins_CU);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.UseMargins"), 0) != 0) {
                X_Margins_CU = Right_HUD_Margin;
            };
            C_Up_BTN_Pos[0] =
                OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.PosX"), 0) + X_Margins_CU);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.PosType"), 0) == ANCHOR_NONE) {
            C_Up_BTN_Pos[0] = CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.PosX"), 0);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.PosType"), 0) == HIDDEN) {
            C_Up_BTN_Pos[0] = -9999;
        }
    } else {
        C_Up_BTN_Pos[1] = C_Up_BTN_Pos_ori[1];
        C_Up_BTN_Pos[0] = OTRGetRectDimensionFromRightEdge(C_Up_BTN_Pos_ori[0]);
    }
    // C Button down
    s16 C_Down_BTN_Size = 32;
    float CDownScale = CVarGetFloat(CVAR_COSMETIC("HUD.CDownButton.Scale"), 0.87f);
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ORIGINAL_LOCATION) {
        CDownScale = 0.87f;
    }
    int CDownScaled = C_Down_BTN_Size * CDownScale;
    int CDown_factor = (1 << 10) * C_Down_BTN_Size / CDownScaled;
    int PositionAdjustment = CDownScaled / 2;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) != ORIGINAL_LOCATION) {
        C_Down_BTN_Pos[1] = CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosY"), 0) + Y_Margins_CD;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
                X_Margins_CD = Left_HUD_Margin;
            };
            C_Down_BTN_Pos[0] =
                OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0) + X_Margins_CD);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
                X_Margins_CD = Right_HUD_Margin;
            };
            C_Down_BTN_Pos[0] =
                OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0) + X_Margins_CD);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_NONE) {
            C_Down_BTN_Pos[0] = CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == HIDDEN) {
            C_Down_BTN_Pos[0] = -9999;
        }
    } else {
        C_Down_BTN_Pos[1] = C_Down_BTN_Pos_ori[1];
        C_Down_BTN_Pos[0] = OTRGetRectDimensionFromRightEdge(C_Down_BTN_Pos_ori[0]);
    }

    OPEN_DISPS(play->state.gfxCtx);

    // B Button Color & Texture
    // Also loads the Item Button Texture reused by other buttons afterwards
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, bButtonColor.r, bButtonColor.g, bButtonColor.b, interfaceCtx->bAlpha);
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);

    OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gButtonBackgroundTex, BBtn_Size, BBtn_Size, PosX_BtnB, PosY_BtnB,
                                  BBtnScaled, BBtnScaled, BBtn_factor, BBtn_factor);

    if (!originalQuickItemModeEnabled) {
        // C-Left Button Color & Texture
        gDPPipeSync(OVERLAY_DISP++);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cLeftButtonColor.r, cLeftButtonColor.g, cLeftButtonColor.b,
                        interfaceCtx->cLeftAlpha);
        gSPWideTextureRectangle(OVERLAY_DISP++, C_Left_BTN_Pos[0] << 2, C_Left_BTN_Pos[1] << 2,
                                (C_Left_BTN_Pos[0] + R_ITEM_BTN_WIDTH(1)) << 2,
                                (C_Left_BTN_Pos[1] + R_ITEM_BTN_WIDTH(1)) << 2, G_TX_RENDERTILE, 0, 0,
                                R_ITEM_BTN_DD(1) << 1, R_ITEM_BTN_DD(1) << 1);

        // C-Down Button Color & Texture
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cDownButtonColor.r, cDownButtonColor.g, cDownButtonColor.b,
                        interfaceCtx->cDownAlpha);
        gSPWideTextureRectangle(OVERLAY_DISP++, C_Down_BTN_Pos[0] << 2, C_Down_BTN_Pos[1] << 2,
                                (C_Down_BTN_Pos[0] + R_ITEM_BTN_WIDTH(2)) << 2,
                                (C_Down_BTN_Pos[1] + R_ITEM_BTN_WIDTH(2)) << 2, G_TX_RENDERTILE, 0, 0,
                                R_ITEM_BTN_DD(2) << 1, R_ITEM_BTN_DD(2) << 1);

        // C-Right Button Color & Texture
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cRightButtonColor.r, cRightButtonColor.g, cRightButtonColor.b,
                        interfaceCtx->cRightAlpha);
        gSPWideTextureRectangle(OVERLAY_DISP++, C_Right_BTN_Pos[0] << 2, C_Right_BTN_Pos[1] << 2,
                                (C_Right_BTN_Pos[0] + R_ITEM_BTN_WIDTH(3)) << 2,
                                (C_Right_BTN_Pos[1] + R_ITEM_BTN_WIDTH(3)) << 2, G_TX_RENDERTILE, 0, 0,
                                R_ITEM_BTN_DD(3) << 1, R_ITEM_BTN_DD(3) << 1);
    }

    if ((pauseCtx->state < 8) || (pauseCtx->state >= 18)) {
        if ((play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0)) {
            // Start Button Texture, Color & Label
            gDPPipeSync(OVERLAY_DISP++);

            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, startButtonColor.r, startButtonColor.g, startButtonColor.b,
                            interfaceCtx->startAlpha);
            gSPWideTextureRectangle(OVERLAY_DISP++, PosX_StartBtn << 2, PosY_StartBtn << 2,
                                    (PosX_StartBtn + StartBTN_W_Scaled) << 2, (PosY_StartBtn + StartBTN_H_Scaled) << 2,
                                    G_TX_RENDERTILE, 0, 0, StartBTN_W_factor, StartBTN_H_factor);
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->startAlpha);
            gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);
            gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                              PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
            Interface_LoadActionLabel(interfaceCtx, 3, 2);
            gDPLoadTextureBlock_4b(OVERLAY_DISP++, interfaceCtx->doActionSegment[2], G_IM_FMT_IA, DO_ACTION_TEX_WIDTH(),
                                   DO_ACTION_TEX_HEIGHT(), 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

            gDPPipeSync(OVERLAY_DISP++);
            gSPSetGeometryMode(OVERLAY_DISP++, G_CULL_BACK);
            gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                              PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->startAlpha);
            gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);
            Matrix_Translate(PosX_StartBtn - 160 + ((Start_BTN_Scale + Start_BTN_Scale / 3) * 11.5f),
                             (PosY_StartBtn - 120 + ((Start_BTN_Scale + Start_BTN_Scale / 3) * 11.5f)) * -1, 1.0f,
                             MTXMODE_NEW);
            Matrix_Scale(Start_BTN_Scale + (Start_BTN_Scale / 3), Start_BTN_Scale + (Start_BTN_Scale / 3),
                         Start_BTN_Scale + (Start_BTN_Scale / 3), MTXMODE_APPLY);
            gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPVertex(OVERLAY_DISP++, &interfaceCtx->actionVtx[4], 4, 0);
            Interface_DrawActionLabel(play->state.gfxCtx, interfaceCtx->doActionSegment[2]);
            gDPPipeSync(OVERLAY_DISP++);
        }
    }

    if (!CVarGetInteger(CVAR_ENHANCEMENT("DisableNaviMessages"), 0) && interfaceCtx->naviCalling &&
        (play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) &&
        (play->csCtx.state == CS_STATE_IDLE)) {
        if (!sCUpInvisible) {
            // C-Up Button Texture, Color & Label (Navi Text)
            gDPPipeSync(OVERLAY_DISP++);

            if ((gSaveContext.unk_13EA == 1) || (gSaveContext.unk_13EA == 2) || (gSaveContext.unk_13EA == 5)) {
                temp = 0;
            } else if ((player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) ||
                       (Player_GetEnvironmentalHazard(play) == 4) || (player->stateFlags2 & PLAYER_STATE2_CRAWLING)) {
                temp = 70;
            } else {
                temp = interfaceCtx->healthAlpha;
            }

            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cUpButtonColor.r, cUpButtonColor.g, cUpButtonColor.b, temp);
            gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            gSPWideTextureRectangle(OVERLAY_DISP++, C_Up_BTN_Pos[0] << 2, C_Up_BTN_Pos[1] << 2,
                                    (C_Up_BTN_Pos[0] + 16) << 2, (C_Up_BTN_Pos[1] + 16) << 2, G_TX_RENDERTILE, 0, 0,
                                    2 << 10, 2 << 10);
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, temp);
            gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);
            gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                              PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);

            gDPLoadTextureBlock_4b(OVERLAY_DISP++, cUpLabelTextures[gSaveContext.language], G_IM_FMT_IA, 32, 8, 0,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                   G_TX_NOLOD, G_TX_NOLOD);

            gSPWideTextureRectangle(OVERLAY_DISP++, C_Up_BTN_Pos[0] - LabelX_Navi << 2,
                                    C_Up_BTN_Pos[1] + LabelY_Navi << 2, (C_Up_BTN_Pos[0] - LabelX_Navi + 32) << 2,
                                    (C_Up_BTN_Pos[1] + LabelY_Navi + 8) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        }

        sCUpTimer--;
        if (sCUpTimer == 0) {
            sCUpInvisible ^= 1;
            sCUpTimer = 10;
        }
    }

    if (!originalQuickItemModeEnabled) {
        gDPPipeSync(OVERLAY_DISP++);

        // Empty C Button Arrows
        for (temp = 1; temp < 4; temp++) {
            if (gSaveContext.equips.buttonItems[temp] > 0xF0) {
            s16 X_Margins_CL;
            s16 X_Margins_CR;
            s16 X_Margins_CD;
            s16 Y_Margins_CL;
            s16 Y_Margins_CR;
            s16 Y_Margins_CD;
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
                if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ORIGINAL_LOCATION) {
                    X_Margins_CL = Right_HUD_Margin;
                };
                Y_Margins_CL = (Top_HUD_Margin * -1);
            } else {
                X_Margins_CL = 0;
                Y_Margins_CL = 0;
            }
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
                if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ORIGINAL_LOCATION) {
                    X_Margins_CR = Right_HUD_Margin;
                };
                Y_Margins_CR = (Top_HUD_Margin * -1);
            } else {
                X_Margins_CR = 0;
                Y_Margins_CR = 0;
            }
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
                if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ORIGINAL_LOCATION) {
                    X_Margins_CD = Right_HUD_Margin;
                };
                Y_Margins_CD = (Top_HUD_Margin * -1);
            } else {
                X_Margins_CD = 0;
                Y_Margins_CD = 0;
            }
            const s16 ItemIconWidthFactor[3][2] = {
                { CLeftScaled, CLeft_factor },
                { CDownScaled, CDown_factor },
                { CRightScaled, CRight_factor },
            };
            const s16 ItemIconPos_ori[3][2] = {
                { R_ITEM_ICON_X(1) + X_Margins_CL, R_ITEM_ICON_Y(1) + Y_Margins_CL },
                { R_ITEM_ICON_X(2) + X_Margins_CD, R_ITEM_ICON_Y(2) + Y_Margins_CD },
                { R_ITEM_ICON_X(3) + X_Margins_CR, R_ITEM_ICON_Y(3) + Y_Margins_CR },
            };
            s16 ItemIconPos[3][2]; //(X,Y)
            // C button Left
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) != ORIGINAL_LOCATION) {
                ItemIconPos[0][1] = CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosY"), 0) + Y_Margins_CL;
                if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_LEFT) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
                        X_Margins_CL = Left_HUD_Margin;
                    };
                    ItemIconPos[0][0] = OTRGetDimensionFromLeftEdge(
                        CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0) + X_Margins_CL);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_RIGHT) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
                        X_Margins_CL = Right_HUD_Margin;
                    };
                    ItemIconPos[0][0] = OTRGetDimensionFromRightEdge(
                        CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0) + X_Margins_CL);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_NONE) {
                    ItemIconPos[0][0] = CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == HIDDEN) {
                    ItemIconPos[0][0] = -9999;
                }
            } else {
                ItemIconPos[0][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[0][0]);
                ItemIconPos[0][1] = ItemIconPos_ori[0][1];
            }
            // C Button down
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) != ORIGINAL_LOCATION) {
                ItemIconPos[1][1] = CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosY"), 0) + Y_Margins_CD;
                if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_LEFT) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
                        X_Margins_CD = Left_HUD_Margin;
                    };
                    ItemIconPos[1][0] = OTRGetDimensionFromLeftEdge(
                        CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0) + X_Margins_CD);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_RIGHT) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
                        X_Margins_CD = Right_HUD_Margin;
                    };
                    ItemIconPos[1][0] = OTRGetDimensionFromRightEdge(
                        CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0) + X_Margins_CD);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_NONE) {
                    ItemIconPos[1][0] = CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == HIDDEN) {
                    ItemIconPos[1][0] = -9999;
                }
            } else {
                ItemIconPos[1][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[1][0]);
                ItemIconPos[1][1] = ItemIconPos_ori[1][1];
            }
            // C button Right
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) != ORIGINAL_LOCATION) {
                ItemIconPos[2][1] = CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosY"), 0) + Y_Margins_CR;
                if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_LEFT) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
                        X_Margins_CR = Left_HUD_Margin;
                    };
                    ItemIconPos[2][0] = OTRGetDimensionFromLeftEdge(
                        CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0) + X_Margins_CR);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_RIGHT) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
                        X_Margins_CR = Right_HUD_Margin;
                    };
                    ItemIconPos[2][0] = OTRGetDimensionFromRightEdge(
                        CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0) + X_Margins_CR);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_NONE) {
                    ItemIconPos[2][0] = CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == HIDDEN) {
                    ItemIconPos[2][0] = -9999;
                }
            } else {
                ItemIconPos[2][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[2][0]);
                ItemIconPos[2][1] = ItemIconPos_ori[2][1];
            }

            if (temp == 1) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cLeftButtonColor.r, cLeftButtonColor.g, cLeftButtonColor.b,
                                interfaceCtx->cLeftAlpha);
            } else if (temp == 2) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cDownButtonColor.r, cDownButtonColor.g, cDownButtonColor.b,
                                interfaceCtx->cDownAlpha);
            } else {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cRightButtonColor.r, cRightButtonColor.g, cRightButtonColor.b,
                                interfaceCtx->cRightAlpha);
            }

            OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, ((u8*)gButtonBackgroundTex), 32, 32, ItemIconPos[temp - 1][0],
                                          ItemIconPos[temp - 1][1], ItemIconWidthFactor[temp - 1][0],
                                          ItemIconWidthFactor[temp - 1][0], ItemIconWidthFactor[temp - 1][1],
                                          ItemIconWidthFactor[temp - 1][1]);

            const char* cButtonIcons[] = { gButtonBackgroundTex, gEquippedItemOutlineTex, gEmptyCLeftArrowTex,
                                           gEmptyCDownArrowTex, gEmptyCRightArrowTex };

            OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, cButtonIcons[(temp + 1)], 32, 32, ItemIconPos[temp - 1][0],
                                          ItemIconPos[temp - 1][1], ItemIconWidthFactor[temp - 1][0],
                                          ItemIconWidthFactor[temp - 1][0], ItemIconWidthFactor[temp - 1][1],
                                          ItemIconWidthFactor[temp - 1][1]);
            }
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

int16_t gItemIconWidth[] = { 30, 24, 24, 24, 16, 16, 16, 16 };
int16_t gItemIconDD[] = { 550, 680, 680, 680, 1024, 1024, 1024, 1024 };

void Interface_DrawItemIconTexture(PlayState* play, void* texture, s16 button) {
    OPEN_DISPS(play->state.gfxCtx);
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 X_Margins_CL;
    s16 X_Margins_CR;
    s16 X_Margins_CD;
    s16 Y_Margins_CL;
    s16 Y_Margins_CR;
    s16 Y_Margins_CD;
    s16 X_Margins_BtnB;
    s16 Y_Margins_BtnB;
    s16 X_Margins_DPad_Items;
    s16 Y_Margins_DPad_Items;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_BtnB = Right_HUD_Margin;
        };
        Y_Margins_BtnB = (Top_HUD_Margin * -1);
    } else {
        X_Margins_BtnB = 0;
        Y_Margins_BtnB = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CL = Right_HUD_Margin;
        };
        Y_Margins_CL = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CL = 0;
        Y_Margins_CL = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CR = Right_HUD_Margin;
        };
        Y_Margins_CR = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CR = 0;
        Y_Margins_CR = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CD = Right_HUD_Margin;
        };
        Y_Margins_CD = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CD = 0;
        Y_Margins_CD = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_DPad_Items = Right_HUD_Margin;
        };
        Y_Margins_DPad_Items = (Top_HUD_Margin * -1);
    } else {
        X_Margins_DPad_Items = 0;
        Y_Margins_DPad_Items = 0;
    }
    const s16 ItemIconPos_ori[8][2] = { { B_BUTTON_X + X_Margins_BtnB, B_BUTTON_Y + Y_Margins_BtnB },
                                        { C_LEFT_BUTTON_X + X_Margins_CL, C_LEFT_BUTTON_Y + Y_Margins_CL },
                                        { C_DOWN_BUTTON_X + X_Margins_CD, C_DOWN_BUTTON_Y + Y_Margins_CD },
                                        { C_RIGHT_BUTTON_X + X_Margins_CR, C_RIGHT_BUTTON_Y + Y_Margins_CR },
                                        { DPAD_UP_X + X_Margins_DPad_Items, DPAD_UP_Y + Y_Margins_DPad_Items },
                                        { DPAD_DOWN_X + X_Margins_DPad_Items, DPAD_DOWN_Y + Y_Margins_DPad_Items },
                                        { DPAD_LEFT_X + X_Margins_DPad_Items, DPAD_LEFT_Y + Y_Margins_DPad_Items },
                                        { DPAD_RIGHT_X + X_Margins_DPad_Items, DPAD_RIGHT_Y + Y_Margins_DPad_Items } };
    u16 ItemsSlotsAlpha[8] = { interfaceCtx->bAlpha,        interfaceCtx->cLeftAlpha,    interfaceCtx->cRightAlpha,
                               interfaceCtx->cDownAlpha,    interfaceCtx->dpadUpAlpha,   interfaceCtx->dpadDownAlpha,
                               interfaceCtx->dpadLeftAlpha, interfaceCtx->dpadRightAlpha };
    s16 DPad_ItemsOffset[4][2] = {
        { 7, -8 },         // Up
        { 7, 24 },         // Down
        { -9, 8 },         // Left
        { 23, 8 },         // Right
    };                     //(X,Y) Used with custom position to place it properly.
    s16 ItemIconPos[8][2]; //(X,Y)
    // DPadItems
    if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) != ORIGINAL_LOCATION) {
        ItemIconPos[4][1] =
            CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_DPad_Items + DPad_ItemsOffset[0][1]; // Up
        ItemIconPos[5][1] =
            CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_DPad_Items + DPad_ItemsOffset[1][1]; // Down
        ItemIconPos[6][1] =
            CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_DPad_Items + DPad_ItemsOffset[2][1]; // Left
        ItemIconPos[7][1] =
            CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_DPad_Items + DPad_ItemsOffset[3][1]; // Right
        if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.UseMargins"), 0) != 0) {
                X_Margins_DPad_Items = Left_HUD_Margin;
            };
            ItemIconPos[4][0] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                            X_Margins_DPad_Items + DPad_ItemsOffset[0][0]);
            ItemIconPos[5][0] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                            X_Margins_DPad_Items + DPad_ItemsOffset[1][0]);
            ItemIconPos[6][0] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                            X_Margins_DPad_Items + DPad_ItemsOffset[2][0]);
            ItemIconPos[7][0] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                            X_Margins_DPad_Items + DPad_ItemsOffset[3][0]);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.UseMargins"), 0) != 0) {
                X_Margins_DPad_Items = Right_HUD_Margin;
            };
            ItemIconPos[4][0] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                             X_Margins_DPad_Items + DPad_ItemsOffset[0][0]);
            ItemIconPos[5][0] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                             X_Margins_DPad_Items + DPad_ItemsOffset[1][0]);
            ItemIconPos[6][0] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                             X_Margins_DPad_Items + DPad_ItemsOffset[2][0]);
            ItemIconPos[7][0] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                             X_Margins_DPad_Items + DPad_ItemsOffset[3][0]);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ANCHOR_NONE) {
            ItemIconPos[4][0] = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + DPad_ItemsOffset[0][0];
            ItemIconPos[5][0] = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + DPad_ItemsOffset[1][0];
            ItemIconPos[6][0] = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + DPad_ItemsOffset[2][0];
            ItemIconPos[7][0] = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + DPad_ItemsOffset[3][0];
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == HIDDEN) {
            ItemIconPos[4][0] = -9999;
            ItemIconPos[5][0] = -9999;
            ItemIconPos[6][0] = -9999;
            ItemIconPos[7][0] = -9999;
        }
    } else {
        ItemIconPos[4][0] = OTRGetDimensionFromRightEdge(ItemIconPos_ori[4][0]);
        ItemIconPos[5][0] = OTRGetDimensionFromRightEdge(ItemIconPos_ori[5][0]);
        ItemIconPos[6][0] = OTRGetDimensionFromRightEdge(ItemIconPos_ori[6][0]);
        ItemIconPos[7][0] = OTRGetDimensionFromRightEdge(ItemIconPos_ori[7][0]);
        ItemIconPos[4][1] = ItemIconPos_ori[4][1];
        ItemIconPos[5][1] = ItemIconPos_ori[5][1];
        ItemIconPos[6][1] = ItemIconPos_ori[6][1];
        ItemIconPos[7][1] = ItemIconPos_ori[7][1];
    }
    // B Button
    if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) != ORIGINAL_LOCATION) {
        ItemIconPos[0][1] = CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosY"), 0) + Y_Margins_BtnB;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
                X_Margins_BtnB = Left_HUD_Margin;
            };
            ItemIconPos[0][0] =
                OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosX"), 0) + X_Margins_BtnB);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
                X_Margins_BtnB = Right_HUD_Margin;
            };
            ItemIconPos[0][0] =
                OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosX"), 0) + X_Margins_BtnB);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ANCHOR_NONE) {
            ItemIconPos[0][0] = CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosX"), 0);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == HIDDEN) {
            ItemIconPos[0][0] = -9999;
        }
    } else {
        ItemIconPos[0][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[0][0]);
        ItemIconPos[0][1] = ItemIconPos_ori[0][1];
    }
    // C button Left
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) != ORIGINAL_LOCATION) {
        ItemIconPos[1][1] = CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosY"), 0) + Y_Margins_CL;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
                X_Margins_CL = Left_HUD_Margin;
            };
            ItemIconPos[1][0] =
                OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0) + X_Margins_CL);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
                X_Margins_CL = Right_HUD_Margin;
            };
            ItemIconPos[1][0] =
                OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0) + X_Margins_CL);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_NONE) {
            ItemIconPos[1][0] = CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == HIDDEN) {
            ItemIconPos[1][0] = -9999;
        }
    } else {
        ItemIconPos[1][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[1][0]);
        ItemIconPos[1][1] = ItemIconPos_ori[1][1];
    }
    // C Button down
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) != ORIGINAL_LOCATION) {
        ItemIconPos[2][1] = CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosY"), 0) + Y_Margins_CD;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
                X_Margins_CD = Left_HUD_Margin;
            };
            ItemIconPos[2][0] =
                OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0) + X_Margins_CD);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
                X_Margins_CD = Right_HUD_Margin;
            };
            ItemIconPos[2][0] =
                OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0) + X_Margins_CD);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_NONE) {
            ItemIconPos[2][0] = CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == HIDDEN) {
            ItemIconPos[2][0] = -9999;
        }
    } else {
        ItemIconPos[2][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[2][0]);
        ItemIconPos[2][1] = ItemIconPos_ori[2][1];
    }
    // C button Right
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) != ORIGINAL_LOCATION) {
        ItemIconPos[3][1] = CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosY"), 0) + Y_Margins_CR;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
                X_Margins_CR = Left_HUD_Margin;
            };
            ItemIconPos[3][0] =
                OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0) + X_Margins_CR);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
                X_Margins_CR = Right_HUD_Margin;
            };
            ItemIconPos[3][0] =
                OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0) + X_Margins_CR);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_NONE) {
            ItemIconPos[3][0] = CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == HIDDEN) {
            ItemIconPos[3][0] = -9999;
        }
    } else {
        ItemIconPos[3][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[3][0]);
        ItemIconPos[3][1] = ItemIconPos_ori[3][1];
    }

    gDPLoadTextureBlock(OVERLAY_DISP++, texture, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    gSPWideTextureRectangle(OVERLAY_DISP++, ItemIconPos[button][0] << 2, ItemIconPos[button][1] << 2,
                            (ItemIconPos[button][0] + gItemIconWidth[button]) << 2,
                            (ItemIconPos[button][1] + gItemIconWidth[button]) << 2, G_TX_RENDERTILE, 0, 0,
                            gItemIconDD[button] << 1, gItemIconDD[button] << 1);

    CLOSE_DISPS(play->state.gfxCtx);
}

const char* _gAmmoDigit0Tex[] = { gAmmoDigit0Tex, gAmmoDigit1Tex, gAmmoDigit2Tex,         gAmmoDigit3Tex,
                                  gAmmoDigit4Tex, gAmmoDigit5Tex, gAmmoDigit6Tex,         gAmmoDigit7Tex,
                                  gAmmoDigit8Tex, gAmmoDigit9Tex, gUnusedAmmoDigitHalfTex };

static int16_t gItemAmmoX[] = { B_BUTTON_X + 2, C_LEFT_BUTTON_X + 1, C_DOWN_BUTTON_X + 1, C_RIGHT_BUTTON_X + 1,
                                DPAD_UP_X,      DPAD_DOWN_X,         DPAD_LEFT_X,         DPAD_RIGHT_X };
static int16_t gItemAmmoY[] = { B_BUTTON_Y + 18, C_LEFT_BUTTON_Y + 17, C_DOWN_BUTTON_Y + 17, C_RIGHT_BUTTON_Y + 17,
                                DPAD_UP_Y + 11,  DPAD_DOWN_Y + 11,     DPAD_LEFT_Y + 11,     DPAD_RIGHT_Y + 11 };

void Interface_DrawAmmoCount(PlayState* play, s16 button, s16 alpha) {
    s16 i;
    s16 ammo;
    s16 X_Margins_CL;
    s16 X_Margins_CR;
    s16 X_Margins_CD;
    s16 Y_Margins_CL;
    s16 Y_Margins_CR;
    s16 Y_Margins_CD;
    s16 X_Margins_BtnB;
    s16 Y_Margins_BtnB;
    s16 X_Margins_DPad_Items;
    s16 Y_Margins_DPad_Items;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_BtnB = Right_HUD_Margin;
        };
        Y_Margins_BtnB = (Top_HUD_Margin * -1);
    } else {
        X_Margins_BtnB = 0;
        Y_Margins_BtnB = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CL = Right_HUD_Margin;
        };
        Y_Margins_CL = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CL = 0;
        Y_Margins_CL = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CR = Right_HUD_Margin;
        };
        Y_Margins_CR = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CR = 0;
        Y_Margins_CR = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CD = Right_HUD_Margin;
        };
        Y_Margins_CD = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CD = 0;
        Y_Margins_CD = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_DPad_Items = Right_HUD_Margin;
        };
        Y_Margins_DPad_Items = (Top_HUD_Margin * -1);
    } else {
        X_Margins_DPad_Items = 0;
        Y_Margins_DPad_Items = 0;
    }
    const s16 ItemIconPos_ori[8][2] = {
        { R_ITEM_AMMO_X(0) + X_Margins_BtnB, R_ITEM_AMMO_Y(0) + Y_Margins_BtnB }, // Bow on Epona?
        { R_ITEM_AMMO_X(1) + X_Margins_CL, R_ITEM_AMMO_Y(1) + Y_Margins_CL },
        { R_ITEM_AMMO_X(2) + X_Margins_CD, R_ITEM_AMMO_Y(2) + Y_Margins_CD },
        { R_ITEM_AMMO_X(3) + X_Margins_CR, R_ITEM_AMMO_Y(3) + Y_Margins_CR },
        { DPAD_UP_X + X_Margins_DPad_Items, DPAD_UP_Y + 11 + Y_Margins_DPad_Items },
        { DPAD_DOWN_X + X_Margins_DPad_Items, DPAD_DOWN_Y + 11 + Y_Margins_DPad_Items },
        { DPAD_LEFT_X + X_Margins_DPad_Items, DPAD_LEFT_Y + 11 + Y_Margins_DPad_Items },
        { DPAD_RIGHT_X + X_Margins_DPad_Items, DPAD_RIGHT_Y + 11 + Y_Margins_DPad_Items }
    };
    s16 ItemIconPos[8][2]; //(X,Y)
    s16 DPad_ItemsOffset[4][2] = {
        { 7, 3 },   // Up
        { 7, 35 },  // Down
        { -9, 19 }, // Left
        { 23, 19 }, // Right
    };              //(X,Y) Used with custom position to place it properly.
    // DPadItems
    if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) != ORIGINAL_LOCATION) {
        ItemIconPos[4][1] =
            CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_DPad_Items + DPad_ItemsOffset[0][1]; // Up
        ItemIconPos[5][1] =
            CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_DPad_Items + DPad_ItemsOffset[1][1]; // Down
        ItemIconPos[6][1] =
            CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_DPad_Items + DPad_ItemsOffset[2][1]; // Left
        ItemIconPos[7][1] =
            CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_DPad_Items + DPad_ItemsOffset[3][1]; // Right
        if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.UseMargins"), 0) != 0) {
                X_Margins_DPad_Items = Left_HUD_Margin;
            };
            ItemIconPos[4][0] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                            X_Margins_DPad_Items + DPad_ItemsOffset[0][0]);
            ItemIconPos[5][0] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                            X_Margins_DPad_Items + DPad_ItemsOffset[1][0]);
            ItemIconPos[6][0] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                            X_Margins_DPad_Items + DPad_ItemsOffset[2][0]);
            ItemIconPos[7][0] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                            X_Margins_DPad_Items + DPad_ItemsOffset[3][0]);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.UseMargins"), 0) != 0) {
                X_Margins_DPad_Items = Right_HUD_Margin;
            };
            ItemIconPos[4][0] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                             X_Margins_DPad_Items + DPad_ItemsOffset[0][0]);
            ItemIconPos[5][0] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                             X_Margins_DPad_Items + DPad_ItemsOffset[1][0]);
            ItemIconPos[6][0] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                             X_Margins_DPad_Items + DPad_ItemsOffset[2][0]);
            ItemIconPos[7][0] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                             X_Margins_DPad_Items + DPad_ItemsOffset[3][0]);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ANCHOR_NONE) {
            ItemIconPos[4][0] = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + DPad_ItemsOffset[0][0];
            ItemIconPos[5][0] = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + DPad_ItemsOffset[1][0];
            ItemIconPos[6][0] = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + DPad_ItemsOffset[2][0];
            ItemIconPos[7][0] = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + DPad_ItemsOffset[3][0];
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == HIDDEN) {
            ItemIconPos[4][0] = -9999;
            ItemIconPos[5][0] = -9999;
            ItemIconPos[6][0] = -9999;
            ItemIconPos[7][0] = -9999;
        }
    } else {
        ItemIconPos[4][0] = OTRGetDimensionFromRightEdge(ItemIconPos_ori[4][0]);
        ItemIconPos[5][0] = OTRGetDimensionFromRightEdge(ItemIconPos_ori[5][0]);
        ItemIconPos[6][0] = OTRGetDimensionFromRightEdge(ItemIconPos_ori[6][0]);
        ItemIconPos[7][0] = OTRGetDimensionFromRightEdge(ItemIconPos_ori[7][0]);
        ItemIconPos[4][1] = ItemIconPos_ori[4][1];
        ItemIconPos[5][1] = ItemIconPos_ori[5][1];
        ItemIconPos[6][1] = ItemIconPos_ori[6][1];
        ItemIconPos[7][1] = ItemIconPos_ori[7][1];
    }
    // B Button
    s16 PosX_adjust = 1;
    s16 PosY_adjust = 17;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) != ORIGINAL_LOCATION) {
        ItemIconPos[0][1] = CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosY"), 0) + Y_Margins_BtnB + PosY_adjust;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
                X_Margins_BtnB = Left_HUD_Margin;
            };
            ItemIconPos[0][0] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosX"), 0) +
                                                            X_Margins_BtnB + PosX_adjust);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
                X_Margins_BtnB = Right_HUD_Margin;
            };
            ItemIconPos[0][0] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosX"), 0) +
                                                             X_Margins_BtnB + PosX_adjust);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ANCHOR_NONE) {
            ItemIconPos[0][0] = CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosX"), 0) + PosX_adjust;
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == HIDDEN) {
            ItemIconPos[0][0] = -9999;
        }
    } else {
        ItemIconPos[0][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[0][0]);
        ItemIconPos[0][1] = ItemIconPos_ori[0][1];
    }
    // C button Left
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) != ORIGINAL_LOCATION) {
        ItemIconPos[1][1] = CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosY"), 0) + Y_Margins_CL + PosY_adjust;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
                X_Margins_CL = Left_HUD_Margin;
            };
            ItemIconPos[1][0] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0) +
                                                            X_Margins_CL + PosX_adjust);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
                X_Margins_CL = Right_HUD_Margin;
            };
            ItemIconPos[1][0] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0) +
                                                             X_Margins_CL + PosX_adjust);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_NONE) {
            ItemIconPos[1][0] = CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0) + PosX_adjust;
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == HIDDEN) {
            ItemIconPos[1][0] = -9999;
        }
    } else {
        ItemIconPos[1][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[1][0]);
        ItemIconPos[1][1] = ItemIconPos_ori[1][1];
    }
    // C Button down
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) != ORIGINAL_LOCATION) {
        ItemIconPos[2][1] = CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosY"), 0) + Y_Margins_CD + PosY_adjust;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
                X_Margins_CD = Left_HUD_Margin;
            };
            ItemIconPos[2][0] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0) +
                                                            X_Margins_CD + PosX_adjust);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
                X_Margins_CD = Right_HUD_Margin;
            };
            ItemIconPos[2][0] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0) +
                                                             X_Margins_CD + PosX_adjust);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_NONE) {
            ItemIconPos[2][0] = CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0) + PosX_adjust;
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == HIDDEN) {
            ItemIconPos[2][0] = -9999;
        }
    } else {
        ItemIconPos[2][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[2][0]);
        ItemIconPos[2][1] = ItemIconPos_ori[2][1];
    }
    // C button Right
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) != ORIGINAL_LOCATION) {
        ItemIconPos[3][1] = CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosY"), 0) + Y_Margins_CR + PosY_adjust;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
                X_Margins_CR = Left_HUD_Margin;
            };
            ItemIconPos[3][0] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0) +
                                                            X_Margins_CR + PosX_adjust);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
                X_Margins_CR = Right_HUD_Margin;
            };
            ItemIconPos[3][0] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0) +
                                                             X_Margins_CR + PosX_adjust);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_NONE) {
            ItemIconPos[3][0] = CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0) + PosX_adjust;
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == HIDDEN) {
            ItemIconPos[3][0] = -9999;
        }
    } else {
        ItemIconPos[3][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[3][0]);
        ItemIconPos[3][1] = ItemIconPos_ori[3][1];
    }

    OPEN_DISPS(play->state.gfxCtx);

    i = gSaveContext.equips.buttonItems[button];

    if (GameInteractor_Should(VB_DRAW_AMMO_COUNT,
                              ((i == ITEM_STICK) || (i == ITEM_NUT) || (i == ITEM_BOMB) || (i == ITEM_BOW) ||
                               ((i >= ITEM_BOW_ARROW_FIRE) && (i <= ITEM_BOW_ARROW_LIGHT)) || (i == ITEM_SLINGSHOT) ||
                               (i == ITEM_BOMBCHU) || (i == ITEM_BEAN)),
                              &i)) {
        if ((i >= ITEM_BOW_ARROW_FIRE) && (i <= ITEM_BOW_ARROW_LIGHT)) {
            i = ITEM_BOW;
        }

        ammo = AMMO(i);

        gDPPipeSync(OVERLAY_DISP++);

        if ((button == 0) && (gSaveContext.minigameState == 1)) {
            ammo = play->interfaceCtx.hbaAmmo;
        } else if ((button == 0) && (play->shootingGalleryStatus > 1)) {
            ammo = play->shootingGalleryStatus - 1;
        } else if ((button == 0) && (play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY) && Flags_GetSwitch(play, 0x38)) {
            ammo = play->bombchuBowlingStatus;
            if (ammo < 0) {
                ammo = 0;
            }
        } else if (((i == ITEM_BOW) && (AMMO(i) == CUR_CAPACITY(UPG_QUIVER))) ||
                   ((i == ITEM_BOMB) && (AMMO(i) == CUR_CAPACITY(UPG_BOMB_BAG))) ||
                   ((i == ITEM_SLINGSHOT) && (AMMO(i) == CUR_CAPACITY(UPG_BULLET_BAG))) ||
                   ((i == ITEM_STICK) && (AMMO(i) == CUR_CAPACITY(UPG_STICKS))) ||
                   ((i == ITEM_NUT) && (AMMO(i) == CUR_CAPACITY(UPG_NUTS))) || ((i == ITEM_BOMBCHU) && (ammo == 50)) ||
                   ((i == ITEM_BEAN) && (ammo == 15)) || GameInteractor_Should(VB_COLOR_AMMO_GREEN, false, i)) {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 120, 255, 0, alpha);
        }

        if (ammo == 0) {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 100, 100, 100, alpha);
        }

        for (i = 0; ammo >= 10; i++) {
            ammo -= 10;
        }

        if (i != 0) {
            OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, (u8*)_gAmmoDigit0Tex[i], 8, 8, ItemIconPos[button][0],
                                          ItemIconPos[button][1], 8, 8, 1 << 10, 1 << 10);
        }

        OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, (u8*)_gAmmoDigit0Tex[ammo], 8, 8, ItemIconPos[button][0] + 6,
                                      ItemIconPos[button][1], 8, 8, 1 << 10, 1 << 10);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static void Interface_DrawItemIconTextureAt(PlayState* play, void* texture, s16 x, s16 y, s16 width, s16 height) {
    u16 dsdx = (32 << 10) / width;
    u16 dtdy = (32 << 10) / height;

    OPEN_DISPS(play->state.gfxCtx);

    gDPLoadTextureBlock(OVERLAY_DISP++, texture, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    gSPWideTextureRectangle(OVERLAY_DISP++, x << 2, y << 2, (x + width) << 2, (y + height) << 2, G_TX_RENDERTILE,
                            0, 0, dsdx, dtdy);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void Interface_DrawAmmoCountForItemAt(PlayState* play, s16 item, s16 alpha, s16 x, s16 y) {
    s16 tens;
    s16 ammo;

    OPEN_DISPS(play->state.gfxCtx);

    if (GameInteractor_Should(VB_DRAW_AMMO_COUNT,
                              ((item == ITEM_STICK) || (item == ITEM_NUT) || (item == ITEM_BOMB) ||
                               (item == ITEM_BOW) || ((item >= ITEM_BOW_ARROW_FIRE) && (item <= ITEM_BOW_ARROW_LIGHT)) ||
                               (item == ITEM_SLINGSHOT) || (item == ITEM_BOMBCHU) || (item == ITEM_BEAN)),
                              &item)) {
        if ((item >= ITEM_BOW_ARROW_FIRE) && (item <= ITEM_BOW_ARROW_LIGHT)) {
            item = ITEM_BOW;
        }

        ammo = AMMO(item);

        gDPPipeSync(OVERLAY_DISP++);

        if (((item == ITEM_BOW) && (ammo == CUR_CAPACITY(UPG_QUIVER))) ||
            ((item == ITEM_BOMB) && (ammo == CUR_CAPACITY(UPG_BOMB_BAG))) ||
            ((item == ITEM_SLINGSHOT) && (ammo == CUR_CAPACITY(UPG_BULLET_BAG))) ||
            ((item == ITEM_STICK) && (ammo == CUR_CAPACITY(UPG_STICKS))) ||
            ((item == ITEM_NUT) && (ammo == CUR_CAPACITY(UPG_NUTS))) || ((item == ITEM_BOMBCHU) && (ammo == 50)) ||
            ((item == ITEM_BEAN) && (ammo == 15)) || GameInteractor_Should(VB_COLOR_AMMO_GREEN, false, item)) {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 120, 255, 0, alpha);
        }

        if (ammo == 0) {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 100, 100, 100, alpha);
        }

        for (tens = 0; ammo >= 10; tens++) {
            ammo -= 10;
        }

        if (tens != 0) {
            OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, (u8*)_gAmmoDigit0Tex[tens], 8, 8, x, y, 8, 8, 1 << 10,
                                          1 << 10);
        }

        OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, (u8*)_gAmmoDigit0Tex[ammo], 8, 8, x + 6, y, 8, 8, 1 << 10,
                                      1 << 10);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static s16 Interface_GetQuickItemAlpha(InterfaceContext* interfaceCtx, u8 controllerPort, s16 slotIndex) {
    (void)slotIndex;

    switch (Interface_GetQuickItemButtonIndexImpl(controllerPort)) {
        case 1:
            return interfaceCtx->cLeftAlpha;

        case 2:
            return interfaceCtx->cDownAlpha;

        case 3:
            return interfaceCtx->cRightAlpha;

        case 4:
        default:
            return interfaceCtx->dpadUpAlpha;
    }
}

static void Interface_DrawQuickItemHudAt(PlayState* play, u8 controllerPort, s16 slotIndex, s16 x, s16 y) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    u8 item = Interface_GetQuickItemForPortAndSlot(controllerPort, slotIndex);
    s16 alpha = Interface_GetQuickItemAlpha(interfaceCtx, controllerPort, slotIndex);

    if ((item == ITEM_NONE) || (item >= ITEM_NONE_FE)) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, alpha);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
    CLOSE_DISPS(play->state.gfxCtx);

    Interface_DrawItemIconTextureAt(play, gItemIcons[item], x, y, 24, 24);
    Interface_DrawAmmoCountForItemAt(play, item, alpha, x + 1, y + 13);
}

static void Interface_DrawRadialQuickMenuHud(PlayState* play, s16 playerCount) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s32 splitScreenEnabled = Interface_IsSplitScreenEnabled();
    const s16 iconSize = 24;
    const s16 bgSize = 18;
    const s16 bgOffset = (iconSize - bgSize) / 2;
    const s16 ringOuterSize = 112;
    const s16 ringInnerSize = 84;
    const s16 ringRadius = 60;
    const s16 ringOuterHalf = ringOuterSize / 2;
    const s16 ringInnerHalf = ringInnerSize / 2;
    Color_RGB8 wheelColor = { 255, 255, 255 };
    Color_RGB8 centerColor = { 255, 255, 255 };
    Color_RGB8 itemButtonColor = { 255, 160, 0 };
    Color_RGB8 selectedItemButtonColor = { 255, 255, 170 };
    u8 wheelAlpha = interfaceCtx->bAlpha / 3;
    u8 centerAlpha = interfaceCtx->bAlpha / 5;
    u8 slotAlpha = interfaceCtx->bAlpha / 2;
    u8 selectedSlotAlpha = (interfaceCtx->bAlpha * 3) / 4;
    s32 i;

    if (wheelAlpha < 35) {
        wheelAlpha = 35;
    }

    if (centerAlpha < 20) {
        centerAlpha = 20;
    }

    if (slotAlpha < 60) {
        slotAlpha = 60;
    }

    if (selectedSlotAlpha < 90) {
        selectedSlotAlpha = 90;
    }

    if (!Interface_IsRadialQuickItemMenuEnabled()) {
        return;
    }

    for (i = 0; i < playerCount; i++) {
        LocalMPRadialQuickMenuState* state = &sRadialQuickMenuStates[i];
        s16 controllerPort = i + 1;
        s16 centerX;
        s16 centerY;
        s16 j;
        Viewport viewport;

        if (!state->active || (state->itemCount == 0)) {
            continue;
        }

        if (splitScreenEnabled) {
            Interface_GetSplitViewportBounds(playerCount, i, &viewport);
            centerX = (viewport.leftX + viewport.rightX) / 2;
            centerY = (viewport.topY + viewport.bottomY) / 2;
        } else {
            centerX = SCREEN_WIDTH / 2;
            centerY = SCREEN_HEIGHT / 2;
        }

        OPEN_DISPS(play->state.gfxCtx);
        gDPPipeSync(OVERLAY_DISP++);
        gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);

        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, wheelColor.r, wheelColor.g, wheelColor.b, wheelAlpha);
        OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gButtonBackgroundTex, 32, 32, centerX - ringOuterHalf,
                                      centerY - ringOuterHalf, ringOuterSize, ringOuterSize,
                                      (32 << 10) / ringOuterSize, (32 << 10) / ringOuterSize);

        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, centerColor.r, centerColor.g, centerColor.b, centerAlpha);
        OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gButtonBackgroundTex, 32, 32, centerX - ringInnerHalf,
                                      centerY - ringInnerHalf, ringInnerSize, ringInnerSize,
                                      (32 << 10) / ringInnerSize, (32 << 10) / ringInnerSize);
        CLOSE_DISPS(play->state.gfxCtx);

        Interface_DrawQuickItemHudAt(play, controllerPort, state->slotIndex, centerX - (iconSize / 2),
                                     centerY - (iconSize / 2));

        for (j = 0; j < state->itemCount; j++) {
            u16 angle = (u16)((0x10000 / state->itemCount) * j);
            s16 itemX = centerX + (s16)(Math_SinS(angle) * ringRadius) - (iconSize / 2);
            s16 itemY = centerY - (s16)(Math_CosS(angle) * ringRadius) - (iconSize / 2);

            OPEN_DISPS(play->state.gfxCtx);
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);

            if (j == state->selectedIndex) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, selectedItemButtonColor.r, selectedItemButtonColor.g,
                                selectedItemButtonColor.b, selectedSlotAlpha);
            } else {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, itemButtonColor.r, itemButtonColor.g, itemButtonColor.b,
                                slotAlpha);
            }

            OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gButtonBackgroundTex, 32, 32, itemX + bgOffset,
                                          itemY + bgOffset, bgSize, bgSize, (32 << 10) / bgSize,
                                          (32 << 10) / bgSize);
            CLOSE_DISPS(play->state.gfxCtx);

            Interface_DrawItemIconTextureAt(play, gItemIcons[state->items[j]], itemX, itemY, iconSize, iconSize);
            Interface_DrawAmmoCountForItemAt(play, state->items[j], interfaceCtx->bAlpha, itemX + 1, itemY + 13);
        }
    }
}

static s16 Interface_GetHealthCapacityForPort(u8 controllerPort) {
    s16 healthCapacity = gSaveContext.healthCapacity;

    if (controllerPort == 4) {
        healthCapacity = gSaveContext.healthCapacity4;
    } else if (controllerPort == 3) {
        healthCapacity = gSaveContext.healthCapacity3;
    } else if (controllerPort == 2) {
        healthCapacity = gSaveContext.healthCapacity2;
    }

    if ((controllerPort >= 2) && (healthCapacity < STARTING_HEALTH)) {
        healthCapacity = gSaveContext.healthCapacity;
        if (healthCapacity < STARTING_HEALTH) {
            healthCapacity = STARTING_HEALTH;
        }
    }

    return healthCapacity;
}

static s16 Interface_GetHeartRowCountForPort(u8 controllerPort) {
    s16 heartCount = Interface_GetHealthCapacityForPort(controllerPort) / FULL_HEART_HEALTH;

    if (heartCount <= 0) {
        heartCount = STARTING_HEALTH / FULL_HEART_HEALTH;
    }

    return (heartCount + 9) / 10;
}

static void Interface_DrawOriginalMultiplayerItemHud(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 playerCount;
    s16 playerIconY[4];
    s16 playerItemSlot2X[4];
    s16 playerItemSlot2Y[4];
    s16 playerMagicX[4];
    s16 playerMagicY[4];
    u8 playerDrawMagic[4];
    s16 playerSwordX[4];
    s16 playerItemX[4];
    s16 playerHeartRows[4];
    f32 heartsScale = 0.68f;
    f32 topHeartsAnchorY;
    f32 topHeartsCenterY;
    f32 bottomHeartsAnchorY;
    f32 splitViewportScaleX;
    s32 splitScreenEnabled;
    s32 secondSlotEnabled;
    s32 sideBySideQuickSlotLayout;
    const f32 topIconGap = 3.0f;
    const f32 bottomIconGap = 1.0f;
    const f32 sharedTopIconGap = 1.0f;
    const f32 sharedBottomIconGap = 0.0f;
    const s16 magicHeight = 16;
    const s16 magicGap = 3;
    const s16 sharedMagicGap = 1;
    const s16 sharedMagicOffset = 2; /* small Y offset to nudge shared-screen magic bars closer */
    const s16 iconSize = 24;
    const s16 bgSize = 18; /* smaller background circle size */
    const s16 buttonGap = 1;
    const s16 buttonStep = bgSize + buttonGap;
    const s16 bgOffset = (iconSize - bgSize) / 2;
    s16 i;
    s16 selectedSlot;
    const s16 leftSwordX = OTRGetRectDimensionFromLeftEdge(22.0f);
    const s16 leftItemX = leftSwordX + buttonStep;
    const s16 rightSwordX = SCREEN_WIDTH - iconSize - leftSwordX;
    const s16 rightItemX = SCREEN_WIDTH - iconSize - leftItemX;
    Color_RGB8 swordButtonColor = { 0, 150, 0 };
    Color_RGB8 itemButtonColor = { 255, 160, 0 };
    Color_RGB8 selectedItemButtonColor = { 255, 255, 150 };

    if (!Interface_IsOriginalMultiplayerItemModeEnabled()) {
        return;
    }

    playerCount = Interface_GetLocalMultiplayerPlayerCount();
    if (playerCount <= 1) {
        return;
    }

    secondSlotEnabled = Interface_IsSecondQuickItemSlotEnabled();
    sideBySideQuickSlotLayout = Interface_IsSecondQuickItemSlotSideBySideEnabled();

    if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) != ORIGINAL_LOCATION) {
        heartsScale = CVarGetFloat(CVAR_COSMETIC("HUD.HeartsCount.Scale"), 0.7f);
    }

    splitScreenEnabled = Interface_IsSplitScreenEnabled();
    splitViewportScaleX =
        (OTRGetDimensionFromRightEdge(SCREEN_WIDTH) - OTRGetDimensionFromLeftEdge(0.0f)) / SCREEN_WIDTH;

    topHeartsAnchorY = getHealthMeterYOffset() + 21.0f - (8.0f * heartsScale);
    topHeartsCenterY = topHeartsAnchorY + (8.0f * heartsScale);
    bottomHeartsAnchorY = SCREEN_HEIGHT - topHeartsCenterY;

    for (i = 0; i < ARRAY_COUNT(playerDrawMagic); i++) {
        playerDrawMagic[i] = false;
        playerMagicX[i] = 0;
        playerMagicY[i] = 0;
    }

    if (splitScreenEnabled) {
        for (i = 0; i < 4; i++) {
            s16 viewportLeftX;
            s16 viewportTopY;
            s16 viewportLeftHudSpace;
            s16 magicCapacity;
            s16 magicWidth;
            f32 heartsTopY;
            f32 heartsBottomY;

            Interface_GetSplitViewportTopLeft(playerCount, i, &viewportLeftX, &viewportTopY);
            viewportLeftHudSpace = (s16)(viewportLeftX * splitViewportScaleX);

            playerHeartRows[i] = Interface_GetHeartRowCountForPort(i + 1);
            heartsTopY = viewportTopY + topHeartsAnchorY;
            heartsBottomY = heartsTopY + ((playerHeartRows[i] - 1) * 10.0f) + (16.0f * heartsScale);
            playerIconY[i] = (s16)(heartsBottomY + topIconGap + 0.999f);
            playerSwordX[i] = viewportLeftHudSpace + leftSwordX;
            playerItemX[i] = viewportLeftHudSpace + leftItemX;
            playerItemSlot2X[i] = playerItemX[i] + (sideBySideQuickSlotLayout ? buttonStep : 0);

            if ((i > 0) && (i < playerCount) && (Magic_GetLevelForPort(i + 1) != 0)) {
                magicCapacity = Magic_GetMeterCapacityForPort(i + 1);
                if (magicCapacity <= 0) {
                    magicCapacity = Magic_GetLevelForPort(i + 1) * MAGIC_NORMAL_METER;
                }

                magicWidth = 16 + magicCapacity;
                playerMagicX[i] = playerSwordX[i];
                playerMagicY[i] = (s16)(heartsBottomY + 1.0f);
                playerIconY[i] = playerMagicY[i] + magicHeight + magicGap;
                playerDrawMagic[i] = magicWidth > 0;
            }

            playerItemSlot2Y[i] = playerIconY[i] + (sideBySideQuickSlotLayout ? 0 : buttonStep);
        }
    } else {
        const s16 sharedLeftItemX = leftSwordX + buttonStep;
        const s16 sharedRightItemX = SCREEN_WIDTH - iconSize - sharedLeftItemX;

        playerSwordX[0] = leftSwordX;
        playerSwordX[1] = rightSwordX;
        playerSwordX[2] = leftSwordX;
        playerSwordX[3] = rightSwordX;

        playerItemX[0] = sharedLeftItemX;
        playerItemX[1] = sharedRightItemX;
        playerItemX[2] = sharedLeftItemX;
        playerItemX[3] = sharedRightItemX;

        for (i = 0; i < 4; i++) {
            f32 heartsTopY;
            f32 heartsBottomY;
            s16 magicCapacity;
            s16 magicWidth;
            s16 magicX;
            s32 rightAligned;

            playerHeartRows[i] = Interface_GetHeartRowCountForPort(i + 1);

            if (i < 2) {
                heartsTopY = topHeartsAnchorY;
                heartsBottomY = heartsTopY + ((playerHeartRows[i] - 1) * 10.0f) + (16.0f * heartsScale);
                playerIconY[i] = (s16)(heartsBottomY + sharedTopIconGap + 0.999f);

                if ((i < playerCount) && (Magic_GetLevelForPort(i + 1) != 0)) {
                    magicCapacity = Magic_GetMeterCapacityForPort(i + 1);
                    if (magicCapacity <= 0) {
                        magicCapacity = Magic_GetLevelForPort(i + 1) * MAGIC_NORMAL_METER;
                    }

                    magicWidth = 16 + magicCapacity;
                    rightAligned = (i % 2) == 1;
                    magicX = rightAligned ? (playerSwordX[i] + iconSize - magicWidth) : playerSwordX[i];

                    if (i > 0) {
                        playerMagicX[i] = magicX;
                        playerMagicY[i] = (s16)(heartsBottomY + sharedMagicOffset);
                        playerIconY[i] = playerMagicY[i] + magicHeight + sharedMagicGap;
                        playerDrawMagic[i] = true;
                    } else {
                        /* Player 1 uses the base magic meter draw path; only push icon row below it here. */
                        playerIconY[i] = (s16)(heartsBottomY + sharedMagicOffset) + magicHeight + sharedMagicGap;
                    }
                }
            } else {
                heartsBottomY = bottomHeartsAnchorY + (8.0f * heartsScale);
                heartsTopY = bottomHeartsAnchorY - ((playerHeartRows[i] - 1) * 10.0f) - (8.0f * heartsScale);
                playerIconY[i] = (s16)(heartsTopY - sharedBottomIconGap - iconSize);

                if ((i < playerCount) && (Magic_GetLevelForPort(i + 1) != 0)) {
                    magicCapacity = Magic_GetMeterCapacityForPort(i + 1);
                    if (magicCapacity <= 0) {
                        magicCapacity = Magic_GetLevelForPort(i + 1) * MAGIC_NORMAL_METER;
                    }

                    magicWidth = 16 + magicCapacity;
                    rightAligned = (i % 2) == 1;
                    magicX = rightAligned ? (playerSwordX[i] + iconSize - magicWidth) : playerSwordX[i];

                    playerMagicX[i] = magicX;
                    playerMagicY[i] = (s16)(heartsTopY - magicHeight + sharedMagicOffset);
                    playerIconY[i] = playerMagicY[i] - sharedMagicGap - iconSize;
                    playerDrawMagic[i] = true;
                }
            }

            if (sideBySideQuickSlotLayout) {
                playerItemSlot2X[i] = playerItemX[i] + (((i % 2) == 1) ? -buttonStep : buttonStep);
                playerItemSlot2Y[i] = playerIconY[i];
            } else {
                playerItemSlot2X[i] = playerItemX[i];
                playerItemSlot2Y[i] = playerIconY[i] + buttonStep;
            }
        }
    }

    if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.Changed"), 0)) {
        swordButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.BButton.Value"), swordButtonColor);
    } else if (CVarGetInteger(CVAR_COSMETIC("DefaultColorScheme"), COLORSCHEME_N64) == COLORSCHEME_GAMECUBE) {
        swordButtonColor = (Color_RGB8){ 255, 30, 30 };
    }

    if (CVarGetInteger(CVAR_COSMETIC("HUD.CButtons.Changed"), 0)) {
        itemButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CButtons.Value"), itemButtonColor);
    }

    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);

    for (i = 0; i < playerCount; i++) {
        s16 controllerPort = i + 1;

        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, swordButtonColor.r, swordButtonColor.g, swordButtonColor.b,
                        interfaceCtx->bAlpha);
        OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gButtonBackgroundTex, 32, 32, playerSwordX[i] + bgOffset,
                                      playerIconY[i] + bgOffset, bgSize, bgSize, (32 << 10) / bgSize,
                                      (32 << 10) / bgSize);

        selectedSlot = secondSlotEnabled ? Interface_GetQuickItemSelectedSlotForPort(controllerPort) : 0;

        if (selectedSlot == 0) {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, selectedItemButtonColor.r, selectedItemButtonColor.g,
                            selectedItemButtonColor.b, Interface_GetQuickItemAlpha(interfaceCtx, controllerPort, 0));
        } else {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, itemButtonColor.r, itemButtonColor.g, itemButtonColor.b,
                            Interface_GetQuickItemAlpha(interfaceCtx, controllerPort, 0));
        }

        OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gButtonBackgroundTex, 32, 32, playerItemX[i] + bgOffset,
                                      playerIconY[i] + bgOffset, bgSize, bgSize, (32 << 10) / bgSize,
                                      (32 << 10) / bgSize);

        if (secondSlotEnabled) {
            if (selectedSlot == 1) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, selectedItemButtonColor.r, selectedItemButtonColor.g,
                                selectedItemButtonColor.b, Interface_GetQuickItemAlpha(interfaceCtx, controllerPort, 1));
            } else {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, itemButtonColor.r, itemButtonColor.g, itemButtonColor.b,
                                Interface_GetQuickItemAlpha(interfaceCtx, controllerPort, 1));
            }

            OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gButtonBackgroundTex, 32, 32,
                                          playerItemSlot2X[i] + bgOffset, playerItemSlot2Y[i] + bgOffset, bgSize,
                                          bgSize, (32 << 10) / bgSize, (32 << 10) / bgSize);
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);

    /* Ensure icons are drawn without being tinted by the button primitive color */
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->bAlpha);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
    CLOSE_DISPS(play->state.gfxCtx);

    for (i = 1; i < playerCount; i++) {
        if (playerDrawMagic[i]) {
            Interface_DrawMagicMeterForPortAt(play, i + 1, playerMagicX[i], playerMagicY[i]);
        }
    }

    /* Magic meter drawing changes combine/env state; restore icon-friendly state before item icon draws. */
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->bAlpha);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);
    CLOSE_DISPS(play->state.gfxCtx);

    if ((gSaveContext.equips.buttonItems[0] != ITEM_NONE) && (gSaveContext.equips.buttonItems[0] < ITEM_NONE_FE)) {
        Interface_DrawItemIconTextureAt(play, gItemIcons[gSaveContext.equips.buttonItems[0]], playerSwordX[0],
                                        playerIconY[0], 24, 24);
        Interface_DrawAmmoCountForItemAt(play, gSaveContext.equips.buttonItems[0], interfaceCtx->bAlpha,
                                         playerSwordX[0] + 1, playerIconY[0] + 13);

        Interface_DrawItemIconTextureAt(play, gItemIcons[gSaveContext.equips.buttonItems[0]], playerSwordX[1],
                                        playerIconY[1], 24, 24);
        Interface_DrawAmmoCountForItemAt(play, gSaveContext.equips.buttonItems[0], interfaceCtx->bAlpha,
                                         playerSwordX[1] + 1, playerIconY[1] + 13);

        if (playerCount >= 3) {
            Interface_DrawItemIconTextureAt(play, gItemIcons[gSaveContext.equips.buttonItems[0]], playerSwordX[2],
                                            playerIconY[2], 24, 24);
            Interface_DrawAmmoCountForItemAt(play, gSaveContext.equips.buttonItems[0], interfaceCtx->bAlpha,
                                             playerSwordX[2] + 1, playerIconY[2] + 13);
        }

        if (playerCount >= 4) {
            Interface_DrawItemIconTextureAt(play, gItemIcons[gSaveContext.equips.buttonItems[0]], playerSwordX[3],
                                            playerIconY[3], 24, 24);
            Interface_DrawAmmoCountForItemAt(play, gSaveContext.equips.buttonItems[0], interfaceCtx->bAlpha,
                                             playerSwordX[3] + 1, playerIconY[3] + 13);
        }
    }

    for (i = 0; i < playerCount; i++) {
        s16 controllerPort = i + 1;

        Interface_DrawQuickItemHudAt(play, controllerPort, 0, playerItemX[i], playerIconY[i]);

        if (secondSlotEnabled) {
            Interface_DrawQuickItemHudAt(play, controllerPort, 1, playerItemSlot2X[i], playerItemSlot2Y[i]);
        }
    }
    Interface_DrawRadialQuickMenuHud(play, playerCount);
}

void Interface_DrawActionButton(PlayState* play, f32 x, f32 y) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Translate(-137.0f + x, 97.0f - y, XREG(18) / 10.0f, MTXMODE_NEW);
    Matrix_Scale(1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
    Matrix_RotateX(interfaceCtx->unk_1F4 / 10000.0f, MTXMODE_APPLY);

    gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPVertex(OVERLAY_DISP++, &interfaceCtx->actionVtx[0], 4, 0);

    gDPLoadTextureBlock(OVERLAY_DISP++, gButtonBackgroundTex, G_IM_FMT_IA, G_IM_SIZ_8b, 32, 32, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);

    gSP1Quadrangle(OVERLAY_DISP++, 0, 2, 3, 1, 0);

    CLOSE_DISPS(play->state.gfxCtx);
}

void Interface_InitVertices(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 i;

    interfaceCtx->actionVtx = Graph_Alloc(play->state.gfxCtx, 8 * sizeof(Vtx));

    // clang-format off
    interfaceCtx->actionVtx[0].v.ob[0] =
    interfaceCtx->actionVtx[2].v.ob[0] = -15;
    interfaceCtx->actionVtx[1].v.ob[0] =
    interfaceCtx->actionVtx[3].v.ob[0] = interfaceCtx->actionVtx[0].v.ob[0] + 29;

    interfaceCtx->actionVtx[0].v.ob[1] =
    interfaceCtx->actionVtx[1].v.ob[1] = 15;
    interfaceCtx->actionVtx[2].v.ob[1] =
    interfaceCtx->actionVtx[3].v.ob[1] = interfaceCtx->actionVtx[0].v.ob[1] - 29;

    interfaceCtx->actionVtx[4].v.ob[0] =
    interfaceCtx->actionVtx[6].v.ob[0] = -((XREG(21) + 1) / 2);
    interfaceCtx->actionVtx[5].v.ob[0] =
    interfaceCtx->actionVtx[7].v.ob[0] = interfaceCtx->actionVtx[4].v.ob[0] + (XREG(21) + 1);

    interfaceCtx->actionVtx[4].v.ob[1] =
    interfaceCtx->actionVtx[5].v.ob[1] = XREG(28) / 2;
    interfaceCtx->actionVtx[6].v.ob[1] =
    interfaceCtx->actionVtx[7].v.ob[1] = interfaceCtx->actionVtx[4].v.ob[1] - XREG(28);

    for (i = 0; i < 8; i += 4) {
        interfaceCtx->actionVtx[i].v.ob[2] = interfaceCtx->actionVtx[i+1].v.ob[2] =
        interfaceCtx->actionVtx[i+2].v.ob[2] = interfaceCtx->actionVtx[i+3].v.ob[2] = 0;

        interfaceCtx->actionVtx[i].v.flag = interfaceCtx->actionVtx[i+1].v.flag =
        interfaceCtx->actionVtx[i+2].v.flag = interfaceCtx->actionVtx[i+3].v.flag = 0;

        interfaceCtx->actionVtx[i].v.tc[0] = interfaceCtx->actionVtx[i].v.tc[1] =
        interfaceCtx->actionVtx[i+1].v.tc[1] = interfaceCtx->actionVtx[i+2].v.tc[0] = -16;
        interfaceCtx->actionVtx[i+1].v.tc[0] = interfaceCtx->actionVtx[i+2].v.tc[1] =
        interfaceCtx->actionVtx[i+3].v.tc[0] = interfaceCtx->actionVtx[i+3].v.tc[1] = 1024 - 16;

        interfaceCtx->actionVtx[i].v.cn[0] = interfaceCtx->actionVtx[i+1].v.cn[0] =
        interfaceCtx->actionVtx[i+2].v.cn[0] = interfaceCtx->actionVtx[i+3].v.cn[0] =
        interfaceCtx->actionVtx[i].v.cn[1] = interfaceCtx->actionVtx[i+1].v.cn[1] =
        interfaceCtx->actionVtx[i+2].v.cn[1] = interfaceCtx->actionVtx[i+3].v.cn[1] =
        interfaceCtx->actionVtx[i].v.cn[2] = interfaceCtx->actionVtx[i+1].v.cn[2] =
        interfaceCtx->actionVtx[i+2].v.cn[2] = interfaceCtx->actionVtx[i+3].v.cn[2] = 255;

        interfaceCtx->actionVtx[i].v.cn[3] = interfaceCtx->actionVtx[i+1].v.cn[3] =
        interfaceCtx->actionVtx[i+2].v.cn[3] = interfaceCtx->actionVtx[i+3].v.cn[3] = 255;
    }

    interfaceCtx->actionVtx[5].v.tc[0] = interfaceCtx->actionVtx[7].v.tc[0] = 1536;
    interfaceCtx->actionVtx[6].v.tc[1] = interfaceCtx->actionVtx[7].v.tc[1] = 512;

    interfaceCtx->beatingHeartVtx = Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));

    interfaceCtx->beatingHeartVtx[0].v.ob[0] = interfaceCtx->beatingHeartVtx[2].v.ob[0] = -8;
    interfaceCtx->beatingHeartVtx[1].v.ob[0] = interfaceCtx->beatingHeartVtx[3].v.ob[0] = 8;
    interfaceCtx->beatingHeartVtx[0].v.ob[1] = interfaceCtx->beatingHeartVtx[1].v.ob[1] = 8;
    interfaceCtx->beatingHeartVtx[2].v.ob[1] = interfaceCtx->beatingHeartVtx[3].v.ob[1] = -8;

    interfaceCtx->beatingHeartVtx[0].v.ob[2] = interfaceCtx->beatingHeartVtx[1].v.ob[2] =
    interfaceCtx->beatingHeartVtx[2].v.ob[2] = interfaceCtx->beatingHeartVtx[3].v.ob[2] = 0;

    interfaceCtx->beatingHeartVtx[0].v.flag = interfaceCtx->beatingHeartVtx[1].v.flag =
    interfaceCtx->beatingHeartVtx[2].v.flag = interfaceCtx->beatingHeartVtx[3].v.flag = 0;

    interfaceCtx->beatingHeartVtx[0].v.tc[0] = interfaceCtx->beatingHeartVtx[0].v.tc[1] =
    interfaceCtx->beatingHeartVtx[1].v.tc[1] = interfaceCtx->beatingHeartVtx[2].v.tc[0] = 0;
    interfaceCtx->beatingHeartVtx[1].v.tc[0] = interfaceCtx->beatingHeartVtx[2].v.tc[1] =
    interfaceCtx->beatingHeartVtx[3].v.tc[0] = interfaceCtx->beatingHeartVtx[3].v.tc[1] = 512;

    interfaceCtx->beatingHeartVtx[0].v.cn[0] = interfaceCtx->beatingHeartVtx[1].v.cn[0] =
    interfaceCtx->beatingHeartVtx[2].v.cn[0] = interfaceCtx->beatingHeartVtx[3].v.cn[0] =
    interfaceCtx->beatingHeartVtx[0].v.cn[1] = interfaceCtx->beatingHeartVtx[1].v.cn[1] =
    interfaceCtx->beatingHeartVtx[2].v.cn[1] = interfaceCtx->beatingHeartVtx[3].v.cn[1] =
    interfaceCtx->beatingHeartVtx[0].v.cn[2] = interfaceCtx->beatingHeartVtx[1].v.cn[2] =
    interfaceCtx->beatingHeartVtx[2].v.cn[2] = interfaceCtx->beatingHeartVtx[3].v.cn[2] =
    interfaceCtx->beatingHeartVtx[0].v.cn[3] = interfaceCtx->beatingHeartVtx[1].v.cn[3] =
    interfaceCtx->beatingHeartVtx[2].v.cn[3] = interfaceCtx->beatingHeartVtx[3].v.cn[3] = 255;
    // clang-format on
}

/*
void func_8008A8B8(PlayState* play, s32 topY, s32 bottomY, s32 leftX, s32 rightX) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Vec3f eye;
    Vec3f lookAt;
    Vec3f up;

    eye.x = eye.y = eye.z = 0.0f;
    lookAt.x = lookAt.y = 0.0f;
    lookAt.z = -1.0f;
    up.x = up.z = 0.0f;
    up.y = 1.0f;

    func_800AA358(&interfaceCtx->view, &eye, &lookAt, &up);

    interfaceCtx->viewport.topY = topY;
    interfaceCtx->viewport.bottomY = bottomY;
    interfaceCtx->viewport.leftX = leftX;
    interfaceCtx->viewport.rightX = rightX;
    View_SetViewport(&interfaceCtx->view, &interfaceCtx->viewport);

    func_800AA460(&interfaceCtx->view, 60.0f, 10.0f, 60.0f);
    func_800AB560(&interfaceCtx->view);
}
*/

void func_8008A994(InterfaceContext* interfaceCtx) {
    SET_FULLSCREEN_VIEWPORT(&interfaceCtx->view);
    func_800AB2C4(&interfaceCtx->view);
}

const char* digitTextures[] = { gCounterDigit0Tex, gCounterDigit1Tex, gCounterDigit2Tex, gCounterDigit3Tex,
                                gCounterDigit4Tex, gCounterDigit5Tex, gCounterDigit6Tex, gCounterDigit7Tex,
                                gCounterDigit8Tex, gCounterDigit9Tex, gCounterColonTex,  gCounterDigit1Tex,
                                gCounterDigit2Tex, gCounterDigit3Tex, gCounterDigit4Tex, gCounterDigit5Tex,
                                gCounterDigit6Tex, gCounterDigit7Tex, gCounterDigit8Tex };

void Interface_Draw(PlayState* play) {
    static s16 magicArrowEffectsR[] = { 255, 100, 255 };
    static s16 magicArrowEffectsG[] = { 0, 100, 255 };
    static s16 magicArrowEffectsB[] = { 0, 255, 100 };
    static s16 timerDigitLeftPos[] = { 16, 25, 34, 42, 51 };
    static s16 digitWidth[] = { 9, 9, 8, 9, 9 };
    // unused, most likely colors
    static s16 D_80125B1C[][3] = {
        { 0, 150, 0 }, { 100, 255, 0 }, { 255, 255, 255 }, { 0, 0, 0 }, { 255, 255, 255 },
    };
    static s16 rupeeDigitsFirst[] = { 1, 0, 0, 0 };
    static s16 rupeeDigitsCount[] = { 2, 3, 3, 3 };

    // courtesy of https://github.com/TestRunnerSRL/OoT-Randomizer/blob/Dev/ASM/c/hud_colors.c
    static Color_RGB8 rupeeWalletColors[4] = {
        { 0xC8, 0xFF, 0x64 }, // Base Wallet (Green)
        { 0x82, 0x82, 0xFF }, // Adult's Wallet (Blue)
        { 0xFF, 0x64, 0x64 }, // Giant's Wallet (Red)
        { 0xFF, 0x5A, 0xFF }, // Tycoon's Wallet (Purple). Only used in rando shopsanity.
    };
    Color_RGB8 rColor;

    Color_RGB8 keyCountColor = { 200, 230, 255 };
    if (CVarGetInteger(CVAR_COSMETIC("HUD.KeyCount.Changed"), 0)) {
        keyCountColor = CVarGetColor24(CVAR_COSMETIC("HUD.KeyCount.Value"), keyCountColor);
    }

    Color_RGB8 dPadColor = { 255, 255, 255 };
    if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.Changed"), 0)) {
        dPadColor = CVarGetColor24(CVAR_COSMETIC("HUD.Dpad.Value"), dPadColor);
    }

    Color_RGB8 aButtonColor = { 90, 90, 255 };
    if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.Changed"), 0)) {
        aButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.AButton.Value"), aButtonColor);
    } else if (CVarGetInteger(CVAR_COSMETIC("DefaultColorScheme"), COLORSCHEME_N64) == COLORSCHEME_GAMECUBE) {
        aButtonColor = (Color_RGB8){ 0, 200, 50 };
    }

    static s16 spoilingItemEntrances[] = { ENTR_LOST_WOODS_2, ENTR_ZORAS_DOMAIN_3, ENTR_ZORAS_DOMAIN_3 };
    static f32 D_80125B54[] = { -40.0f, -35.0f }; // unused
    static s16 D_80125B5C[] = { 91, 91 };         // unused
    static s16 sTimerNextSecondTimer;
    static s16 sTimerStateTimer;
    static s16 sSubTimerNextSecondTimer;
    static s16 sSubTimerStateTimer;
    static s16 timerDigits[5];
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    PauseContext* pauseCtx = &play->pauseCtx;
    MessageContext* msgCtx = &play->msgCtx;
    Player* player = GET_PLAYER(play);
    s16 svar1;
    s16 svar2;
    s16 svar3;
    s16 svar4;
    s16 svar5;
    s16 timerId;
    bool fullUi = !CVarGetInteger(CVAR_ENHANCEMENT("MinimalUI"), 0) || !R_MINIMAP_DISABLED || play->pauseCtx.state != 0;
    s16 localMultiplayerPlayerCount = Interface_GetLocalMultiplayerPlayerCount();
    bool localMultiplayerEnabled = localMultiplayerPlayerCount > 1;
    s32 originalQuickItemModeEnabled = Interface_IsOriginalMultiplayerItemModeEnabled();
    // #region SOH [NTSC]
    s32 languageOffset = gSaveContext.language;

    if (languageOffset == LANGUAGE_JPN) {
        languageOffset = LANGUAGE_ENG;
    }
    // #endregion

    if (GameInteractor_NoUIActive()) {
        return;
    }

    Interface_UpdateRadialQuickMenu(play);

    OPEN_DISPS(play->state.gfxCtx);

    gSPSegment(OVERLAY_DISP++, 0x02, interfaceCtx->parameterSegment);
    gSPSegment(OVERLAY_DISP++, 0x07, interfaceCtx->doActionSegment);
    gSPSegment(OVERLAY_DISP++, 0x08, interfaceCtx->iconItemSegment);
    gSPSegment(OVERLAY_DISP++, 0x0B, interfaceCtx->mapSegment);

    if (pauseCtx->debugState == 0) {
        Interface_InitVertices(play);
        func_8008A994(interfaceCtx);
        if (fullUi || gSaveContext.health != gSaveContext.healthCapacity) {
            HealthMeter_Draw(play);
        }

        Gfx_SetupDL_39Overlay(play->state.gfxCtx);

        if (fullUi) {
            s16 PosX_RC;
            s16 PosY_RC;
            if (GameInteractor_Should(VB_RENDER_RUPEE_COUNTER, true)) {
                // Rupee Icon
                if (CVarGetInteger(CVAR_ENHANCEMENT("DynamicWalletIcon"), 0)) {
                    switch (CUR_UPG_VALUE(UPG_WALLET)) {
                        case 0:
                            if (CVarGetInteger(CVAR_COSMETIC("Consumable.GreenRupee.Changed"), 0)) {
                                rColor =
                                    CVarGetColor24(CVAR_COSMETIC("Consumable.GreenRupee.Value"), rupeeWalletColors[0]);
                            } else {
                                rColor = rupeeWalletColors[0];
                            }
                            break;
                        case 1:
                            if (CVarGetInteger(CVAR_COSMETIC("Consumable.BlueRupee.Changed"), 0)) {
                                rColor =
                                    CVarGetColor24(CVAR_COSMETIC("Consumable.BlueRupee.Value"), rupeeWalletColors[1]);
                            } else {
                                rColor = rupeeWalletColors[1];
                            }
                            break;
                        case 2:
                            if (CVarGetInteger(CVAR_COSMETIC("Consumable.RedRupee.Changed"), 0)) {
                                rColor =
                                    CVarGetColor24(CVAR_COSMETIC("Consumable.RedRupee.Value"), rupeeWalletColors[2]);
                            } else {
                                rColor = rupeeWalletColors[2];
                            }
                            break;
                        case 3:
                            if (CVarGetInteger(CVAR_COSMETIC("Consumable.PurpleRupee.Changed"), 0)) {
                                rColor =
                                    CVarGetColor24(CVAR_COSMETIC("Consumable.PurpleRupee.Value"), rupeeWalletColors[3]);
                            } else {
                                rColor = rupeeWalletColors[3];
                            }
                            break;
                    }
                } else {
                    if (CVarGetInteger(CVAR_COSMETIC("Consumable.GreenRupee.Changed"), rupeeWalletColors)) {
                        rColor = CVarGetColor24(CVAR_COSMETIC("Consumable.GreenRupee.Value"), rupeeWalletColors[0]);
                    } else {
                        rColor = rupeeWalletColors[0];
                    }
                }

                // Rupee icon & counter
                s16 X_Margins_RC;
                s16 Y_Margins_RC;
                if (CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.UseMargins"), 0) != 0) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.PosType"), 0) == ORIGINAL_LOCATION) {
                        X_Margins_RC = Left_HUD_Margin;
                    };
                    Y_Margins_RC = Bottom_HUD_Margin;
                } else {
                    X_Margins_RC = 0;
                    Y_Margins_RC = 0;
                }
                s16 PosX_RC_ori = OTRGetRectDimensionFromLeftEdge(0 + X_Margins_RC);
                s16 PosY_RC_ori = (SCREEN_HEIGHT - 16) + Y_Margins_RC;

                if (PosX_RC_ori < 0) {
                    PosX_RC_ori = 0;
                }

                if (PosY_RC_ori < 0) {
                    PosY_RC_ori = 0;
                } else if (PosY_RC_ori > (SCREEN_HEIGHT - 16)) {
                    PosY_RC_ori = SCREEN_HEIGHT - 16;
                }

                if (CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.PosType"), 0) != ORIGINAL_LOCATION) {
                    PosY_RC = CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.PosY"), 0) + Y_Margins_RC;
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.PosType"), 0) == ANCHOR_LEFT) {
                        if (CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.UseMargins"), 0) != 0) {
                            X_Margins_RC = Left_HUD_Margin;
                        };
                        PosX_RC = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.PosX"), 0) +
                                                              X_Margins_RC);
                    } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.PosType"), 0) == ANCHOR_RIGHT) {
                        if (CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.UseMargins"), 0) != 0) {
                            X_Margins_RC = Right_HUD_Margin;
                        };
                        PosX_RC = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.PosX"), 0) +
                                                               X_Margins_RC);
                    } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.PosType"), 0) == ANCHOR_NONE) {
                        PosX_RC = CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.PosX"), 0);
                    } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Rupees.PosType"), 0) == HIDDEN) {
                        PosX_RC = -9999;
                    }
                } else {
                    PosY_RC = PosY_RC_ori;
                    PosX_RC = PosX_RC_ori;
                }
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, rColor.r, rColor.g, rColor.b, interfaceCtx->magicAlpha);
                OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gRupeeCounterIconTex, 16, 16, PosX_RC, PosY_RC, 16, 16,
                                              1 << 10, 1 << 10);
            }

            if (GameInteractor_Should(VB_RENDER_KEY_COUNTER, true)) {
                switch (play->sceneNum) {
                    case SCENE_FOREST_TEMPLE:
                    case SCENE_FIRE_TEMPLE:
                    case SCENE_WATER_TEMPLE:
                    case SCENE_SPIRIT_TEMPLE:
                    case SCENE_SHADOW_TEMPLE:
                    case SCENE_BOTTOM_OF_THE_WELL:
                    case SCENE_ICE_CAVERN:
                    case SCENE_GANONS_TOWER:
                    case SCENE_GERUDO_TRAINING_GROUND:
                    case SCENE_THIEVES_HIDEOUT:
                    case SCENE_INSIDE_GANONS_CASTLE:
                    case SCENE_GANONS_TOWER_COLLAPSE_INTERIOR:
                    case SCENE_INSIDE_GANONS_CASTLE_COLLAPSE:
                    case SCENE_TREASURE_BOX_SHOP:
                        if (gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex] >= 0) {
                            s16 X_Margins_SKC;
                            s16 Y_Margins_SKC;
                            if (CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.UseMargins"), 0) != 0) {
                                if (CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.PosType"), 0) == ORIGINAL_LOCATION) {
                                    X_Margins_SKC = Left_HUD_Margin;
                                };
                                Y_Margins_SKC = Bottom_HUD_Margin;
                            } else {
                                X_Margins_SKC = 0;
                                Y_Margins_SKC = 0;
                            }
                            s16 PosX_SKC_ori = OTRGetRectDimensionFromLeftEdge(26 + X_Margins_SKC);
                            s16 PosY_SKC_ori = 190 + Y_Margins_SKC;
                            s16 PosX_SKC;
                            s16 PosY_SKC;
                            if (CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.PosType"), 0) != ORIGINAL_LOCATION) {
                                PosY_SKC = CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.PosY"), 0) + Y_Margins_SKC;
                                if (CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.PosType"), 0) == ANCHOR_LEFT) {
                                    if (CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.UseMargins"), 0) != 0) {
                                        X_Margins_SKC = Left_HUD_Margin;
                                    };
                                    PosX_SKC = OTRGetDimensionFromLeftEdge(
                                        CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.PosX"), 0) + X_Margins_SKC);
                                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.PosType"), 0) == ANCHOR_RIGHT) {
                                    if (CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.UseMargins"), 0) != 0) {
                                        X_Margins_SKC = Right_HUD_Margin;
                                    };
                                    PosX_SKC = OTRGetDimensionFromRightEdge(
                                        CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.PosX"), 0) + X_Margins_SKC);
                                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.PosType"), 0) == ANCHOR_NONE) {
                                    PosX_SKC = CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.PosX"), 0);
                                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.SmallKey.PosType"), 0) == HIDDEN) {
                                    PosX_SKC = -9999;
                                }
                            } else {
                                PosY_SKC = PosY_SKC_ori;
                                PosX_SKC = PosX_SKC_ori;
                            }
                            // Small Key Icon
                            gDPPipeSync(OVERLAY_DISP++);

                            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, keyCountColor.r, keyCountColor.g, keyCountColor.b,
                                            interfaceCtx->magicAlpha);
                            gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 20,
                                           255); // We reset this here so it match user color :)
                            OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gSmallKeyCounterIconTex, 16, 16, PosX_SKC,
                                                          PosY_SKC, 16, 16, 1 << 10, 1 << 10);

                            // Small Key Counter
                            gDPPipeSync(OVERLAY_DISP++);
                            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->magicAlpha);
                            gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0,
                                              PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);

                            interfaceCtx->counterDigits[2] = 0;
                            interfaceCtx->counterDigits[3] = gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex];

                            while (interfaceCtx->counterDigits[3] >= 10) {
                                interfaceCtx->counterDigits[2]++;
                                interfaceCtx->counterDigits[3] -= 10;
                            }

                            svar3 = 16;
                            if (interfaceCtx->counterDigits[2] != 0) {
                                OVERLAY_DISP = Gfx_TextureI8(
                                    OVERLAY_DISP, ((u8*)((u8*)digitTextures[interfaceCtx->counterDigits[2]])), 8, 16,
                                    PosX_SKC + 16, PosY_SKC, 8, 16, 1 << 10, 1 << 10);
                                svar3 = 24;
                            }

                            OVERLAY_DISP =
                                Gfx_TextureI8(OVERLAY_DISP, ((u8*)digitTextures[interfaceCtx->counterDigits[3]]), 8, 16,
                                              PosX_SKC + svar3, PosY_SKC, 8, 16, 1 << 10, 1 << 10);
                        }
                        break;
                    default:
                        break;
                }
            }

            if (GameInteractor_Should(VB_RENDER_RUPEE_COUNTER, true)) {
                // Rupee Counter
                gDPPipeSync(OVERLAY_DISP++);

                if (gSaveContext.rupees == CUR_CAPACITY(UPG_WALLET)) {
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 120, 255, 0, interfaceCtx->magicAlpha);
                } else if (gSaveContext.rupees != 0) {
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->magicAlpha);
                } else {
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 100, 100, 100, interfaceCtx->magicAlpha);
                }

                gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE,
                                  TEXEL0, 0, PRIMITIVE, 0);

                interfaceCtx->counterDigits[0] = interfaceCtx->counterDigits[1] = 0;
                interfaceCtx->counterDigits[2] = gSaveContext.rupees;

                if ((interfaceCtx->counterDigits[2] > 9999) || (interfaceCtx->counterDigits[2] < 0)) {
                    interfaceCtx->counterDigits[2] &= 0xDDD;
                }

                while (interfaceCtx->counterDigits[2] >= 100) {
                    interfaceCtx->counterDigits[0]++;
                    interfaceCtx->counterDigits[2] -= 100;
                }

                while (interfaceCtx->counterDigits[2] >= 10) {
                    interfaceCtx->counterDigits[1]++;
                    interfaceCtx->counterDigits[2] -= 10;
                }

                svar2 = rupeeDigitsFirst[CUR_UPG_VALUE(UPG_WALLET)];
                svar5 = rupeeDigitsCount[CUR_UPG_VALUE(UPG_WALLET)];

                for (svar1 = 0, svar3 = 16; svar1 < svar5; svar1++, svar2++, svar3 += 8) {
                    OVERLAY_DISP = Gfx_TextureI8(OVERLAY_DISP, ((u8*)digitTextures[interfaceCtx->counterDigits[svar2]]),
                                                 8, 16, PosX_RC + svar3, PosY_RC, 8, 16, 1 << 10, 1 << 10);
                }
            }
        } else {
            // Make sure item counts have black backgrounds
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, interfaceCtx->magicAlpha);
            gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);
        }

        if (CVarGetInteger(CVAR_ENHANCEMENT("DrawLineupTick"), 0)) {
            Interface_DrawLineupTick(play);
        }

        if (fullUi || gSaveContext.magicState > MAGIC_STATE_IDLE) {
            Interface_DrawMagicBar(play);
        }

        Minimap_Draw(play);

        if ((R_PAUSE_MENU_MODE != 2) && (R_PAUSE_MENU_MODE != 3)) {
            s32 useSplitTargetViewport = false;

            if (localMultiplayerEnabled && Interface_IsSplitScreenEnabled()) {
                Interface_GetSplitViewportBounds(localMultiplayerPlayerCount, 0, &interfaceCtx->viewport);
                View_SetViewport(&interfaceCtx->view, &interfaceCtx->viewport);
                func_800AB2C4(&interfaceCtx->view);
                useSplitTargetViewport = true;
            }

            if (CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0)) {
                gSPMatrix(OVERLAY_DISP++, interfaceCtx->view.projectionFlippedPtr,
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
            }
            func_8002C124(&play->actorCtx.targetCtx, play); // Draw Z-Target
            if (CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0)) {
                gSPMatrix(OVERLAY_DISP++, interfaceCtx->view.projectionPtr,
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
            }

            // Render enemy health bar after Z-target to leverage set variables
            if (CVarGetInteger(CVAR_ENHANCEMENT("EnemyHealthBar"), 0)) {
                Interface_DrawEnemyHealthBar(&play->actorCtx.targetCtx, play);
            }

            if (useSplitTargetViewport) {
                func_8008A994(interfaceCtx);
            }
        }

        Gfx_SetupDL_39Overlay(play->state.gfxCtx);

        if (fullUi) {
            Interface_DrawItemButtons(play);
        }

        gDPPipeSync(OVERLAY_DISP++);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->bAlpha);
        gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);

        if (!originalQuickItemModeEnabled) {
            if (!(interfaceCtx->unk_1FA)) {
            // B Button Icon & Ammo Count
            if (gSaveContext.equips.buttonItems[0] != ITEM_NONE) {
                if (fullUi) {
                    Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[0]], 0);
                }

                if ((player->stateFlags1 & PLAYER_STATE1_ON_HORSE) || (play->shootingGalleryStatus > 1) ||
                    ((play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY) && Flags_GetSwitch(play, 0x38))) {

                    if (!fullUi) {
                        Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[0]], 0);
                    }

                    gDPPipeSync(OVERLAY_DISP++);
                    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE,
                                      0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
                    Interface_DrawAmmoCount(play, 0, interfaceCtx->bAlpha);
                }
            }
            } else {
            // B Button Do Action Label
            s16 PosX_adjust;
            s16 PosY_adjust;
            if (gSaveContext.language == LANGUAGE_FRA) {
                PosX_adjust = -12;
                PosY_adjust = 5;
            } else if (gSaveContext.language == LANGUAGE_GER) {
                PosY_adjust = 6;
                PosX_adjust = -9;
            } else {
                PosY_adjust = 6;
                PosX_adjust = -10;
            }

            s16 BbtnPosX;
            s16 BbtnPosY;
            s16 X_Margins_BtnB_label;
            s16 Y_Margins_BtnB_label;
            if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
                if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ORIGINAL_LOCATION) {
                    X_Margins_BtnB_label = Right_HUD_Margin;
                };
                Y_Margins_BtnB_label = (Top_HUD_Margin * -1);
            } else {
                X_Margins_BtnB_label = 0;
                Y_Margins_BtnB_label = 0;
            }
            if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) != ORIGINAL_LOCATION) {
                BbtnPosY = CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosY"), 0) + Y_Margins_BtnB_label + PosY_adjust;
                if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ANCHOR_LEFT) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
                        X_Margins_BtnB_label = Left_HUD_Margin;
                    };
                    BbtnPosX = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosX"), 0) +
                                                           X_Margins_BtnB_label + PosX_adjust);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ANCHOR_RIGHT) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
                        X_Margins_BtnB_label = Right_HUD_Margin;
                    };
                    BbtnPosX = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosX"), 0) +
                                                            X_Margins_BtnB_label + PosX_adjust);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ANCHOR_NONE) {
                    BbtnPosX = CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosX"), 0) + PosX_adjust;
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == HIDDEN) {
                    BbtnPosX = -9999;
                }
            } else {
                BbtnPosX = OTRGetRectDimensionFromRightEdge(R_B_LABEL_X(languageOffset) + X_Margins_BtnB_label);
                BbtnPosY = R_B_LABEL_Y(languageOffset) + Y_Margins_BtnB_label;
            }
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                              PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->bAlpha);

            gDPLoadTextureBlock_4b(OVERLAY_DISP++, interfaceCtx->doActionSegment[1], G_IM_FMT_IA, DO_ACTION_TEX_WIDTH(),
                                   DO_ACTION_TEX_HEIGHT(), 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

            R_B_LABEL_DD = (1 << 10) / (WREG(37 + languageOffset) / 100.0f);
            gSPWideTextureRectangle(OVERLAY_DISP++, BbtnPosX << 2, BbtnPosY << 2,
                                    (BbtnPosX + DO_ACTION_TEX_WIDTH()) << 2, (BbtnPosY + DO_ACTION_TEX_HEIGHT()) << 2,
                                    G_TX_RENDERTILE, 0, 0, R_B_LABEL_DD, R_B_LABEL_DD);
            }
        }

        if (fullUi && originalQuickItemModeEnabled) {
            Interface_DrawOriginalMultiplayerItemHud(play);
        }

        if (!originalQuickItemModeEnabled) {
            gDPPipeSync(OVERLAY_DISP++);

            // C-Left Button Icon & Ammo Count
            if (gSaveContext.equips.buttonItems[1] < 0xF0) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->cLeftAlpha);
                gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
                Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[1]], 1);
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0,
                                  PRIMITIVE, 0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE,
                                  0);
                Interface_DrawAmmoCount(play, 1, interfaceCtx->cLeftAlpha);
            }

            gDPPipeSync(OVERLAY_DISP++);

            // C-Down Button Icon & Ammo Count
            if (gSaveContext.equips.buttonItems[2] < 0xF0) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->cDownAlpha);
                gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
                Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[2]], 2);
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0,
                                  PRIMITIVE, 0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE,
                                  0);
                Interface_DrawAmmoCount(play, 2, interfaceCtx->cDownAlpha);
            }

            gDPPipeSync(OVERLAY_DISP++);

            // C-Right Button Icon & Ammo Count
            if (gSaveContext.equips.buttonItems[3] < 0xF0) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->cRightAlpha);
                gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
                Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[3]], 3);
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0,
                                  PRIMITIVE, 0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE,
                                  0);
                Interface_DrawAmmoCount(play, 3, interfaceCtx->cRightAlpha);
            }

            if (CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) != 0) {
                // DPad is only greyed-out when all 4 DPad directions are too
                uint16_t dpadAlpha =
                    MAX(MAX(MAX(interfaceCtx->dpadUpAlpha, interfaceCtx->dpadDownAlpha), interfaceCtx->dpadLeftAlpha),
                        interfaceCtx->dpadRightAlpha);

            // Draw DPad
            s16 DpadPosX;
            s16 DpadPosY;
            s16 X_Margins_Dpad;
            s16 Y_Margins_Dpad;
            if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.UseMargins"), 0) != 0) {
                if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ORIGINAL_LOCATION) {
                    X_Margins_Dpad = Right_HUD_Margin;
                };
                Y_Margins_Dpad = (Top_HUD_Margin * -1);
            } else {
                Y_Margins_Dpad = 0;
                X_Margins_Dpad = 0;
            }
            if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) != ORIGINAL_LOCATION) {
                DpadPosY = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_Dpad;
                if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ANCHOR_LEFT) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.UseMargins"), 0) != 0) {
                        X_Margins_Dpad = Left_HUD_Margin;
                    };
                    DpadPosX =
                        OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + X_Margins_Dpad);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ANCHOR_RIGHT) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.UseMargins"), 0) != 0) {
                        X_Margins_Dpad = Right_HUD_Margin;
                    };
                    DpadPosX = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                            X_Margins_Dpad);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ANCHOR_NONE) {
                    DpadPosX = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == HIDDEN) {
                    DpadPosX = -9999;
                }
            } else {
                DpadPosX = OTRGetRectDimensionFromRightEdge(DPAD_X + X_Margins_Dpad);
                DpadPosY = DPAD_Y + Y_Margins_Dpad;
            }

            gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, dPadColor.r, dPadColor.g, dPadColor.b, dpadAlpha);
            if (fullUi) {
                gDPLoadTextureBlock(OVERLAY_DISP++, gDPadTex, G_IM_FMT_IA, G_IM_SIZ_16b, 32, 32, 0,
                                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                    G_TX_NOLOD, G_TX_NOLOD);
                gSPWideTextureRectangle(OVERLAY_DISP++, DpadPosX << 2, DpadPosY << 2, (DpadPosX + 32) << 2,
                                        (DpadPosY + 32) << 2, G_TX_RENDERTILE, 0, 0, (1 << 10), (1 << 10));
            }

            // DPad-Up Button Icon & Ammo Count
            if (gSaveContext.equips.buttonItems[4] < 0xF0) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->dpadUpAlpha);
                gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
                Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[4]], 4);
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                                  PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
                Interface_DrawAmmoCount(play, 4, interfaceCtx->dpadUpAlpha);
            }

            // DPad-Down Button Icon & Ammo Count
            if (gSaveContext.equips.buttonItems[5] < 0xF0) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->dpadDownAlpha);
                gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
                Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[5]], 5);
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                                  PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
                Interface_DrawAmmoCount(play, 5, interfaceCtx->dpadDownAlpha);
            }

            // DPad-Left Button Icon & Ammo Count
            if (gSaveContext.equips.buttonItems[6] < 0xF0) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->dpadLeftAlpha);
                gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
                Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[6]], 6);
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                                  PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
                Interface_DrawAmmoCount(play, 6, interfaceCtx->dpadLeftAlpha);
            }

                // DPad-Right Button Icon & Ammo Count
                if (gSaveContext.equips.buttonItems[7] < 0xF0) {
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->dpadRightAlpha);
                    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
                    Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[7]], 7);
                    gDPPipeSync(OVERLAY_DISP++);
                    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0,
                                      PRIMITIVE, 0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0,
                                      PRIMITIVE, 0);
                    Interface_DrawAmmoCount(play, 7, interfaceCtx->dpadRightAlpha);
                }
            }
        }

        if (!localMultiplayerEnabled) {
            // A Button
            Gfx_SetupDL_42Overlay(play->state.gfxCtx);
            s16 X_Margins_BtnA;
            s16 Y_Margins_BtnA;
            if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.UseMargins"), 0) != 0) {
                X_Margins_BtnA = Right_HUD_Margin;
                Y_Margins_BtnA = (Top_HUD_Margin * -1);
            } else {
                X_Margins_BtnA = 0;
                Y_Margins_BtnA = 0;
            }
            s16 PosX_BtnA_ori = OTRGetDimensionFromRightEdge(R_A_BTN_X + X_Margins_BtnA);
            s16 PosY_BtnA_ori = R_A_BTN_Y + Y_Margins_BtnA;
            const f32 rAIconX_ori = OTRGetDimensionFromRightEdge(R_A_ICON_X + X_Margins_BtnA);
            const f32 rAIconY_ori = 98.0f - (R_A_ICON_Y + Y_Margins_BtnA);
            s16 PosX_BtnA;
            s16 PosY_BtnA;
            s16 rAIconX;
            s16 rAIconY;
            if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.PosType"), 0) != ORIGINAL_LOCATION) {
                PosY_BtnA = CVarGetInteger(CVAR_COSMETIC("HUD.AButton.PosY"), 0) + Y_Margins_BtnA;
                rAIconY = 98.0f - PosY_BtnA;
                if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.PosType"), 0) == ANCHOR_LEFT) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.UseMargins"), 0) != 0) {
                        X_Margins_BtnA = Left_HUD_Margin;
                    };
                    PosX_BtnA = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.AButton.PosX"), 0) +
                                                            X_Margins_BtnA);
                    rAIconX = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.AButton.PosX"), 0) +
                                                          X_Margins_BtnA);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.PosType"), 0) == ANCHOR_RIGHT) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.UseMargins"), 0) != 0) {
                        X_Margins_BtnA = Right_HUD_Margin;
                    };
                    PosX_BtnA = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.AButton.PosX"), 0) +
                                                             X_Margins_BtnA);
                    rAIconX = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.AButton.PosX"), 0) +
                                                           X_Margins_BtnA);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.PosType"), 0) == ANCHOR_NONE) {
                    PosX_BtnA = CVarGetInteger(CVAR_COSMETIC("HUD.AButton.PosX"), 0);
                    rAIconX = CVarGetInteger(CVAR_COSMETIC("HUD.AButton.PosX"), 0);
                } else if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.PosType"), 0) == HIDDEN) {
                    PosX_BtnA = -9999;
                    rAIconX = -9999;
                }
            } else {
                PosY_BtnA = PosY_BtnA_ori;
                PosX_BtnA = PosX_BtnA_ori;
                rAIconY = rAIconY_ori;
                rAIconX = rAIconX_ori;
            }
            gSPClearGeometryMode(OVERLAY_DISP++, G_CULL_BOTH);
            gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, aButtonColor.r, aButtonColor.g, aButtonColor.b, interfaceCtx->aAlpha);
            if (fullUi) {
                Interface_DrawActionButton(play, PosX_BtnA, PosY_BtnA);
            }
            gDPPipeSync(OVERLAY_DISP++);
            gSPSetGeometryMode(OVERLAY_DISP++, G_CULL_BACK);
            gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                              PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->aAlpha);
            gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);
            Matrix_Translate(-138.0f + rAIconX, rAIconY, WREG(46 + languageOffset) / 10.0f, MTXMODE_NEW);
            Matrix_Scale(1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
            Matrix_RotateX(interfaceCtx->unk_1F4 / 10000.0f, MTXMODE_APPLY);
            gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPVertex(OVERLAY_DISP++, &interfaceCtx->actionVtx[4], 4, 0);

            if ((interfaceCtx->unk_1EC < 2) || (interfaceCtx->unk_1EC == 3)) {
                Interface_DrawActionLabel(play->state.gfxCtx, interfaceCtx->doActionSegment[0]);
            } else {
                Interface_DrawActionLabel(play->state.gfxCtx, interfaceCtx->doActionSegment[1]);
            }

            gDPPipeSync(OVERLAY_DISP++);
        }

        func_8008A994(interfaceCtx);
        svar3 = 16;

        if ((pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 3)) {
            // Inventory Equip Effects
            gSPSegment(OVERLAY_DISP++, 0x08, pauseCtx->iconItemSegment);
            Gfx_SetupDL_42Overlay(play->state.gfxCtx);
            gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
            gSPMatrix(OVERLAY_DISP++, &gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD);

            pauseCtx->cursorVtx[svar3].v.ob[0] = pauseCtx->cursorVtx[18].v.ob[0] = svar2 = pauseCtx->equipAnimX / 10;
            pauseCtx->cursorVtx[17].v.ob[0] = pauseCtx->cursorVtx[19].v.ob[0] = svar2 =
                pauseCtx->cursorVtx[svar3].v.ob[0] + WREG(90) / 10;
            pauseCtx->cursorVtx[svar3].v.ob[1] = pauseCtx->cursorVtx[17].v.ob[1] = svar2 = pauseCtx->equipAnimY / 10;
            pauseCtx->cursorVtx[18].v.ob[1] = pauseCtx->cursorVtx[19].v.ob[1] = svar2 =
                pauseCtx->cursorVtx[svar3].v.ob[1] - WREG(90) / 10;

            if (pauseCtx->equipTargetItem < 0xBF) {
                // Normal Equip (icon goes from the inventory slot to the C button when equipping it)
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, pauseCtx->equipAnimAlpha);
                gSPVertex(OVERLAY_DISP++, &pauseCtx->cursorVtx[16], 4, 0);

                gDPLoadTextureBlock(OVERLAY_DISP++, gItemIcons[pauseCtx->equipTargetItem], G_IM_FMT_RGBA, G_IM_SIZ_32b,
                                    32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                                    G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            } else {
                // Magic Arrow Equip Effect
                svar1 = pauseCtx->equipTargetItem - 0xBF;
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicArrowEffectsR[svar1], magicArrowEffectsG[svar1],
                                magicArrowEffectsB[svar1], pauseCtx->equipAnimAlpha);

                if ((pauseCtx->equipAnimAlpha > 0) && (pauseCtx->equipAnimAlpha < 255)) {
                    svar1 = (pauseCtx->equipAnimAlpha / 8) / 2;
                    pauseCtx->cursorVtx[16].v.ob[0] = pauseCtx->cursorVtx[18].v.ob[0] = svar2 =
                        pauseCtx->cursorVtx[16].v.ob[0] - svar1;
                    pauseCtx->cursorVtx[17].v.ob[0] = pauseCtx->cursorVtx[19].v.ob[0] = svar2 =
                        pauseCtx->cursorVtx[16].v.ob[0] + svar1 * 2 + 32;
                    pauseCtx->cursorVtx[16].v.ob[1] = pauseCtx->cursorVtx[17].v.ob[1] = svar2 =
                        pauseCtx->cursorVtx[16].v.ob[1] + svar1;
                    pauseCtx->cursorVtx[18].v.ob[1] = pauseCtx->cursorVtx[19].v.ob[1] = svar2 =
                        pauseCtx->cursorVtx[16].v.ob[1] - svar1 * 2 - 32;
                }

                gSPVertex(OVERLAY_DISP++, &pauseCtx->cursorVtx[16], 4, 0);
                gDPLoadTextureBlock(OVERLAY_DISP++, gMagicArrowEquipEffectTex, G_IM_FMT_IA, G_IM_SIZ_8b, 32, 32, 0,
                                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                    G_TX_NOLOD, G_TX_NOLOD);
            }

            gSP1Quadrangle(OVERLAY_DISP++, 0, 2, 3, 1, 0);
        }

        Gfx_SetupDL_39Overlay(play->state.gfxCtx);

        if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0)) {
            if (gSaveContext.minigameState != 1) {
                // Carrots rendering if the action corresponds to riding a horse
                if (interfaceCtx->unk_1EE == 8 && !CVarGetInteger(CVAR_CHEAT("InfiniteEponaBoost"), 0)) {
                    // Load Carrot Icon
                    gDPLoadTextureBlock(OVERLAY_DISP++, gCarrotIconTex, G_IM_FMT_RGBA, G_IM_SIZ_32b, 16, 16, 0,
                                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                        G_TX_NOLOD, G_TX_NOLOD);

                    // Draw 6 carrots
                    s16 CarrotsPosX = ZREG(14);
                    s16 CarrotsPosY = ZREG(15);
                    s16 CarrotsMargins_X = 0;
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.Carrots.PosType"), 0) != ORIGINAL_LOCATION) {
                        CarrotsPosY = CVarGetInteger(CVAR_COSMETIC("HUD.Carrots.PosY"), 0);
                        if (CVarGetInteger(CVAR_COSMETIC("HUD.Carrots.PosType"), 0) == ANCHOR_LEFT) {
                            if (CVarGetInteger(CVAR_COSMETIC("HUD.Carrots.UseMargins"), 0) != 0) {
                                CarrotsMargins_X = Left_HUD_Margin;
                            };
                            CarrotsPosX = OTRGetDimensionFromLeftEdge(
                                CVarGetInteger(CVAR_COSMETIC("HUD.Carrots.PosX"), 0) + CarrotsMargins_X);
                        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Carrots.PosType"), 0) == ANCHOR_RIGHT) {
                            if (CVarGetInteger(CVAR_COSMETIC("HUD.Carrots.UseMargins"), 0) != 0) {
                                CarrotsMargins_X = Right_HUD_Margin;
                            };
                            CarrotsPosX = OTRGetDimensionFromRightEdge(
                                CVarGetInteger(CVAR_COSMETIC("HUD.Carrots.PosX"), 0) + CarrotsMargins_X);
                        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Carrots.PosType"), 0) == ANCHOR_NONE) {
                            CarrotsPosX = CVarGetInteger(CVAR_COSMETIC("HUD.Carrots.PosX"), 0);
                        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Carrots.PosType"), 0) == HIDDEN) {
                            CarrotsPosX = -9999;
                        }
                    }
                    for (svar1 = 1, svar5 = CarrotsPosX; svar1 < 7; svar1++, svar5 += 16) {
                        // Carrot Color (based on availability)
                        if ((interfaceCtx->numHorseBoosts == 0) || (interfaceCtx->numHorseBoosts < svar1)) {
                            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 150, 255, interfaceCtx->aAlpha);
                        } else {
                            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->aAlpha);
                        }

                        gSPWideTextureRectangle(OVERLAY_DISP++, svar5 << 2, CarrotsPosY << 2, (svar5 + 16) << 2,
                                                (CarrotsPosY + 16) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
                    }
                }
            } else {
                // Score for the Horseback Archery
                s32 X_Margins_Archery;
                if (CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.UseMargins"), 0) != 0) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.PosType"), 0) == ORIGINAL_LOCATION) {
                        X_Margins_Archery = Right_HUD_Margin;
                    };
                } else {
                    X_Margins_Archery = 0;
                }
                s16 ArcheryPos_Y = ZREG(15);
                s16 ArcheryPos_X = OTRGetRectDimensionFromRightEdge(WREG(32) + X_Margins_Archery);

                if (CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.PosType"), 0) != ORIGINAL_LOCATION) {
                    ArcheryPos_Y = CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.PosY"), 0);
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.PosType"), 0) == ANCHOR_LEFT) {
                        if (CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.UseMargins"), 0) != 0) {
                            X_Margins_Archery = Left_HUD_Margin;
                        };
                        ArcheryPos_X = OTRGetRectDimensionFromLeftEdge(
                            CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.PosX"), 0) + X_Margins_Archery);
                    } else if (CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.PosType"), 0) == ANCHOR_RIGHT) {
                        if (CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.UseMargins"), 0) != 0) {
                            X_Margins_Archery = Right_HUD_Margin;
                        };
                        ArcheryPos_X = OTRGetRectDimensionFromRightEdge(
                            CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.PosX"), 0) + X_Margins_Archery);
                    } else if (CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.PosType"), 0) == ANCHOR_NONE) {
                        ArcheryPos_X =
                            CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.PosX"), 0) + 204 + X_Margins_Archery;
                    } else if (CVarGetInteger(CVAR_COSMETIC("HUD.ArcheryScore.PosType"), 0) == HIDDEN) {
                        ArcheryPos_X = -9999;
                    }
                }

                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->bAlpha);

                // Target Icon
                gDPLoadTextureBlock(OVERLAY_DISP++, gArcheryScoreIconTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 24, 16, 0,
                                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                    G_TX_NOLOD, G_TX_NOLOD);

                gSPWideTextureRectangle(OVERLAY_DISP++, (ArcheryPos_X + 28) << 2, ArcheryPos_Y << 2,
                                        (ArcheryPos_X + 52) << 2, (ArcheryPos_Y + 16) << 2, G_TX_RENDERTILE, 0, 0,
                                        1 << 10, 1 << 10);

                // Score Counter
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE,
                                  TEXEL0, 0, PRIMITIVE, 0);

                ArcheryPos_X = ArcheryPos_X + 6 * 9;

                for (svar1 = svar2 = 0; svar1 < 4; svar1++) {
                    if (sHBAScoreDigits[svar1] != 0 || (svar2 != 0) || (svar1 >= 3)) {
                        OVERLAY_DISP =
                            Gfx_TextureI8(OVERLAY_DISP, digitTextures[sHBAScoreDigits[svar1]], 8, 16, ArcheryPos_X,
                                          (ArcheryPos_Y - 2), digitWidth[0], VREG(42), VREG(43) << 1, VREG(43) << 1);
                        ArcheryPos_X += 9;
                        svar2++;
                    }
                }

                gDPPipeSync(OVERLAY_DISP++);
                gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
            }
        }

        if ((gSaveContext.subTimerState == SUBTIMER_STATE_RESPAWN) &&
            (Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT)) {
            // Trade quest timer reached 0
            sSubTimerStateTimer = 40;
            gSaveContext.cutsceneIndex = 0;
            play->transitionTrigger = TRANS_TRIGGER_START;
            play->transitionType = TRANS_TYPE_FADE_WHITE;
            gSaveContext.subTimerState = SUBTIMER_STATE_OFF;

            if ((gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI) &&
                (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_MASTER) &&
                (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_BGS) &&
                (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KNIFE)) {
                if (gSaveContext.buttonStatus[0] != BTN_ENABLED) {
                    gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];
                    Interface_RandoRestoreSwordless();
                } else {
                    gSaveContext.equips.buttonItems[0] = ITEM_NONE;
                }
            }

            // Revert any spoiling trade quest items
            if (GameInteractor_Should(VB_REVERT_SPOILING_ITEMS, true)) {
                for (svar1 = 0; svar1 < ARRAY_COUNT(gSpoilingItems); svar1++) {
                    if (INV_CONTENT(ITEM_TRADE_ADULT) == gSpoilingItems[svar1]) {
                        gSaveContext.eventInf[0] &= 0x7F80;
                        osSyncPrintf("EVENT_INF=%x\n", gSaveContext.eventInf[0]);
                        play->nextEntranceIndex = spoilingItemEntrances[svar1];
                        INV_CONTENT(gSpoilingItemReverts[svar1]) = gSpoilingItemReverts[svar1];

                        for (svar2 = 1; svar2 < ARRAY_COUNT(gSaveContext.equips.buttonItems); svar2++) {
                            if (gSaveContext.equips.buttonItems[svar2] == gSpoilingItems[svar1]) {
                                gSaveContext.equips.buttonItems[svar2] = gSpoilingItemReverts[svar1];
                                Interface_LoadItemIcon1(play, svar2);
                            }
                        }
                    }
                }
            }
        }

        if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) &&
            (play->gameOverCtx.state == GAMEOVER_INACTIVE) && (msgCtx->msgMode == MSGMODE_NONE) &&
            !(player->stateFlags2 & PLAYER_STATE2_ATTEMPT_PLAY_FOR_ACTOR) &&
            (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->transitionMode == TRANS_MODE_OFF) &&
            !Play_InCsMode(play) && (gSaveContext.minigameState != 1) && (play->shootingGalleryStatus <= 1) &&
            !((play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY) && Flags_GetSwitch(play, 0x38))) {
            timerId = TIMER_ID_MAIN;
            switch (gSaveContext.timerState) {
                case TIMER_STATE_ENV_HAZARD_INIT:
                    sTimerStateTimer = 20;
                    sTimerNextSecondTimer = 20;
                    gSaveContext.timerSeconds = gSaveContext.health >> 1;
                    gSaveContext.timerState = TIMER_STATE_ENV_HAZARD_PREVIEW;
                    break;
                case TIMER_STATE_ENV_HAZARD_PREVIEW:
                    sTimerStateTimer--;
                    if (sTimerStateTimer == 0) {
                        sTimerStateTimer = 20;
                        gSaveContext.timerState = TIMER_STATE_ENV_HAZARD_MOVE;
                    }
                    break;
                case TIMER_STATE_DOWN_INIT:
                case TIMER_STATE_UP_INIT:
                    sTimerStateTimer = 20;
                    sTimerNextSecondTimer = 20;
                    if (gSaveContext.timerState == TIMER_STATE_DOWN_INIT) {
                        gSaveContext.timerState = TIMER_STATE_DOWN_PREVIEW;
                    } else {
                        gSaveContext.timerState = TIMER_STATE_UP_PREVIEW;
                    }
                    break;
                case TIMER_STATE_DOWN_PREVIEW:
                case TIMER_STATE_UP_PREVIEW:
                    sTimerStateTimer--;
                    if (sTimerStateTimer == 0) {
                        sTimerStateTimer = 20;
                        if (gSaveContext.timerState == TIMER_STATE_DOWN_PREVIEW) {
                            gSaveContext.timerState = TIMER_STATE_DOWN_MOVE;
                        } else {
                            gSaveContext.timerState = TIMER_STATE_UP_MOVE;
                        }
                    }
                    break;
                case TIMER_STATE_ENV_HAZARD_MOVE:
                case TIMER_STATE_DOWN_MOVE:
                    svar1 = (gSaveContext.timerX[TIMER_ID_MAIN] - 26) / sTimerStateTimer;
                    gSaveContext.timerX[TIMER_ID_MAIN] -= svar1;

                    if (gSaveContext.healthCapacity > 0xA0) {
                        svar1 = (gSaveContext.timerY[TIMER_ID_MAIN] - 54) / sTimerStateTimer;
                    } else {
                        svar1 = (gSaveContext.timerY[TIMER_ID_MAIN] - 46) / sTimerStateTimer;
                    }
                    gSaveContext.timerY[TIMER_ID_MAIN] -= svar1;

                    sTimerStateTimer--;
                    if (sTimerStateTimer == 0) {
                        sTimerStateTimer = 20;
                        gSaveContext.timerX[TIMER_ID_MAIN] = 26;

                        if (gSaveContext.healthCapacity > 0xA0) {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 54;
                        } else {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 46;
                        }

                        if (gSaveContext.timerState == TIMER_STATE_ENV_HAZARD_MOVE) {
                            gSaveContext.timerState = TIMER_STATE_ENV_HAZARD_TICK;
                        } else {
                            gSaveContext.timerState = TIMER_STATE_DOWN_TICK;
                        }
                    }
                case TIMER_STATE_ENV_HAZARD_TICK:
                case TIMER_STATE_DOWN_TICK:
                    if ((gSaveContext.timerState == TIMER_STATE_ENV_HAZARD_TICK) ||
                        (gSaveContext.timerState == TIMER_STATE_DOWN_TICK)) {
                        if (gSaveContext.healthCapacity > 0xA0) {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 54;
                        } else {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 46;
                        }
                    }

                    if ((gSaveContext.timerState >= TIMER_STATE_ENV_HAZARD_MOVE) && (msgCtx->msgLength == 0)) {
                        sTimerNextSecondTimer--;
                        if (sTimerNextSecondTimer == 0) {
                            if (gSaveContext.timerSeconds != 0) {
                                gSaveContext.timerSeconds--;
                            }

                            sTimerNextSecondTimer = 20;

                            if (gSaveContext.timerSeconds == 0) {
                                gSaveContext.timerState = TIMER_STATE_STOP;
                                if (sEnvHazardActive) {
                                    gSaveContext.health = 0;
                                    play->damagePlayer(play, GET_PLAYER(play), -(gSaveContext.health + 2));
                                }
                                sEnvHazardActive = false;
                            } else if (gSaveContext.timerSeconds > 60) {
                                if (timerDigits[4] == 1) {
                                    Audio_PlaySoundGeneral(NA_SE_SY_MESSAGE_WOMAN, &gSfxDefaultPos, 4,
                                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                                           &gSfxDefaultReverb);
                                }
                            } else if (gSaveContext.timerSeconds >= 11) {
                                if (timerDigits[4] & 1) {
                                    Audio_PlaySoundGeneral(NA_SE_SY_WARNING_COUNT_N, &gSfxDefaultPos, 4,
                                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                                           &gSfxDefaultReverb);
                                }
                            } else {
                                Audio_PlaySoundGeneral(NA_SE_SY_WARNING_COUNT_E, &gSfxDefaultPos, 4,
                                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                                       &gSfxDefaultReverb);
                            }
                        }
                    }
                    break;
                case TIMER_STATE_UP_MOVE:
                    svar1 = (gSaveContext.timerX[TIMER_ID_MAIN] - 26) / sTimerStateTimer;
                    gSaveContext.timerX[TIMER_ID_MAIN] -= svar1;

                    if (gSaveContext.healthCapacity > 0xA0) {
                        svar1 = (gSaveContext.timerY[TIMER_ID_MAIN] - 54) / sTimerStateTimer;
                    } else {
                        svar1 = (gSaveContext.timerY[TIMER_ID_MAIN] - 46) / sTimerStateTimer;
                    }
                    gSaveContext.timerY[TIMER_ID_MAIN] -= svar1;

                    sTimerStateTimer--;
                    if (sTimerStateTimer == 0) {
                        sTimerStateTimer = 20;
                        gSaveContext.timerX[TIMER_ID_MAIN] = 26;
                        if (gSaveContext.healthCapacity > 0xA0) {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 54;
                        } else {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 46;
                        }

                        gSaveContext.timerState = TIMER_STATE_UP_TICK;
                    }
                case TIMER_STATE_UP_TICK:
                    if (gSaveContext.timerState == TIMER_STATE_UP_TICK) {
                        if (gSaveContext.healthCapacity > 0xA0) {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 54;
                        } else {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 46;
                        }
                    }

                    if (gSaveContext.timerState >= TIMER_STATE_ENV_HAZARD_MOVE) {
                        sTimerNextSecondTimer--;
                        if (sTimerNextSecondTimer == 0) {
                            gSaveContext.timerSeconds++;
                            sTimerNextSecondTimer = 20;

                            if (gSaveContext.timerSeconds == 3599) {
                                sTimerStateTimer = 40;
                                gSaveContext.timerState = TIMER_STATE_UP_FREEZE;
                            } else {
                                Audio_PlaySoundGeneral(NA_SE_SY_WARNING_COUNT_N, &gSfxDefaultPos, 4,
                                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                                       &gSfxDefaultReverb);
                            }
                        }
                    }
                    break;
                case TIMER_STATE_STOP:
                    if (gSaveContext.subTimerState != SUBTIMER_STATE_OFF) {
                        sSubTimerStateTimer = 20;
                        sSubTimerNextSecondTimer = 20;
                        gSaveContext.timerX[TIMER_ID_SUB] = 140;
                        gSaveContext.timerY[TIMER_ID_SUB] = 80;

                        if (gSaveContext.subTimerState <= SUBTIMER_STATE_STOP) {
                            gSaveContext.subTimerState = SUBTIMER_STATE_DOWN_PREVIEW;
                        } else {
                            gSaveContext.subTimerState = SUBTIMER_STATE_UP_PREVIEW;
                        }

                        gSaveContext.timerState = TIMER_STATE_OFF;
                    } else {
                        gSaveContext.timerState = TIMER_STATE_OFF;
                    }
                case TIMER_STATE_UP_FREEZE:
                    break;
                default:
                    timerId = TIMER_ID_SUB;
                    switch (gSaveContext.subTimerState) {
                        case SUBTIMER_STATE_DOWN_INIT:
                        case SUBTIMER_STATE_UP_INIT:
                            sSubTimerStateTimer = 20;
                            sSubTimerNextSecondTimer = 20;
                            gSaveContext.timerX[TIMER_ID_SUB] = 140;
                            gSaveContext.timerY[TIMER_ID_SUB] = 80;
                            if (gSaveContext.subTimerState == SUBTIMER_STATE_DOWN_INIT) {
                                gSaveContext.subTimerState = SUBTIMER_STATE_DOWN_PREVIEW;
                            } else {
                                gSaveContext.subTimerState = SUBTIMER_STATE_UP_PREVIEW;
                            }
                            break;
                        case SUBTIMER_STATE_DOWN_PREVIEW:
                        case SUBTIMER_STATE_UP_PREVIEW:
                            sSubTimerStateTimer--;
                            if (sSubTimerStateTimer == 0) {
                                sSubTimerStateTimer = 20;
                                if (gSaveContext.subTimerState == SUBTIMER_STATE_DOWN_PREVIEW) {
                                    gSaveContext.subTimerState = SUBTIMER_STATE_DOWN_MOVE;
                                } else {
                                    gSaveContext.subTimerState = SUBTIMER_STATE_UP_MOVE;
                                }
                            }
                            break;
                        case SUBTIMER_STATE_DOWN_MOVE:
                        case SUBTIMER_STATE_UP_MOVE:
                            osSyncPrintf("event_xp[1]=%d,  event_yp[1]=%d  TOTAL_EVENT_TM=%d\n",
                                         svar5 = gSaveContext.timerX[TIMER_ID_SUB],
                                         svar2 = gSaveContext.timerY[TIMER_ID_SUB], gSaveContext.subTimerSeconds);
                            svar1 = (gSaveContext.timerX[TIMER_ID_SUB] - 26) / sSubTimerStateTimer;
                            gSaveContext.timerX[TIMER_ID_SUB] -= svar1;
                            if (gSaveContext.healthCapacity > 0xA0) {
                                svar1 = (gSaveContext.timerY[TIMER_ID_SUB] - 54) / sSubTimerStateTimer;
                            } else {
                                svar1 = (gSaveContext.timerY[TIMER_ID_SUB] - 46) / sSubTimerStateTimer;
                            }
                            gSaveContext.timerY[TIMER_ID_SUB] -= svar1;

                            sSubTimerStateTimer--;
                            if (sSubTimerStateTimer == 0) {
                                sSubTimerStateTimer = 20;
                                gSaveContext.timerX[TIMER_ID_SUB] = 26;

                                if (gSaveContext.healthCapacity > 0xA0) {
                                    gSaveContext.timerY[TIMER_ID_SUB] = 54;
                                } else {
                                    gSaveContext.timerY[TIMER_ID_SUB] = 46;
                                }

                                if (gSaveContext.subTimerState == SUBTIMER_STATE_DOWN_MOVE) {
                                    gSaveContext.subTimerState = SUBTIMER_STATE_DOWN_TICK;
                                } else {
                                    gSaveContext.subTimerState = SUBTIMER_STATE_UP_TICK;
                                }
                            }
                        case SUBTIMER_STATE_DOWN_TICK:
                        case SUBTIMER_STATE_UP_TICK:
                            if ((gSaveContext.subTimerState == SUBTIMER_STATE_DOWN_TICK) ||
                                (gSaveContext.subTimerState == SUBTIMER_STATE_UP_TICK)) {
                                if (gSaveContext.healthCapacity > 0xA0) {
                                    gSaveContext.timerY[TIMER_ID_SUB] = 54;
                                } else {
                                    gSaveContext.timerY[TIMER_ID_SUB] = 46;
                                }
                            }

                            if (gSaveContext.subTimerState >= SUBTIMER_STATE_DOWN_MOVE) {
                                sSubTimerNextSecondTimer--;
                                if (sSubTimerNextSecondTimer == 0) {
                                    sSubTimerNextSecondTimer = 20;
                                    if (gSaveContext.subTimerState == SUBTIMER_STATE_DOWN_TICK) {
                                        gSaveContext.subTimerSeconds--;
                                        osSyncPrintf("TOTAL_EVENT_TM=%d\n", gSaveContext.subTimerSeconds);

                                        if (gSaveContext.subTimerSeconds <= 0) {
                                            if (!Flags_GetSwitch(play, 0x37) ||
                                                ((play->sceneNum != SCENE_GANON_BOSS) &&
                                                 (play->sceneNum != SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR) &&
                                                 (play->sceneNum != SCENE_GANONS_TOWER_COLLAPSE_INTERIOR) &&
                                                 (play->sceneNum != SCENE_INSIDE_GANONS_CASTLE_COLLAPSE))) {
                                                sSubTimerStateTimer = 40;
                                                gSaveContext.subTimerState = SUBTIMER_STATE_RESPAWN;
                                                gSaveContext.cutsceneIndex = 0;
                                                Message_StartTextbox(play, 0x71B0, NULL);
                                                Player_SetCsActionWithHaltedActors(play, NULL, 8);
                                            } else {
                                                sSubTimerStateTimer = 40;
                                                gSaveContext.subTimerState = SUBTIMER_STATE_STOP;
                                            }
                                        } else if (gSaveContext.subTimerSeconds > 60) {
                                            if (timerDigits[4] == 1) {
                                                Audio_PlaySoundGeneral(NA_SE_SY_MESSAGE_WOMAN, &gSfxDefaultPos, 4,
                                                                       &gSfxDefaultFreqAndVolScale,
                                                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                                            }
                                        } else if (gSaveContext.subTimerSeconds > 10) {
                                            if ((timerDigits[4] & 1)) {
                                                Audio_PlaySoundGeneral(NA_SE_SY_WARNING_COUNT_N, &gSfxDefaultPos, 4,
                                                                       &gSfxDefaultFreqAndVolScale,
                                                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                                            }
                                        } else {
                                            Audio_PlaySoundGeneral(NA_SE_SY_WARNING_COUNT_E, &gSfxDefaultPos, 4,
                                                                   &gSfxDefaultFreqAndVolScale,
                                                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                                        }
                                    } else {
                                        gSaveContext.subTimerSeconds++;
                                        if (gSaveContext.eventInf[1] & 1) {
                                            if (gSaveContext.subTimerSeconds == 240) {
                                                Message_StartTextbox(play, 0x6083, NULL);
                                                gSaveContext.eventInf[1] &= ~1;
                                                gSaveContext.subTimerState = SUBTIMER_STATE_OFF;
                                            }
                                        }
                                    }

                                    if ((gSaveContext.subTimerSeconds % 60) == 0) {
                                        Audio_PlaySoundGeneral(NA_SE_SY_WARNING_COUNT_N, &gSfxDefaultPos, 4,
                                                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                                               &gSfxDefaultReverb);
                                    }
                                }
                            }
                            break;
                        case 6:
                            sSubTimerStateTimer--;
                            if (sSubTimerStateTimer == 0) {
                                gSaveContext.subTimerState = SUBTIMER_STATE_OFF;
                            }
                            break;
                    }
                    break;
            }

            if (((gSaveContext.timerState != TIMER_STATE_OFF) && (gSaveContext.timerState != TIMER_STATE_STOP)) ||
                (gSaveContext.subTimerState != SUBTIMER_STATE_OFF)) {
                timerDigits[0] = timerDigits[1] = svar2 = timerDigits[3] = 0;
                timerDigits[2] = 10; // digit 10 is used as ':' (colon)

                if (gSaveContext.timerState != TIMER_STATE_OFF) {
                    timerDigits[4] = gSaveContext.timerSeconds;
                } else {
                    timerDigits[4] = gSaveContext.subTimerSeconds;
                }

                while (timerDigits[4] >= 60) {
                    timerDigits[1]++;
                    if (timerDigits[1] >= 10) {
                        timerDigits[0]++;
                        timerDigits[1] -= 10;
                    }
                    timerDigits[4] -= 60;
                }

                while (timerDigits[4] >= 10) {
                    timerDigits[3]++;
                    timerDigits[4] -= 10;
                }

                // Clock Icon
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);
                gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);
                s32 X_Margins_Timer;
                if (CVarGetInteger(CVAR_COSMETIC("HUD.Timers.UseMargins"), 0) != 0) {
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.Timers.PosType"), 0) == ORIGINAL_LOCATION) {
                        X_Margins_Timer = Left_HUD_Margin;
                    };
                } else {
                    X_Margins_Timer = 0;
                }
                svar5 = OTRGetRectDimensionFromLeftEdge(gSaveContext.timerX[timerId] + X_Margins_Timer);
                svar2 = gSaveContext.timerY[timerId];
                if (CVarGetInteger(CVAR_COSMETIC("HUD.Timers.PosType"), 0) != ORIGINAL_LOCATION) {
                    svar2 = (CVarGetInteger(CVAR_COSMETIC("HUD.Timers.PosY"), 0));
                    if (CVarGetInteger(CVAR_COSMETIC("HUD.Timers.PosType"), 0) == ANCHOR_LEFT) {
                        if (CVarGetInteger(CVAR_COSMETIC("HUD.Timers.UseMargins"), 0) != 0) {
                            X_Margins_Timer = Left_HUD_Margin;
                        };
                        svar5 = OTRGetRectDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Timers.PosX"), 0) +
                                                                X_Margins_Timer);
                    } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Timers.PosType"), 0) == ANCHOR_RIGHT) {
                        if (CVarGetInteger(CVAR_COSMETIC("HUD.Timers.UseMargins"), 0) != 0) {
                            X_Margins_Timer = Right_HUD_Margin;
                        };
                        svar5 = OTRGetRectDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Timers.PosX"), 0) +
                                                                 X_Margins_Timer);
                    } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Timers.PosType"), 0) == ANCHOR_NONE) {
                        svar5 = CVarGetInteger(CVAR_COSMETIC("HUD.Timers.PosX"), 0) + 204 + X_Margins_Timer;
                    } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Timers.PosType"), 0) == HIDDEN) {
                        svar5 = -9999;
                    }
                }

                OVERLAY_DISP =
                    Gfx_TextureIA8(OVERLAY_DISP, gClockIconTex, 16, 16, svar5, svar2 + 2, 16, 16, 1 << 10, 1 << 10);

                // Timer Counter
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE,
                                  TEXEL0, 0, PRIMITIVE, 0);

                if (gSaveContext.timerState != TIMER_STATE_OFF) {
                    if ((gSaveContext.timerSeconds < 10) && (gSaveContext.timerState <= TIMER_STATE_STOP)) {
                        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 50, 0, 255);
                    } else {
                        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);
                    }
                } else {
                    if ((gSaveContext.subTimerSeconds < 10) && (gSaveContext.subTimerState <= SUBTIMER_STATE_RESPAWN)) {
                        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 50, 0, 255);
                    } else {
                        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 0, 255);
                    }
                }

                for (svar1 = 0; svar1 < 5; svar1++) {
                    // clang-format off
                    //svar5 = svar5 + 8;
                    //svar5 = OTRGetRectDimensionFromLeftEdge(gSaveContext.timerX[timerId]);
                    OVERLAY_DISP = Gfx_TextureI8(OVERLAY_DISP, digitTextures[timerDigits[svar1]], 8, 16,
                                      svar5 + timerDigitLeftPos[svar1],
                                      svar2, digitWidth[svar1], VREG(42), VREG(43) << 1,
                                      VREG(43) << 1);
                    // clang-format on
                }
            }
        }
    }

    if (pauseCtx->debugState == 3) {
        FlagSet_Update(play);
    }

    if (interfaceCtx->unk_244 != 0) {
        gDPPipeSync(OVERLAY_DISP++);
        gSPDisplayList(OVERLAY_DISP++, sSetupDL_80125A60);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, interfaceCtx->unk_244);
        gDPFillRectangle(OVERLAY_DISP++, 0, 0, gScreenWidth - 1, gScreenHeight - 1);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void Interface_DrawTotalGameplayTimer(PlayState* play) {
    // Draw timer based on the Gameplay Stats total time.
    if (GameInteractor_Should(VB_SHOW_GAMEPLAY_TIMER,
                              CVarGetInteger(CVAR_GAMEPLAY_STATS("ShowIngameTimer"), 0) && gSaveContext.fileNum >= 0 &&
                                  gSaveContext.fileNum <= 2,
                              play)) {
        s32 X_Margins_Timer = 0;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.IGT.UseMargins"), 0) != 0) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.IGT.PosType"), 0) == ORIGINAL_LOCATION) {
                X_Margins_Timer = Left_HUD_Margin;
            };
        }
        s32 rectLeftOri = OTRGetRectDimensionFromLeftEdge(24 + X_Margins_Timer);
        s32 rectTopOri = 73;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.IGT.PosType"), 0) != ORIGINAL_LOCATION) {
            rectTopOri = (CVarGetInteger(CVAR_COSMETIC("HUD.IGT.PosY"), 0));
            if (CVarGetInteger(CVAR_COSMETIC("HUD.IGT.PosType"), 0) == ANCHOR_LEFT) {
                if (CVarGetInteger(CVAR_COSMETIC("HUD.IGT.UseMargins"), 0) != 0) {
                    X_Margins_Timer = Left_HUD_Margin;
                };
                rectLeftOri =
                    OTRGetRectDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.IGT.PosX"), 0) + X_Margins_Timer);
            } else if (CVarGetInteger(CVAR_COSMETIC("HUD.IGT.PosType"), 0) == ANCHOR_RIGHT) {
                if (CVarGetInteger(CVAR_COSMETIC("HUD.IGT.UseMargins"), 0) != 0) {
                    X_Margins_Timer = Right_HUD_Margin;
                };
                rectLeftOri = OTRGetRectDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.IGT.PosX"), 0) +
                                                               X_Margins_Timer);
            } else if (CVarGetInteger(CVAR_COSMETIC("HUD.IGT.PosType"), 0) == ANCHOR_NONE) {
                rectLeftOri = CVarGetInteger(CVAR_COSMETIC("HUD.IGT.PosX"), 0) + 204 + X_Margins_Timer;
            } else if (CVarGetInteger(CVAR_COSMETIC("HUD.IGT.PosType"), 0) == HIDDEN) {
                rectLeftOri = -9999;
            }
        }

        s32 rectLeft;
        s32 rectTop;
        s32 rectWidth = 8;
        s32 rectHeightOri = 16;
        s32 rectHeight;

        OPEN_DISPS(play->state.gfxCtx);

        gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                          PRIMITIVE, 0);

        gDPSetOtherMode(OVERLAY_DISP++,
                        G_AD_DISABLE | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_IA16 | G_TL_TILE |
                            G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                        G_AC_NONE | G_ZS_PRIM | G_RM_XLU_SURF | G_RM_XLU_SURF2);

        char* totalTimeText = GameplayStats_GetCurrentTime();
        size_t textLength = strlen(totalTimeText);
        uint16_t textureIndex = 0;

        for (size_t i = 0; i < textLength; i++) {
            if (totalTimeText[i] == ':' || totalTimeText[i] == '.') {
                textureIndex = 10;
            } else {
                textureIndex = totalTimeText[i] - 48;
            }

            rectLeft = rectLeftOri + (i * 8);
            rectTop = rectTopOri;
            rectHeight = rectHeightOri;

            // Load correct digit (or : symbol)
            gDPLoadTextureBlock(OVERLAY_DISP++, ((u8*)digitTextures[textureIndex]), G_IM_FMT_I, G_IM_SIZ_8b, rectWidth,
                                rectHeight, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                                G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

            // Create dot image from the colon image.
            if (totalTimeText[i] == '.') {
                rectHeight = rectHeight / 2;
                rectTop += 5;
                rectLeft -= 1;
            }

            // Draw text shadow
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, 255);
            gDPSetEnvColor(OVERLAY_DISP++, 255, 255, 255, 255);
            gSPWideTextureRectangle(OVERLAY_DISP++, rectLeft << 2, rectTop << 2, (rectLeft + rectWidth) << 2,
                                    (rectTop + rectHeight) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);

            // Draw regular text. Change color based on if the timer is paused, running or the game is completed.
            if (gSaveContext.ship.stats.gameComplete) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 120, 255, 0, 255);
            } else if (IS_BOSS_RUSH && gSaveContext.ship.quest.data.bossRush.isPaused &&
                       !gSaveContext.ship.stats.rtaTiming) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 150, 150, 150, 255);
            } else {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);
            }

            // Offset text so underlaying shadow is to the bottom right of the text.
            rectLeft -= 1;
            rectTop -= 1;

            gSPWideTextureRectangle(OVERLAY_DISP++, rectLeft << 2, rectTop << 2, (rectLeft + rectWidth) << 2,
                                    (rectTop + rectHeight) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        }
        free(totalTimeText);

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void Interface_Update(PlayState* play) {
    static u8 D_80125B60 = 0;
    static s16 sPrevTimeIncrement = 0;
    MessageContext* msgCtx = &play->msgCtx;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Player* player = GET_PLAYER(play);
    s16 alpha;
    s16 alpha1;
    u16 action;
    Input* debugInput = &play->state.input[2];

    Top_HUD_Margin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.T"), 0);
    Left_HUD_Margin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.L"), 0);
    Right_HUD_Margin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.R"), 0);
    Bottom_HUD_Margin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.B"), 0);

    GameInteractor_ExecuteOnInterfaceUpdate();

    bool isPal = ResourceMgr_GetGameRegion(0) == GAME_REGION_PAL;

    if ((CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0) ? 1
                                                                      : CVarGetInteger(
                                                                            CVAR_ENHANCEMENT(
                                                                                "LocalMultiplayer.PlayerCount"),
                                                                            2)) <
        3) {
        if (CHECK_BTN_ALL(debugInput->press.button, BTN_DLEFT)) {
            gSaveContext.language = LANGUAGE_ENG;
            CVarSetInteger(CVAR_SETTING("Languages"), LANGUAGE_ENG);
            osSyncPrintf("J_N=%x J_N=%x\n", gSaveContext.language, &gSaveContext.language);
        } else if (CHECK_BTN_ALL(debugInput->press.button, BTN_DUP) && sGerMessageEntryTablePtr != NULL) {
            gSaveContext.language = LANGUAGE_GER;
            CVarSetInteger(CVAR_SETTING("Languages"), LANGUAGE_GER);
            osSyncPrintf("J_N=%x J_N=%x\n", gSaveContext.language, &gSaveContext.language);
        } else if (CHECK_BTN_ALL(debugInput->press.button, BTN_DRIGHT) && sFraMessageEntryTablePtr != NULL) {
            gSaveContext.language = LANGUAGE_FRA;
            CVarSetInteger(CVAR_SETTING("Languages"), LANGUAGE_FRA);
            osSyncPrintf("J_N=%x J_N=%x\n", gSaveContext.language, &gSaveContext.language);
        } else if (CHECK_BTN_ALL(debugInput->press.button, BTN_DDOWN) && sJpnMessageEntryTablePtr != NULL) {
            // Add this in to have an equivalent ntsc language debugging feature
            gSaveContext.language = LANGUAGE_JPN;
            CVarSetInteger(CVAR_SETTING("Languages"), LANGUAGE_JPN);
            osSyncPrintf("J_N=%x J_N=%x\n", gSaveContext.language, &gSaveContext.language);
        }
    }

    if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0)) {
        if ((gSaveContext.minigameState == 1) || (gSaveContext.sceneSetupIndex < 4) ||
            ((play->sceneNum == SCENE_LON_LON_RANCH) && (gSaveContext.sceneSetupIndex == 4))) {
            if ((msgCtx->msgMode == MSGMODE_NONE) ||
                ((msgCtx->msgMode != MSGMODE_NONE) && (play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY))) {
                if (play->gameOverCtx.state == GAMEOVER_INACTIVE) {
                    func_80083108(play);
                }
            }
        }
    }

    switch (gSaveContext.unk_13E8) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            alpha = 255 - (gSaveContext.unk_13EC << 5);
            if (alpha < 0) {
                alpha = 0;
            }

            func_80082850(play, alpha);
            gSaveContext.unk_13EC++;

            if (alpha == 0) {
                gSaveContext.unk_13E8 = 0;
            }
            break;
        case 50:
            alpha = 255 - (gSaveContext.unk_13EC << 5);
            if (alpha < 0) {
                alpha = 0;
            }

            alpha1 = 0xFF - alpha;
            if (alpha1 >= 0xFF) {
                alpha1 = 0xFF;
            }

            osSyncPrintf("case 50 : alpha=%d  alpha1=%d\n", alpha, alpha1);
            func_80082644(play, alpha1);

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha1;
            }

            if (interfaceCtx->magicAlpha != 255) {
                interfaceCtx->magicAlpha = alpha1;
            }

            switch (play->sceneNum) {
                case SCENE_HYRULE_FIELD:
                case SCENE_KAKARIKO_VILLAGE:
                case SCENE_GRAVEYARD:
                case SCENE_ZORAS_RIVER:
                case SCENE_KOKIRI_FOREST:
                case SCENE_SACRED_FOREST_MEADOW:
                case SCENE_LAKE_HYLIA:
                case SCENE_ZORAS_DOMAIN:
                case SCENE_ZORAS_FOUNTAIN:
                case SCENE_GERUDO_VALLEY:
                case SCENE_LOST_WOODS:
                case SCENE_DESERT_COLOSSUS:
                case SCENE_GERUDOS_FORTRESS:
                case SCENE_HAUNTED_WASTELAND:
                case SCENE_HYRULE_CASTLE:
                case SCENE_DEATH_MOUNTAIN_TRAIL:
                case SCENE_DEATH_MOUNTAIN_CRATER:
                case SCENE_GORON_CITY:
                case SCENE_LON_LON_RANCH:
                case SCENE_OUTSIDE_GANONS_CASTLE:
                    if (interfaceCtx->minimapAlpha < 170) {
                        interfaceCtx->minimapAlpha = alpha1;
                    } else {
                        interfaceCtx->minimapAlpha = 170;
                    }
                    break;
                default:
                    if (interfaceCtx->minimapAlpha != 255) {
                        interfaceCtx->minimapAlpha = alpha1;
                    }
                    break;
            }

            gSaveContext.unk_13EC++;
            if (alpha1 == 0xFF) {
                gSaveContext.unk_13E8 = 0;
            }

            break;
        case 52:
            gSaveContext.unk_13E8 = 1;
            func_80082850(play, 0);
            gSaveContext.unk_13E8 = 0;
        default:
            break;
    }

    Map_Update(play);

    if (gSaveContext.healthAccumulator != 0) {
        gSaveContext.healthAccumulator -= 4;
        gSaveContext.health += 4;

        if ((gSaveContext.health & 0xF) < 4) {
            Audio_PlaySoundGeneral(NA_SE_SY_HP_RECOVER, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }

        osSyncPrintf("now_life=%d  max_life=%d\n", gSaveContext.health, gSaveContext.healthCapacity);

        if (gSaveContext.health >= gSaveContext.healthCapacity) {
            gSaveContext.health = gSaveContext.healthCapacity;
            osSyncPrintf("S_Private.now_life=%d  S_Private.max_life=%d\n", gSaveContext.health,
                         gSaveContext.healthCapacity);
            gSaveContext.healthAccumulator = 0;
        }
    }

    HealthMeter_HandleCriticalAlarm(play);
    sEnvHazard = Player_GetEnvironmentalHazard(play);

    if (sEnvHazard == PLAYER_ENV_HAZARD_HOTROOM) {
        if (CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC) == EQUIP_VALUE_TUNIC_GORON ||
            CVarGetInteger(CVAR_CHEAT("SuperTunic"), 0) != 0) {
            sEnvHazard = PLAYER_ENV_HAZARD_NONE;
        }
    } else if ((Player_GetEnvironmentalHazard(play) >= 2) && (Player_GetEnvironmentalHazard(play) < 5)) {
        if (CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC) == EQUIP_VALUE_TUNIC_ZORA ||
            CVarGetInteger(CVAR_CHEAT("SuperTunic"), 0) != 0) {
            sEnvHazard = PLAYER_ENV_HAZARD_NONE;
        }
    }

    HealthMeter_Update(play);

    if ((gSaveContext.timerState >= TIMER_STATE_ENV_HAZARD_MOVE) && (play->pauseCtx.state == 0) &&
        (play->pauseCtx.debugState == 0) && (msgCtx->msgMode == MSGMODE_NONE) &&
        !(player->stateFlags2 & PLAYER_STATE2_ATTEMPT_PLAY_FOR_ACTOR) &&
        (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->transitionMode == TRANS_MODE_OFF) &&
        !Play_InCsMode(play)) {}

    if (gSaveContext.rupeeAccumulator != 0) {
        if (gSaveContext.rupeeAccumulator > 0) {
            if (gSaveContext.rupees < CUR_CAPACITY(UPG_WALLET)) {
                gSaveContext.rupeeAccumulator--;
                gSaveContext.rupees++;
                Audio_PlaySoundGeneral(NA_SE_SY_RUPY_COUNT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            } else {
                // "Rupee Amount MAX = %d"
                osSyncPrintf("ルピー数ＭＡＸ = %d\n", CUR_CAPACITY(UPG_WALLET));
                gSaveContext.rupees = CUR_CAPACITY(UPG_WALLET);
                gSaveContext.rupeeAccumulator = 0;
            }
        } else if (gSaveContext.rupees != 0) {
            if (gSaveContext.rupeeAccumulator <= -50) {
                gSaveContext.rupeeAccumulator += 10;
                gSaveContext.rupees -= 10;

                if (gSaveContext.rupees < 0) {
                    gSaveContext.rupees = 0;
                }

                Audio_PlaySoundGeneral(NA_SE_SY_RUPY_COUNT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            } else {
                gSaveContext.rupeeAccumulator++;
                gSaveContext.rupees--;
                Audio_PlaySoundGeneral(NA_SE_SY_RUPY_COUNT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
        } else {
            gSaveContext.rupeeAccumulator = 0;
        }
        if (gSaveContext.rupeeAccumulator == 0 && gSaveContext.ship.pendingSale != ITEM_NONE) {
            u16 tempSaleItem = gSaveContext.ship.pendingSale;
            u16 tempSaleMod = gSaveContext.ship.pendingSaleMod;
            gSaveContext.ship.pendingSale = ITEM_NONE;
            gSaveContext.ship.pendingSaleMod = MOD_NONE;
            if (tempSaleMod == MOD_NONE) {
                GetItemID getItemID = RetrieveGetItemIDFromItemID(tempSaleItem);
                RandomizerGet randomizerGet = RetrieveRandomizerGetFromItemID(tempSaleItem);
                if (getItemID != GI_MAX) {
                    tempSaleItem = getItemID;
                } else {
                    if (randomizerGet != RG_MAX) {
                        tempSaleItem = randomizerGet;
                    }
                    tempSaleMod = MOD_RANDOMIZER;
                }
            }
            GameInteractor_ExecuteOnSaleEndHooks(ItemTable_RetrieveEntry(tempSaleMod, tempSaleItem));
        }
    }

    switch (interfaceCtx->unk_1EC) {
        case 1:
            interfaceCtx->unk_1F4 += 31400.0f / WREG(5);
            if (interfaceCtx->unk_1F4 >= 15700.0f) {
                interfaceCtx->unk_1F4 = -15700.0f;
                interfaceCtx->unk_1EC = 2;
            }
            break;
        case 2:
            interfaceCtx->unk_1F4 += 31400.0f / WREG(5);
            if (interfaceCtx->unk_1F4 >= 0.0f) {
                interfaceCtx->unk_1F4 = 0.0f;
                interfaceCtx->unk_1EC = 0;
                interfaceCtx->unk_1EE = interfaceCtx->unk_1F0;
                action = interfaceCtx->unk_1EE;
                if ((action == DO_ACTION_MAX) || (action == DO_ACTION_MAX + 1)) {
                    action = DO_ACTION_NONE;
                }
                Interface_LoadActionLabel(interfaceCtx, action, 0);
            }
            break;
        case 3:
            interfaceCtx->unk_1F4 += 31400.0f / WREG(5);
            if (interfaceCtx->unk_1F4 >= 15700.0f) {
                interfaceCtx->unk_1F4 = -15700.0f;
                interfaceCtx->unk_1EC = 2;
            }
            break;
        case 4:
            interfaceCtx->unk_1F4 += 31400.0f / WREG(5);
            if (interfaceCtx->unk_1F4 >= 0.0f) {
                interfaceCtx->unk_1F4 = 0.0f;
                interfaceCtx->unk_1EC = 0;
                interfaceCtx->unk_1EE = interfaceCtx->unk_1F0;
                action = interfaceCtx->unk_1EE;
                if ((action == DO_ACTION_MAX) || (action == DO_ACTION_MAX + 1)) {
                    action = DO_ACTION_NONE;
                }
                Interface_LoadActionLabel(interfaceCtx, action, 0);
            }
            break;
    }

    WREG(7) = interfaceCtx->unk_1F4;

    if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) && (msgCtx->msgMode == MSGMODE_NONE) &&
        (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->gameOverCtx.state == GAMEOVER_INACTIVE) &&
        (play->transitionMode == TRANS_MODE_OFF) && ((play->csCtx.state == CS_STATE_IDLE) || !Player_InCsMode(play))) {
        if ((gSaveContext.isMagicAcquired != 0) && (gSaveContext.magicLevel == 0)) {
            gSaveContext.magicLevel = gSaveContext.isDoubleMagicAcquired + 1;
            gSaveContext.magicState = MAGIC_STATE_STEP_CAPACITY;
            osSyncPrintf(VT_FGCOL(YELLOW));
            osSyncPrintf("魔法スター─────ト！！！！！！！！！\n"); // "Magic Start!!!!!!!!!"
            osSyncPrintf("MAGIC_MAX=%d\n", gSaveContext.magicLevel);
            osSyncPrintf("MAGIC_NOW=%d\n", gSaveContext.magic);
            osSyncPrintf("Z_MAGIC_NOW_NOW=%d\n", gSaveContext.magicFillTarget);
            osSyncPrintf("Z_MAGIC_NOW_MAX=%d\n", gSaveContext.magicCapacity);
            osSyncPrintf(VT_RST);
        }

        Interface_UpdateMagicBar(play);

        if (Interface_GetLocalMultiplayerPlayerCount() > 1) {
            s16 localMpCount = Interface_GetLocalMultiplayerPlayerCount();

            for (u8 controllerPort = 2; controllerPort <= localMpCount; controllerPort++) {
                s16* magicState = Magic_GetSecondaryStatePtr(controllerPort);
                s16* magicCapacity = Magic_GetSecondaryCapacityPtr(controllerPort);

                Magic_InitForPort(controllerPort);

                if ((Magic_GetLevelForPort(controllerPort) != 0) && (*magicCapacity == 0) &&
                    (*magicState == MAGIC_STATE_IDLE)) {
                    *magicState = MAGIC_STATE_STEP_CAPACITY;
                }

                if ((Magic_GetLevelForPort(controllerPort) != 0) || (*magicState != MAGIC_STATE_IDLE)) {
                    Magic_UpdateSecondary(play, controllerPort);
                }
            }
        }
    }

    if (gSaveContext.timerState == TIMER_STATE_OFF) {
        if (((sEnvHazard == PLAYER_ENV_HAZARD_HOTROOM) || (sEnvHazard == PLAYER_ENV_HAZARD_UNDERWATER_FLOOR) ||
             (sEnvHazard == PLAYER_ENV_HAZARD_UNDERWATER_FREE)) &&
            ((gSaveContext.health >> 1) != 0)) {
            gSaveContext.timerState = TIMER_STATE_ENV_HAZARD_INIT;
            gSaveContext.timerX[TIMER_ID_MAIN] = 140;
            gSaveContext.timerY[TIMER_ID_MAIN] = 80;
            sEnvHazardActive = true;
        }
    } else {
        if (((sEnvHazard == PLAYER_ENV_HAZARD_NONE) || (sEnvHazard == PLAYER_ENV_HAZARD_SWIMMING)) &&
            (gSaveContext.timerState <= TIMER_STATE_ENV_HAZARD_TICK)) {
            gSaveContext.timerState = TIMER_STATE_OFF;
        }
    }

    if (gSaveContext.minigameState == 1) {
        gSaveContext.minigameScore += interfaceCtx->unk_23C;
        interfaceCtx->unk_23C = 0;

        if (sHBAScoreTier == 0) {
            if (gSaveContext.minigameScore >= 1000) {
                sHBAScoreTier++;
            }
        } else if (sHBAScoreTier == 1) {
            if (gSaveContext.minigameScore >= 1500) {
                sHBAScoreTier++;
            }
        }

        sHBAScoreDigits[0] = sHBAScoreDigits[1] = 0;
        sHBAScoreDigits[2] = 0;
        sHBAScoreDigits[3] = gSaveContext.minigameScore;

        while (sHBAScoreDigits[3] >= 1000) {
            sHBAScoreDigits[0]++;
            sHBAScoreDigits[3] -= 1000;
        }

        while (sHBAScoreDigits[3] >= 100) {
            sHBAScoreDigits[1]++;
            sHBAScoreDigits[3] -= 100;
        }

        while (sHBAScoreDigits[3] >= 10) {
            sHBAScoreDigits[2]++;
            sHBAScoreDigits[3] -= 10;
        }
    }

    if (gSaveContext.sunsSongState != SUNSSONG_INACTIVE) {
        // exit out of ocarina mode after suns song finishes playing
        if ((msgCtx->ocarinaAction != OCARINA_ACTION_CHECK_NOWARP_DONE) &&
            (gSaveContext.sunsSongState == SUNSSONG_START)) {
            play->msgCtx.ocarinaMode = OCARINA_MODE_04;
        }

        // handle suns song in areas where time moves
        if (play->envCtx.timeIncrement != 0) {
            if (gSaveContext.sunsSongState != SUNSSONG_SPEED_TIME) {
                D_80125B60 = 0;
                if ((gSaveContext.dayTime >= 0x4555) && (gSaveContext.dayTime <= 0xC001)) {
                    D_80125B60 = 1;
                }

                gSaveContext.sunsSongState = SUNSSONG_SPEED_TIME;
                sPrevTimeIncrement = gTimeIncrement;
                gTimeIncrement = 400;
            } else if (D_80125B60 == 0) {
                if ((gSaveContext.dayTime >= 0x4555) && (gSaveContext.dayTime <= 0xC001)) {
                    gSaveContext.sunsSongState = SUNSSONG_INACTIVE;
                    gTimeIncrement = sPrevTimeIncrement;
                    play->msgCtx.ocarinaMode = OCARINA_MODE_04;
                }
            } else if (gSaveContext.dayTime > 0xC001) {
                gSaveContext.sunsSongState = SUNSSONG_INACTIVE;
                gTimeIncrement = sPrevTimeIncrement;
                play->msgCtx.ocarinaMode = OCARINA_MODE_04;
            }
        } else if ((play->roomCtx.curRoom.behaviorType1 != ROOM_BEHAVIOR_TYPE1_1) &&
                   (interfaceCtx->restrictions.sunsSong != 3)) {
            if ((gSaveContext.dayTime >= 0x4555) && (gSaveContext.dayTime < 0xC001)) {
                gSaveContext.nextDayTime = 0;
                play->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
                gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
                play->unk_11DE9 = 1;
            } else {
                gSaveContext.nextDayTime = 0x8001;
                play->transitionType = TRANS_TYPE_FADE_WHITE_FAST;
                gSaveContext.nextTransitionType = TRANS_TYPE_FADE_WHITE;
                play->unk_11DE9 = 1;
            }

            if (play->sceneNum == SCENE_HAUNTED_WASTELAND) {
                play->transitionType = TRANS_TYPE_SANDSTORM_PERSIST;
                gSaveContext.nextTransitionType = TRANS_TYPE_SANDSTORM_PERSIST;
            }

            gSaveContext.respawnFlag = -2;
            play->nextEntranceIndex = gSaveContext.entranceIndex;

            // In ER, handle sun song respawn from last entrance from grottos
            if (IS_RANDO && Randomizer_GetSettingValue(RSK_SHUFFLE_ENTRANCES)) {
                Grotto_ForceGrottoReturn();
            }

            play->transitionTrigger = TRANS_TRIGGER_START;
            gSaveContext.sunsSongState = SUNSSONG_INACTIVE;
            func_800F6964(30);
            gSaveContext.seqId = (u8)NA_BGM_DISABLED;
            gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
        } else {
            gSaveContext.sunsSongState = SUNSSONG_SPECIAL;
        }
    }
}

void Interface_DrawTextCharacter(GraphicsContext* gfx, int16_t x, int16_t y, void* texture, uint16_t colorR,
                                 uint16_t colorG, uint16_t colorB, uint16_t colorA, float textScale,
                                 uint8_t textShadow) {

    int32_t scale = R_TEXT_CHAR_SCALE * textScale;
    int32_t sCharTexSize = (scale / 100.0f) * 16.0f;
    int32_t sCharTexScale = 1024.0f / (scale / 100.0f);

    OPEN_DISPS(gfx);

    gDPPipeSync(POLY_OPA_DISP++);

    gDPLoadTextureBlock_4b(POLY_OPA_DISP++, texture, G_IM_FMT_I, FONT_CHAR_TEX_WIDTH, FONT_CHAR_TEX_HEIGHT, 0,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                           G_TX_NOLOD);

    if (textShadow) {
        // Draw drop shadow
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 0, 0, 0, colorA);
        gSPTextureRectangle(POLY_OPA_DISP++, (x + R_TEXT_DROP_SHADOW_OFFSET) << 2, (y + R_TEXT_DROP_SHADOW_OFFSET) << 2,
                            (x + R_TEXT_DROP_SHADOW_OFFSET + sCharTexSize) << 2,
                            (y + R_TEXT_DROP_SHADOW_OFFSET + sCharTexSize) << 2, G_TX_RENDERTILE, 0, 0, sCharTexScale,
                            sCharTexScale);
    }

    gDPPipeSync(POLY_OPA_DISP++);

    // Draw normal text
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, colorR, colorG, colorB, colorA);
    gSPTextureRectangle(POLY_OPA_DISP++, x << 2, y << 2, (x + sCharTexSize) << 2, (y + sCharTexSize) << 2,
                        G_TX_RENDERTILE, 0, 0, sCharTexScale, sCharTexScale);

    CLOSE_DISPS(gfx);
}

uint16_t Interface_DrawTextLine(GraphicsContext* gfx, char text[], int16_t x, int16_t y, uint16_t colorR,
                                uint16_t colorG, uint16_t colorB, uint16_t colorA, float textScale,
                                uint8_t textShadow) {

    uint16_t textureIndex;
    uint16_t kerningOffset = 0;
    uint16_t lineOffset = 0;
    void* texture;
    const char* processedText = Interface_ReplaceSpecialCharacters(text);
    uint8_t textLength = strlen(processedText);

    for (uint16_t i = 0; i < textLength; i++) {
        if (processedText[i] == '\n') {
            lineOffset += 15 * textScale;
            kerningOffset = 0;
        } else {
            textureIndex = processedText[i] - 32;

            if (textureIndex != 0) {
                texture = Ship_GetCharFontTexture(processedText[i]);
                Interface_DrawTextCharacter(gfx, x + kerningOffset, y + lineOffset, texture, colorR, colorG, colorB,
                                            colorA, textScale, textShadow);
            }
            kerningOffset +=
                (uint16_t)(Ship_GetCharFontWidth(processedText[i]) * (R_TEXT_CHAR_SCALE / 100.0f) * textScale);
        }
    }

    return kerningOffset;
}
