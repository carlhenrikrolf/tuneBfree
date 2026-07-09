#include "PluginEditor.h"
#include <cmath>
#include <optional>

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
    bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    bool isOn   = button.getToggleState();

    // Use the button's own colours so individually-themed buttons (e.g. the
    // red TUNING button) work without special-casing them here.
    auto fill = isOn ? button.findColour (juce::TextButton::buttonOnColourId)
                     : button.findColour (juce::TextButton::buttonColourId);

    // Pressed state: tint towards the "active" colour of the button's context —
    // red for header buttons, amber for the panel buttons. (Momentary buttons
    // like "!" flash this while held.)
    const bool header = button.getProperties().contains ("header");
    if (isDown)
        fill = fill.interpolatedWith (header ? kRed : kAmber, 0.45f);
    else if (isHighlighted)
        fill = fill.brighter (0.12f);

    // Disabled: fade the fill into the page background — except on the amber
    // header, where a translucent black turns brown; there the dimmed text
    // (LookAndFeel_V4::drawButtonText) does the greying alone.
    if (! button.isEnabled() && ! header)
        fill = fill.withAlpha (0.4f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (isOn ? fill.brighter (0.15f) : kBorder);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
}

void TuneBfreeLookAndFeel::drawButtonText (
    juce::Graphics& g, juce::TextButton& button, bool isHighlighted, bool isDown)
{
    // The 🔊 header button: a stylised speaker (macOS-menu-bar-like), drawn as a
    // path — JUCE ships no stock icons, and the emoji renders inconsistently.
    if (button.getProperties().contains ("speakerIcon"))
    {
        auto r = button.getLocalBounds().toFloat().withSizeKeepingCentre (16.0f, 12.0f);
        const float cy = r.getCentreY();

        juce::Path body;   // magnet box + cone
        body.addRectangle (r.getX(), cy - 2.5f, 3.5f, 5.0f);
        body.addTriangle (r.getX() + 2.5f, cy,
                          r.getX() + 8.0f, cy - 6.0f,
                          r.getX() + 8.0f, cy + 6.0f);
        juce::Path waves;  // two sound arcs, opening right
        waves.addCentredArc (r.getX() + 9.5f, cy, 2.6f, 2.6f, 0.0f, 0.6f, 2.5f, true);
        waves.addCentredArc (r.getX() + 9.5f, cy, 5.2f, 5.2f, 0.0f, 0.6f, 2.5f, true);

        g.setColour (button.findColour (button.getToggleState()
                         ? juce::TextButton::textColourOnId
                         : juce::TextButton::textColourOffId));
        g.fillPath (body);
        g.strokePath (waves, juce::PathStrokeType (1.4f));
        return;
    }

    LookAndFeel_V4::drawButtonText (g, button, isHighlighted, isDown);
}

juce::Font TuneBfreeLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return uiFont (juce::jmin (12.0f, (float) buttonHeight * 0.5f));
}

juce::Font TuneBfreeLookAndFeel::getComboBoxFont  (juce::ComboBox&) { return uiFont (12.0f); }
juce::Font TuneBfreeLookAndFeel::getPopupMenuFont ()                { return uiFont (12.0f); }

juce::Font TuneBfreeLookAndFeel::getLabelFont (juce::Label& l)
{
    // Slider value boxes (TINKER/ROTARY knobs) get the UI font; every other
    // label keeps whatever font was set on it explicitly.
    if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
        return uiFont (10.0f);
    return l.getFont();
}

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

// Style a group title ("SCANNER", "HORN MOTOR", "VIBRATO"): amber, bold, left-aligned.
static void styleGroupTitle (juce::Label& l, const juce::String& text)
{
    l.setFont (uiFont (11.0f, true));
    l.setJustificationType (juce::Justification::centredLeft);
    l.setColour (juce::Label::textColourId, kAmber);
    l.setText (text, juce::dontSendNotification);
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
//  RIGHT-CLICK PARAMETER MENU (Surge-style: name / edit value / info).
//  Attached to every parameter control; ctrl-click / two-finger click on Mac.
//  MIDI learn / channel mapping will join this menu with the CONTROL panel.
// ============================================================================

// One-paragraph description per parameter (the "?" info box). Grouped by
// prefix so families share their text. Sentence case on purpose: paragraphs
// in all-caps are unreadable (the all-caps rule is for widget labels).
static juce::String paramInfoText (const juce::String& id)
{
    auto is   = [&id] (const char* v) { return id == v; };
    auto has  = [&id] (const char* v) { return id.startsWith (v); };
    const juce::String rebuild =
        "\n\nChanging this rebuilds the tone generator and cuts sounding notes.";

    // --- PLAY ---
    if (has ("drawbar") || has ("lower_drawbar"))
        return "Harmonic volume, 0 (silent) to 8 (full) - pull down for louder, like the "
               "real drawbars. UPPER/LOWER selects which manual's bank you are editing.";
    if (is ("vibrato") || is ("lower_vibrato"))
        return "Routes this manual through the vibrato/chorus scanner.";
    if (is ("vibrato_type"))
        return "The B3 dial: V1/V2/V3 = vibrato (pitch modulation only), C1/C2/C3 = chorus "
               "(modulated signal mixed with dry). Depth grows from 1 to 3.";
    if (is ("drum") || is ("horn"))
        return "Rotor speed: STOP, CHORALE (slow) or TREMOLO (fast). The PLAY switch drives "
               "both rotors together; the ROTOR page has an independent switch per rotor.";
    if (is ("overdrive"))
        return "Engages the tube-preamp stage (DRIVE above 0 switches it in).";
    if (is ("character"))
        return "DRIVE: overdrive amount/character macro. 0 = clean bypass; above 0 the "
               "preamp stage is engaged. Fine controls live on TINKER > PREAMP.";
    if (is ("reverb_mix"))
        return "Dry/wet of the reverb (Airwindows MatrixVerb). Voicing lives on "
               "ROTOR > REVERB (damping, size, flavor).";
    if (is ("preamp_bias"))
        return "Transfer-curve operating point (asymmetry) of the overdrive - shifts "
               "the balance of even harmonics. Applies live.";
    if (is ("preamp_gfb"))
        return "Global feedback around the overdrive stage - more gives a more "
               "compressed, sustaining drive. Applies live.";
    if (is ("horn_radius") || is ("drum_radius"))
        return "Rotor radius in cm - larger = deeper Doppler swing for this rotor. "
               "Applies live.";
    if (is ("horn_xoff"))
        return "Horn position along the mic axis (toward one mic, away from the "
               "other) - left/right in the x42-whirl picture. Applies live.";
    if (is ("horn_zoff"))
        return "Horn position perpendicular to the mic axis - front/back depth. "
               "Applies live.";
    if (is ("reverb_damping"))
        return "Tail regeneration: more damping = a shorter, tighter tail. Applies live.";
    if (is ("reverb_size"))
        return "Scales every delay line: small = tight and boxy, large = hall. Applies live.";
    if (is ("reverb_flavor"))
        return "Morphs the feedback-matrix character: plate-like at one end, "
               "spring-like at the other. Applies live.";
    if (is ("percussion"))
        return "Percussion on/off (upper manual only, as on a B3). Single-trigger: it "
               "re-arms when upper keys are released; under a keyboard split, suppression "
               "by held notes fades gradually across the crossfade zone.";
    if (is ("percussion_dec"))
        return "Percussion envelope decay: FAST or SLOW (times on TINKER > PERCUSSION).";
    if (is ("percussion_vol"))
        return "Percussion level: SOFT or NORMAL (levels on TINKER > PERCUSSION).";
    if (is ("percussion_har"))
        return "Which harmonic the percussion strikes: 2ND (4') or 3RD (2 2/3').";
    if (is ("expression"))
        return "The swell pedal - on a Hammond it is a pure volume control. Also driven by "
               "MIDI CC 7 and CC 11; smoothed (~25 Hz) so pedal moves don't zipper.";
    if (is ("split_enable"))
        return "BITIMBRAL: splits the keyboard into lower/upper manuals by SOUNDING PITCH "
               "(not key number), each with its own drawbar bank.";
    if (is ("split_point"))
        return "The split frequency: pitches below sound the lower manual, above the upper. "
               "A frequency, not a key - so it works under any tuning. KEYPRESS sets it "
               "from held keys (silently).";
    if (is ("split_width"))
        return "Crossfade width in cents around the split point. Inside the zone a note "
               "sounds on BOTH manuals with equal-power weights.";
    if (is ("whirl_bypass"))
        return "Bypasses the whole rotary-speaker simulation (the organ plays dry).";
    if (is ("master_volume"))
        return "Master output volume, after the whole signal chain. Ramped per block, "
               "click-free.";

    // --- TINKER ---
    if (is ("scanner_hz"))
        return "The vibrato scanner's rate. On the instrument it is fixed by the motor "
               "(~7 Hz); here you may detune it." + rebuild;
    if (has ("scanner_v"))
        return "Modulation depth for the corresponding dial positions (V1/C1, V2/C2, "
               "V3/C3)." + rebuild;
    if (is ("perc_fast_s") || is ("perc_slow_s"))
        return "Percussion decay time in seconds for the FAST / SLOW switch position. "
               "Applies live.";
    if (is ("perc_gain"))
        return "Overall percussion gain scaling. Applies live.";
    if (is ("perc_norm_gain") || is ("perc_soft_gain"))
        return "Envelope starting level for the NORMAL / SOFT switch position. Applies live.";
    if (is ("click_attack_model") || is ("click_release_model"))
        return "Key-contact model at attack/release: CLICK = random contact bounces (the "
               "classic key click), SHELF = debounced step, COSINE/LINEAR = plain fades."
               + rebuild;
    if (is ("click_attack_level") || is ("click_release_level"))
        return "Amount of random contact noise - more simulates worn, oxidised contacts."
               + rebuild;
    if (is ("click_min_length") || is ("click_max_length"))
        return "Bounds of the random key-click burst length (fraction of ~2.9 ms)." + rebuild;
    if (is ("xtalk_compartment"))
        return "Crosstalk between tonewheels sharing a compartment in the generator."
               + rebuild;
    if (is ("xtalk_transformer"))
        return "Pickup between neighbouring filter transformers on top of the generator "
               "(default 0)." + rebuild;
    if (is ("xtalk_terminal"))
        return "Leakage between neighbouring soldering points on the output terminal strip."
               + rebuild;
    if (is ("xtalk_wiring"))
        return "Crosstalk between the unshielded manual wires (they share a loom)." + rebuild;
    if (has ("eq_bass") || has ("eq_treble"))
        return "Tonegenerator output-level spline: BASS/TREBLE set the level at the lowest/"
               "highest wheels, SLOPE bends the curve near that end. A broad tonal tilt "
               "across all 91+ wheels." + rebuild;
    if (is ("wave"))
        return "Tonewheel waveform: pure sine (the ideal wheel) or square/triangle harmonic "
               "series (a deliberately flawed generator)." + rebuild;
    if (is ("preamp_in") || is ("preamp_out"))
        return "Signal level into / out of the overdrive stage. OUT as high as possible "
               "without clipping. Applies live.";
    if (is ("preamp_bass_pre"))
        return "Bass tone control BEFORE the overdrive (unity ~0.58): more sends more bass "
               "into the distortion. Applies live.";
    if (is ("preamp_bass_post"))
        return "Bass recovery AFTER the overdrive - together with BASS PRE it keeps bass "
               "out of the drive and restores it afterwards. Applies live.";
    if (is ("preamp_sag"))
        return "Power-supply sag recovery rate (must stay below 1): emulates the voltage "
               "drop of a loaded amp. Applies live.";
    if (has ("harm_cents"))
        return "CUSTOM interval of this drawbar above the key fundamental. Type a ratio "
               "(3/2), cents (702.23 c) or a bare integer (5 = 5/1) in the field. Active "
               "only when the drawbar is set to C." + rebuild;
    if (has ("harm_auto"))
        return "A = AUTO: the pure just-intonation harmonic, quantized to the current "
               "tuning (stock behaviour). C = CUSTOM: your entry sounds exactly - its "
               "wheel frequencies are added un-quantized. Both remember their state."
               + rebuild;

    // --- ROTOR ---
    if (has ("horn_slow") || has ("horn_fast") || has ("drum_slow") || has ("drum_fast"))
        return "Target rotation speed (RPM) for this rotor's slow (chorale) / fast "
               "(tremolo) setting. Applies live.";
    if (has ("horn_accel") || has ("horn_decel") || has ("drum_accel") || has ("drum_decel"))
        return "Spool-up/down time constant in seconds (time to ~63% of the change) - the "
               "horn is light and quick, the drum heavy and slow.";
    if (has ("horn_brake") || has ("drum_brake"))
        return "Where the rotor parks when stopped: 0 = coast freely, above 0 = brake to "
               "that position on the circle (1.0 = front-centre). Audible only via where "
               "the stopped rotor points.";
    if (has ("horn_filter_a"))
        return "First horn voicing filter (with filter B it forms the band-pass character "
               "of the horn driver).";
    if (has ("horn_filter_b"))
        return "Second horn voicing filter - the low-shelf side of the horn's band-pass "
               "character.";
    if (has ("drum_filter"))
        return "The drum's voicing filter (stock: a high shelf cutting the top end - the "
               "drum only carries the lows).";
    if (is ("horn_level"))
        return "The horn's level relative to the drum - a balance, not a dry/wet (that's "
               "why no extreme silences the Leslie; use BYPASS for that).";
    if (is ("horn_leak"))
        return "Unrotated horn signal leaking past the rotor: band-passed, no Doppler. "
               "Adds presence at slow speeds.";
    if (has ("horn_width") || has ("drum_width"))
        return "Stereo width of this rotor's virtual mic pair: 1 = full stereo, 0 = "
               "collapsed to mono.";
    if (is ("mic_angle"))
        return "Angle between the two virtual microphones (180 = opposite sides - widest "
               "modulation).";
    if (is ("mic_dist"))
        return "Microphone distance from the cabinet in cm. Floor is 25 cm: closer than "
               "the rotor radius would put the mic INSIDE the rotor circle.";

    return "No description yet.";
}

// Small CallOutBox content: type a new value for the parameter.
class ParamEditContent : public juce::Component
{
public:
    explicit ParamEditContent (juce::RangedAudioParameter& param) : p (param)
    {
        ed.setFont (uiFont (13.0f));
        ed.setColour (juce::TextEditor::backgroundColourId, kBtn);
        ed.setColour (juce::TextEditor::textColourId,       kWhite);
        ed.setColour (juce::TextEditor::outlineColourId,    kBorder);
        ed.setColour (juce::TextEditor::focusedOutlineColourId, kAmber);
        ed.setText (p.getCurrentValueAsText(), juce::dontSendNotification);
        ed.setSelectAllWhenFocused (true);
        ed.onReturnKey = [this]
        {
            p.setValueNotifyingHost (p.convertTo0to1 (ed.getText().getFloatValue()));
            dismiss();
        };
        ed.onEscapeKey = [this] { dismiss(); };
        addAndMakeVisible (ed);
        setSize (150, 30);
    }
    void resized() override { ed.setBounds (getLocalBounds().reduced (3)); }
    void parentHierarchyChanged() override { ed.grabKeyboardFocus(); }

private:
    void dismiss()
    {
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    }
    juce::RangedAudioParameter& p;
    juce::TextEditor ed;
};

// Small CallOutBox content: the parameter's info text, wrapped.
class ParamInfoContent : public juce::Component
{
public:
    ParamInfoContent (const juce::String& title, const juce::String& body)
    {
        text.append (title.toUpperCase() + "\n\n", uiFont (12.0f, true), kAmber);
        text.append (body, uiFont (12.0f), kWhite);
        juce::TextLayout probe;
        probe.createLayout (text, 260.0f);
        setSize (280, (int) std::ceil (probe.getHeight()) + 20);
    }
    void paint (juce::Graphics& g) override
    {
        juce::TextLayout tl;
        tl.createLayout (text, (float) getWidth() - 20.0f);
        tl.draw (g, getLocalBounds().toFloat().reduced (10.0f));
    }

private:
    juce::AttributedString text;
};

// General MIDI names for the well-known controllers; "" = no standard name.
static juce::String generalMidiCCName (int cc)
{
    switch (cc)
    {
        case 1:  return "Mod Wheel";
        case 2:  return "Breath";
        case 4:  return "Foot";
        case 5:  return "Portamento Time";
        case 7:  return "Volume";
        case 8:  return "Balance";
        case 10: return "Pan";
        case 11: return "Expression";
        case 64: return "Sustain";
        case 65: return "Portamento";
        case 66: return "Sostenuto";
        case 67: return "Soft";
        case 68: return "Legato";
        case 69: return "Hold 2";
        case 71: return "Resonance";
        case 74: return "Cutoff";
        case 84: return "Portamento Control";
        case 91: return "Reverb Depth";
        case 93: return "Chorus Depth";
        default: return {};
    }
}

// Menu item id scheme (see showParamMenu):
enum { kMenuEdit = 1, kMenuInfo = 2, kMenuLearn = 10, kMenuClear = 11,
       kMenuChannelBase = 200,      // +0 = omni, +1..16 = channel
       kMenuCCBase = 1000 };        // +cc

// The menu itself. Safe against the editor closing mid-menu (SafePointer).
// paramIDs[0] is the PRIMARY — used for the value/edit/info and the mapping
// display. Mapping actions (learn/assign/channel/clear) apply to EVERY id, so
// one control can drive several parameters together (the PLAY Leslie → drum +
// horn). For an ordinary control, paramIDs holds a single entry.
static void showParamMenu (juce::AudioProcessorValueTreeState& state,
                           const juce::StringArray& paramIDs, juce::Component* target)
{
    if (paramIDs.isEmpty()) return;
    const juce::String primary = paramIDs[0];
    auto* p = state.getParameter (primary);
    if (p == nullptr) return;

    auto& proc = static_cast<TuneBfreeAudioProcessor&> (state.processor);
    const bool mappable = proc.isParamMappable (primary);
    TuneBfreeAudioProcessor::MidiSource current;
    const bool mapped   = proc.getMidiMappingFor (primary, current);
    const bool learning = proc.isMidiLearning (primary);

    juce::PopupMenu m;
    m.setLookAndFeel (&target->getLookAndFeel());
    m.addSectionHeader (p->getName (64).toUpperCase());
    m.addItem (kMenuEdit, "EDIT VALUE: " + p->getCurrentValueAsText());
    m.addItem (kMenuInfo, "INFO");

    // --- MIDI mapping ---
    m.addSeparator();
    if (! mappable)
    {
        m.addItem (-1, "MIDI MAPPING N/A (RESTARTS ENGINE)", false, false);
    }
    else
    {
        m.addSectionHeader (mapped ? "MAPPED: " + TuneBfreeAudioProcessor::midiSourceLabel (current)
                                   : "NOT MAPPED");
        m.addItem (kMenuLearn, learning ? "ABORT MIDI LEARN" : "MIDI LEARN", true, learning);

        // ASSIGN CC → bins of 20, each CC shown with its GM name; reserved disabled.
        juce::PopupMenu assign;
        for (int bin = 0; bin < 128; bin += 20)
        {
            juce::PopupMenu binMenu;
            const int hi = juce::jmin (bin + 19, 127);
            for (int cc = bin; cc <= hi; ++cc)
            {
                const auto gm = generalMidiCCName (cc);
                juce::String label = "CC " + juce::String (cc) + (gm.isEmpty() ? "" : "  " + gm);
                const bool tick = mapped && current.type == TuneBfreeAudioProcessor::MidiSource::CC
                                  && current.cc == cc;
                binMenu.addItem (kMenuCCBase + cc, label,
                                 TuneBfreeAudioProcessor::isAssignableCC (cc), tick);
            }
            assign.addSubMenu (juce::String (bin) + "-" + juce::String (hi), binMenu);
        }
        m.addSubMenu ("ASSIGN CC", assign);

        // CHANNEL: which channel the mapping listens on (omni or 1..16).
        juce::PopupMenu chan;
        chan.addItem (kMenuChannelBase + 0, "OMNI", mapped,
                      mapped && current.channel == 0);
        for (int ch = 1; ch <= 16; ++ch)
            chan.addItem (kMenuChannelBase + ch, "Channel " + juce::String (ch), mapped,
                          mapped && current.channel == ch);
        m.addSubMenu ("CHANNEL", chan, mapped);

        m.addItem (kMenuClear, "CLEAR MAPPING", mapped);
    }

    juce::Component::SafePointer<juce::Component> safe (target);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
        [&proc, p, safe, paramIDs, primary] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            using MidiSource = TuneBfreeAudioProcessor::MidiSource;
            auto assignAll = [&] (MidiSource s) {
                for (const auto& id : paramIDs) proc.assignMidiMapping (id, s);
            };
            if (r == kMenuEdit)
                juce::CallOutBox::launchAsynchronously (
                    std::make_unique<ParamEditContent> (*p), safe->getScreenBounds(), nullptr);
            else if (r == kMenuInfo)
                juce::CallOutBox::launchAsynchronously (
                    std::make_unique<ParamInfoContent> (p->getName (64), paramInfoText (primary)),
                    safe->getScreenBounds(), nullptr);
            else if (r == kMenuLearn)
            {
                if (proc.isMidiLearning (primary)) proc.cancelMidiLearn();
                else                               proc.startMidiLearn (paramIDs);
            }
            else if (r == kMenuClear)
                for (const auto& id : paramIDs) proc.clearMidiMapping (id);
            else if (r >= kMenuChannelBase && r < kMenuChannelBase + 17)
            {
                MidiSource s;
                if (proc.getMidiMappingFor (primary, s))   // keep type/cc, change channel
                {
                    s.channel = r - kMenuChannelBase;
                    assignAll (s);
                }
            }
            else if (r >= kMenuCCBase && r < kMenuCCBase + 128)
            {
                MidiSource s;                      // keep the existing channel if mapped
                proc.getMidiMappingFor (primary, s);
                s.type = MidiSource::CC;
                s.cc   = r - kMenuCCBase;
                assignAll (s);
            }
        });
}

// Convenience: single-parameter menu (the common case).
static void showParamMenu (juce::AudioProcessorValueTreeState& state,
                           const juce::String& paramID, juce::Component* target)
{
    showParamMenu (state, juce::StringArray (paramID), target);
}

// Attach to any control: right-click (ctrl-click / two-finger click) opens the
// parameter menu. The id is a function so controls with a DYNAMIC parameter
// (the drawbars follow the UPPER/LOWER selection) stay correct.
class ParamMenuAttachment : public juce::MouseListener
{
public:
    // Single dynamic parameter id (the common case).
    ParamMenuAttachment (juce::AudioProcessorValueTreeState& s, juce::Component& c,
                         std::function<juce::String()> idFn)
        : state (s), comp (c),
          getIds ([fn = std::move (idFn)] { return juce::StringArray (fn()); })
    {
        comp.addMouseListener (this, true);
    }
    // Several parameters driven together (the PLAY Leslie → drum + horn); the
    // first is the primary shown in the menu.
    ParamMenuAttachment (juce::AudioProcessorValueTreeState& s, juce::Component& c,
                         juce::StringArray ids)
        : state (s), comp (c), getIds ([ids = std::move (ids)] { return ids; })
    {
        comp.addMouseListener (this, true);
    }
    ~ParamMenuAttachment() override { comp.removeMouseListener (this); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            showParamMenu (state, getIds(), &comp);
    }

private:
    juce::AudioProcessorValueTreeState& state;
    juce::Component& comp;
    std::function<juce::StringArray()> getIds;
};

// Right-click info for widgets that are NOT plugin parameters (the tuning
// panel): opens the info box directly — there is no value to edit.
class InfoMenuAttachment : public juce::MouseListener
{
public:
    InfoMenuAttachment (juce::Component& c, juce::String t, juce::String b)
        : comp (c), title (std::move (t)), body (std::move (b))
    {
        comp.addMouseListener (this, true);
    }
    ~InfoMenuAttachment() override { comp.removeMouseListener (this); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            juce::CallOutBox::launchAsynchronously (
                std::make_unique<ParamInfoContent> (title, body),
                comp.getScreenBounds(), nullptr);
    }

private:
    juce::Component& comp;
    juce::String title, body;
};

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

        selectAllBtn.onClick   = [this] { for (int c = 0; c < 16; ++c) proc.setChannelActive (c, proc.isChannelMapped (c)); syncChannels(); };
        deselectAllBtn.onClick = [this] { for (int c = 0; c < 16; ++c) proc.setChannelActive (c, false); syncChannels(); };
        addAndMakeVisible (selectAllBtn);
        addAndMakeVisible (deselectAllBtn);

        for (int c = 0; c < 16; ++c)
        {
            auto* b = chanBtns.add (new juce::TextButton (juce::String (c + 1)));
            b->setClickingTogglesState (true);
            b->onClick = [this, c] { proc.setChannelActive (c, chanBtns[c]->getToggleState()); };
            // Unmapped channels (FILE tuning, no _i.kbm and no generic kbm) are
            // silent — grey them out so they can't be selected.
            b->setEnabled (proc.isChannelMapped (c));
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
//  FILES popup (from the tuning panel's FILES button): lists the loaded tuning
//  files — the scale plus the current .kbm batch — with CLEAR ALL to unload.
//  Groundwork for tuning program change (several tunings loaded at once).
// ============================================================================

class TuningFilesContent : public juce::Component
{
public:
    explicit TuningFilesContent (TuneBfreeAudioProcessor& p) : proc (p)
    {
        auto addLine = [this] (const juce::String& text, juce::Colour colour)
        {
            auto* l = lines.add (new juce::Label());
            l->setFont (uiFont (11.0f));
            l->setColour (juce::Label::textColourId, colour);
            l->setText (text, juce::dontSendNotification);
            addAndMakeVisible (l);
        };
        const auto scl  = proc.getLocalSclName();
        const auto kbms = proc.getLocalKbmNames();
        if (scl.isNotEmpty())
            addLine ("SCALE  " + scl.toUpperCase(), kWhite);
        for (const auto& k : kbms)
            addLine ("MAP    " + k.toUpperCase(), kWhite);
        if (lines.isEmpty())
            addLine ("NO TUNING FILES LOADED", kGrey);

        clearBtn.setEnabled (scl.isNotEmpty() || ! kbms.isEmpty());
        clearBtn.onClick = [this]
        {
            proc.clearLocalTuning();
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        };
        addAndMakeVisible (clearBtn);

        setSize (250, 8 + lines.size() * 18 + 8 + 24 + 8);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        clearBtn.setBounds (r.removeFromBottom (24));
        r.removeFromBottom (8);
        for (auto* l : lines)
            l->setBounds (r.removeFromTop (18));
    }

private:
    TuneBfreeAudioProcessor& proc;
    juce::OwnedArray<juce::Label> lines;
    juce::TextButton clearBtn { "CLEAR ALL" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuningFilesContent)
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
    encodingBox.addItem ("SCALA",    TS_FILE);   // .scl/.kbm files (renamed from FILE)
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

    addAndMakeVisible (loadBtn);
    addAndMakeVisible (filesBtn);

    // ONE loader for scale + mappings: multi-select .scl AND .kbm in the same
    // dialog (anything else greyed out). The first .scl becomes the scale; all
    // .kbm files become the per-channel batch ("*_i.kbm" → MIDI channel i).
    // Also a step towards tuning program change (several tunings loaded at once).
    loadBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load tuning files (.scl + .kbm)", lastTuningDir, "*.scl;*.kbm");
        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::canSelectMultipleItems,
            [this] (const juce::FileChooser& fc)
            {
                auto results = fc.getResults();
                if (results.isEmpty()) return;
                lastTuningDir = results[0].getParentDirectory();   // remember for next time

                juce::Array<juce::File> kbms;
                juce::File scl;
                for (const auto& f : results)
                {
                    if (f.hasFileExtension ("scl") && scl == juce::File())
                        scl = f;                       // first .scl wins; extras ignored
                    else if (f.hasFileExtension ("kbm"))
                        kbms.add (f);
                }
                if (scl != juce::File()) proc.loadSCLFile (scl);
                if (! kbms.isEmpty())    proc.loadKBMFiles (kbms);
                refresh();
            });
    };

    // FILES: popup listing the loaded tuning files (like the CHANNELS popup).
    filesBtn.onClick = [this]
    {
        auto content = std::make_unique<TuningFilesContent> (proc);
        content->setLookAndFeel (&getLookAndFeel());
        juce::CallOutBox::launchAsynchronously (std::move (content),
                                                filesBtn.getScreenBounds(), nullptr);
    };

    // NOTE ON vs ALWAYS retuning: a 2-way toggle (UI-only for now). "Always" lets a
    // sounding note change pitch; note-on is the default — it suits tuneBfree's
    // wavetable rebuild step.
    makeRadioGroup ({ &noteOnBtn, &alwaysBtn },
                    [this] { retuneAlwaysPref = alwaysBtn.getToggleState(); });
    noteOnBtn.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (noteOnBtn);
    addAndMakeVisible (alwaysBtn);

    // Right-click info on every panel widget (none of these are parameters).
    {
        auto info = [this] (juce::Component& c, juce::String t, juce::String b)
        { paramMenus.add (new InfoMenuAttachment (c, std::move (t), std::move (b))); };

        info (encodingBox, "TUNING SOURCE",
              "Which encoding feeds the engine. MTS ESP: a live tuning master via the "
              "shared library. SYSEX: MIDI Tuning Standard messages (all bulk-dump and "
              "realtime formats are parsed). SCALA: .scl/.kbm files. STANDARD: plain "
              "12edo - the microtuning panic button. MPE and MIDI 2.0: not implemented. "
              "Each source keeps its own state, so you can toggle back and forth.");
        info (channelsBtn, "CHANNELS",
              "Selects which MIDI channels sound and contribute pitches to the merged "
              "tuning gamut (multichannel = more notes of ONE scale). OMNI collapses "
              "every selected channel onto the generic mapping.");
        info (loadBtn, "LOAD SCL+KBM",
              "Pick a scale (.scl) and any number of keyboard mappings (.kbm) in one "
              "go. Files named *_i.kbm map MIDI channel i; an unsuffixed .kbm is the "
              "generic mapping for unassigned channels. Active only under SCALA.");
        info (filesBtn, "FILES",
              "Lists the loaded tuning files (scale + mapping batch); CLEAR ALL "
              "unloads them. Groundwork for tuning program change.");
        info (noteOnBtn, "NOTE ON / ALWAYS",
              "When retuning takes effect. NOTE ON: pitches update at the next key "
              "press (suits the wavetable rebuild). ALWAYS: sounding notes may move. "
              "Editable under MTS ESP; under SYSEX the greyed switch is an INDICATOR "
              "following the last message (realtime = ALWAYS, bulk dump = NOTE ON).");
        info (alwaysBtn, "NOTE ON / ALWAYS",
              "When retuning takes effect. NOTE ON: pitches update at the next key "
              "press (suits the wavetable rebuild). ALWAYS: sounding notes may move. "
              "Editable under MTS ESP; under SYSEX the greyed switch is an INDICATOR "
              "following the last message (realtime = ALWAYS, bulk dump = NOTE ON).");
        info (penultimateHzLabel, "PENULTIMATE NOTE",
              "The sounding frequency of the note played before the last one.");
        info (lastHzLabel, "LAST NOTE",
              "The sounding frequency of the last note played. Also the reference "
              "for the drawbar error read-outs.");
        info (centsLabel, "INTERVAL",
              "The interval between the last two notes, octave-folded: a cents value "
              "below an octave plus the octave count, e.g. -(702.23 + 2x1200) c.");
        info (scaleNameLabel, "TUNING NAME",
              "From the MTS ESP master, the sysex tuning-dump name, or the .scl "
              "description line. GEAR60 (~12EDO) is the startup default.");
        info (periodLabel, "SCALE PERIOD",
              "The interval at which the tuning repeats. Used to extend the wheel "
              "table upward and to quantize the drawbar harmonics. INFERRED = "
              "detected from the table; SPECIFIED = declared by the .scl file; "
              "NONE = aperiodic (the span in parentheses).");
        info (timestampLabel, "LAST UPDATE",
              "When the tuning last changed. Ticks like a clock while an MTS ESP "
              "master is live - that is how you see the connection is alive.");
    }

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
    loadBtn.setBounds (leftCol.removeFromTop (btnH));
    leftCol.removeFromTop (gap);
    filesBtn.setBounds (leftCol.removeFromTop (btnH));
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
        // Octave-folded read-out: a cents value below an octave plus the octave
        // count, e.g. "-(702.23 + 2x1200) c", "(315.00 + 1200) c", "-498.78 c".
        const double cents = 1200.0 * std::log2 (fl / fp);
        const bool   neg   = cents < 0.0;
        const double mag   = std::abs (cents);
        const int    octs  = (int) (mag / 1200.0);
        const auto   rem   = juce::String (mag - 1200.0 * octs, 2);
        juce::String text;
        if (octs == 0)
            text = (neg ? "-" : "") + rem + " c";
        else
            text = juce::String (neg ? "-(" : "(") + rem + " + "
                   + (octs == 1 ? juce::String ("1200") : juce::String (octs) + "x1200")
                   + ") c";
        centsLabel.setText (text, juce::dontSendNotification);
    }
    else
    {
        centsLabel.setText ("? c", juce::dontSendNotification);
    }

    // --- loader: show the loaded scale name; FILES: show how many are loaded ---
    const auto scl = proc.getLocalSclName();
    const int  n   = (scl.isNotEmpty() ? 1 : 0) + proc.getLocalKbmNames().size();
    loadBtn.setButtonText (scl.isNotEmpty() ? scl.toUpperCase() : "LOAD SCL+KBM");
    filesBtn.setButtonText (n > 0 ? "FILES (" + juce::String (n) + ")" : "FILES");

    // --- per-source grey-out (TUNING_PANEL.md) ---
    // Files load only under SCALA. NOTE ON / ALWAYS is the user's choice under
    // MTS ESP (would also apply to MPE); under SYSEX it's greyed but ACTS AS AN
    // INDICATOR, following the last message (realtime retune vs bulk dump).
    loadBtn.setEnabled  (src == TS_FILE);
    filesBtn.setEnabled (src == TS_FILE);

    const bool retuneEditable = (src == TS_MTS);
    noteOnBtn.setEnabled (retuneEditable);
    alwaysBtn.setEnabled (retuneEditable);
    if (src == TS_SYSEX && proc.getLastSysexKind() >= 0)
    {
        const bool realtime = proc.getLastSysexKind() == 1;
        noteOnBtn.setToggleState (! realtime, juce::dontSendNotification);
        alwaysBtn.setToggleState (realtime,   juce::dontSendNotification);
    }
    else
    {
        noteOnBtn.setToggleState (! retuneAlwaysPref, juce::dontSendNotification);
        alwaysBtn.setToggleState (retuneAlwaysPref,   juce::dontSendNotification);
    }
}

// (The old "switch to FILE?" dialog is gone: the loader is simply greyed out
//  unless the SCALA source is selected — per TUNING_PANEL.md.)

// ============================================================================
//  CONTROL SIDE PANEL (presets / program change; MIDI mappings to follow)
// ============================================================================

// CallOutBox content for SAVE: name + author fields, an INCLUDE TUNING toggle,
// and a SAVE button that then opens the file chooser. Metadata mirrors Surge/
// Vital (name + author); the tuning toggle governs the preset's <TUNING> block.
class PresetSaveContent : public juce::Component
{
public:
    // onSave is invoked (if set) when the user clicks SAVE, just before the
    // callout dismisses — read getName()/getAuthor()/includeTuning() there.
    std::function<void()> onSave;

    explicit PresetSaveContent (TuneBfreeAudioProcessor& p) : proc (p)
    {
        auto field = [this] (juce::Label& cap, const juce::String& text,
                             juce::TextEditor& ed, const juce::String& initial)
        {
            styleCaption (cap, text);
            cap.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (cap);
            ed.setFont (uiFont (13.0f));
            ed.setColour (juce::TextEditor::backgroundColourId,     kBtn);
            ed.setColour (juce::TextEditor::textColourId,           kWhite);
            ed.setColour (juce::TextEditor::outlineColourId,        kBorder);
            ed.setColour (juce::TextEditor::focusedOutlineColourId, kAmber);
            ed.setText (initial, juce::dontSendNotification);
            addAndMakeVisible (ed);
        };
        field (nameCap,   "NAME",   nameEd,   "Untitled");
        field (authorCap, "AUTHOR", authorEd, lastAuthor);
        nameEd.setSelectAllWhenFocused (true);

        tuningBtn.setClickingTogglesState (true);
        tuningBtn.getProperties().set ("header", false);
        addAndMakeVisible (tuningBtn);

        saveBtn.onClick = [this]
        {
            if (nameEd.getText().trim().isEmpty())
                nameEd.setText ("Untitled", juce::dontSendNotification);
            lastAuthor = authorEd.getText();
            if (onSave) onSave();
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        };
        addAndMakeVisible (saveBtn);

        setSize (240, 150);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        const int rowH = 22, gap = 6;
        nameCap.setBounds  (r.removeFromTop (16));
        nameEd.setBounds   (r.removeFromTop (rowH));
        r.removeFromTop (gap);
        authorCap.setBounds (r.removeFromTop (16));
        authorEd.setBounds  (r.removeFromTop (rowH));
        r.removeFromTop (gap);
        tuningBtn.setBounds (r.removeFromTop (rowH));
        r.removeFromTop (gap);
        saveBtn.setBounds   (r.removeFromTop (rowH));
    }

    juce::String getName()      const { return nameEd.getText().trim(); }
    juce::String getAuthor()    const { return authorEd.getText().trim(); }
    bool         includeTuning() const { return tuningBtn.getToggleState(); }

private:
    TuneBfreeAudioProcessor& proc;
    juce::Label      nameCap, authorCap;
    juce::TextEditor  nameEd, authorEd;
    juce::TextButton tuningBtn { "INCLUDE TUNING" };
    juce::TextButton saveBtn   { "SAVE\xe2\x80\xa6" };

    // Author persists across saves within a session (a small convenience).
    static juce::String lastAuthor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetSaveContent)
};

juce::String PresetSaveContent::lastAuthor;

void ControlSidePanelContent::ListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                           int w, int h, bool rowIsSelected)
{
    if (rowText == nullptr) return;
    if (rowIsSelected)
    {
        g.setColour (kRed);
        g.fillRect (0, 0, w, h);
    }
    g.setColour (rowIsSelected ? kWhite : kAmber);
    g.setFont (uiFont (11.0f));
    g.drawText (rowText (row), 6, 0, w - 8, h, juce::Justification::centredLeft, true);
}

ControlSidePanelContent::ControlSidePanelContent (TuneBfreeAudioProcessor& p) : proc (p)
{
    styleSectionTitle (pcTitle, "PROGRAM CHANGE");
    styleSectionTitle (ccTitle, "CONTINUOUS CONTROLLERS");
    addAndMakeVisible (pcTitle);
    addAndMakeVisible (ccTitle);

    // --- bank list ---
    bankModel.rowCount   = [this] { return proc.presets.numBanks(); };
    bankModel.rowText    = [this] (int r) { return proc.presets.getBankName (r); };
    bankModel.rowClicked = [this] (int r)
    {
        proc.presets.setCurrentBank (r);
        proc.presets.setCurrentPreset (-1);
        refresh();
    };
    // --- preset list (of the current bank); clicking a row applies it ---
    presetModel.rowCount   = [this] { return proc.presets.numPresets (proc.presets.getCurrentBank()); };
    presetModel.rowText    = [this] (int r) { return proc.presets.getPresetName (proc.presets.getCurrentBank(), r); };
    presetModel.rowClicked = [this] (int r)
    {
        const int bank = proc.presets.getCurrentBank();
        proc.presets.setCurrentPreset (r);
        proc.presets.apply (proc.presets.getPreset (bank, r));
    };
    // --- CC mappings list (cached in ccRows; see refresh) ---
    ccModel.rowCount   = [this] { return ccRows.size(); };
    ccModel.rowText    = [this] (int r) { return ccRows[r]; };
    ccModel.rowClicked = [this] (int) { removeBtn.setEnabled (ccList.getSelectedRow() >= 0); };

    bankList.setModel   (&bankModel);
    presetList.setModel (&presetModel);
    ccList.setModel     (&ccModel);
    for (auto* lb : { &bankList, &presetList, &ccList })
    {
        lb->setRowHeight (18);
        lb->setColour (juce::ListBox::backgroundColourId, kBtn);
        lb->setColour (juce::ListBox::outlineColourId,    kBorder);
        lb->setOutlineThickness (1);
        addAndMakeVisible (*lb);
    }

    styleInfoBox (ccEmptyLabel, juce::Justification::centred);
    ccEmptyLabel.setText ("NO MAPPINGS", juce::dontSendNotification);
    ccEmptyLabel.setColour (juce::Label::textColourId, kGrey);
    addAndMakeVisible (ccEmptyLabel);

    loadBtn.onClick   = [this] { showLoadMenu(); };
    saveBtn.onClick   = [this] { showSaveDialog(); };
    removeBtn.onClick = [this]
    {
        const int row = ccList.getSelectedRow();
        if (row >= 0) { proc.removeMidiMappingAt (row); refresh(); }
    };
    removeBtn.setEnabled (false);
    for (auto* b : { &loadBtn, &saveBtn, &removeBtn })
        addAndMakeVisible (*b);

    // Right-click info on the panel widgets (none are plugin parameters).
    auto info = [this] (juce::Component& c, juce::String t, juce::String b)
    { paramMenus.add (new InfoMenuAttachment (c, std::move (t), std::move (b))); };
    info (bankList, "BANKS",
          "The loaded preset banks. A bank is an ordered list of presets; it lives in "
          "the plugin state, so banks you load or save persist with the session. Pick a "
          "bank to see its presets. MIDI Bank Select (MSB+LSB) chooses the bank live.");
    info (presetList, "PRESETS",
          "Presets in the selected bank. A preset stores every engine and effect "
          "setting (including the drawbar HARMONICS fine-tuning) but NOT the key tuning "
          "unless you saved it with INCLUDE TUNING, and NOT the MIDI mappings. Click to "
          "load; MIDI Program Change selects within the current bank.");
    info (loadBtn, "LOAD",
          "Load preset .xml file(s) into the current bank, a directory of .xml presets "
          "as a new bank, or a setBfree .pgm file as a new bank (backwards compat).");
    info (saveBtn, "SAVE",
          "Save the current sound as an .xml preset. You choose the name, author, and "
          "whether to bake in the current key tuning, then where to write the file. The "
          "saved preset is also added to the current bank.");
    info (ccList, "CONTINUOUS CONTROLLERS",
          "MIDI controllers currently mapped to parameters. Assign or clear mappings by "
          "right-clicking a parameter (MIDI learn, or set channel + CC manually). "
          "Mappings are saved with the plugin state, not inside presets.");
    info (removeBtn, "REMOVE",
          "Remove the selected MIDI mapping.");

    refresh();
}

void ControlSidePanelContent::paint (juce::Graphics& g)
{
    g.fillAll (kPanel);   // matches the tuning panel
}

void ControlSidePanelContent::resized()
{
    const int pad = 12, gap = 8, titleH = 18, btnH = 30;
    auto r = getLocalBounds().reduced (pad);

    // --- PROGRAM CHANGE ---
    pcTitle.setBounds (r.removeFromTop (titleH));
    r.removeFromTop (gap);

    // Two list columns: banks | presets. Presets get the larger share.
    const int listH = 132;
    auto listRow = r.removeFromTop (listH);
    const int bankW = (listRow.getWidth() - gap) * 2 / 5;
    bankList.setBounds   (listRow.removeFromLeft (bankW));
    listRow.removeFromLeft (gap);
    presetList.setBounds (listRow);
    r.removeFromTop (gap);

    // LOAD | SAVE
    auto btnRow = r.removeFromTop (btnH);
    loadBtn.setBounds (btnRow.removeFromLeft ((btnRow.getWidth() - gap) / 2));
    btnRow.removeFromLeft (gap);
    saveBtn.setBounds (btnRow);
    r.removeFromTop (gap * 2);

    // --- CONTINUOUS CONTROLLERS ---
    ccTitle.setBounds (r.removeFromTop (titleH));
    r.removeFromTop (gap);
    auto removeRow = r.removeFromBottom (btnH);
    removeBtn.setBounds (removeRow.removeFromRight ((removeRow.getWidth() - gap) / 2));
    r.removeFromBottom (gap);
    ccList.setBounds (r);
    ccEmptyLabel.setBounds (r);
}

int ControlSidePanelContent::ensureCurrentBank()
{
    if (proc.presets.numBanks() == 0)
    {
        const int bank = proc.presets.addBank ("User");
        proc.presets.setCurrentBank (bank);
        return bank;
    }
    return proc.presets.getCurrentBank();
}

void ControlSidePanelContent::showLoadMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());
    m.addItem (1, "Load preset file(s)\xe2\x80\xa6");
    m.addItem (2, "Load bank directory\xe2\x80\xa6");
    m.addItem (3, "Import .pgm bank\xe2\x80\xa6");

    juce::Component::SafePointer<ControlSidePanelContent> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (loadBtn),
        [safe] (int r)
        {
            if (safe == nullptr || r == 0) return;
            safe->doLoad (r);
        });
}

void ControlSidePanelContent::doLoad (int which)
{
    const bool dir = (which == 2);
    const juce::String title = which == 1 ? "Load preset file(s)"
                             : which == 2 ? "Load bank directory"
                                          : "Import .pgm bank";
    const juce::String filter = which == 3 ? "*.pgm" : "*.xml";
    auto flags = juce::FileBrowserComponent::openMode
               | (dir ? juce::FileBrowserComponent::canSelectDirectories
                      : juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::canSelectMultipleItems);

    fileChooser = std::make_unique<juce::FileChooser> (title, lastPresetDir, filter);
    juce::Component::SafePointer<ControlSidePanelContent> safe (this);
    fileChooser->launchAsync (flags, [safe, which] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        auto results = fc.getResults();
        if (results.isEmpty()) return;
        safe->lastPresetDir = results[0].isDirectory() ? results[0] : results[0].getParentDirectory();

        auto& pm = safe->proc.presets;
        juce::String error;
        int selectBank = -1;

        if (which == 1)                       // preset file(s) → current bank
        {
            const int bank = safe->ensureCurrentBank();
            pm.loadPresetFilesIntoBank (results, bank, error);
            selectBank = bank;
        }
        else if (which == 2)                  // directory → new bank
            selectBank = pm.loadBankDirectory (results[0], error);
        else                                  // .pgm → new bank
            selectBank = pm.importPgmFile (results[0], error);

        if (selectBank >= 0)
        {
            pm.setCurrentBank (selectBank);
            pm.setCurrentPreset (-1);
        }
        if (error.isNotEmpty())
            juce::NativeMessageBox::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon, "Load", error);
        safe->refresh();
    });
}

void ControlSidePanelContent::showSaveDialog()
{
    juce::Component::SafePointer<ControlSidePanelContent> safe (this);
    auto content = std::make_unique<PresetSaveContent> (proc);
    content->setLookAndFeel (&getLookAndFeel());

    // The dialog's SAVE button dismisses the callout; capture the fields then.
    juce::Component::SafePointer<PresetSaveContent> dlgSafe (content.get());
    content->onSave = [safe, dlgSafe]
    {
        if (safe == nullptr || dlgSafe == nullptr) return;
        safe->doSave (dlgSafe->getName(), dlgSafe->getAuthor(), dlgSafe->includeTuning());
    };
    juce::CallOutBox::launchAsynchronously (std::move (content),
                                            saveBtn.getScreenBounds(), nullptr);
}

void ControlSidePanelContent::doSave (const juce::String& name, const juce::String& author,
                                      bool includeTuning)
{
    auto preset = proc.presets.capture (name, author, includeTuning);

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save preset", lastPresetDir.getChildFile (name + ".xml"), "*.xml");
    juce::Component::SafePointer<ControlSidePanelContent> safe (this);
    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe, preset] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (! f.hasFileExtension ("xml")) f = f.withFileExtension ("xml");
            safe->lastPresetDir = f.getParentDirectory();

            juce::String error;
            if (safe->proc.presets.savePresetFile (preset, f, error))
            {
                // Also add it to the current bank so it shows up immediately.
                const int bank = safe->ensureCurrentBank();
                safe->proc.presets.appendPreset (bank, preset);
                safe->proc.presets.setCurrentBank (bank);
                safe->refresh();
            }
            else
                juce::NativeMessageBox::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon, "Save", error);
        });
}

void ControlSidePanelContent::refresh()
{
    const int bankCount     = proc.presets.numBanks();
    const int currentBank   = proc.presets.getCurrentBank();
    const int presetCount   = proc.presets.numPresets (currentBank);
    const int currentPreset = proc.presets.getCurrentPreset();

    // Rebuild the lists only when something actually changed (this runs on the
    // editor timer). updateContent re-reads the row counts from the models.
    if (bankCount != lastBankCount || currentBank != lastCurrentBank)
    {
        bankList.updateContent();
        if (currentBank >= 0 && currentBank < bankCount) bankList.selectRow (currentBank);
        else                                             bankList.deselectAllRows();
        presetList.updateContent();
    }
    if (presetCount != lastPresetCount || currentPreset != lastCurrentPreset
        || currentBank != lastCurrentBank)
    {
        presetList.updateContent();
        if (currentPreset >= 0 && currentPreset < presetCount) presetList.selectRow (currentPreset);
        else                                                   presetList.deselectAllRows();
    }

    lastBankCount = bankCount; lastCurrentBank = currentBank;
    lastPresetCount = presetCount; lastCurrentPreset = currentPreset;

    // MIDI mappings: refresh the cached rows only when they change.
    auto rows = proc.getMidiMappingList();
    if (rows != ccRows)
    {
        ccRows = std::move (rows);
        ccList.updateContent();
        if (ccList.getSelectedRow() >= ccRows.size())
        {
            ccList.deselectAllRows();
            removeBtn.setEnabled (false);
        }
    }
    const bool hasMappings = ! ccRows.isEmpty();
    ccList.setVisible (hasMappings);
    ccEmptyLabel.setVisible (! hasMappings);
}

// ============================================================================
//  DEFAULT PAGE
// ============================================================================

// Footage labels. ' fractions need UTF-8: ⅓ = e2 85 93, ⅔ = e2 85 94, ⅗ = e2 85 97
static const char* kFootage[9] = {
    "16'", "5\xe2\x85\x93'", "8'", "4'", "2\xe2\x85\x94'", "2'", "1\xe2\x85\x97'", "1\xe2\x85\x93'", "1'"
};

DefaultPage::DefaultPage (TuneBfreeAudioProcessor& p) : proc (p)
{
    // ---- Group titles (amber, matching the TINKER/ROTOR pages) ----
    for (auto* t : { &vibTitle, &percTitle, &timbTitle, &drawTitle, &fxTitle, &leslieTitle })
        addAndMakeVisible (t);
    styleGroupTitle (vibTitle,    "VIBRATO");
    styleGroupTitle (percTitle,   "PERCUSSION");
    styleGroupTitle (timbTitle,   "TIMBRALITY");
    styleGroupTitle (drawTitle,   "DRAWBARS");
    styleGroupTitle (fxTitle,     "EFFECTS");
    styleGroupTitle (leslieTitle, "LESLIE");

    // ---- VIBRATO: the B3 dial (V1 C1 V2 C2 V3 C3 = vibrato_type 0..5) + ON/OFF ----
    vibratoKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    vibratoKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    vibratoKnob.setRange (0.0, 5.0, 1.0);
    vibratoKnob.setValue (3.0, juce::dontSendNotification);   // C2
    vibratoKnob.onValueChange = [this] { applyLfoToParams(); updateValueLabels(); };
    addAndMakeVisible (vibratoKnob);
    // Standard value-label style (white, small) — consistent with every read-out.
    vibratoValue.setFont (uiFont (10.0f));
    vibratoValue.setJustificationType (juce::Justification::centred);
    vibratoValue.setColour (juce::Label::textColourId, kWhite);
    addAndMakeVisible (vibratoValue);

    makeRadioGroup ({ &vibOnBtn, &vibOffBtn }, [this] { applyLfoToParams(); });
    vibOffBtn.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (vibOnBtn);
    addAndMakeVisible (vibOffBtn);

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
        setParam ("active_manual", 0.0f);   // unitimbral routing: upper bank sounds
        upperBtn.setToggleState (true,  juce::dontSendNotification);
        lowerBtn.setToggleState (false, juce::dontSendNotification);
    };
    lowerBtn.onClick = [this] {
        if (isUpper) switchToManual (false);
        setParam ("active_manual", 1.0f);   // unitimbral routing: lower bank sounds
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
    splitNoteLabel.setFont (uiFont (10.0f));
    splitNoteLabel.setJustificationType (juce::Justification::centred);
    splitNoteLabel.setColour (juce::Label::textColourId, kWhite);
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

        // Live JI-error read-out (same semantics as the TINKER HARMONICS labels).
        drawbarErr[i].setFont (uiFont (9.0f));
        drawbarErr[i].setJustificationType (juce::Justification::centred);
        drawbarErr[i].setColour (juce::Label::textColourId, kGrey);
        addAndMakeVisible (drawbarErr[i]);
    }

    // ---- Leslie ----
    makeRadioGroup ({ &choraleBtn, &stopBtn, &tremoloBtn }, [this] { applyLeslieToParams(); });
    choraleBtn.setToggleState (true, juce::dontSendNotification);
    for (auto* b : { &choraleBtn, &stopBtn, &tremoloBtn }) addAndMakeVisible (b);

    // BYPASS the whole Leslie (whirl_bypass). Standard amber when engaged; the
    // speed 3-way greys out while bypassed (see syncFromParams).
    bypassBtn.setClickingTogglesState (true);
    addAndMakeVisible (bypassBtn);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, "whirl_bypass", bypassBtn);

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
    reverbKnob.onValueChange = [this] { setParam ("reverb_mix", (float) reverbKnob.getValue()); updateValueLabels(); };
    driveKnob.onValueChange = [this]   // re-set: adds the value label update
    {
        float d = (float) driveKnob.getValue();
        setParam ("character", d);
        setParam ("overdrive", d > 0.0f ? 1.0f : 0.0f);
        updateValueLabels();
    };
    styleCaption (driveLabel,  "DRIVE");
    styleCaption (reverbLabel, "REVERB");
    addAndMakeVisible (driveLabel);
    addAndMakeVisible (reverbLabel);

    // Value read-outs under the knobs (consistency with the TINKER/ROTOR pages).
    for (auto* v : { &driveValue, &reverbValue, &crossfadeValue, &expressionValue })
    {
        v->setFont (uiFont (10.0f));
        v->setJustificationType (juce::Justification::centred);
        v->setColour (juce::Label::textColourId, kWhite);
        addAndMakeVisible (v);
    }
    crossfadeKnob.onValueChange = [this] { setParam ("split_width", (float) crossfadeKnob.getValue()); updateValueLabels(); };
    expressionKnob.onValueChange = [this] { setParam ("expression", (float) expressionKnob.getValue()); updateValueLabels(); };
    updateValueLabels();

    // Right-click parameter menus. Drawbars resolve their id dynamically so the
    // menu follows the UPPER/LOWER selection.
    {
        auto& st_ = proc.apvts;
        auto menu = [&] (juce::Component& c, juce::String id)
        { paramMenus.add (new ParamMenuAttachment (st_, c, [id] { return id; })); };
        for (int i = 0; i < 9; ++i)
            paramMenus.add (new ParamMenuAttachment (st_, drawbars[i], [this, i]
                { return (isUpper ? "drawbar" : "lower_drawbar") + juce::String (i); }));
        menu (vibratoKnob, "vibrato_type");
        menu (vibOnBtn,  "vibrato");  menu (vibOffBtn, "vibrato");
        menu (percOnBtn, "percussion");     menu (percOffBtn,  "percussion");
        menu (percFastBtn, "percussion_dec"); menu (percSlowBtn, "percussion_dec");
        menu (percSoftBtn, "percussion_vol"); menu (percNormBtn, "percussion_vol");
        menu (perc2ndBtn,  "percussion_har"); menu (perc3rdBtn,  "percussion_har");
        menu (bitimbralBtn, "split_enable");
        menu (splitKnob, "split_point");
        menu (crossfadeKnob, "split_width");
        menu (driveKnob, "character");
        menu (reverbKnob, "reverb_mix");
        menu (expressionKnob, "expression");
        // The PLAY Leslie is a single control that drives BOTH rotors, so its
        // right-click maps horn + drum together (ROTOR keeps them independent).
        const juce::StringArray lesliePair { "horn", "drum" };
        for (auto* b : { &choraleBtn, &stopBtn, &tremoloBtn })
            paramMenus.add (new ParamMenuAttachment (st_, *b, lesliePair));
        menu (bypassBtn, "whirl_bypass");
    }

    // Reflect the processor's current parameter values in every wired control.
    syncFromParams();
}

// ---- State helpers ----

// Dial position names, matching vibrato_type 0..5 (vibrato.cpp).
static const char* kVibratoNames[6] = { "V1", "C1", "V2", "C2", "V3", "C3" };

void DefaultPage::setVibControls (int type, bool on)
{
    vibratoKnob.setValue (juce::jlimit (0, 5, type), juce::dontSendNotification);
    vibOnBtn.setToggleState  (on,   juce::dontSendNotification);
    vibOffBtn.setToggleState (! on, juce::dontSendNotification);
    updateValueLabels();
}

// Percussion is upper-manual-only (B3-like), so grey the section under LOWER.
void DefaultPage::setPercussionSectionEnabled (bool enabled)
{
    for (auto* b : { &percOnBtn, &percOffBtn, &percFastBtn, &percSlowBtn,
                     &percSoftBtn, &percNormBtn, &perc2ndBtn, &perc3rdBtn })
        b->setEnabled (enabled);
    percTitle.setAlpha (enabled ? 1.0f : 0.4f);
}

void DefaultPage::updateValueLabels()
{
    vibratoValue.setText (kVibratoNames[juce::jlimit (0, 5, (int) std::lround (vibratoKnob.getValue()))],
                          juce::dontSendNotification);
    driveValue.setText      (juce::String (driveKnob.getValue(), 2),      juce::dontSendNotification);
    reverbValue.setText     (juce::String (reverbKnob.getValue(), 2),     juce::dontSendNotification);
    crossfadeValue.setText  (juce::String ((int) crossfadeKnob.getValue()) + " C", juce::dontSendNotification);
    expressionValue.setText (juce::String (expressionKnob.getValue(), 2), juce::dontSendNotification);
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
    // Frequency read-out (was the nearest note name): the split point is a pitch,
    // not a key, so Hz is the truthful display under microtuning.
    const double hz = splitKnob.getValue();
    splitNoteLabel.setText (juce::String (hz, hz < 100.0 ? 1 : 0) + " Hz",
                            juce::dontSendNotification);
}

ManualState DefaultPage::captureStateFromControls() const
{
    ManualState s;
    for (int i = 0; i < 9; ++i)
        s.drawbars[i] = (float) drawbars[i].getValue();
    s.vibType     = juce::roundToInt (vibratoKnob.getValue());
    s.vibOn       = vibOnBtn.getToggleState();
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
    setVibControls (s.vibType, s.vibOn);
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
    // Vibrato ON/OFF is per manual too (the dial position is shared).
    setVibControls ((int) std::lround (getParam ("vibrato_type")),
                    getParam (isUpper ? "vibrato" : "lower_vibrato") > 0.5f);
    // Percussion is upper-only: grey it out while editing the lower manual.
    setPercussionSectionEnabled (toUpper);
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

// The B3 dial + ON/OFF -> vibrato (on/off) + vibrato_type.
// vibrato_type is interleaved: 0=V1 1=C1 2=V2 3=C2 4=V3 5=C3 (vibrato.cpp).
void DefaultPage::applyLfoToParams()
{
    // ON/OFF is PER MANUAL (vibrato / lower_vibrato); the dial position is the
    // one shared scanner (vibrato_type) — as on the instrument.
    setParam (isUpper ? "vibrato" : "lower_vibrato",
              vibOnBtn.getToggleState() ? 1.0f : 0.0f);
    setParam ("vibrato_type", (float) juce::roundToInt (vibratoKnob.getValue()));
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

    // Manual selection follows the param (host automation / right-click edit).
    const bool lowerActive = getParam ("active_manual") > 0.5f;
    if (lowerActive == isUpper)   // mismatch: switch banks
        switchToManual (! lowerActive);
    upperBtn.setToggleState (! lowerActive, juce::dontSendNotification);
    lowerBtn.setToggleState (lowerActive,   juce::dontSendNotification);

    // Vibrato: shared dial position + the ACTIVE manual's on/off.
    setVibControls ((int) std::lround (getParam ("vibrato_type")),
                    getParam (isUpper ? "vibrato" : "lower_vibrato") > 0.5f);

    // Leslie speed makes no sense while the whirl is bypassed: grey the 3-way.
    const bool whirlByp = getParam ("whirl_bypass") > 0.5f;
    for (auto* b : { &choraleBtn, &stopBtn, &tremoloBtn })
        b->setEnabled (! whirlByp);

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

    // Drawbar JI-error read-outs (same semantics as the TINKER HARMONICS labels).
    double ref = proc.getLastNoteFreq();
    if (ref <= 0.0) ref = 261.63;
    for (int i = 0; i < 9; ++i)
    {
        const double err  = proc.getHarmonicErrorCents (i, ref);
        const bool   zero = std::abs (err) < 0.05;
        drawbarErr[i].setText (zero ? utf8 ("\xc2\xb1") + juce::String ("0")
                                    : juce::String (err > 0 ? "+" : "") + juce::String (err, 1),
                               juce::dontSendNotification);
        drawbarErr[i].setColour (juce::Label::textColourId, zero ? kGrey : kWhite);
    }
    updateValueLabels();
}

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
    const int titleH   = 12;  // height of a group title
    const int valH     = 12;  // height of a value read-out
    const int gap      = 5;   // small gap between the stacked buttons of a switch
    const int twoWayW  = 56;  // width of EACH half of a 2-way switch
    const int switchW  = twoWayW * 2 + 4;       // 3-way switch == 2-way switch total width
    const int timbralW = 122; // width of the timbrality sub-column
    const int effectsW = 158; // width of the effects/leslie/expression sub-column
    const int rightW   = timbralW + effectsW;

    // Row heights shared by BOTH columns of the right region (grid alignment):
    const int rowAH = titleH + 2 + btnH * 3 + gap * 2;      // 3 stacked buttons + title
    const int rowCH = capH + knob + valH;                   // caption + knob + value

    auto area = getLocalBounds().reduced (margin);          // uniform margin on all sides

    // Cross-column vertical anchors: the drawbar captions top-align with the
    // SPLIT caption; the drawbar bottoms align with the row-C knob bottoms.
    const int splitBlockH  = capH + knob + capH;
    const int midTop       = area.getY() + rowAH;
    const int midBottom    = area.getBottom() - rowCH;
    const int splitLabelY  = midTop + (midBottom - midTop - splitBlockH) / 2;

    auto rightRegion = area.removeFromRight (rightW);
    area.removeFromRight (margin);                          // gap between left & right
    auto leftRegion = area;

    // =====================================================================
    //  LEFT REGION
    //  Top band (VIBRATO + PERCUSSION), drawbars anchored to the bottom. The
    //  freed space sits BETWEEN the band and the drawbars (never at an edge).
    // =====================================================================
    {
        const int stackH = btnH * 2 + gap;                  // a 2-way switch stack
        auto band    = leftRegion.removeFromTop (titleH + 2 + knob + valH);
        auto titles  = band.removeFromTop (titleH);
        band.removeFromTop (2);

        // VIBRATO: the B3 dial + its position read-out, ON/OFF 2-way beside it.
        vibTitle.setBounds (titles.getX(), titles.getY(), 110, titleH);
        vibratoKnob.setBounds  (band.getX(), band.getY(), knob, knob);
        vibratoValue.setBounds (band.getX(), band.getY() + knob, knob, valH);
        const int vsX = band.getX() + knob + 10;
        const int vsY = band.getY() + (knob + valH - stackH) / 2;
        vibOnBtn.setBounds  (vsX, vsY, 44, btnH);
        vibOffBtn.setBounds (vsX, vsY + btnH + gap, 44, btnH);

        // PERCUSSION: four 2-way switches, right-aligned to the drawbars below.
        // Wider than the minimum so the vibrato↔percussion void stays modest.
        const int percW = 52, percGap = 14;
        auto percRow = band.removeFromRight (percW * 4 + percGap * 3);
        percTitle.setBounds (percRow.getX(), titles.getY(), 110, titleH);
        const int psY = percRow.getY() + (knob + valH - stackH) / 2;
        juce::TextButton* tops[4] = { &percOnBtn,  &percFastBtn, &percSoftBtn, &perc2ndBtn };
        juce::TextButton* bots[4] = { &percOffBtn, &percSlowBtn, &percNormBtn, &perc3rdBtn };
        for (int i = 0; i < 4; ++i)
        {
            const int px = percRow.getX() + i * (percW + percGap);
            tops[i]->setBounds (px, psY, percW, btnH);
            bots[i]->setBounds (px, psY + btnH + gap, percW, btnH);
        }

        // DRAWBARS — title just above the footage captions; captions top-align
        // with the SPLIT caption; a JI-error row sits under the bars, level with
        // the row-C value read-outs.
        auto bars = leftRegion;
        bars.setTop    (splitLabelY - (titleH + 4));
        bars.setBottom (area.getBottom());
        drawTitle.setBounds (bars.removeFromTop (titleH));
        bars.removeFromTop (4);
        auto labels = bars.removeFromTop (capH);
        auto errRow = bars.removeFromBottom (valH);
        int  cellW  = bars.getWidth() / 9;
        for (int i = 0; i < 9; ++i)
        {
            drawbars[i].setBounds      (bars.removeFromLeft (cellW).reduced (4, 0));
            footageLabels[i].setBounds (labels.removeFromLeft (cellW));
            drawbarErr[i].setBounds    (errRow.removeFromLeft (cellW));
        }
    }

    // =====================================================================
    //  RIGHT REGION — two sub-columns sharing three aligned rows:
    //    Row A (top)    : TIMBRALITY buttons     | EFFECTS knobs
    //    Row C (bottom) : CROSSFADE knob + value | EXPRESSION knob + value
    //    Row B (middle) : SPLIT knob             | LESLIE (fills the rest)
    // =====================================================================
    auto timbral = rightRegion.removeFromLeft (timbralW).withTrimmedRight (margin / 2);
    auto effects = rightRegion.withTrimmedLeft (margin / 2);

    // ---- Row A ----
    {
        auto tA = timbral.removeFromTop (rowAH);
        timbTitle.setBounds (tA.removeFromTop (titleH));
        tA.removeFromTop (2);
        auto ul = tA.removeFromTop (btnH).withSizeKeepingCentre (switchW, btnH);
        upperBtn.setBounds (ul.removeFromLeft (twoWayW));
        lowerBtn.setBounds (ul.removeFromRight (twoWayW));
        tA.removeFromTop (gap);
        bitimbralBtn.setBounds (tA.removeFromTop (btnH).withSizeKeepingCentre (switchW, btnH));
        tA.removeFromTop (gap);
        learnBtn.setBounds (tA.removeFromTop (btnH).withSizeKeepingCentre (switchW, btnH));

        auto eA = effects.removeFromTop (rowAH);
        fxTitle.setBounds (eA.removeFromTop (titleH));
        eA.removeFromTop (2);
        auto dr = eA.removeFromLeft (eA.getWidth() / 2);
        driveLabel.setBounds (dr.removeFromTop (capH));
        driveKnob.setBounds  (dr.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
        driveValue.setBounds (dr.removeFromTop (valH));
        reverbLabel.setBounds (eA.removeFromTop (capH));
        reverbKnob.setBounds  (eA.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
        reverbValue.setBounds (eA.removeFromTop (valH));
    }

    // ---- Row C: bottom knobs + value read-outs (aligned across the columns) ----
    {
        auto tC = timbral.removeFromBottom (rowCH);
        crossfadeLabel.setBounds (tC.removeFromTop (capH));
        crossfadeKnob.setBounds  (tC.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
        crossfadeValue.setBounds (tC.removeFromTop (valH));

        auto eC = effects.removeFromBottom (rowCH);
        expressionLabel.setBounds (eC.removeFromTop (capH));
        expressionKnob.setBounds  (eC.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
        expressionValue.setBounds (eC.removeFromTop (valH));
    }

    // ---- Row B: middle (SPLIT knob + Hz | LESLIE stack) ----
    {
        auto splitBlock = timbral.withSizeKeepingCentre (timbral.getWidth(), splitBlockH);
        splitLabel.setBounds     (splitBlock.removeFromTop (capH));
        splitKnob.setBounds      (splitBlock.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
        splitNoteLabel.setBounds (splitBlock.removeFromTop (capH));

        // LESLIE: title + 3-way + BYPASS.
        const int leslieH = titleH + 2 + btnH * 4 + gap * 3;
        auto leslie = effects.withSizeKeepingCentre (switchW, leslieH);
        leslieTitle.setBounds (leslie.removeFromTop (titleH));
        leslie.removeFromTop (2);
        choraleBtn.setBounds (leslie.removeFromTop (btnH)); leslie.removeFromTop (gap);
        stopBtn.setBounds    (leslie.removeFromTop (btnH)); leslie.removeFromTop (gap);
        tremoloBtn.setBounds (leslie.removeFromTop (btnH)); leslie.removeFromTop (gap);
        bypassBtn.setBounds  (leslie.removeFromTop (btnH));
    }
}

// ============================================================================
//  LABELLED KNOB (TINKER / ROTARY standard control)
// ============================================================================

LabelledKnob::LabelledKnob()
{
    addAndMakeVisible (caption);
    addAndMakeVisible (knob);
    addMouseListener (this, true);   // right-click anywhere on the knob = param menu
}

void LabelledKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && stateRef != nullptr)
        showParamMenu (*stateRef, paramID, this);
}

void LabelledKnob::init (juce::AudioProcessorValueTreeState& state, const juce::String& paramID_,
                         const juce::String& captionText, const juce::String& valueSuffix,
                         int decimalPlaces)
{
    stateRef = &state;
    paramID  = paramID_;
    styleCaption (caption, captionText);
    knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 13);
    knob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob.setColour (juce::Slider::textBoxTextColourId,    kWhite);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, paramID_, knob);
    // AFTER the attachment: it installs the parameter's own text conversion
    // (raw floats, 7 decimals) as textFromValueFunction, which takes precedence
    // over setNumDecimalPlacesToDisplay — so replace it with our own precision.
    knob.textFromValueFunction = [decimalPlaces] (double v)
    {
        return decimalPlaces > 0 ? juce::String (v, decimalPlaces)
                                 : juce::String (juce::roundToInt (v));
    };
    knob.setTextValueSuffix (valueSuffix);
    knob.updateText();
}

void LabelledKnob::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (12));
    knob.setBounds (r);        // the value box below is part of the slider
}

// Fill a combo box with the whirl filter types (eqcomp types 0-8, ids 1-9).
static void fillFilterTypeBox (juce::ComboBox& box)
{
    static const char* types[9] = { "LOW PASS", "HIGH PASS", "BAND PASS 0", "BAND PASS 1",
                                    "NOTCH", "ALL PASS", "PEAKING", "LOW SHELF", "HIGH SHELF" };
    for (int i = 0; i < 9; ++i)
        box.addItem (types[i], i + 1);
}

// ============================================================================
//  TINKER PAGE
// ============================================================================

// Layout constants shared with DefaultPage::resized() — the HARMONICS columns
// must land on the PLAY-page drawbar cells, so these MUST match its values.
static constexpr int kPageMargin = 14;    // = DefaultPage `margin`
static constexpr int kRightColW  = 280;   // = DefaultPage `rightW`

// Stock harmonics entries (pure JI, matching stockJIRatio).
static const char* kDefaultHarmEntry[9] = {
    "1/2", "3/2", "1/1", "2/1", "3/1", "4/1", "5/1", "6/1", "8/1" };

// Parse a Scala-style harmonics entry: "n/d" = ratio, contains "." = cents,
// bare integer = ratio n/1. Returns the interval in cents, or nullopt if the
// text is unparseable. `canonical` gets the normalised display form.
static std::optional<double> parseHarmonicEntry (const juce::String& raw, juce::String& canonical)
{
    auto s = raw.trim().toLowerCase().removeCharacters (" ");
    if (s.endsWith ("c")) s = s.dropLastCharacters (1);
    if (s.isEmpty()) return {};

    if (s.containsChar ('/'))
    {
        if (! s.containsOnly ("0123456789./")) return {};
        const double n = s.upToFirstOccurrenceOf ("/", false, false).getDoubleValue();
        const double d = s.fromFirstOccurrenceOf ("/", false, false).getDoubleValue();
        if (n <= 0.0 || d <= 0.0) return {};
        canonical = s;
        return 1200.0 * std::log2 (n / d);
    }
    if (s.containsChar ('.'))
    {
        if (! s.containsOnly ("0123456789.-")) return {};
        const double c = s.getDoubleValue();
        canonical = juce::String (c, 2) + " c";
        return c;
    }
    if (! s.containsOnly ("0123456789")) return {};
    const double n = s.getDoubleValue();
    if (n <= 0.0) return {};
    canonical = s + "/1";
    return 1200.0 * std::log2 (n);
}

TinkerPage::TinkerPage (TuneBfreeAudioProcessor& p) : proc (p)
{
    auto& st = proc.apvts;

    for (auto* t : { &scannerTitle, &percTitle, &preampTitle, &clickTitle,
                     &xtalkTitle, &harmTitle, &toneTitle, &biasTitle })
        addAndMakeVisible (t);
    styleGroupTitle (biasTitle, "BIAS");
    styleGroupTitle (scannerTitle, "SCANNER");
    styleGroupTitle (percTitle,    "PERCUSSION");
    styleGroupTitle (preampTitle,  "PREAMP");
    styleGroupTitle (clickTitle,   "KEY CLICK");
    styleGroupTitle (xtalkTitle,   "CROSSTALK");
    styleGroupTitle (harmTitle,    "HARMONICS");
    styleGroupTitle (toneTitle,    "TONE");

    // --- row 1: SCANNER | PERCUSSION | PREAMP ---
    scanSpeed.init (st, "scanner_hz", "SPEED", " Hz", 2);
    scanV1.init    (st, "scanner_v1", "V1", "", 1);
    scanV2.init    (st, "scanner_v2", "V2", "", 1);
    scanV3.init    (st, "scanner_v3", "V3", "", 1);

    percFast.init (st, "perc_fast_s",    "FAST", " S", 2);
    percSlow.init (st, "perc_slow_s",    "SLOW", " S", 2);
    percGain.init (st, "perc_gain",      "GAIN", "", 2);
    percSoft.init (st, "perc_soft_gain", "SOFT", "", 2);

    preIn.init    (st, "preamp_in",        "IN", "", 2);
    preOut.init   (st, "preamp_out",       "OUT", "", 2);
    bassPre.init  (st, "preamp_bass_pre",  "BASS PRE", "", 2);
    bassPost.init (st, "preamp_bass_post", "BASS POST", "", 2);
    sag.init      (st, "preamp_sag",       "SAG", "", 3);

    // --- row 2: KEY CLICK | CROSSTALK ---
    styleCaption (atkModelCap, "ATTACK");
    styleCaption (relModelCap, "RELEASE");
    addAndMakeVisible (atkModelCap);
    addAndMakeVisible (relModelCap);
    for (auto* box : { &atkModelBox, &relModelBox })
    {
        box->addItem ("CLICK",  1);   // ENV_CLICK  = 0
        box->addItem ("COSINE", 2);   // ENV_COSINE = 1
        box->addItem ("LINEAR", 3);   // ENV_LINEAR = 2
        box->addItem ("SHELF",  4);   // ENV_SHELF  = 3
        addAndMakeVisible (box);
    }
    atkModelAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        st, "click_attack_model", atkModelBox);
    relModelAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        st, "click_release_model", relModelBox);

    clickAtkLevel.init (st, "click_attack_level",  "LEVEL", "", 2);
    clickMin.init      (st, "click_min_length",    "MIN", "", 2);
    clickMax.init      (st, "click_max_length",    "MAX", "", 2);
    clickRelLevel.init (st, "click_release_level", "LEVEL", "", 2);

    preBias.init (st, "preamp_bias", "BIAS", "", 3);
    preGfb.init  (st, "preamp_gfb",  "FEEDBACK", "", 2);

    xtComp.init  (st, "xtalk_compartment", "COMPART", "", 3);
    xtXfmr.init  (st, "xtalk_transformer", "XFORMER", "", 3);
    xtTerm.init  (st, "xtalk_terminal",    "TERMINAL", "", 3);
    xtWiring.init(st, "xtalk_wiring",      "WIRING", "", 3);

    for (auto* k : { &scanSpeed, &scanV1, &scanV2, &scanV3,
                     &percFast, &percSlow, &percGain, &percSoft,
                     &preIn, &preOut, &bassPre, &bassPost, &sag,
                     &clickAtkLevel, &clickMin, &clickMax, &clickRelLevel,
                     &xtComp, &xtXfmr, &xtTerm, &xtWiring,
                     &preBias, &preGfb,
                     &eqBass, &eqBassSlope, &eqTreble, &eqTrebleSlope })
        addAndMakeVisible (k);

    // --- row 3 left: HARMONICS (entries at the PLAY drawbar positions) ---
    for (int i = 0; i < 9; ++i)
    {
        const auto colour = TuneBfreeLookAndFeel::drawbarColour (i);

        footage[i].setFont (uiFont (11.0f, true));
        footage[i].setJustificationType (juce::Justification::centred);
        footage[i].setColour (juce::Label::textColourId, colour);
        footage[i].setText (utf8 (kFootage[i]), juce::dontSendNotification);
        addAndMakeVisible (footage[i]);

        auto& e = harmEntry[i];
        e.setFont (uiFont (11.0f));
        e.setJustificationType (juce::Justification::centred);
        e.setColour (juce::Label::backgroundColourId, kBtn);
        e.setColour (juce::Label::outlineColourId,    kBorder);
        e.onTextChange = [this, i] { commitHarmonicEntry (i); };
        addAndMakeVisible (e);
        const auto stored = proc.getHarmonicEntryText (i);
        e.setText (stored.isNotEmpty() ? stored : juce::String (kDefaultHarmEntry[i]),
                   juce::dontSendNotification);

        // A|C two-way toggle: A = AUTO (greys the entry), C = CUSTOM.
        harmAutoBtn[i].setButtonText ("A");
        harmCustomBtn[i].setButtonText ("C");
        makeRadioGroup ({ &harmAutoBtn[i], &harmCustomBtn[i] }, [this, i]
        {
            if (auto* p = proc.apvts.getParameter ("harm_auto_" + juce::String (i)))
                p->setValueNotifyingHost (harmAutoBtn[i].getToggleState() ? 1.0f : 0.0f);
            refreshHarmonics();
        });
        addAndMakeVisible (harmAutoBtn[i]);
        addAndMakeVisible (harmCustomBtn[i]);

        ratioErr[i].setFont (uiFont (9.0f));
        ratioErr[i].setJustificationType (juce::Justification::centred);
        addAndMakeVisible (ratioErr[i]);
    }
    refreshHarmonics();

    // --- row 3 right: TONE (EQ spline + wave preset + harmonics reset) ---
    eqBass.init        (st, "eq_bass",         "BASS", "", 2);
    eqBassSlope.init   (st, "eq_bass_slope",   "SLOPE", "", 2);
    eqTreble.init      (st, "eq_treble",       "TREBLE", "", 2);
    eqTrebleSlope.init (st, "eq_treble_slope", "SLOPE", "", 2);

    // WAVE: a 3-way switch (SINE = 0, SQUARE = 1, TRIANGLE = 2 on the `wave` param).
    styleCaption (waveCap, "WAVE");
    addAndMakeVisible (waveCap);
    makeRadioGroup ({ &sineBtn, &squareBtn, &triangleBtn }, [this]
    {
        const float v = squareBtn.getToggleState() ? 1.0f
                      : triangleBtn.getToggleState() ? 2.0f : 0.0f;
        if (auto* p = proc.apvts.getParameter ("wave"))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (v));
    });
    for (auto* b : { &sineBtn, &squareBtn, &triangleBtn })
        addAndMakeVisible (b);
    syncFromParams();

    // ERROR REFERENCE read-out: which fundamental the error labels measure at.
    styleGroupTitle (refTitle, "ERROR REFERENCE");
    addAndMakeVisible (refTitle);
    refValue.setFont (uiFont (12.0f));
    refValue.setJustificationType (juce::Justification::centredLeft);
    refValue.setColour (juce::Label::textColourId, kWhite);
    addAndMakeVisible (refValue);

    // ALL AUTO: flip every drawbar back to AUTO (the stock sound). The CUSTOM
    // entries are deliberately KEPT — each mode remembers its own state.
    resetBtn.onClick = [this]
    {
        for (int i = 0; i < 9; ++i)
            if (auto* pa = proc.apvts.getParameter ("harm_auto_" + juce::String (i)))
                pa->setValueNotifyingHost (1.0f);
        refreshHarmonics();
    };
    addAndMakeVisible (resetBtn);

    // Right-click parameter menus on the non-LabelledKnob controls.
    {
        auto menu = [&] (juce::Component& c, juce::String id)
        { paramMenus.add (new ParamMenuAttachment (st, c, [id] { return id; })); };
        menu (atkModelBox, "click_attack_model");
        menu (relModelBox, "click_release_model");
        menu (sineBtn, "wave"); menu (squareBtn, "wave"); menu (triangleBtn, "wave");
        for (int i = 0; i < 9; ++i)
        {
            menu (harmEntry[i],     "harm_cents_" + juce::String (i));
            menu (harmAutoBtn[i],   "harm_auto_" + juce::String (i));
            menu (harmCustomBtn[i], "harm_auto_" + juce::String (i));
        }
    }
}

void TinkerPage::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // (The red rebuild-LEDs are gone by user decision 2026-07-05: most TINKER
    // groups rebuild anyway — the right-click INFO texts carry the warning.)
}

void TinkerPage::syncFromParams()
{
    const int wave = (int) std::lround (proc.apvts.getRawParameterValue ("wave")->load());
    sineBtn.setToggleState     (wave == 0, juce::dontSendNotification);
    squareBtn.setToggleState   (wave == 1, juce::dontSendNotification);
    triangleBtn.setToggleState (wave == 2, juce::dontSendNotification);
    refreshHarmonics();   // error read-outs follow the last-played note live
}

// Commit an edited entry: parse, normalise the display, persist the string, and
// push the cents value to the parameter (which triggers the engine rebuild).
// Unparseable text reverts to the last good entry.
void TinkerPage::commitHarmonicEntry (int i)
{
    juce::String canonical;
    const auto cents = parseHarmonicEntry (harmEntry[i].getText(), canonical);
    if (! cents.has_value())
    {
        const auto stored = proc.getHarmonicEntryText (i);
        harmEntry[i].setText (stored.isNotEmpty() ? stored : juce::String (kDefaultHarmEntry[i]),
                              juce::dontSendNotification);
        return;
    }
    harmEntry[i].setText (canonical, juce::dontSendNotification);
    proc.setHarmonicEntryText (i, canonical);
    if (auto* p = proc.apvts.getParameter ("harm_cents_" + juce::String (i)))
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 ((float) *cents));
    refreshHarmonics();
}

// AUTO = entry greyed and read-only (the JI harmonic, quantized to the tuning,
// sounds); CUSTOM = entry editable/white and used exactly. Error label: the
// sounding pitch vs the pure JI harmonic, at the last-played fundamental.
void TinkerPage::refreshHarmonics()
{
    const bool haveNote = proc.getLastNoteFreq() > 0.0;
    double ref = haveNote ? proc.getLastNoteFreq() : 261.63;
    refValue.setText (juce::String (ref, 2) + " Hz  ·  "
                          + (haveNote ? "LAST NOTE" : "MIDDLE C"),
                      juce::dontSendNotification);

    for (int i = 0; i < 9; ++i)
    {
        const bool isAuto =
            proc.apvts.getRawParameterValue ("harm_auto_" + juce::String (i))->load() > 0.5f;
        harmAutoBtn[i].setToggleState   (isAuto,   juce::dontSendNotification);
        harmCustomBtn[i].setToggleState (! isAuto, juce::dontSendNotification);
        harmEntry[i].setEditable (false, ! isAuto);   // double-click edits when CUSTOM
        harmEntry[i].setColour (juce::Label::textColourId, isAuto ? kGrey : kWhite);

        const double err  = proc.getHarmonicErrorCents (i, ref);
        const bool   zero = std::abs (err) < 0.05;
        ratioErr[i].setText (zero ? utf8 ("\xc2\xb1") + juce::String ("0 C")
                                  : juce::String (err > 0 ? "+" : "") + juce::String (err, 1) + " C",
                             juce::dontSendNotification);
        ratioErr[i].setColour (juce::Label::textColourId, zero ? kGrey : kWhite);
    }
}

void TinkerPage::resized()
{
    auto area = getLocalBounds().reduced (kPageMargin);
    const int rightX = area.getRight() - kRightColW;      // = PLAY right region x
    // Four compact rows (the REVERB group claimed a row): knobs are a touch
    // smaller than before, uniform within the page.
    const int titleH = 12, capComboH = 12, comboH = 22, knobH = 62, knobW = 50;
    const int rowGap = 6;

    // Lay a row of LabelledKnobs starting at x with the given cell width.
    auto knobRow = [knobH] (std::initializer_list<LabelledKnob*> ks, int x, int y, int w)
    {
        for (auto* k : ks) { k->setBounds (x, y, w, knobH); x += w; }
    };

    // ---- row 1 ----
    int y = area.getY();
    scannerTitle.setBounds (area.getX(), y, 150, titleH);
    percTitle.setBounds    (area.getX() + 216, y, 150, titleH);
    preampTitle.setBounds  (rightX, y, 150, titleH);
    knobRow ({ &scanSpeed, &scanV1, &scanV2, &scanV3 },       area.getX(),       y + titleH + 2, knobW);
    knobRow ({ &percFast, &percSlow, &percGain, &percSoft },  area.getX() + 216, y + titleH + 2, knobW);
    knobRow ({ &preIn, &preOut, &bassPre, &bassPost, &sag },  rightX,            y + titleH + 2, 56);

    // ---- row 2 ----
    y += titleH + 2 + knobH + rowGap;
    clickTitle.setBounds (area.getX(), y, 150, titleH);
    xtalkTitle.setBounds (rightX, y, 150, titleH);
    {
        const int cy = y + titleH + 2;                     // content top of this row
        const int comboY = cy + capComboH + (knobH - capComboH - comboH - 13) / 2; // centre on knob body
        atkModelCap.setBounds (area.getX(), cy, 86, capComboH);
        atkModelBox.setBounds (area.getX(), comboY, 86, comboH);
        knobRow ({ &clickAtkLevel, &clickMin, &clickMax }, area.getX() + 94, cy, knobW);
        relModelCap.setBounds (area.getX() + 252, cy, 86, capComboH);
        relModelBox.setBounds (area.getX() + 252, comboY, 86, comboH);
        knobRow ({ &clickRelLevel }, area.getX() + 346, cy, knobW);
        knobRow ({ &xtComp, &xtXfmr, &xtTerm, &xtWiring }, rightX, cy, 56);
    }

    // ---- row 3: TONE (EQ + WAVE, left) | BIAS (right, under PREAMP) ----
    y += titleH + 2 + knobH + rowGap;
    toneTitle.setBounds (area.getX(), y, 150, titleH);
    {
        const int cy = y + titleH + 2;
        // TONE: the four EQ-spline knobs in ONE row (level, slope, level, slope).
        knobRow ({ &eqBass, &eqBassSlope, &eqTreble, &eqTrebleSlope }, area.getX(), cy, knobW);
        // BIAS: right-aligned to the margin — its two knobs sit exactly under
        // PREAMP's BASS POST / SAG columns above (grid).
        const int biasX = area.getRight() - 2 * 56;
        biasTitle.setBounds (biasX, y, 150, titleH);
        knobRow ({ &preBias, &preGfb }, biasX, cy, 56);
        // WAVE 3-way centred between TONE and BIAS (equal interior gaps).
        const int ww = 86;
        const int wx = (area.getX() + 4 * knobW + biasX - ww) / 2;
        waveCap.setBounds (wx, cy, ww, capComboH);
        int wy = cy + capComboH + 2;
        for (auto* b : { &sineBtn, &squareBtn, &triangleBtn })
        {
            b->setBounds (wx, wy, ww, 15);
            wy += 15 + 2;
        }
    }

    // ---- row 4: HARMONICS (drawbar-aligned) | ERROR REFERENCE + ALL AUTO ----
    y += titleH + 2 + knobH + rowGap;
    harmTitle.setBounds (area.getX(), y, 150, titleH);
    {
        // Right column block, centred vertically in the remaining band.
        const int bh2 = 12 + 4 + 18 + 10 + 24;
        int ry = y + juce::jmax (0, (area.getBottom() - y - bh2) / 2);
        refTitle.setBounds (rightX, ry, kRightColW, 12);          ry += 12 + 4;
        refValue.setBounds (rightX, ry, kRightColW, 18);          ry += 18 + 10;
        resetBtn.setBounds (rightX, ry, 96, 24);
    }

    // Same cell grid as the PLAY drawbars: left region = area minus right column
    // minus the inter-column margin, split into 9 equal cells.
    const int leftW = area.getWidth() - kRightColW - kPageMargin;
    const int cellW = leftW / 9;
    const int footH = 12, entryLen = 50, entryH = 16, modeW = 17, modeH = 13, errH = 11;
    // Stack: footage / rotated entry / A|C toggle / error, centred in the space
    // below the title (the entry occupies entryLen VISUAL height once rotated).
    const int stackH = footH + 2 + entryLen + 4 + modeH + 2 + errH;
    const int stackY = y + titleH + juce::jmax (0, (area.getBottom() - y - titleH - stackH) / 2);
    for (int i = 0; i < 9; ++i)
    {
        const int cx = area.getX() + i * cellW + cellW / 2;   // column centre
        int cy = stackY;
        footage[i].setBounds (cx - cellW / 2, cy, cellW, footH);
        cy += footH + 2;
        // Entry: laid out horizontally, then rotated -90° about its centre so it
        // reads bottom-to-top along the drawbar column.
        auto& e = harmEntry[i];
        e.setTransform (juce::AffineTransform());
        e.setBounds (cx - entryLen / 2, cy + entryLen / 2 - entryH / 2, entryLen, entryH);
        e.setTransform (juce::AffineTransform::rotation (
            -juce::MathConstants<float>::halfPi,
            (float) e.getBounds().getCentreX(), (float) e.getBounds().getCentreY()));
        cy += entryLen + 4;
        harmAutoBtn[i].setBounds   (cx - modeW - 1, cy, modeW, modeH);
        harmCustomBtn[i].setBounds (cx + 1,         cy, modeW, modeH);
        cy += modeH + 2;
        ratioErr[i].setBounds (cx - cellW / 2, cy, cellW, errH);
    }

}

// ============================================================================
//  ROTARY PAGE
// ============================================================================

RotaryPage::RotaryPage (TuneBfreeAudioProcessor& p) : proc (p)
{
    auto& st = proc.apvts;

    for (auto* t : { &hornMotorTitle, &drumMotorTitle, &micTitle, &revTitle,
                     &cabTitle, &mixTitle, &fATitle, &fBTitle, &dFTitle })
        addAndMakeVisible (t);
    styleGroupTitle (revTitle,       "REVERB");
    styleGroupTitle (hornMotorTitle, "HORN MOTOR");
    styleGroupTitle (drumMotorTitle, "DRUM MOTOR");
    styleGroupTitle (cabTitle,       "CABINET");
    styleGroupTitle (micTitle,       "MIC");
    styleGroupTitle (mixTitle,       "MIX");
    styleGroupTitle (fATitle,        "HORN FILTER A");
    styleGroupTitle (fBTitle,        "HORN FILTER B");
    styleGroupTitle (dFTitle,        "DRUM FILTER");

    // REVERB voicing (before the whirl in the chain, hence the far-left strip).
    revDamp.init   (st, "reverb_damping", "DAMP", "", 2);
    revSize.init   (st, "reverb_size",    "SIZE", "", 2);
    revFlavor.init (st, "reverb_flavor",  "FLAVOR", "", 2);
    // Cabinet geometry
    hornRadius.init (st, "horn_radius", "H RADIUS", " cm", 1);
    drumRadius.init (st, "drum_radius", "D RADIUS", " cm", 1);
    hornXOff.init   (st, "horn_xoff",   "X OFFSET", " cm", 1);
    hornZOff.init   (st, "horn_zoff",   "Z OFFSET", " cm", 1);

    hornSlow.init  (st, "horn_slow_rpm", "SLOW", " RPM", 1);
    hornFast.init  (st, "horn_fast_rpm", "FAST", " RPM", 0);
    hornAccel.init (st, "horn_accel",    "ACCEL", " S", 2);
    hornDecel.init (st, "horn_decel",    "DECEL", " S", 2);
    hornBrake.init (st, "horn_brake",    "BRAKE", "", 2);
    drumSlow.init  (st, "drum_slow_rpm", "SLOW", " RPM", 1);
    drumFast.init  (st, "drum_fast_rpm", "FAST", " RPM", 0);
    drumAccel.init (st, "drum_accel",    "ACCEL", " S", 2);
    drumDecel.init (st, "drum_decel",    "DECEL", " S", 2);
    drumBrake.init (st, "drum_brake",    "BRAKE", "", 2);

    micAngle.init  (st, "mic_angle",  "ANGLE", utf8 ("\xc2\xb0"), 0);
    micDist.init   (st, "mic_dist",   "DIST", " CM", 0);
    hornWidth.init (st, "horn_width", "H WIDTH", "", 2);
    drumWidth.init (st, "drum_width", "D WIDTH", "", 2);
    hornLevel.init (st, "horn_level", "H LEVEL", "", 2);
    hornLeak.init  (st, "horn_leak",  "H LEAK", "", 2);

    for (auto* box : { &fAType, &fBType, &dFType })
    {
        fillFilterTypeBox (*box);
        addAndMakeVisible (box);
    }
    fAAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, "horn_filter_a_type", fAType);
    fBAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, "horn_filter_b_type", fBType);
    dFAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, "drum_filter_type",   dFType);
    for (auto* c : { &fACap, &fBCap, &dFCap }) { styleCaption (*c, "TYPE"); addAndMakeVisible (c); }

    fAFreq.init (st, "horn_filter_a_freq", "FREQ", " Hz", 0);
    fAQ.init    (st, "horn_filter_a_q",    "Q", "", 2);
    fAGain.init (st, "horn_filter_a_gain", "GAIN", " DB", 1);
    fBFreq.init (st, "horn_filter_b_freq", "FREQ", " Hz", 0);
    fBQ.init    (st, "horn_filter_b_q",    "Q", "", 2);
    fBGain.init (st, "horn_filter_b_gain", "GAIN", " DB", 1);
    dFFreq.init (st, "drum_filter_freq",   "FREQ", " Hz", 0);
    dFQ.init    (st, "drum_filter_q",      "Q", "", 2);
    dFGain.init (st, "drum_filter_gain",   "GAIN", " DB", 1);

    for (auto* k : { &hornSlow, &hornFast, &hornAccel, &hornDecel, &hornBrake,
                     &drumSlow, &drumFast, &drumAccel, &drumDecel, &drumBrake,
                     &micAngle, &micDist, &hornWidth, &drumWidth,
                     &hornLevel, &hornLeak,
                     &revDamp, &revSize, &revFlavor,
                     &hornRadius, &drumRadius, &hornXOff, &hornZOff,
                     &fAFreq, &fAQ, &fAGain, &fBFreq, &fBQ, &fBGain,
                     &dFFreq, &dFQ, &dFGain })
        addAndMakeVisible (k);

    // Per-rotor speed 3-ways (inline on the motor rows). CHORALE = 1 (slow),
    // STOP = 0, TREMOLO = 2 — the engine's `horn` / `drum` encoding (useRevOption).
    makeRadioGroup ({ &hornChorale, &hornStop, &hornTremolo }, [this]
    {
        setSpeedParam ("horn", hornChorale.getToggleState() ? 1.0f
                             : hornTremolo.getToggleState() ? 2.0f : 0.0f);
    });
    makeRadioGroup ({ &drumChorale, &drumStop, &drumTremolo }, [this]
    {
        setSpeedParam ("drum", drumChorale.getToggleState() ? 1.0f
                             : drumTremolo.getToggleState() ? 2.0f : 0.0f);
    });
    for (auto* b : { &hornChorale, &hornStop, &hornTremolo,
                     &drumChorale, &drumStop, &drumTremolo })
        addAndMakeVisible (b);
    syncFromParams();

    // Right-click parameter menus on the combos / speed switches.
    {
        auto menu = [&] (juce::Component& c, juce::String id)
        { paramMenus.add (new ParamMenuAttachment (st, c, [id] { return id; })); };
        menu (fAType, "horn_filter_a_type");
        menu (fBType, "horn_filter_b_type");
        menu (dFType, "drum_filter_type");
        menu (hornChorale, "horn"); menu (hornStop, "horn"); menu (hornTremolo, "horn");
        menu (drumChorale, "drum"); menu (drumStop, "drum"); menu (drumTremolo, "drum");
    }
}

void RotaryPage::paint (juce::Graphics& g) { g.fillAll (kBg); }

void RotaryPage::setSpeedParam (const char* paramID, float v)
{
    if (auto* p = proc.apvts.getParameter (paramID))
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (v));
}

void RotaryPage::syncFromParams()
{
    auto set3way = [] (juce::TextButton& chorale, juce::TextButton& stop,
                       juce::TextButton& tremolo, int v)
    {
        chorale.setToggleState (v == 1, juce::dontSendNotification);
        stop.setToggleState    (v == 0, juce::dontSendNotification);
        tremolo.setToggleState (v == 2, juce::dontSendNotification);
    };
    set3way (hornChorale, hornStop, hornTremolo,
             (int) std::lround (proc.apvts.getRawParameterValue ("horn")->load()));
    set3way (drumChorale, drumStop, drumTremolo,
             (int) std::lround (proc.apvts.getRawParameterValue ("drum")->load()));

    // Speed is meaningless while the whirl is bypassed: grey the switches.
    const bool whirlByp = proc.apvts.getRawParameterValue ("whirl_bypass")->load() > 0.5f;
    for (auto* b : { &hornChorale, &hornStop, &hornTremolo,
                     &drumChorale, &drumStop, &drumTremolo })
        b->setEnabled (! whirlByp);
}

void RotaryPage::resized()
{
    // Four rows. Signal flow reads LEFT→RIGHT: the REVERB voicing strip sits at
    // the far left (the reverb precedes the whirl in the chain — Crumar-pedal
    // style), then per-rotor motor/speed/filter rows, then cabinet geometry and
    // mic rows.
    auto area = getLocalBounds().reduced (kPageMargin);
    const int titleH = 12, comboH = 22, rowGap = 6;
    const int rowH   = (area.getHeight() - 3 * rowGap) / 4;
    const int knobH  = rowH - titleH - 2;                  // caption + knob + value
    const int cellW  = 56, comboW = 96, revW = 56;
    const int grpX   = area.getX() + revW + 16;            // groups start after the strip
    const int ftrW   = comboW + 8 + 3 * cellW;             // filter block width
    const int rightX = area.getRight() - ftrW;

    auto knobRow = [knobH] (std::initializer_list<LabelledKnob*> ks, int x, int y, int w)
    {
        for (auto* k : ks) { k->setBounds (x, y, w, knobH); x += w; }
    };
    auto filterBlock = [&] (juce::Label& cap, juce::ComboBox& box,
                            LabelledKnob& fr, LabelledKnob& q, LabelledKnob& gn, int y)
    {
        cap.setBounds (rightX, y, comboW, 12);
        box.setBounds (rightX, y + 12 + (knobH - 12 - comboH - 13) / 2, comboW, comboH);
        knobRow ({ &fr, &q, &gn }, rightX + comboW + 8, y, cellW);
    };
    auto speedStack = [&] (juce::TextButton& a, juce::TextButton& b, juce::TextButton& c, int y)
    {
        const int bw = 84, bh = 20, bgap = 3;
        const int sx = grpX + 5 * cellW + (rightX - grpX - 5 * cellW - bw) / 2;
        const int sy = y + (knobH - (3 * bh + 2 * bgap)) / 2;
        a.setBounds (sx, sy, bw, bh);
        b.setBounds (sx, sy + bh + bgap, bw, bh);
        c.setBounds (sx, sy + 2 * (bh + bgap), bw, bh);
    };

    // ---- rows 1-3 carry the REVERB strip (DAMP / SIZE / FLAVOR) ----
    int y = area.getY();
    revTitle.setBounds (area.getX(), y, revW + 10, titleH);
    revDamp.setBounds   (area.getX(), y + titleH + 2, revW, knobH);
    revSize.setBounds   (area.getX(), y + rowH + rowGap + titleH + 2, revW, knobH);
    revFlavor.setBounds (area.getX(), y + 2 * (rowH + rowGap) + titleH + 2, revW, knobH);

    // ---- row 1: HORN — motor + speed + filter A ----
    hornMotorTitle.setBounds (grpX, y, 150, titleH);
    fATitle.setBounds        (rightX, y, 150, titleH);
    knobRow ({ &hornSlow, &hornFast, &hornAccel, &hornDecel, &hornBrake },
             grpX, y + titleH + 2, cellW);
    speedStack (hornChorale, hornStop, hornTremolo, y + titleH + 2);
    filterBlock (fACap, fAType, fAFreq, fAQ, fAGain, y + titleH + 2);

    // ---- row 2: DRUM — motor + speed + filter B ----
    y += rowH + rowGap;
    drumMotorTitle.setBounds (grpX, y, 150, titleH);
    fBTitle.setBounds        (rightX, y, 150, titleH);
    knobRow ({ &drumSlow, &drumFast, &drumAccel, &drumDecel, &drumBrake },
             grpX, y + titleH + 2, cellW);
    speedStack (drumChorale, drumStop, drumTremolo, y + titleH + 2);
    filterBlock (fBCap, fBType, fBFreq, fBQ, fBGain, y + titleH + 2);

    // ---- row 3: CABINET geometry | MIX (right-aligned, under the Q/GAIN cols) ----
    y += rowH + rowGap;
    cabTitle.setBounds (grpX, y, 150, titleH);
    knobRow ({ &hornRadius, &drumRadius, &hornXOff, &hornZOff },
             grpX, y + titleH + 2, cellW);
    const int mixX = area.getRight() - 2 * cellW;
    mixTitle.setBounds (mixX, y, 150, titleH);
    knobRow ({ &hornLevel, &hornLeak }, mixX, y + titleH + 2, cellW);

    // ---- row 4: MIC | DRUM FILTER (anchors the bottom-right corner) ----
    y += rowH + rowGap;
    micTitle.setBounds (grpX, y, 150, titleH);
    dFTitle.setBounds  (rightX, y, 150, titleH);
    knobRow ({ &micAngle, &micDist, &hornWidth, &drumWidth },
             grpX, y + titleH + 2, cellW);
    filterBlock (dFCap, dFType, dFFreq, dFQ, dFGain, y + titleH + 2);
}

// ============================================================================
//  TOP-LEVEL EDITOR
// ============================================================================

// Popup content for the header 🔊 button: one vertical master-volume slider,
// shown in a CallOutBox (like the menu-bar volume slider on macOS/Debian).
class VolumeSliderContent : public juce::Component
{
public:
    explicit VolumeSliderContent (TuneBfreeAudioProcessor& p)
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            p.apvts, "master_volume", slider);
        setSize (36, 120);
    }
    void resized() override { slider.setBounds (getLocalBounds().reduced (4)); }

private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

TuneBfreeAudioProcessorEditor::TuneBfreeAudioProcessorEditor (TuneBfreeAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      defaultPage (p), tinkerPage (p), rotaryPage (p),
      tuningContent (p), controlContent (p)
{
    setLookAndFeel (&laf);

    // Header: amber background, black title.
    titleLabel.setFont (uiFont (17.0f, true));
    titleLabel.setText ("tuneBfree", juce::dontSendNotification);
    titleLabel.setColour (juce::Label::textColourId, kBtn);
    addAndMakeVisible (titleLabel);

    // Page radio (header centre): PLAY / TINKER / ROTOR. Same design language as
    // the TUNING button: black when unselected, red when selected.
    juce::TextButton* pages[3] = { &playBtn, &tinkerBtn, &rotaryBtn };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = pages[i];
        b->setColour (juce::TextButton::buttonColourId,   kBtn);
        b->setColour (juce::TextButton::buttonOnColourId, kRed);
        b->setColour (juce::TextButton::textColourOffId,  kAmber);
        b->setColour (juce::TextButton::textColourOnId,   kWhite);
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { setPage (i); };
        addAndMakeVisible (b);
    }
    playBtn.setToggleState (true, juce::dontSendNotification);

    // TUNING button: dark normally, RED when the panel is open. The panel is an
    // editor-level overlay, independent of the page radio.
    tuningBtn.setClickingTogglesState (false);
    tuningBtn.setColour (juce::TextButton::buttonColourId,   kBtn);
    tuningBtn.setColour (juce::TextButton::buttonOnColourId, kRed);
    tuningBtn.setColour (juce::TextButton::textColourOffId,  kAmber);
    tuningBtn.setColour (juce::TextButton::textColourOnId,   kWhite);
    // TUNING and CONTROL share the right-column overlay slot: opening one closes
    // the other (both can be closed, but not both open).
    tuningBtn.onClick = [this]
    {
        const bool show = ! tuningContent.isVisible();
        tuningContent.setVisible (show);
        if (show) controlContent.setVisible (false);
        syncSidePanelButtons();
    };
    addAndMakeVisible (tuningBtn);

    // CONTROL: presets (program change) + MIDI mappings.
    controlBtn.setClickingTogglesState (false);
    controlBtn.setColour (juce::TextButton::buttonColourId,   kBtn);
    controlBtn.setColour (juce::TextButton::buttonOnColourId, kRed);
    controlBtn.setColour (juce::TextButton::textColourOffId,  kAmber);
    controlBtn.setColour (juce::TextButton::textColourOnId,   kWhite);
    controlBtn.onClick = [this]
    {
        const bool show = ! controlContent.isVisible();
        controlContent.setVisible (show);
        if (show) tuningContent.setVisible (false);
        syncSidePanelButtons();
    };
    addAndMakeVisible (controlBtn);

    // "!" (panic: release all notes) and 🔊 (master volume popup), mid-header.
    panicBtn.setColour (juce::TextButton::buttonColourId,  kBtn);
    panicBtn.setColour (juce::TextButton::textColourOffId, kAmber);
    panicBtn.onClick = [this] { proc.triggerPanic(); };
    addAndMakeVisible (panicBtn);

    volumeBtn.getProperties().set ("speakerIcon", true);   // drawn by the LookAndFeel
    volumeBtn.setColour (juce::TextButton::buttonColourId,  kBtn);
    volumeBtn.setColour (juce::TextButton::textColourOffId, kAmber);
    volumeBtn.onClick = [this]
    {
        juce::CallOutBox::launchAsynchronously (
            std::make_unique<VolumeSliderContent> (proc),
            volumeBtn.getScreenBounds(), nullptr);
    };
    addAndMakeVisible (volumeBtn);
    paramMenus.add (new ParamMenuAttachment (proc.apvts, volumeBtn,
                                             [] { return juce::String ("master_volume"); }));

    // Every header button presses towards RED (the header's active colour).
    for (auto* b : { &tuningBtn, &controlBtn, &panicBtn, &volumeBtn,
                     &playBtn, &tinkerBtn, &rotaryBtn })
        b->getProperties().set ("header", true);

    addAndMakeVisible (defaultPage);
    addChildComponent (tinkerPage);   // hidden until selected
    addChildComponent (rotaryPage);
    addChildComponent (tuningContent);    // overlays paint on top of the pages
    addChildComponent (controlContent);   // LAST: same slot as tuningContent

    setSize (740, 430);

    // Dev hooks (used for the mockup-review screenshot workflow):
    //   TUNEBFREE_PAGE=1|2          start on TINKER / ROTOR
    //   TUNEBFREE_TUNING=1          start with the tuning panel open
    //   TUNEBFREE_SNAPSHOT=out.png  save a 2x snapshot of the editor and quit
    if (auto* pg = std::getenv ("TUNEBFREE_PAGE"))
        setPage (juce::jlimit (0, 2, juce::String (pg).getIntValue()));
    if (std::getenv ("TUNEBFREE_TUNING") != nullptr)
    {
        tuningContent.setVisible (true);
        syncSidePanelButtons();
    }
    if (std::getenv ("TUNEBFREE_CONTROL") != nullptr)
    {
        controlContent.setVisible (true);
        syncSidePanelButtons();
    }
    if (auto* snap = std::getenv ("TUNEBFREE_SNAPSHOT"))
    {
        juce::String path (snap);
        juce::Timer::callAfterDelay (1200, [this, path]
        {
            auto img = createComponentSnapshot (getLocalBounds(), true, 2.0f);
            juce::File f (path);
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat png;
            if (os.openedOk()) png.writeImageToStream (img, os);
            juce::JUCEApplicationBase::quit();
        });
    }

    startTimerHz (15);   // drives the tuning clock + control sync (host automation / presets)
}

void TuneBfreeAudioProcessorEditor::setPage (int page)
{
    currentPage = page;
    playBtn.setToggleState   (page == 0, juce::dontSendNotification);
    tinkerBtn.setToggleState (page == 1, juce::dontSendNotification);
    rotaryBtn.setToggleState (page == 2, juce::dontSendNotification);
    defaultPage.setVisible (page == 0);
    tinkerPage.setVisible  (page == 1);
    rotaryPage.setVisible  (page == 2);
    playBtn.repaint(); tinkerBtn.repaint(); rotaryBtn.repaint();
}

void TuneBfreeAudioProcessorEditor::syncSidePanelButtons()
{
    tuningBtn.setToggleState  (tuningContent.isVisible(),  juce::dontSendNotification);
    controlBtn.setToggleState (controlContent.isVisible(), juce::dontSendNotification);
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
    r.removeFromTop (44);

    // ---- Header: one consistent button height/row and ONE gap everywhere —
    //      the differing button designs (! and the speaker) do the grouping. ----
    const int bh = 28, by = (44 - bh) / 2, bgap = 6;
    titleLabel.setBounds (14, by, 140, bh);

    tuningBtn.setBounds  (getWidth() - 14 - 76, by, 76, bh);
    controlBtn.setBounds (tuningBtn.getX() - bgap - 76, by, 76, bh);

    const int pw = 68;
    int px = (getWidth() - (3 * pw + 2 * bgap)) / 2;
    for (auto* b : { &playBtn, &tinkerBtn, &rotaryBtn })
    {
        b->setBounds (px, by, pw, bh);
        px += pw + bgap;
    }
    panicBtn.setBounds  (px, by, bh, bh);   // square "!"
    volumeBtn.setBounds (panicBtn.getRight() + bgap, by, bh, bh);

    defaultPage.setBounds (r);
    tinkerPage.setBounds  (r);
    rotaryPage.setBounds  (r);

    // Side-panel overlays: both cover the right column of the window (same
    // width the tuning panel had inside the PLAY page: right region + half the
    // centre gap). Only one is visible at a time.
    const auto panelBounds = getLocalBounds().withTrimmedTop (44).removeFromRight (301);
    tuningContent.setBounds  (panelBounds);
    controlContent.setBounds (panelBounds);
}

bool TuneBfreeAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
#if TUNEBFREE_MELATONIN
    if (key == juce::KeyPress ('i', juce::ModifierKeys::commandModifier, 0))
    {
        inspector.setVisible (! inspector.isVisible());
        inspector.toggle (inspector.isVisible());
        return true;
    }
#else
    juce::ignoreUnused (key);
#endif
    return false;
}

void TuneBfreeAudioProcessorEditor::timerCallback()
{
    if (defaultPage.isVisible())
        defaultPage.syncFromParams();
    if (tuningContent.isVisible())
        tuningContent.refresh();
    if (controlContent.isVisible())
        controlContent.refresh();
    if (rotaryPage.isVisible())
        rotaryPage.syncFromParams();   // follow PLAY-page 3-way / host automation
    if (tinkerPage.isVisible())
        tinkerPage.syncFromParams();   // WAVE 3-way + harmonics follow the params
}
