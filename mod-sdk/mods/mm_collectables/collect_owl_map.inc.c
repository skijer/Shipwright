#define OWL_STATUE_COUNT 10
#define OWL_CLOCK_TOWN 4
#define OWL_MAP_CLOUD_COUNT 15
#define OWL_MAP_LEFT 51
#define OWL_MAP_TOP 62
#define OWL_MAP_WIDTH 216
#define OWL_MAP_HEIGHT 128
#define OWL_MAP_BAND_ROWS 8
#define OWL_FACE_WIDTH 24
#define OWL_FACE_HEIGHT 12
#define OWL_NAME_WIDTH 128
#define OWL_NAME_HEIGHT 16
#define OWL_STICK_DEADZONE 30
#define OWL_MAP_INPUT_OWNER "mm_collectables.owl_map"
#define PLAYER_CSACTION_WAIT_IN_PLACE 1
#define PLAYER_CSACTION_END_WAIT 7
#define PAGE_SCREEN_X(x) (160 + (x))
#define PAGE_SCREEN_Y(y) (121 - (y))
#define PAGE_BG_SCALE 1.0327f
#define CURSOR_SCALE 1.2675f
#define PAGE_BG_TILE_WIDTH 80
#define PAGE_BG_TILE_HEIGHT 32
#define CURSOR_ORB_SIZE 16
#define CURSOR_ORBIT 15.0f
#define CURSOR_SPIN_STEP 0x300
#define NAME_PANEL_WIDTH 72
#define NAME_PANEL_HEIGHT 24

typedef bool (*ReadMmWorldMapFn)(u16* owlStatues, u32* visibleClouds);
typedef void (*DepartFn)(const char* portal);
typedef enum { OWL_MAP_CLOSED, OWL_MAP_CHOOSING, OWL_MAP_CONFIRMING } OwlMapStage;

typedef struct {
    const char* name;
    const char* nameTex;
    s16 x;
    s16 y;
    f32 cursorX;
    f32 cursorY;
} OwlStatue;

typedef struct {
    const char* tex;
    s16 x;
    s16 y;
    s16 width;
    s16 height;
} MapCloud;

static const OwlStatue sOwlStatues[OWL_STATUE_COUNT] = {
    { "Great Bay Coast", "__OTR__map_name_static/gMapPointGreatBayCoastENGTex", -80, -8, -50.0f, -14.0f },
    { "Zora Cape", "__OTR__map_name_static/gMapPointZoraCapeENGTex", -64, -38, -38.0f, -39.0f },
    { "Snowhead", "__OTR__map_name_static/gMapPointSnowheadENGTex", -9, 39, 6.0f, 23.0f },
    { "Mountain Village", "__OTR__map_name_static/gMapPointMountainVillageENGTex", -3, 26, 11.0f, 11.0f },
    { "Clock Town", "__OTR__map_name_static/gMapPointClockTownENGTex", -7, 1, 8.0f, -8.0f },
    { "Milk Road", "__OTR__map_name_static/gMapPointMilkRoadENGTex", -16, -7, 0.0f, -15.0f },
    { "Woodfall", "__OTR__map_name_static/gMapPointWoodfallENGTex", -1, -28, 12.0f, -31.0f },
    { "Southern Swamp", "__OTR__map_name_static/gMapPointSouthernSwampENGTex", 23, -27, 31.0f, -30.0f },
    { "Ikana Canyon", "__OTR__map_name_static/gMapPointIkanaCanyonENGTex", 44, -1, 48.0f, -10.0f },
    { "Stone Tower", "__OTR__map_name_static/gMapPointStoneTowerENGTex", 54, 24, 56.0f, 11.0f },
};

static const char* const sMapPageTextures[3][5] = {
    { "__OTR__icon_item_static_yar/gPauseMap00Tex", "__OTR__icon_item_static_yar/gPauseMap01Tex",
      "__OTR__icon_item_static_yar/gPauseMap02Tex", "__OTR__icon_item_static_yar/gPauseMap03Tex",
      "__OTR__icon_item_static_yar/gPauseMap04Tex" },
    { "__OTR__icon_item_jpn_static/gPauseMap10ENGTex", "__OTR__icon_item_static_yar/gPauseMap11Tex",
      "__OTR__icon_item_static_yar/gPauseMap12Tex", "__OTR__icon_item_static_yar/gPauseMap13Tex",
      "__OTR__icon_item_static_yar/gPauseMap14Tex" },
    { "__OTR__icon_item_static_yar/gPauseMap20Tex", "__OTR__icon_item_static_yar/gPauseMap21Tex",
      "__OTR__icon_item_static_yar/gPauseMap22Tex", "__OTR__icon_item_static_yar/gPauseMap23Tex",
      "__OTR__icon_item_static_yar/gPauseMap24Tex" },
};

static const MapCloud sMapClouds[OWL_MAP_CLOUD_COUNT] = {
    { "__OTR__icon_item_field_static/gWorldMapClockTownCloud1Tex", -41, 11, 80, 40 },
    { "__OTR__icon_item_field_static/gWorldMapClockTownCloud2Tex", 9, 22, 64, 31 },
    { "__OTR__icon_item_field_static/gWorldMapWoodfallCloud1Tex", 43, -16, 64, 53 },
    { "__OTR__icon_item_field_static/gWorldMapWoodfallCloud2Tex", -21, -16, 64, 53 },
    { "__OTR__icon_item_field_static/gWorldMapWoodfallCloud3Tex", -37, -16, 16, 53 },
    { "__OTR__icon_item_field_static/gWorldMapSnowheadCloud1Tex", 43, 59, 64, 52 },
    { "__OTR__icon_item_field_static/gWorldMapSnowheadCloud2Tex", -69, 24, 112, 35 },
    { "__OTR__icon_item_field_static/gWorldMapSnowheadCloud3Tex", -69, 59, 112, 35 },
    { "__OTR__icon_item_field_static/gWorldMapRomaniRanchCloudTex", -32, -11, 48, 32 },
    { "__OTR__icon_item_field_static/gWorldMapGreatBayCloud1Tex", -109, -5, 64, 64 },
    { "__OTR__icon_item_field_static/gWorldMapGreatBayCloud2Tex", -45, -5, 64, 64 },
    { "__OTR__icon_item_field_static/gWorldMapGreatBayCloud3Tex", -109, 59, 64, 64 },
    { "__OTR__icon_item_field_static/gWorldMapGreatBayCloud4Tex", -45, 59, 64, 64 },
    { "__OTR__icon_item_field_static/gWorldMapStoneTowerCloud1Tex", 59, 39, 48, 82 },
    { "__OTR__icon_item_field_static/gWorldMapStoneTowerCloud2Tex", -5, 31, 64, 61 },
};

static const ALIGN_ASSET(2) char sWorldMapTex[] = "__OTR__icon_item_field_static/gWorldMapImageTex";
static const ALIGN_ASSET(2) char sWorldMapTlut[] = "__OTR__icon_item_field_static/gWorldMapImageTLUT";
static const ALIGN_ASSET(2) char sOwlFaceTex[] = "__OTR__icon_item_field_static/gWorldMapOwlFaceTex";
static const ALIGN_ASSET(2) char sCursorOrbTex[] = "__OTR__icon_item_static_yar/gPauseMenuCursorTex";
static const ALIGN_ASSET(2) char sNamePanelLeftTex[] = "__OTR__icon_item_static_yar/gNamePanelLeftTex";
static const ALIGN_ASSET(2) char sNamePanelRightTex[] = "__OTR__icon_item_static_yar/gNamePanelRightTex";
static const char sNoOwlStatuesText[] =
    "You have yet to leave your mark on any of the statues you have come across. Hurry up and find some.";

static OwlMapStage sOwlMapStage;
static u16 sActiveOwls;
static u32 sVisibleClouds;
static s32 sOwlCursor;
static bool sOwlStickHeld;

static DepartFn FindFleetDeparture(void) {
    return SOH_MOD_API_HAS(sApi, FindService) ? (DepartFn)sApi->FindService("fleet.depart") : NULL;
}

static ReadMmWorldMapFn FindMmWorldMap(void) {
    return SOH_MOD_API_HAS(sApi, FindService) ? (ReadMmWorldMapFn)sApi->FindService("fleet.mm_world_map") : NULL;
}

static bool IsOwlActive(s32 owl) {
    return (sActiveOwls & (1 << owl)) != 0;
}

static void FreezePlayer(PlayState* play, bool frozen) {
    Player_SetCsActionWithHaltedActors(play, NULL, frozen ? PLAYER_CSACTION_WAIT_IN_PLACE : PLAYER_CSACTION_END_WAIT);
    if (frozen) {
        sApi->BlockPlayerInput(OWL_MAP_INPUT_OWNER, 0xFFFF, true);
    } else {
        sApi->ReleasePlayerInput(OWL_MAP_INPUT_OWNER);
    }
}

static void CloseOwlMap(PlayState* play) {
    if (sOwlMapStage == OWL_MAP_CLOSED) {
        return;
    }
    sOwlMapStage = OWL_MAP_CLOSED;
    FreezePlayer(play, false);
}

static bool OpenOwlMap(PlayState* play) {
    ReadMmWorldMapFn readWorldMap = FindMmWorldMap();
    if (readWorldMap == NULL || FindFleetDeparture() == NULL || !readWorldMap(&sActiveOwls, &sVisibleClouds)) {
        return false;
    }
    if (sActiveOwls == 0) {
        sApi->ShowTextbox(play, sNoOwlStatuesText, true);
        return true;
    }
    sOwlCursor = 0;
    while (!IsOwlActive(sOwlCursor)) {
        sOwlCursor++;
    }
    if (IsOwlActive(OWL_CLOCK_TOWN)) {
        sOwlCursor = OWL_CLOCK_TOWN;
    }
    sOwlStickHeld = true;
    sOwlMapStage = OWL_MAP_CHOOSING;
    FreezePlayer(play, true);
    Audio_PlaySfxGeneral(NA_SE_SY_WIN_OPEN, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    return true;
}

static void PlayMenuSfx(u16 sfxId) {
    Audio_PlaySfxGeneral(sfxId, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultReverb);
}

static bool ReadOwlDirection(Input* input, s32* dirX, s32* dirY) {
    *dirX = CHECK_BTN_ALL(input->press.button, BTN_DRIGHT) - CHECK_BTN_ALL(input->press.button, BTN_DLEFT);
    *dirY = CHECK_BTN_ALL(input->press.button, BTN_DUP) - CHECK_BTN_ALL(input->press.button, BTN_DDOWN);
    if (*dirX != 0 || *dirY != 0) {
        return true;
    }
    const s32 stickX = input->rel.stick_x;
    const s32 stickY = input->rel.stick_y;
    if (ABS(stickX) < OWL_STICK_DEADZONE && ABS(stickY) < OWL_STICK_DEADZONE) {
        sOwlStickHeld = false;
        return false;
    }
    if (sOwlStickHeld) {
        return false;
    }
    sOwlStickHeld = true;
    *dirX = ABS(stickX) >= OWL_STICK_DEADZONE ? (stickX > 0 ? 1 : -1) : 0;
    *dirY = ABS(stickY) >= OWL_STICK_DEADZONE ? (stickY > 0 ? 1 : -1) : 0;
    return true;
}

static s32 FindOwlTowards(s32 dirX, s32 dirY) {
    const OwlStatue* from = &sOwlStatues[sOwlCursor];
    s32 best = sOwlCursor;
    s32 bestDistance = 0x7FFFFFFF;
    for (s32 owl = 0; owl < OWL_STATUE_COUNT; owl++) {
        const s32 dx = sOwlStatues[owl].x - from->x;
        const s32 dy = sOwlStatues[owl].y - from->y;
        const s32 distance = dx * dx + dy * dy;
        if (owl == sOwlCursor || !IsOwlActive(owl) || dx * dirX + dy * dirY <= 0 || distance >= bestDistance) {
            continue;
        }
        best = owl;
        bestDistance = distance;
    }
    return best;
}

static void AskToSoar(PlayState* play) {
    static char question[96];
    snprintf(question, sizeof(question), "Soar to %%r%s%%w?\x1B%%gYes&No%%w", sOwlStatues[sOwlCursor].name);
    if (sApi->ShowTextbox(play, question, false)) {
        sOwlMapStage = OWL_MAP_CONFIRMING;
        PlayMenuSfx(NA_SE_SY_DECIDE);
    }
}

static void ChooseOwl(PlayState* play) {
    Input* input = &play->state.input[0];
    if (CHECK_BTN_ANY(input->press.button, BTN_B | BTN_START)) {
        PlayMenuSfx(NA_SE_SY_WIN_CLOSE);
        CloseOwlMap(play);
        return;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        AskToSoar(play);
        return;
    }
    s32 dirX;
    s32 dirY;
    if (!ReadOwlDirection(input, &dirX, &dirY)) {
        return;
    }
    const s32 next = FindOwlTowards(dirX, dirY);
    if (next != sOwlCursor) {
        sOwlCursor = next;
        PlayMenuSfx(NA_SE_SY_CURSOR);
    }
}

static void AnswerSoar(PlayState* play) {
    if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
        sOwlMapStage = OWL_MAP_CHOOSING;
        return;
    }
    if (Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE || !Message_ShouldAdvance(play)) {
        return;
    }
    Message_CloseTextbox(play);
    if (play->msgCtx.choiceIndex != 0) {
        sOwlMapStage = OWL_MAP_CHOOSING;
        return;
    }
    char portal[16];
    snprintf(portal, sizeof(portal), "owl:%d", (int)sOwlCursor);
    CloseOwlMap(play);
    FindFleetDeparture()(portal);
}

// Runs before Message_Update: OoT closes a two-choice box on A by itself once the ocarina has ended.
static void UpdateOwlMap(void) {
    PlayState* play = gPlayState;
    if (play == NULL) {
        return;
    }
    switch (sOwlMapStage) {
        case OWL_MAP_CHOOSING:
            ChooseOwl(play);
            break;
        case OWL_MAP_CONFIRMING:
            AnswerSoar(play);
            break;
        case OWL_MAP_CLOSED:
        default:
            break;
    }
}

static void DrawTexRectScaled(Gfx** gfx, f32 left, f32 top, s32 texWidth, s32 texHeight, f32 scale) {
    gSPTextureRectangle((*gfx)++, (s32)(left * 4.0f), (s32)(top * 4.0f), (s32)((left + texWidth * scale) * 4.0f),
                        (s32)((top + texHeight * scale) * 4.0f), G_TX_RENDERTILE, 0, 0, (s32)(1024.0f / scale),
                        (s32)(1024.0f / scale));
}

static void DrawTexRect(Gfx** gfx, s32 left, s32 top, s32 width, s32 height) {
    DrawTexRectScaled(gfx, (f32)left, (f32)top, width, height, 1.0f);
}

static void LoadIA8(Gfx** gfx, const char* tex, s32 width, s32 height) {
    gDPLoadTextureBlock((*gfx)++, tex, G_IM_FMT_IA, G_IM_SIZ_8b, width, height, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
}

static void DrawMapPage(Gfx** gfx) {
    gDPPipeSync((*gfx)++);
    gDPSetCombineLERP((*gfx)++, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, TEXEL0);
    gDPSetPrimColor((*gfx)++, 0, 0, 180, 180, 120, 255);
    for (s32 column = 0; column < 3; column++) {
        for (s32 row = 0; row < 5; row++) {
            const f32 pageX = -120.0f + column * PAGE_BG_TILE_WIDTH;
            const f32 pageY = 80.0f - row * PAGE_BG_TILE_HEIGHT;
            LoadIA8(gfx, sMapPageTextures[column][row], PAGE_BG_TILE_WIDTH, PAGE_BG_TILE_HEIGHT);
            DrawTexRectScaled(gfx, 160.0f + pageX * PAGE_BG_SCALE, 120.0f - pageY * PAGE_BG_SCALE, PAGE_BG_TILE_WIDTH,
                              PAGE_BG_TILE_HEIGHT, PAGE_BG_SCALE);
        }
    }
}

static void DrawWorldMapImage(Gfx** gfx) {
    gDPPipeSync((*gfx)++);
    gDPSetTextureFilter((*gfx)++, G_TF_POINT);
    gDPLoadTLUT_pal256((*gfx)++, sWorldMapTlut);
    gDPSetTextureLUT((*gfx)++, G_TT_RGBA16);
    gDPSetCombineMode((*gfx)++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
    gDPSetPrimColor((*gfx)++, 0, 0, 255, 255, 255, 255);
    for (s32 band = 0; band < OWL_MAP_HEIGHT / OWL_MAP_BAND_ROWS; band++) {
        const s32 top = OWL_MAP_TOP + band * OWL_MAP_BAND_ROWS;
        gDPLoadMultiTile((*gfx)++, sWorldMapTex, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, OWL_MAP_WIDTH,
                         OWL_MAP_HEIGHT, 0, band * OWL_MAP_BAND_ROWS, OWL_MAP_WIDTH - 1,
                         (band + 1) * OWL_MAP_BAND_ROWS - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPSetTileSize((*gfx)++, G_TX_RENDERTILE, 0, 0, (OWL_MAP_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                       (OWL_MAP_BAND_ROWS - 1) << G_TEXTURE_IMAGE_FRAC);
        DrawTexRect(gfx, OWL_MAP_LEFT, top, OWL_MAP_WIDTH, OWL_MAP_BAND_ROWS);
    }
    gDPPipeSync((*gfx)++);
    gDPSetTextureLUT((*gfx)++, G_TT_NONE);
    gDPSetTextureFilter((*gfx)++, G_TF_BILERP);
}

static void DrawMapClouds(Gfx** gfx) {
    gDPSetCombineLERP((*gfx)++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor((*gfx)++, 0, 0, 215, 235, 235, 255);
    gDPSetEnvColor((*gfx)++, 40, 60, 100, 0);
    for (s32 cloud = 0; cloud < OWL_MAP_CLOUD_COUNT; cloud++) {
        const MapCloud* mapCloud = &sMapClouds[cloud];
        if (sVisibleClouds & (1 << cloud)) {
            continue;
        }
        LoadIA8(gfx, mapCloud->tex, mapCloud->width, mapCloud->height);
        DrawTexRect(gfx, PAGE_SCREEN_X(mapCloud->x), PAGE_SCREEN_Y(mapCloud->y), mapCloud->width, mapCloud->height);
    }
}

static void DrawOwlWarpShade(Gfx** gfx) {
    gDPPipeSync((*gfx)++);
    gDPSetCombineMode((*gfx)++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor((*gfx)++, 0, 0, 0, 0, 0, 120);
    gDPFillRectangle((*gfx)++, 50, 62, 270, 190);
}

static void DrawOwlFaces(Gfx** gfx) {
    gDPPipeSync((*gfx)++);
    gDPSetCombineMode((*gfx)++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor((*gfx)++, 0, 0, 255, 255, 255, 255);
    gDPLoadTextureBlock((*gfx)++, sOwlFaceTex, G_IM_FMT_RGBA, G_IM_SIZ_32b, OWL_FACE_WIDTH, OWL_FACE_HEIGHT, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    for (s32 owl = 0; owl < OWL_STATUE_COUNT; owl++) {
        if (IsOwlActive(owl)) {
            DrawTexRect(gfx, PAGE_SCREEN_X(sOwlStatues[owl].x), PAGE_SCREEN_Y(sOwlStatues[owl].y), OWL_FACE_WIDTH,
                        OWL_FACE_HEIGHT);
        }
    }
}

static void DrawOwlCursor(Gfx** gfx, PlayState* play) {
    const s16 phase = (s16)(play->gameplayFrames * CURSOR_SPIN_STEP);
    const f32 blend = (Math_SinS((s16)(play->gameplayFrames * 0x0CCC)) + 1.0f) * 0.5f;
    const OwlStatue* statue = &sOwlStatues[sOwlCursor];
    gDPPipeSync((*gfx)++);
    gDPSetCombineLERP((*gfx)++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor((*gfx)++, 0, 0, 100, (u8)(150 + 105 * blend), 255, 255);
    gDPSetEnvColor((*gfx)++, 0, (u8)(150 * blend), (u8)(100 + 155 * blend), 255);
    LoadIA8(gfx, sCursorOrbTex, CURSOR_ORB_SIZE, CURSOR_ORB_SIZE);
    for (s32 orb = 0; orb < 4; orb++) {
        const s16 angle = (s16)(phase + orb * 0x4000);
        const f32 left = statue->cursorX + Math_SinS(angle) * CURSOR_ORBIT - CURSOR_ORB_SIZE / 2;
        const f32 top = statue->cursorY + Math_CosS(angle) * CURSOR_ORBIT + CURSOR_ORB_SIZE / 2;
        DrawTexRectScaled(gfx, 160.0f + left * CURSOR_SCALE, 120.0f - top * CURSOR_SCALE, CURSOR_ORB_SIZE,
                          CURSOR_ORB_SIZE, CURSOR_SCALE);
    }
}

static void DrawOwlName(Gfx** gfx) {
    gDPPipeSync((*gfx)++);
    gDPSetCombineMode((*gfx)++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor((*gfx)++, 0, 0, 150, 140, 90, 255);
    LoadIA8(gfx, sNamePanelLeftTex, NAME_PANEL_WIDTH, NAME_PANEL_HEIGHT);
    DrawTexRect(gfx, 160 - NAME_PANEL_WIDTH, 196, NAME_PANEL_WIDTH, NAME_PANEL_HEIGHT);
    LoadIA8(gfx, sNamePanelRightTex, NAME_PANEL_WIDTH, NAME_PANEL_HEIGHT);
    DrawTexRect(gfx, 160, 196, NAME_PANEL_WIDTH, NAME_PANEL_HEIGHT);
    gDPPipeSync((*gfx)++);
    gDPSetCombineLERP((*gfx)++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor((*gfx)++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor((*gfx)++, 20, 30, 40, 0);
    gDPLoadTextureBlock_4b((*gfx)++, sOwlStatues[sOwlCursor].nameTex, G_IM_FMT_IA, OWL_NAME_WIDTH, OWL_NAME_HEIGHT, 0,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                           G_TX_NOLOD);
    DrawTexRect(gfx, 97, 200, OWL_NAME_WIDTH, OWL_NAME_HEIGHT);
}

static void DrawOwlMap(PlayState* play) {
    if (sOwlMapStage == OWL_MAP_CLOSED) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, 110);
    gDPFillRectangle(OVERLAY_DISP++, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    DrawMapPage(&OVERLAY_DISP);
    DrawWorldMapImage(&OVERLAY_DISP);
    DrawMapClouds(&OVERLAY_DISP);
    DrawOwlWarpShade(&OVERLAY_DISP);
    DrawOwlFaces(&OVERLAY_DISP);
    if (sOwlMapStage == OWL_MAP_CHOOSING) {
        DrawOwlCursor(&OVERLAY_DISP, play);
    }
    DrawOwlName(&OVERLAY_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}
