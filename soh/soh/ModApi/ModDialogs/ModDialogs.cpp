#include "ModDialogs.h"

#include <string>

#include <libultraship/libultraship.h>
#include <spdlog/spdlog.h>
#include <SDL2/SDL_messagebox.h>

#include "soh/ModApi/ModPermissions/ModPermissions.h"

namespace {

enum class DialogChoice : int {
    No,
    Yes,
};

// Every dialog is titled with the mod's name, so a mod cannot pass its questions off as the game's.
bool AskFor(const ModIdentity* mod, const char* title, const std::string& message, const char* yes, const char* no) {
    if (mod == nullptr) {
        return false;
    }
    const std::string fullTitle = mod->name + ": " + (title != nullptr ? title : "");
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, static_cast<int>(DialogChoice::No), no },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, static_cast<int>(DialogChoice::Yes), yes },
    };
    const SDL_MessageBoxData box = {
        SDL_MESSAGEBOX_INFORMATION, nullptr, fullTitle.c_str(), message.c_str(),
        SDL_arraysize(buttons),     buttons, nullptr,
    };
    int choice = static_cast<int>(DialogChoice::No);
    return SDL_ShowMessageBox(&box, &choice) == 0 && choice == static_cast<int>(DialogChoice::Yes);
}

void TellFor(const ModIdentity* mod, const char* title, const std::string& message) {
    if (mod == nullptr) {
        return;
    }
    const std::string fullTitle = mod->name + ": " + (title != nullptr ? title : "");
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, fullTitle.c_str(), message.c_str(), nullptr);
}

// The game only closes: relaunching would need a helper process, and a console flashing up looks alarming.
bool RequestRestartFor(const ModIdentity* mod, const char* reason) {
    if (mod == nullptr) {
        return false;
    }
    const std::string message = "'" + mod->name + "' (" + mod->trustLabel + ") needs Unbound to restart" +
                                (reason != nullptr ? std::string(": ") + reason : std::string(".")) +
                                "\n\nClose Unbound now? Open it again afterwards.";
    if (!AskFor(mod, "Restart", message, "Close now", "Later")) {
        return false;
    }
    SPDLOG_INFO("[ModApi] Closing for '{}' to restart", mod->name);
    Ship::Context::GetRawInstance()->GetWindow()->Close();
    return true;
}

} // namespace

extern "C" bool ModDialogs_Ask(const char* title, const char* message) {
    return AskFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), title, message != nullptr ? message : "", "Yes",
                  "No");
}

extern "C" void ModDialogs_Tell(const char* title, const char* message) {
    TellFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), title, message != nullptr ? message : "");
}

extern "C" bool ModDialogs_RequestRestart(const char* reason) {
    return RequestRestartFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), reason);
}
