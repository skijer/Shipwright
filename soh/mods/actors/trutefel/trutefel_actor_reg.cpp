/**
 * trutefel_actor_reg.cpp - Runtime ActorDB registration for trueffel's three custom enemies.
 *
 * SoH has no fixed actor-table slots for custom actors; each enemy is registered with
 * ActorDB at runtime and its id stored in a gEn*Id global (-1 until then). This mirrors
 * boss_remains_actor_reg.cpp / sw97_init.cpp.
 *
 * The actor implementations live in mods/actors/trutefel/actors/*.c, unity-#included into
 * trutefel_enemies.cpp inside its extern "C" block — everything referenced here (lifecycle
 * funcs, id globals, struct-size globals) is declared extern "C". Struct sizes travel
 * through gEn*StructSize because the structs are only visible inside that TU.
 *
 * ---- Archive gate -----------------------------------------------------------------------
 * The models/textures live in trutefel-enemies.o2r (mods/). If the archive is missing, the
 * limb DL path strings would resolve to garbage at draw time, so the WHOLE registration is
 * gated on one canonical mesh path per enemy (ResourceMgr_FileExists). Ids stay -1 when the
 * archive is absent — `spawn En_Miniblin` then simply reports an unknown actor.
 *
 * ---- When it runs -----------------------------------------------------------------------
 * RegisterShipInitFunc fires on boot AFTER OTRExtScanner() has indexed every mounted
 * archive (OTRGlobals.cpp: scanner at InitOTR, ShipInit::InitAll right after), so the
 * FileExists gate sees mods/*.o2r content. A cheap OnGameFrameUpdate retry also runs while
 * unregistered, in case the archive set changes after boot. Registration is idempotent
 * (gEnMiniblinId != -1 guard; all three register together).
 *
 * ---- Scissors Beetle Navi hint ----------------------------------------------------------
 * The reference ships a DEFINE_MESSAGE for textId 0x065D (naviEnemyId 0x5D + 0x600, see
 * z_player.c). SoH's message table has no 0x065D, so the text is served through the
 * OnOpenText hook with a CustomMessage (same pattern as picto_message.cpp). English only —
 * the reference's "german"/"french" placeholders were dropped.
 *
 * NOTE: like boss_remains_actor_reg.cpp, add this file to the VS solution by hand.
 */

#include "soh/ActorDB.h"

// Include headers outside extern "C" — they transitively pull in C++ headers
#include "global.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"

extern "C" {

uint8_t ResourceMgr_FileExists(const char* resName); // soh/ResourceManagerHelpers.cpp

// ---- Miniblin (actors/z_en_miniblin.c) ----
extern void EnMiniblin_Init(Actor* thisx, PlayState* play);
extern void EnMiniblin_Destroy(Actor* thisx, PlayState* play);
extern void EnMiniblin_Update(Actor* thisx, PlayState* play);
extern void EnMiniblin_Draw(Actor* thisx, PlayState* play);
extern s16 gEnMiniblinId;
extern size_t gEnMiniblinStructSize;

// ---- Hammergeist / Molmauk (actors/z_en_hammergeist.c) ----
extern void EnHammergeist_Init(Actor* thisx, PlayState* play);
extern void EnHammergeist_Destroy(Actor* thisx, PlayState* play);
extern void EnHammergeist_Update(Actor* thisx, PlayState* play);
extern void EnHammergeist_Draw(Actor* thisx, PlayState* play);
extern s16 gEnHammergeistId;
extern size_t gEnHammergeistStructSize;

// ---- Scissors Beetle (actors/z_en_sbeetle.c) ----
extern void EnSbeetle_Init(Actor* thisx, PlayState* play);
extern void EnSbeetle_Destroy(Actor* thisx, PlayState* play);
extern void EnSbeetle_Update(Actor* thisx, PlayState* play);
extern void EnSbeetle_Draw(Actor* thisx, PlayState* play);
extern s16 gEnSbeetleId;
extern size_t gEnSbeetleStructSize;

void Trutefel_EnsureActorsRegistered(void);

} // extern "C"

// One canonical mesh per enemy inside trutefel-enemies.o2r — if these resolve, the archive
// is mounted and every limb DL/texture the actors reference is available.
static const char* const sMiniblinGatePath =
    "__OTR__objects/trutefel/object_miniblin/gMiniblinSkel_body_mesh_layer_Opaque";
static const char* const sHammergeistGatePath =
    "__OTR__objects/trutefel/object_hammergeist/gHammergeistSkel_body_mesh_layer_Opaque";
static const char* const sSbeetleGatePath =
    "__OTR__objects/trutefel/object_sbeetle/gScissorsBeetleSkel_bodyfront_mesh_layer_Opaque";

// Reference FLAGS combos, spelled with SoH's flag names (same bits):
//   Miniblin:    ACTOR_FLAG_0 | 2 | 4 | 9   (targetable, hostile, update outside cull, hookshottable)
//   Hammergeist: ACTOR_FLAG_0 | 2 | 4 | 5   (targetable, hostile, update+draw outside cull)
//   Sbeetle:     ACTOR_FLAG_0 | 2 | 4 | 9 | 18 (miniblin set + Navi C-up dialogue)
#define TRUTEFEL_MINIBLIN_FLAGS                                                               \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR)
#define TRUTEFEL_HAMMERGEIST_FLAGS                                                            \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_DRAW_CULLING_DISABLED)
#define TRUTEFEL_SBEETLE_FLAGS (TRUTEFEL_MINIBLIN_FLAGS | ACTOR_FLAG_TALK_WITH_C_UP)

// Scissors Beetle Navi hint (naviEnemyId 0x5D -> textId 0x600 + 0x5D)
#define TRUTEFEL_SBEETLE_NAVI_TEXTID 0x065D

void Trutefel_EnsureActorsRegistered(void) {
    // Idempotence guard: all three are registered together, so one id answers for all.
    if (gEnMiniblinId != -1) {
        return;
    }

    // Archive gate: leave ids at -1 (spawn helpers/console no-op) when trutefel-enemies.o2r
    // isn't mounted.
    if (!ResourceMgr_FileExists(sMiniblinGatePath) || !ResourceMgr_FileExists(sHammergeistGatePath) ||
        !ResourceMgr_FileExists(sSbeetleGatePath)) {
        return;
    }

    // Miniblin — Wind Waker-style rupee thief
    {
        ActorDBInit init;
        init.name = "En_Miniblin";
        init.desc = "Miniblin (trueffel custom enemy, steals rupees)";
        init.category = ACTORCAT_ENEMY;
        init.flags = TRUTEFEL_MINIBLIN_FLAGS;
        init.objectId = OBJECT_GAMEPLAY_KEEP; // models come from trutefel-enemies.o2r, no scene object needed
        init.instanceSize = gEnMiniblinStructSize;
        init.init = EnMiniblin_Init;
        init.destroy = EnMiniblin_Destroy;
        init.update = EnMiniblin_Update;
        init.draw = EnMiniblin_Draw;
        gEnMiniblinId = ActorDB::Instance->AddEntry(init).entry.id;
    }

    // Hammergeist (aka Molmauk) — ice hammer + fire hammer bruiser
    {
        ActorDBInit init;
        init.name = "En_Hammergeist";
        init.desc = "Molmauk / Hammergeist (trueffel custom enemy, ice+fire hammers)";
        init.category = ACTORCAT_ENEMY;
        init.flags = TRUTEFEL_HAMMERGEIST_FLAGS;
        init.objectId = OBJECT_GAMEPLAY_KEEP;
        init.instanceSize = gEnHammergeistStructSize;
        init.init = EnHammergeist_Init;
        init.destroy = EnHammergeist_Destroy;
        init.update = EnHammergeist_Update;
        init.draw = EnHammergeist_Draw;
        gEnHammergeistId = ActorDB::Instance->AddEntry(init).entry.id;
    }

    // Scissors Beetle — Minish Cap-style boomerang pincers
    {
        ActorDBInit init;
        init.name = "En_Sbeetle";
        init.desc = "Scissors Beetle (trueffel custom enemy, boomerang pincers)";
        init.category = ACTORCAT_ENEMY;
        init.flags = TRUTEFEL_SBEETLE_FLAGS;
        init.objectId = OBJECT_GAMEPLAY_KEEP;
        init.instanceSize = gEnSbeetleStructSize;
        init.init = EnSbeetle_Init;
        init.destroy = EnSbeetle_Destroy;
        init.update = EnSbeetle_Update;
        init.draw = EnSbeetle_Draw;
        gEnSbeetleId = ActorDB::Instance->AddEntry(init).entry.id;
    }
}

// Scissors Beetle Navi hint (reference sbeetle_message_data.h, english entry).
// CustomMessage: & = newline, %c = light blue, %w = white (AutoFormat handles wrapping).
static void Trutefel_BuildSbeetleNaviMessage(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg = CustomMessage("Scissors Beetle&%cIt attacks by throwing its pincers like "
                                      "boomerangs! Keep moving, then strike when they return!%w");
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

static void Trutefel_RegisterHooks() {
    // ShipInit::InitAll can fire more than once (e.g. preset apply) — hooks register once.
    static bool sHooksRegistered = false;

    // Boot-time attempt (archives are already indexed at this point).
    Trutefel_EnsureActorsRegistered();

    if (sHooksRegistered) {
        return;
    }
    sHooksRegistered = true;

    // Cheap retry while unregistered (guarded no-op once ids are set) — covers any case
    // where the archive shows up after the first attempt.
    static HOOK_ID sFrameHookId = 0;
    if (gEnMiniblinId == -1 && sFrameHookId == 0) {
        sFrameHookId = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>([]() {
            if (gEnMiniblinId == -1) {
                Trutefel_EnsureActorsRegistered();
            }
        });
    }

    // Navi C-up text for the Scissors Beetle (safe to register even without the archive —
    // the textId is only ever requested by a spawned En_Sbeetle).
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(TRUTEFEL_SBEETLE_NAVI_TEXTID,
                                                                                Trutefel_BuildSbeetleNaviMessage);
}

static RegisterShipInitFunc sTrutefelEnemiesInit(Trutefel_RegisterHooks);
