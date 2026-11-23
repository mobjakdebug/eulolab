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
bench::Profile testProfile() {
    bench::Profile p;
    p.family=bench::Family::Esp32;
    p.vendor=0xffff;p.product=0xffff; // test-only reserved IDs; never flash
    p.interfaceNumber=0;p.bulkOut=1;p.bulkIn=0x81;
    p.pins={{4,5,6,7}};
    return p;
}
bench::Capabilities testCapabilities() {
    bench::Capabilities c;
    c.family=1;c.gatePin=8;
    const uint8_t pins[]={4,5,6,7,9,10,11,12};
    for(int i=0;i<8;++i)c.allowed[i]=pins[i];
    return c;
}
void profileTests() {
    const char* path="out/bench-profile-test.cfg";
    std::remove(path);
    std::string error;
    bench::Profile p;
    assert(!p.valid(error) && !error.empty());
    assert(!bench::loadProfile(p,path,error) && p.vendor==0);
    p=testProfile();
    assert(p.valid(error) && bench::saveProfile(p,path,error));
    bench::Profile loaded;
    assert(bench::loadProfile(loaded,path,error) && loaded.vendor==0xffff &&
           loaded.product==0xffff && loaded.pins==p.pins);
    auto invalid=p;invalid.pins[1]=4;
    assert(!bench::saveProfile(invalid,path,error));
    invalid=p;invalid.bulkIn=0x01;
    assert(!invalid.valid(error));
    invalid=p;invalid.pins[2]=0xff;
    assert(invalid.validDraft(error) && !invalid.valid(error));
    assert(bench::saveProfile(invalid,path,error)); // identify before choosing GPIOs
    loaded=bench::Profile{};
    assert(bench::loadProfile(loaded,path,error) && loaded.pins==invalid.pins &&
           loaded.validTransport(error));
    assert(bench::saveProfile(p,path,error));
    FILE* file=std::fopen(path,"wb");
    assert(file && std::fputs("EULOBENCH2\nfamily=1\n",file)>0 &&
           std::fclose(file)==0);
    assert(!bench::loadProfile(loaded,path,error) && loaded.pins==invalid.pins);
    std::remove(path);
    assert(bench::pinName(bench::Family::Stm32,0x1e)=="PB14" &&
           bench::pinName(bench::Family::Esp32,4)=="GPIO4");
    std::puts("PASS bench profile: app-editable IDs/endpoints/pins, atomic persistence and fail-closed parse");
}
struct FakeUsb final : BenchBytes {
    enum class Fault { None, Open, WrongNonce, WrongVersion, WrongFamily,
                       Read, ShortWrite, NoPulseAck, PulseNack, BadAck,
                       NoNeutralAck, NoPinAck, InvalidPinList };
    Fault fault=Fault::None;
    bool gate=false,opened=false,mapped=false;
    unsigned opens=0,closes=0,hellos=0,pulses=0,neutrals=0,pinSets=0;
    uint32_t lastNonce=0;
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
            case bench::Kind::Hello: {
                ++hellos;assert(request.length==4);
                const uint32_t token=bench::read32(request.payload);
                if(hellos>1)assert(token!=lastNonce);
                lastNonce=token;
                const auto caps=testCapabilities();
                response.kind=bench::Kind::Identity;response.length=16;
                bench::write32(response.payload,bench::read32(request.payload)+
                    (fault==Fault::WrongNonce?1:0));
                response.payload[4]=gate?1:0;
                response.payload[5]=fault==Fault::WrongVersion?1:bench::version;
                response.payload[6]=fault==Fault::WrongFamily?2:caps.family;
                response.payload[7]=caps.gatePin;
                for(int i=0;i<8;++i)response.payload[8+i]=caps.allowed[i];
                if(fault==Fault::InvalidPinList)response.payload[8]=caps.gatePin;
                break;
            }
            case bench::Kind::Pins:
                if(request.length==4) {
                    ++pinSets;
                    if(fault==Fault::NoPinAck)return static_cast<int>(size);
                    assert(!gate);
                    mapped=true;
                } else assert(request.length==0);
                response.kind=bench::Kind::PinStatus;response.length=5;
                response.payload[0]=mapped?0:3;
                for(int i=0;i<4;++i)
                    response.payload[1+i]=mapped?static_cast<uint8_t>(4+i):0xff;
                break;
            case bench::Kind::Pulse:
                ++pulses;assert(request.length==5 && gate && mapped);
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
    auto profile=testProfile();
    {
        FakeUsb fake;
        BenchLink link(fake,profile,0x12345678);
        assert(link.connect(error) && error.empty() && link.connected() &&
               !link.physicalGate() && !link.pinsMatched());
        assert(!link.pulse(0,200,150,error) && fake.pulses==0);
        assert(link.applyPins(error) && link.pinsMatched() && fake.pinSets==1);
        fake.gate=true;
        assert(!link.pulse(0,200,150,error) && fake.pulses==0); // cached gate open
        assert(link.connect(error) && link.physicalGate() && link.pinsMatched());
        assert(!link.applyPins(error) && fake.pinSets==1);
        assert(!link.pulse(4,100,150,error) && !link.pulse(0,400,150,error) &&
               !link.pulse(0,0,150,error) && !link.pulse(0,100,301,error));
        assert(link.pulse(3,-200,150,error) && error.empty() && fake.pulses==1);
        assert(link.neutral(error));link.close();
        assert(!link.connected() && fake.neutrals==3 && fake.closes==2);
    }
    for(const auto fault:{FakeUsb::Fault::Open,FakeUsb::Fault::WrongNonce,
                          FakeUsb::Fault::WrongVersion,FakeUsb::Fault::WrongFamily,
                          FakeUsb::Fault::InvalidPinList,FakeUsb::Fault::Read,
                          FakeUsb::Fault::ShortWrite}) {
        FakeUsb fake;fake.fault=fault;
        BenchLink link(fake,profile,9);
        assert(!link.connect(error) && !link.connected() && fake.pulses==0 &&
               !error.empty());
        if(fault!=FakeUsb::Fault::Open)assert(fake.closes==1);
    }
    for(const auto fault:{FakeUsb::Fault::NoPulseAck,FakeUsb::Fault::PulseNack,
                          FakeUsb::Fault::BadAck}) {
        FakeUsb fake;fake.gate=true;fake.mapped=true;
        BenchLink link(fake,profile,9);
        assert(link.connect(error));
        fake.fault=fault;
        assert(!link.pulse(0,200,100,error) && !error.empty() &&
               !link.connected() && fake.pulses==1 && fake.neutrals==1 &&
               fake.closes==1);
        link.close();
    }
    {
        FakeUsb fake;fake.gate=true;fake.mapped=true;
        BenchLink link(fake,profile,9);
        assert(link.connect(error));
        fake.fault=FakeUsb::Fault::NoNeutralAck;
        assert(!link.neutral(error) && !link.connected() && fake.closes==1);
    }
    {
        FakeUsb fake;
        BenchLink link(fake,profile,9);
        assert(link.connect(error));
        fake.fault=FakeUsb::Fault::NoPinAck;
        assert(!link.applyPins(error) && !link.connected() && fake.closes==1);
    }
    auto draft=testProfile();draft.pins={{0xff,0xff,0xff,0xff}};
    FakeUsb draftUsb;
    BenchLink identifyFirst(draftUsb,draft,9);
    assert(identifyFirst.connect(error) && !identifyFirst.pinsMatched());
    assert(!identifyFirst.applyPins(error) && draftUsb.pinSets==0);
    identifyFirst.close();
    auto bad=testProfile();bad.vendor=0;
    FakeUsb fake;
    BenchLink disabled(fake,bad,9);
    assert(!disabled.connect(error) && fake.opens==0);
    std::puts("PASS bench link: draft identity, gated mapping, ACK, failure neutral and close");
}
void boardTests() {
    using namespace bench;
    Board board(testCapabilities());
    Frame request,reply;
    request.kind=Kind::Pulse;request.sequence=2;request.length=5;
    request.payload[0]=1;write16(request.payload+1,200);write16(request.payload+3,150);
    assert(board.receive(request,0,true,true,reply) && reply.payload[1]==3 &&
           !board.active()); // no session and no verified pins
    Frame hello;
    hello.kind=Kind::Hello;hello.length=4;hello.sequence=1;
    write32(hello.payload,0xaabbccdd);
    assert(board.receive(hello,1000,true,false,reply) &&
           reply.kind==Kind::Identity && reply.length==16 && reply.payload[4]==0 &&
           reply.payload[6]==1 && reply.payload[7]==8 &&
           read32(reply.payload)==0xaabbccdd);
    Frame pins;
    pins.kind=Kind::Pins;pins.sequence=2;pins.length=4;
    for(int i=0;i<4;++i)pins.payload[i]=static_cast<uint8_t>(4+i);
    auto invalid=pins;invalid.payload[1]=4;
    assert(board.receive(invalid,1001,true,false,reply) && reply.payload[0]==2);
    invalid=pins;invalid.sequence=3;invalid.payload[1]=8; // gate pin
    assert(board.receive(invalid,1002,true,false,reply) && reply.payload[0]==2);
    invalid=pins;invalid.sequence=4;invalid.payload[1]=20; // outside pool
    assert(board.receive(invalid,1003,true,false,reply) && reply.payload[0]==2);
    pins.sequence=5;
    assert(board.receive(pins,1004,true,false,reply) && reply.kind==Kind::PinStatus &&
           reply.payload[0]==0 && board.pin(1)==5 && !board.active());
    pins.sequence=6;
    assert(board.receive(pins,1005,true,true,reply) && reply.payload[0]==1);
    request.sequence=7;
    assert(board.receive(request,1006,true,true,reply) && reply.payload[1]==0 &&
           board.active() && board.angleOnPin(5)==200 && board.angleOnPin(4)==0);
    assert(board.receive(request,1007,true,true,reply) && reply.payload[1]==3);
    board.tick(1155,true,true);assert(board.angleOnPin(5)==200);
    board.tick(1156,true,true);assert(!board.active() && board.angleOnPin(5)==0);
    request.sequence=8;
    assert(board.receive(request,1157,true,true,reply) && reply.payload[1]==3);
    request.sequence=7; // replayed, not the expected next sequence
    assert(board.receive(request,1657,true,true,reply) && reply.payload[1]==4);
    request.sequence=8;
    assert(board.receive(request,1658,true,true,reply) && board.active());
    board.tick(1659,true,false); // falling gate neutralises + invalidates session
    assert(!board.active() && !board.identified());
    request.sequence=10;
    assert(board.receive(request,1660,true,true,reply) && reply.payload[1]==3);
    assert(board.receive(hello,1661,true,true,reply));
    request.sequence=2;
    assert(board.receive(request,2310,true,true,reply) && board.active());
    board.tick(2311,false,true); // unplug loses pin assignment
    assert(!board.active() && !board.identified() && !board.mapped());
    Frame neutral;
    neutral.kind=Kind::Neutral;neutral.sequence=3;
    assert(board.receive(neutral,2312,true,false,reply) &&
           reply.kind==Kind::Ack && reply.payload[1]==0 && !board.active());
    // Deadline and sequence wrap remain bounded after MCU tick counter rollover.
    Board rollover(testCapabilities());
    hello.sequence=65534;
    assert(rollover.receive(hello,0xfffffff9,true,false,reply));
    pins.sequence=65535;
    assert(rollover.receive(pins,0xfffffffa,true,false,reply) && rollover.mapped());
    assert(rollover.receive(hello,0xfffffffb,true,true,reply));
    hello.sequence=65535;
    assert(rollover.receive(hello,0xfffffffb,true,true,reply));
    request.payload[0]=0;request.sequence=0;
    assert(rollover.receive(request,0xfffffffc,true,true,reply) && rollover.active());
    rollover.tick(0x91,true,true);assert(rollover.active());
    rollover.tick(0x92,true,true);assert(!rollover.active());
    std::puts("PASS bench board: allowed pins, independent gate, stale sequence, USB loss and watchdog");
}
struct LoopbackUsb final : BenchBytes {
    bench::Board board{testCapabilities()};
    bench::Decoder decoder;
    std::deque<uint8_t> incoming;
    bool opened=false,mounted=true,gate=false;
    uint32_t now=100;
    bool open(std::string& error) override {
        if(!mounted) {error="Physically detached";return false;}
        opened=true;error.clear();return true;
    }
    int write(const uint8_t* bytes,std::size_t size,std::string& error) override {
        if(!opened || !mounted) {error="Detached";return -1;}
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
    void close() override {opened=false;incoming.clear();decoder.reset();}
    void advance(uint32_t duration) {now+=duration;board.tick(now,mounted,gate);}
    void detach() {mounted=false;board.tick(now,false,gate);}
};
void roundTripTest() {
    LoopbackUsb device;
    auto profile=testProfile();
    std::string error;
    BenchLink host(device,profile,0x41cafe55);
    assert(host.connect(error) && !host.physicalGate() && !host.pinsMatched());
    assert(!host.pulse(1,200,150,error) && !device.board.active());
    assert(host.applyPins(error) && host.pinsMatched() && device.board.pin(1)==5);
    device.gate=true;host.close();
    assert(host.connect(error) && host.physicalGate() && host.pinsMatched());
    assert(host.pulse(1,200,150,error) && device.board.angleOnPin(5)==200);
    device.advance(149);assert(device.board.angleOnPin(5)==200);
    device.advance(1);assert(device.board.angleOnPin(5)==0);
    assert(!host.pulse(2,-200,150,error) && !host.connected()); // 500 ms neutral dwell
    assert(host.connect(error));
    device.advance(501);
    assert(host.pulse(2,-200,150,error) && device.board.angleOnPin(6)==-200);
    device.gate=false;device.advance(10);
    assert(!device.board.active() && !device.board.identified());
    assert(!host.pulse(0,200,150,error) && !host.connected() &&
           device.board.angleOnPin(6)==0 && !error.empty());
    device.detach();assert(!device.board.mapped());
    assert(!host.connect(error));
    std::puts("PASS bench loopback: actual host/board bytes, remapping, gate and local timeout");
}
void descriptorTests() {
    using namespace bench;
    const uint8_t descriptors[]={
        9,2,32,0,1,1,0,0x80,50,
        9,4,0,0,2,0xff,0x45,0x55,0,
        7,5,0x01,2,64,0,0,
        7,5,0x81,2,64,0,0
    };
    BulkEndpoints result;
    assert(findBenchInterface(descriptors,sizeof(descriptors),result,0,1,0x81));
    assert(result.out==1 && result.in==0x81 && result.interfaceNumber==0 &&
           result.configuration==1);
    assert(!findBenchInterface(descriptors,sizeof(descriptors),result,1));
    assert(!findBenchInterface(descriptors,sizeof(descriptors),result,0,2));
    assert(!findBenchInterface(descriptors,sizeof(descriptors),result,0,1,0x82));
    for(std::size_t position:{2u,9u,13u,14u,15u,16u,20u,21u,25u,27u}) {
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
    // Composite config: skip an unrelated HID and claim ONLY selected vendor
    // interface 1. Wrong alt setting, class or endpoint preference must fail.
    std::vector<uint8_t> composite={
        9,2,57,0,2,1,0,0x80,50,
        9,4,0,0,1,3,1,1,0,
        7,5,0x83,3,8,0,1,
        9,4,1,0,2,0xff,0x45,0x55,0,
        7,5,0x02,2,64,0,0,
        7,5,0x82,2,64,0,0
    };
    // Header total length = 9 + 9 + 7 + 9 + 7 + 7 = 48.
    composite[2]=48;
    assert(findBenchInterface(composite.data(),composite.size(),result,1,2,0x82));
    assert(!findBenchInterface(composite.data(),composite.size(),result,0));
    composite[28]=1; // vendor alternate setting 1
    assert(!findBenchInterface(composite.data(),composite.size(),result,1));
    std::puts("PASS bench USB: selected vendor/bulk interface and strict descriptor matching");
}
} // namespace
int main() {profileTests();linkTests();boardTests();roundTripTest();descriptorTests();}
