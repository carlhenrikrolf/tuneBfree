# Multichannel

> [!note]
> Here, we use 1-indexing for midi channels. In code, 0-indexing is often better. 1-indexing is better for ease of reading.

## MIDI Channels

- MIDI channels are usually used for different timbres.
- Use in microtuning is different.
- Use the 16 midi channels to extend 128 notes to 16*128 notes.
- An example is Lumatone with a lot of keys.
- Personally I use two different digital pianos stacked on top of each other.
- E.g., 24edo would have all the semitones on the upper digital piano and all the quarter tones on the lower. The upper would have midi channel 1 and the lower 2.
- Using this means that the old multi-timbre framework (for both manuals and the pedals) in setbfree must be removed.

## Bitimbrality

- Normally organs have different channels for different manuals (and pedals).
- E.g. MIDI channel 1 for the upper manual, 2 for the lower manual, and 3 for the pedals.
- The reason is that they are often set to different timbres.
- E.g. 16' | 8' for the lower manual and 16' | 5 1/3' | 8' | 4'  for the upper could be a setting for a jazzy/bluesy Hammond organ.
- Given that we use the midi channels for tuning, we loose the ability to separate the timbres.
- I want to bring back using different manuals in some form but it cannot be channel based.
- An idea is to use keyboard split instead.
- There is already a keyboard split in setbfree.
- The problem is that I believe it's an abrupt keyboard split.
- This is ok if your bass and treble are well separated, but for me they often are not.
- Nord keyboards and Korg prologue can use a crossfade to deal with this.
- I don't know to what extent it would be possible to implement a crossfade based on the current implementation, but I would like it.
- vibrato chorus is already different between upper and lower manual and this should be part of the split.
- percussion is typically only applied to the upper manual and should be part of the split. I think it would be good if you could allow it for the lower split as well and that these are independent.
- drive, reverb leslie are effects that come afterwards and are not affected.
- You can ignore the pedals, I don't need a 3 way split, a 2 way split is enough.

## Computational Efficiency

- This plugin is meant to run on a raspberry pi concurrently with other plugins.
- Therefore, it cannot be too wasteful in terms of computational power.
- A naïve approach of increasing the number of channels to 16 would be to simply use several copies of the engine.
- That can clearly not be done with these constraints.
- So, maybe it's better to extend the frequency table?
- One inefficiency that has been noted with respect to setbfree is that it is always on.
- Even when not running it is consuming cpu, see [this issue](https://github.com/pantherb/setBfree/issues/73).
- A better solution would be if only the notes that are played are consuming CPU (maybe on top of some small always on usage).
- If CPU usage scales with number of active notes that's not really a problem in practice.
- I don't know if the current implementation would allow that.
- For an alternative Hammond organ implementation [see Collab3](https://github.com/augustofilocamo/superOrgan), but I don't know if that's better.
- [Aeoulus](https://github.com/Archie3d/aeolus_plugin) is quite light on cpu but it's a pipe organ.

## Extras
- Panic button. not sure where, but could be put to the left of the tuning button. maybe make the sizes of those two buttons consistent with the rest of the gui. This is a temporary solution.
- Claude, you are right that the swell pedal is only volume, but you are wrong that that's the case on the real hammond at least according to [this issue](https://github.com/pantherb/setBfree/issues/92).

---

# Feasibility review & plan (Claude, 2026-06-29)

Annotated after tracing the spec through the actual engine. Read alongside the
code references (paths relative to this doc).

## Headline

The "extend the frequency table, don't clone the engine" instinct is correct — the
codebase is already built that way. There is a single shared tonewheel pool
(`NOF_WHEELS`, [tonegen.h:79](../src/tonegen.h#L79)) and a frequency table
(`NOF_FREQS`, [tonegen.h:85](../src/tonegen.h#L85)) that
[`extendFrequencies()`](../src/tuning.cpp#L109) tiles by period. Per-sample CPU
already scales with **active** wheels, not total wheels. So the efficiency model
you want is essentially the existing design. The remaining work is **indexing and
routing**, not DSP cost — with two real exceptions (wheel-pool size, and reinit
location; see below).

Bonus: **two independent manuals already exist inside one tonegen instance**
(upper = buses 0–8, lower = buses 9–17, with independent drawbars and independent
vibrato routing). The legacy channel-based split in
[midi.cpp:728](../src/midi.cpp#L728) is **not compiled** into the JUCE build —
PluginProcessor does its own MIDI. So "remove the old multi-timbre framework" is
mostly already done; the split just needs reimplementing at the JUCE/tonegen layer.

## Decisions locked in (Q&A 2026-06-29)

1. **Split basis = sounding pitch.** The manual split point is a frequency in the
   merged gamut, not a raw MIDI note. Low pitches on either keyboard → lower
   manual, high → upper. (Consequence: each physical keyboard is itself split.)
2. **Crossfade = set-and-forget config.** Split point + crossfade width are config
   settings; changing them triggers a tonegen reinit (a few ms, like a tuning
   change). Implemented by **baking complementary gain weights into the key wiring**
   (`keyTaper` levels, [tonegen.cpp:793](../src/tonegen.cpp#L793)). No live-draggable
   crossfade in v1 (that would need a per-note runtime gain stage the additive
   engine lacks).
3. **16 channels desired** → recommend **Solution B** (size the wheel pool to the
   actual gamut). See tradeoffs below.
4. **Per-split vibrato + percussion — keep it B3-like** (user, 2026-06-30, change of
   heart, superseding the earlier "independent" idea): **percussion = upper manual only**;
   vibrato/chorus **type shared** across manuals but **on/off per manual** (engine already
   does this via `RT_UPPRVIB`/`RT_LOWRVIB`). No 2nd `b_vibrato`, no 2nd percussion
   envelope. GUI tweaks here may come later (not urgent).

## Section-by-section feasibility

### Multichannel tuning (16×128 → one gamut)

- **Central conflict:** the engine's `keyNumber` dimension currently encodes *both*
  the manual (offset 0/128/256 → bus range) *and* the pitch (`frequency[k]` is "MIDI
  note k's fundamental", [tonegen.cpp:765-777](../src/tonegen.cpp#L765-L777)).
  Multichannel needs `(channel, note) → distinct fundamental`, while the split needs
  `(region) → manual`. These are orthogonal but share one index today → needs a
  2-axis rework (pitch slot × manual).
- **Hardcoded to channel 0 today:** [`getMTSESPFrequencies()`](../src/tuning.cpp#L25)
  calls `MTS_NoteToFrequency(client, i, 0)`, and
  [oscKeyOn](../plugin/PluginProcessor.cpp#L456) passes `noteNumber` while ignoring
  `msg.getChannel()`. Change-*detection* is already 16-channel-aware
  ([PluginProcessor.cpp:373-381](../plugin/PluginProcessor.cpp#L373-L381)); playback
  is the missing half.
- **Array sizing:** `MAX_KEYS = 384` ([tonegen.h:93](../src/tonegen.h#L93)) plus many
  `< 128` / `MAX_KEYS`-bounded loops and `keyTaper/keyContrib/activeKeys` arrays must
  grow, plus the doctests that hardcode those bounds. Init-time memory, not per-sample.
- **Reframe (important):** the wheel pool scales with the number of **distinct
  pitches** in the tuning, *not* the channel count. 16 channels of 24-EDO landing on
  the same pitch set need the same wheels as 2 channels. Channels only multiply
  wheels when each adds genuinely new pitches.

### Bitimbral keyboard split

- **Cheap part (free):** independent drawbars + vibrato routing per manual — engine
  already sums upper/lower separately and routes each to the scanner via
  `RT_UPPRVIB`/`RT_LOWRVIB` ([tonegen.cpp:1635](../src/tonegen.cpp#L1635),
  [:1650](../src/tonegen.cpp#L1650)).
- **Crossfade:** static/baked = feasible (weights into `keyTaper` at init; split-point
  change = reinit). A note in the overlap sounds on both manual-keys with complementary
  gains. Minor side effect: it counts twice in `keyDownCount` (key-compression loudness).
- **Vibrato type is global today:** one scanner ([vibrato.h](../src/vibrato.h),
  `setVibratoFromInt`). Independent type per manual = a **second `b_vibrato`** + a second
  per-oscillator vib accumulator threaded through the **core interpreter** (`CoreIns`,
  [tonegen.cpp:114](../src/tonegen.cpp#L114); emit at
  [3491-3510](../src/tonegen.cpp#L3491-L3510)). CPU cost: *little* (scanner is light,
  only when both routed). Code cost: *moderate but high test-risk* — it's the hottest,
  hairiest loop in the project (envelope + buffer-wrap special cases). Hence staged.
- **Percussion is upper-only:** tied to `upperKeyCount` and a single `percEnvGain`
  ([tonegen.cpp:3154](../src/tonegen.cpp#L3154)). Independent percussion on the lower
  split needs a second percussion envelope + trigger — moderate new code.

### Extras

- **Panic:** easy, and closes a FIELD_NOTES TODO. Processor only handles CC 7/11 today
  ([PluginProcessor.cpp:432](../plugin/PluginProcessor.cpp#L432)); add CC 120/123 + a GUI
  button (release all `activeKeys`, reset `activeNoteCount`).
- **Swell pedal / issue #92:** current code makes it a pure gain
  ([PluginProcessor.cpp:291](../plugin/PluginProcessor.cpp#L291)). If the real Hammond
  expression pedal has a loudness/EQ contour that shifts with level, that's a *new* DSP
  feature, scoped separately.

## Wheel-pool sizing — three solutions

Calibration: current pool is `NOF_WHEELS = 256`. Rough wheels-needed ≈ (notes per
period) × ~13 octaves of fundamentals+harmonics:

| Scale | ≈ distinct wheels | vs. current 256 |
|---|---|---|
| 12-EDO | ~160 | fits |
| **24-EDO (two pianos)** | **~310** | **already over** |
| 53-EDO | ~690 | needs ~3× |
| Lumatone-dense / non-octave | 1000+ | needs ~4×+ |

Costs that grow with pool size: **idle RAM** (one wavetable per wheel) and **reinit
time** (matching loop is O(keys × 9 buses × `NOF_WHEELS`) + wavetable building).
Per-sample cost is unaffected (only active wheels are processed).

- **A — Fixed generous pool.** Bump `NOF_WHEELS`/`NOF_FREQS` to e.g. 1024. Trivial code;
  pays full init + idle RAM even for 12-EDO. Stopgap only.
- **B — Size to the actual gamut (RECOMMENDED).** At reinit, use the inferred scale
  size/period ([inferScaleSize](../src/tuning.cpp#L42)) to allocate only the wheels the
  current tuning needs (~160 for 12-EDO, ~700 for 53-EDO), capped at a ceiling. Cheap
  when simple, scales only when needed. Natural extension of `extendFrequencies`.
- **C — Two-tier (shared harmonic pool + per-voice exact fundamental).** Bounded shared
  pool for harmonics (approximate, as today); each *sounding* note gets its own
  exactly-tuned fundamental wheel allocated at note-on. CPU/RAM track notes held, not
  gamut size — handles arbitrarily fine/non-octave 16-channel tunings without compromise.
  Biggest rework: adds a per-voice oscillator lifecycle at note-on that doesn't exist
  today (notes currently wire all partials to shared wheels at init).

Read: **B** for "16 channels, reasonable scales"; **C** if you want unrestricted
16-channel fineness and will pay for voice allocation; **A** only as a stopgap.

## Cross-cutting risk: reinit runs on the audio thread

`reinitToneGen()` calls `freeToneGenerator` + `allocTonegen` + `initToneGenerator`
(mallocs + wavetable building) **directly from `processBlock`**
([PluginProcessor.cpp:394](../plugin/PluginProcessor.cpp#L394),
[:429](../plugin/PluginProcessor.cpp#L429)), and fires on every tuning change, ratio
tweak, and file load. Survivable today at 256 wheels / 128 keys. With a multichannel
gamut and a 700–1024 wheel pool the rebuild balloons (matching loop goes from ~300K to
tens of millions of iterations) → likely glitch/xrun on every drawbar-ratio nudge,
especially on the Pi. **Move reinit to a background build + atomic pointer swap.**
Highest-priority structural fix before multichannel lands.

Also note: the always-on MTS scan
([PluginProcessor.cpp:373-381](../plugin/PluginProcessor.cpp#L373-L381)) calls
`MTS_NoteToFrequency` 2048× every block regardless of notes — fixed Pi overhead worth
gating (scan only channels in use / throttle). It's also skipped during the silence
early-return (a separate latent bug noted in FIELD_NOTES).

## Suggested phased order

0. **Move reinit off the audio thread** (background build + atomic swap). Prereq for
   everything below; also fixes an existing latent xrun risk.
   **DONE (2026-06-29):** `reinitToneGen()` replaced by an async rebuild in
   [PluginProcessor.cpp](../plugin/PluginProcessor.cpp) — a `RebuildThread` builds a
   fresh `b_tonegen` (`performBackgroundRebuild`), the audio thread swaps it in
   (`applyPendingRebuild`, a pointer swap at the top of `processBlock`) and hands the
   old engine back to the worker to free. Tuning/ratio/file changes now call
   `requestRebuild()` (flag + wake) instead of building inline; the old engine keeps
   playing until the swap. First build (`initDSP`) stays synchronous (message thread,
   pre-playback). *Caveat for later:* teardown joins the worker with a 2 s timeout —
   fine at today's 256-wheel pool, revisit when Solution B grows the pool.
1. **Per-channel tuning playback.** Un-hardcode channel 0; map `(channel, note)` →
   fundamental; grow `MAX_KEYS`/table bounds. Solution **B** wheel-pool sizing.
   **In progress (2026-06-29):**
   - ✅ `buildGamut()` ([tuning.cpp](../src/tuning.cpp), declared in
     [tuning.h](../src/tuning.h)) — merges the 16×128 (channel, note) grid into one
     ascending, de-duplicated gamut + a `slotIndex[16][128]` map; honours the active-channel
     mask and per-note filtering; single-channel case reduces to identity. Unit-tested
     (5 doctest cases: identity, 24-EDO interleave, dedup, unmapped-note exclusion, empty).
   - ✅ Generalized `inferScaleSize`/`extendFrequencies` with a `scaleLen` arg (default
     128, so existing callers are untouched) so the wheel pool extends from the gamut
     size. Unit-tested.
   - ✅ `slotIndex[16][128]` + `gamutSize` added to `b_tonegen` (rides with the engine,
     swaps atomically); `initToneGenerator` identity-inits them so non-multichannel
     sources are unchanged.
   - ✅ Worker rebuild: `buildMTSGamut()` ([PluginProcessor.cpp](../plugin/PluginProcessor.cpp))
     queries MTS over the active channels → `buildGamut` → fills the tonewheel table +
     `slotIndex`; runs on the rebuild worker.
   - ✅ Note routing in `processBlock`: `(channel, note)` → `synth->slotIndex` →
     `oscKeyOn(slot)`; per-`(channel,note)` `soundingSlot[16][128]` tracks the held slot so
     note-off releases the exact key, reset on engine swap. (Replaces channel-agnostic
     `filteredNotes[]`.)
   - ✅ `channelActive[16]` state (default all-active = current behaviour).
   - ✅ **Gamut cap lifted to MAX_GAMUT = 2048 (step 1b, 2026-06-30).** See below.
   - ✅ Per-channel `.kbm` (FILE multichannel): load N `.kbm` files (multi-select).
     `buildFileGamut()` merges them like the MTS path. Editor `.kbm` chooser multi-selects.
     This makes multichannel testable on Mac without a multichannel MTS master.
   - ✅ **`.kbm` assignment (revised 2026-06-30, no alphabetical order):** `*_i.kbm`
     (i in 1..16) → channel i (last selected wins for the same i); a file with no valid
     `_i` suffix is a **default** filling every unassigned channel. `.scl` only = base
     scale on all channels. (`kbmChannelSuffix` + `loadKBMFiles`.)
   - ✅ **Test-C fixes (2026-06-30):** (a) **slot reference counting** — coincident
     pitches across channels de-dup to one gamut slot, so the slot is now ref-counted:
     `oscKeyOn` on the first key, `oscKeyOff` only on the last release (fixes "release one
     key → both stop" and the retrigger click). (b) **Channel-aware Hz/cents read-out** —
     the panel shows each note's *actual* sounding frequency (`synth->frequency[slot]`),
     so two manuals at different pitches read out differently.
   - ✅ **Panic** — PANIC button (left of TUNING) + MIDI CC 120/123 release all notes
     (`allNotesOff`, via an atomic `panicRequested`). Temporary, for debugging.
   - ✅ **CHANNELS popup UI (2026-06-30):** the CHANNELS button (right of the encoding
     menu) opens a CallOutBox with **POLY** (multi- vs single-select), **OMNI**, and 16
     channel toggles (`ChannelSelectorContent`). Wired to `setChannelActive/setOmni/setPoly`;
     each change triggers a rebuild. OMNI merges to the MTS **unspecified channel (-1)** —
     applies to MTS/SYSEX; FILE keeps its per-channel `.kbm` mapping. Channel config
     (active mask + omni + poly) is **persisted** in plugin state (`channelConfig` node).
     Deferred nicety: POLY does not yet remember the *other* mode's last selection.
   - ✅ **step 1b (2026-06-30): keyspace widened + Solution B wheel-pool sizing.**
     The per-manual slot count and the tonewheel count are now **runtime** (`b_tonegen::
     gamutSize`, `nofWheels`), passed into `initToneGenerator`; the manual stride is
     `gamutSize` (upper [0,gs), lower [gs,2gs), pedal [2gs,3gs)). Compile-time maxima
     set to the hard ceiling: `MAX_GAMUT 2048` (= 16×128, every channel/note unique — the
     most distinct pitches MTS can address), `MAX_KEYS 3*2048`, `NOF_WHEELS`/`NOF_FREQS`
     → 2048. `computeNofWheels` sizes the pool to the gamut + ~8× the top pitch (so 12-EDO
     builds ~160 wheels, not 2048). Defaults (128/256) keep the single-channel path
     byte-identical — all prior doctests pass; a new doctest wires a 160-slot gamut.
     The cap **can never silently drop pitches** now; rebuild cost scales with the
     *runtime* gamut, which the user bounds via the CHANNELS selection (the design intent).
     A maxed-out gamut (~2048) takes ~1–2 s to rebuild (async — no glitch); the worker
     stack is 4 MB and teardown waits 8 s to accommodate it. *Future:* the wheel-matching
     is a linear scan — a binary search (frequencies are sorted) would make large gamuts
     rebuild near-instantly.
   - ✅ **Wire upper-manual only (2026-06-30):** `applyDefaultConfiguration` no longer wires
     the unused lower/pedal manuals — cuts rebuild work to ⅓. The keyboard split (step 2)
     re-adds the lower manual.
   - ⬜ `.kbm` directory selection (multi-file select works; directory scan is a nicety).

   **Status: builds clean (Standalone), unit tests 47/47. Needs DAW validation — see
   [MULTICHANNEL_TESTING.md](MULTICHANNEL_TESTING.md).**
2. **Bitimbral split, abrupt first.** Split-by-sounding-pitch → route region to
   upper/lower manual keys. Independent drawbars + vibrato on/off come free.
   Percussion stays upper-only; vibrato/chorus type shared (B3-like, per the 2026-06-30
   change of heart — no per-split percussion, no independent vibrato type).
3. **Baked crossfade.** Complementary `keyTaper` gains across the overlap zone;
   split point/width as config (reinit on change).

   (Dropped: the earlier steps 4 "independent percussion on lower split" and 5
   "independent vibrato type" — superseded by the B3-like decision above.)
6. **Extras:** panic (CC 120/123 + button); swell-pedal contour (#92) separately.

## Update (2026-06-29 #2) — channel selection, idle cost, GUI units

Follow-up after the [TUNING_PANEL.md](TUNING_PANEL.md) CHANNELS popup was added.

- **Channel-set selection composes with Solution B.** The CHANNELS popup (POLY/OMNI
  + per-channel checkboxes) defines which channels feed the merged gamut; B sizes the
  wheel pool to that gamut. Gamut = pitches from *selected* channels, merged → sorted →
  de-duplicated, so identical pitches across channels collapse to one wheel (no
  duplicates). Changing the channel set = a reinit (config-time).
  - **OMNI** = query MTS with channel **-1**, which always reads the single
    (non-multichannel) global table: [`freq()`](../libs/MTS-ESP/Client/libMTSClient.cpp#L222)
    sets `supportsMultiChannelTuning = !(midichannel & ~15)` ([:228](../libs/MTS-ESP/Client/libMTSClient.cpp#L228)),
    so `-1` skips the per-channel branch and falls through to
    `esp_retuning[note]` ([:244](../libs/MTS-ESP/Client/libMTSClient.cpp#L244)).
    **NB (user, RPi dylib):** with a master that has *no* multichannel tables (the
    common case), `-1` and channel `0` return the *identical* single table — so "-1 is
    better than channel 1" is moot there; the two only diverge with a multichannel-capable
    master, where `-1` correctly stays on the single global table. `-1` is the right OMNI
    choice regardless. **POLY off** = a single selected channel.
  - **Notes on unselected channels → silenced** (assumption; alternative is to fall
    through to the unspecified table).

- **Issue [#73](https://github.com/pantherb/setBfree/issues/73) (idle CPU), answered:**
  - Stock setBfree runs the whole chain forever; the Leslie ([whirl.cpp](../src/whirl.cpp))
    and reverb advance internal LFO/delay state every sample regardless of input, so a
    silent buffer costs as much as a loud one — that's the idle burn.
  - tuneBfree already mitigates it: after ~3 s idle, processBlock clears the buffer and
    returns *before* any DSP ([PluginProcessor.cpp:400-403](../plugin/PluginProcessor.cpp#L400-L403)).
  - Residual idle cost = the per-block MTS scan
    ([2048 `MTS_NoteToFrequency` calls](../plugin/PluginProcessor.cpp#L373-L381)) + a cheap
    param-diff loop, both before the early-return. **Fix:** scan only *selected* channels
    and throttle the idle scan to ~10 Hz. Cuts residual idle to near-zero AND fixes the
    FIELD_NOTES "MTS master restart while idle not detected" bug (throttled scan keeps
    polling during silence). The first ~3 s after the last note intentionally keeps the
    chain running so reverb/Leslie tails decay.

- **GUI units for the split:** split point expressed as a **frequency (Hz)** in the
  merged gamut (MIDI note number is ambiguous across channels/tunings; frequency is not),
  snapped to / displaying the nearest gamut pitch (+ nearest note name). Crossfade width
  as **cents** (tuning-independent). Crossfade curve = **equal-power** (constant perceived
  loudness through the overlap). Optional ergonomic "set split from last note-on" button
  (panel already shows last/penultimate note frequency).

- **Persistence:** channel set + split point + crossfade width saved in plugin state.

- **Per-manual vibrato TYPE: deferred** (per user). Keep per-manual vibrato on/off (free);
  do not build the second `b_vibrato` / core-interpreter change for now.

Net effect on the phased order: fold channel-set gating into step 1 (scan + gamut from
selected channels, throttled idle scan), keep equal-power baked crossfade in step 3, and
drop step 5 (independent vibrato type) to "parked / maybe later."