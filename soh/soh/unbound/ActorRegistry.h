#pragma once
#include <libultraship/libultra.h>

#ifdef __cplusplus
#include <cstdint>

#include "DeclaredActorType.h"

namespace SOH::Unbound {

// Clear of ACTOR_ID_MAX (a "no actor" sentinel in randomizer code) and of SoH's built-in custom actors.
inline constexpr int32_t kCustomActorIdBase = 0x1000;

// Call once, after the mod archives are mounted and after SoH's built-in custom actors are registered.
void LoadCustomActors();

const DeclaredActorType* GetDeclaredActorType(int32_t actorId);

} // namespace SOH::Unbound

#endif
