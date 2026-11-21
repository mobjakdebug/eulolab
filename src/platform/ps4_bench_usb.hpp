#pragma once
#include "core/bench_link.hpp"
#include <orbis/Usbd.h>

namespace eulo {
// Orbis-only adapter. It reads an app-edited profile, not build-time IDs.
// It only claims a descriptor that implements the EuloBench vendor interface.
class OrbisBenchUsb final : public BenchBytes {
public:
    explicit OrbisBenchUsb(const bench::Profile& profile):profile_(profile){}
    ~OrbisBenchUsb() override {close();}
    bool configured() const {return profile_.vendor!=0 && profile_.product!=0;}
    uint16_t vendor() const {return profile_.vendor;}
    uint16_t product() const {return profile_.product;}
    bool open(std::string& error) override;
    int write(const uint8_t* bytes,std::size_t length,std::string& error) override;
    int read(uint8_t* bytes,std::size_t capacity,unsigned timeoutMs,
             std::string& error) override;
    void close() override;
private:
    const bench::Profile& profile_;
    libusb_device_handle* device_=nullptr;
    int claimed_=-1;
    uint8_t in_=0,out_=0;
};
} // namespace eulo
