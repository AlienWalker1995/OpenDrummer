#pragma once

#include "MidiMapping.h"

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

/**
    Recorded drum kits, downloaded from free SFZ libraries and played through
    sfizz.

    Each kit ships with its own key layout, and none of them match a Roland TD
    module exactly - one puts its highest tom on 47, another on 48, and only
    some have a china. So every kit carries a table mapping OpenDrummer's
    articulations onto that kit's keys. The pads you hit never change; this
    table is what makes "Tom 1" land on the right drum in every kit.
*/
namespace sampledkits
{

constexpr int kNumArticulations = (int) midimap::Articulation::NumArticulations;

/** A kit key for each articulation. -1 means the kit has nothing to play. */
using KeyTable = std::array<int, (size_t) kNumArticulations>;

enum class LoadMethod
{
    /** Load one SFZ file, optionally with text substitutions applied first. */
    SingleFile,

    /** Layer a folder of per-microphone SFZ files into one instrument, panning
        each mic, because the kit was only published split by microphone. */
    LayeredMics
};

struct MicPlacement
{
    const char* fileName;   // e.g. "Overhead left.sfz"
    int pan;                // -100 hard left .. 100 hard right
    float volumeDb;
};

struct Substitution
{
    const char* find;
    const char* replace;
};

struct KitDefinition
{
    const char* id;             // stable, stored in settings
    const char* name;
    const char* description;
    const char* credit;         // author and licence, shown in the UI
    const char* folderName;     // under the kits folder
    const char* programPath;    // SingleFile: the .sfz; LayeredMics: the mic folder

    LoadMethod method;

    /** Level trim so switching kits does not jump in volume. Set from measured
        renders: a kit layered from fifteen microphones is simply louder. */
    float gainDb = 0.0f;

    std::vector<Substitution> substitutions;
    std::vector<MicPlacement> mics;

    KeyTable keys;

    /** Key to strike when a cymbal is grabbed, for kits that model chokes as
        their own notes. -1 where the kit has no choke for that articulation. */
    KeyTable chokeKeys;
};

struct InstalledKit
{
    const KitDefinition* definition = nullptr;
    juce::File root;

    bool isValid() const { return definition != nullptr; }
};

/** Every kit OpenDrummer knows how to play, installed or not. */
const std::vector<KitDefinition>& getDefinitions();

/** The kits actually present in `kitsFolder`, in display order. */
std::vector<InstalledKit> findInstalledKits (const juce::File& kitsFolder);

/** Looks for a Kits folder beside the application, walking up from the
    executable, and falls back to the user app-data folder. Kits are gigabytes,
    so they deliberately do not default to the OneDrive-synced Documents. */
juce::File getDefaultKitsFolder();

/** Produces the SFZ text and the path its relative includes resolve against.
    Returns false with a reason if the kit's files are not where expected. */
bool prepareSfz (const InstalledKit& kit, juce::File& resolvePath,
                 juce::String& sfzText, juce::String& error);

} // namespace sampledkits
