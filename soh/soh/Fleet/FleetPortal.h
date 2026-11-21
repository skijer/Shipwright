#ifndef FLEET_PORTAL_H
#define FLEET_PORTAL_H

#include <fleet/FleetGameApi.h>

#ifdef __cplusplus
extern "C" {
#endif

void FleetPortal_Arrive(const FleetDestination* arrival);
void FleetPortal_FrameEnd(void);
bool FleetPortal_LoadPending(void);
void FleetPortal_RequestDeparture(const char* portal);

#ifdef __cplusplus
}
#endif

#endif
