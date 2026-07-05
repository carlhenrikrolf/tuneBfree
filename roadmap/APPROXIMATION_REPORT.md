# Approximate Period Inference — the short version

**Status: proposal, not implemented.** The engine still uses exact matching.
Nothing below is decided; §4 lists the decisions to make.

## 1. The problem, in two sentences

tuneBfree looks for the interval at which a tuning repeats (the *period*) so it
can extend the 128-note table up to the ~300 tonewheels the drawbar harmonics
need. Today "repeats" means *exactly* (to about a millionth), so a `.scl` file
whose cents were rounded to three decimals — i.e. most real files — often fails
the test, and the high tonewheels silently degrade (they get clamped to the top
frequency).

## 2. The proposed fix — one rule, one number

> For each candidate scale size s, look at every pair of notes s steps apart.
> Each pair implies a period. If the largest and smallest implied period differ
> by **less than 1 cent**, accept s, and use the **midpoint** between that
> largest and smallest as the period. Take the smallest s that passes.

That's the whole algorithm. The three choices baked into it:

- **Why a max−min bound and not an average?** An average lets one bad pair
  hide among many good ones. The bound is the "every step agrees on the octave"
  test — which is the intuitive definition of a period.
- **Why the midpoint?** Given that we bounded the worst case, the midpoint of
  [min, max] is the estimate that minimises the worst-case error. (Fancy name:
  minimax / midrange. Nothing deeper than that.)
- **Why 1 cent?** Small enough to be far below hearing (musicians treat ±3–5
  cents as "in tune"), big enough to absorb file rounding (~0.001 c) and MTS
  wire resolution (~0.01 c). See §5 for where these numbers come from.

And one fallback:

> If **no** s passes, treat the whole table — lowest to highest note — as one
> period and tile *that*, instead of clamping. The panel's "NONE (x c)" then
> describes what the engine actually does: x IS the period being tiled.

## 3. What you'd see in the tuning panel

The period row gains one word of honesty, based on the measured spread:

| spread of implied periods | label |
|---|---|
| ≈ 0 (float noise, < 0.01 c) | `1200c · EXACT PERIOD` |
| passes the 1 c rule | `1200c · APPROX PERIOD` |
| nothing passes | `NONE (x c)` — whole-table tiling |

(`SPECIFIED PERIOD` stays for SCALA files, which declare their period.)

## 4. Decisions still open (why this isn't implemented yet)

1. **The tolerance: flat 1 cent, or stricter for tiny periods?** A period is
   *tiled* to fill the wheel table; a tiny period (say 78 c) is tiled ~86×, so
   a 1 c per-period error could compound to tens of cents at the very top
   wheels. The strict version scales the tolerance with the period
   (τ = budget × s / wheels-to-fill). My take: ship the flat 1 c — real
   sources are 100× more precise than the tolerance, so compounding is
   theoretical — and revisit only if a real tuning misbehaves. But it's a
   choice.
2. **Is midpoint-of-extremes the right period estimate**, or would you rather
   average all pairs (least squares)? Midpoint fits the bound logic; average
   is more familiar. Audibly identical for real files.
3. **Multichannel:** inference already runs on the merged gamut, so "more
   notes, one scale" is covered. But should the *dedup tolerance* of the gamut
   merge (currently ~0.1 c) and this tolerance be the same knob?
4. **9ed3halves-type files:** for equal-step scales the smallest period is one
   step (~78 c) — correct but maybe not what you want displayed. Options:
   show the smallest period (current behaviour), or prefer the .kbm's formal
   octave when a mapping file is loaded. (This is display-only; the engine can
   tile either.)

## 5. Background: where the numbers come from

**Hearing.** The just-noticeable pitch difference for successive tones is
roughly constant *in cents* through the musical mid-range: ~1–3 cents for
trained listeners under lab conditions, worse below ~500 Hz and above ~4 kHz
(the curve is U-shaped). The everyday working figure is ~5 cents. A 2012
meta-analysis (583 measurements) adds: longer notes and louder notes are
easier, and listeners vary a lot. Two consequences for us: the Hammond's low
wheels are where the ear is *most* forgiving, and the risky region is the
extended high wheels — which is exactly what the tolerance guards.

**Practice.** Tuners call ±1–3 cents "in tune"; strobe tuners resolve 0.1 c;
`.scl` files carry 3–6 decimals of cents; MTS-ESP resolves ~0.006 c. So file
precision is far finer than perception — which is why the current exact test
fails on *inaudible* rounding, and a ~1 c matcher is more faithful to the
file's intent, not less.

**Prior art.** None found — the estimators are textbook, but "infer a scale's
period from a frequency table" is niche enough that there's no named standard
algorithm to defer to.

## References

- Wier, Jesteadt & Green (1977), *Frequency discrimination as a function of
  frequency and sensation level*, JASA 61(1).
- Moore (1973), *Frequency difference limens for short-duration tones*, JASA 54(3).
- *Characterizing the dependence of pure-tone frequency difference limens on
  frequency, duration, and level* (2012 JASA meta-analysis, PMC3455123).
- Moore, *An Introduction to the Psychology of Hearing*; Zwicker & Fastl,
  *Psychoacoustics*.
- Sethares, *Tuning, Timbre, Spectrum, Scale*.
- Scala format (Huygens-Fokker); ODDSound MTS-ESP; consumer/strobe tuner
  accuracy guides (Carvin Audio, StroboPro).
