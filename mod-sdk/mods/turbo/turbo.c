#include <stdbool.h>
#include <stdint.h>

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ModApi/ModApi.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define TURBO_BUTTONS_CVAR "gMods.Turbo.Buttons"
#define TURBO_SPEED_CVAR "gMods.Turbo.Speed"
#define TURBO_TOGGLE_CVAR "gMods.Turbo.Toggle"
#define TURBO_SIDEBAR "Mods"

// Mirrors BTN_CUSTOM_MODIFIER1/2 in soh/OTRGlobals.h, a C++ header a C mod cannot include.
#define TURBO_BTN_MODIFIER1 0x0040
#define TURBO_BTN_MODIFIER2 0x0080

#define TURBO_DEFAULT_BUTTONS TURBO_BTN_MODIFIER1
#define TURBO_DEFAULT_SPEED 8
#define TURBO_MAX_SPEED 20

void Play_Update(PlayState* play);

typedef struct {
    const char* label;
    uint32_t mask;
} TurboButton;

static const TurboButton sButtons[] = {
    { "Modifier 1", TURBO_BTN_MODIFIER1 },
    { "Modifier 2", TURBO_BTN_MODIFIER2 },
    { "L", BTN_L },
    { "R", BTN_R },
    { "Z", BTN_Z },
    { "D-Pad Up", BTN_DUP },
    { "D-Pad Down", BTN_DDOWN },
    { "D-Pad Left", BTN_DLEFT },
    { "D-Pad Right", BTN_DRIGHT },
};

static const char* const sRequiredHooks[] = { "OnGameStateMainStart" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static bool sIsToggledOn;

static uint32_t GetComboMask(void) {
    return (uint32_t)CVarGetInteger(TURBO_BUTTONS_CVAR, TURBO_DEFAULT_BUTTONS);
}

static bool IsComboHeld(const Input* input, uint32_t combo) {
    return combo != 0 && CHECK_BTN_ALL(input->cur.button, combo);
}

static bool IsComboJustPressed(const Input* input, uint32_t combo) {
    return IsComboHeld(input, combo) && (input->press.button & combo) != 0;
}

static void FlipToggle(void) {
    sIsToggledOn = !sIsToggledOn;
    Sfx_PlaySfxCentered(sIsToggledOn ? NA_SE_SY_CAMERA_ZOOM_UP : NA_SE_SY_CAMERA_ZOOM_DOWN);
}

static bool IsTurboActive(const Input* input) {
    uint32_t combo = GetComboMask();

    if (!CVarGetInteger(TURBO_TOGGLE_CVAR, 0)) {
        sIsToggledOn = false;
        return IsComboHeld(input, combo);
    }
    if (IsComboJustPressed(input, combo)) {
        FlipToggle();
    }
    return sIsToggledOn;
}

// The kaleido update loads textures and moves the cursor; it stays at normal speed.
static bool CanRunExtraTicks(PlayState* play) {
    return play->state.running && play->pauseCtx.state == 0;
}

// Extra ticks see held buttons and the stick but no new presses, so one tap is never read N times.
static void RunExtraTicks(PlayState* play, int32_t count) {
    uint32_t pressed[ARRAY_COUNT(play->state.input)];
    uint32_t released[ARRAY_COUNT(play->state.input)];

    for (int32_t pad = 0; pad < ARRAY_COUNT(play->state.input); pad++) {
        pressed[pad] = play->state.input[pad].press.button;
        released[pad] = play->state.input[pad].rel.button;
        play->state.input[pad].press.button = 0;
        play->state.input[pad].rel.button = 0;
    }
    for (int32_t tick = 0; tick < count && CanRunExtraTicks(play); tick++) {
        Play_Update(play);
    }
    for (int32_t pad = 0; pad < ARRAY_COUNT(play->state.input); pad++) {
        play->state.input[pad].press.button = pressed[pad];
        play->state.input[pad].rel.button = released[pad];
    }
}

// Runs before Play_Main, whose own update and draw make the last tick of the frame.
static void SpeedUpFrame(void) {
    PlayState* play = gPlayState;

    if (play == NULL || !IsTurboActive(&play->state.input[0]) || !CanRunExtraTicks(play)) {
        return;
    }
    RunExtraTicks(play, CVarGetInteger(TURBO_SPEED_CVAR, TURBO_DEFAULT_SPEED) - 1);
}

static void DrawButtonPicker(void) {
    uint32_t combo = GetComboMask();

    ModUI_Text("Hold all the ticked buttons together. Bind Modifier 1/2 in Controller Configuration.");
    for (int32_t i = 0; i < ARRAY_COUNT(sButtons); i++) {
        bool isInCombo = (combo & sButtons[i].mask) != 0;

        if (ModUI_Checkbox(sButtons[i].label, &isInCombo)) {
            combo = isInCombo ? (combo | sButtons[i].mask) : (combo & ~sButtons[i].mask);
            CVarSetInteger(TURBO_BUTTONS_CVAR, (int32_t)combo);
            CVarSave();
        }
    }
}

static void RegisterMenu(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterMenuSidebar) || !SOH_MOD_API_HAS(sApi, RegisterMenuWidget)) {
        return;
    }
    sApi->RegisterMenuSidebar("Enhancements", TURBO_SIDEBAR, "gMods.Menu", 1);

    SOHModMenuWidget speed = { sizeof(SOHModMenuWidget) };
    speed.section = "Enhancements";
    speed.sidebar = TURBO_SIDEBAR;
    speed.type = SOH_MOD_MENU_INT_SLIDER;
    speed.label = "Turbo Speed";
    speed.cvar = TURBO_SPEED_CVAR;
    speed.tooltip = "Game ticks per frame while turbo is on. Music keeps its normal speed.";
    speed.defaultInt = TURBO_DEFAULT_SPEED;
    speed.minInt = 2;
    speed.maxInt = TURBO_MAX_SPEED;
    sApi->RegisterMenuWidget(&speed);

    SOHModMenuWidget toggle = { sizeof(SOHModMenuWidget) };
    toggle.section = "Enhancements";
    toggle.sidebar = TURBO_SIDEBAR;
    toggle.type = SOH_MOD_MENU_CHECKBOX;
    toggle.label = "Toggle Turbo Instead of Holding";
    toggle.cvar = TURBO_TOGGLE_CVAR;
    toggle.tooltip = "Tap the combo to turn turbo on and again to turn it off.";
    sApi->RegisterMenuWidget(&toggle);

    SOHModMenuWidget buttons = { sizeof(SOHModMenuWidget) };
    buttons.section = "Enhancements";
    buttons.sidebar = TURBO_SIDEBAR;
    buttons.type = SOH_MOD_MENU_CUSTOM;
    buttons.label = "Turbo Buttons";
    buttons.onChange = DrawButtonPicker;
    sApi->RegisterMenuWidget(&buttons);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    RegisterMenu();
    SOH_REGISTER_HOOK(sApi, OnGameStateMainStart, SpeedUpFrame);
}
