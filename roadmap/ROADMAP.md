# tuneBfree Roadmap

tuneBfree is a microtunable Hammond B3 tonewheel organ emulator, forked from setBfree. Development happens on the `juce` branch, which will become a full JUCE-based replacement for the existing LV2/Elements build. The `main` branch remains the stable LV2 release until the JUCE version is ready.

**Primary deployment target:** Raspberry Pi Debian.  
**Primary development environment:** Mac (also a valid end target, with AU plugin output).

---

## Phase 0 — Skills & Prerequisites

Before any new features, lay the groundwork:

- Check whether public JUCE skill files already exist (JUCE is widely documented and has a large community — there may be existing Claude skill files to build on).
- Create skill files for:
  - **setBfree** — DSP architecture, signal flow, `.cfg` / `.pgm` config formats
  - **microtuning** — MTS-ESP, scale period mathematics, the relationship between timbre and tuning (Sethares)
  - **JUCE** — `AudioProcessor` patterns, JUCE CMake build system, plugin format targets
- Fix the CLAP note-off bug on Raspberry Pi: MIDI note-off messages are currently ignored, leaving notes sounding indefinitely. This blocks all RPi CLAP testing.

---

## Phase 1 — JUCE Foundation

Replace the existing LV2/Elements plugin host with a JUCE `AudioProcessor` wrapping the existing DSP core (`tonegen`, `whirl`, `reverb`, `vibrato`, `overdrive`) without changing the audio engine itself.

**Build targets (in priority order):**
1. Standalone (headless-capable, RPi and Mac)
2. AU (Mac)
3. CLAP
4. VST3 (lower priority)

**Success criteria:**
- Compiles and runs on both Mac and Raspberry Pi Debian
- Note-on/off, drawbar CC, and Leslie switching all work correctly on RPi
- Use JUCE's generic plugin UI as a placeholder — no visual work yet

---

## Phase 2 — Microtuning Completeness

Bring the microtuning implementation up to the full current MTS-ESP specification, and add supporting tools.

- **MTS-ESP on RPi**: The standard MTS-ESP shared library may not work on Raspberry Pi. Investigate the RPi-compatible fork by Naren Ratan and Baconpaul — determine whether it covers the latest MTS-ESP spec (including scale period API). Use the best available version and document the dependency.
- **Period GUI**: There are already gui elements to see e.g. if a plugin is a client. The scale period setting is more novel, so it would be good to see if there is one—either specified in mts esp, calculated from the scale, or whether it cannot really be determined. (Technically if you take all the notes and consider that a period you could always have some period.)
- **Scale period API**: Use MTS-ESP's scale period specification when present; fall back to the existing `tuning.cpp` period calculation algorithm when not.
- **MIDI sysex**: Complete the existing partial implementation for sysex-based tuning messages.
- **3-manual MIDI channel split**: Remove. MTS-ESP assigns tuning data per MIDI channel, which directly conflicts with routing channels to separate manuals. Users needing multiple independent manuals should run multiple plugin instances. What is possible is having a split point between 2 manuals. There are already implementations for a sharp splitpoint, but I would prefer a gradual one like in the Nord keyboards or Korg Prologue for example.
- **Precision toggle**: Display tuning deviations in cents. When rounding is desired, consider modern psychoacoustic models — Weber's law is a starting point but is a simplistic model.
- **Surge XT tuning library**: Evaluate the `tuning-library` from the Surge XT project (Apache 2.0 licensed, JUCE-compatible) for `.scl` / `.kbm` file import.
- **Pitchbend / MPE**: Document why continuous pitch modulation is constrained by the tonewheel wavetable generation process, so the limitation is clear to users and future contributors.
- **Dynamic MTS**: It should be possible to dynamically retune via MTS-ESP or midi sysex. It will have a similar problem as pitchbend but in this case short delays are acceptable.
- **MIDI 2.0**: Investigate per-note pitch expression capabilities from the MIDI 2.0 spec; document feasibility.

---

## Phase 3 — GUI Modernisation

Build the new JUCE GUI in two modes. **Generate mockup drawings and get feedback before writing code.**

### Stage mode
The performance view. Shows the classic setBfree organ UI reflecting real-time MIDI CC and note state. Optimised for live use — no clutter.

### Studio mode
The sound design view. JUCE-native controls for:
- Drawbar harmonic ratios and overrides
- Effect parameters (Leslie, overdrive, reverb)
- Tuning overrides and precision settings
- Config and program management

### Config support
- Load and save both `.cfg` files (general settings) and `.pgm` files (programs/patches).
- Add JSON or TOML as an alternative format for import/export alongside the existing custom formats.

### Headless CLI
A `--config <file>` flag loads settings at startup. After startup, all interaction is via MIDI only — no window is required. This is the primary mode for scripted Raspberry Pi Debian launches.

---

## Phase 4 — Advanced Features

- **Audio input through Leslie**: Add audio input channels so an external signal (e.g. another instrument or microphone) can be routed through the rotary speaker simulation. Note: a plugin already exists that extracts just the Hammond DSP from setBfree as a standalone effect — examine that architecture for reference.
- **Reverb improvement**: The default setBfree reverb is underwhelming. Options to explore: pre-rotary spring reverb stage, alternative algorithmic reverbs, impulse response from Aeolus/Archie3d (York cathedral). Benchmark IR-based options on RPi before committing — convolution reverb is CPU-heavy.
- **Compute Optimization**: When no notes are played a lot of cpu is still consumed, try to optimize this. See for instance comments on GitHub (?) by Zynthian developer riban.
- **Zynthian compatibility**: Design the software so Zynthian deployment remains an open possibility, but do not actively package for it. The Zynthian community is a useful source of RPi implementation feedback.

---

## References & Context

Useful background information for all the phases.
Note also sibling projects:
- https://github.com/carlhenrikrolf/microtonOS
- https://github.com/carlhenrikrolf/ethnologue-synthesizer

### Related organ emulators (context, not build dependencies)
- **foo-YC20** — another open-source organ emulator
- **Aeolus / Archie3d fork** — includes a York cathedral IR; relevant to reverb and as a general reference
- **PJ's modular synth organ** — from the creator of pitchgrid.io, discussed in the Exquis community
- **setBfree** — the upstream project tuneBfree forks from

### Hardware context
- **Crumar DIY Hammond MIDI controllers** — relevant for understanding target controller setups

### Microtuning tools & resources
- Scale Workshop and Sevish — browser-based microtonal scale design
- Surge XT and related Surge community projects — tuning library, `.scl`/`.kbm` tooling
- Exquis — isomorphic MIDI controller with microtuning support
- William Sethares — *Tuning, Timbre, Spectrum, Scale*; the relationship between drawbar settings (timbre) and optimal tuning systems
- MIDI Association — MIDI 2.0 specification

### Platform
- Zynthian community — useful for RPi-specific implementation discussions even if not an active packaging target
