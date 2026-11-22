#include "../../sm64_build.h"
#include <stdlib.h>

#include "n64graphics.h"

#define SCALE_5_8(VAL_) (((VAL_)*0xFF) / 0x1F)

rgba* raw2rgba(const uint8_t* raw, int width, int height, int depth) {
    rgba* img = malloc(width * height * sizeof(*img));

    if (img == NULL || depth != 16) {
        free(img);
        return NULL;
    }
    for (int i = 0; i < width * height; i++) {
        img[i].red = SCALE_5_8((raw[i * 2] & 0xF8) >> 3);
        img[i].green = SCALE_5_8(((raw[i * 2] & 0x07) << 2) | ((raw[i * 2 + 1] & 0xC0) >> 6));
        img[i].blue = SCALE_5_8((raw[i * 2 + 1] & 0x3E) >> 1);
        img[i].alpha = (raw[i * 2 + 1] & 0x01) ? 0xFF : 0x00;
    }
    return img;
}
