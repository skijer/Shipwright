// FleetSharedWindow.cpp (Ship side) — entrada "Fleet Shared" del SohMenu REAL.
//
// Antes era una GuiWindow flotante; ahora se registra como entrada top-level del SohMenu
// (mismo framework que "Skijer's NEI": AddMenuEntry + sidebars + widgets), así que el look
// es 1:1 con el resto del menú (header, sidebar, theme, search, columnas):
//   Fleet Shared -> [ Combo | Rando MM | NEI | Masks | Enhancements | Cheats | Tests ]
//
// - NEI/Masks/Enhancements/Cheats: variables COMPARTIDAS (existen en ambos repos) como
//   WIDGET_CVAR_CHECKBOX nativos; el Callback encola el push a 2ship (op setOptions, batch
//   automático por frame vía el pump).
// - Combo / Rando MM / Tests: WIDGET_CUSTOM (contenido dinámico: generación, manifest, oráculo).
// - El tab "Shared" del tab-row del combo ahora NAVEGA el menú a esta entrada
//   (gSettings.Menu.ActiveHeader), y la petición de 2ship (reservedU[6]) también.

#include "FleetShipCombo.h"
#include "FleetOracleClient.h"
#include "FleetComboRando.h"
#include "soh/SohGui/SohMenu.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

#include <libultraship/libultraship.h>
#include <libultraship/bridge.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace UIWidgets;

namespace {

// ---------------- push compartido a 2ship (batch automático) ----------------

unsigned long long sPushSeq = 0;
std::vector<std::pair<std::string, int>> sPending;
std::vector<std::pair<std::string, float>> sPendingFloat;
std::string sPushStatus;

void QueueSharedPush(const char* cvar) {
    int value = CVarGetInteger(cvar, 0);
    CVarSave();
    for (auto& pending : sPending) {
        if (pending.first == cvar) {
            pending.second = value;
            return;
        }
    }
    sPending.push_back({ cvar, value });
}

// Empuja un valor FLOAT a un cvar de MM (para settings que difieren en encoding, p.ej. volúmenes:
// Ship int 0-100, MM float 0.0-1.0). El destino MM se setea con CVarSetFloat en el oráculo.
void PushFloatToMm(const char* cvar, float value) {
    for (auto& pending : sPendingFloat) {
        if (pending.first == cvar) {
            pending.second = value;
            return;
        }
    }
    sPendingFloat.push_back({ cvar, value });
}

// push directo de un cvar de MM (para opciones unificadas cuyo lado MM no existe como cvar local)
void PushToMm(const char* cvar, int value) {
    for (auto& pending : sPending) {
        if (pending.first == cvar) {
            pending.second = value;
            return;
        }
    }
    sPending.push_back({ cvar, value });
}

void PumpSharedPush() {
    if (sPushSeq != 0) {
        nlohmann::json resp;
        if (FleetOracle_TryGetResponse(sPushSeq, resp)) {
            sPushSeq = 0;
            sPushStatus = resp.contains("applied")
                              ? ("Synced with MM: " + std::to_string(resp["applied"].get<int>()) + " vars")
                              : "Error syncing with MM";
        }
        return;
    }
    if (!sPending.empty() || !sPendingFloat.empty()) {
        size_t total = sPending.size() + sPendingFloat.size();
        sPushSeq = FleetOracle_SendSetOptionsRequest(sPending, sPendingFloat);
        sPushStatus = sPushSeq ? ("Sending " + std::to_string(total) + " vars to MM...")
                               : "No active combo: changes are local only";
        sPending.clear();
        sPendingFloat.clear();
    }
}

// ---------------- variables compartidas (curadas, intersección real de ambos repos) ----------------

struct SharedVar {
    const char* label;
    const char* cvar;
    int defaultValue;
    const char* tooltip;
};

// clang-format off
const SharedVar kNeiVars[] = {
    { "Custom Items (kaleido page 2)", "gMods.CustomItems.Enabled", 1, "The 24 NEI custom items on the second page" },
    { "Extended Equipment", "gCheats.ExtEquip.Enabled", 0, "Extended equipment (ext swords/shields/tunics/boots)" },
    { "Timeless Equipment", "gCheats.TimelessEquipment", 0, "Equip items regardless of age/form" },
    // Bomb Arrows stopped being an inventory item — it is the last entry of the bow's element
    // wheel. The old AutoGrantOnBag checkbox became value 1 of this mode (0 Off / 1 Bomb Bag /
    // 2 Shuffled), mirrored from the seed-locked rando setting. Skijer's NEI
    { "Bomb Arrows mode (0 Off / 1 Bomb Bag / 2 Shuffled)", "gMods.BombArrows.Mode", 0, nullptr },
    { "Aim Cycle (R/L cycles projectiles)", "gEnhancements.NeiAimCycle", 0, nullptr },
    { "Gilded look on Kokiri lvl 2", "gEnhancements.SkijerNEI.GildedUsesGildedLook", 1, nullptr },
    { "GFS look on Biggoron lvl 2", "gEnhancements.SkijerNEI.BgsUsesGfsLook", 1, nullptr },
    { "Roc's items use MM anims", "gEnhancements.RocsItemsUseMmAnims", 0, nullptr },
    { "Roc's: invert anims", "gMods.RocsItems.InvertAnims", 0, nullptr },
    { "Spiritual Stones", "gMods.SpiritualStones.Enabled", 0, "Spiritual Stone items (buffs + warps)" },
    { "SW97 Medallions", "gEnhancements.SkijerNEI.SW97Medallions", 0, "Medallion spells + elemental wheels" },
    { "Pause Play (quest page)", "gEnhancements.SkijerNEI.PausePlay", 0, "A on the quest page plays the song in-world" },
};

const SharedVar kMaskVars[] = {
    { "MM Masks (kaleido page 3)", "gMods.MmMasks.InventoryEnabled", 1, nullptr },
    { "Transformation masks only", "gMods.MmMasks.OnlyTransformation", 0, nullptr },
    { "Transformation Masks", "gMods.TransformMasks.Enabled", 1, "Deku/Goron/Zora/Fierce Deity" },
    { "Instant Transform", "gMods.TransformMasks.InstantTransform", 0, "Skip the transformation cutscene" },
    { "Kafei Mask transforms", "gMods.KafeiMaskTransform", 0, nullptr },
    { "Gerudo Mask transforms", "gMods.GerudoMaskTransform", 0, nullptr },
    { "Garo Mask transforms", "gMods.GaroMaskTransform", 0, nullptr },
    { "Instant Blast Mask", "gMods.BlastMask.Instant", 0, nullptr },
};

const SharedVar kEnhancementVars[] = {
    { "D-pad Equips", "gEnhancements.DpadEquips", 0, "NOTE: equips C-items in OoT, masks/forms in MM" },
    { "Bow/Slingshot ammo fix", "gEnhancements.BowSlingshotAmmoFix", 0, nullptr },
    { "Widescreen-aware culling", "gEnhancements.Graphics.ActorCullingAccountsForWidescreen", 0, nullptr },
    { "Mute ported MM audio (in OoT)", "gEnhancements.SkijerNEI.MuteMmAudio", 0, nullptr },
    { "Voice Pack", "gMods.VoicePack.Enabled", 0, "Toggle only; the pack selection is per-game" },
    { "Pak Loader (custom models)", "gMods.PakLoader.Enabled", 0, "Toggle only; model indices are per-game" },
};

const SharedVar kCheatVars[] = {
    { "Infinite Health", "gCheats.InfiniteHealth", 0, nullptr },
    { "Infinite Magic", "gCheats.InfiniteMagic", 0, nullptr },
    { "No Clip", "gCheats.NoClip", 0, nullptr },
    { "Moon Jump (L)", "gCheats.MoonJumpOnL", 0, nullptr },
    { "Easy Frame Advance", "gCheats.EasyFrameAdvance", 0, nullptr },
};
// clang-format on

// ---------------- widgets custom (dibujan DENTRO del SohMenu real) ----------------

// ---- General: the ONLY combo rando section — generate/load seed, saves, pool/logic, get-items.
// Todo lo demás del randomizer se edita EN CADA JUEGO (menú propio de OoT / BenGui de MM).
// Regla combo: si un item está shuffled y su política lo permite "anywhere", puede salir en
// CUALQUIER juego (limitado hoy a los items expresables en ambos — tabla FC).
void DrawRandoGeneralWidget(WidgetInfo& info) {
    (void)info;
    ImGui::TextWrapped("Combo Randomizer. Configure each game's shuffles in its OWN Randomizer menu (use the top "
                       "tabs); this section only holds the combo-wide knobs and seed generation. Your options "
                       "are NOT overridden — each game fills itself with its own logic. The only thing the "
                       "combo takes over is the goal; see the \"Compatibility\" tab.");
    ImGui::Separator();

    // combo-wide: pool size + logic (applied to BOTH generators at generate time)
    static const char* kPoolLabels[] = { "Scarce", "Normal", "Plentiful" };
    int pool = CVarGetInteger("gFleetCombo.ItemPool", 1);
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::Combo("Item Pool", &pool, kPoolLabels, 3)) {
        CVarSetInteger("gFleetCombo.ItemPool", pool);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Both games. OoT: Scarce/Balanced/Plentiful | MM: plentiful on/off (Scarce=Normal there)");
    }
    static const char* kLogicLabels[] = { "Glitchless", "No Logic" };
    int logic = CVarGetInteger("gFleetCombo.Logic", 0);
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::Combo("Logic", &logic, kLogicLabels, 2)) {
        CVarSetInteger("gFleetCombo.Logic", logic);
    }
    // Skip Get Item cutscene — combined. OoT: TimeSavers.SkipGetItemAnimation {Disabled,Junk,All}(0/1/2).
    // MM: gEnhancements.Cutscenes.SkipGetItemCutscenes {Never,Junk,EverythingButMajor,Always}(0/1/2/3).
    // Map OoT->MM: Disabled->Never(0), Junk->Junk(1), All->Always(3).
    static const char* kGiaLabels[] = { "Disabled", "Junk Items", "All Items" };
    int gia = CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"), 1);
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::Combo("Skip Get Item Cutscene (both games)", &gia, kGiaLabels, 3)) {
        CVarSetInteger(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"), gia);
        CVarSave();
        int mmVal = gia == 0 ? 0 : (gia == 1 ? 1 : 3);
        PushToMm("gEnhancements.Cutscenes.SkipGetItemCutscenes", mmVal);
    }
    ImGui::Separator();

    int goal = CVarGetInteger("gFleetCombo.GoalMode", 0);
    ImGui::TextUnformatted("Goal:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Beat Both Bosses", goal == 0)) {
        CVarSetInteger("gFleetCombo.GoalMode", 0);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Triforce Hunt", goal == 1)) {
        CVarSetInteger("gFleetCombo.GoalMode", 1);
    }
    if (goal == 1) {
        int total = CVarGetInteger("gFleetCombo.TriforceTotal", 15);
        int required = CVarGetInteger("gFleetCombo.TriforceRequired", 10);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputInt("Pieces in pool", &total)) {
            CVarSetInteger("gFleetCombo.TriforceTotal", std::max(1, total));
        }
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputInt("Pieces required", &required)) {
            CVarSetInteger("gFleetCombo.TriforceRequired", std::max(1, std::min(required, total)));
        }
    }

    // Start-in choice, recorded per slot at creation. It no longer changes the boot game: the combo
    // always comes up in OoT (see FleetShipCombo_HostBootstrap). Kept visible — and honest about it
    // — rather than silently doing nothing behind the player's back.
    bool startInMm = CVarGetInteger("gFleetCombo.StartInMM", 0);
    ImGui::BeginDisabled();
    if (ImGui::Checkbox("New combo files start in Majora's Mask", &startInMm)) {
        CVarSetInteger("gFleetCombo.StartInMM", startInMm ? 1 : 0);
    }
    ImGui::EndDisabled();
    // AllowWhenDisabled: a greyed-out control that also swallows its own explanation is worse than
    // no control at all — the tooltip IS the point here.
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Currently disabled: the combo always starts in Ocarina of Time.\n"
                          "Booting straight into MM left OoT sitting on its file select with no file\n"
                          "loaded, which crashed both games. Use the portal to go to MM.");
    }

    // ---- Load a saved combo seed (.fleet) into a slot in BOTH games (replay / share a seed) ----
    // This is the "Load Seed" the file-select COMBO mode points at via "Open Combo Settings".
    ImGui::Separator();
    ImGui::TextUnformatted("Load a saved seed (.fleet):");
    static std::vector<std::string> sFleetFiles;
    static int sFleetIdx = 0;
    static bool sFleetListed = false;
    if (!sFleetListed) {
        sFleetFiles = FleetCombo_ListFleetFiles();
        sFleetListed = true;
    }
    if (ImGui::Button("Refresh list")) {
        sFleetFiles = FleetCombo_ListFleetFiles();
        if (sFleetIdx >= (int)sFleetFiles.size()) {
            sFleetIdx = 0;
        }
    }
    ImGui::SameLine();
    if (sFleetFiles.empty()) {
        ImGui::TextDisabled("(no .fleet files in the fleet/ folder)");
    } else {
        std::vector<const char*> items;
        items.reserve(sFleetFiles.size());
        for (auto& f : sFleetFiles) {
            items.push_back(f.c_str());
        }
        ImGui::SetNextItemWidth(240.0f);
        ImGui::Combo("##fleetfile", &sFleetIdx, items.data(), (int)items.size());
        static char sLoadNameBuf[9] = "LINK";
        static int sLoadSlotIdx = 0;
        static const char* kLoadSlotLabels[] = { "File 1", "File 2", "File 3" };
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputText("Name##fleetname", sLoadNameBuf, sizeof(sLoadNameBuf));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::Combo("Slot##fleetslot", &sLoadSlotIdx, kLoadSlotLabels, 3);
        bool busy = FleetCombo_IsRunning();
        if (busy) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Load .fleet into this slot (both games)") && !busy && sFleetIdx < (int)sFleetFiles.size()) {
            FleetCombo_LoadFleet(sFleetFiles[sFleetIdx], sLoadSlotIdx, sLoadNameBuf);
        }
        if (busy) {
            ImGui::EndDisabled();
        }
    }
    std::string loadStatus = FleetCombo_GetStatus();
    if (!loadStatus.empty()) {
        ImGui::TextWrapped("%s", loadStatus.c_str());
    }
    ImGui::TextDisabled(
        "Drop a .fleet into <ShipDir>/fleet/ and Refresh. Loading BAKES it into that slot in both games.");

    ImGui::Separator();
    ImGui::TextWrapped("Seed GENERATION + new-file creation live in OoT's FILE SELECT: pick the \"COMBO\" quest "
                       "there, then Generate / Start. This panel holds the combo-wide knobs above + the .fleet "
                       "loader (replay a shared seed).");
    // Status of the shared-option push to 2ship. It used to live on the Tests sub-tab; it belongs
    // here, where the settings that trigger it are edited. Skijer's NEI
    if (!sPushStatus.empty()) {
        ImGui::TextDisabled("Shared push: %s", sPushStatus.c_str());
    }
}

// ---- Shared Rules: one row per RESTRICTED CATEGORY, on its own sub-tab ----
//
// A shared category outranks each game's own setting inside its domain and delegates everything else.
// The three modes are the same for every category, and the middle one hands placement to the combo's
// restricted stage: the items are dealt across BOTH games' spots, before anything else runs, and are
// immovable afterwards (out of the shared pool AND out of the available spots).
//
// It lives apart from "Generate / Load" on purpose: these are the rules the seed is BUILT with, not
// actions, and they will keep growing as categories are added.
//
// Keep this table in step with gFcCategories in FleetComboRando.cpp: same order, same count. Adding a
// category is a row there, a row here, and a predicate on the oracle side. Skijer's NEI
void DrawSharedRulesWidget(WidgetInfo& info) {
    struct SharedCategoryRow {
        const char* label;
        const char* cvar;
        const char* spotsMode; // name of mode 1 for this category
        const char* tooltip;
    };
    static const SharedCategoryRow kSharedCategories[] = {
        { "Songs", "gFleetCombo.SharedSongs", "Song Spots",
          "Own Game Logic: OoT and MM each apply their own song shuffle.\n"
          "Song Spots: the 23 song spots (12 OoT + 11 MM) become one shared pool and\n"
          "  the songs mix across games - an OoT song can land on an MM song spot.\n"
          "Anywhere: songs are ordinary shared items and go wherever the fill puts them.\n\n"
          "Song of Double Time and the Inverted Song of Time are never part of the 23;\n"
          "if MM shuffles them they go to its general pool." },
        { "Dungeon Rewards", "gFleetCombo.SharedDungeonRewards", "Reward Spots",
          "Own Game Logic: OoT and MM each apply their own dungeon reward setting.\n"
          "Reward Spots: the 13 boss spots (OoT's 9 + MM's 4) become one shared pool and\n"
          "  the rewards mix across games - beating the Fire Temple can hand you Odolwa's\n"
          "  Remains, and a Woodfall boss can hand you the Fire Medallion.\n"
          "Anywhere: rewards are ordinary shared items and go wherever the fill puts them.\n\n"
          "On Reward Spots, OoT's own reward stages stand down, so Link's Pocket takes an\n"
          "ordinary item like any other location." },
    };

    ImGui::TextUnformatted("How each shared category is placed across the two worlds:");
    ImGui::Separator();
    for (auto& row : kSharedCategories) {
        const char* modes[] = { "Own Game Logic", row.spotsMode, "Anywhere" };
        int mode = CVarGetInteger(row.cvar, 0);
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::Combo(row.label, &mode, modes, IM_ARRAYSIZE(modes))) {
            CVarSetInteger(row.cvar, std::max(0, std::min(mode, 2)));
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", row.tooltip);
        }
    }
    ImGui::Separator();
    ImGui::TextWrapped("A category on its spots mode is placed FIRST and never moved. If its items "
                       "outnumber its spots the generation stops and says so - no restriction is ever "
                       "quietly lifted.");
}

} // namespace

// ---------------- registro en el SohMenu (SohMenu/mSohMenu viven en namespace SohGui) ----------------

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;
namespace {

void AddSharedVarWidgets(WidgetPath& path, const SharedVar* vars, int count) {
    // deque: los c_str() deben sobrevivir todo el runtime (el menú puede guardar el puntero)
    static std::deque<std::string> sTooltipStore;
    for (int i = 0; i < count; i++) {
        const SharedVar& var = vars[i];
        std::string cvar = var.cvar;
        sTooltipStore.push_back(std::string(var.tooltip != nullptr ? var.tooltip : "Shared variable") +
                                "\nApplies in BOTH games (auto-pushed to 2ship).\n(" + var.cvar + ")");
        mSohMenu->AddWidget(path, var.label, WIDGET_CVAR_CHECKBOX)
            .CVar(var.cvar)
            .RaceDisable(false)
            .Callback([cvar](WidgetInfo& info) {
                (void)info;
                QueueSharedPush(cvar.c_str());
            })
            .Options(CheckboxOptions().Tooltip(sTooltipStore.back().c_str()));
    }
}

// Config settings shared by IDENTICAL cvar in both games (auto-link, int checkboxes/sliders).
// Volumes + float/combo settings that need value conversion are pushed via the custom Audio
// widget below (they share meaning but differ in name/encoding across the two games).
// Unified shared setting. shipCvar edits OoT natively; the Callback pushes to mmCvar (== shipCvar
// for auto-link options, or a different name for MM-renamed ones like camera/aiming/FPS).
enum class SOpt { Check, SliderI, SliderF, Combo };
struct SharedOpt {
    const char* label;
    const char* shipCvar;
    const char* mmCvar; // == shipCvar (auto-link) or different (rename)
    SOpt kind;
    float minV, maxV, defV;                      // defV also = default-bool for Check (0/1)
    const std::map<int32_t, const char*>* combo; // Combo only
    const char* tooltip;
};

const std::map<int32_t, const char*> kTextureFilterMap = { { 0, "Three-Point" }, { 1, "Linear" }, { 2, "None" } };
const std::map<int32_t, const char*> kImGuiScaleMap = {
    { 0, "Small" }, { 1, "Normal" }, { 2, "Large" }, { 3, "X-Large" }
};

// clang-format off
// Interface / General — all auto-link (identical cvar). (Theme/ResetBtn omitted: theme is a menu
// look and ResetBtn is a button-combo selector, both low value to share; can add later.)
const SharedOpt kInterfaceOpts[] = {
    { "Menu Background Opacity", "gSettings.Menu.BackgroundOpacity", "gSettings.Menu.BackgroundOpacity", SOpt::SliderF, 0.0f, 1.0f, 0.85f, nullptr, nullptr },
    { "Menu controller navigation", "gControlNav", "gControlNav", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "Cursor always visible", "gSettings.CursorVisibility", "gSettings.CursorVisibility", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "Search in sidebar", "gSettings.Menu.SidebarSearch", "gSettings.Menu.SidebarSearch", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "Search input autofocus", "gSettings.Menu.SearchAutofocus", "gSettings.Menu.SearchAutofocus", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "ImGui Menu Scaling", "gSettings.ImGuiScale", "gSettings.ImGuiScale", SOpt::Combo, 0,0,1, &kImGuiScaleMap, nullptr },
};
// Graphics — auto-link except the two renamed FPS options.
const SharedOpt kGraphicsOpts[] = {
    { "Internal Resolution", "gInternalResolution", "gInternalResolution", SOpt::SliderF, 0.5f, 2.0f, 1.0f, nullptr, "Render scale" },
    { "Anti-aliasing (MSAA)", "gMSAAValue", "gMSAAValue", SOpt::SliderI, 1, 8, 1, nullptr, "Samples per pixel" },
    { "Texture Filter", "gTextureFilter", "gTextureFilter", SOpt::Combo, 0,0,0, &kTextureFilterMap, nullptr },
    { "Enable VSync", "gVsyncEnabled", "gVsyncEnabled", SOpt::Check, 0,0,1, nullptr, nullptr },
    { "Windowed Fullscreen", "gSdlWindowedFullscreen", "gSdlWindowedFullscreen", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "Allow multiple windows", "gEnableMultiViewports", "gEnableMultiViewports", SOpt::Check, 0,0,1, nullptr, nullptr },
    { "Current FPS (interpolation)", "gSettings.InterpolationFPS", "gInterpolationFPS", SOpt::SliderI, 20, 360, 20, nullptr, "Frame interpolation target" },
    { "Match Refresh Rate", "gSettings.MatchRefreshRate", "gMatchRefreshRate", SOpt::Check, 0,0,0, nullptr, nullptr },
};
// Controls — mouse (auto-link) + camera/aiming (Ship gSettings.* <-> MM gEnhancements.Camera.*).
// Button/stick BINDINGS are ControlDeck per-port config (not simple cvars) -> not shareable here.
const SharedOpt kControlsOpts[] = {
    { "Enable mouse controls", "gSettings.EnableMouse", "gSettings.EnableMouse", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "Auto capture mouse", "gSettings.AutoCaptureMouse", "gSettings.AutoCaptureMouse", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "Invert Aiming X", "gSettings.Controls.InvertAimingXAxis", "gEnhancements.Camera.FirstPerson.InvertX", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "Invert Aiming Y", "gSettings.Controls.InvertAimingYAxis", "gEnhancements.Camera.FirstPerson.InvertY", SOpt::Check, 0,0,1, nullptr, nullptr },
    { "Right Stick Aiming (1st person)", "gSettings.Controls.RightStickAim", "gEnhancements.Camera.FirstPerson.RightStickEnabled", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "Move while aiming (1st person)", "gSettings.MoveInFirstPerson", "gEnhancements.Camera.FirstPerson.MoveInFirstPerson", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "Disable 1st-person auto-center", "gSettings.DisableFirstPersonAutoCenterView", "gEnhancements.Camera.FirstPerson.DisableFirstPersonAutoCenterView", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "1st-person Horiz Sensitivity", "gSettings.FirstPersonCameraSensitivity.X", "gEnhancements.Camera.FirstPerson.SensitivityX", SOpt::SliderF, 0.01f, 2.0f, 1.0f, nullptr, "MM clamps to 2.0 max" },
    { "1st-person Vert Sensitivity", "gSettings.FirstPersonCameraSensitivity.Y", "gEnhancements.Camera.FirstPerson.SensitivityY", SOpt::SliderF, 0.01f, 2.0f, 1.0f, nullptr, nullptr },
    { "Free Look (3rd person)", "gSettings.FreeLook.Enabled", "gEnhancements.Camera.FreeLook.Enable", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "Invert Camera X (3rd person)", "gSettings.FreeLook.InvertXAxis", "gEnhancements.Camera.RightStick.InvertXAxis", SOpt::Check, 0,0,0, nullptr, nullptr },
    { "Invert Camera Y (3rd person)", "gSettings.FreeLook.InvertYAxis", "gEnhancements.Camera.RightStick.InvertYAxis", SOpt::Check, 0,0,1, nullptr, nullptr },
    { "3rd-person Horiz Sensitivity", "gSettings.FreeLook.CameraSensitivity.X", "gEnhancements.Camera.RightStick.CameraSensitivity.X", SOpt::SliderF, 0.01f, 5.0f, 1.0f, nullptr, nullptr },
    { "3rd-person Vert Sensitivity", "gSettings.FreeLook.CameraSensitivity.Y", "gEnhancements.Camera.RightStick.CameraSensitivity.Y", SOpt::SliderF, 0.01f, 5.0f, 1.0f, nullptr, nullptr },
    { "Camera Distance", "gSettings.FreeLook.MaxCameraDistance", "gEnhancements.Camera.FreeLook.MaxCameraDistance", SOpt::SliderI, 100, 900, 185, nullptr, nullptr },
    { "Camera Transition Speed", "gSettings.FreeLook.TransitionSpeed", "gEnhancements.Camera.FreeLook.TransitionSpeed", SOpt::SliderI, 1, 900, 25, nullptr, nullptr },
};
// clang-format on

void AddSharedOpt(WidgetPath& path, const SharedOpt& o) {
    static std::deque<std::string> tt;
    std::string shipCvar = o.shipCvar;
    std::string mmCvar = o.mmCvar;
    bool isFloat = (o.kind == SOpt::SliderF);
    float defV = o.defV;
    tt.push_back(std::string(o.tooltip ? o.tooltip : "Applies to both games") + "\n(OoT: " + o.shipCvar +
                 " | MM: " + o.mmCvar + ")");
    const char* ttc = tt.back().c_str();
    auto pushCb = [shipCvar, mmCvar, isFloat, defV](WidgetInfo& info) {
        (void)info;
        if (isFloat) {
            PushFloatToMm(mmCvar.c_str(), CVarGetFloat(shipCvar.c_str(), defV));
        } else {
            PushToMm(mmCvar.c_str(), CVarGetInteger(shipCvar.c_str(), (int)defV));
        }
    };
    switch (o.kind) {
        case SOpt::Check:
            mSohMenu->AddWidget(path, o.label, WIDGET_CVAR_CHECKBOX)
                .CVar(o.shipCvar)
                .RaceDisable(false)
                .Callback(pushCb)
                .Options(CheckboxOptions().DefaultValue(o.defV != 0.0f).Tooltip(ttc));
            break;
        case SOpt::SliderI:
            mSohMenu->AddWidget(path, o.label, WIDGET_CVAR_SLIDER_INT)
                .CVar(o.shipCvar)
                .RaceDisable(false)
                .Callback(pushCb)
                .Options(IntSliderOptions().Min((int)o.minV).Max((int)o.maxV).DefaultValue((int)o.defV).Tooltip(ttc));
            break;
        case SOpt::SliderF:
            mSohMenu->AddWidget(path, o.label, WIDGET_CVAR_SLIDER_FLOAT)
                .CVar(o.shipCvar)
                .RaceDisable(false)
                .Callback(pushCb)
                .Options(FloatSliderOptions().Min(o.minV).Max(o.maxV).DefaultValue(o.defV).IsPercentage().Tooltip(ttc));
            break;
        case SOpt::Combo:
            mSohMenu->AddWidget(path, o.label, WIDGET_CVAR_COMBOBOX)
                .CVar(o.shipCvar)
                .RaceDisable(false)
                .Callback(pushCb)
                .Options(ComboboxOptions().ComboMap(*o.combo).Tooltip(ttc));
            break;
    }
}

void AddSharedOpts(WidgetPath& path, const SharedOpt* arr, int count) {
    for (int i = 0; i < count; i++) {
        AddSharedOpt(path, arr[i]);
    }
}

// Shared audio volumes as NATIVE framework sliders (Shipwright look). Ship stores int 0-100, MM
// stores float 0.0-1.0: the slider edits Ship's int cvar directly (Ship audio reads it live) and
// the Callback pushes the /100 float to MM (master read live; per-player applied in the oracle).
struct AudioVol {
    const char* label;
    const char* shipCvar;
    const char* mmCvar;
    int def;
};
const AudioVol kAudioVols[] = {
    { "Master Volume", "gSettings.Volume.Master", "gSettings.Audio.MasterVolume", 40 },
    { "Main Music Volume", "gSettings.Volume.MainMusic", "gSettings.Audio.MainMusicVolume", 100 },
    { "Sub Music Volume", "gSettings.Volume.SubMusic", "gSettings.Audio.SubMusicVolume", 100 },
    { "Sound Effects Volume", "gSettings.Volume.SFX", "gSettings.Audio.SoundEffectsVolume", 100 },
    { "Fanfare Volume", "gSettings.Volume.Fanfare", "gSettings.Audio.FanfareVolume", 100 },
};

void AddSharedAudioVolumes(WidgetPath& path) {
    static std::deque<std::string> tt;
    for (auto& a : kAudioVols) {
        std::string shipCvar = a.shipCvar;
        std::string mmCvar = a.mmCvar;
        tt.push_back(std::string("Applies to both games (Ship 0-100 <-> MM 0.0-1.0, converted).\n(") + a.shipCvar +
                     " / " + a.mmCvar + ")");
        mSohMenu->AddWidget(path, a.label, WIDGET_CVAR_SLIDER_INT)
            .CVar(a.shipCvar)
            .RaceDisable(false)
            .Callback([shipCvar, mmCvar](WidgetInfo& info) {
                (void)info;
                int v = CVarGetInteger(shipCvar.c_str(), 100);
                PushFloatToMm(mmCvar.c_str(), (float)v / 100.0f);
            })
            .Options(IntSliderOptions().Min(0).Max(100).DefaultValue(a.def).Tooltip(tt.back().c_str()));
    }
}

void RegisterFleetSharedMenu() {
#ifdef COMBO_BUILD
    return; // comboui is the shared menu under ComboShip
#endif
    if (!CVarGetInteger("isFleetShipCombo.Enabled", 0)) {
        return; // solo en combo: sin combo el menú queda exactamente como siempre
    }

    // Headers marked "##FleetShared" only show while the top "Shared" tab is active (the mode filter
    // lives in Menu.cpp). Labels render as "Settings"/"Randomizer"/... (ImGui hides text after ##).

    // ===== Settings (shared config: Audio/Graphics/Controls/Interface) =====
    WidgetPath sp = { "Settings##FleetShared", "Audio", SECTION_COLUMN_1 };
    mSohMenu->AddMenuEntry("Settings##FleetShared", "gFleetShared.SettingsSection");
    mSohMenu->AddSidebarEntry("Settings##FleetShared", "Audio", 1);
    mSohMenu->AddSidebarEntry("Settings##FleetShared", "Graphics", 1);
    mSohMenu->AddSidebarEntry("Settings##FleetShared", "Controls", 1);
    mSohMenu->AddSidebarEntry("Settings##FleetShared", "Interface", 1);
    sp.sidebarName = "Audio";
    mSohMenu->AddWidget(sp, "Volumes (both games)", WIDGET_SEPARATOR_TEXT);
    AddSharedAudioVolumes(sp);
    sp.sidebarName = "Graphics";
    AddSharedOpts(sp, kGraphicsOpts, (int)(sizeof(kGraphicsOpts) / sizeof(kGraphicsOpts[0])));
    sp.sidebarName = "Controls";
    mSohMenu->AddWidget(sp, "Camera / aiming (both games)", WIDGET_SEPARATOR_TEXT);
    AddSharedOpts(sp, kControlsOpts, (int)(sizeof(kControlsOpts) / sizeof(kControlsOpts[0])));
    mSohMenu->AddWidget(sp, "Button bindings are per-game (edit them in each game's Input Editor).",
                        WIDGET_SEPARATOR_TEXT);
    sp.sidebarName = "Interface";
    AddSharedOpts(sp, kInterfaceOpts, (int)(sizeof(kInterfaceOpts) / sizeof(kInterfaceOpts[0])));

    // ===== Randomizer (combo: SOLO General — el resto se edita en el menú de cada juego) =====
    WidgetPath rp = { "Randomizer##FleetShared", "General", SECTION_COLUMN_1 };
    mSohMenu->AddMenuEntry("Randomizer##FleetShared", "gFleetShared.RandoSection");
    mSohMenu->AddSidebarEntry("Randomizer##FleetShared", "General", 1);
    mSohMenu->AddSidebarEntry("Randomizer##FleetShared", "Shuffles", 1);
    rp.sidebarName = "General";
    mSohMenu->AddWidget(rp, "Generate / Load", WIDGET_CUSTOM).CustomFunction(DrawRandoGeneralWidget).HideInSearch(true);
    rp.sidebarName = "Shuffles";
    mSohMenu->AddWidget(rp, "Shared Shuffles", WIDGET_CUSTOM).CustomFunction(DrawSharedRulesWidget).HideInSearch(true);

    // NOTE: Enhancements/Cheats are intentionally NOT shared — they behave per-game (same cvar,
    // different effect in OoT vs MM), so they stay in each game's own menu.

    // ===== NEI (+ Masks) shared content =====
    WidgetPath np = { "NEI##FleetShared", "NEI", SECTION_COLUMN_1 };
    mSohMenu->AddMenuEntry("NEI##FleetShared", "gFleetShared.NEISection");
    mSohMenu->AddSidebarEntry("NEI##FleetShared", "NEI", 1);
    mSohMenu->AddSidebarEntry("NEI##FleetShared", "Masks", 1);
    np.sidebarName = "NEI";
    mSohMenu->AddWidget(np, "NEI (both games)", WIDGET_SEPARATOR_TEXT);
    AddSharedVarWidgets(np, kNeiVars, (int)(sizeof(kNeiVars) / sizeof(kNeiVars[0])));
    np.sidebarName = "Masks";
    mSohMenu->AddWidget(np, "Masks (both games)", WIDGET_SEPARATOR_TEXT);
    AddSharedVarWidgets(np, kMaskVars, (int)(sizeof(kMaskVars) / sizeof(kMaskVars[0])));

    SPDLOG_INFO("[FleetShared] shared headers registered (Settings/Randomizer/Enhancements/NEI)");
}

static RegisterMenuInitFunc fleetSharedMenuInit(RegisterFleetSharedMenu);

} // namespace
} // namespace SohGui

namespace {

// pump del push compartido, por frame (independiente de qué sección esté visible)
void RegisterFleetSharedPump() {
#ifdef COMBO_BUILD
    return; // comboui is the shared menu under ComboShip; there is no second process to push CVars to
#endif
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(PumpSharedPush);
}
static RegisterShipInitFunc fleetSharedPumpInit(RegisterFleetSharedPump, {});

} // namespace

// ---- C API (usada por el tab-row del combo en Menu.cpp y por la petición de 2ship) ----
// La "ventana" ahora es la entrada del menú: abrir/seleccionar = navegar el ActiveHeader.

// La "ventana" ahora es el MODO Shared del menú (gFleetCombo.MenuMode filtra la barra de headers,
// ver Menu.cpp). Estas funciones lo activan/consultan; los headers se registran vía
// RegisterMenuInitFunc.
void FleetShipCombo_RegisterSharedWindow(void) {
    // nada que hacer: los headers "##FleetShared" se registran vía RegisterMenuInitFunc.
}

void FleetShipCombo_ToggleSharedWindow(void) {
    CVarSetInteger("gFleetCombo.MenuMode", CVarGetInteger("gFleetCombo.MenuMode", 0) ? 0 : 1);
}

// La pide 2ship (reservedU[6]) al pulsar su tab "Shared": entra en modo Shared, trae Ship al frente
// y abre el menú.
void FleetShipCombo_OpenSharedWindow(void) {
    CVarSetInteger("gFleetCombo.MenuMode", 1);
    FleetShipCombo_SetUiFocus(0);
    auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
    auto menu = gui ? gui->GetMenu() : nullptr;
    if (menu != nullptr && !menu->IsVisible()) {
        menu->Show();
    }
}

int FleetShipCombo_IsSharedWindowVisible(void) {
    return CVarGetInteger("gFleetCombo.MenuMode", 0) == 1 ? 1 : 0;
}
