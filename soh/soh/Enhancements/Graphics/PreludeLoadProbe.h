#pragma once

// Diagnostic only. Observe main-thread frames and completed import work; do not change loading or caching.
#include <chrono>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <nlohmann/json.hpp>

namespace Prelude::LoadProbe {
using Clock = std::chrono::steady_clock;
using Nanoseconds = std::chrono::nanoseconds;

struct ArchiveReads {
    uint64_t count = 0;
    uint64_t bytes = 0;
    uint64_t nanos = 0;
};

// Process-wide counters advance only when an instrumented import operation
// completes. Frame reports subtract two snapshots of these cumulative values,
// so work may begin before the frame and still appear when it completes.
struct CompletedWork {
    uint64_t lookupNanos = 0;
    uint64_t parseNanos = 0;
    uint64_t decodeNanos = 0;
    uint64_t lookups = 0;
    uint64_t parses = 0;
    uint64_t decodes = 0;
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
    std::map<std::string, ArchiveReads> archives;
};

struct Frame {
    bool active = false;
    bool rendering = false;
    uint64_t epoch = 0;
    uint32_t gameplayFrame = 0;
    int scene = -1;
    int startRoom = -1;
    int room = -1;
    int previousRoom = -1;
    bool altAssets = false;
    Clock::time_point start;
    Clock::time_point renderStart;
    uint64_t lookupNanos = 0;
    uint64_t parseNanos = 0;
    uint64_t decodeNanos = 0;
    uint64_t lookups = 0;
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
    uint64_t startGeneration = 0;
    std::map<std::string, ArchiveReads> archives;
    CompletedWork completedAtStart;
};
struct StateReloadInterval {
    bool active = false;
    int sourceScene = -1;
    int sourceRoom = -1;
};
inline thread_local Frame frame;
inline thread_local StateReloadInterval stateReload;
inline thread_local uint64_t serial = 0;
inline thread_local int lastScene = -1;
inline thread_local int lastRoom = -1;
// Process-wide generation also exposes invalidations performed by a worker.
inline std::atomic<uint64_t> metadataGeneration{ 0 };
inline std::atomic<bool> collectCompletedWork{ false };
inline std::mutex completedWorkMutex;
inline CompletedWork cumulativeCompletedWork;

struct TimedCompletedWorkSnapshot {
    Clock::time_point boundary;
    CompletedWork work;
};

inline TimedCompletedWorkSnapshot BeginCompletedWorkCollection() {
    std::lock_guard lock(completedWorkMutex);
    TimedCompletedWorkSnapshot snapshot{ Clock::now(), cumulativeCompletedWork };
    collectCompletedWork.store(true, std::memory_order_release);
    return snapshot;
}

inline TimedCompletedWorkSnapshot EndCompletedWorkCollection() {
    std::lock_guard lock(completedWorkMutex);
    TimedCompletedWorkSnapshot snapshot{ Clock::now(), cumulativeCompletedWork };
    collectCompletedWork.store(false, std::memory_order_release);
    return snapshot;
}

inline CompletedWork CompletedWorkDelta(const CompletedWork& end, const CompletedWork& start) {
    CompletedWork delta;
    delta.lookupNanos = end.lookupNanos - start.lookupNanos;
    delta.parseNanos = end.parseNanos - start.parseNanos;
    delta.decodeNanos = end.decodeNanos - start.decodeNanos;
    delta.lookups = end.lookups - start.lookups;
    delta.parses = end.parses - start.parses;
    delta.decodes = end.decodes - start.decodes;
    delta.cacheHits = end.cacheHits - start.cacheHits;
    delta.cacheMisses = end.cacheMisses - start.cacheMisses;
    for (const auto& [path, current] : end.archives) {
        const auto previous = start.archives.find(path);
        const ArchiveReads before = previous == start.archives.end() ? ArchiveReads{} : previous->second;
        ArchiveReads change{ current.count - before.count, current.bytes - before.bytes, current.nanos - before.nanos };
        if (change.count != 0 || change.bytes != 0 || change.nanos != 0) {
            delta.archives.emplace(path, change);
        }
    }
    return delta;
}

inline bool HasCompletedWork(const CompletedWork& work) {
    return work.lookups != 0 || work.parses != 0 || work.decodes != 0 || !work.archives.empty();
}

inline nlohmann::json CompletedWorkReport(const CompletedWork& completed) {
    uint64_t reads = 0, bytes = 0, readNanos = 0;
    auto archives = nlohmann::json::array();
    for (const auto& [path, record] : completed.archives) {
        reads += record.count;
        bytes += record.bytes;
        readNanos += record.nanos;
        archives.push_back({ { "archive", path },
                             { "reads", record.count },
                             { "bytes", record.bytes },
                             { "read_work_ms", record.nanos / 1000000.0 } });
    }
    return {
        { "timing_semantics", "elapsed work for operations completed during the measured interval; operations may "
                              "overlap and durations are not additive interval attribution" },
        { "metadata_reads", reads },
        { "metadata_bytes", bytes },
        { "metadata_read_work_ms", readNanos / 1000000.0 },
        { "metadata_parses", completed.parses },
        { "metadata_parse_work_ms", completed.parseNanos / 1000000.0 },
        { "material_lookups", completed.lookups },
        { "material_lookup_work_ms", completed.lookupNanos / 1000000.0 },
        { "binary_decodes", completed.decodes },
        { "binary_decode_work_ms", completed.decodeNanos / 1000000.0 },
        { "metadata_cache_hits", completed.cacheHits },
        { "metadata_cache_misses", completed.cacheMisses },
        { "archives", archives },
    };
}

inline void BeginFrame(int scene, int room, int previousRoom, uint32_t gameplayFrame, bool altAssets, bool enabled) {
    frame = {};
    frame.epoch = ++serial;
    frame.active = enabled && scene == 0x5b;
    if (!frame.active) {
        collectCompletedWork.store(false, std::memory_order_release);
        lastScene = scene;
        lastRoom = room;
        return;
    }
    frame.scene = scene;
    frame.startRoom = frame.room = room;
    frame.previousRoom = previousRoom;
    frame.gameplayFrame = gameplayFrame;
    frame.altAssets = altAssets;
    frame.startGeneration = metadataGeneration.load(std::memory_order_relaxed);
    auto completed = BeginCompletedWorkCollection();
    frame.start = completed.boundary;
    frame.completedAtStart = std::move(completed.work);
}

inline void BeginRender(int room, int previousRoom) {
    if (!frame.active) {
        return;
    }
    frame.rendering = true;
    frame.room = room;
    frame.previousRoom = previousRoom;
    frame.renderStart = Clock::now();
}

inline void BeginStateReload(int sourceScene, int sourceRoom, bool altAssets, bool enabled) {
    stateReload = {};
    if (!enabled || sourceScene != 0x5b) {
        collectCompletedWork.store(false, std::memory_order_release);
        return;
    }
    frame = {};
    frame.epoch = ++serial;
    frame.active = true;
    frame.scene = sourceScene;
    frame.startRoom = frame.room = sourceRoom;
    frame.altAssets = altAssets;
    frame.startGeneration = metadataGeneration.load(std::memory_order_relaxed);
    auto completed = BeginCompletedWorkCollection();
    frame.start = completed.boundary;
    frame.completedAtStart = std::move(completed.work);
    stateReload.active = true;
    stateReload.sourceScene = sourceScene;
    stateReload.sourceRoom = sourceRoom;
}

class Scope {
  public:
    explicit Scope(uint64_t Frame::*field)
        : field(field), mainThreadActive(frame.active), epoch(frame.epoch), start(Clock::now()) {
    }
    ~Scope() {
        const bool collect = collectCompletedWork.load(std::memory_order_acquire);
        if (!mainThreadActive && !collect) {
            return;
        }
        const auto elapsed =
            static_cast<uint64_t>(std::chrono::duration_cast<Nanoseconds>(Clock::now() - start).count());
        if (mainThreadActive && frame.active && epoch == frame.epoch) {
            frame.*field += elapsed;
        }
        if (!collect) {
            return;
        }
        std::lock_guard lock(completedWorkMutex);
        if (!collectCompletedWork.load(std::memory_order_relaxed)) {
            return;
        }
        if (field == &Frame::lookupNanos) {
            cumulativeCompletedWork.lookupNanos += elapsed;
            ++cumulativeCompletedWork.lookups;
            cumulativeCompletedWork.cacheHits += cacheHit;
            cumulativeCompletedWork.cacheMisses += cacheMiss;
        } else if (field == &Frame::parseNanos) {
            cumulativeCompletedWork.parseNanos += elapsed;
            ++cumulativeCompletedWork.parses;
        } else if (field == &Frame::decodeNanos) {
            cumulativeCompletedWork.decodeNanos += elapsed;
            ++cumulativeCompletedWork.decodes;
        }
    }
    void MarkCacheHit() {
        cacheHit = 1;
    }
    void MarkCacheMiss() {
        cacheMiss = 1;
    }

  private:
    uint64_t Frame::*field;
    bool mainThreadActive;
    uint64_t epoch;
    Clock::time_point start;
    uint64_t cacheHit = 0;
    uint64_t cacheMiss = 0;
};

class MetadataRead {
  public:
    explicit MetadataRead(std::string_view archive)
        : mainThreadActive(frame.active), epoch(frame.epoch), archive(archive), start(Clock::now()) {
    }
    void SetBytes(uint64_t value) {
        bytes = value;
    }
    ~MetadataRead() {
        const bool collect = collectCompletedWork.load(std::memory_order_acquire);
        if (!mainThreadActive && !collect) {
            return;
        }
        const auto elapsed =
            static_cast<uint64_t>(std::chrono::duration_cast<Nanoseconds>(Clock::now() - start).count());
        if (mainThreadActive && frame.active && epoch == frame.epoch) {
            auto& record = frame.archives[archive];
            ++record.count;
            record.bytes += bytes;
            record.nanos += elapsed;
        }
        if (!collect) {
            return;
        }
        std::lock_guard lock(completedWorkMutex);
        if (!collectCompletedWork.load(std::memory_order_relaxed)) {
            return;
        }
        auto& completed = cumulativeCompletedWork.archives[archive];
        ++completed.count;
        completed.bytes += bytes;
        completed.nanos += elapsed;
    }

  private:
    bool mainThreadActive;
    uint64_t epoch;
    std::string archive;
    Clock::time_point start;
    uint64_t bytes = 0;
};

inline std::optional<nlohmann::json> EndFrame() {
    if (!frame.active) {
        return std::nullopt;
    }
    const auto completedSnapshot = EndCompletedWorkCollection();
    const auto end = completedSnapshot.boundary;
    const auto completed = CompletedWorkDelta(completedSnapshot.work, frame.completedAtStart);
    frame.active = false; // Report formatting and logging are outside the measured interval.
    const double wallMs = std::chrono::duration<double, std::milli>(end - frame.start).count();
    const bool roomChanged = frame.scene != lastScene || frame.room != lastRoom;
    lastScene = frame.scene;
    lastRoom = frame.room;
    const auto generation = metadataGeneration.load(std::memory_order_relaxed);
    if (!roomChanged && frame.lookups == 0 && frame.archives.empty() && !HasCompletedWork(completed) &&
        generation == frame.startGeneration && wallMs < 250.0) {
        return std::nullopt;
    }
    const double updateMs =
        frame.rendering ? std::chrono::duration<double, std::milli>(frame.renderStart - frame.start).count() : wallMs;
    const double renderMs =
        frame.rendering ? std::chrono::duration<double, std::milli>(end - frame.renderStart).count() : 0;
    uint64_t reads = 0, bytes = 0, nanos = 0;
    auto archives = nlohmann::json::array();
    for (const auto& [path, record] : frame.archives) {
        reads += record.count;
        bytes += record.bytes;
        nanos += record.nanos;
        archives.push_back({ { "archive", path },
                             { "reads", record.count },
                             { "bytes", record.bytes },
                             { "read_ms", record.nanos / 1000000.0 } });
    }
    return nlohmann::json{ { "scene", frame.scene },
                           { "start_room", frame.startRoom },
                           { "render_room", frame.room },
                           { "previous_room", frame.previousRoom },
                           { "gameplay_frame", frame.gameplayFrame },
                           { "alt_assets", frame.altAssets },
                           { "frame_ms", wallMs },
                           { "update_ms", updateMs },
                           { "graphics_ms", renderMs },
                           { "metadata_reads", reads },
                           { "metadata_bytes", bytes },
                           { "metadata_read_ms", nanos / 1000000.0 },
                           { "metadata_parse_ms", frame.parseNanos / 1000000.0 },
                           { "material_lookup_ms", frame.lookupNanos / 1000000.0 },
                           { "binary_decode_ms", frame.decodeNanos / 1000000.0 },
                           { "material_lookups", frame.lookups },
                           { "metadata_cache_hits", frame.cacheHits },
                           { "metadata_cache_misses", frame.cacheMisses },
                           { "metadata_cache_generation", generation },
                           { "metadata_invalidations_during_frame", generation - frame.startGeneration },
                           { "main_thread_only", true },
                           { "main_thread_measurement_scope",
                             "frame/update/graphics and root-level import fields only" },
                           { "archives", archives },
                           { "all_threads_completed_work", CompletedWorkReport(completed) } };
}

inline std::optional<nlohmann::json> EndStateReload(int targetScene, int targetRoom) {
    if (!stateReload.active) {
        return std::nullopt;
    }
    const auto completedSnapshot = EndCompletedWorkCollection();
    const auto completed = CompletedWorkDelta(completedSnapshot.work, frame.completedAtStart);
    const double reloadMs = std::chrono::duration<double, std::milli>(completedSnapshot.boundary - frame.start).count();
    const auto generation = metadataGeneration.load(std::memory_order_relaxed);
    frame.active = false;
    stateReload.active = false;

    uint64_t reads = 0, bytes = 0, nanos = 0;
    auto archives = nlohmann::json::array();
    for (const auto& [path, record] : frame.archives) {
        reads += record.count;
        bytes += record.bytes;
        nanos += record.nanos;
        archives.push_back({ { "archive", path },
                             { "reads", record.count },
                             { "bytes", record.bytes },
                             { "read_ms", record.nanos / 1000000.0 } });
    }
    return nlohmann::json{
        { "kind", "state_reload" },
        { "source_scene", stateReload.sourceScene },
        { "source_room", stateReload.sourceRoom },
        { "target_scene", targetScene },
        { "target_room", targetRoom },
        { "alt_assets", frame.altAssets },
        { "state_reload_ms", reloadMs },
        { "metadata_reads", reads },
        { "metadata_bytes", bytes },
        { "metadata_read_ms", nanos / 1000000.0 },
        { "metadata_parse_ms", frame.parseNanos / 1000000.0 },
        { "material_lookup_ms", frame.lookupNanos / 1000000.0 },
        { "binary_decode_ms", frame.decodeNanos / 1000000.0 },
        { "material_lookups", frame.lookups },
        { "metadata_cache_hits", frame.cacheHits },
        { "metadata_cache_misses", frame.cacheMisses },
        { "metadata_cache_generation", generation },
        { "metadata_invalidations_during_reload", generation - frame.startGeneration },
        { "main_thread_only", true },
        { "main_thread_measurement_scope", "state_reload_ms and root-level import fields only" },
        { "archives", archives },
        { "all_threads_completed_work", CompletedWorkReport(completed) },
    };
}
} // namespace Prelude::LoadProbe
