/**
 * deku_flower.h — Majora's Mask's Deku Flower (Obj_Etcetera) in soh. Skijer's NEI
 * Include after z64.h. Registered at runtime (ActorDB), so the id is a global, -1 until then.
 */
#ifndef DEKU_FLOWER_H
#define DEKU_FLOWER_H

#define DEKU_FLOWER_TYPE(params) (((params)&0xFF80) >> 7)
#define DEKU_FLOWER_PARAMS(type) (((type) << 7) & 0xFF80)

typedef enum {
    DEKU_FLOWER_TYPE_PINK,
    DEKU_FLOWER_TYPE_PINK_WITH_INITIAL_BOUNCE,
    DEKU_FLOWER_TYPE_GOLD,
    DEKU_FLOWER_TYPE_GOLD_WITH_INITIAL_BOUNCE,
    DEKU_FLOWER_TYPE_MAX
} DekuFlowerType;

#ifdef __cplusplus
extern "C" {
#endif

extern s16 gDekuFlowerId;
extern size_t gDekuFlowerStructSize;

void DekuFlower_Init(Actor* thisx, PlayState* play);
void DekuFlower_Destroy(Actor* thisx, PlayState* play);
void DekuFlower_Update(Actor* thisx, PlayState* play);

// The flower `actor` is standing on, or NULL.
DynaPolyActor* DekuFlower_Underfoot(PlayState* play, Actor* actor);
u8 DekuFlower_IsGold(DynaPolyActor* flower);
// Deku Link just shot out of it.
void DekuFlower_OnLaunch(DynaPolyActor* flower);

#ifdef __cplusplus
}
#endif

#endif
