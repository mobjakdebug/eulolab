# EuloLab

A C++ model-rocket workbench with a Linux preview and a PS4-targeted build. **The model is not flight software.** No board, rocket, servo, radio or PS4 console has been tested for this project.

[30-second walkthrough](video/eulolab-teaser.mp4) · [poster](docs/teaser-poster.png) · [captions](video/eulolab-teaser.vtt) — captured from the **host app**, not a console or physical test.

## What you can do

- **Design:** edit 52 geometry, mass and scenario inputs across nine sections. Each saved `.eul` v3 input is marked as a template value, a user-entered **design** value, or an old-file value with unknown origin. Imported OBJ/STL is visual reference only.
- **Compare fins:** import your own `fin-inspection.csv` with per-fin dimensions, root position, cant and azimuth plus UTC time, operator and instrument. The app shows each reading next to the model without changing the solver or declaring a pass. [Schema and measurement procedure](docs/ground-calibration.md).
- **Explore a prediction:** Flight runs an illustrative, open-loop 3D model using [published Klima motor curves](assets/motors/README.md). Displayed altitude, mass/CG and trajectory are **calculated**, not flight measurements or stability approval.
- **Inspect a recording:** Ground imports a user-supplied pressure/temperature/gyro/acceleration CSV (or a legacy altitude/gyro CSV). It derives **relative** barometric height from a stationary baseline and shows sensor-axis rotation and gaps. There is no bundled measurement and no live board-to-app link.
- **Record with Arduino:** [`EuloRecorder.ino`](firmware/arduino/EuloRecorder/EuloRecorder.ino) supports candidate BMP3xx and LSM6DSOX modules on **ESP32 or STM32duino**. Optional SX127x LoRa sends sparse point-to-point status to a receiver sketch. The checked-in configuration is **unprovisioned**: no sensor output or RF until the exact hardware and lawful radio settings are reviewed. No GPS. [Board setup and serial capture](firmware/arduino/EuloRecorder/README.md).
- **Configure a separate USB bench:** the app can edit an EuloBench/2 device profile and request bounded, manually confirmed servo pulses. The host preview has **no USB bridge**. The Arduino recorder is **not** Bench firmware; no compatible Arduino vendor-bulk board or measured servo movement has been demonstrated. [USB contract and precautions](docs/bench-usb.md).

## Run the host app

```sh
make -j2 all test       # native and Python regressions
make preview            # open the shown URL; locally http://localhost:8000
```

The browser displays frames from the C++ app; it does not run a second simulator. Click the canvas for keyboard input; `F1` shows controls and `F2` releases browser focus. `Ctrl+S/O/N/Z` save/open/new/undo; `Ctrl+L` imports `recording.csv`, `Ctrl+I` imports `fin-inspection.csv`. The browser also has validated file pickers. On the host, place these CSVs beside `out/project.eul`, or upload them through the preview. Files are checked before replacing an existing import. **Test fixtures in `tests/` are not physical observations.**

For a real sensor capture, first identify the board, sensor parts, addresses, voltage and pins, then follow the [Arduino bring-up guide](firmware/arduino/EuloRecorder/README.md). `tools/capture_serial.py` preserves received CSV bytes and a hash/port trail; it cannot authenticate the sensor. Keep motors, igniters and powered actuators disconnected during ground bring-up.

## Builds and limits

`make ps4` requires the pinned [OpenOrbis v0.5.4 toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/tag/v0.5.4). CI cross-builds and inspects a homebrew PKG and **compiles** representative ESP32-S3/STM32duino recorder roles; it does not install a console package, flash a board, measure servo travel or transmit RF. The separate ESP32-S3 ESP-IDF Bench reference and STM32 policy adapter are not Arduino `.ino` USB Bench ports.

To advance beyond host/compile validation we need the **actual** board/module markings and wiring, real fin logs and calibrated reference readings, a safe instrumented USB/PWM rig, and a console run. Do not infer flight safety from this app. [Ground evidence plan](docs/ground-calibration.md) · [engineering maturity and remaining gates](docs/project-review.md).

## Every release

Before **each tag**, update this README to match the code, rebuild both host videos/poster/captions with `make release-media`, **watch both videos and review the README**, then complete the [per-tag review record](docs/release-review.md). Run `make release-check BASE=<previous-tag> TAG=<new-tag>` (use the root commit for a first release). The [release workflow](.github/workflows/release.yml) runs on tag push **and** publication: it requires a changed README and signed review record, checks the decoded media against the current native app, regenerates it, and attaches the files. A green build is not hardware evidence; GitHub permits manual publication before CI finishes.

Media tools: `python3 -m venv .venv && .venv/bin/pip install -r requirements-media.txt`, then `PATH="$PWD/.venv/bin:$PATH" make release-media`. Code: [MIT](LICENSE); font: [SIL OFL](assets/OFL.txt). No third-party SDK or sensor library is bundled.
