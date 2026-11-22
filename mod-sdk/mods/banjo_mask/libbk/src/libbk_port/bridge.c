#include "../../shim/libbk_build.h"
// Answers the BK systems the player code talks to (input, camera, collision, assets) from the host's data.
#include "../../shim/ultra64.h"
#include <stdlib.h>
#include <string.h>
#include "../../include/functions.h"
#include "../../include/variables.h"
#include "../../include/core2/ba/physics.h"
#include "../../include/core2/abilityprogress.h"
#include "../../include/core2/playerposition.h"
#include "../../include/core2/yaw.h"
#include "../../src/libbk_port/libbk.h"

#define ASSET_ID_MAX 0x1000
#define ASSET_FIRST_MODEL 0x2D1
#define BK_MAP_MUMBOS_MOUNTAIN 2
#define COLLISION_VERTEX_SCALE 2.0f
#define COLLISION_BOUNDS_EXTENT 1000000
#define HOST_FRAMES_PER_UPDATE 3
#define MODEL_LIFT 2.0f
#define HURT_REBOUND BA_REBOUND_4_UNK

void bsmethods_reset(void);
void func_80295A8C(void);
void func_80295C14(void);
void func_80320B44(void* line, void* movingSphere, void* sphere, void* lastHit);
void func_80320B84(void);
void func_8028F85C(f32 position[3]);
void modelAppendages_loadAppendage(void);
void animcommoncache_init(void);
void func_80296CC0(f32 position[3]);
void func_80296CB4(s32 reboundId);
AnimCtrl* player_getAnimCtrlPtr(void);
enum player_model_direction_e baModel_getDirection(void);
s32 baModel_isVisible(void);
extern f32 baModelDisplacement[3];
BKCollisionTriangle* collisionList_intersectLineGlobal(BKCollisionList* list, BKVertexList* vertices, f32 position[3],
                                                       f32 rotation[3], f32 scale, f32 start[3], f32 end[3],
                                                       f32 normal[3], u32 filter);
BKCollisionTriangle* collisionList_intersectMovingSphereGlobal(BKCollisionList* list, BKVertexList* vertices,
                                                               f32 position[3], f32 rotation[3], f32 scale,
                                                               f32 from[3], f32 to[3], f32 radius, f32 normal[3],
                                                               s32 steps, u32 filter);
BKCollisionTriangle* collisionList_intersectSphereGlobal(BKCollisionList* list, BKVertexList* vertices,
                                                         f32 position[3], f32 rotation[3], f32 scale, f32 center[3],
                                                         f32 radius, f32 normal[3], u32 filter);

// BK's anti-tamper check flips the player model unless the boot chip reads as the retail 6103.
s32 osCicId = 6103;

static LibBkLoadAssetFunc sLoadAsset;
static void* sAssets[ASSET_ID_MAX];
static s32 sAssetSizes[ASSET_ID_MAX];
static s32 sHeldFrames[BUTTON_max];
static f32 sStick[2];
static f32 sViewportPosition[3];
static f32 sViewportRotation[3];
static s32 sGlobalTimer;
static u8 sSelectors[LIBBK_SELECTOR_COUNT];
static ActorMarker sPlayerMarker;
static f32 sCollisionCenter[3];
static f32 sNoRotation[3];
static s32 sCollisionTriangleCount;
static f64 sVertexStore[(sizeof(BKVertexList) + sizeof(Vtx) * LIBBK_TRIANGLE_MAX * 3) / sizeof(f64) + 1];
static f64 sCollisionStore[(sizeof(BKCollisionList) + sizeof(BKCollisionGeometry) +
                            sizeof(BKCollisionTriangle) * LIBBK_TRIANGLE_MAX) / sizeof(f64) + 1];

static u16 ReadBigEndian16(const u8* bytes) {
    return (u16)((bytes[0] << 8) | bytes[1]);
}

static void WriteNative16(u8* bytes, u16 value) {
    memcpy(bytes, &value, sizeof(value));
}

// Lighthouse's AnimFactory layout: native-endian halves with the bitfields reordered for the host.
static void ConvertAnimationToNative(u8* data) {
    s32 elementCount = (s16)ReadBigEndian16(data + 4);
    u8* cursor = data + 8;

    for (s32 i = 0; i < 4; i++) {
        WriteNative16(data + i * 2, ReadBigEndian16(data + i * 2));
    }
    for (s32 element = 0; element < elementCount; element++) {
        u16 boneAndChannel = ReadBigEndian16(cursor);
        s32 keyCount = (s16)ReadBigEndian16(cursor + 2);

        WriteNative16(cursor, (u16)(((boneAndChannel & 0xF) << 12) | (boneAndChannel >> 4)));
        WriteNative16(cursor + 2, (u16)keyCount);
        cursor += 4;
        for (s32 key = 0; key < keyCount; key++) {
            u16 easeAndFrame = ReadBigEndian16(cursor);

            WriteNative16(cursor, (u16)(((easeAndFrame & 0x3FFF) << 2) | (((easeAndFrame >> 14) & 1) << 1) |
                                        (easeAndFrame >> 15)));
            WriteNative16(cursor + 2, ReadBigEndian16(cursor + 2));
            cursor += 4;
        }
    }
}

char* ResourceMgr_LoadByAssetId(uint32_t assetId) {
    if (assetId >= ASSET_ID_MAX || sLoadAsset == NULL) {
        return NULL;
    }
    if (sAssets[assetId] == NULL) {
        sAssets[assetId] = sLoadAsset((s32)assetId, &sAssetSizes[assetId]);
        if (sAssets[assetId] != NULL && assetId < ASSET_FIRST_MODEL) {
            ConvertAnimationToNative(sAssets[assetId]);
        }
    }
    return sAssets[assetId];
}

char* ResourceMgr_ReloadByAssetId(uint32_t assetId) {
    return ResourceMgr_LoadByAssetId(assetId);
}

size_t ResourceMgr_GetResourceSize(uint32_t assetId) {
    return (assetId < ASSET_ID_MAX) ? (size_t)sAssetSizes[assetId] : 0;
}

void bkmemcpy64(void* destination, void* source, s32 size) {
    memcpy(destination, source, size);
}

void osWritebackDCache(void* address, s32 size) {
}

void FrameInterpolation_RecordMatrixToMtx(void* matrix, const float source[4][4]) {
}

int GraphicsPatches_IsLodDisabled(void) {
    return 1;
}

BKSpriteDisplayData* SpritePatches_GetOrCreateDisplayData(BKSprite* sprite) {
    return NULL;
}

void guMtxIdentF(float matrix[4][4]) {
    for (s32 row = 0; row < 4; row++) {
        for (s32 column = 0; column < 4; column++) {
            matrix[row][column] = (row == column) ? 1.0f : 0.0f;
        }
    }
}

float gu_sqrtf(float value) {
    return sqrtf(value);
}

void joy_update(void) {
}

s32 controller_getStartButton(s32 controllerIndex) {
    return sHeldFrames[BUTTON_START];
}

s32 controller_copySideButtons(s32 controllerIndex, s32 held[3]) {
    held[SIDE_BUTTON(BUTTON_Z)] = sHeldFrames[BUTTON_Z];
    held[SIDE_BUTTON(BUTTON_L)] = sHeldFrames[BUTTON_L];
    held[SIDE_BUTTON(BUTTON_R)] = sHeldFrames[BUTTON_R];
    return 0;
}

void controller_copyDpadButtons(s32 controllerIndex, s32 held[4]) {
    for (s32 button = BUTTON_D_UP; button <= BUTTON_D_RIGHT; button++) {
        held[DPAD_BUTTON(button)] = sHeldFrames[button];
    }
}

void controller_copyFaceButtons(s32 controllerIndex, s32 held[6]) {
    for (s32 button = BUTTON_A; button <= BUTTON_C_RIGHT; button++) {
        held[FACE_BUTTON(button)] = sHeldFrames[button];
    }
}

void controller_copyJoystick(s32 controllerIndex, f32 stick[2]) {
    stick[0] = sStick[0];
    stick[1] = sStick[1];
}

s32 globalTimer_getTime(void) {
    return sGlobalTimer;
}

s32 globalTimer_getTimeMasked(s32 mask) {
    return sGlobalTimer & mask;
}

void disableInput_set(void) {
}

enum map_e gsworld_getMap(void) {
    return BK_MAP_MUMBOS_MOUNTAIN;
}

void viewport_getPosition_vec3f(f32 position[3]) {
    ml_vec3f_copy(position, sViewportPosition);
}

void viewport_getRotation_vec3f(f32 rotation[3]) {
    ml_vec3f_copy(rotation, sViewportRotation);
}

f32 viewport_getYaw(void) {
    return sViewportRotation[1];
}

void viewport_getLookVector(f32 look[3]) {
    f32 pitch = sViewportRotation[0] * (BAD_PI / 180.0f);
    f32 yaw = sViewportRotation[1] * (BAD_PI / 180.0f);

    look[0] = -sinf(yaw) * cosf(pitch);
    look[1] = sinf(pitch);
    look[2] = -cosf(yaw) * cosf(pitch);
}

f32 viewport_adjustAngleToRight(f32 position[3], f32 angle) {
    return mlNormalizeAngle((sViewportRotation[1] + angle) + 90.0);
}

void viewport_adjustPointDistance(f32 position[3], f32 distance) {
    f32 toCamera[3];

    ml_vec3f_diff_copy(toCamera, sViewportPosition, position);
    ml_vec3f_set_length_copy(toCamera, toCamera, distance);
    position[0] += toCamera[0];
    position[1] += toCamera[1];
    position[2] += toCamera[2];
}

s32 getGameMode(void) {
    return GAME_MODE_3_NORMAL;
}

enum level_e level_get(void) {
    return LEVEL_1_MUMBOS_MOUNTAIN;
}

s32 viMgr_func_8024BD94(void) {
    return HOST_FRAMES_PER_UPDATE;
}

// Ground traction per surface group, from commonParticle.c's D_80371E30.
f32 func_8033DE30(s32 surfaceGroup) {
    static const f32 kTraction[] = { 0.0f, 0.07f, 0.29f, 0.15f, 0.05f };

    if (surfaceGroup < 0 || surfaceGroup >= (s32)(sizeof(kTraction) / sizeof(kTraction[0]))) {
        return 0.0f;
    }
    return kTraction[surfaceGroup];
}

bool vec4f_isZero(f32 color[4]) {
    return (color[0] == 0.0f) && (color[1] == 0.0f) && (color[2] == 0.0f) && (color[3] == 1.0f);
}

s32 fxairscore_count_to_time(s32 count) {
    return (count * 60 * 60) / 6;
}

s32 fxairscore_time_to_count(s32 time) {
    s32 minutes = (((6 * time) / 60) / 60) + ((time != 0) ? 1 : 0);

    return MIN(6, minutes);
}

Vtx* vtxList_getVertices(BKVertexList* list) {
    return list->vertices;
}

f32 vtxList_getGlobalNorm(BKVertexList* list) {
    return list->global_norm;
}

void vec4f_clone(f32 destination[4], f32 source[4]) {
    memcpy(destination, source, sizeof(f32) * 4);
}

void func_802BC434(f32 angles[3], f32 from[3], f32 to[3]) {
    ml_horizontal_and_vertical_angles(from[0], from[1], from[2], to[0], to[1], to[2], &angles[0], &angles[1]);
    angles[0] = mlNormalizeAngle(-angles[0]);
    angles[2] = 0.0f;
}

void ncba1p_getZoomedInRotation(f32 rotation[3]) {
    ml_vec3f_copy(rotation, sViewportRotation);
}

void core2_12F30_getPlayerTint(s32 tint[3]) {
    tint[0] = 255;
    tint[1] = 255;
    tint[2] = 255;
}

ActorMarker* func_8032FBE4(f32* position, MarkerDrawFunc draw, int isActor, enum asset_e modelId) {
    memset(&sPlayerMarker, 0, sizeof(sPlayerMarker));
    return &sPlayerMarker;
}

// The only marker libbk owns is the player's, so its position is the player's.
void func_80330FCC(ActorMarker* marker, s32 position[3]) {
    f32 playerPosition[3];

    playerPosition_get(playerPosition);
    position[0] = (s32)playerPosition[0];
    position[1] = (s32)playerPosition[1];
    position[2] = (s32)playerPosition[2];
}

void modelRender_func_8033A1FC(void) {
    memset(sSelectors, 0, sizeof(sSelectors));
}

void modelRender_setAppendageVisibility(s32 selector, s32 value) {
    if (selector >= 0 && selector < LIBBK_SELECTOR_COUNT) {
        sSelectors[selector] = (u8)value;
    }
}

static BKVertexList* GetCollisionVertices(void) {
    return (BKVertexList*)sVertexStore;
}

static BKCollisionList* GetCollisionList(void) {
    return (BKCollisionList*)sCollisionStore;
}

static BKCollisionTriangle* IntersectLine(f32 start[3], f32 end[3], f32 normal[3], u32 filter) {
    if (sCollisionTriangleCount == 0) {
        return NULL;
    }
    return collisionList_intersectLineGlobal(GetCollisionList(), GetCollisionVertices(), sCollisionCenter,
                                             sNoRotation, COLLISION_VERTEX_SCALE, start, end, normal, filter);
}

static BKCollisionTriangle* IntersectMovingSphere(f32 from[3], f32 to[3], f32 radius, f32 normal[3], s32 steps,
                                                  u32 filter) {
    if (sCollisionTriangleCount == 0) {
        return NULL;
    }
    return collisionList_intersectMovingSphereGlobal(GetCollisionList(), GetCollisionVertices(), sCollisionCenter,
                                                     sNoRotation, COLLISION_VERTEX_SCALE, from, to, radius, normal,
                                                     steps, filter);
}

static BKCollisionTriangle* IntersectSphere(f32 center[3], f32 radius, f32 normal[3], u32 filter) {
    if (sCollisionTriangleCount == 0) {
        return NULL;
    }
    return collisionList_intersectSphereGlobal(GetCollisionList(), GetCollisionVertices(), sCollisionCenter,
                                               sNoRotation, COLLISION_VERTEX_SCALE, center, radius, normal, filter);
}

// The host writes the footstep sound straight into the surface bits instead of BK's per-map tables.
s32 func_803246B4(enum map_e map, s32 flags) {
    return (flags >> 8) & 0x1F;
}

BKCollisionTriangle* mapModel_intersectLine(f32 start[3], f32 end[3], f32 normal[3], u32 filter) {
    return IntersectLine(start, end, normal, filter);
}

void mapModel_getBounds(s32 min[3], s32 max[3]) {
    for (s32 axis = 0; axis < 3; axis++) {
        min[axis] = -COLLISION_BOUNDS_EXTENT;
        max[axis] = COLLISION_BOUNDS_EXTENT;
    }
}

static s16 ToCollisionUnits(f32 world, f32 center) {
    f32 local = (world - center) / COLLISION_VERTEX_SCALE;

    if (local > 32767.0f) {
        return 32767;
    }
    if (local < -32767.0f) {
        return -32767;
    }
    return (s16)local;
}

void LibBk_SetCollision(const LibBkTriangle* triangles, int32_t count, const float center[3]) {
    BKVertexList* vertexList = GetCollisionVertices();
    BKCollisionList* collisionList = GetCollisionList();
    BKCollisionGeometry* cell = (BKCollisionGeometry*)collisionList->data;
    BKCollisionTriangle* collisionTriangles = (BKCollisionTriangle*)(cell + 1);

    count = MIN(count, LIBBK_TRIANGLE_MAX);
    memset(vertexList, 0, sizeof(BKVertexList));
    memset(collisionList, 0, sizeof(BKCollisionList));
    vertexList->count = (s16)(count * 3);
    vertexList->local_norm = 0x7FFF;
    vertexList->global_norm = 0x7FFF;
    collisionList->geo_count = 1;
    collisionList->tri_count = (s16)count;
    cell->start_tri_index = 0;
    cell->tri_count = (s16)count;
    for (s32 i = 0; i < count; i++) {
        collisionTriangles[i].unk6 = 0;
        collisionTriangles[i].flags = (s32)triangles[i].flags;
        for (s32 corner = 0; corner < 3; corner++) {
            Vtx* vertex = &vertexList->vertices[i * 3 + corner];

            memset(vertex, 0, sizeof(Vtx));
            for (s32 axis = 0; axis < 3; axis++) {
                vertex->v.ob[axis] = ToCollisionUnits(triangles[i].vertices[corner][axis], center[axis]);
            }
            collisionTriangles[i].unk0[corner] = (s16)(i * 3 + corner);
        }
    }
    ml_vec3f_copy(sCollisionCenter, (f32*)center);
    sCollisionTriangleCount = count;
}

void LibBk_Init(LibBkLoadAssetFunc loadAsset) {
    sLoadAsset = loadAsset;
    ml_init();
    rand_reset();
    timedFuncQueue_init();
    time_reset();
    assetCache_init();
    animCache_init();
    animcommoncache_init();
    func_80320B84();
    func_80320B44((void*)IntersectLine, (void*)IntersectMovingSphere, (void*)IntersectSphere, NULL);
    // Rebounds read the active entry even when the hit named none.
    barebound_set_active(BA_REBOUND_1_UNK);
    item_set(ITEM_15_HEALTH_TOTAL, 8);
    item_set(ITEM_14_HEALTH, 8);
    item_set(ITEM_17_AIR, 3600);
}

void LibBk_Spawn(const float position[3], float yaw) {
    bsmethods_reset();
    func_8028F85C((f32*)position);
    func_80295A8C();
    yaw_setIdeal(yaw);
    yaw_applyIdeal();
}

static void UpdateHeldFrames(u32 heldButtons) {
    static const struct {
        u32 libbkButton;
        enum button_e bkButton;
    } kButtons[] = {
        { LIBBK_BUTTON_A, BUTTON_A },           { LIBBK_BUTTON_B, BUTTON_B },
        { LIBBK_BUTTON_Z, BUTTON_Z },           { LIBBK_BUTTON_L, BUTTON_L },
        { LIBBK_BUTTON_R, BUTTON_R },           { LIBBK_BUTTON_C_UP, BUTTON_C_UP },
        { LIBBK_BUTTON_C_DOWN, BUTTON_C_DOWN }, { LIBBK_BUTTON_C_LEFT, BUTTON_C_LEFT },
        { LIBBK_BUTTON_C_RIGHT, BUTTON_C_RIGHT }, { LIBBK_BUTTON_START, BUTTON_START },
    };

    for (u32 i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); i++) {
        s32* frames = &sHeldFrames[kButtons[i].bkButton];

        *frames = (heldButtons & kButtons[i].libbkButton) ? *frames + 1 : 0;
    }
}

// Same dead zone and range as BK's controller_clampAndNormaliseJoyAxis.
static f32 NormaliseStickAxis(s32 value, s32 deadZone, s32 maximum) {
    s32 magnitude = (value < 0) ? -value : value;

    if (magnitude <= deadZone) {
        return 0.0f;
    }
    magnitude = MIN(magnitude, maximum);
    magnitude = ((magnitude - deadZone) * 80) / (maximum - deadZone);
    return ((value < 0) ? -magnitude : magnitude) / 80.0f;
}

void LibBk_Update(const LibBkInput* input) {
    UpdateHeldFrames(input->heldButtons);
    sStick[0] = NormaliseStickAxis(input->stickX, 7, 59);
    sStick[1] = NormaliseStickAxis(input->stickY, 7, 61);
    ml_vec3f_copy(sViewportPosition, (f32*)input->cameraPosition);
    ml_vec3f_copy(sViewportRotation, (f32*)input->cameraPitchYawRoll);
    time_setDeltaReal_frames(HOST_FRAMES_PER_UPDATE);
    sGlobalTimer++;
    func_80295C14();
}

// baModel_draw refreshes the model's yaw; libbk never draws through BK, so it reads the same rule here.
static f32 GetModelYaw(void) {
    switch (baModel_getDirection()) {
        case PLAYER_MODEL_DIR_KAZOOIE:
            return mlNormalizeAngle(yaw_get() + 180.0f);
        case PLAYER_MODEL_DIR_GLOBAL:
            return baModel_getYaw();
        default:
            return yaw_get();
    }
}

void LibBk_GetState(LibBkState* state) {
    AnimCtrl* animation = player_getAnimCtrlPtr();

    playerPosition_get(state->position);
    baphysics_get_velocity(state->velocity);
    state->yaw = yaw_get();
    state->modelYaw = GetModelYaw();
    state->modelOffset[0] = baModelDisplacement[0];
    state->modelOffset[1] = baModelDisplacement[1] + MODEL_LIFT;
    state->modelOffset[2] = baModelDisplacement[2];
    state->isModelVisible = baModel_isVisible() != 0;
    state->modelPitch = pitch_get();
    state->modelRoll = roll_get();
    state->action = bs_getState();
    state->modelId = baModel_getModelId();
    state->animationId = (animation != NULL) ? anctrl_getIndex(animation) : 0;
    state->animationTimer = (animation != NULL) ? anctrl_getAnimTimer(animation) : 0.0f;
    modelAppendages_loadAppendage();
    memcpy(state->selectors, sSelectors, sizeof(sSelectors));
    state->isInWater = player_inWater();
    state->isUnderwater = player_getWaterState() == BSWATERGROUP_2_UNDERWATER;
    state->isGrounded = player_isStable();
}

void LibBk_SetPosition(const float position[3]) {
    playerPosition_set((f32*)position);
}

void LibBk_SetYaw(float yaw) {
    yaw_setIdeal(yaw);
    yaw_applyIdeal();
}

void LibBk_SetAbilityLearned(LibBkAbility ability, bool isLearned) {
    ability_setLearned((enum ability_e)ability, isLearned);
}

void LibBk_SetStandingOnPad(LibBkPad pad) {
    if (pad == LIBBK_PAD_FLIGHT) {
        baflag_set(BA_FLAG_1_ON_FLIGHT_PAD);
    } else if (pad == LIBBK_PAD_SHOCK_SPRING) {
        baflag_set(BA_FLAG_2_ON_SPRING_PAD);
    }
}

int32_t LibBk_GetItemCount(LibBkItem item) {
    return item_getCount((enum item_e)item);
}

void LibBk_SetItemCount(LibBkItem item, int32_t count) {
    item_adjustByDiffWithoutHud((enum item_e)item, count - item_getCount((enum item_e)item));
}

// An enemy's hit in BK names how hard Banjo bounces off; this is its middle strength.
void LibBk_Hurt(const float sourcePosition[3]) {
    func_80296CB4(HURT_REBOUND);
    func_80296CC0((f32*)sourcePosition);
    bs_checkInterrupt(BS_INTR_20);
}
