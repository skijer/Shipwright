
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "z64items.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define MOD_NAME "golden_hearts"
#define CONTAINER_POOL_OPTION "oot.golden_hearts"
#define PIECE_POOL_OPTION "oot.golden_heart_pieces"
#define STARTING_OPTION "oot.starting_golden_hearts"
#define GOLDEN_HEART_KEY "nei.heart.golden"
#define GOLDEN_PIECE_KEY "nei.heart.golden_piece"
#define RED_QUARTERS_SERVICE "nei.heart.golden_red_quarters"
#define CAPACITY_KEY "capacity"
#define HEALTH_KEY "health"
#define PIECES_KEY "pieces"
#define RED_HEARTS_KEY "red_hearts"
#define MAX_GOLDEN_HEARTS 20
#define HEART_HEALTH 16
#define MAX_RED_HEALTH (20 * HEART_HEALTH)
#define PIECES_PER_HEART 4
#define FULL_REFILL 0x140
#define MAX_POOL_CONTAINERS 40

s16 getHealthMeterXOffset(void);
s16 getHealthMeterYOffset(void);

static const char* const sRequiredHooks[] = { "OnPlayerHealthChange", "OnCustomItemReceive", "OnLoadFile",
                                              "OnInterfaceDrawEnd" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const char sHeartFullTex[] = "__OTR__textures/parameter_static/gHeartFullTex";
static const char* const sHeartFractionTex[] = {
    NULL,
    "__OTR__textures/parameter_static/gHeartQuarterTex",
    "__OTR__textures/parameter_static/gHeartHalfTex",
    "__OTR__textures/parameter_static/gHeartThreeQuarterTex",
};

static const SOHModApi* sApi;
static u8 sCapacity;
static s16 sHealth;
static u8 sPieces;
static u8 sRedHearts;

static void Store(void) {
    sApi->StorageSet(MOD_NAME, CAPACITY_KEY, &sCapacity, sizeof(sCapacity));
    sApi->StorageSet(MOD_NAME, HEALTH_KEY, &sHealth, sizeof(sHealth));
    sApi->StorageSet(MOD_NAME, PIECES_KEY, &sPieces, sizeof(sPieces));
    sApi->StorageSet(MOD_NAME, RED_HEARTS_KEY, &sRedHearts, sizeof(sRedHearts));
}

static s16 GetMaxHealth(void) {
    return sCapacity * HEART_HEALTH;
}

static s32 ReadRedQuarters(void) {
    return sRedHearts * PIECES_PER_HEART;
}

static void ReadStoredByte(const char* key, u8* value) {
    if (sApi->StorageHas(MOD_NAME, key)) {
        sApi->StorageGet(MOD_NAME, key, value, sizeof(*value));
    }
}

static void LoadGoldenHearts(int32_t fileNum) {
    sCapacity = 0;
    sHealth = 0;
    sPieces = 0;
    sRedHearts = 0;
    if (sApi->StorageHas(MOD_NAME, CAPACITY_KEY)) {
        sApi->StorageGet(MOD_NAME, CAPACITY_KEY, &sCapacity, sizeof(sCapacity));
        sApi->StorageGet(MOD_NAME, HEALTH_KEY, &sHealth, sizeof(sHealth));
        ReadStoredByte(PIECES_KEY, &sPieces);
        ReadStoredByte(RED_HEARTS_KEY, &sRedHearts);
        return;
    }
    if (IS_RANDO) {
        sCapacity = MIN(sApi->GetRandoOption(STARTING_OPTION), MAX_GOLDEN_HEARTS);
        sHealth = GetMaxHealth();
        Store();
    }
}

static void GrantWholeHeart(void) {
    if (gSaveContext.healthCapacity < MAX_RED_HEALTH) {
        gSaveContext.healthCapacity += HEART_HEALTH;
        gSaveContext.healthAccumulator = FULL_REFILL;
        sRedHearts++;
    } else if (sCapacity < MAX_GOLDEN_HEARTS) {
        sCapacity++;
    }
    sHealth = GetMaxHealth();
}

static void ReceiveGoldenHeart(const char* key) {
    const bool isContainer = strcmp(key, GOLDEN_HEART_KEY) == 0;
    if (!isContainer && strcmp(key, GOLDEN_PIECE_KEY) != 0) {
        return;
    }
    if (!isContainer) {
        sPieces = (sPieces + 1) % PIECES_PER_HEART;
    }
    if (isContainer || sPieces == 0) {
        GrantWholeHeart();
    }
    Store();
}

// Health_ChangeBy runs this before its death check, so restored red health can still save Link.
static void ShieldWithGoldenHearts(int16_t amount) {
    if (amount < 0 && sHealth > 0) {
        const s16 absorbed = MIN(-amount, sHealth);
        sHealth -= absorbed;
        gSaveContext.health = MIN(gSaveContext.health + absorbed, gSaveContext.healthCapacity);
        Store();
    } else if (amount > 0 && gSaveContext.health >= gSaveContext.healthCapacity && sHealth < GetMaxHealth()) {
        sHealth = MIN(sHealth + amount, GetMaxHealth());
        Store();
    }
}

static f32 GetHeartsScale(void) {
    if (CVarGetInteger("gCosmetics.HUD.HeartsCount.PosType", 0) == 0) {
        return 0.7f;
    }
    return CVarGetFloat("gCosmetics.HUD.HeartsCount.Scale", 0.7f);
}

static void DrawHeart(PlayState* play, s32 index, const char* texture, f32 scale) {
    const s32 lineLength = MAX(CVarGetInteger("gCosmetics.HUD.Hearts.LineLength", 10), 1);
    const f32 posX = 30.0f + getHealthMeterXOffset() + 10.0f * (index % lineLength);
    const f32 posY = 26.0f + getHealthMeterYOffset() + 10.0f * (index / lineLength);
    const f32 halfSize = 8.0f * scale;
    const s32 step = (s32)((1.0f / scale) * (1 << 10));

    OPEN_DISPS(play->state.gfxCtx);
    gDPLoadTextureBlock(OVERLAY_DISP++, texture, G_IM_FMT_IA, G_IM_SIZ_8b, 16, 16, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, (s32)((posX - halfSize) * 4), (s32)((posY - halfSize) * 4),
                            (s32)((posX + halfSize) * 4), (s32)((posY + halfSize) * 4), G_TX_RENDERTILE, 0, 0, step,
                            step);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGoldenHearts(PlayState* play) {
    if (sHealth <= 0 || play->interfaceCtx.healthAlpha == 0) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 215, 40, play->interfaceCtx.healthAlpha);
    gDPSetEnvColor(OVERLAY_DISP++, 140, 90, 0, 255);
    CLOSE_DISPS(play->state.gfxCtx);

    const f32 scale = GetHeartsScale();
    const s32 fullHearts = sHealth / HEART_HEALTH;
    const s32 quarters = (sHealth % HEART_HEALTH) / (HEART_HEALTH / 4);
    for (s32 i = 0; i < fullHearts; i++) {
        DrawHeart(play, i, sHeartFullTex, scale);
    }
    if (quarters > 0) {
        DrawHeart(play, fullHearts, sHeartFractionTex[quarters], scale);
    }
}

#define COUNTS_IN_TENS(tens) \
    tens "0", tens "1", tens "2", tens "3", tens "4", tens "5", tens "6", tens "7", tens "8", tens "9"

static const char* const sCounts[] = { "0",
                                       "1",
                                       "2",
                                       "3",
                                       "4",
                                       "5",
                                       "6",
                                       "7",
                                       "8",
                                       "9",
                                       COUNTS_IN_TENS("1"),
                                       COUNTS_IN_TENS("2"),
                                       COUNTS_IN_TENS("3"),
                                       COUNTS_IN_TENS("4"),
                                       COUNTS_IN_TENS("5"),
                                       COUNTS_IN_TENS("6"),
                                       COUNTS_IN_TENS("7"),
                                       "80" };

static const SOHModRandoOption sOptions[] = {
    { sizeof(SOHModRandoOption), CONTAINER_POOL_OPTION, "Golden Heart Containers",
      "Golden Heart Containers added to the item pool. Each adds a heart; past twenty hearts, a golden heart.", sCounts,
      MAX_POOL_CONTAINERS + 1, 0, SOH_MOD_RANDO_WIDGET_SLIDER },
    { sizeof(SOHModRandoOption), PIECE_POOL_OPTION, "Golden Heart Pieces",
      "Golden Heart Pieces added to the item pool. Four make a heart; past twenty hearts, a golden heart.", sCounts,
      ARRAY_COUNT(sCounts), 0, SOH_MOD_RANDO_WIDGET_SLIDER },
    { sizeof(SOHModRandoOption), STARTING_OPTION, "Starting Golden Hearts",
      "Golden hearts Link starts with, past the twenty red ones.", sCounts, MAX_GOLDEN_HEARTS + 1, 0,
      SOH_MOD_RANDO_WIDGET_SLIDER },
};

typedef struct {
    const char* key;
    const char* name;
    const char* icon;
    const char* model;
    const char* text;
    const char* poolOption;
} GoldenItem;

static const GoldenItem sGoldenItems[] = {
    { GOLDEN_HEART_KEY, "Golden Heart Container", "__OTR__textures/icon_item_custom/gItemIconGoldenHeartTex",
      "__OTR__objects/object_gi_hearts/gGiHeartContainerDL",
      "You got a %yGolden Heart Container%w!&Your life grows by one heart.", CONTAINER_POOL_OPTION },
    { GOLDEN_PIECE_KEY, "Golden Heart Piece", "__OTR__textures/icon_item_custom/gItemIconGoldenHeartPieceTex",
      "__OTR__objects/object_gi_hearts/gGiHeartPieceDL",
      "You got a %yGolden Heart Piece%w!&Four of them make a whole heart.", PIECE_POOL_OPTION },
};

static void RegisterGoldenItem(const GoldenItem* item) {
    const SOHCustomItemRandomizer pool = {
        .structSize = sizeof(SOHCustomItemRandomizer),
        .type = SOH_CUSTOM_ITEM_TYPE_ITEM,
        .poolCount = 0,
        .logicKey = item->key,
        .hintKey = item->key,
        .poolCountOption = item->poolOption,
    };
    SOHCustomItemDefinition definition = Z64Items_Define(item->key, item->icon, NULL);
    definition.displayName = item->name;
    definition.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&definition, item->text);
    Z64Items_SetGetItemModel(&definition, item->model);
    Z64Items_SetLogic(&definition, &pool);
    Z64Items_Register(sApi, &definition);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterRandoOption) || !SOH_MOD_API_HAS(sApi, StorageSet)) {
        fprintf(stderr, "[golden_hearts] The host lacks the randomizer or storage API; the mod stays off\n");
        return;
    }
    for (u32 i = 0; i < ARRAY_COUNT(sOptions); i++) {
        sApi->RegisterRandoOption(&sOptions[i]);
    }
    for (u32 i = 0; i < ARRAY_COUNT(sGoldenItems); i++) {
        RegisterGoldenItem(&sGoldenItems[i]);
    }
    if (SOH_MOD_API_HAS(sApi, PublishService)) {
        sApi->PublishService(RED_QUARTERS_SERVICE, (void*)ReadRedQuarters);
    }
    SOH_REGISTER_HOOK(sApi, OnLoadFile, LoadGoldenHearts);
    SOH_REGISTER_HOOK(sApi, OnCustomItemReceive, ReceiveGoldenHeart);
    SOH_REGISTER_HOOK(sApi, OnPlayerHealthChange, ShieldWithGoldenHearts);
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawGoldenHearts);
}
