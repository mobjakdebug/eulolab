# Teaser delivery gate

Scope: [30-second MP4](../video/eulolab-teaser.mp4), [poster](teaser-poster.png) and [text transcript](../video/eulolab-teaser.vtt), **not** a new interactive web page or verified hardware footage. Visual direction and the reason for each technique precede generation in [DESIGN.md](../DESIGN.md#teaser-direction). The existing native UI has its separate [workbench gate](workbench-audit.md).

## Hard gate

| Rule | Result and evidence |
| --- | --- |
| R-02 | **PASS:** on-screen editorial text and transcript contain no em dash. |
| R-03 | **PASS within media scope:** encoded at the native 16:9 frame (1280 × 720), without a claimed mobile UI redesign. Player scaling is external to this file. |
| R-17 | **PASS:** 52 properties/9 sections are counted by the UI test; flight readouts come from the C++ model. There are no invented performance or customer metrics. |
| R-18 | **PASS:** no people, testimonials or fake social proof. |
| R-23 | **PASS:** teaser explicitly requested; no invented logo, launch photo, PS4 screen, actuator footage or human image. The title is text, and product images are genuine native host pixels. |
| R-24 | **N/A to video:** no links/navigation inside the encoded frames; the one repository URL is an ordinary printed address. The README links to real files. |
| R-25 | **PASS:** measured editorial text contrast: ink/paper 12.00:1, muted/paper 6.71:1, blue/paper 6.59:1, white/ink 12.42:1 and light-slate/ink 8.71:1. Native UI colors are audited separately. |
| R-26 | **N/A to video:** no fake clickable UI has been added; shown controls are native host output and are exercised by host tests. |
| R-27 | **N/A to media:** no data-fetching UI added. Shown Bench frame explicitly says no board connected; app empty/error paths are in the workbench gate. |
| R-28 | **PASS:** no FAQ, filler copy or stock sales language. |
| R-32 | **N/A to encoded frames:** playback/seek/pause use the user's video player; the native UI's keyboard and controller tests are recorded separately. |
| R-33 | **PASS:** the editing source is `tools/make_teaser.py`; no generated script patches the application UI or external stylesheet. |
| R-34 | **N/A to video:** no theme toggle; dark bookends and light editorial cards are not an app theme. |
| R-35 | **PASS for host/media:** `make -j2 all test` passed; the MP4 decoded as 720 frames / 30 seconds / 24 fps with an AAC track. Extracted design, flight, detail-crop, Bench and end frames were inspected. No console or servo test is claimed. |
| R-36 | **PASS:** film states host, model-only and no PS4 footage; end card separates untested PS4/servo from unimplemented live sensors/LoRa. The score has measured peak -8.9 dBFS; no fake machinery is sampled. |
| R-37 | **PASS:** explicit ENERGY 1 / RHYTHM 2 / MOTION 1 from the design read, applied before editing; no aimless effects. |
| R-38 | **PASS:** host frames and Bench screenshots are real; title cards are labeled editorial copy, not fabricated measurements or photorealistic scenes. |
| R-39 | **PASS within video scope:** on-screen disclaimers are readable; a WebVTT transcript describes chapters and the lack of dialogue. Player-level controls/semantics are not invented by this repository. |
| R-40 | **PASS within video scope:** intrinsic 1280 × 720 MP4 dimensions; no unsized HTML media, unknown Web Vitals, or fake FPS guarantee. |

## Purpose, liveliness and craft

| Check | Result and evidence |
| --- | --- |
| R-01, R-04, R-07, R-08 to R-14, R-22 | **PASS:** no gradients/glows, avatar art, generic icons, patterned grid, fake CTA or tiled cards. One blue datum rule reflects the assembly's dimensional witness mark, not a background texture. |
| R-06, R-20, R-21, R-29 to R-31 | **PASS:** locally licensed Space Grotesk and the workbench ink/paper/slate palette tie the film to the product. Dark title/end cards mark editorial breaks; the app itself is not shown in a fake dark theme. No cloned landing-page composition. |
| R-19 | **PASS:** only the measured datum reveal, brief opening crop shift and time-compressed *calculated* native flight move; no unrelated bouncing, perpetual glow or stock rocket animation. |
| Liveliness | **PASS:** declared dials align with restrained movement, alternating full UI and typography, one blue accent, white space for hierarchy, a repeated measured silhouette, and one focal point per card. |
| C-1 to C-5, R-05, R-11, R-15, R-16 | **PASS within video scope:** source shots, labels, scores and captions have a stated purpose, sections follow assembly → flight → isolated Bench, no empty CTA/theme/interactive template, and no invented safety/performance proof. Host preview and physical-hardware limits remain visible. |

**Residual limits, not hidden successes:** console runtime, live sensors, USB/PWM on a board and servo travel remain unverified or unimplemented as labeled. A clean visual gate is not a hardware certification.
