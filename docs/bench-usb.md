# EuloBench/2: configurable manual USB servo bench

This protocol links the **PS4 workbench** to a dedicated, compatible microcontroller on an empty bench. It is isolated from the flight solver. There are no arming, ignition, radio, navigation or in-flight control messages. The Ground page still reads **recorded CSV only**; a USB ACK is not gyro/altitude or measured servo travel. EuloBench/2 is **not wire-compatible** with the earlier EuloBench/1 reference.

## What is configurable in the application

Open **Bench → Configure USB and pin map**. The inspector now edits these values **on the PS4, without rebuilding its PKG**:

| Field | Meaning |
| --- | --- |
| Board family | ESP32 GPIO numbers, STM32 `PA0..PH15` encoded as `(port A=0..H=7)<<4 | pin`, or another EuloBench-compatible device |
| USB VID:PID | The actual board's **authorized** nonzero IDs; default `0000:0000` disables USB |
| Interface | The actual EuloBench vendor-interface number (hex, default `00`); this may be part of a composite device |
| Bulk OUT / IN | Actual endpoint addresses (`01..0F` OUT, `81..8F` IN), or `00` for descriptor-checked auto-detection |
| Pitch / yaw / fin A / fin B | Four different device pin IDs, selected from the **read-only allowed pool advertised by that board's firmware**; `FF` means unset |

Select a field and press `×` / Enter to edit its hex digits; on a controller, D-pad left/right selects a digit and up/down changes it. Mouse and keyboard can enter hex directly. `○` / Esc cancels. Save the profile with **Save profile locally**. It lives in `/data/eulolab/bench-profile.cfg` on PS4 (or beside the project in the host test); it is separate from `.eul`, so importing a rocket design cannot import a hardware mapping. A missing or malformed profile is fail-closed, and editing one while connected requests neutral and closes USB. The host preview has **no USB bridge**, though it renders and tests this editor.

These settings are **not a universal driver for arbitrary chips**. The Orbis adapter claims only a selected vendor-specific interface (`class FF, subclass 45, protocol 55`, alt setting 0) with exactly two bulk endpoints, a matching active configuration, one explicitly chosen VID:PID device, and a successful EuloBench/2 handshake. It reads raw configuration bytes because OpenOrbis v0.5.4's `libusb_endpoint_descriptor` declaration is incomplete. It never detaches a kernel driver or guesses serial converters, CDC ACM, STM32 IDs or pin capabilities. A different MCU can participate **only after its board firmware implements this USB descriptor and protocol**. The app cannot rewrite firmware, give a pin PWM capability, change a physical interlock, or reserve a legal USB ID.

### Board implementations

- [`firmware/esp32s3/`](../firmware/esp32s3/) is an ESP-IDF >=5.3 / `esp_tinyusb` 3.0.1 **buildable reference for ESP32-S3 native USB OTG**. Its build-time provisioned, reviewed pool has four required and up to four optional PWM GPIOs; all pool outputs start at neutral. The app chooses which four of these pins correspond to the axes. The board refuses a duplicate, unprovisioned, USB or gate pin. Its gate input is fixed in firmware. VID:PID and this safe pool must first be set for the *actual board* in `idf.py menuconfig`. The CI's `FFFF:FFFF` and example pins are reserved **compile-only fixtures**, not USB identities or wiring to flash.
- [`firmware/stm32/`](../firmware/stm32/README.md) contains a **compiled and host-tested STM32 device-policy adapter** for boards with native USB device and PWM timers. The integrator supplies that exact board's HAL USB/PWM/gate hooks, descriptor, clock tree, pin pool, watchdog and USB IDs; there is **no flashable generic STM32 image** without a model and pinout. Its pin coding and integration requirements are in that README.
- Ordinary ESP32 without native USB, USB-to-UART dongles, existing CDC-only firmware and unrelated STM32 firmware do **not** speak this vendor protocol. A future, separately reviewed transport adapter is required for those devices. No one-size-fits-all GPIO write is safe or possible over USB.

### Before powering a servo

1. Use a restrained, empty desktop rig. **No propellant, motor, igniter, launch structure or flight vehicle.** Disconnect servo horns/linkages during first electrical checks.
2. Use a correctly rated **independent servo power supply** with common signal ground and a **physical, independent power cut-off**. The MCU gate input is a low-by-default software interlock, **not** the cut-off. Never power servos from the PS4 USB port or MCU signal pin.
3. Verify MCU voltage, actual PWM/timer-capable pin pool, polarity, servo neutral width and mechanical stops with measurement equipment. The reference 1,500 µs centre and 90°/1 ms scale are examples, not calibration. Monitor current and keep a person at the power cut-off.
4. Verify descriptors, protocol, PWM and neutral on the board and host workstation before testing the console. PS4 USB permissions and bus/error behaviour have **not** been exercised here. A damaged GPIO, software crash or bad supply can defeat software neutral. This is **not** a safety certification and must not be used in flight.

## Wire contract

All multibyte values are little-endian. Bulk IN/OUT are **byte streams**; a USB transfer may split or combine frames. No control request, newline, CSV stream or flight-mode action carries an actuator command.

| Offset | Bytes | Field |
| --- | ---: | --- |
| 0 | 2 | ASCII `EU` |
| 2 | 1 | version `02` |
| 3 | 1 | message kind |
| 4 | 2 | sequence (uint16, wraps) |
| 6 | 1 | payload length `0..16` |
| 7 | 0–16 | payload |
| final | 2 | CRC-16/CCITT-FALSE (init `FFFF`, polynomial `1021`) over header and payload; `123456789` → `29B1` |

| Kind | Direction | Payload / response |
| --- | --- | --- |
| `01` HELLO | PS4 → board | 4-byte session token, changed on reconnection (**not authentication**) |
| `81` IDENTITY | board → PS4 | 4-byte token, 1-byte gate level, 1-byte protocol version (`02`), 1-byte family (`01` ESP32 / `02` STM32 / `03` other), 1-byte fixed gate pin, 8 allowed-pin bytes (`FF` unused) |
| `04` PINS query | PS4 → board | Empty; read current four-pin assignment (does not move an output) |
| `04` PINS assign | PS4 → board | Four axis pin IDs; requires an identified session, **physical gate OPEN**, no active pulse or dwell, and all four distinct pins in the firmware's allowed pool |
| `84` PIN STATUS | board → PS4 | status byte, then current four pin IDs (`FF` if unassigned); host verifies exact assignment before any pulse |
| `02` PULSE | PS4 → board | axis 0..3, signed centidegrees `-300..-1` or `1..300`, duration `50..300` ms |
| `03` NEUTRAL | PS4 → board | Empty; request all provisioned outputs neutral |
| `82` ACK | board → PS4 | original kind, status, accepted signed centidegrees (zero for neutral/rejection) |

Status `0` means accepted by **board protocol**, not measured position; `1` means gate state forbids this command; `2` invalid data/pins/bounds; `3` no session, no pin mapping, active pulse or 500 ms neutral dwell; `4` stale/out-of-order sequence. For a PINS query, `3` also means no assignment yet. The board accepts only **one timed pulse**, returns that pin to neutral at its own deadline, and holds all allowed outputs at neutral by default. USB detach/suspend clears the assignment and session; opening the gate during a pulse neutralises it and invalidates the session. Its task checks state about every 10 ms, **not** to a certified timing tolerance. The 2 s ESP32 task watchdog is a last resort, not the independent power cut-off. A failed/mismatched pulse ACK prompts neutral and closes the host link; pulses are never retried. CRC is accidental-corruption detection, not authentication.

## Procedure with compatible firmware

1. Provision the target board's **own** authorized VID:PID, gate input and approved PWM pin pool in firmware. For the ESP32-S3 reference: `cd firmware/esp32s3 && idf.py set-target esp32s3 && idf.py menuconfig && idf.py build`. Review and instrument it before flashing; never flash the CI fixture. A precise STM32 board requires HAL glue per its port README.
2. Cross-build the unconfigured PS4 app using `make ps4`; **no VID/PID rebuild parameter exists anymore**. On the console, open Bench → Configure USB and pin map. Enter that board's own USB ID, family and descriptor values; **leave pins `FF`**, and save a transport-only draft. Identification is opt-in and does not need guessed GPIOs.
3. With servo power cut and the physical enable gate **OPEN**, choose **Identify / read GPIO pool**. Inspect the advertised board family, fixed gate input and approved PWM pins in the viewport. Select four of those pins in the inspector (editing closes USB but keeps the last pool **for display only**), save, and **re-identify** with the gate still open. Choose **Apply pin map (gate OPEN)**; its confirmation defaults to Cancel. A matching PIN STATUS means the board accepted a logical mapping, **not** that it measured PWM or moved a servo.
4. Close the gate only after electrical checks; return to manual controls and **re-identify** to read the new gate state and verify the mapping. Select one channel and choose ±2° for 150 ms; the second confirmation again defaults to Cancel. `□` / `N` requests neutral even while the confirmation is visible. Leave Bench or unplug to close the link. Detaching the USB device loses its pin map; repeat the gate-open setup on reconnection.

`make -j2 all test` runs CRC/framing, profile save/load/failure, descriptor matching (including selected composite interface), fragmented byte loopback, gate/allowlist/sequence/dwell/timeout, STM32 policy adapter with fake HAL, and actual native UI/HTTP interactions. The PS4 PKG cross-build and ESP32-S3 firmware **compile-only** job run in CI. None of these verify PS4 runtime USB, actual STM32 firmware, board enumeration, measured PWM, servo motion or electrical safety. LoRa and live gyro/altitude remain unimplemented.
