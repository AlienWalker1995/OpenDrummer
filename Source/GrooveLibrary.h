#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>

/**
    Groove patterns: standard MIDI files, parsed into a flat list of timed hits.

    Note-offs are discarded on load. A drum sample rings out on its own, so the
    only thing a groove needs to say is "hit this, this hard, at this moment".
*/
namespace groove
{

struct Note
{
    double timeSeconds = 0.0;
    int    noteNumber = 36;
    float  velocity = 0.8f;
};

struct Pattern
{
    juce::String name;
    juce::File   file;

    double bpm = 120.0;
    double lengthSeconds = 0.0;    // rounded up to whole bars, so loops line up
    int    bars = 1;
    int    timeSigNumerator = 4;
    int    timeSigDenominator = 4;

    std::vector<Note> notes;       // sorted by time

    bool isValid() const { return ! notes.empty() && lengthSeconds > 0.0; }
};

/** Parses a .mid file. Returns false if it holds no usable drum notes. */
bool loadPattern (const juce::File& file, Pattern& result);

/** Writes a starter set of grooves into `folder`, skipping any that already
    exist. Without this the library would open empty on a machine that has
    never had a MIDI groove pack on it. */
void writeStarterGrooves (const juce::File& folder);

/** The folder the app uses for grooves unless the user picks another. */
juce::File getDefaultGrooveFolder();

//==============================================================================
class Library
{
public:
    /** Scans a folder for .mid files, parsing metadata for each. */
    void scan (const juce::File& folderToScan);

    const std::vector<Pattern>& getPatterns() const noexcept { return patterns; }
    const juce::File& getFolder() const noexcept              { return folder; }

    int getNumPatterns() const noexcept { return (int) patterns.size(); }

    /** Null if the index is out of range. */
    const Pattern* getPattern (int index) const;

private:
    juce::File folder;
    std::vector<Pattern> patterns;
};

} // namespace groove
