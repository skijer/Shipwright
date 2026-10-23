#ifndef NEI_EASY_EQUIPMENT_INPUT_H
#define NEI_EASY_EQUIPMENT_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#define EASY_HOLD_FRAMES 10
typedef enum { EASY_NONE, EASY_TAP, EASY_OPEN, EASY_COMMIT, EASY_CANCEL } EasyGesture;
typedef struct { uint16_t frames; bool open; bool waitRelease; } EasyHold;

// Cancellation consumes the entire gesture, even when C remains held after B.
static inline EasyGesture EasyHold_Update(EasyHold* hold, bool down, bool cancel, bool allowed) {
    if (hold->waitRelease) {
        if (!down) { hold->waitRelease = false; }
        return EASY_NONE;
    }
    if (!allowed || cancel) {
        bool active = hold->frames != 0 || hold->open;
        hold->frames = 0;
        hold->open = false;
        hold->waitRelease = down;
        return active ? EASY_CANCEL : EASY_NONE;
    }
    if (!down) {
        EasyGesture result = hold->open ? EASY_COMMIT : hold->frames != 0 ? EASY_TAP : EASY_NONE;
        hold->frames = 0;
        hold->open = false;
        return result;
    }
    if (hold->frames < EASY_HOLD_FRAMES) { ++hold->frames; }
    if (hold->frames == EASY_HOLD_FRAMES && !hold->open) {
        hold->open = true;
        return EASY_OPEN;
    }
    return EASY_NONE;
}

static inline EasyGesture EasyHold_UpdateMode(EasyHold* hold, bool down, bool cancel, bool allowed, bool instant) {
    if (instant && down && allowed && !cancel && !hold->open && !hold->waitRelease)
        hold->frames=EASY_HOLD_FRAMES;
    return EasyHold_Update(hold,down,cancel,allowed);
}

#endif
