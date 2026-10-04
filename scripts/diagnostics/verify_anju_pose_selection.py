"""Check the production Anju pose selector and decode both native loops.

Usage: python verify_anju_pose_selection.py MM_ARCHIVE R8_OR_NEWER_HD_ARCHIVE [...]
The C++ unit fixture substitutes archive lookup at the graph-resolver boundary;
the Python readback separately resolves actual archive geometry and animations.
This does not claim an in-game or full ResourceManager integration test.
"""
from pathlib import Path
import argparse
import functools
import json
import re
import subprocess
import tempfile
import zipfile

import numpy as np

import render_anju_preview
from render_anju_preview import Anju

ROOT = Path(__file__).resolve().parents[2]
PREFIX = "objects/object_anju_hd/v1/"
STANDING = {
    2: "gAnju1TorsoStandingDL", 3: "gAnju1LeftUpperArmStandingDL",
    4: "gAnju1LeftForearmStandingDL", 5: "gAnju1RelaxedLeftHandDL",
    7: "gAnju1RightForearmStandingDL", 8: "gAnju1RightHandStandingDL",
}
SKIRT = {
    10: "gAnju1PelvisStandingDL", 11: "gAnju1RightThighStandingDL",
    12: "gAnju1RightShinStandingDL", 14: "gAnju1LeftThighStandingDL",
    15: "gAnju1LeftShinStandingDL", 17: "gAnju1Skirt1StandingDL",
    18: "gAnju1Skirt2StandingDL", 20: "gAnju1Skirt4StandingDL",
}


def function(source, name):
    match = re.search(r'^[^\n;{}]*\b' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


def select_standing(model):
    for limb, name in STANDING.items():
        model.dlists[limb - 1] = PREFIX + name
    if any(PREFIX + name in model.resources for name in SKIRT.values()):
        assert all(PREFIX + name in model.resources for name in SKIRT.values())
        for limb, name in SKIRT.items():
            model.dlists[limb - 1] = PREFIX + name
    model.umbrella = PREFIX + "gAnju2UmbrellaStandingDL"


def check_selector(paths):
    source = (ROOT / "soh/mods/transformation_masks/assets/mm_asset_loader.cpp").read_text()
    header = (ROOT / "soh/mods/transformation_masks/assets/mm_asset_loader.h").read_text()
    declarations = source[source.index("static constexpr const char* kAnjuModelPrefix"):
                          source.index("struct MmDisplayListGraphContext")]
    model_type = re.search(r'typedef struct MmAnjuDisplayLists.*?} MmAnjuDisplayLists;', header, re.S)[0]
    fixture = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
struct Gfx { unsigned value; };
MODEL_TYPE
DECLARATIONS
struct Archive {
    std::map<std::string, Gfx> files;
    bool HasFile(const std::string& path) { return files.count(path); }
};
struct MmDisplayListGraphContext {
    std::shared_ptr<Archive> archive = std::make_shared<Archive>();
    bool optionalAnju = true;
    bool anjuAttempted[2] = {}, anjuComplete[2] = {};
    MmAnjuDisplayLists anju[2] = {};
};
static Gfx* MmAssets_PatchDisplayListGraph(const std::string& path, MmDisplayListGraphContext& graph, int) {
    auto it = graph.archive->files.find(path);
    return it == graph.archive->files.end() ? nullptr : &it->second;
}
PRODUCTION
static MmDisplayListGraphContext complete() {
    MmDisplayListGraphContext graph;
    for (const char* path : { PATHS }) graph.archive->files.emplace(path, Gfx{});
    return graph;
}
int main() {
    auto graph = complete();
    auto seated = MmAssets_LoadAnjuModel(graph, 0);
    assert(seated && seated->custom);
    auto snapshot = *seated;
    auto standing = MmAssets_LoadAnjuModel(graph, 1);
    assert(standing && standing != seated && standing->custom);
    for (unsigned i = 2; i <= 20; ++i) assert(seated->limbs[i] == snapshot.limbs[i]);
    CHECK_STANDING
    assert(standing->umbrella == &graph.archive->files.at(std::string(kAnjuModelPrefix) + "gAnju2UmbrellaStandingDL"));
    assert(standing->umbrella != seated->umbrella);
    for (unsigned i = 0; i < 3; ++i) assert(standing->heads[i] == seated->heads[i]);
    for (unsigned i = 0; i < 8; ++i) {
        assert(MmAssets_LoadAnjuModel(graph, 0) == seated);
        assert(MmAssets_LoadAnjuModel(graph, 1) == standing);
    }
    assert(!MmAssets_LoadAnjuModel(graph, 2));
    for (const char* name : { REQUIRED }) {
        auto broken = complete();
        broken.archive->files.erase(std::string(kAnjuModelPrefix) + name);
        assert(!MmAssets_LoadAnjuModel(broken, 1));
        assert(!MmAssets_LoadAnjuModel(broken, 1));
        assert(MmAssets_LoadAnjuModel(broken, 0));
    }
    // A partial preserved-standing skirt must never mix with a seated refit.
    auto partial = complete();
    for (const char* name : { SKIRT_NAMES }) partial.archive->files.erase(std::string(kAnjuModelPrefix) + name);
    partial.archive->files.emplace(std::string(kAnjuModelPrefix) + "gAnju1PelvisStandingDL", Gfx{});
    assert(!MmAssets_LoadAnjuModel(partial, 1));
    assert(MmAssets_LoadAnjuModel(partial, 0));
    auto fitted = complete();
    for (const char* name : { SKIRT_NAMES }) fitted.archive->files.emplace(std::string(kAnjuModelPrefix) + name, Gfx{});
    auto fit = MmAssets_LoadAnjuModel(fitted, 1);
    assert(fit);
    CHECK_SKIRT
    auto native = complete(); native.optionalAnju = false;
    for (unsigned i = 2; i <= 20; ++i) native.archive->files.emplace(std::string("objects/object_an1/") + kAnjuModelLimbs[i], Gfx{});
    native.archive->files.emplace("objects/object_an2/gAnju2UmbrellaDL", Gfx{});
    assert(MmAssets_LoadAnjuModel(native, 1) == MmAssets_LoadAnjuModel(native, 0));
    assert(!MmAssets_LoadAnjuModel(native, 1)->custom);
    puts("PASS production Anju selector: distinct poses, immutable seated data, cache reuse, missing standing roots, partial/complete skirt and native sharing");
}
'''
    checks = lambda model, owner, values: "\n".join(
        f'assert({model}->limbs[{limb}] == &{owner}.archive->files.at(std::string(kAnjuModelPrefix) + "{name}"));'
        for limb, name in values.items())
    replacements = {
        "MODEL_TYPE": model_type, "DECLARATIONS": declarations,
        "PRODUCTION": function(source, "MmAssets_LoadAnjuModel"),
        "PATHS": ",\n".join(json.dumps(p) for p in paths),
        "CHECK_STANDING": checks("standing", "graph", STANDING),
        "CHECK_SKIRT": checks("fit", "fitted", SKIRT),
        "REQUIRED": ",".join(json.dumps(n) for n in [*STANDING.values(), "gAnju2UmbrellaStandingDL"]),
        "SKIRT_NAMES": ",".join(json.dumps(n) for n in SKIRT.values()),
    }
    for token, value in replacements.items():
        fixture = fixture.replace(token, value)
    with tempfile.TemporaryDirectory(prefix="anju-selector-") as folder:
        cpp = Path(folder) / "selector.cpp"
        cpp.write_text(fixture)
        binary = Path(folder) / "selector"
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("archives", nargs="+", type=Path)
    args = parser.parse_args()
    # The renderer queries texture headers per triangle. Cache immutable payloads
    # so HD texture bodies are not recopied for every header-only query.
    render_anju_preview.read_texture = functools.lru_cache(maxsize=32)(render_anju_preview.read_texture)
    for archive in args.archives:
        with zipfile.ZipFile(archive) as pack:
            assert pack.testzip() is None
            check_selector(pack.namelist())
        for standing, frames in ((False, 43), (True, 32)):
            model = Anju(args.native, standing, archive)
            if standing:
                select_standing(model)
            assert len(model.clips[model.animation]) == frames
            for frame in range(frames):
                parts = model.pose(model.animation, frame)
                assert parts
                for part in parts:
                    assert np.isfinite(part["pos"]).all()
                    assert np.isfinite(part["normal"]).all()
            print(f"PASS {archive.name}: {frames} real {'standing' if standing else 'seated'} animation frames and archive geometry")


if __name__ == "__main__":
    main()
