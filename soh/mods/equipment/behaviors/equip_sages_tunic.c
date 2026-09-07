/* The Sage's Tunic grants passive resistances from owned medallions. */

// Slot change: a leftover medallion dye must not replay the next time the tunic goes on.
static void Sages_Cleanup(void) {
    ExtEquip_SagesFlashReset();
}

static void Sages_Behavior(Player* player, PlayState* play) {
    (void)player;
    (void)play;

    ExtEquip_SagesFlashTick();
}
