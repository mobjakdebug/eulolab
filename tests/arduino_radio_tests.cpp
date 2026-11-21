// Pure packet tests. These are NOT radio/MCU/antenna/region validation.
#include "firmware/arduino/EuloRecorder/radio_packet.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

int main() {
    using namespace eulo_radio;
    Sample measured;
    measured.sequence=0x01020304;
    measured.timeMs=123456;
    measured.pressurePa=98675.4;
    measured.temperatureC=-12.35;
    measured.gyroDegS[0]=35.125;
    measured.gyroDegS[1]=-120.5;
    measured.gyroDegS[2]=0;
    measured.accelMs2[0]=1.234;
    measured.accelMs2[1]=-9.81;
    measured.accelMs2[2]=12.42;
    uint8_t bytes[packetSize]={};
    assert(encode(measured,bytes,sizeof(bytes)));
    assert(bytes[0]=='E' && bytes[1]=='R' && bytes[2]==1 && bytes[3]==0);
    assert(bytes[4]==4 && bytes[5]==3 && bytes[6]==2 && bytes[7]==1);
    assert(get16(bytes+42)==crc16(bytes,42));
    Sample received;
    assert(decode(bytes,sizeof(bytes),received));
    assert(received.sequence==measured.sequence && received.timeMs==measured.timeMs);
    assert(received.pressurePa==98675 &&
           std::fabs(received.temperatureC+12.35)<1e-8 &&
           std::fabs(received.gyroDegS[1]+120.5)<1e-8 &&
           std::fabs(received.accelMs2[1]+9.81)<1e-8);
    auto wrong=measured;
    wrong.pressurePa=std::numeric_limits<double>::quiet_NaN();
    assert(!encode(wrong,bytes,sizeof(bytes)) &&
           !encode(measured,bytes,sizeof(bytes)-1));
    wrong=measured;wrong.accelMs2[0]=201;
    assert(!encode(wrong,bytes,sizeof(bytes)));
    assert(!decode(bytes,sizeof(bytes)-1,received));
    bytes[12]^=0x10;
    assert(!decode(bytes,sizeof(bytes),received) && received.sequence==measured.sequence);
    bytes[12]^=0x10;
    bytes[2]=2;put16(bytes+42,crc16(bytes,42));
    assert(!decode(bytes,sizeof(bytes),received));
    std::puts("PASS Arduino radio packet: measured-field encoding, endian, CRC, bounds, version; no RF test");
}
