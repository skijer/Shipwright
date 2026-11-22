#include "banjo_physics.h"

#include <math.h>

#include "banjo_controls.h"
#include "banjo_moves.h"
#include "banjo_rom.h"
#include "banjo_surfaces.h"
#include "banjo_world.h"
#include "bk_assets.h"

#define RADIANS_TO_DEGREES (180.0f / 3.14159265f)
#define STICK_LIMIT 80
#define MIN_LOOK_LENGTH 0.001f
#define FULL_AIR 3600
#define FULL_HEALTH 8
#define HURT_SOURCE_DISTANCE 100.0f

typedef struct ActionButton {
    BanjoAction action;
    uint32_t button;
} ActionButton;

static const ActionButton kActionButtons[] = {
    { BANJO_ACTION_JUMP, LIBBK_BUTTON_A },          { BANJO_ACTION_ATTACK, LIBBK_BUTTON_B },
    { BANJO_ACTION_CROUCH, LIBBK_BUTTON_Z },        { BANJO_ACTION_TARGET, LIBBK_BUTTON_R },
    { BANJO_ACTION_FIRST_PERSON, LIBBK_BUTTON_C_UP }, { BANJO_ACTION_EGGS, LIBBK_BUTTON_C_DOWN },
    { BANJO_ACTION_TALON_TROT, LIBBK_BUTTON_C_LEFT }, { BANJO_ACTION_WONDER_WING, LIBBK_BUTTON_C_RIGHT },
};

static LibBkTriangle sTriangles[LIBBK_TRIANGLE_MAX];
static LibBkState sState;
static bool sIsLibraryReady;
static int32_t sLastHealth;
static int32_t sLastHealthCapacity;

static void* LoadAsset(int32_t assetId, int32_t* size) {
    uint32_t loadedSize = 0;
    void* data = BkAssets_Load((uint16_t)assetId, &loadedSize);

    *size = (int32_t)loadedSize;
    return data;
}

static bool InitLibraryOnce(void) {
    if (sIsLibraryReady) {
        return true;
    }
    if (BanjoRom_Get() == NULL) {
        return false;
    }
    LibBk_Init(LoadAsset);
    sIsLibraryReady = true;
    return true;
}

static void ToBkUnits(Vec3f position, float out[3]) {
    out[0] = position.x * BANJO_BK_UNITS_PER_GAME_UNIT;
    out[1] = position.y * BANJO_BK_UNITS_PER_GAME_UNIT;
    out[2] = position.z * BANJO_BK_UNITS_PER_GAME_UNIT;
}

static int8_t ClampStick(int32_t value) {
    return (int8_t)CLAMP(value, -STICK_LIMIT, STICK_LIMIT);
}

// BK's camera looks along (-cos pitch sin yaw, sin pitch, -cos pitch cos yaw), in degrees.
static void ReadInput(PlayState* play, LibBkInput* input) {
    const Camera* camera = GET_ACTIVE_CAM(play);
    const Input* controller = &play->state.input[0];
    const Vec3f look = { camera->at.x - camera->eye.x, camera->at.y - camera->eye.y, camera->at.z - camera->eye.z };
    const float horizontal = fmaxf(sqrtf(look.x * look.x + look.z * look.z), MIN_LOOK_LENGTH);
    BanjoBindings bindings;

    BanjoControls_ReadBindings(&bindings);
    *input = (LibBkInput){ 0 };
    input->stickX = ClampStick(controller->rel.stick_x);
    input->stickY = ClampStick(controller->rel.stick_y);
    for (int32_t index = 0; index < ARRAY_COUNT(kActionButtons); index++) {
        if (controller->cur.button & bindings.buttons[kActionButtons[index].action]) {
            input->heldButtons |= kActionButtons[index].button;
        }
    }
    ToBkUnits(camera->eye, input->cameraPosition);
    input->cameraPitchYawRoll[0] = atan2f(look.y, horizontal) * RADIANS_TO_DEGREES;
    input->cameraPitchYawRoll[1] = atan2f(-look.x, -look.z) * RADIANS_TO_DEGREES;
}

static void RememberHealth(void) {
    sLastHealth = BANJO_PLAYER_HEALTH;
    sLastHealthCapacity = BANJO_PLAYER_HEALTH_CAPACITY;
}

// The game's damage lands on Link's hearts; Banjo answers with BK's own recoil away from where he faces. Health cut
// down to a new capacity is no hit.
static void RecoilFromLostHealth(void) {
    const bool isHit = BANJO_PLAYER_HEALTH < sLastHealth && BANJO_PLAYER_HEALTH_CAPACITY == sLastHealthCapacity;
    const float yaw = sState.modelYaw / RADIANS_TO_DEGREES;
    const float source[3] = { sState.position[0] + sinf(yaw) * HURT_SOURCE_DISTANCE, sState.position[1],
                              sState.position[2] + cosf(yaw) * HURT_SOURCE_DISTANCE };

    if (isHit) {
        LibBk_Hurt(source);
    }
    RememberHealth();
}

// Link carries no speed of his own: the rest of the player update would move him again after Banjo did.
static void MovePlayerToBanjo(Player* player) {
    player->actor.world.pos.x = sState.position[0] * BANJO_GAME_UNITS_PER_BK_UNIT;
    player->actor.world.pos.y = sState.position[1] * BANJO_GAME_UNITS_PER_BK_UNIT;
    player->actor.world.pos.z = sState.position[2] * BANJO_GAME_UNITS_PER_BK_UNIT;
    player->actor.shape.rot.y = (s16)(sState.modelYaw * BANJO_BINANG_PER_DEGREE);
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    BANJO_PLAYER_SPEED(player) = 0.0f;
}

bool BanjoPhysics_Spawn(Player* player) {
    float position[3];

    if (!InitLibraryOnce()) {
        return false;
    }
    ToBkUnits(player->actor.world.pos, position);
    LibBk_Spawn(position, player->actor.shape.rot.y / BANJO_BINANG_PER_DEGREE);
    LibBk_GetState(&sState);
    RememberHealth();
    return true;
}

void BanjoPhysics_Drive(PlayState* play, Player* player) {
    const int32_t count = BanjoSurfaces_Gather(play, player->actor.world.pos, sTriangles, LIBBK_TRIANGLE_MAX);
    LibBkInput input;
    float center[3];

    ToBkUnits(player->actor.world.pos, center);
    LibBk_SetCollision(sTriangles, count, center);
    ReadInput(play, &input);
    BanjoMoves_ApplyLearned();
    LibBk_SetItemCount(LIBBK_ITEM_AIR, FULL_AIR);
    LibBk_SetItemCount(LIBBK_ITEM_HEALTH, FULL_HEALTH);
    RecoilFromLostHealth();
    LibBk_Update(&input);
    LibBk_GetState(&sState);
    MovePlayerToBanjo(player);
}

void BanjoPhysics_Park(Player* player) {
    float position[3];

    ToBkUnits(player->actor.world.pos, position);
    LibBk_SetPosition(position);
    LibBk_SetYaw(player->actor.shape.rot.y / BANJO_BINANG_PER_DEGREE);
    LibBk_GetState(&sState);
    RememberHealth();
}

const LibBkState* BanjoPhysics_GetState(void) {
    return &sState;
}
