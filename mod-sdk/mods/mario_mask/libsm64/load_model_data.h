#pragma once

#include <stdint.h>

// Converts Mario's mesh from segment 04 of the ROM into the display lists the geo layout draws.
extern void load_mario_model_from_rom(const uint8_t* rom);
extern void unload_mario_model(void);
