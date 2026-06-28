# Testing

tuneBfree's DSP/tuning logic is covered by [doctest](https://github.com/doctest/doctest)
unit tests written inline in the `src/*.cpp` files (guarded by `#ifdef TESTS`). They are
JUCE-free and run as a single console executable, so they're fast and CI-friendly.

## One-time setup

```bash
git submodule update --init libs/doctest
```

## Build & run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DTUNEBFREE_BUILD_TESTS=ON
cmake --build build --target tuneBfree_tests
ctest --test-dir build --output-on-failure        # or: ./build/tuneBfree_tests
```

`-DTUNEBFREE_BUILD_TESTS=ON` is off by default, so normal plugin builds are unaffected.

## In VSCode

With the **CMake Tools** extension, add to `.vscode/settings.json`:

```json
{ "cmake.configureSettings": { "TUNEBFREE_BUILD_TESTS": true } }
```

Then use the **Run CTest** button in the status bar (or "CMake: Run Tests"). The
`tuneBfree_tests` target is also a normal debug target, so you can set breakpoints and
launch it from the Run panel.

## What's covered

- **MTS-ESP frequency pull** and the scale-period inference (`inferScaleSize`,
  `extendFrequencies`) for 12-TET, 19-TET, Bohlen-Pierce, and irregular `.scl` examples.
- **Example tunings** in `tunings/` loaded via Surge's tuning-library: 12ed2 is 12-TET,
  9ed3halves declares a 9/4 period of ~78c steps, its smallest inferred period is one
  step, and its `.kbm` pins note 69 to 440 Hz. (These mirror the values the tuning panel
  reports.)
- The tonewheel pairing helper (`getPairedWheel`).

## Adding tests

Put new `TEST_CASE` blocks inside an existing `#ifdef TESTS … #endif` section (only
`src/tuning.cpp` defines the doctest `main`). Tests that load files from `tunings/` should
use the `tuningFile()` helper in `src/tuning.cpp`, which resolves against
`TUNEBFREE_TUNINGS_DIR` (set by the test target).

## Not covered here

Audio output and live MTS-ESP require a host and an audio device, so they're verified
manually in a DAW (on macOS, MTS-ESP only connects inside a DAW, not the standalone).
