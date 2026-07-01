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
#define P_EXPRESSION      38
#define P_COUNT           39

// Tuning source ids — match the encoding ComboBox item ids in PluginEditor.
// (MPE = 4 and MIDI 2.0 = 5 are shown disabled and not handled here.)
enum TuningSourceId { TS_MTS = 1, TS_SYSEX = 2, TS_FILE = 3, TS_STANDARD = 6 };

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
    // Multiple .kbm files → per-channel mappings of the one .scl. Sorted by filename
    // (natural order) and assigned to MIDI channels 1..N (a generalisation of the
    // _1.kbm…_16.kbm convention). One file = single-channel (applies to all channels).
    void loadKBMFiles(const juce::Array<juce::File>& files);
    void clearLocalTuning();

    bool           getHasLocalTuning()  const noexcept { return hasLocalTuning.load(); }
    juce::String   getLocalSclName()    const { return localSclName; }
    juce::String   getLocalKbmName()    const { return localKbmName; }
    juce::String   getLocalSclDescription() const { return localSclDescription; }
    juce::String   getLocalTuningError() const { return localTuningError; }
    // Period the .scl file itself declares (its last tone), in cents; -1 if none loaded.
    double         getLocalSclPeriodCents() const;

    // Which encoding feeds the engine (TuningSourceId). Files apply only under TS_FILE.
    void setTuningSource(int sourceId);
    int  getTuningSource() const noexcept { return tuningSource.load(); }

    // --- Channel selection (CHANNELS popup) — message thread; triggers a rebuild ---
    // channelActive[ch] = whether MIDI channel ch (0..15) contributes to the gamut.
    // OMNI: merge everything to the MTS-ESP "unspecified" channel (-1) — applies to the
    // MTS/SYSEX source; FILE keeps its per-channel .kbm mapping. POLY is a UI-only mode
    // flag (multi- vs single-select), persisted so the panel can restore it.
    void setChannelActive(int ch, bool active);
    bool getChannelActive(int ch) const noexcept { return ch >= 0 && ch < 16 && channelActive[ch]; }
    void setOmni(bool on);
    bool getOmni()  const noexcept { return omniMode.load(); }
    void setPoly(bool on) noexcept { polyMode = on; }
    bool getPoly()  const noexcept { return polyMode; }

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
    // The actual sounding frequency (Hz) of those two notes — channel-aware, so two
    // manuals at different pitches read out differently. 0 = nothing played yet.
    double getLastNoteFreq()        const noexcept { return lastNoteFreq.load(); }
    double getPenultimateNoteFreq() const noexcept { return penultimateNoteFreq.load(); }

    // Panic: release every sounding note (GUI button / MIDI CC 120 / 123). Thread-safe;
    // the audio thread performs the release on the next block.
    void triggerPanic() noexcept { panicRequested.store(true, std::memory_order_release); }

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

    // --- Async tonegen rebuild ---
    // Rebuilding the tonegen (mallocs + wavetable building + the
    // O(keys × buses × wheels) wheel-matching loop) is milliseconds of work and is
    // never safe on the audio thread. It runs on rebuildThread instead; the audio
    // thread only requests a rebuild and later swaps in the finished engine.
    struct RebuildThread : public juce::Thread
    {
        // 4 MB stack: compilePlayMatrix puts ~300 KB of NOF_WHEELS-sized arrays
        // (cpmGain etc.) on the stack at the 2048-wheel maximum — generous headroom.
        explicit RebuildThread(TuneBfreeAudioProcessor& o)
            : juce::Thread("tuneBfree rebuild", 4 * 1024 * 1024), owner(o) {}
        void run() override { owner.rebuildThreadLoop(*this); }
        TuneBfreeAudioProcessor& owner;
    };
    std::unique_ptr<RebuildThread> rebuildThread;
    std::atomic<bool>       rebuildRequested{ false }; // audio → worker: please rebuild
    std::atomic<b_tonegen*> pendingSynth{ nullptr };   // worker → audio: built, ready to swap
    std::atomic<b_tonegen*> retiredSynth{ nullptr };   // audio → worker: old engine to free

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
    std::atomic<double>      lastNoteFreq{0.0};         // actual sounding Hz of the last note
    std::atomic<double>      penultimateNoteFreq{0.0};  // ...and the one before
    std::atomic<juce::int64> lastTuningChangeMs{0};

    // Per-(channel, note) routing state: the gamut slot a held note is sounding on,
    // or -1 if it was silenced (filtered / unmapped / inactive channel) or not held.
    // Replaces the old channel-agnostic filteredNotes[] and lets note-off release the
    // exact slot the note-on started (the slot can differ per channel under multichannel).
    int soundingSlot[16][128];

    // Per-slot reference count: how many held (channel, note) keys share each gamut slot.
    // Coincident pitches across channels de-duplicate to one slot, so the slot must stay
    // sounding until the LAST key on it is released (oscKeyOn on 0→1, oscKeyOff on 1→0).
    int slotRefCount[NOF_FREQS] = {};

    // Panic: set by triggerPanic()/CC 120/123, consumed on the audio thread to release all.
    std::atomic<bool> panicRequested{ false };

    // Which MIDI channels contribute to the merged tuning gamut (CHANNELS selection).
    // Default: all active — reproduces the pre-multichannel behaviour (every channel
    // plays, single-table tuning) for a non-multichannel MTS master.
    bool channelActive[16];
    std::atomic<bool> omniMode{ false };   // merge to the MTS unspecified channel (-1)
    bool              polyMode = true;     // UI mode: multi-select (true) vs single (false)

    // Local tuning (.scl / .kbm) loaded from file.
    // Written on the message thread; the per-channel grid below is read on the worker
    // thread during a rebuild (after the reinit flag is set).
    Tunings::Scale           localScale;
    Tunings::KeyboardMapping localKBM;       // the single-file mapping (1-kbm case)
    Tunings::Tuning          localTuning;    // tuning used for the panel's display read-outs
    std::atomic<bool>        hasLocalTuning{false};
    std::atomic<bool>        localTuningNeedsReinit{false};
    bool                     hasLocalKBM = false;
    juce::String             localSclName;
    juce::String             localKbmName;
    juce::String             localSclDescription;  // the .scl's name/description line
    juce::String             localTuningError;  // set if last load failed

    // Per-channel .kbm mappings of the one .scl. Assignment (see loadKBMFiles): a file
    // named "*_i.kbm" (i in 1..16) maps to channel i; a file with no valid "_i" suffix is
    // a default that fills every channel not explicitly assigned. localKBMs/hasKBMForChannel
    // hold the composed result; channels with no mapping are inactive. anyKBMLoaded is false
    // when only a .scl is loaded (then all channels use the base scale = single-channel).
    Tunings::KeyboardMapping localKBMs[16];
    bool                     hasKBMForChannel[16] = {};
    bool                     anyKBMLoaded = false;

    // Per-channel frequency + mapping grid for the FILE source, written on the message
    // thread (rebuildLocalTuning), read on the worker (buildFileGamut). fileChannelActive
    // marks which channels contribute to the FILE gamut.
    double                   localFreqGrid[16][128]   = {};
    bool                     localMappedGrid[16][128] = {};
    bool                     fileChannelActive[16]    = {};

    // Which encoding feeds the engine (TuningSourceId). Default: MTS-ESP.
    std::atomic<int>         tuningSource{ TS_MTS };

    // 12-TET frequency table used when the source is STANDARD (ignores MTS / files).
    double                   standardFrequencies[NOF_FREQS] = {};

    void rebuildLocalTuning();

    // --- Private methods ---
    void initDSP(double sampleRate);
    void tearDownDSP();

    // Async tonegen rebuild — heavy work runs on rebuildThread, never the audio thread.
    void startRebuildThread();
    void stopRebuildThread();
    void rebuildThreadLoop(juce::Thread& thread);   // worker body
    void performBackgroundRebuild();                // worker: build a fresh tonegen
    void requestRebuild();                          // audio thread: ask for a rebuild
    void applyPendingRebuild();                     // audio thread: swap in a built tonegen
    void allNotesOff();                             // audio thread: release every sounding slot

    // Build the merged multichannel gamut for the MTS/SYSEX source: queries MTS over
    // the active channels, fills freqTable (NOF_FREQS), the (channel,note)->slot map,
    // the gamut size, and the wheel count to build (sized to the gamut — Solution B).
    void buildMTSGamut(double freqTable[], int slotIndexOut[16][128],
                       int& gamutSizeOut, int& nofWheelsOut);
    // Same, for the FILE source, from the per-channel .kbm grid (localFreqGrid/...).
    void buildFileGamut(double freqTable[], int slotIndexOut[16][128],
                        int& gamutSizeOut, int& nofWheelsOut);

    void updateScalePeriod();
    void applyParam(int index, float value);
    void renderAudio(float* outL, float* outR, int numSamples);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuneBfreeAudioProcessor)
};
