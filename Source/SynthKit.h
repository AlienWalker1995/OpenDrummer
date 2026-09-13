#pragma once

#include "DrumEngine.h"

/**
    The built-in kits.

    Each is synthesised rather than sampled, which keeps them small and lets
    them retune to whatever rate the audio device is running at. They are not
    trying to pass for a recorded kit - they are a set of genuinely different
    voices to play, and they exercise every path a sampled kit will use:
    velocity layers, round robins and choke groups.
*/
namespace synthkit
{

enum class Style
{
    Studio = 0,     // neutral acoustic, the default
    BigRoom,        // acoustic with long decays and a lot of air
    Jazz,           // small, bright, quick
    Eight08,        // long tuned sine kick, tick snare, metallic hats
    Nine09,         // punchier, noisier, brighter than the 808
    CR78,           // tiny, polite, short - the 1978 preset box

    // Appended rather than grouped with the acoustic kits: the saved setting
    // stores this index, so reordering would silently swap people's kits.
    Punk,           // loose, loud, trashy, in a small room
    Metal,          // triggered kick click, cracking snare, deep toms
    NumStyles
};

/** Display name for a style, as shown in the kit selector. */
juce::String getStyleName (Style style);

/** One-line description of what the kit sounds like. */
juce::String getStyleDescription (Style style);

constexpr int getNumStyles() { return (int) Style::NumStyles; }

/** Generates the kit at the given sample rate and installs it in the engine.
    Allocates heavily - call from the message thread, never from audio. */
void build (DrumEngine& engine, double sampleRate, Style style);

} // namespace synthkit
