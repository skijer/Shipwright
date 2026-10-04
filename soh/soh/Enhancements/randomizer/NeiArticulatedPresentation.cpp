#include "NeiArticulatedPresentation.h"
#include "functions.h"
#include "macros.h"
#include "mods/items/helpers/equip_helper.h"

extern "C" bool NeiArticulated_UsesSwitchHook(const Player* player) {
    return player && player->heldItemId == ITEM_SWITCH_HOOK &&
           NeiHeld_HasResources(NEI_HELD_PATH("switch_hook"), nullptr) &&
           NeiHeld_HasResources(NEI_HELD_PATH("switch_hook_body"), nullptr) &&
           NeiHeld_HasResources(NEI_HELD_PATH("switch_hook_tip"), nullptr);
}

extern "C" bool NeiArticulated_ApplySwitchHookHand(PlayState* play, Player* player, Gfx** limb, Gfx* resolvedHand) {
    if (!play || !limb || !resolvedHand || !NeiArticulated_UsesSwitchHook(player)) {
        return false;
    }
    // Per-draw allocation: Link and a clone may hold different poses in one
    // graphics frame. A mutable static compound would repaint earlier draws.
    Gfx* compound = static_cast<Gfx*>(Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Gfx)));
    Gfx* command = compound;
    __gSPDisplayList(command++, resolvedHand);
    const char* item = player->heldActor != nullptr ? NEI_HELD_PATH("switch_hook") : NEI_HELD_PATH("switch_hook_body");
    gDma1p(command++, G_DL_OTR_FILEPATH, item, 0, G_DL_PUSH);
    gSPEndDisplayList(command);
    *limb = compound;
    return true;
}

extern "C" bool NeiArticulated_DrawSwitchHookTip(PlayState* play, Player* player, const Actor* hook) {
    if (!play || !hook || !NeiArticulated_UsesSwitchHook(player)) {
        return false;
    }
    // The full docked mesh is already in the hand. During both extension and
    // timer-zero retraction the actor owns the detached head; never gate on
    // the projectile timer, which would hide the returning head.
    if (player->heldActor != hook) {
        NeiHeld_DrawModel(play, NEI_HELD_PATH("switch_hook_tip"), nullptr);
    }
    return true;
}

extern "C" bool NeiArticulated_HasWhip(void) {
    return NeiHeld_HasResources(NEI_HELD_PATH("whip"), nullptr) &&
           NeiHeld_HasResources(NEI_HELD_PATH("whip_handle"), nullptr) &&
           NeiHeld_HasResources(NEI_HELD_PATH("whip_tip"), nullptr) &&
           NeiHeld_HasResources(NEI_HELD_PATH("whip_segment"), nullptr);
}

extern "C" bool NeiArticulated_DrawWhipGrip(Player* player, PlayState* play, bool coiled, Vec3f* ropeSocket) {
    if (!play || !player || !NeiArticulated_HasWhip()) {
        return false;
    }
    // The approved handle's midpoint is the native item origin. Both states
    // use the same pose, dimensions and socket, with no legacy resource cache.
    static const ItemHandPose pose = { 0, 0, 0, 0, 0, 0, 1.0f };
    Matrix_Push();
    if (!ItemEquip_ApplyHandPose(player, &pose)) {
        Matrix_Pop();
        return false;
    }
    bool drawn = NeiHeld_DrawModel(play, coiled ? NEI_HELD_PATH("whip") : NEI_HELD_PATH("whip_handle"), nullptr);
    if (drawn && ropeSocket) {
        Vec3f socket = { 0.0f, 7.2f, 0.0f };
        Matrix_MultVec3f(&socket, ropeSocket);
    }
    Matrix_Pop();
    return drawn;
}
