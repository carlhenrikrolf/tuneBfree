// Stub implementations of setBfree MIDI and config callbacks.
//
// In the original setBfree these functions:
//   - notify a runtime MIDI CC mapping table (notifyControl*)
//   - read .cfg configuration files at startup (getConfigParameter_*)
//   - register per-parameter MIDI CC handlers (useMIDIControlFunction)
//
// In tuneBfree 2.0, parameters come from JUCE APVTS and config is not
// loaded at DSP-init time, so all of these are intentional no-ops.
// Config file support will be added in Phase 3 (GUI/config work).

#include "midi.h"
#include "cfgParser.h"
#include "state.h"

// --- MIDI CC notification stubs ---

void notifyControlChangeById(void*, int, unsigned char) {}
void notifyControlChangeByName(void*, const char*, unsigned char) {}
void useMIDIControlFunction(void*, const char*, void (*)(void*, unsigned char), void*) {}

// --- Runtime-state stub ---
// program.cpp is linked for its .pgm PARSER (PresetManager's import); its
// saveProgramm() references rc_loop_state but is never called here.
void rc_loop_state(void*, void (*)(int, const char*, const char*, unsigned char, void*), void*) {}

// --- Program-install stubs ---
// PresetManager reads the parsed Programme table directly, so installProgram()
// and the keyboard-split/transpose engine calls it makes are never invoked;
// they only need to link. (Split/transpose come from tuneBfree's own params.)
void callMIDIControlFunction(void*, const char*, unsigned char) {}
void setKeyboardSplitMulti(void*, int, int, int, int, int, int) {}
void setKeyboardTransposeA(void*, int) {}
void setKeyboardTransposeB(void*, int) {}
void setKeyboardTransposeC(void*, int) {}
void setKeyboardTranspose(void*, int)  {}

// --- Config parser stubs ---
// getConfigParameter_* return 0 (not found) so DSP defaults are kept as-is.

const char* getConfigValue(ConfigContext*) { return nullptr; }

void showConfigfileContext(ConfigContext*, const char*) {}
void configIntUnparsable(ConfigContext*) {}
void configIntOutOfRange(ConfigContext*, int, int) {}
void configDoubleUnparsable(ConfigContext*) {}

int getConfigParameter_d(const char*, ConfigContext*, double*) { return 0; }
int getConfigParameter_i(const char*, ConfigContext*, int*)    { return 0; }
int getConfigParameter_f(const char*, ConfigContext*, float*)  { return 0; }

int getConfigParameter_ir(const char*, ConfigContext*, int*, int, int)          { return 0; }
int getConfigParameter_dr(const char*, ConfigContext*, double*, double, double) { return 0; }
int getConfigParameter_fr(const char*, ConfigContext*, float*, float, float)    { return 0; }
