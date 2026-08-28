#include "global.h"
#include "textures/parameter_static/parameter_static.h"
#include "soh/frame_interpolation.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/cosmetics/cosmeticsTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define LOCAL_MP_PLAYER_COUNT_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount")
#define LOCAL_MP_DISABLED_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.Disable")
#define LOCAL_MP_SPLITSCREEN_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreen")
#define LOCAL_MP_SPLITSCREEN_VERTICAL_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreenVertical")

s16 Top_LM_Margin = 0;
s16 Left_LM_Margin = 0;
s16 Right_LM_Margin = 0;
s16 Bottom_LM_Margin = 0;

static s16 sHeartsPrimColors[3][3] = {
    { HEARTS_PRIM_R, HEARTS_PRIM_G, HEARTS_PRIM_B },
    { HEARTS_BURN_PRIM_R, HEARTS_BURN_PRIM_G, HEARTS_BURN_PRIM_B },    // unused
    { HEARTS_DROWN_PRIM_R, HEARTS_DROWN_PRIM_G, HEARTS_DROWN_PRIM_B }, // unused
};

static s16 sHeartsEnvColors[3][3] = {
    { HEARTS_ENV_R, HEARTS_ENV_G, HEARTS_ENV_B },
    { HEARTS_BURN_ENV_R, HEARTS_BURN_ENV_G },                       // unused
    { HEARTS_DROWN_ENV_R, HEARTS_DROWN_ENV_G, HEARTS_DROWN_ENV_B }, // unused
};

static s16 sHeartsPrimFactors[3][3] = {
    {
        HEARTS_PRIM_R - HEARTS_PRIM_R,
        HEARTS_PRIM_G - HEARTS_PRIM_G,
        HEARTS_PRIM_B - HEARTS_PRIM_B,
    },
    // unused
    {
        HEARTS_BURN_PRIM_R - HEARTS_PRIM_R,
        HEARTS_BURN_PRIM_G - HEARTS_PRIM_G,
        HEARTS_BURN_PRIM_B - HEARTS_PRIM_B,
    },
    // unused
    {
        HEARTS_DROWN_PRIM_R - HEARTS_PRIM_R,
        HEARTS_DROWN_PRIM_G - HEARTS_PRIM_G,
        HEARTS_DROWN_PRIM_B - HEARTS_PRIM_B,
    },
};

static s16 sHeartsEnvFactors[3][3] = {
    {
        HEARTS_ENV_R - HEARTS_ENV_R,
        HEARTS_ENV_G - HEARTS_ENV_G,
        HEARTS_ENV_B - HEARTS_ENV_B,
    },
    // unused
    {
        HEARTS_BURN_ENV_R - HEARTS_ENV_R,
        HEARTS_BURN_ENV_G - HEARTS_ENV_G,
        HEARTS_BURN_ENV_B - HEARTS_ENV_B,
    },
    // unused
    {
        HEARTS_DROWN_ENV_R - HEARTS_ENV_R,
        HEARTS_DROWN_ENV_G - HEARTS_ENV_G,
        HEARTS_DROWN_ENV_B - HEARTS_ENV_B,
    },
};

static s16 sHeartsDDPrimColors[3][3] = {
    { HEARTS_DD_PRIM_R, HEARTS_DD_PRIM_G, HEARTS_DD_PRIM_B },
    { HEARTS_BURN_PRIM_R, HEARTS_BURN_PRIM_G, HEARTS_BURN_PRIM_B },    // unused
    { HEARTS_DROWN_PRIM_R, HEARTS_DROWN_PRIM_G, HEARTS_DROWN_PRIM_B }, // unused
};

static s16 sHeartsDDEnvColors[3][3] = {
    { HEARTS_DD_ENV_R, HEARTS_DD_ENV_G, HEARTS_DD_ENV_B },
    { HEARTS_BURN_ENV_R, HEARTS_BURN_ENV_G, HEARTS_BURN_ENV_B },    // unused
    { HEARTS_DROWN_ENV_R, HEARTS_DROWN_ENV_G, HEARTS_DROWN_ENV_B }, // unused
};

static s16 sHeartsDDPrimFactors[3][3] = {
    {
        HEARTS_DD_PRIM_R - HEARTS_DD_PRIM_R,
        HEARTS_DD_PRIM_G - HEARTS_DD_PRIM_G,
        HEARTS_DD_PRIM_B - HEARTS_DD_PRIM_B,
    },
    // unused
    {
        HEARTS_BURN_PRIM_R - HEARTS_DD_PRIM_R,
        HEARTS_BURN_PRIM_G - HEARTS_DD_PRIM_G,
        HEARTS_BURN_PRIM_B - HEARTS_DD_PRIM_B,
    },
    // unused
    {
        HEARTS_DROWN_PRIM_R - HEARTS_DD_PRIM_R,
        HEARTS_DROWN_PRIM_G - HEARTS_DD_PRIM_G,
        HEARTS_DROWN_PRIM_B - HEARTS_DD_PRIM_B,
    },
};

static s16 sHeartsDDEnvFactors[3][3] = {
    {
        HEARTS_DD_ENV_R - HEARTS_DD_ENV_R,
        HEARTS_DD_ENV_G - HEARTS_DD_ENV_G,
        HEARTS_DD_ENV_B - HEARTS_DD_ENV_B,
    },
    // unused
    {
        HEARTS_BURN_ENV_R - HEARTS_DD_ENV_R,
        HEARTS_BURN_ENV_G - HEARTS_DD_ENV_G,
        HEARTS_BURN_ENV_B - HEARTS_DD_ENV_B,
    },
    // unused
    {
        HEARTS_DROWN_ENV_R - HEARTS_DD_ENV_R,
        HEARTS_DROWN_ENV_G - HEARTS_DD_ENV_G,
        HEARTS_DROWN_ENV_B - HEARTS_DD_ENV_B,
    },
};

// Current colors for the double defense hearts
s16 sBeatingHeartsDDPrim[3];
s16 sBeatingHeartsDDEnv[3];
s16 sHeartsDDPrim[2][3];
s16 sHeartsDDEnv[2][3];

void HealthMeter_Init(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Color_RGB8 mainColor = { HEARTS_PRIM_R, HEARTS_PRIM_G, HEARTS_PRIM_B };
    if (CVarGetInteger(CVAR_COSMETIC("Consumable.Hearts.Changed"), 0)) {
        mainColor = CVarGetColor24(CVAR_COSMETIC("Consumable.Hearts.Value"), mainColor);
    }
    Color_RGB8 mainBorder = { HEARTS_ENV_R, HEARTS_ENV_G, HEARTS_ENV_B };
    if (CVarGetInteger(CVAR_COSMETIC("Consumable.HeartBorder.Changed"), 0)) {
        mainBorder = CVarGetColor24(CVAR_COSMETIC("Consumable.HeartBorder.Value"), mainBorder);
    }
    Color_RGB8 ddColor = { HEARTS_DD_ENV_R, HEARTS_DD_ENV_G, HEARTS_DD_ENV_B };
    if (CVarGetInteger(CVAR_COSMETIC("Consumable.DDHearts.Changed"), 0)) {
        ddColor = CVarGetColor24(CVAR_COSMETIC("Consumable.DDHearts.Value"), ddColor);
    }
    Color_RGB8 ddBorder = { HEARTS_DD_PRIM_R, HEARTS_DD_PRIM_G, HEARTS_DD_PRIM_B };
    if (CVarGetInteger(CVAR_COSMETIC("Consumable.DDHeartBorder.Changed"), 0)) {
        ddBorder = CVarGetColor24(CVAR_COSMETIC("Consumable.DDHeartBorder.Value"), ddBorder);
    }

    interfaceCtx->unk_228 = 0x140;
    interfaceCtx->unk_226 = gSaveContext.health;
    interfaceCtx->unk_22A = interfaceCtx->unk_1FE = 0;
    interfaceCtx->unk_22C = interfaceCtx->unk_200 = 0;

    interfaceCtx->heartsPrimR[0] = mainColor.r;
    interfaceCtx->heartsPrimG[0] = mainColor.g;
    interfaceCtx->heartsPrimB[0] = mainColor.b;

    interfaceCtx->heartsEnvR[0] = mainBorder.r;
    interfaceCtx->heartsEnvG[0] = mainBorder.g;
    interfaceCtx->heartsEnvB[0] = mainBorder.b;

    interfaceCtx->heartsPrimR[1] = mainColor.r;
    interfaceCtx->heartsPrimG[1] = mainColor.g;
    interfaceCtx->heartsPrimB[1] = mainColor.b;

    interfaceCtx->heartsEnvR[1] = mainBorder.r;
    interfaceCtx->heartsEnvG[1] = mainBorder.g;
    interfaceCtx->heartsEnvB[1] = mainBorder.b;

    sHeartsDDPrim[0][0] = sHeartsDDPrim[1][0] = ddBorder.r;
    sHeartsDDPrim[0][1] = sHeartsDDPrim[1][1] = ddBorder.g;
    sHeartsDDPrim[0][2] = sHeartsDDPrim[1][2] = ddBorder.b;

    sHeartsDDEnv[0][0] = sHeartsDDEnv[1][0] = ddColor.r;
    sHeartsDDEnv[0][1] = sHeartsDDEnv[1][1] = ddColor.g;
    sHeartsDDEnv[0][2] = sHeartsDDEnv[1][2] = ddColor.b;
}

void HealthMeter_Update(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    f32 factor = interfaceCtx->unk_1FE * 0.1f;
    f32 ddFactor;
    s32 type = 0;
    s32 ddType;
    s16 rFactor;
    s16 gFactor;
    s16 bFactor;

    Top_LM_Margin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.T"), 0);
    Left_LM_Margin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.L"), 0);
    Right_LM_Margin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.R"), 0);
    Bottom_LM_Margin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.B"), 0);

    Color_RGB8 mainColor = { HEARTS_PRIM_R, HEARTS_PRIM_G, HEARTS_PRIM_B };
    if (CVarGetInteger(CVAR_COSMETIC("Consumable.Hearts.Changed"), 0)) {
        mainColor = CVarGetColor24(CVAR_COSMETIC("Consumable.Hearts.Value"), mainColor);
    }
    Color_RGB8 mainBorder = { HEARTS_ENV_R, HEARTS_ENV_G, HEARTS_ENV_B };
    if (CVarGetInteger(CVAR_COSMETIC("Consumable.HeartBorder.Changed"), 0)) {
        mainBorder = CVarGetColor24(CVAR_COSMETIC("Consumable.HeartBorder.Value"), mainBorder);
    }
    Color_RGB8 ddColor = { HEARTS_DD_ENV_R, HEARTS_DD_ENV_G, HEARTS_DD_ENV_B };
    if (CVarGetInteger(CVAR_COSMETIC("Consumable.DDHearts.Changed"), 0)) {
        ddColor = CVarGetColor24(CVAR_COSMETIC("Consumable.DDHearts.Value"), ddColor);
    }
    Color_RGB8 ddBorder = { HEARTS_DD_PRIM_R, HEARTS_DD_PRIM_G, HEARTS_DD_PRIM_B };
    if (CVarGetInteger(CVAR_COSMETIC("Consumable.DDHeartBorder.Changed"), 0)) {
        ddBorder = CVarGetColor24(CVAR_COSMETIC("Consumable.DDHeartBorder.Value"), ddBorder);
    }

    if (interfaceCtx->unk_200 != 0) {
        interfaceCtx->unk_1FE--;
        if (interfaceCtx->unk_1FE <= 0) {
            interfaceCtx->unk_1FE = 0;
            interfaceCtx->unk_200 = 0;
        }
    } else {
        interfaceCtx->unk_1FE++;
        if (interfaceCtx->unk_1FE >= 10) {
            interfaceCtx->unk_1FE = 10;
            interfaceCtx->unk_200 = 1;
        }
    }

    ddFactor = factor;

    interfaceCtx->heartsPrimR[0] = mainColor.r;
    interfaceCtx->heartsPrimG[0] = mainColor.g;
    interfaceCtx->heartsPrimB[0] = mainColor.b;

    interfaceCtx->heartsEnvR[0] = mainBorder.r;
    interfaceCtx->heartsEnvG[0] = mainBorder.g;
    interfaceCtx->heartsEnvB[0] = mainBorder.b;

    interfaceCtx->heartsPrimR[1] = mainColor.r;
    interfaceCtx->heartsPrimG[1] = mainColor.g;
    interfaceCtx->heartsPrimB[1] = mainColor.b;

    interfaceCtx->heartsEnvR[1] = mainBorder.r;
    interfaceCtx->heartsEnvG[1] = mainBorder.g;
    interfaceCtx->heartsEnvB[1] = mainBorder.b;

    rFactor = sHeartsPrimFactors[0][0] * factor;
    gFactor = sHeartsPrimFactors[0][1] * factor;
    bFactor = sHeartsPrimFactors[0][2] * factor;

    interfaceCtx->beatingHeartPrim[0] = (u8)(rFactor + mainColor.r) & 0xFF;
    interfaceCtx->beatingHeartPrim[1] = (u8)(gFactor + mainColor.g) & 0xFF;
    interfaceCtx->beatingHeartPrim[2] = (u8)(bFactor + mainColor.b) & 0xFF;

    rFactor = sHeartsEnvFactors[0][0] * factor;
    gFactor = sHeartsEnvFactors[0][1] * factor;
    bFactor = sHeartsEnvFactors[0][2] * factor;

    if (1) {}
    ddType = type;

    interfaceCtx->beatingHeartEnv[0] = (u8)(rFactor + mainBorder.r) & 0xFF;
    interfaceCtx->beatingHeartEnv[1] = (u8)(gFactor + mainBorder.g) & 0xFF;
    interfaceCtx->beatingHeartEnv[2] = (u8)(bFactor + mainBorder.b) & 0xFF;

    sHeartsDDPrim[0][0] = ddBorder.r;
    sHeartsDDPrim[0][1] = ddBorder.g;
    sHeartsDDPrim[0][2] = ddBorder.b;

    sHeartsDDEnv[0][0] = ddColor.r;
    sHeartsDDEnv[0][1] = ddColor.g;
    sHeartsDDEnv[0][2] = ddColor.b;

    sHeartsDDPrim[1][0] = ddBorder.r;
    sHeartsDDPrim[1][1] = ddBorder.g;
    sHeartsDDPrim[1][2] = ddBorder.b;

    sHeartsDDEnv[1][0] = ddColor.r;
    sHeartsDDEnv[1][1] = ddColor.g;
    sHeartsDDEnv[1][2] = ddColor.b;

    rFactor = sHeartsDDPrimFactors[ddType][0] * ddFactor;
    gFactor = sHeartsDDPrimFactors[ddType][1] * ddFactor;
    bFactor = sHeartsDDPrimFactors[ddType][2] * ddFactor;

    sBeatingHeartsDDPrim[0] = (u8)(rFactor + ddBorder.r) & 0xFF;
    sBeatingHeartsDDPrim[1] = (u8)(gFactor + ddBorder.g) & 0xFF;
    sBeatingHeartsDDPrim[2] = (u8)(bFactor + ddBorder.b) & 0xFF;

    rFactor = sHeartsDDEnvFactors[ddType][0] * ddFactor;
    gFactor = sHeartsDDEnvFactors[ddType][1] * ddFactor;
    bFactor = sHeartsDDEnvFactors[ddType][2] * ddFactor;

    sBeatingHeartsDDEnv[0] = (u8)(rFactor + ddColor.r) & 0xFF;
    sBeatingHeartsDDEnv[1] = (u8)(gFactor + ddColor.g) & 0xFF;
    sBeatingHeartsDDEnv[2] = (u8)(bFactor + ddColor.b) & 0xFF;
}

s32 func_80078E18(PlayState* play) {
    gSaveContext.health = play->interfaceCtx.unk_226;
    return 1;
}

s32 func_80078E34(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    interfaceCtx->unk_228 = 0x140;
    interfaceCtx->unk_226 += 0x10;

    if (interfaceCtx->unk_226 >= gSaveContext.health) {
        interfaceCtx->unk_226 = gSaveContext.health;
        return 1;
    }

    return 0;
}

s32 func_80078E84(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    if (interfaceCtx->unk_228 != 0) {
        interfaceCtx->unk_228--;
    } else {
        interfaceCtx->unk_228 = 0x140;
        interfaceCtx->unk_226 -= 0x10;
        if (interfaceCtx->unk_226 <= 0) {
            interfaceCtx->unk_226 = 0;
            play->damagePlayer(play, GET_PLAYER(play), -(gSaveContext.health + 1));
            return 1;
        }
    }
    return 0;
}

static void* sHeartTextures[] = {
    gHeartFullTex,         gHeartQuarterTex,      gHeartQuarterTex,      gHeartQuarterTex,
    gHeartQuarterTex,      gHeartQuarterTex,      gHeartHalfTex,         gHeartHalfTex,
    gHeartHalfTex,         gHeartHalfTex,         gHeartHalfTex,         gHeartThreeQuarterTex,
    gHeartThreeQuarterTex, gHeartThreeQuarterTex, gHeartThreeQuarterTex, gHeartThreeQuarterTex,
};

static void* sHeartDDTextures[] = {
    gDefenseHeartFullTex,         gDefenseHeartQuarterTex,      gDefenseHeartQuarterTex,
    gDefenseHeartQuarterTex,      gDefenseHeartQuarterTex,      gDefenseHeartQuarterTex,
    gDefenseHeartHalfTex,         gDefenseHeartHalfTex,         gDefenseHeartHalfTex,
    gDefenseHeartHalfTex,         gDefenseHeartHalfTex,         gDefenseHeartThreeQuarterTex,
    gDefenseHeartThreeQuarterTex, gDefenseHeartThreeQuarterTex, gDefenseHeartThreeQuarterTex,
    gDefenseHeartThreeQuarterTex,
};

static s16 HealthMeter_GetLocalMultiplayerPlayerCount(void) {
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

static s32 HealthMeter_IsSplitScreenEnabled(void) {
    return !CVarGetInteger(LOCAL_MP_DISABLED_CVAR, 0) && CVarGetInteger(LOCAL_MP_SPLITSCREEN_CVAR, 1);
}

static void HealthMeter_GetSplitViewportTopLeft(s16 playerCount, s16 playerIndex, s16* leftX, s16* topY) {
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

static void HealthMeter_GetHealthFieldsForPort(u8 controllerPort, s16** health, s16** healthCapacity) {
    *health = &gSaveContext.health;
    *healthCapacity = &gSaveContext.healthCapacity;

    if (controllerPort == 4) {
        *health = &gSaveContext.health4;
        *healthCapacity = &gSaveContext.healthCapacity4;
    } else if (controllerPort == 3) {
        *health = &gSaveContext.health3;
        *healthCapacity = &gSaveContext.healthCapacity3;
    } else if (controllerPort == 2) {
        *health = &gSaveContext.health2;
        *healthCapacity = &gSaveContext.healthCapacity2;
    }

    if ((controllerPort >= 2) && (**healthCapacity < STARTING_HEALTH)) {
        **healthCapacity = gSaveContext.healthCapacity;
        if (**healthCapacity < STARTING_HEALTH) {
            **healthCapacity = STARTING_HEALTH;
        }

        if (**health <= 0) {
            **health = **healthCapacity;
        }
    }
}

static void HealthMeter_GetHealthFieldsForPlayer(Player* player, s16** health, s16** healthCapacity) {
    u8 controllerPort = 1;

    if ((player != NULL) && player->isSecondPlayer && (player->controllerPort >= 2) && (player->controllerPort <= 4)) {
        controllerPort = player->controllerPort;
    }

    HealthMeter_GetHealthFieldsForPort(controllerPort, health, healthCapacity);
}

static void Health_DrawAdditionalMeter(PlayState* play, s16 health, s16 healthCapacity, f32 startX, f32 anchorY,
                                       s32 growRight, s32 alignBottom) {
    void* heartBgImg;
    u32 curColorSet;
    f32 offsetX;
    f32 offsetY;
    s32 heartIndex;
    f32 heartCenterX;
    f32 heartCenterY;
    f32 heartScale;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Vtx* beatingHeartVtx = interfaceCtx->beatingHeartVtx;
    s32 curHeartFraction = health % FULL_HEART_HEALTH;
    s16 totalHeartCount = healthCapacity / FULL_HEART_HEALTH;
    s16 fullHeartCount = health / FULL_HEART_HEALTH;
    s16 totalRows = (totalHeartCount + 9) / 10;
    f32 heartsScale = 0.68f;
    f32 beatingHeartPulsingSize = interfaceCtx->unk_22A * 0.1f;
    s32 curCombineModeSet = 0;
    u8* curBgImgLoaded = NULL;

    if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) != ORIGINAL_LOCATION) {
        heartsScale = CVarGetFloat(CVAR_COSMETIC("HUD.HeartsCount.Scale"), 0.7f);
    }

    if (totalHeartCount <= 0) {
        return;
    }

    OPEN_DISPS(gfxCtx);

    if (!(health % FULL_HEART_HEALTH)) {
        fullHeartCount--;
    }

    if (!alignBottom) {
        anchorY += 8.0f * heartsScale;
    }

    curColorSet = -1;
    for (heartIndex = 0; heartIndex < totalHeartCount; heartIndex++) {
        s32 row = heartIndex / 10;
        s32 col = heartIndex % 10;

        if (heartIndex < fullHeartCount) {
            if (curColorSet != 0) {
                curColorSet = 0;
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, interfaceCtx->heartsPrimR[0], interfaceCtx->heartsPrimG[0],
                                interfaceCtx->heartsPrimB[0], interfaceCtx->healthAlpha);
                gDPSetEnvColor(OVERLAY_DISP++, interfaceCtx->heartsEnvR[0], interfaceCtx->heartsEnvG[0],
                               interfaceCtx->heartsEnvB[0], 255);
            }
        } else if (heartIndex == fullHeartCount) {
            if (curColorSet != 1) {
                curColorSet = 1;
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, interfaceCtx->beatingHeartPrim[0],
                                interfaceCtx->beatingHeartPrim[1], interfaceCtx->beatingHeartPrim[2],
                                interfaceCtx->healthAlpha);
                gDPSetEnvColor(OVERLAY_DISP++, interfaceCtx->beatingHeartEnv[0], interfaceCtx->beatingHeartEnv[1],
                               interfaceCtx->beatingHeartEnv[2], 255);
            }
        } else {
            if (curColorSet != 2) {
                curColorSet = 2;
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, interfaceCtx->heartsPrimR[0], interfaceCtx->heartsPrimG[0],
                                interfaceCtx->heartsPrimB[0], interfaceCtx->healthAlpha);
                gDPSetEnvColor(OVERLAY_DISP++, interfaceCtx->heartsEnvR[0], interfaceCtx->heartsEnvG[0],
                               interfaceCtx->heartsEnvB[0], 255);
            }
        }

        if (heartIndex < fullHeartCount) {
            heartBgImg = gHeartFullTex;
        } else if (heartIndex == fullHeartCount) {
            heartBgImg = sHeartTextures[curHeartFraction];
        } else {
            heartBgImg = gHeartEmptyTex;
        }

        if (curBgImgLoaded != heartBgImg) {
            curBgImgLoaded = heartBgImg;
            gDPLoadTextureBlock(OVERLAY_DISP++, heartBgImg, G_IM_FMT_IA, G_IM_SIZ_8b, 16, 16, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
        }

        offsetX = growRight ? (10.0f * col) : (-10.0f * col);
        offsetY = alignBottom ? (-10.0f * (totalRows - row - 1)) : (10.0f * row);
        heartCenterY = anchorY + offsetY;
        heartCenterX = startX + offsetX;

        if (heartIndex != fullHeartCount) {
            if (curCombineModeSet != 1) {
                curCombineModeSet = 1;
                Gfx_SetupDL_39Overlay(gfxCtx);
                gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE,
                                  0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
            }
            heartScale = heartsScale;
        } else {
            if (curCombineModeSet != 2) {
                curCombineModeSet = 2;
                Gfx_SetupDL_42Overlay(gfxCtx);
                gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE,
                                  0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
            }

            if (CVarGetInteger(CVAR_ENHANCEMENT("NoHUDHeartAnimation"), 0)) {
                heartScale = heartsScale;
            } else {
                heartScale = heartsScale + (heartsScale / 3) - ((heartsScale / 3) * beatingHeartPulsingSize);
            }
        }

        {
            Mtx* matrix = Graph_Alloc(gfxCtx, sizeof(Mtx));

            Matrix_SetTranslateScaleMtx2(matrix, heartScale, heartScale, heartScale, -130.0f + heartCenterX,
                                         115.0f - heartCenterY, 0.0f);
            gSPMatrix(OVERLAY_DISP++, matrix, G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPVertex(OVERLAY_DISP++, beatingHeartVtx, 4, 0);
            gSP1Quadrangle(OVERLAY_DISP++, 0, 2, 3, 1, 0);
        }
    }

    CLOSE_DISPS(gfxCtx);
}

s16 getHealthMeterXOffset() {
    s16 X_Margins;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.Hearts.UseMargins"), 0) != 0)
        X_Margins = Left_LM_Margin;
    else
        X_Margins = 0;

    if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) != ORIGINAL_LOCATION) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) == ANCHOR_LEFT) {
            return OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosX"), 0) + X_Margins +
                                               70.0f);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) == ANCHOR_RIGHT) {
            X_Margins = Right_LM_Margin;
            return OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosX"), 0) + X_Margins +
                                                70.0f);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) == ANCHOR_NONE) {
            return CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosX"), 0) + 70.0f;
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) == HIDDEN) {
            return -9999;
        }
    } else {
        return OTRGetDimensionFromLeftEdge(0.0f) + X_Margins;
    }
}

s16 getHealthMeterYOffset() {
    s16 Y_Margins;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.Hearts.UseMargins"), 0) != 0)
        Y_Margins = (Top_LM_Margin * -1);
    else
        Y_Margins = 0;

    f32 HeartsScale = 0.7f;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) != ORIGINAL_LOCATION) {
        HeartsScale = CVarGetFloat(CVAR_COSMETIC("HUD.HeartsCount.Scale"), 0.7f);
        return CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosY"), 0) + Y_Margins + (HeartsScale * 15);
    } else {
        return 0.0f + Y_Margins;
    }
}

void HealthMeter_Draw(PlayState* play) {
    s32 pad[5];
    void* heartBgImg;
    u32 curColorSet;
    f32 PosX_anchor;
    f32 offsetX;
    f32 offsetY;
    s32 i;
    f32 temp1;
    f32 temp2;
    f32 temp3;
    f32 temp4;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Vtx* sp154 = interfaceCtx->beatingHeartVtx;
    s32 curHeartFraction = gSaveContext.health % FULL_HEART_HEALTH;
    s16 totalHeartCount = gSaveContext.healthCapacity / FULL_HEART_HEALTH;
    s16 fullHeartCount = gSaveContext.health / FULL_HEART_HEALTH;
    s32 pad2;
    f32 sp144 = interfaceCtx->unk_22A * 0.1f;
    s32 curCombineModeSet = 0;
    u8* curBgImgLoaded = NULL;
    s32 ddHeartCountMinusOne = gSaveContext.isDoubleDefenseAcquired ? totalHeartCount - 1 : -1;
    f32 HeartsScale = 0.7f;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) != ORIGINAL_LOCATION) {
        HeartsScale = CVarGetFloat(CVAR_COSMETIC("HUD.HeartsCount.Scale"), 0.7f);
    }
    static u32 epoch = 0;
    epoch++;

    OPEN_DISPS(gfxCtx);

    if (!(gSaveContext.health % FULL_HEART_HEALTH)) {
        fullHeartCount--;
    }

    curColorSet = -1;
    /*
        s16 X_Margins;
        s16 Y_Margins;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.Hearts.UseMargins"), 0) != 0) {
            X_Margins = Left_LM_Margin;
            Y_Margins = (Top_LM_Margin*-1);
        } else {
            X_Margins = 0;
            Y_Margins = 0;
        }
        s16 PosX_original = OTRGetDimensionFromLeftEdge(0.0f)+X_Margins;
        s16 PosY_original = 0.0f+Y_Margins;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) != ORIGINAL_LOCATION) {
            offsetY = CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosY"), 0)+Y_Margins+(HeartsScale*15);
            if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) == ANCHOR_LEFT) {
                offsetX = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosX"),
       0)+X_Margins+70.0f); } else if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) == ANCHOR_RIGHT) {
                X_Margins = Right_LM_Margin;
                offsetX = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosX"),
       0)+X_Margins+70.0f); } else if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) == ANCHOR_NONE) {
                offsetX = CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosX"), 0)+70.0f;
            } else if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) == HIDDEN) {
                offsetX = -9999;
            }
        } else {
            offsetY = PosY_original;
            offsetX = PosX_original;
        }
    */
    offsetX = PosX_anchor = getHealthMeterXOffset();
    offsetY = getHealthMeterYOffset();

    for (i = 0; i < totalHeartCount; i++) {
        FrameInterpolation_RecordOpenChild("HealthMeter Heart", i);

        if ((ddHeartCountMinusOne < 0) || (i > ddHeartCountMinusOne)) {
            if (i < fullHeartCount) {
                if (curColorSet != 0) {
                    curColorSet = 0;
                    gDPPipeSync(OVERLAY_DISP++);
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, interfaceCtx->heartsPrimR[0], interfaceCtx->heartsPrimG[0],
                                    interfaceCtx->heartsPrimB[0], interfaceCtx->healthAlpha);
                    gDPSetEnvColor(OVERLAY_DISP++, interfaceCtx->heartsEnvR[0], interfaceCtx->heartsEnvG[0],
                                   interfaceCtx->heartsEnvB[0], 255);
                }
            } else if (i == fullHeartCount) {
                if (curColorSet != 1) {
                    curColorSet = 1;
                    gDPPipeSync(OVERLAY_DISP++);
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, interfaceCtx->beatingHeartPrim[0],
                                    interfaceCtx->beatingHeartPrim[1], interfaceCtx->beatingHeartPrim[2],
                                    interfaceCtx->healthAlpha);
                    gDPSetEnvColor(OVERLAY_DISP++, interfaceCtx->beatingHeartEnv[0], interfaceCtx->beatingHeartEnv[1],
                                   interfaceCtx->beatingHeartEnv[2], 255);
                }
            } else if (i > fullHeartCount) {
                if (curColorSet != 2) {
                    curColorSet = 2;
                    gDPPipeSync(OVERLAY_DISP++);
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, interfaceCtx->heartsPrimR[0], interfaceCtx->heartsPrimG[0],
                                    interfaceCtx->heartsPrimB[0], interfaceCtx->healthAlpha);
                    gDPSetEnvColor(OVERLAY_DISP++, interfaceCtx->heartsEnvR[0], interfaceCtx->heartsEnvG[0],
                                   interfaceCtx->heartsEnvB[0], 255);
                }
            } else {
                if (curColorSet != 3) {
                    curColorSet = 3;
                    gDPPipeSync(OVERLAY_DISP++);
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, interfaceCtx->heartsPrimR[1], interfaceCtx->heartsPrimG[1],
                                    interfaceCtx->heartsPrimB[1], interfaceCtx->healthAlpha);
                    gDPSetEnvColor(OVERLAY_DISP++, interfaceCtx->heartsEnvR[1], interfaceCtx->heartsEnvG[1],
                                   interfaceCtx->heartsEnvB[1], 255);
                }
            }

            if (i < fullHeartCount) {
                heartBgImg = gHeartFullTex;
            } else if (i == fullHeartCount) {
                heartBgImg = sHeartTextures[curHeartFraction];
            } else {
                heartBgImg = gHeartEmptyTex;
            }
        } else {
            if (i < fullHeartCount) {
                if (curColorSet != 4) {
                    curColorSet = 4;
                    gDPPipeSync(OVERLAY_DISP++);
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, sHeartsDDPrim[0][0], sHeartsDDPrim[0][1], sHeartsDDPrim[0][2],
                                    interfaceCtx->healthAlpha);
                    gDPSetEnvColor(OVERLAY_DISP++, sHeartsDDEnv[0][0], sHeartsDDEnv[0][1], sHeartsDDEnv[0][2], 255);
                }
            } else if (i == fullHeartCount) {
                if (curColorSet != 5) {
                    curColorSet = 5;
                    gDPPipeSync(OVERLAY_DISP++);
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, sBeatingHeartsDDPrim[0], sBeatingHeartsDDPrim[1],
                                    sBeatingHeartsDDPrim[2], interfaceCtx->healthAlpha);
                    gDPSetEnvColor(OVERLAY_DISP++, sBeatingHeartsDDEnv[0], sBeatingHeartsDDEnv[1],
                                   sBeatingHeartsDDEnv[2], 255);
                }
            } else if (i > fullHeartCount) {
                if (curColorSet != 6) {
                    curColorSet = 6;
                    gDPPipeSync(OVERLAY_DISP++);
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, sHeartsDDPrim[0][0], sHeartsDDPrim[0][1], sHeartsDDPrim[0][2],
                                    interfaceCtx->healthAlpha);
                    gDPSetEnvColor(OVERLAY_DISP++, sHeartsDDEnv[0][0], sHeartsDDEnv[0][1], sHeartsDDEnv[0][2], 255);
                }
            } else {
                if (curColorSet != 7) {
                    curColorSet = 7;
                    gDPPipeSync(OVERLAY_DISP++);
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, sHeartsDDPrim[1][0], sHeartsDDPrim[1][1], sHeartsDDPrim[1][2],
                                    interfaceCtx->healthAlpha);
                    gDPSetEnvColor(OVERLAY_DISP++, sHeartsDDEnv[1][0], sHeartsDDEnv[1][1], sHeartsDDEnv[1][2], 255);
                }
            }

            if (i < fullHeartCount) {
                heartBgImg = gDefenseHeartFullTex;
            } else if (i == fullHeartCount) {
                heartBgImg = sHeartDDTextures[curHeartFraction];
            } else {
                heartBgImg = gDefenseHeartEmptyTex;
            }
        }

        if (curBgImgLoaded != heartBgImg) {
            curBgImgLoaded = heartBgImg;
            gDPLoadTextureBlock(OVERLAY_DISP++, heartBgImg, G_IM_FMT_IA, G_IM_SIZ_8b, 16, 16, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
        }

        if (i != fullHeartCount) {
            if ((ddHeartCountMinusOne < 0) || (i > ddHeartCountMinusOne)) {
                if (curCombineModeSet != 1) {
                    curCombineModeSet = 1;
                    Gfx_SetupDL_39Overlay(gfxCtx);
                    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE,
                                      0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
                }
            } else {
                if (curCombineModeSet != 3) {
                    curCombineModeSet = 3;
                    Gfx_SetupDL_39Overlay(gfxCtx);
                    gDPSetCombineLERP(OVERLAY_DISP++, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE, TEXEL0, 0, PRIMITIVE,
                                      0, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
                }
            }

            temp3 = offsetY;
            temp2 = offsetX;
            temp4 = 1.0f;   // Heart texture size
            temp4 /= 0.68f; // Hearts Scaled size
            temp4 *= 1 << 10;
            temp1 = 8.0f;
            temp1 *= 0.68f;
            /*gSPWideTextureRectangle(OVERLAY_DISP++, (s32)((temp2 - temp1) * 4), (s32)((temp3 - temp1) * 4),
                                (s32)((temp2 + temp1) * 4), (s32)((temp3 + temp1) * 4), G_TX_RENDERTILE, 0, 0,
                                (s32)temp4, (s32)temp4);*/
            Mtx* matrix = Graph_Alloc(gfxCtx, sizeof(Mtx));
            Matrix_SetTranslateScaleMtx2(matrix,
                                         HeartsScale,          // Scale X
                                         HeartsScale,          // Scale Y
                                         HeartsScale,          // Scale Z
                                         -130 + offsetX,       // Pos X
                                         (-94 + offsetY) * -1, // Pos Y
                                         0.0f);                // Pos Z
            gSPMatrix(OVERLAY_DISP++, matrix, G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPVertex(OVERLAY_DISP++, sp154, 4, 0);
            gSP1Quadrangle(OVERLAY_DISP++, 0, 2, 3, 1, 0);
        } else {
            if ((ddHeartCountMinusOne < 0) || (i > ddHeartCountMinusOne)) {
                if (curCombineModeSet != 2) {
                    curCombineModeSet = 2;
                    Gfx_SetupDL_42Overlay(gfxCtx);
                    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE,
                                      0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
                }
            } else {
                if (curCombineModeSet != 4) {
                    curCombineModeSet = 4;
                    Gfx_SetupDL_42Overlay(gfxCtx);
                    gDPSetCombineLERP(OVERLAY_DISP++, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE, TEXEL0, 0, PRIMITIVE,
                                      0, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
                }
            }

            {
                Mtx* matrix = Graph_Alloc(gfxCtx, sizeof(Mtx));

                if (CVarGetInteger(CVAR_ENHANCEMENT("NoHUDHeartAnimation"), 0)) {
                    Matrix_SetTranslateScaleMtx2(matrix,
                                                 HeartsScale,          // Scale X
                                                 HeartsScale,          // Scale Y
                                                 HeartsScale,          // Scale Z
                                                 -130 + offsetX,       // Pos X
                                                 (-94 + offsetY) * -1, // Pos Y
                                                 0.0f);
                } else {
                    Matrix_SetTranslateScaleMtx2(matrix, HeartsScale + (HeartsScale / 3) - ((HeartsScale / 3) * sp144),
                                                 HeartsScale + (HeartsScale / 3) - ((HeartsScale / 3) * sp144),
                                                 HeartsScale + (HeartsScale / 3) - ((HeartsScale / 3) * sp144),
                                                 -130 + offsetX,       // Pos X
                                                 (-94 + offsetY) * -1, // Pos Y
                                                 0.0f);
                }

                gSPMatrix(OVERLAY_DISP++, matrix, G_MTX_MODELVIEW | G_MTX_LOAD);
                gSPVertex(OVERLAY_DISP++, sp154, 4, 0);
                gSP1Quadrangle(OVERLAY_DISP++, 0, 2, 3, 1, 0);
            }
        }

        offsetX += 10.0f;
        s32 lineLength = CVarGetInteger(CVAR_COSMETIC("HUD.Hearts.LineLength"), 10);
        if (lineLength != 0 && (i + 1) % lineLength == 0) {
            offsetX = PosX_anchor;
            offsetY += 10.0f;
        }

        FrameInterpolation_RecordCloseChild();
    }

    {
        s16 playerCount = HealthMeter_GetLocalMultiplayerPlayerCount();
        s16* secondaryHealth;
        s16* secondaryHealthCapacity;
        f32 heartsScale = 0.68f;
        f32 p1TopAnchorY;
        f32 p1TopCenterY;
        f32 p1StartX;
        f32 mirroredStartX;
        f32 bottomAnchorY;
        f32 splitViewportScaleX;
        s32 splitScreenEnabled = HealthMeter_IsSplitScreenEnabled() && (playerCount > 1);

        if (CVarGetInteger(CVAR_COSMETIC("HUD.HeartsCount.PosType"), 0) != ORIGINAL_LOCATION) {
            heartsScale = CVarGetFloat(CVAR_COSMETIC("HUD.HeartsCount.Scale"), 0.7f);
        }

        // Keep secondary heart rows mirrored against player 1's heart baseline and scale.
        p1TopAnchorY = getHealthMeterYOffset() + 21.0f - (8.0f * heartsScale);
        p1TopCenterY = p1TopAnchorY + (8.0f * heartsScale);
        p1StartX = getHealthMeterXOffset();
        // Health meters are translated by (-130 + startX), so mirror in this space uses SCREEN_WIDTH - 60 - x.
        mirroredStartX = SCREEN_WIDTH - 60.0f - p1StartX;
        bottomAnchorY = SCREEN_HEIGHT - p1TopCenterY;
        splitViewportScaleX =
            (OTRGetDimensionFromRightEdge(SCREEN_WIDTH) - OTRGetDimensionFromLeftEdge(0.0f)) / SCREEN_WIDTH;

        if (splitScreenEnabled) {
            s16 playerIndex;

            for (playerIndex = 1; playerIndex < playerCount; playerIndex++) {
                s16 viewportLeftX;
                s16 viewportTopY;
                f32 viewportLeftHudSpace;

                HealthMeter_GetSplitViewportTopLeft(playerCount, playerIndex, &viewportLeftX, &viewportTopY);
                viewportLeftHudSpace = viewportLeftX * splitViewportScaleX;
                HealthMeter_GetHealthFieldsForPort(playerIndex + 1, &secondaryHealth, &secondaryHealthCapacity);
                Health_DrawAdditionalMeter(play, *secondaryHealth, *secondaryHealthCapacity,
                                           p1StartX + viewportLeftHudSpace, p1TopAnchorY + viewportTopY, true, false);
            }
        } else {
            if (playerCount >= 2) {
                HealthMeter_GetHealthFieldsForPort(2, &secondaryHealth, &secondaryHealthCapacity);
                Health_DrawAdditionalMeter(play, *secondaryHealth, *secondaryHealthCapacity, mirroredStartX,
                                           p1TopAnchorY, false, false);
            }

            if (playerCount >= 3) {
                HealthMeter_GetHealthFieldsForPort(3, &secondaryHealth, &secondaryHealthCapacity);
                Health_DrawAdditionalMeter(play, *secondaryHealth, *secondaryHealthCapacity, p1StartX, bottomAnchorY,
                                           true, true);
            }

            if (playerCount >= 4) {
                HealthMeter_GetHealthFieldsForPort(4, &secondaryHealth, &secondaryHealthCapacity);
                Health_DrawAdditionalMeter(play, *secondaryHealth, *secondaryHealthCapacity, mirroredStartX,
                                           bottomAnchorY, false, true);
            }
        }
    }

    CLOSE_DISPS(gfxCtx);
}

void HealthMeter_HandleCriticalAlarm(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    if (interfaceCtx->unk_22C != 0) {
        interfaceCtx->unk_22A--;
        if (interfaceCtx->unk_22A <= 0) {
            interfaceCtx->unk_22A = 0;
            interfaceCtx->unk_22C = 0;
            if (CVarGetInteger(CVAR_AUDIO("LowHpAlarm"), 0) == 0 && !Player_InCsMode(play) &&
                (play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) &&
                HealthMeter_IsCritical(GET_PLAYER(play)) &&
                !Play_InCsMode(play)) {
                Sfx_PlaySfxCentered(NA_SE_SY_HITPOINT_ALARM);
            }
        }
    } else {
        interfaceCtx->unk_22A++;
        if (interfaceCtx->unk_22A >= 10) {
            interfaceCtx->unk_22A = 10;
            interfaceCtx->unk_22C = 1;
        }
    }
}

u32 HealthMeter_IsCritical(Player* player) {
    s16* health;
    s16* healthCapacity;
    s32 var;

    HealthMeter_GetHealthFieldsForPlayer(player, &health, &healthCapacity);

    if (*healthCapacity <= 0x50) {
        var = 0x10;
    } else if (*healthCapacity <= 0xA0) {
        var = 0x18;
    } else if (*healthCapacity <= 0xF0) {
        var = 0x20;
    } else {
        var = 0x2C;
    }

    if (GameInteractor_Should(VB_HEALTH_METER_BE_CRITICAL, var >= *health && *health > 0)) {
        return true;
    } else {
        return false;
    }
}
