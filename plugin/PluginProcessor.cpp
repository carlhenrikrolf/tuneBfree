#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cstring>
#include <cmath>

// ============================================================================
// Parameter layout
// ============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout TuneBfreeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Drawbars 0-8 (integer steps 0-8)
    static const float defaultDrawbars[9] = { 7.0f, 8.0f, 8.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 9; i++)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "drawbar" + juce::String(i), 1 },
            "Drawbar " + juce::String(i + 1),
            juce::NormalisableRange<float>(0.0f, 8.0f, 1.0f),
            defaultDrawbars[i]));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "vibrato", 1 }, "Vibrato", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "vibrato_type", 1 }, "Vibrato Type",
        juce::NormalisableRange<float>(0.0f, 5.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "drum", 1 }, "Drum",
        juce::NormalisableRange<float>(0.0f, 2.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "horn", 1 }, "Horn",
        juce::NormalisableRange<float>(0.0f, 2.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "overdrive", 1 }, "Overdrive", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "character", 1 }, "Character",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "reverb_mix", 1 }, "Reverb",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.1f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "percussion", 1 }, "Percussion", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "percussion_vol", 1 }, "Percussion Soft/Norm", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "percussion_dec", 1 }, "Percussion Fast/Slow", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "percussion_har", 1 }, "Percussion 2nd/3rd", false));

    // Drawbar harmonic ratios: top/bottom pairs for each of the 9 drawbars
    static const float defaultRatioTop[9] = { 1, 3, 1, 2, 3, 4, 5, 6, 8 };
    static const float defaultRatioBot[9] = { 2, 2, 1, 1, 1, 1, 1, 1, 1 };
    for (int i = 0; i < 9; i++) {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "ratio_top_" + juce::String(i), 1 },
            "Ratio Top " + juce::String(i),
            juce::NormalisableRange<float>(0.0f, 1000.0f), defaultRatioTop[i]));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "ratio_bot_" + juce::String(i), 1 },
            "Ratio Bottom " + juce::String(i),
            juce::NormalisableRange<float>(0.0f, 1000.0f), defaultRatioBot[i]));
    }

    // Expression / swell pedal (the Hammond expression pedal — a volume control).
    // Default 1.0 = full, matching setBfree's out-of-box swell level.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "expression", 1 }, "Expression",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    // --- Keyboard split (step 2). Default off = single (upper) manual, as before. ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "split_enable", 1 }, "Split", false));
    // Split point as a frequency (Hz). Default ~middle C.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "split_point", 1 }, "Split Point (Hz)",
        juce::NormalisableRange<float>(20.0f, 4000.0f, 0.0f, 0.3f), 261.63f));
    // Crossfade width in cents (0 = hard split).
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "split_width", 1 }, "Split Crossfade (cents)",
        juce::NormalisableRange<float>(0.0f, 1200.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "lower_vibrato", 1 }, "Lower Vibrato", false));

    // Lower-manual drawbars 0-8 (integer steps 0-8). Default: a mellow 16'/8' setting.
    static const float defaultLowerDrawbars[9] = { 8.0f, 0.0f, 8.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 9; i++)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "lower_drawbar" + juce::String(i), 1 },
            "Lower Drawbar " + juce::String(i + 1),
            juce::NormalisableRange<float>(0.0f, 8.0f, 1.0f),
            defaultLowerDrawbars[i]));

    return { params.begin(), params.end() };
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

TuneBfreeAudioProcessor::TuneBfreeAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "tuneBfree", createParameterLayout())
{
    // Map parameter IDs to indexed pointers for lock-free audio thread access
    for (int i = 0; i <= P_DRAWBAR_MAX; i++)
        paramPtrs[i] = apvts.getRawParameterValue("drawbar" + juce::String(i));

    paramPtrs[P_VIBRATO]        = apvts.getRawParameterValue("vibrato");
    paramPtrs[P_VIBRATO_TYPE]   = apvts.getRawParameterValue("vibrato_type");
    paramPtrs[P_DRUM]           = apvts.getRawParameterValue("drum");
    paramPtrs[P_HORN]           = apvts.getRawParameterValue("horn");
    paramPtrs[P_OVERDRIVE]      = apvts.getRawParameterValue("overdrive");
    paramPtrs[P_CHARACTER]      = apvts.getRawParameterValue("character");
    paramPtrs[P_REVERB]         = apvts.getRawParameterValue("reverb_mix");
    paramPtrs[P_PERCUSSION]     = apvts.getRawParameterValue("percussion");
    paramPtrs[P_PERCUSSION_VOL] = apvts.getRawParameterValue("percussion_vol");
    paramPtrs[P_PERCUSSION_DEC] = apvts.getRawParameterValue("percussion_dec");
    paramPtrs[P_PERCUSSION_HAR] = apvts.getRawParameterValue("percussion_har");

    for (int i = 0; i < 9; i++) {
        paramPtrs[P_RATIO_TOP_MIN + i] = apvts.getRawParameterValue("ratio_top_" + juce::String(i));
        paramPtrs[P_RATIO_BOT_MIN + i] = apvts.getRawParameterValue("ratio_bot_" + juce::String(i));
    }

    paramPtrs[P_EXPRESSION] = apvts.getRawParameterValue("expression");

    paramPtrs[P_SPLIT_ENABLE]  = apvts.getRawParameterValue("split_enable");
    paramPtrs[P_SPLIT_POINT]   = apvts.getRawParameterValue("split_point");
    paramPtrs[P_SPLIT_WIDTH]   = apvts.getRawParameterValue("split_width");
    paramPtrs[P_LOWER_VIBRATO] = apvts.getRawParameterValue("lower_vibrato");
    for (int i = 0; i < 9; i++)
        paramPtrs[P_LOWER_DRAWBAR_MIN + i] = apvts.getRawParameterValue("lower_drawbar" + juce::String(i));

    // All channels active by default: every incoming note plays, single-table tuning
    // for a non-multichannel master — i.e. the pre-multichannel behaviour.
    for (int ch = 0; ch < 16; ch++) channelActive[ch] = true;
}

TuneBfreeAudioProcessor::~TuneBfreeAudioProcessor()
{
    tearDownDSP();
}

// ============================================================================
// DSP lifecycle
// ============================================================================

void TuneBfreeAudioProcessor::initDSP(double sampleRate)
{
    currentSampleRate = sampleRate;
    boffset = BUFFER_SIZE_SAMPLES;

    // Plain 12-TET table for the STANDARD source ("Gear60 (~12edo)").
    for (int i = 0; i < NOF_FREQS; ++i)
        standardFrequencies[i] = 440.0 * std::pow(2.0, (i - 69) / 12.0);

    // Build targetRatio from current parameter values
    double targetRatio[NOF_DRAWBARS] = {};
    for (int i = 0; i < NOF_DRAWBARS; i++) {
        float top = paramPtrs[P_RATIO_TOP_MIN + i]->load();
        float bot = paramPtrs[P_RATIO_BOT_MIN + i]->load();
        targetRatio[i] = (bot > 0.0) ? (top / bot) : 1.0;
        previousRatio[i] = targetRatio[i];
    }

    synth = allocTonegen();
    initToneGenerator(synth, nullptr, sampleRate, targetRatio);
    init_vibrato(&synth->inst_vibrato, sampleRate);

    preampModule = (b_preamp*) allocPreamp();
    initPreamp(preampModule, nullptr, sampleRate);

    reverbModule = allocReverb();
    initReverb(reverbModule, nullptr, sampleRate);

    whirlModule = allocWhirl();
    initWhirl(whirlModule, nullptr, sampleRate);

    mtsClient = MTS_RegisterClient();
    memset(previousFrequency, 0, sizeof(previousFrequency));
    memset(soundingUpper, 0xFF, sizeof(soundingUpper));   // all -1: nothing sounding
    memset(soundingLower, 0xFF, sizeof(soundingLower));
    memset(keyRefCount, 0, sizeof(keyRefCount));
    activeNoteCount = 0;
    samplesSinceLastNote = 0;

    // Apply current parameter state to DSP
    for (int i = 0; i < P_COUNT; i++) {
        float val = paramPtrs[i]->load();
        cachedParams[i] = val;
        applyParam(i, val);
    }

    updateScalePeriod();

    // Start the worker that performs all later rebuilds off the audio thread.
    startRebuildThread();
}

void TuneBfreeAudioProcessor::tearDownDSP()
{
    // Stop the rebuild worker first so nothing is mid-build when we free the engine.
    stopRebuildThread();
    if (mtsClient)    { MTS_DeregisterClient(mtsClient); mtsClient = nullptr; }
    if (whirlModule)  { freeWhirl(whirlModule);  whirlModule  = nullptr; }
    if (reverbModule) { freeReverb(reverbModule); reverbModule = nullptr; }
    if (preampModule) { freePreamp(preampModule); preampModule = nullptr; }
    if (synth)        { freeToneGenerator(synth); synth        = nullptr; }
}

void TuneBfreeAudioProcessor::updateScalePeriod()
{
    int size; float period;
    inferScaleSize(synth->frequency, &size, &period, synth->gamutSize);
    inferredPeriod.store(period);
    inferredScaleSize.store(size);
}

// ----------------------------------------------------------------------------
// Async tonegen rebuild
//
// A rebuild frees and rebuilds all wavetables and re-runs the wheel-matching loop
// — milliseconds of work with mallocs, never safe on the audio thread. So:
//
//   audio thread:  requestRebuild()           sets a flag, wakes the worker
//   worker thread: performBackgroundRebuild()  builds a fresh tonegen, publishes it
//   audio thread:  applyPendingRebuild()       cheap pointer swap; retires the old one
//   worker thread:                             frees the retired engine (off-audio)
//
// While a rebuild is in flight the old engine keeps playing, so a tuning/ratio
// change no longer risks an xrun. The first build (initDSP) is still synchronous,
// but that runs on the message thread before playback starts.
// ----------------------------------------------------------------------------

void TuneBfreeAudioProcessor::startRebuildThread()
{
    if (! rebuildThread) {
        rebuildThread = std::make_unique<RebuildThread>(*this);
        rebuildThread->startThread();
    }
}

void TuneBfreeAudioProcessor::stopRebuildThread()
{
    if (rebuildThread) {
        rebuildThread->signalThreadShouldExit();
        rebuildThread->notify();
        // Generous timeout: a maxed-out gamut (2048 slots) can take ~1–2 s to build, and
        // the exit flag isn't checked inside performBackgroundRebuild — wait it out.
        rebuildThread->waitForThreadToExit(8000);
        rebuildThread.reset();
    }
    // Free anything that was built-but-never-swapped or retired-but-never-freed.
    if (auto* p = pendingSynth.exchange(nullptr)) freeToneGenerator(p);
    if (auto* r = retiredSynth.exchange(nullptr)) freeToneGenerator(r);
    rebuildRequested.store(false);
}

void TuneBfreeAudioProcessor::rebuildThreadLoop(juce::Thread& thread)
{
    while (! thread.threadShouldExit()) {
        thread.wait(-1.0);   // sleep until requestRebuild() / applyPendingRebuild() wakes us

        // Free the engine the audio thread retired on its last swap.
        if (auto* old = retiredSynth.exchange(nullptr))
            freeToneGenerator(old);

        if (thread.threadShouldExit())
            break;

        // Coalesce rapid requests: a single build picks up the latest parameters.
        if (rebuildRequested.exchange(false))
            performBackgroundRebuild();
    }
}

void TuneBfreeAudioProcessor::performBackgroundRebuild()
{
    b_tonegen* fresh = allocTonegen();

    double targetRatio[NOF_DRAWBARS] = {};
    for (int i = 0; i < NOF_DRAWBARS; i++) {
        float top = paramPtrs[P_RATIO_TOP_MIN + i]->load();
        float bot = paramPtrs[P_RATIO_BOT_MIN + i]->load();
        targetRatio[i] = (bot > 0.0) ? (top / bot) : 1.0;
    }

    // Pick the frequency table for the active tuning source. STANDARD uses plain 12-TET;
    // FILE builds the gamut from the per-channel .kbm grid; MTS/SYSEX from the MTS client.
    // (The FILE grid is written by the message thread on file load; a concurrent reload at
    // worst yields a transient table, corrected by the next rebuild — it cannot crash.)
    const int     src     = tuningSource.load();
    const double* freqSrc = nullptr;
    double gamutTable[NOF_FREQS];
    int    gamutSlot[16][128];
    int    gamutSz   = 128;   // slots / manual stride
    int    gamutNw   = 256;   // tonewheels to build
    bool   useGamut  = false;
    if (src == TS_STANDARD) {
        freqSrc = standardFrequencies;   // identity slotIndex from initToneGenerator
    }
    else if (src == TS_FILE && hasLocalTuning.load(std::memory_order_acquire)) {
        buildFileGamut(gamutTable, gamutSlot, gamutSz, gamutNw);
        freqSrc = gamutTable; useGamut = true;
    }
    else {   // TS_MTS / TS_SYSEX (or FILE with nothing loaded yet)
        buildMTSGamut(gamutTable, gamutSlot, gamutSz, gamutNw);
        freqSrc = gamutTable; useGamut = true;
    }
    // gamutSize / nofWheels must be passed in: applyManualDefaults (inside
    // initToneGenerator) wires gamutSize keys per manual and searches nofWheels wheels.
    // An empty gamut (size 0) falls back to the default 128/256 with the standard table.
    const int initGamut = (useGamut && gamutSz > 0) ? gamutSz : 128;
    const int initWheels = (useGamut && gamutSz > 0) ? gamutNw : 256;

    // Keyboard split: set on the engine BEFORE init so applyDefaultConfiguration wires
    // (and crossfade-scales) the lower manual. Read from the params.
    fresh->splitEnabled    = (paramPtrs[P_SPLIT_ENABLE]->load() > 0.5f) ? 1 : 0;
    fresh->splitPointHz    = (double) paramPtrs[P_SPLIT_POINT]->load();
    fresh->splitWidthCents = (double) paramPtrs[P_SPLIT_WIDTH]->load();

    initToneGenerator(fresh, nullptr, currentSampleRate, targetRatio, freqSrc, initGamut, initWheels);
    if (useGamut)
        memcpy(fresh->slotIndex, gamutSlot, sizeof(gamutSlot));   // gamutSize set via the init arg
    init_vibrato(&fresh->inst_vibrato, currentSampleRate);

    // Re-apply the tonegen-side parameters (the same set the old synchronous reinit did).
    for (int i = P_DRAWBAR_MIN; i <= P_DRAWBAR_MAX; i++)
        setDrawBar(fresh, i, (unsigned int) std::lround(paramPtrs[i]->load()));
    setVibratoUpper (fresh, (int) std::lround(paramPtrs[P_VIBRATO]->load()));
    setVibratoFromInt(fresh, (int) std::floor (paramPtrs[P_VIBRATO_TYPE]->load()));
    setPercussionEnabled(fresh, (int) std::lround(paramPtrs[P_PERCUSSION]->load()));
    // Lower manual (buses 9-17): drawbars + vibrato on/off. Harmless when split is off
    // (the lower manual isn't wired, so these have no audible effect).
    setVibratoLower(fresh, (int) std::lround(paramPtrs[P_LOWER_VIBRATO]->load()));
    for (int i = 0; i < 9; i++)
        setDrawBar(fresh, 9 + i, (unsigned int) std::lround(paramPtrs[P_LOWER_DRAWBAR_MIN + i]->load()));

    // Scale-period read-out for the UI (inferScaleSize is too heavy for the audio thread).
    int size; float period;
    inferScaleSize(fresh->frequency, &size, &period, fresh->gamutSize);
    inferredPeriod.store(period);
    inferredScaleSize.store(size);

    // Publish. If the audio thread never consumed a previous build, free it here
    // (on the worker) rather than leak it.
    if (auto* stale = pendingSynth.exchange(fresh, std::memory_order_release))
        freeToneGenerator(stale);
}

void TuneBfreeAudioProcessor::requestRebuild()
{
    rebuildRequested.store(true, std::memory_order_release);
    if (rebuildThread) rebuildThread->notify();
}

void TuneBfreeAudioProcessor::applyPendingRebuild()
{
    b_tonegen* fresh = pendingSynth.exchange(nullptr, std::memory_order_acquire);
    if (fresh == nullptr)
        return;

    // Carry over live state the worker couldn't know about: the swell-pedal gain
    // (expression knob / CC 7 / CC 11) and the vibrato routing.
    fresh->swellPedalGain = synth->swellPedalGain;
    fresh->newRouting     = synth->newRouting;

    b_tonegen* old = synth;
    synth = fresh;

    // The fresh engine has no sounding notes; reset the bookkeeping and drop the
    // per-(channel,note) key tracking so a later note-off can't release a key on the
    // new engine that was never started.
    activeNoteCount = 0;
    memset(soundingUpper, 0xFF, sizeof(soundingUpper));   // all -1
    memset(soundingLower, 0xFF, sizeof(soundingLower));
    memset(keyRefCount, 0, sizeof(keyRefCount));

    // Retire the old engine for the worker to free (never free on the audio thread).
    // Only one retire is ever outstanding: a new build cannot be published until the
    // previous one is consumed, so this store never clobbers a live pointer.
    retiredSynth.store(old, std::memory_order_release);
    if (rebuildThread) rebuildThread->notify();
}

// Release every sounding note. Runs on the audio thread (panic button via panicRequested,
// or MIDI CC 120 "all sound off" / CC 123 "all notes off").
void TuneBfreeAudioProcessor::allNotesOff()
{
    if (synth == nullptr) return;
    // Release every engine key across both manuals (no-op for keys that aren't active).
    for (int k = 0; k < 2 * synth->gamutSize && k < MAX_KEYS; ++k)
        oscKeyOff(synth, (short) k, (short) k);
    memset(soundingUpper, 0xFF, sizeof(soundingUpper));   // all -1
    memset(soundingLower, 0xFF, sizeof(soundingLower));
    memset(keyRefCount, 0, sizeof(keyRefCount));
    activeNoteCount = 0;
}

// Size the tonewheel pool to the gamut (Solution B): enough wheels to cover the gamut
// fundamentals plus ~8x the top pitch (the highest drawbar harmonic), capped at the
// compile-time maximum. A simple scale therefore builds far fewer wavetables.
static int computeNofWheels(const double* freqTable, int gamutSize)
{
    if (gamutSize <= 0) return 256;
    const double top = freqTable[gamutSize - 1] * 8.5;
    int nw = gamutSize;
    while (nw < NOF_WHEELS && freqTable[nw - 1] < top) ++nw;
    return nw;   // in [gamutSize, NOF_WHEELS]
}

// Build the merged multichannel gamut for the MTS/SYSEX source. Queries MTS over the
// active channels into a 16×128 grid, collapses it to the distinct sounding pitches
// (buildGamut), and produces the tonewheel frequency table + the (channel,note)->slot
// map + the gamut size + the wheel count. A non-multichannel master returns the same
// table on every channel, so this reduces to the single-table identity case. Runs on
// the worker (its own MTS client). The gamut is capped at MAX_GAMUT slots per manual.
void TuneBfreeAudioProcessor::buildMTSGamut(double freqTable[], int slotIndexOut[16][128],
                                            int& gamutSizeOut, int& nofWheelsOut)
{
    MTSClient* c = MTS_RegisterClient();

    // The popup selection always gates sounding (deselected channels are silent). OMNI
    // ON queries the "unspecified" channel (-1) for every selected channel; OMNI OFF
    // queries each selected channel's own number (no fallback — MTS can't report which
    // channels are "specified", and a plain master returns the same table for all).
    const bool omni = omniMode.load(std::memory_order_acquire);
    static double grid[16][128];          // static: keep these ~40 KB off the worker stack
    static bool   noteMapped[16][128];
    for (int ch = 0; ch < 16; ++ch) {
        const char qch = omni ? (char) -1 : (char) ch;
        for (int n = 0; n < 128; ++n) {
            grid[ch][n]       = MTS_NoteToFrequency(c, (char) n, qch);
            noteMapped[ch][n] = ! MTS_ShouldFilterNote(c, (char) n, qch);
        }
    }
    MTS_DeregisterClient(c);

    static double gamut[16 * 128];
    static int    slot[16][128];
    int size = buildGamut(grid, channelActive, noteMapped, gamut, slot);

    if (size > MAX_GAMUT) size = MAX_GAMUT;   // cap to the per-manual key count

    for (int ch = 0; ch < 16; ++ch)
        for (int n = 0; n < 128; ++n)
            slotIndexOut[ch][n] = (slot[ch][n] >= 0 && slot[ch][n] < size) ? slot[ch][n] : -1;

    if (size == 0) {
        // No active channels / everything filtered: keep a valid (12-TET) wheel table
        // so the engine stays well-formed; all notes route to -1 (silent).
        for (int i = 0; i < NOF_FREQS; ++i) freqTable[i] = standardFrequencies[i];
        gamutSizeOut = 0;
        nofWheelsOut = 256;
        return;
    }

    for (int i = 0; i < size; ++i) freqTable[i] = gamut[i];
    extendFrequencies(freqTable, NOF_FREQS, size);   // higher tonewheels for harmonics
    gamutSizeOut = size;
    nofWheelsOut = computeNofWheels(freqTable, size);
}

// Build the merged gamut for the FILE source. The popup's channelActive mask selects
// which channels sound (deselected → silent → fewer wheels). OMNI ON collapses every
// selected channel onto the generic mapping; OMNI OFF gives each its own _i.kbm (or the
// generic fallback). Unmapped ("x") keys route to slot -1. Capped at MAX_GAMUT slots.
void TuneBfreeAudioProcessor::buildFileGamut(double freqTable[], int slotIndexOut[16][128],
                                             int& gamutSizeOut, int& nofWheelsOut)
{
    const bool omni = omniMode.load(std::memory_order_acquire);

    static double grid[16][128];
    static bool   mask[16][128];
    for (int ch = 0; ch < 16; ++ch)
        for (int n = 0; n < 128; ++n) {
            grid[ch][n] = omni ? genericFreqGrid[n]   : localFreqGrid[ch][n];
            mask[ch][n] = omni ? genericMappedGrid[n] : localMappedGrid[ch][n];
        }

    static double gamut[16 * 128];
    static int    slot[16][128];
    int size = buildGamut(grid, channelActive, mask, gamut, slot);

    if (size > MAX_GAMUT) size = MAX_GAMUT;

    for (int ch = 0; ch < 16; ++ch)
        for (int n = 0; n < 128; ++n)
            slotIndexOut[ch][n] = (slot[ch][n] >= 0 && slot[ch][n] < size) ? slot[ch][n] : -1;

    if (size == 0) {
        for (int i = 0; i < NOF_FREQS; ++i) freqTable[i] = standardFrequencies[i];
        gamutSizeOut = 0;
        nofWheelsOut = 256;
        return;
    }

    for (int i = 0; i < size; ++i) freqTable[i] = gamut[i];
    extendFrequencies(freqTable, NOF_FREQS, size);
    gamutSizeOut = size;
    nofWheelsOut = computeNofWheels(freqTable, size);
}

// ============================================================================
// Parameter application
// ============================================================================

void TuneBfreeAudioProcessor::applyParam(int index, float value)
{
    if (synth == nullptr) return;

    if (index >= P_DRAWBAR_MIN && index <= P_DRAWBAR_MAX) {
        setDrawBar(synth, index, (unsigned int) std::lround(value));
    }
    else if (index == P_VIBRATO) {
        setVibratoUpper(synth, (int) std::lround(value));
    }
    else if (index == P_VIBRATO_TYPE) {
        setVibratoFromInt(synth, (int) std::floor(value));
    }
    else if (index == P_DRUM || index == P_HORN) {
        if (whirlModule) {
            int drumSetting = (int) std::floor(cachedParams[P_DRUM]);
            int hornSetting = (int) std::floor(cachedParams[P_HORN]);
            useRevOption(whirlModule, drumSetting + 3 * hornSetting, 2);
        }
    }
    else if (index == P_OVERDRIVE) {
        if (preampModule) preampModule->isClean = (int) std::lround(1.0f - value);
    }
    else if (index == P_CHARACTER) {
        if (preampModule) fsetCharacter(preampModule, value);
    }
    else if (index == P_REVERB) {
        if (reverbModule) setReverbMix(reverbModule, (double) value);
    }
    else if (index == P_PERCUSSION) {
        setPercussionEnabled(synth, (int) std::lround(value));
    }
    else if (index == P_PERCUSSION_VOL) {
        setPercussionVolume(synth, 1 - (int) std::lround(value));
    }
    else if (index == P_PERCUSSION_DEC) {
        setPercussionFast(synth, (int) std::lround(value));
    }
    else if (index == P_PERCUSSION_HAR) {
        setPercussionFirst(synth, (int) std::lround(value));
    }
    else if (index == P_EXPRESSION) {
        // Hammond expression/swell pedal: a pure output gain (0..outputLevelTrim).
        synth->swellPedalGain = value * (float) synth->outputLevelTrim;
    }
    else if (index == P_LOWER_VIBRATO) {
        setVibratoLower(synth, (int) std::lround(value));
    }
    else if (index >= P_LOWER_DRAWBAR_MIN && index <= P_LOWER_DRAWBAR_MAX) {
        // Lower manual = tonegen buses 9-17.
        setDrawBar(synth, 9 + (index - P_LOWER_DRAWBAR_MIN), (unsigned int) std::lround(value));
    }
    // Ratio and split enable/point/width params trigger a rebuild (from processBlock), not here
}

// ============================================================================
// prepareToPlay / releaseResources
// ============================================================================

void TuneBfreeAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    tearDownDSP();
    initDSP(sampleRate);
}

void TuneBfreeAudioProcessor::releaseResources()
{
    tearDownDSP();
}

bool TuneBfreeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::disabled();
}

// ============================================================================
// Audio rendering
// ============================================================================

void TuneBfreeAudioProcessor::renderAudio(float* outL, float* outR, int numSamples)
{
    int written = 0;
    while (written < numSamples) {
        if (boffset >= BUFFER_SIZE_SAMPLES) {
            boffset = 0;
            oscGenerateFragment(synth, bufA, BUFFER_SIZE_SAMPLES);
            preamp(preampModule, bufA, bufB, BUFFER_SIZE_SAMPLES);
            reverbModule->reverb(bufB, bufC, BUFFER_SIZE_SAMPLES);
            whirlProc3(whirlModule, bufC,
                       bufL[0], bufL[1],
                       bufD[0], bufD[1],
                       BUFFER_SIZE_SAMPLES);
        }

        int nread = std::min(numSamples - written, BUFFER_SIZE_SAMPLES - boffset);
        std::memcpy(outL + written, bufL[0] + boffset, nread * sizeof(float));
        std::memcpy(outR + written, bufL[1] + boffset, nread * sizeof(float));
        written += nread;
        boffset += nread;
    }
}

// ============================================================================
// processBlock
// ============================================================================

void TuneBfreeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    if (synth == nullptr)
        return;

    // --- Swap in a finished rebuild, if the worker has one ready (cheap; runs every
    //     block, including the silent early-return path, so idle tuning changes land) ---
    applyPendingRebuild();

    // --- Panic (GUI button): release everything before processing this block ---
    if (panicRequested.exchange(false, std::memory_order_acquire))
        allNotesOff();

    // --- Detect and apply parameter changes ---
    // Ratio and split enable/point/width changes need a tonegen rebuild (they change the
    // wiring / baked crossfade gains); everything else applies live in applyParam.
    bool needParamRebuild = false;
    for (int i = 0; i < P_COUNT; i++) {
        float val = paramPtrs[i]->load();
        if (val != cachedParams[i]) {
            cachedParams[i] = val;
            applyParam(i, val);
            if ((i >= P_RATIO_TOP_MIN && i <= P_RATIO_BOT_MAX) ||
                i == P_SPLIT_ENABLE || i == P_SPLIT_POINT || i == P_SPLIT_WIDTH)
                needParamRebuild = true;
        }
    }

    // --- Detect MTS-ESP tuning changes across all 16 channels × 128 notes ---
    // MTS-ESP channel is 0-indexed (0 = MIDI ch 1). Per-channel tuning is used
    // when the tuning source assigns different scales to different channels.
    bool tuningChanged = false;
    if (mtsClient) {
        for (int ch = 0; ch < 16; ch++) {
            for (int note = 0; note < 128; note++) {
                double freq = MTS_NoteToFrequency(mtsClient, (char) note, (char) ch);
                if (freq != previousFrequency[ch][note]) {
                    previousFrequency[ch][note] = freq;
                    tuningChanged = true;
                }
            }
        }
    }

    // MTS-ESP frequency changes only matter when an MTS-based source is active.
    const int  src           = tuningSource.load();
    const bool sourceUsesMTS = (src == TS_MTS || src == TS_SYSEX);
    if (tuningChanged && sourceUsesMTS)
        lastTuningChangeMs.store(juce::Time::currentTimeMillis());

    // Always consume the reinit flag (set by file load or source change) so it can't
    // re-trigger; short-circuiting it inside the || would leave it stuck.
    const bool needReinit = localTuningNeedsReinit.exchange(false, std::memory_order_acquire);
    if ((tuningChanged && sourceUsesMTS) || needParamRebuild || needReinit)
        requestRebuild();

    // --- Silence detection: skip DSP when no notes have sounded for > tail length ---
    // This avoids setBfree's known issue of burning CPU even when silent.
    // Tail covers reverb + Leslie decay (~3 s is conservative).
    const int tailSamples = (int)(3.0 * currentSampleRate);
    if (activeNoteCount == 0 && samplesSinceLastNote > tailSamples && midiMessages.isEmpty()) {
        buffer.clear();
        return;
    }
    if (activeNoteCount == 0)
        samplesSinceLastNote += buffer.getNumSamples();

    // --- Sample-accurate MIDI + audio processing ---
    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getWritePointer(1);
    const int totalSamples = buffer.getNumSamples();
    int startSample = 0;

    for (const auto metadata : midiMessages) {
        const int eventPos = metadata.samplePosition;

        if (eventPos > startSample)
            renderAudio(outL + startSample, outR + startSample, eventPos - startSample);

        startSample = eventPos;

        const auto msg = metadata.getMessage();

        if (msg.isSysEx()) {
            // Forward raw sysex to MTS-ESP client. Handles all MTS tuning bulk-dump and
            // single-note retune formats, allowing tuning without an MTS-ESP master plug-in.
            MTS_ParseMIDIDataU(mtsClient, msg.getRawData(), msg.getRawDataSize());
            if (sourceUsesMTS) {
                lastTuningChangeMs.store(juce::Time::currentTimeMillis());
                requestRebuild();
            }
        }
        else if (msg.isController()) {
            // Expression pedal: CC 7 (volume) and CC 11 (expression) BOTH drive the
            // swell-pedal gain, as in setBfree. Sets the DSP directly; the GUI knob
            // doesn't follow an incoming pedal (a later refinement).
            const int cc = msg.getControllerNumber();
            if (cc == 7 || cc == 11)
                synth->swellPedalGain = (float) (synth->outputLevelTrim
                                                 * msg.getControllerValue() / 127.0);
            else if (cc == 120 || cc == 123)   // all sound off / all notes off
                allNotesOff();
        }
        else {
            const int  noteNumber = msg.getNoteNumber();
            const int  ch         = msg.getChannel() - 1;          // JUCE 1-16 → 0-15
            const char midiCh     = (char) ch;

            if (msg.isNoteOn()) {
                // MTS filtering stays live (always current). FILE bakes the .kbm's "x"
                // (unmapped) keys into slotIndex (per channel); STANDARD never filters.
                // OMNI queries the unspecified channel (-1), matching the gamut build.
                const char filtCh = omniMode.load(std::memory_order_acquire) ? (char) -1 : midiCh;
                const bool mtsFilter =
                    (src == TS_MTS && MTS_ShouldFilterNote(mtsClient, (char) noteNumber, filtCh));
                // Route (channel, note) to its gamut slot. -1 = silence (filtered,
                // unmapped, inactive channel, or a pitch beyond the gamut cap).
                const int slot = mtsFilter ? -1 : synth->slotIndex[ch][noteNumber];

                // Decide which manual(s) the note sounds on. Without the split it's the
                // upper manual (engine key == slot). With the split, the note's pitch is
                // crossfaded between the lower manual (key gamutSize+slot) and the upper
                // (key slot); the per-slot gains are already baked into the tapers, so
                // here we only choose which key(s) to press (skip a manual at ~zero gain).
                int keyUpper = -1, keyLower = -1;
                if (slot >= 0) {
                    if (synth->splitEnabled) {
                        double wl, wu;
                        splitCrossfade(synth->frequency[slot], synth->splitPointHz,
                                       synth->splitWidthCents, &wl, &wu);
                        if (wu > 1.0e-4) keyUpper = slot;
                        if (wl > 1.0e-4) keyLower = synth->gamutSize + slot;
                    } else {
                        keyUpper = slot;
                    }
                }

                // Reference-count each engine key: coincident pitches (across channels,
                // or a shared slot) collapse to one key, sounded by the first holder and
                // released by the last — avoids the retrigger click and the stuck/cut bugs.
                bool sounded = false;
                for (int key : { keyUpper, keyLower }) {
                    if (key < 0) continue;
                    if (keyRefCount[key]++ == 0)
                        oscKeyOn(synth, (short) key, (short) noteNumber);
                    sounded = true;
                }
                soundingUpper[ch][noteNumber] = keyUpper;
                soundingLower[ch][noteNumber] = keyLower;
                if (sounded) {
                    activeNoteCount++;
                    samplesSinceLastNote = 0;
                    const double pitch = synth->frequency[slot];
                    // Tuning-panel read-out: note number + the actual sounding frequency.
                    penultimateNoteOn.store(lastNoteOn.load());
                    lastNoteOn.store(noteNumber);
                    penultimateNoteFreq.store(lastNoteFreq.load());
                    lastNoteFreq.store(pitch);

                    // Split "learn": grow the session's pitch range and publish the split.
                    if (learnSplitActive.load(std::memory_order_acquire)) {
                        if (! learnSessionActive) { learnSessionActive = true; learnMinPitch = learnMaxPitch = pitch; }
                        else { learnMinPitch = std::min(learnMinPitch, pitch); learnMaxPitch = std::max(learnMaxPitch, pitch); }
                        if (learnMaxPitch > learnMinPitch) {
                            learnedSplitPoint.store(std::sqrt(learnMinPitch * learnMaxPitch));      // geometric centre
                            learnedSplitWidth.store(1200.0 * std::log2(learnMaxPitch / learnMinPitch));
                        } else {
                            learnedSplitPoint.store(learnMinPitch);   // single note → hard split
                            learnedSplitWidth.store(0.0);
                        }
                    }
                }
            } else if (msg.isNoteOff()) {
                // JUCE normalises velocity-0 note-on to noteOff, so all releases arrive here.
                // Release exactly the keys this (channel, note) started, on both manuals.
                bool released = false;
                for (int key : { soundingUpper[ch][noteNumber], soundingLower[ch][noteNumber] }) {
                    if (key < 0) continue;
                    if (--keyRefCount[key] <= 0) {
                        keyRefCount[key] = 0;
                        oscKeyOff(synth, (short) key, (short) noteNumber);
                    }
                    released = true;
                }
                if (released)
                    activeNoteCount = std::max(0, activeNoteCount - 1);
                soundingUpper[ch][noteNumber] = -1;
                soundingLower[ch][noteNumber] = -1;

                // End a split-learn session (and disarm) once every note is released.
                if (learnSessionActive && activeNoteCount == 0) {
                    learnSessionActive = false;
                    learnSplitActive.store(false, std::memory_order_release);
                }
            }
        }
    }

    if (startSample < totalSamples)
        renderAudio(outL + startSample, outR + startSample, totalSamples - startSample);
}

// ============================================================================
// State persistence
// ============================================================================

void TuneBfreeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Persist the channel selection (not APVTS parameters) as a child node.
    auto ch = state.getOrCreateChildWithName("channelConfig", nullptr);
    ch.setProperty("omni", (bool) omniMode.load(), nullptr);
    ch.setProperty("poly", polyMode, nullptr);
    for (int c = 0; c < 16; ++c)
        ch.setProperty("ch" + juce::String(c), channelActive[c], nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void TuneBfreeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml && xml->hasTagName(apvts.state.getType())) {
        auto tree = juce::ValueTree::fromXml(*xml);
        apvts.replaceState(tree);

        auto ch = tree.getChildWithName("channelConfig");
        if (ch.isValid()) {
            omniMode.store((bool) ch.getProperty("omni", false), std::memory_order_release);
            polyMode = (bool) ch.getProperty("poly", true);
            for (int c = 0; c < 16; ++c)
                channelActive[c] = (bool) ch.getProperty("ch" + juce::String(c), true);
            localTuningNeedsReinit.store(true, std::memory_order_release);   // rebuild with restored config
        }
    }
}

// ============================================================================
// Local tuning (.scl / .kbm)
// ============================================================================

void TuneBfreeAudioProcessor::loadSCLFile(const juce::File& file)
{
    try {
        localScale    = Tunings::readSCLFile(std::filesystem::path(file.getFullPathName().toStdString()));
        localSclName  = file.getFileName();
        // The .scl's own name line (top of the file), shown instead of the filename.
        localSclDescription = juce::String(localScale.description).trim();
        localTuningError = {};
        rebuildLocalTuning();
    } catch (const Tunings::TuningError& e) {
        localTuningError = juce::String(e.what());
    }
}

void TuneBfreeAudioProcessor::loadKBMFile(const juce::File& file)
{
    loadKBMFiles({ file });
}

// Parse a .kbm basename for a trailing "_<i>" channel suffix. Returns the 1-based MIDI
// channel (1..16), or -1 if there's no valid suffix (→ treated as a default mapping).
static int kbmChannelSuffix(const juce::String& baseName)
{
    const int us = baseName.lastIndexOfChar('_');
    if (us < 0 || us == baseName.length() - 1) return -1;
    const juce::String digits = baseName.substring(us + 1);
    if (! digits.containsOnly("0123456789")) return -1;
    const int n = digits.getIntValue();
    return (n >= 1 && n <= 16) ? n : -1;
}

void TuneBfreeAudioProcessor::loadKBMFiles(const juce::Array<juce::File>& files)
{
    if (files.isEmpty()) return;

    try {
        // Assignment: "*_i.kbm" → explicit mapping for channel i (last selected wins for
        // the same i); a file with no valid "_i" suffix → the generic mapping (last wins).
        for (int c = 0; c < 16; ++c) hasExplicitKBM[c] = false;
        hasGenericKBM = false;

        for (const auto& f : files) {
            auto km = Tunings::readKBMFile(std::filesystem::path(f.getFullPathName().toStdString()));
            const int ch = kbmChannelSuffix(f.getFileNameWithoutExtension());
            if (ch >= 1 && ch <= 16) { explicitKBM[ch - 1] = km; hasExplicitKBM[ch - 1] = true; }
            else                     { genericKBM = km;           hasGenericKBM = true; }
        }

        hasLocalKBM  = true;
        localKbmName = (files.size() == 1) ? files[0].getFileName()
                                           : juce::String(files.size()) + " maps";
        localTuningError = {};
        rebuildLocalTuning();
    } catch (const Tunings::TuningError& e) {
        localTuningError = juce::String(e.what());
    }
}

void TuneBfreeAudioProcessor::clearLocalTuning()
{
    localSclName  = {};
    localKbmName  = {};
    localSclDescription = {};
    hasLocalKBM   = false;
    hasGenericKBM = false;
    for (int c = 0; c < 16; ++c) hasExplicitKBM[c] = false;
    hasLocalTuning.store(false, std::memory_order_release);
    localTuningNeedsReinit.store(true, std::memory_order_release);
}

void TuneBfreeAudioProcessor::rebuildLocalTuning()
{
    if (localSclName.isEmpty()) return;

    try {
        // The generic mapping: the no-"_i" .kbm if loaded, else the bare .scl (default
        // linear mapping). Used under OMNI and as the fallback for unassigned channels.
        Tunings::Tuning generic = hasGenericKBM ? Tunings::Tuning(localScale, genericKBM)
                                                : Tunings::Tuning(localScale);
        for (int n = 0; n < 128; ++n) {
            genericFreqGrid[n]   = generic.frequencyForMidiNote(n);
            genericMappedGrid[n] = generic.isMidiNoteMapped(n);
        }

        // Every channel gets a valid tuning: its explicit _i.kbm if assigned, else generic.
        // (Which channels actually sound is the popup's channelActive mask, applied later.)
        for (int c = 0; c < 16; ++c) {
            if (hasExplicitKBM[c]) {
                Tunings::Tuning t(localScale, explicitKBM[c]);
                for (int n = 0; n < 128; ++n) {
                    localFreqGrid[c][n]   = t.frequencyForMidiNote(n);
                    localMappedGrid[c][n] = t.isMidiNoteMapped(n);
                }
            } else {
                for (int n = 0; n < 128; ++n) {
                    localFreqGrid[c][n]   = genericFreqGrid[n];
                    localMappedGrid[c][n] = genericMappedGrid[n];
                }
            }
        }

        localTuning = generic;   // panel display read-outs use the generic/base tuning

        hasLocalTuning.store(true, std::memory_order_release);
        localTuningNeedsReinit.store(true, std::memory_order_release);
        lastTuningChangeMs.store(juce::Time::currentTimeMillis());
    } catch (const Tunings::TuningError& e) {
        localTuningError = juce::String(e.what());
    }
}

double TuneBfreeAudioProcessor::getDisplayFrequency(int midiNote) const
{
    midiNote = juce::jlimit(0, 127, midiNote);
    const int src = tuningSource.load();
    if (src == TS_FILE && hasLocalTuning.load())
        return localTuning.frequencyForMidiNote(midiNote);
    if ((src == TS_MTS || src == TS_SYSEX) && mtsClient)
        return MTS_NoteToFrequency(mtsClient, (char) midiNote, 0);
    // STANDARD, or any source with nothing connected: plain 12-TET.
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

double TuneBfreeAudioProcessor::getDisplayCents(int midiNote) const
{
    midiNote = juce::jlimit(0, 127, midiNote);
    if (tuningSource.load() == TS_FILE && hasLocalTuning.load())
        return localTuning.retuningFromEqualInCentsForMidiNote(midiNote);
    double freq    = getDisplayFrequency(midiNote);
    double refFreq = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
    return (freq > 0 && refFreq > 0) ? 1200.0 * std::log2(freq / refFreq) : 0.0;
}

bool TuneBfreeAudioProcessor::isMidiNoteMapped(int midiNote) const
{
    midiNote = juce::jlimit(0, 127, midiNote);
    const int src = tuningSource.load();
    if (src == TS_FILE && hasLocalTuning.load())
        return localTuning.isMidiNoteMapped(midiNote);
    if (src == TS_MTS && mtsClient)            // filtering is MTS-ESP only (see processBlock)
        return !MTS_ShouldFilterNote(mtsClient, (char) midiNote, 0);
    return true;   // SYSEX / STANDARD map every note
}

void TuneBfreeAudioProcessor::setTuningSource(int sourceId)
{
    tuningSource.store(sourceId);
    // Force the audio thread to rebuild the tonewheel table from the new source.
    localTuningNeedsReinit.store(true, std::memory_order_release);
}

void TuneBfreeAudioProcessor::setChannelActive(int ch, bool active)
{
    if (ch < 0 || ch >= 16) return;
    channelActive[ch] = active;
    localTuningNeedsReinit.store(true, std::memory_order_release);   // rebuild the gamut
}

void TuneBfreeAudioProcessor::setOmni(bool on)
{
    omniMode.store(on, std::memory_order_release);
    localTuningNeedsReinit.store(true, std::memory_order_release);
}

// The period the .scl declares: its last tone (the repeat interval), in cents.
double TuneBfreeAudioProcessor::getLocalSclPeriodCents() const
{
    if (localSclName.isEmpty() || localScale.tones.empty())
        return -1.0;
    return localScale.tones.back().cents;
}

bool TuneBfreeAudioProcessor::isMTSConnected() const noexcept
{
    return mtsClient != nullptr && MTS_HasMaster(mtsClient);
}

juce::String TuneBfreeAudioProcessor::getMTSScaleName() const
{
    if (mtsClient == nullptr || !MTS_HasMaster(mtsClient))
        return {};
    return juce::String(MTS_GetScaleName(mtsClient));
}

// ============================================================================
// Editor
// ============================================================================

juce::AudioProcessorEditor* TuneBfreeAudioProcessor::createEditor()
{
    return new TuneBfreeAudioProcessorEditor(*this);
}

// ============================================================================
// Plugin entry point
// ============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TuneBfreeAudioProcessor();
}
