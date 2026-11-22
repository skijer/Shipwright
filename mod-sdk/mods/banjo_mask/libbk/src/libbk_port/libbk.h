#ifndef LIBBK_H
#define LIBBK_H

#include <stdbool.h>
#include <stdint.h>

#define LIBBK_SELECTOR_COUNT 64
#define LIBBK_TRIANGLE_MAX 4096

// Collision flags, in Banjo-Kazooie's own bits: solid ground carries its footstep sound in bits 8-12.
#define LIBBK_SURFACE_FOOTSTEP(footstep) ((uint32_t)(footstep) << 8)
#define LIBBK_SURFACE_SOLID LIBBK_SURFACE_FOOTSTEP(LIBBK_FOOTSTEP_DIRT)
#define LIBBK_SURFACE_WATER 0x00020000

// Rows of samplerate.c's footstep tables.
typedef enum LibBkFootstep {
    LIBBK_FOOTSTEP_DIRT = 0x1,
    LIBBK_FOOTSTEP_SOFT = 0x2,
    LIBBK_FOOTSTEP_STONE = 0x3,
    LIBBK_FOOTSTEP_SHALLOW_WATER = 0x4,
    LIBBK_FOOTSTEP_LEAVES = 0x7,
    LIBBK_FOOTSTEP_SNOW = 0x8,
    LIBBK_FOOTSTEP_WOOD = 0x9,
    LIBBK_FOOTSTEP_METAL = 0xA,
    LIBBK_FOOTSTEP_DEEP_WATER = 0xB,
} LibBkFootstep;

typedef enum LibBkButton {
    LIBBK_BUTTON_A = 1 << 0,
    LIBBK_BUTTON_B = 1 << 1,
    LIBBK_BUTTON_Z = 1 << 2,
    LIBBK_BUTTON_L = 1 << 3,
    LIBBK_BUTTON_R = 1 << 4,
    LIBBK_BUTTON_C_UP = 1 << 5,
    LIBBK_BUTTON_C_DOWN = 1 << 6,
    LIBBK_BUTTON_C_LEFT = 1 << 7,
    LIBBK_BUTTON_C_RIGHT = 1 << 8,
    LIBBK_BUTTON_START = 1 << 9,
} LibBkButton;

typedef enum LibBkItem {
    LIBBK_ITEM_EGGS = 0xD,
    LIBBK_ITEM_RED_FEATHERS = 0xF,
    LIBBK_ITEM_GOLD_FEATHERS = 0x10,
    LIBBK_ITEM_HEALTH = 0x14,
    LIBBK_ITEM_HEALTH_TOTAL = 0x15,
    LIBBK_ITEM_AIR = 0x17,
} LibBkItem;

typedef enum LibBkPad {
    LIBBK_PAD_NONE,
    LIBBK_PAD_FLIGHT,
    LIBBK_PAD_SHOCK_SPRING,
} LibBkPad;

typedef enum LibBkAbility {
    LIBBK_ABILITY_BEAK_BARGE = 0x00,
    LIBBK_ABILITY_BEAK_BOMB = 0x01,
    LIBBK_ABILITY_BEAK_BUSTER = 0x02,
    LIBBK_ABILITY_CAMERA_CONTROL = 0x03,
    LIBBK_ABILITY_CLAW_SWIPE = 0x04,
    LIBBK_ABILITY_CLIMB = 0x05,
    LIBBK_ABILITY_EGGS = 0x06,
    LIBBK_ABILITY_FEATHERY_FLAP = 0x07,
    LIBBK_ABILITY_FLAP_FLIP = 0x08,
    LIBBK_ABILITY_FLIGHT = 0x09,
    LIBBK_ABILITY_JUMP_HIGHER = 0x0A,
    LIBBK_ABILITY_RAT_A_TAT_RAP = 0x0B,
    LIBBK_ABILITY_ROLL = 0x0C,
    LIBBK_ABILITY_SHOCK_SPRING_JUMP = 0x0D,
    LIBBK_ABILITY_STILT_STRIDE = 0x0E,
    LIBBK_ABILITY_DIVE = 0x0F,
    LIBBK_ABILITY_TALON_TROT = 0x10,
    LIBBK_ABILITY_TURBO_TALON_TROT = 0x11,
    LIBBK_ABILITY_WONDERWING = 0x12,
} LibBkAbility;

typedef struct LibBkInput {
    int8_t stickX;
    int8_t stickY;
    uint32_t heldButtons;
    float cameraPosition[3];
    float cameraPitchYawRoll[3];
} LibBkInput;

typedef struct LibBkSound {
    int32_t sfxId;
    float pitch;
    float volume;
} LibBkSound;

typedef struct LibBkTriangle {
    float vertices[3][3];
    uint32_t flags;
} LibBkTriangle;

typedef struct LibBkState {
    float position[3];
    float velocity[3];
    float yaw;
    float modelYaw;
    float modelPitch;
    float modelRoll;
    float modelOffset[3];
    bool isModelVisible;
    int32_t action;
    int32_t modelId;
    int32_t animationId;
    float animationTimer;
    uint8_t selectors[LIBBK_SELECTOR_COUNT];
    bool isInWater;
    bool isUnderwater;
    bool isGrounded;
} LibBkState;

// Returns the inflated, big-endian asset, allocated with malloc; libbk keeps it for the whole session.
typedef void* (*LibBkLoadAssetFunc)(int32_t assetId, int32_t* size);

void LibBk_Init(LibBkLoadAssetFunc loadAsset);
void LibBk_Spawn(const float position[3], float yaw);
void LibBk_SetCollision(const LibBkTriangle* triangles, int32_t count, const float center[3]);
void LibBk_Update(const LibBkInput* input);
void LibBk_GetState(LibBkState* state);
void LibBk_SetPosition(const float position[3]);
void LibBk_SetYaw(float yaw);
void LibBk_SetAbilityLearned(LibBkAbility ability, bool isLearned);
// Banjo counts as standing on that pad during the next update, the way BK's pad objects mark him.
void LibBk_SetStandingOnPad(LibBkPad pad);
int32_t LibBk_GetItemCount(LibBkItem item);
void LibBk_SetItemCount(LibBkItem item, int32_t count);
void LibBk_Hurt(const float sourcePosition[3]);
// The sounds BK started since the last call, at most `capacity`; the rest are dropped.
int32_t LibBk_TakeSounds(LibBkSound* sounds, int32_t capacity);

#endif
