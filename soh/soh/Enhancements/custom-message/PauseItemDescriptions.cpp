/**
 * PauseItemDescriptions.cpp - C-Up item descriptions in pause menu
 *
 * When the player presses C-Up while hovering over a custom item/equipment/mask
 * in the pause menu, a short utility-focused description textbox is displayed.
 */

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "z64item.h"
#include "macros.h"
#include "variables.h"
#include "mods/extended_equipment.h"
#include "expansions/sw97/sw97_config.h"
#include "mods/extended_inventory.h" // Sw97_EffectiveElement / Wand_GetMode (Skijer's NEI)
}

// ---------------------------------------------------------------------------
// Description table: { itemId, textId, description }
// ---------------------------------------------------------------------------

struct ItemDescEntry {
    u16 itemId;
    u16 textId;
    const char* desc;
};

static const ItemDescEntry sCustomItemDescs[] = {
    { ITEM_ROCS_FEATHER_SKIJER, TEXT_DESC_ROCS_FEATHER, "Jump in ground and small jump from water." },
    { ITEM_ROCS_CAPE, TEXT_DESC_ROCS_CAPE, "Jump from ground or water. Press again&in the air for a double jump." },
    { ITEM_HYLIAS_GRACE, TEXT_DESC_HYLIAS_GRACE,
      "Fairy flight for 10s. Ignores walls.&A=up, B=down, L=sprint. 24 MP." },
    { ITEM_ZONAI_PERMAFROST, TEXT_DESC_ZONAI_PERMAFROST,
      "Toggle the time stop. 4 MP to start,&then 1 MP every 10 frames. Ends on&a second press or an empty meter." },
    { ITEM_DEMISE_DESTRUCTION, TEXT_DESC_DEMISE_DESTRUCTION,
      "Massive AoE explosion. Damages all&enemies in range. Ground only. 12 MP." },
    { ITEM_DEKU_LEAF, TEXT_DESC_DEKU_LEAF, "Ground: blow wind gust. Air: hold&to glide. Drains magic while gliding." },
    { ITEM_SWITCH_HOOK, TEXT_DESC_SWITCH_HOOK, "Aim and fire to swap positions&with objects and enemies." },
    { ITEM_MOGMA_MITTS, TEXT_DESC_MOGMA_MITTS, "Toggle to climb any wall.&Drains magic over time." },
    { ITEM_GUST_JAR, TEXT_DESC_GUST_JAR,
      "Hold C to suck things in, release to&fire them back. Hold C 20 frames while&idle for the element wheel." },
    { ITEM_BALL_AND_CHAIN, TEXT_DESC_BALL_AND_CHAIN,
      "Heavy thrown weapon. Breaks ice walls&and heavy objects. Hold C to charge.&C-Up to aim." },
    { ITEM_WHIP, TEXT_DESC_WHIP, "Grapple from any bar surface. Swing&with joystick. Release for momentum&launch." },
    { ITEM_SPINNER, TEXT_DESC_SPINNER,
      "Hold C to charge, release to ride.&Release while Z-targeting for a&homing dash. Breaks rocks." },
    { ITEM_CANE_OF_SOMARIA, TEXT_DESC_CANE_OF_SOMARIA,
      "Four canes on one cell. A here cycles&the cane; C draws it, then casts.&L and R step the summon." },
    { ITEM_DOMINION_ROD, TEXT_DESC_DOMINION_ROD,
      "Fire orb to possess Beamos, Armos&or Anubis. Control them with analog+C." },
    { ITEM_TIME_GATE, TEXT_DESC_TIME_GATE, "Travel through time. Swap between&child and adult. Costs 48 magic." },
    // ITEM_BOMB_ARROWS moved to sSw97ElemDescs — it owns no inventory cell any more, so it can only
    // be hovered as the bow's primed element.
    // No ITEM_ELEMENTAL_WAND row: this table is searched before the mode block below, so a generic
    // row here would shadow all six of sWandModeDescs.
    { ITEM_ROD_FIRE, TEXT_DESC_FIRE_ROD,
      "Slash=3 fireballs. Stab=long shot.&Jump=flamethrower. Spin=fire AoE.&C-Up to aim." },
    { ITEM_ROD_ICE, TEXT_DESC_ICE_ROD, "Slash=3 iceballs. Stab=long shot.&Jump=ice wave. Spin=ice AoE.&C-Up to aim." },
    { ITEM_ROD_LIGHT, TEXT_DESC_LIGHT_ROD, "Slash=3 orbs. Stab=long shot.&Jump=beam. Spin=light AoE.&C-Up to aim." },
    { ITEM_BEETLE, TEXT_DESC_BEETLE,
      "Hold C to aim, release to launch.&Stick steers, A boosts, Z locks on,&B lets it fly home on its own." },
    { ITEM_SHOVEL, TEXT_DESC_SHOVEL, "Dig to uncover grottos, Gold&Skulltulas and graveyard rewards." },
    { ITEM_MINISH_CAP, TEXT_DESC_MINISH_CAP,
      "C by a pod soil: fast travel map.&C away from one: shrink or grow back.&Gold Skulltulas unlock the soils." },
    { ITEM_LANTERN, TEXT_DESC_LANTERN,
      "Swing near fire to catch it. 4 types.&Blue=melts red ice. Green=HP regen.&Poe/Green=free Lens. Swing=fire "
      "dmg." },
    { ITEM_CHATEAU_ROMANI, TEXT_DESC_CHATEAU_ROMANI, "Drink for infinite magic.&One-time consumable." },
    { ITEM_POKEBALL, TEXT_DESC_POKEBALL, "Transform into Pikachu.&Press again to revert." },
    // Page-2 cells (2026-08-06 re-layout)
    { EXT_ITEM_SHEIKAH_SLATE, TEXT_DESC_SHEIKAH_SLATE,
      "C draws the slate, then casts the&active rune. Hold L for the rune wheel." },
    { EXT_ITEM_PHANTOM_HOURGLASS, TEXT_DESC_PHANTOM_HOURGLASS,
      "C stops time and aims. C again rewinds&what the reticle holds along its own&path. C or B lets go." },
    { EXT_ITEM_SHADOW_CRYSTAL, TEXT_DESC_SHADOW_CRYSTAL,
      "Turn into Wolf Link. Bite combo&and a running dash. Press again&to turn back." },
    { EXT_ITEM_ROD_OF_SEASONS, TEXT_DESC_ROD_OF_SEASONS,
      "C draws the rod, then opens the season&prompt. A confirms, B cancels, and a&new season reloads the scene." },
    // Bottle row
    { ITEM_NET, TEXT_DESC_NET, "Swing it like a sword to scoop bugs,&fish and fairies into a bottle." },
    { ITEM_BOTTOMLESS_BOTTLE, TEXT_DESC_BOTTOMLESS_BOTTLE, "Refills itself for a set number of&uses per fill." },
    { ITEM_MAGIC_MUSHROOM, TEXT_DESC_MAGIC_MUSHROOM,
      "Sniffed out with the Mask of Scents.&Bottle it before it spoils." },
    // Bomb-cell wheel
    { ITEM_POWER_KEG, TEXT_DESC_POWER_KEG, "Goron blasting keg. Clears boulders&and heavy blocks." },
    // Trade wheel: the same id is the Climb Boots on the page-2 equipment grid (sExtEquipDescs).
    { ITEM_EXT_BOOTS_2, TEXT_DESC_EXT_PENDANT_MEMORIES,
      "Three extra B moves: Mortal Draw,&Ground Pound and Parry Leap." },
};

static const ItemDescEntry sMaskDescs[] = {
    { ITEM_MM_MASK_ALL_NIGHT, TEXT_DESC_MASK_ALL_NIGHT, "Spawns night-only Gold Skulltulas&during daytime." },
    { ITEM_MM_MASK_BLAST, TEXT_DESC_MASK_BLAST, "Press B for instant explosion at&your feet. Has cooldown." },
    { ITEM_MM_MASK_STONE, TEXT_DESC_MASK_STONE, "Enemies ignore you completely." },
    { ITEM_MM_MASK_GREAT_FAIRY, TEXT_DESC_MASK_GREAT_FAIRY,
      "In fountain: A=claim reward.&B=teleport menu between fountains." },
    { ITEM_MM_MASK_DEKU, TEXT_DESC_MASK_DEKU, "Transform into Deku form.&Full moveset from Majora's Mask." },
    { ITEM_MM_MASK_BUNNY, TEXT_DESC_MASK_BUNNY, "Run 1.5x faster." },
    { ITEM_MM_MASK_DON_GERO, TEXT_DESC_MASK_DON_GERO, "At Zora's River frog log: A=collect&all frog rewards at once." },
    { ITEM_MM_MASK_GORON, TEXT_DESC_MASK_GORON, "Transform into Goron form.&Full moveset from Majora's Mask." },
    { ITEM_MM_MASK_ROMANI, TEXT_DESC_MASK_ROMANI, "Get milk from cows without&Epona's Song." },
    { ITEM_MM_MASK_COUPLE, TEXT_DESC_MASK_COUPLE, "Passive regen. Day=HP recovery.&Night=MP recovery." },
    { ITEM_MM_MASK_ZORA, TEXT_DESC_MASK_ZORA, "Transform into Zora form.&Full moveset from Majora's Mask." },
    { ITEM_MM_MASK_KAMARO, TEXT_DESC_MASK_KAMARO, "Hold A to dance. Dance near&Darunia for reward." },
    { ITEM_MM_MASK_CAPTAIN, TEXT_DESC_MASK_CAPTAIN,
      "Spawns Stalchildren (child) or Stalfos&(adult) at night in Hyrule Field." },
    { ITEM_MM_MASK_FIERCE_DEITY, TEXT_DESC_MASK_FIERCE_DEITY,
      "Transform into Fierce Deity form.&Full moveset from Majora's Mask." },
    { ITEM_MM_MASK_BREMEN, TEXT_DESC_MASK_BREMEN,
      "Hold B to march to Bremen music.&Keep marching and a cucco appears." },
    { ITEM_MM_MASK_CIRCUS_LEADER, TEXT_DESC_MASK_CIRCUS_LEADER,
      "Minigame owners mistake you for&the king's tax man and pay up." },
    { ITEM_MM_MASK_GIANT, TEXT_DESC_MASK_GIANT, "Grow enormous until your&magic runs out." },
    { ITEM_MM_MASK_GIBDO, TEXT_DESC_MASK_GIBDO, "ReDeads and Gibdos dance&instead of grabbing you." },
    { ITEM_MM_MASK_POSTMAN, TEXT_DESC_MASK_POSTMAN,
      "Warp between the Hyrule mailboxes&you have walked up to at least once." },
    { ITEM_MM_MASK_SCENTS, TEXT_DESC_MASK_SCENTS,
      "Sniff out five hidden mushrooms in&the Lost Woods and bottle them." },
    { ITEM_MM_MASK_TRUTH, TEXT_DESC_MASK_TRUTH, "Does nothing yet." },
    { ITEM_MM_MASK_GARO, TEXT_DESC_MASK_GARO, "Transform into Garo form." },
    { ITEM_MM_MASK_KAFEI, TEXT_DESC_MASK_KAFEI, "Transform into Kafei form." },
    { ITEM_MM_MASK_KEATON, TEXT_DESC_MASK_KEATON, "Transform into Keaton form." },
};

// Keyed by SW97_ELEM_*, NOT by item id — the elemental shot has no item id any more, it is a flag on
// the bow/slingshot. The old strings advertised a magic cost; medallion shots are free.
static const ItemDescEntry sSw97ElemDescs[] = {
    { SW97_ELEM_FIRE, TEXT_DESC_SW97_ARROW_FIRE, "Fire elemental shot. Costs no magic." },
    { SW97_ELEM_ICE, TEXT_DESC_SW97_ARROW_ICE, "Ice elemental shot. Costs no magic." },
    { SW97_ELEM_LIGHT, TEXT_DESC_SW97_ARROW_LIGHT, "Light elemental shot. Costs no magic." },
    { SW97_ELEM_DARK, TEXT_DESC_SW97_ARROW_DARK, "Dark elemental shot. Costs no magic." },
    { SW97_ELEM_SOUL, TEXT_DESC_SW97_ARROW_SOUL, "Soul elemental shot. Costs no magic." },
    { SW97_ELEM_WIND, TEXT_DESC_SW97_ARROW_WIND, "Wind elemental shot. Costs no magic." },
    { SW97_ELEM_BOMB, TEXT_DESC_BOMB_ARROWS,
      "Hold C to aim, release to fire. Costs&1 arrow and 1 bomb. Holding past 70&frames drops a live bomb instead." },
};

// The six rods share one item id, so their descriptions key off the active mode.
static const ItemDescEntry sWandModeDescs[] = {
    { WAND_MODE_SAND, TEXT_DESC_WAND_SAND, "Sand Rod. Unlocked by the Spirit&Medallion." },
    { WAND_MODE_TORNADO, TEXT_DESC_WAND_TORNADO, "Tornado Rod. Unlocked by the Forest&Medallion." },
    { WAND_MODE_WATER, TEXT_DESC_WAND_WATER, "Water Rod. Unlocked by the Water&Medallion." },
    { WAND_MODE_METEOR, TEXT_DESC_WAND_METEOR, "Meteor Rod. Unlocked by the Fire&Medallion." },
    { WAND_MODE_STORM, TEXT_DESC_WAND_STORM, "Storm Rod. Unlocked by the Light&Medallion." },
    { WAND_MODE_SCEPTER, TEXT_DESC_WAND_SCEPTER, "Shadow Scepter. Unlocked by the&Shadow Medallion." },
};

// Skijer 2026-07-29 re-layout. The TEXT_DESC_* ids are kept as-is (they are just message slots) even
// where a slot changed item, so no message table has to be renumbered.
//   NOTE ITEM_EXT_BOOTS_2 is the one shared id: in the INVENTORY / trade wheel it is the Pendant of
//   Memories (described here), while the page-2 GRID cell with the same id is the Climb Boots.
static const ItemDescEntry sExtEquipDescs[] = {
    { ITEM_EXT_SWORD_1, TEXT_DESC_EXT_BYRNA,
      "Glaive: B chains, fwd+B thrusts,&Z+A jumps, R sends the Kinsect.&In the air A dashes, R pounds." },
    { ITEM_EXT_SWORD_2, TEXT_DESC_EXT_FOUR_SWORD,
      "Hold R+B for 3 clones that mirror&your attacks, 12 MP each.&Hold L for the formation wheel." },
    { ITEM_EXT_SWORD_3, TEXT_DESC_EXT_TRIDENT,
      "Gunlance: B chains, hold B charges,&R+B guard dashes, hold R+A flies." },
    { ITEM_EXT_SHIELD_1, TEXT_DESC_EXT_DIVINE_SHIELD,
      "Fire immune. Block within 10 frames&to stun all nearby enemies." },
    { ITEM_EXT_SHIELD_2, TEXT_DESC_EXT_GERUDO_SCIMITAR,
      "R in mid-air to surf. Downhill&builds speed. A hops, B spins, B+R off." },
    { ITEM_EXT_SHIELD_3, TEXT_DESC_EXT_SHIELD_IKANA,
      "Perfect guard drains enemy HP.&Death save: revive once with 3 hearts." },
    { ITEM_EXT_TUNIC_1, TEXT_DESC_EXT_CHAMPION_TUNIC,
      "Dodge past an attack for a Flurry&Rush, aim in mid-air for Bullet Time.&Both slow the world to 33%." },
    { ITEM_EXT_TUNIC_2, TEXT_DESC_EXT_BREASTPLATE,
      "Rupees absorb damage, 1 HP each, and&the fire and water timers stop.&At zero you are slow and unprotected." },
    { ITEM_EXT_TUNIC_3, TEXT_DESC_EXT_SAGES_TUNIC, "Each medallion you own adds a&passive resistance while worn." },
    { ITEM_EXT_BOOTS_1, TEXT_DESC_EXT_PEGASUS_ANKLET, "Keep holding B after a swing to&charge forward, sword first." },
    { ITEM_EXT_BOOTS_2, TEXT_DESC_EXT_CLIMB_BOOTS,
      "Full traction. Ice stops being&slippery and steep slopes stop&sliding you." },
    { ITEM_EXT_BOOTS_3, TEXT_DESC_EXT_ROC_BOOTS, "Water and lava become solid&ground. You fall at half speed." },
};

static const ItemDescEntry sMedallionDescs[] = {
    { ITEM_MEDALLION_FOREST, TEXT_DESC_MEDALLION_FOREST,
      "Wind spell, 12 MP. A tornado that&drags enemies in and grinds them.&C here equips the spell." },
    { ITEM_MEDALLION_FIRE, TEXT_DESC_MEDALLION_FIRE,
      "Fire spell, 12 MP. A column of flame&that burns harder the longer it&stands. C here equips the spell." },
    { ITEM_MEDALLION_WATER, TEXT_DESC_MEDALLION_WATER,
      "Ice spell, 24 MP. Freezes every enemy&it touches for 6 seconds.&C here equips the spell." },
    { ITEM_MEDALLION_SPIRIT, TEXT_DESC_MEDALLION_SPIRIT,
      "Soul spell, 24 MP. Turns you into a&fairy until you cast it again.&C here equips the spell." },
    { ITEM_MEDALLION_SHADOW, TEXT_DESC_MEDALLION_SHADOW,
      "Dark spell, 12 MP. A shield that blocks&all damage for a minute while the&world dims. C here equips the "
      "spell." },
    { ITEM_MEDALLION_LIGHT, TEXT_DESC_MEDALLION_LIGHT,
      "Light spell, 24 MP. Undead freeze for&30 seconds and you heal 6 hearts.&C here equips the spell." },
};

// Boss remains, read on the NEI MM quest page. That page has no item ids of its own: it publishes
// `0x100 + cursor point` as the hovered item, and points 0-3 are the four remains.
#define MM_QUEST_POINT_ITEM(point) (0x100 + (point))

static const ItemDescEntry sBossRemainsDescs[] = {
    { MM_QUEST_POINT_ITEM(0), TEXT_DESC_REMAINS_ODOLWA,
      "Press its button to wear it.&Hold A to sprint, trailing fire.&R+B calls 6 beetles (6 MP).&A by soft soil "
      "takes off on moths." },
    { MM_QUEST_POINT_ITEM(1), TEXT_DESC_REMAINS_GOHT,
      "Press its button to wear it.&Hold A to charge like a bull, R+A to&ground-pound. Hold B for a thunder&bolt "
      "(4 MP). R+B throws a bombchu." },
    { MM_QUEST_POINT_ITEM(2), TEXT_DESC_REMAINS_GYORG,
      "Press its button to wear it.&Swim like a Zora. In water R calls a&fish school and B holds a whirlpool;&on "
      "land R+B calls the fish." },
    { MM_QUEST_POINT_ITEM(3), TEXT_DESC_REMAINS_TWINMOLD,
      "Press its button to wear it.&Its Dark Link companion is not&implemented yet." },
};

// Vanilla OOT usable items (shown on the ITEM page when no custom item matches).
static const ItemDescEntry sVanillaItemDescs[] = {
    { ITEM_STICK, TEXT_DESC_V_STICK, "Deku Stick. Melee weapon that&lights from fire. Burns up fast." },
    { ITEM_NUT, TEXT_DESC_V_NUT, "Deku Nut. Throw to stun enemies&and flash-blind nearby foes." },
    { ITEM_BOMB, TEXT_DESC_V_BOMB, "Throw to blow up walls, enemies&and obstacles. Short fuse." },
    { ITEM_BOW, TEXT_DESC_V_BOW, "Fire arrows. Hold C to aim.&Buy more arrows in shops." },
    { ITEM_ARROW_FIRE, TEXT_DESC_V_ARROW_FIRE, "Fire Arrow. Burns enemies and&lights torches. Costs magic." },
    { ITEM_DINS_FIRE, TEXT_DESC_V_DINS_FIRE, "Ring of flame around you. Burns&foes and lights torches. 6 MP." },
    { ITEM_SLINGSHOT, TEXT_DESC_V_SLINGSHOT, "Child ranged weapon. Fires Deku&Seeds. Hold C to aim." },
    { ITEM_OCARINA_FAIRY, TEXT_DESC_V_OCARINA_FAIRY, "Play songs to trigger magic.&Saria's Fairy Ocarina." },
    { ITEM_OCARINA_TIME, TEXT_DESC_V_OCARINA_TIME, "Play songs to trigger magic.&The royal Ocarina of Time." },
    { ITEM_BOMBCHU, TEXT_DESC_V_BOMBCHU, "Wind-up bomb that crawls along&floors and walls, then explodes." },
    { ITEM_HOOKSHOT, TEXT_DESC_V_HOOKSHOT, "Fire to grab targets and pull&yourself in, or items to you." },
    { ITEM_LONGSHOT, TEXT_DESC_V_LONGSHOT, "Like the Hookshot but with&twice the reach." },
    { ITEM_ARROW_ICE, TEXT_DESC_V_ARROW_ICE, "Ice Arrow. Freezes enemies&solid. Costs magic per shot." },
    { ITEM_FARORES_WIND, TEXT_DESC_V_FARORES_WIND, "Set a warp point, then teleport&back to it later. 6 MP." },
    { ITEM_BOOMERANG, TEXT_DESC_V_BOOMERANG, "Throw to stun foes and grab&distant items. Returns to you." },
    { ITEM_LENS, TEXT_DESC_V_LENS, "Lens of Truth. Reveals hidden&things and invisible foes. Drains MP." },
    { ITEM_BEAN, TEXT_DESC_V_BEAN, "Magic Bean. Plant in soft soil&to grow a ride. 10 total." },
    { ITEM_HAMMER, TEXT_DESC_V_HAMMER, "Megaton Hammer. Smash rusty&switches, posts and armor." },
    { ITEM_ARROW_LIGHT, TEXT_DESC_V_ARROW_LIGHT, "Light Arrow. Devastating holy&damage. High magic cost." },
    { ITEM_NAYRUS_LOVE, TEXT_DESC_V_NAYRUS_LOVE, "Protective barrier that blocks&all damage for a time. 12 MP." },
};

// ---------------------------------------------------------------------------
// Lookup: item ID + page -> text ID (or 0)
// ---------------------------------------------------------------------------

extern "C" u16 PauseItemDesc_GetTextId(u16 cursorItem, s32 pageIndex) {
    // Custom items + masks + SW97 arrows on ITEM pages
    if (pageIndex == PAUSE_ITEM) {
        for (size_t i = 0; i < ARRAY_COUNT(sCustomItemDescs); i++) {
            if (sCustomItemDescs[i].itemId == cursorItem)
                return sCustomItemDescs[i].textId;
        }
        for (size_t i = 0; i < ARRAY_COUNT(sMaskDescs); i++) {
            if (sMaskDescs[i].itemId == cursorItem)
                return sMaskDescs[i].textId;
        }
        // SW97 elemental shot: the cursor is on a plain bow/slingshot and the element rides a flag,
        // so describe whatever is primed on THAT weapon rather than looking the cursor item up.
        if (SW97_MEDALLIONS_ENABLED() && (Sw97_IsBowItem(cursorItem) || Sw97_IsSlingItem(cursorItem))) {
            u8 elem = Sw97_EffectiveElement(Sw97_IsSlingItem(cursorItem));
            for (size_t i = 0; i < ARRAY_COUNT(sSw97ElemDescs); i++) {
                if (sSw97ElemDescs[i].itemId == elem)
                    return sSw97ElemDescs[i].textId;
            }
        }
        // Elemental Wand: one id, six descriptions — follow the active mode.
        if (cursorItem == ITEM_ELEMENTAL_WAND) {
            u8 mode = Wand_GetMode();
            for (size_t i = 0; i < ARRAY_COUNT(sWandModeDescs); i++) {
                if (sWandModeDescs[i].itemId == mode)
                    return sWandModeDescs[i].textId;
            }
        }
        for (size_t i = 0; i < ARRAY_COUNT(sVanillaItemDescs); i++) {
            if (sVanillaItemDescs[i].itemId == cursorItem)
                return sVanillaItemDescs[i].textId;
        }
    }

    // Extended equipment on EQUIP page
    if (pageIndex == PAUSE_EQUIP) {
        for (size_t i = 0; i < ARRAY_COUNT(sExtEquipDescs); i++) {
            if (sExtEquipDescs[i].itemId == cursorItem)
                return sExtEquipDescs[i].textId;
        }
    }

    if (pageIndex == PAUSE_QUEST) {
        // The remains ride the MM quest page, which is independent of the SW97 medallion toggle.
        for (size_t i = 0; i < ARRAY_COUNT(sBossRemainsDescs); i++) {
            if (sBossRemainsDescs[i].itemId == cursorItem)
                return sBossRemainsDescs[i].textId;
        }
        if (SW97_MEDALLIONS_ENABLED()) {
            for (size_t i = 0; i < ARRAY_COUNT(sMedallionDescs); i++) {
                if (sMedallionDescs[i].itemId == cursorItem)
                    return sMedallionDescs[i].textId;
            }
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Message hook: build and load description into font
// ---------------------------------------------------------------------------

static void BuildDescMessage(const char* desc, uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg = CustomMessage(desc, desc, desc);
    msg.Format();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

// All description tables for single-hook lookup
// Matched on textId only, so the element/mode-keyed tables slot in here unchanged.
static const ItemDescEntry* sAllDescs[] = {
    sCustomItemDescs, sMaskDescs,      sSw97ElemDescs,    sWandModeDescs,
    sExtEquipDescs,   sMedallionDescs, sBossRemainsDescs, sVanillaItemDescs,
};
static const size_t sAllDescCounts[] = {
    ARRAY_COUNT(sCustomItemDescs),  ARRAY_COUNT(sMaskDescs),        ARRAY_COUNT(sSw97ElemDescs),
    ARRAY_COUNT(sWandModeDescs),    ARRAY_COUNT(sExtEquipDescs),    ARRAY_COUNT(sMedallionDescs),
    ARRAY_COUNT(sBossRemainsDescs), ARRAY_COUNT(sVanillaItemDescs),
};

// Single hook for all descriptions: fires on ANY OnOpenText, checks if textId matches
static void OnOpenTextDescHook(uint16_t* textId, bool* loadFromMessageTable) {
    for (size_t t = 0; t < ARRAY_COUNT(sAllDescs); t++) {
        for (size_t i = 0; i < sAllDescCounts[t]; i++) {
            if (sAllDescs[t][i].textId == *textId) {
                BuildDescMessage(sAllDescs[t][i].desc, textId, loadFromMessageTable);
                return;
            }
        }
    }
}

// Register all description hooks
static void RegisterPauseItemDescriptions() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenTextDescHook);
}

static RegisterShipInitFunc initPauseDescs(RegisterPauseItemDescriptions);
