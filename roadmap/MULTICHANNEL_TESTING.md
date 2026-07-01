# Manual test plan — async rebuild (step 0) + per-channel tuning (step 1a)

What to test in a DAW after the 2026-06-29 work. I can't run audio / MTS-ESP in the
sandbox, so these are the checks that need your ears. Report back per section with
pass / fail + what you heard.

## Build & install

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release        # AU auto-copies to ~/Library/Audio/Plug-Ins/Components/
```

Quit & relaunch the DAW so it rescans the AU. (On Mac, MTS-ESP only works inside a DAW,
not the Standalone — test the AU.)

## What changed (so you know what you're probing)

- **Step 0:** tuning/ratio/file changes no longer rebuild the tonegen on the audio
  thread. A background worker builds the new engine; the audio thread swaps it in. The
  old engine keeps playing until the swap.
- **Step 1a:** the 16×128 (channel, note) grid is merged into one **gamut** of distinct
  pitches; each `(channel, note)` routes to its gamut slot. Default (all channels active,
  ordinary single MTS master) is designed to behave **exactly like before**.

## Round 2 (2026-06-30) — fixes after your first test

Your first round: A pass, B pass, C fail (two issues), D not tested. Fixed for re-test:
- **C voice bug** — shared-pitch slots are now reference-counted (a slot stays sounding
  until the last key on it is released). Re-run **Test C step 4**.
- **C display bug** — the Hz/cents read-out is now per-note (channel-aware). Re-run
  **Test C step 3**.
- **PANIC button** added (left of TUNING) + MIDI CC 120/123 — releases all notes.
- **`.kbm` rule changed** to `_i` → channel i (+ default for the rest); no alphabetical
  ordering. See the rule note in Test C.

---

## Test A — Step 0: async rebuild (anyone can run this)

Goal: confirm the off-thread rebuild has no glitch/xrun and no stuck notes.

1. **Tuning change while holding a chord.** Hold a sustained chord. With an MTS master
   (e.g. Surge/Scala/MTS-ESP source), change the scale. Expect: a brief moment later the
   pitches update; **no click/dropout/xrun** at the change; held notes may re-trigger or
   cut once (expected) but must not glitch the audio stream.
2. **Drawbar-ratio sweep.** In the Studio/ratio params, drag a `ratio_top_*` / `ratio_bot_*`
   value back and forth quickly while playing. Expect: no xruns, no crashes; the timbre
   updates a beat later (rebuild latency), not instantly.
3. **Rapid-fire changes.** Spam scale/ratio changes for ~10 s while playing. Expect: no
   crash, no runaway CPU, no leaked voices; it settles on the final tuning.
4. **Load a `.scl`/`.kbm` while playing.** Expect: clean swap, no xrun.
5. **Quit the DAW / remove the plugin mid-change.** Expect: no hang, no crash (the worker
   joins on teardown with a 2 s budget).

**Most likely failure modes to watch:** an audible click exactly at a tuning change
(swap not clean), a note that won't stop (note-off lost across a swap), or CPU climbing
after many changes (engine not being freed).

---

## Test B — Step 1a: single-channel REGRESSION (most important)

The rewrite must not change single-channel behaviour. Send everything on **one** MIDI
channel (or a normal single keyboard) and confirm the previously-working behaviour from
the June test round is intact:

1. **Default 12-TET (Gear60).** Fresh instance sounds like before; middle A = 440.
2. **MTS single master.** Connect one MTS master, pick a scale → pitches correct, and the
   tuning-panel last/penultimate note Hz + cents read out correctly.
3. **`.scl` / `.kbm` (FILE source).** Load a scale + mapping → correct pitches; "x"
   (unmapped) keys are silent; notes above/below map as before.
4. **Filtered/unmapped keys.** On a sparse scale, the silenced keys stay silent and the
   played keys sound — and crucially **note-off works** (no stuck notes) when you release.
5. **Everything else still works:** drawbars, percussion, vibrato/chorus, overdrive,
   reverb, Leslie, expression pedal, Bluetooth MIDI.

**Most likely failure modes:** a note that sticks (the note-on→slot→note-off tracking is
new), keys silent that shouldn't be, or wrong pitches — any of these means the
`slotIndex` identity path or the live filter is off. This is the section I'm least able
to verify myself, so please be thorough here.

---

## Test C — Step 1a: multichannel via per-channel `.kbm` (Mac — the easy one)

This is the multichannel path you can test on Mac, no special MTS master needed. One
`.scl`, several `.kbm` (one per MIDI channel).

Suggested files (already in the repo): `tunings/9ed3halves/` has `9ed3halves.scl` plus
`9ed3halves_1.kbm` and `9ed3halves_2.kbm`, which differ by one ~78-cent step (kb2 sits
~78 c below kb1).

**`.kbm` assignment rule (revised — no alphabetical order):** a file named `*_i.kbm`
(i = 1..16) maps to **channel i**; a file with no valid `_i` suffix is a **default** that
fills every channel you didn't explicitly assign. So `9ed3halves_1.kbm` → channel 1,
`9ed3halves_2.kbm` → channel 2, and only those two channels sound.

1. **SCALE** → load `tunings/9ed3halves/9ed3halves.scl`.
2. **MAP** → in the chooser, **multi-select both** `9ed3halves_1.kbm` and
   `9ed3halves_2.kbm` (Cmd-click). MAP reads "2 maps"; it switches the source to FILE.
3. Play the **same key** on **MIDI channel 1** vs **channel 2** (your two keyboards, or
   your DAW's per-track channel) → expect a **~78-cent pitch difference**, and the panel's
   **Hz / cents read-out should now differ between the two** (was the bug last round).
4. **Shared-pitch hold test (the bug you found):** find a key on ch 1 and a key on ch 2
   that sound the **same pitch** (where the two keyboards coincide). Hold the ch-1 key,
   then add the ch-2 key → should keep sounding (no cut, ideally no click); release the
   ch-1 key → **still sounds**; release the ch-2 key → **now it stops**. (Previously the
   second silenced/clicked and releasing the first stopped everything.)
5. Play across both → one coherent merged scale; coincident pitches collapse to one
   (no detuned doubling).
6. Single-channel check: load **one non-`_i` `.kbm`** (e.g. `tunings/7edo/7edo.kbm`,
   which has no `_i` suffix) → it's the default, so **every channel** plays it (the
   pre-multichannel behaviour). NB a lone `*_1.kbm` would map **only channel 1**.

**Gamut cap:** now **2048 distinct pitches** — the hard maximum (16 channels × 128 notes),
so it can never silently drop pitches. Rebuild cost scales with how many channels you
enable (the CHANNELS popup is the throttle); a maxed-out tuning rebuilds in ~1–2 s (async,
no glitch).

**Most likely failure modes:** both channels sounding identical (per-channel grid not
applied), notes silent on one channel (mask / gamut routing), stuck notes when releasing,
or the high end dropping earlier than ~128 distinct pitches implies.

---

## Test D — Step 1a: multichannel via an MTS master (RPi, optional)

If you push and pull on the RPi with your own multichannel MTS-ESP: set different
per-channel tunings on the master and confirm the **same MIDI note plays different
pitches on different channels**, merging into one gamut. Same 128-pitch cap applies.
(On Mac, MTS multichannel needs a multichannel-capable master; the `.kbm` route in
Test C is the simpler check.)

---

## Test E — CHANNELS popup (POLY / OMNI), Mac

The CHANNELS button (right of the encoding menu in the tuning panel) opens a popup.

1. **POLY on (default), select channels.** With two `.kbm` loaded (Test C), open CHANNELS
   and uncheck a channel → notes on that channel go silent; re-check → they return.
2. **POLY off (single).** Turn POLY off → the channel toggles act as a radio (one at a
   time); only the selected channel sounds.
3. **OMNI on.** Channel toggles grey out; every incoming channel now plays the same
   (unspecified-channel) MTS tuning. (OMNI targets MTS; FILE keeps its per-channel `.kbm`.)
4. **Persistence.** Set a channel config, save the project / reload the plugin → the
   config (active channels, OMNI, POLY) should restore.

**Most likely failure modes:** toggling a channel doesn't change what sounds (rebuild not
triggered), the popup doesn't match the plugin's look, or config doesn't persist.
(Known: POLY doesn't yet remember the *other* mode's last selection — minor.)

---

## Not in this build (so you don't test for them)

- `.kbm` *directory* selection — multi-file select works; picking a whole folder doesn't yet.
- The keyboard split / crossfade (step 2+), swell-pedal contour.

## What to report back

For A–D: pass/fail + anything you heard (clicks, stuck notes, wrong pitches, silence, CPU).
Priorities: **Test B** (single-channel must be unchanged) and **Test C** (per-channel
`.kbm` is your two-keyboard workflow). If both are good, the rewire is sound and I can
move to step 1b (keyspace widening past 128) and/or the CHANNELS UI.

___

# Observations
Only FILE tested—not MTS ESP.
- A pass
- B pass
- C fail
- D not tested.

What failed on C?
The notes seem to sound the correct pitches as far as I can tell but I only tested by ear.
78c difference between manuals for 9ed3/2 seems right by ear.
However, the Hz and cents for the notes show the same for the two manuals even if they have different pitch.
If I play one pitch on one manual, then the same pitch on another manual, then I just hear a click from the second note. If I release the first key, then the sound stops. This is not right. When I play the second note, that should just continue the first pitch—I guess a click could be fine?—and it should continue even if I drop the key of the first note. When I then drop the second key again it should stop sounding.

Could you just please add the panic button so that I have it for debugging.

I've had a change of heart on bitimbrality percussion and vibrato/chorus.
Let's keep it more like the B3.
Percussion only affects the upper manual.
Vibrato/chorus type is the same over both manuals even though it can be tunred on and off for each manual.
I will probably want to do some gui changes to this later—not urgent.

As for multiple `.kbm` files. It works well to select several, but I want to remove the alphabetical ordering thing.
- If a `_i.kbm` file is in the selection, then channel `i` will get that mapping.
   - If a second file with the same `i` in `_i.kbm` is selected, then the last selcted file will take precedence.
- If a file ends with `x.kbm` where `x` is not equal to `_i` for any valid `i`, then this will be applied to all channels that have not yet been spcified as above.

I think that will make more sense, right? Does the surge scala file tool have any implementation for this, because here I'm just assume it doesn't?

As an aside I think the mapping file for 14edo is incorrect.

I have only tested standalone on mac and FILE.