#ifndef LIBBK_BUILD_H
#define LIBBK_BUILD_H

// libbk compiles inside the mod's own build, so this replaces the defines of Lighthouse's CMakeLists.
#undef F3DEX_GBI_2
#ifndef F3DEX_GBI
#define F3DEX_GBI 1
#endif
#define VERSION_US 1
#define NON_MATCHING 1
#define NON_EQUIVALENT 1
#define AVOID_UB 1
#define GBI_FLOATS 1
#define N_MICRO 1
#define _LANGUAGE_C 1
#define _USE_MATH_DEFINES 1

#include <stdlib.h>

#ifdef _MSC_VER
#define __attribute__(x)
#endif

#endif
