# Workbench review | anti-slop Delivery Gate

Scope: the **native 1280 x 720 C++ framebuffer interface** and its thin, interactive host transport. The anti-slop skill (miqdadbadjuber/anti-slop, version observed 3.2.32, MIT) was used **During** this redesign, not invoked as a last-minute styling pass. The Design Read and palette/rhythm/interaction decisions are in [`DESIGN.md`](../DESIGN.md). This gate adapts browser-only checks to the native Orbis target; it does not claim an Orbis accessibility service or console runtime that has not been tested.

## Test evidence

- `make -j2 all test`: C++ core and UI tests plus 4 Python tests passed. The C++ test visits **all 32 inspector fields**, checks actual model mutation and framebuffer changes, validation, unsaved confirmation, save/open/undo/reset, every menu and toolbar action, focus/keyboard, controller stepping, timeline, help and USB empty state. Python sends HTTP events to a real host C++ process, checks PNG dimensions and decoded scanline length, and rejects malformed events.
- `g++ -O1 -g -fsanitize=address,undefined ...`: core and UI test binaries passed with leak/error checks. The host receiver is a relative-URL bitmap/event transport only; it never copies the model into JavaScript.
- Real host screenshots `docs/assembly-host.png` and `docs/descent-host.png` are both **1280 x 720**. The 79 s H.264 MP4 (12 fps, 1280 x 720) was rendered anew from native pixels; decoded frames at 22 s and 65 s show the new workbench and the modelled canopy. All frames are labelled `HOST REPLAY / NOT PS4`.
- Representative computed contrast ratios: slate on paper **12.23:1**, muted on paper **6.52:1**, blue on selection **5.75:1**, white on blue **7.03:1**, muted on viewport **5.89:1**. Computed with the WCAG relative-luminance formula from the actual RGB constants in `src/ui/app_draw.cpp`.

## Block 1: Hard Gate (each answer: no)

| Rule | Result and evidence |
| --- | --- |
| R-02 | **PASS:** no em dash in native UI copy or host preview HTML (`grep` across `src/ui`, `src/platform`, preview source). |
| R-03 | **PASS, native scope:** clipped model faces cannot paint outside the viewport (guard/scissor tests); host canvas preserves 16:9 at up to 100% available width without horizontal overflow. Fixed-resolution Orbis UI is not a mobile reflow design. |
| R-17 | **PASS:** displayed mass, CG, altitude, thrust, pressure and timestamps come from `Rocket`, `MotorCurve` and `FlightResult`, not fabricated statistics. |
| R-18 | **PASS:** no testimonials or fictional identities exist. |
| R-23 | **PASS:** the existing package icon was redrawn from the user's requested engineering-workbench direction; screenshots and MP4 were captured from the actual executable. No stock avatar/logo or fabricated data asset. |
| R-24 | **PASS:** File/Edit/View/Simulation/Help menu entries target implemented `Action` handlers, covered item by item in `tests/ui_tests.cpp`; no dead destination. |
| R-25 | **PASS:** contrast samples measured above are all at least 5.75:1 for interface text on its normal/selected backgrounds. |
| R-26 | **PASS:** every displayed command has a handler or a meaningful `Nothing to undo`/invalid-run message; clickable tree, numeric/choice controls, reset, menus, toolbar, chart, help, confirm buttons are exercised in UI tests. |
| R-27 | **PASS:** invalid numerical input keeps the editor open and explains limits; failed Open explains the missing project, invalid motor fit rejects edits, USB displays no-live-data empty state, and the host transport visibly reports connecting/ready/failure. |
| R-28 | **PASS:** there is no templated FAQ. The Help views document actual controls and model limitations. |
| R-32 | **PASS:** Tab/Shift+Tab and controller L2/R2 cycle visually outlined focus; arrows/D-pad operate tree, fields, menus and toolbar; Enter/X activates, Esc/O backs out. Host transport F2 releases canvas focus. Tested in UI suite. |
| R-33 | **PASS:** the interface and backend are repository C++/Python source, not run-time injected CSS or externally patched generated output. |
| R-34 | **PASS (not applicable):** no theme toggle is presented; a single intentionally light workbench is documented. |
| R-35 | **PASS:** built and ran C++ core/UI and live HTTP transport tests, clicked each menu/toolbar item by hitbox, checked keyboard/controller steps, rendered all replay phases, decoded new MP4 frames. No PS4 runtime claim. |
| R-36 | **PASS:** `MODEL ONLY / NOT FLIGHT-RATED`, symbolic flight-height label and `NOT PS4` capture mark prevent fabricated safety, precision, performance or hardware claims. |
| R-37 | **PASS:** direction, dials, palette, structure and product-specific motif were written to `DESIGN.md` before rendering the redesigned UI. |
| R-38 | **PASS:** no invented customer, company, measurement or security claim; the motor curves have documented published provenance. |
| R-39 | **PASS, native-adapted:** visible text labels, contrast, hover/selected/focus states and controller/keyboard input exist for interactive controls. The host document has `lang`, title, main landmark, named application canvas and a real Focus button; individual raster controls do **not** expose Orbis screen-reader semantics, which are not claimed. |
| R-40 | **PASS:** PNG and MP4 dimensions were decoded; the HTML canvas reserves its 1280 x 720 aspect ratio, loads no third-party script and does not assert an unmeasured FPS. |

## Block 2: Purpose Gate (no unexplained technique)

| Rule | Result and evidence |
| --- | --- |
| R-01 | **PASS:** no default gradient or glow. Matte surfaces separate chrome, editing panes and model plane (design rationale in `DESIGN.md`). |
| R-04 | **PASS:** no sparkle/robot/icon-library set; the only graphic motif is the rocket silhouette, CG/mount mark and measurement datum. |
| R-06 | **PASS:** Space Grotesk is the licensed app face; compact uppercase viewport labels communicate view mode and units, not a decorative tech font. |
| R-07 | **PASS:** viewport has a real metre ruler and datum; no nonfunctional blueprint/dot grid. |
| R-08 | **PASS:** `<`/`>` are real choice steppers and +/- changes values, not decorative arrows. |
| R-09 | **PASS:** no hype capsule; `HOST REPLAY / NOT PS4` is plain factual status copy. |
| R-10 | **PASS:** no glass treatment. |
| R-12 | **PASS:** only modal/menu layering has an offset edge to identify blocking overlays; normal panels are flat. |
| R-13 | **PASS:** no glow. |
| R-14 | **PASS:** unequal panels are sized by tree, model and property counts rather than copied feature cards. |
| R-19 | **PASS:** no entrance/floating animation; only input-driven orbit and pausable, model-derived playback. |
| R-22 | **PASS:** no generic illustration; the rendered mesh is generated from the editable part geometry. |

## Block 3: Liveliness (each answer: yes)

| Criterion | Result and evidence |
| --- | --- |
| Declared dials | **PASS:** ENERGY 1, RHYTHM 2, MOTION 1 in `DESIGN.md`. |
| Matches dials | **PASS:** quiet matte palette, uneven work areas, motion solely for interaction and playback. |
| Focal point | **PASS:** dimensional rocket viewport on Design/Flight; explicit no-telemetry message in USB mode. |
| Structural whitespace | **PASS:** narrow controls and inspector use density; viewport and explanatory footer reserve space for model and constraints. |
| Deliberate accent | **PASS:** blue distinguishes selection, keyboard focus, CG and the calculation trace; it is not a surface color everywhere. |
| Identity motif | **PASS:** measured silhouette, CG witness and datum recur in the viewport and package tile. |
| Design Read | **PASS:** audience, decision loop, composition and visual reasons recorded before implementation in `DESIGN.md`. |

## Block 4: Craft and consistency (each answer: no)

| Lock | Result and evidence |
| --- | --- |
| C-1 | **PASS:** layout, accent, type and ruler each have a product-specific rationale in the design document. |
| C-2 | **PASS:** C++ hitbox/menu/toolbar tests and host HTTP input prove actions; invalid input stays in an editable error state. |
| C-3 | **PASS:** assembly, inspector, viewport, calculated timeline and USB empty state correspond directly to tasks, not a landing-page template. |
| C-4 | **PASS, target scope:** tests render all replay modes, field/validation states and confirmation; the viewport scissor preserves surrounding controls at high zoom. 1280 x 720 is the fixed native target. |
| C-5 | **PASS:** data comes from the core and published motor curves, with explicit safety and console-runtime caveats. |
| R-05 | **PASS:** no hero/cards/testimonials/FAQ template; pane widths follow content and the editing workflow. |
| R-11 | **PASS:** square-ended editor fields, flat panel seams and one modal edge; no repeated pills. |
| R-15 | **PASS:** commands name their concrete actions: Save, Fit view, Jump to apogee, Reset component. |
| R-16 | **PASS:** no AI marketing buzzwords in the interface. |
| R-20 | **PASS:** changing the product name would not explain the rocket-specific ruler, CG, fin planform, motor mount or flight curve. |
| R-21 | **PASS:** light desktop-tool theme is a task choice, not forced dark-for-tech. |
| R-29 | **PASS:** neutral/paper/slate plus one blue accent; meaning is also conveyed by text and outline. |
| R-30 | **PASS:** not a skin of Blender, OpenRocket or another popular product; structure is tailored to these six components and the educational model. |
| R-31 | **PASS:** each major visual decision has a one-line purpose in `DESIGN.md`. |

**Boundary:** This audit covers the actual shared host/UI code and the PS4 build pipeline. Console installation and input/display behavior, screen-reader integration with Orbis, full 3-D flight and flight-safety validity remain unverified or unimplemented, and must not be inferred from a green build or this visual gate.
