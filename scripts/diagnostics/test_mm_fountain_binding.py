#!/usr/bin/env python3
"""Finite family, portable transform, negative and optional real archive checks."""
import copy
import importlib.util
import json
import math
from pathlib import Path
import sys
import struct
import tempfile
import unittest
import zipfile

spec=importlib.util.spec_from_file_location('binder',Path(__file__).resolve().parents[1]/'bind_mm_fountain_animation.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
ARCHIVE=Path(sys.argv.pop()) if len(sys.argv)>1 and sys.argv[-1].endswith('.o2r') else None


def family(angle=0,scale=1,y=39,offset=(350.3,-100.2,200.4),scene='arbitrary'):
    co,si=math.cos(math.radians(angle)),math.sin(math.radians(angle))
    rows=[]
    for role,data in b.ROLES.items():
        points=copy.deepcopy(data['xyz'])
        if role=='stone6':
            for p in points:p[1]+=y-39
        v=[[round(offset[0]+scale*(co*x+si*z)),round(offset[1]+scale*yy),
            round(offset[2]+scale*(-si*x+co*z)),*uv] for (x,yy,z),uv in zip(points,data['uv'])]
        rows.append(dict(key=(scene,role),scene=scene,role=role,v=v))
    return rows


class Overlay:
    def __init__(self,archive,changes=None):
        self.archive=archive;self.changes=changes or {}
    def namelist(self):return self.archive.namelist()
    def read(self,name):return self.changes[name] if name in self.changes else self.archive.read(name)
    def getinfo(self,name):
        info=copy.copy(self.archive.getinfo(name))
        if name in self.changes:info.file_size=len(self.changes[name])
        return info


class Renamed(Overlay):
    def __init__(self,archive):
        super().__init__(archive)
        self.forward={n:('custom/prelude/renamed/'+str(i)) for i,n in enumerate(archive.namelist())}
        self.forward[b.EDITS]=b.EDITS
        self.forward['prelude/project/blobs.bin']='prelude/project/blobs.bin'
        self.reverse={v:k for k,v in self.forward.items()}
        def rewrite(node):
            if isinstance(node,str):return self.forward.get(node,node)
            if isinstance(node,list):return [rewrite(x) for x in node]
            if isinstance(node,dict):return {k:rewrite(v) for k,v in node.items()}
            return node
        project=rewrite(json.loads(archive.read(b.EDITS)))
        project['edits']={'arbitrary_scene_'+str(i):v for i,v in enumerate(project['edits'].values())}
        self.changes[b.EDITS]=json.dumps(project).encode()
        hashes={b.crc64(k):b.crc64(v) for k,v in self.forward.items()}
        # Rewrite every actual hash reference, not names inferred from suffixes.
        for original in archive.namelist():
            data=archive.read(original)
            if len(data)>=72 and data[4:8]==b'TLDO':
                data=bytearray(data);i=72
                while i+8<=len(data):
                    a,c=struct.unpack_from('<II',data,i);i+=8
                    if a>>24 in (0x20,0x32,0x33):
                        hi,lo=struct.unpack_from('<II',data,i);h=(hi<<32)|lo
                        if h in hashes:struct.pack_into('<II',data,i,hashes[h]>>32,hashes[h]&0xffffffff)
                        i+=8
                self.changes[self.forward[original]]=bytes(data)
            elif len(data)>=72 and data[4:8]==b'RRAO':
                # One common positive uniform scale/yaw/translation across all
                # authored components, including merged day vertex arrays.
                data=bytearray(data);co=math.cos(.64577);si=math.sin(.64577)
                for i in range(72,len(data),16):
                    x,y,z=struct.unpack_from('<hhh',data,i)
                    q=(round(.75*(co*x+si*z)+.3),round(.75*y+20.2),round(.75*(-si*x+co*z)-.4))
                    if all(-32768<=v<=32767 for v in q):struct.pack_into('<hhh',data,i,*q)
                self.changes[self.forward[original]]=bytes(data)
    def namelist(self):return list(self.reverse)
    def read(self,name):return self.changes[name] if name in self.changes else self.archive.read(self.reverse[name])
    def getinfo(self,name):
        info=copy.copy(self.archive.getinfo(self.reverse[name]))
        if name in self.changes:info.file_size=len(self.changes[name])
        return info


class BindingTest(unittest.TestCase):
    def test_portable_common_transforms(self):
        for angle in (0,37,90,180,-123):
            for scale in (.5,.75,1,1.5,2):
                for y in (-15,39,53):
                    self.assertEqual(len(b.associate(family(angle,scale,y))),1)
        self.assertEqual(len(b.associate(family()+family(37,.75,53,scene='renamed'))),2)

    def test_fail_closed_geometry(self):
        base=family()
        variants=[]
        variants.append(base[:-1])
        dup=copy.deepcopy(base);dup+=copy.deepcopy(base)
        for i,c in enumerate(dup):c['key']=(i,)
        variants.append(dup)
        for operation in ('nonuniform','pitch','reflect','detached','point'):
            rows=copy.deepcopy(base)
            for c in rows:
                for p in c['v']:
                    if operation=='nonuniform':p[0]=round(p[0]*1.2)
                    if operation=='reflect':p[0]=-p[0]
                    if operation=='pitch':p[1],p[2]=p[2],-p[1]
                    if operation=='detached' and c['role']=='raised':p[1]+=12
            if operation=='point':rows[-1]['v'][0][0]+=8
            variants.append(rows)
        for rows in variants:
            with self.assertRaises(ValueError):b.associate(rows)

    @unittest.skipUnless(ARCHIVE,'provide R5 archive')
    def test_real_renamed_transformed_and_negatives(self):
        with zipfile.ZipFile(ARCHIVE) as archive:
            _,report=b.analyze(Renamed(archive))
            self.assertEqual(len(report['bindings']),6)
            self.assertEqual(report['completeStoredGroups'],4)
            pixels='custom/prelude/shrine_n_scene/recipe70_tex0'
            texture=bytearray(archive.read(pixels));texture[-1]^=0xff
            _,retextured=b.analyze(Overlay(archive,{pixels:bytes(texture)}))
            self.assertEqual(len(retextured['bindings']),6)
            project=json.loads(archive.read(b.EDITS))
            rows=[it for es in project['edits'].values() for e in es
                  for k in ('shapes','pastes') for it in e.get('data',{}).get(k,[])]
            water=next(it for it in rows if it.get('newDlPath')=='custom/prelude/shrine_n_scene/recipe70')
            water['nativeAnimation']={'version':1,'source':'wrong'}
            with self.assertRaises(ValueError):b.analyze(Overlay(archive,{b.EDITS:json.dumps(project).encode()}))
            project=json.loads(archive.read(b.EDITS));project['blobs'][0]['n']=10**12
            with self.assertRaises(ValueError):b.analyze(Overlay(archive,{b.EDITS:json.dumps(project).encode()}))
            for resource in ('custom/prelude/shrine_n_scene/recipe71_vtx','custom/prelude/shrine_n_scene/recipe70'):
                with self.assertRaises(ValueError):b.analyze(Overlay(archive,{resource:archive.read(resource)[:-1]}))

    @unittest.skipUnless(ARCHIVE,'provide original R5 archive for payload checks')
    def test_real_archive_copy_and_idempotence(self):
        before=__import__('hashlib').sha256(ARCHIVE.read_bytes()).digest()
        with tempfile.TemporaryDirectory() as temp:
            out=Path(temp)/'bound.o2r'
            report=b.bind(ARCHIVE,out)
            self.assertEqual(len(report['bindings']),6)
            self.assertEqual(report['completeStoredGroups'],4)
            self.assertEqual(sorted(x['storedComponents'] for x in report['bindings']),[1,1,1,1,2,2])
            with zipfile.ZipFile(ARCHIVE) as original,zipfile.ZipFile(out) as result:
                old=json.loads(original.read(b.EDITS));new=json.loads(result.read(b.EDITS))
                again,report2=b.analyze(result)
                self.assertEqual(new,again);self.assertEqual(report,report2)
                changed=0
                for scene,edits in old['edits'].items():
                    for i,e in enumerate(edits):
                        for kind in ('shapes','pastes'):
                            for j,item in enumerate(e.get('data',{}).get(kind,[])):
                                other=new['edits'][scene][i]['data'][kind][j]
                                if item!=other:
                                    changed+=1;other=copy.deepcopy(other);other.pop('nativeAnimation')
                                    self.assertEqual(item,other)
                self.assertEqual(changed,6)
                self.assertEqual(original.namelist(),result.namelist())
                for name in original.namelist():
                    if name!=b.EDITS:self.assertEqual(original.read(name),result.read(name),name)
            with self.assertRaises(ValueError):b.bind(ARCHIVE,out)
            with self.assertRaises(ValueError):b.bind(ARCHIVE,ARCHIVE,True)
        self.assertEqual(before,__import__('hashlib').sha256(ARCHIVE.read_bytes()).digest())

if __name__=='__main__':unittest.main()
