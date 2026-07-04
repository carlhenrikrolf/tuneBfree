#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ============================================================================
//  TuneBfreeLookAndFeel
//  ---------------------------------------------------------------------------
//  All custom drawing lives here: drawbars, knobs, buttons, the expression
//  pedal. Components ask the LookAndFeel how to draw themselves, so changing
//  the visual style of (say) every knob is a one-place edit in PluginEditor.cpp.
// ============================================================================

class TuneBfreeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TuneBfreeLookAndFeel();

    // Vertical/horizontal sliders. We special-case two kinds, tagged with a
    // component property (see DefaultPage ctor):
    //   "drawbar" -> Hammond drawbar (pull down = louder)
    //   "pedal"   -> expression pedal (a big filled box)
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    // All rotary knobs (depth, drive, reverb, split, crossfade).
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    // All text buttons / switches.
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isHighlighted, bool isDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // Keep the encoding dropdown and its popup in the same UI font as everything else.
    juce::Font getComboBoxFont  (juce::ComboBox&) override;
    juce::Font getPopupMenuFont () override;

    // Slider value boxes (the editable read-outs under TINKER/ROTARY knobs).
    juce::Font getLabelFont (juce::Label&) override;

    // Drawbar cap colour by index (0 = 16', 8 = 1').
    static juce::Colour drawbarColour (int index);
};

// ============================================================================
//  ManualState — a snapshot of every per-manual control (UI only, for now).
//  Switching UPPER/LOWER saves the current controls here and restores the
//  other manual's saved values. No audio wiring yet.
// ============================================================================

struct ManualState
{
    float drawbars[9] = { 7.f, 8.f, 8.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    int   vibratoMode = 2;     // 0 = off, 1 = vibrato, 2 = chorus
    int   depth       = 2;     // 1..3
    bool  percOn      = false;
    bool  percFast    = false; // false = slow decay
    bool  percSoft    = false; // false = hard/normal volume
    bool  percThird   = false; // false = 2nd harmonic, true = 3rd
};

// ============================================================================
//  TuningSidePanelContent
//  ---------------------------------------------------------------------------
//  The grey panel that slides in from the right when TUNING is pressed.
//  One vertical column laid out top-to-bottom in three blocks (see TUNING_PANEL.md):
//    1. nameless frequency read-out : penultimate Hz | last Hz, then the interval
//    2. STATUS                      : tuning name, scale period, last-update clock
//    3. SETTINGS                    : encoding menu, then .scl/.kbm loaders + a
//                                     note-on / continuous toggle
//  The two section gaps (above STATUS and above SETTINGS) share the leftover
//  height equally, so the block fills the panel to the bottom.
// ============================================================================

class TuningSidePanelContent : public juce::Component
{
public:
    explicit TuningSidePanelContent (TuneBfreeAudioProcessor& p);
    void paint   (juce::Graphics&) override;
    void resized () override;
    void refresh ();                       // pull live values from the processor

private:
    TuneBfreeAudioProcessor& proc;

    // --- frequency read-out (nameless top block, bordered boxes) ---
    // penultimate note-on (left) and last note-on (right), with the interval below.
    juce::Label      penultimateHzLabel, lastHzLabel, centsLabel;

    // --- STATUS block ---
    juce::Label      statusTitle;          // "STATUS" section header
    juce::Label      scaleNameLabel;       // tuning name, e.g. "13ED3", or "UNNAMED"
    juce::Label      periodLabel;          // "1200c · INFERRED" or "NONE (X c)"
    juce::Label      timestampLabel;       // last-update clock (ticks while MTS is live)

    // --- SETTINGS block ---
    juce::Label      settingsTitle;        // "SETTINGS" section header
    juce::ComboBox   encodingBox;          // microtuning encoding (UI-only for now)
    juce::TextButton channelsBtn { "CHANNELS" };                 // opens the channel popup
    juce::TextButton loadSclBtn, loadKbmBtn;                      // SCALE / MAP loaders
    juce::TextButton noteOnBtn { "NOTE ON" }, alwaysBtn { "ALWAYS" }; // 2-way toggle

    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File lastTuningDir { juce::File::getSpecialLocation (juce::File::userHomeDirectory) };

    // After loading a file while the source isn't FILE, offer to switch to FILE.
    void maybeOfferSwitchToFile();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuningSidePanelContent)
};

// ============================================================================
//  DefaultPage — the main play screen.
//  ---------------------------------------------------------------------------
//  Layout (left to right):
//    LEFT  region : top strip (vibrato / depth / percussion) + drawbars below
//    RIGHT region : timbrality column + effects/leslie/expression column
//  The tuning panel, when shown, covers exactly the RIGHT region so the
//  drawbars and the vibrato/percussion strip stay visible.
// ============================================================================

class DefaultPage : public juce::Component
{
public:
    explicit DefaultPage (TuneBfreeAudioProcessor& p);
    void paint   (juce::Graphics&) override;
    void resized () override;

    void toggleTuningPanel ();
    bool isTuningPanelShowing () const;
    void refreshTuningPanel ();

    // Pull current parameter values into the controls (host automation / preset
    // recall). Called from the editor's timer; skips while the user is dragging.
    void syncFromParams ();

private:
    TuneBfreeAudioProcessor& proc;

    // Tuning overlay (child component; hidden until TUNING is pressed).
    TuningSidePanelContent tuningContent;

    // --- LFO: vibrato/chorus + depth ---
    juce::TextButton vibratoBtn { "VIBRATO" };
    juce::TextButton chorusBtn  { "CHORUS"  };
    juce::TextButton modOffBtn  { "OFF"     };
    juce::Slider     depthKnob;
    juce::Label      depthLabel;

    // --- Envelope: percussion, as four 2-way vertical switches in a row ---
    juce::TextButton percOnBtn   { "ON"   }, percOffBtn  { "OFF"  };
    juce::TextButton percFastBtn { "FAST" }, percSlowBtn { "SLOW" };
    juce::TextButton percSoftBtn { "SOFT" }, percNormBtn { "NORM" };
    juce::TextButton perc2ndBtn  { "2ND"  }, perc3rdBtn  { "3RD"  };

    // --- Timbrality (UI only) ---
    juce::TextButton upperBtn     { "UPPER"     };
    juce::TextButton lowerBtn     { "LOWER"     };
    juce::TextButton bitimbralBtn { "BITIMBRAL" };
    juce::TextButton learnBtn     { "LEARN"     };   // set split from played notes
    juce::Slider     splitKnob;
    juce::Label      splitLabel;
    juce::Label      splitNoteLabel;   // shows the split note, e.g. "C4"
    juce::Slider     crossfadeKnob;
    juce::Label      crossfadeLabel;

    // --- Drawbars ---
    juce::Slider drawbars[9];
    juce::Label  footageLabels[9];

    // --- Leslie ---
    juce::TextButton choraleBtn { "CHORALE" };
    juce::TextButton stopBtn    { "STOP"    };
    juce::TextButton tremoloBtn { "TREMOLO" };

    // --- Expression (knob, aligned with crossfade) ---
    juce::Slider expressionKnob;
    juce::Label  expressionLabel;

    // --- Effects ---
    juce::Slider driveKnob;
    juce::Label  driveLabel;
    juce::Slider reverbKnob;
    juce::Label  reverbLabel;

    // UI-only manual state
    ManualState upperState, lowerState;
    bool        isUpper = true;

    void setModButtons  (int mode);
    void setPercButtons (bool on, bool fast, bool soft, bool third);
    void updateSplitNoteLabel ();
    ManualState captureStateFromControls () const;
    void updateControlsFromState (const ManualState& s);
    void switchToManual (bool toUpper);

    // --- engine wiring (control -> parameter) ---
    void  setParam (const juce::String& id, float realValue);
    float getParam (const juce::String& id) const;
    void  applyLfoToParams ();      // VIBRATO/CHORUS/OFF + DEPTH -> vibrato, vibrato_type
    void  applyLeslieToParams ();   // CHORALE/STOP/TREMOLO      -> drum, horn
    void  applyPercToParams ();     // the four percussion 2-way switches

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DefaultPage)
};

// ============================================================================
//  LabelledKnob — the standard TINKER/ROTARY control: grey caption above a
//  rotary knob with an editable value box below. init() binds it to an APVTS
//  parameter, so host automation / preset recall update it automatically.
// ============================================================================

struct LabelledKnob : public juce::Component
{
    LabelledKnob();
    void init (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
               const juce::String& captionText, const juce::String& valueSuffix = {},
               int decimalPlaces = 2);
    void resized() override;

    juce::Label  caption;
    juce::Slider knob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ============================================================================
//  TinkerPage — engine physics (.cfg territory): scanner, percussion envelope,
//  preamp, key click, crosstalk, tonegen EQ + wave, and the drawbar HARMONICS
//  (ratio_top/ratio_bot fractions), laid out at the SAME x-positions as the
//  PLAY-page drawbars so page flips keep each drawbar's column in place.
// ============================================================================

class TinkerPage : public juce::Component
{
public:
    explicit TinkerPage (TuneBfreeAudioProcessor& p);
    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    TuneBfreeAudioProcessor& proc;

    // group titles (amber, all-caps)
    juce::Label scannerTitle, percTitle, preampTitle, clickTitle,
                xtalkTitle, harmTitle, toneTitle;

    // SCANNER / PERCUSSION / PREAMP (row 1)
    LabelledKnob scanSpeed, scanV1, scanV2, scanV3;
    LabelledKnob percFast, percSlow, percGain, percSoft;
    LabelledKnob preIn, preOut, bassPre, bassPost, sag;

    // KEY CLICK / CROSSTALK (row 2)
    juce::Label    atkModelCap, relModelCap;
    juce::ComboBox atkModelBox, relModelBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> atkModelAtt, relModelAtt;
    LabelledKnob clickAtkLevel, clickMin, clickMax, clickRelLevel;
    LabelledKnob xtComp, xtXfmr, xtTerm, xtWiring;

    // HARMONICS (row 3, left) — numerator over denominator per drawbar, plus a
    // read-out of the deviation from the just-intonation harmonic in cents.
    juce::Label  footage[9];
    juce::Slider ratioTop[9], ratioBot[9];
    juce::Label  ratioErr[9];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> topAtt[9], botAtt[9];

    // TONE (row 3, right; left-aligned with CROSSTALK/PREAMP)
    LabelledKnob eqBass, eqBassSlope, eqTreble, eqTrebleSlope;
    juce::Label    waveCap;
    juce::ComboBox waveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAtt;
    juce::TextButton resetBtn { "RESET" };

    void updateErrorLabel (int i);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TinkerPage)
};

// ============================================================================
//  RotaryPage — Leslie physics (x42-whirl territory): motors, filters, cabinet
//  mics, and independent HORN / DRUM speed switches (the PLAY-page 3-way sets
//  both `horn` and `drum`; here each rotor gets its own CHORALE/STOP/TREMOLO).
// ============================================================================

class RotaryPage : public juce::Component
{
public:
    explicit RotaryPage (TuneBfreeAudioProcessor& p);
    void paint   (juce::Graphics&) override;
    void resized () override;
    void syncFromParams();   // horn/drum switches follow the params (host / PLAY page)

private:
    TuneBfreeAudioProcessor& proc;

    juce::Label hornMotorTitle, drumMotorTitle, micTitle, speedTitle,
                fATitle, fBTitle, dFTitle, mixTitle;

    LabelledKnob hornSlow, hornFast, hornAccel, hornDecel, hornBrake;
    LabelledKnob drumSlow, drumFast, drumAccel, drumDecel, drumBrake;
    LabelledKnob micAngle, micDist, hornWidth, drumWidth;   // MIC & CABINET
    LabelledKnob hornLevel, hornLeak;                       // MIX

    juce::Label    fACap, fBCap, dFCap;
    juce::ComboBox fAType, fBType, dFType;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fAAtt, fBAtt, dFAtt;
    LabelledKnob fAFreq, fAQ, fAGain;
    LabelledKnob fBFreq, fBQ, fBGain;
    LabelledKnob dFFreq, dFQ, dFGain;

    // SPEED — one 3-way per rotor + bypass. CHORALE=1 STOP=0 TREMOLO=2.
    juce::Label      hornSwCap, drumSwCap;
    juce::TextButton hornChorale { "CHORALE" }, hornStop { "STOP" }, hornTremolo { "TREMOLO" };
    juce::TextButton drumChorale { "CHORALE" }, drumStop { "STOP" }, drumTremolo { "TREMOLO" };
    juce::TextButton bypassBtn { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;

    void setSpeedParam (const char* paramID, float v);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RotaryPage)
};

// ============================================================================
//  TuneBfreeAudioProcessorEditor — top-level window: amber header (title, page
//  radio, TUNING) + the current page (PLAY / TINKER / ROTARY).
// ============================================================================

class TuneBfreeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit TuneBfreeAudioProcessorEditor (TuneBfreeAudioProcessor&);
    ~TuneBfreeAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void timerCallback () override;

private:
    TuneBfreeAudioProcessor& proc;
    TuneBfreeLookAndFeel laf;

    juce::Label      titleLabel;
    juce::TextButton tuningBtn { "TUNING" };
    juce::TextButton panicBtn  { "PANIC" };   // release all notes (debugging); temporary

    // Page radio (header centre). PLAY is the default page.
    juce::TextButton playBtn { "PLAY" }, tinkerBtn { "TINKER" }, rotaryBtn { "ROTARY" };
    int currentPage = 0;                      // 0 = PLAY, 1 = TINKER, 2 = ROTARY
    void setPage (int page);

    DefaultPage defaultPage;
    TinkerPage  tinkerPage;
    RotaryPage  rotaryPage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneBfreeAudioProcessorEditor)
};
