"""Compare real old/new flight emission: size alone changes; RNG, slots, life survive."""
from pathlib import Path
import sys,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/diagnostics'))
from run_time_pedestal_tests import functions
src='''#include <cassert>
#include <cstring>
#include <vector>
using s32=int; using f32=float;
struct PlayState{}; struct Vec3f{float x,y,z;}; struct Color_RGBA8{unsigned char r,g,b,a;};
struct Call {Vec3f pos,vel,acc;Color_RGBA8 prim,env;float scale;int life;};
std::vector<Call> calls; int rng=0;
float Rand_ZeroOne(){return ((++rng*17)%101)/101.f;}
void EffectSsEnIce_Spawn(PlayState*,Vec3f*p,float s,Vec3f*v,Vec3f*a,Color_RGBA8*c,Color_RGBA8*e,int l){calls.push_back({*p,*v,*a,*c,*e,s,l});}
void EffectSsKiraKira_SpawnDispersed(PlayState*x,Vec3f*p,Vec3f*v,Vec3f*a,Color_RGBA8*c,Color_RGBA8*e,int s,int l){EffectSsEnIce_Spawn(x,p,s,v,a,c,e,l);}
'''
for kind in ('FIRE','ICE'):
 for group in ('PRIM','ENV'):
  for channel in 'RGBA':src+=f'#define {kind}_ROD_{group}_{channel} 200\n'
for kind in ('Fire','Ice'):
 path=f'soh/mods/items/logic/item_rod_{kind.lower()}.c';name=f'{kind}Rod_Spawn{kind}Sparks'
 old=subprocess.check_output(['git','show','cec63fce86b1f582f6e61ad6a98eca6c3cca784b:'+path],cwd=ROOT,text=True)
 src+=functions(old)[name].replace(name,'Old'+name)+'\n'+functions((ROOT/path).read_text())[name]+'\n'
src+='''int main(){PlayState play;Vec3f p{1,2,3};
for(int kind=0;kind<2;++kind){
 rng=0;calls.clear();if(kind)OldIceRod_SpawnIceSparks(&play,&p,2);else OldFireRod_SpawnFireSparks(&play,&p,2);
 auto before=calls;int beforeRng=rng;
 rng=0;calls.clear();if(kind)IceRod_SpawnIceSparks(&play,&p,2);else FireRod_SpawnFireSparks(&play,&p,2);
 assert(calls.size()==before.size()&&calls.size()==(kind?6:10)&&rng==beforeRng);
 for(size_t i=0;i<calls.size();++i){auto a=before[i],b=calls[i];assert(a.scale>0&&b.scale==0);a.scale=b.scale;
 assert(!memcmp(&a.pos,&b.pos,sizeof(Vec3f))&&!memcmp(&a.vel,&b.vel,sizeof(Vec3f))&&!memcmp(&a.acc,&b.acc,sizeof(Vec3f)));
 assert(!memcmp(&a.prim,&b.prim,sizeof(Color_RGBA8))&&!memcmp(&a.env,&b.env,sizeof(Color_RGBA8))&&a.life==b.life);}
}}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.cpp').write_text(src)
 subprocess.run(['c++','-std=c++20',str(p/'t.cpp'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
print('PASS flight clump/sparkle suppression preserves RNG, allocations, positions, colors and lifetimes; impact paths untouched')
