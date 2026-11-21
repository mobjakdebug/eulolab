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
    if(!bytes_.open(error))return false;
    opened_=true;
    bench::Frame hello;
    hello.kind=bench::Kind::Hello;hello.sequence=++sequence_;hello.length=4;
    bench::write32(hello.payload,nonce_);
    bench::Frame reply;
    if(!exchange(hello,bench::Kind::Identity,reply,error) ||
       reply.length!=6 || bench::read32(reply.payload)!=nonce_ ||
       (reply.payload[4]&~1u)!=0 || reply.payload[5]!=bench::version) {
        if(error.empty())error="USB device did not identify as EuloBench/1.";
        bytes_.close();opened_=false;connected_=false;physicalGate_=false;
        return false;
    }
    connected_=true;
    physicalGate_=(reply.payload[4]&1)!=0;
    error.clear();return true;
}
bool BenchLink::pulse(uint8_t axis,int angleCdeg,uint16_t durationMs,
                      std::string& error) {
    if(!bench::safePulse(axis,angleCdeg,durationMs)) {
        error="Bench pulse exceeds axis, angle or 300 ms travel limits.";return false;
    }
    if(!connected_) {error="Connect a matching EuloBench USB board first.";return false;}
    if(!physicalGate_) {
        error="Physical bench-enable jumper is open; no output sent.";return false;
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
        error=cause+" Link closed; verify the board watchdog and re-identify.";
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
    opened_=false;connected_=false;physicalGate_=false;
}
} // namespace eulo
