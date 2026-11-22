#ifndef BANJO_POSE_TABLE_H
#define BANJO_POSE_TABLE_H

#include <stdint.h>

typedef enum BanjoPosePlayback {
    // Banjo's clip stretched over Link's, frame for frame.
    BANJO_POSE_FOLLOW_LINK,
    BANJO_POSE_LOOP,
    // Played at Banjo's own speed, holding the last frame.
    BANJO_POSE_ONCE,
    // Advanced by the distance Link covers.
    BANJO_POSE_STRIDE,
} BanjoPosePlayback;

typedef struct BanjoPose {
    const char* linkAnimation;
    uint16_t banjoAnimation;
    uint16_t model;
    uint8_t playback;
    // A window of Banjo's clip; lastFrame 0 plays to its end.
    int16_t firstFrame;
    int16_t lastFrame;
} BanjoPose;

// `linkAnimation` is the name after "gPlayerAnim_", the same in both games; NULL when Banjo has no equivalent.
const BanjoPose* BanjoPoseTable_Find(const char* linkAnimation);
// Banjo's eye selector for Link's animated eyes; 0 leaves Banjo to his own blinking.
uint8_t BanjoPoseTable_GetEyeSelector(int32_t linkEyes);

#endif
