#pragma once

#include <string>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#define MOD_CALLER_ADDRESS() _ReturnAddress()
#else
#define MOD_CALLER_ADDRESS() __builtin_return_address(0)
#endif

enum class ModPermission {
    Microphone,
    Files,
};

enum class PermissionAnswer {
    Granted,
    Denied,
    Pending,
};

struct ModIdentity {
    std::string name;
    std::string trustLabel;
    std::string trustKey;
    std::vector<ModPermission> declared;
    const void* moduleBase = nullptr;
};

std::vector<ModPermission> ModPermissions_Parse(const std::vector<std::string>& names, const std::string& modName);
void ModPermissions_RegisterMod(ModIdentity identity);
// Pass MOD_CALLER_ADDRESS() from the service the mod called, never from a helper.
const ModIdentity* ModPermissions_FindCaller(const void* address);
bool ModPermissions_HasDeclared(const ModIdentity& mod, ModPermission permission);
PermissionAnswer ModPermissions_AskOnce(const ModIdentity& mod, ModPermission permission, const std::string& purpose);
bool ModPermissions_ConfirmNow(const ModIdentity& mod, const std::string& title, const std::string& message);
