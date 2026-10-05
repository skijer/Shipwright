// Actor-free rain. Port of marsh6487's GlobalOutdoorRain (bc149477, poc/lost-woods-rain-small-fixes). The engine
// draws rain from envCtx.unk_EE: unk_EE[0] is the density the scene asks for and unk_EE[1] the particle count
// that eases toward it. Rain actors write unk_EE[0]; this mod writes it too, from OnGameFrameUpdate, but only
// when no native source (a placed rain actor, a cutscene, Song of Storms) is already asking for rain.
//
//   Lost Woods     always raining, density LOST_WOODS_DENSITY
//   Outdoor modes  Off / Persistent / Intermittent (rain for a spell, dry for a spell), over the outdoor scenes
//
// Not ported: the private rain and thunder audio, rain colour, overcast sky and lightning control.

#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define CVAR_LOST_WOODS "gMods.OutdoorRain.LostWoods"
#define CVAR_MODE "gMods.OutdoorRain.Mode"
#define CVAR_DENSITY "gMods.OutdoorRain.Density"
#define LOST_WOODS_DENSITY 25
#define RAIN_MAX_DENSITY 64
#define DRY_FRAMES (20 * 60 * 20) // frames of game logic: 20 Hz
#define WET_FRAMES (8 * 60 * 20)

typedef enum {
    RAIN_OFF,
    RAIN_PERSISTENT,
    RAIN_INTERMITTENT,
} RainMode;

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate", "OnSceneInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static s32 sCycleFrames;
static bool sRaining;

static bool IsOutdoorScene(s16 scene) {
    switch (scene) {
        case SCENE_HYRULE_FIELD:
        case SCENE_LAKE_HYLIA:
        case SCENE_GERUDO_VALLEY:
        case SCENE_GERUDOS_FORTRESS:
        case SCENE_LON_LON_RANCH:
        case SCENE_KAKARIKO_VILLAGE:
        case SCENE_GRAVEYARD:
        case SCENE_ZORAS_RIVER:
        case SCENE_KOKIRI_FOREST:
        case SCENE_SACRED_FOREST_MEADOW:
        case SCENE_LOST_WOODS:
        case SCENE_DESERT_COLOSSUS:
        case SCENE_DEATH_MOUNTAIN_TRAIL:
        case SCENE_DEATH_MOUNTAIN_CRATER:
            return true;
        default:
            return false;
    }
}

static void Tick(void) {
    PlayState* play = gPlayState;
    s32 wanted = 0;
    s32 mode;

    if (play == NULL || play->pauseCtx.state != 0 || play->csCtx.state != CS_STATE_IDLE) {
        return;
    }
    if (play->sceneNum == SCENE_LOST_WOODS && CVarGetInteger(CVAR_LOST_WOODS, 1)) {
        wanted = LOST_WOODS_DENSITY;
    } else if (IsOutdoorScene(play->sceneNum)) {
        mode = CVarGetInteger(CVAR_MODE, RAIN_OFF);
        if (mode == RAIN_PERSISTENT) {
            wanted = CLAMP(CVarGetInteger(CVAR_DENSITY, 30), 0, RAIN_MAX_DENSITY);
        } else if (mode == RAIN_INTERMITTENT) {
            if (--sCycleFrames <= 0) {
                sRaining = !sRaining;
                sCycleFrames = sRaining ? WET_FRAMES : DRY_FRAMES;
            }
            wanted = sRaining ? CLAMP(CVarGetInteger(CVAR_DENSITY, 30), 0, RAIN_MAX_DENSITY) : 0;
        }
    }
    // Never lower what a native source asks for; only add rain where there is none.
    if (wanted > play->envCtx.unk_EE[0]) {
        play->envCtx.unk_EE[0] = wanted;
    }
}

static void ResetCycle(int16_t sceneNum) {
    sRaining = false;
    sCycleFrames = DRY_FRAMES / 4;
}

static void RegisterMenu(void) {
    SOHModMenuWidget lostWoods = { sizeof(SOHModMenuWidget) };
    SOHModMenuWidget mode = { sizeof(SOHModMenuWidget) };
    SOHModMenuWidget density = { sizeof(SOHModMenuWidget) };

    lostWoods.section = "Enhancements";
    lostWoods.sidebar = "Graphics";
    lostWoods.type = SOH_MOD_MENU_CHECKBOX;
    lostWoods.label = "Rain in the Lost Woods";
    lostWoods.cvar = CVAR_LOST_WOODS;
    lostWoods.tooltip = "It is always raining in the Lost Woods.";
    lostWoods.defaultInt = 1;
    sApi->RegisterMenuWidget(&lostWoods);

    mode.section = "Enhancements";
    mode.sidebar = "Graphics";
    mode.type = SOH_MOD_MENU_INT_SLIDER;
    mode.label = "Outdoor Rain (0 off, 1 persistent, 2 intermittent)";
    mode.cvar = CVAR_MODE;
    mode.tooltip = "Rain across the outdoor areas, without needing a rain actor.";
    mode.defaultInt = RAIN_OFF;
    mode.minInt = RAIN_OFF;
    mode.maxInt = RAIN_INTERMITTENT;
    sApi->RegisterMenuWidget(&mode);

    density.section = "Enhancements";
    density.sidebar = "Graphics";
    density.type = SOH_MOD_MENU_INT_SLIDER;
    density.label = "Outdoor Rain Density";
    density.cvar = CVAR_DENSITY;
    density.defaultInt = 30;
    density.minInt = 1;
    density.maxInt = RAIN_MAX_DENSITY;
    sApi->RegisterMenuWidget(&density);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, Tick);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ResetCycle);
    if (SOH_MOD_API_HAS(sApi, RegisterMenuWidget)) {
        RegisterMenu();
    }
}
