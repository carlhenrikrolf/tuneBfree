# Control panel — backend survey & agreed design (2026-07-06)

Companion to [CONTROL_PANEL.md](CONTROL_PANEL.md). Records (1) the decisions from the
spec review Q&A, (2) the survey of existing preset/MIDI-mapping backends the spec asked
for, (3) the recommended architecture.

## 1. Decisions locked in (spec review Q&A)

1. **Preset scope** — all engine + effects parameters, *including* the HARMONICS drawbar
   fine-tunings (`harm_cents_*` / `harm_auto_*`). *Excluding* MIDI mappings and the
   tuning-panel (key tuning) state. The save dialog offers an opt-in "include tuning"
   block per preset.
2. **Not mappable** — any parameter whose change forces a tonegen rebuild cannot be
   assigned to a MIDI CC.
3. **Program change** — incoming MIDI Program Change selects the preset within the
   current bank; Bank Select MSB+LSB (CC 0/32) selects the bank.
4. **No standard preset directory yet** — the bank list contains what was loaded via the
   panel, remembered in plugin state. (Standard/factory location: later.) The bank names
   in the mockup are placeholders.
5. **Save flow** — prompt for name + author (Surge/Vital style) + include-tuning toggle;
   user chooses destination directory and filename. `.xml` only.
6. **`.cfg` support dropped** — the load control accepts `.pgm` (one file = one bank,
   backwards compat) and `.xml` (one file = one preset, one directory = one bank).
7. **CC list** — read-only, plus a REMOVE button (where the commented-out edit button
   sat in the mockup) to delete the selected mapping.
8. **MIDI learn** — captures the incoming message's channel; the channel can be widened
   to omni afterwards via the manual-assign menu.
9. **Sources** — MIDI CC and channel aftertouch. Polyphonic aftertouch: not for
   tuneBfree (see §2), but keep the door open for the future reusable control util.
10. **Default mappings** (omni, user-removable): CC 11 → expression, CC 7 → master
    volume, CC 64 → rotor speed. CC 120/123 stay hardwired as panic.
11. **Cardinality** — one CC may drive several parameters; each parameter has at most
    one source.
12. **Audio input** — standalone first; plugin-side input should work in permissive
    hosts (Carla, Reaper) but need not work everywhere.

## 2. Polyphonic aftertouch — is there any target here?

No. Every tuneBfree parameter is per-instance (global): the tonewheels are shared
across keys, so nothing in the engine can be modulated per-note (this is the same
reason MPE pitchbend is hard — see TUNING_PANEL.md). A poly-AT mapping to a global
parameter would just behave like channel aftertouch from whichever held key reported
the highest/latest pressure — confusing, no musical win. Recommendation:

- tuneBfree: don't offer PT as a source.
- Future control util: model a source as {CC n | channel AT | poly AT} × {omni | ch}.
  When a poly-AT source targets a *global* parameter, reduce over held notes (max, or
  last-changed). PT becomes genuinely useful only once a plugin has per-note
  destinations (MPE-style per-note vibrato is on the long-term roadmap).

## 3. Survey

### JUCE built-ins (the baseline)

Plugin state is a `ValueTree`, serialized to XML — the plugin already does
`copyState().createXml()` for DAW sessions, which confirms the `.xml` preset choice.
A preset is the same tree filtered to the preset scope. JUCE also has the host program
API (`getNumPrograms` / `getProgramName` / `setCurrentProgram`) which lets DAWs list
and switch presets; MIDI PC/bank-select we handle ourselves in `processBlock` (JUCE
has **no** MIDI-learn infrastructure). Verdict: sufficient foundation.

### chowdsp_utils presets ([repo](https://github.com/Chowdhury-DSP/chowdsp_utils))

Two systems:
- `chowdsp_presets` (v1): depends only on juce_core/juce_audio_utils; JUCE-compatible.
- `chowdsp_presets_v2`: much richer — backend (`Preset`, `PresetState`, `PresetTree`,
  `PresetSaverLoader`, `PresetManager`) + frontend interfaces (menu, file, clipboard,
  next/previous, and a **ProgramAdapter** that bridges the preset list to the host
  program API). But it serializes to JSON (`chowdsp_json`) and hard-depends on
  `chowdsp_plugin_state`, their *replacement* for APVTS.

**Verdict: don't adopt.** v2 would mean ripping out our whole APVTS layer (112 params +
attachments); v1 doesn't buy enough to justify carrying the multi-module dependency.
**Steal the shape instead**: preset object with metadata + format version, a manager
with a dirty flag ("has the user tweaked since loading?" → show `*` next to the preset
name), and the ProgramAdapter idea for MIDI PC / host program lists.

### Surge XT MIDI learn ([manual](https://surge-synthesizer.github.io/manual-xt/))

Confirmed behaviors: right-click → *Assign to MIDI CC* (0–127, some codes reserved) with
a *MIDI Channel* submenu (specific or Omni); *MIDI Learn* / *Abort MIDI Learn*; *Clear
learned MIDI* displays the current assignment next to it; one controller per parameter
(we deliberately relax that to one-CC-to-many-params); mappings live in session state
and can be stored as a default mapping (separate-file export — our future extension,
same as Pianoteq). Patch save dialog fields: name, category, author, license, comments —
we take name + author.

**Reserved/disabled CCs for the assign menu** (per the MIDI spec, matching Surge's
practice): 0/32 (bank select — reserved for bank switching), 6/38 (data entry),
96–101 (data inc/dec, NRPN, RPN), 88 (high-resolution velocity), 120–127 (channel
mode; 120/123 are our hardwired panic). Everything else is offered, in submenus of 20
with General MIDI names (1 modwheel, 2 breath, 4 foot, 7 volume, 11 expression,
64 sustain, 91 reverb depth, …).

### CLAP preset extensions ([preset-load](https://github.com/free-audio/clap/blob/main/include/clap/ext/preset-load.h), preset-discovery factory)

`preset-discovery` lets a host index a plugin's preset files (with metadata) without
instantiating it; `preset-load` lets the host tell the plugin to load one. Nice — but
host-facing plumbing, orthogonal to the panel's internal design.

**Repo reality check found during the survey:** JUCE 8.0.14 has **no CLAP support** —
`juce_add_plugin` silently ignores unknown format tokens, so the `CLAP` entry in our
`FORMATS` line is inert. `build/tuneBfree_artefacts/Release/` contains only AU /
Standalone / VST3. CLAUDE.md's "builds CLAP" claim refers to the *legacy* `src/`
CLAP build (still buildable separately with `cmake -S src -B src/build`; that's what
the `libs/clap` + `libs/clap-wrapper` submodules are for). A JUCE-based CLAP needs
[clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions).
**Design impact today:** keep presets one-file-per-preset with self-contained metadata,
so a discovery provider can index them if/when CLAP is wired up. Nothing else.

## 4. Recommended architecture (no new dependencies)

- **Preset file** — XML:
  `<tuneBfreePreset name=".." author=".." app="tuneBfree" appVersion="2.0.0" formatVersion="1">`
  containing `<PARAMS>` (the preset-scope parameters) and, when opted in, `<TUNING>`.
- **Bank** — an ordered list of presets held in the plugin state (so it survives
  sessions without a standard directory). Loading a `.xml` directory or a `.pgm` file
  materializes a bank; `.pgm` programs are converted through the existing
  `src/pgmParser` and live in state until saved out as `.xml`.
- **MIDI map** — its own `<MIDI_MAP>` subtree in plugin state (never in presets;
  trivially exportable to a mapping file later). Entries:
  `{source: CC n | channel AT, channel: omni|1–16} → [paramIDs]`. The audio thread
  reads an immutable snapshot swapped in on edit (same pattern as the tonegen rebuild).
- **Mappability flag** — parameters whose change calls `requestRebuild()` are marked
  unmappable; the right-click menu hides the mapping items for them.
- **Right-click additions** (below name/edit/info): MIDI Learn, Assign MIDI CC
  (channel submenu + CC bins of 20, reserved codes greyed), Clear mapping (shows the
  current source, like Surge).
- **Program change** — PC selects within the current bank; bank MSB/LSB pending both
  bytes selects the bank; current bank also exposed through the JUCE program API so
  DAWs can browse it.
- **Audio input** — add a stereo input bus; summed into the chain post-preamp /
  pre-reverb (external audio never passes the organ's overdrive). Standalone: JUCE
  mutes input by default; the user enables it in audio settings. AU is a Music Device
  (`aumu`): GarageBand can't feed it audio; Carla/Reaper/Logic-sidechain can.

Suggested build order: (1) preset save/load + panel PROGRAM CHANGE section,
(2) MIDI PC / bank select + program API, (3) mapping engine + right-click items +
CONTINUOUS CONTROLLERS list, (4) defaults + persistence polish, (5) audio input bus.
