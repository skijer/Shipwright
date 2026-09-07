/**
 * Zonai Permafrost Item Header
 * Time-freeze TOGGLE — press to stop time, press again to let it run.
 *
 * There is no cast animation any more: the freeze snaps on the frame you press
 * the button. It costs a small amount of magic to switch on and then drains
 * continuously, so the meter is what limits how long you can hold the world
 * still. Running the meter dry switches it off on its own.
 */

#ifndef ITEM_ZONAI_PERMAFROST_H
#define ITEM_ZONAI_PERMAFROST_H

#include "z64.h"
#include "../custom_items.h"

// States.
// ACTIVE must stay 2: custom_items_common.c tests the shared state by literal
// number ("!= 2") to decide whether the other custom items may keep updating.
// State 1 used to be CASTING and no longer exists.
#define ZPERM_STATE_IDLE 0
#define ZPERM_STATE_ACTIVE 2 // Freeze held, Link free to move
#define ZPERM_STATE_ENDING 3 // Cleanup frame

// Magic. Switching on costs a lump so flicking the button on and off is not free;
// after that it drains DRAIN_COST every DRAIN_INTERVAL frames. Gameplay runs at
// 20 fps, so 1 per 10 frames is 2 magic per second — roughly 24 s of stopped time
// on a full single meter, against the 10 s the old fixed-duration spell gave.
#define ZPERM_MAGIC_ACTIVATION 4
#define ZPERM_DRAIN_COST 1
#define ZPERM_DRAIN_INTERVAL 10

// The green particles start flickering once this little magic is left, as the
// warning that the freeze is about to drop on its own.
#define ZPERM_FLICKER_MAGIC 8

// State aliases
#define zpActive gCustomItemState.zonaiPermafrostActive
#define zpState gCustomItemState.zonaiPermafrostState
#define zpSubPhase gCustomItemState.zonaiPermafrostSubPhase
#define zpTimer gCustomItemState.zonaiPermafrostTimer
#define zpSavedTime gCustomItemState.zonaiPermafrostSavedTimeIncr

#endif // ITEM_ZONAI_PERMAFROST_H
