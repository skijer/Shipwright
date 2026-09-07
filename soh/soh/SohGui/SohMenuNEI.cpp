#include "SohMenu.h"
#include "soh/OTRGlobals.h"
#include "UIWidgets.hpp"
#include "soh/ResourceManagerHelpers.h"
#include "soh/resource/type/PlayerAnimation.h"
#include "soh/resource/type/SohResourceType.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/static_data.h" // the Sensor rune's item picker names its wishes
#include "soh/Notification/Notification.h"
#include "soh/FleetShipCombo/FleetShipCombo.h"
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#ifdef __APPLE__
#include <SDL_scancode.h>
#include <SDL_gamecontroller.h>
#else
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_gamecontroller.h>
#endif

extern "C" {
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "functions.h"
#include "z64animation.h"
#include "transformation_masks/assets/mm_asset_loader.h"
#include "mods/transformation_masks/transformation_masks.h"
#include "mods/pak_loader/pak_loader.h"
#include "mods/voice_pack/voice_pack.h"
void PikachuControls_OpenWindow(void); // pikachu_hud.cpp — Pikachu mode bindings window
extern PlayState* gPlayState;
u8 GerudoForm_IsActive(void); // gerudo_form.cpp
}

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

// =============================================================================
// Item Editor - per-item live tuning (Skijer's NEI)
// =============================================================================
// One section per custom item. Every control writes a `gItemEditor.<Item>.*`
// CVar that the item's behavior TU reads once per frame, so edits land live and
// the CVar names are shared with 2ship (a preset carries over between games).
// More items get their own section below as they are made tunable.
namespace {

struct CapeFloatParam {
    const char* label;
    const char* cvar;
    float min;
    float max;
    float def;
    const char* tooltip;
};

// The Sheikah Slate, Rod of Seasons, Elemental Wand and Master Cycle sliders are GONE (2026-09-02):
// every one of those poses is baked into its own item now (the ItemHandPose in each
// mods/items/objects/object_*.c, and MC_SEAT_* / MC_MODEL_SCALE in mods/actors/master_cycle.c).
// The Magic Cape is the only rig left live, and it moved out of this tab into its own window.

// El trident ya NO tiene NADA en el Item Editor (2026-08-18). Las tres colocaciones
// -- lanza en mano, estela/hitbox y glow de carga -- son constantes en
// extended_equipment.c (TRIDENT_HELD_* / TRIDENT_TRAIL_* / TRIDENT_THUNDER_*) y ese
// codigo ya no lee un solo CVar: los gItemEditor.Trident.* estan muertos, ni se
// escriben ni se leen. Para retocar cualquiera de esos numeros hay que editarlos alli
// y recompilar.

// Shape ------------------------------------------------------------------
const CapeFloatParam kCapeShapeParams[] = {
    { "Scale", "gItemEditor.Cape.Scale", 0.1f, 5.0f, 1.0f,
      "Master multiplier over both the cape's length and its width." },
    { "Segment Length", "gItemEditor.Cape.Length", 0.5f, 30.0f, 4.5f,
      "Length of one cloth segment. The cape is 12 joints deep, so the total\n"
      "hanging length is about 11x this value. Vanilla: 4.5." },
    { "Width", "gItemEditor.Cape.Width", 0.1f, 5.0f, 1.0f,
      "Multiplier over the shoulder-to-shoulder span the cloth is pinned across." },
    { "Arc Span (deg)", "gItemEditor.Cape.ArcSpan", 10.0f, 360.0f, 180.0f,
      "Angle the 12 strand roots are spread over. 180 is the vanilla half-circle\n"
      "around the shoulders; lower values bunch the cape into a narrower cloak,\n"
      "higher values wrap it further around the body." },
    { "Arc Bulge", "gItemEditor.Cape.ArcBulge", 0.0f, 4.0f, 1.0f,
      "Depth of the parabolic arc the roots trace - how far the middle of the cape\n"
      "bows out behind Link. 0 flattens the arc into a straight line across the\n"
      "shoulders." },
    { "Arc Spread", "gItemEditor.Cape.ArcSpread", 0.0f, 4.0f, 1.0f, "Width of that same arc along the shoulder line." },
};

// Placement & rotation ---------------------------------------------------
const CapeFloatParam kCapePlacementParams[] = {
    { "Offset Back/Forward", "gItemEditor.Cape.OffsetX", -50.0f, 50.0f, 0.0f,
      "Shifts the whole attachment arc backwards (+) or into Link's back (-)." },
    { "Offset Up/Down", "gItemEditor.Cape.OffsetY", -50.0f, 50.0f, 0.0f,
      "Raises (+) or lowers (-) the attachment arc along Link's spine." },
    { "Offset Left/Right", "gItemEditor.Cape.OffsetZ", -50.0f, 50.0f, 0.0f,
      "Slides the attachment arc sideways along the shoulder line." },
    { "Yaw (deg)", "gItemEditor.Cape.Yaw", -180.0f, 180.0f, 0.0f,
      "Turns the cape around Link's vertical axis, on top of the angle derived\n"
      "from his shoulders." },
    { "Pitch (deg)", "gItemEditor.Cape.Pitch", -180.0f, 180.0f, 0.0f, "Tips the cape forwards/backwards." },
    { "Roll (deg)", "gItemEditor.Cape.Roll", -180.0f, 180.0f, 0.0f, "Banks the cape sideways." },
};

// Physics ----------------------------------------------------------------
const CapeFloatParam kCapePhysicsParams[] = {
    { "Gravity", "gItemEditor.Cape.Gravity", -20.0f, 5.0f, -3.0f,
      "Downward pull applied to every joint each tick. Vanilla: -3.0.\n"
      "Positive values make the cape float upwards." },
    { "Back Push", "gItemEditor.Cape.BackPush", -20.0f, 20.0f, -4.0f,
      "Constant push away from Link's back that keeps the cloth from clipping\n"
      "into him while standing still. Vanilla: -4.0." },
    { "Body Radius", "gItemEditor.Cape.MinDist", 0.0f, 60.0f, 8.0f,
      "How far from Link's center the cloth is held off. Raise it if the cape\n"
      "clips through a wider custom model. Vanilla: 8.0." },
    { "Floor Limit", "gItemEditor.Cape.FloorOffset", -400.0f, 0.0f, -200.0f,
      "Lowest the cloth may hang, relative to Link's feet. Vanilla: -200." },
    { "Back Sway", "gItemEditor.Cape.BackSway", 0.0f, 3.0f, 0.3f,
      "Backwards billow per unit of running speed. Vanilla: 0.3." },
    { "Side Sway", "gItemEditor.Cape.SideSway", 0.0f, 3.0f, 0.15f,
      "Sideways flutter per unit of running speed. Vanilla: 0.15." },
    { "Damping", "gItemEditor.Cape.Damping", 0.0f, 1.0f, 0.8f,
      "Fraction of a joint's velocity carried into the next tick. Lower = stiffer,\n"
      "higher = floatier and slower to settle. Vanilla: 0.8." },
    { "Velocity Clamp", "gItemEditor.Cape.VelClamp", 0.5f, 30.0f, 5.0f,
      "Per-axis speed limit for a joint. Keeps the cloth from exploding on hard\n"
      "camera cuts. Vanilla: 5.0." },
    { "Settle Rate", "gItemEditor.Cape.Decel", 0.0f, 1.0f, 0.1f,
      "How quickly leftover motion bleeds away when Link stops. Vanilla: 0.1." },
};

void ItemEditorCapeColorWidget() {
    float col[4] = {
        CVarGetInteger("gItemEditor.Cape.ColorR", 255) / 255.0f,
        CVarGetInteger("gItemEditor.Cape.ColorG", 255) / 255.0f,
        CVarGetInteger("gItemEditor.Cape.ColorB", 255) / 255.0f,
        CVarGetInteger("gItemEditor.Cape.ColorA", 255) / 255.0f,
    };

    if (ImGui::ColorEdit4("Cape Color##ItemEditorCape", col, ImGuiColorEditFlags_AlphaBar)) {
        CVarSetInteger("gItemEditor.Cape.ColorR", (int32_t)(col[0] * 255.0f + 0.5f));
        CVarSetInteger("gItemEditor.Cape.ColorG", (int32_t)(col[1] * 255.0f + 0.5f));
        CVarSetInteger("gItemEditor.Cape.ColorB", (int32_t)(col[2] * 255.0f + 0.5f));
        CVarSetInteger("gItemEditor.Cape.ColorA", (int32_t)(col[3] * 255.0f + 0.5f));
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    UIWidgets::Tooltip("Tints the Ganondorf cape texture. White = untouched.\n"
                       "Dropping the alpha below 255 also switches the cloth to a translucent\n"
                       "render mode so it actually blends instead of being drawn opaque.");
}

// Defined below; the popup's "Reset to Defaults" needs it before that point.
void ItemEditorResetCape();

void ItemEditorFloatSliders(const CapeFloatParam* params, size_t count, const char* fmt) {
    for (size_t i = 0; i < count; i++) {
        const CapeFloatParam& p = params[i];
        float v = CVarGetFloat(p.cvar, p.def);
        // ##cvar suffix: two groups can carry the same visible label, and ImGui keys
        // its state off the whole string — without it they would share one slider.
        std::string id = std::string(p.label) + "##" + p.cvar;
        if (ImGui::SliderFloat(id.c_str(), &v, p.min, p.max, fmt)) {
            CVarSetFloat(p.cvar, v);
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
        UIWidgets::Tooltip(p.tooltip);
    }
}

// One group of cape sliders. The cape's master switch gates them all, which is why the sliders are
// drawn disabled rather than hidden: OFF is meant to A/B against vanilla, not to hide the tune.
void ItemEditorCapePopupGroup(const char* heading, const CapeFloatParam* params, size_t count, const char* fmt) {
    ImGui::SeparatorText(heading);
    ImGui::BeginDisabled(CVarGetInteger("gItemEditor.Cape.Custom", 0) == 0);
    ItemEditorFloatSliders(params, count, fmt);
    ImGui::EndDisabled();
}

// El Cane of Byrna ya NO tiene nada en el Item Editor (2026-08-29). Su colocacion
// en la mano quedo dialada y horneada en BYRNA_CANE_* dentro de extended_equipment.c,
// y los gItemEditor.Byrna.* estan muertos: ni se escriben ni se leen. Para retocarla
// hay que editarla alli y recompilar. Mismo camino que el Trident y el Slate.

// The Magic Cape's 25-slider rig, in a window of its OWN rather than a modal. A modal is trapped
// inside the viewport and dies with the escape menu; dialling cloth means watching Link move with
// the sliders still reachable, so this one has to outlive the menu and be draggable off-screen.
class MagicCapeEditorWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {
    }
    void UpdateElement() override {
    }
    void DrawElement() override;
};

void MagicCapeEditorWindow::DrawElement() {
    bool custom = CVarGetInteger("gItemEditor.Cape.Custom", 0) != 0;

    if (ImGui::Checkbox("Enable Custom Cape Settings", &custom)) {
        CVarSetInteger("gItemEditor.Cape.Custom", custom);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    UIWidgets::Tooltip("Master switch. OFF (default) makes the cape use its built-in values and\n"
                       "ignore everything below, so you can A/B a tune against vanilla without\n"
                       "resetting anything.");

    ImGui::SameLine();
    if (ImGui::Button("Reset to Defaults")) {
        ItemEditorResetCape();
    }

    ImGui::BeginChild("capeScroll");
    ItemEditorCapePopupGroup("Shape & Size", kCapeShapeParams, ARRAY_COUNT(kCapeShapeParams), "%.2f");
    ItemEditorCapePopupGroup("Placement & Rotation", kCapePlacementParams, ARRAY_COUNT(kCapePlacementParams), "%.1f");
    ItemEditorCapePopupGroup("Physics", kCapePhysicsParams, ARRAY_COUNT(kCapePhysicsParams), "%.2f");

    ImGui::SeparatorText("Colour");
    ItemEditorCapeColorWidget();
    ImGui::EndChild();
}

std::shared_ptr<MagicCapeEditorWindow> sCapeEditorWindow = nullptr;

// Registered from the menu build, where the Gui already exists (SohMenu is itself a GuiWindow).
// Idempotent, so a second call is free.
void EnsureMagicCapeEditorWindow() {
    if (sCapeEditorWindow != nullptr) {
        return;
    }
    auto ctx = Ship::Context::GetRawInstance();
    if (ctx == nullptr || ctx->GetWindow() == nullptr) {
        return;
    }
    auto gui = ctx->GetWindow()->GetGui();
    if (gui == nullptr) {
        return;
    }
    sCapeEditorWindow =
        std::make_shared<MagicCapeEditorWindow>(CVAR_WINDOW("MagicCapeEditor"), "Magic Cape Editor", ImVec2(560, 620));
    gui->AddGuiWindow(sCapeEditorWindow);
}

void ItemEditorResetCape() {
    static const char* kIntCVars[] = { "gItemEditor.Cape.ColorR", "gItemEditor.Cape.ColorG", "gItemEditor.Cape.ColorB",
                                       "gItemEditor.Cape.ColorA" };

    for (const auto& p : kCapeShapeParams) {
        CVarSetFloat(p.cvar, p.def);
    }
    for (const auto& p : kCapePlacementParams) {
        CVarSetFloat(p.cvar, p.def);
    }
    for (const auto& p : kCapePhysicsParams) {
        CVarSetFloat(p.cvar, p.def);
    }
    for (const char* cvar : kIntCVars) {
        CVarSetInteger(cvar, 255);
    }
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

// (El PreFunc ItemEditorCapeGate desapareció con los sliders de la pestaña: dentro
// del popup el gris lo pone un BeginDisabled, que no necesita el sistema de widgets.)

struct NeiRandoSetting {
    const char* setting;
    const char* mirror;
    int32_t on;
};

const NeiRandoSetting kNeiRandoSettings[] = {
    { CVAR_RANDOMIZER_SETTING("SkijerCustomItems"), "gMods.CustomItems.Enabled", 1 },
    { CVAR_RANDOMIZER_SETTING("MmMasksAll"), "gMods.MmMasks.InventoryEnabled", 1 },
    { CVAR_RANDOMIZER_SETTING("ExtEquipment"), "gCheats.ExtEquip.Enabled", 1 },
    { CVAR_RANDOMIZER_SETTING("NeiWeaponUpgrades"), nullptr, 1 },
    { CVAR_RANDOMIZER_SETTING("SW97Spells"), "gEnhancements.SkijerNEI.SW97Medallions", 1 },
    { CVAR_RANDOMIZER_SETTING("ShuffleBombArrows"), "gMods.BombArrows.Mode", RO_BOMB_ARROWS_SHUFFLED },
    { CVAR_RANDOMIZER_SETTING("ElementalWandShuffle"), nullptr, RO_WAND_ELEMENTAL_SHUFFLE },
    // No mirror: both Crossover items share ONE feature toggle, so the generic mirror (which
    // copies the setting's value) would let unchecking either one switch the selector off while
    // the other item is still shuffled. NeiRando_EnableCrossover below only ever turns it on.
    { CVAR_RANDOMIZER_SETTING("CrossoverPokeball"), nullptr, 1 },
    { CVAR_RANDOMIZER_SETTING("CrossoverMarioMask"), nullptr, 1 },
};

static void NeiRando_EnableCrossover() {
    if (CVarGetInteger(CVAR_RANDOMIZER_SETTING("CrossoverPokeball"), 0) ||
        CVarGetInteger(CVAR_RANDOMIZER_SETTING("CrossoverMarioMask"), 0)) {
        CVarSetInteger("gBrokenItems.Enabled", 1);
    }
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void NeiRando_MirrorSetting(const NeiRandoSetting& s) {
    if (s.mirror != nullptr) {
        CVarSetInteger(s.mirror, CVarGetInteger(s.setting, 0));
    }
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void NeiRando_MirrorSettingByCVar(const char* setting) {
    for (const auto& s : kNeiRandoSettings) {
        if (strcmp(s.setting, setting) == 0) {
            NeiRando_MirrorSetting(s);
            return;
        }
    }
}

void NeiRando_SetAll(bool on) {
    for (const auto& s : kNeiRandoSettings) {
        CVarSetInteger(s.setting, on ? s.on : 0);
        NeiRando_MirrorSetting(s);
    }
    CVarSetInteger(CVAR_RANDOMIZER_SETTING("MmMasksTransform"), 0); // MmMasksAll already covers those 4
    NeiRando_EnableCrossover();
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

// ---- Fleet Ship Combo: live MM <-> OoT switch ----
// CVar namespace (matches user's "isFleetShipCombo[X]"):
//   isFleetShipCombo.Enabled - master toggle for the combo / launcher bootstrap
//   isPlayerIn2Ship          - persistent "where is the player": 1 = Majora's Mask
//                              (2ship), 0 = Ocarina of Time (Ship). Single source of
//                              truth for which game is active; also drives auto-start
//                              (the host resumes the game the player was last in).
// For now this button just flips and persists isPlayerIn2Ship. Frente B wires the
// actual seamless hand-off: pause the inactive game's SESSION (no logic/audio/render,
// process stays alive) + cross-process shared-texture compositing.
void AddFleetComboSection(WidgetPath& path) {
    mSohMenu->AddWidget(path, "Fleet Ship Combo (MM <-> OoT)", WIDGET_SEPARATOR_TEXT);

    mSohMenu->AddWidget(path, "Enable Fleet Ship Combo", WIDGET_CVAR_CHECKBOX)
        .CVar("isFleetShipCombo.Enabled")
        .RaceDisable(false)
        .Options(
            CheckboxOptions().Tooltip("Master switch for running OoT (Ship) and MM (2ship) together.\n\n"
                                      "RESTART REQUIRED after toggling: the second game is launched at boot.\n"
                                      "Place 2ship.exe in a '2ship' folder next to soh.exe (Ship/2ship/2ship.exe)."));

    // Live status so you always know which game you're in (and whether the combo is up).
    mSohMenu->AddWidget(path, "Combo Status", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        int32_t active = FleetShipCombo_GetActiveGame();
        if (active < 0) {
            ImGui::TextColored(ImVec4(0.95f, 0.7f, 0.3f, 1.0f), "Combo NOT running (single game).");
            ImGui::TextWrapped("Enable above + restart, and put 2ship.exe under Ship/2ship/.");
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "Active game: %s",
                               active == 1 ? "Majora's Mask (2ship)" : "Ocarina of Time (Ship)");
        }
    });

    mSohMenu->AddWidget(path, "Switch Active Game", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            // Use shared memory as the source of truth for the CURRENT active game so
            // the toggle stays correct no matter which process last switched.
            int32_t cur = FleetShipCombo_GetActiveGame();
            if (cur < 0) {
                // No shared region -> the combo isn't actually running two games.
                Notification::Emit({
                    .message = "Combo not running - enable it and restart (need 2ship.exe under Ship/2ship/).",
                });
                return;
            }
            int32_t next = cur ? 0 : 1; // 1 = MM (2ship), 0 = OoT (Ship)
            CVarSetInteger("isPlayerIn2Ship", next);
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            // Publish to shared memory so BOTH processes freeze/unfreeze + show/hide in sync.
            FleetShipCombo_SetActiveGame(next);
            FleetShipCombo_SetUiFocus(next); // front window follows the newly-active game
            SPDLOG_INFO("[FleetShipCombo] isPlayerIn2Ship -> {}",
                        next ? "Majora's Mask (2ship)" : "Ocarina of Time (Ship)");
            Notification::Emit({
                .message = next ? "Switching to Majora's Mask..." : "Switching to Ocarina of Time...",
            });
        })
        .Options(ButtonOptions().Tooltip(
            "Toggle the active game between Ocarina of Time and Majora's Mask at any time.\n"
            "Both games stay loaded and running; the inactive one's session is paused (seamless).\n\n"
            "Persists 'isPlayerIn2Ship' so the combo remembers which game you're in and can\n"
            "auto-start there next launch.\n\n"
            "NOTE: the runtime hand-off (session pause + cross-process compositing) is still\n"
            "being wired up. For now this just flips and remembers the active game."));

    mSohMenu->AddWidget(path, "Show 2ship UI overlay (trackers)", WIDGET_CVAR_CHECKBOX)
        .CVar("gFleetCombo.ShowMmUiOverlay")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Draws 2ship's floating UI windows (Check Tracker, etc.) on top of the game,\n"
            "no matter which game is active — so both games' trackers can be open at once.\n\n"
            "The overlay is view-only (an image): to CLICK 2ship's UI, use the\n"
            "'2 Ship 2 Harkinian' tab at the top of this menu.\n"
            "Open/close the windows themselves from 2ship's own menu.\n\n"
            "2ship-side publishing can be disabled with gFleetShipCombo.UiOverlay = 0\n"
            "(also restores the full render-skip for the inactive game)."));

    // The "View" game switcher (Ship / 2Ship / Shared) moved to the menu's top tab row
    // (DrawFleetShipComboTabs in Menu.cpp) so it's the same in both apps.
}

} // namespace

// =============================================================================
// "Skijer's NEI" - dedicated top-level menu gathering every NEI feature into
// its own tabs (Custom Items, Item Editor, Masks, Spells, Modes, Pak Loader,
// Randomizer, Controls). The tab list and its contents are kept 1:1 with 2ship's
// mm/2s2h/BenGui/BenMenu.cpp AddNEI() so a feature is always in the same place in
// both games; anything a game genuinely lacks (2ship has no .pak player models,
// Ship has no boss remains) is simply absent from that game's tab rather than
// shown as a dead toggle.
// Built as its own translation unit so the rest of Settings stays clean.
// Widgets are grouped by SIDEBAR (not file order), so each block below just
// (re)sets path.sidebarName to land in the right tab.
// =============================================================================

void RegisterNEIMenu() {
    WidgetPath path = { "Skijer's NEI", "Masks", SECTION_COLUMN_1 };

    mSohMenu->AddMenuEntry("Skijer's NEI", CVAR_SETTING("Menu.SkijerNEISidebarSection"));
    // Sidebar order is shared with 2ship's Skijer's NEI menu.
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Custom Items", 3);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Item Editor", 1);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Masks", 1);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Spells", 1);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Modes", 1);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Pak Loader", 3);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Randomizer", 1);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Controls", 1);
    path.sectionName = "Skijer's NEI";

    // ===================== Tab: Item Editor =====================
    path.sidebarName = "Item Editor";
    path.column = SECTION_COLUMN_1;
    mSohMenu->AddWidget(path, "Item Editor", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path,
                        "Live tuning for the custom items. Everything here writes gItemEditor.* CVars that "
                        "the item's behavior reads every frame, so changes apply instantly and are shared "
                        "with 2ship (the same preset works in both games). One section per item - more items "
                        "land here as they are made tunable.",
                        WIDGET_TEXT);

    // ── Dual Cane wheel shape (user 2026-08-06). Mirror of 2ship's control; the CVar is shared so
    // the option carries over between games. A base cane only leaves the wheel once its own
    // end-item is owned (Nei_CaneTypeVisible).
    mSohMenu->AddWidget(path, "Dual Cane", WIDGET_SEPARATOR_TEXT);
    std::map<int32_t, const char*> caneWheelModeOptions = {
        { 0, "Somaria - Trirod - Pacci - Ultrahand" },
        { 1, "Trirod - Ultrahand" },
        { 2, "Somaria - Trirod - Ultrahand" },
        { 3, "Trirod - Pacci - Ultrahand" },
    };
    mSohMenu->AddWidget(path, "Cane Wheel Shape", WIDGET_CVAR_COMBOBOX)
        .CVar("gItemEditor.CaneWheelMode")
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .ComboMap(caneWheelModeOptions)
                     .DefaultIndex(0)
                     .Tooltip("Which canes ride the shared wheel. A base cane is only hidden once\n"
                              "its own end-item (Trirod / Ultrahand) is owned, so the cell can\n"
                              "never lose the only cane you have."));

    // The Magic Cape is the last item still tuned live; the slate, rod, wand and Master Cycle poses
    // are baked into their own code. It pops out as a real window so it survives closing the menu
    // and can be dragged wherever it does not cover Link. Skijer's NEI
    mSohMenu->AddWidget(path, "Magic Cape", WIDGET_SEPARATOR_TEXT);
    EnsureMagicCapeEditorWindow();
    mSohMenu->AddWidget(path, "Popout Magic Cape Editor", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("MagicCapeEditor"))
        .WindowName("Magic Cape Editor")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Shape, placement, physics and colour of the Magic Cape's cloth.\n"
                                               "Everything applies live while the cape is worn."));

    mSohMenu->AddWidget(path, "Sheikah Sensor: Desired Items", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path, "Desired Items", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        DrawSensorDesirePicker();
    });

    // ===================== Tab: Modes =====================
    path.sidebarName = "Modes";
    path.column = SECTION_COLUMN_1;
    mSohMenu->AddWidget(path, "Crossover Items", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path, "Enable Crossover Items", WIDGET_CVAR_CHECKBOX)
        .CVar("gBrokenItems.Enabled")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Form selector (Link / Mario / Pikachu) on the equipment page's transform sub-page (L cycles).\n"
            "Mario needs the Mario Mask and Pikachu needs the Pokeball; both are randomizer items\n"
            "(see 'Include Mario Mask' / 'Include Pikachu Pokeball' in the Randomizer tab)."));

    // ===================== Tab: Custom Items =====================
    path.sidebarName = "Custom Items";
    path.column = SECTION_COLUMN_1;
    mSohMenu->AddWidget(path, "Custom Items", WIDGET_SEPARATOR_TEXT);

    // Gerudo demon mode. The axe placement is dialled and baked (GERUDO_AXE_* in
    // mm_player_form.cpp); what is left is a way INTO demon mode while the rest of its
    // moveset is being built. Skijer's NEI
    mSohMenu->AddWidget(path, "Gerudo Demon Mode", WIDGET_SEPARATOR_TEXT);
    // The axe placement is baked per animation family (sGerudoAxePlacements); only the way
    // INTO demon mode is left here, for building the rest of its moveset. Skijer's NEI
    mSohMenu->AddWidget(path, "Free demon mode (testing)", WIDGET_CVAR_CHECKBOX)
        .CVar("gItemEditor.GerudoAxe.FreeDemon")
        .Options(CheckboxOptions().Tooltip(
            "L enters demon mode with an empty meter and the fuel stops draining, so the axe moveset "
            "can be tested without farming the rage bar first. Turn it off to play normally."));

    // --- MM Quest Page (mirror of the 2ship-side OoT quest page). Skijer's NEI ---
    mSohMenu->AddWidget(path, "MM Quest Page", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path, "Songs: Pause Play (skip minigame)", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.SkijerNEI.PausePlay")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "On the MM Quest Status page (press L on the quest page to flip), if you hold an "
            "ocarina, pressing A on a learned song closes the menu and instantly plays it in-world "
            "(native success flow) INSTEAD of the learn-it minigame. OFF = the minigame."));

    path.column = SECTION_COLUMN_2;

    // NEI Weapon Upgrade appearance — pick which model/icon the progressive weapons display
    // for the levels that have two looks (DL + icon load from mm.o2r, with a vanilla fallback).
    mSohMenu->AddWidget(path, "Weapon Upgrade Appearance", WIDGET_SEPARATOR_TEXT);

    mSohMenu->AddWidget(path, "Gilded Sword: use Gilded look", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.SkijerNEI.GildedUsesGildedLook")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "When the Kokiri Sword is at its top upgrade (Gilded), choose the look:\n"
            "ON  = Gilded Sword model + icon.\n"
            "OFF = keep the Razor Sword look.\n"
            "(Razor level always shows the Razor look. DL/icon load from mm.o2r,\n"
            "falling back to the vanilla Kokiri Sword if unavailable.)"));

    mSohMenu->AddWidget(path, "Biggoron Upgrade: use Great Fairy's Sword look", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.SkijerNEI.BgsUsesGfsLook")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "When the Biggoron's Sword is upgraded (Great Fairy's Sword), choose the look:\n"
            "ON  = Great Fairy's Sword model + icon.\n"
            "OFF = keep the Biggoron's Sword look.\n"
            "(DL/icon load from mm.o2r, falling back to the vanilla Biggoron Sword.)"));

    mSohMenu->AddWidget(path, "Enable Extra Equipment", WIDGET_CVAR_CHECKBOX)
        .CVar("gCheats.ExtEquip.Enabled")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Adds 12 new equipment pieces (3 swords, 3 shields, 3 tunics, 3 boots).\n"
            "Press L on the equipment page to toggle between vanilla and extended equipment.\n"
            "(The 'Add Extended Equipment to Rando' toggle in the Randomizer tab controls\n"
            "whether they are shuffled into the seed.)"));

    // Net Model tuning sliders REMOVED: the in-hand placement is final and baked as constants in
    // object_net.c (Scale 0.49, Rot -5/-169/64, Offset 3.2/4.3/-1.0; catch radius 30). Skijer's NEI

    // Roc's Items MM Animations - requires mm.o2r
    path.column = SECTION_COLUMN_3;
    mSohMenu->AddWidget(path, "Roc's Items", WIDGET_SEPARATOR_TEXT);

    mSohMenu->AddWidget(path, "Roc's Items Use MM Animations", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.RocsItemsUseMmAnims")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!MmAssets_IsAvailable()) {
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0.";
            }
        })
        .Options(CheckboxOptions().Tooltip("Use MM animations for Roc's Feather and Roc's Cape jumps.\n"
                                           "Roc's Feather: Backflip on ground jump.\n"
                                           "Roc's Cape: Backflip on ground jump, roll jump on double jump.\n\n"
                                           "REQUIRES: mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0"));

    mSohMenu->AddWidget(path, "Invert Roc's Items Animations", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.RocsItems.InvertAnims")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!CVarGetInteger("gEnhancements.RocsItemsUseMmAnims", 0)) {
                info.options->disabled = true;
                info.options->disabledTooltip = "Enable 'Roc's Items Use MM Animations' first.";
            }
        })
        .Options(CheckboxOptions().Tooltip("Swaps the animation order for Roc's items.\n"
                                           "OFF: Ground = Backflip, Double = Roll Jump\n"
                                           "ON:  Ground = Roll Jump, Double = Backflip\n\n"
                                           "REQUIRES: Roc's Items Use MM Animations"));

    // NEI Aim Cycle — extends the vanilla BowArrowCycle cheat with L button
    // (previous direction), SW97 elemental arrow cycling, slingshot support,
    // and in-game Gust Jar element switching while in first-person aim.
    mSohMenu->AddWidget(path, "NEI Aim Cycle (R/L while aiming)", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.NeiAimCycle")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Cycle between elements while aiming. R = next, L = previous.\n"
                                           "Applies to:\n"
                                           "  - Bow & Slingshot: cycles vanilla arrows OR SW97 elemental arrows\n"
                                           "    (whichever type is currently equipped on the C-button).\n"
                                           "  - Gust Jar (in first-person, IDLE): cycles unlocked elements\n"
                                           "    based on owned medallions. Hold C + press L+R together to\n"
                                           "    cycle the element AND switch to a stored-blow that fires on\n"
                                           "    C release (no auto-discharge).\n"
                                           "  - Arrow wheel: now also includes Bombchus when you own any.\n\n"
                                           "Extends the vanilla 'Bow Arrow Cycle' cheat — both can be enabled\n"
                                           "at the same time without conflict."));

    // NEI Pictograph Box (MM port). Photographing a mapped OoT actor writes MM's pictoFlags0/1 +
    // the I5 photo into the NEI save in MM's exact layout, for a 2Ship bridge. No in-OoT reward.
    mSohMenu->AddWidget(path, "Pictograph Box", WIDGET_SEPARATOR_TEXT);
    // Both options mirror 2Ship's pictograph enhancements 1:1 — same CVar names, same defaults — so
    // the Pictograph Box behaves identically in Ocarina of Time and Majora's Mask. Skijer's NEI
    mSohMenu->AddWidget(path, "Better Picto Message", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.Equipment.BetterPictoMessage")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Inform the player what target if any is being captured in the\n"
            "pictograph (\"Keep this picture of a Pirate?\"). Same option, same\n"
            "default and same wording as 2Ship's Better Picto Message."));

    mSohMenu->AddWidget(path, "Color Pictograph", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.Items.ColorPictograph")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Display pictographs in color. OFF (the default) shows Majora's Mask's\n"
            "sepia photo, exactly like the original item."));

    // ===================== Tab: Spells =====================
    path.sidebarName = "Spells";
    path.column = SECTION_COLUMN_1;
    mSohMenu->AddWidget(path, "Spells & Spiritual Stones", WIDGET_SEPARATOR_TEXT);

    // SW97 Medallion Spells — merged with the old "Enable Sage Spells" rando
    // toggle. This one checkbox now drives BOTH the C-button medallion casting
    // (gEnhancements.SkijerNEI.SW97Medallions) and the seed-locked elemental-
    // damage rando setting (RSK_SW97_SPELLS via CVAR_RANDOMIZER_SETTING).
    mSohMenu->AddWidget(path, "SW97 Medallion Spells", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.SkijerNEI.SW97Medallions")
        .RaceDisable(false)
        .PostFunc([](WidgetInfo& info) {
            CVarSetInteger(CVAR_RANDOMIZER_SETTING("SW97Spells"),
                           CVarGetInteger("gEnhancements.SkijerNEI.SW97Medallions", 0));
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Equip quest medallions to C-buttons from Quest Status.\n"
            "C = cast elemental spell, L+C = set elemental arrow/slingshot.\n"
            "Adult: elemental arrows. Child: elemental slingshot seeds.\n"
            "Also enables the seed-locked elemental-damage setting (Spell + Projectile\n"
            "paths), synced with 'Sage Spells' in the Randomizer menu.\n\n"
            "Credit: z64proto/sw97 team (spell/arrow actors)"));

    mSohMenu->AddWidget(path, "Enable Spiritual Stones", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.SpiritualStones.Enabled")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Master toggle for the custom Spiritual Stones behavior.\n"
            "ON: each owned stone toggles (A on the Quest page) a passive buff\n"
            "(Kokiri = faster walk, Goron = climb, Zora = swim), and held on a C/D-pad slot\n"
            "sets/warps to a saved waypoint statue.\n"
            "OFF: the stones behave like vanilla (no custom behavior)."));

    // ===================== Tab: Masks =====================
    path.sidebarName = "Masks";
    path.column = SECTION_COLUMN_1;
    mSohMenu->AddWidget(path, "Mask Transformations", WIDGET_SEPARATOR_TEXT);

    mSohMenu->AddWidget(path, "Kafei Mask Transform", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.KafeiMaskTransform")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Wearing the Kafei Mask transforms Link into Kafei.\n"
                                           "Adult Link becomes Adult Kafei, Child Link becomes Child Kafei.\n"
                                           "Remove the mask to revert.\n\n"
                                           "Model ships inside soh.o2r (objects/forms/kafei)."));

    mSohMenu->AddWidget(path, "Keaton Mask Transform", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.KeatonMaskTransform")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Wearing the Keaton Mask transforms Link into a Keaton (visual form).\n"
            "Remove the mask to revert. If the Keaton model isn't shipped yet the\n"
            "mask falls back to plain cosmetic wear.\n\n"
            "Model ships inside soh.o2r (objects/forms/keaton)."));

    mSohMenu->AddWidget(path, "Gerudo Mask Transform", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.GerudoMaskTransform")
        .RaceDisable(false)
        .Options(
            CheckboxOptions().Tooltip("Wearing the vanilla Gerudo Mask transforms Link into a Gerudo — uses a\n"
                                      "Link-rigged gerudo mesh, so all of Link's animations and equipment work\n"
                                      "exactly as normal. The body just looks gerudo.\n"
                                      "Effects while the mask is worn:\n"
                                      "  - Haunted Wasteland sandstorm is suppressed (no more getting lost).\n"
                                      "  - All Gerudos treat you as a fellow Gerudo (Ge1/Ge2/Ge3 are friendly,\n"
                                      "    GTG guard lets you in). Access is TEMPORARY — no Gerudo Card is granted.\n"
                                      "Remove the mask to revert.\n\n"
                                      "Model ships inside soh.o2r (objects/forms/gerudo, Link-21-bone-rigged\n"
                                      "gerudo skin + 11 baked gerudo anims, visible in the anim viewer)."));

    // Rito Mask: gated by gMods.RitoForm (default ON). The mask item itself shares
    // the Farore's Wind cell (press A on it in the pause menu and cycle with the
    // stick), so this toggle only decides whether using it transforms.
    mSohMenu->AddWidget(path, "Rito Mask Transform", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.RitoForm")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Using the Rito Mask transforms Link into a Rito — full transformation\n"
            "cutscene (freeze, mask SFX, white flash) like the MM masks, then a\n"
            "Link-rigged bird body. Every gameplay system stays vanilla Link:\n"
            "sword, shield, items, climbing and swimming all behave normally and\n"
            "the body just plays Link's own animations.\n"
            "Use the mask again to change back.\n\n"
            "The mask shares the Farore's Wind cell: put the cursor on it, press A\n"
            "and move the stick to swap between the spell and the mask.\n\n"
            "Model ships inside soh.o2r (objects/forms/rito)."));

    // Garo Mask: gated by gMods.GaroMaskTransform (default ON). When OFF, the
    // Garo Mask stays a cosmetic mask (no transformation), matching the Gerudo
    // opt-out. Enforced in mm_player_form.cpp (MmForm_GetMaskType / HandleMaskUse).
    mSohMenu->AddWidget(path, "Garo Mask Transform", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.GaroMaskTransform")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Wearing the Garo Mask transforms Link into a Garo (custom skin\n"
            "with a slash-combo + shuriken finisher). Link's normal gameplay still runs\n"
            "1:1 — only the look and the slash combo change.\n"
            "OFF: the Garo Mask draws as a plain cosmetic mask (no transformation).\n\n"
            "Model ships inside soh.o2r (objects/forms/garo)."));

    mSohMenu->AddWidget(path, "MM Masks", WIDGET_SEPARATOR_TEXT);

    // Merged option: "Include MM Masks Inventory" + "Extra Mask Effects" are now
    // a single toggle. Enabling the MM masks page also enables the per-mask
    // visual effects and the transformation system.
    mSohMenu->AddWidget(path, "Include MM Masks (Inventory + Effects)", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.MmMasks.InventoryEnabled")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!MmAssets_IsAvailable()) {
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0.";
            }
        })
        .PostFunc([](WidgetInfo& info) {
            if (CVarGetInteger("gMods.MmMasks.InventoryEnabled", 1)) {
                CVarSetInteger("gMods.TransformMasks.Enabled", 1);
            }
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Adds a 3rd inventory page with all 24 MM masks AND enables each mask's\n"
            "custom visual effects. Transformation masks (Deku, Goron, Zora, Fierce\n"
            "Deity) trigger transformations. Removes OOT Goron/Zora masks from the\n"
            "randomizer pool.\n\n"
            "REQUIRES: mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0"));

    mSohMenu->AddWidget(path, "Enable Transformation Masks", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.TransformMasks.Enabled")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!MmAssets_IsAvailable()) {
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0.\n"
                                                "Download 2Ship, extract your MM ROM, then copy mm.o2r here.";
            }
        })
        .Options(CheckboxOptions().Tooltip("Allows you to transform with certain masks like in Majora's Mask.\n"
                                           "Equip transformation masks from the MM Masks inventory page.\n\n"
                                           "REQUIRES: mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0"));

    mSohMenu->AddWidget(path, "Instant Transform", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.TransformMasks.InstantTransform")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!MmAssets_IsAvailable()) {
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0.";
            } else if (!CVarGetInteger("gMods.MmMasks.InventoryEnabled", 1)) {
                info.options->disabled = true;
                info.options->disabledTooltip = "Enable 'Include MM Masks' first.";
            }
        })
        .Options(CheckboxOptions().Tooltip("Skip the transformation cutscene animation.\n"
                                           "Transform instantly when equipping a transformation mask.\n\n"
                                           "REQUIRES: Include MM Masks + mm.o2r"));

    mSohMenu->AddWidget(path, "Instant Blast Mask", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.BlastMask.Instant")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Removes the cooldown on Blast Mask.\n"
                                           "Normally there is a 310-frame (~5 second) cooldown between uses."));

    mSohMenu->AddWidget(path, "Invisible Non-Transformation Masks", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("HideNonTransformationMasks"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Turns all MM non-transformation masks invisible while still maintaining their effects.\n"
            "Transformation masks (Deku, Goron, Zora, Fierce Deity) remain visible.\n"
            "Only affects MM masks; vanilla OOT child masks are unaffected (use Invisible Bunny Hood for OOT bunny "
            "hood)."));

    // Mute MM Audio (moved here from the Spells tab — it's a mask/MM-assets option).
    mSohMenu->AddWidget(path, "Mute MM Audio", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.SkijerNEI.MuteMmAudio")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip("Mute all sounds from MM (mm.o2r).\n"
                                                              "Transformation mask SFX, voices, and instruments\n"
                                                              "will be silenced. OOT sounds play instead."));

    // ===================== Tab: Pak Loader =====================
    path.sidebarName = "Pak Loader";
    path.column = SECTION_COLUMN_1;
    mSohMenu->AddWidget(path, "Custom Models (.pak)", WIDGET_SEPARATOR_TEXT);

    // Build model combobox maps per age (triggers lazy init of PakLoader)
    {
        s32 pakCount = PakLoader_GetModelCount();

        // Adult models
        std::map<int32_t, const char*> adultModelMap;
        adultModelMap[-1] = "Default Link";
        for (s32 i = 0; i < pakCount; i++) {
            if (PakLoader_ModelHasAdult(i)) {
                adultModelMap[i] = PakLoader_GetModelLabel(i);
            }
        }

        // Child models
        std::map<int32_t, const char*> childModelMap;
        childModelMap[-1] = "Default Link";
        for (s32 i = 0; i < pakCount; i++) {
            if (PakLoader_ModelHasChild(i)) {
                childModelMap[i] = PakLoader_GetModelLabel(i);
            }
        }

        mSohMenu->AddWidget(path, "Enable Custom Player Model", WIDGET_CVAR_CHECKBOX)
            .CVar("gMods.PakLoader.Enabled")
            .RaceDisable(false)
            .PreFunc([](WidgetInfo& info) {
                if (PakLoader_GetModelCount() == 0) {
                    info.options->disabled = true;
                    info.options->disabledTooltip = "No .pak files found.\n"
                                                    "Place ModLoader64 .pak model files in the mods/ folder.";
                }
            })
            .PostFunc([](WidgetInfo& info) {
                if (CVarGetInteger("gMods.PakLoader.Enabled", 0)) {
                    s32 adultIdx = CVarGetInteger("gMods.PakLoader.AdultModel", -1);
                    s32 childIdx = CVarGetInteger("gMods.PakLoader.ChildModel", -1);
                    PakLoader_SelectAdultModel(adultIdx);
                    PakLoader_SelectChildModel(childIdx);
                } else {
                    PakLoader_SelectAdultModel(-1);
                    PakLoader_SelectChildModel(-1);
                }
            })
            .Options(CheckboxOptions().Tooltip("Replaces Link's model with a custom model from a .pak file.\n"
                                               "Place ModLoader64 zzplayas .pak files in the mods/ folder.\n"
                                               "You can choose different models for Adult and Child Link."));

        mSohMenu->AddWidget(path, "Adult Link Model", WIDGET_CVAR_COMBOBOX)
            .CVar("gMods.PakLoader.AdultModel")
            .RaceDisable(false)
            .PreFunc([](WidgetInfo& info) {
                if (PakLoader_GetModelCount() == 0 || !CVarGetInteger("gMods.PakLoader.Enabled", 0)) {
                    info.options->disabled = true;
                }
                // Rebuild the comboMap from current sModels every frame so the
                // dropdown stays in sync if paks load after RegisterNEIMenu ran
                // (lazy init), or if Force* added entries at runtime. Also
                // clamp the CVar to -1 if it points outside the rebuilt map —
                // otherwise Combobox<int>::at() throws std::out_of_range and
                // crashes the renderer.
                auto opt = std::static_pointer_cast<UIWidgets::ComboboxOptions>(info.options);
                opt->comboMap.clear();
                opt->comboMap[-1] = "Default Link";
                s32 n = PakLoader_GetModelCount();
                for (s32 i = 0; i < n; i++) {
                    if (PakLoader_ModelHasAdult(i)) {
                        opt->comboMap[i] = PakLoader_GetModelLabel(i);
                    }
                }
                s32 v = CVarGetInteger("gMods.PakLoader.AdultModel", -1);
                if (v >= 0 && !opt->comboMap.count(v)) {}
            })
            .PostFunc([](WidgetInfo& info) {
                if (CVarGetInteger("gMods.PakLoader.Enabled", 0)) {
                    PakLoader_SelectAdultModel(CVarGetInteger("gMods.PakLoader.AdultModel", -1));
                }
            })
            .Options(ComboboxOptions()
                         .ComboMap(adultModelMap)
                         .DefaultIndex(-1)
                         .Tooltip("Choose a custom model for Adult Link."));

        mSohMenu->AddWidget(path, "Child Link Model", WIDGET_CVAR_COMBOBOX)
            .CVar("gMods.PakLoader.ChildModel")
            .RaceDisable(false)
            .PreFunc([](WidgetInfo& info) {
                if (PakLoader_GetModelCount() == 0 || !CVarGetInteger("gMods.PakLoader.Enabled", 0)) {
                    info.options->disabled = true;
                }
                auto opt = std::static_pointer_cast<UIWidgets::ComboboxOptions>(info.options);
                opt->comboMap.clear();
                opt->comboMap[-1] = "Default Link";
                s32 n = PakLoader_GetModelCount();
                for (s32 i = 0; i < n; i++) {
                    if (PakLoader_ModelHasChild(i)) {
                        opt->comboMap[i] = PakLoader_GetModelLabel(i);
                    }
                }
                s32 v = CVarGetInteger("gMods.PakLoader.ChildModel", -1);
                if (v >= 0 && !opt->comboMap.count(v)) {}
            })
            .PostFunc([](WidgetInfo& info) {
                if (CVarGetInteger("gMods.PakLoader.Enabled", 0)) {
                    PakLoader_SelectChildModel(CVarGetInteger("gMods.PakLoader.ChildModel", -1));
                }
            })
            .Options(ComboboxOptions()
                         .ComboMap(childModelMap)
                         .DefaultIndex(-1)
                         .Tooltip("Choose a custom model for Child Link."));

        // Equipment Pack lists ANY pak that has at least one equipment DL —
        // dedicated zzequipment paks AND Combined paks (body + equipment) both
        // qualify. Selecting a Combined pak here uses its equipment alias
        // table; the body model side is controlled separately by the Adult /
        // Child dropdowns above.
        std::map<int32_t, const char*> equipModelMap;
        equipModelMap[-1] = "Default Equipment";
        for (s32 i = 0; i < pakCount; i++) {
            if (PakLoader_ModelHasAnyEquipment(i)) {
                equipModelMap[i] = PakLoader_GetModelLabel(i);
            }
        }

        if (equipModelMap.size() > 1) { // More than just "Default"
            mSohMenu->AddWidget(path, "Equipment Pack", WIDGET_CVAR_COMBOBOX)
                .CVar("gMods.PakLoader.Equipment")
                .RaceDisable(false)
                .PreFunc([](WidgetInfo& info) {
                    auto opt = std::static_pointer_cast<UIWidgets::ComboboxOptions>(info.options);
                    opt->comboMap.clear();
                    opt->comboMap[-1] = "Default Equipment";
                    s32 n = PakLoader_GetModelCount();
                    for (s32 i = 0; i < n; i++) {
                        if (PakLoader_ModelHasAnyEquipment(i)) {
                            opt->comboMap[i] = PakLoader_GetModelLabel(i);
                        }
                    }
                    s32 v = CVarGetInteger("gMods.PakLoader.Equipment", -1);
                    if (v >= 0 && !opt->comboMap.count(v)) {}
                })
                .PostFunc([](WidgetInfo& info) {
                    // Equipment works independently of the body-model toggle —
                    // pak_loader resolves vanilla fists/hands at draw time so an
                    // equipment-only selection (e.g. just a custom sword) is
                    // valid even when Enable Custom Player Model is off.
                    PakLoader_SelectEquipment(CVarGetInteger("gMods.PakLoader.Equipment", -1));
                })
                .Options(ComboboxOptions()
                             .ComboMap(equipModelMap)
                             .DefaultIndex(-1)
                             .Tooltip("Choose a custom equipment pack.\n"
                                      "Replaces swords, shields, and other items.\n"
                                      "Works on its own — you do NOT have to enable Custom Player Model."));
        }

        // ----- Voice Packs (Z64Online .pak with sounds/<HEX>/*.ogg) -----
        mSohMenu->AddWidget(path, "Custom Link Voice", WIDGET_SEPARATOR_TEXT);

        std::map<int32_t, const char*> voicePackMap;
        voicePackMap[-1] = "None";
        for (s32 i = 0; i < VoicePack_GetCount(); i++) {
            voicePackMap[i] = VoicePack_GetName(i);
        }

        mSohMenu->AddWidget(path, "Enable Custom Voice", WIDGET_CVAR_CHECKBOX)
            .CVar("gMods.VoicePack.Enabled")
            .RaceDisable(false)
            .PreFunc([](WidgetInfo& info) {
                if (VoicePack_GetCount() == 0) {
                    info.options->disabled = true;
                    info.options->disabledTooltip =
                        "No voice packs found.\nPlace Z64Online voice .pak files in the mods/ folder.";
                }
            })
            .PostFunc([](WidgetInfo& info) {
                if (CVarGetInteger("gMods.VoicePack.Enabled", 0)) {
                    VoicePack_Select(CVarGetInteger("gMods.VoicePack.Selection", -1));
                } else {
                    VoicePack_Select(-1);
                }
            })
            .Options(CheckboxOptions().Tooltip("Replaces Link's voice grunts (sword swings, falls, damage, etc.)\n"
                                               "with samples from a Z64Online-format voice pak.\n"
                                               "Voice samples play as 2D audio (no positional attenuation)."));

        if (voicePackMap.size() > 1) {
            mSohMenu->AddWidget(path, "Voice Pack", WIDGET_CVAR_COMBOBOX)
                .CVar("gMods.VoicePack.Selection")
                .RaceDisable(false)
                .PreFunc([](WidgetInfo& info) {
                    if (!CVarGetInteger("gMods.VoicePack.Enabled", 0)) {
                        info.options->disabled = true;
                    }
                    auto opt = std::static_pointer_cast<UIWidgets::ComboboxOptions>(info.options);
                    opt->comboMap.clear();
                    opt->comboMap[-1] = "None";
                    s32 n = VoicePack_GetCount();
                    for (s32 i = 0; i < n; i++) {
                        const char* nm = VoicePack_GetName(i);
                        opt->comboMap[i] = nm ? nm : "(unnamed)";
                    }
                    s32 v = CVarGetInteger("gMods.VoicePack.Selection", -1);
                    if (v >= 0 && !opt->comboMap.count(v)) {}
                })
                .PostFunc([](WidgetInfo& info) {
                    if (CVarGetInteger("gMods.VoicePack.Enabled", 0)) {
                        VoicePack_Select(CVarGetInteger("gMods.VoicePack.Selection", -1));
                    }
                })
                .Options(ComboboxOptions()
                             .ComboMap(voicePackMap)
                             .DefaultIndex(-1)
                             .Tooltip("Choose a voice pack.\n"
                                      "Selecting a pack decodes its OGG samples (lazy, ~one-time cost)."));

            mSohMenu->AddWidget(path, "Voice Pack Volume", WIDGET_CVAR_SLIDER_FLOAT)
                .CVar("gMods.VoicePack.Volume")
                .RaceDisable(false)
                .PreFunc([](WidgetInfo& info) {
                    if (!CVarGetInteger("gMods.VoicePack.Enabled", 0)) {
                        info.options->disabled = true;
                    }
                })
                .Options(FloatSliderOptions()
                             .Tooltip("Mix gain for voice pack samples.")
                             .Min(0.0f)
                             .Max(2.0f)
                             .DefaultValue(1.0f)
                             .IsPercentage());
        }
    }

    // ----- Equipment Mix: swords+shields / ranged+tools+boots / child masks -----
    path.column = SECTION_COLUMN_1;

    mSohMenu->AddWidget(path, "Per-slot equipment override", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path,
                        "Each slot can pull from a different pak. 'Default' inherits from the main "
                        "Equipment Pack dropdown (or vanilla if no pack selected). Sheathed and "
                        "unsheathed pieces always come from the same source pak so the look stays "
                        "consistent.",
                        WIDGET_TEXT);

    mSohMenu->AddWidget(path, "Reset all slots to Default", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            s32 n = PakLoader_GetSlotCount();
            char cvarName[96];
            for (s32 i = 0; i < n; i++) {
                snprintf(cvarName, sizeof(cvarName), "gMods.PakLoader.SlotMix.%s", PakLoader_GetSlotKey(i));
                CVarSetInteger(cvarName, -1);
                PakLoader_SetSlotMix(i, -1);
            }
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(ButtonOptions().Size(Sizes::Inline).Tooltip("Clear every per-slot override at once."));

    // Cached CVar-name strings per slot so the widgets get stable c_str() pointers.
    // RegisterNEIMenu is called once at boot, so static storage is fine.
    static std::vector<std::string> slotCVarNames;
    {
        s32 n = PakLoader_GetSlotCount();
        slotCVarNames.clear();
        slotCVarNames.reserve(n);
        for (s32 i = 0; i < n; i++) {
            slotCVarNames.emplace_back(std::string("gMods.PakLoader.SlotMix.") + PakLoader_GetSlotKey(i));
        }
    }

    // Slot-grouping for placement across the three columns. Indices must match
    // sSlotGroups[] order in pak_loader.cpp.
    //  0: Sword0 (Kokiri)        1: Sword1 (Master)      2: Sword2 (Biggoron)
    //  3: Shield0 (Deku)         4: Shield1 (Hylian)     5: Shield2 (Mirror)
    //  6: Bow                    7: Hookshot             8: Slingshot
    //  9: Boomerang             10: Hammer              11: DekuStick
    // 12: Bottle                13: OcarinaFairy        14: OcarinaTime
    // 15: IronBoots             16: HoverBoots          17: Gauntlets
    // 18: Bracelet              19: MaskSkull           20: MaskSpooky
    // 21: MaskKeaton            22: MaskTruth           23: MaskGoron
    // 24: MaskZora              25: MaskGerudo          26: MaskBunny

    auto addSlotWidget = [&path](s32 slotIdx) {
        const char* label = PakLoader_GetSlotLabel(slotIdx);
        const char* cvarName = slotCVarNames[slotIdx].c_str();

        mSohMenu->AddWidget(path, label, WIDGET_CVAR_COMBOBOX)
            .CVar(cvarName)
            .RaceDisable(false)
            .PreFunc([slotIdx](WidgetInfo& info) {
                // Rebuild every frame so newly-loaded paks appear immediately
                // and removed paks disappear without a restart. Clamp the CVar
                // to -1 if it ends up pointing at a slot the chosen pak no
                // longer satisfies — defends against std::map::at out_of_range
                // crashes in Combobox<int>.
                auto opt = std::static_pointer_cast<UIWidgets::ComboboxOptions>(info.options);
                opt->comboMap.clear();
                opt->comboMap[-1] = "Default (inherit)";
                s32 n = PakLoader_GetModelCount();
                for (s32 i = 0; i < n; i++) {
                    if (PakLoader_PakProvidesSlot(i, slotIdx)) {
                        opt->comboMap[i] = PakLoader_GetModelLabel(i);
                    }
                }
                char cvarName[96];
                snprintf(cvarName, sizeof(cvarName), "gMods.PakLoader.SlotMix.%s", PakLoader_GetSlotKey(slotIdx));
                s32 v = CVarGetInteger(cvarName, -1);
                if (v >= 0 && !opt->comboMap.count(v)) {
                    CVarSetInteger(cvarName, -1);
                }
            })
            .PostFunc([slotIdx](WidgetInfo& info) {
                char cvarName[96];
                snprintf(cvarName, sizeof(cvarName), "gMods.PakLoader.SlotMix.%s", PakLoader_GetSlotKey(slotIdx));
                PakLoader_SetSlotMix(slotIdx, CVarGetInteger(cvarName, -1));
            })
            .Options(ComboboxOptions().DefaultIndex(-1).Tooltip("Pak that provides this piece. 'Default' = inherit "
                                                                "from the Equipment Pack dropdown."));
    };

    // Column 1 — Swords (0..2) + Shields (3..5)
    mSohMenu->AddWidget(path, "Swords", WIDGET_SEPARATOR_TEXT);
    for (s32 i = 0; i <= 2; i++)
        addSlotWidget(i);
    mSohMenu->AddWidget(path, "Shields", WIDGET_SEPARATOR_TEXT);
    for (s32 i = 3; i <= 5; i++)
        addSlotWidget(i);

    // Column 2 — Ranged + Tools + Boots + Gauntlets + Bracelet (6..18)
    path.column = SECTION_COLUMN_2;
    mSohMenu->AddWidget(path, "Ranged & Tools", WIDGET_SEPARATOR_TEXT);
    for (s32 i = 6; i <= 11; i++)
        addSlotWidget(i); // Bow..DekuStick
    mSohMenu->AddWidget(path, "Items", WIDGET_SEPARATOR_TEXT);
    addSlotWidget(12); // Bottle
    addSlotWidget(13); // OcarinaFairy
    addSlotWidget(14); // OcarinaTime
    mSohMenu->AddWidget(path, "Boots & Gauntlets", WIDGET_SEPARATOR_TEXT);
    addSlotWidget(15); // IronBoots
    addSlotWidget(16); // HoverBoots
    addSlotWidget(17); // Gauntlets
    addSlotWidget(18); // Bracelet

    // Column 3 — Child masks (19..26)
    path.column = SECTION_COLUMN_3;
    mSohMenu->AddWidget(path, "Child Masks", WIDGET_SEPARATOR_TEXT);
    for (s32 i = 19; i < PakLoader_GetSlotCount(); i++)
        addSlotWidget(i);

    // ===================== Tab: Randomizer =====================
    path.sidebarName = "Randomizer";
    path.column = SECTION_COLUMN_1;

    if (FleetShipCombo_ShowMenuUi()) {
        AddFleetComboSection(path);
    }

    mSohMenu->AddWidget(path, "Randomizer (seed-locked)", WIDGET_SEPARATOR_TEXT);

    mSohMenu->AddWidget(path, "Add all NEI content to rando", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) { NeiRando_SetAll(true); })
        .Options(ButtonOptions().Tooltip("Turns on every setting below at once: custom items, MM masks,\n"
                                         "extended equipment, weapon upgrades, spells, bomb arrows and the\n"
                                         "elemental wand — plus the in-game systems they need."));

    mSohMenu->AddWidget(path, "Remove all NEI content from rando", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) { NeiRando_SetAll(false); })
        .Options(ButtonOptions().Tooltip("Turns every setting below off."));

    mSohMenu->AddWidget(path, "Enable Custom Items", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("SkijerCustomItems"))
        .PostFunc([](WidgetInfo& info) { NeiRando_MirrorSettingByCVar(CVAR_RANDOMIZER_SETTING("SkijerCustomItems")); })
        .Options(CheckboxOptions().Tooltip(
            "Enables the 24 custom items on the second inventory page (seed-locked rando setting).\n"
            "When enabled, these items are also added to the randomizer pool and gated logic paths.\n"
            "When disabled, page 2 is inaccessible and items are not in rando.\n"
            "Synced with the same setting in the Randomizer menu."));

    mSohMenu->AddWidget(path, "Add All MM Masks to Rando", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("MmMasksAll"))
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!MmAssets_IsAvailable()) {
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0.";
            } else if (!CVarGetInteger("gMods.MmMasks.InventoryEnabled", 1)) {
                info.options->disabled = true;
                info.options->disabledTooltip = "Enable 'Include MM Masks' first.";
            }
        })
        .PostFunc([](WidgetInfo& info) {
            if (CVarGetInteger(CVAR_RANDOMIZER_SETTING("MmMasksAll"), 0)) {
                CVarSetInteger(CVAR_RANDOMIZER_SETTING("MmMasksTransform"), 0);
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            }
        })
        .Options(CheckboxOptions().Tooltip("Adds all 24 MM masks to the randomizer item pool.\n"
                                           "Masks can be found at random locations like custom items.\n"
                                           "Removes OOT Goron/Zora masks from pool.\n\n"
                                           "Seed-locked rando setting.\n"
                                           "REQUIRES: 'Include MM Masks' enabled"));

    mSohMenu->AddWidget(path, "Add Transformation Masks to Rando", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("MmMasksTransform"))
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!MmAssets_IsAvailable()) {
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0.";
            } else if (!CVarGetInteger("gMods.MmMasks.InventoryEnabled", 1)) {
                info.options->disabled = true;
                info.options->disabledTooltip = "Enable 'Include MM Masks' first.";
            } else if (CVarGetInteger(CVAR_RANDOMIZER_SETTING("MmMasksAll"), 0)) {
                info.options->disabled = true;
                info.options->disabledTooltip = "'Add All MM Masks to Rando' already includes transformation masks.";
            }
        })
        .Options(CheckboxOptions().Tooltip("Adds only the 4 transformation masks (Deku, Goron, Zora, Fierce Deity)\n"
                                           "to the randomizer item pool.\n"
                                           "Removes OOT Goron/Zora masks from pool.\n\n"
                                           "Seed-locked rando setting.\n"
                                           "REQUIRES: 'Include MM Masks' enabled"));

    mSohMenu->AddWidget(path, "Add Extended Equipment to Rando", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("ExtEquipment"))
        .RaceDisable(false)
        .PostFunc([](WidgetInfo& info) { NeiRando_MirrorSettingByCVar(CVAR_RANDOMIZER_SETTING("ExtEquipment")); })
        .Options(CheckboxOptions().Tooltip(
            "Adds the 12 extended equipment pieces (3 swords, 3 shields, 3 tunics, 3 boots) to the randomizer pool.\n"
            "Press L on the equipment page to toggle between vanilla and extended equipment.\n\n"
            "Seed-locked rando setting (also enables the in-game equipment system)."));

    mSohMenu->AddWidget(path, "Add NEI Weapon Upgrades to Rando", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("NeiWeaponUpgrades"))
        .RaceDisable(false)
        .PostFunc([](WidgetInfo& info) {
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(CheckboxOptions().Tooltip(
            "Adds NEI weapon upgrades to the randomizer pool. Each requires its base weapon:\n"
            "  - Hammer Upgrade (Iron Knuckle's Axe): double damage/reach + tomahawk throw\n"
            "  - Kokiri Sword Upgrade x2: Razor Sword, then Gilded Sword\n"
            "  - True Master Sword (Master Sword)\n"
            "  - Great Fairy's Sword (Biggoron Sword)\n\n"
            "Only the Hammer upgrade has gameplay behavior for now.\n"
            "Seed-locked rando setting."));

    // Bomb Arrows no longer live in the inventory — they are the last entry of the bow's element
    // wheel. This dropdown replaces the old "Auto-grant with Bomb Bag" checkbox: that behavior is
    // the middle value now. Like SkijerCustomItems, the seed-locked setting mirrors into a plain
    // runtime CVar (gMods.BombArrows.Mode) because the in-game grant logic runs outside seeds too.
    static std::map<int32_t, const char*> bombArrowModeMap = {
        { RO_BOMB_ARROWS_OFF, "Off" },
        { RO_BOMB_ARROWS_BOMB_BAG, "Bomb Bag" },
        { RO_BOMB_ARROWS_SHUFFLED, "Shuffled" },
    };
    mSohMenu->AddWidget(path, "Shuffle Bomb Arrows", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("ShuffleBombArrows"))
        .RaceDisable(false)
        .PostFunc([](WidgetInfo& info) { NeiRando_MirrorSettingByCVar(CVAR_RANDOMIZER_SETTING("ShuffleBombArrows")); })
        .Options(ComboboxOptions()
                     .ComboMap(bombArrowModeMap)
                     .DefaultIndex(RO_BOMB_ARROWS_OFF)
                     .Tooltip("How Bomb Arrows are obtained. They sit at the end of the bow's element wheel,\n"
                              "next to the medallion arrows — they have no inventory slot of their own.\n\n"
                              "Off: never granted on their own (the Twilight Upgrade still unlocks them).\n"
                              "Bomb Bag: granted the moment you own any bomb bag (the old auto-grant).\n"
                              "Shuffled: a real randomizer item."));

    static std::map<int32_t, const char*> wandModeMap = {
        { RO_WAND_MEDALLIONS, "Medallions" },
        { RO_WAND_SINGLE_ITEM, "Single item" },
        { RO_WAND_ELEMENTAL_SHUFFLE, "Elemental shuffle" },
    };
    mSohMenu->AddWidget(path, "Elemental Wand", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("ElementalWandShuffle"))
        .RaceDisable(false)
        .PostFunc([](WidgetInfo& info) {
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(ComboboxOptions()
                     .ComboMap(wandModeMap)
                     .DefaultIndex(RO_WAND_MEDALLIONS)
                     .Tooltip("Six rods — Sand, Tornado, Water, Meteor, Storm and the Shadow Scepter —\n"
                              "share ONE inventory cell and one wheel (the cell Bomb Arrows vacated).\n\n"
                              "Medallions: one wand in the pool; a rod works once you own its medallion.\n"
                              "Single item: one wand in the pool; finding it unlocks all six rods.\n"
                              "Elemental shuffle: the six rods are separate items; the first found also\n"
                              "grants the wand itself."));

    mSohMenu->AddWidget(path, "Crossover Items", WIDGET_SEPARATOR_TEXT);

    mSohMenu->AddWidget(path, "Include Pikachu Pokeball", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("CrossoverPokeball"))
        .RaceDisable(false)
        .PostFunc([](WidgetInfo& info) { NeiRando_EnableCrossover(); })
        .Options(CheckboxOptions().Tooltip(
            "Adds the Pikachu Pokeball to the randomizer pool. It has no inventory cell — finding it\n"
            "unlocks PIKACHU MODE on the equipment page's Crossover Items sub-page.\n\n"
            "Seed-locked rando setting (also enables the in-game form selector)."));

    mSohMenu->AddWidget(path, "Include Mario Mask", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("CrossoverMarioMask"))
        .RaceDisable(false)
        .PostFunc([](WidgetInfo& info) { NeiRando_EnableCrossover(); })
        .Options(CheckboxOptions().Tooltip(
            "Adds the Mario Mask to the randomizer pool. It has no inventory cell — finding it unlocks\n"
            "MARIO MODE on the equipment page's Crossover Items sub-page (the same unlock the\n"
            "Peach's Castle set piece grants).\n\n"
            "Seed-locked rando setting (also enables the in-game form selector)."));

    // ===================== Tab: Controls =====================
    path.sidebarName = "Controls";
    path.column = SECTION_COLUMN_1;

    mSohMenu->AddWidget(path, "Pause Menu", WIDGET_SEPARATOR_TEXT);

    // This dropdown picks the button used to change page WITHIN a kaleido page
    // (inventory sub-page, extended equipment, SW97 arrow mode, Crossover Items).
    // The OTHER shoulder button (+ R) changes BETWEEN kaleido pages.
    // Backed by NGCKaleidoSwitcher: 0 = in-page L (pages on Z), 1 = in-page Z (pages on L).
    // NOTE: each label MUST be >1 char or UIWidgets::Combobox hides it (it skips
    // single-character entries), which is why these aren't just "L"/"Z".
    static std::map<int32_t, const char*> inPageBtnMap = {
        { 0, "L button (default)" },
        { 1, "Z button" },
    };
    mSohMenu->AddWidget(path, "In-Page Change Button (L / Z)", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("NGCKaleidoSwitcher"))
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .ComboMap(inPageBtnMap)
                     .DefaultIndex(0)
                     .Tooltip("Which shoulder button changes page WITHIN the current kaleido page:\n"
                              "inventory sub-page (Items), extended equipment (Equipment), SW97 arrow mode\n"
                              "(Quest), and Crossover Items (Map). The OTHER shoulder button, together with R,\n"
                              "changes BETWEEN the kaleido pages.\n"
                              "L button (default): L changes within the page, Z + R change kaleido pages.\n"
                              "Z button: Z changes within the page, L + R change kaleido pages."));

    mSohMenu->AddWidget(path, "Equip Items on D-Pad", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("DpadEquips"))
        .Options(CheckboxOptions().Tooltip("Allow equipping items to the D-Pad directions."));
    mSohMenu->AddWidget(path, "D-Pad on Pause", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("DPadOnPause"))
        .Options(CheckboxOptions().Tooltip("Use the D-Pad to navigate the pause menu."));

    mSohMenu->AddWidget(path, "Camera", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path, "Free Camera", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("FreeLook.Enabled"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Enables free camera control (right stick / mouse).\n"
            "Same setting as Settings > Controls > Free Look — surfaced here for convenience."));
    mSohMenu->AddWidget(path, "Free Camera in Item Cutscenes", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("FreeLook.TurnAroundCam"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Lets free camera take over the scripted \"turn around\" camera: getting an item (both the\n"
            "animation and its textbox), opening doors, drinking a bottle, playing the ocarina...\n"
            "The vanilla shot plays as usual until you push the right stick; the camera then follows you\n"
            "for the rest of the scene, at the Free Look camera distance.\n"
            "Requires Free Camera to be enabled."));

    mSohMenu->AddWidget(path, "Transformation Controls", WIDGET_SEPARATOR_TEXT);

    // Quick transform. The pad button is read straight from SDL (CrossoverHotkey_Tick in
    // OTRGlobals.cpp) because Back/Select has no N64 button to map it onto.
    static std::map<int32_t, const char*> quickTransformKeyMap = {
        { SDL_SCANCODE_0, "0 (default)" }, { SDL_SCANCODE_1, "1" },          { SDL_SCANCODE_2, "2" },
        { SDL_SCANCODE_3, "3" },           { SDL_SCANCODE_4, "4" },          { SDL_SCANCODE_5, "5" },
        { SDL_SCANCODE_6, "6" },           { SDL_SCANCODE_7, "7" },          { SDL_SCANCODE_8, "8" },
        { SDL_SCANCODE_9, "9" },           { SDL_SCANCODE_T, "T" },          { SDL_SCANCODE_G, "G" },
        { SDL_SCANCODE_V, "V" },           { SDL_SCANCODE_BACKSLASH, "\\" },
    };
    static std::map<int32_t, const char*> quickTransformPadMap = {
        { SDL_CONTROLLER_BUTTON_BACK, "Back / Select (default)" },
        { SDL_CONTROLLER_BUTTON_GUIDE, "Guide / Home" },
        { SDL_CONTROLLER_BUTTON_LEFTSTICK, "Left stick click" },
        { SDL_CONTROLLER_BUTTON_RIGHTSTICK, "Right stick click" },
    };

    mSohMenu->AddWidget(path, "Quick Transform (Crossover Items)", WIDGET_CVAR_CHECKBOX)
        .CVar("gCrossover.Hotkey.Enabled")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "One key/button turns you into the form equipped on the Crossover Items sub-page and\n"
            "back into Link, without opening the pause menu.\n"
            "Ignored while the pause menu or this menu is open."));

    mSohMenu->AddWidget(path, "Quick Transform Key", WIDGET_CVAR_COMBOBOX)
        .CVar("gCrossover.Hotkey.Key")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) { info.options->disabled = !CVarGetInteger("gCrossover.Hotkey.Enabled", 1); })
        .Options(ComboboxOptions().ComboMap(quickTransformKeyMap).DefaultIndex(SDL_SCANCODE_0));

    mSohMenu->AddWidget(path, "Quick Transform Button", WIDGET_CVAR_COMBOBOX)
        .CVar("gCrossover.Hotkey.Pad")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) { info.options->disabled = !CVarGetInteger("gCrossover.Hotkey.Enabled", 1); })
        .Options(ComboboxOptions().ComboMap(quickTransformPadMap).DefaultIndex(SDL_CONTROLLER_BUTTON_BACK));
    // Pikachu Controls — opens a dedicated assignment window (pikachu_hud.cpp):
    // per-move N64 button binds (gPikaBind.*) + the mode UI style. Applies to
    // the SECRET Crossover-Items Pikachu mode only; the pokeball transformation
    // keeps items on C and the vanilla UI.
    mSohMenu->AddWidget(path, "Pikachu Controls", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) { PikachuControls_OpenWindow(); })
        .Options(ButtonOptions()
                     .Size(Sizes::Inline)
                     .Tooltip("Open the Pikachu mode controls window: assign the N64 button for each move\n"
                              "(Jump / Quick Attack / Grass / Gigantamax / Iron Tail / Dark / Sleep) and pick\n"
                              "the mode UI style (icons over OOT buttons, or the corner HUD).\n"
                              "Secret Crossover-Items Pikachu mode only — the pokeball transformation is untouched."));
}

// =============================================================================
// Sheikah Sensor rune — the five wished-for items (Skijer's NEI)
// =============================================================================

// Every named randomizer item, alphabetical. Rebuilt until it comes back non-empty: the menu can
// open before the randomizer's item table is filled in, and a cached empty list would stay empty.
static const std::vector<std::pair<int32_t, std::string>>& SensorItemChoices() {
    static std::vector<std::pair<int32_t, std::string>> choices;

    if (choices.empty()) {
        for (int32_t rg = RG_NONE + 1; rg < RG_MAX; rg++) {
            const std::string& name =
                Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rg)).GetName().GetEnglish();
            if (!name.empty()) {
                choices.emplace_back(rg, name);
            }
        }
        std::sort(choices.begin(), choices.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    }
    return choices;
}

static std::string SensorDesireName(int32_t rg) {
    if (rg <= RG_NONE || rg >= RG_MAX) {
        return "(empty)";
    }
    const std::string& name = Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rg)).GetName().GetEnglish();
    return name.empty() ? "(empty)" : name;
}

void DrawSensorDesirePicker() {
    static char search[SENSOR_DESIRE_SLOTS][64] = {};

    ImGui::TextWrapped("Casting the Sheikah Slate's Sensor rune answers for the FIRST of these still "
                       "out there, so the order is the priority. Each answer costs a Heart Container.");

    for (int32_t slot = 0; slot < SENSOR_DESIRE_SLOTS; slot++) {
        std::string cvar = std::string(CVAR_SENSOR_DESIRE_PREFIX) + std::to_string(slot);
        int32_t current = CVarGetInteger(cvar.c_str(), RG_NONE);
        std::string label = "Desire " + std::to_string(slot + 1) + "##sensorDesire" + std::to_string(slot);

        if (ImGui::BeginCombo(label.c_str(), SensorDesireName(current).c_str())) {
            ImGui::InputTextWithHint("##sensorSearch", "Search", search[slot], sizeof(search[slot]));

            if (ImGui::Selectable("(empty)", current <= RG_NONE)) {
                CVarSetInteger(cvar.c_str(), RG_NONE);
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            }
            for (const auto& [rg, name] : SensorItemChoices()) {
                if (search[slot][0] != '\0' && name.find(search[slot]) == std::string::npos) {
                    continue;
                }
                if (ImGui::Selectable(name.c_str(), rg == current)) {
                    CVarSetInteger(cvar.c_str(), rg);
                    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                }
            }
            ImGui::EndCombo();
        }
    }
}

// Self-register the NEI menu via the same RegisterMenuInitFunc path every other
// module uses (Anchor, Harpoon, Resolution...). SohMenu::AddMenuElements() drains
// MenuInit::GetInitFuncs() at boot, so this replaces the old upstream edits to
// SohMenu.h (member decl) and SohMenu.cpp (explicit AddMenuNEI() call).
static RegisterMenuInitFunc neiMenuInitFunc(RegisterNEIMenu);

} // namespace SohGui
