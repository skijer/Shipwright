"""Exercise the real Kokiri adapter and native frame sampler using a supplied oot.o2r.

The source archive is read-only. Animation data is decoded to a temporary C
fixture; no game assets are copied into the repository or edited.
"""
import argparse
import pathlib
import struct
import subprocess
import tempfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]

def animation(data):
    assert data[4:8] == b"MNAO"
    kind, frames, count = struct.unpack_from("<IhI", data, 64)
    assert kind == 0
    offset = 74
    values = struct.unpack_from(f"<{count}h", data, offset)
    offset += count * 2
    joints, = struct.unpack_from("<I", data, offset)
    offset += 4
    indices = [struct.unpack_from("<HHH", data, offset+i*6) for i in range(joints)]
    offset += joints * 6
    limit, = struct.unpack_from("<H", data, offset)
    assert offset + 2 == len(data)
    return frames, values, indices, limit

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("oot", type=pathlib.Path)
    parser.add_argument("--cc", default="cc")
    args = parser.parse_args()
    with zipfile.ZipFile(args.oot) as archive:
        frames, values, indices, limit = animation(archive.read("objects/object_dy_obj/gGreatFairySittingAnim"))
        _, seat_values, seat_indices, _ = animation(archive.read("objects/object_os_anime/gKokiriSittingCrossedLegsAnim"))
        _, arms_values, arms_indices, _ = animation(archive.read("objects/object_os_anime/gKokiriSittingCrossedArmsLegsAnim"))
    with tempfile.TemporaryDirectory(prefix="kokiri-pose-") as folder:
        build = pathlib.Path(folder)
        fixture = "static s16 fixtureValues[] = {" + ",".join(map(str, values)) + "};\n"
        fixture += "static JointIndex fixtureIndices[] = {" + ",".join("{%d,%d,%d}" % row for row in indices) + "};\n"
        fixture += f"static AnimationHeader fixtureDonor = {{ {{ {frames} }}, fixtureValues, fixtureIndices, {limit} }};\n"
        fixture += "static const Vec3s fixtureSeated[16] = {" + ",".join("{%d,%d,%d}" % tuple(seat_values[i] for i in row) for row in seat_indices) + "};\n"
        fixture += "static const Vec3s fixtureArmsCrossed[16] = {" + ",".join("{%d,%d,%d}" % tuple(arms_values[i] for i in row) for row in arms_indices) + "};\n"
        (build / "kokiri_pose_fixture.h").write_text(fixture)
        command = [args.cc, "-std=gnu11", "-g", "-O1", "-ffunction-sections", "-fdata-sections",
                   "-Wno-incompatible-pointer-types", "-Wno-int-conversion", "-DLOG_LEVEL_GAME_PRINTS=0",
                   '-DCVAR_PREFIX_ENHANCEMENT="gEnhancements"',
                   "-Isoh/include", "-Isoh/src", "-Isoh/assets", "-Isoh", "-Ilibultraship/include", "-I" + str(build),
                   "soh/tests/static_story_kokiri_pose_test.c",
                   "soh/src/overlays/actors/ovl_En_Viewer/static_story_kokiri.c",
                   "soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c",
                   "soh/src/code/z_skelanime.c", "-Wl,--gc-sections",
                   "-Wl,--wrap=SkelAnime_InitFlex", "-Wl,--wrap=Animation_PlayLoopSetSpeed",
                   "-lm", "-o", str(build / "test")]
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
        if result.returncode:
            print(result.stderr)
            raise SystemExit(result.returncode)
        subprocess.run([str(build / "test")], check=True)

if __name__ == "__main__":
    main()
