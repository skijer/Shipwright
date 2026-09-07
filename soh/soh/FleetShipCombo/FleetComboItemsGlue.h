#pragma once
// FleetComboItemsGlue.h — API neutral del glue per-repo de FleetComboItems.h.
// Este header es byte-idéntico en ambos repos; el .cpp que lo implementa NO lo es:
//   soh   -> resuelve a RandomizerGet (RG_*)  [soh/soh/FleetShipCombo/FleetComboItemsGlue.cpp]
//   2ship -> resuelve a RandoItemId  (RI_*)   [mm/2s2h/FleetShipCombo/FleetComboItemsGlue.cpp]
// Mismo nombre de función en ambos lados para que el código combo sea simétrico.

#ifdef __cplusplus
extern "C" {
#endif

// FcComboItemId -> id nativo del juego local. FCI_NO_ITEM (-1) si este juego no tiene el item aún.
int FcCombo_NativeForItem(int fcId);

// id nativo local (RG/RI) -> FcComboItemId. FCI_NO_ITEM (-1) si no es un item compartido.
int FcCombo_ItemForNative(int nativeId);

// Nombre del token nativo LOCAL stringificado ("RG_*" en soh, "RI_*" en 2ship).
// "FCI_NO_ITEM" si este juego no tiene el item.
const char* FcCombo_NativeNameForItem(int fcId);

// Nombre del token del OTRO juego stringificado (soh devuelve "RI_*", 2ship devuelve "RG_*").
// Es lo que usa el host para emitir el spoiler del otro juego (el spoilerName de 2ship ES el
// nombre del enum, así que esta stringificación es exacta por construcción).
const char* FcCombo_PeerNameForItem(int fcId);

// Validador de arranque: compara los nombres de la tabla FC contra las tablas reales del juego
// y loguea cualquier drift (imprescindible tras pulls de upstream). Si la tabla de items del
// juego aún no está inicializada, difiere (es seguro re-llamarla).
void FcCombo_ValidateTable(void);

#ifdef __cplusplus
}
#endif
