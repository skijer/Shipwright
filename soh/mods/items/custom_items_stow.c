/**
 * Native put-away bridge for items whose in-hand state also lives outside Player.
 * Included after the item implementations in the player unity build so cleanup
 * uses the same camera, collider, trail and sound teardown as ordinary cancellation.
 */

static u8 CustomItems_CanStowWhip(void) {
    return whipActive && whipState != WHIP_STATE_SWINGING && whipState != WHIP_STATE_LAUNCHED;
}

s32 CustomItems_HasStowableHeldItem(Player* p) {
    if (p == NULL) {
        return false;
    }
    return gCustomItemState.lanternEquipped || gCustomItemState.lanternSwinging || fireRodActive ||
           fireRodFirstPerson || iceRodActive || iceRodFirstPerson || lightRodActive || lightRodFirstPerson ||
           gCustomItemState.gustJarEquipped || gCustomItemState.mogmaMittsActive ||
           gCustomItemState.ballAndChainThrown || CustomItems_CanStowWhip();
}

void CustomItems_PutAwayHeldItems(Player* p, PlayState* play) {
    if (p == NULL || play == NULL) {
        return;
    }

    Lantern_PutAway(p, play);
    FireRod_PutAway(p, play);
    IceRod_PutAway(p, play);
    LightRod_PutAway(p, play);

    if (gCustomItemState.gustJarEquipped) {
        GustJar_Unequip(play, p);
    }
    if (gCustomItemState.mogmaMittsActive || sMittsEquipState.isEquipped) {
        Mitts_OnUnequip(play, p);
        sMittsEquipState.isEquipped = 0;
    }
    if (gCustomItemState.ballAndChainThrown) {
        ItemInput_SuppressUntilRelease(ITEM_BALL_AND_CHAIN, play);
        BallChain_Stop(p, play);
    }
    if (CustomItems_CanStowWhip()) {
        ItemInput_SuppressUntilRelease(ITEM_WHIP, play);
        Whip_Stop(p, play);
    }
}
