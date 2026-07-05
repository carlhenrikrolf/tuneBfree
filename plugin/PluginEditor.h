#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Optional GUI-layout inspector (see CMakeLists.txt for how to install it).
#if TUNEBFREE_MELATONIN
 #include <melatonin_inspector/melatonin_inspector.h>
#endif

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
    // Handles the drawn speaker icon (header 🔊); everything else falls through.
    void drawButtonText (juce::Graphics&, juce::TextButton&,
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
    int   vibType     = 3;     // vibrato_type 0..5 = V1 C1 V2 C2 V3 C3
    bool  vibOn       = false;
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
    juce::TextButton channelsBtn { "CHANNELS" };   // opens the channel popup
    juce::TextButton loadBtn;                      // ONE loader: .scl AND .kbm together
    juce::TextButton filesBtn { "FILES" };         // popup listing the loaded files
    juce::TextButton noteOnBtn { "NOTE ON" }, alwaysBtn { "ALWAYS" }; // 2-way toggle

    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File lastTuningDir { juce::File::getSpecialLocation (juce::File::userHomeDirectory) };

    // The user's NOTE ON / ALWAYS choice (editable under MTS ESP only). While the
    // source is SYSEX the greyed toggle acts as an INDICATOR instead, following
    // the last received message (realtime = ALWAYS, bulk dump = NOTE ON).
    bool retuneAlwaysPref = false;

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

    // Pull current parameter values into the controls (host automation / preset
    // recall). Called from the editor's timer; skips while the user is dragging.
    void syncFromParams ();

private:
    TuneBfreeAudioProcessor& proc;

    // Group titles (amber, like the TINKER/ROTOR pages).
    juce::Label vibTitle, percTitle, timbTitle, drawTitle, fxTitle, leslieTitle;

    // --- VIBRATO: the B3 dial (V1 C1 V2 C2 V3 C3 = vibrato_type 0..5) + ON/OFF ---
    juce::Slider     vibratoKnob;
    juce::Label      vibratoValue;               // dial position read-out (V1..C3)
    juce::TextButton vibOnBtn { "ON" }, vibOffBtn { "OFF" };

    // --- Envelope: percussion, as four 2-way vertical switches in a row ---
    juce::TextButton percOnBtn   { "ON"   }, percOffBtn  { "OFF"  };
    juce::TextButton percFastBtn { "FAST" }, percSlowBtn { "SLOW" };
    juce::TextButton percSoftBtn { "SOFT" }, percNormBtn { "NORM" };
    juce::TextButton perc2ndBtn  { "2ND"  }, perc3rdBtn  { "3RD"  };

    // --- Timbrality (UI only) ---
    juce::TextButton upperBtn     { "UPPER"     };
    juce::TextButton lowerBtn     { "LOWER"     };
    juce::TextButton bitimbralBtn { "BITIMBRAL" };
    juce::TextButton learnBtn     { "KEYPRESS"  };   // set split from held keys (silent)
    juce::Slider     splitKnob;
    juce::Label      splitLabel;
    juce::Label      splitNoteLabel;   // shows the split note, e.g. "C4"
    juce::Slider     crossfadeKnob;
    juce::Label      crossfadeLabel;

    // --- Drawbars (+ live JI-error read-outs, as on the TINKER page) ---
    juce::Slider drawbars[9];
    juce::Label  footageLabels[9];
    juce::Label  drawbarErr[9];

    // --- Leslie ---
    juce::TextButton choraleBtn { "CHORALE" };
    juce::TextButton stopBtn    { "STOP"    };
    juce::TextButton tremoloBtn { "TREMOLO" };
    juce::TextButton bypassBtn  { "BYPASS"  };   // whirl_bypass (red when engaged)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;

    // --- Expression (knob, aligned with crossfade) ---
    juce::Slider expressionKnob;
    juce::Label  expressionLabel, expressionValue;
    juce::Label  crossfadeValue;                 // cents read-out under CROSSFADE

    // --- Effects ---
    juce::Slider driveKnob;
    juce::Label  driveLabel, driveValue;
    juce::Slider reverbKnob;
    juce::Label  reverbLabel, reverbValue;

    // UI-only manual state
    ManualState upperState, lowerState;
    bool        isUpper = true;

    void setVibControls (int type, bool on);
    void setPercButtons (bool on, bool fast, bool soft, bool third);
    void setPercussionSectionEnabled (bool enabled);   // greyed under LOWER
    void updateSplitNoteLabel ();
    void updateValueLabels ();
    ManualState captureStateFromControls () const;
    void updateControlsFromState (const ManualState& s);
    void switchToManual (bool toUpper);

    // --- engine wiring (control -> parameter) ---
    // Right-click parameter menus (name / edit value / info) on the controls.
    juce::OwnedArray<juce::MouseListener> paramMenus;

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
    void mouseDown (const juce::MouseEvent&) override;   // right-click: parameter menu

    juce::Label  caption;
    juce::Slider knob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    juce::AudioProcessorValueTreeState* stateRef = nullptr;   // for the right-click menu
    juce::String paramID;
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

    // HARMONICS (row 3, left) — one Scala-style entry per drawbar ("3/2" = ratio,
    // "702.23 c" = cents; bare integer = n/1), rotated 90° to read along the
    // drawbar columns. A|C toggle below: A = AUTO (JI harmonic quantized to the
    // tuning; entry greyed out), C = CUSTOM (entry used exactly, un-quantized).
    // Error label: sounding pitch vs pure JI at the last-played note.
    juce::Label      footage[9];
    juce::Label      harmEntry[9];
    juce::TextButton harmAutoBtn[9], harmCustomBtn[9];
    juce::Label      ratioErr[9];

    void commitHarmonicEntry (int i);
    void refreshHarmonics();   // grey-out state, mode toggles, live error labels

    // TONE (row 3, right; left-aligned with CROSSTALK/PREAMP)
    LabelledKnob eqBass, eqBassSlope, eqTreble, eqTrebleSlope;
    juce::Label      waveCap;
    juce::TextButton sineBtn { "SINE" }, squareBtn { "SQUARE" }, triangleBtn { "TRIANGLE" };
    juce::TextButton resetBtn { "ALL AUTO" };   // flip every drawbar to AUTO (entries kept)

    // Right-click parameter menus on the combos / buttons / entries.
    juce::OwnedArray<juce::MouseListener> paramMenus;

public:
    void syncFromParams();   // WAVE 3-way + harmonics follow the params / last note

private:
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

    juce::Label hornMotorTitle, drumMotorTitle, micTitle, fATitle, fBTitle, dFTitle;

    LabelledKnob hornSlow, hornFast, hornAccel, hornDecel, hornBrake;
    LabelledKnob drumSlow, drumFast, drumAccel, drumDecel, drumBrake;
    // MIC & CABINET (incl. the horn level/leak mix)
    LabelledKnob micAngle, micDist, hornWidth, drumWidth, hornLevel, hornLeak;

    juce::Label    fACap, fBCap, dFCap;
    juce::ComboBox fAType, fBType, dFType;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fAAtt, fBAtt, dFAtt;
    LabelledKnob fAFreq, fAQ, fAGain;
    LabelledKnob fBFreq, fBQ, fBGain;
    LabelledKnob dFFreq, dFQ, dFGain;

    // Per-rotor 3-way speed switches, inline on each rotor's motor row.
    // CHORALE=1 STOP=0 TREMOLO=2. (BYPASS lives on the PLAY page's Leslie section.)
    juce::TextButton hornChorale { "CHORALE" }, hornStop { "STOP" }, hornTremolo { "TREMOLO" };
    juce::TextButton drumChorale { "CHORALE" }, drumStop { "STOP" }, drumTremolo { "TREMOLO" };

    void setSpeedParam (const char* paramID, float v);

    // Right-click parameter menus on the combos / speed switches.
    juce::OwnedArray<juce::MouseListener> paramMenus;

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
    bool keyPressed (const juce::KeyPress&) override;   // cmd+I: Melatonin inspector

private:
    TuneBfreeAudioProcessor& proc;
    TuneBfreeLookAndFeel laf;

#if TUNEBFREE_MELATONIN
    melatonin::Inspector inspector { *this, false };    // hidden until cmd+I
#endif

    juce::Label      titleLabel;
    juce::TextButton tuningBtn { "TUNING" };
    // CONTROL: MIDI CC + program-change management (the "preset" concept: a
    // program = default values for the CC controllers). Placeholder for now.
    juce::TextButton controlBtn { "CONTROL" };
    juce::TextButton panicBtn  { "!" };       // release all notes
    juce::TextButton volumeBtn;               // 🔊 — popup master volume slider

    // Page radio (header centre). PLAY is the default page.
    juce::TextButton playBtn { "PLAY" }, tinkerBtn { "TINKER" }, rotaryBtn { "ROTOR" };
    int currentPage = 0;                      // 0 = PLAY, 1 = TINKER, 2 = ROTOR
    void setPage (int page);

    DefaultPage defaultPage;
    TinkerPage  tinkerPage;
    RotaryPage  rotaryPage;

    // Tuning side panel: an editor-level overlay (independent of the page radio,
    // so it survives page switches). Covers the window's right column.
    TuningSidePanelContent tuningContent;

    // Right-click parameter menu on the header volume button.
    juce::OwnedArray<juce::MouseListener> paramMenus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneBfreeAudioProcessorEditor)
};
