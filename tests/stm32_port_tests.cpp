#include "firmware/stm32/bench_port.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <deque>
#include <vector>

using namespace eulo;
namespace {
struct Hardware final : stm32::Hardware {
    bool gate=false;
    std::array<uint16_t,128> outputs{};
    std::deque<uint8_t> replies;
    unsigned writes=0;
    bool gateClosed() const override {return gate;}
    void setPwmMicroseconds(uint8_t pin,uint16_t width) override {
        assert(pin<128 && width>=1450 && width<=1550);
        outputs[pin]=width;++writes;
    }
    bool writeBulkIn(const uint8_t* data,std::size_t length) override {
        for(std::size_t i=0;i<length;++i)replies.push_back(data[i]);
        return true;
    }
    bench::Frame send(stm32::Port& port,bench::Frame request,uint32_t nowMs) {
        uint8_t data[bench::maxFrame];
        const std::size_t length=bench::encode(request,data);
        assert(length);
        // Split a frame across USB OUT callbacks; framing is a byte stream.
        for(std::size_t i=0;i<length;i+=3)
            port.onBulkOut(data+i,std::min<std::size_t>(3,length-i),nowMs);
        bench::Decoder decoder;bench::Frame reply;
        bool decoded=false;
        while(!replies.empty()) {
            decoded=decoder.push(replies.front(),reply)||decoded;
            replies.pop_front();
        }
        assert(decoded);return reply;
    }
};
bench::Capabilities pins() {
    bench::Capabilities caps;caps.family=2;caps.gatePin=0x18; // PB8 input
    const uint8_t allowed[]={0x00,0x01,0x1a,0x1b,0x24,0x25,0xff,0xff};
    for(int i=0;i<8;++i)caps.allowed[i]=allowed[i];
    return caps;
}
void run() {
    Hardware hw;
    auto caps=pins();
    stm32::Port device(hw,caps);
    assert(device.ready());
    device.tick(0);
    assert(hw.outputs[0x00]==1500 && hw.outputs[0x25]==1500 && hw.writes==6);
    device.onUsb(true,10);
    bench::Frame hello;
    hello.kind=bench::Kind::Hello;hello.sequence=41;hello.length=4;
    bench::write32(hello.payload,0x98765432);
    auto reply=hw.send(device,hello,11);
    assert(reply.kind==bench::Kind::Identity && reply.length==16 &&
           reply.payload[6]==2 && reply.payload[7]==0x18 && reply.payload[4]==0);
    bench::Frame configure;
    configure.kind=bench::Kind::Pins;configure.sequence=42;configure.length=4;
    configure.payload[0]=0x24;configure.payload[1]=0x1a;
    configure.payload[2]=0x00;configure.payload[3]=0x1b;
    auto bad=configure;bad.payload[1]=0x18;
    reply=hw.send(device,bad,12);
    assert(reply.kind==bench::Kind::PinStatus && reply.payload[0]==2 &&
           !device.boardForTest().mapped());
    configure.sequence=43;
    reply=hw.send(device,configure,13);
    assert(reply.payload[0]==0 && device.boardForTest().pin(0)==0x24);
    hw.gate=true;device.tick(14);
    hello.sequence=44;reply=hw.send(device,hello,15);
    assert(reply.payload[4]==1);
    bench::Frame query;
    query.kind=bench::Kind::Pins;query.sequence=45;
    reply=hw.send(device,query,16);
    assert(reply.payload[0]==0 && reply.payload[1]==0x24);
    bench::Frame pulse;
    pulse.kind=bench::Kind::Pulse;pulse.sequence=46;pulse.length=5;
    pulse.payload[0]=0;bench::write16(pulse.payload+1,200);
    bench::write16(pulse.payload+3,150);
    reply=hw.send(device,pulse,17);
    assert(reply.kind==bench::Kind::Ack && reply.payload[1]==0 &&
           hw.outputs[0x24]>1500 && hw.outputs[0x00]==1500);
    device.tick(166);assert(hw.outputs[0x24]>1500);
    device.tick(167);assert(hw.outputs[0x24]==1500);
    pulse.sequence=47;reply=hw.send(device,pulse,168);
    assert(reply.payload[1]==3); // board-local 500 ms neutral dwell
    device.tick(818);
    pulse.sequence=48;reply=hw.send(device,pulse,819);
    assert(reply.payload[1]==4); // out-of-order after denied seq47
    pulse.sequence=47;reply=hw.send(device,pulse,820);
    assert(reply.payload[1]==0 && hw.outputs[0x24]>1500);
    hw.gate=false;device.tick(821);
    assert(hw.outputs[0x24]==1500 && !device.boardForTest().identified());
    device.onUsb(false,822);
    assert(!device.boardForTest().mapped());
    auto invalid=caps;invalid.allowed[1]=invalid.gatePin;
    stm32::Port broken(hw,invalid);
    assert(!broken.ready());
    std::puts("PASS STM32 port contract: serialized vendor bytes, GPIO allowlist, timed PWM and detach neutral");
}
}
int main() {run();}
