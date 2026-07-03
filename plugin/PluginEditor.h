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
//  TuneBfreeAudioProcessorEditor — top-level window: amber header + DefaultPage.
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
    DefaultPage      defaultPage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneBfreeAudioProcessorEditor)
};
