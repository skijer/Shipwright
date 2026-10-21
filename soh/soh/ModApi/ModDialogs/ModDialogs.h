#ifndef SOH_MOD_API_MOD_DIALOGS_H
#define SOH_MOD_API_MOD_DIALOGS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ModDialogs_Ask(const char* title, const char* message);
void ModDialogs_Tell(const char* title, const char* message);
bool ModDialogs_RequestRestart(const char* reason);
bool ModDialogs_Restart(const char* reason);

// For main(), not mods (ModTrust refuses them as imports).
void ModRestart_WaitForPrevious(void);
void ModRestart_RelaunchIfRequested(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif

#endif
