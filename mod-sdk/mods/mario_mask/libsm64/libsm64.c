#include "sm64_build.h"
#include "libsm64.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "decomp/audio/external.h"
#include "decomp/include/PR/os_cont.h"
#include "decomp/engine/math_util.h"
#include "decomp/include/sm64.h"
#include "decomp/include/mario_geo_switch_case_ids.h" // MARIO_HAS_*_CAP_* (puppet cap state)
#include "decomp/include/seq_ids.h"
#include "decomp/include/mario_animation_ids.h"
#include "decomp/include/types.h"
#include "decomp/shim.h"
#include "decomp/memory.h"
#include "decomp/global_state.h"
#include "decomp/game/mario.h"
#include "decomp/game/object_stuff.h"
#include "decomp/engine/surface_collision.h"
#include "decomp/engine/graph_node.h"
#include "decomp/engine/geo_layout.h"
#include "decomp/game/rendering_graph_node.h"
#include "decomp/mario/geo.inc.h"
#include "decomp/game/mario_misc.h"
#include "decomp/game/platform_displacement.h"
#include "decomp/game/sound_init.h"

#include "debug_print.h"
#include "load_surfaces.h"
#include "gfx_adapter.h"
#include "load_anim_data.h"
#include "load_audio_data.h"
#include "load_tex_data.h"
#include "obj_pool.h"
#include "fake_interaction.h"
#include "load_model_data.h"

static struct AllocOnlyPool* s_mario_geo_pool = NULL;
static struct GraphNode* s_mario_graph_node = NULL;

static bool s_init_global = false;
static bool s_init_one_mario = false;

struct MarioInstance {
    struct GlobalState* globalState;
    float handTransform[4][4];
};
struct ObjPool s_mario_instance_pool = { 0, 0 };

static struct MarioInstance* find_instance(int32_t marioId) {
    if (marioId < 0 || marioId >= s_mario_instance_pool.size)
        return NULL;
    return s_mario_instance_pool.objects[marioId];
}

static void render_mario(struct MarioInstance* instance, struct SM64MarioGeometryBuffers* outBuffers) {
    gfx_adapter_bind_output_buffers(outBuffers);
    geo_process_root_hack_single_node(s_mario_graph_node);
    memcpy(instance->handTransform, gMarioHandTransform, sizeof(instance->handTransform));
}

static void update_button(bool on, u16 button) {
    gController.buttonPressed &= ~button;

    if (on) {
        if ((gController.buttonDown & button) == 0)
            gController.buttonPressed |= button;

        gController.buttonDown |= button;
    } else {
        gController.buttonDown &= ~button;
    }
}

static struct Area* allocate_area(void) {
    struct Area* result = malloc(sizeof(struct Area));
    memset(result, 0, sizeof(struct Area));

    result->flags = 1;
    result->camera = malloc(sizeof(struct Camera));
    memset(result->camera, 0, sizeof(struct Camera));

    return result;
}

static void free_area(struct Area* area) {
    free(area->camera);
    free(area);
}

typedef void (*SM64DebugPrintFunctionPtr)(const char*);
SM64_LIB_FN void sm64_register_debug_print_function(SM64DebugPrintFunctionPtr debugPrintFunction) {
    g_debug_print_func = debugPrintFunction;
}

typedef void (*SM64PlaySoundFunctionPtr)(uint32_t soundBits, float* pos);
SM64_LIB_FN void sm64_register_play_sound_function(SM64PlaySoundFunctionPtr playSoundFunction) {
    g_play_sound_func = playSoundFunction;
}

SM64_LIB_FN void sm64_global_init(const uint8_t* rom, uint8_t* outTexture) {
    if (s_init_global)
        sm64_global_terminate();

    s_init_global = true;

    load_mario_textures_from_rom(rom, outTexture);
    load_mario_model_from_rom(rom);
    load_mario_anims_from_rom(rom);

    memory_init();
}

SM64_LIB_FN void sm64_global_terminate(void) {
    if (!s_init_global)
        return;

    global_state_bind(NULL);

    if (s_init_one_mario) {
        for (int i = 0; i < s_mario_instance_pool.size; ++i)
            if (s_mario_instance_pool.objects[i] != NULL)
                sm64_mario_delete(i);

        obj_pool_free_all(&s_mario_instance_pool);
    }

    s_init_global = false;
    s_init_one_mario = false;

    if (s_mario_geo_pool) {
        alloc_only_pool_free(s_mario_geo_pool);
        s_mario_geo_pool = NULL;
    }

    unload_mario_model();
    surfaces_unload_all();
    unload_mario_anims();
    memory_terminate();
}

SM64_LIB_FN void sm64_audio_init(const uint8_t* rom) {
    load_audio_banks(rom);
}

#define SAMPLES_HIGH 544
#define SAMPLES_LOW 528

SM64_LIB_FN uint32_t sm64_audio_tick(uint32_t numQueuedSamples, uint32_t numDesiredSamples, int16_t* audio_buffer) {
    if (!g_is_audio_initialized) {
        DEBUG_PRINT("Attempted to tick audio, but sm64_audio_init() has not been called yet.");
        return 0;
    }

    update_game_sound();

    u32 num_audio_samples = numQueuedSamples < numDesiredSamples ? SAMPLES_HIGH : SAMPLES_LOW;
    for (int i = 0; i < 2; i++) {
        create_next_audio_buffer(audio_buffer + i * (2 * num_audio_samples), num_audio_samples);
    }

    return num_audio_samples;
}

SM64_LIB_FN void sm64_static_surfaces_load(const struct SM64Surface* surfaceArray, uint32_t numSurfaces) {
    surfaces_load_static(surfaceArray, numSurfaces);
}

SM64_LIB_FN int32_t sm64_mario_create(float x, float y, float z) {
    int32_t marioIndex = obj_pool_alloc_index(&s_mario_instance_pool, sizeof(struct MarioInstance));
    struct MarioInstance* newInstance = s_mario_instance_pool.objects[marioIndex];

    newInstance->globalState = global_state_create();
    global_state_bind(newInstance->globalState);

    if (!s_init_one_mario) {
        s_init_one_mario = true;
        s_mario_geo_pool = alloc_only_pool_init();
        s_mario_graph_node = process_geo_layout(s_mario_geo_pool, mario_geo_ptr);
    }

    gCurrSaveFileNum = 1;
    gMarioObject = hack_allocate_mario();
    gCurrentArea = allocate_area();
    gCurrentObject = gMarioObject;

    gMarioSpawnInfoVal.startPos[0] = x;
    gMarioSpawnInfoVal.startPos[1] = y;
    gMarioSpawnInfoVal.startPos[2] = z;

    gMarioSpawnInfoVal.startAngle[0] = 0;
    gMarioSpawnInfoVal.startAngle[1] = 0;
    gMarioSpawnInfoVal.startAngle[2] = 0;

    gMarioSpawnInfoVal.areaIndex = 0;
    gMarioSpawnInfoVal.activeAreaIndex = 0;
    gMarioSpawnInfoVal.behaviorArg = 0;
    gMarioSpawnInfoVal.behaviorScript = NULL;
    gMarioSpawnInfoVal.unk18 = NULL;
    gMarioSpawnInfoVal.next = NULL;

    init_mario_from_save_file();

    if (init_mario() < 0) {
        sm64_mario_delete(marioIndex);
        return -1;
    }

    set_mario_action(gMarioState, ACT_SPAWN_SPIN_AIRBORNE, 0);
    find_floor(x, y, z, &gMarioState->floor);

    return marioIndex;
}

// Render-only "puppet" create for network multiplayer (SOH/Harpoon). Identical to
// sm64_mario_create EXCEPT it tolerates a missing floor: init_mario() returns -1
// when gMarioState->floor == NULL (no static surfaces loaded into THIS instance's
// globalState — surfaces are per-globalState, so a render puppet never has them),
// and the normal create treats that as fatal. A puppet never runs physics
// (sm64_mario_tick_puppet skips bhv_mario_update), so a NULL floor is harmless:
// init_mario still sets pos/angle/action/bodystate/gfx, leaving a fully
// skinnable instance. Returns the instance id (never fails on floor alone).
SM64_LIB_FN int32_t sm64_mario_create_puppet(float x, float y, float z) {
    int32_t marioIndex = obj_pool_alloc_index(&s_mario_instance_pool, sizeof(struct MarioInstance));
    struct MarioInstance* newInstance = s_mario_instance_pool.objects[marioIndex];

    newInstance->globalState = global_state_create();
    global_state_bind(newInstance->globalState);

    if (!s_init_one_mario) {
        s_init_one_mario = true;
        s_mario_geo_pool = alloc_only_pool_init();
        s_mario_graph_node = process_geo_layout(s_mario_geo_pool, mario_geo_ptr);
    }

    gCurrSaveFileNum = 1;
    gMarioObject = hack_allocate_mario();
    gCurrentArea = allocate_area();
    gCurrentObject = gMarioObject;

    gMarioSpawnInfoVal.startPos[0] = x;
    gMarioSpawnInfoVal.startPos[1] = y;
    gMarioSpawnInfoVal.startPos[2] = z;

    gMarioSpawnInfoVal.startAngle[0] = 0;
    gMarioSpawnInfoVal.startAngle[1] = 0;
    gMarioSpawnInfoVal.startAngle[2] = 0;

    gMarioSpawnInfoVal.areaIndex = 0;
    gMarioSpawnInfoVal.activeAreaIndex = 0;
    gMarioSpawnInfoVal.behaviorArg = 0;
    gMarioSpawnInfoVal.behaviorScript = NULL;
    gMarioSpawnInfoVal.unk18 = NULL;
    gMarioSpawnInfoVal.next = NULL;

    init_mario_from_save_file();
    init_mario(); // ignore the floor-only -1; a render puppet doesn't need collision

    set_mario_action(gMarioState, ACT_IDLE, 0);
    // A puppet never runs its action, which is what would pick an animation; skinning with none reads a NULL
    // animation, so it idles until the host sets one.
    set_mario_animation(gMarioState, MARIO_ANIM_IDLE_HEAD_LEFT);

    return marioIndex;
}

SM64_LIB_FN void sm64_mario_tick(int32_t marioId, const struct SM64MarioInputs* inputs, struct SM64MarioState* outState,
                                 struct SM64MarioGeometryBuffers* outBuffers) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to tick non-existant Mario with ID: %u", marioId);
        return;
    }

    struct MarioInstance* instance = s_mario_instance_pool.objects[marioId];
    global_state_bind(instance->globalState);

    update_button(inputs->buttonA, A_BUTTON);
    update_button(inputs->buttonB, B_BUTTON);
    update_button(inputs->buttonZ, Z_TRIG);

    gMarioState->marioObj->header.gfx.cameraToObject[0] = 0;
    gMarioState->marioObj->header.gfx.cameraToObject[1] = 0;
    gMarioState->marioObj->header.gfx.cameraToObject[2] = 0;

    gMarioState->area->camera->yaw = atan2s(inputs->camLookZ, inputs->camLookX);

    gController.stickX = -64.0f * inputs->stickX;
    gController.stickY = 64.0f * inputs->stickY;
    gController.stickMag = sqrtf(gController.stickX * gController.stickX + gController.stickY * gController.stickY);

    apply_mario_platform_displacement();
    bhv_mario_update();
    update_mario_platform(); // TODO platform grabbed here and used next tick could be a use-after-free

    render_mario(instance, outBuffers);

    gAreaUpdateCounter++;

    outState->health = gMarioState->health;
    vec3f_copy(outState->position, gMarioState->pos);
    vec3f_copy(outState->velocity, gMarioState->vel);
    outState->faceAngle = (float)gMarioState->faceAngle[1] / 32768.0f * 3.14159f;
    outState->forwardVelocity = gMarioState->forwardVel;
    outState->action = gMarioState->action;

    struct AnimInfo animInfo = gMarioState->marioObj->header.gfx.animInfo;
    outState->animID = animInfo.animID;
    outState->animFrame = animInfo.animFrame;

    outState->flags = gMarioState->flags;
    outState->particleFlags = gMarioState->particleFlags;
    outState->invincTimer = gMarioState->invincTimer;
}

// Geometry-only tick for "puppet" Marios — renders a remote player's Mario in
// network multiplayer (SOH/Harpoon). It runs ONLY the skinning/geometry pass
// (gfx_adapter_bind_output_buffers + geo_process_root_hack_single_node), and
// deliberately SKIPS bhv_mario_update: no physics, no collision queries, no
// action func, no animation advance. The caller is expected to drive the pose
// every frame from the network with sm64_set_mario_position / _faceangle /
// _animation / _anim_frame, so the skinned mesh reflects exactly the remote's
// synced animation pose. Because it never touches surfaces, it is safe on an
// instance whose globalState has no static surfaces loaded.
SM64_LIB_FN void sm64_mario_tick_puppet(int32_t marioId, struct SM64MarioGeometryBuffers* outBuffers) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to puppet-tick non-existant Mario with ID: %u", marioId);
        return;
    }

    struct MarioInstance* instance = s_mario_instance_pool.objects[marioId];
    global_state_bind(instance->globalState);

    gMarioState->marioObj->header.gfx.cameraToObject[0] = 0;
    gMarioState->marioObj->header.gfx.cameraToObject[1] = 0;
    gMarioState->marioObj->header.gfx.cameraToObject[2] = 0;

    // Drive the cap (and its model effect) straight from the synced flags. The
    // puppet skips bhv_mario_update — which normally runs
    // mario_update_hitbox_and_cap_model — so without this bodyState->capState
    // stays at its reset MARIO_HAS_DEFAULT_CAP_OFF value and Mario renders
    // BAREHEADED (no cap, so no transformation either). We set it directly from
    // m->flags (no cap-timer decrement, so the flags never expire — the caller
    // re-applies them from the network each frame via sm64_set_mario_state).
    {
        struct MarioBodyState* bs = gMarioState->marioBodyState;
        bs->modelState = 0;
        if (gMarioState->flags & MARIO_VANISH_CAP) {
            bs->modelState = MODEL_STATE_NOISE_ALPHA;
        }
        if (gMarioState->flags & MARIO_METAL_CAP) {
            bs->modelState |= MODEL_STATE_METAL;
        }
        if (gMarioState->flags & MARIO_CAP_ON_HEAD) {
            bs->capState = (gMarioState->flags & MARIO_WING_CAP) ? MARIO_HAS_WING_CAP_ON : MARIO_HAS_DEFAULT_CAP_ON;
        } else {
            bs->capState = MARIO_HAS_DEFAULT_CAP_OFF;
        }
    }

    render_mario(instance, outBuffers);

    gAreaUpdateCounter++;
}

SM64_LIB_FN void sm64_mario_get_hand_transform(int32_t marioId, float outTransform[4][4]) {
    struct MarioInstance* instance = find_instance(marioId);

    if (instance == NULL)
        return;
    memcpy(outTransform, instance->handTransform, sizeof(instance->handTransform));
}

SM64_LIB_FN void sm64_mario_delete(int32_t marioId) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to delete non-existant Mario with ID: %u", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    if (g_is_audio_initialized) {
        stop_sound(SOUND_MARIO_SNORING3, gMarioState->marioObj->header.gfx.cameraToObject);
    }

    free(gMarioObject);
    free_area(gCurrentArea);

    global_state_delete(globalState);
    obj_pool_free_index(&s_mario_instance_pool, marioId);
}

SM64_LIB_FN void sm64_set_mario_action(int32_t marioId, uint32_t action) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    set_mario_action(gMarioState, action, 0);
}

// Sentinel object backing sm64_mario_grab_dummy. Zero-initialized so
// oInteractionSubtype = 0 (LIGHT path in act_picking_up; no DROP_IMMEDIATELY,
// GRABS_MARIO, or HOLDABLE_NPC bits → no exotic transitions). All fields
// the hold/throw action handlers touch (oPosX/Y/Z, oMoveAngleYaw,
// oInteractionSubtype) are valid bytes inside this struct, so the deref
// inside check_common_hold_idle_cancels and mario_throw_held_object is safe.
// Single static is enough — only one Mario instance is supported by the
// caller.
static struct Object s_dummyHeldObject;

// Caller-side glue for "fake-hold" support: poke a sentinel into
// gMarioState->usedObj so ACT_PICKING_UP / ACT_HOLD_IDLE / ACT_HOLD_WALKING
// / ACT_THROWING run their full animation flow without crashing on a NULL
// heldObj deref. ACT_PICKING_UP's actionState=1 calls mario_grab_used_object
// which copies usedObj→heldObj; from there the LIGHT-object path plays
// MARIO_ANIM_PICK_UP_LIGHT_OBJ → ACT_HOLD_IDLE → MARIO_ANIM_IDLE_WITH_LIGHT_OBJ.
// Mario animations finally match a real held-object stance.
//
// Idempotent: calling repeatedly just keeps usedObj pointed at the sentinel.
// Throws null heldObj as part of mario_throw_held_object — usedObj stays
// pointed at the sentinel so the next ACT_PICKING_UP works without a
// re-call.
SM64_LIB_FN void sm64_mario_grab_dummy(int32_t marioId) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->usedObj = &s_dummyHeldObject;
}

// Releases the sentinel: clears usedObj + heldObj. Use when the caller
// needs Mario to act as if he was never holding anything (e.g. on detransform
// or when reverting to plain idle). Optional — most flows don't need this.
SM64_LIB_FN void sm64_mario_release_dummy(int32_t marioId) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->usedObj = NULL;
    gMarioState->heldObj = NULL;
}

SM64_LIB_FN void sm64_set_mario_action_arg(int32_t marioId, uint32_t action, uint32_t actionArg) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    set_mario_action(gMarioState, action, actionArg);
}

SM64_LIB_FN void sm64_set_mario_animation(int32_t marioId, int32_t animID) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct MarioInstance* instance = s_mario_instance_pool.objects[marioId];
    global_state_bind(instance->globalState);

    set_mario_animation(gMarioState, animID);
}

SM64_LIB_FN void sm64_set_mario_anim_frame(int32_t marioId, int16_t animFrame) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->marioObj->header.gfx.animInfo.animFrame = animFrame;
}

SM64_LIB_FN void sm64_set_mario_eye_state(int32_t marioId, int8_t eyeState) {
    struct MarioInstance* instance = find_instance(marioId);

    if (instance == NULL) {
        return;
    }
    global_state_bind(instance->globalState);
    gMarioState->marioBodyState->eyeState = eyeState;
}

SM64_LIB_FN void sm64_set_mario_hand_state(int32_t marioId, int8_t handState) {
    struct MarioInstance* instance = find_instance(marioId);

    if (instance == NULL) {
        return;
    }
    global_state_bind(instance->globalState);
    gMarioState->marioBodyState->handState = handState;
}

SM64_LIB_FN int16_t sm64_get_animation_frame_count(int32_t animID) {
    return (animID < 0) ? 0 : get_mario_animation_frame_count((u32)animID);
}

SM64_LIB_FN void sm64_set_mario_state(int32_t marioId, uint32_t flags) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->flags = flags;
}

SM64_LIB_FN void sm64_set_mario_position(int32_t marioId, float x, float y, float z) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->pos[0] = x;
    gMarioState->pos[1] = y;
    gMarioState->pos[2] = z;
    vec3f_copy(gMarioState->marioObj->header.gfx.pos, gMarioState->pos);
}

SM64_LIB_FN void sm64_set_mario_angle(int32_t marioId, float x, float y, float z) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    vec3s_set(gMarioState->faceAngle, (int16_t)(x / 3.14159f * 32768.f), (int16_t)(y / 3.14159f * 32768.f),
              (int16_t)(z / 3.14159f * 32768.f));
    vec3s_copy(gMarioState->marioObj->header.gfx.angle, gMarioState->faceAngle);
}

SM64_LIB_FN void sm64_set_mario_faceangle(int32_t marioId, float y) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->faceAngle[1] = (int16_t)(y / 3.14159f * 32768.f);
    vec3s_set(gMarioState->marioObj->header.gfx.angle, 0, gMarioState->faceAngle[1], 0);
}

SM64_LIB_FN void sm64_set_mario_velocity(int32_t marioId, float x, float y, float z) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->vel[0] = x;
    gMarioState->vel[1] = y;
    gMarioState->vel[2] = z;
}

SM64_LIB_FN void sm64_set_mario_forward_velocity(int32_t marioId, float vel) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->forwardVel = vel;
}

SM64_LIB_FN void sm64_set_mario_invincibility(int32_t marioId, int16_t timer) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->invincTimer = timer;
}

SM64_LIB_FN void sm64_set_mario_water_level(int32_t marioId, signed int level) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->waterLevel = level;
}

SM64_LIB_FN void sm64_set_mario_gas_level(int32_t marioId, signed int level) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->gasLevel = level;
}

SM64_LIB_FN void sm64_set_mario_health(int32_t marioId, uint16_t health) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->health = health;
    gMarioState->hurtCounter = 0;
    gMarioState->healCounter = 0;
}

SM64_LIB_FN void sm64_mario_take_damage(int32_t marioId, uint32_t damage, uint32_t subtype, float x, float y, float z) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    fake_damage_knock_back(gMarioState, damage, subtype, x, y, z);
}

SM64_LIB_FN void sm64_mario_heal(int32_t marioId, uint8_t healCounter) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->healCounter += healCounter;
}

SM64_LIB_FN void sm64_mario_kill(int32_t marioId) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->health = 0xff;
}

SM64_LIB_FN void sm64_mario_interact_cap(int32_t marioId, uint32_t capFlag, uint16_t capTime, uint8_t playMusic) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    uint16_t capMusic = 0;
    if (gMarioState->action != ACT_GETTING_BLOWN && capFlag != 0) {
        // libsm64 extension: clear ALL cap-type flags before applying the
        // new one. Vanilla SM64 prevents simultaneous caps via gameplay
        // (you can't pick up a second cap box while another is active),
        // so this clear wasn't needed. External callers (Mario-mode in
        // OOT) DO let the user cast multiple caps in sequence — without
        // this clear, the OR-merge produces modelState=0x380 (vanish |
        // metal), which the visual switch case 3 doesn't handle and Mario
        // renders untextured. Also reset capTimer so the new cap's
        // duration applies fresh instead of being clamped to whatever
        // was left of the previous cap.
        gMarioState->flags &=
            ~MARIO_CAP_ON_HEAD & ~MARIO_CAP_IN_HAND & ~MARIO_VANISH_CAP & ~MARIO_METAL_CAP & ~MARIO_WING_CAP;
        gMarioState->capTimer = 0;
        gMarioState->flags |= capFlag;

        switch (capFlag) {
            case MARIO_VANISH_CAP:
                if (capTime == 0)
                    capTime = 600;
                capMusic = SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP);
                break;
            case MARIO_METAL_CAP:
                if (capTime == 0)
                    capTime = 600;
                capMusic = SEQUENCE_ARGS(4, SEQ_EVENT_METAL_CAP);
                break;
            case MARIO_WING_CAP:
                if (capTime == 0)
                    capTime = 1800;
                capMusic = SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP);
                break;
        }

        if (capTime > gMarioState->capTimer) {
            gMarioState->capTimer = capTime;
        }

        if ((gMarioState->action & ACT_FLAG_IDLE) || gMarioState->action == ACT_WALKING) {
            gMarioState->flags |= MARIO_CAP_IN_HAND;
            set_mario_action(gMarioState, ACT_PUTTING_ON_CAP, 0);
        } else {
            gMarioState->flags |= MARIO_CAP_ON_HEAD;
        }

        play_sound(SOUND_MENU_STAR_SOUND, gMarioState->marioObj->header.gfx.cameraToObject);
        play_sound(SOUND_MARIO_HERE_WE_GO, gMarioState->marioObj->header.gfx.cameraToObject);

        if (playMusic != 0 && capMusic != 0) {
            play_cap_music(capMusic);
        }
    }
}

SM64_LIB_FN void sm64_mario_extend_cap(int32_t marioId, uint16_t capTime) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    gMarioState->capTimer += capTime;
}

SM64_LIB_FN bool sm64_mario_attack(int32_t marioId, float x, float y, float z, float hitboxHeight) {
    if (marioId >= s_mario_instance_pool.size || s_mario_instance_pool.objects[marioId] == NULL) {
        DEBUG_PRINT("Tried to use non-existant Mario with ID: %d", marioId);
        return false;
    }

    struct GlobalState* globalState = ((struct MarioInstance*)s_mario_instance_pool.objects[marioId])->globalState;
    global_state_bind(globalState);

    return fake_interact_bounce_top(gMarioState, x, y, z, hitboxHeight);
}

SM64_LIB_FN uint32_t sm64_surface_object_create(const struct SM64SurfaceObject* surfaceObject) {
    uint32_t id = surfaces_load_object(surfaceObject);
    return id;
}

SM64_LIB_FN void sm64_surface_object_move(uint32_t objectId, const struct SM64ObjectTransform* transform) {
    surface_object_update_transform(objectId, transform);
}

SM64_LIB_FN void sm64_surface_object_delete(uint32_t objectId) {
    // A mario standing on the platform that is being destroyed will have a pointer to freed memory if we don't clear
    // it.
    for (int i = 0; i < s_mario_instance_pool.size; ++i) {
        if (s_mario_instance_pool.objects[i] == NULL)
            continue;

        struct GlobalState* state = ((struct MarioInstance*)s_mario_instance_pool.objects[i])->globalState;
        if (state->mgMarioObject->platform == surfaces_object_get_transform_ptr(objectId))
            state->mgMarioObject->platform = NULL;
    }

    surfaces_unload_object(objectId);
}

SM64_LIB_FN int32_t sm64_surface_find_wall_collision(float* xPtr, float* yPtr, float* zPtr, float offsetY,
                                                     float radius) {
    return f32_find_wall_collision(xPtr, yPtr, zPtr, offsetY, radius);
}

SM64_LIB_FN int32_t sm64_surface_find_wall_collisions(struct SM64WallCollisionData* colData) {
    return find_wall_collisions(colData);
}

SM64_LIB_FN float sm64_surface_find_ceil(float posX, float posY, float posZ, struct SM64SurfaceCollisionData** pceil) {
    return find_ceil(posX, posY, posZ, pceil);
}

SM64_LIB_FN float sm64_surface_find_floor_height_and_data(float xPos, float yPos, float zPos,
                                                          struct SM64FloorCollisionData** floorGeo) {
    return find_floor_height_and_data(xPos, yPos, zPos, floorGeo);
}

SM64_LIB_FN float sm64_surface_find_floor_height(float x, float y, float z) {
    return find_floor_height(x, y, z);
}

SM64_LIB_FN float sm64_surface_find_floor(float xPos, float yPos, float zPos,
                                          struct SM64SurfaceCollisionData** pfloor) {
    return find_floor(xPos, yPos, zPos, pfloor);
}

SM64_LIB_FN float sm64_surface_find_water_level(float x, float z) {
    return find_water_level(x, z);
}

SM64_LIB_FN float sm64_surface_find_poison_gas_level(float x, float z) {
    return find_poison_gas_level(x, z);
}

SM64_LIB_FN void sm64_seq_player_play_sequence(uint8_t player, uint8_t seqId, uint16_t arg2) {
    seq_player_play_sequence(player, seqId, arg2);
}

SM64_LIB_FN void sm64_play_music(uint8_t player, uint16_t seqArgs, uint16_t fadeTimer) {
    play_music(player, seqArgs, fadeTimer);
}

SM64_LIB_FN void sm64_stop_background_music(uint16_t seqId) {
    stop_background_music(seqId);
}

SM64_LIB_FN void sm64_fadeout_background_music(uint16_t arg0, uint16_t fadeOut) {
    fadeout_background_music(arg0, fadeOut);
}

SM64_LIB_FN uint16_t sm64_get_current_background_music() {
    return get_current_background_music();
}

SM64_LIB_FN void sm64_play_sound(int32_t soundBits, float* pos) {
    play_sound(soundBits, pos);
}

SM64_LIB_FN void sm64_play_sound_global(int32_t soundBits) {
    play_sound(soundBits, gGlobalSoundSource);
}

SM64_LIB_FN void sm64_set_sound_volume(float vol) {
    gAudioVolume = vol;
}
