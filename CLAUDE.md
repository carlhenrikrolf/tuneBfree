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
| JUCE CMake build | Working -- builds Standalone, AU, VST3 on Mac; CLAP only via the optional clap-juce-extensions submodule (JUCE has NO native CLAP -- unknown FORMATS tokens are silently ignored) |
| PluginProcessor | Complete -- wraps all DSP, APVTS parameters (see roadmap/PARAMETERS.md), MTS-ESP, silence detection, async rebuild, master volume, active-manual routing |
| PluginEditor | Full 3-page GUI (2026-07): PLAY / TINKER / ROTOR + editor-level tuning + CONTROL overlays (mutually exclusive), right-click param menus (name/edit/info + MIDI learn/assign/clear), header (panic, volume popup, CONTROL). See .claude/skills/gui.md |
| CONTROL panel | Built 2026-07 — presets (all params except master_volume/active_manual + harmonics entry strings; optional tuning block), banks in apvts.state, .xml save + .pgm import; MIDI Program Change / Bank Select; MIDI CC + channel-aftertouch mapping engine (right-click learn/assign, defaults CC 11→expr, 7→master, 64→rotor). See plugin/PresetManager.*, roadmap/CONTROL_PANEL*.md |
| Audio input | Optional stereo input bus (disabled by default), summed to mono and injected post-preamp / pre-reverb via a ring FIFO — external audio through the Leslie |
| CLAP note-off bug | Fixed in src/clap.cpp -- added MIDI dialect + CLAP_EVENT_MIDI handler |
| Skill files | Written: .claude/skills/setbfree.md, juce.md, microtuning.md, gui.md, scala.md, mts-esp.md (OSC quirks documented) |
| GUI mockup | roadmap/gui.json (OSC 1.30.3) -- v1 tested, screenshots in roadmap/screenshots/ |
| JUCE submodule | libs/JUCE -- JUCE 8.0.14 |
| Bluetooth MIDI (Standalone) | Fixed -- use BLUETOOTH_PERMISSION_ENABLED/TEXT in juce_add_plugin (PLIST_TO_MERGE is silently ignored) |

### tuneBfree 2.0 — multichannel tuning + keyboard split (built 2026-06-30)

The big Phase-2+ work is done and builds (Standalone; unit tests green). Full record:
`roadmap/MULTICHANNEL.md`; durable how-it-works notes: the `setbfree` / `microtuning` /
`scala` / `mts-esp` skills. In brief:
- **Multichannel gamut**: 16×128 `(channel,note)` merged into one de-duplicated pitch gamut
  (`buildGamut`); `b_tonegen::slotIndex[16][128]` routes to it; runtime `gamutSize`/`nofWheels`
  (Solution B), compile ceiling `MAX_GAMUT=2048`.
- **Async rebuild**: tonegen rebuilds on a `RebuildThread`, swapped in on the audio thread
  (no more synchronous `reinitToneGen` on the audio thread).
- **Keyboard split**: split by sounding pitch, equal-power crossfade baked into `keyTaper`;
  LEARN (set-from-notes); UPPER/LOWER toggle re-points the one drawbar bank. B3-like
  (percussion upper-only; shared vibrato type, per-manual on/off).
- **CHANNELS popup** governs active channels for MTS *and* FILE (OMNI + generic fallback).
- **Verified by the user (2026-06-30):** plays correctly, split + channel selection work.
  GUI polish + the other TUNING_PANEL.md tweaks landed 2026-07-04/05 (see the `gui`
  skill). Percussion/crossfade limitation SOLVED (graded trigger — `setbfree` skill).

### Phase 1 tested and working (Mac, June 2026)

Standalone and AU both confirmed working:
- Bluetooth MIDI, drawbar volume, Leslie (drum/horn speeds), vibrato, overdrive
- MTS-ESP tuning: works dynamically mid-session (not just at startup)
- State persistence via APVTS

CLAP: built only when `libs/clap-juce-extensions` exists (see Build commands). The artifact
lands in `build/tuneBfree_artefacts/Release/CLAP/tuneBfree.clap` and COPY_PLUGIN_AFTER_BUILD
auto-installs it to `~/Library/Audio/Plug-Ins/CLAP/`. NB: that copy step fails in Claude's
sandbox (writes outside the repo) -- sandboxed builds should target tuneBfree_Standalone /
tuneBfree_snapshot, not tuneBfree_CLAP or the default all-target.

RPi: not yet tested.

### Build commands

```bash
# First-time setup (libs/doctest is only needed for the unit tests)
git submodule update --init libs/JUCE libs/MTS-ESP libs/readerwriterqueue libs/doctest

# Configure + build (auto-copies AU to ~/Library/Audio/Plug-Ins/Components/)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# NOTE: the plain build above compiles EVERY format (Standalone, AU, VST3, and
# CLAP if enabled) plus the tests and the snapshot tool — slow. For iteration,
# build one target:
cmake --build build --target tuneBfree_Standalone   # GUI/audio testing
cmake --build build --target tuneBfree_AU           # GarageBand testing

# Headless GUI screenshots (sandbox-safe; renders the editor in software):
cmake --build build --target tuneBfree_snapshot
build/tuneBfree_snapshot_artefacts/Release/tuneBfree_snapshot 1 build/snapshots/tinker.png
#   page: 0 = PLAY, 1 = TINKER, 2 = ROTOR — output is a 2x PNG at parameter defaults

# Headless offline audio render (sandbox-safe; the audio counterpart — MIDI→WAV):
cmake --build build --target tuneBfree_render
build/tuneBfree_render_artefacts/Release/tuneBfree_render build/renders/note.wav --note 60
#   options: --note --vel --channel --seconds --hold --sr --block --drawbars a,..,i
#            --scl FILE --kbm FILE --source standard|scala|mts|sysex
#   prints frames/peak/rms/nonFinite — the nonFinite (NaN/Inf) count must be 0

# Melatonin Inspector (GUI layout debugging, cmd+I in the editor) — enabled
# automatically once the submodule exists (network needed, run manually):
#   git submodule add https://github.com/sudara/melatonin_inspector.git libs/melatonin_inspector

# CLAP (optional) — JUCE has no native CLAP; enabled automatically once the
# clap-juce-extensions submodule exists (network needed, run manually):
#   git submodule add https://github.com/free-audio/clap-juce-extensions.git libs/clap-juce-extensions
#   git submodule update --init --recursive libs/clap-juce-extensions
# Then reconfigure; the target is tuneBfree_CLAP.
```

### Tests

JUCE-free doctest unit tests cover the DSP/tuning math (see TESTING.md):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DTUNEBFREE_BUILD_TESTS=ON
cmake --build build --target tuneBfree_tests
ctest --test-dir build --output-on-failure
```

Outputs land in build/tuneBfree_artefacts/Release/: Standalone/, AU/, VST3/ (+ CLAP/ when enabled).

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
|   +-- PluginEditor.h/cpp  <- 3-page GUI (PLAY/TINKER/ROTOR) + tuning overlay
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
|   +-- JUCE/               <- git submodule (8.0.14)
|   +-- MTS-ESP/            <- microtuning client
|   +-- readerwriterqueue/  <- lock-free queue (used by legacy CLAP)
+-- .claude/skills/
|   +-- setbfree.md         <- DSP architecture, API, signal chain
|   +-- juce.md             <- JUCE AudioProcessor patterns, CMake
|   +-- microtuning.md      <- MTS-ESP API, scale period, Sethares (overview)
|   +-- scala.md            <- .scl/.kbm format, octaveDegrees gotcha, layout recipe
|   +-- mts-esp.md          <- MTS-ESP client API, note filtering, source gating
+-- roadmap/
|   +-- ROADMAP.md          <- full phased roadmap (Phases 0-4 + References)
|   +-- TUNEBFREE_2.md      <- immediate next steps (Phase 0-1)
|   +-- UPDATE.md           <- original spec by user (to be removed by user)
+-- cfg/                    <- .cfg preset files (setBfree format)
+-- pgm/                    <- .pgm program/patch files
+-- b_synth/                <- legacy LV2 plugin + OpenGL GUI (to be superseded)
```

---

## PluginProcessor parameters

Full audit (exposed / hidden / dead / stubbed): `roadmap/PARAMETERS.md`.
Index map (P_* defines in plugin/PluginProcessor.h):

| Indices | Group |
|---------|-------|
| 0-19 | CLAP-era basics: drawbars, vibrato(+type), drum/horn, overdrive/character, reverb_mix, percussion switches |
| 20-37 | HARMONICS: harm_cents_0-8 + harm_auto_0-8 (replaced ratio_top/bot; AUTO = JI quantized to tuning, CUSTOM = exact cents w/ injected wheels) |
| 38-51 | expression, split enable/point/width, lower_vibrato, lower drawbars |
| 52-80 | TINKER: scanner, percussion physics, key click, crosstalk, EQ spline, wave, preamp |
| 81-109 | ROTOR: bypass, motors, filters, mic & cabinet |
| 110-111 | master_volume (header volume popup), active_manual (unitimbral routing) |
| 112-114 | MatrixVerb voicing: reverb_damping, reverb_size, reverb_flavor |
| 115-118 | cabinet geometry: horn/drum radius, horn x/z offset |
| 119-120 | overdrive bias base (preamp_bias) + global feedback (preamp_gfb) |

Total P_COUNT = 121.

---

## Key architectural decisions

- **Two manuals, pitch-split** -- upper + lower banks; BITIMBRAL splits by sounding
  pitch with an equal-power crossfade; unitimbral mode routes to the bank selected by
  UPPER/LOWER (`active_manual`). Both manuals always wired; percussion is upper-only.
- **MTS-ESP channels** -- Tracking double previousFrequency[16][128] (all 16 MIDI channels
  x 128 notes). Full per-channel tuning at oscKeyOn level is Phase 2 (requires tonegen changes).
- **Silence detection** -- processBlock skips DSP when activeNoteCount == 0 and silent
  for > 3 s. Addresses setBfree's known CPU waste when no notes play (riban comment on
  Zynthian forum / GitHub).
- **Config stubs** -- plugin/midi_stubs.cpp provides no-op implementations of setBfree's
  .cfg parser and MIDI-CC registration callbacks (plus program-install/keyboard stubs so
  src/program.cpp links for the .pgm PARSER used by PresetManager). .cfg import dropped.
- **MIDI mapping** -- lives in apvts.state "MIDI_MAP"; audio thread reads a try-locked
  snapshot and defers parameter writes to the message thread via a lock-free FIFO +
  AsyncUpdater (keeps GUI/automation/DSP consistent). Rebuild-scope params are unmappable.
- **Vibrato** -- Uniform across all notes for now. Long-term: optional MPE per-note expression.
- **Zynthian** -- Design for possible compatibility; do not actively package.
- **CLAP format** -- JUCE 8 has NO native CLAP support (a `CLAP` token in FORMATS is
  silently ignored). The JUCE-based CLAP comes from the optional clap-juce-extensions
  submodule (conditional in CMakeLists.txt, like Melatonin). The legacy non-JUCE CLAP
  (src/clap.cpp + libs/clap + libs/clap-wrapper) still builds separately via `cmake -S src -B src/build`.

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
