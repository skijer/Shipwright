#pragma once
// SOH [Unbound] Registry of custom actor types, one unbound/actors/<name>.json each. Each type becomes its own ActorDB
// entry, numbered from kCustomActorIdBase, that runs the shared driver in DeclaredActor.cpp. Scenes place a type by
// name; the number is never written anywhere. Design: unbound-docs/actors.md.
#include <libultraship/libultra.h>

#ifdef __cplusplus
#include <cstdint>

#include "DeclaredActorType.h"

namespace SOH::Unbound {

// Custom actor types start here, clear of the vanilla table, of ACTOR_ID_MAX (a "no actor" sentinel in randomizer
// code), of SoH's built-in custom actors and of forks that took ids just past the vanilla table.
inline constexpr int32_t kCustomActorIdBase = 0x1000;

// Reads every layer-merged type file and registers each valid one with ActorDB. Call once, after the mod archives
// are mounted and after SoH's built-in custom actors are registered.
void LoadCustomActors();

// The declared type behind an actor id, or nullptr when the id is not a declared type.
const DeclaredActorType* GetDeclaredActorType(int32_t actorId);

} // namespace SOH::Unbound

#endif
