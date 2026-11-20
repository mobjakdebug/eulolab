# EuloLab workbench design direction

## Design Read (settled before implementation)

This is an engineering desktop-style workbench for comparing **educational model-rocket configurations**, not a game menu or a flight certificate. The user's loop is **select component -> change a real property -> inspect shape/mass -> run and scrub a calculated flight**. The output must remain usable with a controller on the PS4 target and with a mouse/keyboard in the host preview. No Windows executable was requested.

Dials: **ENERGY 1 / RHYTHM 2 / MOTION 1**. Quiet, purposeful hierarchy; motion only for deliberate orbit, input feedback and pausable flight playback.

## Composition and identity

- The application shell uses a conventional menu bar and command toolbar above three unequal work areas: **assembly tree (244 px), dimensional viewport and trajectory dock (716 px), property/analysis inspector (320 px)**. A status bar carries actionable errors and the model-only warning. The viewport gets the most space because editing shape is the primary task; the 9-field fin inspector still fits without scrolling at 1280 x 720.
- A dark slate title strip anchors the window. Otherwise surfaces are matte warm-neutral (#f2f4f3) and paper (#f9faf9), text is slate (#27343b), measurement and viewport planes are cool grey. **Blue (#235b93)** is reserved for selected components, CG/mount witness marks, the flight curve and visible keyboard focus. Thin borders denote field boundaries rather than decorative cards or glass.
- The repeated product motif is a **measured vertical silhouette with datum, metre ruler and dimensional witness line**. These lines have units and a job. There is no decorative blueprint grid, glow, fake flight instrument, game mascot, invented performance claim, or marketing logo. The native package tile reuses the same silhouette, datum and type, replacing the earlier dark/grid game-like icon.
- Space Grotesk is licensed under OFL and rasterised once into a compact 17 px interface face and 27 px section face. Square-ended fields and an 8 px spacing cadence keep editing dense but legible. Headers, field labels, values and muted explanations have distinct hierarchy; color is not the sole selection cue (a 3 px selection rail and a 2 px focus outline also remain).
- The assembly ruler is to scale; the **flight viewport explicitly states its altitude is symbolic**, while the measured altitude appears on the trajectory graph and in the right-hand numeric inspector. The canopy is explicitly idealised. USB has a genuine empty state and does not fabricate telemetry.

## Functional contract

- Each of the **32 visible inspector properties** is backed by the real C++ `Rocket` model. Body, nose, fins, avionics, the motor's internal sleeve/fit and recovery alter geometry, mass, CG or the educational flight calculation where physically relevant. Catalog thrust curves are published read-only data; the selector switches between C6 and D9. Editing validates ranges and dependent geometry; invalid changes leave the model unchanged and explain why. Model mass and trajectory recalculate on accepted changes.
- Numeric properties accept keyboard entry in the host preview (Enter to apply, Esc to cancel), step on +/- or D-pad, and show their SI-derived units. Controller-only Orbis mode **never opens an unusable text box**: X steps values and the D-pad adjusts them. An internal mount cutaway appears when the mount is selected. Save/open uses the versioned project file; an unsaved-project confirmation defaults to Cancel.
- Menus, toolbar commands, tree selection, inspector, reset, undo, help, drag orbit, wheel zoom, trajectory scrubbing and playback are wired. Tab/L2/R2 advances focus; focus, hover and selection states are distinct. File, Edit, View, Simulation and Help contain only working actions. Action names and hitboxes are shared by the PS4 and host build.
- The host browser preview is **only a framebuffer/event transport**. The pixels and simulation come from `App::draw` in the real native host C++ executable; HTML/JS does not reproduce the UI. The PS4 build has no browser, server or internet dependency. No console execution or certified flight prediction is claimed.

The anti-slop skill from `miqdadbadjuber/anti-slop` (version observed during this redesign: 3.2.32, MIT) was applied in **During** mode: settle this direction, justify each visual technique, write functional controls and states, test keyboard/pointer/controller actions, then run the Delivery Gate. Web-specific rules are interpreted at the native framebuffer boundary; they are not misrepresented as Orbis screen-reader support. See `docs/workbench-audit.md` for the final gate and concrete test evidence.
