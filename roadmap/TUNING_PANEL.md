# Tuning Panel

This is a more updated specification of the tuning panel than the mockup ([.png](tuning_sidepanel.png), [.json](gui.json)).
Note, however, that this only specifies the layout and the functionality—not the design, sizes, and colours!
For that, please refer to the [GUI_SPEC.md](GUI_SPEC.md).
This tuning panel is designed for tuneBfree—but the plan further ahead is that it should be applicable to any microtunable virtual instrument as a kind of template.

The tuning panel can handle different microtuning encodings, but not all of them are relevant to tuneBfree.
These are all the encodings.
- MTS ESP. MIDI tuning standard extrasensory perception. Used for computers. Relies on a library/shared object file that is edited by a master and queried regularly by a client. A more recent update of MTS ESP can infer the scale period, i.e. a frequency interval at which the scale repeats. See https://github.com/ODDSound/MTS-ESP (general) and https://github.com/baconpaul/mts-dylib-reference (raspberry pi) for more information.
- SYSEX. The original MIDI tuning standard—not to be confused with MTS ESP (although MTS ESP includes tools for interpreting sysex messages). There are numerous different messages, see https://www.midi.org for the full spec.
- FILE. There are numerous tuning file formats.[^TuningFiles] For now, let's focus on scala files (`.scl.`and `.kbm`), but a more complete list would be better further down the line. The `.scl` file contains the pitches of the scale. The `.kbm` file can be used to customize the mapping of pitches to midi notes. For multiple channels, you can use multiple `.kbm` files. If using multiple midi channels, the convention is to suffix them with `_1.kbm`, `_2.kbm`, ..., `_16.kbm`. See https://www.huygens-fokker.org/scala/ for more information.
- MPE. MIDI polyphonic expression. Uses pitchbend across the MIDI channels to microtune. General MIDI (GM1 and GM2) relies on similar principles and could also be used. Problem for tuneBfree is that the wavetable has a build step rather than having pitch updated continuously. therefore pitchbend is difficult to use without large changes. See https://www.midi.org for the full spec.
- MIDI 2.0. Spec still in development so hard to implement right now. See https://www.midi.org for the full spec.
- STANDARD. The standard tuning. A panic button for microtuning. Should in general be 12edo. setBfree has a standard of gear60 which is an approximation to 12edo.

<table style="border: 0px">
    <tr>
        <td rowspan="2">
            <input type="textarea" value="-1200 c" style="width: 2cm" readonly />
        </td>
        <td>
            <input type="text" value="220 Hz" style="width: 2cm" readonly />
        </td>
    </tr>
    <tr>
        <td>
            <input type="text" value="440 Hz" style="width: 2cm" readonly />
        </td>
    </tr>
    <tr>
        <td colspan="2">
            STATUS
        </td>
    </tr>
    <tr>
        <td colspan="2">
            <input type="text" value="Unnamed" style="width: 5cm" readonly />
        </td>
    </tr>
    <tr>
        <td>
            <input type="text" value="inferred period" style="width: 2cm" readonly />
        </td>
        <td>
            <input type="text" value="1200 c" style="width: 2cm" readonly />
        </td>
    </tr>
    <tr>
        <td colspan="2">
            <input type="text" value="21:25:38" style="width: 5cm" />
        </td>
    </tr>
    <tr>
        <td colspan="2">
            SETTINGS
        </td>
    </tr>
    <tr>
        <td>
            <select style="width: 2cm">
                <option>MTS ESP</option>
                <option>SYSEX</option>
                <option>FILE</option>
                <option disabled>MPE</option>
                <option disabled>MIDI 2.0</option>
                <option>STANDARD</option>
            </select>
        </td>
        <td>
            <button>CHANNELS</button>
        </td>
    </tr>
    <tr>
        <td>
            <input type="file" accept=".scl" style="width: 2cm"></input>
        </td>
        <td>
            <input type="radio" name="query">note on</input>
        </td>
    </tr>
    <tr>
        <td>
            <input type="file" accept=".kbm" multiple style="width: 2cm"></input>
        </td>
        <td>
            <input type="radio" name="query">always</input>
        </td>
    </tr>
</table>


Consider the sketch above.
It is in the form of a table, but the table cells are only there to illustrate how the widgets should be laid out on an invisible grid.
It is not to be visible in other words.
The design choices of the html elements are of no importance whatsoever—the design should follow the rest of the plugin, see e.g. [skill](../.claude/skills/gui.md), [spec](GUI_SPEC.md), and [roadmap](ROADMAP.md), as well as Claude's memory files.
Here follows comments on the layout, row by row:

1. Two columns:
    - The ratio between these two notes in cents, negative if descending.[^SurgeTuningEditor] If unknown "? c". (Widget should fill out the cell.)
    - Right. The frequency of the last note on (up) and the penultimate note on (down) in Hz. If unknown "? Hz"
2. Title marking the beginning of the microtuning status section. Note that the section just above is nameless.
3. The name of the tuning. If unknown the name should be "Unnamed". An MTS ESP master can set a string as the name of the tuning. MTS SYSEX messages can set a 16 ASCII character name for some of the messages. `.scl` files can specify the name of a tuning at the top of the file. MPE and pitchbend do not provide a tuning name. MIDI 2.0 I don't know. STANDARD should be 12edo in general but maybe Gear60 or Gear50 for tuneBfree.
4. The scale period in cents. ~~If no period, "None (x c)", where x is the interval between the lowest midi note and the highest midi note, i.e. the set of all specified notes is taken to be a period.~~[^PolyInferPeriod] It can be specified in MTS ESP or `.scl` files (the last specified pitch basically acts as a specification of the period). The second widget should say "inferred period" or "approximated period" or "specified period" or "no period".
5. Time stamp of when tuning was last updated. If using MTS ESP queries will happen continuously (maybe several times per second), so this will look like a ticking clock. That way you see that it's active. However, it can also geneeralise to other systems. For sysex it would be when the last mts sysex message was received. for mpe when the last pitchbend was received (or really last pitchbend or note on or cc or aftertouch, ...). For tuning files, it would be when the file was loaded into the plugin.
6. Title marking the beginning of the microtuning settings section.
7. Two columns.
    - Left. Which of the microtuning encoding schemes used. For each the last state should be saved so that you can toggle back to it.
    - Right. A popup window to select channels, see below.
8. Two columns.
    - Left. Upper is uploading the scale file, e.g. `.scl`. Should probably say SCALE somewhere. Lower is the mapping file. Should probably say "MAPPING" or "MAP". Can select directory of .kbm files or multi-selection of .kbm files. if so `_<i>.kbm` files will be attached to midi channel `<i>`. If there are several different files with different names but the same `_<i>.kbm` suffix, then the most recently selected one is prioritised. If there is a `x.kbm` suffix where `x` does not indicate a midi channel, then it is taken as a "generic channel". The last selected file for a "generic channel" is the prioritised one. The "generic channel" is any channel that is not specifically assigned. `.scl` files can be used without any `.kbm` file, if the `.kbm` files are underspecified, you can fall back to this, e.g. if there are channel files but no generic file, then you can fall back on that option for the "generic channel".[^PopUpWarning]
    - Right. Here represented as radio buttons but really a toggle. If note on is checked, then pitches should only be updated on note on events. This is probably preferable for tuneBfree considering the building of the wavetable. If continuously is checked than a note that is already sounded can have its pitch changed. This is based after the MTS ESP convention but can be applied to mpe and midi 2.0 as well. Wrt sysex it's less clear that you need to specify this as the sysex messages themselves can specify which one it is, maybe let the note on option override continuous sysex messages? Not really applicable to tuning files.


Let us return to the popup window.[^PopUpChannels]
There are a number of checkbox buttons to select one or more channels. (Technically it's possible to select no channels but this is unadvisable.)
OMNI OFF is default. It means that each channel is handled separately according to it's corresponding `_<i>.kbm` file or corresponding MTS ESP channel.
All other channels that are not specified are taken to be the "generic channel".
If OMNI ON, then all channels are mapped to the "generic channel". This is `-1` in MTS ESP code—although I suspect that in the shared library, `-1` is really the same as `0` (MIDI channel 1).
SELECT ALL selects all the channels
DESELECT ALL removes all the selections.

<table>
    <tr>
        <td>
            <input type="radio" name="omni">OMNI ON</bitton>
        </td>
        <td>
            <input type="checkbox" name="ch"> 1</input>
            <input type="checkbox" name="ch"> 2</input>
            <input type="checkbox" name="ch"> 3</input>
            <input type="checkbox" name="ch"> 4</input>
        </td>
    </tr>
    <tr>
        <td>
            <input type="radio" name="omni">OMNI OFF</bitton>
        </td>
        <td>
            <input type="checkbox" name="ch"> 5</input>
            <input type="checkbox" name="ch"> 6</input>
            <input type="checkbox" name="ch"> 7</input>
            <input type="checkbox" name="ch"> 8</input>
        </td>
    </tr>
    <tr>
        <td>
            <button>SELECT ALL</button>
        </td>
        <td>
            <input type="checkbox" name="ch"> 9</input>
            <input type="checkbox" name="ch">10</input>
            <input type="checkbox" name="ch">11</input>
            <input type="checkbox" name="ch">12</input>
        </td>
    </tr>
    <tr>
        <td>
            <button>DESELECT ALL</button>
        </td>
        <td>
            <input type="checkbox" name="ch">13</input>
            <input type="checkbox" name="ch">14</input>
            <input type="checkbox" name="ch">15</input>
            <input type="checkbox" name="ch">16</input>
        </td>
    </tr>
</table>




The space should be consistent between the widgets.
The spaces above the two titles can be used as empty space if necessary, but it should be the same amount above each title.



**Period Inference and Specification.**
Seeing what the period is is actually not that important.
What it is used for is to tune the drawbars, or, rather, to quantize them to the closest pitch in the scale.
To do this, you simply have to merge all the midi channels into one list of frequencies.
Then, you order this list according to ascending pitch (and maybe remove duplicate pitches?).
(Now, it makes no sense to speak of a period with a negative number of cents.)
For the lower pitches, you can simply set the drawbars according to the closest pitch of the available ones in the tuning.
But for higher pitches, the drawbars may end up outside the tuning table.
If a period is found, you can use this to extend the tuning table.
Otherwise, you simply take the entire tuning table as a period.
(Now, the period would simply be the distance between the lowest frequncy in the tuning table and the highest.)
This is what I assume the code already does, but without the multichannel generalisation, is that right? Also I assume that it only works for exact periods?


Approximation. 5 cents is typically assumed to be less than the just-noticeable difference.
This is for example used in some tuners and tuner apps.
A problem in using this for periods is that the errors can compound no noticeable differences.
So, one solution would be to correct for this.
In more precise psychophysical experiments, you see that the just-noticeable difference is a function of both loadness and frequency and.
This is probably overkill from the point of view of engineering.
However, it would be good if you can look into both audio tools and psychophysics papers and geenrate a report on the matter.


**Problems**
- cannot load several kbm files or directories
- name is shown as the filename rather than as the top entry to the scl file
- inferred period on scala files even though this is specified in the files. Take [9ed3halves](../tunings/9ed3halves/) as an example. If you look in the .scl file, it seems as though the scale period is 9/4 (as a ratio, but can be converted to cents). A period of 18 steps per 9/4 is equivalent to 9 steps per 3/2 so that is also an answer. More informative answers would be either the step size ~78c or looking at the kbm files and see that 14 scale steps (~1092c) is an octave in terms of piano keys (i.e. not the actual interval octave). I think the ~78c value is probably the right call here as the smallest possible period and also one that is practically useful. On the other hand it is easy to infer from the penultimate to last note information. So maybe look at scale degree for formal octave in the earliest kbm file is the most reasonable. (ignore the notes about empty map—thats from the surge xt template.)
- shows loaded scale from scala files even though mts esp is activated or anything other than FILE. the files should only be used when FILE is selected. If you select a file and it's not maybe it makes sense to automatically switch to FILE or at least bring up a dialog asking you whether that's what you want to do. So far I have only tested the scala files as that's easier.
- The default directories when you load scl and kbm is always home, can you change this so it is the last opened directory instead.
- I think soft/normal is the wrong way around. Actually I though hard was the opposite of soft so may have tricked you on the labelling in the gui, maybe it should be normal? or norm? or soemthing? It is a bit hard to hear much of a difference though.

**successes**
- Loads scl and kbm files
- The default scale sounds like 12edo.
- The drive is good as it is!
- I think 2nd 3rd is the right way!
- reverb works
- leslie works
- chorus vibrato works
- drawbars work even though I'm not completely sure that the quantization works as it should.
- the last note and penultimate note seem to work for hz and cents. maybe there should be an arrow from the penultimate hx to the last hz, but let's not do that now as the gui looks good as it is. an idea for future small details.
- from what I can hear by briefly testing the tuning works as it should. At least for simple sine waves. I am not entirely sure about the quantization.


**clarifications**
- The default scale should be "Gear60 (~12edo)".
- This should be visible in the scale name at start-up and should be the inital state for all the scale encodings at start-up. From there on, each encodeding scheme is updated separately.
- I have added a tunings directory to the repo with some example tunings.
- It should be the combined switch for the leslie, but we will come back to this later.

**sandbox**
- claude, you are in a sandbox and I want to keep it so.
- Even so, can you test audio?
- Or can you write the test files and then I can run them, e.g. through some of the debugging tools in vscode
- Some things need a daw rather than the standalone. On mac it seems as though mts esp only works within a daw and not for standalones? that is easier on raspberry pi where it doesn't matter.

[^SurgeTuningEditor]: Surge XT has a tuning editor where you can see the frequency of each midi note. This has a use as Surge can act as an MTS ESP master. However, this tuning panel is only meant to be used as a client (or equivalent for other microtuning coding schemes). Therefore, a live update on frequencies and cents ratios is a more lightweight approach that is more suitible here.
[^TuningFiles]: For a complete list of tuning file formats, see https://scaleworkshop.plainsound.org/
[^PolyInferPeriod]: tuneBfree is built for inferring period for one midi channel, we probably have to specify how this should work for multiple midi channels. For that I would probably need a better understanding for how the inference process actually works.
[^PopUpWarning]: So far, we have implemented a popup warning if files are chosen when FILE is not selected. I think a better solution is that file selection is simply greyed out unless FILE is selected. If so, we can also rename FILE into SCALA. SImilarly, NOTE ON/ALWAYS can be greyed out unless MTS ESP is selected. (Maybe NOTE ON/ALWAYS could also be used for MPE—it would make sense. For MIDI 2.0 I simply don't know. For sysex I think it should be greyed out, but depending on what kind of sysex message is received it may change choice as an indicator. I think it's called real time versus bulk dump, but worth double-checking the spec.)
[^PopUpChannels]: This has changed. Previously there was a POLY on/off option, but this is gone. It is POLY on all the time.



