# Parameter Audit — what is exposed, hidden, dead, or stubbed

Audited 2026-07-04 against the setBfree config documentation (the Debian man
page lists the same `.cfg` keys) and the current APVTS parameter set.

Legend:
- **exposed** — a GUI control / plugin parameter exists (page named)
- **hidden** — the DSP supports it, nothing exposes it (candidates for later)
- **dead** — documented but no longer connected to anything in THIS fork
- **superseded** — replaced by a tuneBfree subsystem
- **stubbed** — config parsing is a no-op in the plugin (midi_stubs.cpp);
  planned CONTROL-panel territory

## Tone generator (`osc.*`)

| Parameter | Status |
|---|---|
| osc.tuning, osc.temperament | superseded — the tuning system (MTS/SYSEX/SCALA/STANDARD) owns pitch |
| osc.perc.fast / slow / gain / normal / soft | exposed (TINKER · PERCUSSION) |
| osc.perc.bus.a / bus.b / bus.trig | hidden (percussion bus routing) |
| osc.eq.macro | hidden (only `chspline` is used; `peak24/46` deprecated upstream) |
| osc.eq.p1y / r1y / p4y / r4y | exposed (TINKER · TONE) |
| osc.eqv.ceiling, osc.eqv.\<n\> | hidden — per-wheel EQ table; needs an editor, not a knob |
| osc.harmonic.\<h\> | exposed as the WAVE presets (sine/square/triangle); free tables hidden |
| osc.harmonic.w\<n\>.f\<h\> | hidden — per-wheel harmonics table |
| osc.compartment/transformer/terminalstrip/wiring-crosstalk | exposed (TINKER · CROSSTALK) |
| osc.terminal.* / osc.taper.* / osc.crosstalk.k\<x\> | hidden — "rewire the organ" tables; .cfg-file territory |
| osc.contribution-floor / contribution-min | hidden |
| osc.attack.model / release.model | exposed (TINKER · KEY CLICK) |
| osc.attack.click.level / minlength / maxlength, osc.release.click.level | exposed (TINKER · KEY CLICK) |
| osc.x-precision | hidden (experimental memory/quality trade) |
| osc.psdump | debug-only, N/A |
| drawbar harmonics (Naren's ratio_top/bot) | superseded — HARMONICS cents + AUTO/CUSTOM (params 20–37) |

## Vibrato scanner (`scanner.*`)

All exposed (TINKER · SCANNER): scanner.hz, modulation.v1/v2/v3.

## Rotary speaker (`whirl.*`)

| Parameter | Status |
|---|---|
| slowrpm/fastrpm/acceleration/deceleration/brakepos (horn + drum) | exposed (ROTOR · MOTORS) |
| horn/drum filter type/hz/q/gain (a, b, drum) | exposed (ROTOR · FILTERS) |
| horn.level / horn.leak | exposed (ROTOR · MIC & CABINET) |
| mic distance / horn.mic.angle / mic widths | exposed (ROTOR · MIC & CABINET) |
| whirl.bypass | exposed (PLAY · LESLIE) |
| whirl.speed-preset | covered by PLAY / ROTOR speed switches |
| whirl.horn.radius / drum.radius, horn.offset.x / offset.z | hidden (cheap to add if wanted) |
| whirl.horn.comb.a/b (feedback, delay) | dead — compiled out upstream too (`HORN_COMB_FILTER`) |

## Overdrive (`overdrive.*`, `xov.*`)

| Parameter | Status |
|---|---|
| overdrive.enable + overdrive.character | exposed (PLAY · DRIVE; character = the "fat" macro) |
| overdrive.inputgain / outputgain | exposed (TINKER · PREAMP IN/OUT) |
| xov.ctl_biased_fb / _fb2 | exposed (TINKER · BASS PRE / BASS POST) |
| xov.ctl_sagtobias | exposed (TINKER · SAG) |
| xov.ctl_biased (bias base), xov.ctl_biased_gfb (global feedback) | hidden |

## Reverb (`reverb.*`) — special case, see the chapter below

| Parameter | Status |
|---|---|
| reverb.mix | exposed (PLAY · REVERB) |
| reverb.wet / dry / inputgain / outputgain | **dead** — they belonged to setBfree's original reverb, which this fork REPLACED; the doc table in reverb.cpp (and the man page) is stale |
| MatrixVerb Filter/Damping/Speed/Vibrato/RmSize/Flavor | hidden — real, live-settable, frozen at defaults (prime TINKER candidates) |

## Not DSP

- `midi.*` (controller mapping), `pgm.*` (programs): **stubbed** — CONTROL panel scope.
- `main.*`, `audio.*`, `jack.*`, `midi.driver/port`: host concerns, N/A under JUCE.

---

# The reverb chapter

## What we actually ship

setBfree's original Schroeder reverb is **gone from this fork**. Naren replaced
it with **Airwindows MatrixVerb** (Chris Johnson, MIT-licensed — see the header
of `src/reverb.cpp`), a Householder-matrix feedback-delay-network reverb, and
wired up only its dry/wet as `reverb.mix`. That is why the man-page reverb
parameters do nothing: they document the removed unit.

MatrixVerb's real controls, currently hardcoded (`src/reverb.cpp`, initValues):

| Knob | Default | What it does |
|---|---|---|
| Filter (A) | 1.0 | input lowpass (1.0 = fully open) |
| Damping (B) | 0.2 | high-frequency decay of the tail — more = darker, shorter-feeling |
| Speed (C) | 0.0 | modulation rate of the delay network |
| Vibrato (D) | 0.0 | modulation depth — the tail is currently completely STATIC |
| RmSize (E) | 0.4 | scales every delay length (`delayX * size` in the code) — small = tight/boxy/metallic, large = hall |
| Flavor (F) | 0.8 | morphs the feedback-matrix character (tail density/colour) |
| Dry/Wet (G) | 0.1 | the PLAY REVERB knob |

**On "maybe I'd like it better with smaller RmSize or more damping":** very
plausible — and note the tail is currently unmodulated (Speed/Vibrato = 0),
which is a classic recipe for the metallic/static quality people dislike in
FDN reverbs. A touch of Vibrato, more Damping, and a smaller room may
transform it. Small RmSize + some Vibrato even drifts toward spring-adjacent
character. Recommendation: expose these six on TINKER and re-judge before
adding any new algorithm.

**Relation to MVerb:** none. MVerb (Martin Eastwood, GPL) is a
Dattorro-style *plate* reverb; MatrixVerb (Airwindows) is a Householder
*matrix* FDN. The similar names are coincidence — different authors,
algorithms, and licenses.

## Alternatives, if MatrixVerb still doesn't please

| Option | Effort | RPi | Notes |
|---|---|---|---|
| Expose the 6 MatrixVerb knobs | trivial | fine | live setters, no rebuild; do this first |
| Freeverb (public domain Schroeder) | small | trivial | the "better Schroeder" ask — though arguably a step down from MatrixVerb in quality; different character |
| Zita-rev1 (Fons Adriaensen, GPL) | moderate | proven | high-quality FDN hall/room; the serious algorithmic upgrade |
| Convolution spring: JUCE `dsp::Convolution` + CC0 spring IRs | moderate | plausible, must measure | most authentic spring *sound*; linear only (no driven-spring growl); note the chain currently contains no convolution anywhere (the Leslie is filters, not an IR) |
| Algorithmic spring (Parker/Välimäki chirp-allpass model) | large | fine | true spring *behaviour*; no GPL-compatible drop-in known — would be implemented from the papers |

A REVERB TYPE selector (PLAY gets the second knob the user pondered; TINKER
gets the per-type detail) is the natural GUI shape once >1 engine exists.

---

# Upstream setBfree sync (checked 2026-07-04, network fetch)

Fork point `45f74b3` → upstream `master` (`82931a9`): **only 8 commits**, of
which six are build/UI/JACK plumbing. The two that matter:

1. **`6efe51e` "Use a LPF for smooth swell-pedal gain changes (#96)"** — adds a
   ~25 Hz one-pole smoother on the swell gain inside `oscGenerateFragment`.
   Our fork lacks this: EXPRESSION / CC 7/11 changes are applied as raw gain
   steps (audible zipper on fast pedal moves). **Recommended port** — small,
   self-contained (`targetGain`/`currentGain`/`gainTimeConstant`).
2. **`82931a9` "remove hardcoded preamp bias"** — deletes an `adwFb = 0.5821`
   override in initPreamp so the config value isn't clobbered. Our
   `src/overdrive.cpp:595` has the same line; in tuneBfree it is harmless
   (the BASS PRE parameter re-applies after init) but mirroring the removal
   is correct hygiene.

Everything else upstream (effects-as-LV2-plugins layout, robtk/pugl, JACK
autoconnect) is packaging we don't use. **Conclusion: no merge; tonegen is
permanently diverged. Both patches above were ported on 2026-07-04** (swell
LPF incl. carrying the smoothed gain across engine rebuilds; bias-clobber
line removed).

## What the upstream b_* directories are

Same DSP we already vendored, wrapped as individual LV2 plugins — plus two
things we never had:

| Dir | Contents |
|---|---|
| b_synth | the organ itself as LV2 + the old OpenGL GUI (superseded by our JUCE plugin) |
| b_whirl | the Leslie as standalone LV2 (= x42-whirl) — same whirl DSP as src/whirl.cpp |
| b_overdrive | the preamp/overdrive as standalone LV2 — same overdrive DSP |
| b_chorato | the vibrato/chorus scanner as standalone LV2 (2018) — same vibrato DSP |
| b_reverb | **the ORIGINAL setBfree Schroeder reverb** (the man page's wet/dry/in/out unit) — still alive upstream; our fork replaced the in-engine copy with MatrixVerb |
| b_conv | **experimental convolution cabinet** with bundled Leslie IRs (ir_leslie-44100/48000.wav), bypassed by default upstream — never brought into the fork. Relevant to the spring/convolution reverb discussion: the IRs (and the "cabinet has an IR" memory) live HERE, not in our chain |
