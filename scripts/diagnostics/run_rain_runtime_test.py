#!/usr/bin/env python3
"""Compile real weather actors/resolver and unchanged extracted environment handlers."""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
cc, cxx = (sys.argv[2:4] if len(sys.argv)>3 else ['cc','c++'])
source_path = root / 'soh/src/code/z_kankyo.c'
source = source_path.read_text()
functions=[]
for name in ['func_8006FB94','func_800766C4','Environment_UpdateLightningStrike']:
    match=re.search(r'^[\w* ]+\b'+name+r'\([^;]*?\)\s*\{.*?^}',source,re.M|re.S)
    assert match,name
    line=source.count('\n',0,match.start())+1
    functions.append(f'#line {line} "{source_path}"\n{match[0]}\n')
(out/'rain_environment_functions.inc').write_text('\n'.join(functions))
common=['-g','-O1','-ffunction-sections','-fdata-sections','-DLOG_LEVEL_GAME_PRINTS=0',
        '-DCVAR_PREFIX_AUDIO="gAudioEditor"','-DCVAR_PREFIX_SETTING="gSettings"',
        '-Isoh/tests/rain_engine_stubs','-Isoh/include','-Isoh/src','-Isoh/assets','-Isoh',
        '-Ilibultraship/include','-I'+str(out)]
objects=[]
for source in ['soh/tests/rain_engine_test.c','soh/src/overlays/actors/ovl_En_Weather_Tag/z_en_weather_tag.c',
               'soh/src/code/concurrent_weather_audio.c','soh/tests/rain_engine_boundary.cpp',
               'soh/soh/Enhancements/audio/GlobalOutdoorRain.cpp']:
    is_cpp=source.endswith('.cpp')
    obj=out/(Path(source).name+'.o'); objects.append(str(obj))
    flags=['-std=c++20'] if is_cpp else ['-std=gnu11','-Wno-incompatible-pointer-types','-Wno-int-conversion']
    subprocess.run([cxx if is_cpp else cc,*flags,*common,'-c',source,'-o',str(obj)],cwd=root,check=True)
subprocess.run([cxx,*objects,'-Wl,--gc-sections','-lm','-o',str(out/'rain_engine_test')],cwd=root,check=True)
subprocess.run([str(out/'rain_engine_test')],check=True)
# Compile the actual scene-init call arguments against the production SaveContext.
# Only the Context/resource-manager service expression is replaced at the IO boundary.
init_source=(root/'soh/soh/z_play_otr.cpp').read_text()
call=re.search(r'    InitSceneRainPolicy\(play,.*?\);',init_source,re.S)
assert call, 'scene initialization must bind the archive weather policy'
service='Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager().get()'
assert service in call[0]
boundary='#include "z64.h"\n#include "soh/Enhancements/audio/SceneRainPolicy.h"\n'
boundary+='extern SaveContext gSaveContext;\nvoid CheckSceneRainInit(PlayState* play, Ship::ArchiveManager* archives) {\n'
boundary+=call[0].replace(service,'archives')+'\n}\n'
(out/'rain_scene_init_boundary.cpp').write_text(boundary)
subprocess.run([cxx,'-std=c++20',*common,'-fsyntax-only',str(out/'rain_scene_init_boundary.cpp')],cwd=root,check=True)
print('PASS scene rain initialization uses production scene layer and age fields')
