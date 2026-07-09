// ============================================================================
//  AudioRenderTool — headless offline audio render (dev tool, not shipped).
//  ----------------------------------------------------------------------------
//  The audio counterpart of SnapshotTool: it drives the plugin's DSP entirely
//  offline — no audio device, no MIDI device, no message loop — and writes the
//  stereo output to a WAV. This is the sandbox-safe way to actually LISTEN to a
//  change (or, more usefully here, to check the render for NaN/Inf, silence,
//  level, and tuning) without a DAW or a real soundcard.
//
//      tuneBfree_render <out.wav> [options]
//
//  Options (all optional):
//    --note N          MIDI note number to play           (default 60)
//    --vel V           note-on velocity 1..127            (default 100)
//    --channel C       MIDI channel 1..16                 (default 1)
//    --seconds S       total render length, seconds       (default 3.0)
//    --hold H          note held for H seconds            (default 0.6 * seconds)
//    --sr RATE         sample rate                        (default 48000)
//    --block N         processing block size              (default 512)
//    --drawbars a,b,.. upper drawbar registration, 9 ints 0..8
//                                                         (default 8,8,8,0,0,0,0,0,0)
//    --scl FILE        load a Scala .scl (selects the SCALA source)
//    --kbm FILE        load a .kbm keyboard mapping
//    --source NAME     tuning source: standard|scala|mts|sysex (default standard,
//                      or scala when --scl is given)
//
//  On exit it prints a one-line report: frames, peak, RMS, and — importantly —
//  the count of non-finite (NaN/Inf) samples, which must be 0.
// ============================================================================

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

// Tiny CLI helper: value following "--key", or a default.
juce::String opt (const juce::StringArray& args, const juce::String& key,
                  const juce::String& fallback)
{
    const int i = args.indexOf (key);
    return (i >= 0 && i + 1 < args.size()) ? args[i + 1] : fallback;
}
bool has (const juce::StringArray& args, const juce::String& key) { return args.contains (key); }

} // namespace

int main (int argc, char* argv[])
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (juce::String (argv[i]));

    if (args.isEmpty() || args[0].startsWith ("--")) {
        std::cerr << "usage: tuneBfree_render <out.wav> [--note N] [--seconds S] "
                     "[--drawbars a,b,c,d,e,f,g,h,i] [--scl FILE] [--kbm F1,F2,...] "
                     "[--channels 1,2] [--sweep paramID:from:to] "
                     "[--source standard|scala|mts|sysex] ...\n";
        return 1;
    }

    const juce::File   outFile   = juce::File::getCurrentWorkingDirectory().getChildFile (args[0]);
    const int          note      = opt (args, "--note",    "60").getIntValue();
    const int          velocity  = juce::jlimit (1, 127, opt (args, "--vel", "100").getIntValue());
    const int          channel   = juce::jlimit (1, 16,  opt (args, "--channel", "1").getIntValue());
    const double       seconds   = juce::jmax (0.1, opt (args, "--seconds", "3.0").getDoubleValue());
    const double       hold      = opt (args, "--hold", juce::String (seconds * 0.6)).getDoubleValue();
    const double       sampleRate= juce::jmax (8000.0, opt (args, "--sr", "48000").getDoubleValue());
    const int          blockSize = juce::jmax (16, opt (args, "--block", "512").getIntValue());

    juce::ScopedJuceInitialiser_GUI juceInit;   // JUCE core / leak detector

    TuneBfreeAudioProcessor proc;
    proc.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    proc.prepareToPlay (sampleRate, blockSize);

    // --- Drawbar registration (upper manual) ---
    {
        auto db = juce::StringArray::fromTokens (opt (args, "--drawbars", "8,8,8,0,0,0,0,0,0"), ",", "");
        for (int i = 0; i < 9 && i < db.size(); ++i)
            if (auto* p = proc.apvts.getParameter ("drawbar" + juce::String (i)))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0, 8, db[i].getIntValue())));
    }

    // --- Arbitrary parameter sets: --set paramID:value (repeatable, real units) ---
    // Handy for isolating the tonegen (e.g. --set whirl_bypass:1 --set reverb_mix:0).
    for (int i = 0; i + 1 < args.size(); ++i)
        if (args[i] == "--set") {
            auto t = juce::StringArray::fromTokens (args[i + 1], ":", "");
            if (t.size() == 2)
                if (auto* p = proc.apvts.getParameter (t[0]))
                    p->setValueNotifyingHost (p->convertTo0to1 (t[1].getFloatValue()));
        }

    // --- Tuning ---
    juce::String source = opt (args, "--source", has (args, "--scl") ? "scala" : "standard");
    if (has (args, "--scl"))
        proc.loadSCLFile (juce::File::getCurrentWorkingDirectory().getChildFile (opt (args, "--scl", {})));
    if (has (args, "--kbm")) {
        // --kbm accepts a comma-separated list; "*_i.kbm" → MIDI channel i (multichannel).
        juce::Array<juce::File> kbms;
        for (const auto& name : juce::StringArray::fromTokens (opt (args, "--kbm", {}), ",", ""))
            kbms.add (juce::File::getCurrentWorkingDirectory().getChildFile (name.trim()));
        proc.loadKBMFiles (kbms);
    }
    proc.setTuningSource (source == "scala" ? TS_FILE
                        : source == "mts"   ? TS_MTS
                        : source == "sysex" ? TS_SYSEX
                                            : TS_STANDARD);

    // --active 1,2 : restrict the gamut to these channels (others off), so the
    // generic-fallback pitches of unmapped channels don't pollute it.
    if (has (args, "--active")) {
        juce::Array<int> on;
        for (const auto& c : juce::StringArray::fromTokens (opt (args, "--active", {}), ",", ""))
            on.add (c.getIntValue());
        for (int ch = 1; ch <= 16; ++ch)
            proc.setChannelActive (ch - 1, on.contains (ch));
    }

    // --- Live parameter automation: --sweep ID:from:to (repeatable) ramps a
    //     parameter linearly across the note-held window, mimicking dragging a
    //     control WHILE a note sounds (the reported bug-3 trigger). ---
    struct Sweep { juce::RangedAudioParameter* p; float from, to; juce::String id; };
    std::vector<Sweep> sweeps;
    for (int i = 0; i + 1 < args.size(); ++i)
        if (args[i] == "--sweep") {
            auto t = juce::StringArray::fromTokens (args[i + 1], ":", "");
            if (t.size() == 3)
                if (auto* p = proc.apvts.getParameter (t[0]))
                    sweeps.push_back ({ dynamic_cast<juce::RangedAudioParameter*> (p),
                                        t[1].getFloatValue(), t[2].getFloatValue(), t[0] });
        }
    // Notes can also be requested on several channels at once (multichannel gamut):
    // --channels 1,2 overrides --channel.
    juce::Array<int> noteChannels;
    for (const auto& c : juce::StringArray::fromTokens (opt (args, "--channels", juce::String (channel)), ",", ""))
        noteChannels.addIfNotAlreadyThere (juce::jlimit (1, 16, c.getIntValue()));

    // --- Warm up: process silent blocks so the async tonegen rebuild (triggered
    //     by the tuning / drawbar changes above) completes and swaps in. ---
    {
        juce::AudioBuffer<float> warm (2, blockSize);
        // The async tonegen rebuild triggered by a FILE (.scl/.kbm) tuning — heavier
        // with a multichannel gamut — can take a while to build and swap in. Warm up
        // generously so the note plays on the correct engine (too few blocks and it
        // captures the stale 12-TET engine). Overridable with --warmup.
        const int warmBlocks = opt (args, "--warmup", "400").getIntValue();
        for (int i = 0; i < warmBlocks; ++i) {   // silent blocks; async rebuild lands
            warm.clear();
            juce::MidiBuffer empty;
            proc.processBlock (warm, empty);
            juce::Thread::sleep (5);
        }
    }

    // --- Tuning diagnostics: confirm the requested tuning actually applied
    //     (the sounding pitch of the played note, vs 12-TET). ---
    {
        const double f    = proc.getDisplayFrequency (note);
        const double f12  = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        std::cout << "  tuning: source=" << proc.getTuningSource()
                  << " localTuningLoaded=" << (int) proc.getHasLocalTuning();
        if (proc.getLocalTuningError().isNotEmpty())
            std::cout << " ERROR=\"" << proc.getLocalTuningError() << "\"";
        std::cout << "  note " << note << " f=" << juce::String (f, 2) << "Hz"
                  << " (12-TET " << juce::String (f12, 2) << "Hz, "
                  << juce::String (1200.0 * std::log2 (f / f12), 1) << "c off)\n";
    }

    // --- Render: note-on at t=0, note-off at t=hold, out to `seconds`. ---
    const int totalFrames = (int) std::ceil (seconds * sampleRate);
    const int holdFrames  = juce::jlimit (0, totalFrames, (int) std::ceil (hold * sampleRate));
    juce::AudioBuffer<float> out (2, totalFrames);
    out.clear();

    juce::AudioBuffer<float> block (2, blockSize);
    for (int pos = 0; pos < totalFrames; pos += blockSize) {
        const int n = juce::jmin (blockSize, totalFrames - pos);
        block.setSize (2, n, false, false, true);
        block.clear();

        // Apply the live sweeps at this block: fraction runs 0→1 across the held
        // note, then sticks at the end value (as if the drag finished).
        if (! sweeps.empty()) {
            const float frac = holdFrames > 0 ? juce::jlimit (0.0f, 1.0f, (float) pos / (float) holdFrames) : 1.0f;
            for (const auto& s : sweeps)
                if (s.p != nullptr)
                    s.p->setValueNotifyingHost (s.p->convertTo0to1 (s.from + (s.to - s.from) * frac));
        }

        juce::MidiBuffer midi;
        if (pos == 0)
            for (int ch : noteChannels)
                midi.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) velocity), 0);
        if (holdFrames >= pos && holdFrames < pos + n)
            for (int ch : noteChannels)
                midi.addEvent (juce::MidiMessage::noteOff (ch, note), holdFrames - pos);

        proc.processBlock (block, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, pos, block, ch, 0, n);
    }

    // What the GUI would DISPLAY for the drawbar cents errors: the reference is
    // the last note's sounding frequency (should match the engine pitch), and
    // getHarmonicErrorCents is the ±cents shown under each drawbar.
    {
        const double ref = proc.getLastNoteFreq();
        std::cout << "  display: lastNoteFreq=" << juce::String (ref, 2) << "Hz  drawbarErr(c)=";
        for (int b = 0; b < 9; ++b)
            std::cout << juce::String (proc.getHarmonicErrorCents (b, ref), 1) << " ";
        std::cout << std::endl;
        if (has (args, "--wheels")) {   // dump engine wheels near each drawbar's target
            static const double ratio[9] = { 0.5, 1.5, 1, 2, 3, 4, 5, 6, 8 };
            for (int b = 0; b < 9; ++b)
                if (opt (args, "--drawbars", "8,8,8,0,0,0,0,0,0")
                        .startsWith ("") && ratio[b] > 0)
                    std::cout << "    drawbar " << b << " target " << juce::String (ref * ratio[b], 1)
                              << "Hz: " << proc.debugWheelsNear (ref * ratio[b], 60.0) << "\n";
        }
    }
    proc.releaseResources();

    // --- Report: level + NaN/Inf audit (the point of the tool). ---
    int   nonFinite = 0;
    int   firstNaNframe = -1;
    float peak = 0.0f;
    double sumSq = 0.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < totalFrames; ++i) {
            const float s = out.getSample (ch, i);
            if (! std::isfinite (s)) {
                ++nonFinite;
                if (firstNaNframe < 0 || i < firstNaNframe) firstNaNframe = i;
                continue;
            }
            peak   = juce::jmax (peak, std::abs (s));
            sumSq += (double) s * s;
        }
    const double rms = std::sqrt (sumSq / juce::jmax (1, 2 * totalFrames));

    // When it went non-finite, report the time and the sweep value there — that
    // pins the drag position that triggers the fault.
    if (firstNaNframe >= 0) {
        std::cout << "  first NaN at frame " << firstNaNframe
                  << " (t=" << juce::String (firstNaNframe / sampleRate, 4) << "s)";
        const float frac = holdFrames > 0 ? juce::jlimit (0.0f, 1.0f, (float) firstNaNframe / (float) holdFrames) : 1.0f;
        for (const auto& s : sweeps)
            std::cout << "  " << s.id << "=" << juce::String (s.from + (s.to - s.from) * frac, 3);
        std::cout << std::endl;
    }

    // --- Write WAV (float, so nothing clips in the file). ---
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    if (auto os = std::unique_ptr<juce::FileOutputStream> (outFile.createOutputStream())) {
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (os.get(), sampleRate, 2, 32, {}, 0));
        if (writer != nullptr) {
            os.release();   // writer owns the stream now
            writer->writeFromAudioSampleBuffer (out, 0, totalFrames);
        } else {
            std::cerr << "could not create WAV writer\n";
            return 2;
        }
    } else {
        std::cerr << "could not open " << outFile.getFullPathName() << "\n";
        return 2;
    }

    std::cout << outFile.getFullPathName() << "\n"
              << "  frames=" << totalFrames << " sr=" << (int) sampleRate
              << " peak=" << juce::String (peak, 4)
              << " rms=" << juce::String (rms, 5)
              << " nonFinite=" << nonFinite
              << (nonFinite == 0 ? "  OK" : "  *** NaN/Inf ***") << std::endl;

    return nonFinite == 0 ? 0 : 3;
}
