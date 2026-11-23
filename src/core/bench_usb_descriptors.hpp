#pragma once
#include "bench_wire.hpp"
#include <cstddef>
#include <cstdint>

namespace eulo::bench {
struct BulkEndpoints {uint8_t in=0,out=0,interfaceNumber=0,configuration=0;};
// Read USB raw config bytes: OpenOrbis v0.5.4 leaves its endpoint descriptor
// type incomplete. Only a selected EuloBench vendor interface (alt 0) with
// exactly one bulk IN and OUT is accepted. Other composite interfaces are
// ignored, never claimed; 0 endpoint preference means auto-detect + verify.
inline bool findBenchInterface(const uint8_t* bytes,std::size_t size,
                               BulkEndpoints& result,uint8_t selectedInterface=0,
                               uint8_t expectedOut=0,uint8_t expectedIn=0) {
    if(!bytes || size<9 || size>512 || bytes[0]!=9 || bytes[1]!=2 ||
       read16(bytes+2)!=size || bytes[4]==0 || bytes[5]==0)return false;
    BulkEndpoints found{};
    bool selected=false,inside=false;
    int endpoints=0;
    for(std::size_t pos=9;pos<size;) {
        if(size-pos<2 || bytes[pos]<2 || bytes[pos]>size-pos)return false;
        const uint8_t length=bytes[pos],type=bytes[pos+1];
        if(type==4) {
            if(length<9)return false;
            inside=bytes[pos+2]==selectedInterface;
            if(inside) {
                if(selected || bytes[pos+3]!=0 || bytes[pos+4]!=2 ||
                   bytes[pos+5]!=0xff || bytes[pos+6]!=0x45 ||
                   bytes[pos+7]!=0x55)return false;
                selected=true;found.interfaceNumber=selectedInterface;
                found.configuration=bytes[5];
            }
        } else if(type==5) {
            if(length<7)return false;
            if(inside) {
                const uint8_t address=bytes[pos+2];
                if(++endpoints>2 || (address&0x70)!=0 ||
                   (address&0x0f)==0 || (bytes[pos+3]&3)!=2 ||
                   read16(bytes+pos+4)==0 || read16(bytes+pos+4)>512)
                    return false;
                uint8_t& endpoint=(address&0x80)?found.in:found.out;
                if(endpoint)return false;
                endpoint=address;
            }
        }
        pos+=length;
    }
    if(!selected || endpoints!=2 || !found.in || !found.out ||
       (expectedIn && expectedIn!=found.in) ||
       (expectedOut && expectedOut!=found.out))return false;
    result=found;
    return true;
}
} // namespace eulo::bench
