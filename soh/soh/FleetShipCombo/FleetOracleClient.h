#pragma once
// FleetOracleClient.h — Cliente del oráculo lógico de MM (lado host / soh). C++ only.
//
// El oráculo vive en 2ship (FleetOracle.cpp) y responde con la lógica REAL del rando de MM.
// Transporte: fleet_oracle_req.json / fleet_oracle_resp.json en el dir del Ship host +
// seq/ack por FscShared reservedU[4]/[5] (FleetShipCombo.h).
//
// Uso (async, pensado para el fill de la Fase 2):
//   auto seq = FleetOracle_SendManifestRequest();
//   ... por frame: nlohmann::json resp; if (FleetOracle_TryGetResponse(seq, resp)) { ... }
//
// Smoke test integrado (sin Fase 2): CVarSetInteger("gFleetOracle.Test", N) con el combo activo:
//   1 = manifest (loguea nº de checks/pool), 2 = reachable con inventario vacío (sphere 0),
//   3 = reachable con TODOS los items FC compartidos al máximo. El resultado sale por SPDLOG
//   con prefijo [FleetOracleClient] y el CVar vuelve solo a 0.

#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

// Escribe la request y bumpea el seq. Devuelve el seq emitido, o 0 si no hay combo/shm.
unsigned long long FleetOracle_SendManifestRequest();

// fcItems: pares [FcComboItemId, count] (count = nivel de cadena asumido).
// mmItems: pares [spoilerName de 2ship ("RI_*"), count] para items nativos de MM.
// Items the restricted stages pre-placed at MM checks, as {RC_name: RI_name}. Sent with every
// reachability question so MM's crawl can HAND THEM OVER when it reaches the check holding one —
// exactly like a plando placement. Defined in FleetComboRando.cpp; empty when no category is on.
nlohmann::json FleetOracle_PrePlacedForOracle();

unsigned long long FleetOracle_SendReachableRequest(const std::vector<std::pair<int, int>>& fcItems,
                                                    const std::vector<std::pair<std::string, int>>& mmItems);

// cvars: pares [CVar de 2ship, valor int]. cvarsFloat: pares [CVar, valor float] (p.ej. volúmenes
// de MM que son float). El oráculo los aplica con CVarSetInteger/CVarSetFloat + CVarSave (op
// "setOptions"; whitelist gRando./gMods./gEnhancements./gCheats./gSettings.). Respuesta: {"applied": N}.
unsigned long long FleetOracle_SendSetOptionsRequest(const std::vector<std::pair<std::string, int>>& cvars,
                                                     const std::vector<std::pair<std::string, float>>& cvarsFloat = {});

// op "prepareSeed": el host ya escribió el spoiler combo en <ShipDir>/fleet_oracle_spoiler.json;
// 2ship lo instala en su randomizer/<fileName>, activa gRando y sincroniza el índice.
// Respuesta: {"spoilerIndex": N, "file": nombre}.
// ONE TURN of the delegated fill: MM places what it can of `toPlace` into checks reachable with
// `assumed` (promises already kept by OoT + what MM has placed so far), never reusing `usedChecks`.
// Response: { placed: {RC_name: RI_name}, remaining: [RI_name], blocked: bool, reachable: int }.
// `rngSeed` comes from the generation seed: same seed -> same split, independent of timing.
unsigned long long FleetOracle_SendFillTurnRequest(const std::vector<std::pair<std::string, int>>& assumed,
                                                   const std::vector<std::pair<std::string, int>>& toPlace,
                                                   const std::vector<std::string>& usedChecks, unsigned rngSeed);

unsigned long long FleetOracle_SendPrepareSeedRequest(const std::string& fileName);

// op "createSave": MM crea su save combo en el slot (overwrite en disco, estado vivo intacto)
// aplicando el spoiler preparado. name en ASCII (se codifica allá). Respuesta: {"randoApplied": b}.
unsigned long long FleetOracle_SendCreateSaveRequest(int slot, const std::string& name);

// ---- Pareado de saves (OoT manda, los files de MM son DERIVADOS) ----
// Un file combo es UN save viviendo en dos procesos: el slot N de OoT y el slot N de MM son el
// mismo. Estas dos ops mantienen esa igualdad y van por una COLA interna (una en vuelo, ninguna
// mientras el generador use el canal), así que se pueden llamar desde cualquier hook sin pisar una
// generación en curso.
//   deleteSave: OoT borró su file -> MM borra su mitad.
//   ensureSave: OoT va a jugar su file -> MM garantiza que su mitad existe Y es del mismo seed,
//               recreándola si falta o si quedó de otra seed (si no, cada juego jugaría su propio
//               fill sin que nada lo delate hasta horas después).
void FleetOracle_QueueDeleteSave(int slot);
void FleetOracle_QueueEnsureSave(int slot);
//   createSave:  OoT acaba de crear su file -> MM borra+recrea su mitad con el spoiler preparado.
//                Por la cola (no a pelo) para que respete el orden con un prepareSeed previo.
//   prepareSeed: instala en 2ship el spoiler MM de ESTA seed (va embebido en la request y se
//                escribe al bridge fleet/oracle_spoiler.json justo al enviarse). Se encola ANTES de
//                ensureSave/createSave al cargar un file combo cuyo .fleet existe, para que MM
//                reconstruya el slot con la seed correcta y no con "la última que preparó".
void FleetOracle_QueueCreateSave(int slot, const std::string& name);
void FleetOracle_QueuePrepareSeed(const std::string& fileName, const nlohmann::json& mmSpoiler);

// True cuando la respuesta para `seq` está lista y parseada en `out` (chequea ack + lee el archivo).
// No bloquea; llamar por frame hasta que devuelva true.
bool FleetOracle_TryGetResponse(unsigned long long seq, nlohmann::json& out);

// Resumen humano del último smoke test completado (gFleetOracle.Test) — lo muestra el tab Shared.
std::string FleetOracle_GetLastTestSummary();
