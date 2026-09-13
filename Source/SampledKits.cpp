#include "SampledKits.h"

using midimap::Articulation;

namespace sampledkits
{
namespace
{
    KeyTable makeKeys (std::initializer_list<std::pair<Articulation, int>> entries)
    {
        KeyTable table;
        table.fill (-1);

        for (const auto& [articulation, key] : entries)
            table[(size_t) articulation] = key;

        return table;
    }

    std::vector<KitDefinition> buildDefinitions()
    {
        std::vector<KitDefinition> kits;

        //======================================================================
        // DRSKit - the neutral all-rounder. Ships with its own stereo program.
        {
            KitDefinition kit;
            kit.id = "drs";
            kit.name = "DRS Kit";
            kit.description = "Handmade Danish kit, Paiste cymbals - jazz to rock";
            kit.credit = "DrumGizmo team & Jes Eiler (DRSDrums), CC BY 4.0";
            kit.folderName = "DRSKit";
            kit.programPath = "DrumGizmo/DRSKit/Stereo/DrumGizmo DRSKit.sfz";
            kit.method = LoadMethod::SingleFile;
            kit.gainDb = 5.0f;

            // Only three toms: rack pads share the hanging tom, floor pads get
            // the two floor toms, so low still means low under your sticks.
            kit.keys = makeKeys ({
                { Articulation::Kick, 36 },
                { Articulation::SnareHead, 38 },
                // The kit has no loud rimshot: 37 is a rim click (19 dB under a
                // plain hit) and 40 a rest stroke (17 dB under). A pad hit hard
                // should not come out quiet, so rimshots play the full snare.
                { Articulation::SnareRimshot, 38 },
                { Articulation::SnareCrossStick, 37 },
                { Articulation::Tom1, 47 }, { Articulation::Tom1Rim, 47 },
                { Articulation::Tom2, 47 }, { Articulation::Tom2Rim, 47 },
                { Articulation::Tom3, 43 }, { Articulation::Tom3Rim, 43 },
                { Articulation::Tom4, 41 }, { Articulation::Tom4Rim, 41 },
                // Explicit closed and open keys. The kit's pedal-controlled key
                // shares 46 with a plain semi-open hat, so every "closed" hit also
                // fired a ringing one - measured at 1.5 s. The module already
                // chooses closed or open from the pedal, so these are exact.
                { Articulation::HiHatClosedBow, 56 }, { Articulation::HiHatOpenBow, 58 },
                { Articulation::HiHatClosedEdge, 42 }, { Articulation::HiHatOpenEdge, 58 },
                { Articulation::HiHatPedal, 44 },
                { Articulation::Crash1Bow, 54 }, { Articulation::Crash1Edge, 49 },
                { Articulation::Crash2Bow, 55 }, { Articulation::Crash2Edge, 57 },
                { Articulation::RideBow, 51 }, { Articulation::RideEdge, 66 },
                { Articulation::RideBell, 53 } });

            kit.chokeKeys = makeKeys ({
                { Articulation::Crash1Bow, 48 }, { Articulation::Crash1Edge, 48 },
                { Articulation::Crash2Bow, 50 }, { Articulation::Crash2Edge, 50 },
                { Articulation::RideBow, 52 }, { Articulation::RideEdge, 52 },
                { Articulation::RideBell, 52 } });

            kits.push_back (std::move (kit));
        }

        //======================================================================
        // Big Rusty Drums - loud 1980s kit. The punk kit.
        {
            KitDefinition kit;
            kit.id = "big-rusty";
            kit.name = "Big Rusty";
            kit.description = "Loud, battered 1980s kit with a china - punk";
            kit.credit = "Karoryfer Samples, CC0";
            kit.folderName = "BigRustyDrums";
            kit.programPath = "Programs/01-full.sfz";
            kit.method = LoadMethod::SingleFile;
            kit.gainDb = 5.0f;

            // The program ships set up for a keyboard, where the hi-hat CC runs
            // the other way. Karoryfer include the e-kit ranges in the same
            // folder; this swaps them in without touching their files.
            kit.substitutions = {
                { "#include \"mappings/hihat_14/hihat_cc_ranges_kb.sfz\"",
                  "#include \"mappings/hihat_14/hihat_cc_ranges_ekit.sfz\"" },
                { "#define $ht_lo_hi_init 127", "#define $ht_lo_hi_init 0" }
            };

            kit.keys = makeKeys ({
                { Articulation::Kick, 36 },
                { Articulation::SnareHead, 38 },
                { Articulation::SnareRimshot, 40 },
                { Articulation::SnareCrossStick, 37 },
                { Articulation::Tom1, 47 }, { Articulation::Tom1Rim, 47 },
                { Articulation::Tom2, 45 }, { Articulation::Tom2Rim, 45 },
                { Articulation::Tom3, 43 }, { Articulation::Tom3Rim, 43 },
                { Articulation::Tom4, 41 }, { Articulation::Tom4Rim, 41 },
                { Articulation::HiHatClosedBow, 42 }, { Articulation::HiHatOpenBow, 46 },
                { Articulation::HiHatClosedEdge, 54 }, { Articulation::HiHatOpenEdge, 58 },
                { Articulation::HiHatPedal, 44 },
                { Articulation::Crash1Bow, 49 }, { Articulation::Crash1Edge, 49 },
                // Crash 2 is the china - the trashiest thing in the kit.
                { Articulation::Crash2Bow, 57 }, { Articulation::Crash2Edge, 57 },
                { Articulation::RideBow, 51 }, { Articulation::RideEdge, 52 },
                { Articulation::RideBell, 53 } });

            kit.chokeKeys = makeKeys ({
                { Articulation::Crash1Bow, 50 }, { Articulation::Crash1Edge, 50 },
                { Articulation::Crash2Bow, 59 }, { Articulation::Crash2Edge, 59 },
                { Articulation::RideBow, 55 }, { Articulation::RideEdge, 55 },
                { Articulation::RideBell, 55 } });

            kits.push_back (std::move (kit));
        }

        //======================================================================
        // MuldjordKit - double kick and a china. The metal kit.
        {
            KitDefinition kit;
            kit.id = "muldjord";
            kit.name = "Muldjord";
            kit.description = "Double-kick kit with a china - metal";
            kit.credit = "Lars Muldjord / DrumGizmo, CC BY 4.0";
            kit.folderName = "MuldjordKit";
            kit.programPath = "DrumGizmo/MuldjordKit/Multi";
            kit.method = LoadMethod::LayeredMics;

            // Published only as one file per microphone, none of which pans.
            // Layered as recorded they collapse to mono, so each mic is placed
            // here as seen from the drum stool. KdrumL/R are the two kick drums
            // of the double kick, not a stereo pair. The trigger channel is
            // left out: it is a clicky trigger feed, not a microphone.
            kit.mics = {
                { "Kickdrum left.sfz",   -6,   0.0f },
                { "Kickdrum right.sfz",   6,   0.0f },
                { "Snare top.sfz",       -8,   0.0f },
                { "Snare bottom.sfz",    -8,  -6.0f },
                { "Hihat.sfz",          -45,  -2.0f },
                { "Rack tom 1.sfz",     -30,   0.0f },
                { "Rack tom 2.sfz",     -12,   0.0f },
                { "Rack tom 3.sfz",      10,   0.0f },
                { "Floor tom.sfz",       35,   0.0f },
                { "Ride left.sfz",      -55,  -2.0f },
                { "Ride right.sfz",      55,  -2.0f },
                { "Overhead left.sfz",  -75,   0.0f },
                { "Overhead right.sfz",  75,   0.0f },
                { "Ambience left.sfz",  -90,  -3.0f },
                { "Ambience right.sfz",  90,  -3.0f }
            };

            kit.keys = makeKeys ({
                { Articulation::Kick, 36 },
                { Articulation::SnareHead, 38 },
                { Articulation::SnareRimshot, 37 },
                { Articulation::SnareCrossStick, 37 },
                { Articulation::Tom1, 48 }, { Articulation::Tom1Rim, 48 },
                { Articulation::Tom2, 47 }, { Articulation::Tom2Rim, 47 },
                { Articulation::Tom3, 45 }, { Articulation::Tom3Rim, 45 },
                { Articulation::Tom4, 41 }, { Articulation::Tom4Rim, 41 },
                { Articulation::HiHatClosedBow, 42 }, { Articulation::HiHatClosedEdge, 42 },
                { Articulation::HiHatOpenBow, 46 }, { Articulation::HiHatOpenEdge, 46 },
                { Articulation::HiHatPedal, 44 },
                { Articulation::Crash1Bow, 49 }, { Articulation::Crash1Edge, 49 },
                // Crash 2 is the china on 52.
                { Articulation::Crash2Bow, 52 }, { Articulation::Crash2Edge, 52 },
                { Articulation::RideBow, 51 }, { Articulation::RideEdge, 51 },
                { Articulation::RideBell, 53 } });

            kit.chokeKeys = makeKeys ({
                { Articulation::Crash1Bow, 62 }, { Articulation::Crash1Edge, 62 },
                { Articulation::Crash2Bow, 64 }, { Articulation::Crash2Edge, 64 },
                { Articulation::RideBow, 61 }, { Articulation::RideEdge, 61 },
                { Articulation::RideBell, 61 } });

            kits.push_back (std::move (kit));
        }

        return kits;
    }
}

//==============================================================================
const std::vector<KitDefinition>& getDefinitions()
{
    static const std::vector<KitDefinition> definitions = buildDefinitions();
    return definitions;
}

std::vector<InstalledKit> findInstalledKits (const juce::File& kitsFolder)
{
    std::vector<InstalledKit> installed;

    for (const auto& definition : getDefinitions())
    {
        const auto root = kitsFolder.getChildFile (definition.folderName);
        const auto program = root.getChildFile (definition.programPath);

        const bool present = definition.method == LoadMethod::SingleFile
                           ? program.existsAsFile()
                           : program.isDirectory();

        if (present)
            installed.push_back ({ &definition, root });
    }

    return installed;
}

juce::File getDefaultKitsFolder()
{
    // Walk up from the executable. The build lives several folders below the
    // project root, and an installed copy would sit right beside its kits.
    auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();

    for (int depth = 0; depth < 6 && dir.exists(); ++depth)
    {
        const auto candidate = dir.getChildFile ("Kits");

        if (candidate.isDirectory() && ! findInstalledKits (candidate).empty())
            return candidate;

        dir = dir.getParentDirectory();
    }

    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("OpenDrummer")
             .getChildFile ("Kits");
}

//==============================================================================
bool prepareSfz (const InstalledKit& kit, juce::File& resolvePath,
                 juce::String& sfzText, juce::String& error)
{
    if (! kit.isValid())
    {
        error = "No kit selected";
        return false;
    }

    const auto& definition = *kit.definition;
    const auto program = kit.root.getChildFile (definition.programPath);

    if (definition.method == LoadMethod::SingleFile)
    {
        if (! program.existsAsFile())
        {
            error = "Missing " + program.getFullPathName();
            return false;
        }

        sfzText = program.loadFileAsString();

        for (const auto& substitution : definition.substitutions)
        {
            // A substitution that finds nothing means the kit was updated and
            // the tweak silently stopped applying. Say so rather than shipping
            // a hi-hat that works backwards.
            if (! sfzText.contains (substitution.find))
            {
                error = juce::String ("Kit files have changed - could not find: ") + substitution.find;
                return false;
            }

            sfzText = sfzText.replace (substitution.find, substitution.replace);
        }

        resolvePath = program;
        return true;
    }

    // LayeredMics
    if (! program.isDirectory())
    {
        error = "Missing folder " + program.getFullPathName();
        return false;
    }

    juce::String layered;
    int micsFound = 0;

    for (const auto& mic : definition.mics)
    {
        const auto micFile = program.getChildFile (mic.fileName);

        if (! micFile.existsAsFile())
            continue;

        auto text = micFile.loadFileAsString();

        // Each mic file opens a <global> through this include. Opcodes written
        // straight after it land in that global scope, so the pan and level
        // apply to every region the mic contributes.
        const juce::String globalInclude = "#include \"../Data/global.txt\"";

        if (! text.contains (globalInclude))
        {
            error = juce::String ("Unexpected layout in ") + mic.fileName;
            return false;
        }

        text = text.replace (globalInclude,
                             globalInclude
                               + "\npan=" + juce::String (mic.pan)
                               + "\nvolume=" + juce::String (mic.volumeDb, 1) + "\n");

        // A fresh <control> before each file, so that one mic's label and
        // default-CC opcodes cannot fall into the previous mic's last region.
        layered << "<control>\n" << text << "\n";
        ++micsFound;
    }

    if (micsFound == 0)
    {
        error = "No microphone files found in " + program.getFullPathName();
        return false;
    }

    sfzText = layered;

    // Relative includes in the mic files are written from the mic folder, so
    // the combined text has to resolve as though it lived there too.
    resolvePath = program.getChildFile ("_opendrummer_layered.sfz");
    return true;
}

} // namespace sampledkits
