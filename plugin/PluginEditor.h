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
//  One vertical column: status info at the top, settings + file loaders in the
//  middle, last-played frequency / interval read-outs anchored to the bottom.
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

    // --- status (read-only info, shown in bordered boxes) ---
    juce::Label      scaleNameLabel;       // e.g. "13ED3" or em-dash when none
    juce::Label      statusArea;           // "CONNECTED" / "NO MASTER"
    juce::Label      periodLabel;          // scale period: inferred / MTS / aperiodic

    // --- settings ---
    juce::TextButton monoBtn { "MONO" }, polyBtn { "POLY" };   // 2-way switch
    juce::TextButton stdBtn  { "STD"  };
    juce::TextButton onlyAtNoteOnBtn { "NOTE ON ONLY" };

    // --- file loaders ---
    juce::TextButton loadSclBtn, loadKbmBtn;

    // --- read-outs (bordered boxes), anchored to the bottom ---
    juce::Label      lastHzLabel, currentHzLabel, centsLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;

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

private:
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
    juce::TextButton percSoftBtn { "SOFT" }, percHardBtn { "HARD" };
    juce::TextButton perc2ndBtn  { "2ND"  }, perc3rdBtn  { "3RD"  };

    // --- Timbrality (UI only) ---
    juce::TextButton upperBtn     { "UPPER"     };
    juce::TextButton lowerBtn     { "LOWER"     };
    juce::TextButton bitimbralBtn { "BITIMBRAL" };
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
    TuneBfreeLookAndFeel laf;

    juce::Label      titleLabel;
    juce::TextButton tuningBtn { "TUNING" };
    DefaultPage      defaultPage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneBfreeAudioProcessorEditor)
};
