// SOH [Unbound] unbound/actors/<name>.json -> DeclaredActorType -> ActorDB. The rules each reader enforces are
// unbound-docs/SPEC.md §7.2.
#include "ActorRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "DeclaredActor.h"
#include "soh/ActorDB.h"
#include "soh/unbound/UnboundJson.h"
#include "soh/unbound/UnboundSchema.h"

namespace SOH::Unbound {
namespace {

namespace K = Schema;

// Registered types in id order: sTypes[i] is actor id kCustomActorIdBase + i. Filled once at startup, before any
// actor spawns, so the driver may keep pointers into it.
std::vector<DeclaredActorType> sTypes;

constexpr int64_t kMessageMax = 0xFFFE;

// The path under `key` with the prefix the asset loaders look for, added once whether or not the writer included
// it; "" when absent, empty or not a string.
std::string ReadPath(const Json& obj, const char* key) {
    std::string path = PathField(obj, key);
    if (path.starts_with(DeclaredActorType::kOtrPrefix)) {
        path.erase(0, DeclaredActorType::kOtrPrefixLength);
    }
    return path.empty() ? path : DeclaredActorType::kOtrPrefix + path;
}

// A distance in world units: absent or unreadable reads as `fallback`, negative as 0.
f32 Distance(const Json& obj, const char* key, double fallback = 0.0) {
    return std::max((f32)NumberField(obj, key, fallback), 0.0f);
}

s16 ClampS16(int64_t value) {
    return (s16)std::clamp<int64_t>(value, INT16_MIN, INT16_MAX);
}

// A number the entry may omit: false when absent or unreadable (SPEC.md §2 treats a wrong type as missing).
bool OptionalNumber(const Json& obj, const char* key, f32& out) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        return false;
    }
    double value = ToNumber(*it, NAN);
    if (std::isnan(value)) {
        return false;
    }
    out = (f32)value;
    return true;
}

// The file's path is the type's name: not a number (a scene's `id` would read it as one) and not already an actor's
// name.
bool CheckName(const std::string& name) {
    int64_t number = 0;
    if (name.empty() || ParseIntString(name, number)) {
        SPDLOG_ERROR("[Unbound] actor type '{}': not a valid actor type name", name);
        return false;
    }
    if (ActorDB::Instance->RetrieveId(name) >= 0) {
        SPDLOG_ERROR("[Unbound] actor type '{}': already names an actor", name);
        return false;
    }
    return true;
}

// False, logged, when `obj` has a key outside `known` (keys beginning with "$" are reserved, §2). `where` prefixes
// the key in the message: "" at the top level, "model." inside the model.
bool CheckKeys(const std::string& name, const Json& obj, const char* where, std::initializer_list<const char*> known) {
    for (const auto& [field, value] : obj.items()) {
        if (field.starts_with("$") || std::find(known.begin(), known.end(), field) != known.end()) {
            continue;
        }
        SPDLOG_ERROR("[Unbound] actor type '{}': \"{}{}\" is not a key this build knows", name, where, field);
        return false;
    }
    return true;
}

// Keys this version does not define reject the entry, at the top level and inside each object, so a build that
// predates a later key (base, params, script, model.lod) skips the type instead of spawning it without the
// behavior.
bool CheckAllKeys(const std::string& name, const Json& def) {
    return CheckKeys(name, def, "", { K::kName, K::kModel, K::kCollision, K::kTalk, K::kLook }) &&
           CheckKeys(name, Sub(def, K::kModel), "model.",
                     { K::kSkeleton, K::kAnimation, K::kFrame, K::kSpeed, K::kDisplayList, K::kTranslucent, K::kScale,
                       K::kYOffset, K::kSegments, K::kHideLimbs, K::kShadow, K::kCullRadius, K::kDrawDistance }) &&
           CheckKeys(name, Sub(def, K::kCollision), "collision.", { K::kRadius, K::kHeight, K::kYShift }) &&
           CheckKeys(name, Sub(def, K::kTalk), "talk.", { K::kMessage, K::kRange }) &&
           CheckKeys(name, Sub(def, K::kLook), "look.", { K::kLimb, K::kPivot, K::kRange, K::kTurnAxis, K::kNodAxis });
}

void ReadSegments(const std::string& name, const Json& segments, DeclaredActorType& type) {
    for (const auto& [segKey, value] : segments.items()) {
        int64_t segment = 0;
        std::string path = ReadPath(segments, segKey.c_str());
        if (!ParseIntString(segKey, segment) || segment < DeclaredActorType::kSegmentMin ||
            segment > DeclaredActorType::kSegmentMax || path.empty()) {
            SPDLOG_ERROR("[Unbound] actor type '{}': segment \"{}\" ignored (a segment 8-12 naming a texture path)",
                         name, segKey);
            continue;
        }
        type.segments.emplace_back((u8)segment, std::move(path));
    }
}

// Limbs whose own display list is not drawn: vanilla actors hide spare hands and props their code swaps in.
void ReadHideLimbs(const std::string& name, const Json& limbs, DeclaredActorType& type) {
    for (const Json& limb : limbs) {
        int64_t index = ToInt(limb, 0);
        if (index < 1) {
            SPDLOG_ERROR("[Unbound] actor type '{}': hideLimbs entry {} ignored (limbs are numbered from 1)", name,
                         limb.dump());
            continue;
        }
        type.hideLimbs.push_back((s32)index);
    }
}

// False, logged, unless the scale is positive and finite: zero draws nothing, a negative scale turns the model inside
// out, and the shadow size divides by it.
bool ReadScale(const std::string& name, const Json& model, DeclaredActorType& type) {
    type.scale = (f32)NumberField(model, K::kScale, 0.01);
    if (!std::isfinite(type.scale) || type.scale <= 0.0f) {
        SPDLOG_ERROR("[Unbound] actor type '{}': \"{}.{}\" must be a positive number", name, K::kModel, K::kScale);
        return false;
    }
    return true;
}

bool ReadModel(const std::string& name, const Json& def, DeclaredActorType& type) {
    auto it = def.find(K::kModel);
    if (it == def.end() || !it->is_object()) {
        SPDLOG_ERROR("[Unbound] actor type '{}' has no \"{}\"", name, K::kModel);
        return false;
    }
    const Json& model = *it;
    type.skeleton = ReadPath(model, K::kSkeleton);
    type.displayList = ReadPath(model, K::kDisplayList);
    if (type.skeleton.empty() == type.displayList.empty()) {
        SPDLOG_ERROR("[Unbound] actor type '{}': \"{}\" needs exactly one of \"{}\" or \"{}\"", name, K::kModel,
                     K::kSkeleton, K::kDisplayList);
        return false;
    }
    if (!type.skeleton.empty()) {
        // Required: an OoT skeleton has no usable rest pose. With every joint angle zero it folds up.
        type.animation = ReadPath(model, K::kAnimation);
        if (type.animation.empty()) {
            SPDLOG_ERROR("[Unbound] actor type '{}': a \"{}\" needs an \"{}\"", name, K::kSkeleton, K::kAnimation);
            return false;
        }
    }
    type.holdFrame = OptionalNumber(model, K::kFrame, type.frame);
    type.speed = (f32)NumberField(model, K::kSpeed, 1.0);
    type.translucent = Field(model, K::kTranslucent) != 0;
    if (!ReadScale(name, model, type)) {
        return false;
    }
    type.yOffset = (f32)NumberField(model, K::kYOffset);
    type.shadow = (f32)NumberField(model, K::kShadow);
    type.cullRadius = Distance(model, K::kCullRadius);
    type.drawDistance = Distance(model, K::kDrawDistance);
    ReadSegments(name, Sub(model, K::kSegments), type);
    ReadHideLimbs(name, SubArray(model, K::kHideLimbs), type);
    return true;
}

void ReadCollision(const Json& def, DeclaredActorType& type) {
    const Json& collision = Sub(def, K::kCollision);
    type.radius = ClampS16(Field(collision, K::kRadius));
    type.height = ClampS16(Field(collision, K::kHeight));
    type.yShift = ClampS16(Field(collision, K::kYShift));
}

// Reads after ReadCollision: the default range depends on the radius.
void ReadTalk(const std::string& name, const Json& def, DeclaredActorType& type) {
    auto it = def.find(K::kTalk);
    if (it == def.end() || !it->is_object()) {
        return;
    }
    type.talks = true;
    int64_t message = Field(*it, K::kMessage);
    if (message < 0 || message > kMessageMax) {
        SPDLOG_ERROR("[Unbound] actor type '{}': message {} is not a message id; placements must set params", name,
                     message);
        message = 0;
    }
    type.message = (u16)message;
    type.talkRange = Distance(*it, K::kRange, 50.0 + std::max<s16>(type.radius, 0));
}

// A look axis, normalized into `axis`; absent keeps the default already there. False, logged, unless it is three
// numbers of nonzero length.
bool ReadAxis(const std::string& name, const Json& look, const char* field, Vec3f& axis) {
    auto it = look.find(field);
    if (it == look.end()) {
        return true;
    }
    double v[3] = { NAN, NAN, NAN };
    if (it->is_array() && it->size() == 3) {
        for (size_t i = 0; i < 3; i++) {
            v[i] = ToNumber((*it)[i], NAN);
        }
    }
    double length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (!std::isfinite(length) || length == 0.0) {
        SPDLOG_ERROR("[Unbound] actor type '{}': \"{}.{}\" is not three numbers of nonzero length; the head will not "
                     "turn",
                     name, K::kLook, field);
        return false;
    }
    axis = { (f32)(v[0] / length), (f32)(v[1] / length), (f32)(v[2] / length) };
    return true;
}

// Unit axes closer than this sine of the angle between them are parallel: the head could turn but not nod.
constexpr f32 kParallelSine = 1e-3f;

bool Parallel(const Vec3f& a, const Vec3f& b) {
    f32 x = a.y * b.z - a.z * b.y;
    f32 y = a.z * b.x - a.x * b.z;
    f32 z = a.x * b.y - a.y * b.x;
    return std::sqrt(x * x + y * y + z * z) < kParallelSine;
}

// False, logged, when either axis is unusable or the two are parallel.
bool ReadLookAxes(const std::string& name, const Json& look, DeclaredActorType& type) {
    if (!ReadAxis(name, look, K::kTurnAxis, type.turnAxis) || !ReadAxis(name, look, K::kNodAxis, type.nodAxis)) {
        return false;
    }
    if (Parallel(type.turnAxis, type.nodAxis)) {
        SPDLOG_ERROR("[Unbound] actor type '{}': \"{}.{}\" and \"{}.{}\" are parallel; the head will not turn", name,
                     K::kLook, K::kTurnAxis, K::kLook, K::kNodAxis);
        return false;
    }
    return true;
}

bool ReadLook(const std::string& name, const Json& def, DeclaredActorType& type) {
    auto it = def.find(K::kLook);
    if (it == def.end() || !it->is_object()) {
        return true;
    }
    if (type.skeleton.empty()) {
        SPDLOG_ERROR("[Unbound] actor type '{}': \"{}\" needs a \"{}\"", name, K::kLook, K::kSkeleton);
        return false;
    }
    if (!it->contains(K::kLimb)) {
        SPDLOG_ERROR("[Unbound] actor type '{}': \"{}\" has no \"{}\"; the head will not turn", name, K::kLook,
                     K::kLimb);
        return true;
    }
    type.limb = (s32)Field(*it, K::kLimb);
    type.pivot = (f32)NumberField(*it, K::kPivot);
    type.lookRange = Distance(*it, K::kRange, 200.0);
    type.looks = ReadLookAxes(name, *it, type);
    return true;
}

bool ReadType(const std::string& name, const Json& def, DeclaredActorType& type) {
    if (!CheckName(name) || !CheckAllKeys(name, def) || !ReadModel(name, def, type)) {
        return false;
    }
    ReadCollision(def, type);
    ReadTalk(name, def, type);
    if (!ReadLook(name, def, type)) {
        return false;
    }
    type.name = name;
    type.displayName = def.contains(K::kName) && def[K::kName].is_string() ? def[K::kName].get<std::string>() : name;
    return true;
}

bool RegisterType(const std::string& name, const Json& def) {
    DeclaredActorType type;
    if (!ReadType(name, def, type)) {
        return false;
    }
    int32_t id = kCustomActorIdBase + (int32_t)sTypes.size();
    if (id > INT16_MAX) {
        SPDLOG_ERROR("[Unbound] actor type '{}': does not fit; actor ids are 16-bit", name);
        return false;
    }
    if (ActorDB::Instance->TryAddEntry(DeclaredActor_DBInit(type), id) == nullptr) {
        SPDLOG_ERROR("[Unbound] actor type '{}': cannot take id {:#x}, which another actor already uses", name, id);
        return false;
    }
    sTypes.push_back(std::move(type));
    SPDLOG_INFO("[Unbound] actor type '{}' -> id {:#x}", name, id);
    return true;
}

} // namespace

void LoadCustomActors() {
    size_t loaded = ForEachRegistryFile(K::kActorRegistryDir, "actor type", RegisterType);
    if (loaded == 0) {
        return;
    }
    SPDLOG_INFO("[Unbound] {}: registered {} custom actor type(s)", K::kActorRegistryDir, loaded);
}

const DeclaredActorType* GetDeclaredActorType(int32_t actorId) {
    int32_t index = actorId - kCustomActorIdBase;
    return index >= 0 && index < (int32_t)sTypes.size() ? &sTypes[index] : nullptr;
}

} // namespace SOH::Unbound
