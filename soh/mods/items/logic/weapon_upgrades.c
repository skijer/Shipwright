/**
 * weapon_upgrades.c - NEI Weapon Upgrade bit accessors.
 *
 * This translation unit just owns the Nei_Save()->weaponUpgrades bit
 * accessors so other code (randomizer give/logic, the menu, the IK Axe hammer
 * behavior) doesn't need to know the field layout.
 *
 * #included into mods/items/logic/custom_items.c (the host TU pulled in by
 * z_player.c) — the CMake mods glob only compiles *.cpp/*.h, not *.c.
 */
#include "weapon_upgrades.h"
#include "../../nei_save.h" // Skijer's NEI

u8 WeaponUpgrade_HasHammerAxe(void) {
    return (Nei_Save()->weaponUpgrades & WEAPON_UPGRADE_HAMMER_AXE) != 0;
}

u8 WeaponUpgrade_HasRazor(void) {
    return (Nei_Save()->weaponUpgrades & WEAPON_UPGRADE_KOKIRI_RAZOR) != 0;
}

u8 WeaponUpgrade_HasGilded(void) {
    return (Nei_Save()->weaponUpgrades & WEAPON_UPGRADE_KOKIRI_GILDED) != 0;
}

u8 WeaponUpgrade_HasTrueMaster(void) {
    return (Nei_Save()->weaponUpgrades & WEAPON_UPGRADE_MASTER_TRUE) != 0;
}

u8 WeaponUpgrade_HasGreatFairy(void) {
    return (Nei_Save()->weaponUpgrades & WEAPON_UPGRADE_BGS_GREAT_FAIRY) != 0;
}

u8 WeaponUpgrade_KokiriLevel(void) {
    if (Nei_Save()->weaponUpgrades & WEAPON_UPGRADE_KOKIRI_GILDED) {
        return 2;
    }
    if (Nei_Save()->weaponUpgrades & WEAPON_UPGRADE_KOKIRI_RAZOR) {
        return 1;
    }
    return 0;
}

static void WeaponUpgrade_SetBit(u8 bit, u8 on) {
    if (on) {
        Nei_Save()->weaponUpgrades |= bit;
    } else {
        Nei_Save()->weaponUpgrades &= ~bit;
    }
}

void WeaponUpgrade_SetHammerAxe(u8 on) {
    WeaponUpgrade_SetBit(WEAPON_UPGRADE_HAMMER_AXE, on);
}

void WeaponUpgrade_SetRazor(u8 on) {
    WeaponUpgrade_SetBit(WEAPON_UPGRADE_KOKIRI_RAZOR, on);
}

void WeaponUpgrade_SetGilded(u8 on) {
    WeaponUpgrade_SetBit(WEAPON_UPGRADE_KOKIRI_GILDED, on);
}

void WeaponUpgrade_SetTrueMaster(u8 on) {
    WeaponUpgrade_SetBit(WEAPON_UPGRADE_MASTER_TRUE, on);
}

void WeaponUpgrade_SetGreatFairy(u8 on) {
    WeaponUpgrade_SetBit(WEAPON_UPGRADE_BGS_GREAT_FAIRY, on);
}

void WeaponUpgrade_GiveProgressiveKokiri(void) {
    // First give → Razor, second give → Gilded. Gilded implies Razor was earned.
    if (!(Nei_Save()->weaponUpgrades & WEAPON_UPGRADE_KOKIRI_RAZOR)) {
        Nei_Save()->weaponUpgrades |= WEAPON_UPGRADE_KOKIRI_RAZOR;
    } else {
        Nei_Save()->weaponUpgrades |= WEAPON_UPGRADE_KOKIRI_GILDED;
    }
}

void WeaponUpgrade_GrantAll(void) {
    Nei_Save()->weaponUpgrades |= WEAPON_UPGRADE_ALL;
}

// ---------------------------------------------------------------------------
// Iron Knuckle's Axe prop-smash side table
//
// Lets the Axe (hammer upgrade) break gauntlet-tier props after N swings, with no per-actor
// struct changes: hit counts live in a small static table keyed by Actor*. A "swing" is counted
// at most once via a global swing id (rising edge of the player's melee state), so holding the
// hammer out doesn't rack up hits — only an actual swing in range/facing does.
// ---------------------------------------------------------------------------
#define IKAXE_PROP_SLOTS 12

typedef struct {
    Actor* actor;
    u8 hits;
    u32 lastSwing;
} IKAxePropState;

static IKAxePropState sIKAxeProps[IKAXE_PROP_SLOTS];
static u32 sIKAxeSwingId = 0;

static void IKAxe_UpdateSwingId(Player* player, PlayState* play) {
    static s32 sLastFrame = -1;
    static u8 sPrevSwinging = 0;
    if ((s32)play->gameplayFrames == sLastFrame) {
        return; // already advanced this frame (this helper is called once per prop actor)
    }
    sLastFrame = (s32)play->gameplayFrames;
    u8 swinging = (player->meleeWeaponState != 0);
    if (swinging && !sPrevSwinging) {
        sIKAxeSwingId++; // new swing
    }
    sPrevSwinging = swinging;
}

static IKAxePropState* IKAxe_PropSlot(Actor* actor) {
    s32 i;
    for (i = 0; i < IKAXE_PROP_SLOTS; i++) {
        if (sIKAxeProps[i].actor == actor) {
            return &sIKAxeProps[i];
        }
    }
    for (i = 0; i < IKAXE_PROP_SLOTS; i++) {
        if (sIKAxeProps[i].actor == NULL) {
            sIKAxeProps[i].actor = actor;
            sIKAxeProps[i].hits = 0;
            sIKAxeProps[i].lastSwing = 0;
            return &sIKAxeProps[i];
        }
    }
    return NULL;
}

// In-hand (held) sword model swap — pak_loader-style: keep the OOT/pak hand and draw the MM
// SWORD PIECES (blade + handle) from o2r on top. We can't use MM's combined LeftHandHolding*Sword
// DL because its object_link_child hand collides with OOT's object_link_child (which has no Razor/
// Gilded blade) → it renders empty. The standalone piece DLs are MM-specific, so their sub-
// resources resolve to mm.o2r and render. Builds a compound DL [hand][blade][handle] like the
// Twilight clawshot. `ootHand` is the resolved OOT hand DL for this limb/LOD; returns 1 if it set
// *dList (caller leaves the vanilla DL otherwise).
u8 WeaponUpgrade_ApplyHeldSwordDL(Gfx** dList, void* ootHand, Player* player, u8 bodyEnvR, u8 bodyEnvG, u8 bodyEnvB) {
    extern void* MmAssets_LoadResource(const char* path);
    extern s32 CVarGetInteger(const char* name, s32 defaultValue);
    // Cached loaded pieces per variant.
    static void* sRazorBlade = NULL;
    static void* sRazorHandle = NULL;
    static void* sGildedBlade = NULL;
    static void* sGildedHandle = NULL;
    static void* sGfs = NULL;
    static u8 sRazorTried = 0, sGildedTried = 0, sGfsTried = 0;
    static Gfx sCompound[8];

    if (dList == NULL || ootHand == NULL || player == NULL) {
        return 0;
    }

    void* blade = NULL;
    void* handle = NULL;

    // Four Sword first: it replaces whichever sword is in hand while equipped, so it outranks the
    // per-sword upgrade blades below. Its DLs come from soh.o2r (converted out of the old pak), not
    // from mm.o2r. Defined in equipment/behaviors/equip_foursword.c.
    extern u8 FourSword_HeldSwordDL(void** blade, void** handle);
    if ((player->heldItemAction >= PLAYER_IA_SWORD_MASTER && player->heldItemAction <= PLAYER_IA_SWORD_BIGGORON) &&
        FourSword_HeldSwordDL(&blade, &handle)) {
        // fall through to the compound-DL builder with the Four Sword pieces
    } else if (player->heldItemAction == PLAYER_IA_SWORD_KOKIRI && WeaponUpgrade_KokiriLevel() >= 1) {
        u8 gilded = WeaponUpgrade_HasGilded() && CVarGetInteger("gEnhancements.SkijerNEI.GildedUsesGildedLook", 1);
        if (gilded) {
            if (!sGildedTried) {
                sGildedTried = 1;
                sGildedBlade = MmAssets_LoadResource("__OTR__objects/object_link_child/gLinkHumanGildedSwordBladeDL");
                sGildedHandle = MmAssets_LoadResource("__OTR__objects/object_link_child/gLinkHumanGildedSwordHandleDL");
            }
            blade = sGildedBlade;
            handle = sGildedHandle;
        } else {
            if (!sRazorTried) {
                sRazorTried = 1;
                sRazorBlade = MmAssets_LoadResource("__OTR__objects/gameplay_keep/gRazorSwordBladeDL");
                sRazorHandle = MmAssets_LoadResource("__OTR__objects/gameplay_keep/gRazorSwordHandleDL");
            }
            blade = sRazorBlade;
            handle = sRazorHandle;
        }
    } else if (player->heldItemAction == PLAYER_IA_SWORD_BIGGORON && WeaponUpgrade_HasGreatFairy() &&
               CVarGetInteger("gEnhancements.SkijerNEI.BgsUsesGfsLook", 1)) {
        if (!sGfsTried) {
            sGfsTried = 1;
            sGfs = MmAssets_LoadResource("__OTR__objects/object_link_child/gLinkHumanGreatFairysSwordDL");
        }
        blade = sGfs; // single combined blade+hilt DL
        handle = NULL;
    } else {
        return 0;
    }

    if (blade == NULL) {
        return 0; // asset unavailable → keep vanilla DL
    }

    // Build [MM blade] + [MM handle?] + [OOT hand] + [end] — sword first, hand LAST. The hand DL
    // restores the vanilla skin material/render state, so the next limb (the torso) doesn't inherit
    // the MM sword's combiner/env and render black. Matches "sword luego la mano".
    Gfx* d = sCompound;
    gSPDisplayList(d++, (Gfx*)blade);
    if (handle != NULL) {
        gSPDisplayList(d++, (Gfx*)handle);
    }
    gSPDisplayList(d++, ootHand);
    // Restore the standard player-limb render state so the next limb (the torso) doesn't inherit
    // the MM sword's material and render black.
    gDPPipeSync(d++);
    // Re-apply the tunic env color. The player body tints with env (set once at z_player_lib.c:1181),
    // and a combined MM DL like the Great Fairy's Sword sets its OWN env color and never restores it —
    // without this the torso inherits the GFS env and the chest renders black.
    gDPSetEnvColor(d++, bodyEnvR, bodyEnvG, bodyEnvB, 0);
    gSPLoadGeometryMode(d++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    gSPEndDisplayList(d);
    *dList = sCompound;
    return 1;
}

u8 WeaponUpgrade_IKAxeStrike(Actor* actor, PlayState* play, u8 hitsNeeded, f32 range) {
    // The Power Keg's explosion one-shots these same gauntlet-tier props, bypassing the hammer-swing
    // requirement (it's a blast, not a swing). PowerKeg_BlastQuery lives in power_keg.c — same unity
    // TU, #included later, so a local extern is enough. Skijer's NEI
    extern u8 PowerKeg_BlastQuery(Actor * actor, PlayState * play);
    if ((actor != NULL) && (play != NULL) && PowerKeg_BlastQuery(actor, play)) {
        return 1;
    }
    if (!WeaponUpgrade_HasHammerAxe() || actor == NULL || play == NULL) {
        return 0;
    }
    Player* player = GET_PLAYER(play);
    IKAxe_UpdateSwingId(player, play);

    // Must be actively swinging the hammer (the Axe).
    if (player->heldItemAction != PLAYER_IA_HAMMER || player->meleeWeaponState == 0) {
        return 0;
    }
    // In reach and roughly in front of the player.
    if (Math_Vec3f_DistXYZ(&player->actor.world.pos, &actor->world.pos) > range) {
        return 0;
    }
    s16 yawToActor = Actor_WorldYawTowardActor(&player->actor, actor);
    if (ABS((s16)(yawToActor - player->actor.shape.rot.y)) > 0x3800) {
        return 0;
    }

    IKAxePropState* slot = IKAxe_PropSlot(actor);
    if (slot == NULL || slot->lastSwing == sIKAxeSwingId) {
        return 0; // table full, or this swing already counted for this actor
    }
    slot->lastSwing = sIKAxeSwingId;
    slot->hits++;

    Audio_PlaySoundGeneral(NA_SE_IT_HAMMER_HIT, &actor->world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    CollisionCheck_SpawnShieldParticlesMetal(play, &actor->world.pos);

    if (slot->hits >= hitsNeeded) {
        slot->actor = NULL; // free the slot
        return 1;
    }
    return 0;
}
