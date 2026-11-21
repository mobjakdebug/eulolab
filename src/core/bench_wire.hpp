#pragma once
#include <cstddef>
#include <cstdint>

namespace eulo::bench {
// EuloBench/1 is a bench-only USB byte protocol; it has no arming, ignition,
// radio or in-flight command. See docs/bench-usb.md before wiring hardware.
constexpr std::size_t maxPayload=16,maxFrame=7+maxPayload+2;
constexpr uint8_t version=1;
enum class Kind : uint8_t {
    Hello=1, Pulse=2, Neutral=3,
    Identity=0x81, Ack=0x82
};
struct Frame {
    Kind kind=Kind::Hello;
    uint16_t sequence=0;
    uint8_t length=0;
    uint8_t payload[maxPayload]={};
};
inline uint16_t read16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1])<<8));
}
inline uint32_t read32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1])<<8) |
        (static_cast<uint32_t>(p[2])<<16) | (static_cast<uint32_t>(p[3])<<24);
}
inline void write16(uint8_t* p,uint16_t n) {
    p[0]=static_cast<uint8_t>(n);
    p[1]=static_cast<uint8_t>(n>>8);
}
inline void write32(uint8_t* p,uint32_t n) {
    p[0]=static_cast<uint8_t>(n);p[1]=static_cast<uint8_t>(n>>8);
    p[2]=static_cast<uint8_t>(n>>16);p[3]=static_cast<uint8_t>(n>>24);
}
inline uint16_t crc16(const uint8_t* bytes,std::size_t length) {
    uint16_t crc=0xffff;
    for(std::size_t i=0;i<length;++i) {
        crc^=static_cast<uint16_t>(bytes[i])<<8;
        for(int bit=0;bit<8;++bit)
            crc=(crc&0x8000)?static_cast<uint16_t>((crc<<1)^0x1021):
                static_cast<uint16_t>(crc<<1);
    }
    return crc;
}
inline std::size_t encode(const Frame& frame,uint8_t* out) {
    if(!out || frame.length>maxPayload)return 0;
    out[0]='E';out[1]='U';out[2]=version;
    out[3]=static_cast<uint8_t>(frame.kind);
    write16(out+4,frame.sequence);
    out[6]=frame.length;
    for(std::size_t i=0;i<frame.length;++i)out[7+i]=frame.payload[i];
    const std::size_t body=7+frame.length;
    write16(out+body,crc16(out,body));
    return body+2;
}
class Decoder {
public:
    bool push(uint8_t byte,Frame& result) {
        if(used_==0) {
            if(byte=='E')buffer_[used_++]=byte;
            return false;
        }
        if(used_==1 && byte!='U') {
            used_=byte=='E'?1:0;
            return false;
        }
        buffer_[used_++]=byte;
        if(used_==7 && (buffer_[2]!=version || buffer_[6]>maxPayload)) {
            used_=byte=='E'?1:0;
            if(used_)buffer_[0]='E';
            return false;
        }
        if(used_<7)return false;
        const std::size_t body=7+buffer_[6],total=body+2;
        if(used_<total)return false;
        used_=0;
        if(read16(buffer_+body)!=crc16(buffer_,body))return false;
        result.kind=static_cast<Kind>(buffer_[3]);
        result.sequence=read16(buffer_+4);
        result.length=buffer_[6];
        for(std::size_t i=0;i<result.length;++i)result.payload[i]=buffer_[7+i];
        return true;
    }
    void reset(){used_=0;}
private:
    uint8_t buffer_[maxFrame]={};
    std::size_t used_=0;
};
inline bool safePulse(uint8_t axis,int angleCdeg,uint16_t durationMs) {
    // Fixed 300 ms pulse cap and +/-3 deg travel in this first bench profile.
    return axis<4 && angleCdeg!=0 && angleCdeg>=-300 && angleCdeg<=300 &&
           durationMs>=50 && durationMs<=300;
}
} // namespace eulo::bench
