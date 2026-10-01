/**
 * The Minish Cap: by a pod soil the item opens a warp map over the world map, away from one it shrinks Link to
 * Minish size. A pod soil is a destination once its Gold Skulltula is dead.
 */

#include <math.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "regs.h"
#include "variables.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "textures/icon_item_nes_static/icon_item_nes_static.h"
#include "textures/icon_item_ger_static/icon_item_ger_static.h"
#include "textures/icon_item_fra_static/icon_item_fra_static.h"
#include "textures/icon_item_jpn_static/icon_item_jpn_static.h"
#include "textures/icon_item_field_static/icon_item_field_static.h"
#include "textures/icon_item_24_static/icon_item_24_static.h"
#include "textures/map_name_static/map_name_static.h"

#define MINISH_CAP_KEY "nei.minish_cap"
#define MINISH_CAP_OWNER "minish_cap"
#define MINISH_CAP_TIME_PRIORITY 5000

#define POD_SOIL_COUNT 10
#define POD_SOIL_REACH 50.0f
#define MINISH_LANGUAGE_COUNT 4

#define PLAYER_SCALE_NORMAL 0.01f
#define MINISH_SCALE 0.001f
#define MINISH_SIZE_FACTOR (MINISH_SCALE / PLAYER_SCALE_NORMAL)
#define MINISH_SPEED_FACTOR 0.2f
#define WARP_SCALE 0.0005f
#define SCALE_STEP 0.0005f
#define MINISH_CAMERA_FLOOR 0.14f
#define MINISH_BODY_EXTENT 2
#define CRAWLSPACE_LINGER 8

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconMinishCapTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gMinishCapNameTex";
static const ALIGN_ASSET(2) char sPecoriTex[] = "__OTR__textures/icon_item_custom/gItemIconPecoriTex";
static const ALIGN_ASSET(2) char sCapDL[] = "__OTR__objects/object_nei_minish_cap/Cylinder_opaque_dl";

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate",
    "OnInterfaceDrawEnd",
    "OnCameraResolveView",
    "OnActorResolveBgCheckFlags",
    "OnPlayerResolveMotionScale",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

// A bean spot: where the warp lands and where its box sits on the pause world map. gsMask 0 = no skulltula
// guards it. Positions, params and rooms read off the scene actor lists.
typedef struct {
    s16 sceneId;
    s16 entranceIndex;
    Vec3f pos;
    s16 rotY;
    s16 gsGroup;
    u8 gsMask;
    u8 roomIndex;
    s16 mapX;
    s16 mapY;
    const char* nameTex[MINISH_LANGUAGE_COUNT];
} PodSoil;

static const PodSoil sPodSoils[POD_SOIL_COUNT] = {
    {
        SCENE_KOKIRI_FOREST,
        ENTR_KOKIRI_FOREST_0,
        { 1190.0f, 0.0f, -480.0f },
        0x0000,
        12,
        0x01,
        0,
        73,
        -12,
        { gKokiriForestPositionNameENGTex, gKokiriForestPositionNameGERTex, gKokiriForestPositionNameFRATex,
          gKokiriForestPositionNameJPNTex },
    },
    {
        SCENE_LOST_WOODS,
        ENTR_LOST_WOODS_SOUTH_EXIT,
        { -1220.0f, 0.0f, 935.0f },
        0x0000,
        13,
        0x01,
        5,
        58,
        -6,
        { gLostWoodsPositionNameENGTex, gLostWoodsPositionNameGERTex, gLostWoodsPositionNameFRATex,
          gLostWoodsPositionNameJPNTex },
    },
    {
        SCENE_LOST_WOODS,
        ENTR_LOST_WOODS_SOUTH_EXIT,
        { 610.0f, 0.0f, -1770.0f },
        0x0000,
        13,
        0x02,
        6,
        67,
        3,
        { gSacredForestMeadowPositionNameENGTex, gSacredForestMeadowPositionNameGERTex,
          gSacredForestMeadowPositionNameFRATex, gSacredForestMeadowPositionNameJPNTex },
    },
    {
        SCENE_LAKE_HYLIA,
        ENTR_LAKE_HYLIA_NORTH_EXIT,
        { -2602.0f, -1033.0f, 3617.0f },
        0x0000,
        18,
        0x01,
        0,
        -25,
        -50,
        { gLakeHyliaPositionNameENGTex, gLakeHyliaPositionNameGERTex, gLakeHyliaPositionNameFRATex,
          gLakeHyliaPositionNameJPNTex },
    },
    {
        SCENE_GRAVEYARD,
        ENTR_GRAVEYARD_ENTRANCE,
        { -715.0f, 120.0f, -340.0f },
        0x0000,
        16,
        0x01,
        1,
        60,
        29,
        { gGraveyardPositionNameENGTex, gGraveyardPositionNameGERTex, gGraveyardPositionNameFRATex,
          gGraveyardPositionNameJPNTex },
    },
    {
        SCENE_DEATH_MOUNTAIN_TRAIL,
        ENTR_DEATH_MOUNTAIN_TRAIL_BOTTOM_EXIT,
        { -1610.0f, 677.0f, -735.0f },
        0x0000,
        15,
        0x02,
        0,
        35,
        44,
        { gDeathMountainTrailPositionNameENGTex, gDeathMountainTrailPositionNameGERTex,
          gDeathMountainTrailPositionNameFRATex, gDeathMountainTrailPositionNameJPNTex },
    },
    {
        SCENE_DEATH_MOUNTAIN_CRATER,
        ENTR_DEATH_MOUNTAIN_CRATER_UPPER_EXIT,
        { -127.0f, 421.0f, -168.0f },
        0x0000,
        15,
        0x01,
        1,
        40,
        52,
        { gDeathMountainCraterPositionNameENGTex, gDeathMountainCraterPositionNameGERTex,
          gDeathMountainCraterPositionNameFRATex, gDeathMountainCraterPositionNameJPNTex },
    },
    {
        SCENE_DESERT_COLOSSUS,
        ENTR_DESERT_COLOSSUS_EAST_EXIT,
        { -1330.0f, 8.0f, 290.0f },
        0x0000,
        21,
        0x01,
        0,
        -93,
        29,
        { gDesertColossusPositionNameENGTex, gDesertColossusPositionNameGERTex, gDesertColossusPositionNameFRATex,
          gDesertColossusPositionNameJPNTex },
    },
    {
        SCENE_GERUDO_VALLEY,
        ENTR_GERUDO_VALLEY_EAST_EXIT,
        { -515.0f, -2051.0f, 110.0f },
        0x0000,
        19,
        0x01,
        0,
        -51,
        10,
        { gGerudoValleyPositionNameENGTex, gGerudoValleyPositionNameGERTex, gGerudoValleyPositionNameFRATex,
          gGerudoValleyPositionNameJPNTex },
    },
    {
        SCENE_ZORAS_RIVER,
        ENTR_ZORAS_RIVER_WEST_EXIT,
        { -730.0f, 100.0f, -220.0f },
        0x4000,
        0,
        0x00,
        0,
        78,
        18,
        { gZorasRiverPositionNameENGTex, gZorasRiverPositionNameGERTex, gZorasRiverPositionNameFRATex,
          gZorasRiverPositionNameJPNTex },
    },
};

typedef enum {
    MINISH_SIZE_NORMAL,
    MINISH_SIZE_SHRINKING,
    MINISH_SIZE_TINY,
    MINISH_SIZE_GROWING,
} MinishSizePhase;

typedef enum {
    MINISH_WARP_IDLE,
    MINISH_WARP_DEPARTING,
    MINISH_WARP_ARRIVING,
} MinishWarpPhase;

// The host's own state, declared here because GameInteractor.h is C++: borrowed and handed back like a cheat.
u8 GameInteractor_GetDisableLedgeGrabsActive(void);
void GameInteractor_SetDisableLedgeGrabsActive(u8 state);

static const SOHModApi* sApi;

static bool sIsMapOpen;
static s8 sCursorSoil;
static s8 sWarpDestination = -1;
static MinishWarpPhase sWarpPhase;

static MinishSizePhase sSizePhase;
static f32 sPlayerScale = PLAYER_SCALE_NORMAL;
static bool sIsUsePending;
static s16 sCrawlspaceTimer;
static s16 sLastScene = -1;
static u32 sLastFrames;

// Link's own properties with a Minish-sized wall radius; the vanilla ones are only resolved when the player is
// created, so the mod swaps the pointer on him and hands it back when it is done.
static PlayerAgeProperties sTinyAgeProperties;
static PlayerAgeProperties* sVanillaAgeProperties;
static s16 sVanillaCylinderRadius;
static u8 sVanillaLedgeGrabState;

static void PlaySfx(u16 sfxId) {
    Audio_PlaySoundGeneral(sfxId, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static bool IsTiny(void) {
    return sSizePhase != MINISH_SIZE_NORMAL;
}

static s32 GetLanguage(void) {
    s32 language = gSaveContext.language;

    if (language < 0 || language >= MINISH_LANGUAGE_COUNT) {
        return 0;
    }
    return language;
}

static bool IsSoilUnlocked(s32 index) {
    const PodSoil* soil = &sPodSoils[index];

    if (soil->gsMask == 0) {
        return true;
    }
    return (GET_GS_FLAGS(soil->gsGroup) & soil->gsMask) != 0;
}

static bool IsPlayerAtSoil(Player* player, PlayState* play) {
    for (s32 i = 0; i < POD_SOIL_COUNT; i++) {
        const PodSoil* soil = &sPodSoils[i];
        Vec3f soilPos;

        if (soil->sceneId != play->sceneNum || !IsSoilUnlocked(i)) {
            continue;
        }
        soilPos = soil->pos;
        if (Math3D_Vec3fDistSq(&player->actor.world.pos, &soilPos) <= SQ(POD_SOIL_REACH)) {
            return true;
        }
    }
    return false;
}

// ── Minish size ──────────────────────────────────────────────────────────────

static void ApplyTinyProperties(Player* player) {
    sVanillaAgeProperties = player->ageProperties;
    sTinyAgeProperties = *player->ageProperties;
    sTinyAgeProperties.wallCheckRadius *= MINISH_SIZE_FACTOR;
    player->ageProperties = &sTinyAgeProperties;

    sVanillaCylinderRadius = player->cylinder.dim.radius;
    sVanillaLedgeGrabState = GameInteractor_GetDisableLedgeGrabsActive();
    // The vault thresholds are world units: at a tenth of his size every step is a ledge to climb.
    GameInteractor_SetDisableLedgeGrabsActive(1);
}

static void RestoreTinyProperties(Player* player) {
    if (player != NULL && player->ageProperties == &sTinyAgeProperties) {
        player->ageProperties = sVanillaAgeProperties;
        player->cylinder.dim.radius = sVanillaCylinderRadius;
        Collider_UpdateCylinder(&player->actor, &player->cylinder);
    }
    GameInteractor_SetDisableLedgeGrabsActive(sVanillaLedgeGrabState);
    sCrawlspaceTimer = 0;
}

static void EndMinishSize(Player* player) {
    RestoreTinyProperties(player);
    sSizePhase = MINISH_SIZE_NORMAL;
}

static void StartShrinking(Player* player) {
    ApplyTinyProperties(player);
    sSizePhase = MINISH_SIZE_SHRINKING;
    PlaySfx(NA_SE_SY_CAMERA_ZOOM_UP);
}

// Growing back inside a crawlspace would bury normal-sized Link in the rock above him.
static void StartGrowing(Player* player, PlayState* play) {
    Vec3f checkPos = player->actor.world.pos;
    CollisionPoly* ceilingPoly;
    s32 ceilingBgId;
    f32 ceilingY;

    checkPos.y += 2.0f;
    if (BgCheck_EntityCheckCeiling(&play->colCtx, &ceilingY, &checkPos, sVanillaAgeProperties->ceilingCheckHeight,
                                   &ceilingPoly, &ceilingBgId, &player->actor)) {
        PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    sSizePhase = MINISH_SIZE_GROWING;
    PlaySfx(NA_SE_SY_CAMERA_ZOOM_DOWN);
}

// The tunnel mouth is a wall the movement check never sees; what does see it is the interact-wall probe, the
// same one vanilla reads for its "Enter on A" prompt. Detection stops once the checks are off, so the window
// stays open while a normal-Link-height ceiling is still overhead.
static void UpdateCrawlspaceWindow(Player* player, PlayState* play) {
    Vec3f checkPos = player->actor.world.pos;
    CollisionPoly* ceilingPoly;
    s32 ceilingBgId;
    f32 ceilingY;

    if (sSizePhase != MINISH_SIZE_TINY) {
        sCrawlspaceTimer = 0;
        return;
    }
    if ((player->actor.bgCheckFlags & 0x200) &&
        (func_80041DB8(&play->colCtx, player->actor.wallPoly, player->actor.wallBgId) & 0x30)) {
        sCrawlspaceTimer = CRAWLSPACE_LINGER;
        return;
    }
    if (sCrawlspaceTimer <= 0) {
        return;
    }
    checkPos.y += 2.0f;
    if (BgCheck_EntityCheckCeiling(&play->colCtx, &ceilingY, &checkPos, sVanillaAgeProperties->ceilingCheckHeight,
                                   &ceilingPoly, &ceilingBgId, &player->actor)) {
        sCrawlspaceTimer = CRAWLSPACE_LINGER;
        return;
    }
    sCrawlspaceTimer--;
}

// The per-frame code recomputes the cylinder height from the skeleton but never its radius, so Minish Link
// would keep a hitbox ten times his body.
static void ApplyTinyBody(Player* player) {
    player->cylinder.dim.radius = MINISH_BODY_EXTENT;
    player->cylinder.dim.height = MINISH_BODY_EXTENT;
    Collider_UpdateCylinder(&player->actor, &player->cylinder);
}

static void UpdateMinishSize(Player* player, PlayState* play) {
    if (sSizePhase == MINISH_SIZE_SHRINKING) {
        sPlayerScale -= SCALE_STEP;
        if (sPlayerScale <= MINISH_SCALE) {
            sPlayerScale = MINISH_SCALE;
            sSizePhase = MINISH_SIZE_TINY;
        }
    } else if (sSizePhase == MINISH_SIZE_GROWING) {
        sPlayerScale += SCALE_STEP;
        if (sPlayerScale >= PLAYER_SCALE_NORMAL) {
            sPlayerScale = PLAYER_SCALE_NORMAL;
            EndMinishSize(player);
        }
    }

    Actor_SetScale(&player->actor, sPlayerScale);
    if (IsTiny()) {
        ApplyTinyBody(player);
        UpdateCrawlspaceWindow(player, play);
    }
}

// ── Warp ─────────────────────────────────────────────────────────────────────

// Farore's Wind's own landing: respawnFlag 3 makes the arriving scene read respawn[RESPAWN_MODE_TOP].
static void DepartToSoil(PlayState* play, s8 destination) {
    const PodSoil* soil = &sPodSoils[destination];

    play->nextEntranceIndex = soil->entranceIndex;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_FAST;

    gSaveContext.respawn[RESPAWN_MODE_TOP].entranceIndex = soil->entranceIndex;
    gSaveContext.respawn[RESPAWN_MODE_TOP].pos = soil->pos;
    gSaveContext.respawn[RESPAWN_MODE_TOP].yaw = soil->rotY;
    gSaveContext.respawn[RESPAWN_MODE_TOP].playerParams = 0xDFF;
    gSaveContext.respawn[RESPAWN_MODE_TOP].roomIndex = soil->roomIndex;
    gSaveContext.respawnFlag = 3;
}

static void UpdateWarp(Player* player, PlayState* play) {
    if (sWarpPhase == MINISH_WARP_DEPARTING) {
        sPlayerScale -= SCALE_STEP;
        if (sPlayerScale > WARP_SCALE) {
            Actor_SetScale(&player->actor, sPlayerScale);
            return;
        }
        sPlayerScale = WARP_SCALE;
        Actor_SetScale(&player->actor, sPlayerScale);
        sWarpPhase = MINISH_WARP_ARRIVING;
        DepartToSoil(play, sWarpDestination);
        sWarpDestination = -1;
        return;
    }

    sPlayerScale += SCALE_STEP;
    if (sPlayerScale >= PLAYER_SCALE_NORMAL) {
        sPlayerScale = PLAYER_SCALE_NORMAL;
        sWarpPhase = MINISH_WARP_IDLE;
    }
    Actor_SetScale(&player->actor, sPlayerScale);
}

// ── Warp map ─────────────────────────────────────────────────────────────────

static void CloseMap(void) {
    sIsMapOpen = false;
    sApi->ReleasePlayerInput(MINISH_CAP_OWNER);
    sApi->ReleaseTimeControl(MINISH_CAP_OWNER);
}

static void OpenMap(PlayState* play) {
    sCursorSoil = 0;
    for (s32 i = 0; i < POD_SOIL_COUNT; i++) {
        if (sPodSoils[i].sceneId == play->sceneNum && IsSoilUnlocked(i)) {
            sCursorSoil = i;
            break;
        }
    }
    sIsMapOpen = true;
    sApi->RequestTimeControl(MINISH_CAP_OWNER, MINISH_CAP_TIME_PRIORITY, 0.0f, true);
    sApi->BlockPlayerInput(MINISH_CAP_OWNER, 0xFFFF, true);
    PlaySfx(NA_SE_SY_WIN_OPEN);
}

// The soil whose direction on the map best matches where the stick points, nearest first.
static s8 FindSoilTowardStick(s8 from, s16 stickX, s16 stickY) {
    f32 stickAngle = atan2f((f32)stickX, (f32)stickY);
    s8 best = -1;
    f32 bestScore = -1.0f;

    for (s32 i = 0; i < POD_SOIL_COUNT; i++) {
        f32 deltaX = sPodSoils[i].mapX - sPodSoils[from].mapX;
        f32 deltaY = sPodSoils[i].mapY - sPodSoils[from].mapY;
        f32 distance = sqrtf(SQ(deltaX) + SQ(deltaY));
        f32 angleDifference;
        f32 score;

        if (i == from || distance < 1.0f) {
            continue;
        }
        angleDifference = atan2f(deltaX, deltaY) - stickAngle;
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
        score = cosf(angleDifference) / distance;
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

static void UpdateMap(PlayState* play) {
    static bool sWasStickHeld;
    Input* input = &play->state.input[0];
    f32 stickMagnitude = sqrtf(SQ((f32)input->rel.stick_x) + SQ((f32)input->rel.stick_y));

    if (stickMagnitude > 30.0f) {
        if (!sWasStickHeld) {
            s8 next = FindSoilTowardStick(sCursorSoil, input->rel.stick_x, input->rel.stick_y);

            if (next >= 0) {
                sCursorSoil = next;
                PlaySfx(NA_SE_SY_CURSOR);
            }
        }
        sWasStickHeld = true;
    } else {
        sWasStickHeld = false;
    }

    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        if (!IsSoilUnlocked(sCursorSoil)) {
            PlaySfx(NA_SE_SY_ERROR);
            return;
        }
        sWarpDestination = sCursorSoil;
        sWarpPhase = MINISH_WARP_DEPARTING;
        CloseMap();
        PlaySfx(NA_SE_SY_DECIDE);
        return;
    }

    if (CHECK_BTN_ALL(input->press.button, BTN_B) || CHECK_BTN_ALL(input->press.button, BTN_START)) {
        CloseMap();
        PlaySfx(NA_SE_SY_CANCEL);
    }
}

// ── Warp map drawing ─────────────────────────────────────────────────────────

// The map page of the pause screen, in its own coordinates: the frame spans 240x160 around the origin and the
// world map image sits inside it. Screen is 320x240 in the 10.2 fixed point the RDP takes.
#define MAP_ORIGIN_Y 96
#define MAP_X(x) (640 + (s32)(x)*24 / 5)
#define MAP_Y(y) (480 - MAP_ORIGIN_Y - (s32)(y)*24 / 5)
#define MAP_BOX_HALF_WIDTH 12
#define MAP_BOX_HALF_HEIGHT 8

static const char* sFrameTexs[MINISH_LANGUAGE_COUNT][15] = {
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

static const char* sCurrentPositionTexs[MINISH_LANGUAGE_COUNT] = {
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

static void SolidRect(PlayState* play, s32 x1, s32 y1, s32 x2, s32 y2, u8 r, u8 g, u8 b, u8 a) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, r, g, b, a);
    gSPWideTextureRectangle(OVERLAY_DISP++, x1, y1, x2, y2, G_TX_RENDERTILE, 0, 0, 0, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawFrame(PlayState* play, s32 language) {
    static const s16 sColumnX[] = { -120, -40, 40, 120 };
    static const s16 sRowY[] = { 80, 48, 16, -16, -48, -80 };
    static const u8 sColumnR[] = { 110, 140, 110 };
    static const u8 sColumnG[] = { 50, 60, 50 };
    static const u8 sColumnB[] = { 45, 60, 45 };

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    for (s16 column = 0; column < 3; column++) {
        s32 left = MAP_X(sColumnX[column]);
        s32 right = MAP_X(sColumnX[column + 1]);

        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, sColumnR[column], sColumnG[column], sColumnB[column], 255);
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
        s32 left;
        s32 top;
        s32 right;
        s32 bottom;

        if (gSaveContext.worldMapAreaData & gBitFlags[sCloudFlags[i]]) {
            continue;
        }
        left = MAP_X(sCloudX[i]);
        top = MAP_Y(sCloudY[i]);
        right = MAP_X(sCloudX[i] + sCloudWidths[i]);
        bottom = MAP_Y(sCloudY[i] - sCloudHeights[i]);

        gDPLoadTextureBlock_4b(OVERLAY_DISP++, sCloudTexs[i], G_IM_FMT_I, sCloudWidths[i], sCloudHeights[i], 0,
                               G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK,
                               G_TX_NOLOD, G_TX_NOLOD);
        gSPWideTextureRectangle(OVERLAY_DISP++, left, top, right, bottom, G_TX_RENDERTILE, 0, 0,
                                sCloudWidths[i] * 4096 / (right - left), sCloudHeights[i] * 4096 / (bottom - top));
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawSoilBoxes(PlayState* play, s16 pulse) {
    SetupSolidRects(play);
    for (s16 i = 0; i < POD_SOIL_COUNT; i++) {
        s32 left = MAP_X(sPodSoils[i].mapX - MAP_BOX_HALF_WIDTH);
        s32 top = MAP_Y(sPodSoils[i].mapY + MAP_BOX_HALF_HEIGHT);
        s32 right = MAP_X(sPodSoils[i].mapX + MAP_BOX_HALF_WIDTH);
        s32 bottom = MAP_Y(sPodSoils[i].mapY - MAP_BOX_HALF_HEIGHT);
        u8 r = 100;
        u8 g = 255;
        u8 b = 255;

        if (i == sCursorSoil) {
            g = (u8)(155 + pulse);
            b = (u8)(155 + pulse);
        } else if (!IsSoilUnlocked(i)) {
            g = 100;
            b = 100;
        }
        SolidRect(play, left, top, right, top + (2 << 2), r, g, b, 255);
        SolidRect(play, left, bottom - (2 << 2), right, bottom, r, g, b, 255);
        SolidRect(play, left, top, left + (2 << 2), bottom, r, g, b, 255);
        SolidRect(play, right - (2 << 2), top, right, bottom, r, g, b, 255);
    }
}

static void DrawCursorIcon(PlayState* play) {
    s32 left = MAP_X(sPodSoils[sCursorSoil].mapX - 8);
    s32 top = MAP_Y(sPodSoils[sCursorSoil].mapY + 8);
    s32 right = MAP_X(sPodSoils[sCursorSoil].mapX + 8);
    s32 bottom = MAP_Y(sPodSoils[sCursorSoil].mapY - 8);

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetTextureFilter(OVERLAY_DISP++, G_TF_BILERP);
    gDPSetTextureLUT(OVERLAY_DISP++, G_TT_NONE);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);
    gDPLoadTextureBlock(OVERLAY_DISP++, sPecoriTex, G_IM_FMT_RGBA, G_IM_SIZ_32b, 16, 16, 0, G_TX_WRAP | G_TX_NOMIRROR,
                        G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, left, top, right, bottom, G_TX_RENDERTILE, 0, 0, 16 * 4096 / (right - left),
                            16 * 4096 / (bottom - top));
    gDPPipeSync(OVERLAY_DISP++);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Where the pause screen writes the area you are standing in: the title, then the name on the parchment.
static void DrawSoilName(PlayState* play, s32 language) {
    bool isUnlocked = IsSoilUnlocked(sCursorSoil);
    s32 left;
    s32 top;
    s32 right;
    s32 bottom;

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetTextureFilter(OVERLAY_DISP++, G_TF_BILERP);
    gDPSetTextureLUT(OVERLAY_DISP++, G_TT_NONE);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    gDPSetCombineLERP(OVERLAY_DISP++, 1, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, 1, 0, PRIMITIVE, 0, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, 255);

    left = MAP_X(20);
    top = MAP_Y(-26);
    right = MAP_X(84);
    bottom = MAP_Y(-34);
    gDPLoadTextureBlock_4b(OVERLAY_DISP++, sCurrentPositionTexs[language], G_IM_FMT_I, 64, 8, 0,
                           G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                           G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, left, top, right, bottom, G_TX_RENDERTILE, 0, 0, 64 * 4096 / (right - left),
                            8 * 4096 / (bottom - top));
    gDPPipeSync(OVERLAY_DISP++);

    if (!isUnlocked) {
        left = MAP_X(5);
        top = MAP_Y(-38);
        right = MAP_X(19);
        bottom = MAP_Y(-54);
        gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 200, 200, 200, 255);
        gDPLoadTextureBlock(OVERLAY_DISP++, gQuestIconGoldSkulltulaTex, G_IM_FMT_RGBA, G_IM_SIZ_32b, 24, 24, 0,
                            G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                            G_TX_NOLOD);
        gSPWideTextureRectangle(OVERLAY_DISP++, left, top, right, bottom, G_TX_RENDERTILE, 0, 0,
                                24 * 4096 / (right - left), 24 * 4096 / (bottom - top));
        gDPPipeSync(OVERLAY_DISP++);
    }

    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    if (isUnlocked) {
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 150, 255, 255, 255);
    } else {
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 120, 120, 120, 255);
    }
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);

    left = MAP_X(19);
    top = MAP_Y(-36);
    right = MAP_X(99);
    bottom = MAP_Y(-68);
    gDPLoadTextureBlock(OVERLAY_DISP++, sPodSoils[sCursorSoil].nameTex[language], G_IM_FMT_IA, G_IM_SIZ_8b, 80, 32, 0,
                        G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, left, top, right, bottom, G_TX_RENDERTILE, 0, 0, 80 * 4096 / (right - left),
                            32 * 4096 / (bottom - top));
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawMap(PlayState* play) {
    static s16 sPulseTimer;
    s32 language = GetLanguage();
    s16 pulse;

    if (!sIsMapOpen || play == NULL) {
        return;
    }
    sPulseTimer++;
    pulse = (sPulseTimer % 40 < 20) ? sPulseTimer % 40 : 40 - sPulseTimer % 40;

    SetupSolidRects(play);
    SolidRect(play, 0, 0, SCREEN_WIDTH << 2, SCREEN_HEIGHT << 2, 0, 0, 0, 64);
    DrawFrame(play, language);
    DrawWorldMap(play);
    DrawClouds(play);
    DrawSoilBoxes(play, (s16)(pulse * 5));
    if ((sPulseTimer % 16) < 11) {
        DrawCursorIcon(play);
    }
    DrawSoilName(play, language);
}

// ── Hooks ────────────────────────────────────────────────────────────────────

static void ResolveMotionScale(Player* player, s32 kind, f32* scale) {
    if (sSizePhase != MINISH_SIZE_TINY) {
        return;
    }
    if (kind == SOH_PLAYER_MOTION_STICK_SPEED) {
        *scale *= MINISH_SPEED_FACTOR;
        return;
    }
    // The cap the engine measured against the wall he faces: at Minish size he is next to one everywhere.
    *scale = R_RUN_SPEED_LIMIT / 100.0f;
}

static void ResolveBgCheckFlags(Actor* actor, s32* flags) {
    if (sCrawlspaceTimer <= 0) {
        return;
    }
    *flags &= ~(1 | 2);
}

static void ResolveCameraView(Camera* camera, Vec3f* eye, Vec3f* at, Vec3f* up) {
    Player* player;
    f32 rigScale;

    if (camera == NULL || camera->thisIdx != MAIN_CAM || !IsTiny()) {
        return;
    }
    player = camera->player;
    if (player == NULL) {
        return;
    }
    // The whole rig shrinks toward Link with him, so the zoom eases in and out with the animation. The floor
    // keeps the eye outside the near plane.
    rigScale = player->actor.scale.x / PLAYER_SCALE_NORMAL;
    if (rigScale >= 0.999f) {
        return;
    }
    rigScale = CLAMP_MIN(rigScale, MINISH_CAMERA_FLOOR);

    eye->x = player->actor.world.pos.x + (eye->x - player->actor.world.pos.x) * rigScale;
    eye->y = player->actor.world.pos.y + (eye->y - player->actor.world.pos.y) * rigScale;
    eye->z = player->actor.world.pos.z + (eye->z - player->actor.world.pos.z) * rigScale;
    at->x = player->actor.world.pos.x + (at->x - player->actor.world.pos.x) * rigScale;
    at->y = player->actor.world.pos.y + (at->y - player->actor.world.pos.y) * rigScale;
    at->z = player->actor.world.pos.z + (at->z - player->actor.world.pos.z) * rigScale;
}

static void BlockCrawling(bool* should, va_list args) {
    if (IsTiny()) {
        *should = false;
    }
}

static void BlockLegPlanting(bool* should, va_list args) {
    if (IsTiny()) {
        *should = false;
    }
}

static void BlockPauseMenu(bool* should, va_list args) {
    if (sIsMapOpen || sWarpPhase != MINISH_WARP_IDLE) {
        *should = false;
    }
}

static bool CanUseCap(Player* player, PlayState* play) {
    if (sIsMapOpen || sWarpPhase != MINISH_WARP_IDLE) {
        return false;
    }
    if (play->pauseCtx.state != 0 || play->pauseCtx.debugState != 0) {
        return false;
    }
    if (play->transitionTrigger != TRANS_TRIGGER_OFF || play->gameOverCtx.state != GAMEOVER_INACTIVE) {
        return false;
    }
    return true;
}

static void UseCap(PlayState* play, Player* player) {
    sIsUsePending = true;
}

// A press resolves here, at the end of the player's update, so nothing the rest of the frame chooses runs over
// the map taking the input or over the size change.
static void StartPendingUse(Player* player, PlayState* play) {
    if (!sIsUsePending) {
        return;
    }
    sIsUsePending = false;
    if (!CanUseCap(player, play)) {
        return;
    }

    if (sSizePhase == MINISH_SIZE_TINY) {
        StartGrowing(player, play);
        return;
    }
    if (sSizePhase != MINISH_SIZE_NORMAL) {
        return;
    }
    if (IsPlayerAtSoil(player, play)) {
        OpenMap(play);
        return;
    }
    // A form owns Link's scale; two owners would fight over it every frame.
    if (sApi->GetActiveForm() != NULL) {
        PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    StartShrinking(player);
}

static void ResetOnSceneLoad(Player* player, PlayState* play) {
    if (sIsMapOpen) {
        CloseMap();
    }
    if (IsTiny()) {
        EndMinishSize(player);
    }
    // The arrival grows Link back on the far side of the transition it asked for.
    if (sWarpPhase == MINISH_WARP_ARRIVING) {
        sPlayerScale = WARP_SCALE;
        return;
    }
    sWarpPhase = MINISH_WARP_IDLE;
    sPlayerScale = PLAYER_SCALE_NORMAL;
    sIsUsePending = false;
}

static void UpdateMinishCap(void) {
    PlayState* play = gPlayState;
    Player* player = play == NULL ? NULL : GET_PLAYER(play);
    bool isSceneLoaded;

    if (play == NULL || player == NULL) {
        return;
    }
    isSceneLoaded = (play->sceneNum != sLastScene) || (play->state.frames < sLastFrames);
    sLastScene = play->sceneNum;
    sLastFrames = play->state.frames;
    if (isSceneLoaded) {
        ResetOnSceneLoad(player, play);
    }

    if (sWarpPhase != MINISH_WARP_IDLE) {
        UpdateWarp(player, play);
        return;
    }
    if (sIsMapOpen) {
        UpdateMap(play);
        return;
    }
    StartPendingUse(player, play);
    if (IsTiny()) {
        UpdateMinishSize(player, play);
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.5f, 0.5f, 0.5f, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sCapDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOHCustomItemDefinition cap = Z64Items_Define(MINISH_CAP_KEY, sIconTex, sNameTex);

    if (!SOH_MOD_API_HAS(sApi, GetActiveForm)) {
        return;
    }
    Z64Items_SetAge(&cap, SOH_CUSTOM_ITEM_AGE_CHILD);
    Z64Items_SetButtons(&cap, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&cap, 0, 18, 0);
    Z64Items_SetTextbox(&cap, "You got %rThe Minish Cap%w!&Ezlo, who was a sage before he was&a hat, and is in a "
                              "mood about it.^Press %y\xA1%w by a %ypod soil%w to open&his map and travel to another "
                              "one.&A %yGold Skulltula%w guards each soil.^Anywhere else, %y\xA1%w makes you "
                              "%yMinish%w:&small and slow, but small enough for&a crawlspace. Press again, or load "
                              "a&scene, to grow back.");
    Z64Items_SetPauseText(&cap, "%rThe Minish Cap&%wPress %y\xA1%w by a pod soil for the warp&map. Away from one, "
                                "shrink or grow.");
    Z64Items_SetCanUse(&cap, CanUseCap);
    Z64Items_SetAction(&cap, UseCap, NULL);
    cap.flags |= SOH_CUSTOM_ITEM_INSTANT;
    cap.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &cap)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateMinishCap);
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawMap);
    SOH_REGISTER_HOOK(sApi, OnCameraResolveView, ResolveCameraView);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, ResolveMotionScale);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorResolveBgCheckFlags, ACTOR_PLAYER, ResolveBgCheckFlags);
    sApi->RegisterVB(VB_CRAWL, BlockCrawling);
    sApi->RegisterVB(VB_PLAYER_ADJUST_LEGS_TO_FLOOR, BlockLegPlanting);
    sApi->RegisterVB(VB_OPEN_PAUSE_MENU, BlockPauseMenu);
}
