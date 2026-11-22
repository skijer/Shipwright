#include "mario_config.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "mario_menu.h"

#define MARIO_MODE_CVAR "gMods.MarioMask.MarioMode"
#define MARIO_TICK_RATE_CVAR MARIO_SYNC_CVAR("TickRate")
#define MARIO_SHIP_SHADING_CVAR MARIO_SYNC_CVAR("ShipShading")
#define MARIO_COLLISION_UPDATES_CVAR MARIO_SYNC_CVAR("CollisionUpdates")

#define MARIO_TICK_RATE_MIN 20
#define MARIO_TICK_RATE_ORIGINAL 30
#define MARIO_TICK_RATE_MAX 60

typedef enum MarioCollisionUpdates {
    MARIO_COLLISION_UPDATES_OFF,
    MARIO_COLLISION_UPDATES_LOW,
    MARIO_COLLISION_UPDATES_MEDIUM,
    MARIO_COLLISION_UPDATES_HIGH,
    MARIO_COLLISION_UPDATES_COUNT
} MarioCollisionUpdates;

static const char* const kCollisionUpdateLabels[MARIO_COLLISION_UPDATES_COUNT] = { "Off", "Low", "Medium", "High" };
static const int32_t kCollisionUpdateFrames[MARIO_COLLISION_UPDATES_COUNT] = { 0, 8, 4, 2 };

static void DrawHeading(void) {
    ModUI_Text("Configuration");
}

static void RegisterMarioMode(const SOHModApi* api) {
    SOHModMenuWidget widget = MarioMenu_MakeWidget(MARIO_COLUMN_CONFIGURATION, SOH_MOD_MENU_CHECKBOX, "Mario Mode");

    widget.cvar = MARIO_MODE_CVAR;
    widget.tooltip = "Play as Mario all the time, without the mask.";
    api->RegisterMenuWidget(&widget);
}

static void RegisterTickRate(const SOHModApi* api) {
    SOHModMenuWidget widget =
        MarioMenu_MakeWidget(MARIO_COLUMN_CONFIGURATION, SOH_MOD_MENU_INT_SLIDER, "Sync Speed (ticks per second)");

    widget.cvar = MARIO_TICK_RATE_CVAR;
    widget.tooltip = "How many times per second Mario's physics run. 30 is his original speed; SM64 moves him a "
                     "fixed distance per tick, so other values also make him slower or faster.";
    widget.defaultInt = MARIO_TICK_RATE_ORIGINAL;
    widget.minInt = MARIO_TICK_RATE_MIN;
    widget.maxInt = MARIO_TICK_RATE_MAX;
    api->RegisterMenuWidget(&widget);
}

static void RegisterCollisionUpdates(const SOHModApi* api) {
    SOHModMenuWidget widget =
        MarioMenu_MakeWidget(MARIO_COLUMN_CONFIGURATION, SOH_MOD_MENU_COMBOBOX, "Collision Updates");

    widget.cvar = MARIO_COLLISION_UPDATES_CVAR;
    widget.tooltip = "How often Mario's collision follows the moving world: every 8, 4 or 2 frames. Off freezes "
                     "moving platforms where they were when the scene loaded.";
    widget.defaultInt = MARIO_COLLISION_UPDATES_MEDIUM;
    widget.values = kCollisionUpdateLabels;
    widget.valueCount = MARIO_COLLISION_UPDATES_COUNT;
    api->RegisterMenuWidget(&widget);
}

static void RegisterFloatSlider(const SOHModApi* api, const char* label, const char* cvar, const char* tooltip,
                                float defaultValue) {
    SOHModMenuWidget widget = MarioMenu_MakeWidget(MARIO_COLUMN_CONFIGURATION, SOH_MOD_MENU_FLOAT_SLIDER, label);

    widget.cvar = cvar;
    widget.tooltip = tooltip;
    widget.defaultFloat = defaultValue;
    widget.minFloat = 0.0f;
    widget.maxFloat = 1.0f;
    api->RegisterMenuWidget(&widget);
}

void MarioConfig_RegisterMenu(const SOHModApi* api) {
    MarioMenu_AddDrawn(api, MARIO_COLUMN_CONFIGURATION, "mario_mask.heading.configuration", DrawHeading);
    RegisterMarioMode(api);
    RegisterTickRate(api);
    RegisterFloatSlider(api, "Ship Shading", MARIO_SHIP_SHADING_CVAR,
                        "How much the game's lighting and shading affect Mario. 0 keeps his original SM64 colors.",
                        1.0f);
    RegisterCollisionUpdates(api);
    RegisterFloatSlider(api, "Mario Volume", MARIO_VOLUME_CVAR, "Volume of Mario's voice and sounds.", 0.8f);
}

bool MarioConfig_IsMarioModeOn(void) {
    return CVarGetInteger(MARIO_MODE_CVAR, 0) != 0;
}

int32_t MarioConfig_GetTickRate(void) {
    const int32_t rate = CVarGetInteger(MARIO_TICK_RATE_CVAR, MARIO_TICK_RATE_ORIGINAL);

    if (rate < MARIO_TICK_RATE_MIN || rate > MARIO_TICK_RATE_MAX) {
        return MARIO_TICK_RATE_ORIGINAL;
    }
    return rate;
}

float MarioConfig_GetShipShading(void) {
    return CVarGetFloat(MARIO_SHIP_SHADING_CVAR, 1.0f);
}

int32_t MarioConfig_GetCollisionUpdateFrames(void) {
    const int32_t choice = CVarGetInteger(MARIO_COLLISION_UPDATES_CVAR, MARIO_COLLISION_UPDATES_MEDIUM);

    if (choice < 0 || choice >= MARIO_COLLISION_UPDATES_COUNT) {
        return kCollisionUpdateFrames[MARIO_COLLISION_UPDATES_MEDIUM];
    }
    return kCollisionUpdateFrames[choice];
}
