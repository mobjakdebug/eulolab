#pragma once
#include "bench_wire.hpp"
#include <cstdint>

namespace eulo::bench {
// Board-side policy. It changes only requested output, never reads a servo.
// GPIO/PWM and USB callbacks run outside this state machine.
class Board {
public:
    void tick(uint32_t nowMs,bool usbMounted,bool gateClosed);
    bool receive(const Frame& request,uint32_t nowMs,bool usbMounted,
                 bool gateClosed,Frame& response);
    int angle(uint8_t axis) const {return active_ && axis==axis_?angleCdeg_:0;}
    bool active() const {return active_;}
    bool identified() const {return session_;}
private:
    bool session_=false,active_=false,cooldown_=false;
    uint8_t axis_=0;
    int angleCdeg_=0;
    uint16_t lastSequence_=0;
    uint32_t deadlineMs_=0,nextAllowedMs_=0;
    void clearPulse(){active_=false;angleCdeg_=0;}
};
} // namespace eulo::bench
