# Approved GI17 policy blob, shared by the published baseline and local mirror. Requires baseline git history.
import subprocess,tempfile,pathlib
root=pathlib.Path(__file__).resolve().parents[2]
old=subprocess.check_output(['git','cat-file','blob','abbaeb809c01db6684b021db46c8a5e0095dd3db'],cwd=root,text=True).replace('namespace NeiUsedMagic {','namespace Before {').replace('#include "NeiGiEffectPolicy.h"','#include "soh/Enhancements/randomizer/NeiGiEffectPolicy.h"')
source='''#include "soh/Enhancements/randomizer/NeiUsedMagicPolicy.h"
#include "old.h"
#include <cassert>
#include <cstdio>
void equal(const NeiGi::Mesh&a,const NeiGi::Mesh&b){assert(a.count==b.count);for(size_t i=0;i<a.count;++i){auto x=a.vertices[i],y=b.vertices[i];assert(x.p.x==y.p.x&&x.p.y==y.p.y&&x.p.z==y.p.z&&x.rgb==y.rgb&&x.alpha==y.alpha&&x.u==y.u&&x.v==y.v);}}
int main(){using NeiGi::Kind; for(unsigned f=0;f<180;++f){
equal(Before::SampleProjectile(Kind::Light,f,2,{1,.2f,.1f}),NeiUsedMagic::SampleProjectile(Kind::Light,f,2,{1,.2f,.1f}));
for(bool big:{false,true})for(float radius:{80.f,110.f,230.f,500.f})equal(Before::SampleSpin(Kind::Light,f,radius,big),NeiUsedMagic::SampleSpin(Kind::Light,f,radius,big));
for(auto k:{Kind::Ice,Kind::Light}){equal(Before::SampleCharge(k,f,1),NeiUsedMagic::SampleCharge(k,f,1));equal(Before::SampleChargeSparks(k,f,1),NeiUsedMagic::SampleChargeSparks(k,f,1));equal(Before::SampleChargeSurface(k,f,1),NeiUsedMagic::SampleChargeSurface(k,f,1));}
}puts("PASS Light projectile/release and Ice/Light charges bit-identical over 180 frames");}
'''
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d);(p/'old.h').write_text(old);(p/'test.cpp').write_text(source)
 subprocess.run(['c++','-std=c++20','-O2','-I'+str(root/'soh'),str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
