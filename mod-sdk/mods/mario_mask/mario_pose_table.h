#ifndef MARIO_POSE_TABLE_H
#define MARIO_POSE_TABLE_H

#include <stdint.h>

typedef enum MarioPosePlayback {
    // Mario's clip stretched over Link's, frame for frame.
    MARIO_POSE_FOLLOW_LINK,
    MARIO_POSE_LOOP,
    // Played at Mario's own speed, holding the last frame.
    MARIO_POSE_ONCE,
    // Advanced by the distance Link covers, like SM64 scales a walk by Mario's speed.
    MARIO_POSE_STRIDE,
} MarioPosePlayback;

typedef struct MarioPose {
    const char* linkAnimation;
    int16_t marioAnimation;
    uint8_t playback;
    uint8_t hand;
    // A window of Mario's clip; lastFrame 0 plays to its end.
    int16_t firstFrame;
    int16_t lastFrame;
} MarioPose;

// `linkAnimation` is the name after "gPlayerAnim_", the same in both games; NULL when Mario has no equivalent.
const MarioPose* MarioPoseTable_Find(const char* linkAnimation);

#endif
