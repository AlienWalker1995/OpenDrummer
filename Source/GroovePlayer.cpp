#include "GroovePlayer.h"

namespace
{
    constexpr int kDrumChannel = 10;
}

void GroovePlayer::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    rewind();
}

void GroovePlayer::rewind()
{
    positionSeconds = 0.0;
    nextEventIndex = 0;
    progress.store (0.0f);
    currentBar.store (0);
}

void GroovePlayer::setPattern (const groove::Pattern& patternToPlay)
{
    {
        const juce::ScopedLock sl (patternLock);
        pattern = patternToPlay;
        rewind();
    }

    patternLoaded.store (pattern.isValid());

    // Adopt the pattern's own tempo as the starting point - a groove written
    // at 98 BPM should not silently play at whatever the last one used.
    if (pattern.isValid())
        setTempo (pattern.bpm);
}

void GroovePlayer::clearPattern()
{
    playing.store (false);
    patternLoaded.store (false);

    const juce::ScopedLock sl (patternLock);
    pattern = {};
    rewind();
}

void GroovePlayer::start()
{
    if (! patternLoaded.load())
        return;

    {
        const juce::ScopedLock sl (patternLock);
        rewind();
    }

    playing.store (true);
}

void GroovePlayer::stop()
{
    playing.store (false);

    const juce::ScopedLock sl (patternLock);
    rewind();
}

//==============================================================================
void GroovePlayer::process (int numSamples, juce::MidiBuffer& buffer)
{
    if (! playing.load() || numSamples <= 0)
        return;

    // Never block the audio thread waiting for a pattern swap.
    const juce::ScopedTryLock sl (patternLock);

    if (! sl.isLocked() || ! pattern.isValid())
        return;

    // Playback rate relative to how the pattern was written.
    const double rate = tempoBpm.load() / juce::jmax (1.0, pattern.bpm);
    const double blockSeconds = (double) numSamples / sampleRate * rate;

    const double blockStart = positionSeconds;
    double remaining = blockSeconds;

    // Walk the block in chunks so the loop point is handled exactly rather
    // than being rounded to a block boundary. Beat 1 of every repeat lands
    // where it should.
    while (remaining > 1.0e-12 && playing.load())
    {
        const double chunkEnd = juce::jmin (positionSeconds + remaining, pattern.lengthSeconds);

        while (nextEventIndex < pattern.notes.size())
        {
            const auto& note = pattern.notes[nextEventIndex];

            if (note.timeSeconds >= chunkEnd)
                break;

            // Convert the event's position back into a sample offset within
            // this block, accounting for the tempo scaling.
            const double elapsed = note.timeSeconds - blockStart;
            int offset = (int) std::floor (elapsed / rate * sampleRate);
            offset = juce::jlimit (0, numSamples - 1, offset);

            buffer.addEvent (juce::MidiMessage::noteOn (kDrumChannel, note.noteNumber,
                                                        note.velocity),
                             offset);
            ++nextEventIndex;
        }

        remaining -= (chunkEnd - positionSeconds);
        positionSeconds = chunkEnd;

        if (positionSeconds >= pattern.lengthSeconds - 1.0e-12)
        {
            if (looping.load())
            {
                positionSeconds = 0.0;
                nextEventIndex = 0;
            }
            else
            {
                playing.store (false);
                rewind();
                break;
            }
        }
    }

    if (pattern.lengthSeconds > 0.0)
    {
        const float normalised = (float) (positionSeconds / pattern.lengthSeconds);
        progress.store (juce::jlimit (0.0f, 1.0f, normalised));

        const double barSeconds = pattern.lengthSeconds / juce::jmax (1, pattern.bars);
        currentBar.store (juce::jlimit (1, pattern.bars,
                                        (int) (positionSeconds / barSeconds) + 1));
    }
}
