---
description: tuneBfree GUI architecture, JUCE component patterns, Open Stage Control mockup format, colour palette, and drawbar/effects widget implementation
triggers:
  - gui
  - GUI
  - LookAndFeel
  - drawbar
  - slider
  - SidePanel
  - TabbedComponent
  - resized
  - layout
  - Open Stage Control
  - mockup
  - colour palette
---

# tuneBfree GUI Reference

---

## Architecture Overview

The GUI has four navigation areas:

| Area | JUCE mechanism | Description |
|------|---------------|-------------|
| Default page | shown by default | Drawbars, effects, rotary, volume |
| Configuration page | `setVisible` swap or `TabbedComponent` | .cfg settings, drawbar pitch |
| Presets page | same | .pgm preset list |
| Tuning side panel | `juce::SidePanel` | Expandable from right edge; slides on top of current page |

The mockup lives at `roadmap/gui.json` in **Open Stage Control** format (see section below).

---

## Colour Palette

Inspired by Crumar/GMLab (dark mode, dimly-lit-venue vibe). Primary accent is amber.

```cpp
// In a shared header or LookAndFeel class:
static constexpr juce::uint32 kBg          = 0xff141414; // near-black
static constexpr juce::uint32 kBgPanel     = 0xff1e1e1e; // panel surface
static constexpr juce::uint32 kBgHeader    = 0xff111111; // header strip
static constexpr juce::uint32 kAccent      = 0xffff7f00; // amber (gui.json colorWidget)
static constexpr juce::uint32 kTextMain    = 0xffd8d8d8; // off-white
static constexpr juce::uint32 kTextMuted   = 0xff888888;
static constexpr juce::uint32 kGreen       = 0xff50e060; // MTS-ESP connected
static constexpr juce::uint32 kRed         = 0xffcc3333; // error
```

### Hammond drawbar colours

Standard colours used on real Hammond/clonewheel drawbars:

| Drawbar | Footage | Colour | JUCE colour |
|---------|---------|--------|-------------|
| 0 | 16' | Brown | `0xffa05030` |
| 1 | 5⅓' | Brown | `0xffa05030` |
| 2 | 8' | White | `0xffe8e8e8` |
| 3 | 4' | White | `0xffe8e8e8` |
| 4 | 2⅔' | Black | `0xff303030` |
| 5 | 2' | White | `0xffe8e8e8` |
| 6 | 1⅗' | Black | `0xff303030` |
| 7 | 1⅓' | Black | `0xff303030` |
| 8 | 1' | Black | `0xff303030` |

The OSC mockup files (`roadmap/gui_play.json` etc.) use the correct Hammond colours — copy those directly into JUCE (they match the JUCE constants above).

---

## JUCE Components Used

### juce::Slider — Drawbars

```cpp
juce::Slider drawbar;
drawbar.setSliderStyle(juce::Slider::LinearVertical);
drawbar.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
drawbar.setRange(0.0, 8.0, 1.0);   // integer steps 0–8
drawbar.setDoubleClickReturnValue(true, 0.0); // double-click resets to 0

// Bind to APVTS parameter "drawbar0"
sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
    processor.apvts, "drawbar0", drawbar);
```

Note on direction: on a real Hammond, pulling the drawbar out = louder. The physical maximum (fully pulled out) = 8, displayed at the **bottom** of the fader travel (closest to the player). In JUCE, use a custom `drawLinearSlider` in LookAndFeel to invert the fill direction, or set `drawbar.setRange(8.0, 0.0, 1.0)` (max at top). The OSC mockup uses `range: {min:8, max:0}` (inverted — OSC faders go top=min, bottom=max by default).

For drawbar colour coding, override `drawLinearSliderThumb` or paint a coloured rectangle above/below the track in a subclass.

### juce::Slider — Rotary knobs (effects)

```cpp
juce::Slider reverbKnob;
reverbKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
reverbKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
reverbKnob.setRange(0.0, 1.0);
knobAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
    processor.apvts, "reverb_mix", reverbKnob);
```

### juce::TextButton / ToggleButton — Upper/Lower switch

```cpp
juce::TextButton upperBtn{"Upper"}, lowerBtn{"Lower"};
upperBtn.setClickingTogglesState(false);
lowerBtn.setClickingTogglesState(false);
// Keep them in sync manually; only one active at a time (radio-button style)
// Or use juce::DrawableButton in toggle mode.
```

### juce::TabbedComponent — Main page navigation

```cpp
juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
tabs.addTab("Play",   juce::Colours::transparentBlack, &defaultPage,   false);
tabs.addTab("Config", juce::Colours::transparentBlack, &configPage,    false);
tabs.addTab("Presets",juce::Colours::transparentBlack, &presetsPage,   false);
addAndMakeVisible(tabs);
```

`TabbedButtonBar` placement: `TabsAtTop` or `TabsAtBottom`. The button bar height defaults to 30px. The content area fills the rest.

### juce::SidePanel — Tuning panel

```cpp
// In editor header:
juce::SidePanel tuningPanel{"Tuning", 320, false}; // false = slides from right

// In constructor:
addAndMakeVisible(tuningPanel);
tuningPanel.setContent(&tuningContent, false); // don't delete content

// Toggle from a button:
tuningBtn.onClick = [this] { tuningPanel.showOrHide(!tuningPanel.isPanelShowing()); };
```

Key properties:
- The panel slides on top of other content — it does not push the layout.
- Width is fixed at construction; can be anything (ROADMAP uses ~30% of window width = ~210px on a 700px window).
- JUCE draws a dismiss shadow automatically.
- Set content via `setContent(Component*, bool deleteWhenDone)`.

### APVTS Attachments

Always create attachments in the editor (or sub-component) constructor, not in `resized()`. Attachments keep a reference to the slider alive, so declare them as member variables (not locals).

```cpp
// Header — declare alongside each control:
std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> drawbar0Att;

// Constructor body:
drawbar0Att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
    proc.apvts, "drawbar0", drawbar0Slider);
```

For buttons (bool params):
```cpp
std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> vibratoAtt;
vibratoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
    proc.apvts, "vibrato", vibratoBtn);
```

---

## Custom LookAndFeel

Create one LookAndFeel subclass for the whole plugin. Install it in the editor constructor and uninstall in the destructor.

```cpp
// Header
class TuneBfreeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TuneBfreeLookAndFeel();

    // Override drawbar appearance
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;

    // Override rotary knob
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;

    // Override tab bar buttons
    void drawTabButton(juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
};

// In editor constructor:
setLookAndFeel(&lookAndFeel);

// In editor destructor:
setLookAndFeel(nullptr); // MUST reset before destruction
```

Key LookAndFeel_V4 methods to override for this project:

| Method | Used for |
|--------|---------|
| `drawLinearSlider` | Drawbar fader shape, colour, direction |
| `drawRotarySlider` | Effect knobs |
| `drawButtonBackground` | TextButton, ToggleButton styling |
| `drawTabButton` | Page navigation tabs |
| `getTabButtonBestWidth` | Tab width sizing |
| `drawTableHeaderBackground` / `drawTableHeaderColumn` | Tuning panel table |

---

## Layout Patterns

Use `juce::Rectangle<int>` manipulation in `resized()`. Avoid hardcoded pixel coordinates — derive all positions from `getLocalBounds()`.

```cpp
void MyEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop(40);      // header strip
    auto sidebar = r.removeFromRight(320);  // if sidepanel open

    // Header content
    logo.setBounds(header.removeFromLeft(160).reduced(4));
    tuningBtn.setBounds(header.removeFromRight(80).reduced(4));
    tabBar.setBounds(header);               // remaining header space

    // Main content area
    mainPage.setBounds(r);
}
```

For the drawbar row (9 equally-spaced faders):
```cpp
void DrawbarPanel::resized()
{
    auto r = getLocalBounds().reduced(8);
    auto switchRow = r.removeFromTop(28);
    upperBtn.setBounds(switchRow.removeFromLeft(70).reduced(2));
    lowerBtn.setBounds(switchRow.removeFromLeft(70).reduced(2));

    int faderWidth = r.getWidth() / 9;
    for (int i = 0; i < 9; ++i)
        drawbars[i].setBounds(r.removeFromLeft(faderWidth).reduced(3, 0));
}
```

For `juce::FlexBox` (alternative — useful when number of items varies):
```cpp
juce::FlexBox fb;
fb.flexDirection = juce::FlexBox::Direction::row;
fb.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
for (auto& db : drawbars)
    fb.items.add(juce::FlexItem(db).withFlex(1.0f).withMargin(3));
fb.performLayout(r.toFloat());
```

---

## CC Number Display

ROADMAP requirement: all controls show their associated MIDI CC number and channel unless absent. Implement as a small label below or beside each control. Example:

```cpp
struct LabelledControl : public juce::Component
{
    juce::Slider slider;
    juce::Label  ccLabel;

    LabelledControl(const juce::String& ccText)
    {
        addAndMakeVisible(slider);
        ccLabel.setText(ccText, juce::dontSendNotification);
        ccLabel.setFont(juce::Font(9.0f));
        ccLabel.setJustificationType(juce::Justification::centred);
        ccLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
        addAndMakeVisible(ccLabel);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        ccLabel.setBounds(r.removeFromBottom(14));
        slider.setBounds(r);
    }
};
```

---

## Open Stage Control Mockup Format

`roadmap/gui.json` is an **Open Stage Control** (OSC) session file. OSC is a browser-based WYSIWYG controller builder, useful for rapid layout prototyping. Its JSON format is human-readable but does not translate directly to JUCE — it is a design reference, not code.

### Key OSC widget types → JUCE equivalents

| OSC type | JUCE equivalent | Notes |
|----------|----------------|-------|
| `panel` | `juce::Component` (container) | Position/size via `top`, `left`, `width`, `height` |
| `fader` | `juce::Slider` | `horizontal: false` = vertical; `range: {min:8, max:0}` = inverted |
| `switch` | `juce::TextButton` row or `juce::TabbedButtonBar` | `values` dict maps labels to values |
| `modal` | `juce::TabbedComponent` page or `juce::SidePanel` | `popupWidth: "30%"` → SidePanel; `popupWidth: "100%"` → full-page tab |
| `textarea` | `juce::Label` | Static text display |
| `knob` | `juce::Slider` (RotaryVerticalDrag) | Not used in current gui.json |

### OSC mockup files (split by page, v2 — 10px grid, correct colours)

| File | Contents |
|------|---------|
| `roadmap/gui_play.json` | Play page — 9 drawbars (60px pitch), Leslie/Volume, Effects. Hammond colours per fader. Upper/Lower switch. |
| `roadmap/gui_config.json` | Config page — 9 drawbar ratio rows (numerator/denominator faders, 50px pitch). Load .cfg button. |
| `roadmap/gui_presets.json` | Presets page — preset list with mock rows. Load .pgm button. |
| `roadmap/gui_tuning.json` | Tuning sidepanel — MTS-ESP status, Last Played, Tuning Files (Load .scl/.kbm/Clear), Tuning Presets. |
| `roadmap/gui.json` | Original combined file (v1) — kept for reference, superseded by the split files above. |

All v2 files use a **10px grid** (all `top`/`left` coordinates are multiples of 10), minimum h:30 for interactive widgets, and minimum h:20 for static text at ≥11px font.

The OSC coordinates are design-reference positions — do NOT translate pixel-for-pixel to JUCE. Use them as proportion/grouping guides.

---

## Drawbar-specific Design Notes

- Each drawbar should have a **coloured cap** matching the Hammond convention (brown/white/black).
- The cap colour can be painted in a custom `drawLinearSlider` by reading a property set on the slider (e.g., via `setComponentID` or a simple colour array indexed by drawbar number).
- Drawbar labels below the cap: foot length (`16'`, `5⅓'`, etc.) and harmonic name (`Sub`, `Quint`, `Fund`, etc.).
- The **upper/lower switch** controls which drawbar set is visible/active. This requires two sets of APVTS parameters: `drawbar0`–`drawbar8` (upper) and separate lower params if/when lower manual is implemented.
- **Crossover / split point**: A slider controlling the MIDI note split between upper and lower manual. Compare Nord split: a single note that routes lower notes to the lower set. ROADMAP calls for a gradual crossover (like Nord or Korg Prologue).

---

## Tuning Side Panel Content

Two sub-views, switchable by a toggle inside the panel:

**Status view** (default):
- MTS-ESP connection indicator (green dot + scale name)
- Scale period: value, whether inferred / specified in MTS-ESP / aperiodic
- Multi-channel vs. single-channel (omni) display
- Last-played note frequency in Hz
- Interval between last two notes in cents
- Local .scl / .kbm file loading (current implementation)

**Tuning presets view**:
- List of tuning presets (separate from instrument presets in .pgm)
- Toggle from Status view via a button within the panel

Map to: `juce::TabbedComponent` or simple `Component::setVisible` swap inside the SidePanel content component.

---

## Open Stage Control — What works and what doesn't (from testing gui.json v1)

Tested in OSC 1.30.3. Screenshots in `roadmap/screenshots/`.

### What works
- Overall structure renders correctly: dark background, amber accent, nav bar, three panels (drawbars / leslie+volume / effects) side by side
- Upper/Lower switch, Slow/Brake/Fast switches, V1–C3 vibrato type switch all render fine
- Tuning side panel (30% modal) opens and shows all sections correctly
- Configuration and Presets full-screen modals open correctly
- Section headers in amber, muted CC labels, split-point fader — all visible

### Known OSC quirks / problems

**`toggle` label not rendering**: Toggle buttons show as "−" instead of their label text (e.g. "Load .scl…", "On"). In OSC, the `toggle` widget's `label` property seems not to render the button face text. **Fix**: use `html` property instead: `"html": "Load .scl..."`. Or switch to a `button` widget type.

**`colorKnob` overridden by global `colorWidget`**: The per-fader `colorKnob` property is ignored when a `colorWidget` is set at root level — all fader knobs inherit the global amber. **Fix (confirmed working)**: Set `"colorWidget": "rgba(R,G,B,1)"` explicitly on each individual fader widget. The per-fader `colorWidget` overrides the root. Setting the root `colorWidget` to `"auto"` does NOT fix it. Example for brown drawbars: `"colorWidget": "rgba(139,90,43,1)"`. See `gui_play.json` fader widgets for the working pattern.

**Modal content title overlaps popup header**: The OSC modal popup renders its own title bar (from `popupLabel`). If you also put a title label as the first widget inside the modal content, it duplicates. **Fix**: Remove the title textarea from inside the modal content, or set `popupLabel: ""` and keep the internal label.

**Configuration page: only DB1 row shown**: The JSON only contained one drawbar ratio row (DB1/16'). Need all 9 rows at `top: 70 + i*52` spacing, i = 0..8.

**Drawbar fader track is thin**: The vertical faders look like thin amber lines — the knob cap is small and the track is barely visible. The `knobSize: 20` is the handle, but the track color comes from `colorWidget`/`colorFill`. Acceptable for a prototype; revisit when building JUCE version.

**10px grid alignment**: OSC has a grid-snap editor feature. To make it usable, all `top` and `left` coordinates should be multiples of 10. In v2 files, drawbar pitch is 60px (6 grid units), row pitch is 50px (5 units), section gaps are 10px. When editing the JSON manually, always maintain this alignment.

**Widget sizing and readability**: Widgets whose bounding box is too small will clip text even if the font fits. Minimum sizes that work reliably:
- Static text (`textarea`): h:20, font-size ≥ 11px; for footage labels (4+ chars with fraction glyphs) use h:20, font-size:14px, w:60
- Interactive controls (`fader`, `button`, `switch`): h:30 minimum; drawbar faders are w:60 for adequate touch/click area
- Drawbar cell pitch: 60px minimum — at 40px the footage labels overlap

### Coordinate system reminder
- Root and top-level panels: `top`/`left` are absolute within the OSC canvas
- Widgets inside a panel: `top`/`left` are relative to the panel's top-left corner (not the viewport)
- Panel padding: when `innerPadding: false`, coordinates start at the panel border edge

### Layout from screenshots (actual rendered sizes)
Canvas appears ~1500px wide in browser. The 900px mockup coordinates scale to fill width. When implementing in JUCE at 700–900px, the proportions should translate well.

---

## References

- Crumar/GMLab D9X — example clonewheel UI to draw inspiration from
- setBfree GUI (`b_synth/ui.cpp`) — upstream reference; uses OpenGL, not for direct reuse
- Surge XT tuning panel — reference for Status sub-view layout (see ROADMAP link)
- Open Stage Control docs: https://openstagecontrol.ammd.net/docs/
