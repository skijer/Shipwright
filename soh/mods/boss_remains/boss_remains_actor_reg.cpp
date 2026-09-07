/**
 * boss_remains_actor_reg.cpp - Runtime ActorDB registration for the boss-remains allies.
 *
 * SoH has no fixed actor-table slots for custom actors; instead each ally is registered
 * with ActorDB at runtime and its id stored in a gRemainsAlly*Id global (-1 until then).
 * This mirrors the proven sw97_init.cpp pattern (expansions/sw97/sw97_init.cpp:111-126).
 *
 * The actor implementations live in mods/boss_remains/actors/*.c, unity-#included into
 * the host boss_remains.cpp inside its extern "C" block — so everything referenced here
 * (lifecycle funcs, id globals, struct-size globals) is declared extern "C". The struct
 * sizes travel through gRemainsAlly*StructSize globals because the structs themselves are
 * only visible inside the unity TU.
 *
 * BossRemains_EnsureActorsRegistered() is called lazily by every RemainsAlly*_Spawn helper
 * (first use registers, later calls are a cheap guarded no-op). It is idempotent and safe
 * to also call from a startup hook if desired.
 *
 * NOTE: like sw97_init.cpp, this file must be added to the VS Solution Explorer manually
 * (it is auto-globbed by CMake builds).
 */

#include "soh/ActorDB.h"

// Include headers outside extern "C" — they transitively pull in C++ headers
#include "global.h"

extern "C" {

// ---- Odolwa bug ally (actors/remains_ally_bug.c) ----
extern void RemainsAllyBug_Init(Actor* thisx, PlayState* play);
extern void RemainsAllyBug_Destroy(Actor* thisx, PlayState* play);
extern void RemainsAllyBug_Update(Actor* thisx, PlayState* play);
extern void RemainsAllyBug_Draw(Actor* thisx, PlayState* play);
extern s16 gRemainsAllyBugId;
extern size_t gRemainsAllyBugStructSize;

// ---- Goht bombchu ally (actors/remains_ally_chu.c) ----
extern void RemainsAllyChu_Init(Actor* thisx, PlayState* play);
extern void RemainsAllyChu_Destroy(Actor* thisx, PlayState* play);
extern void RemainsAllyChu_Update(Actor* thisx, PlayState* play);
extern void RemainsAllyChu_Draw(Actor* thisx, PlayState* play);
extern s16 gRemainsAllyChuId;
extern size_t gRemainsAllyChuStructSize;

// ---- Gyorg fish ally (actors/remains_ally_fish.c) ----
extern void RemainsAllyFish_Init(Actor* thisx, PlayState* play);
extern void RemainsAllyFish_Destroy(Actor* thisx, PlayState* play);
extern void RemainsAllyFish_Update(Actor* thisx, PlayState* play);
extern void RemainsAllyFish_Draw(Actor* thisx, PlayState* play);
extern s16 gRemainsAllyFishId;
extern size_t gRemainsAllyFishStructSize;

void BossRemains_EnsureActorsRegistered(void);

} // extern "C"

// Shared profile choices (identical to the MM ActorProfiles these entries replace):
//   ACTORCAT_MISC  — NOT ENEMY: must not pollute enemy-count / room-clear / battle BGM,
//                    and keeps the allies invisible to RemainsAlly_FindNearestEnemy
//                    (which scans ENEMY + BOSS only).
//   culling flags  — persistent followers keep updating + drawing off-screen.
//                    (Flag names verified in soh/include/z64actor.h:115/122; same pair
//                    Ivan's EnPartner entry uses in soh/soh/ActorDB.cpp.)
//   GAMEPLAY_KEEP  — Actor_Spawn never fails on a missing scene object; the real models
//                    are loaded from mm.o2r (or OoT gameplay_keep) inside the actors.
#define REMAINS_ALLY_FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

void BossRemains_EnsureActorsRegistered(void) {
    // Idempotence guard: all three are registered together, so one id answers for all.
    if (gRemainsAllyBugId != -1) {
        return;
    }

    // Odolwa's bug (ground beetle / moth-beam / thunder bolt / nimbus cloud, via params)
    {
        ActorDBInit init;
        init.name = "RemainsAllyBug";
        init.desc = "Odolwa remains bug ally";
        init.category = ACTORCAT_MISC;
        init.flags = REMAINS_ALLY_FLAGS;
        init.objectId = OBJECT_GAMEPLAY_KEEP;
        init.instanceSize = gRemainsAllyBugStructSize;
        init.init = RemainsAllyBug_Init;
        init.destroy = RemainsAllyBug_Destroy;
        init.update = RemainsAllyBug_Update;
        init.draw = RemainsAllyBug_Draw;
        gRemainsAllyBugId = ActorDB::Instance->AddEntry(init).entry.id;
    }

    // Goht's friendly Real Bombchu (one-at-a-time, wall-climbing, manual detonate)
    {
        ActorDBInit init;
        init.name = "RemainsAllyChu";
        init.desc = "Goht remains bombchu ally";
        init.category = ACTORCAT_MISC;
        init.flags = REMAINS_ALLY_FLAGS;
        init.objectId = OBJECT_GAMEPLAY_KEEP;
        init.instanceSize = gRemainsAllyChuStructSize;
        init.init = RemainsAllyChu_Init;
        init.destroy = RemainsAllyChu_Destroy;
        init.update = RemainsAllyChu_Update;
        init.draw = RemainsAllyChu_Draw;
        gRemainsAllyChuId = ActorDB::Instance->AddEntry(init).entry.id;
    }

    // Gyorg's fish school (swim + beach-flop, friendly bite)
    {
        ActorDBInit init;
        init.name = "RemainsAllyFish";
        init.desc = "Gyorg remains fish ally";
        init.category = ACTORCAT_MISC;
        init.flags = REMAINS_ALLY_FLAGS;
        init.objectId = OBJECT_GAMEPLAY_KEEP;
        init.instanceSize = gRemainsAllyFishStructSize;
        init.init = RemainsAllyFish_Init;
        init.destroy = RemainsAllyFish_Destroy;
        init.update = RemainsAllyFish_Update;
        init.draw = RemainsAllyFish_Draw;
        gRemainsAllyFishId = ActorDB::Instance->AddEntry(init).entry.id;
    }
}
