#include "mario_poses.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "libsm64/libsm64.h"
#include "libsm64/decomp/include/mario_animation_ids.h"
#include "libsm64/decomp/include/mario_geo_switch_case_ids.h"
#include "mario_pose_table.h"
#include "mario_world.h"

#define RESOURCE_PATH_PREFIX "__OTR__"
#define LINK_ANIMATION_PREFIX "gPlayerAnim_"
#define LINK_APPEARANCE_JOINT 22
#define LINK_EYES_COUNT 8
#define MARIO_FRAMES_PER_SECOND 30.0f
#define GAME_VIS_PER_SECOND 60.0f
#define MOVING_DISTANCE_MIN 0.1f
// SM64 walks below 18 units per tick, which is this many game units per game frame.
#define RUN_DISTANCE_MIN 6.75f
// SM64 advances a walk by speed / 4 per tick: in game units per game frame that is the distance times the scale / 4.
#define STRIDE_FRAMES_PER_UNIT (MARIO_WORLD_SCALE / 4.0f)

typedef struct PoseState {
    const void* linkAnimation;
    const MarioPose* pose;
    int16_t marioAnimation;
    float frame;
    Vec3f lastPosition;
    bool hasLastPosition;
} PoseState;

static PoseState sState;

// Indexed by Link's eye texture: open, half, closed, left, right, wide, down, wincing.
static const int8_t kMarioEyesForLink[LINK_EYES_COUNT] = {
    MARIO_EYES_OPEN,      MARIO_EYES_HALF_CLOSED, MARIO_EYES_CLOSED,    MARIO_EYES_LOOK_LEFT,
    MARIO_EYES_LOOK_RIGHT, MARIO_EYES_OPEN,       MARIO_EYES_LOOK_DOWN, MARIO_EYES_CLOSED,
};

void MarioPoses_Reset(void) {
    sState = (PoseState){ 0 };
}

// Both hosts keep Link's animation as its resource path, so its file name is the same key in either game.
static const char* GetLinkAnimationName(const void* animation) {
    const char* path = animation;

    if (path == NULL || strncmp(path, RESOURCE_PATH_PREFIX, sizeof(RESOURCE_PATH_PREFIX) - 1) != 0) {
        return NULL;
    }

    const char* name = strrchr(path, '/');

    name = (name != NULL) ? name + 1 : path;
    if (strncmp(name, LINK_ANIMATION_PREFIX, sizeof(LINK_ANIMATION_PREFIX) - 1) == 0) {
        name += sizeof(LINK_ANIMATION_PREFIX) - 1;
    }
    return name;
}

// The animation sets Link's eyes in the low nibble after the limbs, plus one; zero leaves them to his blinking.
static int8_t GetMarioEyes(Player* player) {
    const int32_t linkEyes = (player->skelAnime.jointTable[LINK_APPEARANCE_JOINT].x & 0xF) - 1;

    if (linkEyes < 0 || linkEyes >= LINK_EYES_COUNT) {
        return MARIO_EYES_BLINK;
    }
    return kMarioEyesForLink[linkEyes];
}

static float MeasureStride(Player* player) {
    const Vec3f* position = &player->actor.world.pos;
    float distance = 0.0f;

    if (sState.hasLastPosition) {
        const float deltaX = position->x - sState.lastPosition.x;
        const float deltaZ = position->z - sState.lastPosition.z;

        distance = sqrtf(deltaX * deltaX + deltaZ * deltaZ);
    }
    sState.lastPosition = *position;
    sState.hasLastPosition = true;
    return distance;
}

// Without a matching pose Mario idles, walks or runs as Link moves, carrying what Link carries.
static int16_t ChooseMotionAnimation(Player* player, float stride) {
    const bool isCarrying = (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) != 0;

    if (stride >= RUN_DISTANCE_MIN) {
        return isCarrying ? MARIO_ANIM_RUN_WITH_LIGHT_OBJ : MARIO_ANIM_RUNNING;
    }
    if (stride >= MOVING_DISTANCE_MIN) {
        return isCarrying ? MARIO_ANIM_WALK_WITH_LIGHT_OBJ : MARIO_ANIM_WALKING;
    }
    return isCarrying ? MARIO_ANIM_IDLE_WITH_LIGHT_OBJ : MARIO_ANIM_IDLE_HEAD_CENTER;
}

static float GetOwnSpeedStep(void) {
    const int32_t updateRate = (R_UPDATE_RATE > 0) ? R_UPDATE_RATE : 3;

    return MARIO_FRAMES_PER_SECOND * updateRate / GAME_VIS_PER_SECOND;
}

static float GetLinkProgress(Player* player) {
    const float lastFrame = player->skelAnime.endFrame;

    if (lastFrame <= 0.0f) {
        return 1.0f;
    }
    return fminf(fmaxf(player->skelAnime.curFrame / lastFrame, 0.0f), 1.0f);
}

static int16_t AdvanceFrame(Player* player, const MarioPose* pose, int16_t frameCount, float stride) {
    const int16_t first = pose->firstFrame;
    const int16_t last = (pose->lastFrame > 0 && pose->lastFrame < frameCount) ? pose->lastFrame : frameCount - 1;
    const float length = (float)(last - first);

    switch (pose->playback) {
        case MARIO_POSE_FOLLOW_LINK:
            return first + (int16_t)(GetLinkProgress(player) * length);
        case MARIO_POSE_ONCE:
            sState.frame = fminf(sState.frame + GetOwnSpeedStep(), length);
            return first + (int16_t)sState.frame;
        case MARIO_POSE_STRIDE:
            sState.frame = fmodf(sState.frame + stride * STRIDE_FRAMES_PER_UNIT, length + 1.0f);
            return first + (int16_t)sState.frame;
        case MARIO_POSE_LOOP:
            sState.frame = fmodf(sState.frame + GetOwnSpeedStep(), length + 1.0f);
            return first + (int16_t)sState.frame;
        default:
            return first;
    }
}

static const MarioPose* ResolvePose(Player* player, float stride, MarioPose* motion) {
    const char* name = GetLinkAnimationName(player->skelAnime.animation);
    const MarioPose* pose = (name != NULL) ? MarioPoseTable_Find(name) : NULL;

    if (pose != NULL) {
        return pose;
    }
    *motion = (MarioPose){ NULL, ChooseMotionAnimation(player, stride), MARIO_POSE_STRIDE, MARIO_HAND_FISTS, 0, 0 };
    if (stride < MOVING_DISTANCE_MIN) {
        motion->playback = MARIO_POSE_LOOP;
    }
    return motion;
}

void MarioPoses_Follow(Player* player, int32_t marioId, struct SM64MarioGeometryBuffers* geometry) {
    const float stride = MeasureStride(player);
    MarioPose motion;
    const MarioPose* pose = ResolvePose(player, stride, &motion);
    const bool isNewClip =
        player->skelAnime.animation != sState.linkAnimation || pose->marioAnimation != sState.marioAnimation;

    if (isNewClip) {
        sState.linkAnimation = player->skelAnime.animation;
        sState.marioAnimation = pose->marioAnimation;
        sState.frame = 0.0f;
    }

    const int16_t frameCount = sm64_get_animation_frame_count(pose->marioAnimation);
    const int16_t frame = (frameCount > 1) ? AdvanceFrame(player, pose, frameCount, stride) : 0;

    sm64_set_mario_animation(marioId, pose->marioAnimation);
    sm64_set_mario_anim_frame(marioId, frame);
    sm64_set_mario_eye_state(marioId, GetMarioEyes(player));
    sm64_set_mario_hand_state(marioId, (int8_t)pose->hand);
    geometry->numTrianglesUsed = 0;
    sm64_mario_tick_puppet(marioId, geometry);
}
