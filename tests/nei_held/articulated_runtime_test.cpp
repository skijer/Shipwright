// Exercise the production item dispatch. The resource/graphics queue is the
// boundary: no game, archive, camera, or actor mechanics are simulated here.
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "soh/Enhancements/randomizer/NeiArticulatedPresentation.cpp"
#include "mods/items/helpers/equip_helper.h"
#include "mods/items/custom_items.h"

namespace {
std::set<std::string> available;
std::vector<std::string> drawn;
Gfx arena[32];
size_t used = 0;
int nativeDraws = 0;
MtxF current{};
std::vector<MtxF> matrices;
std::vector<MtxF> poses;
void reset() { available.clear(); drawn.clear(); used = 0; nativeDraws = 0; matrices.clear(); poses.clear(); }
void switchBundle() {
    available.insert(NEI_HELD_PATH("switch_hook"));
    available.insert(NEI_HELD_PATH("switch_hook_body"));
    available.insert(NEI_HELD_PATH("switch_hook_tip"));
}
void whipBundle() {
    for (const char* path : {NEI_HELD_PATH("whip"), NEI_HELD_PATH("whip_handle"),
                             NEI_HELD_PATH("whip_tip"), NEI_HELD_PATH("whip_segment")}) available.insert(path);
}
Vec3f transform(const MtxF& m, Vec3f p) {
    return {m.xx*p.x+m.xy*p.y+m.xz*p.z+m.xw,
            m.yx*p.x+m.yy*p.y+m.yz*p.z+m.yw,
            m.zx*p.x+m.zy*p.y+m.zz*p.z+m.zw};
}
void near(Vec3f a, Vec3f b) {
    assert(std::fabs(a.x-b.x)<.001 && std::fabs(a.y-b.y)<.001 && std::fabs(a.z-b.z)<.001);
}
void writeFrame(std::ostream& out) {
    out << '[';
    for (size_t i=0; i<drawn.size(); ++i) {
        if(i) out << ',';
        out << "{\"path\":\"" << drawn[i] << "\",\"matrix\":[";
        const float* m = reinterpret_cast<const float*>(&poses[i]);
        for(int j=0;j<16;++j) { if(j) out << ','; out << m[j]; }
        out << "]}";
    }
    out << ']';
}
}

extern "C" { CustomItemState gCustomItemState{}; }

extern "C" bool NeiHeld_HasResources(const char* opaque, const char* translucent) {
    return opaque && available.contains(opaque) && (!translucent || available.contains(translucent));
}
extern "C" bool NeiHeld_DrawModel(PlayState*, const char* opaque, const char* translucent) {
    if (!NeiHeld_HasResources(opaque, translucent)) return false;
    drawn.emplace_back(opaque);
    poses.push_back(current);
    return true;
}
extern "C" void* Graph_Alloc(GraphicsContext*, size_t size) {
    assert(size % sizeof(Gfx) == 0 && used + size / sizeof(Gfx) <= 32);
    auto result = arena + used;
    used += size / sizeof(Gfx);
    return result;
}
extern "C" void Matrix_Get(MtxF* matrix) { *matrix = current; }
extern "C" void Matrix_Put(MtxF* matrix) { current = *matrix; }
extern "C" void Matrix_Push() { matrices.push_back(current); }
extern "C" void Matrix_Pop() { current = matrices.back(); matrices.pop_back(); }
extern "C" void Matrix_Scale(f32 x, f32 y, f32 z, u8 mode) {
    assert(mode == MTXMODE_APPLY);
    current.xx *= x; current.yx *= x; current.zx *= x;
    current.xy *= y; current.yy *= y; current.zy *= y;
    current.xz *= z; current.yz *= z; current.zz *= z;
}
extern "C" void Matrix_MultVec3f(Vec3f* a, Vec3f* b) {
    *b = {current.xx*a->x + current.xy*a->y + current.xz*a->z + current.xw,
          current.yx*a->x + current.yy*a->y + current.yz*a->z + current.yw,
          current.zx*a->x + current.zy*a->y + current.zz*a->z + current.zw};
}
extern "C" void Matrix_Translate(f32 x, f32 y, f32 z, u8 mode) {
    if (mode == MTXMODE_NEW) {
        current = {}; current.xx=current.yy=current.zz=current.ww=1;
    } else assert(mode == MTXMODE_APPLY);
    Vec3f a{x,y,z}, b;
    Matrix_MultVec3f(&a, &b);
    current.xw=b.x; current.yw=b.y; current.zw=b.z;
}
void rotate(int a, int b, f32 angle, u8 mode) {
    assert(mode == MTXMODE_APPLY);
    float* m = reinterpret_cast<float*>(&current);
    float c=std::cos(angle), s=std::sin(angle);
    for(int row=0;row<4;++row) {
        float x=m[a*4+row],y=m[b*4+row];
        m[a*4+row]=c*x+s*y; m[b*4+row]=-s*x+c*y;
    }
}
extern "C" void Matrix_RotateX(f32 angle, u8 mode) { rotate(1,2,angle,mode); }
extern "C" void Matrix_RotateY(f32 angle, u8 mode) { rotate(2,0,angle,mode); }
extern "C" void Matrix_RotateZ(f32 angle, u8 mode) { rotate(0,1,angle,mode); }
extern "C" f32 Math_FAtan2F(f32 y, f32 x) { return std::atan2(y,x); }
extern "C" void Gfx_SetupDL_25Opa(GraphicsContext*) {}
extern "C" void Graph_OpenDisps(Gfx**, GraphicsContext*, const char*, s32) {}
extern "C" void Graph_CloseDisps(Gfx**, GraphicsContext*, const char*, s32) {}
extern "C" Mtx* Matrix_NewMtx(GraphicsContext*, char*, s32) { static Mtx matrix; return &matrix; }
extern "C" void gSPDisplayList(Gfx*, Gfx*) { ++nativeDraws; }
extern "C" void FrameInterpolation_RecordOpenChild(const void*, int) {}
extern "C" void FrameInterpolation_RecordCloseChild() {}

int main(int argc, char** argv) {
    Player p{};
    PlayState play{};
    GraphicsContext graphics{};
    Gfx opa[1024];
    graphics.polyOpa.p = opa;
    play.state.gfxCtx = &graphics;
    Actor hook{};
    Gfx original[1]{}, hand[1]{};
    Gfx* limb = original;
    p.heldItemId = ITEM_SWITCH_HOOK;

    // Missing any component leaves both native limb and native actor in charge.
    reset();
    for (const char* missing : {NEI_HELD_PATH("switch_hook"), NEI_HELD_PATH("switch_hook_body"),
                                NEI_HELD_PATH("switch_hook_tip")}) {
        switchBundle();
        available.erase(missing);
        assert(!NeiArticulated_ApplySwitchHookHand(&play, &p, &limb, hand));
        assert(!NeiArticulated_DrawSwitchHookTip(&play, &p, &hook));
        assert(limb == original && used == 0 && drawn.empty());
    }

    // Shared hookshot IA must never opt another item into the switch presentation.
    reset();
    switchBundle();
    for (int item : {ITEM_HOOKSHOT, ITEM_LONGSHOT, ITEM_WHIP}) {
        p.heldItemId = item;
        assert(!NeiArticulated_ApplySwitchHookHand(&play, &p, &limb, hand));
        assert(!NeiArticulated_DrawSwitchHookTip(&play, &p, &hook));
    }
    assert(limb == original && used == 0 && drawn.empty());

    // Docked state includes the tip in the hand compound and suppresses the
    // actor copy; launch/retraction uses body-only plus exactly one actor tip.
    p.heldItemId = ITEM_SWITCH_HOOK;
    for (bool docked : {true, false}) {
        reset(); switchBundle();
        p.heldActor = docked ? &hook : nullptr;
        assert(NeiArticulated_ApplySwitchHookHand(&play, &p, &limb, hand));
        assert(((limb[0].words.w0 >> 24) & 255) == G_DL);
        assert(limb[0].words.w1 == reinterpret_cast<uintptr_t>(hand));
        assert(((limb[1].words.w0 >> 24) & 255) == G_DL_OTR_FILEPATH);
        assert(std::strcmp(reinterpret_cast<const char*>(limb[1].words.w1),
                           docked ? NEI_HELD_PATH("switch_hook") : NEI_HELD_PATH("switch_hook_body")) == 0);
        assert(NeiArticulated_DrawSwitchHookTip(&play, &p, &hook));
        assert(drawn == (docked ? std::vector<std::string>{} :
                                 std::vector<std::string>{NEI_HELD_PATH("switch_hook_tip")}));
    }

    // A partial whip set must not mix the old crystal/snake with the new grip.
    reset();
    for (const char* path : {NEI_HELD_PATH("whip"), NEI_HELD_PATH("whip_handle"),
                             NEI_HELD_PATH("whip_tip")}) available.insert(path);
    assert(!NeiArticulated_HasWhip());
    available.insert(NEI_HELD_PATH("whip_segment"));
    assert(NeiArticulated_HasWhip());

    ItemEquip_ReleaseHandMatrix();
    Vec3f socket{};
    assert(!NeiArticulated_DrawWhipGrip(&p, &play, true, &socket));
    assert(drawn.empty());
    p.actor.scale.x = p.actor.scale.y = p.actor.scale.z = .01f;
    // Wrist rotated 90 degrees about X: its item Y axis points along world Z.
    // Both equip and active state must use the same socket and same hand pose.
    current = {};
    current.xx = .01f; current.yz = -.01f; current.zy = .01f; current.ww = 1;
    current.xw = 10; current.yw = 20; current.zw = 30;
    ItemEquip_CaptureHandMatrix();
    MtxF before = current;
    for (bool coiled : {true, false}) {
        assert(NeiArticulated_DrawWhipGrip(&p, &play, coiled, &socket));
        assert(std::fabs(socket.x-10) < .0001 && std::fabs(socket.y-20) < .0001 &&
               std::fabs(socket.z-37.2) < .0001);
        assert(std::memcmp(&before, &current, sizeof(current)) == 0 && matrices.empty());
    }
    assert(drawn == std::vector<std::string>({NEI_HELD_PATH("whip"), NEI_HELD_PATH("whip_handle")}));
    assert(std::memcmp(&poses[0], &poses[1], sizeof(MtxF)) == 0);
    ItemEquip_ReleaseHandMatrix();
    assert(!NeiArticulated_DrawWhipGrip(&p, &play, false, &socket));
    // Pose application cannot read a previous clone/frame's released wrist.
    const ItemHandPose neutral = {0,0,0,0,0,0,1};
    assert(!ItemEquip_ApplyHandPose(&p, &neutral));

    // Run object_whip.c itself: body intervals meet the actual handle/tip
    // endpoints, equipped coil never leaks into a lash, and the gameplay state
    // remains unchanged across every draw.
    ItemEquip_CaptureHandMatrix();
    p.bodyPartsPos[PLAYER_BODYPART_R_HAND] = {10,20,30};
    for (int state : {1,2,3,4,5,6,7,0}) {
        reset(); whipBundle();
        gCustomItemState = {};
        gCustomItemState.whipActive = 1;
        gCustomItemState.whipState = state;
        gCustomItemState.whipTipPos = {10,20,130};
        gCustomItemState.whipAttachPos = {10,60,130};
        gCustomItemState.whipAttachNormal = {0,1,0};
        const CustomItemState saved = gCustomItemState;
        const Player savedPlayer = p;
        CustomItems_DrawWhip(&p, &play);
        assert(std::memcmp(&saved,&gCustomItemState,sizeof(saved))==0);
        assert(std::memcmp(&savedPlayer,&p,sizeof(p))==0);
        assert(nativeDraws==0);
        if (state==1) {
            assert(drawn==std::vector<std::string>{NEI_HELD_PATH("whip")});
        } else if (state>=2 && state<=6) {
            assert(drawn.front()==NEI_HELD_PATH("whip_handle"));
            assert(drawn.back()==NEI_HELD_PATH("whip_tip"));
            const bool attached=state==4||state==5;
            const Vec3f target=attached ? Vec3f{10,60,130} : Vec3f{10,20,130};
            Vec3f previous{10,20,37.2f};
            size_t count=drawn.size()-2;
            for(size_t i=1;i+1<drawn.size();++i) {
                assert(drawn[i]==NEI_HELD_PATH("whip_segment"));
                near(transform(poses[i],{0,0,-500}),previous);
                previous=transform(poses[i],{0,0,500});
                const float t=float(i)/count;
                near(previous,{10,20+(target.y-20)*t-(attached?60*t*(1-t):0),37.2f+(130-37.2f)*t});
            }
            near(previous,target);
            near(transform(poses.back(),{0,0,0}),target);
        } else assert(drawn.empty());
    }
    reset(); whipBundle();
    // FirstPerson_Init marks aiming before its four camera transition frames.
    // A full GI coil must never flash during that transition or settled aim;
    // the same handle already used by the lash remains attached to the wrist.
    for (int cameraTimer : {14, 13, 12, 11, 10, 1}) {
      reset();
      whipBundle();
      gCustomItemState = {};
      gCustomItemState.whipActive = 1;
      gCustomItemState.whipState = 1;
      gCustomItemState.whipFirstPersonActive = 1;
      p.unk_834 = cameraTimer;
      const CustomItemState saved = gCustomItemState;
      CustomItems_DrawWhip(&p, &play);
      assert(drawn == std::vector<std::string>{NEI_HELD_PATH("whip_handle")});
      assert(nativeDraws == 0);
      assert(std::memcmp(&saved, &gCustomItemState, sizeof(saved)) == 0);
    }
    gCustomItemState.whipFirstPersonActive = 0;
    reset();
    whipBundle();
    available.erase(NEI_HELD_PATH("whip_segment"));
    gCustomItemState.whipState=1;
    CustomItems_DrawWhip(&p,&play);
    assert(drawn.empty() && nativeDraws>0);
    // If first person has hidden the wrist, or the bundle is incomplete,
    // the old equipped snake must not replace the suppressed display coil.
    for (bool wrist : {false, true}) {
      reset();
      if (!wrist) {
        whipBundle();
        ItemEquip_ReleaseHandMatrix();
      } else {
        ItemEquip_CaptureHandMatrix();
      }
      gCustomItemState.whipFirstPersonActive = 1;
      CustomItems_DrawWhip(&p, &play);
      assert(drawn.empty() && nativeDraws == 0);
    }

    // Optional offline preview samples are emitted by the same production
    // object draw exercised above. They are render fixtures, not game captures.
    if(argc>1) {
        std::ofstream out(argv[1]); out << "{\"frames\":[";
        current={}; current.xx=.01f; current.yz=-.01f; current.zy=.01f; current.ww=1;
        ItemEquip_CaptureHandMatrix();
        p.bodyPartsPos[PLAYER_BODYPART_R_HAND]={0,0,0};
        for(int frame=0;frame<120;++frame) {
            reset(); whipBundle();
            gCustomItemState={}; gCustomItemState.whipActive=1;
            if(frame<20) gCustomItemState.whipState=1;
            else if(frame<60) {
                gCustomItemState.whipState=2;
                gCustomItemState.whipTipPos={0,0,7.2f+(frame-19)*2.5f};
            } else if(frame<100) {
                gCustomItemState.whipState=5;
                gCustomItemState.whipAttachPos={0,25,107.2f};
                gCustomItemState.whipAttachNormal={0,1,0};
                current.xw=15*std::sin((frame-60)*.1); current.yw=-8*std::sin((frame-60)*.1);
                ItemEquip_CaptureHandMatrix();
            } else {
                gCustomItemState.whipState=6;
                gCustomItemState.whipTipPos={0,0,7.2f+(119-frame)*5};
            }
            CustomItems_DrawWhip(&p,&play);
            if(frame)out << ',';
            writeFrame(out);
        }
        out << "]}\n";
    }
    std::cout << "PASS: articulated bundle fallback, item isolation, native hands, docked/flying tip ownership\n";
    std::cout << "PASS: real captured wrist pose, release lifetime, equip/active socket continuity and deferred grip paths\n";
    std::cout << "PASS: real whip equip/lash/latch/swing/retract routing, contiguous rope intervals and unchanged gameplay state\n";
    return 0;
}
