#pragma once
#include <functional>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include <libultraship/libultra.h>
#include "z64math.h"

namespace SOH::Unbound {

using Json = nlohmann::json;

struct DocumentError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void MergeJson(Json& base, const Json& overlay);

Json LoadMergedJson(const std::string& path);

size_t ForEachRegistryEntry(const std::string& path, const char* what,
                            const std::function<bool(const std::string& key, const Json& entry)>& add);

// `dir` is matched as a glob prefix, so it must not contain `*`, `?`, `[` or `\`.
size_t ForEachRegistryFile(const std::string& dir, const char* what,
                           const std::function<bool(const std::string& name, const Json& entry)>& add);

std::vector<std::string> ListKeys(const Json& obj);

std::vector<std::string> PositionalKeys(const Json& list, const std::string& what);

// Static empty values are returned for a missing key so callers never index a non-object (nlohmann's value() throws).
const Json& Sub(const Json& obj, const char* key);
const Json& SubArray(const Json& obj, const char* key);

std::string SchemaOf(const Json& doc);

bool ParseSchema(const std::string& schema, std::string& type, int& version);

bool ParseIntString(const std::string& text, int64_t& out);
bool ParseNumberString(const std::string& text, double& out);
int64_t ToInt(const Json& value, int64_t fallback = 0);
double ToNumber(const Json& value, double fallback = 0.0);

int64_t Field(const Json& obj, const char* key, int64_t fallback = 0);
double NumberField(const Json& obj, const char* key, double fallback = 0.0);
s32 IntegralField(const Json& obj, const char* key, s32 fallback = 0);
std::string PathField(const Json& obj, const char* key);

Vec3s ReadVec3s(const Json& v);
Vec3f ReadVec3f(const Json& v);
Vec3i ReadVec3i(const Json& v);

template <typename T> void ReadRgb(const Json& v, T* out) {
    if (v.is_array() && v.size() >= 3) {
        out[0] = (T)ToInt(v[0]);
        out[1] = (T)ToInt(v[1]);
        out[2] = (T)ToInt(v[2]);
    }
}

std::vector<char> LoadBulk(const std::string& path);

} // namespace SOH::Unbound
