#pragma once
#include <JuceHeader.h>

// DSP module types (C++ headers, no extern "C" needed)
#include "tonegen.h"
#include "overdrive.h"
#include "reverb.h"
#include "whirl.h"
#include "libMTSClient.h"

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

    // --- Private methods ---
    void initDSP(double sampleRate);
    void tearDownDSP();
    void reinitToneGen();
    void applyParam(int index, float value);
    void renderAudio(float* outL, float* outR, int numSamples);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuneBfreeAudioProcessor)
};
