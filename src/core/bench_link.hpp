#pragma once
#include "bench_profile.hpp"
#include "bench_wire.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace eulo {
// A transport is closed until the user explicitly opens the bench panel.
// Return byte counts, 0 for read timeout, -1 on IO error.
class BenchBytes {
public:
    virtual ~BenchBytes()=default;
    virtual bool open(std::string& error)=0;
    virtual int write(const uint8_t* bytes,std::size_t length,std::string& error)=0;
    virtual int read(uint8_t* bytes,std::size_t capacity,unsigned timeoutMs,
                     std::string& error)=0;
    virtual void close()=0;
};
class BenchControl {
public:
    virtual ~BenchControl()=default;
    virtual bool connect(std::string& error)=0;
    virtual bool applyPins(std::string& error)=0;
    virtual bool pulse(uint8_t axis,int angleCdeg,uint16_t durationMs,
                       std::string& error)=0;
    virtual bool neutral(std::string& error)=0;
    virtual void close()=0;
    virtual bool connected() const=0;
    virtual bool physicalGate() const=0;
    virtual bool pinsMatched() const=0;
    virtual bench::Capabilities capabilities() const=0;
};
class BenchLink final : public BenchControl {
public:
    BenchLink(BenchBytes& bytes,const bench::Profile& profile,uint32_t nonce):
        bytes_(bytes),profile_(profile),nonce_(nonce){}
    ~BenchLink() override {close();}
    bool connect(std::string& error) override;
    bool applyPins(std::string& error) override;
    bool pulse(uint8_t axis,int angleCdeg,uint16_t durationMs,
               std::string& error) override;
    bool neutral(std::string& error) override;
    void close() override;
    bool connected() const override {return connected_;}
    bool physicalGate() const override {return physicalGate_;}
    bool pinsMatched() const override {return pinsMatched_;}
    bench::Capabilities capabilities() const override {return capabilities_;}
private:
    BenchBytes& bytes_;
    const bench::Profile& profile_;
    uint32_t nonce_;
    uint16_t sequence_=0;
    bool connected_=false,physicalGate_=false,opened_=false,pinsMatched_=false;
    bench::Capabilities capabilities_;
    bench::Decoder decoder_;
    bool exchange(const bench::Frame& request,bench::Kind expected,
                  bench::Frame& response,std::string& error);
};
} // namespace eulo
