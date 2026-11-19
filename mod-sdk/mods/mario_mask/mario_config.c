#include "mario_config.h"

#include "mario_menu.h"

#define MARIO_SYNC_CVAR(name) "gMods.MarioMask.Sync." name

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

static void DrawHeading(void) {
    ModUI_Text("Configuration");
}

static void RegisterTickRate(const SOHModApi* api) {
    SOHModMenuWidget widget =
        MarioMenu_MakeWidget(MARIO_COLUMN_CONFIGURATION, SOH_MOD_MENU_INT_SLIDER, "Sync Speed (ticks per second)");

    widget.cvar = MARIO_SYNC_CVAR("TickRate");
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

    widget.cvar = MARIO_SYNC_CVAR("CollisionUpdates");
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
    RegisterTickRate(api);
    RegisterFloatSlider(api, "Ship Shading", MARIO_SYNC_CVAR("ShipShading"),
                        "How much the game's lighting and shading affect Mario. 0 keeps his original SM64 colors.",
                        1.0f);
    RegisterCollisionUpdates(api);
    RegisterFloatSlider(api, "Mario Volume", MARIO_SYNC_CVAR("Volume"), "Volume of Mario's voice and sounds.", 0.8f);
}
