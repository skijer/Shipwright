#include "mario_body.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "libsm64/libsm64.h"
#include "mario_audio.h"
#include "mario_buttons.h"
#include "mario_config.h"
#include "mario_controls.h"
#include "mario_poses.h"
#include "mario_powers.h"
#include "mario_render.h"
#include "mario_rom.h"
#include "mario_surfaces.h"
#include "mario_world.h"

#define NO_MARIO (-1)
#define NO_SCENE (-1)
#define GAME_VIS_PER_SECOND 60.0f
#define MAX_TICKS_PER_FRAME 3
#define STICK_RANGE 64.0f
#define MIN_LOOK_LENGTH 0.001f
#define REBUILD_WHILE_STILL_FRAMES 300
#define SURFACE_JUMP_DEPTH 20.0f
#define HEALTH_SEGMENTS 8
#define HEALTH_PER_SEGMENT 0x100
#define HEALTH_SEGMENT_HALF 0x80
#define HIT_SOURCE_DISTANCE 100.0f
#define MARIO_HEIGHT 40.0f
#define FOOT_SPREAD 5.0f
#define RADIANS_PER_BINANG (3.14159265f / 32768.0f)

// Share of Mario's height at each of Link's body parts, in PlayerBodyPart order (the same in both games).
static const float kBodyPartHeights[] = {
    0.5f, 0.4f, 0.2f, 0.0f, 0.4f, 0.2f, 0.0f, 0.85f, 0.95f, 0.75f, 0.7f, 0.6f, 0.5f, 0.7f, 0.6f, 0.5f, 0.5f, 0.65f,
};
_Static_assert(ARRAY_COUNT(kBodyPartHeights) == PLAYER_BODYPART_MAX, "one height per body part");

typedef struct MarioBody {
    int32_t id;
    int16_t sceneId;
    float pendingTicks;
    int32_t framesSinceRebuild;
    uint32_t surfaceSignature;
    bool isFloorOnly;
    bool wasParked;
    int32_t lastHealth;
    int32_t lastHealthCapacity;
    struct SM64MarioState state;
    struct SM64MarioGeometryBuffers geometry;
} MarioBody;

static float sPositions[SM64_GEO_MAX_TRIANGLES * 9];
static float sNormals[SM64_GEO_MAX_TRIANGLES * 9];
static float sColors[SM64_GEO_MAX_TRIANGLES * 9];
static float sUvs[SM64_GEO_MAX_TRIANGLES * 6];
static const SOHModApi* sApi;
static uint8_t* sAtlas;
static MarioBody sBody = { NO_MARIO, NO_SCENE };

static bool IsLibraryReady(void) {
    return sAtlas != NULL;
}

static bool InitLibraryOnce(void) {
    const uint8_t* rom = MarioRom_Get();

    if (IsLibraryReady()) {
        return true;
    }
    if (rom == NULL) {
        return false;
    }
    sAtlas = malloc(4 * SM64_TEXTURE_WIDTH * SM64_TEXTURE_HEIGHT);
    if (sAtlas == NULL) {
        return false;
    }
    sm64_global_init(rom, sAtlas);
    sm64_audio_init(rom);
    MarioRender_SetTextureAtlas(sAtlas);
    sBody.geometry.position = sPositions;
    sBody.geometry.normal = sNormals;
    sBody.geometry.color = sColors;
    sBody.geometry.uv = sUvs;
    return true;
}

static Vec3f ToGameUnits(const float* marioPosition) {
    Vec3f position = { marioPosition[0] / MARIO_WORLD_SCALE, marioPosition[1] / MARIO_WORLD_SCALE,
                       marioPosition[2] / MARIO_WORLD_SCALE };

    return position;
}

static void TickMario(const struct SM64MarioInputs* inputs) {
    sBody.geometry.numTrianglesUsed = 0;
    sm64_mario_tick(sBody.id, inputs, &sBody.state, &sBody.geometry);
}

// A zero camera vector would divide by zero inside libsm64.
static void TickWithoutInput(void) {
    struct SM64MarioInputs inputs = { 0 };

    inputs.camLookZ = 1.0f;
    TickMario(&inputs);
}

static void DeleteMario(void) {
    if (sBody.id != NO_MARIO) {
        sm64_mario_delete(sBody.id);
    }
    sBody.id = NO_MARIO;
    sBody.sceneId = NO_SCENE;
    sBody.geometry.numTrianglesUsed = 0;
    sBody.wasParked = false;
    MarioSurfaces_ClearActorColliders();
    MarioPoses_Reset();
}

// Mario keeps SM64's eight segments; they show Link's share of his hearts, so a hit costs as much as it costs Link.
static int32_t GetHealthSegments(int32_t health) {
    const int32_t capacity = MAX(MARIO_PLAYER_HEALTH_CAPACITY, 1);

    return CLAMP((health * HEALTH_SEGMENTS + capacity - 1) / capacity, 0, HEALTH_SEGMENTS);
}

static void RememberHealth(void) {
    sBody.lastHealth = MARIO_PLAYER_HEALTH;
    sBody.lastHealthCapacity = MARIO_PLAYER_HEALTH_CAPACITY;
}

static void ShowHealth(void) {
    sm64_set_mario_health(sBody.id,
                          (uint16_t)(GetHealthSegments(MARIO_PLAYER_HEALTH) * HEALTH_PER_SEGMENT + HEALTH_SEGMENT_HALF));
}

// Health cut down to a new capacity is no hit.
static void FollowLinkHealth(void) {
    const int32_t health = MARIO_PLAYER_HEALTH;
    const bool isHit = health < sBody.lastHealth && MARIO_PLAYER_HEALTH_CAPACITY == sBody.lastHealthCapacity;

    if (isHit) {
        const int32_t lostSegments = MAX(GetHealthSegments(sBody.lastHealth) - GetHealthSegments(health), 1);
        const float facing = sBody.state.faceAngle;

        sm64_mario_take_damage(sBody.id, (uint32_t)lostSegments, 0,
                               sBody.state.position[0] + sinf(facing) * HIT_SOURCE_DISTANCE, sBody.state.position[1],
                               sBody.state.position[2] + cosf(facing) * HIT_SOURCE_DISTANCE);
    }
    ShowHealth();
    RememberHealth();
}

static bool CreateMario(PlayState* play, Player* player) {
    const Vec3f* position = &player->actor.world.pos;

    if (!InitLibraryOnce() || MarioSurfaces_Load(play, false) == 0) {
        return false;
    }
    sBody.id = sm64_mario_create(position->x * MARIO_WORLD_SCALE, position->y * MARIO_WORLD_SCALE,
                                 position->z * MARIO_WORLD_SCALE);
    if (sBody.id == NO_MARIO) {
        return false;
    }
    sBody.sceneId = MARIO_SCENE_ID(play);
    sBody.pendingTicks = 0.0f;
    sBody.framesSinceRebuild = 0;
    sBody.surfaceSignature = MarioSurfaces_GetLiveSignature(play);
    sBody.isFloorOnly = false;
    sm64_set_mario_faceangle(sBody.id, player->actor.shape.rot.y / MARIO_BINANG_PER_RADIAN);
    ShowHealth();
    RememberHealth();
    TickWithoutInput();
    return true;
}

// A new scene throws away Mario with the old scene's collision; he comes back once the new one is loaded.
static bool EnsureMarioInScene(PlayState* play, Player* player) {
    if (sBody.id != NO_MARIO && sBody.sceneId == MARIO_SCENE_ID(play)) {
        return true;
    }
    DeleteMario();
    return CreateMario(play, player);
}

// The vanish cap walks through walls, so its collision is floors only.
static void RefreshSurfaces(PlayState* play) {
    const bool isFloorOnly = (sBody.state.flags & SM64_MARIO_VANISH_CAP) != 0;
    const int32_t movingInterval = MarioConfig_GetCollisionUpdateFrames();

    sBody.framesSinceRebuild++;
    if (isFloorOnly == sBody.isFloorOnly) {
        const uint32_t signature = MarioSurfaces_GetLiveSignature(play);
        const bool hasMoved = signature != sBody.surfaceSignature;
        const int32_t interval = hasMoved ? movingInterval : REBUILD_WHILE_STILL_FRAMES;

        if (movingInterval == 0 || sBody.framesSinceRebuild < interval) {
            return;
        }
        sBody.surfaceSignature = signature;
    }
    if (MarioSurfaces_Load(play, isFloorOnly) > 0) {
        sBody.isFloorOnly = isFloorOnly;
        sBody.framesSinceRebuild = 0;
    }
}

static void ReadInputs(PlayState* play, struct SM64MarioInputs* inputs) {
    const Camera* camera = GET_ACTIVE_CAM(play);
    const Input* input = &play->state.input[0];
    const float lookX = camera->at.x - camera->eye.x;
    const float lookZ = camera->at.z - camera->eye.z;
    const float lookLength = fmaxf(sqrtf(lookX * lookX + lookZ * lookZ), MIN_LOOK_LENGTH);
    MarioBindings bindings;

    MarioControls_ReadBindings(&bindings);
    *inputs = (struct SM64MarioInputs){ 0 };
    inputs->camLookX = lookX / lookLength;
    inputs->camLookZ = lookZ / lookLength;
    inputs->stickX = input->rel.stick_x / STICK_RANGE;
    inputs->stickY = -input->rel.stick_y / STICK_RANGE;
    inputs->buttonA = (input->cur.button & bindings.buttons[MARIO_ACTION_JUMP]) != 0;
    inputs->buttonB = MarioPowers_CanPunch() && (input->cur.button & bindings.buttons[MARIO_ACTION_PUNCH]) != 0;
    inputs->buttonZ = MarioPowers_CanCrouch() && (input->cur.button & bindings.buttons[MARIO_ACTION_CROUCH]) != 0;
}

// SM64 moves Mario a fixed distance per tick, so the ticks follow real time, not the game's frame rate.
static void TickAtConfiguredRate(const struct SM64MarioInputs* inputs) {
    const int32_t updateRate = (R_UPDATE_RATE > 0) ? R_UPDATE_RATE : 3;
    int32_t ticks = 0;

    sBody.pendingTicks += updateRate / GAME_VIS_PER_SECOND * MarioConfig_GetTickRate();
    while (sBody.pendingTicks >= 1.0f && ticks < MAX_TICKS_PER_FRAME) {
        TickMario(inputs);
        sBody.pendingTicks -= 1.0f;
        ticks++;
    }
    if (ticks == MAX_TICKS_PER_FRAME) {
        sBody.pendingTicks = 0.0f;
    }
}

// libsm64 can miss the jump out of the water at the surface of a game waterbox.
static void JumpOutOfWaterSurface(PlayState* play, Player* player) {
    const Input* input = &play->state.input[0];
    MarioBindings bindings;

    MarioControls_ReadBindings(&bindings);
    if (!(input->press.button & bindings.buttons[MARIO_ACTION_JUMP]) ||
        !(sBody.state.action & SM64_ACT_FLAG_SWIMMING)) {
        return;
    }

    const float water = MarioSurfaces_GetWaterLevel(play, player->actor.world.pos.x, player->actor.world.pos.z);

    if (water > MARIO_NO_WATER && player->actor.world.pos.y >= water - SURFACE_JUMP_DEPTH) {
        sm64_set_mario_action(sBody.id, SM64_ACT_WATER_JUMP);
    }
}

// Link carries no speed of his own: the rest of the player update would move him again after Mario did.
static void MovePlayerToMario(Player* player) {
    player->actor.world.pos = ToGameUnits(sBody.state.position);
    player->actor.shape.rot.y = (s16)(sBody.state.faceAngle * MARIO_BINANG_PER_RADIAN);
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    MARIO_PLAYER_SPEED(player) = 0.0f;
}

static void DriveMario(PlayState* play, Player* player) {
    struct SM64MarioInputs inputs;
    const float water = MarioSurfaces_GetWaterLevel(play, player->actor.world.pos.x, player->actor.world.pos.z);

    if (sBody.wasParked) {
        MarioPoses_Reset();
        sBody.wasParked = false;
    }
    RefreshSurfaces(play);
    ReadInputs(play, &inputs);
    FollowLinkHealth();
    sm64_set_mario_water_level(sBody.id, (int32_t)(water * MARIO_WORLD_SCALE));
    TickAtConfiguredRate(&inputs);
    MovePlayerToMario(player);
    JumpOutOfWaterSurface(play, player);
}

// While the game drives Link, Mario stands where Link is, posed as Link is; the puppet tick never reads collision.
static void ParkOnPlayer(Player* player) {
    const Vec3f* position = &player->actor.world.pos;

    sm64_set_mario_position(sBody.id, position->x * MARIO_WORLD_SCALE, position->y * MARIO_WORLD_SCALE,
                            position->z * MARIO_WORLD_SCALE);
    sm64_set_mario_faceangle(sBody.id, player->actor.shape.rot.y / MARIO_BINANG_PER_RADIAN);
    sBody.pendingTicks = 0.0f;
    MarioPoses_Follow(player, sBody.id, &sBody.geometry);
    sBody.state.position[0] = position->x * MARIO_WORLD_SCALE;
    sBody.state.position[1] = position->y * MARIO_WORLD_SCALE;
    sBody.state.position[2] = position->z * MARIO_WORLD_SCALE;
    sBody.wasParked = true;
    RememberHealth();
}

void MarioBody_PutOnCap(uint32_t capFlag, uint16_t ticks) {
    if (sBody.id != NO_MARIO) {
        sm64_mario_interact_cap(sBody.id, capFlag, ticks, true);
    }
}

void MarioBody_Register(const SOHModApi* api) {
    sApi = api;
    MarioAudio_Register(api);
}

void MarioBody_Enter(PlayState* play, Player* player) {
    DeleteMario();
    if (play != NULL) {
        CreateMario(play, player);
    }
}

void MarioBody_Exit(PlayState* play, Player* player) {
    DeleteMario();
}

void MarioBody_Update(PlayState* play, Player* player) {
    if (!EnsureMarioInScene(play, player)) {
        return;
    }
    if (sApi->IsFormSuspended()) {
        ParkOnPlayer(player);
    } else if (MarioButtons_Update(play)) {
        DriveMario(play, player);
    }
    MarioAudio_Refill();
}

// Player_Draw sets these from Link's limbs and the takeover skips it, so Navi, the feet shadow and targeting
// would stay where Link transformed.
static void PlaceLinkBodyParts(Player* player) {
    const Vec3f base = player->actor.world.pos;
    const float yaw = player->actor.shape.rot.y * RADIANS_PER_BINANG;
    const float sideX = cosf(yaw) * FOOT_SPREAD;
    const float sideZ = -sinf(yaw) * FOOT_SPREAD;

    for (int32_t part = 0; part < PLAYER_BODYPART_MAX; part++) {
        player->bodyPartsPos[part] = (Vec3f){ base.x, base.y + MARIO_HEIGHT * kBodyPartHeights[part], base.z };
    }
    player->actor.shape.feetPos[FOOT_LEFT] = (Vec3f){ base.x + sideX, base.y, base.z + sideZ };
    player->actor.shape.feetPos[FOOT_RIGHT] = (Vec3f){ base.x - sideX, base.y, base.z - sideZ };
    player->actor.focus.pos = player->bodyPartsPos[PLAYER_BODYPART_HEAD];
}

// The mesh follows Link: when the tick was skipped it still sits where Mario last stood.
void MarioBody_Draw(PlayState* play, Player* player) {
    const Vec3f marioPosition = ToGameUnits(sBody.state.position);
    Vec3f offset;

    PlaceLinkBodyParts(player);
    if (sBody.id == NO_MARIO) {
        return;
    }
    offset.x = player->actor.world.pos.x - marioPosition.x;
    offset.y = player->actor.world.pos.y - marioPosition.y;
    offset.z = player->actor.world.pos.z - marioPosition.z;
    MarioRender_DrawMesh(play, &sBody.geometry, offset, sBody.state.flags);
    MarioSurfaces_CaptureActorColliders(play);
}
