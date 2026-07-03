---
description: Scala .scl / .kbm tuning files — format, the octaveDegrees gotcha, keyboard-layout design recipe, and how tuneBfree consumes them
triggers:
  - scala
  - .scl
  - .kbm
  - keyboard mapping
  - kbm
  - scale file
  - tuning file
  - octaveDegrees
  - formal octave
---

# Scala `.scl` / `.kbm` files

tuneBfree loads local tunings with **Surge's tuning-library** (header-only, at
`libs/tuning-library/include/Tunings.h`, vendored as a submodule). This is the authoritative
parser; when in doubt read it (especially `TuningsImpl.h`). Official format docs:
[.scl](https://www.huygens-fokker.org/scala/scl_format.html),
[.kbm](https://www.huygens-fokker.org/scala/help.htm#mappings).

## `.scl` — the scale (pitches)

Comment lines start with `!`. Structure:
1. A **description** line (free text). The library puts it in `Scale.description`; tuneBfree
   shows this as the tuning name (not the filename). `Scale.name` falls back to the filename.
2. **count** — number of tones listed.
3. **count** tone lines — each either `cents` (has a decimal point, e.g. `77.995002`) or a
   **ratio** (e.g. `3/2`, `9/4`). The implicit `1/1` (0 c) is NOT listed.

The **last tone is the period** (the repeat interval): `Scale.tones.back().cents`. e.g. an
octave-repeating scale ends in `2/1` (1200 c); Bohlen-Pierce ends in `3/1`. tuneBfree shows
this as the "SPECIFIED PERIOD" under the FILE source.

`Tone::cents` is a `double`; `Scale::count` an `int`; `tones` a `std::vector<Tone>`.

## `.kbm` — the keyboard mapping (which key plays which degree)

Comment lines (`!`) are ignored. The fixed field order (see any file in `tunings/`):
1. **size of map** — keys in one repeating pattern
2. **first MIDI note** to map
3. **last MIDI note** to map
4. **middle note** — where the *first* mapping entry is placed (the 1/1 anchor)
5. **reference note** — the MIDI note whose frequency is pinned
6. **reference frequency** (Hz)
7. **formal octave / octave degrees** — scale degrees one pattern-repeat spans. `0` means
   "use the scale's size" (`KeyboardMapping.octaveDegrees`, an `int`).
8. … then **size** mapping lines — each a **scale degree** (0-based) **or `x`** = unmapped.

`x` (lowercase; capital `X` only in lax mode) means **this key plays nothing**. The parser
turns `x` into `-1` ([TuningsImpl.h:409](../../libs/tuning-library/include/TuningsImpl.h#L409));
a `-1` key sets `disable` so `Tuning::isMidiNoteMapped(n)` returns false
([:875](../../libs/tuning-library/include/TuningsImpl.h#L875)). Trailing `x` may be omitted.

## THE BIG GOTCHA: `octaveDegrees` must equal the scale `count`

When `octaveDegrees != count`, the library does NOT lay degrees out continuously — per
keyboard-octave it adds the scale's **full period** and folds the degree through it
([TuningsImpl.h:~930](../../libs/tuning-library/include/TuningsImpl.h#L930)). Symptoms:
- The first key of each octave **jumps** by ~(period − octaveDegrees·step) instead of one step.
- A naive "formal octave" = `freq(note+12)/freq(note)` comes out wrong (e.g. 9ed3halves with
  od=14, count=18 gave ~312 c, not the expected ~1092 c).

**So for a clean, jump-free layout, set `octaveDegrees == count`, which means the scale's
period must equal the interval you want the keyboard pattern to repeat at.** If you want a
"piano octave" that is some interval I, make a scale whose period is I.

## The incommensurability (why some layouts can't be perfect)

If a scale has **N** notes per period and you map it to a keyboard pattern of **K** keys per
"octave" where N and K don't divide evenly, you CANNOT have all of: every key sounds, uniform
steps, no gap, no duplicate. One must give every lcm boundary. Real cases:
- **13ed3 on white keys**: 13 notes per tritave vs 7 white keys/octave → forced a silent or
  duplicated white key every 2 octaves. Resolved by using a **7-note subset** (period = 7
  steps = 1024 c) so K=N=7 and it tiles cleanly (`tunings/13ed3/13ed3_white.{scl,kbm}`).
- **9ed3halves with a major-seventh octave**: made the scale's **period = 14 steps**
  (~1092 c) so `octaveDegrees = count = 14`; 12 keys map to 12 of the 14 degrees, skipping
  one at E-F and one at B-C (the two ~156 c steps).

## Design recipe (how to build a `.kbm` for a target feel)

1. Decide the **keyboard "octave"** interval (e.g. "piano octave ≈ major seventh").
2. Build a **scale whose period is that interval**, with `count` = the number of *distinct
   degrees per keyboard octave*. Put bigger steps where you want skipped notes.
3. `.kbm`: `octaveDegrees = count`; `size` = MIDI keys per pattern; map keys to degrees,
   using `x` for keys that should be silent. Keep `size` a multiple of 12 to stay aligned
   to the piano's white/black pattern.
4. **Multi-keyboard / "more notes" trick**: a second `.kbm` identical to the first but with
   the **reference frequency shifted by one step** (e.g. `440 / 2^(step_c/1200)`) sits one
   step lower and fills the notes the first keyboard skips. (Stack on two MIDI channels — see
   [[microtuning]] and the multichannel project memory; tuneBfree's engine merge for this is
   still TODO.)

## Why it may "not work" in practice

- **Scale Workshop exports only LINEAR `.kbm`** (no `x`, no real mapping) — you must hand-write
  mappings or it'll just be chromatic. This is the usual reason "removing notes didn't work."
- The synth must choose to honour unmapped keys. tuneBfree does, under FILE — see below.

## tuning-library API (include `Tunings.h`)

```cpp
auto scale = Tunings::readSCLFile(std::filesystem::path(p));   // path overload (string is deprecated)
auto kbm   = Tunings::readKBMFile(std::filesystem::path(p));
Tunings::Tuning t(scale, kbm);          // or Tuning(scale) for a default linear mapping
double hz   = t.frequencyForMidiNote(60);
double cts  = t.retuningFromEqualInCentsForMidiNote(60);
bool   on   = t.isMidiNoteMapped(60);   // false for an "x" key
```

## How tuneBfree consumes it

`PluginProcessor::loadSCLFile / loadKBMFiles` (message thread) build the per-channel FILE
grids used by the multichannel gamut (`buildFileGamut`; see the `microtuning`/`setbfree`
skills). Name shown = `Scale.description`; period shown = `tones.back().cents`. `x`
(unmapped) keys are baked to slot −1 (silent) per channel.

### Multi-`.kbm` assignment rule (per-channel tuning, tuneBfree 2.0)

The `.kbm` chooser is multi-select. Assignment (`loadKBMFiles` / `kbmChannelSuffix`) — NOT
alphabetical order:
- A file named `*_i.kbm` (i = 1..16) → the mapping for **MIDI channel i** (last selected
  wins for a repeated i). Generalises the Scala `_1.kbm … _16.kbm` convention.
- A file with **no valid `_i` suffix** → the **generic** mapping (last wins).
- A channel with no explicit `_i.kbm` falls back to the generic mapping, and that to the
  bare `.scl` (default linear mapping). So `.scl`-only = base scale on every channel.
- Which channels actually *sound* is the CHANNELS popup (`channelActive`), independent of
  which files were loaded. OMNI ON → every selected channel uses the generic mapping.
- This is tuneBfree's own layer — Surge's `Tunings.h` has **no** multichannel/multi-`.kbm`
  concept (one `.scl` + one `.kbm` → one `Tuning`).

## Example tunings (in `tunings/`, all covered by the doctest suite — see TESTING.md)

| Folder | Demonstrates |
|--------|--------------|
| 12ed2 | plain 12-TET (octave, no kbm) |
| 7edo  | `x` keys: 7 notes on the white keys, black keys silent |
| 14ed2 | two `.kbm` files (multichannel pair) |
| 13ed3 | `13ed3.{scl,kbm}` chromatic (tritave); `13ed3_white.{scl,kbm}` = 7-note white-key subset |
| 9ed3halves | major-seventh octave; `_1/_2.kbm` are a one-step-apart keyboard pair |
