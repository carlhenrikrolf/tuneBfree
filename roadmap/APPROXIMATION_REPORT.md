# Approximate Period Inference — Psychophysics & Engineering Report

**Context.** tuneBfree infers a scale *period* (the frequency ratio at which a tuning
repeats) so it can extend the 128-note tuning table out to the 300 tonewheels the
generator needs, and so each drawbar harmonic can be quantized to the nearest available
scale pitch ([tonegen.cpp:765](../src/tonegen.cpp#L765), [tuning.cpp:42](../src/tuning.cpp#L42)).
Today inference is **exact only** — `inferScaleSize` accepts a period only if every pair
`f[i+s]/f[i]` matches to within `1e-6` ([tuning.cpp:58](../src/tuning.cpp#L58)). Real-world
`.scl`/MTS tables are usually quantized to 3-decimal cents or float-rounded, so exact
matching frequently fails and the engine falls back to clamping high tonewheels — a known
bad outcome we intend to replace.

This report covers (1) what perception research says the tolerance *should* be, (2) how
existing audio tools treat tuning tolerance, and (3) a concrete, period-size-aware
algorithm for approximate inference.

---

## 1. Psychophysics: how close is "close enough"?

### 1.1 The frequency difference limen (FDL / pitch JND)

The smallest detectable change between two successive pure tones — the *frequency
difference limen* — is the cleanest perceptual anchor. Classic and modern results agree
on the shape:

- **Weber-law region (~0.5–2 kHz):** the JND is roughly a constant *ratio* Δf/f, i.e.
  roughly **constant in cents**. Wier, Jesteadt & Green (1977) report ≈ 0.1 % at 1 kHz
  for trained listeners in a 2-interval task — about **1.7 cents** (1200·log₂(1.001)).
- **Low frequencies (< ~500 Hz):** Δf becomes roughly *constant in Hz*, so Δf/f — and
  therefore the **cents JND grows** as frequency drops. A few Hz at 100 Hz is tens of
  cents.
- **High frequencies (> ~4 kHz):** discrimination degrades again; cents JND rises.

So the cents-JND curve is **U-shaped**, best (~1–3 cents for trained listeners) in the
0.5–2 kHz region and worse at both extremes.

A 2012 JASA meta-analysis of 583 FDL measurements across 77 listeners fits the dependence
as power laws: FDL rises with **frequency** (exponent ≈ 0.8), falls with **duration**
(exponent ≈ −0.5, the classic inverse-√duration rule), and falls with **sensation
level** (exponent ≈ −1.0, steeply at low levels then shallow). Inter-listener variance
was ~46 % — i.e. *who is listening* matters as much as the stimulus.

**Takeaways for us:**
- Trained-listener FDL bottoms out near **1–3 cents** mid-range.
- The "musically in tune" working figure is **~5 cents** (see §2) — a safe perceptual
  budget for a *single* interval.
- The Hammond's lowest tonewheels (≈ 32–60 Hz) sit where cents-JND is *largest*, so we
  have **more** tolerance down there, not less. The error we most need to control is in
  the **extended high tonewheels**, which are mid/high frequency where the ear is keener.

### 1.2 Why a per-interval budget is not the whole story

The FDL is for *one* comparison. Period inference is different: we **tile** the period
many times to extend the table, so a small per-period misfit **compounds**. A tolerance
that is fine for one octave can drift well past the JND after a dozen repetitions. This
is the crux of §3.

---

## 2. How audio tools treat tuning tolerance

| Tool / format | Effective tolerance | Note |
|---|---|---|
| Needle / clip-on tuners | ±1–3 cents | typical consumer accuracy |
| "In tune" indicator | ±3 cents (common) | what manufacturers call locked |
| Strobe tuners | ±0.1 cent (down to ±0.02) | studio / intonation work |
| Rule-of-thumb JND | ~5 cents | "the ear can't tell below ~5 cents" |
| `.scl` (Scala) files | cents given to 3–6 decimals | text precision, not a perceptual claim |
| MTS-ESP | ~14-bit per semitone (~0.006 cent) | wire resolution, effectively exact |

Two lessons: (a) **musical practice treats ~3–5 cents as "the same pitch,"** while (b)
**file/protocol precision (≤ 0.01 cent) is far finer than perception** — which is exactly
why exact `1e-6` matching fails on rounded files even though the rounding is inaudible.
An approximate matcher with a ~1-cent design point is *more faithful to the source's
intent* than the current exact test, not less.

---

## 3. Inferring the period robustly

> **Is this a standard method?** No. The *estimators* below (minimax/midrange,
> least-squares) are textbook, but inferring a scale period from a frequency table is
> niche enough that there is no off-the-shelf named algorithm. Treat §3 as first
> principles, not a citation.

### 3.1 A bound, not a mean

For a candidate scale size `s` (steps per period), compute the period each adjacent pair
implies, in cents:

```
p_i = 1200 · log₂(f[i+s] / f[i])
```

If the table were exactly periodic all `p_i` would be equal; with rounding they scatter.
The natural test is a **bound on the spread**, not an average:

```
spread(s) = max_i p_i − min_i p_i
accept s   if   spread(s) ≤ τ
```

and take the period as the **midrange**, `P̂ = (max_i p_i + min_i p_i) / 2`. The midrange
is the minimax estimate — it minimises the *worst-case* deviation, which is what a bound
cares about. (A least-squares geometric mean instead minimises RMS and can let a single
outlier pair slip through, which is why it's the wrong tool here.) Pick the **smallest**
`s` that passes, so 12-EDO reports `s = 12`, not 24.

This matches your instinct directly: *a quantised octave is accepted iff every step agrees
on the octave to within τ.* A flat **τ ≈ 1 cent** is a sound default — comfortably below
the ~3–5 cent musical JND (§1–§2) yet loose enough to absorb 14-bit MTS quantisation
(~0.01 c) and cents-rounded `.scl` files.

### 3.2 Optional: tightening τ for very small periods

A flat τ ignores one effect. `extendFrequencies` tiles the period to fill the `N = 172`
wheels above the table ([tonegen.h:85](../src/tonegen.h#L85)), so a per-period error is
re-applied up to `m = N / s` times and the error at the top wheel grows ≈ `m · (per-period
error)`. To *guarantee* the extended top note stays within a budget `B`, tighten the bound
for small `s`:

```
τ(s) = B · s / N
```

Octave (`s=12`, `B=5 c`) → `τ ≈ 0.35 c`; a 78-cent period (`s≈2`) → `τ ≈ 0.06 c` — tighter,
because it is tiled ~86× instead of ~14×. **In practice this is usually moot:** the sources
that actually feed tuneBfree (MTS ~0.01 c, sane `.scl`) sit far under even the flat τ, and
the extended region only matters for the upper harmonics of very high notes. Ship the flat
τ first; add the `·s/N` tightening only if a real tuning trips it.

### 3.3 Labelling the result

1. For each `s = 1 … 127`, compute `spread(s)`.
2. Pick the **smallest** `s` with `spread(s) ≤ τ`.
3. Panel row-5 descriptor:
   - `spread` ≲ 0.01 cent (float noise) → **"specified / exact period"**
   - passes τ but above that floor → **"approximated period"**
   - nothing passes → **no period** → fall back to **whole-table span as the period**
     (replacing the current clamp — see §4).

---

## 4. Recommended changes (in order)

1. **Replace the clamp with the whole-table-as-period fallback** in
   [extendFrequencies](../src/tuning.cpp#L109). When no sub-period is found, treat the
   span from the lowest to the highest *mapped* frequency as one period and tile that.
   This makes the panel's `NONE (x c)` readout truthful and stops high drawbars going
   silent on aperiodic tunings. Small, self-contained, high value.
2. **Add approximate inference** (§3) behind `inferScaleSize`, returning the period, the
   max residual, and the exact/approx/none label. Keep the exact path as the residual ≈ 0
   case.
3. **Multichannel = more notes, one scale.** Different MIDI channels carry different
   keyboard *mappings* into the *same* scale, to address > 128 notes — e.g. two staggered
   keyboards splitting 24-EDO (naturals on one channel, quarter-tones on the other), or a
   Lumatone. Implement by merging every mapped `(note, channel)` pitch into one ascending,
   de-duplicated frequency table (the full gamut, with a small cents tolerance for the
   dedup), then run §3 inference and the step-1 extension on that merged table. Routing then
   needs the channel — today [`oscKeyOn`](../src/tonegen.h#L593) takes only a note number
   and the table is built from channel 0 alone ([tuning.cpp:19](../src/tuning.cpp#L19)) —
   and `NOF_FREQS` (300) may need raising for large gamuts. Per-*channel* different
   *scales* (kora+bala, gamelan+flute) are explicitly **out of scope**: assume one scale.

---

## References

Psychoacoustics
- Wier, C. C., Jesteadt, W., & Green, D. M. (1977). *Frequency discrimination as a
  function of frequency and sensation level.* J. Acoust. Soc. Am. 61(1), 178–184.
- Moore, B. C. J. (1973). *Frequency difference limens for short-duration tones.* J.
  Acoust. Soc. Am. 54(3), 610–619.
- *Characterizing the dependence of pure-tone frequency difference limens on frequency,
  duration, and level* (2012 JASA meta-analysis), PMC3455123.
- Moore, B. C. J. *An Introduction to the Psychology of Hearing.*
- Zwicker, E. & Fastl, H. *Psychoacoustics: Facts and Models.*
- Sethares, W. *Tuning, Timbre, Spectrum, Scale* (timbre↔tuning coupling; see the
  microtuning skill).

Tools / formats
- Carvin Audio, "Guitar Tuner Accuracy — How Accurate is Enough?"
- StroboPro, online strobe-tuner accuracy guide.
- Scala scale-file format (Huygens-Fokker); ODDSound MTS-ESP.

Web sources consulted:
- https://www.fon.hum.uva.nl/praat/manual/Jesteadt__Wier___Green__1977_.html
- https://pmc.ncbi.nlm.nih.gov/articles/PMC3455123/
- https://ccrma.stanford.edu/CCRMA/Courses/152/perceptual.html
- https://carvinaudio.com/blogs/guitar-bass-education/guitar-tuner-accuracy-how-accurate-is-enough
- https://strobopro.se/help.html
