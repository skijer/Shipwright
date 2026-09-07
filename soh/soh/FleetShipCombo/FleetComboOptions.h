#pragma once
// =============================================================================
// FleetComboOptions.h — Combo Randomizer: tabla de OPCIONES/ESTADO NEI compartido
//
// REGLAS (mismas que FleetComboIds.h / FleetComboItems.h):
//  1. Este header DEBE ser byte-idéntico en ambos repos:
//       2ship2harkinian/mm/2s2h/FleetShipCombo/FleetComboOptions.h
//       Shipwright/soh/soh/FleetShipCombo/FleetComboOptions.h
//  2. APPEND-ONLY: nunca reordenar ni borrar filas.
//  3. NO incluir headers de juego aquí.
//
// POR QUÉ EXISTE
// El registro de ITEMS (FleetComboItems.h + comboObtainedFc) ya sincroniza todo
// lo que se OBTIENE por la vía normal de give: cada copia bumpea su contador y
// el otro juego materializa el déficit. Pero hay estado que NO es un item:
//  - flags de posesión escritos fuera del give (save editor, debug, cheats),
//  - PREFERENCIAS del jugador (qué categoría rastrea el Quartz, etc.).
// Eso no tenía ningún camino de sincronización y se quedaba en un solo juego.
//
// Esta tabla es ese camino. Cada fila es un campo u8 de NeiSaveData que viaja en
// el bloque "shared". Añadir una opción futura = UNA línea aquí (en los dos
// repos) y funciona en ambos sentidos sin tocar FleetSync.
//
// MODOS DE MERGE
//  FCO_MERGE_MAX    - desbloqueo/contador de una sola dirección: gana el valor
//                     más alto, nunca se pierde (posesión, niveles, contadores).
//  FCO_MERGE_NEWEST - preferencia del jugador: gana el último que la tocó, así
//                     que cambiarla en un juego se refleja en el otro.
// =============================================================================

#define FCO_MERGE_MAX 0
#define FCO_MERGE_NEWEST 1

// A page-2 cell that holds two items behind a wheel cannot encode "both owned", so ownership is a
// flag; without these entries the Shovel/Dominion wheel never crossed. FleetSync seeds the shared
// cell after this table is applied.
//
// clang-format off
// X(jsonKey, neiField, mergeMode)
#define FC_COMBO_OPTION_TABLE(X)                                  \
    X("quartzOwned",     quartzOwned,     FCO_MERGE_MAX)          \
    X("quartzCategory",  quartzCategory,  FCO_MERGE_NEWEST)       \
    X("quartzSubcat",    quartzSubcat,    FCO_MERGE_NEWEST)       \
    X("shovelOwned",     shovelOwned,     FCO_MERGE_MAX)          \
    X("dominionOwned",   dominionOwned,   FCO_MERGE_MAX)          \
    X("pokeballOwned",   pokeballOwned,   FCO_MERGE_MAX)          \
    X("bombArrowsOwned", bombArrowsOwned, FCO_MERGE_MAX)
// clang-format on
