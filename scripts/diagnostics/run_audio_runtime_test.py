"""Compile selected, unmodified production function bodies with the real types.

This focused harness avoids unavailable libultraship/OS dependencies. It does
not compile whole translation units or render sound. No source is rewritten:
the generated include lives exclusively in the supplied temporary build dir.
"""
import pathlib
import re
import os
import shlex
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
FUNCTIONS = {
    'code_800EC960.c': ['Audio_PlayFanfare'],
    'audio_load.c': ['AudioLoad_IsFontLoadComplete', 'AudioLoad_SetFontLoadStatus',
                     'AudioLoad_IsSeqLoadComplete', 'AudioLoad_SetSeqLoadStatus',
                     'AudioLoad_SyncInitSeqPlayerInternal', 'AudioLoad_GetFontsForSequence',
                     'AudioLoad_SearchCaches', 'AudioLoad_SyncLoad', 'AudioLoad_SyncLoadFont',
                     'AudioLoad_InitFontMetadata', 'AudioLoad_SyncLoadSeq',
                     'AudioLoad_FindNextFreeSeqId', 'AudioLoad_RegisterMmSequence'],
    'audio_seqplayer.c': ['AudioSeq_SequencePlayerSetupChannels', 'AudioSeq_SelectChannelFont'],
    'audio_playback.c': ['Audio_GetInstrumentInner', 'Audio_NoteInitForLayer', 'Audio_NoteInit'],
    'audio_effects.c': ['Audio_AdsrInit'],
    'audio_heap.c': ['AudioHeap_IsSequenceInUse', 'AudioHeap_ReleaseNotesForFont', 'AudioHeap_SearchRegularCaches',
                     'AudioHeap_SearchPermanentCache', 'AudioHeap_SearchCaches',
                     'AudioHeap_Alloc', 'AudioHeap_AllocPermanent', 'AudioHeap_AllocCached',
                     'AudioHeap_TemporaryCacheClear', 'AudioHeap_DiscardSequence', 'AudioHeap_PopCache'],
}


def main(build, compiler='cc'):
    build = pathlib.Path(build).resolve()
    bodies = []
    for filename, names in FUNCTIONS.items():
        path = ROOT / 'soh/src/code' / filename
        source = path.read_text()
        for name in names:
            match = re.search(r'^[\w* ]+\b' + name + r'\([^;]*?\)\s*\{.*?^}', source, re.M | re.S)
            if match is None:
                raise RuntimeError(f'Cannot locate complete production function {name}')
            line = source.count('\n', 0, match.start()) + 1
            bodies.append(f'#line {line} "{path}"\n{match[0]}\n')
    (build / 'audio_runtime_functions.inc').write_text('\n'.join(bodies))
    exe = build / 'audio_stream_runtime_test'
    subprocess.run([compiler, '-std=c11', '-g', '-DNDEBUG', '-D_POSIX_C_SOURCE=200809L', '-Wall',
                    *shlex.split(os.environ.get('AUDIO_TEST_CFLAGS', '')),
                    '-Wno-unused-variable', '-Werror=implicit-function-declaration',
                    '-I' + str(build), '-I' + str(ROOT / 'soh/include'),
                    '-I' + str(ROOT / 'soh/tests/audio_sequence_stubs'),
                    str(ROOT / 'soh/tests/audio_stream_runtime_test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main(*sys.argv[1:])
