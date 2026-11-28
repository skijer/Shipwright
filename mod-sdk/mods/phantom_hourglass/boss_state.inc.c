// Snapshots of a boss's instance past the Actor header, so a recall takes back its AI (a stun included).
// Health, list links and the update functions live in the header and are never part of one.
// Unity-included by phantom_hourglass.c.
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h"
#include "overlays/actors/ovl_Boss_Dodongo/z_boss_dodongo.h"

#define BOSS_STATE_HISTORY 200      // HOURGLASS_FRAMES
#define BOSS_STATE_BUDGET 0x800000   // snapshot bytes per boss: Majora gets every other frame, the rest every one
#define BOSS_STATE_MAX_BODY 0x20000
#define BOSS_STATE_MAX_LINKS 64
#define BOSS_STATE_MAX_LIVE 512
#define BOSS_STATE_MAX_ELEMENTS 64
#define BOSS_STATE_COL_TYPE_COUNT (COLTYPE_TREE + 1) // OoT: MM names it colMaterial / COL_MATERIAL_TREE
#define BOSS_STATE_SHAPE_COUNT COLSHAPE_INVALID       // OoT: MM adds a sphere shape before COLSHAPE_MAX

/** A word of the body that pointed into another actor when the snapshot was taken. */
typedef struct {
    size_t offset;
    Actor* target;
    s16 targetId;
} BossStateLink;

typedef struct {
    u32 serial;
    bool isValid;
    u32 flags;
    f32 gravity;
    f32 shapeYOffset;
    u16 colorFilterParams;
    u8 colorFilterTimer;
    s32 linkCount;
    BossStateLink links[BOSS_STATE_MAX_LINKS];
} BossSnapshot;

typedef struct BossState {
    size_t bodySize;
    s32 interval; // banked frames per snapshot
    s32 snapshotCount;
    s32 next;
    BossSnapshot* snapshots;
    u8* shadow; // the body as of the last HasChanged
    u8* bodies; // snapshotCount * bodySize
} BossState;

typedef struct {
    uintptr_t start;
    uintptr_t end;
    Actor* actor;
} LiveActor;

static LiveActor sLive[BOSS_STATE_MAX_LIVE];
static s32 sLiveCount;
static bool sIsLiveOverflowing;

static size_t GetInstanceSize(Actor* actor) {
    ActorDBEntry* entry = ActorDB_Retrieve(actor->id);

    return entry == NULL ? 0 : entry->instanceSize;
}

// King Dodongo keeps his health in his own field, inside the body a snapshot restores.
static s16* GetOwnHealth(Actor* actor) {
    return actor->id == ACTOR_BOSS_DODONGO ? &((BossDodongo*)actor)->health : NULL;
}

static bool IsBossAlive(Actor* actor) {
    s16* ownHealth = GetOwnHealth(actor);

    return ownHealth != NULL ? *ownHealth > 0 : actor->colChkInfo.health > 0;
}

static u8* GetBody(Actor* actor) {
    return (u8*)actor + sizeof(Actor);
}

static u8* GetSnapshotBody(BossState* state, s32 index) {
    return state->bodies + index * state->bodySize;
}

static BossState* BossState_Create(Actor* actor) {
    size_t size = GetInstanceSize(actor);

    if (size <= sizeof(Actor) || size > sizeof(Actor) + BOSS_STATE_MAX_BODY) {
        return NULL;
    }
    size_t bodySize = size - sizeof(Actor);
    s32 interval = MAX((s32)((BOSS_STATE_HISTORY * bodySize + BOSS_STATE_BUDGET - 1) / BOSS_STATE_BUDGET), 1);
    s32 snapshotCount = (BOSS_STATE_HISTORY + interval - 1) / interval;
    BossState* state =
        calloc(1, sizeof(BossState) + snapshotCount * sizeof(BossSnapshot) + (snapshotCount + 1) * bodySize);

    if (state == NULL) {
        return NULL;
    }
    state->bodySize = bodySize;
    state->interval = interval;
    state->snapshotCount = snapshotCount;
    state->snapshots = (BossSnapshot*)(state + 1);
    state->shadow = (u8*)(state->snapshots + snapshotCount);
    state->bodies = state->shadow + bodySize;
    memcpy(state->shadow, GetBody(actor), bodySize);
    return state;
}

static void BossState_Destroy(BossState* state) {
    free(state);
}

static bool BossState_HasChanged(BossState* state, Actor* actor) {
    if (memcmp(state->shadow, GetBody(actor), state->bodySize) == 0) {
        return false;
    }
    memcpy(state->shadow, GetBody(actor), state->bodySize);
    return true;
}

static bool BossState_IsCalm(PlayState* play, Actor* actor) {
    Player* player = GET_PLAYER(play);

    if (!IsBossAlive(actor) || play->csCtx.state != CS_STATE_IDLE) {
        return false;
    }
    // OoT: MM also asks its CutsceneManager, and names the camera index activeCamId.
    if (play->activeCamera != CAM_ID_MAIN) {
        return false;
    }
    return player == NULL || !(player->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE);
}

static int CompareLive(const void* first, const void* second) {
    uintptr_t a = ((const LiveActor*)first)->start;
    uintptr_t b = ((const LiveActor*)second)->start;

    return (a > b) - (a < b);
}

static void GatherLiveActors(PlayState* play) {
    sLiveCount = 0;
    sIsLiveOverflowing = false;
    for (u8 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = play->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
            size_t size = GetInstanceSize(actor);

            if (sLiveCount >= BOSS_STATE_MAX_LIVE) {
                sIsLiveOverflowing = true;
                return;
            }
            sLive[sLiveCount].start = (uintptr_t)actor;
            sLive[sLiveCount].end = (uintptr_t)actor + (size != 0 ? size : sizeof(Actor));
            sLive[sLiveCount].actor = actor;
            sLiveCount++;
        }
    }
    qsort(sLive, sLiveCount, sizeof(LiveActor), CompareLive);
}

static Actor* FindOwner(uintptr_t address) {
    s32 low = 0;
    s32 high = sLiveCount - 1;

    while (low <= high) {
        s32 middle = (low + high) / 2;

        if (address < sLive[middle].start) {
            high = middle - 1;
        } else if (address >= sLive[middle].end) {
            low = middle + 1;
        } else {
            return sLive[middle].actor;
        }
    }
    return NULL;
}

// Actor memory is pooled: the same address with another id is somebody else.
static bool IsStillAlive(BossStateLink* link) {
    for (s32 index = 0; index < sLiveCount; index++) {
        if (sLive[index].actor == link->target) {
            return link->target->id == link->targetId;
        }
    }
    return false;
}

static bool CollectLinks(Actor* actor, size_t bodySize, BossSnapshot* snapshot) {
    u8* body = GetBody(actor);

    snapshot->linkCount = 0;
    for (size_t offset = 0; offset + sizeof(uintptr_t) <= bodySize; offset += sizeof(uintptr_t)) {
        uintptr_t value;

        memcpy(&value, body + offset, sizeof(value));
        Actor* owner = FindOwner(value);
        if (owner == NULL || owner == actor) {
            continue;
        }
        if (snapshot->linkCount >= BOSS_STATE_MAX_LINKS) {
            return false;
        }
        snapshot->links[snapshot->linkCount].offset = offset;
        snapshot->links[snapshot->linkCount].target = owner;
        snapshot->links[snapshot->linkCount].targetId = owner->id;
        snapshot->linkCount++;
    }
    return true;
}

static void BossState_Capture(BossState* state, PlayState* play, Actor* actor, u32 serial) {
    s32 index = state->next;
    BossSnapshot* snapshot = &state->snapshots[index];

    if (serial % state->interval != 0) {
        return;
    }
    state->next = (index + 1) % state->snapshotCount;
    snapshot->isValid = false;
    if (!BossState_IsCalm(play, actor)) {
        return;
    }
    GatherLiveActors(play);
    if (sIsLiveOverflowing || !CollectLinks(actor, state->bodySize, snapshot)) {
        return;
    }
    memcpy(GetSnapshotBody(state, index), GetBody(actor), state->bodySize);
    snapshot->serial = serial;
    snapshot->flags = actor->flags;
    snapshot->gravity = actor->gravity;
    snapshot->shapeYOffset = actor->shape.yOffset;
    snapshot->colorFilterParams = actor->colorFilterParams;
    snapshot->colorFilterTimer = actor->colorFilterTimer;
    snapshot->isValid = true;
}

static s32 FindSnapshot(BossState* state, u32 serial) {
    for (s32 index = 0; index < state->snapshotCount; index++) {
        if (state->snapshots[index].isValid && state->snapshots[index].serial == serial) {
            return index;
        }
    }
    return -1;
}

static bool BossState_FindAtOrBefore(BossState* state, u32 oldest, u32 latest, u32* serial) {
    bool isFound = false;

    for (s32 index = 0; index < state->snapshotCount; index++) {
        BossSnapshot* snapshot = &state->snapshots[index];

        if (!snapshot->isValid || snapshot->serial < oldest || snapshot->serial > latest) {
            continue;
        }
        if (!isFound || snapshot->serial > *serial) {
            *serial = snapshot->serial;
            isFound = true;
        }
    }
    return isFound;
}

static void ResetCollider(PlayState* play, Collider* collider) {
    switch (collider->shape) {
        case COLSHAPE_JNTSPH:
            Collider_ResetJntSphAT(play, collider);
            Collider_ResetJntSphAC(play, collider);
            Collider_ResetJntSphOC(play, collider);
            break;
        case COLSHAPE_CYLINDER:
            Collider_ResetCylinderAT(play, collider);
            Collider_ResetCylinderAC(play, collider);
            Collider_ResetCylinderOC(play, collider);
            break;
        case COLSHAPE_TRIS:
            Collider_ResetTrisAT(play, collider);
            Collider_ResetTrisAC(play, collider);
            Collider_ResetTrisOC(play, collider);
            break;
        case COLSHAPE_QUAD:
            Collider_ResetQuadAT(play, collider);
            Collider_ResetQuadAC(play, collider);
            Collider_ResetQuadOC(play, collider);
            break;
        default:
            break;
    }
}

static bool IsOwnCollider(Actor* actor, u8* candidate, size_t room) {
    Collider* collider = (Collider*)candidate;

    if (room < sizeof(Collider) || collider->actor != actor || collider->shape >= BOSS_STATE_SHAPE_COUNT ||
        collider->colType >= BOSS_STATE_COL_TYPE_COUNT) {
        return false;
    }
    if (collider->shape != COLSHAPE_JNTSPH && collider->shape != COLSHAPE_TRIS) {
        return true;
    }
    if (room < sizeof(ColliderJntSph)) {
        return false;
    }
    // JntSph and Tris share the count/elements layout.
    ColliderJntSph* elements = (ColliderJntSph*)collider;
    return elements->count >= 0 && elements->count <= BOSS_STATE_MAX_ELEMENTS &&
           (elements->count == 0 || elements->elements != NULL);
}

// A hit banked in the past names an attacker that may be gone: the restored body forgets it.
static void ResetColliders(PlayState* play, Actor* actor, size_t bodySize) {
    u8* body = GetBody(actor);

    for (size_t offset = 0; offset + sizeof(uintptr_t) <= bodySize; offset += sizeof(uintptr_t)) {
        if (IsOwnCollider(actor, body + offset, bodySize - offset)) {
            ResetCollider(play, (Collider*)(body + offset));
        }
    }
}

static void BossState_Restore(BossState* state, PlayState* play, Actor* actor, u32 serial) {
    s32 index = FindSnapshot(state, serial);
    uintptr_t present[BOSS_STATE_MAX_LINKS];
    u8* body = GetBody(actor);
    s16* ownHealth = GetOwnHealth(actor);

    if (index < 0) {
        return;
    }
    BossSnapshot* snapshot = &state->snapshots[index];
    s16 presentHealth = ownHealth != NULL ? *ownHealth : 0;

    for (s32 link = 0; link < snapshot->linkCount; link++) {
        memcpy(&present[link], body + snapshot->links[link].offset, sizeof(uintptr_t));
    }
    memcpy(body, GetSnapshotBody(state, index), state->bodySize);
    if (ownHealth != NULL) {
        *ownHealth = presentHealth;
    }
    // A child that died since keeps the field's present value, never the dangling past one.
    GatherLiveActors(play);
    for (s32 link = 0; link < snapshot->linkCount; link++) {
        if (!IsStillAlive(&snapshot->links[link])) {
            memcpy(body + snapshot->links[link].offset, &present[link], sizeof(uintptr_t));
        }
    }
    actor->flags = snapshot->flags;
    actor->gravity = snapshot->gravity;
    actor->shape.yOffset = snapshot->shapeYOffset;
    actor->colorFilterParams = snapshot->colorFilterParams;
    actor->colorFilterTimer = snapshot->colorFilterTimer;
    ResetColliders(play, actor, state->bodySize);
    memcpy(state->shadow, body, state->bodySize);
}

static void BossState_ForgetFrom(BossState* state, u32 serial) {
    for (s32 index = 0; index < state->snapshotCount; index++) {
        if (state->snapshots[index].serial >= serial) {
            state->snapshots[index].isValid = false;
        }
    }
}
