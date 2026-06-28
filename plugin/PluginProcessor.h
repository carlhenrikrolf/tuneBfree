#pragma once
#include <JuceHeader.h>

// DSP module types (C++ headers, no extern "C" needed)
#include "tonegen.h"
#include "overdrive.h"
#include "reverb.h"
#include "whirl.h"
#include "libMTSClient.h"
#include "tuning.h"
#include "Tunings.h"
#include <filesystem>

// Parameter indices — match the CLAP implementation in src/clap.cpp
#define P_DRAWBAR_MIN     0
#define P_DRAWBAR_MAX     8
#define P_VIBRATO         9
#define P_VIBRATO_TYPE    10
#define P_DRUM            11
#define P_HORN            12
#define P_OVERDRIVE       13
#define P_CHARACTER       14
#define P_REVERB          15
#define P_PERCUSSION      16
#define P_PERCUSSION_VOL  17
#define P_PERCUSSION_DEC  18
#define P_PERCUSSION_HAR  19
#define P_RATIO_TOP_MIN   20
#define P_RATIO_TOP_MAX   28
#define P_RATIO_BOT_MIN   29
#define P_RATIO_BOT_MAX   37
#define P_COUNT           38

class TuneBfreeAudioProcessor : public juce::AudioProcessor
{
public:
    TuneBfreeAudioProcessor();
    ~TuneBfreeAudioProcessor() override;

    // --- AudioProcessor interface ---

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "tuneBfree"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Scale period inferred from the current tuning table.
    // Period > 0 means inference succeeded; -1 means it could not be determined.
    // ScaleSize is the number of notes in one period (e.g. 12 for 12-TET).
    float getInferredPeriod()    const noexcept { return inferredPeriod.load(); }
    int   getInferredScaleSize() const noexcept { return inferredScaleSize.load(); }

    // Local tuning (.scl / .kbm) — called from the message thread (UI).
    // The audio thread picks up the change on the next processBlock.
    void loadSCLFile(const juce::File& file);
    void loadKBMFile(const juce::File& file);
    void clearLocalTuning();

    bool           getHasLocalTuning()  const noexcept { return hasLocalTuning.load(); }
    juce::String   getLocalSclName()    const { return localSclName; }
    juce::String   getLocalKbmName()    const { return localKbmName; }
    juce::String   getLocalTuningError() const { return localTuningError; }

    // Per-note display frequency / cents — safe to call from any thread.
    // Falls back to 12-TET if no tuning source is active.
    double getDisplayFrequency(int midiNote) const;
    double getDisplayCents(int midiNote) const;
    bool   isMidiNoteMapped(int midiNote) const;

    bool         isMTSConnected()  const noexcept;
    juce::String getMTSScaleName() const;

    // --- Tuning-panel telemetry (read-only display; safe from the message thread) ---
    // Last two note-on MIDI notes, for the panel's frequency / interval read-out.
    // -1 means nothing has played yet.
    int getLastNoteOn()        const noexcept { return lastNoteOn.load(); }
    int getPenultimateNoteOn() const noexcept { return penultimateNoteOn.load(); }

    // Wall-clock time (ms since epoch) of the last tuning change (file load, sysex,
    // or MTS frequency change). 0 if tuning has never changed. For a live MTS master
    // the panel shows the current time instead, since the master is queried every block.
    juce::int64 getLastTuningChangeMs() const noexcept { return lastTuningChangeMs.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    // --- DSP modules ---
    b_tonegen* synth       = nullptr;
    b_preamp*  preampModule = nullptr;  // named to avoid shadowing the preamp() function
    b_reverb*  reverbModule = nullptr;
    b_whirl*   whirlModule  = nullptr;
    MTSClient* mtsClient   = nullptr;

    // Internal fixed-size processing buffers (DSP works in 128-sample chunks)
    float bufA[BUFFER_SIZE_SAMPLES] = {};
    float bufB[BUFFER_SIZE_SAMPLES] = {};
    float bufC[BUFFER_SIZE_SAMPLES] = {};
    float bufD[2][BUFFER_SIZE_SAMPLES] = {};  // drum, tmp
    float bufL[2][BUFFER_SIZE_SAMPLES] = {};  // leslie L/R output
    int boffset = BUFFER_SIZE_SAMPLES;        // position within current internal chunk

    double currentSampleRate = 44100.0;

    // MTS-ESP tuning change detection: 16 MIDI channels × 128 notes
    double previousFrequency[16][128] = {};
    double previousRatio[NOF_DRAWBARS] = {};

    // Cached parameter values for detecting changes on the audio thread
    float cachedParams[P_COUNT] = {};

    // Lock-free pointers to APVTS atomics — set in prepareToPlay, read in processBlock
    std::atomic<float>* paramPtrs[P_COUNT] = {};

    // Silence detection: skip DSP when no notes have sounded for > reverb tail length
    int  activeNoteCount      = 0;
    int  samplesSinceLastNote = 0;

    // Inferred scale properties from the current MTS-ESP frequency table.
    // Written on the audio thread after every tonegen init; read by the UI.
    std::atomic<float> inferredPeriod{2.0f};
    std::atomic<int>   inferredScaleSize{12};

    // Tuning-panel telemetry: written on the audio thread, read by the UI.
    std::atomic<int>         lastNoteOn{-1};
    std::atomic<int>         penultimateNoteOn{-1};
    std::atomic<juce::int64> lastTuningChangeMs{0};

    // Per-note filter state: true when a note-on was suppressed by MTS_ShouldFilterNote.
    // Prevents the matching note-off from calling oscKeyOff on a note never started.
    bool filteredNotes[128] = {};

    // Local tuning (.scl / .kbm) loaded from file.
    // Written on the message thread; frequency array read on audio thread after flag is set.
    Tunings::Scale          localScale;
    Tunings::KeyboardMapping localKBM;
    Tunings::Tuning          localTuning;
    double                   localFrequencies[NOF_FREQS] = {};
    std::atomic<bool>        hasLocalTuning{false};
    std::atomic<bool>        localTuningNeedsReinit{false};
    bool                     hasLocalKBM = false;
    juce::String             localSclName;
    juce::String             localKbmName;
    juce::String             localTuningError;  // set if last load failed

    void rebuildLocalTuning();

    // --- Private methods ---
    void initDSP(double sampleRate);
    void tearDownDSP();
    void reinitToneGen();
    void updateScalePeriod();
    void applyParam(int index, float value);
    void renderAudio(float* outL, float* outR, int numSamples);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuneBfreeAudioProcessor)
};
