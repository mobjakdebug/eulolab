#include "bench_link.hpp"

namespace eulo {
bool BenchLink::exchange(const bench::Frame& request,bench::Kind expected,
                         bench::Frame& response,std::string& error) {
    uint8_t outgoing[bench::maxFrame]={};
    const std::size_t length=bench::encode(request,outgoing);
    if(length==0) {error="Cannot encode bench command.";return false;}
    decoder_.reset();
    if(bytes_.write(outgoing,length,error)!=static_cast<int>(length)) {
        if(error.empty())error="USB bench write was incomplete.";
        return false;
    }
    for(int poll=0;poll<10;++poll) {
        uint8_t incoming[64]={};
        const int got=bytes_.read(incoming,sizeof(incoming),40,error);
        if(got<0 || got>static_cast<int>(sizeof(incoming))) {
            if(error.empty())error="USB bench read failed.";
            return false;
        }
        for(int i=0;i<got;++i) {
            bench::Frame frame;
            if(decoder_.push(incoming[i],frame) &&
               frame.sequence==request.sequence && frame.kind==expected) {
                response=frame;error.clear();return true;
            }
        }
    }
    error="No matching bench acknowledgement within 400 ms.";
    return false;
}
bool BenchLink::connect(std::string& error) {
    close();
    if(!profile_.validTransport(error))return false;
    if(!bytes_.open(error))return false;
    opened_=true;
    // Rotate the non-secret session token on EVERY connection. This helps
    // reject cached replies after reopening; it is not authentication.
    nonce_=nonce_*1664525u+1013904223u+sequence_;
    bench::Frame hello;
    hello.kind=bench::Kind::Hello;hello.sequence=++sequence_;hello.length=4;
    bench::write32(hello.payload,nonce_);
    bench::Frame reply;
    if(!exchange(hello,bench::Kind::Identity,reply,error) ||
       reply.length!=16 || bench::read32(reply.payload)!=nonce_ ||
       (reply.payload[4]&~1u)!=0 || reply.payload[5]!=bench::version ||
       reply.payload[6]<1 || reply.payload[6]>3 ||
       reply.payload[7]==bench::unsetPin ||
       (profile_.family!=bench::Family::Any &&
        reply.payload[6]!=static_cast<uint8_t>(profile_.family))) {
        if(error.empty())error="USB device did not identify as the chosen EuloBench/2 board.";
        close();return false;
    }
    capabilities_.family=reply.payload[6];
    capabilities_.gatePin=reply.payload[7];
    unsigned count=0;
    for(int i=0;i<8;++i) {
        const uint8_t pin=reply.payload[8+i];
        if(pin!=bench::unsetPin) {
            ++count;
            if(pin==capabilities_.gatePin ||
               (capabilities_.family==1 && pin>48) ||
               (capabilities_.family==2 && pin>0x7f)) {
                error="Board advertised an invalid or unsafe pin list.";
                close();return false;
            }
            for(int j=0;j<i;++j)if(capabilities_.allowed[j]==pin) {
                error="Board advertised duplicate servo pins.";
                close();return false;
            }
        }
        capabilities_.allowed[i]=pin;
    }
    if(count<4) {
        error="Board did not advertise four available servo pins.";
        close();return false;
    }
    physicalGate_=(reply.payload[4]&1)!=0;
    bench::Frame query;
    query.kind=bench::Kind::Pins;query.sequence=++sequence_;
    if(!exchange(query,bench::Kind::PinStatus,reply,error) ||
       reply.length!=5 || (reply.payload[0]!=0 && reply.payload[0]!=3)) {
        if(error.empty())error="Board pin-status query failed.";
        close();return false;
    }
    if(reply.payload[0]==0) {
        for(int i=0;i<4;++i) {
            if(!bench::allowedPin(capabilities_,reply.payload[1+i])) {
                error="Board reported a pin outside its own allowed list.";
                close();return false;
            }
            for(int j=0;j<i;++j)if(reply.payload[1+i]==reply.payload[1+j]) {
                error="Board reported duplicate active pins.";
                close();return false;
            }
        }
        pinsMatched_=true;
        for(int i=0;i<4;++i)
            if(profile_.pins[i]!=reply.payload[1+i])pinsMatched_=false;
    } else {
        for(int i=0;i<4;++i)if(reply.payload[1+i]!=bench::unsetPin) {
            error="Board sent inconsistent unmapped pin status.";
            close();return false;
        }
    }
    connected_=true;error.clear();return true;
}
bool BenchLink::applyPins(std::string& error) {
    if(!connected_) {error="Identify the EuloBench/2 device first.";return false;}
    if(!profile_.valid(error))return false; // identification may use a pinless draft
    if(physicalGate_) {
        error="Open the physical enable gate before applying pin mapping.";return false;
    }
    for(int i=0;i<4;++i) {
        if(!bench::allowedPin(capabilities_,profile_.pins[i])) {
            error="Selected pin is not in this board's firmware-approved pool.";
            return false;
        }
    }
    bench::Frame request;
    request.kind=bench::Kind::Pins;request.sequence=++sequence_;request.length=4;
    for(int i=0;i<4;++i)request.payload[i]=profile_.pins[i];
    bench::Frame reply;
    const bool received=exchange(request,bench::Kind::PinStatus,reply,error);
    if(!received || reply.length!=5 || reply.payload[0]!=0 ||
       reply.payload[1]!=request.payload[0] || reply.payload[2]!=request.payload[1] ||
       reply.payload[3]!=request.payload[2] || reply.payload[4]!=request.payload[3]) {
        if(received)error="Board rejected or mismatched the requested pin mapping.";
        const std::string cause=error;
        close();error=cause+" Link closed; verify board GPIOs and re-identify.";
        return false;
    }
    pinsMatched_=true;error.clear();return true;
}
bool BenchLink::pulse(uint8_t axis,int angleCdeg,uint16_t durationMs,
                      std::string& error) {
    if(!bench::safePulse(axis,angleCdeg,durationMs)) {
        error="Bench pulse exceeds axis, angle or 300 ms travel limits.";return false;
    }
    if(!connected_) {error="Connect a matching EuloBench/2 USB board first.";return false;}
    if(!physicalGate_) {
        error="Physical bench-enable jumper is open; no output sent.";return false;
    }
    if(!pinsMatched_) {
        error="Pins are not confirmed. Open the gate, identify and apply the mapping.";
        return false;
    }
    bench::Frame command;
    command.kind=bench::Kind::Pulse;command.sequence=++sequence_;command.length=5;
    command.payload[0]=axis;
    bench::write16(command.payload+1,static_cast<uint16_t>(angleCdeg));
    bench::write16(command.payload+3,durationMs);
    bench::Frame response;
    const bool replied=exchange(command,bench::Kind::Ack,response,error);
    if(!replied || response.length!=4 ||
       response.payload[0]!=static_cast<uint8_t>(bench::Kind::Pulse) ||
       response.payload[1]!=0 ||
       static_cast<int16_t>(bench::read16(response.payload+2))!=angleCdeg) {
        if(replied) {
            if(response.length>=2 && response.payload[1]==1)physicalGate_=false;
            error="Bench board rejected the pulse or returned a mismatched ACK.";
        }
        const std::string cause=error;
        std::string neutralError;
        neutral(neutralError);
        bytes_.close();opened_=false;connected_=false;physicalGate_=false;
        pinsMatched_=false;capabilities_={};
        error=cause+" Link closed; verify board neutral and re-identify.";
        return false;
    }
    error.clear();return true;
}
bool BenchLink::neutral(std::string& error) {
    if(!connected_) {error="No identified bench board is connected.";return false;}
    bench::Frame command;
    command.kind=bench::Kind::Neutral;command.sequence=++sequence_;
    bench::Frame response;
    const bool replied=exchange(command,bench::Kind::Ack,response,error);
    if(!replied || response.length!=4 ||
       response.payload[0]!=static_cast<uint8_t>(bench::Kind::Neutral) ||
       response.payload[1]!=0 || bench::read16(response.payload+2)!=0) {
        if(replied)error="Bench board did not acknowledge neutral.";
        bytes_.close();opened_=false;connected_=false;physicalGate_=false;
        pinsMatched_=false;
        return false;
    }
    error.clear();return true;
}
void BenchLink::close() {
    if(connected_) {
        std::string error;
        neutral(error);
    }
    if(opened_)bytes_.close();
    opened_=false;connected_=false;physicalGate_=false;pinsMatched_=false;
    capabilities_={};
}
} // namespace eulo
