// FleetSync.h — Fleet Ship Combo cross-game save cache ("Anchor-style" temp file) + shared
// player-state overlay + Door_Ana cross-game hole support. Same API in both repos; the .cpp
// implementations are game-specific.
//
// The single temp file <ShipDir>/fleet_temp_flags.json carries:
//   "oot"    — full OoT anchor (SaveManager saveBlock json), written when LEAVING OoT
//   "mm"     — full MM anchor (SaveContext json), written when LEAVING MM
//   "shared" — canonical mapped player state (vitals, shared inventory, registry, ...)
// On arrival a game restores its own anchor (then deletes that section) and overlays "shared".

#ifndef FLEET_SYNC_H
#define FLEET_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

// Actor param bit set on the fleet-hole Door_Ana so z_door_ana.c identifies it WITHOUT a pointer
// registry (pointers dangle across PlayState rebuilds). Bit 11: type = (params & 0x300) stays 0
// (VISIBLE), entrance = ((params>>12)&7) stays 0 — vanilla grotto spawns never set it.
#define FLEET_HOLE_PARAM 0x0800

// Absolute path of a file in the shared fleet folder (<ShipDir>/fleet/<name>), created on demand.
// Both exes resolve it to the same physical folder, so it is where the two games keep things they
// SHARE instead of copy — e.g. the combo pictograph (picture.bin + pictoflags.bin, MM's own byte
// layout). Returns "" if the exe dir can't be resolved; valid until the next call.
const char* FleetSync_SharedFilePath(const char* name);

// LEAVING this game (call when the send-fade starts, BEFORE RequestWarp): writes this game's
// anchor section + regenerates "shared" from live state.
void FleetSync_WriteDeparture(int slot);

// ARRIVING in this game (call AFTER the save slot is force-loaded, BEFORE the destination
// overrides + Play_Init): restores this game's anchor section if present (and deletes it),
// then applies the "shared" overlay.
void FleetSync_ApplyArrival(int slot);

// One-shot per process: first time this game reaches the title/file-select after launch,
// delete the whole temp file (fresh combo session).
void FleetSync_OnTitleScreen(void);

// ---- Door_Ana (grotto hole) cross-game portal support ----
// The per-scene spawner registers the hole actor it spawned; z_door_ana.c's fall commit asks
// IsFleetHole and, when true, calls OnHoleFall instead of setting a grotto entrance. The warp
// tick polls HoleFallPending to run the send-fade + flip, then ClearHoleFall.
void FleetSync_RegisterFleetHole(void* actor);
int FleetSync_IsFleetHole(void* actor);
void FleetSync_OnHoleFall(void);
int FleetSync_HoleFallPending(void);
void FleetSync_ClearHoleFall(void);
// Grab cooldown: after an arrival the hole EXISTS AND IS VISIBLE from frame one (holes always
// spawn on scene load) but cannot grab the player for a few seconds — z_door_ana.c's proximity
// check consults HoleGrabInert. Decremented every frame inside FleetSync's own tick.
void FleetSync_SetHoleGrabCooldown(int frames);
int FleetSync_HoleGrabInert(void);

// ---- SWAP TRACE (diagnostic, temporary) ----
// The two processes hand the game over to each other, and when one of them stops there is nothing in
// either log to say which stopped first or how far it got. Both games now carry the same numbered
// trace: a swap arms a short window, and every step of it announces itself with a flush, so the last
// line on each side pins down the moment. Silent outside that window.
void FleetSync_BeginSwapTrace(const char* why);
void FleetSync_SwapTrace(const char* step);

#ifdef __cplusplus
}
#endif

#endif // FLEET_SYNC_H
