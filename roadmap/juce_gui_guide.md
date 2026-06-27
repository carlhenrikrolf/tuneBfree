# Writing the tuneBfree JUCE GUI Yourself

This document explains a workflow where you write the component structure and layout, and then hand the file back to Claude to wire everything to the audio engine (APVTS parameters, MIDI CC labels, value listeners, LookAndFeel details).

The split works well because:
- **Layout is visual.** JUCE's `resized()` is straightforward rectangle arithmetic, and you can see the result immediately by running the plugin. Claude cannot see the screen.
- **Wiring is mechanical.** APVTS attachments, OSC address maps, and MIDI CC display are repetitive but require knowing the parameter IDs exactly. Claude has those and can generate them reliably.

---

## Step 1 — Declare your components

Write a header file with member variable declarations only. Don't add any logic yet.

Follow this naming convention: **slider named after its APVTS parameter ID**. This lets Claude auto-generate the attachment without asking.

```cpp
// plugin/DrawbarPanel.h
#pragma once
#include <JuceHeader.h>

class DrawbarPanel : public juce::Component
{
public:
    DrawbarPanel();
    void resized() override;

    // Name sliders after the APVTS parameter ID
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
};
```

---

## Step 2 — Write `resized()` (layout only)

Place your components using `juce::Rectangle<int>` arithmetic. Don't set any styles, colours, or ranges yet — just positions.

```cpp
void DrawbarPanel::resized()
{
    auto r = getLocalBounds().reduced(8);

    // Upper/Lower switch at top
    auto switchRow = r.removeFromTop(30);
    upperBtn.setBounds(switchRow.removeFromLeft(80).reduced(2));
    lowerBtn.setBounds(switchRow.removeFromLeft(80).reduced(2));

    r.removeFromTop(8); // gap

    // 9 drawbars, equally spaced
    auto faderArea = r.removeFromTop(240);
    int faderWidth = faderArea.getWidth() / 9;
    for (auto* s : { &drawbar0, &drawbar1, &drawbar2, &drawbar3, &drawbar4,
                     &drawbar5, &drawbar6, &drawbar7, &drawbar8 })
    {
        s->setBounds(faderArea.removeFromLeft(faderWidth).reduced(3, 0));
    }

    // Label row below drawbars
    auto labelArea = r.removeFromTop(20);
    int labelWidth = labelArea.getWidth() / 9;
    for (auto* l : { &drawbar0_label, &drawbar1_label, &drawbar2_label, &drawbar3_label,
                     &drawbar4_label, &drawbar5_label, &drawbar6_label, &drawbar7_label,
                     &drawbar8_label })
    {
        l->setBounds(labelArea.removeFromLeft(labelWidth));
    }
}
```

**Tips for layout:**
- `r.removeFromTop(N)` / `removeFromLeft(N)` — the workhorse for sequential layouts
- `r.reduced(margin)` / `r.reduced(hMargin, vMargin)` — add inset
- For equal division: `r.getWidth() / count` then iterate with `removeFromLeft`
- For a fixed-right element: `r.removeFromRight(width)` before the main layout

---

## Step 3 — Add components to the parent

In your constructor, call `addAndMakeVisible` for each component. Claude can generate this list from your header file automatically.

```cpp
DrawbarPanel::DrawbarPanel()
{
    // Claude will generate these:
    addAndMakeVisible(drawbar0);
    addAndMakeVisible(drawbar1);
    // ... (9 drawbars)
    addAndMakeVisible(upperBtn);
    addAndMakeVisible(lowerBtn);
    addAndMakeVisible(drawbar0_label);
    // ... (9 labels)
}
```

---

## Step 4 — Hand it to Claude

Once your component compiles and shows rough positions in the plugin window, tell Claude: *"Wire this up"*. Provide the header file and Claude will add:

1. **APVTS `SliderAttachment` / `ButtonAttachment` declarations** — one per named control, matching the parameter IDs in `PluginProcessor::createParameterLayout()`
2. **Slider style and range calls** — `setSliderStyle`, `setRange`, `setNumDecimalPlacesToDisplay` based on the APVTS parameter type
3. **Drawbar colour coding** — sets each drawbar's `colourId` or installs a custom LookAndFeel to paint Hammond colours (brown/white/black) based on the drawbar index
4. **CC and footage labels** — sets the text on each `*_label` to the correct footage string (`16'`, `5⅓'`, etc.) and MIDI CC number (`CC70`–`CC78`)
5. **Upper/Lower switch logic** — button `onClick` handlers that show/hide the correct drawbar APVTS parameter set
6. **LookAndFeel overrides** — `drawLinearSlider` for inverted fill direction (pulled-out = louder = visually down), rotary arc style for effect knobs

---

## Page-by-page guide

### Play page (`plugin/PlayPage.h`)

Components to declare:
- `DrawbarPanel drawbarPanel` — handles all 9 drawbars + upper/lower switch
- `juce::Slider drum, horn` — Leslie speed (3-way: slow/brake/fast)
- `juce::Slider volume` — expression/volume
- `juce::Slider reverb_mix, overdrive, character` — effects knobs
- `juce::ToggleButton vibrato, percussion, percussion_vol, percussion_dec, percussion_har`
- `juce::ComboBox vibrato_type` — V1–C3 (6 options)

What Claude will wire: all APVTS attachments, the 6-item vibrato_type ComboBox population, CC labels.

### Config page (`plugin/ConfigPage.h`)

Components to declare:
- `juce::TextButton loadCfgBtn{"Load .cfg…"}`
- For each of the 9 drawbars, one row:
  ```cpp
  juce::Slider ratio_top_0, ratio_bot_0; // ratio_top_0 → APVTS "ratio_top_0"
  juce::Label ratio_top_0_label;          // will be set to "16'  Sub"
  ```
  (Repeat for _1 through _8)

What Claude will wire: range 0–1000 / 1–1000 for numerator/denominator, default values, row labels, the Load .cfg button listener.

### Presets page (`plugin/PresetsPage.h`)

Components to declare:
- `juce::TextButton loadPgmBtn{"Load .pgm…"}`
- `juce::ListBox presetList`
- A `juce::ListBoxModel` subclass

What Claude will wire: the ListBoxModel implementation that reads from a parsed `.pgm` structure, `loadPgmBtn.onClick` handler.

### Tuning sidepanel (`plugin/TuningPanel.h`)

Already partially implemented in `plugin/PluginEditor`. The existing `TuningPanel` component has the MTS-ESP status area and file loading buttons. Claude can extend it with:
- Last-played frequency and interval display (needs `PluginProcessor` to report `previousFrequency`)
- Scale period display (uses `tuning.cpp::getScalePeriod()`)

---

## LookAndFeel — what to implement yourself vs. what to delegate

**Write yourself** (visual decisions):
- Which font to use (`juce::Font`)
- Button shape (rounded corners radius)
- Overall colour assignments (see the constants in `gui.md`)

**Delegate to Claude** (mechanical overrides):
- `drawLinearSlider` — inverted drawbar fill direction
- `drawRotarySlider` — arc angles, knob dot
- `drawButtonBackground` — states for TextButton/ToggleButton
- `getTabButtonBestWidth` / `drawTabButton` — nav bar tab appearance

---

## Naming conventions summary

| Pattern | Example | Used for |
|---------|---------|---------|
| Slider named after APVTS ID | `drawbar0`, `reverb_mix` | Auto-generates `SliderAttachment` |
| Button named after APVTS ID | `vibrato`, `overdrive` | Auto-generates `ButtonAttachment` |
| `<control>_label` | `drawbar0_label` | Auto-populated with footage + CC text |
| `<page>Page` | `DrawbarPanel`, `ConfigPage` | Convention only |

If you name a slider anything else, just tell Claude the mapping: *"my slider `myFader` maps to APVTS parameter `reverb_mix`"*.

---

## Quick workflow example

1. You write `DrawbarPanel.h` with declarations + `resized()` positions
2. Plugin compiles and you can see rough box positions
3. You say: *"Wire DrawbarPanel"*
4. Claude writes the `DrawbarPanel.cpp` constructor body with:
   - `addAndMakeVisible` for all components
   - Slider styles, ranges, drawbar colours
   - Footage and CC labels
5. You look at it, adjust font sizes or positions in `resized()`, rebuild
6. Repeat for each page

This is faster than describing what you want Claude to build from scratch, and you stay in control of the spatial layout.
