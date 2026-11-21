# Workbench review: anti-slop Delivery Gate

Scope: the shared **native 1280 × 720 C++ UI** and its thin Linux framebuffer preview. [`DESIGN.md`](../DESIGN.md) records the design read and reasons before the bench panel was drawn. The [`anti-slop`](https://github.com/miqdadbadjuber/anti-slop) core, UI and code checks were applied **during** implementation, not just to a final screenshot. Web-only rules are interpreted at the preview boundary; they do not imply Orbis accessibility support or a responsive console display.

## Reproducible checks

| Check | Evidence |
| --- | --- |
| Functional tests | `make -j2 all test`: core model/mesh/CSV/file tests, 52/52 editable inspector fields, hitboxes, menu and toolbar, keyboard/controller focus, visual reference, playback, opt-in Bench modal and neutral, USB byte stream/descriptor/board loopback, and 7 HTTP tests. Fakes are explicitly not hardware. |
| Build hygiene | `git diff --check` and `g++ -Wall -Wextra -Wpedantic` for host. The Orbis adapter was syntax-checked against OpenOrbis v0.5.4's actual Usbd/libusb declarations, not a generic host libusb. CI now has a separate firmware compile-only job with reserved USB IDs. Full PS4 and firmware CI must run on the pushed commit; an earlier green build does not cover later edits. |
| Image inspection | `docs/assembly-host.png`, `docs/descent-host.png`, `docs/bench-host.png`: newly rendered by the shared executable at 1280 × 720. The bench-only modal was also rendered and inspected in a local test capture. A clipped left label, two oversized buttons and a status overlap were corrected after inspecting pixels. These images are **host output**; there is no photographed servo or PS4 frame. |
| Contrast | WCAG relative-luminance calculation from `app_draw.cpp`: ink on paper 12.23:1, muted on paper 6.52:1, ink on bench surface 11.13:1, blue on selection 5.75:1, white on blue 7.03:1. These are source-color checks, not TV calibration. |

## Delivery Gate (native-adapted)

| Rule | Status and specific observation |
| --- | --- |
| R-02 | **PASS:** no em dash in newly written UI/README copy; punctuation uses periods/colons. |
| R-03 | **NATIVE SCOPE:** canvas preserves a 16:9 aspect ratio and scales to the host wrapper; console UI is deliberately fixed 1280 × 720. No claim of a fully reflowed mobile inspector. |
| R-17, R-18, R-38 | **PASS:** no invented testimonials or usage numbers. Measurements are computed by `Rocket`, `FlightResult` or imported recordings; bench output is labelled *command*, not measured position. |
| R-23 | **PASS:** screenshots come from the running C++ workbench and are labelled host-only; no invented logo, avatar, motion footage or electrical evidence. |
| R-24 | **PASS:** File/Edit/View/Simulation/Help actions, Bench toolbar and modal buttons have tested native handlers; no dead navigation destinations. |
| R-25 | **PASS for source-color pairs measured above:** the bench view also uses text and a focus border, not color alone. |
| R-26 | **PASS:** 52 properties, camera controls, graph scrub, File actions and Bench connect/channel/pulse/neutral have actual handlers. In the host preview Connect fails with an explicit *no USB bridge* error instead of implying movement. |
| R-27 | **PASS:** missing mesh/recording/project, invalid geometry/input, no sensor file, no USB profile/board, open physical gate and missing ACK have distinct status/error paths. Modal defaults to Cancel. |
| R-28 | **PASS:** contextual Help describes controls and limitations, not a generic FAQ. |
| R-32 | **PASS:** Tab/Shift+Tab, arrows and controller focus work in native UI; `F2` exits the host canvas. Bench confirmation has keyboard/controller selection and immediate neutral via `N`/`Square`. |
| R-33, R-34 | **PASS:** repository-owned C++/Python UI source, no injected-theme patch or nonfunctional theme switch. |
| R-35 | **PASS for host:** tests and actual frames were executed/inspected. **Unverified:** console runtime, USB enumeration/transfer and firmware/servo behavior. |
| R-36, R-37 | **PASS:** design read in `DESIGN.md`, viewport/model disclaimers and Bench separation precede any hardware claim. |
| R-39 | **NATIVE SCOPE:** visible labels, focus outline, controller/keyboard and host document semantics exist. Individual framebuffer controls do not expose Orbis screen-reader semantics. |
| R-40 | **PASS for host images:** PNG dimensions decoded to 1280 × 720; browser reserves the canvas aspect ratio. No performance or FPS guarantee. |
| R-01, R-04, R-06 to R-14 | **PASS:** no decorative gradients, glows, glass, blueprint grid, stock icons or cards. Space Grotesk, measured datum, flat inspector fields and one blue accent serve the engineering/editing hierarchy. |
| R-19, R-22 | **PASS:** motion is input-driven orbit/playback; the geometry is calculated from the editable model or an explicitly imported visual reference, not a fake dashboard animation. |
| C-1 to C-5 | **PASS within host scope:** layout/palette rationale, tested interactions and real source data are documented; the console is not represented as tested. |

**Open boundaries:** the flight model uses assumed coefficients and cannot certify stability or safety. The USB bench code and ESP32-S3 reference firmware require a real assigned VID/PID, reviewed pin wiring, firmware compilation, physical cut-off, instrumented PWM/neutral tests and console verification. LoRa and live gyro/altitude are not implemented. Host fakes, photographs of host frames and a PKG integrity check cannot resolve those boundaries.
