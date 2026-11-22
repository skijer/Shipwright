#pragma once

#include "../include/types.h"
#include "../include/PR/gbi.h"
#include "model_lists.h"

#define MARIO_MODEL_LIST(name, offset) extern Gfx name[];
MARIO_MODEL_LISTS
#undef MARIO_MODEL_LIST
