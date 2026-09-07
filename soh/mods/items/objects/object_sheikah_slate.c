/**
 * object_sheikah_slate.c — the Sheikah Slate in Link's hand (Skijer's NEI).
 *
 * Model and pose only; the pose is applied by ItemEquip_DrawHeldModel, off the right hand BONE.
 */

#include "z64.h"
#include "../custom_items.h"
#include "../helpers/equip_helper.h"
#include "macros.h"
#include "functions.h"
#include <math.h>

extern u8 Slate_IsDrawn(void);     // equip state, owned by item_sheikah_slate.c (same TU)
extern u8 RemoteBomb_IsHeld(void); // owned by mods/actors/remote_bomb.c (same TU)

// Dialled against the hand BONE's axes, which is why they come out this round: the bone already
// points the way the tablet wants to go, so the pose is a half turn on two axes and a drop out of
// the palm. 2ship's hand bone and model scale differ — do NOT copy these numbers across.
static const ItemHandPose sSlatePose = {
    0.0f, -8.276f, 0.0f, 180.0f, 180.0f, 0.0f, 0.1f,
};

void CustomItems_DrawSheikahSlate(Player* player, PlayState* play) {
    // Before the gate below: the ghost is world geometry, not part of the tablet, and it has to
    // keep drawing on any frame the tablet itself declines to.
    Cryonis_DrawGhost(play);

    // The remote bomb is carried in the same right hand the tablet is pinned to, so the tablet
    // steps aside while one is out rather than clipping through it.
    if (!Slate_IsDrawn() || RemoteBomb_IsHeld()) {
        return;
    }
    ItemEquip_DrawHeldModel(player, play, "__OTR__objects/object_nei_sheikah_slate/gNeiSheikahSlateDL", NULL,
                            &sSlatePose);
}
