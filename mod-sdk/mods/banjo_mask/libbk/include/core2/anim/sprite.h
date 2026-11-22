#ifndef __CORE2_ANIM_SPRITE_H__
#define __CORE2_ANIM_SPRITE_H__

#include "../../../shim/ultra64.h"
#include "../../../shim/ultra64.h"

typedef struct{
    u8 index;
    u8 duration;
} AnimSpriteStep;

typedef enum {
    ANIM_SPRITE_STATE_NONE,
    ANIM_SPRITE_STATE_ONCE,
    ANIM_SPRITE_STATE_LOOP,
    ANIM_SPRITE_STATE_STOPPED
} AnimSpriteState;

typedef struct{
    AnimSpriteStep *steps;
    f32 remaining_time;
    f32 speed_multiplier;
    u32 step_count:7; //This is the index in the AnimSpriteStep list
    u32 current_step:7; //current frame
    u32 start_step:7;
    u32 current_frame:7; //This is the actual sprite frame
    u32 state:2;
    u32 unknown:1;
    u32 active:1; //active
} AnimSprite;

void animsprite_init(void);
AnimSprite * animsprite_new(void);
void animsprite_free(AnimSprite *self);
void animsprite_terminate(void);
void animsprite_update(AnimSprite *self);
void animsprite_default(AnimSprite *self);
void animsprite_loop(AnimSprite *self);
void animsprite_set_steps(AnimSprite *self, AnimSpriteStep *step_list, s32 size);
void animsprite_set_state(AnimSprite *self, AnimSpriteState step);
void animsprite_set_unknown(AnimSprite *self, s32 value);
void animsprite_set_frame(AnimSprite *self, s32 frame_index);
void animsprite_set_speed(AnimSprite *self, f32 speed);
void animsprite_set_start_step(AnimSprite *self, s32 step);
s32 animsprite_get_frame(AnimSprite *self);
AnimSpriteState animsprite_get_current_state(AnimSprite *self);
s32 animsprite_get_unknown(AnimSprite *self);
f32 animsprite_get_speed(AnimSprite *self);
int animsprite_is_stopped(AnimSprite *self);
int animsprite_is_at_step(AnimSprite *self, s32 frame);
#endif
