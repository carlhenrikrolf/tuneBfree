---
description: JUCE AudioProcessor patterns, CMake setup, parameter system, and plugin format conventions
triggers:
  - JUCE
  - AudioProcessor
  - juce_add_plugin
  - APVTS
  - AudioProcessorValueTreeState
---

# JUCE Plugin Development Reference

JUCE is a C++ framework for audio plugins. This project uses JUCE 8.x targeting Standalone, AU (Mac), VST3, and CLAP formats. JUCE handles platform differences; you write one AudioProcessor class.

---

## CMake Setup

```cmake
cmake_minimum_required(VERSION 3.22)
project(MyPlugin VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 20)

# Add JUCE (submodule preferred; FetchContent as fallback)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt")
    add_subdirectory(libs/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.7 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(MyPlugin
    VERSION                     1.0.0
    COMPANY_NAME                "My Company"
    BUNDLE_ID                   "com.mycompany.myplugin"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    "Manu"   # 4 chars, uppercase start
    PLUGIN_CODE                 "Plug"   # 4 chars, unique per plugin
    FORMATS                     Standalone AU VST3 CLAP
    PRODUCT_NAME                "My Plugin"
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice  # for synths
)

target_sources(MyPlugin PRIVATE plugin/PluginProcessor.cpp plugin/PluginEditor.cpp)
target_include_directories(MyPlugin PRIVATE src)
target_compile_definitions(MyPlugin PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(MyPlugin
    PRIVATE juce::juce_audio_processors juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
```

To add JUCE as a git submodule:
```bash
git submodule add https://github.com/juce-framework/JUCE.git libs/JUCE
git submodule update --init --recursive
```

Build:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On macOS, set `-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64` for universal binary.

---

## AudioProcessor Skeleton

```cpp
// PluginProcessor.h
#pragma once
#include <JuceHeader.h>

class MyAudioProcessor : public juce::AudioProcessor
{
public:
    MyAudioProcessor();
    ~MyAudioProcessor() override;

    // Must implement
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Plugin identity
    const juce::String getName() const override { return "MyPlugin"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    // Programs (1 = no program support)
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    // Editor
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    // State
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Bus layout: stereo out, no input (for synths)
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::disabled();
    }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyAudioProcessor)
};
```

Constructor with bus layout and APVTS:
```cpp
MyAudioProcessor::MyAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{}
```

---

## AudioProcessorValueTreeState (APVTS)

APVTS manages parameters with thread-safe atomic access. Define the layout once; JUCE auto-creates the XML state and handles GUI↔audio sync.

```cpp
juce::AudioProcessorValueTreeState::ParameterLayout MyAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Float parameter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gain", 1},     // {id, version}
        "Gain",                           // display name
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.5f));                           // default

    // Float with step (for discrete values like drawbars 0–8)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"drawbar0", 1}, "Drawbar 1",
        juce::NormalisableRange<float>(0.0f, 8.0f, 1.0f), 7.0f));

    // Bool
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1}, "Bypass", false));

    return {params.begin(), params.end()};
}
```

### Accessing values in the audio thread (lock-free)

```cpp
// Store pointers once in prepareToPlay or constructor
std::atomic<float>* gainParam = apvts.getRawParameterValue("gain");

// In processBlock — direct atomic read, no lock needed
float gain = gainParam->load();
```

### State save / load (XML-based)

```cpp
void MyAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MyAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
```

---

## processBlock Pattern (MIDI + Audio, sample-accurate)

```cpp
void MyAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getWritePointer(1);
    int startSample = 0;

    for (const auto metadata : midiMessages) {
        int eventPos = metadata.samplePosition;
        if (eventPos > startSample)
            renderAudio(outL + startSample, outR + startSample, eventPos - startSample);
        startSample = eventPos;

        auto msg = metadata.getMessage();
        if      (msg.isNoteOn())  noteOn(msg.getNoteNumber());
        else if (msg.isNoteOff()) noteOff(msg.getNoteNumber());
        // JUCE normalizes velocity-0 note-on to noteOff automatically
    }

    if (startSample < buffer.getNumSamples())
        renderAudio(outL + startSample, outR + startSample,
                    buffer.getNumSamples() - startSample);
}
```

---

## Editor: Generic Placeholder

Use `juce::GenericAudioProcessorEditor` for a quick working UI during development. It auto-generates sliders for all APVTS parameters.

```cpp
// In PluginEditor.h
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MyAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    MyAudioProcessorEditor(MyAudioProcessor& p)
        : AudioProcessorEditor(&p), processor(p)
    { setSize(600, 400); }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff1a1a2e));
    }
    void resized() override {}

private:
    MyAudioProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyAudioProcessorEditor)
};
```

Or return a `GenericAudioProcessorEditor` directly:
```cpp
juce::AudioProcessorEditor* MyAudioProcessor::createEditor() {
    return new juce::GenericAudioProcessorEditor(*this);
}
```

---

## Platform Notes

### Raspberry Pi (Standalone target)
The Standalone build produces a native executable. Launch headless with `--no-gui` or configure JUCE's `StandalonePluginHolder` for MIDI-only runtime after startup.

For RPi, disable LTO if build is slow or produces errors:
```cmake
# Remove: juce::juce_recommended_lto_flags
```

RPi cross-compilation (from Mac/Linux):
```bash
cmake -B build-rpi \
  -DCMAKE_TOOLCHAIN_FILE=path/to/rpi-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
```

### Mac (AU + VST3)
JUCE handles AU SDK requirements internally. No separate AU SDK install needed. Xcode command-line tools required. For notarization, enable hardened runtime and sign with your Apple Developer certificate.

### macOS privacy permissions (Bluetooth MIDI, Microphone)

Use JUCE's dedicated permission properties in `juce_add_plugin` — do NOT use `PLIST_TO_MERGE` (it is silently ignored in JUCE 8):

```cmake
juce_add_plugin(MyPlugin
    ...
    BLUETOOTH_PERMISSION_ENABLED    TRUE
    BLUETOOTH_PERMISSION_TEXT       "MyPlugin uses Bluetooth to receive MIDI from wireless controllers."
    MICROPHONE_PERMISSION_ENABLED   TRUE
    MICROPHONE_PERMISSION_TEXT      "MyPlugin uses the microphone for audio input."
)
```

Without `BLUETOOTH_PERMISSION_ENABLED`, the Standalone app crashes on launch with a TCC privacy violation when macOS tries to enumerate Bluetooth MIDI devices. Same principle applies to microphone, camera, etc. — JUCE has first-class properties for each.

### CLAP (JUCE 8+)
CLAP format is natively supported in JUCE 8. Add `CLAP` to `FORMATS` in `juce_add_plugin`. No extra extensions library needed.

---

## Useful JUCE Classes

| Class | Purpose |
|-------|---------|
| `juce::AudioBuffer<float>` | Audio sample buffer, channel-major layout |
| `juce::MidiBuffer` | Timestamped MIDI event list for one block |
| `juce::MidiMessage` | Individual MIDI message with helpers (isNoteOn, etc.) |
| `juce::AudioProcessorValueTreeState` | Thread-safe parameter + state management |
| `juce::NormalisableRange<float>` | Maps parameter range with optional step/skew |
| `juce::ScopedNoDenormals` | Suppresses denormal floats in processBlock |
| `juce::GenericAudioProcessorEditor` | Auto-generated slider UI for all parameters |
