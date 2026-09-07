#ifndef FLEET_SHIP_COMBO_HOST_H
#define FLEET_SHIP_COMBO_HOST_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

// Entry points the ComboShip launcher resolves out of the built binary. __declspec is MSVC-only,
// so clang/gcc get the equivalent visibility attribute instead.
#ifdef _WIN32
#define FLEET_COMBO_EXPORT __declspec(dllexport)
#else
#define FLEET_COMBO_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Host-side Fleet Ship Combo bootstrap (Frente A). Ship (Ocarina of Time) is ALWAYS
// the host. On boot, if the combo is enabled and the player was last in 2ship
// (isPlayerIn2Ship CVar) OR Ship was launched with --boot=mm (handoff from 2ship),
// this launches the 2ship child process (2ship.exe --fleet-child).
//
// Both processes then stay alive; switching games pauses the inactive session (Frente B
// wires the seamless pause + shared-texture compositing). The 2ship child is told
// --fleet-child so it does NOT bounce back to Ship (loop guard).
//
// CVars: isFleetShipCombo.Enabled (master), isPlayerIn2Ship (1 = MM/2ship, 0 = OoT/Ship).
//
// Call once early in boot (after InitOTR), passing the process argc/argv.
void FleetShipCombo_HostBootstrap(int argc, char** argv);

// Mirror mm.o2r + oot.o2r so both sit next to BOTH exes (combo layout: root + /2ship). Call around the
// extractor: an o2r extracted by either game is copied into the sibling dir, and a missing one that's
// present in the sibling is pulled in. No-op with no sibling (standalone).
//
// VERSION-AWARE: each archive has an owner build (mm.o2r <-> 2ship.o2r, oot.o2r <-> soh.o2r) whose
// "portVersion" major it must match. Only a copy that matches its owner is ever mirrored, and it
// OVERWRITES a copy that doesn't. Plain existence-mirroring used to bounce an OUTDATED archive back
// into the dir the game had just deleted it from, so every boot re-deleted it (or, worse, the other
// game loaded it and crashed on the new resource format).
void FleetShipCombo_ProvisionO2rBothDirs(void);

// ---- MM archive gate (runs BEFORE Ship's own extractor) ----
// The combo needs a VALID mm.o2r (matching the 2ship build) somewhere in the layout. If there is none,
// this launches `2ship.exe --fleet-extract` VISIBLE so its ROM extractor can build one, and returns
// true; the caller then pumps FleetShipCombo_GuestExtractRunning() (drawing a "waiting" modal) until
// it returns false, and only THEN runs Ship's own extractor. Order matters and is deliberate: MM's
// archive first, then OoT's. Doing it the other way round meant Ship launched the child HIDDEN
// (parked off-screen) and 2ship's own "No O2R Files - Generate one now?" popup was drawn where nobody
// could see or click it: MM never came up and the combo looked dead.
// Returns false when nothing needs doing (combo off, no 2ship.exe, or a valid mm.o2r exists).
bool FleetShipCombo_GuestExtractStart(void);
// True while the visible extractor child is still running.
bool FleetShipCombo_GuestExtractRunning(void);
// True when a mm.o2r matching the 2ship build exists next to soh.exe or next to 2ship.exe. This is
// what HostBootstrap gates the hidden child on: launching 2ship without it only produces an invisible
// extractor prompt.
bool FleetShipCombo_HaveValidMmArchive(void);

// ---- Shared-memory coordination (Frente B) ----
// A named shared-memory region carries the active-game flag (and later the D3D11
// shared texture handle + per-frame sync) between Ship and 2ship.
//   activeGame: 0 = Ocarina of Time (Ship), 1 = Majora's Mask (2ship)
//
// Opens (or creates) the shared region. Safe to call more than once. Only meaningful
// in combo mode; standalone Ship never creates it, so IsThisGameActive() stays true.
// instanceKey (Ship's own PID) makes the region name UNIQUE per combo, so several combos
// can run on one machine without colliding. Pass 0 to use the legacy unsuffixed name.
void FleetShipCombo_SharedInit(unsigned long instanceKey);

// True when a combo is active AND its goal is "Beat Both Bosses" (gFleetCombo.GoalMode 0).
//
// That goal is genuinely cross-game: Ganon falling is only half of it, so whichever boss dies first
// must NOT roll credits. Both games ask this before running their ending. False outside a combo, so
// a solo seed behaves exactly as it always did.
int FleetCombo_BeatBothBosses(void);

// Active game stored in shared memory, or -1 if the region is unavailable.
int FleetShipCombo_GetActiveGame(void);

// Write the active game to shared memory (used by the Switch button).
void FleetShipCombo_SetActiveGame(int game);

// ---- Cross-game loading-zone WARP (the world connector) ----
// Trigger side: record the target (in the TARGET game's scene-id + world coords), bump the warp
// seq, and flip activeGame so the target game becomes active and applies it. targetGame: 0 = OoT
// (Ship), 1 = MM (2ship). rotY is the s16 binary-angle Link should face on arrival. saveFile is the
// save SLOT the trigger is in (e.g. gSaveContext.fileNum) so the target game lands in its own same slot.
void FleetShipCombo_RequestWarp(int targetGame, int scene, float x, float y, float z, int rotY, int saveFile);

// Scene sentinel for a RESUME warp: "hand the player to the other game AT ITS OWN SAVE", instead of
// at a portal. Sent when a combo file whose last save was made in the other game is loaded — the
// arrival keeps the entrance/respawn the save itself carries (owl save included) and only takes the
// shared-state overlay. Every real scene id is >= 0, so -1 can never collide with one.
#define FC_WARP_SCENE_RESUME (-1)

// Queue a RESUME hand-off to MM. Called when a combo file is loaded whose last save was made in MM:
// the combo always boots in OoT, so OoT loads the file and then walks the player across by itself,
// landing them in MM's own save. Queued rather than done immediately — it waits for OoT to actually
// be in gameplay, so the hand-off uses the ordinary in-game warp path (fade, departure, flip) and
// never the cold-boot one. No-op if the last save was in OoT.
void FleetCombo_QueueResumeToMm(void);

// Applied by whichever game just became active: returns 1 ONCE per new request when a warp is
// addressed to THIS game, filling the target scene + land position/rotation. The receiver then
// loads `scene` and overrides Link's pos/rot to (x,y,z,rotY). Any out-param may be null.
int FleetShipCombo_ConsumePendingWarp(int* scene, float* x, float* y, float* z, int* rotY);

// The save SLOT the pending warp came from (set by RequestWarp). The receiver loads its own save at
// this slot so OoT file_1 <-> MM file_1. Returns -1 if unset/unavailable.
int FleetShipCombo_GetWarpSaveFile(void);

// Cross-game arrival blackout: paint THIS game's screen black for `frames` frames so the stale frame
// of the other game + the warp scene-load are hidden during a flip. The render path queries
// ...Active() once per frame (it decrements and returns 1 while still blacking out).
void FleetShipCombo_BeginArrivalBlackout(int frames);
int FleetShipCombo_ArrivalBlackoutActive(void);

// Sending-side fade overlay alpha (0..255): the active game ramps it while Link walks into the door
// (no scene transition); the host PiP consumer draws black at this alpha over the scene for a real
// fade-out, then flips at full black. Same-process (host) read/write.
void FleetShipCombo_SetSendFadeAlpha(int alpha);
int FleetShipCombo_GetSendFadeAlpha(void);

// DEV: index of the Lost Woods room display list the MM door-tunnel tool is currently showing, shared
// so Ship's on-screen overlay can display it while you cycle to find the tunnel piece.
void FleetShipCombo_SetDoorDLIndex(int index);
int FleetShipCombo_GetDoorDLIndex(void);

// ---- Combo seed identity ----
// The Rando finalSeed OoT generated for this combo. OoT publishes it; MM validates the finalSeed
// baked into its paired save against it and rebuilds the slot when they disagree, so an MM file
// left over from an older seed can never be played against a newer OoT seed. 0 = unset.
void FleetShipCombo_SetComboSeed(unsigned int seed);
unsigned int FleetShipCombo_GetComboSeed(void);

// ---- Anchor-style packet channel (shared-memory rings, region version 2) ----
// The transport under FleetNet: one JSON message per call, same shape as an Anchor packet, but
// through shared memory instead of a socket. Two one-way rings mean no lock and no file, so a
// delta costs microseconds and cannot hit the oracle's "sharing violation" retry loop.
// Push returns 1 if the packet was queued (0 = no combo, peer too old, or payload > 1023 bytes).
// Pop fills `out` with ONE pending packet and returns 1, or returns 0 when the queue is empty --
// call it in a loop from the per-frame pump until it returns 0.
int FleetShipCombo_PushPacket(const char* json);
int FleetShipCombo_PopPacket(char* out, int cap);

// True if THIS process (Ocarina of Time) is the active game, OR if shared memory is
// unavailable (standalone). Drives input blocking, audio mute and the warp triggers.
bool FleetShipCombo_IsThisGameActive(void);

// ---- Waiting room ("limbo") ----
// The inactive game is no longer frozen: before handing over it parks Link in a sealed custom
// scene ("fleet_scene", hijacking SCENE_TEST01) and keeps RUNNING there. Implemented in
// FleetWarpBoot.cpp.
//   FleetLimbo_DepartToMm -> what the send path calls INSTEAD of RequestWarp: records the warp,
//                            walks Link into the room, and the boot tick flips once he is inside.
//   IsGameSuspended       -> 1 only for an inactive game that is NOT parked (the old freeze, kept
//                            as the fallback). The freeze/render gates ask this, not IsThisGameActive.
//   IsParkedInLimbo       -> 1 while the loaded scene is the waiting room.
//   LimboSaveShadow*      -> wrap a save write done while parked so the file records the player's
//                            real place, never the waiting room.
void FleetLimbo_DepartToMm(int scene, float x, float y, float z, int rotY, int saveFile);
int FleetLimbo_InFlight(void); // 1 while walking into the room (already inactive): guards must not squash it
int FleetShipCombo_IsGameSuspended(void);
int FleetShipCombo_IsParkedInLimbo(void);
void FleetShipCombo_LimboSaveShadowBegin(void);
void FleetShipCombo_LimboSaveShadowEnd(void);

// ---- Picture-in-picture: shared D3D11 game texture (Frente B B2-B4) ----
// Read the shared-texture descriptor published by 2ship (the producer). Returns 1 if
// a handle is present, 0 otherwise. Any out-param may be null. Used by the Ship-side
// consumer to OpenSharedResource + draw the 2ship image in an ImGui panel.
int FleetShipCombo_GetSharedTexture(unsigned long long* handle, unsigned int* width, unsigned int* height,
                                    unsigned int* dxgiFormat, unsigned int* frameIndex);

// Consumer (Ship): register the ImGui window that shows 2ship's shared game texture
// (picture-in-picture). Call once after the GUI is set up (e.g. SohGui SetupGuiElements).
// No-op unless the combo is enabled.
void FleetShipCombo_RegisterConsumerWindow(void);

// UI focus: which game's window is in front for CONFIG (0 = Ship, 1 = 2ship). Independent
// of the active game. Ship's NEI "View" selector sets it; 2ship reads it to show/hide its
// own window so the user can reach its BenGui.
int FleetShipCombo_GetUiFocus(void);
void FleetShipCombo_SetUiFocus(int focus);

// Read at menu-REGISTRATION time (boot), so "isFleetShipCombo.DevUi" needs a restart to take effect.
bool FleetShipCombo_ShowMenuUi(void);

// ---- FleetSync save-sync handshake (reservedU[1..3]) ----
// The game that just SAVED signals; the other (frozen) exe applies the shared overlay from the
// temp file, saves its own slot, and acks with the seq it processed. See FleetSync.cpp.
void FleetShipCombo_SignalSyncSave(int slot);
unsigned long long FleetShipCombo_GetSyncSaveSeq(void);
int FleetShipCombo_GetSyncSaveSlot(void);
void FleetShipCombo_AckSyncSave(unsigned long long seq);
unsigned long long FleetShipCombo_GetSyncSaveAck(void);

// ---- Fleet Oracle (combo randomizer generation) handshake (reservedU[4..5]) ----
// The HOST (Ship) writes fleet_oracle_req.json in its own dir and bumps the request seq; the MM
// oracle (2ship FleetOracle.cpp) answers into fleet_oracle_resp.json and acks the seq.
// Host-side client: FleetOracleClient.h.
void FleetShipCombo_SignalOracleRequest(void);
unsigned long long FleetShipCombo_GetOracleRequestSeq(void);
void FleetShipCombo_AckOracleResponse(unsigned long long seq);
unsigned long long FleetShipCombo_GetOracleResponseAck(void);

// ---- Shared-window open request (reservedU[6]) ----
// El tab "Shared" de 2ship bumpea el contador; nuestro pump (FleetOracleClient) abre la ventana.
void FleetShipCombo_RequestSharedWindowOpen(void);
unsigned long long FleetShipCombo_GetSharedWindowOpenSeq(void);

// ---- Cross-game RESTART (reservedU[10]) ----
// A reset in one game signals; the other game's per-frame pump consumes it and resets itself too,
// so "restart one, restart both". SignalRestart marks the bump as ours (we never respond to our own
// reset); ConsumeRestartRequest returns 1 ONCE when the OTHER game reset.
void FleetShipCombo_SignalRestart(void);
int FleetShipCombo_ConsumeRestartRequest(void);

// Hand the combo back to Ocarina of Time: active game 0, front window 0, isPlayerIn2Ship 0.
// MM's own title screen / file select are never screens this combo shows, so every restart ends
// here and the player comes back on OoT's title. Idempotent; no-op outside a combo.
void FleetShipCombo_YieldToOoT(void);

// ---- Guest (2ship) watchdog ----
// The heartbeat 2ship bumps every frame in shared memory (reservedU[0]). Same value twice means
// nothing turned over there.
unsigned long long FleetShipCombo_GetGuestHeartbeat(void);

// Poll the 2ship child: dead process (crash / closed / exited) or a heartbeat that stopped while
// the process lingers (hang) both tear the combo down -- Ship shows MM's image, so a guest that
// stopped rendering leaves Ship on a black screen it can never recover from. Emits a notification,
// then closes Ship a moment later. Call every frame; no-op when there is no child (standalone).
void FleetShipCombo_PollGuestAlive(void);

// "Is 2ship turning frames right now?" — 1 yes, 0 no (dead process, never started, or hung).
// The cross-game warp asks this on the frame it would flip, because the flip is one-way: making a
// dead or hung MM the active game leaves the player on a window that never updates again, with OoT
// frozen behind it and no way back. Unlike the watchdog above this answers immediately and does not
// tear anything down — the warp just declines to travel and the player keeps playing OoT.
int FleetShipCombo_IsGuestResponsive(void);

// Tell the player why a warp did nothing (the warp trigger lives in C and cannot post notifications
// itself). Logs a [FleetWatchdog] line and shows an on-screen notice.
void FleetShipCombo_ReportGuestUnavailable(void);

// ---- UI-overlay texture (reservedU[7..9]) ----
// SECOND shared texture published by 2ship with ONLY its ImGui windows (trackers etc.) on a
// transparent background — excludes its game image and BenGui menu. The consumer draws it on
// top of whichever game is active so both games' trackers can be visible at once. Display is
// gated by gFleetCombo.ShowMmUiOverlay (default on); 2ship's publishing by gFleetShipCombo.UiOverlay.
void FleetShipCombo_PublishUiTexture(unsigned long long handle, unsigned int width, unsigned int height,
                                     unsigned int dxgiFormat, unsigned int frameIndex);
int FleetShipCombo_GetUiTexture(unsigned long long* handle, unsigned int* width, unsigned int* height,
                                unsigned int* dxgiFormat, unsigned int* frameIndex);

// ---- Creación del par de saves combo ----
// Encola la creación del save RANDOMIZER de OoT en el slot (overwrite). Se ejecuta cuando el
// juego está en title/file select (Sram_InitSave necesita FileChooseContext). name = 8 bytes ya
// codificados al charset del file select. El lado MM va por la op createSave del oráculo.
void FleetCombo_RequestCreateSave(int slot, const unsigned char name[8]);

// ---- Ventana "Fleet Shared" (FleetSharedWindow.cpp) ----
// La abre/cierra el tab "Shared" del tab-row del combo (Menu.cpp). Contiene los smoke tests del
// oráculo, el editor de opciones del rando de MM (op setOptions) y las variables compartidas.
void FleetShipCombo_RegisterSharedWindow(void);
void FleetShipCombo_ToggleSharedWindow(void);
void FleetShipCombo_OpenSharedWindow(void);
int FleetShipCombo_IsSharedWindowVisible(void);

// ---- File-select "COMBO" (QUEST_OOTXMM) bridges (called from C: z_file_choose.c / z_sram.c) ----
// C wrappers around the C++ combo generator / oracle so the OoT file-select COMBO mode can:
//   - Generate: kick the combo seed generation (async, own thread).
//   - OpenSettings: open the shared combo randomizer settings (the trimmed Fleet Shared "Randomizer").
//   - IsBusy: true while a combo generation is running (grays the menu / shows "generating").
//   Seed-ready gate reuses the existing Randomizer_IsSeedGenerated() — combo generation marks the
//   SAME Rando context as seed-generated, so no separate accessor is needed.
void FleetComboFS_Generate(void);
void FleetComboFS_OpenSettings(void);
int FleetComboFS_IsBusy(void);
// File-select "Load Combo Seed" picker: refresh the .fleet list (call on menu open), read its size
// / entries for the DL cursor, and load the chosen entry SEED-ONLY (Start Combo then bakes it into
// the file-select slot). LoadSeedIndex uses the last refreshed list.
void FleetComboFS_RefreshFleets(void);
int FleetComboFS_FleetCount(void);
const char* FleetComboFS_FleetName(int idx);
void FleetComboFS_LoadSeedIndex(int idx);
// Combo active save slot published to shared memory (0..2), so MM auto-loads the same slot when it
// becomes active. -1 when unset. (reservedU[11].)
void FleetShipCombo_SetComboSlot(int slot);
int FleetShipCombo_GetComboSlot(void);
// Called from Sram_InitSave right after the OoT combo slot is born (quest.id == QUEST_OOTXMM):
// tells MM to delete+recreate its paired slot WITH the prepared seed, and bakes this file's
// "start in MM vs OoT" choice (from gFleetCombo.StartInMM) so loading it later boots the right game.
void FleetComboFS_OnCreateSave(int slot);
// Called from FileChoose_LoadGame when a QUEST_OOTXMM file is loaded: sets the active game (and
// isPlayerIn2Ship) from this slot's baked start-in flag, handing off to MM when requested.
void FleetComboFS_OnLoadSave(int slot);

#ifdef __cplusplus
}
#endif

#endif // FLEET_SHIP_COMBO_HOST_H
