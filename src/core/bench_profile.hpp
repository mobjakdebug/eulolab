#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace eulo::bench {
// A local, device-specific configuration. It is never stored inside a rocket
// project and never interpreted as permission to drive an arbitrary GPIO.
// Pins are identifiers advertised by EuloBench/2 firmware, not PS4 GPIOs.
enum class Family : uint8_t { Any=0, Esp32=1, Stm32=2, Other=3 };
struct Profile {
    Family family=Family::Any;
    uint16_t vendor=0,product=0; // zero means USB discovery is disabled
    uint8_t interfaceNumber=0;
    uint8_t bulkOut=0,bulkIn=0; // zero: detect from a verified descriptor
    std::array<uint8_t,4> pins{{0xff,0xff,0xff,0xff}};
    bool validTransport(std::string& error) const; // identify without guessing pins
    bool validDraft(std::string& error) const; // save IDs before inspecting board pool
    bool valid(std::string& error) const; // all four pins ready for mapping
};
// Atomic, strictly versioned profile file; no board identity is inferred from
// the console build. A missing/invalid file leaves the application disabled.
bool loadProfile(Profile& profile,const char* path,std::string& error);
bool saveProfile(const Profile& profile,const char* path,std::string& error);
const char* familyName(Family family);
std::string pinName(Family family,uint8_t pin);
} // namespace eulo::bench
