# Field notes — GarageBand testing (2026-06-28)

Observations from playing the AU in GarageBand, with status.

## Done this round

- **Expression pedal** — the EXPRESSION knob now drives setBfree's swell pedal
  (`swellPedalGain`), and **CC 7 and CC 11 both** drive it too (as in setBfree). It is a
  pure output gain — the Hammond expression pedal *is* a volume control; there is **no
  frequency-dependent filtering** in this setBfree code (the "brightness opening up" is the
  Leslie / psychoacoustics, not a filter). Default = full (1.0), matching prior loudness.
  New APVTS param `expression` (index 38). Note: an incoming CC pedal does not move the GUI
  knob yet (it sets the DSP directly) — a later refinement.

## Explained (not a bug)

- **GarageBand exposes "random" parameters (drum, horn, a drawbar ratio…).** All 39 APVTS
  parameters are published to the host — that's required for DAW automation. GarageBand's
  **Smart Controls** auto-pick a small, arbitrary-looking subset to show on its panel; the
  full list is under the automation menu. Nothing is wrong. If we want to declutter, we
  could mark the sound-design `ratio_top_*` / `ratio_bot_*` params non-automatable or group
  them — deferred (user: not important now).

## To investigate / TODO (deferred)

- **MTS-ESP master restart didn't re-attach without restarting tuneBfree.** Leading
  hypothesis: the silence-detection early-return in `processBlock` (skips DSP after ~3 s
  idle) also skips the MTS change-detection scan, so a tuning change while idle isn't picked
  up until you play a note. Likely recovers on the next note; a fix is to keep polling MTS
  for changes during the silent path. Could also be an MTS-ESP library reconnection quirk.
  Not yet reproduced.
- **A note rang continuously once (no panic in GarageBand).** Possibly a lost note-off
  (Bluetooth MIDI) rather than a tuneBfree bug. TODOs: (a) handle incoming **CC 120 (all
  sound off) / CC 123 (all notes off)** so the host transport / panic clears stuck notes;
  (b) add a **GUI panic button**. Not yet reproduced.
- **Master volume** — separate from expression (which is the swell pedal). setBfree has no
  separate master volume; adding one is reasonable but GUI placement is undecided. Later.
- **Multichannel** — user will write a spec another day; MIDI reference files added in
  `roadmap/refs/midi`. See the multichannel project memory and [[scala]] for the two-keyboard
  approach.
- **SYSEX** — deferred; spec PDFs to be downloaded into `roadmap/refs` (Drive links aren't
  fetchable). See `reference_midi_tuning_sysex_specs` memory and [[mts-esp]].
