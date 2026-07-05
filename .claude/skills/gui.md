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

**As implemented (2026-07-05)** in `plugin/PluginEditor.{h,cpp}` (740×430):

| Class | Role |
|-------|------|
| `TuneBfreeAudioProcessorEditor` | Header: title, PLAY/TINKER/ROTOR radio (red = active, TUNING-style; uniform 6px gaps), "!" panic + drawn-speaker volume popup (master_volume), CONTROL (disabled placeholder), TUNING. Owns the pages, the EDITOR-LEVEL tuning overlay (survives page switches), LookAndFeel, 15 Hz sync timer, dev hooks (TUNEBFREE_PAGE / TUNEBFREE_TUNING / TUNEBFREE_SNAPSHOT). |
| `DefaultPage` (PLAY) | Amber group titles; B3 vibrato dial (vibrato_type 0-5 = V1 C1 V2 C2 V3 C3) + PER-MANUAL ON/OFF (vibrato / lower_vibrato); percussion 2-ways (greyed under LOWER); drawbars + live JI-error row; TIMBRALITY (UPPER/LOWER drives `active_manual` routing, BITIMBRAL, KEYPRESS = silent split learn); SPLIT (Hz read-out) / CROSSFADE (cents); EFFECTS w/ value labels; LESLIE 3-way (greyed while bypassed) + BYPASS (standard amber). |
| `TinkerPage` | Engine physics: SCANNER / PERCUSSION / PREAMP / KEY CLICK / CROSSTALK / TONE (EQ spline + WAVE 3-way) + HARMONICS: rotated Scala-style entries ("3/2", "702.23 c") on the PLAY drawbar x-grid, A\|C toggles (A greys the entry), ALL AUTO. Red LEDs after titles of rebuild-scope groups (changes cut sounding notes). |
| `RotaryPage` (ROTOR) | 3 dense rows: per-rotor [motor knobs → speed 3-way → voicing filter]; row 3 = MIC & CABINET (incl. horn level/leak) \| drum filter. Speed switches grey while bypassed. |
| `TuningSidePanelContent` | Overlay: freq read-outs (octave-folded cents `-(702.23 + 2x1200) c`), STATUS, SETTINGS — source dropdown (SCALA renamed from FILE), CHANNELS popup, ONE combined .scl+.kbm multi-select loader + FILES popup (list + CLEAR ALL), NOTE ON/ALWAYS (editable only under MTS; greyed SYSEX indicator follows realtime-vs-dump). Loader greyed unless SCALA. |
| `LabelledKnob` | Standard TINKER/ROTOR control: caption / knob / editable value box. `init(apvts,id,...)`; precision must be set AFTER the attachment (it installs the param's raw textFromValueFunction) + `updateText()`. |
| `ParamMenuAttachment` / `showParamMenu` | Right-click on any parameter control: NAME header / EDIT VALUE (type-in CallOutBox) / INFO (wrapped text; `paramInfoText` registry covers all ~112 params, incl. the red-LED explanation). Drawbar menus resolve upper/lower dynamically. MIDI-learn deferred to the CONTROL phase. |

Cross-page gotchas: layout constants `kPageMargin`=14 / `kRightColW`=280 must
match DefaultPage's own; screenshots via the sandbox-safe `tuneBfree_snapshot`
tool (scripts/SnapshotTool.cpp), NEVER macOS screencapture; Melatonin inspector
auto-enables with the submodule (cmd+I); press-state tints in
drawButtonBackground (header→red, panels→amber; disabled header buttons keep a
SOLID black fill — translucent black over amber renders brown); units "Hz" not
"HZ"; value labels white uiFont(10), never amber bold.

(Sections below describe the 2026-06 single-page state + mockup workflow —
palette/spec/history still valid; where they disagree, trust the table above
and the code.)

Config & Presets pages are **not built yet** (Phase 3 continues). When added,
prefer a `setVisible` swap of child pages over `TabbedComponent` to keep the
header bespoke.

### Tuning panel = child-overlay, NOT juce::SidePanel

`juce::SidePanel` was tried and **abandoned**: it always covers the parent's
full height (you cannot keep the header visible above it) and draws its own
title bar. Instead the panel is a plain child `Component`:

```cpp
// In DefaultPage:
TuningSidePanelContent tuningContent;            // member
addChildComponent (tuningContent);               // added LAST = highest z-order, hidden
// resized(): give it the right region's bounds so it's ready when shown
tuningContent.setBounds (getLocalBounds().removeFromRight (rightW + margin * 2));
// toggle:
tuningContent.setVisible (! tuningContent.isVisible());
```

It covers exactly the right region, so the drawbars + vibrato/percussion strip
stay visible (per GUI_SPEC: "leaves the drawbars and envelope and lfo sections clear").

The mockups live at `roadmap/default_page.png`, `roadmap/tuning_sidepanel.png`
(rendered from `roadmap/gui.json`, Open Stage Control format — see below).
`roadmap/GUI_SPEC.md` is the user's taste spec; `roadmap/GUI_EDIT_GUIDE.md` is
the hand-editing tutorial.

---

## Colour Palette

Inspired by Crumar/GMLab (dark mode, dimly-lit-venue vibe). Restricted to
amber / black / white / red / grey. Amber is the main accent **and** the header
background; near-black is the main background. **These are the values actually
implemented** in `plugin/PluginEditor.cpp` (top of file) — edit them there to
re-theme the whole plugin.

```cpp
static const juce::Colour kBg     { 0xff121212 }; // main background
static const juce::Colour kPanel  { 0xff242424 }; // tuning panel + info boxes
static const juce::Colour kBtn    { 0xff080808 }; // inactive button fill
static const juce::Colour kAmber  { 0xffff8c00 }; // accent + header + active state
static const juce::Colour kRed    { 0xffcc2a2a }; // active TUNING + two drawbars
static const juce::Colour kWhite  { 0xffe8e8e8 }; // main text + three drawbars
static const juce::Colour kGrey   { 0xff707070 }; // muted text (captions)
static const juce::Colour kGreen  { 0xff4cce5c }; // "connected" status only
static const juce::Colour kBorder { 0xff3a3a3a }; // box outlines
```

### Drawbar colours (custom, NOT standard Hammond)

The user chose a custom scheme (see GUI_SPEC.md). Implemented in
`TuneBfreeLookAndFeel::drawbarColour(int)`:

| Drawbar | Footage | Colour |
|---------|---------|--------|
| 0 | 16'  | red   |
| 1 | 5⅓'  | red   |
| 2 | 8'   | white |
| 3 | 4'   | white |
| 4 | 2⅔'  | amber |
| 5 | 2'   | white |
| 6 | 1⅗'  | amber |
| 7 | 1⅓'  | amber |
| 8 | 1'   | white |

(Do not "correct" these to brown/black Hammond colours — the custom scheme is intentional.)

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

### Radio groups (mutually-exclusive switches)

Every switch on the page (vibrato/chorus/off, leslie, mono/poly, the four
percussion toggles) uses one helper instead of hand-written cross-clearing:

```cpp
static void makeRadioGroup (std::initializer_list<juce::TextButton*> list)
{
    std::vector<juce::TextButton*> group (list);   // copy the pointers
    for (auto* b : group)
    {
        b->setClickingTogglesState (true);
        b->onClick = [group, b] {
            for (auto* other : group)
                other->setToggleState (other == b, juce::dontSendNotification);
        };
    }
}
// usage: makeRadioGroup ({ &vibratoBtn, &chorusBtn, &modOffBtn });
```

UPPER/LOWER is the exception — it also swaps manual state, so it keeps a bespoke
`onClick`. Don't run `makeRadioGroup` on it (that would overwrite the handler).

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

## JUCE 8 gotchas & lessons (learned building this GUI)

These cost real iteration time. Read before touching `PluginEditor.cpp`.

1. **UTF-8 string literals get mangled.** Passing a `const char*` with multi-byte
   UTF-8 (the ' fractions ⅓⅔⅗, em-dash —, ellipsis …, middle-dot ·) straight to
   `juce::String`/`setText`/`setButtonText` renders garbage like `â€¦`. Wrap every
   non-ASCII literal: `juce::String (juce::CharPointer_UTF8 (s))`. We keep a
   `utf8()` helper and store the bytes as hex escapes (e.g. `"\xe2\x85\x93"`).

2. **`juce::Font(float)` is deprecated in JUCE 8.** Use
   `juce::Font (juce::FontOptions().withName("Futura").withHeight(h).withStyle("Bold"))`.
   We wrap this in `uiFont(size, bold)`. One font family, two weights → satisfies
   "≤3 fonts". Century-Gothic-like geometric sans (Futura on macOS; bundle a .ttf
   for the RPi build later).

3. **All knobs the same size requires SQUARE bounds.** `drawRotarySlider` derives
   the radius from `jmin(w,h)`. If each `setBounds` passes a different-width
   rectangle, every knob renders a different size (this was a visible bug). Fix:
   give every knob identical square bounds —
   `knob.setBounds (cell.withSizeKeepingCentre (kKnob, kKnob))`.

4. **`drawButtonBackground` must read the button's own colours**, not hardcode
   `kAmber`. Use `button.findColour (TextButton::buttonOnColourId)` /
   `buttonColourId`. Otherwise per-button theming (e.g. the TUNING button going
   RED when active via `setColour(...buttonOnColourId, kRed)`) silently does nothing.

5. **Small buttons clip their text** ("FAST" → "F..."). Override
   `getTextButtonFont` to scale the font to the button height
   (`uiFont (jmin (12.f, buttonHeight * 0.5f))`).

6. **Drawbar drag direction = reversed `NormalisableRange`, not a mirrored paint.**
   Painting the fill upside-down makes value 8 sit at the bottom visually, but the
   mouse still drags the "wrong" way. Give drawbars a reversed range (proportion 0
   → value 8 at the bottom) so drag direction AND fill match. See the lambda triple
   in the drawbar setup.

7. **Top-anchored + bottom-anchored stacks overlap when the window is too short.**
   The tuning panel lays items from the top AND anchors Hz/cents to the bottom; if
   their combined height exceeds the panel, they overlap and whichever was
   `addAndMakeVisible`d **later** draws on top (the earlier one looks "missing").
   Size the window so both stacks fit (we use 740×430), or guard the middle gap.

8. **Layout philosophy that worked** (matches the user's "no empty space at the
   edges, put slack between widgets" taste): name every size as a `const int` at
   the top of `resized()`; anchor fixed groups to top and bottom
   (`removeFromTop`/`removeFromBottom`); let the *middle* absorb leftover space.
   Use `withSizeKeepingCentre` to centre a fixed-size widget in a flexible cell.

## References

- Crumar/GMLab D9X — example clonewheel UI to draw inspiration from
- setBfree GUI (`b_synth/ui.cpp`) — upstream reference; uses OpenGL, not for direct reuse
- Surge XT tuning panel — reference for Status sub-view layout (see ROADMAP link)
- Open Stage Control docs: https://openstagecontrol.ammd.net/docs/
