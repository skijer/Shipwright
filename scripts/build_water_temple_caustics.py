#!/usr/bin/env python3
"""Build a material-only Water Temple caustics O2R from explicitly selected XML.

No geometry is inspected or copied. See docs/stabilization/water-temple-caustics.md
for the deliberately narrow supported opaque material grammar and installation.
MPQ input requires mpyq; ZIP O2R input uses only the Python standard library.
"""
import argparse
from collections import Counter
from contextlib import contextmanager
import hashlib
import json
from pathlib import Path
import re
import struct
import tempfile
import xml.etree.ElementTree as ET
import zipfile

DEFAULT_MATERIAL = 'scenes/shared/spot07_scene/mat_spot07_room_1DL_004200_ZoraDomain00'
DONOR_TEXTURE = 'scenes/nonmq/MIZUsin_scene/MIZUsin_sceneTex_014430'
CAUSTIC_TEXTURE = 'custom/prelude/textures/water_temple_caustics_rgba16'
NATIVE_ANIMATION = dict(version=1, binding='material-motion', source='oot.water_temple.caustics',
                        logicalWidth=32, logicalHeight=32)
MOTION_SOURCES = {'oot.water_temple.caustics': ('WaterTempleCaustics', 10),
                  'oot.zoras_domain.caustics': ('ZorasDomainCaustics', 11)}
GEOMETRY = dict(G_ZBUFFER=1, G_SHADE=4, G_CULL_FRONT=0x200, G_CULL_BACK=0x400,
                G_CULL_BOTH=0x600, G_FOG=0x10000, G_LIGHTING=0x20000,
                G_TEXTURE_GEN=0x40000, G_TEXTURE_GEN_LINEAR=0x80000,
                G_SHADING_SMOOTH=0x200000, G_CLIPPING=0x800000)
WRAP = dict(G_TX_WRAP=0, G_TX_NOMIRROR=0, G_TX_MIRROR=1, G_TX_CLAMP=2)
SIZES = dict(G_IM_SIZ_16b=2, G_IM_SIZ_16b_LOAD_BLOCK=2)
# Same F3DEX2 constants as libultraship/libultra/gbi.h. Checked against its macros
# and the actual StrHash64.cpp by the focused synthetic compiler test.
HIGH_GROUPS = (
    dict(G_AD_PATTERN=0, G_AD_NOTPATTERN=16, G_AD_NOISE=32, G_AD_DISABLE=48),
    dict(G_CD_MAGICSQ=0, G_CD_BAYER=64, G_CD_NOISE=128),
    dict(G_CK_NONE=0), dict(G_TC_FILT=6 << 9),
    dict(G_TF_POINT=0, G_TF_AVERAGE=3 << 12, G_TF_BILERP=2 << 12),
    dict(G_TL_TILE=0), dict(G_TD_CLAMP=0), dict(G_TP_NONE=0, G_TP_PERSP=1 << 19),
    dict(G_CYC_2CYCLE=1 << 20), dict(G_PM_NPRIMITIVE=0, G_PM_1PRIMITIVE=1 << 23),
)
LOW_GROUPS = (dict(G_AC_NONE=0), dict(G_ZS_PIXEL=0),
              dict(G_RM_FOG_SHADE_A=0xC8000000, G_RM_PASS=0x0C080000),
              dict(G_RM_AA_ZB_OPA_SURF2=0x00112078))
COMBINE_ATTRS = 'A0 B0 C0 D0 Aa0 Ab0 Ac0 Ad0 A1 B1 C1 D1 Aa1 Ab1 Ac1 Ad1'.split()
SOURCE_COMBINE = dict(zip(COMBINE_ATTRS, (
    'G_CCMUX_TEXEL0 G_CCMUX_0 G_CCMUX_SHADE G_CCMUX_0 '
    'G_ACMUX_0 G_ACMUX_0 G_ACMUX_0 G_ACMUX_1 '
    'G_CCMUX_COMBINED G_CCMUX_0 G_CCMUX_PRIMITIVE G_CCMUX_0 '
    'G_ACMUX_0 G_ACMUX_0 G_ACMUX_0 G_ACMUX_COMBINED').split()))
SCHEMAS = {
    'PipeSync': '', 'TileSync': '', 'LoadSync': '', 'EndDisplayList': '',
    'SetCombineLERP': ' '.join(COMBINE_ATTRS),
    'Texture': 'S T Level Tile On', 'SetTextureLUT': 'Mode',
    'SetTextureImage': 'Path Format Size Width',
    'SetTile': 'Format Size Line TMem Tile Palette Cms0 Cms1 Cmt0 Cmt1 MaskS ShiftS MaskT ShiftT',
    'LoadBlock': 'Tile Uls Ult Lrs Dxt', 'SetTileSize': 'T Uls Ult Lrs Lrt',
    'SetPrimColor': 'M L R G B A',
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def resource_path(path):
    require(isinstance(path, str) and bool(path) and not path.startswith(('alt/', '/')) and
            all(re.fullmatch(r'[A-Za-z0-9_. -]+', part) and part not in ('.', '..')
                for part in path.split('/')), f'Invalid unprefixed resource path: {path!r}')
    return path


def number(attrs, key, maximum):
    value = attrs[key]
    require(bool(re.fullmatch(r'[0-9]+', value)), f'{key}: expected unsigned decimal integer')
    value = int(value)
    require(0 <= value <= maximum, f'{key}: expected 0..{maximum}')
    return value


def choice(attrs, key, choices):
    require(attrs[key] in choices, f'{key}: unsupported value {attrs[key]!r}')
    return choices[attrs[key]]


def crc64(path):
    """StrHash64::CRC64 string hash: all-ones seed, ECMA polynomial, no final XOR."""
    resource_path(path)
    value = 0xffffffffffffffff
    for byte in path.encode('utf-8'):
        value ^= byte << 56
        for _ in range(8):
            value = ((value << 1) ^ (0x42f0e1eba9ea3693 if value >> 63 else 0)) & 0xffffffffffffffff
    return value


def other_mode(opcode, shift, length, value):
    return (opcode << 24 | (32-shift-length) << 8 | (length-1), value)


def texture_image(path, width=1):
    h = crc64(path)
    # Texture hash is a distinct payload pair, high half first. XML factory
    # appends PipeSync after SetTextureImage; retain that behavior as well.
    return [(0x20100000 | (width-1), 0), (h >> 32, h & 0xffffffff), (0xE7000000, 0)]


def tile(attrs):
    a = attrs
    require(a['Format'] == 'G_IM_FMT_RGBA', 'Only RGBA16 base textures are supported')
    size = choice(a, 'Size', SIZES)
    w0 = (0xF5000000 | size << 19 | number(a, 'Line', 511) << 9 | number(a, 'TMem', 511))
    w1 = (number(a, 'Tile', 7) << 24 | number(a, 'Palette', 15) << 20 |
          (choice(a, 'Cmt0', WRAP) | choice(a, 'Cmt1', WRAP)) << 18 |
          number(a, 'MaskT', 15) << 14 | number(a, 'ShiftT', 15) << 10 |
          (choice(a, 'Cms0', WRAP) | choice(a, 'Cms1', WRAP)) << 8 |
          number(a, 'MaskS', 15) << 4 | number(a, 'ShiftS', 15))
    require(number(a, 'Palette', 15) == 0, 'RGBA palette must be zero')
    return w0, w1


def extent(a, opcode, tile_key, last_key):
    uls, ult = number(a, 'Uls', 4095), number(a, 'Ult', 4095)
    lrs, last = number(a, 'Lrs', 4095), number(a, last_key, 4095)
    if opcode == 0xF2:
        require(lrs >= uls and last >= ult, 'Inverted tile size')
    return (opcode << 24 | uls << 12 | ult,
            number(a, tile_key, 7) << 24 | lrs << 12 | last)


def mode(node):
    a = node.attrib
    require(a.get('Cmd') in ('G_SETOTHERMODE_H', 'G_SETOTHERMODE_L'), 'Unsupported other-mode command')
    high = a['Cmd'] == 'G_SETOTHERMODE_H'
    groups = HIGH_GROUPS if high else LOW_GROUPS
    flags = {key for group in groups for key in group}
    require(set(a) <= flags | {'Cmd', 'Sft', 'Length'}, 'Unsupported other-mode attribute/blend')
    require(a.get('Sft') == ('4' if high else '0') and a.get('Length') == ('20' if high else '32'),
            'Only complete supported opaque other-mode state is accepted')
    value = 0
    for group in groups:
        selected = set(group) & set(a)
        require(len(selected) == 1, f'Expected exactly one flag from {list(group)}')
        flag = selected.pop()
        require(a[flag] == '1', f'{flag}: flag must be 1 (XML factory tests attribute presence)')
        value |= group[flag]
    return other_mode(0xE3 if high else 0xE2, 4 if high else 0, 20 if high else 32, value)


def compile_material(xml, caustic_path, strength=95, shift_s=3, shift_t=0):
    """Compile the documented single-load, opaque XML grammar, failing closed."""
    for name, value, maximum in (('strength', strength, 255), ('shift S', shift_s, 15), ('shift T', shift_t, 15)):
        require(type(value) is int and 0 <= value <= maximum, f'{name}: expected 0..{maximum}')
    resource_path(caustic_path)
    try:
        root = ET.fromstring(xml)
    except ET.ParseError as exc:
        raise ValueError(f'Selected material is not valid XML: {exc}') from exc
    require(root.tag == 'DisplayList' and root.attrib == {'Version': '0'}, 'Expected DisplayList Version="0"')
    nodes = list(root)
    require(nodes and nodes[-1].tag == 'EndDisplayList', 'Material must end with EndDisplayList')
    counts = Counter(n.tag for n in nodes)
    for tag in ('SetCombineLERP', 'Texture', 'SetTextureLUT', 'SetTextureImage', 'LoadBlock', 'SetTileSize',
                'SetPrimColor', 'EndDisplayList'):
        require(counts[tag] == 1, f'Expected exactly one {tag}; multitexture/incomplete materials unsupported')
    require(counts['SetOtherMode'] == 2 and counts['SetTile'] == 2,
            'Expected high/low state and load-tile 7/render-tile 0 descriptors')
    require(not (root.text or '').strip(), 'Unexpected display-list text')
    commands = []
    modes = set()
    phase = 0
    # No control flow or geometry commands are in this allowlist. Ordering
    # establishes one complete image -> load -> render transaction.
    for node in nodes:
        tag, a = node.tag, node.attrib
        require(len(node) == 0 and not (node.text or '').strip() and not (node.tail or '').strip(),
                f'{tag}: nested commands/text unsupported')
        if tag in ('SetGeometryMode', 'ClearGeometryMode'):
            require(set(a) <= GEOMETRY.keys(), f'{tag}: unsupported geometry flag')
            require(all(v == '1' for v in a.values()), f'{tag}: flags must be 1')
            require(tag != 'SetGeometryMode' or not ({'G_TEXTURE_GEN', 'G_TEXTURE_GEN_LINEAR'} & set(a)),
                    'Texture coordinate generation is unsupported; use authored UVs')
            bits = 0
            for flag in a:
                bits |= GEOMETRY[flag]
            commands.append((0xD9FFFFFF, bits) if tag == 'SetGeometryMode' else (0xD9000000 | (~bits & 0xffffff), 0))
            continue
        if tag == 'SetOtherMode':
            commands.append(mode(node)); modes.add(a['Cmd']); continue
        require(tag in SCHEMAS, f'Unsupported material opcode: {tag}')
        require(set(a) == set(SCHEMAS[tag].split()), f'{tag}: unsupported/missing XML attributes')
        if tag in ('PipeSync', 'TileSync', 'LoadSync'):
            commands.append(({'PipeSync': 0xE7, 'TileSync': 0xE8, 'LoadSync': 0xE6}[tag] << 24, 0))
            if tag == 'LoadSync' and phase == 2:
                phase = 3
        elif tag == 'SetCombineLERP':
            require(a == SOURCE_COMBINE, 'Unsupported combine/alpha: expected opaque texture*SHADE, then white primitive')
            # (TEXEL1-TEXEL0)*ENV_ALPHA+TEXEL0, then COMBINED*SHADE.
            # Constant-one alpha in cycle 0, COMBINED alpha in cycle 1.
            commands.append((0xFC000000 | 2 << 20 | 12 << 15 | 7 << 12 | 7 << 9 | 4,
                             1 << 28 | 15 << 24 | 7 << 21 | 7 << 18 | 1 << 15 | 7 << 12 |
                             6 << 9 | 7 << 6 | 7 << 3))
        elif tag == 'Texture':
            s, t = number(a, 'S', 65535), number(a, 'T', 65535)
            require(a['Level'] == '0' and a['Tile'] == '0' and a['On'] == '1', 'Only enabled tile-0, level-0 texturing supported')
            commands.append((0xD7000002, s << 16 | t))
        elif tag == 'SetTextureLUT':
            require(a['Mode'] == 'G_TT_NONE', 'Palette texture layouts unsupported')
            commands.append(other_mode(0xE3, 14, 2, 0))
        elif tag == 'SetTextureImage':
            require(phase == 0 and a['Format'] == 'G_IM_FMT_RGBA', 'Only one RGBA16 texture load supported')
            choice(a, 'Size', SIZES)
            width = number(a, 'Width', 4096)
            require(width > 0, 'Texture width must be positive')
            commands.extend(texture_image(resource_path(a['Path']), width)); phase = 1
        elif tag == 'SetTile':
            encoded = tile(a)
            require(a['TMem'] == '0', 'Base texture must use TMEM 0')
            if phase == 1:
                require(a['Tile'] == '7' and a['Line'] == '0', 'Expected load tile 7 with line 0')
                phase = 2
            elif phase == 4:
                require(a['Tile'] == '0' and int(a['Line']) > 0, 'Expected render tile 0 with positive line')
                phase = 5
            else:
                raise ValueError('Unsupported tile command order')
            commands.append(encoded)
        elif tag == 'LoadBlock':
            require(phase == 3 and a['Tile'] == '7', 'Expected load sync then block load on tile 7')
            commands.append(extent(a, 0xF3, 'Tile', 'Dxt')); phase = 4
        elif tag == 'SetTileSize':
            require(phase == 5 and a['T'] == '0', 'Expected tile-0 size after render descriptor')
            commands.append(extent(a, 0xF2, 'T', 'Lrt')); phase = 6
        elif tag == 'SetPrimColor':
            require(all(a[k] == '255' for k in ('R', 'G', 'B', 'A')), 'Nonwhite primitive tint/alpha unsupported')
            commands.append((0xFA000000 | number(a, 'M', 255) << 8 | number(a, 'L', 255), 0xffffffff))
        elif tag == 'EndDisplayList':
            require(phase == 6, 'Incomplete base texture load')
    require(modes == {'G_SETOTHERMODE_H', 'G_SETOTHERMODE_L'}, 'Missing high/low render state')
    commands.append((0xE8000000, 0))
    commands.extend(texture_image(caustic_path))
    caustic = dict(Format='G_IM_FMT_RGBA', Size='G_IM_SIZ_16b', Line='0', TMem='256', Tile='7', Palette='0',
                   Cms0='G_TX_WRAP', Cms1='G_TX_NOMIRROR', Cmt0='G_TX_WRAP', Cmt1='G_TX_NOMIRROR',
                   MaskS='5', ShiftS=str(shift_s), MaskT='5', ShiftT=str(shift_t))
    commands.extend((tile(caustic), (0xE6000000, 0), (0xF3000000, 0x073FF100), (0xE7000000, 0),
                     tile(caustic | dict(Tile='1', Line='8')), (0xF2000000, 0x0107C07C),
                     (0xD9FDFFFF, 0), (0xD9FFFFFF, GEOMETRY['G_SHADE']),
                     (0xFB000000, strength), (0xDF000000, 0)))
    header = b'\0\1\0\0TLDO' + bytes(56)
    return header + b'\4' + bytes(7) + b''.join(struct.pack('<II', *cmd) for cmd in commands)


def validate_donor(data):
    require(len(data) >= 80 and data[0] == 0 and data[4:8] == b'XETO' and data[8:12] == bytes(4),
            'Donor must be a little-endian native v0 texture')
    require(struct.unpack_from('<4I', data, 64) == (2, 32, 32, 2048) and len(data) == 2128,
            'Donor must be exactly 32x32 RGBA16 (2048 texel bytes)')


@contextmanager
def archive_reader(path):
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            names = archive.namelist()
            require(len(names) == len(set(names)), 'Archive contains duplicate resource names')
            yield archive.read
    else:
        try:
            import mpyq
        except ImportError as exc:
            raise ValueError('MPQ .otr input requires mpyq (python3 -m pip install mpyq)') from exc
        archive = mpyq.MPQArchive(str(path), listfile=False)
        def read(name):
            result = archive.read_file(name)
            if result is None:
                result = archive.read_file(name.replace('/', '\\'))
            if result is None:
                raise KeyError(name)
            return result
        try:
            yield read
        finally:
            archive.file.close()


def sha256(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def build(source, vanilla, selected, output, strength=95, shift_s=3, shift_t=0,
          motion_source='oot.water_temple.caustics'):
    require(motion_source in MOTION_SOURCES, f'Unsupported caustic motion source: {motion_source!r}')
    animation = NATIVE_ANIMATION | {'source': motion_source}
    profile, profile_id = MOTION_SOURCES[motion_source]
    source, vanilla, output = (Path(p).resolve() for p in (source, vanilla, output))
    manifest_path = output.with_suffix('.manifest.json')
    require(output.suffix.lower() == '.o2r', 'Output must be an .o2r archive')
    for target in (output, manifest_path):
        for original in (source, vanilla):
            require(target != original and not (target.exists() and target.samefile(original)),
                    'Output/manifest must not replace an input archive')
    resource_path(selected)
    before = {str(p): sha256(p) for p in (source, vanilla)}
    with archive_reader(source) as read:
        source_name = 'alt/' + selected
        try:
            xml = read(source_name)
        except KeyError:
            source_name = selected
            xml = read(source_name)
    with archive_reader(vanilla) as read:
        texture = read(DONOR_TEXTURE)
    validate_donor(texture)
    native = 'custom/prelude/materials/water_temple_caustics_' + hashlib.sha256(selected.encode()).hexdigest()[:20]
    compiled = compile_material(xml, CAUSTIC_TEXTURE, strength, shift_s, shift_t)
    wrapper = ET.Element('DisplayList', Version='0')
    ET.SubElement(wrapper, 'CallDisplayList', Path=native)
    ET.SubElement(wrapper, 'EndDisplayList')
    metadata = {'edits': {'material-authoring': [{'data': {'materials': [
        {'newDlPath': native, 'nativeAnimation': animation}]}}]}}
    resources = {'alt/' + selected: ET.tostring(wrapper) + b'\n', native: compiled, CAUSTIC_TEXTURE: texture,
                 'prelude/project/edits.json': (json.dumps(metadata, indent=2) + '\n').encode()}
    after = {str(p): sha256(p) for p in (source, vanilla)}
    require(before == after, 'Input archive changed during build; output not written')
    manifest = dict(selected_material=selected, source_resource=source_name,
                    source_sha256={p.name: before[str(p)] for p in (source, vanilla)},
                    native_material=native, caustic_texture=CAUSTIC_TEXTURE,
                    native_profile=profile, native_profile_id=profile_id, nativeAnimation=animation,
                    strength=strength, env_rgb=[0, 0, 0], shift_s=shift_s, shift_t=shift_t,
                    tint='Existing artist vertex RGB preserved; lighting cleared, SHADE enabled; no vertex edits',
                    changed_resources=['alt/' + selected],
                    added_resources=sorted(set(resources) - {'alt/' + selected}),
                    resource_sha256={k: hashlib.sha256(v).hexdigest() for k, v in resources.items()},
                    input_archives_unchanged=True,
                    unsupported='Only documented opaque RGBA16 single-load XML grammar; no translucent alpha, '
                                'primitive tint, palette, existing multitexture, control flow, geometry or scene segments',
                    validation_limits='CPU/asset validation; not visually verified in-game. Native motion needs the '
                                      'feat/reusable-water-temple-caustics build. Older builds render static caustics.',
                    installation='Enable this O2R ABOVE Djipi scenes in the visible Mods menu; Apply & Close, restart. '
                                 'Enable alternate assets and PreludeNativeMaterialScroll. Binding is inside this O2R.')
    output.parent.mkdir(parents=True, exist_ok=True)
    # Build in the output directory and replace only after all validation succeeds.
    with tempfile.NamedTemporaryFile(dir=output.parent, suffix='.o2r', delete=False) as stream:
        temp = Path(stream.name)
    try:
        with zipfile.ZipFile(temp, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in sorted(resources.items()):
                info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                archive.writestr(info, data)
        temp.replace(output)
    finally:
        temp.unlink(missing_ok=True)
    manifest['overlay_sha256'] = sha256(output)
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-scenes', required=True, type=Path)
    parser.add_argument('--vanilla', required=True, type=Path)
    parser.add_argument('--material', default=DEFAULT_MATERIAL, help='Unprefixed XML resource path; alt/ preferred when present')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--strength', default=95, type=int, help='Caustic weighting, 0..255 (default 95)')
    parser.add_argument('--shift-s', default=3, type=int)
    parser.add_argument('--shift-t', default=0, type=int)
    parser.add_argument('--motion-source', choices=MOTION_SOURCES, default=NATIVE_ANIMATION['source'],
                        help='Native motion source, independent of the caustic artwork (default Water Temple)')
    args = parser.parse_args()
    try:
        report = build(args.source_scenes, args.vanilla, args.material, args.output,
                       args.strength, args.shift_s, args.shift_t, args.motion_source)
    except (ValueError, KeyError, OSError, zipfile.BadZipFile) as exc:
        parser.exit(2, f'Cannot build caustic overlay: {exc}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
