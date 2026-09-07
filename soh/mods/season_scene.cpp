/**
 * season_scene.cpp — what a season does to the world (Skijer's NEI).
 *
 * The Rod of Seasons is global, so this is a registry, not a function per place. Much of what a
 * season should change is already implemented as a second state of an existing actor — Lake Hylia's
 * water plane, the Water Temple's level — so an OnActorInit adapter pushes that actor into the
 * state the season wants. Adding a place means adding an adapter, nothing else.
 *
 * A whole-scene swap through gSaveContext.sceneLayer would be cheaper still, but a scene can only
 * be sent to a setup it was authored with: freezing and thawing Zora's Domain is not a layer swap,
 * because the frozen version is the adult setup and the thawed one the child setup.
 *
 * Save flags are NEVER written. There is no read hook for them (the hook table only exposes
 * OnFlagSet/OnFlagUnset), so faking a world state by flipping the real flag would corrupt
 * progression. Adapters touch the actor, never the save.
 *
 * Changing season reloads the scene in place (item_rod_of_seasons.c), which re-runs Play_Init and
 * every actor's Init — that is what makes all of this apply with no transition logic of its own.
 */

#include <libultraship/bridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/ActorDB.h"
#include <string>
#include <vector>
#include <algorithm>
#include "soh/ResourceManagerHelpers.h"
// Not a .h: mods/*.h is globbed with CONFIGURE_DEPENDS, and a new header there forces a full CMake
// regeneration on the next build.
#include "season_water_dls.inc"
// OPEN_DISPS re-declares these with C++ linkage unless they are seen as C first.
extern "C" {
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}
extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "mods/extended_inventory.h"
#include "overlays/actors/ovl_Bg_Spot06_Objects/z_bg_spot06_objects.h"
#include "overlays/actors/ovl_Bg_Mizu_Water/z_bg_mizu_water.h"
#include "overlays/actors/ovl_En_Weather_Tag/z_en_weather_tag.h"
#include "overlays/actors/ovl_Obj_Makekinsuta/z_obj_makekinsuta.h"
#include "objects/object_spot07_object/object_spot07_object.h" // Zora's Domain's frozen-water texture
// Unity-built here: the flower only exists for Summer's bean spots, and its ActorDB entry is C++.
#include "actors/deku_flower.c"

// External linkage in its own overlay.
void BgMizuWater_SetWaterBoxesHeight(WaterBox* waterBoxes, s16 height);
}

extern PlayState* gPlayState;

// Mirrors of constants that live in the actors' .c files, not their headers.
// ovl_Bg_Spot06_Objects: LakeHyliaObjectsType, LakeHyliaWaterBoxIndices and the level defines.
#define LHO_WATER_PLANE 2
#define LHO_ICE_BLOCK 3
#define LHWB_GERUDO_VALLEY_RIVER_LOWER 1
#define LHWB_MAIN_1 2
#define LHWB_MAIN_2 3
#define LH_WATER_LEVEL_RAISED (-1313)
#define LH_WATER_LEVEL_RIVER_RAISED (LH_WATER_LEVEL_RAISED + 200)
#define LH_WATER_LEVEL_LOWERED (LH_WATER_LEVEL_RAISED - 680)
#define LH_WATER_LEVEL_RIVER_LOWERED (LH_WATER_LEVEL_RIVER_RAISED - 80)
#define LH_RIVER_LOWER_Z 2203
// The actor's plane sits at lakeHyliaWaterLevel + LH_WATER_LEVEL_RAISED; 0 is full, -681 is the
// dry lake bed. Drop this further to sink the plane out of the world entirely.
#define LH_LEVEL_FULL 0.0f
#define LH_LEVEL_DRY (-681.0f)

// Zora's Domain's frozen water. Its neighbours in that object are the flowing water (Tex_003930)
// and the waterfall (Tex_005D30).
#define ICE_TEX object_spot07_object_Tex_005530

// ovl_Bg_Mizu_Water: the main water body is type 0, and sWaterLevels' extremes as yDiff off baseY.
#define MIZU_TYPE_MAIN 0
#define MIZU_Y_HIGHEST 0.0f
#define MIZU_Y_LOWEST (WATER_TEMPLE_WATER_F1_Y - WATER_TEMPLE_WATER_F3_Y)

static u8 SeasonScene_Active(void) {
    return (Seasons_SeasonCount() != 0) && (Seasons_GetSeason() != SEASON_OFF);
}

/**
 * Called right before the rod reloads the scene in place. That reload respawns Link through
 * RESPAWN_MODE_DOWN — the same path a fall into a pit takes — so vanilla charges him the void-out
 * damage. Turning the season is not a fall. One-shot: the handler unregisters itself after the
 * single respawn it was armed for (the SwitchAge idiom, which needs it for the same reason).
 */
extern "C" void SeasonScene_SuppressVoidDamage(void) {
    static HOOK_ID hookId = 0;

    hookId = REGISTER_VB_SHOULD(VB_INFLICT_VOID_DAMAGE, {
        *should = false;
        GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::OnVanillaBehavior>(hookId);
    });
}

// ============================================================================
// ACTOR STATE ADAPTERS
// ============================================================================

/**
 * Lake Hylia. The water plane actor IS the visible surface — world.pos.y is
 * lakeHyliaWaterLevel + LH_WATER_LEVEL_RAISED — and it writes the water boxes to match. Both have
 * to move together or Link swims through empty air over a dry bed.
 */
static void SeasonScene_OnInitLakeHylia(void* refActor) {
    BgSpot06Objects* self = static_cast<BgSpot06Objects*>(refActor);

    if (!SeasonScene_Active()) {
        return;
    }

    // The lake's own walkable ice slab is Winter-only.
    if (self->dyna.actor.params == LHO_ICE_BLOCK) {
        if (Seasons_GetSeason() != SEASON_WINTER) {
            Actor_Kill(&self->dyna.actor);
        }
        return;
    }
    if (self->dyna.actor.params != LHO_WATER_PLANE) {
        return;
    }

    u8 dry = (Seasons_GetSeason() == SEASON_SUMMER);
    WaterBox* waterBoxes = gPlayState->colCtx.colHeader->waterBoxes;

    self->lakeHyliaWaterLevel = dry ? LH_LEVEL_DRY : LH_LEVEL_FULL;
    self->dyna.actor.world.pos.y = self->lakeHyliaWaterLevel + LH_WATER_LEVEL_RAISED;

    waterBoxes[LHWB_MAIN_1].ySurface = dry ? LH_WATER_LEVEL_LOWERED : LH_WATER_LEVEL_RAISED;
    waterBoxes[LHWB_MAIN_2].ySurface = waterBoxes[LHWB_MAIN_1].ySurface;
    waterBoxes[LHWB_GERUDO_VALLEY_RIVER_LOWER].ySurface =
        dry ? LH_WATER_LEVEL_RIVER_LOWERED : LH_WATER_LEVEL_RIVER_RAISED;
    // ABSOLUTE, never "-= 50" the way the actor's own lowered branch does it: the collision header
    // is cached resource memory, so the relative form drifts 50 units on every scene load.
    waterBoxes[LHWB_GERUDO_VALLEY_RIVER_LOWER].zMin = dry ? (LH_RIVER_LOWER_Z - 50) : LH_RIVER_LOWER_Z;
}

/**
 * Water Temple. Type 0 is the main body: its world.pos.y is the surface and targetY is where the
 * level machinery is heading, so both have to be set or it slides back on the next update.
 */
static void SeasonScene_OnInitWaterTemple(void* refActor) {
    BgMizuWater* self = static_cast<BgMizuWater*>(refActor);

    if ((self->type != MIZU_TYPE_MAIN) || !SeasonScene_Active()) {
        return;
    }

    u8 season = Seasons_GetSeason();

    if ((season != SEASON_SUMMER) && (season != SEASON_AUTUMN)) {
        return;
    }

    self->actor.world.pos.y = self->baseY + ((season == SEASON_SUMMER) ? MIZU_Y_LOWEST : MIZU_Y_HIGHEST);
    self->targetY = self->actor.world.pos.y;
    BgMizuWater_SetWaterBoxesHeight(gPlayState->colCtx.colHeader->waterBoxes, (s16)self->actor.world.pos.y);
}

/**
 * Every other movable body of water, in any scene.
 *
 * Bg_Spot01_Idomizu, Bg_Mori_Idomizu and Bg_Haka_Water all follow Lake Hylia's shape: world.pos.y
 * IS the visible surface, and each one writes its OWN water boxes to match it every frame. So the
 * season only has to shift the actor and let it propagate — which is why this needs no table of
 * which box belongs to which scene. home.pos.y moves with it because their update functions ease
 * world.pos.y back toward home.
 *
 * The shift is a fixed drop rather than a per-scene depth: nothing in the scene says how deep its
 * basin is, and a well and a lake both read as "drained" once the surface is well below the rim.
 */
#define SEASON_WATER_DROP 600.0f
#define SEASON_WATER_RISE 100.0f

static void SeasonScene_OnInitWaterActor(void* refActor) {
    Actor* self = static_cast<Actor*>(refActor);

    if (!SeasonScene_Active()) {
        return;
    }

    f32 delta = 0.0f;

    switch (Seasons_GetSeason()) {
        case SEASON_SUMMER:
            delta = -SEASON_WATER_DROP;
            break;
        case SEASON_AUTUMN:
            delta = SEASON_WATER_RISE;
            break;
        default:
            return; // Spring keeps the vanilla level, Winter freezes what is there
    }

    self->world.pos.y += delta;
    self->home.pos.y += delta;
}

// ============================================================================
// BEAN SPOTS — the plant's stage per season lives in z_obj_bean.c; the two spots it shares
// a site with are handled here
// ============================================================================

/**
 * The scene data of every bean site, read out of oot.o2r rather than guessed.
 *
 * Three things are authored for ONE age only, which is why the seasons need them written down: the
 * soft soil that grows the site's gold skulltula exists in the CHILD setup alone, and the grown
 * plant's flight path — both the list and the INDEX, which a child's bean spot carries as 0x1F, the
 * "no path" value — in the ADULT one. A season ignores age, so all three have to be reachable from
 * either. Zora's River has a bean but no soil, hence the 0.
 */
typedef struct {
    s16 scene;
    s16 room;
    u16 soilParams;       // Obj_Makekinsuta's, which carry the site's gold skulltula flag
    u8 pathIndex;         // into the list below
    const char* pathList; // the adult setup's
} SeasonBeanSite;

static const SeasonBeanSite sSeasonBeanSites[] = {
    { SCENE_GRAVEYARD, 1, 0x5101, 0, "scenes/shared/spot02_scene/spot02_scenePathwayList_00652C" },
    { SCENE_ZORAS_RIVER, 0, 0, 2, "scenes/shared/spot03_scene/spot03_scenePathwayList_0067A8" },
    { SCENE_KOKIRI_FOREST, 0, 0x4D01, 0, "scenes/shared/spot04_scene/spot04_scenePathwayList_00CEAC" },
    { SCENE_LAKE_HYLIA, 0, 0x5301, 1, "scenes/shared/spot06_scene/spot06_scenePathwayList_0076AC" },
    { SCENE_GERUDO_VALLEY, 0, 0x5401, 1, "scenes/shared/spot09_scene/spot09_scenePathwayList_002E58" },
    { SCENE_LOST_WOODS, 5, 0x4E01, 2, "scenes/shared/spot10_scene/spot10_scenePathwayList_00C018" },
    { SCENE_LOST_WOODS, 6, 0x4E02, 1, "scenes/shared/spot10_scene/spot10_scenePathwayList_00C018" },
    { SCENE_DESERT_COLOSSUS, 0, 0x5601, 0, "scenes/shared/spot11_scene/spot11_scenePathwayList_007750" },
    { SCENE_DEATH_MOUNTAIN_TRAIL, 0, 0x5002, 0, "scenes/shared/spot16_scene/spot16_scenePathwayList_007788" },
    { SCENE_DEATH_MOUNTAIN_CRATER, 1, 0x5001, 0, "scenes/shared/spot17_scene/spot17_scenePathwayList_007424" },
};

static const SeasonBeanSite* SeasonScene_BeanSite(PlayState* play) {
    for (u32 i = 0; i < ARRAY_COUNT(sSeasonBeanSites); i++) {
        const SeasonBeanSite* site = &sSeasonBeanSites[i];

        if ((site->scene == play->sceneNum) && (site->room == play->roomCtx.curRoom.num)) {
            return site;
        }
    }
    return NULL;
}

// Spring's bean platform, for a child too: Obj_Bean asks for this before reading its own params.
extern "C" Path* SeasonBean_Path(PlayState* play) {
    if (!SeasonScene_Active() || (Seasons_GetSeason() != SEASON_SPRING)) {
        return NULL;
    }
    const SeasonBeanSite* site = SeasonScene_BeanSite(play);

    if (site == NULL) {
        return NULL;
    }
    Path* list = reinterpret_cast<Path*>(ResourceMgr_GetResourceDataByNameHandlingMQ(site->pathList));

    if (list == NULL) {
        return NULL;
    }
    return &list[site->pathIndex];
}

// Winter: the soft soil's gold skulltula comes up on its own. unk_152 is the "bugs were dropped"
// latch, the same one the NEI shovel flips when it digs a bean spot.
static void SeasonScene_LatchSoftSoil(Actor* self) {
    reinterpret_cast<ObjMakekinsuta*>(self)->unk_152 = 1;
}

static void SeasonScene_OnInitSoftSoil(void* refActor) {
    if (!SeasonScene_Active() || (Seasons_GetSeason() != SEASON_WINTER)) {
        return;
    }
    SeasonScene_LatchSoftSoil(static_cast<Actor*>(refActor));
}

// ============================================================================
// TUNICS — which season the heat forgives, and which one the desert charges for
// ============================================================================

// Kept as one exchange: Winter puts out the two hot rooms in the game (Death Mountain Crater and
// the Fire Temple), and the desert takes their place, charging a tunic in the two seasons that make
// it deadly. Nothing here starts or stops a timer — z_parameter.c does, off this value alone.
static u8 SeasonScene_WastelandTunic(u8 season) {
    if (season == SEASON_WINTER) {
        return EQUIP_VALUE_TUNIC_GORON;
    }
    if (season == SEASON_SUMMER) {
        return EQUIP_VALUE_TUNIC_ZORA;
    }
    return EQUIP_VALUE_TUNIC_KOKIRI;
}

static u8 SeasonScene_WearsTunic(u8 tunic) {
    Player* player = GET_PLAYER(gPlayState);
    u8 asPlayerTunic = tunic - 1; // PLAYER_TUNIC_* is EQUIP_VALUE_TUNIC_* minus one

    return (CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC) == tunic) || (player->currentTunic == asPlayerTunic);
}

// Does the desert charge a tunic right now? Everything the crossing gives back hangs off this.
static u8 SeasonScene_ClearsWasteland(void) {
    if (!SeasonScene_Active() || (gPlayState->sceneNum != SCENE_HAUNTED_WASTELAND)) {
        return 0;
    }
    return SeasonScene_WastelandTunic(Seasons_GetSeason()) != EQUIP_VALUE_TUNIC_KOKIRI;
}

s16 Seasons_EnvHazard(PlayState* play, s16 hazard) {
    if (!SeasonScene_Active()) {
        return hazard;
    }
    u8 season = Seasons_GetSeason();

    if (play->sceneNum == SCENE_HAUNTED_WASTELAND) {
        u8 needed = SeasonScene_WastelandTunic(season);

        if (needed == EQUIP_VALUE_TUNIC_KOKIRI) {
            return hazard;
        }
        return SeasonScene_WearsTunic(needed) ? PLAYER_ENV_HAZARD_NONE : PLAYER_ENV_HAZARD_HOTROOM;
    }
    if ((season == SEASON_WINTER) && (hazard == PLAYER_ENV_HAZARD_HOTROOM)) {
        return PLAYER_ENV_HAZARD_NONE;
    }
    return hazard;
}

// ============================================================================
// THE WASTELAND'S SAFE PATH — lit up while a season charges a tunic for the crossing
// ============================================================================

// Floor property 12 is the sand that swallows you and sends you back to the entrance, so the ground
// without it IS the path. Read from the collision, not from a hand-drawn route.
#define SEASON_SAND_SWALLOWS 12
// A poly steep enough to be a wall is not floor to walk on. Normals are s16, 0x7FFF being straight up.
#define SEASON_FLOOR_NORMAL 0x4000
#define SEASON_PATH_BATCH 10

// Neither is in libultraship's gbi; colViewer.cpp defines its own pair for the same job.
#define SEASON_CC_PRIMITIVE_ENVA 0, 0, 0, PRIMITIVE, 0, 0, 0, ENVIRONMENT
#define SEASON_DEF_VTX(x, y, z, nx, ny, nz)                                       \
    {                                                                             \
        .n = {.ob = { x, y, z }, .tc = { 0, 0 }, .n = { nx, ny, nz }, .a = 0xFF } \
    }

typedef enum {
    SEASON_GROUND_SAFE,
    SEASON_GROUND_SWALLOWS,
    SEASON_GROUND_MAX,
} SeasonGroundKind;

static std::vector<Gfx> sGroundDl[SEASON_GROUND_MAX];
static std::vector<Vtx> sGroundVtx;
static CollisionHeader* sGroundHeader = NULL;

// -1 for anything too steep to walk on, which has no business being painted either way.
static s32 SeasonScene_GroundKind(CollisionPoly* poly) {
    if (poly->normal.y < SEASON_FLOOR_NORMAL) {
        return -1;
    }
    if (func_80041E80(&gPlayState->colCtx, poly, BGCHECK_SCENE) == SEASON_SAND_SWALLOWS) {
        return SEASON_GROUND_SWALLOWS;
    }
    return SEASON_GROUND_SAFE;
}

// Ten triangles per load: the vertex cache holds 32, and one command per triangle would be some
// eight hundred a frame on ground this size.
static void SeasonScene_AppendGroundTriangles(std::vector<Gfx>& dl, size_t from, size_t to) {
    const size_t perLoad = SEASON_PATH_BATCH * 3;

    for (size_t v = from; v < to; v += perLoad) {
        size_t batch = std::min(perLoad, to - v);

        dl.push_back(gsSPVertex((uintptr_t)&sGroundVtx[v], (s32)batch, 0));
        for (size_t t = 0; t + 6 <= batch; t += 6) {
            dl.push_back(gsSP2Triangles((s32)t, (s32)t + 1, (s32)t + 2, 0, (s32)t + 3, (s32)t + 4, (s32)t + 5, 0));
        }
        if ((batch % 6) != 0) {
            dl.push_back(gsSP1Triangle((s32)batch - 3, (s32)batch - 2, (s32)batch - 1, 0));
        }
    }
    dl.push_back(gsSPEndDisplayList());
}

// Decal, so it lies on the sand instead of hovering over it — which is also what keeps it from
// z-fighting the dunes.
static void SeasonScene_AppendGroundSetup(std::vector<Gfx>& dl) {
    dl.push_back(gsSPTexture(0, 0, 0, G_TX_RENDERTILE, G_OFF));
    dl.push_back(gsDPSetCycleType(G_CYC_1CYCLE));
    dl.push_back(gsDPSetRenderMode(
        Z_CMP | IM_RD | CVG_DST_FULL | FORCE_BL | ZMODE_DEC | GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA),
        Z_CMP | IM_RD | CVG_DST_FULL | FORCE_BL | ZMODE_DEC | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)));
    dl.push_back(gsDPSetCombineMode(SEASON_CC_PRIMITIVE_ENVA, SEASON_CC_PRIMITIVE_ENVA));
    dl.push_back(gsSPLoadGeometryMode(G_ZBUFFER));
    dl.push_back(gsSPMatrix(&gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH));
}

/**
 * Bakes both kinds of ground into a display list each, kept until the scene's collision changes.
 *
 * The vertices of both live in ONE vector filled to its final size before a single command is
 * written, because those commands hold pointers into it.
 */
static void SeasonScene_BuildGround(CollisionHeader* col) {
    sGroundVtx.clear();

    s32 count = 0;

    for (s32 i = 0; i < col->numPolygons; i++) {
        count += (SeasonScene_GroundKind(&col->polyList[i]) >= 0);
    }
    sGroundVtx.reserve(count * 3);

    size_t start[SEASON_GROUND_MAX];

    for (s32 kind = 0; kind < SEASON_GROUND_MAX; kind++) {
        start[kind] = sGroundVtx.size();
        for (s32 i = 0; i < col->numPolygons; i++) {
            CollisionPoly* poly = &col->polyList[i];

            if (SeasonScene_GroundKind(poly) != kind) {
                continue;
            }
            Vec3s* vertices[3] = { &col->vtxList[COLPOLY_VTX_INDEX(poly->flags_vIA)],
                                   &col->vtxList[COLPOLY_VTX_INDEX(poly->flags_vIB)],
                                   &col->vtxList[COLPOLY_VTX_INDEX(poly->vIC)] };

            for (s32 v = 0; v < 3; v++) {
                Vtx vertex = SEASON_DEF_VTX(vertices[v]->x, vertices[v]->y, vertices[v]->z, 0, 0x7F, 0);

                sGroundVtx.push_back(vertex);
            }
        }
    }

    for (s32 kind = 0; kind < SEASON_GROUND_MAX; kind++) {
        size_t end = (kind + 1 < SEASON_GROUND_MAX) ? start[kind + 1] : sGroundVtx.size();

        sGroundDl[kind].clear();
        sGroundDl[kind].reserve((end - start[kind]) / 3 + 16);
        SeasonScene_AppendGroundSetup(sGroundDl[kind]);
        SeasonScene_AppendGroundTriangles(sGroundDl[kind], start[kind], end);
    }
    sGroundHeader = col;
}

// The storm is gone in these seasons, so the ground is read at a glance and the wash over the safe
// path only has to hint. What must not be missed is the sand that swallows, so it carries the weight.
#define SEASON_SAFE_ALPHA 26
#define SEASON_SWALLOWS_ALPHA 64
#define SEASON_PATH_LIGHTEN(c) ((u8)(((c) + 255 * 2) / 3))

static void SeasonScene_DrawGround() {
    if ((gPlayState == NULL) || !SeasonScene_ClearsWasteland()) {
        return;
    }
    if (gPlayState->colCtx.colHeader != sGroundHeader) {
        SeasonScene_BuildGround(gPlayState->colCtx.colHeader);
    }
    if (sGroundVtx.empty()) {
        return;
    }

    u8 r, g, b;

    Seasons_SeasonColor(Seasons_GetSeason(), &r, &g, &b);

    OPEN_DISPS(gPlayState->state.gfxCtx);
    // Washed out where you may walk, the season's own colour where the sand takes you.
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, SEASON_PATH_LIGHTEN(r), SEASON_PATH_LIGHTEN(g), SEASON_PATH_LIGHTEN(b), 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 0xFF, 0xFF, 0xFF, SEASON_SAFE_ALPHA);
    gSPDisplayList(POLY_XLU_DISP++, sGroundDl[SEASON_GROUND_SAFE].data());

    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, r, g, b, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 0xFF, 0xFF, 0xFF, SEASON_SWALLOWS_ALPHA);
    gSPDisplayList(POLY_XLU_DISP++, sGroundDl[SEASON_GROUND_SWALLOWS].data());
    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

/**
 * Winter and Summer clear the desert's sandstorm, which is what the tunic buys.
 *
 * Written every frame rather than once on entry: the storm is also the screen wipe those entrances
 * use, so it is turned off after the transition hands the scene over, never during it.
 */
extern "C" void SeasonScene_UpdateWasteland(PlayState* play) {
    if (!SeasonScene_ClearsWasteland() || (play->transitionMode != TRANS_MODE_OFF)) {
        return;
    }
    play->envCtx.sandstormState = SANDSTORM_OFF;
    play->envCtx.sandstormPrimA = 0;
    play->envCtx.sandstormEnvA = 0;
}

static void SeasonScene_RegisterDekuFlower() {
    if (gDekuFlowerId != -1) {
        return;
    }
    ActorDBInit init;

    init.name = "DekuFlower";
    init.desc = "Majora's Mask Deku Flower";
    init.category = ACTORCAT_BG;
    init.objectId = OBJECT_GAMEPLAY_KEEP;
    init.instanceSize = gDekuFlowerStructSize;
    init.init = DekuFlower_Init;
    init.destroy = DekuFlower_Destroy;
    init.update = DekuFlower_Update;
    gDekuFlowerId = ActorDB::Instance->AddEntry(init).entry.id;
}

/**
 * Summer turns the spot into Majora's gold Deku Flower, Deku Link's launch pad.
 *
 * Winter's skulltula is raised from here as well, because the soft soil that carries it is in the
 * child setup only and an adult would never meet one. The bean spot is in both, and is the actor
 * that marks the site.
 */
static void SeasonScene_OnInitBeanSpot(void* refActor) {
    Actor* self = static_cast<Actor*>(refActor);

    if (!SeasonScene_Active()) {
        return;
    }
    if (Seasons_GetSeason() == SEASON_SUMMER) {
        SeasonScene_RegisterDekuFlower();
        Actor_Spawn(&gPlayState->actorCtx, gPlayState, gDekuFlowerId, self->world.pos.x, self->world.pos.y,
                    self->world.pos.z, 0, self->shape.rot.y, 0, DEKU_FLOWER_PARAMS(DEKU_FLOWER_TYPE_GOLD));
        Actor_Kill(self);
        return;
    }

    const SeasonBeanSite* site = SeasonScene_BeanSite(gPlayState);

    if ((Seasons_GetSeason() != SEASON_WINTER) || !LINK_IS_ADULT || (site == NULL) || (site->soilParams == 0)) {
        return;
    }
    Actor* soil = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_OBJ_MAKEKINSUTA, self->world.pos.x,
                              self->world.pos.y, self->world.pos.z, 0, self->shape.rot.y, 0, site->soilParams);

    if (soil != NULL) {
        SeasonScene_LatchSoftSoil(soil);
    }
}

// ============================================================================
// GENERIC RULES — an actor is absent, or loses its collision, in given seasons
// ============================================================================

// Most scene-by-scene wishes turn out to be one of these two, so they are a table rather than an
// adapter each: red ice and icicles should not exist in Summer, the Zora's Domain waterfall should
// be walkable through. Adding a behaviour is adding a row.
typedef enum {
    SEASON_RULE_ABSENT,       // the actor is not in the world this season
    SEASON_RULE_NO_COLLISION, // it is there, but you pass through it (dynapoly actors only)
} SeasonRuleKind;

#define SEASON_BIT(s) (1 << (s))

#define SEASON_RULE_ANY_PARAMS (-1)

typedef struct {
    s16 actorId;
    s16 params; // SEASON_RULE_ANY_PARAMS matches every variant of the actor
    u8 seasons; // SEASON_BIT mask of the seasons the rule fires in
    u8 kind;
} SeasonActorRule;

static const SeasonActorRule sSeasonActorRules[] = {
    // Ice Cavern thaws in Summer: no red ice to burn, no icicles overhead.
    { ACTOR_BG_ICE_SHELTER, SEASON_RULE_ANY_PARAMS, SEASON_BIT(SEASON_SUMMER), SEASON_RULE_ABSENT },
    { ACTOR_BG_ICE_TURARA, SEASON_RULE_ANY_PARAMS, SEASON_BIT(SEASON_SUMMER), SEASON_RULE_ABSENT },
    // Bg_Spot07_Taki is NOT listed: it already carries both states itself, and its own
    // BgSpot07Taki_IsFrozen now answers to the season. Thawed it registers no collision at all, so
    // a NO_COLLISION rule here would be redundant and would fight the real ice in Winter.
    // Winter is snow and a grey sky, nothing else — so Kokiri Forest's fairies (params 0) sit it
    // out. The rod's own snow is params 3 and is left alone.
    { ACTOR_OBJECT_KANKYO, 0, SEASON_BIT(SEASON_WINTER), SEASON_RULE_ABSENT },
};

static void SeasonScene_ApplyActorRule(void* refActor) {
    Actor* self = static_cast<Actor*>(refActor);

    if (!SeasonScene_Active()) {
        return;
    }

    u8 seasonBit = SEASON_BIT(Seasons_GetSeason());

    for (u32 i = 0; i < ARRAY_COUNT(sSeasonActorRules); i++) {
        const SeasonActorRule* rule = &sSeasonActorRules[i];

        if ((rule->actorId != self->id) || !(rule->seasons & seasonBit) ||
            ((rule->params != SEASON_RULE_ANY_PARAMS) && (rule->params != self->params))) {
            continue;
        }
        if (rule->kind == SEASON_RULE_ABSENT) {
            Actor_Kill(self);
            return;
        }
        // Drop the collision but keep the actor drawing, so the water still falls and only stops
        // being a wall. The bgId has to be cleared too or Destroy unregisters it a second time.
        DynaPolyActor* dyna = reinterpret_cast<DynaPolyActor*>(self);

        if (dyna->bgId != BGACTOR_NEG_ONE) {
            DynaPoly_DeleteBgActor(gPlayState, &gPlayState->colCtx.dyna, dyna->bgId);
            dyna->bgId = BGACTOR_NEG_ONE;
        }
        return;
    }
}

// ============================================================================
// WINTER — every water surface becomes standable ice
// ============================================================================

/**
 * Drained water stops being water: no swimming, no drowning, no surface to break. A water box whose
 * surface sits below anything reachable can never contain Link, which removes the behaviour without
 * touching a single thing that is drawn. Summer only — Winter's water stays live under its ice.
 *
 * The header is cached resource memory shared by every later load of that scene, so the untouched
 * surfaces are kept and put back — including when Link walks out of the scene, which is why the
 * header POINTER is remembered rather than the scene number.
 */
#define SEASON_WATER_GONE (-32000)
#define SEASON_MAX_WATERBOXES 32

static CollisionHeader* sSuppressedHeader = NULL;
static s16 sWaterSurfaceBackup[SEASON_MAX_WATERBOXES];
static s32 sSuppressedCount = 0;

static void SeasonScene_RestoreWaterBoxes(void) {
    if (sSuppressedHeader == NULL) {
        return;
    }
    for (s32 i = 0; i < sSuppressedCount; i++) {
        sSuppressedHeader->waterBoxes[i].ySurface = sWaterSurfaceBackup[i];
    }
    sSuppressedHeader = NULL;
    sSuppressedCount = 0;
}

// Re-applied every frame, because the actors that own their water rewrite these surfaces too.
static void SeasonScene_SuppressWaterBoxes(CollisionHeader* colHeader) {
    if (sSuppressedHeader != colHeader) {
        SeasonScene_RestoreWaterBoxes();
        sSuppressedCount = MIN(colHeader->numWaterBoxes, SEASON_MAX_WATERBOXES);
        for (s32 i = 0; i < sSuppressedCount; i++) {
            sWaterSurfaceBackup[i] = colHeader->waterBoxes[i].ySurface;
        }
        sSuppressedHeader = colHeader;
    }
    for (s32 i = 0; i < sSuppressedCount; i++) {
        colHeader->waterBoxes[i].ySurface = SEASON_WATER_GONE;
    }
}

typedef enum {
    SEASON_LOOK_VANILLA,
    SEASON_LOOK_ICE,
    SEASON_LOOK_INVISIBLE,
} SeasonWaterLook;

/**
 * Repoints every scene's water at another texture, or drops its triangles, or puts both back.
 *
 * Winter names Zora's Domain's frozen water by its __OTR__ path rather than resolving it to pixels,
 * so an HD pack or a mod that replaces it still wins — the rule gDPLoadTextureBlock follows. Summer
 * removes the triangles instead: water takes its alpha from prim, so a transparent texture came out
 * as a black sheet rather than as nothing.
 *
 * This edits the resources, not the scene, so it survives room changes and has to be undone
 * explicitly. season_water_dls.inc lists only 32x32 RGBA16 water, for the reason given there.
 */
static void SeasonScene_SetWaterLook(u8 look) {
    static u8 applied = SEASON_LOOK_VANILLA;

    if (applied == look) {
        return;
    }
    for (s32 i = 0; i < ARRAY_COUNT(sSeasonWaterDLs); i++) {
        const SeasonWaterDL* row = &sSeasonWaterDLs[i];
        // Every patch name carries its instruction: one display list can hold several water sites,
        // and a shared name would let the second overwrite the first's bookkeeping.
        std::string textureTag = "seasonWaterTex_" + std::to_string(row->textureInstruction);
        std::string hashTag = "seasonWaterHash_" + std::to_string(row->textureInstruction);

        ResourceMgr_UnpatchGfxByName(row->dlPath, textureTag.c_str());
        ResourceMgr_UnpatchGfxByName(row->dlPath, hashTag.c_str());
        for (s32 t = 0; t < row->triangleCount; t++) {
            std::string triangleTag = "seasonWaterTri_" + std::to_string(row->triangles[t]);

            ResourceMgr_UnpatchGfxByName(row->dlPath, triangleTag.c_str());
        }

        if (look == SEASON_LOOK_ICE) {
            ResourceMgr_PatchGfxByName(row->dlPath, textureTag.c_str(), row->textureInstruction,
                                       gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, ICE_TEX));
            // The original is a 128-bit SETTIMG_OTR_HASH: its second word (the texture's CRC64) is
            // still there and would be executed as an opcode after the one-word replacement.
            ResourceMgr_PatchGfxByName(row->dlPath, hashTag.c_str(), row->textureInstruction + 1, gsSPNoOp());
        } else if (look == SEASON_LOOK_INVISIBLE) {
            for (s32 t = 0; t < row->triangleCount; t++) {
                std::string triangleTag = "seasonWaterTri_" + std::to_string(row->triangles[t]);

                ResourceMgr_PatchGfxByName(row->dlPath, triangleTag.c_str(), row->triangles[t], gsSPNoOp());
            }
        }
    }
    applied = look;
}

static u8 SeasonScene_WaterLook(void) {
    if (!SeasonScene_Active()) {
        return SEASON_LOOK_VANILLA;
    }
    switch (Seasons_GetSeason()) {
        case SEASON_WINTER:
            return SEASON_LOOK_ICE;
        case SEASON_SUMMER:
            return SEASON_LOOK_INVISIBLE;
        default:
            return SEASON_LOOK_VANILLA;
    }
}

extern "C" void SeasonScene_UpdateWater(PlayState* play) {
    CollisionHeader* colHeader = play->colCtx.colHeader;
    u8 season = Seasons_GetSeason();

    SeasonScene_SetWaterLook(SeasonScene_WaterLook());

    if (!SeasonScene_Active() || (colHeader == NULL) || (colHeader->numWaterBoxes == 0)) {
        SeasonScene_RestoreWaterBoxes();
        return;
    }

    if (season == SEASON_SUMMER) {
        SeasonScene_SuppressWaterBoxes(colHeader);
        return;
    }

    // Winter leaves its water alone. Link walks it through RocBoots_WalksOnWater, whose own
    // conditions drop the pin the moment he is actually in the water — so falling under the ice
    // still lets him swim back up, which suppressed boxes would not.
    SeasonScene_RestoreWaterBoxes();
}

// ============================================================================
// VANILLA WEATHER TAGS — the weather each scene was actually designed to have
// ============================================================================

/**
 * Retires each scene's own weather tags while a season is running.
 *
 * The rod drives the weather everywhere, and it owns gloomySkyMode to do it — which is also the
 * field WeatherTag_CheckEnableWeatherEffect refuses to run against when it is non-zero. So the tags
 * could not fire anyway, and leaving them alive only risks a proximity tag fighting the season for
 * gWeatherMode. The sandstorm type stays: it is fog intensity, not weather, and the desert is not
 * seasonal.
 */
static void SeasonScene_OnInitWeatherTag(void* refActor) {
    Actor* self = static_cast<Actor*>(refActor);

    if (!SeasonScene_Active()) {
        return;
    }
    // The sandstorm tag is fog rather than weather, so it normally survives — but not in the seasons
    // that clear the desert, where thick fog is the storm by another name.
    if (((self->params & 0xF) != EN_WEATHER_TAG_TYPE_SANDSTORM_INTENSITY) || SeasonScene_ClearsWasteland()) {
        Actor_Kill(self);
    }
}

void RegisterSeasonSceneEffects() {
    COND_ID_HOOK(OnActorInit, ACTOR_BG_SPOT06_OBJECTS, true, SeasonScene_OnInitLakeHylia);
    COND_ID_HOOK(OnActorInit, ACTOR_BG_MIZU_WATER, true, SeasonScene_OnInitWaterTemple);

    // One hook per actor the table mentions; the handler is shared and finds its own row.
    COND_ID_HOOK(OnActorInit, ACTOR_BG_ICE_SHELTER, true, SeasonScene_ApplyActorRule);
    COND_ID_HOOK(OnActorInit, ACTOR_BG_ICE_TURARA, true, SeasonScene_ApplyActorRule);
    COND_ID_HOOK(OnActorInit, ACTOR_OBJECT_KANKYO, true, SeasonScene_ApplyActorRule);

    COND_ID_HOOK(OnActorInit, ACTOR_OBJ_MAKEKINSUTA, true, SeasonScene_OnInitSoftSoil);
    COND_ID_HOOK(OnActorInit, ACTOR_OBJ_BEAN, true, SeasonScene_OnInitBeanSpot);

    COND_ID_HOOK(OnActorInit, ACTOR_EN_WEATHER_TAG, true, SeasonScene_OnInitWeatherTag);

    // Every other movable body of water: same shape, one handler.
    COND_ID_HOOK(OnActorInit, ACTOR_BG_SPOT01_IDOMIZU, true, SeasonScene_OnInitWaterActor);
    COND_ID_HOOK(OnActorInit, ACTOR_BG_MORI_IDOMIZU, true, SeasonScene_OnInitWaterActor);
    COND_ID_HOOK(OnActorInit, ACTOR_BG_HAKA_WATER, true, SeasonScene_OnInitWaterActor);

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(SeasonScene_DrawGround);
}

static RegisterShipInitFunc initFuncSeasonScene(RegisterSeasonSceneEffects, {});
