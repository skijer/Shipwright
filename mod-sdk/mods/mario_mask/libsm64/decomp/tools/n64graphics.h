#ifndef N64GRAPHICS_H_
#define N64GRAPHICS_H_

#include <stdint.h>

typedef struct _rgba {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} rgba;

// N64 raw RGBA16 -> RGBA; the caller frees the result.
rgba* raw2rgba(const uint8_t* raw, int width, int height, int depth);

#endif // N64GRAPHICS_H_
