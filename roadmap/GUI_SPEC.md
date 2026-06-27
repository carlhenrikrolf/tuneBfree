# Fixes for the GUI

- write the code simply, so that future AI-s can read it and I can go in and edit it
- please write an instruction file for me to go in and edit the code for the ui
- my background knowledge is basic for c/c++ (but advanced for python), basic for html and css (but advanced for latex), I don't really know javascript. I have never really prohrammed anything in JUCE myself.
- this specification is an attempt to describe what my taste is. it's better to try and use it to extrapolate what my taste is rather than following it literally.

## Colours

- use a restricted palette
    - amber
    - black
    - white
    - red
    - grey (could possibly be 2 different shades if necessary)
- you can adjust the precise shades according to fit. I like monokai, but I'm not quite sure it's the right fit for the dimly lit stage vibe.
- amber is the main foreground colour and black the main background
- white text on black and black text on amber is good.

## Consistency
- all knobs should be the same size.
- buttons can only come in one of two sizes.
    - I suggest to make the percussion settings buttons more square
    - all other buttons should be the same height but possibly differing in width.
- the two three-way switches—chorus and rotary speed—should be the same size.
- Likewise, the two two-way switches—upper-lower and mono-poly—shoudl be the same size.
- the expression pedal should have a completely different design than the drawbars, take note of the [default page](default_page.png) mockup. Also, title it EXPRESSION rather than expr
- apart from the tuneBfree title, all words should be in all-caps. This wasn't present in the mockup, I'm sorry didn't have time. This may mean making the font a bit smaller—it shouldn't look aggressive just clear.
- Not counting the title, there should at most be 3 different fonts in use. I'm also not counting the inversion in colour between white and black that can happen when pressing a button.


## Empty spaces

- I don't want empty spaces along the edges of a window, it's bettter if necessary empty spaces are in between widgets intead. You can go back to the [default page](default_page.png) and [tuning sidepanel](tuning_sidepanel.png) mockups to see what I mean by this.
    - timbrality section should definitely be more spread out downwards.
    - the tuning side panel should maybe also be spread out downwards.
- Even though I don't want lines separating different panels, there could be a bit of a larger space separating them, make sure that is a consistent space.
- In general it's nice if it feels like widgets are placed according to some kind of grid—or possibly nested grids. This should be interpreted more that you psychophysically see a grid gestalt rather than you manage to find some really fine-grained grid that you technically manage to fit the widgets into.

## Header

- inversion, let amber be the background colour of the header
- tuneBfree inverted to black
- the button also needs some inversion, maybe replace amber with red when it is active?

## Tuning Sidepanel

- keep it grey but remove the light grey border
- actually thinking of whether you could keep it slightly transparent—maybe also blurring the content behind it?
- I think it would be neat if the tuning sidebar has a width such that it covers the effects, pedals, and timbrality panels but leaves the drawbars and envelope and lfo sections clear.
- remove text tuning source
- move only @ note on to the right column—you can rename this if necessary
- after those 2 changes, all of the left column should be status and all of the right column should be settings, but I'm not sure you need to write this.
- could be nice if it's filled out to the bottom.
- the 2 Hz fields should be next to one another horizontally roughly half as wide as the cents field just as in [tuning side panel](tuning_sidepanel.png). It is meant to show that from one frequency to another frequncy the change is so many cents. If the numbers don't fit, maybe move the units outside the fields. if it still doesnt fit the current frequency is more important than the last frequency. but you can of course make the entire tuning menu wider as long as that widths is consistent with the underlying panels as discussed before.

## Drawbars

- The drawbars now pull and move in the right directions! :)
- Colouring is not entirely correct. The drawbar highest in pitch (1') should be the same colour as 2', 4' and 8'. I kind of want the coloring similar to the mockup albeit not completely standard, viz.:
    - red
    - red
    - white
    - white
    - amber
    - white
    - amber
    - amber
    - white
- it's probably good to make the drawbars a bit shorter to help with the problem of empty space.