/**
 * equip_climb_boots.c - Climb Boots (Extended Boots Slot 2)
 *
 * Takes over the slot the Pendant of Memories used to squat on. The Pendant is NOT gone: it lives
 * on the equipment page's left column (ownership = the adult trade wheel, TRADE_ADULT_PENDANT) and
 * its moveset is dispatched cheat-independently, so this slot is free for a real pair of boots.
 *
 * BEHAVIOR: full traction. While equipped, Link grips every floor —
 *   - ice (sFloorType 5) stops skating: same exemption the Iron Boots get, so he walks normally;
 *   - steep slopes (floor effect 1) neither force Player_Action_SlideOnSlope nor slow the climb,
 *     and A can jump off them like on flat ground.
 * The behavior is all gates in z_player.c keyed on ClimbBoots_HasGrip(); nothing runs per frame.
 *
 * Included by ext_equip_behavior.c (unity build).
 */

// Grip predicate for the z_player.c gates (this file is part of the z_player TU).
u8 ClimbBoots_HasGrip(void) {
    return ExtEquip_IsEnabled() && (gExtEquipState.currentExtBoots == 2);
}

// Per-frame behavior while the Climb Boots are the equipped ext boots.
static void ClimbBoots_Behavior(Player* player, PlayState* play) {
    (void)player;
    (void)play;
}

// Called when the Climb Boots are unequipped (restore anything the behavior forced).
static void ClimbBoots_Cleanup(void) {
}
