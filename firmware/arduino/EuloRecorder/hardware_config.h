#pragma once
// Deliberately unprovisioned. This sketch emits NO samples and NO RF until
// the owner identifies the exact board, sensor modules, wiring, supply and
// legal radio region. Edit this file locally after reviewing README.md. NEVER
// flash the CI compile-only configuration to an unidentified real board.
// Both Arduino-ESP32 and STM32duino use this same .ino file.
#ifndef EULO_BOARD_REVIEWED
#define EULO_BOARD_REVIEWED 0
#endif
// Verify the variant's default Wire SDA/SCL pins and voltage levels. Wire.begin()
// does not mean that unknown wiring is correct.
#ifndef EULO_WIRE_PINS_REVIEWED
#define EULO_WIRE_PINS_REVIEWED 0
#endif
#ifndef EULO_PRESSURE_ADDRESS
#define EULO_PRESSURE_ADDRESS 0 // choose observed BMP388/390 I2C address 0x76/0x77
#endif
#ifndef EULO_IMU_ADDRESS
#define EULO_IMU_ADDRESS 0 // choose observed LSM6DSOX I2C address 0x6A/0x6B
#endif
// Serial may be UART or a USB VCP depending on board/core menu settings.
// Change to SerialUSB ONLY when the particular STM32 variant provides it.
#ifndef EULO_LOG_PORT
#define EULO_LOG_PORT Serial
#endif
// 0: serial sensor recorder, 1: sensor recorder + SX127x TX,
// 2: SX127x receiver forwarding accepted radio packets to serial.
#ifndef EULO_RADIO_MODE
#define EULO_RADIO_MODE 0
#endif
// Separate deliberate RF transmit authorization. This is NOT regulatory
// authorization; check your local band, EIRP and duty-cycle before using RF.
#ifndef EULO_ALLOW_RF_TX
#define EULO_ALLOW_RF_TX 0
#endif
#ifndef EULO_RADIO_FREQUENCY_HZ
#define EULO_RADIO_FREQUENCY_HZ 0L // never choose a frequency automatically
#endif
#ifndef EULO_RADIO_SS
#define EULO_RADIO_SS -1
#endif
#ifndef EULO_RADIO_RESET
#define EULO_RADIO_RESET -1
#endif
#ifndef EULO_RADIO_DIO0
#define EULO_RADIO_DIO0 -1
#endif
#ifndef EULO_RADIO_TX_POWER_DBM
#define EULO_RADIO_TX_POWER_DBM 0 // review RF module, antenna and regulatory limit
#endif
#ifndef EULO_RADIO_INTERVAL_MS
#define EULO_RADIO_INTERVAL_MS 60000UL // minimum; only sparse status, NOT live attitude
#endif
