# tuneBfree — Claude Code Context

## What this project is

tuneBfree is a microtunable Hammond B3 tonewheel organ emulator, forked from setBfree. It is being migrated from its LV2/CLAP architecture to JUCE. Development happens on the `juce` branch; `main` is the stable LV2 release.

**Primary deployment target:** Raspberry Pi Debian (headless standalone).
**Development environment:** Mac (also a final target, AU plugin).

---

## Current state (tuneBfree 2.0)

### What has been built

| Area | Status |
|------|--------|
| JUCE CMake build | Working -- builds Standalone, AU, VST3, CLAP on Mac |
| PluginProcessor | Complete -- wraps all DSP, 38 APVTS parameters, MTS-ESP, silence detection |
| PluginEditor | Phase 2 tuning UI: menu bar + TuningPanel (MTS-ESP status, .scl/.kbm load, cents table) |
| CLAP note-off bug | Fixed in src/clap.cpp -- added MIDI dialect + CLAP_EVENT_MIDI handler |
| Skill files | Written: .claude/skills/setbfree.md, juce.md, microtuning.md, gui.md (OSC quirks documented) |
| GUI mockup | roadmap/gui.json (OSC 1.30.3) -- v1 tested, screenshots in roadmap/screenshots/ |
| JUCE submodule | libs/JUCE -- JUCE 8.0.14 |
| Bluetooth MIDI (Standalone) | Fixed -- use BLUETOOTH_PERMISSION_ENABLED/TEXT in juce_add_plugin (PLIST_TO_MERGE is silently ignored) |

### Phase 1 tested and working (Mac, June 2026)

Standalone and AU both confirmed working:
- Bluetooth MIDI, drawbar volume, Leslie (drum/horn speeds), vibrato, overdrive
- MTS-ESP tuning: works dynamically mid-session (not just at startup)
- State persistence via APVTS

CLAP artifact: `build/tuneBfree_artefacts/Release/CLAP/tuneBfree.clap` -- COPY_PLUGIN_AFTER_BUILD
does not auto-install CLAP on Mac (no standard system path). Install manually to `~/Library/Audio/Plug-Ins/CLAP/` if needed.

RPi: not yet tested.

### Build commands

```bash
# First-time setup
git submodule update --init libs/JUCE libs/MTS-ESP libs/readerwriterqueue

# Configure + build (auto-copies AU to ~/Library/Audio/Plug-Ins/Components/)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Outputs land in build/tuneBfree_artefacts/Release/: Standalone/, AU/, VST3/, CLAP/.

### GarageBand workflow

COPY_PLUGIN_AFTER_BUILD is TRUE in CMakeLists.txt, so the AU is auto-copied to
~/Library/Audio/Plug-Ins/Components/ on every build. GarageBand must be quit and restarted
to pick up a new or updated AU. If it fails to appear after restart, run:

```bash
auval -v aumu Tfre Tunb    # validates the AU; forces a rescan
```

---

## Repository structure

```
tuneBfree/
+-- CMakeLists.txt          <- JUCE build (root level, new)
+-- plugin/                 <- JUCE plugin wrapper (new)
|   +-- PluginProcessor.h/cpp
|   +-- PluginEditor.h/cpp  (Phase 3 placeholder, not yet used)
|   +-- midi_stubs.cpp      <- stubs for setBfree config/MIDI-CC callbacks
+-- src/                    <- DSP core (setBfree, unchanged except clap.cpp)
|   +-- tonegen.cpp/h       <- tonewheel oscillator bank
|   +-- whirl.cpp/h         <- Leslie rotary speaker
|   +-- reverb.cpp/h        <- reverb
|   +-- overdrive.cpp/h     <- preamp/saturation
|   +-- vibrato.cpp/h       <- vibrato/chorus scanner
|   +-- tuning.cpp/h        <- MTS-ESP + scale period math
|   +-- eqcomp.cpp/h        <- EQ/compression
|   +-- clap.cpp            <- legacy CLAP wrapper (kept; note-off bug fixed)
|   +-- CMakeLists.txt      <- legacy CLAP-only build (still works independently)
+-- libs/
|   +-- JUCE/               <- git submodule (6.0.8)
|   +-- MTS-ESP/            <- microtuning client
|   +-- readerwriterqueue/  <- lock-free queue (used by legacy CLAP)
+-- .claude/skills/
|   +-- setbfree.md         <- DSP architecture, API, signal chain
|   +-- juce.md             <- JUCE AudioProcessor patterns, CMake
|   +-- microtuning.md      <- MTS-ESP API, scale period, Sethares
+-- roadmap/
|   +-- ROADMAP.md          <- full phased roadmap (Phases 0-4 + References)
|   +-- TUNEBFREE_2.md      <- immediate next steps (Phase 0-1)
|   +-- UPDATE.md           <- original spec by user (to be removed by user)
+-- cfg/                    <- .cfg preset files (setBfree format)
+-- pgm/                    <- .pgm program/patch files
+-- b_synth/                <- legacy LV2 plugin + OpenGL GUI (to be superseded)
```

---

## PluginProcessor parameters (38 total)

Indices match src/clap.cpp for compatibility.

| Index | ID | Range | Default | Maps to |
|-------|----|-------|---------|---------|
| 0-8 | drawbar0-8 | 0-8 (step 1) | 7,8,8,0... | setDrawBar |
| 9 | vibrato | 0-1 | 0 | setVibratoUpper |
| 10 | vibrato_type | 0-5 | 0 | setVibratoFromInt |
| 11 | drum | 0-2 | 1 | useRevOption |
| 12 | horn | 0-2 | 1 | useRevOption |
| 13 | overdrive | 0-1 | 0 | preamp->isClean |
| 14 | character | 0-1 | 0 | fsetCharacter |
| 15 | reverb_mix | 0-1 | 0.1 | setReverbMix |
| 16 | percussion | 0-1 | 0 | setPercussionEnabled |
| 17 | percussion_vol | 0-1 | 0 | setPercussionVolume |
| 18 | percussion_dec | 0-1 | 0 | setPercussionFast |
| 19 | percussion_har | 0-1 | 0 | setPercussionFirst |
| 20-28 | ratio_top_0-8 | 0-1000 | 1,3,1,2,3,4,5,6,8 | targetRatio[i] |
| 29-37 | ratio_bot_0-8 | 0-1000 | 2,2,1,1,1,1,1,1,1 | targetRatio[i] |

---

## Key architectural decisions

- **Single manual only** -- The 3-manual MIDI channel split is removed in Phase 2.
  Remaining manual is the upper manual (has percussion, vibrato).
  Multi-manual setups: run multiple instances.
- **MTS-ESP channels** -- Tracking double previousFrequency[16][128] (all 16 MIDI channels
  x 128 notes). Full per-channel tuning at oscKeyOn level is Phase 2 (requires tonegen changes).
- **Silence detection** -- processBlock skips DSP when activeNoteCount == 0 and silent
  for > 3 s. Addresses setBfree's known CPU waste when no notes play (riban comment on
  Zynthian forum / GitHub).
- **Config stubs** -- plugin/midi_stubs.cpp provides no-op implementations of setBfree's
  .cfg parser and MIDI-CC registration callbacks. Config file support is Phase 3.
- **Vibrato** -- Uniform across all notes for now. Long-term: optional MPE per-note expression.
- **Zynthian** -- Design for possible compatibility; do not actively package.
- **CLAP format** -- Natively supported in JUCE 8. CLAP artifact builds but is not auto-installed on Mac.

---

## GUI plan (Phase 3)

Two modes:
- **Stage** -- JUCE-native controls equivalent to Naren's CLAP plugin UI (drawbar sliders,
  Leslie, percussion, vibrato, overdrive, reverb). NOT the old setBfree OpenGL GUI.
- **Studio** -- Sound design view: drawbar ratio overrides, effect parameters,
  config/program management.

Generate mockup drawings and get user feedback before implementing.

---

## DSP signal chain

```
MIDI --> oscKeyOn/Off (tonegen)
     --> oscGenerateFragment --> bufA [mono, 128 samples]
     --> preamp(preampModule, bufA, bufB)
     --> reverbModule->reverb(bufB, bufC)
     --> whirlProc3(whirlModule, bufC, bufL[0], bufL[1], bufD[0], bufD[1])
     --> stereo output
```

reinitToneGen() frees and rebuilds the tonegen when MTS-ESP frequencies or ratio parameters
change. Called from the audio thread in processBlock.

---

## References (from ROADMAP.md)

- Aeolus/Archie3d -- IR reverb (York cathedral), RPi-tested
- Surge XT tuning-library -- Apache 2.0, .scl/.kbm import
- MTS-ESP RPi fork -- Naren Ratan / Baconpaul (standard shared lib may not work on RPi)
- William Sethares -- timbre/tuning relationship (drawbar settings <-> optimal scale)
- foo-YC20, PJ's organ (pitchgrid.io/Exquis community) -- reference implementations
- Crumar DIY Hammond controllers -- target hardware context
