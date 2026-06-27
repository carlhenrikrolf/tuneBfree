#pragma once
#include <JuceHeader.h>

class DrawbarPanel : public juce::Component
{
public:
    DrawbarPanel();
    void resized() override;

    // Name slider after the APVTS parameter ID
    juce::Slider drawbar0, drawbar1, drawbar2, drawbar3, drawbar4,
                 drawbar5, drawbar6, drawbar7, drawbar8;
    
    // Name buttons/toggles after their APVTS ID where applicable
    juce::TextButton upperBtn{"Upper"}, lowerBtn{"Lower"};
    
    // Labels: use <slider_name>_label pattern
    juce::Label drawbar0_label, drawbar1_label, drawbar2_label, drawbar3_label,
                drawbar4_label, drawbar5_label, drawbar6_label, drawbar7_label,
                drawbar8_label;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawbarPanel)
}