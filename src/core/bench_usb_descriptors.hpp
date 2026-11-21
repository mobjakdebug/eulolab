#pragma once
#include "bench_wire.hpp"
#include <cstddef>
#include <cstdint>

namespace eulo::bench {
struct BulkEndpoints {uint8_t in=0,out=0,interfaceNumber=0,configuration=0;};
// Read endpoint addresses from USB's raw configuration bytes: OpenOrbis v0.5.4
// declares libusb_endpoint_descriptor only as an incomplete type.
inline bool findBenchInterface(const uint8_t* bytes,std::size_t size,
                               BulkEndpoints& result) {
    if(!bytes || size<9 || size>512 || bytes[0]!=9 || bytes[1]!=2 ||
       read16(bytes+2)!=size || bytes[4]!=1 || bytes[5]==0)return false;
    BulkEndpoints found{};
    bool interfaceSeen=false;
    int endpoints=0;
    for(std::size_t pos=9;pos<size;) {
        if(size-pos<2 || bytes[pos]<2 || bytes[pos]>size-pos)return false;
        const uint8_t length=bytes[pos],type=bytes[pos+1];
        if(type==4) {
            if(length<9 || interfaceSeen || bytes[pos+2]!=0 ||
               bytes[pos+3]!=0 || bytes[pos+4]!=2 ||
               bytes[pos+5]!=0xff || bytes[pos+6]!=0x45 ||
               bytes[pos+7]!=0x55)return false;
            interfaceSeen=true;
            found.interfaceNumber=bytes[pos+2];
            found.configuration=bytes[5];
        }
        if(type==5) {
            if(length<7 || !interfaceSeen || ++endpoints>2 ||
               (bytes[pos+2]&0x70)!=0 || (bytes[pos+2]&0xf)==0 ||
               (bytes[pos+3]&3)!=2 || read16(bytes+pos+4)==0 ||
               read16(bytes+pos+4)>512)return false;
            const uint8_t address=bytes[pos+2];
            uint8_t& endpoint=(address&0x80)?found.in:found.out;
            if(endpoint)return false;
            endpoint=address;
        }
        pos+=length;
    }
    if(!interfaceSeen || endpoints!=2 || !found.in || !found.out)return false;
    result=found;
    return true;
}
} // namespace eulo::bench
