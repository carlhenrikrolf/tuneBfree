---
description: tuneBfree / setBfree DSP architecture, signal chain, key functions, and config formats
triggers:
  - setBfree
  - tuneBfree
  - tonegen
  - drawbar
  - Leslie
  - whirl
---

# setBfree / tuneBfree DSP Reference

tuneBfree is a fork of setBfree — a tonewheel Hammond B3 organ emulator — extended with MTS-ESP microtuning. All DSP lives in `src/`. The JUCE plugin wrapper lives in `plugin/`.

---

## Signal Chain

```
MIDI notes
    ↓
tonegen (tonewheel oscillators + vibrato/chorus)
    ↓  bufA [mono, 128 samples]
preamp / overdrive
    ↓  bufB
reverb
    ↓  bufC
whirlProc3 (Leslie rotary speaker)
    ↓  bufL[0] (L)  bufL[1] (R)
stereo output
```

All internal buffers are `float[BUFFER_SIZE_SAMPLES]` where `BUFFER_SIZE_SAMPLES = 128`.

---

## Key Files

| File | Purpose |
|------|---------|
| `src/tonegen.cpp / .h` | Tonewheel oscillator bank, 91 wheels, 27 buses, key handling, vibrato |
| `src/whirl.cpp / .h` | Leslie rotary speaker (horn + drum), Doppler, filtering |
| `src/reverb.cpp / .h` | Reverb (algorithmic, based on Airwindows) |
| `src/overdrive.cpp / .h` | Preamp / saturation (Airwindows Density) |
| `src/vibrato.cpp / .h` | Vibrato/chorus scanner |
| `src/tuning.cpp / .h` | MTS-ESP integration, scale period calculation |
| `src/eqcomp.cpp / .h` | EQ / compression stage |
| `src/cfgParser.cpp / .h` | Parser for `.cfg` runtime config files |
| `src/pgmParser.cpp / .h` | Parser for `.pgm` organ program/patch files |
| `src/clap.cpp` | Legacy CLAP plugin wrapper (reference implementation) |
| `plugin/PluginProcessor.cpp` | JUCE AudioProcessor wrapper |

---

## Core API

### Tone generator

```cpp
// Lifecycle
struct b_tonegen* allocTonegen();
void initToneGenerator(struct b_tonegen* t, void* cfg, double sampleRate, double* targetRatio);
void freeToneGenerator(struct b_tonegen* t);

// MIDI
void oscKeyOn(struct b_tonegen* t, short midiNote, short realKey);
void oscKeyOff(struct b_tonegen* t, short midiNote, short realKey);

// Audio
void oscGenerateFragment(struct b_tonegen* t, float* buf, size_t lengthSamples);

// Parameters
void setDrawBar(struct b_tonegen* t, int bus, unsigned int setting); // setting 0-8
void setVibratoUpper(struct b_tonegen* t, int isEnabled);
void setVibratoFromInt(struct b_tonegen* t, int vibratoType);       // 0-5
void setPercussionEnabled(struct b_tonegen* t, int enabled);
void setPercussionVolume(struct b_tonegen* t, int isSoft);           // 0=norm, 1=soft
void setPercussionFast(struct b_tonegen* t, int isFast);             // 1=fast, 0=slow
void setPercussionFirst(struct b_tonegen* t, int is2nd);             // 0=3rd harm, 1=2nd
```

`targetRatio` is a 9-element double array; ratio[i] = top/bottom for drawbar i.
Default 12-tone equal temperament: `{0.5, 1.5, 1, 2, 3, 4, 5, 6, 8}`.

After changing `targetRatio`, you must free and reinit the tone generator (see `reinitToneGen` in `src/clap.cpp`).

### Preamp / Overdrive

```cpp
void* allocPreamp();
void  initPreamp(void* pa, void* cfg, double sampleRate);
void  freePreamp(void* pa);
float* preamp(void* pa, float* inBuf, float* outBuf, size_t N);
void  fsetCharacter(struct b_preamp* d, float A);  // A = 0.0–1.0
// isClean member: set to 1 to bypass overdrive, 0 to enable
```

Note: `preamp` is both the function name and a natural variable name — use `preampModule` or similar to avoid shadowing.

### Reverb

```cpp
struct b_reverb* allocReverb();
void initReverb(struct b_reverb* r, void* cfg, double sampleRate);
void freeReverb(struct b_reverb* r);
void setReverbMix(struct b_reverb* r, double mix);  // 0.0–1.0
// Call as method: r->reverb(inBuf, outBuf, N)
```

### Leslie / Whirl

```cpp
struct b_whirl* allocWhirl();
void initWhirl(struct b_whirl* w, void* cfg, double sampleRate);
void freeWhirl(struct b_whirl* w);

// Main render call
float* whirlProc3(struct b_whirl* w,
                  const float* inBufMono,
                  float* outBufL, float* outBufR,
                  float* drumBuf, float* tmpBuf,
                  size_t N);

// Leslie speed routing
// option = floor(drumParam) + 3*floor(hornParam), drumOption = 2
void useRevOption(struct b_whirl* w, int option, int drumOption);
```

---

## The 38 Parameters (matches `src/clap.cpp`)

```
Index  Name                   Range     Default
0–8    Drawbar 0–8            0–8       7,8,8,0,0,0,0,0,0
9      Vibrato on/off         0–1       0
10     Vibrato type           0–5.99    0
11     Drum                   0–2.99    1
12     Horn                   0–2.99    1
13     Overdrive on/off       0–1       0
14     Character              0–1       0
15     Reverb wet/dry         0–1       0.1
16     Percussion on/off      0–1       0
17     Percussion soft/norm   0–1       0
18     Percussion fast/slow   0–1       0
19     Percussion 2nd/3rd     0–1       0
20–28  Ratio top 0–8          0–1000    1,3,1,2,3,4,5,6,8
29–37  Ratio bottom 0–8       0–1000    2,2,1,1,1,1,1,1,1
```

---

## Config Files

### `.cfg` files (`cfg/` directory)
Runtime parameters: Leslie speed, preamp gain, reverb depth, vibrato type, etc.
Parsed by `cfgParser.cpp`. Custom key=value format, one section per module:
```
[tonegen]
tuning=440.0
[leslie]
speed.horn.fast=410.0
```

### `.pgm` files (`pgm/` directory)
Organ programs/patches: drawbar settings, percussion, vibrato per preset.
Parsed by `pgmParser.cpp`. Custom format with `[program]` blocks.

---

## Reinit on Tuning Change

When MTS-ESP frequencies or ratio parameters change, the full tone generator must be freed and rebuilt. This is handled by `reinitToneGen()` in `src/clap.cpp` / `plugin/PluginProcessor.cpp`. It:
1. Saves current `newRouting` state
2. Frees the old `b_tonegen`
3. Allocates and inits a new one with updated `targetRatio`
4. Re-applies all drawbar/vibrato params

---

## BUFFER_SIZE_SAMPLES = 128

The DSP processes audio in 128-sample internal chunks regardless of the host buffer size. The plugin wrapper (`synthSound` / `renderAudio`) loops to handle host buffers of any size.
