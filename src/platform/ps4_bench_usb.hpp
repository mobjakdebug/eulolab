#pragma once
#include "core/bench_link.hpp"
#include <orbis/Usbd.h>

namespace eulo {
// This adapter is compiled for Orbis only; it never probes unconfigured IDs.
class OrbisBenchUsb final : public BenchBytes {
public:
    OrbisBenchUsb(uint16_t vendor,uint16_t product):vendor_(vendor),product_(product){}
    ~OrbisBenchUsb() override {close();}
    bool configured() const {return vendor_!=0 && product_!=0;}
    uint16_t vendor() const {return vendor_;}
    uint16_t product() const {return product_;}
    bool open(std::string& error) override;
    int write(const uint8_t* bytes,std::size_t length,std::string& error) override;
    int read(uint8_t* bytes,std::size_t capacity,unsigned timeoutMs,
             std::string& error) override;
    void close() override;
private:
    uint16_t vendor_,product_;
    libusb_device_handle* device_=nullptr;
    int claimed_=-1;
    uint8_t in_=0,out_=0;
};
} // namespace eulo
