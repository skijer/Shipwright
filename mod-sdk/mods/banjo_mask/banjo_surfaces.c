#include "banjo_surfaces.h"

#include <math.h>
#include <stdbool.h>

#include "banjo_world.h"

#define GATHER_RADIUS 700.0f
#define PRISM_MAX 64
#define PRISM_SIDES 8
#define BG_ACTOR_IS_IN_USE 1

typedef struct GatherBox {
    Vec3f min;
    Vec3f max;
} GatherBox;

// Both games number their floor materials alike: dirt, sand, stone, wet dirt, shallow water, deep water, tall grass,
// lava, grass, bridge, wood, soft dirt, ice, carpet, snow.
static const LibBkFootstep kFootstepByMaterial[] = {
    LIBBK_FOOTSTEP_DIRT,          LIBBK_FOOTSTEP_SOFT,       LIBBK_FOOTSTEP_STONE,  LIBBK_FOOTSTEP_SHALLOW_WATER,
    LIBBK_FOOTSTEP_SHALLOW_WATER, LIBBK_FOOTSTEP_DEEP_WATER, LIBBK_FOOTSTEP_LEAVES, LIBBK_FOOTSTEP_STONE,
    LIBBK_FOOTSTEP_DIRT,          LIBBK_FOOTSTEP_WOOD,       LIBBK_FOOTSTEP_WOOD,   LIBBK_FOOTSTEP_DIRT,
    LIBBK_FOOTSTEP_SNOW,          LIBBK_FOOTSTEP_SOFT,       LIBBK_FOOTSTEP_SNOW,
};

static Cylinder16 sPrisms[PRISM_MAX];
static int32_t sPrismCount;

static const float kPrismDirections[PRISM_SIDES][2] = {
    { 1.000000f, 0.000000f },  { 0.707107f, 0.707107f },   { 0.000000f, 1.000000f },  { -0.707107f, 0.707107f },
    { -1.000000f, 0.000000f }, { -0.707107f, -0.707107f }, { 0.000000f, -1.000000f }, { 0.707107f, -0.707107f },
};

static GatherBox MakeGatherBox(Vec3f center) {
    GatherBox box = {
        { center.x - GATHER_RADIUS, center.y - GATHER_RADIUS, center.z - GATHER_RADIUS },
        { center.x + GATHER_RADIUS, center.y + GATHER_RADIUS, center.z + GATHER_RADIUS },
    };

    return box;
}

static bool IsTriangleInBox(const Vec3f corners[3], const GatherBox* box) {
    const float minX = fminf(corners[0].x, fminf(corners[1].x, corners[2].x));
    const float maxX = fmaxf(corners[0].x, fmaxf(corners[1].x, corners[2].x));
    const float minY = fminf(corners[0].y, fminf(corners[1].y, corners[2].y));
    const float maxY = fmaxf(corners[0].y, fmaxf(corners[1].y, corners[2].y));
    const float minZ = fminf(corners[0].z, fminf(corners[1].z, corners[2].z));
    const float maxZ = fmaxf(corners[0].z, fmaxf(corners[1].z, corners[2].z));

    return maxX >= box->min.x && minX <= box->max.x && maxY >= box->min.y && minY <= box->max.y &&
           maxZ >= box->min.z && minZ <= box->max.z;
}

static void WriteTriangle(LibBkTriangle* triangle, const Vec3f corners[3], uint32_t flags) {
    for (int32_t corner = 0; corner < 3; corner++) {
        triangle->vertices[corner][0] = corners[corner].x * BANJO_BK_UNITS_PER_GAME_UNIT;
        triangle->vertices[corner][1] = corners[corner].y * BANJO_BK_UNITS_PER_GAME_UNIT;
        triangle->vertices[corner][2] = corners[corner].z * BANJO_BK_UNITS_PER_GAME_UNIT;
    }
    triangle->flags = flags;
}

static Vec3f ToVec3f(const Vec3i* vertex) {
    Vec3f position = { (float)vertex->x, (float)vertex->y, (float)vertex->z };

    return position;
}

// BK takes the normal from the winding, so the corners turn whichever way agrees with the game's own normal.
static void ReadPolyCorners(const CollisionPoly* poly, const Vec3i* vertices, Vec3f corners[3]) {
    const Vec3f first = ToVec3f(&vertices[COLPOLY_VTX_INDEX(poly->flags_vIA)]);
    const Vec3f second = ToVec3f(&vertices[COLPOLY_VTX_INDEX(poly->flags_vIB)]);
    const Vec3f third = ToVec3f(&vertices[poly->vIC]);
    const Vec3f edgeA = { second.x - first.x, second.y - first.y, second.z - first.z };
    const Vec3f edgeB = { third.x - first.x, third.y - first.y, third.z - first.z };
    const float windingDotNormal = (edgeA.y * edgeB.z - edgeA.z * edgeB.y) * poly->normal.x +
                                   (edgeA.z * edgeB.x - edgeA.x * edgeB.z) * poly->normal.y +
                                   (edgeA.x * edgeB.y - edgeA.y * edgeB.x) * poly->normal.z;
    const bool isReversed = windingDotNormal < 0.0f;

    corners[0] = first;
    corners[1] = isReversed ? third : second;
    corners[2] = isReversed ? second : third;
}

static uint32_t GetSurfaceFlags(PlayState* play, CollisionPoly* poly, int32_t bgId) {
    const uint32_t material = (uint32_t)BANJO_SURFACE_MATERIAL(&play->colCtx, poly, bgId);

    if (material >= ARRAY_COUNT(kFootstepByMaterial)) {
        return LIBBK_SURFACE_SOLID;
    }
    return LIBBK_SURFACE_FOOTSTEP(kFootstepByMaterial[material]);
}

static int32_t AppendPoly(PlayState* play, CollisionPoly* poly, int32_t bgId, const Vec3i* vertices,
                          const GatherBox* box, LibBkTriangle* out) {
    Vec3f corners[3];

    if (SurfaceType_IsIgnoredByEntities(&play->colCtx, poly, bgId)) {
        return 0;
    }
    ReadPolyCorners(poly, vertices, corners);
    if (!IsTriangleInBox(corners, box)) {
        return 0;
    }
    WriteTriangle(out, corners, GetSurfaceFlags(play, poly, bgId));
    return 1;
}

static int32_t AppendScenePolys(PlayState* play, const GatherBox* box, LibBkTriangle* out, int32_t capacity) {
    CollisionHeader* header = play->colCtx.colHeader;
    int32_t count = 0;

    for (uint32_t index = 0; index < header->numPolygons && count < capacity; index++) {
        count += AppendPoly(play, &header->polyList[index], BGCHECK_SCENE, header->vtxList, box, &out[count]);
    }
    return count;
}

// Dynapoly vertices are already in world space: the game applies each platform's transform when it moves.
static int32_t AppendDynaPolys(PlayState* play, const GatherBox* box, LibBkTriangle* out, int32_t capacity) {
    DynaCollisionContext* dyna = &play->colCtx.dyna;
    int32_t count = 0;

    if (dyna->polyList == NULL || dyna->vtxList == NULL) {
        return 0;
    }
    for (int32_t bgId = 0; bgId < dyna->bgActorMax && count < capacity; bgId++) {
        BgActor* bgActor = &dyna->bgActors[bgId];

        if (!(dyna->bgActorFlags[bgId] & BG_ACTOR_IS_IN_USE) || bgActor->colHeader == NULL) {
            continue;
        }

        const int32_t start = bgActor->dynaLookup.polyStartIndex;
        const int32_t end = MIN(start + (int32_t)bgActor->colHeader->numPolygons, (int32_t)dyna->polyListMax);

        for (int32_t index = start; index < end && count < capacity; index++) {
            count += AppendPoly(play, &dyna->polyList[index], bgId, dyna->vtxList, box, &out[count]);
        }
    }
    return count;
}

// Two upward triangles across the part of the waterbox around Banjo, flagged as BK water.
static int32_t AppendWaterBoxes(PlayState* play, const GatherBox* box, LibBkTriangle* out, int32_t capacity) {
    CollisionHeader* header = play->colCtx.colHeader;
    int32_t count = 0;

    for (uint32_t index = 0; index < header->numWaterBoxes && count + 2 <= capacity; index++) {
        const WaterBox* water = &header->waterBoxes[index];
        const float surface = (float)BANJO_WATERBOX_SURFACE(water);
        const float minX = fmaxf((float)BANJO_WATERBOX_MIN_X(water), box->min.x);
        const float maxX = fminf((float)(BANJO_WATERBOX_MIN_X(water) + water->xLength), box->max.x);
        const float minZ = fmaxf((float)BANJO_WATERBOX_MIN_Z(water), box->min.z);
        const float maxZ = fminf((float)(BANJO_WATERBOX_MIN_Z(water) + water->zLength), box->max.z);

        if (minX >= maxX || minZ >= maxZ || surface < box->min.y || surface > box->max.y) {
            continue;
        }

        const Vec3f nearLeft = { minX, surface, minZ };
        const Vec3f nearRight = { maxX, surface, minZ };
        const Vec3f farRight = { maxX, surface, maxZ };
        const Vec3f farLeft = { minX, surface, maxZ };
        const Vec3f first[3] = { nearLeft, farLeft, farRight };
        const Vec3f second[3] = { nearLeft, farRight, nearRight };

        WriteTriangle(&out[count++], first, LIBBK_SURFACE_WATER);
        WriteTriangle(&out[count++], second, LIBBK_SURFACE_WATER);
    }
    return count;
}

// The walls face outwards, so BK treats each one as solid from outside the prop.
static int32_t AppendPrism(const Cylinder16* cylinder, LibBkTriangle* out) {
    const float bottom = cylinder->pos.y + cylinder->yShift;
    const float top = bottom + cylinder->height;

    for (int32_t side = 0; side < PRISM_SIDES; side++) {
        const float* from = kPrismDirections[side];
        const float* to = kPrismDirections[(side + 1) % PRISM_SIDES];
        const float fromX = cylinder->pos.x + from[0] * cylinder->radius;
        const float fromZ = cylinder->pos.z + from[1] * cylinder->radius;
        const float toX = cylinder->pos.x + to[0] * cylinder->radius;
        const float toZ = cylinder->pos.z + to[1] * cylinder->radius;
        const Vec3f lower[3] = { { toX, bottom, toZ }, { fromX, bottom, fromZ }, { fromX, top, fromZ } };
        const Vec3f upper[3] = { { toX, bottom, toZ }, { fromX, top, fromZ }, { toX, top, toZ } };

        WriteTriangle(&out[side * 2], lower, LIBBK_SURFACE_SOLID);
        WriteTriangle(&out[side * 2 + 1], upper, LIBBK_SURFACE_SOLID);
    }
    return PRISM_SIDES * 2;
}

static int32_t AppendPrisms(LibBkTriangle* out, int32_t capacity) {
    int32_t count = 0;

    for (int32_t index = 0; index < sPrismCount && count + PRISM_SIDES * 2 <= capacity; index++) {
        count += AppendPrism(&sPrisms[index], &out[count]);
    }
    return count;
}

int32_t BanjoSurfaces_Gather(PlayState* play, Vec3f center, LibBkTriangle* triangles, int32_t capacity) {
    const GatherBox box = MakeGatherBox(center);
    int32_t count = 0;

    if (play->colCtx.colHeader == NULL) {
        return 0;
    }
    count += AppendScenePolys(play, &box, &triangles[count], capacity - count);
    count += AppendDynaPolys(play, &box, &triangles[count], capacity - count);
    count += AppendWaterBoxes(play, &box, &triangles[count], capacity - count);
    count += AppendPrisms(&triangles[count], capacity - count);
    return count;
}

// Enemies and bosses are left out so Banjo can still run into them to attack.
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

void BanjoSurfaces_CaptureActorColliders(PlayState* play) {
    CollisionCheckContext* checks = &play->colChkCtx;

    BanjoSurfaces_ClearActorColliders();
    for (int32_t index = 0; index < checks->colOCCount && sPrismCount < PRISM_MAX; index++) {
        const Collider* collider = checks->colOC[index];

        if (collider != NULL && IsBlockingProp(collider)) {
            sPrisms[sPrismCount++] = ((const ColliderCylinder*)collider)->dim;
        }
    }
}

void BanjoSurfaces_ClearActorColliders(void) {
    sPrismCount = 0;
}
