#pragma once

#include <stdint.h>

// The look-around eyes join the atlas after the wings, so wing UVs keep tiles 9 and 10.
enum MarioTextures {
    mario_texture_metal = 0,
    mario_texture_yellow_button,
    mario_texture_m_logo,
    mario_texture_hair_sideburn,
    mario_texture_mustache,
    mario_texture_eyes_front,
    mario_texture_eyes_half_closed,
    mario_texture_eyes_closed,
    mario_texture_eyes_dead,
    mario_texture_wings_half_1,
    mario_texture_wings_half_2,
    mario_texture_eyes_right,
    mario_texture_eyes_left,
    mario_texture_eyes_up,
    mario_texture_eyes_down,
    NUM_USED_TEXTURES
};

static const int mario_tex_offsets[NUM_USED_TEXTURES] = { 144,   4240,  6288,  8336,  10384, 12432, 14480, 16528,
                                                          30864, 32912, 37008, 22672, 24720, 26768, 28816 };
static const int mario_tex_widths[NUM_USED_TEXTURES] = { 64, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 };
static const int mario_tex_heights[NUM_USED_TEXTURES] = { 32, 32, 32, 32, 32, 32, 32, 32, 32, 64, 64, 32, 32, 32, 32 };

extern void load_mario_textures_from_rom(const uint8_t* rom, uint8_t* outTexture);