#ifndef FLEET_PORTAL_H
#define FLEET_PORTAL_H

#include <fleet/FleetGameApi.h>

#ifdef __cplusplus
extern "C" {
#endif

// The doorways between the games. Ship's side of "market" is the Happy Mask Shop: entering it leaves for the other
// game, and arriving through it lands outside its door.
void FleetPortal_Arrive(const FleetDestination* arrival);
// Once per frame: hands a pending departure to Fleet and walks into a pending arrival.
void FleetPortal_FrameEnd(void);

#ifdef __cplusplus
}
#endif

#endif
