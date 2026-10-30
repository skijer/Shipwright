#ifndef FLEET_GAME_H
#define FLEET_GAME_H

#include <stdbool.h>

#ifdef __cplusplus
#include <memory>
#include <string>

namespace Ship {
class ResourceManager;
}

// Gives Ship its own resources inside the Context Fleet created.
std::shared_ptr<Ship::ResourceManager> FleetGame_CreateResources(const std::string& archivePath);
// What keeps Ship from booting from its install folder, in words a player can act on; empty when nothing does.
std::string Ship_InstallProblem(const std::string& directory);

extern "C" {
#endif

// True while Ship runs inside Fleet rather than as its own program.
bool FleetGame_IsHosted(void);
// Joins the Context Fleet created: Ship's folder, settings and console variables become current.
void FleetGame_JoinHostContext(void);
// Releases what JoinHostContext and CreateResources registered. Main thread, at deinit.
void FleetGame_LeaveHostContext(void);
// Between frames: Fleet may park Ship here and run the other game. No-op outside Fleet.
void FleetGame_FrameEnd(void);

#ifdef __cplusplus
}
#endif

#endif
