#include "../soh/Enhancements/Graphics/PreludeLoadProbe.h"
#include <latch>
#include <thread>
#include <stdexcept>
#include <iostream>
using namespace Prelude::LoadProbe;
#define require(value)                                                                                   \
    do {                                                                                                 \
        if (!(value)) {                                                                                  \
            throw std::runtime_error("counter verification failed at line " + std::to_string(__LINE__)); \
        }                                                                                                \
    } while (false)
int main() {
    BeginFrame(0x5b, 9, 0, 1, true, true);
    {
        Scope lookup(&Frame::lookupNanos);
        frame.lookups++;
        MetadataRead read("C:\\mods\\room11.o2r");
        read.SetBytes(4706089);
    }
    std::thread worker([] {
        MetadataRead read("worker.o2r");
        read.SetBytes(99);
    });
    worker.join();
    BeginRender(11, 9);
    auto result = EndFrame();
    require(result.has_value());
    require((*result)["metadata_reads"] == 1 && (*result)["metadata_bytes"] == 4706089);
    require((*result)["start_room"] == 9 && (*result)["render_room"] == 11 && (*result)["previous_room"] == 9);
    require((*result)["archives"].size() == 1 && (*result)["archives"][0]["archive"] == "C:\\mods\\room11.o2r");
    require((*result)["material_lookup_ms"].get<double>() >= (*result)["metadata_read_ms"].get<double>());
    require((*result)["all_threads_completed_work"]["metadata_reads"] == 2);
    require((*result)["all_threads_completed_work"]["metadata_bytes"] == 4706188);
    require(!EndFrame().has_value());
    BeginFrame(0x5b, 11, 9, 2, true, true);
    BeginRender(11, 9);
    require(!EndFrame().has_value());

    // Completed-work deltas include work that started before this frame on a
    // resource thread. No main-thread probe state is active on that worker.
    std::latch workerStarted(1), allowWorkerCompletion(1);
    std::thread crossingWorker([&] {
        Scope lookup(&Frame::lookupNanos);
        Scope parse(&Frame::parseNanos);
        Scope decode(&Frame::decodeNanos);
        MetadataRead read("crossing-worker.o2r");
        read.SetBytes(123);
        workerStarted.count_down();
        allowWorkerCompletion.wait();
    });
    workerStarted.wait();
    BeginFrame(0x5b, 11, 9, 3, true, true);
    allowWorkerCompletion.count_down();
    crossingWorker.join();
    result = EndFrame();
    require(result.has_value());
    require((*result)["metadata_reads"] == 0 && (*result)["material_lookups"] == 0);
    const auto& completed = (*result)["all_threads_completed_work"];
    require(completed["metadata_reads"] == 1 && completed["metadata_bytes"] == 123);
    require(completed["material_lookups"] == 1);
    require(completed["metadata_parse_work_ms"].get<double>() >= 0.0);
    require(completed["material_lookup_work_ms"].get<double>() >= completed["metadata_read_work_ms"].get<double>());
    require(completed["binary_decode_work_ms"].get<double>() >= 0.0);
    require(completed["archives"].size() == 1 && completed["archives"][0]["archive"] == "crossing-worker.o2r");

    BeginFrame(0x5b, 11, 9, 4, true, true);
    std::thread invalidator([] { metadataGeneration.fetch_add(1, std::memory_order_relaxed); });
    invalidator.join();
    result = EndFrame();
    require(result.has_value());
    require((*result)["metadata_invalidations_during_frame"] == 1 && (*result)["metadata_reads"] == 0);
    BeginFrame(0x5b, 11, 9, 5, true, false);
    {
        MetadataRead read("disabled");
        read.SetBytes(1);
    }
    require(!EndFrame().has_value());
    BeginFrame(0x51, 11, 9, 6, true, true);
    {
        MetadataRead read("other scene");
        read.SetBytes(1);
    }
    require(!EndFrame().has_value());
    BeginFrame(0x5b, 11, 9, 7, true, true);
    result = EndFrame();
    require(result.has_value());
    require((*result)["all_threads_completed_work"]["metadata_reads"] == 0);
    BeginFrame(0x5b, 11, 9, 8, true, true);
    require(!EndFrame().has_value());

    // State teardown and the following initialization live outside the ordinary
    // frame interval. A short reload still reports both main and worker imports.
    BeginStateReload(0x5b, 9, true, true);
    {
        Scope lookup(&Frame::lookupNanos);
        lookup.MarkCacheHit();
        frame.lookups++;
        frame.cacheHits++;
        MetadataRead read("reload-main.o2r");
        read.SetBytes(10);
    }
    std::thread reloadWorker([] {
        Scope lookup(&Frame::lookupNanos);
        lookup.MarkCacheMiss();
        Scope parse(&Frame::parseNanos);
        Scope decode(&Frame::decodeNanos);
        MetadataRead read("reload-worker.o2r");
        read.SetBytes(20);
    });
    reloadWorker.join();
    result = EndStateReload(0x52, 4);
    require(result.has_value());
    require((*result)["kind"] == "state_reload" && (*result)["state_reload_ms"].get<double>() >= 0.0);
    require((*result)["source_scene"] == 0x5b && (*result)["source_room"] == 9);
    require((*result)["target_scene"] == 0x52 && (*result)["target_room"] == 4);
    require(!result->contains("frame_ms") && !result->contains("update_ms") && !result->contains("graphics_ms"));
    require((*result)["metadata_reads"] == 1 && (*result)["metadata_bytes"] == 10);
    require((*result)["material_lookups"] == 1 && (*result)["metadata_cache_hits"] == 1);
    require((*result)["all_threads_completed_work"]["metadata_reads"] == 2);
    require((*result)["all_threads_completed_work"]["metadata_bytes"] == 30);
    require((*result)["all_threads_completed_work"]["material_lookups"] == 2);
    require((*result)["all_threads_completed_work"]["metadata_cache_hits"] == 1);
    require((*result)["all_threads_completed_work"]["metadata_cache_misses"] == 1);
    require(!EndStateReload(-1, -1).has_value());

    BeginStateReload(0x51, 2, true, true);
    {
        MetadataRead read("not-lost-woods");
        read.SetBytes(1);
    }
    require(!EndStateReload(0x5b, 9).has_value());
    BeginStateReload(0x5b, 9, true, false);
    {
        MetadataRead read("disabled-reload");
        read.SetBytes(1);
    }
    require(!EndStateReload(0x52, 4).has_value());
    std::cout << "PASS main-thread attribution, all-thread completed work, state reload interval, room tags, frame "
                 "reset, quiet warm frames, disabled mode and scene filter\n";
}
