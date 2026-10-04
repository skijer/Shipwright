"""Move only room-10 Saria two units left in the accepted Lost Woods POC3.

The two compiled normal/Alt room records and retained Prelude placement agree.
Every other archive member, including collision and weather, is byte-preserved.
"""
import argparse
import hashlib
import json
import pathlib
import struct
import zipfile

BASE_SHA256 = "988c7c0a49e43acdc941894de9ba391f32831a302205e6699251ea80d83979d6"
ROOM = "custom/prelude/spot10_scene/added/room0"
PROJECT = "prelude/project/edits.json"


def build(source, destination):
    if source.resolve() == destination.resolve():
        raise ValueError("Write the candidate separately from the accepted source")
    if hashlib.sha256(source.read_bytes()).hexdigest() != BASE_SHA256:
        raise ValueError("Expected the exact accepted Continuous Rain POC3 archive")
    old = struct.pack("<HhhhhhhH", 42, -710, -68, -2373, 0, 16575, 0, 0x7F23)
    new = struct.pack("<HhhhhhhH", 42, -710, -68, -2371, 0, 16575, 0, 0x7F23)
    with zipfile.ZipFile(source) as original:
        project = json.loads(original.read(PROJECT))
        actors = next(edit["data"]["rooms"]["10"]["0"]
                      for edit in project["edits"]["spot10_scene"] if edit["kind"] == "actor")
        matches = [actor for actor in actors if actor["id"] == 42 and actor["params"] == 0x7F23]
        if len(matches) != 1 or matches[0]["pos"] != [-710, -68, -2373]:
            raise ValueError("Room-10 Saria placement does not match the reviewed source")
        matches[0]["pos"][2] += 2
        replacements = {PROJECT: json.dumps(project, separators=(",", ":"), ensure_ascii=False).encode()}
        for name in (ROOM, "alt/" + ROOM):
            data = original.read(name)
            if data.count(old) != 1:
                raise ValueError(f"Expected exactly one compiled Saria in {name}")
            replacements[name] = data.replace(old, new)
        destination.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(destination, "w") as candidate:
            candidate.comment = original.comment
            for entry in original.infolist():
                candidate.writestr(entry, replacements.get(entry.filename, original.read(entry.filename)))
    with zipfile.ZipFile(source) as original, zipfile.ZipFile(destination) as candidate:
        assert original.namelist() == candidate.namelist()
        changed = [name for name in original.namelist() if original.read(name) != candidate.read(name)]
        assert set(changed) == set(replacements)
        assert candidate.read(ROOM) == candidate.read("alt/" + ROOM)
        restored = json.loads(candidate.read(PROJECT))
        restored_actors = next(edit["data"]["rooms"]["10"]["0"]
                              for edit in restored["edits"]["spot10_scene"] if edit["kind"] == "actor")
        next(actor for actor in restored_actors if actor["id"] == 42 and actor["params"] == 0x7F23)["pos"][2] -= 2
        assert restored == json.loads(original.read(PROJECT))
        print(f"PASS {len(changed)} changed entries; {len(original.namelist()) - len(changed)} byte-identical entries")
    print(f"SHA256 {hashlib.sha256(destination.read_bytes()).hexdigest()}")
    print(destination)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    options = parser.parse_args()
    build(options.source, options.destination)
