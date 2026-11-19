// NotEnoughBottles: four more bottles, the Net and the Bottomless Bottle.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"
#include "soh/Enhancements/randomizer/randomizerEnums.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "regs.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
#include "objects/object_gi_bottle/object_gi_bottle.h"
#include "variables.h"
#include "textures/item_name_static/item_name_static.h"

#define NEB_MOD_NAME "not_enough_bottles"
#define NEB_SAVE_FIELD "custom_bottles"
#define NEB_LEGACY_SAVE_FIELD "bottles"
#define NEB_NET_KEY "nei.net"
#define NEB_BOTTOMLESS_KEY "nei.bottomless_bottle"

#define NEB_EXTRA_COUNT 4
#define NEB_BOTTOMLESS NEB_EXTRA_COUNT
#define NEB_BOTTLE_COUNT (NEB_EXTRA_COUNT + 1)
#define NEB_NONE (-1)
#define NEB_VANILLA_COUNT 4
#define NEB_BUTTON_COUNT 8
#define NEB_CONTENT_COUNT (ITEM_POE - ITEM_BOTTLE + 1)
#define NEB_DIGIT_SIZE 8
#define NEB_DIGIT_GAP 6
#define NEB_CATCH_RANGE 90.0f
#define NEB_CATCH_FIRST_FRAME 2
#define NEB_CATCH_LAST_FRAME 6
#define NEB_NET_GIVE_SCALE 0.65f
#define NEB_NET_HELD_ROT_X (-5.0f)
#define NEB_NET_HELD_ROT_Y (-169.0f)
#define NEB_NET_HELD_ROT_Z 64.0f
#define NEB_NET_HELD_OFFSET_X 3.2f
#define NEB_NET_HELD_OFFSET_Y 4.3f
#define NEB_NET_HELD_OFFSET_Z (-1.0f)
#define NEB_NET_HELD_SCALE 0.49f
#define NEB_DEFAULT_USES 1
#define NEB_LEGACY_WHEEL_SIZE 4
#define NEB_LEGACY_BOTTLE_COUNT 8
#define NEB_LEGACY_EMPTY 0xFF

typedef struct {
    uint8_t content[NEB_BOTTLE_COUNT];
    uint8_t bottomlessUses;
} NebSave;

typedef struct {
    uint8_t bottles[NEB_LEGACY_BOTTLE_COUNT];
    uint8_t active[2];
    uint8_t bottomlessContent;
    uint8_t bottomlessUses;
} NebLegacySave;

typedef struct {
    int8_t slot;
    int8_t bottle;
    uint8_t original;
    uint8_t lent;
} NebLoan;

typedef struct {
    uint8_t item;
    uint8_t uses;
} NebContentUses;

typedef struct {
    int16_t actorId;
    uint8_t item;
} NebCatch;

typedef struct {
    int16_t x;
    int16_t y;
} NebButtonPos;

typedef struct {
    uint8_t content;
    bool isStoredAsRefill;
    bool hadFreeVanillaSlot;
} NebOffer;

static const char* const sBottleKeys[NEB_BOTTLE_COUNT] = { "nei.bottle_5", "nei.bottle_6", "nei.bottle_7",
                                                           "nei.bottle_8", NEB_BOTTOMLESS_KEY };
static const uint8_t sExtraCells[NEB_EXTRA_COUNT] = { SLOT_BOTTLE_1, SLOT_BOTTLE_2, SLOT_BOTTLE_1, SLOT_BOTTLE_2 };
static const int32_t sExtraPriorities[NEB_EXTRA_COUNT] = { -1, -1, -2, -2 };

static const NebButtonPos sDpadAmmoPos[] = {
    { DPAD_UP_X, DPAD_UP_Y + 11 },
    { DPAD_DOWN_X, DPAD_DOWN_Y + 11 },
    { DPAD_LEFT_X, DPAD_LEFT_Y + 11 },
    { DPAD_RIGHT_X, DPAD_RIGHT_Y + 11 },
};

static const ALIGN_ASSET(2) char sNetIconTex[] = "__OTR__textures/icon_item_custom/gItemIconNetTex";
static const ALIGN_ASSET(2) char sNetNameTex[] = "__OTR__textures/item_name_custom/gNetNameTex";
static const ALIGN_ASSET(2) char sNetXluDL[] = "__OTR__objects/object_nei_net/g_net_xlu_dl";
static const ALIGN_ASSET(2) char sNetDL[] = "__OTR__objects/object_nei_net/g_net_dl";
static const ALIGN_ASSET(2) char sBottomlessIconTex[] = "__OTR__textures/icon_item_custom/gItemIconBottomlessBottleTex";
static const ALIGN_ASSET(2) char sBottomlessNameTex[] = "__OTR__textures/item_name_custom/gBottomlessBottleNameTex";
static const ALIGN_ASSET(2) char sSwingAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_bottle_bug_miss";
static const ALIGN_ASSET(2) char sSwingWaterAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_bottle_fish_miss";
static const ALIGN_ASSET(2) char sDigitTex[10][48] = {
    "__OTR__textures/parameter_static/gAmmoDigit0Tex", "__OTR__textures/parameter_static/gAmmoDigit1Tex",
    "__OTR__textures/parameter_static/gAmmoDigit2Tex", "__OTR__textures/parameter_static/gAmmoDigit3Tex",
    "__OTR__textures/parameter_static/gAmmoDigit4Tex", "__OTR__textures/parameter_static/gAmmoDigit5Tex",
    "__OTR__textures/parameter_static/gAmmoDigit6Tex", "__OTR__textures/parameter_static/gAmmoDigit7Tex",
    "__OTR__textures/parameter_static/gAmmoDigit8Tex", "__OTR__textures/parameter_static/gAmmoDigit9Tex",
};

static const char* const sContentNames[NEB_CONTENT_COUNT][3] = {
    { gEmptyBottleItemNameENGTex, gEmptyBottleItemNameGERTex, gEmptyBottleItemNameFRATex },
    { gRedPotionItemNameENGTex, gRedPotionItemNameGERTex, gRedPotionItemNameFRATex },
    { gGreenPotionItemNameENGTex, gGreenPotionItemNameGERTex, gGreenPotionItemNameFRATex },
    { gBluePotionItemNameENGTex, gBluePotionItemNameGERTex, gBluePotionItemNameFRATex },
    { gBottledFairyItemNameENGTex, gBottledFairyItemNameGERTex, gBottledFairyItemNameFRATex },
    { gFishItemNameENGTex, gFishItemNameGERTex, gFishItemNameFRATex },
    { gFullMilkItemNameENGTex, gFullMilkItemNameGERTex, gFullMilkItemNameFRATex },
    { gRutosLetterItemNameENGTex, gRutosLetterItemNameGERTex, gRutosLetterItemNameFRATex },
    { gBlueFireItemNameENGTex, gBlueFireItemNameGERTex, gBlueFireItemNameFRATex },
    { gBugItemNameENGTex, gBugItemNameGERTex, gBugItemNameFRATex },
    { gBigPoeItemNameENGTex, gBigPoeItemNameGERTex, gBigPoeItemNameFRATex },
    { gHalfMilkItemNameENGTex, gHalfMilkItemNameGERTex, gHalfMilkItemNameFRATex },
    { gPoeItemNameENGTex, gPoeItemNameGERTex, gPoeItemNameFRATex },
};

static const NebContentUses sBottomlessUses[] = {
    { ITEM_POTION_RED, 5 }, { ITEM_POTION_GREEN, 5 }, { ITEM_POTION_BLUE, 5 }, { ITEM_MILK_BOTTLE, 5 },
    { ITEM_FISH, 6 },       { ITEM_BUG, 9 },          { ITEM_FAIRY, 3 },       { ITEM_BLUE_FIRE, 7 },
    { ITEM_POE, 3 },        { ITEM_BIG_POE, 3 },
};

static const NebCatch sNetCatch[] = {
    { ACTOR_EN_ELF, ITEM_FAIRY },  { ACTOR_EN_FISH, ITEM_FISH },        { ACTOR_EN_ICE_HONO, ITEM_BLUE_FIRE },
    { ACTOR_EN_INSECT, ITEM_BUG }, { ACTOR_EN_PO_FIELD, ITEM_BIG_POE },
};

static const int16_t sEmptyBottleAskers[] = { ACTOR_EN_OSSAN, ACTOR_EN_COW,      ACTOR_EN_TA, ACTOR_EN_DNS,
                                              ACTOR_EN_DS,    ACTOR_EN_PO_FIELD, ACTOR_EN_POH };
static const uint8_t sBeggarWants[] = { ITEM_BLUE_FIRE, ITEM_BUG, ITEM_FISH };

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate",
                                              "OnPlayerUpdate",
                                              "OnPlayerResolveItemAction",
                                              "OnResolveItemGive",
                                              "OnItemReceive",
                                              "OnKaleidoResolveName",
                                              "OnInterfaceResolveButtonIcon",
                                              "OnKaleidoItemDraw",
                                              "OnInterfaceDrawEnd",
                                              "OnLoadFile",
                                              "ShouldActorUpdate",
                                              "OnActorUpdate",
                                              "OnFlagSet",
                                              "OnGameFrameUpdate",
                                              "OnPlayerResolveLimbDraw",
                                              "OnPlayerPostLimbDraw" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
#define REMOTE_VISUAL_INTERVAL 5u

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual);
static void InitRemoteVisuals(void);
static uint32_t CaptureRemoteLimbEnv(PlayState* play);
static void RestoreRemoteLimbEnv(PlayState* play, uint32_t color);
static NebSave sSave;
static u8 sChateauBottles[NEB_BOTTLE_COUNT + NEB_VANILLA_COUNT];
static const ALIGN_ASSET(2) char sChateauContentIcon[] = "__OTR__icon_item_static_yar/gItemIconChateauRomaniTex";
static const ALIGN_ASSET(2) char sChateauContentName[] = "__OTR__item_name_static/gItemNameChateauRomaniENGTex";
static void DrinkChateauContent(PlayState* play) {
    typedef void (*DrinkFunc)(PlayState*);
    DrinkFunc drink = SOH_MOD_API_HAS(sApi, FindService) ? (DrinkFunc)sApi->FindService("nei.chateau.drink") : NULL;
    if (drink)
        drink(play);
}
static void StoreChateauBottles(void) {
    sApi->StorageSet(NEB_MOD_NAME, "chateau_contents", sChateauBottles, sizeof(sChateauBottles));
}

#define NEB_FOREIGN_FIRST 0xF0
#define NEB_FOREIGN_BASE ITEM_LETTER_RUTO
#define NEB_GOLD_DUST_KEY "fleet.bottle.gold_dust"
#define NEB_CHATEAU_KEY "fleet.bottle.chateau"
#define NEB_MM_ICON(name) "__OTR__icon_item_static_yar/gItemIcon" name "Tex"
#define NEB_MM_NAME(name) "__OTR__item_name_static/gItemName" name "Tex"

typedef struct {
    const char* token;
    const char* icon;
    const char* name;
} NebForeign;

static const NebForeign sForeign[] = {
    { "gold_dust", NEB_MM_ICON("BottledGoldDust"), NEB_MM_NAME("GoldDustENG") },
    { "deku_princess", NEB_MM_ICON("BottledDekuPrincess"), NEB_MM_NAME("DekuPrincessENG") },
    { "spring_water", NEB_MM_ICON("SpringWater"), NEB_MM_NAME("SpringWaterENG") },
    { "hot_spring_water", NEB_MM_ICON("HotSpringWater"), NEB_MM_NAME("HotSpringWaterENG") },
    { "zora_egg", NEB_MM_ICON("BottledZoraEgg"), NEB_MM_NAME("ZoraEggENG") },
    { "mushroom", NEB_MM_ICON("BottledMushroom"), NEB_MM_NAME("MagicalMushroomENG") },
    { "seahorse", NEB_MM_ICON("BottledSeahorse"), NEB_MM_NAME("SeaHorseENG") },
    { "hylian_loach", NEB_MM_ICON("BottledHylianLoach"), NEB_MM_NAME("HylianLoachJPN") },
    { "obaba_drink", NEB_MM_ICON("BottledFullMilk"), NEB_MM_NAME("ObabasDrinkJPN") },
};
#define NEB_FOREIGN_GOLD_DUST NEB_FOREIGN_FIRST

typedef struct {
    const char* token;
    uint8_t content;
} NebNative;

static const NebNative sNatives[] = {
    { "empty", ITEM_BOTTLE },
    { "red_potion", ITEM_POTION_RED },
    { "green_potion", ITEM_POTION_GREEN },
    { "blue_potion", ITEM_POTION_BLUE },
    { "fairy", ITEM_FAIRY },
    { "fish", ITEM_FISH },
    { "milk", ITEM_MILK_BOTTLE },
    { "half_milk", ITEM_MILK_HALF },
    { "ruto_letter", ITEM_LETTER_RUTO },
    { "blue_fire", ITEM_BLUE_FIRE },
    { "bugs", ITEM_BUG },
    { "big_poe", ITEM_BIG_POE },
    { "poe", ITEM_POE },
};

static const char sTimeAndPlace[] = "Oak's words echoed...^There's a time and place for&everything! But not now.";
static bool sIsTimeAndPlaceSaid;
static uint8_t sForeignVanilla[NEB_VANILLA_COUNT];

static bool IsForeignContent(uint8_t content) {
    return content >= NEB_FOREIGN_FIRST && content < NEB_FOREIGN_FIRST + ARRAY_COUNT(sForeign);
}

static const NebForeign* ForeignOf(uint8_t content) {
    return IsForeignContent(content) ? &sForeign[content - NEB_FOREIGN_FIRST] : NULL;
}

static void StoreForeignVanilla(void) {
    sApi->StorageSet(NEB_MOD_NAME, "foreign_vanilla", sForeignVanilla, sizeof(sForeignVanilla));
}

static void GuardForeignVanilla(void);

static NebLoan sLoan = { NEB_NONE, NEB_NONE, ITEM_NONE, ITEM_NONE };
static NebOffer sOffer = { ITEM_NONE, false, false };
static int8_t sAcquiredBottle = NEB_NONE;
static uint8_t sButtonsToRestore;
static bool sIsCatchResolved;
static int sNetCaptureFrames;
static const char* sCatchMessage;

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_ZeroSpeedXZ(Player* player);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
void func_80839FFC(Player* player, PlayState* play);
s8 Player_ItemToItemAction(s32 item);
GetItemEntry ItemTable_RetrieveEntry(s16 modIndex, s16 getItemID);
int16_t OTRGetRectDimensionFromRightEdge(float v);
float OTRGetDimensionFromRightEdge(float v);

static bool IsBottleContent(uint8_t item) {
    return item >= ITEM_BOTTLE && item <= ITEM_POE;
}

static bool IsOwned(int8_t bottle) {
    return sApi->IsCustomItemOwned(sBottleKeys[bottle]);
}

static bool IsEmpty(int8_t bottle) {
    return sSave.content[bottle] == ITEM_BOTTLE;
}

static void StoreBottles(void) {
    sApi->StorageSet(NEB_MOD_NAME, NEB_SAVE_FIELD, &sSave, sizeof(sSave));
}

static int8_t BottleForKey(const char* key) {
    if (key == NULL) {
        return NEB_NONE;
    }
    for (int8_t bottle = 0; bottle < NEB_BOTTLE_COUNT; bottle++) {
        if (strcmp(key, sBottleKeys[bottle]) == 0) {
            return bottle;
        }
    }
    return NEB_NONE;
}

static int8_t BottleOnButton(int32_t button) {
    if (button < 0 || button >= NEB_BUTTON_COUNT || gSaveContext.equips.buttonItems[button] != ITEM_CUSTOM) {
        return NEB_NONE;
    }
    return BottleForKey(sApi->GetEquippedCustomItem((uint8_t)button));
}

static void ShowContent(int8_t bottle) {
    const char* icon = IsForeignContent(sSave.content[bottle])       ? ForeignOf(sSave.content[bottle])->icon
                       : sChateauBottles[bottle]                     ? sChateauContentIcon
                       : bottle == NEB_BOTTOMLESS && IsEmpty(bottle) ? sBottomlessIconTex
                                                                     : (const char*)gItemIcons[sSave.content[bottle]];

    CustomItemRegistry_SetIconPath(sBottleKeys[bottle], icon);
}

static uint8_t BottomlessUsesFor(uint8_t item) {
    for (uint8_t index = 0; index < ARRAY_COUNT(sBottomlessUses); index++) {
        if (sBottomlessUses[index].item == item) {
            return sBottomlessUses[index].uses;
        }
    }
    return NEB_DEFAULT_USES;
}

static void SetContent(int8_t bottle, uint8_t item) {
    bool chateau = sChateauBottles[bottle] != 0;
    if (chateau && (item == ITEM_BOTTLE || item == ITEM_MILK_HALF) && gPlayState) {
        DrinkChateauContent(gPlayState);
        item = ITEM_BOTTLE;
    }
    if (bottle == NEB_BOTTOMLESS && !IsEmpty(bottle) && (item == ITEM_BOTTLE || item == ITEM_MILK_HALF) &&
        sSave.bottomlessUses > 1) {
        sSave.bottomlessUses--;
    } else {
        sSave.content[bottle] = item;
        if (bottle == NEB_BOTTOMLESS) {
            sSave.bottomlessUses = item == ITEM_BOTTLE ? 0 : BottomlessUsesFor(item);
        }
    }
    if (sSave.content[bottle] != ITEM_MILK_BOTTLE && sSave.content[bottle] != ITEM_MILK_HALF)
        sChateauBottles[bottle] = 0;
    StoreChateauBottles();
    StoreBottles();
    ShowContent(bottle);
}

static void Acquire(int8_t bottle, uint8_t content) {
    sApi->SetCustomItemOwned(sBottleKeys[bottle], true);
    sAcquiredBottle = NEB_NONE;
    SetContent(bottle, content);
}

static int8_t FindUnownedExtra(void) {
    for (int8_t bottle = 0; bottle < NEB_EXTRA_COUNT; bottle++) {
        if (!IsOwned(bottle)) {
            return bottle;
        }
    }
    return NEB_NONE;
}

static int8_t FindOwnedHolding(uint8_t content) {
    if (IsOwned(NEB_BOTTOMLESS) && sSave.content[NEB_BOTTOMLESS] == content) {
        return NEB_BOTTOMLESS;
    }
    for (int8_t bottle = 0; bottle < NEB_EXTRA_COUNT; bottle++) {
        if (IsOwned(bottle) && sSave.content[bottle] == content) {
            return bottle;
        }
    }
    return NEB_NONE;
}

static int8_t FindVanillaSlot(uint8_t content) {
    for (int8_t i = 0; i < NEB_VANILLA_COUNT; i++) {
        if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == content) {
            return (int8_t)(SLOT_BOTTLE_1 + i);
        }
    }
    return NEB_NONE;
}

static bool AddBottle(uint8_t content) {
    int8_t slot = FindVanillaSlot(ITEM_NONE);

    if (slot != NEB_NONE) {
        gSaveContext.inventory.items[slot] = content;
        return true;
    }
    int8_t bottle = FindUnownedExtra();

    if (bottle == NEB_NONE) {
        return false;
    }
    Acquire(bottle, content);
    return true;
}

static void AddForeignBottle(uint8_t content) {
    int8_t bottle = FindUnownedExtra();
    if (bottle != NEB_NONE) {
        Acquire(bottle, content);
        return;
    }
    int8_t slot = FindVanillaSlot(ITEM_NONE);
    if (slot == NEB_NONE) {
        return;
    }
    gSaveContext.inventory.items[slot] = NEB_FOREIGN_BASE;
    sForeignVanilla[slot - SLOT_BOTTLE_1] = content;
    StoreForeignVanilla();
}

static void AddChateauBottle(void) {
    int8_t slot = FindVanillaSlot(ITEM_NONE);
    if (slot != NEB_NONE) {
        gSaveContext.inventory.items[slot] = ITEM_MILK_BOTTLE;
        sChateauBottles[NEB_BOTTLE_COUNT + slot - SLOT_BOTTLE_1] = ITEM_MILK_BOTTLE;
        StoreChateauBottles();
        return;
    }
    int8_t bottle = FindUnownedExtra();
    if (bottle == NEB_NONE) {
        return;
    }
    Acquire(bottle, ITEM_MILK_BOTTLE);
    sChateauBottles[bottle] = 1;
    StoreChateauBottles();
    ShowContent(bottle);
}

static void ReceiveSharedBottle(const char* key) {
    if (strcmp(key, NEB_GOLD_DUST_KEY) == 0) {
        AddForeignBottle(NEB_FOREIGN_GOLD_DUST);
    } else if (strcmp(key, NEB_CHATEAU_KEY) == 0) {
        AddChateauBottle();
    }
}

static bool FillChateauBottle(PlayState* play) {
    int bottle = FindOwnedHolding(ITEM_BOTTLE);
    if (bottle != NEB_NONE) {
        SetContent(bottle, ITEM_MILK_BOTTLE);
        sChateauBottles[bottle] = 1;
        if (bottle == NEB_BOTTOMLESS)
            sSave.bottomlessUses = 5;
        StoreChateauBottles();
        StoreBottles();
        ShowContent(bottle);
        return true;
    }
    int slot = FindVanillaSlot(ITEM_BOTTLE);
    if (slot == NEB_NONE)
        return false;
    gSaveContext.inventory.items[slot] = ITEM_MILK_BOTTLE;
    sChateauBottles[NEB_BOTTLE_COUNT + slot - SLOT_BOTTLE_1] = ITEM_MILK_BOTTLE;
    StoreChateauBottles();
    for (int b = 1; b < NEB_BUTTON_COUNT; ++b)
        if (gSaveContext.equips.cButtonSlots[b - 1] == slot) {
            gSaveContext.equips.buttonItems[b] = ITEM_MILK_BOTTLE;
            Interface_LoadItemIcon1(play, b);
        }
    return true;
}
static void WatchVanillaChateau(void) {
    for (int b = 0; b < NEB_VANILLA_COUNT; ++b) {
        u8* previous = &sChateauBottles[NEB_BOTTLE_COUNT + b];
        if (!*previous)
            continue;
        u8 item = gSaveContext.inventory.items[SLOT_BOTTLE_1 + b];
        if (item == *previous)
            continue;
        if (gPlayState && (item == ITEM_MILK_HALF || item == ITEM_BOTTLE)) {
            DrinkChateauContent(gPlayState);
            gSaveContext.inventory.items[SLOT_BOTTLE_1 + b] = ITEM_BOTTLE;
            for (int button = 1; button <= 3; ++button)
                if (gSaveContext.equips.cButtonSlots[button - 1] == SLOT_BOTTLE_1 + b) {
                    gSaveContext.equips.buttonItems[button] = ITEM_BOTTLE;
                    Interface_LoadItemIcon1(gPlayState, button);
                }
            item = ITEM_BOTTLE;
        }
        *previous = item == ITEM_MILK_HALF ? item : 0;
        StoreChateauBottles();
    }
}

static bool HasEmptyBottle(void) {
    return FindVanillaSlot(ITEM_BOTTLE) != NEB_NONE || FindOwnedHolding(ITEM_BOTTLE) != NEB_NONE;
}

static void CatchBottleWrite(bool* should, va_list args) {
    int32_t button = va_arg(args, int32_t);
    int32_t item = va_arg(args, int32_t);
    int8_t bottle = BottleOnButton(button);

    if (bottle == NEB_NONE) {
        return;
    }
    if (sSave.content[bottle] == ITEM_MILK_BOTTLE && item == ITEM_BOTTLE) {
        item = ITEM_MILK_HALF;
    } else if (sSave.content[bottle] != ITEM_MILK_BOTTLE && item == ITEM_MILK_HALF) {
        item = ITEM_BOTTLE;
    }
    SetContent(bottle, (uint8_t)item);
    sButtonsToRestore |= 1 << button;
    *should = false;
}

static void RestoreButtons(void) {
    if (sButtonsToRestore == 0) {
        return;
    }
    for (uint8_t button = 0; button < NEB_BUTTON_COUNT; button++) {
        if (sButtonsToRestore & (1 << button)) {
            gSaveContext.equips.buttonItems[button] = ITEM_CUSTOM;
        }
    }
    sButtonsToRestore = 0;
}

static int8_t VanillaBottleOnButton(int32_t button) {
    if (button < 1 || button >= NEB_BUTTON_COUNT) {
        return NEB_NONE;
    }
    const int32_t index = gSaveContext.equips.cButtonSlots[button - 1] - SLOT_BOTTLE_1;
    return index >= 0 && index < NEB_VANILLA_COUNT ? (int8_t)index : NEB_NONE;
}

static uint8_t ForeignOnButton(int32_t button) {
    const int8_t bottle = BottleOnButton(button);
    if (bottle != NEB_NONE) {
        return IsForeignContent(sSave.content[bottle]) ? sSave.content[bottle] : 0;
    }
    const int8_t vanilla = VanillaBottleOnButton(button);
    return vanilla != NEB_NONE ? sForeignVanilla[vanilla] : 0;
}

static void ResolveBottleAction(int32_t item, int8_t* itemAction) {
    if (gPlayState == NULL) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (ForeignOnButton(player->heldItemButton) != 0) {
        const bool isTalking = player->talkActor != NULL || Message_GetState(&gPlayState->msgCtx) != TEXT_STATE_NONE;
        *itemAction = isTalking ? PLAYER_IA_NONE : PLAYER_IA_BOTTLE_BIG_POE;
        return;
    }
    int8_t bottle = item == ITEM_CUSTOM ? BottleOnButton(player->heldItemButton) : NEB_NONE;
    if (bottle != NEB_NONE) {
        *itemAction = Player_ItemToItemAction(sSave.content[bottle]);
    }
}

static void SayTimeAndPlace(void) {
    if (gPlayState == NULL) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    const bool isShowingForeign =
        player->heldItemAction == PLAYER_IA_BOTTLE_BIG_POE && ForeignOnButton(player->heldItemButton) != 0;
    if (!isShowingForeign) {
        sIsTimeAndPlaceSaid = false;
        return;
    }
    if (!sIsTimeAndPlaceSaid && Message_GetState(&gPlayState->msgCtx) == TEXT_STATE_NONE &&
        SOH_MOD_API_HAS(sApi, ShowTextbox)) {
        sIsTimeAndPlaceSaid = sApi->ShowTextbox(gPlayState, sTimeAndPlace, true);
    }
}

uint8_t KaleidoItemManager_GetVanillaSlot(uint8_t cell);

static int32_t InventorySlotForCell(int32_t cell) {
    if (ModLayout_IsActive(SOH_LAYOUT_ITEMS)) {
        return ModLayout_GetItemCellSlot((uint8_t)cell);
    }
    return KaleidoItemManager_GetVanillaSlot((uint8_t)cell);
}

static bool IsVanillaChateauSlot(PlayState* play, uint16_t item) {
    int32_t bottle = InventorySlotForCell(play->pauseCtx.cursorPoint[PAUSE_ITEM]) - SLOT_BOTTLE_1;

    return (item == ITEM_MILK_BOTTLE || item == ITEM_MILK_HALF) && bottle >= 0 && bottle < NEB_VANILLA_COUNT &&
           sChateauBottles[NEB_BOTTLE_COUNT + bottle] != 0;
}

static uint8_t ForeignUnderCursor(PlayState* play) {
    const int32_t bottle = InventorySlotForCell(play->pauseCtx.cursorPoint[PAUSE_ITEM]) - SLOT_BOTTLE_1;
    return bottle >= 0 && bottle < NEB_VANILLA_COUNT ? sForeignVanilla[bottle] : 0;
}

static void ResolveBottleName(PlayState* play, uint16_t item, const char** namePath) {
    if (IsVanillaChateauSlot(play, item)) {
        *namePath = sChateauContentName;
        return;
    }
    const uint8_t foreign = item == NEB_FOREIGN_BASE ? ForeignUnderCursor(play) : 0;
    if (foreign != 0) {
        *namePath = ForeignOf(foreign)->name;
        return;
    }
    const SOHCustomItemDefinition* selected = CustomItemRegistry_GetPauseItem();
    int8_t bottle = item == ITEM_CUSTOM && selected != NULL ? BottleForKey(selected->key) : NEB_NONE;

    if (bottle == NEB_NONE || (bottle == NEB_BOTTOMLESS && IsEmpty(bottle))) {
        return;
    }
    if (sChateauBottles[bottle] != 0) {
        *namePath = sChateauContentName;
        return;
    }
    if (IsForeignContent(sSave.content[bottle])) {
        *namePath = ForeignOf(sSave.content[bottle])->name;
        return;
    }
    uint8_t language = gSaveContext.language <= LANGUAGE_FRA ? gSaveContext.language : LANGUAGE_ENG;

    *namePath = sContentNames[sSave.content[bottle] - ITEM_BOTTLE][language];
}

static bool TakeNewBottle(uint8_t content) {
    int8_t bottle = FindVanillaSlot(ITEM_NONE) == NEB_NONE ? FindUnownedExtra() : NEB_NONE;

    if (bottle == NEB_NONE) {
        return false;
    }
    Acquire(bottle, content);
    // Handling the give skips Item_Give, which is where vanilla records the letter.
    if (content == ITEM_LETTER_RUTO) {
        Flags_SetRandomizerInf(RAND_INF_OBTAINED_RUTOS_LETTER);
    }
    return true;
}

static void GiveIntoBottles(PlayState* play, uint8_t* item, bool* handled) {
    uint8_t given = *item == ITEM_MILK ? ITEM_MILK_BOTTLE : *item;

    sOffer.content = ITEM_NONE;
    if (*handled) {
        return;
    }
    if (*item == ITEM_BOTTLE || *item == ITEM_MILK_BOTTLE || *item == ITEM_LETTER_RUTO) {
        const bool isFull = FindVanillaSlot(ITEM_NONE) == NEB_NONE && FindUnownedExtra() == NEB_NONE;

        // Item_Give writes an empty bottle that finds no free slot over the first bottle, content and all.
        *handled = TakeNewBottle(given) || (isFull && *item == ITEM_BOTTLE);
        return;
    }
    if (!IsBottleContent(given)) {
        return;
    }
    sOffer.content = given;
    sOffer.hadFreeVanillaSlot = FindVanillaSlot(ITEM_NONE) != NEB_NONE;
    sOffer.isStoredAsRefill = false;
    if (FindVanillaSlot(ITEM_BOTTLE) != NEB_NONE) {
        return;
    }
    int8_t bottle = FindOwnedHolding(ITEM_BOTTLE);

    if (bottle != NEB_NONE) {
        SetContent(bottle, given);
        sOffer.isStoredAsRefill = true;
        *handled = true;
    }
}

static void CompleteSeededBottle(GetItemEntry entry) {
    const uint8_t content = sOffer.content;

    sOffer.content = ITEM_NONE;
    if (content == ITEM_NONE || entry.modIndex != MOD_RANDOMIZER || entry.getItemId < RG_BOTTLE_WITH_RED_POTION ||
        entry.getItemId > RG_BOTTLE_WITH_BIG_POE) {
        return;
    }
    if (sOffer.isStoredAsRefill) {
        AddBottle(ITEM_BOTTLE);
    } else if (!sOffer.hadFreeVanillaSlot) {
        AddBottle(content);
    }
}

static void NoteAcquiredBottle(const char* key) {
    sAcquiredBottle = BottleForKey(key);
}

static void ReceiveExtraBottle(const char* key) {
    const bool isDuplicate = sAcquiredBottle != BottleForKey(key);

    sAcquiredBottle = NEB_NONE;
    if (isDuplicate) {
        AddBottle(ITEM_BOTTLE);
    }
}

static void ResyncSlotButtons(uint8_t slot) {
    for (uint8_t button = 1; button < NEB_BUTTON_COUNT; button++) {
        if (gSaveContext.equips.cButtonSlots[button - 1] != slot ||
            gSaveContext.equips.buttonItems[button] == ITEM_CUSTOM) {
            continue;
        }
        gSaveContext.equips.buttonItems[button] = gSaveContext.inventory.items[slot];
        if (gPlayState != NULL) {
            Interface_LoadItemIcon1(gPlayState, button);
        }
    }
}

static void SettleLoan(void) {
    if (sLoan.slot == NEB_NONE) {
        return;
    }
    uint8_t now = gSaveContext.inventory.items[sLoan.slot];

    if (now != sLoan.lent) {
        SetContent(sLoan.bottle, now);
    }
    gSaveContext.inventory.items[sLoan.slot] = sLoan.original;
    ResyncSlotButtons((uint8_t)sLoan.slot);
    sLoan.slot = NEB_NONE;
}

static void LendContent(uint8_t content) {
    int8_t bottle;
    int8_t slot;

    SettleLoan();
    if (FindVanillaSlot(content) != NEB_NONE) {
        return;
    }
    bottle = FindOwnedHolding(content);
    if (bottle == NEB_NONE) {
        return;
    }
    slot = FindVanillaSlot(ITEM_NONE);
    sLoan.slot = slot != NEB_NONE ? slot : (int8_t)SLOT_BOTTLE_1;
    sLoan.bottle = bottle;
    sLoan.original = gSaveContext.inventory.items[sLoan.slot];
    sLoan.lent = content;
    gSaveContext.inventory.items[sLoan.slot] = content;
}

static void LendForEmptyBottleAsker(void* actor, bool* result) {
    if (*result) {
        LendContent(ITEM_BOTTLE);
    }
}

static void LendForBeggar(void* actor, bool* result) {
    if (!*result) {
        return;
    }
    for (uint8_t i = 0; i < ARRAY_COUNT(sBeggarWants); i++) {
        if (FindVanillaSlot(sBeggarWants[i]) != NEB_NONE) {
            return;
        }
    }
    for (uint8_t i = 0; i < ARRAY_COUNT(sBeggarWants); i++) {
        if (FindOwnedHolding(sBeggarWants[i]) != NEB_NONE) {
            LendContent(sBeggarWants[i]);
            return;
        }
    }
}

static void LendFairyToFallingLink(void* actor, bool* result) {
    if (*result && gSaveContext.health <= 0) {
        LendContent(ITEM_FAIRY);
    }
}

static void CloseActorWindow(void* actor) {
    SettleLoan();
    RestoreButtons();
}

static bool NetHeld(void);
static void ResolveNetCatch(Player* player, PlayState* play);
static void CloseFrame(void) {
    sAcquiredBottle = NEB_NONE;
    WatchVanillaChateau();
    GuardForeignVanilla();
    if (gPlayState && NetHeld()) {
        Player* player = GET_PLAYER(gPlayState);
        for (int i = 0; i < ARRAY_COUNT(player->meleeWeaponQuads); ++i)
            player->meleeWeaponQuads[i].base.atFlags &= ~AT_ON;
        if (gPlayState->state.input[0].press.button & BTN_B)
            sNetCaptureFrames = 12;
        if (player->meleeWeaponState != 0 || sNetCaptureFrames > 0)
            ResolveNetCatch(player, gPlayState);
        if (sNetCaptureFrames > 0)
            --sNetCaptureFrames;
    }
    SettleLoan();
    RestoreButtons();
}

static void TakeBackRutosLetter(int16_t flagType, int16_t flag) {
    int8_t bottle;

    if (flagType != FLAG_EVENT_CHECK_INF || flag != EVENTCHKINF_KING_ZORA_MOVED) {
        return;
    }
    bottle = FindOwnedHolding(ITEM_LETTER_RUTO);
    if (bottle != NEB_NONE) {
        SetContent(bottle, ITEM_BOTTLE);
    }
}

static void GuardForeignVanilla(void) {
    for (int8_t bottle = 0; bottle < NEB_VANILLA_COUNT; bottle++) {
        const uint8_t slot = SLOT_BOTTLE_1 + bottle;
        const uint8_t item = gSaveContext.inventory.items[slot];
        if (sForeignVanilla[bottle] == 0 || item == NEB_FOREIGN_BASE) {
            continue;
        }
        if (item == ITEM_NONE) {
            sForeignVanilla[bottle] = 0;
            StoreForeignVanilla();
            continue;
        }
        for (int8_t other = 0; other < NEB_VANILLA_COUNT; other++) {
            const uint8_t otherSlot = SLOT_BOTTLE_1 + other;
            if (sForeignVanilla[other] == 0 && gSaveContext.inventory.items[otherSlot] == ITEM_LETTER_RUTO) {
                gSaveContext.inventory.items[otherSlot] = item;
                ResyncSlotButtons(otherSlot);
                break;
            }
        }
        gSaveContext.inventory.items[slot] = NEB_FOREIGN_BASE;
        ResyncSlotButtons(slot);
    }
}

static const char* NativeToken(uint8_t content) {
    for (uint8_t i = 0; i < ARRAY_COUNT(sNatives); i++) {
        if (sNatives[i].content == content) {
            return sNatives[i].token;
        }
    }
    return "empty";
}

static const char* ExtraToken(int8_t bottle) {
    const uint8_t content = sSave.content[bottle];
    if (IsForeignContent(content)) {
        return ForeignOf(content)->token;
    }
    return sChateauBottles[bottle] != 0 ? "chateau" : NativeToken(content);
}

static const char* VanillaToken(int8_t bottle) {
    if (sForeignVanilla[bottle] != 0) {
        return ForeignOf(sForeignVanilla[bottle])->token;
    }
    if (sChateauBottles[NEB_BOTTLE_COUNT + bottle] != 0) {
        return "chateau";
    }
    return NativeToken(gSaveContext.inventory.items[SLOT_BOTTLE_1 + bottle]);
}

static int CompareTokens(const void* left, const void* right) {
    return strcmp(*(const char* const*)left, *(const char* const*)right);
}

static const char* ReadBottles(void) {
    static char text[512];
    const char* tokens[NEB_VANILLA_COUNT + NEB_EXTRA_COUNT];
    size_t count = 0;
    for (int8_t bottle = 0; bottle < NEB_VANILLA_COUNT; bottle++) {
        if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + bottle] != ITEM_NONE) {
            tokens[count++] = VanillaToken(bottle);
        }
    }
    for (int8_t bottle = 0; bottle < NEB_EXTRA_COUNT; bottle++) {
        if (IsOwned(bottle)) {
            tokens[count++] = ExtraToken(bottle);
        }
    }
    qsort((void*)tokens, count, sizeof(tokens[0]), CompareTokens);
    size_t length = 0;
    text[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        length += snprintf(text + length, sizeof(text) - length, "%s%s", i == 0 ? "" : ",", tokens[i]);
    }
    if (IsOwned(NEB_BOTTOMLESS)) {
        snprintf(text + length, sizeof(text) - length, "|%s*%u", ExtraToken(NEB_BOTTOMLESS), sSave.bottomlessUses);
    }
    return text;
}

static bool IsTokenNamed(const char* token, size_t length, const char* name) {
    return strlen(name) == length && strncmp(token, name, length) == 0;
}

static void SetVanillaToken(int8_t bottle, const char* token, size_t length) {
    const uint8_t slot = SLOT_BOTTLE_1 + bottle;
    uint8_t content = ITEM_BOTTLE;
    sForeignVanilla[bottle] = 0;
    sChateauBottles[NEB_BOTTLE_COUNT + bottle] = 0;
    for (uint8_t i = 0; i < ARRAY_COUNT(sForeign); i++) {
        if (IsTokenNamed(token, length, sForeign[i].token)) {
            sForeignVanilla[bottle] = NEB_FOREIGN_FIRST + i;
            content = NEB_FOREIGN_BASE;
        }
    }
    for (uint8_t i = 0; i < ARRAY_COUNT(sNatives); i++) {
        if (IsTokenNamed(token, length, sNatives[i].token)) {
            content = sNatives[i].content;
        }
    }
    if (IsTokenNamed(token, length, "chateau")) {
        content = ITEM_MILK_BOTTLE;
        sChateauBottles[NEB_BOTTLE_COUNT + bottle] = ITEM_MILK_BOTTLE;
    }
    gSaveContext.inventory.items[slot] = content;
    ResyncSlotButtons(slot);
}

static void SetExtraToken(int8_t bottle, const char* token, size_t length) {
    uint8_t content = ITEM_BOTTLE;
    for (uint8_t i = 0; i < ARRAY_COUNT(sForeign); i++) {
        if (IsTokenNamed(token, length, sForeign[i].token)) {
            content = NEB_FOREIGN_FIRST + i;
        }
    }
    for (uint8_t i = 0; i < ARRAY_COUNT(sNatives); i++) {
        if (IsTokenNamed(token, length, sNatives[i].token)) {
            content = sNatives[i].content;
        }
    }
    const bool isChateau = IsTokenNamed(token, length, "chateau");
    sSave.content[bottle] = isChateau ? ITEM_MILK_BOTTLE : content;
    sChateauBottles[bottle] = isChateau ? 1 : 0;
    StoreBottles();
    ShowContent(bottle);
}

static void AddBottleOfToken(const char* token, size_t length) {
    if (IsTokenNamed(token, length, "chateau")) {
        AddChateauBottle();
        return;
    }
    for (uint8_t i = 0; i < ARRAY_COUNT(sForeign); i++) {
        if (IsTokenNamed(token, length, sForeign[i].token)) {
            AddForeignBottle(NEB_FOREIGN_FIRST + i);
            return;
        }
    }
    for (uint8_t i = 0; i < ARRAY_COUNT(sNatives); i++) {
        if (IsTokenNamed(token, length, sNatives[i].token)) {
            AddBottle(sNatives[i].content);
            return;
        }
    }
}

static void WriteBottles(const char* text) {
    char wanted[NEB_VANILLA_COUNT + NEB_EXTRA_COUNT][32];
    size_t wantedCount = 0;
    const char* cursor = text;
    while (*cursor != '\0' && *cursor != '|' && wantedCount < ARRAY_COUNT(wanted)) {
        const size_t length = strcspn(cursor, ",|");
        snprintf(wanted[wantedCount++], sizeof(wanted[0]), "%.*s", (int)length, cursor);
        cursor += length + (cursor[length] == ',' ? 1 : 0);
    }
    bool isTaken[ARRAY_COUNT(wanted)] = { false };
    int8_t unmatchedVanilla[NEB_VANILLA_COUNT];
    int8_t unmatchedExtra[NEB_EXTRA_COUNT];
    size_t vanillaLeft = 0;
    size_t extraLeft = 0;
    for (int8_t bottle = 0; bottle < NEB_VANILLA_COUNT + NEB_EXTRA_COUNT; bottle++) {
        const bool isVanilla = bottle < NEB_VANILLA_COUNT;
        const int8_t index = isVanilla ? bottle : (int8_t)(bottle - NEB_VANILLA_COUNT);
        if (isVanilla ? gSaveContext.inventory.items[SLOT_BOTTLE_1 + index] == ITEM_NONE : !IsOwned(index)) {
            continue;
        }
        const char* token = isVanilla ? VanillaToken(index) : ExtraToken(index);
        bool isMatched = false;
        for (size_t i = 0; i < wantedCount && !isMatched; i++) {
            isMatched = !isTaken[i] && strcmp(wanted[i], token) == 0;
            isTaken[i] = isTaken[i] || isMatched;
        }
        if (!isMatched && isVanilla) {
            unmatchedVanilla[vanillaLeft++] = index;
        } else if (!isMatched) {
            unmatchedExtra[extraLeft++] = index;
        }
    }
    size_t vanillaUsed = 0;
    size_t extraUsed = 0;
    for (size_t i = 0; i < wantedCount; i++) {
        if (isTaken[i]) {
            continue;
        }
        const size_t length = strlen(wanted[i]);
        if (vanillaUsed < vanillaLeft) {
            SetVanillaToken(unmatchedVanilla[vanillaUsed++], wanted[i], length);
        } else if (extraUsed < extraLeft) {
            SetExtraToken(unmatchedExtra[extraUsed++], wanted[i], length);
        } else {
            AddBottleOfToken(wanted[i], length);
        }
    }
    StoreForeignVanilla();
    StoreChateauBottles();
    const char* bottomless = strchr(text, '|');
    if (bottomless != NULL && IsOwned(NEB_BOTTOMLESS)) {
        const char* uses = strchr(bottomless, '*');
        const size_t length = uses != NULL ? (size_t)(uses - bottomless - 1) : strlen(bottomless + 1);
        SetExtraToken(NEB_BOTTOMLESS, bottomless + 1, length);
        sSave.bottomlessUses = uses != NULL ? (uint8_t)atoi(uses + 1) : 0;
        StoreBottles();
    }
}

static int32_t GetBottomlessUses(const char* key) {
    return IsEmpty(NEB_BOTTOMLESS) ? 0 : sSave.bottomlessUses;
}

static void ForgetBottles(void) {
    for (int8_t bottle = 0; bottle < NEB_BOTTLE_COUNT; bottle++) {
        sSave.content[bottle] = ITEM_BOTTLE;
    }
    sSave.bottomlessUses = 0;
}

static void AdoptLegacyBottles(void) {
    NebLegacySave legacy;

    if (!sApi->StorageHas(NEB_MOD_NAME, NEB_LEGACY_SAVE_FIELD) ||
        sApi->StorageGet(NEB_MOD_NAME, NEB_LEGACY_SAVE_FIELD, &legacy, sizeof(legacy)) != sizeof(legacy)) {
        return;
    }
    if (IsOwned(NEB_BOTTOMLESS)) {
        gSaveContext.inventory.items[SLOT_BOTTLE_4] = ITEM_NONE;
        sSave.content[NEB_BOTTOMLESS] =
            IsBottleContent(legacy.bottomlessContent) ? legacy.bottomlessContent : ITEM_BOTTLE;
        sSave.bottomlessUses = legacy.bottomlessUses;
    }
    for (uint8_t index = 0; index < NEB_LEGACY_BOTTLE_COUNT; index++) {
        uint8_t content = legacy.bottles[index];
        bool isShown = index % NEB_LEGACY_WHEEL_SIZE == legacy.active[index / NEB_LEGACY_WHEEL_SIZE];

        if (content == NEB_LEGACY_EMPTY || isShown) {
            continue;
        }
        AddBottle(content);
    }
    sApi->StorageRemove(NEB_MOD_NAME, NEB_LEGACY_SAVE_FIELD);
    StoreBottles();
}

static void LoadBottles(int32_t fileNum) {
    ForgetBottles();
    memset(sChateauBottles, 0, sizeof(sChateauBottles));
    sApi->StorageGet(NEB_MOD_NAME, "chateau_contents", sChateauBottles, sizeof(sChateauBottles));
    memset(sForeignVanilla, 0, sizeof(sForeignVanilla));
    sApi->StorageGet(NEB_MOD_NAME, "foreign_vanilla", sForeignVanilla, sizeof(sForeignVanilla));
    sLoan.slot = NEB_NONE;
    sButtonsToRestore = 0;
    sApi->StorageGet(NEB_MOD_NAME, NEB_SAVE_FIELD, &sSave, sizeof(sSave));
    for (int8_t bottle = 0; bottle < NEB_BOTTLE_COUNT; bottle++) {
        if (!IsBottleContent(sSave.content[bottle]) && !IsForeignContent(sSave.content[bottle])) {
            sSave.content[bottle] = ITEM_BOTTLE;
        }
    }
    AdoptLegacyBottles();
    for (int8_t bottle = 0; bottle < NEB_BOTTLE_COUNT; bottle++) {
        ShowContent(bottle);
    }
}

static uint8_t NetContentOf(Actor* actor) {
    if (actor->id == ACTOR_FISHING && actor->params >= 100)
        return ITEM_FISH;
    if (actor->id == ACTOR_EN_BUTTE)
        return ITEM_FAIRY;
    for (uint8_t index = 0; index < ARRAY_COUNT(sNetCatch); index++) {
        if (sNetCatch[index].actorId == actor->id) {
            return sNetCatch[index].item;
        }
    }
    return ITEM_NONE;
}

static Vec3f sNetCatchPoints[9];
static bool sNetCatchPointsValid;
static bool IsInFrontOfPlayer(Player* player, Actor* actor) {
    if (Math_Vec3f_DistXZ(&player->actor.world.pos, &actor->world.pos) < 160.0f &&
        fabsf(player->actor.world.pos.y - actor->world.pos.y) < 120.0f)
        return true;
    if (sNetCatchPointsValid) {
        for (int i = 0; i < ARRAY_COUNT(sNetCatchPoints); ++i)
            if (Math_Vec3f_DistXYZ(&actor->world.pos, &sNetCatchPoints[i]) < 100.0f)
                return true;
        return false;
    }
    f32 deltaX = actor->world.pos.x - player->actor.world.pos.x;
    f32 deltaZ = actor->world.pos.z - player->actor.world.pos.z;
    s32 angleDiff = ABS((s16)(Math_Atan2S(deltaX, deltaZ) - player->actor.shape.rot.y));

    if (SQ(deltaX) + SQ(deltaZ) >= SQ(NEB_CATCH_RANGE)) {
        return false;
    }
    return angleDiff < 0x4000;
}

static Actor* FindCatchableActor(Player* player, PlayState* play, uint8_t* content) {
    static const uint8_t categories[] = { ACTORCAT_SWITCH, ACTORCAT_BG,    ACTORCAT_PLAYER, ACTORCAT_EXPLOSIVE,
                                          ACTORCAT_NPC,    ACTORCAT_ENEMY, ACTORCAT_PROP,   ACTORCAT_ITEMACTION,
                                          ACTORCAT_MISC,   ACTORCAT_BOSS,  ACTORCAT_DOOR,   ACTORCAT_CHEST };

    for (uint8_t index = 0; index < ARRAY_COUNT(categories); index++) {
        for (Actor* actor = play->actorCtx.actorLists[categories[index]].head; actor != NULL; actor = actor->next) {
            uint8_t caught = actor->update == NULL ? ITEM_NONE : NetContentOf(actor);

            if (caught != ITEM_NONE && IsInFrontOfPlayer(player, actor)) {
                *content = caught;
                return actor;
            }
        }
    }
    return NULL;
}

static void ResolveNetCatch(Player* player, PlayState* play) {
    uint8_t content = ITEM_NONE;
    Actor* caught = FindCatchableActor(player, play, &content);

    if (caught == NULL) {
        return;
    }
    if (!HasEmptyBottle()) {
        sCatchMessage = "The net comes up full, and there&is no empty bottle to hold what&it caught.";
        return;
    }
    if (caught->id == ACTOR_EN_BUTTE) {
        Vec3f at = caught->world.pos;
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ELF, at.x, at.y, at.z, 0, 0, 0, FAIRY_HEAL_TIMED);
        Actor_Kill(caught);
        sCatchMessage = "The butterfly becomes a fairy!";
        return;
    }
    Item_Give(play, content);
    Actor_Kill(caught);
    Audio_PlayFanfare(NA_BGM_ITEM_GET | 0x900);
    sCatchMessage = "The net scoops it up, and it lands&in an empty bottle.";
}

s32 Player_UpperAction_Sword(Player* player, PlayState* play);
static int32_t HoldNet(Player* player, PlayState* play) {
    int result = Player_UpperAction_Sword(player, play);
    for (int i = 0; i < ARRAY_COUNT(player->meleeWeaponQuads); ++i)
        player->meleeWeaponQuads[i].base.atFlags &= ~AT_ON;
    if (player->meleeWeaponState) {
        if (!sIsCatchResolved) {
            sIsCatchResolved = true;
            ResolveNetCatch(player, play);
        }
    } else
        sIsCatchResolved = false;
    return result;
}
static bool NetHeld(void) {
    const char* key = CustomItemRegistry_GetHeldKey();
    return key && !strcmp(key, NEB_NET_KEY);
}
static void NetSwordHand(Player* player, int32_t limb, Gfx** dl, Gfx* original, Vec3f* pos) {
    if (player != GET_PLAYER(gPlayState)) {
        RemoteVisualState visual;
        if (limb == PLAYER_LIMB_L_HAND && ReadRemoteVisual(player, &visual)) {
            *dl = (Gfx*)(visual.adult ? gLinkAdultLeftHandClosedNearDL : gLinkChildLeftFistNearDL);
        }
        return;
    }

    if (gPlayState == NULL || player != GET_PLAYER(gPlayState)) {
        return;
    }

    if (NetHeld() && limb == PLAYER_LIMB_L_HAND)
        *dl = (Gfx*)(LINK_IS_ADULT ? gLinkAdultLeftHandClosedNearDL : gLinkChildLeftFistNearDL);
}
static void DrawNetInHand(PlayState* play, Player* player, int32_t limb) {
    RemoteVisualState visual;
    bool local = player == GET_PLAYER(play);
    bool held = local ? NetHeld() : ReadRemoteVisual(player, &visual);
    if (!held || limb != PLAYER_LIMB_L_HAND) {
        return;
    }
    Matrix_Push();
    float scale = player->actor.scale.x != 0 ? 1 / player->actor.scale.x : 1;
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_RotateX(DEG_TO_RAD(NEB_NET_HELD_ROT_X), MTXMODE_APPLY);
    Matrix_RotateY(DEG_TO_RAD(NEB_NET_HELD_ROT_Y), MTXMODE_APPLY);
    Matrix_RotateZ(DEG_TO_RAD(NEB_NET_HELD_ROT_Z), MTXMODE_APPLY);
    Matrix_Translate(NEB_NET_HELD_OFFSET_X, NEB_NET_HELD_OFFSET_Y, NEB_NET_HELD_OFFSET_Z, MTXMODE_APPLY);
    Matrix_Scale(NEB_NET_HELD_SCALE, NEB_NET_HELD_SCALE, NEB_NET_HELD_SCALE, MTXMODE_APPLY);
    if (local) {
        for (int i = 0; i < ARRAY_COUNT(sNetCatchPoints); i++) {
            float t = (float)i / (ARRAY_COUNT(sNetCatchPoints) - 1);
            Vec3f sample = { -57 * t, 55 * t, 0 };
            Matrix_MultVec3f(&sample, &sNetCatchPoints[i]);
        }
        sNetCatchPointsValid = true;
    }
    uint32_t savedRemoteEnv = CaptureRemoteLimbEnv(play);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sNetDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sNetXluDL);
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Pop();

    if (player != GET_PLAYER(play)) {
        RestoreRemoteLimbEnv(play, savedRemoteEnv);
    }
}

void func_80837948(PlayState* play, Player* player, s32 meleeWeaponAnim);
static void SwingNetRequest(Player* player, PlayState* play) {
    sNetCaptureFrames = 12;
    func_80837948(play, player, PLAYER_MWA_FORWARD_SLASH_1H);
}
static void TakeOutNet(PlayState* play, Player* player) {
    sNetCatchPointsValid = false;
    sIsCatchResolved = false;
    sCatchMessage = NULL;
}

static void DrawNetGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    Matrix_Scale(NEB_NET_GIVE_SCALE, NEB_NET_GIVE_SCALE, NEB_NET_GIVE_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sNetDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sNetXluDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawDigit(PlayState* play, const char* texture, int16_t x, int16_t y, uint8_t alpha) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, alpha);
    gDPLoadTextureBlock(OVERLAY_DISP++, texture, G_IM_FMT_IA, G_IM_SIZ_8b, NEB_DIGIT_SIZE, NEB_DIGIT_SIZE, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, x << 2, y << 2, (x + NEB_DIGIT_SIZE) << 2, (y + NEB_DIGIT_SIZE) << 2,
                            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    CLOSE_DISPS(play->state.gfxCtx);
}

static uint8_t ButtonAlpha(PlayState* play, uint8_t button) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    switch (button) {
        case 0:
            return (uint8_t)interfaceCtx->bAlpha;
        case 1:
            return (uint8_t)interfaceCtx->cLeftAlpha;
        case 2:
            return (uint8_t)interfaceCtx->cDownAlpha;
        case 3:
            return (uint8_t)interfaceCtx->cRightAlpha;
        case 4:
            return (uint8_t)interfaceCtx->dpadUpAlpha;
        case 5:
            return (uint8_t)interfaceCtx->dpadDownAlpha;
        case 6:
            return (uint8_t)interfaceCtx->dpadLeftAlpha;
        case 7:
            return (uint8_t)interfaceCtx->dpadRightAlpha;
        default:
            return 0;
    }
}

static NebButtonPos AmmoPosition(uint8_t button) {
    NebButtonPos position;

    if (button >= 4) {
        position = sDpadAmmoPos[button - 4];
        position.x = (int16_t)OTRGetDimensionFromRightEdge((float)position.x);
        return position;
    }
    position.x = OTRGetRectDimensionFromRightEdge((float)R_ITEM_AMMO_X(button));
    position.y = (int16_t)R_ITEM_AMMO_Y(button);
    return position;
}

static void DrawBottomlessCount(PlayState* play) {
    if (!IsOwned(NEB_BOTTOMLESS) || IsEmpty(NEB_BOTTOMLESS)) {
        return;
    }
    for (uint8_t button = 1; button < NEB_BUTTON_COUNT; button++) {
        uint8_t alpha = ButtonAlpha(play, button);

        if (BottleOnButton(button) != NEB_BOTTOMLESS || alpha == 0) {
            continue;
        }
        NebButtonPos position = AmmoPosition(button);
        uint8_t tens = (uint8_t)(sSave.bottomlessUses / 10);

        if (tens != 0) {
            DrawDigit(play, sDigitTex[tens], position.x, position.y, alpha);
        }
        DrawDigit(play, sDigitTex[sSave.bottomlessUses % 10], (int16_t)(position.x + NEB_DIGIT_GAP), position.y, alpha);
    }
}

static const char* VanillaOverlayIcon(int32_t bottle) {
    if (bottle < 0 || bottle >= NEB_VANILLA_COUNT) {
        return NULL;
    }
    if (sForeignVanilla[bottle] != 0) {
        return ForeignOf(sForeignVanilla[bottle])->icon;
    }
    return sChateauBottles[NEB_BOTTLE_COUNT + bottle] != 0 ? sChateauContentIcon : NULL;
}

static void ChateauButtonIcon(PlayState* play, u8 button, u16 item, const char** icon) {
    const int8_t bottle = VanillaBottleOnButton(button);
    const char* overlay = bottle != NEB_NONE ? VanillaOverlayIcon(bottle) : NULL;
    if (overlay != NULL) {
        *icon = overlay;
    }
}

void KaleidoScope_DrawQuadTextureRGBA32(GraphicsContext* gfxCtx, void* texture, s16 width, s16 height, u16 point);
static void ChateauPauseIcon(PlayState* play, int32_t cell, int32_t item, Vtx* vertices) {
    const char* overlay = VanillaOverlayIcon(InventorySlotForCell(cell) - SLOT_BOTTLE_1);
    if (overlay == NULL) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gSPVertex(POLY_OPA_DISP++, (uintptr_t)vertices, 4, 0);
    KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, (void*)overlay, 32, 32, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}
static const SOHCustomItemRandomizer sNetRandomizer = {
    sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, NEB_NET_KEY, NEB_NET_KEY, "",
};
static const SOHCustomItemRandomizer sBottomlessRandomizer = {
    sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, NEB_BOTTOMLESS_KEY, NEB_BOTTOMLESS_KEY, "",
};

static bool sRemoteVisualReady;
static bool sRemoteVisualSent;
static uint32_t sRemoteVisualFrame;
static RemoteVisualState sLastRemoteVisual;

static void ReceiveRemoteVisual(const SOHNetMessage* message) {
    (void)message;
}

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual) {
    if (!sRemoteVisualReady || gPlayState == NULL || player == GET_PLAYER(gPlayState)) {
        return false;
    }
    uint32_t peerId = sApi->GetNetPeerIdForActor(&player->actor);
    if (peerId == 0 ||
        sApi->GetNetPeerState(peerId, "nei.not_enough_bottles.visual", visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->adult > 1 || visual->scene != gPlayState->sceneNum || visual->flags[0] > 255 ||
        (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return false;
    }
    for (uint32_t i = 0; i < ARRAY_COUNT(visual->values); i++) {
        if (!isfinite(visual->values[i]) || fabsf(visual->values[i]) > 10000000.0f) {
            return false;
        }
    }
    return true;
}

static void PublishRemoteVisual(void) {
    if (!sRemoteVisualReady) {
        return;
    }
    RemoteVisualState visual = { 0 };
    PlayState* play = gPlayState;
    visual.scene = -1;
    if (play != NULL) {
        Player* player = GET_PLAYER(play);
        visual.scene = play->sceneNum;
        visual.adult = LINK_IS_ADULT;
        visual.active = (NetHeld());
        if (visual.active) {}
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (!sRemoteVisualSent || memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) != 0) {
        if (sApi->SetNetPeerState("nei.not_enough_bottles.visual", &visual, sizeof(visual))) {
            sLastRemoteVisual = visual;
            sRemoteVisualSent = true;
            sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
        }
    }
}

static void InitRemoteVisuals(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterNetChannel) || !SOH_MOD_API_HAS(sApi, SetNetPeerState) ||
        !SOH_MOD_API_HAS(sApi, GetNetPeerState) || !SOH_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel("nei.not_enough_bottles.visual", 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
    }
}

static uint32_t CaptureRemoteLimbEnv(PlayState* play) {
    Gfx* begin = play->state.gfxCtx->polyOpa.bufp;
    Gfx* cursor = play->state.gfxCtx->polyOpa.p;
    while (cursor > begin) {
        cursor--;
        if (((cursor->words.w0 >> 24) & 255) == G_SETENVCOLOR) {
            return (uint32_t)cursor->words.w1;
        }
    }
    return UINT32_MAX;
}

static void RestoreRemoteLimbEnv(PlayState* play, uint32_t color) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, color >> 24, (color >> 16) & 255, (color >> 8) & 255, color & 255);
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

static void DrawBottleModel(PlayState* play, bool black) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiBottleStopperDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    if (black) {
        gDPSetGrayscaleColor(POLY_XLU_DISP++, 28, 28, 36, 255);
        gSPGrayscale(POLY_XLU_DISP++, true);
    }
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiBottleDL);
    if (black) {
        gSPGrayscale(POLY_XLU_DISP++, false);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
static void DrawExtraBottle(PlayState* play, GetItemEntry* entry) {
    DrawBottleModel(play, false);
}
static void DrawBottomlessBottle(PlayState* play, GetItemEntry* entry) {
    DrawBottleModel(play, true);
}

static bool RegisterNet(void) {
    SOHCustomItemDefinition net = Z64Items_Define(NEB_NET_KEY, sNetIconTex, sNetNameTex);

    Z64Items_SetButtons(&net, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&net, SOH_ITEM_PAGE_VANILLA, SLOT_BOTTLE_3, 0);
    Z64Items_SetTextbox(&net, "You got the %rNet%w! A&long-handled net that scoops&up what a bottle would.^"
                              "Swing it with %y\xA1%w at a fairy, a&fish, a bug or a flame and it&"
                              "lands in an empty bottle.");
    Z64Items_SetPauseText(&net, "%rNet&%wSwing it with %y\xA1%w to catch something&into an empty bottle.");
    Z64Items_SetAction(&net, TakeOutNet, HoldNet);
    Z64Items_SetMeleeWeapon(&net, 2);
    Z64Items_SetHeldCallbacks(&net, SwingNetRequest, NULL, NULL);
    net.modelGroup = PLAYER_MODELGROUP_SWORD_AND_SHIELD;
    net.getItemEntry.drawFunc = DrawNetGetItem;
    Z64Items_SetLogic(&net, &sNetRandomizer);
    return Z64Items_Register(sApi, &net);
}

static bool RegisterBottomless(void) {
    SOHCustomItemDefinition bottomless = Z64Items_Define(NEB_BOTTOMLESS_KEY, sBottomlessIconTex, sBottomlessNameTex);

    Z64Items_SetButtons(&bottomless, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    bottomless.flags |= SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&bottomless, SOH_ITEM_PAGE_VANILLA, SLOT_BOTTLE_4, 0);
    Z64Items_SetAmmo(&bottomless, GetBottomlessUses);
    Z64Items_SetTextbox(&bottomless,
                        "You got the %rBottomless Bottle%w!&A bottle that never empties at&once.^"
                        "Whatever you put in it lasts&for several uses: the number&on its button counts them&"
                        "down.");
    Z64Items_SetPauseText(&bottomless, "%rBottomless Bottle&%wIt holds one content for several&"
                                       "uses. The number counts them&down.");
    bottomless.getItemEntry.drawFunc = DrawBottomlessBottle;
    Z64Items_SetLogic(&bottomless, &sBottomlessRandomizer);
    return Z64Items_Register(sApi, &bottomless);
}

static const int32_t sBottlePoolSources[] = { RG_EMPTY_BOTTLE,
                                              RG_BOTTLE_WITH_MILK,
                                              RG_BOTTLE_WITH_RED_POTION,
                                              RG_BOTTLE_WITH_GREEN_POTION,
                                              RG_BOTTLE_WITH_BLUE_POTION,
                                              RG_BOTTLE_WITH_FAIRY,
                                              RG_BOTTLE_WITH_FISH,
                                              RG_BOTTLE_WITH_BUGS,
                                              RG_BOTTLE_WITH_POE,
                                              RG_BOTTLE_WITH_BIG_POE,
                                              RG_BOTTLE_WITH_BLUE_FIRE };
static bool RegisterExtraBottle(int8_t bottle) {
    SOHCustomItemDefinition extra =
        Z64Items_Define(sBottleKeys[bottle], (const char*)gItemIcons[ITEM_BOTTLE], gEmptyBottleItemNameENGTex);

    Z64Items_SetButtons(&extra, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    extra.flags |= SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&extra, SOH_ITEM_PAGE_VANILLA, sExtraCells[bottle], sExtraPriorities[bottle]);
    extra.getItemEntry = ItemTable_RetrieveEntry(MOD_NONE, GI_BOTTLE);
    extra.getItemEntry.textId = 0;
    extra.getItemEntry.drawFunc = DrawExtraBottle;
    extra.onAcquire = NoteAcquiredBottle;
    extra.onReceive = ReceiveExtraBottle;
    Z64Items_SetTextbox(&extra, "You got an %rEmpty Bottle%w! You&can put something in this&bottle.");
    Z64Items_SetPauseText(&extra, "%rBottle&%wWhatever it holds, %y\xA1%w uses it like&any other bottle.");
    const SOHCustomItemRandomizer randomizer = {
        sizeof(SOHCustomItemRandomizer),
        0,
        SOH_CUSTOM_ITEM_TYPE_ITEM,
        0,
        1,
        sBottleKeys[bottle],
        sBottleKeys[bottle],
        "",
        NULL,
        NULL,
        0,
        false,
        sBottlePoolSources,
        ARRAY_COUNT(sBottlePoolSources),
    };
    Z64Items_SetLogic(&extra, &randomizer);
    return Z64Items_Register(sApi, &extra);
}

static bool RegisterSharedBottle(const char* key, const char* icon, const char* name, const char* text) {
    SOHCustomItemDefinition shared = Z64Items_Define(key, icon, name);

    shared.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO | SOH_ITEM_HIDE_FROM_SAVE_EDITOR;
    shared.getItemEntry = ItemTable_RetrieveEntry(MOD_NONE, GI_BOTTLE);
    shared.getItemEntry.textId = 0;
    shared.getItemEntry.drawFunc = DrawExtraBottle;
    shared.onReceive = ReceiveSharedBottle;
    Z64Items_SetTextbox(&shared, text);
    const SOHCustomItemRandomizer randomizer = {
        sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 0, key, key, "",
    };
    Z64Items_SetLogic(&shared, &randomizer);
    return Z64Items_Register(sApi, &shared);
}

static void RegisterSharedBottles(void) {
    RegisterSharedBottle(NEB_GOLD_DUST_KEY, ForeignOf(NEB_FOREIGN_GOLD_DUST)->icon,
                         ForeignOf(NEB_FOREIGN_GOLD_DUST)->name,
                         "You got a %rBottle of Gold Dust%w!&It shines like Termina's finest.");
    RegisterSharedBottle(NEB_CHATEAU_KEY, sChateauContentIcon, sChateauContentName,
                         "You got a %rBottle of Chateau Romani%w!&Drink it for infinite magic.");
}

static void GroupVanillaBottles(void) {
    if (!SOH_MOD_API_HAS(sApi, PlaceLayoutItem)) {
        return;
    }
    sApi->PlaceLayoutItem("vanilla.0.20", "vanilla", SLOT_BOTTLE_1);
    sApi->PlaceLayoutItem("vanilla.0.21", "vanilla", SLOT_BOTTLE_2);
}

SOH_MOD_EXPORT void ModInit(void) {
    InitRemoteVisuals();
    if (!SOH_MOD_API_HAS(sApi, StorageRemove) || !SOH_MOD_API_HAS(sApi, KeepCustomItemHeld)) {
        return;
    }
    if (!RegisterNet() || !RegisterBottomless()) {
        return;
    }
    for (int8_t bottle = 0; bottle < NEB_EXTRA_COUNT; bottle++) {
        if (!RegisterExtraBottle(bottle)) {
            return;
        }
    }
    if (SOH_MOD_API_HAS(sApi, PublishService))
        sApi->PublishService("nei.bottles.fill_chateau", (void*)FillChateauBottle);
    if (SOH_MOD_API_HAS(sApi, PublishService)) {
        sApi->PublishService("nei.bottles.read", (void*)ReadBottles);
        sApi->PublishService("nei.bottles.write", (void*)WriteBottles);
    }
    RegisterSharedBottles();
    GroupVanillaBottles();
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, SayTimeAndPlace);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, NetSwordHand);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawNetInHand);
    sApi->RegisterVB(VB_UPDATE_BOTTLE_ITEM, CatchBottleWrite);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveItemAction, ResolveBottleAction);
    SOH_REGISTER_HOOK(sApi, OnResolveItemGive, GiveIntoBottles);
    SOH_REGISTER_HOOK(sApi, OnItemReceive, CompleteSeededBottle);
    SOH_REGISTER_HOOK(sApi, OnKaleidoResolveName, ResolveBottleName);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, CloseFrame);
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, CloseFrame);
    SOH_REGISTER_HOOK(sApi, OnFlagSet, TakeBackRutosLetter);
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawBottomlessCount);
    SOH_REGISTER_HOOK(sApi, OnInterfaceResolveButtonIcon, ChateauButtonIcon);
    SOH_REGISTER_HOOK(sApi, OnKaleidoItemDraw, ChateauPauseIcon);
    SOH_REGISTER_HOOK(sApi, OnLoadFile, LoadBottles);
    SOH_REGISTER_HOOK_FOR_ID(sApi, ShouldActorUpdate, ACTOR_PLAYER, LendFairyToFallingLink);
    SOH_REGISTER_HOOK_FOR_ID(sApi, ShouldActorUpdate, ACTOR_EN_HY, LendForBeggar);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_HY, CloseActorWindow);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_GB, CloseActorWindow);
    for (uint8_t i = 0; i < ARRAY_COUNT(sEmptyBottleAskers); i++) {
        SOH_REGISTER_HOOK_FOR_ID(sApi, ShouldActorUpdate, sEmptyBottleAskers[i], LendForEmptyBottleAsker);
        SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, sEmptyBottleAskers[i], CloseActorWindow);
    }
    LoadBottles(0);
}
