# tuneBfree to JUCE

*This is a specification for desirable updates. It is a good idea to make a plan that can be executed in phases. This should be a high-level update specification. Technical low-level details can be put within `<details><summary></summary></details>`.*

MTS-ESP periods.
- Last version includes an algorithm for calculating the periods of scales. These are useful for tuning the drawbars. Keep this, but optionally include some kind of precision toggle, e.g. in cents (Weber's law).
- There are updates to MTS-ESP to specify scale periods. If scale period is specify use this, otherwise fall back to the period calculation.
- MTS-ESP also includes interpreting midi sysex messages. Include this.
- MTS-ESP and midi sysex can specify midi channels. Allow for this possibility. Note that this will conflict with the 3 manuals setup. I suggest that you remove the possibility to split into different manuals. If that is what you want to do, maybe just use different instances of the virtual instrument.
- The team behind Surge XT has developed tools for more microtuning options such as uploading tuning files. These tools work well with JUCE so investigate to what extent these can be included.
- It is also worth investigating to what extent the midi 2.0 specification is publicly available and maybe implement such capabilities for tuning.
- A final way to do microtuning is via pitchbend/MPE/general MIDI, but because of the wavetable generation process this may be difficult to implement. However, it is worth investigating.

GUI.
- tuneBfree/setBfree has a well-developed GUI, but it may not be straight forward update all relevant updates within that framework. JUCE has tools for building UI. I suggest the following: On one hand, you can switch between a play/video/or something mode where you see the old GUI and see how midi notes and midi CC affects it. One another hand, you have a sound design mode or whatever to call it where you use more appropriate tools for designing the sound. Partially implemented in tuneBfree is the ability to override the period tuning of the drawbars and use your own ratios. I wouldn't prioritise this, but it is an example where the old gui is not adequate.
- Config files are used together with the old GUI, it could be relevant to include these in the new GUI as well. The config files follow a custom format, it might make sense to keep this but also allow for a more modern format such as json or toml.
- In addition it would be good to have a terminal interface where you can load settings and 
- Maybe generate a few drawings of potential guis and receive feedback.

Compatibility.
- Mac
- Raspberry Pi debian (and optionally zynthian)
- standalone
- lv plugin
- au plugin
- clap plugin

Miscellaneous.
- Audio input. It would be nice to have input channels such that you can run the input directly through the leslie/rotary speaker together with the hammond organ.
- In the long run I would want a nicer reverb. Could also make sense to have a pre-rotary spring reverb. Aeolus Archie3d fork has an impulse response from a York cathedral that works on raspberry pi but in general those are cpu heavy.

Create skills files.
- setBfree
- microtuning
- JUCE

Resources.
- Zynthian. For running on raspberry pi.
- Scale workshop and Sevish for microtuning tools.
- Surge XT and related projects for microtunign tools.
- The MIDI association for specs.
- William Sethares for the relationship between tuning and timbre (timbre ~ drawbar settings).
- JUCE has lots of online documentation.
- Exquis for microtuning.
- Aeolus, especially Archie3d
- foo-YC (not microtuned yet).
- Let me know if I can somehow invite you for read-only access to relevant discord servers.
