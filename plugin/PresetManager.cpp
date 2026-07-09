#include "PresetManager.h"
#include "PluginProcessor.h"

// setBfree .pgm machinery (parse-only: we read the Programme table directly,
// installProgram and its engine callbacks are never called from here).
#include "program.h"
#include "pgmParser.h"
#include "vibrato.h"   // VIB1..CHO3 scanner codes
#include "whirl.h"     // WHIRL_SLOW/STOP/FAST rotary codes

// ============================================================================
//  Scope / capture / apply
// ============================================================================

bool PresetManager::isPresetScopeParam (const juce::String& paramID)
{
    return paramID != "master_volume" && paramID != "active_manual";
}

juce::ValueTree PresetManager::capture (const juce::String& name, const juce::String& author,
                                        bool includeTuning) const
{
    juce::ValueTree preset (presetTag);
    preset.setProperty ("name",   name,   nullptr);
    preset.setProperty ("author", author, nullptr);

    juce::ValueTree params ("PARAMS");
    for (auto* ap : proc.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (ap))
            if (isPresetScopeParam (p->paramID))
                params.setProperty (p->paramID, p->convertFrom0to1 (p->getValue()), nullptr);

    // HARMONICS entry strings ("3/2", "702.23 c") — the user's notation; the
    // harm_cents_* params above carry the values that drive the engine.
    for (int i = 0; i < 9; ++i)
        params.setProperty ("harmEntry" + juce::String (i),
                            proc.getHarmonicEntryText (i), nullptr);

    preset.appendChild (params, nullptr);

    if (includeTuning)
        preset.appendChild (proc.captureTuningTree(), nullptr);

    return preset;
}

void PresetManager::apply (const juce::ValueTree& preset)
{
    const auto params = preset.getChildWithName ("PARAMS");
    if (! params.isValid())
        return;

    for (int i = 0; i < params.getNumProperties(); ++i)
    {
        const auto id = params.getPropertyName (i).toString();
        if (id.startsWith ("harmEntry") || ! isPresetScopeParam (id))
            continue;
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (double) params.getProperty (id)));
    }

    // Entry strings: always all nine, so a preset without them (e.g. a .pgm
    // import) clears stale notation instead of leaving it under default cents.
    for (int i = 0; i < 9; ++i)
        proc.setHarmonicEntryText (i, params.getProperty ("harmEntry" + juce::String (i),
                                                          juce::String()));

    if (const auto tuning = preset.getChildWithName ("TUNING"); tuning.isValid())
        proc.applyTuningTree (tuning);
}

juce::ValueTree PresetManager::defaultParams() const
{
    juce::ValueTree params ("PARAMS");
    for (auto* ap : proc.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (ap))
            if (isPresetScopeParam (p->paramID))
                params.setProperty (p->paramID, p->convertFrom0to1 (p->getDefaultValue()), nullptr);
    return params;
}

// ============================================================================
//  Banks model (under apvts.state / "BANKS" — persists with the session)
// ============================================================================

juce::ValueTree PresetManager::banksTree() const
{
    return proc.apvts.state.getOrCreateChildWithName ("BANKS", nullptr);
}

int PresetManager::numBanks() const                { return banksTree().getNumChildren(); }
juce::ValueTree PresetManager::getBank (int i) const { return banksTree().getChild (i); }

juce::String PresetManager::getBankName (int i) const
{
    return getBank (i).getProperty ("name", "Bank " + juce::String (i)).toString();
}

int PresetManager::addBank (const juce::String& name)
{
    auto banks = banksTree();
    juce::ValueTree bank ("BANK");
    bank.setProperty ("name", name, nullptr);
    banks.appendChild (bank, nullptr);
    return banks.getNumChildren() - 1;
}

int PresetManager::numPresets (int bankIndex) const
{
    return getBank (bankIndex).getNumChildren();
}

juce::ValueTree PresetManager::getPreset (int bankIndex, int presetIndex) const
{
    return getBank (bankIndex).getChild (presetIndex);
}

juce::String PresetManager::getPresetName (int bankIndex, int presetIndex) const
{
    return getPreset (bankIndex, presetIndex).getProperty ("name", "Untitled").toString();
}

void PresetManager::appendPreset (int bankIndex, juce::ValueTree preset)
{
    auto bank = getBank (bankIndex);
    if (bank.isValid())
        bank.appendChild (preset, nullptr);
}

int  PresetManager::getCurrentBank() const   { return banksTree().getProperty ("currentBank", 0); }
void PresetManager::setCurrentBank (int i)   { banksTree().setProperty ("currentBank", i, nullptr); }
int  PresetManager::getCurrentPreset() const { return banksTree().getProperty ("currentPreset", -1); }
void PresetManager::setCurrentPreset (int i) { banksTree().setProperty ("currentPreset", i, nullptr); }

// ============================================================================
//  File I/O
// ============================================================================

bool PresetManager::savePresetFile (const juce::ValueTree& preset, const juce::File& f,
                                    juce::String& error) const
{
    auto out = preset.createCopy();
    out.setProperty ("app",           "tuneBfree", nullptr);
    out.setProperty ("appVersion",    "2.0.0",     nullptr);   // keep in sync with CMake project()
    out.setProperty ("formatVersion", 1,           nullptr);

    if (const auto xml = out.createXml(); xml != nullptr && xml->writeTo (f))
        return true;

    error = "Could not write " + f.getFullPathName();
    return false;
}

juce::ValueTree PresetManager::loadPresetFile (const juce::File& f, juce::String& error) const
{
    const auto xml = juce::parseXML (f);
    if (xml == nullptr || ! xml->hasTagName (presetTag))
    {
        error = f.getFileName() + " is not a tuneBfree preset";
        return {};
    }
    auto preset = juce::ValueTree::fromXml (*xml);
    if (preset.getProperty ("name").toString().isEmpty())
        preset.setProperty ("name", f.getFileNameWithoutExtension(), nullptr);
    return preset;
}

int PresetManager::loadPresetFilesIntoBank (const juce::Array<juce::File>& files, int bankIndex,
                                            juce::String& error)
{
    int loaded = 0;
    for (const auto& f : files)
    {
        juce::String err;
        if (auto preset = loadPresetFile (f, err); preset.isValid())
        {
            appendPreset (bankIndex, preset);
            ++loaded;
        }
        else if (error.isEmpty())
            error = err;
    }
    return loaded;
}

int PresetManager::loadBankDirectory (const juce::File& dir, juce::String& error)
{
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();
    if (files.isEmpty())
    {
        error = "No .xml presets in " + dir.getFileName();
        return -1;
    }

    const int bank = addBank (dir.getFileName());
    if (loadPresetFilesIntoBank (files, bank, error) == 0)
    {
        banksTree().removeChild (bank, nullptr);
        if (error.isEmpty())
            error = "No presets could be loaded from " + dir.getFileName();
        return -1;
    }
    return bank;
}

// ============================================================================
//  .pgm import (setBfree programme file = one bank)
// ============================================================================

// Map one parsed Programme onto a full parameter snapshot: defaults + the
// fields the programme actually flags. Field polarities mirror what
// installProgram feeds the engine (see src/program.cpp) translated to the
// applyParam() conventions in PluginProcessor.cpp.
static void applyProgrammeToParams (const Programme& pgm, juce::ValueTree& params)
{
    const auto flags = pgm.flags[0];
    auto set = [&params] (const char* id, float v) { params.setProperty (id, v, nullptr); };

    if ((flags & FL_DRAWBR) && ! (flags & FL_DRWRND))
        for (int i = 0; i < 9; ++i)
            set (("drawbar" + juce::String (i)).toRawUTF8(), (float) pgm.drawbars[i]);

    if ((flags & FL_LOWDRW) && ! (flags & FL_DRWRND))
        for (int i = 0; i < 9; ++i)
            set (("lower_drawbar" + juce::String (i)).toRawUTF8(), (float) pgm.lowerDrawbars[i]);
    // pedalDrawbars: no pedal manual in tuneBfree — skipped.

    if (flags & FL_SCANNR)
    {
        // scanner low byte = VIB1..CHO3; vibrato_type 0..5 = V1 C1 V2 C2 V3 C3.
        switch (pgm.scanner & 0xff)
        {
            case VIB1: set ("vibrato_type", 0.0f); break;
            case CHO1: set ("vibrato_type", 1.0f); break;
            case VIB2: set ("vibrato_type", 2.0f); break;
            case CHO2: set ("vibrato_type", 3.0f); break;
            case VIB3: set ("vibrato_type", 4.0f); break;
            case CHO3: set ("vibrato_type", 5.0f); break;
            default: break;
        }
    }
    if (flags & FL_VCRUPR) set ("vibrato",       (pgm.scanner & 0x200) ? 1.0f : 0.0f);
    if (flags & FL_VCRLWR) set ("lower_vibrato", (pgm.scanner & 0x100) ? 1.0f : 0.0f);

    if (flags & FL_PRCENA) set ("percussion", pgm.percussionEnabled ? 1.0f : 0.0f);
    // .pgm TRUE = soft; applyParam passes (1 - value) to setPercussionVolume.
    if (flags & FL_PRCVOL) set ("percussion_vol", pgm.percussionVolume ? 0.0f : 1.0f);
    if (flags & FL_PRCSPD) set ("percussion_dec", pgm.percussionSpeed    ? 1.0f : 0.0f);
    if (flags & FL_PRCHRM) set ("percussion_har", pgm.percussionHarmonic ? 1.0f : 0.0f);

    if (flags & FL_OVRSEL) set ("overdrive", pgm.overdriveSelect ? 1.0f : 0.0f);

    // Rotary: one .pgm speed drives both rotors; the drum/horn params use the
    // same WHIRL_SLOW/STOP/FAST code space (applyParam passes them through).
    if (flags & FL_ROTSPS)
    {
        set ("drum", (float) pgm.rotarySpeedSelect);
        set ("horn", (float) pgm.rotarySpeedSelect);
    }

    if (flags & FL_RVBMIX) set ("reverb_mix", pgm.reverbMix);
    // keyboardSplit* / transpose: different split model (pitch-based) — skipped.
}

int PresetManager::importPgmFile (const juce::File& f, juce::String& error)
{
    struct b_programme* progs = allocProgs();
    if (progs == nullptr)
    {
        error = "Out of memory";
        return -1;
    }

    std::string path = f.getFullPathName().toStdString();
    const int parseResult = loadProgrammeFile (progs, path.data());

    int bank = -1;
    for (int i = 0; i < MAXPROGS; ++i)
    {
        const Programme& pgm = progs->programmes[i];
        if (! (pgm.flags[0] & FL_INUSE))
            continue;

        if (bank < 0)
            bank = addBank (f.getFileNameWithoutExtension());

        juce::ValueTree preset (presetTag);
        const auto name = juce::String::fromUTF8 (pgm.name).trim();
        preset.setProperty ("name", name.isEmpty() ? "Program " + juce::String (i) : name, nullptr);

        auto params = defaultParams();
        applyProgrammeToParams (pgm, params);
        preset.appendChild (params, nullptr);
        appendPreset (bank, preset);
    }
    freeProgs (progs);

    if (bank < 0)
        error = parseResult != 0 ? "Could not parse " + f.getFileName()
                                 : "No programs in " + f.getFileName();
    return bank;
}
