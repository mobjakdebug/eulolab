#pragma once
#include "core/bench_board.hpp"
#include <cstddef>
#include <cstdint>

namespace eulo::stm32 {
// STM32 PA0..PH15 are encoded as (port A=0 .. H=7) << 4 | pin 0..15.
// A board's HAL glue must provision gate + 4..8 safe timer/PWM-capable pins,
// native USB vendor/bulk endpoints, independent servo power and a watchdog.
// Callbacks below are TASK-context, serialized, not concurrent USB IRQ code.
class Hardware {
public:
    virtual ~Hardware()=default;
    virtual bool gateClosed() const=0; // physical input, never writable over USB
    virtual void setPwmMicroseconds(uint8_t pin,uint16_t width)=0;
    virtual bool writeBulkIn(const uint8_t* frame,std::size_t length)=0;
};
class Port {
public:
    Port(Hardware& hardware,const bench::Capabilities& capabilities);
    bool ready() const {return ready_;}
    // Call whenever USB mount state changes; detach invalidates the mapping.
    void onUsb(bool mounted,uint32_t nowMs);
    // Queue USB OUT bytes from IRQ; drain them into this method in task context.
    void onBulkOut(const uint8_t* bytes,std::size_t count,uint32_t nowMs);
    // Call at least every ~10 ms even if USB traffic stops. This cannot replace
    // an external servo-power cut-off or a hardware watchdog on the MCU.
    void tick(uint32_t nowMs);
    const bench::Board& boardForTest() const {return board_;}
private:
    Hardware& hardware_;
    bench::Capabilities capabilities_;
    bench::Board board_;
    bench::Decoder decoder_;
    bool mounted_=false,ready_=false;
    int applied_[8];
    void applyOutputs();
};
} // namespace eulo::stm32
