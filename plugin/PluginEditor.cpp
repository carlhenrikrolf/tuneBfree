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
static const juce::Colour kGreen     { 0xff4cce5c }; // "connected" status only
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

// ============================================================================
//  SHARED HELPERS
// ============================================================================

// Wire a set of buttons as a mutually-exclusive (radio) group: clicking one
// turns it on and the others off. Used for every switch on the page.
static void makeRadioGroup (std::initializer_list<juce::TextButton*> list)
{
    std::vector<juce::TextButton*> group (list);
    for (auto* b : group)
    {
        b->setClickingTogglesState (true);
        b->onClick = [group, b]
        {
            for (auto* other : group)
                other->setToggleState (other == b, juce::dontSendNotification);
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

// ============================================================================
//  TUNING SIDE PANEL
// ============================================================================

TuningSidePanelContent::TuningSidePanelContent (TuneBfreeAudioProcessor& p) : proc (p)
{
    styleInfoBox (scaleNameLabel, juce::Justification::centredLeft);
    scaleNameLabel.setFont (uiFont (12.0f));
    addAndMakeVisible (scaleNameLabel);

    styleInfoBox (statusArea);
    statusArea.setFont (uiFont (13.0f, true));
    addAndMakeVisible (statusArea);

    styleInfoBox (periodLabel);
    addAndMakeVisible (periodLabel);

    makeRadioGroup ({ &monoBtn, &polyBtn });
    polyBtn.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (monoBtn);
    addAndMakeVisible (polyBtn);

    stdBtn.setClickingTogglesState (true);
    stdBtn.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (stdBtn);

    onlyAtNoteOnBtn.setClickingTogglesState (true);
    addAndMakeVisible (onlyAtNoteOnBtn);

    addAndMakeVisible (loadSclBtn);
    addAndMakeVisible (loadKbmBtn);

    loadSclBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load Scala Scale (.scl)",
            juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.scl");
        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto r = fc.getResults();
                if (! r.isEmpty()) { proc.loadSCLFile (r[0]); refresh(); }
            });
    };

    loadKbmBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load Keyboard Mapping (.kbm)",
            juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.kbm");
        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto r = fc.getResults();
                if (! r.isEmpty()) { proc.loadKBMFile (r[0]); refresh(); }
            });
    };

    for (auto* l : { &lastHzLabel, &currentHzLabel, &centsLabel })
        { styleInfoBox (*l); addAndMakeVisible (l); }
    lastHzLabel.setText    ("? Hz", juce::dontSendNotification);
    currentHzLabel.setText ("? Hz", juce::dontSendNotification);
    centsLabel.setText     ("? c",  juce::dontSendNotification);

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
    const int twoWayW = 56;   // same as UPPER/LOWER on the main page

    auto r = getLocalBounds().reduced (pad);

    // --- bottom: [last Hz][current Hz] over [cents] (Hz fields half-width) ---
    centsLabel.setBounds (r.removeFromBottom (btnH));
    r.removeFromBottom (gap);
    auto hzRow = r.removeFromBottom (btnH);
    lastHzLabel.setBounds    (hzRow.removeFromLeft  (hzRow.getWidth() / 2).withTrimmedRight (gap / 2));
    currentHzLabel.setBounds (hzRow.withTrimmedLeft (gap / 2));

    // --- top: status, then settings, then file loaders (top-anchored) ---
    scaleNameLabel.setBounds (r.removeFromTop (btnH));
    r.removeFromTop (gap);
    statusArea.setBounds (r.removeFromTop (50));
    r.removeFromTop (gap);
    periodLabel.setBounds (r.removeFromTop (btnH));
    r.removeFromTop (gap * 2);

    // MONO/POLY: fixed-size 2-way switch (matches UPPER/LOWER); STD beside it.
    auto mpRow = r.removeFromTop (btnH);
    monoBtn.setBounds (mpRow.removeFromLeft (twoWayW));
    polyBtn.setBounds (mpRow.removeFromLeft (twoWayW));
    mpRow.removeFromLeft (gap);
    stdBtn.setBounds (mpRow);
    r.removeFromTop (gap);

    onlyAtNoteOnBtn.setBounds (r.removeFromTop (btnH));
    r.removeFromTop (gap * 2);

    loadSclBtn.setBounds (r.removeFromTop (btnH));
    r.removeFromTop (gap);
    loadKbmBtn.setBounds (r.removeFromTop (btnH));
    // whatever is left between the loaders and the Hz row stays empty (centre gap)
}

void TuningSidePanelContent::refresh()
{
    bool connected = proc.isMTSConnected();
    statusArea.setText (connected ? "CONNECTED" : "NO MASTER", juce::dontSendNotification);
    statusArea.setColour (juce::Label::textColourId, connected ? kGreen : kGrey);

    scaleNameLabel.setText (connected ? proc.getMTSScaleName().toUpperCase()
                                      : utf8 ("\xe2\x80\x94"),   // em-dash
                            juce::dontSendNotification);

    // Scale period: shown in cents. Source is currently always "inferred" from
    // the tuning table; when MTS-ESP exposes a period we'll add an "MTS" state.
    float period = proc.getInferredPeriod();
    if (period > 0.0f)
    {
        double cents = 1200.0 * std::log2 ((double) period);
        periodLabel.setText (utf8 ("PERIOD ") + juce::String (cents, 0)
                                 + utf8 ("c \xc2\xb7 INFERRED"),   // middle dot
                             juce::dontSendNotification);
    }
    else
    {
        periodLabel.setText (utf8 ("PERIOD \xc2\xb7 APERIODIC"), juce::dontSendNotification);
    }

    auto scl = proc.getLocalSclName();
    auto kbm = proc.getLocalKbmName();
    loadSclBtn.setButtonText (scl.isNotEmpty() ? scl.toUpperCase() : utf8 ("LOAD .SCL\xe2\x80\xa6"));
    loadKbmBtn.setButtonText (kbm.isNotEmpty() ? kbm.toUpperCase() : utf8 ("LOAD .KBM\xe2\x80\xa6"));
}

// ============================================================================
//  DEFAULT PAGE
// ============================================================================

// Footage labels. ' fractions need UTF-8: ⅓ = e2 85 93, ⅔ = e2 85 94, ⅗ = e2 85 97
static const char* kFootage[9] = {
    "16'", "5\xe2\x85\x93'", "8'", "4'", "2\xe2\x85\x94'", "2'", "1\xe2\x85\x97'", "1\xe2\x85\x93'", "1'"
};

DefaultPage::DefaultPage (TuneBfreeAudioProcessor& p) : tuningContent (p)
{
    // ---- LFO: vibrato / chorus / off ----
    makeRadioGroup ({ &vibratoBtn, &chorusBtn, &modOffBtn });
    for (auto* b : { &vibratoBtn, &chorusBtn, &modOffBtn }) addAndMakeVisible (b);

    depthKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    depthKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    depthKnob.setRange (1.0, 3.0, 1.0);
    depthKnob.setValue (2.0, juce::dontSendNotification);
    addAndMakeVisible (depthKnob);
    styleCaption (depthLabel, "DEPTH");
    addAndMakeVisible (depthLabel);

    // ---- Envelope: percussion, four 2-way vertical switches ----
    makeRadioGroup ({ &percOnBtn,   &percOffBtn  });
    makeRadioGroup ({ &percFastBtn, &percSlowBtn });
    makeRadioGroup ({ &percSoftBtn, &percHardBtn });
    makeRadioGroup ({ &perc2ndBtn,  &perc3rdBtn  });
    for (auto* b : { &percOnBtn, &percOffBtn, &percFastBtn, &percSlowBtn,
                     &percSoftBtn, &percHardBtn, &perc2ndBtn, &perc3rdBtn })
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
    addAndMakeVisible (bitimbralBtn);

    for (auto* k : { &splitKnob, &crossfadeKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (k);
    }
    splitKnob.setRange (0.0, 127.0, 1.0);
    splitKnob.setValue (60.0, juce::dontSendNotification);   // C4
    splitKnob.onValueChange = [this] { updateSplitNoteLabel(); };
    crossfadeKnob.setRange (0.0, 24.0, 1.0);
    crossfadeKnob.setValue (6.0, juce::dontSendNotification);

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
        addAndMakeVisible (db);

        styleCaption (footageLabels[i], utf8 (kFootage[i]));
        addAndMakeVisible (footageLabels[i]);
    }

    // ---- Leslie ----
    makeRadioGroup ({ &choraleBtn, &stopBtn, &tremoloBtn });
    choraleBtn.setToggleState (true, juce::dontSendNotification);
    for (auto* b : { &choraleBtn, &stopBtn, &tremoloBtn }) addAndMakeVisible (b);

    // ---- Expression (knob; same size as every other knob) ----
    expressionKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    expressionKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    expressionKnob.setRange (0.0, 1.0);
    expressionKnob.setValue (0.8, juce::dontSendNotification);
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
    styleCaption (driveLabel,  "DRIVE");
    styleCaption (reverbLabel, "REVERB");
    addAndMakeVisible (driveLabel);
    addAndMakeVisible (reverbLabel);

    // Initial UI state
    setModButtons  (upperState.vibratoMode);
    setPercButtons (upperState.percOn, upperState.percFast, upperState.percSoft, upperState.percThird);

    // Add the tuning overlay LAST so it paints on top of everything.
    addChildComponent (tuningContent);
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
    percHardBtn.setToggleState (! soft, juce::dontSendNotification);
    perc2ndBtn.setToggleState (! third, juce::dontSendNotification);
    perc3rdBtn.setToggleState (third,  juce::dontSendNotification);
}

void DefaultPage::updateSplitNoteLabel()
{
    splitNoteLabel.setText (noteName ((int) splitKnob.getValue()), juce::dontSendNotification);
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
    (isUpper ? upperState : lowerState) = captureStateFromControls();
    isUpper = toUpper;
    updateControlsFromState (isUpper ? upperState : lowerState);
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
        juce::TextButton* bots[4] = { &percOffBtn, &percSlowBtn, &percHardBtn, &perc3rdBtn };
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

    // ---- Row B: middle, fills the remaining space (SPLIT knob | LESLIE) ----
    {
        auto splitBlock = timbral.withSizeKeepingCentre (timbral.getWidth(), capH + knob + capH);
        splitLabel.setBounds     (splitBlock.removeFromTop (capH));
        splitKnob.setBounds      (splitBlock.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
        splitNoteLabel.setBounds (splitBlock.removeFromTop (capH));

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
    : AudioProcessorEditor (&p), defaultPage (p)
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

    addAndMakeVisible (defaultPage);

    setSize (740, 430);
    startTimerHz (2);
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
    defaultPage.setBounds (r);
}

void TuneBfreeAudioProcessorEditor::timerCallback()
{
    if (defaultPage.isTuningPanelShowing())
        defaultPage.refreshTuningPanel();
}
