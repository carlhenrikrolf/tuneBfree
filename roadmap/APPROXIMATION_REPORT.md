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

## 3. A period-size-aware inference algorithm

### 3.1 Fit, don't chain

For a candidate scale size `s` (steps per period), let the per-pair period in octaves be
`ρ_i = log₂(f[i+s] / f[i])`. The current code effectively takes `ρ_0` and demands every
other pair equal it — errors **chain**. Instead, fit a single period by least squares in
log space, which is just the mean:

```
ρ̂ = mean_i(ρ_i)          P̂ = 2^ρ̂   (geometric mean of the ratios)
residual_i (cents) = 1200·(ρ_i − ρ̂)
```

One global `P̂` cannot compound, because every pair is compared to the same fitted value.

### 3.2 The threshold must scale with period size (your point, formalized)

What we actually care about is the **accumulated pitch error at the top of the range we
tile to**, not the per-step residual. Extending the table adds
`N = NOF_FREQS − 128 = 172` wheels ([tonegen.h:85](../src/tonegen.h#L85)); reaching the
top wheel tiles the period

```
m = N / s           (number of period repetitions in the extension)
```

times, so worst-case accumulated error ≈ `m · ē`, where `ē` is the typical per-period
residual (§3.1). Requiring that to stay within a budget `B` gives a tolerance that
**depends on `s`**:

```
ē_max  =  B · s / N            (per-period residual tolerance, in cents)
```

This is exactly why "~1 cent" only made sense for an octave-ish period:

- Octave EDO, `s = 12`, `B = 5 c`:  `ē_max ≈ 5·12/172 ≈ 0.35 cent`.
- A **78-cent** period spanning `s = 2` steps, same budget: `ē_max ≈ 5·2/172 ≈ 0.06
  cent` — much tighter, because we tile it ~86 times instead of ~14.

So the smaller the period (the smaller `s`), the tighter the per-period residual must be.
A flat cent threshold is wrong; `B·s/N` makes it self-scaling. (The same logic bounds the
*within-table* fit, which tiles `128/s` times — use whichever range is longer.)

### 3.3 Choosing `s` and labelling the result

1. For each `s = 1 … 127`, compute `ρ̂(s)` and `max_i |residual_i|`.
2. Pick the **smallest `s`** whose fit passes `max|residual| < ē_max(s)`. (Smallest, so
   12-EDO reports `s = 12` / period 2:1, not `s = 24`.)
3. Label for the panel's row-5 descriptor:
   - `max|residual|` ≲ 0.01 cent (float noise) → **"specified / exact period"**
   - passes but above that floor → **"approximated period"**
   - nothing passes → **no period** → fall back to **whole-table span as the period**
     (replacing the current clamp — see §4).

`B` should be a single tunable constant; **`B ≈ 3–5 cents`** is well-justified by §1–§2.
Optionally make `B` frequency-aware (looser below ~200 Hz where cents-JND balloons), but
that is almost certainly over-engineering for this use — a flat `B` with the `·s/N`
scaling already captures the important effect.

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
3. **Multichannel** — separate design task; the table is single-channel today
   (channel 0). The fundamental-vs-harmonic question is architectural, not perceptual
   (see the discussion accompanying this report), and is deferred.

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
