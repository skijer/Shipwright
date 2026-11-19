"""Add each MM song's icon to a personal MM archive: the game's own note in the song's quest colour."""
import argparse
import struct
import zipfile
from pathlib import Path

RESOURCE_HEADER_SIZE = 0x40
TEXTURE_IA8 = 8
TEXTURE_RGBA32 = 1
ICON_SIZE = 32
NOTE_SOURCE = "icon_item_static_yar/gItemIconSongNoteTex"
# MM's sQuestSongsPrim colours; mm_collectables reads these icons from textures/mm_song_notes.
NOTE_TINTS = {"SonataOfAwakening": (150, 255, 100), "GoronLullaby": (255, 80, 40),
              "NewWaveBossaNova": (100, 150, 255), "ElegyOfEmptiness": (255, 160, 0),
              "OathToOrder": (255, 100, 255), "SongOfHealing": (255, 255, 255),
              "SongOfSoaring": (255, 255, 255), "FugueOfHome": (255, 255, 255),
              "BalladOfHero": (255, 255, 255)}


def tinted_note_icon(note, tint):
    """The 16x24 IA8 note centred in a 32x32 RGBA32 icon, coloured as the quest screen colours it."""
    texture_type, width, height, size = struct.unpack_from("<4I", note, RESOURCE_HEADER_SIZE)
    if texture_type != TEXTURE_IA8 or width > ICON_SIZE or height > ICON_SIZE:
        raise ValueError("The song note is not the IA8 texture this tool knows")
    texels = note[RESOURCE_HEADER_SIZE + 16:RESOURCE_HEADER_SIZE + 16 + size]
    left = (ICON_SIZE - width) // 2
    top = (ICON_SIZE - height) // 2
    pixels = bytearray(ICON_SIZE * ICON_SIZE * 4)
    for index, texel in enumerate(texels):
        intensity = (texel >> 4) * 17
        offset = ((top + index // width) * ICON_SIZE + left + index % width) * 4
        pixels[offset:offset + 4] = bytes(channel * intensity // 255 for channel in tint) + bytes([(texel & 15) * 17])
    layout = struct.pack("<4I", TEXTURE_RGBA32, ICON_SIZE, ICON_SIZE, len(pixels))
    return note[:RESOURCE_HEADER_SIZE] + layout + bytes(pixels)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True, help="the mods folder's mm.o2r")
    args = parser.parse_args()
    with zipfile.ZipFile(args.archive) as archive:
        note = archive.read(NOTE_SOURCE)
        present = set(archive.namelist())
    icons = {f"textures/mm_song_notes/gItemIcon{song}Tex": tinted_note_icon(note, tint)
             for song, tint in NOTE_TINTS.items()}
    with zipfile.ZipFile(args.archive, "a", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in icons.items():
            if name not in present:
                archive.writestr(name, data)
    print(f"Added {len(icons)} song note icons to {args.archive}")


if __name__ == "__main__":
    main()
