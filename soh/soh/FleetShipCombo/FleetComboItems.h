#pragma once
// =============================================================================
// FleetComboItems.h — Combo Randomizer: tabla de ITEMS COMPARTIDOS (Fase 0)
//
// REGLAS (mismas que FleetComboIds.h):
//  1. Este header DEBE ser byte-idéntico en ambos repos:
//       2ship2harkinian/mm/2s2h/FleetShipCombo/FleetComboItems.h
//       Shipwright/soh/soh/FleetShipCombo/FleetComboItems.h
//  2. APPEND-ONLY: nunca reordenar ni borrar filas; los FCI_* ids son estables.
//  3. NO incluir headers de juego aquí. Las columnas rgToken/riToken son tokens
//     de preprocesador que SOLO expande el glue de cada repo:
//       - soh:   #define X(id, len, fl, rg, ri, cn, on, mn) { id, rg },
//       - 2ship: #define X(id, len, fl, rg, ri, cn, on, mn) { id, ri },
//     El token del otro juego nunca se emite, así que no necesita existir.
//  4. Los nombres ootName/mmName alimentan el spoiler y el VALIDADOR de arranque
//     (compara contra las tablas de items reales de cada juego y loguea drift;
//     obligatorio porque el parser de spoilers de SoH cae silencioso a enum 0).
//  5. Solo van aquí los items COMPARTIDOS (1 copia global para ambos juegos).
//     Los items solo-OoT / solo-MM NO tienen FCI: cada juego aporta su pool
//     dinámicamente vía la interfaz World (pull-from-main safe).
//
// V1 (decisión Skijer 2026-07-16): SIN gates OoT-only —
//     sin Child Wallet (cadena wallet arranca en Adult),
//     sin stick/nut bag-gate (solo upgrades de capacidad),
//     sin grab-gate (strength arranca en Bracelet).
// =============================================================================

// Sentinel para columna RG/RI sin item nativo todavía (expande a -1 en ambos lados)
#define FCI_NO_ITEM (-1)

// Flags
#define FCI_F_NONE 0
#define FCI_F_GOAL_BOTH (1 << 0)    // Greg: +1 a los DOS contadores de meta
#define FCI_F_TRIFORCE (1 << 1)     // pieza de triforce (contador comboTriforce)
#define FCI_F_TRAP (1 << 2)         // trampa: el juego receptor materializa su sabor local
#define FCI_F_DRAW_ONLY_MM (1 << 3) // sin relative funcional en MM: solo DL + messagebox
#define FCI_F_DUAL_GRANT (1 << 4)   // Pendant: en MM otorga trade item (check) + equip ext
// Deliberadamente LOCAL de su juego: no cruza y NO sale en el informe unshared. Para consumibles de
// relleno (bombchus, flechas...) y items sin sentido cross: el modelo FC es "1 copia global", asi que
// un item que cada juego reparte a docenas no encaja — le meti fila a los bombchus y la validacion
// canto "expected 1, got 24". La fila se queda (ids append-only) pero marcada y con peer vacio.
#define FCI_F_NOT_SHARED (1 << 5)
// Ocarina song: candidata a los 24 song spots compartidos (opcion gFleetCombo.SharedSongs).
// Double Time e Inverted Time quedan FUERA a proposito (decision del usuario): si MM las
// baraja van a su pool general, no ocupan spot compartido. No hay fila de Scarecrow.
#define FCI_F_SONG (1 << 6)
// Dungeon reward: OoT's 6 medallions + 3 spiritual stones and MM's 4 boss remains. The shared
// "Dungeon Rewards" option can bind these 13 items to the 13 boss spots across BOTH games, so
// beating the Fire Temple can hand you Odolwa's Remains. Same restricted-category machinery as
// FCI_F_SONG; see gFcCategories in FleetComboRando.cpp. Skijer's NEI
#define FCI_F_DUNGEON_REWARD (1 << 7)

// X(fcId, chainLen, flags, rgToken, riToken, "Combo Name", "OoT item name", "MM display name")
//   chainLen: 1 = item único; >1 = cadena progresiva (= copias en el pool v1)
//   ootName: nombre EXACTO de itemTable de soh (validado al arranque contra GetName().GetEnglish()).
//   mmName: nombre legible SOLO informativo. El spoilerName real de 2ship es el nombre del enum
//           ("RI_HOOKSHOT") y se deriva automáticamente stringificando el token riToken (#ri) en los
//           glue — tanto para validar como para emitir el spoiler MM. No puede driftear.
//   Known-gap: RG_CHATEAU_ROMANI existe como RG pero no tiene fila en itemTable (logic-only) —
//           el validador OoT lo canta hasta que le demos fila en la fase de draws.
#define FC_COMBO_ITEM_LIST(X)                                                                                          \
    /* ---------- A1: cadenas progresivas ---------- */                                                                \
    X(FCI_HOOKSHOT, 3, FCI_F_NONE, RG_PROGRESSIVE_HOOKSHOT, RI_HOOKSHOT, "Progressive Hookshot",                       \
      "Progressive Hookshot", "Hookshot")                                                                              \
    X(FCI_BOW, 4, FCI_F_NONE, RG_PROGRESSIVE_BOW, RI_PROGRESSIVE_BOW, "Progressive Bow", "Progressive Bow",            \
      "Progressive Bow")                                                                                               \
    X(FCI_BOMB_BAG, 4, FCI_F_NONE, RG_PROGRESSIVE_BOMB_BAG, RI_PROGRESSIVE_BOMB_BAG, "Progressive Bomb Bag",           \
      "Progressive Bomb Bag", "Progressive Bomb Bag")                                                                  \
    X(FCI_MAGIC, 3, FCI_F_NONE, RG_PROGRESSIVE_MAGIC_METER, RI_PROGRESSIVE_MAGIC, "Progressive Magic",                 \
      "Progressive Magic Meter", "Progressive Magic")                                                                  \
    X(FCI_WALLET, 4, FCI_F_NONE, RG_PROGRESSIVE_WALLET, RI_PROGRESSIVE_WALLET, "Progressive Wallet",                   \
      "Progressive Wallet", "Progressive Wallet") /* v1: Adult->Giant->Tycoon->Inf (sin Child) */                      \
    X(FCI_SLINGSHOT, 3, FCI_F_NONE, RG_PROGRESSIVE_SLINGSHOT, RI_FAIRY_SLINGSHOT, "Progressive Slingshot",             \
      "Progressive Slingshot", "Fairy Slingshot")                                                                      \
    X(FCI_STICK_CAPACITY, 2, FCI_F_NONE, RG_PROGRESSIVE_STICK_UPGRADE, RI_OOT_PROGRESSIVE_STICK_CAPACITY,              \
      "Progressive Stick Capacity", "Progressive Stick Capacity", "Progressive Stick Capacity") /* v1 sin bag-gate */  \
    X(FCI_NUT_CAPACITY, 2, FCI_F_NONE, RG_PROGRESSIVE_NUT_UPGRADE, RI_OOT_PROGRESSIVE_NUT_CAPACITY,                    \
      "Progressive Nut Capacity", "Progressive Nut Capacity", "Progressive Nut Capacity") /* v1 sin bag-gate */        \
    X(FCI_STRENGTH, 3, FCI_F_NONE, RG_PROGRESSIVE_STRENGTH, RI_OOT_PROGRESSIVE_STRENGTH, "Progressive Strength",       \
      "Strength Upgrade", "Progressive Strength Upgrade") /* Bracelet -> Silver -> Gold. The MM peer used to be        \
                                                             FCI_NO_ITEM, so the per-game filter saw it in NEITHER     \
                                                             pool and it never crossed in any seed. Skijer's NEI */    \
    X(FCI_SCALE, 3, FCI_F_NONE, RG_PROGRESSIVE_SCALE, RI_ABILITY_SWIM, "Progressive Scale", "Progressive Scale",       \
      "Ability to Swim") /* L1 Bronze = swim MM */                                                                     \
    X(FCI_KOKIRI_SWORD, 3, FCI_F_NONE, RG_PROGRESSIVE_KOKIRI_SWORD, RI_PROGRESSIVE_SWORD, "Progressive Sword",         \
      "Progressive Kokiri Sword", "Progressive Sword") /* Kokiri->Razor->Gilded */                                     \
    X(FCI_MASTER_SWORD, 2, FCI_F_NONE, RG_PROGRESSIVE_MASTER_SWORD, RI_OOT_PROGRESSIVE_MASTER_SWORD,                   \
      "Progressive Master Sword", "Progressive Master Sword", "Progressive Master Sword")                              \
    X(FCI_BIGGORON_SWORD, 2, FCI_F_NONE, RG_PROGRESSIVE_BGS, RI_OOT_PROGRESSIVE_BGS, "Progressive Biggoron Sword",     \
      "Progressive Biggoron's Sword",                                                                                  \
      "Progressive Biggoron's Sword") /* L1 BGS -> L2 GFS (weaponUpgrades bit 4). The MM peer is the PROGRESSIVE item, \
                                         not the bare Great Fairy's Sword: the chain only escalates if both sides hand \
                                         out the same escalating item. Skijer's NEI */                                 \
    X(FCI_HAMMER, 2, FCI_F_NONE, RG_PROGRESSIVE_HAMMER, RI_OOT_PROGRESSIVE_HAMMER, "Progressive Hammer",               \
      "Progressive Hammer", "Progressive Hammer") /* Hammer->Hammer-Axe */                                             \
    X(FCI_SKIJER_ROC, 2, FCI_F_NONE, RG_PROGRESSIVE_ROCS, RI_OOT_PROGRESSIVE_ROC, "Progressive Roc",                   \
      "Progressive Roc", "Progressive Roc") /* Feather SKIJER->Cape; NO es el Roc's Feather regular */                 \
    X(FCI_OCARINA, 2, FCI_F_NONE, RG_PROGRESSIVE_OCARINA, RI_OCARINA, "Progressive Ocarina", "Progressive Ocarina",    \
      "Ocarina")                                                                                                       \
    X(FCI_BOMBCHU_BAG, 1, FCI_F_NONE, RG_PROGRESSIVE_BOMBCHU_BAG, RI_OOT_BOMBCHU_BAG, "Bombchu Bag", "Bombchu Bag",    \
      "Bombchu Bag") /* gate logico MM pendiente */                                                                    \
    /* ---------- A2: items unicos ---------- */                                                                       \
    X(FCI_FIRE_ARROWS, 1, FCI_F_NONE, RG_FIRE_ARROWS, RI_ARROW_FIRE, "Fire Arrows", "Fire Arrows", "Fire Arrows")      \
    X(FCI_ICE_ARROWS, 1, FCI_F_NONE, RG_ICE_ARROWS, RI_ARROW_ICE, "Ice Arrows", "Ice Arrows", "Ice Arrows")            \
    X(FCI_LIGHT_ARROWS, 1, FCI_F_NONE, RG_LIGHT_ARROWS, RI_ARROW_LIGHT, "Light Arrows", "Light Arrows",                \
      "Light Arrows")                                                                                                  \
    X(FCI_LENS_OF_TRUTH, 1, FCI_F_NONE, RG_LENS_OF_TRUTH, RI_LENS, "Lens of Truth", "Lens of Truth", "Lens of Truth")  \
    X(FCI_BOOMERANG, 1, FCI_F_NONE, RG_BOOMERANG, RI_OOT_BOOMERANG, "Boomerang", "Boomerang", "Boomerang")             \
    X(FCI_DINS_FIRE, 1, FCI_F_NONE, RG_DINS_FIRE, RI_OOT_DINS_FIRE, "Din's Fire", "Din's Fire", "Din's Fire")          \
    X(FCI_FARORES_WIND, 1, FCI_F_NONE, RG_FARORES_WIND, RI_OOT_FARORES_WIND, "Farore's Wind", "Farore's Wind",         \
      "Farore's Wind")                                                                                                 \
    X(FCI_NAYRUS_LOVE, 1, FCI_F_NONE, RG_NAYRUS_LOVE, RI_OOT_NAYRUS_LOVE, "Nayru's Love", "Nayru's Love",              \
      "Nayru's Love")                                                                                                  \
    X(FCI_IRON_BOOTS, 1, FCI_F_NONE, RG_IRON_BOOTS, RI_OOT_IRON_BOOTS, "Iron Boots", "Iron Boots", "Iron Boots")       \
    X(FCI_HOVER_BOOTS, 1, FCI_F_NONE, RG_HOVER_BOOTS, RI_OOT_HOVER_BOOTS, "Hover Boots", "Hover Boots", "Hover Boots") \
    X(FCI_GORON_TUNIC, 1, FCI_F_NONE, RG_GORON_TUNIC, RI_OOT_GORON_TUNIC, "Goron Tunic", "Goron Tunic", "Goron Tunic") \
    X(FCI_ZORA_TUNIC, 1, FCI_F_NONE, RG_ZORA_TUNIC, RI_OOT_ZORA_TUNIC, "Zora Tunic", "Zora Tunic", "Zora Tunic")       \
    X(FCI_MIRROR_SHIELD_OOT, 1, FCI_F_NONE, RG_MIRROR_SHIELD, RI_OOT_MIRROR_SHIELD, "Mirror Shield (OoT)",             \
      "Mirror Shield", "Mirror Shield (OoT)") /* en MM = vanillaShieldSkin */                                          \
    X(FCI_SHIELD_OF_IKANA, 1, FCI_F_NONE, RG_EXT_SHIELD_OF_IKANA, RI_SHIELD_MIRROR, "Shield of Ikana",                 \
      "Shield of Ikana", "Mirror Shield") /* = Mirror Shield vanilla de MM */                                          \
    X(FCI_HYLIAN_SHIELD, 1, FCI_F_NONE, RG_HYLIAN_SHIELD, RI_SHIELD_HERO, "Hylian Shield", "Hylian Shield",            \
      "Hero's Shield")                                                                                                 \
    X(FCI_DOUBLE_DEFENSE, 1, FCI_F_NONE, RG_DOUBLE_DEFENSE, RI_DOUBLE_DEFENSE, "Double Defense", "Double Defense",     \
      "Double Defense")                                                                                                \
    X(FCI_STONE_OF_AGONY, 2, FCI_F_NONE, RG_STONE_OF_AGONY, RI_OOT_STONE_OF_AGONY, "Stone of Agony", "Stone of Agony", \
      "Stone of Agony") /* MM: ootQuestItems bit 21; chain: 1=Stone of Agony, 2=Quartz of Motion */                    \
    X(FCI_GERUDO_CARD, 1, FCI_F_NONE, RG_GERUDO_MEMBERSHIP_CARD, RI_OOT_GERUDO_MEMBERSHIP_CARD,                        \
      "Gerudo Membership Card", "Gerudo Membership Card", "Gerudo Membership Card") /* MM: ootQuestItems bit 22 */     \
    X(FCI_MAGIC_BEAN_PACK, 1, FCI_F_NONE, RG_MAGIC_BEAN_PACK, RI_MAGIC_BEAN, "Magic Bean Pack", "Magic Bean Pack",     \
      "Magic Bean") /* capacidad = sitios de AMBOS juegos */                                                           \
    X(FCI_FISHING_POLE, 1, FCI_F_DRAW_ONLY_MM, RG_FISHING_POLE, RI_OOT_FISHING_POLE, "Fishing Pole", "Fishing Pole",   \
      "Fishing Pole") /* MM: solo DL+mensaje por ahora */                                                              \
    X(FCI_POWDER_KEG, 1, FCI_F_NONE, RG_MM_POWDER_KEG, RI_POWDER_KEG, "Powder Keg", "Powder Keg", "Powder Keg")        \
    X(FCI_PICTOGRAPH_BOX, 1, FCI_F_NONE, RG_MM_PICTOGRAPH_BOX, RI_PICTOGRAPH_BOX, "Pictograph Box", "Pictograph Box",  \
      "Pictograph Box")                                                                                                \
    X(FCI_BOMBERS_NOTEBOOK, 1, FCI_F_NONE, RG_MM_BOMBERS_NOTEBOOK, RI_BOMBERS_NOTEBOOK, "Bomber's Notebook",           \
      "Bomber's Notebook", "Bomber's Notebook")                                                                        \
    X(FCI_PENDANT_OF_MEMORIES, 1, FCI_F_DUAL_GRANT, RG_EXT_PENDANT_OF_MEMORIES, RI_PENDANT_OF_MEMORIES,                \
      "Pendant of Memories", "Pendant of Memories", "Pendant of Memories")                                             \
    X(FCI_SKELETON_KEY, 1, FCI_F_NONE, RG_SKELETON_KEY, RI_SKELETON_KEY, "Skeleton Key", "Skeleton Key",               \
      "Skeleton Key") /* MM peer = the NATIVE 2ship 5.0.0 Skeleton Key (grants every dungeon's small keys);            \
                         RI_OOT_SKELETON_KEY was a no-op placeholder */                                                \
    X(FCI_GREG, 1, FCI_F_GOAL_BOTH, RG_GREG_RUPEE, RI_OOT_GREG, "Greg", "Greg the Green Rupee", "Greg")                \
    X(FCI_TRIFORCE_PIECE, 1, FCI_F_TRIFORCE, RG_TRIFORCE_PIECE, RI_TRIFORCE_PIECE, "Triforce Piece", "Triforce Piece", \
      "Piece of the Triforce")                                                                                         \
    X(FCI_TRAP, 1, FCI_F_TRAP, RG_ICE_TRAP, RI_TRAP, "Trap", "Ice Trap", "Knockoff Item")                              \
    /* ---------- A3: canciones compartidas ---------- */                                                              \
    X(FCI_SONG_EPONA, 1, FCI_F_SONG, RG_EPONAS_SONG, RI_SONG_EPONA, "Epona's Song", "Epona's Song", "Epona's Song")    \
    X(FCI_SONG_TIME, 1, FCI_F_SONG, RG_SONG_OF_TIME, RI_SONG_TIME, "Song of Time", "Song of Time",                     \
      "Song of Time") /* starting item por defecto */                                                                  \
    X(FCI_SONG_STORMS, 1, FCI_F_SONG, RG_SONG_OF_STORMS, RI_SONG_STORMS, "Song of Storms", "Song of Storms",           \
      "Song of Storms")                                                                                                \
    X(FCI_SONG_SUN, 1, FCI_F_SONG, RG_SUNS_SONG, RI_SONG_SUN, "Sun's Song", "Sun's Song", "Sun's Song")                \
    X(FCI_SONG_SARIA, 1, FCI_F_SONG, RG_SARIAS_SONG, RI_SONG_SARIA, "Saria's Song", "Saria's Song", "Saria's Song")    \
    X(FCI_SONG_FUGUE_OF_HOME, 1, FCI_F_SONG, RG_NEI_SONG_FUGUE_OF_HOME, RI_OOT_SONG_FUGUE_OF_HOME, "Fugue of Home",    \
      "Fugue of Home", "Fugue of Home") /* NEI custom; storage ootQuestItems bit 7 / OCARINA_SONG_NEI_* */             \
    X(FCI_SONG_COMMAND_MELODY, 1, FCI_F_SONG, RG_NEI_SONG_COMMAND_MELODY, RI_OOT_SONG_COMMAND_MELODY,                  \
      "Command Melody", "Command Melody", "Command Melody") /* NEI custom; bit 10 */                                   \
    X(FCI_SONG_BALLAD_OF_HERO, 1, FCI_F_SONG, RG_NEI_SONG_BALLAD_OF_HERO, RI_OOT_SONG_BALLAD_OF_THE_HERO,              \
      "Ballad of the Hero", "Ballad of the Hero", "Ballad of the Hero") /* NEI custom; bit 11 */                       \
    /* ---------- A4: botones de ocarina ---------- */                                                                 \
    X(FCI_OCARINA_BUTTON_A, 1, FCI_F_NONE, RG_OCARINA_A_BUTTON, RI_OCARINA_BUTTON_A, "Ocarina A Button",               \
      "Ocarina A Button", "Ocarina A Button")                                                                          \
    X(FCI_OCARINA_BUTTON_C_UP, 1, FCI_F_NONE, RG_OCARINA_C_UP_BUTTON, RI_OCARINA_BUTTON_C_UP, "Ocarina C Up Button",   \
      "Ocarina C Up Button", "Ocarina C Up Button")                                                                    \
    X(FCI_OCARINA_BUTTON_C_DOWN, 1, FCI_F_NONE, RG_OCARINA_C_DOWN_BUTTON, RI_OCARINA_BUTTON_C_DOWN,                    \
      "Ocarina C Down Button", "Ocarina C Down Button", "Ocarina C Down Button")                                       \
    X(FCI_OCARINA_BUTTON_C_LEFT, 1, FCI_F_NONE, RG_OCARINA_C_LEFT_BUTTON, RI_OCARINA_BUTTON_C_LEFT,                    \
      "Ocarina C Left Button", "Ocarina C Left Button", "Ocarina C Left Button")                                       \
    X(FCI_OCARINA_BUTTON_C_RIGHT, 1, FCI_F_NONE, RG_OCARINA_C_RIGHT_BUTTON, RI_OCARINA_BUTTON_C_RIGHT,                 \
      "Ocarina C Right Button", "Ocarina C Right Button", "Ocarina C Right Button")                                    \
    /* ---------- A5: las 24 mascaras de MM ---------- */                                                              \
    X(FCI_MASK_POSTMAN, 1, FCI_F_NONE, RG_MM_MASK_POSTMAN, RI_MASK_POSTMAN, "Postman's Hat", "Postman's Hat",          \
      "Postman's Hat")                                                                                                 \
    X(FCI_MASK_ALL_NIGHT, 1, FCI_F_NONE, RG_MM_MASK_ALL_NIGHT, RI_MASK_ALL_NIGHT, "All-Night Mask", "All-Night Mask",  \
      "All-Night Mask")                                                                                                \
    X(FCI_MASK_BLAST, 1, FCI_F_NONE, RG_MM_MASK_BLAST, RI_MASK_BLAST, "Blast Mask", "Blast Mask", "Blast Mask")        \
    X(FCI_MASK_STONE, 1, FCI_F_NONE, RG_MM_MASK_STONE, RI_MASK_STONE, "Stone Mask", "Stone Mask", "Stone Mask")        \
    X(FCI_MASK_GREAT_FAIRY, 1, FCI_F_NONE, RG_MM_MASK_GREAT_FAIRY, RI_MASK_GREAT_FAIRY, "Great Fairy Mask",            \
      "Great Fairy Mask", "Great Fairy Mask")                                                                          \
    X(FCI_MASK_DEKU, 1, FCI_F_NONE, RG_MM_MASK_DEKU, RI_MASK_DEKU, "Deku Mask", "Deku Mask", "Deku Mask")              \
    X(FCI_MASK_KEATON, 1, FCI_F_NONE, RG_MM_MASK_KEATON, RI_MASK_KEATON, "Keaton Mask", "Keaton Mask (MM)",            \
      "Keaton Mask")                                                                                                   \
    X(FCI_MASK_BREMEN, 1, FCI_F_NONE, RG_MM_MASK_BREMEN, RI_MASK_BREMEN, "Bremen Mask", "Bremen Mask", "Bremen Mask")  \
    X(FCI_MASK_BUNNY, 1, FCI_F_NONE, RG_MM_MASK_BUNNY, RI_MASK_BUNNY, "Bunny Hood", "Bunny Hood (MM)", "Bunny Hood")   \
    X(FCI_MASK_DON_GERO, 1, FCI_F_NONE, RG_MM_MASK_DON_GERO, RI_MASK_DON_GERO, "Don Gero's Mask", "Don Gero's Mask",   \
      "Don Gero's Mask")                                                                                               \
    X(FCI_MASK_SCENTS, 1, FCI_F_NONE, RG_MM_MASK_SCENTS, RI_MASK_SCENTS, "Mask of Scents", "Mask of Scents",           \
      "Mask of Scents")                                                                                                \
    X(FCI_MASK_GORON, 1, FCI_F_NONE, RG_MM_MASK_GORON, RI_MASK_GORON, "Goron Mask", "Goron Mask (MM)", "Goron Mask")   \
    X(FCI_MASK_ROMANI, 1, FCI_F_NONE, RG_MM_MASK_ROMANI, RI_MASK_ROMANI, "Romani's Mask", "Romani's Mask",             \
      "Romani's Mask")                                                                                                 \
    X(FCI_MASK_CIRCUS_LEADER, 1, FCI_F_NONE, RG_MM_MASK_CIRCUS_LEADER, RI_MASK_CIRCUS_LEADER, "Circus Leader's Mask",  \
      "Circus Leader's Mask", "Circus Leader's Mask")                                                                  \
    X(FCI_MASK_KAFEI, 1, FCI_F_NONE, RG_MM_MASK_KAFEI, RI_MASK_KAFEIS_MASK, "Kafei's Mask", "Kafei's Mask",            \
      "Kafei's Mask")                                                                                                  \
    X(FCI_MASK_COUPLE, 1, FCI_F_NONE, RG_MM_MASK_COUPLE, RI_MASK_COUPLE, "Couple's Mask", "Couple's Mask",             \
      "Couple's Mask")                                                                                                 \
    X(FCI_MASK_TRUTH, 1, FCI_F_NONE, RG_MM_MASK_TRUTH, RI_MASK_TRUTH, "Mask of Truth", "Mask of Truth (MM)",           \
      "Mask of Truth")                                                                                                 \
    X(FCI_MASK_ZORA, 1, FCI_F_NONE, RG_MM_MASK_ZORA, RI_MASK_ZORA, "Zora Mask", "Zora Mask (MM)", "Zora Mask")         \
    X(FCI_MASK_KAMARO, 1, FCI_F_NONE, RG_MM_MASK_KAMARO, RI_MASK_KAMARO, "Kamaro's Mask", "Kamaro's Mask",             \
      "Kamaro's Mask")                                                                                                 \
    X(FCI_MASK_GIBDO, 1, FCI_F_NONE, RG_MM_MASK_GIBDO, RI_MASK_GIBDO, "Gibdo Mask", "Gibdo Mask", "Gibdo Mask")        \
    X(FCI_MASK_GARO, 1, FCI_F_NONE, RG_MM_MASK_GARO, RI_MASK_GARO, "Garo's Mask", "Garo's Mask", "Garo's Mask")        \
    X(FCI_MASK_CAPTAIN, 1, FCI_F_NONE, RG_MM_MASK_CAPTAIN, RI_MASK_CAPTAIN, "Captain's Hat", "Captain's Hat",          \
      "Captain's Hat")                                                                                                 \
    X(FCI_MASK_GIANT, 1, FCI_F_NONE, RG_MM_MASK_GIANT, RI_MASK_GIANT, "Giant's Mask", "Giant's Mask", "Giant's Mask")  \
    X(FCI_MASK_FIERCE_DEITY, 1, FCI_F_NONE, RG_MM_MASK_FIERCE_DEITY, RI_MASK_FIERCE_DEITY, "Fierce Deity's Mask",      \
      "Fierce Deity's Mask", "Fierce Deity's Mask")                                                                    \
    /* ---------- A6: los 24 items NEI pagina-2 (Roc va en A1) ---------- */                                           \
    X(FCI_WHIP, 1, FCI_F_NONE, RG_WHIP, RI_OOT_NEI_WHIP, "Whip", "Whip", "Whip")                                       \
    X(FCI_SPINNER, 1, FCI_F_NONE, RG_SPINNER, RI_OOT_NEI_SPINNER, "Spinner", "Spinner", "Spinner")                     \
    X(FCI_BOMB_ARROWS, 1, FCI_F_NONE, RG_BOMB_ARROWS, RI_OOT_NEI_BOMB_ARROWS, "Bomb Arrows", "Bomb Arrows",            \
      "Bomb Arrows")                                                                                                   \
    X(FCI_FIRE_ROD, 1, FCI_F_NONE, RG_FIRE_ROD, RI_OOT_NEI_FIRE_ROD, "Fire Rod", "Fire Rod", "Fire Rod")               \
    X(FCI_DEMISE_DESTRUCTION, 1, FCI_F_NONE, RG_DEMISE_DESTRUCTION, RI_OOT_NEI_DEMISE_DESTRUCTION,                     \
      "Demise Destruction", "Demise Destruction", "Demise Destruction")                                                \
    X(FCI_DEKU_LEAF, 1, FCI_F_NONE, RG_DEKU_LEAF, RI_OOT_NEI_DEKU_LEAF, "Deku Leaf", "Deku Leaf", "Deku Leaf")         \
    X(FCI_TIME_GATE, 1, FCI_F_NONE, RG_TIME_GATE, RI_OOT_NEI_TIME_GATE, "Time Gate", "Time Gate", "Time Gate")         \
    X(FCI_BEETLE, 1, FCI_F_NONE, RG_BEETLE, RI_OOT_NEI_BEETLE, "Beetle", "Beetle", "Beetle")                           \
    X(FCI_SWITCH_HOOK, 1, FCI_F_NONE, RG_SWITCH_HOOK, RI_OOT_NEI_SWITCH_HOOK, "Switch Hook", "Switch Hook",            \
      "Switch Hook")                                                                                                   \
    X(FCI_ICE_ROD, 1, FCI_F_NONE, RG_ICE_ROD, RI_OOT_NEI_ICE_ROD, "Ice Rod", "Ice Rod", "Ice Rod")                     \
    X(FCI_ZONAI_PERMAFROST, 1, FCI_F_NONE, RG_ZONAI_PERMAFROST, RI_OOT_NEI_ZONAI_PERMAFROST, "Zonai Timer",            \
      "Zonai Timer", "Zonai Timer")                                                                                    \
    X(FCI_MOGMA_MITTS, 1, FCI_F_NONE, RG_MOGMA_MITTS, RI_OOT_NEI_MOGMA_MITTS, "Mogma Mitts", "Mogma Mitts",            \
      "Mogma Mitts")                                                                                                   \
    X(FCI_GUST_JAR, 1, FCI_F_NONE, RG_GUST_JAR, RI_OOT_NEI_GUST_JAR, "Gust Jar", "Gust Jar", "Gust Jar")               \
    X(FCI_BALL_AND_CHAIN, 1, FCI_F_NONE, RG_BALL_AND_CHAIN, RI_OOT_NEI_BALL_AND_CHAIN, "Ball and Chain",               \
      "Ball and Chain", "Ball and Chain")                                                                              \
    X(FCI_LIGHT_ROD, 1, FCI_F_NONE, RG_LIGHT_ROD, RI_OOT_NEI_LIGHT_ROD, "Light Rod", "Light Rod", "Light Rod")         \
    X(FCI_HYLIAS_GRACE, 1, FCI_F_NONE, RG_HYLIAS_GRACE, RI_OOT_NEI_HYLIAS_GRACE, "Hylia's Grace", "Hylia's Grace",     \
      "Hylia's Grace")                                                                                                 \
    X(FCI_LANTERN, 1, FCI_F_NONE, RG_LANTERN, RI_OOT_NEI_LANTERN, "Lantern", "Lantern", "Lantern")                     \
    X(FCI_MINISH_CAP, 1, FCI_F_NONE, RG_MINISH_CAP, RI_OOT_NEI_MINISH_CAP, "The Minish Cap", "The Minish Cap",         \
      "The Minish Cap")                                                                                                \
    X(FCI_POKEBALL, 1, FCI_F_NONE, RG_POKEBALL, RI_OOT_NEI_POKE_BALL, "Poke Ball", "Pok\xC3\xA9 Ball", "Poke Ball")    \
    X(FCI_CANE_OF_SOMARIA, 6, FCI_F_NONE, RG_CANE_OF_SOMARIA, RI_OOT_NEI_CANE_OF_SOMARIA, "Cane of Somaria",           \
      "Cane of Somaria", "Cane of Somaria")                                                                            \
    X(FCI_SHOVEL, 1, FCI_F_NONE, RG_SHOVEL, RI_OOT_NEI_SHOVEL, "Shovel", "Shovel", "Shovel")                           \
    X(FCI_DOMINION_ROD, 1, FCI_F_NONE, RG_DOMINION_ROD, RI_OOT_NEI_DOMINION_ROD, "Dominion Rod", "Dominion Rod",       \
      "Dominion Rod")                                                                                                  \
    X(FCI_DESIRE_SENSOR, 1, FCI_F_NONE, RG_DESIRE_SENSOR, RI_OOT_NEI_DESIRE_SENSOR, "Desire Sensor", "Desire Sensor",  \
      "Desire Sensor")                                                                                                 \
    /* ---------- A7: extended equipment (Ikana y Pendant estan arriba) ---------- */                                  \
    X(FCI_EXT_CANE_OF_BYRNA, 1, FCI_F_NONE, RG_EXT_CANE_OF_BYRNA, RI_OOT_EXT_CANE_OF_BYRNA, "Cane of Byrna",           \
      "Cane of Byrna", "Cane of Byrna")                                                                                \
    X(FCI_EXT_FOUR_SWORD, 1, FCI_F_NONE, RG_EXT_FOUR_SWORD, RI_OOT_EXT_FOUR_SWORD, "Four Sword", "Four Sword",         \
      "Four Sword")                                                                                                    \
    X(FCI_EXT_DIVINE_SHIELD, 1, FCI_F_NONE, RG_EXT_DIVINE_SHIELD, RI_OOT_EXT_DIVINE_SHIELD, "Goddess Shield",          \
      "Goddess Shield", "Goddess Shield")                                                                              \
    X(FCI_EXT_SHEIKAH_SHIELD, 1, FCI_F_NONE, RG_EXT_SHEIKAH_SHIELD, RI_OOT_EXT_SHEIKAH_SHIELD, "Kite Shield",          \
      "Kite Shield", "Kite Shield")                                                                                    \
    X(FCI_EXT_MAGIC_CAPE, 1, FCI_F_NONE, RG_EXT_MAGIC_CAPE, RI_OOT_EXT_MAGIC_CAPE, "Magic Cape", "Magic Cape",         \
      "Magic Cape")                                                                                                    \
    X(FCI_EXT_SPIRIT_BREASTPLATE, 1, FCI_F_NONE, RG_EXT_SPIRIT_BREASTPLATE, RI_OOT_EXT_SPIRIT_BREASTPLATE,             \
      "Magic Tunic", "Magic Tunic", "Magic Tunic")                                                                     \
    X(FCI_EXT_CHAMPIONS_TUNIC, 1, FCI_F_NONE, RG_EXT_CHAMPIONS_TUNIC, RI_OOT_EXT_CHAMPIONS_TUNIC, "Champion's Tunic",  \
      "Champion's Tunic", "Champion's Tunic")                                                                          \
    X(FCI_EXT_PEGASUS_ANKLET, 1, FCI_F_NONE, RG_EXT_PEGASUS_ANKLET, RI_OOT_EXT_PEGASUS_ANKLET, "Pegasus Boots",        \
      "Pegasus Boots", "Pegasus Boots")                                                                                \
    X(FCI_EXT_WATER_DRAGON_SCALE, 1, FCI_F_NONE, RG_EXT_WATER_DRAGON_SCALE, RI_OOT_EXT_WATER_DRAGON_SCALE,             \
      "Sage's Tunic", "Sage's Tunic",                                                                                  \
      "Sage's Tunic") /* legacy IDs retained for serialized FleetCombo compatibility */                                \
    /* ---------- A8: mascaras OoT sin par MM (ownership ootMasksOwned compartido) ---------- */                       \
    X(FCI_SKULL_MASK, 1, FCI_F_NONE, RG_SKULL_MASK, RI_OOT_MASK_SKULL, "Skull Mask", "Skull Mask", "Skull Mask")       \
    X(FCI_SPOOKY_MASK, 1, FCI_F_NONE, RG_SPOOKY_MASK, RI_OOT_MASK_SPOOKY, "Spooky Mask", "Spooky Mask", "Spooky Mask") \
    X(FCI_GERUDO_MASK, 1, FCI_F_NONE, RG_GERUDO_MASK, RI_OOT_MASK_GERUDO, "Gerudo Mask", "Gerudo Mask", "Gerudo Mask") \
    /* ---------- A9: corazones (economia UNICA ~20) ---------- */                                                     \
    X(FCI_HEART_PIECE, 1, FCI_F_NONE, RG_PIECE_OF_HEART, RI_HEART_PIECE, "Piece of Heart", "Piece of Heart",           \
      "Heart Piece")                                                                                                   \
    X(FCI_HEART_CONTAINER, 1, FCI_F_NONE, RG_HEART_CONTAINER, RI_HEART_CONTAINER, "Heart Container",                   \
      "Heart Container", "Heart Container")                                                                            \
    /* ---------- A10: botellas compartidas (contenidos sin par quedan como items locales) ---------- */               \
    X(FCI_BOTTLE_EMPTY, 1, FCI_F_NONE, RG_EMPTY_BOTTLE, RI_BOTTLE_EMPTY, "Empty Bottle", "Empty Bottle",               \
      "Empty Bottle")                                                                                                  \
    X(FCI_BOTTLE_MILK, 1, FCI_F_NONE, RG_BOTTLE_WITH_MILK, RI_BOTTLE_MILK, "Bottle with Milk", "Bottle with Milk",     \
      "Bottle With Milk")                                                                                              \
    X(FCI_BOTTLE_RED_POTION, 1, FCI_F_NONE, RG_BOTTLE_WITH_RED_POTION, RI_BOTTLE_RED_POTION, "Bottle with Red Potion", \
      "Bottle with Red Potion", "Bottle With Red Potion")                                                              \
    X(FCI_BOTTLE_CHATEAU, 1, FCI_F_NONE, RG_CHATEAU_ROMANI, RI_BOTTLE_CHATEAU_ROMANI, "Bottle with Chateau Romani",    \
      "Chateau Romani", "Bottle With Chateau Romani")                                                                  \
    /* ========== CROSS-PLACEMENT: items exclusivos portados (2026-07-17) ========== */                                \
    /* -- MM boss souls (native MM) -- */                                                                              \
    X(FCI_MM_SOUL_GOHT, 1, FCI_F_NONE, RG_MM_SOUL_GOHT, RI_SOUL_BOSS_GOHT, "Soul of Goht", "Soul of Goht",             \
      "Soul of Goht")                                                                                                  \
    X(FCI_MM_SOUL_GYORG, 1, FCI_F_NONE, RG_MM_SOUL_GYORG, RI_SOUL_BOSS_GYORG, "Soul of Gyorg", "Soul of Gyorg",        \
      "Soul of Gyorg")                                                                                                 \
    X(FCI_MM_SOUL_MAJORA, 1, FCI_F_NONE, RG_MM_SOUL_MAJORA, RI_SOUL_BOSS_MAJORA, "Soul of Majora", "Soul of Majora",   \
      "Soul of Majora")                                                                                                \
    X(FCI_MM_SOUL_ODOLWA, 1, FCI_F_NONE, RG_MM_SOUL_ODOLWA, RI_SOUL_BOSS_ODOLWA, "Soul of Odolwa", "Soul of Odolwa",   \
      "Soul of Odolwa")                                                                                                \
    X(FCI_MM_SOUL_TWINMOLD, 1, FCI_F_NONE, RG_MM_SOUL_TWINMOLD, RI_SOUL_BOSS_TWINMOLD, "Soul of Twinmold",             \
      "Soul of Twinmold", "Soul of Twinmold")                                                                          \
    /* -- MM enemy souls (native MM) -- */                                                                             \
    X(FCI_MM_SOUL_ALIEN, 1, FCI_F_NONE, RG_MM_SOUL_ALIEN, RI_SOUL_ENEMY_ALIEN, "Soul of Aliens", "Soul of Aliens",     \
      "Soul of Aliens")                                                                                                \
    X(FCI_MM_SOUL_ARMOS, 1, FCI_F_NONE, RG_MM_SOUL_ARMOS, RI_SOUL_ENEMY_ARMOS, "Soul of Armos", "Soul of Armos",       \
      "Soul of Armos")                                                                                                 \
    X(FCI_MM_SOUL_BAD_BAT, 1, FCI_F_NONE, RG_MM_SOUL_BAD_BAT, RI_SOUL_ENEMY_BAD_BAT, "Soul of Bad Bats",               \
      "Soul of Bad Bats", "Soul of Bad Bats")                                                                          \
    X(FCI_MM_SOUL_BEAMOS, 1, FCI_F_NONE, RG_MM_SOUL_BEAMOS, RI_SOUL_ENEMY_BEAMOS, "Soul of Beamos", "Soul of Beamos",  \
      "Soul of Beamos")                                                                                                \
    X(FCI_MM_SOUL_BOE, 1, FCI_F_NONE, RG_MM_SOUL_BOE, RI_SOUL_ENEMY_BOE, "Soul of Boes", "Soul of Boes",               \
      "Soul of Boes")                                                                                                  \
    X(FCI_MM_SOUL_BUBBLE, 1, FCI_F_NONE, RG_MM_SOUL_BUBBLE, RI_SOUL_ENEMY_BUBBLE, "Soul of Bubbles",                   \
      "Soul of Bubbles", "Soul of Bubbles")                                                                            \
    X(FCI_MM_SOUL_CAPTAIN_KEETA, 1, FCI_F_NONE, RG_MM_SOUL_CAPTAIN_KEETA, RI_SOUL_ENEMY_CAPTAIN_KEETA,                 \
      "Soul of Captain Keeta", "Soul of Captain Keeta", "Soul of Captain Keeta")                                       \
    X(FCI_MM_SOUL_CHUCHU, 1, FCI_F_NONE, RG_MM_SOUL_CHUCHU, RI_SOUL_ENEMY_CHUCHU, "Soul of Chuchus",                   \
      "Soul of Chuchus", "Soul of Chuchus")                                                                            \
    X(FCI_MM_SOUL_DEATH_ARMOS, 1, FCI_F_NONE, RG_MM_SOUL_DEATH_ARMOS, RI_SOUL_ENEMY_DEATH_ARMOS,                       \
      "Soul of Death Armos", "Soul of Death Armos", "Soul of Death Armos")                                             \
    X(FCI_MM_SOUL_DEEP_PYTHON, 1, FCI_F_NONE, RG_MM_SOUL_DEEP_PYTHON, RI_SOUL_ENEMY_DEEP_PYTHON,                       \
      "Soul of Deep Pythons", "Soul of Deep Pythons", "Soul of Deep Pythons")                                          \
    X(FCI_MM_SOUL_DEKU_BABA, 1, FCI_F_NONE, RG_MM_SOUL_DEKU_BABA, RI_SOUL_ENEMY_DEKU_BABA, "Soul of Deku Babas",       \
      "Soul of Deku Babas", "Soul of Deku Babas")                                                                      \
    X(FCI_MM_SOUL_DEXIHAND, 1, FCI_F_NONE, RG_MM_SOUL_DEXIHAND, RI_SOUL_ENEMY_DEXIHAND, "Soul of Dexihands",           \
      "Soul of Dexihands", "Soul of Dexihands")                                                                        \
    X(FCI_MM_SOUL_DINOLFOS, 1, FCI_F_NONE, RG_MM_SOUL_DINOLFOS, RI_SOUL_ENEMY_DINOLFOS, "Soul of Dinolfos",            \
      "Soul of Dinolfos", "Soul of Dinolfos")                                                                          \
    X(FCI_MM_SOUL_DODONGO, 1, FCI_F_NONE, RG_MM_SOUL_DODONGO, RI_SOUL_ENEMY_DODONGO, "Soul of Dodongos",               \
      "Soul of Dodongos", "Soul of Dodongos")                                                                          \
    X(FCI_MM_SOUL_DRAGONFLY, 1, FCI_F_NONE, RG_MM_SOUL_DRAGONFLY, RI_SOUL_ENEMY_DRAGONFLY, "Soul of Dragonflies",      \
      "Soul of Dragonflies", "Soul of Dragonflies")                                                                    \
    X(FCI_MM_SOUL_EENO, 1, FCI_F_NONE, RG_MM_SOUL_EENO, RI_SOUL_ENEMY_EENO, "Soul of Eenos", "Soul of Eenos",          \
      "Soul of Eenos")                                                                                                 \
    X(FCI_MM_SOUL_EYEGORE, 1, FCI_F_NONE, RG_MM_SOUL_EYEGORE, RI_SOUL_ENEMY_EYEGORE, "Soul of Eyegores",               \
      "Soul of Eyegores", "Soul of Eyegores")                                                                          \
    X(FCI_MM_SOUL_FREEZARD, 1, FCI_F_NONE, RG_MM_SOUL_FREEZARD, RI_SOUL_ENEMY_FREEZARD, "Soul of Freezards",           \
      "Soul of Freezards", "Soul of Freezards")                                                                        \
    X(FCI_MM_SOUL_GARO, 1, FCI_F_NONE, RG_MM_SOUL_GARO, RI_SOUL_ENEMY_GARO, "Soul of Garos", "Soul of Garos",          \
      "Soul of Garos")                                                                                                 \
    X(FCI_MM_SOUL_GEKKO, 1, FCI_F_NONE, RG_MM_SOUL_GEKKO, RI_SOUL_ENEMY_GEKKO, "Soul of Gekkos", "Soul of Gekkos",     \
      "Soul of Gekkos")                                                                                                \
    X(FCI_MM_SOUL_GIANT_BEE, 1, FCI_F_NONE, RG_MM_SOUL_GIANT_BEE, RI_SOUL_ENEMY_GIANT_BEE, "Soul of Giant Bees",       \
      "Soul of Giant Bees", "Soul of Giant Bees")                                                                      \
    X(FCI_MM_SOUL_GOMESS, 1, FCI_F_NONE, RG_MM_SOUL_GOMESS, RI_SOUL_ENEMY_GOMESS, "Soul of Gomess", "Soul of Gomess",  \
      "Soul of Gomess")                                                                                                \
    X(FCI_MM_SOUL_GUAY, 1, FCI_F_NONE, RG_MM_SOUL_GUAY, RI_SOUL_ENEMY_GUAY, "Soul of Guays", "Soul of Guays",          \
      "Soul of Guays")                                                                                                 \
    X(FCI_MM_SOUL_HIPLOOP, 1, FCI_F_NONE, RG_MM_SOUL_HIPLOOP, RI_SOUL_ENEMY_HIPLOOP, "Soul of Hiploops",               \
      "Soul of Hiploops", "Soul of Hiploops")                                                                          \
    X(FCI_MM_SOUL_IGOS_DU_IKANA, 1, FCI_F_NONE, RG_MM_SOUL_IGOS_DU_IKANA, RI_SOUL_ENEMY_IGOS_DU_IKANA,                 \
      "Soul of Igos du Ikana", "Soul of Igos du Ikana", "Soul of Igos du Ikana")                                       \
    X(FCI_MM_SOUL_IRON_KNUCKLE, 1, FCI_F_NONE, RG_MM_SOUL_IRON_KNUCKLE, RI_SOUL_ENEMY_IRON_KNUCKLE,                    \
      "Soul of Iron Knuckles", "Soul of Iron Knuckles", "Soul of Iron Knuckles")                                       \
    X(FCI_MM_SOUL_KEESE, 1, FCI_F_NONE, RG_MM_SOUL_KEESE, RI_SOUL_ENEMY_KEESE, "Soul of Keese", "Soul of Keese",       \
      "Soul of Keese")                                                                                                 \
    X(FCI_MM_SOUL_LEEVER, 1, FCI_F_NONE, RG_MM_SOUL_LEEVER, RI_SOUL_ENEMY_LEEVER, "Soul of Leevers",                   \
      "Soul of Leevers", "Soul of Leevers")                                                                            \
    X(FCI_MM_SOUL_LIKE_LIKE, 1, FCI_F_NONE, RG_MM_SOUL_LIKE_LIKE, RI_SOUL_ENEMY_LIKE_LIKE, "Soul of Like Likes",       \
      "Soul of Like Likes", "Soul of Like Likes")                                                                      \
    X(FCI_MM_SOUL_MAD_SCRUB, 1, FCI_F_NONE, RG_MM_SOUL_MAD_SCRUB, RI_SOUL_ENEMY_MAD_SCRUB, "Soul of Mad Scrubs",       \
      "Soul of Mad Scrubs", "Soul of Mad Scrubs")                                                                      \
    X(FCI_MM_SOUL_NEJIRON, 1, FCI_F_NONE, RG_MM_SOUL_NEJIRON, RI_SOUL_ENEMY_NEJIRON, "Soul of Nejirons",               \
      "Soul of Nejirons", "Soul of Nejirons")                                                                          \
    X(FCI_MM_SOUL_OCTOROK, 1, FCI_F_NONE, RG_MM_SOUL_OCTOROK, RI_SOUL_ENEMY_OCTOROK, "Soul of Octoroks",               \
      "Soul of Octoroks", "Soul of Octoroks")                                                                          \
    X(FCI_MM_SOUL_PEAHAT, 1, FCI_F_NONE, RG_MM_SOUL_PEAHAT, RI_SOUL_ENEMY_PEAHAT, "Soul of Peahats",                   \
      "Soul of Peahats", "Soul of Peahats")                                                                            \
    X(FCI_MM_SOUL_PIRATE, 1, FCI_F_NONE, RG_MM_SOUL_PIRATE, RI_SOUL_ENEMY_PIRATE, "Soul of Pirates",                   \
      "Soul of Pirates", "Soul of Pirates")                                                                            \
    X(FCI_MM_SOUL_POE, 1, FCI_F_NONE, RG_MM_SOUL_POE, RI_SOUL_ENEMY_POE, "Soul of Poes", "Soul of Poes",               \
      "Soul of Poes")                                                                                                  \
    X(FCI_MM_SOUL_REDEAD, 1, FCI_F_NONE, RG_MM_SOUL_REDEAD, RI_SOUL_ENEMY_REDEAD, "Soul of Redeads",                   \
      "Soul of Redeads", "Soul of Redeads")                                                                            \
    X(FCI_MM_SOUL_SHELLBLADE, 1, FCI_F_NONE, RG_MM_SOUL_SHELLBLADE, RI_SOUL_ENEMY_SHELLBLADE, "Soul of Shellblades",   \
      "Soul of Shellblades", "Soul of Shellblades")                                                                    \
    X(FCI_MM_SOUL_SKULLFISH, 1, FCI_F_NONE, RG_MM_SOUL_SKULLFISH, RI_SOUL_ENEMY_SKULLFISH, "Soul of Skullfish",        \
      "Soul of Skullfish", "Soul of Skullfish")                                                                        \
    X(FCI_MM_SOUL_SKULLTULA, 1, FCI_F_NONE, RG_MM_SOUL_SKULLTULA, RI_SOUL_ENEMY_SKULLTULA, "Soul of Skulltulas",       \
      "Soul of Skulltulas", "Soul of Skulltulas")                                                                      \
    X(FCI_MM_SOUL_SNAPPER, 1, FCI_F_NONE, RG_MM_SOUL_SNAPPER, RI_SOUL_ENEMY_SNAPPER, "Soul of Snappers",               \
      "Soul of Snappers", "Soul of Snappers")                                                                          \
    X(FCI_MM_SOUL_STALCHILD, 1, FCI_F_NONE, RG_MM_SOUL_STALCHILD, RI_SOUL_ENEMY_STALCHILD, "Soul of Stalchildren",     \
      "Soul of Stalchildren", "Soul of Stalchildren")                                                                  \
    X(FCI_MM_SOUL_TAKKURI, 1, FCI_F_NONE, RG_MM_SOUL_TAKKURI, RI_SOUL_ENEMY_TAKKURI, "Soul of Takkuri",                \
      "Soul of Takkuri", "Soul of Takkuri")                                                                            \
    X(FCI_MM_SOUL_TEKTITE, 1, FCI_F_NONE, RG_MM_SOUL_TEKTITE, RI_SOUL_ENEMY_TEKTITE, "Soul of Tektites",               \
      "Soul of Tektites", "Soul of Tektites")                                                                          \
    X(FCI_MM_SOUL_WALLMASTER, 1, FCI_F_NONE, RG_MM_SOUL_WALLMASTER, RI_SOUL_ENEMY_WALLMASTER, "Soul of Wallmasters",   \
      "Soul of Wallmasters", "Soul of Wallmasters")                                                                    \
    X(FCI_MM_SOUL_WART, 1, FCI_F_NONE, RG_MM_SOUL_WART, RI_SOUL_ENEMY_WART, "Soul of Warts", "Soul of Warts",          \
      "Soul of Warts")                                                                                                 \
    X(FCI_MM_SOUL_WIZROBE, 1, FCI_F_NONE, RG_MM_SOUL_WIZROBE, RI_SOUL_ENEMY_WIZROBE, "Soul of Wizrobes",               \
      "Soul of Wizrobes", "Soul of Wizrobes")                                                                          \
    X(FCI_MM_SOUL_WOLFOS, 1, FCI_F_NONE, RG_MM_SOUL_WOLFOS, RI_SOUL_ENEMY_WOLFOS, "Soul of Wolfos", "Soul of Wolfos",  \
      "Soul of Wolfos")                                                                                                \
    /* -- MM boss remains (native MM) -- */                                                                            \
    X(FCI_MM_REMAINS_ODOLWA, 1, FCI_F_DUNGEON_REWARD, RG_MM_REMAINS_ODOLWA, RI_REMAINS_ODOLWA, "Odolwa's Remains",     \
      "Odolwa's Remains", "Odolwa's Remains")                                                                          \
    X(FCI_MM_REMAINS_GOHT, 1, FCI_F_DUNGEON_REWARD, RG_MM_REMAINS_GOHT, RI_REMAINS_GOHT, "Goht's Remains",             \
      "Goht's Remains", "Goht's Remains")                                                                              \
    X(FCI_MM_REMAINS_GYORG, 1, FCI_F_DUNGEON_REWARD, RG_MM_REMAINS_GYORG, RI_REMAINS_GYORG, "Gyorg's Remains",         \
      "Gyorg's Remains", "Gyorg's Remains")                                                                            \
    X(FCI_MM_REMAINS_TWINMOLD, 1, FCI_F_DUNGEON_REWARD, RG_MM_REMAINS_TWINMOLD, RI_REMAINS_TWINMOLD,                   \
      "Twinmold's Remains", "Twinmold's Remains", "Twinmold's Remains")                                                \
    /* -- MM stray fairies (native MM, multi-copia por mazmorra; modelo OoT compartido) -- */                          \
    X(FCI_MM_STRAY_FAIRY_CLOCK_TOWN, 1, FCI_F_NONE, RG_MM_STRAY_FAIRY, RI_CLOCK_TOWN_STRAY_FAIRY,                      \
      "Clock Town Stray Fairy", "Clock Town Stray Fairy", "Clock Town Stray Fairy")                                    \
    X(FCI_MM_STRAY_FAIRY_WOODFALL, 15, FCI_F_NONE, RG_MM_STRAY_FAIRY_WOODFALL, RI_WOODFALL_STRAY_FAIRY,                \
      "Woodfall Stray Fairy", "Woodfall Stray Fairy", "Woodfall Stray Fairy")                                          \
    X(FCI_MM_STRAY_FAIRY_SNOWHEAD, 15, FCI_F_NONE, RG_MM_STRAY_FAIRY_SNOWHEAD, RI_SNOWHEAD_STRAY_FAIRY,                \
      "Snowhead Stray Fairy", "Snowhead Stray Fairy", "Snowhead Stray Fairy")                                          \
    X(FCI_MM_STRAY_FAIRY_GREAT_BAY, 15, FCI_F_NONE, RG_MM_STRAY_FAIRY_GREAT_BAY, RI_GREAT_BAY_STRAY_FAIRY,             \
      "Great Bay Stray Fairy", "Great Bay Stray Fairy", "Great Bay Stray Fairy")                                       \
    X(FCI_MM_STRAY_FAIRY_STONE_TOWER, 15, FCI_F_NONE, RG_MM_STRAY_FAIRY_STONE_TOWER, RI_STONE_TOWER_STRAY_FAIRY,       \
      "Stone Tower Stray Fairy", "Stone Tower Stray Fairy", "Stone Tower Stray Fairy")                                 \
    /* -- MM-exclusive songs (native MM) -- */                                                                         \
    X(FCI_MM_SONG_SONATA, 1, FCI_F_SONG, RG_MM_SONG_SONATA, RI_SONG_SONATA, "Sonata of Awakening",                     \
      "Sonata of Awakening", "Sonata of Awakening")                                                                    \
    X(FCI_MM_SONG_LULLABY, 1, FCI_F_SONG, RG_MM_SONG_LULLABY, RI_SONG_LULLABY, "Goron Lullaby", "Goron Lullaby",       \
      "Goron Lullaby")                                                                                                 \
    X(FCI_MM_SONG_LULLABY_INTRO, 1, FCI_F_SONG, RG_MM_SONG_LULLABY_INTRO, RI_SONG_LULLABY_INTRO,                       \
      "Goron Lullaby Intro", "Goron Lullaby Intro", "Goron Lullaby Intro")                                             \
    X(FCI_MM_SONG_NOVA, 1, FCI_F_SONG, RG_MM_SONG_NOVA, RI_SONG_NOVA, "New Wave Bossa Nova", "New Wave Bossa Nova",    \
      "New Wave Bossa Nova")                                                                                           \
    X(FCI_MM_SONG_ELEGY, 1, FCI_F_SONG, RG_MM_SONG_ELEGY, RI_SONG_ELEGY, "Elegy of Emptiness", "Elegy of Emptiness",   \
      "Elegy of Emptiness")                                                                                            \
    X(FCI_MM_SONG_OATH, 1, FCI_F_SONG, RG_MM_SONG_OATH, RI_SONG_OATH, "Oath to Order", "Oath to Order",                \
      "Oath to Order")                                                                                                 \
    X(FCI_MM_SONG_SOARING, 1, FCI_F_SONG, RG_MM_SONG_SOARING, RI_SONG_SOARING, "Song of Soaring", "Song of Soaring",   \
      "Song of Soaring")                                                                                               \
    X(FCI_MM_SONG_HEALING, 1, FCI_F_SONG, RG_MM_SONG_HEALING, RI_SONG_HEALING, "Song of Healing", "Song of Healing",   \
      "Song of Healing")                                                                                               \
    X(FCI_MM_SONG_DOUBLE_TIME, 1, FCI_F_NONE, RG_MM_SONG_DOUBLE_TIME, RI_SONG_DOUBLE_TIME, "Song of Double Time",      \
      "Song of Double Time", "Song of Double Time")                                                                    \
    X(FCI_MM_SONG_INVERTED_TIME, 1, FCI_F_NONE, RG_MM_SONG_INVERTED_TIME, RI_SONG_INVERTED_TIME,                       \
      "Inverted Song of Time", "Inverted Song of Time", "Inverted Song of Time")                                       \
    /* -- MM owl statues (native MM) -- */                                                                             \
    X(FCI_MM_OWL_CLOCK_TOWN, 1, FCI_F_NONE, RG_MM_OWL_CLOCK_TOWN_SOUTH, RI_OWL_CLOCK_TOWN_SOUTH,                       \
      "Clock Town Owl Statue", "Clock Town Owl Statue", "Clock Town Owl Statue")                                       \
    X(FCI_MM_OWL_GREAT_BAY, 1, FCI_F_NONE, RG_MM_OWL_GREAT_BAY_COAST, RI_OWL_GREAT_BAY_COAST,                          \
      "Great Bay Coast Owl Statue", "Great Bay Coast Owl Statue", "Great Bay Coast Owl Statue")                        \
    X(FCI_MM_OWL_IKANA, 1, FCI_F_NONE, RG_MM_OWL_IKANA_CANYON, RI_OWL_IKANA_CANYON, "Ikana Canyon Owl Statue",         \
      "Ikana Canyon Owl Statue", "Ikana Canyon Owl Statue")                                                            \
    X(FCI_MM_OWL_MILK_ROAD, 1, FCI_F_NONE, RG_MM_OWL_MILK_ROAD, RI_OWL_MILK_ROAD, "Milk Road Owl Statue",              \
      "Milk Road Owl Statue", "Milk Road Owl Statue")                                                                  \
    X(FCI_MM_OWL_MOUNTAIN_VILLAGE, 1, FCI_F_NONE, RG_MM_OWL_MOUNTAIN_VILLAGE, RI_OWL_MOUNTAIN_VILLAGE,                 \
      "Mountain Village Owl Statue", "Mountain Village Owl Statue", "Mountain Village Owl Statue")                     \
    X(FCI_MM_OWL_SNOWHEAD, 1, FCI_F_NONE, RG_MM_OWL_SNOWHEAD, RI_OWL_SNOWHEAD, "Snowhead Owl Statue",                  \
      "Snowhead Owl Statue", "Snowhead Owl Statue")                                                                    \
    X(FCI_MM_OWL_SOUTHERN_SWAMP, 1, FCI_F_NONE, RG_MM_OWL_SOUTHERN_SWAMP, RI_OWL_SOUTHERN_SWAMP,                       \
      "Southern Swamp Owl Statue", "Southern Swamp Owl Statue", "Southern Swamp Owl Statue")                           \
    X(FCI_MM_OWL_STONE_TOWER, 1, FCI_F_NONE, RG_MM_OWL_STONE_TOWER, RI_OWL_STONE_TOWER, "Stone Tower Owl Statue",      \
      "Stone Tower Owl Statue", "Stone Tower Owl Statue")                                                              \
    X(FCI_MM_OWL_WOODFALL, 1, FCI_F_NONE, RG_MM_OWL_WOODFALL, RI_OWL_WOODFALL, "Woodfall Owl Statue",                  \
      "Woodfall Owl Statue", "Woodfall Owl Statue")                                                                    \
    X(FCI_MM_OWL_ZORA_CAPE, 1, FCI_F_NONE, RG_MM_OWL_ZORA_CAPE, RI_OWL_ZORA_CAPE, "Zora Cape Owl Statue",              \
      "Zora Cape Owl Statue", "Zora Cape Owl Statue")                                                                  \
    /* -- MM Tingle maps (native MM) -- */                                                                             \
    X(FCI_MM_TINGLE_CLOCK_TOWN, 1, FCI_F_NONE, RG_MM_TINGLE_MAP_CLOCK_TOWN, RI_TINGLE_MAP_CLOCK_TOWN,                  \
      "Tingle's Clock Town Map", "Tingle's Clock Town Map", "Tingle's Clock Town Map")                                 \
    X(FCI_MM_TINGLE_WOODFALL, 1, FCI_F_NONE, RG_MM_TINGLE_MAP_WOODFALL, RI_TINGLE_MAP_WOODFALL,                        \
      "Tingle's Woodfall Map", "Tingle's Woodfall Map", "Tingle's Woodfall Map")                                       \
    X(FCI_MM_TINGLE_SNOWHEAD, 1, FCI_F_NONE, RG_MM_TINGLE_MAP_SNOWHEAD, RI_TINGLE_MAP_SNOWHEAD,                        \
      "Tingle's Snowhead Map", "Tingle's Snowhead Map", "Tingle's Snowhead Map")                                       \
    X(FCI_MM_TINGLE_ROMANI_RANCH, 1, FCI_F_NONE, RG_MM_TINGLE_MAP_ROMANI_RANCH, RI_TINGLE_MAP_ROMANI_RANCH,            \
      "Tingle's Romani Ranch Map", "Tingle's Romani Ranch Map", "Tingle's Romani Ranch Map")                           \
    X(FCI_MM_TINGLE_GREAT_BAY, 1, FCI_F_NONE, RG_MM_TINGLE_MAP_GREAT_BAY, RI_TINGLE_MAP_GREAT_BAY,                     \
      "Tingle's Great Bay Map", "Tingle's Great Bay Map", "Tingle's Great Bay Map")                                    \
    X(FCI_MM_TINGLE_STONE_TOWER, 1, FCI_F_NONE, RG_MM_TINGLE_MAP_STONE_TOWER, RI_TINGLE_MAP_STONE_TOWER,               \
      "Tingle's Stone Tower Map", "Tingle's Stone Tower Map", "Tingle's Stone Tower Map")                              \
    /* -- MM trade/quest (native MM) -- */                                                                             \
    X(FCI_MM_MOONS_TEAR, 1, FCI_F_NONE, RG_MM_MOONS_TEAR, RI_MOONS_TEAR, "Moon's Tear", "Moon's Tear", "Moon's Tear")  \
    X(FCI_MM_DEED_LAND, 1, FCI_F_NONE, RG_MM_DEED_LAND, RI_DEED_LAND, "Town Title Deed", "Town Title Deed",            \
      "Land Title Deed")                                                                                               \
    X(FCI_MM_DEED_SWAMP, 1, FCI_F_NONE, RG_MM_DEED_SWAMP, RI_DEED_SWAMP, "Swamp Title Deed", "Swamp Title Deed",       \
      "Swamp Title Deed")                                                                                              \
    X(FCI_MM_DEED_MOUNTAIN, 1, FCI_F_NONE, RG_MM_DEED_MOUNTAIN, RI_DEED_MOUNTAIN, "Mountain Title Deed",               \
      "Mountain Title Deed", "Mountain Title Deed")                                                                    \
    X(FCI_MM_DEED_OCEAN, 1, FCI_F_NONE, RG_MM_DEED_OCEAN, RI_DEED_OCEAN, "Ocean Title Deed", "Ocean Title Deed",       \
      "Ocean Title Deed")                                                                                              \
    X(FCI_MM_ROOM_KEY, 1, FCI_F_NONE, RG_MM_ROOM_KEY, RI_ROOM_KEY, "Room Key", "Room Key", "Room Key")                 \
    X(FCI_MM_LETTER_TO_KAFEI, 1, FCI_F_NONE, RG_MM_LETTER_TO_KAFEI, RI_LETTER_TO_KAFEI, "Letter to Kafei",             \
      "Letter to Kafei", "Letter to Kafei")                                                                            \
    X(FCI_MM_LETTER_TO_MAMA, 1, FCI_F_NONE, RG_MM_LETTER_TO_MAMA, RI_LETTER_TO_MAMA, "Letter to Mama",                 \
      "Letter to Mama", "Letter to Mama")                                                                              \
    /* -- OoT medallions (native OoT) -- */                                                                            \
    X(FCI_OOT_MEDALLION_FOREST, 1, FCI_F_DUNGEON_REWARD, RG_FOREST_MEDALLION, RI_OOT_MEDALLION_FOREST,                 \
      "Forest Medallion", "Forest Medallion", "Forest Medallion")                                                      \
    X(FCI_OOT_MEDALLION_FIRE, 1, FCI_F_DUNGEON_REWARD, RG_FIRE_MEDALLION, RI_OOT_MEDALLION_FIRE, "Fire Medallion",     \
      "Fire Medallion", "Fire Medallion")                                                                              \
    X(FCI_OOT_MEDALLION_WATER, 1, FCI_F_DUNGEON_REWARD, RG_WATER_MEDALLION, RI_OOT_MEDALLION_WATER, "Water Medallion", \
      "Water Medallion", "Water Medallion")                                                                            \
    X(FCI_OOT_MEDALLION_SPIRIT, 1, FCI_F_DUNGEON_REWARD, RG_SPIRIT_MEDALLION, RI_OOT_MEDALLION_SPIRIT,                 \
      "Spirit Medallion", "Spirit Medallion", "Spirit Medallion")                                                      \
    X(FCI_OOT_MEDALLION_SHADOW, 1, FCI_F_DUNGEON_REWARD, RG_SHADOW_MEDALLION, RI_OOT_MEDALLION_SHADOW,                 \
      "Shadow Medallion", "Shadow Medallion", "Shadow Medallion")                                                      \
    X(FCI_OOT_MEDALLION_LIGHT, 1, FCI_F_DUNGEON_REWARD, RG_LIGHT_MEDALLION, RI_OOT_MEDALLION_LIGHT, "Light Medallion", \
      "Light Medallion", "Light Medallion")                                                                            \
    /* -- OoT spiritual stones (native OoT) -- */                                                                      \
    X(FCI_OOT_STONE_KOKIRI, 1, FCI_F_DUNGEON_REWARD, RG_KOKIRI_EMERALD, RI_OOT_STONE_KOKIRI_EMERALD,                   \
      "Kokiri's Emerald", "Kokiri's Emerald", "Kokiri's Emerald")                                                      \
    X(FCI_OOT_STONE_GORON, 1, FCI_F_DUNGEON_REWARD, RG_GORON_RUBY, RI_OOT_STONE_GORON_RUBY, "Goron's Ruby",            \
      "Goron's Ruby", "Goron's Ruby")                                                                                  \
    X(FCI_OOT_STONE_ZORA, 1, FCI_F_DUNGEON_REWARD, RG_ZORA_SAPPHIRE, RI_OOT_STONE_ZORA_SAPPHIRE, "Zora's Sapphire",    \
      "Zora's Sapphire", "Zora's Sapphire")                                                                            \
    /* -- OoT warp songs (native OoT) -- */                                                                            \
    X(FCI_OOT_SONG_MINUET, 1, FCI_F_SONG, RG_MINUET_OF_FOREST, RI_OOT_SONG_MINUET_OF_FOREST, "Minuet of Forest",       \
      "Minuet of Forest", "Minuet of Forest")                                                                          \
    X(FCI_OOT_SONG_BOLERO, 1, FCI_F_SONG, RG_BOLERO_OF_FIRE, RI_OOT_SONG_BOLERO_OF_FIRE, "Bolero of Fire",             \
      "Bolero of Fire", "Bolero of Fire")                                                                              \
    X(FCI_OOT_SONG_SERENADE, 1, FCI_F_SONG, RG_SERENADE_OF_WATER, RI_OOT_SONG_SERENADE_OF_WATER, "Serenade of Water",  \
      "Serenade of Water", "Serenade of Water")                                                                        \
    X(FCI_OOT_SONG_REQUIEM, 1, FCI_F_SONG, RG_REQUIEM_OF_SPIRIT, RI_OOT_SONG_REQUIEM_OF_SPIRIT, "Requiem of Spirit",   \
      "Requiem of Spirit", "Requiem of Spirit")                                                                        \
    X(FCI_OOT_SONG_NOCTURNE, 1, FCI_F_SONG, RG_NOCTURNE_OF_SHADOW, RI_OOT_SONG_NOCTURNE_OF_SHADOW,                     \
      "Nocturne of Shadow", "Nocturne of Shadow", "Nocturne of Shadow")                                                \
    X(FCI_OOT_SONG_PRELUDE, 1, FCI_F_SONG, RG_PRELUDE_OF_LIGHT, RI_OOT_SONG_PRELUDE_OF_LIGHT, "Prelude of Light",      \
      "Prelude of Light", "Prelude of Light")                                                                          \
    /* -- OoT adult/child trade (native OoT) -- */                                                                     \
    X(FCI_OOT_TRADE_WEIRD_EGG, 1, FCI_F_NONE, RG_WEIRD_EGG, RI_OOT_TRADE_WEIRD_EGG, "Weird Egg", "Weird Egg",          \
      "Weird Egg")                                                                                                     \
    X(FCI_OOT_TRADE_ZELDAS_LETTER, 1, FCI_F_NONE, RG_ZELDAS_LETTER, RI_OOT_TRADE_ZELDAS_LETTER, "Zelda's Letter",      \
      "Zelda's Letter", "Zelda's Letter")                                                                              \
    X(FCI_OOT_TRADE_POCKET_EGG, 1, FCI_F_NONE, RG_POCKET_EGG, RI_OOT_TRADE_POCKET_EGG, "Pocket Egg", "Pocket Egg",     \
      "Pocket Egg")                                                                                                    \
    X(FCI_OOT_TRADE_COJIRO, 1, FCI_F_NONE, RG_COJIRO, RI_OOT_TRADE_COJIRO, "Cojiro", "Cojiro", "Cojiro")               \
    X(FCI_OOT_TRADE_ODD_MUSHROOM, 1, FCI_F_NONE, RG_ODD_MUSHROOM, RI_OOT_TRADE_ODD_MUSHROOM, "Odd Mushroom",           \
      "Odd Mushroom", "Odd Mushroom")                                                                                  \
    X(FCI_OOT_TRADE_ODD_POTION, 1, FCI_F_NONE, RG_ODD_POTION, RI_OOT_TRADE_ODD_POTION, "Odd Potion", "Odd Potion",     \
      "Odd Potion")                                                                                                    \
    X(FCI_OOT_TRADE_POACHERS_SAW, 1, FCI_F_NONE, RG_POACHERS_SAW, RI_OOT_TRADE_POACHERS_SAW, "Poacher's Saw",          \
      "Poacher's Saw", "Poacher's Saw")                                                                                \
    X(FCI_OOT_TRADE_BROKEN_SWORD, 1, FCI_F_NONE, RG_BROKEN_SWORD, RI_OOT_TRADE_BROKEN_GORONS_SWORD,                    \
      "Broken Goron's Sword", "Broken Goron's Sword", "Broken Goron's Sword")                                          \
    X(FCI_OOT_TRADE_PRESCRIPTION, 1, FCI_F_NONE, RG_PRESCRIPTION, RI_OOT_TRADE_PRESCRIPTION, "Prescription",           \
      "Prescription", "Prescription")                                                                                  \
    X(FCI_OOT_TRADE_EYEBALL_FROG, 1, FCI_F_NONE, RG_EYEBALL_FROG, RI_OOT_TRADE_EYEBALL_FROG, "Eyeball Frog",           \
      "Eyeball Frog", "Eyeball Frog")                                                                                  \
    X(FCI_OOT_TRADE_EYEDROPS, 1, FCI_F_NONE, RG_EYEDROPS, RI_OOT_TRADE_EYEDROPS, "World's Finest Eyedrops",            \
      "World's Finest Eyedrops", "World's Finest Eyedrops")                                                            \
    X(FCI_OOT_TRADE_CLAIM_CHECK, 1, FCI_F_NONE, RG_CLAIM_CHECK, RI_OOT_TRADE_CLAIM_CHECK, "Claim Check",               \
      "Claim Check", "Claim Check")                                                                                    \
    /* -- OoT bean souls (native OoT) -- */                                                                            \
    X(FCI_OOT_BEAN_DMC, 1, FCI_F_NONE, RG_DEATH_MOUNTAIN_CRATER_BEAN_SOUL, RI_SOUL_OOT_BEAN_DEATH_MOUNTAIN_CRATER,     \
      "Death Mountain Crater Bean Soul", "Death Mountain Crater Bean Soul", "Death Mountain Crater Bean Soul")         \
    X(FCI_OOT_BEAN_DMT, 1, FCI_F_NONE, RG_DEATH_MOUNTAIN_TRAIL_BEAN_SOUL, RI_SOUL_OOT_BEAN_DEATH_MOUNTAIN_TRAIL,       \
      "Death Mountain Trail Bean Soul", "Death Mountain Trail Bean Soul", "Death Mountain Trail Bean Soul")            \
    X(FCI_OOT_BEAN_COLOSSUS, 1, FCI_F_NONE, RG_DESERT_COLOSSUS_BEAN_SOUL, RI_SOUL_OOT_BEAN_DESERT_COLOSSUS,            \
      "Desert Colossus Bean Soul", "Desert Colossus Bean Soul", "Desert Colossus Bean Soul")                           \
    X(FCI_OOT_BEAN_GV, 1, FCI_F_NONE, RG_GERUDO_VALLEY_BEAN_SOUL, RI_SOUL_OOT_BEAN_GERUDO_VALLEY,                      \
      "Gerudo Valley Bean Soul", "Gerudo Valley Bean Soul", "Gerudo Valley Bean Soul")                                 \
    X(FCI_OOT_BEAN_GRAVEYARD, 1, FCI_F_NONE, RG_GRAVEYARD_BEAN_SOUL, RI_SOUL_OOT_BEAN_GRAVEYARD,                       \
      "Graveyard Bean Soul", "Graveyard Bean Soul", "Graveyard Bean Soul")                                             \
    X(FCI_OOT_BEAN_KOKIRI, 1, FCI_F_NONE, RG_KOKIRI_FOREST_BEAN_SOUL, RI_SOUL_OOT_BEAN_KOKIRI_FOREST,                  \
      "Kokiri Forest Bean Soul", "Kokiri Forest Bean Soul", "Kokiri Forest Bean Soul")                                 \
    X(FCI_OOT_BEAN_LAKE_HYLIA, 1, FCI_F_NONE, RG_LAKE_HYLIA_BEAN_SOUL, RI_SOUL_OOT_BEAN_LAKE_HYLIA,                    \
      "Lake Hylia Bean Soul", "Lake Hylia Bean Soul", "Lake Hylia Bean Soul")                                          \
    X(FCI_OOT_BEAN_LOST_WOODS, 1, FCI_F_NONE, RG_LOST_WOODS_BEAN_SOUL, RI_SOUL_OOT_BEAN_LOST_WOODS,                    \
      "Lost Woods Bean Soul", "Lost Woods Bean Soul", "Lost Woods Bean Soul")                                          \
    X(FCI_OOT_BEAN_LW_BRIDGE, 1, FCI_F_NONE, RG_LOST_WOODS_BRIDGE_BEAN_SOUL, RI_SOUL_OOT_BEAN_LOST_WOODS_BRIDGE,       \
      "Lost Woods Bridge Bean Soul", "Lost Woods Bridge Bean Soul", "Lost Woods Bridge Bean Soul")                     \
    X(FCI_OOT_BEAN_ZORAS_RIVER, 1, FCI_F_NONE, RG_ZORAS_RIVER_BEAN_SOUL, RI_SOUL_OOT_BEAN_ZORAS_RIVER,                 \
      "Zora's River Bean Soul", "Zora's River Bean Soul", "Zora's River Bean Soul")                                    \
    /* -- OoT boss souls (native OoT) -- */                                                                            \
    X(FCI_OOT_BOSS_GOHMA, 1, FCI_F_NONE, RG_GOHMA_SOUL, RI_SOUL_OOT_BOSS_GOHMA, "Gohma's Soul", "Gohma's Soul",        \
      "Gohma's Soul")                                                                                                  \
    X(FCI_OOT_BOSS_KING_DODONGO, 1, FCI_F_NONE, RG_KING_DODONGO_SOUL, RI_SOUL_OOT_BOSS_KING_DODONGO,                   \
      "King Dodongo's Soul", "King Dodongo's Soul", "King Dodongo's Soul")                                             \
    X(FCI_OOT_BOSS_BARINADE, 1, FCI_F_NONE, RG_BARINADE_SOUL, RI_SOUL_OOT_BOSS_BARINADE, "Barinade's Soul",            \
      "Barinade's Soul", "Barinade's Soul")                                                                            \
    X(FCI_OOT_BOSS_PHANTOM_GANON, 1, FCI_F_NONE, RG_PHANTOM_GANON_SOUL, RI_SOUL_OOT_BOSS_PHANTOM_GANON,                \
      "Phantom Ganon's Soul", "Phantom Ganon's Soul", "Phantom Ganon's Soul")                                          \
    X(FCI_OOT_BOSS_VOLVAGIA, 1, FCI_F_NONE, RG_VOLVAGIA_SOUL, RI_SOUL_OOT_BOSS_VOLVAGIA, "Volvagia's Soul",            \
      "Volvagia's Soul", "Volvagia's Soul")                                                                            \
    X(FCI_OOT_BOSS_MORPHA, 1, FCI_F_NONE, RG_MORPHA_SOUL, RI_SOUL_OOT_BOSS_MORPHA, "Morpha's Soul", "Morpha's Soul",   \
      "Morpha's Soul")                                                                                                 \
    X(FCI_OOT_BOSS_BONGO_BONGO, 1, FCI_F_NONE, RG_BONGO_BONGO_SOUL, RI_SOUL_OOT_BOSS_BONGO_BONGO,                      \
      "Bongo Bongo's Soul", "Bongo Bongo's Soul", "Bongo Bongo's Soul")                                                \
    X(FCI_OOT_BOSS_TWINROVA, 1, FCI_F_NONE, RG_TWINROVA_SOUL, RI_SOUL_OOT_BOSS_TWINROVA, "Twinrova's Soul",            \
      "Twinrova's Soul", "Twinrova's Soul")                                                                            \
    X(FCI_OOT_BOSS_GANON, 1, FCI_F_NONE, RG_GANON_SOUL, RI_SOUL_OOT_BOSS_GANON, "Ganon's Soul", "Ganon's Soul",        \
      "Ganon's Soul")                                                                                                  \
    /* ========== CROSS-PLACEMENT: dungeon items OoT->MM (2026-07-17) ========== */                                    \
    X(FCI_OOT_SMALL_KEY_FOREST_TEMPLE, 5, FCI_F_NONE, RG_FOREST_TEMPLE_SMALL_KEY, RI_OOT_SMALL_KEY_FOREST_TEMPLE,      \
      "Forest Temple Small Key", "Forest Temple Small Key", "Forest Temple Small Key")                                 \
    X(FCI_OOT_SMALL_KEY_FIRE_TEMPLE, 8, FCI_F_NONE, RG_FIRE_TEMPLE_SMALL_KEY, RI_OOT_SMALL_KEY_FIRE_TEMPLE,            \
      "Fire Temple Small Key", "Fire Temple Small Key", "Fire Temple Small Key")                                       \
    X(FCI_OOT_SMALL_KEY_WATER_TEMPLE, 6, FCI_F_NONE, RG_WATER_TEMPLE_SMALL_KEY, RI_OOT_SMALL_KEY_WATER_TEMPLE,         \
      "Water Temple Small Key", "Water Temple Small Key", "Water Temple Small Key")                                    \
    X(FCI_OOT_SMALL_KEY_SPIRIT_TEMPLE, 5, FCI_F_NONE, RG_SPIRIT_TEMPLE_SMALL_KEY, RI_OOT_SMALL_KEY_SPIRIT_TEMPLE,      \
      "Spirit Temple Small Key", "Spirit Temple Small Key", "Spirit Temple Small Key")                                 \
    X(FCI_OOT_SMALL_KEY_SHADOW_TEMPLE, 5, FCI_F_NONE, RG_SHADOW_TEMPLE_SMALL_KEY, RI_OOT_SMALL_KEY_SHADOW_TEMPLE,      \
      "Shadow Temple Small Key", "Shadow Temple Small Key", "Shadow Temple Small Key")                                 \
    X(FCI_OOT_SMALL_KEY_BOTTOM_OF_THE_WELL, 3, FCI_F_NONE, RG_BOTTOM_OF_THE_WELL_SMALL_KEY,                            \
      RI_OOT_SMALL_KEY_BOTTOM_OF_THE_WELL, "Bottom of the Well Small Key", "Bottom of the Well Small Key",             \
      "Bottom of the Well Small Key")                                                                                  \
    X(FCI_OOT_SMALL_KEY_GERUDO_TRAINING_GROUND, 9, FCI_F_NONE, RG_GERUDO_TRAINING_GROUND_SMALL_KEY,                    \
      RI_OOT_SMALL_KEY_GERUDO_TRAINING_GROUND, "Training Ground Small Key", "Training Ground Small Key",               \
      "Training Ground Small Key")                                                                                     \
    X(FCI_OOT_SMALL_KEY_GERUDO_FORTRESS, 4, FCI_F_NONE, RG_GERUDO_FORTRESS_SMALL_KEY,                                  \
      RI_OOT_SMALL_KEY_GERUDO_FORTRESS, "Gerudo Fortress Small Key", "Gerudo Fortress Small Key",                      \
      "Gerudo Fortress Small Key")                                                                                     \
    X(FCI_OOT_SMALL_KEY_GANONS_CASTLE, 2, FCI_F_NONE, RG_GANONS_CASTLE_SMALL_KEY, RI_OOT_SMALL_KEY_GANONS_CASTLE,      \
      "Ganon's Castle Small Key", "Ganon's Castle Small Key", "Ganon's Castle Small Key")                              \
    X(FCI_OOT_BOSS_KEY_FOREST_TEMPLE, 1, FCI_F_NONE, RG_FOREST_TEMPLE_BOSS_KEY, RI_OOT_BOSS_KEY_FOREST_TEMPLE,         \
      "Forest Temple Boss Key", "Forest Temple Boss Key", "Forest Temple Boss Key")                                    \
    X(FCI_OOT_BOSS_KEY_FIRE_TEMPLE, 1, FCI_F_NONE, RG_FIRE_TEMPLE_BOSS_KEY, RI_OOT_BOSS_KEY_FIRE_TEMPLE,               \
      "Fire Temple Boss Key", "Fire Temple Boss Key", "Fire Temple Boss Key")                                          \
    X(FCI_OOT_BOSS_KEY_WATER_TEMPLE, 1, FCI_F_NONE, RG_WATER_TEMPLE_BOSS_KEY, RI_OOT_BOSS_KEY_WATER_TEMPLE,            \
      "Water Temple Boss Key", "Water Temple Boss Key", "Water Temple Boss Key")                                       \
    X(FCI_OOT_BOSS_KEY_SPIRIT_TEMPLE, 1, FCI_F_NONE, RG_SPIRIT_TEMPLE_BOSS_KEY, RI_OOT_BOSS_KEY_SPIRIT_TEMPLE,         \
      "Spirit Temple Boss Key", "Spirit Temple Boss Key", "Spirit Temple Boss Key")                                    \
    X(FCI_OOT_BOSS_KEY_SHADOW_TEMPLE, 1, FCI_F_NONE, RG_SHADOW_TEMPLE_BOSS_KEY, RI_OOT_BOSS_KEY_SHADOW_TEMPLE,         \
      "Shadow Temple Boss Key", "Shadow Temple Boss Key", "Shadow Temple Boss Key")                                    \
    X(FCI_OOT_BOSS_KEY_GANONS_CASTLE, 1, FCI_F_NONE, RG_GANONS_CASTLE_BOSS_KEY, RI_OOT_BOSS_KEY_GANONS_CASTLE,         \
      "Ganon's Castle Boss Key", "Ganon's Castle Boss Key", "Ganon's Castle Boss Key")                                 \
    X(FCI_OOT_MAP_DEKU_TREE, 1, FCI_F_NONE, RG_DEKU_TREE_MAP, RI_OOT_MAP_DEKU_TREE, "Great Deku Tree Map",             \
      "Great Deku Tree Map", "Great Deku Tree Map")                                                                    \
    X(FCI_OOT_MAP_DODONGOS_CAVERN, 1, FCI_F_NONE, RG_DODONGOS_CAVERN_MAP, RI_OOT_MAP_DODONGOS_CAVERN,                  \
      "Dodongo's Cavern Map", "Dodongo's Cavern Map", "Dodongo's Cavern Map")                                          \
    X(FCI_OOT_MAP_JABU_JABUS_BELLY, 1, FCI_F_NONE, RG_JABU_JABUS_BELLY_MAP, RI_OOT_MAP_JABU_JABUS_BELLY,               \
      "Jabu-Jabu's Belly Map", "Jabu-Jabu's Belly Map", "Jabu-Jabu's Belly Map")                                       \
    X(FCI_OOT_MAP_FOREST_TEMPLE, 1, FCI_F_NONE, RG_FOREST_TEMPLE_MAP, RI_OOT_MAP_FOREST_TEMPLE, "Forest Temple Map",   \
      "Forest Temple Map", "Forest Temple Map")                                                                        \
    X(FCI_OOT_MAP_FIRE_TEMPLE, 1, FCI_F_NONE, RG_FIRE_TEMPLE_MAP, RI_OOT_MAP_FIRE_TEMPLE, "Fire Temple Map",           \
      "Fire Temple Map", "Fire Temple Map")                                                                            \
    X(FCI_OOT_MAP_WATER_TEMPLE, 1, FCI_F_NONE, RG_WATER_TEMPLE_MAP, RI_OOT_MAP_WATER_TEMPLE, "Water Temple Map",       \
      "Water Temple Map", "Water Temple Map")                                                                          \
    X(FCI_OOT_MAP_SPIRIT_TEMPLE, 1, FCI_F_NONE, RG_SPIRIT_TEMPLE_MAP, RI_OOT_MAP_SPIRIT_TEMPLE, "Spirit Temple Map",   \
      "Spirit Temple Map", "Spirit Temple Map")                                                                        \
    X(FCI_OOT_MAP_SHADOW_TEMPLE, 1, FCI_F_NONE, RG_SHADOW_TEMPLE_MAP, RI_OOT_MAP_SHADOW_TEMPLE, "Shadow Temple Map",   \
      "Shadow Temple Map", "Shadow Temple Map")                                                                        \
    X(FCI_OOT_MAP_BOTTOM_OF_THE_WELL, 1, FCI_F_NONE, RG_BOTTOM_OF_THE_WELL_MAP, RI_OOT_MAP_BOTTOM_OF_THE_WELL,         \
      "Bottom of the Well Map", "Bottom of the Well Map", "Bottom of the Well Map")                                    \
    X(FCI_OOT_MAP_ICE_CAVERN, 1, FCI_F_NONE, RG_ICE_CAVERN_MAP, RI_OOT_MAP_ICE_CAVERN, "Ice Cavern Map",               \
      "Ice Cavern Map", "Ice Cavern Map")                                                                              \
    X(FCI_OOT_COMPASS_DEKU_TREE, 1, FCI_F_NONE, RG_DEKU_TREE_COMPASS, RI_OOT_COMPASS_DEKU_TREE,                        \
      "Great Deku Tree Compass", "Great Deku Tree Compass", "Great Deku Tree Compass")                                 \
    X(FCI_OOT_COMPASS_DODONGOS_CAVERN, 1, FCI_F_NONE, RG_DODONGOS_CAVERN_COMPASS, RI_OOT_COMPASS_DODONGOS_CAVERN,      \
      "Dodongo's Cavern Compass", "Dodongo's Cavern Compass", "Dodongo's Cavern Compass")                              \
    X(FCI_OOT_COMPASS_JABU_JABUS_BELLY, 1, FCI_F_NONE, RG_JABU_JABUS_BELLY_COMPASS, RI_OOT_COMPASS_JABU_JABUS_BELLY,   \
      "Jabu-Jabu's Belly Compass", "Jabu-Jabu's Belly Compass", "Jabu-Jabu's Belly Compass")                           \
    X(FCI_OOT_COMPASS_FOREST_TEMPLE, 1, FCI_F_NONE, RG_FOREST_TEMPLE_COMPASS, RI_OOT_COMPASS_FOREST_TEMPLE,            \
      "Forest Temple Compass", "Forest Temple Compass", "Forest Temple Compass")                                       \
    X(FCI_OOT_COMPASS_FIRE_TEMPLE, 1, FCI_F_NONE, RG_FIRE_TEMPLE_COMPASS, RI_OOT_COMPASS_FIRE_TEMPLE,                  \
      "Fire Temple Compass", "Fire Temple Compass", "Fire Temple Compass")                                             \
    X(FCI_OOT_COMPASS_WATER_TEMPLE, 1, FCI_F_NONE, RG_WATER_TEMPLE_COMPASS, RI_OOT_COMPASS_WATER_TEMPLE,               \
      "Water Temple Compass", "Water Temple Compass", "Water Temple Compass")                                          \
    X(FCI_OOT_COMPASS_SPIRIT_TEMPLE, 1, FCI_F_NONE, RG_SPIRIT_TEMPLE_COMPASS, RI_OOT_COMPASS_SPIRIT_TEMPLE,            \
      "Spirit Temple Compass", "Spirit Temple Compass", "Spirit Temple Compass")                                       \
    X(FCI_OOT_COMPASS_SHADOW_TEMPLE, 1, FCI_F_NONE, RG_SHADOW_TEMPLE_COMPASS, RI_OOT_COMPASS_SHADOW_TEMPLE,            \
      "Shadow Temple Compass", "Shadow Temple Compass", "Shadow Temple Compass")                                       \
    X(FCI_OOT_COMPASS_BOTTOM_OF_THE_WELL, 1, FCI_F_NONE, RG_BOTTOM_OF_THE_WELL_COMPASS,                                \
      RI_OOT_COMPASS_BOTTOM_OF_THE_WELL, "Bottom of the Well Compass", "Bottom of the Well Compass",                   \
      "Bottom of the Well Compass")                                                                                    \
    X(FCI_OOT_COMPASS_ICE_CAVERN, 1, FCI_F_NONE, RG_ICE_CAVERN_COMPASS, RI_OOT_COMPASS_ICE_CAVERN,                     \
      "Ice Cavern Compass", "Ice Cavern Compass", "Ice Cavern Compass")                                                \
    X(FCI_OOT_KEY_RING_FOREST_TEMPLE, 1, FCI_F_NONE, RG_FOREST_TEMPLE_KEY_RING, RI_OOT_KEY_RING_FOREST_TEMPLE,         \
      "Forest Temple Key Ring", "Forest Temple Key Ring", "Forest Temple Key Ring")                                    \
    X(FCI_OOT_KEY_RING_FIRE_TEMPLE, 1, FCI_F_NONE, RG_FIRE_TEMPLE_KEY_RING, RI_OOT_KEY_RING_FIRE_TEMPLE,               \
      "Fire Temple Key Ring", "Fire Temple Key Ring", "Fire Temple Key Ring")                                          \
    X(FCI_OOT_KEY_RING_WATER_TEMPLE, 1, FCI_F_NONE, RG_WATER_TEMPLE_KEY_RING, RI_OOT_KEY_RING_WATER_TEMPLE,            \
      "Water Temple Key Ring", "Water Temple Key Ring", "Water Temple Key Ring")                                       \
    X(FCI_OOT_KEY_RING_SPIRIT_TEMPLE, 1, FCI_F_NONE, RG_SPIRIT_TEMPLE_KEY_RING, RI_OOT_KEY_RING_SPIRIT_TEMPLE,         \
      "Spirit Temple Key Ring", "Spirit Temple Key Ring", "Spirit Temple Key Ring")                                    \
    X(FCI_OOT_KEY_RING_SHADOW_TEMPLE, 1, FCI_F_NONE, RG_SHADOW_TEMPLE_KEY_RING, RI_OOT_KEY_RING_SHADOW_TEMPLE,         \
      "Shadow Temple Key Ring", "Shadow Temple Key Ring", "Shadow Temple Key Ring")                                    \
    X(FCI_OOT_KEY_RING_BOTTOM_OF_THE_WELL, 1, FCI_F_NONE, RG_BOTTOM_OF_THE_WELL_KEY_RING,                              \
      RI_OOT_KEY_RING_BOTTOM_OF_THE_WELL, "Bottom of the Well Key Ring", "Bottom of the Well Key Ring",                \
      "Bottom of the Well Key Ring")                                                                                   \
    X(FCI_OOT_KEY_RING_GERUDO_TRAINING_GROUND, 1, FCI_F_NONE, RG_GERUDO_TRAINING_GROUND_KEY_RING,                      \
      RI_OOT_KEY_RING_GERUDO_TRAINING_GROUND, "Training Ground Key Ring", "Training Ground Key Ring",                  \
      "Training Ground Key Ring")                                                                                      \
    X(FCI_OOT_KEY_RING_GERUDO_FORTRESS, 1, FCI_F_NONE, RG_GERUDO_FORTRESS_KEY_RING, RI_OOT_KEY_RING_GERUDO_FORTRESS,   \
      "Gerudo Fortress Key Ring", "Gerudo Fortress Key Ring", "Gerudo Fortress Key Ring")                              \
    X(FCI_OOT_KEY_RING_GANONS_CASTLE, 1, FCI_F_NONE, RG_GANONS_CASTLE_KEY_RING, RI_OOT_KEY_RING_GANONS_CASTLE,         \
      "Ganon's Castle Key Ring", "Ganon's Castle Key Ring", "Ganon's Castle Key Ring")                                 \
    X(FCI_OOT_KEY_RING_TREASURE_GAME, 1, FCI_F_NONE, RG_TREASURE_GAME_KEY_RING, RI_OOT_KEY_RING_TREASURE_GAME,         \
      "Chest Game Key Ring", "Chest Game Key Ring", "Chest Game Key Ring")                                             \
    /* ========== CROSS-PLACEMENT: dungeon items MM->OoT (2026-07-17) ========== */                                    \
    X(FCI_MM_SMALL_KEY_WOODFALL, 1, FCI_F_NONE, RG_MM_SMALL_KEY_WOODFALL, RI_WOODFALL_SMALL_KEY, "Woodfall Small Key", \
      "Woodfall Small Key", "Woodfall Small Key")                                                                      \
    X(FCI_MM_SMALL_KEY_SNOWHEAD, 3, FCI_F_NONE, RG_MM_SMALL_KEY_SNOWHEAD, RI_SNOWHEAD_SMALL_KEY, "Snowhead Small Key", \
      "Snowhead Small Key", "Snowhead Small Key")                                                                      \
    X(FCI_MM_SMALL_KEY_GREAT_BAY, 1, FCI_F_NONE, RG_MM_SMALL_KEY_GREAT_BAY, RI_GREAT_BAY_SMALL_KEY,                    \
      "Great Bay Small Key", "Great Bay Small Key", "Great Bay Small Key")                                             \
    X(FCI_MM_SMALL_KEY_STONE_TOWER, 4, FCI_F_NONE, RG_MM_SMALL_KEY_STONE_TOWER, RI_STONE_TOWER_SMALL_KEY,              \
      "Stone Tower Small Key", "Stone Tower Small Key", "Stone Tower Small Key")                                       \
    X(FCI_MM_BOSS_KEY_WOODFALL, 1, FCI_F_NONE, RG_MM_BOSS_KEY_WOODFALL, RI_WOODFALL_BOSS_KEY, "Woodfall Boss Key",     \
      "Woodfall Boss Key", "Woodfall Boss Key")                                                                        \
    X(FCI_MM_BOSS_KEY_SNOWHEAD, 1, FCI_F_NONE, RG_MM_BOSS_KEY_SNOWHEAD, RI_SNOWHEAD_BOSS_KEY, "Snowhead Boss Key",     \
      "Snowhead Boss Key", "Snowhead Boss Key")                                                                        \
    X(FCI_MM_BOSS_KEY_GREAT_BAY, 1, FCI_F_NONE, RG_MM_BOSS_KEY_GREAT_BAY, RI_GREAT_BAY_BOSS_KEY, "Great Bay Boss Key", \
      "Great Bay Boss Key", "Great Bay Boss Key")                                                                      \
    X(FCI_MM_BOSS_KEY_STONE_TOWER, 1, FCI_F_NONE, RG_MM_BOSS_KEY_STONE_TOWER, RI_STONE_TOWER_BOSS_KEY,                 \
      "Stone Tower Boss Key", "Stone Tower Boss Key", "Stone Tower Boss Key")                                          \
    X(FCI_MM_MAP_WOODFALL, 1, FCI_F_NONE, RG_MM_MAP_WOODFALL, RI_WOODFALL_MAP, "Woodfall Map", "Woodfall Map",         \
      "Woodfall Map")                                                                                                  \
    X(FCI_MM_MAP_SNOWHEAD, 1, FCI_F_NONE, RG_MM_MAP_SNOWHEAD, RI_SNOWHEAD_MAP, "Snowhead Map", "Snowhead Map",         \
      "Snowhead Map")                                                                                                  \
    X(FCI_MM_MAP_GREAT_BAY, 1, FCI_F_NONE, RG_MM_MAP_GREAT_BAY, RI_GREAT_BAY_MAP, "Great Bay Map", "Great Bay Map",    \
      "Great Bay Map")                                                                                                 \
    X(FCI_MM_MAP_STONE_TOWER, 1, FCI_F_NONE, RG_MM_MAP_STONE_TOWER, RI_STONE_TOWER_MAP, "Stone Tower Map",             \
      "Stone Tower Map", "Stone Tower Map")                                                                            \
    X(FCI_MM_COMPASS_WOODFALL, 1, FCI_F_NONE, RG_MM_COMPASS_WOODFALL, RI_WOODFALL_COMPASS, "Woodfall Compass",         \
      "Woodfall Compass", "Woodfall Compass")                                                                          \
    X(FCI_MM_COMPASS_SNOWHEAD, 1, FCI_F_NONE, RG_MM_COMPASS_SNOWHEAD, RI_SNOWHEAD_COMPASS, "Snowhead Compass",         \
      "Snowhead Compass", "Snowhead Compass")                                                                          \
    X(FCI_MM_COMPASS_GREAT_BAY, 1, FCI_F_NONE, RG_MM_COMPASS_GREAT_BAY, RI_GREAT_BAY_COMPASS, "Great Bay Compass",     \
      "Great Bay Compass", "Great Bay Compass")                                                                        \
    X(FCI_MM_COMPASS_STONE_TOWER, 1, FCI_F_NONE, RG_MM_COMPASS_STONE_TOWER, RI_STONE_TOWER_COMPASS,                    \
      "Stone Tower Compass", "Stone Tower Compass", "Stone Tower Compass")                                             \
    X(FCI_CLAWSHOT, 1, FCI_F_NONE, RG_CLAWSHOT, RI_CLAWSHOT, "Clawshot", "Clawshot", "Clawshot")                       \
    X(FCI_NET, 1, FCI_F_NONE, RG_NET, RI_NET, "Net", "Net", "Bug-Catching Net")                                        \
    X(FCI_BOTTOMLESS_BOTTLE, 1, FCI_F_NONE, RG_BOTTOMLESS_BOTTLE, RI_BOTTOMLESS_BOTTLE, "Bottomless Bottle",           \
      "Bottomless Bottle", "Bottomless Bottle")                                                                        \
    X(FCI_DEKU_SHIELD, 1, FCI_F_NONE, RG_DEKU_SHIELD, RI_OOT_DEKU_SHIELD, "Deku Shield", "Deku Shield", "Deku Shield") \
    X(FCI_CLIMB, 1, FCI_F_NONE, RG_CLIMB, RI_OOT_ABILITY_CLIMB, "Climb", "Climb", "Climb")                             \
    X(FCI_CRAWL, 1, FCI_F_NONE, RG_CRAWL, RI_OOT_ABILITY_CRAWL, "Crawl", "Crawl", "Crawl")                             \
    X(FCI_SPEAK_DEKU, 1, FCI_F_NONE, RG_SPEAK_DEKU, RI_OOT_SPEAK_DEKU, "Deku Jabber Nut", "Deku Jabber Nut",           \
      "Deku Jabber Nut")                                                                                               \
    X(FCI_SPEAK_GERUDO, 1, FCI_F_NONE, RG_SPEAK_GERUDO, RI_OOT_SPEAK_GERUDO, "Gerudo Jabber Nut", "Gerudo Jabber Nut", \
      "Gerudo Jabber Nut")                                                                                             \
    X(FCI_SPEAK_GORON, 1, FCI_F_NONE, RG_SPEAK_GORON, RI_OOT_SPEAK_GORON, "Goron Jabber Nut", "Goron Jabber Nut",      \
      "Goron Jabber Nut")                                                                                              \
    X(FCI_SPEAK_HYLIAN, 1, FCI_F_NONE, RG_SPEAK_HYLIAN, RI_OOT_SPEAK_HYLIAN, "Hylian Jabber Nut", "Hylian Jabber Nut", \
      "Hylian Jabber Nut")                                                                                             \
    X(FCI_SPEAK_KOKIRI, 1, FCI_F_NONE, RG_SPEAK_KOKIRI, RI_OOT_SPEAK_KOKIRI, "Kokiri Jabber Nut", "Kokiri Jabber Nut", \
      "Kokiri Jabber Nut")                                                                                             \
    X(FCI_SPEAK_ZORA, 1, FCI_F_NONE, RG_SPEAK_ZORA, RI_OOT_SPEAK_ZORA, "Zora Jabber Nut", "Zora Jabber Nut",           \
      "Zora Jabber Nut")                                                                                               \
    X(FCI_OOT_GS_TOKEN, 100, FCI_F_NONE, RG_GOLD_SKULLTULA_TOKEN, RI_OOT_GS_TOKEN, "Gold Skulltula Token",             \
      "Gold Skulltula Token", "Gold Skulltula Token")                                                                  \
    X(FCI_RUTOS_LETTER, 1, FCI_F_NONE, RG_RUTOS_LETTER, RI_OOT_RUTOS_LETTER, "Bottle with Ruto's Letter",              \
      "Bottle with Ruto's Letter", "Bottle with Ruto's Letter")                                                        \
    X(FCI_BOTTLE_BIG_POE, 1, FCI_F_NONE, RG_BOTTLE_WITH_BIG_POE, RI_OOT_BOTTLE_BIG_POE, "Bottle with Big Poe",         \
      "Bottle with Big Poe", "Bottle with Big Poe")                                                                    \
    X(FCI_BOTTLE_BLUE_FIRE, 1, FCI_F_NONE, RG_BOTTLE_WITH_BLUE_FIRE, RI_OOT_BOTTLE_BLUE_FIRE, "Bottle with Blue Fire", \
      "Bottle with Blue Fire", "Bottle with Blue Fire")                                                                \
    X(FCI_BOTTLE_BLUE_POTION, 1, FCI_F_NONE, RG_BOTTLE_WITH_BLUE_POTION, RI_OOT_BOTTLE_BLUE_POTION,                    \
      "Bottle with Blue Potion", "Bottle with Blue Potion", "Bottle with Blue Potion")                                 \
    X(FCI_BOTTLE_BUGS, 1, FCI_F_NONE, RG_BOTTLE_WITH_BUGS, RI_OOT_BOTTLE_BUGS, "Bottle with Bugs", "Bottle with Bugs", \
      "Bottle with Bugs")                                                                                              \
    X(FCI_BOTTLE_FAIRY, 1, FCI_F_NONE, RG_BOTTLE_WITH_FAIRY, RI_OOT_BOTTLE_FAIRY, "Bottle with Fairy",                 \
      "Bottle with Fairy", "Bottle with Fairy")                                                                        \
    X(FCI_BOTTLE_FISH, 1, FCI_F_NONE, RG_BOTTLE_WITH_FISH, RI_OOT_BOTTLE_FISH, "Bottle with Fish", "Bottle with Fish", \
      "Bottle with Fish")                                                                                              \
    X(FCI_BOTTLE_GREEN_POTION, 1, FCI_F_NONE, RG_BOTTLE_WITH_GREEN_POTION, RI_OOT_BOTTLE_GREEN_POTION,                 \
      "Bottle with Green Potion", "Bottle with Green Potion", "Bottle with Green Potion")                              \
    X(FCI_BOTTLE_MAGIC_MUSHROOM, 1, FCI_F_NONE, RG_BOTTLE_WITH_MAGIC_MUSHROOM, RI_OOT_BOTTLE_MAGIC_MUSHROOM,           \
      "Bottle with Magic Mushroom", "Bottle with Magic Mushroom", "Bottle with Magic Mushroom")                        \
    X(FCI_BOTTLE_POE, 1, FCI_F_NONE, RG_BOTTLE_WITH_POE, RI_OOT_BOTTLE_POE, "Bottle with Poe", "Bottle with Poe",      \
      "Bottle with Poe")                                                                                               \
    X(FCI_MM_GS_TOKEN_SWAMP, 30, FCI_F_NONE, RG_MM_GS_TOKEN_SWAMP, RI_GS_TOKEN_SWAMP, "Swamp Gold Skulltula Token",    \
      "Swamp Gold Skulltula Token", "Swamp Gold Skulltula Token")                                                      \
    X(FCI_MM_GS_TOKEN_OCEAN, 30, FCI_F_NONE, RG_MM_GS_TOKEN_OCEAN, RI_GS_TOKEN_OCEAN, "Ocean Gold Skulltula Token",    \
      "Ocean Gold Skulltula Token", "Ocean Gold Skulltula Token")                                                      \
    X(FCI_MM_FROG_BLUE, 1, FCI_F_NONE, RG_MM_FROG_BLUE, RI_FROG_BLUE, "Blue Frog", "Blue Frog", "Blue Frog")           \
    X(FCI_MM_FROG_CYAN, 1, FCI_F_NONE, RG_MM_FROG_CYAN, RI_FROG_CYAN, "Cyan Frog", "Cyan Frog", "Cyan Frog")           \
    X(FCI_MM_FROG_PINK, 1, FCI_F_NONE, RG_MM_FROG_PINK, RI_FROG_PINK, "Pink Frog", "Pink Frog", "Pink Frog")           \
    X(FCI_MM_FROG_WHITE, 1, FCI_F_NONE, RG_MM_FROG_WHITE, RI_FROG_WHITE, "White Frog", "White Frog", "White Frog")     \
    X(FCI_MM_BOTTLE_GOLD_DUST, 1, FCI_F_NONE, RG_MM_BOTTLE_GOLD_DUST, RI_BOTTLE_GOLD_DUST, "Bottle With Gold Dust",    \
      "Bottle With Gold Dust", "Bottle With Gold Dust")                                                                \
    X(FCI_GREAT_SPIN, 1, FCI_F_NONE, RG_MM_GREAT_SPIN_ATTACK, RI_GREAT_SPIN_ATTACK, "Great Spin Attack",               \
      "Great Spin Attack", "Great Spin Attack")                                                                        \
    X(FCI_MM_TIME_DAY_1, 1, FCI_F_NONE, RG_MM_TIME_DAY_1, RI_TIME_DAY_1, "Time (Day 1)", "Time (Day 1)",               \
      "Time (Day 1)")                                                                                                  \
    X(FCI_MM_TIME_DAY_2, 1, FCI_F_NONE, RG_MM_TIME_DAY_2, RI_TIME_DAY_2, "Time (Day 2)", "Time (Day 2)",               \
      "Time (Day 2)")                                                                                                  \
    X(FCI_MM_TIME_DAY_3, 1, FCI_F_NONE, RG_MM_TIME_DAY_3, RI_TIME_DAY_3, "Time (Day 3)", "Time (Day 3)",               \
      "Time (Day 3)")                                                                                                  \
    X(FCI_MM_TIME_NIGHT_1, 1, FCI_F_NONE, RG_MM_TIME_NIGHT_1, RI_TIME_NIGHT_1, "Time (Night 1)", "Time (Night 1)",     \
      "Time (Night 1)")                                                                                                \
    X(FCI_MM_TIME_NIGHT_2, 1, FCI_F_NONE, RG_MM_TIME_NIGHT_2, RI_TIME_NIGHT_2, "Time (Night 2)", "Time (Night 2)",     \
      "Time (Night 2)")                                                                                                \
    X(FCI_MM_TIME_NIGHT_3, 1, FCI_F_NONE, RG_MM_TIME_NIGHT_3, RI_TIME_NIGHT_3, "Time (Night 3)", "Time (Night 3)",     \
      "Time (Night 3)")                                                                                                \
    X(FCI_TREASURE_GAME_SMALL_KEY, 6, FCI_F_NONE, RG_TREASURE_GAME_SMALL_KEY, RI_OOT_SMALL_KEY_TREASURE_GAME,          \
      "Chest Game Small Key", "Chest Game Small Key", "Chest Game Small Key")                                          \
    /* APPEND-ONLY desde aqui (regla 2): filas nuevas al final, nunca intercaladas — los FCI_* ids                   \
       indexan comboObtainedFc[] en el save de los dos juegos. */                                                      \
    /* Unica cancion de OoT sin contrapartida en MM hasta ahora; RI creado en 2ship para que cruce. */                 \
    X(FCI_SONG_ZELDAS_LULLABY, 1, FCI_F_SONG, RG_ZELDAS_LULLABY, RI_OOT_SONG_ZELDAS_LULLABY, "Zelda's Lullaby",        \
      "Zelda's Lullaby", "Zelda's Lullaby")                                                                            \
    /* La OTRA pluma de SoH: la ship-vanilla que vive en el slot de Nayru's Love. Check distinto del                   \
       Progressive Roc de Skijer (FCI_SKIJER_ROC), que ya tenia fila propia. */                                        \
    X(FCI_ROCS_FEATHER, 1, FCI_F_NONE, RG_ROCS_FEATHER, RI_OOT_ROCS_FEATHER, "Roc's Feather", "Roc's Feather",         \
      "Roc's Feather")                                                                                                 \
    /* Skill de OoT "can open chests": Termina no tiene esa puerta, asi que en MM es un no-op que                      \
       solo hay que poder encontrar — mismo trato que Climb/Crawl y los jabber nuts. */                              \
    X(FCI_OPEN_CHESTS, 2, FCI_F_NONE, RG_OPEN_CHEST, RI_OOT_ABILITY_CHESTS, "Open Chests", "Open Chests",              \
      "Open Chests")                                                                                                   \
    /* Bombchus: existen identicos en los dos juegos y nunca tuvieron fila. */                                         \
    X(FCI_BOMBCHU_5, 1, FCI_F_NOT_SHARED, RG_BOMBCHU_5, FCI_NO_ITEM, "Bombchus (5)", "Bombchus (5)", "")               \
    X(FCI_BOMBCHU_10, 1, FCI_F_NOT_SHARED, RG_BOMBCHU_10, FCI_NO_ITEM, "Bombchus (10)", "Bombchus (10)", "")           \
    /* Formas PROGRESIVAS de MM. 2ship elige una forma u otra por seed (RO_CLOCK_SHUFFLE_PROGRESSIVE                   \
       para el reloj, la opcion de Goron Lullaby para la cancion), asi que NUNCA coexisten con las                     \
       filas sueltas de arriba: el filtro de elegibilidad por lado deja viva la que toque. Sin estas                   \
       filas, la forma POR DEFECTO del pool de MM no podia cruzar (84 items por seed atrapados). */                    \
    X(FCI_MM_TIME_PROGRESSIVE, 6, FCI_F_NONE, RG_MM_TIME_PROGRESSIVE, RI_TIME_PROGRESSIVE, "Progressive Time",         \
      "Progressive Time", "Progressive Time")                                                                          \
    X(FCI_MM_SONG_LULLABY_PROGRESSIVE, 2, FCI_F_SONG, RG_MM_SONG_LULLABY_PROGRESSIVE, RI_PROGRESSIVE_LULLABY,          \
      "Progressive Goron Lullaby", "Progressive Goron Lullaby", "Progressive Goron Lullaby")                           \
    /* Last three page-2 equipment cells: playable in both games but with no cross-game identity until now. APPENDED   \
     * -- fcIds index comboObtainedFc[] in both saves. Skijer's NEI */                                                 \
    X(FCI_EXT_TRIDENT, 1, FCI_F_NONE, RG_EXT_TRIDENT, RI_OOT_EXT_TRIDENT, "Trident", "Trident", "Trident")             \
    X(FCI_EXT_CLIMB_BOOTS, 1, FCI_F_NONE, RG_EXT_CLIMB_BOOTS, RI_OOT_EXT_CLIMB_BOOTS, "Climb Boots", "Climb Boots",    \
      "Climb Boots")                                                                                                   \
    X(FCI_EXT_ROC_BOOTS, 1, FCI_F_NONE, RG_EXT_ROC_BOOTS, RI_OOT_EXT_ROC_BOOTS, "Roc's Boots", "Roc's Boots",          \
      "Roc's Boots")                                                                                                   \
    /* Elemental Wand family. The wand slot's SIX rods have clean 1:1 peers on both sides (RG_WAND_* in soh,           \
     * RI_OOT_NEI_WAND_* in 2ship), and per seed only ONE form is in a pool (the option picks wand-as-one-item OR six  \
     * rods), so supplying both rows never double-counts. The generic wand row covers the Medallions/Single modes.     \
     * Before these rows NONE of the seven could cross in combo. Skijer's NEI */                                       \
    X(FCI_ELEMENTAL_WAND, 1, FCI_F_NONE, RG_ELEMENTAL_WAND, RI_OOT_NEI_ELEMENTAL_WAND, "Elemental Wand",               \
      "Elemental Wand", "Elemental Wand")                                                                              \
    X(FCI_WAND_SAND_ROD, 1, FCI_F_NONE, RG_WAND_SAND_ROD, RI_OOT_NEI_WAND_SAND_ROD, "Sand Rod", "Sand Rod",            \
      "Sand Rod")                                                                                                      \
    X(FCI_WAND_TORNADO_ROD, 1, FCI_F_NONE, RG_WAND_TORNADO_ROD, RI_OOT_NEI_WAND_TORNADO_ROD, "Tornado Rod",            \
      "Tornado Rod", "Tornado Rod")                                                                                    \
    X(FCI_WAND_WATER_ROD, 1, FCI_F_NONE, RG_WAND_WATER_ROD, RI_OOT_NEI_WAND_WATER_ROD, "Water Rod", "Water Rod",       \
      "Water Rod")                                                                                                     \
    X(FCI_WAND_METEOR_ROD, 1, FCI_F_NONE, RG_WAND_METEOR_ROD, RI_OOT_NEI_WAND_METEOR_ROD, "Meteor Rod", "Meteor Rod",  \
      "Meteor Rod")                                                                                                    \
    X(FCI_WAND_STORM_ROD, 1, FCI_F_NONE, RG_WAND_STORM_ROD, RI_OOT_NEI_WAND_STORM_ROD, "Storm Rod", "Storm Rod",       \
      "Storm Rod")                                                                                                     \
    X(FCI_WAND_SHADOW_SCEPTER, 1, FCI_F_NONE, RG_WAND_SHADOW_SCEPTER, RI_OOT_NEI_WAND_SHADOW_SCEPTER,                  \
      "Shadow Scepter", "Shadow Scepter", "Shadow Scepter")                                                            \
    /* The four page-2 cells opened by the 2026-08-06 re-layout (behaviorless-for-now real items). Skijer's NEI */     \
    X(FCI_SHEIKAH_SLATE, 1, FCI_F_NONE, RG_SHEIKAH_SLATE, RI_OOT_NEI_SHEIKAH_SLATE, "Sheikah Slate", "Sheikah Slate",  \
      "Sheikah Slate")                                                                                                 \
    X(FCI_PHANTOM_HOURGLASS, 1, FCI_F_NONE, RG_PHANTOM_HOURGLASS, RI_OOT_NEI_PHANTOM_HOURGLASS, "Phantom Hourglass",   \
      "Phantom Hourglass", "Phantom Hourglass")                                                                        \
    X(FCI_SHADOW_CRYSTAL, 1, FCI_F_NONE, RG_SHADOW_CRYSTAL, RI_OOT_NEI_SHADOW_CRYSTAL, "Shadow Crystal",               \
      "Shadow Crystal", "Shadow Crystal")                                                                              \
    X(FCI_ROD_OF_SEASONS, 1, FCI_F_NONE, RG_ROD_OF_SEASONS, RI_OOT_NEI_ROD_OF_SEASONS, "Rod of Seasons",               \
      "Rod of Seasons", "Rod of Seasons")                                                                              \
    /* Sheikah Slate runes — sibling items over the slate cell (wand idiom). APPENDED: FCI ids are serialized.       \
     * Skijer's NEI */                                                                                                 \
    X(FCI_SLATE_RUNE_BOMB, 1, FCI_F_NONE, RG_SLATE_RUNE_BOMB, RI_OOT_NEI_SLATE_RUNE_BOMB, "Rune: Remote Bomb",         \
      "Rune: Remote Bomb", "Rune: Remote Bomb")                                                                        \
    X(FCI_SLATE_RUNE_MASTER_CYCLE, 1, FCI_F_NONE, RG_SLATE_RUNE_MASTER_CYCLE, RI_OOT_NEI_SLATE_RUNE_MASTER_CYCLE,      \
      "Rune: Master Cycle", "Rune: Master Cycle", "Rune: Master Cycle")                                                \
    X(FCI_SLATE_RUNE_STASIS, 1, FCI_F_NONE, RG_SLATE_RUNE_STASIS, RI_OOT_NEI_SLATE_RUNE_STASIS, "Rune: Stasis",        \
      "Rune: Stasis", "Rune: Stasis")                                                                                  \
    X(FCI_SLATE_RUNE_CRYONIS, 1, FCI_F_NONE, RG_SLATE_RUNE_CRYONIS, RI_OOT_NEI_SLATE_RUNE_CRYONIS, "Rune: Cryonis",    \
      "Rune: Cryonis", "Rune: Cryonis")                                                                                \
    X(FCI_MARIO_MASK, 1, FCI_F_NONE, RG_MARIO_MASK, RI_OOT_NEI_MARIO_MASK, "Mario Mask", "Mario Mask", "Mario Mask")   \
    X(FCI_SEASON_SPRING, 1, FCI_F_NONE, RG_SEASON_SPRING, RI_OOT_NEI_SEASON_SPRING, "Season: Spring",                  \
      "Season: Spring", "Season: Spring")                                                                              \
    X(FCI_SEASON_SUMMER, 1, FCI_F_NONE, RG_SEASON_SUMMER, RI_OOT_NEI_SEASON_SUMMER, "Season: Summer",                  \
      "Season: Summer", "Season: Summer")                                                                              \
    X(FCI_SEASON_AUTUMN, 1, FCI_F_NONE, RG_SEASON_AUTUMN, RI_OOT_NEI_SEASON_AUTUMN, "Season: Autumn",                  \
      "Season: Autumn", "Season: Autumn")                                                                              \
    X(FCI_SEASON_WINTER, 1, FCI_F_NONE, RG_SEASON_WINTER, RI_OOT_NEI_SEASON_WINTER, "Season: Winter",                  \
      "Season: Winter", "Season: Winter")

// -----------------------------------------------------------------------------
// Enum estable de items combo (generado por la lista; APPEND-ONLY)
// -----------------------------------------------------------------------------
typedef enum FcComboItemId {
#define X(id, chainLen, flags, rg, ri, comboName, ootName, mmName) id,
    FC_COMBO_ITEM_LIST(X)
#undef X
        FCI_MAX
} FcComboItemId;

// -----------------------------------------------------------------------------
// Metadata neutral (sin enums de juego) — usable por ambos lados y por el host
// para construir el pool, emitir spoilers y validar nombres al arranque.
// -----------------------------------------------------------------------------
typedef struct FcComboItemInfo {
    int fcId;               // FcComboItemId
    unsigned char chainLen; // 1 = unico; >1 = niveles de cadena progresiva (= copias pool v1)
    unsigned char flags;    // FCI_F_*
    const char* comboName;  // nombre neutral del combo
    const char* ootName;    // nombre de item SoH (spoiler/validador); "" = sin item nativo
    const char* mmName;     // spoilerName 2ship (spoiler/validador); "" = sin item nativo
} FcComboItemInfo;

static const FcComboItemInfo gFcComboItems[] = {
#define X(id, chainLen, flags, rg, ri, comboName, ootName, mmName) { id, chainLen, flags, comboName, ootName, mmName },
    FC_COMBO_ITEM_LIST(X)
#undef X
};

#define FC_COMBO_ITEM_COUNT ((int)(sizeof(gFcComboItems) / sizeof(gFcComboItems[0])))
