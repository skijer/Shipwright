#ifndef FLEET_RANDO_H
#define FLEET_RANDO_H

#include <fleet/FleetGameApi.h>

// Ship's half of Fleet's combined randomizer: the reachability oracle and the services behind RandoRequest.
char* FleetRando_DumpStaticData(void);
void FleetRando_Reset(void);
void FleetRando_SetOwnedItems(const char* itemsJson);
char* FleetRando_GetReachableChecks(void);
bool FleetRando_PlaceItem(const char* check, const char* item);
char* FleetRando_Request(const char* op, const char* json);
void FleetRando_FreeString(char* text);

#endif
