#include <emscripten.h>
#include <libultraship/libultraship.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include "soh/OTRGlobals.h"
#include "soh/unbound/UnboundExporter.h"
#include "soh/unbound/UnboundSchema.h"

extern "C" {
#include "variables.h"
}

namespace {

enum ConvertCode {
    kConvertOk = 0,
    kConvertAlreadyRan = -1,
    kConvertNoArchive = -2,
    kConvertWrongVersion = -3,
    kConvertFailed = -4,
};

struct ConvertResult {
    int code = kConvertOk;
    std::string error;
    SOH::Unbound::ExportReport report;
};

ConvertResult sResult;
bool sRan = false;
Ship::Context* sContext = nullptr;

void StartResourceManager(const std::string& sourcePath) {
    sContext = Ship::Context::CreateUninitializedInstance("Ship of Harkinian", "soh", "shipofharkinian.json");
    sContext->InitConfiguration();
    sContext->InitConsoleVariables();
    sContext->InitResourceManager({ sourcePath }, {}, 0, true);
    SOH_RegisterResourceFactories(sContext->GetResourceManager()->GetResourceLoader());
}

ConvertResult CheckSourceArchive() {
    ConvertResult result;
    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    if (!archives->IsLoaded() || archives->GetGameVersions().empty()) {
        result.code = kConvertNoArchive;
        result.error = "not an OoT game archive: supply the oot.o2r (or oot-mq.o2r) that soh-extract produced";
        return result;
    }
    if (archives->HasFile(SOH::Unbound::Schema::kManifestPath)) {
        result.code = kConvertNoArchive;
        result.error = "this is already an Unbound base (oot-unbound.o2r); convert the oot.o2r it came from";
        return result;
    }
    int major = SOH::Unbound::MountedPortVersion().major;
    if (major != gBuildVersionMajor) {
        result.code = kConvertWrongVersion;
        result.error = "the archive was made by an incompatible version of SoH (port version " + std::to_string(major) +
                       ", this converter is " + std::to_string(gBuildVersionMajor) + "); extract the ROM again";
    }
    return result;
}

ConvertResult Export(const std::string& outPath) {
    ConvertResult result;
    try {
        result.report = SOH::Unbound::ExportArchive(outPath);
    } catch (const std::exception& e) { result.report.error = std::string("the converter threw: ") + e.what(); }
    if (!result.report.ok) {
        result.code = kConvertFailed;
        result.error = result.report.error;
    }
    return result;
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int Unbound_ConvertArchive(const char* sourcePath, const char* outPath) {
    if (sRan) {
        sResult = {};
        sResult.code = kConvertAlreadyRan;
        sResult.error = "this converter instance has already run; create a new one per archive";
        return sResult.code;
    }
    sRan = true;
    StartResourceManager(sourcePath);
    sResult = CheckSourceArchive();
    if (sResult.code == kConvertOk) {
        sResult = Export(outPath);
    }
    if (sResult.code != kConvertOk) {
        SPDLOG_ERROR("[Unbound convert] {}", sResult.error);
    }
    return sResult.code;
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* Unbound_ConvertResultJson(void) {
    static std::string json;
    const auto& r = sResult.report;
    json = nlohmann::json{
        { "code", sResult.code }, { "error", sResult.error }, { "converter", SOH::Unbound::ConverterName() },
        { "scenes", r.scenes },   { "rooms", r.rooms },       { "messages", r.messages },
        { "copied", r.copied },   { "failures", r.failures }
    }.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    return json.c_str();
}
