#pragma once

#include <filesystem>

// Must run before soh.o2r is mounted: it swaps in the files an update staged last session.
void ModStore_ApplyStagedUpdate();
// Must run before the mods folder is scanned: it disables and removes what the store queued.
void ModStore_ApplyPending(const std::filesystem::path& modsDirectory);
void ModStore_Init();
void ModStore_DrawMenu();
