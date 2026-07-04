# Polish

## GUI

Help me install Melatonin as a submodule so I can make edits to the GUI more easily.scr

Some of the newly added parameters will cut the currently playing tone, maybe indicate that more clearly in the ui.

The exact GUI implementation below needs not be followed literally. I'm trying to give ideas.
It's more that I have certain design pronciples I want followed and they are the ones that typically underly virtual instruments.
- Use a fixed colour palette throughout.
- Use a limited number of designs for knobs, sliders, buttons, encoders, and toggles. Somewhere between 1 and 3 for each of those categories.
- Make widgets the same size and colour and design in general.
    - It is ok to sidestep this if there is some radically different kind of function
    - Or if there is some limitation like one parameter being "continuous" (I count 128 values as continuous) or discrete (like less than a dozen values)
- Imagine that there is some kind of grid that all widgets and all pieces of text are aligned to. The grid can have some rows that are higher than others and some columns that are wider than others.
- Preferably consistency translates across pages and side panels, but when they cannot easily be made to do so, at least it should be consistent within one page. For example I would like the knobs to be more consistent within pages. (Fable wasn't allowed to make changes to the play page so that may be why it was difficult to make it consistent, but now Fable may do such modifications. Opus should follow my instructions more carefully though.)
- There should not be empty spaces at the borders of the plugin. There shouldn't be unnecessary epty space at all, but to the extent that it is unavoidable, it should be more in the middle.
- Widgets should be places in different section according to function and where they intervene on the signal chain.
- The interface should in general illustrate the signal chain by having earlier signal processing widgets more to the left and then step by step going to the right. This is common in synthesizers.

A useful exercise for consistency is having more or less global variables for the paelette (already done), sizes, fonts, grid-related variables.

### Right click

I think surge xt has a nice rightclick menu

- parameter name
    - ? for more info about parameter. (they do browser, which i don't want to do here, but maybe there is another solution like a small text box or something)
- Edit value: `<value>`
- ~~Extend range~~ not needed
- ~~Assign modulation from~~ not needed
- midi channel
    - 20 channels bins
        - number and general midi name
    - omni or channel
- midi learn

Many of my comments here are really what different parameters do, so part of solving that is having such info boxes available via right click.

### Header

Buttons should be red when marked just like the tuning panel.
Move the panic button.
Where the panic button was, there should be a PRESET side panel. It should have the same place as the TUNING sidepanel. Since it has the same not both of them can be active simultaneously but one switch for the other. Note that TUNING–PRESET is not quite a toggle as both can be inactive at the same time even though they can't both be active.

I suggest something like this layout for the header

<table>
    <td>tuneBfree</td>
    <td> <!-- space --> </td>
    <td>
        <input type="radio", name="page">PLAY</input>
        <input type="radio", name="page">TINKER</input>
        <input type="radio", name="page">ROTOR</input>
    </td>
    <td> <!-- space --> </td>
    <td>
        <button>!</button>
        <button>🔊</button>
    </td>
    <td> <!-- space --> </td>
    <td>
        <button>PRESET</button>
        <button>TUNING</BUTTON>
    </td>
</table>

"🔊" is a button for a volume slider appearing just like in the top menu on MacOS or Debian.
"!" is the panic button.

### Tuning Sidepanel

<!-- Rename FILE into SCALA.
Grey out the .scl and .kbm selectors when tuning menu not in use.
Grey out note-always unless MTS ESP is active (it could plausibly be used for MPE as well, but that is not relevant here.) Also depending on the sysex message, it should automatically shift even if its greyed out (as an indicator). MTS ESP has a tool for reading sysex messages. Could you see if it is actively working? There is alse the spec in roadmap/refs/midi.
Some of these changes are already suggested in TUNING_PANEL.md. Could you go ahead and implement those changes.  -->

Could you implement the changes that I proposed in TUNING_PANEL.md that have not yet been implemented.

I have had a change of mind regarding the indicator where you see how many cents between the last two notes. could it be on the form `-(702.23 + 2x1200) c` or `(315.00 + 1200) c` or `-498.78 c` as a a few examples. That is, you have a cents value less than an octave and then you add how many octaves there are next to it. It makes it easier to read. I proposed a different layout in TUNING_PANEL.md, but with these exact phrasings, it's better to stick with the old (current) one.

Tuning menu should be independent from PLAY-TINKER-ROTOR.
Now it disappears when you switch page—it shouldn't if it's active.
It moves you back to the PLAY page when you turn it on—it should stay on the page.

### Preset Sidepanel

here's a sketch on a preset panel. Maybe you can implement a simplified version for basic functionality as I haven't completely thought this through.

<table>
    <tr>
        <td>
            PROGRAM
        </td>
    </tr>
        <tr>
            <td>
            <select multiple>
                <option>1 Jimmy Smith</option>
                <option>2 Green Onions</option>
                <option>3 All Stops Out</option>
                <option>4 Smoke On the Water</option>
                <option>5 A Whiter Shade of Pale</option>
                <option>6 Reggae</option>
                <option>7 Bossa Nova</option>
            </select>
        </td>
    </tr>
    <tr>
        <td>
            options for adding, removing, reordering, renaming, saving and loading .pgm files and (?) .cfg files (maybe on banks?).
        </td>
    </tr>
        <td>
            BANKS
        </td>
    </tr>
    <tr>
        <td>
            <select multiple>
                <option>1 B3</option>
                <option>2 Space Organ</option>
                <option>3 Farfisish</option>
            </select>
        <td>
    </tr>
    <tr>
        <td>
            similar options as above
        </td>
    </tr>
</table>

`<select multiple>` is used more for graphical purposes, you can ofc not have several programs (banks) selected at once.

### Play Page

The other pages have names for their sections and values underneath their knobs. for consistency, we may want to do similarly here.

#### Drawbars Section

Like in the tinker page, it would be nice to indicate the error to the just intonation pitch in cents.

#### Vibrato/Chorus and Percussion section

Change vibrato into the following (laid out like in Crumar D9X)

<table>
    <tr>
        <td><input type="radio">V3</input></td>
        <td><input type="radio">C3</input></td>
    </tr>
    <tr>
        <td><input type="radio">C2</input></td>
        <td><input type="radio">V1</input></td>
    </tr>
    <tr>
        <td><input type="radio">V2</input></td>
        <td><input type="radio">C1</input></td>
    </tr>
</table>

Or just the discrete labelled knob as in Hammond B3/setbfree
Together with this, have just a simple onoff switch.

The percussion section is pretty much unchanged, but when lower is selected it should be greyed out.

#### Bitimbral Section

Rename the learn button into KEYPRESS or soemthing.
Collect it with the other buttons in the section.
One knob shows the key—this should be replaced by the frequency in Hz.
The other knob should show the interval in cents.

Now the keyboard makes sound when you press the keys after the KEYPRESS is on. The keys should be silent.

#### Effects

There is only one reverb knob, but I believe that there are more paramters. I assume the current one is a feedback knob? Maybe it could be good to have a mix knob as well—I think in the implementation that dry and wet are separate, not superkeen on that although you might add such fine details to tinker.
I don't really like the reverb, so it is also worth adding other third party reverbs. For example freeverb should also be a Schröder reverb? Just a nicer sounding implementation? We can keep the old reverb and have a menu for choosing an alternative one. A spring reverb emulation may be more authentic? But I don't really know any good opensource ones. Maybe there are some impulse responses, but those tend to run poorly on raspberry pi and the cabinet already has an impulse response so not sure about adding another one for reverb, but let me know what you find.

Related to this is that since Naren Ratan forked tuneBfree from setBfree, setBfree has seen updates in which the various effects have been taken out of the src directory and turned into their own plugins. It could be a good idea to merge those updates in somehow?

#### Leslie section
I think there should be a bypass option in here.

I would also want to be able to route external audio through the leslie or maybe the entire effects chain. Audio input can be set in the options part of the juce standalone and I assume from somewhere in the daw for plugins.

### Tinker Page

#### Scanner
seems fine

#### Key Click
So many settings, but seems fine.
Maybe some other gui than dropdown could be used, on the other hand, we might want to be able to use that space for something.

#### Percussion
seems fine

#### Crosstalk
bit hard to understand the labels.
why is turning down xformer removing drawbars? or so it seems?

#### Harmonics Section

I don't really understand how to set the ratios.
The error you see is not what is in the tuning setting.
The default ratios are perfect, but that's not actually the case in a Hammond, where it is the gear60 or gear50 approximation to 12edo.
That makes me wonder: How does this pitch finetuning actually work with tonewheels sharing pitch and so on?
As for the interface, I think it's better like in scala, that you write an entry for each drawbar finetuning. If it contains a "." then it's cents. (It should automatically complete with a " c" after the number.) If it contains a "/" then it's ratio.
(Ratio doesn't complete with any unit symbol).
It could make sense to flip the direction of the text by 90 degress to make it align better with the drawbars.
There should also be a button for each where you switch between the drawbar following the general tuning and the drawbar being what you fine tuned it on. When you switch between them it is the case that the finetuning and the genral tuning remember their respective states.
It would be nice if input field + button + footage label + error had the same height as, in the play page, the drawbar + footage label + error.

#### Tone Section
What is the reset button supposed to do?
Not sure we need it?

The wave selector is nice. I think we could have a three-way toggle for it isntead of a drop down menu.

bass and treble seem fine but they cut of the current note.

### Rotary Page (-> Rotor Page)

Should be renamed from ROTARY to ROTOR.

#### Horn Motor
Haven't checked in detail, but see drum motor.

#### Drum Motor
Slow and fast seem to work if you set them and then use the chorale-stop-tremolo, but they don't seem to work as they should live, they seem to stop the motor if changed then?

Acceleration and deceleration seems to work as they should.
I'm not sure what brake is supposed to do.

#### Mic and cabinet

Angle I don't hear a lot of difference but I guess it's a rather subtle effect.

It sounds as though dist (-ance?) is the other way around. what is labelled as 9 cm sounds far away (weak effect) and vice versa for 200 cm.

Horn width is weird. It sounds the most along the middle (0), with the effect disappearing for both the maximum and minimum values.
Unit?

Drum width is the same as horn width—here it's even more pronounced.

#### Speed

Works as it should.
The bypass option should probably be moved to PLAY.

#### Filters
They seem to work as expected although I only tested briefly.

#### Mix
Not quite sure what these do? I tried to set mix to the extremes to see if the leslie vanishes, but I couldnt find such a spot.


## Remaining tickets OPUS has truggled with

According to priority:
1. There is still a sharp cutoff when it comes to bitimbral percussion. Opus thought it was difficult, but maybe you could have another look at it because it seems like it should be solveable to me.
2. sysex implementation (and midi 2.0). Maybe not so much struggle as having been left for later.
3. APPROXIMATION_REPORT.md—seems unintuitive to me, shouldn't it be simpler? Do you have any suggestions?

## Misc
Maybe the snapshot tool should be in the scripts directory?