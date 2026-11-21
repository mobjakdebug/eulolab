#include "bench_board.hpp"

namespace eulo::bench {
void Board::tick(uint32_t nowMs,bool usbMounted,bool gateClosed) {
    if(cooldown_ && static_cast<int32_t>(nowMs-nextAllowedMs_)>=0)cooldown_=false;
    if(!usbMounted || !gateClosed) {
        session_=false;clearPulse();return;
    }
    if(active_ && static_cast<int32_t>(nowMs-deadlineMs_)>=0)clearPulse();
}
bool Board::receive(const Frame& request,uint32_t nowMs,bool usbMounted,
                    bool gateClosed,Frame& response) {
    tick(nowMs,usbMounted,gateClosed);
    if(!usbMounted)return false;
    response={};response.sequence=request.sequence;
    if(request.kind==Kind::Hello) {
        if(request.length!=4)return false;
        clearPulse();
        lastSequence_=request.sequence;
        session_=true;
        response.kind=Kind::Identity;response.length=6;
        write32(response.payload,read32(request.payload));
        response.payload[4]=gateClosed?1:0;
        response.payload[5]=version;
        return true;
    }
    if(request.kind==Kind::Neutral) {
        clearPulse();lastSequence_=request.sequence;
        response.kind=Kind::Ack;response.length=4;
        response.payload[0]=static_cast<uint8_t>(Kind::Neutral);
        response.payload[1]=request.length==0?0:2;
        return true;
    }
    if(request.kind!=Kind::Pulse)return false;
    response.kind=Kind::Ack;response.length=4;
    response.payload[0]=static_cast<uint8_t>(Kind::Pulse);
    if(request.length!=5)response.payload[1]=2;
    else {
        const uint8_t axis=request.payload[0];
        const int angleCdeg=static_cast<int16_t>(read16(request.payload+1));
        const uint16_t duration=read16(request.payload+3);
        if(!safePulse(axis,angleCdeg,duration))response.payload[1]=2;
        else if(!gateClosed)response.payload[1]=1;
        else if(!session_ || active_ ||
                (cooldown_ && static_cast<int32_t>(nowMs-nextAllowedMs_)<0))
            response.payload[1]=3;
        else if(static_cast<uint16_t>(request.sequence-lastSequence_)!=1)
            response.payload[1]=4;
        else {
            axis_=axis;angleCdeg_=angleCdeg;active_=true;
            deadlineMs_=nowMs+duration;
            nextAllowedMs_=deadlineMs_+500;cooldown_=true;
            lastSequence_=request.sequence;
            write16(response.payload+2,static_cast<uint16_t>(angleCdeg));
        }
    }
    return true;
}
} // namespace eulo::bench
