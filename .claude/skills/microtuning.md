---
description: Microtuning with MTS-ESP, scale period math, and tuneBfree's tuning architecture
triggers:
  - microtuning
  - MTS-ESP
  - scale period
  - tuning
  - temperament
  - targetRatio
  - Sethares
---

# Microtuning Reference for tuneBfree

tuneBfree extends setBfree with microtuning: the ability to retune all 128 MIDI notes away from 12-tone equal temperament using external tuning tables or internally computed scale periods.

---

## MTS-ESP (Micro-Tuning Standard Exchange Protocol)

MTS-ESP is a plugin-level tuning protocol developed by ODDSound. A **tuning source** (e.g., Scale Workshop, Surge XT, a DAW plugin) publishes per-note frequency tables. tuneBfree registers as a **client** and queries these tables on every audio block.

### Client API (`libs/MTS-ESP/Client/libMTSClient.h`)

```cpp
// Register/deregister
MTSClient* MTS_RegisterClient();
void       MTS_DeregisterClient(MTSClient* client);

// Query frequency for a MIDI note on a given channel (0 = all channels)
double MTS_NoteToFrequency(MTSClient* client, char midinote, char midichannel);

// Check if a master tuning source is connected
bool MTS_HasMaster(MTSClient* client);

// Check if a note should be filtered (silenced) by the tuning source
bool MTS_ShouldFilterNote(MTSClient* client, char midinote, char midichannel);
```

### Usage in audio thread (`plugin/PluginProcessor.cpp`)

tuneBfree tracks **16 channels × 128 notes = 2048 frequency slots** so that per-channel tuning assignments (from a tuning source like Surge XT or Scale Workshop) are detected correctly. Channel is 0-indexed (0 = MIDI ch 1).

```cpp
// In prepareToPlay
mtsClient = MTS_RegisterClient();
memset(previousFrequency, 0, sizeof(previousFrequency));  // double[16][128]

// In processBlock — detect any change across all channels
bool tuningChanged = false;
for (int ch = 0; ch < 16; ch++) {
    for (int note = 0; note < 128; note++) {
        double freq = MTS_NoteToFrequency(mtsClient, (char)note, (char)ch);
        if (freq != previousFrequency[ch][note]) {
            previousFrequency[ch][note] = freq;
            tuningChanged = true;
        }
    }
}
if (tuningChanged) reinitToneGen();

// In releaseResources / destructor
MTS_DeregisterClient(mtsClient);
mtsClient = nullptr;
```

MTS-ESP calls are thread-safe and lock-free; safe to call from the audio thread.

### Multichannel tuning — the gamut model (DONE, tuneBfree 2.0)

Per-channel tuning is implemented via a **merged gamut** (full detail in the `setbfree`
skill and `roadmap/MULTICHANNEL.md`). Short version:

- `buildGamut()` (`src/tuning.cpp`) merges the 16×128 `(channel, note)` frequency grid into
  one ascending, de-duplicated list of the distinct pitches in use, plus a
  `slotIndex[16][128]` map `(channel, note) → gamut slot`. Channels extend **one** scale
  (more notes), not different scales.
- `inferScaleSize`/`extendFrequencies` take a `scaleLen` arg (default 128) so the period is
  inferred over the gamut and the wheel table extends from the gamut size.
- The tonegen wires `gamutSize` slots per manual with a runtime `nofWheels` pool; the
  rebuild is async (off the audio thread). `oscKeyOn` is called with the **gamut slot**, not
  the raw MIDI note.
- **CHANNELS selection** (`channelActive[16]` + OMNI) governs which channels sound, for MTS
  *and* FILE. OMNI OFF: MTS queries channel `i`; FILE uses `_i.kbm` → generic → base `.scl`.
  OMNI ON: MTS queries `-1`, FILE uses the generic mapping. Fewer channels = fewer wheels.

**Vibrato**: shared type across manuals, per-manual on/off (B3-like). Long-term per-note
MPE expression is still out of scope (the wavetable build step makes continuous pitchbend
hard).

### Raspberry Pi note

The standard MTS-ESP shared library may not load on Raspberry Pi because it uses a POSIX shared memory interface that behaves differently on ARM Linux. A fork by Naren Ratan and Baconpaul works on RPi — check whether it supports the latest MTS-ESP spec (scale period API) before choosing between them.

---

## Scale Period and Drawbar Ratios

The Hammond organ generates sound by spinning tonewheels at fixed gear ratios. The standard drawbar layout approximates harmonic overtones of the fundamental:

```
Drawbar  Harmonic  Default ratio  Notes
0        sub-oct    1/2           16' sub-bass
1        sub-3rd   3/2            5⅓' quint
2        fund       1/1           8' fundamental
3        2nd        2/1           4' octave
4        3rd        3/1           2⅔' nazard
5        4th        4/1           2' super octave
6        5th        5/1           1⅗' tierce
7        6th        6/1           1⅓' larigot
8        8th        8/1           1' sifflöte
```

In tuneBfree, the `targetRatio[9]` array in `b_tonegen` defines the exact frequency ratio between each drawbar and the fundamental. These are stored as top/bottom integer pairs (parameters 20–37):

```
ratio = top / bottom
Default: {1/2, 3/2, 1/1, 2/1, 3/1, 4/1, 5/1, 6/1, 8/1}
```

### Scale Period Calculation (`src/tuning.cpp`)

For non-12-TET tunings, the "period" of the scale determines where octaves (and other intervals) fall. tuneBfree calculates appropriate `targetRatio` values so the drawbars align with the actual scale intervals rather than the idealized harmonic series.

Key concepts:
- **Period**: the interval that maps to a frequency doubling (or other ratio). In 12-TET this is 2:1 (octave = 12 semitones). In 10-TET it might still be 2:1 but distributed across 10 steps.
- The MTS-ESP spec includes a scale period API: when present, use it; otherwise fall back to the calculated value from `tuning.cpp`.
- After changing ratios or tuning: request an async rebuild (`requestRebuild()` in
  `PluginProcessor`), which rebuilds the tonewheel bank on the worker thread and swaps it
  in — NOT a synchronous `reinitToneGen()` on the audio thread. `inferScaleSize` only finds
  **exact** periods (within 1e-6); `roadmap/MULTICHANNEL.md` notes the open question of
  approximate-period inference (JND / cents tolerance) for the drawbar quantization.

---

## Timbre and Tuning (Sethares)

William Sethares (*Tuning, Timbre, Spectrum, Scale*) shows that the perceptually "consonant" tuning system depends on the instrument's timbre — specifically, which overtones are present and at what amplitudes.

For a Hammond organ:
- The drawbar settings define the timbre (which overtones are active and at what level).
- The optimal microtuning is not simply 12-TET; it depends on the drawbar mix.
- This means **changing drawbars can affect what tuning sounds most consonant**, and vice versa.

Practical implication: tuneBfree's `targetRatio` parameters allow the user to align the drawbar frequencies with the intervals of their chosen tuning system, bringing the timbre into correspondence with the tuning.

---

## Other Microtuning Sources

### Surge XT tuning-library (integrated)
The Surge XT project's `tuning-library` (Apache 2.0, header-only C++20) is integrated as a git submodule at `libs/tuning-library/`. It handles `.scl` (Scala scale) and `.kbm` (keyboard mapping) file loading, giving users local tuning without needing an MTS-ESP master.

Key API (include `Tunings.h`):

```cpp
#include "Tunings.h"

auto scale = Tunings::readSCLFile("/path/to/file.scl");  // throws on parse error
auto kbm   = Tunings::readKBMFile("/path/to/file.kbm");
auto tuning = Tunings::Tuning(scale, kbm);  // or just Tunings::Tuning(scale)

double freq  = tuning.frequencyForMidiNote(60);           // Hz
double cents = tuning.retuningFromEqualInCentsForMidiNote(60); // deviation from 12-TET
bool mapped  = tuning.isMidiNoteMapped(60);               // false if note is outside KBM range
```

The library only covers MIDI notes 0–127. tuneBfree extends the 128-note table to `NOF_FREQS=300` for the tonewheel generator using `extendFrequencies()` from `src/tuning.h`.

In `PluginProcessor`, local tuning is applied via:
- UI thread writes `localFrequencies[NOF_FREQS]` from the Tunings objects, then sets `localTuningNeedsReinit` (atomic, release ordering).
- Audio thread detects the flag in `processBlock` and calls `reinitToneGen()`, which passes `localFrequencies` as `freqOverride` to `initToneGenerator()`.

GitHub: `surge-synthesizer/tuning-library`

### MIDI Sysex
MTS-ESP supports distributing tuning tables via MIDI sysex. tuneBfree has partial sysex handling; completing it would allow hardware synth-style tuning updates from MIDI hardware.

### Pitchbend / MPE
Continuous pitch modulation via pitchbend or MPE is difficult to implement cleanly in tuneBfree because frequency is baked into the tonewheel synthesis at init time. Each tonewheel runs at a fixed frequency derived from `targetRatio`. Real-time pitch bending would require either: (a) separate oscillators per voice with variable frequency (architectural change), or (b) interpolated tonewheel lookup (complex, potentially expensive). Document this limitation clearly for users.

---

## Tools and Resources

| Tool | Use |
|------|-----|
| Scale Workshop (browser) | Design and export .scl / .kbm files |
| Sevish | Microtonal music production reference |
| Surge XT | MTS-ESP master source for DAW use; also ships tuning-library |
| Exquis (Intuitive Instruments) | Isomorphic MIDI controller with built-in MTS-ESP support |
| MIDI Association | MIDI 2.0 per-note pitch spec |
