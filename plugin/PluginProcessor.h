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
// HARMONICS (drawbar fine-tuning). Replaces Naren's CLAP ratio_top/ratio_bot
// integer pairs (same 18 parameter slots): per drawbar, the CUSTOM interval in
// cents above the key fundamental, plus an AUTO flag. AUTO = the pure JI
// harmonic quantized to the current tuning (stock behaviour); CUSTOM = the
// cents value used exactly (its wheel frequencies are injected un-quantized).
#define P_HARM_CENTS_MIN  20
#define P_HARM_CENTS_MAX  28
#define P_HARM_AUTO_MIN   29
#define P_HARM_AUTO_MAX   37
#define P_EXPRESSION      38
// Keyboard split (step 2). Indices past the CLAP-compatible range (0..38).
#define P_SPLIT_ENABLE     39
#define P_SPLIT_POINT      40   // split point, Hz
#define P_SPLIT_WIDTH      41   // crossfade width, cents (0 = hard split)
#define P_LOWER_VIBRATO    42   // lower-manual vibrato/chorus on/off
#define P_LOWER_DRAWBAR_MIN 43
#define P_LOWER_DRAWBAR_MAX 51  // 9 lower-manual drawbars

// --- TINKER page (engine physics; .cfg-file territory) ---
// Scanner + key click + crosstalk + EQ spline + wave need a tonegen rebuild;
// percussion times/gains and the preamp apply live.
#define P_SCANNER_HZ       52
#define P_SCANNER_V1       53
#define P_SCANNER_V2       54
#define P_SCANNER_V3       55
#define P_PERC_FAST_S      56
#define P_PERC_SLOW_S      57
#define P_PERC_GAIN        58
#define P_PERC_NORM_G      59
#define P_PERC_SOFT_G      60
#define P_CLICK_ATK_MODEL  61
#define P_CLICK_REL_MODEL  62
#define P_CLICK_ATK_LEVEL  63
#define P_CLICK_MIN        64
#define P_CLICK_MAX        65
#define P_CLICK_REL_LEVEL  66
#define P_XT_COMPARTMENT   67
#define P_XT_TRANSFORMER   68
#define P_XT_TERMINAL      69
#define P_XT_WIRING        70
#define P_EQ_BASS          71
#define P_EQ_BASS_SLOPE    72
#define P_EQ_TREBLE        73
#define P_EQ_TREBLE_SLOPE  74
#define P_WAVE             75   // 0 sine, 1 square, 2 triangle
#define P_PRE_IN           76
#define P_PRE_OUT          77
#define P_PRE_BASS_PRE     78
#define P_PRE_BASS_POST    79
#define P_PRE_SAG          80

// --- ROTARY page (whirl physics) — all apply live on the whirl instance ---
#define P_WHIRL_BYPASS     81
#define P_HORN_SLOW        82
#define P_HORN_FAST        83
#define P_HORN_ACCEL       84
#define P_HORN_DECEL       85
#define P_HORN_BRAKE       86
#define P_DRUM_SLOW        87
#define P_DRUM_FAST        88
#define P_DRUM_ACCEL       89
#define P_DRUM_DECEL       90
#define P_DRUM_BRAKE       91
#define P_HF_A_TYPE        92
#define P_HF_A_FREQ        93
#define P_HF_A_Q           94
#define P_HF_A_GAIN        95
#define P_HF_B_TYPE        96
#define P_HF_B_FREQ        97
#define P_HF_B_Q           98
#define P_HF_B_GAIN        99
#define P_DF_TYPE         100
#define P_DF_FREQ         101
#define P_DF_Q            102
#define P_DF_GAIN         103
#define P_HORN_LEVEL      104
#define P_HORN_LEAK       105
#define P_HORN_WIDTH      106
#define P_DRUM_WIDTH      107
#define P_MIC_ANGLE       108
#define P_MIC_DIST        109
#define P_MASTER_VOL      110   // header 🔊 popup — plain output gain after the chain
#define P_ACTIVE_MANUAL   111   // unitimbral routing: 0 = upper bank sounds, 1 = lower
#define P_COUNT           112

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
    // Individual .kbm filenames of the current batch (for the FILES popup).
    juce::StringArray getLocalKbmNames() const { return localKbmNames; }
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

    // --- Split "learn" (set-from-notes). GUI arms it; the audio thread tracks the held
    // notes and computes the split point/width, then disarms itself when all are released.
    void   setLearnSplit(bool on) noexcept { learnSplitActive.store(on, std::memory_order_release); }
    bool   isLearnSplitActive() const noexcept { return learnSplitActive.load(); }
    double getLearnedSplitPoint() const noexcept { return learnedSplitPoint.load(); }  // Hz
    double getLearnedSplitWidth() const noexcept { return learnedSplitWidth.load(); }  // cents

    // Wall-clock time (ms since epoch) of the last tuning change (file load, sysex,
    // or MTS frequency change). 0 if tuning has never changed. For a live MTS master
    // the panel shows the current time instead, since the master is queried every block.
    juce::int64 getLastTuningChangeMs() const noexcept { return lastTuningChangeMs.load(); }

    // Kind of the last MIDI-tuning sysex received: -1 none yet, 0 = non-realtime
    // (bulk dump → applies at note-on), 1 = realtime (retunes sounding notes).
    // Drives the greyed NOTE ON / ALWAYS indicator while the source is SYSEX.
    int getLastSysexKind() const noexcept { return lastSysexKind.load(); }

    // --- HARMONICS panel support (message thread) ---
    // The pure JI harmonic ratio of each drawbar (1/2, 3/2, 1/1, 2/1, ...).
    static const double stockJIRatio[9];
    // Deviation (cents) of drawbar b's SOUNDING pitch from the pure JI harmonic,
    // at the reference fundamental refHz. Mimics the engine's wheel choice on a
    // UI-side snapshot of the wheel table (refreshed after every rebuild).
    double getHarmonicErrorCents(int b, double refHz) const;
    // The raw entry strings ("3/2", "702.23 c") — persisted with the state so the
    // display keeps the user's chosen notation (the cents PARAM drives the engine).
    void         setHarmonicEntryText(int i, const juce::String& s);
    juce::String getHarmonicEntryText(int i) const;

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

    // UI snapshot of the wheel table + target ratios (for the HARMONICS error
    // read-outs). Written after every engine build (worker / init), read by the
    // message thread — never touched by the audio thread.
    mutable juce::SpinLock uiWheelLock;
    std::vector<double>    uiWheelFreqs;
    std::vector<char>      uiWheelInjected;   // parallel to uiWheelFreqs
    char                   uiBusCustom[9] = {};
    double                 uiTargetRatio[9] = { 0.5, 1.5, 1, 2, 3, 4, 5, 6, 8 };

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

    // Master volume: ramped per block so knob moves / automation stay click-free.
    float masterGainCur = 1.0f;

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
    std::atomic<int>         lastSysexKind{-1};   // see getLastSysexKind()

    // Per-(channel, note) routing state: the engine key a held note is sounding on for
    // the upper and lower manuals, or -1 if not sounding there. Under the keyboard split
    // a note can sound on both manuals (crossfade zone); each is released independently.
    int soundingUpper[16][128];
    int soundingLower[16][128];

    // Per-engine-key reference count: how many held notes share each tonegen key.
    // Coincident pitches (across channels, or the same slot on one manual) de-duplicate
    // to one key, which must stay sounding until the LAST holder releases (oscKeyOn on
    // 0→1, oscKeyOff on 1→0). Indexed by engine key (upper = slot, lower = gamutSize+slot).
    int keyRefCount[MAX_KEYS] = {};

    // Panic: set by triggerPanic()/CC 120/123, consumed on the audio thread to release all.
    std::atomic<bool> panicRequested{ false };

    // Split "learn": armed by the GUI, driven by the audio thread. While a note session
    // is active (>=1 held) it tracks the lowest/highest sounding pitch and publishes the
    // split point (geometric centre) + width (interval, cents). Disarms on all-released.
    std::atomic<bool>   learnSplitActive{ false };
    std::atomic<double> learnedSplitPoint{ 261.63 };
    std::atomic<double> learnedSplitWidth{ 0.0 };
    bool                learnSessionActive = false;   // audio thread only
    double              learnMinPitch = 0.0, learnMaxPitch = 0.0;   // audio thread only
    // Notes held silently while KEYPRESS (learn) is armed — they set the split but
    // must not sound, and are tracked separately so releases balance correctly.
    bool                learnHeld[16][128] = {};      // audio thread only
    int                 learnHeldCount = 0;           // audio thread only

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
    Tunings::Tuning          localTuning;    // tuning used for the panel's display read-outs
    std::atomic<bool>        hasLocalTuning{false};
    std::atomic<bool>        localTuningNeedsReinit{false};
    bool                     hasLocalKBM = false;
    juce::String             localSclName;
    juce::String             localKbmName;
    juce::StringArray        localKbmNames;     // every file of the current .kbm batch
    juce::String             localSclDescription;  // the .scl's name/description line
    juce::String             localTuningError;  // set if last load failed

    // Per-channel .kbm assignment (see loadKBMFiles): "*_i.kbm" (i in 1..16) → explicit
    // mapping for channel i; a file with no valid "_i" suffix → the generic mapping. A
    // channel with no explicit mapping falls back to the generic mapping, and that to the
    // base .scl (no .kbm). MTS has no such fallback — it always queries channel i / -1.
    Tunings::KeyboardMapping explicitKBM[16];
    bool                     hasExplicitKBM[16] = {};
    Tunings::KeyboardMapping genericKBM;
    bool                     hasGenericKBM = false;

    // Frequency + mapping grids for the FILE source, written by rebuildLocalTuning
    // (message thread), read by buildFileGamut (worker). localFreqGrid[c] is channel c's
    // tuning (explicit → generic → base); the generic* row is used under OMNI. Which
    // channels actually sound is the popup's channelActive mask, not these.
    double                   localFreqGrid[16][128]   = {};
    bool                     localMappedGrid[16][128] = {};
    double                   genericFreqGrid[128]     = {};
    bool                     genericMappedGrid[128]   = {};

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
    // Apply the build-time TINKER parameters (scanner, key click, crosstalk, EQ
    // spline, wave, percussion) to a tonegen BEFORE initToneGenerator/init_vibrato.
    void applyEngineBuildParams(b_tonegen* t);

    // --- HARMONICS engine support ---
    // targetRatio per drawbar from the params: AUTO → stock JI, CUSTOM → 2^(cents/1200).
    // Returns true if any drawbar is CUSTOM (needs wheel injection).
    bool computeTargetRatios(double outRatio[], bool outCustom[]) const;
    // Insert the exact wheel frequencies CUSTOM drawbars need (fundamental × ratio
    // for every gamut slot) into the extended region of the wheel table, so the
    // engine's closest-wheel search finds them un-quantized. injectedFlags
    // (length NOF_FREQS) marks which final wheels came from injection — AUTO
    // drawbars must not quantize to those.
    static void injectCustomWheels(double* freqTable, char* injectedFlags,
                                   int gamutSize, int& nofWheels,
                                   const double targetRatio[], const bool custom[]);
    // Refresh the UI wheel snapshot from a freshly built engine.
    void publishUIWheelSnapshot(const b_tonegen* t, const double targetRatio[]);
    void renderAudio(float* outL, float* outR, int numSamples);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuneBfreeAudioProcessor)
};
