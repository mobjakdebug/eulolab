#include "bench_port.hpp"
#include <climits>

namespace eulo::stm32 {
namespace {
bool validate(const bench::Capabilities& caps) {
    if(caps.family!=2 || caps.gatePin>0x7f)return false;
    unsigned count=0;
    for(int i=0;i<8;++i) {
        const uint8_t pin=caps.allowed[i];
        if(pin==bench::unsetPin)continue;
        if(pin>0x7f || pin==caps.gatePin)return false;
        for(int j=0;j<i;++j)if(caps.allowed[j]==pin)return false;
        ++count;
    }
    return count>=4;
}
}
Port::Port(Hardware& hardware,const bench::Capabilities& capabilities):
    hardware_(hardware),capabilities_(capabilities),board_(capabilities),
    ready_(validate(capabilities)) {
    for(int& applied:applied_)applied=INT_MAX; // first tick forces 1500 us
}
void Port::applyOutputs() {
    if(!ready_)return;
    for(int i=0;i<8;++i) {
        const uint8_t pin=capabilities_.allowed[i];
        if(pin==bench::unsetPin)continue;
        const int angle=board_.angleOnPin(pin);
        if(applied_[i]==angle)continue;
        // Example only: 90 deg per 1 ms, neutral 1500 us. Not feedback or
        // per-servo calibration. The shared Board bounds angle to +/-3 deg.
        const auto width=static_cast<uint16_t>(1500+angle*1000/9000);
        hardware_.setPwmMicroseconds(pin,width);
        applied_[i]=angle;
    }
}
void Port::onUsb(bool mounted,uint32_t nowMs) {
    mounted_=mounted;
    board_.tick(nowMs,mounted_,hardware_.gateClosed());
    if(!mounted_)decoder_.reset();
    applyOutputs();
}
void Port::onBulkOut(const uint8_t* bytes,std::size_t count,uint32_t nowMs) {
    if(!mounted_ || !ready_ || !bytes || count>64)return;
    for(std::size_t i=0;i<count;++i) {
        bench::Frame request,response;
        if(!decoder_.push(bytes[i],request))continue;
        const bool reply=board_.receive(request,nowMs,true,hardware_.gateClosed(),response);
        // Apply locally even if the IN endpoint is unavailable. No host ACK
        // is needed to meet the board-side neutral deadline.
        applyOutputs();
        if(reply) {
            uint8_t frame[bench::maxFrame];
            const std::size_t length=bench::encode(response,frame);
            if(length)hardware_.writeBulkIn(frame,length);
        }
    }
}
void Port::tick(uint32_t nowMs) {
    board_.tick(nowMs,mounted_,hardware_.gateClosed());
    applyOutputs();
}
} // namespace eulo::stm32
