#pragma once

#include "GrooveLibrary.h"

#include <atomic>

/**
    Plays a groove pattern, emitting MIDI into a buffer the engine consumes.

    The clock lives on the audio thread and advances in samples, so playback is
    sample-accurate and cannot drift against the audio it triggers. Driving it
    from a Timer instead would put millisecond jitter on every hit, which is
    exactly where a programmed groove gives itself away.
*/
class GroovePlayer
{
public:
    // Explicit, because JUCE_DECLARE_NON_COPYABLE declares a deleted copy
    // constructor, and any user-declared constructor suppresses the
    // implicit default one.
    GroovePlayer() = default;

    void prepare (double newSampleRate);

    //==============================================================================
    // Message thread.

    /** Installs a pattern. Playback restarts from the top. */
    void setPattern (const groove::Pattern& patternToPlay);
    void clearPattern();

    void start();
    void stop();

    void setLooping (bool shouldLoop) noexcept { looping.store (shouldLoop); }
    bool isLooping() const noexcept            { return looping.load(); }

    /** Playback tempo. The pattern's own tempo is used as the reference, so a
        groove written at 98 BPM played at 120 comes out proportionally faster
        rather than being re-quantised. */
    void setTempo (double bpm) noexcept { tempoBpm.store (juce::jlimit (20.0, 300.0, bpm)); }
    double getTempo() const noexcept    { return tempoBpm.load(); }

    bool isPlaying() const noexcept   { return playing.load(); }
    bool hasPattern() const noexcept  { return patternLoaded.load(); }

    /** 0..1 through the loop, for the progress bar. */
    float getProgress() const noexcept { return progress.load(); }

    /** Which bar is playing, 1-based. 0 when stopped. */
    int getCurrentBar() const noexcept { return currentBar.load(); }

    //==============================================================================
    // Audio thread.

    /** Appends this block's notes to `buffer` with sample offsets. */
    void process (int numSamples, juce::MidiBuffer& buffer);

private:
    void rewind();

    juce::CriticalSection patternLock;
    groove::Pattern pattern;

    double sampleRate = 48000.0;
    double positionSeconds = 0.0;
    size_t nextEventIndex = 0;

    std::atomic<bool>   playing { false };
    std::atomic<bool>   looping { true };
    std::atomic<bool>   patternLoaded { false };
    std::atomic<double> tempoBpm { 120.0 };
    std::atomic<float>  progress { 0.0f };
    std::atomic<int>    currentBar { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GroovePlayer)
};
