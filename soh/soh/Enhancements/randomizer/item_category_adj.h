#pragma once

#include "../item-tables/ItemTableTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Container Style Matches Contents classes, numbered as 2Ship's S2HContainerStyle so one mod palette fits both.
// Mods read them through the "rando.container_style" service.
typedef enum {
    CONTAINER_STYLE_NONE,
    CONTAINER_STYLE_JUNK,
    CONTAINER_STYLE_LESSER,
    CONTAINER_STYLE_MAJOR,
    CONTAINER_STYLE_HEALTH,
    CONTAINER_STYLE_SMALL_KEY,
    CONTAINER_STYLE_BOSS_KEY,
    CONTAINER_STYLE_TOKEN,
} ContainerStyle;

GetItemCategory Randomizer_AdjustItemCategory(GetItemEntry item);

#ifdef __cplusplus
}
#endif
