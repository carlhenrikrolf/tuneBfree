#include "PluginEditor.h"
#include <cmath>

// ============================================================================
//  PALETTE & FONT
//  ---------------------------------------------------------------------------
//  Restricted palette: amber / black / white / red / grey. Amber is the main
//  accent (and the header background); near-black is the main background.
//  To re-theme the whole plugin, edit these constants.
// ============================================================================

static const juce::Colour kBg        { 0xff121212 }; // main background
static const juce::Colour kPanel     { 0xff242424 }; // tuning panel + info boxes
static const juce::Colour kBtn       { 0xff080808 }; // inactive button fill
static const juce::Colour kAmber     { 0xffff8c00 }; // accent + header + active
static const juce::Colour kRed       { 0xffcc2a2a }; // active TUNING + two drawbars
static const juce::Colour kWhite     { 0xffe8e8e8 }; // main text + three drawbars
static const juce::Colour kGrey      { 0xff707070 }; // muted text (labels)
static const juce::Colour kBorder    { 0xff3a3a3a }; // box outlines

// One geometric sans-serif everywhere (Century-Gothic-like). Two weights only.
// Futura ships with macOS; for the Raspberry Pi build, bundle a .ttf later.
static juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions()
        .withName ("Futura")
        .withHeight (size)
        .withStyle (bold ? "Bold" : "Regular"));
}

// Wrap a C-string so JUCE always reads it as UTF-8 (needed for ' fractions, em-dash).
static juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

// MIDI note number (0..127) -> name, e.g. 60 -> "C4", 69 -> "A4".
static juce::String noteName (int midi)
{
    static const char* names[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    return juce::String (names[midi % 12]) + juce::String (midi / 12 - 1);
}

// ============================================================================
//  LOOK AND FEEL
// ============================================================================

juce::Colour TuneBfreeLookAndFeel::drawbarColour (int i)
{
    // Custom (non-standard) drawbar colours, matching the mockup:
    //   16' 5'/3 -> red | 8' 4' 2' 1' -> white | 2'/3 1'/5 1'/3 -> amber
    switch (i)
    {
        case 0: case 1:                  return kRed;
        case 2: case 3: case 5: case 8:  return kWhite;
        default:                         return kAmber;   // 4, 6, 7
    }
}

TuneBfreeLookAndFeel::TuneBfreeLookAndFeel()
{
    // Default button colours: inactive = dark fill + white text,
    //                         active   = amber fill + black text.
    setColour (juce::TextButton::buttonColourId,   kBtn);
    setColour (juce::TextButton::buttonOnColourId, kAmber);
    setColour (juce::TextButton::textColourOffId,  kWhite);
    setColour (juce::TextButton::textColourOnId,   kBtn);
    setColour (juce::Slider::thumbColourId,        kAmber);
    setColour (juce::Label::textColourId,          kWhite);
    setColour (juce::Label::backgroundColourId,    juce::Colours::transparentBlack);
    setColour (juce::PopupMenu::backgroundColourId, kPanel);
    setColour (juce::PopupMenu::textColourId,       kWhite);

    // Encoding dropdown: same dark fill / white text / amber arrow as the buttons.
    setColour (juce::ComboBox::backgroundColourId, kBtn);
    setColour (juce::ComboBox::textColourId,       kWhite);
    setColour (juce::ComboBox::outlineColourId,    kBorder);
    setColour (juce::ComboBox::arrowColourId,      kAmber);
}

void TuneBfreeLookAndFeel::drawLinearSlider (
    juce::Graphics& g, int x, int y, int w, int h,
    float sliderPos, float, float,
    juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto fill = slider.findColour (juce::Slider::thumbColourId);

    // ---- Vertical sliders (drawbars) ----
    if (style == juce::Slider::LinearVertical)
    {
        const float trackW = 5.0f;
        const float cx     = x + w * 0.5f;

        g.setColour (kBorder);
        g.fillRoundedRectangle (cx - trackW * 0.5f, (float) y, trackW, (float) h, 2.5f);

        // Drawbars use a reversed range, so sliderPos is already at the bottom
        // for high (loud) values. Fill from the top down to the cap.
        const float fillH = sliderPos - (float) y;
        if (fillH > 0.0f)
        {
            g.setColour (fill.withAlpha (0.55f));
            g.fillRoundedRectangle (cx - trackW * 0.5f, (float) y, trackW, fillH, 2.5f);
        }

        const float capH = 14.0f;
        const float capW = juce::jmin ((float) w - 4.0f, 30.0f);
        const float capY = juce::jlimit ((float) y, (float) (y + h) - capH, sliderPos - capH * 0.5f);
        juce::Rectangle<float> cap (cx - capW * 0.5f, capY, capW, capH);
        g.setColour (fill);
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (fill.brighter (0.25f));
        g.drawRoundedRectangle (cap.reduced (0.5f), 3.0f, 1.0f);
        return;
    }

    // ---- Fallback: default JUCE rendering ----
    LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, slider);
}

void TuneBfreeLookAndFeel::drawRotarySlider (
    juce::Graphics& g, int x, int y, int w, int h,
    float pos, float startAngle, float endAngle, juce::Slider&)
{
    // Always draw inside a centred square so every knob is the SAME size,
    // regardless of the (possibly non-square) bounds it was given.
    auto  area   = juce::Rectangle<int> (x, y, w, h).toFloat();
    float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 4.0f;
    float cx     = area.getCentreX();
    float cy     = area.getCentreY();

    // Body
    g.setColour (kPanel);
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (kBorder);
    g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    const float ringR = radius - 4.0f;

    // Background ring
    juce::Path track;
    track.addArc (cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f, startAngle, endAngle, true);
    g.setColour (kBorder);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value ring
    const float angle = startAngle + pos * (endAngle - startAngle);
    if (angle > startAngle)
    {
        juce::Path value;
        value.addArc (cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f, startAngle, angle, true);
        g.setColour (kAmber);
        g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer
    const float dot = ringR - 5.0f;
    g.setColour (kWhite);
    g.fillEllipse (cx + dot * std::sin (angle) - 2.5f, cy - dot * std::cos (angle) - 2.5f, 5.0f, 5.0f);
}

void TuneBfreeLookAndFeel::drawButtonBackground (
    juce::Graphics& g, juce::Button& button, const juce::Colour&,
    bool isHighlighted, bool)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    bool isOn   = button.getToggleState();

    // Use the button's own colours so individually-themed buttons (e.g. the
    // red TUNING button) work without special-casing them here.
    auto fill = isOn ? button.findColour (juce::TextButton::buttonOnColourId)
                     : button.findColour (juce::TextButton::buttonColourId);
    if (isHighlighted) fill = fill.brighter (0.12f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (isOn ? fill.brighter (0.15f) : kBorder);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
}

juce::Font TuneBfreeLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return uiFont (juce::jmin (12.0f, (float) buttonHeight * 0.5f));
}

juce::Font TuneBfreeLookAndFeel::getComboBoxFont  (juce::ComboBox&) { return uiFont (12.0f); }
juce::Font TuneBfreeLookAndFeel::getPopupMenuFont ()                { return uiFont (12.0f); }

// ============================================================================
//  SHARED HELPERS
// ============================================================================

// Wire a set of buttons as a mutually-exclusive (radio) group: clicking one
// turns it on and the others off. Used for every switch on the page. The optional
// onChange callback runs after the toggle update (used to push the new state to a
// parameter).
static void makeRadioGroup (std::initializer_list<juce::TextButton*> list,
                            std::function<void()> onChange = {})
{
    std::vector<juce::TextButton*> group (list);
    for (auto* b : group)
    {
        b->setClickingTogglesState (true);
        b->onClick = [group, b, onChange]
        {
            for (auto* other : group)
                other->setToggleState (other == b, juce::dontSendNotification);
            if (onChange) onChange();
        };
    }
}

// Style a label as a read-only info box (visible border + dark fill).
static void styleInfoBox (juce::Label& l, juce::Justification j = juce::Justification::centred)
{
    l.setFont (uiFont (11.0f));
    l.setJustificationType (j);
    l.setColour (juce::Label::backgroundColourId, kBtn);
    l.setColour (juce::Label::outlineColourId,    kBorder);
}

// Style a small caption label (the grey text above knobs etc.).
static void styleCaption (juce::Label& l, const juce::String& text)
{
    l.setFont (uiFont (10.0f));
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, kGrey);
    l.setText (text, juce::dontSendNotification);
}

// Style a section header ("STATUS", "SETTINGS"): muted grey, left-aligned.
static void styleSectionTitle (juce::Label& l, const juce::String& text)
{
    l.setFont (uiFont (11.0f, true));
    l.setJustificationType (juce::Justification::centredLeft);
    l.setColour (juce::Label::textColourId, kGrey);
    l.setText (text, juce::dontSendNotification);
}

// ============================================================================
//  CHANNELS popup (shown from the CHANNELS button via a CallOutBox).
//  MODE column: OMNI ON / OMNI OFF (radio) + SELECT ALL / DESELECT ALL.
//  CHANNELS column: 16 channel checkboxes (4×4). See TUNING_PANEL.md.
//  Selection gates which channels sound (for MTS *and* FILE); OMNI merges the
//  selected channels onto the generic channel. Always multi-select (no POLY).
// ============================================================================

class ChannelSelectorContent : public juce::Component
{
public:
    explicit ChannelSelectorContent (TuneBfreeAudioProcessor& p) : proc (p)
    {
        for (auto* b : { &omniOnBtn, &omniOffBtn })
        {
            b->setClickingTogglesState (true);
            addAndMakeVisible (*b);
        }
        omniOnBtn.onClick  = [this] { proc.setOmni (true);  syncOmni(); };
        omniOffBtn.onClick = [this] { proc.setOmni (false); syncOmni(); };

        selectAllBtn.onClick   = [this] { for (int c = 0; c < 16; ++c) proc.setChannelActive (c, true);  syncChannels(); };
        deselectAllBtn.onClick = [this] { for (int c = 0; c < 16; ++c) proc.setChannelActive (c, false); syncChannels(); };
        addAndMakeVisible (selectAllBtn);
        addAndMakeVisible (deselectAllBtn);

        for (int c = 0; c < 16; ++c)
        {
            auto* b = chanBtns.add (new juce::TextButton (juce::String (c + 1)));
            b->setClickingTogglesState (true);
            b->onClick = [this, c] { proc.setChannelActive (c, chanBtns[c]->getToggleState()); };
            addAndMakeVisible (b);
        }
        syncOmni();
        syncChannels();
        setSize (360, 8 + 4 * rowH_ + 3 * gap_ + 8);   // exactly 4 equal rows
    }

    ~ChannelSelectorContent() override { setLookAndFeel (nullptr); }

    void resized() override
    {
        const int modeW = 96;
        auto area = getLocalBounds().reduced (8);

        // Four equal rows: [mode button | 4 channel checkboxes].
        juce::TextButton* modeRows[4] = { &omniOnBtn, &omniOffBtn, &selectAllBtn, &deselectAllBtn };
        for (int row = 0; row < 4; ++row)
        {
            auto rowArea = area.removeFromTop (rowH_);
            if (row < 3) area.removeFromTop (gap_);
            modeRows[row]->setBounds (rowArea.removeFromLeft (modeW));
            rowArea.removeFromLeft (gap_);
            const int bw = rowArea.getWidth() / 4;
            for (int col = 0; col < 4; ++col)
                chanBtns[row * 4 + col]->setBounds (rowArea.removeFromLeft (bw).reduced (1));
        }
    }

private:
    void syncOmni()
    {
        const bool omni = proc.getOmni();
        omniOnBtn.setToggleState (omni,   juce::dontSendNotification);
        omniOffBtn.setToggleState (! omni, juce::dontSendNotification);
    }

    void syncChannels()
    {
        for (int c = 0; c < 16; ++c)
            chanBtns[c]->setToggleState (proc.getChannelActive (c), juce::dontSendNotification);
    }

    static constexpr int rowH_ = 28;
    static constexpr int gap_  = 6;

    TuneBfreeAudioProcessor& proc;
    juce::TextButton omniOnBtn { "OMNI ON" }, omniOffBtn { "OMNI OFF" };
    juce::TextButton selectAllBtn { "SEL ALL" }, deselectAllBtn { "DESEL ALL" };
    juce::OwnedArray<juce::TextButton> chanBtns;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelSelectorContent)
};

// ============================================================================
//  TUNING SIDE PANEL
// ============================================================================

TuningSidePanelContent::TuningSidePanelContent (TuneBfreeAudioProcessor& p) : proc (p)
{
    // ---- frequency read-out (top block) ----
    for (auto* l : { &penultimateHzLabel, &lastHzLabel, &centsLabel })
        { styleInfoBox (*l); addAndMakeVisible (l); }

    // ---- STATUS block ----
    styleSectionTitle (statusTitle, "STATUS");
    addAndMakeVisible (statusTitle);

    // Tuning name is centred to match the (centred) encoding dropdown below — the
    // first widget under each section title looks the same.
    styleInfoBox (scaleNameLabel, juce::Justification::centred);
    scaleNameLabel.setFont (uiFont (12.0f));
    addAndMakeVisible (scaleNameLabel);

    styleInfoBox (periodLabel);
    addAndMakeVisible (periodLabel);

    // Clock uses the same font as the other read-out boxes (styleInfoBox); only its
    // colour changes (green while a live MTS master ticks).
    styleInfoBox (timestampLabel);
    addAndMakeVisible (timestampLabel);

    // ---- SETTINGS block ----
    styleSectionTitle (settingsTitle, "SETTINGS");
    addAndMakeVisible (settingsTitle);

    // Encoding menu — selects which source feeds the engine. Item ids match the
    // TuningSourceId enum. MPE / MIDI 2.0 are shown but disabled.
    encodingBox.addItem ("MTS ESP",  TS_MTS);
    encodingBox.addItem ("SYSEX",    TS_SYSEX);
    encodingBox.addItem ("FILE",     TS_FILE);
    encodingBox.addItem ("MPE",      4);
    encodingBox.addItem ("MIDI 2.0", 5);
    encodingBox.addItem ("STANDARD", TS_STANDARD);
    encodingBox.setItemEnabled (4, false);
    encodingBox.setItemEnabled (5, false);
    encodingBox.setSelectedId (proc.getTuningSource(), juce::dontSendNotification);
    encodingBox.setJustificationType (juce::Justification::centred);
    encodingBox.onChange = [this] { proc.setTuningSource (encodingBox.getSelectedId()); refresh(); };
    addAndMakeVisible (encodingBox);

    // CHANNELS: opens the POLY/OMNI + channel-select popup (anchored to the button).
    channelsBtn.onClick = [this]
    {
        auto content = std::make_unique<ChannelSelectorContent> (proc);
        content->setLookAndFeel (&getLookAndFeel());   // match the plugin's look
        juce::CallOutBox::launchAsynchronously (std::move (content),
                                                channelsBtn.getScreenBounds(), nullptr);
    };
    addAndMakeVisible (channelsBtn);

    addAndMakeVisible (loadSclBtn);
    addAndMakeVisible (loadKbmBtn);

    loadSclBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load Scala Scale (.scl)", lastTuningDir, "*.scl");
        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto r = fc.getResults();
                if (! r.isEmpty())
                {
                    lastTuningDir = r[0].getParentDirectory();   // remember for next time
                    proc.loadSCLFile (r[0]);
                    maybeOfferSwitchToFile();
                    refresh();
                }
            });
    };

    loadKbmBtn.onClick = [this]
    {
        // Multi-select: several .kbm map to MIDI channels 1..N (per-channel multichannel
        // tuning of one .scl). A single selection behaves as before (all channels).
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load Keyboard Mapping(s) (.kbm) — one per MIDI channel", lastTuningDir, "*.kbm");
        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::canSelectMultipleItems,
            [this] (const juce::FileChooser& fc)
            {
                auto r = fc.getResults();
                if (! r.isEmpty())
                {
                    lastTuningDir = r[0].getParentDirectory();
                    proc.loadKBMFiles (r);
                    maybeOfferSwitchToFile();
                    refresh();
                }
            });
    };

    // NOTE ON vs ALWAYS retuning: a 2-way toggle (UI-only for now). "Always" lets a
    // sounding note change pitch; note-on is the default — it suits tuneBfree's
    // wavetable rebuild step.
    makeRadioGroup ({ &noteOnBtn, &alwaysBtn });
    noteOnBtn.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (noteOnBtn);
    addAndMakeVisible (alwaysBtn);

    refresh();
}

void TuningSidePanelContent::paint (juce::Graphics& g)
{
    g.fillAll (kPanel);   // solid grey, no border (per spec)
}

void TuningSidePanelContent::resized()
{
    const int pad     = 12;
    const int gap     = 8;
    const int btnH    = 30;
    const int titleH  = 18;

    auto r = getLocalBounds().reduced (pad);

    // Fixed heights of the three blocks (everything except the two section gaps).
    const int freqH     = btnH + gap + btnH;                 // Hz row + cents
    const int statusH   = titleH + gap + btnH * 3 + gap * 2; // title + 3 boxes
    const int settingsH = titleH + gap + btnH + gap          // title + menu
                        + btnH * 2 + gap;                     // loader / toggle row
    // Leftover height is split equally into the gap above each section title, so
    // the panel fills to the bottom with the same space above STATUS and SETTINGS.
    const int sectionGap = juce::jmax (gap, (r.getHeight() - freqH - statusH - settingsH) / 2);

    // --- top block: [penultimate Hz][last Hz] over [cents] (Hz fields half-width) ---
    auto hzRow = r.removeFromTop (btnH);
    penultimateHzLabel.setBounds (hzRow.removeFromLeft (hzRow.getWidth() / 2).withTrimmedRight (gap / 2));
    lastHzLabel.setBounds        (hzRow.withTrimmedLeft (gap / 2));
    r.removeFromTop (gap);
    centsLabel.setBounds (r.removeFromTop (btnH));

    // --- STATUS block ---
    r.removeFromTop (sectionGap);
    statusTitle.setBounds (r.removeFromTop (titleH));
    r.removeFromTop (gap);
    scaleNameLabel.setBounds (r.removeFromTop (btnH));
    r.removeFromTop (gap);
    periodLabel.setBounds (r.removeFromTop (btnH));
    r.removeFromTop (gap);
    timestampLabel.setBounds (r.removeFromTop (btnH));

    // --- SETTINGS block ---
    r.removeFromTop (sectionGap);
    settingsTitle.setBounds (r.removeFromTop (titleH));
    r.removeFromTop (gap);
    // Encoding menu (left) | CHANNELS button (right), as in TUNING_PANEL.md row 8.
    auto encRow = r.removeFromTop (btnH);
    encodingBox.setBounds (encRow.removeFromLeft (encRow.getWidth() / 2).withTrimmedRight (gap / 2));
    channelsBtn.setBounds (encRow.withTrimmedLeft (gap / 2));
    r.removeFromTop (gap);

    // Two columns: SCALE/MAP loaders (left) | NOTE ON / CONTINUOUS toggle (right).
    auto fileRow  = r.removeFromTop (btnH * 2 + gap);
    auto leftCol  = fileRow.removeFromLeft (fileRow.getWidth() / 2).withTrimmedRight (gap / 2);
    auto rightCol = fileRow.withTrimmedLeft (gap / 2);
    loadSclBtn.setBounds (leftCol.removeFromTop (btnH));
    leftCol.removeFromTop (gap);
    loadKbmBtn.setBounds (leftCol.removeFromTop (btnH));
    noteOnBtn.setBounds (rightCol.removeFromTop (btnH));
    rightCol.removeFromTop (gap);
    alwaysBtn.setBounds (rightCol.removeFromTop (btnH));
}

void TuningSidePanelContent::refresh()
{
    const int  src       = proc.getTuningSource();
    const bool connected = proc.isMTSConnected();
    const bool mtsLive    = (src == TS_MTS || src == TS_SYSEX) && connected;

    // --- tuning name: depends on the active source. "GEAR60 (~12EDO)" is the start-up
    //     state for every encoding, shown whenever nothing is providing a tuning. ---
    juce::String name;
    if (src == TS_FILE)
        name = proc.getLocalSclDescription().isNotEmpty() ? proc.getLocalSclDescription()
                                                          : proc.getLocalSclName();
    else if ((src == TS_MTS || src == TS_SYSEX) && connected)
        name = proc.getMTSScaleName();
    scaleNameLabel.setText (name.isNotEmpty() ? name.toUpperCase() : "GEAR60 (~12EDO)",
                            juce::dontSendNotification);

    // --- scale period (cents) ---
    // Under FILE, the .scl declares its own period (its last tone) -> "SPECIFIED".
    double sclPeriod = (src == TS_FILE) ? proc.getLocalSclPeriodCents() : -1.0;
    if (sclPeriod > 0.0)
    {
        periodLabel.setText (juce::String (sclPeriod, 0) + utf8 ("c \xc2\xb7 SPECIFIED PERIOD"),
                             juce::dontSendNotification);
    }
    else if (proc.getInferredPeriod() > 0.0f)
    {
        double cents = 1200.0 * std::log2 ((double) proc.getInferredPeriod());
        periodLabel.setText (juce::String (cents, 0) + utf8 ("c \xc2\xb7 INFERRED PERIOD"),
                             juce::dontSendNotification);
    }
    else
    {
        // No repeating period: treat the whole span of mapped notes as one period
        // and report that interval in cents — "NONE (x c)".
        int lo = -1, hi = -1;
        for (int n = 0; n < 128; ++n)
            if (proc.isMidiNoteMapped (n)) { if (lo < 0) lo = n; hi = n; }

        double fLo = lo >= 0 ? proc.getDisplayFrequency (lo) : 0.0;
        double fHi = hi >= 0 ? proc.getDisplayFrequency (hi) : 0.0;
        if (hi > lo && fLo > 0.0 && fHi > 0.0)
            periodLabel.setText (utf8 ("NONE (") + juce::String (1200.0 * std::log2 (fHi / fLo), 0)
                                     + utf8 (" c)"),
                                 juce::dontSendNotification);
        else
            periodLabel.setText ("NONE", juce::dontSendNotification);
    }

    // --- last-update clock: white text like the other boxes. While a live MTS source
    //     is queried it shows the current time, so the ticking seconds signal it is
    //     active; otherwise it shows the time of the last file load / sysex retune. ---
    if (mtsLive)
    {
        timestampLabel.setText (juce::Time::getCurrentTime().toString (false, true, true, true),
                                juce::dontSendNotification);
    }
    else
    {
        auto ms = proc.getLastTuningChangeMs();
        timestampLabel.setText (ms > 0 ? juce::Time (ms).toString (false, true, true, true)
                                       : utf8 ("\xe2\x80\x94"),   // em-dash
                                juce::dontSendNotification);
    }

    // --- frequency read-out: penultimate (left) and last (right) note-on ---
    // Use the actual sounding frequency (channel-aware), so two manuals at different
    // pitches read out differently. getLastNoteOn() is the "has anything played" check.
    int    pen  = proc.getPenultimateNoteOn();
    int    last = proc.getLastNoteOn();
    double fp   = proc.getPenultimateNoteFreq();
    double fl   = proc.getLastNoteFreq();
    auto   hz   = [] (double f) { return juce::String (f, 2) + " Hz"; };
    penultimateHzLabel.setText (pen  >= 0 ? hz (fp) : "? Hz", juce::dontSendNotification);
    lastHzLabel.setText        (last >= 0 ? hz (fl) : "? Hz", juce::dontSendNotification);

    if (pen >= 0 && last >= 0 && fp > 0.0 && fl > 0.0)
    {
        double cents = 1200.0 * std::log2 (fl / fp);
        centsLabel.setText ((cents >= 0.0 ? "+" : "") + juce::String (cents, 1) + " c",
                            juce::dontSendNotification);
    }
    else
    {
        centsLabel.setText ("? c", juce::dontSendNotification);
    }

    // --- file loaders: show the loaded filename, else the SCALE / MAP label ---
    auto scl = proc.getLocalSclName();
    auto kbm = proc.getLocalKbmName();
    loadSclBtn.setButtonText (scl.isNotEmpty() ? scl.toUpperCase() : "SCALE");
    loadKbmBtn.setButtonText (kbm.isNotEmpty() ? kbm.toUpperCase() : "MAP");
}

void TuningSidePanelContent::maybeOfferSwitchToFile()
{
    if (proc.getTuningSource() == TS_FILE)
        return;   // already using files

    juce::AlertWindow::showOkCancelBox (
        juce::MessageBoxIconType::QuestionIcon,
        "Tuning source",
        "A tuning file was loaded, but the active source is not FILE.\nSwitch to FILE now?",
        "Switch", "Cancel", this,
        juce::ModalCallbackFunction::create ([this] (int result)
        {
            if (result == 1)   // "Switch" -> sendNotification fires encodingBox.onChange
                encodingBox.setSelectedId (TS_FILE, juce::sendNotification);
        }));
}

// ============================================================================
//  DEFAULT PAGE
// ============================================================================

// Footage labels. ' fractions need UTF-8: ⅓ = e2 85 93, ⅔ = e2 85 94, ⅗ = e2 85 97
static const char* kFootage[9] = {
    "16'", "5\xe2\x85\x93'", "8'", "4'", "2\xe2\x85\x94'", "2'", "1\xe2\x85\x97'", "1\xe2\x85\x93'", "1'"
};

DefaultPage::DefaultPage (TuneBfreeAudioProcessor& p) : proc (p), tuningContent (p)
{
    // ---- LFO: vibrato / chorus / off (+ depth) -> vibrato, vibrato_type ----
    makeRadioGroup ({ &vibratoBtn, &chorusBtn, &modOffBtn }, [this] { applyLfoToParams(); });
    for (auto* b : { &vibratoBtn, &chorusBtn, &modOffBtn }) addAndMakeVisible (b);

    depthKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    depthKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    depthKnob.setRange (1.0, 3.0, 1.0);
    depthKnob.setValue (2.0, juce::dontSendNotification);
    depthKnob.onValueChange = [this] { applyLfoToParams(); };
    addAndMakeVisible (depthKnob);
    styleCaption (depthLabel, "DEPTH");
    addAndMakeVisible (depthLabel);

    // ---- Envelope: percussion, four 2-way vertical switches ----
    makeRadioGroup ({ &percOnBtn,   &percOffBtn  }, [this] { applyPercToParams(); });
    makeRadioGroup ({ &percFastBtn, &percSlowBtn }, [this] { applyPercToParams(); });
    makeRadioGroup ({ &percSoftBtn, &percNormBtn }, [this] { applyPercToParams(); });
    makeRadioGroup ({ &perc2ndBtn,  &perc3rdBtn  }, [this] { applyPercToParams(); });
    for (auto* b : { &percOnBtn, &percOffBtn, &percFastBtn, &percSlowBtn,
                     &percSoftBtn, &percNormBtn, &perc2ndBtn, &perc3rdBtn })
        addAndMakeVisible (b);

    // ---- Timbrality ----
    upperBtn.setClickingTogglesState (true);
    lowerBtn.setClickingTogglesState (true);
    upperBtn.setToggleState (true, juce::dontSendNotification);
    upperBtn.onClick = [this] {
        if (! isUpper) switchToManual (true);
        upperBtn.setToggleState (true,  juce::dontSendNotification);
        lowerBtn.setToggleState (false, juce::dontSendNotification);
    };
    lowerBtn.onClick = [this] {
        if (isUpper) switchToManual (false);
        lowerBtn.setToggleState (true,  juce::dontSendNotification);
        upperBtn.setToggleState (false, juce::dontSendNotification);
    };
    addAndMakeVisible (upperBtn);
    addAndMakeVisible (lowerBtn);

    bitimbralBtn.setClickingTogglesState (true);
    bitimbralBtn.onClick = [this] { setParam ("split_enable", bitimbralBtn.getToggleState() ? 1.0f : 0.0f); };
    addAndMakeVisible (bitimbralBtn);

    // LEARN: arm the "set split from played notes" mode. The processor disarms itself
    // when all notes are released; the timer un-toggles the button then.
    learnBtn.setClickingTogglesState (true);
    learnBtn.onClick = [this] { proc.setLearnSplit (learnBtn.getToggleState()); };
    addAndMakeVisible (learnBtn);

    for (auto* k : { &splitKnob, &crossfadeKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (k);
    }
    // Split point is a frequency (Hz); the label shows the nearest note. Skewed so the
    // musically useful low/mid range gets most of the travel.
    splitKnob.setRange (20.0, 4000.0);
    splitKnob.setSkewFactor (0.3);
    splitKnob.setValue (261.63, juce::dontSendNotification);   // ~C4
    splitKnob.onValueChange = [this] { setParam ("split_point", (float) splitKnob.getValue()); updateSplitNoteLabel(); };
    // Crossfade width in cents (0 = hard split).
    crossfadeKnob.setRange (0.0, 1200.0);
    crossfadeKnob.setValue (0.0, juce::dontSendNotification);
    crossfadeKnob.onValueChange = [this] { setParam ("split_width", (float) crossfadeKnob.getValue()); };

    styleCaption (splitLabel, "SPLIT");
    addAndMakeVisible (splitLabel);
    splitNoteLabel.setFont (uiFont (12.0f, true));
    splitNoteLabel.setJustificationType (juce::Justification::centred);
    splitNoteLabel.setColour (juce::Label::textColourId, kAmber);
    addAndMakeVisible (splitNoteLabel);
    updateSplitNoteLabel();

    styleCaption (crossfadeLabel, "CROSSFADE");
    addAndMakeVisible (crossfadeLabel);

    // ---- Drawbars ----
    for (int i = 0; i < 9; ++i)
    {
        auto& db = drawbars[i];
        db.setSliderStyle (juce::Slider::LinearVertical);
        db.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        // Reversed range so dragging DOWN increases the value (Hammond style):
        // value 8 (loud) sits at the bottom, value 0 (silent) at the top.
        db.setNormalisableRange (juce::NormalisableRange<double> (
            0.0, 8.0,
            [] (double s, double e, double v)     { return e - (e - s) * v; },
            [] (double s, double e, double value) { return (e - value) / (e - s); },
            [] (double, double, double value)     { return std::round (value); }));
        db.setValue ((double) upperState.drawbars[i], juce::dontSendNotification);
        db.setColour (juce::Slider::thumbColourId, TuneBfreeLookAndFeel::drawbarColour (i));
        db.getProperties().set ("drawbar", true);
        // Route to the active manual's drawbar param (upper "drawbar" / lower "lower_drawbar").
        db.onValueChange = [this, i] {
            setParam ((isUpper ? "drawbar" : "lower_drawbar") + juce::String (i), (float) drawbars[i].getValue());
        };
        addAndMakeVisible (db);

        styleCaption (footageLabels[i], utf8 (kFootage[i]));
        addAndMakeVisible (footageLabels[i]);
    }

    // ---- Leslie ----
    makeRadioGroup ({ &choraleBtn, &stopBtn, &tremoloBtn }, [this] { applyLeslieToParams(); });
    choraleBtn.setToggleState (true, juce::dontSendNotification);
    for (auto* b : { &choraleBtn, &stopBtn, &tremoloBtn }) addAndMakeVisible (b);

    // ---- Expression (knob; same size as every other knob) ----
    expressionKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    expressionKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    expressionKnob.setRange (0.0, 1.0);
    expressionKnob.setValue (1.0, juce::dontSendNotification);
    expressionKnob.onValueChange = [this] { setParam ("expression", (float) expressionKnob.getValue()); };
    addAndMakeVisible (expressionKnob);
    styleCaption (expressionLabel, "EXPRESSION");
    addAndMakeVisible (expressionLabel);

    // ---- Effects ----
    for (auto* k : { &driveKnob, &reverbKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->setRange (0.0, 1.0);
        addAndMakeVisible (k);
    }
    // DRIVE drives the overdrive "character" (0..1) and switches overdrive on when > 0.
    driveKnob.onValueChange = [this]
    {
        float d = (float) driveKnob.getValue();
        setParam ("character", d);
        setParam ("overdrive", d > 0.0f ? 1.0f : 0.0f);
    };
    reverbKnob.onValueChange = [this] { setParam ("reverb_mix", (float) reverbKnob.getValue()); };
    styleCaption (driveLabel,  "DRIVE");
    styleCaption (reverbLabel, "REVERB");
    addAndMakeVisible (driveLabel);
    addAndMakeVisible (reverbLabel);

    // Split / bitimbral controls are now wired to the engine (step 2), so they're
    // enabled. (EXPRESSION was already wired to the swell pedal.)

    // Add the tuning overlay LAST so it paints on top of everything.
    addChildComponent (tuningContent);

    // Reflect the processor's current parameter values in every wired control.
    syncFromParams();
}

// ---- State helpers ----

void DefaultPage::setModButtons (int mode)
{
    vibratoBtn.setToggleState (mode == 1, juce::dontSendNotification);
    chorusBtn.setToggleState  (mode == 2, juce::dontSendNotification);
    modOffBtn.setToggleState  (mode == 0, juce::dontSendNotification);
}

void DefaultPage::setPercButtons (bool on, bool fast, bool soft, bool third)
{
    percOnBtn.setToggleState  (on,     juce::dontSendNotification);
    percOffBtn.setToggleState (! on,   juce::dontSendNotification);
    percFastBtn.setToggleState (fast,  juce::dontSendNotification);
    percSlowBtn.setToggleState (! fast, juce::dontSendNotification);
    percSoftBtn.setToggleState (soft,  juce::dontSendNotification);
    percNormBtn.setToggleState (! soft, juce::dontSendNotification);
    perc2ndBtn.setToggleState (! third, juce::dontSendNotification);
    perc3rdBtn.setToggleState (third,  juce::dontSendNotification);
}

void DefaultPage::updateSplitNoteLabel()
{
    const double hz = splitKnob.getValue();
    const int note = (hz > 0.0)
        ? juce::jlimit (0, 127, (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0)))
        : 60;
    splitNoteLabel.setText (noteName (note), juce::dontSendNotification);
}

ManualState DefaultPage::captureStateFromControls() const
{
    ManualState s;
    for (int i = 0; i < 9; ++i)
        s.drawbars[i] = (float) drawbars[i].getValue();
    s.vibratoMode = vibratoBtn.getToggleState() ? 1 : chorusBtn.getToggleState() ? 2 : 0;
    s.depth       = juce::roundToInt (depthKnob.getValue());
    s.percOn      = percOnBtn.getToggleState();
    s.percFast    = percFastBtn.getToggleState();
    s.percSoft    = percSoftBtn.getToggleState();
    s.percThird   = perc3rdBtn.getToggleState();
    return s;
}

void DefaultPage::updateControlsFromState (const ManualState& s)
{
    for (int i = 0; i < 9; ++i)
        drawbars[i].setValue ((double) s.drawbars[i], juce::dontSendNotification);
    depthKnob.setValue ((double) s.depth, juce::dontSendNotification);
    setModButtons  (s.vibratoMode);
    setPercButtons (s.percOn, s.percFast, s.percSoft, s.percThird);
}

void DefaultPage::switchToManual (bool toUpper)
{
    // Drawbars are engine-backed per manual (upper "drawbar" / lower "lower_drawbar"),
    // so switching just reloads the bank from the newly-active manual's parameters.
    isUpper = toUpper;
    for (int i = 0; i < 9; ++i)
        drawbars[i].setValue (getParam ((isUpper ? "drawbar" : "lower_drawbar") + juce::String (i)),
                              juce::dontSendNotification);
}

// ---- Engine wiring (control -> parameter) ----

void DefaultPage::setParam (const juce::String& id, float realValue)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

float DefaultPage::getParam (const juce::String& id) const
{
    return proc.apvts.getRawParameterValue (id)->load();
}

// VIBRATO/CHORUS/OFF + DEPTH -> vibrato (on/off) + vibrato_type.
// vibrato_type is interleaved: 0=V1 1=C1 2=V2 3=C2 4=V3 5=C3 (vibrato.cpp).
void DefaultPage::applyLfoToParams()
{
    const bool off = modOffBtn.getToggleState();
    setParam ("vibrato", off ? 0.0f : 1.0f);
    if (! off)
    {
        int depth = juce::jlimit (1, 3, (int) std::lround (depthKnob.getValue()));
        int type  = 2 * (depth - 1) + (chorusBtn.getToggleState() ? 1 : 0);
        setParam ("vibrato_type", (float) type);
    }
}

// CHORALE/STOP/TREMOLO -> drum & horn together (0=stop, 1=slow, 2=fast; whirl.cpp).
void DefaultPage::applyLeslieToParams()
{
    int speed = stopBtn.getToggleState() ? 0 : (choraleBtn.getToggleState() ? 1 : 2);
    setParam ("drum", (float) speed);
    setParam ("horn", (float) speed);
}

// The four percussion 2-way switches. NOTE: percussion_vol is inverted in the
// engine (applyParam does 1 - value), so SOFT -> 0 and NORMAL/HARD -> 1.
void DefaultPage::applyPercToParams()
{
    setParam ("percussion",     percOnBtn.getToggleState()   ? 1.0f : 0.0f);
    setParam ("percussion_dec", percFastBtn.getToggleState() ? 1.0f : 0.0f);
    setParam ("percussion_vol", percSoftBtn.getToggleState() ? 0.0f : 1.0f);
    setParam ("percussion_har", perc3rdBtn.getToggleState()  ? 1.0f : 0.0f);
}

void DefaultPage::syncFromParams()
{
    // Don't fight the user mid-gesture; resume syncing once the mouse is released.
    if (juce::Component::isMouseButtonDownAnywhere())
        return;

    // Split "learn": while armed, push the processor's learned values into the params so
    // the knobs move live; the button reflects the armed state (processor auto-disarms
    // when all notes are released).
    const bool learning = proc.isLearnSplitActive();
    if (learning)
    {
        setParam ("split_point", (float) proc.getLearnedSplitPoint());
        setParam ("split_width", (float) proc.getLearnedSplitWidth());
    }
    learnBtn.setToggleState (learning, juce::dontSendNotification);

    // Split controls reflect the parameters.
    bitimbralBtn.setToggleState (getParam ("split_enable") > 0.5f, juce::dontSendNotification);
    splitKnob.setValue     (getParam ("split_point"), juce::dontSendNotification);
    crossfadeKnob.setValue (getParam ("split_width"), juce::dontSendNotification);
    updateSplitNoteLabel();

    // Drawbars reflect the ACTIVE manual's parameters.
    for (int i = 0; i < 9; ++i)
        drawbars[i].setValue (getParam ((isUpper ? "drawbar" : "lower_drawbar") + juce::String (i)),
                              juce::dontSendNotification);

    reverbKnob.setValue     (getParam ("reverb_mix"), juce::dontSendNotification);
    driveKnob.setValue      (getParam ("character"),  juce::dontSendNotification);
    expressionKnob.setValue (getParam ("expression"), juce::dontSendNotification);

    // LFO: rebuild mode + depth from vibrato / vibrato_type.
    const bool vibOn = getParam ("vibrato") > 0.5f;
    const int  vtype = (int) std::lround (getParam ("vibrato_type"));
    setModButtons (vibOn ? ((vtype % 2 == 1) ? 2 : 1) : 0);   // odd type = chorus
    depthKnob.setValue (vtype / 2 + 1, juce::dontSendNotification);

    // Percussion (note the inverted percussion_vol: value 0 = soft).
    setPercButtons (getParam ("percussion")     > 0.5f,
                    getParam ("percussion_dec") > 0.5f,
                    getParam ("percussion_vol") < 0.5f,
                    getParam ("percussion_har") > 0.5f);

    // Leslie: derive the 3-way from the horn speed.
    const int horn = (int) std::lround (getParam ("horn"));
    choraleBtn.setToggleState (horn == 1, juce::dontSendNotification);
    stopBtn.setToggleState    (horn == 0, juce::dontSendNotification);
    tremoloBtn.setToggleState (horn == 2, juce::dontSendNotification);
}

// ---- Tuning panel ----

void DefaultPage::toggleTuningPanel()       { tuningContent.setVisible (! tuningContent.isVisible()); }
bool DefaultPage::isTuningPanelShowing() const { return tuningContent.isVisible(); }
void DefaultPage::refreshTuningPanel()      { tuningContent.refresh(); }

// ============================================================================
//  DEFAULT PAGE LAYOUT
//  ---------------------------------------------------------------------------
//  All sizes come from the named constants below. Free vertical space is put
//  BETWEEN sections (never as a gap at the window edges).
// ============================================================================

void DefaultPage::paint (juce::Graphics& g) { g.fillAll (kBg); }

void DefaultPage::resized()
{
    // ---- tuning grid constants (edit these to retune the layout) ----
    const int margin   = 14;  // uniform breathing room around / between sections
    const int knob     = 62;  // diameter of EVERY knob (kept identical)
    const int btnH     = 30;  // height of a standard button
    const int capH     = 14;  // height of a caption label
    const int gap      = 5;   // small gap between the stacked buttons of a switch
    const int twoWayW  = 56;  // width of EACH half of a 2-way switch
    const int switchW  = twoWayW * 2 + 4;       // 3-way switch == 2-way switch total width
    const int timbralW = 122; // width of the timbrality sub-column
    const int effectsW = 158; // width of the effects/leslie/expression sub-column
    const int rightW   = timbralW + effectsW;
    const int tuningPanelW = rightW + margin * 3 / 2;

    const int bandH    = btnH * 3 + gap * 2;    // height of the vibrato 3-way switch

    auto area = getLocalBounds().reduced (margin);          // uniform margin on all sides

    // Cross-column vertical anchors — must mirror the RIGHT-region layout below:
    //   splitLabelY  — top of the SPLIT caption (centre of the middle row)
    //   knobBaseline — bottom edge of the CROSSFADE / EXPRESSION knobs
    // The drawbar captions (LEFT region) top-align with splitLabelY and the
    // drawbar bottoms align with knobBaseline, so the drawbar travel is derived
    // from these anchors rather than a fixed height.
    const int splitBlockH  = capH + knob + capH;
    const int midTop       = area.getY() + bandH;
    const int midBottom    = area.getBottom() - (capH + knob);
    const int splitLabelY  = midTop + (midBottom - midTop - splitBlockH) / 2;
    const int knobBaseline = area.getBottom();

    auto rightRegion = area.removeFromRight (rightW);
    area.removeFromRight (margin);                          // gap between left & right
    auto leftRegion = area;

    // =====================================================================
    //  LEFT REGION
    //  Top band (LFO + envelope), drawbars anchored to the bottom. The freed
    //  space sits BETWEEN the band and the drawbars (never at an edge).
    // =====================================================================
    {
        auto band = leftRegion.removeFromTop (bandH);

        // VIBRATO / CHORUS / OFF (3-way switch)
        auto vc = band.removeFromLeft (switchW);
        vibratoBtn.setBounds (vc.removeFromTop (btnH)); vc.removeFromTop (gap);
        chorusBtn.setBounds  (vc.removeFromTop (btnH)); vc.removeFromTop (gap);
        modOffBtn.setBounds  (vc.removeFromTop (btnH));

        band.removeFromLeft (margin);

        // DEPTH knob — top-anchored so it lines up with DRIVE/REVERB on the right.
        auto depthCol = band.removeFromLeft (knob + 8);
        depthLabel.setBounds (depthCol.removeFromTop (capH));
        depthKnob.setBounds  (depthCol.removeFromTop (knob).withSizeKeepingCentre (knob, knob));

        // PERCUSSION — four 2-way switches, RIGHT-aligned so their right edge
        // matches the drawbars below. Button height makes the 2-stack equal the
        // vibrato 3-stack (tops AND bottoms line up).
        const int percW   = 44;
        const int percGap = 8;
        const int percBtnH = (bandH - gap) / 2;
        auto percRow = band.removeFromRight (percW * 4 + percGap * 3);
        juce::TextButton* tops[4] = { &percOnBtn,  &percFastBtn, &percSoftBtn, &perc2ndBtn };
        juce::TextButton* bots[4] = { &percOffBtn, &percSlowBtn, &percNormBtn, &perc3rdBtn };
        for (int i = 0; i < 4; ++i)
        {
            auto cell = percRow.removeFromLeft (percW);
            tops[i]->setBounds (cell.removeFromTop (percBtnH)); cell.removeFromTop (gap);
            bots[i]->setBounds (cell.removeFromTop (percBtnH));
            percRow.removeFromLeft (percGap);
        }

        // DRAWBARS — footage captions ABOVE the bars (correction: matches the
        // labels-above-knob layout used elsewhere on the page). Two cross-column
        // alignments, both driven by the anchors computed at the top of resized():
        //   • the caption row top-aligns with the SPLIT label on the right
        //   • the drawbar bottoms align with the CROSSFADE / EXPRESSION knob bottoms
        auto bars = leftRegion;
        bars.setTop    (splitLabelY);
        bars.setBottom (knobBaseline);
        auto labels = bars.removeFromTop (capH);
        int  cellW  = bars.getWidth() / 9;
        for (int i = 0; i < 9; ++i)
        {
            drawbars[i].setBounds      (bars.removeFromLeft (cellW).reduced (4, 0));
            footageLabels[i].setBounds (labels.removeFromLeft (cellW));
        }
    }

    // =====================================================================
    //  RIGHT REGION — two sub-columns sharing three aligned rows:
    //    Row A (top)    : UPPER/LOWER+BITIMBRAL | DRIVE+REVERB
    //    Row C (bottom) : CROSSFADE knob        | EXPRESSION knob   (aligned)
    //    Row B (middle) : SPLIT knob            | LESLIE            (fills the rest)
    // =====================================================================
    auto timbral = rightRegion.removeFromLeft (timbralW).withTrimmedRight (margin / 2);
    auto effects = rightRegion.withTrimmedLeft (margin / 2);

    // ---- Row A: top, height == left band so DEPTH lines up with DRIVE/REVERB ----
    {
        auto tA = timbral.removeFromTop (bandH);
        auto ul = tA.removeFromTop (btnH).withSizeKeepingCentre (switchW, btnH);
        upperBtn.setBounds (ul.removeFromLeft (twoWayW));
        lowerBtn.setBounds (ul.removeFromRight (twoWayW));
        tA.removeFromTop (gap);
        bitimbralBtn.setBounds (tA.removeFromTop (btnH).withSizeKeepingCentre (switchW, btnH));

        auto eA = effects.removeFromTop (bandH);
        auto dr = eA.removeFromLeft (eA.getWidth() / 2);
        driveLabel.setBounds (dr.removeFromTop (capH));
        driveKnob.setBounds  (dr.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
        reverbLabel.setBounds (eA.removeFromTop (capH));
        reverbKnob.setBounds  (eA.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
    }

    // ---- Row C: bottom knobs (CROSSFADE and EXPRESSION line up) ----
    {
        auto tC = timbral.removeFromBottom (capH + knob);
        crossfadeLabel.setBounds (tC.removeFromTop (capH));
        crossfadeKnob.setBounds  (tC.withSizeKeepingCentre (knob, knob));

        auto eC = effects.removeFromBottom (capH + knob);
        expressionLabel.setBounds (eC.removeFromTop (capH));
        expressionKnob.setBounds  (eC.withSizeKeepingCentre (knob, knob));
    }

    // ---- Row B: middle, fills the remaining space (SPLIT knob + LEARN | LESLIE) ----
    {
        auto splitBlock = timbral.withSizeKeepingCentre (timbral.getWidth(), capH + knob + capH + gap + btnH);
        splitLabel.setBounds     (splitBlock.removeFromTop (capH));
        splitKnob.setBounds      (splitBlock.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
        splitNoteLabel.setBounds (splitBlock.removeFromTop (capH));
        splitBlock.removeFromTop (gap);
        learnBtn.setBounds       (splitBlock.removeFromTop (btnH).withSizeKeepingCentre (switchW, btnH));

        auto leslie = effects.withSizeKeepingCentre (switchW, bandH);
        choraleBtn.setBounds (leslie.removeFromTop (btnH)); leslie.removeFromTop (gap);
        stopBtn.setBounds    (leslie.removeFromTop (btnH)); leslie.removeFromTop (gap);
        tremoloBtn.setBounds (leslie.removeFromTop (btnH));
    }

    // The tuning panel covers the right region plus half the centre gap, so its
    // left edge lands in the MIDDLE of that margin (correction), leaving the
    // drawbars + vibrato/depth/percussion strip clear. tuningPanelW is defined
    // with the layout constants at the top of resized().
    tuningContent.setBounds (getLocalBounds().removeFromRight (tuningPanelW));
}

// ============================================================================
//  TOP-LEVEL EDITOR
// ============================================================================

TuneBfreeAudioProcessorEditor::TuneBfreeAudioProcessorEditor (TuneBfreeAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), defaultPage (p)
{
    setLookAndFeel (&laf);

    // Header: amber background, black title.
    titleLabel.setFont (uiFont (17.0f, true));
    titleLabel.setText ("tuneBfree", juce::dontSendNotification);
    titleLabel.setColour (juce::Label::textColourId, kBtn);
    addAndMakeVisible (titleLabel);

    // TUNING button: dark normally, RED when the panel is open.
    tuningBtn.setClickingTogglesState (false);
    tuningBtn.setColour (juce::TextButton::buttonColourId,   kBtn);
    tuningBtn.setColour (juce::TextButton::buttonOnColourId, kRed);
    tuningBtn.setColour (juce::TextButton::textColourOffId,  kAmber);
    tuningBtn.setColour (juce::TextButton::textColourOnId,   kWhite);
    tuningBtn.onClick = [this]
    {
        defaultPage.toggleTuningPanel();
        tuningBtn.setToggleState (defaultPage.isTuningPanelShowing(), juce::dontSendNotification);
    };
    addAndMakeVisible (tuningBtn);

    // PANIC: release all notes (temporary, for debugging stuck notes). Sits left of TUNING.
    panicBtn.setClickingTogglesState (false);
    panicBtn.setColour (juce::TextButton::buttonColourId,  kBtn);
    panicBtn.setColour (juce::TextButton::textColourOffId, kAmber);
    panicBtn.onClick = [this] { proc.triggerPanic(); };
    addAndMakeVisible (panicBtn);

    addAndMakeVisible (defaultPage);

    setSize (740, 430);
    startTimerHz (15);   // drives the tuning clock + control sync (host automation / presets)
}

TuneBfreeAudioProcessorEditor::~TuneBfreeAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TuneBfreeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kAmber);
    g.fillRect (getLocalBounds().removeFromTop (44));   // header strip
}

void TuneBfreeAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (44);
    titleLabel.setBounds (header.removeFromLeft (180).reduced (12, 8));
    tuningBtn.setBounds  (header.removeFromRight (96).reduced (10, 8));
    panicBtn.setBounds   (header.removeFromRight (96).reduced (10, 8));   // left of TUNING
    defaultPage.setBounds (r);
}

void TuneBfreeAudioProcessorEditor::timerCallback()
{
    defaultPage.syncFromParams();
    if (defaultPage.isTuningPanelShowing())
        defaultPage.refreshTuningPanel();
}
