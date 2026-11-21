// EuloLab Arduino ground-data recorder: ESP32 or STM32 (STM32duino).
// No bench PWM, servo, pyro, or flight-control outputs. No GPS.
// The default hardware_config.h fails closed: no sensor reads or radio traffic.
// This is a candidate for IDENTIFIED BMP388/390 + LSM6DSOX + optional SX127x
// modules only, not a universal driver for unidentified hardware.
#include <Arduino.h>
#include <Wire.h>
#include <cmath>
#include "hardware_config.h"
#include "radio_packet.hpp"

#if !defined(ARDUINO_ARCH_ESP32) && !defined(ARDUINO_ARCH_STM32)
#error Select a reviewed Arduino-ESP32 or STM32duino board variant.
#endif
#if EULO_RADIO_MODE < 0 || EULO_RADIO_MODE > 2
#error EULO_RADIO_MODE must be 0 (serial), 1 (TX) or 2 (RX).
#endif
#if EULO_RADIO_MODE == 1 && EULO_RADIO_INTERVAL_MS < 60000UL
#error Radio updates are intentionally sparse; review regional duty-cycle rules.
#endif
#if EULO_RADIO_MODE != 2
#include <Adafruit_BMP3XX.h>
#include <Adafruit_LSM6DSOX.h>
Adafruit_BMP3XX pressureSensor;
Adafruit_LSM6DSOX inertialSensor;
#endif
#if EULO_RADIO_MODE != 0
#include <SPI.h>
#include <LoRa.h>
#endif

namespace {
constexpr const char* csvHeader=
    "time_s,pressure_pa,temperature_c,gx_deg_s,gy_deg_s,gz_deg_s,ax_m_s2,ay_m_s2,az_m_s2";
constexpr uint32_t samplePeriodMs=50; // 20 Hz serial; NOT a flight-control rate
constexpr uint32_t maxElapsedMs=3600000;
constexpr uint32_t maxSerialSamples=12000; // bounds CSV size (<2 MiB)
constexpr double degreesPerRadian=57.29577951308232;
bool ready=false;
uint32_t beginMs=0,lastSampleMs=0,rows=0;
#if EULO_RADIO_MODE == 1
uint32_t lastRadioMs=0,radioSequence=0;
#endif
#if EULO_RADIO_MODE == 2
uint32_t firstRemoteMs=0,lastRemoteMs=0,lastSequence=0;
bool receivedFirst=false;
#endif
void announce() {
#if EULO_RADIO_MODE == 2
    EULO_LOG_PORT.println("# EULOLAB_RADIO_RECEIVER/1");
#else
    EULO_LOG_PORT.println("# EULOLAB_RECORDER/1");
#endif
    EULO_LOG_PORT.println(csvHeader);
}
void serialHandshake() {
    while(EULO_LOG_PORT.available())
        if(EULO_LOG_PORT.read()=='H')announce(); // read-only; never arms hardware
}

void fail(const char* reason) {
    ready=false;
    EULO_LOG_PORT.print("# EULOLAB_ERROR ");
    EULO_LOG_PORT.println(reason); // never substitute a synthetic sensor value
}
void emit(const eulo_radio::Sample& s) {
    EULO_LOG_PORT.print(s.timeMs*.001,3);
    EULO_LOG_PORT.print(',');EULO_LOG_PORT.print(s.pressurePa,1);
    EULO_LOG_PORT.print(',');EULO_LOG_PORT.print(s.temperatureC,2);
    for(int axis=0;axis<3;++axis) {
        EULO_LOG_PORT.print(',');EULO_LOG_PORT.print(s.gyroDegS[axis],3);
    }
    for(int axis=0;axis<3;++axis) {
        EULO_LOG_PORT.print(',');EULO_LOG_PORT.print(s.accelMs2[axis],3);
    }
    EULO_LOG_PORT.println();
}
#if EULO_RADIO_MODE != 0
bool radioConfigValid() {
    // These are only sanity checks. The integrator must review the exact
    // board pinout, radio module voltage, antenna, band and EIRP.
    return EULO_RADIO_SS>=0 && EULO_RADIO_RESET>=0 && EULO_RADIO_DIO0>=0 &&
           EULO_RADIO_SS!=EULO_RADIO_RESET &&
           EULO_RADIO_SS!=EULO_RADIO_DIO0 &&
           EULO_RADIO_RESET!=EULO_RADIO_DIO0 &&
           EULO_RADIO_FREQUENCY_HZ>=400000000L &&
           EULO_RADIO_FREQUENCY_HZ<=930000000L;
}
bool beginRadio() {
    if(!radioConfigValid())return false;
    SPI.begin(); // use only a variant whose default SPI pins were reviewed
    LoRa.setPins(EULO_RADIO_SS,EULO_RADIO_RESET,EULO_RADIO_DIO0);
    if(!LoRa.begin(EULO_RADIO_FREQUENCY_HZ))return false;
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125000L);
    LoRa.setCodingRate4(5);
    LoRa.enableCrc();
    return true;
}
#endif
#if EULO_RADIO_MODE != 2
bool sensorsConfigValid() {
    return EULO_WIRE_PINS_REVIEWED==1 &&
        (EULO_PRESSURE_ADDRESS==0x76 || EULO_PRESSURE_ADDRESS==0x77) &&
        (EULO_IMU_ADDRESS==0x6a || EULO_IMU_ADDRESS==0x6b) &&
        EULO_PRESSURE_ADDRESS!=EULO_IMU_ADDRESS;
}
bool sensorPresent(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission()==0;
}
bool sampleSensors(uint32_t elapsed,eulo_radio::Sample& s) {
    // Some IMU libraries do not propagate I2C read failure from getEvent();
    // re-probe both devices per sample and stop entirely on disconnection.
    if(!sensorPresent(EULO_PRESSURE_ADDRESS) ||
       !sensorPresent(EULO_IMU_ADDRESS) || !pressureSensor.performReading())
        return false;
    sensors_event_t accel,gyro,temp;
    if(!inertialSensor.getEvent(&accel,&gyro,&temp))return false;
    s.timeMs=elapsed;
    s.pressurePa=pressureSensor.pressure; // library returns Pa, not hPa
    s.temperatureC=pressureSensor.temperature;
    const double gyroRadS[]={gyro.gyro.x,gyro.gyro.y,gyro.gyro.z};
    const double acceleration[]={accel.acceleration.x,accel.acceleration.y,
                                 accel.acceleration.z};
    for(int i=0;i<3;++i) {
        s.gyroDegS[i]=gyroRadS[i]*degreesPerRadian;
        s.accelMs2[i]=acceleration[i];
    }
    return eulo_radio::valid(s); // rejects NaN, saturation and out-of-range
}
#endif
#if EULO_RADIO_MODE == 1
void transmit(const eulo_radio::Sample& measured,uint32_t now) {
    if(static_cast<uint32_t>(now-lastRadioMs)<EULO_RADIO_INTERVAL_MS)return;
    lastRadioMs=now; // no retries or flood on busy/error
    eulo_radio::Sample packet=measured;
    packet.sequence=radioSequence++;
    uint8_t bytes[eulo_radio::packetSize];
    if(!eulo_radio::encode(packet,bytes,sizeof(bytes)) ||
       LoRa.beginPacket()!=1 ||
       LoRa.write(bytes,sizeof(bytes))!=sizeof(bytes) ||
       LoRa.endPacket()!=1) {
        fail("radio transmission failed; no further samples sent");
    }
}
#endif
#if EULO_RADIO_MODE == 2
void receivePacket() {
    const int length=LoRa.parsePacket();
    if(!length)return;
    uint8_t bytes[eulo_radio::packetSize];
    if(length!=static_cast<int>(sizeof(bytes))) {
        while(LoRa.available())LoRa.read();
        EULO_LOG_PORT.println("# EULOLAB_RADIO_REJECT size");return;
    }
    for(size_t i=0;i<sizeof(bytes);++i) {
        const int value=LoRa.read();
        if(value<0) {
            EULO_LOG_PORT.println("# EULOLAB_RADIO_REJECT short_read");return;
        }
        bytes[i]=static_cast<uint8_t>(value);
    }
    eulo_radio::Sample s;
    if(!eulo_radio::decode(bytes,sizeof(bytes),s)) {
        EULO_LOG_PORT.println("# EULOLAB_RADIO_REJECT crc_or_data");return;
    }
    if(receivedFirst && (s.sequence<=lastSequence || s.timeMs<=lastRemoteMs)) {
        EULO_LOG_PORT.println("# EULOLAB_RADIO_REJECT order_or_restart");return;
    }
    if(receivedFirst && s.sequence-lastSequence!=1)
        EULO_LOG_PORT.println("# EULOLAB_RADIO_GAP missing_packets");
    if(!receivedFirst) {firstRemoteMs=s.timeMs;receivedFirst=true;}
    lastSequence=s.sequence;lastRemoteMs=s.timeMs;
    // Normalize sender uptime; only measured fields travel over the radio.
    s.timeMs-=firstRemoteMs;
    emit(s);
    // Link-quality measurements are diagnostics, not sensor fields or position.
    EULO_LOG_PORT.print("# EULOLAB_RADIO_LINK rssi_dbm=");
    EULO_LOG_PORT.print(LoRa.packetRssi());
    EULO_LOG_PORT.print(" snr_db=");EULO_LOG_PORT.println(LoRa.packetSnr(),1);
}
#endif
} // namespace

void setup() {
    EULO_LOG_PORT.begin(115200);
    delay(250); // do not block forever waiting for a serial terminal
    if(EULO_BOARD_REVIEWED!=1) {
        fail("hardware not identified; edit hardware_config.h after review");
        return;
    }
#if EULO_RADIO_MODE == 2
    if(!beginRadio()) {fail("SX127x receiver not detected/configured");return;}
#else
    if(!sensorsConfigValid()) {fail("I2C pinout/address not reviewed");return;}
    Wire.begin();
    Wire.setClock(100000);
    if(!pressureSensor.begin_I2C(EULO_PRESSURE_ADDRESS,&Wire) ||
       !inertialSensor.begin_I2C(EULO_IMU_ADDRESS,&Wire)) {
        fail("BMP3xx or LSM6DSOX probe failed; check actual parts/wiring");return;
    }
    pressureSensor.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    pressureSensor.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    pressureSensor.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
    pressureSensor.setOutputDataRate(BMP3_ODR_50_HZ);
    inertialSensor.setGyroRange(LSM6DS_GYRO_RANGE_2000_DPS);
    inertialSensor.setAccelRange(LSM6DS_ACCEL_RANGE_16_G);
    inertialSensor.setGyroDataRate(LSM6DS_RATE_104_HZ);
    inertialSensor.setAccelDataRate(LSM6DS_RATE_104_HZ);
#if EULO_RADIO_MODE == 1
    if(EULO_ALLOW_RF_TX!=1 || EULO_RADIO_TX_POWER_DBM<2 ||
       EULO_RADIO_TX_POWER_DBM>14 || !beginRadio()) {
        fail("RF TX not authorized or SX127x not configured");return;
    }
    LoRa.setTxPower(EULO_RADIO_TX_POWER_DBM);
#endif
    beginMs=millis();lastSampleMs=beginMs;
#endif
    announce();
    ready=true;
}

void loop() {
    if(!ready) {delay(250);return;}
    serialHandshake();
#if EULO_RADIO_MODE == 2
    receivePacket();
#else
    const uint32_t now=millis();
    if(static_cast<uint32_t>(now-lastSampleMs)<samplePeriodMs)return;
    lastSampleMs=now;
    const uint32_t elapsed=now-beginMs;
    if(elapsed>maxElapsedMs || rows>=maxSerialSamples) {
        ready=false;
        EULO_LOG_PORT.println("# EULOLAB_STOP time_or_size_limit");return;
    }
    eulo_radio::Sample measured;
    if(!sampleSensors(elapsed,measured)) {
        fail("sensor read failed or value out of range; no substitute sample");return;
    }
    emit(measured);++rows;
#if EULO_RADIO_MODE == 1
    if(rows==1)lastRadioMs=now-EULO_RADIO_INTERVAL_MS; // first real sample only
    transmit(measured,now);
#endif
#endif
}
