# Doing stuff with tuneBfree

[tuneBfree](https://github.com/narenratan/tuneBfree) is [naren](https://github.com/narenratan)’s fork of a setBfree tonewheel organ emulation that allows custom tuning using [MTS-ESP](https://github.com/ODDSound/MTS-ESP). It being one of the first microtunable tonewheel organ synths, I thought it might be useful to write a quick post of what one might wish to consider or have in mind tuning it.

## MTS-ESP connection basics

An MTS-ESP master plugin like [MTS-ESP Mini](https://oddsound.com/mtsespmini.php) should be present in your project somewhere. It can be in passthru mode to conserve resources; the only thing you need to be sure of is that it says that a client is connected. If tuneBfree is the sole MTS-ESP-aware plugin in there, that’s it! Load a tuning and it will apply immediately.

Note that hosts like Carla or Element, as far I was able to work them, don’t allow MTS-ESP connection. Either I didn’t do something crucial or you need another more diversely functional DAW.

## Tonewheels and tuning

It should be noted that classic Hammond organs have non-equal tuning of the notes. A motor having a target rotation speed then drives tonewheels through couples of gears. Each couple of gears determines (with some exceptions at the higher notes) tuning of some pitch class like C, C#, D etc.. The notes A get the concert tuning of 2<sup>*n*</sup> ⋅ 440 Hz, all others though get tuned a cent or so away from their 12edo intervals.

The organs have drawbars for managing partials 1/2 and 3/2 (“subharmonics”, though 3/2 is just a fifth above) and 1 to 8 except 7. But *actual* partials aren’t exactly in this rational relationship with the fundamental. Rather, existing tonewheels are used to approximate them. So partials 1/2, 2, 4 and 8 are represented exactly because 12edo and its Hammond organ approximation have pure octaves. Partials 3/2, 3, 6 are represented very well because, again, 12edo and the approximation are good at doing fifths—though each note will have slightly different tunings of these. Partial 5 is represented with an error of ≈15¢.

The original setBfree supports tunings used in organs driven by 60 Hz and 50 Hz current (which have different gear ratios because their motors have different RPM), as well as 12edo (which gives all notes a timbre that is the most stable but maybe lacks a little bit of soul). As tuneBfree supports arbitrary tuning and picks tonewheels for actual played partials based on closeness to the target partial, you’d need to take all the above into account depending on your goals:

- If you want timbre that’s close to the classic feel, be sure to pick a tuning that has good octaves as well as good fifths. Or, in case you’re playing with drawbars for octave or fifth partials moved to zero, disregard either octave or fifth closeness because they won’t be used. For the 888 timbre to sound normal, you’ll still need good octaves and fifths.
- Also, to be closer to normality, consider tunings that aren’t too much far from equal division because then two notes can get assigned the same tonewheel as one of the prominent partials, which would make those notes be hardly distinguishable.
- On the other hand, if you’re feeling nostalgic or alt-universe, don’t just use a diatonic edo like 19, 17, 24, 31 etc. either. Instead, use a close approximation. The next section is about doing that.
- But if you specifically want to be timbrally weird 🤩 then consider edos like 9, 11, 13, 16, 18, 23, and scales with many different step sizes, and Bohlen—Pierce as a primer in nonoctave tunings.

## Approximations to equal tunings

### Historical well temperaments

There are already established tunings, though many of them are 12-note and divide an octave and not, say, 3/1, which might be not so interesting.

### Approximating with arbitrary ratios

Going the way the original organs went, one can choose small rational numbers close to pitches of the given equal division. [Scale Workshop 2](https://scaleworkshop.plainsound.org) has a relatively hassle-free way to choose approximations which are under a specified amount of cents away.

Generate or load your scale (for an equal division, click or hover on *New scale* and then choose *Equal temperament*). Then hover/click *Modify scale* and choose *Approximate by ratios*. The simplest way to proceed then is to set *Maximum error* to a value you desire and then repeatedly choose approximations for each consecutive scale degree from an *Approximation* list, pushing *Apply* after each choice (which automatically goes to the next scale degree).

Don’t forget to export the tuning, of course.

Also I looked into doing this in Scala but wasn’t too well-versed in the software to have any success.

### Approximating with a subset of a harmonic mode (NEJI)

You can also wish to have a [NEJI](https://en.xen.wiki/w/Neji) of your equal division. In this case, if you have a small edo, you might end up satisfied with one of these handy lists:

- [sorted by decreasing average error](https://en.xen.wiki/w/NEJI_Tables/Average_Error),
- [sorted by decreasing greatest error](https://en.xen.wiki/w/NEJI_Tables/Greatest_Error),

both of them currently up to 34edo and with the root harmonics going up to 128. Having chosen such a harmonic scale like `47:54:62:71:82:94`, go to Scale Worksop and do *New scale* > *Enumerate chord*, then paste it there and you can have a tuning file.

Otherwise, [Scala](https://www.huygens-fokker.org/scala/) is easy to use this time. Load your tuning, then choose *Approximate* > *Fit to harmonic scale* in the menu, or execute `fit /harmonic`. A list of options sorted by standard deviation will be given. Alas if the scale is too large, it won’t show exact harmonics verbatim. Despite you can then make Scala into giving you out a scale you picked from those options, for me it would be better to open Scale Workshop and then, knowing the first harmonic of a given approximation, just enter it in *Modify scale* > *Approximate by harmonics*.

### Just randomizing a bit

Also a valid approach is to jiggle degrees of the tuning. That’s *Modify* > *Vary…* in Scala, and *Modify scale* > *Random variance* in Scale Workshop. 

## Final words

Well, that’s all I had in mind to write here for now. Have fun!
