# tuneBfree 2 — Immediate Next Steps

This document covers the immediate work needed before any feature development can begin. See `ROADMAP.md` for the full long-term plan.

---

## 1. Fix CLAP note-off bug (RPi blocker)

**Problem:** On Raspberry Pi, sending MIDI note-off does not stop a sounding note. Notes keep sounding indefinitely. This makes the CLAP build unusable on RPi.

**Priority:** Fix before any other RPi testing.

---

## 2. Create skill files

Skill files teach the AI assistant about the project's domain. Check first whether public JUCE skill files already exist — JUCE is widely used and community-maintained skill files may be available.

Create the following if not already available:

| Skill | Content |
|-------|---------|
| `juce` | `AudioProcessor` patterns, JUCE CMake build, plugin format targets (AU, CLAP, VST3, Standalone) |
| `setBfree` | DSP architecture, signal flow (`tonegen` → `whirl` → `reverb`), `.cfg` / `.pgm` config formats, key source files in `src/` |
| `microtuning` | MTS-ESP client API, scale period calculation in `tuning.cpp`, tuning-timbre relationship (Sethares) |

---

## 3. JUCE Foundation

Wrap the existing DSP core in a JUCE `AudioProcessor`. Do not change the audio engine — only replace the plugin host layer.

**Files involved:**
- `src/tonegen.cpp` / `tonegen.h`
- `src/whirl.cpp`
- `src/reverb.cpp`
- `src/vibrato.cpp`
- `src/overdrive.cpp`
- `src/midi.cpp` / `midi.h`
- `src/tuning.cpp`

**Steps:**
1. Add JUCE as a dependency (CMake `FetchContent` or git submodule)
2. Create a `JucePlugin/` directory with `PluginProcessor.cpp/h` and `PluginEditor.cpp/h`
3. Wire DSP init/process/cleanup into `prepareToPlay`, `processBlock`, `releaseResources`
4. Route MIDI events into the existing `midi.cpp` handler
5. Build targets: Standalone first, then AU (Mac), then CLAP

**Done when:**
- Plays and releases notes correctly on both Mac and Raspberry Pi Debian
- Drawbar CC messages update the sound in real time
- Leslie on/off switching works
- Uses JUCE generic UI — no custom GUI yet
