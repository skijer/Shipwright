// Postman's Hat and Great Fairy's Mask travel: the pause world map redrawn as a warp menu over a stopped
// world, the same screen the Minish Cap opens by a pod soil.
#include "textures/icon_item_static/icon_item_static.h"
#include "textures/icon_item_nes_static/icon_item_nes_static.h"
#include "textures/icon_item_ger_static/icon_item_ger_static.h"
#include "textures/icon_item_fra_static/icon_item_fra_static.h"
#include "textures/icon_item_jpn_static/icon_item_jpn_static.h"
#include "textures/icon_item_field_static/icon_item_field_static.h"
#include "textures/map_name_static/map_name_static.h"
extern void func_80853080(Player* player, PlayState* play);

#define TRAVEL_OWNER "mm_wearables.travel"
#define TRAVEL_TIME_PRIORITY 5000
#define TRAVEL_LANGUAGE_COUNT 4
#define MAILBOX_UNLOCK_RADIUS 150.0f
#define MAILBOX_TALK_RADIUS 80.0f
#define NO_ITEMGETINF (-1)

typedef enum {
    TRAVEL_NONE,
    TRAVEL_MAILBOX,
    TRAVEL_FOUNTAIN,
} TravelMode;

// Where a destination sits on the pause world map, and the area name the pause screen prints for it.
typedef struct {
    int16_t x;
    int16_t y;
    const char* nameTex[TRAVEL_LANGUAGE_COUNT];
} MapMarker;

#define POSITION_NAMES(area)                                                                                   \
    { g##area##PositionNameENGTex, g##area##PositionNameGERTex, g##area##PositionNameFRATex,                   \
      g##area##PositionNameJPNTex }

typedef struct {
    int16_t scene;
    int16_t alternate;
    int16_t entrance;
    int16_t yaw;
    Vec3f pos;
    MapMarker marker;
    const char* flag;
} MailPoint;

static const MailPoint sMailPoints[] = {
    { SCENE_KOKIRI_FOREST, -1, ENTR_KOKIRI_FOREST_0, 16774, { -1441.042f, -76.593f, -175.555f },
      { 73, -12, POSITION_NAMES(KokiriForest) }, "skijer.mailbox.0" },
    { SCENE_MARKET_DAY, SCENE_MARKET_NIGHT, ENTR_MARKET_SOUTH_EXIT, 0, { -4.156f, 0.0f, 124.724f },
      { 14, 4, POSITION_NAMES(Market) }, "skijer.mailbox.1" },
    { SCENE_KAKARIKO_VILLAGE, -1, ENTR_KAKARIKO_VILLAGE_FRONT_GATE, -21923, { -2162.320f, 138.0f, 1158.205f },
      { 38, 15, POSITION_NAMES(KakarikoVillage) }, "skijer.mailbox.2" },
    { SCENE_LON_LON_RANCH, -1, ENTR_LON_LON_RANCH_ENTRANCE, -16291, { 986.0f, 0.0f, -3376.015f },
      { 10, -15, POSITION_NAMES(LonLonRanch) }, "skijer.mailbox.3" },
    { SCENE_DEATH_MOUNTAIN_TRAIL, -1, ENTR_DEATH_MOUNTAIN_TRAIL_BOTTOM_EXIT, 8402, { -540.559f, 1194.025f, -1858.751f },
      { 35, 44, POSITION_NAMES(DeathMountainTrail) }, "skijer.mailbox.4" },
    { SCENE_ZORAS_RIVER, -1, ENTR_ZORAS_RIVER_WEST_EXIT, -203, { 4095.814f, 960.0f, -1684.251f },
      { 78, 18, POSITION_NAMES(ZorasRiver) }, "skijer.mailbox.5" },
    { SCENE_GERUDO_VALLEY, -1, ENTR_GERUDO_VALLEY_EAST_EXIT, 16342, { 427.205f, 36.0f, 7.694f },
      { -51, 10, POSITION_NAMES(GerudoValley) }, "skijer.mailbox.6" },
};

// A fountain opens once its reward is claimed; Death Mountain Trail's reward is the magic meter itself.
typedef struct {
    int16_t entrance;
    int16_t vanillaFlag;
    RandomizerInf randoFlag;
    Vec3f pedestal;
    MapMarker marker;
} FountainPoint;

static const FountainPoint sFountainPoints[] = {
    { 0x315, NO_ITEMGETINF, RAND_INF_DMT_GREAT_FAIRY_REWARD, { -22.0f, 10.0f, -798.0f },
      { 32, 42, POSITION_NAMES(DeathMountainTrail) } },
    { 0x4BE, ITEMGETINF_30, RAND_INF_DMC_GREAT_FAIRY_REWARD, { -22.0f, 10.0f, -798.0f },
      { 42, 52, POSITION_NAMES(DeathMountainCrater) } },
    { 0x4C2, ITEMGETINF_38, RAND_INF_OGC_GREAT_FAIRY_REWARD, { -22.0f, 10.0f, -798.0f },
      { 12, 24, POSITION_NAMES(GanonsCastle) } },
    { 0x371, ITEMGETINF_19, RAND_INF_ZF_GREAT_FAIRY_REWARD, { -21.0f, 10.0f, -802.0f },
      { 82, 26, POSITION_NAMES(ZorasFountain) } },
    { 0x578, ITEMGETINF_18, RAND_INF_HC_GREAT_FAIRY_REWARD, { -21.0f, 10.0f, -802.0f },
      { -2, 14, POSITION_NAMES(HyruleCastle) } },
    { 0x588, ITEMGETINF_1A, RAND_INF_COLOSSUS_GREAT_FAIRY_REWARD, { -21.0f, 10.0f, -802.0f },
      { -90, 28, POSITION_NAMES(DesertColossus) } },
};

static const ALIGN_ASSET(2) char sLetterIconTex[] = MM_ICON_DIR "gItemIconLetterToKafeiTex";
static const ALIGN_ASSET(2) char sStrayFairyHeadTex[] = "__OTR__objects/gameplay_keep/gStrayFairyRightFacingHeadTex";
static const ALIGN_ASSET(2) char sStrayFairyGlowTex[] = "__OTR__objects/gameplay_keep/gStrayFairyGlowTex";

static int16_t sMailboxActorId = -1;
static uint8_t sMailboxSpawned;
static TravelMode sTravelMode;
static int8_t sTravelCursor;
static bool sHasSelectorArt;

// ---- destinations ----

static int32_t CountDestinations(TravelMode mode) {
    return mode == TRAVEL_MAILBOX ? ARRAY_COUNT(sMailPoints) : ARRAY_COUNT(sFountainPoints);
}

static const MapMarker* GetMarker(TravelMode mode, int32_t index) {
    return mode == TRAVEL_MAILBOX ? &sMailPoints[index].marker : &sFountainPoints[index].marker;
}

static bool IsFountainUnlocked(int32_t index) {
    const FountainPoint* fountain = &sFountainPoints[index];

    if (IS_RANDO) {
        return Flags_GetRandomizerInf(fountain->randoFlag);
    }
    if (fountain->vanillaFlag == NO_ITEMGETINF) {
        return gSaveContext.isMagicAcquired;
    }
    return Flags_GetItemGetInf(fountain->vanillaFlag);
}

static bool IsDestinationUnlocked(TravelMode mode, int32_t index) {
    if (mode == TRAVEL_MAILBOX) {
        return sApi->GetRandoFlag(sMailPoints[index].flag);
    }
    return IsFountainUnlocked(index);
}

static int32_t FindFirstUnlocked(TravelMode mode) {
    for (int32_t i = 0; i < CountDestinations(mode); i++) {
        if (IsDestinationUnlocked(mode, i)) {
            return i;
        }
    }
    return -1;
}

// Farore's Wind's own landing: respawnFlag 3 makes the arriving scene read respawn[RESPAWN_MODE_TOP].
static void SetLanding(int16_t entrance, const Vec3f* pos, int16_t yaw) {
    gSaveContext.respawn[RESPAWN_MODE_TOP].entranceIndex = entrance;
    gSaveContext.respawn[RESPAWN_MODE_TOP].pos = *pos;
    gSaveContext.respawn[RESPAWN_MODE_TOP].yaw = yaw;
    gSaveContext.respawn[RESPAWN_MODE_TOP].playerParams = 0xDFF;
    gSaveContext.respawn[RESPAWN_MODE_TOP].roomIndex = 0;
    gSaveContext.respawnFlag = 3;
}

static void StartTravelTransition(PlayState* play, int16_t entrance, uint8_t transitionType,
                                  uint8_t nextTransitionType) {
    play->nextEntranceIndex = entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = transitionType;
    gSaveContext.nextTransitionType = nextTransitionType;
    gSaveContext.nextCutsceneIndex = 0xFFEF;
}

static void DepartToDestination(PlayState* play, TravelMode mode, int32_t index) {
    if (mode == TRAVEL_MAILBOX) {
        const MailPoint* mailbox = &sMailPoints[index];

        StartTravelTransition(play, mailbox->entrance, TRANS_TYPE_FADE_BLACK_FAST, TRANS_TYPE_FADE_BLACK_FAST);
        SetLanding(mailbox->entrance, &mailbox->pos, mailbox->yaw);
        return;
    }

    const FountainPoint* fountain = &sFountainPoints[index];

    StartTravelTransition(play, fountain->entrance, TRANS_TYPE_FADE_WHITE, TRANS_TYPE_FADE_WHITE_FAST);
    SetLanding(fountain->entrance, &fountain->pedestal, 0);
    Sfx_PlaySfxCentered(NA_SE_EV_GREAT_FAIRY_APPEAR);
}

// ---- travel map ----

static bool CanOpenTravelMap(PlayState* play, Player* player) {
    return !IsPlayerBusy(player) && play->pauseCtx.state == 0 && play->pauseCtx.debugState == 0 &&
           play->transitionTrigger == TRANS_TRIGGER_OFF && play->gameOverCtx.state == GAMEOVER_INACTIVE &&
           play->msgCtx.msgMode == MSGMODE_NONE && !Player_InCsMode(play);
}

// Drawing a texture no archive provides reads its own path string as pixels.
static bool HasSelectorArt(TravelMode mode) {
    if (mode == TRAVEL_MAILBOX) {
        return sApi->HasResource(sLetterIconTex);
    }
    return sApi->HasResource(sStrayFairyHeadTex) && sApi->HasResource(sStrayFairyGlowTex);
}

static void OpenTravelMap(PlayState* play, Player* player, TravelMode mode) {
    if (sTravelMode != TRAVEL_NONE || !CanOpenTravelMap(play, player)) {
        return;
    }

    int32_t first = FindFirstUnlocked(mode);

    if (first < 0) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return;
    }
    sTravelMode = mode;
    sTravelCursor = (int8_t)first;
    sHasSelectorArt = HasSelectorArt(mode);
    sApi->RequestTimeControl(TRAVEL_OWNER, TRAVEL_TIME_PRIORITY, 0.0f, true);
    sApi->BlockPlayerInput(TRAVEL_OWNER, 0xFFFF, true);
    player->linearVelocity = 0.0f;
    Sfx_PlaySfxCentered(NA_SE_SY_WIN_OPEN);
}

static void CloseTravelMap(void) {
    if (sTravelMode == TRAVEL_NONE) {
        return;
    }
    sTravelMode = TRAVEL_NONE;
    sApi->ReleasePlayerInput(TRAVEL_OWNER);
    sApi->ReleaseTimeControl(TRAVEL_OWNER);
}

// The destination whose direction on the map best matches where the stick points, nearest first.
static int8_t FindDestinationTowardStick(int8_t from, int8_t stickX, int8_t stickY) {
    const MapMarker* origin = GetMarker(sTravelMode, from);
    f32 stickAngle = atan2f((f32)stickX, (f32)stickY);
    int8_t best = -1;
    f32 bestScore = -1.0f;

    for (int32_t i = 0; i < CountDestinations(sTravelMode); i++) {
        const MapMarker* marker = GetMarker(sTravelMode, i);
        f32 deltaX = marker->x - origin->x;
        f32 deltaY = marker->y - origin->y;
        f32 distance = sqrtf(SQ(deltaX) + SQ(deltaY));

        if (i == from || distance < 1.0f) {
            continue;
        }

        f32 angleDifference = atan2f(deltaX, deltaY) - stickAngle;

        while (angleDifference > M_PI) {
            angleDifference -= 2.0f * M_PI;
        }
        while (angleDifference < -M_PI) {
            angleDifference += 2.0f * M_PI;
        }
        angleDifference = fabsf(angleDifference);
        if (angleDifference > M_PI / 2.0f) {
            continue;
        }

        f32 score = cosf(angleDifference) / distance;

        if (score > bestScore) {
            bestScore = score;
            best = (int8_t)i;
        }
    }
    return best;
}

static void MoveTravelCursor(Input* input) {
    static bool sWasStickHeld;
    f32 stickMagnitude = sqrtf(SQ((f32)input->rel.stick_x) + SQ((f32)input->rel.stick_y));

    if (stickMagnitude <= 30.0f) {
        sWasStickHeld = false;
        return;
    }
    if (sWasStickHeld) {
        return;
    }
    sWasStickHeld = true;

    int8_t next = FindDestinationTowardStick(sTravelCursor, input->rel.stick_x, input->rel.stick_y);

    if (next >= 0) {
        sTravelCursor = next;
        Sfx_PlaySfxCentered(NA_SE_SY_CURSOR);
    }
}

// Reads the raw controller: the input block only covers Link's copy of it.
static bool UpdateTravelMap(PlayState* play) {
    if (sTravelMode == TRAVEL_NONE) {
        return false;
    }

    Input* input = &play->state.input[0];

    if (play->transitionTrigger != TRANS_TRIGGER_OFF) {
        CloseTravelMap();
        return true;
    }
    MoveTravelCursor(input);

    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        TravelMode mode = sTravelMode;

        if (!IsDestinationUnlocked(mode, sTravelCursor)) {
            Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
            return true;
        }
        CloseTravelMap();
        Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
        DepartToDestination(play, mode, sTravelCursor);
        return true;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_B) || CHECK_BTN_ALL(input->press.button, BTN_START)) {
        CloseTravelMap();
        Sfx_PlaySfxCentered(NA_SE_SY_CANCEL);
    }
    return true;
}

// START would open the real pause screen on top of the map.
static void BlockPauseDuringTravelMap(bool* should, va_list args) {
    if (sTravelMode != TRAVEL_NONE) {
        *should = false;
    }
}

// ---- travel map drawing ----

// The map page of the pause screen in its own coordinates: the frame spans 240x160 around the origin. Screen
// is 320x240 in the 10.2 fixed point the RDP takes.
#define MAP_ORIGIN_Y 96
#define MAP_X(x) (640 + (s32)(x)*24 / 5)
#define MAP_Y(y) (480 - MAP_ORIGIN_Y - (s32)(y)*24 / 5)
#define MAP_BOX_HALF_WIDTH 12
#define MAP_BOX_HALF_HEIGHT 8

typedef struct {
    Color_RGB8 frameEdge;
    Color_RGB8 frameCenter;
    Color_RGB8 marker;
    Color_RGB8 name;
} TravelMapStyle;

static const TravelMapStyle sMailboxStyle = { { 110, 50, 45 }, { 140, 60, 60 }, { 100, 255, 255 }, { 150, 255, 255 } };
static const TravelMapStyle sFountainStyle = { { 80, 40, 100 }, { 110, 60, 130 }, { 150, 255, 200 }, { 200, 255, 220 } };

static const char* sFrameTexs[TRAVEL_LANGUAGE_COUNT][15] = {
    { gPauseMap00Tex, gPauseMap01Tex, gPauseMap02Tex, gPauseMap03Tex, gPauseMap04Tex, gPauseMap10ENGTex, gPauseMap11Tex,
      gPauseMap12Tex, gPauseMap13Tex, gPauseMap14Tex, gPauseMap20Tex, gPauseMap21Tex, gPauseMap22Tex, gPauseMap23Tex,
      gPauseMap24Tex },
    { gPauseMap00Tex, gPauseMap01Tex, gPauseMap02Tex, gPauseMap03Tex, gPauseMap04Tex, gPauseMap10GERTex, gPauseMap11Tex,
      gPauseMap12Tex, gPauseMap13Tex, gPauseMap14Tex, gPauseMap20Tex, gPauseMap21Tex, gPauseMap22Tex, gPauseMap23Tex,
      gPauseMap24Tex },
    { gPauseMap00Tex, gPauseMap01Tex, gPauseMap02Tex, gPauseMap03Tex, gPauseMap04Tex, gPauseMap10FRATex, gPauseMap11Tex,
      gPauseMap12Tex, gPauseMap13Tex, gPauseMap14Tex, gPauseMap20Tex, gPauseMap21Tex, gPauseMap22Tex, gPauseMap23Tex,
      gPauseMap24Tex },
    { gPauseMap00Tex, gPauseMap01Tex, gPauseMap02Tex, gPauseMap03Tex, gPauseMap04Tex, gPauseMap10JPNTex, gPauseMap11Tex,
      gPauseMap12Tex, gPauseMap13Tex, gPauseMap14Tex, gPauseMap20Tex, gPauseMap21Tex, gPauseMap22Tex, gPauseMap23Tex,
      gPauseMap24Tex },
};

static const char* sCurrentPositionTexs[TRAVEL_LANGUAGE_COUNT] = {
    gPauseCurrentPositionENGTex,
    gPauseCurrentPositionGERTex,
    gPauseCurrentPositionFRATex,
    gPauseCurrentPositionJPNTex,
};

// Clouds cover the areas the file has not discovered yet, each with the world map flag it hides behind.
static const char* sCloudTexs[] = {
    gWorldMapCloud16Tex, gWorldMapCloud15Tex, gWorldMapCloud14Tex, gWorldMapCloud13Tex,
    gWorldMapCloud12Tex, gWorldMapCloud11Tex, gWorldMapCloud10Tex, gWorldMapCloud9Tex,
    gWorldMapCloud8Tex,  gWorldMapCloud7Tex,  gWorldMapCloud6Tex,  gWorldMapCloud5Tex,
    gWorldMapCloud4Tex,  gWorldMapCloud3Tex,  gWorldMapCloud2Tex,  gWorldMapCloud1Tex,
};
static const u16 sCloudFlags[] = {
    0x05, 0x00, 0x13, 0x0E, 0x0F, 0x01, 0x02, 0x10, 0x12, 0x03, 0x07, 0x08, 0x09, 0x0C, 0x0B, 0x06,
};
static const s16 sCloudWidths[] = {
    32, 112, 32, 48, 32, 32, 32, 48, 32, 64, 32, 48, 48, 48, 48, 64,
};
static const s16 sCloudHeights[] = {
    24, 72, 13, 22, 19, 20, 19, 27, 14, 26, 22, 21, 49, 32, 45, 60,
};
static const s16 sCloudX[] = {
    0x002F, 0xFFCF, 0xFFEF, 0xFFF1, 0xFFF7, 0x0018, 0x002B, 0x000E,
    0x0009, 0x0026, 0x0052, 0x0047, 0xFFB4, 0xFFA9, 0xFF94, 0xFFCA,
};
static const s16 sCloudY[] = {
    0x000F, 0x0028, 0x000B, 0x002D, 0x0034, 0x0025, 0x0024, 0x0039,
    0x0036, 0x0021, 0x001F, 0x002D, 0x0020, 0x002A, 0x0031, 0xFFF6,
};

static const TravelMapStyle* GetTravelMapStyle(void) {
    return sTravelMode == TRAVEL_MAILBOX ? &sMailboxStyle : &sFountainStyle;
}

static s32 GetLanguage(void) {
    s32 language = gSaveContext.language;

    if (language < 0 || language >= TRAVEL_LANGUAGE_COUNT) {
        return 0;
    }
    return language;
}

static void SetupSolidRects(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetOtherMode(OVERLAY_DISP++,
                    G_AD_DISABLE | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PRIM | G_RM_CLD_SURF | G_RM_CLD_SURF2);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void SolidRect(PlayState* play, s32 x1, s32 y1, s32 x2, s32 y2, Color_RGBA8 color) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, color.r, color.g, color.b, color.a);
    gSPWideTextureRectangle(OVERLAY_DISP++, x1, y1, x2, y2, G_TX_RENDERTILE, 0, 0, 0, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawMapFrame(PlayState* play, s32 language) {
    static const s16 sColumnX[] = { -120, -40, 40, 120 };
    static const s16 sRowY[] = { 80, 48, 16, -16, -48, -80 };
    const TravelMapStyle* style = GetTravelMapStyle();

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    for (s16 column = 0; column < 3; column++) {
        const Color_RGB8* tint = (column == 1) ? &style->frameCenter : &style->frameEdge;
        s32 left = MAP_X(sColumnX[column]);
        s32 right = MAP_X(sColumnX[column + 1]);

        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, tint->r, tint->g, tint->b, 255);
        for (s16 row = 0; row < 5; row++) {
            s32 top = MAP_Y(sRowY[row]);
            s32 bottom = MAP_Y(sRowY[row + 1]);

            gDPLoadTextureBlock(OVERLAY_DISP++, sFrameTexs[language][column * 5 + row], G_IM_FMT_IA, G_IM_SIZ_8b, 80,
                                32, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gSPWideTextureRectangle(OVERLAY_DISP++, left, top, right, bottom, G_TX_RENDERTILE, 0, 0,
                                    80 * 4096 / (right - left), 32 * 4096 / (bottom - top));
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// The world map is one 216x128 CI8 image, drawn in the same strips the pause screen uses.
static void DrawWorldMap(PlayState* play) {
    s32 left = MAP_X(-108);
    s32 right = MAP_X(108);

    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetTextureFilter(OVERLAY_DISP++, G_TF_POINT);
    gDPLoadTLUT_pal256(OVERLAY_DISP++, gWorldMapImageTLUT);
    gDPSetTextureLUT(OVERLAY_DISP++, G_TT_RGBA16);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);

    for (s16 strip = 0; strip < 15; strip++) {
        s16 height = (strip < 14) ? 9 : 2;
        s32 top = MAP_Y(58 - strip * 9);
        s32 bottom = MAP_Y(58 - strip * 9 - height);

        gDPLoadMultiTile(OVERLAY_DISP++, gWorldMapImageTex, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, 216, 128, 0,
                         strip * 9, 215, strip * 9 + height - 1, 0, G_TX_WRAP | G_TX_NOMIRROR,
                         G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPSetTileSize(OVERLAY_DISP++, G_TX_RENDERTILE, 0, 0, (216 - 1) << G_TEXTURE_IMAGE_FRAC,
                       (height - 1) << G_TEXTURE_IMAGE_FRAC);
        gSPWideTextureRectangle(OVERLAY_DISP++, left, top, right, bottom, G_TX_RENDERTILE, 0, 0,
                                216 * 4096 / (right - left), height * 4096 / (bottom - top));
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawClouds(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetTextureFilter(OVERLAY_DISP++, G_TF_BILERP);
    gDPSetTextureLUT(OVERLAY_DISP++, G_TT_NONE);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    gDPSetCombineLERP(OVERLAY_DISP++, 1, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, 1, 0, PRIMITIVE, 0, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 235, 235, 235, 255);

    for (s16 i = 0; i < ARRAY_COUNT(sCloudTexs); i++) {
        if (gSaveContext.worldMapAreaData & gBitFlags[sCloudFlags[i]]) {
            continue;
        }

        s32 left = MAP_X(sCloudX[i]);
        s32 top = MAP_Y(sCloudY[i]);
        s32 right = MAP_X(sCloudX[i] + sCloudWidths[i]);
        s32 bottom = MAP_Y(sCloudY[i] - sCloudHeights[i]);

        gDPLoadTextureBlock_4b(OVERLAY_DISP++, sCloudTexs[i], G_IM_FMT_I, sCloudWidths[i], sCloudHeights[i], 0,
                               G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK,
                               G_TX_NOLOD, G_TX_NOLOD);
        gSPWideTextureRectangle(OVERLAY_DISP++, left, top, right, bottom, G_TX_RENDERTILE, 0, 0,
                                sCloudWidths[i] * 4096 / (right - left), sCloudHeights[i] * 4096 / (bottom - top));
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// The selected box pulses from the marker colour to white; locked destinations stay grey.
static Color_RGBA8 GetMarkerColor(int32_t index, s16 pulse) {
    const Color_RGB8* base = &GetTravelMapStyle()->marker;
    Color_RGBA8 color = { 100, 100, 100, 255 };

    if (index == sTravelCursor) {
        color.r = (u8)(base->r + ((255 - base->r) * pulse) / 100);
        color.g = (u8)(base->g + ((255 - base->g) * pulse) / 100);
        color.b = (u8)(base->b + ((255 - base->b) * pulse) / 100);
    } else if (IsDestinationUnlocked(sTravelMode, index)) {
        color.r = base->r;
        color.g = base->g;
        color.b = base->b;
    }
    return color;
}

static void DrawMarkerBoxes(PlayState* play, s16 pulse) {
    SetupSolidRects(play);
    for (int32_t i = 0; i < CountDestinations(sTravelMode); i++) {
        const MapMarker* marker = GetMarker(sTravelMode, i);
        Color_RGBA8 color = GetMarkerColor(i, pulse);
        s32 left = MAP_X(marker->x - MAP_BOX_HALF_WIDTH);
        s32 top = MAP_Y(marker->y + MAP_BOX_HALF_HEIGHT);
        s32 right = MAP_X(marker->x + MAP_BOX_HALF_WIDTH);
        s32 bottom = MAP_Y(marker->y - MAP_BOX_HALF_HEIGHT);

        SolidRect(play, left, top, right, top + (2 << 2), color);
        SolidRect(play, left, bottom - (2 << 2), right, bottom, color);
        SolidRect(play, left, top, left + (2 << 2), bottom, color);
        SolidRect(play, right - (2 << 2), top, right, bottom, color);
    }
}

// Stretches the texture already loaded onto a square around the cursor's marker.
static void DrawLoadedIconAtCursor(PlayState* play, s16 texels, s16 halfExtent) {
    const MapMarker* marker = GetMarker(sTravelMode, sTravelCursor);
    s32 left = MAP_X(marker->x - halfExtent);
    s32 top = MAP_Y(marker->y + halfExtent);
    s32 right = MAP_X(marker->x + halfExtent);
    s32 bottom = MAP_Y(marker->y - halfExtent);

    OPEN_DISPS(play->state.gfxCtx);
    gSPWideTextureRectangle(OVERLAY_DISP++, left, top, right, bottom, G_TX_RENDERTILE, 0, 0,
                            texels * 4096 / (right - left), texels * 4096 / (bottom - top));
    gDPPipeSync(OVERLAY_DISP++);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawLetterSelector(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);
    gDPLoadTextureBlock(OVERLAY_DISP++, sLetterIconTex, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0,
                        G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawLoadedIconAtCursor(play, 32, 10);
}

static void DrawStrayFairySelector(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetCombineLERP(OVERLAY_DISP++, 1, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, 1, 0, PRIMITIVE, 0, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 180, 255, 140);
    gDPLoadTextureBlock_4b(OVERLAY_DISP++, sStrayFairyGlowTex, G_IM_FMT_I, 16, 16, 0, G_TX_WRAP | G_TX_NOMIRROR,
                           G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawLoadedIconAtCursor(play, 16, 14);

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 210, 255, 255);
    gDPLoadTextureBlock(OVERLAY_DISP++, sStrayFairyHeadTex, G_IM_FMT_IA, G_IM_SIZ_8b, 32, 32, 0,
                        G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawLoadedIconAtCursor(play, 32, 10);
}

// Majora's Letter to Kafei marks the mailbox, a Stray Fairy in her glow marks the fountain.
static void DrawSelectorIcon(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetTextureFilter(OVERLAY_DISP++, G_TF_BILERP);
    gDPSetTextureLUT(OVERLAY_DISP++, G_TT_NONE);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);

    if (sTravelMode == TRAVEL_MAILBOX) {
        DrawLetterSelector(play);
    } else {
        DrawStrayFairySelector(play);
    }
}

// Where the pause screen writes the area you are standing in: the title, then the name on the parchment.
static void DrawDestinationName(PlayState* play, s32 language) {
    const Color_RGB8* nameColor = &GetTravelMapStyle()->name;
    bool isUnlocked = IsDestinationUnlocked(sTravelMode, sTravelCursor);
    s32 left = MAP_X(20);
    s32 top = MAP_Y(-26);
    s32 right = MAP_X(84);
    s32 bottom = MAP_Y(-34);

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetTextureFilter(OVERLAY_DISP++, G_TF_BILERP);
    gDPSetTextureLUT(OVERLAY_DISP++, G_TT_NONE);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    gDPSetCombineLERP(OVERLAY_DISP++, 1, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, 1, 0, PRIMITIVE, 0, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, 255);
    gDPLoadTextureBlock_4b(OVERLAY_DISP++, sCurrentPositionTexs[language], G_IM_FMT_I, 64, 8, 0,
                           G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                           G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, left, top, right, bottom, G_TX_RENDERTILE, 0, 0, 64 * 4096 / (right - left),
                            8 * 4096 / (bottom - top));
    gDPPipeSync(OVERLAY_DISP++);

    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    if (isUnlocked) {
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, nameColor->r, nameColor->g, nameColor->b, 255);
    } else {
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 120, 120, 120, 255);
    }
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);

    left = MAP_X(19);
    top = MAP_Y(-36);
    right = MAP_X(99);
    bottom = MAP_Y(-68);
    gDPLoadTextureBlock(OVERLAY_DISP++, GetMarker(sTravelMode, sTravelCursor)->nameTex[language], G_IM_FMT_IA,
                        G_IM_SIZ_8b, 80, 32, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK,
                        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, left, top, right, bottom, G_TX_RENDERTILE, 0, 0, 80 * 4096 / (right - left),
                            32 * 4096 / (bottom - top));
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawTravelMap(PlayState* play) {
    static const Color_RGBA8 sBackdrop = { 0, 0, 0, 64 };
    static s16 sPulseTimer;

    if (sTravelMode == TRAVEL_NONE || play == NULL) {
        return;
    }
    sPulseTimer++;

    s32 language = GetLanguage();
    s16 phase = sPulseTimer % 40;
    s16 pulse = (phase < 20) ? phase * 5 : (40 - phase) * 5;

    SetupSolidRects(play);
    SolidRect(play, 0, 0, SCREEN_WIDTH << 2, SCREEN_HEIGHT << 2, sBackdrop);
    DrawMapFrame(play, language);
    DrawWorldMap(play);
    DrawClouds(play);
    DrawMarkerBoxes(play, pulse);
    if (sHasSelectorArt && (sPulseTimer % 16) < 11) {
        DrawSelectorIcon(play);
    }
    DrawDestinationName(play, language);
}

// ---- mailboxes ----

typedef struct {
    Actor actor;
    ColliderCylinder collider;
} MaskMailbox;

static ColliderCylinderInit sMailboxCollider = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0, 0, 0 }, { 0, 0, 0 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON },
    { 28, 80, 0, { 0, 0, 0 } },
};

static void MailboxInit(Actor* actor, PlayState* play) {
    MaskMailbox* mailbox = (MaskMailbox*)actor;

    Collider_InitCylinder(play, &mailbox->collider);
    Collider_SetCylinder(play, &mailbox->collider, actor, &sMailboxCollider);
    actor->colChkInfo.mass = MASS_IMMOVABLE;
    Actor_SetScale(actor, 0.02f);
}

static void MailboxDestroy(Actor* actor, PlayState* play) {
    Collider_DestroyCylinder(play, &((MaskMailbox*)actor)->collider);
}

// textId 0 means accepting the talk never starts a textbox; only the talking state is left to undo.
static void MailboxUpdate(Actor* actor, PlayState* play) {
    MaskMailbox* mailbox = (MaskMailbox*)actor;
    Player* player = GET_PLAYER(play);

    Collider_UpdateCylinder(actor, &mailbox->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &mailbox->collider.base);
    if (Actor_ProcessTalkRequest(actor, play)) {
        player->stateFlags1 &= ~(PLAYER_STATE1_TALKING | PLAYER_STATE1_IN_CUTSCENE);
        player->talkActor = NULL;
        player->actor.flags &= ~ACTOR_FLAG_TALK;
        func_80853080(player, play);
        if (IsWearing(MASK_POSTMAN)) {
            OpenTravelMap(play, player, TRAVEL_MAILBOX);
        }
        return;
    }
    if (IsWearing(MASK_POSTMAN) && sTravelMode == TRAVEL_NONE) {
        actor->textId = 0;
        Actor_OfferTalk(actor, play, MAILBOX_TALK_RADIUS);
    }
}

static void MailboxDraw(Actor* actor, PlayState* play) {
    static const char path[] = "__OTR__objects/object_pst/gPostboxFrameDL";

    if (!sApi->HasResource(path)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)gCullBackDList);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gEmptyDL);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)path);
    CLOSE_DISPS(play->state.gfxCtx);
}

static bool IsMailboxInScene(const MailPoint* point, int16_t sceneNum) {
    return point->scene == sceneNum || point->alternate == sceneNum;
}

// Walking up to a mailbox is what adds it to the map.
static void UpdatePostmanMailboxes(PlayState* play, Player* player) {
    if (!sApi->IsCustomItemOwned(sMasks[MASK_POSTMAN].key) || sMailboxActorId < 0) {
        return;
    }
    for (int32_t i = 0; i < ARRAY_COUNT(sMailPoints); i++) {
        const MailPoint* point = &sMailPoints[i];

        if (!IsMailboxInScene(point, play->sceneNum)) {
            continue;
        }
        if (!(sMailboxSpawned & (1 << i)) &&
            Actor_Spawn(&play->actorCtx, play, sMailboxActorId, point->pos.x, point->pos.y, point->pos.z, 0,
                        point->yaw, 0, i) != NULL) {
            sMailboxSpawned |= 1 << i;
        }

        f32 dx = player->actor.world.pos.x - point->pos.x;
        f32 dz = player->actor.world.pos.z - point->pos.z;

        if (SQ(dx) + SQ(dz) < SQ(MAILBOX_UNLOCK_RADIUS) && !sApi->GetRandoFlag(point->flag)) {
            sApi->SetRandoFlag(point->flag, true);
            Sfx_PlaySfxCentered(NA_SE_SY_GET_ITEM);
        }
    }
    if (IsWearing(MASK_POSTMAN) && CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
        OpenTravelMap(play, player, TRAVEL_MAILBOX);
    }
}

static void RegisterMaskTravel(void) {
    SOHActorDefinition mailbox = { sizeof(SOHActorDefinition), "skijer.postman_mailbox", "Postman's mailbox",
        ACTORCAT_PROP, ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED,
        OBJECT_GAMEPLAY_KEEP, sizeof(MaskMailbox), MailboxInit, MailboxDestroy, MailboxUpdate, MailboxDraw };

    sMailboxActorId = sApi->RegisterActor(&mailbox);
    for (int32_t i = 0; i < ARRAY_COUNT(sMailPoints); i++) {
        sApi->RegisterRandoFlag(sMailPoints[i].flag);
    }
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawTravelMap);
    sApi->RegisterVB(VB_OPEN_PAUSE_MENU, BlockPauseDuringTravelMap);
}
