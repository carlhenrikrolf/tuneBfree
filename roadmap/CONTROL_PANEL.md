# Control Panel

Here is a suggestion for the CONTROL panel.
As in TUNING_PANEL.md and POLISH.md (read them!), this is for illustarting the layout—not to be taken to literally.
For example, `<select multiple>` does not mean multiple choices, just that the design is directionally correct.

<table>
    <tr>
        <td colspan="2">
            PROGRAM CHANGE
        </td>
    </tr>
    <tr>
        <td>
            <select multiple name="bank" style="width:3cm">
                <option selected>0 tonewheel</selected>
                <option>1 combo</option>
                <option>2 experimental</option>
                <option>3 harmoniumlike</option>
            </select>
        </td>
        <td>
            <select multiple name="program" style="width:3cm">
                <option>0 Jimmy Smith</option>
                <option>1 Green Onions</option>
                <option selected>2 Reggae</option>
                <option>3 Bossa Nova</option>
                <option>4 All Stops Out</option>
            </select>
        </td>
    </tr>
    <tr>
        <td>
            <input type="file" accept=".cfg|.pgm|.xml" style="width:3cm"></input>
        </td>
        <td>
            <button style="width:3cm">save</button>
        </td>
    </tr>
    <tr>
        <td colspan="2">
            CONTINUOUS CONTROLLERS
        </td>
    </tr>
    <tr>
        <td colspan="2">
            <select multiple style="width:5cm">
                <option>omni 64 rotor switch</option>
                <option>omni AT expression</option>
                <option>ch 1 cc 11 expression</option>
                <option>ch 2 cc 1 drawbar 1</option>
                <option>ch 2 cc 2 drawbar 2</option>
                <option>ch 2 cc 3 drawbar 3</option>
            </select>
        </td>
    </tr>
    <!-- <tr>
        <td colspan="2">
            <button style="width:5cm">edit</button>
        </td>
    </tr> -->
</table>

Going through the panel line by line.

1. Title of the program change section
2. Selection.
    - Left. pick a bank.
    - Right. pick a preset within that bank.
3. file management.
    - left. load the presets. a single (multiple) `.xml` for a single (multiple) preset, a (several) directory of `.xml` presets for a (several) bank. `.pgm` can be loaded for backwards compatibility. a single such file is one bank.
    - right. save files, only as `.xml`. I keep saying `.xml` as I believe that's how JUCE saves parametre values, but feel free to correct me.
4. continuous controllers title
5. what is currently mapped to cc-s. cc number in the left column (AT = aftertouch, PT = polytouch, but not relevant here). parameter name in the righ column.

How preset files are saved and loaded are specified above, but what about midi mappings?
Midi mappings are saved as a part of the state of the plugin/standalone.
A JUCE standard functionality (I think it uses `.xml`).
(I have though a lot about having separate midimapping files to save such as in pianoteq and surge xt, but that may be a future extension.)
I'm not really sure how to make the plugin backwards compatible with `.cfg`files. Maybe they can only be added through commandline at startup? Or maybe just load them together with .pgm files?

We already have rightclick functionality on the parameters, this should be augmented by mapping the parameters.
- one midi learn option in the menu.
- Another option (I want both the options available) is to set channel and cc manually.
    - channel selection with omni if any channel will be accepted
    - As there are many cc-s it makes sense to add them in bins of 20 as Surge XT does.
    They also add general midi names, which is a good idea.
    Some cc numbers options should be disabled because the would be inapproprate for mapping, e.g. bank select, rpn, nrpn, the last few cc-s for all notes off etc., the velocity lsb. (that's probably not an exhaistive list.)
- There should also be an option to remove a mapping already set.

Of course, you should be able to rightclick the widgets in the CONTROL panel for info—just as for the TUNING panel.

Preset management and midimappings are recurring problems to be solved in audio plugin development, so it could make sense to look for useful backend solutions, e.g. CLAP, chowdsp utils, surge synth team, etc.


## Misc

I would like it to be possible to input audio into the signal chain after the preamp but before the reverb+leslie.
JUCE should already have a method for controlling audio in which is usually off by default to avoid feedback.
That is good.
You can e.g. have a guitar plugged in, or you could have to instances of tuneBfree for e.g. different manuals, bypass the leslie on one of them and put it in the other and you can have them share the rotary speaker.
(preferably you only edit the reverb on the instance with audio input)




