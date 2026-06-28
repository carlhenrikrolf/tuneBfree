# GUI ↔ Engine Wiring — status & open specs

What got wired from the default page to the APVTS parameters (2026-06-28), and the
decisions still needed for the parts left **disabled** (greyed at 40 % alpha) in the UI.

## How the wiring works

Each control writes its parameter on change (`control.onValueChange → setParam`). A 15 Hz
timer in the editor (`syncFromParams`) pulls parameter values *back* into the controls so
host automation and preset recall show up; it **skips while the mouse is down** so it never
fights an active drag. This manual scheme (rather than `APVTS::SliderAttachment`) is
deliberate: the attachment overwrites a slider's `NormalisableRange`, which would destroy
the drawbars' reversed (pull-down-louder) range.

> Trade-off to revisit: parameter→GUI updates are at 15 Hz, not sample-accurate. Fine for
> a live instrument; if we later want smoother automation display we'd move to attachments
> and solve the drawbar-range problem another way.

## Wired and believed correct

| Control | Parameter(s) | Notes |
|---|---|---|
| 9 drawbars | `drawbar0…8` | reversed range preserved |
| REVERB knob | `reverb_mix` | direct 0–1 |
| VIBRATO/CHORUS/OFF + DEPTH | `vibrato`, `vibrato_type` | type interleaved `0=V1,1=C1,2=V2,…`; `type = 2·(depth−1)+chorus` |
| CHORALE/STOP/TREMOLO | `drum`, `horn` (set together) | speed `0=stop,1=slow,2=fast` |

## Confirmed by the user (2026-06-28 test pass)

- Drive, reverb, Leslie, vibrato/chorus, drawbars, default 12-EDO all good.
- **Percussion 2ND/3RD is correct** (my guessed `3RD → 1, 2ND → 0` mapping was right).
- **Percussion SOFT/NORM**: the button was relabelled HARD → **NORM**. Polarity kept as
  **SOFT → `percussion_vol` 0** (engine `isSoft=1`); I traced this as correct, but it's
  subtle to hear — re-confirm, and if inverted it's a one-line swap in `applyPercToParams()`.

## Tuning-source gating (done 2026-06-28)

The encoding dropdown now actually selects which source feeds the engine
(`TuningSourceId`: MTS / SYSEX / FILE / STANDARD). Implemented:
- `reinitToneGen`, `getDisplayFrequency`, `getDisplayCents`, `isMidiNoteMapped` all branch
  on the source — `.scl/.kbm` apply **only under FILE**; STANDARD uses a plain 12-TET
  table; MTS/SYSEX read the MTS-ESP client. MTS change-detection only reinits under
  MTS/SYSEX.
- Loading a file while the source isn't FILE pops an OK/Cancel dialog offering to switch.
- Scale **name** comes from the `.scl` description line; **period** under FILE shows the
  Scala-declared period (last tone) as "SPECIFIED PERIOD"; default name "GEAR60 (~12EDO)".
- File choosers remember the last-used directory.

- **Note-on filtering** silences notes the active source marks as unplayed, per source:
  **MTS-ESP** via `MTS_ShouldFilterNote` (the master's per-scale signal), **FILE** via the
  `.kbm`'s `x` (unmapped) keys. SYSEX and STANDARD never filter. The `.kbm` mapping is
  snapshotted into an audio-thread-owned `currentNoteMapped[]` at reinit (filled from
  `localMapped[]`, written on the message thread), so `localTuning` is never read on the
  audio thread.

Caveats / still open:
- **`tuningSource` + loaded files are not persisted** across plugin reloads yet (reset to
  MTS / Gear60). Worth adding to `get/setStateInformation`.
- Per-encoding state is shallow: FILE keeps its loaded files when you toggle away and back,
  but the NOTE ON / ALWAYS toggle is global, not per-encoding.

## Disabled in the GUI — decisions needed before wiring

1. **EXPRESSION (volume).** No parameter exists. Decisions:
   - *What does it control* — the swell-pedal gain (`swellPedalGain`, pre-effects, the
     authentic Hammond behaviour) or a master output trim (post-effects)?
   - Add a new APVTS parameter (index 38, after the current 38) — note this shifts the
     "indices match `src/clap.cpp`" invariant; confirm that's OK or reserve an index.
     - Range/curve (linear vs dB taper) and default.
   - MIDI: should it follow CC 11 (expression) and/or CC 7 (volume) automatically?

2. **UPPER / LOWER / BITIMBRAL / SPLIT / CROSSFADE.** The DSP is single-manual by design
   (CLAUDE.md). Real second-manual / split support is a large DSP change. Decision: do we
   ever want it in-plugin, or is "run multiple instances" the answer? If the staggered
   multi-channel idea (see `project_multichannel_tuning` memory) lands, UPPER/LOWER may be
   repurposed — worth deciding before building UI for it.

3. **Leslie detail.** Today one 3-way sets horn and drum to the *same* speed. The engine
   has independent `drum` and `horn` (0–2 each) and the whirl supports more (brake,
   per-rotor speeds). Decision: expose separate horn/drum controls, a brake, or keep the
   single combined switch for the stage view (and put detail in a future studio view)?

4. **Drive detail.** `character` is one of several overdrive params (the preamp also has
   input/output gain shaping). Decision: should DRIVE be character only, or a macro over
   several? And is "DRIVE 0 = bypass" the right gesture, or do we want an explicit
   overdrive on/off?

## Not in scope here (separate work)

- **Drawbar ratio overrides** (`ratio_top_0…8`, `ratio_bot_0…8`, params 20–37) — these are
  the Studio/sound-design view, not built.
- **Presets / programs** management page — APVTS state already persists via
  `get/setStateInformation`; a UI for named presets is separate.
- **MIDI-CC mapping / MIDI learn** for live control — Phase 3 / config.
