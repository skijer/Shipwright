/* SharedItemTable.h — the canonical cross-game item list (Skijer's NEI).
 *
 * BYTE-IDENTICAL in both repos:
 *   2ship:  mm/2s2h/SharedItems/SharedItemTable.h
 *   soh:    soh/soh/SharedItems/SharedItemTable.h
 * Any edit is made to BOTH files or to neither. Bump SI_TABLE_REVISION on every edit.
 *
 * SHADOW MODE: nothing includes this header yet. It is the plan's F1 stage — the single source of
 * truth being assembled BEFORE any consumer switches over, so it carries zero build/runtime risk.
 * Consumers land later, one at a time (give -> draw -> pool -> icons/descriptions), each replacing
 * a per-repo hardcoded list with an expansion of this X-macro.
 *
 * NOT FleetComboItems.h: that table is APPEND-ONLY because its fcIds index comboObtainedFc[] in
 * both saves. SI_* ids are NEVER serialized, so rows here can be freely reordered — and the order
 * IS the user's canonical pause-menu layout (page 2 cell order, then equipment, then upgrades).
 *
 * Row shape:
 *   SI(siId, chain, level, category, slotToken, rgToken, riToken, canonName)
 *     siId      SI_* token (enum by position; never serialized)
 *     chain     progression-group tag (SI_CHAIN_NONE for a standalone item)
 *     level     0-based level inside the chain (0 for standalone)
 *     category  SIC_* — exactly the pool-checkbox granularity
 *     slotToken shared SLOT_* name (same token in both repos) or SI_NO_SLOT
 *     rgToken   soh-side RandomizerGet (expanded ONLY by the soh glue)
 *     riToken   2ship-side RandoItemId (expanded ONLY by the 2ship glue)
 *     canonName canonical English display name
 *
 * The peer token does not need to exist in this repo: each glue expands only its own column
 * (same trick as FleetComboItemsGlue.cpp).
 */
#ifndef SHARED_ITEM_TABLE_H
#define SHARED_ITEM_TABLE_H

#define SI_TABLE_REVISION 1
#define SI_ROW_COUNT 55

#define SI_NO_SLOT 0xFF
#define SI_CHAIN_NONE 0

/* Chains (progression groups). */
#define SI_CHAIN_ROC 1          /* Feather -> Cape */
#define SI_CHAIN_CANE_SOMARIA 2 /* Statue -> Block -> Platform (L3 unlocks the Trirod wheel entry) */
#define SI_CHAIN_CANE_PACCI 3   /* Flip -> Stone -> Ultrahand (L3 unlocks the Ultrahand wheel entry) */
#define SI_CHAIN_WAND 4         /* the six rods share the wand cell */
#define SI_CHAIN_SLATE 5        /* the four runes share the slate cell */

/* Categories == pool checkboxes (2ship RO_* / soh RSK_* map onto these 1:1). */
#define SIC_NEI_PAGE2 0 /* 2ship RO_SHUFFLE_NEI_ITEMS  | soh RSK_SKIJER_CUSTOM_ITEMS */
#define SIC_EQUIPMENT 1 /* 2ship RO_SHUFFLE_OOT_EQUIPMENT | soh RSK_EXT_EQUIPMENT */
#define SIC_OOT_GEAR 2  /* 2ship RO_SHUFFLE_OOT_GEAR   | soh: native pool */
#define SIC_OOT_QUEST 3 /* 2ship RO_SHUFFLE_OOT_QUEST  | soh: native pool */
#define SIC_OOT_MASKS 4 /* 2ship RO_SHUFFLE_OOT_MASKS  | soh: native pool */
#define SIC_MM_SONGS 5  /* 2ship: native pool          | soh RSK_MM_SONGS */
#define SIC_MM_MASKS 6  /* 2ship: native pool          | soh RSK_MM_MASKS_ALL */

/* clang-format off */
#define SHARED_ITEM_TABLE(SI) \
    /* ── Page 2, in the user's canonical cell order (cells 24..47) ─────────────────────────── */ \
    SI(SI_ROCS_FEATHER,      SI_CHAIN_ROC, 0, SIC_NEI_PAGE2, SLOT_ROCS,             RG_PROGRESSIVE_ROCS,      RI_OOT_PROGRESSIVE_ROC,          "Roc's Feather") \
    SI(SI_ROCS_CAPE,         SI_CHAIN_ROC, 1, SIC_NEI_PAGE2, SLOT_ROCS,             RG_PROGRESSIVE_ROCS,      RI_OOT_PROGRESSIVE_ROC,          "Roc's Cape") \
    SI(SI_WHIP,              SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_WHIP,            RG_WHIP,                  RI_OOT_NEI_WHIP,                 "Whip") \
    SI(SI_SPINNER,           SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_SPINNER,         RG_SPINNER,               RI_OOT_NEI_SPINNER,              "Spinner") \
    SI(SI_ELEMENTAL_WAND,    SI_CHAIN_WAND, 0, SIC_NEI_PAGE2, SLOT_ELEMENTAL_WAND,  RG_ELEMENTAL_WAND,        RI_OOT_NEI_ELEMENTAL_WAND,       "Elemental Wand") \
    SI(SI_WAND_SAND_ROD,     SI_CHAIN_WAND, 1, SIC_NEI_PAGE2, SLOT_ELEMENTAL_WAND,  RG_WAND_SAND_ROD,         RI_OOT_NEI_WAND_SAND_ROD,        "Sand Rod") \
    SI(SI_WAND_TORNADO_ROD,  SI_CHAIN_WAND, 2, SIC_NEI_PAGE2, SLOT_ELEMENTAL_WAND,  RG_WAND_TORNADO_ROD,      RI_OOT_NEI_WAND_TORNADO_ROD,     "Tornado Rod") \
    SI(SI_WAND_WATER_ROD,    SI_CHAIN_WAND, 3, SIC_NEI_PAGE2, SLOT_ELEMENTAL_WAND,  RG_WAND_WATER_ROD,        RI_OOT_NEI_WAND_WATER_ROD,       "Water Rod") \
    SI(SI_WAND_METEOR_ROD,   SI_CHAIN_WAND, 4, SIC_NEI_PAGE2, SLOT_ELEMENTAL_WAND,  RG_WAND_METEOR_ROD,       RI_OOT_NEI_WAND_METEOR_ROD,      "Meteor Rod") \
    SI(SI_WAND_STORM_ROD,    SI_CHAIN_WAND, 5, SIC_NEI_PAGE2, SLOT_ELEMENTAL_WAND,  RG_WAND_STORM_ROD,        RI_OOT_NEI_WAND_STORM_ROD,       "Storm Rod") \
    SI(SI_WAND_SHADOW_SCEPTER, SI_CHAIN_WAND, 6, SIC_NEI_PAGE2, SLOT_ELEMENTAL_WAND, RG_WAND_SHADOW_SCEPTER,  RI_OOT_NEI_WAND_SHADOW_SCEPTER,  "Shadow Scepter") \
    SI(SI_FIRE_ROD,          SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_FIRE_ROD,        RG_FIRE_ROD,              RI_OOT_NEI_FIRE_ROD,             "Fire Rod") \
    SI(SI_DEMISE_DESTRUCTION, SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_DEMISE_DESTRUCTION, RG_DEMISE_DESTRUCTION, RI_OOT_NEI_DEMISE_DESTRUCTION,  "Demise Destruction") \
    SI(SI_DEKU_LEAF,         SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_DEKU_LEAF,       RG_DEKU_LEAF,             RI_OOT_NEI_DEKU_LEAF,            "Deku Leaf") \
    SI(SI_TIME_GATE,         SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_TIME_GATE,       RG_TIME_GATE,             RI_OOT_NEI_TIME_GATE,            "Time Gate") \
    SI(SI_BEETLE,            SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_BEETLE,          RG_BEETLE,                RI_OOT_NEI_BEETLE,               "Beetle") \
    SI(SI_SWITCH_HOOK,       SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_SWITCH_HOOK,     RG_SWITCH_HOOK,           RI_OOT_NEI_SWITCH_HOOK,          "Switch Hook") \
    SI(SI_ICE_ROD,           SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_ICE_ROD,         RG_ICE_ROD,               RI_OOT_NEI_ICE_ROD,              "Ice Rod") \
    SI(SI_ZONAI_TIMER,       SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_ZONAI_PERMAFROST, RG_ZONAI_PERMAFROST,     RI_OOT_NEI_ZONAI_PERMAFROST,     "Zonai Timer") \
    SI(SI_MOGMA_MITTS,       SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_MOGMA_MITTS,     RG_MOGMA_MITTS,           RI_OOT_NEI_MOGMA_MITTS,          "Mogma Mitts") \
    SI(SI_GUST_JAR,          SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_GUST_JAR,        RG_GUST_JAR,              RI_OOT_NEI_GUST_JAR,             "Gust Jar") \
    SI(SI_BALL_AND_CHAIN,    SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_BALL_AND_CHAIN,  RG_BALL_AND_CHAIN,        RI_OOT_NEI_BALL_AND_CHAIN,       "Ball and Chain") \
    SI(SI_SHEIKAH_SLATE,     SI_CHAIN_SLATE, 0, SIC_NEI_PAGE2, SLOT_SHEIKAH_SLATE,  RG_SHEIKAH_SLATE,         RI_OOT_NEI_SHEIKAH_SLATE,        "Sheikah Slate") \
    SI(SI_LIGHT_ROD,         SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_LIGHT_ROD,       RG_LIGHT_ROD,             RI_OOT_NEI_LIGHT_ROD,            "Light Rod") \
    SI(SI_PHANTOM_HOURGLASS, SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_PHANTOM_HOURGLASS, RG_PHANTOM_HOURGLASS,   RI_OOT_NEI_PHANTOM_HOURGLASS,    "Phantom Hourglass") \
    SI(SI_LANTERN,           SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_LANTERN,         RG_LANTERN,               RI_OOT_NEI_LANTERN,              "Lantern") \
    SI(SI_MINISH_CAP,        SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_MINISH_CAP,      RG_MINISH_CAP,            RI_OOT_NEI_MINISH_CAP,           "The Minish Cap") \
    SI(SI_SHADOW_CRYSTAL,    SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_SHADOW_CRYSTAL,  RG_SHADOW_CRYSTAL,        RI_OOT_NEI_SHADOW_CRYSTAL,       "Shadow Crystal") \
    SI(SI_CANE_SOMARIA,      SI_CHAIN_CANE_SOMARIA, 0, SIC_NEI_PAGE2, SLOT_CANE_OF_SOMARIA, RG_CANE_OF_SOMARIA, RI_OOT_NEI_CANE_OF_SOMARIA,    "Cane of Somaria") \
    SI(SI_CANE_SOMARIA_BLOCK, SI_CHAIN_CANE_SOMARIA, 1, SIC_NEI_PAGE2, SLOT_CANE_OF_SOMARIA, RG_CANE_OF_SOMARIA, RI_OOT_NEI_CANE_SOMARIA_BLOCK, "Somaria Block") \
    SI(SI_CANE_SOMARIA_PLATFORM, SI_CHAIN_CANE_SOMARIA, 2, SIC_NEI_PAGE2, SLOT_CANE_OF_SOMARIA, RG_CANE_OF_SOMARIA, RI_OOT_NEI_CANE_SOMARIA_PLATFORM, "Somaria Platform") \
    SI(SI_CANE_PACCI_FLIP,   SI_CHAIN_CANE_PACCI, 0, SIC_NEI_PAGE2, SLOT_CANE_OF_SOMARIA, RG_CANE_OF_SOMARIA,  RI_OOT_NEI_CANE_PACCI_FLIP,      "Cane of Pacci") \
    SI(SI_CANE_PACCI_STONE,  SI_CHAIN_CANE_PACCI, 1, SIC_NEI_PAGE2, SLOT_CANE_OF_SOMARIA, RG_CANE_OF_SOMARIA,  RI_OOT_NEI_CANE_PACCI_STONE,     "Magic Powder") \
    SI(SI_CANE_PACCI_ULTRAHAND, SI_CHAIN_CANE_PACCI, 2, SIC_NEI_PAGE2, SLOT_CANE_OF_SOMARIA, RG_CANE_OF_SOMARIA, RI_OOT_NEI_CANE_PACCI_ULTRAHAND, "Pacci Ultrahand") \
    SI(SI_SHOVEL,            SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_SHOVEL,          RG_SHOVEL,                RI_OOT_NEI_SHOVEL,               "Shovel") \
    SI(SI_DOMINION_ROD,      SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_SHOVEL,          RG_DOMINION_ROD,          RI_OOT_NEI_DOMINION_ROD,         "Dominion Rod") \
    SI(SI_ROD_OF_SEASONS,    SI_CHAIN_NONE, 0, SIC_NEI_PAGE2, SLOT_ROD_OF_SEASONS,  RG_ROD_OF_SEASONS,        RI_OOT_NEI_ROD_OF_SEASONS,       "Rod of Seasons") \
    /* ── Equipment page 2, grid order (sword/shield/tunic/boots) + upgrade column ───────────── */ \
    SI(SI_CANE_OF_BYRNA,     SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_CANE_OF_BYRNA,     RI_OOT_EXT_CANE_OF_BYRNA,        "Cane of Byrna") \
    SI(SI_FOUR_SWORD,        SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_FOUR_SWORD,        RI_OOT_EXT_FOUR_SWORD,           "Four Sword") \
    SI(SI_TRIDENT,           SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_TRIDENT,           RI_OOT_EXT_TRIDENT,              "Trident") \
    SI(SI_DIVINE_SHIELD,     SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_DIVINE_SHIELD,     RI_OOT_EXT_DIVINE_SHIELD,        "Goddess Shield") \
    SI(SI_KITE_SHIELD,       SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_SHEIKAH_SHIELD,    RI_OOT_EXT_SHEIKAH_SHIELD,       "Kite Shield") \
    SI(SI_SHIELD_OF_IKANA,   SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_SHIELD_OF_IKANA,   RI_SHIELD_MIRROR,                "Shield of Ikana") \
    SI(SI_CHAMPIONS_TUNIC,   SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_CHAMPIONS_TUNIC,   RI_OOT_EXT_CHAMPIONS_TUNIC,      "Champion's Tunic") \
    SI(SI_MAGIC_TUNIC,       SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_SPIRIT_BREASTPLATE, RI_OOT_EXT_SPIRIT_BREASTPLATE,  "Magic Tunic") \
    SI(SI_SAGES_TUNIC,       SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_WATER_DRAGON_SCALE, RI_OOT_EXT_WATER_DRAGON_SCALE,  "Sage's Tunic") \
    SI(SI_PEGASUS_BOOTS,     SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_PEGASUS_ANKLET,    RI_OOT_EXT_PEGASUS_ANKLET,       "Pegasus Boots") \
    SI(SI_CLIMB_BOOTS,       SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_CLIMB_BOOTS,       RI_OOT_EXT_CLIMB_BOOTS,          "Climb Boots") \
    SI(SI_ROC_BOOTS,         SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_ROC_BOOTS,         RI_OOT_EXT_ROC_BOOTS,            "Roc's Boots") \
    SI(SI_MAGIC_CAPE,        SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_MAGIC_CAPE,        RI_OOT_EXT_MAGIC_CAPE,           "Magic Cape") \
    SI(SI_PENDANT_TRADE,     SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_PENDANT_OF_MEMORIES, RI_PENDANT_OF_MEMORIES,        "Pendant of Memories") \
    SI(SI_PENDANT_EQUIP,     SI_CHAIN_NONE, 0, SIC_EQUIPMENT, SI_NO_SLOT,           RG_EXT_PENDANT_OF_MEMORIES, RI_PENDANT_OF_MEMORIES,        "Pendant of Memories (moveset)") \
    /* ── Sheikah Slate runes (wand idiom over the slate cell). APPENDED so SI ids stay stable ── */ \
    SI(SI_SLATE_RUNE_BOMB,     SI_CHAIN_SLATE, 1, SIC_NEI_PAGE2, SLOT_SHEIKAH_SLATE, RG_SLATE_RUNE_BOMB,     RI_OOT_NEI_SLATE_RUNE_BOMB,     "Rune: Remote Bomb") \
    SI(SI_SLATE_RUNE_MASTER_CYCLE, SI_CHAIN_SLATE, 2, SIC_NEI_PAGE2, SLOT_SHEIKAH_SLATE, RG_SLATE_RUNE_MASTER_CYCLE, RI_OOT_NEI_SLATE_RUNE_MASTER_CYCLE, "Rune: Master Cycle") \
    SI(SI_SLATE_RUNE_STASIS,   SI_CHAIN_SLATE, 3, SIC_NEI_PAGE2, SLOT_SHEIKAH_SLATE, RG_SLATE_RUNE_STASIS,   RI_OOT_NEI_SLATE_RUNE_STASIS,   "Rune: Stasis") \
    SI(SI_SLATE_RUNE_CRYONIS,  SI_CHAIN_SLATE, 4, SIC_NEI_PAGE2, SLOT_SHEIKAH_SLATE, RG_SLATE_RUNE_CRYONIS,  RI_OOT_NEI_SLATE_RUNE_CRYONIS,  "Rune: Cryonis")
/* clang-format on */

/* Rows for the still-nonexistent items (Mipha's Grace, Urbosa's Fury, Falcon Helmet, Fuse, Mario
 * Mask normalization, Gale Boomerang as a rando item, Trirod as its own item) enter this table the
 * moment they get native ids on BOTH sides — a row needs its rg/ri pair to expand. */

#endif /* SHARED_ITEM_TABLE_H */
