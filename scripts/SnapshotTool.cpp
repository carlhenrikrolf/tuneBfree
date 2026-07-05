// ============================================================================
//  SnapshotTool — headless GUI screenshot for review (dev tool, not shipped).
//  ----------------------------------------------------------------------------
//  Renders the plugin editor to a PNG entirely in software: no window is ever
//  opened, no audio/MIDI device is touched, and nothing outside this process
//  is read from the screen. This is the sandbox-friendly way to produce the
//  GUI screenshots used in the mockup-review workflow.
//
//      tuneBfree_snapshot <page 0|1|2> <out.png>
//
//  Page 0 = PLAY, 1 = TINKER, 2 = ROTARY. Renders at 2x (Retina-like).
// ============================================================================

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <cstdlib>
#include <iostream>

int main (int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cerr << "usage: tuneBfree_snapshot <page 0|1|2> <out.png>\n";
        return 1;
    }

    // The editor reads TUNEBFREE_PAGE to pick the start page; make sure the
    // in-editor snapshot hook stays off (this tool does its own snapshot).
    setenv ("TUNEBFREE_PAGE", argv[1], 1);
    unsetenv ("TUNEBFREE_SNAPSHOT");

    juce::ScopedJuceInitialiser_GUI juceInit;

    TuneBfreeAudioProcessor proc;
    proc.prepareToPlay (44100.0, 512);        // build the engine so read-outs are real

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);

        juce::File out (juce::File::getCurrentWorkingDirectory()
                            .getChildFile (juce::String (argv[2])));
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat png;
        if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
        {
            std::cerr << "could not write " << out.getFullPathName() << "\n";
            return 2;
        }
        std::cout << out.getFullPathName() << std::endl;
    }

    proc.releaseResources();
    return 0;
}
