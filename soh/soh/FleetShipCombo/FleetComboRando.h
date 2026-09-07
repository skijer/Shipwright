#pragma once
// FleetComboRando.h — Generador del Combo Randomizer (Fases 2+3, lado host / soh). C++ only.
//
// Arquitectura: NO reimplementa el fill de SoH. La generación combo llama al GenerateRandomizer
// nativo (3drando) con un hook de pre-colocación insertado en Fill() (fill.cpp): ahí el combo
// coloca (a) los items FC compartidos ambos-lados (pueden caer en checks de OoT O de MM) y
// (b) TODA la progresión nativa de MM (manifest del oráculo), usando assumed fill con
// reachability de OoT nativa (ReachabilitySearch) + reachability de MM por oráculo IPC.
// Después el fill nativo de SoH termina las etapas de OoT (shops, rewards, keys, junk) como
// siempre — los items FC que cayeron en MM se inyectan al StartingInventory lógico para que
// la lógica nativa los asuma obtenibles (sound: fueron colocados con assumed fill correcto).
//
// Fase 3 integrada: al terminar, escribe el spoiler 2S2H_RANDO_SPOILER de MM (checks RC_* →
// items RI_*, strings de enum) al puente fleet_oracle_spoiler.json y manda la op prepareSeed;
// 2ship lo instala y lo aplicará en su OnFileCreate al crear el save pareado.
//
// CVars combo: gFleetCombo.GoalMode (0 = Beat Both Bosses, 1 = Triforce Hunt),
//              gFleetCombo.TriforceTotal, gFleetCombo.TriforceRequired.
//
// Nada se fuerza: cada juego conserva sus opciones tal cual. El entrance shuffle de OoT funciona
// normal (lo único no modelado es barajar entradas ENTRE juegos: el portal ToT <-> Clock Town es
// fijo), y las categorías compartidas (canciones, dungeon rewards) solo mandan si tú las pones en su
// modo de spots — ahí la etapa restringida coloca antes que nada y las etapas nativas se apartan.

#include <string>
#include <vector>

// Lanza la generación combo en su propio thread (como GenerateRandomizerImgui). seedString
// vacío = semilla aleatoria. Devuelve false si ya hay una generación corriendo o no hay combo.
bool FleetCombo_StartGeneration(const std::string& seedString);

// Carga una seed combo ya generada desde un .fleet (fleet/<fileName>) SIN regenerar: aplica el
// spoiler OoT al Context y manda el spoiler MM a 2ship. Devuelve false si ocupado/sin combo.
// Loads a .fleet seed AND bakes it into the paired slot in BOTH games (OoT File N + MM File N share
// the seed). slot 0..2 = File 1..3; name is the save name (ASCII, encoded internally for OoT).
bool FleetCombo_LoadFleet(const std::string& fileName, int slot, const std::string& name);

// Lista los archivos .fleet disponibles en la carpeta fleet/.
std::vector<std::string> FleetCombo_ListFleetFiles();

bool FleetCombo_IsRunning();

// Última línea de estado/progreso para la UI (thread-safe, copia).
std::string FleetCombo_GetStatus();

// Restricted-category stages (shared Songs / Dungeon Rewards on their spots mode). Called from
// Fill() BEFORE every native placement stage, so those spots are already occupied when own-dungeon
// items, dungeon rewards, Link's Pocket and the rest run — they only fill empty locations, so they
// skip them by themselves and no per-stage reservation is needed. true = seguir; false = reintentar.
bool FleetCombo_RestrictedStageHook();

// Hook llamado desde Fill() (fill.cpp) en cada intento. true = seguir; false = reintentar.
// No-op (true) cuando no hay generación combo activa.
bool FleetCombo_PrePlacementHook();

// Shared Songs option: 0 = Own Game Logic, 1 = Song Spots, 2 = Anywhere. Always 0 when no combo
// generation is running, so the native song stages behave exactly as they always did.
int FleetCombo_SharedSongsMode();

// The progressive chain a concrete tier belongs to (RG_LONGSHOT -> RG_PROGRESSIVE_HOOKSHOT), or 0.
// Pure table lookup: valid whether or not a combo generation is running.
int FleetCombo_ChainAliasFor(int randomizerGet);

// True when the shared Dungeon Rewards option is on "Reward Spots" during a combo generation: the
// combo deals OoT's 6 medallions + 3 stones and MM's 4 remains across the 13 boss spots of BOTH
// games, so OoT's own reward stages must stand down and leave those locations empty.
bool FleetCombo_RestrictedDungeonRewards();

// True only while a combo seed is being generated. Lets native stages use combo numbers without
// affecting solo-OoT seeds (today: the bottle count, 8 instead of 4).
bool FleetCombo_IsComboGeneration();

// Bottles MM contributes that OoT cannot draw (Gold Dust, Chateau Romani). OoT's pool subtracts these
// from its 8 so the SHARED 8-slot bottle inventory adds up exactly. 0 outside a combo.
int FleetCombo_MmOnlyBottleCount();

// ---- Cross-game hints ----
// Readable area of the MM check where pre-placement put `riName` (MM's RI_* name), e.g. "Woodfall
// Temple". Empty string if the item is not placed in MM, or if the oracle manifest carried no areas
// (2ship builds older than checkAreas).
//
// Consumed by OoT's hint generation: without it an item placed in MM resolves to RC_UNKNOWN_CHECK ->
// "Invalid Location", the `areas` array comes out shorter than the locations one, and the template's
// leftover [[N]] tokens end up printed on screen.
std::string FleetCombo_GetMmAreaForItem(const std::string& riName);

// Same, but keyed by OoT's RandomizerGet (what hint generation deals in).
// Chain: RandomizerGet -> RI_ name (FC table) -> MM check (sMmPlacements) -> area.
std::string FleetCombo_GetMmAreaForOotItem(int randomizerGet);

// A hint names a CONCRETE item (RG_MASTER_SWORD); the combo may only carry the CHAIN that grants it
// (RG_PROGRESSIVE_MASTER_SWORD). Returns the chain's RandomizerGet in that case, 0 otherwise.
//
// Hint generation must translate BEFORE it searches: looking for the concrete id finds nothing,
// because no location holds it, and the hint degrades to "an Isolated Place" even when the item is
// sitting in Hyrule. Searching for the chain finds it in whichever world it landed in.
int FleetCombo_ChainForItem(int randomizerGet);

// true when MM area data is loaded (recent manifest + pre-placement done).
bool FleetCombo_HasMmHintData();

// NOTE: there is no incompatibility table any more. It existed for the monolithic pre-placement,
// which claimed any location it liked and so could not coexist with each game's restricted stages.
// The delegated fill runs AFTER those stages and takes only what they leave, so nothing has to be
// forced: every option holds as the player set it, and the cross-game goal is the one thing the combo
// owns. Skijer's NEI
