#include "z_bg_toki_swd.h"

#include <limits.h>
#include <string.h>

// The native actor is at (-1, 68, 0), facing yaw 0 in every Temple of Time setup.
// Apply the same rigid transform to its camera splines, player cues and the
// player's otherwise hardcoded action-18 animation origin.
void TimePedestalCutscene_TransformPoint(Vec3f* point, const Vec3f* origin, s16 yaw) {
    f32 x = point->x + 1.0f;
    f32 z = point->z;
    f32 sinYaw = Math_SinS(yaw);
    f32 cosYaw = Math_CosS(yaw);

    point->x = origin->x + x * cosYaw + z * sinYaw;
    point->y += origin->y - 68.0f;
    point->z = origin->z + z * cosYaw - x * sinYaw;
}

// Only the two native sword scripts are accepted. Unknown command types fail
// closed, so adding a native script command cannot silently introduce logic.
size_t TimePedestalCutscene_Build(CutsceneData* output, size_t capacity, const CutsceneData* source, size_t wordCount,
                                  const Vec3f* origin, s16 yaw, s16* ageSwapFrame) {
    size_t read = 2;
    size_t write = 2;
    s32 entries = 0;
    s16 swapFrame = -1;

    if (output == NULL || source == NULL || origin == NULL || ageSwapFrame == NULL || wordCount < 4 ||
        capacity < wordCount || source[0].i < 1) {
        return 0;
    }

    for (s32 entry = 0; entry < source[0].i; entry++) {
        size_t start = read;
        size_t length;
        s32 type;

        if (read + 2 > wordCount) {
            return 0;
        }
        type = source[read].i;
        if (type == CS_CMD_CAM_EYE || type == CS_CMD_CAM_AT) {
            read += 3;
            do {
                if (read + 4 > wordCount) {
                    return 0;
                }
                read += 4;
            } while (source[read - 4].b[0] != CS_CMD_STOP);
        } else if (type == CS_CMD_SET_PLAYER_ACTION || type == CS_CMD_MISC || type == CS_CMD_SET_LIGHTING) {
            s32 count = source[read + 1].i;
            if (count < 0 || (size_t)count > (wordCount - read - 2) / 12) {
                return 0;
            }
            read += 2 + (size_t)count * 12;
        } else if (type == CS_CMD_TERMINATOR || type == CS_CMD_SCENE_TRANS_FX) {
            if (read + 4 > wordCount) {
                return 0;
            }
            read += 4;
        } else {
            return 0;
        }

        // The destination normally edits story flags, Farore's Wind, discovery
        // and the randomizer check. Keep only its timing; it never reaches the
        // cutscene interpreter. Temple lighting/room effects are not portable.
        if (type == CS_CMD_TERMINATOR) {
            if (source[start + 2].s[0] != TEMPLE_OF_TIME_AFTER_USE_MS || swapFrame >= 0) {
                return 0;
            }
            swapFrame = source[start + 2].s[1];
            continue;
        }
        if (type == CS_CMD_MISC || type == CS_CMD_SET_LIGHTING) {
            continue;
        }

        length = read - start;
        memcpy(output + write, source + start, length * sizeof(*output));
        if (type == CS_CMD_CAM_EYE || type == CS_CMD_CAM_AT) {
            for (size_t point = write + 3; point < write + length; point += 4) {
                Vec3f pos = { output[point + 2].s[0], output[point + 2].s[1], output[point + 3].s[0] };
                TimePedestalCutscene_TransformPoint(&pos, origin, yaw);
                if (pos.x < SHRT_MIN || pos.x > SHRT_MAX || pos.y < SHRT_MIN || pos.y > SHRT_MAX || pos.z < SHRT_MIN ||
                    pos.z > SHRT_MAX) {
                    return 0;
                }
                output[point + 2].s[0] = (s16)roundf(pos.x);
                output[point + 2].s[1] = (s16)roundf(pos.y);
                output[point + 3].s[0] = (s16)roundf(pos.z);
            }
        } else if (type == CS_CMD_SET_PLAYER_ACTION) {
            for (size_t cue = write + 2; cue < write + length; cue += 12) {
                for (size_t position = cue + 3; position <= cue + 6; position += 3) {
                    Vec3f pos = { output[position].i, output[position + 1].i, output[position + 2].i };
                    TimePedestalCutscene_TransformPoint(&pos, origin, yaw);
                    output[position].i = (s32)roundf(pos.x);
                    output[position + 1].i = (s32)roundf(pos.y);
                    output[position + 2].i = (s32)roundf(pos.z);
                }
                output[cue + 2].s[0] += yaw;
                f32 x = output[cue + 9].f;
                f32 z = output[cue + 11].f;
                output[cue + 9].f = x * Math_CosS(yaw) + z * Math_SinS(yaw);
                output[cue + 11].f = z * Math_CosS(yaw) - x * Math_SinS(yaw);
            }
        }
        write += length;
        entries++;
    }

    if (read + 2 > wordCount || source[read].i != -1 || swapFrame < 0) {
        return 0;
    }
    output[0].i = entries;
    output[1] = source[1];
    output[write++].i = -1;
    output[write++].i = 0;
    *ageSwapFrame = swapFrame;
    return write;
}
