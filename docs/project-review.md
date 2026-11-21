# EuloLab: project maturity review

This is a subjective assessment of the **current prototype**, not a flight-safety rating, a console benchmark, or a measured prediction-accuracy score. The 30-second [teaser](../video/eulolab-teaser.mp4) is finished first; it shows authentic C++ **host** frames, not PS4 or actuator footage. Source, sound and limitations are described in the [README](../README.md#تیزر-پروژه) and [design direction](../DESIGN.md#teaser-direction).

**Scale:** 0 = absent, 5 = meaningful host prototype with major target-system gaps, 10 = validated on the intended hardware for the stated purpose. Scores are editorial, in half-point steps, and each category has equal weight. They do not assert that any rocket is safe to build or launch.

| Part | Score / 10 | Evidence and the main limit |
| --- | ---: | --- |
| Native workbench and interaction | **7.5** | Shared C++ renderer; functional part tree, inspector, menus, focus, keyboard and controller paths ([UI](../src/ui/), [tests](../tests/ui_tests.cpp)). PS4 ergonomics and execution have not been tested. |
| Parametric assembly and materials | **7.0** | Nine sections, 52 editable properties, mesh, approximate mass/CG and a cylindrical custom insert ([model](../src/core/rocket.cpp), [mesh](../src/core/mesh.cpp)). It is not arbitrary-part CAD or measured materials engineering. |
| Projects, import and export | **6.5** | Versioned `.eul` with atomic save; OBJ and ASCII/binary STL visual reference; visual OBJ export ([file/model code](../src/core/rocket.cpp), [mesh code](../src/core/mesh.cpp)). Imported geometry does **not** affect mass or flight, and linked reference files must travel with the project. |
| 3D flight and virtual TVC | **5.0** | Open-loop translation, attitude, wind, fixed gimbal settings, range and Mach with playback ([flight solver](../src/core/flight.cpp)). Coefficients and recovery are illustrative; no closed-loop TVC, flight validation or model-to-actuator control. |
| Recorded-data Ground page | **3.5** | CSV gyro/altitude import, stationary-window calibration and relative attitude replay ([telemetry](../src/core/telemetry.cpp)). No live sensor transport or LoRa; integrated gyro attitude drifts. |
| USB servo bench and in-app configuration | **4.5** | Runtime VID:PID/interface/endpoints, board-advertised pin pool, gated mapping, fixed brief manual pulses, neutral paths and protocol tests ([Bench protocol](../src/core/bench_link.cpp), [profile](../src/core/bench_profile.cpp), [USB adapter](../src/platform/ps4_bench_usb.cpp)). Neither console USB nor a real PWM/servo response has been observed. |
| ESP32-S3 and STM32 board support | **3.0** | ESP32-S3 reference **compiles** in CI; STM32 portable port passes tests with a fake HAL ([ESP32-S3](../firmware/esp32s3/), [STM32](../firmware/stm32/)). No flashed-board test or STM32-specific USB/timer integration; not a universal chip driver. |
| PS4 package and runtime confidence | **4.0** | OpenOrbis build/packaging CI and a host-shared app ([PS4 target](../ps4/Makefile), [CI workflow](../.github/workflows/build.yml)). A package check is not installation or a frame rendered on a PS4. |
| Tests, documentation and presentation | **7.5** | Host model/UI/protocol/HTTP tests, compile-only firmware CI, explicit [limitations](workbench-audit.md), and a reproducible [teaser](../tools/make_teaser.py) with original non-diegetic audio. No physical lab evidence, certification or console performance measurement. |

**Equal-weight average:** (7.5 + 7.0 + 6.5 + 5.0 + 3.5 + 4.5 + 3.0 + 4.0 + 7.5) / 9 = **48.5 / 9 = 5.39 / 10**, or **5.4 / 10** to one decimal place.

The strongest parts are the host workbench and testable editing workflow. The largest gaps are live Ground telemetry, firmware tied to specific boards, verified PS4 USB/graphics, and physically measured servo behavior. Raising the maturity score requires an instrumented, power-isolated actuator test on each named board, a console runtime/USB test, real sensor integration, and validation of the model against independently measured flight data. None of those checks can be inferred from the teaser or CI alone.
