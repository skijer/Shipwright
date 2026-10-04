"""Keep the user-accepted Fire projectile body, textured wake and history trail exact."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = r'''
#include "soh/Enhancements/randomizer/NeiUsedMagicPolicy.h"
#include "tests/nei_used_fx/fixtures/accepted_fire_policy.h"
#include <cassert>
#include <cstdio>
void equal(const NeiGi::Mesh& a, const NeiGi::Mesh& b) {
    assert(a.count == b.count);
    for (size_t i=0; i<a.count; ++i) {
        const auto &x=a.vertices[i], &y=b.vertices[i];
        assert(x.p.x==y.p.x && x.p.y==y.p.y && x.p.z==y.p.z);
        assert(x.rgb==y.rgb && x.alpha==y.alpha && x.u==y.u && x.v==y.v);
    }
}
int main() {
    using namespace NeiGi;
    const Point trail[]={{0,0,0},{-15,-3,-1},{-30,-6,-2},{-45,-9,-3},{-60,-12,-4},{-75,-15,-5}};
    const Point directions[]={{1,.2f,.1f},{0,0,1},{0,1,0}};
    for (unsigned frame=0; frame<180; ++frame) {
        for (float scale : {.2f,1.f,2.f,4.f}) {
            for (Point direction : directions) {
                equal(AcceptedFire::SampleProjectile(Kind::Fire,frame,scale,direction),
                      NeiUsedMagic::SampleProjectile(Kind::Fire,frame,scale,direction));
                equal(AcceptedFire::SampleProjectileSurface(Kind::Fire,frame,scale,direction),
                      NeiUsedMagic::SampleProjectileSurface(Kind::Fire,frame,scale,direction));
            }
            equal(AcceptedFire::SampleTrail(Kind::Fire,frame,trail,6,scale),
                  NeiUsedMagic::SampleTrail(Kind::Fire,frame,trail,6,scale));
        }
    }
    puts("PASS accepted Fire body, per-head textured wake and shared trail exact over 180 frames");
}
'''
with tempfile.TemporaryDirectory(prefix="accepted-fire-") as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(SOURCE)
    subprocess.run([*shlex.split(os.environ.get("CXX", "c++")), "-std=c++20", "-O2",
                    "-I" + str(ROOT), "-I" + str(ROOT / "soh"), str(path / "test.cpp"),
                    "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
