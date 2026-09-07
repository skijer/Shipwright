/**
 * ext_equip_behavior.c - Behavior handlers for extended equipment
 *
 * Unity build hub: includes individual behavior files and dispatches
 * update/draw/hit callbacks to active equipment.
 *
 * Included by extended_equipment.c (unity build).
 */

// No extra includes — inherits all from extended_equipment.c (unity build root)
// Somaria cane DL header included by extended_equipment.c (unity root)

// ---------------------------------------------------------------------------
// Include behavior implementations
// ---------------------------------------------------------------------------
// Before equip_byrna.c: the Byrna's R press hands off to the glaive's aerial entry.
#include "behaviors/byrna_ig_combat.inc.c"
#include "behaviors/equip_byrna.c"
#include "behaviors/equip_ikaxe.c"
#include "behaviors/equip_pegasus.c"
#include "behaviors/equip_dragonscale.c"
#include "behaviors/equip_ikana.c"
#include "behaviors/equip_magiccape.c"
#include "behaviors/equip_breastplate.c"
#include "behaviors/equip_pendant.c"
#include "behaviors/equip_divine_shield.c"
#include "behaviors/equip_champion.c"
#include "behaviors/equip_sages_tunic.c"
// Before equip_foursword.c: it calls straight into the clone actor's API, which ships no header.
#include "actors/four_sword_clone.c"
#include "behaviors/equip_foursword.c"
// Skijer 2026-07-29 kaleido re-layout: the four slots that changed hands.
#include "behaviors/equip_trident.c"     // sword 3 (was the Iron Knuckle's Axe, now the Hammer upgrade)
#include "behaviors/equip_kite_shield.c" // shield 2 (was the Gerudo Scimitar placeholder)
#include "behaviors/equip_climb_boots.c" // boots 2 (was the Pendant of Memories)
#include "behaviors/equip_roc_boots.c"   // boots 3 (was the deleted Water Dragon Scale)

// ---------------------------------------------------------------------------
// Sword behaviors
// ---------------------------------------------------------------------------
static void ExtEquip_Behavior_Sword1(Player* player, PlayState* play) {
    Byrna_Behavior(player, play);
    ByrnaIg_Behavior(player, play);
}

static void ExtEquip_Behavior_Sword2(Player* player, PlayState* play) {
    FourSword_Behavior(player, play);
}

static void ExtEquip_Behavior_Sword3(Player* player, PlayState* play) {
    // The Iron Knuckle's Axe left this slot for good — it is the HAMMER UPGRADE now, driven from
    // ExtEquip_UpdateBehavior via WeaponUpgrade_HasHammerAxe(). The slot holds the TRIDENT.
    Trident_Behavior(player, play);
}

// ---------------------------------------------------------------------------
// Shield behaviors (stubs)
// ---------------------------------------------------------------------------
static void ExtEquip_Behavior_Shield1(Player* player, PlayState* play) {
    DivineShield_Behavior(player, play);
}

static void ExtEquip_Behavior_Shield2(Player* player, PlayState* play) {
    KiteShield_Behavior(player, play);
}

static void ExtEquip_Behavior_Shield3(Player* player, PlayState* play) {
    Ikana_Behavior(player, play);
}

// ---------------------------------------------------------------------------
// Tunic behaviors (stubs)
// ---------------------------------------------------------------------------
// Tunic slots remapped (Skijer 2026-07-16): 1 = Champion's Tunic, 2 = Spirit Tunic, 3 = Sage's.
static void ExtEquip_Behavior_Tunic1(Player* player, PlayState* play) {
    Champion_Behavior(player, play);
}

static void ExtEquip_Behavior_Tunic2(Player* player, PlayState* play) {
    Spirit_Behavior(player, play); // MAGIC TUNIC: rupee-paid damage immunity + fire/water timer skip
}

static void ExtEquip_Behavior_Tunic3(Player* player, PlayState* play) {
    Sages_Behavior(player, play);
}

// ---------------------------------------------------------------------------
// Boots behaviors
// ---------------------------------------------------------------------------
static void ExtEquip_Behavior_Boots1(Player* player, PlayState* play) {
    Pegasus_Behavior(player, play);
}

static void ExtEquip_Behavior_Boots2(Player* player, PlayState* play) {
    // The Pendant of Memories keeps its left-column cell (ownership = the adult trade wheel) and its
    // moveset is dispatched cheat-independently; this GRID slot is the CLIMB BOOTS.
    ClimbBoots_Behavior(player, play);
}

static void ExtEquip_Behavior_Boots3(Player* player, PlayState* play) {
    // The Water Dragon Scale is deleted (its Zora swim is the Zora Tunic's permanent effect); this
    // slot is the ROC BOOTS.
    RocBoots_Behavior(player, play);
}

// ---------------------------------------------------------------------------
// Behavior dispatch tables
// ---------------------------------------------------------------------------
typedef void (*ExtEquipBehaviorFunc)(Player*, PlayState*);

static const ExtEquipBehaviorFunc sExtSwordBehaviors[3] = {
    ExtEquip_Behavior_Sword1,
    ExtEquip_Behavior_Sword2,
    ExtEquip_Behavior_Sword3,
};

static const ExtEquipBehaviorFunc sExtShieldBehaviors[3] = {
    ExtEquip_Behavior_Shield1,
    ExtEquip_Behavior_Shield2,
    ExtEquip_Behavior_Shield3,
};

static const ExtEquipBehaviorFunc sExtTunicBehaviors[3] = {
    ExtEquip_Behavior_Tunic1,
    ExtEquip_Behavior_Tunic2,
    ExtEquip_Behavior_Tunic3,
};

static const ExtEquipBehaviorFunc sExtBootsBehaviors[3] = {
    ExtEquip_Behavior_Boots1,
    ExtEquip_Behavior_Boots2,
    ExtEquip_Behavior_Boots3,
};

// Cleanup of the piece leaving a slot. Called synchronously from ExtEquip_SetSlot — the ONLY
// caller — so a switch never leaves the old behavior's state (timers, colliders, forced player
// flags, anim tables) behind for a frame, and every slot has an entry.
static void ExtEquip_CleanupSlot(s16 equipType, u8 index) {
    switch (equipType) {
        case EQUIP_TYPE_SWORD:
            if (index == 1) {
                Byrna_Cleanup();
            } else if (index == 2) {
                FourSword_Cleanup();
            } else if (index == 3) {
                Trident_Cleanup();
            }
            break;
        case EQUIP_TYPE_SHIELD:
            if (index == 1) {
                DivineShield_Cleanup();
            } else if (index == 2) {
                KiteShield_Cleanup();
            } else if (index == 3) {
                Ikana_Cleanup();
            }
            break;
        case EQUIP_TYPE_TUNIC:
            if (index == 1) {
                if (gPlayState != NULL) {
                    Champion_Cleanup(gPlayState);
                }
            } else if (index == 2) {
                Breastplate_Cleanup();
            } else if (index == 3) {
                Sages_Cleanup();
            }
            break;
        case EQUIP_TYPE_BOOTS:
            if (index == 1) {
                Pegasus_Cleanup();
            } else if (index == 2) {
                ClimbBoots_Cleanup();
            } else if (index == 3) {
                RocBoots_Cleanup();
            }
            break;
    }
}

static void ExtEquip_DispatchBehavior(Player* player, PlayState* play) {
    // Upgrade-column passives and the Zora Tunic swim run cheat-independently
    // from ExtEquip_UpdateBehavior.

    if (gExtEquipState.currentExtSword > 0 && gExtEquipState.currentExtSword <= 3) {
        sExtSwordBehaviors[gExtEquipState.currentExtSword - 1](player, play);
    }
    if (gExtEquipState.currentExtShield > 0 && gExtEquipState.currentExtShield <= 3) {
        sExtShieldBehaviors[gExtEquipState.currentExtShield - 1](player, play);
    }
    if (gExtEquipState.currentExtTunic > 0 && gExtEquipState.currentExtTunic <= 3) {
        sExtTunicBehaviors[gExtEquipState.currentExtTunic - 1](player, play);
    }
    if (gExtEquipState.currentExtBoots > 0 && gExtEquipState.currentExtBoots <= 3) {
        sExtBootsBehaviors[gExtEquipState.currentExtBoots - 1](player, play);
    }
}

// ---------------------------------------------------------------------------
// Melee hit dispatch (called from z_player.c)
// ---------------------------------------------------------------------------
static void ExtEquip_OnMeleeHitDispatch(Player* player, PlayState* play) {
    // (The Cane of Byrna is a dummy slot now — its HP/MP-on-hit recovery belongs to the Great Fairy's
    // Sword, dispatched from ExtEquip_UpdateBehavior.)
    // Trident
    if (gExtEquipState.currentExtSword == 3) {
        Trident_OnMeleeHit(player, play);
    }
    // Champion's Tunic: count hits during Flurry Rush window (slot 1 now)
    if (gExtEquipState.currentExtTunic == 1) {
        Champion_OnMeleeHit(player, play);
    }
}

// ---------------------------------------------------------------------------
// Draw dispatch (called from z_player.c draw section)
// ---------------------------------------------------------------------------
static void ExtEquip_DrawDispatch(Player* player, PlayState* play) {
    // Cane of Byrna: drawn from PostLimbDraw via ExtEquip_DrawSwordDL (follows limb matrix)
    // Pegasus Anklet: wind barrier
    if (gExtEquipState.currentExtBoots == 1) {
        Pegasus_Draw(player, play);
    }
    // Zora Tunic and Magic Cape visuals are dispatched cheat-independently.
    // The Four Sword clones are real actors now — they draw themselves.
    // Trident: Ganondorf's light ball growing on the lance tip while the charge runs
    if (gExtEquipState.currentExtSword == 3) {
        Trident_Draw(player, play);
    }
    // Byrna: the spiral trail that marks a full Kinsect charge
    if (gExtEquipState.currentExtSword == 1) {
        ByrnaOrb_Draw(play);
    }
}
