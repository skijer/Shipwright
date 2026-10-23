#pragma once
#include "archive.h"
#include <map>
#include <memory>
#include <set>

namespace pak {
struct Model {
    std::string key, label, path, prefix;
    bool child=false, deity=false, native=false, ready=false;
    Bytes bytes;
    std::array<LodLimb,21> limbs{};
    std::array<void*,21> table{};
    std::map<uint32_t,std::vector<Gfx>> lists;
    std::map<uint32_t,std::vector<Vtx>> vertices;
    std::map<uint32_t,Mtx> matrices;
    std::map<uint32_t,Gfx*> aliases;
    std::set<uint32_t> visiting;
    size_t commands=0;
    float rootRatio=1.f;
    void Range(size_t off,size_t n) const { Require(off<=bytes.size() && n<=bytes.size()-off,"ZOBJ reference outside file"); }
    uint32_t Pointer(size_t at) const {
        Range(at,4); uint32_t v=Be32(bytes.data()+at);
        Require((v>>24)==6,"Expected ZOBJ segment 06"); return v&0xffffff;
    }
    Gfx* Translate(uint32_t off,unsigned depth=0) {
        Require(depth<64 && !visiting.count(off),"Recursive ZOBJ display list");
        auto existing=lists.find(off); if(existing!=lists.end()) return existing->second.data();
        visiting.insert(off);
        size_t end=off;
        for(;;end+=8) {
            Range(end,8); Require(++commands<131072,"Too many ZOBJ commands");
            uint32_t w=Be32(bytes.data()+end), op=w>>24;
            if(op==0xdf || (op==0xde && ((w>>16)&1))) break;
        }
        auto& dl=lists[off]; dl.resize((end-off)/8+1);
        uint32_t texture=0,textureSize=0;bool localTexture=false;
        for(size_t i=0;i<dl.size();++i) {
            uint32_t w0=Be32(bytes.data()+off+i*8), w1=Be32(bytes.data()+off+i*8+4);
            uint32_t op=w0>>24, seg=w1>>24, addr=w1&0xffffff;
            dl[i].words.w0=w0; dl[i].words.w1=w1;
            if(op==0x01) {
                unsigned count=(w0>>12)&255;
                Require(seg==6 && count>0 && count<=32,"Unsupported vertex reference"); Range(addr,count*16);
                auto& v=vertices[addr];
                if(v.size()<count) {
                    // Allocate for the largest possible load now: pointers already embedded in DLs stay valid.
                    if(v.empty()) v.resize(std::min<size_t>(32,(bytes.size()-addr)/16));
                    Require(v.size()>=count,"Vertex range overflow");
                }
                for(unsigned n=0;n<count;++n) {
                    const auto* p=bytes.data()+addr+n*16;
                    for(unsigned a=0;a<3;++a) v[n].v.ob[a]=Be16(p+a*2);
                    v[n].v.flag=uint16_t(Be16(p+6));
                    v[n].v.tc[0]=Be16(p+8);v[n].v.tc[1]=Be16(p+10);memcpy(v[n].v.cn,p+12,4);
                }
                dl[i].words.w1=reinterpret_cast<uintptr_t>(v.data());
            } else if(op==0xde) {
                Require(seg==6,"External display lists are not supported in imported ZOBJs");
                dl[i].words.w1=reinterpret_cast<uintptr_t>(Translate(addr,depth+1));
            } else if(op==0xfd || op==0xdc || op==0xda) {
                if(op==0xfd) {texture=addr;textureSize=(w0>>19)&3;localTexture=seg==6;}
                if(seg==6) {
                    Range(addr,op==0xda?64:op==0xdc?24:1);
                    if(op==0xda) {
                        auto& m=matrices[addr];
                        for(unsigned n=0;n<16;++n) m.mf[n/4][n%4]=float(Be16(bytes.data()+addr+n*2))+
                            float(uint16_t(Be16(bytes.data()+addr+32+n*2)))/65536.f;
                        dl[i].words.w1=reinterpret_cast<uintptr_t>(&m);
                    } else dl[i].words.w1=reinterpret_cast<uintptr_t>(bytes.data()+addr);
                } else {
                    Require((op==0xfd && (seg==8 || seg==9)) || (op==0xda && seg==13 && addr<21*64),
                            "Unsupported external ZOBJ segment");
                    dl[i].words.w1=w1|1u;
                }
            } else {
                if(localTexture && op==0xf3) Range(texture,(((w1>>12)&4095)+1)*(1u<<textureSize)/2);
                if(localTexture && op==0xf0) Range(texture,(((w1>>14)&1023)+1)*2);
                switch(op) {
                    case 0xdb: {
                        unsigned index=(w0>>16)&255;
                        Require(index==2 || index==4 || index==8 || index==10 || index==12,
                                "ZOBJ attempts to replace renderer segment/matrix state");break;
                    }
                    case 0x00:case 0x03:case 0x05:case 0x06:case 0x07:case 0xd7:case 0xd8:case 0xd9:
                    case 0xdf:case 0xe2:case 0xe3:case 0xe6:case 0xe7:case 0xe8:case 0xe9:
                    case 0xf0:case 0xf2:case 0xf3:case 0xf4:case 0xf5:case 0xf8:case 0xf9:
                    case 0xfa:case 0xfb:case 0xfc: break;
                    default: throw std::runtime_error("Unsupported ZOBJ graphics command");
                }
            }
        }
        visiting.erase(off); return dl.data();
    }
    void Validate() {
        // Animation indices and matrix ordering must match Link's complete 21-limb rig.
        const char* base=child?"objects/object_link_child/gLinkChildSkel":"objects/object_link_boy/gLinkAdultSkel";
        auto* header=ResourceMgr_LoadSkeletonByName(base,nullptr);
        Require(header && header->limbCount==21 && header->segment,"Base Link skeleton unavailable");
        for(unsigned i=0;i<21;++i) {
            auto* vanilla=reinterpret_cast<StandardLimb*>(header->segment[i]);
            Require(vanilla && limbs[i].child==vanilla->child && limbs[i].sibling==vanilla->sibling,
                    "Model does not use the OoT/MM human Link limb hierarchy");
            Require((limbs[i].dLists[0]!=nullptr)==(vanilla->dList!=nullptr),
                    "Model changes Link's flex matrix layout; unsupported rig");
            table[i]=&limbs[i];
        }
        float original=0.f, replacement=0.f;
        for(unsigned i:{3u,4u,5u,10u}) {
            auto* v=reinterpret_cast<StandardLimb*>(header->segment[i]);
            original+=float(v->jointPos.x); replacement+=float(limbs[i].jointPos.x);
        }
        if(original>0 && replacement>0) rootRatio=replacement/original;
        ready=true;
    }
    void LoadNative() {
        if(ready) return;
        auto* header=ResourceMgr_LoadSkeletonByName(path.c_str(),nullptr);
        Require(header && header->limbCount==21 && header->segment && header->skeletonType==SKELANIME_TYPE_FLEX,
                "Select a flex skeleton with 21 Link limbs");
        for(unsigned i=0;i<21;++i) {
            // Standard and LOD limbs have identical position, child, sibling and first-DL fields.
            auto* src=reinterpret_cast<StandardLimb*>(header->segment[i]);
            Require(src!=nullptr,"Missing model limb");
            limbs[i].jointPos=src->jointPos;limbs[i].child=src->child;limbs[i].sibling=src->sibling;
            limbs[i].dLists[0]=limbs[i].dLists[1]=src->dList;
        }
        Validate();
    }
    void LoadZobj() {
        Range(0x5000,16);
        Require(!memcmp(bytes.data()+0x5000,"MODL",4),"Missing MODLOADER64 ZOBJ manifest");
        uint32_t skeleton=Pointer(0x500c); Range(skeleton,9);
        Require(bytes[skeleton+4]==21,"ZOBJ must have 21 human Link limbs");
        uint32_t lt=Pointer(skeleton);Range(lt,84);
        for(unsigned i=0;i<21;++i) {
            uint32_t pos=Pointer(lt+i*4);Range(pos,12);auto& limb=limbs[i];
            limb.jointPos={Be16(bytes.data()+pos),Be16(bytes.data()+pos+2),Be16(bytes.data()+pos+4)};
            limb.child=bytes[pos+6];limb.sibling=bytes[pos+7];
            uint32_t dl=Be32(bytes.data()+pos+8);
            if(dl) { Require((dl>>24)==6,"Invalid limb display list");limb.dLists[0]=limb.dLists[1]=Translate(dl&0xffffff); }
        }
        // Universal aliases are DL branch stubs. Older zzplayas offsets are remapped below.
        bool universal=std::search(bytes.begin(),bytes.end(),"UNIVERSAL_ALIAS_TABLE","UNIVERSAL_ALIAS_TABLE"+21)!=bytes.end();
        auto add=[&](uint32_t at,uint32_t alias) {
            if(at+8>bytes.size() || bytes[at]!=0xde || bytes[at+4]!=6) return;
            aliases[alias]=Translate(Be32(bytes.data()+at+4)&0xffffff);
        };
        if(universal) for(uint32_t at=0x5018;at<0x5800;at+=8) add(at,at);
        else {
            // zzplayas aliases retained from NEI's authoritative built-in loader (not soh/tools).
            static constexpr uint32_t adult[][2]={{0x5108,0x5098},{0x5110,0x50a0},{0x5118,0x50a8},{0x5120,0x50b0},
                {0x5128,0x50b8},{0x52b8,0x5448},{0x52d0,0x5458},{0x52e8,0x54f0},{0x5300,0x5460},
                {0x5310,0x5470},{0x5320,0x5478},{0x5330,0x5480},{0x5340,0x5488},{0x5350,0x5490},
                {0x5360,0x5498},{0x5370,0x54a0},{0x5238,0x53d8},{0x5258,0x53f0},{0x5268,0x53f8},
                {0x5278,0x5420},{0x5288,0x5428}};
            static constexpr uint32_t kid[][2]={{0x5150,0x5098},{0x5158,0x50a0},{0x5160,0x50a8},{0x5168,0x50b0},
                {0x5170,0x50b8},{0x51b8,0x53f0}};
            if(child) for(auto& pair:kid) add(pair[0],pair[1]);
            else for(auto& pair:adult) add(pair[0],pair[1]);
        }
        Validate();
    }
};
}
