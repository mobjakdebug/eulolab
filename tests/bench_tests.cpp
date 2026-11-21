#include "core/bench_link.hpp"
#include "core/bench_board.hpp"
#include "core/bench_usb_descriptors.hpp"
#include <cassert>
#include <cstdio>
#include <deque>
#include <iterator>
#include <string>
#include <vector>

using namespace eulo;
namespace {
struct FakeUsb final : BenchBytes {
    enum class Fault { None, Open, WrongNonce, WrongVersion, Read, ShortWrite,
                       NoPulseAck, PulseNack, BadAck, NoNeutralAck };
    Fault fault=Fault::None;
    bool gate=true,opened=false;
    unsigned opens=0,closes=0,hellos=0,pulses=0,neutrals=0;
    std::deque<uint8_t> incoming;
    bool open(std::string& error) override {
        ++opens;
        if(fault==Fault::Open) {error="No matching board.";return false;}
        opened=true;return true;
    }
    int write(const uint8_t* data,std::size_t size,std::string& error) override {
        if(!opened || (fault==Fault::ShortWrite && size>0)) {
            error="Incomplete USB write";return static_cast<int>(size)-1;
        }
        bench::Decoder decoder;
        bench::Frame request;
        bool parsed=false;
        for(std::size_t i=0;i<size;++i)parsed=decoder.push(data[i],request)||parsed;
        assert(parsed);
        bench::Frame response;
        response.sequence=request.sequence;
        switch(request.kind) {
            case bench::Kind::Hello:
                ++hellos;assert(request.length==4);
                response.kind=bench::Kind::Identity;response.length=6;
                bench::write32(response.payload,bench::read32(request.payload)+
                    (fault==Fault::WrongNonce?1:0));
                response.payload[4]=gate?1:0;
                response.payload[5]=fault==Fault::WrongVersion?2:bench::version;
                break;
            case bench::Kind::Pulse:
                ++pulses;assert(request.length==5 && gate);
                assert(bench::safePulse(request.payload[0],
                    static_cast<int16_t>(bench::read16(request.payload+1)),
                    bench::read16(request.payload+3)));
                if(fault==Fault::NoPulseAck)return static_cast<int>(size);
                response.kind=bench::Kind::Ack;response.length=4;
                response.payload[0]=static_cast<uint8_t>(bench::Kind::Pulse);
                response.payload[1]=fault==Fault::PulseNack?1:0;
                bench::write16(response.payload+2,
                    fault==Fault::BadAck?0:bench::read16(request.payload+1));
                break;
            case bench::Kind::Neutral:
                ++neutrals;assert(request.length==0);
                if(fault==Fault::NoNeutralAck)return static_cast<int>(size);
                response.kind=bench::Kind::Ack;response.length=4;
                response.payload[0]=static_cast<uint8_t>(bench::Kind::Neutral);
                break;
            default:assert(false);
        }
        uint8_t bytes[bench::maxFrame];
        const std::size_t replySize=bench::encode(response,bytes);
        assert(replySize>0);
        for(std::size_t i=0;i<replySize;++i)incoming.push_back(bytes[i]);
        return static_cast<int>(size);
    }
    int read(uint8_t* bytes,std::size_t capacity,unsigned timeoutMs,
             std::string& error) override {
        assert(timeoutMs==40 && capacity>0);
        if(fault==Fault::Read) {error="Board unplugged.";return -1;}
        int count=0;
        while(!incoming.empty() && count<3 && count<static_cast<int>(capacity)) {
            bytes[count++]=incoming.front();incoming.pop_front();
        }
        return count;
    }
    void close() override {if(opened)++closes;opened=false;incoming.clear();}
};
void linkTests() {
    std::string error;
    {
        FakeUsb fake;
        BenchLink link(fake,0x12345678);
        assert(link.connect(error) && error.empty() && link.connected() && link.physicalGate());
        assert(!link.pulse(4,100,150,error) && !link.pulse(0,400,150,error) &&
               !link.pulse(0,0,150,error) && !link.pulse(0,100,301,error) &&
               fake.pulses==0);
        assert(link.pulse(3,-200,150,error) && error.empty() && fake.pulses==1);
        assert(link.neutral(error) && fake.neutrals==1);
        link.close();
        assert(!link.connected() && fake.neutrals==2 && fake.closes==1);
    }
    for(const auto fault:{FakeUsb::Fault::Open,FakeUsb::Fault::WrongNonce,
                          FakeUsb::Fault::WrongVersion,FakeUsb::Fault::Read,
                          FakeUsb::Fault::ShortWrite}) {
        FakeUsb fake;fake.fault=fault;
        BenchLink link(fake,9);
        assert(!link.connect(error) && !link.connected() && fake.pulses==0 &&
               !error.empty());
        if(fault!=FakeUsb::Fault::Open)assert(fake.closes==1);
    }
    {
        FakeUsb fake;fake.gate=false;
        BenchLink link(fake,9);
        assert(link.connect(error) && !link.physicalGate());
        assert(!link.pulse(0,-100,100,error) && fake.pulses==0);
        link.close();
    }
    for(const auto fault:{FakeUsb::Fault::NoPulseAck,FakeUsb::Fault::PulseNack,
                          FakeUsb::Fault::BadAck}) {
        FakeUsb fake;
        BenchLink link(fake,9);
        assert(link.connect(error));
        fake.fault=fault;
        assert(!link.pulse(0,200,100,error) && !error.empty() &&
               !link.connected() && fake.pulses==1 && fake.neutrals==1 &&
               fake.closes==1);
        link.close();
    }
    {
        FakeUsb fake;
        BenchLink link(fake,9);
        assert(link.connect(error));
        fake.fault=FakeUsb::Fault::NoNeutralAck;
        assert(!link.neutral(error) && !link.connected() && fake.closes==1);
        link.close();
    }
    std::puts("PASS bench link: identity, gate, bounds, ACK, failure neutral and close");
}
void boardTests() {
    using namespace bench;
    Board board;
    Frame request,reply;
    request.kind=Kind::Pulse;request.sequence=2;request.length=5;
    request.payload[0]=1;write16(request.payload+1,200);write16(request.payload+3,150);
    assert(board.receive(request,0,true,true,reply) && reply.payload[1]==3 &&
           !board.active()); // no session
    Frame hello;
    hello.kind=Kind::Hello;hello.length=4;hello.sequence=1;
    write32(hello.payload,0xaabbccdd);
    assert(board.receive(hello,1000,true,false,reply) &&
           reply.kind==Kind::Identity && reply.payload[4]==0 &&
           read32(reply.payload)==0xaabbccdd);
    assert(board.receive(request,1001,true,false,reply) && reply.payload[1]==1 &&
           !board.active());
    assert(board.receive(hello,1002,true,true,reply) && reply.payload[4]==1);
    assert(board.receive(request,1003,true,true,reply) && reply.kind==Kind::Ack &&
           reply.payload[1]==0 && read16(reply.payload+2)==200 &&
           board.active() && board.angle(1)==200 && board.angle(0)==0);
    assert(board.receive(request,1004,true,true,reply) && reply.payload[1]==3);
    board.tick(1152,true,true);assert(board.angle(1)==200);
    board.tick(1153,true,true);assert(!board.active() && board.angle(1)==0);
    assert(board.receive(request,1154,true,true,reply) && reply.payload[1]==3);
    assert(board.receive(request,1654,true,true,reply) && reply.payload[1]==4);
    request.sequence=3;
    assert(board.receive(request,1655,true,true,reply) && board.active());
    board.tick(1656,true,false);
    assert(!board.active() && !board.identified());
    request.sequence=4;
    assert(board.receive(request,1657,true,true,reply) && reply.payload[1]==3);
    assert(board.receive(hello,1658,true,true,reply));
    request.sequence=2;
    assert(board.receive(request,2306,true,true,reply) && board.active());
    board.tick(2307,false,true);
    assert(!board.active() && !board.identified());
    Frame neutral;
    neutral.kind=Kind::Neutral;neutral.sequence=3;
    assert(board.receive(neutral,2308,true,false,reply) &&
           reply.kind==Kind::Ack && reply.payload[1]==0 && !board.active());
    request.payload[0]=4;request.sequence=4;
    assert(board.receive(request,2309,true,true,reply) && reply.payload[1]==2);
    // Deadline and sequence wrap remain bounded after MCU tick counter rollover.
    Board rollover;
    hello.sequence=65535;
    assert(rollover.receive(hello,0xfffffffa,true,true,reply));
    request.payload[0]=0;request.sequence=0;
    assert(rollover.receive(request,0xfffffffc,true,true,reply) && rollover.active());
    rollover.tick(0x91,true,true);assert(rollover.active());
    rollover.tick(0x92,true,true);assert(!rollover.active());
    std::puts("PASS bench board: independent gate, stale sequence, USB loss, single pulse and watchdog");
}
struct LoopbackUsb final : BenchBytes {
    bench::Board board;
    bench::Decoder decoder;
    std::deque<uint8_t> incoming;
    bool mounted=false,gate=true;
    uint32_t now=100;
    bool open(std::string& error) override {mounted=true;error.clear();return true;}
    int write(const uint8_t* bytes,std::size_t size,std::string& error) override {
        if(!mounted) {error="Detached";return -1;}
        for(std::size_t i=0;i<size;++i) {
            bench::Frame request,reply;
            if(decoder.push(bytes[i],request) &&
               board.receive(request,now,mounted,gate,reply)) {
                uint8_t response[bench::maxFrame];
                const size_t length=bench::encode(reply,response);
                for(size_t j=0;j<length;++j)incoming.push_back(response[j]);
            }
        }
        return static_cast<int>(size);
    }
    int read(uint8_t* bytes,std::size_t capacity,unsigned,
             std::string&) override {
        int count=0;
        while(!incoming.empty() && count<5 && count<static_cast<int>(capacity)) {
            bytes[count++]=incoming.front();incoming.pop_front();
        }
        return count;
    }
    void close() override {
        mounted=false;board.tick(now,false,gate);incoming.clear();decoder.reset();
    }
    void advance(uint32_t duration) {now+=duration;board.tick(now,mounted,gate);}
};
void roundTripTest() {
    LoopbackUsb device;
    BenchLink host(device,0xaabbccdd);
    std::string error;
    assert(host.connect(error) && host.physicalGate() && !device.board.active());
    assert(host.pulse(1,200,150,error) && device.board.angle(1)==200);
    device.advance(149);assert(device.board.angle(1)==200);
    device.advance(1);assert(device.board.angle(1)==0);
    assert(!host.pulse(2,-200,150,error) && !host.connected()); // 500 ms neutral dwell
    assert(host.connect(error));
    device.advance(501);
    assert(host.pulse(2,-200,150,error) && device.board.angle(2)==-200);
    device.gate=false;device.advance(10);
    assert(!device.board.active() && !device.board.identified());
    assert(!host.pulse(0,200,150,error) && !host.connected() &&
           device.board.angle(2)==0 && !error.empty());
    std::puts("PASS bench loopback: actual host/board byte exchange and board-local timeout");
}
void descriptorTests() {
    using namespace bench;
    // Standard config, one EuloBench vendor interface, bulk OUT 0x01 / IN 0x81.
    const uint8_t descriptors[]={
        9,2,32,0,1,1,0,0x80,50,
        9,4,0,0,2,0xff,0x45,0x55,0,
        7,5,0x01,2,64,0,0,
        7,5,0x81,2,64,0,0
    };
    BulkEndpoints result;
    assert(findBenchInterface(descriptors,sizeof(descriptors),result));
    assert(result.out==1 && result.in==0x81 && result.interfaceNumber==0 &&
           result.configuration==1);
    for(std::size_t position:{2u,4u,9u,13u,14u,15u,16u,20u,21u,25u,27u}) {
        std::vector<uint8_t> candidate(std::begin(descriptors),std::end(descriptors));
        candidate[position]^=1;
        assert(!findBenchInterface(candidate.data(),candidate.size(),result));
    }
    for(std::size_t length=0;length<sizeof(descriptors);++length)
        assert(!findBenchInterface(descriptors,length,result));
    auto duplicate=std::vector<uint8_t>(std::begin(descriptors),std::end(descriptors));
    duplicate[20]=0x81;assert(!findBenchInterface(duplicate.data(),duplicate.size(),result));
    duplicate.assign(std::begin(descriptors),std::end(descriptors));
    duplicate[27]=0x01;assert(!findBenchInterface(duplicate.data(),duplicate.size(),result));
    std::puts("PASS bench USB: strict raw vendor/interface/bulk endpoint matching");
}
} // namespace
int main() {linkTests();boardTests();roundTripTest();descriptorTests();}
