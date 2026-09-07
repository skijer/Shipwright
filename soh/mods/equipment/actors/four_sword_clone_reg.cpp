/**
 * four_sword_clone_reg.cpp - lazy ActorDB registration and tunic hooks for the Four Sword clones.
 * Like boss_remains_actor_reg.cpp, this file must be added to the .vcxproj by hand.
 */

#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

// Outside extern "C" — it transitively pulls in C++ headers.
#include "global.h"

extern "C" {

extern void FourSwordClone_Init(Actor* thisx, PlayState* play);
extern void FourSwordClone_Destroy(Actor* thisx, PlayState* play);
extern void FourSwordClone_Update(Actor* thisx, PlayState* play);
extern void FourSwordClone_Draw(Actor* thisx, PlayState* play);
extern void FourSwordClone_ReapplyTint(PlayState* play);
extern u8 FourSwordClone_ActiveTint(Color_RGB8* out);
extern s16 gFourSwordCloneId;
extern size_t gFourSwordCloneStructSize;
extern s32 gFourSwordCloneDrawing;
}

#define FOUR_SWORD_CLONE_FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

// Lifting the totem is lifting a rock, so it answers to the same rando gate. RAND_INF_CAN_GRAB
// lives in a C++ enum header, which is why this is here and not in the actor's C file.
extern "C" u8 FourSword_TotemGrabAllowed(void) {
    return !IS_RANDO || Flags_GetRandomizerInf(RAND_INF_CAN_GRAB);
}

extern "C" void FourSwordClone_EnsureRegistered(void) {
    if (gFourSwordCloneId != -1) {
        return;
    }

    ActorDBInit init;
    init.name = "FourSwordClone";
    init.desc = "Four Sword formation clone";
    init.category = ACTORCAT_MISC;
    init.flags = FOUR_SWORD_CLONE_FLAGS;
    init.objectId = OBJECT_GAMEPLAY_KEEP;
    init.instanceSize = gFourSwordCloneStructSize;
    init.init = FourSwordClone_Init;
    init.destroy = FourSwordClone_Destroy;
    init.update = FourSwordClone_Update;
    init.draw = FourSwordClone_Draw;
    gFourSwordCloneId = ActorDB::Instance->AddEntry(init).entry.id;

    // ⚠️ NEVER write through this hook's Color_RGB8*: it points at the GLOBAL sTunicColors, so
    // assigning through it repaints every later Link, the real player included.
    REGISTER_VB_SHOULD(VB_APPLY_TUNIC_COLOR, {
        Color_RGB8 unused;

        if (FourSwordClone_ActiveTint(&unused)) {
            *should = false;
        }
    });

    REGISTER_VB_SHOULD(VB_PLAYER_OVERRIDE_LIMB_DRAW, {
        s32 limbIndex = va_arg(args, s32);
        Gfx** dList = va_arg(args, Gfx**);
        void* thisx = va_arg(args, void*);
        PlayState* play = va_arg(args, PlayState*);

        (void)limbIndex;
        (void)dList;
        (void)thisx;
        FourSwordClone_ReapplyTint(play);
    });
}
