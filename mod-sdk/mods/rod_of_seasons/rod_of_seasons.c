#include <stdio.h>

#include "soh/ResourceManagerHelpers.h"
#include "z64items.h"
#include "z64wheel.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Bg_Mizu_Water/z_bg_mizu_water.h"
#include "overlays/actors/ovl_Bg_Spot06_Objects/z_bg_spot06_objects.h"
#include "overlays/actors/ovl_Obj_Makekinsuta/z_obj_makekinsuta.h"
#include "overlays/actors/ovl_Obj_Bean/z_obj_bean.h"
#include "objects/object_mamenoki/object_mamenoki.h"
#include "objects/object_spot07_object/object_spot07_object.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"

#include "season_water_dls.inc"

#define SEASONS_KEY "nei.rod_of_seasons"
// The pose NEI dialled in and baked: the staff is 96 units tall, so the scale is the share of Link's own
// height it takes up.
#define SEASONS_HELD_SCALE 0.4f
#define SEASONS_HELD_ROT_Y 72.414f
#define SEASONS_HELD_ROT_X 109.655f
#define SEASONS_HELD_OFFSET_Y 5.977f
#define SEASONS_HELD_OFFSET_Z -3.218f
#define SEASONS_GIVE_SCALE 0.4f
#define SEASONS_RAIN_DROPS 30
#define SEASONS_SNOWFLAKES 64
#define SEASONS_BLOSSOM 32
#define SEASONS_KANKYO_FAIRIES 0
#define SEASONS_KANKYO_SNOW 3
#define SEASONS_SPARKLE_COUNT 12
#define SEASONS_SPARKLE_SPREAD 35.0f
#define SEASONS_WATER_DROP 600.0f
#define SEASONS_WATER_RISE 100.0f
#define SEASONS_WATER_GONE -32000
#define SEASONS_MAX_WATERBOXES 32
#define SEASONS_LAKE_FULL_LEVEL 0.0f
#define SEASONS_LAKE_DRY_LEVEL -681.0f
#define SEASONS_LAKE_BASIN_DROP -1313.0f
#define SEASONS_LAKE_RAISED -1313
#define SEASONS_LAKE_LOWERED -1993
#define SEASONS_LAKE_RIVER_RAISED -1113
#define SEASONS_LAKE_RIVER_LOWERED -1193
#define SEASONS_LAKE_RIVER_ZMIN 2203
#define SEASONS_LAKE_WATER_PLANE 2
#define SEASONS_LAKE_ICE_BLOCK 3
#define SEASONS_HAZARD_PRIORITY 100
#define SEASONS_WEATHER_TAG_SANDSTORM 6
// How far below his feet the frozen surface may sit and still count as the floor he is standing on.
#define SEASONS_ICE_FOOTING 2.0f
#define SEASONS_BGCHECK_WATER 0x60

typedef enum {
    SEASON_OFF,
    SEASON_SPRING,
    SEASON_SUMMER,
    SEASON_AUTUMN,
    SEASON_WINTER,
    SEASON_COUNT,
} Season;

typedef struct {
    s16 light[3];
    s16 ambient[3];
    u8 sparkle[3];
    u16 chime;
} SeasonEntry;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsTex";
static const ALIGN_ASSET(2) char sSpringIcon[] = "__OTR__textures/icon_item_custom/gItemIconSeasonSpringTex";
static const ALIGN_ASSET(2) char sSummerIcon[] = "__OTR__textures/icon_item_custom/gItemIconSeasonSummerTex";
static const ALIGN_ASSET(2) char sAutumnIcon[] = "__OTR__textures/icon_item_custom/gItemIconSeasonAutumnTex";
static const ALIGN_ASSET(2) char sWinterIcon[] = "__OTR__textures/icon_item_custom/gItemIconSeasonWinterTex";

// The rod itself never changes on the pause screen or its button: the season rides along as a coin in the corner.
static const ALIGN_ASSET(2) char sSpringRod[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsSpringTex";
static const ALIGN_ASSET(2) char sSummerRod[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsSummerTex";
static const ALIGN_ASSET(2) char sAutumnRod[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsAutumnTex";
static const ALIGN_ASSET(2) char sWinterRod[] = "__OTR__textures/icon_item_custom/gItemIconRodOfSeasonsWinterTex";

// Offsets onto whatever light the scene already has, so they shade every room without knowing anything
// about it. Winter pulls red down hardest and barely touches blue, which reads as cold rather than dark.
static const SeasonEntry sSeasons[SEASON_COUNT] = {
    { { 0, 0, 0 }, { 0, 0, 0 }, { 255, 255, 255 }, NA_SE_SY_TRE_BOX_APPEAR },
    { { 0, 0, 0 }, { 0, 0, 0 }, { 190, 245, 200 }, NA_SE_EV_CHICKEN_CRY_A },
    { { 18, 14, -6 }, { 16, 12, -8 }, { 255, 230, 140 }, NA_SE_EV_FLAME_IGNITION },
    { { -34, -32, -26 }, { -42, -40, -34 }, { 235, 170, 90 }, NA_SE_EV_RAIN },
    { { -46, -34, -14 }, { -54, -42, -20 }, { 190, 220, 255 }, NA_SE_EV_ICE_FREEZE },
};

static const Z64WheelEntry sWheelEntries[SEASON_COUNT] = {
    { sIconTex, "No Season", sIconTex },   { sSpringIcon, "Spring", sSpringRod }, { sSummerIcon, "Summer", sSummerRod },
    { sAutumnIcon, "Autumn", sAutumnRod }, { sWinterIcon, "Winter", sWinterRod },
};

static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gRodOfSeasonsNameTex";
static const ALIGN_ASSET(2) char sRodDL[] = "__OTR__objects/object_nei_rod_of_seasons/gNeiRodOfSeasonsDL";

static const char* const sRequiredHooks[] = { "OnPlayerUpdate",
                                              "OnActorDrawEnd",
                                              "OnActorInit",
                                              "OnBgCheckRaycastFloor",
                                              "OnSceneInit",
                                              "OnPlayerPostLimbDraw",
                                              "OnPlayDrawEnd",
                                              "OnResolveEnvHazard",
                                              "OnPlayerResolveLimbDraw",
                                              "OnActorResolveBgCheckFlags",
                                              "OnActorDraw",
                                              "OnPlayerResolveItemActionInit",
                                              "OnInterfaceDrawEnd",
                                              "OnLoadFile" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

// One item and one slot, found four times over: the seed spreads the copies, and each one wakes a season.
static const SOHCustomItemRandomizer sRodLogic = {
    sizeof(SOHCustomItemRandomizer),
    SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT | SOH_CUSTOM_ITEM_RANDO_PROGRESSIVE,
    SOH_CUSTOM_ITEM_TYPE_ITEM,
    0,
    SEASON_COUNT - 1,
    SEASONS_KEY,
    SEASONS_KEY,
    "",
};

static const SOHModApi* sApi;
static bool sIsRodDrawn;
static bool sIsTurningSeason;
static bool sOwnsPrecipitation;
static bool sOwnsRain;
static bool sOwnsSky;
static u8 sWokenSeason;
static u8 sAppliedSeason;
static MtxF sHandMatrix;
static bool sHasHandMatrix;

void Player_PlayVoiceSfx(Player* player, u16 sfxId);
void BgMizuWater_SetWaterBoxesHeight(WaterBox* waterBoxes, s32 height);

static void ReloadSceneInPlace(Player* player, PlayState* play);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// Which seasons Link carries, and which one the world is wearing, both live in the wheel, and the wheel
// lives in the save file: one file's spring is not another's.
static u8 CurrentSeason(void) {
    u32 selection = Z64Wheel_GetSelection();

    return selection < SEASON_COUNT ? (u8)selection : SEASON_OFF;
}

static bool IsSeasonOwned(u8 season) {
    return season != SEASON_OFF && Z64Wheel_IsEntryUnlocked(season);
}

// Snow and blossom are drawn by Object_Kankyo, never by the engine: setting the count with no such actor in
// the scene falls dry. Spawning a second one is the vanilla "Let It Snow" idiom, its init kills the copy.
static void TakePrecipitation(PlayState* play, u8 count, s16 kankyoType) {
    play->envCtx.unk_EE[3] = count;
    sOwnsPrecipitation = true;
    if (play->envCtx.unk_EE[2] == 0) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_OBJECT_KANKYO, 0, 0, 0, 0, 0, 0, kankyoType);
    }
}

// The scene's own weather writes these same fields, so the rod may only undo what the rod set.
static void ReleasePrecipitation(PlayState* play) {
    if (!sOwnsPrecipitation) {
        return;
    }
    play->envCtx.unk_EE[3] = 0;
    if (play->envCtx.unk_EE[2] == 0) {
        sOwnsPrecipitation = false;
    }
}

static void TakeRain(EnvironmentContext* env, u8 drops) {
    env->unk_EE[0] = drops;
    sOwnsRain = true;
}

static void ReleaseRain(EnvironmentContext* env) {
    if (!sOwnsRain) {
        return;
    }
    env->unk_EE[0] = 0;
    sOwnsRain = false;
}

// gloomySkyMode is a two-step handshake: 1 darkens and 2 hands it to the restore pass, which clears it.
static void TakeSky(EnvironmentContext* env) {
    env->gloomySkyMode = 1;
    sOwnsSky = true;
}

static void ReleaseSky(EnvironmentContext* env) {
    if (!sOwnsSky) {
        return;
    }
    if (env->gloomySkyMode == 1) {
        env->gloomySkyMode = 2;
    }
    sOwnsSky = false;
}

static void ShadeWorld(EnvironmentContext* env, u8 season) {
    const SeasonEntry* entry = &sSeasons[season];

    for (u32 i = 0; i < 3; i++) {
        env->adjLight1Color[i] = entry->light[i];
        env->adjAmbientColor[i] = entry->ambient[i];
    }
}

static void ClearWeather(PlayState* play) {
    ReleaseRain(&play->envCtx);
    ReleasePrecipitation(play);
    ReleaseSky(&play->envCtx);
    ShadeWorld(&play->envCtx, SEASON_OFF);
}

// Environment_Update rebuilds all of this every pass, which is also what carries the season through a scene
// load: every write here is idempotent, so it simply happens again.
static void ApplySeason(PlayState* play) {
    EnvironmentContext* env = &play->envCtx;
    u8 season = CurrentSeason();

    if (!IsSeasonOwned(season) || env->indoors) {
        ClearWeather(play);
        return;
    }
    ShadeWorld(env, season);
    switch (season) {
        case SEASON_SPRING:
            ReleaseRain(env);
            ReleaseSky(env);
            TakePrecipitation(play, SEASONS_BLOSSOM, SEASONS_KANKYO_FAIRIES);
            break;
        case SEASON_SUMMER:
            ReleaseRain(env);
            ReleaseSky(env);
            ReleasePrecipitation(play);
            break;
        case SEASON_AUTUMN:
            // Particles on screen suppress the engine's whole rain pass, so the flakes drain out first.
            ReleasePrecipitation(play);
            TakeRain(env, SEASONS_RAIN_DROPS);
            TakeSky(env);
            Sfx_PlaySfxCentered(NA_SE_EV_RAIN - SFX_FLAG);
            // The flag the Song of Storms raises: a bean sprout watches it before shooting up.
            Flags_SetEnv(play, 5);
            break;
        case SEASON_WINTER:
            ReleaseRain(env);
            TakePrecipitation(play, SEASONS_SNOWFLAKES, SEASONS_KANKYO_SNOW);
            TakeSky(env);
            break;
        default:
            ClearWeather(play);
            break;
    }
}

// Summer drains every basin. The scene's own header is shared cached memory, so the surfaces are put back
// the moment the season lets go of them, and re-applied each frame because the actors that own their water
// rewrite the same fields.
static WaterBox* sDriedHeader;
static s32 sDriedSurfaces[SEASONS_MAX_WATERBOXES];
static u16 sDriedCount;

static void RestoreWaterBoxes(void) {
    if (sDriedHeader == NULL) {
        return;
    }
    for (u16 i = 0; i < sDriedCount; i++) {
        sDriedHeader[i].ySurface = sDriedSurfaces[i];
    }
    sDriedHeader = NULL;
    sDriedCount = 0;
}

static void DryWaterBoxes(PlayState* play) {
    CollisionHeader* header = play->colCtx.colHeader;

    if (header == NULL || header->waterBoxes == NULL || header->numWaterBoxes == 0) {
        return;
    }
    if (sDriedHeader != header->waterBoxes) {
        RestoreWaterBoxes();
        sDriedCount = MIN(header->numWaterBoxes, SEASONS_MAX_WATERBOXES);
        for (u16 i = 0; i < sDriedCount; i++) {
            sDriedSurfaces[i] = header->waterBoxes[i].ySurface;
        }
        sDriedHeader = header->waterBoxes;
    }
    for (u16 i = 0; i < sDriedCount; i++) {
        sDriedHeader[i].ySurface = SEASONS_WATER_GONE;
    }
}

typedef enum {
    WATER_LOOK_VANILLA,
    WATER_LOOK_ICE,
    WATER_LOOK_GONE,
} WaterLook;

static const ALIGN_ASSET(2) char sIceTex[] = "__OTR__objects/object_spot07_object/object_spot07_object_Tex_005530";
static u8 sWaterLook;

// Water takes its alpha from prim, not from the texel, so a transparent texture comes out as a black sheet:
// summer drops the triangles instead, and winter swaps in Zora's Domain's own ice.
static void SetWaterLook(u8 look) {
    char tag[32];

    if (sWaterLook == look) {
        return;
    }
    for (u32 row = 0; row < ARRAY_COUNT(sSeasonWaterDLs); row++) {
        const SeasonWaterDL* water = &sSeasonWaterDLs[row];

        snprintf(tag, sizeof(tag), "seasonWaterTex_%d", water->textureInstruction);
        ResourceMgr_UnpatchGfxByName(water->dlPath, tag);
        snprintf(tag, sizeof(tag), "seasonWaterHash_%d", water->textureInstruction);
        ResourceMgr_UnpatchGfxByName(water->dlPath, tag);
        for (s32 triangle = 0; triangle < water->triangleCount; triangle++) {
            snprintf(tag, sizeof(tag), "seasonWaterTri_%d", water->triangles[triangle]);
            ResourceMgr_UnpatchGfxByName(water->dlPath, tag);
        }
        if (look == WATER_LOOK_ICE) {
            Gfx image = gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, sIceTex);
            Gfx skip = gsSPNoOp();

            snprintf(tag, sizeof(tag), "seasonWaterTex_%d", water->textureInstruction);
            ResourceMgr_PatchGfxByName(water->dlPath, tag, water->textureInstruction, image);
            // The original is a 128-bit SETTIMG_OTR_HASH: its second word holds the texture's CRC64, which
            // would run as an opcode once the first word is a plain SETTIMG.
            snprintf(tag, sizeof(tag), "seasonWaterHash_%d", water->textureInstruction);
            ResourceMgr_PatchGfxByName(water->dlPath, tag, water->textureInstruction + 1, skip);
        }
        if (look == WATER_LOOK_GONE) {
            Gfx skip = gsSPNoOp();

            for (s32 triangle = 0; triangle < water->triangleCount; triangle++) {
                snprintf(tag, sizeof(tag), "seasonWaterTri_%d", water->triangles[triangle]);
                ResourceMgr_PatchGfxByName(water->dlPath, tag, water->triangles[triangle], skip);
            }
        }
    }
    sWaterLook = look;
}

static u8 WaterLookFor(u8 season) {
    if (!IsSeasonOwned(season)) {
        return WATER_LOOK_VANILLA;
    }
    if (season == SEASON_WINTER) {
        return WATER_LOOK_ICE;
    }
    if (season == SEASON_SUMMER) {
        return WATER_LOOK_GONE;
    }
    return WATER_LOOK_VANILLA;
}

// Crossing the Haunted Wasteland costs a tunic once a season rules it: the Zora one against the summer heat,
// the Goron one against the winter cold. Winter also puts out every hot room there is.
static void ClaimHazard(PlayState* play, u8 season) {
    if (!IsSeasonOwned(season)) {
        sApi->ReleaseHazard(SEASONS_KEY);
        return;
    }
    if (play->sceneNum == SCENE_HAUNTED_WASTELAND && (season == SEASON_SUMMER || season == SEASON_WINTER)) {
        sApi->RequestHazard(SEASONS_KEY, SEASONS_HAZARD_PRIORITY, PLAYER_ENV_HAZARD_HOTROOM,
                            season == SEASON_SUMMER ? EQUIP_VALUE_TUNIC_ZORA : EQUIP_VALUE_TUNIC_GORON);
        return;
    }
    sApi->ReleaseHazard(SEASONS_KEY);
}

// Winter puts out the hot rooms, and only those: an underwater hazard is not heat, and waiving every one of
// them would hand Link endless air. The desert is left alone, because there winter IS the hazard.
static void ThawHotRooms(PlayState* play, s16* hazard) {
    if (*hazard != PLAYER_ENV_HAZARD_HOTROOM || CurrentSeason() != SEASON_WINTER || !IsSeasonOwned(SEASON_WINTER) ||
        play->sceneNum == SCENE_HAUNTED_WASTELAND) {
        return;
    }
    *hazard = PLAYER_ENV_HAZARD_NONE;
}

// Floor property 12 is the sand that swallows Link and sends him back to the entrance, so the ground
// without it IS the path. Read from the collision, never from a hand-drawn route.
#define SEASONS_SAND_SWALLOWS FUNC_80041EA4_VOID_OUT
// A poly steep enough to be a wall is not floor to walk on. Normals are s16, 0x7FFF being straight up.
#define SEASONS_FLOOR_NORMAL 0x4000
#define SEASONS_GROUND_BATCH 10
#define SEASONS_GROUND_MAX_VTX 4096
#define SEASONS_GROUND_MAX_GFX 1024
// The storm is gone in these seasons, so the wash over the safe path only has to hint. What must not be
// missed is the sand that swallows, so it carries the weight.
#define SEASONS_SAFE_ALPHA 26
#define SEASONS_SWALLOWS_ALPHA 64
#define SEASONS_LIGHTEN(c) ((u8)(((c) + 255 * 2) / 3))

// Neither is in libultraship's gbi; colViewer.cpp defines its own pair for the same job.
#define SEASONS_CC_PRIMITIVE_ENVA 0, 0, 0, PRIMITIVE, 0, 0, 0, ENVIRONMENT
#define SEASONS_DEF_VTX(x, y, z)                                                  \
    {                                                                             \
        .n = {.ob = { x, y, z }, .tc = { 0, 0 }, .n = { 0, 0x7F, 0 }, .a = 0xFF } \
    }

typedef enum {
    GROUND_SAFE,
    GROUND_SWALLOWS,
    GROUND_KIND_MAX,
} GroundKind;

// The emblem colour of each season's coin on the staff.
static const u8 sSeasonColor[SEASON_COUNT][3] = {
    { 40, 36, 48 }, { 6, 235, 64 }, { 235, 5, 7 }, { 235, 166, 6 }, { 4, 105, 235 },
};

static Vtx sGroundVtx[SEASONS_GROUND_MAX_VTX];
static u32 sGroundVtxCount;
static Gfx sGroundDL[GROUND_KIND_MAX][SEASONS_GROUND_MAX_GFX];
static CollisionHeader* sGroundHeader;

// Does the desert charge a tunic right now? Everything the crossing gives back hangs off this.
static bool ClearsWasteland(PlayState* play) {
    u8 season = CurrentSeason();

    if (play->sceneNum != SCENE_HAUNTED_WASTELAND || !IsSeasonOwned(season)) {
        return false;
    }
    return season == SEASON_SUMMER || season == SEASON_WINTER;
}

// -1 for anything too steep to walk on, which has no business being painted either way.
static s32 GroundKindOf(PlayState* play, CollisionPoly* poly) {
    if (poly->normal.y < SEASONS_FLOOR_NORMAL) {
        return -1;
    }
    if (func_80041EA4(&play->colCtx, poly, BGCHECK_SCENE) == SEASONS_SAND_SWALLOWS) {
        return GROUND_SWALLOWS;
    }
    return GROUND_SAFE;
}

// Decal, so it lies on the sand instead of hovering over it — which is also what keeps it from z-fighting
// the dunes. LoadGeometryMode rather than Set: it drops culling and lighting in one go, so the colour comes
// from the combiner alone and the paint reads from both sides.
static u32 AppendGroundSetup(Gfx* dl) {
    u32 count = 0;

    dl[count++] = (Gfx)gsSPTexture(0, 0, 0, G_TX_RENDERTILE, G_OFF);
    dl[count++] = (Gfx)gsDPSetCycleType(G_CYC_1CYCLE);
    dl[count++] = (Gfx)gsDPSetRenderMode(
        Z_CMP | IM_RD | CVG_DST_FULL | FORCE_BL | ZMODE_DEC | GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA),
        Z_CMP | IM_RD | CVG_DST_FULL | FORCE_BL | ZMODE_DEC | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA));
    dl[count++] = (Gfx)gsDPSetCombineMode(SEASONS_CC_PRIMITIVE_ENVA, SEASONS_CC_PRIMITIVE_ENVA);
    dl[count++] = (Gfx)gsSPLoadGeometryMode(G_ZBUFFER);
    dl[count++] = (Gfx)gsSPMatrix(&gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    return count;
}

// Ten triangles per load: the vertex cache holds 32, and one command per triangle would be some eight
// hundred a frame on ground this size.
static void AppendGroundTriangles(Gfx* dl, u32 count, u32 from, u32 to) {
    const u32 perLoad = SEASONS_GROUND_BATCH * 3;

    for (u32 v = from; v < to && count + 2 + (perLoad / 6) < SEASONS_GROUND_MAX_GFX; v += perLoad) {
        u32 batch = MIN(perLoad, to - v);

        dl[count++] = (Gfx)gsSPVertex(&sGroundVtx[v], (s32)batch, 0);
        for (u32 t = 0; t + 6 <= batch; t += 6) {
            dl[count++] = (Gfx)gsSP2Triangles((s32)t, (s32)t + 1, (s32)t + 2, 0, (s32)t + 3, (s32)t + 4, (s32)t + 5, 0);
        }
        if ((batch % 6) != 0) {
            dl[count++] = (Gfx)gsSP1Triangle((s32)batch - 3, (s32)batch - 2, (s32)batch - 1, 0);
        }
    }
    dl[count] = (Gfx)gsSPEndDisplayList();
}

// Bakes both kinds of ground into a display list each, kept until the scene's collision changes. The
// commands hold pointers into sGroundVtx, so every vertex is written before the first command is.
static void BuildGround(PlayState* play, CollisionHeader* col) {
    u32 start[GROUND_KIND_MAX];

    sGroundVtxCount = 0;
    sGroundHeader = col;
    for (s32 kind = 0; kind < GROUND_KIND_MAX; kind++) {
        start[kind] = sGroundVtxCount;
        for (u16 i = 0; i < col->numPolygons && sGroundVtxCount + 3 <= SEASONS_GROUND_MAX_VTX; i++) {
            CollisionPoly* poly = &col->polyList[i];

            if (GroundKindOf(play, poly) != kind) {
                continue;
            }
            for (s32 v = 0; v < 3; v++) {
                Vec3i* corner = &col->vtxList[COLPOLY_VTX_INDEX(poly->vtxData[v])];
                Vtx vertex = SEASONS_DEF_VTX(corner->x, corner->y, corner->z);

                sGroundVtx[sGroundVtxCount++] = vertex;
            }
        }
    }
    for (s32 kind = 0; kind < GROUND_KIND_MAX; kind++) {
        u32 end = kind + 1 < GROUND_KIND_MAX ? start[kind + 1] : sGroundVtxCount;

        AppendGroundTriangles(sGroundDL[kind], AppendGroundSetup(sGroundDL[kind]), start[kind], end);
    }
}

// Washed out where Link may walk, the season's own colour where the sand takes him. The alpha rides the
// env colour, not prim: that is what the combiner above reads it from.
static void PaintGround(void) {
    PlayState* play = gPlayState;
    const u8* color;

    if (play == NULL || !ClearsWasteland(play) || play->colCtx.colHeader == NULL) {
        return;
    }
    if (play->colCtx.colHeader != sGroundHeader) {
        BuildGround(play, play->colCtx.colHeader);
    }
    if (sGroundVtxCount == 0) {
        return;
    }
    color = sSeasonColor[CurrentSeason()];
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, SEASONS_LIGHTEN(color[0]), SEASONS_LIGHTEN(color[1]),
                    SEASONS_LIGHTEN(color[2]), 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 0xFF, 0xFF, 0xFF, SEASONS_SAFE_ALPHA);
    gSPDisplayList(POLY_XLU_DISP++, sGroundDL[GROUND_SAFE]);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, color[0], color[1], color[2], 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 0xFF, 0xFF, 0xFF, SEASONS_SWALLOWS_ALPHA);
    gSPDisplayList(POLY_XLU_DISP++, sGroundDL[GROUND_SWALLOWS]);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The sandstorm is also the transition wipe, so it is only silenced once the wipe is over.
static void CalmWasteland(PlayState* play, u8 season) {
    if (!IsSeasonOwned(season) || play->sceneNum != SCENE_HAUNTED_WASTELAND || season == SEASON_SPRING ||
        season == SEASON_AUTUMN || play->transitionMode != TRANS_MODE_OFF) {
        return;
    }
    play->envCtx.sandstormState = SANDSTORM_OFF;
    play->envCtx.sandstormPrimA = 0;
    play->envCtx.sandstormEnvA = 0;
}

static void SpawnSeasonSparkles(Player* player, PlayState* play, u8 season) {
    const SeasonEntry* entry = &sSeasons[season];
    Color_RGBA8 prim = { entry->sparkle[0], entry->sparkle[1], entry->sparkle[2], 255 };
    Color_RGBA8 env = { entry->sparkle[0] / 2, entry->sparkle[1] / 2, entry->sparkle[2] / 2, 255 };
    Vec3f accel = { 0.0f, -0.2f, 0.0f };

    for (u32 i = 0; i < SEASONS_SPARKLE_COUNT; i++) {
        Vec3f pos = { player->actor.world.pos.x + Rand_CenteredFloat(SEASONS_SPARKLE_SPREAD),
                      player->actor.world.pos.y + 20.0f + Rand_ZeroFloat(50.0f),
                      player->actor.world.pos.z + Rand_CenteredFloat(SEASONS_SPARKLE_SPREAD) };
        Vec3f velocity = { Rand_CenteredFloat(2.0f), 1.5f, Rand_CenteredFloat(2.0f) };

        EffectSsKiraKira_SpawnFocused(play, &pos, &velocity, &accel, &prim, &env, 400, 20);
    }
}

// Locked seasons are not offered: the button turns through what Link carries, plus the blank setting that
// hands the weather back to the scene. The wheel itself only ever shows the unlocked ones.
static u8 NextAvailableSeason(u8 from) {
    for (u8 step = 1; step <= SEASON_COUNT; step++) {
        u8 candidate = (from + step) % SEASON_COUNT;

        if (candidate == SEASON_OFF || IsSeasonOwned(candidate)) {
            return candidate;
        }
    }
    return SEASON_OFF;
}

static void TurnWorldToSeason(Player* player, PlayState* play, u8 season) {
    if (player == NULL) {
        return;
    }
    RestoreWaterBoxes();
    ClearWeather(play);
    ApplySeason(play);
    SpawnSeasonSparkles(player, play, season);
    PlaySfxAt(sSeasons[season].chime, &player->actor.world.pos);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_MAGIC_ATTACK);
    sAppliedSeason = season;
    sIsTurningSeason = true;
    ReloadSceneInPlace(player, play);
}

// The wheel is the picker NEI uses for this rod: it stops the world and waits for an answer, so the season
// is chosen rather than cycled past. Whoever writes the selection, the world follows it a frame later.
static void TurnSeason(Player* player, PlayState* play) {
    if (!Z64Wheel_Open()) {
        Z64Wheel_SetSelection(NextAvailableSeason(CurrentSeason()));
    }
}

// A scene that has just loaded already stands in the season the save holds, reload or not.
static void SyncSeasonOnSceneInit(int16_t sceneNum) {
    sAppliedSeason = CurrentSeason();
}

// Winter freezes every surface: the floor raycast answers with the water's own surface, so Link stands on it
// instead of sinking. Only for Link, only while he is above the water and not already swimming in it.
static bool IsStandingOnIce(PlayState* play, Player* player) {
    f32 surface;
    WaterBox* waterBox;

    if (play == NULL || player == NULL || CurrentSeason() != SEASON_WINTER || !IsSeasonOwned(SEASON_WINTER)) {
        return false;
    }
    if ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) || player->actor.velocity.y > 0.0f) {
        return false;
    }
    if (!WaterBox_GetSurface1(play, &play->colCtx, player->actor.world.pos.x, player->actor.world.pos.z, &surface,
                              &waterBox)) {
        return false;
    }
    return player->actor.world.pos.y >= surface - SEASONS_ICE_FOOTING;
}

static void FreezeWaterUnderfoot(CollisionContext* colCtx, Vec3f* pos, Actor* actor, CollisionPoly** poly,
                                 int32_t* bgId, float* floorY) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);
    f32 surface;
    WaterBox* waterBox;

    if (play == NULL || actor != &player->actor || CurrentSeason() != SEASON_WINTER || !IsSeasonOwned(SEASON_WINTER)) {
        return;
    }
    if ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) || player->actor.velocity.y > 0.0f) {
        return;
    }
    if (!WaterBox_GetSurface1(play, colCtx, pos->x, pos->z, &surface, &waterBox) || surface <= *floorY) {
        return;
    }
    *floorY = surface;
}

// Bit 0x40 of the scene-check mask is the engine's own "no ripples this frame": walking on ice is walking on
// a solid, so the water it stands on never notices.
static void SkipRipplesOnIce(Actor* actor, int32_t* flags) {
    PlayState* play = gPlayState;

    if (play == NULL || actor != &GET_PLAYER(play)->actor) {
        return;
    }
    if (IsStandingOnIce(play, GET_PLAYER(play))) {
        *flags |= 0x40;
    }
}

// Everything an actor decides about the season it decides in its init, so turning the year reloads the room
// around Link: the respawn-down path is what keeps him exactly where he stands.
static void ReloadSceneInPlace(Player* player, PlayState* play) {
    gSaveContext.respawnFlag = 1;
    play->nextEntranceIndex = gSaveContext.entranceIndex;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = play->nextEntranceIndex;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = play->roomCtx.curRoom.num;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].pos = player->actor.world.pos;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].yaw = player->actor.shape.rot.y;
    if (play->roomCtx.curRoom.behaviorType2 < 4) {
        gSaveContext.respawn[RESPAWN_MODE_DOWN].playerParams = 0x0DFF;
    } else {
        // Rooms on a static background ride a fixed camera that has to come back with them.
        gSaveContext.respawn[RESPAWN_MODE_DOWN].playerParams = 0x0D00 | GET_ACTIVE_CAM(play)->camDataIdx;
    }
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_INSTANT;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_FAST;
}

// That respawn path is the pit-fall one, and vanilla charges its damage for the fall: not for a season.
static void WaiveVoidDamage(bool* should, va_list args) {
    if (!sIsTurningSeason) {
        return;
    }
    sIsTurningSeason = false;
    *should = false;
}

static void TakeOutRod(PlayState* play, Player* player) {
    sIsRodDrawn = true;
    PlaySfxAt(NA_SE_PL_CHANGE_ARMS, &player->actor.world.pos);
}

static void StowRod(Player* player, PlayState* play) {
    sIsRodDrawn = false;
    PlaySfxAt(NA_SE_PL_CHANGE_ARMS, &player->actor.world.pos);
}

// The staff is gripped, not palmed: the same closed fist the Hookshot puts on that hand.
static void CloseRodHand(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!sIsRodDrawn || limbIndex != PLAYER_LIMB_R_HAND) {
        return;
    }
    *dList = (Gfx*)(LINK_IS_ADULT ? gLinkAdultRightHandClosedNearDL : gLinkChildRightHandClosedNearDL);
}

static u8 CountLockedSeasons(void) {
    u8 locked = 0;

    for (u8 season = SEASON_SPRING; season < SEASON_COUNT; season++) {
        locked += IsSeasonOwned(season) ? 0 : 1;
    }
    return locked;
}

// Whichever one turns up: each pickup hands over a season Link is missing, so the four of them add up to
// the whole year however the seed spreads them out.
static u8 PickMissingSeason(void) {
    u8 locked = CountLockedSeasons();

    if (locked == 0) {
        return SEASON_OFF;
    }
    u8 wanted = (u8)(Rand_ZeroOne() * locked);

    for (u8 season = SEASON_SPRING; season < SEASON_COUNT; season++) {
        if (IsSeasonOwned(season)) {
            continue;
        }
        if (wanted == 0) {
            return season;
        }
        wanted--;
    }
    return SEASON_OFF;
}

#define DEKU_FLOWER_KEY "nei.deku_flower"
#define DEKU_FLOWER_LIMB_MAX 11
#define DEKU_FLOWER_REST_SCALE 0.01f
#define DEKU_FLOWER_WOBBLE_STEPS 18
#define DEKU_FLOWER_RUSTLE_FRAMES 10
#define DEKU_FLOWER_BOUNCE_FRAMES 30

typedef struct DekuFlower DekuFlower;
typedef void (*DekuFlowerActionFunc)(DekuFlower*, PlayState*);

struct DekuFlower {
    DynaPolyActor dyna;
    SkelAnime skelAnime;
    ColliderCylinder collider;
    Vec3s jointTable[DEKU_FLOWER_LIMB_MAX];
    Vec3s morphTable[DEKU_FLOWER_LIMB_MAX];
    f32 bounceScale;
    s16 wobbleTimer;
    u8 playerOnTop;
    DekuFlowerActionFunc actionFunc;
};

static const ALIGN_ASSET(2) char sFlowerSkel[] = "__OTR__objects/gameplay_keep/gGoldDekuFlowerSkel";
static const ALIGN_ASSET(2) char sFlowerIdleDL[] = "__OTR__objects/gameplay_keep/gGoldDekuFlowerIdleDL";
static const ALIGN_ASSET(2) char sFlowerCol[] = "__OTR__objects/gameplay_keep/gGoldDekuFlowerCol";
static const ALIGN_ASSET(2) char sFlowerBounceAnim[] = "__OTR__objects/gameplay_keep/gDekuFlowerBounceAnim";
static const ALIGN_ASSET(2) char sFlowerRustleAnim[] = "__OTR__objects/gameplay_keep/gDekuFlowerRustleAnim";

static ColliderCylinderInit sFlowerCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0x00000000, 0x00, 0x00 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_NONE | TOUCH_SFX_NORMAL,
      BUMP_ON,
      OCELEM_NONE },
    { 20, 20, 0, { 0, 0, 0 } },
};

// Scales the wobble of every interaction, walked one step per frame.
static const f32 sFlowerWobble[DEKU_FLOWER_WOBBLE_STEPS] = {
    -1.0f, -1.0f, -1.0f, -0.7f, 0.0f, 0.7f, 1.0f, 0.7f, 0.0f, -0.7f, -1.0f, -0.7f, 0.0f, 0.7f, 1.0f, 0.7f, 0.0f, -0.7f,
};

static s16 sDekuFlowerId = -1;
static bool sIsDekuFlowerChecked;

SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);

static void FlowerIdle(DekuFlower* self, PlayState* play);
static void FlowerRustle(DekuFlower* self, PlayState* play);
static void FlowerBounce(DekuFlower* self, PlayState* play);
static void DrawFlowerIdle(Actor* thisx, PlayState* play);
static void DrawFlowerAnimated(Actor* thisx, PlayState* play);

static void RestFlower(DekuFlower* self) {
    Actor_SetScale(&self->dyna.actor, DEKU_FLOWER_REST_SCALE);
    self->dyna.actor.scale.y = 2.0f * DEKU_FLOWER_REST_SCALE;
}

static void SettleFlower(DekuFlower* self) {
    self->dyna.actor.draw = DrawFlowerIdle;
    self->actionFunc = FlowerIdle;
    RestFlower(self);
}

static void StartFlowerRustle(DekuFlower* self) {
    Animation_Change(&self->skelAnime, (AnimationHeader*)sFlowerRustleAnim, 1.0f, 0.0f,
                     Animation_GetLastFrame((void*)sFlowerRustleAnim), ANIMMODE_ONCE, 0.0f);
    self->dyna.actor.draw = DrawFlowerAnimated;
    self->actionFunc = FlowerRustle;
    self->wobbleTimer = DEKU_FLOWER_RUSTLE_FRAMES;
}

static void StartFlowerBounce(DekuFlower* self, f32 bounceScale) {
    Animation_Change(&self->skelAnime, (AnimationHeader*)sFlowerBounceAnim, 1.0f, 0.0f,
                     Animation_GetLastFrame((void*)sFlowerBounceAnim), ANIMMODE_ONCE, 0.0f);
    self->dyna.actor.draw = DrawFlowerAnimated;
    self->actionFunc = FlowerBounce;
    self->wobbleTimer = DEKU_FLOWER_BOUNCE_FRAMES;
    self->bounceScale = bounceScale;
}

static void InitDekuFlower(Actor* thisx, PlayState* play) {
    DekuFlower* self = (DekuFlower*)thisx;
    CollisionHeader* colHeader = NULL;
    SkeletonHeader* skeleton = ResourceMgr_LoadSkeletonByName(sFlowerSkel, NULL);

    // Destroy runs even on an actor killed from its init, so both handles exist before the bail-out.
    DynaPolyActor_Init(&self->dyna, DPM_PLAYER);
    Collider_InitCylinder(play, &self->collider);
    if (skeleton == NULL) {
        Actor_Kill(thisx);
        return;
    }
    Collider_SetCylinder(play, &self->collider, thisx, &sFlowerCylinderInit);
    Collider_UpdateCylinder(thisx, &self->collider);
    CollisionHeader_GetVirtual((void*)sFlowerCol, &colHeader);
    self->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);
    SkelAnime_Init(play, &self->skelAnime, skeleton, (AnimationHeader*)sFlowerBounceAnim, self->jointTable,
                   self->morphTable, DEKU_FLOWER_LIMB_MAX);
    thisx->focus.pos.y = thisx->home.pos.y + 10.0f;
    thisx->targetMode = 3;
    SettleFlower(self);
}

static void DestroyDekuFlower(Actor* thisx, PlayState* play) {
    DekuFlower* self = (DekuFlower*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, self->dyna.bgId);
    Collider_DestroyCylinder(play, &self->collider);
}

static void WobbleFlower(DekuFlower* self, PlayState* play) {
    if (self->wobbleTimer <= 0) {
        RestFlower(self);
        return;
    }

    f32 wobble = sFlowerWobble[play->gameplayFrames % DEKU_FLOWER_WOBBLE_STEPS] * (0.0001f * self->wobbleTimer);

    Actor_SetScale(&self->dyna.actor, DEKU_FLOWER_REST_SCALE + wobble);
    self->dyna.actor.scale.y = 2.0f * DEKU_FLOWER_REST_SCALE;
    self->wobbleTimer--;
}

static void FlowerIdle(DekuFlower* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    u8 onTop = DynaPolyActor_IsPlayerOnTop(&self->dyna);

    if (onTop != self->playerOnTop) {
        StartFlowerRustle(self);
    } else if (onTop && player->actor.speedXZ > 0.1f) {
        self->wobbleTimer = DEKU_FLOWER_RUSTLE_FRAMES;
    }
    self->playerOnTop = onTop;
    if (self->collider.base.acFlags & AC_HIT) {
        self->collider.base.acFlags &= ~AC_HIT;
        StartFlowerRustle(self);
    }
    WobbleFlower(self, play);
}

static void FlowerRustle(DekuFlower* self, PlayState* play) {
    self->playerOnTop = DynaPolyActor_IsPlayerOnTop(&self->dyna);
    if (SkelAnime_Update(&self->skelAnime)) {
        self->dyna.actor.draw = DrawFlowerIdle;
        self->actionFunc = FlowerIdle;
    }
    WobbleFlower(self, play);
}

// Stronger than the rustle, and on Y too: the flower that has just sprung open.
static void FlowerBounce(DekuFlower* self, PlayState* play) {
    self->playerOnTop = DynaPolyActor_IsPlayerOnTop(&self->dyna);
    SkelAnime_Update(&self->skelAnime);
    if (self->wobbleTimer <= 0) {
        self->bounceScale = 0.0f;
        SettleFlower(self);
        return;
    }
    self->wobbleTimer--;
    self->bounceScale *= 0.8f;
    self->bounceScale -= (self->dyna.actor.scale.x - DEKU_FLOWER_REST_SCALE) * 0.4f;

    f32 scale = self->dyna.actor.scale.x + self->bounceScale;

    Actor_SetScale(&self->dyna.actor, scale);
    self->dyna.actor.scale.y = 2.0f * scale;
}

static void UpdateDekuFlower(Actor* thisx, PlayState* play) {
    DekuFlower* self = (DekuFlower*)thisx;

    self->actionFunc(self, play);
    CollisionCheck_SetAC(play, &play->colChkCtx, &self->collider.base);
}

static void DrawFlowerIdle(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sFlowerIdleDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawFlowerAnimated(Actor* thisx, PlayState* play) {
    DekuFlower* self = (DekuFlower*)thisx;

    Gfx_SetupDL_37Opa(play->state.gfxCtx);
    SkelAnime_DrawOpa(play, self->skelAnime.skeleton, self->skelAnime.jointTable, NULL, NULL, thisx);
}

// The flower is Majora's Mask's, and most players have no mm.o2r to take it from: then it simply never grows.
static s16 DekuFlowerId(void) {
    if (sIsDekuFlowerChecked) {
        return sDekuFlowerId;
    }
    sIsDekuFlowerChecked = true;
    if (!SOH_MOD_API_HAS(sApi, HasResource) || !sApi->HasResource(sFlowerSkel)) {
        return sDekuFlowerId;
    }

    SOHActorDefinition flower = { sizeof(SOHActorDefinition),
                                  DEKU_FLOWER_KEY,
                                  "Majora's Mask Deku Flower",
                                  ACTORCAT_BG,
                                  0,
                                  OBJECT_GAMEPLAY_KEEP,
                                  sizeof(DekuFlower),
                                  InitDekuFlower,
                                  DestroyDekuFlower,
                                  UpdateDekuFlower,
                                  NULL };

    sDekuFlowerId = sApi->RegisterActor(&flower);
    return sDekuFlowerId;
}

// The two state bits the bean keeps its collision in; the actor declares them in its own .c.
#define BEAN_STATE_COLLIDER_SET (1 << 4)
#define BEAN_STATE_DYNAPOLY_SET (1 << 5)

// The ten bean spots, with the soft soil params their child setup uses.
typedef struct {
    s16 scene;
    s8 room;
    u16 soilParams;
} BeanSite;

static const BeanSite sBeanSites[] = {
    { SCENE_GRAVEYARD, 1, 0x5101 },
    { SCENE_KOKIRI_FOREST, 0, 0x4D01 },
    { SCENE_LAKE_HYLIA, 0, 0x5301 },
    { SCENE_GERUDO_VALLEY, 0, 0x5401 },
    { SCENE_LOST_WOODS, 5, 0x4E01 },
    { SCENE_LOST_WOODS, 6, 0x4E02 },
    { SCENE_DESERT_COLOSSUS, 0, 0x5601 },
    { SCENE_DEATH_MOUNTAIN_TRAIL, 0, 0x5002 },
    { SCENE_DEATH_MOUNTAIN_CRATER, 1, 0x5001 },
};

void ObjBean_SetupWaitForBean(ObjBean* bean);
void ObjBean_SetupWaitForWater(ObjBean* bean);
void ObjBean_SetupWaitForPlayer(ObjBean* bean);
void ObjBean_SetupPathCount(ObjBean* bean, PlayState* play);
void ObjBean_SetupPath(ObjBean* bean, PlayState* play);
void ObjBean_Move(ObjBean* bean);
void ObjBean_InitDynaPoly(ObjBean* bean, PlayState* play, CollisionHeader* collision, s32 moveFlag);
void ObjBean_InitCollider(Actor* actor, PlayState* play);
void ObjBean_FindFloor(ObjBean* bean, PlayState* play);
void ObjBean_Update(Actor* actor, PlayState* play);
void ObjBean_Draw(Actor* actor, PlayState* play);

// Summer opens the bean spot into a gold flower, the one a Deku Link burrows into.
static void PlantDekuFlower(Actor* bean, PlayState* play) {
    s16 flowerId = DekuFlowerId();

    if (flowerId < 0) {
        return;
    }
    Actor_Spawn(&play->actorCtx, play, flowerId, bean->world.pos.x, bean->world.pos.y, bean->world.pos.z, 0,
                bean->shape.rot.y, 0, 0);
    Actor_Kill(bean);
}

// The soft soil the gold skulltula sleeps under, which only the child setups place. Winter digs it up where
// the bean spot stands, so an adult has it too.
static u16 SoftSoilParams(PlayState* play) {
    for (u32 i = 0; i < ARRAY_COUNT(sBeanSites); i++) {
        if (sBeanSites[i].scene == play->sceneNum && sBeanSites[i].room == play->roomCtx.curRoom.num) {
            return sBeanSites[i].soilParams;
        }
    }
    return 0;
}

static bool HasFlightPath(ObjBean* bean, PlayState* play) {
    s32 path = (bean->dyna.actor.params >> 8) & 0x1F;

    return path != 0x1F && play->setupPathList != NULL && play->setupPathList[path].count >= 3;
}

// Vanilla's own adult branch, rebuilt: a bean that has already grown into the flying platform.
static void GrowBeanPlatform(ObjBean* bean, PlayState* play) {
    ObjBean_SetupPathCount(bean, play);
    ObjBean_SetupPath(bean, play);
    ObjBean_Move(bean);
    ObjBean_SetupWaitForPlayer(bean);
    if (!(bean->stateFlags & BEAN_STATE_DYNAPOLY_SET)) {
        ObjBean_InitDynaPoly(bean, play, (CollisionHeader*)gMagicBeanPlatformCol, DPM_UNK3);
        bean->stateFlags |= BEAN_STATE_DYNAPOLY_SET;
    }
    if (!(bean->stateFlags & BEAN_STATE_COLLIDER_SET)) {
        ObjBean_InitCollider(&bean->dyna.actor, play);
        bean->stateFlags |= BEAN_STATE_COLLIDER_SET;
    }
    ActorShape_Init(&bean->dyna.actor.shape, 0.0f, ActorShadow_DrawCircle, 8.8f);
    ObjBean_FindFloor(bean, play);
    bean->unk_1F6 = bean->dyna.actor.home.rot.z & 3;
}

// Which stage a bean spot stands in is the season's to say, not the save flag's: spring has already grown it,
// autumn is a sprout its own rain will raise, and the cold seasons leave bare soil.
static void SetBeanStage(void* refActor) {
    ObjBean* bean = (ObjBean*)refActor;
    PlayState* play = gPlayState;
    u8 season = CurrentSeason();

    if (play == NULL || !IsSeasonOwned(season)) {
        return;
    }
    if (season == SEASON_SUMMER) {
        PlantDekuFlower(&bean->dyna.actor, play);
        return;
    }
    // Vanilla kills an adult's unplanted bean outright, and Actor_Kill only clears these two.
    bean->dyna.actor.update = ObjBean_Update;
    bean->dyna.actor.draw = ObjBean_Draw;
    if (season == SEASON_SPRING && HasFlightPath(bean, play)) {
        GrowBeanPlatform(bean, play);
        return;
    }
    if (season == SEASON_AUTUMN) {
        ObjBean_SetupWaitForWater(bean);
        return;
    }
    ObjBean_SetupWaitForBean(bean);

    u16 soilParams = SoftSoilParams(play);

    if (season == SEASON_WINTER && LINK_IS_ADULT && soilParams != 0) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_OBJ_MAKEKINSUTA, bean->dyna.actor.world.pos.x,
                    bean->dyna.actor.world.pos.y, bean->dyna.actor.world.pos.z, 0, bean->dyna.actor.shape.rot.y, 0,
                    soilParams);
    }
}

// Summer thaws the Ice Cavern, and winter takes the fairy swarm's slot so its snow has somewhere to fall.
static void RuleOutActor(void* refActor) {
    Actor* actor = (Actor*)refActor;
    u8 season = CurrentSeason();

    if (!IsSeasonOwned(season)) {
        return;
    }
    if (actor->id == ACTOR_OBJECT_KANKYO) {
        if (season == SEASON_WINTER && actor->params == SEASONS_KANKYO_FAIRIES) {
            Actor_Kill(actor);
        }
        return;
    }
    if (season == SEASON_SUMMER) {
        Actor_Kill(actor);
    }
}

// The rod owns the sky while a season runs, and a weather tag would fight it for the same fields. The
// sandstorm tag is fog rather than weather, so it only goes in the seasons that clear the desert.
static void RuleOutWeatherTag(void* refActor) {
    Actor* tag = (Actor*)refActor;
    PlayState* play = gPlayState;
    bool isSandstorm = (tag->params & 0xF) == SEASONS_WEATHER_TAG_SANDSTORM;

    if (!IsSeasonOwned(CurrentSeason())) {
        return;
    }
    if (!isSandstorm || (play != NULL && ClearsWasteland(play))) {
        Actor_Kill(tag);
    }
}

// Water that moves with the scene rides the season: summer drops it out of reach, autumn brings it up.
static void LevelWaterActor(void* refActor) {
    Actor* actor = (Actor*)refActor;
    u8 season = CurrentSeason();
    f32 delta;

    if (!IsSeasonOwned(season)) {
        return;
    }
    if (season == SEASON_SUMMER) {
        delta = -SEASONS_WATER_DROP;
    } else if (season == SEASON_AUTUMN) {
        delta = SEASONS_WATER_RISE;
    } else {
        return;
    }
    actor->world.pos.y += delta;
    actor->home.pos.y += delta;
}

// Both levels are written, never just the dry one: the collision header is cached resource memory, so a
// lake left lowered by summer stays lowered into the next season.
static void LevelLakeHylia(void* refActor) {
    BgSpot06Objects* lake = (BgSpot06Objects*)refActor;
    PlayState* play = gPlayState;
    u8 season = CurrentSeason();
    bool isDry = season == SEASON_SUMMER;

    if (!IsSeasonOwned(season) || play == NULL) {
        return;
    }
    // The frozen slab is only standing while the lake is frozen over.
    if (lake->dyna.actor.params == SEASONS_LAKE_ICE_BLOCK) {
        if (season != SEASON_WINTER) {
            Actor_Kill(&lake->dyna.actor);
        }
        return;
    }
    if (lake->dyna.actor.params != SEASONS_LAKE_WATER_PLANE || play->colCtx.colHeader == NULL ||
        play->colCtx.colHeader->numWaterBoxes < 4) {
        return;
    }

    WaterBox* waterBoxes = play->colCtx.colHeader->waterBoxes;

    lake->lakeHyliaWaterLevel = isDry ? SEASONS_LAKE_DRY_LEVEL : SEASONS_LAKE_FULL_LEVEL;
    lake->dyna.actor.world.pos.y = lake->lakeHyliaWaterLevel + SEASONS_LAKE_BASIN_DROP;
    waterBoxes[2].ySurface = isDry ? SEASONS_LAKE_LOWERED : SEASONS_LAKE_RAISED;
    waterBoxes[3].ySurface = waterBoxes[2].ySurface;
    waterBoxes[1].ySurface = isDry ? SEASONS_LAKE_RIVER_LOWERED : SEASONS_LAKE_RIVER_RAISED;
    // Absolute, never the actor's own "-= 50": on cached collision that drifts 50 units per scene load.
    waterBoxes[1].zMin = isDry ? SEASONS_LAKE_RIVER_ZMIN - 50 : SEASONS_LAKE_RIVER_ZMIN;
}

static void LevelWaterTemple(void* refActor) {
    BgMizuWater* water = (BgMizuWater*)refActor;
    PlayState* play = gPlayState;
    u8 season = CurrentSeason();
    f32 level;

    // Only the main body: types 1-4 are the switchable walls and pillars, which keep their own heights.
    if (water->type != 0 || !IsSeasonOwned(season) || play == NULL || play->colCtx.colHeader == NULL) {
        return;
    }
    if (season == SEASON_SUMMER) {
        level = WATER_TEMPLE_WATER_F1_Y - WATER_TEMPLE_WATER_F3_Y;
    } else if (season == SEASON_AUTUMN) {
        level = 0.0f;
    } else {
        return;
    }
    water->actor.world.pos.y = water->actor.home.pos.y + level;
    water->targetY = water->actor.world.pos.y;
    BgMizuWater_SetWaterBoxesHeight(play->colCtx.colHeader->waterBoxes, (s32)water->actor.world.pos.y);
}

// Zora's Domain's waterfall already carries both states; vanilla picks between them by age, and the season
// speaks over it. Frozen it is a platform, thawed it is water Link walks through.
static bool IsWaterfallFrozen(void) {
    u8 season = CurrentSeason();

    if (!IsSeasonOwned(season)) {
        return LINK_IS_ADULT;
    }
    if (season == SEASON_WINTER) {
        return true;
    }
    if (season == SEASON_SUMMER) {
        return false;
    }
    return LINK_IS_ADULT;
}

static void SetWaterfallCollision(void* refActor) {
    DynaPolyActor* waterfall = (DynaPolyActor*)refActor;
    PlayState* play = gPlayState;
    CollisionHeader* colHeader = NULL;
    bool isFrozen = IsWaterfallFrozen();

    if (play == NULL || isFrozen == LINK_IS_ADULT) {
        return;
    }
    if (!isFrozen) {
        if (waterfall->bgId != BGACTOR_NEG_ONE) {
            DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, waterfall->bgId);
            waterfall->bgId = BGACTOR_NEG_ONE;
        }
        return;
    }
    CollisionHeader_GetVirtual(waterfall->actor.params == 0 ? (void*)object_spot07_object_Col_002590
                                                            : (void*)object_spot07_object_Col_0038FC,
                               &colHeader);
    waterfall->bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &waterfall->actor, colHeader);
}

// Only the two states vanilla would draw the other way round: the rest is left to its own draw.
static void DrawWaterfall(Actor* actor, PlayState* play, bool* drawVanilla) {
    u32 frames = play->gameplayFrames;
    bool isFrozen = IsWaterfallFrozen();

    if (isFrozen == LINK_IS_ADULT) {
        return;
    }
    *drawVanilla = false;
    OPEN_DISPS(play->state.gfxCtx);
    if (isFrozen) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++,
                       (Gfx*)(actor->params == 0 ? object_spot07_object_DL_001CF0 : object_spot07_object_DL_003210));
    }
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, ((frames * -1) & 0x7F), ((frames * 1) & 0x7F), 32,
                                             32, 1, ((frames * 1) & 0x7F), ((frames * 1) & 0x7F), 32, 32, -1, 1, 1, 1));
    if (isFrozen) {
        gSPDisplayList(POLY_XLU_DISP++,
                       (Gfx*)(actor->params == 0 ? object_spot07_object_DL_001F68 : object_spot07_object_DL_0032D8));
    } else if (actor->params == 0) {
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 128);
        gSPSegment(POLY_XLU_DISP++, 0x09,
                   (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, ((frames * -1) & 0x7F), ((frames * -3) & 0xFF),
                                                 64, 64, 1, ((frames * 1) & 0x7F), ((frames * -3) & 0xFF), 64, 64, -1,
                                                 -3, 1, -3));
        gSPSegment(POLY_XLU_DISP++, 0x0A,
                   (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, frames * 0, ((frames * 3) & 0x1FF), 32, 128, 1,
                                                 frames * 0, ((frames * 3) & 0x1FF), 32, 128, 0, 3, 0, 3));
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)object_spot07_object_DL_000460);
    } else {
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 128);
        gSPSegment(POLY_XLU_DISP++, 0x09,
                   (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, frames * 0, ((frames * -1) & 0x7F), 32, 32, 1,
                                                 frames * 0, ((frames * -1) & 0x7F), 32, 32, 0, -1, 0, -1));
        gSPSegment(POLY_XLU_DISP++, 0x0A,
                   (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, frames * 0, ((frames * 3) & 0x1FF), 32, 128, 1,
                                                 frames * 0, ((frames * 3) & 0x1FF), 32, 128, 0, 3, 0, 3));
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)object_spot07_object_DL_000BE0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// Winter brings the soft soil up: the latch is what tells it its bugs are already out.
static void RaiseSoftSoil(void* refActor) {
    if (CurrentSeason() == SEASON_WINTER && IsSeasonOwned(SEASON_WINTER)) {
        ((ObjMakekinsuta*)refActor)->unk_152 = 1;
    }
}

static void GrantSeason(void) {
    u8 season = PickMissingSeason();

    if (season == SEASON_OFF) {
        return;
    }
    Z64Wheel_SetEntryUnlocked(season, true);
    sWokenSeason = season;
}

// Every copy of the rod in the pool is this same item: what a copy hands over is a season, not a second rod.
static void WakeSeason(const char* key) {
    GrantSeason();
}

static const char* WakeMessage(u8 season) {
    switch (season) {
        case SEASON_SPRING:
            return "The rod wakes to %gSpring%w!&Flowers open, and what sleeps&through winter is awake.";
        case SEASON_SUMMER:
            return "The rod wakes to %rSummer%w!&The heat dries shallow water and&thaws what the cold held.";
        case SEASON_AUTUMN:
            return "The rod wakes to %yAutumn%w!&Rain falls, and the wind carries&leaves across the land.";
        case SEASON_WINTER:
            return "The rod wakes to %bWinter%w!&Snow falls, and water freezes hard&enough to walk on.";
        default:
            return "The rod wakes to a new season!";
    }
}

// The pickup has its own textbox running, so the season it woke waits until Link has the screen back.
static void AnnounceWokenSeason(PlayState* play) {
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (player == NULL || play->msgCtx.msgMode != MSGMODE_NONE ||
        (player->stateFlags1 & (PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_IN_ITEM_CS))) {
        return;
    }
    sApi->ShowTextbox(play, WakeMessage(sWokenSeason), false);
    sWokenSeason = SEASON_OFF;
}

// The season is world state, not a spell: it keeps running whether or not the rod sits on a button.
static void KeepSeasonRunning(void) {
    PlayState* play = gPlayState;
    u8 season;

    if (play == NULL || !sApi->IsCustomItemOwned(SEASONS_KEY)) {
        return;
    }
    // Carrying the rod means carrying at least one season, whichever way it arrived.
    if (CountLockedSeasons() == SEASON_COUNT - 1) {
        GrantSeason();
    }
    if (sWokenSeason != SEASON_OFF) {
        AnnounceWokenSeason(play);
    }
    season = CurrentSeason();
    if (season != sAppliedSeason && !Z64Wheel_IsOpen()) {
        TurnWorldToSeason(GET_PLAYER(gPlayState), play, season);
        return;
    }
    // Clearing it after the scene checks is what keeps the splash, the swim and the wading sfx off the ice.
    if (IsStandingOnIce(play, GET_PLAYER(play))) {
        GET_PLAYER(play)->actor.bgCheckFlags &= ~SEASONS_BGCHECK_WATER;
    }
    ApplySeason(play);
    CalmWasteland(play, season);
    ClaimHazard(play, season);
    SetWaterLook(WaterLookFor(season));
    if (IsSeasonOwned(season) && season == SEASON_SUMMER) {
        DryWaterBoxes(play);
    } else {
        RestoreWaterBoxes();
    }
}

static int32_t HoldRod(Player* player, PlayState* play) {
    return 0;
}

// Nothing touches the matrix between the hand limb and the end of the post-limb pass while the rod is out, so
// this is the hand bone's own matrix: the staff follows the arm instead of floating beside Link.
static void CaptureHandMatrix(PlayState* play, Player* player, int32_t limbIndex) {
    if (limbIndex != PLAYER_LIMB_R_HAND) {
        return;
    }
    Matrix_Get(&sHandMatrix);
    sHasHandMatrix = true;
}

static void DrawRodInHand(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    f32 unscale = player->actor.scale.x != 0.0f ? 1.0f / player->actor.scale.x : 1.0f;

    if (!sIsRodDrawn || !sHasHandMatrix || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Put(&sHandMatrix);
    // That matrix carries Link's own 0.01 body scale: dividing it back out leaves the offsets in world units.
    Matrix_Scale(unscale, unscale, unscale, MTXMODE_APPLY);
    Matrix_RotateY(DEG_TO_RAD(SEASONS_HELD_ROT_Y), MTXMODE_APPLY);
    Matrix_RotateX(DEG_TO_RAD(SEASONS_HELD_ROT_X), MTXMODE_APPLY);
    // The offsets come after the rotations, so each slides the staff along its OWN shaft.
    Matrix_Translate(0.0f, SEASONS_HELD_OFFSET_Y, SEASONS_HELD_OFFSET_Z, MTXMODE_APPLY);
    Matrix_Scale(SEASONS_HELD_SCALE, SEASONS_HELD_SCALE, SEASONS_HELD_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRodDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(SEASONS_GIVE_SCALE, SEASONS_GIVE_SCALE, SEASONS_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRodDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, StorageSet)) {
        return;
    }

    SOHCustomItemDefinition rod = Z64Items_Define(SEASONS_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&rod, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&rod, 0, 23, 0);
    Z64Items_SetTextbox(&rod, "You got the %rRod of Seasons%w!&A staff the old spirits answer to.^Press %y\xA1%w to "
                              "turn the world through&the seasons it has woken to, or hold&%y\xA2%w to pick one.^"
                              "It wakes to one more season each&time you find it again.");
    Z64Items_SetPauseText(&rod, "%rRod of Seasons&%wPress %y\xA1%w for the next season,&%y\xA2%w to pick one.");
    Z64Items_SetAction(&rod, TakeOutRod, HoldRod);
    Z64Items_SetHeldCallbacks(&rod, TurnSeason, StowRod, NULL);
    rod.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    rod.getItemEntry.drawFunc = DrawGetItem;
    rod.onReceive = WakeSeason;
    Z64Items_SetLogic(&rod, &sRodLogic);

    if (!Z64Items_Register(sApi, &rod)) {
        return;
    }
    Z64Wheel_Register(sApi, SEASONS_KEY, SEASONS_KEY, sWheelEntries, ARRAY_COUNT(sWheelEntries), true);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, KeepSeasonRunning);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, SyncSeasonOnSceneInit);
    SOH_REGISTER_HOOK(sApi, OnPlayDrawEnd, PaintGround);
    SOH_REGISTER_HOOK(sApi, OnResolveEnvHazard, ThawHotRooms);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, CloseRodHand);
    SOH_REGISTER_HOOK(sApi, OnActorResolveBgCheckFlags, SkipRipplesOnIce);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, CaptureHandMatrix);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawRodInHand);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_BG_ICE_SHELTER, RuleOutActor);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_BG_ICE_TURARA, RuleOutActor);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_OBJECT_KANKYO, RuleOutActor);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_EN_WEATHER_TAG, RuleOutWeatherTag);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_BG_SPOT06_OBJECTS, LevelLakeHylia);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_BG_MIZU_WATER, LevelWaterTemple);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_BG_SPOT01_IDOMIZU, LevelWaterActor);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_BG_MORI_IDOMIZU, LevelWaterActor);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_BG_HAKA_WATER, LevelWaterActor);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_OBJ_MAKEKINSUTA, RaiseSoftSoil);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_BG_SPOT07_TAKI, SetWaterfallCollision);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_BG_SPOT07_TAKI, DrawWaterfall);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_OBJ_BEAN, SetBeanStage);
    sApi->RegisterVB(VB_INFLICT_VOID_DAMAGE, WaiveVoidDamage);
    SOH_REGISTER_HOOK(sApi, OnBgCheckRaycastFloor, FreezeWaterUnderfoot);
}
