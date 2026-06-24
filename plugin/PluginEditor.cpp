#include "PluginEditor.h"
#include <cmath>

// ============================================================================
// Colours
// ============================================================================

static const juce::Colour kBg          { 0xff1a1a2e };
static const juce::Colour kBgAlt       { 0xff22223a };
static const juce::Colour kBgSelected  { 0xff3a4a6a };
static const juce::Colour kDivider     { 0xff333355 };
static const juce::Colour kTextMain    { 0xffd0d0e8 };
static const juce::Colour kTextMuted   { 0xff888899 };
static const juce::Colour kGreen       { 0xff50e060 };
static const juce::Colour kRed         { 0xffdd4444 };
static const juce::Colour kYellow      { 0xfff0c040 };

// ============================================================================
// Helpers
// ============================================================================

static const char* kNoteNames[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

juce::String TuningPanel::noteName(int n)
{
    if (n < 0 || n > 127) return "---";
    return juce::String(kNoteNames[n % 12]) + juce::String(n / 12 - 1);
}

// ============================================================================
// NoteTableModel
// ============================================================================

void TuningPanel::NoteTableModel::paintRowBackground(
    juce::Graphics& g, int row, int /*w*/, int /*h*/, bool selected)
{
    if (selected)
        g.fillAll(kBgSelected);
    else
        g.fillAll(row % 2 == 0 ? kBg : kBgAlt);
}

juce::String TuningPanel::NoteTableModel::getCellText(int row, int col) const
{
    switch (col)
    {
        case 0: return TuningPanel::noteName(row);
        case 1:
        {
            double f = proc.getDisplayFrequency(row);
            return juce::String(f, 3) + " Hz";
        }
        case 2:
        {
            double c = proc.getDisplayCents(row);
            return (c >= 0.0 ? "+" : "") + juce::String(c, 2);
        }
        case 3:
            return proc.isMidiNoteMapped(row) ? "yes" : "—";
        default:
            return {};
    }
}

void TuningPanel::NoteTableModel::paintCell(
    juce::Graphics& g, int row, int col, int w, int h, bool /*rowSelected*/)
{
    auto text = getCellText(row, col);

    juce::Colour fg = kTextMain;
    if (col == 2)
    {
        double c = proc.getDisplayCents(row);
        double ac = std::abs(c);
        if (ac > 20.0)      fg = kRed;
        else if (ac > 5.0)  fg = kYellow;
    }
    else if (col == 3 && !proc.isMidiNoteMapped(row))
    {
        fg = kTextMuted;
    }

    g.setColour(fg);
    g.setFont(juce::Font(12.0f));
    g.drawText(text, 4, 0, w - 4, h, juce::Justification::centredLeft, true);
}

// ============================================================================
// TuningPanel
// ============================================================================

TuningPanel::TuningPanel(TuneBfreeAudioProcessor& p) : proc(p)
{
    // --- MTS status row ---
    mtsStatusDot.setFont(juce::Font(14.0f));
    mtsStatusDot.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mtsStatusDot);

    mtsStatusText.setFont(juce::Font(13.0f));
    mtsStatusText.setJustificationType(juce::Justification::centredLeft);
    mtsStatusText.setColour(juce::Label::textColourId, kTextMain);
    addAndMakeVisible(mtsStatusText);

    scalePeriodLabel.setFont(juce::Font(12.0f));
    scalePeriodLabel.setJustificationType(juce::Justification::centredRight);
    scalePeriodLabel.setColour(juce::Label::textColourId, kTextMuted);
    addAndMakeVisible(scalePeriodLabel);

    // --- Local tuning row ---
    for (auto* btn : { &loadSclBtn, &loadKbmBtn, &clearBtn })
        addAndMakeVisible(btn);

    sclFileLabel.setFont(juce::Font(12.0f));
    sclFileLabel.setJustificationType(juce::Justification::centredLeft);
    sclFileLabel.setColour(juce::Label::textColourId, kTextMuted);
    addAndMakeVisible(sclFileLabel);

    kbmFileLabel.setFont(juce::Font(12.0f));
    kbmFileLabel.setJustificationType(juce::Justification::centredLeft);
    kbmFileLabel.setColour(juce::Label::textColourId, kTextMuted);
    addAndMakeVisible(kbmFileLabel);

    loadSclBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Load Scala Scale (.scl)",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.scl");
        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto results = fc.getResults();
                if (!results.isEmpty())
                    proc.loadSCLFile(results[0]);
                refresh();
            });
    };

    loadKbmBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Load Keyboard Mapping (.kbm)",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.kbm");
        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto results = fc.getResults();
                if (!results.isEmpty())
                    proc.loadKBMFile(results[0]);
                refresh();
            });
    };

    clearBtn.onClick = [this]
    {
        proc.clearLocalTuning();
        refresh();
    };

    // --- Error label ---
    errorLabel.setFont(juce::Font(12.0f));
    errorLabel.setJustificationType(juce::Justification::centredLeft);
    errorLabel.setColour(juce::Label::textColourId, kRed);
    addAndMakeVisible(errorLabel);

    // --- Note table ---
    tableModel = std::make_unique<NoteTableModel>(p);
    noteTable.setModel(tableModel.get());

    auto& hdr = noteTable.getHeader();
    hdr.addColumn("Note",        1,  60,  40,  90,  juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("Frequency",   2, 130,  80, 180,  juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("Cents",       3,  90,  60, 130,  juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("Mapped",      4,  60,  40,  80,  juce::TableHeaderComponent::defaultFlags);

    noteTable.setRowHeight(18);
    noteTable.setColour(juce::ListBox::backgroundColourId,      kBg);
    noteTable.setColour(juce::TableListBox::backgroundColourId, kBg);
    addAndMakeVisible(noteTable);

    refresh();
}

TuningPanel::~TuningPanel() = default;

void TuningPanel::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    g.setColour(kDivider);
    g.drawHorizontalLine(36, 0.0f, (float)getWidth());
    g.drawHorizontalLine(72, 0.0f, (float)getWidth());

    // Section labels
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.setColour(kTextMuted);
    g.drawText("MTS-ESP",      6, 0,  90, 14, juce::Justification::centredLeft);
    g.drawText("LOCAL TUNING", 6, 38, 100, 14, juce::Justification::centredLeft);
}

void TuningPanel::resized()
{
    auto r = getLocalBounds().withTrimmedTop(14).reduced(6, 0);

    // Row 1: MTS status (22px)
    auto row1 = r.removeFromTop(22);
    mtsStatusDot.setBounds (row1.removeFromLeft(20));
    mtsStatusText.setBounds(row1.removeFromLeft(260));
    scalePeriodLabel.setBounds(row1);

    r.removeFromTop(18); // row 2 gap

    // Row 2: local tuning (22px below divider label)
    auto row2 = r.removeFromTop(22);
    loadSclBtn.setBounds (row2.removeFromLeft(90).reduced(0, 2));
    sclFileLabel.setBounds(row2.removeFromLeft(140).reduced(2, 0));
    row2.removeFromLeft(6);
    loadKbmBtn.setBounds (row2.removeFromLeft(90).reduced(0, 2));
    kbmFileLabel.setBounds(row2.removeFromLeft(140).reduced(2, 0));
    row2.removeFromLeft(6);
    clearBtn.setBounds   (row2.removeFromLeft(60).reduced(0, 2));

    r.removeFromTop(4);

    // Error label
    errorLabel.setBounds(r.removeFromTop(18));

    r.removeFromTop(4);

    // Note table fills the rest
    noteTable.setBounds(r);
}

void TuningPanel::refresh()
{
    // MTS-ESP
    bool connected = proc.isMTSConnected();
    mtsStatusDot.setColour(juce::Label::textColourId, connected ? kGreen : kTextMuted);
    mtsStatusDot.setText(juce::String::fromUTF8("\xe2\x97\x8f"), juce::dontSendNotification);

    juce::String statusText = connected
        ? ("Connected — " + proc.getMTSScaleName())
        : "No master connected";
    mtsStatusText.setText(statusText, juce::dontSendNotification);

    // Scale period / size
    float period = proc.getInferredPeriod();
    int   size   = proc.getInferredScaleSize();
    if (period > 0.0f)
        scalePeriodLabel.setText(juce::String(size) + " notes, period x" + juce::String(period, 4),
                                 juce::dontSendNotification);
    else
        scalePeriodLabel.setText("period unknown", juce::dontSendNotification);

    // Local tuning file labels
    auto sclName = proc.getLocalSclName();
    auto kbmName = proc.getLocalKbmName();
    sclFileLabel.setText(sclName.isEmpty() ? "(none)" : sclName, juce::dontSendNotification);
    kbmFileLabel.setText(kbmName.isEmpty() ? "(none)" : kbmName, juce::dontSendNotification);

    // Error
    auto err = proc.getLocalTuningError();
    errorLabel.setText(err, juce::dontSendNotification);
    errorLabel.setVisible(err.isNotEmpty());

    // Refresh table
    noteTable.updateContent();
    noteTable.repaint();
}

void TuningPanel::triggerLoadScl() { loadSclBtn.triggerClick(); }
void TuningPanel::triggerLoadKbm() { loadKbmBtn.triggerClick(); }

// ============================================================================
// TuneBfreeAudioProcessorEditor
// ============================================================================

TuneBfreeAudioProcessorEditor::TuneBfreeAudioProcessorEditor(TuneBfreeAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), tuningPanel(p)
{
    addAndMakeVisible(menuBar);
    addAndMakeVisible(tuningPanel);

    setSize(700, 520);
    startTimerHz(2);
}

TuneBfreeAudioProcessorEditor::~TuneBfreeAudioProcessorEditor()
{
    stopTimer();
}

void TuneBfreeAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
}

void TuneBfreeAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    menuBar.setBounds(r.removeFromTop(24));
    tuningPanel.setBounds(r);
}

// --- MenuBarModel ---

juce::StringArray TuneBfreeAudioProcessorEditor::getMenuBarNames()
{
    return { "Tuning" };
}

juce::PopupMenu TuneBfreeAudioProcessorEditor::getMenuForIndex(int menuIndex, const juce::String&)
{
    juce::PopupMenu menu;
    if (menuIndex == 0)
    {
        menu.addItem(LoadSCL,     "Load .scl file...");
        menu.addItem(LoadKBM,     "Load .kbm file...");
        menu.addSeparator();
        menu.addItem(ClearTuning, "Clear local tuning");
    }
    return menu;
}

void TuneBfreeAudioProcessorEditor::menuItemSelected(int id, int /*topLevelMenuIndex*/)
{
    switch (id)
    {
        case LoadSCL:     tuningPanel.triggerLoadScl(); break;
        case LoadKBM:     tuningPanel.triggerLoadKbm(); break;
        case ClearTuning:
            audioProcessor.clearLocalTuning();
            tuningPanel.refresh();
            break;
    }
}

// --- Timer ---

void TuneBfreeAudioProcessorEditor::timerCallback()
{
    tuningPanel.refresh();
}
