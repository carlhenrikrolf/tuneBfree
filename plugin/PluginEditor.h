#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Stage/Studio mode placeholder editor.
// Stage mode and Studio mode will be built here in Phase 3.
// For now this shows a minimal placeholder so the plugin loads with a visible window.

class TuneBfreeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    TuneBfreeAudioProcessorEditor(TuneBfreeAudioProcessor& p);
    ~TuneBfreeAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    TuneBfreeAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuneBfreeAudioProcessorEditor)
};
