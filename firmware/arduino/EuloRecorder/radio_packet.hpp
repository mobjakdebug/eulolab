// EuloLab/1 point-to-point SX127x packet. Not LoRaWAN, mesh, or encrypted.
// Pure C++11: shared by Arduino transmitter, receiver and native protocol tests.
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace eulo_radio {
constexpr std::size_t packetSize=44;
struct Sample {
    uint32_t sequence=0,timeMs=0;
    double pressurePa=0,temperatureC=0;
    double gyroDegS[3]={},accelMs2[3]={};
};
inline void put16(uint8_t* p,uint16_t v) {
    p[0]=static_cast<uint8_t>(v);p[1]=static_cast<uint8_t>(v>>8);
}
inline void put32(uint8_t* p,uint32_t v) {
    for(int i=0;i<4;++i)p[i]=static_cast<uint8_t>(v>>(i*8));
}
inline uint16_t get16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]|(static_cast<uint16_t>(p[1])<<8));
}
inline uint32_t get32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])|(static_cast<uint32_t>(p[1])<<8)|
        (static_cast<uint32_t>(p[2])<<16)|(static_cast<uint32_t>(p[3])<<24);
}
inline uint16_t crc16(const uint8_t* bytes,std::size_t count) {
    uint16_t crc=0xffff;
    for(std::size_t i=0;i<count;++i) {
        crc^=static_cast<uint16_t>(bytes[i])<<8;
        for(int bit=0;bit<8;++bit)
            crc=(crc&0x8000)?static_cast<uint16_t>((crc<<1)^0x1021):
                               static_cast<uint16_t>(crc<<1);
    }
    return crc;
}
inline bool valid(const Sample& s) {
    if(s.timeMs>3600000 || !std::isfinite(s.pressurePa) ||
       s.pressurePa<30000 || s.pressurePa>125000 ||
       !std::isfinite(s.temperatureC) || s.temperatureC < -40 ||
       s.temperatureC > 85)return false;
    for(int i=0;i<3;++i)if(!std::isfinite(s.gyroDegS[i]) ||
        !std::isfinite(s.accelMs2[i]) || std::fabs(s.gyroDegS[i])>2000 ||
        std::fabs(s.accelMs2[i])>200)return false;
    return true;
}
inline int32_t signed32(uint32_t u) {
    return u<=0x7fffffffU?static_cast<int32_t>(u):
        static_cast<int32_t>(-static_cast<int64_t>(0x100000000ULL-u));
}
inline int16_t signed16(uint16_t u) {
    return u<=0x7fffU?static_cast<int16_t>(u):
        static_cast<int16_t>(-static_cast<int32_t>(0x10000U-u));
}
inline bool encode(const Sample& s,uint8_t* out,std::size_t capacity) {
    if(!out || capacity<packetSize || !valid(s))return false;
    out[0]='E';out[1]='R';out[2]=1;out[3]=0; // version and reserved flags
    put32(out+4,s.sequence);put32(out+8,s.timeMs);
    put32(out+12,static_cast<uint32_t>(std::lround(s.pressurePa)));
    put16(out+16,static_cast<uint16_t>(static_cast<int16_t>(
        std::lround(s.temperatureC*100))));
    for(int i=0;i<3;++i) {
        put32(out+18+i*4,static_cast<uint32_t>(static_cast<int32_t>(
            std::lround(s.gyroDegS[i]*1000))));
        put32(out+30+i*4,static_cast<uint32_t>(static_cast<int32_t>(
            std::lround(s.accelMs2[i]*1000))));
    }
    put16(out+42,crc16(out,42));
    return true;
}
inline bool decode(const uint8_t* bytes,std::size_t length,Sample& out) {
    if(!bytes || length!=packetSize || bytes[0]!='E' || bytes[1]!='R' ||
       bytes[2]!=1 || bytes[3]!=0 || get16(bytes+42)!=crc16(bytes,42))
        return false;
    Sample candidate;
    candidate.sequence=get32(bytes+4);candidate.timeMs=get32(bytes+8);
    candidate.pressurePa=get32(bytes+12);
    candidate.temperatureC=signed16(get16(bytes+16))*.01;
    for(int i=0;i<3;++i) {
        candidate.gyroDegS[i]=signed32(get32(bytes+18+i*4))*.001;
        candidate.accelMs2[i]=signed32(get32(bytes+30+i*4))*.001;
    }
    if(!valid(candidate))return false;
    out=candidate;
    return true;
}
} // namespace eulo_radio
