// The tails play Keaton's own clips, baked out of object_kitan and picked by Link's
// eye state. No physics: the clips carry the motion. See keaton_tails.h.
#include "keaton_tails.h"
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "soh/ResourceManagerHelpers.h"

#include <cstring>

#include <libultraship/bridge.h>

// OPEN_DISPS declares these at block scope, which in a .cpp binds to a mangled
// symbol no translation unit defines unless a file-scope extern "C" is visible.
extern "C" void FrameInterpolation_RecordOpenChild(const void* a, int b);
extern "C" void FrameInterpolation_RecordCloseChild(void);

#define TAIL_SKEL_PATH "__OTR__objects/forms/keaton/object_link_boy/gLinkAdultTails"
#define TAIL_SEGMENTS 9
#define TAIL_MTX_SEGMENT 0x0B

namespace {

#include "keaton_tail_anim.inc.c"

Vec3s sTail[TAIL_SEGMENTS];
u32 sFrame = 0;
SkeletonHeader* sSkel = nullptr;
bool sTried = false;
u8 sPose = 0;

SkeletonHeader* Skel() {
    if (!sTried) {
        sTried = true;
        SkeletonHeader* h = ResourceMgr_LoadSkeletonByName(TAIL_SKEL_PATH, nullptr);
        // A missing resource hands back an unrelated pointer; walking it crashes.
        if (h != nullptr && h->limbCount > 0 && h->limbCount <= TAIL_SEGMENTS && h->segment != nullptr) {
            sSkel = h;
        }
    }
    return sSkel;
}

// Player_DrawImpl reads the animation-driven face out of joint 22, low nibble the
// eye, one-based so zero means "not overridden this frame". sEyeTextures names the
// slots: 5 is Shock, 6 and 7 the pained pair.
u8 PoseFor(Player* player) {
    s32 eye = (player->skelAnime.jointTable[22].x & 0xF) - 1;
    if (eye < 0) {
        return sPose;
    }
    if (eye == 5) {
        return 2; // shock -> chuckle
    }
    if (eye >= 6) {
        return 1; // hurt -> celebrate
    }
    return 0;
}

} // namespace

// Outside the namespace: OPEN_DISPS' block-scope declaration binds to the nearest
// enclosing namespace, and in an anonymous one that symbol has no definition.
static void WalkChain(PlayState* play, StandardLimb** limbs, s32 count, s32 i, Mtx* mtx) {
    while (i != 0xFF && i < count) {
        StandardLimb* limb = limbs[i];
        Matrix_Push();
        Matrix_Translate(limb->jointPos.x, limb->jointPos.y, limb->jointPos.z, MTXMODE_APPLY);
        Matrix_RotateZYX(sTail[i].x, sTail[i].y, sTail[i].z, MTXMODE_APPLY);
        // Not MATRIX_TOMTX: it passes __FILE__ to a non-const char*.
        Matrix_ToMtx(&mtx[i], (char*)__FILE__, __LINE__);
        if (limb->dList != nullptr) {
            OPEN_DISPS(play->state.gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, limb->dList);
            CLOSE_DISPS(play->state.gfxCtx);
        }
        if (limb->child != 0xFF) {
            WalkChain(play, limbs, count, limb->child, mtx);
        }
        Matrix_Pop();
        i = limb->sibling;
    }
}

extern "C" void KeatonTails_Reset(void) {
    std::memset(sTail, 0, sizeof(sTail));
    sFrame = 0;
    sPose = 0;
}

extern "C" void KeatonTails_Update(Player* player) {
    SkeletonHeader* skel = Skel();
    if (skel == nullptr || player == nullptr) {
        return;
    }
    u8 want = PoseFor(player);
    if (want != sPose) {
        sPose = want;
        sFrame = 0;
    }
    sFrame = (sFrame + 1) % (u32)kTailClipFrames[sPose];
    const s16(*clip)[3] = kTailClip[sPose][sFrame];
    const f32 amount = 0.39f; // tuned in-game, then baked

    // Eased rather than assigned: a clip switch would otherwise snap, and the three
    // clips do not start from the same tail pose.
    for (s32 i = 0; i < skel->limbCount && i < TAIL_SEGMENTS; i++) {
        s16 tx = (s16)(clip[i][0] * amount);
        s16 ty = (s16)(clip[i][1] * amount);
        s16 tz = (s16)(clip[i][2] * amount);
        sTail[i].x += (tx - sTail[i].x) >> 2;
        sTail[i].y += (ty - sTail[i].y) >> 2;
        sTail[i].z += (tz - sTail[i].z) >> 2;
    }
}

extern "C" void KeatonTails_Draw(PlayState* play, Player* player) {
    (void)player;
    SkeletonHeader* skel = Skel();
    if (skel == nullptr || play == nullptr) {
        return;
    }
    Mtx* mtx = (Mtx*)Graph_Alloc(play->state.gfxCtx, skel->limbCount * sizeof(Mtx));
    if (mtx == nullptr) {
        return;
    }
    {
        OPEN_DISPS(play->state.gfxCtx);
        // 0x0B: 0x0D stays bound to the skeleton for the whole player draw.
        gSPSegment(POLY_OPA_DISP++, TAIL_MTX_SEGMENT, (uintptr_t)mtx);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    WalkChain(play, (StandardLimb**)skel->segment, skel->limbCount, 0, mtx);
}
