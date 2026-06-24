#include "PluginEditor.h"

TuneBfreeAudioProcessorEditor::TuneBfreeAudioProcessorEditor(TuneBfreeAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(480, 200);
}

TuneBfreeAudioProcessorEditor::~TuneBfreeAudioProcessorEditor() {}

void TuneBfreeAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(28.0f, juce::Font::bold));
    g.drawText("tuneBfree 2.0", getLocalBounds().reduced(20, 0).removeFromTop(80),
               juce::Justification::centredLeft);

    g.setFont(juce::Font(14.0f));
    g.setColour(juce::Colour(0xffaaaaaa));
    g.drawText("Stage / Studio UI — Phase 3",
               getLocalBounds().reduced(20, 0).removeFromTop(130).removeFromTop(50).translated(0, 50),
               juce::Justification::centredLeft);

    g.setFont(juce::Font(12.0f));
    g.setColour(juce::Colour(0xff666688));
    g.drawText("Use your DAW's generic parameter editor to control drawbars and effects.",
               getLocalBounds().reduced(20, 40).removeFromBottom(40),
               juce::Justification::centredLeft);
}

void TuneBfreeAudioProcessorEditor::resized() {}
