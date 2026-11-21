# Current host walkthrough: media QA

This is an **editorial host capture**, not PS4, flight, RF, sensor or servo footage. The actual C++ workbench supplies every product frame. `tools/make_teaser.py` generates the 30-second film, poster and time-aligned captions from one storyboard; `make video` separately captures the longer unedited host replay.

| Segment | Actual source | What it does **not** claim |
| --- | --- | --- |
| Design | Native host model with scripted **built-in template** body/nose/fin changes; the inspector labels their origin and shows an empty fin log. | No user-entered fin measurement or certified geometry; the demo preset is not a physical design. |
| Ground | Native **empty** recorded-data page. | No sample CSV, real sensor, live USB stream, GPS or fabricated altitude. |
| Flight | Time-compressed calculated path and a labeled crop of the same native pixels. | Not a launched rocket or measured trajectory. |
| USB Bench | Current native host app with **no board connected**; both profile and manual views captured fresh. | No static old screenshot, PWM, USB transfer, servo travel or physical interlock test. |
| End card | Editorial text and a labeled calculated path crop. | Arduino ESP32/STM32 sketch is **compile-only**; hardware and PS4 remain untested. |

The restrained score is synthesized for the film; no motor, radio, servo or console sounds were sampled. The file has no testimonial, launch photo, invented metric or imitation hardware display. Readable English captions are provided in `video/eulolab-teaser.vtt` (there is no speech).

## Release proof

`make release-media` rebuilds both videos, poster and captions from the **current** C++ host binary and stamps their hashes against the README and source. `make release-check BASE=<previous-tag> TAG=<new-tag>` requires a changed README and a [per-tag review record](release-review.md) signed after watching the videos and reading the README. The [release workflow](../.github/workflows/release.yml) checks both a tag push and its published GitHub release, regenerates media and attaches files only after passing. GitHub still permits manual publication before CI finishes; do not trust those assets until both runs pass.

Automated QA decodes **all** frames/audio, checks count, size and duration, and compares decoded Design, **empty Ground**, Flight and **disconnected native Bench** frames (plus replay samples and the poster) with the current host executable. This cannot replace a person's review, and **neither media nor CI validates a physical system**. Hardware acceptance requires boards, instruments, operators and independent safety review; see the [ground evidence plan](ground-calibration.md).
