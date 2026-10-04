#!/usr/bin/env python3
"""Compile focused tests and inspect real Prelude exports; never modify archives.

Usage: python3 scripts/diagnostics/run_native_material_probe.py --json-include DIR
       --spdlog-include DIR [path/to/vanilla.o2r path/to/retextured.o2r]
Requires initialized libultraship plus a C++20 compiler. No game/ROM is required;
this is CPU validation, not an in-game high-resolution rendering test.
The diagnostic conservatively rejects duplicate metadata paths, including
identical declarations that the runtime can accept.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def run(args):
    subprocess.run([str(x) for x in args], cwd=ROOT, check=True)


def fixture(path, bind_fountain=False):
    with zipfile.ZipFile(path) as archive:
        project = json.loads(archive.read('prelude/project/edits.json'))
        if bind_fountain:
            import importlib.util
            spec = importlib.util.spec_from_file_location('mm_fountain_binder', ROOT / 'scripts/bind_mm_fountain_animation.py')
            binder = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(binder)
            project, report = binder.analyze(archive)
            print('Verified fountain metadata bindings:', len(report['bindings']), flush=True)
        items = []
        declared_paths = set()
        for edits in project['edits'].values():
            if not isinstance(edits, list):
                continue
            for edit in edits:
                for kind in ('pastes', 'shapes', 'materials'):
                    for item in edit.get('data', {}).get(kind, []):
                        name = item['newDlPath']
                        # Check before filtering unbound/invalid materials or missing
                        # resources: another declaration can disable this path at runtime.
                        if name in declared_paths:
                            raise ValueError(f'Duplicate metadata declaration for path: {name}')
                        declared_paths.add(name)
                        if kind == 'materials' and 'nativeAnimation' not in item:
                            continue  # Match the runtime: materials never infer a shape profile.
                        if name not in archive.namelist():
                            continue
                        data = archive.read(name)
                        if len(data) < 72 or data[4:8] != b'TLDO':
                            continue
                        words = list(struct.iter_unpack('<II', data[72:]))
                        items.append(dict(path=name, metadata=item, pasted=kind == 'pastes',
                                          ucode=data[64], commands=words))
        return items


def check_caustics(items, results, required=False, source='oot.water_temple.caustics'):
    """An export probe exits successfully even when no binding/insertion resolves."""
    profile, profile_id = {'oot.water_temple.caustics': ('WaterTempleCaustics', 10),
                           'oot.zoras_domain.caustics': ('ZorasDomainCaustics', 11)}[source]
    expected = {item['path']: len(item['commands']) for item in items
                if isinstance(item['metadata'].get('nativeAnimation'), dict) and
                item['metadata']['nativeAnimation'].get('source') == source}
    if required and not expected:
        raise ValueError(f'No explicit {profile} materials found in archive')
    for path, length in expected.items():
        matches = [result for result in results if result['path'] == path]
        if (len(matches) != 1 or matches[0]['profile'] != profile_id or
                type(matches[0]['insertion']) is not int or not 0 <= matches[0]['insertion'] < length):
            raise ValueError(f'{profile} profile/insertion did not resolve: {path}: {matches}')
    if expected:
        print(f'ASSERTED {len(expected)} {profile} profile(s) with valid insertion', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json-include', required=True)
    parser.add_argument('--spdlog-include', required=True)
    parser.add_argument('--bind-mm-fountain', action='store_true', help='Analyze fountain metadata in memory; archive stays read-only')
    parser.add_argument('--require-water-temple-caustics', action='store_true',
                        help='Require a WaterTempleCaustics binding and valid insertion in each archive')
    parser.add_argument('--require-zoras-domain-caustics', action='store_true',
                        help='Require a ZorasDomainCaustics binding and valid insertion in each archive')
    parser.add_argument('--cxx', default='c++')
    parser.add_argument('archives', nargs='*', type=Path)
    args = parser.parse_args()
    core = ROOT / 'soh/soh/Enhancements/Graphics/NativeMaterialProfile.cpp'
    with tempfile.TemporaryDirectory(prefix='prelude-native-probe-') as temp:
        temp = Path(temp)
        common = [args.cxx, '-std=c++20', '-pthread', '-g', '-ffunction-sections', '-fdata-sections',
                  '-Wl,--gc-sections', '-I' + args.json_include]
        for test in ('native_material_scroll_test', 'native_material_export_probe', 'prelude_load_probe_test'):
            run(common + [ROOT / f'soh/tests/{test}.cpp', core, '-o', temp / test])
        run([temp / 'native_material_scroll_test'])
        run([temp / 'prelude_load_probe_test'])
        native = (ROOT / 'soh/src/code/z_rcp.c').read_text()
        match = re.search(r'Gfx\* Gfx_TwoTexScrollEx\([^}]+\n}', native)
        if not match:
            raise RuntimeError('Cannot isolate native Gfx_TwoTexScrollEx; inspect source change')
        (temp / 'native_two_tex_scroll.inc').write_text(match.group())
        run(common + ['-DF3DEX_GBI_2', '-D_LANGUAGE_C', '-DCVAR_PREFIX_ENHANCEMENT="gEnhancements"',
                      '-I' + str(ROOT / 'libultraship/include'), '-I' + str(ROOT / 'soh'),
                      '-I' + args.spdlog_include, '-I' + str(temp),
                      ROOT / 'soh/tests/native_material_runtime_test.cpp', core,
                      '-o', temp / 'native_material_runtime_test'])
        run([temp / 'native_material_runtime_test'])
        # Compile the actual decorator and native binary parser together. Keep
        # only XML loading out of this focused harness (and its dependencies).
        loader = (ROOT / 'libultraship/src/fast/resource/factory/DisplayListFactory.cpp').read_text()
        loader_start = loader.index('int8_t GetEndOpcodeByUCode(')
        loader_end = loader.index('std::shared_ptr<Ship::IResource>\nResourceFactoryXMLDisplayListV0::ReadResource',
                                  loader_start)
        (temp / 'native_binary_display_list_factory.inc').write_text(
            'namespace Fast {\n' + loader[loader_start:loader_end] + '\n}\n')
        factory_sources = [ROOT / 'libultraship/src' / source for source in (
            'fast/resource/type/DisplayList.cpp', 'ship/resource/Resource.cpp',
            'ship/resource/ResourceFactoryBinary.cpp', 'ship/utils/binarytools/BinaryReader.cpp',
            'ship/utils/binarytools/MemoryStream.cpp', 'ship/utils/binarytools/Stream.cpp')]
        run(common + ['-DF3DEX_GBI_2', '-D_LANGUAGE_C=', '-DCVAR_PREFIX_ENHANCEMENT="gEnhancements"',
                      '-I' + str(ROOT / 'libultraship/include'), '-I' + str(ROOT / 'soh'),
                      '-I' + args.spdlog_include, '-I' + str(temp),
                      ROOT / 'soh/tests/native_material_factory_test.cpp', core, *factory_sources,
                      '-o', temp / 'native_material_factory_test'])
        run([temp / 'native_material_factory_test'])
        for path in args.archives:
            data = temp / 'fixture.json'
            items = fixture(path.resolve(), args.bind_mm_fountain)
            data.write_text(json.dumps(items))
            print(f'EXPORT {path.name}', flush=True)
            result = subprocess.run([str(temp / 'native_material_export_probe'), str(data)],
                                    cwd=ROOT, check=True, capture_output=True, text=True)
            print(result.stdout, end='', flush=True)
            check_caustics(items, json.loads(result.stdout), args.require_water_temple_caustics)
            check_caustics(items, json.loads(result.stdout), args.require_zoras_domain_caustics,
                           source='oot.zoras_domain.caustics')


if __name__ == '__main__':
    main()
