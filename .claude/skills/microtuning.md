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

### Single manual and per-channel tuning (Phase 2 work)

tuneBfree uses a **single upper manual** (removing the 3-manual MIDI channel split). This creates a tension with per-channel MTS-ESP tuning: when notes arrive on different MIDI channels with different tunings, the tonegen currently uses one shared tonewheel bank.

Full per-channel tuning support requires one of:
- Passing the MIDI channel into `oscKeyOn` and applying the correct `MTS_NoteToFrequency` for that note's channel (needs tonegen changes)
- Reinitialising the tonegen when the "active channel" changes (causes audio gaps)

For Phase 1, the change detection covers all channels (so any per-channel tuning update triggers a global reinit). Per-note-per-channel accuracy is Phase 2.

**Vibrato**: Currently applied uniformly across all notes. Long-term option: implement as an MPE signal (per-note pitch expression), but this requires architectural changes to the vibrato scanner.

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
- After changing ratios: call `reinitToneGen()` to rebuild the tonewheel oscillator bank.

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

### Surge XT tuning-library
The Surge XT project publishes a standalone `tuning-library` (Apache 2.0, header-only C++, JUCE-compatible) for loading `.scl` (Scala scale) and `.kbm` (keyboard mapping) files. This would allow users to import arbitrary scale files directly without needing a running MTS-ESP master.

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
