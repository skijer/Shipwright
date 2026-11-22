#ifndef BANJO_KAZOOIE_CORE2_ANIM_COMMONCACHE_H
#define BANJO_KAZOOIE_CORE2_ANIM_COMMONCACHE_H

#include "../../../shim/ultra64.h"
#include "../../../include/enums.h"
#include "../../../include/core2/animationfile.h"

void __animcommoncache_initPersistent(void);
void __animcommoncache_loadAll(void);
BKAnimationFileBin *animcommoncache_get(enum asset_e asset_id);
void animcommoncache_free(void);
void animcommoncache_init(void);
void animcommoncache_flushStale(bool persistant);
void animcommoncache_update(void);

#endif
