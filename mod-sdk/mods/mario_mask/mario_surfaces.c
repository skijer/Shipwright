#include "mario_surfaces.h"

#include <stdlib.h>

#include "libsm64/libsm64.h"
#include "mario_world.h"

#define ACTOR_SURFACE_MAX 1024
#define PRISM_SIDES 8
#define TRIANGLES_PER_PRISM_SIDE 2
#define FLOOR_MIN_NORMAL_Y 0.5f
#define BG_ACTOR_IS_IN_USE 1
#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME 16777619u

static struct SM64Surface sActorSurfaces[ACTOR_SURFACE_MAX];
static uint32_t sActorSurfaceCount;
static uint32_t sActorSignature;

// An octagon hugs the cylinder far closer than a box, whose corners stuck out as walls nobody could see.
static const float kPrismDirections[PRISM_SIDES][2] = {
    { 1.000000f, 0.000000f },  { 0.707107f, 0.707107f },   { 0.000000f, 1.000000f },  { -0.707107f, 0.707107f },
    { -1.000000f, 0.000000f }, { -0.707107f, -0.707107f }, { 0.000000f, -1.000000f }, { 0.707107f, -0.707107f },
};

static uint32_t HashValue(uint32_t hash, int32_t value) {
    return (hash ^ (uint32_t)value) * FNV_PRIME;
}

static void SetCorner(struct SM64Surface* surface, int32_t corner, float x, float y, float z) {
    surface->vertices[corner][0] = (int32_t)(x * MARIO_WORLD_SCALE);
    surface->vertices[corner][1] = (int32_t)(y * MARIO_WORLD_SCALE);
    surface->vertices[corner][2] = (int32_t)(z * MARIO_WORLD_SCALE);
}

// The game's winding is kept: libsm64 computes the same normal from it, so floors stay floors.
static void WritePolySurface(struct SM64Surface* surface, const CollisionPoly* poly, const Vec3i* vertices) {
    const Vec3i* first = &vertices[COLPOLY_VTX_INDEX(poly->flags_vIA)];
    const Vec3i* second = &vertices[COLPOLY_VTX_INDEX(poly->flags_vIB)];
    const Vec3i* third = &vertices[poly->vIC];

    *surface = (struct SM64Surface){ 0 };
    SetCorner(surface, 0, first->x, first->y, first->z);
    SetCorner(surface, 1, second->x, second->y, second->z);
    SetCorner(surface, 2, third->x, third->y, third->z);
}

static bool IsFloorPoly(const CollisionPoly* poly) {
    return COLPOLY_GET_NORMAL(poly->normal.y) > FLOOR_MIN_NORMAL_Y;
}

// Exit walls stay solid even floor-only: the game reads the wall Link touches to start the scene change.
static bool IsKeptPoly(PlayState* play, CollisionPoly* poly, int32_t bgId, bool isFloorOnly) {
    if (SurfaceType_IsIgnoredByEntities(&play->colCtx, poly, bgId)) {
        return false;
    }
    if (!isFloorOnly || IsFloorPoly(poly)) {
        return true;
    }
    return SurfaceType_GetSceneExitIndex(&play->colCtx, poly, bgId) != 0;
}

static uint32_t AppendScenePolys(PlayState* play, struct SM64Surface* out, bool isFloorOnly) {
    CollisionHeader* header = play->colCtx.colHeader;
    uint32_t count = 0;

    for (uint32_t index = 0; index < header->numPolygons; index++) {
        CollisionPoly* poly = &header->polyList[index];

        if (IsKeptPoly(play, poly, BGCHECK_SCENE, isFloorOnly)) {
            WritePolySurface(&out[count++], poly, header->vtxList);
        }
    }
    return count;
}

// Dynapoly vertices are already in world space: the game applies each platform's transform when it moves.
static uint32_t AppendDynaPolys(PlayState* play, struct SM64Surface* out, bool isFloorOnly) {
    DynaCollisionContext* dyna = &play->colCtx.dyna;
    const uint32_t capacity = (uint32_t)dyna->polyListMax;
    uint32_t count = 0;

    if (dyna->polyList == NULL || dyna->vtxList == NULL) {
        return 0;
    }
    for (int32_t bgId = 0; bgId < dyna->bgActorMax; bgId++) {
        BgActor* bgActor = &dyna->bgActors[bgId];

        if (!(dyna->bgActorFlags[bgId] & BG_ACTOR_IS_IN_USE) || bgActor->colHeader == NULL) {
            continue;
        }

        const uint32_t start = (uint32_t)bgActor->dynaLookup.polyStartIndex;
        uint32_t end = start + bgActor->colHeader->numPolygons;

        if (end > capacity) {
            end = capacity;
        }
        for (uint32_t index = start; index < end; index++) {
            if (IsKeptPoly(play, &dyna->polyList[index], bgId, isFloorOnly)) {
                WritePolySurface(&out[count++], &dyna->polyList[index], dyna->vtxList);
            }
        }
    }
    return count;
}

uint32_t MarioSurfaces_Load(PlayState* play, bool isFloorOnly) {
    if (play->colCtx.colHeader == NULL || play->colCtx.colHeader->numPolygons == 0) {
        return 0;
    }

    const uint32_t actorCount = isFloorOnly ? 0 : sActorSurfaceCount;
    const uint32_t capacity =
        play->colCtx.colHeader->numPolygons + (uint32_t)play->colCtx.dyna.polyListMax + actorCount;
    struct SM64Surface* surfaces = malloc(capacity * sizeof(struct SM64Surface));

    if (surfaces == NULL) {
        return 0;
    }

    uint32_t count = AppendScenePolys(play, surfaces, isFloorOnly);

    count += AppendDynaPolys(play, &surfaces[count], isFloorOnly);
    for (uint32_t index = 0; index < actorCount; index++) {
        surfaces[count++] = sActorSurfaces[index];
    }
    // An empty load would erase the surfaces libsm64 already has.
    if (count > 0) {
        sm64_static_surfaces_load(surfaces, count);
    }
    free(surfaces);
    return count;
}

static void AppendPrismTriangle(float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2,
                                float z2) {
    struct SM64Surface* surface = &sActorSurfaces[sActorSurfaceCount++];

    *surface = (struct SM64Surface){ 0 };
    SetCorner(surface, 0, x0, y0, z0);
    SetCorner(surface, 1, x1, y1, z1);
    SetCorner(surface, 2, x2, y2, z2);
}

// The walls face outwards, so libsm64 treats each one as solid from outside the prop.
static void AppendPrism(const Cylinder16* cylinder) {
    const float radius = cylinder->radius;
    const float centerX = cylinder->pos.x;
    const float centerZ = cylinder->pos.z;
    const float bottom = cylinder->pos.y + cylinder->yShift;
    const float top = bottom + cylinder->height;

    if (sActorSurfaceCount + PRISM_SIDES * TRIANGLES_PER_PRISM_SIDE > ACTOR_SURFACE_MAX) {
        return;
    }
    for (int32_t side = 0; side < PRISM_SIDES; side++) {
        const float* from = kPrismDirections[side];
        const float* to = kPrismDirections[(side + 1) % PRISM_SIDES];
        const float fromX = centerX + from[0] * radius;
        const float fromZ = centerZ + from[1] * radius;
        const float toX = centerX + to[0] * radius;
        const float toZ = centerZ + to[1] * radius;

        AppendPrismTriangle(toX, bottom, toZ, fromX, bottom, fromZ, fromX, top, fromZ);
        AppendPrismTriangle(toX, bottom, toZ, fromX, top, fromZ, toX, top, toZ);
    }
}

// Enemies and bosses are left out so Mario can still run into them to attack.
static bool IsBlockingProp(const Collider* collider) {
    const Actor* actor = collider->actor;

    if (actor == NULL || actor->category == ACTORCAT_PLAYER || actor->category == ACTORCAT_ENEMY ||
        actor->category == ACTORCAT_BOSS) {
        return false;
    }
    if (!(collider->ocFlags1 & OC1_ON) || !(collider->ocFlags1 & OC1_TYPE_PLAYER)) {
        return false;
    }
    return collider->shape == COLSHAPE_CYLINDER && ((const ColliderCylinder*)collider)->dim.radius > 0;
}

void MarioSurfaces_CaptureActorColliders(PlayState* play) {
    CollisionCheckContext* checks = &play->colChkCtx;

    MarioSurfaces_ClearActorColliders();
    for (int32_t index = 0; index < checks->colOCCount; index++) {
        const Collider* collider = checks->colOC[index];

        if (collider == NULL || !IsBlockingProp(collider)) {
            continue;
        }

        const Cylinder16* cylinder = &((const ColliderCylinder*)collider)->dim;

        AppendPrism(cylinder);
        sActorSignature = HashValue(sActorSignature, cylinder->pos.x);
        sActorSignature = HashValue(sActorSignature, cylinder->pos.y + cylinder->yShift);
        sActorSignature = HashValue(sActorSignature, cylinder->pos.z);
        sActorSignature = HashValue(sActorSignature, cylinder->height);
        sActorSignature = HashValue(sActorSignature, cylinder->radius);
    }
}

void MarioSurfaces_ClearActorColliders(void) {
    sActorSurfaceCount = 0;
    sActorSignature = 0;
}

// Whole game units only, so a platform's idle sub-unit bob does not rebuild every frame.
uint32_t MarioSurfaces_GetLiveSignature(PlayState* play) {
    DynaCollisionContext* dyna = &play->colCtx.dyna;
    uint32_t signature = FNV_OFFSET_BASIS;

    for (int32_t bgId = 0; bgId < dyna->bgActorMax; bgId++) {
        const BgActor* bgActor = &dyna->bgActors[bgId];

        if (!(dyna->bgActorFlags[bgId] & BG_ACTOR_IS_IN_USE)) {
            continue;
        }
        signature = HashValue(signature, bgId);
        signature = HashValue(signature, (int32_t)bgActor->dynaLookup.polyStartIndex);
        signature = HashValue(signature, (int32_t)bgActor->curTransform.pos.x);
        signature = HashValue(signature, (int32_t)bgActor->curTransform.pos.y);
        signature = HashValue(signature, (int32_t)bgActor->curTransform.pos.z);
        signature = HashValue(signature, bgActor->curTransform.rot.x);
        signature = HashValue(signature, bgActor->curTransform.rot.y);
        signature = HashValue(signature, bgActor->curTransform.rot.z);
    }
    return signature ^ sActorSignature;
}

float MarioSurfaces_GetWaterLevel(PlayState* play, float x, float z) {
    float surface;
    WaterBox* waterBox;

    if (WaterBox_GetSurface1(play, &play->colCtx, x, z, &surface, &waterBox)) {
        return surface;
    }
    return MARIO_NO_WATER;
}
