#include "banjo_poses.h"

#include <math.h>
#include <string.h>

#include "banjo_animation_ids.h"
#include "banjo_pose_table.h"

#define RESOURCE_PATH_PREFIX "__OTR__"
#define LINK_ANIMATION_PREFIX "gPlayerAnim_"
#define LINK_APPEARANCE_JOINT 22
#define BANJO_FRAMES_PER_SECOND 30.0f
#define GAME_VIS_PER_SECOND 60.0f
#define MOVING_DISTANCE_MIN 0.1f
#define RUN_DISTANCE_MIN 4.0f
// Frames of Banjo's walk and run cycles per game unit Link covers, measured against his stride.
#define WALK_FRAMES_PER_UNIT 3.0f
#define RUN_FRAMES_PER_UNIT 1.5f
#define EYES_OPEN 1
#define EYES_SHUT 8
#define BLINK_PERIOD_FRAMES 80
#define BLINK_LENGTH_FRAMES 4

typedef struct PoseState {
    const void* linkAnimation;
    uint16_t animation;
    uint16_t model;
    float frame;
    Vec3f lastPosition;
    bool hasLastPosition;
    int32_t blinkTimer;
} PoseState;

static PoseState sState;

static const uint16_t kKazooieClips[] = {
    BANJO_ANIM_STAND_IDLE,   BANJO_ANIM_STAND_KAZOOIE_PECK, BANJO_ANIM_STAND_PULL_KAZOOIE, BANJO_ANIM_REST_LISTEN,
    BANJO_ANIM_TIMEOUT,      BANJO_ANIM_JIG_JIGGY,          BANJO_ANIM_SHOCK_CHARGE,       BANJO_ANIM_BARGE,
    BANJO_ANIM_CROUCH_IDLE,
};

void BanjoPoses_Reset(void) {
    sState = (PoseState){ 0 };
}

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

static bool ShowsKazooie(uint16_t animation) {
    for (size_t index = 0; index < sizeof(kKazooieClips) / sizeof(kKazooieClips[0]); index++) {
        if (kKazooieClips[index] == animation) {
            return true;
        }
    }
    return false;
}

// Link's animated eyes win; otherwise Banjo blinks on his own.
static uint8_t GetEyes(Player* player) {
    const int32_t linkEyes = (player->skelAnime.jointTable[LINK_APPEARANCE_JOINT].x & 0xF) - 1;
    const uint8_t selector = BanjoPoseTable_GetEyeSelector(linkEyes);

    sState.blinkTimer = (sState.blinkTimer + 1) % BLINK_PERIOD_FRAMES;
    if (selector != 0) {
        return selector;
    }
    return (sState.blinkTimer < BLINK_LENGTH_FRAMES) ? EYES_SHUT : EYES_OPEN;
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

// What Link does in normal play, read from his state instead of his clip, which has no table entry.
static BanjoPose ChooseMotionPose(Player* player, float stride) {
    BanjoPose pose = { NULL, BANJO_ANIM_STAND_IDLE, BANJO_MODEL_PLAYER, BANJO_POSE_LOOP, 0, 0 };
    const bool isSwimming = (player->stateFlags1 & BANJO_STATE1_SWIMMING) != 0;
    const bool isGrounded = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;

    if (isSwimming) {
        pose.banjoAnimation = (stride >= MOVING_DISTANCE_MIN) ? BANJO_ANIM_SWIM_MOVE : BANJO_ANIM_SWIM_IDLE;
    } else if (!isGrounded) {
        pose.banjoAnimation = (player->actor.velocity.y > 0.0f) ? BANJO_ANIM_JUMP : BANJO_ANIM_JUMP_FALL;
        pose.playback = (player->actor.velocity.y > 0.0f) ? BANJO_POSE_ONCE : BANJO_POSE_LOOP;
    } else if (player->meleeWeaponState != 0) {
        pose.banjoAnimation = BANJO_ANIM_PUNCH;
        pose.playback = BANJO_POSE_FOLLOW_LINK;
    } else if (stride >= RUN_DISTANCE_MIN) {
        pose.banjoAnimation = BANJO_ANIM_WALK_RUN;
        pose.playback = BANJO_POSE_STRIDE;
    } else if (stride >= MOVING_DISTANCE_MIN) {
        pose.banjoAnimation = BANJO_ANIM_WALK;
        pose.playback = BANJO_POSE_STRIDE;
    }
    return pose;
}

static float GetOwnSpeedStep(void) {
    const int32_t updateRate = (R_UPDATE_RATE > 0) ? R_UPDATE_RATE : 3;

    return BANJO_FRAMES_PER_SECOND * updateRate / GAME_VIS_PER_SECOND;
}

static float GetLinkProgress(Player* player) {
    const float lastFrame = player->skelAnime.endFrame;

    if (lastFrame <= 0.0f) {
        return 1.0f;
    }
    return fminf(fmaxf(player->skelAnime.curFrame / lastFrame, 0.0f), 1.0f);
}

static float AdvanceFrame(Player* player, const BanjoPose* pose, float first, float last, float stride) {
    const float length = last - first;
    const float strideRate = (pose->banjoAnimation == BANJO_ANIM_WALK_RUN) ? RUN_FRAMES_PER_UNIT : WALK_FRAMES_PER_UNIT;

    if (length <= 0.0f) {
        return first;
    }
    switch (pose->playback) {
        case BANJO_POSE_FOLLOW_LINK:
            return first + GetLinkProgress(player) * length;
        case BANJO_POSE_ONCE:
            sState.frame = fminf(sState.frame + GetOwnSpeedStep(), length);
            return first + sState.frame;
        case BANJO_POSE_STRIDE:
            sState.frame = fmodf(sState.frame + stride * strideRate, length);
            return first + sState.frame;
        case BANJO_POSE_LOOP:
            sState.frame = fmodf(sState.frame + GetOwnSpeedStep(), length);
            return first + sState.frame;
        default:
            return first;
    }
}

void BanjoPoses_Follow(Player* player, BanjoClipRangeFunc getClipRange, BanjoPoseFrame* out) {
    const float stride = MeasureStride(player);
    const char* name = GetLinkAnimationName(player->skelAnime.animation);
    const BanjoPose* tablePose = (name != NULL) ? BanjoPoseTable_Find(name) : NULL;
    const BanjoPose pose = (tablePose != NULL) ? *tablePose : ChooseMotionPose(player, stride);

    if (player->skelAnime.animation != sState.linkAnimation || pose.banjoAnimation != sState.animation ||
        pose.model != sState.model) {
        sState.linkAnimation = player->skelAnime.animation;
        sState.animation = pose.banjoAnimation;
        sState.model = pose.model;
        sState.frame = 0.0f;
    }
    out->model = pose.model;
    out->animation = pose.banjoAnimation;
    out->eyes = GetEyes(player);
    out->isKazooieOut = ShowsKazooie(pose.banjoAnimation);
    float first = 0.0f;
    float last = 0.0f;

    if (!getClipRange(pose.banjoAnimation, &first, &last)) {
        out->frame = 0.0f;
        return;
    }
    first = (pose.firstFrame > 0) ? pose.firstFrame : first;
    last = (pose.lastFrame > 0) ? pose.lastFrame : last;
    out->frame = AdvanceFrame(player, &pose, first, last, stride);
}
