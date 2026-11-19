#pragma once
#include "archive.h"
#include "keep_textures.h"
#include <map>
#include <set>
#include <string_view>

namespace pak {
const char* BaseSkeletonPath(bool childSized);
const char* ResolveKeepTexture(const char* name);

static constexpr unsigned kLimbCount = 21;
static constexpr uint32_t kManifest = 0x5000;
static constexpr unsigned kLegLimbs[] = { 3, 4, 5, 10 };

static const char* const kOotEyes[] = { "EyesOpen",      "EyesHalf",  "EyesClosedf", "EyesRollLeft",
                                        "EyesRollRight", "EyesShock", "EyesUnk1",    "EyesUnk2" };
static const char* const kMmEyes[] = { "EyesOpen", "EyesHalf", "EyesClosed", "EyesRight",
                                       "EyesLeft", "EyesUp",   "EyesDown",   "EyesWincing" };
static const char* const kOotMouths[] = { "Mouth1", "Mouth2", "Mouth3", "Mouth4" };
static const char* const kMmMouths[] = { "MouthClosed", "MouthHalf", "MouthOpen", "MouthSmile" };

static int FaceIndex(const char* texture, bool eyes) {
    const char* const* oot = eyes ? kOotEyes : kOotMouths;
    const char* const* mm = eyes ? kMmEyes : kMmMouths;
    int count = eyes ? 8 : 4;
    for (int i = count - 1; i >= 0; --i) {
        if (strstr(texture, oot[i]) != nullptr || strstr(texture, mm[i]) != nullptr) {
            return i;
        }
    }
    return -1;
}

static const uint8_t* MissingTexture() {
    static const std::vector<uint8_t> white(0x2000, 0xFF);
    return white.data();
}

static const char* FindKeepTexture(uint32_t offset) {
    for (const auto& texture : kOotKeepTextures) {
        if (texture.offset == offset) {
            return texture.name;
        }
    }
    return nullptr;
}

static const StandardLimb* BaseLimb(const SkeletonHeader* header, unsigned index) {
    return reinterpret_cast<const StandardLimb*>(header->segment[index]);
}

static float SkeletonLegLength(const SkeletonHeader* header) {
    float length = 0.f;
    for (unsigned index : kLegLimbs) {
        length += float(BaseLimb(header, index)->jointPos.x);
    }
    return length;
}

static const SkeletonHeader* LoadLinkSkeleton(const char* path) {
    auto* header = reinterpret_cast<const SkeletonHeader*>(ResourceMgr_LoadSkeletonByName(path, nullptr));
    Require(header != nullptr && header->limbCount == kLimbCount && header->segment != nullptr,
            "Select a skeleton with 21 Link limbs");
    for (unsigned i = 0; i < kLimbCount; ++i) {
        Require(header->segment[i] != nullptr, "Missing skeleton limb");
    }
    return header;
}

struct Model {
    std::string key;
    std::string label;
    std::string author;
    std::string path;
    std::string prefix;
    std::string stem;
    bool child = false;
    bool mm = false;
    bool deity = false;
    bool native = false;
    bool ready = false;
    bool equipmentOnly = false;
    std::string equipmentName;
    std::array<const Mtx*, 3> swordBack{};
    std::array<const Mtx*, 3> shieldBack{};
    Bytes bytes;
    std::array<LodLimb, kLimbCount> limbs{};
    std::array<uint32_t, kLimbCount> limbLists{};
    std::vector<uint32_t> matrixSlots;
    std::map<uint32_t, std::vector<Gfx>> lists;
    std::map<uint32_t, std::vector<Vtx>> vertices;
    std::map<uint32_t, Mtx> matrices;
    std::map<uint32_t, Gfx*> aliases;
    std::set<uint32_t> visiting;
    size_t commands = 0;
    float legLength = 0.f;
    float rootRatio = 1.f;

    struct TextureState {
        uint32_t address = 0;
        uint32_t size = 0;
        bool local = false;
    };

    void Range(size_t offset, size_t size) const {
        Require(offset <= bytes.size() && size <= bytes.size() - offset, "ZOBJ reference outside file");
    }

    uint32_t Word(size_t at) const {
        Range(at, 4);
        return Be32(bytes.data() + at);
    }

    uint32_t Pointer(size_t at) const {
        uint32_t value = Word(at);
        Require((value >> 24) == 6, "Expected ZOBJ segment 06");
        return value & 0xFFFFFF;
    }

    bool Contains(std::string_view text) const {
        return std::search(bytes.begin(), bytes.end(), text.begin(), text.end()) != bytes.end();
    }

    size_t FindListEnd(uint32_t offset) {
        for (size_t end = offset;; end += 8) {
            Range(end, 8);
            Require(++commands < 131072, "Too many ZOBJ commands");
            uint32_t w0 = Be32(bytes.data() + end);
            uint32_t op = w0 >> 24;
            bool branch = op == 0xDE && ((w0 >> 16) & 1);
            if (op == 0xDF || branch) {
                return end;
            }
        }
    }

    uintptr_t TranslateVertices(uint32_t w0, uint32_t segment, uint32_t address) {
        unsigned count = (w0 >> 12) & 255;
        Require(segment == 6 && count > 0 && count <= 32, "Unsupported vertex reference");
        Range(address, count * 16);
        auto& loaded = vertices[address];
        if (loaded.empty()) {
            // Sized up front so pointers already embedded in lists stay valid.
            loaded.resize(std::min<size_t>(32, (bytes.size() - address) / 16));
        }
        Require(loaded.size() >= count, "Vertex range overflow");
        for (unsigned n = 0; n < count; ++n) {
            const uint8_t* source = bytes.data() + address + n * 16;
            for (unsigned axis = 0; axis < 3; ++axis) {
                loaded[n].v.ob[axis] = Be16(source + axis * 2);
            }
            loaded[n].v.flag = uint16_t(Be16(source + 6));
            loaded[n].v.tc[0] = Be16(source + 8);
            loaded[n].v.tc[1] = Be16(source + 10);
            memcpy(loaded[n].v.cn, source + 12, 4);
        }
        return reinterpret_cast<uintptr_t>(loaded.data());
    }

    uintptr_t TranslateCall(uint32_t w1, unsigned depth) {
        uint32_t segment = w1 >> 24;
        if (segment == 0x0C) {
            // Player_Draw binds the cull-mode list to segment 0x0C in both games.
            return w1 | 1u;
        }
        Require(segment == 6, "External display lists are not supported in imported ZOBJs");
        return reinterpret_cast<uintptr_t>(Translate(w1 & 0xFFFFFF, depth + 1));
    }

    const Mtx* ConvertMatrix(uint32_t address) {
        Range(address, 64);
        auto& matrix = matrices[address];
        const uint8_t* source = bytes.data() + address;
        for (unsigned n = 0; n < 16; ++n) {
            float whole = float(Be16(source + n * 2));
            float fraction = float(uint16_t(Be16(source + 32 + n * 2))) / 65536.f;
            matrix.mf[n / 4][n % 4] = whole + fraction;
        }
        return &matrix;
    }

    uintptr_t TranslateMatrix(uint32_t w1) {
        uint32_t segment = w1 >> 24;
        uint32_t address = w1 & 0xFFFFFF;
        if (segment == 6) {
            return reinterpret_cast<uintptr_t>(ConvertMatrix(address));
        }
        Require(segment == 0x0D && address % 64 == 0 && address / 64 < matrixSlots.size(),
                "Unsupported ZOBJ matrix reference");
        return 0x0D000000u | (matrixSlots[address / 64] * 64u) | 1u;
    }

    uintptr_t TranslateTexture(uint32_t w1) {
        uint32_t segment = w1 >> 24;
        uint32_t address = w1 & 0xFFFFFF;
        if (segment == 6) {
            Range(address, 1);
            return reinterpret_cast<uintptr_t>(bytes.data() + address);
        }
        if (segment == 8 || segment == 9) {
            return w1 | 1u;
        }
        Require(segment == 4, "Unsupported external ZOBJ segment");
        const char* name = mm ? nullptr : FindKeepTexture(address);
        const char* path = name != nullptr ? ResolveKeepTexture(name) : nullptr;
        return path != nullptr ? reinterpret_cast<uintptr_t>(path) : reinterpret_cast<uintptr_t>(MissingTexture());
    }

    void TranslateCommand(Gfx& command, size_t at, TextureState& texture, unsigned depth) {
        uint32_t w0 = Be32(bytes.data() + at);
        uint32_t w1 = Be32(bytes.data() + at + 4);
        uint32_t op = w0 >> 24;
        command.words.w0 = w0;
        command.words.w1 = w1;
        switch (op) {
            case 0x01:
                command.words.w1 = TranslateVertices(w0, w1 >> 24, w1 & 0xFFFFFF);
                break;
            case 0xDE:
                command.words.w1 = TranslateCall(w1, depth);
                break;
            case 0xDA:
                command.words.w1 = TranslateMatrix(w1);
                break;
            case 0xDC:
                Require((w1 >> 24) == 6, "Unsupported external ZOBJ segment");
                Range(w1 & 0xFFFFFF, 24);
                command.words.w1 = reinterpret_cast<uintptr_t>(bytes.data() + (w1 & 0xFFFFFF));
                break;
            case 0xFD:
                texture = { w1 & 0xFFFFFF, (w0 >> 19) & 3, (w1 >> 24) == 6 };
                command.words.w1 = TranslateTexture(w1);
                break;
            case 0xF3:
                if (texture.local) {
                    Range(texture.address, (((w1 >> 12) & 4095) + 1) * (1u << texture.size) / 2);
                }
                break;
            case 0xF0:
                if (texture.local) {
                    Range(texture.address, (((w1 >> 14) & 1023) + 1) * 2);
                }
                break;
            case 0xDB: {
                unsigned index = (w0 >> 16) & 255;
                Require(index == 2 || index == 4 || index == 8 || index == 10 || index == 12,
                        "ZOBJ attempts to replace renderer segment/matrix state");
                break;
            }
            case 0x00:
            case 0x03:
            case 0x05:
            case 0x06:
            case 0x07:
            case 0xD7:
            case 0xD8:
            case 0xD9:
            case 0xDF:
            case 0xE2:
            case 0xE3:
            case 0xE6:
            case 0xE7:
            case 0xE8:
            case 0xE9:
            case 0xF2:
            case 0xF4:
            case 0xF5:
            case 0xF8:
            case 0xF9:
            case 0xFA:
            case 0xFB:
            case 0xFC:
                break;
            default:
                throw std::runtime_error("Unsupported ZOBJ graphics command");
        }
    }

    Gfx* Translate(uint32_t offset, unsigned depth = 0) {
        Require(depth < 64 && !visiting.count(offset), "Recursive ZOBJ display list");
        auto existing = lists.find(offset);
        if (existing != lists.end()) {
            return existing->second.data();
        }
        visiting.insert(offset);
        try {
            size_t end = FindListEnd(offset);
            auto& list = lists[offset];
            list.resize((end - offset) / 8 + 1);
            TextureState texture;
            for (size_t i = 0; i < list.size(); ++i) {
                TranslateCommand(list[i], offset + i * 8, texture, depth);
            }
        } catch (...) {
            lists.erase(offset);
            visiting.erase(offset);
            throw;
        }
        visiting.erase(offset);
        return lists[offset].data();
    }

    std::array<int, kLimbCount> Parents() const {
        std::array<int, kLimbCount> parents;
        parents.fill(-1);
        for (unsigned i = 0; i < kLimbCount; ++i) {
            unsigned steps = 0;
            for (uint8_t next = limbs[i].child; next != 0xFF; next = limbs[next].sibling) {
                Require(next < kLimbCount && ++steps <= kLimbCount, "Invalid limb hierarchy");
                parents[next] = int(i);
            }
        }
        return parents;
    }

    void MapMatrixSlots(const SkeletonHeader* base) {
        std::array<int, kLimbCount> baseSlots;
        int nextSlot = 0;
        for (unsigned i = 0; i < kLimbCount; ++i) {
            baseSlots[i] = BaseLimb(base, i)->dList != nullptr ? nextSlot++ : -1;
        }
        auto parents = Parents();
        matrixSlots.clear();
        for (unsigned i = 0; i < kLimbCount; ++i) {
            if (limbLists[i] == 0) {
                continue;
            }
            int owner = int(i);
            while (owner >= 0 && baseSlots[owner] < 0) {
                owner = parents[owner];
            }
            matrixSlots.push_back(owner >= 0 ? uint32_t(baseSlots[owner]) : 0u);
        }
        for (unsigned i = 0; i < kLimbCount; ++i) {
            if (baseSlots[i] < 0) {
                limbLists[i] = 0;
            }
        }
    }

    void Validate() {
        const SkeletonHeader* base = LoadLinkSkeleton(BaseSkeletonPath(child));
        for (unsigned i = 0; i < kLimbCount; ++i) {
            const StandardLimb* vanilla = BaseLimb(base, i);
            Require(limbs[i].child == vanilla->child && limbs[i].sibling == vanilla->sibling,
                    "Model does not use the OoT/MM human Link limb hierarchy");
            if (native) {
                Require((limbs[i].dLists[0] != nullptr) == (vanilla->dList != nullptr),
                        "Model changes Link's flex matrix layout; unsupported rig");
            }
        }
        if (!native) {
            MapMatrixSlots(base);
        }
        legLength = 0.f;
        for (unsigned index : kLegLimbs) {
            legLength += float(limbs[index].jointPos.x);
        }
        float baseLength = SkeletonLegLength(base);
        rootRatio = legLength > 0.f && baseLength > 0.f ? legLength / baseLength : 1.f;
    }

    void LoadNative() {
        if (ready) {
            return;
        }
        const SkeletonHeader* header = LoadLinkSkeleton(path.c_str());
        for (unsigned i = 0; i < kLimbCount; ++i) {
            const StandardLimb* source = BaseLimb(header, i);
            limbs[i].jointPos = source->jointPos;
            limbs[i].child = source->child;
            limbs[i].sibling = source->sibling;
            limbs[i].dLists[0] = source->dList;
            limbs[i].dLists[1] = source->dList;
        }
        Validate();
        ready = true;
    }

    bool ReadBodyKind(uint8_t value) {
        switch (value) {
            case 0:
                child = false;
                mm = false;
                return true;
            case 1:
                child = true;
                mm = false;
                return true;
            case 4:
            case 0x68:
                child = true;
                mm = true;
                return true;
            default:
                return false;
        }
    }

    uint32_t FindSkeleton(bool universal) const {
        uint32_t declared = Word(universal ? 0x501C : 0x500C);
        if ((declared >> 24) == 6) {
            return declared & 0xFFFFFF;
        }
        Require(!universal, "Z64O model has no skeleton section");
        // zzplayas exporters may blank 0x500C; the header still sits in its fixed per-age slot.
        return mm ? 0x5420 : child ? 0x53A8 : 0x5380;
    }

    void AddAlias(uint32_t at, uint32_t alias) {
        if (at + 8 > bytes.size() || bytes[at] != 0xDE || bytes[at + 4] != 6) {
            return;
        }
        try {
            aliases[alias] = Translate(Be32(bytes.data() + at + 4) & 0xFFFFFF);
        } catch (const std::exception&) {}
    }

    void LoadAliases(bool universal) {
        if (universal) {
            for (uint32_t at = 0x5020; at < 0x5800; at += 8) {
                AddAlias(at, at);
            }
            return;
        }
        if (mm) {
            return;
        }
        static constexpr uint32_t kAdult[][2] = {
            { 0x5108, 0x5098 }, { 0x5110, 0x50A0 }, { 0x5118, 0x50A8 }, { 0x5120, 0x50B0 }, { 0x5128, 0x50B8 },
            { 0x5130, 0x50C8 }, { 0x5138, 0x50E0 }, { 0x5140, 0x50F8 }, { 0x5148, 0x50E8 }, { 0x5150, 0x5100 },
            { 0x5160, 0x5110 }, { 0x5168, 0x5118 }, { 0x5170, 0x51F0 }, { 0x5178, 0x5120 }, { 0x5180, 0x5138 },
            { 0x5188, 0x5128 }, { 0x5190, 0x5148 }, { 0x52B8, 0x5448 }, { 0x52D0, 0x5458 }, { 0x52E8, 0x54F0 },
            { 0x5300, 0x5460 }, { 0x5310, 0x5470 }, { 0x5320, 0x5478 }, { 0x5330, 0x5480 }, { 0x5340, 0x5488 },
            { 0x5350, 0x5490 }, { 0x5360, 0x5498 }, { 0x5370, 0x54A0 }, { 0x5238, 0x53D8 }, { 0x5258, 0x53F0 },
            { 0x5268, 0x53F8 }, { 0x5278, 0x5420 }, { 0x5288, 0x5428 },
        };
        static constexpr uint32_t kChild[][2] = {
            { 0x50D0, 0x5108 }, { 0x5150, 0x5098 }, { 0x5158, 0x50A0 }, { 0x5160, 0x50A8 },
            { 0x5168, 0x50B0 }, { 0x5170, 0x50B8 }, { 0x5178, 0x50C0 }, { 0x5180, 0x50D8 },
            { 0x5188, 0x50F0 }, { 0x5190, 0x5180 }, { 0x5198, 0x5190 }, { 0x51A0, 0x5128 },
            { 0x51A8, 0x5130 }, { 0x51B0, 0x5178 }, { 0x51B8, 0x53F0 }, { 0x51C0, 0x5120 },
        };
        if (child) {
            for (const auto& pair : kChild) {
                AddAlias(pair[0], pair[1]);
            }
        } else {
            for (const auto& pair : kAdult) {
                AddAlias(pair[0], pair[1]);
            }
        }
    }

    void LoadZobj() {
        Range(kManifest, 0x20);
        Require(!memcmp(bytes.data() + kManifest, "MODL", 4), "Missing MODLOADER64 ZOBJ manifest");
        bool universal = Contains("UNIVERSAL_ALIAS_TABLE");
        Require(ReadBodyKind(bytes[universal ? 0x501B : 0x500B]), "Not a human Link body");
        uint32_t skeleton = FindSkeleton(universal);
        Range(skeleton, 8);
        Require(bytes[skeleton + 4] == kLimbCount, "ZOBJ must have 21 human Link limbs");
        uint32_t limbTable = Pointer(skeleton);
        for (unsigned i = 0; i < kLimbCount; ++i) {
            uint32_t limb = Pointer(limbTable + i * 4);
            Range(limb, 12);
            const uint8_t* source = bytes.data() + limb;
            limbs[i].jointPos = { Be16(source), Be16(source + 2), Be16(source + 4) };
            limbs[i].child = source[6];
            limbs[i].sibling = source[7];
            uint32_t list = Be32(source + 8);
            Require(list == 0 || (list >> 24) == 6, "Invalid limb display list");
            limbLists[i] = list & 0xFFFFFF;
        }
        Validate();
        for (unsigned i = 0; i < kLimbCount; ++i) {
            if (limbLists[i] != 0) {
                Gfx* list = Translate(limbLists[i]);
                limbs[i].dLists[0] = list;
                limbs[i].dLists[1] = list;
            }
        }
        LoadAliases(universal);
        LoadBackMatrices(universal);
        ready = true;
    }

    const Mtx* OptionalMatrix(uint32_t address) {
        if (address + 64 > bytes.size() || Be16(bytes.data() + address + 30) != 1) {
            return nullptr;
        }
        return ConvertMatrix(address);
    }

    // Universal tables keep three sword and three shield back matrices; zzplayas keeps up to four at 0x5010.
    void LoadBackMatrices(bool universal) {
        if (universal) {
            for (unsigned i = 0; i < 3; ++i) {
                swordBack[i] = OptionalMatrix(0x5890 + i * 0x40);
                shieldBack[i] = OptionalMatrix(0x5950 + i * 0x40);
            }
            return;
        }
        unsigned count = mm ? 4 : child ? 3 : 2;
        for (unsigned i = 0; i < count; ++i) {
            const Mtx* matrix = OptionalMatrix(0x5010 + i * 0x40);
            if (i < 3) {
                swordBack[i] = matrix;
            } else {
                shieldBack[0] = matrix;
            }
        }
        for (unsigned i = 1; i < 3; ++i) {
            if (swordBack[i] == nullptr) {
                swordBack[i] = swordBack[i - 1];
            }
        }
    }

    static uint32_t EquipmentAlias(const std::string& name) {
        static const std::pair<const char*, uint32_t> kPieces[] = {
            { "sword0_sheath", 0x50C0 }, { "sword1_sheath", 0x50C8 }, { "sword2_sheath", 0x50D0 },
            { "sword0_hilt", 0x50D8 },   { "sword1_hilt", 0x50E0 },   { "sword2_hilt", 0x50E8 },
            { "sword0_blade", 0x50F0 },  { "sword1_blade", 0x50F8 },  { "sword2_blade", 0x5100 },
            { "shield0_held", 0x5108 },  { "shield1_held", 0x5110 },  { "shield2_held", 0x5118 },
            { "bottle", 0x5120 },        { "ocarina_1_a", 0x5128 },   { "ocarina_1", 0x5128 },
            { "deku_stick", 0x5130 },    { "bow", 0x5138 },           { "bow_string", 0x5140 },
            { "hookshot", 0x5148 },      { "boomerang", 0x5178 },     { "slingshot", 0x5180 },
            { "ocarina_0", 0x5190 },     { "hammer", 0x51F0 },
        };
        for (const auto& [piece, alias] : kPieces) {
            if (name == piece) {
                return alias;
            }
        }
        return 0;
    }

    size_t Find(std::string_view text, size_t from = 0) const {
        auto found = std::search(bytes.begin() + std::min(from, bytes.size()), bytes.end(), text.begin(), text.end());
        return found == bytes.end() ? std::string::npos : size_t(found - bytes.begin());
    }

    void LoadEquipment() {
        size_t marker = Find("MODLOADER64");
        Require(marker != std::string::npos && marker + 0x10 <= bytes.size(), "Missing MODLOADER64 equipment block");
        uint32_t count = Be32(bytes.data() + marker + 0x0C);
        size_t table = marker + 0x10;
        Require(count > 0 && count <= 256, "Invalid equipment count");
        Range(table, count * 8);
        size_t manifest = Find("EQUIPMANIFEST", table);
        Require(manifest != std::string::npos, "Missing EQUIPMANIFEST");
        size_t end = manifest + 16;
        while (end < bytes.size() && bytes[end] != 0xFF && bytes[end] != 0) {
            ++end;
        }
        std::string json(bytes.begin() + std::min(manifest + 16, bytes.size()), bytes.begin() + end);
        for (size_t at = json.find('"'); at != std::string::npos; at = json.find('"', at + 1)) {
            size_t close = json.find('"', at + 1);
            if (close == std::string::npos || close == at + 1 ||
                !std::all_of(json.begin() + at + 1, json.begin() + close, ::isdigit)) {
                continue;
            }
            unsigned index = unsigned(std::stoul(json.substr(at + 1, close - at - 1)));
            size_t open = json.find('"', close + 1);
            size_t finish = open == std::string::npos ? open : json.find('"', open + 1);
            if (finish == std::string::npos || index >= count) {
                continue;
            }
            uint32_t alias = EquipmentAlias(json.substr(open + 1, finish - open - 1));
            if (alias != 0) {
                AddAlias(uint32_t(table + index * 8), alias);
            }
            at = finish;
        }
        size_t name = Find("EQUIPMENTNAME", manifest);
        if (name != std::string::npos && name + 16 < bytes.size()) {
            const char* text = reinterpret_cast<const char*>(bytes.data() + name + 16);
            equipmentName.assign(text, strnlen(text, std::min<size_t>(32, bytes.size() - name - 16)));
        }
        Require(!aliases.empty(), "No supported equipment pieces");
        equipmentOnly = true;
        ready = true;
    }

    const uint8_t* EyeTexture(int index) const {
        return bytes.data() + index * 0x800;
    }

    const uint8_t* MouthTexture(int index) const {
        return bytes.data() + 0x4000 + index * 0x400;
    }

    std::string NativeFaceName(int index, bool eyes) const {
        if (deity) {
            return prefix + (eyes ? "gLinkFierceDeityEyesTex" : "gLinkFierceDeityMouthTex");
        }
        const char* const* names = eyes ? (mm ? kMmEyes : kOotEyes) : (mm ? kMmMouths : kOotMouths);
        return prefix + stem + names[index] + "Tex";
    }

    std::string NativeResourceFor(const std::string& original) const {
        size_t leaf = original.rfind('/');
        std::string name = leaf == std::string::npos ? original : original.substr(leaf + 1);
        static const char* const kStems[] = { "gLinkFierceDeity", "gLinkHuman", "gLinkChild", "gLinkAdult" };
        const char* from = nullptr;
        for (const char* candidate : kStems) {
            if (name.rfind(candidate, 0) == 0) {
                from = candidate;
                break;
            }
        }
        if (from == nullptr) {
            return "";
        }
        bool sourceMm = !strcmp(from, "gLinkHuman") || !strcmp(from, "gLinkFierceDeity");
        name.replace(0, strlen(from), stem);
        if (mm && !sourceMm) {
            for (const char* lod : { "NearDL", "FarDL" }) {
                size_t at = name.find(lod);
                if (at != std::string::npos) {
                    name.replace(at, strlen(lod), "DL");
                }
            }
        } else if (!mm && sourceMm && name.size() > 2 && name.ends_with("DL") && !name.ends_with("NearDL")) {
            name.insert(name.size() - 2, "Near");
        }
        return prefix + name;
    }
};
} // namespace pak
