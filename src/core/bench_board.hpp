#pragma once
#include "bench_wire.hpp"
#include <cstdint>

namespace eulo::bench {
// Board-side policy. The HAL/USB adapter applies requested PWM outside this
// state machine. Only firmware-provisioned pins may be assigned from the app.
class Board {
public:
    explicit Board(const Capabilities& capabilities={}):capabilities_(capabilities){}
    void tick(uint32_t nowMs,bool usbMounted,bool gateClosed);
    bool receive(const Frame& request,uint32_t nowMs,bool usbMounted,
                 bool gateClosed,Frame& response);
    int angle(uint8_t axis) const {return active_ && axis==axis_?angleCdeg_:0;}
    int angleOnPin(uint8_t pin) const;
    uint8_t pin(uint8_t axis) const {return mapped_ && axis<4?pins_[axis]:unsetPin;}
    bool active() const {return active_;}
    bool identified() const {return session_;}
    bool mapped() const {return mapped_;}
    const Capabilities& capabilities() const {return capabilities_;}
private:
    Capabilities capabilities_;
    bool session_=false,active_=false,cooldown_=false,mapped_=false;
    bool gateWasClosed_=false;
    uint8_t pins_[4]={unsetPin,unsetPin,unsetPin,unsetPin};
    uint8_t axis_=0;
    int angleCdeg_=0;
    uint16_t lastSequence_=0;
    uint32_t deadlineMs_=0,nextAllowedMs_=0;
    void clearPulse(){active_=false;angleCdeg_=0;}
    bool validPins(const uint8_t* pins) const;
};
} // namespace eulo::bench
