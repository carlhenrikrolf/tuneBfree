#pragma once
#include <JuceHeader.h>

class TuneBfreeAudioProcessor;

// ============================================================================
//  PresetManager — the CONTROL panel's preset/bank library.
//  ---------------------------------------------------------------------------
//  Design record: roadmap/CONTROL_PANEL.md + roadmap/CONTROL_PANEL_SURVEY.md.
//
//  A preset is a full snapshot of every engine/effects parameter — including
//  the HARMONICS drawbar fine-tunings (cents params + entry strings) — but NOT
//  master_volume / active_manual (instance state, not sound) and NOT the MIDI
//  mappings. Key tuning (the TUNING panel) is captured only when the save
//  dialog's INCLUDE TUNING toggle is on, as a <TUNING> child block.
//
//  Banks are ordered lists of presets held in a <BANKS> subtree of apvts.state,
//  so everything loaded via the panel persists in the DAW session / standalone
//  state without a standard preset directory (deferred by design). Because a
//  session restore replaces the whole state tree, the model NEVER caches
//  ValueTrees — every accessor re-fetches from apvts.state.
//
//  File format (one preset per file, self-describing so a CLAP preset-discovery
//  provider could index it later):
//    <tuneBfreePreset name=".." author=".." app="tuneBfree" appVersion=".."
//                     formatVersion="1">
//      <PARAMS drawbar0=".." ... harmEntry0=".." .../>
//      <TUNING .../>                                    (optional)
//    </tuneBfreePreset>
//
//  Message thread only.
// ============================================================================

class PresetManager
{
public:
    explicit PresetManager (TuneBfreeAudioProcessor& p) : proc (p) {}

    static constexpr const char* presetTag = "tuneBfreePreset";

    // Parameters excluded from presets (and, later, from MIDI mapping of the
    // volume default): instance state rather than sound.
    static bool isPresetScopeParam (const juce::String& paramID);

    // --- capture / apply (current engine state <-> preset tree) ---
    juce::ValueTree capture (const juce::String& name, const juce::String& author,
                             bool includeTuning) const;
    void apply (const juce::ValueTree& preset);

    // --- banks model (lives under apvts.state / "BANKS") ---
    int             numBanks() const;
    juce::ValueTree getBank (int index) const;              // invalid tree if out of range
    juce::String    getBankName (int index) const;
    int             addBank (const juce::String& name);     // returns the new index
    int             numPresets (int bankIndex) const;
    juce::ValueTree getPreset (int bankIndex, int presetIndex) const;
    juce::String    getPresetName (int bankIndex, int presetIndex) const;
    void            appendPreset (int bankIndex, juce::ValueTree preset);

    // Selection (persisted as properties on the BANKS node).
    int  getCurrentBank() const;
    void setCurrentBank (int index);
    int  getCurrentPreset() const;                          // -1 = none selected
    void setCurrentPreset (int index);

    // --- file I/O ---
    bool savePresetFile (const juce::ValueTree& preset, const juce::File& f,
                         juce::String& error) const;
    juce::ValueTree loadPresetFile (const juce::File& f, juce::String& error) const;

    // Load .xml preset files into an existing bank. Returns how many loaded.
    int loadPresetFilesIntoBank (const juce::Array<juce::File>& files, int bankIndex,
                                 juce::String& error);
    // One directory of .xml presets = one new bank. Returns its index, or -1.
    int loadBankDirectory (const juce::File& dir, juce::String& error);
    // One .pgm file = one new bank (setBfree backwards compat: each in-use
    // programme becomes a preset of defaults + the programme's overrides).
    int importPgmFile (const juce::File& f, juce::String& error);

private:
    juce::ValueTree banksTree() const;      // create-on-demand, never cached
    juce::ValueTree defaultParams() const;  // every scope param at its default

    TuneBfreeAudioProcessor& proc;
};
