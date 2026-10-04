"""Synthetic assets only. Run: python3 -m unittest discover -s soh/tests -p test_water_temple_caustics.py -v"""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / 'scripts/build_water_temple_caustics.py'


def material(base='textures/unrelated_stone', size=64, offset=0):
    root = ET.Element('DisplayList', Version='0')
    def add(tag, **attrs):
        ET.SubElement(root, tag, {k: str(v) for k, v in attrs.items()})
    add('PipeSync')
    colors = ['TEXEL0', '0', 'SHADE', '0', 'COMBINED', '0', 'PRIMITIVE', '0']
    combine = dict(zip(('A0', 'B0', 'C0', 'D0', 'A1', 'B1', 'C1', 'D1'),
                       ['G_CCMUX_' + v for v in colors]))
    combine.update(dict(zip(('Aa0', 'Ab0', 'Ac0', 'Ad0', 'Aa1', 'Ab1', 'Ac1', 'Ad1'),
                            ['G_ACMUX_' + v for v in ('0', '0', '0', '1', '0', '0', '0', 'COMBINED')])))
    add('SetCombineLERP', **combine)
    add('SetGeometryMode', G_ZBUFFER=1, G_SHADE=1, G_CULL_BACK=1, G_FOG=1, G_SHADING_SMOOTH=1, G_CLIPPING=1)
    add('SetOtherMode', Cmd='G_SETOTHERMODE_H', Sft=4, Length=20, G_AD_NOISE=1,
        G_CD_MAGICSQ=1, G_CK_NONE=1, G_TC_FILT=1, G_TF_BILERP=1, G_TL_TILE=1,
        G_TD_CLAMP=1, G_TP_PERSP=1, G_CYC_2CYCLE=1, G_PM_NPRIMITIVE=1)
    add('SetOtherMode', Cmd='G_SETOTHERMODE_L', Sft=0, Length=32,
        G_AC_NONE=1, G_ZS_PIXEL=1, G_RM_FOG_SHADE_A=1, G_RM_AA_ZB_OPA_SURF2=1)
    add('Texture', S=32768, T=49152, Level=0, Tile=0, On=1)
    add('SetTextureLUT', Mode='G_TT_NONE')
    add('TileSync')
    add('SetTextureImage', Path=base, Format='G_IM_FMT_RGBA', Size='G_IM_SIZ_16b_LOAD_BLOCK', Width=1)
    tile = dict(Format='G_IM_FMT_RGBA', Size='G_IM_SIZ_16b', Line=size // 4,
                TMem=0, Tile=0, Palette=0, Cms0='G_TX_WRAP', Cms1='G_TX_NOMIRROR',
                Cmt0='G_TX_WRAP', Cmt1='G_TX_NOMIRROR', MaskS=size.bit_length()-1,
                MaskT=size.bit_length()-1, ShiftS=1, ShiftT=2)
    add('SetTile', **(tile | dict(Line=0, Tile=7, Size='G_IM_SIZ_16b_LOAD_BLOCK')))
    add('LoadSync')
    add('LoadBlock', Tile=7, Uls=0, Ult=0, Lrs=min(size*size-1,4095), Dxt=128)
    add('PipeSync')
    add('SetTile', **tile)
    add('SetTileSize', T=0, Uls=offset, Ult=offset, Lrs=offset+(size-1)*4, Lrt=offset+(size-1)*4)
    add('SetPrimColor', M=0, L=0, R=255, G=255, B=255, A=255)
    add('EndDisplayList')
    return ET.tostring(root)


def donor():
    return b'\0'*4 + b'XETO' + b'\0'*56 + struct.pack('<4I',2,32,32,2048) + bytes(2048)


def synthetic_mpq(path, name, data):
    """Tiny uncompressed MPQ with authentic encrypted tables and slash resource keys."""
    import mpyq
    mpq = mpyq.MPQArchive.__new__(mpyq.MPQArchive)
    entries = [(name, data), ('(listfile)', name.encode() + b'\n')]
    hashes, blocks, payload = bytearray(), bytearray(), bytearray()
    for index, (key, value) in enumerate(entries):
        hashes.extend(struct.pack('<IIHHI',mpq._hash(key,'HASH_A'),mpq._hash(key,'HASH_B'),0,0,index))
        blocks.extend(struct.pack('<4I',96+len(payload),len(value),len(value),0x81000000))
        payload.extend(value)
    def encrypt(data, key):
        seed=0xEEEEEEEE; result=bytearray()
        for (value,) in struct.iter_unpack('<I',data):
            seed=(seed+mpq.encryption_table[0x400+(key&255)])&0xffffffff
            result.extend(struct.pack('<I',(value^(key+seed))&0xffffffff))
            key=(((~key<<21)+0x11111111)|(key>>11))&0xffffffff
            seed=(value+seed+(seed<<5)+3)&0xffffffff
        return result
    path.write_bytes(struct.pack('<4s2I2H4I',b'MPQ\x1a',32,96+len(payload),0,3,32,64,2,2)+
                     encrypt(hashes,mpq._hash('(hash table)','TABLE'))+
                     encrypt(blocks,mpq._hash('(block table)','TABLE'))+payload)


class CausticAuthoringTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(TOOL.exists(), 'Reusable material compiler is not implemented')
        spec = importlib.util.spec_from_file_location('caustics', TOOL)
        self.api = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.api)

    def test_compiled_material_matches_actual_gbi_and_strhash64(self):
        # A different opcode/bitfield/hash seed/order in the compiler breaks this.
        compiled = self.api.compile_material(material(), 'custom/prelude/textures/test', 95, 3, 0)
        self.assertEqual(compiled[:12], b'\0\1\0\0TLDO\0\0\0\0')
        self.assertEqual(compiled[64:72], b'\4'+b'\0'*7)
        source = r'''
#include <cstdio>
#include <cstdint>
#include "libultraship/libultra/gbi.h"
#include "ship/utils/StrHash64.h"
void out(Gfx c) { printf("%08x %08x\n", unsigned(c.words.w0), unsigned(c.words.w1)); }
void image(const char* path) {
    Gfx c = gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 0);
    c.words.w0 = (c.words.w0 & 0xffffff) | (G_SETTIMG_OTR_HASH << 24); out(c);
    auto h = CRC64(path); printf("%08x %08x\n", unsigned(h >> 32), unsigned(h));
    out(Gfx gsDPPipeSync());
}
int main() {
    out(Gfx gsDPPipeSync());
    out(Gfx gsDPSetCombineLERP(TEXEL1,TEXEL0,ENV_ALPHA,TEXEL0,0,0,0,1,COMBINED,0,SHADE,0,0,0,0,COMBINED));
    out(Gfx gsSPSetGeometryMode(G_ZBUFFER|G_SHADE|G_CULL_BACK|G_FOG|G_SHADING_SMOOTH|G_CLIPPING));
    out(Gfx gsSPSetOtherMode(G_SETOTHERMODE_H,4,20,G_AD_NOISE|G_CD_MAGICSQ|G_CK_NONE|G_TC_FILT|G_TF_BILERP|G_TL_TILE|G_TD_CLAMP|G_TP_PERSP|G_CYC_2CYCLE|G_PM_NPRIMITIVE));
    out(Gfx gsSPSetOtherMode(G_SETOTHERMODE_L,0,32,G_AC_NONE|G_ZS_PIXEL|G_RM_FOG_SHADE_A|G_RM_AA_ZB_OPA_SURF2));
    out(Gfx gsSPTexture(32768,49152,0,0,1));
    out(Gfx gsDPSetTextureLUT(G_TT_NONE));
    out(Gfx gsDPTileSync());
    image("textures/unrelated_stone");
    out(Gfx gsDPSetTile(G_IM_FMT_RGBA,G_IM_SIZ_16b,0,0,7,0,G_TX_WRAP,6,2,G_TX_WRAP,6,1));
    out(Gfx gsDPLoadSync());
    out(Gfx gsDPLoadBlock(7,0,0,4095,128));
    out(Gfx gsDPPipeSync());
    out(Gfx gsDPSetTile(G_IM_FMT_RGBA,G_IM_SIZ_16b,16,0,0,0,G_TX_WRAP,6,2,G_TX_WRAP,6,1));
    out(Gfx gsDPSetTileSize(0,0,0,252,252));
    out(Gfx gsDPSetPrimColor(0,0,255,255,255,255));
    out(Gfx gsDPTileSync());
    image("custom/prelude/textures/test");
    out(Gfx gsDPSetTile(G_IM_FMT_RGBA,G_IM_SIZ_16b,0,256,7,0,G_TX_WRAP,5,0,G_TX_WRAP,5,3));
    out(Gfx gsDPLoadSync());
    out(Gfx gsDPLoadBlock(7,0,0,1023,256));
    out(Gfx gsDPPipeSync());
    out(Gfx gsDPSetTile(G_IM_FMT_RGBA,G_IM_SIZ_16b,8,256,1,0,G_TX_WRAP,5,0,G_TX_WRAP,5,3));
    out(Gfx gsDPSetTileSize(1,0,0,124,124));
    out(Gfx gsSPClearGeometryMode(G_LIGHTING));
    out(Gfx gsSPSetGeometryMode(G_SHADE));
    out(Gfx gsDPSetEnvColor(0,0,0,95));
    out(Gfx gsSPEndDisplayList());
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp, exe = Path(temp)/'encoding.cpp', Path(temp)/'encoding'
            cpp.write_text(source)
            subprocess.run(['c++','-std=c++20','-DF3DEX_GBI_2','-D_LANGUAGE_C',
                            '-I'+str(ROOT/'libultraship/include'),str(cpp),
                            str(ROOT/'libultraship/src/ship/utils/StrHash64.cpp'),'-o',str(exe)],check=True)
            expected = [tuple(int(w,16) for w in line.split()) for line in
                        subprocess.check_output([str(exe)],text=True).splitlines()]
        self.assertEqual(list(struct.iter_unpack('<II',compiled[72:])), expected)

    def test_tile_zero_dimensions_offsets_and_artist_state_survive(self):
        for size in (32,64,256):
            with self.subTest(size=size):
                data = self.api.compile_material(material(size=size,offset=12),'custom/prelude/tex',31,7,9)
                commands = list(struct.iter_unpack('<II',data[72:]))
                self.assertIn((0xF200C00C,((12+(size-1)*4)<<12)|(12+(size-1)*4)),commands)
                self.assertIn((0xD7000002,0x8000C000),commands)
                self.assertIn((0xFB000000,31),commands)
                self.assertIn((0xF5101100,0x01016457),commands)
                self.assertIn((0xD9FDFFFF,0),commands) # use authored vertex colors even if caller enabled lighting
                self.assertIn((0xD9FFFFFF,0x00A10405),commands)

    def test_unsupported_layouts_and_unknown_attributes_fail_closed(self):
        changes = [
            lambda r: ET.SubElement(r,'CallDisplayList',Path='unrelated'),
            lambda r: ET.SubElement(r,'Vertex',Path='mesh'),
            lambda r: ET.SubElement(r,'SetTextureImage',r.find('SetTextureImage').attrib),
            lambda r: r.find('SetTile').set('TMem','256'),
            lambda r: r.find('SetTileSize').set('T','1'),
            lambda r: r.find('Texture').set('Level','1'),
            lambda r: r.find('Texture').set('Surprise','1'),
            lambda r: r.find('SetPrimColor').set('R','100'),
            lambda r: r.find('SetCombineLERP').set('Ad0','G_ACMUX_TEXEL0'),
            lambda r: r.find('SetGeometryMode').set('G_TEXTURE_GEN','1'),
            lambda r: r.find('SetTextureImage').set('Path','>0x08000000'),
            lambda r: r.find('SetTextureImage').set('Width','4097'),
            lambda r: r.find('SetOtherMode').set('G_CYC_2CYCLE','0'),
            lambda r: r.find('SetTile').set('MaskS','16'),
            lambda r: r.remove(r.find('LoadBlock')),
            lambda r: r.find('SetTextureLUT').set('Mode','G_TT_RGBA16'),
            lambda r: r.find('SetOtherMode').set('G_CYC_COPY','1'),
            lambda r: r.set('Version','1'),
        ]
        for change in changes:
            root = ET.fromstring(material()); change(root)
            with self.subTest(xml=ET.tostring(root)), self.assertRaises(ValueError):
                self.api.compile_material(ET.tostring(root),'custom/prelude/tex')
        for strength,s,t in ((-1,3,0),(256,3,0),(95,16,0),(95,3,-1)):
            with self.assertRaises(ValueError):
                self.api.compile_material(material(),'custom/prelude/tex',strength,s,t)

    def test_cli_exact_overlay_allowlist_reproducible_and_inputs_unchanged(self):
        with tempfile.TemporaryDirectory() as temp:
            temp=Path(temp); src=temp/'source.o2r'; vanilla=temp/'vanilla.o2r'; out=temp/'overlay.o2r'
            selected='scenes/custom/another_room/material_stone'
            with zipfile.ZipFile(src,'w') as z:
                z.writestr('alt/'+selected,material()); z.writestr('vertices/do_not_copy',b'geometry')
            with zipfile.ZipFile(vanilla,'w') as z:
                z.writestr(self.api.DONOR_TEXTURE,donor())
            before=[hashlib.sha256(p.read_bytes()).hexdigest() for p in (src,vanilla)]
            args=['python3',str(TOOL),'--source-scenes',str(src),'--vanilla',str(vanilla),
                  '--material',selected,'--output',str(out)]
            subprocess.run(args,check=True,capture_output=True,text=True)
            first=out.read_bytes()
            subprocess.run(args,check=True,capture_output=True,text=True)
            self.assertEqual(first,out.read_bytes())
            manifest=json.loads(out.with_suffix('.manifest.json').read_text())
            with zipfile.ZipFile(out) as z:
                native=manifest['native_material']; texture=manifest['caustic_texture']
                self.assertEqual(set(z.namelist()),{'alt/'+selected,native,texture,'prelude/project/edits.json'})
                self.assertEqual(z.read(texture),donor())
                wrapper=ET.fromstring(z.read('alt/'+selected))
                self.assertEqual([n.tag for n in wrapper],['CallDisplayList','EndDisplayList'])
                self.assertEqual(wrapper[0].get('Path'),native)
                project=json.loads(z.read('prelude/project/edits.json'))
                items=[i for edits in project['edits'].values() for edit in edits for i in edit['data']['materials']]
                self.assertEqual(items,[{'newDlPath':native,'nativeAnimation':self.api.NATIVE_ANIMATION}])
            self.assertEqual(before,[hashlib.sha256(p.read_bytes()).hexdigest() for p in (src,vanilla)])
            # Diagnostic must discover the actual material-only payload.
            spec=importlib.util.spec_from_file_location('probe',ROOT/'scripts/diagnostics/run_native_material_probe.py')
            probe=importlib.util.module_from_spec(spec); spec.loader.exec_module(probe)
            fixtures=probe.fixture(out)
            self.assertEqual(len(fixtures),1)
            self.assertEqual(fixtures[0]['path'],native)
            self.assertEqual(fixtures[0]['ucode'],4)
            self.assertFalse(fixtures[0]['pasted'])
            # Even an explicit output path cannot clobber either source archive.
            bad=args[:-1]+[str(src)]
            self.assertNotEqual(subprocess.run(bad,capture_output=True).returncode,0)
            self.assertEqual(before,[hashlib.sha256(p.read_bytes()).hexdigest() for p in (src,vanilla)])

    def test_motion_source_changes_only_metadata(self):
        with tempfile.TemporaryDirectory() as temp:
            temp=Path(temp); src=temp/'source.o2r'; vanilla=temp/'vanilla.o2r'
            horizontal=temp/'horizontal.o2r'; vertical=temp/'vertical.o2r'
            with zipfile.ZipFile(src,'w') as z:
                z.writestr('alt/'+self.api.DEFAULT_MATERIAL,material())
            with zipfile.ZipFile(vanilla,'w') as z:
                z.writestr(self.api.DONOR_TEXTURE,donor())
            args=['python3',str(TOOL),'--source-scenes',str(src),'--vanilla',str(vanilla),'--output']
            subprocess.run(args+[str(horizontal)],check=True,capture_output=True,text=True)
            source='oot.zoras_domain.caustics'
            result=subprocess.run(args+[str(vertical),'--motion-source',source],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            manifest=json.loads(result.stdout)
            self.assertEqual(manifest['native_profile_id'],11)
            self.assertEqual(manifest['native_profile'],'ZorasDomainCaustics')
            with zipfile.ZipFile(horizontal) as a, zipfile.ZipFile(vertical) as b:
                self.assertEqual(a.namelist(),b.namelist())
                self.assertEqual([n for n in a.namelist() if a.read(n)!=b.read(n)],['prelude/project/edits.json'])
                metadata=json.loads(b.read('prelude/project/edits.json'))
                animation=metadata['edits']['material-authoring'][0]['data']['materials'][0]['nativeAnimation']
                self.assertEqual(animation,self.api.NATIVE_ANIMATION | {'source':source})
            bad=temp/'unsupported.o2r'
            result=subprocess.run(args+[str(bad),'--motion-source','unknown'],capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0)
            self.assertFalse(bad.exists())
            with self.assertRaises(ValueError):
                self.api.build(src,vanilla,self.api.DEFAULT_MATERIAL,bad,motion_source='unknown')
            self.assertFalse(bad.exists())

    def test_domain_probe_requires_the_correct_profile(self):
        spec=importlib.util.spec_from_file_location('probe',ROOT/'scripts/diagnostics/run_native_material_probe.py')
        probe=importlib.util.module_from_spec(spec); spec.loader.exec_module(probe)
        source='oot.zoras_domain.caustics'
        fixture=[dict(path='custom/prelude/test',metadata={'nativeAnimation':self.api.NATIVE_ANIMATION | {'source':source}},
                      commands=[(0xE7000000,0),(0xDF000000,0)])]
        good=[dict(path='custom/prelude/test',profile=11,insertion=1)]
        probe.check_caustics(fixture,good,True,source=source)
        for result in ([],[good[0] | {'profile':10}],[good[0] | {'insertion':None}],
                       [good[0] | {'insertion':2}],good+good):
            with self.assertRaises(ValueError): probe.check_caustics(fixture,result,True,source=source)
        with self.assertRaises(ValueError): probe.check_caustics([],[],True,source=source)

    def test_donor_validation(self):
        self.api.validate_donor(donor())
        for data in (donor()[:-1],donor()[:64]+struct.pack('<4I',1,32,32,2048)+bytes(2048),
                     donor()[:68]+struct.pack('<I',64)+donor()[72:]):
            with self.assertRaises(ValueError): self.api.validate_donor(data)

    def test_mpq_preserves_forward_slash_resource_lookup(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'synthetic.otr'
            name='alt/scenes/unrelated/material'
            synthetic_mpq(path,name,material())
            with self.api.archive_reader(path) as read:
                self.assertEqual(read(name),material())

    def test_probe_checks_profile_and_insertion_instead_of_exit_status(self):
        spec=importlib.util.spec_from_file_location('probe',ROOT/'scripts/diagnostics/run_native_material_probe.py')
        probe=importlib.util.module_from_spec(spec); spec.loader.exec_module(probe)
        self.assertTrue(hasattr(probe,'check_caustics'), 'Probe needs semantic output validation')
        fixture=[dict(path='custom/prelude/test',metadata={'nativeAnimation':self.api.NATIVE_ANIMATION},
                      commands=[(0xE7000000,0),(0xDF000000,0)])]
        good=[dict(path='custom/prelude/test',profile=10,insertion=1)]
        probe.check_caustics(fixture,good,True)
        for result in ([],[good[0] | {'profile':9}],[good[0] | {'insertion':None}],
                       [good[0] | {'insertion':2}],good+good):
            with self.assertRaises(ValueError): probe.check_caustics(fixture,result,True)
        with self.assertRaises(ValueError): probe.check_caustics([],[],True)
        for metadata in (None, 'invalid', 10):
            probe.check_caustics([fixture[0] | {'metadata': {'nativeAnimation':metadata}}], [], False)

    def test_probe_materials_do_not_use_implicit_shape_recognition(self):
        spec=importlib.util.spec_from_file_location('probe',ROOT/'scripts/diagnostics/run_native_material_probe.py')
        probe=importlib.util.module_from_spec(spec); spec.loader.exec_module(probe)
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'implicit.o2r'
            native='custom/prelude/materials/unbound'
            with zipfile.ZipFile(path,'w') as z:
                z.writestr(native,self.api.compile_material(material(),'custom/prelude/tex'))
                z.writestr('prelude/project/edits.json',json.dumps({'edits':{'any':[
                    {'data':{'materials':[{'newDlPath':native}]}}]}}))
            self.assertEqual(probe.fixture(path),[])

    def assert_probe_rejects_duplicate_declarations(self, second_metadata, kinds=('materials',)):
        spec=importlib.util.spec_from_file_location('probe',ROOT/'scripts/diagnostics/run_native_material_probe.py')
        probe=importlib.util.module_from_spec(spec); spec.loader.exec_module(probe)
        native='custom/prelude/materials/collision'
        valid={'newDlPath':native,'nativeAnimation':self.api.NATIVE_ANIMATION}
        second={'newDlPath':native} | second_metadata
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'duplicate.o2r'
            for kind in kinds:
                for reverse in (False,True):
                    declarations=[{'data':{'materials':[valid]}},{'data':{kind:[second]}}]
                    if reverse:
                        declarations.reverse()
                    # Different scene/edit buckets must not hide an archive-wide collision.
                    project={'edits':{'first':[declarations[0]],'second':[declarations[1]]}}
                    with zipfile.ZipFile(path,'w') as z:
                        z.writestr(native,self.api.compile_material(material(),'custom/prelude/tex'))
                        z.writestr('prelude/project/edits.json',json.dumps(project))
                    with self.subTest(kind=kind,reverse=reverse), self.assertRaisesRegex(
                            ValueError,'Duplicate metadata declaration for path'):
                        probe.fixture(path)

    def test_probe_rejects_valid_plus_invalid_binding_for_same_path(self):
        for invalid in (None, self.api.NATIVE_ANIMATION | {'version':2}):
            self.assert_probe_rejects_duplicate_declarations(
                {'nativeAnimation':invalid},kinds=('materials','pastes','shapes'))

    def test_probe_rejects_valid_plus_unbound_material_for_same_path(self):
        self.assert_probe_rejects_duplicate_declarations({})

    def test_probe_conservatively_rejects_identical_declarations_for_same_path(self):
        self.assert_probe_rejects_duplicate_declarations({'nativeAnimation':self.api.NATIVE_ANIMATION})


if __name__=='__main__': unittest.main()
