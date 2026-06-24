#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ============================================================================
// TuningPanel — the main tuning content area
// ============================================================================

class TuningPanel : public juce::Component
{
public:
    explicit TuningPanel(TuneBfreeAudioProcessor& p);
    ~TuningPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();

    void triggerLoadScl();
    void triggerLoadKbm();

private:
    TuneBfreeAudioProcessor& proc;

    // --- Status row ---
    juce::Label mtsStatusDot;
    juce::Label mtsStatusText;
    juce::Label scalePeriodLabel;

    // --- Local tuning row ---
    juce::Label sclFileLabel;
    juce::Label kbmFileLabel;
    juce::TextButton loadSclBtn  {"Load .scl…"};
    juce::TextButton loadKbmBtn  {"Load .kbm…"};
    juce::TextButton clearBtn    {"Clear"};
    juce::Label errorLabel;

    // --- Note table ---
    struct NoteTableModel : public juce::TableListBoxModel
    {
        TuneBfreeAudioProcessor& proc;
        explicit NoteTableModel(TuneBfreeAudioProcessor& p) : proc(p) {}

        int  getNumRows() override { return 128; }
        void paintRowBackground(juce::Graphics&, int, int, int, bool) override;
        void paintCell(juce::Graphics&, int row, int col, int w, int h, bool) override;
        juce::String getCellText(int row, int col) const;
    };

    std::unique_ptr<NoteTableModel> tableModel;
    juce::TableListBox noteTable;

    std::unique_ptr<juce::FileChooser> fileChooser;

    static juce::String noteName(int midiNote);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPanel)
};

// ============================================================================
// TuneBfreeAudioProcessorEditor — top-level plugin window
// ============================================================================

class TuneBfreeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::MenuBarModel,
                                       private juce::Timer
{
public:
    explicit TuneBfreeAudioProcessorEditor(TuneBfreeAudioProcessor&);
    ~TuneBfreeAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu   getMenuForIndex(int menuIndex, const juce::String& menuName) override;
    void              menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    // Timer — refresh MTS-ESP status
    void timerCallback() override;

private:
    TuneBfreeAudioProcessor& audioProcessor;

    juce::MenuBarComponent menuBar{this};
    TuningPanel            tuningPanel;

    enum MenuIDs {
        LoadSCL = 1, LoadKBM, ClearTuning
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuneBfreeAudioProcessorEditor)
};
