#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    activeNoteCount = 0;
    samplesSinceLastNote = 0;

    // Apply current parameter state to DSP
    for (int i = 0; i < P_COUNT; i++) {
        float val = paramPtrs[i]->load();
        cachedParams[i] = val;
        applyParam(i, val);
    }

    updateScalePeriod();
}

void TuneBfreeAudioProcessor::tearDownDSP()
{
    if (mtsClient)    { MTS_DeregisterClient(mtsClient); mtsClient = nullptr; }
    if (whirlModule)  { freeWhirl(whirlModule);  whirlModule  = nullptr; }
    if (reverbModule) { freeReverb(reverbModule); reverbModule = nullptr; }
    if (preampModule) { freePreamp(preampModule); preampModule = nullptr; }
    if (synth)        { freeToneGenerator(synth); synth        = nullptr; }
}

void TuneBfreeAudioProcessor::updateScalePeriod()
{
    int size; float period;
    inferScaleSize(synth->frequency, &size, &period);
    inferredPeriod.store(period);
    inferredScaleSize.store(size);
}

void TuneBfreeAudioProcessor::reinitToneGen()
{
    // Preserve routing state across reinit
    unsigned int savedRouting = synth->newRouting;

    freeToneGenerator(synth);
    synth = allocTonegen();

    double targetRatio[NOF_DRAWBARS] = {};
    for (int i = 0; i < NOF_DRAWBARS; i++) {
        float top = paramPtrs[P_RATIO_TOP_MIN + i]->load();
        float bot = paramPtrs[P_RATIO_BOT_MIN + i]->load();
        targetRatio[i] = (bot > 0.0) ? (top / bot) : 1.0;
        previousRatio[i] = targetRatio[i];
    }

    const double* freqSrc = hasLocalTuning.load() ? localFrequencies : nullptr;
    initToneGenerator(synth, nullptr, currentSampleRate, targetRatio, freqSrc);
    init_vibrato(&synth->inst_vibrato, currentSampleRate);

    // Re-apply tonegen parameters
    for (int i = P_DRAWBAR_MIN; i <= P_DRAWBAR_MAX; i++)
        applyParam(i, cachedParams[i]);
    applyParam(P_VIBRATO,      cachedParams[P_VIBRATO]);
    applyParam(P_VIBRATO_TYPE, cachedParams[P_VIBRATO_TYPE]);
    applyParam(P_PERCUSSION,   cachedParams[P_PERCUSSION]);

    synth->newRouting = savedRouting;

    // The tonegen is rebuilt with no active notes. Reset tracking state so that
    // silence detection and note-off bookkeeping are consistent with the new tonegen.
    activeNoteCount = 0;
    memset(filteredNotes, 0, sizeof(filteredNotes));

    updateScalePeriod();
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
    // Ratio params are handled via reinitToneGen(), not here
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

    // --- Detect and apply parameter changes ---
    bool ratioChanged = false;
    for (int i = 0; i < P_COUNT; i++) {
        float val = paramPtrs[i]->load();
        if (val != cachedParams[i]) {
            cachedParams[i] = val;
            applyParam(i, val);
            if (i >= P_RATIO_TOP_MIN && i <= P_RATIO_BOT_MAX)
                ratioChanged = true;
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

    if (tuningChanged || ratioChanged || localTuningNeedsReinit.exchange(false, std::memory_order_acquire))
        reinitToneGen();

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
            reinitToneGen();
        }
        else {
            const int  noteNumber = msg.getNoteNumber();
            const char midiCh     = (char)(msg.getChannel() - 1); // JUCE 1-16 → MTS-ESP 0-15

            if (msg.isNoteOn()) {
                if (MTS_ShouldFilterNote(mtsClient, (char) noteNumber, midiCh)) {
                    filteredNotes[noteNumber] = true;
                } else {
                    filteredNotes[noteNumber] = false;
                    oscKeyOn(synth, (short) noteNumber, (short) noteNumber);
                    activeNoteCount++;
                    samplesSinceLastNote = 0;
                }
            } else if (msg.isNoteOff()) {
                // JUCE normalises velocity-0 note-on to noteOff, so all releases arrive here.
                // Skip oscKeyOff for notes that were filtered at note-on time.
                if (!filteredNotes[noteNumber]) {
                    oscKeyOff(synth, (short) noteNumber, (short) noteNumber);
                    activeNoteCount = std::max(0, activeNoteCount - 1);
                }
                filteredNotes[noteNumber] = false;
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
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void TuneBfreeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// ============================================================================
// Local tuning (.scl / .kbm)
// ============================================================================

void TuneBfreeAudioProcessor::loadSCLFile(const juce::File& file)
{
    try {
        localScale    = Tunings::readSCLFile(std::filesystem::path(file.getFullPathName().toStdString()));
        localSclName  = file.getFileName();
        localTuningError = {};
        rebuildLocalTuning();
    } catch (const Tunings::TuningError& e) {
        localTuningError = juce::String(e.what());
    }
}

void TuneBfreeAudioProcessor::loadKBMFile(const juce::File& file)
{
    try {
        localKBM      = Tunings::readKBMFile(std::filesystem::path(file.getFullPathName().toStdString()));
        hasLocalKBM   = true;
        localKbmName  = file.getFileName();
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
    hasLocalKBM   = false;
    hasLocalTuning.store(false, std::memory_order_release);
    localTuningNeedsReinit.store(true, std::memory_order_release);
}

void TuneBfreeAudioProcessor::rebuildLocalTuning()
{
    if (localSclName.isEmpty()) return;

    try {
        localTuning = hasLocalKBM ? Tunings::Tuning(localScale, localKBM)
                                  : Tunings::Tuning(localScale);

        for (int i = 0; i < 128; ++i)
            localFrequencies[i] = localTuning.frequencyForMidiNote(i);

        extendFrequencies(localFrequencies, NOF_FREQS);

        hasLocalTuning.store(true, std::memory_order_release);
        localTuningNeedsReinit.store(true, std::memory_order_release);
    } catch (const Tunings::TuningError& e) {
        localTuningError = juce::String(e.what());
    }
}

double TuneBfreeAudioProcessor::getDisplayFrequency(int midiNote) const
{
    midiNote = juce::jlimit(0, 127, midiNote);
    if (hasLocalTuning.load())
        return localTuning.frequencyForMidiNote(midiNote);
    if (mtsClient)
        return MTS_NoteToFrequency(mtsClient, (char) midiNote, 0);
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

double TuneBfreeAudioProcessor::getDisplayCents(int midiNote) const
{
    midiNote = juce::jlimit(0, 127, midiNote);
    if (hasLocalTuning.load())
        return localTuning.retuningFromEqualInCentsForMidiNote(midiNote);
    double freq    = getDisplayFrequency(midiNote);
    double refFreq = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
    return (freq > 0 && refFreq > 0) ? 1200.0 * std::log2(freq / refFreq) : 0.0;
}

bool TuneBfreeAudioProcessor::isMidiNoteMapped(int midiNote) const
{
    midiNote = juce::jlimit(0, 127, midiNote);
    if (hasLocalTuning.load())
        return localTuning.isMidiNoteMapped(midiNote);
    if (mtsClient)
        return !MTS_ShouldFilterNote(mtsClient, (char) midiNote, 0);
    return true;
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
