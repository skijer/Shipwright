#include "SohMenu.h"
#include "soh/Notification/Notification.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "SohModals.h"
#include "soh/OTRGlobals.h"
#include <soh/GameVersions.h>
#include "soh/ResourceManagerHelpers.h"
#include "UIWidgets.hpp"
#include <spdlog/fmt/fmt.h>

extern "C" {
#include "include/z64audio.h"
#include "variables.h"
}

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
extern std::shared_ptr<SohModalWindow> mModalWindow;
using namespace UIWidgets;

static std::map<int32_t, const char*> imguiScaleOptions = {
    { 0, "Small" },
    { 1, "Normal" },
    { 2, "Large" },
    { 3, "X-Large" },
};

static const std::map<int32_t, const char*> menuThemeOptions = {
    { UIWidgets::Colors::Red, "Red" },
    { UIWidgets::Colors::DarkRed, "Dark Red" },
    { UIWidgets::Colors::Orange, "Orange" },
    { UIWidgets::Colors::Green, "Green" },
    { UIWidgets::Colors::DarkGreen, "Dark Green" },
    { UIWidgets::Colors::LightBlue, "Light Blue" },
    { UIWidgets::Colors::Blue, "Blue" },
    { UIWidgets::Colors::DarkBlue, "Dark Blue" },
    { UIWidgets::Colors::Indigo, "Indigo" },
    { UIWidgets::Colors::Violet, "Violet" },
    { UIWidgets::Colors::Purple, "Purple" },
    { UIWidgets::Colors::Brown, "Brown" },
    { UIWidgets::Colors::Gray, "Gray" },
    { UIWidgets::Colors::DarkGray, "Dark Gray" },
};

static const std::map<int32_t, const char*> textureFilteringMap = {
    { Fast::FILTER_THREE_POINT, "Three-Point" },
    { Fast::FILTER_LINEAR, "Linear" },
    { Fast::FILTER_NONE, "None" },
};

static const std::map<int32_t, const char*> notificationPosition = {
    { 0, "Top Left" }, { 1, "Top Right" }, { 2, "Bottom Left" }, { 3, "Bottom Right" }, { 4, "Hidden" },
};

static const std::map<int32_t, const char*> bootSequenceLabels = {
    { BOOTSEQUENCE_DEFAULT, "Default" },        { BOOTSEQUENCE_AUTHENTIC, "Authentic" },
    { BOOTSEQUENCE_FILESELECT, "File Select" }, { BOOTSEQUENCE_DEBUGWARPSCREEN, "Debug Warp Screen" },
    { BOOTSEQUENCE_WARPPOINT, "Warp Point" },
};

const char* GetGameVersionString(uint32_t index) {
    uint32_t gameVersion = ResourceMgr_GetGameVersion(index);
    switch (gameVersion) {
        case OOT_NTSC_US_10:
            return "NTSC 1.0";
        case OOT_NTSC_US_11:
            return "NTSC 1.1";
        case OOT_NTSC_US_12:
            return "NTSC 1.2";
        case OOT_NTSC_US_GC:
            return "NTSC-U GC";
        case OOT_NTSC_JP_GC:
            return "NTSC-J GC";
        case OOT_NTSC_JP_GC_CE:
            return "NTSC-J GC (Collector's Edition)";
        case OOT_NTSC_US_MQ:
            return "NTSC-U MQ";
        case OOT_NTSC_JP_MQ:
            return "NTSC-J MQ";
        case OOT_PAL_10:
            return "PAL 1.0";
        case OOT_PAL_11:
            return "PAL 1.1";
        case OOT_PAL_GC:
            return "PAL GC";
        case OOT_PAL_MQ:
            return "PAL MQ";
        case OOT_PAL_GC_DBG1:
        case OOT_PAL_GC_DBG2:
            return "PAL GC-D";
        case OOT_PAL_GC_MQ_DBG:
            return "PAL MQ-D";
        case OOT_IQUE_CN:
            return "IQUE CN";
        case OOT_IQUE_TW:
            return "IQUE TW";
        default:
            return "UNKNOWN";
    }
}

#include "message_data_static.h"
extern "C" MessageTableEntry* sNesMessageEntryTablePtr;
extern "C" MessageTableEntry* sGerMessageEntryTablePtr;
extern "C" MessageTableEntry* sFraMessageEntryTablePtr;
extern "C" MessageTableEntry* sJpnMessageEntryTablePtr;

static const std::array<MessageTableEntry**, LANGUAGE_MAX> messageTables = {
    &sNesMessageEntryTablePtr, &sGerMessageEntryTablePtr, &sFraMessageEntryTablePtr, &sJpnMessageEntryTablePtr
};

void SohMenu::UpdateLanguageMap(std::map<int32_t, const char*>& languageMap) {
    for (int32_t i = LANGUAGE_ENG; i < LANGUAGE_MAX; i++) {
        if (*messageTables.at(i) != NULL) {
            if (!languageMap.contains(i)) {
                languageMap.insert(std::make_pair(i, languages.at(i)));
            }
        } else {
            languageMap.erase(i);
        }
    }
}

void SohMenu::AddMenuSettings() {
    // Add Settings Menu
    AddMenuEntry("Settings", CVAR_SETTING("Menu.SettingsSidebarSection"));
    AddSidebarEntry("Settings", "General", 2);
    WidgetPath path = { "Settings", "General", SECTION_COLUMN_1 };

    // General - Settings
    AddWidget(path, "Menu Settings", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Menu Theme", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_SETTING("Menu.Theme"))
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .Tooltip("Changes the Theme of the Menu Widgets.")
                     .ComboMap(menuThemeOptions)
                     .DefaultIndex(Colors::LightBlue));
#if not defined(__SWITCH__) and not defined(__WIIU__)
    AddWidget(path, "Menu Controller Navigation", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_IMGUI_CONTROLLER_NAV)
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Allows controller navigation of the port menu (Settings, Enhancements,...)\nCAUTION: "
            "This will disable game inputs while the menu is visible.\n\nD-pad to move between "
            "items, A to select, B to move up in scope."));
    AddWidget(path, "Allow background inputs", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ALLOW_BACKGROUND_INPUTS)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
                        CVarGetInteger(CVAR_ALLOW_BACKGROUND_INPUTS, 1) ? "1" : "0");
        })
        .Options(CheckboxOptions()
                     .Tooltip("Allows controller inputs to be picked up by the game even when the game window isn't "
                              "the focused window.")
                     .DefaultValue(1));
    AddWidget(path, "Menu Background Opacity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("Menu.BackgroundOpacity"))
        .RaceDisable(false)
        .Options(FloatSliderOptions().DefaultValue(0.85f).IsPercentage().Tooltip(
            "Sets the opacity of the background of the port menu."));

    AddWidget(path, "General Settings", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Cursor Always Visible", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("CursorVisibility"))
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            Ship::Context::GetInstance()->GetWindow()->SetForceCursorVisibility(
                CVarGetInteger(CVAR_SETTING("CursorVisibility"), 0));
        })
        .Options(CheckboxOptions().Tooltip("Makes the cursor always visible, even in full screen."));
#endif
    AddWidget(path, "Search In Sidebar", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("Menu.SidebarSearch"))
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            if (CVarGetInteger(CVAR_SETTING("Menu.SidebarSearch"), 0)) {
                mSohMenu->InsertSidebarSearch();
            } else {
                mSohMenu->RemoveSidebarSearch();
            }
        })
        .Options(CheckboxOptions().Tooltip(
            "Displays the Search menu as a sidebar entry in Settings instead of in the header."));
    AddWidget(path, "Search Input Autofocus", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("Menu.SearchAutofocus"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Search input box gets autofocus when visible. Does not affect using other widgets."));
    AddWidget(path, "Reset Button Combination:", WIDGET_CVAR_BTN_SELECTOR)
        .CVar("gSettings.ResetBtn")
        .Options(BtnSelectorOptions().DefaultValue(BTN_CUSTOM_MODIFIER2));
    AddWidget(path, "Open App Files Folder", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            std::string filesPath = Ship::Context::GetInstance()->GetAppDirectoryPath();
            SDL_OpenURL(std::string("file:///" + std::filesystem::absolute(filesPath).string()).c_str());
        })
        .Options(ButtonOptions().Tooltip("Opens the folder that contains the save and mods folders, etc."));

    AddWidget(path, "Boot", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Boot Sequence", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_SETTING("BootSequence"))
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .DefaultIndex(BOOTSEQUENCE_DEFAULT)
                     .LabelPosition(LabelPositions::Far)
                     .ComponentAlignment(ComponentAlignments::Right)
                     .ComboMap(bootSequenceLabels)
                     .Tooltip("Configure what happens when starting or resetting the game.\n\n"
                              "Default: LUS logo -> N64 logo\n"
                              "Authentic: N64 logo only\n"
                              "File Select: Skip to file select menu\n"
                              "Debug Warp Screen: Skip to the debug warp screen\n"
                              "Warp Point: Skip to active warp point (if set), see Dev Tools -> General"));

    AddWidget(path, "Languages", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Translate Title Screen", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("TitleScreenTranslation"))
        .RaceDisable(false);
    AddWidget(path, "Language", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_SETTING("Languages"))
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            auto options = std::static_pointer_cast<UIWidgets::ComboboxOptions>(info.options);
            SohMenu::UpdateLanguageMap(options->comboMap);
        })
        .Options(ComboboxOptions()
                     .LabelPosition(LabelPositions::Far)
                     .ComponentAlignment(ComponentAlignments::Right)
                     .ComboMap(languages)
                     .DefaultIndex(LANGUAGE_ENG));
    AddWidget(path, "Accessibility", WIDGET_SEPARATOR_TEXT);
#if defined(_WIN32) || defined(__APPLE__) || defined(ESPEAK)
    AddWidget(path, "Text to Speech", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("A11yTTS"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Enables text to speech for in game dialog"));
#endif
    AddWidget(path, "Disable Idle Camera Re-Centering", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("A11yDisableIdleCam"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Disables the automatic re-centering of the camera when idle."));
    AddWidget(path, "Disable Screen Flash for Finishing Blow", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("A11yNoScreenFlashForFinishingBlow"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Disables the white screen flash on enemy kill."));
    AddWidget(path, "Disable Jabu Wobble", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("A11yNoJabuWobble"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Disable the geometry wobble and camera distortion inside Jabu."));
    AddWidget(path, "EXPERIMENTAL", WIDGET_SEPARATOR_TEXT).Options(TextOptions().Color(Colors::Orange));
    AddWidget(path, "ImGui Menu Scaling", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_SETTING("ImGuiScale"))
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .ComboMap(imguiScaleOptions)
                     .Tooltip("Changes the scaling of the ImGui menu elements.")
                     .DefaultIndex(1)
                     .ComponentAlignment(ComponentAlignments::Right)
                     .LabelPosition(LabelPositions::Far))
        .Callback([](WidgetInfo& info) { OTRGlobals::Instance->ScaleImGui(); });

    // General - About
    path.column = SECTION_COLUMN_2;

    AddWidget(path, "About", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Ship Of Harkinian", WIDGET_TEXT);
    if (gGitCommitTag[0] != 0) {
        AddWidget(path, gBuildVersion, WIDGET_TEXT);
    } else {
        AddWidget(path, ("Branch: " + std::string(gGitBranch)), WIDGET_TEXT);
        AddWidget(path, ("Commit: " + std::string(gGitCommitHash)), WIDGET_TEXT);
    }
    for (uint32_t i = 0; i < ResourceMgr_GetNumGameVersions(); i++) {
        AddWidget(path, GetGameVersionString(i), WIDGET_TEXT);
    }

    // Audio Settings
    path.sidebarName = "Audio";
    path.column = SECTION_COLUMN_1;
    AddSidebarEntry("Settings", "Audio", 3);

    AddWidget(path, "Master Volume: %d %%", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_SETTING("Volume.Master"))
        .RaceDisable(false)
        .Options(IntSliderOptions().Min(0).Max(100).DefaultValue(40).ShowButtons(true).Format(""));
    AddWidget(path, "Main Music Volume: %d %%", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_SETTING("Volume.MainMusic"))
        .RaceDisable(false)
        .Options(IntSliderOptions().Min(0).Max(100).DefaultValue(100).ShowButtons(true).Format(""))
        .Callback([](WidgetInfo& info) {
            Audio_SetGameVolume(SEQ_PLAYER_BGM_MAIN,
                                ((float)CVarGetInteger(CVAR_SETTING("Volume.MainMusic"), 100) / 100.0f));
        });
    AddWidget(path, "Sub Music Volume: %d %%", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_SETTING("Volume.SubMusic"))
        .RaceDisable(false)
        .Options(IntSliderOptions().Min(0).Max(100).DefaultValue(100).ShowButtons(true).Format(""))
        .Callback([](WidgetInfo& info) {
            Audio_SetGameVolume(SEQ_PLAYER_BGM_SUB,
                                ((float)CVarGetInteger(CVAR_SETTING("Volume.SubMusic"), 100) / 100.0f));
        });
    AddWidget(path, "Fanfare Volume: %d %%", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_SETTING("Volume.Fanfare"))
        .RaceDisable(false)
        .Options(IntSliderOptions().Min(0).Max(100).DefaultValue(100).ShowButtons(true).Format(""))
        .Callback([](WidgetInfo& info) {
            Audio_SetGameVolume(SEQ_PLAYER_FANFARE,
                                ((float)CVarGetInteger(CVAR_SETTING("Volume.Fanfare"), 100) / 100.0f));
        });
    AddWidget(path, "Sound Effects Volume: %d %%", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_SETTING("Volume.SFX"))
        .RaceDisable(false)
        .Options(IntSliderOptions().Min(0).Max(100).DefaultValue(100).ShowButtons(true).Format(""))
        .Callback([](WidgetInfo& info) {
            Audio_SetGameVolume(SEQ_PLAYER_SFX, ((float)CVarGetInteger(CVAR_SETTING("Volume.SFX"), 100) / 100.0f));
        });
    AddWidget(path, "Audio API (Needs reload)", WIDGET_AUDIO_BACKEND).RaceDisable(false);

    // Graphics Settings
    static int32_t maxFps = 360;
    const char* tooltip = "Uses Matrix Interpolation to create extra frames, resulting in smoother graphics. This is "
                          "purely visual and does not impact game logic, execution of glitches etc.\n\nA higher target "
                          "FPS than your monitor's refresh rate will waste resources, and might give a worse result.";
    path.sidebarName = "Graphics";
    AddSidebarEntry("Settings", "Graphics", 3);
    AddWidget(path, "Graphics Options", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Toggle Fullscreen", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) { Ship::Context::GetInstance()->GetWindow()->ToggleFullscreen(); })
        .Options(ButtonOptions().Tooltip("Toggles Fullscreen On/Off."));
    AddWidget(path, "Internal Resolution", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_INTERNAL_RESOLUTION)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            Ship::Context::GetInstance()->GetWindow()->SetResolutionMultiplier(
                CVarGetFloat(CVAR_INTERNAL_RESOLUTION, 1));
        })
        .PreFunc([](WidgetInfo& info) {
            if (mSohMenu->disabledMap.at(DISABLE_FOR_ADVANCED_RESOLUTION_ON).active &&
                mSohMenu->disabledMap.at(DISABLE_FOR_VERTICAL_RES_TOGGLE_ON).active) {
                info.activeDisables.push_back(DISABLE_FOR_ADVANCED_RESOLUTION_ON);
                info.activeDisables.push_back(DISABLE_FOR_VERTICAL_RES_TOGGLE_ON);
            } else if (mSohMenu->disabledMap.at(DISABLE_FOR_LOW_RES_MODE_ON).active) {
                info.activeDisables.push_back(DISABLE_FOR_LOW_RES_MODE_ON);
            }
        })
        .Options(
            FloatSliderOptions()
                .Tooltip("Multiplies your output resolution by the value inputted, as a more intensive but effective "
                         "form of anti-aliasing.")
                .ShowButtons(false)
                .IsPercentage()
                .Min(0.5f)
                .Max(2.0f));
#ifndef __WIIU__
    AddWidget(path, "Anti-aliasing (MSAA)", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_MSAA_VALUE)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            Ship::Context::GetInstance()->GetWindow()->SetMsaaLevel(CVarGetInteger(CVAR_MSAA_VALUE, 1));
        })
        .Options(
            IntSliderOptions()
                .Tooltip("Activates MSAA (multi-sample anti-aliasing) from 2x up to 8x, to smooth the edges of "
                         "rendered geometry.\n"
                         "Higher sample count will result in smoother edges on models, but may reduce performance.")
                .Min(1)
                .Max(8)
                .DefaultValue(1));
#endif
    auto fps = CVarGetInteger(CVAR_SETTING("InterpolationFPS"), 20);
    const char* fpsFormat = fps == 20 ? "Original (%d)" : "%d";
    AddWidget(path, "Current FPS", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_SETTING("InterpolationFPS"))
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            auto options = std::static_pointer_cast<IntSliderOptions>(info.options);
            int32_t defaultValue = options->defaultValue;
            if (CVarGetInteger(info.cVar, defaultValue) == defaultValue) {
                options->format = "Original (%d)";
            } else {
                options->format = "%d";
            }
        })
        .PreFunc([](WidgetInfo& info) {
            if (mSohMenu->disabledMap.at(DISABLE_FOR_MATCH_REFRESH_RATE_ON).active)
                info.activeDisables.push_back(DISABLE_FOR_MATCH_REFRESH_RATE_ON);
        })
        .Options(IntSliderOptions().Tooltip(tooltip).Min(20).Max(maxFps).DefaultValue(20).Format(fpsFormat));
    AddWidget(path, "Match Refresh Rate", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("MatchRefreshRate"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Matches interpolation value to the refresh rate of your display."));
    AddWidget(path, "Renderer API (Needs reload)", WIDGET_VIDEO_BACKEND).RaceDisable(false);
    AddWidget(path, "Enable Vsync", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_VSYNC_ENABLED)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) { info.isHidden = mSohMenu->disabledMap.at(DISABLE_FOR_NO_VSYNC).active; })
        .Options(CheckboxOptions()
                     .Tooltip("Removes tearing, but clamps your max FPS to your displays refresh rate.")
                     .DefaultValue(true));
    AddWidget(path, "Windowed Fullscreen", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SDL_WINDOWED_FULLSCREEN)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = mSohMenu->disabledMap.at(DISABLE_FOR_NO_WINDOWED_FULLSCREEN).active;
        })
        .Options(CheckboxOptions().Tooltip("Enables Windowed Fullscreen Mode."));
    AddWidget(path, "Allow multi-windows", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENABLE_MULTI_VIEWPORTS)
        .RaceDisable(false)
        .PreFunc(
            [](WidgetInfo& info) { info.isHidden = mSohMenu->disabledMap.at(DISABLE_FOR_NO_MULTI_VIEWPORT).active; })
        .Options(CheckboxOptions()
                     .Tooltip("Allows multiple windows to be opened at once. Requires a reload to take effect.")
                     .DefaultValue(true));
    AddWidget(path, "Texture Filter (Needs reload)", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_TEXTURE_FILTER)
        .RaceDisable(false)
        .Options(ComboboxOptions().Tooltip("Sets the applied Texture Filtering.").ComboMap(textureFilteringMap));

    path.column = SECTION_COLUMN_2;
    AddWidget(path, "Advanced Graphics Options", WIDGET_SEPARATOR_TEXT);

    // Controls
    path.sidebarName = "Controls";
    path.column = SECTION_COLUMN_1;
    AddSidebarEntry("Settings", "Controls", 2);
    AddWidget(path, "Clear Devices", WIDGET_BUTTON)
        .Callback([](WidgetInfo& info) {
            SohGui::mModalWindow->RegisterPopup(
                "Clear Config",
                "This will completely erase the controls config, including registered devices.\nContinue?", "Clear",
                "Cancel",
                []() {
                    Ship::Context::GetInstance()->GetConsoleVariables()->ClearBlock(CVAR_PREFIX_SETTING ".Controllers");
                    uint8_t bits = 0;
                    Ship::Context::GetInstance()->GetControlDeck()->Init(&bits);
                },
                nullptr);
        })
        .Options(ButtonOptions().Size(Sizes::Inline));
    AddWidget(path, "Controller Bindings", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Popout Bindings Window", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("ControllerConfiguration"))
        .RaceDisable(false)
        .WindowName("Configure Controller")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Enables the separate Bindings Window."));

    // Input Viewer
    path.sidebarName = "Input Viewer";
    AddSidebarEntry("Settings", path.sidebarName, 3);
    AddWidget(path, "Input Viewer", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Toggle Input Viewer", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("InputViewer"))
        .RaceDisable(false)
        .WindowName("Input Viewer")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Input Viewer.").EmbedWindow(false));

    AddWidget(path, "Input Viewer Settings", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Popout Input Viewer Settings", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("InputViewerSettings"))
        .RaceDisable(false)
        .WindowName("Input Viewer Settings")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Enables the separate Input Viewer Settings Window."));

    // Notifications
    path.sidebarName = "Notifications";
    path.column = SECTION_COLUMN_1;
    AddSidebarEntry("Settings", path.sidebarName, 3);
    AddWidget(path, "Position", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_SETTING("Notifications.Position"))
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .Tooltip("Which corner of the screen notifications appear in.")
                     .ComboMap(notificationPosition)
                     .DefaultIndex(3));
    AddWidget(path, "Duration (seconds):", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("Notifications.Duration"))
        .RaceDisable(false)
        .Options(FloatSliderOptions()
                     .Tooltip("How long notifications are displayed for.")
                     .Format("%.1f")
                     .Step(0.1f)
                     .Min(3.0f)
                     .Max(30.0f)
                     .DefaultValue(10.0f));
    AddWidget(path, "Background Opacity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("Notifications.BgOpacity"))
        .RaceDisable(false)
        .Options(FloatSliderOptions()
                     .Tooltip("How opaque the background of notifications is.")
                     .DefaultValue(0.5f)
                     .IsPercentage());
    AddWidget(path, "Size:", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("Notifications.Size"))
        .RaceDisable(false)
        .Options(FloatSliderOptions()
                     .Tooltip("How large notifications are.")
                     .Format("%.1f")
                     .Step(0.1f)
                     .Min(1.0f)
                     .Max(5.0f)
                     .DefaultValue(1.8f));
    AddWidget(path, "Test Notification", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            Notification::Emit({
                .itemIcon = "__OTR__textures/icon_item_24_static/gQuestIconGoldSkulltulaTex",
                .prefix = "This",
                .message = "is a",
                .suffix = "test.",
            });
        })
        .Options(ButtonOptions().Tooltip("Displays a test notification."));
    AddWidget(path, "Mute Notification Sound", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("Notifications.Mute"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Prevent notifications from playing a sound."));

    // Mods
    path.sidebarName = "Mods";
    path.column = SECTION_COLUMN_1;
    AddSidebarEntry("Settings", path.sidebarName, 2);

    AddWidget(path, "Graphics Mods", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Disable Bomb Billboarding", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("DisableBombBillboarding"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Disables bombs always rotating to face the camera. To be used in conjunction with mods that want to "
            "replace bombs with 3D objects."));
    AddWidget(path, "Disable Grotto Fixed Rotation", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("DisableGrottoRotation"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Disables Grottos rotating with the Camera. To be used in conjuction with mods that want to "
            "replace grottos with 3D objects."));
    AddWidget(path, "Disable Link's Sword Trail", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("DisableLinkSwordTrail"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Disables the sword trail effect when swinging Link's sword. Useful when "
                                           "using mods that replace Link's sword model."));
    AddWidget(path, "Disable 2D Pre-Rendered Scenes", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("3DSceneRender"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Disables 2D pre-rendered backgrounds. Enable this when using a mod that "
                                           "implements 3D backdrops for these areas.\n"
                                           "Requires Scene Change to alter."));
    AddWidget(path, "Disable Fixed Camera", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("DisableFixedCamera"))
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (CVarGetInteger(CVAR_ENHANCEMENT("3DSceneRender"), 0) == 0) {
                CVarSetInteger(CVAR_ENHANCEMENT("DisableFixedCamera"), 0);
                info.options->disabled = true;
            } else {
                info.options->disabled = false;
            }
            info.options->disabledTooltip = "Requires \"Disable 2D Pre-Rendered Scenes\" to be enabled.";
        })
        .Options(CheckboxOptions().Tooltip(
            "Disables the fixed camera in maps that use 2D pre-rendered backgrounds. Enable this when using a mod "
            "that implements 3D backdrops for these areas.\n"
            "Requires Scene Change to alter."));
    AddWidget(path, "Ingame Text Spacing: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_ENHANCEMENT("TextSpacing"))
        .RaceDisable(false)
        .Options(IntSliderOptions().Min(4).Max(6).DefaultValue(6).Tooltip(
            "Space between text characters (useful for HD font textures)."));

    AddWidget(path, "Mod Menu", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Popout Mod Menu Window", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("ModMenu"))
        .WindowName("Mod Menu")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Enables the separate Mod Menu Window."));
}

void SohMenu::AddMenuLocalMultiplayer() {
    AddMenuEntry("Local Multiplayer", CVAR_SETTING("Menu.LocalMultiplayerSidebarSection"));
    AddSidebarEntry("Local Multiplayer", "General", 1);

    WidgetPath path = { "Local Multiplayer", "General", SECTION_COLUMN_1 };

    AddWidget(path, "Settings", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Disable Local Multiplayer (run stock settings)", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"))
        .Options(CheckboxOptions()
                     .DefaultValue(false)
                     .Tooltip(
            "When enabled, disables local multiplayer and reverts to stock settings (1 player)."));
    AddWidget(path, "Local Multiplayer Players: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"))
        .Options(IntSliderOptions()
                     .Min(1)
                     .Max(4)
                     .DefaultValue(2)
                     .Format("%d")
                     .Tooltip("Spawns/despawns local controllable players.\n\n"
                              " - 1: Single-player\n"
                              " - 2-4: Spawn extra players on controller ports 2-4."));
    AddWidget(path, "Custom Item System", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.OriginalItemSystem"))
        .PreFunc([](WidgetInfo& info) {
            info.options->disabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0);
            info.options->disabledTooltip = "Requires local multiplayer to be enabled.";
        })
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip(
            "Matches the original mod behavior:\n"
            " - Players can use an optional second quick-item slot\n"
            " - D-pad Left/Right cycles the selected slot\n"
            " - D-pad Up/Down selects slot 1/2 while slot 2 is enabled\n"
            " - L uses the selected slot\n\n"
            "This mode ignores D-pad equip behavior so item settings won't interfere."));
    AddWidget(path, "Enable Second Item Slot", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.SecondItemSlot"))
        .PreFunc([](WidgetInfo& info) {
            s32 localMultiplayerDisabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0);
            s32 originalItemSystemEnabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.OriginalItemSystem"), 1);

            info.options->disabled = localMultiplayerDisabled || !originalItemSystemEnabled;
            info.options->disabledTooltip = localMultiplayerDisabled
                                                ? "Requires local multiplayer to be enabled."
                                                : "Requires custom item system to be enabled.";
        })
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip(
            "Shows and enables the second quick-item slot.\n\n"
            "When disabled:\n"
            " - only slot 1 is active\n"
            " - L always uses slot 1\n"
            " - D-pad Left/Right cycles slot 1"));
    AddWidget(path, "Place Second Slot Beside First", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.SecondItemSlotSideBySide"))
        .PreFunc([](WidgetInfo& info) {
            s32 localMultiplayerDisabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0);
            s32 originalItemSystemEnabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.OriginalItemSystem"), 1);
            s32 secondSlotEnabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.SecondItemSlot"), 1);

            info.options->disabled = localMultiplayerDisabled || !originalItemSystemEnabled || !secondSlotEnabled;
                 info.options->disabledTooltip = localMultiplayerDisabled
                                      ? "Requires local multiplayer to be enabled."
                                      : (!originalItemSystemEnabled
                                          ? "Requires custom item system to be enabled."
                                          : "Requires the second item slot to be enabled.");
        })
        .Options(CheckboxOptions()
                    .DefaultValue(true)
                     .Tooltip(
            "Places slot 2 beside slot 1 instead of directly below it.\n\n"
            "Shared-screen HUD keeps right-side players mirrored toward the screen center."));
    AddWidget(path, "Independent Slot Usage", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.IndependentSlotUsage"))
        .PreFunc([](WidgetInfo& info) {
            s32 localMultiplayerDisabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0);
            s32 originalItemSystemEnabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.OriginalItemSystem"), 1);
            s32 secondSlotEnabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.SecondItemSlot"), 1);

            info.options->disabled = localMultiplayerDisabled || !originalItemSystemEnabled || !secondSlotEnabled;
                 info.options->disabledTooltip = localMultiplayerDisabled
                                      ? "Requires local multiplayer to be enabled."
                                      : (!originalItemSystemEnabled
                                          ? "Requires custom item system to be enabled."
                                          : "Requires the second item slot to be enabled.");
        })
        .Options(CheckboxOptions()
                     .DefaultValue(false)
                     .Tooltip(
            "Changes how quick slots are used:\n\n"
            "When disabled (default):\n"
            " - L uses the selected slot\n"
            " - D-pad Up/Down selects slot 1/2\n\n"
            "When enabled:\n"
            " - L always uses slot 1\n"
            " - D-pad Down uses slot 2\n"
            " - D-pad Up only changes which slot is cycled by Left/Right"));
    AddWidget(path, "Use Radial Quick Item Menu", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.RadialQuickItemMenu"))
        .PreFunc([](WidgetInfo& info) {
            s32 localMultiplayerDisabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0);
            s32 originalItemSystemEnabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.OriginalItemSystem"), 1);

            info.options->disabled = localMultiplayerDisabled || !originalItemSystemEnabled;
            info.options->disabledTooltip = localMultiplayerDisabled
                                                ? "Requires local multiplayer to be enabled."
                                                : "Requires custom item system to be enabled.";
        })
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip(
            "Replaces D-pad Left/Right item cycling with a radial quick-swap wheel.\n\n"
            "Controls:\n"
            " - Hold D-pad Left or Right to open the radial menu\n"
            " - Tilt the Control Stick to highlight an item\n"
            " - Release D-pad Left/Right to equip the highlighted item\n\n"
            "D-pad Up/Down slot controls remain unchanged."));
    AddWidget(path, "Enable Player Pickup and Throw", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCarry"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);

            info.options->disabled = playerCount <= 1;
            info.options->disabledTooltip = "Requires local multiplayer with 2 or more players.";
        })
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Allows local players to pick up and throw each other.\n"
                              "Carried players can break free by pressing A."));
    AddWidget(path, "Chaotix Ring Teather", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.ElasticRopeMode"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);

            info.options->disabled = playerCount <= 1;
            info.options->disabledTooltip = "Requires local multiplayer with 2 or more players.";
        })
        .Options(CheckboxOptions()
                     .DefaultValue(false)
                     .Tooltip("Adds a shared elastic link between local players.\n"
                              "The farther a player gets from the group, the stronger the pull back.\n"
                              "In 2P, players are tethered to each other. In 3P/4P, players are tethered to the\n"
                              "group center."));
    AddWidget(path, "Elastic Rope Strength", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.ElasticRopeStrength"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);

            info.options->disabled = playerCount <= 1;
            info.options->disabledTooltip = "Requires local multiplayer with 2 or more players.";
        })
        .Options(FloatSliderOptions()
                     .DefaultValue(1.0f)
                     .Min(0.25f)
                     .Max(3.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Adjusts how strongly the rope pulls players together.\n"
                              "Lower values make the rope looser and allow more separation.\n"
                              "Higher values tighten the rope and pull harder."));
    AddWidget(path, "Teleport Target Player", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.TeleportTargetPlayer"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);

            info.options->disabled = playerCount <= 1;
            info.options->disabledTooltip = "Requires local multiplayer with 2 or more players.";
        })
        .Options(IntSliderOptions()
                     .DefaultValue(1)
                     .Min(1)
                     .Max(4)
                     .Format("Player %d")
                     .Tooltip("Selects which player all active local players teleport to when the button below is pressed."));
    AddWidget(path, "Teleport All Players To Target", WIDGET_BUTTON)
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);

            info.options->disabled = playerCount <= 1;
            info.options->disabledTooltip = "Requires local multiplayer with 2 or more players.";
        })
        .Callback([](WidgetInfo& info) {
            s32 request = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.TeleportAllToSelectedRequest"), 0);

            if (request < 0) {
                request = 0;
            }

            CVarSetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.TeleportAllToSelectedRequest"), request + 1);
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(ButtonOptions().Tooltip("Teleports every active local player to the selected target player's position."));
    AddWidget(path, "Per-Player Tunic Colors", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "P1 Tunic Color", WIDGET_CVAR_COLOR_PICKER)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.Player1TunicColor"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);

            info.options->disabled = playerCount <= 1;
            info.options->disabledTooltip = "Requires local multiplayer with 2 or more players.";
        })
        .Options(ColorPickerOptions()
                     .DefaultValue({ 30, 105, 27, 255 })
                     .UseAlpha(false)
                     .ShowReset(true)
                     .ShowRandom(true)
                     .ShowRainbow(false)
                     .ShowLock(false)
                     .Tooltip("Default: green."));
    AddWidget(path, "P2 Tunic Color", WIDGET_CVAR_COLOR_PICKER)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.Player2TunicColor"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);

            info.options->disabled = playerCount <= 1;
            info.options->disabledTooltip = "Requires local multiplayer with 2 or more players.";
        })
        .Options(ColorPickerOptions()
                     .DefaultValue({ 255, 0, 0, 255 })
                     .UseAlpha(false)
                     .ShowReset(true)
                     .ShowRandom(true)
                     .ShowRainbow(false)
                     .ShowLock(false)
                     .Tooltip("Default: red."));
    AddWidget(path, "P3 Tunic Color", WIDGET_CVAR_COLOR_PICKER)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.Player3TunicColor"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);

            info.options->disabled = playerCount <= 2;
            info.options->disabledTooltip = "Requires local multiplayer with 3 or more players.";
        })
        .Options(ColorPickerOptions()
                     .DefaultValue({ 0, 120, 255, 255 })
                     .UseAlpha(false)
                     .ShowReset(true)
                     .ShowRandom(true)
                     .ShowRainbow(false)
                     .ShowLock(false)
                     .Tooltip("Default: blue."));
    AddWidget(path, "P4 Tunic Color", WIDGET_CVAR_COLOR_PICKER)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.Player4TunicColor"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);

            info.options->disabled = playerCount <= 3;
            info.options->disabledTooltip = "Requires local multiplayer with 4 players.";
        })
        .Options(ColorPickerOptions()
                     .DefaultValue({ 122, 57, 163, 255 })
                     .UseAlpha(false)
                     .ShowReset(true)
                     .ShowRandom(true)
                     .ShowRainbow(false)
                     .ShowLock(false)
                     .Tooltip("Default: purple."));
    AddWidget(path, "Allow Player 2-4 to Load Areas", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.SecondaryPlayersLoadAreas"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);

            info.options->disabled = playerCount <= 1;
            info.options->disabledTooltip = "Requires local multiplayer with 2 or more players.";
        })
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Allows players 2-4 to trigger area loading checks (like invisible exit boundaries).\n"
                              "Useful for dungeon room transitions and connected area boundaries."));
    AddWidget(path, "Enable Splitscreen", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreen"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);

            info.options->disabled = playerCount <= 1;
            info.options->disabledTooltip = "Requires local multiplayer with 2 or more players.";
        })
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Renders one camera viewport per active local player.\n"
                              "Supports 2, 3, and 4 player split-screen layouts."));
    AddWidget(path, "Vertical Split-Screen (2P)", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreenVertical"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);
            bool splitScreenEnabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreen"), 1) != 0;

            info.options->disabled = (playerCount <= 1) || !splitScreenEnabled;
            info.options->disabledTooltip = "Requires local multiplayer split-screen to be enabled.";
        })
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("For 2-player split-screen, choose vertical (left/right) layout when enabled.\n"
                              "When disabled, 2-player uses horizontal (top/bottom) layout."));
    AddWidget(path, "Split-Screen Performance Mode (Reduced Effects)", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreenPerformanceMode"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);
            bool splitScreenEnabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreen"), 1) != 0;

            info.options->disabled = (playerCount <= 1) || !splitScreenEnabled;
            info.options->disabledTooltip = "Requires local multiplayer split-screen to be enabled.";
        })
        .Options(CheckboxOptions()
                     .DefaultValue(false)
                     .Tooltip("Improves split-screen performance by reducing duplicate secondary viewport effects.\n\n"
                              "Applies to non-primary split viewports:\n"
                              " - skips previous-room blend pass\n"
                              " - skips rain and skybox quake pass\n"
                              " - skips lens flare effects\n\n"
                              "Recommended for low-end hardware."));
    AddWidget(path, "Split-Screen Aggressive Performance Mode", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreenAggressiveMode"))
        .PreFunc([](WidgetInfo& info) {
            s32 playerCount = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.Disable"), 0)
                                  ? 1
                                  : CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount"), 2);
            bool splitScreenEnabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreen"), 1) != 0;
            bool perfModeEnabled = CVarGetInteger(CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreenPerformanceMode"), 1) != 0;

            info.options->disabled = (playerCount <= 1) || !splitScreenEnabled || !perfModeEnabled;
            info.options->disabledTooltip =
                "Requires local multiplayer split-screen and performance mode to be enabled.";
        })
        .Options(CheckboxOptions()
                     .DefaultValue(false)
                     .Tooltip("Maximum split-screen stability mode for low-end hardware and 3P/4P sessions.\n\n"
                              "Applies stronger reductions:\n"
                              " - uses opaque room pass only\n"
                              " - skips skybox/rain/lightning/lens effects\n"
                              " - draws only player actors in secondary split viewports\n"
                              " - disables local multiplayer debug marker rendering\n\n"
                              "Best for preventing crashes in resource-heavy areas."));
    AddWidget(path, "Local Multiplayer Debug Markers", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("LocalMultiplayer.DebugMarkers"))
        .Options(CheckboxOptions()
                 .DefaultValue(false)
                 .Tooltip("Draw debug markers for local multiplayer positions and midpoint camera."));
}

} // namespace SohGui
