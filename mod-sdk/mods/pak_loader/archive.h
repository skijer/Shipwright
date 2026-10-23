#pragma once
// Bounded, memory-only ModLoader64/zlib reader. No filesystem or engine dependency.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace pak {
using Bytes = std::vector<uint8_t>;
static constexpr size_t Limit = 32 * 1024 * 1024;
static uint32_t Be32(const uint8_t* p) { return uint32_t(p[0])<<24 | uint32_t(p[1])<<16 | uint32_t(p[2])<<8 | p[3]; }
static int16_t Be16(const uint8_t* p) { return int16_t(uint16_t(p[0])<<8 | p[1]); }
static void Require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
struct Bits {
    const uint8_t* data; size_t size, bit = 0;
    uint32_t Read(unsigned n) {
        Require(n <= 16 && bit + n <= size * 8, "Truncated DEFL stream");
        uint32_t v = 0;
        for (unsigned i=0; i<n; ++i, ++bit) v |= ((data[bit/8] >> (bit%8)) & 1u) << i;
        return v;
    }
};
struct Huffman {
    std::array<unsigned,16> counts{};
    std::vector<unsigned> symbols;
    explicit Huffman(const std::vector<unsigned>& lens) {
        for (unsigned n : lens) { Require(n<=15, "Invalid Huffman length"); ++counts[n]; }
        int left=1;
        for (unsigned n=1;n<=15;++n) { left=left*2-int(counts[n]); Require(left>=0, "Oversubscribed Huffman tree"); }
        std::array<unsigned,16> offsets{};
        for (unsigned n=1;n<15;++n) offsets[n+1]=offsets[n]+counts[n];
        symbols.resize(lens.size()-counts[0]);
        for (unsigned i=0;i<lens.size();++i) if(lens[i]) symbols[offsets[lens[i]]++]=i;
    }
    unsigned Decode(Bits& b) const {
        unsigned code=0, first=0, index=0;
        for(unsigned len=1;len<=15;++len) {
            code |= b.Read(1);
            if(code>=first && code-first<counts[len]) return symbols[index+code-first];
            index+=counts[len]; first=(first+counts[len])<<1; code<<=1;
        }
        throw std::runtime_error("Invalid Huffman code");
    }
};
static uint32_t Adler(const Bytes& b) {
    uint32_t a=1,c=0; for(uint8_t v:b) { a=(a+v)%65521; c=(c+a)%65521; } return c<<16 | a;
}
static Bytes Inflate(const uint8_t* data, size_t size) {
    Require(size>=6 && (data[0]&15)==8 && (data[0]>>4)<=7 && !(data[1]&32) &&
            ((unsigned(data[0])<<8 | data[1])%31)==0, "Unsupported zlib header");
    Bits b{data+2,size-6}; Bytes out;
    static constexpr unsigned lb[]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static constexpr unsigned le[]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static constexpr unsigned db[]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static constexpr unsigned de[]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    bool final=false;
    while(!final) {
        final=b.Read(1)!=0; unsigned type=b.Read(2);
        if(type==0) {
            b.bit=(b.bit+7)&~size_t(7); unsigned n=b.Read(16), inv=b.Read(16);
            Require((n^inv)==65535 && n<=Limit-out.size(), "Invalid stored block");
            for(unsigned i=0;i<n;++i) out.push_back(uint8_t(b.Read(8)));
            continue;
        }
        Require(type!=3, "Reserved DEFL block");
        std::vector<unsigned> lengths, distances;
        if(type==1) {
            lengths.resize(288);
            for(unsigned i=0;i<288;++i) lengths[i]=i<144?8:i<256?9:i<280?7:8;
            distances.assign(32,5);
        } else {
            unsigned nl=b.Read(5)+257, nd=b.Read(5)+1, nc=b.Read(4)+4;
            Require(nl<=286, "Invalid DEFL literal count");
            // RFC 1951 code-length alphabet order.
            static constexpr unsigned alphabet[]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            std::vector<unsigned> cl(19);
            for(unsigned i=0;i<nc;++i) cl[alphabet[i]]=b.Read(3);
            Huffman tree(cl); std::vector<unsigned> all;
            while(all.size()<nl+nd) {
                unsigned v=tree.Decode(b), repeat=1, value=v;
                if(v==16) { Require(!all.empty(),"Invalid repeat"); repeat=b.Read(2)+3; value=all.back(); }
                else if(v==17) { repeat=b.Read(3)+3; value=0; }
                else if(v==18) { repeat=b.Read(7)+11; value=0; }
                Require(repeat<=nl+nd-all.size(), "Huffman repeat overflow");
                all.insert(all.end(),repeat,value);
            }
            lengths.clear(); distances.clear();
            for(size_t i=0;i<all.size();++i) (i<nl ? lengths : distances).push_back(all[i]);
        }
        Require(lengths.size()>256 && lengths[256]!=0,"Missing DEFL end code");
        Huffman lit(lengths), dist(distances);
        for(;;) {
            unsigned v=lit.Decode(b);
            if(v==256) break;
            if(v<256) { Require(out.size()<Limit,"DEFL output exceeds 32 MiB"); out.push_back(uint8_t(v)); continue; }
            Require(v>=257 && v<=285,"Reserved length code"); v-=257;
            unsigned n=lb[v]+b.Read(le[v]), d=dist.Decode(b);
            Require(d<30,"Reserved distance code"); d=db[d]+b.Read(de[d]);
            Require(d<=out.size() && n<=Limit-out.size(),"Invalid DEFL backreference");
            while(n--) out.push_back(out[out.size()-d]);
        }
    }
    Require(Adler(out)==Be32(data+size-4),"DEFL checksum mismatch");
    return out;
}
struct File { std::string name; Bytes data; };
static std::vector<File> Read(const uint8_t* data, size_t size) {
    Require(size>=28 && size<=Limit && !memcmp(data,"ModLoader64",11),"Not a ModLoader64 PAK");
    size_t pos=12;
    while(pos+16<=size && memcmp(data+pos,"UNCO",4) && memcmp(data+pos,"DEFL",4)) ++pos;
    std::vector<File> result; size_t total=0;
    for(;pos+16<=size;pos+=16) {
        bool compressed=!memcmp(data+pos,"DEFL",4);
        if(!compressed && memcmp(data+pos,"UNCO",4)) break;
        Require(result.size()<512,"Too many PAK entries");
        uint32_t name=Be32(data+pos+4), start=Be32(data+pos+8), end=Be32(data+pos+12);
        Require(name<size && start<=end && end<=size,"Invalid PAK entry range");
        size_t last=name;
        while(last<size && data[last]!=0 && data[last]!=255 && last-name<=1024) ++last;
        Require(last<size && last-name<=1024,"Invalid PAK filename");
        Bytes bytes=compressed?Inflate(data+start,end-start):Bytes(data+start,data+end);
        Require(bytes.size()<=Limit-total,"PAK output exceeds 32 MiB"); total+=bytes.size();
        result.push_back({std::string(reinterpret_cast<const char*>(data+name),last-name),std::move(bytes)});
    }
    Require(!result.empty(),"Empty PAK"); return result;
}
static uint32_t Crc(const uint8_t* data,size_t size) {
    uint32_t crc=~0u; for(size_t i=0;i<size;++i) { crc^=data[i]; for(int b=0;b<8;++b) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1))); } return ~crc;
}
// Store the original PAK inside an asset-only O2R so it can be reopened through ReadArchiveFiles next boot.
static Bytes Wrap(const std::string& path,const uint8_t* data,size_t size) {
    Require(size<=Limit && path.size()<65536,"Import too large"); Bytes out;
    auto w16=[&](uint32_t v){out.push_back(uint8_t(v));out.push_back(uint8_t(v>>8));};
    auto w32=[&](uint32_t v){w16(v);w16(v>>16);};
    uint32_t crc=Crc(data,size);
    w32(0x04034b50);w16(20);w16(0);w16(0);w16(0);w16(0);w32(crc);w32(uint32_t(size));w32(uint32_t(size));w16(uint32_t(path.size()));w16(0);
    out.insert(out.end(),path.begin(),path.end());out.insert(out.end(),data,data+size);
    uint32_t central=uint32_t(out.size());
    w32(0x02014b50);w16(20);w16(20);w16(0);w16(0);w16(0);w16(0);w32(crc);w32(uint32_t(size));w32(uint32_t(size));
    w16(uint32_t(path.size()));w16(0);w16(0);w16(0);w16(0);w32(0);w32(0);out.insert(out.end(),path.begin(),path.end());
    uint32_t centralSize=uint32_t(out.size())-central;
    w32(0x06054b50);w16(0);w16(0);w16(1);w16(1);w32(centralSize);w32(central);w16(0); return out;
}
}
