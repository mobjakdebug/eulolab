# EuloBench/1: manual USB actuator bench

This is a **bench-only**, opt-in protocol between the PS4 workbench and a dedicated microcontroller. It cannot be called from the virtual flight solver. There are no arming, propulsion, ignition, radio, navigation, or flight-control messages. The Ground page remains **recorded CSV only**; an ACK is not gyro, altitude or position telemetry. No PS4 console/USB device or servo has been exercised in this repository's tests.

## Scope and prerequisites

The Orbis adapter is implemented in `src/platform/ps4_bench_usb.cpp`. By default both USB IDs are **zero**, so it cannot open any device. It searches for exactly one configured VID:PID with one configuration, one vendor-specific interface (`class FF, subclass 45, protocol 55`, alternate setting 0), two **bulk** endpoints, and matching active configuration. It reads raw configuration bytes because OpenOrbis v0.5.4's bundled `libusb_endpoint_descriptor` is incomplete. It does not detach a kernel driver, probe generic serial adapters, configure CDC ACM or use a guessed ESP32/STM32 ID. A nonce/version exchange must succeed before any pulse. A second matching board is an error.

The reference board source is `firmware/esp32s3/` (ESP-IDF >=5.3, `esp_tinyusb` 3.0.1). It uses the **ESP32-S3 native USB OTG** peripheral and four configurable LEDC PWM pins; an ordinary ESP32 without native USB, a USB-to-UART adapter, STM32 USB CDC, and unrelated firmware do **not** speak this interface. The board project deliberately does not build until you configure *your board's authorized VID/PID* and five actual GPIOs. Do not borrow another manufacturer's USB IDs. You must inspect the real enumerated USB descriptor and pinout; no hardware identity has been provided to this project.

### Before connecting a servo

1. Use an empty, restrained desktop rig. **No motor, propellant, igniter, launch structure, or flight vehicle.** Disconnect the servo horns/linkages for the first electrical check.
2. Provide a correctly rated, separately powered servo supply with common signal ground and a **physical, independent power cut-off**. The GPIO bench-enable input is a low-by-default software interlock, **not** that cut-off. Never power motors/servos from a PS4 USB port or a microcontroller signal pin.
3. Check board voltage, PWM polarity, actual pin mapping, neutral width and mechanical end stops with measurement equipment; the 1,500 µs center and 90°/1 ms scale are examples, not calibration. Monitor supply current. Close the enable jumper only after these checks.
4. Test on the board and host workstation first. PS4 compatibility, USB permissions, USB clocking and bus-error codes require testing on the target console/firmware. Have someone able to remove servo power independently of this software.

These are risk reductions, **not a safety certification**. USB loss, a crashed MCU, a damaged output pin, or an incorrectly wired supply can defeat software neutral. Never use this firmware in flight.

## Wire format

All multibyte values are **little-endian**. Bulk OUT and IN carry a byte stream; transfers need not align to frames. No newline delimiter or USB control request carries an actuator command.

| Offset | Bytes | Field |
| --- | ---: | --- |
| 0 | 2 | ASCII `EU` |
| 2 | 1 | version `01` |
| 3 | 1 | message kind |
| 4 | 2 | sequence (uint16, wraps) |
| 6 | 1 | payload length, `0..16` |
| 7 | 0–16 | payload |
| final | 2 | CRC-16/CCITT-FALSE, init `FFFF`, polynomial `1021`, over the header and payload (test vector `123456789` → `29B1`) |

| Kind | Direction | Payload / response |
| --- | --- | --- |
| `01` HELLO | PS4 → board | 4-byte session nonce (stale-reply check, **not authentication**) |
| `81` IDENTITY | board → PS4 | Same nonce (4), enable jumper state (bit 0), protocol version `01` |
| `02` PULSE | PS4 → board | axis (0 pitch, 1 yaw, 2 fin A, 3 fin B), signed centidegrees `-300..-1` or `1..300`, duration `50..300` ms |
| `03` NEUTRAL | PS4 → board | Empty; command all four outputs to neutral |
| `82` ACK | board → PS4 | original kind, status, accepted angle (signed centidegrees, zero for neutral/rejection) |

ACK status `0` means accepted by the **board protocol**, not measured travel; `1` means gate open; `2` invalid data/bounds; `3` no session, busy, or 500 ms neutral dwell; `4` stale/out-of-order sequence. The board accepts one pulse at a time, automatically returns it to neutral at its own deadline, and requires **500 ms neutral dwell** after every accepted pulse. Opening the gate, USB detach/suspend or the board task's watchdog also neutralises and invalidates the session. The host never retries a pulse: on missing/mismatched ACK it requests neutral, closes USB, and requires another identification. The board task checks state every ~10 ms, not to a certified real-time tolerance; its 2 s task watchdog only resets software on a stuck task and is **not** a substitute for the independent power cut-off. CRC detects accidental corruption, not hostile commands.

The UI sends a fixed **±2° / 150 ms** command for the selected axis; it asks for separate, default-cancel confirmation **each time**. The protocol and board both reject commands outside their harder bounds. Neither flight settings nor simulation playback can enqueue a bench command.

## Configure and build

For a board whose USB ID and GPIO assignments you have verified:

```bash
cd firmware/esp32s3
idf.py set-target esp32s3
idf.py menuconfig   # EuloBench USB / bench-only servo wiring
idf.py build        # no build with unset IDs/pins; do not flash unreviewed hardware
```

After checking firmware and actual USB descriptor, pass the **same authorized** numeric VID and PID to the PS4 build. `BENCH_VID` and `BENCH_PID` default to zero; the generated `out/ps4/bench_config.hpp` forces an eboot rebuild if either changes. Example with values already set for your actual board:

```bash
make ps4 BENCH_VID="$BOARD_VID" BENCH_PID="$BOARD_PID"
```

On the console, **View → Servo bench (USB)** or the Bench toolbar button enters the isolated manual screen. Identify the board; check the reported jumper state; select one channel; choose a ±2° pulse; explicitly select **Send one pulse** in the confirmation dialog. **Neutral now** is visible in the modal, and `Square` (controller) / `N` (host keyboard) requests neutral even over that modal. Leaving the bench, controller loss, or an ACK failure closes the link. The host browser preview has **no USB bridge**; its Bench page exercises only the UI, not an actuator. No USB sensor stream or LoRa support is included.

For a different ESP32/STM32, implement this exact USB interface and board-local state machine on that target, then verify its descriptor, endpoints, neutral behavior and gate on physical hardware. A generic USB-serial driver is not equivalent. Share the actual board model, descriptor dump, firmware and servo wiring to assess compatibility with the console implementation.

## Verification boundary

`make -j2 all test` checks wire CRC/framing, malformed descriptors, byte-fragmented host ↔ board loopback, false nonce, wrong version, disabled gate, out-of-bounds pulses, missing ACK, sequence replay, dwell, USB loss, timeout and UI confirmation using fakes. `g++ -fsyntax-only` checks the Orbis USB adapter against the OpenOrbis v0.5.4 header. When CI passes **on this revision**, it also cross-builds the PS4 adapter and compiles the ESP32-S3 source with **reserved, non-enumerable test IDs and compile-only pins**. It does not flash or publish that firmware. None of this proves PS4 USB behavior, board enumeration, servo motion or electrical safety. A build and instrumented neutral/PWM/console test with the specific real board and an independently powered rig remain necessary.
