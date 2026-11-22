#ifndef SM64_BUILD_H
#define SM64_BUILD_H

// libsm64 compiles inside the mod's own build, so this replaces the defines of its Makefile.
#undef F3DEX_GBI_2
#define GBI_FLOATS
#define VERSION_US
#define NO_SEGMENTED_MEMORY

#ifdef _MSC_VER
#define __attribute__(x)
#endif

#endif
