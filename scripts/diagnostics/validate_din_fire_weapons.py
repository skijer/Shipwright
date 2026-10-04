"""Independent archive bounds, DL dependency, HD contract and preservation checks."""
import hashlib
import json
import struct
import sys
import zipfile


def validate(candidate, shield):
    with zipfile.ZipFile(candidate) as z, zipfile.ZipFile(shield) as base:
        assert z.testzip() is None
        for name in base.namelist():
            assert z.read(name) == base.read(name), name
        meta = json.loads(z.read('DinFireWeaponsPOC1.json'))
        for name, digest in meta['resources'].items():
            assert hashlib.sha256(z.read(name)).hexdigest() == digest
        root = 'objects/din_fire_sword/poc1/'
        for profile in ('adult', 'child', 'bgs', 'broken'):
            for layer in ('Core', 'Flame'):
                data = z.read(root+profile+'/'+layer+'Vertices')
                count = struct.unpack_from('<I', data, 68)[0]
                assert len(data) == 72+16*count and count % 3 == 0
                vertices = [struct.unpack_from('<hhhHhhBBBB', data, 72+i*16) for i in range(count)]
                assert all(v[0] > 0 and 0 <= v[4] <= 2048 and 0 <= v[5] <= 1024 for v in vertices)
                dl = z.read(root+profile+'/'+layer+'DL')
                offset, loaded, triangles = 72, 0, 0
                while offset < len(dl):
                    w0, w1 = struct.unpack_from('<II', dl, offset)
                    op = w0 >> 24
                    if op == 0x32:
                        loaded = (w0 >> 12) & 255
                        assert 0 < loaded <= 30 and w1 % 16 == 0 and w1//16+loaded <= count
                        offset += 16
                    elif op == 5:
                        assert all(((w0 >> shift)&255)//2 < loaded for shift in (16,8,0))
                        triangles += 1
                        offset += 8
                    else:
                        assert op == 0xdf and offset+8 == len(dl)
                        break
                assert triangles*3 == count
        for name in ('CoreTex','FlameTex'):
            data = z.read(root+name)
            assert struct.unpack_from('<I',data,8)[0] == 1
            kind,w,h,flags,sx,sy,size = struct.unpack_from('<IIIIffI',data,64)
            assert kind == 6 and (w,h) == tuple(meta['texture_contract']['physical_size'])
            assert flags == 3 and sx == sy == 1 and size == w*h*4 and len(data) == 92+size
            assert data[92::4] == data[93::4] == data[94::4] == data[95::4]
    print('PASS combined archive: all shield bytes preserved; four sword profiles, mesh indices and HD texture contracts')

if __name__ == '__main__':
    validate(*sys.argv[1:])
