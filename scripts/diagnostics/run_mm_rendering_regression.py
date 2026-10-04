#!/usr/bin/env python3
"""Verify production MM rendering adapters without a custom game executable.

Pass --viewer-only for the actor lifecycle/render fixture and viewer syntax check,
or a path to mm.o2r to include the read-only archive checks.
"""
import ctypes as c
import importlib.util
import re
import struct
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
WORK = Path(tempfile.mkdtemp(prefix='mm-rendering-'))
def run(*args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)
def function(source, name):
    m = re.search(r'^[^\n;{}]*\b'+re.escape(name)+r'\([^;{}]*\)\s*\{', source, re.M)
    if not m: raise RuntimeError(name)
    i, depth = m.end(), 1
    while depth:
        depth += (source[i]=='{') - (source[i]=='}'); i += 1
    return source[m.start():i]+'\n'
flags=['-std=gnu11','-DF3DEX_GBI_2','-DLOG_LEVEL_GAME_PRINTS=6','-DNDEBUG','-ffunction-sections','-fdata-sections',
       '-Ilibultraship/include','-Isoh/include','-Isoh/src','-Isoh/assets','-Isoh','-Isoh/mods']
viewer=(ROOT/'soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c').read_text()
loader=(ROOT/'soh/mods/transformation_masks/assets/mm_asset_loader.cpp').read_text()
fixture=(ROOT/'soh/tests/static_story_mm_viewer_test.c').read_text()
names=['EnViewer_Update','EnViewer_Destroy','EnViewer_StaticSelectSkullKidModel','EnViewerStatic_WaitForObjects','EnViewerStatic_Update',
       'EnViewer_StaticGreatFairyEyeIndex','EnViewer_StaticGreatFairyOverrideLimbDraw','EnViewer_DrawStaticGreatFairy',
       'EnViewer_StaticTreasureChestShopGalOverrideLimbDraw','EnViewer_StaticOrdinaryMmOverrideLimbDraw',
       'EnViewer_StaticAnjuPostLimbDraw','EnViewer_DrawStaticMmActor','EnViewer_DrawStaticSkullKid','EnViewerStatic_Draw']
table=re.search(r'static Gfx sMmOpaqueRenderModeDL\[\] = \{.*?\n\};',loader,re.S).group(0)
fixture=fixture.replace('/* PRODUCTION_OPAQUE_RENDER_MODE */',table+'\n'+function(loader,'MmAssets_GetOpaqueRenderMode'))
draw_table=re.search(r'static EnViewerDrawFunc sDrawFuncs\[\] = \{.*?\n\};',viewer,re.S).group(0)
fixture=fixture.replace('/* PRODUCTION_VIEWER_FUNCTIONS */','\n'.join(function(viewer,n) for n in names)+
                        draw_table+'\n'+function(viewer,'EnViewer_Draw'))
p=WORK/'viewer.c';p.write_text(fixture)
objects=[]
for source in [p]+[ROOT/f'soh/src/overlays/actors/ovl_En_Viewer/static_story_{n}.c' for n in ['actor','mm_actor','ganon']]:
    out=WORK/(source.stem+'.o');run('cc',*flags,'-c',source,'-o',out);objects.append(out)
run('cc',*objects,'-Wl,--gc-sections','-lm','-o',WORK/'viewer');run(WORK/'viewer')
run('cc',*flags,'-fsyntax-only',ROOT/'soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c')
if '--viewer-only' in sys.argv:
    print('PASS production viewer lifecycle/render fixture and full viewer syntax')
    sys.exit(0)
patch=ROOT/'soh/mods/transformation_masks/assets/mm_display_list_patch.cpp'
run('c++','-std=c++20','-DNDEBUG',ROOT/'soh/tests/mm_display_list_patch_test.cpp',patch,'-o',WORK/'patch');run(WORK/'patch')
run('c++','-std=c++20','-shared','-fPIC',ROOT/'soh/tests/mm_display_list_patch_bridge.cpp',patch,'-o',WORK/'patch.so')
sys.path.insert(0,str(ROOT/'scripts'))
from bind_mm_fountain_animation import crc64
class Command(c.Structure): _fields_=[('w0',c.c_uint32),('w1',c.c_size_t)]
class Stats(c.Structure): _fields_=[(n,c.c_size_t) for n in ['nested','vertices','unresolved','malformed','renderMode','textures']]
Resolver=c.CFUNCTYPE(c.c_size_t,c.c_void_p,c.c_int,c.c_uint64,c.POINTER(c.c_size_t))
patcher=c.CDLL(str(WORK/'patch.so')).Test_PatchMmDisplayList
patcher.argtypes=[c.POINTER(Command),c.c_size_t,Resolver,c.c_void_p,c.POINTER(Stats)];patcher.restype=c.c_bool
storage={}; changed=total=lists=0
@Resolver
def resolve(ctx,kind,key,size):
    size[0]=65536
    if kind==1:
        buf=storage.get(key)
        if buf is None:return 0
        size[0]=len(buf);return c.addressof(buf)
    return 0x30000000+key if kind==2 else 0x40000000
with zipfile.ZipFile(sys.argv[1]) as archive:
    for name in archive.namelist():
        if not name.startswith('objects/object_stk/'):continue
        data=archive.read(name)
        if data[4:8]==b'RRAO' and struct.unpack_from('<I',data,64)[0]==25:
            count=struct.unpack_from('<I',data,68)[0]
            assert len(data)==72+count*16
            storage[crc64(name)]=(c.c_char*(count*16)).from_buffer_copy(data[72:])
    for name in archive.namelist():
        if not name.startswith('objects/object_stk/'):continue
        data=archive.read(name)
        if data[4:8]!=b'TLDO':continue
        words=list(struct.iter_unpack('<II',data[72:]));commands=(Command*len(words))(*(Command(*w) for w in words))
        expected=[];i=0
        while i<len(words):
            w0,w1=words[i];op=w0>>24
            if op==0xDF:break
            if op==0x32:
                key=(words[i+1][0]<<32)|words[i+1][1];buf=storage[key];count=(w0>>12)&255
                assert w1+count*16<=len(buf)
                expected.append((i,c.addressof(buf)+w1,c.string_at(c.addressof(buf)+w1,count*16),count))
                changed+=bool(w1)
            i+=2 if op in {0x20,0x24,0x25,0x27,0x31,0x32,0x33,0x35,0x36,0x42} else 1
        stats=Stats();assert patcher(commands,len(words),resolve,None,c.byref(stats)),name
        assert not stats.unresolved and not stats.malformed,name
        for i,pointer,vertices,count in expected:
            assert commands[i].w1==pointer,(name,i)
            assert c.string_at(commands[i].w1,count*16)==vertices,(name,i)
        total+=len(expected);lists+=1
print(f'PASS: {total} real vertex loads across {lists} Skull Kid lists; {changed} nonzero offsets preserved')
print('PASS: production viewer draw fixture, opaque table, patch bounds, and real viewer syntax')
