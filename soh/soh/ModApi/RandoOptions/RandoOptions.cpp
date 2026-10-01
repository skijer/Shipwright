#include "RandoOptions.h"

#include <deque>
#include <string>
#include <spdlog/spdlog.h>

#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/SaveManager.h"
#include "soh/SohGui/SohMenu.h"
#include "z64save.h"
#include "variables.h"

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;
}

namespace {

constexpr const char* kSection = "Randomizer";
constexpr const char* kSidebar = "Mods";

struct RegisteredOption {
    std::string key;
    std::string cvar;
    RandomizerSettingKey settingKey;
    uint8_t defaultValue;
};

std::deque<RegisteredOption> sOptions;

RegisteredOption* Find(const char* key) {
    if (key == nullptr) {
        return nullptr;
    }
    for (auto& option : sOptions) {
        if (option.key == key) {
            return &option;
        }
    }
    return nullptr;
}

bool IsNameTaken(const std::string& label) {
    for (const auto& option : Rando::Settings::GetInstance()->GetAllOptions()) {
        if (option.GetName() == label) {
            return true;
        }
    }
    return false;
}

bool IsMenuBuilt() {
    return SohGui::mSohMenu != nullptr && SohGui::mSohMenu->AreMenuElementsInitialized();
}

void AddWidget(const RegisteredOption& option) {
    WidgetPath path = { kSection, kSidebar, SECTION_COLUMN_1 };

    SohGui::mSohMenu->AddSidebarEntry(kSection, kSidebar, 1);
    Rando::Settings::GetInstance()->GetOption(option.settingKey).AddWidget(path);
}

void AddAllWidgets() {
    for (const auto& option : sOptions) {
        AddWidget(option);
    }
}

RegisterMenuInitFunc sMenuInit(AddAllWidgets);

} // namespace

bool RandoOptions_Register(const SOHModRandoOption* option) {
    if (option == nullptr || option->structSize < SOH_MOD_RANDO_OPTION_MIN_SIZE || option->key == nullptr ||
        option->label == nullptr || option->values == nullptr || option->valueCount < 2 ||
        option->defaultValue >= option->valueCount) {
        return false;
    }
    if (Find(option->key) != nullptr || IsNameTaken(option->label)) {
        SPDLOG_ERROR("[RandoOptions] '{}' is already registered", option->key);
        return false;
    }

    std::vector<std::string> values;
    for (uint32_t index = 0; index < option->valueCount; index++) {
        values.emplace_back(option->values[index] == nullptr ? "" : option->values[index]);
    }

    RegisteredOption registered = { option->key, std::string("gRandoSettings.Mods.") + option->key,
                                    static_cast<RandomizerSettingKey>(0), option->defaultValue };
    auto settings = Rando::Settings::GetInstance();
    size_t nextKey = settings->GetAllOptions().size();
    const bool isSlider = option->structSize >= offsetof(SOHModRandoOption, widget) + sizeof(option->widget) &&
                          option->widget == SOH_MOD_RANDO_WIDGET_SLIDER;

    registered.settingKey = settings->AddModOption(Rando::Option::U8(
        static_cast<RandomizerSettingKey>(nextKey), option->label, values, Rando::OptionCategory::Setting,
        registered.cvar, option->tooltip == nullptr ? "" : option->tooltip,
        isSlider ? WIDGET_CVAR_SLIDER_INT : WIDGET_CVAR_COMBOBOX, option->defaultValue));
    sOptions.push_back(std::move(registered));

    if (IsMenuBuilt()) {
        AddWidget(sOptions.back());
    }
    return true;
}

uint8_t RandoOptions_Get(const char* key) {
    RegisteredOption* option = Find(key);

    if (option == nullptr) {
        return 0;
    }
    if (IS_RANDO) {
        return Rando::Context::GetInstance()->GetOption(option->settingKey).Get();
    }
    return Rando::Settings::GetInstance()->GetOption(option->settingKey).GetOptionIndex();
}

uint8_t RandoOptions_GetForSeed(const char* key) {
    RegisteredOption* option = Find(key);

    return option != nullptr ? Rando::Context::GetInstance()->GetOption(option->settingKey).Get() : 0;
}

void RandoOptions_SaveAll() {
    for (const auto& option : sOptions) {
        SaveManager::Instance->SaveData(option.key, Rando::Context::GetInstance()->GetOption(option.settingKey).Get());
    }
}

void RandoOptions_LoadAll() {
    for (const auto& option : sOptions) {
        uint8_t value = option.defaultValue;

        SaveManager::Instance->LoadData(option.key, value, option.defaultValue);
        Rando::Context::GetInstance()->GetOption(option.settingKey).Set(value);
    }
}
