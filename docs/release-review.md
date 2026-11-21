# Release review record

Update this record **for each release tag** before pushing the tag. Change `Release tag` to that exact tag, name the person who reviewed it, and tick the checks *only after doing them*. `make release-check TAG=<tag> BASE=<previous-tag>` and the tag/release workflows reject missing, unchanged or unticked review records. A checked box is an attestation, not proof of a physical test; CI cannot know whether someone watched a video.

Release tag: pending (PR #1 candidate, not a release)
Reviewed by: pending maintainer review

- [ ] Read the current README against the shipped code: run steps, working features and unverified hardware claims.
- [ ] Watched **both** regenerated videos end-to-end; inspected the poster and captions for timing/readability.
- [ ] Checked that fins are unmeasured, Ground is empty, Flight is a model and Bench is disconnected; no board, RF, GPS, servo, flight or PS4 footage is implied.
- [ ] Checked the decoded media QA result and `make release-check` for this exact source commit.

For this PR candidate, the build agent decoded the film/replay and visually inspected representative Design, Ground, Flight, disconnected Bench, opening and end frames. The recorded output comes from the native host binary; this is **not** the maintainer sign-off above and does not validate hardware. The release operator must update this record before tagging/publishing.
