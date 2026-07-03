---
description: MTS-ESP microtuning (client API, note filtering, sysex parsing, source gating, RPi/standalone caveats) as used in tuneBfree
triggers:
  - MTS-ESP
  - MTS
  - MTS_NoteToFrequency
  - ShouldFilterNote
  - tuning master
  - ODDSound
  - sysex tuning
---

# MTS-ESP in tuneBfree

MTS-ESP (ODDSound) is a plugin-level tuning protocol. A **master** (Surge XT, Scale Workshop,
MTS-ESP Mini, …) publishes per-note frequencies; tuneBfree registers as a **client** and
queries them. Library: `libs/MTS-ESP/Client/libMTSClient.{h,cpp}`. See [[scala]] for file-based
tuning and [[microtuning]] for the bigger picture (Sethares, drawbar ratios).

## Client API

```cpp
MTSClient* MTS_RegisterClient();
void       MTS_DeregisterClient(MTSClient*);
double MTS_NoteToFrequency(MTSClient*, char note, char channel);  // channel 0 = "all"
bool   MTS_HasMaster(MTSClient*);
bool   MTS_ShouldFilterNote(MTSClient*, char note, char channel); // master says "don't play this"
const char* MTS_GetScaleName(MTSClient*);
void   MTS_ParseMIDIDataU(MTSClient*, const unsigned char* data, int len); // feed MIDI sysex
```
All calls are lock-free and safe from the audio thread.

## How tuneBfree uses it (`PluginProcessor.cpp`)

- `prepareToPlay` registers the client; `releaseResources` deregisters it.
- `processBlock` scans **16 channels × 128 notes** comparing `MTS_NoteToFrequency` against
  `previousFrequency[16][128]`; any change (when an MTS source is active) triggers an **async
  rebuild** (`requestRebuild()` → worker → swap; NOT a synchronous `reinitToneGen` on the
  audio thread anymore).
- The worker's `buildMTSGamut` now queries **all active channels** (not just channel 0) and
  merges them into the gamut (`buildGamut`) — see the `microtuning`/`setbfree` skills.
- Scale name in the panel = `MTS_GetScaleName`; the last-update clock ticks while
  `MTS_HasMaster` is true.

## Channel selection + OMNI (tuneBfree 2.0)

The CHANNELS popup (`channelActive[16]` + OMNI) governs **both** MTS and FILE. For MTS:
- **OMNI OFF**: each *selected* channel queries its own number, `MTS_NoteToFrequency(note, i)`.
  There is **no "generic fallback" for MTS** — the client can't report which channels the
  master actually specified, so you always query `i`. (A non-multichannel master returns the
  same single table for every channel, so this reduces to single-channel automatically.)
- **OMNI ON**: every selected channel queries **`-1`** (the "unspecified"/non-multichannel
  table). Confirmed in `libMTSClient.cpp`: `-1` makes `supportsMultiChannelTuning` false, so
  it reads `esp_retuning` (the single table). With a non-multichannel master `-1` == channel 0.
- **Deselected channels are silent** regardless of OMNI (the generic-channel *fallback* is a
  FILE-only concept — see the `scala` skill).

## Note filtering is MTS-ESP-ONLY

`MTS_ShouldFilterNote` is the master's *per-scale* signal to silence keys not in the scale
(e.g. play a 7-note scale only on the white keys). In tuneBfree it is consulted **only when
the active source is MTS-ESP** — never SYSEX, FILE, or STANDARD. (FILE silences keys via the
`.kbm` `x` mechanism instead; SYSEX leaves un-played notes untuned.) Do not re-add it to the
other sources — that was a corrected mistake.

## SYSEX (MIDI Tuning Standard)

Raw MIDI sysex is forwarded to the same client via `MTS_ParseMIDIDataU`, so SYSEX tuning data
lives in the MTS client and `MTS_NoteToFrequency` returns it. That's why `getDisplayFrequency`
reads the client for both MTS and SYSEX, while *filtering* stays MTS-only. Detailed sysex
message handling (bulk dump, single-note retune, the "don't update vs filter" distinction) is
future work — the user will supply the MIDI Tuning spec PDFs to read at that point.

## Source gating (`TuningSourceId` in PluginProcessor.h)

`TS_MTS` / `TS_SYSEX` / `TS_FILE` / `TS_STANDARD` select which source feeds the engine. MTS
frequency-change reinits and the filter signal apply only under MTS/SYSEX (filter: MTS only);
FILE uses the local `.scl/.kbm`; STANDARD is a plain 12-TET table.

## Scale period API

Newer MTS-ESP can report a scale period directly. tuneBfree currently always *infers* the
period (`inferScaleSize`); if/when we use the master-reported period, label it "specified"
rather than "inferred". Check the installed dylib supports it first.

## Platform caveats

- **macOS standalone**: MTS-ESP only connects **inside a DAW**, not the standalone app
  (observed). Test MTS in a host; the standalone is fine for FILE/STANDARD tuning.
- **Raspberry Pi**: the stock shared library may not load (POSIX shared-memory behaves
  differently on ARM Linux). Use the Naren Ratan / Baconpaul fork
  (`baconpaul/mts-dylib-reference`) and check it supports the latest spec (period API).

Refs: <https://github.com/ODDSound/MTS-ESP>, <https://github.com/baconpaul/mts-dylib-reference>.
