#pragma once
#include <stdint.h>
#include <libultraship/color.h>
#ifdef __cplusplus
extern "C" {
#endif
int32_t CVarGetInteger(const char*, int32_t);
Color_RGB8 CVarGetColor24(const char*, Color_RGB8);
#ifdef __cplusplus
}
#endif
