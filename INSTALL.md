# tuneBfree 2.0 — Build and Install

## Prerequisites

### Mac

- Xcode command-line tools: `xcode-select --install`
- CMake: `brew install cmake`

### Raspberry Pi Debian

- `sudo apt install cmake build-essential`
- Cross-compilation from Mac is possible with a toolchain file (documented separately when RPi build is set up).

---

## Building

### First-time setup

Clone the repo and initialise the required submodules:

```bash
git clone https://github.com/carlhenrikrolf/tuneBfree.git
cd tuneBfree
git checkout juce
git submodule update --init libs/JUCE libs/MTS-ESP libs/readerwriterqueue libs/tuning-library
```

### Configure and build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The build auto-copies plugins to the system plugin folders on Mac
(`COPY_PLUGIN_AFTER_BUILD TRUE` in CMakeLists.txt). Outputs also available in
`build/tuneBfree_artefacts/Release/`.

### Subsequent builds

After any code change:

```bash
cmake --build build --config Release
```

No need to re-run `cmake -B build` unless CMakeLists.txt changed.

---

## Plugin formats (Mac)

| Format | Location after build |
|--------|----------------------|
| AU | `~/Library/Audio/Plug-Ins/Components/tuneBfree.component` |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/tuneBfree.vst3` |
| Standalone | `build/tuneBfree_artefacts/Release/Standalone/tuneBfree.app` |

---

## GarageBand

### First use

1. Build (see above) — the AU is auto-copied on each build.
2. Quit GarageBand fully if it was open.
3. Open GarageBand. tuneBfree appears under **Audio Units > tuneBfree > tuneBfree**.

### After every rebuild

1. Run `cmake --build build --config Release`
2. Quit GarageBand and reopen it.

GarageBand does not hot-reload plugins. A full quit-and-reopen is always required to pick up a new or updated AU.

### If the plugin does not appear

Force the OS to validate and register it:

```bash
auval -v aumu Tfre Tunb
```

Then quit and reopen GarageBand. If it still does not appear, clear the AU cache:

```bash
rm -rf ~/Library/Caches/com.apple.audio.InfoHelper*
```

---

## Standalone (headless, Raspberry Pi)

The Standalone target is a normal executable. Run it directly:

```bash
./build/tuneBfree_artefacts/Release/Standalone/tuneBfree.app/Contents/MacOS/tuneBfree
```

On RPi it will be a plain binary without the `.app` wrapper. MIDI input and audio output
are configured via JUCE's standalone settings on first launch, or via command-line flags
in a future headless mode (Phase 3).

---

## Legacy CLAP build (src/)

The original CLAP plugin is still buildable independently:

```bash
cd src
cmake -B build
cmake --build build
# Output: src/build/tuneBfree.clap
```

This build has a known note-off bug on Raspberry Pi (fixed in the JUCE version).
