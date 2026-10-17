#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <cmath>

namespace viewer {
constexpr size_t Stride = 67;
struct ClipInfo { int frames = 0; std::string target; bool packed = false; };
static uint32_t U32(const uint8_t* p, bool be) {
    return be ? (uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3])
              : (uint32_t(p[3]) << 24 | uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0]);
}
static uint16_t U16(const uint8_t* p, bool be) {
    return be ? uint16_t(p[0]) << 8 | p[1] : uint16_t(p[1]) << 8 | p[0];
}
// The archive buffer belongs to the host. Never retain its address or cast it to an engine structure.
static ClipInfo Inspect(const uint8_t* p, size_t n) {
    ClipInfo out;
    if (!p || n < 68 || p[0] > 1) return out;
    bool be = p[0] == 1;
    uint32_t type = U32(p + 4, be);
    if (U32(p + 8, be) != 0) return out;
    if (type == 0x4f50414d) {
        uint32_t count = U32(p + 64, be);
        if (!count || count % Stride || count / Stride > 32767 || count > (n - 68) / 2) return out;
        out.frames = int(count / Stride); out.packed = true;
    } else if (type == 0x4f414e4d && n >= 74 && U32(p + 64, be) == 1) {
        uint16_t frames = U16(p + 68, be);
        uint32_t length = U32(p + 70, be);
        if (!frames || frames > 32767 || !length || length > n - 74 || length > 2048) return out;
        out.target.assign(reinterpret_cast<const char*>(p + 74), length);
        while (!out.target.empty() && out.target.back() == '\0') out.target.pop_back();
        if (out.target.find('\0') != std::string::npos) return ClipInfo{};
        if (out.target.compare(0, 7, "__OTR__") == 0) out.target.erase(0, 7);
        out.frames = frames;
    }
    return out;
}
static bool Decode(const uint8_t* p, size_t n, std::vector<int16_t>& data) {
    auto info = Inspect(p, n);
    if (!info.packed) return false;
    data.resize(size_t(info.frames) * Stride);
    for (size_t i = 0; i < data.size(); ++i) data[i] = int16_t(U16(p + 68 + i * 2, p[0] == 1));
    return true;
}
static float Advance(float frame, float speed, int count, bool loop) {
    if (count <= 1) return 0;
    float next = frame + speed;
    if (!loop) return next < 0 ? 0 : next > count - 1 ? float(count - 1) : next;
    next = std::fmod(next, float(count));
    return next < 0 ? next + count : next;
}
}
