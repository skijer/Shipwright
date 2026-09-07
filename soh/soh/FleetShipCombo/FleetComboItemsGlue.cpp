// FleetComboItemsGlue.cpp (OoT side) — resuelve la tabla FC compartida a RandomizerGet (RG_*).
//
// Mirror del lado MM (2ship: mm/2s2h/FleetShipCombo/FleetComboItemsGlue.cpp, que resuelve a RI_*).
// La columna riToken de la X-macro NUNCA se expande aquí: los tokens RI_* no existen en soh
// y no hace falta que existan (los argumentos de macro no emitidos son solo tokens).

#include "FleetComboItemsGlue.h"
#include "FleetComboItems.h"
#include "FleetComboIds.h"  // FC_COMBO_OBTAINED_FC_SIZE
#include "FleetShipCombo.h" // FLEET_COMBO_EXPORT
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/ShipInit.hpp"

#include <cstring>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace {

// Columna RG de la tabla compartida (índices alineados 1:1 con gFcComboItems / FcComboItemId).
const int sFcNative[] = {
#define X(id, chainLen, flags, rg, ri, comboName, ootName, mmName) (int)(rg),
    FC_COMBO_ITEM_LIST(X)
#undef X
};
static_assert(sizeof(sFcNative) / sizeof(sFcNative[0]) == FCI_MAX, "FleetComboItems.h desalineado con FCI_MAX");
static_assert(FCI_MAX <= FC_COMBO_OBTAINED_FC_SIZE, "comboObtainedFc[] demasiado pequeño para la tabla FC");

// Tokens stringificados. Peer = tokens RI_* de 2ship: su spoilerName ES el nombre del enum, así
// que esta stringificación es exactamente lo que el host escribe en el spoiler de MM.
const char* sFcNativeName[] = {
#define X(id, chainLen, flags, rg, ri, comboName, ootName, mmName) #rg,
    FC_COMBO_ITEM_LIST(X)
#undef X
};
const char* sFcPeerName[] = {
#define X(id, chainLen, flags, rg, ri, comboName, ootName, mmName) #ri,
    FC_COMBO_ITEM_LIST(X)
#undef X
};

std::unordered_map<int, int>& ReverseMap() {
    static std::unordered_map<int, int> map = [] {
        std::unordered_map<int, int> m;
        for (int i = 0; i < FCI_MAX; i++) {
            if (sFcNative[i] != FCI_NO_ITEM) {
                m.emplace(sFcNative[i], i);
            }
        }
        return m;
    }();
    return map;
}

} // namespace

extern "C" int FcCombo_NativeForItem(int fcId) {
    if (fcId < 0 || fcId >= FCI_MAX) {
        return FCI_NO_ITEM;
    }
    return sFcNative[fcId];
}

extern "C" int FcCombo_ItemForNative(int nativeId) {
    auto& m = ReverseMap();
    auto it = m.find(nativeId);
    return it == m.end() ? FCI_NO_ITEM : it->second;
}

extern "C" const char* FcCombo_NativeNameForItem(int fcId) {
    if (fcId < 0 || fcId >= FCI_MAX) {
        return "FCI_NO_ITEM";
    }
    return sFcNativeName[fcId];
}

extern "C" const char* FcCombo_PeerNameForItem(int fcId) {
    if (fcId < 0 || fcId >= FCI_MAX) {
        return "FCI_NO_ITEM";
    }
    return sFcPeerName[fcId];
}

extern "C" void FcCombo_ValidateTable(void) {
    // La tabla se llena en StaticData::InitItemTable(); si aún no corrió, difiere (re-llamable).
    if (Rando::StaticData::RetrieveItem(RG_PROGRESSIVE_HOOKSHOT).GetName().GetEnglish().empty()) {
        SPDLOG_WARN("[FcCombo] tabla de items de soh aún no inicializada — validación diferida");
        return;
    }
    int issues = 0;
    for (int i = 0; i < FC_COMBO_ITEM_COUNT; i++) {
        const FcComboItemInfo& row = gFcComboItems[i];
        int rg = sFcNative[i];
        if (rg == FCI_NO_ITEM) {
            continue;
        }
        const std::string& english = Rando::StaticData::RetrieveItem((RandomizerGet)rg).GetName().GetEnglish();
        if (english.empty()) {
            SPDLOG_WARN("[FcCombo] drift OoT: {} — el RG {} no tiene fila en itemTable", row.comboName, rg);
            issues++;
            continue;
        }
        if (row.ootName[0] != '\0' && english != row.ootName) {
            SPDLOG_WARN("[FcCombo] drift OoT: {} — tabla dice \"{}\", juego dice \"{}\"", row.comboName, row.ootName,
                        english);
            issues++;
        }
    }
    SPDLOG_INFO("[FcCombo] validación OoT: {} items compartidos, {} discrepancias", FC_COMBO_ITEM_COUNT, issues);
}

static void RegisterFcComboItems() {
    FcCombo_ValidateTable();
}

static RegisterShipInitFunc initFcComboItems(RegisterFcComboItems, {});

// ComboShip: the shared-item pairs for the launcher's cross-world fill, as
// [{"oot": <soh itemTable English name>, "mm": <2ship spoiler name>, "chain": <n>}]. Only rows with
// an item on BOTH sides are pairs; the names are exactly what each game's static-data dump emits.
extern "C" FLEET_COMBO_EXPORT const char* SOH_DumpSharedItemPairs(void) {
    static std::string cached;
    nlohmann::json pairs = nlohmann::json::array();
    for (int fcId = 0; fcId < FCI_MAX; fcId++) {
        const char* ootName = gFcComboItems[fcId].ootName;
        const char* mmName = sFcPeerName[fcId];
        if (ootName == nullptr || ootName[0] == '\0' || std::strcmp(mmName, "FCI_NO_ITEM") == 0) {
            continue;
        }
        pairs.push_back({ { "oot", ootName }, { "mm", mmName }, { "chain", gFcComboItems[fcId].chainLen } });
    }
    cached = pairs.dump();
    return cached.c_str();
}
