#pragma once
// Cross-game item sharing under ComboShip: an item with a FleetComboItems row obtained in one game is
// granted to the other through its dormant-grant export. This header is byte-identical in both games.
// Standalone builds: every call is a no-op.

#ifdef __cplusplus
extern "C" {
#endif

// Call once per NATIVE acquisition (the give choke). Folds a tier onto its chain, maps it to the FC
// row and grants the peer game its equivalent. Silent while this game is itself receiving a grant.
void FleetShared_OnNativeObtained(int nativeId);

// Bracket any grant that arrives from the other game so the choke it runs through does not share it
// back. Nestable.
void FleetShared_BeginReceive(void);
void FleetShared_EndReceive(void);
int FleetShared_IsReceiving(void);

// Call when this game comes to the foreground. The other game's shared state (inventory, upgrades,
// vitals, masks...) is pulled and max-merged on the first tick with a loaded file, covering anything
// that never went through a pickup: save editor, cheats, starting items, saves older than sharing.
void FleetShared_RequestPullFromPeer(void);

#ifdef __cplusplus
}
#endif
