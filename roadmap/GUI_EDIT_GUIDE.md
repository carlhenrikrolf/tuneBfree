# Editing the tuneBfree GUI by hand

A practical guide to changing the plugin's interface yourself. It assumes you
know basic C++ (classes, pointers, lambdas) but **not** JUCE. Everything GUI
lives in two files:

- `plugin/PluginEditor.h` — declarations (the list of widgets and classes)
- `plugin/PluginEditor.cpp` — the actual code (setup, layout, drawing)

You do **not** need to touch any other file to change how the GUI looks.

---

## 1. How to build and see your change

```bash
cmake --build build --config Release --target tuneBfree_Standalone
open build/tuneBfree_artefacts/Release/Standalone/tuneBfree.app
```

Quit the app before rebuilding, and quit + relaunch to see changes. If you break
something, `git diff plugin/PluginEditor.cpp` shows what you changed and
`git checkout plugin/PluginEditor.cpp` throws it all away.

The build prints `error:` lines with a file and line number if the C++ doesn't
compile. Warnings are fine to ignore.

---

## 2. The one JUCE idea you must understand

In JUCE, every control (a knob, a button, a text label) is a **Component**.
Building a UI is four separate steps, and they live in four different places.
This split is the thing that's unfamiliar coming from most other GUI toolkits:

| Step | Where | What it does |
|------|-------|--------------|
| 1. **Declare** | `PluginEditor.h` | "this screen has a knob called `driveKnob`" |
| 2. **Configure** | constructor in `.cpp` | set its range, default, make it visible |
| 3. **Position** | `resized()` in `.cpp` | give it an x/y/width/height rectangle |
| 4. **Draw** | `LookAndFeel` in `.cpp` | how a knob is *painted* (shared by all knobs) |

So to add one knob you edit three places (declare, configure, position). To
change how *all* knobs look, you edit one place (step 4). Keep this table in mind
— "where do I change this?" almost always maps to one of these rows.

There are two screens, each its own class in the file:

- **`DefaultPage`** — the main screen (drawbars, effects, etc.)
- **`TuningSidePanelContent`** — the grey panel that slides in on TUNING

and a small wrapper, **`TuneBfreeAudioProcessorEditor`**, which is just the amber
header bar plus the `DefaultPage`.

---

## 3. The most common edits (cookbook)

### Change a colour

Top of `PluginEditor.cpp`, the block of `kBg`, `kAmber`, etc. These are the only
colours used anywhere. `0xffRRGGBB` is opaque (the `ff`), then red/green/blue in
hex. Change `kAmber` and the header, active buttons, and knob rings all change at
once.

```cpp
static const juce::Colour kAmber { 0xffff8c00 };  // <- edit this hex
```

### Change a button's or label's text

Button text is set where the button is **declared** in `PluginEditor.h`:

```cpp
juce::TextButton vibratoBtn { "VIBRATO" };   // <- change the string
```

A few buttons set their text in code instead (search `setButtonText` /
`setText`). Caption labels (DEPTH, DRIVE…) are set in the constructor via
`styleCaption (driveLabel, "DRIVE")` — change the second argument.

> If your new text has a non-ASCII character (´, ⅓, —, °) it will render as
> garbage unless you wrap it: `utf8 ("…")`. There is a `utf8()` helper at the top
> of the file for exactly this. Plain ASCII needs nothing.

### Change the font or a text size

All text goes through one helper, `uiFont (size, bold)`, near the top of the
`.cpp`. To change the typeface for the whole plugin, edit the `.withName("Futura")`
there. Individual sizes are the numbers passed to `uiFont(...)` and `styleCaption`.

### Resize the whole window

In the editor constructor (bottom of the file):

```cpp
setSize (740, 430);   // width, height
```

The header is 44px tall; everything else scales from the layout constants below.

### Move or resize widgets on the main page

This is the big one. **All positions for the main page come from named constants
at the top of `DefaultPage::resized()`** — you rarely need to touch the
positioning code itself, just these numbers:

```cpp
const int margin   = 14;  // gap around and between sections
const int knob     = 62;  // diameter of EVERY knob
const int btnH     = 30;  // height of a standard button
const int capH     = 14;  // height of a small caption label
const int gap      = 5;   // gap between the stacked buttons of a switch
const int twoWayW  = 56;  // width of each half of a 2-way switch
const int switchW  = twoWayW * 2 + 4;  // width of the 3-way switches
const int timbralW = 122; // width of the timbrality column
const int effectsW = 158; // width of the effects column
const int drawbarH = 220; // drawbar length
```

Want bigger knobs? Change `knob`. Shorter drawbars? Change `drawbarH`. Wider
tuning side of the screen? Change `timbralW` / `effectsW` (the tuning panel
automatically covers `timbralW + effectsW`). Because every knob uses the single
`knob` constant, they stay equal — don't give a knob a custom size or it will
look different from the rest (see Gotchas).

To understand the positioning code itself, read section 4.

### Change drawbar colours

`TuneBfreeLookAndFeel::drawbarColour(int i)` — a `switch` mapping drawbar index
0–8 to a colour. Edit which indices return `kRed` / `kWhite` / `kAmber`.

### Change which percussion option is the "top" of each switch

The percussion is four 2-way switches. The top/bottom button of each is set in
`DefaultPage::resized()`:

```cpp
juce::TextButton* tops[4] = { &percOnBtn,  &percFastBtn, &percSoftBtn, &perc2ndBtn };
juce::TextButton* bots[4] = { &percOffBtn, &percSlowBtn, &percHardBtn, &perc3rdBtn };
```

Swap entries to reorder. The labels themselves are in `PluginEditor.h`.

---

## 4. Reading the layout code (`resized()`)

JUCE positions widgets by **slicing up a rectangle**. There's no grid system;
you start with the whole area and chop pieces off the edges. The key methods on a
`juce::Rectangle`:

```cpp
auto area = getLocalBounds();        // the whole component, as a rectangle
auto top  = area.removeFromTop (44); // CUTS 44px off the top:
                                     //  'top'  is now that 44px strip
                                     //  'area' is now everything BELOW it
```

`removeFromTop / removeFromBottom / removeFromLeft / removeFromRight` all work the
same way: they return the slice **and** shrink the original. So you lay out a
column by repeatedly cutting strips off the top:

```cpp
label.setBounds (col.removeFromTop (14));   // caption gets the top 14px
knob.setBounds  (col.removeFromTop (62));   // knob gets the next 62px
```

Two more helpers used a lot here:

- `area.reduced (10)` — shrinks the rectangle inward by 10px on all sides (a
  margin). `reduced (10, 0)` = 10px left/right only.
- `cell.withSizeKeepingCentre (62, 62)` — returns a 62×62 rectangle centred
  inside `cell`. This is how a fixed-size knob gets centred in a flexible space.

**The trick for "no empty space at the edges":** anchor fixed things to the top
*and* the bottom, and let the middle absorb the slack. In `DefaultPage::resized()`
the drawbars are cut from the **bottom** (`removeFromBottom`) and the control
strip from the **top**, so any extra height ends up as a gap *between* them, never
as a dead band at the window edge. The right side does the same: DRIVE/REVERB at
the top, CROSSFADE/EXPRESSION at the bottom, LESLIE/SPLIT centred in what's left.

Once you internalise "cut strips off a shrinking rectangle," the whole `resized()`
reads top-to-bottom like a recipe.

---

## 5. Adding a new control (full example)

Say you want a new "VOLUME" knob. Four edits:

**1. Declare it** in `PluginEditor.h`, inside `class DefaultPage` next to the
other knobs:

```cpp
juce::Slider volumeKnob;
juce::Label  volumeLabel;
```

**2. Configure it** in the `DefaultPage` constructor in the `.cpp` (copy an
existing knob block like `driveKnob`):

```cpp
volumeKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
volumeKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
volumeKnob.setRange (0.0, 1.0);
addAndMakeVisible (volumeKnob);              // <- without this it won't show
styleCaption (volumeLabel, "VOLUME");
addAndMakeVisible (volumeLabel);
```

`addAndMakeVisible` is the JUCE call that actually attaches the child to the
screen — forgetting it is the #1 reason "my widget doesn't appear."

**3. Position it** in `DefaultPage::resized()`. Find a column rectangle and cut a
slot for it, e.g.:

```cpp
someColumn.removeFromTop (margin);
volumeLabel.setBounds (someColumn.removeFromTop (capH));
volumeKnob.setBounds  (someColumn.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
```

**4. (Later) make it do something.** Right now the GUI is visual only — controls
aren't wired to the audio engine yet. When that day comes you connect a control
to a parameter with an *attachment* (see section 7). For a pure-visual control,
skip this.

For a button instead of a knob, declare a `juce::TextButton`, and if it's part of
a mutually-exclusive set use `makeRadioGroup ({ &a, &b, &c })` (see the existing
switches).

---

## 6. How drawing works (LookAndFeel)

You'll notice the knobs and drawbars don't look like default JUCE controls.
That's `TuneBfreeLookAndFeel`. A LookAndFeel is a class JUCE asks "how do I paint
a knob / a button / a slider?" — so one method controls the appearance of *every*
control of that type. The relevant methods:

| Method | Paints |
|--------|--------|
| `drawRotarySlider` | every knob (the ring + pointer) |
| `drawLinearSlider` | the drawbars (track + coloured cap) |
| `drawButtonBackground` | every button (fill + outline) |
| `getTextButtonFont` | button text size |

These use `juce::Graphics& g` — think of `g` as a canvas with calls like
`g.setColour(...)`, `g.fillRoundedRectangle(...)`, `g.fillEllipse(...)`. If you
want, say, square knobs instead of round, this is the only place you'd change it.
You don't need to understand the maths to tweak colours or thicknesses.

The header bar itself is painted in `TuneBfreeAudioProcessorEditor::paint()`
(the `g.fillRect` filled with `kAmber`).

---

## 7. (For later) wiring a control to the sound

When controls get connected to the engine, each one is tied to a *parameter* with
an **attachment**. You declare the attachment as a member, then create it in the
constructor:

```cpp
// member in PluginEditor.h:
std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAtt;
// constructor:
driveAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
    proc.apvts, "overdrive", driveKnob);
```

The attachment keeps the slider and the parameter in sync both ways
automatically. Parameter IDs (`"overdrive"`, `"drawbar0"`, …) are listed in
`CLAUDE.md`. This is Phase 3 work — ignore it while the GUI is visual-only.

---

## 8. Gotchas that will bite you

1. **Widget doesn't appear** → you forgot `addAndMakeVisible (thing)` in the
   constructor, or you never called `setBounds` on it in `resized()`.
2. **Garbled text** (`â€¦`) → a non-ASCII character not wrapped in `utf8("…")`.
3. **A knob is a different size from the others** → you gave it non-square bounds.
   Always use `.withSizeKeepingCentre (knob, knob)` for knobs.
4. **Two things overlap** → in `resized()` you cut the same space twice, or the
   window is too short for everything to fit. Whichever widget was added *later*
   (`addAndMakeVisible` order) draws on top, so the other looks "missing."
5. **`juce::Font(16.0f)` won't compile cleanly** → it's deprecated in JUCE 8; use
   the `uiFont(...)` helper instead.
6. Editing `.h` (declarations) usually triggers a longer rebuild than editing
   `.cpp` — normal.

---

## 9. Where things are (quick map)

| You want to change… | File · place |
|---------------------|--------------|
| Any colour | `.cpp` top · `kBg`/`kAmber`/… |
| Font / text sizes | `.cpp` top · `uiFont()` and the `uiFont(n)` calls |
| Window size | `.cpp` · editor constructor · `setSize` |
| Header (title, TUNING button) | `.cpp` · `TuneBfreeAudioProcessorEditor` ctor + `paint` |
| Main page layout / sizes | `.cpp` · `DefaultPage::resized()` constants |
| Tuning panel layout | `.cpp` · `TuningSidePanelContent::resized()` |
| Drawbar colours | `.cpp` · `drawbarColour()` |
| How knobs/buttons are drawn | `.cpp` · `TuneBfreeLookAndFeel::draw…` |
| Add / rename a widget | `.h` (declare) + `.cpp` ctor (configure) + `resized()` (position) |

The taste spec these were built from is `roadmap/GUI_SPEC.md`; reference mockups
are `roadmap/default_page.png` and `roadmap/tuning_sidepanel.png`.
